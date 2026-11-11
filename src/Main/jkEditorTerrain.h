static int terrainSelected=-1,terrainStroke,terrainTextureMode;
static int terrainPendingSlot=-1,terrainPendingMaterial;
static char terrainPendingName[32];
static double terrainRadius=.5,terrainElapsed,terrainGrabRadius,terrainIntensity=.25;
static int terrainGrabbed=-1;
static EdVec terrainGrabCenter,terrainGrabDelta;
static EdHit terrainHover={-1,-1,-1,-1};
static int terrainSculptCommit(void){
    terrainDeferBake=1;int result=commit();terrainDeferBake=0;return result;
}
static void terrainRefreshPaint(void){
    if(terrainDeferredBakes.empty())return;
    for(int a=0;a<model->assetCount;a++)if(terrainDeferredBakes.count(model->assets[a].name)){
        EdPaintBake bake;if(edPainted(model,a))terrainBakeLive(model,a,&bake);
    }
    terrainDeferredBakes.clear();
}
static int terrainPick(EdVec origin,EdVec direction,EdHit *hit){
    *hit=EdHit{-1,-1,-1,-1,1e30};EdHit wall;edRay(model,origin,direction,&wall);
    for(int a=0;a<model->assetCount;a++)if(model->assets[a].terrain){EdHit h;if(edMeshRay(model,a,origin,direction,0,&h)&&h.distance<hit->distance&&(wall.cell<0||h.distance<=wall.distance+.002))*hit=h;}
    return hit->cell>=0?model->meshes[hit->cell].asset:-1;
}
static int terrainCreate(void){
    if(terrainSessionOpen()){terrainSessionFinish();say("Current terrain finished. Click again to create a new terrain.");return 0;}
    if(netEditPending())return 0;EdSelection surfaces=picks;
    if(!surfaces.count){if(hover.cell<0)return 0;surfaces.count=1;surfaces.hits[0]=hover;surfaces.hits[0].edge=surfaces.hits[0].vertex=-1;}
    char name[32];int found=0;
    for(int i=1;i<10000;i++){snprintf(name,32,"terrain%d",i);found=edTemplateIndex(model,name)>=0||sithTemplate_GetTemplate(name)!=NULL;for(int a=0;a<model->assetCount;a++)found|=!strcmp(name,model->assets[a].name);char file[64];snprintf(file,64,"%s.3do",name);found|=builderNameExists(file);if(!found)break;}
    if(found||!edTerrainCreate(candidate,model,&surfaces,name)){say("Could not create terrain: select solid faces; subdivision or geometry validation failed.");return 0;}
    int asset=model->assetCount;for(int a=asset;a<candidate->assetCount;a++)for(int slot=0;slot<2;slot++)snprintf(candidate->assets[a].paintSlots[slot],128,"%s",paletteNames[currentMaterial]);for(int a=asset;a<candidate->assetCount;a++)if(!builderWalkstruct(candidate,a,candidate->assets[a].name))return 0;
    remember();if(!commit())return 0;terrainSelected=asset;picks.count=0;sectorPicks.clear();
    say("Terrain created. Fire 1 raises; Shift lowers; Ctrl smooths; Fire 2 grabs; Shift+wheel resizes. Edges can be sculpted too. Middle mouse: terrain options.");return 1;
}
static int terrainConvert(int asset,int walkstruct,int remove=0){
    if(terrainSessionOpen()){terrainSessionFinish();say("Finish sharing the terrain before converting or deleting it.");return 0;}
    if(asset<0||asset>=model->assetCount||!model->assets[asset].terrain||netEditPending())return 0;
    edCopyModel(candidate,model);int group=model->assets[asset].terrainGroup;
    for(int part=model->assetCount-1;part>=0;part--){if(part!=asset&&(!group||model->assets[part].terrainGroup!=group||!model->assets[part].terrain))continue;
    char name[32];snprintf(name,32,"%s",candidate->assets[part].name);
    if(!walkstruct){
        for(int o=candidate->objectCount-1;o>=0;o--)if(!strcmp(candidate->objects[o].templateName,name)){for(int j=o+1;j<candidate->objectCount;j++)candidate->objects[j-1]=candidate->objects[j];candidate->objectCount--;edCogThingChanged(candidate,candidate->spawnCount+o,0);}
        int t=edTemplateIndex(candidate,name);if(t>=0){for(int j=t+1;j<candidate->templateCount;j++)candidate->templates[j-1]=candidate->templates[j];candidate->templateCount--;}
    }
    candidate->assets[part].terrain=0;candidate->assets[part].terrainGroup=0;candidate->assets[part].terrainNormal=edV(0,0,0);for(int i=0;i<candidate->meshCount;i++)if(candidate->meshes[i].asset==part){candidate->meshes[i].terrainPinned.clear();candidate->meshes[i].terrainNormals.clear();}
    if(remove)builderRemoveAsset(candidate,part);
    }
    remember();if(!commit())return 0;terrainSelected=-1;if(remove){say("Terrain deleted. Ctrl+Z: undo.");return 1;}say(walkstruct?"Terrain converted to a normal walkstruct in place. Ctrl+Z restores sculpting.":"Terrain saved in the custom 3DO library and removed from the scene. Ctrl+Z restores it.");return 1;
}
static int terrainDelete(int asset){terrainStroke=grabbing=0;terrainHover.cell=-1;return terrainConvert(asset,0,1);}
static void terrainIntensityMenu(void){
    char16_t value[128];char text[128];snprintf(text,sizeof(text),"%.0f",terrainIntensity*100);stdString_CharToWchar(value,text,128);
    for(;;){jkGuiElement e[6]={};e[0]={ELEMENT_TEXT,0,2,NULL,3,{30,30,580,40},1};e[1]={ELEMENT_TEXT,0,0,NULL,3,{40,150,280,30},1};e[2]={ELEMENT_TEXTBOX,0,0,NULL,127,{340,150,220,32},1};e[3]={ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,400,220,36},1};e[4]={ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,400,220,36},1};e[5].type=ELEMENT_END;
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"Terrain Brush Intensity";e[1].wstr=u"Intensity (1-100%)";e[2].wstr=value;e[3].wstr=u"Apply";
        int result=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);if(result<0)return;stdString_WcharToChar(text,value,128);double number;char extra;
        if(sscanf(text," %lf %c",&number,&extra)==1&&isfinite(number)&&number>=1&&number<=100){terrainIntensity=number/100;return;}jkGuiDialog_ErrorDialog(u"Brush intensity",u"Enter a number from 1 to 100.");
    }
}
static int terrainContactSelfTest(int editorMode){
    if(!bindEditorWorld()){printf("Terrain contact failure at line %d\n",__LINE__);fflush(stdout);return 0;}
    active=editorMode;syncEditorObjects(model);
    int asset=-1;for(int a=0;a<model->assetCount;a++)if(model->assets[a].terrain){asset=a;break;}if(asset<0){printf("Terrain contact failure at line %d\n",__LINE__);fflush(stdout);return 0;}
    edCopyModel(candidate,model);for(int i=0;i<candidate->meshCount;i++)if(candidate->meshes[i].asset==asset)candidate->meshes[i].paint.clear();if(!commit()){printf("Terrain contact failure at line %d\n",__LINE__);fflush(stdout);return 0;}
    char file[64];snprintf(file,sizeof(file),"%s.3do",model->assets[asset].name);rdModel3 *render=sithModel_Find(file);if(!render){printf("Terrain contact failure at line %d\n",__LINE__);fflush(stdout);return 0;}
    SithWorld *world=sithWorld_g_pCurrentWorld;SithThing *support=NULL,*player=world->pLocalPlayer;
    for(int i=0;i<model->objectCount;i++)if(objectThings[i]>=0){SithThing *t=&world->aThings[objectThings[i]];if(t->renderData.type==RD_THING_MODEL3&&t->renderData.model3==render){support=t;break;}}
    if(!support||!player){printf("Terrain contact failure at line %d\n",__LINE__);fflush(stdout);return 0;}
    rdMesh *lastMesh=&render->aGeos[0].aMeshes[render->aGeos[0].numMeshes-1];
    sithThing_AttachThingToThingFace(player,support,&lastMesh->faces[0],lastMesh->aVertices,1);
    sithDSSThing_Attachment(player,-1,0,0);SithMessage attachment=sithComm_netMsgTmp;
    sithThing_DetachThing(player);
    if(attachment.netMsg.msg_size!=16||!sithDSSThing_ProcessAttachment(&attachment)||(rdFace*)player->attachedSufaceInfo!=&lastMesh->faces[0]){puts("Terrain multi-mesh attachment roundtrip failed");return 0;}
    puts("Terrain multi-mesh attachment roundtrip PASS");
    rdMesh *mesh=&render->aGeos[0].aMeshes[0];sithThing_AttachThingToThingFace(player,support,&mesh->faces[0],mesh->aVertices,1);
    rdVector3 position=player->position,velocity=player->physicsParams.vel;int signature=support->signature;
    EdVec point={0};for(int i=0;i<model->meshCount;i++)if(model->meshes[i].asset==asset){point=model->meshes[i].geometry.f[0].v[0].p;break;}
    for(int step=0;step<20;step++){
        edCopyModel(candidate,model);if(!edTerrainPaint(candidate,asset,point,.75,.3,step%2?"sand1.mat":"dflt.mat",terrainMaterialName)){printf("Terrain contact failure at line %d\n",__LINE__);fflush(stdout);return 0;}
        unsigned bytes=0;unsigned char *wire=edWireEncode(candidate,&bytes);EdModel decoded;if(!wire||!edWireDecode(&decoded,wire,bytes)){edFree(wire);{printf("Terrain contact failure at line %d\n",__LINE__);fflush(stdout);return 0;}}edFree(wire);
        if(!netApplyLocal(&decoded)){printf("Terrain contact failure at line %d\n",__LINE__);fflush(stdout);return 0;}
        mesh=&render->aGeos[0].aMeshes[0];
        if(!(player->attach_flags&SITH_ATTACH_THINGFACE)||player->attachedThing!=support||support->signature!=signature
            ||(rdFace*)player->attachedSufaceInfo!=&mesh->faces[0]||memcmp(&position,&player->position,sizeof(position))||memcmp(&velocity,&player->physicsParams.vel,sizeof(velocity))){printf("Terrain contact failure at line %d\n",__LINE__);fflush(stdout);return 0;}
    }
    /* A later non-paint edit must not recreate the freshly painted support. */
    edCopyModel(candidate,model);candidate->header.gravity+=.01;if(!commit()||support->signature!=signature||player->attachedThing!=support||!(player->attach_flags&SITH_ATTACH_THINGFACE)){printf("Terrain contact failure at line %d\n",__LINE__);fflush(stdout);return 0;}
    edCopyModel(candidate,model);if(!edTerrainSculpt(candidate,asset,point,.75,.001,edV(0,0,1))||!commit()||(player->attach_flags&SITH_ATTACH_THINGFACE)){printf("Terrain sculpt contact failure\n");return 0;}
    printf("Terrain contact PASS (editor=%d): first paint, 20 wire updates, later edit, safe sculpt detachment.\n",editorMode);return 1;
}
static void terrainApplyPendingTexture(void){
    if(terrainPendingSlot<0)return;int asset=edTerrainNamed(model,terrainPendingName);
    if(asset<0){terrainPendingSlot=-1;return;}
    if(!terrainSessionBegin(asset))return;
    edCopyModel(candidate,model);for(int a=0;a<candidate->assetCount;a++)if(edTerrainMember(candidate,a,asset))snprintf(candidate->assets[a].paintSlots[terrainPendingSlot],128,"%s",paletteNames[terrainPendingMaterial]);
    remember();if(commit())say("Paint texture selected. Existing painting is preserved.");terrainPendingSlot=-1;
}
static void terrainMenu(void){
    BuilderMenuState state=builderMenuBegin();
    if(terrainSelected<0||terrainSelected>=model->assetCount||!model->assets[terrainSelected].terrain){std::unique_ptr<char16_t[][128]> names(new char16_t[model->assetCount][128]{});std::vector<int> ids(model->assetCount);int count=0;for(int a=0;a<model->assetCount;a++)if(model->assets[a].terrain){ids[count]=a;stdString_CharToWchar(names[count++],model->assets[a].name,128);}if(!count){terrainIntensityMenu();builderMenuEnd(state);return;}int choice=chooseList(u"Choose Terrain",names.get(),count,0);terrainSelected=choice>=0?ids[choice]:-1;}
    if(terrainSelected>=0&&terrainSelected<model->assetCount&&model->assets[terrainSelected].terrain){
        char name[32];snprintf(name,32,"%s",model->assets[terrainSelected].name);char16_t title[128],choices[5][128]={u"Texture 1...",u"Convert to 3DO (model library)",u"Convert to walkstruct (keep in level)",u"Brush intensity...",u"Texture 2..."};stdString_CharToWchar(title,name,128);
        char intensityText[80];snprintf(intensityText,sizeof(intensityText),"Brush intensity: %.0f%%...",terrainIntensity*100);stdString_CharToWchar(choices[3],intensityText,128);
        for(int slot=0;slot<2;slot++){char label[180];const char *texture=model->assets[terrainSelected].paintSlots[slot];snprintf(label,sizeof(label),"Texture %d: %s...",slot+1,*texture?texture:"choose material");stdString_CharToWchar(choices[slot?4:0],label,128);}
        int choice=chooseList(title,choices,5,0),asset=-1;for(int a=0;a<model->assetCount;a++)if(model->assets[a].terrain&&!strcmp(name,model->assets[a].name))asset=a;
        if(asset>=0&&(choice==0||choice==4)&&!netEditPending()){
            int slot=choice==4;
            int oldMaterial=currentMaterial,oldObjects=browserObjects;for(int i=0;i<model->meshCount;i++)if(model->meshes[i].asset==asset){currentMaterial=model->meshes[i].geometry.f[0].material;break;}
            if(model->assets[asset].paintSlots[slot][0]){int id=addMaterialName(model->assets[asset].paintSlots[slot]);if(id>=0)currentMaterial=id;}
            browserObjects=0;materialMenu(1);int material=currentMaterial;currentMaterial=oldMaterial;browserObjects=oldObjects;
            if(asset<model->assetCount&&model->assets[asset].terrain&&!strcmp(name,model->assets[asset].name)&&!netEditPending()){terrainPendingSlot=slot;terrainPendingMaterial=material;snprintf(terrainPendingName,32,"%s",name);terrainApplyPendingTexture();}
        }else if(choice==3)terrainIntensityMenu();else if(asset>=0&&(choice==1||choice==2))terrainConvert(asset,choice==2);
    }
    builderMenuEnd(state);
}
static int terrainControl(EdVec origin,EdVec direction,int fire,int middle,int shift,int wheel,double delta,int smooth=0,int grab=0){
    terrainApplyPendingTexture();
    if(terrainStroke&&!grabbing){terrainStroke=0;terrainElapsed=0;}
    int asset=terrainPick(origin,direction,&terrainHover);if(asset>=0)terrainSelected=asset;
    if(wheel&&smooth)terrainIntensity=fmax(.01,fmin(1,terrainIntensity+(wheel>0?.05:-.05)));
    else if(wheel&&shift)terrainRadius=fmax(ED_GRID*2,fmin(32,terrainRadius*pow(1.2,wheel>0?1:-1)));
    else if(wheel&&!terrainStroke){terrainTextureMode=!terrainTextureMode;say(terrainTextureMode?"Terrain texture mode: Fire 1 paints slot 1; Fire 2 paints slot 2.":"Terrain sculpt mode.");}
    if(middle){terrainStroke=grabbing=0;terrainMenu();return 1;}
    if(asset>=0&&(fire||grab)&&!terrainSessionBegin(asset)){terrainStroke=grabbing=0;terrainElapsed=0;fireDown=grabDown=0;return 1;}
    if(terrainTextureMode){
        if((fire||grab)&&!terrainStroke&&asset>=0){remember();terrainStroke=3;grabbing=1;terrainElapsed=.05;}
        if(terrainStroke==3){terrainElapsed+=fmin(.1,fmax(0,delta));
            if(asset>=0&&(fire||grab)&&terrainElapsed>=.05){
                int slot=grab?1:0;const char *texture=model->assets[asset].paintSlots[slot];
                if(!*texture)texture=terrainMaterialName(currentMaterial);
                edCopyModel(candidate,model);
                if(texture&&edTerrainPaint(candidate,asset,terrainHover.point,terrainRadius,4*terrainElapsed*terrainIntensity,texture,terrainMaterialName))commit();
                terrainElapsed=0;
            }
            if(!fire&&!grab){terrainStroke=grabbing=0;terrainElapsed=0;}
        }
        fireDown=fire;grabDown=grab;return 1;
    }
    if(grab&&!grabDown&&!terrainStroke&&asset>=0){
        remember();edCopyModel(snapshot,model);terrainGrabbed=asset;terrainGrabCenter=terrainHover.point;terrainGrabRadius=terrainRadius;terrainGrabDelta=edV(0,0,0);
        edDragTraceBegin(&geometryDragTrace,origin,direction,terrainGrabCenter,direction,fmax(4,terrainHover.distance*8));terrainStroke=2;grabbing=1;
    }
    if(terrainStroke==2){
        if(grab){terrainGrabDelta=edAdd(terrainGrabDelta,edDragTraceStep(&geometryDragTrace,origin,direction));if(edTerrainGrab(candidate,snapshot,terrainGrabbed,terrainGrabCenter,terrainGrabRadius,terrainGrabDelta))terrainSculptCommit();}
        else {terrainStroke=grabbing=0;terrainGrabbed=-1;}
        fireDown=fire;grabDown=grab;return 1;
    }
    if(fire&&!fireDown&&!terrainStroke&&!grab){
        if(asset<0){if(!smooth)terrainCreate();fireDown=1;return 1;}
        remember();terrainStroke=grabbing=1;terrainElapsed=0;
    }
    if(terrainStroke){terrainElapsed+=fmin(.1,fmax(0,delta));
        if(asset>=0&&(terrainElapsed>=.05||!fire)){edCopyModel(candidate,model);if(smooth?edTerrainSmooth(candidate,asset,terrainHover.point,terrainRadius,fmin(.9,16*terrainElapsed*terrainIntensity)):edTerrainSculpt(candidate,asset,terrainHover.point,terrainRadius,(shift?-1:1)*.4*terrainElapsed*terrainIntensity,edNormal(&model->meshes[terrainHover.cell].geometry.f[terrainHover.face])))terrainSculptCommit();terrainElapsed=0;}
        if(!fire){terrainStroke=grabbing=0;terrainElapsed=0;}
    }
    fireDown=fire;grabDown=grab;return 1;
}
static void terrainDrawBorder(int color){
    // Count geometric edges across every mesh in the owned group. Shared
    // triangle edges and seams between terrain assets are not borders.
    struct Border {EdVec a,b;int count=0;};
    std::map<std::vector<long long>,Border> edges;
    for(int m=0;m<model->meshCount;m++)if(terrainSessionOwned(model->meshes[m].asset)){
        const EdCell &cell=model->meshes[m].geometry;
        for(int f=0;f<cell.count;f++)for(int v=0;v<cell.f[f].count;v++){
            EdVec a=cell.f[f].v[v].p,b=cell.f[f].v[(v+1)%cell.f[f].count].p;
            std::vector<long long> ka={llround(a.x*1000000),llround(a.y*1000000),llround(a.z*1000000)},kb={llround(b.x*1000000),llround(b.y*1000000),llround(b.z*1000000)};
            if(kb<ka)ka.swap(kb);ka.insert(ka.end(),kb.begin(),kb.end());
            Border &edge=edges[ka];edge.a=a;edge.b=b;edge.count++;
        }
    }
    for(const auto &entry:edges)if(entry.second.count==1)line(entry.second.a,entry.second.b,color);
}
static void terrainDraw(int color){
    if(tool!=7||terrainHover.cell<0||terrainHover.cell>=model->meshCount)return;
    int asset=model->meshes[terrainHover.cell].asset;if(!model->assets[asset].terrain)return;EdVec n=edNormal(&model->meshes[terrainHover.cell].geometry.f[terrainHover.face]);
    EdVec u=edCross(fabs(n.z)<.9?edV(0,0,1):edV(0,1,0),n);u=edScale(u,1/sqrt(edDot(u,u)));EdVec v=edCross(n,u),previous={};int previousValid=0;
    for(int i=0;i<=64;i++){double angle=i*6.283185307179586/64;EdVec p=edAdd(terrainHover.point,edAdd(edScale(u,cos(angle)*terrainRadius),edScale(v,sin(angle)*terrainRadius)));EdHit hit;
        int valid=edMeshRay(model,asset,edAdd(p,edScale(n,2048)),edScale(n,-1),0,&hit);if(valid){p=edAdd(hit.point,edScale(n,.003));if(previousValid)line(previous,p,color);previous=p;}previousValid=valid;
    }
}

