static void conveyorMenuTest(jkGuiMenu *menu){menu->lastClicked=menu->paElements[0].wstr[0]==u'G'?303:300;}
static std::unique_ptr<EdModel> conveyorBefore,conveyorDraft;
static EdSelection conveyorSelection;
static EdPlacedCog conveyorScript;
static std::vector<EdVec> conveyorDirections;
static int conveyorDragging=-1,conveyorEnterDown;
static int conveyorActive(){return conveyorDraft!=nullptr;}
static int conveyorCancel(){int was=conveyorActive();conveyorBefore.reset();conveyorDraft.reset();conveyorDirections.clear();conveyorDragging=-1;if(was)say("Conveyor generation cancelled.");return was;}
static EdVec conveyorCenter(int index){
    EdHit h=conveyorSelection.hits[index];const auto &face=conveyorDraft->c[h.cell].f[h.face];EdVec center={};
    for(int v=0;v<face.count;v++)center=edAdd(center,face.v[v].p);
    return edAdd(edScale(center,1.0/face.count),edScale(edNormal(&face),.003));
}
static double conveyorRadius(int index){
    EdHit h=conveyorSelection.hits[index];const auto &face=conveyorDraft->c[h.cell].f[h.face];EdVec center=conveyorCenter(index),n=edNormal(&face);double radius=.4;
    for(int v=0;v<face.count;v++){EdVec edge=edSub(face.v[(v+1)%face.count].p,face.v[v].p);double len=sqrt(edDot(edge,edge));if(len>ED_EPS)radius=fmin(radius,fabs(edDot(edSub(center,face.v[v].p),edCross(edge,n)))/len*.7);}
    return fmax(ED_GRID*.25,radius);
}
static int conveyorStart(const EdSelection &selected,const EdFace *draft){
    char *text=cogSourceText("00_conveyor.cog");int ok=text&&edCogParseScript(&conveyorScript,"00_conveyor.cog",text);edFree(text);
    if(!ok){jkGuiDialog_ErrorDialog(u"Conveyor Belt",u"Could not load 00_conveyor.cog from the game resources.");return 0;}
    auto work=std::make_unique<EdModel>(*model);std::vector<EdVec> directions;
    for(int i=0;i<selected.count;i++){
        EdHit h=selected.hits[i];work->c[h.cell].f[h.face]=draft[i];EdVec n=edNormal(&draft[i]),d=edSub(edV(1,0,0),edScale(n,n.x));
        if(edDot(d,d)<.01)d=edSub(edV(0,1,0),edScale(n,n.y));d=edScale(d,1/sqrt(edDot(d,d)));directions.push_back(d);
    }
    auto probe=std::make_unique<EdModel>(*work);
    if(!edGenerateConveyors(probe.get(),selected,directions.data(),conveyorScript,2)){jkGuiDialog_ErrorDialog(u"Conveyor Belt",u"Cannot configure these surfaces. Convert editable splines first. Remove existing shared conveyor COGs before regenerating those surfaces.");return 0;}
    conveyorBefore=std::make_unique<EdModel>(*model);conveyorDraft=std::move(work);conveyorSelection=selected;conveyorDirections=std::move(directions);conveyorDragging=-1;
    if(mapActive)mapClose(sithWorld_g_pCurrentWorld->pLocalPlayer);
    grabbing=groupGrab=cleaving=0;fireDown=grabDown=1;conveyorEnterDown=1;
    say("Conveyors: RMB-drag the first surface's arrow to set all belts. Enter / Fire 1 applies; Escape cancels.");return 1;
}
static int conveyorApply(){
    if(netEditPending()){say("Wait for host approval before applying conveyors.");return 0;}
    if(!edModelsEqual(model,conveyorBefore.get())){conveyorCancel();say("Level changed. Reopen the conveyor generator.");return 0;}
    edCopyModel(candidate,conveyorDraft.get());
    std::fill(conveyorDirections.begin(),conveyorDirections.end(),conveyorDirections[0]);
    if(!edGenerateConveyors(candidate,conveyorSelection,conveyorDirections.data(),conveyorScript,2)){say("Could not create conveyors. Choose a direction that runs along every selected surface.");return 0;}
    remember();if(!commit())return 0;conveyorDraft.reset();conveyorBefore.reset();conveyorDirections.clear();conveyorDragging=-1;
    say("Conveyor belts generated. Speed is adjustable in Placed COGs. Ctrl+Z: undo.");return 1;
}
static int conveyorAim(int i,EdVec origin,EdVec direction,EdVec *point){
    EdHit h=conveyorSelection.hits[i];EdVec n=edNormal(&conveyorDraft->c[h.cell].f[h.face]),center=conveyorCenter(i);double denom=edDot(n,direction);
    if(fabs(denom)<1e-5)return 0;double t=edDot(edSub(center,origin),n)/denom;if(t<=0||t>100)return 0;
    *point=edAdd(origin,edScale(direction,t));return 1;
}
static int conveyorControl(){
    if(!conveyorActive())return 0;
    int fire=editButton(INPUT_FUNC_FIRE1),grab=editButton(INPUT_FUNC_FIRE2),enter=stdControl_ReadKey(DIK_RETURN,NULL)!=0;
    int accept=(fire&&!fireDown)||(enter&&!conveyorEnterDown),start=grab&&!grabDown;fireDown=fire;grabDown=grab;conveyorEnterDown=enter;
    if(accept&&!grab){conveyorApply();return 1;}
    if(!grab){conveyorDragging=-1;return 1;}
    EdVec origin,direction;ray(&origin,&direction);
    if(start){
        EdHit visible;if(!edRayMaterial(model,origin,direction,&visible))return 1;
        for(int i=0;i<1;i++){
            EdHit h=conveyorSelection.hits[i];if(h.cell!=visible.cell||h.face!=visible.face)continue;
            EdVec p;if(!conveyorAim(i,origin,direction,&p))continue;
            EdVec tip=edAdd(conveyorCenter(i),edScale(conveyorDirections[i],conveyorRadius(i))),d=edSub(p,tip);
            if(edDot(d,d)<pow(fmax(ED_GRID*.5,conveyorRadius(i)*.3),2)){conveyorDragging=i;break;}
        }
    }
    if(conveyorDragging>=0){EdVec p;if(conveyorAim(conveyorDragging,origin,direction,&p)){EdVec d=edSub(p,conveyorCenter(conveyorDragging));double len=sqrt(edDot(d,d));if(len>ED_GRID*.1)conveyorDirections[conveyorDragging]=edScale(d,1/len);}}
    return 1;
}
static void conveyorOverlay(int color){
    if(!conveyorActive())return;
    for(int i=0;i<1;i++){
        EdVec center=conveyorCenter(i);if(!objectOverlayVisible(center))continue;
        EdHit h=conveyorSelection.hits[i];EdVec n=edNormal(&conveyorDraft->c[h.cell].f[h.face]),d=conveyorDirections[i],side=edCross(n,d);double r=conveyorRadius(i);
        EdVec tip=edAdd(center,edScale(d,r));line(center,tip,color);vertexCircle(tip,conveyorDragging==i,color);
        line(tip,edAdd(center,edAdd(edScale(d,r*.7),edScale(side,r*.15))),color);line(tip,edAdd(center,edAdd(edScale(d,r*.7),edScale(side,-r*.15))),color);
        EdVec previous=tip;for(int j=1;j<=32;j++){double a=j*6.283185307179586/32;EdVec p=edAdd(center,edAdd(edScale(d,cos(a)*r),edScale(side,sin(a)*r)));line(previous,p,color);previous=p;}
    }
    editorDrawText(highlightCanvas->pVBuffer,jkGui_stdFonts[0],8,70,highlightCanvas->pVBuffer->format.width-16,"Conveyor: RMB-drag circle | Enter / Fire 1: apply | Escape: cancel",1);
}
