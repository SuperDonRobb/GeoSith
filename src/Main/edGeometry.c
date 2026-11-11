#include <stddef.h>
#include "edGeometry.h"
#include "edWire.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string>
void edCopyModel(EdModel *out,const EdModel *source){
 if(out==source)return;EdModel copy=*source;
 copy.c.resize(copy.count);for(int s=0;s<copy.count;s++)copy.c[s].f.resize(copy.c[s].count);
 copy.splines.resize(copy.splineCount);for(int i=0;i<copy.splineCount;i++)copy.splines[i].points.resize(copy.splines[i].count);
 copy.objects.resize(copy.objectCount);copy.lights.resize(copy.lightCount);copy.cogs.resize(copy.cogCount);
 copy.assets.resize(copy.assetCount);copy.meshes.resize(copy.meshCount);copy.templates.resize(copy.templateCount);
 for(int i=0;i<copy.meshCount;i++)copy.meshes[i].geometry.f.resize(copy.meshes[i].geometry.count);
 *out=std::move(copy);
}
int edCellsEqual(const EdCell *a,const EdCell *b){
 if(a->arrayId!=b->arrayId||a->splineId!=b->splineId||std::memcmp(a->colormap,b->colormap,sizeof(a->colormap))||std::memcmp(a->layer,b->layer,sizeof(a->layer))||a->count!=b->count||a->ambientLight!=b->ambientLight||a->extraLight!=b->extraLight||a->flags!=b->flags||a->soundVolume!=b->soundVolume||a->tolerance!=b->tolerance||std::memcmp(&a->tint,&b->tint,sizeof(EdVec))||std::memcmp(&a->thrust,&b->thrust,sizeof(EdVec))||std::memcmp(a->sound,b->sound,32))return 0;
 for(int f=0;f<a->count;f++){const EdFace& x=a->f[f];const EdFace& y=b->f[f];
 if(x.decalId!=y.decalId||x.customUV!=y.customUV||x.count!=y.count||x.material!=y.material||x.neighbor!=y.neighbor||x.mirror!=y.mirror||x.uOffset!=y.uOffset||x.vOffset!=y.vOffset||x.uvRotation!=y.uvRotation||x.hasAttributes!=y.hasAttributes)return 0;
 if(x.attributes.surfaceFlags!=y.attributes.surfaceFlags||x.attributes.faceFlags!=y.attributes.faceFlags||x.attributes.adjoinFlags!=y.attributes.adjoinFlags||x.attributes.geometryMode!=y.attributes.geometryMode||x.attributes.textureMode!=y.attributes.textureMode||x.attributes.lightingMode!=y.attributes.lightingMode||x.attributes.portal!=y.attributes.portal||x.attributes.uScale!=y.attributes.uScale||x.attributes.vScale!=y.attributes.vScale||x.attributes.extraLight!=y.attributes.extraLight||x.attributes.adjoinDistance!=y.attributes.adjoinDistance)return 0;
 for(int v=0;v<x.count;v++)if(std::memcmp(&x.v[v],&y.v[v],sizeof(EdVertex)))return 0;
 }return 1;
}
int edModelsEqual(const EdModel *a,const EdModel *b){
 uint32_t an=0,bn=0;unsigned char *aa=edWireEncode(a,&an),*bb=edWireEncode(b,&bn);
 int equal=aa&&bb&&an==bn&&!std::memcmp(aa,bb,an);edFree(aa);edFree(bb);return equal;
}
static int vertexLimitError,surfaceLimitError,sectorLimitError;
void edClearGeometryError(void){vertexLimitError=surfaceLimitError=sectorLimitError=0;}
const char *edGeometryFailure(void){return sectorLimitError?"Placement exceeds the engine sector index range.":surfaceLimitError?"Placement exceeds the engine surface index range.":vertexLimitError?"Placement would exceed 24 vertices on a surface.":"Placement creates invalid geometry while rebuilding sector connections.";}
int edVertexLimitHit(void){return vertexLimitError;}
int edSurfaceVertexCountAllowed(int count){if(count>ED_SURFACE_VERTS){vertexLimitError=1;return 0;}return 1;}

EdVec edV(double x,double y,double z) { EdVec v={x,y,z}; return v; }
EdVec edAdd(EdVec a,EdVec b) { return edV(a.x+b.x,a.y+b.y,a.z+b.z); }
EdVec edSub(EdVec a,EdVec b) { return edV(a.x-b.x,a.y-b.y,a.z-b.z); }
EdVec edScale(EdVec a,double s) { return edV(a.x*s,a.y*s,a.z*s); }
double edDot(EdVec a,EdVec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
EdVec edCross(EdVec a,EdVec b) { return edV(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x); }
static double length(EdVec a) { return sqrt(edDot(a,a)); }
static EdVec unit(EdVec a) { double n=length(a); return n>ED_EPS?edScale(a,1/n):edV(0,0,0); }
static int same(EdVec a,EdVec b) { return length(edSub(a,b))<ED_EPS; }
EdVec edSnapPoint(EdVec p) {return edV(round(p.x/ED_GRID)*ED_GRID,round(p.y/ED_GRID)*ED_GRID,round(p.z/ED_GRID)*ED_GRID);}
static int projectionAxis(EdVec n) {
    if(fabs(n.z)>=fabs(n.x)&&fabs(n.z)>=fabs(n.y))return 2;
    return fabs(n.x)>fabs(n.y)?0:1;
}
static EdVec snapFaceGrid(const EdFace *face,EdVec p,double grid) {
    EdVec n=edNormal(face),q=edV(round(p.x/grid)*grid,round(p.y/grid)*grid,round(p.z/grid)*grid);double d=edDot(n,face->v[0].p);
    switch(projectionAxis(n)) {
        case 0:q.x=(d-n.y*q.y-n.z*q.z)/n.x;break;
        case 1:q.y=(d-n.x*q.x-n.z*q.z)/n.y;break;
        default:q.z=(d-n.x*q.x-n.y*q.y)/n.z;break;
    }
    return q;
}
EdVec edSnapFacePoint(const EdFace *face,EdVec p) {return snapFaceGrid(face,p,ED_GRID);}
EdVec edSnapMeshFacePoint(const EdFace *face,EdVec p) {return snapFaceGrid(face,p,ED_GRID*.5);}
EdSurfaceAttributes edSurfaceAttributes(const EdFace *face) {
    int portal=face->neighbor>=0;unsigned int solid=4|(edNormal(face).z>.5?1:0);
    EdSurfaceAttributes a=face->hasAttributes?face->attributes:EdSurfaceAttributes{solid,0,7,4,1,3,0,1,1,0,0};
    if(!face->hasAttributes){a.surfaceFlags=portal?0:solid;a.geometryMode=portal?0:4;}
    else if(a.portal!=portal){a.surfaceFlags=(a.surfaceFlags&~5u)|(portal?0:solid);a.geometryMode=portal?0:4;if(portal)a.adjoinFlags=7;}
    a.portal=portal;return a;
}
static EdVec texturePoint(const EdFace *face,EdVec p){int axis=projectionAxis(edNormal(face));return edV(axis==0?p.y:p.x,-(axis==2?p.y:p.z),0);}
static EdVec textureUV(const EdFace *face,EdVec p){
    if(face->customUV&&face->count>=3){
        EdVec a=edSub(face->v[1].p,face->v[0].p),b=edSub(face->v[2].p,face->v[0].p),q=edSub(p,face->v[0].p);
        double aa=edDot(a,a),ab=edDot(a,b),bb=edDot(b,b),det=aa*bb-ab*ab;
        if(fabs(det)>1e-20){double u=(edDot(q,a)*bb-edDot(q,b)*ab)/det,v=(edDot(q,b)*aa-edDot(q,a)*ab)/det;
            return edV(face->v[0].u+u*(face->v[1].u-face->v[0].u)+v*(face->v[2].u-face->v[0].u),face->v[0].v+u*(face->v[1].v-face->v[0].v)+v*(face->v[2].v-face->v[0].v),0);}
    }

    EdVec q=texturePoint(face,p);EdSurfaceAttributes a=edSurfaceAttributes(face);double angle=face->uvRotation*3.141592653589793/180,c=cos(angle),s=sin(angle);
    return edV((q.x*c+q.y*s)*ED_UV_SCALE/a.uScale+face->uOffset,(-q.x*s+q.y*c)*ED_UV_SCALE/a.vScale+face->vOffset,0);
}
void edMapFace(EdFace *face) {
    if(face->customUV)return;
    int axis=projectionAxis(edNormal(face));
    EdSurfaceAttributes a=edSurfaceAttributes(face);
    double angle=face->uvRotation*3.141592653589793/180,c=cos(angle),s=sin(angle);
    for(int i=0;i<face->count;i++) {
        EdVertex *v=&face->v[i];
        double x=axis==0?v->p.y:v->p.x,y=-(axis==2?v->p.y:v->p.z);
        v->u=(x*c+y*s)*ED_UV_SCALE/a.uScale+face->uOffset;
        v->v=(-x*s+y*c)*ED_UV_SCALE/a.vScale+face->vOffset;
    }
}
// Unfold the target plane around its intersection with the source plane.
// Mapping the unfolded points preserves texture scale and phase at the seam.
int edStitchTexture(EdFace *target,const EdFace *source){
    if(!target||!source||target->count<3||source->count<3)return 0;
    EdVec n=edNormal(source),t=edNormal(target),axis=edCross(t,n);
    double sine=length(axis),cosine=edDot(t,n);EdVec anchor=source->v[0].p;
    if(length(n)<.9||length(t)<.9||cosine<-.999999)return 0;
    if(sine>1e-8){
        axis=edScale(axis,1/sine);
        EdVec inPlane=edSub(t,edScale(n,cosine));
        anchor=edAdd(anchor,edScale(inPlane,edDot(t,edSub(target->v[0].p,anchor))/edDot(inPlane,inPlane)));
    }
    // Use actual stored UVs, including imported/custom mappings. Cleaved
    // polygons may start with three collinear corners, so find a valid basis.
    int b=-1,c=-1;EdVec aVec={},bVec={};double aa=0,ab=0,bb=0,det=0;
    for(int i=1;i<source->count&&b<0;i++)for(int j=i+1;j<source->count;j++){
        aVec=edSub(source->v[i].p,source->v[0].p);bVec=edSub(source->v[j].p,source->v[0].p);
        aa=edDot(aVec,aVec);ab=edDot(aVec,bVec);bb=edDot(bVec,bVec);det=aa*bb-ab*ab;
        if(det>1e-20){b=i;c=j;break;}
    }
    if(b<0)return 0;
    EdFace result=*target;
    for(int v=0;v<result.count;v++){
        EdVec point=result.v[v].p;
        if(sine>1e-8){EdVec q=edSub(point,anchor);point=edAdd(anchor,edAdd(edAdd(edScale(q,cosine),edScale(edCross(axis,q),sine)),edScale(axis,edDot(axis,q)*(1-cosine))));}
        EdVec q=edSub(point,source->v[0].p);double x=(edDot(q,aVec)*bb-edDot(q,bVec)*ab)/det,y=(edDot(q,bVec)*aa-edDot(q,aVec)*ab)/det;
        result.v[v].u=source->v[0].u+x*(source->v[b].u-source->v[0].u)+y*(source->v[c].u-source->v[0].u);
        result.v[v].v=source->v[0].v+x*(source->v[b].v-source->v[0].v)+y*(source->v[c].v-source->v[0].v);
        if(!isfinite(result.v[v].u)||!isfinite(result.v[v].v))return 0;
    }
    result.customUV=1;result.uOffset=result.vOffset=result.uvRotation=0;*target=result;return 1;
}
void edMapWorld(EdModel *m) {for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++){EdFace *face=&m->c[s].f[f];if(face->hasAttributes)face->attributes=edSurfaceAttributes(face);edMapFace(face);}}
void edShiftFaceUV(EdFace *face,EdVec movement) {
    movement=edSnapPoint(movement);
    EdVec uv=edSub(textureUV(face,movement),textureUV(face,edV(0,0,0)));
    if(face->customUV){for(int i=0;i<face->count;i++){face->v[i].u+=uv.x;face->v[i].v+=uv.y;}return;}
    face->uOffset+=uv.x;face->vOffset+=uv.y;edMapFace(face);
}
void edRotateFaceTexture(EdFace *face,double degrees,EdVec pivot){
    if(!isfinite(degrees))return;EdVec before=textureUV(face,pivot);
    if(face->customUV){double a=degrees*3.141592653589793/180,c=cos(a),s=sin(a);for(int i=0;i<face->count;i++){double u=face->v[i].u-before.x,v=face->v[i].v-before.y;face->v[i].u=before.x+u*c+v*s;face->v[i].v=before.y-u*s+v*c;}return;}
    face->uvRotation=remainder(face->uvRotation+degrees,360.0);EdVec after=textureUV(face,pivot);
    face->uOffset+=before.x-after.x;face->vOffset+=before.y-after.y;edMapFace(face);
}
void edRotateTextureDrag(EdFace *face,EdVec start,EdVec movement,int edge){
    EdVec center=edV(0,0,0);for(int i=0;i<face->count;i++)center=edAdd(center,face->v[i].p);center=edScale(center,1.0/face->count);
    double angle=0;
    if(edge>=0&&edge<face->count){EdVec d=edSub(texturePoint(face,face->v[(edge+1)%face->count].p),texturePoint(face,face->v[edge].p));
        angle=face->customUV?atan2(face->v[(edge+1)%face->count].v-face->v[edge].v,face->v[(edge+1)%face->count].u-face->v[edge].u)*180/3.141592653589793:remainder(atan2(d.y,d.x)*180/3.141592653589793-face->uvRotation,180.0);
    }else{
        EdVec a=edSub(texturePoint(face,start),texturePoint(face,center)),b=edAdd(a,texturePoint(face,movement));
        if(edDot(a,a)>ED_GRID*ED_GRID&&edDot(b,b)>ED_EPS*ED_EPS)angle=atan2(a.x*b.y-a.y*b.x,edDot(a,b))*180/3.141592653589793;
        else angle=texturePoint(face,movement).x*180;
    }
    edRotateFaceTexture(face,angle,center);
}
EdVec edDragDelta(EdVec startOrigin,EdVec startDirection,EdVec origin,EdVec direction,double depth,const EdVec *normal) {
    EdVec point=edAdd(startOrigin,edScale(unit(startDirection),depth)),hit;
    EdVec n=normal?unit(*normal):unit(startDirection);
    if(!edTracePlane(origin,direction,point,n,&hit))return edV(0,0,0);
    return edAdd(edSub(hit,point),edScale(n,edDot(edSub(origin,startOrigin),n)));
}
void edPanFaceTexture(EdFace *face,EdVec movement) {
    /* Keep the grabbed texel under the moving cursor: sampling offsets move
       opposite to the visible texture features. */
    edPanFaceTextureGrid(face,movement,ED_GRID);
}
void edPanFaceTextureGrid(EdFace *face,EdVec movement,double grid) {
    if(!isfinite(grid)||grid<=0)return;
    movement=edV(-round(movement.x/grid)*grid,-round(movement.y/grid)*grid,-round(movement.z/grid)*grid);
    EdVec uv=edSub(textureUV(face,movement),textureUV(face,edV(0,0,0)));
    if(face->customUV){for(int i=0;i<face->count;i++){face->v[i].u+=uv.x;face->v[i].v+=uv.y;}return;}
    face->uOffset+=uv.x;face->vOffset+=uv.y;edMapFace(face);
}
int edTracePlane(EdVec origin,EdVec direction,EdVec point,EdVec normal,EdVec *hit) {
    direction=unit(direction);normal=unit(normal);double den=edDot(normal,direction);
    if(fabs(den)<.01)return 0;
    double distance=edDot(normal,edSub(point,origin))/den;
    if(!isfinite(distance)||distance<.01||distance>1000)return 0;
    *hit=edAdd(origin,edScale(direction,distance));return 1;
}
void edDragTraceBegin(EdDragTrace *trace,EdVec origin,EdVec direction,EdVec point,EdVec normal,double maxDistance) {
    *trace=EdDragTrace{0};trace->point=point;trace->normal=normal;
    trace->origin=origin;trace->maxDistance=maxDistance;
    trace->valid=edTracePlane(origin,direction,point,normal,&trace->hit);
}
EdVec edDragTraceStep(EdDragTrace *trace,EdVec origin,EdVec direction) {
    EdVec travel=edSub(origin,trace->origin),hit;
    trace->point=edAdd(trace->point,travel);
    trace->hit=edAdd(trace->hit,travel);
    int valid=edTracePlane(origin,direction,trace->point,trace->normal,&hit);
    if(valid&&length(edSub(hit,origin))>trace->maxDistance)valid=0;
    EdVec delta=travel;
    if(valid&&trace->valid)delta=edAdd(delta,edSub(hit,trace->hit));
    if(valid)trace->hit=hit;
    trace->origin=origin;trace->valid=valid;
    return delta;
}
EdVec edSurfaceDragDelta(EdVec delta,EdVec normal) {
    EdVec n=unit(normal);
    EdVec up=edSub(edV(0,0,1),edScale(n,n.z));
    /* Horizontal surfaces need a tangent reference other than world-up. */
    if(length(up)<ED_EPS)up=edSub(edV(0,1,0),edScale(n,n.y));
    up=unit(up);EdVec right=unit(edCross(up,n));
    EdVec axis=right;double amount=edDot(delta,right);
    double vertical=edDot(delta,up),depth=edDot(delta,n);
    if(fabs(vertical)>fabs(amount)){axis=up;amount=vertical;}
    if(fabs(depth)>fabs(amount)){axis=n;amount=depth;}
    return edScale(axis,amount);
}
EdVec edNormal(const EdFace *f) {
    EdVec n=edV(0,0,0);
    for(int i=1;i+1<f->count;i++) n=edAdd(n,edCross(edSub(f->v[i].p,f->v[0].p),edSub(f->v[i+1].p,f->v[0].p)));
    /* Cross products measure area, so use the squared positional tolerance.
       Otherwise a small but valid boundary triangle is discarded as flat,
       leaving a crack when a rotated shape nearly meets the surface edge. */
    double magnitude=length(n);return magnitude>ED_EPS*ED_EPS?edScale(n,1/magnitude):edV(0,0,0);
}
static void reverse(EdFace *f) { for(int i=0;i<f->count/2;i++) { EdVertex v=f->v[i]; f->v[i]=f->v[f->count-i-1]; f->v[f->count-i-1]=v; } }
static int push(EdFace *f,EdVertex v) {
    if(f->count && same(f->v[f->count-1].p,v.p)) return 1;
    if(f->count>=ED_VERTS){vertexLimitError=1;return 0;}
    f->v[f->count++]=v; return 1;
}
/* Sutherland-Hodgman, retaining the positive half-space and interpolating UVs. */
static int clip(const EdFace *in,EdVec normal,double d,EdFace *out) {
    *out=*in; out->count=0; out->neighbor=out->mirror=-1;
    for(int i=0;i<in->count;i++) {
        EdVertex a=in->v[i],b=in->v[(i+1)%in->count];
        double da=edDot(normal,a.p)-d,db=edDot(normal,b.p)-d;
        if(da>=-ED_EPS && !push(out,a)) return 0;
        if((da>ED_EPS && db<-ED_EPS)||(da<-ED_EPS && db>ED_EPS)) {
            double t=da/(da-db); EdVertex v;
            v.p=edAdd(a.p,edScale(edSub(b.p,a.p),t)); v.u=a.u+(b.u-a.u)*t; v.v=a.v+(b.v-a.v)*t; v.light=a.light+(b.light-a.light)*t;
            if(!push(out,v)) return 0;
        }
    }
    if(out->count>1 && same(out->v[0].p,out->v[out->count-1].p)) --out->count;
    if(out->count<3 || length(edNormal(out))<0.5) out->count=0;
    /* Rounded boundary points can leave a polygon thinner than the positional
       tolerance. Such slivers may even have an inverted normal; they are edge
       contact, not a new face. Use width rather than an absolute area cutoff
       so small, well-shaped faces remain supported. */
    if(out->count){
        EdVec area=edV(0,0,0);double perimeter=0;
        for(int v=0;v<out->count;v++){EdVec a=out->v[v].p,b=out->v[(v+1)%out->count].p;area=edAdd(area,edCross(edSub(a,out->v[0].p),edSub(b,out->v[0].p)));perimeter+=length(edSub(b,a));}
        if(length(area)<=ED_EPS*perimeter)out->count=0;
    }
    return 1;
}
static int addface(EdCell *c,EdFace f) {
    if(!f.count) return 1;
    if(c->count==ED_FACES){surfaceLimitError=1;return 0;}
    f.neighbor=f.mirror=-1; c->f[c->count++]=f; return 1;
}
/* Rebuild a convex hull after vertex/edge edits so twisted quads acquire new
   planar faces. UVs travel with the vertices; coplanar hull planes are emitted once. */
