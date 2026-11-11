/* Included after the map projection helpers. */
static int splineMapInput(int left,int right,int space){
    if(splineDragging&&!grabbing)splineDragging=0;
    int start=stdControl_ReadKey(DIK_S,NULL)!=0,enter=stdControl_ReadKey(DIK_RETURN,NULL)||stdControl_ReadKey(DIK_NUMPADENTER,NULL);
    int startPressed=start&&!splineStartDown,enterPressed=enter&&!splineEnterDown;splineStartDown=start;splineEnterDown=enter;
    if(startPressed&&!space&&!netEditPending()){mapCancel();EdVec p=mapSnapXY(mapPoint(mapX,mapY));p=edAdd(p,edScale(mapN(),mapFloorZ+.5));splineBegin(p);return 1;}
    int index=splineIndex(splineSelected);if(index<0){splineSelected=splineDrawing=splineDragging=0;splinePoint=-1;}
    if(splineDrawing&&right&&!mapRight){splineFinish();return 1;}
    if(enterPressed&&!splineDrawing&&!splineDragging&&!space){int selected=0;for(int c=0;c<model->count;c++)selected|=mapSelected[c];if(selected||index<0){arrayMenu();return 1;}}
    if(enterPressed&&index>=0&&!splineDrawing&&!splineDragging){splineAttributes();return 1;}
    if(space)return splineDrawing||splineDragging;
    EdVec mouse=mapPoint(mapX,mapY),point=mapSnapXY(mouse);
    if(!splineDrawing&&!splineDragging&&right&&!mapRight&&!left&&index>=0&&splinePoint<model->splines[index].count&&!netEditPending()){
        splineDraft=model->splines[index];splineHandle=0;splineDragRight=1;splineDragging=grabbing=1;splineDragStart=splineDraft.points[splinePoint<0?0:splinePoint].position;splineDragDelta=mouse;edCopyModel(snapshot,model);return 1;
    }
    if(splineDragging){
        if(splineDragRight?right:left){
            if(splinePoint<0){EdVec delta=mapPlanar(edSub(mapSnapXY(edAdd(splineDragStart,edSub(mouse,splineDragDelta))),splineDragStart));const EdSpline &start=snapshot->splines[index];for(int p=0;p<splineDraft.count;p++)splineDraft.points[p].position=edAdd(start.points[p].position,delta);return 1;}
            EdSplinePoint &p=splineDraft.points[splinePoint];
            if(splineHandle){EdVec d=edSub(mouse,p.position);d=mapPlanar(d);p.handle=edAdd(edScale(d,splineHandle),edScale(mapN(),edDot(p.handle,mapN())));}
            else{EdVec moved=mapSnapXY(edAdd(splineDragStart,edSub(mouse,splineDragDelta)));p.position=moved;}
        }else{if(edModelsEqual(snapshot,model))splinePublish(splineDraft);else say("Spline drag cancelled: level changed.");splineDragging=grabbing=0;}
        return 1;
    }
    if(left&&!mapLeft&&!right&&!netEditPending()){
        if(splineDrawing&&index>=0){splineDraft=model->splines[index];
            int last=splineDraft.count-1;
            if(last>=2&&mapDistance(mouse,splineDraft.points[0].position)*mapZoom<8){
                splineDraft.closed=1;if(splinePublish(splineDraft)){splineFinish();splinePoint=-1;say("Closed spline. The final sector connects back to the first.");}return 1;
            }
            if(mapDistance(mouse,splineDraft.points[last].position)*mapZoom<8)splinePoint=last;
            else {if(splineDraft.count==ED_SPLINE_POINTS){say("Spline point limit reached.");return 1;}splinePoint=splineDraft.count++;point=edAdd(point,edScale(mapN(),edDot(splineDraft.points[last].position,mapN())));splineDraft.points[splinePoint]={point,edV(0,0,0)};}
            splineHandle=1;splineDragRight=0;splineDragging=grabbing=1;edCopyModel(snapshot,model);return 1;
        }
        int id=0,hit=-1,handle=0;double best=9/mapZoom;
        if(index>=0&&splinePoint>=0&&splinePoint<model->splines[index].count){const EdSplinePoint &p=model->splines[index].points[splinePoint];
            for(int sign=-1;sign<=1;sign+=2){EdVec h=edAdd(p.position,edScale(splineVisibleHandle(model->splines[index],splinePoint),sign));double d=mapDistance(mouse,h);if(d<best){best=d;id=splineSelected;hit=splinePoint;handle=sign;}}
        }
        if(!id)for(int i=0;i<model->splineCount;i++)for(int p=0;p<model->splines[i].count;p++){EdVec v=model->splines[i].points[p].position;double d=mapDistance(mouse,v);if(d<best){best=d;id=model->splines[i].id;hit=p;}}
        if(id){splineSelected=id;splinePoint=hit;splineHandle=handle;splineDragRight=0;splineDraft=model->splines[splineIndex(id)];splineDragging=grabbing=1;splineDragStart=splineDraft.points[hit].position;splineDragDelta=mouse;edCopyModel(snapshot,model);picks.count=0;sectorPicks.clear();mapSelected.clear();return 1;}
        for(int i=0;i<model->splineCount;i++){const EdSpline &s=model->splines[i];EdVec a=edSplinePosition(&s,0);for(int j=1;j<=(s.closed?s.count:s.count-1)*32;j++){EdVec b=edSplinePosition(&s,j/32.0),e=mapProject(edSub(b,a)),offset=mapProject(edSub(mouse,a));double den=e.x*e.x+e.y*e.y,u=den?fmax(0,fmin(1,(offset.x*e.x+offset.y*e.y)/den)):0;double d=hypot(offset.x-u*e.x,offset.y-u*e.y);if(d<best){best=d;id=s.id;}a=b;}}
        if(id){splineSelected=id;splinePoint=-1;picks.count=0;mapSelected.clear();say("Spline selected. Enter: shape, sector count, width and height.");return 1;}
        splineSelected=0;splinePoint=-1;
    }
    return splineDrawing;
}
static void splineMapDraw(void){
    int cyan=mapColor(40,210,255),yellow=mapColor(255,220,20);
    for(int i=0;i<model->splineCount;i++){const EdSpline &s=splineDragging&&model->splines[i].id==splineSelected?splineDraft:model->splines[i];int color=s.invalid?mapColor(255,45,45):s.id==splineSelected?yellow:cyan;
        if(s.invalid)for(size_t c=0;c<s.preview.size();c++)for(int f=0;f<s.preview[c].count;f++){const EdFace &face=s.preview[c].f[f];for(int v=0;v<face.count;v++)mapWorldLine(face.v[v].p,face.v[(v+1)%face.count].p,color,2);}
        EdVec a=edSplinePosition(&s,0);for(int j=1;j<=(s.closed?s.count:s.count-1)*32;j++){EdVec b=edSplinePosition(&s,j/32.0);mapWorldLine(a,b,color,3);a=b;}
        for(int p=0;p<s.count;p++){mapMarker(s.points[p].position,1,color);if(s.id==splineSelected&&p==splinePoint){EdVec h=splineVisibleHandle(s,p),center=s.points[p].position;mapWorldLine(edSub(center,h),edAdd(center,h),cyan,2);mapMarker(edSub(center,h),0,cyan);mapMarker(edAdd(center,h),0,cyan);}}
        if(splineDrawing&&s.id==splineSelected&&!splineDragging){EdVec end=mapPoint(mapX,mapY);mapWorldLine(s.points[s.count-1].position,end,cyan,1);}
    }
}
