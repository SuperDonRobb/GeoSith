/* Thing attributes and path-frame handles share the object tool's drag rules.
 * Frame IDs are editor-only; they never enter the world's thing/COG indices. */
static int frameId(int parent,int frame){return ED_OBJECTS+(parent<0?ED_OBJECTS-parent-1:parent)*ED_FRAMES+frame;}
static int frameParent(int id){int slot=(id-ED_OBJECTS)/ED_FRAMES;return slot>=ED_OBJECTS?-(slot-ED_OBJECTS)-1:slot;}
static int frameIndex(int id){return (id-ED_OBJECTS)%ED_FRAMES;}
static EdThingAttributes *thingAttributes(EdModel *source,int id){if(id>=ED_OBJECTS)id=frameParent(id);return id<0?&source->spawns[-id-1].attributes:&source->objects[id].attributes;}
static const char *thingTemplate(const EdModel *source,int id){if(id>=ED_OBJECTS)id=frameParent(id);return id<0?"walkplayer":source->objects[id].templateName;}
static EdFrame thingPose(const EdModel *source,int id){if(id>=ED_OBJECTS){int parent=frameParent(id);const EdThingAttributes *a=parent<0?&source->spawns[-parent-1].attributes:&source->objects[parent].attributes;return a->frames[frameIndex(id)];}return id<0?EdFrame{source->spawns[-id-1].position,source->spawns[-id-1].rotation}:EdFrame{source->objects[id].position,source->objects[id].rotation};}
static void setThingPose(EdModel *source,int id,EdFrame pose){if(id>=ED_OBJECTS){thingAttributes(source,id)->frames[frameIndex(id)]=pose;return;}if(id<0){source->spawns[-id-1].position=pose.position;source->spawns[-id-1].rotation=pose.rotation;if(id==-1)source->spawn=pose.position;}else{source->objects[id].position=pose.position;source->objects[id].rotation=pose.rotation;}}
static void readThingAttributes(EdThingAttributes *a,const SithThing *thing){edZero(a,0,sizeof(*a));a->flags=thing->flags;SithThing *base=thing->pTemplate;a->hasFlags=!base||base->flags!=thing->flags;
    if(thing->moveType==SITH_MT_PHYSICS){a->hasVelocity=1;a->velocity=vec(thing->physicsParams.vel);}
    if(thing->pClass)snprintf(a->aiClass,sizeof(a->aiClass),"%s",thing->pClass->fpath);
    if(thing->controlType==SITH_CT_AI&&thing->actor&&thing->actor->loadedFrames){a->aiFrames=1;a->frameCount=thing->actor->loadedFrames;if(a->frameCount>ED_FRAMES)a->frameCount=ED_FRAMES;for(int i=0;i<a->frameCount;i++)a->frames[i].position=vec(thing->actor->aFrames[i]);}
    if(thing->moveType==SITH_MT_PATH){a->pathOverride=1;a->frameCount=thing->trackParams.loadedFrames;if(a->frameCount>ED_FRAMES)a->frameCount=ED_FRAMES;
        for(int i=0;i<a->frameCount;i++)a->frames[i]=EdFrame{vec(thing->trackParams.aFrames[i].pos),vec(thing->trackParams.aFrames[i].rot)};}
}
static void syncThingAttributes(SithThing *thing,const EdThingAttributes *a){
    if(a->hasFlags)thing->flags=a->flags;
    if(a->hasVelocity&&thing->moveType==SITH_MT_PHYSICS)thing->physicsParams.vel=rdvec(a->velocity);
    if(a->aiClass[0]){SithAIClass *definition=sithAIClass_Load((char*)a->aiClass);if(definition){thing->pClass=definition;if(thing->actor)thing->actor->pClass=definition;}}
    if(a->aiFrames){if(thing->actor){if(thing->actor->aFrames)SITH_FREE(thing->actor->aFrames);thing->actor->aFrames=a->frameCount?(rdVector3*)SITH_ALLOC(sizeof(rdVector3)*a->frameCount):NULL;thing->actor->sizeFrames=thing->actor->loadedFrames=thing->actor->aFrames?a->frameCount:0;for(int i=0;i<thing->actor->loadedFrames;i++)thing->actor->aFrames[i]=rdvec(a->frames[i].position);}return;}

    if(!a->pathOverride&&!a->frameCount)return;
    if(thing->moveType==SITH_MT_PATH&&thing->trackParams.aFrames)SITH_FREE(thing->trackParams.aFrames);
    edZero(&thing->trackParams,0,sizeof(thing->trackParams));thing->moveType=a->frameCount?SITH_MT_PATH:SITH_MT_NONE;
    thing->curframe=0;thing->goalframe=0;
    if(a->frameCount){thing->trackParams.aFrames=(SithPathFrame*)SITH_ALLOC(sizeof(SithPathFrame)*a->frameCount);if(!thing->trackParams.aFrames){thing->moveType=SITH_MT_NONE;return;}
        thing->trackParams.loadedFrames=thing->trackParams.sizeFrames=a->frameCount;
        for(int i=0;i<a->frameCount;i++){thing->trackParams.aFrames[i].pos=rdvec(a->frames[i].position);thing->trackParams.aFrames[i].rot=rdvec(a->frames[i].rotation);}}
}
static int pickEditorFrame(EdVec origin,EdVec direction,double wallDistance,int handles){int found=-10000;double nearest=wallDistance;
    for(int i=0;i<model->objectCount+model->spawnCount;i++){int parent=i<model->objectCount?i:-(i-model->objectCount)-1;EdThingAttributes *a=thingAttributes(model,parent);
        SithThing *definition=sithTemplate_GetTemplate(thingTemplate(model,parent));double bodyRadius=.08;
        if(definition){bodyRadius=fmax(.04,definition->collideSize);if(definition->renderData.type==RD_THING_MODEL3&&definition->renderData.model3)bodyRadius=fmax(bodyRadius,definition->renderData.model3->radius);}
        for(int j=0;j<a->frameCount;j++){int id=frameId(parent,j);if(editorViewReady&&!thingOverlayVisible(id))continue;EdVec point=handles?objectFacingHandle(model,id):a->frames[j].position,offset=edSub(point,origin);double depth=edDot(offset,direction),radius=handles?fmax(.012,fmin(.06,depth*.012)):bodyRadius;
            if(depth<.01||depth>nearest+ED_EPS)continue;EdVec perpendicular=edSub(offset,edScale(direction,depth));if(edDot(perpendicular,perpendicular)>radius*radius)continue;nearest=depth;found=id;}
    }return found;
}
static void addThingFrame(EdThingAttributes *a,EdFrame pose){if(a->frameCount>=ED_FRAMES)return;if(a->frameCount){pose=a->frames[a->frameCount-1];pose.position.z+=ED_GRID;}a->frames[a->frameCount++]=pose;a->pathOverride=1;}
static int frameMenuCaptureStage;
static void captureThingAttributes(jkGuiMenu *menu){static int ticks;if(++ticks%3)return;
#ifdef SDL2_RENDER
    if(frameMenuCaptureStage==3)std3D_ScreenshotWindow(getenv("GEOSITH_FRAMES_CAPTURE"));
#endif
    menu->lastClicked=frameMenuCaptureStage++<3?2:1;
}
#include "jkEditorResize.h"
static void thingAttributesDialog(int id){
    EdThingAttributes draft=*thingAttributes(model,id);EdFrame pose=thingPose(model,id);int selected=0,previousModal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;rdVector_Zero3(&player->physicsParams.vel);rdVector_Zero3(&player->physicsParams.acceleration);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    for(;;){jkGuiElement e[40]={0};char16_t title[128],count[64],inputs[13][64],frameLabel[64];char text[160];
        snprintf(text,sizeof(text),"Thing %d | %s",id<0?-id-1:model->spawnCount+id,thingTemplate(model,id));stdString_CharToWchar(title,text,128);
        snprintf(text,sizeof(text),"Numframes: %d / %d",draft.frameCount,ED_FRAMES);stdString_CharToWchar(count,text,64);
        snprintf(text,sizeof(text),"Frame %d",draft.frameCount?selected:0);stdString_CharToWchar(frameLabel,text,64);
        e[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,10,600,38},1};e[1]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,58,285,25},1};e[2]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{330,58,205,25},1};
        e[3]=jkGuiElement{ELEMENT_TEXTBUTTON,2,2,NULL,3,{540,54,36,32},1};e[4]=jkGuiElement{ELEMENT_TEXTBUTTON,3,2,NULL,3,{582,54,36,32},draft.frameCount>0};
        const char16_t *labels[]={u"X",u"Y",u"Z",u"Pitch",u"Yaw",u"Roll"};EdFrame frame=draft.frameCount?draft.frames[selected]:EdFrame{0};
        double values[12]={pose.position.x,pose.position.y,pose.position.z,pose.rotation.x,pose.rotation.y,pose.rotation.z,frame.position.x,frame.position.y,frame.position.z,frame.rotation.x,frame.rotation.y,frame.rotation.z};
        for(int j=0;j<12;j++){int column=j/6,row=j%6,x=column?330:20,y=98+row*31;snprintf(text,sizeof(text),"%.9g",values[j]);stdString_CharToWchar(inputs[j],text,64);
            e[5+j*2]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{x,y,74,26},column?draft.frameCount>0:1};e[6+j*2]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,63,{x+78,y,198,27},column?draft.frameCount>0:1};}
        snprintf(text,sizeof(text),"0x%08X",draft.hasFlags?draft.flags:0);if(!draft.hasFlags){SithThing *t=sithTemplate_GetTemplate(thingTemplate(model,id));if(t)snprintf(text,sizeof(text),"0x%08X",t->flags);}stdString_CharToWchar(inputs[12],text,64);
        e[29]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,300,90,28},1};e[30]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,63,{114,300,182,28},1};
        e[31]=jkGuiElement{ELEMENT_TEXTBUTTON,4,0,NULL,3,{330,300,72,30},draft.frameCount>1};e[32]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{407,301,125,28},1};e[33]=jkGuiElement{ELEMENT_TEXTBUTTON,5,0,NULL,3,{536,300,72,30},draft.frameCount>1};
        e[34]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,378,600,32},1};e[35]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,423,230,40},1};e[36]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,423,230,40},1};e[37]=jkGuiElement{ELEMENT_TEXTBUTTON,6,0,NULL,3,{20,337,200,30},id>=0};e[38].type=ELEMENT_END;
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=title;e[1].wstr=u"Thing position / rotation";e[2].wstr=count;e[3].wstr=u"+";e[4].wstr=u"-";
        for(int j=0;j<12;j++){e[5+j*2].wstr=labels[j%6];e[6+j*2].wstr=inputs[j];}e[29].wstr=u"Thing flags";e[30].wstr=inputs[12];e[31].wstr=u"<";e[32].wstr=frameLabel;e[33].wstr=u">";e[34].wstr=u"+ adds a frame; - removes the displayed frame.\nIn weapon 3, grab a red frame or its rotation circle.";e[35].wstr=u"Apply";e[37].wstr=u"Generate";
        if(getenv("GEOSITH_FRAMES_CAPTURE")&&getenv("GEOSITH_STRESS"))menu.idkFunc=captureThingAttributes;
        jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&e[35]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[36]);int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);if(choice<0)break;
        int valid=1;for(int j=0;j<12;j++){if(j>=6&&!draft.frameCount)continue;char *end;stdString_WcharToChar(text,inputs[j],sizeof(text));values[j]=strtod(text,&end);if(end==text||*end||!isfinite(values[j]))valid=0;}
        stdString_WcharToChar(text,inputs[12],sizeof(text));char *end;unsigned long long flags=strtoull(text,&end,0);if(end==text||*end||flags>0xffffffffu)valid=0;
        if(!valid){jkGuiDialog_ErrorDialog(u"Thing Attributes",u"Use finite numbers for coordinates and angles, and a 32-bit number (or 0x hex) for flags.");continue;}
        pose=EdFrame{edV(values[0],values[1],values[2]),edV(values[3],values[4],values[5])};draft.flags=(unsigned int)flags;draft.hasFlags=1;
        if(draft.frameCount)draft.frames[selected]=EdFrame{edV(values[6],values[7],values[8]),edV(values[9],values[10],values[11])};
        if(choice==2){addThingFrame(&draft,pose);selected=draft.frameCount-1;}
        else if(choice==3&&draft.frameCount){memmove(&draft.frames[selected],&draft.frames[selected+1],(draft.frameCount-selected-1)*sizeof(EdFrame));draft.frames[--draft.frameCount]=EdFrame{0};draft.pathOverride=1;if(selected>=draft.frameCount)selected=draft.frameCount?draft.frameCount-1:0;}
        else if(choice==4)selected=(selected+draft.frameCount-1)%draft.frameCount;else if(choice==5)selected=(selected+1)%draft.frameCount;
        else if(choice==6){if(thingGenerateDialog(id,draft,pose))break;}
        else if(choice==1){if(objectSector(model,pose.position)<0){jkGuiDialog_ErrorDialog(u"Thing Attributes",u"The thing position must be inside a sector.");continue;}edCopyModel(candidate,model);*thingAttributes(candidate,id)=draft;setThingPose(candidate,id,pose);remember();if(commit()){say("Thing attributes updated. Red previews are path frames; Ctrl+Z undoes.");break;}}
    }
    jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=previousModal;stdControl_SetActivation(1);stdControl_Flush();fireDown=grabDown=middleDown=1;
}
static void drawFrameGhosts(void){
    rdCache_Flush();
    int savedZ=rdGetZBufferMethod();rdSetZBufferMethod(RD_ZBUFFER_READ_NOWRITE);
    /* A translucent red wash retains the textured shape even for nearly black
       model textures, which cannot be made red by multiplying their RGB. */
    rdMaterial red={0};rdTexinfo redInfo={0};red.num_texinfo=1;red.texinfos[0]=&redInfo;
#ifdef RDMATERIAL_LRU_LOAD_UNLOAD
    red.bDataLoaded=red.bMetadataLoaded=1;
#endif
    rdColor24 *colors=rdColormap_pCurMap?rdColormap_pCurMap->colors:(rdColor24*)stdDisplay_masterPalette;
    redInfo.header.solidColor=stdColor_FindClosest(colors,256,255,24,24);
    typedef struct {unsigned int flags;rdGeoMode_t mode;rdMaterial *material;} SavedGhostFace;
    for(int i=0;i<model->objectCount+model->spawnCount;i++){
        int parent=i<model->objectCount?i:-(i-model->objectCount)-1;EdThingAttributes *a=thingAttributes(model,parent);if(!a->frameCount)continue;
        SithThing *definition=sithTemplate_GetTemplate(thingTemplate(model,parent));if(!definition||definition->renderData.type!=RD_THING_MODEL3||!definition->renderData.model3)continue;
        rdModel3 *mesh=definition->renderData.model3;rdThing ghost={0};rdThing_NewEntry(&ghost,NULL);if(!rdThing_SetModel3(&ghost,mesh)){rdThing_FreeEntry(&ghost);continue;}
        ghost.desiredLightMode=ghost.curLightMode=RD_LIGHTMODE_FULLYLIT;rdModel3_EnsureMaterialData(&ghost);
        int count=0;for(int g=0;g<mesh->numGeos;g++)for(unsigned int m=0;m<mesh->aGeos[g].numMeshes;m++)count+=mesh->aGeos[g].aMeshes[m].numFaces;
        SavedGhostFace *saved=edAlloc(sizeof(*saved)*count);if(!saved){rdThing_FreeEntry(&ghost);continue;}int k=0;
        for(int g=0;g<mesh->numGeos;g++)for(unsigned int m=0;m<mesh->aGeos[g].numMeshes;m++)for(int f=0;f<mesh->aGeos[g].aMeshes[m].numFaces;f++){
            rdFace *face=&mesh->aGeos[g].aMeshes[m].faces[f];saved[k++]=SavedGhostFace{face->type,face->geometryMode,face->material};
        }
        for(int pass=0;pass<2;pass++){
            k=0;for(int g=0;g<mesh->numGeos;g++)for(unsigned int m=0;m<mesh->aGeos[g].numMeshes;m++)for(int f=0;f<mesh->aGeos[g].aMeshes[m].numFaces;f++){
                rdFace *face=&mesh->aGeos[g].aMeshes[m].faces[f];face->type=saved[k].flags|2;
                if(pass){face->geometryMode=RD_GEOMETRY_SOLID;face->material=&red;}k++;
            }
            for(int j=0;j<a->frameCount;j++){if(!thingOverlayVisible(frameId(parent,j)))continue;rdMatrix34 orient;rdVector3 rotation=rdvec(a->frames[j].rotation);rdMatrix_BuildRotate34(&orient,&rotation);orient.scale=rdvec(a->frames[j].position);ghost.rdFrameNum=rdroid_frameTrue-1;rdThing_Draw(&ghost,&orient);rdCache_Flush();}
        }
        k=0;for(int g=0;g<mesh->numGeos;g++)for(unsigned int m=0;m<mesh->aGeos[g].numMeshes;m++)for(int f=0;f<mesh->aGeos[g].aMeshes[m].numFaces;f++){
            rdFace *face=&mesh->aGeos[g].aMeshes[m].faces[f];face->type=saved[k].flags;face->geometryMode=saved[k].mode;face->material=saved[k].material;k++;
        }edFree(saved);rdThing_FreeEntry(&ghost);
    }rdSetZBufferMethod(savedZ);
}
static int framesSelfTest(void){EdModel *saved=edAlloc(sizeof(*saved));if(!saved)return 0;edCopyModel(saved,model);int oldActive=active;active=1;clearHistory();
    edCopyModel(candidate,model);candidate->objectCount=1;EdObject *o=&candidate->objects[0];edZero(o,0,sizeof(*o));snprintf(o->templateName,64,"crate4_1");o->position=edV(.2,.4,.2);o->rotation=edV(0,25,0);
    addThingFrame(&o->attributes,EdFrame{o->position,o->rotation});addThingFrame(&o->attributes,EdFrame{0});addThingFrame(&o->attributes,EdFrame{0});int ok=commit();
    ok=ok&&model->objects[0].attributes.frameCount==3&&fabs(model->objects[0].attributes.frames[1].position.z-(.2+ED_GRID))<.00001&&model->objects[0].attributes.frames[0].rotation.y==25;
    SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;int id=frameId(0,2);EdVec center=thingPose(model,id).position,origin=edAdd(center,edV(0,-.3,0)),direction=edV(0,1,0);hover.cell=-1;fireDown=grabDown=grabbing=0;
    ok=ok&&pickEditorFrame(origin,direction,1000,0)==id;
    objectControl(player,origin,direction,0,1,0,0);ok=ok&&objectGrabbed==id&&!objectRotating;
    objectControl(player,edAdd(origin,edV(ED_GRID,0,0)),direction,0,1,0,0);objectControl(player,origin,direction,0,0,0,0);
    ok=ok&&fabs(thingPose(model,id).position.x-edSnapPoint(edAdd(center,edV(ED_GRID,0,0))).x)<.00001&&fabs(model->objects[0].position.x-.2)<.00001;
    ok=ok&&historyStep(0)&&fabs(thingPose(model,id).position.x-center.x)<.00001&&historyStep(1);
    EdVec handle=objectFacingHandle(model,id);origin=edAdd(handle,edV(0,-.3,0));fireDown=grabDown=0;objectControl(player,origin,direction,0,1,0,0);ok=ok&&objectRotating&&objectGrabbed==id;
    objectControl(player,edAdd(origin,edV(ED_GRID,0,0)),direction,0,1,0,0);objectControl(player,origin,direction,0,0,0,0);EdVec rotation=thingPose(model,id).rotation;ok=ok&&fabs(remainder(rotation.y,5))<.0001&&rotation.x==0&&rotation.z==0;
    active=0;syncEditorObjects(model);SithThing *thing=objectThings[0]>=0?&sithWorld_g_pCurrentWorld->aThings[objectThings[0]]:NULL;
    ok=ok&&thing&&thing->moveType==SITH_MT_PATH&&thing->trackParams.loadedFrames==3&&fabs(thing->trackParams.aFrames[2].rot.y-rotation.y)<.001;
    edCopyModel(candidate,model);while(candidate->objects[0].attributes.frameCount<ED_FRAMES)addThingFrame(&candidate->objects[0].attributes,EdFrame{0});
    /* Use deterministic coordinates for a full-length line reload test. */
    for(int i=3;i<ED_FRAMES;i++)candidate->objects[0].attributes.frames[i].position=edV(.2,.4,.2+i*ED_GRID);
    ok=ok&&writeEditorGob(candidate,"Episode/sithed_frames_test.gob","Frames test",0);
    active=1;edCopyModel(candidate,saved);int restored=commit();active=oldActive;clearHistory();grabbing=objectGrabbing=objectRotating=fireDown=grabDown=0;edFree(saved);printf("GEOSITH_FRAMES %s: defaults, frame picking, drag/rotation, undo, native path and GOB export\n",ok&&restored?"PASS":"FAIL");fflush(stdout);return ok&&restored;
}
static int stressFrames(SithThing *p){static int step;
    if(!step){stressRunning=1;stressRenderFrames=0;active=1;tool=3;savedPhysics=p->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=p->flags;setEditorFlight(p,1);
        edCopyModel(candidate,model);candidate->objectCount=1;candidate->objects[0]=EdObject{.position={.15,.6,.18}};snprintf(candidate->objects[0].templateName,64,"crate4_1");if(!commit())exit(2);clearHistory();frameMenuCaptureStage=0;thingAttributesDialog(0);if(model->objects[0].attributes.frameCount!=3||undoCount!=1)exit(2);
        if(!historyStep(0)||model->objects[0].attributes.frameCount||!historyStep(1))exit(2);
        edCopyModel(candidate,model);candidate->objects[0].attributes.frames[1].position.z+=.15;candidate->objects[0].attributes.frames[2].position.z+=.3;if(!commit())exit(2);
        p->position=rdvec(edV(.15,.05,.25));rdVector3 angles={0};rdMatrix_BuildRotate34(&p->orient,&angles);p->actorParams.headPYR=angles;sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);
    }
#ifdef SDL2_RENDER
    if(step==20){char path[512];snprintf(path,sizeof(path),"%s.world.png",getenv("GEOSITH_FRAMES_CAPTURE"));std3D_ScreenshotWindow(path);}
#endif
    if(step++>=28){printf("GEOSITH_STRESS PASS: thing attributes, frame count controls, ghost previews and undo, %d rendered frames\n",stressRenderFrames);fflush(stdout);exit(0);}return 1;
}
