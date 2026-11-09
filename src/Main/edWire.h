#ifndef ED_WIRE_H
#define ED_WIRE_H
#include "edGeometry.h"
#include "edNet.h"
#include <stdint.h>
/* Versioned, little-endian field encoding. Only populated records go on wire;
   no compiler padding, pointers, unused cells or native struct layout. */
#define ED_WIRE_VERSION 17u
typedef struct {unsigned char *bytes;size_t size,at;int read,ok,legacyUV;} EdWire;
static void edWireBytes(EdWire *w,void *value,size_t size){if(!w->ok||w->at>w->size||size>w->size-w->at){w->ok=0;return;}if(w->read)edCopy(value,w->bytes+w->at,size);else if(w->bytes)edCopy(w->bytes+w->at,value,size);w->at+=size;}
// Added: terminate decoded fixed-width strings, including individual intro rows
static void edWireText(EdWire *w,void *value,size_t size,size_t stride=0){edWireBytes(w,value,size);if(!stride)stride=size;if(w->read&&w->ok&&stride)for(size_t i=stride;i<=size;i+=stride)((char*)value)[i-1]=0;}
static void edWireNumber(EdWire *w,void *value,int size){unsigned char bytes[8];uint64_t number=0;if(!w->read){if(size==4){uint32_t n;edCopy(&n,value,4);number=n;}else edCopy(&number,value,size);}for(int i=0;i<size;i++)bytes[i]=(unsigned char)(number>>(i*8));edWireBytes(w,bytes,size);if(w->read&&w->ok){number=0;for(int i=0;i<size;i++)number|=(uint64_t)bytes[i]<<(i*8);if(size==4){uint32_t n=(uint32_t)number;edCopy(value,&n,4);}else edCopy(value,&number,size);}}
static void edWireCount(EdWire *w,int *value,int limit){edWireNumber(w,value,4);if(*value<0||*value>limit)w->ok=0;}
static void edWireVec(EdWire *w,EdVec *v){edWireNumber(w,&v->x,8);edWireNumber(w,&v->y,8);edWireNumber(w,&v->z,8);}
static void edWireThing(EdWire *w,EdThingAttributes *a){edWireCount(w,&a->frameCount,ED_FRAMES);edWireNumber(w,&a->hasFlags,4);edWireNumber(w,&a->pathOverride,4);edWireNumber(w,&a->flags,4);for(int i=0;w->ok&&i<a->frameCount;i++){edWireVec(w,&a->frames[i].position);edWireVec(w,&a->frames[i].rotation);}edWireNumber(w,&a->aiFrames,4);edWireNumber(w,&a->hasVelocity,4);edWireVec(w,&a->velocity);edWireText(w,a->aiClass,32);}
static void edWireCell(EdWire *w,EdCell *c){edWireNumber(w,&c->ambientLight,8);edWireNumber(w,&c->extraLight,8);edWireCount(w,&c->count,ED_FACES);for(int j=0;w->ok&&j<c->count;j++){EdFace *p=&c->f[j];edWireCount(w,&p->count,ED_VERTS);
    if(!w->legacyUV)edWireNumber(w,&p->customUV,4);edWireNumber(w,&p->material,sizeof(p->material));
    edWireNumber(w,&p->neighbor,sizeof(p->neighbor));
    edWireNumber(w,&p->mirror,sizeof(p->mirror));
    edWireNumber(w,&p->uOffset,sizeof(p->uOffset));
    edWireNumber(w,&p->vOffset,sizeof(p->vOffset));
    edWireNumber(w,&p->hasAttributes,sizeof(p->hasAttributes));
    edWireNumber(w,&p->attributes.surfaceFlags,sizeof(p->attributes.surfaceFlags));
    edWireNumber(w,&p->attributes.faceFlags,sizeof(p->attributes.faceFlags));
    edWireNumber(w,&p->attributes.adjoinFlags,sizeof(p->attributes.adjoinFlags));
    edWireNumber(w,&p->attributes.geometryMode,sizeof(p->attributes.geometryMode));
    edWireNumber(w,&p->attributes.textureMode,sizeof(p->attributes.textureMode));
    edWireNumber(w,&p->attributes.lightingMode,sizeof(p->attributes.lightingMode));
    edWireNumber(w,&p->attributes.portal,sizeof(p->attributes.portal));
    edWireNumber(w,&p->attributes.uScale,sizeof(p->attributes.uScale));
    edWireNumber(w,&p->attributes.vScale,sizeof(p->attributes.vScale));
    edWireNumber(w,&p->attributes.extraLight,sizeof(p->attributes.extraLight));
    edWireNumber(w,&p->attributes.adjoinDistance,sizeof(p->attributes.adjoinDistance));
    for(int v=0;w->ok&&v<p->count;v++){edWireVec(w,&p->v[v].p);edWireNumber(w,&p->v[v].u,8);edWireNumber(w,&p->v[v].v,8);}}
}
static void edWireAssetsLegacy(EdWire *w,EdModel *m){
    edWireCount(w,&m->assetCount,ED_ASSETS);edWireCount(w,&m->meshCount,ED_MESHES);
    for(int i=0;w->ok&&i<m->assetCount;i++){edWireText(w,m->assets[i].name,32);edWireVec(w,&m->assets[i].origin);}
    for(int i=0;w->ok&&i<m->meshCount;i++){edWireNumber(w,&m->meshes[i].asset,4);edWireCell(w,&m->meshes[i].geometry);}
}
static void edWireAssets(EdWire *w,EdModel *m){edWireAssetsLegacy(w,m);for(int a=0;w->ok&&a<m->assetCount;a++)edWireNumber(w,&m->assets[a].saved,4);}
static void edWireMeshRotations(EdWire *w,EdModel *m){for(int i=0;w->ok&&i<m->meshCount;i++)for(int f=0;w->ok&&f<m->meshes[i].geometry.count;f++)edWireNumber(w,&m->meshes[i].geometry.f[f].uvRotation,8);}
static void edWireTerrain(EdWire *w,EdModel *m){
    for(int a=0;w->ok&&a<m->assetCount;a++){edWireNumber(w,&m->assets[a].terrain,4);edWireVec(w,&m->assets[a].terrainNormal);}
    for(int i=0;w->ok&&i<m->meshCount;i++){int a=m->meshes[i].asset;if(a<0||a>=m->assetCount){w->ok=0;break;}if(m->assets[a].terrain)for(int f=0;w->ok&&f<m->meshes[i].geometry.count;f++)for(int v=0;v<m->meshes[i].geometry.f[f].count;v++)edWireBytes(w,&m->meshes[i].terrainPinned[f*3+v],1);}
}
static void edWireTerrainGroups(EdWire *w,EdModel *m){
    for(int a=0;w->ok&&a<m->assetCount;a++)edWireNumber(w,&m->assets[a].terrainGroup,4);
    for(int i=0;w->ok&&i<m->meshCount;i++){EdMesh *mesh=&m->meshes[i];int n=(int)mesh->terrainNormals.size();edWireCount(w,&n,ED_FACES*3);if(w->read)mesh->terrainNormals.resize(n);for(int j=0;w->ok&&j<n;j++)edWireVec(w,&mesh->terrainNormals[j]);}
}
static void edWireTemplates(EdWire *w,EdModel *m){
    edWireCount(w,&m->templateCount,ED_CUSTOM_TEMPLATES);
    for(int i=0;w->ok&&i<m->templateCount;i++){EdTemplate *t=&m->templates[i];edWireText(w,t->name,64);edWireText(w,t->parent,64);edWireCount(w,&t->count,ED_TEMPLATE_PARAMS);
        for(int j=0;w->ok&&j<t->count;j++){edWireText(w,t->params[j].key,32);edWireText(w,t->params[j].value,128);}}
}
static void edWireModel(EdWire *w,EdModel *m){
    edWireCount(w,&m->count,ED_CELLS);
    edWireVec(w,&m->spawn);
    edWireCount(w,&m->spawnCount,ED_SPAWNS);
    for(int i=0;w->ok&&i<m->spawnCount;i++){edWireVec(w,&m->spawns[i].position);edWireVec(w,&m->spawns[i].rotation);edWireThing(w,&m->spawns[i].attributes);}
    edWireText(w,m->header.title,sizeof(m->header.title));
    edWireText(w,m->header.colormap,sizeof(m->header.colormap));
    edWireText(w,m->header.intro,sizeof(m->header.intro),160);
    edWireNumber(w,&m->header.gravity,sizeof(m->header.gravity));
    edWireNumber(w,&m->header.ceilingSkyZ,sizeof(m->header.ceilingSkyZ));
    edWireNumber(w,&m->header.horizonDistance,sizeof(m->header.horizonDistance));
    edWireNumber(w,&m->header.horizonPixels,sizeof(m->header.horizonPixels));
    edWireNumber(w,&m->header.perspective,sizeof(m->header.perspective));
    edWireNumber(w,&m->header.gouraud,sizeof(m->header.gouraud));
    for(int i=0;i<2;i++)edWireNumber(w,&m->header.horizonOffset[i],8);
    for(int i=0;i<2;i++)edWireNumber(w,&m->header.ceilingOffset[i],8);
    for(int i=0;i<4;i++)edWireNumber(w,&m->header.mip[i],8);
    for(int i=0;i<4;i++)edWireNumber(w,&m->header.lod[i],8);
    edWireCount(w,&m->objectCount,ED_OBJECTS);
    for(int i=0;w->ok&&i<m->objectCount;i++){EdObject *a=&m->objects[i];edWireText(w,a->templateName,64);edWireVec(w,&a->position);edWireVec(w,&a->rotation);edWireThing(w,&a->attributes);}
    edWireCount(w,&m->lightCount,ED_LIGHTS);
    for(int i=0;w->ok&&i<m->lightCount;i++){EdLight *a=&m->lights[i];edWireNumber(w,&a->type,4);edWireVec(w,&a->position);edWireNumber(w,&a->intensity,8);edWireNumber(w,&a->range,8);}
    edWireText(w,m->episode.title,sizeof(m->episode.title));
    edWireNumber(w,&m->episode.type,sizeof(m->episode.type));
    edWireCount(w,&m->episode.count,ED_EPISODE_ROWS);
    edWireNumber(w,&m->episode.current,sizeof(m->episode.current));
    for(int i=0;w->ok&&i<m->episode.count;i++){EdEpisodeRow *r=&m->episode.rows[i];
    edWireNumber(w,&r->line,sizeof(r->line));
    edWireNumber(w,&r->cd,sizeof(r->cd));
    edWireNumber(w,&r->level,sizeof(r->level));
    edWireNumber(w,&r->cutscene,sizeof(r->cutscene));
    edWireNumber(w,&r->lightPower,sizeof(r->lightPower));
    edWireNumber(w,&r->darkPower,sizeof(r->darkPower));
    edWireNumber(w,&r->gotoA,sizeof(r->gotoA));
    edWireNumber(w,&r->gotoB,sizeof(r->gotoB));
    edWireText(w,r->file,sizeof(r->file));
    edWireText(w,r->title,sizeof(r->title));
    edWireText(w,r->intro,sizeof(r->intro),160);
    }
    edWireCount(w,&m->cogCount,ED_COGS);
    for(int i=0;w->ok&&i<m->cogCount;i++){EdPlacedCog *c=&m->cogs[i];edWireText(w,c->file,32);edWireCount(w,&c->count,ED_COG_PARAMS);for(int j=0;w->ok&&j<c->count;j++){EdCogParam *p=&c->params[j];edWireText(w,p->name,32);edWireText(w,p->value,32);edWireNumber(w,&p->type,4);edWireNumber(w,&p->cell,4);edWireNumber(w,&p->face,4);}}
    edWireAssets(w,m);edWireTemplates(w,m);edWireMeshRotations(w,m);edWireTerrain(w,m);edWireTerrainGroups(w,m);
    for(int s=0;w->ok&&s<m->count;s++)edWireCell(w,&m->c[s]);
    for(int s=0;w->ok&&s<m->count;s++){EdCell *c=&m->c[s];edWireNumber(w,&c->flags,4);edWireVec(w,&c->tint);edWireVec(w,&c->thrust);edWireText(w,c->sound,32);edWireNumber(w,&c->soundVolume,8);edWireNumber(w,&c->tolerance,8);edWireText(w,c->colormap,128);edWireText(w,c->layer,64);
        for(int f=0;w->ok&&f<c->count;f++)edWireNumber(w,&c->f[f].uvRotation,8);
        for(int f=0;w->ok&&f<c->count;f++)for(int v=0;w->ok&&v<c->f[f].count;v++)edWireNumber(w,&c->f[f].v[v].light,8);
    }
    edWireCount(w,&m->splineCount,ED_SPLINES);
    for(int i=0;w->ok&&i<m->splineCount;i++){EdSpline *s=&m->splines[i];edWireNumber(w,&s->id,4);edWireNumber(w,&s->closed,4);edWireNumber(w,&s->mirrorAxes,4);edWireCount(w,&s->count,ED_SPLINE_POINTS);edWireNumber(w,&s->sides,4);edWireNumber(w,&s->segments,4);edWireNumber(w,&s->material,4);edWireNumber(w,&s->width,8);edWireNumber(w,&s->height,8);edWireNumber(w,&s->geometryHash,4);for(int p=0;w->ok&&p<s->count;p++){edWireVec(w,&s->points[p].position);edWireVec(w,&s->points[p].handle);}}
    for(int i=0;w->ok&&i<m->count;i++)edWireNumber(w,&m->c[i].splineId,4);
}
static unsigned char *edWireEncode(const EdModel *model,uint32_t *size){
    EdWire measure={NULL,ED_NET_LIMIT/2,0,0,1};edWireModel(&measure,(EdModel*)model);if(!measure.ok)return NULL;
    unsigned char *bytes=edAlloc(measure.at);if(!bytes)return NULL;
    EdWire w={bytes,measure.at,0,0,1};edWireModel(&w,(EdModel*)model);if(!w.ok){edFree(bytes);return NULL;}*size=(uint32_t)w.at;return bytes;
}
static int edWireDecode(EdModel *model,const unsigned char *bytes,uint32_t size){edZero(model,0,sizeof(*model));EdWire w={(unsigned char*)bytes,size,0,1,1};edWireModel(&w,model);return w.ok&&w.at==size;}
#endif
