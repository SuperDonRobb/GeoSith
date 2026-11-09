#include "stdComm_GNS.h"

#include "Win95/stdComm.h"
#include "Dss/sithMulti.h"
#include "General/stdString.h"
#include "stdPlatform.h"
#include "jk.h"

#include "SDL2_helper.h"

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <map>
#include <cctype>
#include <atomic>
#include <filesystem>
#include <vector>
#include <memory>
extern "C" {
#include "Main/Main.h"
#include "Main/jkEditor.h"
#include "Gameplay/sithPlayer.h"
}
#include "gnsAddress.h"
#include "gnsPosition.h"

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingmessages.h>
#include <steam/steamnetworkingtypes.h>
#include <steam/isteamnetworkingutils.h>
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h> // Ug, for NukeProcess -- see below
    #include <ws2tcpip.h>
    typedef int socklen_t;
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <sys/ioctl.h>
    typedef int SOCKET;
    constexpr SOCKET INVALID_SOCKET = -1;
    #include <signal.h>
    #include <dlfcn.h>
#endif

#define sithDplayGNS_infoPrintf(fmt, ...) stdPlatform_Printf(fmt, ##__VA_ARGS__)
#define sithDplayGNS_verbosePrintf(fmt, ...) if (Main_bVerboseNetworking) \
    { \
        stdPlatform_Printf(fmt, ##__VA_ARGS__);  \
    } \
    ;

extern "C"
{
void Hack_ResetClients();

static uint32_t GNSLayout(){return (uint32_t)(sizeof(jkMultiEntry)^sizeof(rdVector3)*17u^sizeof(flex_t)*31u);}
enum { GNS_MAGIC=0x47534954, GNS_VERSION=17, GNS_INFO=1, GNS_AUTH=2, GNS_ACCEPT=3 };
#pragma pack(push, 4)
struct GNSInfoPacket { uint32_t magic,version,kind,id,mode,layout; uint64_t assets; jkMultiEntry entry; };
struct GNSAuthPacket { uint32_t magic,version,kind,mode,layout; uint64_t assets; char16_t password[32]; };
#pragma pack(pop)
static jkMultiEntry sithDplayGNS_storedEntryEnum;
static jkMultiEntry sithDplayGNS_storedEntry;
extern char16_t jkGuiMultiplayer_ipText[256];
char jkGuiMultiplayer_ipText_conv[256];
static int sithDplayGNS_numEnumd = 0;
extern int Main_bVerboseNetworking;

typedef bool (*GameNetworkingSockets_Init_t)( const SteamNetworkingIdentity *pIdentity, SteamNetworkingErrMsg &errMsg );
typedef ISteamNetworkingUtils* (*SteamNetworkingUtils_t)(void);
typedef void (*GameNetworkingSockets_Kill_t)(void);
typedef ISteamNetworkingSockets* (*SteamNetworkingSockets_t)(void);
typedef void (*SteamNetworkingIPAddr_ToString_t)( const SteamNetworkingIPAddr *pAddr, char *buf, size_t cbBuf, bool bWithPort );
typedef bool (*SteamNetworkingIPAddr_ParseString_t)( SteamNetworkingIPAddr *pAddr, const char *pszStr );

GameNetworkingSockets_Init_t g_GameNetworkingSockets_Init = NULL;
SteamNetworkingUtils_t g_SteamNetworkingUtils = NULL;
GameNetworkingSockets_Kill_t g_GameNetworkingSockets_Kill = NULL;
SteamNetworkingSockets_t g_SteamNetworkingSockets = NULL;
SteamNetworkingIPAddr_ToString_t g_SteamNetworkingIPAddr_ToString = NULL;
SteamNetworkingIPAddr_ParseString_t g_SteamNetworkingIPAddr_ParseString = NULL;

}
/////////////////////////////////////////////////////////////////////////////
//
// Common stuff
//
/////////////////////////////////////////////////////////////////////////////

bool g_bQuit = false;

SteamNetworkingMicroseconds g_logTimeZero;

#ifdef WIN32
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h>   // for nanosleep
#endif

void sleep_ms(int milliseconds){ // cross-platform sleep function
    SDL_Delay(milliseconds);
}

// We do this because I won't want to figure out how to cleanly shut
// down the thread that is reading from stdin.
static void NukeProcess( int rc )
{
    #ifdef _WIN32
        ExitProcess( rc );
    #else
        (void)rc; // Unused formal parameter
        kill( getpid(), SIGKILL );
    #endif
}

static void DebugOutput( ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg )
{
    //SteamNetworkingMicroseconds time = g_SteamNetworkingUtils()->GetLocalTimestamp() - g_logTimeZero;
    //sithDplayGNS_verbosePrintf( "%10.6f %s\n", time*1e-6, pszMsg );
    if(eType<=k_ESteamNetworkingSocketsDebugOutputType_Warning)stdPlatform_Printf("GeoSith transport: %s\n",pszMsg);else sithDplayGNS_verbosePrintf("%s\n",pszMsg);
    fflush(stdout);
    if ( eType == k_ESteamNetworkingSocketsDebugOutputType_Bug )
    {
        fflush(stdout);
        fflush(stderr);
        //NukeProcess(1);
    }
}

static void FatalError( const char *fmt, ... )
{
    char text[ 2048 ];
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( text, sizeof(text), fmt, ap );
    va_end(ap);
    char *nl = strchr( text, '\0' ) - 1;
    if ( nl >= text && *nl == '\n' )
        *nl = '\0';
    DebugOutput( k_ESteamNetworkingSocketsDebugOutputType_Bug, text );
}

static void Printf( const char *fmt, ... )
{
    char text[ 2048 ];
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( text, sizeof(text), fmt, ap );
    va_end(ap);
    char *nl = strchr( text, '\0' ) - 1;
    if ( nl >= text && *nl == '\n' )
        *nl = '\0';
    DebugOutput( k_ESteamNetworkingSocketsDebugOutputType_Msg, text );
}

static int stdComm_GNS_bInitted = 0;
static int stdComm_GNS_bForceStubs = 0;
static int stdComm_GNS_bSymbolsLoaded = 0;

static void stdComm_GNS_LoadSymbols()
{
    if (stdComm_GNS_bSymbolsLoaded) return;

    static const char pszExportFunc[] = "GameNetworkingSockets_Init";

    #if defined( WIN32 )
        static const char pszModule[] = "libGameNetworkingSockets.dll";
        HMODULE h = ::LoadLibraryA( pszModule );
        if ( h == NULL ) h = ::LoadLibraryA("GameNetworkingSockets.dll");
        if ( h == NULL )
        {
            stdPlatform_Printf("Failed to load %s.\n", pszModule );
            stdComm_GNS_bForceStubs = 1;
            
            return;
        }
        g_GameNetworkingSockets_Init = (GameNetworkingSockets_Init_t)::GetProcAddress(h, "GameNetworkingSockets_Init");
        g_SteamNetworkingUtils = (SteamNetworkingUtils_t)::GetProcAddress(h, "SteamNetworkingUtils_LibV4");
        g_GameNetworkingSockets_Kill = (GameNetworkingSockets_Kill_t)::GetProcAddress(h, "GameNetworkingSockets_Kill");
        g_SteamNetworkingSockets = (SteamNetworkingSockets_t)::GetProcAddress(h, "SteamNetworkingSockets_LibV12");
        g_SteamNetworkingIPAddr_ToString = (SteamNetworkingIPAddr_ToString_t)::GetProcAddress(h, "SteamNetworkingIPAddr_ToString");
        g_SteamNetworkingIPAddr_ParseString = (SteamNetworkingIPAddr_ParseString_t)::GetProcAddress(h, "SteamNetworkingIPAddr_ParseString");
    #elif defined(LINUX) | defined(MACOS)
        #if defined(MACOS)
            static const char pszModule[] = "libGameNetworkingSockets.dylib";
        #else
            static const char pszModule[] = "libGameNetworkingSockets.so";
        #endif
        void* h = dlopen(pszModule, RTLD_LAZY);
        if ( h == NULL )
        {
            stdPlatform_Printf("Failed to dlopen %s.  %s\n", pszModule, dlerror() );
            stdComm_GNS_bForceStubs = 1;
            return;
        }
        g_GameNetworkingSockets_Init = (GameNetworkingSockets_Init_t)dlsym(h, "GameNetworkingSockets_Init");
        g_SteamNetworkingUtils = (SteamNetworkingUtils_t)dlsym(h, "SteamNetworkingUtils_LibV4");
        g_GameNetworkingSockets_Kill = (GameNetworkingSockets_Kill_t)dlsym(h, "GameNetworkingSockets_Kill");
        g_SteamNetworkingSockets = (SteamNetworkingSockets_t)dlsym(h, "SteamNetworkingSockets_LibV12");
        g_SteamNetworkingIPAddr_ToString = (SteamNetworkingIPAddr_ToString_t)dlsym(h, "SteamNetworkingIPAddr_ToString");
        g_SteamNetworkingIPAddr_ParseString = (SteamNetworkingIPAddr_ParseString_t)dlsym(h, "SteamNetworkingIPAddr_ParseString");
    #else
        
    #endif

    if (!g_SteamNetworkingIPAddr_ToString || !g_GameNetworkingSockets_Init 
        || !g_GameNetworkingSockets_Kill || !g_SteamNetworkingSockets 
        || !g_SteamNetworkingUtils || !g_SteamNetworkingIPAddr_ParseString)
    {
        stdPlatform_Printf("Failed to load networking symbols, reverting to stubs.\n");
        stdComm_GNS_bForceStubs = 1;
        return;
    }
    stdPlatform_Printf("Loaded %s successfully.\n", pszModule);

    stdComm_GNS_bSymbolsLoaded = 1;
}

static void InitSteamDatagramConnectionSockets()
{
    if (stdComm_GNS_bInitted) return;

    stdComm_GNS_LoadSymbols();

    #ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
        SteamDatagramErrMsg errMsg;
        if ( !g_GameNetworkingSockets_Init( nullptr, errMsg ) )
            {FatalError("GameNetworkingSockets_Init failed. %s",errMsg);stdComm_GNS_bForceStubs=1;return;}
    #else
        SteamDatagram_SetAppID( 570 ); // Just set something, doesn't matter what
        SteamDatagram_SetUniverse( false, k_EUniverseDev );

        SteamDatagramErrMsg errMsg;
        if ( !SteamDatagramClient_Init( errMsg ) )
            FatalError( "SteamDatagramClient_Init failed.  %s", errMsg );

        // Disable authentication when running with Steam, for this
        // example, since we're not a real app.
        //
        // Authentication is disabled automatically in the open-source
        // version since we don't have a trusted third party to issue
        // certs.
        g_SteamNetworkingUtils()->SetGlobalConfigValueInt32( k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 1 );
    #endif

    g_logTimeZero = g_SteamNetworkingUtils()->GetLocalTimestamp();

    g_SteamNetworkingUtils()->SetDebugOutputFunction( k_ESteamNetworkingSocketsDebugOutputType_Msg, DebugOutput );
    if(const char *lag=getenv("GEOSITH_NET_LAG_MS")){float value=(float)atof(lag);if(value>=0&&value<=5000){g_SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send,(int)value);g_SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Recv,(int)value);}}
    if(const char *loss=getenv("GEOSITH_NET_LOSS_PERCENT")){float value=(float)atof(loss);if(value>=0&&value<=40){g_SteamNetworkingUtils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send,value);g_SteamNetworkingUtils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Recv,value);}}
    if(getenv("GEOSITH_NET_ROLE")){
        bool bounded=g_SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_SendBufferSize,8192);
        stdPlatform_Printf("GEOSITH_GNS_SIMULATION %s: send/receive lag %s ms, packet loss %s%%, 8192-byte send buffer\n",bounded?"PASS":"FAIL",getenv("GEOSITH_NET_LAG_MS")?getenv("GEOSITH_NET_LAG_MS"):"0",getenv("GEOSITH_NET_LOSS_PERCENT")?getenv("GEOSITH_NET_LOSS_PERCENT"):"0");
        if(!bounded)exit(2);
    }
    stdComm_GNS_bInitted = 1;
}

