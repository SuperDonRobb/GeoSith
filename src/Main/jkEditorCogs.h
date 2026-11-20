#include "edHazardScript.h"
#include "edTeleporterScript.h"
#include "edWindScript.h"
#include "edDoorScript.h"
#include "edDestructibleScript.h"
#include "edWaterThrustScript.h"
/* Placed COG authoring owns a draft across the temporary world-picking mode. */
#define EDITOR_COG_SOURCES 2048
typedef struct {char file[32],archive[260];unsigned int offset,size;} EditorCogSource;
typedef struct {int count;EdBuffer<EdPlacedCog> cogs;} EditorCogDraft;
static EditorCogSource cogSources[EDITOR_COG_SOURCES];
static int cogSourceCount,cogSelected,cogPage,cogParamPage,cogPickParam=-1,cogPickCancelled,cogPickThing=-10000;
static EditorCogDraft *cogDraft;
static EdHit cogPickHit;
static int ownedCogSlots[ED_COGS],ownedCogCount;
/* Placed COG runtime slots differ after a bootstrap join. The document
 * ordinal is shared by all peers, unlike those local allocation indices. */
int jkEditor_NetCogDocumentId(const sithCog *cog){
    SithWorld *w=sithWorld_g_pCurrentWorld;if(!sithNet_isMulti||!jkEditor_IsLevel()||!model||!w||!cog)return -1;
    for(int i=0;i<ownedCogCount&&i<model->cogCount;i++)if(ownedCogSlots[i]>=0&&ownedCogSlots[i]<(int)w->numCogs&&cog==&w->aCogs[ownedCogSlots[i]])return i;return -1;
}
sithCog *jkEditor_NetDocumentCog(int id){
    SithWorld *w=sithWorld_g_pCurrentWorld;if(!sithNet_isMulti||!jkEditor_IsLevel()||!model||!w||id<0||id>=model->cogCount||id>=ownedCogCount)return NULL;
    int slot=ownedCogSlots[id];return slot>=0&&slot<(int)w->numCogs?&w->aCogs[slot]:NULL;
}
static uint32_t cogRead32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int cogSourceCompare(const void *a,const void *b){return __strcmpi(((const EditorCogSource*)a)->file,((const EditorCogSource*)b)->file);}
static void addCogSource(const char *file,const char *archive,unsigned int offset,unsigned int size){
    file=baseName(file);if(strlen(file)>31||strlen(file)<5||__strcmpi(file+strlen(file)-4,".cog"))return;
    for(int i=0;i<cogSourceCount;i++)if(!__strcmpi(cogSources[i].file,file))return;if(cogSourceCount>=EDITOR_COG_SOURCES)return;
    EditorCogSource *source=&cogSources[cogSourceCount++];edZero(source,0,sizeof(*source));snprintf(source->file,32,"%s",file);snprintf(source->archive,260,"%s",archive?archive:"");source->offset=offset;source->size=size;
}
static void scanCogArchive(const char *path){FILE *f=fopen(path,"rb");if(!f)return;unsigned char header[12],countData[4],entry[136];
    if(fread(header,1,12,f)==12&&!edCompare(header,"GOB ",4)&&!fseek(f,cogRead32(header+8),SEEK_SET)&&fread(countData,1,4,f)==4){unsigned int count=cogRead32(countData);if(count<=100000)for(unsigned int i=0;i<count&&fread(entry,1,136,f)==136;i++){entry[135]=0;const char *name=(const char*)entry+8;if(!__strnicmp(name,"cog\\",4))addCogSource(name,path,cogRead32(entry),cogRead32(entry+4));}}fclose(f);
}
static void discoverCogs(void){
    if(cogSourceCount)return;
    for(auto &f:customResources.files)if(f.first.starts_with("cog/"))addCogSource(f.first.c_str(),NULL,0,0);
    for(int d=0;d<5;d++){jkResGobDirectory *dir=&jkRes_gCtx.aGobDirectories[d];for(int g=0;g<dir->numGobs;g++){
#ifndef STDGOB_COMPACT_ENTRIES
        Gob *gob=dir->gobs[g];if(gob)for(unsigned int i=0;i<gob->numFiles;i++)if(!__strnicmp(gob->entries[i].fname,"cog\\",4))addCogSource(gob->entries[i].fname,gob->fpath,gob->entries[i].fileOffset,gob->entries[i].fileSize);
#endif
    }}
    scanCogArchive("Episode/JK1.GOB");scanCogArchive("Episode/JK1MP.GOB");
    const char *folders[]={"Resource","Episode"};for(int d=0;d<2;d++){stdFileSearch *search=stdFileUtil_NewFind(folders[d],3,"gob");stdFileSearchResult entry;if(search){while(stdFileUtil_FindNext(search,&entry))if(!entry.is_subdirectory){char path[300];snprintf(path,sizeof(path),"%s/%s",folders[d],baseName(entry.fpath));scanCogArchive(path);}stdFileUtil_DisposeFind(search);}}
    for(int d=0;d<5;d++){char path[300];snprintf(path,sizeof(path),"%s%scog",jkRes_gCtx.aGobDirectories[d].name,jkRes_gCtx.aGobDirectories[d].name[0]?"/":"");stdFileSearch *search=stdFileUtil_NewFind(path,3,"cog");stdFileSearchResult entry;if(search){while(stdFileUtil_FindNext(search,&entry))if(!entry.is_subdirectory)addCogSource(entry.fpath,NULL,0,0);stdFileUtil_DisposeFind(search);}}
    qsort(cogSources,cogSourceCount,sizeof(cogSources[0]),cogSourceCompare);
}
static char *cogSourceText(const char *name){if(!__strcmpi(name,"gs_hazard.cog")){char *copy=edAlloc(sizeof(edHazardScript));if(copy)memcpy(copy,edHazardScript,sizeof(edHazardScript));return copy;}if(!__strcmpi(name,"gs_wind.cog")){char *copy=edAlloc(sizeof(edWindScript));if(copy)memcpy(copy,edWindScript,sizeof(edWindScript));return copy;}if(!__strcmpi(name,"gs_teleport.cog")){char *copy=edAlloc(sizeof(edTeleporterScript));if(copy)memcpy(copy,edTeleporterScript,sizeof(edTeleporterScript));return copy;}if(!__strcmpi(name,"gs_door.cog")){char *copy=edAlloc(sizeof(edDoorScript));if(copy)memcpy(copy,edDoorScript,sizeof(edDoorScript));return copy;}if(!__strcmpi(name,"gs_waterthrust.cog")){char *copy=edAlloc(sizeof(edWaterThrustScript));if(copy)memcpy(copy,edWaterThrustScript,sizeof(edWaterThrustScript));return copy;}if(!__strcmpi(name,"gs_destructible.cog")){char *copy=edAlloc(sizeof(edDestructibleScript));if(copy)memcpy(copy,edDestructibleScript,sizeof(edDestructibleScript));return copy;}auto custom=customResources.files.find(EdResources::path(std::string("cog/")+name));if(custom!=customResources.files.end()){char *copy=edAlloc(custom->second->size()+1);if(copy){memcpy(copy,custom->second->data(),custom->second->size());copy[custom->second->size()]=0;}return copy;}char *networkSource=netCogSource(name);if(networkSource)return networkSource;char path[80];snprintf(path,sizeof(path),"cog\\%s",name);stdFile_t file=jkRes_FileOpen(path,"rb");char *text=NULL;
    if(file){int size=jkRes_FileSize(file);if(size>0&&size<2*1024*1024){text=edAlloc(size+1);if(text){if(jkRes_FileRead(file,text,size)==size)text[size]=0;else{edFree(text);text=NULL;}}}jkRes_FileClose(file);if(text)return text;}
    discoverCogs();for(int i=0;i<cogSourceCount;i++){EditorCogSource *s=&cogSources[i];if(__strcmpi(s->file,name)||!s->archive[0]||!s->size||s->size>2*1024*1024)continue;FILE *f=fopen(s->archive,"rb");if(!f)continue;text=edAlloc(s->size+1);if(text){if(!fseek(f,s->offset,SEEK_SET)&&fread(text,1,s->size,f)==s->size)text[s->size]=0;else{edFree(text);text=NULL;}}fclose(f);if(text)return text;}return NULL;
}
static int ensureCogSource(const char *name){char path[80];snprintf(path,sizeof(path),"cog\\%s",name);stdFile_t file=jkRes_FileOpen(path,"rb");if(file){jkRes_FileClose(file);return 1;}char *text=cogSourceText(name);if(!text)return 0;stdFileUtil_MkDir("cog");snprintf(path,sizeof(path),"cog/%s",name);FILE *out=fopen(path,"wb");int ok=out&&fwrite(text,1,strlen(text),out)==strlen(text);if(out&&fclose(out))ok=0;edFree(text);return ok;}
static int loadPlacedCog(const char *name,EdPlacedCog *cog){char *text=cogSourceText(name);int ok=text&&edCogParseScript(cog,name,text);edFree(text);if(ok)for(int i=0;i<cog->count;i++){EdCogParam *p=&cog->params[i];if(!edCogSetValue(model,p,p->value)){if(p->type==ED_COG_VECTOR)snprintf(p->value,32,"(0/0/0)");else snprintf(p->value,32,"%s",p->type==ED_COG_INT||p->type==ED_COG_FLEX?"0":"-1");}}return ok&&ensureCogSource(name);}
static int writeCogSources(const EdModel *source,const char *path){for(int i=0;i<source->cogCount;i++){int duplicate=0;for(int j=0;j<i;j++)if(!__strcmpi(source->cogs[i].file,source->cogs[j].file))duplicate=1;if(duplicate)continue;char *text=cogSourceText(source->cogs[i].file);if(!text)return 0;char name[64];snprintf(name,sizeof(name),"cog\\%s",source->cogs[i].file);int ok=edGobAddFile(path,name,text,(unsigned int)strlen(text));edFree(text);if(!ok)return 0;}return 1;}
static int readPlacedCogs(EdModel *m,const char *text){
    int section=0;m->cogCount=0;
    for(const char *p=text;*p;){const char *end=strchr(p,'\n');size_t length=end?(size_t)(end-p):strlen(p);char line[8192];if(length>=sizeof(line))return 0;edCopy(line,p,length);line[length]=0;p=end?end+1:p+length;
        char lower[128];snprintf(lower,sizeof(lower),"%.127s",line);for(char *s=lower;*s;s++)*s=(char)tolower((unsigned char)*s);char name[64];if(sscanf(lower," section: %63s",name)==1){section=!strcmp(name,"cogs");continue;}if(!section)continue;
        int index,used=0;char file[32];if(sscanf(line," %d: %31s %n",&index,file,&used)!=2)continue;if(m->cogCount>=ED_COGS||index!=m->cogCount)return 0;
        EdPlacedCog *c=&m->cogs[m->cogCount++];if(!loadPlacedCog(file,c))return 0;char *cursor=line+used;
        for(int i=0;i<c->count;i++){while(isspace((unsigned char)*cursor))cursor++;char value[32];int n=0;if(*cursor&&*cursor!='#'){if(sscanf(cursor,"%31s%n",value,&n)!=1)return 0;cursor+=n;}else snprintf(value,32,"%s",c->params[i].value);
            if(importingStock){
                EdCogParam *param=&c->params[i];int oldId=atoi(value),mapped=-1;
                if(oldId>=0&&param->type==ED_COG_THING){
                    for(int j=0;j<m->spawnCount;j++)if(jkPlayer_playerInfos[j].pLocalPlayer&&jkPlayer_playerInfos[j].pLocalPlayer->idx==oldId)mapped=j;
                    for(int j=0;j<m->objectCount;j++)if(objectThings[j]==oldId)mapped=m->spawnCount+j;
                    snprintf(value,32,"%d",mapped);
                }else if(oldId>=0&&param->type==ED_COG_SURFACE){
                    SithWorld *w=sithWorld_g_pCurrentWorld;int index=0;for(int j=0;j<m->count;j++){for(int k=0;k<m->c[j].count;k++)if(w->aSectors[j].surfaces[k].index==oldId)mapped=index+k;index+=m->c[j].count;}
                    if(mapped<0)return 0;snprintf(value,32,"%d",mapped);
                }
            }
            if(!edCogSetValue(m,&c->params[i],value)){if(c->params[i].type==ED_COG_COG){snprintf(c->params[i].value,32,"%s",value);}else return 0;}}
    }
    for(int i=0;i<m->cogCount;i++)for(int j=0;j<m->cogs[i].count;j++){EdCogParam *p=&m->cogs[i].params[j];if(p->type==ED_COG_COG&&!edCogSetValue(m,p,p->value))return 0;}
    ownedCogCount=m->cogCount;for(int i=0;i<ownedCogCount;i++)ownedCogSlots[i]=i;return 1;
}
static int cogPicking(void){return cogDraft&&cogPickParam>=0;}
int jkEditor_HandleEscape(void){if(active&&spiralCancel())return 1;if(active&&bevelCancel())return 1;if(active&&windCancel())return 1;if(active&&conveyorCancel())return 1;if(active&&doorCancel())return 1;if(active&&waterSimFinish())return 1;if(active&&presetEscape())return 1;if(active&&terrainSessionOpen()){terrainSessionFinish();return 1;}if(active&&builderAsset>=0){if(builderInverting){builderInverting=0;say("Mesh invert cancelled.");return 1;}builderExitRequested=1;return 1;}if(active&&mapActive){int editing=splineDrawing||splineDragging||mapCleaveMode||mapDrawing||mapUnplaced||mapDragging||mapBox;if(editing)mapEscape();return editing?1:0;}if(active&&cogPicking()){cogPickCancelled=1;return 1;}return 0;}
static EdCogParam *pickingCogParam(void){return cogPicking()&&cogSelected<cogDraft->count&&cogPickParam<cogDraft->cogs[cogSelected].count?&cogDraft->cogs[cogSelected].params[cogPickParam]:NULL;}
static void updateCogPick(EdVec origin,EdVec direction){EdCogParam *param=pickingCogParam();cogPickThing=-10000;cogPickHit.cell=-1;if(!param)return;
    if(param->type==ED_COG_SURFACE||param->type==ED_COG_SECTOR)edRaySelection(model,origin,direction,&cogPickHit);else edRayMaterial(model,origin,direction,&cogPickHit);
    cogPickHit.vertex=cogPickHit.edge=-1;
    if(param->type==ED_COG_THING||param->type==ED_COG_TEMPLATE){EdHit wall;edRay(model,origin,direction,&wall);cogPickThing=pickEditorObject(origin,direction,wall.cell>=0?wall.distance:1000);}
}
static int finishCogPick(void){EdCogParam *p=pickingCogParam();if(!p)return 0;char value[32];
    if(p->type==ED_COG_THING){if(cogPickThing==-10000)return 0;int id=cogPickThing<0?-cogPickThing-1:model->spawnCount+cogPickThing;snprintf(value,32,"%d",id);}
    else if(p->type==ED_COG_TEMPLATE){if(cogPickThing==-10000)return 0;snprintf(value,32,"%s",cogPickThing<0?"walkplayer":model->objects[cogPickThing].templateName);}
    else{if(cogPickHit.cell<0)return 0;
        if(p->type==ED_COG_SECTOR)snprintf(value,32,"%d",cogPickHit.cell);
        else if(p->type==ED_COG_SURFACE){int id=cogPickHit.face;for(int s=0;s<cogPickHit.cell;s++)id+=model->c[s].count;snprintf(value,32,"%d",id);}
        else if(p->type==ED_COG_MATERIAL)snprintf(value,32,"%s",paletteNames[model->c[cogPickHit.cell].f[cogPickHit.face].material]);
        else if(p->type==ED_COG_VECTOR){EdVec point=edSnapFacePoint(&model->c[cogPickHit.cell].f[cogPickHit.face],cogPickHit.point);if(snprintf(value,32,"(%.6g/%.6g/%.6g)",point.x,point.y,point.z)>=32)return 0;}else return 0;
    }
    edCopyModel(candidate,model);candidate->cogCount=cogDraft->count;return edCogSetValue(candidate,p,value);
}
static void drawCogPicker(int color){EdCogParam *p=pickingCogParam();if(!p)return;
    if(p->type==ED_COG_THING||p->type==ED_COG_TEMPLATE){if(cogPickThing!=-10000){EdVec center=cogPickThing<0?model->spawns[-cogPickThing-1].position:model->objects[cogPickThing].position;double radius=.06;if(cogPickThing>=0){SithThing *t=sithTemplate_GetTemplate(model->objects[cogPickThing].templateName);if(t)radius=fmax(radius,t->collideSize);}
            EdVec corners[8];for(int i=0;i<8;i++)corners[i]=edAdd(center,edV((i&1)?radius:-radius,(i&2)?radius:-radius,(i&4)?radius:-radius));for(int i=0;i<8;i++)for(int axis=0;axis<3;axis++)if(!(i&(1<<axis)))line(corners[i],corners[i|(1<<axis)],color);vertexCircle(center,1,color);}}
    else if(cogPickHit.cell>=0){if(p->type==ED_COG_SECTOR){for(int f=0;f<model->c[cogPickHit.cell].count;f++){EdHit hit={cogPickHit.cell,f,-1,-1};drawHit(&hit,color);}}else drawHit(&cogPickHit,color);}
}
static void cogPickPrompt(void){EdCogParam *p=pickingCogParam();if(p){char text[160];snprintf(text,sizeof(text),"Choose a %s for %s. Fire 1 selects; Esc cancels.",p->type==ED_COG_VECTOR?"point":edCogTypeName(p->type),p->name);say(text);}}
static int cogPickControl(SithThing *player){EdVec origin,direction;ray(&origin,&direction);updateCogPick(origin,direction);int fire=editButton(INPUT_FUNC_FIRE1);
    if(cogPickCancelled||(fire&&!fireDown&&finishCogPick())){cogPickCancelled=0;cogPickParam=-1;placedCogsMenu(1);fireDown=grabDown=middleDown=1;return 1;}
    fireDown=fire;grabDown=editButton(INPUT_FUNC_FIRE2);return 1;
}
static void captureCogBrowser(jkGuiMenu *menu){static int frames;if(++frames%3)return;
#ifdef SDL2_RENDER
    char path[512];snprintf(path,sizeof(path),"%s.browser.png",getenv("GEOSITH_COG_CAPTURE"));std3D_ScreenshotWindow(path);
#endif
    menu->lastClicked=100;
}
static int chooseCogScript(void){discoverCogs();int page=0,ids[EDITOR_COG_SOURCES];char16_t query[64]=u"";if(getenv("GEOSITH_COG_CAPTURE"))stdString_SafeWStrCopy(query,u"01_cargoelev1",64);
    for(;;){int count=0;for(int i=0;i<cogSourceCount;i++)if(materialMatches(cogSources[i].file,query))ids[count++]=i;int pages=(count+11)/12;if(!pages)pages=1;if(page>=pages)page=pages-1;
        jkGuiElement e[20]={0};char16_t names[12][64],info[96];char text[96];snprintf(text,sizeof(text),"%d scripts | Page %d / %d",count,page+1,pages);stdString_CharToWchar(info,text,96);
        e[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,10,600,40},1};e[1]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,63,{20,60,450,30},1};e[2]=jkGuiElement{ELEMENT_TEXTBUTTON,4,0,NULL,3,{480,60,140,30},1};
        for(int i=0;i<12;i++){e[3+i]=jkGuiElement{ELEMENT_TEXTBUTTON,100+i,0,NULL,3,{20+(i%2)*305,112+(i/2)*43,295,38},page*12+i<count};if(page*12+i<count)stdString_CharToWchar(names[i],cogSources[ids[page*12+i]].file,64);else names[i][0]=0;}
        e[15]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,377,600,25},1};e[16]=jkGuiElement{ELEMENT_TEXTBUTTON,2,2,NULL,3,{20,423,180,40},1};e[17]=jkGuiElement{ELEMENT_TEXTBUTTON,3,2,NULL,3,{440,423,180,40},1};e[18]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{230,423,180,40},1};e[19].type=ELEMENT_END;
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"Add COG Script";e[1].wstr=query;e[2].wstr=u"Search";for(int i=0;i<12;i++)e[3+i].wstr=names[i];e[15].wstr=info;e[16].wstr=u"Previous";e[17].wstr=u"Next";
        if(getenv("GEOSITH_COG_CAPTURE")&&getenv("GEOSITH_STRESS"))menu.idkFunc=captureCogBrowser;
        jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&e[2]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[18]);int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);
        if(choice>=100&&choice<112&&page*12+choice-100<count)return ids[page*12+choice-100];if(choice==2&&page>0)page--;else if(choice==3&&page+1<pages)page++;else if(choice==4)page=0;else if(choice<0)return -1;
    }
}
static std::string cogSoundTestName;
static void testCogSoundBrowser(jkGuiMenu *menu){static int stage;
    if(stage==0){stdString_SafeWStrCopy(const_cast<char16_t*>(menu->paElements[1].wstr),u"__no_sound_matches__",64);menu->lastClicked=4;}
    else if(stage==1){if(menu->paElements[3].wstr[0])exit(2);stdString_CharToWchar(const_cast<char16_t*>(menu->paElements[1].wstr),cogSoundTestName.c_str(),64);menu->lastClicked=4;}
    else menu->lastClicked=100;stage++;
}
static int chooseCogSound(const std::vector<std::string> &files,const char *current){
    int page=0;for(size_t i=0;i<files.size();i++)if(!__strcmpi(files[i].c_str(),current))page=(int)i/12;
    std::vector<int> ids(files.size());char16_t query[64]=u"";
    for(;;){int count=0;for(int i=0;i<(int)files.size();i++)if(materialMatches(files[i].c_str(),query))ids[count++]=i;int pages=(count+11)/12;if(!pages)pages=1;if(page>=pages)page=pages-1;
        jkGuiElement e[20]={0};char16_t names[12][64],info[96];char text[96];snprintf(text,sizeof(text),"%d sounds | Page %d / %d",count,page+1,pages);stdString_CharToWchar(info,text,96);
        e[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,10,600,40},1};e[1]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,63,{20,60,450,30},1};e[2]=jkGuiElement{ELEMENT_TEXTBUTTON,4,0,NULL,3,{480,60,140,30},1};
        for(int i=0;i<12;i++){e[3+i]=jkGuiElement{ELEMENT_TEXTBUTTON,100+i,0,NULL,3,{20+(i%2)*305,112+(i/2)*43,295,38},page*12+i<count};if(page*12+i<count)stdString_CharToWchar(names[i],files[ids[page*12+i]].c_str(),64);else names[i][0]=0;}
        e[15]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,377,600,25},1};e[16]=jkGuiElement{ELEMENT_TEXTBUTTON,2,2,NULL,3,{20,423,180,40},1};e[17]=jkGuiElement{ELEMENT_TEXTBUTTON,3,2,NULL,3,{440,423,180,40},1};e[18]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{230,423,180,40},1};e[19].type=ELEMENT_END;
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"Choose Sound";if(getenv("GEOSITH_COG_SOUND_TEST")&&getenv("GEOSITH_STRESS"))menu.idkFunc=testCogSoundBrowser;e[1].wstr=query;e[2].wstr=u"Search";for(int i=0;i<12;i++)e[3+i].wstr=names[i];e[15].wstr=info;e[16].wstr=u"Previous";e[17].wstr=u"Next";
        jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&e[2]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[18]);int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);
        if(choice>=100&&choice<112&&page*12+choice-100<count)return ids[page*12+choice-100];if(choice==2&&page>0)page--;else if(choice==3&&page+1<pages)page++;else if(choice==4)page=0;else if(choice<0)return -1;
    }
}
static int cogChosenParam;
static void editCogValue(EdCogParam *param){char16_t input[64];char value[32];edCogParamValue(model,param,value);stdString_CharToWchar(input,value,64);
    for(;;){jkGuiElement e[]={{ELEMENT_TEXT,0,2,NULL,3,{20,40,600,45},1},{ELEMENT_TEXT,0,0,NULL,3,{20,110,600,40},1},{ELEMENT_TEXTBOX,0,0,NULL,31,{80,180,480,40},1},{ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,330,230,40},1},{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,330,230,40},1},{ELEMENT_END,0}};
        char16_t title[96];char text[96];snprintf(text,sizeof(text),"%s (%s)",param->name,edCogTypeName(param->type));stdString_CharToWchar(title,text,96);jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"COG Parameter";e[1].wstr=title;e[2].wstr=input;e[3].wstr=u"Set Value";jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&e[3]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[4]);int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);if(choice<0)return;
        stdString_WcharToChar(value,input,32);edCopyModel(candidate,model);candidate->cogCount=cogDraft->count;if(edCogSetValue(candidate,param,value))return;jkGuiDialog_ErrorDialog(u"COG Parameter",u"Enter a valid value for this type.\nReferences use a level item number or -1. Vectors use (x/y/z).");}
}
static void capturePlacedCogs(jkGuiMenu *menu){static int frames,stage;if(++frames%3)return;
#ifdef SDL2_RENDER
    std3D_ScreenshotWindow(getenv("GEOSITH_COG_CAPTURE"));
#endif
    if(stage==0)menu->lastClicked=12;else if(stage==1)menu->lastClicked=200;else if(stage==2||stage==3)menu->lastClicked=201;else menu->lastClicked=1;stage++;
}
#include "jkEditorCogExternal.h"
static void placedCogsMenu(int resume){
    if(!resume){edFree(cogDraft);cogDraft=edCalloc(1,sizeof(*cogDraft));if(!cogDraft){say("Could not allocate the COG editor.");return;}cogDraft->count=model->cogCount;edCopy(cogDraft->cogs,model->cogs,sizeof(model->cogs));cogSelected=cogPage=cogParamPage=cogChosenParam=0;}
    if(!cogDraft)return;int previousModal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;rdVector_Zero3(&player->physicsParams.vel);rdVector_Zero3(&player->physicsParams.acceleration);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    int picking=0;
    for(;;){if(cogSelected>=cogDraft->count)cogSelected=cogDraft->count?cogDraft->count-1:0;EdPlacedCog *c=cogDraft->count?&cogDraft->cogs[cogSelected]:NULL;if(c&&cogParamPage*8>=c->count)cogParamPage=0;
        char16_t rows[5][96],params[8][160],info[128];char text[256];snprintf(text,sizeof(text),"Placed COGs | %d scripts%s%s",cogDraft->count,c?" | ":"",c?c->file:"");stdString_CharToWchar(info,text,128);
        for(int i=0;i<5;i++){int index=cogPage*5+i;if(index<cogDraft->count){snprintf(text,sizeof(text),"%s%d: %s",index==cogSelected?"> ":"",index,cogDraft->cogs[index].file);stdString_CharToWchar(rows[i],text,96);}else rows[i][0]=0;}
        edCopyModel(candidate,model);candidate->cogCount=cogDraft->count;
        for(int i=0;i<8;i++){int index=cogParamPage*8+i;if(c&&index<c->count){char value[32];edCogParamValue(candidate,&c->params[index],value);snprintf(text,sizeof(text),"%s (%s) = %s",c->params[index].name,edCogTypeName(c->params[index].type),value);stdString_CharToWchar(params[i],text,160);}else params[i][0]=0;}
        jkGuiElement e[27]={0};e[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,8,600,40},1};e[1]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,52,270,28},1};e[2]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{305,52,315,28},1};
        for(int i=0;i<5;i++)e[3+i]=jkGuiElement{ELEMENT_TEXTBUTTON,100+i,0,NULL,3,{20,87+i*44,270,42},cogPage*5+i<cogDraft->count};
        for(int i=0;i<8;i++)e[8+i]=jkGuiElement{ELEMENT_TEXTBUTTON,200+i,0,NULL,3,{305,87+i*33,315,32},c&&cogParamPage*8+i<c->count};
        e[16]=jkGuiElement{ELEMENT_TEXTBUTTON,10,0,NULL,3,{20,320,130,30},1};e[17]=jkGuiElement{ELEMENT_TEXTBUTTON,11,0,NULL,3,{160,320,130,30},1};e[18]=jkGuiElement{ELEMENT_TEXTBUTTON,12,0,NULL,3,{20,359,130,30},1};e[19]=jkGuiElement{ELEMENT_TEXTBUTTON,13,0,NULL,3,{160,359,130,30},c!=NULL};
        e[20]=jkGuiElement{ELEMENT_TEXTBUTTON,14,0,NULL,3,{20,398,270,28},c!=NULL};e[21]=jkGuiElement{ELEMENT_TEXTBUTTON,15,0,NULL,3,{305,359,150,30},c!=NULL};e[22]=jkGuiElement{ELEMENT_TEXTBUTTON,16,0,NULL,3,{470,359,150,30},c!=NULL};e[23]=jkGuiElement{ELEMENT_TEXTBUTTON,17,0,NULL,3,{305,398,150,28},c&&c->count>0};e[24]=jkGuiElement{ELEMENT_TEXTBUTTON,18,0,NULL,3,{470,398,150,28},c&&c->count>0};
        e[25]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,435,230,35},1};e[26]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,435,230,35},1};jkGuiElement all[31]={};edCopy(all,e,sizeof(e));
        // Keep room for resource actions above Apply/Cancel.
        for(int i=3;i<8;i++)all[i].rect={20,83+(i-3)*38,270,36};
        for(int i=8;i<16;i++)all[i].rect={305,83+(i-8)*29,315,28};
        all[16].rect.y=all[17].rect.y=280;
        all[18].rect.y=all[19].rect.y=315;all[20].rect.y=350;
        all[21].rect.y=all[22].rect.y=320;all[23].rect.y=all[24].rect.y=355;
        for(int i=0;i<3;i++)all[27+i]={ELEMENT_TEXTBUTTON,30+i,0,NULL,3,{20+i*205,397,195,28},1};all[30].type=ELEMENT_END;
        jkGuiMenu menu={all,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);all[0].wstr=info;all[1].wstr=u"Scripts";all[2].wstr=u"Click a parameter to assign it";for(int i=0;i<5;i++)all[3+i].wstr=rows[i];for(int i=0;i<8;i++)all[8+i].wstr=params[i];
        const char16_t *labels[]={u"Previous",u"Next",u"Add COG",u"Delete",u"Duplicate",u"Prev params",u"Next params",u"Edit value",u"Clear value"};for(int i=0;i<9;i++)all[16+i].wstr=labels[i];all[25].wstr=u"Apply";all[27].wstr=u"Import COG";all[28].wstr=u"Create COG";all[29].wstr=u"Edit COG";
        menu.idkFunc=cogExternalMenuTick;
        if(getenv("GEOSITH_COG_CAPTURE")&&getenv("GEOSITH_STRESS"))menu.idkFunc=capturePlacedCogs;
        jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&all[25]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&all[26]);int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(all);
        if(choice<0)break;
        if(choice>=30&&choice<=32){cogExternalMenu(choice);continue;}
        if(choice>=100&&choice<105){cogSelected=cogPage*5+choice-100;cogParamPage=cogChosenParam=0;}
        else if(choice==10&&cogPage>0)cogPage--;else if(choice==11&&(cogPage+1)*5<cogDraft->count)cogPage++;
        else if(choice==12||choice==14){if(cogDraft->count>=ED_COGS)jkGuiDialog_ErrorDialog(u"Placed COGs",u"The engine COG instance limit has been reached.");else if(choice==14&&c){cogDraft->cogs[cogDraft->count]=*c;cogSelected=cogDraft->count++;cogPage=cogSelected/5;}else{int id=chooseCogScript();if(id>=0){EdPlacedCog added;if(loadPlacedCog(cogSources[id].file,&added)){cogSelected=cogDraft->count;cogDraft->cogs[cogDraft->count++]=added;cogPage=cogSelected/5;cogParamPage=cogChosenParam=0;}else jkGuiDialog_ErrorDialog(u"Placed COGs",u"Could not read the COG symbol declarations.");}}}
        else if(choice==13&&c){edCopyModel(candidate,model);candidate->cogCount=cogDraft->count;edCopy(candidate->cogs,cogDraft->cogs,sizeof(candidate->cogs));edCogDelete(candidate,cogSelected);cogDraft->count=candidate->cogCount;edCopy(cogDraft->cogs,candidate->cogs,sizeof(candidate->cogs));if(cogPage*5>=cogDraft->count)cogPage=cogDraft->count?(cogDraft->count-1)/5:0;}
        else if(choice==15&&cogParamPage>0)cogParamPage--;else if(choice==16&&c&&(cogParamPage+1)*8<c->count)cogParamPage++;
        else if((choice>=200&&choice<208)||choice==17||choice==18){if(!c||!c->count)continue;int index=choice>=200?cogParamPage*8+choice-200:cogChosenParam;if(index>=c->count)continue;cogChosenParam=index;EdCogParam *param=&c->params[index];
            if(choice==18){edCopyModel(candidate,model);candidate->cogCount=cogDraft->count;edCogSetValue(candidate,param,param->type==ED_COG_VECTOR?"(0/0/0)":param->type==ED_COG_INT||param->type==ED_COG_FLEX?"0":"-1");}
            else if(choice==17)editCogValue(param);
            else if(param->type==ED_COG_THING||param->type==ED_COG_SURFACE||param->type==ED_COG_SECTOR||param->type==ED_COG_MATERIAL||param->type==ED_COG_TEMPLATE||param->type==ED_COG_VECTOR){cogPickParam=index;cogPickCancelled=0;picking=1;break;}
            else if(param->type==ED_COG_SOUND){auto files=sectorSoundResources();int id=chooseCogSound(files,param->value);if(id>=0){
                if(!sithSound_Load((char*)files[id].c_str(),0))jkGuiDialog_ErrorDialog(u"Sound",u"Could not load this sound resource.");
                else edCogSetValue(candidate,param,files[id].c_str());}}
            else if(param->type==ED_COG_COG){std::vector<std::array<char16_t,128>> names(cogDraft->count);for(int i=0;i<cogDraft->count;i++){char name[96];snprintf(name,sizeof(name),"%d: %s",i,cogDraft->cogs[i].file);stdString_CharToWchar(names[i].data(),name,128);}int id=chooseList(u"Choose a Placed COG",(char16_t (*)[128])names.data(),cogDraft->count,0);if(id>=0)snprintf(param->value,32,"%d",id);}
            else editCogValue(param);
        }
        else if(choice==1){edCopyModel(candidate,model);candidate->cogCount=cogDraft->count;edCopy(candidate->cogs,cogDraft->cogs,sizeof(candidate->cogs));if(!edValidate(candidate)){jkGuiDialog_ErrorDialog(u"Placed COGs",u"One or more parameter values are invalid. Set a valid value or clear the reference.");continue;}remember();if(commit()){say("Placed COGs updated. F8 play-tests scripts; Save Level keeps them.");break;}}
    }
    jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=previousModal;stdControl_SetActivation(1);stdControl_Flush();fireDown=grabDown=middleDown=1;
    if(picking)cogPickPrompt();else{edFree(cogDraft);cogDraft=NULL;cogPickParam=-1;}
}
struct EditorCogSound {sithCog *cog;int synced;};
static std::map<int,EditorCogSound> editorCogSounds;
void jkEditor_CogSoundStarted(sithCog *cog,sithPlayingSound *sound,int synced) try {
    if(!cog||!sound||!jkEditor_IsLevel())return;
    for(auto i=editorCogSounds.begin();i!=editorCogSounds.end();)if(!sithSoundMixer_GetChannelHandle(i->first))i=editorCogSounds.erase(i);else ++i;
    editorCogSounds[sound->refid]={cog,synced};
} catch(const std::bad_alloc&) {}
static int ownsCog(sithCog *cog){SithWorld *w=sithWorld_g_pCurrentWorld;if(!cog||!w)return 0;for(int i=0;i<ownedCogCount;i++)if(ownedCogSlots[i]<(int)w->numCogs&&cog==&w->aCogs[ownedCogSlots[i]])return 1;return 0;}
static void suspendPlacedCogs(int geometry){SithWorld *w=sithWorld_g_pCurrentWorld;if(!w)return;
    for(auto i=editorCogSounds.begin();i!=editorCogSounds.end();){
        sithPlayingSound *sound=sithSoundMixer_GetChannelHandle(i->first);
        if(!sound||ownsCog(i->second.cog)){
            if(sound){if(i->second.synced&&sithNet_isMulti)sithDSSThing_StopSound(sound,0,-1,255);sithSoundMixer_StopSound(sound);}
            i=editorCogSounds.erase(i);
        }else ++i;
    }
    for(int i=0;i<ownedCogCount;i++)if(ownedCogSlots[i]<(int)w->numCogs)w->aCogs[ownedCogSlots[i]].flags=(w->aCogs[ownedCogSlots[i]].flags|SITH_COG_DISABLED)&~(SITH_COG_TIMER_SET|SITH_COG_PULSE_SET);
    // Extended timers address runtime slots; discard them before reusing a slot.
    for(SithEvent **link=&sithEvent_g_pFirstQueuedEvent;*link;){SithEvent *event=*link;int owned=0;
        if(event->taskNum==4)for(int i=0;i<ownedCogCount;i++)if(event->params.idx==ownedCogSlots[i])owned=1;
        if(owned){*link=event->pNextEvent;sithEvent_FreeEvent(event);}else link=&event->pNextEvent;
    }
    for(int i=0;i<sithCog_numSurfaceLinks;)if(geometry||ownsCog(sithCog_aSurfaceLinks[i].cog)){sithCog_aSurfaceLinks[i]=sithCog_aSurfaceLinks[--sithCog_numSurfaceLinks];}else i++;
    for(int i=0;i<sithCog_numSectorLinks;)if(geometry||ownsCog(sithCog_aSectorLinks[i].cog)){sithCog_aSectorLinks[i]=sithCog_aSectorLinks[--sithCog_numSectorLinks];}else i++;
    for(int i=0;i<sithCog_numThingLinks;)if(ownsCog(sithCog_aThingLinks[i].cog)){sithCog_aThingLinks[i]=sithCog_aThingLinks[--sithCog_numThingLinks];}else i++;
    for(int i=0;i<w->numThingsLoaded;i++)if(ownsCog(w->aThings[i].pCaptureCog))w->aThings[i].pCaptureCog=NULL;
}
// Play-mode grate COGs change live surfaces, not the editor document. Restore
// just those surfaces without rebuilding the world or adding an undo entry.
static void resetSlashGrates(void){
    SithWorld *w=sithWorld_g_pCurrentWorld;if(!w||!model)return;
    std::set<std::pair<int,int>> faces;
    for(int i=0;i<model->cogCount;i++)if(!__strcmpi(model->cogs[i].file,"00_slashgrate.cog")){
        const EdPlacedCog &cog=model->cogs[i];
        for(int j=0;j<cog.count;j++){
            const EdCogParam &p=cog.params[j];
            if(p.type!=ED_COG_SURFACE||(__strcmpi(p.name,"grate")&&__strcmpi(p.name,"grateback"))||p.cell<0||p.cell>=model->count||p.face<0||p.face>=model->c[p.cell].count)continue;
            faces.emplace(p.cell,p.face);
            const EdFace &face=model->c[p.cell].f[p.face];
            if(face.neighbor>=0&&face.neighbor<model->count&&face.mirror>=0&&face.mirror<model->c[face.neighbor].count)faces.emplace(face.neighbor,face.mirror);
        }
    }
    for(auto [cell,face]:faces){
        if(cell>=w->numSectors||face>=w->aSectors[cell].numSurfaces)continue;
        SithSurface *surface=&w->aSectors[cell].surfaces[face];
        if(!surface->pAdjoin)continue;
        surface->surfaceInfo.face.wallCel=0;
        surface->pAdjoin->flags=edSurfaceAttributes(&model->c[cell].f[face]).adjoinFlags;
        if(sithNet_isMulti&&sithNet_isServer)sithSurface_SyncSurface(surface);
    }
}
static void clearCogInstance(sithCog *c){if(c->pSymbolTable)sithCogParse_FreeSymbolTable(c->pSymbolTable);if(c->heap)SITH_FREE(c->heap);
#ifdef COG_HEAP_INIT_ARGS
    if(c->aInitArgs)SITH_FREE(c->aInitArgs);
#endif
#ifdef COG_DYNAMIC_STACKS
    if(c->stack)SITH_FREE(c->stack);
#endif
    edZero(c,0,sizeof(*c));}
