/* Opt-in, three-process test over the actual game transport. Marker files only
 * coordinate assertions; every level, edit and undo travels as a game packet. */
extern "C" {
#include "Gui/jkGUIMultiplayer.h"
#include "Dss/sithDSSThing.h"
#include "Dss/jkDSS.h"
#include "Devices/sithSoundMixer.h"
extern char jkGuiMultiplayer_ipText_conv[256];
extern int jkGuiNetHost_portNum;
}
#include "Dss/sithDSSSoundIdentity.h"
static unsigned netTestSoundMask;
static int netTestSoundMessage(SithMessage *message){
    unsigned flags=0,ref=0;memcpy(&flags,message->pktData,4);
    unsigned size=18+((flags&SITHSOUNDFLAG_FOLLOWSTHING)?4:((flags&SITHSOUNDFLAG_ABSOLUTE)?12:0));
    if(message->netMsg.msg_size==size+32){
        memcpy(&ref,(const unsigned char*)message->pktData+size-4,4);
        if(ref>=0x67891230u&&ref<=0x67891232u){
            unsigned short index;memcpy(&index,(const unsigned char*)message->pktData+12,2);
            sithSound *sound=sithDSSSound_Resolve(message,size,index);
            if(!sound||strcmp(sound->sound_fname,"walkmetaldry02.wav")||sound==sithSound_GetFromIdx(index))exit(2);
            if(!sithDSSThing_ProcessPlaySound(message))exit(2);
            sithPlayingSound *channel=sithSoundMixer_GetChannelHandle(ref);
            if(channel&&channel->sound!=sound)exit(2);
            netTestSoundMask|=1u<<(ref-0x67891230u);printf("SOUND TEST play mask=%u\n",netTestSoundMask);fflush(stdout);return 1;
        }
    }
    return sithDSSThing_ProcessPlaySound(message);
}
static int netTestFireSoundMessage(SithMessage *message){
    unsigned ref=0;if(message->netMsg.msg_size==80)memcpy(&ref,(const unsigned char*)message->pktData+44,4);
    if(ref==0x67891233u){
        unsigned short index;memcpy(&index,(const unsigned char*)message->pktData+8,2);
        sithSound *sound=sithDSSSound_Resolve(message,48,index);
        if(!sound||strcmp(sound->sound_fname,"walkmetaldry02.wav")||sound==sithSound_GetFromIdx(index))exit(2);
        if(!sithDSSThing_ProcessFire(message))exit(2);
        netTestSoundMask|=8;printf("SOUND TEST fire mask=%u\n",netTestSoundMask);fflush(stdout);return 1;
    }
    return sithDSSThing_ProcessFire(message);
}
static uint32_t netTestModelHash(void){uint32_t size=0;unsigned char *bytes=edWireEncode(model,&size);uint32_t hash=bytes?edNetHash(bytes,size):0;edFree(bytes);return hash;}
static int netTestRole,netTestStage,netTestPackets;
static uint32_t netTestStart;
static uint32_t netTestJoinTime,netTestPulseTime,netTestPositionTime;
static int netTestJoinPulses;
static int netTestRollbackSeen;
static unsigned int netTestLockedRejections;
static uint32_t netTestEnemyTime;
static int netTestEnemyStage;
static int netTestMessage(SithMessage *message){
    EditorNetHeader header={0};if(message->netMsg.msg_size>=sizeof(header))edCopy(&header,message->pktData,sizeof(header));
    int result=jkEditor_NetMessage(message);
    /* The test deliberately drops packets. If the small rejection notice is
       lost, completing the authoritative rejection snapshot must also revert. */
    int rejected=header.kind==EN_REJECT||(header.kind==EN_STATE&&!header.origin&&netReady&&netRejections&&netConfirmed&&edModelsEqual(model,netConfirmed));
    if(!sithNet_isServer&&rejected){
        if(!netConfirmed||!edModelsEqual(model,netConfirmed)||netQueued||grabbing||
           sithWorld_g_pCurrentWorld->numSectors!=model->count||fabs(sithWorld_g_pCurrentWorld->gravity-model->header.gravity)>.001)exit(2);
        netTestRollbackSeen=1;printf("GEOSITH_NET_PREDICTION PASS: rejected preview rolled back immediately\n");fflush(stdout);
    }return result;
}
static void netTestPath(char *out,const char *name){snprintf(out,512,"%s/%s",getenv("GEOSITH_NET_DIR"),name);}
static int netTestMarker(const char *name){char path[512];netTestPath(path,name);FILE *f=fopen(path,"rb");if(f)fclose(f);return f!=NULL;}
static void netTestMark(const char *name,uint32_t value){char path[512],temp[520];netTestPath(path,name);if(netTestMarker(name))return;snprintf(temp,sizeof(temp),"%s.tmp",path);FILE *f=fopen(temp,"wb");if(!f)exit(2);fprintf(f,"%u\n",value);fclose(f);if(rename(temp,path))exit(2);}
static int netTestTransmit(int player,const SithMessage *m){
    static uint32_t droppedProposal;EditorNetHeader h={0};if(m->netMsg.msg_size>=sizeof(h))edCopy(&h,m->pktData,sizeof(h));EditorNetPeer *peer=netPeer(player,0);
    if(netTestRole==1&&h.kind==EN_ACK&&peer&&(peer->rx.h.kind==EN_EDIT||peer->rx.h.kind==EN_TERRAIN_EDIT)&&h.offset==peer->rx.h.total){
        if(!droppedProposal){droppedProposal=h.id;printf("GEOSITH_NET_TEST: deliberately dropping final proposal ACK and retries\n");}
        if(h.id==droppedProposal)return 1;
    }
    int n=++netTestPackets;int faultInjection=!getenv("GEOSITH_NET_NO_DROP");if(faultInjection&&n%19==3)return 1;int sent=stdComm_SendToPlayer((SithMessage*)m,player);if(faultInjection&&sent&&n%13==7)stdComm_SendToPlayer((SithMessage*)m,player);return sent;}
