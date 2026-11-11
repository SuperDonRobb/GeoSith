/* The dialog edits private face copies. Apply commits one undo transaction;
 * Cancel, including nested flag dialogs, never mutates the running world. */
static const char *surfaceFlagNames[32]={
    "Floor","Used in COG","Impassable","AI floor / walk flag",
    "Double texture scale","Half texture scale","1/8 texture scale","No fall damage / 0x80",
    NULL,"Horizon sky","Ceiling sky","Scrolling","Icy","Very icy","Magsealed","Changed / 0x8000",
    "Metal","Deep water","Shallow water","Dirt","Very deep water",
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,"Texture flipped"
};
static const char *faceFlagNames[32]={"Double sided","Translucent","Clamp texture U","Clamp texture V","Nearest texture filter","Disable depth writes","3DO ledge (Jones)",NULL,"Fog (Jones)","Whip aim (Jones)"};
static const char *adjoinFlagNames[32]={"Visible","Allow movement","Allow sound","Player only","AI only","Set by sector"};
static unsigned int surfaceFlagsTouched;
static void surfaceFlagDraw(jkGuiElement *element,jkGuiMenu *menu,tVBuffer *buffer,BOOL redraw) {
    if(redraw)jkGuiRend_CopyVBuffer(menu,&element->rect);
    if(!stdDisplay_VBufferLock(buffer))return;
    rdCanvas canvas={0};canvas.pVBuffer=buffer;canvas.widthMinusOne=buffer->format.width-1;canvas.heightMinusOne=buffer->format.height-1;
    int x=element->rect.x,y=element->rect.y+2,color=palettePixel(buffer,220,220,220);
    rdPrimit2_DrawRectangle(&canvas,x,y,x+14,y+14,color,-1);
    if(element->boxChecked){rdPrimit2_DrawClippedLine(&canvas,x+3,y+3,x+11,y+11,color,-1);rdPrimit2_DrawClippedLine(&canvas,x+3,y+11,x+11,y+3,color,-1);}
    char label[128];stdString_WcharToChar(label,element->wstr,sizeof(label));stdFont_DrawAscii(buffer,menu->fonts[element->textType],x+20,element->rect.y,element->rect.width-20,label,1);
    stdDisplay_VBufferUnlock(buffer);
}
static int surfaceFlagClick(jkGuiElement *element,jkGuiMenu *menu,int32_t x,int32_t y,BOOL redraw) {
surfaceFlagsTouched|=1u<<(element->hoverId-100);
    return jkGuiRend_DrawClickableAndUpdatebool(element,menu,x,y,redraw);
}
static unsigned int *surfaceFlagValue(EdFace *face,int field) {
    return field==8?&face->attributes.surfaceFlags:field==9?&face->attributes.faceFlags:&face->attributes.adjoinFlags;
}
static void captureSurfaceFlags(jkGuiMenu *menu) {
    static int frames;if(++frames%3)return;
    int face=menu->paElements[0].wstr[0]=='F',adjoin=menu->paElements[0].wstr[0]=='A';
    int bit=face?0:adjoin?1:16;surfaceFlagClick(&menu->paElements[1+bit],menu,0,0,1);
#ifdef SDL2_RENDER
    const char *path=getenv(face?"GEOSITH_FACE_FLAGS_CAPTURE":adjoin?"GEOSITH_ADJOIN_FLAGS_CAPTURE":"GEOSITH_SURFACE_FLAGS_CAPTURE");if(path)std3D_ScreenshotWindow(path);
#endif
    menu->lastClicked=1;
}
static void surfaceFlagsDialog(EdFace *draft,int count,int field) {
    unsigned int common=~0u,combined=0;int eligible=0;
    for(int i=0;i<count;i++)if(field!=7||draft[i].neighbor>=0){unsigned int bits=*surfaceFlagValue(&draft[i],field);common&=bits;combined|=bits;eligible++;}
    if(!eligible){jkGuiDialog_ErrorDialog(u"Adjoin Flags",u"The selected surfaces have no adjoins.\nConnect sectors with geometry before editing adjoin flags.");return;}
    const char **names=field==8?surfaceFlagNames:field==9?faceFlagNames:adjoinFlagNames;
    jkGuiElement elements[37]={0};char16_t labels[32][96];
    elements[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,8,600,40},1};
    for(int bit=0;bit<32;bit++){
        unsigned int mask=1u<<bit;char text[96];snprintf(text,sizeof(text),"%s%s",((combined^common)&mask)?"~ ":"",names[bit]?names[bit]:"Unknown / unused");
        if(!names[bit])snprintf(text,sizeof(text),"%sUnknown 0x%08X",((combined^common)&mask)?"~ ":"",mask);
        stdString_CharToWchar(labels[bit],text,96);
        elements[bit+1]=jkGuiElement{ELEMENT_CHECKBOX,100+bit,0,NULL,(common&mask)!=0,{18+(bit/16)*310,56+(bit%16)*21,302,20},1};
        elements[bit+1].clickHandlerFunc=surfaceFlagClick;
        elements[bit+1].drawFuncOverride=surfaceFlagDraw;
    }
    elements[33]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,396,600,24},1};
    elements[34]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,430,230,36},1};
    elements[35]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,430,230,36},1};elements[36].type=ELEMENT_END;
    jkGuiMenu menu={elements,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};
    jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
    elements[0].wstr=field==8?u"Surface Flags":field==9?u"Face Flags":u"Adjoin Flags";
    for(int bit=0;bit<32;bit++)elements[bit+1].wstr=labels[bit];
    elements[33].wstr=u"~ means mixed. Untouched flags keep their individual values.";elements[34].wstr=u"OK";
    surfaceFlagsTouched=0;
    if(getenv("GEOSITH_SURFACE_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))menu.idkFunc=captureSurfaceFlags;
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[35]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&elements[34]);
    int choice=jkGuiRend_DisplayAndReturnClicked(&menu);
    if(choice==1){unsigned int value=0;for(int bit=0;bit<32;bit++)if(elements[bit+1].boxChecked)value|=1u<<bit;
        for(int i=0;i<count;i++)if(field!=7||draft[i].neighbor>=0){unsigned int *bits=surfaceFlagValue(&draft[i],field);*bits=(*bits&~surfaceFlagsTouched)|(value&surfaceFlagsTouched);}
    }
    freeMenuStrings(elements);
}
static double surfaceNumeric(const EdFace *face,int field) {
    const EdSurfaceAttributes *a=&face->attributes;
    switch(field){case 1:return a->uScale;case 2:return a->vScale;case 3:return a->geometryMode;case 4:return a->textureMode;case 5:return a->lightingMode;case 7:return a->adjoinFlags;case 8:return a->surfaceFlags;case 9:return a->faceFlags;case 10:return a->extraLight;case 11:return a->adjoinDistance;}return 0;
}
static void captureSurfaceAttributes(jkGuiMenu *menu) {
    static int frames,stage;if(++frames%3)return;
    if(stage==0){stdString_SafeWStrCopy((char16_t*)menu->paElements[14].wstr,u"2",128);stdString_SafeWStrCopy((char16_t*)menu->paElements[23].wstr,u"0.25",128);
#ifdef SDL2_RENDER
        std3D_ScreenshotWindow(getenv("GEOSITH_SURFACE_CAPTURE"));
#endif
    }
    menu->lastClicked=stage==0?208:stage==1?209:stage==2&&getenv("GEOSITH_ADJOIN_FLAGS_CAPTURE")?207:1;stage++;
}
static void captureStairsAttributes(jkGuiMenu *menu) {
    static int frames;if(++frames<3)return;
#ifdef SDL2_RENDER
    std3D_ScreenshotWindow(getenv("GEOSITH_STAIRS_CAPTURE"));
#endif
    menu->lastClicked=300;
}
static void surfaceAttributesDialog(void) {
    if(!allSelectedSurfaces(&picks)){say("Select one or more surfaces with weapon 1 first.");return;}
    int count=picks.count;EdSelection selected=picks;EdFace *draft=edCalloc(count,sizeof(*draft));if(!draft)return;
    char16_t stairSteps[16];double stairRise=1,stairRun=1;
    if(count==2)edStairsDimensions(model,selected.hits[0],selected.hits[1],&stairRise,&stairRun);
    char stepText[16];snprintf(stepText,sizeof(stepText),"%d",(int)fmin(ED_FACES-4,fmax(1,ceil(stairRise/ED_GRID))));stdString_CharToWchar(stairSteps,stepText,16);
    for(int i=0;i<count;i++){EdHit h=selected.hits[i];draft[i]=model->c[h.cell].f[h.face];draft[i].attributes=edSurfaceAttributes(&draft[i]);draft[i].hasAttributes=1;}
    SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;rdVector_Zero3(&player->physicsParams.vel);rdVector_Zero3(&player->physicsParams.acceleration);
    int previousModal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    for(;;){
        const char16_t *labels[]={u"Material",u"U scale",u"V scale",u"Geometry mode",u"Texture mode",u"Light mode",u"Adjoin number",u"Adjoin flags",u"Surface flags",u"Face flags",u"Extra light",u"Adjoin distance"};
        char16_t values[12][128],before[12][128],title[128];char text[160];jkGuiElement elements[32]={0};
        elements[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,10,600,40},1};
        snprintf(text,sizeof(text),"Surface Attributes - %d selected",count);stdString_CharToWchar(title,text,128);
        for(int field=0;field<12;field++){
            int mixed=0;for(int i=1;i<count;i++)if(field==0?draft[i].material!=draft[0].material:field==6?draft[i].neighbor!=draft[0].neighbor||draft[i].mirror!=draft[0].mirror:surfaceNumeric(&draft[i],field)!=surfaceNumeric(&draft[0],field))mixed=1;
            if(mixed)snprintf(text,sizeof(text),"<mixed>");else if(field==0)snprintf(text,sizeof(text),"%s",paletteNames[draft[0].material]);
            else if(field==6){int id=-1,index=0;EdHit h=selected.hits[0];for(int s=0;s<model->count;s++)for(int f=0;f<model->c[s].count;f++)if(model->c[s].f[f].neighbor>=0){if(s==h.cell&&f==h.face)id=index;index++;}snprintf(text,sizeof(text),"%d (connection)",id);}
            else if(field>=7&&field<=9)snprintf(text,sizeof(text),"0x%08X",(unsigned int)surfaceNumeric(&draft[0],field));
            else snprintf(text,sizeof(text),"%.9g",surfaceNumeric(&draft[0],field));
            stdString_CharToWchar(values[field],text,128);edCopy(before[field],values[field],sizeof(values[field]));
            elements[1+field]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{24,65+field*25,185,24},1};
            elements[13+field]=jkGuiElement{field==6?ELEMENT_TEXT:field>=3&&field<=9?ELEMENT_TEXTBUTTON:ELEMENT_TEXTBOX,200+field,0,NULL,127,{218,63+field*25,398,24},1};
        }
        elements[25]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{24,370,592,24},1};
        elements[26]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,430,230,36},1};
        elements[27]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,430,230,36},1};elements[28]=jkGuiElement{ELEMENT_TEXTBUTTON,300,0,NULL,3,{314,395,302,28},1};
        elements[29]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{24,397,176,24},1};
        elements[30]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,3,{218,395,84,26},1};elements[31].type=ELEMENT_END;
        jkGuiMenu menu={elements,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};
        jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);elements[0].wstr=title;
        for(int i=0;i<12;i++){elements[1+i].wstr=labels[i];elements[13+i].wstr=values[i];}
        elements[25].wstr=u"Mixed values stay unchanged unless edited. Click modes or flags.";elements[26].wstr=u"Apply";
        elements[28].wstr=u"Create Stairs";elements[29].wstr=u"Stair steps";elements[30].wstr=stairSteps;
        if(getenv("GEOSITH_SURFACE_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))menu.idkFunc=captureSurfaceAttributes;
        if(getenv("GEOSITH_STAIRS_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))menu.idkFunc=captureStairsAttributes;
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[27]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&elements[26]);
        int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(elements);if(choice<0)break;
        int valid=1;EdFace *next=edAlloc(count*sizeof(*next));if(!next)break;edCopy(next,draft,count*sizeof(*next));
        for(int field=0;field<12;field++)if((field<3||field>9)&&edCompare(values[field],before[field],sizeof(values[field]))){
            if(field==0){char name[128];stdString_WcharToChar(name,values[0],128);int id=-1;for(int i=0;i<paletteCount;i++)if(!__strcmpi(baseName(name),paletteNames[i]))id=i;
                if(id<0||!editorMaterial(id))valid=0;else for(int i=0;i<count;i++)next[i].material=id;
            }else{double number;if(!parseHeaderNumbers(values[field],&number,1)||((field==1||field==2)&&fabs(number)<.000001)||(field==11&&number<0)){valid=0;continue;}
                for(int i=0;i<count;i++){if(field==1)next[i].attributes.uScale=number;else if(field==2)next[i].attributes.vScale=number;else if(field==10)next[i].attributes.extraLight=number;else if(next[i].neighbor>=0)next[i].attributes.adjoinDistance=number;}
            }
        }
        if(!valid){edFree(next);jkGuiDialog_ErrorDialog(u"Surface Attributes",u"Enter a known material name and finite numbers.\nScales must be nonzero; adjoin distance cannot be negative.");continue;}
        edCopy(draft,next,count*sizeof(*next));edFree(next);
        if(choice==300){double number=0;
            if(count!=2||!edStairsDimensions(model,selected.hits[0],selected.hits[1],NULL,NULL)){
                jkGuiDialog_ErrorDialog(u"Create Stairs",u"Select two opposite vertical end walls in one six-face sector.\nSelect the high end first, then the low end.\nThe floor and ceiling must be horizontal.");continue;
            }
            if(!parseHeaderNumbers(stairSteps,&number,1)||number<1||number>ED_FACES-4||number!=floor(number)){
                jkGuiDialog_ErrorDialog(u"Create Stairs",u"Enter a positive whole step count.");continue;
            }
            edCopyModel(candidate,model);for(int i=0;i<count;i++){EdHit h=selected.hits[i];candidate->c[h.cell].f[h.face].material=draft[i].material;candidate->c[h.cell].f[h.face].attributes=draft[i].attributes;candidate->c[h.cell].f[h.face].hasAttributes=1;}
            if(!edCreateStairs(candidate,selected.hits[0],selected.hits[1],(int)number)){
                jkGuiDialog_ErrorDialog(u"Create Stairs",u"Could not build valid stairs within the sector and surface limits.\nTry fewer steps.");continue;
            }
            int stranded=objectSector(candidate,vec(player->position))<0;remember();if(commit()){
                picks.count=0;grabbing=groupGrab=cleaving=0;if(stranded){nextSpawn=0;teleportWalkplayer(player);}
                say("Stairs created. Invisible floor ramp added. Ctrl+Z: undo.");break;
            }
        }else if(choice>=207&&choice<=209)surfaceFlagsDialog(draft,count,choice-200);
        else if(choice>=203&&choice<=205){int field=choice-200,limit=field==3?5:field==4?2:4;char16_t names[5][128];
            const char *geo[]={"0 - Not drawn","1 - Vertices","2 - Wireframe","3 - Solid color","4 - Textured"};
            const char *tex[]={"0 - Affine","1 - Perspective"};const char *light[]={"0 - Fully lit","1 - Not lit","2 - Diffuse","3 - Gouraud"};
            for(int i=0;i<limit;i++)stdString_CharToWchar(names[i],field==3?geo[i]:field==4?tex[i]:light[i],128);
            int value=chooseList(labels[field],names,limit,(int)surfaceNumeric(&draft[0],field));
            if(value>=0)for(int i=0;i<count;i++){if(field==3)draft[i].attributes.geometryMode=value;else if(field==4)draft[i].attributes.textureMode=value;else draft[i].attributes.lightingMode=value;}
        }else if(choice==1){remember();edCopyModel(candidate,model);
            for(int i=0;i<count;i++){EdHit h=selected.hits[i];EdFace *face=&candidate->c[h.cell].f[h.face];if(face->customUV){EdSurfaceAttributes old=edSurfaceAttributes(face);for(int v=0;v<face->count;v++){face->v[v].u*=old.uScale/draft[i].attributes.uScale;face->v[v].v*=old.vScale/draft[i].attributes.vScale;}}if(face->material!=draft[i].material)applyFaceMaterial(face,draft[i].material);face->attributes=draft[i].attributes;face->hasAttributes=1;edMapFace(face);}
            if(commit()){say("Surface attributes updated. Ctrl+Z: undo.");break;}
        }
    }
    edFree(draft);jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=previousModal;stdControl_SetActivation(1);stdControl_Flush();
}