static SithCogScript *findEditorScript(const char *name){SithWorld *worlds[]={sithWorld_g_pCurrentWorld,sithWorld_g_pStaticWorld};
#ifdef SITH_DEBUG_STRUCT_NAMES
    for(int w=0;w<2;w++)if(worlds[w])for(int i=0;i<worlds[w]->numCogScripts;i++)if(!__strcmpi(baseName(worlds[w]->aCogScripts[i].aName),name))return &worlds[w]->aCogScripts[i];
#endif
    return NULL;
}
static int buildPlacedCogs(const EdModel *source){SithWorld *w=sithWorld_g_pCurrentWorld;suspendPlacedCogs(0);if(!w)return 0;
    for(int i=0;i<source->cogCount;i++){
        if(!ensureCogSource(source->cogs[i].file))return 0;SithCogScript *script=customResources.files.count(EdResources::path(std::string("cog/")+source->cogs[i].file))?cogCustomCompile(source->cogs[i].file):findEditorScript(source->cogs[i].file);sithCog *loaded=NULL;
        if(!script&&customResources.files.count(EdResources::path(std::string("cog/")+source->cogs[i].file)))return 0;
        if(!script){SithWorld *previous=sithWorld_g_pLastLoadedWorld;sithWorld_g_pLastLoadedWorld=w;
            /* A placed instance is independent of class-script instances. */
            EdBuffer<unsigned int> flags;for(int j=0;j<w->numCogs;j++){flags[j]=w->aCogs[j].flags;w->aCogs[j].flags&=~SITH_COG_CLASS;}
            int oldCount=w->numCogs;loaded=sithCog_Load(source->cogs[i].file);for(int j=0;j<oldCount;j++)w->aCogs[j].flags=flags[j];sithWorld_g_pLastLoadedWorld=previous;if(!loaded)return 0;script=loaded->pScript;
        }
        int slot;if(i<ownedCogCount)slot=ownedCogSlots[i];else{if(loaded)slot=loaded->idx&0x7fff;else{if(w->numCogs>=w->sizeCogs)return 0;slot=w->numCogs++;}ownedCogSlots[ownedCogCount++]=slot;}
        sithCog *c=&w->aCogs[slot];if(loaded&&loaded!=c){clearCogInstance(loaded);w->numCogs--;}
        clearCogInstance(c);c->idx=slot;c->pScript=script;c->flags=script->flags&~SITH_COG_CLASS;c->pSymbolTable=sithCogParse_DuplicateSymbolTable(script->pSymbolTable);if(!c->pSymbolTable){c->flags|=SITH_COG_DISABLED;return 0;}
        // This stock ambient script starts its loops in a delayed timer. Clients
        // normally ignore non-local COG timers, leaving them dependent on the
        // host's playback (which stops while the host edits). Each peer owns its
        // ambient loops instead; NOSYNC prevents duplicate playback/remote stops.
        if(!__strcmpi(source->cogs[i].file,"00_playambient.cog"))c->flags|=SITH_COG_LOCAL|SITHCOG_NOSYNC;
#ifdef SITH_DEBUG_STRUCT_NAMES
        snprintf(c->aName,sizeof(c->aName),"%s",source->cogs[i].file);
#endif
    }
    for(int i=0;i<source->cogCount;i++){sithCog *c=&w->aCogs[ownedCogSlots[i]];int param=0;
        for(unsigned int j=0;j<c->pScript->numSymbolRefs;j++){SithCogSymbolRef *ref=&c->pScript->aSymRefs[j];SithCogSymbol *symbol=&c->pSymbolTable->aSymbols[ref->hash];char value[32];
            if(ref->flags&1){if(ref->value[0])sithCog_ParseSymbolRef(symbol,ref,ref->value);continue;}
            if(param>=source->cogs[i].count)return 0;const EdCogParam *p=&source->cogs[i].params[param++];edCogParamValue(source,p,value);
            if(p->type==ED_COG_THING){int id=atoi(value),live=-1;if(id>=0&&id<source->spawnCount){SithThing *thing=id<JKPLAYER_NUM_INFOS?jkPlayer_playerInfos[id].pLocalPlayer:NULL;if(thing)live=thing->idx;else if(id<(int)w->numThingsLoaded&&w->aThings[id].type==SITH_THING_PLAYER)live=id;}
                else if(id>=source->spawnCount&&id<source->spawnCount+source->objectCount)live=objectThings[id-source->spawnCount];snprintf(value,32,"%d",live);}
            if(p->type==ED_COG_COG){int id=atoi(value);snprintf(value,32,"%d",id>=0&&id<source->cogCount?ownedCogSlots[id]:-1);}
            sithCog_ParseSymbolRef(symbol,ref,value);sithCog_LinkCog(c,ref,symbol);
        }
        sithCog_SendMessage(c,SITH_MESSAGE_LOADING,0,0,0,0,0);
    }
    for(int i=0;i<source->cogCount;i++)sithCog_SendMessage(&w->aCogs[ownedCogSlots[i]],SITH_MESSAGE_STARTUP,0,0,0,0,0);return 1;
}
static int startPlacedCogs(const EdModel *source){int ok=buildPlacedCogs(source);if(!ok)suspendPlacedCogs(0);return ok;}
static void closeCogEditor(void){cogExternalClose();editorCogSounds.clear();edFree(cogDraft);cogDraft=NULL;cogPickParam=-1;cogPickCancelled=0;cogSourceCount=ownedCogCount=0;}
static int cogExternalSelfTest(void){
    active=1;const char *name="zz_external_test.cog";
    std::string first="symbols\nint value=7\nint result=0 local\nmessage startup\nend\ncode\nstartup:\nresult=value+10;\nreturn;\nend\n";
    auto path=std::filesystem::absolute("editor-cogs-test.cog");{std::ofstream out(path,std::ios::binary);out<<first;}
    if(!cogPublishCustom(name,cogReadExternal(path))){printf("COG EXTERNAL failed line %d\n",__LINE__);return 0;}
    discoverCogs();int found=0;for(int i=0;i<cogSourceCount;i++)found|=!strcmp(cogSources[i].file,name);if(!found){printf("COG EXTERNAL failed line %d\n",__LINE__);return 0;}
    EdPlacedCog placed;if(!loadPlacedCog(name,&placed)||placed.count!=1){printf("COG EXTERNAL failed line %d\n",__LINE__);return 0;}strcpy(placed.params[0].value,"9");
    edCopyModel(candidate,model);candidate->cogs[0]=placed;candidate->cogCount=1;remember();if(!commit()){printf("COG EXTERNAL failed line %d\n",__LINE__);return 0;}
    active=0;if(!startPlacedCogs(model)){printf("COG EXTERNAL failed line %d\n",__LINE__);return 0;}
    auto result=[](){sithCog *c=&sithWorld_g_pCurrentWorld->aCogs[ownedCogSlots[0]];SithCogSymbol *symbol=(c->pSymbolTable->numUsedSymbols>1?&c->pSymbolTable->aSymbols[1]:NULL);return symbol?(symbol->val.type==SITHCOG_VALUE_FLOAT?(int)symbol->val.dataAsFloat[0]:(int)symbol->val.data[0]):-1;};
    if(result()!=19){printf("COG EXTERNAL failed line %d\n",__LINE__);return 0;}
    std::string second=first;second.replace(second.find("+10"),3,"+20");
    cogExternalFiles[name]={path,first};{std::ofstream out(path,std::ios::binary);out<<second;}cogExternalTick();
    if(cogReadExternal(path)!=second||result()!=29||strcmp(model->cogs[0].params[0].value,"9")){printf("COG EXTERNAL failed line %d result=%d value=%s active=%d cache=%zu\n",__LINE__,result(),model->cogs[0].params[0].value,active,cogCompiled.size());return 0;}
    if(cogPublishCustom(name,"symbols\nmessage startup\nend\ncode\nstartup:\nnot_valid( ;\nend\n")){printf("COG EXTERNAL failed line %d\n",__LINE__);return 0;}
    char *text=cogSourceText(name);int ok=text&&second==text;edFree(text);
    EdResources bad=customResources;bad.files.erase("cog/zz_external_test.cog");ok=ok&&!cogCustomOnlyChanges(customResources,bad);
    std::filesystem::remove(path);puts("COG EXTERNAL: import/discovery, compile, startup restart, watched save, preserved values, invalid-script rollback passed");return ok;
}
static int cogSoundSelfTest(void){
    active=1;auto files=sectorSoundResources();if(files.empty())return 0;cogSoundTestName=files[0];
    puts("COG_SOUND browser begin");fflush(stdout);BuilderMenuState menu=builderMenuBegin();int chosen=chooseCogSound(files,"");builderMenuEnd(menu);puts("COG_SOUND browser end");fflush(stdout);if(chosen<0||files[chosen]!=cogSoundTestName)return 0;
    edCopyModel(candidate,model);candidate->cogCount=1;if(!loadPlacedCog("00_playambient.cog",&candidate->cogs[0]))return 0;
    for(int i=0;i<candidate->cogs[0].count;i++){EdCogParam &p=candidate->cogs[0].params[i];const char *value=p.value;
        if(p.type==ED_COG_THING)value=!strcmp(p.name,"soundPos0")?"0":"-1";
        if(p.type==ED_COG_SOUND)value=files[chosen].c_str();if(!strcmp(p.name,"volume"))value="0.8";
        if(!edCogSetValue(candidate,&p,value))return 0;
    }
    if(!commit())return 0;active=0;if(!startPlacedCogs(model))return 0;
    sithCog *c=&sithWorld_g_pCurrentWorld->aCogs[ownedCogSlots[0]];
    if(!(c->flags&SITH_COG_TIMER_SET))return 0;
    // Exercise the client dispatch path with no host executing the timer.
    int wasMulti=sithNet_isMulti,wasServer=sithNet_isServer,wasOutput=sithMessage_g_outputstream;
    sithNet_isMulti=1;sithNet_isServer=0;sithMessage_g_outputstream=1;
    sithCog_SendMessage(c,SITH_MESSAGE_TIMER,0,0,0,0,0);
    int localOnly=!COG_SHOULD_SYNC(c)&&editorCogSounds.size()==1&&!editorCogSounds.begin()->second.synced;
    sithNet_isMulti=wasMulti;sithNet_isServer=wasServer;sithMessage_g_outputstream=wasOutput;
    if(!localOnly)return 0;
    if(editorCogSounds.size()!=1){printf("COG_SOUND expected 1 channel, got %zu, mixer=%d\n",editorCogSounds.size(),sithSoundMixer_bOpened);return 0;}
    int old=editorCogSounds.begin()->first;sithPlayingSound *playing=sithSoundMixer_GetChannelHandle(old);if(!playing||fabs(playing->vol_2-.8)>.001)return 0;
    sithPlayingSound *unrelated=sithSoundMixer_PlaySound(playing->sound,.1,0,SITHSOUNDFLAG_LOOP);if(!unrelated)return 0;int unrelatedId=unrelated->refid;
    SithEventParams params={};params.idx=c->idx;params.timerIdx=77;sithEvent_CreateEvent(4,&params,10000);
    active=1;suspendPlacedCogs(0);if(sithSoundMixer_GetChannelHandle(old)||!sithSoundMixer_GetChannelHandle(unrelatedId)||!editorCogSounds.empty())return 0;
    for(SithEvent *e=sithEvent_g_pFirstQueuedEvent;e;e=e->pNextEvent)if(e->taskNum==4&&e->params.idx==c->idx)return 0;
    edCopyModel(candidate,model);for(int i=0;i<candidate->cogs[0].count;i++)if(!strcmp(candidate->cogs[0].params[i].name,"volume"))edCogSetValue(candidate,&candidate->cogs[0].params[i],"0.2");
    if(!commit())return 0;active=0;if(!startPlacedCogs(model))return 0;c=&sithWorld_g_pCurrentWorld->aCogs[ownedCogSlots[0]];
    if(!(c->flags&SITH_COG_TIMER_SET))return 0;
    sithNet_isMulti=1;sithNet_isServer=1;sithMessage_g_outputstream=1;
    sithCog_SendMessage(c,SITH_MESSAGE_TIMER,0,0,0,0,0);
    localOnly=!COG_SHOULD_SYNC(c)&&editorCogSounds.size()==1&&!editorCogSounds.begin()->second.synced;
    sithNet_isMulti=wasMulti;sithNet_isServer=wasServer;sithMessage_g_outputstream=wasOutput;
    if(!localOnly)return 0;
    if(editorCogSounds.size()!=1)return 0;int updated=editorCogSounds.begin()->first;playing=sithSoundMixer_GetChannelHandle(updated);if(!playing||fabs(playing->vol_2-.2)>.001)return 0;
    edCopyModel(candidate,model);edCogDelete(candidate,0);if(!commit()||sithSoundMixer_GetChannelHandle(updated))return 0;
    if(!sithSoundMixer_GetChannelHandle(unrelatedId))return 0;sithSoundMixer_StopSound(sithSoundMixer_GetChannelHandle(unrelatedId));
    puts("COG_SOUND: client/host local ambient timers, no sound replication, search, changed volume, deletion, timer cleanup and unrelated sound preservation passed");return 1;
}
static int cogRuntimeCheck(void){if(model->cogCount!=1||ownedCogCount<1)return 0;sithCog *c=&sithWorld_g_pCurrentWorld->aCogs[ownedCogSlots[0]];if(c->flags&SITH_COG_DISABLED)return 0;
    int surface=0,thing=0;char value[32];edCogParamValue(model,&model->cogs[0].params[0],value);int id=atoi(value);
    for(int i=0;i<sithCog_numSurfaceLinks;i++)if(sithCog_aSurfaceLinks[i].cog==c&&sithCog_aSurfaceLinks[i].surface->index==id)surface=1;
    for(int i=0;i<sithCog_numThingLinks;i++)if(sithCog_aThingLinks[i].cog==c&&sithCog_aThingLinks[i].thing->idx==objectThings[0])thing=1;
    SithCogSymbolRef *ref=&c->pScript->aSymRefs[2];return surface&&thing&&fabs(c->pSymbolTable->aSymbols[ref->hash].val.dataAsFloat[0]-8)<.001;
}
static int cogReloadTest(void){int ok=model->cogCount==1&&model->cogs[0].count==3&&!__strcmpi(model->cogs[0].file,"01_cargoelev1.cog")&&model->cogs[0].params[0].cell>=0&&atoi(model->cogs[0].params[1].value)==1;
    return ok&&startPlacedCogs(model)&&cogRuntimeCheck();}
