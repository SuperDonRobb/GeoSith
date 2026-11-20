static std::vector<int> generatorThings;
static uint64_t generatorThingsRevision;
static void thingGeneratorClear(){generatorThings.clear();generatorThingsRevision=geometryDocumentRevision;}
static int thingGeneratorHasSelection(){if(generatorThingsRevision!=geometryDocumentRevision)thingGeneratorClear();return !generatorThings.empty();}
static void thingGeneratorSelect(int id){
    thingGeneratorHasSelection();if(id<0||id>=model->objectCount){say("Shift+Fire 1 selects placed things for generators.");return;}
    auto it=std::find(generatorThings.begin(),generatorThings.end(),id);
    if(it!=generatorThings.end())generatorThings.erase(it);else if(generatorThings.size()<ED_SELECTIONS)generatorThings.push_back(id);
    char text[160];snprintf(text,sizeof(text),"%d things selected. Enter: Thing Generators. Shift+click toggles selection.",(int)generatorThings.size());say(text);
}
static void thingGeneratorTestMenu(jkGuiMenu *menu){menu->lastClicked=100;}
static void thingGeneratorMenu(){
    if(!thingGeneratorHasSelection())return;
    auto original=std::make_unique<EdModel>(*model);auto selected=generatorThings;
    SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;rdVector_Zero3(&player->physicsParams.vel);rdVector_Zero3(&player->physicsParams.acceleration);
    int previous=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    char16_t names[1][128]={u"Teleporter"};
    if(chooseList(u"Thing Generators",names,1,0)==0){
        int valid=selected.size()==2;
        for(int id:selected){SithThing *definition=sithTemplate_GetTemplate(original->objects[id].templateName);valid=valid&&definition&&definition->type==SITH_THING_GHOST;}
        if(!valid)jkGuiDialog_ErrorDialog(u"Teleporter",u"Select exactly two ghost things with weapon 3: Shift+Fire 1 on each ghost.");
        else {
            char16_t modes[2][128]={u"Two-way",u"One-way (1 to 2)"};int mode=chooseList(u"Teleporter travel",modes,2,0);
            if(mode>=0){
                if(netEditPending()||!edModelsEqual(original.get(),model))say("Level changed or an edit is pending. Select the ghosts again.");
                else {edCopyModel(candidate,model);
                    if(!edGenerateTeleporter(candidate,selected[0],selected[1],mode==0))jkGuiDialog_ErrorDialog(u"Teleporter",u"Ghosts must be inside the level, at least 0.5 units apart, and not already assigned to a generated teleporter.");
                    else {remember();if(commit()){thingGeneratorClear();say("Teleporter created. Approach a ghost to travel; move away from the exit before returning. Ctrl+Z: undo.");}}
                }
            }
        }
    }
    jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=previous;stdControl_SetActivation(1);stdControl_Flush();
}
static void thingGeneratorOverlay(){
    if(tool!=3||!thingGeneratorHasSelection())return;int purple=palettePixel(highlightCanvas->pVBuffer,200,64,255);
    for(size_t i=0;i<generatorThings.size();i++){
        int id=generatorThings[i];if(!thingOverlayVisible(id))continue;EdVec center=model->objects[id].position,end=objectFacingHandle(model,id);
        int centerColor=(objectGrabbing?objectGrabbed:objectHovered)==id?palettePixel(highlightCanvas->pVBuffer,255,224,64):purple;
        vertexCircle(center,1,centerColor);line(center,end,purple);vertexCircle(end,1,purple);
        rdVector3 screen;if(project(center,&screen)){char label[64];snprintf(label,sizeof(label),"Selected %d | thing %d",(int)i+1,model->spawnCount+id);editorDrawText(highlightCanvas->pVBuffer,jkGui_stdFonts[0],(int)screen.x+12,(int)screen.y-18,230,label,1);}
    }
}
