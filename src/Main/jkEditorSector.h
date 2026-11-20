/* Sector selection uses the same transactions as surface edits. */
static void mergeSelectedSectors(void){
    if(selectedSectorCount()<2){say("Shift+click at least two sectors, then press M to merge.");return;}
    if(netEditPending()){say("Wait for the current edit before merging sectors.");return;}
    edCopyModel(candidate,model);int merged=-1;char reason[192];
    if(!edMergeSectors(candidate,sectorPicks,&merged,reason,sizeof(reason))){say(reason);return;}
    remember();if(commit()){sectorPicks.clear();sectorPicks[merged]=1;picks.count=0;hover.cell=-1;say("Sectors merged. Lowest sector's attributes retained; internal surface references cleared. Ctrl+Z: undo.");}
}
static void captureSectorFlags(jkGuiMenu *menu){static int frame;if(++frame%3)return;surfaceFlagClick(&menu->paElements[1],menu,0,0,1);menu->lastClicked=1;}
static void captureSectorAttributes(jkGuiMenu *menu){
    static int frame,stage;if(++frame%3)return;
    stdString_SafeWStrCopy((char16_t*)menu->paElements[9].wstr,u"0.35",128);
    stdString_SafeWStrCopy((char16_t*)menu->paElements[11].wstr,u"0.1 0.2 0.3",128);
#ifdef SDL2_RENDER
    if(getenv("GEOSITH_SECTOR_CAPTURE"))std3D_ScreenshotWindow(getenv("GEOSITH_SECTOR_CAPTURE"));
#endif
    menu->lastClicked=stage++?1:200;
}
static unsigned int sectorFlagsDialog(std::vector<EdCell>& draft){
    const char *names[32]={"No gravity","Underwater","Used in COG","Apply thrust","Hide on map","No actor entry","Fall death","Adjoins off",NULL,NULL,NULL,NULL,"Collision box (managed)",NULL,"Seen on map","Network sync"};
    unsigned int common=~0u,combined=0;for(auto &cell:draft){common&=cell.flags;combined|=cell.flags;}
    jkGuiElement elements[37]={0};char16_t labels[32][96];
    elements[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,8,600,40},1};
    for(int bit=0;bit<32;bit++){
        char text[96];if(names[bit])snprintf(text,sizeof(text),"%s%s",((common^combined)&(1u<<bit))?"~ ":"",names[bit]);else snprintf(text,sizeof(text),"%sUnknown 0x%08X",((common^combined)&(1u<<bit))?"~ ":"",1u<<bit);
        stdString_CharToWchar(labels[bit],text,96);
        elements[bit+1]=jkGuiElement{ELEMENT_CHECKBOX,100+bit,0,NULL,(common&(1u<<bit))!=0,{18+(bit/16)*310,56+(bit%16)*21,302,20},1};
        elements[bit+1].clickHandlerFunc=surfaceFlagClick;elements[bit+1].drawFuncOverride=surfaceFlagDraw;
    }
    elements[33]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,396,600,24},1};
    elements[34]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,430,230,36},1};
    elements[35]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,430,230,36},1};elements[36].type=ELEMENT_END;
    jkGuiMenu menu={elements,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
    elements[0].wstr=u"Sector Flags";elements[33].wstr=u"~ Mixed values. Collision box is managed by the engine.";elements[34].wstr=u"OK";
    for(int bit=0;bit<32;bit++)elements[bit+1].wstr=labels[bit];surfaceFlagsTouched=0;
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[35]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&elements[34]);
    if((getenv("GEOSITH_SECTOR_TEST")||getenv("GEOSITH_SPLINE_FLAGS_TEST"))&&getenv("GEOSITH_STRESS"))menu.idkFunc=captureSectorFlags;
    unsigned int appliedMask=0;
    if(jkGuiRend_DisplayAndReturnClicked(&menu)==1){unsigned int value=0;for(int bit=0;bit<32;bit++)if(elements[bit+1].boxChecked)value|=1u<<bit;unsigned int mask=surfaceFlagsTouched&~SITH_SECTOR_HASCOLLIDEBOX;appliedMask=mask;for(auto &cell:draft)cell.flags=(cell.flags&~mask)|(value&mask);}
    freeMenuStrings(elements);return appliedMask;
}
static void sectorFieldText(const EdCell &cell,int field,char *text,size_t size){
    switch(field){
        case 0:snprintf(text,size,"0x%08X",cell.flags);break;
        case 1:snprintf(text,size,"%.9g",cell.extraLight);break;
        case 2:snprintf(text,size,"%s",cell.colormap[0]?cell.colormap:"<level default>");break;
        case 3:snprintf(text,size,"%.9g %.9g %.9g",cell.tint.x,cell.tint.y,cell.tint.z);break;
        case 4:snprintf(text,size,"%s",cell.sound);break;
        case 5:snprintf(text,size,"%.9g",cell.soundVolume);break;
    }
}
static int sectorFieldSet(EdCell &cell,int field,const char *text){
    char extra;double number;EdVec tint;
    if(field==1||field==5){if(sscanf(text," %lf %c",&number,&extra)!=1||!isfinite(number)||number<0||number>1)return 0;if(field==1)cell.extraLight=number;else cell.soundVolume=number;}
    else if(field==3){if(sscanf(text," %lf %lf %lf %c",&tint.x,&tint.y,&tint.z,&extra)!=3||!isfinite(tint.x)||!isfinite(tint.y)||!isfinite(tint.z)||tint.x<0||tint.x>1||tint.y<0||tint.y>1||tint.z<0||tint.z>1)return 0;cell.tint=tint;}
    else if(field==4){if(strlen(text)>=sizeof(cell.sound))return 0;for(const char *p=text;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='_'&&*p!='-'&&*p!='.')return 0;snprintf(cell.sound,sizeof(cell.sound),"%s",text);}
    return 1;
}
static std::vector<std::string> sectorSoundResources(void){
    std::vector<std::string> files;
    auto add=[&](const char *path){std::string name=baseName(path);for(char &c:name)c=(char)tolower((unsigned char)c);
        if(name.size()>4&&name.size()<32&&name.ends_with(".wav"))files.push_back(name);};
    for(auto &entry:customResources.files)add(entry.first.c_str());
    SithWorld *worlds[]={sithWorld_g_pCurrentWorld,sithWorld_g_pStaticWorld};
    for(auto world:worlds)if(world)for(int i=0;i<world->numSoundsLoaded;i++)add(world->sounds[i].sound_fname);
    for(int d=0;d<5;d++){
        jkResGobDirectory *dir=&jkRes_gCtx.aGobDirectories[d];
        for(int g=0;g<dir->numGobs;g++){Gob *gob=dir->gobs[g];if(!gob)continue;
#ifndef STDGOB_COMPACT_ENTRIES
            for(unsigned e=0;e<gob->numFiles;e++)add(gob->entries[e].fname);
#endif
        }
        char path[256];snprintf(path,sizeof(path),"%s%ssound",dir->name,dir->name[0]?"/":"");
        stdFileSearch *search=stdFileUtil_NewFind(path,3,"wav");stdFileSearchResult entry;
        if(search){while(stdFileUtil_FindNext(search,&entry))if(!entry.is_subdirectory)add(entry.fpath);stdFileUtil_DisposeFind(search);}
    }
    std::sort(files.begin(),files.end());files.erase(std::unique(files.begin(),files.end()),files.end());return files;
}
static void sectorSoundDialog(std::vector<EdCell>& draft){
    auto files=sectorSoundResources();std::vector<std::array<char16_t,128>> names(files.size()+1);
    stdString_SafeWStrCopy(names[0].data(),u"<none>",128);int current=0;
    for(size_t i=0;i<files.size();i++){stdString_CharToWchar(names[i+1].data(),files[i].c_str(),128);if(!__strcmpi(draft[0].sound,files[i].c_str()))current=(int)i+1;}
    int choice=chooseList(u"Ambient Sound",reinterpret_cast<char16_t (*)[128]>(names.data()),(int)names.size(),current);
    if(choice<0)return;const char *name=choice?files[choice-1].c_str():"";
    if(choice&&!sithSound_Load((char*)name,0)){jkGuiDialog_ErrorDialog(u"Ambient Sound",u"Could not load that sound.");return;}
    for(auto &cell:draft)snprintf(cell.sound,sizeof(cell.sound),"%s",name);
}
static int sectorGenerateMenu(void){
    char16_t entries[11][128];stdString_SafeWStrCopy(entries[0],u"Destructible",128);stdString_SafeWStrCopy(entries[1],u"Lift_Bottom",128);stdString_SafeWStrCopy(entries[2],u"Lift_Top",128);stdString_SafeWStrCopy(entries[3],u"Water",128);stdString_SafeWStrCopy(entries[4],u"Flowing Water SIM",128);
    stdString_SafeWStrCopy(entries[5],u"Door",128);
    stdString_SafeWStrCopy(entries[6],u"Wind Tunnel",128);
    stdString_SafeWStrCopy(entries[7],u"Lava",128);stdString_SafeWStrCopy(entries[8],u"Acid",128);stdString_SafeWStrCopy(entries[9],u"Electricity",128);
    stdString_SafeWStrCopy(entries[10],u"Spiral Staircase",128);
    return chooseList(u"Generate",entries,11,0);
}
static void sectorAttributesDialog(void){
    std::vector<int> selected;std::vector<EdCell> draft;
    for(int s=0;s<model->count;s++)if(sectorPicks[s]){selected.push_back(s);draft.push_back(model->c[s]);}
    if(selected.empty())return;EdModel original;edCopyModel(&original,model);
    SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;rdVector_Zero3(&player->physicsParams.vel);rdVector_Zero3(&player->physicsParams.acceleration);
    int previousModal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    for(;;){
        const char16_t *labels[]={u"Flags",u"Extra light (0-1)",u"Colormap",u"Tint R G B (0-1)",u"Ambient sound",u"Volume (0-1)"};
        char16_t values[7][128],before[7][128],title[128];jkGuiElement elements[21]={0};char text[160],other[160];
        if(selected.size()==1)snprintf(text,sizeof(text),"Sector %d Attributes",selected[0]);
        else snprintf(text,sizeof(text),"Sector Attributes - %d selected",(int)selected.size());stdString_CharToWchar(title,text,128);
        elements[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,10,600,40},1};
        for(int field=0;field<6;field++){
            sectorFieldText(draft[0],field,text,sizeof(text));int mixed=0;
            for(size_t i=1;i<draft.size();i++){sectorFieldText(draft[i],field,other,sizeof(other));mixed|=strcmp(text,other)!=0;}if(mixed)strcpy(text,"<mixed>");
            stdString_CharToWchar(values[field],text,128);edCopy(before[field],values[field],sizeof(values[field]));
            elements[1+field]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{24,75+field*39,190,26},1};
            elements[8+field]=jkGuiElement{field==0||field==2?ELEMENT_TEXTBUTTON:ELEMENT_TEXTBOX,200+field,0,NULL,127,{220,73+field*39,396,28},1};
        }
        elements[7]=jkGuiElement{ELEMENT_TEXTBUTTON,204,0,NULL,3,{520,229,96,28},1};
        elements[12].rect={220,229,290,28};
        elements[14]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{24,320,592,24},1};
        elements[15]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{24,365,592,48},1};
        elements[16]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,430,230,36},1};
        elements[17]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,430,230,36},1};elements[18]=jkGuiElement{ELEMENT_TEXTBUTTON,205,0,NULL,3,{400,365,216,36},1};elements[19]=jkGuiElement{ELEMENT_TEXTBUTTON,206,0,NULL,3,{160,48,320,24},selected.size()>1};elements[20].type=ELEMENT_END;
        jkGuiMenu menu={elements,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
        elements[0].wstr=title;elements[19].wstr=u"View selected sector numbers...";for(int f=0;f<6;f++){elements[1+f].wstr=labels[f];elements[8+f].wstr=values[f];}
        elements[7].wstr=u"Browse";elements[14].wstr=u"Choose <none> in the sound browser to clear ambient sound.";
        elements[15].rect.width=360;elements[15].wstr=u"Mixed values stay unchanged unless edited.";elements[18].wstr=u"Generate";elements[16].wstr=u"Apply";
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[17]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&elements[16]);
        if((getenv("GEOSITH_SECTOR_TEST")||getenv("GEOSITH_SPLINE_FLAGS_TEST"))&&getenv("GEOSITH_STRESS"))menu.idkFunc=captureSectorAttributes;
        if(getenv("GEOSITH_DESTRUCTIBLE_TEST")&&getenv("GEOSITH_STRESS"))menu.idkFunc=generateMenuTest;
        if(getenv("GEOSITH_WIND_TEST")&&getenv("GEOSITH_STRESS"))menu.idkFunc=windMenuTest;
        int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(elements);if(choice<0)break;
        std::vector<EdCell> edited=draft;int valid=1;
        for(int f=0;f<6;f++)if(f!=0&&f!=2&&memcmp(before[f],values[f],sizeof(values[f]))){stdString_WcharToChar(text,values[f],sizeof(text));for(auto &cell:edited)valid=valid&&sectorFieldSet(cell,f,text);}
        if(!valid){jkGuiDialog_ErrorDialog(u"Sector Attributes",u"Use finite values from 0 to 1 for light, volume and tint.\nSound names must be WAV filenames.");continue;}draft=std::move(edited);
        if(choice==206){
            std::vector<std::array<char16_t,128>> names(selected.size());
            for(size_t i=0;i<selected.size();i++){snprintf(text,sizeof(text),"Sector %d",selected[i]);stdString_CharToWchar(names[i].data(),text,128);}
            chooseList(u"Selected Sector Numbers",reinterpret_cast<char16_t (*)[128]>(names.data()),(int)names.size(),0);
        }
        else if(choice==205){
            if(!edModelsEqual(&original,model)||netEditPending()){say("Level changed or an edit is pending. Reopen sector attributes.");break;}
            int generate=sectorGenerateMenu();if(generate<0)continue;
            if(generate>=7&&generate<=9){
                int material=-1;const char *name=generate==7?"00fyfl01.mat":"00tfuel.mat";
                if(generate!=9){for(int i=0;i<paletteCount;i++)if(!__strcmpi(paletteNames[i],name)){material=i;break;}
                    if(material<0||!editorMaterial(material)){jkGuiDialog_ErrorDialog(u"Hazard",generate==7?u"Could not load 00fyfl01.mat.":u"Could not load 00tfuel.mat.");continue;}}
                if(generate==9&&(!sithTemplate_GetTemplate("ghost")||!sithTemplate_GetTemplate("+force_lightning"))){jkGuiDialog_ErrorDialog(u"Electricity",u"The ghost and Force Lightning templates are required.");continue;}
                edCopyModel(candidate,model);for(size_t i=0;i<selected.size();i++)candidate->c[selected[i]]=draft[i];
                if(!edGenerateHazard(candidate,sectorPicks,material,generate-7)){jkGuiDialog_ErrorDialog(u"Hazard",u"Could not create the hazard. Convert editable splines first and check geometry / COG capacity.");continue;}
                remember();if(commit()){say("Hazard created. Damage / interval are editable in Placed COGs. Ctrl+Z: undo.");break;}continue;
            }
            if(generate==10){if(selected.size()!=1){jkGuiDialog_ErrorDialog(u"Spiral Staircase",u"Select one six-sided sector.");continue;}if(spiralOptions(selected[0],draft[0]))break;continue;}
            if(generate==6){if(selected.size()!=1){jkGuiDialog_ErrorDialog(u"Wind Tunnel",u"Select one sector for the wind gizmo.");continue;}if(windStart(selected[0],draft[0]))break;continue;}
            if(generate==5){if(selected.size()!=1){jkGuiDialog_ErrorDialog(u"Door",u"Select one sector to turn into a door.");continue;}if(doorStart(selected[0],draft[0]))break;continue;}
            if(generate==4){if(selected.size()!=1){jkGuiDialog_ErrorDialog(u"Flowing Water SIM",u"Select one sector as the water source.");continue;}if(waterSimStart(selected[0]))break;continue;}
            if(generate==3){
                int material=-1;for(int i=0;i<paletteCount;i++)if(!__strcmpi(paletteNames[i],"oasis_water.mat")){material=i;break;}
                if(material<0||!editorMaterial(material)||!sithSound_Load("00underwater.wav",0)){jkGuiDialog_ErrorDialog(u"Water",u"Could not load oasis_water.mat or 00underwater.wav from the game resources.");continue;}
                edCopyModel(candidate,model);for(size_t i=0;i<selected.size();i++)candidate->c[selected[i]]=draft[i];
                if(!edGenerateWater(candidate,sectorPicks,material)){jkGuiDialog_ErrorDialog(u"Water",u"Could not configure water sectors. Convert editable splines first and check the selected geometry.");continue;}
                remember();if(commit()){say("Water sectors created with ambient sound and translucent boundary surfaces. Ctrl+Z: undo.");break;}continue;
            }
            if(generate==1||generate==2){
                if(selected.size()!=1){jkGuiDialog_ErrorDialog(u"Lift",u"Select one sector to turn into a lift.");continue;}
                if(!sithTemplate_GetTemplate("_walkstruct")){jkGuiDialog_ErrorDialog(u"Lift",u"The _walkstruct base template is unavailable.");continue;}
                EdPlacedCog cog={};char *script=cogSourceText("12_trooperelev.cog");int parsed=script&&edCogParseScript(&cog,"12_trooperelev.cog",script);edFree(script);
                if(!parsed){jkGuiDialog_ErrorDialog(u"Lift",u"Could not load the game's 12_trooperelev.cog elevator script.");continue;}
                char name[32];for(int n=1;;n++){snprintf(name,sizeof(name),"gs_lift%d",n);int exists=sithTemplate_GetTemplate(name)!=NULL||edTemplateIndex(model,name)>=0;for(int a=0;a<model->assetCount;a++)exists|=!strcmp(model->assets[a].name,name);if(!exists)break;}
                edCopyModel(candidate,model);candidate->c[selected[0]]=draft[0];
                if(!edGenerateLift(candidate,selected[0],name,cog,generate==2)){jkGuiDialog_ErrorDialog(u"Lift",u"Could not create the lift. The selected sector must be taller than 0.03125 units, with a shaft below for the lowered model.\nConvert editable splines first and check custom template capacity.");continue;}
                remember();if(commit()){sectorPicks.clear();picks.count=0;say("Lift generated with two frames and a step-on elevator COG. Ctrl+Z: undo.");break;}continue;
            }
            if(!sithTemplate_GetTemplate("_walkstruct")||!sithTemplate_GetTemplate("_throwable")||!sithTemplate_GetTemplate("_explosion")){jkGuiDialog_ErrorDialog(u"Generate",u"Required game templates are unavailable.");continue;}
            char name[32];for(int n=1;;n++){snprintf(name,sizeof(name),"gs_wall%d",n);int exists=sithTemplate_GetTemplate(name)!=NULL||edTemplateIndex(model,name)>=0;for(int a=0;a<model->assetCount;a++)exists|=!strcmp(model->assets[a].name,name);if(!exists)break;}
            edCopyModel(candidate,model);for(size_t i=0;i<selected.size();i++)candidate->c[selected[i]]=draft[i];
            if(!edGenerateDestructible(candidate,sectorPicks,name)){jkGuiDialog_ErrorDialog(u"Destructible",u"Could not fracture these sectors. Convert editable splines first.\nRequires free custom template capacity (14 by default), valid convex cells, and at most 32 selected sectors.");continue;}
            remember();if(commit()){sectorPicks.clear();picks.count=0;say("Destructible wall generated with debris and a placed COG. Explosion/saber damage breaks it. Ctrl+Z: undo.");break;}
        }
        else if(choice==200)sectorFlagsDialog(draft);
        else if(choice==204)sectorSoundDialog(draft);
        else if(choice==202){std::vector<std::array<char16_t,128>> names(colormapCount+1);stdString_SafeWStrCopy(names[0].data(),u"<level default>",128);int current=0;
            for(int i=0;i<colormapCount;i++){stdString_CharToWchar(names[i+1].data(),editorColormaps[i].name,128);if(!__strcmpi(draft[0].colormap,editorColormaps[i].name))current=i+1;}
            int id=chooseList(u"Sector Colormap",reinterpret_cast<char16_t (*)[128]>(names.data()),(int)names.size(),current);
            if(id>=0){const char *name=id?editorColormaps[id-1].name:"";if(id&&!sectorColormapAvailable(name))jkGuiDialog_ErrorDialog(u"Colormap",u"Could not load that colormap.");else for(auto &cell:draft)snprintf(cell.colormap,128,"%s",name);}
        }else if(choice==1){
            if(!edModelsEqual(&original,model)){say("The level changed while this menu was open. Reopen sector attributes.");break;}
            int soundsValid=1;for(auto &cell:draft)if(cell.sound[0]&&!sithSound_Load(cell.sound,0))soundsValid=0;
            if(!soundsValid){jkGuiDialog_ErrorDialog(u"Sound",u"Could not load that ambient sound.");continue;}
            remember();edCopyModel(candidate,model);for(size_t i=0;i<selected.size();i++)candidate->c[selected[i]]=draft[i];
            if(commit()){say("Sector attributes updated. Ctrl+Z: undo.");break;}
        }
    }
    jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=previousModal;stdControl_SetActivation(1);stdControl_Flush();
}