static void ShutdownSteamDatagramConnectionSockets()
{
    if (!stdComm_GNS_bInitted) return;

    // Give connections time to finish up.  This is an application layer protocol
    // here, it's not TCP.  Note that if you have an application and you need to be
    // more sure about cleanup, you won't be able to do this.  You will need to send
    // a message and then either wait for the peer to close the connection, or
    // you can pool the connection to see if any reliable data is pending.
    sleep_ms( 500 );

    #ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
        g_GameNetworkingSockets_Kill();
    #else
        SteamDatagramClient_Kill();
    #endif

    stdComm_GNS_bInitted = 0;
}


/////////////////////////////////////////////////////////////////////////////
//
// GNSServer
//
/////////////////////////////////////////////////////////////////////////////

static std::atomic<bool> discoveryCancelled(false);
static std::mutex gnsStatusMutex;
static char gnsStatus[256]="Enter the host's public IP or hostname:port. Default: UDP 27020.";
static void NetworkStatus(const char *message) {
    std::lock_guard<std::mutex> lock(gnsStatusMutex);
    snprintf(gnsStatus,sizeof(gnsStatus),"%s",message);
    stdPlatform_Printf("GeoSith network: %s\n",message);
}
#include "gnsRouter.h"
static uint64_t gnsAssets;
static std::atomic<bool> allowClientEdits(true);
static bool HashAssets() {
    if(gnsAssets)return true;
    std::error_code ec;std::vector<std::filesystem::path> files;
    const char *folder=std::filesystem::is_directory("Resource",ec)?"Resource":"resource";ec.clear();
    if(!std::filesystem::is_directory(folder,ec)){NetworkStatus("Cannot find the game's Resource directory.");return false;}ec.clear();
    for(const auto &f:std::filesystem::directory_iterator(folder,ec)) {
        std::string ext=f.path().extension().string();for(char &c:ext)c=(char)tolower((unsigned char)c);
        if(ext==".gob"||ext==".goo")files.push_back(f.path());
    }
    if(ec||files.empty()){NetworkStatus("Cannot fingerprint Resource archives. Check your Jedi Knight installation.");return false;}
    /* Include loose asset overrides, but not episodes or shared COG sources.
       Levels/COGs are synchronized separately and may differ before joining. */
    for(const char *base:{"mat","3do","sprite","spr","sound","sfx","misc",folder}){
        ec.clear();if(!std::filesystem::is_directory(base,ec))continue;ec.clear();
        for(const auto &f:std::filesystem::recursive_directory_iterator(base,std::filesystem::directory_options::skip_permission_denied,ec)){
            if(!f.is_regular_file(ec))continue;
            std::string ext=f.path().extension().string();for(char &c:ext)c=(char)tolower((unsigned char)c);
            if((std::string(base)=="mods"&&(ext==".gob"||ext==".goo"))||ext==".mat"||ext==".3do"||ext==".spr"||ext==".cmp"||ext==".wav"||ext==".snd"||ext==".ai"||ext==".key"||ext==".pup"||ext==".par")files.push_back(f.path());
        }if(ec){NetworkStatus("Cannot read one of the game's asset directories.");return false;}
    }
    auto normalized=[](const std::filesystem::path &p){std::string name=p.generic_string();for(char &c:name)c=(char)tolower((unsigned char)c);return name;};
    std::sort(files.begin(),files.end(),[&](const auto &a,const auto &b){return normalized(a)<normalized(b);});files.erase(std::unique(files.begin(),files.end()),files.end());uint64_t hash=14695981039346656037ULL;
    auto add=[&hash](const unsigned char *p,size_t n){while(n--)hash=(hash^*p++)*1099511628211ULL;};
    unsigned char data[65536];
    for(const auto &file:files){std::string name=normalized(file);add((const unsigned char*)name.c_str(),name.size()+1);
        FILE *f=fopen(file.string().c_str(),"rb");if(!f)return false;size_t n;while((n=fread(data,1,sizeof(data),f)))add(data,n);int bad=ferror(f);fclose(f);if(bad)return false;}
    gnsAssets=hash?hash:1;return true;
}
static void ConfigureLanes(HSteamNetConnection connection) {
    int priorities[2]={10,0};uint16 weights[2]={1,1};
    g_SteamNetworkingSockets()->ConfigureConnectionLanes(connection,2,priorities,weights);
}
static bool SendTransport(HSteamNetConnection connection,const void *data,uint32_t length,bool envelope=true) {
    if(!connection||length>4096||!length)return false;
    int lane=0,flags=k_nSteamNetworkingSend_ReliableNoNagle;
    if(envelope&&length>=12){uint32_t type;memcpy(&type,(const char*)data+8,4);type&=255;
        if(type==JKEDITOR_NET_MESSAGE&&length>=24){uint32_t kind;memcpy(&kind,(const char*)data+20,4);if(kind==1||kind==2)lane=1;}
        else if(type==DSS_THINGPOS)flags=k_nSteamNetworkingSend_UnreliableNoNagle;}
    SteamNetworkingMessage_t *message=g_SteamNetworkingUtils()->AllocateMessage(length);if(!message)return false;
    memcpy(message->m_pData,data,length);message->m_conn=connection;message->m_nFlags=flags;message->m_idxLane=lane;
    int64 result=0;g_SteamNetworkingSockets()->SendMessages(1,&message,&result);
    if(result<0){if(result!=-k_EResultLimitExceeded)stdPlatform_Printf("GeoSith network send failed: %lld\n",(long long)result);return false;}return true;
}
static GNSInfoPacket PublicInfo(uint32_t kind,uint32_t id) {
    GNSInfoPacket info={};info.magic=GNS_MAGIC;info.version=GNS_VERSION;info.kind=kind;info.id=id;
    info.mode=Main_bMotsCompat;info.layout=GNSLayout();info.assets=gnsAssets;info.entry=sithDplayGNS_storedEntry;
    memset(info.entry.wPassword,0,sizeof(info.entry.wPassword));info.entry.numPlayers=0;
    for(int i=0;i<jkPlayer_maxPlayers;i++)if(jkPlayer_playerInfos[i].playerNetId&&(jkPlayer_playerInfos[i].flags&1))info.entry.numPlayers++;
    info.entry.multiModeFlags=sithMulti_multiModeFlags;return info;
}
#include "gnsLan.h"
class GNSServer {
    struct Peer { uint32_t id=0;uint32_t started=0;std::map<uint64_t,int64_t> positions; };
    HSteamListenSocket listener=k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup group=k_HSteamNetPollGroup_Invalid;
    std::map<HSteamNetConnection,Peer> peers;std::queue<int> disconnected;
    static GNSServer *instance;
    void Drop(HSteamNetConnection conn,const char *reason) {
        auto it=peers.find(conn);if(it!=peers.end()){
            if(it->second.id){jkEditor_NetPlayerLeft(it->second.id);if((int)sithPlayer_GetPlayerNum(it->second.id)>=0)disconnected.push(it->second.id);}
            peers.erase(it);
        }
        g_SteamNetworkingSockets()->CloseConnection(conn,1000,reason,false);
    }
    static void Callback(SteamNetConnectionStatusChangedCallback_t *event){if(instance)instance->Changed(event);}
    void Changed(SteamNetConnectionStatusChangedCallback_t *event) {
        auto api=g_SteamNetworkingSockets();
        if(event->m_info.m_eState==k_ESteamNetworkingConnectionState_Connecting){
            if(event->m_info.m_hListenSocket!=listener)return;
            if(peers.size()>=64){api->CloseConnection(event->m_hConn,1000,"Too many pending connections",false);return;}
            if(api->AcceptConnection(event->m_hConn)!=k_EResultOK||!api->SetConnectionPollGroup(event->m_hConn,group)){Drop(event->m_hConn,"Cannot accept connection");return;}
            ConfigureLanes(event->m_hConn);peers[event->m_hConn]={0,stdPlatform_GetTimeMsec()};
            GNSInfoPacket info=PublicInfo(GNS_INFO,0);if(!SendTransport(event->m_hConn,&info,sizeof(info),false))Drop(event->m_hConn,"Cannot send server information");
        } else if(event->m_info.m_eState==k_ESteamNetworkingConnectionState_ClosedByPeer||event->m_info.m_eState==k_ESteamNetworkingConnectionState_ProblemDetectedLocally){
            if(peers.find(event->m_hConn)!=peers.end()){stdPlatform_Printf("GeoSith connection ended: %s\n",event->m_info.m_szEndDebug);Drop(event->m_hConn,event->m_info.m_szEndDebug);}
            // Added: close connections that fail before entering the accepted-peer map
            else api->CloseConnection(event->m_hConn,0,nullptr,false);
        }
    }
    void Authenticate(HSteamNetConnection conn,const void *data,int length) {
        auto it=peers.find(conn);if(it==peers.end())return;
        GNSAuthPacket auth={};if(length!=sizeof(auth)){Drop(conn,"Incompatible GeoSith network protocol. Install the same version.");return;}memcpy(&auth,data,sizeof(auth));
        if(auth.magic!=GNS_MAGIC||auth.version!=GNS_VERSION||auth.kind!=GNS_AUTH||auth.layout!=GNSLayout()){Drop(conn,"Incompatible GeoSith version");return;}
        if(auth.mode!=(uint32_t)Main_bMotsCompat||auth.assets!=gnsAssets){Drop(conn,"Jedi Knight resources or loose assets differ from the host");return;}
        unsigned int mismatch=0;for(int i=0;i<32;i++)mismatch|=(unsigned int)(auth.password[i]^sithDplayGNS_storedEntry.wPassword[i]);
        if(mismatch){Drop(conn,"Incorrect game password");return;}
        unsigned int next=0;for(unsigned int id=2;id<=32;id++){bool used=false;for(const auto &p:peers)if(p.second.id==id)used=true;if(!used){next=id;break;}}
        int authenticated=1;for(const auto &p:peers)if(p.second.id)authenticated++;
        if(!next||authenticated>=sithDplayGNS_storedEntry.maxPlayers){Drop(conn,"The game is full");return;}
        it->second.id=next;GNSInfoPacket info=PublicInfo(GNS_ACCEPT,next);
        if(!SendTransport(conn,&info,sizeof(info),false))Drop(conn,"Cannot complete authentication");
    }
public:
    uint32_t id=1;
    bool Init(uint16 port) {
        if(!HashAssets())return false;auto api=g_SteamNetworkingSockets();
        /* GNS can share an underlying UDP socket between listen handles. Do
           an exclusive bind probe so a second game cannot silently steal it. */
        SOCKET probe=socket(AF_INET6,SOCK_DGRAM,IPPROTO_UDP);int available=0;
        if(probe==INVALID_SOCKET){probe=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(probe!=INVALID_SOCKET){
#ifdef _WIN32
            int exclusive=1;setsockopt(probe,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char*)&exclusive,sizeof(exclusive));
#endif
            sockaddr_in local={};local.sin_family=AF_INET;local.sin_port=htons(port);available=bind(probe,(sockaddr*)&local,sizeof(local))==0;closesocket(probe);probe=INVALID_SOCKET;
        }}
        if(probe!=INVALID_SOCKET){int zero=0;setsockopt(probe,IPPROTO_IPV6,IPV6_V6ONLY,(const char*)&zero,sizeof(zero));
#ifdef _WIN32
            int exclusive=1;setsockopt(probe,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char*)&exclusive,sizeof(exclusive));
#endif
            sockaddr_in6 local={};local.sin6_family=AF_INET6;local.sin6_port=htons(port);available=bind(probe,(sockaddr*)&local,sizeof(local))==0;closesocket(probe);
        }
        if(!available){char text[256];snprintf(text,sizeof(text),"Cannot host on UDP %u. The port is unavailable or already in use.",port);NetworkStatus(text);return false;}
        instance=this;
        SteamNetworkingIPAddr address;address.Clear();address.m_port=port;
        SteamNetworkingConfigValue_t options[2];options[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,(void*)Callback);options[1].SetInt32(k_ESteamNetworkingConfig_TimeoutInitial,15000);
        listener=api->CreateListenSocketIP(address,2,options);
        if(!listener){char text[256];snprintf(text,sizeof(text),"Cannot host on UDP %u. The port may already be in use.",port);NetworkStatus(text);return false;}
        group=api->CreatePollGroup();if(!group){api->CloseListenSocket(listener);listener=0;NetworkStatus("Cannot allocate a network poll group.");return false;}
        std::queue<int>().swap(disconnected);char text[256];snprintf(text,sizeof(text),"Hosting on UDP %u. Friends need your public IP and this port; allow GeoSith through your firewall.",port);NetworkStatus(text);LanStart(port);StartRouterMapping(port);return true;
    }
    void Shutdown(){StopRouterMapping();LanStop();auto api=g_SteamNetworkingSockets();for(const auto &p:peers)api->CloseConnection(p.first,1000,"Host ended the game",false);peers.clear();if(listener)api->CloseListenSocket(listener);if(group)api->DestroyPollGroup(group);listener=0;group=0;std::queue<int>().swap(disconnected);}
    void RunStep(){instance=this;g_SteamNetworkingSockets()->RunCallbacks();LanPoll();std::vector<HSteamNetConnection> expired;uint32_t now=stdPlatform_GetTimeMsec();for(const auto &p:peers)if(!p.second.id&&now-p.second.started>15000)expired.push_back(p.first);for(auto conn:expired)Drop(conn,"Authentication timed out");}
    int Receive(int *sender,void *data,int *length) {
        int capacity=*length;*length=0;*sender=0;
        if(!disconnected.empty()){*sender=disconnected.front();disconnected.pop();return 2;}
        for(int processed=0;processed<64;processed++){
            SteamNetworkingMessage_t *message=nullptr;int count=g_SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(group,&message,1);if(count<=0)return -1;
            auto it=peers.find(message->m_conn);if(it==peers.end()){message->Release();continue;}
            if(!it->second.id){Authenticate(message->m_conn,message->m_pData,message->m_cbSize);message->Release();continue;}
            if(message->m_cbSize<12||message->m_cbSize>capacity+8){message->Release();continue;}
            uint32_t to,type;memcpy(&to,(const char*)message->m_pData+4,4);int from=it->second.id;memcpy(&type,(const char*)message->m_pData+8,4);
            if((type&255)==DSS_THINGPOS&&message->m_cbSize>=16){uint32_t guid;memcpy(&guid,(const char*)message->m_pData+12,4);if(!gnsAcceptPosition(it->second.positions,from,guid,message->m_nMessageNumber)){message->Release();continue;}}
            if(to&&to!=id){Send(from,to,(char*)message->m_pData+8,message->m_cbSize-8);message->Release();continue;}
            *sender=from;*length=message->m_cbSize-8;memcpy(data,(char*)message->m_pData+8,*length);message->Release();return 0;
        }return -1;
    }
    int Send(uint32_t from,uint32_t to,void *data,uint32_t length){if(length<4||length>2052)return 0;unsigned char packet[2060];memcpy(packet,&from,4);memcpy(packet+4,&to,4);memcpy(packet+8,data,length);for(const auto &p:peers)if(p.second.id&&p.second.id==to)return SendTransport(p.first,packet,length+8);return to==id;}
};
GNSServer *GNSServer::instance=nullptr;
class GNSClient {
    HSteamNetConnection connection=0;bool closed=true,query=false,infoReceived=false;uint32_t started=0;
    char16_t password[32]={};std::map<uint64_t,int64_t> positions;static GNSClient *instance;
    static void Callback(SteamNetConnectionStatusChangedCallback_t *event){if(instance)instance->Changed(event);}
    void Changed(SteamNetConnectionStatusChangedCallback_t *event){if(event->m_hConn!=connection)return;
        if(event->m_info.m_eState==k_ESteamNetworkingConnectionState_ClosedByPeer||event->m_info.m_eState==k_ESteamNetworkingConnectionState_ProblemDetectedLocally){NetworkStatus(event->m_info.m_szEndDebug);g_SteamNetworkingSockets()->CloseConnection(connection,0,nullptr,false);connection=0;closed=true;}}
    void PollInfo(){SteamNetworkingMessage_t *message=nullptr;if(g_SteamNetworkingSockets()->ReceiveMessagesOnConnection(connection,&message,1)<=0)return;
        GNSInfoPacket info={};bool valid=message->m_cbSize==sizeof(info);if(valid)memcpy(&info,message->m_pData,sizeof(info));message->Release();
        if(!valid||info.magic!=GNS_MAGIC||info.version!=GNS_VERSION||info.layout!=GNSLayout()){NetworkStatus("Different GeoSith version. Install the same release on both computers.");Shutdown();return;}
        if(info.mode!=(uint32_t)Main_bMotsCompat||info.assets!=gnsAssets){NetworkStatus("Game resources or loose assets differ. Both players must use matching game data.");Shutdown();return;}
        if(info.entry.maxPlayers<1||info.entry.maxPlayers>32||!memchr(info.entry.episodeGobName,0,32)||!memchr(info.entry.mapJklFname,0,32)){NetworkStatus("Invalid server information.");Shutdown();return;}
        info.entry.serverName[31]=0;memset(info.entry.wPassword,0,sizeof(info.entry.wPassword));
        if(info.kind==GNS_INFO){sithDplayGNS_storedEntryEnum=info.entry;sithDplayGNS_storedEntryEnum.field_E0=10;infoReceived=true;
            if(query){sithDplayGNS_numEnumd=1;NetworkStatus("Game found. Select it and join.");return;}
            GNSAuthPacket auth={};auth.magic=GNS_MAGIC;auth.version=GNS_VERSION;auth.kind=GNS_AUTH;auth.mode=Main_bMotsCompat;auth.layout=GNSLayout();auth.assets=gnsAssets;memcpy(auth.password,password,sizeof(password));
            if(!SendTransport(connection,&auth,sizeof(auth),false)){NetworkStatus("Could not send authentication.");Shutdown();}
            memset(auth.password,0,sizeof(auth.password));memset(password,0,sizeof(password));
        } else if(info.kind==GNS_ACCEPT&&!query&&info.id>=2&&info.id<=32){id=info.id;sithMulti_multiModeFlags=info.entry.multiModeFlags;sithNet_MultiModeFlags=info.entry.multiModeFlags;Hack_ResetClients();NetworkStatus("Connected and authenticated. Synchronizing the level.");}
        else {NetworkStatus("Unexpected server handshake.");Shutdown();}
    }
public:
    bool IsClosed() const{return closed;}
    uint32_t id=0xffffffff;
    bool Init(const SteamNetworkingIPAddr &address,bool browsing=false,const char16_t *suppliedPassword=nullptr){Shutdown();if(!HashAssets())return false;instance=this;query=browsing;if(!query){char endpoint[128],text[192];g_SteamNetworkingIPAddr_ToString(&address,endpoint,sizeof(endpoint),true);snprintf(text,sizeof(text),"Connecting to %s...",endpoint);NetworkStatus(text);}infoReceived=false;closed=false;started=stdPlatform_GetTimeMsec();memset(password,0,sizeof(password));if(suppliedPassword)for(int i=0;i<31&&suppliedPassword[i];i++)password[i]=suppliedPassword[i];
        SteamNetworkingConfigValue_t options[2];options[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,(void*)Callback);options[1].SetInt32(k_ESteamNetworkingConfig_TimeoutInitial,12000);
        connection=g_SteamNetworkingSockets()->ConnectByIPAddress(address,2,options);if(!connection){closed=true;NetworkStatus("Cannot create the outgoing connection.");return false;}ConfigureLanes(connection);return true;}
    void Shutdown(){if(connection&&stdComm_GNS_bInitted)g_SteamNetworkingSockets()->CloseConnection(connection,0,nullptr,false);connection=0;id=0xffffffff;closed=true;positions.clear();memset(password,0,sizeof(password));}
    void RunStep(){instance=this;g_SteamNetworkingSockets()->RunCallbacks();if(!closed&&id==0xffffffff&&!(query&&infoReceived))PollInfo();if(!closed&&id==0xffffffff&&stdPlatform_GetTimeMsec()-started>15000){NetworkStatus("Connection timed out. Check the host address, UDP port, firewall and router forwarding.");Shutdown();}}
    bool GetServerInfo(const SteamNetworkingIPAddr &address,const std::atomic<bool> &running){sithDplayGNS_numEnumd=0;if(!Init(address,true))return false;while(running&&!discoveryCancelled&&!closed&&!infoReceived&&stdPlatform_GetTimeMsec()-started<12000){RunStep();sleep_ms(10);}bool found=infoReceived&&!closed&&!discoveryCancelled;if(!found&&!closed&&!discoveryCancelled&&running)NetworkStatus("No reply. Check the public IP, UDP port, firewall and router forwarding.");Shutdown();return found;}
    int Receive(int *sender,void *data,int *length){int capacity=*length;*length=0;*sender=0;if(closed){*sender=1;return 2;}if(id==0xffffffff)return -1;for(int i=0;i<64;i++){SteamNetworkingMessage_t *message=nullptr;if(g_SteamNetworkingSockets()->ReceiveMessagesOnConnection(connection,&message,1)<=0)return -1;
        if(message->m_cbSize<12||message->m_cbSize>capacity+8){message->Release();continue;}uint32_t from,to;memcpy(&from,message->m_pData,4);memcpy(&to,(char*)message->m_pData+4,4);if(!from||from>32||(to&&to!=id)){message->Release();continue;}uint32_t type;memcpy(&type,(const char*)message->m_pData+8,4);if((type&255)==DSS_THINGPOS&&message->m_cbSize>=16){uint32_t guid;memcpy(&guid,(const char*)message->m_pData+12,4);if(!gnsAcceptPosition(positions,from,guid,message->m_nMessageNumber)){message->Release();continue;}}*sender=from;*length=message->m_cbSize-8;memcpy(data,(char*)message->m_pData+8,*length);message->Release();return 0;}return -1;}
    int Send(uint32_t from,uint32_t to,void *data,uint32_t length){if(closed)return 0;if(id==0xffffffff)return 1;if(length<4||length>2052)return 0;unsigned char packet[2060];memcpy(packet,&from,4);memcpy(packet+4,&to,4);memcpy(packet+8,data,length);return SendTransport(connection,packet,length+8);}
};
GNSClient *GNSClient::instance=nullptr;

