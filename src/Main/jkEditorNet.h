#include "edNet.h"
#include "edWire.h"
#include "Gameplay/sithPlayer.h"
/* Editor messages have their own bounded, cumulative-ACK stream. This avoids
 * exhausting JK's 32-entry reliable gameplay-message retry queue. */
#define EN_MAGIC 0x45444e32u
#define EN_VERSION 5u
#define EN_CHUNK 1400u
enum {EN_STATE=1,EN_EDIT,EN_HELLO,EN_ACK,EN_UNDO,EN_REDO,EN_REJECT};
typedef struct {uint32_t magic,version,kind,id,revision,gesture,origin,total,raw,hash,offset,length,base;} EditorNetHeader;
typedef struct {unsigned char *bytes;EditorNetHeader h;uint32_t at,sent,time;unsigned char *rawBytes;} EditorNetStream;
typedef struct {int player,ready,dirty,dirtyOrigin;uint32_t gesture,lastCommand,dirtyGesture;EditorNetStream tx,rx;unsigned char *outBase,*inBase;uint32_t outSize,inSize;} EditorNetPeer;
typedef struct {uint32_t layout,modelBytes,materials,resources,cogs,editing,objects,assets;int32_t players[ED_SPAWNS];} EditorNetDocument;
static EditorNetPeer netPeers[32];
static uint32_t netRevision,netGesture=1,netSequence=1,netLastHello,netCommand;
static unsigned int netRejections,netDeltaTransfers;
static uint64_t netRawBytes,netPackedBytes;
static int netReady,netWaiting,netBusy,netLocalAuthor;
static int netNeedsEditorSetup;
static int netHadState,netFatal,netValidationFailures,netSafeDialog;
static char netFailureReason[192];
static int (*netTestSend)(int,const SithMessage*);
static EdModel *netQueued;
/* Live client geometry may be speculative. Keep the last accepted document
   separately so a rejection can roll back before its replacement arrives. */