static int hull(EdCell *cell,double epsilon=ED_EPS) {
    EdScratch<EdVertex> vertices;int count=0;EdCell *out=(EdCell*)edCalloc(1,sizeof(EdCell));if(!out)return 0;
    for(int f=0;f<cell->count;f++)for(int v=0;v<cell->f[f].count;v++) {EdVertex p=cell->f[f].v[v];int found=0;if(!isfinite(p.p.x)||!isfinite(p.p.y)||!isfinite(p.p.z)){edFree(out);return 0;}for(int i=0;i<count;i++)if(same(p.p,vertices[i].p))found=1;if(!found)vertices[count++]=p;}
    if(count>96){edFree(out);return 0;}
    for(int a=0;a<count;a++)for(int b=a+1;b<count;b++)for(int c=b+1;c<count;c++) {
        EdVec n=unit(edCross(edSub(vertices[b].p,vertices[a].p),edSub(vertices[c].p,vertices[a].p)));if(length(n)<.5)continue;double d=edDot(n,vertices[a].p);int pos=0,neg=0;
        for(int i=0;i<count;i++){double t=edDot(n,vertices[i].p)-d;if(t>epsilon)pos=1;if(t<-epsilon)neg=1;}if(pos&&neg)continue;if(!pos&&!neg)continue;if(neg){n=edScale(n,-1);d=-d;}
        int found=0;for(int f=0;f<out->count;f++)if(edDot(n,edNormal(&out->f[f]))>(epsilon<ED_EPS?1-1e-12:.99999)&&fabs(d-edDot(n,out->f[f].v[0].p))<epsilon)found=1;if(found)continue;
        EdFace face={0};double best=-1e9;
        for(int f=0;f<cell->count;f++){double score=edDot(n,edNormal(&cell->f[f]))-fabs(edDot(n,cell->f[f].v[0].p)-d);if(score>best){best=score;face.material=cell->f[f].material;face.uOffset=cell->f[f].uOffset;face.vOffset=cell->f[f].vOffset;face.uvRotation=cell->f[f].uvRotation;face.hasAttributes=cell->f[f].hasAttributes;face.attributes=cell->f[f].attributes;}}
        /* A supporting plane can contain interior cleave vertices. Angular
         * sorting alone puts those vertices on the perimeter and creates holes.
         * Build the actual 2D convex hull, excluding interior/collinear points. */
        EdVec u=unit(edCross(n,fabs(n.z)<.9?edV(0,0,1):edV(0,1,0))),v=edCross(n,u);
        int sorted[96],num=0,chain[192],k=0;
        for(int i=0;i<count;i++)if(fabs(edDot(n,vertices[i].p)-d)<epsilon){
            double x=edDot(vertices[i].p,u),y=edDot(vertices[i].p,v);int j=num;
            while(j>0){EdVec q=vertices[sorted[j-1]].p;double qx=edDot(q,u),qy=edDot(q,v);if(qx<x||(qx==x&&qy<=y))break;sorted[j]=sorted[j-1];j--;}
            sorted[j]=i;num++;
        }
        for(int i=0;i<num;i++){
            while(k>=2&&edDot(edCross(edSub(vertices[chain[k-1]].p,vertices[chain[k-2]].p),edSub(vertices[sorted[i]].p,vertices[chain[k-1]].p)),n)<=epsilon*epsilon)k--;
            chain[k++]=sorted[i];
        }
        int upper=k+1;
        for(int i=num-2;i>=0;i--){
            while(k>=upper&&edDot(edCross(edSub(vertices[chain[k-1]].p,vertices[chain[k-2]].p),edSub(vertices[sorted[i]].p,vertices[chain[k-1]].p)),n)<=epsilon*epsilon)k--;
            chain[k++]=sorted[i];
        }
        if(k>1)k--;for(int i=0;i<k;i++)if(!push(&face,vertices[chain[i]])){edFree(out);return 0;}
        if(!addface(out,face)){edFree(out);return 0;}
    }
    edCopy((char*)out+offsetof(EdCell,ambientLight),(char*)cell+offsetof(EdCell,ambientLight),sizeof(EdCell)-offsetof(EdCell,ambientLight));
    *cell=*out;edFree(out);return cell->count>=4;
}
typedef struct {double t;int delta;} EdBoundaryEvent;
static int boundaryEventCompare(const void *a,const void *b) {
    double delta=((const EdBoundaryEvent*)a)->t-((const EdBoundaryEvent*)b)->t;return delta<0?-1:delta>0?1:0;
}
/* Every directed boundary segment must cancel against the neighboring faces.
 * Split edges are allowed, so cleaves may terminate at a wall/ceiling T junction.
 * Convexity and volume alone cannot detect a missing coplanar triangle. */
static int closedCell(const EdCell *cell) {
    double epsilon=fmax(ED_EPS,cell->tolerance);
    EdScratch<EdBoundaryEvent> events;
    for(int f=0;f<cell->count;f++)for(int v=0;v<cell->f[f].count;v++){
        EdVec a=cell->f[f].v[v].p,b=cell->f[f].v[(v+1)%cell->f[f].count].p,e=edSub(b,a);double size=length(e),den=edDot(e,e);
        if(size<epsilon)continue;int count=2;events[0]=EdBoundaryEvent{0,1};events[1]=EdBoundaryEvent{1,-1};
        for(int g=0;g<cell->count;g++)for(int w=0;w<cell->f[g].count;w++){
            if(f==g&&v==w)continue;
            EdVec p=cell->f[g].v[w].p,q=cell->f[g].v[(w+1)%cell->f[g].count].p;
            if(length(edCross(edSub(p,a),e))>epsilon*size||length(edCross(edSub(q,a),e))>epsilon*size)continue;
            double t0=edDot(edSub(p,a),e)/den,t1=edDot(edSub(q,a),e)/den;
            double lo=fmax(0,fmin(t0,t1)),hi=fmin(1,fmax(t0,t1));if(hi-lo<epsilon/size)continue;
            int sign=t1>t0?1:-1;events[count++]=EdBoundaryEvent{lo,sign};events[count++]=EdBoundaryEvent{hi,-sign};
        }
        qsort(events,count,sizeof(events[0]),boundaryEventCompare);int balance=0;
        for(int i=0;i<count;i++){balance+=events[i].delta;if(i+1<count&&events[i+1].t-events[i].t>epsilon/size&&balance)return 0;}
    }
    return 1;
}
/* Reconstruct a closed convex boundary, retaining valid material/cleave patches
 * on each supporting plane. Only uncovered pieces receive new faces; overlaps
 * are trimmed so inverted/collapsed weld triangles cannot leave a hole. */
