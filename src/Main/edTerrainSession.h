#ifndef ED_TERRAIN_SESSION_H
#define ED_TERRAIN_SESSION_H
#include "edWire.h"
#include <string>
#include <cmath>
#include <cstdio>
#include "edSurfaceSession.h"
static int edTerrainNamed(const EdModel *m,const std::string &name){for(int a=0;a<m->assetCount;a++)if(m->assets[a].terrain&&name==m->assets[a].name)return a;return -1;}
static std::string edTerrainKey(const EdModel *m,int asset){
    if(asset<0||asset>=m->assetCount||!m->assets[asset].terrain)return {};
    int group=m->assets[asset].terrainGroup;
    if(group)for(int a=0;a<asset;a++)if(m->assets[a].terrain&&m->assets[a].terrainGroup==group)return m->assets[a].name;
    return m->assets[asset].name;
}
static bool edTerrainMember(const EdModel *m,int a,int root){return a==root||(m->assets[a].terrain&&m->assets[root].terrainGroup&&m->assets[a].terrainGroup==m->assets[root].terrainGroup);}
static EdModel edTerrainPart(const EdModel *m,const std::string &key){
    EdModel part={};int root=edTerrainNamed(m,key);if(root<0)return part;
    for(int a=0;a<m->assetCount;a++)if(edTerrainMember(m,a,root)){
        int id=part.assetCount++;part.assets[id]=m->assets[a];
        for(int i=0;i<m->meshCount;i++)if(m->meshes[i].asset==a){part.meshes[part.meshCount]=m->meshes[i];part.meshes[part.meshCount++].asset=id;}
        for(int i=0;i<m->objectCount;i++)if(!strcmp(m->objects[i].templateName,m->assets[a].name))part.objects[part.objectCount++]=m->objects[i];
        for(int i=0;i<m->templateCount;i++)if(!strcmp(m->templates[i].name,m->assets[a].name))part.templates[part.templateCount++]=m->templates[i];
    }return part;
}
static bool edTerrainSame(const EdModel *a,const EdModel *b,const std::string &key){if(edSurfaceSessionKey(key))return edSurfaceSame(a,b,key);EdModel x=edTerrainPart(a,key),y=edTerrainPart(b,key);return edModelsEqual(&x,&y)!=0;}
// Only replace the leased terrain's mesh/paint data. Other edits and indices
// belong to the current authoritative document, not the editor's old snapshot.
static bool edTerrainOverlay(EdModel *out,const EdModel *from,const std::string &key){
    if(edSurfaceSessionKey(key))return edSurfaceOverlay(out,from,key);
    int root=edTerrainNamed(from,key);if(root<0)return false;
    for(int a=0;a<from->assetCount;a++)if(edTerrainMember(from,a,root)){
        int target=edTerrainNamed(out,from->assets[a].name);if(target<0)return false;
        std::vector<int> dst,src;for(int i=0;i<out->meshCount;i++)if(out->meshes[i].asset==target)dst.push_back(i);
        for(int i=0;i<from->meshCount;i++)if(from->meshes[i].asset==a)src.push_back(i);
        if(src.size()!=dst.size())return false;
        out->assets[target]=from->assets[a];
        for(size_t i=0;i<src.size();i++){out->meshes[dst[i]]=from->meshes[src[i]];out->meshes[dst[i]].asset=target;}
    }return true;
}
#endif
