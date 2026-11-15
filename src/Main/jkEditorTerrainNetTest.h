static unsigned terrainTestHash(int asset){EdModel part=edTerrainPart(model,edTerrainKey(model,asset));unsigned n=0;unsigned char *bytes=edWireEncode(&part,&n);if(!bytes){printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}unsigned hash=edNetHash(bytes,n);edFree(bytes);return hash;}
static int stressTerrainSession(){
    static int phase,hostUndos,created;static unsigned baseline,local;
    if(getenv("GEOSITH_NET_CLIENT_TERRAIN_TEST")&&!netTestMarker("terrain.created")){
        active=1;tool=7;
        if(netTestRole==2&&!created&&!netEditPending()){
            for(int f=0;f<model->c[0].count;f++)if(model->c[0].f[f].neighbor<0&&edNormal(&model->c[0].f[f]).z>.9){picks.count=1;picks.hits[0]={0,f,-1,-1};if(!terrainCreate())exit(2);
                // The second click arrives before creation approval. Releasing
                // it must not lose the request, nor submit another document.
                if(!netEditPending())exit(2);terrainSessionQueueBegin(terrainSelected);
                if(!terrainSessionOpen()||!terrainSessionFinish()||terrainSessionOpen())exit(2);
                terrainSessionQueueBegin(terrainSelected);fireDown=1;created=1;break;}
        }else if(netTestRole==2&&created&&!netEditPending())netTestMark("terrain.created",1);
        sithNet_isMulti=0;sithMessage_g_outputstream&=~1;sithMessage_g_inputstream&=~1;return 1;
    }
    active=1;tool=7;int asset=-1;for(int a=0;a<model->assetCount;a++)if(model->assets[a].terrain){asset=a;break;}if(asset<0&&getenv("GEOSITH_NET_CLIENT_TERRAIN_TEST")){sithNet_isMulti=0;sithMessage_g_outputstream&=~1;sithMessage_g_inputstream&=~1;return 1;}if(asset<0){printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}
    if(!baseline)baseline=terrainTestHash(asset);
    EdVec point={};for(int i=0;i<model->meshCount;i++)if(model->meshes[i].asset==asset){point=model->meshes[i].geometry.f[0].v[0].p;break;}
    EditorNetPeer *a=netPeer(2,0),*b=netPeer(3,0);
    if(netTestRole==1&&a&&b&&a->ready==(int)netRevision&&b->ready==(int)netRevision&&!phase){netTestMark("terrain.ready",1);phase=1;}
    if(netTestRole==2&&netTestMarker("terrain.ready")&&!phase&&(getenv("GEOSITH_NET_CLIENT_TERRAIN_TEST")?(terrainSessionBase!=NULL):terrainSessionBegin(asset))){
        if(!terrainSessionOwned(asset)||terrainSessionRemote(asset))exit(2);
        if(getenv("GEOSITH_NET_CLIENT_TERRAIN_TEST")){
            if(fireDown||grabDown||!terrainQueuedLockKey.empty()||netEditPending())exit(2);
            puts("CLIENT TERRAIN PASS: pending second click acquired lock without a third click or extra proposal; cancellation cleared queued lock");
            SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;netControlReady(player);
            if(sithInventory_GetInventory(player,SITHBIN_RAILCHARGES)<=0)exit(2);
            const EdFace *face=NULL;for(int i=0;i<model->meshCount;i++)if(model->meshes[i].asset==asset){face=&model->meshes[i].geometry.f[0];break;}
            EdVec center=edScale(edAdd(edAdd(face->v[0].p,face->v[1].p),face->v[2].p),1.0/3),normal=edNormal(face);
            EdVec origin=edAdd(center,edScale(normal,.1)),direction=edScale(normal,-1);EdHit hit;
            if(terrainPick(origin,direction,&hit)!=asset){puts("CLIENT TERRAIN FAIL: new terrain cannot be picked");exit(2);}
            fireDown=grabDown=terrainStroke=grabbing=0;terrainTextureMode=0;
            for(int i=0;i<6;i++)terrainControl(origin,direction,1,0,0,0,.05);
            terrainControl(origin,direction,0,0,0,0,.05);
            if(terrainTestHash(asset)==baseline){puts("CLIENT TERRAIN FAIL: brush input did not sculpt");exit(2);}
            puts("CLIENT TERRAIN PASS: tools initialized and new terrain picked/sculpted before any host sculpt");
        }
        unsigned revision=netRevision;for(int i=0;i<12;i++){remember();edCopyModel(candidate,model);if(!edTerrainSculpt(candidate,asset,point,.5,.002,edV(0,0,1))||!commit()){printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}}
        edCopyModel(candidate,model);if(!edTerrainPaint(candidate,asset,point,.5,.3,"dflt.mat",terrainMaterialName)||!commit()){printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}
        local=terrainTestHash(asset);if(local==baseline||netQueued||netBusy||netRevision!=revision){printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}
        netTestMark("terrain.private",local);phase=1;
    }
    if(netTestRole==3&&netTestMarker("terrain.private")&&!phase&&terrainSessionRemote(asset)){
        if(terrainSessionOwned(asset)||terrainSessionBegin(asset)||terrainTestHash(asset)!=baseline){printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}
        edCopyModel(candidate,model);if(!edTerrainSculpt(candidate,asset,point,.5,.002,edV(0,0,1))||commit()){printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}
        netTestMark("terrain.blocked",1);phase=1;
    }
    if(netTestRole==1&&phase==1&&netTestMarker("terrain.private")&&netTestMarker("terrain.blocked")){
        if(terrainTestHash(asset)!=baseline){printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}
        remember();edCopyModel(candidate,model);candidate->header.gravity=4.25;if(!commit()){printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}hostUndos=undoCount;netTestMark("terrain.concurrent",1);phase=2;
    }
    if(netTestRole==2&&phase==1&&model->header.gravity==4.25){
        if(terrainTestHash(asset)!=local||!jkEditor_HandleEscape()||!terrainSessionFinishing){printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}phase=2;
    }
    if(phase>=1&&model->header.gravity==4.25&&terrainTestHash(asset)!=baseline&&!terrainSessionOpen()&&!netEditPending()){
        if(netTestRole==1&&undoCount!=hostUndos+1){printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}
        char marker[40];snprintf(marker,40,"terrain.shared%d",netTestRole);netTestMark(marker,terrainTestHash(asset));
    }
    if(netTestRole==2&&phase==2&&netTestMarker("terrain.shared1")&&netTestMarker("terrain.shared2")&&netTestMarker("terrain.shared3")){if(!historyStep(0)){printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}phase=3;}
    if(netTestMarker("terrain.shared1")&&terrainTestHash(asset)==baseline&&!netEditPending()&&model->header.gravity==4.25){char marker[40];snprintf(marker,40,"terrain.undo%d",netTestRole);netTestMark(marker,baseline);}
    if(netTestMarker("terrain.undo1")&&netTestMarker("terrain.undo2")&&netTestMarker("terrain.undo3")){
        if(netTestRole==1&&phase==2&&terrainSessionBegin(asset)){
            if(!terrainSessionOwned(asset)||terrainSessionRemote(asset))exit(2);
            unsigned revision=netRevision;hostUndos=undoCount;
            for(int i=0;i<8;i++){remember();edCopyModel(candidate,model);if(!edTerrainSculpt(candidate,asset,point,.5,.003,edV(0,0,1))||!terrainSculptCommit())exit(2);}
            terrainRefreshPaint();
            uint32_t length=0;unsigned char *document=netDocument(model,&length);EdModel published;
            if(!document||!edWireDecode(&published,document+length-edNetLoad32(document+4),edNetLoad32(document+4))||!edTerrainSame(&published,terrainSessionBase,terrainSessionKey)||netRevision!=revision)exit(2);edFree(document);
            netTestMark("terrain.hostprivate",1);phase=4;
        }
        if(netTestRole!=1&&netTestMarker("terrain.hostprivate")&&terrainSessionRemote(asset)){
            if(terrainTestHash(asset)!=baseline||terrainSessionBegin(asset))exit(2);
            char marker[40];snprintf(marker,40,"terrain.hostblocked%d",netTestRole);netTestMark(marker,1);
        }
        if(netTestRole==1&&phase==4&&netTestMarker("terrain.hostblocked2")&&netTestMarker("terrain.hostblocked3")){if(!terrainSessionFinish()||undoCount!=hostUndos+1)exit(2);phase=5;}
        if(netTestMarker("terrain.hostprivate")&&!terrainSessionOpen()&&!netEditPending()&&terrainTestHash(asset)!=baseline){char marker[40];snprintf(marker,40,"terrain.hostshared%d",netTestRole);netTestMark(marker,terrainTestHash(asset));}
        if(netTestMarker("terrain.hostshared1")&&netTestMarker("terrain.hostshared2")&&netTestMarker("terrain.hostshared3")){
            if(netTestRole==1){auto key=edTerrainKey(model,asset);terrainLeases[key].owner=3;jkEditor_NetPlayerLeft(3);if(terrainLeases[key].owner)exit(2);}
            puts("GEOSITH_NET_TERRAIN_SESSION PASS: client and host private edits, exclusive leases, unrelated edits preserved, one publication/undo, join snapshot isolation, disconnect cleanup");fflush(stdout);exit(0);
        }
    }
    if(stdPlatform_GetTimeMsec()-netTestStart>120000){printf("TERRAIN SESSION FAIL role=%d phase=%d session=%d pending=%d revision=%u\n",netTestRole,phase,terrainSessionOpen(),netEditPending(),netRevision);fflush(stdout);{printf("Terrain session assertion failed at line %d, role %d\n",__LINE__,netTestRole);fflush(stdout);exit(2);}}
    sithNet_isMulti=0;sithMessage_g_outputstream&=~1;sithMessage_g_inputstream&=~1;return 1;
}
