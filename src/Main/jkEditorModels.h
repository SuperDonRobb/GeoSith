/* Weapon 9 owns a mesh selection separate from the level selection. */
#include "edNet.h"
static int builderDetail[4];
static int builderPrimitive,builderRendering,builderTestDiscard,builderTestWalkstruct;
static char builderEditingName[32];
static double builderSize=.5,builderRotation;
static EdModel *builderPreview;
static EdHit builderHover={-1,-1,-1,-1},builderGrab;
static EdSelection builderPicks,builderGrabPicks;
static EdVec builderStart,builderEnd,builderDelta;
static EdDragTrace builderTrace;
static int builderOriginGrabbing;
/* The origin handle is visible through the mesh, so picking it is too. */
static int builderOriginHit(EdVec origin,EdVec direction){
    if(tool!=1||builderAsset<0||builderAsset>=model->assetCount)return 0;
    EdVec offset=edSub(model->assets[builderAsset].origin,origin);double distance=edDot(offset,direction);
    double radius=fmax(.012,fmin(.06,distance*.012));
    EdVec perpendicular=edSub(offset,edScale(direction,distance));
    return distance>.01&&edDot(perpendicular,perpendicular)<=radius*radius;
}
static EdBuffer<rdModel3> builderModels;
static EdBuffer<rdThing> builderThings;
static EdBuffer<unsigned int> builderHashes;
static const char *builderPrimitiveNames[]={"Box","Sphere","Cylinder","Pyramid"};
static void line(EdVec a,EdVec b,int color);
static void vertexCircle(EdVec p,int selected,int color);
static void drawEditorCrosshair(void);
int jkEditor_ModelBlue(void){return active&&builderAsset>=0&&!builderRendering;}
static void builderFreeCache(void){edFree(builderPreview);builderPreview=NULL;for(int a=0;a<(int)builderModels.size();a++){rdThing_FreeEntry(&builderThings[a]);builderModels[a].sizeMaterials=0;rdModel3_FreeEntry(&builderModels[a]);edZero(&builderModels[a],0,sizeof(rdModel3));edZero(&builderThings[a],0,sizeof(rdThing));builderHashes[a]=0;}builderModels.clear();builderThings.clear();builderHashes.clear();}
static int builderCacheFrom(const EdModel *model,int asset){
    unsigned int hash=2166136261u;int count=0;for(int i=0;i<model->meshCount;i++)if(model->meshes[i].asset==asset){hash=(hash^edCellHash(&model->meshes[i].geometry))*16777619u;count++;}
    hash^=edNetHash(&model->assets[asset],sizeof(EdAsset));hash^=edPaintHash(model,asset)^decalAssetHash(model,asset);if(!count)return 0;if(builderHashes[asset]==hash&&builderModels[asset].numGeos)return 1;
    EdPaintBake bake;int painted=edPainted(model,asset),bakeVertex=0;rdMaterial *paintMaterial=painted?terrainBakeLive(model,asset,&bake):NULL;if(painted&&!paintMaterial)return 0;
    rdModel3 *m=&builderModels[asset];rdThing *thing=&builderThings[asset];rdThing_FreeEntry(thing);m->sizeMaterials=0;rdModel3_FreeEntry(m);edZero(m,0,sizeof(*m));edZero(thing,0,sizeof(*thing));builderHashes[asset]=0;
    m->numGeos=1;m->numHNodes=count;m->aGeos[0].numMeshes=count;m->aGeos[0].aMeshes=edCalloc(count,sizeof(rdMesh));m->aHierarchyNodes=edCalloc(count,sizeof(rdHierarchyNode));
    if(!m->aGeos[0].aMeshes||!m->aHierarchyNodes)goto failed;
    {int index=0;for(int i=0;i<model->meshCount;i++)if(model->meshes[i].asset==asset){
        const EdCell *c=&model->meshes[i].geometry;rdMesh *mesh=&m->aGeos[0].aMeshes[index];rdHierarchyNode *node=&m->aHierarchyNodes[index];
        node->idx=node->meshIdx=index;node->parent=index?m->aHierarchyNodes:NULL;node->numChildren=index?0:count-1;node->child=!index&&count>1?m->aHierarchyNodes+1:NULL;node->nextSibling=index&&index+1<count?m->aHierarchyNodes+index+1:NULL;
        mesh->mesh_num=index++;mesh->geometryMode=4;mesh->lightingMode=3;mesh->textureMode=1;mesh->numFaces=c->count;
        for(int f=0;f<c->count;f++)mesh->numVertices+=c->f[f].count;mesh->numUVs=mesh->numVertices;
        mesh->aVertices=edCalloc(mesh->numVertices,sizeof(rdVector3));mesh->aTexVerticies=edCalloc(mesh->numVertices,sizeof(rdVector2));mesh->vertexNormals=edCalloc(mesh->numVertices,sizeof(rdVector3));mesh->vertices_i=edCalloc(mesh->numVertices,sizeof(flex_t));mesh->vertices_unk=edCalloc(mesh->numVertices,sizeof(flex_t));mesh->faces=edCalloc(c->count,sizeof(rdFace));
        if(!mesh->aVertices||!mesh->aTexVerticies||!mesh->vertexNormals||!mesh->vertices_i||!mesh->vertices_unk||!mesh->faces)goto failed;
        int vnum=0;for(int f=0;f<c->count;f++){
            const EdFace *src=&c->f[f];rdFace *face=&mesh->faces[f];face->numVertices=src->count;face->geometryMode=4;face->lightingMode=3;face->textureMode=1;face->wallCel=-1;face->material=painted?paintMaterial:editorFaceMaterial(model,src);face->normal=rdvec(edNormal(src));
            face->vertexPosIdx=edCalloc(src->count,sizeof(int));face->vertexUVIdx=edCalloc(src->count,sizeof(int));if(!face->vertexPosIdx||!face->vertexUVIdx)goto failed;
            // Match exported 3DO prelighting: sector and dynamic lights supply brightness.
            // The isolated builder preview has its own FULLYLIT rdThing mode below.
            for(int v=0;v<src->count;v++,vnum++){EdVec p=edSub(src->v[v].p,model->assets[asset].origin);mesh->aVertices[vnum]=rdvec(p);EdVec uv=painted?bake.uv[bakeVertex++]:edV(src->v[v].u,src->v[v].v,0);mesh->aTexVerticies[vnum]=rdVector2{(flex_t)uv.x,(flex_t)uv.y};mesh->vertexNormals[vnum]=face->normal;mesh->vertices_i[vnum]=0;face->vertexPosIdx[v]=face->vertexUVIdx[v]=vnum;mesh->radius=fmax(mesh->radius,sqrt(edDot(p,p)));}
        }m->radius=fmax(m->radius,mesh->radius);
    }
    rdThing_NewEntry(thing,NULL);if(!rdThing_SetModel3(thing,m))goto failed;thing->desiredLightMode=thing->curLightMode=RD_LIGHTMODE_FULLYLIT;builderHashes[asset]=hash;return 1;
}
failed:
    rdThing_FreeEntry(thing);/* Partial allocations are handled by the normal geometry destructor. */
    if(!m->aGeos[0].aMeshes)m->aGeos[0].numMeshes=0;
    rdModel3_FreeEntry(m);edZero(m,0,sizeof(*m));edZero(thing,0,sizeof(*thing));return 0;
}
static int builderCache(int asset){return builderCacheFrom(model,asset);}
static int builderNameExists(const char *file){if(sithModel_Find(file))return 1;char path[160];snprintf(path,sizeof(path),"3do\\%s",file);stdFile_t f=jkRes_FileOpen(path,"rb");if(!f)return 0;jkRes_FileClose(f);return 1;}
static void builderDrawWorld(void){
    if(!model||builderAsset<0||builderAsset>=model->assetCount||mapActive)return;rdCache_Flush();
    for(int a=builderAsset;a==builderAsset;a++)if(builderCache(a)){
        builderRendering=a==builderAsset;rdMatrix34 orient;rdMatrix_Identity34(&orient);orient.scale=rdvec(model->assets[a].origin);builderThings[a].rdFrameNum=rdroid_frameTrue-1;
        rdThing_Draw(&builderThings[a],&orient);rdCache_Flush();builderRendering=0;
    }
    if(builderAsset>=0&&cleaving){
        if(!bladeReady){if(!rdPolyline_NewEntry(&cleaveBlade,"Editor cleave","sabergreen1.mat","sabergreen0.mat",.01,.004,.004,RD_GEOMETRY_FULL,RD_LIGHTMODE_FULLYLIT,RD_TEXTUREMODE_PERSPECTIVE,1))return;rdThing_NewEntry(&cleaveThing,NULL);rdThing_SetPolyline(&cleaveThing,&cleaveBlade);bladeReady=1;}
        EdVec delta=edSub(builderEnd,builderStart);double size=sqrt(edDot(delta,delta));rdVector3 forward=size>ED_EPS?rdvec(edScale(delta,1/size)):rdVector3{(flex_t)(0),(flex_t)(1),(flex_t)(0)};rdMatrix34 orient;rdMatrix_BuildFromLook34(&orient,&forward);orient.scale=rdvec(builderStart);cleaveBlade.length=size;
        builderRendering=1;rdPolyline_Draw(&cleaveThing,&orient);rdCache_Flush();builderRendering=0;
    }
}
typedef struct {int safe,modal;} BuilderMenuState;
static BuilderMenuState builderMenuBegin(void){BuilderMenuState state={jkEditor_NetSafeDialog(1),jkMain_bInModalGui};jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);return state;}
static void builderMenuEnd(BuilderMenuState state){jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=state.modal;jkEditor_NetSafeDialog(state.safe);stdControl_SetActivation(1);stdControl_Flush();fireDown=grabDown=middleDown=1;}
static void testNestedMaterialDialogs(void){
    if(!bindEditorWorld())exit(2);
    int initialDepth=jkGui_modesets,initialMode=jkGui_GdiMode;
    for(int i=0;i<8;i++){
        jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
        int outerDepth=jkGui_modesets;
        browserObjects=0;materialMenu(1);
        if(jkGui_modesets!=outerDepth||!jkGui_GdiMode){puts("GEOSITH_NESTED_MATERIAL FAIL: browser leaked menu mode");fflush(stdout);exit(2);}
        jkGui_SetModeGame();
        if(jkGui_modesets!=initialDepth||jkGui_GdiMode!=initialMode){puts("GEOSITH_NESTED_MATERIAL FAIL: outer menu did not restore mode");fflush(stdout);exit(2);}
    }
    puts("GEOSITH_NESTED_MATERIAL PASS: eight nested material dialogs restore menu mode");fflush(stdout);exit(0);
}
static void builderMenu(void){BuilderMenuState state=builderMenuBegin();char16_t names[4][128];for(int i=0;i<4;i++)stdString_CharToWchar(names[i],builderPrimitiveNames[i],128);int choice=chooseList(u"3DO Primitives",names,4,builderPrimitive);if(choice>=0)builderPrimitive=choice;builderMenuEnd(state);}
static void builderRemoveAsset(EdModel *m,int asset){
    for(int i=0;i<m->meshCount;)if(m->meshes[i].asset==asset){--m->meshCount;for(int shift=i;shift<m->meshCount;shift++)m->meshes[shift]=std::move(m->meshes[shift+1]);}else{if(m->meshes[i].asset>asset)m->meshes[i].asset--;i++;}
    {--m->assetCount;for(int shift=asset;shift<m->assetCount;shift++)m->assets[shift]=std::move(m->assets[shift+1]);}
}
static void builderFinish(void){
    int asset=builderAsset;builderExitRequested=0;grabbing=cleaving=0;builderPicks.count=0;
    if(asset<0||asset>=model->assetCount){builderAsset=-1;return;}
    BuilderMenuState state=builderMenuBegin();char16_t name[32];stdString_CharToWchar(name,builderEditingName[0]?builderEditingName:model->assets[asset].name,32);
    jkGuiElement elements[]={{ELEMENT_TEXT,0,2,NULL,3,{50,110,540,60},1},{ELEMENT_TEXTBOX,0,0,NULL,27,{120,200,400,30},1},{ELEMENT_TEXTBUTTON,1,2,NULL,3,{340,300,220,40},1},{ELEMENT_TEXTBUTTON,-1,2,NULL,3,{80,300,220,40},1},{ELEMENT_TEXTBUTTON,2,2,NULL,3,{130,360,380,40},1},{ELEMENT_END}};
    jkGuiMenu menu={elements,0,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);elements[0].wstr=u"Save to the level's 3DO library";elements[1].wstr=name;elements[2].wstr=u"Save as 3DO";elements[3].wstr=u"Discard";elements[4].wstr=u"Save as _walkstruct";
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[3]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&elements[2]);
    if(getenv("GEOSITH_STRESS")&&getenv("GEOSITH_BUILDER_CAPTURE"))menu.idkFunc=builderCaptureMenu;
    int saved=0;
    for(;;){asset=builderAsset;int action=jkGuiRend_DisplayAndReturnClicked(&menu);
        if(netEditPending()){jkGuiDialog_ErrorDialog(u"3DO",u"The host is still checking your edit. Please retry shortly.");continue;}
        char text[32];if(action==1||action==2){if(!editorLevelFilename(name,text)||strlen(text)>27){jkGuiDialog_ErrorDialog(u"3DO name",u"Use 1-27 letters, numbers, underscores or hyphens.");continue;}
            int exists=0,meshes=0;for(int a=0;a<model->assetCount;a++)if(a!=asset&&!__strcmpi(text,model->assets[a].name)&&__strcmpi(text,builderEditingName))exists=1;for(int i=0;i<model->meshCount;i++)meshes+=model->meshes[i].asset==asset;
            char file[64];snprintf(file,sizeof(file),"%s.3do",text);if(exists||(builderNameExists(file)&&__strcmpi(text,builderEditingName))){jkGuiDialog_ErrorDialog(u"3DO name",u"That model name is already in use.");continue;}if(!meshes){jkGuiDialog_ErrorDialog(u"3DO",u"Place a primitive before saving, or choose Discard.");continue;}
        }
        remember();edCopyModel(candidate,model);
        if(action==1||action==2){
            if(builderEditingName[0]&&!__strcmpi(text,builderEditingName)){for(int a=0;a<candidate->assetCount;a++)if(a!=asset&&!strcmp(candidate->assets[a].name,builderEditingName)){builderRemoveAsset(candidate,a);if(a<asset)asset--;break;}}
            snprintf(candidate->assets[asset].name,32,"%s",text);candidate->assets[asset].saved=1;if(action==2&&!builderWalkstruct(candidate,asset,text)){char16_t error[160];stdString_CharToWchar(error,message,160);jkGuiDialog_ErrorDialog(u"Walkstruct",error);continue;}}
        else{for(int i=0;i<candidate->meshCount;){if(candidate->meshes[i].asset==asset){{--candidate->meshCount;for(int shift=i;shift<candidate->meshCount;shift++)candidate->meshes[shift]=std::move(candidate->meshes[shift+1]);}}else{if(candidate->meshes[i].asset>asset)candidate->meshes[i].asset--;i++;}}
            {--candidate->assetCount;for(int shift=asset;shift<candidate->assetCount;shift++)candidate->assets[shift]=std::move(candidate->assets[shift+1]);}}
        if(commit()){saved=action>0;break;}jkGuiDialog_ErrorDialog(u"3DO",u"Could not finish the model. Please retry.");
    }
    freeMenuStrings(elements);builderEditingName[0]=0;builderAsset=-1;builderHover.cell=-1;builderMenuEnd(state);say(saved?"3DO saved to the model library. Weapon 8: create a template. Save Level packages the model.":"3DO draft discarded.");
}
static int builderBegin(EdVec origin,EdVec direction){
    if(model->assetCount==ED_ASSETS){say("Model index range exhausted.");return 0;}
    builderEditingName[0]=0;remember();edCopyModel(candidate,model);int a=candidate->assetCount++;EdAsset *asset=&candidate->assets[a];edZero(asset,0,sizeof(*asset));asset->origin=edSnapPoint(edAdd(origin,edScale(direction,.75)));
    for(int n=1;n<10000;n++){snprintf(asset->name,32,"model%d",n);int found=0;for(int b=0;b<a;b++)if(!__strcmpi(asset->name,candidate->assets[b].name))found=1;char file[64];snprintf(file,sizeof(file),"%s.3do",asset->name);if(!found&&!builderNameExists(file))break;}
    if(!commit())return 0;builderAsset=a;
    builderPicks.count=0;builderHover.cell=-1;grabbing=cleaving=0;fireDown=grabDown=1;edResetDoubleTap(&selectionTap,1);say("3DO mode: 9 primitives, 0 cleave, 1 select, 2 materials. Escape: save 3DO, save walkstruct, or discard.");return 1;
}

