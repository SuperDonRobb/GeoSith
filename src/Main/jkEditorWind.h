static std::unique_ptr<EdModel> windBefore,windDraft;
static int windSector,windDragging,windEnterDown;
static EdVec windCenter,windDirection,windDragPlane,windDragNormal;
static double windStrength=1,windRadius=.35;
static int windActive(){return windDraft!=nullptr;}
static int windCancel(){int was=windActive();windBefore.reset();windDraft.reset();windDragging=0;if(was)say("Wind tunnel cancelled.");return was;}
static int windStart(int sector,const EdCell &cell){
    if(cell.splineId){jkGuiDialog_ErrorDialog(u"Wind Tunnel",u"Use the spline's Wind Tunnel option for editable spline sectors.");return 0;}
    windBefore=std::make_unique<EdModel>(*model);windDraft=std::make_unique<EdModel>(*model);windDraft->c[sector]=cell;windSector=sector;
    windCenter=edMapCenter(&cell);windStrength=sqrt(edDot(cell.thrust,cell.thrust));windDirection=windStrength>.001?edScale(cell.thrust,1/windStrength):edV(1,0,0);if(windStrength<.001)windStrength=1;
    windRadius=.5;for(int f=0;f<cell.count;f++)windRadius=fmin(windRadius,fabs(edDot(edNormal(&cell.f[f]),edSub(windCenter,cell.f[f].v[0].p)))*.7);windRadius=fmax(ED_GRID,windRadius);
    if(mapActive)mapClose(sithWorld_g_pCurrentWorld->pLocalPlayer);grabbing=groupGrab=cleaving=windDragging=0;fireDown=grabDown=windEnterDown=1;
    say("Wind: RMB-drag the tip to aim in 3D. Wheel: strength. Enter / Fire 1: apply. Escape: cancel.");return 1;
}
static int windApply(){
    if(netEditPending()){say("Wait for host approval before applying wind.");return 0;}
    if(!edModelsEqual(model,windBefore.get())){windCancel();say("Level changed. Reopen the wind generator.");return 0;}
    edCopyModel(candidate,windDraft.get());if(!edSetWind(candidate,windSector,edScale(windDirection,windStrength))||!edValidate(candidate)){say("Could not generate wind. No changes applied.");return 0;}
    remember();if(!commit())return 0;windBefore.reset();windDraft.reset();windDragging=0;say("Wind tunnel generated. Ctrl+Z: undo. Strength is also editable in Placed COGs.");return 1;
}
static int windControl(){
    if(!windActive())return 0;
    int fire=editButton(INPUT_FUNC_FIRE1),grab=editButton(INPUT_FUNC_FIRE2),enter=stdControl_ReadKey(DIK_RETURN,NULL)!=0;
    int accept=(fire&&!fireDown)||(enter&&!windEnterDown),start=grab&&!grabDown;fireDown=fire;grabDown=grab;windEnterDown=enter;
    int wheel=stdControl_aAxisStates[AXIS_MOUSE_Z];if(wheel)windStrength=fmax(.25,fmin(100,windStrength+(wheel>0?.25:-.25)));
    if(accept&&!grab){windApply();return 1;}if(!grab){windDragging=0;return 1;}
    EdVec origin,direction;ray(&origin,&direction);EdVec tip=edAdd(windCenter,edScale(windDirection,windRadius));
    if(start){EdVec d=edSub(tip,origin);double depth=edDot(d,direction);EdVec side=edSub(d,edScale(direction,depth));
        if(depth>0&&edDot(side,side)<pow(fmax(ED_GRID*.5,depth*.012),2)){windDragging=1;windDragPlane=tip;windDragNormal=direction;}}
    if(windDragging){EdVec point;if(edTracePlane(origin,direction,windDragPlane,windDragNormal,&point)){EdVec d=edSub(point,windCenter);double len=sqrt(edDot(d,d));if(len>ED_GRID*.1)windDirection=edScale(d,1/len);}}
    return 1;
}
static void windOverlay(int color){
    if(!windActive())return;
    EdVec tip=edAdd(windCenter,edScale(windDirection,windRadius));line(windCenter,tip,color);vertexCircle(windCenter,0,color);vertexCircle(tip,windDragging,color);
    EdVec side=edCross(windDirection,edV(0,0,1));double len=sqrt(edDot(side,side));if(len<.01)side=edV(1,0,0);else side=edScale(side,1/len);
    for(int sign:{-1,1})line(tip,edAdd(windCenter,edAdd(edScale(windDirection,windRadius*.7),edScale(side,sign*windRadius*.15))),color);
    char text[160];snprintf(text,sizeof(text),"Wind thrust %.2f | RMB-drag tip | Wheel: strength | Enter / Fire 1: apply | Escape: cancel",windStrength);
    editorDrawText(highlightCanvas->pVBuffer,jkGui_stdFonts[0],8,70,highlightCanvas->pVBuffer->format.width-16,text,1);
}
static void windMenuTest(jkGuiMenu *menu){
    std::u16string title=menu->paElements[0].wstr;static int splineStep=0;
    if(title.find(u"Sector ")==0)menu->lastClicked=205;
    else if(title==u"Generate")menu->lastClicked=106;
    else if(title==u"Spline Shape and Geometry")menu->lastClicked=splineStep++?1:24;
    else menu->lastClicked=102; // Reverse, strength 2.
}
static int windTest(SithThing *player){
    clearHistory();sectorPicks.clear();sectorPicks[0]=1;sectorAttributesDialog();
    if(!windActive()||!windCancel()||undoCount)return 0;
    sectorAttributesDialog();if(!windActive())return 0;windDirection=edV(.6,0,.8);windStrength=2;
    if(!windApply()||model->cogCount!=1||!historyStep(0)||model->cogCount||!historyStep(1))return 0;
    EdSpline spline={};spline.id=1;spline.count=3;spline.sides=4;spline.segments=8;spline.width=spline.height=1;
    spline.points[0]={edV(4,0,.5),edV(0,1,0)};spline.points[1]={edV(4,3,.5),edV(1,1,0)};spline.points[2]={edV(7,4,.5),edV(1,0,0)};
    if(!edSplineUpdate(candidate,model,&spline)||!commit())return 0;splineSelected=1;splinePoint=-1;splineAttributes();
    if(model->splines[0].windSpeed!=-2||model->cogCount!=9)return 0;
    if(!writeEditorGob(model,"Episode/sithed_wind_test.gob","Wind test",0))return 0;
    leaveEditor(player);auto *world=sithWorld_g_pCurrentWorld;
    if(fabs(world->aSectors[0].thrust.x-1.2)>.0001||fabs(world->aSectors[0].thrust.z-1.6)>.0001)return 0;
    for(int c=1;c<model->count;c++)if(!(world->aSectors[c].flags&8)||world->aSectors[c].thrust.y>0)return 0;
    puts("GEOSITH_WIND PASS: sector and spline menus, cancel, undo/redo, reverse curved wind, saved COGs and live startup thrust");return 1;
}
