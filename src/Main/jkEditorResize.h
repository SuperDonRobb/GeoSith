#include "edResizeResource.h"
#include "World/sithSoundClass.h"
extern "C" {
#include "Engine/sithPhysics.h"
}
static std::map<std::string,std::string> resizeParameters(const char *name){
    std::map<std::string,std::string> result;std::string current=name;std::vector<std::string> seen;
    while(!current.empty()&&current!="none"){
        if(std::find(seen.begin(),seen.end(),current)!=seen.end())throw std::runtime_error("Template inheritance cycle");seen.push_back(current);
        int custom=edTemplateIndex(model,current.c_str());std::string parent;
        if(custom>=0){const auto &t=model->templates[custom];parent=t.parent;for(int i=0;i<t.count;i++)result.emplace(t.params[i].key,t.params[i].value);}
        else {const char *block=templateBlock(resources);bool found=false;
            std::istringstream lines(block?block:"");std::string line;while(std::getline(lines,line)){auto t=EdResize::tokens(line);if(t.empty())continue;if(EdResize::lower(t[0])=="end")break;if(t.size()<2||EdResize::lower(t[0])!=EdResize::lower(current))continue;
                found=true;parent=EdResize::lower(t[1]);for(size_t i=2;i<t.size();i++){auto at=t[i].find('=');if(at!=std::string::npos)result.emplace(EdResize::lower(t[i].substr(0,at)),t[i].substr(at+1));}break;
            }if(!found)throw std::runtime_error("Cannot find the original template definition");
        }current=parent;
    }return result;
}
static std::string resizeRead(const std::string &path){
    stdFile_t file=jkRes_FileOpen(path.c_str(),"rb");if(!file)throw std::runtime_error("Cannot read "+path);
    int size=jkRes_FileSize(file);std::string text;if(size>0&&size<=EdResources::MaxFile)text.resize(size);
    bool ok=!text.empty()&&jkRes_FileRead(file,text.data(),size)==size;jkRes_FileClose(file);if(!ok)throw std::runtime_error("Cannot read "+path);return text;
}
static void resizeParam(EdTemplate &t,const char *key,const std::string &value){
    if(t.count>=ED_TEMPLATE_PARAMS||value.size()>=sizeof(t.params[0].value))throw std::runtime_error("Too many resize parameters");
    snprintf(t.params[t.count].key,sizeof(t.params[0].key),"%s",key);snprintf(t.params[t.count++].value,sizeof(t.params[0].value),"%s",value.c_str());
}
static int resizeThing(int id,double factor,const EdThingAttributes &attributes,EdFrame pose,std::string &error){
    error.clear();EdResources previous=customResources;bool installed=false;
    try {
        if(id<0||id>=model->objectCount||!std::isfinite(factor)||factor<=0)throw std::runtime_error("Choose a placed thing and a positive scale");
        if(netEditPending()||terrainSessionOpen())throw std::runtime_error("Finish the pending edit before resizing");
        if(model->templateCount>=ED_CUSTOM_TEMPLATES)throw std::runtime_error("Custom template capacity reached");
        if(objectSector(model,pose.position)<0)throw std::runtime_error("The thing center must be inside a sector");
        auto params=resizeParameters(thingTemplate(model,id));SithThing *base=sithTemplate_GetTemplate(thingTemplate(model,id));if(!base)throw std::runtime_error("Missing source template");
        EdResources next=customResources;EdTemplate t={};static unsigned serial=0;std::string prefix;
        for(;;){char name[32];snprintf(name,sizeof(name),"rs%08x_%u",stdPlatform_GetTimeMsec(),++serial);prefix=name;bool used=sithTemplate_GetTemplate(name)!=NULL;for(auto &f:next.files)if(f.first.find(prefix)!=std::string::npos)used=true;if(!used)break;}
        std::string templateName;unsigned duplicate=0;
        do {templateName=EdResize::templateName(thingTemplate(model,id),factor,duplicate);duplicate=duplicate?duplicate+1:2;}while(sithTemplate_GetTemplate(templateName.c_str())||edTemplateIndex(model,templateName.c_str())>=0);
        snprintf(t.name,sizeof(t.name),"%s",templateName.c_str());snprintf(t.parent,sizeof(t.parent),"%s",thingTemplate(model,id));
        auto add=[&](const std::string &path,const std::string &text){next.add(path,EdResources::Bytes(text.begin(),text.end()));};
        std::map<std::string,std::string> keys;
        auto copyKey=[&](const std::string &source){auto key=EdResize::lower(source);auto found=keys.find(key);if(found!=keys.end())return found->second;
            std::string name=prefix+"k"+std::to_string(keys.size())+".key";auto text=EdResize::transform(resizeRead("3do/key/"+key),".key",factor);add("3do/key/"+name,text);keys[key]=name;return name;};
        struct Resource {const char *param,*root,*ext;};
        const Resource kinds[]={{"model3d","3do/",".3do"},{"sprite","misc/spr/",".spr"},{"particle","misc/par/",".par"},{"puppet","misc/pup/",".pup"}};
        std::map<std::string,std::string> sounds;
        auto copySound=[&](const std::string &source){auto key=EdResize::lower(source);auto found=sounds.find(key);if(found!=sounds.end())return found->second;
            // Stock sound classes contain optional/missing voice lines; the
            // normal loader skips these. Preserve them rather than failing resize.
            std::string path="sound/"+key;stdFile_t probe=jkRes_FileOpen(path.c_str(),"rb");if(!probe){path="voice/"+key;probe=jkRes_FileOpen(path.c_str(),"rb");}if(!probe)return source;jkRes_FileClose(probe);
            std::string name=prefix+"s"+std::to_string(sounds.size())+".wav";
            add("sound/"+name,EdResize::wave(resizeRead(path),factor));sounds[key]=name;return name;};
        unsigned newModels=0,newPuppets=0,newSprites=0,newParticles=0,newSoundClasses=0;
        auto copyResources=[&](const std::map<std::string,std::string> &params,EdTemplate &target,const std::string &resourcePrefix){
        for(auto kind:kinds){auto it=params.find(kind.param);if(it==params.end()||it->second=="none")continue;std::string source;
            if(!strcmp(kind.param,"model3d"))for(int a=0;a<model->assetCount;a++)if(EdResize::lower(it->second)==EdResize::lower(std::string(model->assets[a].name)+".3do")){
                EdResources::Bytes bytes;if(!edEncode3do(model,a,resources,bytes))throw std::runtime_error("Could not serialize custom model");source.assign(bytes.begin(),bytes.end());
                if(edPainted(model,a)){EdPaintBake bake;if(!edPaintBakeAsset(model,a,&bake))throw std::runtime_error("Could not bake custom model texture");char mat[32];edPaintFilename(model,a,mat);next.add(std::string("3do/mat/")+mat,std::move(bake.mat));}
                for(int d=0;d<model->decalCount;d++)next.add(std::string("3do/mat/")+model->decals[d].name,*model->decals[d].data);
                break;
            }
            if(source.empty())source=resizeRead(std::string(kind.root)+it->second);
            std::string name=resourcePrefix+kind.ext;add(std::string(kind.root)+name,EdResize::transform(source,kind.ext,factor,copyKey));resizeParam(target,kind.param,name);if(!strcmp(kind.param,"model3d"))newModels++;if(!strcmp(kind.param,"puppet"))newPuppets++;if(!strcmp(kind.param,"sprite"))newSprites++;if(!strcmp(kind.param,"particle"))newParticles++;
        }
            auto snd=params.find("soundclass");if(snd!=params.end()&&snd->second!="none"){
                std::string name=resourcePrefix+".snd";add("misc/snd/"+name,EdResize::soundClass(resizeRead("misc/snd/"+snd->second),copySound,factor));resizeParam(target,"soundclass",name);newSoundClasses++;
            }
        };
        copyResources(params,t,prefix);
        // Melee/hitscan reach lives on a separate weapon template. Never edit
        // the shared stock weapon (or scale damage, timing or projectile speed).
        std::vector<EdTemplate> weapons;
        std::map<std::string,std::string> effects;std::vector<std::string> effectStack;unsigned effectSerial=0;
        std::function<std::string(const std::string&)> copyEffect;
        copyEffect=[&](const std::string &source)->std::string{
            auto key=EdResize::lower(source);if(key=="none")return source;
            if(std::find(effectStack.begin(),effectStack.end(),key)!=effectStack.end())throw std::runtime_error("Cyclic explosion dependencies cannot be resized");
            auto cached=effects.find(key);if(cached!=effects.end())return cached->second;
            auto *original=sithTemplate_GetTemplate(key.c_str());if(!original)throw std::runtime_error("Missing explosion dependency: "+key);
            auto values=resizeParameters(key.c_str());EdTemplate effect={};std::string name=prefix+"e"+std::to_string(effectSerial++);
            effects[key]=name;effectStack.push_back(key);snprintf(effect.name,sizeof(effect.name),"%s",name.c_str());
            // Flatten explosions to avoid inheriting a full four-entry debris
            // array, which the engine otherwise appends to rather than replaces.
            snprintf(effect.parent,sizeof(effect.parent),"%s",original->type==SITH_THING_EXPLOSION?"none":key.c_str());
            auto generated=values;
            for(auto field:{"explode","fleshhit","creatething","trailthing"}){auto it=values.find(field);if(it!=values.end()&&it->second!="none")generated[field]=copyEffect(it->second);}
            for(auto field:{"size","movesize","height","range","elementsize","minsize","trailcylradius","damage","mindamage","force"}){auto it=values.find(field);if(it!=values.end()){std::vector<std::string> value{it->second};double amount=original->type==SITH_THING_EXPLOSION&&(!strcmp(field,"damage")||!strcmp(field,"range")||!strcmp(field,"force"))?EdResize::blastScale(factor):factor;EdResize::scale(value,0,1,amount);generated[field]=value[0];}}
            copyResources(values,effect,name);
            for(auto &entry:generated){if(entry.first=="debris")continue;bool overridden=false;for(int i=0;i<effect.count;i++)if(entry.first==effect.params[i].key)overridden=true;
                if(!overridden&&(original->type==SITH_THING_EXPLOSION||entry.second!=values[entry.first]))resizeParam(effect,entry.first.c_str(),entry.second);}
            if(original->type==SITH_THING_EXPLOSION)for(auto *debris:original->explosionParams.apDebries)if(debris){
                std::string debrisName;for(int i=0;i<templateCount;i++)if(sithTemplate_GetTemplate(templateNames[i])==debris){debrisName=templateNames[i];break;}
                if(debrisName.empty())throw std::runtime_error("Cannot identify explosion debris template");resizeParam(effect,"debris",copyEffect(debrisName));
            }
            effectStack.pop_back();weapons.push_back(effect);return name;
        };

        for(auto slot:{"weapon","weapon2"}){
            auto it=params.find(slot);if(it==params.end())continue;auto *weapon=sithTemplate_GetTemplate(it->second.c_str());
            if(!weapon||weapon->type!=SITH_THING_WEAPON)continue;
            bool hasModel=weapon->renderData.type==RD_THING_MODEL3&&weapon->renderData.model3;
            bool instant=(weapon->weaponParams.flags&SITH_WF_INSTANT_IMPACT)!=0;
            if(!hasModel&&!instant)continue;
            EdTemplate copy={};snprintf(copy.name,sizeof(copy.name),"%sw%u",prefix.c_str(),(unsigned)weapons.size());snprintf(copy.parent,sizeof(copy.parent),"%s",it->second.c_str());
            if(instant)resizeParam(copy,"range",EdResize::number(weapon->weaponParams.range*factor));
            if(hasModel){
                auto projectileParams=resizeParameters(it->second.c_str());copyResources(projectileParams,copy,copy.name);
                resizeParam(copy,"size",EdResize::number(weapon->collideSize*factor));
                resizeParam(copy,"movesize",EdResize::number(weapon->moveSize*factor));
                if(weapon->moveType==SITH_MT_PHYSICS)resizeParam(copy,"height",EdResize::number(weapon->physicsParams.height*factor));
                for(auto dimension:{"trailcylradius","elementsize","minsize"}){auto value=projectileParams.find(dimension);if(value!=projectileParams.end()){std::vector<std::string> number{value->second};EdResize::scale(number,0,1,factor);resizeParam(copy,dimension,number[0]);}}
            }
            auto projectileParams=resizeParameters(it->second.c_str());
            for(auto field:{"explode","fleshhit"}){auto effect=projectileParams.find(field);if(effect!=projectileParams.end()&&effect->second!="none")resizeParam(copy,field,copyEffect(effect->second));}
            resizeParam(t,slot,copy.name);weapons.push_back(copy);
        }
        if(base->type==SITH_THING_WEAPON)for(auto field:{"explode","fleshhit"}){auto effect=params.find(field);if(effect!=params.end()&&effect->second!="none")resizeParam(t,field,copyEffect(effect->second));}
        // Actor COG template symbols, rather than weapon/weapon2, choose drops.
        // Clone item references and patch a private script; other symbols (limbs,
        // projectiles, effects) keep their existing behavior.
        bool newActorCog=false;
        auto actorCog=params.find("cog");
        if(base->type==SITH_THING_ACTOR&&actorCog!=params.end()&&actorCog->second!="none"){
            std::map<std::string,std::string> pickups;
            auto script=EdResize::pickupCog(resizeRead("cog/"+actorCog->second),[&](const std::string &source){
                auto key=EdResize::lower(source);auto found=pickups.find(key);if(found!=pickups.end())return found->second;
                auto *pickup=sithTemplate_GetTemplate(key.c_str());if(!pickup||pickup->type!=SITH_THING_ITEM)return source;
                EdTemplate copy={};unsigned duplicate=0;std::string name;
                for(;;){name=EdResize::templateName(key,factor,duplicate);bool used=sithTemplate_GetTemplate(name.c_str())||edTemplateIndex(model,name.c_str())>=0||name==t.name;
                    for(const auto &w:weapons)if(name==w.name)used=true;if(!used)break;duplicate=duplicate?duplicate+1:2;}
                snprintf(copy.name,sizeof(copy.name),"%s",name.c_str());snprintf(copy.parent,sizeof(copy.parent),"%s",key.c_str());
                auto pickupParams=resizeParameters(key.c_str());copyResources(pickupParams,copy,prefix+"d"+std::to_string(pickups.size()));
                resizeParam(copy,"size",EdResize::number(pickup->collideSize*factor));resizeParam(copy,"movesize",EdResize::number(pickup->moveSize*factor));
                if(pickup->moveType==SITH_MT_PHYSICS)resizeParam(copy,"height",EdResize::number(pickup->physicsParams.height*factor));
                weapons.push_back(copy);pickups[key]=name;return name;
            });
            if(!pickups.empty()){std::string name=prefix+".cog";add("cog/"+name,script);resizeParam(t,"cog",name);newActorCog=true;}
        }
        std::string aiName=attributes.aiClass[0]?attributes.aiClass:(params.count("aiclass")?params["aiclass"]:"");
        std::string scaledAI;
        if(!aiName.empty()&&aiName!="none"){
            scaledAI=prefix+".ai";add("misc/ai/"+scaledAI,EdResize::ai(resizeRead("misc/ai/"+aiName),factor));resizeParam(t,"aiclass",scaledAI);
        }
        if(base->type==SITH_THING_ACTOR){resizeParam(t,"health",EdResize::number(base->actorParams.health*factor));resizeParam(t,"maxhealth",EdResize::number(base->actorParams.maxHealth*factor));}
        resizeParam(t,"size",EdResize::number(base->collideSize*factor));resizeParam(t,"movesize",EdResize::number(base->moveSize*factor));
        if(base->moveType==SITH_MT_PHYSICS)resizeParam(t,"height",EdResize::number(base->physicsParams.height*factor));
        for(auto name:{"eyeoffset","fireoffset","lightoffset"}){auto it=params.find(name);if(it==params.end())continue;double x,y,z;if(sscanf(it->second.c_str()," ( %lf / %lf / %lf )",&x,&y,&z)!=3)throw std::runtime_error("Invalid template offset");resizeParam(t,name,"("+EdResize::number(x*factor)+"/"+EdResize::number(y*factor)+"/"+EdResize::number(z*factor)+")");}
        for(auto name:{"elementsize","minsize","trailcylradius","range"}){auto it=params.find(name);if(it==params.end())continue;std::vector<std::string> value{it->second};EdResize::scale(value,0,1,factor);resizeParam(t,name,value[0]);}
        auto *world=sithWorld_g_pCurrentWorld;
        if(newActorCog&&(world->numCogScripts>=world->sizeCogScripts||world->numCogs>=world->sizeCogs))throw std::runtime_error("Not enough actor COG capacity; save and reload before resizing");
        if(world->numSoundClasses+newSoundClasses>world->sizeSoundClasses||world->numSoundsLoaded+sounds.size()>world->numSounds)throw std::runtime_error("Not enough sound capacity; save and reload before resizing");
        if(world->numKeyframes+keys.size()>world->sizeKeyframes)throw std::runtime_error("Not enough animation capacity to resize this thing");
        if((world->numModels+newModels>world->sizeModels)||
           (world->numPuppetClasses+newPuppets>world->sizePuppetClasses)||
           (world->numSprites+newSprites>world->sizeSprites)||
           (world->numParticles+newParticles>world->sizeParticles)||
           world->numThingTemplates>=world->sizeThingTemplates)throw std::runtime_error("Not enough live resource capacity; save and reload before resizing");
        if(model->templateCount+weapons.size()+1>ED_CUSTOM_TEMPLATES||world->numThingTemplates+weapons.size()+1>world->sizeThingTemplates)throw std::runtime_error("Not enough template capacity for resized weapons");
        if(!scaledAI.empty()&&world->numAIClasses>=world->sizeAIClasses)throw std::runtime_error("Not enough AI resource capacity; save and reload before resizing");
        edCopyModel(candidate,model);for(const auto &weapon:weapons)candidate->templates[candidate->templateCount++]=weapon;
        candidate->templates[candidate->templateCount++]=t;
        candidate->objects[id].attributes=attributes;
        if(attributes.aiClass[0]&&!scaledAI.empty())snprintf(candidate->objects[id].attributes.aiClass,sizeof(candidate->objects[id].attributes.aiClass),"%s",scaledAI.c_str());
        // Preserve the existing foot/base elevation, including when shrinking.
        // Match JK floor attachment for physics things, visual bottoms otherwise.
        double lift=0;
        if(base->moveType==SITH_MT_PHYSICS){
            double offset=base->renderData.type==RD_THING_MODEL3&&base->renderData.model3?base->renderData.model3->insertOffset.z:0;
            lift=EdResize::groundHeight(base->physicsParams.height,offset,base->moveSize,factor)-EdResize::groundHeight(base->physicsParams.height,offset,base->moveSize,1);
        }else{
            auto bounds=mapThingBounds(id);double bottom=bounds.first.z-thingPose(model,id).position.z;
            if(base->renderData.type==RD_THING_SPRITE3&&base->renderData.sprite3)bottom=base->renderData.sprite3->offset.z-base->renderData.sprite3->height*.5;
            lift=bottom*(1-factor);
        }
        pose.position.z+=lift;
        candidate->objects[id].position=pose.position;candidate->objects[id].rotation=pose.rotation;
        snprintf(candidate->objects[id].templateName,sizeof(candidate->objects[id].templateName),"%s",t.name);
        if(!edTemplatesValidate(candidate))throw std::runtime_error("Resized template failed validation");
        setCustomResources(std::move(next));installed=true;remember();if(!commit())throw std::runtime_error("Could not apply resized thing");
        int selected=templateIndex(t.name);if(selected>=0)currentTemplate=selected;
        say("Resized thing created with its own template and resources. Ctrl+Z undoes.");return 1;
    }catch(const std::exception &e){if(installed)setCustomResources(std::move(previous));error=e.what();return 0;}
}
static int thingGenerateDialog(int id,const EdThingAttributes &draft,EdFrame pose){
    char16_t input[64]=u"2";
    for(;;){jkGuiElement e[]={
        {ELEMENT_TEXT,0,2,NULL,3,{30,40,580,40},1},
        {ELEMENT_TEXT,0,0,NULL,3,{40,125,560,65},1},
        {ELEMENT_TEXT,0,0,NULL,3,{80,235,100,30},1},
        {ELEMENT_TEXTBOX,0,0,NULL,63,{190,235,150,30},1},
        {ELEMENT_TEXTBUTTON,1,2,NULL,3,{365,228,180,42},1},
        {ELEMENT_TEXTBUTTON,-1,2,NULL,3,{200,400,240,40},1},{ELEMENT_END}};
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
        e[0].wstr=u"Thing Generator";e[1].wstr=u"Create scaled copies of the template and its resources.\n2 doubles the size; 0.5 halves it. Replaces this thing only.";e[2].wstr=u"Scale";e[3].wstr=input;e[4].wstr=u"ReSize";e[5].wstr=u"Back";
        jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&e[4]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[5]);int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);if(choice<0)return 0;
        char text[128],*end;stdString_WcharToChar(text,input,sizeof(text));double factor=strtod(text,&end);std::string error;
        if(end==text||*end||!std::isfinite(factor)||factor<=0)error="Enter a finite scale greater than zero.";
        else if(resizeThing(id,factor,draft,pose,error))return 1;
        char16_t message[512];stdString_CharToWchar(message,error.c_str(),512);jkGuiDialog_ErrorDialog(u"ReSize",message);
    }
}
static int resizeSoundCheck(const SithThing *scaled,const SithThing *original,double factor){
    if(!original->pSoundClass)return 1;
    if(!scaled->pSoundClass||scaled->pSoundClass==original->pSoundClass)return 0;
    for(int mode=0;mode<SITH_SC_MAX;mode++){
        auto *a=original->pSoundClass->entries[mode],*b=scaled->pSoundClass->entries[mode];
        for(;a&&b;a=a->pNextMode,b=b->pNextMode){
            if(a->playflags!=b->playflags||a->maxVolume!=b->maxVolume||fabs(a->minRadius*factor-b->minRadius)>.0001||fabs(a->maxRadius*factor-b->maxRadius)>.0001)return 0;
            if(a->sound&&(!b->sound||a->sound==b->sound||b->sound->sampleRate!=(unsigned)std::round(a->sound->sampleRate/EdResize::timingScale(factor))||a->sound->bufferBytes!=b->sound->bufferBytes))return 0;
        }if(a||b)return 0;
    }return 1;
}
static int resizeActorCheck(const SithThing *scaled,const SithThing *original,double factor){
    if(!scaled||!original||fabs(scaled->actorParams.health-original->actorParams.health*factor)>.0001||fabs(scaled->actorParams.maxHealth-original->actorParams.maxHealth*factor)>.0001)return 0;
    if(original->pPuppetClass){if(!scaled->pPuppetClass)return 0;for(int mode=0;mode<6;mode++)for(unsigned k=0;k<sizeof(original->pPuppetClass->modes[mode].keyframe)/sizeof(SithPuppetClassSubmode);k++){
        auto *a=original->pPuppetClass->modes[mode].keyframe[k].keyframe,*b=scaled->pPuppetClass->modes[mode].keyframe[k].keyframe;
        if(a&&(!b||fabs(b->fps-a->fps/EdResize::timingScale(factor))>.0001||b->numFrames!=a->numFrames))return 0;
    }}
    auto *a=original->actorParams.pWeaponTemplate,*b=scaled->actorParams.pWeaponTemplate;
    if(a&&a->renderData.type==RD_THING_MODEL3){if(!b||a==b||b->renderData.type!=RD_THING_MODEL3||a->renderData.model3==b->renderData.model3||fabs(b->renderData.model3->radius-a->renderData.model3->radius*factor)>.0001||fabs(b->collideSize-a->collideSize*factor)>.0001||fabs(b->moveSize-a->moveSize*factor)>.0001||b->weaponParams.damage!=a->weaponParams.damage)return 0;}
    return resizeSoundCheck(scaled,original,factor);
}
static int resizeDropCheck(){
    auto params=resizeParameters(model->objects[1].templateName);if(!params.count("cog")||params["cog"]=="actor_st.cog")return 0;
    auto script=resizeRead("cog/"+params["cog"]);int count=0,ok=1;
    EdResize::pickupCog(script,[&](const std::string &name){
        auto *drop=sithTemplate_GetTemplate(name.c_str());if(drop&&drop->type==SITH_THING_ITEM){
            count++;int index=edTemplateIndex(model,name.c_str());if(index<0){ok=0;return name;}
            auto *base=sithTemplate_GetTemplate(model->templates[index].parent);
            ok=ok&&base&&fabs(drop->collideSize-base->collideSize*2)<.00001&&fabs(drop->moveSize-base->moveSize*2)<.00001;
            if(base&&base->renderData.type==RD_THING_MODEL3)ok=ok&&drop->renderData.model3!=base->renderData.model3&&fabs(drop->renderData.model3->radius-base->renderData.model3->radius*2)<.00001;
        }return name;
    });return ok&&count>0;
}
static int resizeDeathDropCheck(){
    auto *world=sithWorld_g_pCurrentWorld;auto *definition=sithTemplate_GetTemplate(model->objects[1].templateName);
    int sector=objectSector(model,model->spawn);if(!definition||!definition->pCog||sector<0)return 0;
    rdVector3 position=rdvec(model->spawn);rdMatrix34 orient;rdVector3 angles={0};rdMatrix_BuildRotate34(&orient,&angles);
    auto *enemy=sithThing_CreateThingAtPos(definition,&position,&orient,&world->aSectors[sector],NULL);if(!enemy)return 0;
    std::vector<int> signatures;for(unsigned i=0;i<world->numThingsLoaded;i++)signatures.push_back(world->aThings[i].signature);
    sithCog_SendMessage(enemy->pCog,SITH_MESSAGE_KILLED,SENDERTYPE_THING,enemy->idx,0,0,0);
    int drops=0;for(unsigned i=0;i<world->numThingsLoaded;i++){auto *thing=&world->aThings[i];
        if(thing->type==SITH_THING_ITEM&&(i>=signatures.size()||thing->signature!=signatures[i])){drops++;sithThing_RemoveThing(thing);}}
    sithThing_RemoveThing(enemy);printf("RESIZE actual death drops: %d\n",drops);return drops==1;
}
static int resizeExplosionCheck(){
    if(model->objectCount<7)return 0;auto *weapon=sithTemplate_GetTemplate(model->objects[6].templateName),*original=sithTemplate_GetTemplate("+raildet");
    if(!weapon||!original)return 0;auto *blast=weapon->weaponParams.pExplosionTemplate,*stock=original->weaponParams.pExplosionTemplate;
    if(!blast||!stock||blast==stock||blast!=weapon->weaponParams.fleshHitTemplate||blast->type!=SITH_THING_EXPLOSION)return 0;
    if(fabs(blast->explosionParams.damage-stock->explosionParams.damage*4)>.0001||fabs(blast->explosionParams.range-stock->explosionParams.range*4)>.0001||fabs(blast->explosionParams.force-stock->explosionParams.force*4)>.0001)return 0;
    if(blast->renderData.type!=RD_THING_SPRITE3||blast->renderData.sprite3==stock->renderData.sprite3||fabs(blast->renderData.sprite3->width-stock->renderData.sprite3->width*8)>.0001)return 0;
    for(int i=0;i<4;i++){auto *d=blast->explosionParams.apDebries[i],*o=stock->explosionParams.apDebries[i];if(!d||!o||d==o||fabs(d->collideSize-o->collideSize*8)>.0001||d->renderData.model3==o->renderData.model3)return 0;
        auto *hit=d->weaponParams.pExplosionTemplate;if(!hit||hit==o->weaponParams.pExplosionTemplate||!hit->pCreateThingTemplate||hit->pCreateThingTemplate==o->weaponParams.pExplosionTemplate->pCreateThingTemplate)return 0;}
    // Exercise the actual template spawn path as well as the serialized values.
    auto *world=sithWorld_g_pCurrentWorld;rdVector3 pos=rdvec(model->spawn),angles={0};rdMatrix34 orient;rdMatrix_BuildRotate34(&orient,&angles);
    int sector=objectSector(model,model->spawn);auto *spawned=sector>=0?sithThing_CreateThingAtPos(blast,&pos,&orient,&world->aSectors[sector],NULL):NULL;
    if(!spawned)return 0;int ok=spawned->explosionParams.damage==blast->explosionParams.damage&&spawned->renderData.sprite3==blast->renderData.sprite3;sithThing_RemoveThing(spawned);return ok;
}
static int resizeSelfTest(){
    active=1;clearHistory();
    if(getenv("GEOSITH_RESIZE_RELOAD")){
        int ok=model->objectCount>=4;
        for(int i=0;i<4&&ok;i++){auto *t=sithTemplate_GetTemplate(model->objects[i].templateName);ok=t&&std::string(model->objects[i].templateName).find("size")!=std::string::npos;if(i==1)ok=ok&&t->pPuppetClass&&fabs(t->collideSize-.13)<.00001;}
        if(ok&&model->objectCount>5){auto *dragon=sithTemplate_GetTemplate(model->objects[5].templateName);ok=dragon&&dragon->pClass&&dragon->actorParams.pWeaponTemplate&&fabs(dragon->pClass->entries[2].fltArg[2]-2)<.00001&&fabs(dragon->actorParams.pWeaponTemplate->weaponParams.range-2.4)<.00001;}
        if(ok)ok=resizeActorCheck(sithTemplate_GetTemplate(model->objects[1].templateName),sithTemplate_GetTemplate("stormtroop"),2);
        if(ok)ok=resizeDropCheck()&&resizeDeathDropCheck()&&resizeExplosionCheck();
        printf("GEOSITH_RESIZE_RELOAD %s\n",ok?"PASS":"FAIL");return ok;
    }
    const char *names[]={"crate4_1","stormtroop","+laserhit","+force_shield"};std::string error;int ok=1;
    edCopyModel(candidate,model);candidate->objectCount=4;
    for(int i=0;i<4;i++){candidate->objects[i]=EdObject{};snprintf(candidate->objects[i].templateName,64,"%s",names[i]);candidate->objects[i].position=edAdd(model->spawn,edV(.05*i,.1,0));}
    if(!commit()){puts("RESIZE fixture failed");return 0;}
    for(int i=0;i<4&&ok;i++){
        auto *base=sithTemplate_GetTemplate(names[i]);if(!base){printf("RESIZE missing %s\n",names[i]);return 0;}
        double collision=base->collideSize;auto originalPuppet=base->pPuppetClass;auto originalSprite=base->renderData.sprite3;double spriteWidth=base->renderData.type==RD_THING_SPRITE3?base->renderData.sprite3->width:0;int originalType=base->renderData.type;
        double radius=base->renderData.type==RD_THING_MODEL3?base->renderData.model3->radius:0;
        auto bounds=mapThingBounds(i);double bottom=bounds.first.z-model->objects[i].position.z;
        if(originalType==RD_THING_SPRITE3)bottom=base->renderData.sprite3->offset.z-base->renderData.sprite3->height*.5;
        double expectedLift=-bottom;
        if(base->moveType==SITH_MT_PHYSICS){double offset=originalType==RD_THING_MODEL3?base->renderData.model3->insertOffset.z:0;expectedLift=EdResize::groundHeight(base->physicsParams.height,offset,base->moveSize,2)-sithPhysics_GetThingHeight(base);}
        double oldZ=model->objects[i].position.z;
        ok=resizeThing(i,2,model->objects[i].attributes,thingPose(model,i),error);
        ok=ok&&fabs(model->objects[i].position.z-oldZ-expectedLift)<.00001&&currentTemplate==templateIndex(model->objects[i].templateName);
        printf("RESIZE %s %s %s\n",names[i],ok?"applied":"FAILED",error.c_str());
        auto *scaled=sithTemplate_GetTemplate(model->objects[i].templateName);
        ok=ok&&scaled&&scaled!=base&&fabs(scaled->collideSize-collision*2)<.00001;
        printf("RESIZE values collision %g/%g radius %g/%g puppet %p/%p\n",scaled?scaled->collideSize:0,collision,scaled&&scaled->renderData.type==RD_THING_MODEL3?scaled->renderData.model3->radius:0,radius,scaled?scaled->pPuppetClass:NULL,originalPuppet);
        if(ok)ok=resizeSoundCheck(scaled,sithTemplate_GetTemplate(names[i]),2);
        if(ok&&radius)ok=fabs(scaled->renderData.model3->radius-radius*2)<.00001;
        if(ok&&originalPuppet)ok=scaled->pPuppetClass&&scaled->pPuppetClass!=originalPuppet;
        if(ok&&i==1){ok=resizeActorCheck(scaled,sithTemplate_GetTemplate("stormtroop"),2);printf("RESIZE actor/projectile/health/playback %s\n",ok?"PASS":"FAIL");}
        if(ok&&originalType==RD_THING_SPRITE3)ok=scaled->renderData.sprite3!=originalSprite&&fabs(scaled->renderData.sprite3->width-spriteWidth*2)<.00001;
        if(ok)ok=historyStep(0)&&!strcmp(model->objects[i].templateName,names[i])&&historyStep(1);
    }
    if(ok){auto old=customResourceBytes;auto t=model->templateCount;ok=!resizeThing(0,-1,model->objects[0].attributes,thingPose(model,0),error)&&t==model->templateCount&&old==customResourceBytes;}
    if(ok){
        edCopyModel(candidate,model);int asset=candidate->assetCount++;candidate->assets[asset]=EdAsset{};snprintf(candidate->assets[asset].name,32,"resize_fixture");candidate->assets[asset].origin=model->spawn;candidate->assets[asset].saved=1;
        ok=edMeshPrimitive(candidate,asset,0,model->spawn,0);
        EdTemplate t={};snprintf(t.name,64,"resize_fixture");snprintf(t.parent,64,"_walkstruct");resizeParam(t,"model3d","resize_fixture.3do");candidate->templates[candidate->templateCount++]=t;
        int id=candidate->objectCount++;candidate->objects[id]=EdObject{};snprintf(candidate->objects[id].templateName,64,"resize_fixture");candidate->objects[id].position=model->spawn;
        ok=ok&&commit();if(ok){double radius=sithTemplate_GetTemplate("resize_fixture")->renderData.model3->radius;ok=resizeThing(id,.5,model->objects[id].attributes,thingPose(model,id),error);auto *scaled=sithTemplate_GetTemplate(model->objects[id].templateName);ok=ok&&scaled&&fabs(scaled->renderData.model3->radius-radius*.5)<.00001;}
        printf("RESIZE custom model %s %s\n",ok?"PASS":"FAIL",error.c_str());
    }
    if(ok){
        edCopyModel(candidate,model);int id=candidate->objectCount++;candidate->objects[id]=EdObject{};snprintf(candidate->objects[id].templateName,64,"kelldragon");candidate->objects[id].position=model->spawn;
        // Exercise the placed-thing AI override as well as template inheritance.
        snprintf(candidate->objects[id].attributes.aiClass,sizeof(candidate->objects[id].attributes.aiClass),"kddefault.ai");
        ok=commit()&&resizeThing(id,8,model->objects[id].attributes,thingPose(model,id),error);
        auto *dragon=sithTemplate_GetTemplate(model->objects[id].templateName);auto *bite=dragon?dragon->actorParams.pWeaponTemplate:NULL;
        ok=ok&&dragon&&dragon->pClass&&fabs(dragon->collideSize-1)<.00001&&bite&&fabs(bite->weaponParams.range-2.4)<.00001;
        ok=ok&&fabs(dragon->pClass->entries[2].fltArg[2]-2)<.00001&&fabs(dragon->pClass->entries[3].fltArg[1]-.8)<.00001;
        ok=ok&&resizeActorCheck(dragon,sithTemplate_GetTemplate("kelldragon"),8);
        auto *original=sithTemplate_GetTemplate("+kellbite");ok=ok&&original&&fabs(original->weaponParams.range-.3)<.00001&&bite->weaponParams.damage==original->weaponParams.damage;
        printf("RESIZE KELL 8x %s: collision 1, AI attack distance 2, bite reach 2.4, original bite and damage preserved (%s)\n",ok?"PASS":"FAIL",error.c_str());
    }
    if(ok){edCopyModel(candidate,model);int id=candidate->objectCount++;candidate->objects[id]=EdObject{};snprintf(candidate->objects[id].templateName,64,"+raildet");candidate->objects[id].position=model->spawn;
        ok=commit()&&resizeThing(id,8,model->objects[id].attributes,thingPose(model,id),error)&&resizeExplosionCheck();printf("RESIZE explosion/debris %s %s\n",ok?"PASS":"FAIL",error.c_str());}
    // The smoke room is shorter than an 8x dragon's requested vertical lift.
    // Placement was checked above; bring the fixtures inside for export/reload.
    if(ok){edCopyModel(candidate,model);for(int i=0;i<candidate->objectCount;i++)candidate->objects[i].position=model->spawn;ok=commit();}
    if(ok){ok=resizeDropCheck()&&resizeDeathDropCheck();printf("RESIZE actor pickup %s\n",ok?"PASS":"FAIL");}
    if(ok)ok=writeEditorGob(model,"Episode/sithed_resize_test.gob","Resize test",0);
    printf("GEOSITH_RESIZE %s: models, animations, sprites, particles, collision, undo/redo and GOB\n",ok?"PASS":"FAIL");return ok;
}