#include "gnsRelay.h"

const uint16 DEFAULT_SERVER_PORT = 27020;

extern "C"
{

extern int jkGuiNetHost_portNum;
SteamNetworkingIPAddr addrServer;
int addrServerPortLast = -1;
char addrServerLast[256];
static SteamNetworkingIPAddr enumAddresses[32],displayAddresses[32];
static jkMultiEntry enumResults[32];
static int enumResultCount;
static RelayListing relayListings[32],relayDisplay[32];
GNSClient client;
GNSServer server;

void stdComm_GNS_GetStatus(char *text,int size){if(size<=0)return;std::lock_guard<std::mutex> lock(gnsStatusMutex);if(relayActive)snprintf(text,size,"Relay %s | %s",relayAddress,gnsStatus);else if(stdComm_bIsServer)snprintf(text,size,"UDP %d | %s",jkGuiNetHost_portNum,gnsStatus);else snprintf(text,size,"%s",gnsStatus);}
void stdComm_GNS_AllowEdits(int allow){allowClientEdits=allow!=0;}
int stdComm_GNS_EditsAllowed(void){return allowClientEdits;}
void stdComm_GNS_RetryRouter(void){if(stdComm_bIsServer&&!relayActive)StartRouterMapping(jkGuiNetHost_portNum);}
int stdComm_GNS_IsConnecting(void){return relayActive?relay.Connecting():!stdComm_bIsServer&&client.id==0xffffffff;}
int stdComm_GNS_IsRelay(void){return relayActive;}
const char* stdComm_GNS_RelayAddress(void){return relayAddress;}
int stdComm_GNS_RelayDirect(void){return relayDirect;}
void stdComm_GNS_SetRelay(const char* address,int direct){if(address&&*address)snprintf(relayAddress,sizeof(relayAddress),"%s",address);relayDirect=direct!=0;relayActive=true;}
void Hack_ResetClients()
{

    DirectPlay_numPlayers = 32;
    for (int i = 0; i < 32; i++)
    {
        DirectPlay_aPlayers[i].dpId = i+1;
        jk_snwprintf(DirectPlay_aPlayers[i].waName, 32, u"asdf");
    }

    int id_self = 1;
    int id_other = 2;
    if (!stdComm_bIsServer)
    {
        id_self = 2;
        id_other = 1;
    }
    //jkPlayer_playerInfos[0].playerNetId = id_self;
    //jkPlayer_playerInfos[1].playerNetId = id_other;
    //jk_snwprintf(jkPlayer_playerInfos[0].player_name, 32, "asdf1");
    //jk_snwprintf(jkPlayer_playerInfos[1].player_name, 32, "asdf2");

    //jkPlayer_maxPlayers = 2;

    if(relayActive)stdComm_dplayIdSelf=relay.id;
    else if (stdComm_bIsServer)
        stdComm_dplayIdSelf = server.id;
    else
        stdComm_dplayIdSelf = client.id;
}

void stdComm_GNS_Startup()
{
    stdComm_GNS_LoadSymbols();

    if (stdComm_GNS_bForceStubs)
    {
        NetworkStatus("Cannot load the networking runtime. Reinstall GeoSith with its matching GameNetworkingSockets.dll.");
        jkGuiMultiplayer_numConnections = 1;
        jk_snwprintf(jkGuiMultiplayer_aConnections[0].name, 0x80, u"Networking unavailable (GNS DLL failed)");
        stdComm_dword_8321E0 = 0;

        memset(jkGuiMultiplayer_aEntries, 0, sizeof(jkMultiEntry) * 32);
        dplay_dword_55D618 = 0;
        return;
    }

    jkGuiMultiplayer_numConnections = 2;
    jk_snwprintf(jkGuiMultiplayer_aConnections[0].name, 0x80, u"Valve GNS (LAN / direct IP / VPN)");
    jk_snwprintf(jkGuiMultiplayer_aConnections[1].name, 0x80, u"Imperial Relay (no port forwarding)");
    const char* relayEnv=getenv("GEOSITH_RELAY");if(!relayEnv||!*relayEnv)relayEnv=getenv("OPENJKDF2_RELAY");
    if(relayEnv&&*relayEnv)stdComm_GNS_SetRelay(relayEnv,getenv("GEOSITH_RELAY_ONLY")?0:1);

    stdComm_dword_8321E0 = 0;

    memset(jkGuiMultiplayer_aEntries, 0, sizeof(jkMultiEntry) * 32);
    dplay_dword_55D618 = 0;
    /*jk_snwprintf(jkGuiMultiplayer_aEntries[0].serverName, 0x20, u"OpenJKDF2 Loopback");
    stdString_snprintf(jkGuiMultiplayer_aEntries[0].episodeGobName, 0x20, "JK1MP");
    stdString_snprintf(jkGuiMultiplayer_aEntries[0].mapJklFname, 0x20, "m2.jkl");
    jkGuiMultiplayer_aEntries[0].field_E0 = 10;*/

    Hack_ResetClients();

    addrServer.Clear();
    g_SteamNetworkingIPAddr_ParseString(&addrServer, "127.0.0.1");
    addrServer.m_port = DEFAULT_SERVER_PORT;

    // Create client and server sockets
    InitSteamDatagramConnectionSockets();
    if(!stdComm_GNS_bForceStubs)HashAssets();
}

void stdComm_GNS_Shutdown()
{
    DirectPlay_EnumSessions2();relay.Shutdown();StopRouterMapping();LanStop();
    if (stdComm_GNS_bForceStubs)
        return;

    ShutdownSteamDatagramConnectionSockets();
}

int DirectPlay_Receive(int *pIdOut, int *pMsgIdOut, int *pLenOut)
{
    if(relayActive){int result=relay.Receive(pIdOut,pMsgIdOut,pLenOut);Hack_ResetClients();return result;}

    if (stdComm_GNS_bForceStubs)
        return -1;

    Hack_ResetClients();

    if (stdComm_bIsServer)
    {
        server.RunStep();
        return server.Receive(pIdOut, (void*)pMsgIdOut, pLenOut);
    }
    else 
    {
        client.RunStep();
        return client.Receive(pIdOut, (void*)pMsgIdOut, pLenOut);
    }

    return -1;
}

BOOL DirectPlay_Send(DPID idFrom, DPID idTo, void *lpData, DWORD dwDataSize)
{
    if(relayActive){relay.Tick();Hack_ResetClients();return relay.Send(idFrom,idTo,lpData,dwDataSize);}

    if (stdComm_GNS_bForceStubs)
        return 0;

    Hack_ResetClients();

    if (stdComm_bIsServer)
    {
        server.RunStep();
        return server.Send(idFrom, idTo, lpData, dwDataSize);
    }
    else 
    {
        client.RunStep();
        return client.Send(idFrom, idTo, lpData, dwDataSize);
    }

    return 1;
}

int stdComm_OpenConnection(void* a)
{
    DirectPlay_EnumSessions2();relay.Shutdown();
    relayActive=(intptr_t)a==1;
    if(relayActive){stdString_CharToWchar(jkGuiMultiplayer_ipText,relayAddress,256);NetworkStatus("Relay selected. Host and clients use the same relay address.");}

    stdComm_dword_8321DC = 1;
    return 0;
}

void stdComm_CloseConnection()
{
    if ( stdComm_dword_8321DC )
    {
        if ( stdComm_dword_8321E0 )
        {
            stdComm_EnumSessions2();
            DirectPlay_Close();
            stdComm_dword_8321E0 = 0;
            stdComm_bIsServer = 0;
            stdComm_dplayIdSelf = 0;
        }
        //DirectPlay_CloseConnection();
        stdComm_dword_8321DC = 0;
    }
}

int stdComm_Open(int idx, char16_t* pwPassword)
{
    if(idx<0||idx>=32)return 0x80004005;
    if(relayActive){RelayListing game=relayDisplay[idx];DirectPlay_EnumSessions2();stdComm_dword_8321E8=0;stdComm_dword_8321E0=1;stdComm_bIsServer=0;stdComm_dplayIdSelf=0xffffffff;jkGuiMultiplayer_checksumSeed=game.entry.checksumSeed;return relay.Join(game,pwPassword)?0:0x80004005;}

    SteamNetworkingIPAddr selectedAddress=addrServer;if(idx<dplay_dword_55D618)selectedAddress=displayAddresses[idx];
    DirectPlay_EnumSessions2();addrServer=selectedAddress;

    stdComm_dword_8321E8 = 0;
    stdComm_dword_8321E0 = 1;
    stdComm_dplayIdSelf = DirectPlay_CreatePlayer(jkPlayer_playerShortName, 0);
    stdComm_bIsServer = 0;
    stdComm_dplayIdSelf = 0xffffffff; // Assigned only after host authentication
    jkGuiMultiplayer_checksumSeed = jkGuiMultiplayer_aEntries[idx].checksumSeed;

    if (stdComm_GNS_bForceStubs)
        return 0x80004005;

    if(!client.Init(addrServer,false,pwPassword))return 0x80004005;
    return 0;
}

void stdComm_Close()
{
    DirectPlay_EnumSessions2();
    if(relayActive){relay.Close();return;}
    if (stdComm_GNS_bForceStubs)
        return;

    if (stdComm_bIsServer)
    {
        server.Shutdown();
    }
    else 
    {
        client.Shutdown();
    }
}

int DirectPlay_SendLobbyMessage(void* pPkt, uint32_t pktLen)
{
    return 1;
}

void DirectPlay_SetSessionDesc(const char* a1, DWORD maxPlayers)
{
    _strncpy(sithDplayGNS_storedEntry.mapJklFname, jkMain_aLevelJklFname, 0x20);
    if(relayActive)relay.UpdateDescription();
}

BOOL DirectPlay_SetSessionFlagidk(int a1)
{
    return 1;
}

BOOL DirectPlay_Startup()
{
    //IDirectPlay3Vtbl *v0; // esi
    //uint32_t *v1; // eax

    //CoInitialize(0);
    //CoCreateInstance(&rclsid, 0, 1u, &riid, (LPVOID *)&idirectplay);
    jkGuiMultiplayer_numConnections = 0;
    memset(&jkGuiMultiplayer_aConnections, 0, 0x1180u);
    //v0 = idirectplay->lpVtbl;
    //v1 = WinIdk_GetDplayGuid();
    //return v0->EnumConnections(idirectplay, (LPCGUID)v1, (LPDPENUMCONNECTIONSCALLBACK)DirectPlay_EnumConnectionsCallback, 0, 0) >= 0;
    return 1;
}

int DirectPlay_EarlyInit(char16_t* pwIdk, char16_t* pwPlayerName)
{
    // This can launch straight into a game? Gaming Zone stuff. 1 and 2 autolaunch an MP game.
    return 0;
}

DPID DirectPlay_CreatePlayer(char16_t* pwIdk, int idk2)
{
    return 1;
}

void DirectPlay_Close()
{
    if(relayActive){relay.Close();return;}
    if(!stdComm_GNS_bForceStubs){if(stdComm_bIsServer)server.Shutdown();else client.Shutdown();}
}

int DirectPlay_OpenHost(jkMultiEntry* pEntry)
{
    sithDplayGNS_storedEntry = *pEntry;
    sithDplayGNS_storedEntry.wPassword[31]=0;for(int i=0;i<32;i++)if(!sithDplayGNS_storedEntry.wPassword[i]){memset(&sithDplayGNS_storedEntry.wPassword[i],0,(32-i)*sizeof(char16_t));break;}

    jkPlayer_maxPlayers = pEntry->maxPlayers; // Hack?

    stdComm_bIsServer = 1;

    if (stdComm_GNS_bForceStubs)
        return 0x80004005;
    if(relayActive){if(relay.Host())return 0;stdComm_bIsServer=0;return 0x80004005;}
    if(!server.Init(jkGuiNetHost_portNum)){stdComm_bIsServer=0;return 0x80004005;}
    return 0;
}

int DirectPlay_GetSession_passwordidk(jkMultiEntry* pEntry)
{
    sithDplayGNS_storedEntry = *pEntry;
    sithDplayGNS_storedEntry.wPassword[31]=0;for(int i=0;i<32;i++)if(!sithDplayGNS_storedEntry.wPassword[i]){memset(&sithDplayGNS_storedEntry.wPassword[i],0,(32-i)*sizeof(char16_t));break;}

    jkPlayer_maxPlayers = pEntry->maxPlayers; // Hack?

    return 1;
}

static int stdComm_EnumThread_bForce = 0;
static std::atomic<bool> stdComm_EnumThread_bInit(false);
static SDL_Thread *stdComm_EnumThread_thread = NULL;
static SDL_Mutex* stdComm_EnumThread_mutex = NULL;

char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    s, maxlen);
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                    s, maxlen);
            break;

        default:
            strncpy(s, "Unknown AF", maxlen);
            return NULL;
    }

    return s;
}


