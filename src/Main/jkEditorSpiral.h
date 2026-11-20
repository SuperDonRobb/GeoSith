static std::unique_ptr<EdModel> spiralBefore,spiralSource,spiralDraft;
static int spiralSector,spiralSteps=8,spiralValid,spiralEnterDown,spiralPost,spiralPostDown;
static double spiralRotation,spiralGap;
static int spiralActive(){return spiralBefore!=nullptr;}
static int spiralCancel(){int was=spiralActive();spiralBefore.reset();spiralSource.reset();spiralDraft.reset();if(was)say("Spiral staircase cancelled.");return was;}
static void spiralPreview(){spiralValid=edCreateSpiralStairs(spiralDraft.get(),spiralSource.get(),spiralSector,spiralSteps,spiralRotation,spiralPost,spiralGap);}
static int spiralStart(int sector,const EdCell &cell){
    if(netEditPending())return 0;
    spiralBefore=std::make_unique<EdModel>(*model);spiralSource=std::make_unique<EdModel>(*model);spiralSource->c[sector]=cell;spiralDraft=std::make_unique<EdModel>();spiralSector=sector;spiralPostDown=0;spiralPreview();
    if(!spiralValid){spiralCancel();jkGuiDialog_ErrorDialog(u"Spiral Staircase",u"Select a six-sided sector with vertical walls, a level floor and ceiling, and enough height for the selected step count. Convert editable splines first.");return 0;}
    if(mapActive)mapClose(sithWorld_g_pCurrentWorld->pLocalPlayer);grabbing=groupGrab=cleaving=0;fireDown=grabDown=spiralEnterDown=1;
    say("Spiral staircase: Wheel changes steps. Fire 1 / Enter applies. Escape cancels.");return 1;
}
// Invoked from Sector Attributes, which owns the modal GUI / input lifecycle.
static int spiralOptions(int sector,const EdCell &cell){
    char16_t values[3][64];char text[96];
    snprintf(text,sizeof(text),"%d",spiralSteps);stdString_CharToWchar(values[0],text,64);
    snprintf(text,sizeof(text),"%.6g",spiralGap);stdString_CharToWchar(values[1],text,64);
    snprintf(text,sizeof(text),"%.6g",spiralRotation*180/3.141592653589793);stdString_CharToWchar(values[2],text,64);
    int post=spiralPost;
    for(;;){
        jkGuiElement e[12]={0};e[0]={ELEMENT_TEXT,0,2,NULL,3,{20,10,600,40},1};
        for(int i=0;i<3;i++){e[1+i]={ELEMENT_TEXT,0,0,NULL,3,{30,85+i*52,270,28},1};e[4+i]={ELEMENT_TEXTBOX,0,0,NULL,63,{320,85+i*52,280,28},1};}
        e[7]={ELEMENT_CHECKBOX,0,0,NULL,post,{30,260,570,28},1};
        e[8]={ELEMENT_TEXT,0,0,NULL,3,{30,310,580,85},1};
        e[9]={ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,430,230,36},1};e[10]={ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,430,230,36},1};e[11].type=ELEMENT_END;
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
        e[0].wstr=u"Spiral Staircase Options";e[1].wstr=u"Number of steps (1-64)";e[2].wstr=u"Gap between steps";e[3].wstr=u"Rotation (degrees)";
        for(int i=0;i<3;i++)e[4+i].wstr=values[i];
        e[7].wstr=u"Solid center post (remove central sectors)";
        e[8].wstr=u"Gap is vertical: 0 closes the risers. Maximum is less than 0.0625. Rise: 0.0625, 16 steps per turn. Accept opens the 3D preview.";e[9].wstr=u"Accept";
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[10]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&e[9]);
        int choice=jkGuiRend_DisplayAndReturnClicked(&menu);post=e[7].boxChecked;freeMenuStrings(e);if(choice<0)return 0;
        double parsed[3];int valid=1;char extra;
        for(int i=0;i<3;i++){stdString_WcharToChar(text,values[i],sizeof(text));if(sscanf(text," %lf %c",&parsed[i],&extra)!=1||!std::isfinite(parsed[i]))valid=0;}
        if(!valid||parsed[0]<1||parsed[0]>64||floor(parsed[0])!=parsed[0]||parsed[1]<0||parsed[1]>=ED_GRID-ED_EPS){jkGuiDialog_ErrorDialog(u"Spiral Staircase",u"Use 1-64 whole steps, a gap from 0 to less than 0.0625, and a finite rotation angle.");continue;}
        int oldSteps=spiralSteps,oldPost=spiralPost;double oldGap=spiralGap,oldRotation=spiralRotation;
        spiralSteps=(int)parsed[0];spiralPost=post;spiralGap=parsed[1];spiralRotation=fmod(parsed[2],360)*3.141592653589793/180;
        if(spiralStart(sector,cell))return 1;
        spiralSteps=oldSteps;spiralPost=oldPost;spiralGap=oldGap;spiralRotation=oldRotation;
    }
}
static int spiralApply(){
    if(!spiralActive()||!spiralValid||netEditPending())return 0;
    if(!edModelsEqual(model,spiralBefore.get())){spiralCancel();say("Level changed. Reopen the staircase generator.");return 0;}
    edCopyModel(candidate,spiralDraft.get());remember();if(!commit())return 0;
    spiralBefore.reset();spiralSource.reset();spiralDraft.reset();picks.count=0;sectorPicks.clear();hover.cell=-1;fireDown=grabDown=attributesDown=1;
    say("Spiral staircase created. Ctrl+Z: undo.");return 1;
}
static int spiralControl(){
    if(!spiralActive())return 0;int fire=editButton(INPUT_FUNC_FIRE1),enter=stdControl_ReadKey(DIK_RETURN,NULL)||stdControl_ReadKey(DIK_NUMPADENTER,NULL);
    int apply=(fire&&!fireDown)||(enter&&!spiralEnterDown);fireDown=fire;spiralEnterDown=enter;
    int wheel=stdControl_aAxisStates[AXIS_MOUSE_Z];if(wheel){if(stdControl_ReadKey(DIK_LSHIFT,NULL)||stdControl_ReadKey(DIK_RSHIFT,NULL))spiralRotation+=(wheel>0?1:-1)*3.141592653589793/36;else spiralSteps=std::clamp(spiralSteps+(wheel>0?1:-1),1,64);spiralPreview();}
    int postKey=stdControl_ReadKey(DIK_P,NULL);if(postKey&&!spiralPostDown){spiralPost=!spiralPost;spiralPreview();}spiralPostDown=postKey;
    if(apply)spiralApply();return 1;
}
static void spiralOverlay(){
    if(!spiralActive())return;int color=palettePixel(highlightCanvas->pVBuffer,255,spiralValid?220:50,40);
    if(spiralValid)for(int c=0;c<spiralDraft->count;c++)if(c==spiralSector||c>=spiralSource->count)for(int f=0;f<spiralDraft->c[c].count;f++){const EdFace &face=spiralDraft->c[c].f[f];if(face.neighbor>=0&&!(edSurfaceAttributes(&face).surfaceFlags&4))continue;for(int v=0;v<face.count;v++)line(face.v[v].p,face.v[(v+1)%face.count].p,color);}
    char text[240];snprintf(text,sizeof(text),"Spiral: %d steps | Shift+wheel: rotate | P: post %s | Enter / Fire 1: apply | Esc: cancel%s",spiralSteps,spiralPost?"on":"off",spiralValid?"":" | Too tall / invalid");
    editorDrawText(highlightCanvas->pVBuffer,jkGui_stdFonts[0],8,70,highlightCanvas->pVBuffer->format.width-16,text,1);
}

static int spiralTest(){
    static int stage=0;static std::unique_ptr<EdModel> before;
    if(!stage){clearHistory();before=std::make_unique<EdModel>(*model);if(!spiralStart(0,model->c[0]))return -1;stage++;return 0;}
    if(!jkEditor_HandleEscape()||spiralActive()||undoCount||!edModelsEqual(model,before.get()))return -1;
    if(!spiralStart(0,model->c[0]))return -1;spiralSteps=10;spiralPreview();if(!spiralValid||!spiralApply()||model->count<10)return -1;
    if(!historyStep(0)||!edModelsEqual(model,before.get())||!historyStep(1)||model->count<10)return -1;
    if(!writeEditorGob(model,"Episode/sithed_spiral_test.gob","Spiral test",0))return -1;
    puts("GEOSITH_SPIRAL PASS: preview, cancellation, step adjustment, apply, undo/redo, GOB save");return 1;
}
