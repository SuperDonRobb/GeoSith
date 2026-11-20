#ifndef ED_PRESET_H
#define ED_PRESET_H
#include "edGeometry.h"
#include "edWire.h"
#include "edResources.h"
#include <set>
#include <string>

struct EdPreset {
    EdModel model;
    int mountCell=-1,mountFace=-1;
    std::vector<std::string> materials;
    EdResources resources;
};
static bool edPresetFilename(const char16_t *input,std::string &name){
    name.clear();for(int i=0;input[i];i++){if(i>=27)return false;char16_t c=input[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='_'||c=='-'))return false;name+=(char)c;}
    size_t first=name.find_first_not_of(' ');if(first==std::string::npos)return false;
    name=name.substr(first,name.find_last_not_of(' ')-first+1);
    std::string upper=name;for(char &c:upper)if(c>='a'&&c<='z')c-=32;
    if(upper=="CON"||upper=="PRN"||upper=="AUX"||upper=="NUL"||(upper.size()==4&&(upper.starts_with("COM")||upper.starts_with("LPT"))&&upper[3]>='0'&&upper[3]<='9'))return false;
    return true;
}
static bool edPresetValid(const EdModel &m){
    if(m.count)return edValidate(&m)!=0;
    if(!m.objectCount&&!m.spawnCount)return false;
    auto check=std::make_unique<EdModel>(m),room=std::make_unique<EdModel>();edDefault(room.get());check->count=1;check->c[0]=room->c[0];return edValidate(check.get())!=0;
}
static bool edPresetCapture(EdPreset &result,const EdModel &source,const EdBuffer<unsigned char> &sectors,const std::set<int> &things,EdHit mount,std::string &error){
    auto storage=std::make_unique<EdPreset>();auto &out=*storage;auto &m=out.model;
    EdBuffer<unsigned char> included=sectors;bool hasSectors=false;
    for(int s=0;s<source.count;s++)hasSectors|=sectors[s]!=0;
    // A door/lift's origin can be in an unselected pocket between its frame sectors.
    // Keep that room in the assembly; otherwise the thing cannot exist after placement.
    if(hasSectors)for(int id:things){
        if((id>=0&&id>=source.objectCount)||(id<0&&-id-1>=source.spawnCount))continue;
        EdVec point=id>=0?source.objects[id].position:source.spawns[-id-1].position;
        int owner=-1;for(int s=0;s<source.count;s++)if(edContains(&source.c[s],point)){owner=s;if(included[s])break;}
        if(owner<0){error="A selected thing is outside the source level. Move its origin inside a sector before creating the preset.";return false;}
        included[owner]=1;
    }
    m.header=source.header;std::vector<int> cells(source.count,-1),ids(source.spawnCount+source.objectCount,-1),cogs(source.cogCount,-1);
    for(int s=0;s<source.count;s++)if(included[s]){cells[s]=m.count;m.c[m.count]=source.c[s];m.c[m.count].splineId=m.c[m.count].arrayId=0;m.count++;}
    for(int s=0;s<source.count;s++)if(cells[s]>=0)for(int f=0;f<source.c[s].count;f++){
        EdFace &face=m.c[cells[s]].f[f];face.attributes=edSurfaceAttributes(&face);face.hasAttributes=1;face.customUV=1;
        if(face.neighbor>=0){face.neighbor=cells[face.neighbor];if(face.neighbor<0){face.mirror=-1;face.attributes=edSurfaceAttributes(&face);}}
    }
    for(int i=0;i<source.spawnCount;i++)if(things.count(-i-1)){ids[i]=m.spawnCount;m.spawns[m.spawnCount++]=source.spawns[i];}
    for(int i=0;i<source.objectCount;i++)if(things.count(i)){ids[source.spawnCount+i]=m.spawnCount+m.objectCount;m.objects[m.objectCount++]=source.objects[i];}
    if(!m.count&&!m.objectCount&&!m.spawnCount){error="Select sectors and/or things first.";return false;}
    if(m.spawnCount)m.spawn=m.spawns[0].position;
    auto related=[&](const EdCogParam &p){if(p.type==ED_COG_SECTOR||p.type==ED_COG_SURFACE)return p.cell>=0&&p.cell<source.count&&cells[p.cell]>=0;if(p.type==ED_COG_THING){int n=atoi(p.value);return n>=0&&n<(int)ids.size()&&ids[n]>=0;}return false;};
    for(int c=0;c<source.cogCount;c++)for(int p=0;p<source.cogs[c].count;p++)if(related(source.cogs[c].params[p]))cogs[c]=0;
    bool changed=true;while(changed){changed=false;for(int c=0;c<source.cogCount;c++)if(cogs[c]>=0)for(int p=0;p<source.cogs[c].count;p++)if(source.cogs[c].params[p].type==ED_COG_COG){int n=atoi(source.cogs[c].params[p].value);if(n>=0&&n<source.cogCount&&cogs[n]<0){cogs[n]=0;changed=true;}}}
    for(int c=0;c<source.cogCount;c++)if(cogs[c]>=0)cogs[c]=m.cogCount++;
    for(int c=0;c<source.cogCount;c++)if(cogs[c]>=0){EdPlacedCog cog=source.cogs[c];for(int j=0;j<cog.count;j++){
        EdCogParam &p=cog.params[j];int missing=0;
        if((p.type==ED_COG_SECTOR||p.type==ED_COG_SURFACE)&&p.cell>=0){missing=p.cell>=source.count||cells[p.cell]<0;if(!missing)p.cell=cells[p.cell];}
        if(p.type==ED_COG_THING){int n=atoi(p.value);missing=n>=0&&(n>=(int)ids.size()||ids[n]<0);if(n>=0&&!missing)snprintf(p.value,sizeof(p.value),"%d",ids[n]);}
        if(p.type==ED_COG_COG){int n=atoi(p.value);if(n>=0)snprintf(p.value,sizeof(p.value),"%d",cogs[n]);}
        if(missing){error=std::string(cog.file)+" / "+p.name+": select the referenced sector or thing too.";return false;}
    }m.cogs[cogs[c]]=cog;}
    // Copy custom template dependencies, including parents and debris templates.
    std::set<std::string> names;
    for(int i=0;i<m.objectCount;i++)names.insert(m.objects[i].templateName);
    for(int c=0;c<m.cogCount;c++)for(int p=0;p<m.cogs[c].count;p++)if(m.cogs[c].params[p].type==ED_COG_TEMPLATE)names.insert(m.cogs[c].params[p].value);
    changed=true;while(changed){changed=false;for(int t=0;t<source.templateCount;t++)if(names.count(source.templates[t].name)){
        changed|=names.insert(source.templates[t].parent).second;
        for(int p=0;p<source.templates[t].count;p++)if(edTemplateIndex(&source,source.templates[t].params[p].value)>=0)changed|=names.insert(source.templates[t].params[p].value).second;
    }}
    for(int t=0;t<source.templateCount;t++)if(names.count(source.templates[t].name))m.templates[m.templateCount++]=source.templates[t];
    std::set<std::string> models;
    for(int t=0;t<m.templateCount;t++)for(int p=0;p<m.templates[t].count;p++)if(!strcmp(m.templates[t].params[p].key,"model3d"))models.insert(m.templates[t].params[p].value);
    for(int c=0;c<m.cogCount;c++)for(int p=0;p<m.cogs[c].count;p++)if(m.cogs[c].params[p].type==ED_COG_MODEL)models.insert(m.cogs[c].params[p].value);
    for(int a=0;a<source.assetCount;a++)if(models.count(std::string(source.assets[a].name)+".3do")){
        int id=m.assetCount++;m.assets[id]=source.assets[a];m.assets[id].terrainGroup=0;
        for(int i=0;i<source.meshCount;i++)if(source.meshes[i].asset==a){m.meshes[m.meshCount]=source.meshes[i];m.meshes[m.meshCount++].asset=id;}
    }
    m.decalCount=source.decalCount;m.decals=source.decals;edCompactDecals(&m);
    if(m.count){
        if(mount.cell<0||mount.cell>=source.count||cells[mount.cell]<0||mount.face<0||mount.face>=source.c[mount.cell].count){error="Click an outer sector face first to set the mounting face.";return false;}
        out.mountCell=cells[mount.cell];out.mountFace=mount.face;const EdFace &f=m.c[out.mountCell].f[out.mountFace];EdVec n=edNormal(&f);
        if(f.neighbor>=0){error="The mounting face must be on the outside of the selected sectors.";return false;}
        for(int s=0;s<m.count;s++)for(int j=0;j<m.c[s].count;j++)for(int v=0;v<m.c[s].f[j].count;v++)if(edDot(n,edSub(m.c[s].f[j].v[v].p,f.v[0].p))<-.001){error="Choose a mounting face with the entire preset behind it.";return false;}
    }
    if(!edPresetValid(m)){error="The selected preset geometry or COG references are invalid.";return false;}result=std::move(out);return true;
}
// The caller transforms positions/rotations and maps materials before insertion.
static bool edPresetInsert(EdModel &result,const EdModel &source,const EdModel &clip,std::string &error){
    if(source.spawnCount+clip.spawnCount>ED_SPAWNS||source.objectCount+clip.objectCount>ED_OBJECTS||source.cogCount+clip.cogCount>ED_COGS||source.templateCount+clip.templateCount>ED_CUSTOM_TEMPLATES){error="The preset exceeds this level's thing, template, spawn or COG capacity.";return false;}
    auto storage=std::make_unique<EdModel>(source);EdModel &m=*storage;
    // New walkplayers shift the IDs of every existing non-player thing.
    for(int c=0;c<m.cogCount;c++)for(int p=0;p<m.cogs[c].count;p++)if(m.cogs[c].params[p].type==ED_COG_THING){auto &param=m.cogs[c].params[p];int id=atoi(param.value);if(id>=source.spawnCount)snprintf(param.value,32,"%d",id+clip.spawnCount);}
    std::map<std::string,std::string> rename;
    int serial=1;std::string prefix;
    for(;;serial++){prefix="ps"+std::to_string(serial)+"_";bool exists=false;for(int t=0;t<source.templateCount;t++)exists|=std::string(source.templates[t].name).starts_with(prefix);for(int a=0;a<source.assetCount;a++)exists|=std::string(source.assets[a].name).starts_with(prefix);if(!exists)break;}
    for(int t=0;t<clip.templateCount;t++)rename[clip.templates[t].name]=prefix+"t"+std::to_string(t);
    for(int a=0;a<clip.assetCount;a++)rename[std::string(clip.assets[a].name)+".3do"]=prefix+"m"+std::to_string(a)+".3do";
    auto remap=[&](char *text,size_t size){auto it=rename.find(text);if(it!=rename.end())snprintf(text,size,"%s",it->second.c_str());};
    for(int a=0;a<clip.assetCount;a++){EdAsset asset=clip.assets[a];snprintf(asset.name,sizeof(asset.name),"%sm%d",prefix.c_str(),a);m.assets[m.assetCount++]=asset;}
    for(int i=0;i<clip.meshCount;i++){EdMesh mesh=clip.meshes[i];mesh.asset+=source.assetCount;for(int f=0;f<mesh.geometry.count;f++)if(mesh.geometry.f[f].decalId)mesh.geometry.f[f].decalId+=source.decalCount;m.meshes[m.meshCount++]=std::move(mesh);}
    for(int d=0;d<clip.decalCount;d++)m.decals[m.decalCount++]=clip.decals[d];
    for(int t=0;t<clip.templateCount;t++){EdTemplate temp=clip.templates[t];remap(temp.name,sizeof(temp.name));remap(temp.parent,sizeof(temp.parent));for(int p=0;p<temp.count;p++)remap(temp.params[p].value,sizeof(temp.params[p].value));m.templates[m.templateCount++]=temp;}
    for(int i=0;i<clip.spawnCount;i++)m.spawns[m.spawnCount++]=clip.spawns[i];
    for(int i=0;i<clip.objectCount;i++){EdObject obj=clip.objects[i];remap(obj.templateName,sizeof(obj.templateName));m.objects[m.objectCount++]=obj;}
    for(int c=0;c<clip.cogCount;c++){EdPlacedCog cog=clip.cogs[c];for(int p=0;p<cog.count;p++){
        EdCogParam &v=cog.params[p];if((v.type==ED_COG_SECTOR||v.type==ED_COG_SURFACE)&&v.cell>=0)v.cell+=source.count;
        if(v.type==ED_COG_THING){int id=atoi(v.value);if(id>=0)snprintf(v.value,32,"%d",id<clip.spawnCount?source.spawnCount+id:source.spawnCount+source.objectCount+id);}
        if(v.type==ED_COG_COG){int id=atoi(v.value);if(id>=0)snprintf(v.value,32,"%d",source.cogCount+id);}
        if(v.type==ED_COG_TEMPLATE||v.type==ED_COG_MODEL)remap(v.value,sizeof(v.value));
    }m.cogs[m.cogCount++]=cog;}
    for(int c=0;c<clip.count;c++){EdCell cell=clip.c[c];cell.splineId=cell.arrayId=0;for(int f=0;f<cell.count;f++){EdFace &face=cell.f[f];if(face.neighbor>=0)face.neighbor+=source.count;if(face.decalId)face.decalId+=source.decalCount;}m.c[m.count++]=std::move(cell);}
    if(clip.count&&!edResolve(&m)){error=edGeometryFailure();return false;}
    if(clip.count&&m.count<=source.count){error="The preset adds no room beyond this surface.";return false;}
    if(!edValidate(&m)){error="Preset placement would create invalid geometry or references.";return false;}
    result=std::move(m);return true;
}
static EdResources::Bytes edPresetEncode(const EdPreset &preset){
    uint32_t size=0;auto wire=edWireEncode(&preset.model,&size);if(!wire)throw std::runtime_error("Could not serialize preset.");
    auto resources=preset.resources.encode();EdResources::Bytes out;for(uint32_t v:{0x31505347u,1u,(uint32_t)preset.mountCell,(uint32_t)preset.mountFace,(uint32_t)preset.materials.size(),size,(uint32_t)resources.size()})EdResources::put(out,v);
    for(auto &name:preset.materials){EdResources::put(out,(uint32_t)name.size());out.insert(out.end(),name.begin(),name.end());}
    out.insert(out.end(),wire,wire+size);edFree(wire);out.insert(out.end(),resources.begin(),resources.end());return out;
}
static bool edPresetDecode(const EdResources::Bytes &data,EdPreset &out){
    if(data.size()<28||data.size()>ED_NET_LIMIT||EdResources::get(data.data())!=0x31505347u||EdResources::get(data.data()+4)!=1)return false;
    auto storage=std::make_unique<EdPreset>();auto &p=*storage;p.mountCell=(int)EdResources::get(data.data()+8);p.mountFace=(int)EdResources::get(data.data()+12);
    uint32_t count=EdResources::get(data.data()+16),modelSize=EdResources::get(data.data()+20),resourceSize=EdResources::get(data.data()+24);if(count>65536||!count)return false;size_t at=28;
    for(uint32_t i=0;i<count;i++){if(at+4>data.size())return false;uint32_t n=EdResources::get(data.data()+at);at+=4;if(n<1||n>127||n>data.size()-at)return false;std::string name((const char*)data.data()+at,n);at+=n;if(name.find_first_of("/\\")!=std::string::npos||EdResources::path("mat/"+name).empty())return false;p.materials.push_back(name);}
    if(modelSize>data.size()-at||resourceSize!=data.size()-at-modelSize||!edWireDecode(&p.model,data.data()+at,modelSize)||!EdResources::decode(data.data()+at+modelSize,resourceSize,p.resources)||!edPresetValid(p.model))return false;
    if(p.model.count&&(p.mountCell<0||p.mountCell>=p.model.count||p.mountFace<0||p.mountFace>=p.model.c[p.mountCell].count))return false;
    auto validMaterial=[&](const EdCell &c){for(int f=0;f<c.count;f++)if(c.f[f].material<0||c.f[f].material>=(int)p.materials.size())return false;return true;};
    for(int c=0;c<p.model.count;c++)if(!validMaterial(p.model.c[c]))return false;
    for(int m=0;m<p.model.meshCount;m++)if(!validMaterial(p.model.meshes[m].geometry))return false;
    out=std::move(p);return true;
}
#endif