static int terrainPaintSelfTest(int asset){
    int material=-1;for(int i=1;i<paletteCount;i++)if(editorMaterial(i)){material=i;break;}if(material<0)return 0;
    edCopyModel(candidate,model);snprintf(candidate->assets[asset].paintSlots[1],128,"%s",paletteNames[material]);if(!commit())return 0;
    EdModel before=*model;EdVec origin=edV(0,0,.5),direction=edV(0,0,-1);terrainTextureMode=1;terrainStroke=grabbing=fireDown=grabDown=0;
    SithSector *sectors=sithWorld_g_pCurrentWorld->aSectors;
    terrainControl(origin,direction,0,0,0,0,.1,0,1);
    rdMesh *liveMeshes=sithTemplate_GetTemplate(model->assets[asset].name)->renderData.model3->aGeos[0].aMeshes;
    terrainControl(origin,direction,0,0,0,0,.1,0,1);terrainControl(origin,direction,0,0,0,0,0);
    if(sectors!=sithWorld_g_pCurrentWorld->aSectors||liveMeshes!=sithTemplate_GetTemplate(model->assets[asset].name)->renderData.model3->aGeos[0].aMeshes)return 0;
    if(!edPainted(model,asset)||edModelsEqual(&before,model))return 0;
    EdModel painted=*model;if(!historyStep(0)||!edModelsEqual(model,&before)||!historyStep(1)||!edModelsEqual(model,&painted))return 0;
    SithThing *definition=sithTemplate_GetTemplate(model->assets[asset].name);if(!definition||!definition->renderData.model3)return 0;
    rdMaterial *mat=definition->renderData.model3->aGeos[0].aMeshes[0].faces[0].material;
    if(!mat||!rdMaterial_EnsureDataForced(mat)||!mat->textures||!mat->textures[0].texture_struct[0]||mat->textures[0].texture_struct[0]->format.format.bpp!=16)return 0;
    terrainControl(origin,direction,1,0,0,0,.1);terrainControl(origin,direction,0,0,0,0,0);terrainTextureMode=0;
    // Sculpt updates must reuse atlas pixels throughout the stroke, then
    // produce exactly the same final atlas as an immediate full bake.
    char bakedName[32];edPaintFilename(model,asset,bakedName);std::string path=std::string("mat/")+bakedName;
    std::vector<unsigned char> pixels=terrainBakeCache[path].mat;
    for(int i=0;i<8;i++){
        edCopyModel(candidate,model);
        if(!edTerrainSculpt(candidate,asset,edV(0,0,0),.5,.002)||!terrainSculptCommit())return 0;
        if(terrainBakeCache[path].mat!=pixels||!terrainDeferredBakes.count(model->assets[asset].name))return 0;
    }
    terrainRefreshPaint();EdPaintBake reference;
    if(!terrainDeferredBakes.empty()||!edPaintBakeAsset(model,asset,&reference)||terrainBakeCache[path].mat!=reference.mat||reference.mat==pixels)return 0;
    printf("GEOSITH_TERRAIN deferred sculpt bake PASS: 8 updates reused atlas, final bake matches reference\n");
    return edPainted(model,asset);
}
static int terrainSelfTest(SithThing *player){
    clearHistory();tool=7;fireDown=grabDown=0;picks.count=0;
    EdVec origin=edV(0,0,.5),direction=edV(0,0,-1);if(!edRay(model,origin,direction,&hover))return 0;
    terrainTextureMode=0;if(!terrainCreate()||terrainSelected<0)return 0;int asset=terrainSelected;
    if(edTerrainObject(model,model->objectCount-1)!=asset||!sithTemplate_GetTemplate(model->assets[asset].name))return 0;
    SithThing *terrainTemplate=sithTemplate_GetTemplate(model->assets[asset].name);if(!(terrainTemplate->flags&SITH_TF_EARTH)||(terrainTemplate->flags&SITH_TF_METAL))return 0;
    // Live terrain must not carry fullbright vertex prelighting (saved 3DOs use zero).
    rdModel3 *terrainModel=terrainTemplate->renderData.model3;if(!terrainModel||!terrainModel->numGeos)return 0;
    for(int g=0;g<terrainModel->numGeos;g++)for(int m=0;m<terrainModel->aGeos[g].numMeshes;m++){
        rdMesh *mesh=&terrainModel->aGeos[g].aMeshes[m];
        if(mesh->lightingMode!=3)return 0;
        for(int v=0;v<mesh->numVertices;v++)if(mesh->vertices_i[v]!=0)return 0;
    }
    if(getenv("GEOSITH_TERRAIN_GROW_TEST")){
        for(int i=1;i<24;i++)if(!terrainCreate())return 0;
        if(model->assetCount!=24||model->meshCount<24)return 0;
        for(int a=0;a<24;a++){SithThing *t=sithTemplate_GetTemplate(model->assets[a].name);if(!t||!t->renderData.model3)return 0;}
        EdModel final=*model; if(!historyStep(0)||model->assetCount!=23||!historyStep(1)||!edModelsEqual(model,&final))return 0;
        printf("GEOSITH_TERRAIN_GROW PASS: 24 live assets, model cache growth, undo/redo and export\n");
        return writeEditorGob(model,"Episode/sithed_terrain_growth_test.gob","Terrain growth test",0);
    }
    // Repeated brush updates must work even with no unused live-model slots.
    SithWorld *world=sithWorld_g_pCurrentWorld;int modelCount=world->numModels,capacity=world->sizeModels;
    world->sizeModels=modelCount;
    for(int tick=0;tick<80;tick++){
        edCopyModel(candidate,model);
        if(!edTerrainSculpt(candidate,asset,edV(0,0,0),.5,tick%2?-.001:.001)||!commit()||world->numModels!=modelCount){world->sizeModels=capacity;return 0;}
    }
    world->sizeModels=capacity;
    EdModel flat;edCopyModel(&flat,model);int history=undoCount;
    terrainControl(origin,direction,1,0,0,0,.1);terrainControl(origin,direction,1,0,0,0,.1);terrainControl(origin,direction,0,0,0,0,0);
    if(edModelsEqual(&flat,model)||undoCount!=history+1||!historyStep(0)||!edModelsEqual(&flat,model)||!historyStep(1))return 0;
    EdModel sculpted;edCopyModel(&sculpted,model);
    history=undoCount;terrainControl(origin,direction,1,0,0,0,.1,1);terrainControl(origin,direction,1,0,0,0,.1,1);terrainControl(origin,direction,0,0,0,0,0,1);
    if(edModelsEqual(model,&sculpted)||undoCount!=history+1||!historyStep(0)||!edModelsEqual(model,&sculpted))return 0;

    if(!terrainConvert(asset,1)||model->assets[asset].terrain||!historyStep(0)||!edModelsEqual(&sculpted,model))return 0;
    double oldIntensity=terrainIntensity;terrainControl(origin,direction,0,0,0,1,0,1);if(terrainIntensity<=oldIntensity)return 0;
    terrainControl(origin,direction,0,0,0,-1,0,1);if(fabs(terrainIntensity-oldIntensity)>1e-9)return 0;
    double radius=terrainRadius;terrainControl(origin,direction,0,0,0,1,0);if(terrainRadius!=radius||!terrainTextureMode)return 0;terrainControl(origin,direction,0,0,0,-1,0);if(terrainTextureMode)return 0;
    terrainControl(origin,direction,0,0,1,1,0);if(terrainRadius<=radius)return 0;terrainRadius=radius;
    history=undoCount;terrainControl(origin,direction,0,0,0,0,0,0,1);
    terrainControl(edAdd(origin,edV(.125,0,0)),direction,0,0,0,0,.1,0,1);
    terrainControl(origin,direction,0,0,0,0,0,0,0);
    if(edModelsEqual(model,&sculpted)||undoCount!=history+1||!historyStep(0)||!edModelsEqual(model,&sculpted))return 0;
    int objects=model->objectCount;if(!terrainConvert(asset,0)||model->objectCount!=objects-1||!historyStep(0)||!edModelsEqual(&sculpted,model))return 0;
    if(!terrainDelete(asset)||model->assetCount||model->meshCount||model->objectCount!=objects-1||!historyStep(0)||!edModelsEqual(&sculpted,model))return 0;
    if(getenv("GEOSITH_TERRAIN_GROUP_TEST")){
        edDefault(candidate);if(!edExtrude(candidate,0,4,edV(0,1,0))||!commit())return 0;clearHistory();picks.count=0;
        for(int c=0;c<model->count;c++)for(int f=0;f<model->c[c].count;f++)if(edNormal(&model->c[c].f[f]).z>.99)picks.hits[picks.count++]={c,f,-1,-1};
        if(!terrainCreate()||model->assetCount!=2||model->objectCount!=2)return 0;
        for(int o=0;o<2;o++)if(objectSector(model,model->objects[o].position)!=o)return 0;
        EdModel grouped;edCopyModel(&grouped,model);
        if(!terrainConvert(0,1)||model->assets[0].terrain||model->assets[1].terrain||!historyStep(0)||!edModelsEqual(model,&grouped))return 0;
        if(!terrainDelete(1)||model->assetCount||model->meshCount||model->objectCount||!historyStep(0)||!edModelsEqual(model,&grouped))return 0;
        if(!terrainPaintSelfTest(0))return 0;
        return writeEditorGob(model,"Episode/sithed_terrain_group_test.gob","Terrain group test",0);
    }
    if(!terrainPaintSelfTest(asset))return 0;
    return writeEditorGob(model,"Episode/sithed_terrain_test.gob","Terrain integration test",0);
}