static int cogSelfTest(void){EdModel *saved=edAlloc(sizeof(*saved));if(!saved)return 0;edCopyModel(saved,model);int savedActive=active;active=1;clearHistory();edCopyModel(candidate,model);
    candidate->objectCount=1;candidate->objects[0]=EdObject{.position={.3,.6,.2}};snprintf(candidate->objects[0].templateName,64,"crate4_1");candidate->cogCount=1;
    int ok=loadPlacedCog("01_cargoelev1.cog",&candidate->cogs[0])&&candidate->cogs[0].count==3;
    ok=ok&&edCogSetValue(candidate,&candidate->cogs[0].params[0],"4")&&edCogSetValue(candidate,&candidate->cogs[0].params[1],"1");remember();ok=ok&&commit()&&undoCount==1;
    active=0;syncEditorObjects(model);ok=ok&&startPlacedCogs(model)&&cogRuntimeCheck();active=1;suspendPlacedCogs(0);syncEditorObjects(model);
    ok=ok&&writeEditorGob(model,"Episode/sithed_cogs_test.gob","COG integration test",0)&&historyStep(0)&&!model->cogCount&&historyStep(1)&&model->cogCount==1;
    cogDraft=edCalloc(1,sizeof(*cogDraft));ok=ok&&cogDraft!=NULL;
    if(cogDraft){cogDraft->count=1;cogDraft->cogs[0]=model->cogs[0];cogSelected=0;cogPickParam=0;EdCogParam *param=pickingCogParam();const int types[]={ED_COG_SECTOR,ED_COG_SURFACE,ED_COG_MATERIAL,ED_COG_VECTOR};
        for(int i=0;i<4&&ok;i++){param->type=types[i];updateCogPick(edV(0,0,.5),edV(0,1,0));ok=finishCogPick();if(types[i]==ED_COG_SECTOR)ok=ok&&param->cell==0;if(types[i]==ED_COG_SURFACE)ok=ok&&param->face==4;}
        param->type=ED_COG_TEMPLATE;updateCogPick(edV(.8,.6,.2),edV(-1,0,0));ok=ok&&finishCogPick()&&!strcmp(param->value,"crate4_1");
        edFree(cogDraft);cogDraft=NULL;cogPickParam=-1;
    }
    edCopyModel(candidate,saved);active=1;int restored=commit();active=savedActive;clearHistory();edFree(saved);printf("GEOSITH_COGS %s: declarations, live links, edit suspension, undo and bundled save\n",ok&&restored?"PASS":"FAIL");fflush(stdout);return ok&&restored;
}
static int stressCogs(SithThing *p){static int step;
    if(!step){stressRunning=1;stressRenderFrames=0;active=1;tool=5;savedPhysics=p->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=p->flags;setEditorFlight(p,1);
        edCopyModel(candidate,model);candidate->objectCount=1;candidate->objects[0]=EdObject{.position={.3,.6,.2}};snprintf(candidate->objects[0].templateName,64,"crate4_1");if(!commit())exit(2);clearHistory();
        p->position=rdvec(edV(0,-.75,.35));rdVector3 angles={0};rdMatrix_BuildRotate34(&p->orient,&angles);p->actorParams.headPYR=angles;sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);placedCogsMenu(0);if(!cogPicking())exit(2);
    }
    if(step==3){updateCogPick(edV(0,0,.5),edV(0,1,0));if(!finishCogPick())exit(2);cogPickParam=-1;placedCogsMenu(1);if(!cogPicking()||pickingCogParam()->type!=ED_COG_THING)exit(2);
        p->position=rdvec(edV(.3,-.75,.17));
    }
    if(step==6){char old[32];snprintf(old,32,"%s",pickingCogParam()->value);if(!jkEditor_HandleEscape())exit(2);cogPickControl(p);if(!cogPicking()||strcmp(old,pickingCogParam()->value))exit(2);}
