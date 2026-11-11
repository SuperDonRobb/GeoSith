/* Template creation shares the document transaction/undo/network path. */
static int templateTestDelete;
static void templateTestMenu(jkGuiMenu *menu){
    const char16_t *title=menu->paElements[0].wstr;int action=-1;const char *suffix="template";
    if(title[0]=='C'){action=model->templateCount?-1:100;}
    else if(title[0]=='T'){action=templateTestDelete?6:menu->paElements[5].bIsVisible?5:1;}
    else if(title[0]=='A'){suffix="parameters";if(menu->paElements[1].wstr[0])action=100;else{stdString_CharToWchar((char16_t*)menu->paElements[1].wstr,"model3d",128);action=1;}}
    else if(title[0]=='3'){suffix="models";action=100;}
#ifdef SDL2_RENDER
    char path[700];snprintf(path,sizeof(path),"%s.%s.png",getenv("GEOSITH_BUILDER_CAPTURE"),suffix);std3D_ScreenshotWindow(path);
#endif
    menu->lastClicked=action;
}
static char customRegistered[EDITOR_TEMPLATES][64];
static int customRegisteredCount;
static unsigned int customRegisteredHashes[EDITOR_TEMPLATES],customDefinitionHashes[EDITOR_TEMPLATES];
static SithWorld *customRegistryWorld;
static EdBuffer<std::array<char,32>> customModelNames;
static EdBuffer<unsigned int> customModelHashes,customGeometryHashes;
static void resetCustomTemplates(void){customRegistryWorld=NULL;customRegisteredCount=0;edZero(customModelNames,0,sizeof(customModelNames));edZero(customModelHashes,0,sizeof(customModelHashes));edZero(customGeometryHashes,0,sizeof(customGeometryHashes));edZero(customRegisteredHashes,0,sizeof(customRegisteredHashes));edZero(customDefinitionHashes,0,sizeof(customDefinitionHashes));}
static int templateParameterOrder(const char *key){return !strcmp(key,"type")?0:!strcmp(key,"move")?1:!strcmp(key,"numframes")?2:3;}
static int customTemplateLine(const EdTemplate *t,char *line,size_t size){
    int n=snprintf(line,size,"%s %s",t->name,t->parent);
    for(int order=0;order<4;order++)for(int j=0;j<t->count;j++)if(templateParameterOrder(t->params[j].key)==order){int used=snprintf(line+n,size-n," %s=%s",t->params[j].key,t->params[j].value);if(used<0||used>=size-n)return 0;n+=used;}
    return n>0&&n<STDCONF_LINEBUFFER_LEN-2;
}
static unsigned int customAssetHash(const EdModel *source,int asset){
    unsigned int hash=edNetHash(&source->assets[asset],sizeof(EdAsset))^edPaintHash(source,asset)^decalAssetHash(source,asset);for(int i=0;i<source->meshCount;i++)if(source->meshes[i].asset==asset)hash=(hash^edCellHash(&source->meshes[i].geometry))*16777619u;return hash;
}
static unsigned int customTemplateHash(const EdModel *source,const char *name){
    unsigned int hash=0;int index=edTemplateIndex(source,name),depth=0;while(index>=0&&depth++<ED_CUSTOM_TEMPLATES){const EdTemplate *t=&source->templates[index];hash=(hash^edNetHash(t,offsetof(EdTemplate,params)+t->count*sizeof(EdTemplateParam)))*16777619u;for(int p=0;p<t->count;p++)if(!strcmp(t->params[p].key,"model3d"))for(int a=0;a<source->assetCount;a++){char file[64];snprintf(file,64,"%s.3do",source->assets[a].name);if(!__strcmpi(file,t->params[p].value))hash=(hash^customAssetHash(source,a))*16777619u;}
        index=edTemplateIndex(source,t->parent);}return hash;
}
static int syncCustomTemplates(const EdModel *source){
    SithWorld *world=sithWorld_g_pCurrentWorld;if(!world||!edTemplatesValidate(source))return 0;
    if(customRegistryWorld!=world){resetCustomTemplates();customRegistryWorld=world;}
    int changed=0;unsigned char done[ED_CUSTOM_TEMPLATES]={0};
    for(int pass=0;pass<source->templateCount;pass++)for(int i=0;i<source->templateCount;i++)if(!done[i]){
        const EdTemplate *t=&source->templates[i];int parent=edTemplateIndex(source,t->parent);if(parent>=0&&!done[parent])continue;
        int registry=0;while(registry<customRegisteredCount&&strcmp(customRegistered[registry],t->name))registry++;
        unsigned int hash=customTemplateHash(source,t->name);SithThing *definition=sithTemplate_GetTemplate(t->name);
        if(!definition||registry==customRegisteredCount||customRegisteredHashes[registry]!=hash){
            if(strcmp(t->parent,"none")&&!sithTemplate_GetTemplate(t->parent))return 0;
            for(int j=0;j<t->count;j++)if(!strcmp(t->params[j].key,"model3d")){
                const char *file=t->params[j].value;for(int a=0;a<source->assetCount;a++){char name[64];snprintf(name,sizeof(name),"%s.3do",source->assets[a].name);
                    if(!__strcmpi(file,name)&&!source->assets[a].saved)return 0;
                    if(!__strcmpi(file,name)){
                        unsigned int modelHash=customAssetHash(source,a);
                        if(strcmp(customModelNames[a].data(),name)||customModelHashes[a]!=modelHash||!sithModel_Find(file)){
                            unsigned int geometryHash=decalAssetHash(source,a)^edNetHash(&source->assets[a],sizeof(EdAsset))^(edPainted(source,a)?0x714bc21u:0);for(int mesh=0;mesh<source->meshCount;mesh++)if(source->meshes[mesh].asset==a)geometryHash=(geometryHash^edCellHash(&source->meshes[mesh].geometry))*16777619u;
                            if(edPainted(source,a)&&!strcmp(customModelNames[a].data(),name)&&customGeometryHashes[a]==geometryHash&&sithModel_Find(file)){
                                EdPaintBake bake;if(!terrainBakeLive(source,a,&bake))return 0;
                            }else{
                                if(!builderCacheFrom(source,a))return 0;rdThing_FreeEntry(&builderThings[a]);if(!sithModel_ReplaceEditor(file,&builderModels[a]))return 0;builderHashes[a]=0;
                            }
                            customGeometryHashes[a]=geometryHash;
                            snprintf(customModelNames[a].data(),32,"%s",name);customModelHashes[a]=modelHash;
                        }
                    }}
                if(!sithModel_Load(file,1))return 0;
            }
            char line[16384];if(!customTemplateLine(t,line,sizeof(line)))return 0;
            StdConffileEntry old=stdConffile_g_entry;stdConffile_ReadArgsFromStr(line);definition=sithTemplate_UpdateEditor(world);stdConffile_g_entry=old;if(!definition)return 0;
            if(registry==customRegisteredCount){if(customRegisteredCount==EDITOR_TEMPLATES)return 0;snprintf(customRegistered[customRegisteredCount++],64,"%s",t->name);}
            unsigned int definitionHash=edNetHash(t,offsetof(EdTemplate,params)+t->count*sizeof(EdTemplateParam));
            if(customDefinitionHashes[registry]!=definitionHash)changed=1;
            customDefinitionHashes[registry]=definitionHash;customRegisteredHashes[registry]=hash;clearObjectPreviews();
        }
        int known=0;for(int j=0;j<templateCount;j++)if(!strcmp(templateNames[j],t->name))known=1;
        if(!known&&templateCount<EDITOR_TEMPLATES){snprintf(templateNames[templateCount++],128,"%s",t->name);changed=1;}done[i]=1;
    }
    for(int i=0;i<source->templateCount;i++)if(!done[i])return 0;
    for(int j=0;j<templateCount;){int hidden=0;for(int i=0;i<customRegisteredCount;i++)if(!strcmp(templateNames[j],customRegistered[i])&&edTemplateIndex(source,templateNames[j])<0)hidden=1;
        if(hidden){memmove(templateNames+j,templateNames+j+1,(--templateCount-j)*sizeof(templateNames[0]));changed=1;}else j++;}
    if(changed){clearObjectPreviews();templateMetadata();if(currentTemplate>=templateCount)currentTemplate=0;}
    return 1;
}
static char (*templateBrowserModels)[128];
static void templateModelTile(jkGuiElement *element,jkGuiMenu *menu,tVBuffer *buffer,BOOL redraw){
    int id=(int)element->otherDataPtr;const char *name=templateBrowserModels[id];if(redraw)jkGuiRend_CopyVBuffer(menu,&element->rect);
    rdModel3 loaded={0},*mesh=sithModel_Find(name);int borrowed=1;
    if(!mesh)for(int a=0;a<model->assetCount;a++){char file[64];snprintf(file,sizeof(file),"%s.3do",model->assets[a].name);if(!__strcmpi(name,file)&&builderCache(a)){mesh=&builderModels[a];break;}}
    if(!mesh){char path[160];snprintf(path,sizeof(path),"3do\\%s",name);if(rdModel3_LoadEntry(path,&loaded)){mesh=&loaded;borrowed=0;}}
    int width=element->rect.width-8,height=element->rect.height-24;EditorPreviewCache *preview=mesh?renderObjectPreview(buffer,mesh,10000+id,width,height):NULL;
    if(!borrowed)rdModel3_FreeEntryGeometryOnly(&loaded);
    if(stdDisplay_VBufferLock(buffer)&&buffer->surface_lock_alloc){
        if(preview)for(int y=0;y<height;y++)for(int x=0;x<width;x++){uint32_t color=preview->pixels[y*width+x];if(color==0x10000)continue;unsigned char *pixel=(unsigned char*)buffer->surface_lock_alloc+(element->rect.y+2+y)*buffer->format.rowSize+(element->rect.x+4+x)*(buffer->format.format.bpp/8);if(buffer->format.format.bpp==8)*pixel=(unsigned char)color;else if(buffer->format.format.bpp==16){uint16_t c=(uint16_t)color;edCopy(pixel,&c,2);}}
        stdFont_DrawAscii(buffer,menu->fonts[0],element->rect.x+4,element->rect.y+element->rect.height-20,element->rect.width-8,(char*)name,1);stdDisplay_VBufferUnlock(buffer);
    }
}
/* Search is explicit so typing never loses focus while filtering/paging. */
static int templateChoose(const char16_t *title,char names[][128],int count){
    int page=0;char16_t search[128]=u"";char filter[128]="";int *ids=edAlloc((count?count:1)*sizeof(int));if(!ids)return -1;
    for(;;){int found=0;for(int i=0;i<count;i++){char lower[128];snprintf(lower,sizeof(lower),"%s",names[i]);for(char *p=lower;*p;p++)*p=(char)tolower((unsigned char)*p);if(!filter[0]||strstr(lower,filter))ids[found++]=i;}if(page*12>=found)page=0;
        jkGuiElement e[20]={0};char16_t labels[12][128];e[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,8,600,40},1};e[1]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,127,{20,54,420,30},1};e[2]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{450,50,170,40},1};
        for(int i=0;i<12;i++){int id=page*12+i;e[3+i]=jkGuiElement{ELEMENT_TEXTBUTTON,100+i,0,NULL,3,{20+(i%2)*300,96+(i/2)*45,294,40},id<found};if(id<found){stdString_CharToWchar(labels[i],names[ids[id]],128);if(templateBrowserModels){e[3+i].rect=rdRect{20+(i%4)*150,96+(i/4)*100,144,94};e[3+i].drawFuncOverride=templateModelTile;e[3+i].otherDataPtr=ids[id];}}}
        e[15]=jkGuiElement{ELEMENT_TEXTBUTTON,2,2,NULL,3,{20,410,180,40},1};e[16]=jkGuiElement{ELEMENT_TEXTBUTTON,3,2,NULL,3,{440,410,180,40},1};e[17]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{230,410,180,40},1};e[18].type=ELEMENT_END;
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=title;e[1].wstr=search;e[2].wstr=u"Search";for(int i=0;i<12;i++)e[3+i].wstr=labels[i];e[15].wstr=u"Previous";e[16].wstr=u"Next";
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[17]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&e[2]);if(getenv("GEOSITH_TEMPLATE_TEST")&&getenv("GEOSITH_BUILDER_CAPTURE"))menu.idkFunc=templateTestMenu;int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);
        if(choice>=100&&choice<112&&page*12+choice-100<found){int id=ids[page*12+choice-100];edFree(ids);return id;}
        if(choice==1){stdString_WcharToChar(filter,search,128);for(char *p=filter;*p;p++)*p=(char)tolower((unsigned char)*p);page=0;}else if(choice==2){if(page)page--;}else if(choice==3){if((page+1)*12<found)page++;}else{edFree(ids);return -1;}
    }
}
static void templateAddModel(char names[][128],int *count,const char *path){const char *name=baseName(path);size_t n=strlen(name);if(n<5||__strcmpi(name+n-4,".3do")||n>=128||*count>=4096)return;for(int i=0;i<*count;i++)if(!__strcmpi(names[i],name))return;snprintf(names[(*count)++],128,"%s",name);}
static int templateModelBrowser(char value[128]){
    char (*names)[128]=edCalloc(4096,128);if(!names)return 0;int count=0;
    for(int a=0;a<model->assetCount;a++)if(a!=builderAsset&&model->assets[a].saved){char file[64];snprintf(file,sizeof(file),"%s.3do",model->assets[a].name);templateAddModel(names,&count,file);}
    for(auto &f:customResources.files)if(f.first.ends_with(".3do"))templateAddModel(names,&count,f.first.c_str());
    int custom=count;
    for(int d=0;d<5;d++){jkResGobDirectory *dir=&jkRes_gCtx.aGobDirectories[d];for(int g=0;g<dir->numGobs;g++){Gob *gob=dir->gobs[g];if(!gob)continue;
#ifndef STDGOB_COMPACT_ENTRIES
        for(unsigned i=0;i<gob->numFiles;i++)if(!strncmp(gob->entries[i].fname,"3do\\",4))templateAddModel(names,&count,gob->entries[i].fname);
#endif
    }char path[256];snprintf(path,sizeof(path),"%s%s3do",dir->name,dir->name[0]?"/":"");stdFileSearch *search=stdFileUtil_NewFind(path,3,"3do");stdFileSearchResult entry;
    if(search){while(stdFileUtil_FindNext(search,&entry))if(!entry.is_subdirectory)templateAddModel(names,&count,entry.fpath);stdFileUtil_DisposeFind(search);}}
    qsort(names+custom,count-custom,sizeof(*names),materialNameCompare);clearObjectPreviews();templateBrowserModels=names;int choice=templateChoose(u"3DO Models (custom models listed first)",names,count);templateBrowserModels=NULL;clearObjectPreviews();if(choice>=0)snprintf(value,128,"%s",names[choice]);edFree(names);return choice>=0;
}
static const char *templateDefaultValue(const char *key){
    if(!strcmp(key,"type"))return "cog";if(!strcmp(key,"move"))return "none";
    if(!strcmp(key,"model3d"))return "dflt.3do";
    if(!strcmp(key,"frame"))return "(0/0/0:0/0/0)";
    if(!strcmp(key,"vel")||!strcmp(key,"angvel")||strstr(key,"offset")||!strcmp(key,"orient")||!strcmp(key,"flashrgb"))return "(0/0/0)";
    if(strstr(key,"flags"))return "0x0";
    return "0";
}
static void templateEditMenu(int editing){
    EdTemplate draft={0};snprintf(draft.parent,64,"%s",sithTemplate_GetTemplate("_decor")?"_decor":"none");
    for(int n=1;n<10000;n++){snprintf(draft.name,64,"custom%d",n);if(!sithTemplate_GetTemplate(draft.name))break;}
    if(!strcmp(draft.parent,"none")){draft.count=1;strcpy(draft.params[0].key,"type");strcpy(draft.params[0].value,"cog");}
    if(editing>=0&&editing<model->templateCount)draft=model->templates[editing];
    int page=0;
    for(;;){jkGuiElement e[38]={0};char16_t name[64],parent[64],keys[8][32],values[8][128];stdString_CharToWchar(name,draft.name,64);stdString_CharToWchar(parent,draft.parent,64);
        e[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,4,600,34},1};e[1]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,63,{140,42,470,28},1};e[2]=jkGuiElement{ELEMENT_TEXTBUTTON,4,0,NULL,3,{140,76,470,28},1};
        e[3]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,42,115,28},1};e[4]=jkGuiElement{ELEMENT_TEXTBUTTON,4,0,NULL,3,{20,76,115,28},1};
        for(int i=0;i<8;i++){int id=page*8+i,visible=id<draft.count;int y=116+i*31;e[5+i*3]=jkGuiElement{ELEMENT_TEXTBUTTON,100+i,0,NULL,3,{20,y,170,28},visible};e[6+i*3]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,127,{192,y,380,28},visible};e[7+i*3]=jkGuiElement{ELEMENT_TEXTBUTTON,200+i,0,NULL,3,{580,y,38,28},visible};if(visible){if(!strcmp(draft.params[id].key,"model3d")){e[6+i*3]=jkGuiElement{ELEMENT_TEXTBUTTON,100+i,0,NULL,3,{192,y,380,28},visible};}stdString_CharToWchar(keys[i],draft.params[id].key,32);stdString_CharToWchar(values[i],draft.params[id].value,128);}}
        e[29]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{20,372,170,34},1};e[30]=jkGuiElement{ELEMENT_TEXTBUTTON,2,2,NULL,3,{200,372,110,34},1};e[31]=jkGuiElement{ELEMENT_TEXTBUTTON,3,2,NULL,3,{320,372,110,34},1};e[32]=jkGuiElement{ELEMENT_TEXTBUTTON,5,2,NULL,3,{440,372,180,34},1};e[33]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{230,425,180,36},1};e[34]=jkGuiElement{ELEMENT_TEXTBUTTON,6,2,NULL,3,{440,425,180,36},editing>=0};e[35].type=ELEMENT_END;
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"Template Creator";e[1].wstr=name;e[2].wstr=parent;e[3].wstr=u"Name";e[4].wstr=u"Base template";
        for(int i=0;i<8;i++){e[5+i*3].wstr=keys[i];e[6+i*3].wstr=values[i];e[7+i*3].wstr=u"-";}
        e[34].wstr=u"Delete template";e[29].wstr=u"+ Parameter";e[30].wstr=u"Previous";e[31].wstr=u"Next";e[32].wstr=u"Save template";jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[33]);
        if(getenv("GEOSITH_TEMPLATE_TEST")&&getenv("GEOSITH_BUILDER_CAPTURE"))menu.idkFunc=templateTestMenu;int action=jkGuiRend_DisplayAndReturnClicked(&menu);stdString_WcharToChar(draft.name,name,64);stdString_WcharToChar(draft.parent,parent,64);for(int i=0;i<8&&page*8+i<draft.count;i++)stdString_WcharToChar(draft.params[page*8+i].value,values[i],128);freeMenuStrings(e);
        if(action<0)break;
        if(action==6&&editing>=0){
            const char *deleted=model->templates[editing].name;int used=0;
            for(int i=0;i<model->objectCount;i++)if(!strcmp(model->objects[i].templateName,deleted))used=1;
            for(int i=0;i<model->templateCount;i++)if(i!=editing&&!strcmp(model->templates[i].parent,deleted))used=1;
            if(used){jkGuiDialog_ErrorDialog(u"Template in use",u"Remove placed things and change child templates that use this base before deleting it.");continue;}
            if(netEditPending())continue;remember();edCopyModel(candidate,model);{--candidate->templateCount;for(int shift=editing;shift<candidate->templateCount;shift++)candidate->templates[shift]=std::move(candidate->templates[shift+1]);}
            if(commit()){say("Custom template deleted. Ctrl+Z: undo.");break;}continue;
        }
        if(action==1&&draft.count<ED_TEMPLATE_PARAMS){char names[100][128];int count=0;const char *key;while(count<100&&(key=sithThing_TemplateParameter(count))){snprintf(names[count++],128,"%s",key);}int id=templateChoose(u"Add Template Parameter",names,count);if(id>=0){EdTemplateParam *p=&draft.params[draft.count++];snprintf(p->key,32,"%s",names[id]);snprintf(p->value,128,"%s",templateDefaultValue(p->key));page=(draft.count-1)/8;if(!strcmp(p->key,"model3d"))templateModelBrowser(p->value);}}
        else if(action==2){if(page)page--;}else if(action==3){if((page+1)*8<draft.count)page++;}
        else if(action==4){int id=templateChoose(u"Base Template",templateNames,templateCount);if(id>=0)snprintf(draft.parent,64,"%s",templateNames[id]);}
        else if(action>=100&&action<108){int id=page*8+action-100;if(id<draft.count&&!strcmp(draft.params[id].key,"model3d"))templateModelBrowser(draft.params[id].value);}
        else if(action>=200&&action<208){int id=page*8+action-200;if(id<draft.count){memmove(draft.params+id,draft.params+id+1,(--draft.count-id)*sizeof(EdTemplateParam));if(page&&page*8>=draft.count)page--;}}
        else if(action==5){
            for(char *p=draft.name;*p;p++)*p=(char)tolower((unsigned char)*p);for(char *p=draft.parent;*p;p++)*p=(char)tolower((unsigned char)*p);
            if(netEditPending()){jkGuiDialog_ErrorDialog(u"Template",u"The host is still checking an edit. Please retry shortly.");continue;}
            int existing=edTemplateIndex(model,draft.name);if((existing<0&&model->templateCount>=ED_CUSTOM_TEMPLATES)||(existing<0&&sithTemplate_GetTemplate(draft.name))){jkGuiDialog_ErrorDialog(u"Template",u"Use a new name or an existing custom template name. Game templates cannot be overwritten.");continue;}
            edCopyModel(candidate,model);candidate->templates[existing>=0?existing:candidate->templateCount++]=draft;char line[16384];
            if(!edTemplatesValidate(candidate)||!customTemplateLine(&draft,line,sizeof(line))||(strcmp(draft.parent,"none")&&!sithTemplate_GetTemplate(draft.parent))){jkGuiDialog_ErrorDialog(u"Template",u"Use an existing base, a simple unique name, and nonempty parameter values without spaces. Vectors use (x/y/z).");continue;}
            remember();if(!commit()){jkGuiDialog_ErrorDialog(u"Template",u"Could not create template. Check its type, base and resource names.");continue;}
            for(int i=0;i<templateCount;i++)if(!strcmp(templateNames[i],draft.name))currentTemplate=i;say("Template saved. Weapon 3 can now place it. Save Level packages templates and custom 3DOs.");break;
        }
    }
}
static void templateCreatorMenu(void){
    BuilderMenuState state=builderMenuBegin();
    for(;;){char names[ED_CUSTOM_TEMPLATES+1][128];snprintf(names[0],128,"+ New template");for(int i=0;i<model->templateCount;i++)snprintf(names[i+1],128,"%s",model->templates[i].name);
        int choice=templateChoose(u"Created Templates - select to edit",names,model->templateCount+1);if(choice<0)break;templateEditMenu(choice-1);
    }builderMenuEnd(state);
}

