/* Door and switch placement are a single undoable, host-approved transaction. */
static std::unique_ptr<EdModel> doorBefore,doorDraft;
static int doorSector,doorSplit,doorActivation,doorKey,doorMaterial;
static EdVec doorAxis;
static char doorName[32];
static int doorActive(){return doorDraft!=nullptr;}
static int doorCancel(){int was=doorActive();doorDraft.reset();doorBefore.reset();if(was)say("Door and switch placement cancelled.");return was;}
static int doorChoices(const char16_t *title,std::initializer_list<const char16_t*> choices){
    std::vector<std::array<char16_t,128>> names(choices.size());int i=0;
    for(auto name:choices)stdString_SafeWStrCopy(names[i++].data(),name,128);
    return chooseList(title,reinterpret_cast<char16_t (*)[128]>(names.data()),(int)names.size(),0);
}
static int doorStart(int sector,const EdCell &draft){
    auto original=std::make_unique<EdModel>(*model);
    if(!sithTemplate_GetTemplate("_walkstruct")){jkGuiDialog_ErrorDialog(u"Door",u"The _walkstruct base template is unavailable.");return 0;}
    int split=doorChoices(u"Door panels",{u"Sliding door",u"Split door"});if(split<0)return 0;
    int direction=doorChoices(u"Opening direction (split: first panel)",{u"Up (+Z)",u"Down (-Z)",u"+X",u"-X",u"+Y",u"-Y"});if(direction<0)return 0;
    int activation=doorChoices(u"Door activation",{u"Automatic",u"Player Activate",u"Wall switch",u"Key pickup"});if(activation<0)return 0;
    int key=0;if(activation==3){key=doorChoices(u"Required key pickup",{u"Red key",u"Blue key",u"Yellow key"});if(key<0)return 0;}
    if(netEditPending()||!edModelsEqual(model,original.get())){jkGuiDialog_ErrorDialog(u"Door",u"The level changed while choosing door options. Reopen sector attributes.");return 0;}
    const EdVec axes[]={{0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
    int material=-1;if(activation==2){
        for(int i=0;i<paletteCount;i++)if(!__strcmpi(paletteNames[i],"00tcall.mat")){material=i;break;}
        if(material<0||!editorMaterial(material)){jkGuiDialog_ErrorDialog(u"Door",u"Could not load the game's 00tcall.mat switch material.");return 0;}
    }
    for(int n=1;;n++){
        snprintf(doorName,sizeof(doorName),"gs_door%d",n);int exists=0;
        for(int part=0;part<2;part++){char id[40];snprintf(id,sizeof(id),"%s%s",doorName,part?"_b":"");exists|=sithTemplate_GetTemplate(id)!=NULL||edTemplateIndex(model,id)>=0;for(int a=0;a<model->assetCount;a++)exists|=!strcmp(model->assets[a].name,id);}
        if(!exists)break;
    }
    doorSector=sector;doorSplit=split;doorAxis=axes[direction];doorActivation=activation;doorKey=key;doorMaterial=material;
    edCopyModel(candidate,model);candidate->c[sector]=draft;
    if(activation==2){
        // Validate the panels now, without retaining a dummy COG or wall.
        EdModel probe=*candidate;if(!edGenerateDoor(&probe,sector,doorName,split,doorAxis,1,key)){jkGuiDialog_ErrorDialog(u"Door",u"Cannot generate panels. Convert editable splines first; check geometry and template capacity.");return 0;}
        doorBefore=std::make_unique<EdModel>(*model);doorDraft=std::make_unique<EdModel>(*candidate);
        if(mapActive)mapClose(sithWorld_g_pCurrentWorld->pLocalPlayer);
        grabbing=groupGrab=cleaving=0;fireDown=grabDown=1;
        say("Place door switch: aim at a solid wall and Fire 1. Escape cancels the door.");return 1;
    }
    if(!edGenerateDoor(candidate,sector,doorName,split,doorAxis,activation,key)){jkGuiDialog_ErrorDialog(u"Door",u"Cannot generate panels. Convert editable splines first; check geometry and template capacity.");return 0;}
    remember();if(!commit())return 0;sectorPicks.clear();picks.count=0;
    say(activation==3?"Key-locked door generated. Place the matching key pickup with weapon 3. Ctrl+Z: undo.":"Door generated with movement frames, sounds and a placed COG. Ctrl+Z: undo.");return 1;
}
static void doorPreview(EdVec origin,EdVec direction){
    EdHit hit;EdFace outline;if(!doorActive()||!edRay(model,origin,direction,&hit)||hit.cell==doorSector||!edDoorSwitchOutline(&model->c[hit.cell].f[hit.face],hit.point,&outline))return;
    rdMaterial *material=editorMaterial(doorMaterial);if(!material||!rdMaterial_EnsureDataForced(material)||!material->num_texinfo||!material->texinfos[0]||!material->texinfos[0]->texture_ptr)return;
    tVBuffer *buffer=material->texinfos[0]->texture_ptr->texture_struct[0];if(!buffer)return;
    drawTextureOutline(&model->c[hit.cell].f[hit.face],hit.point,outline,material,buffer->format.width,buffer->format.height);
}
static int doorPlace(EdHit hit){
    if(!doorActive()||!edModelsEqual(model,doorBefore.get())||netEditPending())return 0;
    rdMaterial *mat=editorMaterial(doorMaterial);if(!mat||!rdMaterial_EnsureDataForced(mat)||!mat->num_texinfo||!mat->texinfos[0]||!mat->texinfos[0]->texture_ptr)return 1;
    tVBuffer *buffer=mat->texinfos[0]->texture_ptr->texture_struct[0];if(!buffer)return 1;
    edCopyModel(candidate,doorDraft.get());
    if(!edDoorSwitch(candidate,hit.cell,hit.face,hit.point,doorMaterial,(std::min)(buffer->format.width,256),(std::min)(buffer->format.height,256))
       ||!edGenerateDoor(candidate,doorSector,doorName,doorSplit,doorAxis,doorActivation,doorKey,hit.cell,hit.face)){
        say("Switch needs an unobstructed square on a solid wall. Aim farther from its edges.");return 1;
    }
    remember();if(commit()){doorDraft.reset();doorBefore.reset();sectorPicks.clear();picks.count=0;say("Door and wall switch created. Use the switch in game mode. Ctrl+Z: undo.");}return 1;
}
static int doorControl(){
    if(!doorActive())return 0;int fire=editButton(INPUT_FUNC_FIRE1),pressed=fire&&!fireDown;fireDown=fire;grabDown=editButton(INPUT_FUNC_FIRE2);
    if(!pressed)return 1;
    if(netEditPending()){say("Wait for host approval before placing the switch.");return 1;}
    if(!edModelsEqual(model,doorBefore.get())){doorCancel();say("Level changed during switch placement. Generate the door again.");return 1;}
    EdVec origin,direction;EdHit hit;ray(&origin,&direction);
    if(!edRay(model,origin,direction,&hit)||hit.cell==doorSector){say("Place the switch on a solid wall outside the door sector.");return 1;}
    doorPlace(hit);return 1;
}
