// Optional JED Light Baker integration. DLL code runs only in the x86 helper.
static int lightBakerMode;
static void lightBakerProperties();
#ifdef _WIN32
static std::filesystem::path lightBakerExeDir(){wchar_t name[32768];DWORD n=GetModuleFileNameW(NULL,name,32768);return n?std::filesystem::path(std::wstring(name,n)).parent_path():std::filesystem::path();}
static std::filesystem::path lightBakerPlugin(){
    auto exe=lightBakerExeDir();for(auto dir:{std::filesystem::current_path()/"plugins",exe/"plugins",std::filesystem::current_path().parent_path()/"plugins",exe.parent_path().parent_path()/"plugins"}){auto dll=dir/"Light Baker.dll";if(std::filesystem::is_regular_file(dll))return std::filesystem::absolute(dll);}return {};
}
static HANDLE lightBakerProcess;
static void lightBakerWait(jkGuiMenu *menu){if(lightBakerProcess&&WaitForSingleObject(lightBakerProcess,0)==WAIT_OBJECT_0)menu->lastClicked=1;}
#endif
static bool lightBakerAvailable(){
#ifdef _WIN32
    try{return !lightBakerPlugin().empty()&&std::filesystem::is_regular_file(lightBakerExeDir()/"GeoSithLightBaker.exe");}catch(...){return false;}
#else
    return false;
#endif
}
static bool lightBakerResource(const std::filesystem::path &job,const std::string &name){
    std::string canonical=EdResources::path(name);if(canonical.empty())return false;
    stdFile_t f=jkRes_FileOpen(name.c_str(),"rb");if(!f)return false;int size=jkRes_FileSize(f);if(size<=0||size>64*1024*1024){jkRes_FileClose(f);return false;}
    std::vector<char> data(size);bool ok=jkRes_FileRead(f,data.data(),size)==size;jkRes_FileClose(f);if(!ok)return false;
    auto path=job/"resources"/canonical;std::filesystem::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out.write(data.data(),size);return !!out;
}
// The library stores vertices around its authoring origin. Bake one placed
// instance in world space, then transfer only its lighting back to the library.
static void lightBakerPlaceModels(EdModel *input){
    for(int a=0;a<input->assetCount;a++){
        EdAsset &asset=input->assets[a];if(asset.terrain||!asset.lightBaker)continue;
        int object=-1;char name[64];snprintf(name,sizeof(name),"%s.3do",asset.name);
        for(int o=0;o<input->objectCount;o++){SithThing *t=sithTemplate_GetTemplate(input->objects[o].templateName);
            if(t&&t->renderData.type==RD_THING_MODEL3&&t->renderData.model3&&!__strcmpi(baseName(t->renderData.model3->filename),name)){object=o;break;}}
        if(object<0){asset.lightBaker=0;continue;}
        rdMatrix34 rotation;rdVector3 angles=rdvec(input->objects[object].rotation);rdMatrix_BuildRotate34(&rotation,&angles);
        for(int m=0;m<input->meshCount;m++)if(input->meshes[m].asset==a)for(int f=0;f<input->meshes[m].geometry.count;f++){
            EdFace &face=input->meshes[m].geometry.f[f];
            for(int v=0;v<face.count;v++){rdVector3 local=rdvec(edSub(face.v[v].p,asset.origin)),world;rdMatrix_TransformVector34(&world,&local,&rotation);face.v[v].p=edAdd(vec(world),input->objects[object].position);}
        }
    }
}
static bool lightBakerRestoreModels(EdModel *result,const EdModel *original){
    if(result->assetCount!=original->assetCount||result->meshCount!=original->meshCount)return false;
    for(int a=0;a<result->assetCount;a++)result->assets[a].lightBaker=original->assets[a].lightBaker;
    for(int m=0;m<result->meshCount;m++){
        auto &mesh=result->meshes[m];auto &old=original->meshes[m];if(mesh.asset!=old.asset||mesh.geometry.count!=old.geometry.count)return false;
        if(!original->assets[old.asset].lightBaker||original->assets[old.asset].terrain)continue;
        for(int f=0;f<mesh.geometry.count;f++){if(mesh.geometry.f[f].count!=old.geometry.f[f].count)return false;
            for(int v=0;v<mesh.geometry.f[f].count;v++)mesh.geometry.f[f].v[v].p=old.geometry.f[f].v[v].p;}
    }return true;
}
static void lightBakerRun(){
#ifdef _WIN32
    if(!lightBakerAvailable()){say("Install Light Baker.dll in plugins and GeoSithLightBaker.exe beside GeoSith.exe.");return;}
    if(netEditPending()||terrainSessionOpen()){say("Finish the pending edit or painting session before baking.");return;}
    if(getenv("GEOSITH_BAKER_TEST"))fprintf(stderr,"BAKER: starting\n");
    auto original=std::make_unique<EdModel>(*model);std::filesystem::path job;std::string resultMessage;
    int previousSafe=jkEditor_NetSafeDialog(1),previousModal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    if(getenv("GEOSITH_BAKER_TEST"))fprintf(stderr,"BAKER: menu mode\n");
    try{
        job=std::filesystem::absolute("editor-plugin-data")/("bake-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));std::filesystem::create_directories(job/"cog");
        auto input=std::make_unique<EdModel>(*model);lightBakerPlaceModels(input.get());
        uint32_t size;auto bytes=edWireEncode(input.get(),&size);if(!bytes)throw std::runtime_error("Could not serialize level for Light Baker.");std::ofstream out(job/"input.bin",std::ios::binary);out.write((char*)bytes,size);edFree(bytes);out.close();if(!out)throw std::runtime_error("Could not write Light Baker input.");
        {auto p=sithWorld_g_pCurrentWorld->pLocalPlayer->position;std::ofstream(job/"light-position.txt")<<p.x<<' '<<p.y<<' '<<p.z;}
        std::ofstream names(job/"materials.txt"),selected(job/"selected.txt");std::set<int> used;
        for(int s=0;s<model->count;s++){if(sectorPicks[s])selected<<s<<'\n';for(int f=0;f<model->c[s].count;f++)used.insert(model->c[s].f[f].material);}
        for(int i=0;i<model->meshCount;i++)if(model->assets[model->meshes[i].asset].terrain||model->assets[model->meshes[i].asset].lightBaker)for(int f=0;f<model->meshes[i].geometry.count;f++)used.insert(model->meshes[i].geometry.f[f].material);
        for(int i=0;i<paletteCount;i++){names<<paletteNames[i]<<'\n';if(used.count(i)){std::string name=std::string("mat/")+paletteNames[i];if(!lightBakerResource(job,name))lightBakerResource(job,std::string("3do/mat/")+paletteNames[i]);}}
        names.close();selected.close();lightBakerResource(job,std::string("misc/cmp/")+model->header.colormap);
        for(int s=0;s<model->count;s++)if(model->c[s].colormap[0])lightBakerResource(job,std::string("misc/cmp/")+model->c[s].colormap);
        if(getenv("GEOSITH_BAKER_TEST"))fprintf(stderr,"BAKER: resources ready\n");
        auto helper=lightBakerExeDir()/"GeoSithLightBaker.exe";auto dll=lightBakerPlugin();std::wstring command=L"\""+helper.wstring()+L"\" \""+dll.wstring()+L"\" \""+job.wstring()+L"\"";
        if(getenv("GEOSITH_BAKER_TEST"))command+=L" --test";
        STARTUPINFOW startup={sizeof(startup)};PROCESS_INFORMATION process={};startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_SHOWNORMAL;
        if(!CreateProcessW(helper.c_str(),command.data(),NULL,NULL,FALSE,0,NULL,job.c_str(),&startup,&process))throw std::runtime_error("Could not start the Light Baker compatibility host.");
        CloseHandle(process.hThread);lightBakerProcess=process.hProcess;
        jkGuiElement e[]={{ELEMENT_TEXT,0,2,NULL,3,{20,60,600,40},1},{ELEMENT_TEXT,0,0,NULL,3,{40,130,560,140},1},{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{200,380,240,45},1},{ELEMENT_END}};
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"JED Light Baker";e[1].wstr=u"Use the Light Baker window. A successful Bake returns here and applies the result automatically. Plugin messages appear below its controls.";menu.idkFunc=lightBakerWait;jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[2]);int action=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);
        if(getenv("GEOSITH_BAKER_TEST"))fprintf(stderr,"BAKER: menu returned %d\n",action);
        DWORD status=STILL_ACTIVE;GetExitCodeProcess(lightBakerProcess,&status);CloseHandle(lightBakerProcess);lightBakerProcess=NULL;
        if(action<0){std::ofstream(job/"cancel")<<"cancel";resultMessage="Light bake cancelled.";}
        else if(status!=0){std::ifstream error(job/"error.txt");std::getline(error,resultMessage);if(resultMessage.empty())resultMessage="Light Baker failed; no level changes were applied.";}
        else if(!edModelsEqual(original.get(),model))resultMessage="Level changed during the bake; results were not applied. Bake again using the current level.";
        else if(std::filesystem::exists(job/"output.bin")){
            auto n=std::filesystem::file_size(job/"output.bin");if(n>ED_NET_LIMIT/2)throw std::runtime_error("Light Baker result is too large.");auto data=EdResources::read(job/"output.bin",0,(size_t)n);
            if(!edWireDecode(candidate,data.data(),(uint32_t)data.size())||!lightBakerRestoreModels(candidate,original.get())||!edValidate(candidate))throw std::runtime_error("Light Baker result failed validation.");
            // Editor lights remain available for future bakes; baked faces bypass live light accumulation.
            remember();if(commit())resultMessage="Baked lighting applied. Ctrl+Z: undo.";else resultMessage="Baked lighting was not accepted.";
        }else resultMessage="Light Baker closed without changing the level.";
        if(action>=0&&status==0){int id=-1;std::ifstream selectedLight(job/"selected-light.txt");if(selectedLight>>id&&id>=0&&id<model->lightCount){lightBakerMode=0;lightHovered=id;lightBakerProperties();resultMessage="Light selected. Lights mode: move it with Fire 2; middle mouse: Light Properties.";}}
        // Only this newly-created job directory is disposable; settings live in its parent.
        if(action>=0&&status==0){std::error_code cleanupError;std::filesystem::remove_all(job,cleanupError);}
    }catch(const std::exception &e){if(lightBakerProcess){if(!job.empty())std::ofstream(job/"cancel")<<"cancel";CloseHandle(lightBakerProcess);lightBakerProcess=NULL;}resultMessage=e.what();}
    if(getenv("GEOSITH_BAKER_TEST"))fprintf(stderr,"BAKER: restore %s\n",resultMessage.c_str());
    jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=previousModal;jkEditor_NetSafeDialog(previousSafe);stdControl_SetActivation(1);stdControl_Flush();fireDown=grabDown=1;say(resultMessage.c_str());
