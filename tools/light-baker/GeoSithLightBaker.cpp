// Runs the user's unmodified 32-bit JED Light Baker DLL outside the 64-bit game.
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
#include <algorithm>
#include <cmath>
#include "JedDefaults.h"
#include "edGeometry.h"
#include "edWire.h"
static std::unique_ptr<EdModel> level;
static std::filesystem::path job;
static std::vector<std::wstring> materials;
static std::wstring colormap;
static std::vector<std::wstring> sectorColormaps;
static std::vector<int> selected;
static HWND owner,dialog,statusText;
static WNDPROC pluginWindowProc;
static unsigned bakedVertices,cleaveAttempts,cleaveSuccesses;
static bool cleaveAllCommand;
static int bakerLightFlags(int flags){
 // JED normally stores this extra per-light flag. In GeoSith the plugin
 // checkboxes opt the corresponding lights into the shadow-cleaving pass.
 bool eligible=!(flags&(4|8|32));
 bool enabled=cleaveAllCommand||(dialog&&IsDlgButtonChecked(dialog,(flags&2)?2008:2010)==BST_CHECKED);
 return (flags&~0x40)|(eligible&&enabled?0x40:0);
}
static bool applyRequested;
static int createdLight=-1;
static EdVec newLightPosition;
static void status(const char *message){if(statusText){SetWindowTextA(statusText,message);UpdateWindow(statusText);}}
static void applyAndReturn(){applyRequested=true;if(dialog)PostMessageW(dialog,WM_CLOSE,0,0);}

static bool changed,failed;
static std::vector<std::string> settings;
static constexpr char settingsName[]="light_baker_settings.cog";
static void saveSettings(){std::ofstream f(job.parent_path()/"baker-settings.txt");for(auto &v:settings)f<<v<<'\n';}