static int repairCell(EdCell *cell) {
    EdCell *storage=edCalloc(4,sizeof(EdCell));if(!storage)return 0;
    EdCell *boundary=&storage[0],*result=&storage[1],*remaining=&storage[2],*next=&storage[3];
    *boundary=*cell;int ok=hull(boundary);
    for(int h=0;ok&&h<boundary->count;h++){
        EdFace plane=boundary->f[h];EdVec normal=edNormal(&plane);remaining->count=1;remaining->f[0]=plane;
        for(int f=0;ok&&f<cell->count&&remaining->count;f++){
            EdFace patch=cell->f[f];EdVec n=edNormal(&patch);if(patch.count<3||fabs(edDot(n,normal))<.99999)continue;
            int planar=1;for(int v=0;v<patch.count;v++)if(fabs(edDot(normal,edSub(patch.v[v].p,plane.v[0].p)))>ED_EPS*4)planar=0;
            if(!planar)continue;if(edDot(n,normal)<0)reverse(&patch);
            int convex=1;for(int v=0;v<patch.count;v++)if(edDot(edCross(edSub(patch.v[(v+1)%patch.count].p,patch.v[v].p),edSub(patch.v[(v+2)%patch.count].p,patch.v[(v+1)%patch.count].p)),normal)<-ED_EPS)convex=0;
            if(!convex)continue;next->count=0;
            for(int r=0;ok&&r<remaining->count;r++){
                EdFace inside=remaining->f[r],outside,tmp={0};
                for(int e=0;ok&&e<patch.count&&inside.count;e++){
                    EdVec edge=edSub(patch.v[(e+1)%patch.count].p,patch.v[e].p);if(length(edge)<ED_EPS)continue;
                    EdVec en=unit(edCross(normal,edge));double d=edDot(en,patch.v[e].p);
                    ok=clip(&inside,edScale(en,-1),-d,&outside)&&clip(&inside,en,d,&tmp)&&addface(next,outside);inside=tmp;
                }
                if(ok&&inside.count){inside.customUV=patch.customUV;if(patch.customUV)for(int v=0;v<inside.count;v++){EdVec uv=textureUV(&patch,inside.v[v].p);for(int k=0;k<patch.count;k++)if(same(inside.v[v].p,patch.v[k].p)){uv=edV(patch.v[k].u,patch.v[k].v,0);break;}inside.v[v].u=uv.x;inside.v[v].v=uv.y;}inside.material=patch.material;inside.uOffset=patch.uOffset;inside.vOffset=patch.vOffset;inside.uvRotation=patch.uvRotation;inside.hasAttributes=patch.hasAttributes;inside.attributes=patch.attributes;ok=addface(result,inside);}
            }
            EdCell *swap=remaining;remaining=next;next=swap;
        }
        for(int r=0;ok&&r<remaining->count;r++)ok=addface(result,remaining->f[r]);
    }
    edCopy((char*)result+offsetof(EdCell,ambientLight),(char*)cell+offsetof(EdCell,ambientLight),sizeof(EdCell)-offsetof(EdCell,ambientLight));
    if(ok)ok=closedCell(result);if(ok)*cell=*result;edFree(storage);return ok;
}
static double volume(const EdCell *c) {
    double v=0;
    for(int i=0;i<c->count;i++) { const EdFace *f=&c->f[i];
        for(int j=1;j+1<f->count;j++) v+=edDot(f->v[0].p,edCross(f->v[j].p,f->v[j+1].p))/6;
    } return fabs(v);
}
static EdVec capProject(EdVec p,int axis){return axis==0?edV(p.y,p.z,0):axis==1?edV(p.z,p.x,0):edV(p.x,p.y,0);}
static double capTurn(EdVertex a,EdVertex b,EdVertex c,int axis){
    EdVec p=capProject(a.p,axis),q=capProject(b.p,axis),r=capProject(c.p,axis);return (q.x-p.x)*(r.y-p.y)-(q.y-p.y)*(r.x-p.x);
}
static int capHull(EdFace *cap,EdVertex *points,int count,EdVec normal){
    int axis=projectionAxis(normal),size=0;EdScratch<int> hull;
    /* Face partitions can contribute interior vertices, not just the perimeter.
       A radial sort would weave those vertices into a concave/self-crossing cap. */
    for(int i=1;i<count;i++){EdVertex point=points[i];EdVec p=capProject(point.p,axis);int j=i;
        while(j>0){EdVec q=capProject(points[j-1].p,axis);if(q.x<p.x||(q.x==p.x&&q.y<=p.y))break;points[j]=points[j-1];j--;}points[j]=point;
    }
    for(int i=0;i<count;i++){while(size>=2&&capTurn(points[hull[size-2]],points[hull[size-1]],points[i],axis)<=ED_EPS*ED_EPS)size--;hull[size++]=i;}
    int lower=size;for(int i=count-2;i>=0;i--){while(size>lower&&capTurn(points[hull[size-2]],points[hull[size-1]],points[i],axis)<=ED_EPS*ED_EPS)size--;hull[size++]=i;}
    if(size>1)size--;cap->count=0;if(size<3)return 1;if(size>ED_VERTS){edSurfaceVertexCountAllowed(size);return 0;}
    for(int i=0;i<size;i++)cap->v[cap->count++]=points[hull[i]];
    if(edDot(edNormal(cap),normal)<0)reverse(cap);return 1;
}
/* Clip a convex cell, constructing the newly exposed cap with an inward normal. */
static int cellclip(const EdCell *in,EdVec n,double d,EdCell *out) {
    int positive=0,negative=0;
    for(int f=0;f<in->count;f++)for(int v=0;v<in->f[f].count;v++){
        double side=edDot(n,in->f[f].v[v].p)-d;
        positive|=side>ED_EPS;negative|=side<-ED_EPS;
    }
    /* A tangent plane does not cut a volume. In particular, rounded points
       on a shared edge must not manufacture a thin, sideways-facing cap. */
    if(!positive||!negative){
        *out=*in;if(!positive&&negative)out->count=0;
        for(int f=0;f<out->count;f++)out->f[f].neighbor=out->f[f].mirror=-1;
        return 1;
    }
    EdFace cap={0};EdScratch<EdVertex> points;int count=0;*out=*in;out->count=0;
    cap.material=in->f[0].material;
    cap.hasAttributes=in->f[0].hasAttributes;cap.attributes=in->f[0].attributes;
    cap.uOffset=in->f[0].uOffset;cap.vOffset=in->f[0].vOffset;cap.uvRotation=in->f[0].uvRotation;
    for(int i=0;i<in->count;i++) {
        EdFace f;
        if(!clip(&in->f[i],n,d,&f)||!addface(out,f)) return 0;
        for(int j=0;j<f.count;j++) if(fabs(edDot(n,f.v[j].p)-d)<ED_EPS*2) {
            int found=0;for(int k=0;k<count;k++)if(same(points[k].p,f.v[j].p))found=1;
            if(!found){if(count==INT_MAX)return 0;points[count++]=f.v[j];}
        }
    }
    /* An unchanged face on the clipping plane is already a cap. */
    for(int i=0;i<out->count;i++)if(edDot(edNormal(&out->f[i]),n)>.99999){
        // One corner on the plane does not make a tilted face a cap. Skipping
        // the new cap in that case leaves the clipped half of a sector open.
        bool coplanar=true;for(int v=0;v<out->f[i].count;v++)if(fabs(edDot(n,out->f[i].v[v].p)-d)>=ED_EPS){coplanar=false;break;}
        if(coplanar)return 1;
    }
    if(!capHull(&cap,points,count,n))return 0;
    if(cap.count>=3) {
        if(cap.hasAttributes)cap.attributes.surfaceFlags=(cap.attributes.surfaceFlags&~1u)|(n.z>.5?1u:0u);
        if(!addface(out,cap)) return 0;
    }
    if(out->count<4 || volume(out)<ED_EPS*ED_EPS) out->count=0;
    return 1;
}
void edDefault(EdModel *m) {
    edZero(m,0,sizeof(*m)); m->count=1; m->spawn=edV(0,0,0.12);
    m->spawnCount=1;m->spawns[0].position=m->spawn;
    strcpy(m->header.title,"New Editor Level");strcpy(m->header.colormap,"dflt.cmp");
    m->header.gravity=4;m->header.ceilingSkyZ=15;m->header.horizonDistance=100;m->header.horizonPixels=768;
    for(int i=0;i<4;i++){m->header.mip[i]=i+1;m->header.lod[i]=.3*(i+1);}
    m->header.perspective=m->header.gouraud=2;
    const int indices[6][4]={{0,1,2,3},{7,6,5,4},{4,5,1,0},{1,5,6,2},{2,6,7,3},{3,7,4,0}};
    EdVec p[8]={ {-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1} };
    m->c[0].count=6;m->c[0].extraLight=.5;
    for(int i=0;i<6;i++) { EdFace *f=&m->c[0].f[i]; f->count=4; f->neighbor=f->mirror=-1;
        for(int j=0;j<4;j++) { f->v[j].p=p[indices[i][j]]; f->v[j].u=(j==1||j==2)?128:0; f->v[j].v=j>=2?128:0; }
    }
    edMapWorld(m);
}
int edContains(const EdCell *c,EdVec p) {
    for(int i=0;i<c->count;i++) if(edDot(edNormal(&c->f[i]),edSub(p,c->f[i].v[0].p))<-fmax(ED_EPS,c->tolerance)) return 0;
    return c->count>=4;
}
static int edResourceText(const char *text,size_t capacity){
    const char *end=(const char*)memchr(text,0,capacity);if(!end)return 0;for(const char *p=text;p<end;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'||*p=='.'||*p=='+'))return 0;return 1;
}
int edValidate(const EdModel *m) {
    if(!edSplineValidate(m)||!edGroupsValidate(m))return 0;
    if(!edMeshValidate(m)){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
    if(m->count<1||m->count>ED_CELLS) {if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
    if(m->objectCount<0||m->objectCount>ED_OBJECTS||m->spawnCount<0||m->spawnCount>ED_SPAWNS){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
    for(int i=0;i<m->spawnCount+m->objectCount;i++){
        const EdThingAttributes *a=i<m->spawnCount?&m->spawns[i].attributes:&m->objects[i-m->spawnCount].attributes;
        if(!edResourceText(a->aiClass,sizeof(a->aiClass))||a->aiFrames<0||a->aiFrames>1||a->hasVelocity<0||a->hasVelocity>1||!isfinite(a->velocity.x)||!isfinite(a->velocity.y)||!isfinite(a->velocity.z))return 0;
        if(a->frameCount<0||a->frameCount>ED_FRAMES){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
        for(int j=0;j<a->frameCount;j++){EdFrame q=a->frames[j];if(!isfinite(q.position.x)||!isfinite(q.position.y)||!isfinite(q.position.z)||!isfinite(q.rotation.x)||!isfinite(q.rotation.y)||!isfinite(q.rotation.z)){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}}
    }
    if(m->cogCount<0||m->cogCount>ED_COGS){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
    for(int i=0;i<m->cogCount;i++){const EdPlacedCog *c=&m->cogs[i];if(!c->file[0]||c->count<0||c->count>ED_COG_PARAMS){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
        for(const char *s=c->file;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='_'||*s=='-'||*s=='.')){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
        for(int j=0;j<c->count;j++){const EdCogParam *p=&c->params[j];if(p->type<2||p->type>14){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}char value[32];EdCogParam check=*p;edCogParamValue(m,p,value);if(!edCogSetValue(m,&check,value)){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}}
    }
    if(m->lightCount<0||m->lightCount>ED_LIGHTS){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
    for(int i=0;i<m->lightCount;i++){const EdLight *l=&m->lights[i];if(!isfinite(l->position.x)||!isfinite(l->position.y)||!isfinite(l->position.z)||!isfinite(l->intensity)||!isfinite(l->range)||l->intensity<0||l->intensity>100||l->range<ED_GRID||l->range>10000){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}}
    for(int s=0;s<m->count;s++) { const EdCell *c=&m->c[s];
        if(!edResourceText(c->colormap,sizeof(c->colormap))||!memchr(c->layer,0,sizeof(c->layer))||!edResourceText(c->sound,sizeof(c->sound))||!isfinite(c->soundVolume)||!isfinite(c->tint.x)||!isfinite(c->tint.y)||!isfinite(c->tint.z)||!isfinite(c->thrust.x)||!isfinite(c->thrust.y)||!isfinite(c->thrust.z))return 0;
        for(const char *p=c->colormap;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'||*p=='.'))return 0;
        for(const char *p=c->layer;*p;p++)if((unsigned char)*p<32||*p=='"')return 0;
        if(!isfinite(c->tolerance)||c->tolerance<0||c->tolerance>ED_IMPORT_EPS||c->count<4||c->count>ED_FACES||!isfinite(c->ambientLight)||!isfinite(c->extraLight)||c->ambientLight<0||c->ambientLight>1||c->extraLight<0||c->extraLight>1){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
        for(int f=0;f<c->count;f++)if(!edSurfaceVertexCountAllowed(c->f[f].count)||c->f[f].count<3){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
        if(volume(c)<ED_EPS*ED_EPS){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
        for(int f=0;f<c->count;f++) { const EdFace *p=&c->f[f];if(!edSurfaceVertexCountAllowed(p->count)){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}EdVec n=edNormal(p);
            EdSurfaceAttributes a=edSurfaceAttributes(p);
            if((p->customUV!=0&&p->customUV!=1)||!isfinite(p->uvRotation)||!isfinite(a.uScale)||!isfinite(a.vScale)||fabs(a.uScale)<.000001||fabs(a.vScale)<.000001||!isfinite(a.extraLight)||!isfinite(a.adjoinDistance)||a.adjoinDistance<0||a.geometryMode<0||a.geometryMode>4||a.textureMode<0||a.textureMode>4||a.lightingMode<0||a.lightingMode>6){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation attributes s=%d f=%d u=%g v=%g dist=%g modes=%d/%d/%d\n",s,f,a.uScale,a.vScale,a.adjoinDistance,a.geometryMode,a.textureMode,a.lightingMode);return 0;}
            if(p->count<3||p->count>ED_VERTS||length(n)<.5) {if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
            for(int j=0;j<p->count;j++) {
                EdVec q=p->v[j].p;
                if(!isfinite(p->v[j].light)||!isfinite(p->v[j].u)||!isfinite(p->v[j].v)||!isfinite(q.x)||!isfinite(q.y)||!isfinite(q.z)||fabs(edDot(n,edSub(q,p->v[0].p)))>fmax(ED_EPS*4,c->tolerance)||!edContains(c,q)) {if(getenv("GEOSITH_STOCK_TEST")){double worst=0;int wf=-1;for(int k=0;k<c->count;k++){double d=edDot(edNormal(&c->f[k]),edSub(q,c->f[k].v[0].p));if(d<worst){worst=d;wf=k;}}fprintf(stderr,"Worst plane %g face %d\n",worst,wf);fprintf(stderr,"Validation failure line %d sector=%d face=%d vertex=%d plane=%.9g contains=%d\n",__LINE__,s,f,j,fabs(edDot(n,edSub(q,p->v[0].p))),edContains(c,q));}return 0;}
                EdVec a=edSub(p->v[(j+1)%p->count].p,q),b=edSub(p->v[(j+2)%p->count].p,p->v[(j+1)%p->count].p);
                if(edDot(edCross(a,b),n)<-fmax(ED_EPS,c->tolerance*(length(a)+length(b)))) {if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation failure line %d\n",__LINE__);return 0;}
            }
        }
        if(!closedCell(c)){if(getenv("GEOSITH_STOCK_TEST"))fprintf(stderr,"Validation open sector %d faces %d\n",s,c->count);return 0;}
    } return 1;
}
static int rayFaces(const EdModel *m,EdVec o,EdVec d,EdHit *h,int includeAdjoins) {
    h->cell=h->face=h->vertex=h->edge=-1; h->distance=1000;
    for(int s=0;s<m->count;s++) for(int i=0;i<m->c[s].count;i++) {
        const EdFace *f=&m->c[s].f[i];
        if(f->neighbor>=0&&(!includeAdjoins||(includeAdjoins==2&&edSurfaceAttributes(f).geometryMode==0))) continue;
        EdVec n=edNormal(f); double den=edDot(n,d); if(den>=-ED_EPS) continue;
        double t=edDot(n,edSub(f->v[0].p,o))/den; if(t<0||t>=h->distance) continue;
        EdVec p=edAdd(o,edScale(d,t)); int inside=1;
        for(int j=0;j<f->count;j++) if(edDot(edCross(edSub(f->v[(j+1)%f->count].p,f->v[j].p),edSub(p,f->v[j].p)),n)<-ED_EPS) inside=0;
        if(inside) { h->cell=s; h->face=i; h->distance=t; h->point=p; }
    }
    if(h->cell<0) return 0;
    /* Materials target the intersected face, including its corners and edges. */
    if(includeAdjoins==2){const EdFace *face=&m->c[h->cell].f[h->face];double best=fmin(.04,h->distance*.018);
        for(int j=0;j<face->count;j++){EdVec a=face->v[j].p,e=edSub(face->v[(j+1)%face->count].p,a);double den=edDot(e,e);if(den<ED_EPS*ED_EPS)continue;
            double t=fmax(0,fmin(1,edDot(edSub(h->point,a),e)/den)),dist=length(edSub(h->point,edAdd(a,edScale(e,t))));if(dist<best){best=dist;h->edge=j;}}
        return 1;
    }
    const EdFace *f=&m->c[h->cell].f[h->face]; double threshold=fmin(.04,h->distance*.018),best=threshold;
    /* A cleave corner may belong to the wall while the ray hits its ceiling. */
    for(int face=0;face<m->c[h->cell].count;face++)for(int j=0;j<m->c[h->cell].f[face].count;j++) {
        double distance=length(edSub(m->c[h->cell].f[face].v[j].p,h->point));
        if(distance<best){best=distance;h->face=face;h->vertex=j;}
    }
    f=&m->c[h->cell].f[h->face];
    if(h->vertex<0) for(int j=0;j<f->count;j++) {
        EdVec a=f->v[j].p,b=f->v[(j+1)%f->count].p,e=edSub(b,a); double t=edDot(edSub(h->point,a),e)/edDot(e,e);
        t=fmax(0,fmin(1,t)); double dist=length(edSub(h->point,edAdd(a,edScale(e,t)))); if(dist<best) { best=dist; h->edge=j; }
    } return 1;
}
int edRay(const EdModel *m,EdVec o,EdVec d,EdHit *h) {return rayFaces(m,o,d,h,0);}
int edRaySelection(const EdModel *m,EdVec o,EdVec d,EdHit *h) {return rayFaces(m,o,d,h,1);}
int edRayMaterial(const EdModel *m,EdVec o,EdVec d,EdHit *h) {return rayFaces(m,o,d,h,2);}
double edVertexLight(const EdModel *m,int cell,int face,int vertex) {
    const EdCell *c=&m->c[cell];const EdFace *f=&c->f[face];EdVec point=f->v[vertex].p,normal=edNormal(f),center=edV(0,0,0);int count=0;
    if(!m->lightCount)return f->v[vertex].light;
    for(int i=0;i<c->count;i++)for(int v=0;v<c->f[i].count;v++){center=edAdd(center,c->f[i].v[v].p);count++;}
    /* Move the visibility endpoint inside every incident wall at a corner. */
    EdVec sample=edAdd(point,edScale(unit(edSub(edScale(center,1.0/count),point)),ED_EPS*16));double value=f->v[vertex].light;
    for(int i=0;i<m->lightCount;i++){
        const EdLight *l=&m->lights[i];EdVec delta=edSub(l->position,point);double distance=length(delta);
        if(l->intensity<=0||distance>=l->range)continue;
        double incidence=distance>ED_EPS?fmax(0,edDot(normal,edScale(delta,1.0/distance))):1;if(incidence<=0)continue;
        EdVec travel=edSub(sample,l->position);double traceDistance=length(travel);EdHit hit;
        if(traceDistance>ED_EPS&&edRay(m,l->position,edScale(travel,1.0/traceDistance),&hit)&&hit.distance<traceDistance-ED_EPS*8)continue;
        value+=l->intensity*(1-distance/l->range)*incidence;
    }
    return fmin(1,value);
}
int edVertexSlideAxis(const EdModel *m,EdHit hit,EdVec *axis) {
    if(hit.cell<0||hit.cell>=m->count||hit.vertex<0)return 0;
    EdVec point=m->c[hit.cell].f[hit.face].v[hit.vertex].p;
    for(int f=0;f<m->c[hit.cell].count;f++)for(int v=0;v<m->c[hit.cell].f[f].count;v++){
        const EdFace *face=&m->c[hit.cell].f[f];if(!same(face->v[v].p,point))continue;
        EdVec a=edSub(point,face->v[(v+face->count-1)%face->count].p),b=edSub(face->v[(v+1)%face->count].p,point);
        if(length(edCross(a,b))<ED_EPS*(length(a)+length(b))&&edDot(a,b)>0){*axis=unit(a);return 1;}
    }
    for(int f=0;f<m->c[hit.cell].count;f++)for(int v=0;v<m->c[hit.cell].f[f].count;v++) {
        const EdFace *face=&m->c[hit.cell].f[f];EdVec a=face->v[v].p,b=face->v[(v+1)%face->count].p,e=edSub(b,a);double len=length(e);
        if(len<ED_EPS)continue;double t=edDot(edSub(point,a),e)/edDot(e,e);
        if(t>ED_EPS&&t<1-ED_EPS&&length(edCross(edSub(point,a),e))<ED_EPS*len){*axis=unit(e);return 1;}
    }
    return 0;
}
EdVec edVertexEdgeDelta(const EdModel *m,EdHit hit,EdVec delta) {
    if(hit.cell<0||hit.cell>=m->count||hit.face<0||hit.face>=m->c[hit.cell].count||hit.vertex<0||hit.vertex>=m->c[hit.cell].f[hit.face].count)return edV(0,0,0);
    EdVec point=m->c[hit.cell].f[hit.face].v[hit.vertex].p,preferred,best=edV(0,0,0);double score=0,limit=0;
    int sliding=edVertexSlideAxis(m,hit,&preferred);
    for(int f=0;f<m->c[hit.cell].count;f++)for(int v=0;v<m->c[hit.cell].f[f].count;v++){
        const EdFace *face=&m->c[hit.cell].f[f];EdVec a=face->v[v].p,b=face->v[(v+1)%face->count].p,e=edSub(b,a);double len=length(e);
        if(len<ED_EPS||length(edCross(edSub(point,a),e))>ED_EPS*len)continue;
        double t=edDot(edSub(point,a),e)/edDot(e,e);if(t<-ED_EPS||t>1+ED_EPS)continue;
        EdVec ends[2]={a,b};for(int side=0;side<2;side++){
            EdVec toward=edSub(ends[side],point);double distance=length(toward);if(distance<ED_EPS)continue;
            EdVec axis=edScale(toward,1/distance);if(sliding&&fabs(edDot(axis,preferred))<.99999)continue;
            double amount=edDot(delta,axis);
            if(amount>score+ED_EPS||(amount>0&&fabs(amount-score)<ED_EPS&&distance<limit)){score=amount;best=axis;limit=distance;}
        }
    }
    double distance=score>=limit-ED_GRID*.5?limit:fmin(limit,fmax(0,round(score/ED_GRID)*ED_GRID));
    return edScale(best,distance);
}
int edCleave(EdModel *m,int s,int f,EdVec a,EdVec b) {
    if(s>=0&&s<m->count&&m->c[s].splineId)return 0;
    if(s<0||s>=m->count||f<0||f>=m->c[s].count) return 0;
    EdCell *c=&m->c[s]; EdFace original=c->f[f],left,right;
    a=edSnapFacePoint(&original,a);b=edSnapFacePoint(&original,b);
    if(original.neighbor>=0 || length(edSub(a,b))<ED_EPS) return 0;
    EdVec n=unit(edCross(edNormal(&original),edSub(b,a))); double d=edDot(n,a);
    if(!clip(&original,n,d,&left)||!clip(&original,edScale(n,-1),-d,&right)||!left.count||!right.count||c->count==ED_FACES) return 0;
    if(!edSurfaceVertexCountAllowed(left.count)||!edSurfaceVertexCountAllowed(right.count))return 0;
    c->f[f]=left; c->f[c->count++]=right;edMapWorld(m); return 1;
}
int edShapePoints(const EdFace *face,EdVec center,double radius,int sides,double degrees,EdVec points[32]){
    if(!face||sides<3||sides>32||!isfinite(degrees)||!isfinite(radius)||radius<ED_GRID||!isfinite(center.x)||!isfinite(center.y)||!isfinite(center.z))return 0;
    EdVec n=edNormal(face),up=edSub(edV(0,0,1),edScale(n,n.z));
    if(length(up)<.01)up=edSub(edV(0,1,0),edScale(n,n.y));up=unit(up);EdVec right=unit(edCross(up,n));
    center=edSnapFacePoint(face,center);radius=round(radius/ED_GRID)*ED_GRID;
    double start=1.5707963267948966+(sides%2?0:3.141592653589793/sides)+fmod(degrees,360)*3.141592653589793/180;
    for(int i=0;i<sides;i++){double angle=start+6.283185307179586*i/sides;points[i]=edAdd(center,edScale(edAdd(edScale(right,cos(angle)),edScale(up,sin(angle))),radius));}
    return 1;
}
int edCleaveShape(EdModel *m,int s,int f,EdVec center,double radius,int sides){return edCleaveShapeRotated(m,s,f,center,radius,sides,0);}
int edClipShape(const EdFace *face,EdVec center,double radius,int sides,double degrees,EdFace *outline){
    EdVec points[32];if(!edShapePoints(face,center,radius,sides,degrees,points))return 0;
    *outline=*face;outline->count=sides;outline->neighbor=outline->mirror=-1;
    for(int i=0;i<sides;i++)outline->v[i]=EdVertex{.p=points[i]};
    EdVec n=edNormal(face);int previousError=vertexLimitError,result=1;
    for(int e=0;e<face->count&&outline->count;e++){
        EdVec edge=unit(edCross(n,edSub(face->v[(e+1)%face->count].p,face->v[e].p)));EdFace clipped;
        if(!clip(outline,edge,edDot(edge,face->v[e].p),&clipped)){result=-1;break;}*outline=clipped;
    }
    /* Drawing a preview must not raise an edit error. The cleave checks the
       resulting count separately, after all boundary clipping is complete. */
    vertexLimitError=previousError;return result<0?result:outline->count>=3;
}
int edCleaveShapeRotated(EdModel *m,int s,int f,EdVec center,double radius,int sides,double degrees){
    if(s>=0&&s<m->count&&m->c[s].splineId)return 0;
    if(s<0||s>=m->count||f<0||f>=m->c[s].count)return 0;
    const EdFace *original=&m->c[s].f[f];EdFace outline;EdVec n=edNormal(original);
    if(original->neighbor>=0)return 0;int clipped=edClipShape(original,center,radius,sides,degrees,&outline);
    if(clipped<0){edSurfaceVertexCountAllowed(ED_VERTS+1);return 0;}
    if(!clipped||!edSurfaceVertexCountAllowed(outline.count))return 0;
    /* Partition around the actual intersection. Edges inherited from the
       original boundary produce no outside piece, leaving a flat floor/side. */
    EdCell *out=edAlloc(sizeof(*out));if(!out)return 0;*out=m->c[s];EdFace remaining=*original,inside,outside;int ok=1;
    for(int i=0;i<outline.count&&ok;i++){
        EdVec a=outline.v[i].p,b=outline.v[(i+1)%outline.count].p,edge=unit(edCross(n,edSub(b,a)));double d=edDot(edge,a);
        ok=clip(&remaining,edge,d,&inside)&&clip(&remaining,edScale(edge,-1),-d,&outside)&&inside.count&&addface(out,outside);remaining=inside;
    }
    if(out->count==m->c[s].count)ok=0; /* Covering the whole face makes no cut. */
    if(ok){out->f[f]=remaining;EdCell previous=m->c[s];m->c[s]=*out;edMapWorld(m);if(!edValidate(m)){m->c[s]=previous;ok=0;}}
    edFree(out);return ok;
}
static void remapCogSurface(EdCogParam *p,const EdFace *source,const EdModel *out){
    EdVec n=edNormal(source),center=edV(0,0,0);for(int v=0;v<source->count;v++)center=edAdd(center,source->v[v].p);center=edScale(center,1.0/source->count);
    int cell=-1,face=-1;double best=1e30;for(int s=0;s<out->count;s++)for(int f=0;f<out->c[s].count;f++){const EdFace *target=&out->c[s].f[f];if(edDot(n,edNormal(target))<.99999||fabs(edDot(n,edSub(target->v[0].p,source->v[0].p)))>ED_EPS*4)continue;
        EdFace intersection=*target,tmp;for(int e=0;e<source->count&&intersection.count;e++){EdVec edge=unit(edCross(n,edSub(source->v[(e+1)%source->count].p,source->v[e].p)));if(!clip(&intersection,edge,edDot(edge,source->v[e].p),&tmp)){intersection.count=0;break;}intersection=tmp;}if(intersection.count<3)continue;
        EdVec c=edV(0,0,0);for(int v=0;v<target->count;v++)c=edAdd(c,target->v[v].p);double distance=length(edSub(edScale(c,1.0/target->count),center));if(distance<best){best=distance;cell=s;face=f;}}
    p->cell=cell;p->face=face;
}
int edCleaveSectorsPlane(EdModel *m,const EdBuffer<unsigned char>& selected,EdVec normal,EdVec point){
    for(int s=0;s<m->count;s++)if(selected[s]&&m->c[s].splineId)return 0;
    if(!isfinite(normal.x)||!isfinite(normal.y)||!isfinite(normal.z)||!isfinite(point.x)||!isfinite(point.y)||!isfinite(point.z)||length(normal)<ED_EPS)return 0;
    int count=0;for(int s=0;s<m->count;s++)count+=selected[s]!=0;if(!count||count>ED_CELLS-m->count)return 0;
    EdModel *out=edAlloc(sizeof(*out));EdCell *halves=edAlloc(2*sizeof(*halves));if(!out||!halves){edFree(out);edFree(halves);return 0;}edCopyModel(out,m);
    normal=unit(normal);double d=edDot(normal,point);int ok=1;
    for(int s=0;s<m->count&&ok;s++)if(selected[s]){
        ok=cellclip(&m->c[s],normal,d,&halves[0])&&cellclip(&m->c[s],edScale(normal,-1),-d,&halves[1])&&halves[0].count&&halves[1].count;
        if(ok){out->c[s]=halves[0];out->c[out->count++]=halves[1];}
    }
    if(ok)for(int i=0;i<out->cogCount;i++)for(int j=0;j<out->cogs[i].count;j++){
        EdCogParam *p=&out->cogs[i].params[j];if(p->type==ED_COG_SURFACE&&p->cell>=0&&p->cell<m->count&&p->face>=0&&p->face<m->c[p->cell].count)remapCogSurface(p,&m->c[p->cell].f[p->face],out);
    }
    if(ok)ok=edValidate(out)&&edResolve(out);if(ok)edCopyModel(m,out);edFree(out);edFree(halves);return ok;
}
int edCleaveSector(EdModel *m,int s,int f,EdVec a,EdVec b) {
    if(s<0||s>=m->count||f<0||f>=m->c[s].count||m->count==ED_CELLS)return 0;
    const EdFace *face=&m->c[s].f[f];a=edSnapFacePoint(face,a);b=edSnapFacePoint(face,b);
    if(length(edSub(a,b))<ED_GRID*.5)return 0;
    EdVec normal=unit(edCross(edNormal(face),edSub(b,a)));EdBuffer<unsigned char> selected;selected[s]=1;
    return edCleaveSectorsPlane(m,selected,normal,a);
}
/* Subtract older cells from newer cells. Each emitted cell is convex and disjoint. */
static int matchingPortalFaces(const EdFace *a,const EdFace *b){
    EdVec normal=edNormal(a);
    if(edDot(normal,edNormal(b))>-.99999||fabs(edDot(normal,edSub(a->v[0].p,b->v[0].p)))>ED_EPS)return 0;
    /* Collinear junctions need not be explicitly listed on both sides. */
    for(int side=0;side<2;side++){
        const EdFace *from=side?b:a,*to=side?a:b;
        for(int v=0;v<from->count;v++){
            int found=0;EdVec point=from->v[v].p;
            for(int e=0;e<to->count&&!found;e++){
                EdVec start=to->v[e].p,edge=edSub(to->v[(e+1)%to->count].p,start),offset=edSub(point,start);double size=length(edge);
                if(size>ED_EPS&&length(edCross(offset,edge))<=ED_EPS*size&&edDot(offset,edge)>=-ED_EPS*size&&edDot(offset,edge)<=edDot(edge,edge)+ED_EPS*size)found=1;
            }
            if(!found)return 0;
        }
    }return 1;
}
int edResolve(EdModel *m) {
    EdModel *out=(EdModel*)edCalloc(1,sizeof(EdModel)); EdCell *remain=(EdCell*)edAlloc(sizeof(EdCell)),*inside=(EdCell*)edAlloc(sizeof(EdCell)),*outside=(EdCell*)edAlloc(sizeof(EdCell));
    if(!out||!remain||!inside||!outside) { edFree(out); edFree(remain); edFree(inside); edFree(outside); return 0; }
    /* Preserve all authored document data, including the model/template library. */
    *out=*m;out->c.clear();out->count=0;int ok=1;
    for(int s=0;s<m->count && ok;s++) {
        /* Work list contains fragments of this input cell after each subtraction. */
        EdModel *fragments=(EdModel*)edCalloc(1,sizeof(EdModel)); if(!fragments) { ok=0; break; }
        fragments->count=1; fragments->c[0]=m->c[s];
        for(int old=0;old<s && ok;old++) {
            int count=fragments->count; fragments->count=0;
            EdCell *pieces=(EdCell*)edAlloc(sizeof(EdCell)*count); if(!pieces) { ok=0; break; }
            edCopy(pieces,fragments->c,sizeof(EdCell)*count);
            for(int p=0;p<count && ok;p++) {
                /* Merely touching cells have no overlapping volume. Keep
                   their topology intact rather than slicing them on distant
                   planes while searching for an intersection. */
                *remain=pieces[p];
                for(int plane=0;plane<m->c[old].count && remain->count;plane++) {
                    EdFace *face=&m->c[old].f[plane];EdVec n=edNormal(face);double d=edDot(n,face->v[0].p);
                    if(!cellclip(remain,n,d,inside)){ok=0;break;}*remain=*inside;
                }
                if(!ok)break;
                if(!remain->count){if(fragments->count==ED_CELLS){sectorLimitError=1;ok=0;break;}fragments->c[fragments->count++]=pieces[p];continue;}
                *remain=pieces[p];
                for(int plane=0;plane<m->c[old].count && remain->count;plane++) {
                    EdFace *face=&m->c[old].f[plane]; EdVec n=edNormal(face); double d=edDot(n,face->v[0].p);
                    if(!cellclip(remain,n,d,inside)||!cellclip(remain,edScale(n,-1),-d,outside)) { ok=0; break; }
                    if(outside->count) { if(fragments->count==ED_CELLS) {sectorLimitError=1;ok=0;break;} fragments->c[fragments->count++]=*outside; }
                    *remain=*inside;
                }
            } edFree(pieces);
        }
        for(int p=0;p<fragments->count && ok;p++) { if(out->count==ED_CELLS){sectorLimitError=1;ok=0;} else out->c[out->count++]=fragments->c[p]; }
        edFree(fragments);
    }
    /* Partition coplanar opposing polygons by one another's edges, then pair
       identical pieces. This creates partial portals without overlapping cells. */
    if(ok)for(int s=0;s<out->count;s++)for(int f=0;f<out->c[s].count;f++){
        EdFace *face=&out->c[s].f[f];
        face->neighbor=face->mirror=-1;
    }
    if(ok) for(int s=0;s<out->count && ok;s++) for(int t=s+1;t<out->count && ok;t++) {
        for(int a=0;a<out->c[s].count && ok;a++) for(int b=0;b<out->c[t].count && ok;b++) {
            EdFace fa=out->c[s].f[a],fb=out->c[t].f[b]; EdVec n=edNormal(&fa);
            if(edDot(n,edNormal(&fb))>-.99999||fabs(edDot(n,edSub(fb.v[0].p,fa.v[0].p)))>ED_EPS) continue;
            int equal=matchingPortalFaces(&fa,&fb);
            if(equal){out->c[s].f[a].neighbor=t;out->c[s].f[a].mirror=b;out->c[t].f[b].neighbor=s;out->c[t].f[b].mirror=a;continue;}
            EdFace intersection=fa,tmp;
            EdVec bn=edNormal(&fb);
            for(int e=0;e<fb.count && intersection.count;e++) { EdVec edge=edSub(fb.v[(e+1)%fb.count].p,fb.v[e].p);if(length(edge)<ED_EPS)continue;EdVec en=unit(edCross(bn,edge)); if(!clip(&intersection,en,edDot(en,fb.v[e].p),&tmp)) { ok=0; break; } intersection=tmp; }
            if(!intersection.count) continue;
            /* JKL decimal round-off can turn edge contact into a microscopic
               triangle. It is not a portal and must not repeatedly split faces. */
            EdVec areaVector=edV(0,0,0);double perimeter=0;
            for(int v=0;v<intersection.count;v++){EdVec a=intersection.v[v].p,b=intersection.v[(v+1)%intersection.count].p;perimeter+=length(edSub(b,a));areaVector=edAdd(areaVector,edCross(edSub(a,intersection.v[0].p),edSub(b,intersection.v[0].p)));}
            if(length(areaVector)<=ED_EPS*perimeter)continue;
            /* Split both polygons into the shared region and convex leftovers. */
            for(int side=0;side<2 && ok;side++) {
                EdCell *cell=side?&out->c[t]:&out->c[s]; int index=side?b:a;
                EdFace remaining=side?fb:fa, cutter=side?fa:fb; EdVec cn=edNormal(&cutter);
                for(int e=0;e<cutter.count && remaining.count;e++) {
                    EdVec edge=edSub(cutter.v[(e+1)%cutter.count].p,cutter.v[e].p);if(length(edge)<ED_EPS)continue;EdVec en=unit(edCross(cn,edge)); double d=edDot(en,cutter.v[e].p); EdFace part;
                    if(!clip(&remaining,edScale(en,-1),-d,&part)||!clip(&remaining,en,d,&tmp)) { ok=0; break; }
                    if(part.count && !addface(cell,part)) { ok=0; break; } remaining=tmp;
                }
                cell->f[index]=remaining;
                cell->f[index].neighbor=side?s:t; cell->f[index].mirror=side?a:b;
            }
        }
    }
    /* Later partitions can replace faces paired earlier in the loop. Rebuild
       reciprocal indices from the final boundaries, never the old indices. */
    if(ok){
        for(int s=0;s<out->count;s++)for(int f=0;f<out->c[s].count;f++)out->c[s].f[f].neighbor=out->c[s].f[f].mirror=-1;
        for(int s=0;s<out->count;s++)for(int t=s+1;t<out->count;t++)for(int a=0;a<out->c[s].count;a++)if(out->c[s].f[a].neighbor<0)
            for(int b=0;b<out->c[t].count;b++)if(out->c[t].f[b].neighbor<0&&matchingPortalFaces(&out->c[s].f[a],&out->c[t].f[b])){
                out->c[s].f[a].neighbor=t;out->c[s].f[a].mirror=b;out->c[t].f[b].neighbor=s;out->c[t].f[b].mirror=a;break;
            }
    }
    if(ok)for(int i=0;i<out->cogCount;i++)for(int j=0;j<out->cogs[i].count;j++){
        EdCogParam *p=&out->cogs[i].params[j];if(p->cell<0||p->cell>=m->count)continue;
        if(p->type==ED_COG_SECTOR){EdVec center=edV(0,0,0);int count=0;const EdCell *cell=&m->c[p->cell];for(int f=0;f<cell->count;f++)for(int v=0;v<cell->f[f].count;v++){center=edAdd(center,cell->f[f].v[v].p);count++;}center=edScale(center,1.0/count);p->cell=-1;for(int s=0;s<out->count;s++)if(edContains(&out->c[s],center)){p->cell=s;break;}}
        else if(p->type==ED_COG_SURFACE&&p->face>=0&&p->face<m->c[p->cell].count)remapCogSurface(p,&m->c[p->cell].f[p->face],out);
    }
    if(ok && edValidate(out)) {edMapWorld(out);edCopyModel(m,out);} else ok=0;
    edFree(out); edFree(remain); edFree(inside); edFree(outside); return ok;
}
static int dissolvePointOnFace(const EdFace *face,EdVec point){
    EdVec normal=edNormal(face);if(fabs(edDot(normal,edSub(point,face->v[0].p)))>ED_EPS)return 0;
    for(int v=0;v<face->count;v++)if(edDot(edCross(edSub(face->v[(v+1)%face->count].p,face->v[v].p),edSub(point,face->v[v].p)),normal)<-ED_EPS)return 0;
    return 1;
}
static int dissolveSharedEdge(const EdFace *face,EdVec a,EdVec b){
    EdVec direction=edSub(b,a);double size=length(direction),den=edDot(direction,direction);if(size<ED_EPS)return 0;
    for(int v=0;v<face->count;v++){
        EdVec p=face->v[v].p,q=face->v[(v+1)%face->count].p;
        if(length(edCross(edSub(p,a),direction))>ED_EPS*size||length(edCross(edSub(q,a),direction))>ED_EPS*size)continue;
        double x=edDot(edSub(p,a),direction)/den,y=edDot(edSub(q,a),direction)/den;
        if(fmin(1,fmax(x,y))-fmax(0,fmin(x,y))>ED_EPS/size)return 1;
    }return 0;
}
static double dissolveArea(const EdFace *face){
    EdVec sum=edV(0,0,0);for(int v=1;v+1<face->count;v++)sum=edAdd(sum,edCross(edSub(face->v[v].p,face->v[0].p),edSub(face->v[v+1].p,face->v[0].p)));
    return length(sum)*.5;
}
/* A convex hull is a union only when its area equals the patches' total area.
   This prevents filling concave notches, holes, or gaps between patches. */
static int dissolveFaces(const EdCell *cell,const EdBuffer<unsigned char>& group,int seed,EdFace *face){
    EdScratch<EdVertex> points;int count=0;double area=0;*face=cell->f[seed];
    for(int f=0;f<cell->count;f++)if(group[f]){area+=dissolveArea(&cell->f[f]);for(int v=0;v<cell->f[f].count;v++)points[count++]=cell->f[f].v[v];}
    if(!capHull(face,points,count,edNormal(&cell->f[seed]))||face->count<3)return 0;
    return fabs(dissolveArea(face)-area)<=ED_EPS*fmax(1,area);
}
int edDeleteHandle(EdModel *m,EdHit hit){
    if(hit.cell>=0&&hit.cell<m->count&&m->c[hit.cell].splineId)return 0;
    if(!edValidate(m))return 0;
    for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++){
        const EdFace *face=&m->c[s].f[f];if(face->neighbor<0)continue;
        int t=face->neighbor,g=face->mirror;if(t>=m->count||g<0||g>=m->c[t].count||m->c[t].f[g].neighbor!=s||m->c[t].f[g].mirror!=f)return 0;
    }
    if(hit.cell<0||hit.cell>=m->count||hit.face<0||hit.face>=m->c[hit.cell].count)return 0;
    const EdFace *selected=&m->c[hit.cell].f[hit.face];int vertex=hit.vertex>=0,index=vertex?hit.vertex:hit.edge;
    if(index<0||index>=selected->count)return 0;
    EdVec a=selected->v[index].p,b=selected->v[(index+1)%selected->count].p,normal=edNormal(selected);
    EdBuffer<unsigned char> scope;scope[hit.cell]=1;
    if(vertex){
        /* Shared portal vertices must disappear on both sides together. */
        int changed;do{changed=0;for(int s=0;s<m->count;s++)if(scope[s])for(int f=0;f<m->c[s].count;f++){
            const EdFace *face=&m->c[s].f[f];if(face->neighbor>=0&&!scope[face->neighbor]&&dissolvePointOnFace(face,a)){scope[face->neighbor]=1;changed=1;}
        }}while(changed);
    }else if(selected->neighbor>=0)scope[selected->neighbor]=1;
    EdModel *out=edAlloc(sizeof(*out));if(!out)return 0;edCopyModel(out,m);
    EdBuffer<EdBuffer<int>> maps;int ok=1,merged=0;
    for(int s=0;s<m->count&&ok;s++){
        const EdCell *cell=&m->c[s];EdCell *result=&out->c[s];EdBuffer<unsigned char> done;result->count=0;
        for(int f=0;f<cell->count;f++){
            if(done[f])continue;
            const EdFace *source=&cell->f[f];EdVec n=edNormal(source);EdBuffer<unsigned char> group;group[f]=1;int count=1;
            int eligible=scope[s]&&(vertex?dissolvePointOnFace(source,a):fabs(edDot(normal,n))>.99999&&fabs(edDot(normal,edSub(source->v[0].p,a)))<ED_EPS&&dissolveSharedEdge(source,a,b));
            if(eligible)for(int g=0;g<cell->count;g++)if(g!=f&&!done[g]){
                const EdFace *other=&cell->f[g];
                if(other->neighbor==source->neighbor&&edDot(n,edNormal(other))>.99999&&fabs(edDot(n,edSub(other->v[0].p,source->v[0].p)))<ED_EPS&&
                   (vertex?dissolvePointOnFace(other,a):dissolveSharedEdge(other,a,b))){group[g]=1;count++;}
            }
            EdFace face=*source;
            if(eligible&&(vertex||count>1)){
                /* Preserve face order; the highlighted patch supplies the
                   merged material/settings even when it occurs later. */
                int seed=s==hit.cell&&group[hit.face]?hit.face:f;
                if(!dissolveFaces(cell,group,seed,&face)){ok=0;break;}
                if(vertex){for(int v=0;v<face.count;v++)if(same(face.v[v].p,a))ok=0;}
                else if(count>1)merged=1;
                edMapFace(&face);
            }
            for(int g=0;g<cell->count;g++)if(group[g]){done[g]=1;maps[s][g]=result->count;}
            result->f[result->count++]=face;
        }
    }
    if(!vertex&&!merged)ok=0;
    /* Remap both directions of every portal; groups may not combine patches
       whose mirrors would still be separate surfaces in the neighbor. */
    if(ok)for(int s=0;s<m->count&&ok;s++)for(int f=0;f<m->c[s].count;f++){
        const EdFace *old=&m->c[s].f[f];EdFace *face=&out->c[s].f[maps[s][f]];
        if(old->neighbor>=0){int t=old->neighbor,g=old->mirror;
            if(t>=m->count||g<0||g>=m->c[t].count){ok=0;break;}
            face->mirror=maps[t][g];EdFace *mirror=&out->c[t].f[face->mirror];
            if(mirror->neighbor!=s){ok=0;break;}
            for(int h=0;h<m->c[s].count;h++)if(maps[s][h]==maps[s][f]&&m->c[s].f[h].neighbor>=0&&maps[t][m->c[s].f[h].mirror]!=face->mirror)ok=0;
        }
    }
    if(ok)for(int i=0;i<out->cogCount;i++)for(int j=0;j<out->cogs[i].count;j++){
        EdCogParam *p=&out->cogs[i].params[j];if(p->type==ED_COG_SURFACE&&p->cell>=0&&p->cell<m->count&&p->face>=0&&p->face<m->c[p->cell].count)p->face=maps[p->cell][p->face];
    }
    if(ok)ok=edValidate(out);
    if(ok)edCopyModel(m,out);edFree(out);return ok;
}
/* Cleanup preserves texture mapping, flags and rendering settings as well as
   the material. A portal can only merge if its reciprocal patches can too. */
static int cleanupCompatible(const EdFace *a,const EdFace *b){
    if(a->customUV||b->customUV||a->material!=b->material||a->neighbor!=b->neighbor||a->uOffset!=b->uOffset||a->vOffset!=b->vOffset||a->uvRotation!=b->uvRotation)return 0;
    EdVec n=edNormal(a);if(edDot(n,edNormal(b))<.99999)return 0;
    for(int v=0;v<b->count;v++)if(fabs(edDot(n,edSub(b->v[v].p,a->v[0].p)))>ED_EPS)return 0;
    EdSurfaceAttributes x=edSurfaceAttributes(a),y=edSurfaceAttributes(b);
    return x.surfaceFlags==y.surfaceFlags&&x.faceFlags==y.faceFlags&&x.adjoinFlags==y.adjoinFlags&&x.geometryMode==y.geometryMode&&x.textureMode==y.textureMode&&x.lightingMode==y.lightingMode&&x.uScale==y.uScale&&x.vScale==y.vScale&&x.extraLight==y.extraLight&&x.adjoinDistance==y.adjoinDistance;
}
static int cleanupConnected(const EdFace *a,const EdFace *b){
    for(int v=0;v<a->count;v++)if(dissolveSharedEdge(b,a->v[v].p,a->v[(v+1)%a->count].p))return 1;return 0;
}
static void cleanupCollapse(EdCell *cell,const EdBuffer<unsigned char>& group,EdFace merged,EdBuffer<int>& map){
    int count=cell->count,at=0,target=-1;
    for(int f=0;f<count;f++){
        if(group[f]){if(target<0){target=at;cell->f[at++]=merged;}map[f]=target;}
        else{map[f]=at;cell->f[at++]=cell->f[f];}
    }cell->count=at;
}
static int cleanupMerge(EdModel *m,int sector,const EdBuffer<unsigned char>& group,int seed){
    EdCell *cell=&m->c[sector];EdFace merged;int savedError=vertexLimitError;
    int ok=dissolveFaces(cell,group,seed,&merged)&&merged.count<=ED_SURFACE_VERTS;
    vertexLimitError=savedError;if(!ok)return 0;
    int removed=-merged.count;for(int f=0;f<cell->count;f++)if(group[f])removed+=cell->f[f].count;
    if(!removed)return 0;
    int neighbor=merged.neighbor,mirrorSeed=-1;EdBuffer<unsigned char> mirrors;EdFace mirror;
    if(neighbor>=0){
        if(neighbor>=m->count||neighbor==sector)return 0;EdCell *other=&m->c[neighbor];
        for(int f=0;f<cell->count;f++)if(group[f]){
            int g=cell->f[f].mirror;if(g<0||g>=other->count||other->f[g].neighbor!=sector||other->f[g].mirror!=f)return 0;
            if(mirrorSeed<0)mirrorSeed=g;else if(!cleanupCompatible(&other->f[mirrorSeed],&other->f[g]))return 0;mirrors[g]=1;
        }
        ok=dissolveFaces(other,mirrors,mirrorSeed,&mirror)&&mirror.count==merged.count;vertexLimitError=savedError;if(!ok)return 0;
        for(int v=0;v<merged.count;v++){int found=0;for(int w=0;w<mirror.count;w++)if(same(merged.v[v].p,mirror.v[w].p))found=1;if(!found)return 0;}
    }
    int oldCount=cell->count,otherCount=neighbor>=0?m->c[neighbor].count:0;
    EdBuffer<int> map,mirrorMap;EdCell *replacement=edAlloc(2*sizeof(*replacement));if(!replacement)return 0;
    replacement[0]=*cell;cleanupCollapse(&replacement[0],group,merged,map);
    if(neighbor>=0){replacement[1]=m->c[neighbor];cleanupCollapse(&replacement[1],mirrors,mirror,mirrorMap);}
    /* Area equality alone can accept a sliver across a concave boundary.
       Check coverage before committing each merge, not just at the end. */
    if(!closedCell(&replacement[0])||(neighbor>=0&&!closedCell(&replacement[1]))){edFree(replacement);return 0;}
    *cell=replacement[0];if(neighbor>=0)m->c[neighbor]=replacement[1];edFree(replacement);
    for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++){
        EdFace *face=&m->c[s].f[f];if(face->neighbor==sector)face->mirror=map[face->mirror];
        else if(neighbor>=0&&face->neighbor==neighbor)face->mirror=mirrorMap[face->mirror];
    }
    for(int i=0;i<m->cogCount;i++)for(int j=0;j<m->cogs[i].count;j++){
        EdCogParam *p=&m->cogs[i].params[j];if(p->type!=ED_COG_SURFACE||p->face<0)continue;
        if(p->cell==sector&&p->face<oldCount)p->face=map[p->face];else if(neighbor>=0&&p->cell==neighbor&&p->face<otherCount)p->face=mirrorMap[p->face];
    }
    return 1;
}
/* Rebuild a concave solid patch around preserved openings/material islands.
   This discards its internal mesh before convex partitioning, avoiding the
   local minima of pairwise merging a dense fan around a doorway/notch. */
static int cleanupRepartition(EdModel *m,int sector,const EdBuffer<unsigned char>& group,int seed){
    EdCell *cell=&m->c[sector];if(cell->f[seed].neighbor>=0)return 0;
    EdScratch<EdVertex> points;int count=0,oldFaces=0,oldVertices=0,savedError=vertexLimitError;double oldArea=0;
    for(int f=0;f<cell->count;f++)if(group[f]){oldFaces++;oldVertices+=cell->f[f].count;oldArea+=dissolveArea(&cell->f[f]);for(int v=0;v<cell->f[f].count;v++)points[count++]=cell->f[f].v[v];}
    if(oldFaces<2)return 0;EdFace plane=cell->f[seed];EdVec normal=edNormal(&plane);
    int ok=capHull(&plane,points,count,normal)&&plane.count>=3;vertexLimitError=savedError;if(!ok)return 0;
    EdCell *storage=edCalloc(3,sizeof(*storage));if(!storage)return 0;EdCell *pieces=&storage[0],*next=&storage[1],*result=&storage[2];pieces->count=1;pieces->f[0]=plane;
    for(int f=0;ok&&f<cell->count;f++)if(!group[f]){
        const EdFace *cutter=&cell->f[f];if(edDot(normal,edNormal(cutter))<.99999)continue;
        int planar=1;for(int v=0;v<cutter->count;v++)if(fabs(edDot(normal,edSub(cutter->v[v].p,plane.v[0].p)))>ED_EPS)planar=0;if(!planar)continue;
        next->count=0;
        for(int p=0;ok&&p<pieces->count;p++){
            EdFace intersection=pieces->f[p],tmp;
            for(int e=0;ok&&e<cutter->count&&intersection.count;e++){EdVec edge=edSub(cutter->v[(e+1)%cutter->count].p,cutter->v[e].p);if(length(edge)<ED_EPS)continue;EdVec n=unit(edCross(normal,edge));ok=clip(&intersection,n,edDot(n,cutter->v[e].p),&tmp);intersection=tmp;}
            if(!ok)break;if(!intersection.count){ok=addface(next,pieces->f[p]);continue;}
            EdFace remaining=pieces->f[p],outside;
            for(int e=0;ok&&e<cutter->count&&remaining.count;e++){EdVec edge=edSub(cutter->v[(e+1)%cutter->count].p,cutter->v[e].p);if(length(edge)<ED_EPS)continue;EdVec n=unit(edCross(normal,edge));double d=edDot(n,cutter->v[e].p);ok=clip(&remaining,edScale(n,-1),-d,&outside)&&clip(&remaining,n,d,&tmp)&&addface(next,outside);remaining=tmp;}
        }EdCell *swap=pieces;pieces=next;next=swap;
    }
    double area=0;int vertices=0;
    for(int f=0;ok&&f<pieces->count;f++){
        EdFace *p=&pieces->f[f];EdVertex boundary[ED_VERTS];edCopy(boundary,p->v,p->count*sizeof(*boundary));ok=capHull(p,boundary,p->count,normal)&&p->count>=3&&p->count<=ED_SURFACE_VERTS;area+=dissolveArea(p);vertices+=p->count;
    }
    vertexLimitError=savedError;
    if(!ok||!pieces->count||pieces->count>oldFaces||(pieces->count==oldFaces&&vertices>=oldVertices)||fabs(area-oldArea)>ED_EPS*ED_EPS*fmax(1,oldArea)*16){edFree(storage);return 0;}
    *result=*cell;result->count=0;EdBuffer<int> map;int inserted=0;
    for(int f=0;ok&&f<cell->count;f++){
        map[f]=result->count;if(group[f]){if(!inserted){for(int p=0;ok&&p<pieces->count;p++)ok=addface(result,pieces->f[p]);inserted=1;}}
        else if(result->count==ED_FACES)ok=0;else result->f[result->count++]=cell->f[f];
    }
    if(!ok||!closedCell(result)){edFree(storage);return 0;}
    /* Non-group faces (including portals) keep their geometry and attributes.
       Remap references to the old fragments onto overlapping new pieces. */
    EdCell *previous=edAlloc(sizeof(*previous));if(!previous){edFree(storage);return 0;}*previous=*cell;*cell=*result;
    for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++){EdFace *p=&m->c[s].f[f];if(p->neighbor==sector)p->mirror=map[p->mirror];}
    for(int i=0;i<m->cogCount;i++)for(int j=0;j<m->cogs[i].count;j++){EdCogParam *p=&m->cogs[i].params[j];if(p->type==ED_COG_SURFACE&&p->cell==sector&&p->face>=0&&p->face<previous->count){int f=p->face;if(group[f])remapCogSurface(p,&previous->f[f],m);else p->face=map[f];}}
    edFree(previous);edFree(storage);return 1;
}
int edCleanupSector(EdModel *m,int sector){
    if(sector>=0&&sector<m->count&&m->c[sector].splineId)return 0;
    if(sector<0||sector>=m->count||!edValidate(m))return 0;
    /* Check reciprocal indices before using them to remap portals. */
    for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++){const EdFace *p=&m->c[s].f[f];if(p->neighbor>=0&&(p->neighbor>=m->count||p->mirror<0||p->mirror>=m->c[p->neighbor].count||m->c[p->neighbor].f[p->mirror].neighbor!=s||m->c[p->neighbor].f[p->mirror].mirror!=f))return 0;}
    EdModel *out=edAlloc(sizeof(*out));if(!out)return 0;edCopyModel(out,m);int changed=0,again;
    do{again=0;EdCell *cell=&out->c[sector];
        for(int seed=0;seed<cell->count&&!again;seed++){
            EdBuffer<unsigned char> group;group[seed]=1;int added;
            do{added=0;for(int f=0;f<cell->count;f++)if(!group[f]&&cleanupCompatible(&cell->f[seed],&cell->f[f])){
                for(int g=0;g<cell->count;g++)if(group[g]&&cleanupConnected(&cell->f[g],&cell->f[f])){group[f]=1;added=1;break;}
            }}while(added);
            if(cleanupMerge(out,sector,group,seed)){again=changed=1;break;}
            if(cleanupRepartition(out,sector,group,seed)){again=changed=1;break;}
            /* A concave component may still contain convex subsets. */
            for(int f=seed+1;f<cell->count&&!again;f++)if(group[f]&&cleanupConnected(&cell->f[seed],&cell->f[f])){
                EdBuffer<unsigned char> pair;pair[seed]=pair[f]=1;if(cleanupMerge(out,sector,pair,seed))again=changed=1;
            }
            if(!again){EdBuffer<unsigned char> single;single[seed]=1;if(cleanupMerge(out,sector,single,seed))again=changed=1;}
        }
    }while(again);
    int ok=changed&&edValidate(out);if(ok){edMapWorld(out);edCopyModel(m,out);}edFree(out);return ok;
}
/* A merge removes only internal adjoins. Validation of the retained boundary
   rejects disconnected, concave, open or oversized results without a hull fill. */
int edMergeSectors(EdModel *m,const EdBuffer<unsigned char>& selected,int *merged){
    for(int s=0;s<m->count;s++)if(selected[s]&&m->c[s].splineId)return 0;
    int first=-1,count=0;for(int s=0;s<m->count;s++)if(selected[s]){if(first<0)first=s;count++;}
    if(count<2||!edValidate(m))return 0;
    EdModel out;edCopyModel(&out,m);out.c.clear();out.count=0;
    EdBuffer<int> cells;EdBuffer<EdBuffer<int>> faces;
    for(int s=0;s<m->count;s++)if(!selected[s]||s==first){cells[s]=out.count;out.c[out.count++]=m->c[s];}
    int target=cells[first];out.c[target].count=0;out.c[target].f.clear();
    for(int s=0;s<m->count;s++)if(selected[s])cells[s]=target;
    for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++){
        const EdFace &face=m->c[s].f[f];
        if(selected[s]){
            if(face.neighbor>=0&&face.neighbor<m->count&&selected[face.neighbor]){faces[s][f]=-1;continue;}
            faces[s][f]=out.c[target].count;out.c[target].f[out.c[target].count++]=face;
        }else faces[s][f]=f;
    }
    for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++)if(faces[s][f]>=0){
        EdFace &face=out.c[cells[s]].f[faces[s][f]];int neighbor=face.neighbor,mirror=face.mirror;
        if(neighbor>=0){if(neighbor>=m->count||mirror<0||mirror>=m->c[neighbor].count||faces[neighbor][mirror]<0)return 0;face.neighbor=cells[neighbor];face.mirror=faces[neighbor][mirror];}
    }
    for(int i=0;i<out.cogCount;i++)for(int j=0;j<out.cogs[i].count;j++){
        EdCogParam &p=out.cogs[i].params[j];if(p.cell<0||p.cell>=m->count)continue;
        if(p.type==ED_COG_SURFACE){if(p.face<0||p.face>=m->c[p.cell].count)return 0;p.face=faces[p.cell][p.face];p.cell=p.face<0?-1:cells[p.cell];}
        else if(p.type==ED_COG_SECTOR)p.cell=cells[p.cell];
    }
    if(!edValidate(&out))return 0;
    edCleanupSector(&out,target);
    if(!edValidate(&out))return 0;
    edCopyModel(m,&out);if(merged)*merged=target;return 1;
}
int edDeleteSector(EdModel *m,int sector) {
    if(m->count<=1||sector<0||sector>=m->count)return 0;
    for(int i=0;i<m->cogCount;i++)for(int j=0;j<m->cogs[i].count;j++){EdCogParam *p=&m->cogs[i].params[j];if(p->type==ED_COG_SECTOR||p->type==ED_COG_SURFACE){if(p->cell==sector)p->cell=p->face=-1;else if(p->cell>sector)p->cell--;}}
    for(int i=sector;i+1<m->count;i++)m->c[i]=std::move(m->c[i+1]);
    edZero(&m->c[--m->count],0,sizeof(EdCell));
    /* Rebuild portals so former neighbors become solid walls. */
    for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++)m->c[s].f[f].neighbor=m->c[s].f[f].mirror=-1;
    if(!edResolve(m))return 0;
    EdVec center=edV(0,0,0);int vertices=0;
    for(int f=0;f<m->c[0].count;f++)for(int v=0;v<m->c[0].f[f].count;v++){center=edAdd(center,m->c[0].f[f].v[v].p);vertices++;}
    center=edScale(center,1.0/vertices);
    for(int i=0;i<m->spawnCount;i++){
        int inside=0;for(int s=0;s<m->count;s++)if(edContains(&m->c[s],m->spawns[i].position)){inside=1;break;}
        if(!inside)m->spawns[i].position=center;
    }
    if(m->spawnCount)m->spawn=m->spawns[0].position;
    else {int inside=0;for(int s=0;s<m->count;s++)if(edContains(&m->c[s],m->spawn))inside=1;if(!inside)m->spawn=center;}
    for(int i=0;i<m->objectCount;i++){int inside=0;for(int s=0;s<m->count;s++)if(edContains(&m->c[s],m->objects[i].position))inside=1;if(!inside)m->objects[i].position=center;}
    return 1;
}
int edStairsDimensions(const EdModel *m,EdHit top,EdHit bottom,double *rise,double *run) {
    if(!m||top.cell<0||top.cell>=m->count||bottom.cell!=top.cell||top.face<0||top.face>=6||bottom.face<0||bottom.face>=6||top.face==bottom.face||top.vertex>=0||top.edge>=0||bottom.vertex>=0||bottom.edge>=0)return 0;
    const EdCell *cell=&m->c[top.cell];if(cell->count!=6)return 0;
    EdVec axis=edNormal(&cell->f[bottom.face]),other=edNormal(&cell->f[top.face]);
    if(fabs(axis.z)>ED_EPS||edDot(axis,other)>-.99999)return 0;
    int floor=-1,ceiling=-1;
    for(int f=0;f<6;f++){
        EdVec n=edNormal(&cell->f[f]);if(cell->f[f].count<3||cell->f[f].count>ED_VERTS)return 0;
        if(n.z>.99999)floor=f;else if(n.z<-.99999)ceiling=f;else if(fabs(n.z)>ED_EPS)return 0;
    }
    if(floor<0||ceiling<0)return 0;
    double height=cell->f[ceiling].v[0].p.z-cell->f[floor].v[0].p.z;
    double distance=edDot(axis,edSub(cell->f[top.face].v[0].p,cell->f[bottom.face].v[0].p));
    if(height<ED_GRID-ED_EPS||distance<ED_GRID-ED_EPS)return 0;
    if(rise)*rise=height;if(run)*run=distance;return 1;
}
int edCreateStairs(EdModel *m,EdHit top,EdHit bottom,int steps) {
    if(top.cell>=0&&top.cell<m->count&&m->c[top.cell].splineId)return 0;
    double rise,run;if(!edStairsDimensions(m,top,bottom,&rise,&run)||steps<1||steps>ED_FACES-4||steps>ED_CELLS-m->count)return 0;
    EdModel *out=edAlloc(sizeof(*out));EdCell *work=edCalloc(5,sizeof(*work));if(!out||!work){edFree(out);edFree(work);return 0;}
    edCopyModel(out,m);EdCell *source=&work[0],*below=&work[1],*a=&work[2],*b=&work[3],*wedge=&work[4];*source=m->c[top.cell];
    EdVec axis=edNormal(&source->f[bottom.face]);double low=edDot(axis,source->f[bottom.face].v[0].p),floorZ=0;
    /* Caps inherit the source floor's material and UV settings. */
    for(int f=0;f<6;f++)if(edNormal(&source->f[f]).z>.99999){floorZ=source->f[f].v[0].p.z;EdFace swap=source->f[0];source->f[0]=source->f[f];source->f[f]=swap;break;}
    EdVec ramp=unit(edSub(edV(0,0,1),edScale(axis,rise/run)));double rampD=(floorZ-rise/run*low)/sqrt(1+rise*rise/(run*run));
    int ok=cellclip(source,ramp,rampD,&out->c[top.cell])&&out->c[top.cell].count&&cellclip(source,edScale(ramp,-1),-rampD,below)&&below->count;
    /* Retain only the air wedges between each tread and the diagonal ramp.
     * The discarded portions become solid space below the visible stairs. */
    for(int i=0;ok&&i<steps;i++){
        ok=cellclip(below,axis,low+run*i/steps,a)&&cellclip(a,edScale(axis,-1),-(low+run*(i+1)/steps),b)&&cellclip(b,edV(0,0,1),floorZ+rise*i/steps,wedge)&&wedge->count;
        if(ok)out->c[out->count++]=*wedge;
    }
    if(ok){
        /* Preserve attributes before rebuilding all reciprocal connections. */
        for(int s=0;s<out->count;s++)for(int f=0;f<out->c[s].count;f++){EdFace *face=&out->c[s].f[f];face->attributes=edSurfaceAttributes(face);face->hasAttributes=1;face->neighbor=face->mirror=-1;}
        ok=edValidate(out)&&edResolve(out);
    }
    int rampFaces=0;
    if(ok)for(int s=0;s<out->count;s++)for(int f=0;f<out->c[s].count;f++){
        EdFace *face=&out->c[s].f[f];if(face->neighbor<0)continue;
        if(fabs(edDot(edNormal(face),ramp))<.99999||fabs(edDot(ramp,face->v[0].p)-rampD)>ED_EPS*4)continue;
        /* Touch only the ramp connecting this sector's retained air wedges. */
        if(s!=top.cell&&face->neighbor!=top.cell)continue;
        face->attributes=edSurfaceAttributes(face);face->hasAttributes=1;face->attributes.surfaceFlags|=5u;face->attributes.geometryMode=0;
        face->attributes.adjoinFlags=(face->attributes.adjoinFlags|5u)&~2u;rampFaces++;
    }
    if(ok)ok=rampFaces==steps*2&&edValidate(out);
    if(ok){
        /* Move records stranded in the discarded solid volume into the main
         * air sector, preserving positions that still belong to the level. */
        EdVec center=edV(0,0,0);int count=0;EdCell *air=&out->c[top.cell];
        for(int f=0;f<air->count;f++)for(int v=0;v<air->f[f].count;v++){center=edAdd(center,air->f[f].v[v].p);count++;}center=edScale(center,1.0/count);
        for(int i=0;i<out->spawnCount+out->objectCount;i++){
            EdVec *p=i<out->spawnCount?&out->spawns[i].position:&out->objects[i-out->spawnCount].position;int inside=0;
            for(int s=0;s<out->count;s++)if(edContains(&out->c[s],*p)){inside=1;break;}if(!inside&&edContains(source,*p))*p=center;
        }
        if(out->spawnCount)out->spawn=out->spawns[0].position;
        for(int i=0;i<out->cogCount;i++)for(int j=0;j<out->cogs[i].count;j++){const EdCogParam *source=&m->cogs[i].params[j];if(source->type==ED_COG_SURFACE&&source->cell>=0&&source->cell<m->count&&source->face>=0&&source->face<m->c[source->cell].count)remapCogSurface(&out->cogs[i].params[j],&m->c[source->cell].f[source->face],out);}
        edMapWorld(out);edCopyModel(m,out);
    }
    edFree(work);edFree(out);return ok;
}
int edExtrusionCap(const EdFace *face,int horizontal,EdFace *cap) {
    EdVec normal=edNormal(face),axis=edScale(normal,-1);*cap=*face;
    if(!horizontal){for(int i=0;i<cap->count;i++)cap->v[i].p=edAdd(cap->v[i].p,edScale(axis,ED_GRID));return 1;}
    axis.z=0;if(length(axis)<ED_EPS)return 0;axis=unit(axis);
    /* Keep each vertex's height and width, extending it horizontally onto a
       common vertical plane one grid unit beyond the outermost source point. */
    double plane=-1e30;for(int i=0;i<face->count;i++)plane=fmax(plane,edDot(axis,face->v[i].p));plane+=ED_GRID;
    for(int i=0;i<cap->count;i++)cap->v[i].p=edAdd(face->v[i].p,edScale(axis,plane-edDot(axis,face->v[i].p)));
    return length(edNormal(cap))>.5;
}
static int appendExtrusionCap(EdModel *m,int s,int f,const EdFace *farFace) {
    if(s>=0&&s<m->count&&m->c[s].splineId)return 0;
    if(s<0||s>=m->count||f<0||f>=m->c[s].count||m->count==ED_CELLS) return 0;
    EdFace base=m->c[s].f[f];if(base.neighbor>=0||farFace->count!=base.count)return 0;
    EdVec normal=edNormal(&base);
    for(int i=0;i<base.count;i++)if(edDot(edSub(farFace->v[i].p,base.v[i].p),normal)>=-ED_EPS)return 0;
    EdCell *c=&m->c[m->count++]; edZero(c,0,sizeof(*c));c->count=0;c->ambientLight=m->c[s].ambientLight;c->extraLight=m->c[s].extraLight;
    EdFace cap=base; reverse(&cap); if(!addface(c,cap)) return 0;
    if(!addface(c,*farFace)) return 0;
    for(int i=0;i<base.count;i++) { EdFace wall=base; int j=(i+1)%base.count; wall.count=4;
        wall.v[0]=base.v[i]; wall.v[1]=base.v[j]; wall.v[2]=base.v[j]; wall.v[3]=base.v[i];
        wall.v[2].p=farFace->v[j].p; wall.v[3].p=farFace->v[i].p;
        wall.v[0].u=0;wall.v[0].v=0;wall.v[1].u=128;wall.v[1].v=0;wall.v[2].u=128;wall.v[2].v=128;wall.v[3].u=0;wall.v[3].v=128;
        if(wall.hasAttributes)wall.attributes.surfaceFlags=(wall.attributes.surfaceFlags&~1u)|(edNormal(&wall).z>.5?1u:0u);
        if(!addface(c,wall)) return 0;
    }
    return 1;
}
static int appendExtrusion(EdModel *m,int s,int f,EdVec delta) {
    if(s<0||s>=m->count||f<0||f>=m->c[s].count)return 0;
    EdFace cap=m->c[s].f[f];for(int i=0;i<cap.count;i++)cap.v[i].p=edAdd(cap.v[i].p,delta);
    return appendExtrusionCap(m,s,f,&cap);
}
int edExtrude(EdModel *m,int s,int f,EdVec delta) {return appendExtrusion(m,s,f,delta)&&edValidate(m)&&edResolve(m);}
int edExtrudeStep(EdModel *m,int s,int f) {
    if(s<0||s>=m->count||f<0||f>=m->c[s].count)return 0;
    return edExtrude(m,s,f,edScale(edNormal(&m->c[s].f[f]),-ED_GRID));
}
int edExtrudeHorizontal(EdModel *m,int s,int f) {
    if(s<0||s>=m->count||f<0||f>=m->c[s].count)return 0;
    EdFace cap;if(!edExtrusionCap(&m->c[s].f[f],1,&cap))return 0;
    return appendExtrusionCap(m,s,f,&cap)&&edValidate(m)&&edResolve(m);
}
static int validHit(const EdModel *m,EdHit h) {
    return h.cell>=0&&h.cell<m->count&&h.face>=0&&h.face<m->c[h.cell].count&&h.vertex<m->c[h.cell].f[h.face].count&&h.edge<m->c[h.cell].f[h.face].count;
}
/* Face-local loops may omit a cleave endpoint that lies on their edge. Make
 * those T junctions explicit before moving a shared handle: otherwise moving
 * the edge's corner changes its implicit junction too, while the adjoining
 * face keeps its endpoint in place and leaves a gap/overlap for hull repair. */
static int connectCellEdges(EdCell *cell) {
    EdScratch<EdVec> points;int count=0;
    for(int f=0;f<cell->count;f++)for(int v=0;v<cell->f[f].count;v++){
        EdVec p=cell->f[f].v[v].p;int found=0;for(int i=0;i<count;i++)if(same(points[i],p)){found=1;break;}
        if(!found)points[count++]=p;
    }
    for(int f=0;f<cell->count;f++){
        EdFace source=cell->f[f],out=source;out.count=0;
        for(int v=0;v<source.count;v++){
            EdVertex a=source.v[v],b=source.v[(v+1)%source.count];EdVec edge=edSub(b.p,a.p);double size=length(edge),den=edDot(edge,edge);
            if(!push(&out,a))return 0;if(size<ED_EPS)continue;
            double positions[ED_VERTS];EdVec split[ED_VERTS];int splits=0;
            for(int i=0;i<count;i++){
                double t=edDot(edSub(points[i],a.p),edge)/den;
                if(t<=ED_EPS/size||t>=1-ED_EPS/size||length(edCross(edSub(points[i],a.p),edge))>ED_EPS*size)continue;
                if(splits==ED_VERTS)return 0;int j=splits++;
                while(j>0&&positions[j-1]>t){positions[j]=positions[j-1];split[j]=split[j-1];j--;}
                positions[j]=t;split[j]=points[i];
            }
            for(int i=0;i<splits;i++){EdVertex p={split[i],a.u+(b.u-a.u)*positions[i],a.v+(b.v-a.v)*positions[i],a.light+(b.light-a.light)*positions[i]};if(!push(&out,p))return 0;}
        }
        cell->f[f]=out;
    }
    return 1;
}
EdVec edSelectionDelta(const EdModel *original,const EdSelection *selection,EdVec delta) {
    if(!selection->count||!validHit(original,selection->hits[0]))return edV(0,0,0);
    EdHit hit=selection->hits[0];const EdFace *selected=&original->c[hit.cell].f[hit.face];
    int handles=0;for(int i=0;i<selection->count;i++)if(selection->hits[i].vertex>=0||selection->hits[i].edge>=0)handles=1;
    if(handles) {
        int anchor=hit.vertex>=0?hit.vertex:hit.edge>=0?hit.edge:0;
        EdVec target=edSnapPoint(edAdd(selected->v[anchor].p,delta));
        if(selection->count==1&&hit.vertex>=0){EdVec along=edVertexEdgeDelta(original,hit,delta);
            if(length(edSub(along,delta))<ED_EPS)return delta;
        }
        if(selection->count==1&&hit.vertex>=0){double nearest=ED_GRID*.55;const EdCell *cell=&original->c[hit.cell];
            for(int f=0;f<cell->count;f++)for(int v=0;v<cell->f[f].count;v++){EdVec p=cell->f[f].v[v].p;double distance=length(edSub(target,p));if(distance<nearest){target=p;nearest=distance;}}
        }
        return edSub(target,selected->v[anchor].p);
    }
    double distance=length(delta);
    return distance>ED_EPS?edScale(delta,round(distance/ED_GRID)*ED_GRID/distance):edV(0,0,0);
}
/* A surface includes implicit T junctions in adjoining face loops, not only
   the corners explicitly stored in its own polygon. Leave no boundary point
   behind when moving a ceiling/wall that has been cleaved on its other side. */
static int pointOnSurface(const EdFace *face,EdVec point) {
    EdVec n=edNormal(face);if(fabs(edDot(n,edSub(point,face->v[0].p)))>ED_EPS)return 0;
    for(int v=0;v<face->count;v++){
        EdVec a=face->v[v].p,edge=edSub(face->v[(v+1)%face->count].p,a);
        if(edDot(edCross(edge,edSub(point,a)),n)<-ED_EPS*length(edge))return 0;
    }return 1;
}
EdVec edSelectionDeltaXYGrid(const EdModel *original,const EdSelection *selection,EdVec delta,double grid) {
    if(!isfinite(grid)||grid<ED_GRID)grid=ED_GRID;
    delta.z=0;
    /* A click without movement must not alter off-grid geometry. Snap the
       vertex destination, not the drag distance, preserving group spacing. */
    if(edDot(delta,delta)<=ED_EPS*ED_EPS)return edV(0,0,0);
    for(int i=0;i<selection->count;i++) {
        EdHit hit=selection->hits[i];if(hit.vertex<0||!validHit(original,hit))continue;
        EdVec anchor=original->c[hit.cell].f[hit.face].v[hit.vertex].p;
        EdVec target=edAdd(anchor,delta);
        return edV(round(target.x/grid)*grid-anchor.x,round(target.y/grid)*grid-anchor.y,0);
    }
    return edV(round(delta.x/grid)*grid,round(delta.y/grid)*grid,0);
}
EdVec edSelectionDeltaXY(const EdModel *original,const EdSelection *selection,EdVec delta){return edSelectionDeltaXYGrid(original,selection,delta,ED_GRID);}
static int moveSelection(EdModel *m,const EdModel *original,const EdSelection *selection,EdVec delta,int xy) {
    for(int i=0;i<selection->count;i++){int s=selection->hits[i].cell;if(s>=0&&s<original->count&&original->c[s].splineId)return 0;}
    if(selection->count<1||selection->count>ED_SELECTIONS)return 0;
    if(!isfinite(delta.x)||!isfinite(delta.y)||!isfinite(delta.z))return 0;
    int handles=0;for(int i=0;i<selection->count;i++){if(!validHit(original,selection->hits[i]))return 0;if(selection->hits[i].vertex>=0||selection->hits[i].edge>=0)handles=1;}
    if(xy==1)delta=edSelectionDeltaXY(original,selection,delta);else if(!xy)delta=edSelectionDelta(original,selection,delta);edCopyModel(m,original);
    if(handles)for(int s=0;s<m->count;s++){
        int affected=0;
        for(int h=0;h<selection->count&&!affected;h++){
            EdHit hit=selection->hits[h];const EdFace *face=&original->c[hit.cell].f[hit.face];
            for(int v=0;v<face->count&&!affected;v++)if((hit.vertex<0||v==hit.vertex)&&(hit.edge<0||v==hit.edge||v==(hit.edge+1)%face->count))
                for(int f=0;f<m->c[s].count&&!affected;f++)affected=pointOnSurface(&m->c[s].f[f],face->v[v].p);
        }
        if(affected&&!connectCellEdges(&m->c[s]))return 0;
    }
    for(int s=0;s<m->count;s++) for(int f=0;f<m->c[s].count;f++) for(int v=0;v<m->c[s].f[f].count;v++) {
        EdVec p=m->c[s].f[f].v[v].p; int move=0;
        for(int h=0;h<selection->count&&!move;h++) {
            EdHit hit=selection->hits[h];const EdFace *selected=&original->c[hit.cell].f[hit.face];
            if(hit.vertex<0&&hit.edge<0&&pointOnSurface(selected,p)){move=1;continue;}
            for(int i=0;i<selected->count;i++) if((hit.vertex<0||i==hit.vertex) && (hit.edge<0||i==hit.edge||i==(hit.edge+1)%selected->count) && same(p,selected->v[i].p)) move=1;
        }
        if(move) m->c[s].f[f].v[v].p=edAdd(p,delta);
    }
    for(int s=0;s<original->count;s++)if(original->c[s].splineId&&!edCellsEqual(&original->c[s],&m->c[s]))return 0;
    /* Preserve planar face partitions during cleanup rather than discarding
       all cleave lines by rebuilding the entire sector hull. */
    if(handles)for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;) {
        EdFace *face=&m->c[s].f[f];
        for(int v=0;v<face->count&&face->count>1;) {int next=(v+1)%face->count;
            if(same(face->v[v].p,face->v[next].p)){memmove(&face->v[next],&face->v[next+1],sizeof(EdVertex)*(face->count-next-1));face->count--;v=0;}else v++;
        }
        if(face->count<3||length(edNormal(face))<.5){
            for(int i=0;i<m->cogCount;i++)for(int j=0;j<m->cogs[i].count;j++){EdCogParam *p=&m->cogs[i].params[j];if(p->type==ED_COG_SURFACE&&p->cell==s){if(p->face==f)p->cell=p->face=-1;else if(p->face>f)p->face--;}}
            for(int i=f;i+1<m->c[s].count;i++)m->c[s].f[i]=m->c[s].f[i+1];m->c[s].count--;}else f++;
    }
    if(!edValidate(m))for(int s=0;s<m->count;s++)if(edCompare(&m->c[s],&original->c[s],sizeof(EdCell))){
        EdCell *before=m->cogCount?(EdCell*)edAlloc(sizeof(*before)):NULL;if(m->cogCount&&!before)return 0;if(before)*before=m->c[s];
        if(!repairCell(&m->c[s])){edFree(before);return 0;}
        if(before)for(int i=0;i<m->cogCount;i++)for(int j=0;j<m->cogs[i].count;j++){EdCogParam *p=&m->cogs[i].params[j];if(p->type==ED_COG_SURFACE&&p->cell==s&&p->face>=0&&p->face<before->count)remapCogSurface(p,&before->f[p->face],m);}edFree(before);
    }
    return edValidate(m)&&edResolve(m);
}
int edMoveSelection(EdModel *m,const EdModel *original,const EdSelection *selection,EdVec delta){return moveSelection(m,original,selection,delta,0);}
int edScaleSelection(EdModel *out,const EdModel *source,const EdSelection *selection,const EdBuffer<unsigned char>& sectors,double factor,EdVec *pivot){
    if(!isfinite(factor)||factor<=0||factor>16)return 0;
    int sectorMode=0;for(int s=0;s<source->count;s++)if(sectors[s]){if(source->c[s].splineId)return 0;sectorMode=1;}
    if(!sectorMode){if(!selection||selection->count<1||selection->count>ED_SELECTIONS)return 0;for(int i=0;i<selection->count;i++){EdHit h=selection->hits[i];if(!validHit(source,h)||h.vertex>=0||h.edge>=0||source->c[h.cell].splineId)return 0;}}
    std::vector<EdVec> corners;
    auto add=[&](EdVec p){for(const auto &q:corners)if(same(p,q))return;corners.push_back(p);};
    if(sectorMode){for(int s=0;s<source->count;s++)if(sectors[s])for(int f=0;f<source->c[s].count;f++)for(int v=0;v<source->c[s].f[f].count;v++)add(source->c[s].f[f].v[v].p);}
    else for(int i=0;i<selection->count;i++){EdHit h=selection->hits[i];const EdFace &face=source->c[h.cell].f[h.face];for(int v=0;v<face.count;v++)add(face.v[v].p);}
    if(corners.empty())return 0;EdVec center={};for(const auto &p:corners)center=edAdd(center,p);center=edScale(center,1.0/corners.size());
    EdModel draft;edCopyModel(&draft,source);
    for(int s=0;s<source->count;s++)for(int f=0;f<source->c[s].count;f++)for(int v=0;v<source->c[s].f[f].count;v++){
        EdVec p=source->c[s].f[f].v[v].p;int selected=sectorMode&&sectors[s];
        if(!selected&&sectorMode)for(int c=0;c<source->count;c++)if(sectors[c]&&edContains(&source->c[c],p)){selected=1;break;}
        if(!selected){for(const auto &q:corners)if(same(p,q)){selected=1;break;}}
        // Include collinear junctions on the selected surfaces, too.
        if(!selected&&!sectorMode)for(int i=0;i<selection->count;i++){EdHit h=selection->hits[i];if(pointOnSurface(&source->c[h.cell].f[h.face],p)){selected=1;break;}}
        if(selected){if(source->c[s].splineId)return 0;draft.c[s].f[f].v[v].p=edAdd(center,edScale(edSub(p,center),factor));}
    }
    // Reject nonplanar/concave results rather than triangulating or repairing them.
    if(!edValidate(&draft)||!edResolve(&draft)||!edValidate(&draft))return 0;
    edCopyModel(out,&draft);if(pivot)*pivot=center;return 1;
}
int edMoveSelectionExact(EdModel *m,const EdModel *original,const EdSelection *selection,EdVec delta){return moveSelection(m,original,selection,delta,2);}
int edMoveSelectionXY(EdModel *m,const EdModel *original,const EdSelection *selection,EdVec delta){return moveSelection(m,original,selection,delta,1);}
int edMove(EdModel *m,const EdModel *original,EdHit hit,EdVec delta) {EdSelection selection={0};selection.count=1;selection.hits[0]=hit;return edMoveSelection(m,original,&selection,delta);}
static int extrudeSelection(EdModel *m,const EdSelection *selection,int horizontal) {
    if(selection->count<1||selection->count>ED_SELECTIONS)return 0;
    for(int i=0;i<selection->count;i++) {
        EdHit h=selection->hits[i];
        if(!validHit(m,h)||h.vertex>=0||h.edge>=0||m->c[h.cell].f[h.face].neighbor>=0)return 0;
    }
    /* Append all prisms before CSG changes any of the selected face indices.
       Shared boundaries become adjoins in one transaction. */
    for(int i=0;i<selection->count;i++) {
        EdHit h=selection->hits[i];EdFace cap;
        if(!edExtrusionCap(&m->c[h.cell].f[h.face],horizontal,&cap)||!appendExtrusionCap(m,h.cell,h.face,&cap))return 0;
    }
    return edValidate(m)&&edResolve(m);
}
int edExtrudeSelection(EdModel *m,const EdSelection *selection) {return extrudeSelection(m,selection,0);}
int edExtrudeSelectionHorizontal(EdModel *m,const EdSelection *selection) {return extrudeSelection(m,selection,1);}
void edCogThingChanged(EdModel *m,int index,int inserted){for(int i=0;i<m->cogCount;i++)for(int j=0;j<m->cogs[i].count;j++){EdCogParam *p=&m->cogs[i].params[j];if(p->type!=ED_COG_THING)continue;int id=atoi(p->value);if(inserted&&id>=index)id++;else if(!inserted&&id==index)id=-1;else if(!inserted&&id>index)id--;snprintf(p->value,sizeof(p->value),"%d",id);}}
int edCogDelete(EdModel *m,int index){if(index<0||index>=m->cogCount)return 0;for(int shift=index;shift+1<m->cogCount;shift++)m->cogs[shift]=std::move(m->cogs[shift+1]);edZero(&m->cogs[--m->cogCount],0,sizeof(EdPlacedCog));
    for(int i=0;i<m->cogCount;i++)for(int j=0;j<m->cogs[i].count;j++){EdCogParam *p=&m->cogs[i].params[j];if(p->type==ED_COG_COG){int id=atoi(p->value);if(id==index)id=-1;else if(id>index)id--;snprintf(p->value,sizeof(p->value),"%d",id);}}return 1;}
static int equalHit(const EdModel *m,EdHit a,EdHit b) {
    if(!validHit(m,a)||!validHit(m,b))return 0;
    const EdFace *fa=&m->c[a.cell].f[a.face],*fb=&m->c[b.cell].f[b.face];
    if(a.vertex>=0&&b.vertex>=0)return same(fa->v[a.vertex].p,fb->v[b.vertex].p);
    if(a.edge>=0&&a.vertex<0&&b.edge>=0&&b.vertex<0) {
        EdVec a0=fa->v[a.edge].p,a1=fa->v[(a.edge+1)%fa->count].p,b0=fb->v[b.edge].p,b1=fb->v[(b.edge+1)%fb->count].p;
        return (same(a0,b0)&&same(a1,b1))||(same(a0,b1)&&same(a1,b0));
    }
    return a.vertex<0&&a.edge<0&&b.vertex<0&&b.edge<0&&a.cell==b.cell&&a.face==b.face;
}
static void addHit(const EdModel *m,EdSelection *selection,EdHit hit) {
    for(int i=0;i<selection->count;i++)if(equalHit(m,selection->hits[i],hit))return;
    if(selection->count<ED_SELECTIONS)selection->hits[selection->count++]=hit;
}
int edSelectionToggle(const EdModel *m,EdSelection *selection,EdHit hit) {
    if(!validHit(m,hit))return 0;
    for(int i=0;i<selection->count;i++)if(equalHit(m,selection->hits[i],hit)){memmove(&selection->hits[i],&selection->hits[i+1],sizeof(EdHit)*(selection->count-i-1));selection->count--;return 1;}
    if(selection->count==ED_SELECTIONS)return 0;addHit(m,selection,hit);return 1;
}
static int onSegment(EdVec p,EdVec a,EdVec b) {
    EdVec e=edSub(b,a);double d=edDot(e,e);if(d<ED_EPS*ED_EPS)return same(p,a);
    double t=edDot(edSub(p,a),e)/d;return t>=-ED_EPS&&t<=1+ED_EPS&&length(edCross(edSub(p,a),e))<ED_EPS*sqrt(d);
}
void edSelectionRemap(const EdModel *from,const EdModel *to,const EdSelection *selection,EdVec delta,EdSelection *result) {
    result->count=0;
    for(int i=0;i<selection->count;i++) {
        EdHit h=selection->hits[i];if(!validHit(from,h))continue;
        EdFace source=from->c[h.cell].f[h.face];for(int v=0;v<source.count;v++)source.v[v].p=edAdd(source.v[v].p,delta);
        EdVec n=edNormal(&source);
        for(int s=0;s<to->count;s++)for(int f=0;f<to->c[s].count;f++) {
            const EdFace *face=&to->c[s].f[f];if((face->neighbor>=0)!=(source.neighbor>=0))continue;
            EdHit match={s,f,-1,-1};
            if(h.vertex>=0) {
                for(int v=0;v<face->count;v++)if(same(source.v[h.vertex].p,face->v[v].p)){match.vertex=v;match.point=face->v[v].p;addHit(to,result,match);}
            } else if(h.edge>=0) {
                EdVec a=source.v[h.edge].p,b=source.v[(h.edge+1)%source.count].p;
                for(int v=0;v<face->count;v++)if(onSegment(face->v[v].p,a,b)&&onSegment(face->v[(v+1)%face->count].p,a,b)){match.edge=v;match.point=edScale(edAdd(face->v[v].p,face->v[(v+1)%face->count].p),.5);addHit(to,result,match);}
            } else if(edDot(n,edNormal(face))>.99999&&fabs(edDot(n,edSub(face->v[0].p,source.v[0].p)))<ED_EPS*4) {
                EdFace intersection=*face,tmp;
                for(int e=0;e<source.count&&intersection.count;e++) {
                    EdVec en=unit(edCross(n,edSub(source.v[(e+1)%source.count].p,source.v[e].p)));
                    if(!clip(&intersection,en,edDot(en,source.v[e].p),&tmp)){intersection.count=0;break;}intersection=tmp;
                }
                if(intersection.count) {match.point=edV(0,0,0);for(int v=0;v<face->count;v++)match.point=edAdd(match.point,face->v[v].p);match.point=edScale(match.point,1.0/face->count);addHit(to,result,match);}
            }
        }
    }
}
#include "edMesh.h"

unsigned int edCellHash(const EdCell *c){EdWire measure={NULL,ED_NET_LIMIT/2,0,0,1};edWireCell(&measure,(EdCell*)c);if(!measure.ok)return 0;std::vector<unsigned char> bytes(measure.at);EdWire w={bytes.data(),bytes.size(),0,0,1};edWireCell(&w,(EdCell*)c);unsigned int hash=w.ok?edNetHash(bytes.data(),(uint32_t)bytes.size()):0;for(int f=0;f<c->count;f++)if(c->f[f].decalId)hash=(hash^(unsigned int)c->f[f].decalId)*16777619u;return hash;}

#include "edGroups.h"
#include "edSpline.h"

#include "edTerrain.h"

#include "edTerrainPaint.h"

#include "edDecal.h"