#endif
}
static void lightBakerProperties(){
    int id=lightHovered;if(id<0||id>=model->lightCount){jkGuiDialog_ErrorDialog(u"Light Properties",u"Point at an editor light before opening this menu.");return;}
    EdLight light=model->lights[id];const char16_t *labels[]={u"Type",u"Intensity (0-100)",u"Range",u"Red (0-1)",u"Green (0-1)",u"Blue (0-1)"};
    char16_t types[5][128]={u"Point",u"Sun",u"Sky",u"Ambient",u"Shadow cleave anchor"};int flags[]={0,2,4,32,8},type=0;for(int i=0;i<5;i++)if(light.type&flags[i])type=i;
    double values[]={light.intensity,light.range,light.color.x,light.color.y,light.color.z};char16_t input[5][32];for(int i=0;i<5;i++){char text[32];snprintf(text,sizeof(text),"%.6g",values[i]);stdString_CharToWchar(input[i],text,32);}
    for(;;){jkGuiElement e[16]={};e[0]={ELEMENT_TEXT,0,2,NULL,3,{20,20,600,40},1};
        for(int i=0;i<6;i++){e[1+i*2]={ELEMENT_TEXT,0,0,NULL,3,{35,90+i*43,270,32},1};e[2+i*2]={i?ELEMENT_TEXTBOX:ELEMENT_TEXTBUTTON,2,0,NULL,31,{310,90+i*43,290,32},1};}
        e[13]={ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,390,230,40},1};e[14]={ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,390,230,40},1};e[15]={ELEMENT_END};
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"Light Baker Light Properties";for(int i=0;i<6;i++){e[1+i*2].wstr=labels[i];e[2+i*2].wstr=i?input[i-1]:types[type];}e[13].wstr=u"Apply";jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[14]);int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);if(choice<0)return;
        if(choice==2){int pick=chooseList(u"Light Type",types,5,type);if(pick>=0)type=pick;continue;}
        bool valid=true;for(int i=0;i<5;i++){char text[32],*end;stdString_WcharToChar(text,input[i],32);values[i]=strtod(text,&end);if(end==text||*end||!isfinite(values[i]))valid=false;}
        valid=valid&&values[0]>=0&&values[0]<=100&&values[1]>=ED_GRID&&values[1]<=10000;for(int i=2;i<5;i++)valid=valid&&values[i]>=0&&values[i]<=1;
        if(!valid){jkGuiDialog_ErrorDialog(u"Light Properties",u"Enter valid intensity, range, and RGB values between 0 and 1.");continue;}
        light.type=flags[type];light.intensity=values[0];light.range=values[1];light.color=edV(values[2],values[3],values[4]);edCopyModel(candidate,model);candidate->lights[id]=light;remember();commit();return;
    }
}
static void removeAllSectorLight(){
    edCopyModel(candidate,model);bool changed=false;
    for(int s=0;s<candidate->count;s++){auto &sector=candidate->c[s];changed|=sector.ambientLight!=0||sector.extraLight!=0;sector.ambientLight=sector.extraLight=0;}
    if(!changed){say("All sector light is already 0.");return;}
    remember();if(commit())say("All sector ambient and extra light set to 0. Ctrl+Z: undo.");
}
static void lightBakerMenu(){
    bool bakerAvailable=lightBakerAvailable();
    int previousSafe=jkEditor_NetSafeDialog(1),previousModal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    char16_t names[5][128]={u"Lights",u"Light Baker",u"Open Light Baker",u"Light Properties",u"Remove All Sector Light"};
    if(!bakerAvailable)stdString_SafeWStrCopy(names[1],names[4],128);
    int choice=chooseList(u"Lighting Tool",names,bakerAvailable?5:2,bakerAvailable?lightBakerMode:0);
    if(!bakerAvailable&&choice==1)choice=4;
    if(choice>=0&&choice<3)lightBakerMode=choice!=0;
    if(choice==3)lightBakerProperties();
    jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=previousModal;jkEditor_NetSafeDialog(previousSafe);stdControl_SetActivation(1);stdControl_Flush();
    if(choice==4)removeAllSectorLight();else if(choice==2)lightBakerRun();else say(lightBakerMode?"Light Baker: Fire 1 opens the plugin. Middle mouse: lighting modes.":"Lights: Fire 1 places lights. Middle mouse: lighting modes.");
}

