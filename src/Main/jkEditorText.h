/* The GUI font stores palette indices, not RGB colors. In-game overlays use
   the level palette, so copy/remap the atlas without changing the shared font. */
static tVBuffer *editorTextAtlas,*editorTextSource;
static rdColor24 editorTextSourcePalette[256],editorTextLevelPalette[256];
static void freeEditorText(void){
    if(editorTextAtlas)stdDisplay_VBufferFree(editorTextAtlas);
    editorTextAtlas=editorTextSource=NULL;
}
// Added: accept immutable text and cast only at the legacy drawing API
static uint32_t editorDrawText(tVBuffer *buffer,stdFont *font,unsigned int x,int y,int width,const char *text,int alpha){
    if(!buffer||!font||!font->pBitmap||!font->pBitmap->mipSurfaces)return 0;
    tVBuffer *source=font->pBitmap->mipSurfaces[0];
    if(!source||source->format.format.bpp!=8)return stdFont_DrawAscii(buffer,font,x,y,width,(char*)text,alpha);
    /* Interpret glyph shading using 01narsh.cmp, then give editor text a
       readable warm-white tint rather than the menu's dark inactive tint. */
    int cmp=addColormap("01narsh.cmp");rdColor24 *colors=colormapColors(cmp);
    if(editorColormaps[cmp].loaded!=1&&jkGui_stdBitmaps[JKGUI_BM_BK_ESC]&&jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette)colors=(rdColor24*)jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette;
    if(!editorTextAtlas||editorTextSource!=source||edCompare(editorTextSourcePalette,colors,768)||edCompare(editorTextLevelPalette,stdDisplay_masterPalette,768)){
        if(editorTextSource!=source)freeEditorText();
        if(!editorTextAtlas){tRasterInfo format=source->format;editorTextAtlas=stdDisplay_VBufferNew(&format,0,0,stdDisplay_masterPalette);}
        if(!editorTextAtlas)return 0;
        int sourceLocked=stdDisplay_VBufferLock(source),atlasLocked=stdDisplay_VBufferLock(editorTextAtlas);
        if(!sourceLocked||!atlasLocked||!source->surface_lock_alloc||!editorTextAtlas->surface_lock_alloc){
            if(sourceLocked)stdDisplay_VBufferUnlock(source);if(atlasLocked)stdDisplay_VBufferUnlock(editorTextAtlas);freeEditorText();return 0;
        }
        int peak=1,key=font->pBitmap->colorkey&255;
        for(int row=0;row<source->format.height;row++){
            unsigned char *pixels=(unsigned char*)source->surface_lock_alloc+row*source->format.rowSize;
            for(int col=0;col<source->format.width;col++){int i=pixels[col];if(i!=key){int light=3*colors[i].r+6*colors[i].g+colors[i].b;if(light>peak)peak=light;}}
        }
        unsigned char lut[256];for(int i=0;i<256;i++){
            int light=3*colors[i].r+6*colors[i].g+colors[i].b;if(light>peak)light=peak;
            lut[i]=(unsigned char)(1+stdColor_FindClosest(stdDisplay_masterPalette+1,255,255*light/peak,240*light/peak,192*light/peak));
        }
        /* Reserve zero for transparency, even when the level's closest match
           to a visible glyph pixel happens to be palette entry zero. */
        lut[font->pBitmap->colorkey&255]=0;
        for(int row=0;row<source->format.height;row++){
            unsigned char *from=(unsigned char*)source->surface_lock_alloc+row*source->format.rowSize;
            unsigned char *to=(unsigned char*)editorTextAtlas->surface_lock_alloc+row*editorTextAtlas->format.rowSize;
            for(int col=0;col<source->format.width;col++)to[col]=lut[from[col]];
        }
        stdDisplay_VBufferUnlock(editorTextAtlas);stdDisplay_VBufferUnlock(source);stdDisplay_VBufferSetColorKey(editorTextAtlas,0);
        editorTextSource=source;edCopy(editorTextSourcePalette,colors,768);edCopy(editorTextLevelPalette,stdDisplay_masterPalette,768);
    }
    stdBitmap bitmap=*font->pBitmap;bitmap.mipSurfaces=&editorTextAtlas;bitmap.numMips=1;bitmap.colorkey=0;bitmap.palette=stdDisplay_masterPalette;
    stdFont mapped=*font;mapped.pBitmap=&bitmap;
    return stdFont_DrawAscii(buffer,&mapped,x,y,width,(char*)text,alpha);
}
