// Presets are independent files, not episode metadata. Only placement edits the level.
#include "edPreset.h"
struct PresetEntry {std::filesystem::path path;std::string name;};
static std::vector<PresetEntry> presetEntries;
static std::unique_ptr<EdPreset> presetLoaded;
static std::map<std::string,rdMaterial*> presetPreviewMaterials;
static void presetFreePreviewMaterials(){rdCache_Flush();for(auto &entry:presetPreviewMaterials){if(entry.second)rdMaterial_Free(entry.second);terrainBakedResources.files.erase(entry.first);}presetPreviewMaterials.clear();}
static rdMaterial *presetPreviewMaterial(const EdFace &face){
    auto &preset=*presetLoaded;std::shared_ptr<const EdResources::Bytes> bytes;std::string key;
    if(face.decalId>0&&face.decalId<=preset.model.decalCount){bytes=preset.model.decals[face.decalId-1].data;key="decal"+std::to_string(face.decalId);}
    else if(face.material>=0&&face.material<(int)preset.materials.size()){
        const auto &name=preset.materials[face.material];key="material"+std::to_string(face.material);
        for(auto &entry:preset.resources.files)if(!__strcmpi(baseName(entry.first.c_str()),name.c_str())){bytes=entry.second;break;}
        if(!bytes)return editorMaterial(addMaterialName(name.c_str()));
    }
    if(!bytes)return editorMaterial(0);
    std::string path="mat/geosith_preset_preview_"+key+".mat";auto found=presetPreviewMaterials.find(path);if(found!=presetPreviewMaterials.end())return found->second;
    terrainBakedResources.add(path,*bytes);rdMaterialLoader_t previous=rdMaterial_RegisterLoader(NULL);rdMaterial *mat=rdMaterial_Load(path.data(),0,0);rdMaterial_RegisterLoader(previous);presetPreviewMaterials[path]=mat;return mat;
}
static std::set<int> presetThings;
static EdBuffer<unsigned char> presetSectors;
static EdHit presetMount={-1,-1,-1,-1};
static int presetIndex=-1,presetEnterDown;
static double presetOffsetRight,presetOffsetUp;
static unsigned presetOffsetKeys;
static void presetMovementKeys(int suppress){
    static int masked,saved[5][3];const int keys[]={DIK_NUMPAD8,DIK_NUMPAD4,DIK_NUMPAD6,DIK_NUMPAD2,DIK_NUMPAD5};
    if(suppress){masked=tool==6&&presetLoaded&&builderAsset<0&&!mapActive;if(!masked)return;
        for(int i=0;i<5;i++){int k=keys[i];saved[i][0]=stdControl_aKeyInfo[k];saved[i][1]=stdControl_aKeyPressed[k];saved[i][2]=stdControl_aKeyIdleTimes[k];stdControl_aKeyInfo[k]=stdControl_aKeyPressed[k]=stdControl_aKeyIdleTimes[k]=0;}}
    else if(masked){for(int i=0;i<5;i++){int k=keys[i];stdControl_aKeyInfo[k]=saved[i][0];stdControl_aKeyPressed[k]=saved[i][1];stdControl_aKeyIdleTimes[k]=saved[i][2];}masked=0;}
}
static void presetOffsetControl(){
    const int keys[]={DIK_NUMPAD8,DIK_NUMPAD4,DIK_NUMPAD6,DIK_NUMPAD2,DIK_NUMPAD5};unsigned down=0;
    for(int i=0;i<5;i++)if(stdControl_ReadKey(keys[i],NULL))down|=1u<<i;
    unsigned pressed=down&~presetOffsetKeys;presetOffsetKeys=down;if(!presetLoaded)return;
    if(pressed&16)presetOffsetRight=presetOffsetUp=0;
    else {presetOffsetUp+=((pressed&1)?1:0)-((pressed&8)?1:0);presetOffsetRight+=((pressed&4)?1:0)-((pressed&2)?1:0);}
}
static uint64_t presetSelectionRevision;
static std::filesystem::path presetUserDir(){return std::filesystem::absolute(getenv("GEOSITH_PRESET_DIR")?getenv("GEOSITH_PRESET_DIR"):"presets");}
static void presetRefresh(){
    std::string chosen=presetIndex>=0&&presetIndex<(int)presetEntries.size()?presetEntries[presetIndex].path.string():"";
    presetEntries.clear();std::set<std::string> seen;
    std::vector<std::filesystem::path> dirs={presetUserDir()};
#ifdef _WIN32
    auto exe=lightBakerExeDir();dirs.push_back(exe/"presets");dirs.push_back(exe.parent_path().parent_path()/"presets");
#endif
    for(auto &dir:dirs)if(std::filesystem::is_directory(dir))for(auto &file:std::filesystem::directory_iterator(dir)){
        if(file.is_symlink()||!file.is_regular_file()||file.path().extension()!=".gsp")continue;
        auto path=std::filesystem::weakly_canonical(file.path());if(seen.insert(path.string()).second)presetEntries.push_back({path,path.stem().string()});
    }
    std::sort(presetEntries.begin(),presetEntries.end(),[](const auto &a,const auto &b){return a.name<b.name;});presetIndex=-1;
    for(int i=0;i<(int)presetEntries.size();i++)if(presetEntries[i].path.string()==chosen)presetIndex=i;
}
static void presetClearSelection(){presetThings.clear();presetSectors.clear();presetMount={-1,-1,-1,-1};presetSelectionRevision=geometryDocumentRevision;}
static void presetReset(){presetFreePreviewMaterials();presetLoaded.reset();presetIndex=-1;presetEntries.clear();presetClearSelection();presetEnterDown=0;presetOffsetRight=presetOffsetUp=0;presetOffsetKeys=0;}
static int presetEscape(){if(tool!=6||!presetLoaded||mapActive||builderAsset>=0)return 0;presetLoaded.reset();say("Presets: click sectors/things to select; Enter creates a preset. Shift+click selects sectors.");return 1;}
static void presetError(const std::string &error){char16_t text[512];stdString_CharToWchar(text,error.c_str(),512);jkGuiDialog_ErrorDialog(u"Presets",text);}
static void presetTestMenu(jkGuiMenu *menu){menu->lastClicked=1;}
static bool presetLoad(int index)try{
    if(index<0||index>=(int)presetEntries.size())return false;auto &file=presetEntries[index].path;
    auto size=std::filesystem::file_size(file);if(size>ED_NET_LIMIT)throw std::runtime_error("Preset file is too large.");
    EdResources::Bytes bytes((size_t)size);std::ifstream in(file,std::ios::binary);if(!in.read((char*)bytes.data(),size))throw std::runtime_error("Cannot read preset.");
    auto preset=std::make_unique<EdPreset>();if(!edPresetDecode(bytes,*preset))throw std::runtime_error("Invalid or incompatible preset file.");
    presetFreePreviewMaterials();presetLoaded=std::move(preset);presetIndex=index;presetOffsetRight=presetOffsetUp=0;return true;
}catch(const std::exception &e){say(e.what());return false;}
static void presetResources(EdPreset &p){
    std::set<std::string> used;
    std::map<int,int> materials;
    auto face=[&](EdFace &f){auto it=materials.find(f.material);if(it==materials.end()){
        if(f.material<0||f.material>=paletteCount)throw std::runtime_error("Invalid material in preset.");
        int id=(int)p.materials.size();p.materials.push_back(paletteNames[f.material]);materials[f.material]=id;used.insert(paletteNames[f.material]);f.material=id;
    }else f.material=it->second;};
    for(int s=0;s<p.model.count;s++){auto &cell=p.model.c[s];used.insert(cell.sound);used.insert(cell.colormap);for(int f=0;f<cell.count;f++)face(cell.f[f]);}
    for(int m=0;m<p.model.meshCount;m++){for(int f=0;f<p.model.meshes[m].geometry.count;f++)face(p.model.meshes[m].geometry.f[f]);for(size_t l=0;l<p.model.meshes[m].paint.size();l++)used.insert(p.model.meshes[m].paint[l].material);}
    if(p.materials.empty())p.materials.push_back("dflt.mat");
    for(int t=0;t<p.model.templateCount;t++)for(int v=0;v<p.model.templates[t].count;v++)used.insert(p.model.templates[t].params[v].value);
    for(int c=0;c<p.model.cogCount;c++){used.insert(p.model.cogs[c].file);for(int v=0;v<p.model.cogs[c].count;v++)used.insert(p.model.cogs[c].params[v].value);}
    // Follow filenames in custom script/model/soundclass resources; never bundle
    // unreferenced episode assets or the stock game archives.
    std::set<std::string> lower;for(auto name:used){for(char &c:name)c=(char)tolower((unsigned char)c);lower.insert(name);}used=std::move(lower);
    bool added=true;while(added){added=false;for(auto &entry:customResources.files){
        std::string name=entry.first.substr(entry.first.find_last_of('/')+1);if(p.resources.files.count(entry.first)||!used.count(name))continue;
        p.resources.files[entry.first]=entry.second;added=true;
        if(!entry.first.ends_with(".mat")&&!entry.first.ends_with(".wav")&&!entry.first.ends_with(".cmp")){
            std::string text(entry.second->begin(),entry.second->end());for(char &c:text)c=(char)tolower((unsigned char)c);
            for(auto &dependency:customResources.files){auto base=dependency.first.substr(dependency.first.find_last_of('/')+1);if(text.find(base)!=std::string::npos)used.insert(base);}
        }
    }}
}
static void presetCreate(){
    auto preset=std::make_unique<EdPreset>();std::string error;
    if(!edPresetCapture(*preset,*model,presetSectors,presetThings,presetMount,error)){presetError(error);return;}
    char16_t name[32]=u"preset";
    if(getenv("GEOSITH_PRESET_TEST")&&getenv("GEOSITH_PRESET_DIR"))stdString_CharToWchar(name,"Preset With Spaces",32);
    jkGuiElement e[]={{ELEMENT_TEXT,0,2,NULL,3,{20,90,600,40},1},{ELEMENT_TEXTBOX,0,0,NULL,27,{100,180,440,30},1},{ELEMENT_TEXTBUTTON,1,2,NULL,3,{330,300,250,40},1},{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,300,240,40},1},{ELEMENT_END}};
    for(;;){jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"Name Preset";e[1].wstr=name;e[2].wstr=u"Create Preset";jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&e[2]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[3]);if(getenv("GEOSITH_PRESET_TEST")&&getenv("GEOSITH_PRESET_DIR"))menu.idkFunc=presetTestMenu;int action=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);if(action<0)return;
        std::string file;if(!edPresetFilename(name,file)){presetError("Use 1-27 letters, numbers, spaces, underscores or hyphens. Reserved device names are not allowed.");continue;}
        auto dir=presetUserDir(),path=dir/(std::string(file)+".gsp");if(std::filesystem::exists(path)){presetError("A preset with that name already exists. Choose a new name or delete the old preset.");continue;}
        presetResources(*preset);auto data=edPresetEncode(*preset);std::filesystem::create_directories(dir);auto temp=path;temp+=".tmp";
        {std::ofstream out(temp,std::ios::binary);out.write((char*)data.data(),data.size());out.close();if(!out)throw std::runtime_error("Could not write preset library file.");}std::filesystem::rename(temp,path);
        presetRefresh();for(int i=0;i<(int)presetEntries.size();i++)if(presetEntries[i].path==std::filesystem::weakly_canonical(path))presetLoad(i);
        presetClearSelection();say("Preset saved to the shared library. Aim at a surface and Fire 1 to place; Escape selects again.");return;
    }
}
static void presetMenu(){
    BuilderMenuState state=builderMenuBegin();
    try{presetRefresh();int page=presetIndex<0?0:presetIndex/8;
        for(;;){jkGuiElement e[17]={};std::array<std::array<char16_t,128>,8> labels;
            e[0]={ELEMENT_TEXT,0,2,NULL,3,{20,15,600,40},1};
            for(int i=0;i<8;i++){int id=page*8+i;e[i+1]={ELEMENT_TEXTBUTTON,100+i,0,NULL,3,{30,70+i*34,580,30},id<(int)presetEntries.size()};if(id<(int)presetEntries.size())stdString_CharToWchar(labels[i].data(),presetEntries[id].name.c_str(),128);}
            e[9]={ELEMENT_TEXTBUTTON,1,0,NULL,3,{20,355,195,32},1};e[10]={ELEMENT_TEXTBUTTON,2,0,NULL,3,{225,355,190,32},presetIndex>=0};e[11]={ELEMENT_TEXTBUTTON,3,0,NULL,3,{425,355,195,32},1};
            e[12]={ELEMENT_TEXTBUTTON,4,0,NULL,3,{20,405,150,32},page>0};e[13]={ELEMENT_TEXTBUTTON,5,0,NULL,3,{470,405,150,32},(page+1)*8<(int)presetEntries.size()};e[14]={ELEMENT_TEXTBUTTON,-1,2,"GUI_CLOSE",3,{210,420,220,40},1};e[15].type=ELEMENT_END;
            jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"Preset Library";for(int i=0;i<8;i++)e[i+1].wstr=labels[i].data();e[9].wstr=u"Create Preset";e[10].wstr=u"Delete Selected";e[11].wstr=u"Select Geometry";e[12].wstr=u"Previous";e[13].wstr=u"Next";jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[14]);int action=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);
            if(action<0)break;
            if(action>=100){if(presetLoad(page*8+action-100))break;continue;}
            if(action==1){presetCreate();break;}
            if(action==2&&presetIndex>=0){auto path=presetEntries[presetIndex].path;std::filesystem::remove(path);presetLoaded.reset();presetIndex=-1;presetRefresh();page=0;say("Preset removed from the library. Placed copies remain in the level.");}
            if(action==3){presetLoaded.reset();break;}if(action==4)page--;if(action==5)page++;
        }
    }catch(const std::exception &e){presetError(e.what());}
    builderMenuEnd(state);fireDown=grabDown=middleDown=1;edResetDoubleTap(&selectionTap,1);
}
static rdMatrix34 presetTransform;
static EdVec presetVector(EdVec p){rdVector3 in=rdvec(p),out;rdMatrix_TransformVector34(&out,&in,&presetTransform);return vec(out);}
static EdVec presetPoint(EdVec p){return edAdd(presetVector(p),vec(presetTransform.scale));}
static EdVec presetAngles(EdVec angles){rdMatrix34 a,b;rdVector3 in=rdvec(angles),out;rdMatrix_BuildRotate34(&a,&in);b=a;b.rvec=rdvec(presetVector(vec(a.rvec)));b.lvec=rdvec(presetVector(vec(a.lvec)));b.uvec=rdvec(presetVector(vec(a.uvec)));rdMatrix_ExtractAngles34(&b,&out);return vec(out);}
static bool presetAim(EdHit hit){
    if(!presetLoaded||hit.cell<0)return false;const EdModel &p=presetLoaded->model;EdVec origin={},normal=edV(0,1,0);
    if(p.count){const EdFace &face=p.c[presetLoaded->mountCell].f[presetLoaded->mountFace];normal=edNormal(&face);for(int v=0;v<face.count;v++)origin=edAdd(origin,face.v[v].p);origin=edScale(origin,1.0/face.count);}
    else {int n=p.objectCount+p.spawnCount;for(int i=0;i<p.objectCount;i++)origin=edAdd(origin,p.objects[i].position);for(int i=0;i<p.spawnCount;i++)origin=edAdd(origin,p.spawns[i].position);origin=edScale(origin,1.0/n);}
    EdVec destination=edScale(edNormal(&model->c[hit.cell].f[hit.face]),-1);
    rdMatrix34 from,to;rdVector3 a=rdvec(normal),b=rdvec(destination);rdMatrix_BuildFromLook34(&from,&a);rdMatrix_BuildFromLook34(&to,&b);
    auto rotate=[&](EdVec v){return edAdd(edAdd(edScale(vec(to.rvec),edDot(v,vec(from.rvec))),edScale(vec(to.lvec),edDot(v,vec(from.lvec)))),edScale(vec(to.uvec),edDot(v,vec(from.uvec))));};
    rdMatrix_Identity34(&presetTransform);presetTransform.rvec=rdvec(rotate(edV(1,0,0)));presetTransform.lvec=rdvec(rotate(edV(0,1,0)));presetTransform.uvec=rdvec(rotate(edV(0,0,1)));
    EdVec target=edSnapFacePoint(&model->c[hit.cell].f[hit.face],hit.point);
    // Screen-right projected onto the mounting plane keeps the offsets on the
    // wall, and also gives intuitive directions on floors and ceilings.
    rdMatrix34 viewWorld;if(editorViewReady)rdMatrix_InvertOrtho34(&viewWorld,&editorView.orient);else viewWorld=to;
    EdVec right=vec(viewWorld.rvec);right=edSub(right,edScale(destination,edDot(right,destination)));
    double magnitude=sqrt(edDot(right,right));right=magnitude>1e-6?edScale(right,1/magnitude):vec(to.rvec);
    EdVec up=edCross(right,destination);
    target=edAdd(target,edAdd(edScale(right,presetOffsetRight*ED_GRID),edScale(up,presetOffsetUp*ED_GRID)));
    if(!p.count)target=edSub(target,edScale(destination,ED_GRID));
    presetTransform.scale=rdvec(edSub(target,rotate(origin)));return true;
}
static void presetPlace(EdHit hit){
    if(!presetAim(hit))return;
    try{
        EdResources merged=customResources;for(auto &file:presetLoaded->resources.files){auto found=merged.files.find(file.first);if(found!=merged.files.end()&&*found->second!=*file.second)throw std::runtime_error("Preset resource conflicts with this level: "+file.first);merged.files[file.first]=file.second;}
        // Clients may place presets using already-installed resources. Hosts own
        // new resource imports, matching the existing multiplayer resource policy.
        if(sithNet_isMulti&&!sithNet_isServer&&merged.encode()!=customResourceBytes)throw std::runtime_error("Ask the host to place this preset once to install its custom resources first.");
        auto clip=std::make_unique<EdModel>(presetLoaded->model);std::vector<int> materials;
        for(auto &name:presetLoaded->materials){int id=addMaterialName(name.c_str());if(id<0)throw std::runtime_error("Material palette is full.");materials.push_back(id);}
        for(int s=0;s<clip->count;s++){auto &cell=clip->c[s];cell.thrust=presetVector(cell.thrust);for(int f=0;f<cell.count;f++){auto &face=cell.f[f];face.material=materials[face.material];face.customUV=1;for(int v=0;v<face.count;v++)face.v[v].p=presetPoint(face.v[v].p);}}
        for(int m=0;m<clip->meshCount;m++)for(int f=0;f<clip->meshes[m].geometry.count;f++){auto &face=clip->meshes[m].geometry.f[f];face.material=materials[face.material];}
        for(int i=0;i<clip->spawnCount+clip->objectCount;i++){
            EdVec *position,*rotation;EdThingAttributes *attrs;if(i<clip->spawnCount){position=&clip->spawns[i].position;rotation=&clip->spawns[i].rotation;attrs=&clip->spawns[i].attributes;}else{auto &obj=clip->objects[i-clip->spawnCount];position=&obj.position;rotation=&obj.rotation;attrs=&obj.attributes;}
            *position=presetPoint(*position);*rotation=presetAngles(*rotation);attrs->velocity=presetVector(attrs->velocity);
            for(int f=0;f<attrs->frameCount;f++){attrs->frames[f].position=presetPoint(attrs->frames[f].position);attrs->frames[f].rotation=presetAngles(attrs->frames[f].rotation);}
        }
        if(clip->spawnCount)clip->spawn=clip->spawns[0].position;
        std::string error;if(!edPresetInsert(*candidate,*model,*clip,error)){say(error.c_str());return;}
        for(int i=model->objectCount;i<candidate->objectCount;i++)if(objectSector(candidate,candidate->objects[i].position)<0)throw std::runtime_error("A preset thing would lie outside the level. Adjust placement.");
        EdResources previous=customResources;setCustomResources(std::move(merged));
        bool valid=true;for(int id:materials)valid&=editorMaterial(id)!=NULL;
        if(!valid){setCustomResources(std::move(previous));throw std::runtime_error("A preset material could not be loaded.");}
        remember();if(!commit())setCustomResources(std::move(previous));else {cogSourceCount=0;presetClearSelection();say("Preset placed with things, frames and COGs. Ctrl+Z: undo.");}
    }catch(const std::exception &e){say(e.what());}
}
static void presetControl(int fire,int grab,int middle,int shift){
    if(presetSelectionRevision!=geometryDocumentRevision)presetClearSelection();
    presetOffsetControl();
    int ctrl=stdControl_ReadKey(DIK_LCONTROL,NULL)||stdControl_ReadKey(DIK_RCONTROL,NULL);
    int historyKey=ctrl&&stdControl_ReadKey(DIK_Z,NULL)?(shift?2:1):ctrl&&stdControl_ReadKey(DIK_Y,NULL)?2:0;
    int historyPressed=historyKey&&!historyDown;historyDown=historyKey;
    if(historyPressed){historyStep(historyKey==2);fireDown=fire;grabDown=grab;edResetDoubleTap(&selectionTap,fire);return;}
    if(edDoubleTap(&selectionTap,!presetLoaded&&fire&&!grab,stdPlatform_GetTimeMsec(),300)){
        presetClearSelection();say("Preset selection cleared.");fireDown=fire;grabDown=grab;return;
    }
    int enter=stdControl_ReadKey(DIK_RETURN,NULL)||stdControl_ReadKey(DIK_NUMPADENTER,NULL),pressed=enter&&!presetEnterDown;presetEnterDown=enter;
    if(middle||pressed){presetMenu();return;}
    int wheel=stdControl_aAxisStates[AXIS_MOUSE_Z];if(wheel){presetRefresh();if(!presetEntries.empty())presetLoad((presetIndex+(wheel>0?1:(int)presetEntries.size()-1)+(int)presetEntries.size())%(int)presetEntries.size());}
    EdVec origin,direction;ray(&origin,&direction);edRaySurface(model,origin,direction,&hover,ctrl);
    if(fire&&!fireDown&&!grab){
        if(ctrl&&hover.cell>=0){presetLoaded.reset();presetSectors[hover.cell]=1;presetMount=hover;say("Preset mounting face selected. Enter: Create Preset.");}
        else if(presetLoaded&&!shift)presetPlace(hover);
        else {int thing=shift?-10000:pickEditorObject(origin,direction,hover.cell>=0?hover.distance:1000);
            if(thing!=-10000){if(presetThings.count(thing))presetThings.erase(thing);else presetThings.insert(thing);}
            else if(hover.cell>=0){bool on=!presetSectors[hover.cell];presetSectors[hover.cell]=on;if(on&&(presetMount.cell<0||!presetSectors[presetMount.cell]))presetMount=hover;}
            say("Preset selection updated. Enter: Create Preset. First sector face is the mounting face.");
        }
    }fireDown=fire;grabDown=grab;
}
// A textured preview is rendered through the destination wall,
// so the volume being placed just beyond it remains visible.
static void presetPreviewWorld(){
    if(tool!=6||!presetLoaded||builderAsset>=0||mapActive||!presetAim(hover))return;
    std::vector<rdVector3> vertices,normals;std::vector<rdVector2> uv;std::vector<flex_t> lights,dynamic;std::vector<rdFace> faces;std::vector<std::vector<int>> indices;
    auto add=[&](const EdFace &source){rdFace f={};f.numVertices=source.count;f.material=presetPreviewMaterial(source);f.geometryMode=(rdGeoMode_t)4;f.lightingMode=RD_LIGHTMODE_FULLYLIT;f.textureMode=RD_TEXTUREMODE_PERSPECTIVE;f.type=edSurfaceAttributes(&source).faceFlags|1;f.wallCel=0;f.normal=rdvec(edNormal(&source));std::vector<int> index;
        for(int v=0;v<source.count;v++){index.push_back((int)vertices.size());vertices.push_back(rdvec(source.v[v].p));normals.push_back(f.normal);uv.push_back({(flex_t)source.v[v].u,(flex_t)source.v[v].v});lights.push_back(1);dynamic.push_back(0);}faces.push_back(f);indices.push_back(std::move(index));};
    const EdModel &p=presetLoaded->model;
    for(int s=0;s<p.count;s++)for(int f=0;f<p.c[s].count;f++)if(p.c[s].f[f].neighbor<0)add(p.c[s].f[f]);
    // Include authored model geometry at each thing's pose in the same preview.
    for(int o=0;o<p.objectCount;o++){const auto &obj=p.objects[o];int t=edTemplateIndex(&p,obj.templateName);if(t<0)continue;std::string file;for(int q=0;q<p.templates[t].count;q++)if(!strcmp(p.templates[t].params[q].key,"model3d"))file=p.templates[t].params[q].value;
        for(int a=0;a<p.assetCount;a++)if(file==std::string(p.assets[a].name)+".3do"){rdMatrix34 transform;rdVector3 angle=rdvec(obj.rotation);rdMatrix_BuildRotate34(&transform,&angle);for(int m=0;m<p.meshCount;m++)if(p.meshes[m].asset==a)for(int f=0;f<p.meshes[m].geometry.count;f++){EdFace face=p.meshes[m].geometry.f[f];for(int v=0;v<face.count;v++){rdVector3 local=rdvec(edSub(face.v[v].p,p.assets[a].origin)),world;rdMatrix_TransformVector34(&world,&local,&transform);face.v[v].p=edAdd(vec(world),obj.position);}add(face);}}
    }
    int objectZ=rdGetZBufferMethod();rdCache_Flush();rdSetZBufferMethod(RD_ZBUFFER_NOREAD_NOWRITE);
    for(int i=0;i<p.objectCount+p.spawnCount;i++){
        const char *name=i<p.objectCount?p.objects[i].templateName:"walkplayer";
        if(edTemplateIndex(&p,name)>=0)continue;
        SithThing *definition=sithTemplate_GetTemplate(name);if(!definition||definition->renderData.type!=RD_THING_MODEL3||!definition->renderData.model3)continue;
        rdModel3 *mesh=definition->renderData.model3;rdThing ghost={};rdThing_NewEntry(&ghost,NULL);
        if(!rdThing_SetModel3(&ghost,mesh)){rdThing_FreeEntry(&ghost);continue;}
        ghost.desiredLightMode=ghost.curLightMode=RD_LIGHTMODE_FULLYLIT;
        EdVec position=i<p.objectCount?p.objects[i].position:p.spawns[i-p.objectCount].position,angles=i<p.objectCount?p.objects[i].rotation:p.spawns[i-p.objectCount].rotation;
        rdMatrix34 orient;rdVector3 rotation=rdvec(presetAngles(angles));rdMatrix_BuildRotate34(&orient,&rotation);orient.scale=rdvec(presetPoint(position));rdThing_Draw(&ghost,&orient);rdCache_Flush();
        rdThing_FreeEntry(&ghost);
    }
    rdSetZBufferMethod(objectZ);
    if(faces.empty())return;for(size_t f=0;f<faces.size();f++)faces[f].vertexPosIdx=faces[f].vertexUVIdx=indices[f].data();
    std::vector<rdMesh> meshes;std::vector<rdHierarchyNode> nodes;
    size_t startFace=0,startVertex=0;double radius=0;
    for(auto v:vertices)radius=fmax(radius,sqrt(v.x*v.x+v.y*v.y+v.z*v.z));
    while(startFace<faces.size()){
        size_t end=startFace,n=0;while(end<faces.size()&&n+faces[end].numVertices<=480)n+=faces[end++].numVertices;
        rdMesh mesh={};mesh.mesh_num=(int)meshes.size();mesh.geometryMode=(rdGeoMode_t)4;mesh.lightingMode=RD_LIGHTMODE_FULLYLIT;mesh.textureMode=RD_TEXTUREMODE_PERSPECTIVE;
        mesh.numVertices=mesh.numUVs=(int)n;mesh.numFaces=(int)(end-startFace);mesh.aVertices=vertices.data()+startVertex;mesh.aTexVerticies=uv.data()+startVertex;mesh.vertexNormals=normals.data()+startVertex;mesh.vertices_i=lights.data()+startVertex;mesh.vertices_unk=dynamic.data()+startVertex;mesh.faces=faces.data()+startFace;mesh.radius=radius;
        for(size_t f=startFace;f<end;f++)for(int &i:indices[f])i-=(int)startVertex;
        meshes.push_back(mesh);startFace=end;startVertex+=n;
    }
    nodes.resize(meshes.size());for(size_t i=0;i<nodes.size();i++){auto &node=nodes[i];node.idx=node.meshIdx=(int)i;node.parent=i?nodes.data():nullptr;node.child=!i&&nodes.size()>1?nodes.data()+1:nullptr;node.numChildren=i?0:(int)nodes.size()-1;node.nextSibling=i&&i+1<nodes.size()?nodes.data()+i+1:nullptr;}
    rdModel3 preview={};preview.numGeos=1;preview.numHNodes=(int)nodes.size();preview.aGeos[0].numMeshes=(int)meshes.size();preview.aGeos[0].aMeshes=meshes.data();preview.aHierarchyNodes=nodes.data();preview.radius=radius;
    rdThing thing={};rdThing_NewEntry(&thing,NULL);int z=rdGetZBufferMethod();rdCache_Flush();rdSetZBufferMethod(RD_ZBUFFER_NOREAD_NOWRITE);
    if(rdThing_SetModel3(&thing,&preview)){thing.desiredLightMode=thing.curLightMode=RD_LIGHTMODE_FULLYLIT;rdThing_Draw(&thing,&presetTransform);rdCache_Flush();}rdThing_FreeEntry(&thing);rdSetZBufferMethod(z);
}
static void presetOverlay(){
    if(tool!=6)return;int yellow=palettePixel(highlightCanvas->pVBuffer,255,225,32),red=palettePixel(highlightCanvas->pVBuffer,255,40,40),purple=palettePixel(highlightCanvas->pVBuffer,200,64,255);
    int ctrl=stdControl_ReadKey(DIK_LCONTROL,NULL)||stdControl_ReadKey(DIK_RCONTROL,NULL);
    EdVec origin,direction;ray(&origin,&direction);int thing=ctrl?-10000:pickEditorObject(origin,direction,hover.cell>=0?hover.distance:1000);
    for(int s=0;s<model->count;s++)if(presetSectors[s]||(!presetLoaded&&hover.cell==s&&thing==-10000))for(int f=0;f<model->c[s].count;f++){EdHit h={s,f,-1,-1};drawHit(&h,presetSectors[s]?red:yellow);}
    if(ctrl&&hover.cell>=0)drawHit(&hover,yellow);
    for(int id:presetThings){if((id>=0&&id>=model->objectCount)||(id<0&&-id-1>=model->spawnCount)||!thingOverlayVisible(id))continue;EdVec center=thingPose(model,id).position,end=objectFacingHandle(model,id);vertexCircle(center,1,purple);line(center,end,purple);vertexCircle(end,1,purple);}
    if(!presetLoaded&&thing!=-10000&&!presetThings.count(thing))vertexCircle(thingPose(model,thing).position,1,yellow);
    if(presetLoaded&&presetAim(hover)){
        for(int s=0;s<presetLoaded->model.count;s++)for(int f=0;f<presetLoaded->model.c[s].count;f++){const auto &face=presetLoaded->model.c[s].f[f];if(face.neighbor>=0)continue;for(int v=0;v<face.count;v++)line(presetPoint(face.v[v].p),presetPoint(face.v[(v+1)%face.count].p),yellow);}
        for(int i=0;i<presetLoaded->model.objectCount;i++)vertexCircle(presetPoint(presetLoaded->model.objects[i].position),1,yellow);
    }
    auto b=highlightCanvas->pVBuffer;int x=(std::max)(8,b->format.width-400),y=(std::max)(60,b->format.height-140);
    std::string label=presetLoaded&&presetIndex>=0?"Preset: "+presetEntries[presetIndex].name:"Presets: select sectors and things";
    editorDrawText(b,jkGui_stdFonts[0],x,y,390,label.c_str(),1);editorDrawText(b,jkGui_stdFonts[0],x,y+24,390,"Enter / Middle mouse: preset library",1);editorDrawText(b,jkGui_stdFonts[0],x,y+48,390,"Wheel: preset | Escape: selection mode",1);editorDrawText(b,jkGui_stdFonts[0],x,y+72,390,presetLoaded?"Fire 1: place | Shift+click: select sector":"Click: select | Shift+click: sector",1);
    if(presetLoaded){char offset[128];snprintf(offset,sizeof(offset),"Numpad 8/2/4/6: grid (%.0f, %.0f) | 5: center",presetOffsetRight,presetOffsetUp);editorDrawText(b,jkGui_stdFonts[0],x,y+96,390,offset,1);}
}
static int presetSelfTest(SithThing *player){
    // Validation retries reuse menu elements: freeing their labels must be safe
    // before InitMenu releases/replaces those same allocations on the next pass.
    jkGuiElement retry[]={{ELEMENT_TEXTBUTTON,1,2,"GUI_CANCEL",3,{60,300,240,40},1},{ELEMENT_END}};
    for(int i=0;i<3;i++){jkGuiMenu menu={retry,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);freeMenuStrings(retry);if(retry[0].strAlloced||retry[0].wHintTextAlloced)return 0;}
    if(!getenv("GEOSITH_PRESET_DIR"))return 0;
    active=1;tool=6;edDefault(candidate);candidate->assetCount=1;candidate->assets[0].saved=1;strcpy(candidate->assets[0].name,"preset_platform");
    if(!edMeshPrimitive(candidate,0,0,edV(0,0,.2),0))return 0;
    EdTemplate t={};strcpy(t.name,"preset_platform");strcpy(t.parent,"_walkstruct");t.count=2;strcpy(t.params[0].key,"model3d");strcpy(t.params[0].value,"preset_platform.3do");strcpy(t.params[1].key,"move");strcpy(t.params[1].value,"path");candidate->templates[candidate->templateCount++]=t;
    EdObject object={};strcpy(object.templateName,t.name);object.position=edV(0,0,.2);object.attributes.pathOverride=1;object.attributes.frameCount=2;object.attributes.frames[0].position=object.position;object.attributes.frames[1].position=edV(0,0,.8);candidate->objects[candidate->objectCount++]=object;
    char *text=cogSourceText("12_trooperelev.cog");EdPlacedCog cog={};int ok=text&&edCogParseScript(&cog,"12_trooperelev.cog",text);edFree(text);if(!ok)return 0;
    for(int p=0;p<cog.count;p++){if(cog.params[p].type==ED_COG_THING)strcpy(cog.params[p].value,"1");if(cog.params[p].type==ED_COG_SECTOR)cog.params[p].cell=0;}candidate->cogs[candidate->cogCount++]=cog;
    if(!commit())return 0;clearHistory();presetClearSelection();presetSectors[0]=1;presetThings.insert(0);presetMount={0,2,-1,-1};
    auto state=builderMenuBegin();presetCreate();builderMenuEnd(state);if(!presetLoaded||presetEntries.empty())return 0;
    const int offsetKeys[]={DIK_NUMPAD8,DIK_NUMPAD6,DIK_NUMPAD4,DIK_NUMPAD2};
    const double expectedRight[]={0,1,0,0},expectedUp[]={1,1,1,0};
    for(int i=0;i<4;i++){stdControl_UpdateKeyState(offsetKeys[i],1,stdPlatform_GetTimeMsec());presetOffsetControl();presetOffsetControl();if(presetOffsetRight!=expectedRight[i]||presetOffsetUp!=expectedUp[i])return 0;stdControl_UpdateKeyState(offsetKeys[i],0,stdPlatform_GetTimeMsec());presetOffsetControl();}
    presetOffsetRight=3;presetOffsetUp=-2;stdControl_UpdateKeyState(DIK_NUMPAD5,1,stdPlatform_GetTimeMsec());presetOffsetControl();stdControl_UpdateKeyState(DIK_NUMPAD5,0,stdPlatform_GetTimeMsec());presetOffsetControl();if(presetOffsetRight||presetOffsetUp)return 0;
    // Place into an independently-created level, not back into the source.
    edDefault(candidate);if(!commit())return 0;clearHistory();EdHit target={0,4,-1,-1};target.point=edV(0,1,.5);presetPlace(target);
    if(model->count!=2||model->objectCount!=1||model->cogCount!=1||undoCount!=1||model->objects[0].attributes.frameCount!=2)return 0;
    if(fabs(model->objects[0].position.y-2)>.001||fabs(model->objects[0].attributes.frames[1].position.y-2)>.001)return 0;
    // Exercise the weapon 6 keyboard path, not just the history implementation.
    historyDown=0;stdControl_UpdateKeyState(DIK_LCONTROL,1,stdPlatform_GetTimeMsec());stdControl_UpdateKeyState(DIK_Z,1,stdPlatform_GetTimeMsec());presetControl(0,0,0,0);
    stdControl_UpdateKeyState(DIK_Z,0,stdPlatform_GetTimeMsec());presetControl(0,0,0,0);
    if(model->count!=1||model->objectCount)return 0;
    stdControl_UpdateKeyState(DIK_Y,1,stdPlatform_GetTimeMsec());presetControl(0,0,0,0);
    stdControl_UpdateKeyState(DIK_Y,0,stdPlatform_GetTimeMsec());stdControl_UpdateKeyState(DIK_LCONTROL,0,stdPlatform_GetTimeMsec());presetControl(0,0,0,0);
    if(model->count!=2||model->objectCount!=1)return 0;
    // Floor/ceiling placement rotates the whole lift and its frame direction.
    EdHit floor={0,0,-1,-1};floor.point=edV(0,0,0);if(!presetAim(floor))return 0;
    EdVec before=presetLoaded->model.objects[0].attributes.frames[0].position,after=presetLoaded->model.objects[0].attributes.frames[1].position;
    EdVec delta=edSub(presetPoint(after),presetPoint(before)),rotated=presetVector(edSub(after,before));if(edDot(edSub(delta,rotated),edSub(delta,rotated))>1e-8||fabs(edDot(delta,delta)-.36)>.001)return 0;
    if(const char *file=getenv("GEOSITH_PRESET_ORIENTATION_TEST")){
        std::ifstream input(file,std::ios::binary);EdResources::Bytes bytes((std::istreambuf_iterator<char>(input)),{});auto saved=std::make_unique<EdPreset>();if(!edPresetDecode(bytes,*saved))return 0;presetLoaded=std::move(saved);
        for(int f=0;f<model->c[0].count;f++){
            const EdFace &wall=model->c[0].f[f];EdHit hit={0,f,-1,-1};hit.point=wall.v[0].p;if(!presetAim(hit))return 0;
            for(int s=0;s<presetLoaded->model.count;s++)for(int q=0;q<presetLoaded->model.c[s].count;q++)for(int v=0;v<presetLoaded->model.c[s].f[q].count;v++)
                if(edDot(edNormal(&wall),edSub(presetPoint(presetLoaded->model.c[s].f[q].v[v].p),hit.point))>.001)return 0;
        }
        puts("PRESET ORIENTATION PASS: saved preset extends outward on every room face");
    }
    active=0;if(!startPlacedCogs(model))return 0;active=1;suspendPlacedCogs(0);
    if(!writeEditorGob(model,"Episode/sithed_preset_test.gob","Preset Test",0))return 0;
    if(!presetEscape()||presetLoaded)return 0;
    puts("GEOSITH_PRESET PASS: library create/load, cross-level lift placement, frames, COG startup, undo/redo and GOB save");return 1;
}
static int presetTestFrame(SithThing *player){
    static int frame;if(!frame){int ok=bindEditorWorld()&&presetSelfTest(player);printf("PRESET TEST %s\n",ok?"PASS":"FAIL");fflush(stdout);if(!ok||!getenv("GEOSITH_PRESET_CAPTURE"))exit(ok?0:2);
        presetLoad(0);active=stressRunning=1;tool=6;player->position=rdvec(edV(0,-.8,.5));rdVector3 angles={0};rdMatrix_BuildRotate34(&player->orient,&angles);player->actorParams.headPYR=angles;setEditorFlight(player,1);sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);}
#ifdef SDL2_RENDER
    if(frame==20){std3D_ScreenshotWindow(getenv("GEOSITH_PRESET_CAPTURE"));exit(0);}
#endif
    frame++;return 1;
}