static int builderBakedLightTest();
static int lightBakerSelfTest(){
#ifdef _WIN32
 bool reload=getenv("GEOSITH_BAKER_RELOAD")!=NULL;
 if(!reload){if(!lightBakerAvailable())return 0;clearHistory();EdSelection floor={};floor.count=1;floor.hits[0]={0,0,-1,-1};if(!edTerrainCreate(candidate,model,&floor,"baker_terrain"))return 0;*model=*candidate;model->lightCount=1;model->lights[0]=EdLight{0,edV(.25,.25,.5),1,10};model->lights[0].color=edV(1,.2,.1);
    int asset=model->assetCount++;model->assets[asset]={};strcpy(model->assets[asset].name,"baker_box");model->assets[asset].saved=1;model->assets[asset].lightBaker=1;model->assets[asset].origin=edV(10,10,10);
    if(!edMeshPrimitive(model,asset,0,edV(10,10,10),0))return 0;
    EdTemplate &t=model->templates[model->templateCount++];t={};strcpy(t.name,"baker_box");strcpy(t.parent,"_walkstruct");t.count=1;strcpy(t.params[0].key,"model3d");strcpy(t.params[0].value,"baker_box.3do");
    if(!syncCustomTemplates(model))return 0;
    for(int n=0;n<2;n++){EdObject &o=model->objects[model->objectCount++];o={};strcpy(o.templateName,"baker_box");o.position=edV(n?.6:-.5,0,.5);o.rotation=edV(0,45,0);}
    auto before=std::make_unique<EdModel>(*model);auto placed=std::make_unique<EdModel>(*model);lightBakerPlaceModels(placed.get());
    EdVec center={};int vertices=0;for(int m=0;m<placed->meshCount;m++)if(placed->meshes[m].asset==asset)for(int f=0;f<placed->meshes[m].geometry.count;f++)for(int v=0;v<placed->meshes[m].geometry.f[f].count;v++){center=edAdd(center,placed->meshes[m].geometry.f[f].v[v].p);vertices++;}
    if(edDot(edSub(edScale(center,1./vertices),edV(-.5,0,.5)),edSub(edScale(center,1./vertices),edV(-.5,0,.5)))>.000001)return 0;
    lightBakerRun();
    for(int m=0;m<model->meshCount;m++)if(model->meshes[m].asset==asset)for(int f=0;f<model->meshes[m].geometry.count;f++)for(int v=0;v<model->meshes[m].geometry.f[f].count;v++)if(edDot(edSub(model->meshes[m].geometry.f[f].v[v].p,before->meshes[m].geometry.f[f].v[v].p),edSub(model->meshes[m].geometry.f[f].v[v].p,before->meshes[m].geometry.f[f].v[v].p))>1e-12)return 0;
}
 int baked=0;double red=0,blue=0;bool live=true;
 for(int s=0;s<model->count;s++)for(int f=0;f<model->c[s].count;f++){auto &face=model->c[s].f[f];baked+=face.bakedLight;for(int v=0;v<face.count;v++){red+=face.v[v].lightRGB.x;blue+=face.v[v].lightRGB.z;auto *surface=&sithWorld_g_pCurrentWorld->aSectors[s].surfaces[f];live=live&&fabs(surface->surfaceInfo.intensities[face.count+v]-face.v[v].lightRGB.x)<.00001;}}
 int terrainBaked=0;double terrainMin=1,terrainMax=0;
 for(int i=0;i<model->meshCount;i++){auto &mesh=model->meshes[i];if(!model->assets[mesh.asset].terrain)continue;
 for(int f=0;f<mesh.geometry.count;f++){auto &face=mesh.geometry.f[f];terrainBaked+=face.bakedLight;for(int v=0;v<face.count;v++){terrainMin=fmin(terrainMin,face.v[v].light);terrainMax=fmax(terrainMax,face.v[v].light);}}}
 live=live&&builderBakedLightTest();
 int customBaked=0;for(int m=0;m<model->meshCount;m++)if(model->assets[model->meshes[m].asset].lightBaker)for(int f=0;f<model->meshes[m].geometry.count;f++)customBaked+=model->meshes[m].geometry.f[f].bakedLight;
 fprintf(stderr,"Custom model bake: %d faces\n",customBaked);
 bool ok=customBaked>0&&baked>0&&terrainBaked>0&&terrainMax>terrainMin+.001&&red>blue&&live&&edValidate(model);
 fprintf(stderr,"Terrain bake: %d faces, intensity %.6f..%.6f, live %d\n",terrainBaked,terrainMin,terrainMax,live);
 if(!reload){auto saved=std::make_unique<EdModel>(*model);ok=ok&&historyStep(0)&&!model->c[0].f[0].bakedLight&&historyStep(1)&&edModelsEqual(model,saved.get());ok=ok&&edWriteGob(model,"Episode/sithed_baker_test.gob","Light Baker integration test",0,resources);}
 fprintf(stderr,"Light Baker test: %d baked faces, RGB %.4f / %.4f, live %d, reload %d\n",baked,red,blue,live,reload);return ok;

#else
 return 0;
#endif
}