#include "jkEditorTerrainNetTest.h"
#include "jkEditorSurfaceNetTest.h"
static int stressNetwork(SithThing *player){
    if(!getenv("GEOSITH_NET_DIR"))exit(2);
    sithNet_isMulti=1;sithMessage_g_outputstream|=1;sithMessage_g_inputstream|=1;
    if(!netTestRole){sithMessage_RegisterFunction(DSS_PLAYSOUND,netTestSoundMessage);sithMessage_RegisterFunction(DSS_FIREPROJECTILE,netTestFireSoundMessage);Main_bVerboseNetworking=1;netTestRole=atoi(getenv("GEOSITH_NET_ROLE"));netTestStart=stdPlatform_GetTimeMsec();active=1;tool=1;savedPhysics=player->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=player->flags;
        jkEditor_NetPreparePlayers(sithWorld_g_pCurrentWorld);sithPlayer_PlacePlayers(sithWorld_g_pCurrentWorld);
        for(int i=0;i<jkPlayer_maxPlayers;i++){jkPlayer_playerInfos[i].playerNetId=0;jkPlayer_playerInfos[i].flags=2;}player=sithWorld_g_pCurrentWorld->pLocalPlayer;
        sithNet_isServer=stdComm_bIsServer=netTestRole==1;sithNet_serverNetId=1;netTestSend=netTestTransmit;sithMessage_aTypeFuncs[JKEDITOR_NET_MESSAGE]=netTestMessage;
        if(netTestRole==1){jkGuiNetHost_portNum=getenv("GEOSITH_NET_PORT")?atoi(getenv("GEOSITH_NET_PORT")):27020;jkMultiEntry entry={0};entry.maxPlayers=3;stdString_SafeWStrCopy(entry.wPassword,u"geosith-network-test",32);entry.multiModeFlags=JKEDITOR_NET_FLAG;stdString_SafeWStrCopy(entry.serverName,u"GeoSith network test",32);snprintf(entry.episodeGobName,32,"%s",getenv("GEOSITH_NET_LEVEL")?getenv("GEOSITH_NET_LEVEL"):"sithed_smoke");snprintf(entry.mapJklFname,32,"%s",sithWorld_g_pCurrentWorld->map_jkl_fname);if(stdComm_CreatePlayer(&entry)||!stdComm_GNS_TestListener())exit(2);
            sithPlayer_ShowPlayer(0,stdComm_dplayIdSelf);
            if(getenv("GEOSITH_NET_LEVEL")){active=0;sithMulti_StartupServer();}else{
            edCopyModel(candidate,model);if(!edExtrudeStep(candidate,0,4))exit(2);candidate->header.gravity=3.5;candidate->objectCount=1;candidate->objects[0]=EdObject{.position={.2,.4,.2}};snprintf(candidate->objects[0].templateName,64,"network_template");
            addThingFrame(&candidate->objects[0].attributes,EdFrame{candidate->objects[0].position,edV(0,0,0)});addThingFrame(&candidate->objects[0].attributes,EdFrame{0});candidate->lightCount=1;candidate->lights[0]=EdLight{0,{0,0,.5},1,2};
            candidate->assetCount=1;strcpy(candidate->assets[0].name,"network_model");candidate->assets[0].origin=edV(0,0,.5);candidate->assets[0].saved=1;if(!edMeshPrimitive(candidate,0,0,edV(0,0,.5),0))exit(2);
            candidate->templateCount=1;EdTemplate *created=&candidate->templates[0];strcpy(created->name,"network_template");strcpy(created->parent,"_decor");created->count=1;strcpy(created->params[0].key,"model3d");strcpy(created->params[0].value,"network_model.3do");
            strcpy(candidate->c[1].colormap,"01narsh.cmp");strcpy(candidate->c[1].layer,"Network sector");candidate->c[1].extraLight=.35;candidate->c[1].ambientLight=.1;
            for(int f=0;f<candidate->c[1].count;f++)if(edNormal(&candidate->c[1].f[f]).z>.99){EdFace floor=candidate->c[1].f[f];if(!edCleave(candidate,1,f,floor.v[0].p,floor.v[2].p))exit(2);break;}
            if(!loadPlacedCog("01_cargoelev1.cog",&candidate->cogs[0]))exit(2);candidate->cogCount=1;candidate->cogs[0].params[0].cell=0;candidate->cogs[0].params[0].face=4;snprintf(candidate->cogs[0].params[1].value,32,"1");remember();if(!commit())exit(2);
            remember();edCopyModel(candidate,model);if(!edCleaveShape(candidate,0,2,edV(0,-1,0),.25,32)||!commit())exit(2);
            remember();edCopyModel(candidate,model);edRotateFaceTexture(&candidate->c[0].f[0],37,edV(0,0,0));edRotateFaceTexture(&candidate->meshes[0].geometry.f[0],23,edV(0,0,.5));if(!commit())exit(2);
            /* Static GUIDs need not equal local allocation slots after a join. */
            jkPlayer_playerInfos[0].pLocalPlayer->guid=100001;jkPlayer_playerInfos[1].pLocalPlayer->guid=100000;sithWorld_g_pCurrentWorld->aThings[objectThings[0]].guid=1;
            }
            if(getenv("GEOSITH_NET_TERRAIN_SESSION_TEST")){for(int f=0;f<model->c[0].count;f++)if(model->c[0].f[f].neighbor<0&&edNormal(&model->c[0].f[f]).z>.9){picks.count=1;picks.hits[0]={0,f,-1,-1};if(!terrainCreate())exit(2);break;}}
            jkDSS_SendSaberInfo_alt(player,(char*)"kyb3.3do",(char*)"kymp4.snd",(char*)"saberblue1.mat",(char*)"saberblue0.mat");grabbing=objectGrabbing=1;char bootstrap[32]="";if(!jkEditor_NetPrepareJoin(bootstrap,sithWorld_g_pCurrentWorld->map_jkl_fname,JKEDITOR_NET_FLAG))exit(2);char path[512];netTestPath(path,"bootstrap.name");FILE *f=fopen(path,"wb");if(!f)exit(2);fprintf(f,"%s",bootstrap);fclose(f);netTestMark("host.ready",netRevision);
        }else{if(!stdComm_GNS_TestDiscovery())exit(2);snprintf(jkGuiMultiplayer_ipText_conv,256,"127.0.0.1:27020");if(stdComm_Open(0,u"geosith-network-test"))exit(2);sithMulti_StartupClient();sithNet_serverNetId=0;
            /* The connection screen has no local player until DSS_WELCOME.
               Use the real join request, dispatcher and welcome handler. */
            sithWorld_g_pCurrentWorld->pLocalPlayer=NULL;boundWorld=NULL;g_submodeFlags|=8;
            if(netTestRole==2){
                int oldLevel=jkMain_bLevelActive,oldModal=jkMain_bInModalGui;
                jkMain_bLevelActive=1;jkMain_bInModalGui=1;
                int joined=jkGuiMultiplayer_ShowSynchronizing();
                jkMain_bInModalGui=oldModal;jkMain_bLevelActive=oldLevel;
                if(joined!=1||(g_submodeFlags&8)){printf("GEOSITH_NET_SYNC_MENU FAIL: result %d\n",joined);fflush(stdout);exit(2);}
                printf("GEOSITH_NET_SYNC_MENU PASS: real synchronization menu survived snapshot and completed welcome\n");fflush(stdout);
            }
            while(g_submodeFlags&8){uint32_t now=stdPlatform_GetTimeMsec();
                if(getenv("GEOSITH_NET_EXPECT_SECTORS")){static uint32_t progress; if(now-progress>5000){progress=now;EditorNetPeer *p=netPeer(sithNet_serverNetId,0);printf("LARGE JOIN role=%d ready=%d rx=%u/%u\n",netTestRole,netReady,p?p->rx.at:0,p?p->rx.h.total:0);fflush(stdout);}}
                if(now-netTestStart>(getenv("GEOSITH_NET_TIMEOUT_MS")?(uint32_t)atoi(getenv("GEOSITH_NET_TIMEOUT_MS")):(getenv("GEOSITH_NET_EXPECT_SECTORS")?120000u:45000u))){printf("GEOSITH_NET_JOIN FAIL: welcome timeout role %d ready %d server %d\n",netTestRole,netReady,sithNet_serverNetId);fflush(stdout);exit(2);}
                if(now-netTestJoinTime>500){netTestJoinTime=now;if(!sithMulti_SendJoinRequest(sithNet_serverNetId))exit(2);}
                sithMessage_ProcessMessages();
            }
            if(!getenv("GEOSITH_NET_LEVEL")){if(fabs(model->c[0].f[0].uvRotation-37)>.001||fabs(model->meshes[0].geometry.f[0].uvRotation-23)>.001)exit(2);printf("GEOSITH_NET_TEXTURE_ROTATION PASS: world and model texture rotation synchronized\n");}
            if(!netReady||!sithWorld_g_pCurrentWorld->pLocalPlayer||sithMulti_quitGameState==2)exit(2);
            SithThing *custom=sithTemplate_GetTemplate("network_template");if(!getenv("GEOSITH_NET_LEVEL")&&((!getenv("GEOSITH_NET_TERRAIN_SESSION_TEST")&&model->templateCount!=1)||!custom||custom->renderData.type!=RD_THING_MODEL3||!custom->renderData.model3||objectThings[0]<0))exit(2);
            if(getenv("GEOSITH_CUSTOM_RESOURCES_TEST")){
                if(customResources.files.size()!=3||!customMaterial("zz_net_custom.mat"))exit(2);
                int material=addMaterialName("zz_net_custom.mat");if(material<0||!editorMaterial(material))exit(2);
                materialMetadata();if(materialSection[material]!=103)exit(2);
                if(!sithModel_Load("zz_net_model.3do",1))exit(2);
                discoverCogs();int script=0;for(int i=0;i<cogSourceCount;i++)script|=!strcmp(cogSources[i].file,"zz_net_script.cog");if(!script)exit(2);
                char *text=cogSourceText("zz_net_script.cog");if(!text||!strstr(text,"GeoSith resource fixture"))exit(2);edFree(text);
                char path[512],name[32];snprintf(name,sizeof(name),"assets-%d.gob",netTestRole);netTestPath(path,name);
                if(!writeEditorGob(model,path,"Resources",1))exit(2);
                EdResources reloaded;reloaded.archive(path);for(auto &f:customResources.files){auto it=reloaded.files.find(f.first);if(it==reloaded.files.end()||*it->second!=*f.second)exit(2);}
                puts("GEOSITH_NET_RESOURCES PASS: host-only MAT/3DO/COG received, loaded, browsed and saved in GOB");fflush(stdout);
            }
            if(!getenv("GEOSITH_NET_LEVEL")&&(strcmp(model->c[1].colormap,"01narsh.cmp")||strcmp(model->c[1].layer,"Network sector")||sithWorld_g_pCurrentWorld->numColormaps!=2||sithWorld_g_pCurrentWorld->aSectors[1].colormap!=&sithWorld_g_pCurrentWorld->colormaps[1]))exit(2);
            printf("GEOSITH_NET_TEMPLATE PASS: custom model/template and placed instance replicated on join\n");fflush(stdout);
            printf("GEOSITH_NET_JOIN PASS: role %d synchronized before welcome and assigned local player %d\n",netTestRole,playerThingIdx);fflush(stdout);
            SithThing *hostBody=jkPlayer_playerInfos[0].pLocalPlayer;
            printf("GEOSITH_NET_VISIBILITY: role=%d guid=%d flags=%x geo=%d sector=%d model=%p\n",netTestRole,hostBody->guid,hostBody->flags,hostBody->renderData.curGeoMode,hostBody->sector?hostBody->sector->id:-1,(void*)hostBody->renderData.model3);fflush(stdout);
            if((hostBody->flags&SITH_TF_DISABLED)||!hostBody->renderData.curGeoMode||!hostBody->renderData.model3||!hostBody->sector){puts("GEOSITH_NET_VISIBILITY FAIL: host is hidden after welcome");fflush(stdout);exit(2);}
        }
        if(netTestRole!=1){jkPlayer_Startup();jkPlayer_InitSaber();jkDSS_SendSaberInfo_alt(sithWorld_g_pCurrentWorld->pLocalPlayer,(char*)"kyb3.3do",(char*)"kymp4.snd",(char*)"saberblue1.mat",(char*)"saberblue0.mat");}
        printf("NETTEST role %d initialized\n",netTestRole);fflush(stdout);
    }
    sithNet_isServer=netTestRole==1;if(netTestRole==1)sithNet_serverNetId=1;
    if(netTestRole==1){int oldLevel=jkMain_bLevelActive,oldModal=jkMain_bInModalGui;jkMain_bLevelActive=1;jkMain_bInModalGui=1;jkMain_NetworkTick();jkMain_bInModalGui=oldModal;jkMain_bLevelActive=oldLevel;}else sithMessage_ProcessMessages();
    if(getenv("GEOSITH_NET_EDITOR_LOOK_TEST")){
        static int checked;
        if(!checked){
            if(!active)exit(2);
            if(netTestRole!=1){netControlReady(sithWorld_g_pCurrentWorld->pLocalPlayer);if(!editorLookSelfTest(sithWorld_g_pCurrentWorld->pLocalPlayer))exit(2);}
            char marker[32];snprintf(marker,32,"look.ready%d",netTestRole);netTestMark(marker,1);checked=1;
        }
        if(netTestMarker("look.ready1")&&netTestMarker("look.ready2")&&netTestMarker("look.ready3")){puts("GEOSITH_NET_EDITOR_LOOK PASS: host remains editing, client up/down look works immediately after welcome");fflush(stdout);exit(0);}
        if(stdPlatform_GetTimeMsec()-netTestStart>90000)exit(2);
        sithNet_isMulti=0;sithMessage_g_outputstream&=~1;sithMessage_g_inputstream&=~1;return 1;
    }
    if(getenv("GEOSITH_NET_SURFACE_SESSION_TEST"))return stressSurfaceSession();
    if(getenv("GEOSITH_NET_TERRAIN_SESSION_TEST"))return stressTerrainSession();
    if(getenv("GEOSITH_NET_CLIENT_DECAL_TEST")){
        static int phase, baseline=-1;
        if(baseline<0&&(netTestRole==1||netReady))baseline=model->decalCount;
        auto decalHit=[](int skip){EdHit hit={};hit.cell=-1;hit.vertex=hit.edge=-1;for(int c=0;c<model->count;c++)for(int f=0;f<model->c[c].count;f++){const EdFace &face=model->c[c].f[f];if(face.neighbor>=0||face.decalId)continue;if(skip-- >0)continue;hit.cell=c;hit.face=f;hit.point=edV(0,0,0);for(int v=0;v<face.count;v++)hit.point=edAdd(hit.point,face.v[v].p);hit.point=edScale(hit.point,1.0/face.count);return hit;}return hit;};EditorNetPeer *a=netPeer(2,0),*b=netPeer(3,0);
        if(netTestRole==1&&a&&b&&a->ready==(int)netRevision&&b->ready==(int)netRevision)netTestMark("decals.ready",1);
        if(netTestRole==2&&netReady&&!netEditPending()&&netTestMarker("decals.ready")){
            if(!phase){EdHit hit=decalHit(0);if(hit.cell<0)exit(2);currentMaterial=addMaterialName("dflt.mat");if(!placeMaterialDecal(hit,0))exit(2);phase=1;}
            else if(phase==1&&netTestMarker("decal.seen1")&&netTestMarker("decal.seen2")&&netTestMarker("decal.seen3")){if(!historyStep(0))exit(2);phase=2;}
            else if(phase==2&&netTestMarker("decal.undo1")&&netTestMarker("decal.undo2")&&netTestMarker("decal.undo3")){if(!historyStep(1))exit(2);phase=3;}
        }
        if(netTestRole==2&&phase==1){static int checked;EditorNetPeer *peer=netPeer(sithNet_serverNetId,0);if(!checked&&peer&&peer->tx.bytes){
            char status[192];if(!peer->tx.h.base||!netEditStatus(status,sizeof(status))||!strstr(status,"Sending edit"))exit(2);
            printf("GEOSITH_NET_CLIENT_DECAL_DELTA PASS: first proposal raw=%u packed=%u base=%u\n",peer->tx.h.raw,peer->tx.h.total,peer->tx.h.base);fflush(stdout);checked=1;
        }}
        if((netTestRole==1||netReady)&&!netEditPending()){
            char name[32];
            if(model->decalCount==baseline+1){
                int found=0;for(int c=0;c<model->count;c++)for(int f=0;f<model->c[c].count;f++)if(model->c[c].f[f].decalId){rdMaterial *mat=editorFaceMaterial(model,&model->c[c].f[f]);if(!mat||sithWorld_g_pCurrentWorld->aSectors[c].surfaces[f].surfaceInfo.face.material!=mat)exit(2);found++;}if(!found)exit(2);
                snprintf(name,32,netTestMarker("decal.undo1")&&netTestMarker("decal.undo2")&&netTestMarker("decal.undo3")?"decal.redo%d":"decal.seen%d",netTestRole);netTestMark(name,edNetHash(model->decals[baseline].data->data(),(uint32_t)model->decals[baseline].data->size()));
            }else if(model->decalCount==baseline&&netTestMarker("decal.seen1")&&netTestMarker("decal.seen2")&&netTestMarker("decal.seen3")){snprintf(name,32,"decal.undo%d",netTestRole);netTestMark(name,1);}
        }
        if(netTestRole==1&&!phase&&netTestMarker("decal.redo1")&&netTestMarker("decal.redo2")&&netTestMarker("decal.redo3")){
            EdHit hit=decalHit(0);if(hit.cell<0)exit(2);currentMaterial=addMaterialName("dflt.mat");if(!placeMaterialDecal(hit,0))exit(2);phase=1;
        }
        if(model->decalCount==baseline+2&&(netTestRole==1||netReady)&&!netEditPending()){
            char name[32];snprintf(name,32,"decal.host%d",netTestRole);netTestMark(name,netTestModelHash());
            if(netTestMarker("decal.host1")&&netTestMarker("decal.host2")&&netTestMarker("decal.host3")){puts("GEOSITH_NET_CLIENT_DECAL PASS: client placement, live host/peer material, shared undo/redo and host placement");fflush(stdout);exit(0);}
        }
        if(stdPlatform_GetTimeMsec()-netTestStart>(getenv("GEOSITH_NET_TIMEOUT_MS")?(unsigned)atoi(getenv("GEOSITH_NET_TIMEOUT_MS")):90000)){printf("GEOSITH_NET_CLIENT_DECAL FAIL role=%d decals=%d ready=%d busy=%d phase=%d rejections=%u\n",netTestRole,model->decalCount,netReady,netBusy,phase,netRejections);fflush(stdout);exit(2);}
        sithNet_isMulti=0;sithMessage_g_outputstream&=~1;sithMessage_g_inputstream&=~1;return 1;
    }
    if(getenv("GEOSITH_NET_LEVEL")){
        if(getenv("GEOSITH_NET_ARRAY_TEST")){
            if(model->groupCount!=1||strcmp(model->groups[0].name,"Hallway Module")||model->splineCount!=1||model->splines[0].groupId!=1)exit(2);
            char ready[32];snprintf(ready,sizeof(ready),"array.ready%d",netTestRole);netTestMark(ready,1);jkEditor_NetTick();
            if(netTestRole==1&&!netTestMarker("array.changed")){
                EditorNetPeer *a=netPeer(2,0),*b=netPeer(3,0);if(!netTestMarker("array.ready2")||!netTestMarker("array.ready3")||!a||!b||a->ready!=(int)netRevision||b->ready!=(int)netRevision)return 1;
                EdModel updated;EdSpline spline=model->splines[0];spline.points[1].position.x=8.25;
                if(!edGroupArray(&updated,model,1,2,2)||!edSplineRebuild(candidate,&updated,&spline))exit(2);
                remember();if(!commit()||!historyStep(0)||model->groups[0].copies!=3||!historyStep(1)||model->groups[0].copies!=2)exit(2);
                netTestMark("array.changed",netRevision);
            }
            if(!netTestMarker("array.changed")||model->groups[0].copies!=2||fabs(model->splines[0].points[1].position.x-8.25)>.001)return 1;
            puts("GEOSITH_NET_ARRAY PASS: saved groups, array regeneration, bent spline and undo/redo synchronized");fflush(stdout);
        }
        if(getenv("GEOSITH_NET_DECAL_TEST")){
            char ready[32];snprintf(ready,sizeof(ready),"decal.ready%d",netTestRole);netTestMark(ready,1);jkEditor_NetTick();
            if(netTestRole==1&&!netTestMarker("decal.changed")){
                EditorNetPeer *a=netPeer(2,0),*b=netPeer(3,0);if(!netTestMarker("decal.ready2")||!netTestMarker("decal.ready3")||!a||!b||a->ready!=(int)netRevision||b->ready!=(int)netRevision)return 1;
                EdHit hit;if(!edRay(model,edV(0,0,.5),edV(0,1,0),&hit))exit(2);EdPaintImage base,decal;decal.width=decal.height=1;decal.rgba={255,32,32,255};
                if(!terrainLoadImage(paletteNames[model->c[hit.cell].f[hit.face].material],&base))exit(2);
                remember();edCopyModel(candidate,model);if(!edPlaceDecal(candidate,&candidate->c[hit.cell].f[hit.face],hit.point,.4,&base,&decal,getenv("GEOSITH_NET_SURFACE_PAINT_TEST")?.5:0)||!commit())exit(2);
                if(!historyStep(0)||!historyStep(1)||!model->decalCount)exit(2);
                netTestMark("decal.changed",edNetHash(model->decals[0].data->data(),(unsigned)model->decals[0].data->size()));
            }
            char path[512];netTestPath(path,"decal.changed");FILE *f=fopen(path,"rb");unsigned hash=0;if(f){fscanf(f,"%u",&hash);fclose(f);}if(!hash||!model->decalCount||edNetHash(model->decals[0].data->data(),(unsigned)model->decals[0].data->size())!=hash)return 1;
            int visible=0;for(int c=0;c<model->count;c++)for(int face=0;face<model->c[c].count;face++)if(model->c[c].f[face].decalId){if(!editorFaceMaterial(model,&model->c[c].f[face]))exit(2);visible++;}if(!visible)exit(2);
            puts("GEOSITH_NET_DECAL PASS: baked texture, surface UVs, undo and redo synchronized");fflush(stdout);
        }
        if(getenv("GEOSITH_NET_PAINT_TEST")){
            if(!edPainted(model,0))exit(2);char ready[32];snprintf(ready,sizeof(ready),"paint.ready%d",netTestRole);netTestMark(ready,1);jkEditor_NetTick();
            if(netTestRole==1&&!netTestMarker("paint.changed")){
                EditorNetPeer *a=netPeer(2,0),*b=netPeer(3,0);if(!netTestMarker("paint.ready2")||!netTestMarker("paint.ready3")||!a||!b||a->ready!=(int)netRevision||b->ready!=(int)netRevision)return 1;
                unsigned before=edPaintHash(model,0);remember();edCopyModel(candidate,model);
                if(!edTerrainPaint(candidate,0,edV(0,0,.002),.75,.8,"dflt.mat",terrainMaterialName)||!commit()||edPaintHash(model,0)==before)exit(2);
                unsigned after=edPaintHash(model,0);if(!historyStep(0)||edPaintHash(model,0)!=before||!historyStep(1)||edPaintHash(model,0)!=after)exit(2);
                netTestMark("paint.changed",after);
            }
            char path[512];netTestPath(path,"paint.changed");FILE *f=fopen(path,"rb");unsigned hash=0;if(f){fscanf(f,"%u",&hash);fclose(f);}if(!hash||edPaintHash(model,0)!=hash)return 1;
            puts("GEOSITH_NET_PAINT PASS: vertex layers, painting, undo and redo synchronized");fflush(stdout);
        }
        if(getenv("GEOSITH_NET_EXPECT_SECTORS")){
            /* This long-running harness bypasses normal gameplay transmission. */
            if(netTestRole!=1){static uint32_t heartbeat;uint32_t now=stdPlatform_GetTimeMsec();if(now-heartbeat>=1000){heartbeat=now;sithMulti_Ping(sithNet_serverNetId);}}
            if(model->count!=atoi(getenv("GEOSITH_NET_EXPECT_SECTORS")))exit(2);jkEditor_NetTick();
            {static uint32_t traceTime;uint32_t now=stdPlatform_GetTimeMsec();if(now-traceTime>10000){traceTime=now;printf("LARGE STATE role=%d revision=%u gravity=%g ready=%d\n",netTestRole,netRevision,model->header.gravity,netReady);for(int i=0;i<32;i++)if(netPeers[i].player)printf("LARGE PEER player=%d ready=%d tx=%u/%u rev=%u rx=%u/%u dirty=%d\n",netPeers[i].player,netPeers[i].ready,netPeers[i].tx.at,netPeers[i].tx.h.total,netPeers[i].tx.h.revision,netPeers[i].rx.at,netPeers[i].rx.h.total,netPeers[i].dirty);fflush(stdout);}}

            if(stdPlatform_GetTimeMsec()-netTestStart>180000)exit(2);
            if(netTestRole==1&&!netTestMarker("large.revision")){
                EditorNetPeer *a=netPeer(2,0),*b=netPeer(3,0);if(!a||!b||(int)sithPlayer_GetPlayerNum(2)<0||(int)sithPlayer_GetPlayerNum(3)<0||a->ready!=(int)netRevision||b->ready!=(int)netRevision)return 1;
                remember();edCopyModel(candidate,model);candidate->header.gravity=7.25;
                if(!commit()||!historyStep(0)||!historyStep(1)||model->header.gravity!=7.25)exit(2);
                netTestMark("large.revision",netRevision);puts("GEOSITH_NET_LARGE PASS: large document edit, undo and redo");fflush(stdout);
            }
            if(netTestRole!=1){
                char path[512];netTestPath(path,"large.revision");FILE *file=fopen(path,"rb");unsigned int revision=0;
                if(file){fscanf(file,"%u",&revision);fclose(file);}if(!revision||netRevision<revision||model->header.gravity!=7.25)return 1;
                puts("GEOSITH_NET_LARGE PASS: complete large document and host undo/redo synchronized");fflush(stdout);
            }
        }
        if(netTestRole!=1){char marker[32];snprintf(marker,sizeof(marker),"loaded%d.ok",netTestRole);netTestMark(marker,1);puts("GEOSITH_NET_LOADED PASS: saved custom level joined with visible host");fflush(stdout);exit(0);}
        if(netTestMarker("loaded2.ok")&&netTestMarker("loaded3.ok")){puts("GEOSITH_NET_LOADED PASS: both clients joined saved level");fflush(stdout);exit(0);}
        if(stdPlatform_GetTimeMsec()-netTestStart>(getenv("GEOSITH_NET_EXPECT_SECTORS")?180000u:90000u))exit(2);return 1;
    }
    jkEditor_NetTick();
    sithMessage_RegisterFunction(DSS_PLAYSOUND,netTestSoundMessage);sithMessage_RegisterFunction(DSS_FIREPROJECTILE,netTestFireSoundMessage);
    if(netTestRole==1){
        EditorNetPeer *first=netPeer(2,0),*second=netPeer(3,0);uint32_t now=stdPlatform_GetTimeMsec();
        if(first&&first->tx.bytes&&(int)sithPlayer_GetPlayerNum(2)<0&&now-netTestPulseTime>100&&netTestJoinPulses<40){netTestPulseTime=now;remember();edCopyModel(candidate,model);candidate->lights[0].range=2+(++netTestJoinPulses%2)*ED_GRID;if(!commit())exit(2);}
        if(!netTestMarker("edits.ready")&&first&&second&&(int)sithPlayer_GetPlayerNum(2)>=0&&(int)sithPlayer_GetPlayerNum(3)>=0&&first->ready==(int)netRevision&&second->ready==(int)netRevision){
            if(!netTestJoinPulses)exit(2);netTestMark("edits.ready",netRevision);printf("GEOSITH_NET_MOVING_JOIN PASS: %d host edits during initial transfer\n",netTestJoinPulses);fflush(stdout);
        }
        if(netTestMarker("redo.done")&&!netTestMarker("stale.ready")&&first&&second&&first->ready==(int)netRevision&&second->ready==(int)netRevision)netTestMark("stale.ready",netRevision);
    }
    if(netTestRole==1&&netTestMarker("conflict.ready")&&!netTestMarker("conflict.sent")){
        stdComm_GNS_AllowEdits(0);remember();edCopyModel(candidate,model);candidate->header.gravity=5;if(!commit())exit(2);netTestMark("conflict.sent",netRevision);
    }
    if(netTestRole==1&&fabs(model->header.gravity-3.5)>.001&&(grabbing||objectGrabbing))exit(2);
    if(netTestRole==2&&netReady&&!netBusy&&!netQueued&&netTestMarker("edits.ready")){
        if(netTestStage==0){if(model->count!=2||model->objectCount!=1||model->objects[0].attributes.frameCount!=2||model->lightCount!=1||model->cogCount!=1||fabs(model->header.gravity-3.5)>.001)exit(2);
            if(model->c[0].f[2].count!=18||!edValidate(model))exit(2);printf("GEOSITH_NET_SHAPE PASS: clipped 32-sided arch replicated with 18 vertices\n");fflush(stdout);
            if(fabs(model->c[1].extraLight-.35)>.00001||fabs(model->c[1].ambientLight-.1)>.00001||fabs(sithWorld_g_pCurrentWorld->aSectors[1].extraLight-.35)>.00001)exit(2);
            printf("GEOSITH_NET_LIGHT PASS: sector brightness replicated independently of point lights\n");fflush(stdout);
            SithThing *object=&sithWorld_g_pCurrentWorld->aThings[objectThings[0]];if(object->guid!=1||sithThing_GetGuidThing(1)!=object||jkEditor_NetThingSlot(1,1)!=object->idx)exit(2);
            int freeSlot=jkEditor_NetThingSlot(object->idx,123456);if(freeSlot<0||sithWorld_g_pCurrentWorld->aThings[freeSlot].type)exit(2);
            SithMessage native={0};native.netMsg.cogMsgId=DSS_THINGPOS;native.netMsg.msg_size=4;edCopy(native.pktData,&object->guid,4);if(!jkEditor_NetIgnoreMessage(&native))exit(2);int guid=100000;edCopy(native.pktData,&guid,4);if(jkEditor_NetIgnoreMessage(&native))exit(2);
            remember();edCopyModel(candidate,model);candidate->header.gravity=4.5;if(candidate->meshCount!=1||!edMeshExtrude(candidate,EdHit{0,2,-1,-1}))exit(2);if(!edCleanupSector(candidate,1)||candidate->c[1].count!=6||!commit()||!netEditPending()||fabs(model->header.gravity-4.5)>.001||fabs(sithWorld_g_pCurrentWorld->gravity-4.5)>.001||fabs(netConfirmed->header.gravity-3.5)>.001)exit(2);
            jkEditor_NetTick();if(!netBusy)exit(2);
            edCopyModel(candidate,model);candidate->header.gravity=4.75;if(!commit()||!netQueued||fabs(model->header.gravity-4.75)>.001)exit(2);netTestStage=1;
            printf("GEOSITH_NET_PREDICTION PASS: immediate preview and newer drag sample while approval pending\n");fflush(stdout);}
        else if(netTestStage==1&&fabs(model->header.gravity-4.75)<.001){if(!historyStep(0))exit(2);netTestStage=2;}
        else if(netTestStage==2&&fabs(model->header.gravity-3.5)<.001){if(model->c[1].count!=7||model->meshes[0].geometry.count!=6||!historyStep(1))exit(2);netTestStage=3;}
        else if(netTestStage==3&&fabs(model->header.gravity-4.75)<.001){if(model->c[1].count!=6||model->meshes[0].geometry.count!=10)exit(2);printf("GEOSITH_NET_3DO PASS: full join, mesh extrusion, host replication and shared undo/redo\n");printf("GEOSITH_NET_CLEANUP PASS: predicted sector cleanup, host replication and shared undo/redo\n");fflush(stdout);netTestMark("redo.done",netRevision);netTestStage=4;}
    }
    if(netTestRole==3&&netReady&&!netBusy&&!netQueued){
        if(netTestStage==0&&netTestMarker("stale.ready")&&fabs(model->header.gravity-4.75)<.001){remember();edCopyModel(candidate,model);candidate->header.gravity=99;
            if(!edExtrudeStep(candidate,1,1)||!commit()||model->count!=3||sithWorld_g_pCurrentWorld->numSectors!=3||fabs(model->header.gravity-99)>.001)exit(2);
            EditorNetPeer *p=netPeer(1,1);if(!netQueueState(p,EN_EDIT,stdComm_dplayIdSelf,netGesture,netQueued))exit(2);edFree(netQueued);netQueued=NULL;p->tx.h.revision=netRevision-1;netBusy=1;netTestStage=1;}
        else if(netTestStage==1&&netRejections){if(!netTestRollbackSeen||fabs(model->header.gravity-4.75)>.001||model->count!=2)exit(2);remember();edCopyModel(candidate,model);if(!edExtrudeStep(candidate,1,1)||!commit()||model->count!=3||sithWorld_g_pCurrentWorld->numSectors!=3)exit(2);netTestStage=2;}
        else if(netTestStage==2&&model->count==3){remember();edCopyModel(candidate,model);candidate->objects[0].attributes.frames[1].position.x+=ED_GRID;if(!commit())exit(2);netTestStage=3;}
        else if(netTestStage==3&&fabs(model->objects[0].attributes.frames[1].position.x-(.2+ED_GRID))<.00001){EdBuffer<unsigned char> result;EdVec outline[3]={{0,0,0},{1,0,0},{0,1,0}};if(!mapAllocatePending()||!edMapPolygon(mapPending,outline,3,0,1,0))exit(2);remember();if(!edMapPaste(candidate,model,mapPending,edV(3,0,0),result)||!commit()||model->count!=4||sithWorld_g_pCurrentWorld->numSectors!=4)exit(2);netTestStage=4;}
        else if(netTestStage==4&&model->count==4){if(!historyStep(0))exit(2);netTestStage=5;}
        else if(netTestStage==5&&model->count==3){if(!historyStep(1))exit(2);netTestStage=6;}
        else if(netTestStage==6&&model->count==4){
            EdBuffer<unsigned char> selected,result;selected[3]=1;remember();
            if(!edMapCleave(candidate,model,selected,edV(3.5,-1,0),edV(3.5,2,0),result)||!commit()||model->count!=5||sithWorld_g_pCurrentWorld->numSectors!=5)exit(2);netTestStage=12;
        }
        else if(netTestStage==12&&model->count==5){
            EdBuffer<unsigned char> selected;selected[4]=1;remember();
            if(!edMapDelete(candidate,model,selected)||!commit()||model->count!=4)exit(2);netTestStage=13;
        }
        else if(netTestStage==13&&model->count==4){if(!historyStep(0))exit(2);netTestStage=14;}
        else if(netTestStage==14&&model->count==5){if(!historyStep(1))exit(2);netTestStage=15;}
        else if(netTestStage==15&&model->count==4){
            printf("GEOSITH_NET_MAP PASS: predicted cleave/deletion, host replication and shared undo/redo\n");fflush(stdout);
            remember();edCopyModel(candidate,model);candidate->header.gravity=88;if(!commit()||fabs(model->header.gravity-88)>.001)exit(2);
            /* Hold this speculative edit locally while the host commits a
               competing edit. Its state must cancel, never submit, our draft. */
            netBusy=1;netTestMark("conflict.ready",netRevision);netTestStage=7;
        }
        else if(netTestStage==7&&fabs(model->header.gravity-5)<.001){
            if(netQueued||grabbing||fabs(netConfirmed->header.gravity-5)>.001)exit(2);
            printf("GEOSITH_NET_PREDICTION PASS: remote host edit superseded pending preview\n");fflush(stdout);
            netTestLockedRejections=netRejections;remember();edCopyModel(candidate,model);candidate->header.gravity=99;if(!commit()||fabs(model->header.gravity-99)>.001)exit(2);netTestStage=8;
        }
        else if(netTestStage==8&&netRejections>netTestLockedRejections&&fabs(model->header.gravity-5)<.001){
            printf("GEOSITH_NET_PERMISSION PASS: host edit lock rejects client changes and restores prediction\n");fflush(stdout);netTestMark("map.done",netRevision);netTestStage=9;
        }
    }
    if(netTestRole==2&&netTestStage==1&&fabs(model->header.gravity-4.75)>.001)exit(2);
    int final=netTestMarker("map.done")&&model->count==4&&model->objectCount==1&&model->cogCount==1&&model->lightCount==1&&model->objects[0].attributes.frameCount==2&&fabs(model->objects[0].attributes.frames[1].position.x-(.2+ED_GRID))<.00001&&fabs(model->header.gravity-5)<.001;
    if(final&&netTestRole==1&&!netTestEnemyStage){
        remember();edCopyModel(candidate,model);candidate->objectCount=2;candidate->objects[1]=EdObject{.position={-.3,.2,.2}};snprintf(candidate->objects[1].templateName,64,"stormtroop");
        if(!commit()||objectThings[1]<0)exit(2);netTestEnemyStage=1;
    }
    int enemyPresent=netTestMarker("map.done")&&model->objectCount==2&&objectThings[1]>=0;
    if(enemyPresent&&netTestRole!=1&&!netTestEnemyStage){
        if(netTestRole==3)leaveEditor(sithWorld_g_pCurrentWorld->pLocalPlayer);
        SithThing *enemy=&sithWorld_g_pCurrentWorld->aThings[objectThings[1]];
        printf("NETTEST enemy role=%d active=%d type=%d guid=%d puppet=%p\n",netTestRole,active,enemy->type,enemy->guid,(void*)enemy->puppet);fflush(stdout);
        netTestMark(netTestRole==2?"enemy.edit.ready":"enemy.play.ready",netRevision);netTestEnemyStage=1;
    }
    if(enemyPresent&&netTestRole==1&&netTestEnemyStage==1&&netTestMarker("enemy.edit.ready")&&netTestMarker("enemy.play.ready")){
        leaveEditor(sithWorld_g_pCurrentWorld->pLocalPlayer);netCommitted();netTestEnemyStage=2;
    }
    if(enemyPresent&&netTestRole==1&&netTestEnemyStage==2){EditorNetPeer *a=netPeer(2,0),*b=netPeer(3,0);
        if(a&&b&&a->ready==(int)netRevision&&b->ready==(int)netRevision){
            SithThing *enemy=&sithWorld_g_pCurrentWorld->aThings[objectThings[1]];
            printf("NETTEST killing stormtrooper guid=%d puppet=%p\n",enemy->guid,(void*)enemy->puppet);fflush(stdout);
            sithThing_DamageThing(enemy,sithWorld_g_pCurrentWorld->pLocalPlayer,1000,SITH_DAMAGE_SABER);
            for(int peerId=2;peerId<=3;peerId++){
                sithDSSThing_Death(enemy,sithWorld_g_pCurrentWorld->pLocalPlayer,0,peerId,1);
                sithDSSThing_PlayKeyMode(enemy,SITH_ANIM_DEATH,0,peerId,1);
            }
            netTestMark("enemy.killed",netRevision);netTestEnemyStage=3;
        }
    }
    if(enemyPresent&&netTestRole!=1&&netTestEnemyStage==1&&netTestMarker("enemy.killed")){
        if(!netTestEnemyTime)netTestEnemyTime=stdPlatform_GetTimeMsec();
        SithThing *enemy=&sithWorld_g_pCurrentWorld->aThings[objectThings[1]];
        if(stdPlatform_GetTimeMsec()-netTestEnemyTime>1000&&(netTestRole==2||enemy->actorParams.health<=0)){
            if(netTestRole==2&&(enemy->type!=SITH_THING_COG||enemy->puppet))exit(2);
            netTestMark(netTestRole==2?"enemy.edit.ok":"enemy.play.ok",netRevision);
            if(netTestEnemyStage<2)printf("GEOSITH_NET_ENEMY PASS: role %d survived host kill in %s mode\n",netTestRole,netTestRole==2?"editor":"play");
            netTestEnemyStage=2;
        }
    }
    if(enemyPresent&&netTestRole==1&&netTestEnemyStage==3&&netTestMarker("enemy.edit.ok")&&netTestMarker("enemy.play.ok")){
        active=1;syncEditorObjects(model);suspendPlacedCogs(0);netCommitted();
        netTestMark("enemy.host.reset",netRevision);netTestEnemyStage=4;
    }
    if(enemyPresent&&netTestRole!=1&&netTestEnemyStage==2&&netTestMarker("enemy.host.reset")){
        active=1;syncEditorObjects(model);suspendPlacedCogs(0);
        SithThing *enemy=&sithWorld_g_pCurrentWorld->aThings[objectThings[1]];
        if(enemy->type!=SITH_THING_COG||enemy->puppet||enemy->guid!=netObjectGuids[1])exit(2);
        netTestMark(netTestRole==2?"enemy.client2.reset":"enemy.client3.reset",netRevision);netTestEnemyStage=4;
    }
    if(enemyPresent&&netTestRole==1&&netTestEnemyStage==4&&netTestMarker("enemy.client2.reset")&&netTestMarker("enemy.client3.reset")){
        leaveEditor(sithWorld_g_pCurrentWorld->pLocalPlayer);netCommitted();netTestEnemyStage=5;
    }
    if(enemyPresent&&netTestRole==1&&netTestEnemyStage==5){EditorNetPeer *a=netPeer(2,0),*b=netPeer(3,0);
        if(a&&b&&a->ready==(int)netRevision&&b->ready==(int)netRevision){
            SithThing *enemy=&sithWorld_g_pCurrentWorld->aThings[objectThings[1]],*projectile=sithTemplate_GetTemplate("+elaser");
            rdVector3 direction={0,1,0};if(!projectile||enemy->actorParams.health<=0)exit(2);
            printf("NETTEST reset stormtrooper firing at editing clients guid=%d\n",enemy->guid);fflush(stdout);
            for(int peerId=2;peerId<=3;peerId++)sithDSSThing_Fire(enemy,projectile,&enemy->position,&direction,NULL,SITH_ANIM_FIRE,1,0,0,0x70001,peerId,1,0);
            netTestMark("enemy.reset.fire",netRevision);netTestEnemyStage=6;
        }
    }
    if(enemyPresent&&netTestRole!=1&&netTestEnemyStage==4&&netTestMarker("enemy.reset.fire")){
        netTestEnemyTime=stdPlatform_GetTimeMsec();netTestEnemyStage=5;
    }
    if(enemyPresent&&netTestRole!=1&&netTestEnemyStage==5&&stdPlatform_GetTimeMsec()-netTestEnemyTime>1000){
        SithThing *enemy=&sithWorld_g_pCurrentWorld->aThings[objectThings[1]];
        if(enemy->type!=SITH_THING_COG||enemy->puppet||sithThing_GetGuidThing(0x70001))exit(2);
        netTestMark(netTestRole==2?"enemy.cycle2.ok":"enemy.cycle3.ok",netRevision);netTestEnemyStage=6;
        printf("GEOSITH_NET_ENEMY_RESET PASS: role %d survived kill, host reset, client reset and host resuming fire\n",netTestRole);fflush(stdout);
    }
    final=enemyPresent&&netTestMarker("enemy.cycle2.ok")&&netTestMarker("enemy.cycle3.ok");
    if(final){static int checked; if(!checked){int activePlayers=0;for(unsigned int i=0;i<jkPlayer_maxPlayers;i++)if(jkPlayer_playerInfos[i].flags&1){activePlayers++;SithThing *body=jkPlayer_playerInfos[i].pLocalPlayer;if(!body||!body->renderData.model3||strcmp(body->renderData.model3->filename,"kyb3.3do")){printf("GEOSITH_NET_SKIN FAIL: role %d slot %u model %s\n",netTestRole,i,body&&body->renderData.model3?body->renderData.model3->filename:"missing");fflush(stdout);exit(2);}}if(activePlayers!=3){printf("GEOSITH_NET_ROSTER FAIL: role %d active players %d\n",netTestRole,activePlayers);fflush(stdout);exit(2);}checked=1;puts("GEOSITH_NET_SKIN PASS: selected Krugon models survive join and edits on every peer");fflush(stdout);}}
    if(final&&netTestRole==1){
        static uint32_t lastSound;uint32_t now=stdPlatform_GetTimeMsec();
        if(now-lastSound>250){lastSound=now;sithSound *loaded=sithSound_Load("walkmetaldry02.wav",0);if(!loaded)exit(2);
            sithSound mismatched=*loaded;mismatched.id=0;SithThing *body=jkPlayer_playerInfos[0].pLocalPlayer;rdVector3 direction={0,1,0};
            for(int peer=2;peer<=3;peer++){
                sithDSSThing_PlaySound(body,&body->position,&mismatched,1,0,0,0x67891230,peer,1);
                sithDSSThing_PlaySound(body,&body->position,&mismatched,1,4,SITHSOUNDFLAG_ABSOLUTE,0x67891231,peer,1);
                sithDSSThing_PlaySound(body,&body->position,&mismatched,1,4,SITHSOUNDFLAG_FOLLOWSTHING,0x67891232,peer,1);
                sithDSSThing_Fire(body,NULL,&direction,&body->position,&mismatched,-1,1,0,0,0x67891233,peer,1,0);
            }
        }
    }
    if(final&&netTestRole==1&&stdPlatform_GetTimeMsec()-netTestPositionTime>100){netTestPositionTime=stdPlatform_GetTimeMsec();SithThing *body=jkPlayer_playerInfos[0].pLocalPlayer;body->position.x=.75;body->physicsParams.vel.x=0;sithDSSThing_Pos(body,-1,0);}
    if(final&&netTestRole==1){EditorNetPeer *a=netPeer(2,0),*b=netPeer(3,0);if(a&&b&&a->ready==(int)netRevision&&b->ready==(int)netRevision)netTestMark("host.hash",netTestModelHash());}
    if(final&&netTestRole!=1&&netTestSoundMask==15&&netTestMarker("host.hash")&&jkPlayer_playerInfos[0].pLocalPlayer&&fabs(jkPlayer_playerInfos[0].pLocalPlayer->position.x-.75)<.001){printf("GEOSITH_NET_SOUND PASS: weapon/global/attached/positional sounds resolved by filename despite conflicting indices\n");printf("GEOSITH_NET_POSITION PASS: remote host movement replicated over sequenced unreliable packets\n");char path[512];netTestPath(path,"host.hash");FILE *f=fopen(path,"rb");unsigned int hash=0;if(!f||fscanf(f,"%u",&hash)!=1)exit(2);fclose(f);if(hash!=netTestModelHash()){printf("GEOSITH_NET FAIL: hash mismatch role=%d revision=%u host=%u local=%u\n",netTestRole,netRevision,hash,netTestModelHash());fflush(stdout);exit(2);}char name[32];snprintf(name,32,"client%d.ok",netTestRole);netTestMark(name,netRevision);printf("GEOSITH_NET_CLIENT PASS: role %d, full level, edits, undo/redo, stale rejection and matching document\n",netTestRole);fflush(stdout);exit(0);}
    if(netTestRole==1&&netTestMarker("client2.ok")&&netTestMarker("client3.ok")){if(netDeltaTransfers<2)exit(2);printf("GEOSITH_NET_MODAL PASS: discovery, join and edits while host modal; %u delta transfers, %llu / %llu bytes\n",netDeltaTransfers,(unsigned long long)netPackedBytes,(unsigned long long)netRawBytes);printf("GEOSITH_NET_HOST PASS: two clients, authoritative join/edit/undo, dropped and duplicated packets, revision %u\n",netRevision);fflush(stdout);exit(0);}
    if(stdPlatform_GetTimeMsec()-netTestStart>(getenv("GEOSITH_NET_TIMEOUT_MS")?(uint32_t)atoi(getenv("GEOSITH_NET_TIMEOUT_MS")):90000u)){printf("GEOSITH_NET FAIL: timeout role=%d stage=%d ready=%d busy=%d revision=%u peers=%d,%d self=%d\n",netTestRole,netTestStage,netReady,netBusy,netRevision,netPeers[0].player,netPeers[1].player,stdComm_dplayIdSelf);fflush(stdout);exit(2);}
    sithNet_isMulti=0;sithMessage_g_outputstream&=~1;sithMessage_g_inputstream&=~1;return 1;
}