struct ResolverResult {std::atomic<bool> done{false};std::vector<std::string> addresses;};
static std::atomic<int> activeResolvers(0);
static void ResolveAddress(const std::string &host,const std::string &service,const std::atomic<bool> &running,std::vector<SteamNetworkingIPAddr> &result){
    result.clear();auto state=std::make_shared<ResolverResult>();
    if(activeResolvers.fetch_add(1)>=4){activeResolvers--;NetworkStatus("Too many pending DNS requests. Try again shortly.");return;}
    std::thread([host,service,state](){addrinfo hints={};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_DGRAM;hints.ai_protocol=IPPROTO_UDP;addrinfo *resolved=nullptr;
        if(!getaddrinfo(host.c_str(),service.c_str(),&hints,&resolved)&&resolved){
            for(int family:{AF_INET,AF_INET6})for(addrinfo *entry=resolved;entry;entry=entry->ai_next)if(entry->ai_family==family&&state->addresses.size()<8){char ip[256];if(!get_ip_str(entry->ai_addr,ip,sizeof(ip)))continue;
                std::string text=family==AF_INET6?"["+std::string(ip)+"]:"+service:std::string(ip)+":"+service;
                if(std::find(state->addresses.begin(),state->addresses.end(),text)==state->addresses.end())state->addresses.push_back(text);
            }freeaddrinfo(resolved);
        }state->done=true;activeResolvers--;
    }).detach();
    uint32_t began=stdPlatform_GetTimeMsec();while(running&&!discoveryCancelled&&!state->done&&stdPlatform_GetTimeMsec()-began<5000)SDL_Delay(10);
    if(!running||discoveryCancelled)return;if(!state->done){NetworkStatus("DNS lookup timed out. Try the host's numeric IP address.");return;}
    for(const auto &text:state->addresses){SteamNetworkingIPAddr parsed;parsed.Clear();if(g_SteamNetworkingIPAddr_ParseString(&parsed,text.c_str()))result.push_back(parsed);}
    if(result.empty())NetworkStatus("Could not resolve the host name. Check the address.");return;
}
static char enumAddress[256];
static int stdComm_EnumThread(void *ptr) {
    std::string previous;uint32_t last=0;
    while(stdComm_EnumThread_bInit){
        char address[256];bool force;
        SDL_LockMutex(stdComm_EnumThread_mutex);snprintf(address,sizeof(address),"%s",enumAddress);force=stdComm_EnumThread_bForce!=0;stdComm_EnumThread_bForce=0;discoveryCancelled=false;SDL_UnlockMutex(stdComm_EnumThread_mutex);
        uint32_t now=stdPlatform_GetTimeMsec();
        if(force||previous!=address||now-last>10000){previous=address;last=now;std::string host,service,error;
            if(relayActive){{std::lock_guard<std::mutex> lock(gnsStatusMutex);snprintf(relayAddress,sizeof(relayAddress),"%s",address);}auto found=relay.Enumerate();
                SDL_LockMutex(stdComm_EnumThread_mutex);enumResultCount=(int)found.size();for(int i=0;i<enumResultCount;i++){enumResults[i]=found[i].entry;relayListings[i]=found[i];}SDL_UnlockMutex(stdComm_EnumThread_mutex);continue;}

            std::vector<SteamNetworkingIPAddr> candidates;std::vector<LanGame> games;
            if(!__strcmpi(address,"LAN")){NetworkStatus("Searching the local network...");games=LanQuery(stdComm_EnumThread_bInit);NetworkStatus(games.empty()?"No LAN games found. For internet play enter the host's public IP or hostname:port.":"LAN games found. Select a game and join.");}
            else if(!gnsParseAddress(address,host,service,error)){NetworkStatus(error.c_str());}
            else {
                NetworkStatus("Looking up host and connecting (up to 12 seconds per address)...");
                ResolveAddress(host,service,stdComm_EnumThread_bInit,candidates);
            }
            uint32_t lookupStart=stdPlatform_GetTimeMsec();bool found=!games.empty();for(const auto &candidate:candidates){if(!stdComm_EnumThread_bInit||stdPlatform_GetTimeMsec()-lookupStart>30000)break;if(client.GetServerInfo(candidate,stdComm_EnumThread_bInit)){SDL_LockMutex(stdComm_EnumThread_mutex);addrServer=candidate;games.push_back({candidate,sithDplayGNS_storedEntryEnum});SDL_UnlockMutex(stdComm_EnumThread_mutex);found=true;break;}}
            SDL_LockMutex(stdComm_EnumThread_mutex);enumResultCount=(int)games.size();for(int i=0;i<enumResultCount;i++){enumResults[i]=games[i].entry;enumAddresses[i]=games[i].address;}SDL_UnlockMutex(stdComm_EnumThread_mutex);
        }SDL_Delay(25);
    }return 0;
}