static std::vector<unsigned char> readBytes(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
// Append terrain triangles only to the baker's view of each sector. They are
// lighting/occlusion geometry, never real sector walls or shadow-cleave targets.
static std::vector<std::vector<std::pair<int,int>>> terrainFaces;
static int faceCount(int s){return level->c[s].count+(s<(int)terrainFaces.size()?(int)terrainFaces[s].size():0);}
static EdFace &faceAt(int s,int f){if(f<level->c[s].count)return level->c[s].f[f];auto ref=terrainFaces[s][f-level->c[s].count];return level->meshes[ref.first].geometry.f[ref.second];}
static bool valid(int s,int f=-1,int v=-1){return s>=0&&s<level->count&&(f<0||(f<faceCount(s)&&(v<0||v<faceAt(s,f).count)));}
static void prepareTerrain(){
 terrainFaces.assign(level->count,{});
 for(int i=0;i<level->meshCount;i++){
  auto &mesh=level->meshes[i];if(!level->assets[mesh.asset].terrain&&!level->assets[mesh.asset].lightBaker)continue;
  for(int f=0;f<mesh.geometry.count;f++){
   auto &face=mesh.geometry.f[f];EdVec center={};for(int v=0;v<face.count;v++)center=edAdd(center,face.v[v].p);center=edScale(center,1./face.count);
   for(int s=0;s<level->count;s++)if(edContains(&level->c[s],center)){terrainFaces[s].push_back({i,f});break;}
  }
 }
}
static double clamp(double x){return std::isfinite(x)?std::clamp(x,0.,1.):0;}
static void error(const char *message){failed=true;std::ofstream(job/"error.txt")<<message;}
struct Level : IJEDLevelDefaults {
 HRESULT __stdcall GetLevelHeader(tlevelheader *r,int) override {*r={};r->version=1;r->mastercmp=colormap.data();r->pixelperunit=256;return S_OK;}
 int __stdcall NSectors() override {return level->count;}
 int __stdcall NLights() override {return level->lightCount;}
 int __stdcall NLayers() override {prepareTerrain();return 1;}
 char* __stdcall GetLayerName(int) override {return const_cast<char*>("Level");}
 int __stdcall SectorNSurfaces(int s) override {return valid(s)?faceCount(s):0;}
 int __stdcall SectorNVertices(int s) override {int n=0;if(valid(s))for(int f=0;f<faceCount(s);f++)n+=faceAt(s,f).count;return n;}
 int __stdcall SurfaceNVertices(int s,int f) override {return valid(s,f)?faceAt(s,f).count:0;}
 int __stdcall SurfaceGetVertexNum(int s,int f,int v) override {if(!valid(s,f,v))return -1;for(int i=0;i<f;i++)v+=faceAt(s,i).count;return v;}
 HRESULT __stdcall SectorGetVertex(int s,int v,double *x,double *y,double *z) override {if(!valid(s)||v<0)return E_INVALIDARG;for(int f=0;f<faceCount(s);f++){auto &face=faceAt(s,f);if(v<face.count){auto p=face.v[v].p;*x=p.x;*y=p.y;*z=p.z;return S_OK;}v-=face.count;}return E_INVALIDARG;}
 HRESULT __stdcall GetSector(int s,tjedsectorrec *r,int) override {if(!valid(s)||!r)return E_INVALIDARG;auto &c=level->c[s];*r={};r->flags=c.flags;r->ambient=c.ambientLight;r->extra=c.extraLight;r->colormap=sectorColormaps[s].data();r->tint_r=c.tint.x;r->tint_g=c.tint.y;r->tint_b=c.tint.z;return S_OK;}
 HRESULT __stdcall SetSector(int s,tjedsectorrec *r,int mask) override {if(!valid(s)||!r)return E_INVALIDARG;auto &c=level->c[s];if(mask&s_ambient)c.ambientLight=clamp(r->ambient);if(mask&s_extra)c.extraLight=clamp(r->extra);if(mask&s_flags)c.flags=r->flags;changed=true;return S_OK;}
 HRESULT __stdcall SectorUpdate(int) override {return S_OK;}
 HRESULT __stdcall GetSurface(int s,int f,tjedsurfacerec *r,int) override {if(!valid(s,f)||!r)return E_INVALIDARG;auto &face=faceAt(s,f);auto a=edSurfaceAttributes(&face);*r={};r->adjoinsc=face.neighbor;r->adjoinsf=face.mirror;r->adjoinflags=a.adjoinFlags;r->surfflags=a.surfaceFlags;r->faceflags=a.faceFlags;r->geo=a.geometryMode;r->light=a.lightingMode;r->tex=a.textureMode;r->extralight=a.extraLight;r->uscale=a.uScale;r->vscale=a.vScale;r->material=face.material>=0&&face.material<(int)materials.size()?materials[face.material].data():const_cast<wchar_t*>(L"dflt.mat");return S_OK;}
 HRESULT __stdcall SetSurface(int s,int f,tjedsurfacerec *r,int mask) override {if(!valid(s,f)||!r)return E_INVALIDARG;auto &face=faceAt(s,f);auto a=edSurfaceAttributes(&face);if(mask&sf_SurfFlags)a.surfaceFlags=r->surfflags;if(mask&sf_FaceFlags)a.faceFlags=r->faceflags;if(mask&sf_adjoinflags)a.adjoinFlags=r->adjoinflags;if(mask&sf_geo)a.geometryMode=r->geo;if(mask&sf_light)a.lightingMode=r->light;if(mask&sf_tex)a.textureMode=r->tex;if(mask&sf_ExtraLight)a.extraLight=r->extralight;face.attributes=a;face.hasAttributes=1;changed=true;return S_OK;}
 HRESULT __stdcall GetSurfaceNormal(int s,int f,tjedvector *r) override {if(!valid(s,f)||!r)return E_INVALIDARG;auto n=edNormal(&faceAt(s,f));r->v.s1={n.x,n.y,n.z};return S_OK;}
 HRESULT __stdcall SurfaceUpdate(int,int,int) override {return S_OK;}
 HRESULT __stdcall SurfaceGetVertexLight(int s,int f,int v,float *w,float *r,float *g,float *b) override {if(!valid(s,f,v))return E_INVALIDARG;auto &face=faceAt(s,f);auto &p=face.v[v];*w=p.light;*r=face.bakedLight?p.lightRGB.x:p.light;*g=face.bakedLight?p.lightRGB.y:p.light;*b=face.bakedLight?p.lightRGB.z:p.light;return S_OK;}
 HRESULT __stdcall SurfaceSetVertexLight(int s,int f,int v,float w,float r,float g,float b) override {if(!valid(s,f,v))return E_INVALIDARG;auto &face=faceAt(s,f);++bakedVertices;face.bakedLight=1;face.v[v].light=clamp(w);face.v[v].lightRGB=edV(clamp(r),clamp(g),clamp(b));face.attributes=edSurfaceAttributes(&face);face.attributes.lightingMode=3;face.hasAttributes=1;changed=true;return S_OK;}
 HRESULT __stdcall SurfaceGetVertexUV(int s,int f,int v,float *u,float *vv) override {if(!valid(s,f,v))return E_INVALIDARG;*u=faceAt(s,f).v[v].u;*vv=faceAt(s,f).v[v].v;return S_OK;}
 int __stdcall AddLight() override {if(level->lightCount>=ED_LIGHTS)return -1;int i=level->lightCount++;level->lights[i]=EdLight{0,newLightPosition,1,4};createdLight=i;changed=true;return i;}
 HRESULT __stdcall GetLight(int i,tjedlightrec *r,int) override {if(i<0||i>=level->lightCount||!r)return E_INVALIDARG;auto &l=level->lights[i];*r={};r->x=l.position.x;r->y=l.position.y;r->z=l.position.z;r->intensity=r->rgbintensity=l.intensity;r->range=r->rgbrange=l.range;r->r=l.color.x;r->g=l.color.y;r->b=l.color.z;r->flags=bakerLightFlags(l.type);return S_OK;}
 HRESULT __stdcall SetLight(int i,tjedlightrec *r,int mask) override {if(i<0||i>=level->lightCount||!r)return E_INVALIDARG;auto &l=level->lights[i];if((mask&lt_position)&&!(i==createdLight&&r->x==0&&r->y==0&&r->z==0))l.position=edV(r->x,r->y,r->z);if(mask&lt_intensity)l.intensity=std::clamp(r->intensity,0.,100.);if(mask&lt_range)l.range=std::clamp(r->range,ED_GRID,10000.);if(mask&lt_rgb)l.color=edV(clamp(r->r),clamp(r->g),clamp(r->b));if(mask&lt_flags)l.type=r->flags;changed=true;return S_OK;}
 int __stdcall NCOgs() override {return level->cogCount+(settings.empty()?0:1);}
 char* __stdcall GetCOGFileName(int i) override {if(i==level->cogCount&&!settings.empty())return const_cast<char*>(settingsName);return i>=0&&i<level->cogCount?level->cogs[i].file:nullptr;}
 int __stdcall NCOGValues(int i) override {if(i==level->cogCount)return (int)settings.size();return i>=0&&i<level->cogCount?level->cogs[i].count:0;}
 int __stdcall GetCOGValueType(int i,int p) override {return i>=0&&i<level->cogCount&&p>=0&&p<level->cogs[i].count?level->cogs[i].params[p].type:0;}
 char* __stdcall GetCOGValue(int i,int p) override {static char text[32];if(i==level->cogCount&&p>=0&&p<(int)settings.size())return settings[p].data();if(i<0||i>=level->cogCount||p<0||p>=level->cogs[i].count)return nullptr;edCogParamValue(level.get(),&level->cogs[i].params[p],text);return text;}
 // Baker settings are stored by the compatibility host, never inserted as a gameplay COG.
 int __stdcall AddCOG(char *name) override {if(!name||strcmp(name,settingsName))return -1;settings.resize(16,"0");return level->cogCount;}
 bool __stdcall SetCOGValue(int i,int p,char *value) override {if(i!=level->cogCount||p<0||p>=16||!value)return false;settings[p]=value;saveSettings();return true;}
 HRESULT __stdcall DeleteCOG(int i) override {if(i!=level->cogCount)return E_INVALIDARG;settings.clear();return S_OK;}
} jedLevel;
struct Host : IJEDDefaults {
 std::map<int,FILE*> files;int nextFile=1;
 double __stdcall GetVersion() override {return .951;}
 IJEDLevel* __stdcall GetLevel() override {return &jedLevel;}
 int __stdcall GetJedWindow(int) override {return (int)(intptr_t)owner;}
 int __stdcall GetApplicationHandle() override {return (int)(intptr_t)owner;}
 bool __stdcall GetIsMOTS() override {return false;}
 bool __stdcall IsLayerVisible(int) override {return true;}
 int __stdcall GetNMultiselected(int what) override {return what==MM_SC?(int)selected.size():0;}
 int __stdcall GetSelectedSC(int i) override {return i>=0&&i<(int)selected.size()?selected[i]:-1;}
 int __stdcall FindSectorForXYZ(double x,double y,double z) override {for(int s=0;s<level->count;s++)if(edContains(&level->c[s],edV(x,y,z)))return s;return -1;}
 HRESULT __stdcall FindBBox(int s,tjedbox *b) override {if(!valid(s)||!b)return E_INVALIDARG;*b={1e30,1e30,1e30,-1e30,-1e30,-1e30};for(int f=0;f<level->c[s].count;f++)for(int v=0;v<level->c[s].f[f].count;v++){auto p=level->c[s].f[f].v[v].p;b->x1=std::min(b->x1,p.x);b->y1=std::min(b->y1,p.y);b->z1=std::min(b->z1,p.z);b->x2=std::max(b->x2,p.x);b->y2=std::max(b->y2,p.y);b->z2=std::max(b->z2,p.z);}return S_OK;}
 HRESULT __stdcall FindSurfaceCenter(int s,int f,double *x,double *y,double *z) override {if(!valid(s,f))return E_INVALIDARG;auto &face=faceAt(s,f);EdVec p={};for(int v=0;v<face.count;v++)p=edAdd(p,face.v[v].p);p=edScale(p,1./face.count);*x=p.x;*y=p.y;*z=p.z;return S_OK;}
 int __stdcall CleaveSurface(int s,int f,tjedvector *n,double x,double y,double z) override {++cleaveAttempts;if(!valid(s,f)||f>=level->c[s].count||!n||level->c[s].splineId)return -1;int count=level->c[s].count;auto copy=std::make_unique<EdModel>(*level);if(!edCleaveFacePlane(copy.get(),s,f,edV(n->v.s1.x,n->v.s1.y,n->v.s1.z),edV(x,y,z))||!edValidate(copy.get()))return -1;*level=std::move(*copy);++cleaveSuccesses;changed=true;return count;}
 bool __stdcall FindCommonEdges(int s,int f,int t,int g,int *a,int *b,int *c,int *d) override {if(!valid(s,f)||!valid(t,g))return false;auto &x=faceAt(s,f);auto &y=faceAt(t,g);for(int i=0;i<x.count;i++)for(int j=0;j<y.count;j++){auto closePoint=[](EdVec a,EdVec b){EdVec d=edSub(a,b);return edDot(d,d)<ED_EPS*ED_EPS;};if(closePoint(x.v[i].p,y.v[(j+1)%y.count].p)&&closePoint(x.v[(i+1)%x.count].p,y.v[j].p)){*a=i;*b=(i+1)%x.count;*c=j;*d=(j+1)%y.count;return true;}}return false;}
 HRESULT __stdcall StartUndo(char*) override {terrainFaces.clear();failed=false;return S_OK;}
 HRESULT __stdcall SaveUndoForSector(int,int,int) override {return S_OK;}
 HRESULT __stdcall UpdateMap() override {return S_OK;}
 HRESULT __stdcall SetMapMode(int) override {return S_OK;}
 HRESULT __stdcall SetCurLT(int i) override {if(i<0||i>=level->lightCount)return E_INVALIDARG;std::ofstream(job/"selected-light.txt")<<i;applyAndReturn();return S_OK;}
 HRESULT __stdcall PanMessage(int type,char *message) override {status(message);std::ofstream(job/"messages.txt",std::ios::app)<<message<<'\n';if(type==msg_error)error(message);return S_OK;}
 char* __stdcall GetJEDString(int) override {static std::string dir;dir=job.string();return dir.data();}
 int __stdcall OpenGameFile(char *name) override {
 // This plugin passes its UTF-16 JED filenames through the legacy char* ABI.
 if(!name)return -1;std::filesystem::path p(reinterpret_cast<wchar_t*>(name));if(p.is_absolute())return -1;for(auto part:p)if(part=="..")return -1;
 for(auto prefix:{"","mat","3do/mat","misc/cmp"}){auto data=job/"resources"/prefix/p;FILE *f=_wfopen(data.c_str(),L"rb");if(f){int id=nextFile++;files[id]=f;return id;}}return -1;
 }
 int __stdcall ReadFile(int id,void *p,long size) override {auto i=files.find(id);return i!=files.end()&&size>=0?(int)fread(p,1,size,i->second):0;}
 HRESULT __stdcall CloseFile(int id) override {auto i=files.find(id);if(i!=files.end()){fclose(i->second);files.erase(i);}return S_OK;}
 HRESULT __stdcall SetFilePos(int id,long p) override {auto i=files.find(id);return i!=files.end()&&!fseek(i->second,p,SEEK_SET)?S_OK:E_FAIL;}
 long __stdcall GetFileSize(int id) override {auto i=files.find(id);if(i==files.end())return 0;long pos=ftell(i->second);fseek(i->second,0,SEEK_END);long size=ftell(i->second);fseek(i->second,pos,SEEK_SET);return size;}
} host;
// Route actual button notifications through the plugin, then apply completed work.
// A failed/no-light bake stays open with its diagnostic visible.
static LRESULT CALLBACK bakerWindowProc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    bool clicked=message==WM_COMMAND&&HIWORD(wp)==BN_CLICKED;
    int id=LOWORD(wp);unsigned before=bakedVertices;
    if(clicked&&(id==2005||id==2006||id==2007||id==1031))status("Working... GeoSith will apply successful results automatically.");
    bool previousCleaveAll=cleaveAllCommand;if(clicked&&id==1031)cleaveAllCommand=true;
    LRESULT result=CallWindowProcW(pluginWindowProc,window,message,wp,lp);
    cleaveAllCommand=previousCleaveAll;
    if(clicked&&!failed){
        if((id>=2005&&id<=2007)&&bakedVertices>before)applyAndReturn();
        if(id==1031&&changed)applyAndReturn();
        if(id==1034)status("Light Baker settings saved on this computer.");
        if(id==1035)status("Light Baker settings loaded.");
    }
    return result;
}
static void installFeedback(){
    RECT client,outer;GetClientRect(dialog,&client);GetWindowRect(dialog,&outer);
    SetWindowPos(dialog,nullptr,0,0,outer.right-outer.left,outer.bottom-outer.top+90,SWP_NOMOVE|SWP_NOZORDER);
    statusText=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_READONLY|WS_VSCROLL,10,client.bottom+8,client.right-20,72,dialog,nullptr,GetModuleHandleW(nullptr),nullptr);
    SendMessageW(statusText,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
    pluginWindowProc=(WNDPROC)SetWindowLongPtrW(dialog,GWLP_WNDPROC,(LONG_PTR)bakerWindowProc);
    status(level->lightCount?"Ready. Bake applies the result and returns to GeoSith. Sun / Sky / Anchor opens that light's properties.":"No editor lights are placed. Return to Lights mode to place a light, or use Sun / Sky here. Bake All requires at least one light.");
}

