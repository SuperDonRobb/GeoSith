extern "C" {
#include "jkEditor.h"
}
#include "edGeometry.h"
#include "edResources.h"
#include "edPngMat.h"
#include "jkEditorResources.h"
#include <map>
#include <memory>
#include <array>
#include "edLevelIO.h"
#include "edInput.h"
#include "edMap.h"
#include "edWire.h"
extern "C" {
#include "jk.h"
#include "Main/jkControl.h"
#include "Main/jkGame.h"
#include "stdPlatform.h"
#include "World/sithWorld.h"
#include "World/sithSurface.h"
#include "World/sithSector.h"
#include "World/sithMaterial.h"
#include "World/sithThing.h"
#include "World/sithWeapon.h"
#include "World/sithTemplate.h"
#include "World/sithModel.h"
#include "General/stdConffile.h"
#include "World/sithExplosion.h"
#include "World/jkPlayer.h"
#include "Gameplay/sithInventory.h"
#include "Gameplay/sithTime.h"
#include "AI/sithAIAwareness.h"
#include "AI/sithAIClass.h"
#include "Devices/sithControl.h"
#include "Devices/sithSound.h"
#include "Devices/sithComm.h"
#include "Win95/stdComm.h"
#include "Dss/sithMulti.h"
int sithMulti_StartupServer(void);
int sithMulti_StartupClient(void);
#include "Devices/sithConsole.h"
#include "Platform/stdControl.h"
#include "Main/jkMain.h"
#include "Main/jkRes.h"
#include "Engine/rdCamera.h"
#include "Engine/sithCamera.h"
#include "Engine/sithIntersect.h"
#include "Engine/rdThing.h"
#include "Primitives/rdPolyline.h"
#include "Raster/rdCache.h"
#include "Win95/stdDisplay.h"
#include "Primitives/rdMatrix.h"
#include "Primitives/rdPrimit2.h"
#include "Primitives/rdModel3.h"
#include "Primitives/rdSprite.h"
#include "General/stdFont.h"
#include "General/stdString.h"
#include "General/stdStrTable.h"
#include "Cog/jkCog.h"
#include "Cog/sithCog.h"
#include "Cog/sithCogParse.h"
#include "General/stdColor.h"
#include "General/stdFileUtil.h"
#include "General/stdBitmap.h"
#include "Platform/std3D.h"
#include "Win95/stdGob.h"
#include "Engine/rdMaterial.h"
#include "Engine/rdroid.h"
#include "Engine/rdColormap.h"
#include "Engine/sithRender.h"
#include "Engine/sithCollision.h"
#include "General/stdPalEffects.h"
#include "Gui/jkGUI.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUIDialog.h"
#include "Gui/jkGUITitle.h"
#include "Gui/jkGUIEsc.h"
#include "Win95/Video.h"
#include "Win95/Windows.h"
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stddef.h>

static void say(const char *s);
static EdModel *model,*snapshot,*candidate,*undoModel;
static int selectionCleanup;
static SithWorld *boundWorld;
static SithWorld *failedImportWorld;
static int importingStock;
static char protectedEpisode[32];
#define EDITOR_MATERIALS 4096
static rdMaterial *palette[EDITOR_MATERIALS];
static char paletteNames[EDITOR_MATERIALS][128];
static unsigned char paletteOwned[EDITOR_MATERIALS];
static int baseMaterialCount,currentMaterial;
#define EDITOR_COLORMAPS 128
typedef struct {char name[128];rdColor24 colors[256];int loaded;} EditorColormap;
static EditorColormap editorColormaps[EDITOR_COLORMAPS];
static int colormapCount,materialSection[EDITOR_MATERIALS],materialColormap[EDITOR_MATERIALS];
static EdSelection picks,grabPicks,undoPicks;
#define EDITOR_UNDOS 30
typedef struct {unsigned char *data;uint32_t size,rawSize;EdSelection selection;} EditorHistoryEntry;
static EditorHistoryEntry undoHistory[EDITOR_UNDOS],redoHistory[EDITOR_UNDOS];
static int undoCount,redoCount,pendingUndo,historyDown;
static EdSelection grabVisual;
static int groupGrab,sectorGrab;
static EdBuffer<unsigned char> grabSectors;
static EdDoubleTap selectionTap;
static int paletteCount,active,toggleDown,fireDown,grabDown,grabbing,uvMode,uvRotating,cleaving,tool=10,clipboard=-1,multi;
static int homeDown,tabDown,qDown,nextSpawn,deleteDown,middleDown,attributesDown;
static int flightSpeedBoost,flightSpeedDown,oppositeAdjoinDown,uvFine;
static int mapActive;
static int builderAsset=-1;
static int builderExitRequested;
static void builderFreeCache(void);
static int stressBuilder(SithThing *player);
static int builderOriginSelfTest(void);
static int builderCleaveSelfTest(void);
static int builderGrabSelfTest(void);
static void stressTemplates(void);
static int builderWalkstruct(EdModel *draft,int asset,const char *name);
static void terrainTemplateFlags(EdModel *draft);
static void builderCaptureMenu(jkGuiMenu *menu);
static void mapClose(SithThing *player);
static int splineSelfTest(SithThing *player);
static int terrainSelfTest(SithThing *player);
static void mapOpen(SithThing *player);
static void mapControl(SithThing *player);
static void mapDraw(void);
static void mapInvalidate(void);
static void mapEscape(void);
static int mapSelfTest(SithThing *player);
static int placedEditingSelfTest(void);
static int stressMap(SithThing *player);
static int mapViewsTest(SithThing *player);
static int mapMirrorTest(SithThing *player);
static int mapMirrorCandidate(void);
static void screenLine(int x1,int y1,int x2,int y2,int color,int width);
static void surfaceAttributesDialog(void);
static void sectorAttributesDialog(void);
static int stressSectors(SithThing *player);
static EdBuffer<unsigned char> sectorPicks;
static int mergeDown;
static void mergeSelectedSectors(void);
static int selectedSectorCount(void){int count=0;for(int s=0;s<model->count;s++)count+=sectorPicks[s]!=0;return count;}
static void remember(void);
static int commit(void);
static SithThing railEffectTemplate;
static uint32_t savedPhysics,savedDebug,savedThingFlags;
static EdDoubleTap jumpTap;
static EdHit hover={-1,-1,-1,-1},selection;
static EdVec cleaveStart,cleaveEnd,grabNormal;
static double grabDepth;
static EdVec grabPlanePoint,grabPlaneNormal,grabLastPoint,grabLastOrigin,grabDelta;
static EdDragTrace geometryDragTrace;
static int grabTraceValid,sectorCleave;
static char liveColormap[128];
#define EDITOR_TEMPLATES 2048
static char templateNames[EDITOR_TEMPLATES][128];
static unsigned int templateSections[EDITOR_TEMPLATES];
static int templateParents[EDITOR_TEMPLATES];
static char templateHints[EDITOR_TEMPLATES][2048];
static const char *objectSections[]={"All templates","Sprites","Walkstructs","Projectiles","Pickups","Ammo","Weapon pickups","Enemies","NPC","Dark Jedi","Ships","Elevators","Ghosts","Decor","Throwable","Other"};
static int templateCount,currentTemplate,browserObjects;
static int objectSlotCount;
static int objectThings[ED_OBJECTS],objectSignatures[ED_OBJECTS],objectProxyMode[ED_OBJECTS];
static char objectThingNames[ED_OBJECTS][64];
static unsigned int objectTemplateHashes[ED_OBJECTS];
static int objectHovered=-10000,objectGrabbed=-10000,objectGrabbing;
static int objectHandleHovered=-10000,objectRotating;
static int lightHovered=-1,lightGrabbed=-1,lightGrabbing;
static EdVec objectRotationCenter;
static char *resources;
static char message[160]="F8: editor mode";
static char16_t levelName[32]=u"MyLevel";
static int cleaveShape;
static double shapeRadius=.25;
static double shapeRotation;
static rdPolyline cleaveBlade;
static rdThing cleaveThing;
static int bladeReady;
/* Keep the world view separate from the weapon/HUD rendering state. */
static rdCamera editorView;
static rdCanvas editorViewCanvas;
static int editorViewReady;
static rdCanvas *highlightCanvas;
static int edgeWidth,outlineColor;
static int stressRunning,stressRenderFrames;
static int highlightSelfTest(void);
static void materialMenu(int nested=0);
static void syncEditorObjects(const EdModel *source);
static void recoverOutsideEditorPlayer(const EdModel *source);
static int syncCustomTemplates(const EdModel *source);
static unsigned int customTemplateHash(const EdModel *source,const char *name);
static void resetCustomTemplates(void);
static void templateCreatorMenu(void);
static void drawObjectPreview(tVBuffer *buffer,int id,int x,int y,int width,int height);
static void objectTile(jkGuiElement *element,jkGuiMenu *menu,tVBuffer *buffer,BOOL redraw);
static int objectControl(SithThing *player,EdVec origin,EdVec direction,int fire,int grab,int middlePressed,int freeRotation);
static int lightControl(EdVec origin,EdVec direction,int fire,int grab,int shift,int wheel);
static int pickEditorLight(EdVec origin,EdVec direction,double wallDistance);
static int deleteEditorLight(int id);
static int lightSelfTest(void);
static int lightsWorldCheck(void);
static int episodeSelfTest(void);
static int cogPicking(void);
static int cogPickControl(SithThing *player);
static void placedCogsMenu(int resume);
static void updateCogPick(EdVec origin,EdVec direction);
static void drawCogPicker(int color);
static int readPlacedCogs(EdModel *source,const char *text);
static int writeCogSources(const EdModel *source,const char *path);
static char *cogSourceText(const char *name);
static void suspendPlacedCogs(int geometry);
static int startPlacedCogs(const EdModel *source);
static void closeCogEditor(void);
static int cogSelfTest(void);
static int stressCogs(SithThing *player);
static int stressFrames(SithThing *player);
static int framesSelfTest(void);
static int cogReloadTest(void);
static int deleteEditorObject(int id);
static int objectPlacement(SithThing *definition,EdVec direction,EdVec *position);
static EdVec objectFacingRotation(EdVec rotation,EdVec facing,int freeRotation);
static EdVec objectFacingHandle(const EdModel *source,int id);
static int pickObjectFacingHandle(EdVec origin,EdVec direction,double wallDistance);
static int objectOverlayVisible(EdVec position);
static int thingOverlayVisible(int id);
static void captureOverlayVisibility(void);
static int overlayVisibilityTest(void);
static void thingAttributesDialog(int id);
static EdThingAttributes *thingAttributes(EdModel *source,int id);
static EdFrame thingPose(const EdModel *source,int id);
static void setThingPose(EdModel *source,int id,EdFrame pose);
static int frameId(int parent,int frame);
static int frameParent(int id);
static int frameIndex(int id);
static const char *thingTemplate(const EdModel *source,int id);
static int pickEditorFrame(EdVec origin,EdVec direction,double wallDistance,int handles);
static void readThingAttributes(EdThingAttributes *a,const SithThing *thing);
static void syncThingAttributes(SithThing *thing,const EdThingAttributes *a);
static void drawFrameGhosts(void);
static int netApplying,netAuthoredSpawns;
static int netClient(void);
static int netPropose(const EdModel *proposal);
static int netHistory(int redo);
static void netCommitted(void);
static void netRemember(void);
static void netClose(void);
static int netSelfTest(void);
static void netSyncObjectGuid(SithThing *thing,int id);
static void netControlReady(SithThing *player);
static int netEditPending(void);
static int stressNetwork(SithThing *player);
static char *netCogSource(const char *name);
/* Stock archives are read-only sources. Imported documents are separate episodes. */
static int stockEpisode(const char *name){return name&&(!__strcmpi(name,"jk1")||!__strcmpi(name,"jk1mp")||!__strcmpi(name,"jk1.gob")||!__strcmpi(name,"jk1mp.gob"));}
static int editorWorld(SithWorld *world){
    if(world&&world==boundWorld&&model)return 1;
    if(!world||!world->map_jkl_fname[0])return 0;
    if(stockEpisode(jkRes_episodeGobName))return 1;if(!__strcmpi(world->map_jkl_fname,"sithed.jkl"))return 1;
    stdFile_t file=jkRes_FileOpen("misc\\sithed_editor.txt","rt");char name[64]={0};if(file){jkRes_FileGets(file,name,sizeof(name));jkRes_FileClose(file);}name[strcspn(name,"\r\n\t ")]=0;
    return name[0]&&!__strcmpi(name,world->map_jkl_fname);
}
int jkEditor_ResourceCapacity(SithWorld *world,int requested,int minimum) try {return requested<minimum&&editorWorld(world)?minimum:requested;} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
static stdFile_t openEditorJkl(const char *mode){char path[64];snprintf(path,sizeof(path),"jkl\\%s",sithWorld_g_pCurrentWorld->map_jkl_fname);return jkRes_FileOpen(path,mode);}
static int historyStep(int redo);
static void clearHistoryEntries(EditorHistoryEntry *entries,int *count) {
    for(int i=0;i<*count;i++)edFree(entries[i].data);edZero(entries,0,sizeof(EditorHistoryEntry)*EDITOR_UNDOS);*count=0;
}
static void clearHistory(void) {
    clearHistoryEntries(undoHistory,&undoCount);clearHistoryEntries(redoHistory,&redoCount);pendingUndo=historyDown=0;
}
static EditorHistoryEntry historyEntry(const EdModel *source,const EdSelection *selected) {
    EditorHistoryEntry entry={0};unsigned char *wire=edWireEncode(source,&entry.rawSize);
    if(wire){entry.data=edNetPack(wire,entry.rawSize,&entry.size);edFree(wire);if(entry.data)entry.selection=*selected;}return entry;
}
static void pushHistory(EditorHistoryEntry *entries,int *count,EditorHistoryEntry entry) {
    if(*count==EDITOR_UNDOS){edFree(entries[0].data);memmove(entries,entries+1,sizeof(*entries)*(EDITOR_UNDOS-1));(*count)--;}
    entries[(*count)++]=entry;
}
static void captureChoiceMenu(jkGuiMenu *menu) {
    static int frames;if(++frames<3)return;
#ifdef SDL2_RENDER
    std3D_ScreenshotWindow(getenv("GEOSITH_SECTION_CAPTURE"));
#endif
    menu->lastClicked=103;printf("GEOSITH_SECTION_MENU PASS\n");fflush(stdout);
}
static void drawMaterialPreview(tVBuffer *buffer,int material,int x,int y,int width,int height);
static EdVec vec(rdVector3 p) { return edV(p.x,p.y,p.z); }
static rdVector3 rdvec(EdVec p) { rdVector3 q={(flex_t)p.x,(flex_t)p.y,(flex_t)p.z}; return q; }
static void say(const char *s) { snprintf(message,sizeof(message),"%s",s); sithConsole_PrintString(message); }
static EdResources customResources;
static EdResources::Bytes customResourceBytes;
static bool customResourcesReady,resourceForceMaterials;
static std::vector<rdMaterial*> retiredResourceMaterials;
static std::string customResourceError;
static void setCustomResources(EdResources data){auto encoded=data.encode();customResources=std::move(data);customResourceBytes=std::move(encoded);customResourcesReady=true;customResourceError.clear();}
void jkEditor_ResourcesLoad(void)try {
    resourceForceMaterials=false;
    EdResources draft;draft.pngToMat=edPngToMat;draft.discover(getenv("GEOSITH_MODS_DIR")?getenv("GEOSITH_MODS_DIR"):"mods");
    if(!stockEpisode(jkRes_episodeGobName))for(int d=1;d<=2;d++)for(int g=0;g<jkRes_gCtx.aGobDirectories[d].numGobs;g++){
        Gob *gob=jkRes_gCtx.aGobDirectories[d].gobs[g];if(gob)draft.archive(gob->fpath);
    }
    setCustomResources(std::move(draft));
}catch(const std::exception &e){customResources.files.clear();customResourceBytes=customResources.encode();customResourcesReady=true;customResourceError=e.what();stdPlatform_Printf("GeoSith resources: %s\n",e.what());}
void *jkEditor_ResourceOpen(const char *name,const char *mode)try {
    if(!mode||mode[0]!='r'||strchr(mode,'+'))return NULL;
    if(!customResourcesReady)jkEditor_ResourcesLoad();
    auto found=customResources.files.find(EdResources::path(name));if(found==customResources.files.end())return NULL;
    return new EdResourceFile{found->second};
}catch(const std::exception&){return NULL;}
void jkEditor_ResourceClose(void *f){delete (EdResourceFile*)f;}
size_t jkEditor_ResourceRead(void *f,void *out,size_t n){return ((EdResourceFile*)f)->read(out,n);}
const char *jkEditor_ResourceGets(void *f,char *out,size_t n){return ((EdResourceFile*)f)->gets(out,n);}
int jkEditor_ResourceSeek(void *f,int n,int w){return ((EdResourceFile*)f)->seek(n,w);}
int jkEditor_ResourceTell(void *f){return (int)((EdResourceFile*)f)->position;}
int jkEditor_ResourceSize(void *f){return (int)((EdResourceFile*)f)->data->size();}
int jkEditor_ResourceEof(void *f){return ((EdResourceFile*)f)->eof;}
static bool customMaterial(const char *name){return customResources.files.count(std::string("mat/")+name)||customResources.files.count(std::string("3do/mat/")+name);}
static int writeCustomResources(const EdModel *source,const char *path){
    if(!customResourceError.empty()){say(customResourceError.c_str());return 0;}
    for(auto &f:customResources.files){
        // Authored models are serialized by edWriteGob and win over imports.
        bool authored=false;for(int a=0;a<source->assetCount;a++)if(f.first==std::string("3do/")+source->assets[a].name+".3do")authored=true;
        if(authored)continue;
        std::string name=f.first;std::replace(name.begin(),name.end(),'/','\\');
        if(!edGobAddFile(path,name.c_str(),f.second->data(),(unsigned)f.second->size()))return 0;
    }return 1;
}
static int readResources(void) {
    if(resources) return 1;
    FILE *f=fopen("editor-resources.jkl","rb"); if(!f) { say("Run tools/prepare_assets.py before creating a level."); return 0; }
    fseek(f,0,SEEK_END); long n=ftell(f); rewind(f);
    if(n<1||n>4000000) { fclose(f); return 0; }
    resources=(char*)edAlloc(n+1); if(!resources) { fclose(f); return 0; }
    if(fread(resources,1,n,f)!=(size_t)n) { edFree(resources);resources=NULL;fclose(f);return 0; }
    resources[n]=0; fclose(f); return 1;
}
static const char *baseName(const char *name) {const char *base=name;for(const char *p=name;*p;p++)if(*p=='/'||*p=='\\')base=p+1;return base;}
static void freeEditorMaterials(void) {
    rdMaterialUnloader_t previous=rdMaterial_RegisterUnloader(NULL);
    for(int i=0;i<paletteCount;i++)if(paletteOwned[i]&&palette[i])rdMaterial_Free(palette[i]);
    for(auto *mat:retiredResourceMaterials)rdMaterial_Free(mat);retiredResourceMaterials.clear();
    rdMaterial_RegisterUnloader(previous);
    edZero(palette,0,sizeof(palette));edZero(paletteOwned,0,sizeof(paletteOwned));paletteCount=0;
}
static std::vector<std::string> liveSectorColormaps;
static int sectorColormapAvailable(const char *name){
    char path[160];snprintf(path,sizeof(path),"misc\\cmp\\%s",name);rdColormap map={0};int ok=rdColormap_LoadEntry(path,&map);rdColormap_FreeEntry(&map);return ok;
}
static int addMaterialName(const char *name) {
    name=baseName(name);size_t length=strlen(name);
    if(length<5||length>=128||__strcmpi(name+length-4,".mat"))return -1;
    for(int i=0;i<paletteCount;i++)if(!__strcmpi(paletteNames[i],name))return i;
    if(paletteCount==EDITOR_MATERIALS)return -1;
    int id=paletteCount++;snprintf(paletteNames[id],128,"%s",name);return id;
}
static int materialNameCompare(const void *a,const void *b) {return __strcmpi((const char*)a,(const char*)b);}
static int addColormap(const char *name) {
    name=baseName(name);for(int i=0;i<colormapCount;i++)if(!__strcmpi(name,editorColormaps[i].name))return i;
    if(colormapCount>=EDITOR_COLORMAPS||strlen(name)>=128)return 0;
    int id=colormapCount++;snprintf(editorColormaps[id].name,128,"%s",name);return id;
}
static rdColor24 *colormapColors(int id) {
    if(id<0||id>=colormapCount)return sithWorld_g_pCurrentWorld->colormaps[0].colors;
    EditorColormap *cmp=&editorColormaps[id];
    if(!cmp->loaded) {
        char path[160];snprintf(path,sizeof(path),"misc\\cmp\\%s",cmp->name);
        stdFile_t file=jkRes_FileOpen(path,"rb");unsigned char data[64+768];
        cmp->loaded=-1;if(file){if(jkRes_FileRead(file,data,sizeof(data))==sizeof(data)&&!edCompare(data,"CMP ",4)){edCopy(cmp->colors,data+64,768);cmp->loaded=1;}jkRes_FileClose(file);}
    }
    return cmp->loaded==1?cmp->colors:sithWorld_g_pCurrentWorld->colormaps[0].colors;
}
/* 0=All, 1=Models, 2=Other, 3..102=level prefixes 00..99. */
static void materialMetadata(void) {
    colormapCount=0;edZero(editorColormaps,0,sizeof(editorColormaps));addColormap("dflt.cmp");
    for(int i=0;i<paletteCount;i++) {
        const char *n=paletteNames[i];materialSection[i]=isdigit((unsigned char)n[0])&&isdigit((unsigned char)n[1])?3+(n[0]-'0')*10+n[1]-'0':2;
        materialColormap[i]=0;
    }
    /* Model provenance takes priority over a numeric filename prefix. */
    for(int d=0;d<5;d++)for(int g=0;g<jkRes_gCtx.aGobDirectories[d].numGobs;g++) {
        Gob *gob=jkRes_gCtx.aGobDirectories[d].gobs[g];if(!gob)continue;
#ifndef STDGOB_COMPACT_ENTRIES
        for(unsigned int e=0;e<gob->numFiles;e++) {
            const char *name=gob->entries[e].fname;
            if(!strncmp(name,"misc\\cmp\\",9))addColormap(baseName(name));
            if(!strncmp(name,"3do\\mat\\",8))for(int i=0;i<paletteCount;i++)if(!__strcmpi(baseName(name),paletteNames[i]))materialSection[i]=1;
        }
#endif
    }
    for(int d=0;d<5;d++)for(int type=0;type<2;type++) {
        jkResGobDirectory *directory=&jkRes_gCtx.aGobDirectories[d];char path[256];
        snprintf(path,sizeof(path),"%s%s%s",directory->name,directory->name[0]?"/":"",type?"3do/mat":"misc/cmp");
        stdFileSearch *search=stdFileUtil_NewFind(path,3,type?"mat":"cmp");stdFileSearchResult entry;
        if(search){while(stdFileUtil_FindNext(search,&entry))if(!entry.is_subdirectory){if(!type)addColormap(baseName(entry.fpath));else for(int i=0;i<paletteCount;i++)if(!__strcmpi(baseName(entry.fpath),paletteNames[i]))materialSection[i]=1;}stdFileUtil_DisposeFind(search);}
    }
    FILE *file=fopen("editor-materials.tsv","rb");if(file) {
        char line[400],name[128],kind[32],cmp[128];
        while(fgets(line,sizeof(line),file))if(sscanf(line,"%127s %31s %127s",name,kind,cmp)==3&&name[0]!='#')
            for(int i=0;i<paletteCount;i++)if(!__strcmpi(name,paletteNames[i])){materialColormap[i]=addColormap(cmp);if(!strcmp(kind,"models"))materialSection[i]=1;break;}
        fclose(file);
    }
    for(int i=0;i<paletteCount;i++)if(customMaterial(paletteNames[i]))materialSection[i]=103;
    for(auto &f:customResources.files)if(f.first.ends_with(".cmp"))addColormap(f.first.c_str());
}
static void discoverMaterials(void) {
    for(auto &f:customResources.files)if(f.first.ends_with(".mat"))addMaterialName(f.first.c_str());
    for(int d=0;d<5;d++) {
        jkResGobDirectory *directory=&jkRes_gCtx.aGobDirectories[d];
        for(int g=0;g<directory->numGobs;g++) {
            Gob *gob=directory->gobs[g];if(!gob)continue;
#ifndef STDGOB_COMPACT_ENTRIES
            for(unsigned int i=0;i<gob->numFiles;i++) {
                const char *name=gob->entries[i].fname;
                if(!strncmp(name,"mat\\",4)||!strncmp(name,"3do\\mat\\",8))addMaterialName(name);
            }
#endif
        }
        for(int sub=0;sub<2;sub++) {
            char path[256];snprintf(path,sizeof(path),"%s%s%s",directory->name,directory->name[0]?"/":"",sub?"3do/mat":"mat");
            stdFileSearch *search=stdFileUtil_NewFind(path,3,"mat");stdFileSearchResult entry;
            if(search){while(stdFileUtil_FindNext(search,&entry))if(!entry.is_subdirectory)addMaterialName(entry.fpath);stdFileUtil_DisposeFind(search);}
        }
    }
    if(paletteCount>baseMaterialCount)qsort(paletteNames+baseMaterialCount,paletteCount-baseMaterialCount,sizeof(paletteNames[0]),materialNameCompare);
    materialMetadata();
}
static rdMaterial *editorMaterial(int id) {
    if(id<0||id>=paletteCount)return NULL;
    if(palette[id])return palette[id];
    SithWorld *worlds[2]={sithWorld_g_pCurrentWorld,sithWorld_g_pStaticWorld};
    for(int w=0;w<2;w++)if(!resourceForceMaterials&&!customMaterial(paletteNames[id])&&worlds[w])for(int i=0;i<worlds[w]->numMaterials;i++)
        if(!__strcmpi(baseName(worlds[w]->aMaterials[i].mat_fpath),paletteNames[id]))return palette[id]=&worlds[w]->aMaterials[i];
    rdMaterialLoader_t previous=rdMaterial_RegisterLoader(NULL);
    char path[256];snprintf(path,sizeof(path),"mat\\%s",paletteNames[id]);palette[id]=rdMaterial_Load(path,0,0);
    if(!palette[id]){snprintf(path,sizeof(path),"3do\\mat\\%s",paletteNames[id]);palette[id]=rdMaterial_Load(path,0,0);}
    rdMaterial_RegisterLoader(previous);
    if(palette[id])paletteOwned[id]=1;return palette[id];
}
static int palettePixel(tVBuffer *buffer,int r,int g,int b) {
    rdColor24 *colors=(rdColor24*)stdDisplay_masterPalette;
    int color=1+stdColor_FindClosest(colors+1,255,r,g,b);
    return buffer->format.format.bpp==8?color:stdColor_Indexed8ToRGB16(color,colors,&buffer->format.format);
}
#include "jkEditorText.h"
static void drawMaterialPreview(tVBuffer *buffer,int material,int x,int y,int width,int height) {
    rdMaterial *mat=editorMaterial(material);if(!mat||!buffer||width<1||height<1)return;
    if(!rdMaterial_EnsureDataForced(mat)||!mat->num_texinfo||!mat->texinfos[0])return;
    rdTexinfo *info=mat->texinfos[0];tVBuffer *source=info->texture_ptr?info->texture_ptr->texture_struct[0]:NULL;
    rdColor24 *colors=mat->palette_alloc?mat->palette_alloc:colormapColors(materialColormap[material]);
    int lut[256];for(int i=0;i<256;i++)lut[i]=palettePixel(buffer,colors[i].r,colors[i].g,colors[i].b);
    int checker[2]={palettePixel(buffer,30,30,30),palettePixel(buffer,70,70,70)};
    // Added: unlock the preview source only after a successful lock
    if(source){int locked=stdDisplay_VBufferLock(source);if(!locked||!source->surface_lock_alloc){if(locked)stdDisplay_VBufferUnlock(source);source=NULL;}}
    int destinationLocked=stdDisplay_VBufferLock(buffer);
    if(!destinationLocked||!buffer->surface_lock_alloc){if(destinationLocked)stdDisplay_VBufferUnlock(buffer);if(source)stdDisplay_VBufferUnlock(source);return;}
    int fitWidth=width,fitHeight=height;
    if(source&&source->format.width>0&&source->format.height>0) {
        double scale=fmin((double)width/source->format.width,(double)height/source->format.height);
        fitWidth=(int)(source->format.width*scale);fitHeight=(int)(source->format.height*scale);
        if(fitWidth<1)fitWidth=1;if(fitHeight<1)fitHeight=1;
    }
    int left=(width-fitWidth)/2,top=(height-fitHeight)/2;
    for(int row=0;row<height;row++)for(int col=0;col<width;col++) {
        int dx=x+col,dy=y+row;if(dx<0||dy<0||dx>=buffer->format.width||dy>=buffer->format.height)continue;
        int color=lut[info->header.solidColor&255];
        if(col<left||row<top||col>=left+fitWidth||row>=top+fitHeight)color=checker[0];
        else if(source&&source->format.width>0&&source->format.height>0) {
            int sx=(col-left)*source->format.width/fitWidth,sy=(row-top)*source->format.height/fitHeight;
            unsigned char *pixel=(unsigned char*)source->surface_lock_alloc+sy*source->format.rowSize+sx*(source->format.format.bpp/8);
            if(source->format.format.bpp==8)color=info->texture_ptr->alpha_en&&pixel[0]==info->texture_ptr->color_transparent?checker[((col/8)+(row/8))&1]:lut[pixel[0]];
            else if(source->format.format.bpp==16) {
                uint16_t value;edCopy(&value,pixel,2);rdTexFormat *fmt=&source->format.format;
                int r=fmt->r_bits?((value>>fmt->r_shift)&((1u<<fmt->r_bits)-1))*255/((1u<<fmt->r_bits)-1):0;
                int g=fmt->g_bits?((value>>fmt->g_shift)&((1u<<fmt->g_bits)-1))*255/((1u<<fmt->g_bits)-1):0;
                int b=fmt->b_bits?((value>>fmt->b_shift)&((1u<<fmt->b_bits)-1))*255/((1u<<fmt->b_bits)-1):0;
                color=palettePixel(buffer,r,g,b);
                if(info->texture_ptr->alpha_en&&value==info->texture_ptr->color_transparent)color=checker[((col/8)+(row/8))&1];
            }
        }
        unsigned char *destination=(unsigned char*)buffer->surface_lock_alloc+dy*buffer->format.rowSize+dx*(buffer->format.format.bpp/8);
        if(buffer->format.format.bpp==8)stdPlatform_WriteByte16(destination,(uint8_t)color);else if(buffer->format.format.bpp==16){uint16_t value=(uint16_t)color;edCopy(destination,&value,2);}
    }
    stdDisplay_VBufferUnlock(buffer);if(source)stdDisplay_VBufferUnlock(source);
}
static void materialTile(jkGuiElement *element,jkGuiMenu *menu,tVBuffer *buffer,BOOL redraw) {
    if(redraw)jkGuiRend_CopyVBuffer(menu,&element->rect);
    int id=(int)element->otherDataPtr;
    drawMaterialPreview(buffer,id,element->rect.x+3,element->rect.y+3,element->rect.width-6,element->rect.height-23);
    stdDisplay_VBufferLock(buffer);
    rdCanvas canvas={0};canvas.pVBuffer=buffer;canvas.widthMinusOne=buffer->format.width-1;canvas.heightMinusOne=buffer->format.height-1;
    int color=palettePixel(buffer,id==currentMaterial?255:menu->lastMouseOverClickable==element?80:140,id==currentMaterial?70:220,id==currentMaterial?60:220);
    rdPrimit2_DrawRectangle(&canvas,element->rect.x,element->rect.y,element->rect.x+element->rect.width-1,element->rect.y+element->rect.height-1,color,-1);
    stdFont_DrawAscii(buffer,menu->fonts[0],element->rect.x+3,element->rect.y+element->rect.height-18,element->rect.width-6,paletteNames[id],1);
    stdDisplay_VBufferUnlock(buffer);
}
#include "jkEditorPreview.h"
static void drawObjectPreview(tVBuffer *buffer,int id,int x,int y,int width,int height) {
    if(id<0||id>=templateCount||width<8||height<8)return;SithThing *object=sithTemplate_GetTemplate(templateNames[id]);if(!object)return;
    if(object->renderData.type==RD_THING_SPRITE3&&object->renderData.sprite3){rdMaterial *mat=object->renderData.sprite3->face.material;
        for(int i=0;mat&&i<paletteCount;i++)if(!__strcmpi(baseName(mat->mat_fpath),paletteNames[i])){drawMaterialPreview(buffer,i,x,y,width,height);return;}}
    rdModel3 *mesh=object->renderData.type==RD_THING_MODEL3?object->renderData.model3:NULL;
    EditorPreviewCache *preview=mesh&&mesh->numGeos?renderObjectPreview(buffer,mesh,id,width,height):NULL;
    if(!stdDisplay_VBufferLock(buffer)||!buffer->surface_lock_alloc)return;
    if(preview){for(int row=0;row<height;row++)for(int col=0;col<width;col++){
        int dx=x+col,dy=y+row;uint32_t color=preview->pixels[row*width+col];if(color==0x10000||dx<0||dy<0||dx>=buffer->format.width||dy>=buffer->format.height)continue;
        unsigned char *destination=(unsigned char*)buffer->surface_lock_alloc+dy*buffer->format.rowSize+dx*(buffer->format.format.bpp/8);
        if(buffer->format.format.bpp==8)stdPlatform_WriteByte16(destination,(uint8_t)color);else if(buffer->format.format.bpp==16){uint16_t value=(uint16_t)color;edCopy(destination,&value,2);}
    }}else{
        rdCanvas canvas={0};canvas.pVBuffer=buffer;canvas.widthMinusOne=buffer->format.width-1;canvas.heightMinusOne=buffer->format.height-1;int color=palettePixel(buffer,120,220,255);
        rdPrimit2_DrawClippedLine(&canvas,x+width/2,y+4,x+width-4,y+height/2,color,-1);rdPrimit2_DrawClippedLine(&canvas,x+width-4,y+height/2,x+width/2,y+height-4,color,-1);
        rdPrimit2_DrawClippedLine(&canvas,x+width/2,y+height-4,x+4,y+height/2,color,-1);rdPrimit2_DrawClippedLine(&canvas,x+4,y+height/2,x+width/2,y+4,color,-1);
    }
    stdDisplay_VBufferUnlock(buffer);
}
static void objectTile(jkGuiElement *element,jkGuiMenu *menu,tVBuffer *buffer,BOOL redraw) {
    if(redraw)jkGuiRend_CopyVBuffer(menu,&element->rect);int id=(int)element->otherDataPtr;
    drawObjectPreview(buffer,id,element->rect.x+3,element->rect.y+3,element->rect.width-6,element->rect.height-23);
    stdDisplay_VBufferLock(buffer);stdFont_DrawAscii(buffer,menu->fonts[0],element->rect.x+3,element->rect.y+element->rect.height-18,element->rect.width-6,templateNames[id],1);stdDisplay_VBufferUnlock(buffer);
}
static int materialMatches(const char *name,const char16_t *query) {
    char filter[64];int i=0;for(;i<63&&query[i];i++)filter[i]=(char)tolower((unsigned char)query[i]);filter[i]=0;
    char lower[128];for(i=0;i<127&&name[i];i++)lower[i]=(char)tolower((unsigned char)name[i]);lower[i]=0;
    return strstr(lower,filter)!=NULL;
}
static int templateIndex(const char *name) {
    for(int i=0;i<templateCount;i++)if(!__strcmpi(name,templateNames[i]))return i;return -1;
}
static void templateMetadataLine(const char *line,char *description) {
    if(!strncmp(line,"# DESC:",7)){snprintf(description,256,"%s",line+7);return;}
    char name[128],parent[128];if(sscanf(line," %127s %127s",name,parent)!=2||name[0]=='#')return;
    int id=templateIndex(name);if(id<0)return;templateParents[id]=templateIndex(parent);
    snprintf(templateHints[id],sizeof(templateHints[id]),"%s %s",description,line);description[0]=0;
    for(char *p=templateHints[id];*p;p++)*p=(char)tolower((unsigned char)*p);
}
static int templateDescends(int id,const char *base) {
    for(int n=0;id>=0&&n<templateCount;n++,id=templateParents[id])if(!__strcmpi(templateNames[id],base))return 1;return 0;
}
static void templateMetadata(void) {
    edZero(templateHints,0,sizeof(templateHints));for(int i=0;i<templateCount;i++)templateParents[i]=-1;
    char line[4096],description[256]="";
    /* The saved sidecar keeps inheritance available when the GOB is moved. */
    stdFile_t file=jkRes_FileOpen("misc\\sithed_templates.tpl","rt");
    if(file){while(jkRes_FileGets(file,line,sizeof(line)))templateMetadataLine(line,description);jkRes_FileClose(file);}
    const char *paths[]={"../master.tpl","../mater.tpl","master.tpl","mater.tpl"};
    for(int p=0;p<4;p++){FILE *source=fopen(paths[p],"rt");if(!source)continue;while(fgets(line,sizeof(line),source))templateMetadataLine(line,description);fclose(source);break;}
    for(int i=0;i<templateCount;i++){
        SithThing *object=sithTemplate_GetTemplate(templateNames[i]);unsigned int bits=0;
        if(object&&object->renderData.type==RD_THING_SPRITE3)bits|=1u<<1;
        if(templateDescends(i,"_walkstruct"))bits|=1u<<2;
        if(object&&object->type==SITH_THING_WEAPON)bits|=1u<<3;
        if(object&&object->type==SITH_THING_ITEM)bits|=1u<<4;
        /* Inherited hints also cover respawning and dropped pickup variants. */
        for(int ancestor=i,n=0;ancestor>=0&&n<templateCount;n++,ancestor=templateParents[ancestor]){
            char *h=templateHints[ancestor];
            if(strstr(h,"pow_energy.")||strstr(h,"pow_power.")||strstr(h,"pow_railcharges.")||strstr(h,"pow_thermal.")||strstr(h,"pow_sequencer.")||strstr(h," ammunition")||strstr(h,"pow_single_thermal.")||strstr(h,"pow_single_sequencer."))bits|=1u<<5;
            if(strstr(h,"pow_bryar.")||strstr(h,"pow_strifle.")||strstr(h,"pow_crossbow.")||strstr(h,"pow_repeater.")||strstr(h,"pow_railgun.")||strstr(h,"pow_drailgun.")||strstr(h,"pow_concrifle.")||strstr(h,"pow_rifle.")||strstr(h,"pow_dcrossbow.")||strstr(h,"pow_dstrifle.")||strstr(h,"pow_drepeater.")||strstr(h,"pow_dconcrifle.")||strstr(h,"pow_saber."))bits|=1u<<6;
            if((object&&object->renderData.type==RD_THING_MODEL3)&&(strstr(h,"spaceship")||strstr(h," ship")||strstr(h,"cargoship")||strstr(h,"tie fighter")||strstr(h,"tiebomber")||strstr(h,"tieinterceptor")||strstr(h,"shuttle")||strstr(h,"moldy")||strstr(h,"crow")||strstr(h,"xwing")||strstr(h,"tiefighter")||strstr(h,"tiebomb")))bits|=1u<<10;
            if(strstr(h,"_elev.snd")||strstr(h,"elevator")||strstr(h,"funicular")||strstr(h,"lift"))bits|=1u<<11;
        }
        if(object&&object->type==SITH_THING_ACTOR)bits|=1u<<(templateDescends(i,"_civilian")?8:7);
        if(templateDescends(i,"_darkjedi"))bits|=1u<<9;
        if((object&&object->type==SITH_THING_GHOST)||templateDescends(i,"_ghoststructure")||templateDescends(i,"_ghostdecor"))bits|=1u<<12;
        if(templateDescends(i,"_decor"))bits|=1u<<13;
        if(templateDescends(i,"_throwable"))bits|=1u<<14;
        templateSections[i]=bits?bits:1u<<15;
    }
}
static void freeMenuStrings(jkGuiElement *elements) {
    for(int i=0;elements[i].type!=ELEMENT_END;i++){if(elements[i].strAlloced)STD_FREE((void*)elements[i].strAlloced);if(elements[i].wHintTextAlloced)STD_FREE((void*)elements[i].wHintTextAlloced);}
}
static void captureObjectSections(jkGuiMenu *menu) {
    static int frames;if(++frames<3)return;
#ifdef SDL2_RENDER
    std3D_ScreenshotWindow(getenv("GEOSITH_OBJECT_SECTION_CAPTURE"));
#endif
    menu->lastClicked=102;printf("GEOSITH_OBJECT_SECTIONS PASS: choose Walkstructs\n");fflush(stdout);
}
static int chooseList(const char16_t *title,char16_t names[][128],int count,int current) {
    int page=current/24;
    for(;;) {
        jkGuiElement elements[29]={0};
        elements[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,12,600,40},1};
        for(int i=0;i<24;i++)elements[i+1]=jkGuiElement{ELEMENT_TEXTBUTTON,100+i,0,NULL,3,{20+(i%4)*150,70+(i/4)*52,144,44},page*24+i<count};
        elements[25]=jkGuiElement{ELEMENT_TEXTBUTTON,2,2,NULL,3,{20,410,180,40},1};
        elements[26]=jkGuiElement{ELEMENT_TEXTBUTTON,3,2,NULL,3,{440,410,180,40},1};
        elements[27]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{230,410,180,40},1};
        elements[28].type=ELEMENT_END;
        jkGuiMenu menu={elements,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};
        if(getenv("GEOSITH_SECTION_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))menu.idkFunc=captureChoiceMenu;
        if(title[0]=='O'&&getenv("GEOSITH_OBJECT_SECTION_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))menu.idkFunc=captureObjectSections;
        if(title[0]=='3'&&getenv("GEOSITH_BUILDER_CAPTURE")&&getenv("GEOSITH_STRESS"))menu.idkFunc=builderCaptureMenu;
        jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);elements[0].wstr=title;
        for(int i=0;i<24;i++)if(page*24+i<count)elements[i+1].wstr=names[page*24+i];
        elements[25].wstr=u"Previous";elements[26].wstr=u"Next";
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[27]);
        int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(elements);
        if(choice>=100&&choice<124&&page*24+choice-100<count)return page*24+choice-100;
        if(choice==2){if(page>0)page--;}else if(choice==3){if((page+1)*24<count)page++;}else return -1;
    }
}
static void sectionName(int section,char *name,size_t size) {
    if(!section)snprintf(name,size,"All materials");else if(section==1)snprintf(name,size,"Models");
    else if(section==2)snprintf(name,size,"Other");else if(section==103)snprintf(name,size,"Custom");else snprintf(name,size,"Level %02d",section-3);
}
static int chooseMaterialSection(int current) {
    char16_t names[104][128];int ids[104],count=0,selected=0;
    for(int s=0;s<104;s++) {
        int present=s==0;for(int i=0;i<paletteCount&&!present;i++)present=materialSection[i]==s;
        if(!present)continue;char text[128];sectionName(s,text,sizeof(text));stdString_CharToWchar(names[count],text,128);
        if(s==current)selected=count;ids[count++]=s;
    }
    int choice=chooseList(u"Material Sections",names,count,selected);return choice<0?current:ids[choice];
}
static int chooseObjectSection(int current) {
    char16_t names[16][128];for(int i=0;i<16;i++)stdString_CharToWchar(names[i],objectSections[i],128);
    int choice=chooseList(u"Object Sections",names,16,current);return choice<0?current:choice;
}
static void captureMaterialMenu(jkGuiMenu *menu) {
    static int frames;
    if(++frames<3)return;
#ifdef SDL2_RENDER
    std3D_ScreenshotWindow(getenv("GEOSITH_MATERIAL_CAPTURE"));
#endif
    menu->lastClicked=100+(currentMaterial+(getenv("GEOSITH_SPLINE_MENU_CAPTURE")?1:0))%20;
    printf("GEOSITH_MATERIAL_MENU PASS: preview and choose material\n");fflush(stdout);
}
static void captureObjectMenu(jkGuiMenu *menu) {
    static int frames;if(++frames<3)return;
#ifdef SDL2_RENDER
    std3D_ScreenshotWindow(getenv("GEOSITH_OBJECT_CAPTURE"));
#endif
    menu->lastClicked=100;
    for(int i=8;i<28;i++)if(menu->paElements[i].bIsVisible){int id=(int)menu->paElements[i].otherDataPtr;SithThing *definition=sithTemplate_GetTemplate(templateNames[id]);if(definition&&definition->renderData.type==RD_THING_MODEL3){menu->lastClicked=menu->paElements[i].hoverId;break;}}
    printf("GEOSITH_OBJECT_MENU PASS\n");fflush(stdout);
}
static void materialMenu(int nested) {
    int objects=browserObjects;char (*names)[128]=objects?templateNames:paletteNames;int total=objects?templateCount:paletteCount;
    static int categories[2],pagesSaved[2];static char16_t queries[2][64];
    int section=categories[objects!=0],page=pagesSaved[objects!=0],ids[EDITOR_MATERIALS],locateSelection=1;
    char16_t query[64];stdString_SafeWStrCopy(query,queries[objects!=0],64);
    if(objects&&getenv("GEOSITH_OBJECT_CAPTURE")&&getenv("GEOSITH_STRESS"))stdString_SafeWStrCopy(query,u"crate",64);
    if(objects&&getenv("GEOSITH_OBJECT_QUERY")&&getenv("GEOSITH_STRESS"))stdString_CharToWchar(query,getenv("GEOSITH_OBJECT_QUERY"),64);
    SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;
    rdVector_Zero3(&player->physicsParams.vel);rdVector_Zero3(&player->physicsParams.acceleration);rdVector_Zero3(&player->physicsParams.deltaVelocity);
    int previousNetSafe=jkEditor_NetSafeDialog(1);int previousModal=jkMain_bInModalGui;jkMain_bInModalGui=1;if(!nested)sithTime_Pause();stdControl_SetActivation(0);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    if(getenv("GEOSITH_SECTION_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))section=chooseMaterialSection(section);
    if(objects&&getenv("GEOSITH_OBJECT_SECTION_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))section=chooseObjectSection(section);
    for(;;) {
        int count=0;for(int i=0;i<total;i++)if((!section||(objects?(templateSections[i]&(1u<<section))!=0:materialSection[i]==section))&&materialMatches(names[i],query))ids[count++]=i;
        /* Locate by filtered position, not catalog index; filters can reorder
           the visible pages and the current item can change via the wheel. */
        if(locateSelection){int selected=objects?currentTemplate:currentMaterial;for(int i=0;i<count;i++)if(ids[i]==selected){page=i/20;break;}locateSelection=0;}
        int pages=(count+19)/20;if(!pages)pages=1;if(page>=pages)page=pages-1;if(page<0)page=0;
        jkGuiElement elements[29]={
            {ELEMENT_TEXT,0,2,NULL,3,{20,12,600,32},1},
            {ELEMENT_TEXTBUTTON,5,0,NULL,3,{20,48,180,32},1},
            {ELEMENT_TEXTBOX,0,0,NULL,63,{210,50,214,28},1},
            {ELEMENT_TEXTBUTTON,4,2,NULL,3,{444,48,172,32},1},
            {ELEMENT_TEXTBUTTON,2,2,NULL,3,{20,424,150,36},1},
            {ELEMENT_TEXTBUTTON,3,2,NULL,3,{470,424,150,36},1},
            {ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{245,424,150,36},1},
            {ELEMENT_TEXT,0,0,NULL,3,{20,82,600,20},1}
        };
        for(int i=0;i<20;i++) {
            jkGuiElement *tile=&elements[8+i];tile->type=ELEMENT_TEXTBUTTON;tile->hoverId=100+i;tile->rect=rdRect{20+(i%5)*120,108+(i/5)*76,112,70};tile->bIsVisible=page*20+i<count;tile->drawFuncOverride=objects?objectTile:materialTile;
        }
        elements[28].type=ELEMENT_END;
        jkGuiMenu menu={elements,0,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};
        if(getenv("GEOSITH_MATERIAL_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))menu.idkFunc=captureMaterialMenu;
        if(objects&&getenv("GEOSITH_OBJECT_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))menu.idkFunc=captureObjectMenu;
        jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
        char16_t pageText[96],sectionText[128];char text[96],category[64];sectionName(section,category,sizeof(category));
        if(objects)snprintf(category,sizeof(category),"%s",objectSections[section]);
        snprintf(text,sizeof(text),"%s | %d items | Page %d / %d",category,count,page+1,pages);stdString_CharToWchar(pageText,text,96);
        stdString_CharToWchar(sectionText,category,128);
        elements[0].wstr=objects?u"Choose Object":u"Choose Material";elements[1].wstr=sectionText;elements[2].wstr=query;
        elements[3].wstr=u"Search";elements[4].wstr=u"Previous";elements[5].wstr=u"Next";elements[7].wstr=pageText;
        for(int i=0;i<20;i++){elements[8+i].wstr=u"";elements[8+i].otherDataPtr=page*20+i<count?ids[page*20+i]:0;}
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[6]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&elements[3]);
        int choice=jkGuiRend_DisplayAndReturnClicked(&menu);int id=choice>=100&&choice<120&&page*20+choice-100<count?ids[page*20+choice-100]:-1;
        freeMenuStrings(elements);
        if(id>=0){if(objects)currentTemplate=id;else if(editorMaterial(id))currentMaterial=id;break;}
        if(choice==2)page--;else if(choice==3)page++;else if(choice==4)page=0;else if(choice==5){section=objects?chooseObjectSection(section):chooseMaterialSection(section);page=0;}else break;
    }
    categories[objects!=0]=section;pagesSaved[objects!=0]=page;stdString_SafeWStrCopy(queries[objects!=0],query,64);
    if(getenv("GEOSITH_HEADER_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))jkEditor_HeaderDialog();
    if(!nested){jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();stdControl_SetActivation(1);}jkMain_bInModalGui=previousModal;jkEditor_NetSafeDialog(previousNetSafe);stdControl_Flush();
    char message[160];snprintf(message,sizeof(message),"%s: %s",objects?"Template":"Material",objects?templateNames[currentTemplate]:paletteNames[currentMaterial]);say(message);
}
static void shapeLabel(int sides,char *text,int size){
    const char *names[]={"Line cleave","Triangle","Square","Pentagon","Hexagon","Heptagon","Octagon","Nonagon","Decagon","Hendecagon","Dodecagon","Tridecagon","Tetradecagon","Pentadecagon","Hexadecagon","Heptadecagon","Octadecagon","Enneadecagon","Icosagon","Henicosagon","Docosagon","Tricosagon","Tetracosagon","Pentacosagon","Hexacosagon","Heptacosagon","Octacosagon","Enneacosagon","Triacontagon","Hentriacontagon","Dotriacontagon"};
    snprintf(text,size,"%s",names[sides>=3&&sides<=32?sides-2:0]);
}
static void shapeTile(jkGuiElement *element,jkGuiMenu *menu,tVBuffer *buffer,BOOL redraw){
    if(redraw)jkGuiRend_CopyVBuffer(menu,&element->rect);int sides=(int)element->otherDataPtr;
    stdDisplay_VBufferLock(buffer);rdCanvas canvas={0};canvas.pVBuffer=buffer;canvas.widthMinusOne=buffer->format.width-1;canvas.heightMinusOne=buffer->format.height-1;
    int color=palettePixel(buffer,sides==cleaveShape?255:100,255,50),cx=element->rect.x+element->rect.width/2,cy=element->rect.y+24;double radius=21;
    if(!sides)rdPrimit2_DrawLine(&canvas,cx-26,cy+12,cx+26,cy-12,color,-1);
    else{double start=1.5707963267948966+(sides%2?0:3.141592653589793/sides);for(int i=0;i<sides;i++){double a=start+6.283185307179586*i/sides,b=start+6.283185307179586*(i+1)/sides;rdPrimit2_DrawLine(&canvas,cx+(int)(radius*cos(a)),cy-(int)(radius*sin(a)),cx+(int)(radius*cos(b)),cy-(int)(radius*sin(b)),color,-1);}}
    char label[48];shapeLabel(sides,label,sizeof(label));
    if(strlen(label)>10){char first[16];int split=(int)strlen(label)/2;edCopy(first,label,split);first[split]='-';first[split+1]=0;stdFont_DrawAscii(buffer,menu->fonts[0],element->rect.x+4,element->rect.y+49,element->rect.width-8,first,1);stdFont_DrawAscii(buffer,menu->fonts[0],element->rect.x+4,element->rect.y+64,element->rect.width-8,label+split,1);}
    else stdFont_DrawAscii(buffer,menu->fonts[0],element->rect.x+4,element->rect.y+56,element->rect.width-8,label,1);stdDisplay_VBufferUnlock(buffer);
}
static void testShapeMenu(jkGuiMenu *menu){static int frames;if(++frames<3)return;frames=0;
#ifdef SDL2_RENDER
    if(!menu->paElements[4].otherDataPtr&&getenv("GEOSITH_SHAPE_MENU_CAPTURE"))std3D_ScreenshotWindow(getenv("GEOSITH_SHAPE_MENU_CAPTURE"));
    if(menu->paElements[4].otherDataPtr&&getenv("GEOSITH_SHAPE_MENU_MORE_CAPTURE"))std3D_ScreenshotWindow(getenv("GEOSITH_SHAPE_MENU_MORE_CAPTURE"));
#endif
    menu->lastClicked=menu->paElements[4].otherDataPtr?110:2;
}
static void shapeTitle(jkGuiElement *element,jkGuiMenu *menu,tVBuffer *buffer,BOOL redraw){
    if(redraw)jkGuiRend_CopyVBuffer(menu,&element->rect);stdDisplay_VBufferLock(buffer);stdFont_DrawAscii(buffer,menu->fonts[2],element->rect.x+100,element->rect.y+8,element->rect.width-120,"Saber Cleave Shapes",1);stdDisplay_VBufferUnlock(buffer);
}
static void shapeMenu(void){
    SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;rdVector_Zero3(&player->physicsParams.vel);rdVector_Zero3(&player->physicsParams.acceleration);
    int previousSafe=jkEditor_NetSafeDialog(1),previousModal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    int page=cleaveShape>21?1:0;
    if(getenv("GEOSITH_STRESS")&&getenv("GEOSITH_SHAPES_TEST"))page=0;
    for(;;){jkGuiElement elements[25]={{ELEMENT_TEXT,0,2,NULL,3,{20,8,600,45},1},{ELEMENT_TEXTBUTTON,1,2,NULL,3,{20,425,150,35},1},{ELEMENT_TEXTBUTTON,2,2,NULL,3,{470,425,150,35},1},{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{245,425,150,35},1}};
        for(int i=0;i<20;i++){int index=page*20+i;jkGuiElement *tile=&elements[4+i];tile->type=ELEMENT_TEXTBUTTON;tile->hoverId=100+i;tile->rect=rdRect{20+(i%5)*120,65+(i/5)*85,112,78};tile->bIsVisible=index<31;tile->otherDataPtr=index?index+2:0;tile->drawFuncOverride=shapeTile;tile->wstr=u"";}
        elements[24].type=ELEMENT_END;jkGuiMenu menu={elements,0,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);elements[0].wstr=u"Saber Cleave Shapes";elements[1].wstr=u"Previous";elements[2].wstr=u"Next";
        elements[0].drawFuncOverride=shapeTitle;
        if(getenv("GEOSITH_STRESS")&&getenv("GEOSITH_SHAPES_TEST"))menu.idkFunc=testShapeMenu;
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[3]);int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(elements);
        if(choice>=100&&choice<120&&page*20+choice-100<31){int index=page*20+choice-100;cleaveShape=index?index+2:0;break;}
        if(choice==1)page=0;else if(choice==2)page=1;else break;
    }
    jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=previousModal;jkEditor_NetSafeDialog(previousSafe);stdControl_SetActivation(1);stdControl_Flush();
}
static void testSelectionMenu(jkGuiMenu *menu){static int frames;if(++frames<3)return;frames=0;
#ifdef SDL2_RENDER
    if(getenv("GEOSITH_CLEANUP_MENU_CAPTURE"))std3D_ScreenshotWindow(getenv("GEOSITH_CLEANUP_MENU_CAPTURE"));
#endif
    menu->lastClicked=2;
}
static int moveGrabbedSectors(EdVec delta){
    EdBuffer<unsigned char> mapped;
    if(!edMapTranslate(candidate,snapshot,grabSectors,delta,0,mapped)||!commit())return 0;
    sectorPicks=mapped;return 1;
}
static void selectionScale(int wheel){
    if(netEditPending())return;double factor=wheel>0?1.1:1.0/1.1;EdVec pivot;EdModel before;edCopyModel(&before,model);EdSelection selected=picks;EdBuffer<unsigned char> sectors=sectorPicks;
    if(!edScaleSelection(candidate,model,&selected,sectors,factor,&pivot)){say("Scale rejected: select surfaces or sectors; geometry must stay planar and convex. Convert editable splines first.");return;}
    remember();if(!commit())return;
    if(selectedSectorCount()){sectorPicks.clear();for(int s=0;s<model->count;s++){EdVec center={};int n=0;for(int f=0;f<model->c[s].count;f++)for(int v=0;v<model->c[s].f[f].count;v++){center=edAdd(center,model->c[s].f[f].v[v].p);n++;}center=edAdd(pivot,edScale(edSub(edScale(center,1.0/n),pivot),1/factor));for(int b=0;b<before.count;b++)if(sectors[b]&&edContains(&before.c[b],center)){sectorPicks[s]=1;break;}}}
    else {picks.count=0;for(int i=0;i<selected.count;i++){EdHit h=selected.hits[i];const EdFace &old=before.c[h.cell].f[h.face];EdVec center={};for(int v=0;v<old.count;v++)center=edAdd(center,old.v[v].p);center=edAdd(pivot,edScale(edSub(edScale(center,1.0/old.count),pivot),factor));EdVec normal=edNormal(&old);EdHit hit;
        if(edRaySelection(model,edAdd(center,edScale(normal,.001)),edScale(normal,-1),&hit)&&hit.distance<.002){hit.edge=hit.vertex=-1;edSelectionToggle(model,&picks,hit);}}}
    say(wheel>0?"Selection scaled up 10%. Ctrl+Z: undo.":"Selection scaled down. Ctrl+Z: undo.");
}
static void selectionWheel(int wheel){
    if(!wheel)return;selectionCleanup=!selectionCleanup;edResetDoubleTap(&selectionTap,1);
    say(selectionCleanup?"Clean Up: aim at a surface and press Fire 1 to simplify its sector.":"Select: Fire 1 selects; Fire 2 moves the selection.");
}
static void selectionMenu(void){
    SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;rdVector_Zero3(&player->physicsParams.vel);rdVector_Zero3(&player->physicsParams.acceleration);
    int previousSafe=jkEditor_NetSafeDialog(1),previousModal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    jkGuiElement elements[]={{ELEMENT_TEXT,0,2,NULL,3,{40,50,560,45},1},{ELEMENT_TEXTBUTTON,1,2,NULL,3,{170,145,300,55},1},{ELEMENT_TEXTBUTTON,2,2,NULL,3,{170,220,300,55},1},{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{220,375,200,40},1},{ELEMENT_END}};
    jkGuiMenu menu={elements,0,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
    elements[0].wstr=u"Selection Tool";elements[1].wstr=u"Select";elements[2].wstr=u"Clean Up";
    if(getenv("GEOSITH_STRESS")&&getenv("GEOSITH_CLEANUP_MENU_CAPTURE"))menu.idkFunc=testSelectionMenu;
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[3]);int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(elements);
    if(choice==1||choice==2)selectionCleanup=choice==2;
    jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=previousModal;jkEditor_NetSafeDialog(previousSafe);stdControl_SetActivation(1);stdControl_Flush();
    say(selectionCleanup?"Clean Up: aim at a surface and press Fire 1 to simplify its sector.":"Select: Fire 1 selects; Fire 2 moves the selection.");
}
static int cleanupAimedSector(void){
    if(hover.cell<0)return 0;remember();edCopyModel(candidate,model);
    if(!edCleanupSector(candidate,hover.cell)){say("No compatible surfaces or redundant vertices to clean up.");return 0;}
    if(!commit())return 0;picks.count=0;say("Sector cleaned up. Ctrl+Z: undo.");return 1;
}
static int writeEditorGob(const EdModel *source,const char *path,const char *title,int multiplayer) {
    const char *start=strstr(resources,"World materials");if(!start)start=strstr(resources,"world materials");
    const char *tail=start?strstr(start,"\nend"):NULL;if(!tail)return 0;
    tail=strchr(tail+1,'\n');if(!tail)tail=resources+strlen(resources);else tail++;
    int map[EDITOR_MATERIALS],ids[EDITOR_MATERIALS],count=baseMaterialCount;
    for(int i=0;i<EDITOR_MATERIALS;i++)map[i]=-1;
    for(int i=0;i<baseMaterialCount;i++){map[i]=i;ids[i]=i;}
    EdModel *mapped=(EdModel*)edAlloc(sizeof(*mapped));if(!mapped)return 0;edCopyModel(mapped,source);
    for(int s=0;s<mapped->count;s++)for(int f=0;f<mapped->c[s].count;f++) {
        int id=mapped->c[s].f[f].material;if(id<0||id>=paletteCount)id=0;
        if(map[id]<0){map[id]=count;ids[count++]=id;}mapped->c[s].f[f].material=map[id];
    }
    for(int s=0;s<mapped->meshCount;s++)for(int f=0;f<mapped->meshes[s].geometry.count;f++){
        int id=mapped->meshes[s].geometry.f[f].material;if(id<0||id>=paletteCount)id=0;
        if(map[id]<0){map[id]=count;ids[count++]=id;}mapped->meshes[s].geometry.f[f].material=map[id];
    }
    for(int i=0;i<mapped->splineCount;i++){int id=mapped->splines[i].material;if(id<0||id>=paletteCount)id=0;if(map[id]<0){map[id]=count;ids[count++]=id;}mapped->splines[i].material=map[id];}
    size_t capacity=strlen(resources)+(size_t)count*180+128;
    char *manifest=(char*)edAlloc(capacity);if(!manifest){edFree(mapped);return 0;}
    size_t used=start-resources;edCopy(manifest,resources,used);
    used+=snprintf(manifest+used,capacity-used,"World materials %d\n",count);
    for(int i=0;i<count;i++)used+=snprintf(manifest+used,capacity-used,"%d: %s 1.0 1.0\n",i,paletteNames[ids[i]]);
    snprintf(manifest+used,capacity-used,"end\n%s",tail);
    int ok=edWriteGob(mapped,path,title,multiplayer,manifest);if(ok)ok=writeCustomResources(mapped,path);if(ok)ok=writeCogSources(mapped,path);edFree(manifest);edFree(mapped);return ok;
}
int jkEditor_IsLevel(void) try { return editorWorld(sithWorld_g_pCurrentWorld); } catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
int jkEditor_Active(void) try { return active&&jkEditor_IsLevel(); } catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
int jkEditor_MapActive(void) try { return mapActive&&jkEditor_Active(); } catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
static int editorLevelFilename(const char16_t *input,char name[32]){
    int i=0;for(;i<31&&input[i];i++){int c=input[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return 0;name[i]=(char)c;}name[i]=0;return i>0&&!input[i];
}
static void testNewLevelMenu(jkGuiMenu *menu){static int frames;if(++frames<3)return;
#ifdef SDL2_RENDER
    if(getenv("GEOSITH_NAME_MENU_CAPTURE"))std3D_ScreenshotWindow(getenv("GEOSITH_NAME_MENU_CAPTURE"));
#endif
    menu->lastClicked=1;
}
static int newLevelName(char name[32]){
    char16_t input[32]=u"MyLevel";
    jkGuiElement elements[]={
        {ELEMENT_TEXT,0,2,NULL,3,{80,130,480,40},1},
        {ELEMENT_TEXTBOX,0,0,NULL,31,{120,200,400,30},1},
        {ELEMENT_TEXTBUTTON,1,2,NULL,3,{340,300,180,40},1},
        {ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{120,300,180,40},1},{ELEMENT_END,0}};
    jkGuiMenu menu={elements,0,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};
    jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);elements[0].wstr=u"New Level (letters, numbers, _ or -)";elements[1].wstr=input;elements[2].wstr=u"Create";
    if(getenv("GEOSITH_STRESS")&&getenv("GEOSITH_SHAPES_TEST")&&getenv("GEOSITH_NAME_TEST_PREFIX")){stdString_CharToWchar(input,getenv("GEOSITH_NAME_TEST_PREFIX"),32);menu.idkFunc=testNewLevelMenu;}
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[3]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&elements[2]);
    int ok=0;while(jkGuiRend_DisplayAndReturnClicked(&menu)==1){
        if(!editorLevelFilename(input,name)){jkGuiDialog_ErrorDialog(u"New Level",u"Use 1-31 letters, numbers, underscores or hyphens.");continue;}
        char path[128];snprintf(path,sizeof(path),"Episode/%s.gob",name);FILE *existing=fopen(path,"rb");
        if(existing){fclose(existing);jkGuiDialog_ErrorDialog(u"New Level",u"A level with this name already exists. Choose another name.");continue;}
        stdString_SafeWStrCopy(levelName,input,32);ok=1;break;
    }
    freeMenuStrings(elements);return ok;
}
int jkEditor_Create(int multiplayer) try {
    char name[32];if(!newLevelName(name))return 0;
    if(!readResources()) return 0;
    EdModel *fresh=(EdModel*)edAlloc(sizeof(EdModel)); if(!fresh) return 0; edDefault(fresh);
    if(multiplayer){
        /* Spread authored starts around the default room, clear of its walls
           and each other. Runtime networking slots remain separate. */
        fresh->spawnCount=4;
        for(int i=0;i<4;i++){fresh->spawns[i]=fresh->spawns[0];fresh->spawns[i].position=edV(i&1?.5:-.5,i&2?.5:-.5,.12);}
        fresh->spawn=fresh->spawns[0].position;
    }
    const char *mat=resources;int wallMaterial=0;
    while(mat&&*mat) {int id;char name[128];if(sscanf(mat," %d: %127s",&id,name)==2&&!strcmp(name,"dflt.mat")){wallMaterial=id;break;}mat=strchr(mat,'\n');if(mat)mat++;}
    for(int i=0;i<6;i++)fresh->c[0].f[i].material=wallMaterial;
    snprintf(fresh->header.title,sizeof(fresh->header.title),"%s",name);edEpisodeDefault(&fresh->episode,name,multiplayer);snprintf(fresh->episode.rows[0].title,sizeof(fresh->episode.rows[0].title),"%s",name);
    char path[128];snprintf(path,sizeof(path),"Episode/%s.gob",name);
    int ok=edWriteGob(fresh,path,name,multiplayer,resources);if(ok)ok=writeCustomResources(fresh,path); edFree(fresh);
    if(ok) { snprintf(jkRes_episodeGobName,32,"%s",name); multi=multiplayer; }
    else say("Could not create the episode GOB. Check the Episode folder.");
    return ok;
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 0;}
/* Build a replacement geometry allocation before releasing any live pointers. */
static int objectSector(const EdModel *source,EdVec point) {for(int s=0;s<source->count;s++)if(edContains(&source->c[s],point))return s;return -1;}
static int ensureThingSlots(SithWorld *world,int needed){
    if(needed>SITH_MAX_THINGS)return 0;
    if(needed>(int)world->numThingsLoaded){
        int old=world->numThingsLoaded,next=(int)fmin(SITH_MAX_THINGS,fmax(needed,old*2));
        world->numThingsLoaded=next;
        for(int i=next-1;i>=old;i--){SithThing *slot=&world->aThings[i];slot->idx=i;slot->signature=0;sithThing_Reset(slot);slot->guid=-1;sithThing_FreeThingIndex(i);}
    }return 1;
}
static void syncEditorObjects(const EdModel *source) {
    SithWorld *world=sithWorld_g_pCurrentWorld;if(!world||!world->aThings)return;
    if(!ensureThingSlots(world,source->objectCount+source->spawnCount+640))return;
    if(!syncCustomTemplates(source)){say("Could not load a custom template or model.");return;}
    int slots=source->objectCount>objectSlotCount?source->objectCount:objectSlotCount;objectSlotCount=source->objectCount;
    for(int i=0;i<slots;i++){
        SithThing *thing=objectThings[i]>=0&&objectThings[i]<(int)world->numThingsLoaded?&world->aThings[objectThings[i]]:NULL;
        if(thing&&(!thing->type||thing->signature!=objectSignatures[i]))thing=NULL;
        const EdObject *object=i<source->objectCount?&source->objects[i]:NULL;
        /* Preview/play changes replace the instance, not the authored object.
           Keep its network identity so queued gameplay packets still address
           the same object on peers with different allocation slots. */
        int previousGuid=thing?thing->guid:-1;
        if(thing&&(!object||objectProxyMode[i]!=active||__strcmpi(objectThingNames[i],object->templateName)||objectTemplateHashes[i]!=customTemplateHash(source,object->templateName))){sithThing_RemoveThing(thing);thing=NULL;}
        if(!object){objectThings[i]=-1;continue;}
        int sector=objectSector(source,object->position);if(sector<0)continue;
        rdVector3 angles=rdvec(object->rotation),position=rdvec(object->position);rdMatrix34 orient;rdMatrix_BuildRotate34(&orient,&angles);
        if(!thing){SithThing *definition=sithTemplate_GetTemplate(object->templateName);if(!definition)continue;
            if(active){thing=sithThing_Create(SITH_THING_COG);if(thing){sithThing_SetThingBasedOn(thing,definition);thing->type=SITH_THING_COG;thing->controlType=0;if(thing->moveType==SITH_MT_PATH&&thing->trackParams.aFrames)SITH_FREE(thing->trackParams.aFrames);edZero(&thing->trackParams,0,sizeof(thing->trackParams));thing->moveType=SITH_MT_NONE;thing->msecLifeLeft=0;thing->pCog=thing->pCaptureCog=NULL;thing->flags&=~(SITH_TF_DESTROYED|SITH_TF_DISABLED|SITH_TF_LEVELGEO|SITH_TF_10|SITH_TF_DEAD);thing->position=position;thing->orient=orient;sithThing_EnterSector(thing,&world->aSectors[sector],1,1);}}
            else thing=sithThing_CreateThingAtPos(definition,&position,&orient,&world->aSectors[sector],NULL);
            if(!thing)continue;if(sithNet_isMulti&&previousGuid>=0)thing->guid=previousGuid;objectThings[i]=thing->idx;objectSignatures[i]=thing->signature;objectProxyMode[i]=active;snprintf(objectThingNames[i],64,"%s",object->templateName);objectTemplateHashes[i]=customTemplateHash(source,object->templateName);
        }
        if(thing->attach_flags)sithThing_DetachThing(thing);thing->position=position;thing->orient=orient;sithThing_SetSector(thing,&world->aSectors[sector],1);
        if(!active)syncThingAttributes(thing,&object->attributes);
        netSyncObjectGuid(thing,i);
    }
}
/* Rebuilds replace surface allocations, not physical contact. Keep an existing
   contact only when its plane and supporting polygon survive the edit. This
   also handles cleaved/renumbered floors without replaying landing effects. */
static SithSurface *rebuiltSupport(SithThing *thing,SithWorld *oldWorld,SithWorld *geometry,const EdModel *m) {
    SithSurface *old=thing->attachedSurface;
    if(!old||old->surfaceInfo.face.numVertices<3||old->flags&SITH_SURFACE_SCROLLING)return NULL;
    EdVec normal=vec(old->surfaceInfo.face.normal);
    EdVec point=vec(oldWorld->aVertices[old->surfaceInfo.face.vertexPosIdx[0]]);
    double plane=edDot(normal,point);
    EdVec projected=edSub(vec(thing->position),edScale(normal,edDot(normal,vec(thing->position))-plane));
    rdVector3 contact=rdvec(projected);
    for(int s=0;s<m->count;s++) {
        if(!edContains(&m->c[s],vec(thing->position)))continue;
        SithSector *sector=&geometry->aSectors[s];
        for(int f=0;f<sector->numSurfaces;f++) {
            SithSurface *surface=&sector->surfaces[f];rdFace *face=&surface->surfaceInfo.face;
            if(!(surface->flags&SITH_SURFACE_HAS_COLLISION)||surface->flags&SITH_SURFACE_SCROLLING)continue;
            EdVec difference=edSub(vec(face->normal),normal);
            if(edDot(difference,difference)>ED_EPS*ED_EPS)continue;
            if(fabs(edDot(normal,vec(geometry->aVertices[face->vertexPosIdx[0]]))-plane)>ED_EPS)continue;
            if(sithIntersect_TestSphereFaceHit(&contact,0,face,geometry->aVertices,NULL))return surface;
        }
    }
    return NULL;
}
static int apply(const EdModel *m) {
    // Added: reject invalid or non-reciprocal portals before indexing adjoins
    for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++){const EdFace *p=&m->c[s].f[f];if(p->neighbor<0)continue;
        if(p->neighbor>=m->count||p->mirror<0||p->mirror>=m->c[p->neighbor].count||m->c[p->neighbor].f[p->mirror].neighbor!=s||m->c[p->neighbor].f[p->mirror].mirror!=f)return 0;}
    if(!syncCustomTemplates(m)){if(m!=model)syncCustomTemplates(model);return 0;}
    SithWorld *w=sithWorld_g_pCurrentWorld,geo={0}; EdBuffer<int> starts;EdBuffer<EdBuffer<int>> fi,ai;
    sithSectorAlloc *awareness=NULL;
    int nf=0,nv=0,na=0;rdColormap *replacementMaps=NULL;EdBuffer<int> sectorMapIndices;
    std::vector<std::string> mapNames={m->header.colormap};
    for(int s=0;s<m->count;s++){const char *name=m->c[s].colormap[0]?m->c[s].colormap:m->header.colormap;int i=0;for(;i<(int)mapNames.size();i++)if(mapNames[i]==name)break;if(i==(int)mapNames.size())mapNames.push_back(name);sectorMapIndices[s]=i;}
    int replaceMap=liveSectorColormaps!=mapNames||__strcmpi(liveColormap,m->header.colormap)!=0;
    if(replaceMap){replacementMaps=(rdColormap*)SITH_ALLOC(sizeof(rdColormap)*mapNames.size());if(!replacementMaps)return 0;memset(replacementMaps,0,sizeof(rdColormap)*mapNames.size());
        for(size_t i=0;i<mapNames.size();i++){char path[160];snprintf(path,sizeof(path),"misc\\cmp\\%s",mapNames[i].c_str());if(!rdColormap_LoadEntry(path,&replacementMaps[i]))goto fail;}
    }
    for(int s=0;s<m->count;s++) { starts[s]=nf; for(int f=0;f<m->c[s].count;f++) { fi[s][f]=nf++; nv+=m->c[s].f[f].count; ai[s][f]=m->c[s].f[f].neighbor>=0?na++:-1; } }
    geo.numSectors=m->count;geo.numSurfaces=nf;geo.numVertices=geo.numTexVertices=nv;geo.numAdjoins=geo.numAdjoinsLoaded=na;
#define ALLOC(field,type,count) geo.field=(type*)SITH_ALLOC(sizeof(type)*(count)); if(!geo.field) goto fail; edZero(geo.field,0,sizeof(type)*(count))
    ALLOC(aSectors,SithSector,m->count); ALLOC(surfaces,SithSurface,nf); ALLOC(aVertices,rdVector3,nv); ALLOC(aTexVerticies,rdVector2,nv);
    ALLOC(aTransformedVertices,rdVector3,nv); ALLOC(aVertDynamicLights,flex_t,nv); ALLOC(alloc_unk98,int32_t,nv); ALLOC(alloc_unk9c,int32_t,nv);
    if(na) { ALLOC(adjoins,SithSurfaceAdjoin,na); }
    if(sithAIAwareness_bInitted) {
        awareness=(sithSectorAlloc*)SITH_ALLOC(sizeof(sithSectorAlloc)*m->count);
        if(!awareness)goto fail;
        edZero(awareness,0,sizeof(sithSectorAlloc)*m->count);
    }
    {int k=0,positionCount=0;
    for(int s=0;s<m->count;s++) {
        SithSector *sector=&geo.aSectors[s]; EdVec lo=edV(1e9,1e9,1e9),hi=edV(-1e9,-1e9,-1e9);
        sector->flags=m->c[s].flags;sector->tint=rdvec(m->c[s].tint);sector->thrust=rdvec(m->c[s].thrust);sector->ambientSoundVolume=m->c[s].soundVolume;sector->hAmbientSound=m->c[s].sound[0]?sithSound_Load((char*)m->c[s].sound,0):NULL;
        sector->id=s;sector->ambientLight=m->c[s].ambientLight;sector->extraLight=m->c[s].extraLight;sector->colormap=&(replaceMap?replacementMaps:w->colormaps)[sectorMapIndices[s]];sector->numSurfaces=m->c[s].count;sector->surfaces=&geo.surfaces[starts[s]];
        int count=0;for(int f=0;f<m->c[s].count;f++)count+=m->c[s].f[f].count;
        sector->aVertIdxs=(int32_t*)SITH_ALLOC(sizeof(int32_t)*count); if(!sector->aVertIdxs)goto fail;sector->numVertices=count;int sv=0;
        for(int f=0;f<m->c[s].count;f++) {
            const EdFace *src=&m->c[s].f[f];SithSurface *surface=&geo.surfaces[fi[s][f]];rdFace *face=&surface->surfaceInfo.face;
            EdSurfaceAttributes attributes=edSurfaceAttributes(src);
            surface->index=fi[s][f];surface->pSector=sector;surface->flags=attributes.surfaceFlags;
            face->num=surface->index;face->numVertices=src->count;face->normal=rdvec(edNormal(src));face->wallCel=0;
            face->geometryMode=attributes.geometryMode;face->lightingMode=m->lightCount?RD_LIGHTMODE_GOURAUD:attributes.lightingMode;face->textureMode=attributes.textureMode;face->type=attributes.faceFlags;face->extraLight=attributes.extraLight;
            face->material=editorMaterial(src->material);
            face->vertexPosIdx=(int*)SITH_ALLOC(sizeof(int)*src->count);face->vertexUVIdx=(int*)SITH_ALLOC(sizeof(int)*src->count);
            surface->surfaceInfo.intensities=(flex_t*)SITH_ALLOC(sizeof(flex_t)*(src->count*4+1));
            if(!face->vertexPosIdx||!face->vertexUVIdx||!surface->surfaceInfo.intensities)goto fail;
            edZero(surface->surfaceInfo.intensities,0,sizeof(flex_t)*(src->count*4+1));
            for(int v=0;v<src->count;v++,k++) { EdVec p=src->v[v].p;int position=0;
                for(;position<positionCount;position++){EdVec difference=edSub(vec(geo.aVertices[position]),p);if(edDot(difference,difference)<ED_EPS*ED_EPS)break;}
                if(position==positionCount)geo.aVertices[positionCount++]=rdvec(p);
                geo.aTexVerticies[k].x=src->v[v].u;geo.aTexVerticies[k].y=src->v[v].v;face->vertexPosIdx[v]=position;face->vertexUVIdx[v]=k;sector->aVertIdxs[sv++]=position;
                flex_t intensity=(flex_t)edVertexLight(m,s,f,v);for(int channel=0;channel<4;channel++)surface->surfaceInfo.intensities[channel*src->count+v]=intensity;
                if(p.x<lo.x)lo.x=p.x;if(p.y<lo.y)lo.y=p.y;if(p.z<lo.z)lo.z=p.z;if(p.x>hi.x)hi.x=p.x;if(p.y>hi.y)hi.y=p.y;if(p.z>hi.z)hi.z=p.z;
            }
            if(src->neighbor>=0) { SithSurfaceAdjoin *a=&geo.adjoins[ai[s][f]];surface->pAdjoin=a;a->surface=surface;a->sector=&geo.aSectors[src->neighbor];a->mirror=&geo.adjoins[ai[src->neighbor][src->mirror]];a->flags=attributes.adjoinFlags;a->dist=attributes.adjoinDistance;a->next=sector->adjoins;sector->adjoins=a; }
#ifdef SITHRENDER_SPHERE_TEST_SURFACES
            EdVec center=edV(0,0,0);for(int v=0;v<src->count;v++)center=edAdd(center,src->v[v].p);center=edScale(center,1.0/src->count);surface->center=rdvec(center);surface->radius=0;
            for(int v=0;v<src->count;v++) { EdVec d=edSub(src->v[v].p,center);double r=sqrt(edDot(d,d));if(r>surface->radius)surface->radius=r; }
#endif
        }
        sector->boundingbox_onecorner=sector->collidebox_onecorner=rdvec(lo);sector->boundingbox_othercorner=sector->collidebox_othercorner=rdvec(hi);sector->center=rdvec(edScale(edAdd(lo,hi),.5));EdVec d=edSub(hi,lo);sector->radius=sqrt(edDot(d,d))*.5;
    }
    /* Animations tick after controls and can otherwise write into freed geometry.
       Stop them while scrolling surfaces and their sector pointers are valid. */
    uintptr_t surfaceStart=(uintptr_t)w->surfaces,surfaceEnd=surfaceStart+sizeof(SithSurface)*w->numSurfaces;
    uintptr_t sectorStart=(uintptr_t)w->aSectors,sectorEnd=sectorStart+sizeof(SithSector)*w->numSectors;
    for(int i=sithSurface_numSurfaces;i>=0;i--) {
        rdSurface *animation=&sithSurface_aSurfaces[i];
        uintptr_t surface=(uintptr_t)animation->sithSurfaceParent,sector=(uintptr_t)animation->sector;
        if(animation->flags&&((surface>=surfaceStart&&surface<surfaceEnd)||(sector>=sectorStart&&sector<sectorEnd))) {
            sithSurface_StopAnim(animation);animation->sithSurfaceParent=NULL;animation->sector=NULL;
        }
    }
    for(int i=0;i<w->numThingsLoaded;i++) {
        SithThing *t=&w->aThings[i];
        if(t->attach_flags&SITH_ATTACH_SURFACE) {
            SithSurface *support=rebuiltSupport(t,w,&geo,m);
            if(support) {
                t->attachedSurface=support;t->attachedSufaceInfo=&support->surfaceInfo;
                t->field_38=geo.aVertices[support->surfaceInfo.face.vertexPosIdx[0]];
            }else sithThing_DetachThing(t);
        }
        t->sector=NULL;t->pNextThingInSector=t->pPrevThingInSector=NULL;
    }
    sithSoundMixer_pCurSector=NULL;
    /* Footsteps queue sector pointers for a later AI tick. Its per-sector
       allocation must also grow/shrink with the edited world. */
    sithAIAwareness_numEntries=0;
    edZero(sithAIAwareness_aEntries,0,sizeof(sithAIAwareness_aEntries));
    if(awareness) {
        SITH_FREE(sithAIAwareness_g_aSectors);
        sithAIAwareness_g_aSectors=awareness;awareness=NULL;
    }
    suspendPlacedCogs(1);sithSector_FreeWorldSectors(w);sithSurface_FreeWorldSurfaces(w);
#define SWAP(field) SITH_FREE(w->field);w->field=geo.field;geo.field=NULL
    SWAP(aVertices);SWAP(aTexVerticies);SWAP(aTransformedVertices);SWAP(aVertDynamicLights);SWAP(alloc_unk98);SWAP(alloc_unk9c);
    w->aSectors=geo.aSectors;w->surfaces=geo.surfaces;w->adjoins=geo.adjoins;w->numSectors=m->count;w->numSurfaces=nf;w->numVertices=positionCount;w->numTexVertices=nv;w->numAdjoins=w->numAdjoinsLoaded=na;
    if(replaceMap){for(int i=0;i<w->numColormaps;i++)rdColormap_FreeEntry(&w->colormaps[i]);SITH_FREE(w->colormaps);w->colormaps=replacementMaps;replacementMaps=NULL;w->numColormaps=(int)mapNames.size();liveSectorColormaps=mapNames;}
    for(int i=0;i<w->numThingsLoaded;i++) { SithThing *t=&w->aThings[i];if(!t->type)continue;int found=0;for(int s=0;s<m->count;s++)if(edContains(&m->c[s],vec(t->position))) { found=s;break; }sithThing_EnterSector(t,&w->aSectors[found],1,1); }
    for(int i=0;i<7;i++)sithCamera_g_aCameras[i].sector=w->pLocalPlayer?w->pLocalPlayer->sector:&w->aSectors[0];
    for(unsigned int i=0;i<jkPlayer_maxPlayers&&i<JKPLAYER_NUM_INFOS;i++) {
        if(m->spawnCount&&(i<(unsigned int)m->spawnCount||sithNet_isMulti)){int spawn=i%m->spawnCount;rdVector3 angles=rdvec(m->spawns[spawn].rotation);rdMatrix_BuildRotate34(&jkPlayer_playerInfos[i].orient,&angles);jkPlayer_playerInfos[i].orient.scale=rdvec(m->spawns[spawn].position);}
        jkPlayer_playerInfos[i].pInSector=NULL;
        for(int s=0;s<m->count;s++)if(edContains(&m->c[s],vec(jkPlayer_playerInfos[i].orient.scale))){jkPlayer_playerInfos[i].pInSector=&w->aSectors[s];break;}
    }
    const EdLevelHeader *h=&m->header;
    w->gravity=h->gravity;w->ceilingSkyHeight=h->ceilingSkyZ;w->horizonDistance=h->horizonDistance;w->horizontalPixelsPerRev=h->horizonPixels;
    w->horizonSkyOffset=rdVector2{(flex_t)(h->horizonOffset[0]),(flex_t)(h->horizonOffset[1])};w->ceilingSkyOffset=rdVector2{(flex_t)(h->ceilingOffset[0]),(flex_t)(h->ceilingOffset[1])};
    w->mipmapDistance=rdVector4{(flex_t)(h->mip[0]),(flex_t)(h->mip[1]),(flex_t)(h->mip[2]),(flex_t)(h->mip[3])};w->distancesLOD=rdVector4{(flex_t)(h->lod[0]),(flex_t)(h->lod[1]),(flex_t)(h->lod[2]),(flex_t)(h->lod[3])};
    w->perspectiveDistance=h->perspective;w->gouradDistance=h->gouraud;
    if(replaceMap){snprintf(liveColormap,sizeof(liveColormap),"%s",m->header.colormap);
        rdColormap_SetCurrent(&w->colormaps[0]);rdColormap_pIdentityMap=&w->colormaps[0];
        stdPalEffects_RefreshPalette();if(jkGame_isDDraw){stdDisplay_SetMasterPalette((uint8_t*)w->colormaps[0].colors);stdPalEffects_UpdatePalette(w->colormaps[0].colors);}
        sithRender_SetPalette(w->colormaps[0].colors);std3D_PurgeEntireTextureCache();
    }
    rdResetWorldDistances();rdSetMipDistances(&w->mipmapDistance);
    sithWorld_ResetRenderState(w);syncEditorObjects(m);recoverOutsideEditorPlayer(m);if(!active&&m->cogCount&&!startPlacedCogs(m))say("Could not start one or more placed COGs. Check script resources and capacity.");return 1;
}
fail:
    if(replacementMaps){for(size_t i=0;i<mapNames.size();i++)rdColormap_FreeEntry(&replacementMaps[i]);SITH_FREE(replacementMaps);}
    SITH_FREE(awareness);
    if(geo.aSectors)sithSector_FreeWorldSectors(&geo);if(geo.surfaces)sithSurface_FreeWorldSurfaces(&geo);
    SITH_FREE(geo.aVertices);SITH_FREE(geo.aTexVerticies);SITH_FREE(geo.aTransformedVertices);SITH_FREE(geo.aVertDynamicLights);SITH_FREE(geo.alloc_unk98);SITH_FREE(geo.alloc_unk9c);return 0;
#undef ALLOC
#undef SWAP
}
static void readLevelHeader(EdLevelHeader *h,SithWorld *w) {
    edZero(h,0,sizeof(*h));snprintf(h->title,sizeof(h->title),"New Editor Level");snprintf(h->colormap,sizeof(h->colormap),"dflt.cmp");
    h->gravity=w->gravity;h->ceilingSkyZ=w->ceilingSkyHeight;h->horizonDistance=w->horizonDistance;h->horizonPixels=w->horizontalPixelsPerRev;
    h->horizonOffset[0]=w->horizonSkyOffset.x;h->horizonOffset[1]=w->horizonSkyOffset.y;h->ceilingOffset[0]=w->ceilingSkyOffset.x;h->ceilingOffset[1]=w->ceilingSkyOffset.y;
    h->mip[0]=w->mipmapDistance.x;h->mip[1]=w->mipmapDistance.y;h->mip[2]=w->mipmapDistance.z;h->mip[3]=w->mipmapDistance.w;
    h->lod[0]=w->distancesLOD.x;h->lod[1]=w->distancesLOD.y;h->lod[2]=w->distancesLOD.z;h->lod[3]=w->distancesLOD.w;
    h->perspective=w->perspectiveDistance;h->gouraud=w->gouradDistance;
    char levelKey[32];snprintf(levelKey,sizeof(levelKey),"%s",w->map_jkl_fname);char *dot=strrchr(levelKey,'.');if(dot)*dot=0;for(char *p=levelKey;*p;p++)*p=(char)toupper((unsigned char)*p);
    char16_t *title=stdStrTable_GetValue(&jkCog_strings,levelKey);if(!title)title=stdStrTable_GetValue(&jkCog_strings,"SITHED");if(title)stdString_WcharToChar(h->title,title,sizeof(h->title));
    for(int i=0;i<20;i++){char key[64];snprintf(key,sizeof(key),"%s_TEXT_%02d",levelKey,i);char16_t *text=stdStrTable_GetValue(&jkCog_strings,key);if(!text){snprintf(key,sizeof(key),"SITHED_TEXT_%02d",i);text=stdStrTable_GetValue(&jkCog_strings,key);}if(text)stdString_WcharToChar(h->intro[i],text,sizeof(h->intro[i]));}
    stdFile_t file=openEditorJkl("rt");char line[512];int next=0;
    if(file){while(jkRes_FileGets(file,line,sizeof(line))){
        if(next&&sscanf(line," 0: %127s",h->colormap)==1)break;
        /* Read source values: the renderer scales live LOD distances by resolution. */
        for(char *p=line;*p;p++)*p=(char)tolower((unsigned char)*p);
        if(strstr(line,"world colormaps"))next=1;
        sscanf(line," world gravity %lf",&h->gravity);sscanf(line," ceiling sky z %lf",&h->ceilingSkyZ);
        sscanf(line," horizon distance %lf",&h->horizonDistance);sscanf(line," horizon pixels per rev %lf",&h->horizonPixels);
        sscanf(line," horizon sky offset %lf %lf",&h->horizonOffset[0],&h->horizonOffset[1]);sscanf(line," ceiling sky offset %lf %lf",&h->ceilingOffset[0],&h->ceilingOffset[1]);
        sscanf(line," mipmap distances %lf %lf %lf %lf",&h->mip[0],&h->mip[1],&h->mip[2],&h->mip[3]);sscanf(line," lod distances %lf %lf %lf %lf",&h->lod[0],&h->lod[1],&h->lod[2],&h->lod[3]);
        sscanf(line," perspective distance %lf",&h->perspective);sscanf(line," gouraud distance %lf",&h->gouraud);
    }jkRes_FileClose(file);}
}
static const char *templateBlock(const char *text){
    for(const char *p=text;p&&*p;p=strchr(p,'\n')){if(*p=='\n')p++;if(!__strnicmp(p,"World templates",15))return strchr(p,'\n');}return NULL;
}
static int templateInBlock(const char *block,const char *name){
    for(const char *p=block;p&&*p;p=strchr(p,'\n')){if(*p=='\n')p++;char token[64];if(sscanf(p," %63s",token)==1){if(!__strcmpi(token,"end"))break;if(!__strcmpi(token,name))return 1;}}return 0;
}
static int loadMissingManifestTemplates(const char *manifest){
    const char *block=templateBlock(manifest);if(!block)return 0;StdConffileEntry previous=stdConffile_g_entry;int ok=1;
    for(const char *p=block;p&&*p;p=strchr(p,'\n')){if(*p=='\n')p++;char name[64],parent[64];if(sscanf(p," %63s",name)!=1||name[0]=='#')continue;if(!__strcmpi(name,"end"))break;if(sscanf(p," %63s %63s",name,parent)!=2)continue;
        if(sithTemplate_GetTemplate(name))continue;const char *end=strchr(p,'\n');size_t n=end?(size_t)(end-p):strlen(p);char line[16384];if(n>=sizeof(line)){ok=0;break;}edCopy(line,p,n);line[n]=0;
        if(sithWorld_g_pCurrentWorld->numThingTemplates>=sithWorld_g_pCurrentWorld->sizeThingTemplates){ok=0;break;}stdConffile_ReadArgsFromStr(line);sithTemplate_Parse(sithWorld_g_pCurrentWorld);
    }stdConffile_g_entry=previous;return ok;
}
/* Keep level-local template definitions, then add catalog tools not used by
   that stock map. Saving must retain both sets, with the stock version winning. */
static int addCatalogTemplates(void){
    FILE *f=fopen("editor-resources.jkl","rb");if(!f)return 0;fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);char *catalog=size>0&&size<=4000000?(char*)edAlloc(size+1):NULL;
    int ok=catalog&&fread(catalog,1,size,f)==(size_t)size;fclose(f);if(!ok){edFree(catalog);return 0;}catalog[size]=0;
    const char *original=templateBlock(resources),*source=templateBlock(catalog),*tail=original?strstr(original,"\nend"):NULL;
    if(!source||!tail){edFree(catalog);return 0;}tail++;size_t prefix=tail-resources,used=prefix,capacity=strlen(resources)+size+2;char *merged=edAlloc(capacity);if(!merged){edFree(catalog);return 0;}edCopy(merged,resources,prefix);
    for(const char *p=source;p&&*p;p=strchr(p,'\n')){if(*p=='\n')p++;char name[64],parent[64];if(sscanf(p," %63s",name)!=1||name[0]=='#')continue;if(!__strcmpi(name,"end"))break;if(sscanf(p," %63s %63s",name,parent)!=2)continue;
        if(!templateInBlock(original,name)){const char *end=strchr(p,'\n');size_t n=end?(size_t)(end-p)+1:strlen(p);edCopy(merged+used,p,n);used+=n;}
    }snprintf(merged+used,capacity-used,"%s",tail);edFree(resources);resources=merged;edFree(catalog);return 1;
}
static int readStockResources(void){
    stdFile_t f=openEditorJkl("rb");if(!f)return 0;int size=jkRes_FileSize(f);
    char *text=size>0&&size<16000000?(char*)edAlloc(size+1):NULL;int ok=text&&jkRes_FileRead(f,text,size)==size;jkRes_FileClose(f);if(!ok){edFree(text);return 0;}text[size]=0;
    char *out=edAlloc(size+1);if(!out){edFree(text);return 0;}size_t used=0;int keep=0;
    for(char *p=text;*p;){char *end=strchr(p,'\n');size_t n=end?(size_t)(end-p)+1:strlen(p);char line[128],section[64];snprintf(line,sizeof(line),"%.*s",(int)(n<127?n:127),p);for(char *c=line;*c;c++)*c=(char)tolower((unsigned char)*c);
        if(sscanf(line," section: %63s",section)==1)keep=strcmp(section,"georesource")&&strcmp(section,"sectors")&&strcmp(section,"things")&&strcmp(section,"lights");
        if(keep){edCopy(out+used,p,n);used+=n;}p+=n;
    }out[used]=0;edFree(text);edFree(resources);resources=out;return used>0&&addCatalogTemplates();
}
static int importFailure(const char *reason){failedImportWorld=sithWorld_g_pCurrentWorld;boundWorld=NULL;stdPlatform_Printf("GeoSith import failed: %s\n",reason);say(reason);return 0;}
static int bindEditorWorld(void) {
    /* Join synchronization precedes the welcome that assigns a local player.
       Bind the loaded level independently so the host can finish the join. */
    if(!sithWorld_g_pCurrentWorld||!sithWorld_g_pCurrentWorld->aSectors||sithWorld_g_pCurrentWorld->numSectors<1||!sithWorld_g_pCurrentWorld->aThings)return 0;
    if(boundWorld==sithWorld_g_pCurrentWorld&&model) return 1;
    if(failedImportWorld==sithWorld_g_pCurrentWorld)return 0;
    if(!customResourceError.empty())return importFailure(customResourceError.c_str());
    importingStock=stockEpisode(jkRes_episodeGobName);
    if(!readStockResources())return importFailure("Could not read stock level resources.");
    if(!readResources()) return 0;
    if(!model)model=(EdModel*)edCalloc(1,sizeof(EdModel));if(!snapshot)snapshot=(EdModel*)edAlloc(sizeof(EdModel));if(!candidate)candidate=(EdModel*)edAlloc(sizeof(EdModel));if(!undoModel)undoModel=(EdModel*)edAlloc(sizeof(EdModel));
    if(!model||!snapshot||!candidate||!undoModel)return 0;
    SithWorld *w=sithWorld_g_pCurrentWorld;if(w->numSectors>ED_CELLS)return importFailure("Level exceeds the editor sector capacity.");
    /* Resolve the manifest's material indices through the engine's material cache. */
    const char *line=strstr(resources,"World materials");if(!line)line=strstr(resources,"world materials");freeEditorMaterials();
    if(line) { line=strchr(line,'\n');while(line && *++line) { int id;char name[128];if(sscanf(line," %d: %127s",&id,name)==2&&id==paletteCount&&id<EDITOR_MATERIALS){snprintf(paletteNames[id],128,"%s",name);palette[paletteCount++]=sithMaterial_Load(name,0,0);}else if(!strncmp(line,"end",3))break;line=strchr(line,'\n'); } }
    baseMaterialCount=paletteCount;discoverMaterials();builderFreeCache();resetCustomTemplates();
    liveSectorColormaps.clear();edZero(model,0,sizeof(*model));readLevelHeader(&model->header,w);snprintf(liveColormap,sizeof(liveColormap),"%s",model->header.colormap);model->count=w->numSectors;model->spawn=w->pLocalPlayer?vec(w->pLocalPlayer->position):edV(0,0,0);
    templateCount=0;currentTemplate=0;
    for(int i=0;i<w->numThingTemplates&&templateCount<EDITOR_TEMPLATES;i++)if(w->aThingTemplates[i].aName[0])snprintf(templateNames[templateCount++],128,"%s",w->aThingTemplates[i].aName);
    SithWorld *staticWorld=sithWorld_g_pStaticWorld;
    if(staticWorld)for(int i=0;i<staticWorld->numThingTemplates&&templateCount<EDITOR_TEMPLATES;i++)if(staticWorld->aThingTemplates[i].aName[0]){const char *name=staticWorld->aThingTemplates[i].aName;int found=0;for(int j=0;j<templateCount;j++)if(!__strcmpi(name,templateNames[j]))found=1;if(!found)snprintf(templateNames[templateCount++],128,"%s",name);}
    qsort(templateNames,templateCount,sizeof(templateNames[0]),materialNameCompare);
    clearObjectPreviews();templateMetadata();
    for(int i=0;i<templateCount;i++)if(!__strcmpi(templateNames[i],"walkplayer"))currentTemplate=i;
    objectSlotCount=0;for(int i=0;i<ED_OBJECTS;i++){objectThings[i]=-1;objectSignatures[i]=0;objectThingNames[i][0]=0;objectProxyMode[i]=0;objectTemplateHashes[i]=0;}
    for(int i=0;i<w->numThingsLoaded;i++){SithThing *thing=&w->aThings[i];if(!thing->type||thing==w->pLocalPlayer||!thing->pTemplate||thing->type==SITH_THING_PLAYER)continue;
        if(model->objectCount>=ED_OBJECTS)return importFailure("Level exceeds the editor object capacity.");
        int id=model->objectCount++;EdObject *object=&model->objects[id];snprintf(object->templateName,sizeof(object->templateName),"%s",thing->pTemplate->aName);object->position=vec(thing->position);rdVector3 angles;rdMatrix_ExtractAngles34(&thing->orient,&angles);object->rotation=vec(angles);
        if((thing->moveType==SITH_MT_PATH&&thing->trackParams.loadedFrames>ED_FRAMES)||(thing->controlType==SITH_CT_AI&&thing->actor&&thing->actor->loadedFrames>ED_FRAMES))return importFailure("A thing exceeds the editor frame capacity.");
        readThingAttributes(&object->attributes,thing);objectThings[id]=i;objectSignatures[id]=thing->signature;snprintf(objectThingNames[id],64,"%s",object->templateName);
    }
    model->spawnCount=netAuthoredSpawns?netAuthoredSpawns:jkPlayer_maxPlayers<ED_SPAWNS?jkPlayer_maxPlayers:ED_SPAWNS;
    for(int i=0;i<model->spawnCount;i++) {
        rdVector3 angles;rdMatrix_ExtractAngles34(&jkPlayer_playerInfos[i].orient,&angles);
        model->spawns[i].position=vec(jkPlayer_playerInfos[i].orient.scale);model->spawns[i].rotation=vec(angles);
        if(jkPlayer_playerInfos[i].pLocalPlayer)readThingAttributes(&model->spawns[i].attributes,jkPlayer_playerInfos[i].pLocalPlayer);
    }
    if(!model->spawnCount){model->spawnCount=1;model->spawns[0].position=model->spawn;}
    model->spawn=model->spawns[0].position;nextSpawn=0;int changedUV=0;
    for(int s=0;s<w->numSectors;s++) { SithSector *sector=&w->aSectors[s];EdCell *cell=&model->c[s];if(sector->numSurfaces>ED_FACES)return importFailure("A sector exceeds the editor surface capacity.");cell->tolerance=importingStock?ED_IMPORT_EPS:0;cell->flags=sector->flags&~SITH_SECTOR_HASCOLLIDEBOX;cell->tint=vec(sector->tint);cell->thrust=vec(sector->thrust);cell->soundVolume=sector->ambientSoundVolume;if(sector->hAmbientSound)snprintf(cell->sound,32,"%s",sector->hAmbientSound->sound_fname);
#ifdef SITH_DEBUG_STRUCT_NAMES
        if(sector->colormap&&sector->colormap!=&w->colormaps[0])snprintf(cell->colormap,128,"%s",sector->colormap->colormap_fname);
#endif
        cell->count=sector->numSurfaces;cell->ambientLight=fmin(1,fmax(0,sector->ambientLight));cell->extraLight=fmin(1,fmax(0,sector->extraLight));
        for(int f=0;f<cell->count;f++) { SithSurface *surface=&sector->surfaces[f];rdFace *src=&surface->surfaceInfo.face;EdFace *dst=&cell->f[f];if(src->numVertices>ED_SURFACE_VERTS)return importFailure("A surface exceeds the 24 vertex limit.");dst->count=src->numVertices;dst->neighbor=dst->mirror=-1;dst->material=0;
            for(int i=0;i<paletteCount;i++)if(palette[i]==src->material||(src->material&&!__strcmpi(paletteNames[i],baseName(src->material->mat_fpath)))){dst->material=i;palette[i]=src->material;break;}
            if(surface->pAdjoin) { dst->neighbor=surface->pAdjoin->sector->id;dst->mirror=(int)(surface->pAdjoin->mirror->surface-surface->pAdjoin->sector->surfaces); }
            dst->hasAttributes=1;dst->attributes=EdSurfaceAttributes{surface->flags,src->type,surface->pAdjoin?surface->pAdjoin->flags:7,src->geometryMode,src->textureMode,src->lightingMode,dst->neighbor>=0,1,1,src->extraLight,surface->pAdjoin?surface->pAdjoin->dist:0};
            for(int v=0;v<dst->count;v++) { dst->v[v].light=surface->surfaceInfo.intensities?surface->surfaceInfo.intensities[v]:0;dst->v[v].p=vec(w->aVertices[src->vertexPosIdx[v]]);if(src->vertexUVIdx) { rdVector2 uv=w->aTexVerticies[src->vertexUVIdx[v]];dst->v[v].u=uv.x+src->texVertOffset.x;dst->v[v].v=uv.y+src->texVertOffset.y; } }
            /* JKL stores scale in UV coordinates. Recover independent U/V
             * scales so reopening the level retains attribute edits. */
            EdFace projected=*dst;projected.uOffset=projected.vOffset=0;edMapFace(&projected);
            double us=1,vs=1;
            for(int v=1;v<dst->count;v++){
                double pu=projected.v[v].u-projected.v[0].u,pv=projected.v[v].v-projected.v[0].v;
                double tu=dst->v[v].u-dst->v[0].u,tv=dst->v[v].v-dst->v[0].v;
                if(fabs(pu)>.001&&fabs(tu)>.001)us=pu/tu;if(fabs(pv)>.001&&fabs(tv)>.001)vs=pv/tv;
            }
            if(!isfinite(us)||fabs(us)<.000001)us=1;if(!isfinite(vs)||fabs(vs)<.000001)vs=1;
            dst->attributes.uScale=us;dst->attributes.vScale=vs;edMapFace(&projected);
            dst->uOffset=dst->v[0].u-projected.v[0].u/us;dst->vOffset=dst->v[0].v-projected.v[0].v/vs;
            EdFace check=*dst;edMapFace(&check);for(int v=0;v<dst->count;v++)if(fabs(check.v[v].u-dst->v[v].u)>.001||fabs(check.v[v].v-dst->v[v].v)>.001)changedUV=1;

        }
    }
    stdFile_t lightFile=openEditorJkl("rb");int lightsValid=1;
    stdFile_t meshFile=jkRes_FileOpen("misc\\geosith_models.bin","rb");
    if(meshFile){int size=jkRes_FileSize(meshFile);unsigned char *data=size>0&&size<=ED_NET_LIMIT/2?(unsigned char*)edAlloc(size):NULL;
        lightsValid=data&&jkRes_FileRead(meshFile,data,size)==size&&edReadMeshData(model,data,size);edFree(data);jkRes_FileClose(meshFile);}
    if(lightFile){int size=jkRes_FileSize(lightFile);char *text=size>0&&size<64*1024*1024?(char*)edAlloc((size_t)size+1):NULL;
        lightsValid=lightsValid&&text&&jkRes_FileRead(lightFile,text,size)==size;if(lightsValid){text[size]=0;lightsValid=edReadLights(model,text)&&readPlacedCogs(model,text)&&edReadTextureRotations(model,text);
            if(model->lightCount)for(int s=0;s<model->count;s++)for(int f=0;f<model->c[s].count;f++)for(int v=0;v<model->c[s].f[f].count;v++)model->c[s].f[f].v[v].light=0;
            for(const char *line=text;line&&*line;line=strchr(line,'\n')){if(*line=='\n')line++;int sector,face,vertex;double value;char layer[64];if(sscanf(line,"# GeoSith layer %d %63[^\r\n]",&sector,layer)==2&&sector>=0&&sector<model->count)snprintf(model->c[sector].layer,64,"%s",layer);if(sscanf(line,"# GeoSith tolerance %d %lf",&sector,&value)==2&&sector>=0&&sector<model->count&&value>=0&&value<=ED_IMPORT_EPS)model->c[sector].tolerance=value;
                if(sscanf(line,"# GeoSith base light %d %d %d %lf",&sector,&face,&vertex,&value)==4&&sector>=0&&sector<model->count&&face>=0&&face<model->c[sector].count&&vertex>=0&&vertex<model->c[sector].f[face].count&&isfinite(value)&&value>=0&&value<=1)model->c[sector].f[face].v[vertex].light=value;
            }
            edSplineReadMetadata(model,text);
            if(model->meshCount||model->splineCount){int mapping[EDITOR_MATERIALS];for(int i=0;i<EDITOR_MATERIALS;i++)mapping[i]=-1;const char *p=strstr(text,"World materials");if(!p)p=strstr(text,"world materials");p=p?strchr(p,'\n'):NULL;
                while(p&&*++p&&strncmp(p,"end",3)){int id;char name[128];if(sscanf(p," %d: %127s",&id,name)==2&&id>=0&&id<EDITOR_MATERIALS)mapping[id]=addMaterialName(name);p=strchr(p,'\n');}
                for(int i=0;i<model->splineCount;i++){int *id=&model->splines[i].material;if(*id<0||*id>=EDITOR_MATERIALS||mapping[*id]<0)lightsValid=0;else *id=mapping[*id];}
                for(int i=0;i<model->meshCount;i++)for(int f=0;f<model->meshes[i].geometry.count;f++){int *id=&model->meshes[i].geometry.f[f].material;if(*id<0||*id>=EDITOR_MATERIALS||mapping[*id]<0)lightsValid=0;else *id=mapping[*id];}
            }
        }edFree(text);jkRes_FileClose(lightFile);}
    stdFile_t episodeFile=jkRes_FileOpen("episode.jk","rb");int episodeValid=1;
    if(episodeFile){int size=jkRes_FileSize(episodeFile);char *text=size>0&&size<128*1024?(char*)edAlloc(size+1):NULL;
        episodeValid=text&&jkRes_FileRead(episodeFile,text,size)==size;if(episodeValid){text[size]=0;episodeValid=edReadEpisode(&model->episode,text,w->map_jkl_fname);}edFree(text);jkRes_FileClose(episodeFile);}
    else edEpisodeDefault(&model->episode,model->header.title,sithNet_isMulti);
    for(int i=0;i<model->episode.count;i++){EdEpisodeRow *r=&model->episode.rows[i];char key[64];snprintf(key,sizeof(key),"%s",r->file);char *dot=strrchr(key,'.');if(dot)*dot=0;for(char *p=key;*p;p++)*p=(char)toupper((unsigned char)*p);
        char16_t *name=stdStrTable_GetValue(&jkCog_strings,key);if(name)stdString_WcharToChar(r->title,name,sizeof(r->title));
        for(int j=0;j<20;j++){char entry[64];snprintf(entry,sizeof(entry),"%s_TEXT_%02d",key,j);char16_t *value=stdStrTable_GetValue(&jkCog_strings,entry);if(value)stdString_WcharToChar(r->intro[j],value,sizeof(r->intro[j]));}
    }
    if(importingStock){
        EdEpisodeRow row=model->episode.rows[model->episode.current];edEpisodeDefault(&model->episode,model->header.title,sithNet_isMulti);row.line=0;row.gotoA=row.gotoB=-1;model->episode.rows[0]=row;episodeValid=1;
        snprintf(protectedEpisode,sizeof(protectedEpisode),"%s",jkRes_episodeGobName);
    }
    active=0;clearHistory();multi=sithNet_isMulti;picks.count=grabPicks.count=undoPicks.count=0;currentMaterial=model->c[0].f[0].material;lightHovered=lightGrabbed=-1;lightGrabbing=0;
    if(importingStock){char name[32];snprintf(name,sizeof(name),"%.20s",w->map_jkl_fname);char *dot=strrchr(name,'.');if(dot)*dot=0;strcat(name,"_edited");stdString_CharToWchar(levelName,name,32);}else stdString_CharToWchar(levelName,jkRes_episodeGobName,32);
    terrainTemplateFlags(model);
    int geometryValid=edValidate(model);stdPlatform_Printf("GeoSith import checks: lights/cogs=%d episode=%d geometry=%d\n",lightsValid,episodeValid,geometryValid);int valid=lightsValid&&episodeValid&&geometryValid&&syncCustomTemplates(model);if(valid&&!importingStock&&(changedUV||model->lightCount))valid=apply(model);if(valid)boundWorld=w;else importFailure("Level conversion failed validation; original level is unchanged.");stdPlatform_Printf("GeoSith: bound %d sectors, %d materials, %d lights, geometry valid=%d\n",model->count,paletteCount,model->lightCount,valid);return valid;
}
static void remember(void) { edCopyModel(undoModel,model);undoPicks=picks;pendingUndo=1;netRemember(); }
static int commit(void) {
    if(mapActive&&!netApplying&&!mapMirrorCandidate())return 0;
    terrainTemplateFlags(candidate);
    edSplineReconcile(candidate);
    if(edModelsEqual(candidate,model)){edClearGeometryError();return 1;}
    if(netClient()&&!netApplying)return netPropose(candidate);
    if(!edValidate(candidate)){say("Edit rejected: geometry must remain closed, planar and convex.");return 0;}
    // Added: heap-allocate the large published model instead of using the stack
    std::unique_ptr<EdModel> published(new EdModel());edCopyModel(published.get(),candidate);
    EditorHistoryEntry entry={0};
    if(pendingUndo){entry=historyEntry(undoModel,&undoPicks);if(!entry.data){say("Edit rejected: could not allocate undo history.");return 0;}}
    EdSelection remapped;edSelectionRemap(model,candidate,&picks,edV(0,0,0),&remapped);
    if(!apply(candidate)){edFree(entry.data);say("Edit rejected: could not allocate live geometry.");return 0;}
    if(entry.data){pushHistory(undoHistory,&undoCount,entry);clearHistoryEntries(redoHistory,&redoCount);pendingUndo=0;}
    *model=std::move(*published);picks=remapped;edClearGeometryError();if(!netApplying)netCommitted();return 1;
}
static void ray(EdVec *o,EdVec *d) { SithThing *p=sithWorld_g_pCurrentWorld->pLocalPlayer;rdMatrix34 head,look;rdVector3 eye;rdMatrix_BuildRotate34(&head,&p->actorParams.headPYR);rdMatrix_Multiply34(&look,&p->orient,&head);rdMatrix_TransformVector34(&eye,&p->actorParams.eyeOffset,&look);*o=edAdd(vec(p->position),vec(eye));*d=vec(look.lvec); }
static int editButton(int function) {
    /* Ctrl/Shift are editor modifiers even if the player's JK profile also
       binds them to fire. Honor all remaining keyboard/mouse/controller binds. */
    int down=0;for(unsigned int i=0;i<sithControl_aInputFuncToKeyinfo[function].numEntries;i++){int key=sithControl_aInputFuncToKeyinfo[function].aEntries[i].dxKeyNum;if(key==DIK_LCONTROL||key==DIK_RCONTROL||key==DIK_LSHIFT||key==DIK_RSHIFT)continue;down|=stdControl_ReadKey(key,NULL);}return down!=0;
}
static void leaveEditor(SithThing *p) {
    mapClose(p);
    active=0;p->physicsParams.flags=savedPhysics;g_debugmodeFlags=savedDebug;p->flags=savedThingFlags;grabbing=groupGrab=cleaving=objectGrabbing=objectRotating=lightGrabbing=0;objectHandleHovered=-10000;
    edResetDoubleTap(&jumpTap,0);
    int sector=-1;for(int s=0;s<model->count;s++)if(edContains(&model->c[s],vec(p->position))){sector=s;break;}
    // Added: avoid dividing by zero when the fallback sector has no vertices
    if(sector<0) {EdVec center=edV(0,0,0);int count=0;for(int f=0;f<model->c[0].count;f++)for(int v=0;v<model->c[0].f[f].count;v++){center=edAdd(center,model->c[0].f[f].v[v].p);count++;}if(count)p->position=rdvec(edScale(center,1.0/count));sector=0;}
    sithThing_SetSector(p,&sithWorld_g_pCurrentWorld->aSectors[sector],0);
    syncEditorObjects(model);
    if(!startPlacedCogs(model))say("Some placed COGs could not start. Save and reload to verify script resources.");
}
static int editorForceSpeedHeld(void) {
    int held=0;
    for(int i=0;i<SITHINVENTORY_NUM_POWERKEYBINDS-1;i++)
        if(sithInventory_GetPowerKeybind(i)==SITHBIN_F_SPEED)held|=sithControl_GetKey(INPUT_FUNC_ACTIVATE0+i,NULL)!=0;
    return held;
}
static void setEditorFlight(SithThing *p,int enabled) {
    /* Flying changes gravity and movement, never collision. */
    g_debugmodeFlags&=~DEBUGFLAG_NOCLIP;
    if(enabled) {
        p->physicsParams.flags=(p->physicsParams.flags&~SITH_PF_USEGRAVITY)|SITH_PF_FLY;
        if(p->attach_flags)sithThing_DetachThing(p);
        p->physicsParams.vel.z=0;
        say("Flight on. Double-tap jump to turn flight off.");
    } else {
        p->physicsParams.flags=(p->physicsParams.flags&~SITH_PF_FLY)|SITH_PF_USEGRAVITY;
        flightSpeedBoost=0;
        say("Flight off. Double-tap jump to fly.");
    }
}
static void editorFlightMovement(SithThing *p) {
    /* Rebuild thrust from current input; looking alone must not retain thrust. */
    rdVector_Zero3(&p->physicsParams.acceleration);
    /* Use identical flight controls with collision on or off; stock noclip
       otherwise changes the controls and bypasses jump/crouch ascent. */
    uint32_t debug=g_debugmodeFlags;g_debugmodeFlags&=~DEBUGFLAG_NOCLIP;
    sithControl_FreeCam(p);g_debugmodeFlags=debug;
    if(flightSpeedBoost)rdVector_Scale3Acc(&p->physicsParams.acceleration,2.0);
    if(rdVector_IsZero3(&p->physicsParams.acceleration)) {
        rdVector_Zero3(&p->physicsParams.vel);
        rdVector_Zero3(&p->physicsParams.deltaVelocity);
    }
}
static int toggleFlightCollision(SithThing *p) {
    if(!(p->physicsParams.flags&SITH_PF_FLY))return 0;
    g_debugmodeFlags^=DEBUGFLAG_NOCLIP;
    if(p->attach_flags)sithThing_DetachThing(p);
    say(g_debugmodeFlags&DEBUGFLAG_NOCLIP?"Flight collision off. Q turns it on.":"Flight collision on. Q turns it off.");return 1;
}
static void recoverOutsideEditorPlayer(const EdModel *source){
    SithWorld *world=sithWorld_g_pCurrentWorld;SithThing *p=world?world->pLocalPlayer:NULL;
    if(!active||!p||objectSector(source,vec(p->position))>=0)return;
    // apply() has assigned a valid sector for engine bookkeeping, but the
    // authored geometry no longer contains this position. Keep the viewpoint.
    setEditorFlight(p,1);g_debugmodeFlags|=DEBUGFLAG_NOCLIP;
    rdVector_Zero3(&p->physicsParams.vel);rdVector_Zero3(&p->physicsParams.acceleration);
    rdVector_Zero3(&p->physicsParams.deltaVelocity);
    say("Outside the level: flight on, collision off. Q restores collision.");
}
static int teleportWalkplayer(SithThing *p) {
    int count=model->spawnCount;
    for(int attempt=0;attempt<count;attempt++) {
        int index=nextSpawn%count;nextSpawn=(index+1)%count;EdSpawn *spawn=&model->spawns[index];
        int sector=-1;for(int s=0;s<model->count;s++)if(edContains(&model->c[s],spawn->position)){sector=s;break;}
        if(sector<0)continue;
        grabbing=cleaving=0;if(p->attach_flags)sithThing_DetachThing(p);
        p->position=rdvec(spawn->position);rdVector3 angles=rdvec(spawn->rotation);rdMatrix_BuildRotate34(&p->orient,&angles);
        rdVector_Zero3(&p->actorParams.headPYR);rdVector_Zero3(&p->physicsParams.vel);
        rdVector_Zero3(&p->physicsParams.acceleration);rdVector_Zero3(&p->physicsParams.deltaVelocity);rdVector_Zero3(&p->physicsParams.angularVelocity);
        sithThing_SetSector(p,&sithWorld_g_pCurrentWorld->aSectors[sector],1);
        for(int i=0;i<7;i++)sithCamera_g_aCameras[i].sector=p->sector;
        char text[80];snprintf(text,sizeof(text),"Walkplayer %d / %d",index+1,count);say(text);return index+1;
    }
    say("No walkplayer is inside the current geometry.");return 0;
}
static int historyStep(int redo) {
    if(netClient()&&!netApplying)return netHistory(redo);
    EditorHistoryEntry *source=redo?redoHistory:undoHistory,*destination=redo?undoHistory:redoHistory;
    int *sourceCount=redo?&redoCount:&undoCount,*destinationCount=redo?&undoCount:&redoCount;
    pendingUndo=0;
    EditorHistoryEntry current=historyEntry(model,&picks);if(!current.data){say("Could not allocate edit history.");return 0;}
    /* Store only populated, losslessly compressed records: large stock maps
       must not allocate an entire maximum-sized sector array per undo. */
    while(*sourceCount&&source[*sourceCount-1].size==current.size&&!edCompare(source[*sourceCount-1].data,current.data,current.size)){edFree(source[--*sourceCount].data);source[*sourceCount]=EditorHistoryEntry{0};}
    if(!*sourceCount){edFree(current.data);say(redo?"Nothing to redo.":"Nothing to undo.");return 0;}
    EditorHistoryEntry restored=source[*sourceCount-1];unsigned char *wire=edNetUnpack(restored.data,restored.size,restored.rawSize);int decoded=wire&&edWireDecode(candidate,wire,restored.rawSize);edFree(wire);
    if(!decoded){edFree(current.data);say("Could not restore edit history.");return 0;}
    if(!commit()){edFree(current.data);return 0;}
    sectorPicks.clear();picks=restored.selection;edFree(restored.data);source[--*sourceCount]=EditorHistoryEntry{0};pushHistory(destination,destinationCount,current);
    grabbing=groupGrab=cleaving=lightGrabbing=0;lightHovered=lightGrabbed=-1;hover.cell=-1;
    SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;int inside=0;
    for(int s=0;s<model->count;s++)if(edContains(&model->c[s],vec(player->position))){inside=1;break;}
    if(!inside&&!active){nextSpawn=0;teleportWalkplayer(player);}
    char text[96];snprintf(text,sizeof(text),"%s. %d undo / %d redo steps.",redo?"Redone":"Undone",undoCount,redoCount);say(text);return 1;
}
static int historySelfTest(void) {
    EdModel *saved=(EdModel*)edAlloc(sizeof(*saved));if(!saved)return 0;edCopyModel(saved,model);EdSelection savedPicks=picks;
    clearHistory();double base=model->header.gravity;int ok=1;
    for(int i=1;i<=40&&ok;i++){remember();edCopyModel(candidate,model);candidate->header.gravity=base+i;ok=commit();}
    ok=ok&&undoCount==30&&redoCount==0;
    for(int i=0;i<30&&ok;i++)ok=historyStep(0);
    ok=ok&&fabs(model->header.gravity-base-10)<.001&&undoCount==0&&redoCount==30&&!historyStep(0);
    for(int i=0;i<30&&ok;i++)ok=historyStep(1);
    ok=ok&&fabs(model->header.gravity-base-40)<.001&&undoCount==30&&redoCount==0;
    ok=ok&&historyStep(0)&&historyStep(0)&&redoCount==2;
    remember();edCopyModel(candidate,model);candidate->header.gravity=base+100;ok=ok&&commit()&&redoCount==0&&!historyStep(1);
    clearHistory();edCopyModel(candidate,saved);ok=ok&&commit();
    /* Many frame updates within a single grab must occupy only one slot. */
    picks.count=1;picks.hits[0]=EdHit{0,0,-1,-1};remember();
    edCopyModel(candidate,model);candidate->header.gravity=base+1;ok=ok&&commit();
    edCopyModel(candidate,model);candidate->header.gravity=base+2;ok=ok&&commit()&&undoCount==1;
    picks.count=0;ok=ok&&historyStep(0)&&fabs(model->header.gravity-base)<.001&&picks.count==1;
    ok=ok&&historyStep(1)&&fabs(model->header.gravity-base-2)<.001;
    int count=undoCount;remember();edCopyModel(candidate,model);ok=ok&&commit()&&undoCount==count;
    candidate->count=0;ok=ok&&!commit()&&undoCount==count;
    clearHistory();edCopyModel(candidate,saved);int restored=commit();picks=savedPicks;edFree(saved);
    printf("GEOSITH_HISTORY %s: 30 undos/redos, rollover, branches, grouped updates, selection, rejected edits\n",ok&&restored?"PASS":"FAIL");fflush(stdout);return ok&&restored;
}
static int objectSelfTest(SithThing *player) {
    EdModel *saved=(EdModel*)edAlloc(sizeof(*saved));if(!saved)return 0;edCopyModel(saved,model);EdSelection oldPicks=picks;
    int oldActive=active,oldTool=tool,oldTemplate=currentTemplate;active=1;tool=3;clearHistory();fireDown=grabDown=0;
    int ok=templateCount>500;for(int i=0;i<templateCount;i++)if(!__strcmpi(templateNames[i],"crate4_1"))currentTemplate=i;
    const char *categoryTemplates[]={"+laserhit","crate4_1","+bryarbolt","bactatank","powercell","strifle","reeyeesgun","woman2","yun","cargoship","4x4elev","2x3jam","console2","throwcrate1"};
    for(int c=0;c<14;c++){int id=templateIndex(categoryTemplates[c]);ok=ok&&id>=0&&(templateSections[id]&(1u<<(c+1)));}
    EdVec locked=objectFacingRotation(edV(15,0,25),edV(1,1,.8),0),unlocked=objectFacingRotation(edV(15,0,25),edV(1,1,.8),1);
    ok=ok&&locked.x==15&&locked.z==25&&locked.y==-45&&fabs(unlocked.x-15)>=5;
    ok=ok&&fabs(remainder(unlocked.x,5))<.001&&fabs(remainder(unlocked.y,5))<.001&&fabs(remainder(unlocked.z,5))<.001;
    SithThing *crate=sithTemplate_GetTemplate("crate4_1");
    EdVec probeOrigin=edV(0,0,.5),directions[]={edV(0,0,-1),edV(0,0,1),edV(1,0,0),edV(-1,0,0),edV(0,1,0),edV(0,-1,0),edV(1,1,-.5)};
    for(int d=0;d<7;d++){
        EdVec direction=edScale(directions[d],1/sqrt(edDot(directions[d],directions[d]))),position;
        edRay(model,probeOrigin,direction,&hover);int fits=objectPlacement(crate,direction,&position);ok=ok&&fits;
        if(fits)for(int f=0;f<model->c[hover.cell].count;f++){EdFace *face=&model->c[hover.cell].f[f];if(face->neighbor<0)ok=ok&&edDot(edNormal(face),edSub(position,face->v[0].p))>=.029;}
    }
    remember();edCopyModel(candidate,model);snprintf(candidate->header.colormap,128,"01narsh.cmp");ok=ok&&commit();
    int cmp=addColormap("01narsh.cmp");ok=ok&&!edCompare(sithWorld_g_pCurrentWorld->colormaps[0].colors,colormapColors(cmp),768)&&historyStep(0)&&!__strcmpi(liveColormap,saved->header.colormap);
    clearHistory();EdVec origin=edV(0,0,.5),direction=edV(0,.6,-.8);edRay(model,origin,direction,&hover);
    ok=ok&&objectControl(player,origin,direction,1,0,0,0)&&model->objectCount==1&&objectThings[0]>=0;
    EdVec placed=model->objects[0].position,look=edSub(placed,origin);look=edScale(look,1/sqrt(edDot(look,look)));
    fireDown=grabDown=0;edRay(model,origin,look,&hover);objectControl(player,origin,look,0,1,0,0);
    objectControl(player,edAdd(origin,edV(ED_GRID,0,0)),look,0,1,0,0);objectControl(player,origin,look,0,0,0,0);
    ok=ok&&fabs(model->objects[0].position.x-placed.x-ED_GRID)<.001&&undoCount==2;
    ok=ok&&historyStep(0)&&fabs(model->objects[0].position.x-placed.x)<.001&&historyStep(0)&&model->objectCount==0&&historyStep(1)&&model->objectCount==1;
    clearHistory();grabbing=objectGrabbing=objectRotating=fireDown=grabDown=0;
    EdVec end=objectFacingHandle(model,0),rotationOrigin=edAdd(end,edV(.3,-.35,.3)),rotationLook=edSub(end,rotationOrigin);
    rotationLook=edScale(rotationLook,1/sqrt(edDot(rotationLook,rotationLook)));EdVec previousRotation=model->objects[0].rotation,previousPosition=model->objects[0].position;
    edRay(model,rotationOrigin,rotationLook,&hover);objectControl(player,rotationOrigin,rotationLook,0,1,0,0);ok=ok&&objectRotating&&objectGrabbed==0;
    objectControl(player,edAdd(rotationOrigin,edV(ED_GRID,0,0)),rotationLook,0,1,0,0);
    objectControl(player,edAdd(rotationOrigin,edV(ED_GRID,0,ED_GRID)),rotationLook,0,1,0,0);objectControl(player,rotationOrigin,rotationLook,0,0,0,0);
    EdVec turn=edSub(model->objects[0].rotation,previousRotation),shift=edSub(model->objects[0].position,previousPosition);
    ok=ok&&fabs(turn.x)<.001&&fabs(turn.y)>=5&&fabs(remainder(turn.y,5))<.001&&edDot(shift,shift)<ED_EPS*ED_EPS&&undoCount==1;
    rdVector3 liveForward=sithWorld_g_pCurrentWorld->aThings[objectThings[0]].orient.lvec;EdVec handleForward=edSub(objectFacingHandle(model,0),previousPosition);
    ok=ok&&edDot(vec(liveForward),handleForward)/sqrt(edDot(handleForward,handleForward))>.999;
    ok=ok&&historyStep(0)&&fabs(model->objects[0].rotation.x-previousRotation.x)<.001&&fabs(model->objects[0].rotation.y-previousRotation.y)<.001;
    ok=ok&&historyStep(1)&&fabs(model->objects[0].rotation.y-previousRotation.y)>.1;
    ok=ok&&deleteEditorObject(0)&&model->objectCount==0&&objectThings[0]<0;
    ok=ok&&historyStep(0)&&model->objectCount==1&&objectThings[0]>=0&&historyStep(1)&&model->objectCount==0;
    ok=ok&&!deleteEditorObject(-10000)&&!deleteEditorObject(-1)&&model->spawnCount==1;
    EdVec walkOrigin=edAdd(model->spawns[0].position,edV(0,-.5,0)),walkPosition=model->spawns[0].position;fireDown=grabDown=0;grabbing=objectGrabbing=0;
    edRay(model,walkOrigin,edV(0,1,0),&hover);objectControl(player,walkOrigin,edV(0,1,0),0,1,0,0);
    objectControl(player,edAdd(walkOrigin,edV(ED_GRID,0,0)),edV(0,1,0),0,1,0,0);objectControl(player,walkOrigin,edV(0,1,0),0,0,0,0);
    ok=ok&&fabs(model->spawns[0].position.x-walkPosition.x-ED_GRID)<.001&&fabs(jkPlayer_playerInfos[0].orient.scale.x-model->spawns[0].position.x)<.001;
    clearHistory();grabbing=objectGrabbing=objectRotating=fireDown=grabDown=0;
    EdVec spawnPosition=model->spawns[0].position,spawnRotation=model->spawns[0].rotation;
    end=objectFacingHandle(model,-1);rotationOrigin=edAdd(end,edV(.3,-.35,.3));rotationLook=edSub(end,rotationOrigin);rotationLook=edScale(rotationLook,1/sqrt(edDot(rotationLook,rotationLook)));
    edRay(model,rotationOrigin,rotationLook,&hover);objectControl(player,rotationOrigin,rotationLook,0,1,0,0);ok=ok&&objectRotating&&objectGrabbed==-1;
    objectControl(player,edAdd(rotationOrigin,edV(ED_GRID,0,ED_GRID)),rotationLook,0,1,0,0);objectControl(player,rotationOrigin,rotationLook,0,0,0,0);
    turn=edSub(model->spawns[0].rotation,spawnRotation);shift=edSub(model->spawns[0].position,spawnPosition);
    handleForward=edSub(objectFacingHandle(model,-1),spawnPosition);liveForward=jkPlayer_playerInfos[0].orient.lvec;
    ok=ok&&fabs(turn.x)<.001&&fabs(turn.y)>=5&&fabs(remainder(turn.y,5))<.001&&edDot(shift,shift)<ED_EPS*ED_EPS&&undoCount==1;
    ok=ok&&edDot(vec(liveForward),handleForward)/sqrt(edDot(handleForward,handleForward))>.999&&historyStep(0);
    clearHistory();edCopyModel(candidate,saved);int restored=commit();picks=oldPicks;active=oldActive;tool=oldTool;currentTemplate=oldTemplate;grabbing=objectGrabbing=fireDown=grabDown=0;syncEditorObjects(model);edFree(saved);
    printf("GEOSITH_OBJECTS %s: template categories, live CMP/undo, floor/wall/corner clearance, dragging, object/spawn facing handles and rotation/undo, deletion/undo, spawn movement\n",ok&&restored?"PASS":"FAIL");fflush(stdout);return ok&&restored;
}
static int stairsWorldCheck(int steps) {
    SithWorld *world=sithWorld_g_pCurrentWorld;
    if(model->count!=steps+1||world->numAdjoinsLoaded!=steps*2)return 0;
    int ok=1;for(int a=0;a<world->numAdjoinsLoaded;a++){SithSurfaceAdjoin *adjoin=&world->adjoins[a];ok=ok&&(adjoin->flags&7)==5&&adjoin->mirror&&adjoin->mirror->mirror==adjoin&&(adjoin->surface->flags&5)==5&&adjoin->surface->surfaceInfo.face.geometryMode==0;}
    for(int i=0;i<steps;i++)for(int sphere=0;sphere<2;sphere++){
        double y=-1+2.0*(i+.5)/steps,z=(i+.5)/steps,height=sphere?.1:.04,radius=sphere?.02:0;
        rdVector3 origin=rdvec(edV(0,y,z+height)),direction=rdvec(edV(0,0,-1));
        sithCollision_SearchForCollisions(&world->aSectors[0],NULL,&origin,&direction,.2,radius,RAYCAST_2000|RAYCAST_800|RAYCAST_2);
        SithCollision *hit;int found=0;while((hit=sithCollision_PopStack()))if((hit->type&SITHCOLLISION_WORLD)&&hit->surface){
            found=hit->surface->pAdjoin&&(hit->surface->flags&5)==5&&hit->surface->surfaceInfo.face.geometryMode==0&&fabs(hit->distance-(height-radius/sqrt(.8)))<.001;break;
        }
        sithCollision_DecreaseStackLevel();ok=ok&&found;
    }
    return ok;
}
static int stairsSelfTest(SithThing *player) {
    EdModel *saved=edAlloc(sizeof(*saved));if(!saved)return 0;edCopyModel(saved,model);EdSelection selected=picks;EdVec position=vec(player->position);
    clearHistory();picks.count=2;picks.hits[0]=EdHit{0,4,-1,-1};picks.hits[1]=EdHit{0,2,-1,-1};
    edCopyModel(candidate,model);int ok=edCreateStairs(candidate,picks.hits[0],picks.hits[1],4);remember();ok=ok&&commit()&&model->count==5&&undoCount==1;
    ok=ok&&stairsWorldCheck(4)&&writeEditorGob(model,"Episode/sithed_stairs_test.gob","Stairs integration test",0);
    ok=ok&&historyStep(0)&&model->count==1&&picks.count==2&&picks.hits[0].face==4&&picks.hits[1].face==2&&historyStep(1)&&model->count==5;
    clearHistory();edCopyModel(candidate,saved);player->position=rdvec(position);int restored=commit();picks=selected;edFree(saved);
    printf("GEOSITH_STAIRS %s: wedge sectors, smooth ramp collision, selection order and one-step undo/redo\n",ok&&restored?"PASS":"FAIL");fflush(stdout);return ok&&restored;
}
static int deleteSector(SithThing *player,int sector) {
    if(model->count<=1){say("Keep at least one sector in the level.");return 0;}
    edCopyModel(candidate,model);if(!edDeleteSector(candidate,sector)){say("Could not delete that sector.");return 0;}
    int inside=0;for(int s=0;s<candidate->count;s++)if(edContains(&candidate->c[s],vec(player->position))){inside=1;break;}
    remember();grabbing=groupGrab=cleaving=0;
    if(!commit())return 0;
    if(!inside&&!active){nextSpawn=0;teleportWalkplayer(player);}
    hover.cell=-1;say("Sector deleted. Ctrl+Z to undo.");return 1;
}
static int deleteGeometryHandle(EdHit hit){
    edCopyModel(candidate,model);
    if(!edDeleteHandle(candidate,hit)){say("Cannot delete this handle: surfaces must remain closed, planar and convex, with matching adjoins.");return 0;}
    remember();grabbing=groupGrab=cleaving=0;if(!commit())return 0;
    picks.count=0;hover.cell=-1;say(hit.vertex>=0?"Vertex removed. Ctrl+Z to undo.":"Edge dissolved; coplanar surfaces merged. Ctrl+Z to undo.");return 1;
}
static int deleteHandleSelfTest(void){
    EdModel *saved=edAlloc(sizeof(*saved));if(!saved)return 0;edCopyModel(saved,model);EdSelection selected=picks;clearHistory();
    edDefault(candidate);candidate->episode=saved->episode;int ok=commit();
    edCopyModel(candidate,model);EdFace face=candidate->c[0].f[4];ok=ok&&edCleave(candidate,0,4,face.v[0].p,face.v[2].p)&&commit();
    EdHit edge;ok=ok&&edRaySelection(model,edV(0,0,.5),edV(0,1,0),&edge)&&edge.edge>=0;
    ok=ok&&deleteGeometryHandle(edge)&&model->c[0].count==6&&sithWorld_g_pCurrentWorld->numSurfaces==6&&undoCount==1;
    ok=ok&&historyStep(0)&&model->c[0].count==7&&sithWorld_g_pCurrentWorld->numSurfaces==7;
    ok=ok&&historyStep(1)&&model->c[0].count==6&&sithWorld_g_pCurrentWorld->numSurfaces==6;
    edCopyModel(candidate,saved);int restored=commit();clearHistory();picks=selected;edFree(saved);
    printf("GEOSITH_DELETE_HANDLES %s: highlighted edge, coplanar merge, live geometry and undo/redo\n",ok&&restored?"PASS":"FAIL");fflush(stdout);return ok&&restored;
}
static SithThing *extrudeEffect(EdVec center,int sector) {
    SithThing *source=sithTemplate_GetTemplate("+raildet_exp");if(!source)return NULL;
    railEffectTemplate=*source;
    railEffectTemplate.explosionParams.flags=SITHEXPLOSION_FLAG_ANIMATED_SPRITE;
    railEffectTemplate.explosionParams.damage=railEffectTemplate.explosionParams.force=railEffectTemplate.explosionParams.range=0;
    edZero(railEffectTemplate.explosionParams.apDebries,0,sizeof(railEffectTemplate.explosionParams.apDebries));
    railEffectTemplate.collide=0;railEffectTemplate.moveType=SITH_MT_NONE;
    railEffectTemplate.pCog=railEffectTemplate.pCaptureCog=NULL;railEffectTemplate.pCreateThingTemplate=NULL;
    rdMatrix34 orient;rdMatrix_BuildRotate34(&orient,&rdroid_zeroVector3);rdVector3 position=rdvec(center);
    return sithThing_CreateThingAtPos(&railEffectTemplate,&position,&orient,&sithWorld_g_pCurrentWorld->aSectors[sector],NULL);
}
static int extrudeFace(int cell,int face,int horizontal) {
    if(cell<0||cell>=model->count||face<0||face>=model->c[cell].count)return 0;
    const EdFace *f=&model->c[cell].f[face];EdVec center=edV(0,0,0);
    for(int i=0;i<f->count;i++)center=edAdd(center,f->v[i].p);
    center=edScale(center,1.0/f->count);
    edCopyModel(candidate,model);if(!(horizontal?edExtrudeHorizontal(candidate,cell,face):edExtrudeStep(candidate,cell,face))) {say(horizontal?"Horizontal extrusion rejected: choose an angled or vertical wall with room for a valid sector.":"Extrusion rejected: no room for a valid sector.");return 0;}
    remember();if(!commit())return 0;
    extrudeEffect(center,cell);grabbing=cleaving=0;
    say(horizontal?"Extruded horizontally to a vertical face. Release and grab the new face to drag it.":"Extruded one grid unit. Release and grab the new face to drag it.");return 1;
}
static int extrudeGrabbedFace(EdVec movement,int horizontal) {
    const EdFace *face=&snapshot->c[selection.cell].f[selection.face];EdVec center=edV(0,0,0);
    for(int i=0;i<face->count;i++)center=edAdd(center,face->v[i].p);
    double distance=sqrt(edDot(movement,movement));
    if(distance>ED_EPS)movement=edScale(movement,round(distance/ED_GRID)*ED_GRID/distance);
    center=edAdd(edScale(center,1.0/face->count),movement);
    /* Pick the moved grabbed polygon itself, even when CSG renumbered it.
       Rotating away while grabbing must never extrude a different wall. */
    EdHit target;
    if(edRay(model,edAdd(center,edScale(grabNormal,ED_GRID*.1)),edScale(grabNormal,-1),&target)&&target.distance<ED_GRID*.25)
        return extrudeFace(target.cell,target.face,horizontal);
    say("The grabbed face cannot be extruded here.");return 0;
}
static void selectOppositeAdjoins(void) {
    int changed=0;
    for(int i=0;i<picks.count;i++){
        EdHit *hit=&picks.hits[i];
        if(hit->vertex>=0||hit->edge>=0||hit->cell<0||hit->cell>=model->count||hit->face<0||hit->face>=model->c[hit->cell].count)continue;
        const EdFace *face=&model->c[hit->cell].f[hit->face];
        if(face->neighbor<0||face->neighbor>=model->count||face->mirror<0||face->mirror>=model->c[face->neighbor].count)continue;
        const EdFace *opposite=&model->c[face->neighbor].f[face->mirror];
        if(opposite->neighbor!=hit->cell||opposite->mirror!=hit->face)continue;
        hit->cell=face->neighbor;hit->face=face->mirror;changed++;
    }
    if(changed)say("Selected the opposite side of the adjoin. Backspace switches back.");
}
static int allSelectedSurfaces(const EdSelection *set) {if(!set->count)return 0;for(int i=0;i<set->count;i++)if(set->hits[i].vertex>=0||set->hits[i].edge>=0)return 0;return 1;}
static int extrudeSelectedFaces(int horizontal) {
    if(!allSelectedSurfaces(&picks)){say("Select surfaces only for grouped extrusion.");return 0;}
    edCopyModel(candidate,model);if(!(horizontal?edExtrudeSelectionHorizontal(candidate,&picks):edExtrudeSelection(candidate,&picks))){say("Grouped extrusion rejected: geometry must remain valid.");return 0;}
    EdSelection caps={0};EdVec centers[ED_SELECTIONS];int count=picks.count;
    EdModel *capSource=edAlloc(sizeof(*capSource));if(!capSource)return 0;edCopyModel(capSource,model);
    for(int i=0;i<count;i++) {
        EdHit h=picks.hits[i];const EdFace *face=&model->c[h.cell].f[h.face];centers[i]=edV(0,0,0);
        for(int v=0;v<face->count;v++)centers[i]=edAdd(centers[i],face->v[v].p);centers[i]=edScale(centers[i],1.0/face->count);
        EdSelection one={0},mapped;one.count=1;one.hits[0]=h;
        edExtrusionCap(face,horizontal,&capSource->c[h.cell].f[h.face]);
        edSelectionRemap(capSource,candidate,&one,edV(0,0,0),&mapped);
        for(int j=0;j<mapped.count&&caps.count<ED_SELECTIONS;j++) {
            int duplicate=0;for(int k=0;k<caps.count;k++)if(caps.hits[k].cell==mapped.hits[j].cell&&caps.hits[k].face==mapped.hits[j].face)duplicate=1;
            if(!duplicate)caps.hits[caps.count++]=mapped.hits[j];
        }
    }
    edFree(capSource);remember();if(!commit())return 0;picks=caps;grabbing=groupGrab=cleaving=0;edResetDoubleTap(&selectionTap,1);
    for(int i=0;i<count;i++){int sector=0;for(int s=0;s<model->count;s++)if(edContains(&model->c[s],centers[i])){sector=s;break;}extrudeEffect(centers[i],sector);}
    say(horizontal?"Extruded selected surfaces horizontally. The new vertical faces remain selected.":"Extruded selected surfaces one grid unit. The new faces remain selected.");return 1;
}
static int horizontalExtrusionSelfTest(void) {
    EdModel *saved=edAlloc(sizeof(*saved));if(!saved)return 0;edCopyModel(saved,model);int ok=1;
    for(int grouped=0;grouped<2&&ok;grouped++){
        edCopyModel(candidate,saved);
        if(grouped)ok=edCleave(candidate,0,4,edV(0,1,0),edV(0,1,1));
        for(int f=0;f<candidate->c[0].count;f++)for(int v=0;v<candidate->c[0].f[f].count;v++)candidate->c[0].f[f].v[v].p.y+=1.5*(candidate->c[0].f[f].v[v].p.z-.5);
        edMapWorld(candidate);ok=ok&&commit();clearHistory();
        picks.count=grouped?2:0;picks.hits[0]=EdHit{0,4,-1,-1};picks.hits[1]=EdHit{0,6,-1,-1};
        ok=ok&&(grouped?extrudeSelectedFaces(1):extrudeFace(0,4,1));
        ok=ok&&model->count==2+grouped&&sithWorld_g_pCurrentWorld->numSectors==2+grouped&&undoCount==1;
        if(grouped){ok=ok&&picks.count==2;for(int i=0;i<picks.count;i++){EdHit h=picks.hits[i];ok=ok&&fabs(edNormal(&model->c[h.cell].f[h.face]).z)<ED_EPS;}}
        else ok=ok&&fabs(edNormal(&model->c[1].f[1]).z)<ED_EPS;
        ok=ok&&historyStep(0)&&model->count==1&&historyStep(1)&&model->count==2+grouped;
    }
    edCopyModel(candidate,saved);int restored=commit();clearHistory();picks.count=0;edFree(saved);
    printf("GEOSITH_HORIZONTAL_EXTRUDE %s: angled walls, selected vertical caps, one-step undo/redo\n",ok&&restored?"PASS":"FAIL");fflush(stdout);return ok&&restored;
}
void jkEditor_SelfTest(void) try {
    if(getenv("GEOSITH_OUTSIDE_TEST")){
        int ok=jkEditor_IsLevel()&&bindEditorWorld();
        if(ok){SithThing *p=sithWorld_g_pCurrentWorld->pLocalPlayer;active=1;clearHistory();
            rdVector3 position=p->position;rdMatrix34 orientation=p->orient;
            edCopyModel(candidate,model);ok=edExtrude(candidate,0,4,edV(0,1,0))&&commit();
            setEditorFlight(p,0);ok=ok&&deleteSector(p,0)&&objectSector(model,vec(p->position))<0;
            ok=ok&&rdVector_Dist3(&position,&p->position)<ED_EPS&&!edCompare(&orientation,&p->orient,sizeof(orientation));
            ok=ok&&(p->physicsParams.flags&SITH_PF_FLY)&&!(p->physicsParams.flags&SITH_PF_USEGRAVITY)&&(g_debugmodeFlags&DEBUGFLAG_NOCLIP)&&p->sector;
            ok=ok&&historyStep(0)&&objectSector(model,vec(p->position))>=0;
            setEditorFlight(p,0);ok=ok&&historyStep(1)&&rdVector_Dist3(&position,&p->position)<ED_EPS&&(p->physicsParams.flags&SITH_PF_FLY)&&(g_debugmodeFlags&DEBUGFLAG_NOCLIP);
            ok=ok&&historyStep(0);setEditorFlight(p,0);mapOpen(p);edCopyModel(candidate,model);ok=ok&&edDeleteSector(candidate,0)&&commit();mapClose(p);
            ok=ok&&rdVector_Dist3(&position,&p->position)<ED_EPS&&(p->physicsParams.flags&SITH_PF_FLY)&&(g_debugmodeFlags&DEBUGFLAG_NOCLIP);
        }
        printf("GEOSITH_OUTSIDE %s: deletion, undo/redo and map edits preserve viewpoint with collision-free flight\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_OVERLAY_TEST")){int ok=jkEditor_IsLevel()&&bindEditorWorld()&&overlayVisibilityTest();printf("GEOSITH_OVERLAY %s: portal visibility, boundary origins, model faces, lights and frames\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);}

    if(getenv("GEOSITH_MESH_GRAB_TEST")){int ok=jkEditor_IsLevel()&&bindEditorWorld()&&builderGrabSelfTest();printf("GEOSITH_MESH_GRAB %s: stationary grab, remapped extrusion and undo\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);}
    if(getenv("GEOSITH_CLEAVE_UNDO_TEST")){int ok=jkEditor_IsLevel()&&bindEditorWorld()&&builderCleaveSelfTest();printf("GEOSITH_CLEAVE_UNDO %s: fire/release cleave, Ctrl+Z/Ctrl+Y and primitive detail controls\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);}
    if(getenv("GEOSITH_ORIGIN_TEST")){int ok=jkEditor_IsLevel()&&bindEditorWorld()&&builderOriginSelfTest();printf("GEOSITH_ORIGIN %s: through-mesh picking, snapped origin drag, fixed vertices, undo/redo and serialization\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);}
    if(getenv("GEOSITH_STORAGE_RUNTIME")){
        int ok=jkEditor_IsLevel()&&bindEditorWorld()&&sithTemplate_GetTemplate("crate4_1");
        if(ok){active=1;remember();edDefault(candidate);candidate->objectCount=2300;
            for(int i=0;i<candidate->objectCount;i++){snprintf(candidate->objects[i].templateName,64,"crate4_1");candidate->objects[i].position=edV(0,0,.5);}
            ok=commit()&&sithWorld_g_pCurrentWorld->numThingsLoaded>=2941;
            for(int i=0;ok&&i<model->objectCount;i++)ok=objectThings[i]>=0&&sithThing_ValidateThingPointer(&sithWorld_g_pCurrentWorld->aThings[objectThings[i]]);
            ok=ok&&historyStep(0)&&model->objectCount==0&&historyStep(1)&&model->objectCount==2300;
            SithWorld *world=sithWorld_g_pCurrentWorld;int old=world->numThingsLoaded,freeBefore=sithNet_thingsIdx;
            ok=ok&&ensureThingSlots(world,SITH_MAX_THINGS)&&sithNet_thingsIdx==freeBefore+SITH_MAX_THINGS-old&&sithThing_ValidateThingPointer(&world->aThings[SITH_MAX_THINGS-1]);
        }
        printf("GEOSITH_STORAGE_RUNTIME %s: 2300 live objects, undo/redo, stable growth to %d thing slots\n",ok?"PASS":"FAIL",SITH_MAX_THINGS);fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_STOCK_TEST")){int ok=jkEditor_IsLevel()&&bindEditorWorld();
        ok=ok&&stockEpisode("JK1")&&stockEpisode("jK1mP")&&!stockEpisode("m10_edited");
        if(ok&&getenv("GEOSITH_STOCK_EDIT")){double original=model->header.gravity;int cells=model->count,objects=model->objectCount;active=1;remember();edCopyModel(candidate,model);candidate->header.gravity=original+.25;
            ok=commit()&&historyStep(0)&&fabs(model->header.gravity-original)<.00001&&historyStep(1)&&fabs(model->header.gravity-original-.25)<.00001&&historyStep(0);
            active=0;ok=ok&&apply(model)&&model->count==cells&&model->objectCount==objects;printf("GEOSITH_STOCK_EDIT %s: live editor/play rebuild, compressed undo/redo\n",ok?"PASS":"FAIL");fflush(stdout);
        }
        if(ok&&getenv("GEOSITH_STOCK_EXPORT")){const char *path="Episode/geosith_stock_conversion_test.gob";FILE *existing=fopen(path,"rb");if(existing){fclose(existing);ok=0;}else ok=writeEditorGob(model,path,"Stock conversion test",sithNet_isMulti);}
        printf("GEOSITH_STOCK %s: sectors=%d objects=%d cogs=%d\n",ok?"PASS":"FAIL",model?model->count:0,model?model->objectCount:0,model?model->cogCount:0);fflush(stdout);exit(ok?0:2);}

    if(getenv("GEOSITH_VERIFY_TERRAIN_GROUP")){
        int ok=bindEditorWorld()&&model->assetCount==2&&model->objectCount==2&&model->assets[0].terrainGroup>0&&model->assets[0].terrainGroup==model->assets[1].terrainGroup;
        if(ok){edCopyModel(candidate,model);ok=edTerrainSculpt(candidate,1,edV(0,1,0),.5,.1)&&edMeshValidate(candidate);int moved[2]={};
            for(int i=0;i<candidate->meshCount;i++)for(int f=0;f<candidate->meshes[i].geometry.count;f++)for(int v=0;v<3;v++){
                EdVec p=candidate->meshes[i].geometry.f[f].v[v].p;if(fabs(p.x)<1e-8&&fabs(p.y-1)<1e-8&&p.z>.09)moved[candidate->meshes[i].asset]++;
            }ok=ok&&moved[0]&&moved[1];
        }
        printf("GEOSITH_TERRAIN_GROUP_RELOAD %s: sector objects, group metadata and welded sculpting\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_VERIFY_TERRAIN")&&!strcmp(jkRes_episodeGobName,"sithed_terrain_test")){
        int ok=bindEditorWorld()&&model->assetCount>0&&model->assets[0].terrain&&model->objectCount>0&&edTerrainObject(model,model->objectCount-1)==0;
        if(ok){edCopyModel(candidate,model);ok=edTerrainSculpt(candidate,0,edSub(model->assets[0].origin,edScale(model->assets[0].terrainNormal,.05)),.75,.1)&&edMeshValidate(candidate);}
        printf("GEOSITH_TERRAIN_RELOAD %s: playable model and editable terrain metadata\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_VERIFY_SPLINES")&&!strcmp(jkRes_episodeGobName,"sithed_spline_test")){
        int ok=jkEditor_IsLevel()&&bindEditorWorld()&&model->splineCount==1&&model->splines[0].count==3;edSplineReconcile(model);ok=ok&&model->splineCount==1;
        if(ok){EdSpline draft=model->splines[0];draft.points[1].position.z+=.125;ok=edSplineRebuild(candidate,model,&draft)&&edValidate(candidate);}
        printf("GEOSITH_SPLINE_RELOAD %s: saved controls, generated sectors and editable heights\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_VERIFY_PLACED")&&jkEditor_IsLevel()){
        int ok=bindEditorWorld()&&placedEditingSelfTest();printf("GEOSITH_PLACED %s: saved overlapping level, live extrusion, cleave, consecutive insertions and undo/redo\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_VERIFY_SECTORS")&&!strcmp(jkRes_episodeGobName,"sithed_sector_test")){
        int ok=jkEditor_IsLevel()&&bindEditorWorld();SithWorld *world=sithWorld_g_pCurrentWorld;
        ok=ok&&model->count==1&&!strcmp(model->c[0].colormap,"01narsh.cmp")&&!strcmp(model->c[0].layer,"Sector test")&&fabs(model->c[0].extraLight-.35)<.00001&&(model->c[0].flags&1)&&world->numColormaps==2&&world->aSectors[0].colormap==&world->colormaps[1];
        printf("GEOSITH_SECTORS_RELOAD %s: flags, extra light, tint, layer and native sector colormap\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_VERIFY_FRAMES")&&!strcmp(jkRes_episodeGobName,"sithed_frames_test")){
        int ok=jkEditor_IsLevel()&&bindEditorWorld()&&model->objectCount==1&&model->objects[0].attributes.frameCount==ED_FRAMES;
        if(ok){SithThing *thing=&sithWorld_g_pCurrentWorld->aThings[objectThings[0]];ok=thing->moveType==SITH_MT_PATH&&thing->trackParams.loadedFrames==ED_FRAMES&&fabs(thing->trackParams.aFrames[2].pos.x-model->objects[0].attributes.frames[2].position.x)<.001&&fabs(model->objects[0].attributes.frames[ED_FRAMES-1].position.z-(.2+(ED_FRAMES-1)*ED_GRID))<.001;}
        printf("GEOSITH_FRAMES_RELOAD %s: full path, positions, rotations and native frame count\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_VERIFY_COGS")&&!strcmp(jkRes_episodeGobName,"sithed_cogs_test")){int ok=jkEditor_IsLevel()&&bindEditorWorld()&&cogReloadTest();printf("GEOSITH_COGS_RELOAD %s: bundled script, typed parameters, thing/surface links\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);}
    if(getenv("GEOSITH_VERIFY_EPISODE")&&!strcmp(jkRes_episodeGobName,"sithed_episode_test")){
        int ok=jkEditor_IsLevel()&&bindEditorWorld();EdEpisode *e=&model->episode;
        ok=ok&&e->count==2&&e->type==3&&!strcmp(e->title,"Episode integration test")&&e->rows[0].line==10&&e->rows[0].lightPower==2&&e->rows[0].darkPower==1&&e->rows[0].gotoB==20&&e->rows[1].cutscene&&!strcmp(e->rows[1].file,"outro.smk");
        ok=ok&&!strcmp(sithWorld_g_pCurrentWorld->map_jkl_fname,"episode_test.jkl")&&!strcmp(model->header.title,"Episode Level Name")&&!strcmp(model->header.intro[0],"Episode loading text")&&!strcmp(model->episode.rows[1].title,"Ending")&&!strcmp(model->episode.rows[1].intro[0],"Ending text");
        printf("GEOSITH_EPISODE_RELOAD %s: sequence fields, custom JKL, editor recognition and loading strings\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_VERIFY_LIGHTS")&&!strcmp(jkRes_episodeGobName,"sithed_lights_test")&&jkEditor_IsLevel()){
        SithWorld *w=sithWorld_g_pCurrentWorld;flex_t baked[6][ED_VERTS];int ok=w->numSurfaces==6;
        for(int f=0;f<6&&ok;f++){SithSurface *surface=&w->surfaces[f];ok=surface->surfaceInfo.face.numVertices<=ED_VERTS&&surface->surfaceInfo.face.lightingMode==RD_LIGHTMODE_GOURAUD&&surface->pSector->ambientLight==0&&fabs(surface->pSector->extraLight-.5)<.00001;
            for(int v=0;v<surface->surfaceInfo.face.numVertices&&ok;v++)baked[f][v]=surface->surfaceInfo.intensities[v];}
        ok=ok&&bindEditorWorld()&&model->lightCount==1&&fabs(model->lights[0].intensity-1.1)<.001&&fabs(model->lights[0].range-(4-ED_GRID))<.001&&lightsWorldCheck();
        for(int f=0;f<6&&ok;f++)for(int v=0;v<model->c[0].f[f].count;v++)ok=ok&&fabs(baked[f][v]-edVertexLight(model,0,f,v))<.00001;
        printf("GEOSITH_LIGHTS_RELOAD %s: editor light records and baked vertex lighting\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_VERIFY_STAIRS")&&!strcmp(jkRes_episodeGobName,"sithed_stairs_test")&&jkEditor_IsLevel()){
        int ok=bindEditorWorld()&&stairsWorldCheck(4);printf("GEOSITH_STAIRS_RELOAD %s: wedge sectors, invisible floor flags and ramp collision\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_VERIFY_LEGACY")&&!strcmp(jkRes_episodeGobName,"sithed_smoke")&&jkEditor_IsLevel()) {
        int ok=bindEditorWorld();
        if(ok){const EdFace *f=&model->c[0].f[0];SithWorld *w=sithWorld_g_pCurrentWorld;rdFace *live=&w->surfaces[0].surfaceInfo.face;
            ok=fabs(f->uOffset-16)<.001&&fabs(f->vOffset+16)<.001;
            for(int i=0;i<f->count;i++){rdVector2 uv=w->aTexVerticies[live->vertexUVIdx[i]];ok=ok&&fabs(uv.x-f->v[i].u)<.001&&fabs(uv.y-f->v[i].v)<.001;}
        }
        printf("GEOSITH_LEGACY_UV %s: density and offsets upgraded in live world\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_VERIFY_RELOAD")&&!strcmp(jkRes_episodeGobName,"sithed_runtime_test")&&jkEditor_IsLevel()) {
        int ok=bindEditorWorld()&&model->spawnCount==2&&jkPlayer_maxPlayers==2;
        if(ok) {
            ok=fabs(model->spawns[1].position.x-.5)<ED_EPS&&fabs(model->spawns[1].rotation.y-90)<.001;
            ok=ok&&fabs(model->c[0].f[0].uOffset-ED_UV_SCALE*ED_GRID)<.001&&fabs(model->c[0].f[0].vOffset+ED_UV_SCALE*ED_GRID)<.001;
            ok=ok&&model->c[0].f[0].material>=baseMaterialCount&&editorMaterial(model->c[0].f[0].material);
            ok=ok&&model->objectCount==1&&!__strcmpi(model->objects[0].templateName,"crate4_1")&&fabs(model->objects[0].position.x-.25)<.001&&fabs(model->objects[0].rotation.y-90)<.001;
            EdSurfaceAttributes attrs=edSurfaceAttributes(&model->c[0].f[0]);ok=ok&&attrs.surfaceFlags==0x10005&&attrs.faceFlags==1&&fabs(attrs.uScale-2)<.001&&fabs(attrs.vScale-.5)<.001&&fabs(attrs.extraLight-.25)<.001;
            const EdLevelHeader *h=&model->header;
            ok=ok&&!strcmp(h->title,"Header integration test")&&!strcmp(h->colormap,"01narsh.cmp")&&!strcmp(h->intro[0],"Welcome to the test level.")&&!strcmp(h->intro[19],"Final intro line.");
            ok=ok&&fabs(h->gravity-3)<.001&&fabs(h->ceilingSkyZ-18)<.001&&fabs(h->horizonDistance-120)<.001&&fabs(h->horizonPixels-1024)<.001;
            ok=ok&&fabs(h->horizonOffset[0]-2)<.001&&fabs(h->ceilingOffset[1]+3)<.001&&fabs(h->mip[3]-8)<.001&&fabs(h->lod[3]-2.4)<.001&&fabs(h->perspective-3)<.001&&fabs(h->gouraud-4)<.001;
            nextSpawn=0;SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;
            ok=ok&&teleportWalkplayer(player)==1&&teleportWalkplayer(player)==2&&fabs(player->position.x-.5)<ED_EPS;
        }
        printf("GEOSITH_RELOAD %s: level header, title, intro, colormap, UV offsets and walkplayers\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_STRESS")&&(getenv("GEOSITH_NET_ROLE")||!strcmp(jkRes_episodeGobName,"sithed_smoke"))) {
        jkGuiTitle_bSkipLoadingWait=1;return;
    }
    if(!getenv("GEOSITH_SELFTEST")||strcmp(jkRes_episodeGobName,"sithed_smoke")||!jkEditor_IsLevel())return;
    if(!bindEditorWorld()){printf("GEOSITH_SELFTEST FAIL: import\n");fflush(stdout);exit(2);}
    /* Explicit opt-in integration test for the generated smoke-test episode. */
    static int selfTestRan=0;
    if(!selfTestRan && getenv("GEOSITH_SELFTEST") && !strcmp(jkRes_episodeGobName,"sithed_smoke")) {
        selfTestRan=1;edCopyModel(snapshot,model);edCopyModel(candidate,model);
        SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;
        uint32_t originalPhysics=player->physicsParams.flags,originalDebug=g_debugmodeFlags;
        setEditorFlight(player,0);
        int ok=highlightSelfTest()&&!(player->physicsParams.flags&SITH_PF_FLY)&&(player->physicsParams.flags&SITH_PF_USEGRAVITY)&&!(g_debugmodeFlags&DEBUGFLAG_NOCLIP);
        ok=ok&&!toggleFlightCollision(player);
        setEditorFlight(player,1);
        ok=ok&&(player->physicsParams.flags&SITH_PF_FLY)&&!(player->physicsParams.flags&SITH_PF_USEGRAVITY)&&!(g_debugmodeFlags&DEBUGFLAG_NOCLIP);
        ok=ok&&toggleFlightCollision(player)&&(g_debugmodeFlags&DEBUGFLAG_NOCLIP);
        ok=ok&&toggleFlightCollision(player)&&!(g_debugmodeFlags&DEBUGFLAG_NOCLIP);
        /* Releasing flight controls stops existing momentum, including vertical drift. */
        player->physicsParams.vel=rdvec(edV(1,2,3));
        player->physicsParams.acceleration=rdvec(edV(4,5,6));
        player->physicsParams.deltaVelocity=rdvec(edV(.1,.2,.3));
        editorFlightMovement(player);
        ok=ok&&rdVector_IsZero3(&player->physicsParams.vel)&&rdVector_IsZero3(&player->physicsParams.acceleration)&&rdVector_IsZero3(&player->physicsParams.deltaVelocity);
        setEditorFlight(player,0);
        ok=ok&&!(player->physicsParams.flags&SITH_PF_FLY)&&(player->physicsParams.flags&SITH_PF_USEGRAVITY)&&!(g_debugmodeFlags&DEBUGFLAG_NOCLIP);
        player->physicsParams.flags=originalPhysics;g_debugmodeFlags=originalDebug;
        model->spawnCount=2;model->spawns[1].position=edV(.5,.5,.12);model->spawns[1].rotation=edV(0,90,0);nextSpawn=0;
        ok=ok&&teleportWalkplayer(player)==1&&edDot(edSub(vec(player->position),model->spawns[0].position),edSub(vec(player->position),model->spawns[0].position))<ED_EPS;
        ok=ok&&teleportWalkplayer(player)==2&&fabs(player->position.x-.5)<ED_EPS;
        ok=ok&&teleportWalkplayer(player)==1;edCopyModel(model,snapshot);nextSpawn=0;
        for(int repeat=0;repeat<64&&ok;repeat++) {
            sithAIAwareness_CreateTransmittingEvent(player->sector,&player->position,0,1,player);
            sithSurface_SurfaceLightAnim(&sithWorld_g_pCurrentWorld->surfaces[0],.5,1);
            sithSurface_SetSectorLight(&sithWorld_g_pCurrentWorld->aSectors[0],.5,1,0);
            edCopyModel(candidate,snapshot);
            EdHit moved={0,4,-1,-1};
            ok=edMove(candidate,snapshot,moved,edV(.01*(repeat%7),.02*(repeat%5),0))&&commit();
            edCopyModel(candidate,snapshot);
            ok=ok&&edExtrude(candidate,0,4,edV(0,.05+.03*(repeat%9),0))&&commit();
            sithSurface_Tick(.02);
            ok=ok&&sithAIAwareness_numEntries==0;
            sithAIAwareness_CreateTransmittingEvent(&sithWorld_g_pCurrentWorld->aSectors[sithWorld_g_pCurrentWorld->numSectors-1],&player->position,0,1,player);
            sithAIAwareness_ProcessEvents();
        }
        edCopyModel(candidate,snapshot);ok=ok&&commit();
        /* Connected selected faces move shared vertices once and extrude as
           one undoable action, retaining selected caps after the rebuild. */
        ok=ok&&historySelfTest();
        ok=ok&&objectSelfTest(player);
        ok=ok&&stairsSelfTest(player);
        ok=ok&&horizontalExtrusionSelfTest();
        ok=ok&&lightSelfTest();
        ok=ok&&episodeSelfTest();
        ok=ok&&cogSelfTest();
        ok=ok&&framesSelfTest();
        ok=ok&&netSelfTest();
        ok=ok&&deleteHandleSelfTest();
        ok=ok&&mapSelfTest(player);
        edCopyModel(candidate,snapshot);EdHit weldedVertex={0,4,0,-1};ok=ok&&edMove(candidate,snapshot,weldedVertex,edV(0,0,1))&&commit()&&sithWorld_g_pCurrentWorld->numVertices==7;
        edCopyModel(candidate,snapshot);ok=ok&&commit();
        edCopyModel(candidate,snapshot);EdHit weldedEdge={0,4,-1,3};ok=ok&&edMove(candidate,snapshot,weldedEdge,edV(0,0,1))&&commit()&&sithWorld_g_pCurrentWorld->numVertices==6;
        edCopyModel(candidate,snapshot);ok=ok&&commit();
        edCopyModel(candidate,model);ok=ok&&edCleaveSector(candidate,0,4,edV(0,1,0),edV(0,1,1))&&commit()&&sithWorld_g_pCurrentWorld->numAdjoinsLoaded==2;
        edCopyModel(candidate,snapshot);ok=ok&&commit();
        edCopyModel(candidate,model);ok=ok&&edCleave(candidate,0,4,edV(0,1,0),edV(0,1,1))&&commit();
        EdSelection group={0};group.count=2;group.hits[0]=EdHit{0,4,-1,-1};group.hits[1]=EdHit{0,6,-1,-1};picks=group;
        ok=ok&&extrudeSelectedFaces(0)&&picks.count==2&&model->count==3;
        ok=ok&&historyStep(0);ok=ok&&picks.count==2&&model->count==1;
        picks.count=0;edCopyModel(candidate,snapshot);ok=ok&&commit();
        ok=ok&&edCleave(candidate,0,4,edV(0,1,.2),edV(0,1,.8))&&commit();
        ok=ok&&sithWorld_g_pCurrentWorld->numSurfaces==7;
        edCopyModel(candidate,snapshot);ok=ok&&edExtrude(candidate,0,4,edV(0,1,0))&&commit();
        ok=ok&&sithWorld_g_pCurrentWorld->numSectors==2&&sithWorld_g_pCurrentWorld->numAdjoinsLoaded==2;
        /* Deleting the occupied sector repairs portals, player collision and spawn records. */
        ok=ok&&deleteSector(player,0)&&model->count==1&&sithWorld_g_pCurrentWorld->numAdjoinsLoaded==0;
        ok=ok&&edContains(&model->c[0],vec(player->position))&&jkPlayer_playerInfos[0].pInSector==player->sector;
        ok=ok&&historyStep(0);nextSpawn=0;ok=ok&&teleportWalkplayer(player)==1;
        ok=ok&&model->count==2&&sithWorld_g_pCurrentWorld->numAdjoinsLoaded==2;
        SithThing *effect=extrudeEffect(edV(0,1,.5),0);
        ok=ok&&effect&&effect->type==SITH_THING_EXPLOSION&&effect->renderData.type==RD_THING_SPRITE3&&effect->pSoundClass;
        if(effect) {
            flex_t health=player->actorParams.health;
            ok=ok&&effect->explosionParams.damage==0&&effect->explosionParams.force==0&&effect->explosionParams.range==0&&!(effect->explosionParams.flags&SITHEXPLOSION_FLAG_HAS_BLAST_PHASE);
            for(int i=0;i<4;i++)ok=ok&&!effect->explosionParams.apDebries[i];
            sithExplosion_MakeBlast(effect);ok=ok&&player->actorParams.health==health;
        }
        for(int i=0;i<sithWorld_g_pCurrentWorld->numAdjoinsLoaded;i++){SithSurfaceAdjoin *a=&sithWorld_g_pCurrentWorld->adjoins[i];ok=ok&&a->mirror&&a->mirror->mirror==a&&a->sector==a->mirror->surface->pSector;}
        edCopyModel(candidate,model);candidate->spawnCount=2;candidate->spawns[1].position=edV(.5,.5,.12);candidate->spawns[1].rotation=edV(0,90,0);
        edShiftFaceUV(&candidate->c[0].f[0],edV(ED_GRID,ED_GRID,0));ok=ok&&commit();
        int extra=-1;for(int i=baseMaterialCount;i<paletteCount;i++)if(editorMaterial(i)){extra=i;break;}
        ok=ok&&extra>=baseMaterialCount;
        if(extra>=0){edCopyModel(candidate,model);candidate->c[0].f[0].material=extra;ok=ok&&commit();}
        edCopyModel(candidate,model);EdLevelHeader *h=&candidate->header;
        snprintf(h->title,sizeof(h->title),"Header integration test");snprintf(h->colormap,sizeof(h->colormap),"01narsh.cmp");snprintf(h->intro[0],160,"Welcome to the test level.");snprintf(h->intro[19],160,"Final intro line.");
        h->gravity=3;h->ceilingSkyZ=18;h->horizonDistance=120;h->horizonPixels=1024;h->horizonOffset[0]=2;h->ceilingOffset[1]=-3;
        candidate->objectCount=1;snprintf(candidate->objects[0].templateName,64,"crate4_1");candidate->objects[0].position=edV(.25,.25,.12);candidate->objects[0].rotation=edV(0,90,0);
        for(int i=0;i<4;i++){h->mip[i]=(i+1)*2;h->lod[i]=(i+1)*.6;}h->perspective=3;h->gouraud=4;ok=ok&&commit();
        edCopyModel(candidate,model);EdFace *attributesFace=&candidate->c[0].f[0];attributesFace->attributes=edSurfaceAttributes(attributesFace);attributesFace->hasAttributes=1;
        attributesFace->attributes.surfaceFlags=0x10005;attributesFace->attributes.faceFlags=1;attributesFace->attributes.uScale=2;attributesFace->attributes.vScale=.5;attributesFace->attributes.extraLight=.25;edMapFace(attributesFace);ok=ok&&commit();
        ok=ok&&writeEditorGob(model,"Episode/sithed_runtime_test.gob","Runtime integration test",0);
        edCopyModel(candidate,snapshot);ok=ok&&commit();
        printf("GEOSITH_SELFTEST %s: import, cleave, live geometry rebuild, extrusion, portals, export, restore\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return;}
static int stressStairs(SithThing *p) {
    static int frame;
    if(!frame){
        stressRunning=1;stressRenderFrames=0;active=1;tool=1;savedPhysics=p->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=p->flags;setEditorFlight(p,1);clearHistory();
        picks.count=0;edSelectionToggle(model,&picks,EdHit{0,4,-1,-1});edSelectionToggle(model,&picks,EdHit{0,2,-1,-1});
        surfaceAttributesDialog();if(model->count!=17||sithWorld_g_pCurrentWorld->numAdjoinsLoaded!=32||undoCount!=1||!historyStep(0)||model->count!=1||!historyStep(1)||model->count!=17)exit(2);
        tool=10;picks.count=0;p->position=rdvec(edV(-.6,-.85,.45));rdVector3 angles=rdvec(edV(0,-18,0));rdMatrix_BuildRotate34(&p->orient,&angles);p->actorParams.headPYR=rdvec(edV(-12,0,0));
        if(p->attach_flags)sithThing_DetachThing(p);sithThing_SetSector(p,&sithWorld_g_pCurrentWorld->aSectors[0],1);sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);
    }
#ifdef SDL2_RENDER
    if(frame==12&&getenv("GEOSITH_STAIRS_SCENE"))std3D_ScreenshotWindow(getenv("GEOSITH_STAIRS_SCENE"));
#endif
    if(frame++>=24){printf("GEOSITH_STRESS PASS: stairs menu, geometry and undo/redo, %d rendered frames\n",stressRenderFrames);fflush(stdout);exit(0);}return 1;
}
static int stressCleanup(SithThing *p){
    static int frame;int saved=getenv("GEOSITH_CLEANUP_SAVED_TEST")!=NULL,sector=saved?50:0,before=saved?51:9,after=saved?10:6;
    if(!frame){stressRunning=1;stressRenderFrames=0;active=1;tool=1;selectionCleanup=0;savedPhysics=p->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=p->flags;p->position=rdvec(saved?edV(-3.4375,4.4,1):edV(0,0,.5));rdVector3 angles={0};rdMatrix_BuildRotate34(&p->orient,&angles);p->actorParams.headPYR=angles;setEditorFlight(p,1);sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);
        if(saved){if(model->count!=53||model->c[sector].count!=before)exit(2);rdVector3 look={0,-1,0};rdMatrix_BuildFromLook34(&p->orient,&look);if(p->attach_flags)sithThing_DetachThing(p);sithThing_SetSector(p,&sithWorld_g_pCurrentWorld->aSectors[sector],1);}
        else{edCopyModel(candidate,model);if(!edCleave(candidate,0,4,edV(0,1,0),edV(0,1,1))||!edCleave(candidate,0,4,edV(-1,1,.5),edV(1,1,.5))||!edCleave(candidate,0,6,edV(-1,1,.5),edV(1,1,.5))||!commit()||model->c[0].count!=before)exit(2);}clearHistory();}
    if(frame==3){selectionMenu();if(!selectionCleanup)exit(2);}
    if(frame==4){selectionWheel(1);if(selectionCleanup)exit(2);selectionWheel(-1);if(!selectionCleanup)exit(2);}
    if(frame==6){EdVec origin,direction;ray(&origin,&direction);edRaySelection(model,origin,direction,&hover);
        if(hover.cell!=sector||!cleanupAimedSector()||model->c[sector].count!=after||sithWorld_g_pCurrentWorld->aSectors[sector].numSurfaces!=after||undoCount!=1||!historyStep(0)||model->c[sector].count!=before||!historyStep(1)||model->c[sector].count!=after)exit(2);}
#ifdef SDL2_RENDER
    if(frame==2&&getenv("GEOSITH_CLEANUP_BEFORE_CAPTURE"))std3D_ScreenshotWindow(getenv("GEOSITH_CLEANUP_BEFORE_CAPTURE"));
    if(frame==12&&getenv("GEOSITH_CLEANUP_SCENE_CAPTURE"))std3D_ScreenshotWindow(getenv("GEOSITH_CLEANUP_SCENE_CAPTURE"));
#endif
    if(frame++>=20){printf("GEOSITH_STRESS PASS: selection tools menu, sector cleanup, live faces and undo/redo, %d rendered frames\n",stressRenderFrames);fflush(stdout);exit(0);}return 1;
}
static int stressLights(SithThing *p) {
    static int frame;
    if(!frame){
        stressRunning=1;stressRenderFrames=0;active=1;tool=4;clearHistory();savedPhysics=p->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=p->flags;setEditorFlight(p,1);
        p->position=rdvec(edV(0,-.75,.35));rdVector3 angles={0};rdMatrix_BuildRotate34(&p->orient,&angles);p->actorParams.headPYR=angles;
        if(p->attach_flags)sithThing_DetachThing(p);sithThing_SetSector(p,&sithWorld_g_pCurrentWorld->aSectors[0],1);sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);
        EdVec origin,direction;ray(&origin,&direction);edRay(model,origin,direction,&hover);fireDown=grabDown=0;lightControl(origin,direction,1,0,0,0);lightControl(origin,direction,0,0,0,0);
        if(model->lightCount!=1||!lightsWorldCheck())exit(2);
    }
    EdVec origin,direction;ray(&origin,&direction);edRay(model,origin,direction,&hover);
    if(frame==8){lightControl(origin,direction,0,0,0,1);lightControl(origin,direction,0,0,1,1);}
    if(frame==12)lightControl(origin,direction,0,1,0,0);
    if(frame==13){p->position.x+=ED_GRID;ray(&origin,&direction);lightControl(origin,direction,0,1,0,0);}
    if(frame==14)lightControl(origin,direction,0,0,0,0);
    if(frame==20){rdVector3 look={.5,1,.1};rdVector_Normalize3Acc(&look);rdMatrix_BuildFromLook34(&p->orient,&look);ray(&origin,&direction);edRay(model,origin,direction,&hover);}
    if(frame>=20)lightControl(origin,direction,0,0,0,frame==21?1:0);
#ifdef SDL2_RENDER
    if(frame==24)std3D_ScreenshotWindow(getenv("GEOSITH_LIGHTS_CAPTURE"));
#endif
    if(frame++>=32){int ok=lightsWorldCheck()&&model->lightCount==1&&fabs(model->c[0].extraLight-.55)<.00001&&fabs(model->lights[0].intensity-1.1)<.001&&fabs(model->lights[0].range-(4+ED_GRID))<.001&&fabs(model->lights[0].position.x-ED_GRID)<.001;
        printf("GEOSITH_STRESS %s: light placement, wheel controls, dragging and vertex lighting, %d rendered frames\n",ok?"PASS":"FAIL",stressRenderFrames);fflush(stdout);exit(ok?0:2);}
    return 1;
}
static int stressEpisode(SithThing *p){
    static int frame;
    if(!frame){stressRunning=1;stressRenderFrames=0;active=1;savedPhysics=p->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=p->flags;clearHistory();
        int previousModal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
        if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
        stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
        jkGuiEsc_Show();jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=previousModal;stdControl_SetActivation(1);stdControl_Flush();
        int ok=model->episode.type==3&&!strcmp(model->episode.title,"Episode editor test")&&model->episode.rows[0].line==10&&!strcmp(model->episode.rows[0].file,"episode_test.jkl")&&!strcmp(model->header.title,"Episode Level Name")&&!strcmp(model->header.intro[0],"Episode loading text")&&!strcmp(model->header.intro[19],"Final episode intro line")&&undoCount==1;
        ok=ok&&historyStep(0)&&!strcmp(model->episode.rows[0].file,"sithed.jkl")&&historyStep(1)&&!strcmp(model->episode.rows[0].file,"episode_test.jkl");if(!ok)exit(2);
    }
    if(frame++>=24){printf("GEOSITH_STRESS PASS: pause Episode Editor, sequence entry, loading strings and undo/redo, %d rendered frames\n",stressRenderFrames);fflush(stdout);exit(0);}return 1;
}
static int stressWalkingDrag(SithThing *p) {
    static int frame,grounded;static EdHit cap;static EdVec start,delta;static EdDragTrace trace;
    if(!frame) {
        stressRunning=1;stressRenderFrames=0;active=1;tool=1;
        savedPhysics=p->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=p->flags;setEditorFlight(p,0);
        edDefault(candidate);if(!edExtrudeStep(candidate,0,4)||!commit())exit(2);edCopyModel(snapshot,model);
        if(p->attach_flags)sithThing_DetachThing(p);
        p->position=rdvec(edV(0,.7,.125));rdVector3 angles={0};rdMatrix_BuildRotate34(&p->orient,&angles);p->actorParams.headPYR=angles;
        sithThing_SetSector(p,&sithWorld_g_pCurrentWorld->aSectors[0],1);sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);
        EdVec origin,direction;ray(&origin,&direction);start=origin;
        if(!edRay(model,origin,direction,&cap)||cap.cell!=1)exit(2);
        edDragTraceBegin(&trace,origin,direction,cap.point,edNormal(&model->c[cap.cell].f[cap.face]),4);
        delta=edV(0,0,0);
    }
    if(p->attach_flags&SITH_ATTACH_SURFACE)grounded++;
    EdVec origin,direction;ray(&origin,&direction);
    delta=edAdd(delta,edDragTraceStep(&trace,origin,direction));
    EdVec movement=edSurfaceDragDelta(delta,edNormal(&snapshot->c[cap.cell].f[cap.face]));
    int wasGrounded=(p->attach_flags&SITH_ATTACH_SURFACE)!=0;
    rdVector3 velocity=p->physicsParams.vel;
    if(!trace.valid||fabs(delta.y-(origin.y-start.y))>.00001||!edMove(candidate,snapshot,cap,movement)||!commit())exit(2);
    if(wasGrounded&&(!(p->attach_flags&SITH_ATTACH_SURFACE)||p->attachedSurface->pSector!=p->sector||
       p->physicsParams.vel.x!=velocity.x||p->physicsParams.vel.y!=velocity.y||p->physicsParams.vel.z!=velocity.z)) {
        printf("GEOSITH_WALK_DRAG FAIL: rebuild interrupted floor contact\n");fflush(stdout);exit(2);
    }
    rdVector_Zero3(&p->physicsParams.acceleration);p->physicsParams.vel.x=0;p->physicsParams.vel.y=1;
    if(frame++>=180) {
        int ok=grounded>10&&origin.y>1.5&&!(p->physicsParams.flags&SITH_PF_FLY)&&!(g_debugmodeFlags&DEBUGFLAG_NOCLIP);
        printf("GEOSITH_WALK_DRAG %s: travel %.3f, %d grounded frames, %d rebuilds\n",ok?"PASS":"FAIL",origin.y-start.y,grounded,frame);
        printf("GEOSITH_STRESS %s: grounded surface drag, %d rendered frames\n",ok?"PASS":"FAIL",stressRenderFrames);fflush(stdout);exit(ok?0:2);
    }
    return 1;
}
static int stressShapes(SithThing *p){
    static int frame;if(!frame){stressRunning=1;stressRenderFrames=0;active=1;tool=10;cleaveShape=32;shapeRadius=.25;savedPhysics=p->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=p->flags;p->flags|=SITH_TF_INVULN;p->position.z=.5;p->physicsParams.flags=(p->physicsParams.flags&~SITH_PF_USEGRAVITY)|SITH_PF_FLY;sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);}
    if(!frame&&getenv("GEOSITH_CLIPPED_SHAPE_TEST")){shapeRadius=.5;rdVector3 look={0,1,-.52};rdVector_Normalize3Acc(&look);rdMatrix_BuildFromLook34(&p->orient,&look);rdVector_Zero3(&p->actorParams.headPYR);}
    rdVector_Zero3(&p->physicsParams.vel);rdVector_Zero3(&p->physicsParams.acceleration);
    if(frame==8&&getenv("GEOSITH_SHAPE_PREVIEW_CAPTURE"))std3D_ScreenshotWindow(getenv("GEOSITH_SHAPE_PREVIEW_CAPTURE"));
    if(frame==12){shapeMenu();if(cleaveShape!=32)exit(2);}
    if(frame==16){
        char previous[32];snprintf(previous,32,"%s",jkRes_episodeGobName);int oldModal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
        if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
        jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
        int ok=jkEditor_Create(atoi(getenv("GEOSITH_SHAPES_TEST")));char name[32];ok=ok&&editorLevelFilename(levelName,name)&&!strcmp(name,getenv("GEOSITH_NAME_TEST_PREFIX"))&&!strcmp(name,jkRes_episodeGobName);
        snprintf(jkRes_episodeGobName,32,"%s",previous);jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=oldModal;stdControl_SetActivation(1);stdControl_Flush();if(!ok)exit(2);
        printf("GEOSITH_NAMING PASS: named episode created and Save Level prefill retained\n");fflush(stdout);
    }
    if(frame==18&&getenv("GEOSITH_VERTEX_LIMIT_CAPTURE")){edCopyModel(candidate,model);if(edCleaveShape(candidate,0,4,edV(0,1,.5),shapeRadius,32)||!edVertexLimitHit()||!edModelsEqual(candidate,model))exit(2);}
    if(frame==20){edCopyModel(candidate,model);edClearGeometryError();if(edCleaveShape(candidate,0,4,edV(0,1,.5),shapeRadius,32)||!edVertexLimitHit()||!edModelsEqual(candidate,model))exit(2);edClearGeometryError();int clipped=getenv("GEOSITH_CLIPPED_SHAPE_TEST")!=NULL;cleaveShape=clipped?32:24;remember();if(!edCleaveShape(candidate,0,4,edV(0,1,clipped?0:.5),shapeRadius,cleaveShape)||!commit()||!historyStep(0)||!historyStep(1)||model->c[0].f[4].count!=(clipped?18:24))exit(2);}
    if(frame==26&&getenv("GEOSITH_CLIPPED_SHAPE_TEST")){remember();edCopyModel(candidate,model);if(!edExtrude(candidate,0,4,edV(0,.5,0))||!commit()||model->count!=2)exit(2);rdVector3 look={0,1,-.35};rdVector_Normalize3Acc(&look);rdMatrix_BuildFromLook34(&p->orient,&look);}
    if(frame==32&&getenv("GEOSITH_SHAPE_SCENE_CAPTURE"))std3D_ScreenshotWindow(getenv("GEOSITH_SHAPE_SCENE_CAPTURE"));
    if(frame++>=40){printf("GEOSITH_STRESS PASS: shape browser, named creation, 24-sided cleave, oversized rejection and undo/redo, %d rendered frames\n",stressRenderFrames);fflush(stdout);exit(0);}return 1;
}
static int stressEditor(SithThing *p) {
    static int step;
    if(!getenv("GEOSITH_STRESS")||(!getenv("GEOSITH_NET_ROLE")&&!getenv("GEOSITH_RESOURCE_RELOAD_TEST")&&strcmp(jkRes_episodeGobName,"sithed_smoke")))return 0;
    if(getenv("GEOSITH_TERRAIN_TEST")){
        static int frame;if(!frame){int ok=bindEditorWorld()&&terrainSelfTest(p);printf("GEOSITH_TERRAIN %s: live creation, sculpt strokes, undo, conversion and export\n",ok?"PASS":"FAIL");fflush(stdout);if(!ok||!getenv("GEOSITH_TERRAIN_CAPTURE"))exit(ok?0:2);
            active=stressRunning=1;p->position=rdvec(edV(0,-.8,.65));rdVector3 angles={-30,0,0};rdMatrix_BuildRotate34(&p->orient,&angles);p->actorParams.headPYR=rdVector3{0,0,0};setEditorFlight(p,1);sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);}
#ifdef SDL2_RENDER
        if(frame==20)std3D_ScreenshotWindow(getenv("GEOSITH_TERRAIN_CAPTURE"));
#endif
        if(frame++>=30)exit(0);return 1;
    }
    if(getenv("GEOSITH_SPLINE_TEST")){
        static int frame;if(!frame){if(!bindEditorWorld()||!splineSelfTest(p))exit(2);active=1;stressRunning=1;}
#ifdef SDL2_RENDER
        if(frame==12&&getenv("GEOSITH_SPLINE_MAP_CAPTURE"))std3D_ScreenshotWindow(getenv("GEOSITH_SPLINE_MAP_CAPTURE"));
#endif
        if(frame++>=40){printf("GEOSITH_SPLINE PASS: map gestures, live sectors, point movement, undo/redo and explicit conversion\n");fflush(stdout);exit(0);}return 1;
    }
    if(getenv("GEOSITH_SECTOR_TEST"))return stressSectors(p);
    if(getenv("GEOSITH_BUILDER_CAPTURE"))return stressBuilder(p);
    if(getenv("GEOSITH_RESOURCE_RELOAD_TEST")){
        int material=addMaterialName("zz_net_custom.mat");
        char *script=cogSourceText("zz_net_script.cog");
        int ok=material>=0&&customMaterial("zz_net_custom.mat")&&editorMaterial(material)&&sithModel_Load("zz_net_model.3do",1)&&script&&strstr(script,"GeoSith resource fixture");
        edFree(script);materialMetadata();ok=ok&&materialSection[material]==103;
        printf("GEOSITH_RESOURCE_RELOAD %s: embedded MAT/3DO/COG loaded without source mods\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_WALK_DRAG_TEST"))return stressWalkingDrag(p);
    if(getenv("GEOSITH_SHAPES_TEST"))return stressShapes(p);
    if(getenv("GEOSITH_SECTOR_GRAB_TEST")){
        int ok=bindEditorWorld();if(!ok)exit(2);clearHistory();EdModel before;edCopyModel(&before,model);
        remember();edCopyModel(snapshot,model);grabSectors.clear();grabSectors[0]=1;sectorGrab=groupGrab=grabbing=1;
        EdVec center=edMapCenter(&before.c[0]),delta=edV(.25,.5,.125);
        ok=moveGrabbedSectors(delta)&&sectorPicks[0];delta=edV(.5,.75,.25);
        ok=ok&&moveGrabbedSectors(delta)&&sectorPicks[0];
        EdVec error=edSub(edSub(edMapCenter(&model->c[0]),center),delta);
        ok=ok&&edDot(error,error)<1e-12&&undoCount==1;grabbing=groupGrab=sectorGrab=0;
        ok=ok&&historyStep(0)&&edModelsEqual(model,&before);
        printf("GEOSITH_SECTOR_GRAB %s: repeated 3D translation, retained selection and single-step undo\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_SCALE_TEST")){
        int ok=bindEditorWorld();if(!ok)exit(2);clearHistory();EdModel before;edCopyModel(&before,model);sectorPicks.clear();picks.count=1;picks.hits[0]={0,0,-1,-1};
        selectionScale(1);ok=picks.count==1&&undoCount==1&&!edModelsEqual(model,&before);selectionScale(-1);ok=ok&&picks.count==1&&undoCount==2&&historyStep(0)&&historyStep(0)&&edModelsEqual(model,&before);
        picks.count=0;sectorPicks[0]=1;selectionScale(1);ok=ok&&sectorPicks[0]&&undoCount==1&&historyStep(0)&&edModelsEqual(model,&before);
        printf("GEOSITH_SCALE %s: repeated face scaling, sector scaling, retained selection and undo\n",ok?"PASS":"FAIL");fflush(stdout);exit(ok?0:2);
    }
    if(getenv("GEOSITH_MAP_MIRROR_TEST")){int ok=bindEditorWorld()&&mapMirrorTest(p);exit(ok?0:2);}
    if(getenv("GEOSITH_MAP_VIEWS_TEST")){int ok=bindEditorWorld()&&mapViewsTest(p);exit(ok?0:2);}
    if(getenv("GEOSITH_MAP_CAPTURE"))return stressMap(p);
    if(getenv("GEOSITH_STAIRS_CAPTURE"))return stressStairs(p);
    if(getenv("GEOSITH_LIGHTS_CAPTURE"))return stressLights(p);
    if(getenv("GEOSITH_CLEANUP_MENU_CAPTURE"))return stressCleanup(p);
    if(getenv("GEOSITH_EPISODE_CAPTURE"))return stressEpisode(p);
    if(getenv("GEOSITH_COG_CAPTURE"))return stressCogs(p);
    if(getenv("GEOSITH_NET_ROLE"))return stressNetwork(p);
    if(getenv("GEOSITH_FRAMES_CAPTURE"))return stressFrames(p);
    if(!step) {
        stressRunning=1;stressRenderFrames=0;
        if(fabs(model->header.lod[0]-.3)>.001||fabs(model->header.perspective-2)>.001){printf("GEOSITH_STRESS FAIL: resolution-scaled source header\n");exit(2);}
        savedPhysics=p->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=p->flags;
        active=1;p->flags|=SITH_TF_INVULN;setEditorFlight(p,0);
        edCopyModel(candidate,model);
        if(getenv("GEOSITH_OBJECT_CAPTURE")){candidate->spawnCount=2;candidate->spawns[1]=EdSpawn{edV(.4,.5,.125),edV(0,0,0)};candidate->objectCount=1;snprintf(candidate->objects[0].templateName,64,"crate4_1");candidate->objects[0].position=edV(.1,.6,.2);snprintf(candidate->objects[0].templateName,64,"crate4_3");candidate->objects[0].rotation=edV(15,-50,0);}
        if(!edCleave(candidate,0,4,edV(0,1,0),edV(0,1,1))||!commit())exit(2);edCopyModel(snapshot,model);
        sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);
        sithSurface_SurfaceLightAnim(&sithWorld_g_pCurrentWorld->surfaces[0],.5,1);
        sithSurface_SetSectorLight(&sithWorld_g_pCurrentWorld->aSectors[0],.5,1,0);
    }
    if(step>=240) {printf("GEOSITH_STRESS PASS: 240 live edits with physics, %d rendered frames\n",stressRenderFrames);fflush(stdout);exit(0);}
    edCopyModel(candidate,snapshot);
    if(getenv("GEOSITH_COLORMAP_TEST")&&(step/20)%2)snprintf(candidate->header.colormap,128,"01narsh.cmp");
    if(getenv("GEOSITH_OBJECT_CAPTURE")&&step>=8&&step<120)snprintf(candidate->header.colormap,128,"01narsh.cmp");
    EdHit moved={0,4,-1,-1};
    EdSelection group={0};group.count=2;group.hits[0]=moved;group.hits[1]=EdHit{0,6,-1,-1};
    EdVec movement=edV(.01*(step%13),.02*(step%7),0);
    int ok=1;
    if(step%3==0)ok=edMoveSelection(candidate,snapshot,&group,movement);
    ok=ok&&commit();
    if(ok)edSelectionRemap(snapshot,model,&group,step%3==0?edSelectionDelta(snapshot,&group,movement):edV(0,0,0),&picks);
    if(ok&&step%3==1)ok=extrudeSelectedFaces(0);
    if(!ok){printf("GEOSITH_STRESS FAIL: edit %d\n",step);fflush(stdout);exit(2);}
    rdVector_Zero3(&p->physicsParams.acceleration);rdVector_Zero3(&p->physicsParams.vel);
    rdVector_Zero3(&p->physicsParams.angularVelocity);
    p->actorParams.headPYR.x=(step%41)-20;
    p->actorParams.headPYR.y=(step%121)-60;
    selection=moved;grabbing=groupGrab=0;tool=step%24<12?1:2;
    if(step==16&&getenv("GEOSITH_SURFACE_CAPTURE")){
        if(getenv("GEOSITH_ADJOIN_FLAGS_CAPTURE")){picks.count=0;for(int s=0;s<model->count&&picks.count<2;s++)for(int f=0;f<model->c[s].count&&picks.count<2;f++)if(model->c[s].f[f].neighbor>=0)picks.hits[picks.count++]=EdHit{s,f,-1,-1};}
        if(picks.count<2)exit(2);
        edCopyModel(candidate,model);EdHit a=picks.hits[0];candidate->c[a.cell].f[a.face].attributes=edSurfaceAttributes(&candidate->c[a.cell].f[a.face]);candidate->c[a.cell].f[a.face].hasAttributes=1;candidate->c[a.cell].f[a.face].attributes.surfaceFlags|=0x80000000u;if(!commit())exit(2);
        surfaceAttributesDialog();
        for(int i=0;i<picks.count;i++){EdHit h=picks.hits[i];EdSurfaceAttributes attrs=edSurfaceAttributes(&model->c[h.cell].f[h.face]);
            if(attrs.uScale!=2||attrs.extraLight!=.25||!(attrs.surfaceFlags&0x10000)||!(attrs.faceFlags&1)||((attrs.surfaceFlags>>31)!=(!i))||(getenv("GEOSITH_ADJOIN_FLAGS_CAPTURE")&&attrs.adjoinFlags!=5))exit(2);
        }
        if(!historyStep(0)||!historyStep(1))exit(2);
        printf("GEOSITH_SURFACE_MENU PASS: multi-edit, mixed flags preserved, scales, extra light, undo/redo\n");fflush(stdout);
    }
    if(step==16&&getenv("GEOSITH_NETWORK_DETAILS_CAPTURE")){
        int oldServer=sithNet_isServer,oldModal=jkMain_bInModalGui;sithNet_isServer=1;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
        if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
        jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);jkGuiEsc_Show();jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();stdControl_SetActivation(1);stdControl_Flush();jkMain_bInModalGui=oldModal;sithNet_isServer=oldServer;
    }
    if(step==16&&getenv("GEOSITH_MATERIAL_CAPTURE")){currentMaterial=baseMaterialCount;materialMenu();}
    if(step==16&&getenv("GEOSITH_OBJECT_CAPTURE")){browserObjects=1;materialMenu();}
    if(getenv("GEOSITH_OBJECT_CAPTURE"))tool=3;
#ifdef SDL2_RENDER
    if(step==24&&getenv("GEOSITH_HUD_CAPTURE"))std3D_ScreenshotWindow(getenv("GEOSITH_HUD_CAPTURE"));
#endif
#ifdef SDL2_RENDER
    if(getenv("GEOSITH_SCREENSHOT_TEST")){
        static char testShot[128];
        if(step==24){
            stdControlKeyInfo *binding=&sithControl_aInputFuncToKeyinfo[INPUT_FUNC_SCREENSHOT];if(!binding->numEntries)exit(2);
            int key=binding->aEntries[0].dxKeyNum;stdControl_UpdateKeyState(key,1,stdPlatform_GetTimeMsec());jkControl_HandleScreenshot();stdControl_UpdateKeyState(key,0,stdPlatform_GetTimeMsec());
            if(!jkGame_bScreenshotMessage)exit(2);snprintf(testShot,sizeof(testShot),"SHOT%04d.PNG",Video_dword_5528B0-1);
        }
        if(step==25){FILE *shot=fopen(testShot,"rb");unsigned char signature[8];int ok=shot&&fread(signature,1,8,shot)==8&&!edCompare(signature,"\x89PNG\r\n\x1a\n",8);if(shot)fclose(shot);
            if(!ok||jkGame_bScreenshotMessage)exit(2);remove(testShot);printf("GEOSITH_SCREENSHOT PASS: bound key captured the completed editor frame\n");fflush(stdout);}
    }
#endif
    if(!(step%32)){printf("GEOSITH_STRESS step %d\n",step);fflush(stdout);}
    step++;return 1;
}
static int pickEditorObject(EdVec origin,EdVec direction,double wallDistance) {
    int found=-10000;double nearest=wallDistance;
    for(int i=0;i<model->spawnCount+model->objectCount;i++){
        int spawn=i<model->spawnCount;if(!spawn&&edTerrainObject(model,i-model->spawnCount)>=0)continue;EdVec position=spawn?model->spawns[i].position:model->objects[i-model->spawnCount].position;
        double radius=.08;if(!spawn){SithThing *definition=sithTemplate_GetTemplate(model->objects[i-model->spawnCount].templateName);if(definition)radius=fmax(.04,definition->collideSize);}
        EdVec offset=edSub(position,origin);double distance=edDot(offset,direction);if(distance<0||distance>nearest+radius)continue;
        EdVec perpendicular=edSub(offset,edScale(direction,distance));if(edDot(perpendicular,perpendicular)>radius*radius)continue;
        found=spawn?-i-1:i-model->spawnCount;nearest=distance;
    }return found;
}
static EdVec objectFacingHandle(const EdModel *source,int id) {
    EdFrame pose=thingPose(source,id);EdVec center=pose.position,rotation=pose.rotation;
    SithThing *definition=sithTemplate_GetTemplate(thingTemplate(source,id));
    double radius=definition?definition->collideSize:0;
    if(definition&&definition->renderData.type==RD_THING_MODEL3&&definition->renderData.model3)radius=fmax(radius,definition->renderData.model3->radius);
    double distance=fmax(.22,radius*1.25+.05);rdVector3 angles=rdvec(rotation);rdMatrix34 orientation;rdMatrix_BuildRotate34(&orientation,&angles);
    return edAdd(center,edScale(vec(orientation.lvec),distance));
}
static std::vector<int> overlaySectors;
static std::map<int,int> overlayThingVisibility;
static void captureOverlayVisibility(void){
    overlaySectors.clear();overlayThingVisibility.clear();
    SithWorld *world=sithWorld_g_pCurrentWorld;if(!world)return;
    /* renderTick also marks hidden sectors visited for light collection.
       Capture the portal-visible list before POV/editor rendering changes it. */
    for(int i=0;i<sithRender_g_numVisibleSectors;i++){
        int sector=(int)(sithRender_aVisibleSectors[i]-world->aSectors);
        if(sector>=0&&sector<model->count)overlaySectors.push_back(sector);
    }

}
static int objectOverlayVisible(EdVec position){
    /* Check every visible sector: a point on an adjoin belongs to both sides,
       and the first containing sector need not be the visible one. */
    for(int sector:overlaySectors)if(sector<model->count&&edContains(&model->c[sector],position))return 1;
    return 0;
}
// Intersect an actual model face with a visible convex sector. Bounds alone
// would incorrectly expose a coffin's handle through a nearby solid wall.
static int overlayFaceTouchesSector(std::vector<EdVec> polygon,const EdCell& cell){
    std::vector<EdVec> clipped;
    for(int f=0;f<cell.count&&!polygon.empty();f++){
        const EdFace& face=cell.f[f];EdVec normal=edNormal(&face);clipped.clear();
        double tolerance=fmax(ED_EPS,cell.tolerance);
        for(size_t j=0;j<polygon.size();j++){
            EdVec a=polygon[j],b=polygon[(j+1)%polygon.size()];
            double da=edDot(normal,edSub(a,face.v[0].p))+tolerance,db=edDot(normal,edSub(b,face.v[0].p))+tolerance;
            if(da>=0)clipped.push_back(a);
            if((da>=0)!=(db>=0))clipped.push_back(edAdd(a,edScale(edSub(b,a),da/(da-db))));
        }
        polygon.swap(clipped);
    }
    return !polygon.empty();
}
static int thingOverlayVisible(int id){
    auto cached=overlayThingVisibility.find(id);if(cached!=overlayThingVisibility.end())return cached->second;
    EdFrame pose=thingPose(model,id);int& visible=overlayThingVisibility[id];
    if(objectOverlayVisible(pose.position))return visible=1;
    SithThing *definition=sithTemplate_GetTemplate(thingTemplate(model,id));
    if(!definition||definition->renderData.type!=RD_THING_MODEL3||!definition->renderData.model3)return 0;
    rdModel3 *mesh=definition->renderData.model3;if(!mesh->numGeos)return 0;
    std::vector<int> nearby;
    for(int sector:overlaySectors){
        const EdCell& cell=model->c[sector];int outside=0;
        for(int f=0;f<cell.count;f++)if(edDot(edNormal(&cell.f[f]),edSub(pose.position,cell.f[f].v[0].p)) < -mesh->radius-fmax(ED_EPS,cell.tolerance)){outside=1;break;}
        if(!outside)nearby.push_back(sector);
    }
    if(nearby.empty())return 0;
    // Use the hierarchy transforms, including mesh offsets, for elevators whose
    // origin lies below their visible platform. Frames use their own pose.
    rdThing preview={0};rdThing_NewEntry(&preview,NULL);
    if(!rdThing_SetModel3(&preview,mesh)){rdThing_FreeEntry(&preview);return 0;}
    rdMatrix34 placement;rdVector3 angles=rdvec(pose.rotation);rdMatrix_BuildRotate34(&placement,&angles);placement.scale=rdvec(pose.position);preview.rdFrameNum=rdroid_frameTrue-1;
    for(int n=0;n<mesh->numHNodes&&!visible;n++){
        int index=mesh->aHierarchyNodes[n].meshIdx;if(index<0||index>=(int)mesh->aGeos[0].numMeshes)continue;
        rdMatrix34 transform;if(!rdModel3_GetMeshMatrix(&preview,&placement,n,&transform))continue;
        rdMesh *part=&mesh->aGeos[0].aMeshes[index];std::vector<EdVec> points(part->numVertices);
        for(int v=0;v<part->numVertices;v++){rdVector3 point;rdMatrix_TransformPoint34(&point,&part->aVertices[v],&transform);points[v]=vec(point);}
        for(int f=0;f<part->numFaces&&!visible;f++){
            rdFace *face=&part->faces[f];std::vector<EdVec> polygon;
            for(unsigned int v=0;v<face->numVertices;v++)polygon.push_back(points[face->vertexPosIdx[v]]);
            for(int sector:nearby)if(overlayFaceTouchesSector(polygon,model->c[sector])){visible=1;break;}
        }
    }
    rdThing_FreeEntry(&preview);return visible;
}
static int overlayVisibilityTest(void){
    SithWorld *world=sithWorld_g_pCurrentWorld;EdVec p=model->spawns[0].position;int sector=objectSector(model,p);if(sector<0)return 0;
    sithRender_g_numVisibleSectors=0;world->aSectors[sector].renderTick=sithRender_lastRenderTick;
    captureOverlayVisibility();int ok=!objectOverlayVisible(p)&&!thingOverlayVisible(-1);
    sithRender_aVisibleSectors[0]=&world->aSectors[sector];sithRender_g_numVisibleSectors=1;
    captureOverlayVisibility();ok=ok&&objectOverlayVisible(p)&&thingOverlayVisible(-1);
    // Boundary points remain visible even when another containing sector is hidden.
    EdCell original=model->c[sector];model->c[model->count]=original;int other=model->count++;
    overlaySectors.assign(1,other);overlayThingVisibility.clear();ok=ok&&objectOverlayVisible(original.f[0].v[0].p);
    model->count--;overlaySectors.assign(1,sector);
    EdThingAttributes *a=&model->spawns[0].attributes;a->frameCount=2;a->frames[0].position=p;a->frames[1].position=edV(10000,10000,10000);
    ok=ok&&thingOverlayVisible(frameId(-1,0))&&!thingOverlayVisible(frameId(-1,1))&&!objectOverlayVisible(a->frames[1].position);
    // A platform face crosses the room while all its corners and origin are
    // outside; clipping must still retain it, but reject a distant face.
    std::vector<EdVec> platform;
    for(int v=0;v<original.f[0].count;v++)platform.push_back(original.f[0].v[v].p);
    ok=ok&&overlayFaceTouchesSector(platform,original);
    for(EdVec& v:platform)v=edAdd(v,edV(10000,10000,10000));
    ok=ok&&!overlayFaceTouchesSector(platform,original);
    // Real model and hierarchy transforms: its origin is beyond a wall while
    // part of the crate (or a frame of it) still occupies the visible room.
    SithThing *definition=sithTemplate_GetTemplate("crate4_1");
    if(!definition||definition->renderData.type!=RD_THING_MODEL3)return 0;
    int wall=0;for(int f=0;f<original.count;f++)if(fabs(edNormal(&original.f[f]).z)<.1){wall=f;break;}
    const EdFace& face=original.f[wall];EdVec center=edV(0,0,0);
    for(int v=0;v<face.count;v++)center=edAdd(center,edScale(face.v[v].p,1.0/face.count));
    model->objectCount=1;EdObject& object=model->objects[0];object=EdObject{};snprintf(object.templateName,sizeof(object.templateName),"crate4_1");
    object.position=edSub(center,edScale(edNormal(&face),definition->renderData.model3->radius*.1));
    overlayThingVisibility.clear();ok=ok&&!objectOverlayVisible(object.position)&&thingOverlayVisible(0);
    object.attributes.frameCount=2;object.attributes.frames[0].position=object.position;object.attributes.frames[1].position=edV(10000,10000,10000);
    ok=ok&&thingOverlayVisible(frameId(0,0))&&!thingOverlayVisible(frameId(0,1));
    object.position=object.attributes.frames[1].position;overlayThingVisibility.clear();ok=ok&&!thingOverlayVisible(0);
    return ok;
}
static int pickObjectFacingHandle(EdVec origin,EdVec direction,double wallDistance) {
    int found=-10000;double nearest=wallDistance;
    for(int i=0;i<model->spawnCount+model->objectCount;i++){
        int id=i<model->spawnCount?-i-1:i-model->spawnCount;if(id>=0&&edTerrainObject(model,id)>=0)continue;
        if(editorViewReady&&!thingOverlayVisible(id))continue;
        EdVec offset=edSub(objectFacingHandle(model,id),origin);double distance=edDot(offset,direction);
        double radius=fmax(.012,fmin(.06,distance*.012));if(distance<.01||distance>nearest+radius)continue;
        EdVec perpendicular=edSub(offset,edScale(direction,distance));if(edDot(perpendicular,perpendicular)>radius*radius)continue;
        nearest=distance;found=id;
    }
    return found;
}
static EdVec objectFacingRotation(EdVec rotation,EdVec facing,int freeRotation) {
    double distance=sqrt(edDot(facing,facing));if(distance<.01)return rotation;
    if(!freeRotation){if(hypot(facing.x,facing.y)>.001)rotation.y=round(atan2(-facing.x,facing.y)*180.0/3.141592653589793/5)*5;return rotation;}
    rdVector3 angles=rdvec(rotation);rdMatrix34 matrix;rdMatrix_BuildRotate34(&matrix,&angles);
    EdVec from=vec(matrix.lvec),to=edScale(facing,1/distance),axis=edCross(from,to);double sine=sqrt(edDot(axis,axis)),cosine=fmax(-1,fmin(1,edDot(from,to)));
    if(sine>.000001)axis=edScale(axis,1/sine);else if(cosine<0)axis=vec(matrix.uvec);else return edV(round(rotation.x/5)*5,round(rotation.y/5)*5,round(rotation.z/5)*5);
    rdVector3 *basis[3]={&matrix.rvec,&matrix.lvec,&matrix.uvec};
    for(int i=0;i<3;i++){EdVec v=vec(*basis[i]);*basis[i]=rdvec(edAdd(edAdd(edScale(v,cosine),edScale(edCross(axis,v),sine)),edScale(axis,edDot(axis,v)*(1-cosine))));}
    rdMatrix_ExtractAngles34(&matrix,&angles);return edV(round(angles.x/5)*5,round(angles.y/5)*5,round(angles.z/5)*5);
}
static int objectPlacement(SithThing *definition,EdVec direction,EdVec *position) {
    if(hover.cell<0||hover.cell>=model->count)return 0;
    EdCell *cell=&model->c[hover.cell];int surface=-1;double facing=0;
    /* The handle picker can report a neighboring face at an edge. Recover the
     * actual collision plane rather than placing relative to that handle. */
    for(int i=0;i<cell->count;i++){
        EdFace *face=&cell->f[i];EdVec n=edNormal(face);double score=edDot(n,direction);
        if(fabs(edDot(n,edSub(hover.point,face->v[0].p)))<.0001&&score<facing){surface=i;facing=score;}
    }
    if(surface<0)return 0;
    EdFace *face=&cell->f[surface];EdVec normal=edNormal(face);
    rdModel3 *mesh=definition&&definition->renderData.type==RD_THING_MODEL3?definition->renderData.model3:NULL;
    rdMatrix34 *matrices=previewMatrices(mesh);EdBuffer<double> clearance;
    for(int f=0;f<cell->count;f++){
        EdVec n=edNormal(&cell->f[f]);double support=mesh?fmax(mesh->radius,definition->collideSize):0;
        if(matrices){double minimum=1e30;
            for(unsigned int m=0;m<mesh->aGeos[0].numMeshes;m++)for(int v=0;v<mesh->aGeos[0].aMeshes[m].numVertices;v++){
                rdVector3 p;rdMatrix_TransformPoint34(&p,&mesh->aGeos[0].aMeshes[m].aVertices[v],&matrices[m]);minimum=fmin(minimum,edDot(n,edV(p.x,p.y,p.z)));
            }
            if(minimum<1e29)support=fmax(0,-minimum);
        }
        clearance[f]=support+.002;
    }
    edFree(matrices);
    /* Snap along the wall, then restore model clearance. Test the other solid
     * walls too, so aiming at a corner cannot embed the far side of the model. */
    *position=edSnapPoint(hover.point);double distance=edDot(normal,edSub(*position,face->v[0].p));
    *position=edAdd(*position,edScale(normal,clearance[surface]-distance));
    for(int pass=0;pass<16;pass++){
        int adjusted=0;for(int f=0;f<cell->count;f++)if(cell->f[f].neighbor<0){
            EdVec n=edNormal(&cell->f[f]);double deficit=clearance[f]-edDot(n,edSub(*position,cell->f[f].v[0].p));
            if(deficit>ED_EPS){*position=edAdd(*position,edScale(n,deficit));adjusted=1;}
        }
        if(!adjusted)return objectSector(model,*position)>=0;
    }
    /* Clearance is a convenience, never a placement requirement. Keep the
       origin in the aimed sector even when the mesh crosses its boundaries. */
    EdVec center=edV(0,0,0);int vertices=0;for(int f=0;f<cell->count;f++)for(int v=0;v<cell->f[f].count;v++){center=edAdd(center,cell->f[f].v[v].p);vertices++;}
    center=edScale(center,1.0/vertices);*position=edAdd(hover.point,edScale(edSub(center,hover.point),.001));
    if(!edContains(cell,*position))*position=center;return objectSector(model,*position)>=0;
}
static int deleteEditorObject(int id) {
    if(id==-10000)return 0;
    edCopyModel(candidate,model);
    if(id<0){int spawn=-id-1;if(spawn>=candidate->spawnCount)return 0;
        if(candidate->spawnCount<=1){say("Keep at least one walkplayer.");return 0;}
        memmove(&candidate->spawns[spawn],&candidate->spawns[spawn+1],(candidate->spawnCount-spawn-1)*sizeof(EdSpawn));candidate->spawnCount--;candidate->spawn=candidate->spawns[0].position;
    }else{if(id>=candidate->objectCount)return 0;
        for(int shift=id;shift+1<candidate->objectCount;shift++)candidate->objects[shift]=std::move(candidate->objects[shift+1]);candidate->objectCount--;
    }
    edCogThingChanged(candidate,id<0?-id-1:model->spawnCount+id,0);remember();if(!commit())return 0;objectHovered=objectGrabbed=objectHandleHovered=-10000;grabbing=objectGrabbing=objectRotating=0;say("Object deleted. Ctrl+Z: undo.");return 1;
}
static int objectControl(SithThing *player,EdVec origin,EdVec direction,int fire,int grab,int middlePressed,int freeRotation) {
    if(!grabbing)objectGrabbing=objectRotating=0;
    int wheel=stdControl_aAxisStates[AXIS_MOUSE_Z];if(wheel&&templateCount)currentTemplate=(currentTemplate+wheel%templateCount+templateCount)%templateCount;
    if(middlePressed){grabbing=objectGrabbing=cleaving=0;browserObjects=1;materialMenu();middleDown=1;fireDown=grabDown=1;return 1;}
    /* Rotation gizmos can extend beyond the room; walls must not block picking them. */
    objectHandleHovered=pickObjectFacingHandle(origin,direction,1000);
    int body=pickEditorObject(origin,direction,hover.cell>=0?hover.distance:1000);
    int frameBody=pickEditorFrame(origin,direction,hover.cell>=0?hover.distance:1000,0),frameHandle=pickEditorFrame(origin,direction,1000,1);
    if(frameBody!=-10000&&(body==-10000||edDot(edSub(thingPose(model,frameBody).position,origin),direction)<=edDot(edSub(thingPose(model,body).position,origin),direction)+ED_EPS))body=frameBody;
    if(frameHandle!=-10000&&(objectHandleHovered==-10000||edDot(edSub(objectFacingHandle(model,frameHandle),origin),direction)<edDot(edSub(objectFacingHandle(model,objectHandleHovered),origin),direction)-ED_EPS))objectHandleHovered=frameHandle;
    if(body!=-10000&&objectHandleHovered!=-10000){EdVec center=thingPose(model,body).position;
        if(edDot(edSub(center,origin),direction)<edDot(edSub(objectFacingHandle(model,objectHandleHovered),origin),direction))objectHandleHovered=-10000;
    }
    objectHovered=objectHandleHovered!=-10000?objectHandleHovered:body;
    if(grab&&!grabDown&&objectHovered!=-10000){remember();edCopyModel(snapshot,model);objectGrabbed=objectHovered;objectGrabbing=grabbing=1;objectRotating=objectHandleHovered!=-10000;
        grabPlanePoint=thingPose(model,objectGrabbed).position;grabPlaneNormal=direction;
        objectRotationCenter=grabPlanePoint;if(objectRotating)grabPlanePoint=objectFacingHandle(snapshot,objectGrabbed);
        grabTraceValid=edTracePlane(origin,direction,grabPlanePoint,grabPlaneNormal,&grabLastPoint);grabLastOrigin=origin;grabDelta=edV(0,0,0);
    }
    if(objectGrabbing&&grab){EdVec hit;int valid=edTracePlane(origin,direction,grabPlanePoint,grabPlaneNormal,&hit);
        if(valid&&grabTraceValid)grabDelta=edAdd(grabDelta,edAdd(edSub(hit,grabLastPoint),edScale(grabPlaneNormal,edDot(edSub(origin,grabLastOrigin),grabPlaneNormal))));
        if(valid)grabLastPoint=hit;grabLastOrigin=origin;grabTraceValid=valid;
        if(objectRotating){EdVec facing=edSub(edAdd(grabPlanePoint,grabDelta),objectRotationCenter);double distance=sqrt(edDot(facing,facing));
            if(distance>.01){edCopyModel(candidate,snapshot);EdFrame pose=thingPose(candidate,objectGrabbed);pose.rotation=objectFacingRotation(pose.rotation,facing,freeRotation);setThingPose(candidate,objectGrabbed,pose);commit();}

        }else{/* Thing dragging uses half the geometry grid, including walkplayers and frames. */
            EdVec position=edScale(edSnapPoint(edScale(edAdd(grabPlanePoint,grabDelta),2)),.5);
            if(objectGrabbed>=ED_OBJECTS||objectSector(snapshot,position)>=0){edCopyModel(candidate,snapshot);EdFrame pose=thingPose(candidate,objectGrabbed);pose.position=position;setThingPose(candidate,objectGrabbed,pose);commit();}
        }
    }
    if(!grab&&grabDown)objectGrabbing=grabbing=objectRotating=0;
    if(fire&&!fireDown&&!objectGrabbing&&hover.cell>=0&&templateCount){
        const char *name=templateNames[currentTemplate];int walkplayer=!__strcmpi(name,"walkplayer");
        if((walkplayer&&model->spawnCount>=ED_SPAWNS)||(!walkplayer&&model->objectCount>=ED_OBJECTS))say("Object limit reached.");
        else {EdVec position;
            if(objectPlacement(sithTemplate_GetTemplate(name),direction,&position)){remember();edCopyModel(candidate,model);if(walkplayer){edCogThingChanged(candidate,candidate->spawnCount,1);candidate->spawns[candidate->spawnCount++]=EdSpawn{position,edV(0,0,0)};}
                else{EdObject *object=&candidate->objects[candidate->objectCount++];edZero(object,0,sizeof(*object));snprintf(object->templateName,sizeof(object->templateName),"%s",name);object->position=position;}
                if(commit())say(walkplayer?"Walkplayer placed.":"Object placed.");
            }
        }
    }
    fireDown=fire;grabDown=grab;return 1;
}
static int pickEditorLight(EdVec origin,EdVec direction,double wallDistance) {
    double norm=sqrt(edDot(direction,direction));if(norm<ED_EPS)return -1;direction=edScale(direction,1/norm);
    int found=-1;double nearest=wallDistance;
    for(int i=0;i<model->lightCount;i++){
        EdVec offset=edSub(model->lights[i].position,origin);double depth=edDot(offset,direction),radius=fmin(.1,.035+depth*.008);
        if(depth<0||depth>nearest+ED_EPS)continue;
        EdVec side=edSub(offset,edScale(direction,depth));if(edDot(side,side)<=radius*radius){found=i;nearest=depth;}
    }return found;
}
static int deleteEditorLight(int id) {
    if(id<0||id>=model->lightCount)return 0;
    remember();edCopyModel(candidate,model);for(int shift=id;shift+1<candidate->lightCount;shift++)candidate->lights[shift]=std::move(candidate->lights[shift+1]);
    edZero(&candidate->lights[--candidate->lightCount],0,sizeof(EdLight));if(!commit())return 0;
    lightHovered=lightGrabbed=-1;grabbing=lightGrabbing=0;say("Light deleted. Ctrl+Z: undo.");return 1;
}
static int lightControl(EdVec origin,EdVec direction,int fire,int grab,int shift,int wheel) {
    lightHovered=pickEditorLight(origin,direction,hover.cell>=0?hover.distance:1000);
    if(grab&&!grabDown&&lightHovered>=0){
        remember();edCopyModel(snapshot,model);lightGrabbed=lightHovered;lightGrabbing=grabbing=1;
        grabPlanePoint=model->lights[lightGrabbed].position;grabPlaneNormal=direction;
        grabTraceValid=edTracePlane(origin,direction,grabPlanePoint,grabPlaneNormal,&grabLastPoint);grabLastOrigin=origin;grabDelta=edV(0,0,0);
    }
    if(lightGrabbing&&grab&&lightGrabbed>=0&&lightGrabbed<snapshot->lightCount){
        EdVec hit;int valid=edTracePlane(origin,direction,grabPlanePoint,grabPlaneNormal,&hit);
        if(valid&&grabTraceValid)grabDelta=edAdd(grabDelta,edAdd(edSub(hit,grabLastPoint),edScale(grabPlaneNormal,edDot(edSub(origin,grabLastOrigin),grabPlaneNormal))));
        if(valid)grabLastPoint=hit;grabLastOrigin=origin;grabTraceValid=valid;
        EdVec position=edSnapPoint(edAdd(grabPlanePoint,grabDelta));
        if(objectSector(snapshot,position)>=0){edCopyModel(candidate,snapshot);candidate->lights[lightGrabbed].position=position;commit();}
    }
    if(!grab&&grabDown)lightGrabbing=grabbing=0;
    if(wheel&&lightHovered>=0&&!lightGrabbing){
        edCopyModel(candidate,model);EdLight *l=&candidate->lights[lightHovered];double step=wheel>0?1:-1;
        if(shift)l->range=fmin(10000,fmax(ED_GRID,l->range+step*ED_GRID));else l->intensity=fmin(100,fmax(0,round((l->intensity+step*.1)*10)/10));
        if(!edModelsEqual(candidate,model)){remember();commit();}
    }
    else if(wheel&&shift&&hover.cell>=0&&hover.face>=0&&!lightGrabbing){
        edCopyModel(candidate,model);EdFace *face=&candidate->c[hover.cell].f[hover.face];
        EdSurfaceAttributes attributes=edSurfaceAttributes(face);
        double value=fmin(1,fmax(0,round((attributes.extraLight+(wheel>0?.05:-.05))*100)/100));
        if(value!=attributes.extraLight){
            face->hasAttributes=1;face->attributes=attributes;face->attributes.extraLight=value;
            if(face->attributes.lightingMode==RD_LIGHTMODE_FULLYLIT)face->attributes.lightingMode=RD_LIGHTMODE_GOURAUD;
            remember();if(commit()){char text[96];snprintf(text,sizeof(text),"Surface %d:%d extra light: %.2f",hover.cell,hover.face,value);say(text);}
        }
    }
    else if(wheel&&hover.cell>=0&&!lightGrabbing){
        edCopyModel(candidate,model);EdCell *sector=&candidate->c[hover.cell];
        sector->extraLight=fmin(1,fmax(0,round((sector->extraLight+(wheel>0?.05:-.05))*100)/100));
        if(sector->extraLight!=model->c[hover.cell].extraLight){
            /* Older editor levels used fully-lit faces. Make their sector
               brightness adjustable without changing materials or UVs. */
            for(int f=0;f<sector->count;f++){EdFace *face=&sector->f[f];EdSurfaceAttributes a=edSurfaceAttributes(face);if(a.lightingMode==RD_LIGHTMODE_FULLYLIT){face->hasAttributes=1;face->attributes=a;face->attributes.lightingMode=RD_LIGHTMODE_GOURAUD;}}
            remember();if(commit()){char text[96];snprintf(text,sizeof(text),"Sector %d light: %.2f",hover.cell,model->c[hover.cell].extraLight);say(text);}
        }
    }
    if(fire&&!fireDown&&!lightGrabbing){
        if(model->lightCount>=ED_LIGHTS)say("Light limit reached.");
        else{double depth=.5;if(hover.cell>=0)depth=fmin(depth,hover.distance-ED_GRID*.5);int placed=0;
            for(;depth>=ED_GRID;depth-=ED_GRID){EdVec position=edSnapPoint(edAdd(origin,edScale(direction,depth)));if(objectSector(model,position)<0)continue;
                remember();edCopyModel(candidate,model);lightHovered=candidate->lightCount;candidate->lights[candidate->lightCount++]=EdLight{0,position,1,4};
                placed=commit();if(placed)say("Light placed. Wheel: intensity; Shift+wheel: range. RMB: move.");break;}
            if(!placed)say("No space in front of the player to place a light.");
        }
    }
    fireDown=fire;grabDown=grab;return 1;
}
static int lightsWorldCheck(void) {
    SithWorld *w=sithWorld_g_pCurrentWorld;int ok=model->lightCount>0;double lo=1,hi=0;
    for(int s=0;s<model->count;s++){
        ok=ok&&fabs(w->aSectors[s].ambientLight-model->c[s].ambientLight)<.00001&&fabs(w->aSectors[s].extraLight-model->c[s].extraLight)<.00001;
        for(int f=0;f<model->c[s].count;f++){const EdFace *face=&model->c[s].f[f];SithSurface *surface=&w->aSectors[s].surfaces[f];ok=ok&&surface->surfaceInfo.face.lightingMode==RD_LIGHTMODE_GOURAUD;
            for(int v=0;v<face->count;v++){double expected=edVertexLight(model,s,f,v);lo=fmin(lo,expected);hi=fmax(hi,expected);
                for(int channel=0;channel<4;channel++)ok=ok&&fabs(surface->surfaceInfo.intensities[channel*face->count+v]-expected)<.00001;}
        }
    }return ok&&hi>0&&hi-lo>.01;
}
static int lightSelfTest(void) {
    EdModel *saved=edAlloc(sizeof(*saved));if(!saved)return 0;edCopyModel(saved,model);EdSelection savedPicks=picks;
    clearHistory();fireDown=grabDown=0;EdVec origin=edV(0,0,.5),direction=edV(0,1,0);edRay(model,origin,direction,&hover);
    int ok=fabs(model->c[0].extraLight-.5)<.00001;
    lightControl(origin,direction,0,0,0,1);ok=ok&&fabs(model->c[0].extraLight-.55)<.00001&&fabs(sithWorld_g_pCurrentWorld->aSectors[0].extraLight-.55)<.00001;
    ok=ok&&historyStep(0)&&fabs(model->c[0].extraLight-.5)<.00001&&historyStep(1)&&fabs(model->c[0].extraLight-.55)<.00001&&historyStep(0);clearHistory();
    ok=ok&&lightControl(origin,direction,1,0,0,0)&&model->lightCount==1&&undoCount==1&&fabs(model->c[0].extraLight-.5)<.00001&&lightsWorldCheck();
    lightControl(origin,direction,0,0,0,1);ok=ok&&fabs(model->lights[0].intensity-1.1)<.001;
    lightControl(origin,direction,0,0,1,-1);ok=ok&&fabs(model->lights[0].range-(4-ED_GRID))<.001;
    lightControl(origin,direction,0,1,0,0);ok=ok&&lightGrabbing;
    lightControl(edAdd(origin,edV(ED_GRID,0,0)),direction,0,1,0,0);lightControl(origin,direction,0,0,0,0);ok=ok&&fabs(model->lights[0].position.x-ED_GRID)<.001;
    ok=ok&&historyStep(0)&&fabs(model->lights[0].position.x)<.001&&historyStep(1)&&fabs(model->lights[0].position.x-ED_GRID)<.001&&lightsWorldCheck();
    ok=ok&&edWriteGob(model,"Episode/sithed_lights_test.gob","Lights integration test",0,resources);
    remember();edCopyModel(candidate,model);ok=ok&&edExtrudeStep(candidate,0,4)&&commit()&&model->lightCount==1&&lightsWorldCheck();
    ok=ok&&historyStep(0)&&model->count==1&&model->lightCount==1&&lightsWorldCheck();
    ok=ok&&deleteEditorLight(0)&&!model->lightCount&&fabs(sithWorld_g_pCurrentWorld->aSectors[0].extraLight-.5)<.00001&&historyStep(0)&&model->lightCount==1&&lightsWorldCheck();
    edCopyModel(candidate,saved);int restored=commit();clearHistory();picks=savedPicks;edFree(saved);grabbing=lightGrabbing=0;lightHovered=lightGrabbed=-1;
    printf("GEOSITH_LIGHTS %s: placement, dragging, intensity/range, baked lighting, undo and save\n",ok&&restored?"PASS":"FAIL");fflush(stdout);return ok&&restored;
}
static int episodeSelfTest(void){
    EdModel *saved=edAlloc(sizeof(*saved));if(!saved)return 0;edCopyModel(saved,model);clearHistory();remember();edCopyModel(candidate,model);
    EdEpisode *e=&candidate->episode;edEpisodeDefault(e,"Episode integration test",0);e->type=3;e->count=2;e->rows[0].line=10;e->rows[0].lightPower=2;e->rows[0].darkPower=1;e->rows[0].gotoB=20;strcpy(e->rows[0].file,"episode_test.jkl");
    e->rows[1]=EdEpisodeRow{.line=20,.cd=1,.level=2,.cutscene=1,.gotoA=-1,.gotoB=-1};strcpy(e->rows[1].file,"outro.smk");strcpy(e->rows[1].title,"Ending");strcpy(e->rows[1].intro[0],"Ending text");
    strcpy(candidate->header.title,"Episode Level Name");strcpy(candidate->header.intro[0],"Episode loading text");
    int ok=edEpisodeValidate(e)&&commit()&&undoCount==1&&writeEditorGob(model,"Episode/sithed_episode_test.gob","Episode level",0);
    ok=ok&&historyStep(0)&&!strcmp(model->episode.rows[0].file,"sithed.jkl")&&historyStep(1)&&!strcmp(model->episode.rows[0].file,"episode_test.jkl");
    edCopyModel(candidate,saved);int restored=commit();clearHistory();edFree(saved);printf("GEOSITH_EPISODE %s: sequence changes, strings, one-step undo and custom filename export\n",ok&&restored?"PASS":"FAIL");fflush(stdout);return ok&&restored;
}
#include "jkEditorSpline.h"
#include "jkEditorMap.h"
#include "jkEditorModels.h"
#include "jkEditorTemplates.h"
#include "jkEditorTerrain.h"
static void mapOptionsCapture(jkGuiMenu *menu){
#ifdef SDL2_RENDER
    static int frame;for(int i=1;i<=3;i++)menu->paElements[i].selectedTextEntry=1;if(++frame==3){std3D_ScreenshotWindow(getenv("GEOSITH_MAP_OPTIONS_CAPTURE"));menu->lastClicked=1;}
#endif
}
static void mapOptions(void){
    BuilderMenuState state=builderMenuBegin();jkGuiElement e[8]={};e[0]={ELEMENT_TEXT,0,2,u"Map Editing Options",3,{20,30,600,30},1};
    const char16_t *labels[]={u"Mirror X",u"Mirror Y",u"Mirror Z"};for(int i=0;i<3;i++){e[i+1]={ELEMENT_CHECKBOX,0,0,labels[i],0,{180,120+i*45,300,30},1};e[i+1].selectedTextEntry=(mapMirrorAxes>>i)&1;}
    e[4]={ELEMENT_TEXT,0,0,u"Mirror planes: world X = 0, Y = 0, Z = 0",3,{20,280,600,30},1};e[5]={ELEMENT_TEXTBUTTON,1,2,u"Apply",3,{80,370,200,40},1};e[6]={ELEMENT_TEXTBUTTON,-1,2,u"Cancel",3,{360,370,200,40},1};e[7].type=ELEMENT_END;
    jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[6]);if(getenv("GEOSITH_MAP_OPTIONS_CAPTURE"))menu.idkFunc=mapOptionsCapture;
    if(jkGuiRend_DisplayAndReturnClicked(&menu)==1){mapMirrorAxes=0;for(int i=0;i<3;i++)if(e[i+1].selectedTextEntry)mapMirrorAxes|=1<<i;}
    builderMenuEnd(state);
}

static int editorControl(SithThing *p,flex_t delta) {
    if(!jkEditor_IsLevel()||!p) return 0;
    if(!bindEditorWorld())return 0;
    netControlReady(p);
    if(stressEditor(p))return 1;
    int speedHeld=active?editorForceSpeedHeld():0;
    int speedPressed=speedHeld&&!flightSpeedDown;flightSpeedDown=speedHeld;
    /* Keep the normal bound chat/console key available in every editor view.
       Text entry must not also move the camera or trigger editing shortcuts. */
    if(active&&jkControl_HandleChat()){
        splineDragging=terrainStroke=0;
        grabbing=groupGrab=cleaving=objectGrabbing=objectRotating=lightGrabbing=0;
        mapBox=mapDragging=mapGeometryDrag=mapCleaveDragged=0;
        fireDown=grabDown=middleDown=mapLeft=mapRight=1;
        rdVector_Zero3(&p->physicsParams.vel);rdVector_Zero3(&p->physicsParams.acceleration);
        return 1;
    }
    /* Map controls own the entire frame, including the transition back to 3D. */
    if(active&&mapActive){mapControl(p);return 1;}
    int toggle=stdControl_ReadKey(DIK_F8,NULL)!=0;
    if(toggle&&!toggleDown&&!cogPicking()&&builderAsset<0) {
        if(!active) { savedPhysics=p->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=p->flags;active=1;setEditorFlight(p,0);p->flags|=SITH_TF_INVULN;
            homeDown=stdControl_ReadKey(DIK_HOME,NULL)!=0;tabDown=stdControl_ReadKey(DIK_TAB,NULL)!=0;qDown=stdControl_ReadKey(DIK_Q,NULL)!=0;
            deleteDown=stdControl_ReadKey(DIK_DELETE,NULL)!=0;middleDown=stdControl_ReadKey(KEY_MOUSE_B3,NULL)!=0;
            syncEditorObjects(model);suspendPlacedCogs(0);edResetDoubleTap(&jumpTap,sithControl_GetKey(INPUT_FUNC_JUMP,NULL));
            edResetDoubleTap(&selectionTap,editButton(INPUT_FUNC_FIRE1));
            for(int i=1;i<=10;i++) { sithInventory_SetInventoryAvailable(p,i,1);sithInventory_SetInventory(p,i,100); }
            sithInventory_SetInventory(p,SITHBIN_ENERGY,500);sithInventory_SetInventory(p,SITHBIN_POWER,500);sithInventory_SetInventory(p,SITHBIN_RAILCHARGES,100);sithWeapon_SelectWeapon(p,tool,0);sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);
            say("Editor: double-tap jump to fly. RMB grab, LMB cleave. F8 play.");
        } else { terrainStroke=splineDragging=0;leaveEditor(p);say("Play mode. F8 returns to the editor."); }
        if(sithNet_isMulti&&sithNet_isServer)netCommitted();
    }toggleDown=toggle;if(!active)return 0;
    jkControl_HandleScreenshot();
    int tab=stdControl_ReadKey(DIK_TAB,NULL)!=0;
    if(tab&&!tabDown&&!cogPicking()&&builderAsset<0){tabDown=tab;mapOpen(p);return 1;}tabDown=tab;
    if(edDoubleTap(&jumpTap,sithControl_GetKey(INPUT_FUNC_JUMP,NULL),stdPlatform_GetTimeMsec(),350))
        setEditorFlight(p,(p->physicsParams.flags&SITH_PF_FLY)==0);
    int home=stdControl_ReadKey(DIK_HOME,NULL)!=0,q=stdControl_ReadKey(DIK_Q,NULL)!=0;
    if(home&&!homeDown)teleportWalkplayer(p);
    if(q&&!qDown)toggleFlightCollision(p);
    homeDown=home;qDown=q;
    if(speedPressed&&(p->physicsParams.flags&SITH_PF_FLY)){
        flightSpeedBoost=!flightSpeedBoost;
        rdVector_Scale3Acc(&p->physicsParams.vel,flightSpeedBoost?2.0:0.5);
        say(flightSpeedBoost?"Flight speed: 2x. Force Speed toggles back to normal.":"Flight speed: normal. Force Speed toggles 2x.");
    }
    sithControl_PlayerLook(p,delta);
    if(p->physicsParams.flags&SITH_PF_FLY)editorFlightMovement(p);
    else if(p->attach_flags)sithControl_PlayerMovement(p);
    else sithControl_FreeCam(p);
    if(cogPicking())return cogPickControl(p);
    for(int i=0;i<10;i++)if(stdControl_ReadKey(i?DIK_1+i-1:DIK_0,NULL)) { int weapon=i?i:10;if(weapon!=tool){tool=weapon;terrainStroke=0;splineDragging=0;grabbing=groupGrab=cleaving=lightGrabbing=objectGrabbing=objectRotating=0;edResetDoubleTap(&selectionTap,editButton(INPUT_FUNC_FIRE1));sithWeapon_SelectWeapon(p,weapon,0);} }
    int shift=stdControl_ReadKey(DIK_LSHIFT,NULL)||stdControl_ReadKey(DIK_RSHIFT,NULL),ctrl=stdControl_ReadKey(DIK_LCONTROL,NULL)||stdControl_ReadKey(DIK_RCONTROL,NULL);
    int fire=editButton(INPUT_FUNC_FIRE1),grab=editButton(INPUT_FUNC_FIRE2),didExtrude=0;
    int middle=stdControl_ReadKey(KEY_MOUSE_B3,NULL)!=0,middlePressed=middle&&!middleDown;middleDown=middle;
    if(builderAsset>=0&&builderExitRequested){builderFinish();return 1;}
    /* A new gesture must start from an acknowledged document. An existing
       drag can still coalesce its latest position while awaiting the host. */
    if(netEditPending()&&!grabbing&&!cleaving){fireDown=fire;grabDown=grab;return 1;}
    if(builderAsset>=0){builderControl(fire,grab,middlePressed,shift,ctrl);return 1;}
    if(tool==8){if(middlePressed)templateCreatorMenu();fireDown=fire;grabDown=grab;return 1;}
    if(tool==9){if(middlePressed){builderLibraryMenu();return 1;}if(fire&&!fireDown){EdVec origin,direction;ray(&origin,&direction);builderBegin(origin,direction);}fireDown=fire;grabDown=grab;return 1;}
    if(tool==10){int wheel=stdControl_aAxisStates[AXIS_MOUSE_Z];
        if(wheel&&ctrl&&cleaveShape)shapeRotation=fmod(shapeRotation+(wheel>0?5:-5)+360,360);
        else if(wheel&&shift)shapeRadius=fmax(ED_GRID,fmin(64,shapeRadius+(wheel>0?ED_GRID:-ED_GRID)));
        else if(wheel){int index=cleaveShape?cleaveShape-2:0;index=(index+wheel%31+31)%31;cleaveShape=index?index+2:0;cleaving=0;}
        if(middlePressed){grabbing=groupGrab=cleaving=0;shapeMenu();middleDown=1;fireDown=grabDown=1;return 1;}
    }
    if(tool==2) {
        /* The raw wheel state also works when the profile has no wheel-axis binding. */
        int wheel=stdControl_aAxisStates[AXIS_MOUSE_Z];
        if(wheel&&paletteCount)currentMaterial=(currentMaterial+wheel%paletteCount+paletteCount)%paletteCount;
        if(middlePressed) {grabbing=groupGrab=cleaving=0;browserObjects=0;materialMenu();middleDown=1;fireDown=grabDown=1;return 1;}
    }
    if(tool==1&&middlePressed){splineDragging=0;grabbing=groupGrab=cleaving=0;selectionMenu();middleDown=1;fireDown=grabDown=1;edResetDoubleTap(&selectionTap,1);return 1;}
    if(tool==1&&!grabbing&&!cleaving&&stdControl_aAxisStates[AXIS_MOUSE_Z]){if(shift&&!selectionCleanup)selectionScale(stdControl_aAxisStates[AXIS_MOUSE_Z]);else selectionWheel(stdControl_aAxisStates[AXIS_MOUSE_Z]);fireDown=fire;grabDown=grab;return 1;}
    int opposite=stdControl_ReadKey(DIK_BACK,NULL)!=0;
    if(opposite&&!oppositeAdjoinDown&&tool==1&&!selectionCleanup&&!grabbing&&!cleaving)selectOppositeAdjoins();
    oppositeAdjoinDown=opposite;
    int doubleSelect=edDoubleTap(&selectionTap,tool==1&&!selectionCleanup&&fire&&!grabbing&&!ctrl,stdPlatform_GetTimeMsec(),300);
    EdVec origin,direction;ray(&origin,&direction);if(tool==1&&shift)edRaySelection(model,origin,direction,&hover);else if(tool==2)edRayMaterial(model,origin,direction,&hover);else edRay(model,origin,direction,&hover);
    if(tool==1&&(selectionCleanup||ctrl))hover.vertex=hover.edge=-1;
    int deleting=stdControl_ReadKey(DIK_DELETE,NULL)!=0,deletePressed=deleting&&!deleteDown;deleteDown=deleting;
    if(deletePressed&&tool!=3&&tool!=4&&tool!=5&&tool!=7&&hover.cell>=0&&model->c[hover.cell].splineId){say("Editable spline: use Convert to Sectors in the spline menu first.");fireDown=fire;grabDown=grab;return 1;}
    if(deletePressed){if(tool==7)terrainDelete(terrainPick(origin,direction,&terrainHover));else if(tool==3)deleteEditorObject(pickEditorObject(origin,direction,hover.cell>=0?hover.distance:1000));else if(tool==4)deleteEditorLight(pickEditorLight(origin,direction,hover.cell>=0?hover.distance:1000));else if(tool==1&&hover.cell>=0&&(hover.vertex>=0||hover.edge>=0))deleteGeometryHandle(hover);else if(tool!=5&&tool!=7&&hover.cell>=0)deleteSector(p,hover.cell);fireDown=fire;grabDown=grab;return 1;}
    int historyKey=ctrl&&stdControl_ReadKey(DIK_Z,NULL)?(shift?2:1):ctrl&&stdControl_ReadKey(DIK_Y,NULL)?2:0;
    int historyPressed=historyKey&&!historyDown;historyDown=historyKey;
    if(historyPressed&&!grabbing&&!cleaving){historyStep(historyKey==2);fireDown=fire;grabDown=grab;return 1;}
    if(tool==5){if(middlePressed){grabbing=groupGrab=cleaving=0;placedCogsMenu(0);return 1;}fireDown=fire;grabDown=grab;return 1;}
    int enter=stdControl_ReadKey(DIK_RETURN,NULL)||stdControl_ReadKey(DIK_NUMPADENTER,NULL),openAttributes=enter&&!attributesDown;attributesDown=enter;
    if(tool==1&&!selectionCleanup&&splineControl3D(origin,direction,fire,grab,openAttributes))return 1;
    if((tool==1||tool==2||tool==10)&&(grab||(tool==10||tool==2||selectionCleanup)&&fire)){
        int protectedGeometry=hover.cell>=0&&model->c[hover.cell].splineId;
        if(tool==1)for(int i=0;i<picks.count;i++){int c=picks.hits[i].cell;if(c>=0&&c<model->count)protectedGeometry|=model->c[c].splineId!=0;}
        if(protectedGeometry){say("Editable spline: use Convert to Sectors in the spline menu first.");grabbing=groupGrab=cleaving=0;fireDown=fire;grabDown=grab;return 1;}
    }
    if(tool==1&&!selectionCleanup&&openAttributes&&!grabbing&&!cleaving){if(selectedSectorCount())sectorAttributesDialog();else surfaceAttributesDialog();attributesDown=1;fireDown=grabDown=1;return 1;}
    int merge=stdControl_ReadKey(DIK_M,NULL)!=0,mergePressed=merge&&!mergeDown;mergeDown=merge;
    if(tool==1&&!selectionCleanup&&mergePressed&&!grabbing&&!cleaving){mergeSelectedSectors();fireDown=fire;grabDown=grab;return 1;}
    if(tool==3&&openAttributes&&!grabbing){int id=pickEditorObject(origin,direction,hover.cell>=0?hover.distance:1000);if(id!=-10000)thingAttributesDialog(id);attributesDown=1;fireDown=grabDown=1;return 1;}
    if(tool==3)return objectControl(p,origin,direction,fire,grab,middlePressed,shift);
    if(tool==7)return terrainControl(origin,direction,fire,middlePressed,shift,stdControl_aAxisStates[AXIS_MOUSE_Z],delta,ctrl);
    if(tool==4)return lightControl(origin,direction,fire,grab,shift,stdControl_aAxisStates[AXIS_MOUSE_Z]);
    if(tool==1&&selectionCleanup){hover.vertex=hover.edge=-1;if(fire&&!fireDown)cleanupAimedSector();fireDown=fire;grabDown=grab;return 1;}
    if(grab&&!grabDown&&((tool==1&&(picks.count>0||selectedSectorCount()))||hover.cell>=0)) {
        remember();edCopyModel(snapshot,model);grabPicks=picks;sectorGrab=tool==1&&selectedSectorCount();grabSectors=sectorPicks;
        groupGrab=sectorGrab||(tool==1&&picks.count>0);selection=groupGrab&&!sectorGrab?picks.hits[0]:hover;
        if(sectorGrab){
            if(selection.cell<0||!grabSectors[selection.cell]){int cell=0;while(cell<snapshot->count&&!grabSectors[cell])cell++;selection={cell,0,-1,-1};selection.point=edMapCenter(&snapshot->c[cell]);}
            selection.vertex=selection.edge=-1;
        }
        if(tool==2)selection.vertex=selection.edge=-1;
        uvRotating=tool==2&&shift;uvFine=tool==2&&ctrl;
        if(uvRotating)say("Rotate texture: drag around the face center; hover an edge to align. Release RMB to finish.");
        edResetDoubleTap(&selectionTap,fire);
        EdVec anchor=edSub(selection.point,origin);grabDepth=groupGrab?fmax(.1,sqrt(edDot(anchor,anchor))):hover.distance;
        uvMode=!sectorGrab&&(tool==2||(shift&&selection.vertex<0&&selection.edge<0));grabNormal=edNormal(&snapshot->c[selection.cell].f[selection.face]);
        grabPlanePoint=snapshot->c[selection.cell].f[selection.face].v[0].p;grabPlaneNormal=grabNormal;
        if(sectorGrab||(!uvMode&&(selection.vertex>=0||selection.edge>=0))){grabPlanePoint=selection.point;grabPlaneNormal=direction;}
        grabTraceValid=edTracePlane(origin,direction,grabPlanePoint,grabPlaneNormal,&grabLastPoint);
        if(!grabTraceValid){grabPlanePoint=selection.point;grabPlaneNormal=direction;grabTraceValid=edTracePlane(origin,direction,grabPlanePoint,grabPlaneNormal,&grabLastPoint);}
        edDragTraceBegin(&geometryDragTrace,origin,direction,grabPlanePoint,grabPlaneNormal,fmax(4,grabDepth*8));
        grabLastOrigin=origin;grabDelta=edV(0,0,0);grabVisual.count=1;grabVisual.hits[0]=selection;
        grabbing=1;uvMode=!sectorGrab&&(tool==2||(shift&&selection.vertex<0&&selection.edge<0));cleaving=0;
    }
    if(grabbing&&grab) {
        int surface=!sectorGrab&&(groupGrab?allSelectedSurfaces(&grabPicks):selection.vertex<0&&selection.edge<0);
        /* Keep a stable ray plane relative to the player, independent of live
           mesh rebuilds, ground motion and crossing the original wall. */
        grabDelta=edAdd(grabDelta,edDragTraceStep(&geometryDragTrace,origin,direction));
        EdVec uvMove=edSub(grabDelta,edScale(grabNormal,edDot(grabDelta,grabNormal))),move=grabDelta;
        if(!shift&&!uvMode&&selection.vertex>=0){
            move=edVertexEdgeDelta(snapshot,selection,move);
            if(groupGrab)for(int i=0;i<grabPicks.count;i++){EdHit h=grabPicks.hits[i];if(h.vertex>=0){EdVec other=edVertexEdgeDelta(snapshot,h,move),difference=edSub(other,move);if(edDot(difference,difference)>ED_EPS*ED_EPS){move=edV(0,0,0);break;}}}
        }
        if(surface)move=edSurfaceDragDelta(move,grabNormal);
        if(sectorGrab){
            moveGrabbedSectors(move);
        } else if(groupGrab) {
            int ok=1;
            if(uvMode&&surface) {
                edCopyModel(candidate,snapshot);for(int i=0;i<grabPicks.count;i++){EdHit h=grabPicks.hits[i];edPanFaceTexture(&candidate->c[h.cell].f[h.face],uvMove);}
            } else ok=edMoveSelection(candidate,snapshot,&grabPicks,move);
            if(ok&&commit()) {
                EdSelection mapped;edSelectionRemap(snapshot,model,&grabPicks,uvMode&&surface?edV(0,0,0):edSelectionDelta(snapshot,&grabPicks,move),&mapped);picks=mapped;
            }
        } else if(uvMode) { edCopyModel(candidate,snapshot);EdFace *f=&candidate->c[selection.cell].f[selection.face];
            if(uvRotating){int edge=hover.cell==selection.cell&&hover.face==selection.face?hover.edge:-1;edRotateTextureDrag(f,selection.point,uvMove,edge);}
            else edPanFaceTextureGrid(f,uvMove,uvFine?ED_GRID*.5:ED_GRID);commit();
        } else if(edDot(move,move)<=ED_EPS*ED_EPS) { edCopyModel(candidate,snapshot);commit(); }
        else if(edMove(candidate,snapshot,selection,move))commit();
        if(!groupGrab){EdSelection one={0};one.count=1;one.hits[0]=selection;edSelectionRemap(snapshot,model,&one,uvMode?edV(0,0,0):edSelectionDelta(snapshot,&one,move),&grabVisual);}
        if(fire&&!fireDown&&surface&&(!uvMode||(shift&&tool!=2))) {
            didExtrude=groupGrab?extrudeSelectedFaces(shift):extrudeGrabbedFace(uvMode?edV(0,0,0):move,shift);
        }
    }
    if(!grab&&grabDown)grabbing=groupGrab=0;
    if(!grabbing&&!didExtrude&&fire&&!fireDown) {
        if(tool==1&&ctrl){if(hover.cell>=0){sectorPicks[hover.cell]=!sectorPicks[hover.cell];picks.count=0;say("Sector selection: RMB moves; Ctrl+click toggles; M merges; Enter edits attributes.");}}
        else if(shift&&tool!=10&&tool!=1&&hover.cell>=0) {clipboard=currentMaterial=model->c[hover.cell].f[hover.face].material;say("Material copied.");}
        else if(ctrl&&clipboard>=0&&hover.cell>=0) {remember();edCopyModel(candidate,model);candidate->c[hover.cell].f[hover.face].material=clipboard;commit();}
        else if(tool==1&&!ctrl) {
            if(doubleSelect){sectorPicks.clear();picks.count=0;say("Selection cleared.");}
            else if(hover.cell>=0){sectorPicks.clear();if(!edSelectionToggle(model,&picks,hover))say("Selection limit reached.");else {char text[120];snprintf(text,sizeof(text),"%d selected. RMB move; RMB + LMB extrude; double-click LMB clear.",picks.count);say(text);}}
        }
        else if(tool==2&&hover.cell>=0) {if(editorMaterial(currentMaterial)){remember();edCopyModel(candidate,model);candidate->c[hover.cell].f[hover.face].material=currentMaterial;commit();}}
        else if(tool==10&&hover.cell>=0&&cleaveShape){remember();edCopyModel(candidate,model);if(edCleaveShapeRotated(candidate,hover.cell,hover.face,hover.point,shapeRadius,cleaveShape,shapeRotation))commit();else say("Shape cleave cancelled: the shape must cut an area of a solid surface, with room for its new faces.");}
        else if(tool==10&&hover.cell>=0) {cleaving=1;sectorCleave=shift;selection=hover;cleaveStart=cleaveEnd=edSnapFacePoint(&model->c[hover.cell].f[hover.face],hover.point);remember();}
    }
    if(cleaving) { EdFace *face=&model->c[selection.cell].f[selection.face];EdVec n=edNormal(face);double den=edDot(n,direction);if(fabs(den)>ED_EPS) {double t=edDot(n,edSub(cleaveStart,origin))/den;if(t>0)cleaveEnd=edSnapFacePoint(face,edAdd(origin,edScale(direction,t)));}
        if(!fire&&fireDown) { edCopyModel(candidate,model);int ok=sectorCleave?edCleaveSector(candidate,selection.cell,selection.face,cleaveStart,cleaveEnd):edCleave(candidate,selection.cell,selection.face,cleaveStart,cleaveEnd);if(ok)commit();else say("Cleave cancelled: aim two different points across the surface.");cleaving=0; }
    }
    fireDown=fire;grabDown=grab;return 1;
}
int jkEditor_Control(SithThing *p,flex_t delta)try {
    edClearGeometryError();int result=editorControl(p,delta);
    if(active&&edVertexLimitHit()){
        edClearGeometryError();grabbing=groupGrab=cleaving=mapDragging=0;fireDown=grabDown=1;
        rdVector_Zero3(&p->physicsParams.vel);rdVector_Zero3(&p->physicsParams.acceleration);
        int previousSafe=jkEditor_NetSafeDialog(1),previousModal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
        if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
        stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
        jkGuiDialog_ErrorDialog(u"Surface vertex limit",u"Operation cancelled. Jedi Knight allows at most 24 vertices per surface.\nSplit the surface, clip more of the shape off its edge, or choose fewer sides.");
        jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=previousModal;jkEditor_NetSafeDialog(previousSafe);stdControl_SetActivation(1);stdControl_Flush();
    }
    return result;
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return 1;}
void jkEditor_DrawWorld(void) try {
    editorViewReady=0;
    if(jkEditor_IsLevel()&&boundWorld==sithWorld_g_pCurrentWorld&&rdCamera_g_pCurCamera)builderDrawWorld();
    if(!active||!jkEditor_IsLevel()||!rdCamera_g_pCurCamera||!rdCamera_g_pCurCamera->pCanvas)return;
    if(stressRunning)stressRenderFrames++;
    editorView=*rdCamera_g_pCurCamera;
    editorViewCanvas=*editorView.pCanvas;
    editorView.pCanvas=&editorViewCanvas;
    editorViewReady=1;
    captureOverlayVisibility();
    if(mapActive)return;
    /* Pick against the same camera and player position used by this frame. */
    rdMatrix34 cameraWorld;rdMatrix_InvertOrtho34(&cameraWorld,&editorView.orient);
    if(builderAsset>=0){edMeshRay(model,builderAsset,vec(cameraWorld.scale),vec(cameraWorld.lvec),tool==1,&builderHover);return;}
    if(cogPicking())updateCogPick(vec(cameraWorld.scale),vec(cameraWorld.lvec));
    if(tool==1&&(stdControl_ReadKey(DIK_LSHIFT,NULL)||stdControl_ReadKey(DIK_RSHIFT,NULL)))edRaySelection(model,vec(cameraWorld.scale),vec(cameraWorld.lvec),&hover);else if(tool==2)edRayMaterial(model,vec(cameraWorld.scale),vec(cameraWorld.lvec),&hover);else edRay(model,vec(cameraWorld.scale),vec(cameraWorld.lvec),&hover);
    if(tool==7)terrainPick(vec(cameraWorld.scale),vec(cameraWorld.lvec),&terrainHover);
    if(tool==4&&!lightGrabbing)lightHovered=pickEditorLight(vec(cameraWorld.scale),vec(cameraWorld.lvec),hover.cell>=0?hover.distance:1000);
    if(!cogPicking())drawFrameGhosts();
    int shapePreview=tool==10&&cleaveShape&&!grabbing&&hover.cell>=0;
    if(!cleaving&&!shapePreview)return;
    if(!bladeReady) {if(!rdPolyline_NewEntry(&cleaveBlade,"Editor cleave","sabergreen1.mat","sabergreen0.mat",.01,.004,.004,RD_GEOMETRY_FULL,RD_LIGHTMODE_FULLYLIT,RD_TEXTUREMODE_PERSPECTIVE,1))return;rdThing_NewEntry(&cleaveThing,NULL);rdThing_SetPolyline(&cleaveThing,&cleaveBlade);bladeReady=1;}
    const EdFace *face=shapePreview?&model->c[hover.cell].f[hover.face]:&model->c[selection.cell].f[selection.face];EdFace outline;EdVec n=edNormal(face);
    if(shapePreview&&edClipShape(face,hover.point,shapeRadius,cleaveShape,shapeRotation,&outline)!=1)return;
    for(int i=0;i<(shapePreview?outline.count:1);i++){
        EdVec a=shapePreview?outline.v[i].p:cleaveStart,b=shapePreview?outline.v[(i+1)%outline.count].p:cleaveEnd;
        EdVec d=edSub(b,a);double length=sqrt(edDot(d,d));rdMatrix34 orient;rdVector3 forward=length>ED_EPS?rdvec(edScale(d,1/length)):rdVector3{(flex_t)(0),(flex_t)(1),(flex_t)(0)};rdMatrix_BuildFromLook34(&orient,&forward);
        orient.scale=rdvec(edAdd(a,edScale(n,.001)));cleaveBlade.length=length;rdPolyline_Draw(&cleaveThing,&orient);rdCache_Flush();
    }
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return;}
static void projectCameraPoint(const rdVector3 *p,rdVector3 *out) {
    rdCamera *saved=rdCamera_g_pCurCamera;
    rdCamera_SetCurrent(&editorView);editorView.pfProject(out,p);rdCamera_SetCurrent(saved);
}
static int project(EdVec p,rdVector3 *out) {
    rdVector3 world=rdvec(p),cam;rdMatrix_TransformPoint34(&cam,&world,&editorView.orient);
    if(cam.y<.01)return 0;projectCameraPoint(&cam,out);return 1;
}
static void screenLine(int x1,int y1,int x2,int y2,int color,int width) {
    /* Offset perpendicular to the major axis to keep diagonals equally thick. */
    int horizontal=abs(x2-x1)>=abs(y2-y1);
    for(int offset=-(width/2);offset<=width/2;offset++)
        rdPrimit2_DrawClippedLine(highlightCanvas,x1+(horizontal?0:offset),y1+(horizontal?offset:0),x2+(horizontal?0:offset),y2+(horizontal?offset:0),color,-1);
}
static void drawEditorCrosshair(void) {
    /* Use the picking camera's projection center, including reduced viewports. */
    int x=(int)lround(editorViewCanvas.half_screen_width);
    int y=(int)lround(editorViewCanvas.half_screen_height);
    int unit=(int)lround(highlightCanvas->pVBuffer->format.height/960.0);
    if(unit<1)unit=1;
    int yellow=palettePixel(highlightCanvas->pVBuffer,255,255,0);
    int orange=palettePixel(highlightCanvas->pVBuffer,144,56,0);
    int gap=3*unit,radius=8*unit;
    for(int axis=0;axis<2;axis++)for(int sign=-1;sign<=1;sign+=2) {
        int a=sign*gap,b=sign*radius;
        int outerA=sign*(gap-unit),outerB=sign*(radius+unit);
        screenLine(x+(axis?0:outerA),y+(axis?outerA:0),x+(axis?0:outerB),y+(axis?outerB:0),orange,4*unit-1);
        screenLine(x+(axis?0:a),y+(axis?a:0),x+(axis?0:b),y+(axis?b:0),yellow,2*unit-1);
    }
}
static void line(EdVec a,EdVec b,int color) {
    rdVector3 wa=rdvec(a),wb=rdvec(b),ca,cb,p,q;
    rdMatrix_TransformPoint34(&ca,&wa,&editorView.orient);rdMatrix_TransformPoint34(&cb,&wb,&editorView.orient);
    /* Clip at the eye plane so an edge remains visible when one end is behind us. */
    const flex_t nearDepth=.01;
    if(ca.y<nearDepth&&cb.y<nearDepth)return;
    if(ca.y<nearDepth||cb.y<nearDepth) {
        rdVector3 *back=ca.y<nearDepth?&ca:&cb,*front=ca.y<nearDepth?&cb:&ca;
        flex_t t=(nearDepth-back->y)/(front->y-back->y);
        back->x+=(front->x-back->x)*t;back->z+=(front->z-back->z)*t;back->y=nearDepth;
    }
    projectCameraPoint(&ca,&p);projectCameraPoint(&cb,&q);
    screenLine((int)p.x,(int)p.y,(int)q.x,(int)q.y,outlineColor,edgeWidth+2);
    screenLine((int)p.x,(int)p.y,(int)q.x,(int)q.y,color,edgeWidth);
}
static void vertexCircle(EdVec vertex,int selected,int color) {
    rdVector3 p;if(!project(vertex,&p))return;
    int radius=edgeWidth*(selected?4:3);
    int x=(int)lround(p.x),y=(int)lround(p.y);
    for(int r=radius-1;r<=radius+2;r++)
        rdPrimit2_DrawClippedCircle(highlightCanvas,x,y,r,10.0,(r==radius-1||r==radius+2)?outlineColor:color,-1);
}
static void splineDraw3D(int selectedColor,int vertexColor){
    if(tool!=1||selectionCleanup)return;
    for(int i=0;i<model->splineCount;i++){const EdSpline &s=splineDragging&&model->splines[i].id==splineSelected?splineDraft:model->splines[i];int color=s.id==splineSelected?selectedColor:vertexColor;
        EdVec a=edSplinePosition(&s,0);for(int j=1;j<=(s.closed?s.count:s.count-1)*24;j++){EdVec b=edSplinePosition(&s,j/24.0);line(a,b,color);a=b;}
        for(int p=0;p<s.count;p++)vertexCircle(s.points[p].position,s.id==splineSelected&&p==splinePoint,color);
    }
}

static void drawHit(const EdHit *hit,int color) {
    if(hit->cell<0||hit->cell>=model->count||hit->face<0||hit->face>=model->c[hit->cell].count)return;
    EdFace *face=&model->c[hit->cell].f[hit->face];
    if(hit->vertex>=0) {if(hit->vertex<face->count)vertexCircle(face->v[hit->vertex].p,1,color);}
    else if(hit->edge>=0) {
        if(hit->edge<face->count){int next=(hit->edge+1)%face->count;line(face->v[hit->edge].p,face->v[next].p,color);vertexCircle(face->v[hit->edge].p,0,color);vertexCircle(face->v[next].p,0,color);}
    } else for(int i=0;i<face->count;i++){line(face->v[i].p,face->v[(i+1)%face->count].p,color);vertexCircle(face->v[i].p,0,color);}
}
static int highlightSelfTest(void) {
    /* Exercise the actual clipped rasterizer with guard bytes around its buffer. */
    enum {W=64,H=48,GUARD=16384};
    unsigned char *pixels=(unsigned char*)edAlloc(GUARD+W*H*2+GUARD);
    if(!pixels)return 0;
    rdCanvas canvas={0};tVBuffer buffer={0};
    rdCanvas *savedCanvas=highlightCanvas;int savedWidth=edgeWidth,savedColor=outlineColor;
    canvas.pVBuffer=&buffer;canvas.widthMinusOne=W-1;canvas.heightMinusOne=H-1;
    buffer.format.width=W;buffer.format.height=H;buffer.format.rowWidth=W;
    buffer.surface_lock_alloc=pixels+GUARD;highlightCanvas=&canvas;edgeWidth=3;outlineColor=1;
    int ok=1;uint32_t seed=1;
    for(int bpp=8;bpp<=16&&ok;bpp+=8) {
        int size=W*H*(bpp/8);buffer.format.format.bpp=bpp;buffer.format.rowSize=W*(bpp/8);
        edZero(pixels,0xa5,GUARD+W*H*2+GUARD);edZero(pixels+GUARD,0,size);
        for(int i=0;i<2000;i++) {
            int xy[4];for(int j=0;j<4;j++){seed=seed*1664525u+1013904223u;xy[j]=(int)(seed%200001)-100000;}
            screenLine(xy[0],xy[1],xy[2],xy[3],2,5);
        }
        for(int i=0;i<GUARD;i++)if(pixels[i]!=0xa5||pixels[GUARD+size+i]!=0xa5){ok=0;break;}
    }
    highlightCanvas=savedCanvas;edgeWidth=savedWidth;outlineColor=savedColor;edFree(pixels);
    printf("GEOSITH_HIGHLIGHT_TEST %s\n",ok?"PASS":"FAIL");fflush(stdout);return ok;
}
void jkEditor_Draw(void) try {
    if(!active||!jkEditor_IsLevel()||!editorViewReady)return;
    highlightCanvas=Video_pCanvas;
#ifdef SDL2_RENDER
    /* The menu buffer is cropped/rescaled for HUD panels. The map overlay is
       presented 1:1 over the world and uses the world's screen coordinates. */
    highlightCanvas=Video_pCanvasOverlayMap;
#endif
    if(!highlightCanvas||!highlightCanvas->pVBuffer)return;
    if(mapActive){mapDraw();return;}
    edgeWidth=3;if(highlightCanvas->pVBuffer->format.height>=1440)edgeWidth=5;
    rdColor24 *colors=(rdColor24*)stdDisplay_masterPalette;
    int edgeColor=1+stdColor_FindClosest(colors+1,255,grabbing?255:64,255,grabbing?64:255);
    int vertexColor=1+stdColor_FindClosest(colors+1,255,255,224,64);
    int selectedColor=1+stdColor_FindClosest(colors+1,255,255,32,32);
    int facingColor=1+stdColor_FindClosest(colors+1,255,32,255,64);
    outlineColor=1+stdColor_FindClosest(colors+1,255,0,0,0);
    if(highlightCanvas->pVBuffer->format.format.bpp!=8) {
        edgeColor=stdColor_Indexed8ToRGB16(edgeColor,colors,&highlightCanvas->pVBuffer->format.format);
        vertexColor=stdColor_Indexed8ToRGB16(vertexColor,colors,&highlightCanvas->pVBuffer->format.format);
        selectedColor=stdColor_Indexed8ToRGB16(selectedColor,colors,&highlightCanvas->pVBuffer->format.format);
        facingColor=stdColor_Indexed8ToRGB16(facingColor,colors,&highlightCanvas->pVBuffer->format.format);
        outlineColor=stdColor_Indexed8ToRGB16(outlineColor,colors,&highlightCanvas->pVBuffer->format.format);
    }
    stdDisplay_VBufferLock(highlightCanvas->pVBuffer);
    if(builderAsset>=0&&builderAsset<model->assetCount){builderDrawOverlay(edgeColor,selectedColor);stdDisplay_VBufferUnlock(highlightCanvas->pVBuffer);
        if(tool==2){tVBuffer *buffer=highlightCanvas->pVBuffer;int x=buffer->format.width-170,y=buffer->format.height-260;if(x<8)x=8;if(y<80)y=80;drawMaterialPreview(buffer,currentMaterial,x,y,96,96);stdDisplay_VBufferLock(buffer);editorDrawText(buffer,jkGui_stdFonts[0],x,y+104,160,paletteNames[currentMaterial],1);stdDisplay_VBufferUnlock(buffer);}return;}
    if(cogPicking()){drawCogPicker(edgeColor);drawEditorCrosshair();stdDisplay_VBufferUnlock(highlightCanvas->pVBuffer);stdDisplay_VBufferLock(Video_pMenuBuffer);editorDrawText(Video_pMenuBuffer,jkGui_stdFonts[0],8,8,630,"EDITOR | Placed COG parameter picker",1);editorDrawText(Video_pMenuBuffer,jkGui_stdFonts[0],8,28,630,message,1);stdDisplay_VBufferUnlock(Video_pMenuBuffer);return;}
    splineDraw3D(selectedColor,vertexColor);terrainDraw(vertexColor);
    if(tool==1&&selectionCleanup)hover.vertex=hover.edge=-1;
    const EdHit *h=grabbing&&!groupGrab?&selection:&hover;
    int sectorMode=(tool==1&&!selectionCleanup&&(stdControl_ReadKey(DIK_LCONTROL,NULL)||stdControl_ReadKey(DIK_RCONTROL,NULL)))||(tool==10&&!cleaveShape&&(cleaving?sectorCleave:stdControl_ReadKey(DIK_LSHIFT,NULL)||stdControl_ReadKey(DIK_RSHIFT,NULL)));
    for(int i=0;i<model->lightCount;i++){
        EdVec p=model->lights[i].position;if(!objectOverlayVisible(p))continue;int selected=tool==4&&(lightGrabbing?lightGrabbed:lightHovered)==i,color=selected?selectedColor:vertexColor;
        vertexCircle(p,selected,color);
        for(int axis=0;axis<3;axis++){EdVec d=axis==0?edV(.045,0,0):axis==1?edV(0,.045,0):edV(0,0,.045);line(edSub(p,d),edAdd(p,d),color);}
        if(selected){double radius=model->lights[i].range;
            for(int j=0;j<48;j++){double a=j*6.283185307179586/48,b=(j+1)*6.283185307179586/48;
                line(edAdd(p,edV(cos(a)*radius,sin(a)*radius,0)),edAdd(p,edV(cos(b)*radius,sin(b)*radius,0)),vertexColor);
                line(edAdd(p,edV(cos(a)*radius,0,sin(a)*radius)),edAdd(p,edV(cos(b)*radius,0,sin(b)*radius)),vertexColor);}
        }
    }
    for(int i=0;i<model->spawnCount+model->objectCount;i++){
        int id=i<model->spawnCount?-i-1:i-model->spawnCount;if(id>=0&&edTerrainObject(model,id)>=0)continue;
        EdVec center=id<0?model->spawns[-id-1].position:model->objects[id].position,end=objectFacingHandle(model,id);if(!thingOverlayVisible(id))continue;
        int selected=tool==3&&(objectGrabbing&&objectRotating?objectGrabbed==id:objectHandleHovered==id);
        line(center,end,facingColor);vertexCircle(end,selected,selected?vertexColor:facingColor);
    }
    for(int i=0;i<model->objectCount+model->spawnCount;i++){
        int parent=i<model->objectCount?i:-(i-model->objectCount)-1;EdThingAttributes *a=thingAttributes(model,parent);
        for(int j=0;j<a->frameCount;j++){int id=frameId(parent,j);EdVec center=a->frames[j].position,end=objectFacingHandle(model,id);if(!thingOverlayVisible(id))continue;int selected=tool==3&&(objectGrabbing?objectGrabbed:objectHovered)==id;
            line(center,end,selectedColor);vertexCircle(end,selected,selected?vertexColor:selectedColor);vertexCircle(center,selected,selectedColor);
            rdVector3 screen;if(project(center,&screen)){char label[64];snprintf(label,sizeof(label),"Thing %d Frame %d",parent<0?-parent-1:model->spawnCount+parent,j);editorDrawText(highlightCanvas->pVBuffer,jkGui_stdFonts[0],(int)screen.x+14,(int)screen.y-22,260,label,1);}
        }
    }
    for(int i=0;i<model->spawnCount;i++){
        EdVec p=model->spawns[i].position;if(!objectOverlayVisible(p))continue;int color=tool==3&&objectHovered==-i-1?selectedColor:vertexColor;
        vertexCircle(p,1,color);line(edAdd(p,edV(-.045,0,0)),edAdd(p,edV(.045,0,0)),color);line(edAdd(p,edV(0,-.045,0)),edAdd(p,edV(0,.045,0)),color);line(p,edAdd(p,edV(0,0,.1)),color);
        rdVector3 screen;if(project(p,&screen)){char label[48];snprintf(label,sizeof(label),"walkplayer %d",i+1);editorDrawText(highlightCanvas->pVBuffer,jkGui_stdFonts[0],(int)screen.x+12,(int)screen.y+8,180,label,1);}
    }
    if(tool==3){int id=objectGrabbing?objectGrabbed:objectHovered;if(id>=0&&id<model->objectCount&&thingOverlayVisible(id)){EdVec p=model->objects[id].position;vertexCircle(p,1,selectedColor);line(edAdd(p,edV(0,0,-.1)),edAdd(p,edV(0,0,.1)),selectedColor);}}
    else if(tool==7){if(terrainHover.cell<0&&hover.cell>=0){EdHit surface=hover;surface.vertex=surface.edge=-1;drawHit(&surface,edgeColor);}}
    else if(tool==4){if(hover.cell>=0){EdHit surface=hover;surface.vertex=surface.edge=-1;drawHit(&surface,edgeColor);}}
    else {
        if(sectorMode&&h->cell>=0&&h->cell<model->count){for(int f=0;f<model->c[h->cell].count;f++){EdHit face={h->cell,f,-1,-1};drawHit(&face,edgeColor);}}
        else if(grabbing&&!groupGrab){for(int i=0;i<grabVisual.count;i++)drawHit(&grabVisual.hits[i],edgeColor);}
        else drawHit(h,h->vertex>=0?vertexColor:edgeColor);
    }
    if(tool!=1||!selectionCleanup)for(int i=0;i<picks.count;i++)drawHit(&picks.hits[i],selectedColor);
    if(tool==1&&!selectionCleanup)for(int s=0;s<model->count;s++)if(sectorPicks[s])for(int f=0;f<model->c[s].count;f++){EdHit face={s,f,-1,-1};drawHit(&face,selectedColor);}
    drawEditorCrosshair();
    stdDisplay_VBufferUnlock(highlightCanvas->pVBuffer);
    if(tool==2||tool==3) {
        tVBuffer *buffer=highlightCanvas->pVBuffer;int x=buffer->format.width-190,y=buffer->format.height-260;
        if(x<8)x=8;if(y<60)y=60;
        if(tool==3)drawObjectPreview(buffer,currentTemplate,x+46,y,96,96);else drawMaterialPreview(buffer,currentMaterial,x+46,y,96,96);
        char label[160];snprintf(label,sizeof(label),"%d/%d %s",tool==3?currentTemplate+1:currentMaterial+1,tool==3?templateCount:paletteCount,tool==3?templateNames[currentTemplate]:paletteNames[currentMaterial]);
        stdDisplay_VBufferLock(buffer);editorDrawText(buffer,jkGui_stdFonts[0],x,y+100,182,label,1);stdDisplay_VBufferUnlock(buffer);
    }
    if(tool==1){tVBuffer *buffer=highlightCanvas->pVBuffer;int x=buffer->format.width-280,y=buffer->format.height-175;if(x<8)x=8;if(y<60)y=60;stdDisplay_VBufferLock(buffer);editorDrawText(buffer,jkGui_stdFonts[0],x,y,270,selectionCleanup?"Clean Up":"Select",1);editorDrawText(buffer,jkGui_stdFonts[0],x,y+24,270,"Middle mouse: selection tools",1);editorDrawText(buffer,jkGui_stdFonts[0],x,y+48,270,"Wheel: Select / Clean Up",1);if(selectionCleanup)editorDrawText(buffer,jkGui_stdFonts[0],x,y+72,270,"Fire 1: clean aimed sector",1);else {editorDrawText(buffer,jkGui_stdFonts[0],x,y+72,270,"Ctrl+click: sectors | M: merge",1);editorDrawText(buffer,jkGui_stdFonts[0],x,y+96,270,"Enter: attributes | Shift+wheel: scale",1);}stdDisplay_VBufferUnlock(buffer);}
    if(tool==10){tVBuffer *buffer=highlightCanvas->pVBuffer;char name[48],text[128];shapeLabel(cleaveShape,name,sizeof(name));snprintf(text,sizeof(text),"%s | radius %.4g",name,shapeRadius);int x=buffer->format.width-280,y=buffer->format.height-180;if(x<8)x=8;if(y<60)y=60;stdDisplay_VBufferLock(buffer);editorDrawText(buffer,jkGui_stdFonts[0],x,y,270,text,1);editorDrawText(buffer,jkGui_stdFonts[0],x,y+24,270,"Middle mouse: shapes",1);editorDrawText(buffer,jkGui_stdFonts[0],x,y+48,270,"Wheel: shape | Shift: radius",1);snprintf(text,sizeof(text),"Ctrl+wheel: rotate (%.0f deg)",shapeRotation);editorDrawText(buffer,jkGui_stdFonts[0],x,y+72,270,text,1);stdDisplay_VBufferUnlock(buffer);}
    if(tool==7){tVBuffer *buffer=highlightCanvas->pVBuffer;int x=(std::max)(8,buffer->format.width-320),y=(std::max)(60,buffer->format.height-175);char label[128];snprintf(label,sizeof(label),"Terrain | brush radius %.3g",terrainRadius);stdDisplay_VBufferLock(buffer);editorDrawText(buffer,jkGui_stdFonts[0],x,y,310,label,1);editorDrawText(buffer,jkGui_stdFonts[0],x,y+24,310,"Fire 1: create / raise | Shift: lower",1);editorDrawText(buffer,jkGui_stdFonts[0],x,y+48,310,"Wheel: radius | Middle: terrain menu",1);stdDisplay_VBufferUnlock(buffer);}
    if(tool==4){
        tVBuffer *buffer=highlightCanvas->pVBuffer;int id=lightGrabbing?lightGrabbed:lightHovered,x=buffer->format.width-250,y=buffer->format.height-210;char text[128];
        if(x<8)x=8;if(y<60)y=60;stdDisplay_VBufferLock(buffer);
        if(id>=0&&id<model->lightCount){snprintf(text,sizeof(text),"Light %d / %d",id+1,model->lightCount);editorDrawText(buffer,jkGui_stdFonts[0],x,y,240,text,1);
            snprintf(text,sizeof(text),"Intensity: %.1f",model->lights[id].intensity);editorDrawText(buffer,jkGui_stdFonts[0],x,y+22,240,text,1);
            snprintf(text,sizeof(text),"Range: %.4g",model->lights[id].range);editorDrawText(buffer,jkGui_stdFonts[0],x,y+44,240,text,1);
        }else {editorDrawText(buffer,jkGui_stdFonts[0],x,y,240,"Fire 1: place light",1);if(hover.cell>=0){snprintf(text,sizeof(text),"Sector %d light: %.2f",hover.cell,model->c[hover.cell].extraLight);editorDrawText(buffer,jkGui_stdFonts[0],x,y+22,240,text,1);}}
        editorDrawText(buffer,jkGui_stdFonts[0],x,y+70,240,id>=0?"Wheel: intensity":"Wheel: sector light",1);editorDrawText(buffer,jkGui_stdFonts[0],x,y+92,240,id>=0?"Shift+wheel: range":"Shift+wheel: surface light",1);stdDisplay_VBufferUnlock(buffer);
    }
    stdDisplay_VBufferLock(Video_pMenuBuffer);
    char status[256];snprintf(status,sizeof(status),"EDITOR | %s | %d sectors | %d selected | %s",tool==1?"Selection":tool==2?"Materials":tool==3?"Objects":tool==4?"Lights":tool==5?"Placed COGs":tool==7?"Terrain":tool==8?"Templates (MMB)":tool==9?"3DO Builder":tool==10?"Saber":"Reserved",model->count,picks.count+selectedSectorCount(),sectorMode||selectedSectorCount()?"SECTOR":tool==5?"Middle mouse: placed COGs":tool==4?"LIGHT":h->vertex>=0?"VERTEX":h->edge>=0?"EDGE":"SURFACE");
    editorDrawText(Video_pMenuBuffer,jkGui_stdFonts[0],8,8,630,status,1);editorDrawText(Video_pMenuBuffer,jkGui_stdFonts[0],8,28,630,message,1);
    stdDisplay_VBufferUnlock(Video_pMenuBuffer);
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return;}
static int parseHeaderNumbers(const char16_t *text,double *out,int count) {
    char ascii[160];stdString_WcharToChar(ascii,text,sizeof(ascii));char *p=ascii;
    for(int i=0;i<count;i++){char *end;double value=strtod(p,&end);if(end==p||!isfinite(value)||fabs(value)>1000000)return 0;out[i]=value;p=end;}
    while(isspace((unsigned char)*p))p++;return *p==0;
}
static int headerText(char *out,size_t size,const char16_t *text) {
    for(int i=0;text[i];i++)if(text[i]>255||text[i]=='"'||text[i]=='\r'||text[i]=='\n')return 0;
    stdString_WcharToChar(out,text,(int)size);return 1;
}
static void captureHeaderMenu(jkGuiMenu *menu) {
    static int frames,stage;if(++frames%3)return;
#ifdef SDL2_RENDER
    char path[512];const char *base=getenv("GEOSITH_HEADER_CAPTURE");
    if(!stage)snprintf(path,sizeof(path),"%s",base);else if(stage==1)snprintf(path,sizeof(path),"%s.rendering.png",base);else snprintf(path,sizeof(path),"%s.intro%d.png",base,stage-2);
    std3D_ScreenshotWindow(path);
#endif
    menu->lastClicked=stage==0?11:stage==1?12:stage<5?21:1;
    if(stage==5){printf("GEOSITH_HEADER_MENU PASS: general, rendering, 20 intro lines, apply\n");fflush(stdout);}stage++;
}
void jkEditor_HeaderDialog(void) try {
    if(!jkEditor_IsLevel()||!bindEditorWorld())return;
    EdLevelHeader draft=model->header;int page=0,introPage=0;
    for(;;) {
        const char16_t *general[]={u"Level name",u"Colormap",u"World gravity",u"Ceiling sky Z",u"Horizon distance",u"Horizon pixels / rev"};
        const char16_t *render[]={u"Horizon offset (x y)",u"Ceiling offset (x y)",u"Mip distances (4)",u"LOD distances (4)",u"Perspective distance",u"Gouraud distance"};
        char16_t values[6][160]={{0}},introLabels[5][32];double *numbers[6]={0};int dimensions[6]={1,1,1,1,1,1};
        if(page==0){stdString_CharToWchar(values[0],draft.title,160);stdString_CharToWchar(values[1],draft.colormap,160);numbers[2]=&draft.gravity;numbers[3]=&draft.ceilingSkyZ;numbers[4]=&draft.horizonDistance;numbers[5]=&draft.horizonPixels;}
        else if(page==1){numbers[0]=draft.horizonOffset;numbers[1]=draft.ceilingOffset;numbers[2]=draft.mip;numbers[3]=draft.lod;numbers[4]=&draft.perspective;numbers[5]=&draft.gouraud;dimensions[0]=dimensions[1]=2;dimensions[2]=dimensions[3]=4;}
        else for(int i=0;i<5;i++){int line=introPage*5+i;char label[32];snprintf(label,sizeof(label),"Intro line %d / 20",line+1);stdString_CharToWchar(introLabels[i],label,32);stdString_CharToWchar(values[i],draft.intro[line],160);}
        for(int i=0;i<6;i++)if(numbers[i]){char text[160];size_t used=0;for(int n=0;n<dimensions[i];n++)used+=snprintf(text+used,sizeof(text)-used,"%s%.9g",n?" ":"",numbers[i][n]);stdString_CharToWchar(values[i],text,160);}
        jkGuiElement elements[22]={0};
        elements[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,12,600,40},1};
        for(int i=0;i<3;i++)elements[1+i]=jkGuiElement{ELEMENT_TEXTBUTTON,10+i,2,NULL,3,{20+i*200,56,194,36},1};
        for(int i=0;i<6;i++) {
            int visible=page<2||i<5;
            elements[4+i]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,110+i*36,210,30},visible};
            elements[10+i]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,159,{240,108+i*36,378,30},visible};
        }
        if(page==0){elements[10].selectedTextEntry=95;elements[11].type=ELEMENT_TEXTBUTTON;elements[11].selectedTextEntry=3;elements[11].hoverId=200;}
        elements[16]=jkGuiElement{ELEMENT_TEXTBUTTON,20,0,NULL,3,{20,308,180,32},page==2};
        elements[17]=jkGuiElement{ELEMENT_TEXTBUTTON,21,0,NULL,3,{440,308,180,32},page==2};
        elements[18]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{340,414,220,40},1};
        elements[19]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{80,414,220,40},1};
        elements[20]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,352,600,54},1};elements[21].type=ELEMENT_END;
        jkGuiMenu menu={elements,0,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};
        if(getenv("GEOSITH_HEADER_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))menu.idkFunc=captureHeaderMenu;
        jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);elements[0].wstr=u"Level Header";elements[1].wstr=u"General";elements[2].wstr=u"Rendering";elements[3].wstr=u"Intro Text";
        for(int i=0;i<6;i++){elements[4+i].wstr=page==0?general[i]:page==1?render[i]:i<5?introLabels[i]:u"";elements[10+i].wstr=values[i];}
        elements[16].wstr=u"Previous lines";elements[17].wstr=u"Next lines";elements[18].wstr=u"Apply";
        elements[20].wstr=u"JK format version 1. Save Level to keep changes.\nApply updates the running level, including its colormap.";
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[19]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&elements[18]);
        int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(elements);if(choice<0)break;
        int valid=1;
        for(int i=0;i<6;i++)if(numbers[i])valid=valid&&parseHeaderNumbers(values[i],numbers[i],dimensions[i]);
        if(page==0)valid=valid&&headerText(draft.title,sizeof(draft.title),values[0])&&draft.title[0];
        if(page==2)for(int i=0;i<5;i++)valid=valid&&headerText(draft.intro[introPage*5+i],160,values[i]);
        valid=valid&&draft.gravity>=0&&draft.horizonDistance>0&&draft.horizonPixels>0&&draft.perspective>=0&&draft.gouraud>=0;
        for(int i=0;i<4;i++)valid=valid&&draft.mip[i]>=0&&draft.lod[i]>=0;
        if(!valid){jkGuiDialog_ErrorDialog(u"Level Header",u"Enter finite numeric values and a level name.\nText cannot contain double quotes. Distances must be positive.");continue;}
        if(choice==200) {
            char16_t names[EDITOR_COLORMAPS][128];int current=0;
            for(int i=0;i<colormapCount;i++){stdString_CharToWchar(names[i],editorColormaps[i].name,128);if(!__strcmpi(draft.colormap,editorColormaps[i].name))current=i;}
            int id=chooseList(u"Level Colormap",names,colormapCount,current);
            if(id>=0){colormapColors(id);if(editorColormaps[id].loaded==1)snprintf(draft.colormap,sizeof(draft.colormap),"%s",editorColormaps[id].name);else jkGuiDialog_ErrorDialog(u"Colormap",u"Could not load that colormap.");}
        } else if(choice>=10&&choice<=12)page=choice-10;
        else if(choice==20){if(introPage>0)introPage--;}
        else if(choice==21){if(introPage<3)introPage++;}
        else if(choice==1){remember();edCopyModel(candidate,model);candidate->header=draft;if(commit()){say("Level header updated. Use Save Level to keep changes.");break;}}
    }
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return;}
#include "jkEditorSurface.h"
#include "jkEditorSector.h"
#include "jkEditorEpisode.h"
#include "jkEditorCogs.h"
#include "jkEditorThings.h"
#include "jkEditorNet.h"
#include "jkEditorNetTest.h"
void jkEditor_SaveDialog(void) try {
    if(!jkEditor_IsLevel()||!bindEditorWorld())return;
    if(netEditPending()){jkGuiDialog_ErrorDialog(u"Save Level",u"Wait for the host to finish checking your edit before saving.");return;}
    jkGuiElement elements[]={
        {ELEMENT_TEXT,0,2,NULL,3,{80,130,480,40},1},
        {ELEMENT_TEXTBOX,0,0,NULL,31,{120,200,400,30},1},
        {ELEMENT_TEXTBUTTON,1,2,"GUI_SAVE",3,{340,300,180,40},1},
        {ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{120,300,180,40},1},
        {ELEMENT_END,0}
    };
    jkGuiMenu menu={elements,0,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};
    jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);elements[0].wstr=u"Save Level (letters, numbers, _ or -)";elements[1].wstr=levelName;
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[3]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&elements[2]);
    while(jkGuiRend_DisplayAndReturnClicked(&menu)==1) {
        char name[32];int valid=editorLevelFilename(levelName,name);
        if(!valid) {jkGuiDialog_ErrorDialog(u"Save Level",u"Use letters, numbers, underscores or hyphens.");continue;}
        if(stockEpisode(name)||(protectedEpisode[0]&&!__strcmpi(name,protectedEpisode))){jkGuiDialog_ErrorDialog(u"Save Level",u"Stock episodes are protected. Save under a different name, such as the suggested _edited name.");continue;}
        char path[128],temp[128];snprintf(path,sizeof(path),"Episode/%s.gob",name);snprintf(temp,sizeof(temp),"Episode/%s.gob.tmp",name);
        FILE *exists=fopen(path,"rb");if(exists) {fclose(exists);if(!jkGuiDialog_YesNoDialog(u"Save Level",u"Replace the existing level with this name?"))continue;}
        edCopyModel(candidate,model);
        int ok=writeEditorGob(candidate,temp,name,multi);
        char currentEpisode[32];snprintf(currentEpisode,sizeof(currentEpisode),"%s",jkRes_episodeGobName);
        int replacingCurrent=!__strcmpi(name,currentEpisode);
        if(ok&&replacingCurrent)jkRes_FreeGobs(1);
#ifdef _WIN32
        if(ok)ok=MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        if(ok)ok=rename(temp,path)==0;
#endif
        if(replacingCurrent)jkRes_LoadGob(currentEpisode);
        if(!ok) {jkGuiDialog_ErrorDialog(u"Save Level",u"Could not save the episode GOB.");continue;}
        if(replacingCurrent&&model->episode.count)snprintf(sithWorld_g_pCurrentWorld->map_jkl_fname,sizeof(sithWorld_g_pCurrentWorld->map_jkl_fname),"%s",model->episode.rows[model->episode.current].file);
        say("Level saved to the Episode folder.");break;
    }
    for(int i=0;elements[i].type!=ELEMENT_END;i++){if(elements[i].strAlloced)STD_FREE((void*)elements[i].strAlloced);if(elements[i].wHintTextAlloced)STD_FREE((void*)elements[i].wHintTextAlloced);}
} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return;}
void jkEditor_Close(void) try {terrainSelected=-1;terrainStroke=0;terrainHover.cell=-1;splineSelected=splineDrawing=splineDragging=splineStartDown=splineEnterDown=0;splinePoint=-1;oppositeAdjoinDown=0;flightSpeedBoost=flightSpeedDown=0;sectorPicks.clear();mergeDown=0;liveSectorColormaps.clear();failedImportWorld=NULL;importingStock=0;protectedEpisode[0]=0;edFree(resources);resources=NULL;builderAsset=-1;builderExitRequested=0;builderFreeCache();freeEditorText();if(mapActive)mapClose(boundWorld?boundWorld->pLocalPlayer:NULL);edFree(mapClipboard);mapClipboard=NULL;edFree(mapPending);mapPending=NULL;mapInvalidate();netClose();closeCogEditor();if(active)g_debugmodeFlags=savedDebug;active=0;boundWorld=NULL;grabbing=groupGrab=cleaving=objectGrabbing=objectRotating=0;objectHandleHovered=-10000;clearHistory();toggleDown=fireDown=grabDown=homeDown=tabDown=qDown=nextSpawn=deleteDown=middleDown=attributesDown=0;picks.count=grabPicks.count=undoPicks.count=0;edResetDoubleTap(&jumpTap,0);edResetDoubleTap(&selectionTap,0);hover.cell=-1;clearObjectPreviews();freeEditorMaterials();if(bladeReady){rdPolyline_FreeEntry(&cleaveBlade);edZero(&cleaveBlade,0,sizeof(cleaveBlade));bladeReady=0;}} catch(const std::bad_alloc&) {say("Editor operation stopped: not enough memory.");return;}
