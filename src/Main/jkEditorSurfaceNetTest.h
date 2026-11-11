
static unsigned surfaceTestHash(){const EdFace &f=model->c[0].f[0];return f.decalId?edNetHash(model->decals[f.decalId-1].data->data(),model->decals[f.decalId-1].data->size()):1;}
static int surfaceTestPaint(int count){
    EdPaintImage paint;paint.width=paint.height=1;paint.rgba={255,0,0,255};
    for(int i=0;i<count;i++){EdPaintImage base;const EdFace &f=model->c[0].f[0];if(f.decalId){if(!edDecalImage(model,f.decalId,&base))return 0;}else{base.width=base.height=1;base.rgba={0,255,0,255};}
        EdVec center={};for(int v=0;v<f.count;v++)center=edAdd(center,f.v[v].p);center=edScale(center,1.0/f.count);
        remember();edCopyModel(candidate,model);if(!edPlaceDecal(candidate,&candidate->c[0].f[0],center,.7,&base,&paint,.1)||!commit())return 0;
    }return 1;
}
#define SURFACE_CHECK(x) do{if(!(x)){printf("Surface session FAIL role=%d line=%d: %s\n",netTestRole,__LINE__,#x);fflush(stdout);exit(2);}}while(0)
static int stressSurfaceSession(){
    static int phase,hostUndos;static unsigned local;active=1;tool=2;EdHit hit={};hit.cell=hit.face=0;hit.vertex=hit.edge=-1;
    EditorNetPeer *a=netPeer(2,0),*b=netPeer(3,0);
    if(netTestRole==1&&!phase&&a&&b&&a->ready==(int)netRevision&&b->ready==(int)netRevision){netTestMark("surface.ready",1);phase=1;}
    if(netTestRole==2&&!phase&&netTestMarker("surface.ready")&&surfaceSessionBegin(hit)){
        unsigned revision=netRevision;SURFACE_CHECK(surfaceTestPaint(12));local=surfaceTestHash();SURFACE_CHECK(local!=1&&!netQueued&&!netBusy&&netRevision==revision);netTestMark("surface.private",local);phase=1;
    }
    if(netTestRole==3&&!phase&&netTestMarker("surface.private")&&surfaceSessionOwner(0,0)&&surfaceSessionOwner(0,0)!=stdComm_dplayIdSelf){
        SURFACE_CHECK(!surfaceSessionBegin(hit)&&surfaceTestHash()==1);SURFACE_CHECK(jkEditor_SurfaceLocked(&sithWorld_g_pCurrentWorld->aSectors[0].surfaces[0]));
        SURFACE_CHECK(!surfaceTestPaint(1));netTestMark("surface.blocked",1);phase=1;
    }
    if(netTestRole==1&&phase==1&&netTestMarker("surface.blocked")){
        SURFACE_CHECK(surfaceTestHash()==1);remember();edCopyModel(candidate,model);candidate->header.gravity=4.25;SURFACE_CHECK(commit());hostUndos=undoCount;phase=2;
    }
    if(netTestRole==2&&phase==1&&model->header.gravity==4.25){SURFACE_CHECK(surfaceTestHash()==local&&jkEditor_HandleEscape()&&terrainSessionFinishing);phase=2;}
    if(phase&&model->header.gravity==4.25&&surfaceTestHash()!=1&&!terrainSessionOpen()&&!netEditPending()){
        if(netTestRole==1)SURFACE_CHECK(undoCount==hostUndos+1);char marker[40];snprintf(marker,40,"surface.shared%d",netTestRole);netTestMark(marker,surfaceTestHash());
    }
    if(netTestRole==2&&phase==2&&netTestMarker("surface.shared1")&&netTestMarker("surface.shared2")&&netTestMarker("surface.shared3")){SURFACE_CHECK(historyStep(0));phase=3;}
    if(netTestMarker("surface.shared1")&&surfaceTestHash()==1&&!netEditPending()&&model->header.gravity==4.25){char marker[40];snprintf(marker,40,"surface.undo%d",netTestRole);netTestMark(marker,1);}
    if(netTestMarker("surface.undo1")&&netTestMarker("surface.undo2")&&netTestMarker("surface.undo3")){
        if(netTestRole==1&&phase==2&&surfaceSessionBegin(hit)){
            unsigned revision=netRevision;hostUndos=undoCount;SURFACE_CHECK(surfaceTestPaint(8));
            uint32_t length=0;unsigned char *document=netDocument(model,&length);auto published=std::make_unique<EdModel>();
            SURFACE_CHECK(document&&edWireDecode(published.get(),document+length-edNetLoad32(document+4),edNetLoad32(document+4))&&edTerrainSame(published.get(),terrainSessionBase,terrainSessionKey)&&netRevision==revision);edFree(document);netTestMark("surface.hostprivate",1);phase=4;
        }
        if(netTestRole!=1&&netTestMarker("surface.hostprivate")&&surfaceSessionOwner(0,0)==1){SURFACE_CHECK(surfaceTestHash()==1&&!surfaceSessionBegin(hit));char marker[40];snprintf(marker,40,"surface.hostblocked%d",netTestRole);netTestMark(marker,1);}
        if(netTestRole==1&&phase==4&&netTestMarker("surface.hostblocked2")&&netTestMarker("surface.hostblocked3")){SURFACE_CHECK(terrainSessionFinish()&&undoCount==hostUndos+1);phase=5;}
        if(netTestMarker("surface.hostprivate")&&!terrainSessionOpen()&&!netEditPending()&&surfaceTestHash()!=1){char marker[40];snprintf(marker,40,"surface.hostshared%d",netTestRole);netTestMark(marker,surfaceTestHash());}
        if(netTestMarker("surface.hostshared1")&&netTestMarker("surface.hostshared2")&&netTestMarker("surface.hostshared3")){puts("GEOSITH_NET_SURFACE_SESSION PASS: client/host deferred painting, exclusive locks, concurrent edits, single undo, hidden join preview");fflush(stdout);exit(0);}
    }
    SURFACE_CHECK(stdPlatform_GetTimeMsec()-netTestStart<120000);sithNet_isMulti=0;sithMessage_g_outputstream&=~1;sithMessage_g_inputstream&=~1;return 1;
}
#undef SURFACE_CHECK