static EdVec builderPlacementPoint(EdVec origin,EdVec direction){
    EdHit wall;edRay(model,origin,direction,&wall);double margin=builderSize*.5+.05,distance=fmin(fmax(.75,margin),wall.cell>=0?fmax(.15,wall.distance-margin):fmax(.75,margin));return edSnapPoint(edAdd(origin,edScale(direction,distance)));
}
static void builderEditAsset(int asset){
    int meshes=0;for(int i=0;i<model->meshCount;i++)meshes+=model->meshes[i].asset==asset;
    if(model->assetCount>=ED_ASSETS||meshes>ED_MESHES-model->meshCount){say("Not enough free model/mesh capacity for an editable copy.");return;}
    char name[32];snprintf(name,32,"%s",model->assets[asset].name);EdVec original=model->assets[asset].origin,origin,direction;ray(&origin,&direction);
    if(!builderBegin(origin,direction))return;edCopyModel(candidate,model);EdVec offset=edSub(candidate->assets[builderAsset].origin,original);int total=candidate->meshCount;
    for(int i=0;i<total;i++)if(candidate->meshes[i].asset==asset){EdMesh *copy=&candidate->meshes[candidate->meshCount++];*copy=candidate->meshes[i];copy->asset=builderAsset;
        for(int f=0;f<copy->geometry.count;f++)for(int v=0;v<copy->geometry.f[f].count;v++)copy->geometry.f[f].v[v].p=edAdd(copy->geometry.f[f].v[v].p,offset);}
    if(commit())snprintf(builderEditingName,32,"%s",name);
}
static void builderLibraryMenu(void){
    BuilderMenuState state=builderMenuBegin();std::unique_ptr<char16_t[][128]> names(new char16_t[model->assetCount][128]{});std::vector<int> ids(model->assetCount);int count=0;
    for(int a=0;a<model->assetCount;a++)if(model->assets[a].saved&&!model->assets[a].terrain){ids[count]=a;stdString_CharToWchar(names[count++],model->assets[a].name,128);}
    int choice=chooseList(u"Custom 3DOs",names.get(),count,0),action=-1,asset=choice>=0?ids[choice]:-1;
    if(asset>=0){char16_t options[2][128]={u"Edit 3DO",u"Delete"};action=chooseList(names[choice],options,2,0);}
    builderMenuEnd(state);if(asset<0||action<0||netEditPending())return;
    if(action==1){char file[64];snprintf(file,64,"%s.3do",model->assets[asset].name);int used=0;
        for(int t=0;t<model->templateCount;t++)for(int p=0;p<model->templates[t].count;p++)if(!strcmp(model->templates[t].params[p].key,"model3d")&&!__strcmpi(model->templates[t].params[p].value,file))used=1;
        for(int o=0;o<model->objectCount;o++){SithThing *definition=sithTemplate_GetTemplate(model->objects[o].templateName);if(definition&&definition->renderData.type==RD_THING_MODEL3&&!__strcmpi(definition->renderData.model3->filename,file))used=1;}
        if(used){say("3DO is used by a template or placed thing. Remove those references before deleting.");return;}
        remember();edCopyModel(candidate,model);builderRemoveAsset(candidate,asset);if(commit())say("Custom 3DO deleted. Ctrl+Z: undo.");return;
    }
    builderEditAsset(asset);
}
static int builderSameHit(EdHit a,EdHit b){
    if(a.cell!=b.cell)return 0;const EdCell *c=&model->meshes[a.cell].geometry;const EdFace *fa=&c->f[a.face],*fb=&c->f[b.face];
    if(a.vertex>=0&&b.vertex>=0){EdVec d=edSub(fa->v[a.vertex].p,fb->v[b.vertex].p);return edDot(d,d)<ED_EPS*ED_EPS;}
    return a.face==b.face&&a.vertex==b.vertex&&a.edge==b.edge;
}
// Moving a face can delete adjacent collapsed faces and shift their indices.
// Find the grabbed polygon at its translated position, never reuse a stale index.
static int builderExtrudeTargets(EdSelection *targets){
    *targets=builderGrabPicks;EdVec delta=edSnapPoint(builderDelta);
    for(int i=0;i<targets->count;i++){
        EdHit h=builderGrabPicks.hits[i];if(h.vertex>=0||h.edge>=0)return 0;
        const EdFace& source=snapshot->meshes[h.cell].geometry.f[h.face];
        const EdCell& cell=model->meshes[h.cell].geometry;int found=-1;
        for(int f=0;f<cell.count;f++){
            const EdFace& face=cell.f[f];if(face.count!=source.count)continue;int matches=1;
            for(int v=0;v<source.count&&matches;v++){
                EdVec expected=edAdd(source.v[v].p,delta);int corner=0;
                for(int w=0;w<face.count;w++){EdVec d=edSub(face.v[w].p,expected);if(edDot(d,d)<ED_EPS*ED_EPS){corner=1;break;}}
                matches=corner;
            }
            if(matches){if(found>=0)return 0;found=f;}
        }
        if(found<0)return 0;targets->hits[i].face=found;
    }
    return targets->count>0;
}
static void builderControl(int fire,int grab,int middle,int shift,int ctrl){
    if(builderExitRequested){builderFinish();return;}
    if(builderAsset>=model->assetCount){builderAsset=-1;grabbing=cleaving=0;return;}
    EdVec origin,direction;ray(&origin,&direction);edMeshRay(model,builderAsset,origin,direction,tool==1,&builderHover);
    if(textureStitchControl(builderHover,1))return;
    if(!grabbing)builderOriginGrabbing=0;
    int originHovered=builderOriginHit(origin,direction);if(originHovered)builderHover.cell=-1;
    int wheel=stdControl_aAxisStates[AXIS_MOUSE_Z];
    int alt=stdControl_ReadKey(DIK_LMENU,NULL)||stdControl_ReadKey(DIK_RMENU,NULL);
    if(tool==9){if(wheel&&alt){if(builderPrimitive==1||builderPrimitive==2)builderDetail[builderPrimitive]=(builderDetail[builderPrimitive]+(wheel>0?1:2))%3;}else if(wheel&&shift)builderSize=fmax(ED_GRID,fmin(32,builderSize+(wheel>0?ED_GRID:-ED_GRID)));else if(wheel&&ctrl)builderRotation=fmod(builderRotation+(wheel>0?5:-5)+360,360);else if(wheel)builderPrimitive=(builderPrimitive+(wheel>0?1:3))%4;if(middle){builderMenu();return;}}
    if(tool==2){materialWheel(wheel,shift,ctrl);if(middle){browserObjects=0;materialMenu();fireDown=grabDown=1;return;}}
    int history=ctrl&&stdControl_ReadKey(DIK_Z,NULL)?(shift?2:1):ctrl&&stdControl_ReadKey(DIK_Y,NULL)?2:0;
    if(history&&!historyDown&&!grabbing){cleaving=0;historyStep(history==2);builderPicks.count=0;builderHover.cell=-1;historyDown=history;fireDown=fire;grabDown=grab;return;}historyDown=history;
    if(tool==2&&materialDecalMode==2){if(fire&&!fireDown)say("Texture Paint paints sector faces. Use Decal mode for model faces.");fireDown=fire;grabDown=grab;return;}
    if(tool==2&&materialDecalMode==1){if(fire&&!fireDown&&!grab&&builderHover.cell>=0)placeMaterialDecal(builderHover,1);fireDown=fire;grabDown=grab;return;}
    if(grab&&!grabDown&&originHovered){
        remember();edCopyModel(snapshot,model);builderOriginGrabbing=grabbing=1;uvMode=0;builderDelta=edV(0,0,0);
        edDragTraceBegin(&builderTrace,origin,direction,model->assets[builderAsset].origin,direction,1000);
        say("Moving 3DO center point. Mesh geometry stays in place.");
    }else if(grab&&!grabDown&&builderHover.cell>=0&&(tool==1||tool==2||tool==10)){
        remember();edCopyModel(snapshot,model);builderGrab=builderHover;builderGrabPicks=builderPicks;if(tool!=1||!builderGrabPicks.count){builderGrabPicks.count=1;builderGrabPicks.hits[0]=builderGrab;}
        builderDelta=edV(0,0,0);uvRotating=tool==2&&shift;uvFine=tool==2&&ctrl;edDragTraceBegin(&builderTrace,origin,direction,builderGrab.point,tool==2?edNormal(&snapshot->meshes[builderGrab.cell].geometry.f[builderGrab.face]):direction,1000);grabbing=1;uvMode=tool==2;
    }
    if(grabbing&&grab){
        if(fire&&!fireDown&&!uvMode&&!builderOriginGrabbing){edCopyModel(candidate,model);EdSelection targets;int ok=builderExtrudeTargets(&targets);for(int i=0;i<targets.count&&ok;i++)ok=edMeshExtrude(candidate,targets.hits[i]);if(ok&&commit()){grabbing=0;builderPicks.count=0;say("Mesh faces extruded one grid unit along their outward normals.");}else say("Cannot extrude this grab. Release and grab an intact face again.");}
        else {builderDelta=edAdd(builderDelta,edDragTraceStep(&builderTrace,origin,direction));int ok;
            if(builderOriginGrabbing){edCopyModel(candidate,snapshot);candidate->assets[builderAsset].origin=edSnapPoint(edAdd(snapshot->assets[builderAsset].origin,builderDelta));ok=1;}
            else if(uvMode){edCopyModel(candidate,snapshot);EdFace *face=&candidate->meshes[builderGrab.cell].geometry.f[builderGrab.face];if(uvRotating){int edge=builderHover.cell==builderGrab.cell&&builderHover.face==builderGrab.face?builderHover.edge:-1;edRotateTextureDrag(face,builderGrab.point,builderDelta,edge);}else edPanFaceTextureGrid(face,builderDelta,uvFine?ED_GRID*.5:ED_GRID);ok=1;}
            else ok=edMeshMoveConstrained(candidate,snapshot,&builderGrabPicks,builderDelta,shift);
            if(ok)commit();
        }
    }
    if(!grab&&grabDown){grabbing=builderOriginGrabbing=0;builderPicks.count=0;}
    int doubleClick=edDoubleTap(&selectionTap,tool==1&&fire&&!grab,stdPlatform_GetTimeMsec(),300);
    if(fire&&!fireDown&&!grab&&!grabbing){
        if(tool==9){EdVec center=builderPlacementPoint(origin,direction);
            remember();edCopyModel(candidate,model);if(edMeshPrimitiveTransformedDetail(candidate,builderAsset,builderPrimitive,center,currentMaterial,builderSize,builderRotation,builderDetail[builderPrimitive])&&commit())say("Primitive placed as a new mesh.");else say("Cannot add primitive: mesh/vertex limit reached.");}
        else if(tool==1){if(doubleClick)builderPicks.count=0;else if(builderHover.cell>=0){int found=-1;for(int i=0;i<builderPicks.count;i++)if(builderSameHit(builderPicks.hits[i],builderHover)){found=i;break;}if(found>=0){memmove(builderPicks.hits+found,builderPicks.hits+found+1,(--builderPicks.count-found)*sizeof(EdHit));}else if(builderPicks.count<ED_SELECTIONS)builderPicks.hits[builderPicks.count++]=builderHover;}}
        else if(tool==2&&builderHover.cell>=0){remember();edCopyModel(candidate,model);applyFaceMaterial(&candidate->meshes[builderHover.cell].geometry.f[builderHover.face],currentMaterial);edCompactDecals(candidate);commit();}
        else if(tool==10&&builderHover.cell>=0){builderGrab=builderHover;builderStart=builderEnd=edSnapMeshFacePoint(&model->meshes[builderGrab.cell].geometry.f[builderGrab.face],builderHover.point);cleaving=1;}
    }
    if(cleaving){const EdFace *face=&model->meshes[builderGrab.cell].geometry.f[builderGrab.face];EdVec hit;if(edTracePlane(origin,direction,builderStart,edNormal(face),&hit))builderEnd=edSnapMeshFacePoint(face,hit);
        if(!fire&&fireDown){remember();edCopyModel(candidate,model);if(edMeshCleave(candidate,builderGrab,builderStart,builderEnd)&&commit())say("Mesh face cleaved.");else say("Cleave cancelled: draw across the face.");cleaving=0;builderPicks.count=0;}}
    fireDown=fire;grabDown=grab;
}
static int builderOriginSelfTest(void){
    SithThing *p=sithWorld_g_pCurrentWorld->pLocalPlayer;if(!p)return 0;
    active=1;tool=9;EdVec eye,aim;ray(&eye,&aim);if(!builderBegin(eye,aim))return 0;
    EdVec center=model->assets[builderAsset].origin;
    edCopyModel(candidate,model);if(!edMeshPrimitive(candidate,builderAsset,0,center,currentMaterial)||!commit())return 0;
    EdCell geometry=model->meshes[0].geometry;clearHistory();tool=1;
    ray(&eye,&aim);p->position=rdvec(edAdd(vec(p->position),edSub(center,edAdd(eye,edScale(aim,.75)))));
    ray(&eye,&aim);if(!builderOriginHit(eye,aim)||builderOriginHit(center,edScale(aim,-1)))return 0;
    fireDown=grabDown=grabbing=0;builderControl(0,1,0,0,0);if(!builderOriginGrabbing)return 0;
    p->position.x+=ED_GRID*.6;builderControl(0,1,0,0,0);
    p->position.x+=ED_GRID*.4;builderControl(1,1,0,0,0);builderControl(0,0,0,0,0);
    EdVec moved=model->assets[builderAsset].origin;
    if(fabs(moved.x-center.x-ED_GRID)>ED_EPS||fabs(moved.y-center.y)>ED_EPS||fabs(moved.z-center.z)>ED_EPS||undoCount!=1||!edCellsEqual(&geometry,&model->meshes[0].geometry))return 0;
    if(!historyStep(0)||fabs(model->assets[builderAsset].origin.x-center.x)>ED_EPS||!historyStep(1)||fabs(model->assets[builderAsset].origin.x-moved.x)>ED_EPS)return 0;
    uint32_t size=0;unsigned char *wire=edWireEncode(model,&size);EdModel restored;
    int ok=wire&&edWireDecode(&restored,wire,size)&&fabs(restored.assets[builderAsset].origin.x-moved.x)<ED_EPS&&edCellsEqual(&geometry,&restored.meshes[0].geometry);edFree(wire);return ok;
}
static int builderGrabSelfTest(void){
    SithThing *p=sithWorld_g_pCurrentWorld->pLocalPlayer;if(!p)return 0;active=1;tool=9;
    p->position=rdvec(edV(0,-.85,.45));rdVector3 angles={0};rdMatrix_BuildRotate34(&p->orient,&angles);p->actorParams.headPYR=angles;
    EdVec eye,aim;ray(&eye,&aim);if(!builderBegin(eye,aim))return 0;
    edCopyModel(candidate,model);if(!edMeshPrimitiveTransformedDetail(candidate,builderAsset,2,edV(0,0,.5),0,.37,15,2)||!commit())return 0;
    clearHistory();tool=10;fireDown=grabDown=grabbing=cleaving=0;EdModel before=*model;
    builderControl(0,1,0,0,0);if(!grabbing||!edModelsEqual(model,&before))return 0;
    for(int i=0;i<5;i++)builderControl(0,1,0,0,0);
    if(!edModelsEqual(model,&before)||undoCount)return 0;
    EdHit hit=builderGrab;EdFace original=before.meshes[hit.cell].geometry.f[hit.face];
    // Simulate a face index changing during a drag: extrusion must resolve
    // the grabbed polygon, rather than blindly extrude whatever now occupies it.
    int moved=(hit.face+1)%model->meshes[hit.cell].geometry.count;
    std::swap(model->meshes[hit.cell].geometry.f[hit.face],model->meshes[hit.cell].geometry.f[moved]);
    builderControl(1,1,0,0,0);if(grabbing||undoCount!=1)return 0;
    const EdFace& face=model->meshes[hit.cell].geometry.f[moved];EdVec delta=edScale(edNormal(&original),ED_GRID);
    for(int v=0;v<original.count;v++){EdVec error=edSub(face.v[v].p,edAdd(original.v[v].p,delta));if(edDot(error,error)>ED_EPS*ED_EPS)return 0;}
    return historyStep(0)&&edModelsEqual(model,&before);
}
static int builderCleaveSelfTest(void){
    SithThing *p=sithWorld_g_pCurrentWorld->pLocalPlayer;if(!p)return 0;active=1;tool=9;
    p->position=rdvec(edV(0,-.85,.45));rdVector3 angles={0};rdMatrix_BuildRotate34(&p->orient,&angles);p->actorParams.headPYR=angles;
    EdVec eye,aim;ray(&eye,&aim);if(!builderBegin(eye,aim))return 0;
    edCopyModel(candidate,model);if(!edMeshPrimitive(candidate,builderAsset,0,edV(0,0,.5),0)||!commit())return 0;
    clearHistory();tool=10;fireDown=grabDown=grabbing=cleaving=0;EdCell before=model->meshes[0].geometry;
    builderControl(1,0,0,0,0);if(!cleaving)return 0;p->position.x+=.15;builderControl(1,0,0,0,0);builderControl(0,0,0,0,0);
    if(model->meshes[0].geometry.count!=7||undoCount!=1||cleaving)return 0;
    stdControl_bControlsActive=1;stdControl_bDisableKeyboard=0;stdControl_aKeyInfo[DIK_Z]=1;builderControl(0,0,0,0,1);stdControl_aKeyInfo[DIK_Z]=0;builderControl(0,0,0,0,0);
    if(!edCellsEqual(&before,&model->meshes[0].geometry)||redoCount!=1)return 0;
    stdControl_aKeyInfo[DIK_Y]=1;builderControl(0,0,0,0,1);stdControl_aKeyInfo[DIK_Y]=0;builderControl(0,0,0,0,0);
    if(model->meshes[0].geometry.count!=7||undoCount!=1)return 0;
    tool=9;builderPrimitive=1;builderDetail[1]=0;stdControl_aKeyInfo[DIK_LMENU]=1;stdControl_aAxisStates[AXIS_MOUSE_Z]=1;
    for(int i=1;i<=3;i++){builderControl(0,0,0,0,0);if(builderPrimitive!=1||builderDetail[1]!=i%3)return 0;}
    stdControl_aAxisStates[AXIS_MOUSE_Z]=-1;builderControl(0,0,0,0,0);if(builderDetail[1]!=2)return 0;
    builderPrimitive=2;builderDetail[2]=0;builderControl(0,0,0,0,0);if(builderDetail[2]!=2)return 0;
    stdControl_aKeyInfo[DIK_LMENU]=0;stdControl_aAxisStates[AXIS_MOUSE_Z]=0;return 1;
}
static void builderDrawHit(EdHit h,int color){
    if(h.cell<0||h.cell>=model->meshCount||h.face<0||h.face>=model->meshes[h.cell].geometry.count)return;
    const EdFace *f=&model->meshes[h.cell].geometry.f[h.face];if(h.vertex>=0){if(h.vertex<f->count)vertexCircle(f->v[h.vertex].p,1,color);return;}
    for(int v=0;v<f->count;v++)if(h.edge<0||v==h.edge){line(f->v[v].p,f->v[(v+1)%f->count].p,color);vertexCircle(f->v[v].p,0,color);vertexCircle(f->v[(v+1)%f->count].p,0,color);}
}
static void builderDrawOverlay(int edge,int selected){
    EdVec eye,aim;ray(&eye,&aim);int originHovered=builderOriginHit(eye,aim);
    vertexCircle(model->assets[builderAsset].origin,1,originHovered||(grabbing&&builderOriginGrabbing)?selected:edge);
    if(tool==9){if(!builderPreview)builderPreview=edCalloc(1,sizeof(*builderPreview));if(builderPreview){builderPreview->assetCount=1;builderPreview->meshCount=0;strcpy(builderPreview->assets[0].name,"preview");EdVec origin,direction;ray(&origin,&direction);
        if(edMeshPrimitiveTransformedDetail(builderPreview,0,builderPrimitive,builderPlacementPoint(origin,direction),currentMaterial,builderSize,builderRotation,builderDetail[builderPrimitive])){const EdCell *cell=&builderPreview->meshes[0].geometry;for(int f=0;f<cell->count;f++)for(int v=0;v<cell->f[f].count;v++)line(cell->f[f].v[v].p,cell->f[f].v[(v+1)%cell->f[f].count].p,edge);}
    }}
    if(!originHovered)builderDrawHit(builderHover,edge);for(int i=0;i<builderPicks.count;i++)builderDrawHit(builderPicks.hits[i],selected);
    if(cleaving){line(builderStart,builderEnd,edge);vertexCircle(builderStart,1,edge);}drawEditorCrosshair();
    char text[200],detail[40]="";if(builderPrimitive==1||builderPrimitive==2)snprintf(detail,sizeof(detail)," | detail %d/3",builderDetail[builderPrimitive]+1);snprintf(text,sizeof(text),"3DO | %s | %s | size %.4g | rotation %.0f%s | Esc: finish",model->assets[builderAsset].name,builderPrimitiveNames[builderPrimitive],builderSize,builderRotation,detail);
    editorDrawText(highlightCanvas->pVBuffer,jkGui_stdFonts[0],12,12,highlightCanvas->pVBuffer->format.width-24,text,1);
    editorDrawText(highlightCanvas->pVBuffer,jkGui_stdFonts[0],12,34,highlightCanvas->pVBuffer->format.width-24,"LMB use tool | RMB grab (Select: center point too) | RMB+LMB extrude | Wheel: primitive | Shift+wheel: size | Ctrl+wheel: rotate | Alt+wheel: detail | MMB browser",1);
    editorDrawText(highlightCanvas->pVBuffer,jkGui_stdFonts[0],12,56,highlightCanvas->pVBuffer->format.width-24,message,1);
}
static void builderCaptureMenu(jkGuiMenu *menu){
#ifdef SDL2_RENDER
    char path[600];snprintf(path,sizeof(path),"%s.%s.png",getenv("GEOSITH_BUILDER_CAPTURE"),menu->paElements[1].type==ELEMENT_TEXTBOX?"name":"menu");std3D_ScreenshotWindow(path);
#endif
    menu->lastClicked=menu->paElements[1].type==ELEMENT_TEXTBOX?(builderTestDiscard?-1:builderTestWalkstruct?2:1):100;
}
static int stressBuilder(SithThing *p){
    static int frame;
    if(!frame){stressRunning=1;stressRenderFrames=0;active=1;tool=9;savedPhysics=p->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=p->flags;
        p->position=rdvec(edV(0,-.85,.45));rdVector3 angles={0};rdMatrix_BuildRotate34(&p->orient,&angles);p->actorParams.headPYR=angles;setEditorFlight(p,1);sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);
        if(getenv("GEOSITH_BUILDER_RELOAD")){if(model->assetCount!=1||model->meshCount!=4||strcmp(model->assets[0].name,"model1")||!edValidate(model))exit(2);if(getenv("GEOSITH_TEMPLATE_RELOAD")){SithThing *custom=sithTemplate_GetTemplate("custom1");if(model->templateCount!=1||!custom||custom->renderData.type!=RD_THING_MODEL3||custom->renderData.model3->aGeos[0].numMeshes!=4||model->objectCount!=1)exit(2);puts("GEOSITH_TEMPLATE_RELOAD PASS: GOB library, template and placed custom model loaded");}builderAsset=0;printf("GEOSITH_3DO_RELOAD PASS: named model and four meshes loaded from saved GOB\n");fflush(stdout);goto ready;}
        {EdVec origin,direction;ray(&origin,&direction);if(!builderBegin(origin,direction))exit(2);clearHistory();fireDown=0;builderControl(1,0,0,0,0);builderControl(0,0,0,0,0);
        if(model->meshCount!=1||!historyStep(0)||model->meshCount||!historyStep(1)||model->meshCount!=1)exit(2);
        /* Drive painting, selecting and grabbing through the same input path. */
        int material=currentMaterial;currentMaterial=1;tool=2;fireDown=0;builderControl(1,0,0,0,0);builderControl(0,0,0,0,0);
        if(builderHover.cell<0||model->meshes[0].geometry.f[builderHover.face].material!=1)exit(2);
        p->position.x+=.1;tool=1;edResetDoubleTap(&selectionTap,0);builderControl(1,0,0,0,0);builderControl(0,0,0,0,0);if(builderPicks.count!=1)exit(2);
        int undo=undoCount;builderControl(0,1,0,0,0);p->position.x+=ED_GRID;builderControl(0,1,0,0,0);builderControl(0,0,0,0,0);
        if(undoCount!=undo+1||!edValidate(model)||!historyStep(0)||!historyStep(1))exit(2);p->position.x-=ED_GRID+.1;currentMaterial=material;tool=9;
        edCopyModel(candidate,model);for(int i=0;i<4;i++){EdVec center=edV(i%2?.42:-.42,.2,i/2?.82:.25);if(!i)candidate->meshCount=0;if(!edMeshPrimitive(candidate,builderAsset,i,center,currentMaterial))exit(2);}
        if(!commit())exit(2);
        EdHit h={0,2,-1,-1};edCopyModel(candidate,model);if(!edMeshExtrude(candidate,h)||!commit())exit(2);
        if(!edWrite3do(model,0,"geosith-builder-test.3do",resources))exit(2);
        rdModel3 loaded={0};char path[]="geosith-builder-test.3do";if(!rdModel3_LoadEntry(path,&loaded)||loaded.aGeos[0].numMeshes!=4){printf("GEOSITH_BUILDER FAIL: standard 3DO loader\n");fflush(stdout);exit(2);}loaded.sizeMaterials=0;rdModel3_FreeEntry(&loaded);remove(path);
        if(!writeEditorGob(model,"Episode/sithed_builder_test.gob","Builder test",0))exit(2);
}
ready:;
    }
    if(frame==3)builderMenu();
#ifdef SDL2_RENDER
    if(frame==12)std3D_ScreenshotWindow(getenv("GEOSITH_BUILDER_CAPTURE"));
#endif
    if(frame==14&&!getenv("GEOSITH_TEMPLATE_RELOAD")){if(!jkEditor_HandleEscape()||!builderExitRequested)exit(2);builderFinish();if(builderAsset!=-1||!model->assets[0].saved||!writeEditorGob(model,"Episode/sithed_builder_test.gob","Builder test",0))exit(2);}
    if(frame==15&&getenv("GEOSITH_TEMPLATE_TEST"))stressTemplates();
    if(frame++>=18){printf("GEOSITH_STRESS PASS: 3DO primitives, outward extrusion, mesh undo/redo, standard loader, naming, GOB save and blue isolation, %d rendered frames\n",stressRenderFrames);fflush(stdout);exit(0);}return 1;
}
