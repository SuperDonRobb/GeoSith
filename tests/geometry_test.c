#include "edGeometry.h"
#include "edLevelIO.h"
#include "edInput.h"
#include "edMap.h"
#include "edMirror.h"
#include "edWire.h"
#include "edTerrainSession.h"
#include "General/stdNetReader.h"
#include <stdio.h>
#include <chrono>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "placed_fixture.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
static EdModel *m,*backup;
static void dynamicStorageTests(void){
    edDefault(m);CHECK(sizeof(EdModel)<1024*1024&&m->c.size()==1&&m->c[0].f.size()==6);
    EdCell room=m->c[0];m->count=1536;
    for(int i=1;i<m->count;i++){m->c[i]=room;for(int f=0;f<6;f++)for(int v=0;v<4;v++)m->c[i].f[f].v[v].p.x+=i*4;}
    CHECK(edValidate(m));edCopyModel(backup,m);m->c[1535].f[0].material=3;CHECK(backup->c[1535].f[0].material!=3);
    EdBuffer<unsigned char> selected;selected[1535]=1;CHECK(edMapDelete(m,backup,selected)&&m->count==1535);edCopyModel(m,backup);
    CHECK(edWriteGob(m,"test-growing-storage.gob","Growing storage",0,"SECTION: SOUNDS\nWorld sounds 0\nend\n"));remove("test-growing-storage.gob");
    edDefault(m);EdFace wall=m->c[0].f[4];const int strips=256;m->c[0].count=6+strips-1;
    for(int i=0;i<strips;i++){EdFace& face=m->c[0].f[i?5+i:4];face=wall;for(int v=0;v<4;v++)face.v[v].p.x=-1.0+2.0*(i+(wall.v[v].p.x>0))/strips;}
    CHECK(edValidate(m)&&m->c[0].count>192);edCopyModel(backup,m);CHECK(edCleanupSector(m,0)&&m->c[0].count==6&&edValidate(m));CHECK(backup->c[0].count==261);
    edDefault(m);m->objectCount=1025;for(int i=0;i<m->objectCount;i++){strcpy(m->objects[i].templateName,"walkstruct");m->objects[i].position=edV(0,0,.5);}
    m->cogCount=257;for(int i=0;i<m->cogCount;i++)strcpy(m->cogs[i].file,"test.cog");CHECK(edValidate(m));
    uint32_t size=0;unsigned char *wire=edWireEncode(m,&size);CHECK(wire&&edWireDecode(backup,wire,size)&&backup->objectCount==1025&&backup->cogCount==257&&edModelsEqual(m,backup));edFree(wire);
    edDefault(m);m->count=11000;for(int i=1;i<m->count;i++)m->c[i]=room;
    wire=edWireEncode(m,&size);CHECK(wire&&size>16*1024*1024);uint32_t packedSize=0;unsigned char *packed=edNetPack(wire,size,&packedSize),*restored=packed?edNetUnpack(packed,packedSize,size):NULL;
    CHECK(restored&&edWireDecode(backup,restored,size)&&backup->count==11000&&edCellsEqual(&m->c[10999],&backup->c[10999]));edFree(wire);edFree(packed);edFree(restored);
    printf("PASS: growing storage: 1536 sectors saved, 261 faces cleaned, 1025 objects/257 COGs, %u-byte snapshot round trip\n",size);
    edDefault(m);edDefault(backup);
}
static void stockMetadataTests(void){
    edDefault(m);m->c[0].f[0].v[0].p.z+=.00005;CHECK(!edValidate(m));
    m->c[0].tolerance=ED_IMPORT_EPS;CHECK(edValidate(m));
    m->c[0].flags=2;m->c[0].tint=edV(.1,.2,.3);strcpy(m->c[0].sound,"water.wav");m->c[0].soundVolume=.4;m->c[0].f[0].v[0].light=.375;
    m->objectCount=1;strcpy(m->objects[0].templateName,"walkstruct");m->objects[0].position=edV(0,0,.5);
    EdThingAttributes *a=&m->objects[0].attributes;strcpy(a->aiClass,"gonkroam.ai");a->aiFrames=1;a->frameCount=2;a->frames[0].position=edV(.1,.2,.3);a->frames[1].position=edV(.4,.5,.6);a->hasVelocity=1;a->velocity=edV(.01,.02,.03);
    edRotateFaceTexture(&m->meshes[0].geometry.f[0],23,edV(0,0,0));
    uint32_t size;unsigned char *wire=edWireEncode(m,&size);CHECK(wire&&edWireDecode(backup,wire,size)&&edValidate(backup));edFree(wire);
    CHECK(!edCompare(&m->objects[0].attributes,&backup->objects[0].attributes,sizeof(*a)));
    CHECK(backup->c[0].tolerance==ED_IMPORT_EPS&&backup->c[0].flags==2&&backup->c[0].soundVolume==.4&&!strcmp(backup->c[0].sound,"water.wav")&&backup->c[0].f[0].v[0].light==.375);
    backup->c[0].tolerance=ED_IMPORT_EPS*2;CHECK(!edValidate(backup));backup->c[0].tolerance=0;CHECK(!edValidate(backup));
    *backup=*m;backup->c[0].f[0].v[0].light=NAN;CHECK(!edValidate(backup));edDefault(m);
    puts("PASS: stock rounding is bounded; baked lighting, sector metadata and AI paths survive wire encoding");
}
static void checkClosedMesh(const EdCell *cell){
    for(int f=0;f<cell->count;f++)for(int v=0;v<cell->f[f].count;v++){
        EdVec a=cell->f[f].v[v].p,b=cell->f[f].v[(v+1)%cell->f[f].count].p;int reverse=0;
        for(int g=0;g<cell->count;g++)for(int w=0;w<cell->f[g].count;w++)if(g!=f){EdVec c=cell->f[g].v[w].p,d=cell->f[g].v[(w+1)%cell->f[g].count].p;if(edDot(edSub(a,d),edSub(a,d))<ED_EPS*ED_EPS&&edDot(edSub(b,c),edSub(b,c))<ED_EPS*ED_EPS)reverse++;}
        CHECK(reverse==1);
    }
}
static EdHit meshVertexAt(const EdCell *cell,EdVec point){
    for(int f=0;f<cell->count;f++)for(int v=0;v<cell->f[f].count;v++){EdVec d=edSub(cell->f[f].v[v].p,point);if(edDot(d,d)<ED_EPS*ED_EPS)return EdHit{0,f,v,-1};}return EdHit{-1,-1,-1,-1};
}
static void meshTopologyTests(void){
    for(int sloped=0;sloped<2;sloped++){
        edDefault(m);m->assetCount=1;strcpy(m->assets[0].name,"topology");CHECK(edMeshPrimitive(m,0,0,edV(0,0,.5),0));
        if(sloped)for(int f=0;f<6;f++)for(int v=0;v<4;v++)m->meshes[0].geometry.f[f].v[v].p.y+=.3*m->meshes[0].geometry.f[f].v[v].p.x;
        EdHit face;CHECK(edMeshRay(m,0,edV(0,-1,.5),edV(0,1,0),0,&face));CHECK(edMeshCleave(m,face,edV(0,-.25,.25),edV(0,-.25,.75)));checkClosedMesh(&m->meshes[0].geometry);
        EdSelection pick={0};pick.count=1;pick.hits[0]=meshVertexAt(&m->meshes[0].geometry,edV(0,-.25,.75));CHECK(pick.hits[0].cell==0);edCopyModel(backup,m);
        CHECK(edMeshMoveConstrained(m,backup,&pick,edV(.12,.02,.04),0));checkClosedMesh(&m->meshes[0].geometry);
        CHECK(meshVertexAt(&m->meshes[0].geometry,edV(0,-.25,.75)).cell<0);
        /* Clamp to the connected corner, including an off-grid diagonal endpoint. */
        CHECK(edMeshMoveConstrained(m,backup,&pick,edV(1,.3*sloped,.04),0));checkClosedMesh(&m->meshes[0].geometry);
        CHECK(meshVertexAt(&m->meshes[0].geometry,edV(.25,-.25+.075*sloped,.75)).cell==0);
        pick.hits[0]=meshVertexAt(&m->meshes[0].geometry,edV(0,-.25,.25));edCopyModel(backup,m);CHECK(edMeshMoveConstrained(m,backup,&pick,edV(1,.3*sloped,0),0));
        CHECK(m->meshes[0].geometry.count==6);checkClosedMesh(&m->meshes[0].geometry);CHECK(edValidate(m));
    }
    /* Free movement can leave the connected edges. */
    edDefault(m);m->assetCount=1;strcpy(m->assets[0].name,"free");CHECK(edMeshPrimitive(m,0,0,edV(0,0,.5),0));edCopyModel(backup,m);
    EdSelection pick={0};pick.count=1;pick.hits[0]=meshVertexAt(&m->meshes[0].geometry,edV(.25,-.25,.75));CHECK(edMeshMoveConstrained(m,backup,&pick,edV(ED_GRID,-ED_GRID,ED_GRID),1));checkClosedMesh(&m->meshes[0].geometry);
    edDefault(m);m->assetCount=1;strcpy(m->assets[0].name,"free_weld");CHECK(edMeshPrimitive(m,0,0,edV(0,0,.5),0));EdHit face;
    CHECK(edMeshRay(m,0,edV(0,-1,.5),edV(0,1,0),0,&face));CHECK(edMeshCleave(m,face,edV(0,-.25,.25),edV(0,-.25,.75)));
    pick.hits[0]=meshVertexAt(&m->meshes[0].geometry,edV(0,-.25,.75));edCopyModel(backup,m);CHECK(edMeshMoveConstrained(m,backup,&pick,edV(.25,0,0),1));checkClosedMesh(&m->meshes[0].geometry);
    edDefault(m);m->assetCount=1;strcpy(m->assets[0].name,"limit");CHECK(edMeshPrimitiveDetail(m,0,2,edV(0,0,.5),0,2));
    double angle=3.141592653589793/24;EdVec direction=edV(-cos(angle),-sin(angle),0);
    CHECK(edMeshRay(m,0,edV(cos(angle),sin(angle),.5),direction,0,&face));EdCell before=m->meshes[0].geometry;
    CHECK(!edMeshCleave(m,face,edV(face.point.x,face.point.y,.25),edV(face.point.x,face.point.y,.75)));CHECK(edCellsEqual(&before,&m->meshes[0].geometry));
    puts("PASS: cleaved mesh edges stay connected; constrained and free vertex moves, corner welds and collapsed-face removal stay watertight");
}
static void meshCleaveSnapTests(void){
    for(int sloped=0;sloped<2;sloped++){
        edDefault(m);m->assetCount=1;strcpy(m->assets[0].name,"snap_test");CHECK(edMeshPrimitive(m,0,0,edV(0,0,.5),0));
        if(sloped)for(int f=0;f<m->meshes[0].geometry.count;f++)for(int v=0;v<m->meshes[0].geometry.f[f].count;v++){EdVec *p=&m->meshes[0].geometry.f[f].v[v].p;p->y+=.5*p->x;}
        EdHit hit;CHECK(edMeshRay(m,0,edV(0,-1,.5),edV(0,1,0),0,&hit));EdFace face=m->meshes[0].geometry.f[hit.face];
        double grid=ED_GRID*.5;EdVec a=edV(grid*.7,-.25,.26),b=edV(grid*.8,-.25,.74);
        EdVec snapped=edSnapMeshFacePoint(&face,a),normal=edNormal(&face);
        CHECK(fabs(snapped.x-grid)<ED_EPS&&fabs(snapped.z-.25)<ED_EPS&&fabs(edDot(normal,edSub(snapped,face.v[0].p)))<ED_EPS);
        CHECK(!edMeshCleave(m,hit,a,edAdd(a,edV(.001,0,.001)))&&m->meshes[0].geometry.count==6);
        CHECK(edMeshCleave(m,hit,a,b)&&m->meshes[0].geometry.count==7&&edValidate(m));
        int cutVertices=0;int faces[]={hit.face,6};for(int f:faces)for(int v=0;v<m->meshes[0].geometry.f[f].count;v++){EdVec p=m->meshes[0].geometry.f[f].v[v].p;CHECK(fabs(edDot(normal,edSub(p,face.v[0].p)))<ED_EPS);if(fabs(p.x-grid)<ED_EPS)cutVertices++;}
        CHECK(cutVertices==4);
    }
    puts("PASS: 3DO cleaves snap to half-world grid on flat and angled faces; collapsed cuts are rejected");
}
static void meshStationaryGrabTests(void){
    for(int primitive=0;primitive<4;primitive++){
        edDefault(m);m->assetCount=1;strcpy(m->assets[0].name,"grab");
        CHECK(edMeshPrimitiveTransformedDetail(m,0,primitive,edV(0,0,.5),0,.37,15,2));
        edCopyModel(backup,m);
        for(int kind=0;kind<3;kind++)for(int free=0;free<2;free++){
            EdSelection pick={0};pick.count=1;pick.hits[0]=EdHit{0,0,kind==1?0:-1,kind==2?0:-1};
            CHECK(edMeshMoveConstrained(m,backup,&pick,edV(0,0,0),free)&&edModelsEqual(m,backup));
            CHECK(edMeshMoveConstrained(m,backup,&pick,edV(ED_GRID*.001,0,0),free)&&edModelsEqual(m,backup));
        }
        EdSelection face={0};face.count=1;face.hits[0]=EdHit{0,0,-1,-1};
        CHECK(edMeshMoveConstrained(m,backup,&face,edV(ED_GRID*.2,0,0),0)&&edModelsEqual(m,backup));
        CHECK(edMeshExtrude(m,face.hits[0]));
        const EdFace& before=backup->meshes[0].geometry.f[0];const EdFace& after=m->meshes[0].geometry.f[0];
        EdVec shift=edScale(edNormal(&before),ED_GRID);
        for(int v=0;v<before.count;v++){EdVec error=edSub(after.v[v].p,edAdd(before.v[v].p,shift));CHECK(edDot(error,error)<ED_EPS*ED_EPS);}
    }
    puts("PASS: stationary/sub-grid mesh grabs preserve primitives and extrude the intended face");
}
static void meshTests(void){
    meshStationaryGrabTests();
    meshTopologyTests();
    meshCleaveSnapTests();
    edDefault(m);m->assetCount=1;strcpy(m->assets[0].name,"mesh_test");m->assets[0].origin=edV(0,0,.5);m->assets[0].saved=1;
    for(int i=0;i<4;i++)CHECK(edMeshPrimitive(m,0,i,edV(i,0,.5),0));CHECK(edValidate(m));CHECK(m->meshCount==4);
    CHECK(m->meshes[0].geometry.count==6&&m->meshes[1].geometry.count==32&&m->meshes[2].geometry.count==14&&m->meshes[3].geometry.count==5);
    for(int i=0;i<4;i++)for(int f=0;f<m->meshes[i].geometry.count;f++){const EdFace *face=&m->meshes[i].geometry.f[f];CHECK(edDot(edNormal(face),edSub(face->v[0].p,edV(i,0,.5)))>0);}
    EdHit hit;CHECK(edMeshRay(m,0,edV(0,-1,.5),edV(0,1,0),0,&hit));CHECK(hit.cell==0);
    EdFace before=m->meshes[0].geometry.f[hit.face];CHECK(edMeshExtrude(m,hit));CHECK(m->meshes[0].geometry.count==10);CHECK(fabs(edDot(edNormal(&before),edSub(m->meshes[0].geometry.f[hit.face].v[0].p,before.v[0].p))-ED_GRID)<1e-8);
    CHECK(edMeshCleave(m,hit,edV(0,-.3125,.25),edV(0,-.3125,.75)));CHECK(m->meshes[0].geometry.count==11);
    *backup=*m;EdSelection selected={0};selected.count=1;selected.hits[0]=EdHit{0,0,0,-1};EdVec vertex=m->meshes[0].geometry.f[0].v[0].p;
    CHECK(edMeshMove(m,backup,&selected,edV(-ED_GRID,0,0)));CHECK(edValidate(m));CHECK(!edCompare(m->c,backup->c,sizeof(m->c)));CHECK(fabs(m->meshes[0].geometry.f[0].v[0].p.x-vertex.x+ED_GRID)<1e-8);
    m->templateCount=1;EdTemplate *t=&m->templates[0];strcpy(t->name,"custom_mesh");strcpy(t->parent,"_decor");t->count=2;strcpy(t->params[0].key,"model3d");strcpy(t->params[0].value,"mesh_test.3do");strcpy(t->params[1].key,"size");strcpy(t->params[1].value,"0.25");CHECK(edTemplatesValidate(m));
    strcpy(t->params[1].value,"0.25 type=actor");CHECK(!edTemplatesValidate(m));strcpy(t->params[1].value,"0.25");strcpy(t->parent,"custom_mesh");CHECK(!edTemplatesValidate(m));strcpy(t->parent,"_decor");
    unsigned char *legacy=edAlloc(ED_NET_LIMIT/2);CHECK(legacy);edCopy(legacy,"G3DO",4);edNetStore32(legacy+4,1);EdWire old={legacy,ED_NET_LIMIT/2,8,0,1};edWireAssetsLegacy(&old,m);edDefault(backup);CHECK(old.ok&&edReadMeshData(backup,legacy,(unsigned int)old.at)&&backup->assets[0].saved==1&&backup->templateCount==0);edFree(legacy);
    uint32_t size;unsigned char *wire=edWireEncode(m,&size);CHECK(wire&&edWireDecode(backup,wire,size)&&edValidate(backup));CHECK(m->templateCount==backup->templateCount&&!edCompare(m->templates,backup->templates,m->templateCount*sizeof(EdTemplate)));CHECK(m->assets[0].saved==backup->assets[0].saved);CHECK(m->meshCount==backup->meshCount);for(int mi=0;mi<m->meshCount;mi++){CHECK(m->meshes[mi].geometry.count==backup->meshes[mi].geometry.count);for(int fi=0;fi<m->meshes[mi].geometry.count;fi++){const EdFace *x=&m->meshes[mi].geometry.f[fi],*y=&backup->meshes[mi].geometry.f[fi];CHECK(x->count==y->count&&x->material==y->material&&x->uvRotation==y->uvRotation&&!edCompare(x->v,y->v,x->count*sizeof(EdVertex)));}}edFree(wire);
    const char *resources="SECTION: MATERIALS\nWorld materials 1\n0: dflt.mat 1 1\nend\nSECTION: TEMPLATES\nWorld templates 1\n_decor none type=cog\nend\n";
    CHECK(edWriteGob(m,"test-model-builder.gob","Mesh test",0,resources));FILE *f=fopen("test-model-builder.gob","rb");CHECK(f);fseek(f,0,SEEK_END);long bytes=ftell(f);rewind(f);unsigned char *data=edAlloc(bytes);CHECK(data&&fread(data,1,bytes,f)==bytes);fclose(f);
    uint32_t dir=edNetLoad32(data+8),count=edNetLoad32(data+dir);int models=0,metadata=0;
    for(uint32_t i=0;i<count;i++){unsigned char *entry=data+dir+4+i*136;uint32_t offset=edNetLoad32(entry),size=edNetLoad32(entry+4);CHECK(offset+size<=(uint32_t)bytes);
        if(!strcmp((char*)entry+8,"3do\\mesh_test.3do")){CHECK(!edCompare(data+offset,"SECTION: HEADER",15));models++;}
        if(!strcmp((char*)entry+8,"misc\\geosith_models.bin")){edDefault(backup);CHECK(edReadMeshData(backup,data+offset,size));CHECK(m->templateCount==backup->templateCount&&!edCompare(m->templates,backup->templates,m->templateCount*sizeof(EdTemplate)));CHECK(m->assets[0].saved==backup->assets[0].saved);CHECK(m->meshCount==backup->meshCount);for(int mi=0;mi<m->meshCount;mi++){CHECK(m->meshes[mi].geometry.count==backup->meshes[mi].geometry.count);for(int fi=0;fi<m->meshes[mi].geometry.count;fi++){const EdFace *x=&m->meshes[mi].geometry.f[fi],*y=&backup->meshes[mi].geometry.f[fi];CHECK(x->count==y->count&&x->material==y->material&&x->uvRotation==y->uvRotation&&!edCompare(x->v,y->v,x->count*sizeof(EdVertex)));}}CHECK(!edReadMeshData(backup,data+offset,size-1));metadata++;}
    }CHECK(models==1&&metadata==1);edFree(data);remove("test-model-builder.gob");
    *backup=*m;CHECK(edResolve(m));CHECK(m->assetCount==backup->assetCount&&m->meshCount==backup->meshCount&&m->templateCount==backup->templateCount&&!edCompare(m->templates,backup->templates,sizeof(m->templates)));
    for(int primitive=0;primitive<4;primitive++){
        m->meshCount=0;CHECK(edMeshPrimitiveTransformed(m,0,primitive,edV(.03,.02,.01),0,.5,15));
        for(int f=0;f<m->meshes[0].geometry.count;f++)for(int v=0;v<m->meshes[0].geometry.f[f].count;v++){
            EdVec p=m->meshes[0].geometry.f[f].v[v].p;if(primitive==0)CHECK(fabs(remainder(p.x,ED_GRID))<ED_EPS&&fabs(remainder(p.y,ED_GRID))<ED_EPS&&fabs(remainder(p.z,ED_GRID))<ED_EPS);else CHECK(fabs(remainder(p.z,ED_GRID))>ED_EPS);
        }
    }
    for(int primitive=1;primitive<=2;primitive++){int previous=0;for(int detail=0;detail<3;detail++){
        m->meshCount=0;EdVec center=edV(.03,.02,.01);CHECK(edMeshPrimitiveTransformedDetail(m,0,primitive,center,0,.5,17,detail));
        const EdCell *cell=&m->meshes[0].geometry;CHECK(cell->count>previous);previous=cell->count;
        for(int f=0;f<cell->count;f++){CHECK(cell->f[f].count<=24);for(int v=0;v<cell->f[f].count;v++){
            EdVec p=edSub(cell->f[f].v[v].p,center);if(primitive==1)CHECK(fabs(edDot(p,p)-.25*.25)<ED_EPS);else CHECK(fabs(p.x*p.x+p.y*p.y-.25*.25)<ED_EPS&&fabs(fabs(p.z)-.25)<ED_EPS);
        }}CHECK(edValidate(m));
    }}
    puts("PASS: curved primitives preserve radii at all three detail levels; only placed cubes snap");
    m->meshCount=0;CHECK(edMeshPrimitiveTransformed(m,0,0,edV(0,0,0),0,1,45));CHECK(fabs(m->meshes[0].geometry.f[0].v[0].p.z)==.5);CHECK(edMeshValidate(m));
    m->meshCount=0;CHECK(edMeshPrimitive(m,0,0,edV(.03,.02,.01),0));*backup=*m;
    selected.count=6;for(int f=0;f<6;f++)selected.hits[f]=EdHit{0,f,-1,-1};CHECK(edMeshMove(m,backup,&selected,edV(.04,0,0)));
    for(int f=0;f<m->meshes[0].geometry.count;f++)for(int v=0;v<m->meshes[0].geometry.f[f].count;v++){EdVec delta=edSub(m->meshes[0].geometry.f[f].v[v].p,backup->meshes[0].geometry.f[f].v[v].p);CHECK(fabs(delta.x-ED_GRID)<ED_EPS&&fabs(delta.y)<ED_EPS&&fabs(delta.z)<ED_EPS);}
    puts("PASS: 3DO primitives, outward normals/extrusion, cleave, vertex movement, wire and GOB metadata round trips");
}
static double totalVolume(const EdModel *model) {
    double total=0;
    for(int s=0;s<model->count;s++){double volume=0;for(int f=0;f<model->c[s].count;f++){const EdFace *p=&model->c[s].f[f];for(int i=1;i+1<p->count;i++)volume+=edDot(p->v[0].p,edCross(p->v[i].p,p->v[i+1].p))/6;}total+=fabs(volume);}return total;
}
static int portals(void) {
    int count=0;for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++){EdFace *p=&m->c[s].f[f];if(p->neighbor>=0){CHECK(p->neighbor<m->count);EdFace *q=&m->c[p->neighbor].f[p->mirror];CHECK(q->neighbor==s);CHECK(q->mirror==f);CHECK(q->count==p->count);CHECK(edDot(edNormal(p),edNormal(q))<-.999);for(int v=0;v<p->count;v++){int match=0;for(int j=0;j<q->count;j++){EdVec d=edSub(p->v[v].p,q->v[j].p);if(edDot(d,d)<1e-8)match=1;}CHECK(match);}count++;}}return count;
}
static int uniqueVertices(const EdModel *model) {
    EdVec points[512];int count=0;
    for(int s=0;s<model->count;s++)for(int f=0;f<model->c[s].count;f++)for(int v=0;v<model->c[s].f[f].count;v++){
        EdVec p=model->c[s].f[f].v[v].p;int i=0;for(;i<count;i++){EdVec d=edSub(p,points[i]);if(edDot(d,d)<ED_EPS*ED_EPS)break;}if(i==count){CHECK(count<512);points[count++]=p;}
    }return count;
}
static int readPacketTest(const unsigned char *bytes,size_t size,int aligned){intptr_t cursor=(intptr_t)bytes,end=cursor+size;jmp_buf abortRead;if(setjmp(abortRead))return 0;uint32_t first,second;stdNetRead(&cursor,end,&first,4,aligned?4:1,abortRead);stdNetRead(&cursor,end,&second,4,1,abortRead);return first==0x04030201&&second==0x08070605;}
static void wireTests(void){
    unsigned char packet[8]={1,2,3,4,5,6,7,8};CHECK(readPacketTest(packet,8,0));for(int n=0;n<8;n++)CHECK(!readPacketTest(packet,n,0));
    edDefault(m);m->c[0].extraLight=.35;m->c[0].ambientLight=.1;CHECK(edExtrudeStep(m,0,4));uint32_t size;unsigned char *wire=edWireEncode(m,&size);CHECK(wire&&size<20000);CHECK(edWireDecode(backup,wire,size));CHECK(edValidate(backup));
    CHECK(backup->c[0].extraLight==.35&&backup->c[1].extraLight==.35&&backup->c[1].ambientLight==.1);
    uint32_t againSize;unsigned char *again=edWireEncode(backup,&againSize);CHECK(again&&size==againSize&&!edCompare(wire,again,size));edFree(again);
    for(uint32_t n=0;n<size;n+=137)CHECK(!edWireDecode(backup,wire,n));
    unsigned char saved[4];edCopy(saved,wire,4);edZero(wire,255,4);CHECK(!edWireDecode(backup,wire,size));edCopy(wire,saved,4);
    CHECK(edWireDecode(backup,wire,size));backup->header.gravity=3.25;unsigned char *next=edWireEncode(backup,&againSize);CHECK(next);
    uint32_t packedSize;unsigned char *packed=edNetDelta(wire,size,next,againSize,&packedSize);CHECK(packed&&packedSize<againSize/8);unsigned char *decoded=edNetUnpack(packed,packedSize,againSize);CHECK(decoded);
    for(uint32_t i=0;i<againSize;i++)decoded[i]^=i<size?wire[i]:0;CHECK(!edCompare(decoded,next,againSize));CHECK(edWireDecode(backup,decoded,againSize));CHECK(backup->header.gravity==3.25);
    edFree(decoded);edFree(packed);edFree(next);edFree(wire);
}
static void walkingDragTests(void) {
    /* Start very close to an extrusion and walk well beyond its original
       plane, with a downward look and small ground-height changes. */
    edDefault(m);CHECK(edExtrudeStep(m,0,4));*backup=*m;
    EdVec origin=edV(0,.9,.16),direction=edV(0,1,-.15);
    EdHit hit;CHECK(edRay(m,origin,direction,&hit));CHECK(hit.cell==1&&hit.vertex<0&&hit.edge<0);
    EdVec normal=edNormal(&m->c[hit.cell].f[hit.face]);
    EdDragTrace trace;edDragTraceBegin(&trace,origin,direction,hit.point,normal,4);
    EdVec delta=edV(0,0,0);
    for(int frame=1;frame<=80;frame++) {
        EdVec eye=edAdd(origin,edV(0,frame*.025,.002*sin(frame)));
        delta=edAdd(delta,edDragTraceStep(&trace,eye,direction));
        CHECK(trace.valid);CHECK(fabs(delta.y-frame*.025)<1e-8);
        CHECK(edMove(m,backup,hit,edSurfaceDragDelta(delta,normal)));
        CHECK(edValidate(m));CHECK(portals()==2);
        CHECK(edContains(&m->c[eye.y>1?1:0],edV(0,eye.y,.16)));
    }
    CHECK(fabs(delta.y-2)<1e-8);
    /* Cursor rotation still works after crossing; no lost motion, shrinking
       lever arm or jumps when the aim briefly becomes parallel/behind. */
    EdVec eye=trace.origin;
    EdVec cursor=edDragTraceStep(&trace,eye,edV(.6,.8,-.15));
    CHECK(cursor.x>.1&&fabs(cursor.y)<1e-8);
    EdVec step=edDragTraceStep(&trace,edAdd(eye,edV(0,.1,0)),edV(1,0,0));
    CHECK(!trace.valid&&fabs(step.y-.1)<1e-8);
    eye=trace.origin;step=edDragTraceStep(&trace,edAdd(eye,edV(0,.1,0)),edV(0,-1,0));
    CHECK(!trace.valid&&fabs(step.y-.1)<1e-8);
    eye=trace.origin;step=edDragTraceStep(&trace,eye,direction);
    CHECK(trace.valid&&edDot(step,step)<1e-12);
    step=edDragTraceStep(&trace,eye,direction);CHECK(edDot(step,step)<1e-12);
    edDragTraceBegin(&trace,origin,direction,hit.point,normal,4);
    step=edDragTraceStep(&trace,origin,edV(1,.011,0));CHECK(!trace.valid&&edDot(step,step)<1e-12);
}
static void placedSectorEditTests(void){
    EdModel *source=edAlloc(sizeof(*source)),*clip=edAlloc(sizeof(*clip)),*placed=edAlloc(sizeof(*placed));CHECK(source&&clip&&placed);
    const EdVec offsets[]={{3,0,0},{2,0,0},{1.5,0,0},{1.5,.5,0},{.5,1.5,0},{.5,-.5,0}};
    for(int connected=0;connected<5;connected++)for(int overlap=0;overlap<6;overlap++){
        edDefault(source);if(connected==1)CHECK(edExtrudeStep(source,0,4));
        if(connected==2){CHECK(edCleave(source,0,4,edV(0,1,0),edV(0,1,1)));CHECK(edCleave(source,0,2,edV(0,-1,0),edV(0,-1,1)));}
        if(connected==3)CHECK(edCleaveSector(source,0,4,edV(0,1,0),edV(0,1,1)));
        if(connected==4){CHECK(edCleave(source,0,4,edV(0,1,0),edV(0,1,1)));CHECK(edCleave(source,0,4,edV(-1,1,.5),edV(1,1,.5)));CHECK(edCleave(source,0,6,edV(-1,1,.5),edV(1,1,.5)));}
        *clip=*source;clip->count=1;
        EdBuffer<unsigned char> selected;CHECK(edMapPaste(placed,source,clip,offsets[overlap],selected));*m=*placed;portals();
        for(int repeat=0;repeat<3;repeat++){CHECK(edResolve(placed));*m=*placed;portals();}
        for(int s=0;s<placed->count;s++)for(int f=0;f<placed->c[s].count;f++){
            const EdFace *face=&placed->c[s].f[f];if(face->neighbor>=0)continue;
            EdVec a=edScale(edAdd(face->v[0].p,face->v[1].p),.5),b=edScale(edAdd(face->v[2].p,face->v[3].p),.5);
            EdSelection selectedFace={0};selectedFace.count=1;selectedFace.hits[0]=EdHit{s,f,-1,-1};CHECK(edMoveSelection(m,placed,&selectedFace,edV(0,0,0)));CHECK(edExtrudeSelection(m,&selectedFace));CHECK(edValidate(m));portals();
            *m=*placed;CHECK(edExtrudeStep(m,s,f));CHECK(edValidate(m));portals();
            *m=*placed;CHECK(edCleave(m,s,f,a,b));CHECK(edValidate(m));
            *m=*placed;CHECK(edCleaveSector(m,s,f,a,b));CHECK(edValidate(m));portals();
        }
    }
    edFree(source);edFree(clip);edFree(placed);
}
static void placedFixtureTests(void){
    edZero(backup,0,sizeof(*backup));backup->count=PLACED_FIXTURE_CELLS;edCopy(backup->c,placedFixture,sizeof(placedFixture));CHECK(edValidate(backup));*m=*backup;portals();CHECK(edResolve(m));
    for(int s=0;s<backup->count;s++)for(int f=0;f<backup->c[s].count;f++){
        const EdFace *face=&backup->c[s].f[f];if(face->neighbor>=0)continue;
        *m=*backup;if(!edExtrudeStep(m,s,f)){fprintf(stderr,"Placed fixture extrusion failed: sector %d face %d, post-failure count=%d valid=%d\n",s,f,m->count,edValidate(m));CHECK(0);}CHECK(edValidate(m));portals();
    }
    *m=*backup;CHECK(edCleave(m,0,4,edV(0,1,0),edV(0,1,1)));CHECK(edValidate(m));
    *m=*backup;CHECK(edCleaveSector(m,0,4,edV(0,1,0),edV(0,1,1)));CHECK(edValidate(m));portals();
    EdModel *clip=edAlloc(sizeof(*clip));CHECK(clip);edDefault(clip);EdBuffer<unsigned char> selected;
    CHECK(edMapPaste(m,backup,clip,edV(-1.5,0,0),selected));CHECK(edValidate(m));portals();
    /* A second insertion/copy must also work after the first merge. */
    *backup=*m;CHECK(edMapPaste(m,backup,clip,edV(-2.5,1.5,0),selected));CHECK(edValidate(m));portals();edFree(clip);
}
static void checkRoomCoverage(const EdModel *model) {
    CHECK(edValidate(model));
    for(int side=0;side<6;side++)for(int a=1;a<12;a++)for(int b=1;b<12;b++){
        double u=-1+a/6.0,v=-1+b/6.0;EdVec target;
        if(side<2)target=edV(side?1:-1,u,(v+1)*.5);
        else if(side<4)target=edV(u,side==3?1:-1,(v+1)*.5);
        else target=edV(u,v,side==5?1:0);
        EdVec direction=edSub(target,edV(0,0,.5));double distance=sqrt(edDot(direction,direction));EdHit hit;
        CHECK(edRay(model,edV(0,0,.5),edScale(direction,1/distance),&hit));CHECK(fabs(hit.distance-distance)<.0001);
    }
}
static void weldCoverageTests(void) {
    /* The previous validator accepted a room with half a wall missing. */
    edDefault(m);EdFace wall=m->c[0].f[4];m->c[0].f[4].count=3;CHECK(!edValidate(m));
    for(int start=0;start<2;start++)for(int end=0;end<2;end++){
        edDefault(m);wall=m->c[0].f[4];EdVertex ring[6]={wall.v[0],wall.v[1],wall.v[1],wall.v[2],wall.v[2],wall.v[3]};
        ring[2].p=edV(.5,1,1);ring[3].p=edV(-.5,1,1);
        /* Triangles sharing an edge vertex may collapse or reverse when that
         * vertex crosses another cleave point on its way to a room corner. */
        m->c[0].count=5;m->c[0].f[4]=m->c[0].f[5];
        for(int i=0;i<6;i++){EdFace triangle=wall;triangle.count=3;triangle.v[0].p=edV(0,1,.5);triangle.v[1]=ring[i];triangle.v[2]=ring[(i+1)%6];edMapFace(&triangle);m->c[0].f[m->c[0].count++]=triangle;}
        checkRoomCoverage(m);int count=uniqueVertices(m);EdVec point=edV(start?.5:-.5,1,1),target=edV(end?1:-1,1,1);EdHit vertex={-1,-1,-1,-1};
        for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++)if(edDot(edSub(m->c[0].f[f].v[v].p,point),edSub(m->c[0].f[f].v[v].p,point))<ED_EPS*ED_EPS)vertex=EdHit{0,f,v,-1};
        CHECK(vertex.vertex>=0);*backup=*m;CHECK(edMove(m,backup,vertex,edSub(target,point)));checkRoomCoverage(m);
        if(start==end)CHECK(uniqueVertices(m)==count-1);else CHECK(uniqueVertices(m)<=count);
        for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++){EdVec away=edSub(m->c[0].f[f].v[v].p,point);CHECK(edDot(away,away)>ED_EPS*ED_EPS);}
    }
    edDefault(m);
}
static void singleDoorCutTests(void) {
    for(int extra=0;extra<2;extra++)for(int reverse=0;reverse<2;reverse++){
        edDefault(m);if(extra)CHECK(edExtrudeStep(m,0,2));
        CHECK(edCleave(m,0,4,edV(-.25,1,0),edV(-.25,1,1)));
        EdHit hit;CHECK(edRay(m,edV(0,0,.5),edV(.25,1,0),&hit));CHECK(edCleave(m,0,hit.face,edV(.25,1,0),edV(.25,1,1)));
        CHECK(edRay(m,edV(0,0,.5),edV(0,1,0),&hit));CHECK(edCleave(m,0,hit.face,edV(-.25,1,.75),edV(.25,1,.75)));
        int vertices=uniqueVertices(m),faces=m->c[0].count;
        for(int side=0;side<2;side++){
            int right=side^reverse;double x=right?.25:-.25;CHECK(edRay(m,edV(0,0,.5),edV(x,1,.5),&hit));CHECK(hit.vertex>=0);
            *backup=*m;
            for(int frame=0;frame<=16;frame++){
                EdVec delta=edVertexEdgeDelta(backup,hit,edV((right?.75:-.75)*frame/16,.2*frame/16,-.3*frame/16));
                CHECK(edMove(m,backup,hit,delta));CHECK(edValidate(m));
            }
            CHECK(edValidate(m));CHECK(uniqueVertices(m)==vertices-side-1);CHECK(m->c[0].count<=faces);
            for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++)for(int v=0;v<m->c[s].f[f].count;v++){EdVec p=m->c[s].f[f].v[v].p;CHECK(fabs(p.x-x)>ED_EPS||fabs(p.y-1)>ED_EPS||fabs(p.z-1)>ED_EPS);}
        }
        CHECK(m->c[0].f[1].count==4);
        int sectors=m->count;CHECK(edExtrudeStep(m,0,1));CHECK(m->count==sectors+1);CHECK(m->c[sectors].count==6);CHECK(edValidate(m));
    }
    edDefault(m);
}
static void ceilingMoveTests(void) {
    /* Wall cleaves end on the ceiling edge without being listed in the
       ceiling polygon. Raising it must move those junctions too. */
    edDefault(m);
    CHECK(edCleave(m,0,4,edV(-.25,1,0),edV(-.25,1,1)));
    EdHit wall;CHECK(edRay(m,edV(0,0,.5),edV(.25,1,0),&wall));
    CHECK(edCleave(m,0,wall.face,edV(.25,1,0),edV(.25,1,1)));
    CHECK(edRay(m,edV(0,0,.5),edV(0,1,0),&wall));
    CHECK(edCleave(m,0,wall.face,edV(-.25,1,.75),edV(.25,1,.75)));
    *backup=*m;int faces=m->c[0].count,vertices=uniqueVertices(m);
    for(int frame=1;frame<=20;frame++){
        double rise=frame*ED_GRID;
        CHECK(edMove(m,backup,EdHit{0,1,-1,-1},edV(0,0,rise)));
        CHECK(edValidate(m));CHECK(m->c[0].count==faces);
        CHECK(uniqueVertices(m)==vertices);
        int topJunctions=0;
        for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++){
            EdVec p=m->c[0].f[f].v[v].p;
            if(fabs(p.y-1)<ED_EPS&&fabs(fabs(p.x)-.25)<ED_EPS&&p.z>.9){CHECK(fabs(p.z-1-rise)<ED_EPS);topJunctions++;}
        }CHECK(topJunctions>=4);
    }
    for(int repeat=0;repeat<8;repeat++){
        *backup=*m;CHECK(edMove(m,backup,EdHit{0,1,-1,-1},edV(0,0,ED_GRID)));
        CHECK(m->c[0].count==faces&&uniqueVertices(m)==vertices&&edValidate(m));
    }
    edDefault(m);
}
static void surfaceAttributeTests(void) {
    edDefault(m);EdFace *face=&m->c[0].f[4];face->attributes=edSurfaceAttributes(face);face->hasAttributes=1;
    face->attributes.surfaceFlags=0x10004;face->attributes.faceFlags=3;face->attributes.geometryMode=3;face->attributes.lightingMode=2;face->attributes.textureMode=0;
    face->attributes.uScale=2;face->attributes.vScale=.5;face->attributes.extraLight=.25;edMapFace(face);CHECK(edValidate(m));
    CHECK(fabs(face->v[1].u-face->v[2].u)==256);CHECK(fabs(face->v[1].v-face->v[0].v)==512);
    CHECK(edWriteGob(m,"test-attributes.gob","Attributes",0,"SECTION: HEADER\n"));
    FILE *f=fopen("test-attributes.gob","rb");CHECK(f);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);char *text=edAlloc(size+1);CHECK(text&&fread(text,1,size,f)==(size_t)size);text[size]=0;fclose(f);
    CHECK(strstr(text+12,"0x10004 0x3 3 2 0 -1 0.250000000"));edFree(text);remove("test-attributes.gob");
    CHECK(edCleave(m,0,4,edV(0,1,0),edV(0,1,1)));CHECK(m->c[0].f[4].attributes.faceFlags==3&&m->c[0].f[6].attributes.faceFlags==3);
    CHECK(edResolve(m));CHECK(m->c[0].f[4].attributes.uScale==2&&m->c[0].f[6].attributes.extraLight==.25);
    CHECK(edExtrudeStep(m,0,4));EdFace *portal=&m->c[0].f[4];CHECK(portal->neighbor>=0);EdSurfaceAttributes attrs=edSurfaceAttributes(portal);CHECK(attrs.geometryMode==0&&!(attrs.surfaceFlags&4)&&attrs.uScale==2);
    EdHit portalHit;CHECK(edRaySelection(m,edV(-.5,0,.5),edV(0,1,0),&portalHit));CHECK(m->c[portalHit.cell].f[portalHit.face].neighbor>=0);
    EdHit materialHit;CHECK(edRayMaterial(m,edV(-.5,0,.5),edV(0,1,0),&materialHit));CHECK(materialHit.distance>portalHit.distance&&m->c[materialHit.cell].f[materialHit.face].neighbor<0);
    EdSelection portalPick={0},portalMapped;portalPick.count=1;portalPick.hits[0]=portalHit;portalPick.hits[0].vertex=portalPick.hits[0].edge=-1;edSelectionRemap(m,m,&portalPick,edV(0,0,0),&portalMapped);CHECK(portalMapped.count==1);
    portal->hasAttributes=1;portal->attributes=attrs;portal->attributes.adjoinFlags=5;portal->attributes.adjoinDistance=.125;portal->attributes.geometryMode=4;
    CHECK(edResolve(m));attrs=edSurfaceAttributes(&m->c[0].f[4]);CHECK(attrs.adjoinFlags==5&&attrs.geometryMode==4&&attrs.adjoinDistance==.125);
    CHECK(edRayMaterial(m,edV(-.5,0,.5),edV(0,1,0),&materialHit));CHECK(materialHit.cell==portalHit.cell&&materialHit.face==portalHit.face&&materialHit.vertex<0&&materialHit.edge<0);
    CHECK(edRay(m,edV(-.5,0,.5),edV(0,1,0),&materialHit));CHECK(materialHit.distance>portalHit.distance);
    CHECK(edDeleteSector(m,1));attrs=edSurfaceAttributes(&m->c[0].f[4]);CHECK(attrs.geometryMode==4&&(attrs.surfaceFlags&4));
    m->c[0].f[4].attributes.uScale=0;CHECK(!edValidate(m));edDefault(m);
}
static void cogTests(void){
    const char *script="symbols\nsurface switch linkid=0\nthing elev linkid=1\nint position=0 local\nflex speed=8\nsound noise=set_hi2.wav local\nsector room=-1\nint count=3\nvector point=(1/2/3)\nend\ncode\nend\n";
    edDefault(m);m->cogCount=1;CHECK(edCogParseScript(&m->cogs[0],"test.cog",script));CHECK(m->cogs[0].count==6);CHECK(!strcmp(m->cogs[0].params[0].name,"switch")&&m->cogs[0].params[0].type==ED_COG_SURFACE);CHECK(m->cogs[0].params[2].type==ED_COG_FLEX&&!strcmp(m->cogs[0].params[2].value,"8"));CHECK(m->cogs[0].params[4].type==ED_COG_INT);
    CHECK(edCogSetValue(m,&m->cogs[0].params[0],"4"));CHECK(edCogSetValue(m,&m->cogs[0].params[1],"0"));CHECK(edCogSetValue(m,&m->cogs[0].params[3],"0"));CHECK(edValidate(m));
    CHECK(!edCogSetValue(m,&m->cogs[0].params[1],"1"));CHECK(!edCogSetValue(m,&m->cogs[0].params[2],"nan"));CHECK(!edCogSetValue(m,&m->cogs[0].params[4],"1.5"));CHECK(!edCogSetValue(m,&m->cogs[0].params[5],"(1/2/nan)"));
    CHECK(edWriteGob(m,"test-cogs.gob","COGs",0,"SECTION: HEADER\nSECTION: COGS\nWorld cogs 128\n0: obsolete.cog\nend\nSECTION: TEMPLATES\nWorld templates 0\nend\n"));
    const char embedded[]="symbols\nend\ncode\nend\n";CHECK(edGobAddFile("test-cogs.gob","cog\\test.cog",embedded,sizeof(embedded)-1));
    FILE *f=fopen("test-cogs.gob","rb");CHECK(f);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);char *text=edAlloc(size+1);CHECK(text&&fread(text,1,size,f)==(size_t)size);text[size]=0;fclose(f);
    CHECK(strstr(text+12,"0: test.cog 4 0 8 0 3 (1/2/3)"));CHECK(!strstr(text+12,"obsolete.cog"));CHECK(strstr(text+12,"SECTION: COGS")<strstr(text+12,"SECTION: TEMPLATES"));
    unsigned char *bytes=(unsigned char*)text;unsigned int directory=(unsigned int)bytes[8]|(unsigned int)bytes[9]<<8|(unsigned int)bytes[10]<<16|(unsigned int)bytes[11]<<24;CHECK(bytes[directory]==6);CHECK(!strcmp(text+directory+4+5*136+8,"cog\\test.cog"));edFree(text);remove("test-cogs.gob");
    CHECK(edExtrudeStep(m,0,4));CHECK(m->cogCount==1);EdCogParam *p=&m->cogs[0].params[0];CHECK(p->cell==0&&m->c[p->cell].f[p->face].neighbor>=0);
    CHECK(edCogSetValue(m,p,"7"));int face=p->face;CHECK(p->cell==1);CHECK(edCleave(m,0,2,edV(0,-1,0),edV(0,-1,1)));char value[32];CHECK(edCogParamValue(m,p,value));CHECK(atoi(value)==8&&p->face==face);
    CHECK(edDeleteSector(m,0));p=&m->cogs[0].params[0];CHECK(p->cell==0);CHECK(edValidate(m));CHECK(m->cogs[0].params[3].cell==-1);
    edCogThingChanged(m,0,1);CHECK(atoi(m->cogs[0].params[1].value)==1);edCogThingChanged(m,0,0);CHECK(atoi(m->cogs[0].params[1].value)==0);edCogThingChanged(m,0,0);CHECK(atoi(m->cogs[0].params[1].value)==-1);
    m->cogs[1]=m->cogs[0];m->cogCount=2;m->cogs[1].params[4].type=ED_COG_COG;CHECK(edCogSetValue(m,&m->cogs[1].params[4],"0"));CHECK(edCogDelete(m,0));CHECK(m->cogCount==1&&atoi(m->cogs[0].params[4].value)==-1);
    edDefault(m);m->cogCount=1;CHECK(edCogParseScript(&m->cogs[0],"test.cog",script));CHECK(edCogSetValue(m,&m->cogs[0].params[0],"4"));EdVec before=edNormal(&m->c[0].f[4]);
    CHECK(edCleaveSector(m,0,0,edV(0,-1,0),edV(0,1,0)));p=&m->cogs[0].params[0];CHECK(p->cell>=0&&p->face>=0&&edDot(before,edNormal(&m->c[p->cell].f[p->face]))>.99999);edDefault(m);
    m->cogCount=1;CHECK(edCogParseScript(&m->cogs[0],"test.cog",script));CHECK(edCogSetValue(m,&m->cogs[0].params[0],"4"));CHECK(edCreateStairs(m,EdHit{0,4,-1,-1},EdHit{0,2,-1,-1},4));p=&m->cogs[0].params[0];CHECK(p->cell>=0&&p->face>=0&&edDot(before,edNormal(&m->c[p->cell].f[p->face]))>.99999);
    EdPlacedCog check;CHECK(edCogParseScript(&check,"test.cog","symbols\nflex value=1 desc=\"local\"\nend\n"));CHECK(check.count==1);edDefault(m);
}
static void episodeTests(void){
    edDefault(m);edEpisodeDefault(&m->episode,"Episode Test",0);EdEpisode *e=&m->episode;
    e->type=3;e->count=3;e->current=1;e->rows[0]=EdEpisodeRow{.line=10,.cd=0,.level=1,.cutscene=1,.gotoA=20,.gotoB=20};strcpy(e->rows[0].file,"intro.smk");
    e->rows[1]=EdEpisodeRow{.line=20,.cd=1,.level=2,.lightPower=2,.darkPower=3,.gotoA=30,.gotoB=-1};strcpy(e->rows[1].file,"episode_test.jkl");
    e->rows[2]=EdEpisodeRow{.line=30,.cd=1,.level=3,.gotoA=-1,.gotoB=-1};strcpy(e->rows[2].file,"next.jkl");strcpy(e->rows[2].title,"Next level");strcpy(e->rows[2].intro[0],"The next challenge");
    CHECK(edEpisodeValidate(e));CHECK(edWriteGob(m,"test-episode.gob","Level",0,"SECTION: HEADER\n"));
    FILE *f=fopen("test-episode.gob","rb");CHECK(f);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);char *text=edAlloc(size+1);CHECK(text&&fread(text,1,size,f)==(size_t)size);text[size]=0;fclose(f);
    CHECK(strstr(text+12,"\"Episode Test\"\nTYPE 3\nSEQ 3\n10: 0 1 CUT intro.smk 0 0 20 20\n20: 1 2 LEVEL episode_test.jkl 2 3 30 -1\n30: 1 3 LEVEL next.jkl 0 0 -1 -1\nend"));
    CHECK(strstr(text+12,"\"EPISODE_TEST\" 0 \"New Editor Level\""));CHECK(strstr(text+12,"\"NEXT_TEXT_00\" 0 \"The next challenge\""));
    EdEpisode *parsed=edAlloc(sizeof(*parsed));CHECK(parsed&&edReadEpisode(parsed,text+12,"episode_test.jkl"));CHECK(parsed->count==3&&parsed->current==1&&parsed->type==3&&parsed->rows[1].lightPower==2&&parsed->rows[1].darkPower==3);edFree(parsed);
    unsigned char *bytes=(unsigned char*)text;unsigned int directory=bytes[8]|bytes[9]<<8|bytes[10]<<16|bytes[11]<<24;CHECK(directory+4+5*136<=size);CHECK(bytes[directory]==5);
    CHECK(!strcmp(text+directory+4+136+8,"jkl\\episode_test.jkl"));CHECK(!strcmp(text+directory+4+4*136+8,"misc\\sithed_editor.txt"));
    edFree(text);remove("test-episode.gob");*backup=*m;CHECK(edExtrudeStep(m,0,4));CHECK(!edCompare(&m->episode,&backup->episode,sizeof(m->episode)));
    e=&m->episode;e->rows[1].gotoA=999;CHECK(!edEpisodeValidate(e));CHECK(!edWriteGob(m,"test-episode.gob","Level",0,""));e->rows[1].gotoA=30;e->rows[2].line=20;CHECK(!edEpisodeValidate(e));e->rows[2].line=30;
    strcpy(e->rows[1].file,"../escape.jkl");CHECK(!edEpisodeValidate(e));strcpy(e->rows[1].file,"episode_test.jkl");e->rows[1].cutscene=1;CHECK(!edEpisodeValidate(e));
    edDefault(m);
}
static void lightTests(void) {
    edDefault(m);CHECK(!m->lightCount);CHECK(m->c[0].extraLight==.5&&m->c[0].ambientLight==0);CHECK(edVertexLight(m,0,4,0)==0);
    m->lightCount=1;m->lights[0]=EdLight{0,edV(.2,.3,.6),1,4};CHECK(edValidate(m));
    double initial=edVertexLight(m,0,4,0);CHECK(initial>0&&initial<.5);
    m->lights[0].intensity=2;CHECK(fabs(edVertexLight(m,0,4,0)-initial*2)<1e-8);
    m->lights[0].intensity=1;m->lights[0].range=ED_GRID;CHECK(edVertexLight(m,0,4,0)==0);
    m->lights[0].range=4;m->lightCount=2;m->lights[1]=m->lights[0];CHECK(fabs(edVertexLight(m,0,4,0)-initial*2)<1e-8);
    m->lights[0].intensity=m->lights[1].intensity=100;CHECK(edVertexLight(m,0,4,0)==1);
    m->lightCount=1;m->lights[0].intensity=1;*backup=*m;CHECK(edExtrudeStep(m,0,4));CHECK(m->lightCount==1&&!edCompare(m->lights,backup->lights,m->lightCount*sizeof(EdLight)));
    /* Portals pass light; a closed intervening room wall blocks it. */
    m->lights[0].position=edV(0,1.03,.5);CHECK(edVertexLight(m,0,2,0)>0);
    *m=*backup;m->c[1]=m->c[0];m->count=2;for(int f=0;f<m->c[1].count;f++)for(int v=0;v<m->c[1].f[f].count;v++)m->c[1].f[f].v[v].p.y+=3;
    m->lights[0].position=edV(0,3,.5);m->lights[0].range=10;CHECK(edVertexLight(m,0,2,0)==0);
    *m=*backup;CHECK(edWriteGob(m,"test-lights.gob","Lights",0,"SECTION: HEADER\n"));
    FILE *f=fopen("test-lights.gob","rb");CHECK(f);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);char *text=edAlloc(size+1);CHECK(text&&fread(text,1,size,f)==(size_t)size);text[size]=0;fclose(f);
    CHECK(strstr(text+12,"Section: Lights\nEditor lights 1"));CHECK(strstr(text+12,"Ambient light 0.000000000\nExtra light 0.500000000"));
    m->lightCount=0;CHECK(edReadLights(m,text+12));CHECK(m->lightCount==1&&!edCompare(m->lights,backup->lights,sizeof(EdLight)));edFree(text);remove("test-lights.gob");
    *backup=*m;CHECK(!edReadLights(m,"Section: Lights\nEditor lights 1\n0: 0 0 0 .5 nan 4\nend\n"));CHECK(!edCompare(m,backup,sizeof(*m)));
    CHECK(!edReadLights(m,"Section: Lights\nEditor lights 257\nend\n"));CHECK(!edReadLights(m,"Section: Lights\nEditor lights 1\nend\n"));
    CHECK(edReadLights(m,"Section: Lights\r\nEditor lights 1\r\n#num type X Y Z intensity range\r\n0: 0x1 .1 .2 .3 .7 2.5\r\nend\r\n"));CHECK(m->lights[0].type==1&&m->lights[0].intensity==.7);
    CHECK(edReadLights(m,"Section: Lights\nEditor lights 0\nend\n"));CHECK(!m->lightCount);edDefault(m);
}
static void horizontalExtrusionTests(void) {
    for(int yaw=0;yaw<3;yaw++)for(int slope=-2;slope<=2;slope++)for(int grouped=0;grouped<2;grouped++){
        edDefault(m);if(grouped)CHECK(edCleave(m,0,4,edV(0,1,0),edV(0,1,1)));
        double angle=yaw*.7;
        for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++){
            EdVec p=m->c[0].f[f].v[v].p;p.y+=slope*1.5*(p.z-.5);p.z+=.2*p.x;
            m->c[0].f[f].v[v].p=edV(cos(angle)*p.x-sin(angle)*p.y,sin(angle)*p.x+cos(angle)*p.y,p.z);
        }
        CHECK(edValidate(m));edMapWorld(m);*backup=*m;double volume=totalVolume(m);
        EdSelection selected={0};selected.count=grouped?2:1;selected.hits[0]=EdHit{0,4,-1,-1};selected.hits[1]=EdHit{0,6,-1,-1};
        for(int i=0;i<selected.count;i++){
            EdFace *base=&backup->c[0].f[selected.hits[i].face],cap;CHECK(edExtrusionCap(base,1,&cap));
            EdVec n=edNormal(&cap),axis=edScale(n,-1);CHECK(fabs(n.z)<1e-8);double furthest=-1e30;
            for(int v=0;v<base->count;v++)furthest=fmax(furthest,edDot(axis,base->v[v].p));
            for(int v=0;v<base->count;v++){
                EdVec move=edSub(cap.v[v].p,base->v[v].p);CHECK(fabs(move.z)<1e-8);CHECK(edDot(move,axis)>=ED_GRID-1e-8);
                CHECK(fabs(edDot(axis,cap.v[v].p)-furthest-ED_GRID)<1e-8);CHECK(edDot(edCross(move,axis),edCross(move,axis))<1e-12);
            }
        }
        CHECK(grouped?edExtrudeSelectionHorizontal(m,&selected):edExtrudeHorizontal(m,0,4));CHECK(edValidate(m));CHECK(totalVolume(m)>volume);CHECK(portals()>=2*selected.count);
        for(int i=0;i<selected.count;i++){
            EdHit h=selected.hits[i];EdFace cap;CHECK(edExtrusionCap(&backup->c[h.cell].f[h.face],1,&cap));backup->c[h.cell].f[h.face]=cap;
            EdSelection one={0},mapped;one.count=1;one.hits[0]=h;edSelectionRemap(backup,m,&one,edV(0,0,0),&mapped);CHECK(mapped.count>=1);
            for(int j=0;j<mapped.count;j++){EdHit match=mapped.hits[j];CHECK(fabs(edNormal(&m->c[match.cell].f[match.face]).z)<1e-8);}
        }
    }
    edDefault(m);*backup=*m;CHECK(!edExtrudeHorizontal(m,0,0));CHECK(!edCompare(m,backup,sizeof(*m)));CHECK(!edExtrudeHorizontal(m,0,1));CHECK(!edCompare(m,backup,sizeof(*m)));
    CHECK(edExtrudeHorizontal(m,0,4));CHECK(fabs(totalVolume(m)-(4+2*ED_GRID))<1e-6);CHECK(portals()==2);
    CHECK(edExtrudeHorizontal(m,1,1));CHECK(edValidate(m));CHECK(fabs(totalVolume(m)-(4+4*ED_GRID))<1e-6);CHECK(portals()==4);
    edDefault(m);
}
static void stairsTests(void) {
    const int highFaces[]={4,2,3,5},lowFaces[]={2,4,5,3};
    const int counts[]={1,4,16};
    for(int direction=0;direction<4;direction++)for(int test=0;test<3;test++){
        edDefault(m);int steps=counts[test];EdHit high={0,highFaces[direction],-1,-1},low={0,lowFaces[direction],-1,-1};
        EdVec axis=edNormal(&m->c[0].f[low.face]);double rise=0,run=0;
        CHECK(edStairsDimensions(m,high,low,&rise,&run)&&rise==1&&run==2);
        CHECK(edCreateStairs(m,high,low,steps));CHECK(m->count==steps+1);CHECK(edValidate(m));CHECK(portals()==steps*2);
        CHECK(fabs(totalVolume(m)-4.0*(steps+1)/(2*steps))<.0001);
        for(int i=1;i<m->count;i++)CHECK(m->c[i].count==5);
        int rampFaces=0;
        for(int cell=0;cell<m->count;cell++)for(int f=0;f<m->c[cell].count;f++){
            EdFace *face=&m->c[cell].f[f];if(face->neighbor<0)continue;EdSurfaceAttributes a=edSurfaceAttributes(face);
            CHECK(a.geometryMode==0&&(a.surfaceFlags&5)==5&&(a.adjoinFlags&7)==5);rampFaces++;
        }
        CHECK(rampFaces==2*steps);
        for(int i=0;i<steps;i++){
            double along=-1+2.0*(i+.5)/steps,level=(double)i/steps,ramp=(i+.5)/steps;
            EdVec point=edV(axis.x*along,axis.y*along,(ramp+1)*.5);CHECK(edContains(&m->c[0],point));
            point.z=level+.25/steps;CHECK(edContains(&m->c[i+1],point));
            if(i){point.z=level-.01/steps;for(int cell=0;cell<m->count;cell++)CHECK(!edContains(&m->c[cell],point));}
        }
        for(int i=0;i<m->spawnCount;i++){int inside=0;for(int cell=0;cell<m->count;cell++)inside|=edContains(&m->c[cell],m->spawns[i].position);CHECK(inside);}
    }
    edDefault(m);CHECK(edExtrudeStep(m,0,5));int previous=m->count;EdHit high={0,4,-1,-1},low={0,2,-1,-1};CHECK(edCreateStairs(m,high,low,4));CHECK(m->count==previous+4);CHECK(edValidate(m));CHECK(portals()>8);
    edDefault(m);for(int f=0;f<6;f++)for(int v=0;v<4;v++){EdVec p=m->c[0].f[f].v[v].p;m->c[0].f[f].v[v].p=edV(p.x*cos(.4)-p.y*sin(.4),p.x*sin(.4)+p.y*cos(.4),p.z);}
    CHECK(edCreateStairs(m,high,low,4));CHECK(edValidate(m));CHECK(portals()==8);
    edDefault(m);*backup=*m;CHECK(!edCreateStairs(m,high,high,4));CHECK(!edCreateStairs(m,high,low,0));CHECK(!edCompare(m,backup,sizeof(*m)));
    low.face=0;CHECK(!edCreateStairs(m,high,low,4));CHECK(!edCompare(m,backup,sizeof(*m)));edDefault(m);
}
static EdHit deleteEdgeHit(int sector,EdVec a,EdVec b){
    EdHit hit={-1,-1,-1,-1};
    for(int f=0;f<m->c[sector].count;f++)for(int v=0;v<m->c[sector].f[f].count;v++){
        EdFace *face=&m->c[sector].f[f];EdVec p=face->v[v].p,q=face->v[(v+1)%face->count].p;
        if((edDot(edSub(p,a),edSub(p,a))<1e-10&&edDot(edSub(q,b),edSub(q,b))<1e-10)||(edDot(edSub(p,b),edSub(p,b))<1e-10&&edDot(edSub(q,a),edSub(q,a))<1e-10))hit=EdHit{sector,f,-1,v};
    }return hit;
}
static EdVec deleteFan(int sector,int f){
    EdCell *cell=&m->c[sector];EdFace original=cell->f[f];CHECK(original.count==4);EdVertex center={0};
    for(int v=0;v<4;v++)center.p=edAdd(center.p,edScale(original.v[v].p,.25));
    for(int v=0;v<4;v++){EdFace triangle=original;triangle.count=3;triangle.v[0]=original.v[v];triangle.v[1]=original.v[(v+1)%4];triangle.v[2]=center;edMapFace(&triangle);cell->f[v?cell->count++:f]=triangle;}
    return center.p;
}
static int portalCoverage(void){
    /* Saved levels may omit collinear junctions from one portal loop. Check
       reciprocal polygon coverage rather than requiring identical lists. */
    int count=0;for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++){
        EdFace *p=&m->c[s].f[f];if(p->neighbor<0)continue;CHECK(p->neighbor<m->count&&p->mirror>=0&&p->mirror<m->c[p->neighbor].count);EdFace *q=&m->c[p->neighbor].f[p->mirror];CHECK(q->neighbor==s&&q->mirror==f);EdVec n=edNormal(q);CHECK(edDot(edNormal(p),n)<-.99999);
        for(int v=0;v<p->count;v++){EdVec point=p->v[v].p;CHECK(fabs(edDot(n,edSub(point,q->v[0].p)))<ED_EPS);for(int e=0;e<q->count;e++){EdVec edge=edSub(q->v[(e+1)%q->count].p,q->v[e].p);CHECK(edDot(edCross(edge,edSub(point,q->v[e].p)),n)>=-ED_EPS*sqrt(edDot(edge,edge)));}}
        count++;
    }return count;
}
static void readGeometryFixture(const char *filename){
    /* Geometry-only regression fixtures; no game resources. */
    char path[1024];snprintf(path,sizeof(path),"%s",__FILE__);char *end=strrchr(path,'/'),*win=strrchr(path,'\\');if(win&&(!end||win>end))end=win;CHECK(end);strcpy(end+1,filename);
    FILE *file=fopen(path,"r");CHECK(file);edZero(m,0,sizeof(*m));CHECK(fscanf(file,"%d",&m->count)==1&&m->count>0);
    for(int s=0;s<m->count;s++){EdCell *c=&m->c[s];CHECK(fscanf(file,"%d %lf %lf",&c->count,&c->ambientLight,&c->extraLight)==3&&c->count<=ED_FACES);
        for(int f=0;f<c->count;f++){EdFace *p=&c->f[f];EdSurfaceAttributes *a=&p->attributes;p->hasAttributes=1;
            CHECK(fscanf(file,"%d %d %d %d %lf %lf %u %u %u %d %d %d %lf %lf %lf %lf",&p->count,&p->material,&p->neighbor,&p->mirror,&p->uOffset,&p->vOffset,&a->surfaceFlags,&a->faceFlags,&a->adjoinFlags,&a->geometryMode,&a->lightingMode,&a->textureMode,&a->uScale,&a->vScale,&a->extraLight,&a->adjoinDistance)==16&&p->count<=ED_VERTS);a->portal=p->neighbor>=0;
            for(int v=0;v<p->count;v++)CHECK(fscanf(file,"%lf %lf %lf",&p->v[v].p.x,&p->v[v].p.y,&p->v[v].p.z)==3);edMapFace(p);
        }
    }fclose(file);
}
static void mergeSectorTests(void){
    edDefault(m);m->c[0].extraLight=.3;m->c[0].flags=1;m->c[0].tint=edV(.1,.2,.3);strcpy(m->c[0].colormap,"01narsh.cmp");strcpy(m->c[0].layer,"Main rooms");
    CHECK(edExtrudeStep(m,0,4));
    m->cogCount=1;strcpy(m->cogs[0].file,"test.cog");m->cogs[0].count=3;
    for(int i=0;i<3;i++){snprintf(m->cogs[0].params[i].name,32,"arg%d",i);m->cogs[0].params[i].type=i==1?ED_COG_SECTOR:ED_COG_SURFACE;}
    m->cogs[0].params[0].cell=0;m->cogs[0].params[0].face=4;
    m->cogs[0].params[1].cell=1;m->cogs[0].params[2].cell=1;m->cogs[0].params[2].face=1;
    double before=totalVolume(m);EdBuffer<unsigned char> selected;selected[0]=selected[1]=1;int merged=-1;
    CHECK(edMergeSectors(m,selected,&merged)&&merged==0&&m->count==1&&m->c[0].count==6&&edValidate(m));CHECK(fabs(totalVolume(m)-before)<1e-6&&m->c[0].extraLight==.3&&m->c[0].flags==1&&!strcmp(m->c[0].layer,"Main rooms"));
    CHECK(m->cogs[0].params[0].cell==-1&&m->cogs[0].params[0].face==-1&&m->cogs[0].params[1].cell==0&&m->cogs[0].params[2].cell==0&&m->cogs[0].params[2].face>=0);m->cogCount=0;
    uint32_t bytes=0;unsigned char *wire=edWireEncode(m,&bytes);CHECK(wire&&edWireDecode(backup,wire,bytes));edFree(wire);CHECK(!strcmp(backup->c[0].colormap,"01narsh.cmp")&&!strcmp(backup->c[0].layer,"Main rooms"));
    CHECK(edWriteGob(m,"test-sectors.gob","Sectors",0,"SECTION: HEADER\n"));FILE *file=fopen("test-sectors.gob","rb");CHECK(file);fseek(file,0,SEEK_END);long length=ftell(file);rewind(file);char *text=edAlloc(length+1);CHECK(text&&fread(text,1,length,file)==length);fclose(file);text[length]=0;
    CHECK(strstr(text+12,"World colormaps 2\n0: dflt.cmp\n1: 01narsh.cmp")&&strstr(text+12,"Colormap 1\n")&&strstr(text+12,"# GeoSith layer 0 Main rooms"));edFree(text);remove("test-sectors.gob");
    // Concave L-shaped unions and disconnected rooms must never be filled in.
    edDefault(m);EdModel clip;edDefault(&clip);EdBuffer<unsigned char> result;*backup=*m;CHECK(edMapPaste(m,backup,&clip,edV(2,0,0),result));*backup=*m;CHECK(edMapPaste(m,backup,&clip,edV(0,2,0),result));selected[2]=1;*backup=*m;CHECK(!edMergeSectors(m,selected,&merged)&&edModelsEqual(m,backup));
    edDefault(m);*backup=*m;CHECK(edMapPaste(m,backup,&clip,edV(3,0,0),result));*backup=*m;CHECK(!edMergeSectors(m,selected,&merged)&&edModelsEqual(m,backup));
    // Retain reciprocal external adjoins when removing a middle sector ID.
    edDefault(m);CHECK(edExtrudeStep(m,0,4));CHECK(edExtrudeStep(m,0,2));selected.clear();selected[0]=selected[1]=1;CHECK(edMergeSectors(m,selected,&merged)&&m->count==2&&edValidate(m));CHECK(portalCoverage()==2);
    puts("PASS: convex sector merge, concave/disconnected rejection, external adjoins, sector attributes and saved colormaps");
}
static void savedMapVertexTests(void){
    readGeometryFixture("triforcitadel_geometry.txt");CHECK(m->count==19&&edValidate(m));*backup=*m;
    // Every direction used to fail here, including movements of a whole face.
    for(int sector: {4,14})for(int f=0;f<backup->c[sector].count;f++){
        EdSelection selected={0};const EdFace *face=&backup->c[sector].f[f];
        for(int v=0;v<face->count;v++)selected.hits[selected.count++]=EdHit{sector,f,-1,v};
        for(int axis=0;axis<4;axis++){
            EdVec delta=edV(axis<2?(axis?-ED_GRID:ED_GRID):0,axis>=2?(axis==2?ED_GRID:-ED_GRID):0,0);
            CHECK(edMoveSelectionXY(m,backup,&selected,delta)&&edValidate(m));
            portalCoverage();
        }
    }
    // Match top-down picking: include each height at the selected XY points.
    EdSelection selected={0};const EdFace *face=&backup->c[4].f[2];
    for(int s=0;s<backup->count;s++)for(int f=0;f<backup->c[s].count;f++)for(int v=0;v<backup->c[s].f[f].count;v++){
        EdVec point=backup->c[s].f[f].v[v].p;int match=0;
        for(int j=0;j<face->count;j++)if(hypot(point.x-face->v[j].p.x,point.y-face->v[j].p.y)<ED_EPS)match=1;
        if(match){EdSelection next=selected;CHECK(edSelectionToggle(backup,&next,EdHit{s,f,-1,v}));if(next.count>selected.count)selected=next;}
    }
    CHECK(selected.count>0&&edMoveSelectionXY(m,backup,&selected,edV(ED_GRID,0,0))&&edValidate(m));portalCoverage();
    puts("PASS: Triforcitadel face vertices and top-down groups move without unrelated topology changes or rounded-edge slivers");
}
static void savedPlacementTests(void){
    readGeometryFixture("placement_limit_fixture.txt");CHECK(edValidate(m));*backup=*m;
    EdModel *clip=edAlloc(sizeof(*clip));CHECK(clip);edDefault(clip);EdBuffer<unsigned char> selected;
    CHECK(edMapPaste(m,backup,clip,edV(100,100,0),selected));CHECK(m->count==54&&edValidate(m));
    CHECK(edMapPaste(m,backup,clip,edV(3,-2,0),selected));CHECK(m->count>53&&edValidate(m));
    for(int i=0;i<3;i++){int count=m->count;CHECK(edResolve(m)&&m->count==count&&edValidate(m));}
    puts("PASS: saved 53-sector level accepts distant and overlapping sectors without runaway portal splitting");
    edFree(clip);
}
static void savedCleanupTests(void){
    readGeometryFixture("cleanup_fixture.txt");
    CHECK(edValidate(m));int connections=portalCoverage();double volume=totalVolume(m);*backup=*m;
    CHECK(edCleanupSector(m,50));CHECK(edValidate(m)&&portalCoverage()==connections);CHECK(m->c[50].count==10&&backup->c[50].count==51);CHECK(fabs(totalVolume(m)-volume)<1e-8);
    int wallFaces=0;for(int f=0;f<m->c[50].count;f++){EdFace *p=&m->c[50].f[f];if(p->neighbor<0&&edNormal(p).y>.99){CHECK(p->count==4);wallFaces++;}}
    CHECK(wallFaces==3);
    *backup=*m;CHECK(!edCleanupSector(m,50));CHECK(!edCompare(m,backup,sizeof(*m)));
    puts("PASS: reported wall reduced from 44 fragments to 3 quads, preserving its opening, volume and adjoins");
}
static void cleanupTests(void){
    edDefault(m);deleteFan(0,4);deleteFan(0,1);double volume=totalVolume(m);
    m->cogCount=1;strcpy(m->cogs[0].file,"test.cog");m->cogs[0].count=2;
    m->cogs[0].params[0]=EdCogParam{.type=ED_COG_SURFACE,.cell=0,.face=7};m->cogs[0].params[1]=EdCogParam{.type=ED_COG_SURFACE,.cell=0,.face=10};
    CHECK(edValidate(m));CHECK(edCleanupSector(m,0));CHECK(m->c[0].count==6&&uniqueVertices(m)==8&&edValidate(m));CHECK(fabs(totalVolume(m)-volume)<1e-9);
    for(int f=0;f<6;f++)CHECK(m->c[0].f[f].count==4);CHECK(m->c[0].extraLight==.5);
    CHECK(edNormal(&m->c[0].f[m->cogs[0].params[0].face]).y<-.99);CHECK(edNormal(&m->c[0].f[m->cogs[0].params[1].face]).z<-.99);
    *backup=*m;CHECK(!edCleanupSector(m,0)&&!edCompare(m,backup,sizeof(*m)));
    /* Different textures, UV mappings and flags remain separate. */
    for(int kind=0;kind<3;kind++){
        edDefault(m);CHECK(edCleave(m,0,4,edV(0,1,0),edV(0,1,1)));EdFace *patch=&m->c[0].f[6];
        if(kind==0)patch->material=1;else if(kind==1)patch->uOffset=16;else{patch->attributes=edSurfaceAttributes(patch);patch->hasAttributes=1;patch->attributes.faceFlags=1;}
        edMapWorld(m);*backup=*m;CHECK(!edCleanupSector(m,0));CHECK(!edCompare(m,backup,sizeof(*m)));
    }
    /* A concave region of the same texture must not swallow a different patch. */
    edDefault(m);CHECK(edCleave(m,0,4,edV(0,1,0),edV(0,1,1)));EdHit hit;CHECK(edRay(m,edV(0,0,.5),edV(.5,1,0),&hit));
    CHECK(edCleave(m,0,hit.face,edV(0,1,.5),edV(1,1,.5)));CHECK(edRay(m,edV(0,0,.5),edV(.5,1,.25),&hit));m->c[0].f[hit.face].material=1;
    *backup=*m;CHECK(!edCleanupSector(m,0));CHECK(!edCompare(m,backup,sizeof(*m)));
    /* Reciprocal portal patches consolidate together; neither side is lost. */
    edDefault(m);CHECK(edExtrudeStep(m,0,4));int neighbor=m->c[0].f[4].neighbor,mirror=m->c[0].f[4].mirror;
    deleteFan(0,4);deleteFan(neighbor,mirror);CHECK(edResolve(m));CHECK(portals()>2);volume=totalVolume(m);
    CHECK(edCleanupSector(m,0));CHECK(edValidate(m)&&portals()==2&&m->c[0].count==6&&m->c[1].count==6);CHECK(fabs(totalVolume(m)-volume)<1e-9);
    /* Collinear boundary vertices disappear even without a face merge. */
    edDefault(m);EdFace *face=&m->c[0].f[4];EdVertex mid=face->v[0];mid.p=edScale(edAdd(face->v[0].p,face->v[1].p),.5);memmove(&face->v[2],&face->v[1],3*sizeof(EdVertex));face->v[1]=mid;face->count=5;
    CHECK(edCleanupSector(m,0));CHECK(m->c[0].f[4].count==4&&edValidate(m));
    *backup=*m;CHECK(!edCleanupSector(m,-1)&&!edCompare(m,backup,sizeof(*m)));edDefault(m);
}
static void deleteHandleTests(void){
    edDefault(m);EdFace quad=m->c[0].f[4];EdVec a=quad.v[0].p,b=quad.v[2].p;
    CHECK(edCleave(m,0,4,a,b));CHECK(m->c[0].count==7);EdHit edge=deleteEdgeHit(0,a,b);CHECK(edge.cell==0);
    m->c[0].f[edge.face].material=17;m->c[0].f[edge.face].uOffset=12;
    CHECK(edDeleteHandle(m,edge));CHECK(edValidate(m)&&m->c[0].count==6&&fabs(totalVolume(m)-4)<1e-8);
    int found=0;for(int f=0;f<m->c[0].count;f++)if(edDot(edNormal(&m->c[0].f[f]),edNormal(&quad))>.99999){CHECK(m->c[0].f[f].count==4&&m->c[0].f[f].material==17&&m->c[0].f[f].uOffset==12);found++;}CHECK(found==1);
    /* A real corner/crease cannot be removed to leave open or twisted faces. */
    edDefault(m);*backup=*m;CHECK(!edDeleteHandle(m,EdHit{0,4,-1,0}));CHECK(!edCompare(m,backup,sizeof(*m)));
    CHECK(!edDeleteHandle(m,EdHit{0,4,0,-1}));CHECK(!edCompare(m,backup,sizeof(*m)));
    /* Deleting the center of a triangle fan leaves one quad and remaps COGs. */
    EdVec center=deleteFan(0,4);m->cogCount=1;strcpy(m->cogs[0].file,"test.cog");m->cogs[0].count=2;
    m->cogs[0].params[0]=EdCogParam{.type=ED_COG_SURFACE,.cell=0,.face=7};m->cogs[0].params[1]=EdCogParam{.type=ED_COG_SURFACE,.cell=0,.face=1};
    CHECK(edValidate(m));CHECK(edDeleteHandle(m,EdHit{0,4,2,-1}));CHECK(edValidate(m)&&m->c[0].count==6);
    CHECK(edDot(edNormal(&m->c[0].f[m->cogs[0].params[0].face]),edNormal(&quad))>.99999);
    CHECK(edNormal(&m->c[0].f[m->cogs[0].params[1].face]).z<-.99);
    /* Remove a redundant boundary vertex from its shared wall/ceiling edge. */
    edDefault(m);EdFace *face=&m->c[0].f[4];EdVertex midpoint=face->v[0];midpoint.p=edScale(edAdd(face->v[0].p,face->v[1].p),.5);
    memmove(&face->v[2],&face->v[1],3*sizeof(EdVertex));face->v[1]=midpoint;face->count=5;CHECK(edValidate(m));
    CHECK(edDeleteHandle(m,EdHit{0,4,1,-1}));CHECK(edValidate(m)&&m->c[0].count==6);
    /* Two patches whose union is an L must not turn into a filled convex hull. */
    edDefault(m);CHECK(edCleave(m,0,4,edV(0,1,0),edV(0,1,1)));
    int right=-1;for(int f=0;f<m->c[0].count;f++){EdFace *p=&m->c[0].f[f];EdVec c=edV(0,0,0);for(int v=0;v<p->count;v++)c=edAdd(c,edScale(p->v[v].p,1.0/p->count));if(c.y>.99&&c.x>.1)right=f;}
    CHECK(right>=0&&edCleave(m,0,right,edV(0,1,.5),edV(1,1,.5)));edge=deleteEdgeHit(0,edV(0,1,0),edV(0,1,.5));CHECK(edge.cell==0);
    *backup=*m;CHECK(!edDeleteHandle(m,edge));CHECK(!edCompare(m,backup,sizeof(*m)));
    /* A portal's subdivided faces dissolve on both sides at once. */
    edDefault(m);CHECK(edExtrudeStep(m,0,4));int neighbor=m->c[0].f[4].neighbor,mirror=m->c[0].f[4].mirror;
    center=deleteFan(0,4);deleteFan(neighbor,mirror);CHECK(edResolve(m));CHECK(portals()>2);
    edge=deleteEdgeHit(0,center,edV(1,1,1));CHECK(edge.cell==0&&edDeleteHandle(m,edge));CHECK(portals()==6&&edValidate(m));
    EdHit vertex={-1,-1,-1,-1};for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++)if(edDot(edSub(m->c[0].f[f].v[v].p,center),edSub(m->c[0].f[f].v[v].p,center))<1e-10)vertex=EdHit{0,f,v,-1};
    CHECK(vertex.cell==0&&edDeleteHandle(m,vertex));CHECK(edValidate(m)&&portals()==2&&fabs(totalVolume(m)-(4+2*ED_GRID))<1e-7);
    CHECK(!edDeleteHandle(m,EdHit{-1,0,0,-1}));edDefault(m);
}
static void mapCleaveDeleteTests(void){
    EdBuffer<unsigned char> selected={1},result;edDefault(m);*backup=*m;
    CHECK(edMapCleave(m,backup,selected,edV(0,-2,0),edV(0,2,0),result));CHECK(m->count==2&&result[0]&&result[1]&&portals()==2&&edValidate(m));CHECK(fabs(totalVolume(m)-4)<1e-7);
    *backup=*m;edZero(selected,0,sizeof(selected));selected[0]=1;CHECK(edMapDelete(m,backup,selected));CHECK(m->count==1&&portals()==0&&edValidate(m));CHECK(fabs(totalVolume(m)-2)<1e-7);
    edDefault(m);*backup=*m;CHECK(!edMapCleave(m,backup,selected,edV(0,3,0),edV(0,4,0),result));CHECK(!edCompare(m,backup,sizeof(*m)));
    CHECK(!edMapCleave(m,backup,selected,edV(0,0,0),edV(0,0,0),result));CHECK(!edCompare(m,backup,sizeof(*m)));
    CHECK(!edMapCleave(m,backup,selected,edV(1,-2,0),edV(1,2,0),result));CHECK(!edCompare(m,backup,sizeof(*m)));
    CHECK(!edMapDelete(m,backup,selected));CHECK(!edCompare(m,backup,sizeof(*m)));
    /* The whole cut across connected selections is one geometry transaction. */
    CHECK(edExtrudeStep(m,0,4));*backup=*m;selected[1]=1;
    CHECK(edMapCleave(m,backup,selected,edV(0,-2,0),edV(0,3,0),result));CHECK(m->count==4&&portals()>=6&&edValidate(m));CHECK(fabs(totalVolume(m)-(4+2*ED_GRID))<1e-7);
    for(int s=0;s<4;s++)CHECK(result[s]);*backup=*m;
    selected.clear();for(int s=0;s<m->count;s++)selected[s]=1;CHECK(!edMapDelete(m,backup,selected));CHECK(!edCompare(m,backup,sizeof(*m)));
    edZero(selected,0,sizeof(selected));selected[1]=selected[3]=1;CHECK(edMapDelete(m,backup,selected));CHECK(m->count==2&&edValidate(m));
    /* A projected line cuts sloped six-sided sectors with a vertical plane. */
    edDefault(m);for(int f=0;f<6;f++)for(int v=0;v<4;v++)m->c[0].f[f].v[v].p.z+=m->c[0].f[f].v[v].p.x*.2;
    CHECK(edValidate(m));*backup=*m;edZero(selected,0,sizeof(selected));selected[0]=1;
    CHECK(edMapCleave(m,backup,selected,edV(-2,-2,0),edV(2,2,0),result));CHECK(m->count==2&&portals()==2&&edValidate(m));CHECK(fabs(totalVolume(m)-4)<1e-7);
    /* A disjoint, unselected room under the same infinite line is untouched. */
    edDefault(m);*backup=*m;CHECK(edMapTranslate(m,backup,selected,edV(0,4,0),1,result));*backup=*m;
    CHECK(edMapCleave(m,backup,selected,edV(0,-2,0),edV(0,6,0),result));CHECK(m->count==3&&!result[1]&&edValidate(m));CHECK(!edCompare(&m->c[1],&backup->c[1],sizeof(EdCell)));
    edDefault(m);
}
static void scaleTests(void){
    EdModel source,out,back;edDefault(&source);EdSelection faces={};faces.count=1;faces.hits[0]={0,0,-1,-1};EdBuffer<unsigned char> sectors;EdVec pivot;
    CHECK(edScaleSelection(&out,&source,&faces,sectors,1.1,&pivot)&&edValidate(&out));
    CHECK(fabs(out.c[0].f[0].v[0].p.x-source.c[0].f[0].v[0].p.x*1.1)<1e-9&&fabs(out.c[0].f[0].v[0].p.z)<1e-9);
    CHECK(edScaleSelection(&back,&out,&faces,sectors,1/1.1,&pivot)&&edValidate(&back));for(int v=0;v<4;v++)CHECK(fabs(back.c[0].f[0].v[v].p.x-source.c[0].f[0].v[v].p.x)<1e-8);
    sectors[0]=1;CHECK(edScaleSelection(&out,&source,&faces,sectors,1.1,&pivot)&&fabs(pivot.z-.5)<1e-9&&edValidate(&out));
    CHECK(fabs(out.c[0].f[0].v[0].p.z+.05)<1e-9);
    // A zero or nonfinite scale must not modify the destination.
    back=out;CHECK(!edScaleSelection(&out,&source,&faces,sectors,0,&pivot)&&edModelsEqual(&out,&back));
    source.c[0].splineId=1;CHECK(!edScaleSelection(&out,&source,&faces,sectors,1.1,&pivot)&&edModelsEqual(&out,&back));
    edDefault(&source);sectors.clear();faces.hits[0].vertex=0;CHECK(!edScaleSelection(&out,&source,&faces,sectors,1.1,&pivot));
    // Two nonparallel faces produce a nonplanar adjoining wall: reject it.
    faces.hits[0]={0,0,-1,-1};faces.hits[1]={0,2,-1,-1};faces.count=2;CHECK(!edScaleSelection(&out,&source,&faces,sectors,1.1,&pivot)&&edModelsEqual(&out,&back));
    puts("PASS: face/sector scaling, fixed pivots, inverse scale, invalid geometry and spline protection");
}
static void mirrorTests(void){
    EdModel base,clip,placed,mirrored,moved,changed,result;edDefault(&base);edDefault(&clip);EdBuffer<unsigned char> selected;
    CHECK(edMapPaste(&placed,&base,&clip,edV(4,4,4),selected));int mirrorOK=edMirrorEdit(&mirrored,&base,&placed,7);CHECK(mirrorOK&&mirrored.count==9&&edValidate(&mirrored));
    for(int mask=0;mask<8;mask++){EdVec center=edMirrorPoint(edMapCenter(&placed.c[1]),mask);int found=0;for(int s=0;s<mirrored.count;s++)found|=edContains(&mirrored.c[s],center);CHECK(found);}
    selected.clear();selected[1]=1;CHECK(edMapTranslate(&moved,&mirrored,selected,edV(1,0,0),0,selected));CHECK(edMirrorEdit(&result,&mirrored,&moved,7)&&result.count==9&&edValidate(&result));
    EdVec target=edMapCenter(&moved.c[1]);for(int mask=0;mask<8;mask++){int found=0;for(int s=0;s<result.count;s++)found|=edContains(&result.c[s],edMirrorPoint(target,mask));CHECK(found);}
    selected.clear();selected[1]=1;CHECK(edMapDelete(&changed,&mirrored,selected));CHECK(edMirrorEdit(&result,&mirrored,&changed,7)&&result.count==1);
    selected.clear();selected[1]=1;CHECK(edMapCleave(&changed,&mirrored,selected,edV(4,2,0),edV(4,6,0),selected));mirrorOK=edMirrorEdit(&result,&mirrored,&changed,7);CHECK(mirrorOK&&result.count==17&&edValidate(&result));
    // An unchanged/self-symmetric room must never be duplicated.
    CHECK(edMirrorEdit(&result,&base,&base,7)&&result.count==1);
    CHECK(edMapPaste(&placed,&base,&clip,edV(4,0,0),selected));CHECK(edMirrorEdit(&result,&base,&placed,3)&&result.count==3);
    // Mirrored placement may touch/overlap existing sectors; resolve it once.
    CHECK(edMapPaste(&placed,&base,&clip,edV(1.5,0,0),selected));CHECK(edMirrorEdit(&result,&base,&placed,1)&&edValidate(&result));
    for(double x=-2.4;x<2.5;x+=.2){int left=0,right=0;for(int c=0;c<result.count;c++){left|=edContains(&result.c[c],edV(x,0,.5));right|=edContains(&result.c[c],edV(-x,0,.5));}CHECK(left==right);}
    uint32_t n;unsigned char *wire=edWireEncode(&result,&n);CHECK(wire&&edWireDecode(&changed,wire,n)&&edModelsEqual(&changed,&result));edFree(wire);
    puts("PASS: map mirror combinations, movement, deletion, cleaves, self-symmetry and wire round trip");
}
static const char *paintTestName(int){return "red.mat";}
static int paintTestImage(const char *name,EdPaintImage *image){
    image->width=image->height=1;image->rgba={0,0,0,255};
    image->rgba[!strcmp(name,"red.mat")?0:!strcmp(name,"green.mat")?1:2]=255;return 1;
}
static void surfaceSessionTests(){
    auto base=std::make_unique<EdModel>(),local=std::make_unique<EdModel>(),remote=std::make_unique<EdModel>();edDefault(base.get());*local=*remote=*base;
    EdPaintImage red,green;paintTestImage("red.mat",&red);paintTestImage("green.mat",&green);
    auto key=edSurfaceKey(base->c[0].f[0]),other=edSurfaceKey(base->c[0].f[1]);
    CHECK(edPlaceDecal(local.get(),&local->c[0].f[0],edV(0,0,0),1,&red,&green,.5));
    EdVec center={};for(int v=0;v<4;v++)center=edAdd(center,remote->c[0].f[1].v[v].p);center=edScale(center,.25);
    CHECK(edPlaceDecal(remote.get(),&remote->c[0].f[1],center,1,&green,&red,.4));
    auto concurrent=std::make_unique<EdModel>(*remote);
    CHECK(!edTerrainSame(base.get(),local.get(),key)&&edTerrainSame(base.get(),remote.get(),key));
    CHECK(edTerrainOverlay(remote.get(),local.get(),key)&&edValidate(remote.get()));
    CHECK(edTerrainSame(remote.get(),local.get(),key)&&edTerrainSame(remote.get(),concurrent.get(),other)&&remote->decalCount==2);
    CHECK(edTerrainOverlay(remote.get(),base.get(),key)&&edModelsEqual(remote.get(),concurrent.get()));
    local->c[0].f[0].v[0].p.x+=.01;CHECK(!edTerrainOverlay(remote.get(),local.get(),key)&&edModelsEqual(remote.get(),concurrent.get()));
    puts("PASS: surface paint sessions preserve concurrent textures, normalize material IDs, hide previews and reject changed geometry");
}
static void decalTests(void){
    EdModel m;edDefault(&m);EdPaintImage red,green,image;paintTestImage("red.mat",&red);paintTestImage("green.mat",&green);
    EdFace &face=m.c[0].f[0];EdVec center=edV(0,0,0);EdFace outline;
    CHECK(edDecalOutline(&face,center,.5,1,&outline)&&outline.count==4);
    CHECK(edPlaceDecal(&m,&face,center,.5,&red,&green)&&edValidate(&m)&&m.decalCount==1);
    CHECK(edDecalImage(&m,face.decalId,&image));int w=image.width,h=image.height;
    CHECK(image.rgba[((h/2)*w+w/2)*4+1]>240&&image.rgba[0]>240);
    EdModel before=m;auto saved=m.decals[0].data;auto bytes=*saved;
    CHECK(edPlaceDecal(&m,&m.c[0].f[0],edV(.75,0,0),.5,&image,&green)&&m.decalCount==1);
    CHECK(*before.decals[0].data==bytes&&before.decals[0].data==saved&&!edModelsEqual(&before,&m));
    CHECK(edDecalOutline(&face,edV(1,0,0),.5,1,&outline));for(int i=0;i<outline.count;i++)CHECK(outline.v[i].p.x<=1.000001);
    unsigned size;unsigned char *wire=edWireEncode(&m,&size);EdModel decoded;CHECK(wire&&edWireDecode(&decoded,wire,size)&&edModelsEqual(&m,&decoded));edFree(wire);
    auto bad=std::make_shared<std::vector<unsigned char>>(*decoded.decals[0].data);(*bad)[100]=1;decoded.decals[0].data=bad;CHECK(!edValidate(&decoded));
    EdModel transparent;edDefault(&transparent);green.rgba[3]=0;CHECK(!edPlaceDecal(&transparent,&transparent.c[0].f[0],center,.5,&red,&green));
    green.rgba[3]=128;CHECK(edPlaceDecal(&transparent,&transparent.c[0].f[0],center,.5,&red,&green));CHECK(edDecalImage(&transparent,1,&image));size_t pixel=((image.height/2)*image.width+image.width/2)*4;CHECK(image.rgba[pixel]>115&&image.rgba[pixel]<140&&image.rgba[pixel+1]>115&&image.rgba[pixel+1]<140);
    char resources[512];snprintf(resources,sizeof(resources),"SECTION: MATERIALS\nWorld materials 2\n0: red.mat 1 1\n1: %s 1 1\nend\n",m.decals[0].name);m.c[0].f[0].material=1;
    CHECK(edWriteGob(&m,"test-decal.gob","Decal",0,resources));FILE *file=fopen("test-decal.gob","rb");CHECK(file);fseek(file,0,SEEK_END);long length=ftell(file);rewind(file);std::vector<unsigned char> gob(length);CHECK(fread(gob.data(),1,length,file)==length);fclose(file);bool found=false;
    unsigned dir=edNetLoad32(gob.data()+8),entries=edNetLoad32(gob.data()+dir);for(unsigned i=0;i<entries;i++){const unsigned char *entry=gob.data()+dir+4+i*136;if(strstr((const char*)entry+8,m.decals[0].name)){unsigned offset=edNetLoad32(entry),count=edNetLoad32(entry+4);CHECK(count==m.decals[0].data->size()&&!memcmp(gob.data()+offset,m.decals[0].data->data(),count));found=true;}}CHECK(found);remove("test-decal.gob");
    m.c[0].f[0].decalId=0;edCompactDecals(&m);CHECK(m.decalCount==0);
    puts("PASS: clipped decals, RGB565 compositing, immutable undo, network serialization and GOB assets");
}
static void surfacePaintTests(void){
    EdModel m;edDefault(&m);EdModel before=m;EdPaintImage red,green,image;paintTestImage("red.mat",&red);paintTestImage("green.mat",&green);
    CHECK(edPlaceDecal(&m,&m.c[0].f[0],edV(0,0,0),1,&red,&green,.5));CHECK(edValidate(&m)&&edSurfacePaintOnlyChange(&before,&m));
    CHECK(edDecalImage(&m,1,&image));auto sample=[&](double x){int px=(int)(2.5+(x+1)/2*(image.width-5));return image.rgba[((image.height/2)*image.width+px)*4+1];};
    CHECK(sample(0)>115&&sample(0)<140&&sample(.25)>sample(.4)&&sample(.4)>sample(.6)&&sample(.6)==0);
    EdModel changed=m;changed.c[0].f[0].v[0].p.x+=.1;CHECK(!edSurfacePaintOnlyChange(&m,&changed));changed=m;changed.c[0].extraLight+=.1;CHECK(!edSurfacePaintOnlyChange(&m,&changed));
    EdModel fresh=m;fresh.c[0].f[0].decalId=0;CHECK(edPlaceDecal(&fresh,&fresh.c[0].f[0],edV(1,0,0),1,&image,&green,.5));
    before=m;CHECK(edPlaceDecal(&m,&m.c[0].f[0],edV(1,0,0),1,&image,&green,.5)&&edSurfacePaintOnlyChange(&before,&m));CHECK(*m.decals[0].data==*fresh.decals[0].data);CHECK(edDecalImage(&m,1,&image));CHECK(image.rgba[((image.height/2)*image.width+image.width-3)*4+1]>100);
    // A tiled pattern stays anchored to the face, not to the brush center.
    EdPaintImage pattern;pattern.width=2;pattern.height=1;pattern.rgba={0,255,0,255,0,0,255,255};
    EdModel a,b;edDefault(&a);b=a;CHECK(edPlaceDecal(&a,&a.c[0].f[0],edV(-.1,0,0),1,&red,&pattern,.8));CHECK(edPlaceDecal(&b,&b.c[0].f[0],edV(.1,0,0),1,&red,&pattern,.8));EdPaintImage ia,ib;CHECK(edDecalImage(&a,1,&ia)&&edDecalImage(&b,1,&ib));for(int x=ia.width/2-8;x<ia.width/2+8;x++){size_t p=((ia.height/2)*ia.width+x)*4;CHECK((ia.rgba[p+1]>=ia.rgba[p+2])==(ib.rgba[p+1]>=ib.rgba[p+2]));}
    unsigned size;unsigned char *wire=edWireEncode(&m,&size);EdModel decoded;CHECK(wire&&edWireDecode(&decoded,wire,size)&&edModelsEqual(&m,&decoded));edFree(wire);
    puts("PASS: surface paint falloff, edge painting, anchored tiling and geometry-preserving updates");
}
static void terrainPaintTests(void){
    EdModel source,m,backup,decoded;edDefault(&source);EdSelection selection={};selection.count=1;selection.hits[0]={0,0,-1,-1};
    CHECK(edTerrainCreate(&m,&source,&selection,"paint_test"));backup=m;
    CHECK(edTerrainPaint(&m,0,edV(0,0,.002),.75,.5,"green.mat",paintTestName));CHECK(edValidate(&m)&&!edModelsEqual(&m,&backup));
    for(int i=0;i<m.meshCount;i++)for(int f=0;f<m.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){
        EdVec p=m.meshes[i].geometry.f[f].v[v].p;double distance=sqrt(p.x*p.x+p.y*p.y)/.75;
        double expected=distance<1?.5*pow(1-distance*distance,2):0;
        CHECK(fabs(m.meshes[i].paint[1].weights[f*3+v]-expected)<1e-8);
    }
    backup=m;snprintf(m.assets[0].paintSlots[1],128,"blue.mat");
    CHECK(edTerrainPaint(&m,0,edV(.5,0,.002),.4,.5,"blue.mat",paintTestName));CHECK(m.meshes[0].paint.size()==3);
    // A changed slot adds a new layer, rather than recoloring old green paint.
    for(int i=0;i<m.meshCount;i++)for(int f=0;f<m.meshes[i].geometry.count;f++)for(int v=0;v<3;v++)if(m.meshes[i].geometry.f[f].v[v].p.x<0)CHECK(m.meshes[i].paint[1].weights[f*3+v]==backup.meshes[i].paint[1].weights[f*3+v]);
    uint32_t n;unsigned char *wire=edWireEncode(&m,&n);CHECK(wire&&edWireDecode(&decoded,wire,n)&&edModelsEqual(&decoded,&m));edFree(wire);
    decoded.meshes[0].paint[0].weights[0]=-1;CHECK(!edMeshValidate(&decoded));
    edPaintLoadImage=paintTestImage;EdPaintBake bake;CHECK(edPaintBakeAsset(&m,0,&bake)&&bake.mat.size()==140+(size_t)bake.size*bake.size*2&&edNetLoad32(bake.mat.data()+24)==16);
    // Incremental output must be byte-identical to a fresh bake, including
    // undo, new layers, texture changes and UV changes.
    EdPaintBake cached=bake,fresh;CHECK(edPaintBakeAsset(&m,0,&cached,1)&&cached.updatedFaces==0);
    EdModel painted=m;CHECK(edTerrainPaint(&painted,0,edV(0,0,.002),.26,.25,"blue.mat",paintTestName));
    CHECK(edPaintOnlyChange(&m,&painted));EdModel moved=painted;moved.c[0].extraLight+=.1;CHECK(!edPaintOnlyChange(&m,&moved));
    auto begin=std::chrono::steady_clock::now();CHECK(edPaintBakeAsset(&painted,0,&fresh));auto fullEnd=std::chrono::steady_clock::now();
    CHECK(edPaintBakeAsset(&painted,0,&cached,1));auto partialEnd=std::chrono::steady_clock::now();
    CHECK(cached.mat==fresh.mat&&cached.updatedFaces>0&&cached.updatedFaces<(int)cached.faceHashes.size()/2);
    printf("PAINT BAKE: full %.2f ms, incremental %.2f ms (%d/%zu faces)\n",std::chrono::duration<double,std::milli>(fullEnd-begin).count(),std::chrono::duration<double,std::milli>(partialEnd-fullEnd).count(),cached.updatedFaces,cached.faceHashes.size());
    CHECK(edPaintBakeAsset(&m,0,&cached,1)&&cached.mat==bake.mat); // undo
    moved=m;moved.meshes[0].geometry.f[0].v[0].u+=.5;
    CHECK(edPaintBakeAsset(&moved,0,&fresh)&&edPaintBakeAsset(&moved,0,&cached,1)&&cached.mat==fresh.mat&&cached.updatedFaces==1);
    int vertex=0;for(int i=0;i<m.meshCount;i++)for(int f=0;f<m.meshes[i].geometry.count;f++)for(int v=0;v<3;v++,vertex++){
        EdVec uv=edScale(bake.uv[vertex],(double)bake.size/std::min(bake.size,256));size_t offset=140+((size_t)(int)uv.y*bake.size+(int)uv.x)*2;unsigned pixel=bake.mat[offset]|unsigned(bake.mat[offset+1])<<8;
        unsigned red=(unsigned)(m.meshes[i].paint[0].weights[f*3+v]*255+.5)>>3;
        unsigned green=(unsigned)(m.meshes[i].paint[1].weights[f*3+v]*255+.5)>>2;
        unsigned blue=(unsigned)(m.meshes[i].paint[2].weights[f*3+v]*255+.5)>>3;CHECK(pixel==(red<<11|green<<5|blue));
    }
    const char *resources="SECTION: MATERIALS\nWorld materials 1\n0: red.mat 1 1\nend\n";
    CHECK(edWriteGob(&m,"test-paint.gob","Paint",0,resources));FILE *file=fopen("test-paint.gob","rb");CHECK(file);fseek(file,0,SEEK_END);long size=ftell(file);rewind(file);std::vector<unsigned char> bytes(size);CHECK(fread(bytes.data(),1,size,file)==size);fclose(file);
    unsigned dir=edNetLoad32(bytes.data()+8),entries=edNetLoad32(bytes.data()+dir);bool mat=false,meta=false,model=false;char name[32];edPaintFilename(&m,0,name);
    for(unsigned i=0;i<entries;i++){const unsigned char *entry=bytes.data()+dir+4+i*136;unsigned offset=edNetLoad32(entry),count=edNetLoad32(entry+4);CHECK((size_t)offset+count<=bytes.size());const char *path=(const char*)entry+8;
        if(strstr(path,name)){CHECK(count==bake.mat.size()&&!memcmp(bytes.data()+offset,bake.mat.data(),count));mat=true;}
        if(!strcmp(path,"misc\\geosith_models.bin")){edDefault(&decoded);CHECK(edReadMeshData(&decoded,bytes.data()+offset,count)&&edPaintHash(&decoded,0)==edPaintHash(&m,0)&&!strcmp(decoded.assets[0].paintSlots[1],"blue.mat"));meta=true;}
        if(!strcmp(path,"3do\\paint_test.3do")){std::string text((const char*)bytes.data()+offset,count);CHECK(text.find(name)!=std::string::npos&&text.find("MATERIALS 1")!=std::string::npos);model=true;}
    }CHECK(mat&&meta&&model);remove("test-paint.gob");edPaintLoadImage=NULL;
}
static void textureStitchTests(void){
    EdFace source={},target={};source.count=target.count=4;source.material=2;target.material=7;
    EdVec points[]={edV(0,0,0),edV(1,0,0),edV(1,1,0),edV(0,1,0)};
    for(int v=0;v<4;v++){source.v[v].p=points[v];source.v[v].u=13+points[v].x*80+points[v].y*30;source.v[v].v=-7+points[v].x*20-points[v].y*100;target.v[v].p=edAdd(points[v],edV(1,0,0));}
    CHECK(edStitchTexture(&target,&source)&&target.material==7&&target.customUV);
    for(int v=0;v<4;v++){EdVec p=target.v[v].p;CHECK(fabs(target.v[v].u-(13+p.x*80+p.y*30))<1e-8);CHECK(fabs(target.v[v].v-(-7+p.x*20-p.y*100))<1e-8);}
    EdVec wall[]={edV(1,0,0),edV(1,0,1),edV(1,1,1),edV(1,1,0)};
    for(int v=0;v<4;v++)target.v[v].p=wall[v];CHECK(edStitchTexture(&target,&source));
    for(int v=0;v<4;v++){EdVec p=wall[v];CHECK(fabs(target.v[v].u-(13+(1+p.z)*80+p.y*30))<1e-8);CHECK(fabs(target.v[v].v-(-7+(1+p.z)*20-p.y*100))<1e-8);}
    // Cleaved surfaces can have collinear first corners.
    source.count=5;source.v[4]=source.v[3];source.v[3]=source.v[2];source.v[2]=source.v[1];source.v[1].p=edV(.5,0,0);source.v[1].u=53;source.v[1].v=3;
    CHECK(edStitchTexture(&target,&source)&&fabs(target.v[1].u-173)<1e-8);
    puts("PASS: texture stitching, coplanar tiling, angled seam, scale/rotation and collinear corners");
}
static void terrainGrowthTests(void){
    EdModel source,result,decoded;edDefault(&source);EdSelection selection={};selection.count=1;selection.hits[0]={0,0,-1,-1};
    for(int i=0;i<24;i++){char name[32];snprintf(name,sizeof(name),"growth%d",i);CHECK(edTerrainCreate(&result,&source,&selection,name));source=result;}
    CHECK(source.assetCount==24&&source.meshCount>=24&&edValidate(&source));
    unsigned size=0;unsigned char *wire=edWireEncode(&source,&size);CHECK(wire&&edWireDecode(&decoded,wire,size)&&edModelsEqual(&source,&decoded));edFree(wire);
    // Malformed large counts cannot allocate based on a tiny metadata payload.
    unsigned char invalid[16]={'G','3','D','O',7};edNetStore32(invalid+8,INT_MAX);CHECK(!edReadMeshData(&decoded,invalid,sizeof(invalid)));
    puts("PASS: 24 terrain assets, growing meshes, network roundtrip and bounded malformed metadata");
}
static void terrainTests(void){
    EdModel source,terrain,before,decoded;edDefault(&source);EdSelection surfaces={};surfaces.count=1;surfaces.hits[0]={0,0,-1,-1};
    CHECK(edTerrainCreate(&terrain,&source,&surfaces,"terrain_test")&&edValidate(&terrain));CHECK(terrain.assetCount==1&&terrain.assets[0].terrain&&terrain.assets[0].terrainNormal.z>.999);
    int triangles=0,pinned=0,free=0;double area=0;for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++){const EdFace &face=terrain.meshes[i].geometry.f[f];CHECK(face.count==3);triangles++;EdVec cross=edCross(edSub(face.v[1].p,face.v[0].p),edSub(face.v[2].p,face.v[0].p));area+=sqrt(edDot(cross,cross))*.5;for(int v=0;v<3;v++)if(terrain.meshes[i].terrainPinned[f*3+v])pinned++;else free++;}
    CHECK(triangles==128&&free&&fabs(area-4)<1e-8);before=terrain;
    CHECK(!edTerrainSmooth(&terrain,0,edV(0,0,0),.75,.5)&&edModelsEqual(&terrain,&before));
    CHECK(edTerrainSculpt(&terrain,0,edV(0,0,.002),.75,.2)&&edValidate(&terrain));double maxHeight=0;
    for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){const EdVertex &vertex=terrain.meshes[i].geometry.f[f].v[v];maxHeight=fmax(maxHeight,vertex.p.z);if(terrain.meshes[i].terrainPinned[f*3+v])CHECK(vertex.p.z==0);CHECK(isfinite(vertex.u)&&isfinite(vertex.v));}
    CHECK(fabs(maxHeight-.202)<1e-8);CHECK(edTerrainSculpt(&terrain,0,edV(0,0,0),.75,-.1)&&edValidate(&terrain));
    int below=0;for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int v=0;v<3;v++)below|=terrain.meshes[i].geometry.f[f].v[v].p.z<-.1;CHECK(!below);
    // Outer-edge vertices from both newly created and previously saved terrain
    // must sculpt even when their old boundary marker is set.
    CHECK(edTerrainSculpt(&terrain,0,edV(1,0,.002),.5,.2,edV(0,0,1)));int movedEdge=0;
    for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){EdVec p=terrain.meshes[i].geometry.f[f].v[v].p;if(fabs(p.x-1)<1e-8&&fabs(p.y)<1e-8){CHECK(fabs(p.z-.202)<1e-8);movedEdge++;}}CHECK(movedEdge>0);
    uint32_t size;unsigned char *wire=edWireEncode(&terrain,&size);CHECK(wire&&edWireDecode(&decoded,wire,size)&&edModelsEqual(&terrain,&decoded));edFree(wire);
    const char *resources="SECTION: MATERIALS\nWorld materials 1\n0: dflt.mat 1 1\nend\n";
    CHECK(edWriteGob(&terrain,"test-terrain.gob","Terrain",0,resources));FILE *file=fopen("test-terrain.gob","rb");CHECK(file);fseek(file,0,SEEK_END);long bytes=ftell(file);rewind(file);unsigned char *data=edAlloc(bytes);CHECK(data&&fread(data,1,bytes,file)==bytes);fclose(file);uint32_t dir=edNetLoad32(data+8),count=edNetLoad32(data+dir);int meshFound=0,metaFound=0;
    for(uint32_t i=0;i<count;i++){unsigned char *entry=data+dir+4+i*136;uint32_t offset=edNetLoad32(entry),n=edNetLoad32(entry+4);CHECK(offset+n<=bytes);if(!strcmp((char*)entry+8,"3do\\terrain_test.3do"))meshFound=1;if(!strcmp((char*)entry+8,"misc\\geosith_models.bin")){edDefault(&decoded);CHECK(edReadMeshData(&decoded,data+offset,n)&&decoded.assets[0].terrain&&decoded.assets[0].terrainNormal.z>.999&&decoded.meshes[0].terrainPinned.size()==terrain.meshes[0].terrainPinned.size());metaFound=1;}}
    CHECK(meshFound&&metaFound);edFree(data);remove("test-terrain.gob");
    // Multiple coplanar surfaces share unpinned interior seams.
    CHECK(edCleave(&source,0,0,edV(0,-1,0),edV(0,1,0)));surfaces.count=0;for(int f=0;f<source.c[0].count;f++)if(edNormal(&source.c[0].f[f]).z>.99)surfaces.hits[surfaces.count++]={0,f,-1,-1};CHECK(surfaces.count==2&&edTerrainCreate(&terrain,&source,&surfaces,"joined"));
    CHECK(edTerrainSculpt(&terrain,0,edV(0,0,.002),.5,.1,edV(0,0,1)));int seamMoved=0;for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){EdVec p=terrain.meshes[i].geometry.f[f].v[v].p;if(fabs(p.x)<1e-9&&fabs(p.y)<1e-9){CHECK(fabs(p.z-.102)<1e-8);seamMoved++;}}CHECK(seamMoved>1);
    before=terrain;surfaces.hits[1]={-1,1,-1,-1};CHECK(!edTerrainCreate(&terrain,&source,&surfaces,"invalid")&&edModelsEqual(&terrain,&before));
    edDefault(&source);CHECK(edCleave(&source,0,0,edV(.13,-1,0),edV(.13,1,0)));surfaces.count=0;for(int f=0;f<source.c[0].count;f++)if(edNormal(&source.c[0].f[f]).z>.99)surfaces.hits[surfaces.count++]={0,f,-1,-1};CHECK(edTerrainCreate(&terrain,&source,&surfaces,"offgrid_seam"));
    for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int e=0;e<3;e++){
        EdVec a=terrain.meshes[i].geometry.f[f].v[e].p,b=terrain.meshes[i].geometry.f[f].v[(e+1)%3].p,d=edSub(b,a);
        for(int j=0;j<terrain.meshCount;j++)for(int g=0;g<terrain.meshes[j].geometry.count;g++)for(int v=0;v<3;v++){EdVec p=terrain.meshes[j].geometry.f[g].v[v].p;double t=edDot(edSub(p,a),d)/edDot(d,d);if(t>1e-7&&t<1-1e-7){EdVec gap=edSub(p,edAdd(a,edScale(d,t)));CHECK(edDot(gap,gap)>1e-16);}}
    }
    // Sloped, non-grid-aligned faces on a scaled room with an adjoining room.
    // Creation must preserve all sector geometry, topology and attributes.
    edDefault(&source);CHECK(edExtrude(&source,0,4,edV(0,1,0)));
    for(int c=0;c<source.count;c++)for(int f=0;f<source.c[c].count;f++)for(int v=0;v<source.c[c].f[f].count;v++){
        EdVec &p=source.c[c].f[f].v[v].p;p.x*=1.37;p.y*=.83;p.z=1.21*p.z+.31*p.y;
    }
    CHECK(edValidate(&source));
    for(int face=0;face<source.c[0].count;face++)if(source.c[0].f[face].neighbor<0){
        surfaces.count=1;surfaces.hits[0]={0,face,-1,-1};
        CHECK(edTerrainCreate(&terrain,&source,&surfaces,"angled")&&edValidate(&terrain));
        EdModel geometryOnly=terrain;geometryOnly.assetCount=source.assetCount;geometryOnly.meshCount=source.meshCount;CHECK(edModelsEqual(&geometryOnly,&source));
        for(int c=0;c<source.count;c++)for(int f=0;f<source.c[c].count;f++)for(int v=0;v<source.c[c].f[f].count;v++){
            EdVec p=source.c[c].f[f].v[v].p;int found=0;
            for(int g=0;g<terrain.c[c].count;g++)for(int w=0;w<terrain.c[c].f[g].count;w++){
                EdVec d=edSub(p,terrain.c[c].f[g].v[w].p);found|=edDot(d,d)<1e-16;
            }CHECK(found);
        }
    }
    // Joined sectors produce separate assets, but their common edge is welded.
    edDefault(&source);CHECK(edExtrude(&source,0,4,edV(0,1,0)));surfaces.count=0;
    for(int c=0;c<source.count;c++)for(int f=0;f<source.c[c].count;f++)if(edNormal(&source.c[c].f[f]).z>.99)surfaces.hits[surfaces.count++]={c,f,-1,-1};
    CHECK(edTerrainCreate(&terrain,&source,&surfaces,"multi_sector")&&terrain.assetCount==2);
    CHECK(terrain.assets[0].terrainGroup==terrain.assets[1].terrainGroup);before=terrain;
    for(int stroke=0;stroke<4;stroke++)CHECK(edTerrainSculpt(&terrain,stroke%2,edV(0,1,.002+.05*stroke),.7,.05,edV(0,0,1)));
    int shared[2]={};EdVec seam={};
    for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){
        EdVec old=before.meshes[i].geometry.f[f].v[v].p,p=terrain.meshes[i].geometry.f[f].v[v].p;
        if(fabs(old.x)<1e-8&&fabs(old.y-1)<1e-8){if(shared[0]+shared[1])CHECK(edDot(edSub(p,seam),edSub(p,seam))<1e-20);seam=p;shared[terrain.meshes[i].asset]++;}
    }CHECK(shared[0]&&shared[1]&&seam.z>.1);
    before=terrain;EdVec brush=seam;CHECK(edTerrainSmooth(&terrain,1,brush,.6,.5)&&edMeshValidate(&terrain));shared[0]=shared[1]=0;
    for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){
        EdVec old=before.meshes[i].geometry.f[f].v[v].p,p=terrain.meshes[i].geometry.f[f].v[v].p;

        EdVec d=edSub(old,brush);if(edDot(d,d)>=.36)CHECK(edDot(edSub(old,p),edSub(old,p))<1e-20);
        if(fabs(old.x)<1e-8&&fabs(old.y-1)<1e-8){if(shared[0]+shared[1])CHECK(edDot(edSub(p,seam),edSub(p,seam))<1e-20);seam=p;shared[terrain.meshes[i].asset]++;}
    }CHECK(shared[0]&&shared[1]&&seam.z<brush.z);
    before=terrain;CHECK(!edTerrainSmooth(&terrain,0,brush,.5,-1)&&edModelsEqual(&terrain,&before));

    wire=edWireEncode(&terrain,&size);CHECK(wire&&edWireDecode(&decoded,wire,size)&&edModelsEqual(&terrain,&decoded));edFree(wire);
    // Floor and wall share a bisector displacement rather than opening a crack.
    edDefault(&source);surfaces.count=2;surfaces.hits[0]={0,0,-1,-1};surfaces.hits[1]={0,4,-1,-1};
    CHECK(edTerrainCreate(&terrain,&source,&surfaces,"corner")&&terrain.assetCount==1);before=terrain;
    CHECK(edTerrainSculpt(&terrain,0,edV(0,1,0),.5,.1,edV(0,-.7071067811865475,.7071067811865475)));int copies=0;
    for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){
        EdVec old=before.meshes[i].geometry.f[f].v[v].p,p=terrain.meshes[i].geometry.f[f].v[v].p;
        if(fabs(old.x)<1e-8&&fabs(old.y-(1-.002/sqrt(2.0)))<1e-8&&fabs(old.z-.002/sqrt(2.0))<1e-8){if(copies++)CHECK(edDot(edSub(p,seam),edSub(p,seam))<1e-20);seam=p;}
    }CHECK(copies>1&&seam.y<1&&seam.z>0);
    // Raised initial vertices and changing face normals, not stored base normals.
    edDefault(&source);surfaces.count=1;surfaces.hits[0]={0,0,-1,-1};CHECK(edTerrainCreate(&terrain,&source,&surfaces,"brushes"));
    for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){
        EdVec &p=terrain.meshes[i].geometry.f[f].v[v].p;CHECK(fabs(p.z-.002)<1e-9);p.z+=.3+.2*p.x;
    }
    before=terrain;CHECK(edTerrainSculpt(&terrain,0,edV(0,0,.302),.5,.05));int tilted=0;
    for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){
        EdVec old=before.meshes[i].geometry.f[f].v[v].p,p=terrain.meshes[i].geometry.f[f].v[v].p;
        if(fabs(old.x)<1e-9&&fabs(old.y)<1e-9){CHECK(p.x<-.005&&p.z>old.z);tilted++;}
    }CHECK(tilted);
    // Grab weights come from the original snapshot and allow all three axes.
    before=terrain;EdVec brushCenter=edV(0,0,.302),drag=edV(.1,.08,.05);CHECK(edTerrainGrab(&terrain,&before,0,brushCenter,.7,drag));int grabbed=0;
    for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){
        EdVec old=before.meshes[i].geometry.f[f].v[v].p,p=terrain.meshes[i].geometry.f[f].v[v].p;double r=sqrt(edDot(edSub(old,brushCenter),edSub(old,brushCenter)))/.7;
        EdVec expected=r<1?edAdd(old,edScale(drag,(1-r*r)*(1-r*r))):old;CHECK(edDot(edSub(p,expected),edSub(p,expected))<1e-20);grabbed+=r<.5;
    }CHECK(grabbed);CHECK(edTerrainGrab(&terrain,&before,0,brushCenter,.7,edV(0,0,0))&&edModelsEqual(&terrain,&before));
    // Source texture offsets do not create seams in a linked terrain UV chart.
    edDefault(&source);CHECK(edExtrude(&source,0,4,edV(0,1,0)));surfaces.count=0;
    for(int c=0;c<source.count;c++)for(int f=0;f<source.c[c].count;f++)if(edNormal(&source.c[c].f[f]).z>.99){source.c[c].f[f].uOffset=1234*c;source.c[c].f[f].vOffset=-543*c;edMapFace(&source.c[c].f[f]);surfaces.hits[surfaces.count++]={c,f,-1,-1};}
    CHECK(edTerrainCreate(&terrain,&source,&surfaces,"uv_group"));
    for(int pass=0;pass<4;pass++){
        if(pass==1)CHECK(edTerrainSculpt(&terrain,0,edV(0,1,.002),.8,.3,edV(0,0,1)));
        if(pass==2)CHECK(edTerrainSmooth(&terrain,1,edV(0,1,.3),.8,.4));
        if(pass==3){before=terrain;CHECK(edTerrainGrab(&terrain,&before,0,edV(0,1,.2),.8,edV(.1,0,.1)));}
        int seamPairs=0;
        for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){
            EdVertex a=terrain.meshes[i].geometry.f[f].v[v];
            for(int j=i+1;j<terrain.meshCount;j++)for(int g=0;g<terrain.meshes[j].geometry.count;g++)for(int w=0;w<3;w++){
                EdVertex b=terrain.meshes[j].geometry.f[g].v[w];if(edDot(edSub(a.p,b.p),edSub(a.p,b.p))<1e-16){CHECK(fabs(a.u-b.u)<1e-9&&fabs(a.v-b.v)<1e-9);seamPairs++;}
            }
        }CHECK(seamPairs);
        double relaxedError=0,projectedError=0;
        for(int i=0;i<terrain.meshCount;i++)for(int f=0;f<terrain.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){
            EdVertex a=terrain.meshes[i].geometry.f[f].v[v],b=terrain.meshes[i].geometry.f[f].v[(v+1)%3];EdVec d=edSub(a.p,b.p);double target=sqrt(edDot(d,d)),uv=hypot(a.u-b.u,a.v-b.v)/ED_UV_SCALE;
            relaxedError+=(uv-target)*(uv-target);double projected=hypot(d.x,d.y);projectedError+=(projected-target)*(projected-target);
        }CHECK(relaxedError<=projectedError+1e-9);
    }
    puts("PASS: terrain subdivisions, coplanar unions, pinned borders, normal-axis falloff, unchanged backing geometry, mesh export and editable metadata");
}
static void splineTests(void){
    EdModel original,out;edDefault(&original);EdSpline spline={};spline.id=1;spline.count=3;spline.sides=4;spline.segments=8;spline.width=1;spline.height=1;
    spline.points[0]={edV(4,0,.5),edV(0,1,0)};spline.points[1]={edV(4,3,.5),edV(1,1,0)};spline.points[2]={edV(7,4,.5),edV(1,0,0)};
    if(!edSplineRebuild(&out,&original,&spline)||!edValidate(&out)||out.splineCount!=1||out.count!=9){fprintf(stderr,"Spline generation failed\n");{fprintf(stderr,"Spline test line %d\n",__LINE__);exit(2);}}
    for(int i=1;i<out.count;i++){int adjoins=0;for(int f=0;f<out.c[i].count;f++)adjoins+=out.c[i].f[f].neighbor>=0;if(!adjoins){fprintf(stderr,"Spline test line %d\n",__LINE__);exit(2);}}
    int wrapped=0;double minV=1e30,maxV=-1e30;
    for(int c=1;c<out.count;c++)for(int f=0;f<out.c[c].count;f++)if(out.c[c].f[f].customUV){wrapped++;for(int v=0;v<out.c[c].f[f].count;v++){minV=fmin(minV,out.c[c].f[f].v[v].v);maxV=fmax(maxV,out.c[c].f[f].v[v].v);}}
    CHECK(wrapped==32&&fabs(minV)<1e-8&&fabs(maxV/64-round(maxV/64))<1e-8);
    for(double pitch : {-.5,.5}){
        EdSpline ramp=spline;ramp.count=2;ramp.points[0]={edV(14,0,1),edV(0,1,pitch)};ramp.points[1]={edV(14,5,1+pitch*3),edV(.2,1,pitch)};
        EdModel generated;CHECK(edSplineRebuild(&generated,&original,&ramp)&&edValidate(&generated));
        for(int end : {0,1}){int matched=0;EdVec tangent=ramp.points[end].handle,center=ramp.points[end].position;
            double len=sqrt(edDot(tangent,tangent));tangent=edScale(tangent,1/len);
            for(int c=1;c<generated.count;c++)for(int f=0;f<generated.c[c].count;f++){
                const EdFace &face=generated.c[c].f[f];bool cap=face.neighbor<0;
                for(int v=0;v<face.count;v++)cap=cap&&fabs(edDot(tangent,edSub(face.v[v].p,center)))<1e-8;
                if(cap){CHECK(fabs(edDot(edNormal(&face),tangent))>.999999);matched++;}
            }CHECK(matched==1);
        }
    }
    // Every interior ring, not only the ends, stays perpendicular on slopes.
    for(EdVec tangent : {edV(0,1,.5),edV(.3,1,-.7),edV(0,0,1)}){
        EdSpline slope=spline;slope.count=2;slope.width=slope.height=.5;
        slope.points[0]={edV(20,0,3),tangent};slope.points[1]={edAdd(slope.points[0].position,edScale(tangent,5)),tangent};
        EdModel generated;CHECK(edSplineRebuild(&generated,&original,&slope)&&edValidate(&generated));int joins=0;
        EdVec normal=edScale(tangent,1/sqrt(edDot(tangent,tangent)));
        for(int c=1;c<generated.count;c++)for(int f=0;f<generated.c[c].count;f++)if(generated.c[c].f[f].neighbor>=0){CHECK(fabs(edDot(edNormal(&generated.c[c].f[f]),normal))>.999999);joins++;}
        CHECK(joins==2*(slope.segments-1));
    }
    EdModel uvSaved=out;edMapWorld(&out);CHECK(edModelsEqual(&out,&uvSaved));
    // Every shared corner has the same axial UV and wraps by whole texture tiles.
    for(int c=1;c<out.count;c++)for(int f=0;f<out.c[c].count;f++)if(out.c[c].f[f].customUV)for(int v=0;v<out.c[c].f[f].count;v++){
        EdVertex a=out.c[c].f[f].v[v];for(int d=c;d<out.count;d++)for(int g=0;g<out.c[d].count;g++)if(out.c[d].f[g].customUV)for(int w=0;w<out.c[d].f[g].count;w++){EdVertex b=out.c[d].f[g].v[w];EdVec delta=edSub(a.p,b.p);if(edDot(delta,delta)<1e-12)CHECK(fabs(a.u-b.u)<1e-8&&fabs((a.v-b.v)/64-round((a.v-b.v)/64))<1e-8);}}
    uint32_t size=0;unsigned char *data=edWireEncode(&out,&size);EdModel decoded;if(!data||!edWireDecode(&decoded,data,size)||!edModelsEqual(&out,&decoded)){fprintf(stderr,"Spline test line %d\n",__LINE__);exit(2);}edFree(data);
    EdModel moved;spline.points[1].position.z+=.25;if(!edSplineRebuild(&moved,&out,&spline)||!edValidate(&moved)||moved.splineCount!=1){fprintf(stderr,"Spline test line %d\n",__LINE__);exit(2);}
    edSplineReconcile(&moved);if(moved.splineCount!=1){fprintf(stderr,"Spline test line %d\n",__LINE__);exit(2);}
    moved.c[1].f[0].material++;edSplineReconcile(&moved);if(moved.splineCount!=1){fprintf(stderr,"Spline test line %d\n",__LINE__);exit(2);}
    EdSpline straight=spline;straight.sides=3;straight.count=2;straight.points[0]={edV(4,0,.5),edV(0,0,0)};straight.points[1]={edV(4,4,.5),edV(0,0,0)};
    CHECK(edSplineRebuild(&moved,&original,&straight));int quads=0;for(int c=1;c<moved.count;c++)for(int f=0;f<moved.c[c].count;f++)if(moved.c[c].f[f].customUV){CHECK(moved.c[c].f[f].count==4);quads++;}CHECK(quads==straight.segments*3);
    spline.sides=3;CHECK(edSplineRebuild(&moved,&original,&spline));int triangles=0;for(int c=1;c<moved.count;c++)for(int f=0;f<moved.c[c].count;f++)triangles+=moved.c[c].f[f].customUV&&moved.c[c].f[f].count==3;CHECK(triangles>0);
    spline.sides=8;if(!edSplineRebuild(&moved,&out,&spline)||!edValidate(&moved)){fprintf(stderr,"Spline test line %d\n",__LINE__);exit(2);}
    spline.sides=32;if(!edSplineRebuild(&moved,&out,&spline)||!edValidate(&moved)){fprintf(stderr,"Spline circle failed\n");exit(2);}
    EdModel saved=moved;CHECK(edWriteGob(&saved,"test-splines.gob","Spline test",0,"SECTION: HEADER\n"));
    FILE *file=fopen("test-splines.gob","rb");CHECK(file);fseek(file,0,SEEK_END);long bytes=ftell(file);rewind(file);char *text=edAlloc(bytes+1);CHECK(text&&fread(text,1,bytes,file)==bytes);fclose(file);text[bytes]=0;
    for(int c=0;c<moved.count;c++)for(int f=0;f<moved.c[c].count;f++)moved.c[c].f[f].customUV=0;CHECK(edReadTextureRotations(&moved,text+12));moved.splineCount=0;edSplineReadMetadata(&moved,text+12);edFree(text);remove("test-splines.gob");CHECK(edModelsEqual(&moved,&saved));
    moved.c[0].f[0].material++;edSplineReconcile(&moved);CHECK(moved.splineCount==1);
    saved=moved;
    spline.width=-1;if(edSplineRebuild(&moved,&out,&spline)){fprintf(stderr,"Spline test line %d\n",__LINE__);exit(2);}
    CHECK(edModelsEqual(&saved,&moved));
    EdSpline loop={};loop.id=2;loop.count=4;loop.closed=1;loop.sides=4;loop.segments=16;loop.width=loop.height=.5;
    loop.points[0]={edV(13,0,.5),edV(0,1.65,0)};loop.points[1]={edV(10,3,.5),edV(-1.65,0,0)};loop.points[2]={edV(7,0,.5),edV(0,-1.65,0)};loop.points[3]={edV(10,-3,.5),edV(1.65,0,0)};
    CHECK(edSplineRebuild(&moved,&original,&loop)&&moved.count==17&&edValidate(&moved));
    for(int c=1;c<moved.count;c++){int portals=0;for(int f=0;f<moved.c[c].count;f++)portals+=moved.c[c].f[f].neighbor>=0;CHECK(portals==2);}
    data=edWireEncode(&moved,&size);CHECK(data&&edWireDecode(&decoded,data,size)&&edModelsEqual(&decoded,&moved)&&decoded.splines[0].closed);edFree(data);
    CHECK(edWriteGob(&moved,"test-spline-loop.gob","Loop",0,"SECTION: HEADER\n"));file=fopen("test-spline-loop.gob","rb");CHECK(file);fseek(file,0,SEEK_END);bytes=ftell(file);rewind(file);text=edAlloc(bytes+1);CHECK(text&&fread(text,1,bytes,file)==bytes);fclose(file);text[bytes]=0;decoded.splines[0].closed=0;edSplineReadMetadata(&decoded,text+12);CHECK(decoded.splines[0].closed);edFree(text);remove("test-spline-loop.gob");
    EdSpline reflected={};reflected.id=1;reflected.count=2;reflected.sides=4;reflected.segments=3;reflected.width=reflected.height=1;reflected.points[0]={edV(0,4,.5),edV(0,0,0)};reflected.points[1]={edV(3,4,.5),edV(0,0,0)};
    CHECK(edSplineRebuild(&out,&original,&reflected));CHECK(edMirrorEdit(&moved,&original,&out,1));edSplineReconcile(&moved);CHECK(moved.splineCount==1&&moved.splines[0].mirrorAxes==1);
    for(int c=1;c<moved.count;c++)CHECK(moved.c[c].splineId==1);
    reflected=moved.splines[0];reflected.points[1].position.x=4;CHECK(edSplineRebuild(&out,&moved,&reflected));CHECK(out.splineCount==1);int negative=0;for(int c=1;c<out.count;c++)negative|=edContains(&out.c[c],edV(-3.8,4,.5));CHECK(negative);
    EdBuffer<unsigned char> selection;selection[1]=1;EdSelection face={};face.count=1;face.hits[0]={1,0,-1,-1};
    saved=out;CHECK(!edCleaveSectorsPlane(&out,selection,edV(1,0,0),edMapCenter(&out.c[1]))&&!edExtrudeStep(&out,1,0)&&!edMoveSelectionExact(&moved,&out,&face,edV(0,0,.125))&&edModelsEqual(&saved,&out));
    CHECK(!edMapTranslate(&moved,&out,selection,edV(0,1,0),0,selection));
    data=edWireEncode(&out,&size);CHECK(data&&edWireDecode(&decoded,data,size)&&edModelsEqual(&decoded,&out)&&decoded.splines[0].mirrorAxes==1);edFree(data);
    CHECK(edWriteGob(&out,"test-spline-mirror.gob","Mirrored spline",0,"SECTION: HEADER\n"));file=fopen("test-spline-mirror.gob","rb");CHECK(file);fseek(file,0,SEEK_END);bytes=ftell(file);rewind(file);text=edAlloc(bytes+1);CHECK(text&&fread(text,1,bytes,file)==bytes);fclose(file);text[bytes]=0;
    decoded=out;decoded.splines[0].mirrorAxes=0;edSplineReadMetadata(&decoded,text+12);CHECK(decoded.splines[0].mirrorAxes==1&&edModelsEqual(&decoded,&out));edFree(text);remove("test-spline-mirror.gob");
    CHECK(edSplineConvert(&out,1)&&!out.splineCount);for(int c=0;c<out.count;c++)CHECK(!out.c[c].splineId);CHECK(edCleaveSectorsPlane(&out,selection,edV(1,0,0),edMapCenter(&out.c[1])));
    // A spline fully hidden inside an existing sector still has usable controls.
    reflected.mirrorAxes=0;reflected.points[0].position=edV(-.5,0,.5);reflected.points[1].position=edV(.5,0,.5);reflected.width=reflected.height=.25;
    CHECK(edSplineRebuild(&out,&original,&reflected)&&out.splineCount==1);reflected.points[0].position.y=reflected.points[1].position.y=4;CHECK(edSplineRebuild(&moved,&out,&reflected)&&moved.splineCount==1&&moved.count>original.count);
    // Monolith: a narrow 12-sided spline turning from vertical to horizontal.
    // Near-coplanar hull planes previously left holes in four of its sections.
    EdSpline monolith={};monolith.id=91;monolith.count=4;monolith.sides=12;monolith.segments=32;monolith.width=monolith.height=.25;
    monolith.points[0]={edV(6.375,-.875,2.625),edV(-9.7986284348650088e-6,0,-.12636165063206306)};
    monolith.points[1]={edV(5.75,-.875,-.5),edV(-.77401585539385032,0,-.6672152185094129)};
    monolith.points[2]={edV(3.25,-.875,-1.125),edV(-.64390861331719629,0,-.0012763111440730057)};
    monolith.points[3]={edV(1.25,-.875,-1.125),edV(0,0,0)};
    EdModel repaired;CHECK(edSplineUpdate(&repaired,&original,&monolith)&&!repaired.splines[0].invalid&&edValidate(&repaired));
    auto endRoom=std::make_unique<EdModel>(original);for(int f=0;f<endRoom->c[0].count;f++)for(int v=0;v<endRoom->c[0].f[f].count;v++)endRoom->c[0].f[f].v[v].p=edAdd(endRoom->c[0].f[f].v[v].p,edV(.25,-.875,-1.625));
    CHECK(edSplineUpdate(&repaired,endRoom.get(),&monolith)&&!repaired.splines[0].invalid&&edValidate(&repaired));

    // A temporary intersection must not leave its portal subdivisions behind.
    EdSpline touching={};touching.id=1;touching.count=2;touching.sides=4;touching.segments=4;touching.width=touching.height=.5;
    touching.points[0]={edV(0,0,.5),edV(1,0,0)};touching.points[1]={edV(5,0,.5),edV(1,0,0)};
    EdModel joined,away,invalid;CHECK(edSplineUpdate(&joined,&original,&touching));CHECK(!joined.splines[0].invalid&&joined.c[0].count>original.c[0].count);
    EdSpline shifted=joined.splines[0];for(int p=0;p<shifted.count;p++)shifted.points[p].position.y+=8;
    CHECK(edSplineUpdate(&away,&joined,&shifted));CHECK(away.c[0].count==original.c[0].count);
    for(int f=0;f<away.c[0].count;f++)CHECK(away.c[0].f[f].count==4);
    EdSpline second=touching;second.id=2;for(int p=0;p<second.count;p++)second.points[p].position.y+=.125;
    EdModel both,firstAway,allAway;CHECK(edSplineUpdate(&both,&joined,&second));CHECK(edSplineUpdate(&firstAway,&both,&shifted));for(int p=0;p<second.count;p++)second.points[p].position.y+=8;CHECK(edSplineUpdate(&allAway,&firstAway,&second));CHECK(allAway.c[0].count==original.c[0].count);for(int f=0;f<allAway.c[0].count;f++)CHECK(allAway.c[0].f[f].count==4);
    // Collapsed/invalid controls remain editable, with no live spline cells.
    EdSpline bad=touching;bad.points[1]=bad.points[0];bad.points[0].handle=bad.points[1].handle=edV(0,0,0);
    CHECK(edSplineUpdate(&invalid,&joined,&bad));CHECK(invalid.splines[0].invalid&&invalid.count==original.count&&edValidate(&invalid));
    data=edWireEncode(&invalid,&size);CHECK(data&&edWireDecode(&decoded,data,size)&&edModelsEqual(&decoded,&invalid));edFree(data);
    CHECK(edWriteGob(&invalid,"test-spline-invalid.gob","Draft",0,"SECTION: HEADER\n"));file=fopen("test-spline-invalid.gob","rb");CHECK(file);fseek(file,0,SEEK_END);bytes=ftell(file);rewind(file);text=edAlloc(bytes+1);CHECK(text&&fread(text,1,bytes,file)==bytes);fclose(file);text[bytes]=0;
    decoded=invalid;decoded.splines.clear();edSplineReadMetadata(&decoded,text+12);edFree(text);remove("test-spline-invalid.gob");CHECK(edModelsEqual(&decoded,&invalid));
    CHECK(edSplineUpdate(&away,&invalid,&shifted)&&!away.splines[0].invalid&&away.count>original.count);
    // Saved cut provenance survives a reload and still restores the wall.
    CHECK(edWriteGob(&joined,"test-spline-cuts.gob","Cuts",0,"SECTION: HEADER\n"));file=fopen("test-spline-cuts.gob","rb");CHECK(file);fseek(file,0,SEEK_END);bytes=ftell(file);rewind(file);text=edAlloc(bytes+1);CHECK(text&&fread(text,1,bytes,file)==bytes);fclose(file);text[bytes]=0;
    decoded=joined;decoded.splines.clear();edSplineReadMetadata(&decoded,text+12);edFree(text);remove("test-spline-cuts.gob");CHECK(edModelsEqual(&decoded,&joined));CHECK(edSplineUpdate(&away,&decoded,&shifted)&&away.c[0].count==original.c[0].count);
    puts("PASS: spline corridors, connected convex sectors, elevation, shape regeneration, wire data and explicit conversion");
}
static void arrayTests(void){
    // A straight spline must retain coplanar face partitions, not replace
    // their combined outline with a hull face belonging to no source patch.
    {
        auto sourceStorage=std::make_unique<EdModel>(),resultStorage=std::make_unique<EdModel>();EdModel &source=*sourceStorage,&result=*resultStorage;edDefault(&source);CHECK(edCleave(&source,0,4,edV(0,1,0),edV(0,1,1)));
        source.c[0].f[4].material=7;source.c[0].f[6].material=8;EdBuffer<unsigned char> selected;selected[0]=1;CHECK(edGroupCapture(&source,selected,"Split wall"));
        EdSpline s={};s.id=1;s.count=2;s.sides=4;s.segments=4;s.groupId=1;s.width=2;s.height=1;s.points[0]={edV(30,0,0),edV(0,2,0)};s.points[1]={edV(30,8,0),edV(0,2,0)};
        CHECK(edSplineRebuild(&result,&source,&s)&&edValidate(&result));int patches=0;
        for(int c=0;c<result.count;c++)if(result.c[c].splineId)for(int f=0;f<result.c[c].count;f++)if(result.c[c].f[f].material==7||result.c[c].f[f].material==8)patches++;
        CHECK(patches==8);
    }
    auto sourceStorage=std::make_unique<EdModel>(),resultStorage=std::make_unique<EdModel>();EdModel &source=*sourceStorage,&result=*resultStorage;edDefault(&source);for(int f=0;f<6;f++)source.c[0].f[f].material=f+1;EdBuffer<unsigned char> selected;selected[0]=1;
    CHECK(edGroupCapture(&source,selected,"Hall section")&&source.groupCount==1&&edValidate(&source));
    for(int direction=0;direction<6;direction++){
        CHECK(edGroupArray(&result,&source,1,direction,3)&&edValidate(&result)&&result.count==4);int adjoins=0;for(int c=0;c<result.count;c++)for(int f=0;f<result.c[c].count;f++)adjoins+=result.c[c].f[f].neighbor>=0;CHECK(adjoins==6);
        EdModel changed;CHECK(edGroupArray(&changed,&result,1,direction,1)&&changed.count==2);CHECK(edGroupArray(&result,&changed,1,direction,0)&&result.count==1);
    }
    CHECK(edExtrudeStep(&source,0,4));selected[1]=1;CHECK(edGroupCapture(&source,selected,"Detailed hallway"));CHECK(edGroupArray(&result,&source,2,2,2)&&edValidate(&result));
    EdSpline spline={};spline.id=1;spline.count=3;spline.sides=4;spline.segments=4;spline.width=2;spline.height=1;spline.groupId=2;spline.material=99;
    spline.points[0]={edV(8,0,.5),edV(0,2,0)};spline.points[1]={edV(8,8,.5),edV(1,2,0)};spline.points[2]={edV(12,14,1.5),edV(2,1,0)};
    CHECK(edSplineRebuild(&result,&source,&spline)&&edValidate(&result));int generated=0,portals=0;
    for(int c=0;c<result.count;c++)if(result.c[c].splineId){generated++;for(int f=0;f<result.c[c].count;f++){CHECK(result.c[c].f[f].material!=99&&result.c[c].f[f].customUV);portals+=result.c[c].f[f].neighbor>=0;}}CHECK(generated>=8&&portals>=14);
    unsigned n;unsigned char *wire=edWireEncode(&result,&n);EdModel decoded;CHECK(wire&&edWireDecode(&decoded,wire,n)&&edModelsEqual(&decoded,&result));edFree(wire);
    EdModel before=result;spline.points[1].position.x+=.25;CHECK(edSplineRebuild(&result,&before,&spline)&&edValidate(&result));
    {EdModel removed=result;CHECK(edGroupDelete(&removed,1)&&edValidate(&removed));CHECK(removed.groupCount==1&&removed.splines[0].groupId==1&&removed.count==result.count);
        for(int c=0;c<removed.count;c++)CHECK(removed.c[c].arrayId<=1);
        CHECK(edGroupDelete(&removed,1)&&edValidate(&removed));CHECK(removed.groupCount==0&&removed.splineCount==0&&removed.count==result.count);
        for(int c=0;c<removed.count;c++)CHECK(!removed.c[c].arrayId&&!removed.c[c].splineId);
        CHECK(!edGroupDelete(&removed,1));unsigned length;unsigned char *data=edWireEncode(&removed,&length);EdModel restored;CHECK(data&&edWireDecode(&restored,data,length)&&edModelsEqual(&removed,&restored));edFree(data);}
    const char *resources="SECTION: MATERIALS\nWorld materials 7\n0: dflt.mat 1 1\n1: dflt.mat 1 1\n2: dflt.mat 1 1\n3: dflt.mat 1 1\n4: dflt.mat 1 1\n5: dflt.mat 1 1\n6: dflt.mat 1 1\nend\n";
    CHECK(edWriteGob(&result,"test-arrays.gob","Arrays",0,resources));FILE *file=fopen("test-arrays.gob","rb");CHECK(file);fseek(file,0,SEEK_END);long length=ftell(file);rewind(file);std::vector<unsigned char> bytes(length);CHECK(fread(bytes.data(),1,length,file)==length);fclose(file);bool found=false;unsigned dir=edNetLoad32(bytes.data()+8),entries=edNetLoad32(bytes.data()+dir);
    decoded=result;decoded.groupCount=0;decoded.groups.clear();for(unsigned i=0;i<entries;i++){const unsigned char *entry=bytes.data()+dir+4+i*136;if(!strcmp((const char*)entry+8,"misc\\geosith_groups.bin")){CHECK(edReadGroupData(&decoded,bytes.data()+edNetLoad32(entry),edNetLoad32(entry+4)));found=true;}}CHECK(found&&edModelsEqual(&decoded,&result));
    decoded.splineCount=0;edSplineReadMetadata(&decoded,(const char*)bytes.data()+12);CHECK(decoded.splineCount==1&&decoded.splines[0].groupId==2);remove("test-arrays.gob");
    puts("PASS: six array directions, multi-sector groups, regeneration, bent textured spline groups, wire and saved metadata");
}
int main(int argc,char **argv) {
    if(argc==3&&!strcmp(argv[1],"--spline-world")){
        FILE *f=fopen(argv[2],"rb");CHECK(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);std::vector<unsigned char> bytes(n);CHECK(fread(bytes.data(),1,n,f)==n);fclose(f);auto sourceStorage=std::make_unique<EdModel>(),resultStorage=std::make_unique<EdModel>();EdModel &source=*sourceStorage,&result=*resultStorage;CHECK(edWireDecode(&source,bytes.data(),(uint32_t)n));for(int i=0;i<source.splineCount;i++){CHECK(edSplineUpdate(&result,&source,&source.splines[i])&&!result.splines[i].invalid&&edValidate(&result));}puts("PASS: saved world spline rebuild");return 0;
    }
    if(argc==3&&!strcmp(argv[1],"--spline-fixture")){
        FILE *f=fopen(argv[2],"rb");CHECK(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);std::vector<char> text(n+1);CHECK(fread(text.data(),1,n,f)==n);fclose(f);
        auto sourceStorage=std::make_unique<EdModel>(),resultStorage=std::make_unique<EdModel>();EdModel &source=*sourceStorage,&result=*resultStorage;edDefault(&source);for(int c=0;c<source.count;c++)for(int f=0;f<source.c[c].count;f++)for(int v=0;v<source.c[c].f[f].count;v++)source.c[c].f[f].v[v].p.x-=100;
        edSplineReadMetadata(&source,text.data());CHECK(source.splineCount);for(int i=0;i<source.splineCount;i++){EdSpline s=source.splines[i];s.cutBefore.clear();s.cutAfter.clear();printf("Spline %d sides %d segments %d\n",s.id,s.sides,s.segments);CHECK(edSplineRebuild(&result,&source,&s)&&edValidate(&result));}puts("PASS: saved spline generates valid geometry in isolation");return 0;
    }
    if(argc==3&&!strcmp(argv[1],"--group-fixture")){
        FILE *file=fopen(argv[2],"rb");CHECK(file);fseek(file,0,SEEK_END);long size=ftell(file);rewind(file);std::vector<unsigned char> bytes(size);CHECK(fread(bytes.data(),1,size,file)==size);fclose(file);
        EdModel loaded={},source={},result={};loaded.count=edNetLoad32(bytes.data()+8);CHECK(edReadGroupData(&loaded,bytes.data(),(unsigned)size));edDefault(&source);source.groups=loaded.groups;source.groupCount=loaded.groupCount;
        for(int g=0;g<source.groupCount;g++){
            auto &group=source.groups[g];printf("Group %s: %d cells, direction %d\n",group.name,group.count,group.direction);
            for(int c=0;c<group.count;c++)printf(" cell %d: %d faces\n",c,group.cells[c].count);
            EdVec lo,hi;CHECK(edGroupBounds(&group,&lo,&hi));EdVec size=edSub(hi,lo),forward=edGroupAxis(group.direction),up=group.direction/2==2?edV(0,1,0):edV(0,0,1);
            printf("bounds size %g %g %g\n",size.x,size.y,size.z);EdSpline s={};s.id=1;s.count=2;s.sides=4;s.segments=4;s.groupId=g+1;s.width=fabs(edDot(size,edCross(forward,up)));s.height=fabs(edDot(size,up));s.points[0]={edV(30,0,0),edV(0,2,0)};s.points[1]={edV(30,8,0),edV(0,2,0)};
            CHECK(edSplineRebuild(&result,&source,&s)&&edValidate(&result));
            for(double bend : {.001,.1,1.0,4.0,-1.0})for(int segments : {4,8}){
                s.count=3;s.segments=segments;s.points[1]={edV(30,8,0),edV(bend,2,0)};s.points[2]={edV(30+4*bend,16,.5),edV(bend,2,0)};
                CHECK(edSplineRebuild(&result,&source,&s)&&edValidate(&result));
                int portals=0;for(int c=0;c<result.count;c++)for(int f=0;f<result.c[c].count;f++){
                    const EdFace &face=result.c[c].f[f];if(face.neighbor<0)continue;portals++;CHECK(face.mirror>=0&&face.neighbor<result.count);
                    const EdFace &other=result.c[face.neighbor].f[face.mirror];CHECK(other.neighbor==c&&other.mirror==f);
                }CHECK(portals>=2*(segments-1));
                unsigned length=0;unsigned char *wire=edWireEncode(&result,&length);EdModel decoded;CHECK(wire&&edWireDecode(&decoded,wire,length)&&edModelsEqual(&result,&decoded));edFree(wire);
            }
            puts("PASS: saved group on straight and curved splines");
        }return 0;
    }
    scaleTests();
    mirrorTests();
    textureStitchTests();
    terrainGrowthTests();
    terrainTests();
    arrayTests();
    surfaceSessionTests();
    decalTests();
    surfacePaintTests();
    terrainPaintTests();
    splineTests();
    setvbuf(stdout,NULL,_IONBF,0);
    EdDoubleTap tap;edResetDoubleTap(&tap,0);
    CHECK(!edDoubleTap(&tap,1,100,350));CHECK(!edDoubleTap(&tap,1,110,350));
    CHECK(!edDoubleTap(&tap,0,130,350));CHECK(edDoubleTap(&tap,1,200,350));
    CHECK(!edDoubleTap(&tap,1,210,350));CHECK(!edDoubleTap(&tap,0,220,350));
    CHECK(!edDoubleTap(&tap,1,300,350));CHECK(!edDoubleTap(&tap,0,320,350));CHECK(edDoubleTap(&tap,1,400,350));
    edResetDoubleTap(&tap,0);CHECK(!edDoubleTap(&tap,1,500,350));CHECK(!edDoubleTap(&tap,0,510,350));CHECK(!edDoubleTap(&tap,1,900,350));
    edResetDoubleTap(&tap,1);CHECK(!edDoubleTap(&tap,1,1000,350));CHECK(!edDoubleTap(&tap,0,1010,350));CHECK(!edDoubleTap(&tap,1,1020,350));
    edResetDoubleTap(&tap,0);CHECK(!edDoubleTap(&tap,1,UINT32_MAX-100,350));CHECK(!edDoubleTap(&tap,0,UINT32_MAX-50,350));CHECK(edDoubleTap(&tap,1,50,350));
    m=(EdModel*)edAlloc(sizeof(EdModel));backup=(EdModel*)edAlloc(sizeof(EdModel));CHECK(m&&backup);
    if(argc>1&&!strcmp(argv[1],"--storage-only")){dynamicStorageTests();edFree(m);edFree(backup);return 0;}
    edDefault(m);if(!(argc>3&&(!strcmp(argv[3],"--fixture-only")||!strcmp(argv[3],"--large-fixture")))){
    stockMetadataTests();
    savedCleanupTests();
    savedPlacementTests();
    savedMapVertexTests();
    mergeSectorTests();
    meshTests();
    wireTests();walkingDragTests();placedFixtureTests();placedSectorEditTests();weldCoverageTests();singleDoorCutTests();ceilingMoveTests();surfaceAttributeTests();stairsTests();horizontalExtrusionTests();lightTests();episodeTests();cogTests();cleanupTests();deleteHandleTests();mapCleaveDeleteTests();
    /* Top-down hit testing includes overlapping floors; selection rectangles
       intersect actual projected polygons rather than sector bounding boxes. */
    edDefault(m);CHECK(edMapSectorPoint(&m->c[0],0,0));CHECK(!edMapSectorPoint(&m->c[0],2,0));
    CHECK(edMapSectorBox(&m->c[0],-.1,-.1,.1,.1));CHECK(edMapSectorBox(&m->c[0],-2,-.1,2,.1));CHECK(!edMapSectorBox(&m->c[0],2,2,3,3));
    *backup=*m;for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++){EdVec p=m->c[0].f[f].v[v].p;m->c[0].f[f].v[v].p=edV((p.x-p.y)*sqrt(.5),(p.x+p.y)*sqrt(.5),p.z);}
    CHECK(!edMapSectorBox(&m->c[0],1.2,1.2,1.3,1.3));CHECK(edMapSectorPoint(&m->c[0],1.3,0));*m=*backup;
    EdBuffer<unsigned char> sectors={1},mapped;
    *backup=*m;CHECK(edMapTranslate(m,backup,sectors,edV(3,0,0),1,mapped));CHECK(m->count==2&&mapped[1]&&!mapped[0]&&edValidate(m));CHECK(fabs(totalVolume(m)-8)<1e-6);
    *backup=*m;edZero(sectors,0,sizeof(sectors));sectors[1]=1;CHECK(edMapTranslate(m,backup,sectors,edV(-1,0,0),0,mapped));CHECK(edValidate(m)&&portals()==2);CHECK(mapped[1]&&!mapped[0]);
    /* Moving connected sectors carries their contents and reconstructs portals. */
    sectors[0]=sectors[1]=1;*backup=*m;CHECK(edMapTranslate(m,backup,sectors,edV(0,2,0),0,mapped));CHECK(edValidate(m)&&portals()==2);CHECK(fabs(m->spawn.y-backup->spawn.y-2)<1e-6);
    for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++)if(m->c[s].f[f].neighbor>=0){EdFace *face=&m->c[s].f[f];face->attributes=edSurfaceAttributes(face);face->attributes.geometryMode=4;face->attributes.surfaceFlags|=5;face->hasAttributes=1;}
    *backup=*m;CHECK(edMapPaste(m,backup,backup,edV(0,3,0),mapped));CHECK(m->count==4&&portals()==4&&mapped[2]&&mapped[3]&&!mapped[0]);
    for(int s=2;s<m->count;s++)for(int f=0;f<m->c[s].count;f++)if(m->c[s].f[f].neighbor>=0){EdSurfaceAttributes a=edSurfaceAttributes(&m->c[s].f[f]);CHECK(a.geometryMode==4&&(a.surfaceFlags&5)==5);}
    CHECK(!edMapPaste(m,backup,backup,edV(0,0,0),mapped));
    edDefault(m);*backup=*m;CHECK(edMapTranslate(m,backup,sectors,edV(0,0,0),0,mapped));
    for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++)m->c[0].f[f].v[v].p.z+=2;
    CHECK(edMapSectorPoint(&m->c[0],0,0));CHECK(edMapSectorPoint(&backup->c[0],0,0));
    {
        EdModel *outline=edAlloc(sizeof(*outline));CHECK(outline);
        EdVec square[4]={{0,0,0},{2,0,0},{2,2,0},{0,2,0}},clockwise[4]={{0,2,0},{2,2,0},{2,0,0},{0,0,0}};
        CHECK(edMapPolygon(outline,square,4,0,1,0)&&outline->count==1&&outline->c[0].count==6&&edValidate(outline));
        CHECK(edMapPolygon(outline,clockwise,4,0,1,0)&&edValidate(outline));
        EdVec concave[6]={{0,0,0},{2,0,0},{2,1,0},{1,1,0},{1,2,0},{0,2,0}};
        CHECK(!edMapPolygon(outline,concave,6,0,1,0));CHECK(outline->count==1&&fabs(totalVolume(outline)-4)<1e-6);
        CHECK(edMapDrawValid(square,3,edV(0,2,0),0));CHECK(edMapDrawValid(clockwise,3,edV(0,0,0),0));
        CHECK(!edMapDrawValid(square,3,edV(1,.5,0),0));CHECK(!edMapDrawValid(square,3,edV(1,0,0),0));CHECK(!edMapDrawValid(square,3,square[2],0));
        CHECK(edMapDrawValid(square,3,square[0],1));CHECK(!edMapDrawValid(square,2,square[0],1));
        EdVec straight[3]={{0,0,0},{1,0,0},{2,0,0}};CHECK(edMapDrawValid(straight,2,edV(2,0,0),0));CHECK(!edMapDrawValid(straight,2,edV(.5,0,0),0));CHECK(!edMapDrawValid(straight,3,straight[0],1));
        edDefault(backup);CHECK(edMapPaste(m,backup,outline,edV(3,0,0),mapped));CHECK(edValidate(m)&&fabs(totalVolume(m)-8)<1e-6&&m->count==2);
        EdVec triangle[3]={{0,0,0},{1,0,0},{0,1,0}},crossed[4]={{0,0,0},{2,2,0},{0,2,0},{2,0,0}},line[3]={{0,0,0},{1,0,0},{2,0,0}};
        CHECK(edMapPolygon(outline,triangle,3,0,1,0)&&outline->c[0].count==5);
        CHECK(!edMapPolygon(outline,crossed,4,0,1,0));CHECK(!edMapPolygon(outline,line,3,0,1,0));CHECK(!edMapPolygon(outline,triangle,2,0,1,0));
        edFree(outline);
    }
    edDefault(m);EdHit corner={0,4,0,-1};
    EdVec constrainedCorner=edVertexEdgeDelta(m,corner,edV(-.7,-.2,.1));CHECK(fabs(constrainedCorner.y)<ED_EPS&&fabs(constrainedCorner.z)<ED_EPS&&constrainedCorner.x<-.6);
    constrainedCorner=edVertexEdgeDelta(m,corner,edV(0,0,4));CHECK(fabs(constrainedCorner.z-1)<ED_EPS);
    for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++)m->c[0].f[f].v[v].p.z*=.08;
    constrainedCorner=edVertexEdgeDelta(m,corner,edV(0,0,4));CHECK(fabs(constrainedCorner.z-.08)<ED_EPS);
    EdSelection edgeSnap={0};edgeSnap.count=1;edgeSnap.hits[0]=corner;CHECK(fabs(edSelectionDelta(m,&edgeSnap,constrainedCorner).z-.08)<ED_EPS);
    edDefault(m);CHECK(ED_UV_SCALE==256.0);
    /* Door-frame cuts leave pickable ceiling-edge corners without rebuilding
       the entire sector or losing the wall's face partitions. */
    EdFace oldCeiling=m->c[0].f[1];EdFace *ceilingWithCuts=&m->c[0].f[1];ceilingWithCuts->count=6;
    ceilingWithCuts->v[0]=oldCeiling.v[0];ceilingWithCuts->v[1].p=edV(-.5,1,1);ceilingWithCuts->v[2].p=edV(.5,1,1);
    ceilingWithCuts->v[3]=oldCeiling.v[1];ceilingWithCuts->v[4]=oldCeiling.v[2];ceilingWithCuts->v[5]=oldCeiling.v[3];edMapFace(ceilingWithCuts);
    CHECK(edCleave(m,0,4,edV(-.5,1,0),edV(-.5,1,1)));
    EdHit frameHit;CHECK(edRay(m,edV(0,0,.5),edV(.5,1,0),&frameHit));CHECK(edCleave(m,0,frameHit.face,edV(.5,1,0),edV(.5,1,1)));
    int beforeHorizontal=m->c[0].count;
    for(int f=0;f<beforeHorizontal;f++)if(edNormal(&m->c[0].f[f]).y<-.99)CHECK(edCleave(m,0,f,edV(-1,1,.75),edV(1,1,.75)));
    int cleanVertices=uniqueVertices(m),cleanFaces=m->c[0].count;
    CHECK(edRay(m,edV(0,0,.5),edV(-.5,1,.5),&frameHit));CHECK(frameHit.vertex>=0);EdVec slideAxis;CHECK(edVertexSlideAxis(m,frameHit,&slideAxis));CHECK(fabs(slideAxis.x)>.99);
    *backup=*m;CHECK(edMove(m,backup,frameHit,edVertexEdgeDelta(backup,frameHit,edV(-.5,.2,-.3))));CHECK(edValidate(m));CHECK(m->c[0].count>6);
    checkRoomCoverage(m);CHECK(uniqueVertices(m)==cleanVertices-1&&m->c[0].count<=cleanFaces);
    CHECK(edRay(m,edV(0,0,.5),edV(.5,1,.5),&frameHit));CHECK(frameHit.vertex>=0);*backup=*m;CHECK(edMove(m,backup,frameHit,edVertexEdgeDelta(backup,frameHit,edV(.5,.2,-.3))));
    checkRoomCoverage(m);
    CHECK(uniqueVertices(m)==cleanVertices-2&&m->c[0].count<=cleanFaces);
    /* Constrained door-frame cleanup must not drift into the wall or across
     * the next cleave vertex. Both sides keep the same face/vertex counts. */
    EdVec constrained=edVertexEdgeDelta(backup,frameHit,edV(.5,.3,-.4));CHECK(fabs(constrained.y)<ED_EPS&&fabs(constrained.z)<ED_EPS);
    int ceiling=-1;for(int f=0;f<m->c[0].count;f++)if(edNormal(&m->c[0].f[f]).z<-.99){ceiling=f;CHECK(m->c[0].f[f].count==4);}
    CHECK(ceiling>=0);CHECK(edExtrudeStep(m,0,ceiling));CHECK(m->c[1].count==6);CHECK(portals()==2);
    edDefault(m);
    {EdHit edgeHit;CHECK(edRayMaterial(m,edV(.99,0,.5),edV(0,1,0),&edgeHit));CHECK(edgeHit.face==4&&edgeHit.edge>=0&&edgeHit.vertex<0);
        EdFace wall=m->c[0].f[edgeHit.face];edRotateTextureDrag(&wall,edgeHit.point,edV(0,0,0),edgeHit.edge);CHECK(fabs(wall.v[edgeHit.edge].v-wall.v[(edgeHit.edge+1)%wall.count].v)<1e-7);}
    /* Rotate about a fixed texel, preserve density, align the U axis to an
       angled edge, and retain the mapping through remaps, save and wire data. */
    {EdFace *face=&m->c[0].f[0];EdVec pivot=edV(0,0,face->v[0].p.z);double beforeU=0,beforeV=0;
        for(int i=0;i<face->count;i++){beforeU+=face->v[i].u;beforeV+=face->v[i].v;}
        edRotateFaceTexture(face,37,pivot);CHECK(fabs(face->uvRotation-37)<1e-9);double afterU=0,afterV=0;
        for(int i=0;i<face->count;i++){afterU+=face->v[i].u;afterV+=face->v[i].v;}CHECK(fabs(beforeU-afterU)<1e-7&&fabs(beforeV-afterV)<1e-7);
        EdFace saved=*face;edMapFace(face);for(int i=0;i<face->count;i++)CHECK(fabs(face->v[i].u-saved.v[i].u)<1e-9&&fabs(face->v[i].v-saved.v[i].v)<1e-9);
        unsigned int wireSize=0;unsigned char *wire=edWireEncode(m,&wireSize);CHECK(wire&&edWireDecode(backup,wire,wireSize));CHECK(backup->c[0].f[0].uvRotation==37);edFree(wire);
        CHECK(edWriteGob(m,"test-rotation.gob","Rotated",0,"SECTION: HEADER\n"));FILE *rf=fopen("test-rotation.gob","rb");CHECK(rf);fseek(rf,0,SEEK_END);long size=ftell(rf);rewind(rf);char *text=edAlloc(size+1);CHECK(text&&fread(text,1,size,rf)==size);fclose(rf);text[size]=0;
        edDefault(backup);CHECK(edReadTextureRotations(backup,text+12)&&backup->c[0].f[0].uvRotation==37);for(int i=0;i<face->count;i++)CHECK(fabs(backup->c[0].f[0].v[i].u-face->v[i].u)<1e-7);edFree(text);remove("test-rotation.gob");
        CHECK(!edReadTextureRotations(backup,"# GeoSith texture rotation 0 0 nan 0 0 1 1\n"));
        EdVec panRotate=edV(ED_GRID,ED_GRID,0);edPanFaceTexture(face,panRotate);for(int i=0;i<face->count;i++)face->v[i].p=edAdd(face->v[i].p,panRotate);edMapFace(face);
        for(int i=0;i<face->count;i++)CHECK(fabs(face->v[i].u-saved.v[i].u)<1e-7&&fabs(face->v[i].v-saved.v[i].v)<1e-7);
        *face=saved;for(int i=0;i<face->count;i++){EdVec p=face->v[i].p;face->v[i].p=edV((p.x-p.y)*sqrt(.5),(p.x+p.y)*sqrt(.5),p.z);}edMapFace(face);
        edRotateTextureDrag(face,pivot,edV(0,0,0),0);CHECK(fabs(face->v[0].v-face->v[1].v)<1e-7);
    }
    edDefault(m);
    EdFace beforePan=m->c[0].f[4],afterPan=beforePan;EdVec pan=edV(ED_GRID,0,ED_GRID);edPanFaceTexture(&afterPan,pan);
    for(int v=0;v<afterPan.count;v++)afterPan.v[v].p=edAdd(afterPan.v[v].p,pan);edMapFace(&afterPan);
    for(int v=0;v<afterPan.count;v++){CHECK(fabs(afterPan.v[v].u-beforePan.v[v].u)<1e-6);CHECK(fabs(afterPan.v[v].v-beforePan.v[v].v)<1e-6);}
    *backup=*m;EdHit weldVertex={0,4,0,-1};CHECK(edMove(m,backup,weldVertex,edV(0,0,1)));CHECK(uniqueVertices(m)==7);CHECK(edValidate(m));
    *m=*backup;EdHit weldEdge={0,4,-1,3};CHECK(edMove(m,backup,weldEdge,edV(0,0,1)));CHECK(uniqueVertices(m)==6);CHECK(edValidate(m));
    edDefault(m);CHECK(edCleaveSector(m,0,4,edV(0,1,0),edV(0,1,1)));CHECK(m->count==2);CHECK(portals()==2);CHECK(fabs(totalVolume(m)-4)<1e-6);
    edDefault(m);CHECK(edExtrude(m,0,4,edV(0,1,0)));CHECK(edCleaveSector(m,0,0,edV(0,-1,0),edV(0,1,0)));CHECK(m->count==3);CHECK(portals()==6);CHECK(fabs(totalVolume(m)-6)<1e-6);
    edDefault(m);
    CHECK(fabs(fabs(m->c[0].f[4].v[2].u-m->c[0].f[4].v[1].u)-512)<1e-6);
    CHECK(!edDeleteSector(m,0));CHECK(m->count==1);
    CHECK(edExtrude(m,0,4,edV(0,1,0)));CHECK(edExtrude(m,0,3,edV(1,0,0)));CHECK(m->count==3);CHECK(portals()==4);
    m->spawnCount=2;m->spawns[1].position=edV(0,1.5,.5);m->spawns[1].rotation=edV(0,90,0);*backup=*m;
    CHECK(!edDeleteSector(m,-1));CHECK(!edDeleteSector(m,3));
    for(int deleted=0;deleted<3;deleted++){
        *m=*backup;CHECK(edDeleteSector(m,deleted));CHECK(m->count==2);CHECK(edValidate(m));CHECK(portals()==(deleted==0?0:2));
        for(int i=0;i<m->spawnCount;i++){int inside=0;for(int s=0;s<m->count;s++)inside|=edContains(&m->c[s],m->spawns[i].position);CHECK(inside);}
        CHECK(m->spawns[1].rotation.y==90);CHECK(!strcmp(m->header.title,backup->header.title));
        CHECK(edDeleteSector(m,0));CHECK(m->count==1);CHECK(portals()==0);CHECK(!edDeleteSector(m,0));
    }
    edDefault(m);CHECK(edCleave(m,0,4,edV(0,1,0),edV(0,1,1)));*backup=*m;
    EdSelection group={0},remapped={0};EdHit halfA={0,4,-1,-1},halfB={0,6,-1,-1};
    CHECK(edSelectionToggle(m,&group,halfA));CHECK(edSelectionToggle(m,&group,halfB));CHECK(group.count==2);
    CHECK(edMoveSelection(m,backup,&group,edV(0,2*ED_GRID,0)));CHECK(fabs(totalVolume(m)-(4+4*ED_GRID))<1e-6);
    edSelectionRemap(backup,m,&group,edV(0,2*ED_GRID,0),&remapped);CHECK(remapped.count==2);
    *m=*backup;CHECK(edExtrudeSelection(m,&group));CHECK(edValidate(m));CHECK(fabs(totalVolume(m)-(4+2*ED_GRID))<1e-6);CHECK(portals()>=6);
    edSelectionRemap(backup,m,&group,edV(0,ED_GRID,0),&remapped);CHECK(remapped.count==2);
    *backup=*m;CHECK(edMoveSelection(m,backup,&remapped,edV(0,ED_GRID,0)));CHECK(fabs(totalVolume(m)-(4+4*ED_GRID))<1e-6);CHECK(portals()>=6);
    edDefault(m);*backup=*m;group.count=0;EdHit adjacentA={0,3,-1,-1},adjacentB={0,4,-1,-1};
    CHECK(edSelectionToggle(m,&group,adjacentA));CHECK(edSelectionToggle(m,&group,adjacentB));
    CHECK(edMoveSelection(m,backup,&group,edV(0,ED_GRID,0)));
    double furthest=-1e9;for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++)if(m->c[0].f[f].v[v].p.y>furthest)furthest=m->c[0].f[f].v[v].p.y;
    CHECK(fabs(furthest-1-ED_GRID)<1e-6); /* Shared corners moved once. */
    /* Map vertex columns starting off-grid snap to absolute XY grid lines.
       Height and unselected corners stay unchanged; remapping retains picks. */
    edDefault(m);
    for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++)m->c[0].f[f].v[v].p=edAdd(m->c[0].f[f].v[v].p,edV(ED_GRID*.3,-ED_GRID*.2,.013));
    *backup=*m;EdSelection column={0},columnMapped={0};EdVec columnStart=m->c[0].f[0].v[0].p;
    for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++){
        EdVec p=m->c[0].f[f].v[v].p;if(fabs(p.x-columnStart.x)<ED_EPS&&fabs(p.y-columnStart.y)<ED_EPS)edSelectionToggle(m,&column,EdHit{0,f,v,-1});
    }
    CHECK(column.count==2);
    CHECK(edDot(edSelectionDeltaXY(backup,&column,edV(0,0,0)),edSelectionDeltaXY(backup,&column,edV(0,0,0)))==0);
    for(double zoom=4;zoom<=32768;zoom*=2){double grid=edMapGridSpacing(zoom);
        CHECK(grid>=ED_GRID&&grid*zoom>=12);
        if(grid>ED_GRID)CHECK(grid*zoom<24);
        if(grid>ED_GRID)CHECK(edMapGridSpacing(zoom*2)==grid/2);
        EdVec snappedMap=edMapSnapXY(edV(.37,-.51,.013),grid);CHECK(snappedMap.z==.013);
        CHECK(fabs(snappedMap.x/grid-round(snappedMap.x/grid))<1e-8&&fabs(snappedMap.y/grid-round(snappedMap.y/grid))<1e-8);
    }
    for(double grid=ED_GRID;grid<=ED_GRID*4;grid*=2)for(int direction=-1;direction<=1;direction+=2){
        EdVec motion=edV(direction*grid*1.2,direction*grid*.8,99);
        EdVec deltaXY=edSelectionDeltaXYGrid(backup,&column,motion,grid);
        CHECK(edMoveSelectionXY(m,backup,&column,deltaXY)&&edValidate(m));
        edSelectionRemap(backup,m,&column,deltaXY,&columnMapped);CHECK(columnMapped.count==2);
        for(int i=0;i<columnMapped.count;i++){EdHit h=columnMapped.hits[i];EdVec p=m->c[h.cell].f[h.face].v[h.vertex].p;
            CHECK(fabs(p.x/grid-round(p.x/grid))<1e-8&&fabs(p.y/grid-round(p.y/grid))<1e-8);
            CHECK(fabs(p.z-.013)<ED_EPS||fabs(p.z-1-.013)<ED_EPS);
        }
    }
    EdVec snapped=edSnapPoint(edV(.04,-.04,.01));CHECK(fabs(snapped.x-ED_GRID)<1e-9);CHECK(fabs(snapped.y+ED_GRID)<1e-9);CHECK(snapped.z==0);
    edDefault(m);*backup=*m;EdHit gridFace={0,4,-1,-1};
    CHECK(edMove(m,backup,gridFace,edV(0,.04,0)));CHECK(fabs(totalVolume(m)-(4+2*ED_GRID))<1e-6);
    edDefault(m);*backup=*m;EdHit gridVertex={0,4,0,-1};
    EdVec targetVertex=edSnapPoint(edAdd(backup->c[0].f[4].v[0].p,edV(.04,0,.04)));
    CHECK(edMove(m,backup,gridVertex,edV(.04,0,.04)));
    int foundVertex=0;for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++)if(edDot(edSub(m->c[0].f[f].v[v].p,targetVertex),edSub(m->c[0].f[f].v[v].p,targetVertex))<1e-12)foundVertex=1;
    CHECK(foundVertex);
    /* World UVs add repetitions as a wall expands, retaining texel density. */
    for(int f=0;f<m->c[0].count;f++) {
        EdFace projected=m->c[0].f[f];edMapFace(&projected);
        for(int v=0;v<projected.count;v++){CHECK(fabs(projected.v[v].u-m->c[0].f[f].v[v].u)<1e-6);CHECK(fabs(projected.v[v].v-m->c[0].f[f].v[v].v)<1e-6);}
    }
    edDefault(m);edShiftFaceUV(&m->c[0].f[4],edV(ED_GRID,0,ED_GRID));*backup=*m;
    CHECK(edMove(m,backup,gridFace,edV(0,ED_GRID,0)));
    CHECK(fabs(m->c[0].f[4].uOffset-ED_UV_SCALE*ED_GRID)<1e-6);CHECK(fabs(m->c[0].f[4].vOffset+ED_UV_SCALE*ED_GRID)<1e-6);
    edDefault(m);CHECK(edExtrudeStep(m,0,4));CHECK(m->count==2);CHECK(portals()==2);CHECK(fabs(totalVolume(m)-(4+2*ED_GRID))<1e-6);
    edDefault(m);CHECK(edCleave(m,0,4,edV(-.8,1,.21),edV(.8,1,.21)));
    int cutVertices=0;for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++)if(fabs(m->c[0].f[f].v[v].p.z-3*ED_GRID)<1e-6)cutVertices++;
    CHECK(cutVertices>=4);
    edDefault(m);m->spawnCount=2;m->spawns[1].position=edV(.5,.5,.12);m->spawns[1].rotation=edV(0,90,0);
    CHECK(edExtrudeStep(m,0,4));CHECK(m->spawnCount==2);CHECK(m->spawns[1].rotation.y==90);
    CHECK(edWriteGob(m,"test-spawns.gob","Spawns",0,"SECTION: HEADER\n"));
    FILE *spawnFile=fopen("test-spawns.gob","rb");CHECK(spawnFile);char spawnText[20000];size_t spawnBytes=fread(spawnText,1,sizeof(spawnText)-1,spawnFile);spawnText[spawnBytes]=0;fclose(spawnFile);
    const char *spawnLine="1: walkplayer walkplayer 0.500000000";
    int foundSpawn=0;for(size_t i=0;i+strlen(spawnLine)<spawnBytes;i++)if(!edCompare(spawnText+i,spawnLine,strlen(spawnLine)))foundSpawn=1;
    CHECK(foundSpawn);remove("test-spawns.gob");
    edDefault(m);CHECK(edValidate(m));CHECK(fabs(totalVolume(m)-4)<1e-6);CHECK(edContains(&m->c[0],m->spawn));
    EdHit hit;CHECK(edRay(m,edV(0,0,.5),edV(0,1,0),&hit));CHECK(hit.face==4);CHECK(fabs(hit.distance-1)<1e-6);
    /* Consecutive ray/plane collisions follow cursor travel, including oblique walls. */
    EdVec origin=edV(0,0,.5),look=edV(0,1,0),wallPlane=edV(0,-1,0),point=edV(0,2,.5),first,last;
    CHECK(edTracePlane(origin,look,point,wallPlane,&first));
    CHECK(edTracePlane(origin,edV(.6,.8,0),point,wallPlane,&last));
    CHECK(fabs(last.x-first.x-1.5)<1e-6);CHECK(fabs(last.y-first.y)<1e-6);
    EdVec drag=edDragDelta(origin,look,origin,edV(.6,.8,0),2,NULL);
    CHECK(fabs(drag.x-1.5)<1e-6);CHECK(fabs(drag.y)<1e-6);
    CHECK(edTracePlane(origin,edV(.3,.95,0),point,edV(1,-1,0),&last));CHECK(last.x>0&&last.y>2);
    CHECK(!edTracePlane(origin,edV(1,0,0),point,wallPlane,&last));
    CHECK(!edTracePlane(origin,edV(0,-1,0),point,wallPlane,&last));
    drag=edDragDelta(origin,look,edV(-.5,1,2.5),edV(.6,.8,0),2,&wallPlane);
    CHECK(fabs(drag.x-.25)<1e-6);CHECK(fabs(drag.y-1)<1e-6);CHECK(fabs(drag.z-2)<1e-6);
    drag=edDragDelta(origin,look,origin,edV(.6,.8,0),2,NULL);
    /* Moving a wall must select its tangent axes as well as its depth axis. */
    EdVec wallNormal=edV(0,-1,0);
    EdVec surfaceDrag=edSurfaceDragDelta(drag,wallNormal);
    CHECK(fabs(surfaceDrag.x-1.5)<1e-6);CHECK(fabs(surfaceDrag.y)<1e-6);
    drag=edDragDelta(origin,look,origin,edV(0,.8,.6),2,NULL);
    surfaceDrag=edSurfaceDragDelta(drag,wallNormal);
    CHECK(fabs(surfaceDrag.z-1.5)<1e-6);CHECK(fabs(surfaceDrag.y)<1e-6);
    surfaceDrag=edSurfaceDragDelta(edV(.1,-.5,.1),wallNormal);
    CHECK(fabs(surfaceDrag.y+.5)<1e-6);CHECK(fabs(surfaceDrag.x)<1e-6);CHECK(fabs(surfaceDrag.z)<1e-6);
    surfaceDrag=edSurfaceDragDelta(edV(.1,.2,1),edV(1,1,0));
    CHECK(fabs(surfaceDrag.z-1)<1e-6);CHECK(fabs(surfaceDrag.x)<1e-6);CHECK(fabs(surfaceDrag.y)<1e-6);
    surfaceDrag=edSurfaceDragDelta(edV(1,.1,.2),edV(0,0,1));CHECK(fabs(surfaceDrag.x-1)<1e-6);CHECK(fabs(surfaceDrag.z)<1e-6);
    *backup=*m;EdHit wallHit={0,4,-1,-1};
    CHECK(edMove(m,backup,wallHit,edSurfaceDragDelta(edV(.25,.01,0),wallNormal)));
    CHECK(fabs(totalVolume(m)-4)<1e-6);
    for(int v=0;v<m->c[0].f[4].count;v++){CHECK(fabs(m->c[0].f[4].v[v].p.x-backup->c[0].f[4].v[v].p.x-.25)<1e-6);CHECK(fabs(m->c[0].f[4].v[v].p.y-1)<1e-6);}
    CHECK(edMove(m,backup,wallHit,edSurfaceDragDelta(edV(0,.01,.25),wallNormal)));CHECK(fabs(totalVolume(m)-4)<1e-6);
    edDefault(m);
    CHECK(edRay(m,edV(.99,0,.5),edV(0,1,0),&hit));CHECK(hit.edge>=0);
    CHECK(edRay(m,edV(.999,0,.999),edV(0,1,0),&hit));CHECK(hit.vertex>=0);
    CHECK(edCleave(m,0,4,edV(0,1,.2),edV(0,1,.8)));CHECK(m->c[0].count==7);CHECK(edValidate(m));CHECK(fabs(totalVolume(m)-4)<1e-6);
    edDefault(m);*backup=*m;CHECK(edExtrude(m,0,4,edV(0,1,0)));CHECK(edValidate(m));CHECK(m->count==2);CHECK(portals()==2);CHECK(fabs(totalVolume(m)-6)<1e-6);
    CHECK(edRay(m,edV(0,0,.5),edV(0,1,0),&hit));CHECK(fabs(hit.distance-2)<1e-6);
    /* Extrusion into an existing cell removes the overlap and creates portals. */
    EdCell target=backup->c[0];for(int f=0;f<target.count;f++)for(int v=0;v<target.f[f].count;v++)target.f[f].v[v].p.y+=2.5;
    edDefault(m);m->c[m->count++]=target;CHECK(edExtrude(m,0,4,edV(0,2,0)));CHECK(edValidate(m));CHECK(fabs(totalVolume(m)-9)<1e-5);CHECK(portals()>=4);
    /* Partial intersection only opens the shared rectangle. */
    edDefault(m);target=backup->c[0];for(int f=0;f<target.count;f++)for(int v=0;v<target.f[f].count;v++){target.f[f].v[v].p.y+=2.5;target.f[f].v[v].p.x+=1;}
    m->c[m->count++]=target;CHECK(edExtrude(m,0,4,edV(0,2,0)));CHECK(edValidate(m));CHECK(fabs(totalVolume(m)-10.5)<1e-5);CHECK(portals()>=4);
    for(int repeat=0;repeat<8;repeat++){CHECK(edResolve(m));CHECK(portals()>=4);}
    edDefault(m);*backup=*m;hit.cell=0;hit.face=4;hit.vertex=hit.edge=-1;CHECK(edMove(m,backup,hit,edV(0,.5,0)));CHECK(fabs(totalVolume(m)-5)<1e-5);
    /* Moving a corner rebuilds twisted quads into planar convex hull faces. */
    hit.vertex=0;CHECK(edMove(m,backup,hit,edV(.1,.2,.3)));CHECK(edValidate(m));
    CHECK(!edMove(m,backup,hit,edV(NAN,0,0)));
    hit.vertex=-1;hit.edge=0;CHECK(edMove(m,backup,hit,edV(.1,.2,.1)));CHECK(edValidate(m));
    edDefault(m);CHECK(edExtrude(m,0,4,edV(.2,1,.3)));CHECK(edValidate(m));CHECK(portals()==2);CHECK(fabs(totalVolume(m)-6)<1e-6);
    edDefault(m);CHECK(edExtrude(m,0,4,edV(0,1,0)));int cellCount=m->count,faceCount=m->c[0].count+m->c[1].count;
    for(int repeat=0;repeat<8;repeat++){CHECK(edResolve(m));CHECK(m->count==cellCount);CHECK(m->c[0].count+m->c[1].count==faceCount);CHECK(portals()==2);}
    /* Keep moving/extruding a connected level, including partial overlaps. */
    for(int repeat=0;repeat<24;repeat++) {
        edDefault(m);CHECK(edExtrude(m,0,4,edV(0,1,0)));*backup=*m;
        EdHit moved={1,1,-1,-1};
        CHECK(edMove(m,backup,moved,edV(.05*(repeat%4),0,.02*(repeat%3))));
        CHECK(edValidate(m));CHECK(portals()>=2);
        CHECK(edResolve(m));CHECK(edValidate(m));CHECK(portals()>=2);
    }
    /* Closed polygon cleaves partition one surface, preserve the solid, and
       allow the JK maximum of 24 vertices per surface. */
    for(int sides=3;sides<=ED_SURFACE_VERTS;sides++){
        edDefault(m);EdSurfaceAttributes attr=edSurfaceAttributes(&m->c[0].f[4]);attr.extraLight=.25;m->c[0].f[4].attributes=attr;m->c[0].f[4].hasAttributes=1;m->c[0].f[4].material=7;
        double before=totalVolume(m);CHECK(edCleaveShape(m,0,4,edV(0,1,.5),.25,sides));CHECK(edValidate(m));CHECK(m->count==1&&m->c[0].count==6+sides);CHECK(m->c[0].f[4].count==sides);CHECK(fabs(totalVolume(m)-before)<1e-6);
        for(int face=6;face<m->c[0].count;face++){CHECK(m->c[0].f[face].material==7);CHECK(fabs(edSurfaceAttributes(&m->c[0].f[face]).extraLight-.25)<1e-6);}
        *backup=*m;CHECK(!edCleaveShape(m,0,4,edV(0,1,.5),10,sides));CHECK(!edCompare(m,backup,sizeof(*m)));
    }
    edDefault(m);CHECK(edCleaveShape(m,0,0,edV(0,0,0),.25,24));CHECK(edValidate(m));
    for(int sides=3;sides<=ED_SURFACE_VERTS;sides++){
        edDefault(m);EdVec preview[32],unrotated[32];CHECK(edShapePoints(&m->c[0].f[4],edV(0,1,.5),.25,sides,17,preview));CHECK(edShapePoints(&m->c[0].f[4],edV(0,1,.5),.25,sides,0,unrotated));
        CHECK(edDot(edSub(preview[0],unrotated[0]),edSub(preview[0],unrotated[0]))>.001);
        CHECK(edCleaveShapeRotated(m,0,4,edV(0,1,.5),.25,sides,17));CHECK(edValidate(m));CHECK(m->c[0].f[4].count==sides);
        for(int i=0;i<sides;i++){int found=0;for(int v=0;v<sides;v++){EdVec delta=edSub(preview[i],m->c[0].f[4].v[v].p);if(edDot(delta,delta)<ED_EPS*ED_EPS)found=1;}CHECK(found);}
    }
    edDefault(m);CHECK(edCleaveShape(m,0,4,edV(0,1,.5),.25,24));CHECK(edExtrudeStep(m,0,4));CHECK(edValidate(m));CHECK(m->count==2&&portals()==2);
    for(int sides=25;sides<=32;sides++){edDefault(m);*backup=*m;EdVec preview[32];CHECK(edShapePoints(&m->c[0].f[4],edV(0,1,.5),.25,sides,0,preview));edClearGeometryError();CHECK(!edCleaveShape(m,0,4,edV(0,1,.5),.25,sides));CHECK(edVertexLimitHit());CHECK(!edCompare(m,backup,sizeof(*m)));}
    /* Clipping a circle against the wall's bottom edge leaves a flat base
       with two intersection vertices, and can be extruded into a tunnel. */
    edDefault(m);EdFace arch;edClearGeometryError();CHECK(edClipShape(&m->c[0].f[4],edV(0,1,0),.25,32,0,&arch)==1);CHECK(!edVertexLimitHit());CHECK(arch.count==18);
    int baseVertices=0;double archArea=0;EdVec archNormal=edNormal(&arch);
    for(int i=0;i<arch.count;i++){CHECK(arch.v[i].p.z>=-ED_EPS);if(fabs(arch.v[i].p.z)<ED_EPS)baseVertices++;archArea+=edDot(edCross(arch.v[i].p,arch.v[(i+1)%arch.count].p),archNormal)*.5;}
    CHECK(baseVertices==2);double roomVolume=totalVolume(m);CHECK(edCleaveShape(m,0,4,edV(0,1,0),.25,32));CHECK(m->c[0].f[4].count==arch.count&&edValidate(m));CHECK(fabs(totalVolume(m)-roomVolume)<1e-8);
    for(int i=0;i<arch.count;i++){int found=0;for(int j=0;j<arch.count;j++){EdVec d=edSub(arch.v[i].p,m->c[0].f[4].v[j].p);if(edDot(d,d)<ED_EPS*ED_EPS)found=1;}CHECK(found);}
    CHECK(edExtrudeStep(m,0,4));CHECK(edValidate(m)&&m->count==2&&portals()==2);CHECK(fabs(totalVolume(m)-(roomVolume+archArea*ED_GRID))<1e-7);
    int flatFloor=0;for(int f=0;f<m->c[1].count;f++){EdFace *face=&m->c[1].f[f];int flat=face->count==4;for(int v=0;v<face->count;v++)if(fabs(face->v[v].p.z)>ED_EPS)flat=0;flatFloor+=flat;}CHECK(flatFloor==1);
    for(int sides=3;sides<=32;sides++){edDefault(m);int cut=edCleaveShapeRotated(m,0,4,edV(0,1,0),.25,sides,17);if(!cut)printf("Clipped shape failed: %d sides, vertex limit %d\n",sides,edVertexLimitHit());CHECK(cut);CHECK(edValidate(m));}
    edDefault(m);CHECK(edCleaveShapeRotated(m,0,4,edV(1,1,0),.25,32,13));CHECK(edValidate(m)&&m->c[0].f[4].count<=24);
    edDefault(m);*backup=*m;edClearGeometryError();CHECK(!edCleaveShape(m,0,4,edV(0,1,.1875),.25,32));CHECK(edVertexLimitHit()&&!edCompare(m,backup,sizeof(*m)));
    edDefault(m);*backup=*m;edClearGeometryError();CHECK(!edCleaveShape(m,0,4,edV(0,1,-1),.25,32));CHECK(!edVertexLimitHit()&&!edCompare(m,backup,sizeof(*m)));
    CHECK(!edCleaveShape(m,0,4,edV(0,1,.5),10,32));CHECK(!edVertexLimitHit()&&!edCompare(m,backup,sizeof(*m)));
    /* A line that trims one corner off a 24-gon would leave a 25-gon. */
    edDefault(m);for(int f=0;f<6;f++)for(int v=0;v<4;v++)m->c[0].f[f].v[v].p=edScale(m->c[0].f[f].v[v].p,8);
    CHECK(edCleaveShapeRotated(m,0,4,edV(0,8,4),3,24,7.5));*backup=*m;edClearGeometryError();CHECK(!edCleave(m,0,4,edV(2.9375,8,0),edV(2.9375,8,8)));CHECK(edVertexLimitHit());CHECK(!edCompare(m,backup,sizeof(*m)));
    edDefault(m);m->c[0].f[0].count=25;edClearGeometryError();CHECK(!edValidate(m)&&edVertexLimitHit());CHECK(!edWriteGob(m,"test-invalid-limit.gob","Invalid",0,""));
    edDefault(m);for(int f=0;f<6;f++)for(int v=0;v<4;v++)m->c[0].f[f].v[v].p.y+=.3*m->c[0].f[f].v[v].p.z;
    CHECK(edCleaveShape(m,0,4,edV(0,1.15,.5),.25,8));CHECK(edValidate(m));
    edDefault(m);*backup=*m;CHECK(!edCleaveShape(m,0,4,edV(0,1,.5),.25,2));CHECK(!edCleaveShape(m,0,4,edV(0,1,.5),NAN,4));CHECK(!edCompare(m,backup,sizeof(*m)));
    edDefault(m);CHECK(!edCleave(m,0,4,edV(0,1,.2),edV(0,1,.2)));
    CHECK(!edExtrude(m,0,4,edV(0,-1,0)));
    edDefault(m);CHECK(edWriteGob(m,"test-level.gob","Editor test",0,"SECTION: HEADER\n"));
    FILE *f=fopen("test-level.gob","rb");CHECK(f);char magic[4];CHECK(fread(magic,1,4,f)==4);CHECK(!edCompare(magic,"GOB ",4));fclose(f);remove("test-level.gob");
    strcpy(m->header.title,"Custom header title");strcpy(m->header.colormap,"01narsh.cmp");strcpy(m->header.intro[19],"Last intro line");m->header.gravity=3.5;
    CHECK(edResolve(m));CHECK(!strcmp(m->header.title,"Custom header title"));CHECK(m->header.gravity==3.5);
    CHECK(edWriteGob(m,"test-level.gob","ignored filename",0,"SECTION: HEADER\nWorld Gravity 4\nSECTION: SOUNDS\nWorld sounds 0\nend\n"));
    f=fopen("test-level.gob","rb");CHECK(f);fseek(f,0,SEEK_END);long bytes=ftell(f);rewind(f);char *gob=edAlloc(bytes+1);CHECK(gob&&fread(gob,1,bytes,f)==(size_t)bytes);gob[bytes]=0;fclose(f);
    /* Check the actual text payloads, past the binary GOB header. */
    char *payload=gob+12;CHECK(strstr(payload,"Custom header title"));CHECK(strstr(payload,"World Gravity 3.5\n"));CHECK(strstr(payload,"0: 01narsh.cmp"));CHECK(strstr(payload,"\"SITHED_TEXT_19\" 0 \"Last intro line\""));edFree(gob);remove("test-level.gob");
    strcpy(m->header.intro[0],"Invalid \"quote\"");CHECK(!edWriteGob(m,"test-level.gob","test",0,""));edDefault(m);
}
    if(argc>3&&!strcmp(argv[3],"--large-fixture")){EdCell room=m->c[0];m->count=1536;for(int i=1;i<m->count;i++){m->c[i]=room;for(int f=0;f<6;f++)for(int v=0;v<4;v++)m->c[i].f[f].v[v].p.x+=i*4;}CHECK(edValidate(m));}
    if(argc>2){FILE *r=fopen(argv[1],"rb");CHECK(r);fseek(r,0,SEEK_END);long n=ftell(r);rewind(r);char *resources=edAlloc(n+1);CHECK(fread(resources,1,n,r)==(size_t)n);resources[n]=0;fclose(r);
        if(getenv("GEOSITH_CLEANUP_FIXTURE_GOB")){EdLevelHeader header=m->header;readGeometryFixture("cleanup_fixture.txt");m->header=header;m->spawnCount=1;m->spawn=m->spawns[0].position=edV(-3.4375,4.4,.2);for(int s=0;s<m->count;s++){m->c[s].extraLight=.5;for(int f=0;f<m->c[s].count;f++)m->c[s].f[f].material=0;}edMapWorld(m);}
        CHECK(edWriteGob(m,argv[2],"Editor smoke test",0,resources));edFree(resources);}
    edFree(m);edFree(backup);puts("PASS: selection, cleave, extrusion, overlap CSG, partial portals, dragging, validation and GOB output");return 0;
}