#ifdef SDL2_RENDER
    if(step==8){char path[512];snprintf(path,sizeof(path),"%s.pick.png",getenv("GEOSITH_COG_CAPTURE"));std3D_ScreenshotWindow(path);}
#endif
    if(step==9){updateCogPick(edV(.8,.6,.2),edV(-1,0,0));if(cogPickThing!=0||!finishCogPick())exit(2);cogPickParam=-1;placedCogsMenu(1);
        if(cogDraft||model->cogCount!=1||atoi(model->cogs[0].params[1].value)!=1||undoCount!=1)exit(2);
        if(!historyStep(0)||model->cogCount||!historyStep(1)||model->cogCount!=1)exit(2);
        active=0;syncEditorObjects(model);if(!startPlacedCogs(model)||!cogRuntimeCheck())exit(2);active=1;suspendPlacedCogs(0);syncEditorObjects(model);
    }
    if(step++>=32){printf("GEOSITH_STRESS PASS: COG browser, surface/thing picking, cancel, undo and live script links, %d rendered frames\n",stressRenderFrames);fflush(stdout);exit(0);}return 1;
}

static int stressDestructible(SithThing *player){
    static int frame;stressRunning=1;active=1;tool=1;if(frame++<5)return 1;
    if(getenv("GEOSITH_SPIRAL_TEST")){int result=spiralTest();if(!result)return 1;fflush(stdout);sithSound_Shutdown();exit(result>0?0:2);}
    if(getenv("GEOSITH_BEVEL_TEST")){int result=bevelTest();if(!result)return 1;fflush(stdout);sithSound_Shutdown();exit(result>0?0:2);}
    if(getenv("GEOSITH_HAZARD_TEST")){
        EdBuffer<unsigned char> selected;selected[0]=1;edCopyModel(candidate,model);
        if(!edGenerateHazard(candidate,selected,0,2)||!commit())exit(2);
        if(!writeEditorGob(model,"Episode/sithed_hazard_test.gob","Hazard test",0))exit(2);
        leaveEditor(player);auto *world=sithWorld_g_pCurrentWorld;if(ownedCogCount!=1)exit(2);
        auto *c=&world->aCogs[ownedCogSlots[0]];if(!c->pScript||!(c->flags&SITH_COG_PULSE_SET))exit(2);
        player->actorParams.health=100;sithInventory_SetInventory(player,60,0);player->actorParams.flags&=~SITH_AF_INVULNERABLE;int before=0;for(int i=0;i<=world->numThings;i++)before+=world->aThings[i].type!=0;
        sithCog_SendMessage(c,SITH_MESSAGE_PULSE,0,0,0,0,0);
        int after=0;for(int i=0;i<=world->numThings;i++)after+=world->aThings[i].type!=0;
        printf("GEOSITH_HAZARD health %.3f things %d -> %d\n",player->actorParams.health,before,after);
        int ok=player->actorParams.health<100&&after>before;
        puts(ok?"GEOSITH_HAZARD PASS: live damage, lightning effect and saved script":"GEOSITH_HAZARD FAIL");fflush(stdout);sithSound_Shutdown();exit(ok?0:2);
    }
    if(getenv("GEOSITH_WIND_TEST")){int ok=windTest(player);fflush(stdout);sithSound_Shutdown();exit(ok?0:2);}
    if(getenv("GEOSITH_TELEPORTER_TEST")){
        edCopyModel(candidate,model);candidate->objectCount=2;
        for(int i=0;i<2;i++){auto &o=candidate->objects[i];o={};strcpy(o.templateName,"ghost");o.position=edV(i?.5:-.5,0,.5);o.rotation=edV(0,i?90:0,0);}
        if(!commit())exit(2);clearHistory();thingGeneratorSelect(0);thingGeneratorSelect(1);thingGeneratorMenu();
        if(model->cogCount!=1||thingGeneratorHasSelection()||!historyStep(0)||model->cogCount||!historyStep(1)||model->cogCount!=1)exit(2);
        if(!writeEditorGob(model,"Episode/sithed_teleporter_test.gob","Teleporter test",0))exit(2);
        leaveEditor(player);auto *w=sithWorld_g_pCurrentWorld;auto *c=&w->aCogs[ownedCogSlots[0]];
        player->position=w->aThings[objectThings[0]].position;
        auto *sparks=sithTemplate_GetTemplate("+telesparks");if(!sparks)exit(2);
        auto particleCount=[&](){int count=0;for(int i=0;i<=w->numThings;i++)if(w->aThings[i].type&&w->aThings[i].pTemplate==sparks)count++;return count;};
        int before=particleCount();
        auto pulse=[&](){sithCog_SendMessage(c,SITH_MESSAGE_PULSE,0,0,0,0,0);};pulse();
        if(particleCount()!=before+2)exit(2);
        if(fabs(player->position.x-.5)>.001)exit(2);pulse();if(fabs(player->position.x-.5)>.001)exit(2);
        player->position={0,.75,.5};pulse();player->position=w->aThings[objectThings[1]].position;pulse();
        if(fabs(player->position.x+.5)>.001)exit(2);
        if(particleCount()!=before+4)exit(2);
        puts("GEOSITH_TELEPORTER PASS: ordered thing selection, menus, undo/redo, save, COG compilation, both directions, endpoint particles and bounce protection");fflush(stdout);sithSound_Shutdown();exit(0);
    }
    if(getenv("GEOSITH_CONVEYOR_TEST")){
        // Different handedness, rotations and scales must still visibly move
        // along the same world velocity, without remapping either texture.
        edCopyModel(candidate,model);
        for(int i=0;i<2;i++){auto &f=candidate->c[0].f[i?4:0];f.customUV=1;
            for(int v=0;v<f.count;v++){auto &p=f.v[v];p.u=i?-160*p.p.x+64*p.p.z:320*p.p.x+80*p.p.y;p.v=i?320*p.p.x+320*p.p.z:-160*p.p.x+320*p.p.y;}}
        if(!commit())exit(2);
        clearHistory();picks.count=2;picks.hits[0]={0,0,-1,-1};picks.hits[1]={0,4,-1,-1};mapOpen(player);surfaceAttributesDialog();
        if(!conveyorActive()||mapActive||model->cogCount||!conveyorCancel()||undoCount)exit(2);
        surfaceAttributesDialog();if(!conveyorActive())exit(2);
        EdVec point;if(!conveyorAim(0,edV(0,0,.5),edV(0,0,-1),&point)||fabs(point.z-.003)>1e-5)exit(2);
        conveyorDirections[0]=edV(-1,0,0);
        if(!conveyorApply()||model->cogCount!=2||!historyStep(0)||model->cogCount||!historyStep(1)||model->cogCount!=2)exit(2);
        if(!writeEditorGob(model,"Episode/sithed_conveyor_test.gob","Conveyor test",0))exit(2);
        leaveEditor(player);SithWorld *world=sithWorld_g_pCurrentWorld;
        for(int i=0;i<2;i++){SithSurface *surface=&world->aSectors[0].surfaces[i?4:0];rdSurface *anim=sithSurface_GetRdSurface(surface);
            if(!anim||!(surface->flags&SITH_SURFACE_SCROLLING)||fabs(anim->field_24.x+.2)>.0001||fabs(anim->field_24.y)>.0001||fabs(anim->field_24.z)>.0001)exit(2);
            if(fabs(anim->scrollVector.x-(i?-32:64))>.001||fabs(anim->scrollVector.y-(i?64:-32))>.001)exit(2);}
        puts("GEOSITH_CONVEYOR PASS: menus, shared world direction, rotated/mirrored/scaled UV scrolling, undo/redo, save and stock COG velocity");fflush(stdout);sithSound_Shutdown();exit(0);
    }
    if(const char *modeText=getenv("GEOSITH_DOOR_TEST")){
        int mode=atoi(modeText);edCopyModel(candidate,model);
        if(!edExtrudeStep(candidate,0,1))exit(2);
        if(!commit())exit(2);clearHistory();sectorPicks.clear();sectorPicks[1]=1;
        if(mode==2)mapOpen(player);
        sectorAttributesDialog();
        if(mode==2){
            if(!doorActive()||mapActive||model->objectCount)exit(2);
            if(!doorCancel()||model->objectCount||undoCount)exit(2);
            sectorPicks[1]=1;sectorAttributesDialog();
            EdHit hit={};hit.cell=0;hit.face=4;hit.point=edV(0,1,.5);
            doorPreview(edV(0,0,.5),edV(0,1,0));doorPlace(hit);if(doorActive())exit(2);
        }
        if(model->objectCount!=2||!historyStep(0)||model->objectCount||!historyStep(1)||model->objectCount!=2)exit(2);
        if(!writeEditorGob(model,"Episode/sithed_door_test.gob","Door test",0))exit(2);
        leaveEditor(player);SithWorld *world=sithWorld_g_pCurrentWorld;
        SithThing *door=&world->aThings[objectThings[0]],*other=&world->aThings[objectThings[1]];
        if(door->moveType!=SITH_MT_PATH||door->trackParams.loadedFrames!=2||!door->pSoundClass)exit(2);
        if(mode==3){sithInventory_SetInventory(player,46,0);sithCog_ThingSendMessage(door,player,SITH_MESSAGE_ACTIVATE);if(door->trackParams.flags)exit(2);sithInventory_SetInventory(player,46,1);}
        if(mode==2){sithCog_ThingSendMessage(door,player,SITH_MESSAGE_ACTIVATE);if(door->trackParams.flags)exit(2);sithCog_SurfaceSendMessage(&world->aSectors[0].surfaces[4],player,SITH_MESSAGE_ACTIVATE);}
        else if(mode==0){player->position=door->position;sithCog_SendMessage(&world->aCogs[ownedCogSlots[0]],SITH_MESSAGE_PULSE,0,0,0,0,0);}
        else sithCog_ThingSendMessage(door,player,SITH_MESSAGE_ACTIVATE);
        if(door->goalframe!=1||other->goalframe!=1||!door->trackParams.flags||!other->trackParams.flags){printf("DOOR FAIL trigger mode %d\n",mode);fflush(stdout);exit(2);}
        printf("GEOSITH_DOOR PASS: activation %d, menus, split panels, movement/sound, undo/redo, COG compilation and GOB packaging\n",mode);fflush(stdout);sithSound_Shutdown();exit(0);
    }
    if(getenv("GEOSITH_LIFT_TEST")){
        // Exercise the actual bound camera key in both directions while editing.
        stdControlKeyInfo *cameraBinding=&sithControl_aInputFuncToKeyinfo[INPUT_FUNC_CAMERAMODE];if(!cameraBinding->numEntries)exit(2);
        int cameraKey=cameraBinding->aEntries[0].dxKeyNum;sithCamera_g_curCycleCamNum=0;sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[0]);
        for(int cycle=0;cycle<2;cycle++){
            stdControl_aKeyPressed[cameraKey]=0;stdControl_UpdateKeyState(cameraKey,1,stdPlatform_GetTimeMsec());jkControl_HandleCamera(player);
            stdControl_UpdateKeyState(cameraKey,0,stdPlatform_GetTimeMsec());
            if((sithCamera_g_pCurCamera==&sithCamera_g_aCameras[0])!=(cycle==1)){puts("GEOSITH_LIFT FAIL editor camera binding");exit(2);}
        }
        edCopyModel(candidate,model);if(!edExtrudeStep(candidate,0,1))exit(2);
        for(int f=0;f<candidate->c[1].count;f++)for(int v=0;v<candidate->c[1].f[f].count;v++)if(candidate->c[1].f[f].v[v].p.z>1)candidate->c[1].f[f].v[v].p.z+=.5;
        if(!commit())exit(2);clearHistory();
        sectorPicks.clear();sectorPicks[1]=1;sectorAttributesDialog();
        if(model->cogCount!=1||model->objectCount!=1||model->objects[0].attributes.frameCount!=2)exit(2);
        if(!historyStep(0)||model->cogCount||!historyStep(1)||model->cogCount!=1)exit(2);
        const EdObject &generatedLift=model->objects[0];
        if((generatedLift.attributes.frames[1].position.z<generatedLift.position.z)!=(getenv("GEOSITH_LIFT_TOP_TEST")!=NULL))exit(2);
        if(getenv("GEOSITH_OBJECT_MODEL_TEST")){
            if(builderPickModel(edV(.8,0,1.3),edV(0,0,-1))!=0){puts("OBJECT_MODEL FAIL face picking");exit(2);}
            int assets=model->assetCount;if(!builderEditObject(0)||!builderEditingName[0])exit(2);
            builderTestDiscard=1;builderFinish();builderTestDiscard=0;if(model->assetCount!=assets)exit(2);
            if(!builderEditObject(0))exit(2);edCopyModel(candidate,model);
            for(int i=0;i<candidate->meshCount;i++)if(candidate->meshes[i].asset==builderAsset)candidate->meshes[i].geometry.f[0].v[0].u+=17;
            if(!commit())exit(2);builderFinish();if(model->assetCount!=assets||builderAsset!=-1)exit(2);
            remember();edCopyModel(candidate,model);for(int i=0;i<2;i++){EdObject object={};strcpy(object.templateName,"crate4_1");object.position=edV(i?.4:-.4,0,.3);candidate->objects[candidate->objectCount++]=object;}if(!commit())exit(2);
            rdModel3 *original=sithTemplate_GetTemplate("crate4_1")->renderData.model3;char filename[64];snprintf(filename,64,"%s",original->filename);
            if(!builderEditObject(1))exit(2);edCopyModel(candidate,model);for(int i=0;i<candidate->meshCount;i++)if(candidate->meshes[i].asset==builderAsset)candidate->meshes[i].geometry.f[0].v[0].u+=23;if(!commit())exit(2);
            builderFinish();rdModel3 *updated=sithModel_Find(filename);if(updated==original)exit(2);
            for(int o=1;o<3;o++)if(sithWorld_g_pCurrentWorld->aThings[objectThings[o]].renderData.model3!=updated)exit(2);
            if(!historyStep(0)||sithTemplate_GetTemplate("crate4_1")->renderData.model3!=original||!historyStep(1)||sithTemplate_GetTemplate("crate4_1")->renderData.model3==original)exit(2);
            if(!writeEditorGob(model,"Episode/sithed_object_model_test.gob","Object model test",0))exit(2);
            puts("GEOSITH_OBJECT_MODEL PASS: mesh-face picking, custom edit/discard, stock import, shared instance refresh, undo/redo and packaging");fflush(stdout);sithSound_Shutdown();exit(0);
        }
        if(!writeEditorGob(model,"Episode/sithed_lift_test.gob","Lift test",0))exit(2);
        leaveEditor(player);SithThing *lift=&sithWorld_g_pCurrentWorld->aThings[objectThings[0]];
        if(lift->moveType!=SITH_MT_PATH||lift->trackParams.loadedFrames!=2||lift->trackParams.flags)exit(2);
        sithCog_ThingSendMessage(lift,player,SITH_MESSAGE_ENTERED);
        if(lift->goalframe!=1||!lift->trackParams.flags||fabs(lift->trackParams.moveVel-.8)>.001||!lift->pSoundClass){puts("GEOSITH_LIFT FAIL step-on trigger/speed/sound");fflush(stdout);exit(2);}
        puts("GEOSITH_LIFT PASS: menu, floor clearance, frames, undo/redo, packaging and stock COG step-on trigger");fflush(stdout);sithSound_Shutdown();exit(0);
    }
    if(const char *sector=getenv("GEOSITH_DESTRUCTIBLE_DIAG")){
        EdBuffer<unsigned char> picked;picked[atoi(sector)]=1;edCopyModel(candidate,model);
        printf("DESTRUCTIBLE diagnostic sectors=%d templates=%d meshes=%d valid=%d target=%d spline=%d\n",model->count,model->templateCount,model->meshCount,edValidate(model),atoi(sector),model->c[atoi(sector)].splineId);fflush(stdout);
        if(const char *path=getenv("GEOSITH_DESTRUCTIBLE_FIXTURE")){
            EdModel fixture;edDefault(&fixture);fixture.c[0]=model->c[atoi(sector)];EdVec center={};int n=0;
            for(int f=0;f<fixture.c[0].count;f++){EdFace &face=fixture.c[0].f[f];face.neighbor=face.mirror=-1;face.hasAttributes=0;face.material=0;face.decalId=0;for(int v=0;v<face.count;v++){center=edAdd(center,face.v[v].p);n++;}}
            fixture.spawn=fixture.spawns[0].position=edScale(center,1.0/n);
            unsigned size=0;unsigned char *data=edWireEncode(&fixture,&size);FILE *file=fopen(path,"wb");if(file&&data){fwrite(data,1,size,file);fclose(file);}edFree(data);
        }
        int ok;
        if(getenv("GEOSITH_LIFT_DIAG")){
            EdPlacedCog cog={};char *source=cogSourceText("12_trooperelev.cog");ok=source&&edCogParseScript(&cog,"12_trooperelev.cog",source);edFree(source);
            if(ok)ok=edGenerateLift(candidate,atoi(sector),"diag_lift",cog);
            if(ok)printf("LIFT diagnostic bottom=%g top=%g\n",candidate->objects[candidate->objectCount-1].position.z,candidate->objects[candidate->objectCount-1].attributes.frames[1].position.z);
        }else ok=edGenerateDestructible(candidate,picked,"diag_wall");printf("GENERATOR diagnostic result=%d\n",ok);fflush(stdout);sithSound_Shutdown();exit(ok?0:2);
    }
    sectorPicks.clear();sectorPicks[0]=1;sectorAttributesDialog();
    if(model->cogCount!=1){puts("DESTRUCTIBLE FAIL menu");exit(2);}
    if(!historyStep(0)||model->cogCount||!historyStep(1)||model->cogCount!=1){puts("DESTRUCTIBLE FAIL undo");exit(2);}
    if(!writeEditorGob(model,"Episode/sithed_destructible_test.gob","Destructible test",0)){puts("DESTRUCTIBLE FAIL save");exit(2);}
    leaveEditor(player);SithWorld *world=sithWorld_g_pCurrentWorld;
    int wallId=objectThings[0];if(wallId<0){puts("DESTRUCTIBLE FAIL wall");exit(2);}SithThing *wall=&world->aThings[wallId];
    sithThing_DamageThing(wall,player,10,SITH_DAMAGE_ENERGY);
    if(wall->flags&(SITH_TF_DESTROYED|SITH_TF_DISABLED)){puts("DESTRUCTIBLE FAIL energy filter");exit(2);}
    int damage=getenv("GEOSITH_DESTRUCTIBLE_EXPLOSION_TEST")?SITH_DAMAGE_FIRE:SITH_DAMAGE_SABER;
    sithThing_DamageThing(wall,player,10,damage);
    int debris=0,blast=0;for(int i=0;i<world->numThingsLoaded;i++){debris+=world->aThings[i].type==SITH_THING_DEBRIS;blast+=world->aThings[i].type==SITH_THING_EXPLOSION;}
    printf("DESTRUCTIBLE damage=%d flags=%x debris=%d explosion=%d\n",damage,wall->flags,debris,blast);
    if(!(wall->flags&SITH_TF_DESTROYED)||debris!=12||blast!=1){puts("DESTRUCTIBLE FAIL COG");fflush(stdout);exit(2);}
    puts("GEOSITH_DESTRUCTIBLE PASS: generation, undo/redo, packaging, damage filter, fracture debris and explosion");fflush(stdout);sithSound_Shutdown();exit(0);
}