int DirectPlay_EnumSessions2()
{
    if (stdComm_GNS_bForceStubs)
        return 0;

    if (!stdComm_EnumThread_bInit)
        return 0;

    stdComm_EnumThread_bInit = 0;

    int threadReturnValue;
    SDL_WaitThread(stdComm_EnumThread_thread, &threadReturnValue);
    stdComm_EnumThread_thread = NULL;

    sithDplayGNS_verbosePrintf("Enum thread done\n");

    return 0;
}

int stdComm_EnumSessions(int a, void* b)
{
    if (stdComm_GNS_bForceStubs)
        return 0;

    if (!stdComm_EnumThread_mutex)
        stdComm_EnumThread_mutex = SDL_CreateMutex();

    SDL_LockMutex(stdComm_EnumThread_mutex);
    char requested[256];stdString_WcharToChar(requested,jkGuiMultiplayer_ipText,255);if(strcmp(requested,enumAddress)){discoveryCancelled=true;enumResultCount=0;snprintf(enumAddress,sizeof(enumAddress),"%s",requested);}
    for(int i=0;i<enumResultCount;i++){jkGuiMultiplayer_aEntries[i]=enumResults[i];displayAddresses[i]=enumAddresses[i];relayDisplay[i]=relayListings[i];}
    dplay_dword_55D618=enumResultCount;
    SDL_UnlockMutex(stdComm_EnumThread_mutex);

    if (stdComm_EnumThread_bInit)
        return 0;

    Hack_ResetClients();

    dplay_dword_55D618 = 0;
    sithDplayGNS_numEnumd = 0;enumResultCount=0;
    memset(&sithDplayGNS_storedEntryEnum, 0, sizeof(sithDplayGNS_storedEntryEnum));
    memset(&jkGuiMultiplayer_aEntries[0], 0, sizeof(jkGuiMultiplayer_aEntries[0]));
    memset(addrServerLast, 0, sizeof(addrServerLast));
    addrServerPortLast = -1;

    stdComm_EnumThread_bInit = 1;

    stdComm_EnumThread_thread = SDL_CreateThread(stdComm_EnumThread, "stdComm_EnumThread", (void *)NULL);
    sithDplayGNS_verbosePrintf("Enum thread start\n");

    //DirectPlay_EnumSessions2();

    //client.GetServerInfo(addrServer);

    SDL_LockMutex(stdComm_EnumThread_mutex);
    for(int i=0;i<enumResultCount;i++){jkGuiMultiplayer_aEntries[i]=enumResults[i];displayAddresses[i]=enumAddresses[i];relayDisplay[i]=relayListings[i];}
    dplay_dword_55D618=enumResultCount;
    SDL_UnlockMutex(stdComm_EnumThread_mutex);
    

    return 0;
}

