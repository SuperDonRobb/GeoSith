#include "edWaterSim.h"
extern "C" {
#include "Engine/sithPhysics.h"
}
static std::unique_ptr<EdWaterSim> waterSimulation;
static uint64_t waterRevision;
static std::vector<int> waterPreviewTops;
static uint32_t waterLastTick;
static double waterSimSpeed=1;
static int waterMaterial=-1;
static EdPlacedCog waterConveyor;
static int waterSimActive(){return waterSimulation!=nullptr;}
static void waterSimReset(){waterSimulation.reset();}
static int waterSimStart(int sector){
    if(netEditPending()||terrainSessionOpen()){say("Finish the pending edit before starting a water simulation.");return 0;}
    int material=addMaterialName("oasis_water.mat");
    if(material<0||!editorMaterial(material)||!sithSound_Load("00underwater.wav",0)){say("Water simulation requires oasis_water.mat and 00underwater.wav.");return 0;}
    char *script=cogSourceText("00_conveyor.cog");int parsed=script&&edCogParseScript(&waterConveyor,"00_conveyor.cog",script);edFree(script);
    if(!parsed){say("Could not load 00_conveyor.cog from the game resources.");return 0;}
    auto simulation=std::make_unique<EdWaterSim>();std::string error;
    if(!simulation->start(*model,sector,error)){say(error.c_str());return 0;}
    if(mapActive)mapClose(sithWorld_g_pCurrentWorld->pLocalPlayer);
    waterSimSpeed=1;waterMaterial=material;waterRevision=geometryDocumentRevision;waterLastTick=stdPlatform_GetTimeMsec();waterSimulation=std::move(simulation);waterPreviewTops=waterSimulation->previewTops();
    grabbing=groupGrab=cleaving=objectGrabbing=lightGrabbing=0;
    say("Flowing Water SIM: wheel adjusts speed. Escape bakes the water; movement remains available.");return 1;
}
static int waterSimFinish(){
    if(!waterSimulation)return 0;
    auto simulation=std::move(waterSimulation);
    if(waterRevision!=geometryDocumentRevision||netEditPending()){say("Water simulation discarded because the level changed. Start it again on the updated level.");return 1;}
    std::string error;edCopyModel(candidate,model);
    if(!edBakeWaterSim(candidate,*simulation,waterMaterial,waterConveyor,error,sithPhysics_GetThingHeight(sithWorld_g_pCurrentWorld->pLocalPlayer))){say(error.c_str());return 1;}
    remember();if(commit()){sectorPicks.clear();picks.count=0;hover.cell=-1;say("Flowing water baked into sectors with water surfaces and conveyor COGs. Ctrl+Z: undo.");}
    fireDown=grabDown=1;return 1;
}
static int waterSimTick(){
    if(!waterSimulation)return 0;
    if(waterRevision!=geometryDocumentRevision||netEditPending()){waterSimulation.reset();say("Water simulation stopped: level changed. No simulation geometry was applied.");return 1;}
    // Spread the accelerated simulation over frames; never repay a slow
    // frame with sixteen more expensive steps on the following frame.
    uint32_t now=stdPlatform_GetTimeMsec();
    int wheel=stdControl_aAxisStates[AXIS_MOUSE_Z];
    if(wheel){waterSimSpeed=std::clamp(waterSimSpeed*(wheel>0?1.4142135623730951:0.7071067811865475),.25,10.0);waterLastTick=now;}
    // Change playback rate, not the solver timestep or source/flow balance.
    uint32_t interval=(uint32_t)std::lround(13.0/waterSimSpeed);
    uint32_t maxSteps=(uint32_t)std::ceil(4*waterSimSpeed),backlog=interval*maxSteps;
    int steps=(std::min)(maxSteps,(now-waterLastTick)/interval);
    if(now-waterLastTick>backlog)waterLastTick=now-backlog;
    int completed=0;
    for(int i=0;i<steps;i++){
        waterSimulation->step(.05);completed++;waterLastTick+=interval;
        if(waterSimulation->finished){waterSimFinish();break;}
        if(stdPlatform_GetTimeMsec()-now>=4){waterLastTick=stdPlatform_GetTimeMsec();break;}
    }
    if(completed&&waterSimulation)waterPreviewTops=waterSimulation->previewTops();
    fireDown=editButton(INPUT_FUNC_FIRE1);grabDown=editButton(INPUT_FUNC_FIRE2);return 1;
}
static void waterSimOverlay(){
    if(!waterSimulation||!highlightCanvas)return;
    int blue=palettePixel(highlightCanvas->pVBuffer,40,170,255),white=palettePixel(highlightCanvas->pVBuffer,160,230,255);
    double volume=0,capacity=0;int wet=0;
    for(auto &pool:waterSimulation->pools){volume+=pool.volume;capacity+=pool.capacity;wet+=pool.volume>pool.capacity*1e-6;}
    for(int index:waterPreviewTops){const auto &pool=waterSimulation->pools[index];
        double h=waterSimulation->spacing*.5;
        std::array<EdVec,4> points={edV(pool.center.x-h,pool.center.y-h,pool.level),edV(pool.center.x+h,pool.center.y-h,pool.level),edV(pool.center.x+h,pool.center.y+h,pool.level),edV(pool.center.x-h,pool.center.y+h,pool.level)};
        // Project each corner once, and draw single-pixel lines without
        // selection outlines. Subpixel tiles do not need a wire square.
        rdVector3 screen[4];bool inFront=true;
        for(int k=0;k<4;k++)if(!project(points[k],&screen[k])){inFront=false;break;}
        if(!inFront)continue;
        double minX=screen[0].x,maxX=minX,minY=screen[0].y,maxY=minY;
        for(int k=1;k<4;k++){minX=fmin(minX,screen[k].x);maxX=fmax(maxX,screen[k].x);minY=fmin(minY,screen[k].y);maxY=fmax(maxY,screen[k].y);}
        if(maxX<0||maxY<0||minX>=highlightCanvas->pVBuffer->format.width||minY>=highlightCanvas->pVBuffer->format.height||fmax(maxX-minX,maxY-minY)<2)continue;
        for(int k=0;k<4;k++){auto &a=screen[k];auto &b=screen[(k+1)%4];
            screenLine((int)std::clamp((double)a.x,-32768.0,32767.0),(int)std::clamp((double)a.y,-32768.0,32767.0),(int)std::clamp((double)b.x,-32768.0,32767.0),(int)std::clamp((double)b.y,-32768.0,32767.0),blue,1);
        }
        EdVec center=edV(pool.center.x,pool.center.y,pool.level),flow=pool.flow;flow.z=0;double length=sqrt(edDot(flow,flow));
        if(index%32==0&&length>1e-5){rdVector3 a,b;
            if(project(center,&a)&&project(edAdd(center,edScale(flow,fmin(.5,length)/length)),&b)&&fabs(a.x)<32768&&fabs(a.y)<32768&&fabs(b.x)<32768&&fabs(b.y)<32768)screenLine((int)a.x,(int)a.y,(int)b.x,(int)b.y,white,1);
        }

    }
    char label[180];snprintf(label,sizeof(label),"Flowing Water SIM | %.1f%% | Speed %.2gx | Wheel: speed | Escape: bake",100*volume/fmax(capacity,1e-12),waterSimSpeed);
    editorDrawText(highlightCanvas->pVBuffer,jkGui_stdFonts[0],8,70,highlightCanvas->pVBuffer->format.width-16,label,1);
}
static int waterSimSelfTest(SithThing *player){
    if(!bindEditorWorld())return 0;active=1;tool=1;
    if(const char *source=getenv("GEOSITH_WATER_SIM_MAP_TEST")){
        clearHistory();if(!waterSimStart(atoi(source)))return 0;
        for(int i=0;i<100;i++)waterSimulation->step(.05);
        size_t regions=waterSimulation->pools.size();
        for(int s=0;s<model->count;s++){double volume=0,capacity=0;for(auto &pool:waterSimulation->pools)if(pool.sector==s){volume+=pool.volume;capacity+=pool.capacity;}fprintf(stderr,"WATER VOXELS sector %d: %.6f / %.6f\n",s,volume,capacity);}
        if(!jkEditor_HandleEscape()||undoCount!=1||!edValidate(model))return 0;
        fprintf(stderr,"WATER SIM MAP PASS: %zu flow regions, %d baked sectors, %d cogs\n",regions,model->count,model->cogCount);return 1;
    }
    edDefault(candidate);if(!commit())return 0;clearHistory();
    if(!waterSimStart(0))return 0;
    for(int i=0;i<4;i++)waterSimulation->step(.05);
    if(!jkEditor_HandleEscape()||model->count!=2||model->cogCount!=2||undoCount!=1)return 0;
    int wet=0;for(int s=0;s<model->count;s++)wet+=(model->c[s].flags&2)!=0;if(wet!=1)return 0;
    active=0;if(!startPlacedCogs(model))return 0;active=1;suspendPlacedCogs(0);
    if(!historyStep(0)||model->count!=1||model->cogCount||!historyStep(1)||model->count!=2)return 0;
    if(!writeEditorGob(model,"Episode/sithed_water_sim_test.gob","Water simulation test",0))return 0;
    puts("WATER SIM PASS: partial fill, Escape bake, water attributes, stock conveyor startup, undo/redo and GOB save");return 1;
}
static int waterSimTestFrame(SithThing *player){
    static int frame;
    if(!frame){int ok=waterSimSelfTest(player);fflush(stdout);if(!ok||!getenv("GEOSITH_WATER_SIM_CAPTURE"))exit(ok?0:2);
        stressRunning=1;if(!waterSimStart(0))exit(2);player->position=rdvec(edV(0,-.8,.8));int sector=objectSector(model,vec(player->position));if(sector>=0)sithThing_SetSector(player,&sithWorld_g_pCurrentWorld->aSectors[sector],1);setEditorFlight(player,1);rdVector3 angle={-15,0,0};rdMatrix_BuildRotate34(&player->orient,&angle);sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);}
    if(waterSimulation)waterSimulation->step(.05);
#ifdef SDL2_RENDER
    if(frame==30){std3D_ScreenshotWindow(getenv("GEOSITH_WATER_SIM_CAPTURE"));exit(0);}
#endif
    frame++;return 1;
}
