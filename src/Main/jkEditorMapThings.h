// Things share the map's six projections and ordinary edit transactions.
static int mapThingId(int index){return index<model->spawnCount?-index-1:index-model->spawnCount;}
static int mapThingEditable(int id){return id<0?(-id-1<model->spawnCount):(id<model->objectCount&&edTerrainObject(model,id)<0);}
static int mapPickThing(EdVec point){
    int best=-10000;double distance=10/mapZoom;
    for(int i=0;i<model->spawnCount+model->objectCount;i++){
        int id=mapThingId(i);if(!mapThingEditable(id))continue;
        double d=mapDistance(point,thingPose(model,id).position);
        if(d<distance){distance=d;best=id;}
    }return best;
}
static int mapSelectThings(EdVec a,EdVec b,int box){
    if(box){
        // Weapon 3 gives box selection to things; other tools keep sector boxes.
        if(tool!=3)return 0;mapThings.clear();a=mapProject(a);b=mapProject(b);
        for(int i=0;i<model->spawnCount+model->objectCount;i++){
            int id=mapThingId(i);if(!mapThingEditable(id))continue;EdVec p=mapProject(thingPose(model,id).position);
            if(p.x>=fmin(a.x,b.x)&&p.x<=fmax(a.x,b.x)&&p.y>=fmin(a.y,b.y)&&p.y<=fmax(a.y,b.y))mapThings.push_back(id);
        }
    }else{
        int id=mapPickThing(b);if(id==-10000)return 0;
        auto it=std::find(mapThings.begin(),mapThings.end(),id);
        if(it==mapThings.end())mapThings.push_back(id);else mapThings.erase(it);
    }
    mapSelected.clear();sectorPicks.clear();picks.count=0;splineSelected=0;splinePoint=-1;
    char text[100];snprintf(text,sizeof(text),"%d things selected. RMB: move. Ctrl+Z: undo.",(int)mapThings.size());say(text);return 1;
}
static int mapMoveThings(EdVec rawDelta,EdVec *delta){
    if(mapGrabThings.empty())return 0;
    double grid=mapGridSpacing()*.5;EdVec anchor=mapProject(thingPose(snapshot,mapGrabThings[0]).position),d=mapProject(rawDelta);
    *delta=mapUnproject(edV(round((anchor.x+d.x)/grid)*grid-anchor.x,round((anchor.y+d.y)/grid)*grid-anchor.y,0));
    if(edDot(edSub(*delta,mapLastDelta),edSub(*delta,mapLastDelta))<=ED_EPS*ED_EPS)return 0;
    edCopyModel(candidate,snapshot);
    for(int id:mapGrabThings){
        EdFrame pose=thingPose(snapshot,id);pose.position=edAdd(pose.position,*delta);
        if(objectSector(snapshot,pose.position)<0){say("Thing centers must stay inside a sector.");return 0;}
        setThingPose(candidate,id,pose);
    }return 1;
}
static std::pair<EdVec,EdVec> mapThingBounds(int id){
    static uint64_t revision=~uint64_t(0);static std::map<int,std::pair<EdVec,EdVec>> cache;
    if(revision!=geometryDocumentRevision){cache.clear();revision=geometryDocumentRevision;}
    auto found=cache.find(id);if(found!=cache.end())return found->second;
    EdFrame pose=thingPose(model,id);SithThing *definition=sithTemplate_GetTemplate(thingTemplate(model,id));
    double radius=definition?fmax(.025,definition->collideSize):.05;
    EdVec lo=edSub(pose.position,edV(radius,radius,radius)),hi=edAdd(pose.position,edV(radius,radius,radius));
    if(definition&&definition->renderData.type==RD_THING_MODEL3&&definition->renderData.model3){
        rdModel3 *mesh=definition->renderData.model3;rdThing preview={};rdThing_NewEntry(&preview,NULL);bool first=true;
        if(mesh->numGeos&&rdThing_SetModel3(&preview,mesh)){
            rdMatrix34 placement;rdVector3 angles=rdvec(pose.rotation);rdMatrix_BuildRotate34(&placement,&angles);placement.scale=rdvec(pose.position);preview.rdFrameNum=rdroid_frameTrue-1;
            for(int n=0;n<mesh->numHNodes;n++){
                int index=mesh->aHierarchyNodes[n].meshIdx;if(index<0||index>=(int)mesh->aGeos[0].numMeshes)continue;
                rdMatrix34 transform;if(!rdModel3_GetMeshMatrix(&preview,&placement,n,&transform))continue;
                rdMesh *part=&mesh->aGeos[0].aMeshes[index];
                for(int v=0;v<part->numVertices;v++){
                    rdVector3 world;rdMatrix_TransformPoint34(&world,&part->aVertices[v],&transform);EdVec p=vec(world);
                    if(first){lo=hi=p;first=false;}else{lo=edV(fmin(lo.x,p.x),fmin(lo.y,p.y),fmin(lo.z,p.z));hi=edV(fmax(hi.x,p.x),fmax(hi.y,p.y),fmax(hi.z,p.z));}
                }
            }
        }rdThing_FreeEntry(&preview);
    }
    return cache[id]={lo,hi};
}
static void mapDrawThings(void){
    int hovered=mapPickThing(mapPoint(mapX,mapY));
    for(int i=0;i<model->spawnCount+model->objectCount;i++){
        int id=mapThingId(i);if(!mapThingEditable(id))continue;
        bool selected=std::find(mapThings.begin(),mapThings.end(),id)!=mapThings.end();
        int color=selected?mapColor(255,48,48):id==hovered?mapColor(255,224,64):mapColor(64,192,255);
        auto bounds=mapThingBounds(id);EdVec corners[8];
        for(int j=0;j<8;j++)corners[j]=edV(j&1?bounds.second.x:bounds.first.x,j&2?bounds.second.y:bounds.first.y,j&4?bounds.second.z:bounds.first.z);
        for(int j=0;j<8;j++)for(int axis=1;axis<=4;axis*=2)if(!(j&axis))mapWorldLine(corners[j],corners[j|axis],color,selected?2:1);
        mapMarker(thingPose(model,id).position,selected||id==hovered,color);
    }
}
static int mapThingsTest(SithThing *player){
    tool=3;active=1;edCopyModel(candidate,model);candidate->objectCount=1;
    candidate->objects[0]={};strcpy(candidate->objects[0].templateName,"ghost");candidate->objects[0].position=edV(.25,.25,.5);
    if(!commit())return 0;mapOpen(player);int ok=1;
    for(int view=0;view<6&&ok;view++){
        mapSetView(view);mapThings.clear();clearHistory();mapClickReady=mapClickTracking=mapClickConsumed=0;
        EdVec start=model->objects[0].position;mapCenter=start;mapX=mapWidth*.5;mapY=mapHeight*.5;mapLeft=mapRight=0;
        mapInput(0,0,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
        ok=mapThings.size()==1&&mapThings[0]==0;
        mapInput(0,0,0,0,1,0,0,0,0,0);double step=mapGridSpacing()*.5;
        mapInput(step*mapZoom,0,0,0,1,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
        EdVec delta=edSub(model->objects[0].position,start);ok=ok&&fabs(edDot(delta,mapU())-step)<1e-7&&fabs(edDot(delta,mapN()))<1e-7&&undoCount==1;
        auto bounds=mapThingBounds(0);ok=ok&&bounds.second.x>bounds.first.x&&bounds.second.z>bounds.first.z;
        ok=ok&&historyStep(0)&&edDot(edSub(model->objects[0].position,start),edSub(model->objects[0].position,start))<1e-12;
    }
    mapView=0;tool=3;ok=ok&&mapSelectThings(edV(-1,-1,0),edV(1,1,0),1)&&mapThings.size()==2;
    mapClose(player);printf("GEOSITH_MAP_THINGS %s: six-view node selection, drag, half-grid snap, bounds, box selection and undo\n",ok?"PASS":"FAIL");return ok;
}
