#ifndef ED_MAP_H
#define ED_MAP_H
#include "edGeometry.h"
#include <math.h>
#include <string.h>
/* Drawing and input share the same power-of-two grid, at least 12 pixels apart. */
static double edMapGridSpacing(double zoom){double spacing=ED_GRID;if(!isfinite(zoom)||zoom<=0)return spacing;while(spacing*zoom<12)spacing*=2;return spacing;}
static EdVec edMapSnapXY(EdVec p,double spacing){return edV(round(p.x/spacing)*spacing,round(p.y/spacing)*spacing,p.z);}
/* Project every convex boundary polygon. Their union is the sector footprint,
   including sloped sectors and sectors stacked above one another. */
static int edMapFacePoint(const EdFace *f,double x,double y) {
    double area=0;int positive=0,negative=0;
    for(int v=0;v<f->count;v++){EdVec a=f->v[v].p,b=f->v[(v+1)%f->count].p;area+=a.x*b.y-a.y*b.x;double d=(b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);positive|=d>ED_EPS;negative|=d<-ED_EPS;}
    return fabs(area)>ED_EPS&&!(positive&&negative);
}
static int edMapSectorPoint(const EdCell *c,double x,double y) {
    for(int f=0;f<c->count;f++)if(edMapFacePoint(&c->f[f],x,y))return 1;return 0;
}
static int edMapSectorBox(const EdCell *c,double x0,double y0,double x1,double y1) {
    double left=fmin(x0,x1),right=fmax(x0,x1),bottom=fmin(y0,y1),top=fmax(y0,y1);
    if(edMapSectorPoint(c,left,bottom)||edMapSectorPoint(c,left,top)||edMapSectorPoint(c,right,bottom)||edMapSectorPoint(c,right,top))return 1;
    for(int f=0;f<c->count;f++)for(int v=0;v<c->f[f].count;v++){
        EdVec a=c->f[f].v[v].p,b=c->f[f].v[(v+1)%c->f[f].count].p;
        if(a.x>=left&&a.x<=right&&a.y>=bottom&&a.y<=top)return 1;
        double lo=0,hi=1,d[2]={b.x-a.x,b.y-a.y},p[2]={a.x,a.y},mn[2]={left,bottom},mx[2]={right,top};int hit=1;
        for(int axis=0;axis<2;axis++){if(fabs(d[axis])<ED_EPS){if(p[axis]<mn[axis]||p[axis]>mx[axis])hit=0;}else{double t=(mn[axis]-p[axis])/d[axis],u=(mx[axis]-p[axis])/d[axis];lo=fmax(lo,fmin(t,u));hi=fmin(hi,fmax(t,u));}}
        if(hit&&lo<=hi)return 1;
    }return 0;
}
static EdVec edMapCenter(const EdCell *c) {
    EdVec p=edV(0,0,0);int count=0;for(int f=0;f<c->count;f++)for(int v=0;v<c->f[f].count;v++){p=edAdd(p,c->f[f].v[v].p);count++;}return edScale(p,1.0/count);
}
static int edMapInsideSelected(const EdModel *m,const EdBuffer<unsigned char>& selected,EdVec p) {
    for(int s=0;s<m->count;s++)if(selected[s]&&edContains(&m->c[s],p))return 1;return 0;
}
/* Work on a transaction candidate. Rebuild every portal after detaching/moving
   cells; retain surface attributes so newly solid boundaries become visible. */
