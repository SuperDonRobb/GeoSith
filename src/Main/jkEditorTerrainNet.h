// Host-owned leases are separate from the level document and undo history.
static void terrainSessionReset(void){edFree(terrainSessionBase);terrainSessionBase=NULL;terrainSessionKey.clear();terrainPendingSlot=-1;terrainSessionFinishing=terrainLockPending=0;terrainLockToken=terrainLockTime=terrainAcceptedRevision=0;}
static int terrainSessionOpen(void){return terrainSessionBase||terrainLockPending;}
static void terrainLeaseBroadcast(const std::string &key,int player){
    const TerrainLease &lease=terrainLeases[key];char name[32]={};snprintf(name,sizeof(name),"%s",key.c_str());
    EditorNetHeader h={};h.kind=EN_TERRAIN_LOCK_STATE;h.id=lease.completed;h.origin=lease.owner;h.gesture=lease.token;h.revision=lease.revision;h.length=32;
    if(player)netSend(player,h,name);else for(auto &peer:netPeers)if(peer.player)netSend(peer.player,h,name);
}
static void terrainLeaseRelease(const std::string &key,int completed){auto it=terrainLeases.find(key);if(it==terrainLeases.end())return;it->second.completed=completed?netRevision:0;it->second.owner=0;it->second.revision=++terrainLeaseRevision;terrainLeaseBroadcast(key);}
static int terrainSessionRemote(int asset){if(!netEnabled())return 0;auto it=terrainLeases.find(edTerrainKey(model,asset));return it!=terrainLeases.end()&&it->second.owner&&it->second.owner!=stdComm_dplayIdSelf;}
static int terrainSessionOwned(int asset){
    if(!netEnabled()||asset<0||asset>=model->assetCount||!model->assets[asset].terrain)return 0;
    auto it=terrainLeases.find(edTerrainKey(model,asset));return it!=terrainLeases.end()&&it->second.owner==stdComm_dplayIdSelf&&it->second.owner;
}
int jkEditor_TerrainLockedThing(const SithThing *thing){
    if(!active||!netEnabled()||terrainLeases.empty()||!model||!thing||!thing->pTemplate)return 0;
    for(int a=0;a<model->assetCount;a++)if(model->assets[a].terrain&&!strcmp(thing->pTemplate->aName,model->assets[a].name)&&terrainSessionRemote(a))return 1;
    return 0;
}
static int terrainSessionGuard(const EdModel *before,const EdModel *after,int owner){
    if(!netEnabled())return 1;
    for(auto &entry:terrainLeases)if(entry.second.owner&&entry.second.owner!=owner&&!edTerrainSame(before,after,entry.first))return 0;
    return 1;
}
static int terrainSessionLocalCommit(void){
    if(!netEnabled()||!terrainSessionBase||terrainSessionFinishing)return -1;
    // Only sculpt/paint/slot changes belong to a deferred terrain session.
    EdModel check=*candidate;if(!edTerrainOverlay(&check,model,terrainSessionKey)||!edModelsEqual(&check,model))return -1;
    if(!edValidate(candidate))return 0;
    pendingUndo=0;return netApplyLocal(candidate);
}
static int editingSessionBegin(const std::string &key){
    if(!netEnabled())return 1;
    if(key.empty())return 0;
    if(terrainSessionOpen()&&key!=terrainSessionKey){terrainSessionFinish();return 0;}
    if(terrainSessionFinishing||netEditPending())return 0;
    if(terrainSessionBase)return 1;
    auto it=terrainLeases.find(key);
    if(it!=terrainLeases.end()&&it->second.owner&&it->second.owner!=stdComm_dplayIdSelf){say("This surface or terrain is locked by another player.");return 0;}
    if(!terrainLockPending){terrainSessionKey=key;terrainLockToken=++netSequence;terrainLockPending=1;terrainLockTime=0;}
    if(sithNet_isServer){auto &lease=terrainLeases[key];lease.owner=stdComm_dplayIdSelf;lease.token=terrainLockToken;lease.completed=0;lease.revision=++terrainLeaseRevision;terrainLeaseBroadcast(key);}
    it=terrainLeases.find(key);
    if(it!=terrainLeases.end()&&it->second.owner==stdComm_dplayIdSelf&&it->second.token==terrainLockToken){
        terrainSessionBase=edAlloc(sizeof(*terrainSessionBase));if(!terrainSessionBase)return 0;edCopyModel(terrainSessionBase,model);terrainLockPending=0;
        say("Editing locked to you. Esc or switch weapons to finish and send.");return 1;
    }
    say("Requesting edit lock from host...");return 0;
}
static int terrainSessionBegin(int asset){return editingSessionBegin(edTerrainKey(model,asset));}
static int surfaceSessionBegin(EdHit hit){
    if(!netEnabled())return 1;
    if(hit.cell<0||hit.cell>=model->count||hit.face<0||hit.face>=model->c[hit.cell].count)return 0;
    return editingSessionBegin(edSurfaceKey(model->c[hit.cell].f[hit.face]));
}
static int surfaceSessionOwner(int cell,int face){
    if(!netEnabled()||!model||terrainLeases.empty()||cell<0||cell>=model->count||face<0||face>=model->c[cell].count)return 0;
    auto it=terrainLeases.find(edSurfaceKey(model->c[cell].f[face]));return it==terrainLeases.end()?0:it->second.owner;
}
static int surfaceSessionOwnedHit(EdHit *hit){
    if(!netEnabled()||!edSurfaceSessionKey(terrainSessionKey))return 0;
    auto it=terrainLeases.find(terrainSessionKey);if(it==terrainLeases.end()||it->second.owner!=stdComm_dplayIdSelf)return 0;
    return edSurfaceFind(model,terrainSessionKey,&hit->cell,&hit->face);
}
int jkEditor_SurfaceLocked(const SithSurface *surface){
    if(!active||!surface||!surface->pSector)return 0;int owner=surfaceSessionOwner(surface->pSector->id,(int)(surface-surface->pSector->surfaces));return owner&&owner!=stdComm_dplayIdSelf;
}
static bool editingSessionExists(const std::string &key){return edSurfaceSessionKey(key)?edSurfaceExists(model,key):edTerrainNamed(model,key)>=0;}
static int terrainSessionFinish(void){
    if(!terrainSessionOpen()||terrainSessionFinishing)return 0;
    terrainPendingSlot=-1;terrainStroke=grabbing=0;terrainElapsed=0;
    if(!terrainSessionBase||edTerrainSame(terrainSessionBase,model,terrainSessionKey)){
        if(sithNet_isServer)terrainLeaseRelease(terrainSessionKey);
        else{char name[32]={};snprintf(name,32,"%s",terrainSessionKey.c_str());netSend(sithNet_serverNetId,EditorNetHeader{.kind=EN_TERRAIN_RELEASE,.gesture=terrainLockToken,.length=32},name);}
        // Keep cancellation retrying until the host echoes the unlocked lease.
        if(netClient()){terrainSessionFinishing=2;return 1;}terrainSessionReset();return 1;
    }
    terrainSessionFinishing=1;
    if(sithNet_isServer){
        EdModel finished=*model,previous=*model;edTerrainOverlay(&previous,terrainSessionBase,terrainSessionKey);
        // commit must see the published terrain so its single undo covers all strokes.
        *model=previous;edCopyModel(undoModel,&previous);undoPicks=picks;pendingUndo=1;edCopyModel(candidate,&finished);
        netApplying=1;int ok=commit();netApplying=0;
        if(!ok){*model=std::move(finished);terrainSessionFinishing=0;return 0;}
        netRevision++;netBroadcast(stdComm_dplayIdSelf,terrainLockToken);terrainLeaseRelease(terrainSessionKey,1);terrainSessionReset();say("Edit finished and shared. Ctrl+Z: undo.");return 1;
    }
    netQueued=edAlloc(sizeof(*netQueued));if(!netQueued){terrainSessionFinishing=0;return 0;}edCopyModel(netQueued,model);netQueuedGesture=terrainLockToken;
    say("Sending completed edit to host...");return 1;
}
static int terrainLeaseMessage(int sender,const EditorNetHeader &h,const unsigned char *data){
    if(h.length!=32||!netFilename((const char*)data,32))return 0;std::string key((const char*)data);
    if(h.kind==EN_TERRAIN_LOCK_STATE){
        if(sithNet_isServer||sender!=sithNet_serverNetId)return 0;
        auto &lease=terrainLeases[key];if(h.revision<lease.revision)return 1;lease={int(h.origin),h.gesture,h.revision,h.id};
        if(terrainSessionFinishing==1&&key==terrainSessionKey&&h.gesture==terrainLockToken&&!lease.owner&&h.id){terrainAcceptedRevision=h.id;
            if(netRevision>=h.id&&netConfirmed){netApplyLocal(netConfirmed);EditorNetPeer *peer=netPeer(sithNet_serverNetId,0);if(peer){netFreeStream(&peer->tx);edFree(peer->outBase);peer->outBase=NULL;peer->outSize=0;}edFree(netQueued);netQueued=NULL;netBusy=0;terrainSessionReset();say("Host accepted the completed edit.");}
        }
        if(terrainSessionFinishing==2&&key==terrainSessionKey&&!lease.owner)terrainSessionReset();
        return 1;
    }
    if(!sithNet_isServer||(int)sithPlayer_GetPlayerNum(sender)<0)return 0;
    if(!editingSessionExists(key)&&terrainLeases.find(key)==terrainLeases.end())return 0;
    auto &lease=terrainLeases[key];
    if(h.kind==EN_TERRAIN_RELEASE){terrainCancelled[sender]=(std::max)(terrainCancelled[sender],h.gesture);if(lease.owner==sender&&lease.token==h.gesture)terrainLeaseRelease(key);else terrainLeaseBroadcast(key,sender);return 1;}
    if(!editingSessionExists(key)||h.gesture<=terrainCancelled[sender]){terrainLeaseBroadcast(key,sender);return 0;}
    if(!lease.owner){for(auto &entry:terrainLeases)if(entry.second.owner==sender){terrainLeaseBroadcast(key,sender);return 1;}
        lease.owner=sender;lease.token=h.gesture;lease.completed=0;lease.revision=++terrainLeaseRevision;terrainLeaseBroadcast(key);
    }else terrainLeaseBroadcast(key,sender);
    return 1;
}
static void terrainLeaseTick(uint32_t now){
    if(sithNet_isServer){if(now-terrainLeaseTime>1000){terrainLeaseTime=now;for(auto &entry:terrainLeases)terrainLeaseBroadcast(entry.first);}}
    else if((terrainLockPending||terrainSessionFinishing==2)&&(!terrainLockTime||now-terrainLockTime>500)){
        terrainLockTime=now;char name[32]={};snprintf(name,32,"%s",terrainSessionKey.c_str());
        netSend(sithNet_serverNetId,EditorNetHeader{.kind=(uint32_t)(terrainSessionFinishing==2?EN_TERRAIN_RELEASE:EN_TERRAIN_LOCK),.gesture=terrainLockToken,.length=32},name);
    }
}
