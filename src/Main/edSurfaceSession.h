#ifndef ED_SURFACE_SESSION_H
#define ED_SURFACE_SESSION_H
#include "edWire.h"
#include <string>
#include <cmath>
#include <cstdio>
// Face identity excludes texture data and document indices, so painting and
// unrelated sector insertion/removal do not change a lease's identity.
static std::string edSurfaceKey(const EdFace &face){
    uint64_t hash=1469598103934665603ull;
    auto add=[&](uint64_t n){for(int b=0;b<8;b++){hash^=(n>>(b*8))&255;hash*=1099511628211ull;}};
    add(face.count);for(int v=0;v<face.count;v++){EdVec p=face.v[v].p;add((uint64_t)llround(p.x*1000000));add((uint64_t)llround(p.y*1000000));add((uint64_t)llround(p.z*1000000));}
    char key[32];snprintf(key,sizeof(key),"surface_%016llx",(unsigned long long)hash);return key;
}
static bool edSurfaceSessionKey(const std::string &key){return key.compare(0,8,"surface_")==0;}
static bool edSurfaceFind(const EdModel *m,const std::string &key,int *cell,int *face){
    bool found=false;for(int c=0;c<m->count;c++)for(int f=0;f<m->c[c].count;f++)if(edSurfaceKey(m->c[c].f[f])==key){if(found)return false;*cell=c;*face=f;found=true;}return found;
}
static bool edSurfaceExists(const EdModel *m,const std::string &key){int c,f;return edSurfaceFind(m,key,&c,&f);}
static bool edSurfaceSame(const EdModel *a,const EdModel *b,const std::string &key){
    int ac,af,bc,bf;if(!edSurfaceFind(a,key,&ac,&af)||!edSurfaceFind(b,key,&bc,&bf))return false;
    // Normalize baked material indices; another face's painting can compact them.
    auto x=std::make_unique<EdModel>(),y=std::make_unique<EdModel>();
    auto part=[](EdModel *p,const EdModel *m,const EdFace &face){p->count=1;p->c[0].count=1;p->c[0].f[0]=face;p->c[0].f[0].neighbor=p->c[0].f[0].mirror=-1;if(face.decalId){p->decalCount=1;p->decals[0]=m->decals[face.decalId-1];p->c[0].f[0].decalId=1;}};
    part(x.get(),a,a->c[ac].f[af]);part(y.get(),b,b->c[bc].f[bf]);return edModelsEqual(x.get(),y.get())!=0;
}
static bool edSurfaceOverlay(EdModel *out,const EdModel *from,const std::string &key){
    int dc,df,sc,sf;if(!edSurfaceFind(out,key,&dc,&df)||!edSurfaceFind(from,key,&sc,&sf))return false;
    EdFace &dst=out->c[dc].f[df];const EdFace &src=from->c[sc].f[sf];if(dst.count!=src.count)return false;
    for(int v=0;v<src.count;v++)if(edDot(edSub(dst.v[v].p,src.v[v].p),edSub(dst.v[v].p,src.v[v].p))>1e-16)return false;
    int decal=0;if(src.decalId){if(src.decalId<1||src.decalId>from->decalCount)return false;const EdDecalMaterial &image=from->decals[src.decalId-1];if(!image.data)return false;
        for(int i=0;i<out->decalCount;i++)if(out->decals[i].data&&*out->decals[i].data==*image.data){decal=i+1;break;}
        if(!decal){if(out->decalCount>=256)return false;out->decals[out->decalCount]=image;decal=++out->decalCount;}
    }
    // A texture lease cannot be used to submit geometry or surface flag changes.
    dst.material=src.material;dst.customUV=src.customUV;dst.uOffset=src.uOffset;dst.vOffset=src.vOffset;dst.uvRotation=src.uvRotation;dst.decalId=decal;
    for(int v=0;v<src.count;v++){dst.v[v].u=src.v[v].u;dst.v[v].v=src.v[v].v;}
    dst.attributes.uScale=src.attributes.uScale;dst.attributes.vScale=src.attributes.vScale;
    edCompactDecals(out);return true;
}
#endif
