static std::unique_ptr<EdModel> bevelBefore,bevelDraft;
static std::vector<int> bevelChanged;
static EdSelection bevelSelection;
static double bevelWidth;
static int bevelEnterDown,bevelValid;
static int bevelActive(){return bevelBefore!=nullptr;}
static int bevelCancel(){int was=bevelActive();bevelBefore.reset();bevelDraft.reset();bevelChanged.clear();bevelValid=0;if(was)say("Bevel cancelled.");return was;}
static void bevelPreview(){
    bevelValid=edBevelSelection(bevelDraft.get(),bevelBefore.get(),&bevelSelection,bevelWidth);bevelChanged.clear();
    if(bevelValid)for(int c=0;c<bevelDraft->count;c++)if(c>=bevelBefore->count||!edCellsEqual(&bevelDraft->c[c],&bevelBefore->c[c]))bevelChanged.push_back(c);
    char text[180];snprintf(text,sizeof(text),"Bevel %.5g | Wheel: width | Fire 1 / Enter: apply | Escape: cancel%s",bevelWidth,bevelValid?"":" | Invalid width: reduce it");say(text);
}
static int bevelStart(){
    if(netEditPending()){say("Wait for host approval before beveling.");return 0;}
    if(!picks.count){say("Select edges, then press B to bevel them together.");return 0;}
    for(int i=0;i<picks.count;i++){
        EdHit hit=picks.hits[i];if(hit.edge<0||hit.vertex>=0){say("Select only edges to bevel them together.");return 0;}
        if(model->c[hit.cell].splineId){say("Convert the editable spline to sectors before beveling.");return 0;}
    }
    bevelSelection=picks;
    bevelBefore=std::make_unique<EdModel>(*model);bevelDraft=std::make_unique<EdModel>();bevelWidth=ED_GRID;bevelEnterDown=1;fireDown=grabDown=1;bevelPreview();return 1;
}
static int bevelApply(){
    if(!bevelActive()||!bevelValid)return 0;
    if(netEditPending()){say("Wait for host approval before applying the bevel.");return 0;}
    if(!edModelsEqual(model,bevelBefore.get())){bevelCancel();say("Level changed. Select the edge again to bevel it.");return 0;}
    edCopyModel(candidate,bevelDraft.get());remember();if(!commit())return 0;
    bevelBefore.reset();bevelDraft.reset();bevelChanged.clear();picks.count=0;sectorPicks.clear();hover.cell=-1;
    fireDown=grabDown=attributesDown=1;edResetDoubleTap(&selectionTap,1);say("Bevel applied. Ctrl+Z: undo.");return 1;
}
static int bevelControl(){
    if(!bevelActive())return 0;
    int fire=editButton(INPUT_FUNC_FIRE1),enter=stdControl_ReadKey(DIK_RETURN,NULL)||stdControl_ReadKey(DIK_NUMPADENTER,NULL);
    int apply=(fire&&!fireDown)||(enter&&!bevelEnterDown);fireDown=fire;bevelEnterDown=enter;
    int wheel=stdControl_aAxisStates[AXIS_MOUSE_Z];if(wheel){bevelWidth=fmax(ED_GRID*.5,fmin(64,bevelWidth+(wheel>0?ED_GRID*.5:-ED_GRID*.5)));bevelPreview();}
    if(apply)bevelApply();return 1;
}
static void bevelOverlay(){
    if(!bevelActive())return;int color=palettePixel(highlightCanvas->pVBuffer,255,bevelValid?220:50,40);
    if(bevelValid)for(int c:bevelChanged)for(int f=0;f<bevelDraft->c[c].count;f++){const EdFace &face=bevelDraft->c[c].f[f];for(int v=0;v<face.count;v++)line(face.v[v].p,face.v[(v+1)%face.count].p,color);}
    else for(int i=0;i<bevelSelection.count;i++){EdHit h=bevelSelection.hits[i];const EdFace &face=bevelBefore->c[h.cell].f[h.face];line(face.v[h.edge].p,face.v[(h.edge+1)%face.count].p,color);}
    char text[180];snprintf(text,sizeof(text),"Bevel width %.5g | Wheel: width | Fire 1 / Enter: apply | Escape: cancel%s",bevelWidth,bevelValid?"":" | Invalid width");
    editorDrawText(highlightCanvas->pVBuffer,jkGui_stdFonts[0],8,70,highlightCanvas->pVBuffer->format.width-16,text,1);
}

static int bevelTest(){
    static int stage=0;static std::unique_ptr<EdModel> before;
    if(stage==0){clearHistory();before=std::make_unique<EdModel>(*model);picks.count=2;picks.hits[0]={0,0,-1,0};picks.hits[1]={0,0,-1,1};if(!bevelStart()||!bevelValid)return -1;stage++;return 0;}
    if(stage==1){
        if(!jkEditor_HandleEscape()||bevelActive()||undoCount||!edModelsEqual(model,before.get()))return -1;
        if(!bevelStart())return -1;bevelWidth=ED_GRID*2;bevelPreview();if(!bevelValid||!bevelApply()||model->c[0].count!=8||!historyStep(0)||!edModelsEqual(model,before.get())||!historyStep(1))return -1;
        if(!historyStep(0))return -1;edCopyModel(candidate,model);if(!edExtrude(candidate,0,4,edV(0,1,0))||!commit())return -1;
        clearHistory();before=std::make_unique<EdModel>(*model);picks.count=2;picks.hits[0]={0,4,-1,0};picks.hits[1]={0,4,-1,1};if(!bevelStart()||!bevelValid)return -1;stage++;return 0;
    }
    if(!bevelApply()||model->count!=3||!historyStep(0)||!edModelsEqual(model,before.get())||!historyStep(1)||model->count!=3)return -1;
    if(!writeEditorGob(model,"Episode/sithed_bevel_test.gob","Bevel test",0))return -1;
    puts("GEOSITH_BEVEL PASS: preview, Escape cancellation, width adjustment, apply, undo/redo, expanded adjoin and GOB save");return 1;
}
