#include "edPreset.h"
#include <cstdio>
#include <fstream>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    std::string name;CHECK(edPresetFilename(u"Door Frame",name)&&name=="Door Frame");CHECK(edPresetFilename(u"  Door Frame  ",name)&&name=="Door Frame");
    CHECK(!edPresetFilename(u"   ",name)&&!edPresetFilename(u"../door",name)&&!edPresetFilename(u"CON",name)&&!edPresetFilename(u"Door:Frame",name));
    auto source=std::make_unique<EdModel>(),target=std::make_unique<EdModel>(),out=std::make_unique<EdModel>();edDefault(source.get());edDefault(target.get());
    EdHit exact;EdVec look=edV(.99,1,-.49);look=edScale(look,1/sqrt(edDot(look,look)));
    CHECK(edRaySurface(target.get(),edV(0,0,.5),look,&exact,0));
    CHECK(edDot(edNormal(&target->c[exact.cell].f[exact.face]),edV(0,-1,0))>.999&&exact.vertex<0&&exact.edge<0);
    EdObject lift={};strcpy(lift.templateName,"lift");lift.position=edV(0,0,.2);lift.attributes.frameCount=2;lift.attributes.pathOverride=1;lift.attributes.frames[0].position=lift.position;lift.attributes.frames[1].position=edV(0,0,.8);source->objects[source->objectCount++]=lift;
    EdTemplate t={};strcpy(t.name,"lift");strcpy(t.parent,"_walkstruct");t.count=1;strcpy(t.params[0].key,"model3d");strcpy(t.params[0].value,"platform.3do");source->templates[source->templateCount++]=t;
    source->assetCount=1;strcpy(source->assets[0].name,"platform");CHECK(edMeshPrimitive(source.get(),0,0,edV(0,0,.2),0));
    EdPlacedCog cog={};CHECK(edCogParseScript(&cog,"elev.cog","symbols\nthing elevator\nsector top\nsurface switch\nend\n"));
    strcpy(cog.params[0].value,"1");cog.params[1].cell=0;cog.params[2].cell=0;cog.params[2].face=0;source->cogs[source->cogCount++]=cog;
    auto preset=std::make_unique<EdPreset>();EdBuffer<unsigned char> sectors;sectors[0]=1;std::set<int> objects={0};std::string error;
    CHECK(edPresetCapture(*preset,*source,sectors,objects,{0,2,-1,-1},error));
    CHECK(preset->model.objectCount==1&&preset->model.cogCount==1&&preset->model.templateCount==1&&preset->model.assetCount==1);
    CHECK(!strcmp(preset->model.cogs[0].params[0].value,"0")&&preset->model.objects[0].attributes.frameCount==2);
    preset->materials={"dflt.mat"};preset->resources.add("cog/elev.cog",{'x','y'});
    auto bytes=edPresetEncode(*preset);auto loaded=std::make_unique<EdPreset>();CHECK(edPresetDecode(bytes,*loaded)&&edModelsEqual(&preset->model,&loaded->model));
    auto invalid=bytes;invalid.pop_back();CHECK(!edPresetDecode(invalid,*loaded));CHECK(edPresetDecode(bytes,*loaded));
    for(int s=0;s<loaded->model.count;s++)for(int f=0;f<loaded->model.c[s].count;f++)for(int v=0;v<loaded->model.c[s].f[f].count;v++)loaded->model.c[s].f[f].v[v].p.y+=2;
    loaded->model.objects[0].position.y+=2;for(int f=0;f<2;f++)loaded->model.objects[0].attributes.frames[f].position.y+=2;
    CHECK(edPresetInsert(*out,*target,loaded->model,error)&&edValidate(out.get()));
    CHECK(out->count==2&&out->objectCount==1&&out->cogCount==1&&out->cogs[0].params[1].cell==1);
    CHECK(atoi(out->cogs[0].params[0].value)==out->spawnCount&&!strcmp(out->objects[0].templateName,out->templates[0].name));
    int portals=0;for(int s=0;s<out->count;s++)for(int f=0;f<out->c[s].count;f++)portals+=out->c[s].f[f].neighbor>=0;CHECK(portals==2);
    CHECK(out->objects[0].attributes.frames[1].position.y==2&&out->objects[0].attributes.frames[1].position.z==.8);
    // An adjoin to an unselected room is a valid mounting face.
    EdBuffer<unsigned char> portalSelection;portalSelection[1]=1;int mountFace=-1;
    for(int f=0;f<out->c[1].count;f++)if(out->c[1].f[f].neighbor==0)mountFace=f;
    auto portalPreset=std::make_unique<EdPreset>();CHECK(mountFace>=0);
    CHECK(edPresetCapture(*portalPreset,*out,portalSelection,{0},{1,mountFace,-1,-1},error));
    CHECK(portalPreset->model.c[0].f[portalPreset->mountFace].neighbor==-1);
    EdBuffer<unsigned char> frameOnly;frameOnly[0]=1;
    CHECK(edPresetCapture(*portalPreset,*out,frameOnly,{0},{0,2,-1,-1},error));
    CHECK(portalPreset->model.count==2); // Include the selected lift's unselected containing room.
    auto again=std::make_unique<EdModel>(*out);for(int f=0;f<loaded->model.c[0].count;f++)for(int v=0;v<loaded->model.c[0].f[f].count;v++)loaded->model.c[0].f[f].v[v].p.y+=2;
    CHECK(edPresetInsert(*again,*out,loaded->model,error)&&again->templateCount==2&&strcmp(again->templates[0].name,again->templates[1].name));
    objects.clear();CHECK(!edPresetCapture(*preset,*source,sectors,objects,{0,2,-1,-1},error)&&error.find("elevator")!=std::string::npos);
    source->cogCount=0;sectors.clear();objects.insert(0);CHECK(edPresetCapture(*preset,*source,sectors,objects,{-1,-1,-1,-1},error)&&preset->model.count==0);
    CHECK(edPresetInsert(*out,*target,preset->model,error)&&out->count==1&&out->objectCount==1);
    puts("PASS: preset capture, dependencies, frames, COG references, file roundtrip, portal placement, repeat placement and thing-only presets");return 0;
}
