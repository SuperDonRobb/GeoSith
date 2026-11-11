/* Editable spline controls use the normal document transaction path. */
static int splineSelected,splinePoint=-1,splineDrawing,splineDragging,splineHandle,splineStartDown,splineEnterDown,splineDragRight;
static EdSpline splineDraft;
static int splineLastShape=4;
static EdVec splineDragStart,splineDragDelta;
static EdDragTrace splineDragTrace;
static int splineIndex(int id){for(int i=0;i<model->splineCount;i++)if(model->splines[i].id==id)return i;return -1;}
// A clicked point can have a zero tangent. Expose a usable handle without
// changing existing geometry until the user actually drags it.
static EdVec splineVisibleHandle(const EdSpline &s,int point){
    EdVec h=s.points[point].handle;if(edDot(h,h)>ED_EPS*ED_EPS)return h;
    if(s.count<2)return edV(ED_GRID*4,0,0);
    int before=point?point-1:s.closed?s.count-1:0,after=point+1<s.count?point+1:s.closed?0:point;
    h=edScale(edSub(s.points[after].position,s.points[before].position),1.0/3);
    return edDot(h,h)>ED_EPS*ED_EPS?h:edV(ED_GRID*4,0,0);
}
static int splinePublish(const EdSpline &draft){
    if(netEditPending()){say("Wait for the host before changing a spline.");return 0;}
    int textureHeight=64;rdMaterial *material=editorMaterial(draft.material);
    if(material&&rdMaterial_EnsureDataForced(material)&&material->num_texinfo&&material->texinfos[0]&&material->texinfos[0]->texture_ptr){tVBuffer *texture=material->texinfos[0]->texture_ptr->texture_struct[0];if(texture&&texture->format.height>0)textureHeight=texture->format.height;}
    if(!edSplineUpdate(candidate,model,&draft,textureHeight)){say("Spline unchanged: curve is too tight, overlaps completely, or produces invalid geometry. Adjust width, handles or sector count.");return 0;}
    remember();if(!commit())return 0;splineLastShape=draft.sides;int i=splineIndex(draft.id);if(i>=0&&model->splines[i].invalid)say("Invalid spline: red map preview only. Adjust points, handles or width to build it.");return 1;
}
static int splineDeleteSelected(void){
    int index=splineIndex(splineSelected);if(index<0)return 0;
    if(netEditPending()||splineDragging){say("Finish the current edit before deleting a spline or point.");return 1;}
    EdSpline draft=model->splines[index];
    if(splinePoint>=0&&splinePoint<draft.count&&draft.count>1){
        for(int i=splinePoint+1;i<draft.count;i++)draft.points[i-1]=draft.points[i];draft.count--;draft.points.resize(draft.count);if(draft.count<3)draft.closed=0;
        if(splinePublish(draft)){splinePoint=(std::min)(splinePoint,draft.count-1);say("Spline point deleted. Ctrl+Z: undo.");}return 1;
    }
    EdSpline empty=draft;empty.count=1;empty.closed=0;
    if(!edSplineUpdate(candidate,model,&empty)){say("Cannot remove this spline safely.");return 1;}
    for(int i=0;i<candidate->splineCount;i++)if(candidate->splines[i].id==splineSelected){for(int j=i+1;j<candidate->splineCount;j++)candidate->splines[j-1]=candidate->splines[j];candidate->splineCount--;break;}
    remember();if(commit()){splineSelected=splineDrawing=0;splinePoint=-1;say("Spline and generated sectors deleted. Ctrl+Z: undo.");}return 1;
}
static void splineFinish(void){
    int index=splineIndex(splineSelected),unfinished=splineDrawing&&index>=0&&model->splines[index].count<2;
    splineDrawing=splineDragging=0;grabbing=0;
    if(unfinished){splinePoint=-1;splineDeleteSelected();return;}
    say("Spline finished. Select the curve or a point; Enter edits its attributes.");}