static int edMapTranslate(EdModel *out,const EdModel *source,const EdBuffer<unsigned char>& selected,EdVec delta,int copy,EdBuffer<unsigned char>& result) {
    for(int s=0;s<source->count;s++)if(selected[s]&&source->c[s].splineId)return 0;
    delta=edSnapPoint(delta);int count=0;for(int s=0;s<source->count;s++)count+=selected[s]!=0;
    if(!count||(copy&&count>ED_CELLS-source->count)||!isfinite(delta.x)||!isfinite(delta.y)||!isfinite(delta.z))return 0;
    edCopyModel(out,source);
    for(int s=0;s<source->count;s++)if(selected[s]){
        int target=copy?out->count++:s;out->c[target]=source->c[s];if(copy)out->c[target].arrayId=0;
        for(int f=0;f<out->c[target].count;f++)for(int v=0;v<out->c[target].f[f].count;v++)out->c[target].f[f].v[v].p=edAdd(out->c[target].f[f].v[v].p,delta);
    }
    if(!copy){
        for(int i=0;i<out->spawnCount;i++)if(edMapInsideSelected(source,selected,out->spawns[i].position)){out->spawns[i].position=edAdd(out->spawns[i].position,delta);for(int f=0;f<out->spawns[i].attributes.frameCount;f++)out->spawns[i].attributes.frames[f].position=edAdd(out->spawns[i].attributes.frames[f].position,delta);}
        if(out->spawnCount)out->spawn=out->spawns[0].position;
        for(int i=0;i<out->objectCount;i++)if(edMapInsideSelected(source,selected,out->objects[i].position)){out->objects[i].position=edAdd(out->objects[i].position,delta);for(int f=0;f<out->objects[i].attributes.frameCount;f++)out->objects[i].attributes.frames[f].position=edAdd(out->objects[i].attributes.frames[f].position,delta);}
        for(int i=0;i<out->lightCount;i++)if(edMapInsideSelected(source,selected,out->lights[i].position))out->lights[i].position=edAdd(out->lights[i].position,delta);
    }
    for(int s=0;s<out->count;s++)for(int f=0;f<out->c[s].count;f++){EdFace *face=&out->c[s].f[f];face->attributes=edSurfaceAttributes(face);face->hasAttributes=1;face->neighbor=face->mirror=-1;}
    if(!edResolve(out))return 0;
    edZero(result,0,ED_CELLS);
    for(int s=copy?source->count:0;s<out->count;s++)if(edMapInsideSelected(source,selected,edSub(edMapCenter(&out->c[s]),delta)))result[s]=1;
    return 1;
}
static const char *edMapPlacementFailure="";
static int edMapPaste(EdModel *out,const EdModel *source,const EdModel *clip,EdVec delta,EdBuffer<unsigned char>& result) {
    delta=edSnapPoint(delta);
    edClearGeometryError();edMapPlacementFailure="No sectors are ready to place.";
    if(!clip->count)return 0;
    edMapPlacementFailure="Placement exceeds the engine sector index range.";if(clip->count>ED_CELLS-source->count)return 0;
    edMapPlacementFailure="Invalid placement coordinates.";if(!isfinite(delta.x)||!isfinite(delta.y)||!isfinite(delta.z))return 0;
    edCopyModel(out,source);
    for(int s=0;s<clip->count;s++){
        EdCell *cell=&out->c[out->count++];*cell=clip->c[s];cell->arrayId=0;
        for(int f=0;f<cell->count;f++)for(int v=0;v<cell->f[f].count;v++)cell->f[f].v[v].p=edAdd(cell->f[f].v[v].p,delta);
    }
    for(int s=0;s<out->count;s++)for(int f=0;f<out->c[s].count;f++){EdFace *face=&out->c[s].f[f];face->attributes=edSurfaceAttributes(face);face->hasAttributes=1;face->neighbor=face->mirror=-1;}
    if(!edResolve(out)){edMapPlacementFailure=edGeometryFailure();return 0;}
    if(out->count==source->count){edMapPlacementFailure="No added space: the preview is entirely inside existing sectors.";return 0;}
    edZero(result,0,ED_CELLS);for(int s=source->count;s<out->count;s++)result[s]=1;return 1;
}
static double edMapTurn(EdVec a,EdVec b,EdVec c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
static int edMapOnSegment(EdVec a,EdVec b,EdVec p){return fabs(edMapTurn(a,b,p))<ED_EPS&&p.x>=fmin(a.x,b.x)-ED_EPS&&p.x<=fmax(a.x,b.x)+ED_EPS&&p.y>=fmin(a.y,b.y)-ED_EPS&&p.y<=fmax(a.y,b.y)+ED_EPS;}
static int edMapCrosses(EdVec a,EdVec b,EdVec c,EdVec d){
    double x=edMapTurn(a,b,c),y=edMapTurn(a,b,d),u=edMapTurn(c,d,a),v=edMapTurn(c,d,b);
    return ((x>ED_EPS&&y<-ED_EPS)||(x<-ED_EPS&&y>ED_EPS))&&((u>ED_EPS&&v<-ED_EPS)||(u<-ED_EPS&&v>ED_EPS))||edMapOnSegment(a,b,c)||edMapOnSegment(a,b,d)||edMapOnSegment(c,d,a)||edMapOnSegment(c,d,b);
}
/* Check the prospective closed outline, including its implicit last-to-first
   edge, so accepting another vertex can never introduce a concave corner. */
static int edMapDrawValid(const EdVec *points,int count,EdVec point,int closing){
    if(count<0||count>ED_VERTS||(!closing&&count==ED_VERTS)||closing&&count<3)return 0;
    EdVec p[ED_VERTS];int n=count;
    for(int i=0;i<count;i++)p[i]=edSnapPoint(points[i]);
    if(!closing)p[n++]=edSnapPoint(point);
    for(int i=0;i<n;i++){
        if(!isfinite(p[i].x)||!isfinite(p[i].y))return 0;
        for(int j=0;j<i;j++)if(hypot(p[i].x-p[j].x,p[i].y-p[j].y)<ED_EPS)return 0;
    }
    if(n<3)return !closing;
    int positive=0,negative=0;double area=0;
    for(int i=0;i<n;i++){double turn=edMapTurn(p[i],p[(i+1)%n],p[(i+2)%n]);positive|=turn>ED_EPS;negative|=turn<-ED_EPS;area+=p[i].x*p[(i+1)%n].y-p[i].y*p[(i+1)%n].x;}
    if(positive&&negative)return 0;
    if(fabs(area)<ED_EPS){
        if(closing)return 0;
        for(int i=1;i<n-1;i++)if(edDot(edSub(p[i],p[i-1]),edSub(p[i+1],p[i]))<=0)return 0;
        return 1;
    }
    for(int i=0;i<n;i++){
        EdVec a=p[i],b=p[(i+1)%n],c=p[(i+2)%n];
        if(fabs(edMapTurn(a,b,c))<ED_EPS&&edDot(edSub(b,a),edSub(c,b))<0)return 0;
        for(int j=i+1;j<n;j++)if(j!=i+1&&!(i==0&&j==n-1)&&edMapCrosses(a,b,p[j],p[(j+1)%n]))return 0;
    }
    return 1;
}
static void edMapPrism(EdCell *cell,const EdVec *points,int count,double floorZ,double ceilingZ,int material){
    edZero(cell,0,sizeof(*cell));cell->count=count+2;cell->extraLight=.5;
    for(int f=0;f<cell->count;f++){EdFace *face=&cell->f[f];face->neighbor=face->mirror=-1;face->material=material;face->count=f<2?count:4;
        for(int v=0;v<face->count;v++){
            int index=f==0?v:f==1?count-1-v:v<2?f-2:(f-1)%count;
            face->v[v].p=points[index];face->v[v].p.z=f==0?floorZ:f==1?ceilingZ:(v==1||v==2)?ceilingZ:floorZ;
        }edMapFace(face);
    }
}
/* Each drawn outline creates one convex prism. */
static int edMapPolygon(EdModel *clip,const EdVec *points,int count,double floorZ,double ceilingZ,int material){
    if(count<3||count>ED_VERTS||!isfinite(floorZ)||!isfinite(ceilingZ)||ceilingZ-floorZ<ED_GRID)return 0;
    EdVec p[ED_VERTS];int n=0;for(int i=0;i<count;i++){EdVec q=edSnapPoint(points[i]);q.z=0;if(!isfinite(q.x)||!isfinite(q.y))return 0;if(!n||edDot(edSub(q,p[n-1]),edSub(q,p[n-1]))>ED_EPS)p[n++]=q;}
    if(n>1&&edDot(edSub(p[0],p[n-1]),edSub(p[0],p[n-1]))<ED_EPS)n--;
    if(n<3)return 0;
    for(int i=0;i<n;i++)for(int j=i+1;j<n;j++)if(j!=i+1&&!(i==0&&j==n-1)&&edMapCrosses(p[i],p[(i+1)%n],p[j],p[(j+1)%n]))return 0;
    for(int i=0;n>3&&i<n;){int before=(i+n-1)%n,after=(i+1)%n;if(fabs(edMapTurn(p[before],p[i],p[after]))<ED_EPS&&edMapOnSegment(p[before],p[after],p[i])){memmove(p+i,p+i+1,(n-i-1)*sizeof(*p));n--;i=0;}else i++;}
    double area=0;for(int i=0;i<n;i++)area+=p[i].x*p[(i+1)%n].y-p[i].y*p[(i+1)%n].x;if(fabs(area)<ED_EPS)return 0;
    if(area<0)for(int i=0;i<n/2;i++){EdVec q=p[i];p[i]=p[n-1-i];p[n-1-i]=q;}
    int convex=1;for(int i=0;i<n;i++)if(edMapTurn(p[i],p[(i+1)%n],p[(i+2)%n])<-ED_EPS)convex=0;
    if(!convex)return 0;
    edZero(clip,0,sizeof(*clip));clip->count=1;edMapPrism(&clip->c[0],p,n,floorZ,ceilingZ,material);return edValidate(clip);
}
/* A map line defines a vertical cutting plane. Only selected sectors actually
   touched by the drawn segment are cut; other selected sectors stay selected. */
static int edMapCutMask(const EdModel *model,const EdBuffer<unsigned char>& selected,EdVec a,EdVec b,EdBuffer<unsigned char>& cut){
    a=edSnapPoint(a);b=edSnapPoint(b);a.z=b.z=0;edZero(cut,0,ED_CELLS);
    EdVec n=edV(a.y-b.y,b.x-a.x,0);double size=sqrt(edDot(n,n));if(size<ED_GRID*.5)return 0;n=edScale(n,1/size);int count=0;
    for(int s=0;s<model->count;s++)if(selected[s]){
        int positive=0,negative=0,touched=edMapSectorPoint(&model->c[s],a.x,a.y)||edMapSectorPoint(&model->c[s],b.x,b.y);
        for(int f=0;f<model->c[s].count;f++)for(int v=0;v<model->c[s].f[f].count;v++){
            const EdFace *face=&model->c[s].f[f];EdVec p=face->v[v].p,q=face->v[(v+1)%face->count].p;double d=edDot(n,edSub(p,a));positive|=d>ED_EPS;negative|=d<-ED_EPS;
            if(edMapCrosses(a,b,p,q))touched=1;
        }
        if(positive&&negative&&touched){cut[s]=1;count++;}
    }return count;
}
static int edMapCleave(EdModel *out,const EdModel *source,const EdBuffer<unsigned char>& selected,EdVec a,EdVec b,EdBuffer<unsigned char>& result){
    EdBuffer<unsigned char> cut;a=edSnapPoint(a);b=edSnapPoint(b);a.z=b.z=0;
    if(!edMapCutMask(source,selected,a,b,cut))return 0;edCopyModel(out,source);
    if(!edCleaveSectorsPlane(out,cut,edV(a.y-b.y,b.x-a.x,0),a))return 0;
    edZero(result,0,ED_CELLS);for(int s=0;s<out->count;s++)result[s]=edMapInsideSelected(source,selected,edMapCenter(&out->c[s]));return 1;
}
static int edMapDelete(EdModel *out,const EdModel *source,const EdBuffer<unsigned char>& selected,int allowSpline=0){
    for(int s=0;s<source->count;s++)if(selected[s]&&source->c[s].splineId&&!allowSpline)return 0;
    int count=0;for(int s=0;s<source->count;s++)count+=selected[s]!=0;if(!count||count==source->count)return 0;
    edCopyModel(out,source);for(int s=source->count-1;s>=0;s--)if(selected[s]){int before=out->count;if(!edDeleteSector(out,s)||out->count!=before-1){edCopyModel(out,source);return 0;}}
    return edValidate(out);
}
#endif