static EdModel *netConfirmed;
static uint32_t netQueuedGesture;
static int32_t netObjectGuids[ED_OBJECTS];
static char *netCogTexts[ED_COGS];static char netCogNames[ED_COGS][32];static int netCogCount;
static uint32_t netLayout(void){return ED_WIRE_VERSION;}
static int netEnabled(void){return sithNet_isMulti&&jkEditor_IsLevel();}
int jkEditor_NetOwnsGuid(int guid)try {if(!netEnabled()||!model)return 0;for(int i=0;i<ED_SPAWNS;i++)if(jkPlayer_playerInfos[i].pLocalPlayer&&jkPlayer_playerInfos[i].pLocalPlayer->guid==guid)return 1;for(int i=0;i<model->objectCount;i++){int id=objectThings[i];if(id>=0&&id<sithWorld_g_pCurrentWorld->numThingsLoaded&&sithWorld_g_pCurrentWorld->aThings[id].guid==guid)return 1;}return 0;} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
int jkEditor_NetThingSlot(int requested,int guid)try {if(!netEnabled())return requested;SithThing *existing=sithThing_GetGuidThing(guid);if(existing)return existing->idx;SithWorld *w=sithWorld_g_pCurrentWorld;if(requested>=0&&requested<w->numThingsLoaded&&!w->aThings[requested].type)return requested;for(int i=0;i<w->numThingsLoaded;i++)if(!w->aThings[i].type)return i;return -1;} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
int jkEditor_NetIgnoreMessage(const SithMessage *m)try {if(!netEnabled())return 0;int type=m->netMsg.cogMsgId;
    if(type==DSS_LEAVEJOIN)return sithNet_isServer||m->netMsg.idx!=(uint32_t)(sithNet_serverNetId?sithNet_serverNetId:1);
    if(!model)return 0;
    /* An editor preview has no gameplay puppet/AI. A host may play and kill
       its real actor while this peer keeps editing. Never run gameplay
       lifecycle or animation handlers against that peer's preview. */
    switch(type){
        case DSS_SYNCTHING:case DSS_THINGPOS:case DSS_PATHMOVE:
        case DSS_PLAYSOUNDMODE:case DSS_PLAYKEY:case DSS_PLAYKEYMODE:
        case DSS_FIREPROJECTILE:case DSS_MOTS_NEW_2:
        case DSS_SETTHINGMODEL:case DSS_STOPKEY:case DSS_DEATH:case DSS_DAMAGE:
        case DSS_SYNCTHINGATTACHMENT:case DSS_DESTROYTHING:
            if(m->netMsg.msg_size>=4){int guid;edCopy(&guid,m->pktData,4);
                for(int i=0;i<model->objectCount;i++){int id=objectThings[i];
                    if(objectProxyMode[i]&&id>=0&&id<sithWorld_g_pCurrentWorld->numThingsLoaded){SithThing *t=&sithWorld_g_pCurrentWorld->aThings[id];if(t->type&&t->signature==objectSignatures[i]&&t->guid==guid)return 1;}
                }
            }break;
        default:break;
    }
    if(!active)return 0;if(type==DSS_SURFACESTATUS||type==DSS_SECTORSTATUS||type==DSS_SECTORFLAGS||type==DSS_SURFACE)return 1;
    if((type==DSS_SYNCTHING||type==DSS_THINGPOS||type==DSS_PATHMOVE)&&m->netMsg.msg_size>=4){int guid;edCopy(&guid,m->pktData,4);for(int i=0;i<model->objectCount;i++){int id=objectThings[i];if(id>=0&&id<sithWorld_g_pCurrentWorld->numThingsLoaded&&sithWorld_g_pCurrentWorld->aThings[id].guid==guid)return 1;}}return 0;
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
static int netClient(void){return netEnabled()&&!sithNet_isServer;}
static int netEditPending(void){return netClient()&&(!netReady||netBusy||netQueued!=NULL);}
static void netSyncObjectGuid(SithThing *thing,int id){if(netClient()&&netReady&&netObjectGuids[id]>=0)thing->guid=netObjectGuids[id];}
static void netFreeStream(EditorNetStream *s){edFree(s->bytes);edFree(s->rawBytes);edZero(s,0,sizeof(*s));}
static EditorNetPeer *netPeer(int player,int create){for(int i=0;i<32;i++)if(netPeers[i].player==player)return &netPeers[i];if(create&&player>0)for(int i=0;i<32;i++)if(!netPeers[i].player){netPeers[i].player=player;netPeers[i].ready=-1;return &netPeers[i];}return NULL;}
static int netSend(int player,EditorNetHeader h,const void *payload){SithMessage m={0};h.magic=EN_MAGIC;h.version=EN_VERSION;m.netMsg.cogMsgId=JKEDITOR_NET_MESSAGE;m.netMsg.msg_size=sizeof(h)+h.length;for(size_t i=0;i<sizeof(h)/4;i++){uint32_t n;edCopy(&n,(unsigned char*)&h+i*4,4);edNetStore32((unsigned char*)m.pktData+i*4,n);}if(h.length)edCopy((unsigned char*)m.pktData+sizeof(h),payload,h.length);if(netTestSend)return netTestSend(player,&m);return stdComm_SendToPlayer(&m,player);}
static int netString(const char *s,size_t length){return memchr(s,0,length)!=NULL;}
static int netFilename(const char *s,size_t length){if(!netString(s,length)||!*s)return 0;for(;*s;s++)if(!(isalnum((unsigned char)*s)||*s=='_'||*s=='-'||*s=='.'||*s=='+'||*s=='~'))return 0;return 1;}
static int netInvalid(int line){if(!netFailureReason[0])snprintf(netFailureReason,sizeof(netFailureReason),"Invalid or incompatible level data (check %d).",line);if(getenv("GEOSITH_NET_ROLE")){printf("NET validation failed at line %d\n",line);fflush(stdout);}return 0;}
static int netValidModel(const EdModel *m,int materials){
    if(!edTemplatesValidate(m))return netInvalid(__LINE__);
    if(m->count<1||m->count>ED_CELLS||m->spawnCount<1||m->spawnCount>ED_SPAWNS||m->objectCount<0||m->objectCount>ED_OBJECTS||m->cogCount<0||m->cogCount>ED_COGS)return netInvalid(__LINE__);
    if(!netString(m->header.title,sizeof(m->header.title))||!netFilename(m->header.colormap,sizeof(m->header.colormap)))return netInvalid(__LINE__);
    for(int i=0;i<20;i++)if(!netString(m->header.intro[i],160))return netInvalid(__LINE__);
    const double *numbers=&m->header.gravity;for(size_t i=0;i<(sizeof(m->header)-offsetof(EdLevelHeader,gravity))/sizeof(double);i++)if(!isfinite(numbers[i]))return netInvalid(__LINE__);
    if(m->episode.count<1||m->episode.count>ED_EPISODE_ROWS||!netString(m->episode.title,96))return netInvalid(__LINE__);
    for(int i=0;i<m->episode.count;i++){const EdEpisodeRow *r=&m->episode.rows[i];if(!netFilename(r->file,32)||!netString(r->title,96))return netInvalid(__LINE__);for(int j=0;j<20;j++)if(!netString(r->intro[j],160))return netInvalid(__LINE__);}
    if(!edEpisodeValidate(&m->episode))return netInvalid(__LINE__);
    for(int i=0;i<m->objectCount;i++)if(!netFilename(m->objects[i].templateName,64)||(!sithTemplate_GetTemplate(m->objects[i].templateName)&&edTemplateIndex(m,m->objects[i].templateName)<0))return netInvalid(__LINE__);
    for(int i=0;i<m->spawnCount+m->objectCount;i++){int id=i<m->spawnCount?-i-1:i-m->spawnCount;EdFrame q=thingPose(m,id);const double values[]={q.position.x,q.position.y,q.position.z,q.rotation.x,q.rotation.y,q.rotation.z};for(int j=0;j<6;j++)if(!isfinite(values[j])||!isfinite((float)values[j]))return netInvalid(__LINE__);if(objectSector(m,q.position)<0)return netInvalid(__LINE__);}
    for(int i=0;i<m->cogCount;i++){const EdPlacedCog *c=&m->cogs[i];if(!netFilename(c->file,32)||c->count<0||c->count>ED_COG_PARAMS)return netInvalid(__LINE__);for(int j=0;j<c->count;j++)if(!netString(c->params[j].name,32)||!netString(c->params[j].value,32))return netInvalid(__LINE__);}
    for(int s=0;s<m->count;s++){if(m->c[s].count<4||m->c[s].count>ED_FACES)return netInvalid(__LINE__);for(int f=0;f<m->c[s].count;f++){const EdFace *p=&m->c[s].f[f];if(p->count<3||!edSurfaceVertexCountAllowed(p->count)||p->material<0||p->material>=materials)return netInvalid(__LINE__);
        if(p->neighbor!=-1){if(p->neighbor<0||p->neighbor>=m->count||p->mirror<0||p->mirror>=m->c[p->neighbor].count||m->c[p->neighbor].count>ED_FACES)return netInvalid(__LINE__);const EdFace *q=&m->c[p->neighbor].f[p->mirror];if(q->neighbor!=s||q->mirror!=f)return netInvalid(__LINE__);}
        for(int v=0;v<p->count;v++)if(!isfinite(p->v[v].u)||!isfinite(p->v[v].v))return netInvalid(__LINE__);
    }}if(!edMeshValidate(m))return netInvalid(__LINE__);for(int i=0;i<m->meshCount;i++)for(int f=0;f<m->meshes[i].geometry.count;f++)if(m->meshes[i].geometry.f[f].material>=materials)return netInvalid(__LINE__);return edValidate(m);
}
static unsigned char *netDocument(const EdModel *source,uint32_t *length){
    if(!customResourceError.empty())return NULL;
    uint32_t modelBytes=0,resourceBytes=resources?(uint32_t)strlen(resources)+1:1;
    unsigned char *encoded=edWireEncode(source,&modelBytes);if(!encoded)return NULL;
    char *scripts[ED_COGS]={0};size_t size=sizeof(EditorNetDocument)+(size_t)source->objectCount*4+modelBytes+paletteCount*128u+resourceBytes+customResourceBytes.size();
    for(int i=0;i<source->cogCount;i++){scripts[i]=cogSourceText(source->cogs[i].file);if(!scripts[i])goto fail;size+=4+strlen(scripts[i])+1;}
    if(size>ED_NET_LIMIT/2)goto fail;{unsigned char *data=edCalloc(1,size);if(!data)goto fail;EditorNetDocument h={0};h.layout=netLayout();h.modelBytes=modelBytes;h.materials=paletteCount;h.resources=resourceBytes;h.cogs=source->cogCount;h.editing=active;h.objects=source->objectCount;h.assets=(uint32_t)customResourceBytes.size();
    for(int i=0;i<ED_SPAWNS;i++){SithThing *t=jkPlayer_playerInfos[i].pLocalPlayer;h.players[i]=t?t->guid:-1;}

    unsigned char *p=data;for(size_t i=0;i<sizeof(h)/4;i++){uint32_t n;edCopy(&n,(unsigned char*)&h+i*4,4);edNetStore32(p+i*4,n);}p+=sizeof(h);
    for(int i=0;i<source->objectCount;i++){int index=objectThings[i];int guid=index>=0&&index<sithWorld_g_pCurrentWorld->numThingsLoaded?sithWorld_g_pCurrentWorld->aThings[index].guid:-1;edNetStore32(p,(uint32_t)guid);p+=4;}
    for(int i=0;i<paletteCount;i++)snprintf((char*)p+i*128,128,"%s",paletteNames[i][0]?baseName(paletteNames[i]):"dflt.mat");p+=paletteCount*128u;if(resources)edCopy(p,resources,resourceBytes);p+=resourceBytes;
    for(int i=0;i<source->cogCount;i++){uint32_t n=(uint32_t)strlen(scripts[i])+1;edNetStore32(p,n);p+=4;edCopy(p,scripts[i],n);p+=n;edFree(scripts[i]);}edCopy(p,customResourceBytes.data(),customResourceBytes.size());p+=customResourceBytes.size();edCopy(p,encoded,modelBytes);edFree(encoded);encoded=NULL;*length=(uint32_t)size;return data;
}
fail:edFree(encoded);for(int i=0;i<ED_COGS;i++)edFree(scripts[i]);return NULL;
}
static int netQueueState(EditorNetPeer *peer,int kind,int origin,uint32_t gesture,const EdModel *source){
    uint32_t rawLength=0,packedLength=0;unsigned char *raw=netDocument(source,&rawLength);if(!raw)return 0;
    unsigned char *packed=edNetPack(raw,rawLength,&packedLength);uint32_t base=0;
    if(peer->outBase){uint32_t deltaLength=0;unsigned char *delta=edNetDelta(peer->outBase,peer->outSize,raw,rawLength,&deltaLength);
        if(delta&&deltaLength<packedLength){netDeltaTransfers++;edFree(packed);packed=delta;packedLength=deltaLength;base=edNetHash(peer->outBase,peer->outSize);}else edFree(delta);}
    if(!packed){edFree(raw);return 0;}netRawBytes+=rawLength;netPackedBytes+=packedLength;netFreeStream(&peer->tx);peer->tx.bytes=packed;peer->tx.rawBytes=raw;
    peer->tx.h=EditorNetHeader{.kind=(uint32_t)kind,.id=++netSequence,.revision=netRevision,.gesture=gesture,.origin=(uint32_t)origin,.total=packedLength,.raw=rawLength,.hash=edNetHash(raw,rawLength),.base=base};return 1;
}
static void netFullState(EditorNetPeer *peer,int origin){edFree(peer->outBase);peer->outBase=NULL;peer->outSize=0;netQueueState(peer,EN_STATE,origin,0,model);}

static void netBroadcast(int origin,uint32_t gesture){for(int i=0;i<32;i++)if(netPeers[i].player){EditorNetPeer *p=&netPeers[i];if(p->tx.bytes||(int)sithPlayer_GetPlayerNum(p->player)<0){p->dirty=1;p->dirtyOrigin=origin;p->dirtyGesture=gesture;}else netQueueState(p,EN_STATE,origin,gesture,model);}}
static void netCommitted(void){if(netEnabled()&&sithNet_isServer){netRevision++;netBroadcast(stdComm_dplayIdSelf,netGesture);}}
static void netRemember(void){if(!netApplying)netGesture++;}
static int netApplyLocal(const EdModel *source){
    if(source!=candidate)edCopyModel(candidate,source);
    int previous=netApplying;netApplying=1;pendingUndo=0;int ok=commit();netApplying=previous;return ok;
}
static void netCancelPrediction(void){
    edFree(netQueued);netQueued=NULL;mapInvalidate();builderPicks.count=0;
    grabbing=groupGrab=cleaving=objectGrabbing=objectRotating=lightGrabbing=0;hover.cell=-1;picks.count=0;
    if(netConfirmed&&netHadState){netApplyLocal(netConfirmed);SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;
        if(!active&&player&&objectSector(model,vec(player->position))<0){nextSpawn=0;teleportWalkplayer(player);}}
}
static int netPropose(const EdModel *proposal){
    if(!netReady||!netConfirmed){say("Waiting for the host's level data.");return 0;}
    if((netBusy||netQueued)&&netQueuedGesture!=netGesture){say("Wait for the host to finish the previous edit.");return 0;}
    if(!edValidate(proposal))return 0;
    EdModel *queued=netQueued?netQueued:edAlloc(sizeof(*queued));if(!queued)return 0;
    if(!netApplyLocal(proposal)){if(!netQueued)edFree(queued);return 0;}
    netQueued=queued;edCopyModel(netQueued,model);netQueuedGesture=netGesture;
    say("Edit previewed; checking with host.");return 1;
}
static int netHistory(int redo){if(!netReady||netBusy||netQueued){say("Wait for the host to finish the pending edit.");return 0;}EditorNetPeer *p=netPeer(sithNet_serverNetId,1);if(!p)return 0;netCommand=++netSequence;netBusy=1;p->tx.h=EditorNetHeader{.kind=(uint32_t)(redo?EN_REDO:EN_UNDO),.id=netCommand,.revision=netRevision};p->tx.time=0;say(redo?"Redo requested from host.":"Undo requested from host.");return 1;}
static void netReject(EditorNetPeer *p){netSend(p->player,EditorNetHeader{.kind=EN_REJECT,.revision=netRevision},NULL);netFullState(p,0);}
static int netApplyDocument(EditorNetPeer *peer,EditorNetHeader h,const unsigned char *raw,uint32_t length){
    /* A retry can use a new transfer ID for an already-applied revision. It
       must not cancel a newer pending client request or an ongoing grab. */
    if(!sithNet_isServer&&netReady&&h.revision==netRevision&&h.origin!=0)return 1;
    netFailureReason[0]=0;
    if(length<sizeof(EditorNetDocument))return 0;EditorNetDocument d;for(size_t i=0;i<sizeof(d)/4;i++){uint32_t n=edNetLoad32(raw+i*4);edCopy((unsigned char*)&d+i*4,&n,4);}if(d.layout!=netLayout()||d.modelBytes<4||d.modelBytes>ED_NET_LIMIT/2||d.materials<1||d.materials>EDITOR_MATERIALS||d.resources<1||d.resources>4000001||d.cogs>ED_COGS||d.objects>ED_OBJECTS)return 0;
    size_t prefix=sizeof(d)+(size_t)d.objects*4+(size_t)d.materials*128u+d.resources;if(d.assets<8||d.assets>EdResources::MaxBytes+EdResources::MaxFiles*135||d.modelBytes>length||d.assets>length-d.modelBytes||prefix>length-d.modelBytes-d.assets)return 0;size_t modelOffset=length-d.modelBytes;
    EdModel *incoming=edCalloc(1,sizeof(*incoming));if(!incoming)return 0;if(!edWireDecode(incoming,raw+modelOffset,d.modelBytes)){edFree(incoming);return 0;}
    const char (*names)[128]=(const char (*)[128])(raw+sizeof(d)+(size_t)d.objects*4);const char *manifest=(const char*)names+d.materials*128u;const unsigned char *p=raw+prefix,*end=raw+modelOffset-d.assets;EdResources incomingResources;
    bool sameAssets=customResourceBytes.size()==d.assets&&!memcmp(customResourceBytes.data(),end,d.assets);
    if(sithNet_isServer&&!sameAssets){edFree(incoming);netReject(peer);return 1;}
    if(!sameAssets&&!EdResources::decode(end,d.assets,incomingResources)){edFree(incoming);return 0;}
    EdResources previousResources=customResources;bool resourcesChanged=false;char *scripts[ED_COGS]={0};int ok=0;
{
    if(incoming->count<1||incoming->count>ED_CELLS||incoming->cogCount!=(int)d.cogs||incoming->objectCount!=(int)d.objects||manifest[d.resources-1]||strlen(manifest)!=d.resources-1){netInvalid(__LINE__);goto done;}
    for(unsigned int i=0;i<d.materials;i++)if(!netFilename(names[i],128)){if(getenv("GEOSITH_NET_ROLE"))printf("NET bad material %u: %.127s\n",i,names[i]);netInvalid(__LINE__);goto done;}
    for(unsigned int i=0;i<d.cogs;i++){uint32_t n;if(end-p<4){netInvalid(__LINE__);goto done;}n=edNetLoad32(p);p+=4;if(n<1||n>2*1024*1024||(size_t)(end-p)<n||p[n-1]||strlen((const char*)p)!=n-1){netInvalid(__LINE__);goto done;}
        EdPlacedCog parsed;if(!edCogParseScript(&parsed,incoming->cogs[i].file,(const char*)p)||parsed.count!=incoming->cogs[i].count){netInvalid(__LINE__);goto done;}
        for(int j=0;j<parsed.count;j++)if(parsed.params[j].type!=incoming->cogs[i].params[j].type||strcmp(parsed.params[j].name,incoming->cogs[i].params[j].name)){netInvalid(__LINE__);goto done;}
        scripts[i]=edAlloc(n);if(!scripts[i]){netInvalid(__LINE__);goto done;}edCopy(scripts[i],p,n);p+=n;
    }if(p!=end){netInvalid(__LINE__);goto done;}
    if(sithNet_isServer&&(h.revision!=netRevision||!resources||strcmp(resources,manifest))){netReject(peer);ok=1;{netInvalid(__LINE__);goto done;}}
    if(!sithNet_isServer&&!sameAssets){
        setCustomResources(std::move(incomingResources));resourcesChanged=true;resourceForceMaterials=true;liveColormap[0]=0;
        for(int i=0;i<paletteCount;i++){if(paletteOwned[i]&&palette[i])retiredResourceMaterials.push_back(palette[i]);palette[i]=NULL;paletteOwned[i]=0;}
        cogSourceCount=0;
    }
    if(!sithNet_isServer&&(!netHadState||!resources||strcmp(resources,manifest))&&!loadMissingManifestTemplates(manifest)){netInvalid(__LINE__);goto done;}
    if(!netValidModel(incoming,d.materials)){netInvalid(__LINE__);goto done;}
    int map[EDITOR_MATERIALS];for(unsigned int i=0;i<d.materials;i++){map[i]=addMaterialName(names[i]);if(map[i]<0){netInvalid(__LINE__);goto done;}}
    for(int s=0;s<incoming->count;s++)for(int f=0;f<incoming->c[s].count;f++){int id=map[incoming->c[s].f[f].material];if(!editorMaterial(id)){netInvalid(__LINE__);goto done;}incoming->c[s].f[f].material=id;}
    char *newResources=NULL;if(!sithNet_isServer){newResources=edAlloc(d.resources);if(!newResources){netInvalid(__LINE__);goto done;}edCopy(newResources,manifest,d.resources);}
    /* Cache scripts only after the whole document and its exposed symbols pass
       validation. ensureCogSource never overwrites an existing loose script. */
    for(int i=0;i<netCogCount;i++)edFree(netCogTexts[i]);netCogCount=incoming->cogCount;
    for(int i=0;i<netCogCount;i++){snprintf(netCogNames[i],32,"%s",incoming->cogs[i].file);netCogTexts[i]=scripts[i];scripts[i]=NULL;}
    for(int i=0;i<netCogCount;i++)if(!ensureCogSource(netCogNames[i])){edFree(newResources);{netInvalid(__LINE__);goto done;}}
    if(newResources){edFree(resources);resources=newResources;}
    int initial=!netHadState;if(!sithNet_isServer&&initial&&d.editing){active=1;netNeedsEditorSetup=1;}
    /* Joining uses a temporary bootstrap GOB. Offer the host's authored name
       when saving a copy, rather than that internal bootstrap filename. */
    if(!sithNet_isServer&&initial&&strlen(incoming->header.title)<32){char16_t proposed[32];char filename[32];stdString_CharToWchar(proposed,incoming->header.title,32);if(editorLevelFilename(proposed,filename))stdString_SafeWStrCopy(levelName,proposed,32);}
    netApplying=1;
    if(sithNet_isServer){if(peer->gesture!=h.gesture){remember();peer->gesture=h.gesture;}edCopyModel(candidate,incoming);ok=commit();if(ok){mapInvalidate();builderPicks.count=0;grabbing=groupGrab=cleaving=objectGrabbing=objectRotating=lightGrabbing=0;netRevision++;netBroadcast(peer->player,h.gesture);}}
    else{int own=netHadState&&h.origin==(uint32_t)stdComm_dplayIdSelf;
        if(!netConfirmed)netConfirmed=edAlloc(sizeof(*netConfirmed));
        /* An older accepted drag sample must not replace the newer sample
           already shown locally. Send that sample against the new revision. */
        int keepPreview=own&&netQueued&&netQueuedGesture==h.gesture;
        ok=netConfirmed&&(keepPreview||netApplyLocal(incoming));if(ok){edCopyModel(netConfirmed,incoming);netRevision=h.revision;netReady=netHadState=1;netValidationFailures=netFatal=0;netBusy=0;netWaiting=0;
        /* Approval proves this proposal arrived, even if its final stream ACK
           was lost. Retain that exact raw baseline before releasing the TX. */
        if(own&&peer->tx.rawBytes&&peer->tx.h.kind==EN_EDIT&&peer->tx.h.gesture==h.gesture){edFree(peer->outBase);peer->outBase=peer->tx.rawBytes;peer->outSize=peer->tx.h.raw;peer->tx.rawBytes=NULL;if(getenv("GEOSITH_NET_ROLE"))printf("GEOSITH_NET_ACK_LOSS PASS: approval confirmed an unacknowledged proposal baseline\n");}
        netFreeStream(&peer->tx);clearHistory();
        if(!own){edFree(peer->outBase);peer->outBase=NULL;peer->outSize=0;mapInvalidate();builderPicks.count=0;edFree(netQueued);netQueued=NULL;grabbing=groupGrab=cleaving=objectGrabbing=objectRotating=lightGrabbing=0;hover.cell=-1;picks.count=0;}
        for(int i=0;i<ED_SPAWNS;i++)if(d.players[i]>=0&&jkPlayer_playerInfos[i].pLocalPlayer)jkPlayer_playerInfos[i].pLocalPlayer->guid=d.players[i];
        for(int i=0;i<model->objectCount;i++){netObjectGuids[i]=(int32_t)edNetLoad32(raw+sizeof(d)+(size_t)i*4);if(netObjectGuids[i]>=0&&objectThings[i]>=0)sithWorld_g_pCurrentWorld->aThings[objectThings[i]].guid=netObjectGuids[i];}
        SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;if(player&&(initial||(!active&&objectSector(model,vec(player->position))<0))){EdSpawn spawn=model->spawns[playerThingIdx%model->spawnCount];player->position=rdvec(spawn.position);rdVector3 angles=rdvec(spawn.rotation);rdMatrix_BuildRotate34(&player->orient,&angles);rdVector_Zero3(&player->actorParams.headPYR);sithThing_SetSector(player,&sithWorld_g_pCurrentWorld->aSectors[objectSector(model,spawn.position)],1);}
        if(!h.origin)netRejections++;say(!h.origin?"Host rejected the edit; level synchronized.":own?"Host accepted the edit.":"Level synchronized from host.");}}
    netApplying=0;
    /* Joining applies the snapshot inside the synchronization menu. It has no
       stale attribute draft to discard and must stay open until DSS_WELCOME. */
    if(ok&&jkMain_bInModalGui&&!(g_submodeFlags&8)&&!netSafeDialog){jkGuiRend_CancelActiveMenu();say("The level changed while this draft was open. Reopen it to edit the current level.");}
}
done:if(!ok&&resourcesChanged)setCustomResources(std::move(previousResources));if(ok)materialMetadata();for(int i=0;i<ED_COGS;i++)edFree(scripts[i]);edFree(incoming);return ok;
}
static char *netCogSource(const char *name){for(int i=0;i<netCogCount;i++)if(!__strcmpi(netCogNames[i],name)){size_t n=strlen(netCogTexts[i])+1;char *copy=edAlloc(n);if(copy)edCopy(copy,netCogTexts[i],n);return copy;}return NULL;}
static void netControlReady(SithThing *player){if(!netNeedsEditorSetup||!netClient()||(g_submodeFlags&8))return;netNeedsEditorSetup=0;savedPhysics=player->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=player->flags;player->flags|=SITH_TF_INVULN;setEditorFlight(player,0);for(int i=1;i<=10;i++){sithInventory_SetInventoryAvailable(player,i,1);sithInventory_SetInventory(player,i,100);}sithWeapon_SelectWeapon(player,tool,0);sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);}
int jkEditor_NetMessage(SithMessage *message)try {
    if(!netEnabled()||message->netMsg.msg_size<sizeof(EditorNetHeader)||message->netMsg.msg_size>sizeof(message->pktData))return 0;
    EditorNetHeader h;for(size_t i=0;i<sizeof(h)/4;i++){uint32_t n=edNetLoad32((unsigned char*)message->pktData+i*4);edCopy((unsigned char*)&h+i*4,&n,4);}int sender=message->netMsg.idx;
    if(h.magic!=EN_MAGIC||h.version!=EN_VERSION||sender<=0||sender==stdComm_dplayIdSelf||h.length>EN_CHUNK||message->netMsg.msg_size!=sizeof(h)+h.length)return 0;
    if(!sithNet_isServer&&sender!=sithNet_serverNetId)return 0;
    /* The host waits for this transfer before welcoming the client. Bind the
       level here even though welcome has not assigned a local player yet. */
    if(!bindEditorWorld())return 0;
    EditorNetPeer *peer=netPeer(sender,sithNet_isServer?(h.kind==EN_HELLO):1);if(!peer)return 0;
#ifdef PLATFORM_GNS
    if(sithNet_isServer&&!stdComm_GNS_EditsAllowed()&&(h.kind==EN_EDIT||h.kind==EN_UNDO||h.kind==EN_REDO)){if(h.kind!=EN_EDIT||h.offset==0)netReject(peer);return 1;}
#endif
    if(h.kind==EN_ACK){EditorNetStream *tx=&peer->tx;if(tx->bytes&&h.id==tx->h.id&&h.offset>tx->at&&h.offset<=tx->sent){tx->at=h.offset;tx->time=0;if(tx->at==tx->h.total){peer->ready=tx->h.revision;edFree(peer->outBase);peer->outBase=tx->rawBytes;peer->outSize=tx->h.raw;tx->rawBytes=NULL;netFreeStream(tx);if(peer->dirty&&(int)sithPlayer_GetPlayerNum(peer->player)>=0){peer->dirty=0;netQueueState(peer,EN_STATE,peer->dirtyOrigin,peer->dirtyGesture,model);}}}return 1;}
    if(h.kind==EN_HELLO){if(sithNet_isServer)netFullState(peer,stdComm_dplayIdSelf);return 1;}
    if(h.kind==EN_REJECT){if(!sithNet_isServer){edFree(peer->outBase);peer->outBase=NULL;peer->outSize=0;netRejections++;netReady=netBusy=0;netFreeStream(&peer->tx);netCancelPrediction();say("Host rejected the edit; preview reverted. Synchronizing the current level.");}return 1;}
    if(h.kind==EN_UNDO||h.kind==EN_REDO){if(!sithNet_isServer||(int)sithPlayer_GetPlayerNum(sender)<0)return 0;if(peer->lastCommand==h.id){if(!peer->tx.bytes)netQueueState(peer,EN_STATE,sender,0,model);return 1;}peer->lastCommand=h.id;
        if(h.revision!=netRevision){netReject(peer);return 1;}netApplying=1;int ok=historyStep(h.kind==EN_REDO);netApplying=0;if(ok){netRevision++;netBroadcast(sender,0);}else netReject(peer);return 1;}
    if((h.kind!=EN_EDIT||!sithNet_isServer)&&(h.kind!=EN_STATE||sithNet_isServer))return 0;
    if(h.kind==EN_EDIT&&(int)sithPlayer_GetPlayerNum(sender)<0)return 0;if(h.kind==EN_STATE&&netHadState&&h.revision<netRevision)return 0;
    if(h.total<1||h.total>ED_NET_LIMIT||h.raw<sizeof(EditorNetDocument)||h.raw>ED_NET_LIMIT/2||h.offset>h.total||h.length>h.total-h.offset||!h.length)return 0;
    EditorNetStream *rx=&peer->rx;if(!rx->bytes||rx->h.id!=h.id){if(h.offset)return 0;netFreeStream(rx);size_t receiving=0;for(int i=0;i<32;i++)if(netPeers[i].rx.bytes)receiving+=netPeers[i].rx.h.total;if(receiving+h.total>ED_NET_LIMIT)return 0;rx->bytes=edAlloc(h.total);if(!rx->bytes)return 0;rx->h=h;}
    if(rx->h.total!=h.total||rx->h.raw!=h.raw||rx->h.hash!=h.hash||rx->h.revision!=h.revision||rx->h.gesture!=h.gesture||rx->h.origin!=h.origin||rx->h.base!=h.base)return 0;
    if(h.offset==rx->at){edCopy(rx->bytes+rx->at,(unsigned char*)message->pktData+sizeof(h),h.length);rx->at+=h.length;}rx->time=stdPlatform_GetTimeMsec();
    if(rx->at==h.total&&!rx->sent){rx->sent=1;unsigned char *raw=edNetUnpack(rx->bytes,h.total,h.raw);
        if(raw&&h.base){if(!peer->inBase||edNetHash(peer->inBase,peer->inSize)!=h.base){edFree(raw);raw=NULL;}else for(uint32_t i=0;i<h.raw;i++)raw[i]^=i<peer->inSize?peer->inBase[i]:0;}
        int ok=raw&&edNetHash(raw,h.raw)==h.hash&&netApplyDocument(peer,h,raw,h.raw);if(ok){edFree(peer->inBase);peer->inBase=raw;peer->inSize=h.raw;raw=NULL;}edFree(raw);if(!ok){if(sithNet_isServer)netReject(peer);else{netReady=0;if(++netValidationFailures>=3){netFatal=1;say(netFailureReason[0]?netFailureReason:"Level synchronization failed repeatedly. Check matching game files and GeoSith versions.");}else say("Level validation failed; requesting a complete fresh copy.");}netFreeStream(rx);return 0;}}
    netSend(sender,EditorNetHeader{.kind=EN_ACK,.id=h.id,.offset=rx->at},NULL);return 1;
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
void jkEditor_NetTick(void)try {if(!netEnabled()||!bindEditorWorld())return;uint32_t now=stdPlatform_GetTimeMsec();
    if(!sithNet_isServer){EditorNetPeer *p=netPeer(sithNet_serverNetId,1);if(!p)return;
        if(!netReady&&!netFatal&&now-netLastHello>2000&&(!p->rx.bytes||now-p->rx.time>2000)){netLastHello=now;netSend(p->player,EditorNetHeader{.kind=EN_HELLO},NULL);}
        if(netReady&&!netBusy&&netQueued){EdModel *q=netQueued;netQueued=NULL;if(netQueueState(p,EN_EDIT,stdComm_dplayIdSelf,netQueuedGesture,q))netBusy=1;
            else{netReady=0;netCancelPrediction();say("Could not submit the edit; preview reverted. Synchronizing with host.");}edFree(q);}
        if(netBusy&&!p->tx.bytes&&(!p->rx.bytes||p->rx.sent)&&(p->tx.h.kind==EN_UNDO||p->tx.h.kind==EN_REDO)&&(!p->tx.time||now-p->tx.time>500)){netSend(p->player,p->tx.h,NULL);p->tx.time=now;}
    }
    for(int i=0;i<32;i++){EditorNetPeer *p=&netPeers[i];EditorNetStream *tx=&p->tx;if(!p->player)continue;
        if(sithNet_isServer&&p->dirty&&!tx->bytes&&(int)sithPlayer_GetPlayerNum(p->player)>=0){p->dirty=0;netQueueState(p,EN_STATE,p->dirtyOrigin,p->dirtyGesture,model);}
        if(tx->bytes&&(!tx->time||now-tx->time>300)){tx->time=now;for(uint32_t offset=tx->at,n=0;offset<tx->h.total&&n<8;n++){EditorNetHeader h=tx->h;h.offset=offset;h.length=h.total-offset;if(h.length>EN_CHUNK)h.length=EN_CHUNK;if(!netSend(p->player,h,tx->bytes+offset))break;offset+=h.length;if(offset>tx->sent)tx->sent=offset;}}
        if(p->rx.bytes&&now-p->rx.time>30000)netFreeStream(&p->rx);
    }
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return;}
int jkEditor_NetJoinRequest(int player)try {if(!netEnabled()||!sithNet_isServer||!bindEditorWorld())return 0;EditorNetPeer *p=netPeer(player,1);if(!p)return 1;if(p->ready>=0)return 0;sithMulti_SyncPlayers(player,0);if(!p->tx.bytes)netQueueState(p,EN_STATE,stdComm_dplayIdSelf,0,model);return 1;} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
void jkEditor_NetPlayerLeft(int player)try {EditorNetPeer *p=netPeer(player,0);if(!p)return;netFreeStream(&p->tx);netFreeStream(&p->rx);edFree(p->outBase);edFree(p->inBase);edZero(p,0,sizeof(*p));} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return;}
static void netClose(void){netFatal=netValidationFailures=0;netFailureReason[0]=0;netHadState=netNeedsEditorSetup=netRejections=0;netTestSend=NULL;edZero(netObjectGuids,0xff,sizeof(netObjectGuids));for(int i=0;i<32;i++){netFreeStream(&netPeers[i].tx);netFreeStream(&netPeers[i].rx);edFree(netPeers[i].outBase);edFree(netPeers[i].inBase);}edZero(netPeers,0,sizeof(netPeers));for(int i=0;i<netCogCount;i++)edFree(netCogTexts[i]);netCogCount=0;edFree(netQueued);netQueued=NULL;edFree(netConfirmed);netConfirmed=NULL;netRevision=netLastHello=netCommand=0;netGesture=netSequence=1;netReady=netWaiting=netBusy=netApplying=netAuthoredSpawns=0;}
int jkEditor_NetEpisode(const char *episode)try {if(stockEpisode(episode))return 1;if(!episode||!*episode)return 0;char path[160];snprintf(path,sizeof(path),"Episode/%s%s",episode,strchr(episode,'.')?"":".gob");FILE *f=fopen(path,"rb");if(!f)return 0;uint32_t header[3],count;int found=0;
    if(fread(header,4,3,f)==3&&header[0]==0x20424f47&&header[1]==20&&!fseek(f,header[2],SEEK_SET)&&fread(&count,4,1,f)==1&&count<4096){stdGobDiskEntry e;for(uint32_t i=0;i<count;i++)if(fread(&e,1,sizeof(e),f)==sizeof(e)&&netString(e.fname,128)&&!__strcmpi(e.fname,"misc\\sithed_editor.txt")){found=1;break;}}fclose(f);return found;
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
int jkEditor_NetPrepareJoin(char *episode,const char *map,int flags)try {if(!(flags&JKEDITOR_NET_FLAG))return 1;if(!netFilename(map,32)||!readResources())return 0;EdModel *fresh=edAlloc(sizeof(*fresh));if(!fresh)return 0;edDefault(fresh);fresh->spawnCount=ED_SPAWNS;for(int i=0;i<ED_SPAWNS;i++)fresh->spawns[i]=EdSpawn{fresh->spawn,edV(0,0,0)};
    edEpisodeDefault(&fresh->episode,"Editor multiplayer connection",1);snprintf(fresh->episode.rows[0].file,32,"%s",map);char path[128],name[32];int slot;for(slot=1;slot<10000;slot++){snprintf(name,32,"netedit%04d",slot);snprintf(path,128,"Episode/%s.gob",name);FILE *f=fopen(path,"rb");if(!f)break;fclose(f);}int ok=slot<10000&&edWriteGob(fresh,path,"Editor connection",1,resources);edFree(fresh);if(ok)snprintf(episode,32,"%s",name);return ok;
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
// Added: cap authored spawn count at the editor spawn-array capacity
void jkEditor_NetPreparePlayers(SithWorld *world)try {if(!sithNet_isMulti||!editorWorld(world))return;int count=0;SithThing *first=NULL;for(int i=0;i<world->numThingsLoaded;i++)if(world->aThings[i].type==SITH_THING_PLAYER){count++;if(!first)first=&world->aThings[i];}netAuthoredSpawns=count<ED_SPAWNS?count:ED_SPAWNS;if(!first)return;
    while(count<ED_SPAWNS){SithThing *p=sithThing_CreateThingAtPos(first->pTemplate,&first->position,&first->orient,first->sector,NULL);if(!p)break;p->flags|=SITH_TF_DISABLED;count++;}
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return;}
static int netSelfTest(void){
    SithMessage shortPacket={0};int checked=1;for(int n=0;n<12;n++){shortPacket.netMsg.msg_size=n;checked=checked&&!sithMulti_ProcessChat(&shortPacket);}
    shortPacket.netMsg.msg_size=12;int bad=-1;edCopy((unsigned char*)shortPacket.pktData+8,&bad,4);checked=checked&&!sithMulti_ProcessChat(&shortPacket);
    unsigned char data[1025];for(int i=0;i<1025;i++)data[i]=i%7?0:(unsigned char)(i+1);uint32_t length;unsigned char *packed=edNetPack(data,sizeof(data),&length),*copy=packed?edNetUnpack(packed,length,sizeof(data)):NULL;int ok=checked&&copy&&!edCompare(data,copy,sizeof(data));edFree(copy);if(packed){copy=edNetUnpack(packed,length,sizeof(data)-1);ok=ok&&!copy;edFree(copy);}edFree(packed);
    uint32_t n=0;unsigned char *doc=netDocument(model,&n);ok=ok&&doc&&n>sizeof(EditorNetDocument)&&netValidModel(model,paletteCount);edFree(doc);printf("GEOSITH_NET %s: bounded codec, document serialization and validation\n",ok?"PASS":"FAIL");fflush(stdout);return ok;
}

void jkEditor_NetProgress(char *text,int size)try {if(size<=0)return;text[0]=0;if(netFatal){snprintf(text,size,"Synchronization failed: %.160s",netFailureReason);return;}if(!netClient())return;EditorNetPeer *p=netPeer(sithNet_serverNetId,0);if(p&&p->rx.bytes&&p->rx.h.total)snprintf(text,size,"Receiving level + resources: %u%% (%u / %u KB)",(unsigned int)(100.0*p->rx.at/p->rx.h.total),(p->rx.at+1023)/1024,(p->rx.h.total+1023)/1024);} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return;}

int jkEditor_NetFailed(void)try {return netFatal;} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}

int jkEditor_NetSafeDialog(int safe)try {int previous=netSafeDialog;netSafeDialog=safe;return previous;} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
