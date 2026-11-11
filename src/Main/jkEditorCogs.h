/* Placed COG authoring owns a draft across the temporary world-picking mode. */
#define EDITOR_COG_SOURCES 2048
typedef struct {char file[32],archive[260];unsigned int offset,size;} EditorCogSource;
typedef struct {int count;EdBuffer<EdPlacedCog> cogs;} EditorCogDraft;
static EditorCogSource cogSources[EDITOR_COG_SOURCES];
static int cogSourceCount,cogSelected,cogPage,cogParamPage,cogPickParam=-1,cogPickCancelled,cogPickThing=-10000;
static EditorCogDraft *cogDraft;
static EdHit cogPickHit;
static int ownedCogSlots[ED_COGS],ownedCogCount;
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
static char *cogSourceText(const char *name){char *networkSource=netCogSource(name);if(networkSource)return networkSource;char path[80];snprintf(path,sizeof(path),"cog\\%s",name);stdFile_t file=jkRes_FileOpen(path,"rb");char *text=NULL;
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
int jkEditor_HandleEscape(void){if(active&&terrainSessionOpen()){terrainSessionFinish();return 1;}if(active&&builderAsset>=0){builderExitRequested=1;return 1;}if(active&&mapActive){int editing=splineDrawing||splineDragging||mapCleaveMode||mapDrawing||mapUnplaced||mapDragging||mapBox;if(editing)mapEscape();return editing?1:0;}if(active&&cogPicking()){cogPickCancelled=1;return 1;}return 0;}
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
        e[25]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,435,230,35},1};e[26]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,435,230,35},1};jkGuiElement all[28];edCopy(all,e,sizeof(e));edZero(&all[27],0,sizeof(all[27]));all[27].type=ELEMENT_END;
        jkGuiMenu menu={all,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);all[0].wstr=info;all[1].wstr=u"Scripts";all[2].wstr=u"Click a parameter to assign it";for(int i=0;i<5;i++)all[3+i].wstr=rows[i];for(int i=0;i<8;i++)all[8+i].wstr=params[i];
        const char16_t *labels[]={u"Previous",u"Next",u"Add COG",u"Delete",u"Duplicate",u"Prev params",u"Next params",u"Edit value",u"Clear value"};for(int i=0;i<9;i++)all[16+i].wstr=labels[i];all[25].wstr=u"Apply";
        if(getenv("GEOSITH_COG_CAPTURE")&&getenv("GEOSITH_STRESS"))menu.idkFunc=capturePlacedCogs;
        jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&all[25]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&all[26]);int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(all);
        if(choice<0)break;
        if(choice>=100&&choice<105){cogSelected=cogPage*5+choice-100;cogParamPage=cogChosenParam=0;}
        else if(choice==10&&cogPage>0)cogPage--;else if(choice==11&&(cogPage+1)*5<cogDraft->count)cogPage++;
        else if(choice==12||choice==14){if(cogDraft->count>=ED_COGS)jkGuiDialog_ErrorDialog(u"Placed COGs",u"The engine COG instance limit has been reached.");else if(choice==14&&c){cogDraft->cogs[cogDraft->count]=*c;cogSelected=cogDraft->count++;cogPage=cogSelected/5;}else{int id=chooseCogScript();if(id>=0){EdPlacedCog added;if(loadPlacedCog(cogSources[id].file,&added)){cogSelected=cogDraft->count;cogDraft->cogs[cogDraft->count++]=added;cogPage=cogSelected/5;cogParamPage=cogChosenParam=0;}else jkGuiDialog_ErrorDialog(u"Placed COGs",u"Could not read the COG symbol declarations.");}}}
        else if(choice==13&&c){edCopyModel(candidate,model);candidate->cogCount=cogDraft->count;edCopy(candidate->cogs,cogDraft->cogs,sizeof(candidate->cogs));edCogDelete(candidate,cogSelected);cogDraft->count=candidate->cogCount;edCopy(cogDraft->cogs,candidate->cogs,sizeof(candidate->cogs));if(cogPage*5>=cogDraft->count)cogPage=cogDraft->count?(cogDraft->count-1)/5:0;}
        else if(choice==15&&cogParamPage>0)cogParamPage--;else if(choice==16&&c&&(cogParamPage+1)*8<c->count)cogParamPage++;
        else if((choice>=200&&choice<208)||choice==17||choice==18){if(!c||!c->count)continue;int index=choice>=200?cogParamPage*8+choice-200:cogChosenParam;if(index>=c->count)continue;cogChosenParam=index;EdCogParam *param=&c->params[index];
            if(choice==18){edCopyModel(candidate,model);candidate->cogCount=cogDraft->count;edCogSetValue(candidate,param,param->type==ED_COG_VECTOR?"(0/0/0)":param->type==ED_COG_INT||param->type==ED_COG_FLEX?"0":"-1");}
            else if(choice==17)editCogValue(param);
            else if(param->type==ED_COG_THING||param->type==ED_COG_SURFACE||param->type==ED_COG_SECTOR||param->type==ED_COG_MATERIAL||param->type==ED_COG_TEMPLATE||param->type==ED_COG_VECTOR){cogPickParam=index;cogPickCancelled=0;picking=1;break;}
            else if(param->type==ED_COG_COG){std::vector<std::array<char16_t,128>> names(cogDraft->count);for(int i=0;i<cogDraft->count;i++){char name[96];snprintf(name,sizeof(name),"%d: %s",i,cogDraft->cogs[i].file);stdString_CharToWchar(names[i].data(),name,128);}int id=chooseList(u"Choose a Placed COG",(char16_t (*)[128])names.data(),cogDraft->count,0);if(id>=0)snprintf(param->value,32,"%d",id);}
            else editCogValue(param);
        }
        else if(choice==1){edCopyModel(candidate,model);candidate->cogCount=cogDraft->count;edCopy(candidate->cogs,cogDraft->cogs,sizeof(candidate->cogs));if(!edValidate(candidate)){jkGuiDialog_ErrorDialog(u"Placed COGs",u"One or more parameter values are invalid. Set a valid value or clear the reference.");continue;}remember();if(commit()){say("Placed COGs updated. F8 play-tests scripts; Save Level keeps them.");break;}}
    }
    jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=previousModal;stdControl_SetActivation(1);stdControl_Flush();fireDown=grabDown=middleDown=1;
    if(picking)cogPickPrompt();else{edFree(cogDraft);cogDraft=NULL;cogPickParam=-1;}
}
static int ownsCog(sithCog *cog){SithWorld *w=sithWorld_g_pCurrentWorld;if(!cog||!w)return 0;for(int i=0;i<ownedCogCount;i++)if(ownedCogSlots[i]<(int)w->numCogs&&cog==&w->aCogs[ownedCogSlots[i]])return 1;return 0;}
static void suspendPlacedCogs(int geometry){SithWorld *w=sithWorld_g_pCurrentWorld;if(!w)return;
    for(int i=0;i<ownedCogCount;i++)if(ownedCogSlots[i]<(int)w->numCogs)w->aCogs[ownedCogSlots[i]].flags|=SITH_COG_DISABLED;
    for(int i=0;i<sithCog_numSurfaceLinks;)if(geometry||ownsCog(sithCog_aSurfaceLinks[i].cog)){sithCog_aSurfaceLinks[i]=sithCog_aSurfaceLinks[--sithCog_numSurfaceLinks];}else i++;
    for(int i=0;i<sithCog_numSectorLinks;)if(geometry||ownsCog(sithCog_aSectorLinks[i].cog)){sithCog_aSectorLinks[i]=sithCog_aSectorLinks[--sithCog_numSectorLinks];}else i++;
    for(int i=0;i<sithCog_numThingLinks;)if(ownsCog(sithCog_aThingLinks[i].cog)){sithCog_aThingLinks[i]=sithCog_aThingLinks[--sithCog_numThingLinks];}else i++;
    for(int i=0;i<w->numThingsLoaded;i++)if(ownsCog(w->aThings[i].pCaptureCog))w->aThings[i].pCaptureCog=NULL;
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
        if(!ensureCogSource(source->cogs[i].file))return 0;SithCogScript *script=findEditorScript(source->cogs[i].file);sithCog *loaded=NULL;
        if(!script){SithWorld *previous=sithWorld_g_pLastLoadedWorld;sithWorld_g_pLastLoadedWorld=w;
            /* A placed instance is independent of class-script instances. */
            EdBuffer<unsigned int> flags;for(int j=0;j<w->numCogs;j++){flags[j]=w->aCogs[j].flags;w->aCogs[j].flags&=~SITH_COG_CLASS;}
            int oldCount=w->numCogs;loaded=sithCog_Load(source->cogs[i].file);for(int j=0;j<oldCount;j++)w->aCogs[j].flags=flags[j];sithWorld_g_pLastLoadedWorld=previous;if(!loaded)return 0;script=loaded->pScript;
        }
        int slot;if(i<ownedCogCount)slot=ownedCogSlots[i];else{if(loaded)slot=loaded->idx&0x7fff;else{if(w->numCogs>=w->sizeCogs)return 0;slot=w->numCogs++;}ownedCogSlots[ownedCogCount++]=slot;}
        sithCog *c=&w->aCogs[slot];if(loaded&&loaded!=c){clearCogInstance(loaded);w->numCogs--;}
        clearCogInstance(c);c->idx=slot;c->pScript=script;c->flags=script->flags&~SITH_COG_CLASS;c->pSymbolTable=sithCogParse_DuplicateSymbolTable(script->pSymbolTable);if(!c->pSymbolTable){c->flags|=SITH_COG_DISABLED;return 0;}
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
static void closeCogEditor(void){edFree(cogDraft);cogDraft=NULL;cogPickParam=-1;cogPickCancelled=0;cogSourceCount=ownedCogCount=0;}
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
