static void arrayTestMenu(jkGuiMenu *menu){
    static int frame;if(++frame%3)return;const char16_t *title=menu->paElements[0].wstr;const char *suffix="groups";
    if(title[0]=='N'){stdString_CharToWchar((char16_t*)menu->paElements[1].wstr,"Hallway Module",64);menu->lastClicked=1;suffix="name";}
    else if(title[0]=='A'){menu->lastClicked=102;suffix="axis";}
    else if(title[0]=='S'&&title[7]=='C'){
        menu->lastClicked=3;suffix="shapes";for(int i=1;i<=10;i++)if(menu->paElements[i].bIsVisible&&(int)menu->paElements[i].otherDataPtr>32){menu->lastClicked=99+i;break;}
    }else if(title[0]=='S'&&title[7]=='S'){menu->lastClicked=menu->paElements[7].wstr[0]=='G'?1:20;suffix="spline";}
    else {menu->lastClicked=!model->groupCount?1:!model->groups[0].copies?100:-1;if(menu->lastClicked==100)stdString_CharToWchar((char16_t*)menu->paElements[3].wstr,"3",16);}
#ifdef SDL2_RENDER
    if(getenv("GEOSITH_ARRAY_CAPTURE")){char path[700];snprintf(path,sizeof(path),"%s.%s.png",getenv("GEOSITH_ARRAY_CAPTURE"),suffix);std3D_ScreenshotWindow(path);}
#endif
}
/* Map-view sector group and array controls. */
static int groupNameDialog(char name[64]){
    char16_t value[64]=u"Group";
    jkGuiElement e[]={{ELEMENT_TEXT,0,2,NULL,3,{30,80,580,40},1},{ELEMENT_TEXTBOX,0,0,NULL,63,{80,180,480,32},1},{ELEMENT_TEXTBUTTON,1,2,NULL,3,{340,300,220,40},1},{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{80,300,220,40},1},{ELEMENT_END}};
    jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"Name the selected sector group";e[1].wstr=value;e[2].wstr=u"Create Group";jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[3]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&e[2]);
    if(getenv("GEOSITH_ARRAY_TEST")&&getenv("GEOSITH_STRESS"))menu.idkFunc=arrayTestMenu;int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);if(choice!=1)return 0;stdString_WcharToChar(name,value,64);return name[0]!=0;
}
static void arrayMenu(void){
    if(netEditPending())return;
    int modal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    int page=0;
    for(;;){
        EdModel original=*model;char16_t names[5][128],counts[5][16];jkGuiElement e[27]={};
        e[0]={ELEMENT_TEXT,0,2,NULL,3,{20,16,600,40},1};
        e[1]={ELEMENT_TEXTBUTTON,1,2,NULL,3,{20,65,300,35},1};
        for(int i=0;i<5;i++){int g=page*5+i,shown=g<model->groupCount;
            e[2+i*3]={ELEMENT_TEXT,0,0,NULL,3,{20,118+i*48,250,35},shown};e[3+i*3]={ELEMENT_TEXTBOX,0,0,NULL,15,{275,118+i*48,55,32},shown};e[4+i*3]={ELEMENT_TEXTBUTTON,100+i,0,NULL,3,{340,118+i*48,150,35},shown};
            e[20+i]={ELEMENT_TEXTBUTTON,200+i,0,NULL,3,{505,118+i*48,115,35},shown};
            if(shown){char label[128],count[16];snprintf(label,sizeof(label),"%s (%d sectors)",model->groups[g].name,model->groups[g].count);stdString_CharToWchar(names[i],label,128);snprintf(count,sizeof(count),"%d",model->groups[g].copies?model->groups[g].copies:1);stdString_CharToWchar(counts[i],count,16);}
        }
        e[17]={ELEMENT_TEXTBUTTON,2,2,NULL,3,{20,410,160,36},1};e[18]={ELEMENT_TEXTBUTTON,3,2,NULL,3,{460,410,160,36},1};e[19]={ELEMENT_TEXTBUTTON,-1,2,NULL,3,{235,410,170,36},1};e[25]={ELEMENT_TEXT,0,0,NULL,3,{20,365,600,35},1};e[26].type=ELEMENT_END;
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"Sector Groups / Arrays - additional copies";e[1].wstr=u"+ Create Group from Selection";e[17].wstr=u"Previous";e[18].wstr=u"Next";e[19].wstr=u"Close";e[25].wstr=u"Delete keeps placed geometry; group splines become sectors.";
        for(int i=0;i<5;i++){e[2+i*3].wstr=names[i];e[3+i*3].wstr=counts[i];e[4+i*3].wstr=u"Add Array...";e[20+i].wstr=u"Delete";}
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[19]);if(getenv("GEOSITH_ARRAY_TEST")&&getenv("GEOSITH_STRESS"))menu.idkFunc=arrayTestMenu;int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);if(choice<0)break;
        if(choice==2){page=(std::max)(0,page-1);continue;}if(choice==3){page=(std::min)((std::max)(0,(model->groupCount-1)/5),page+1);continue;}
        if(!edModelsEqual(&original,model)||netEditPending()){say("Level changed. Reopen the groups menu.");break;}
        if(choice==1){
            int count=0;for(int c=0;c<model->count;c++)count+=mapSelected[c]!=0;
            if(!count){jkGuiDialog_ErrorDialog(u"Sector Group",u"Select one or more sectors in the map first.");continue;}
            char name[64];if(!groupNameDialog(name))continue;edCopyModel(candidate,model);
            if(!edGroupCapture(candidate,mapSelected,name)){jkGuiDialog_ErrorDialog(u"Sector Group",u"Could not capture this group. Choose a printable name and valid sectors.");continue;}
            remember();if(commit()){page=(model->groupCount-1)/5;say("Sector group created. Available in array and spline menus.");}
        }else if(choice>=200&&choice<205){
            int id=page*5+choice-200+1;edCopyModel(candidate,model);
            if(!edGroupDelete(candidate,id))continue;
            remember();if(commit()){page=(std::min)(page,(std::max)(0,(model->groupCount-1)/5));say("Group deleted. Placed geometry kept. Ctrl+Z: undo.");}
        }else if(choice>=100&&choice<105){
            int row=choice-100,g=page*5+row;if(g>=model->groupCount)continue;char text[32],extra;int copies;stdString_WcharToChar(text,counts[row],32);
            if(sscanf(text," %d %c",&copies,&extra)!=1||copies<0||copies>256){jkGuiDialog_ErrorDialog(u"Array Copies",u"Enter 0-256 additional copies. Zero removes the generated array.");continue;}
            char16_t axes[6][128]={u"+X",u"-X",u"+Y",u"-Y",u"+Z",u"-Z"};int direction=chooseList(u"Array direction / spline forward axis",axes,6,model->groups[g].direction);if(direction<0)continue;
            if(!edModelsEqual(&original,model)||netEditPending())break;
            if(!edGroupArray(candidate,model,g+1,direction,copies)){jkGuiDialog_ErrorDialog(u"Array",u"Cannot create a valid array here. Try fewer copies or another direction.");continue;}
            remember();if(commit())say("Array updated. Copies use the group size for spacing. Ctrl+Z: undo.");
        }
    }
    jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=modal;stdControl_SetActivation(1);stdControl_Flush();fireDown=grabDown=1;
}