static int stressSectors(SithThing *p){
    static int frame;stressRunning=1;active=1;tool=1;
    if(frame++<5)return 1;
    edCopyModel(candidate,model);if(!edExtrudeStep(candidate,0,4)||!commit())exit(2);
    clearHistory();sectorPicks[0]=sectorPicks[1]=1;mergeSelectedSectors();
    if(model->count!=1||!sectorPicks[0]||!historyStep(0)||model->count!=2||!historyStep(1)||model->count!=1)exit(2);
    remember();edCopyModel(candidate,model);strcpy(candidate->c[0].colormap,"01narsh.cmp");strcpy(candidate->c[0].layer,"Sector test");if(!commit())exit(2);
    SithWorld *world=sithWorld_g_pCurrentWorld;if(world->numColormaps!=2||world->aSectors[0].colormap!=&world->colormaps[1])exit(2);
    sectorPicks[0]=1;sectorAttributesDialog();
    if(fabs(model->c[0].extraLight-.35)>.00001||fabs(world->aSectors[0].extraLight-.35)>.00001||!(model->c[0].flags&1)||fabs(model->c[0].tint.y-.2)>.00001||strcmp(model->c[0].layer,"Sector test"))exit(2);
    if(!historyStep(0)||model->c[0].flags&1||!historyStep(1)||strcmp(model->c[0].layer,"Sector test"))exit(2);
    sectorPicks[0]=1;mapOpen(p);if(!mapSelected[0])exit(2);mapClose(p);if(!sectorPicks[0])exit(2);
    if(!writeEditorGob(model,"Episode/sithed_sector_test.gob","Sector attributes test",0))exit(2);
    printf("GEOSITH_STRESS PASS: sector merge, flags/attributes menu, per-sector colormap, map selection transfer, undo/redo and save, %d rendered frames\n",stressRenderFrames);fflush(stdout);
    /* Unlike normal game exit, this opt-in harness ends inside the game loop.
       Close OpenAL before CRT teardown joins its audio backend thread. */
    sithSound_Shutdown();exit(0);
}