int stdComm_GNS_TestDiscovery(void){
    if(relayActive){stdString_CharToWchar(jkGuiMultiplayer_ipText,relayAddress,256);uint32_t start=stdPlatform_GetTimeMsec();do{stdComm_EnumSessions(0,nullptr);if(dplay_dword_55D618)break;SDL_Delay(25);}while(stdPlatform_GetTimeMsec()-start<15000);DirectPlay_EnumSessions2();if(!dplay_dword_55D618)return 0;
        if(relay.Join(relayDisplay[0],u"incorrect-password")){relay.Close();return 0;}
        stdPlatform_Printf("GEOSITH_GNS_DISCOVERY PASS: relay listing, compatible metadata, wrong password rejected\n");return 1;}

    char endpoint[64];snprintf(endpoint,sizeof(endpoint),"localhost:%d",getenv("GEOSITH_NET_PORT")?atoi(getenv("GEOSITH_NET_PORT")):27020);stdString_CharToWchar(jkGuiMultiplayer_ipText,endpoint,256);uint32_t began=stdPlatform_GetTimeMsec();
    do{stdComm_EnumSessions(0,nullptr);if(dplay_dword_55D618>0)break;SDL_Delay(25);}while(stdPlatform_GetTimeMsec()-began<15000);
    DirectPlay_EnumSessions2();if(dplay_dword_55D618!=1)return 0;
    std::atomic<bool> running(true);auto localGames=LanQuery(running);bool foundLAN=false;for(const auto &game:localGames)if(game.address.m_port==addrServer.m_port)foundLAN=true;if(!foundLAN)return 0;
    SteamNetworkingIPAddr ipv6;ipv6.Clear();char bracketed[64];snprintf(bracketed,sizeof(bracketed),"[::1]:%u",addrServer.m_port);if(!g_SteamNetworkingIPAddr_ParseString(&ipv6,bracketed)||!client.GetServerInfo(ipv6,running))return 0;
    stdPlatform_Printf("GEOSITH_GNS_LAN PASS: LAN broadcast and IPv6 discovery\n");
    for(int i=0;i<32;i++)if(jkGuiMultiplayer_aEntries[0].wPassword[i])return 0;
    if(!client.Init(addrServer,false,u"incorrect-password"))return 0;began=stdPlatform_GetTimeMsec();
    while(!client.IsClosed()&&stdPlatform_GetTimeMsec()-began<5000){client.RunStep();SDL_Delay(5);}
    bool rejected=client.IsClosed()&&client.id==0xffffffff;client.Shutdown();
    if(!rejected)return 0;
    stdPlatform_Printf("GEOSITH_GNS_DISCOVERY PASS: hostname lookup, public metadata, password rejection before player allocation\n");return 1;
}
int stdComm_GNS_TestListener(void){if(relayActive){stdPlatform_Printf("GEOSITH_RELAY_HOST PASS: outbound relay hosting\n");return 1;}GNSServer conflict;bool opened=conflict.Init(jkGuiNetHost_portNum);if(opened){conflict.Shutdown();return 0;}server.RunStep();stdPlatform_Printf("GEOSITH_GNS_LISTEN PASS: duplicate port refused\n");return 1;}

void DirectPlay_EnumPlayers(int a)
{
    Hack_ResetClients();
}

int DirectPlay_StartSession(void* a, void* b)
{
    return 1;
}

void DirectPlay_Destroy()
{
    
}

int DirectPlay_IdkSessionDesc(jkMultiEntry* pEntry)
{
    //TODO
    return 1;
}

}