static int arraySelfTest(SithThing *player){
    clearHistory();mapOpen(player);mapSelected.clear();mapSelected[0]=1;arrayMenu();
    if(model->groupCount!=1||model->groups[0].copies!=3||model->count!=4)return 0;
    EdModel array=*model;if(!historyStep(0)||model->count!=1||!historyStep(1)||!edModelsEqual(model,&array))return 0;
    EdSpline spline={};spline.id=1;spline.count=3;spline.sides=4;spline.segments=4;spline.width=2;spline.height=1;spline.material=0;
    spline.points[0]={edV(8,0,.5),edV(0,2,0)};spline.points[1]={edV(8,8,.5),edV(1,2,0)};spline.points[2]={edV(12,14,1.5),edV(2,1,0)};
    mapSelected.clear();if(!splinePublish(spline))return 0;splineSelected=1;splinePoint=-1;splineAttributes();
    if(model->splines[0].groupId!=1)return 0;EdModel curved=*model;
    if(!historyStep(0)||model->splines[0].groupId||!historyStep(1)||!edModelsEqual(model,&curved))return 0;
    if(!writeEditorGob(model,"Episode/sithed_array_test.gob","Array test",1))return 0;
    mapOpen(player);mapCenter=edV(5,7,0);mapZoom=20;mapSelected.clear();return 1;
}