// Terrain inherits walkstruct collision flags, but uses earth footsteps.
static void terrainTemplateFlags(EdModel *draft){
    for(int a=0;a<draft->assetCount;a++)if(draft->assets[a].terrain){int t=edTemplateIndex(draft,draft->assets[a].name);if(t<0)continue;EdTemplate &entry=draft->templates[t];int p=0;for(;p<entry.count;p++)if(!__strcmpi(entry.params[p].key,"thingflags"))break;
        SithThing *base=sithTemplate_GetTemplate(entry.parent);unsigned flags=base?base->flags:0x400048;
        if(p<entry.count)flags=(unsigned)strtoul(entry.params[p].value,NULL,16);else{if(entry.count>=ED_TEMPLATE_PARAMS)continue;entry.count++;strcpy(entry.params[p].key,"thingflags");}
        flags=(flags&~SITH_TF_METAL)|SITH_TF_EARTH;snprintf(entry.params[p].value,128,"0x%x",flags);
    }
}
static int builderWalkstruct(EdModel *draft,int asset,const char *name){
    int existing=edTemplateIndex(draft,name);
    if((existing<0&&sithTemplate_GetTemplate(name))||!sithTemplate_GetTemplate("_walkstruct")||draft->objectCount>=ED_OBJECTS||(existing<0&&draft->templateCount>=ED_CUSTOM_TEMPLATES)){say("Cannot create walkstruct: name conflict, missing base, or object/template limit.");return 0;}
    EdVec position=draft->assets[asset].origin;
    if(objectSector(draft,position)<0){position=vec(sithWorld_g_pCurrentWorld->pLocalPlayer->position);if(objectSector(draft,position)<0){say("Stand inside a sector before saving a walkstruct.");return 0;}draft->assets[asset].origin=position;}
    EdTemplate *t=&draft->templates[existing>=0?existing:draft->templateCount++];edZero(t,0,sizeof(*t));snprintf(t->name,64,"%s",name);strcpy(t->parent,"_walkstruct");t->count=1;strcpy(t->params[0].key,"model3d");snprintf(t->params[0].value,128,"%s.3do",name);
    EdObject *thing=&draft->objects[draft->objectCount++];edZero(thing,0,sizeof(*thing));snprintf(thing->templateName,64,"%s",name);thing->position=position;terrainTemplateFlags(draft);return 1;
}
static void stressTemplates(void){
    templateCreatorMenu();if(model->templateCount!=1)exit(2);const char *name=model->templates[0].name;SithThing *t=sithTemplate_GetTemplate(name);
    if(!t||t->renderData.type!=RD_THING_MODEL3||t->renderData.model3->aGeos[0].numMeshes!=4){puts("GEOSITH_TEMPLATE FAIL: engine model registration");exit(2);}
    if(!historyStep(0)||model->templateCount||!historyStep(1)||model->templateCount!=1)exit(2);
    BuilderMenuState editingState=builderMenuBegin();templateEditMenu(0);builderMenuEnd(editingState);
    remember();edCopyModel(candidate,model);EdObject *object=&candidate->objects[candidate->objectCount++];edZero(object,0,sizeof(*object));snprintf(object->templateName,64,"%s",name);object->position=edV(0,.2,.5);if(!commit()||objectThings[model->objectCount-1]<0)exit(2);
    remember();edCopyModel(candidate,model);EdTemplate *changed=&candidate->templates[0];int parameter=changed->count++;strcpy(changed->params[parameter].key,"size");strcpy(changed->params[parameter].value,"0.123");
    if(!commit()||fabs(sithTemplate_GetTemplate(name)->collideSize-.123)>.001||objectThings[model->objectCount-1]<0){puts("GEOSITH_TEMPLATE FAIL: overwrite");exit(2);}
    if(!historyStep(0)||!historyStep(1)||fabs(sithTemplate_GetTemplate(name)->collideSize-.123)>.001||!historyStep(0))exit(2);
    if(!writeEditorGob(model,"Episode/sithed_builder_test.gob","Builder test",0))exit(2);
    int count=model->assetCount,meshes=model->meshCount;EdVec origin,direction;ray(&origin,&direction);if(!builderBegin(origin,direction))exit(2);edCopyModel(candidate,model);if(!edMeshPrimitive(candidate,builderAsset,0,edV(0,0,.5),0)||!commit())exit(2);
    builderTestDiscard=1;builderFinish();builderTestDiscard=0;if(model->assetCount!=count||model->meshCount!=meshes||builderAsset!=-1)exit(2);
    if(!builderBegin(origin,direction))exit(2);edCopyModel(candidate,model);if(!edMeshPrimitiveTransformed(candidate,builderAsset,0,edV(0,0,.5),0,1,45)||!commit())exit(2);
    EdVec vertex=model->meshes[model->meshCount-1].geometry.f[0].v[0].p;builderTestWalkstruct=1;builderFinish();builderTestWalkstruct=0;
    int objectId=model->objectCount-1;SithThing *walk=sithTemplate_GetTemplate(model->objects[objectId].templateName);
    if(builderAsset!=-1||model->assetCount!=count+1||!walk||!walk->renderData.model3||objectThings[objectId]<0||edDot(edSub(vertex,model->meshes[model->meshCount-1].geometry.f[0].v[0].p),edSub(vertex,model->meshes[model->meshCount-1].geometry.f[0].v[0].p))>ED_EPS*ED_EPS){puts("GEOSITH_TEMPLATE FAIL: walkstruct save");exit(2);}
    /* Reopen a model used by a placed custom template, save its replacement,
       and verify the engine rebinds it without changing its library name. */
    int originalAssets=model->assetCount,originalMeshes=model->meshCount;builderEditAsset(0);
    if(builderAsset<0||strcmp(builderEditingName,"model1"))exit(2);
    edCopyModel(candidate,model);int draftMesh=-1;for(int i=0;i<candidate->meshCount;i++)if(candidate->meshes[i].asset==builderAsset){draftMesh=i;break;}
    if(draftMesh<0)exit(2);for(int f=0;f<candidate->meshes[draftMesh].geometry.count;f++)for(int v=0;v<candidate->meshes[draftMesh].geometry.f[f].count;v++)candidate->meshes[draftMesh].geometry.f[f].v[v].p.x+=ED_GRID;
    if(!commit())exit(2);rdModel3 *previous=sithTemplate_GetTemplate("custom1")->renderData.model3;
    builderFinish();if(builderAsset!=-1||model->assetCount!=originalAssets||model->meshCount!=originalMeshes||previous==sithTemplate_GetTemplate("custom1")->renderData.model3){puts("GEOSITH_TEMPLATE FAIL: saved model replacement");exit(2);}
    builderEditAsset(0);builderTestDiscard=1;builderFinish();builderTestDiscard=0;
    if(model->assetCount!=originalAssets||model->meshCount!=originalMeshes)exit(2);
    remember();edCopyModel(candidate,model);int temporary=candidate->templateCount++;candidate->templates[temporary]=candidate->templates[0];strcpy(candidate->templates[temporary].name,"delete_test");if(!commit())exit(2);
    templateTestDelete=1;editingState=builderMenuBegin();templateEditMenu(temporary);builderMenuEnd(editingState);templateTestDelete=0;
    if(model->templateCount!=temporary||!historyStep(0)||edTemplateIndex(model,"delete_test")<0||!historyStep(1)||edTemplateIndex(model,"delete_test")>=0)exit(2);
    remember();edCopyModel(candidate,model);int removable=candidate->assetCount++;candidate->assets[removable]=EdAsset{0};strcpy(candidate->assets[removable].name,"delete_model");candidate->assets[removable].saved=1;
    if(!edMeshPrimitive(candidate,removable,0,edV(0,0,.5),0)||!commit())exit(2);
    remember();edCopyModel(candidate,model);builderRemoveAsset(candidate,removable);if(!commit()||model->assetCount!=removable||!historyStep(0)||model->assetCount!=removable+1||!historyStep(1)||model->assetCount!=removable)exit(2);
    puts("GEOSITH_LIBRARY_DELETE PASS: template delete menu, model deletion, undo/redo");
    puts("GEOSITH_LIBRARY PASS: reopen, replace referenced model, discard editable copy");
    puts("GEOSITH_TEMPLATE PASS: searchable parameter/model menus, custom template registration, object placement, undo/redo, model discard and GOB save");fflush(stdout);
}