static void splineBegin(EdVec position){
    if(netEditPending())return;EdSpline s={};s.id=1;for(int i=0;i<model->splineCount;i++)s.id=(std::max)(s.id,model->splines[i].id+1);
    s.count=1;s.sides=splineLastShape;s.segments=8;s.width=s.height=1;s.material=currentMaterial;s.points[0]={position,edV(0,0,0)};
    if(splinePublish(s)){splineSelected=s.id;splinePoint=0;splineDrawing=1;picks.count=0;sectorPicks.clear();say("Spline: click for points; hold and drag for tangent handles. Escape / RMB finishes.");}
}
static void splineShapeDraw(jkGuiElement *e,jkGuiMenu *menu,tVBuffer *buffer,BOOL redraw){
    if(redraw)jkGuiRend_CopyVBuffer(menu,&e->rect);int sides=(int)e->otherDataPtr;stdDisplay_VBufferLock(buffer);rdCanvas canvas={};canvas.pVBuffer=buffer;canvas.widthMinusOne=buffer->format.width-1;canvas.heightMinusOne=buffer->format.height-1;
    int color=palettePixel(buffer,80,255,240),cx=e->rect.x+e->rect.width/2,cy=e->rect.y+27;double start=sides==4?3.141592653589793/4:3.141592653589793/2;
    if(sides>32){int group=sides-33;stdFont_DrawAscii(buffer,menu->fonts[0],e->rect.x+2,e->rect.y+15,e->rect.width-4,"Sector group",1);stdFont_DrawAscii(buffer,menu->fonts[0],e->rect.x+2,e->rect.y+55,e->rect.width-4,model->groups[group].name,1);stdDisplay_VBufferUnlock(buffer);return;}
    for(int i=0;i<sides;i++){double a=start+i*6.283185307179586/sides,b=start+(i+1)*6.283185307179586/sides;rdPrimit2_DrawLine(&canvas,cx+(int)(22*cos(a)),cy-(int)(22*sin(a)),cx+(int)(22*cos(b)),cy-(int)(22*sin(b)),color,-1);}
    char name[48];shapeLabel(sides,name,sizeof(name));if(sides==32)strcpy(name,"Circle (32)");stdFont_DrawAscii(buffer,menu->fonts[0],e->rect.x+2,e->rect.y+58,e->rect.width-4,name,1);stdDisplay_VBufferUnlock(buffer);
}
static void splineMenuTest(jkGuiMenu *menu){
    static int frame,stage;if(++frame%3)return;
    if(getenv("GEOSITH_SPLINE_CONVERT_CAPTURE")){
#ifdef SDL2_RENDER
        std3D_ScreenshotWindow(getenv("GEOSITH_SPLINE_CONVERT_CAPTURE"));
#endif
        menu->lastClicked=22;return;
    }
#ifdef SDL2_RENDER
    const char *path=menu->paElements[1].type==ELEMENT_TEXTBUTTON?getenv("GEOSITH_SPLINE_SHAPE_CAPTURE"):getenv("GEOSITH_SPLINE_MENU_CAPTURE");if(path)std3D_ScreenshotWindow(path);
#endif
    if(menu->paElements[1].type==ELEMENT_TEXTBUTTON)menu->lastClicked=101;
    else {int choices[]={20,21,1};menu->lastClicked=choices[(std::min)(stage++,2)];}
}
static int splineShapeMenu(int sides){
    int page=(sides<0?30-sides-1:sides-3)/10;
    for(;;){jkGuiElement e[15]={};e[0]={ELEMENT_TEXT,0,2,NULL,3,{20,15,600,40},1};
        for(int i=0;i<10;i++){int n=3+page*10+i;e[i+1]={ELEMENT_TEXTBUTTON,100+i,0,NULL,3,{20+(i%5)*120,90+(i/5)*115,112,100},n<=32+model->groupCount};e[i+1].drawFuncOverride=splineShapeDraw;e[i+1].otherDataPtr=n;}
        e[11]={ELEMENT_TEXTBUTTON,2,2,NULL,3,{20,400,180,40},1};e[12]={ELEMENT_TEXTBUTTON,3,2,NULL,3,{440,400,180,40},1};e[13]={ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{230,400,180,40},1};e[14].type=ELEMENT_END;
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"Spline Cross-section";e[11].wstr=u"Previous";e[12].wstr=u"Next";jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[13]);if((getenv("GEOSITH_SPLINE_MENU_CAPTURE")||getenv("GEOSITH_SPLINE_CONVERT_CAPTURE"))&&getenv("GEOSITH_SPLINE_TEST"))menu.idkFunc=splineMenuTest;if(getenv("GEOSITH_ARRAY_TEST")&&getenv("GEOSITH_STRESS"))menu.idkFunc=arrayTestMenu;int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);
        if(choice>=100&&choice<110){int item=page*10+choice-100;return item<30?item+3:-(item-29);}if(choice==2)page=(std::max)(0,page-1);else if(choice==3)page=(std::min)((29+model->groupCount)/10,page+1);else return sides;
    }
}
static int splineConvertSelected(void){
    if(netEditPending())return 0;edCopyModel(candidate,model);if(!edSplineConvert(candidate,splineSelected))return 0;
    remember();if(!commit())return 0;splineSelected=splineDrawing=splineDragging=0;splinePoint=-1;say("Spline converted to sectors. Geometry tools are now available. Ctrl+Z restores the spline.");return 1;
}
static void splineAttributes(void){
    int index=splineIndex(splineSelected);if(index<0||netEditPending())return;EdSpline draft=model->splines[index];int point=splinePoint;
    if(point>=draft.count)point=-1;EdModel original;edCopyModel(&original,model);
    int modal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    for(;;){char16_t values[3][128],shape[128],texture[160];char text[128];double numbers[3];
        if(point>=0){numbers[0]=draft.points[point].position.x;numbers[1]=draft.points[point].position.y;numbers[2]=draft.points[point].position.z;}else{numbers[0]=draft.segments;numbers[1]=draft.width;numbers[2]=draft.height;}
        jkGuiElement e[14]={};e[0]={ELEMENT_TEXT,0,2,NULL,3,{20,18,600,40},1};
        for(int i=0;i<3;i++){snprintf(text,sizeof(text),"%.9g",numbers[i]);stdString_CharToWchar(values[i],text,128);e[i+1]={ELEMENT_TEXT,0,0,NULL,3,{30,100+i*60,180,28},1};e[i+4]={ELEMENT_TEXTBOX,10+i,0,NULL,127,{220,98+i*60,380,32},1};}
        e[7]={point<0?ELEMENT_TEXTBUTTON:ELEMENT_TEXT,20,0,NULL,3,{80,290,480,36},1};e[8]={ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,425,230,36},1};e[9]={ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,425,230,36},1};e[10]={ELEMENT_TEXT,0,0,NULL,3,{24,382,592,36},1};e[11]={ELEMENT_TEXTBUTTON,21,0,NULL,3,{40,336,560,36},point<0};e[12]={ELEMENT_TEXTBUTTON,22,0,NULL,3,{40,378,560,32},point<0};e[13].type=ELEMENT_END;
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=point>=0?u"Spline Point Attributes":u"Spline Shape and Geometry";
        const char16_t *labels[]={u"Sectors (1-256)",u"Width",u"Height",u"X",u"Y",u"Z"};for(int i=0;i<3;i++){e[i+1].wstr=labels[i+(point>=0?3:0)];e[i+4].wstr=values[i];}
        shapeLabel(draft.sides,text,sizeof(text));if(draft.groupId)snprintf(text,sizeof(text),"Group: %s",model->groups[draft.groupId-1].name);else if(draft.sides==32)strcpy(text,"Circle (32)");stdString_CharToWchar(shape,text,128);e[7].wstr=point>=0?u"Position is the center of the corridor cross-section.":shape;e[8].wstr=u"Apply";e[12].wstr=u"Convert to Sectors";e[10].bIsVisible=point>=0;e[10].wstr=point>=0?u"Move the point without changing its tangent handle.":u"Click the shape to choose a cross-section.\nTight curves may need narrower corridors or more sectors.";
        char textureText[160];snprintf(textureText,sizeof(textureText),"Texture: %s",draft.material>=0&&draft.material<paletteCount?paletteNames[draft.material]:"Choose...");stdString_CharToWchar(texture,textureText,160);e[11].wstr=texture;if(draft.groupId){e[11].type=ELEMENT_TEXT;e[11].wstr=u"Uses group textures (forward axis set in Array menu)";if(point<0)e[1].wstr=u"Group copies (1-256)";}
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[9]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&e[8]);if((getenv("GEOSITH_SPLINE_MENU_CAPTURE")||getenv("GEOSITH_SPLINE_CONVERT_CAPTURE"))&&getenv("GEOSITH_SPLINE_TEST"))menu.idkFunc=splineMenuTest;if(getenv("GEOSITH_ARRAY_TEST")&&getenv("GEOSITH_STRESS"))menu.idkFunc=arrayTestMenu;int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);if(choice<0)break;
        if(choice==22&&point<0){if(edModelsEqual(&original,model)&&splineConvertSelected())break;say("Spline conversion cancelled: level changed.");break;}
        int valid=1;for(int i=0;i<3;i++){char extra;stdString_WcharToChar(text,values[i],128);if(sscanf(text," %lf %c",&numbers[i],&extra)!=1||!isfinite(numbers[i]))valid=0;}
        if(point<0&&(numbers[0]<1||numbers[0]>ED_SPLINE_SECTORS||floor(numbers[0])!=numbers[0]||numbers[1]<ED_GRID||numbers[2]<ED_GRID||numbers[1]>1000||numbers[2]>1000))valid=0;
        if(!valid){jkGuiDialog_ErrorDialog(u"Spline",u"Enter valid numbers. Sector count must be 1-256; width and height must be 0.0625-1000.");continue;}
        if(point>=0)draft.points[point].position=edV(numbers[0],numbers[1],numbers[2]);else{draft.segments=(int)numbers[0];draft.width=numbers[1];draft.height=numbers[2];}
        if(choice==21&&point<0){int oldMaterial=currentMaterial,oldObjects=browserObjects;currentMaterial=draft.material;browserObjects=0;materialMenu(1);draft.material=currentMaterial;currentMaterial=oldMaterial;browserObjects=oldObjects;continue;}
        if(choice==20&&point<0){int choice=splineShapeMenu(draft.groupId?-draft.groupId:draft.sides);if(choice<0){draft.groupId=-choice;EdVec lo,hi;const EdSectorGroup &g=model->groups[draft.groupId-1];edGroupBounds(&g,&lo,&hi);EdVec size=edSub(hi,lo),forward=edGroupAxis(g.direction),up=g.direction/2==2?edV(0,1,0):edV(0,0,1);draft.width=fabs(edDot(size,edCross(forward,up)));draft.height=fabs(edDot(size,up));}else{draft.groupId=0;draft.sides=choice;}continue;}
        if(!edModelsEqual(&original,model)){say("Level changed while the spline menu was open. Reopen the menu.");break;}
        if(splinePublish(draft))break;jkGuiDialog_ErrorDialog(u"Spline",u"Cannot generate this corridor. Adjust its shape, dimensions or point positions.");
    }
    jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=modal;stdControl_SetActivation(1);stdControl_Flush();fireDown=grabDown=1;
}
static int splineControl3D(EdVec origin,EdVec direction,int fire,int grab,int enter){
    if(splineDragging&&!grabbing)splineDragging=0;
    int hitId=0,hitPoint=-1,hitHandle=0;double best=1000;
    for(int i=0;i<model->splineCount;i++)for(int p=0;p<model->splines[i].count;p++){EdVec offset=edSub(model->splines[i].points[p].position,origin);double distance=edDot(offset,direction);if(distance<=0||distance>=best)continue;double radius=fmin(.12,fmax(.035,distance*.012));EdVec miss=edSub(offset,edScale(direction,distance));if(edDot(miss,miss)<radius*radius){hitId=model->splines[i].id;hitPoint=p;best=distance;}}
    int selected=splineIndex(splineSelected);
    if(selected>=0&&splinePoint>=0&&splinePoint<model->splines[selected].count){
        const EdSpline &s=model->splines[selected];EdVec h=splineVisibleHandle(s,splinePoint);
        for(int sign=-1;sign<=1;sign+=2){EdVec position=edAdd(s.points[splinePoint].position,edScale(h,sign)),offset=edSub(position,origin);double distance=edDot(offset,direction),radius=fmin(.12,fmax(.035,distance*.012));EdVec miss=edSub(offset,edScale(direction,distance));
            if(distance>0&&distance<best&&edDot(miss,miss)<radius*radius){hitId=s.id;hitPoint=splinePoint;hitHandle=sign;best=distance;}
        }
    }
    if(!hitId)for(int i=0;i<model->splineCount;i++){
        const EdSpline &s=model->splines[i];EdVec a=edSplinePosition(&s,0);
        for(int j=1;j<=(s.closed?s.count:s.count-1)*32;j++){EdVec b=edSplinePosition(&s,j/32.0),e=edSub(b,a),o=edSub(a,origin);double de=edDot(direction,e),den=edDot(e,e)-de*de;
            double u=den>1e-12?fmax(0,fmin(1,(de*edDot(direction,o)-edDot(e,o))/den)):0;EdVec q=edAdd(o,edScale(e,u));double distance=edDot(q,direction),radius=fmin(.12,fmax(.035,distance*.012));EdVec miss=edSub(q,edScale(direction,distance));
            if(distance>0&&distance<best&&edDot(miss,miss)<radius*radius){hitId=s.id;hitPoint=-1;hitHandle=0;best=distance;}a=b;
        }
    }
    if(fire&&!fireDown&&hitId){splineSelected=hitId;splinePoint=hitPoint;picks.count=0;sectorPicks.clear();fireDown=fire;grabDown=grab;return 1;}
    if(enter&&!splineDragging&&splineIndex(splineSelected)>=0){splineAttributes();attributesDown=1;return 1;}
    if(grab&&!grabDown&&hitId){splineSelected=hitId;splinePoint=hitPoint;splineDraft=model->splines[splineIndex(hitId)];edCopyModel(snapshot,model);splineDragging=grabbing=1;splineHandle=hitHandle;splineDragStart=hitPoint<0?splineDraft.points[0].position:edAdd(splineDraft.points[hitPoint].position,edScale(splineVisibleHandle(splineDraft,hitPoint),hitHandle));splineDragDelta=edV(0,0,0);edDragTraceBegin(&splineDragTrace,origin,direction,splineDragStart,direction,1000);}
    if(splineDragging){if(grab){splineDragDelta=edAdd(splineDragDelta,edDragTraceStep(&splineDragTrace,origin,direction));EdVec moved=edSnapPoint(edAdd(splineDragStart,splineDragDelta));if(splinePoint<0){const EdSpline &start=snapshot->splines[splineIndex(splineSelected)];EdVec delta=edSub(moved,splineDragStart);for(int p=0;p<splineDraft.count;p++)splineDraft.points[p].position=edAdd(start.points[p].position,delta);}else if(splineHandle)splineDraft.points[splinePoint].handle=edScale(edSub(moved,splineDraft.points[splinePoint].position),splineHandle);else splineDraft.points[splinePoint].position=moved;}
        else {if(edModelsEqual(snapshot,model))splinePublish(splineDraft);splineDragging=grabbing=0;}fireDown=fire;grabDown=grab;return 1;}
    if(fire&&!fireDown&&!hitId){splineSelected=0;splinePoint=-1;}return 0;
}