static BOOL CALLBACK findDialog(HWND w,LPARAM){DWORD pid;GetWindowThreadProcessId(w,&pid);if(pid==GetCurrentProcessId()&&w!=owner&&IsWindowVisible(w)){wchar_t cls[64];GetClassNameW(w,cls,64);if(!wcscmp(cls,L"#32770"))dialog=w;}return TRUE;}
// Test-only acknowledgement of the plugin's own destructive-cleave prompt.
static LRESULT CALLBACK confirmTestCleave(int code,WPARAM wp,LPARAM lp){
    if(code==HCBT_ACTIVATE){HWND w=(HWND)wp;if(GetDlgItem(w,IDYES)&&GetDlgItem(w,IDNO))PostMessageW(w,WM_COMMAND,IDYES,0);}
    return CallNextHookEx(nullptr,code,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
 int argc;LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(argc<3)return 2;
 try{
 job=argv[2];level=std::make_unique<EdModel>();auto bytes=readBytes(job/"input.bin");if(bytes.empty()||!edWireDecode(level.get(),bytes.data(),(uint32_t)bytes.size())||!edValidate(level.get()))return 3;
 std::ifstream names(job/"materials.txt");std::string line;while(std::getline(names,line))materials.emplace_back(line.begin(),line.end());colormap=std::filesystem::path(level->header.colormap).wstring();for(int s=0;s<level->count;s++)sectorColormaps.push_back(level->c[s].colormap[0]?std::filesystem::path(level->c[s].colormap).wstring():colormap);std::ifstream selection(job/"selected.txt");int s;while(selection>>s)if(valid(s))selected.push_back(s);
 {std::ifstream f(job.parent_path()/"baker-settings.txt");std::string value;while(std::getline(f,value))settings.push_back(value);if(settings.size()!=16)settings.clear();}
 newLightPosition=level->spawn;{std::ifstream f(job/"light-position.txt");EdVec p;if(f>>p.x>>p.y>>p.z)newLightPosition=p;}
 owner=CreateWindowW(L"STATIC",L"GeoSith Light Baker",WS_OVERLAPPEDWINDOW,0,0,200,100,nullptr,nullptr,instance,nullptr);
 HMODULE plugin=LoadLibraryExW(argv[1],nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);if(!plugin){error("Could not load Light Baker.dll. Install its required x86 Visual C++ runtime.");return 4;}
 auto load=(bool (__stdcall *)(IJED*))GetProcAddress(plugin,"JEDPluginLoadStdCall");if(!load||!load(&host)){error("The DLL did not initialize its JED Light Baker interface.");return 5;}
 EnumWindows(findDialog,0);if(!dialog){error("The plugin did not open a window.");return 6;}installFeedback();SetForegroundWindow(dialog);
 if(argc>3&&(!wcscmp(argv[3],L"--test")||!wcscmp(argv[3],L"--test-no-lights")||!wcscmp(argv[3],L"--test-sun")||!wcscmp(argv[3],L"--test-cleave"))){
     bool noLights=!wcscmp(argv[3],L"--test-no-lights"),sun=!wcscmp(argv[3],L"--test-sun");
     bool cleave=!wcscmp(argv[3],L"--test-cleave");
     if(cleave){
         edDefault(level.get());level->c[1]=level->c[0];level->count=2;
         for(int f=0;f<6;f++)for(int v=0;v<4;v++){auto &p=level->c[1].f[f].v[v].p;p.x*=4;p.y=p.y*2+3;p.z*=3;}
         if(!edResolve(level.get())||!edValidate(level.get()))return 15;
         level->lightCount=1;level->lights[0]=EdLight{0,edV(0,-.75,.5),1,20};sectorColormaps.assign(level->count,colormap);
         CheckDlgButton(dialog,2010,BST_UNCHECKED);CheckDlgButton(dialog,2008,BST_UNCHECKED);
         if(bakerLightFlags(0)&0x40)return 16;
         CheckDlgButton(dialog,2010,BST_CHECKED);
         if(!(bakerLightFlags(0)&0x40)||(bakerLightFlags(2)&0x40)||(bakerLightFlags(4)&0x40))return 16;
         CheckDlgButton(dialog,2008,BST_CHECKED);if(!(bakerLightFlags(2)&0x40))return 16;
         unsigned faces=0;for(int s=0;s<level->count;s++)faces+=level->c[s].count;
         for(int id:{1008,2002,2003,2004})CheckDlgButton(dialog,id,BST_UNCHECKED);
         CheckDlgButton(dialog,2001,BST_CHECKED);
         HHOOK confirm=SetWindowsHookExW(WH_CBT,confirmTestCleave,nullptr,GetCurrentThreadId());
         SendMessageW(GetDlgItem(dialog,2005),BM_CLICK,0,0);
         if(confirm)UnhookWindowsHookEx(confirm);
         unsigned after=0;for(int s=0;s<level->count;s++)after+=level->c[s].count;
         std::ofstream(job/"cleave-test.txt")<<"faces "<<faces<<" -> "<<after<<", attempts "<<cleaveAttempts<<", accepted "<<cleaveSuccesses<<"\n";
         if(!cleaveSuccesses||after<=faces||!bakedVertices||!applyRequested||!edValidate(level.get()))return 17;
     }
     if(noLights)level->lightCount=0;
     if(cleave){}else if(sun){SendMessageW(GetDlgItem(dialog,1028),BM_CLICK,0,0);if(!applyRequested||!std::filesystem::exists(job/"selected-light.txt"))return 14;}
     else {
     for(int id:{1008,2002,2003,2004,2008,2010})CheckDlgButton(dialog,id,BST_UNCHECKED);
     CheckDlgButton(dialog,2001,BST_CHECKED);SendMessageW(GetDlgItem(dialog,2005),BM_CLICK,0,0);
     if(noLights){char text[512];GetWindowTextA(statusText,text,sizeof(text));if(applyRequested||!strstr(text,"No lights available"))return 13;std::ofstream(job/"feedback-test.txt")<<text;PostMessageW(dialog,WM_CLOSE,0,0);}
     else if(!applyRequested||!bakedVertices){error("Bake button did not request application of completed lighting.");return 13;}
     }
 }
 MSG msg;while(IsWindow(dialog)&&IsWindowVisible(dialog)){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(!IsDialogMessageW(dialog,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(std::filesystem::exists(job/"cancel"))return 7;Sleep(10);}
 if(failed)return 8;if(changed){if(!edValidate(level.get())){error("Plugin result failed geometry validation.");return 9;}uint32_t size;auto out=edWireEncode(level.get(),&size);if(!out)return 10;std::ofstream f(job/"output.bin",std::ios::binary);f.write((char*)out,size);edFree(out);if(!f)return 11;}return 0;
 }catch(const std::exception &e){error(e.what());return 12;}
}
