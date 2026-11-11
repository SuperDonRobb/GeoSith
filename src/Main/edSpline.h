/* Included after the convex geometry implementation. */
EdVec edSplinePosition(const EdSpline *s,double t){
    if(s->count<2)return s->count?s->points[0].position:edV(0,0,0);
    int spans=s->closed?s->count:s->count-1;t=fmax(0,fmin(spans,t));int i=(int)t;if(i>=spans)i=spans-1;t-=i;double u=1-t;
    const EdSplinePoint &a=s->points[i],&b=s->points[(i+1)%s->count];
    return edAdd(edAdd(edScale(a.position,u*u*u),edScale(edAdd(a.position,a.handle),3*u*u*t)),edAdd(edScale(edSub(b.position,b.handle),3*u*t*t),edScale(b.position,t*t*t)));
}
static unsigned splineHash(const EdModel *m,int id){
    unsigned hash=2166136261u;for(int i=0;i<m->count;i++)if(m->c[i].splineId==id){EdCell c=m->c[i];
        for(int f=0;f<c.count;f++){c.f[f].neighbor=c.f[f].neighbor<0?-1:0;c.f[f].mirror=-1;}
        hash=(hash^edCellHash(&c))*16777619u;
    }return hash;
}
int edSplineValidate(const EdModel *m){
    if(m->splineCount<0||m->splineCount>ED_SPLINES)return 0;
    for(int i=0;i<m->splineCount;i++){const EdSpline &s=m->splines[i];
        if(s.groupId<0||s.groupId>m->groupCount||s.mirrorAxes<0||s.mirrorAxes>7||(s.closed!=0&&s.closed!=1)||(s.closed&&s.count<3)||s.id<=0||s.count<1||s.count>ED_SPLINE_POINTS||s.sides<3||s.sides>32||s.segments<1||s.segments>ED_SPLINE_SECTORS||s.material<0||!isfinite(s.width)||!isfinite(s.height)||s.width<ED_GRID||s.height<ED_GRID||s.width>1000||s.height>1000)return 0;
        if(s.textureHeight<0||s.textureHeight>16384||s.invalid<0||s.invalid>1||s.cutBefore.size()!=s.cutAfter.size())return 0;
        for(const EdBuffer<EdCell> *cells:{&s.cutBefore,&s.cutAfter,&s.preview})for(size_t c=0;c<cells->size();c++)for(int f=0;f<(*cells)[c].count;f++)for(int v=0;v<(*cells)[c].f[f].count;v++){EdVec p=(*cells)[c].f[f].v[v].p;if(!isfinite(p.x)||!isfinite(p.y)||!isfinite(p.z)||fabs(p.x)>1000000||fabs(p.y)>1000000||fabs(p.z)>1000000)return 0;}
        for(int j=0;j<i;j++)if(m->splines[j].id==s.id)return 0;
        for(int j=0;j<s.count;j++){const EdSplinePoint &p=s.points[j];double a[]={p.position.x,p.position.y,p.position.z,p.handle.x,p.handle.y,p.handle.z};for(double v:a)if(!isfinite(v)||fabs(v)>100000)return 0;}
    }return 1;
}
void edSplineReconcile(EdModel *m){
    // CSG intersections and adjoin changes are not a request to bake controls.
    for(int i=0;i<m->splineCount;i++)m->splines[i].geometryHash=splineHash(m,m->splines[i].id);
    for(int c=0;c<m->count;c++)if(m->c[c].splineId){int found=0;for(int i=0;i<m->splineCount;i++)found|=m->splines[i].id==m->c[c].splineId;if(!found)m->c[c].splineId=0;}
}
int edSplineConvert(EdModel *m,int id){
    for(int i=0;i<m->splineCount;i++)if(m->splines[i].id==id){
        if(m->splines[i].invalid)return 0;
        for(int c=0;c<m->count;c++)if(m->c[c].splineId==id)m->c[c].splineId=0;
        for(int j=i+1;j<m->splineCount;j++)m->splines[j-1]=std::move(m->splines[j]);m->splineCount--;return 1;
    }return 0;
}
// Restore only cells still identical to the cut result. Never overwrite a
// subsequent material/geometry edit made by the user or another player.
static bool splineSameCell(EdCell a,EdCell b){
    for(EdCell *c:{&a,&b})for(int f=0;f<c->count;f++)c->f[f].neighbor=c->f[f].mirror=-1;
    if(a.count!=b.count)return false;
    for(int f=0;f<a.count;f++){if(a.f[f].count!=b.f[f].count)return false;for(int v=0;v<a.f[f].count;v++){
        EdVertex &x=a.f[f].v[v];const EdVertex &y=b.f[f].v[v];double *xs[]={&x.p.x,&x.p.y,&x.p.z,&x.u,&x.v,&x.light};double ys[]={y.p.x,y.p.y,y.p.z,y.u,y.v,y.light};for(int k=0;k<6;k++)if(fabs(*xs[k]-ys[k])<1e-8)*xs[k]=ys[k];
    }}
    return edCellsEqual(&a,&b)!=0;
}
static void splineRestoreCuts(EdModel *m,const EdSpline &s){
    for(size_t p=0;p<s.cutAfter.size();p++)for(int c=0;c<m->count;c++)if(splineSameCell(m->c[c],s.cutAfter[p])){m->c[c]=s.cutBefore[p];break;}
}
static int splineBuild(EdModel *out,const EdModel *source,const EdSpline *spline,int textureHeight,EdBuffer<EdCell> *preview){
    if(textureHeight<1||textureHeight>16384)return 0;
    EdModel work;edCopyModel(&work,source);EdSpline settings=*spline;int index=-1;
    for(int i=0;i<work.splineCount;i++)if(work.splines[i].id==settings.id)index=i;
    if(index<0){if(work.splineCount==ED_SPLINES)return 0;index=work.splineCount++;}
    splineRestoreCuts(&work,work.splines[index]);
    settings.invalid=0;settings.textureHeight=textureHeight;settings.cutBefore.clear();settings.cutAfter.clear();settings.preview.clear();
    work.splines[index]=settings;if(!edSplineValidate(&work))return 0;
    EdBuffer<int> remap;int count=0;
    for(int i=0;i<work.count;i++){remap[i]=work.c[i].splineId==settings.id?-1:count;if(remap[i]>=0)work.c[count++]=work.c[i];}
    for(int c=0;c<work.cogCount;c++)for(int p=0;p<work.cogs[c].count;p++){EdCogParam &v=work.cogs[c].params[p];if((v.type==ED_COG_SECTOR||v.type==ED_COG_SURFACE)&&v.cell>=0&&v.cell<work.count){v.cell=remap[v.cell];if(v.cell<0)v.face=-1;}}
    work.count=count;work.c.resize(count);
    EdBuffer<EdCell> background=work.c;
    if(settings.count>=2){
        /* Arc-length sampling makes the requested number of sectors useful even
           when control points are unevenly spaced. Shared cross-section frames keep adjoining rings identical in 3D. */
        int spans=settings.closed?settings.count:settings.count-1;int samples=spans*64;std::vector<double> distances(samples+1);EdVec previous=edSplinePosition(&settings,0);
        for(int i=1;i<=samples;i++){EdVec p=edSplinePosition(&settings,spans*(double)i/samples);distances[i]=distances[i-1]+length(edSub(p,previous));previous=p;}
        if(distances.back()<ED_GRID)return 0;
        if(settings.closed&&settings.segments<3)return 0;
        std::vector<EdVec> centers(settings.segments+1),rights(settings.segments+1),ups(settings.segments+1),tangents(settings.segments+1);
        std::vector<double> parameters(settings.segments+1);
        for(int i=0;i<=settings.segments;i++){double d=distances.back()*i/settings.segments;int k=(int)(std::lower_bound(distances.begin(),distances.end(),d)-distances.begin());if(k<1)k=1;
            double u=distances[k]>distances[k-1]?(d-distances[k-1])/(distances[k]-distances[k-1]):0,t=spans*(k-1+u)/samples;centers[i]=edSplinePosition(&settings,t);parameters[i]=t;
        }
        for(int i=0;i<=settings.segments;i++){
            double t=parameters[i],epsilon=1e-5;
            EdVec tangent=edSub(edSplinePosition(&settings,settings.closed&&t+epsilon>spans?t+epsilon-spans:t+epsilon),edSplinePosition(&settings,settings.closed&&t-epsilon<0?t-epsilon+spans:t-epsilon));
            // Exact endpoint handles avoid finite-difference drift on end caps.
            if(!settings.closed&&(i==0||i==settings.segments)){
                EdVec handle=settings.points[i?settings.count-1:0].handle;if(length(handle)>ED_EPS)tangent=handle;
            }
            double n=length(tangent);if(n<1e-12)return 0;tangents[i]=edScale(tangent,1/n);
            if(!i){EdVec right=edCross(tangents[i],edV(0,0,1));if(length(right)<1e-6)right=edCross(tangents[i],edV(0,1,0));rights[i]=unit(right);}
            else{
                // Parallel transport avoids sudden rolls when the curve goes vertical.
                EdVec axis=edCross(tangents[i-1],tangents[i]);double sine=length(axis),cosine=edDot(tangents[i-1],tangents[i]);
                if(cosine<-.999999)return 0;EdVec right=rights[i-1];
                if(sine>1e-10){axis=edScale(axis,1/sine);right=edAdd(edAdd(edScale(right,cosine),edScale(edCross(axis,right),sine)),edScale(axis,edDot(axis,right)*(1-cosine)));}
                rights[i]=unit(edSub(right,edScale(tangents[i],edDot(right,tangents[i]))));
            }
            ups[i]=unit(edCross(rights[i],tangents[i]));
        }
        if(settings.closed){
            double twist=atan2(edDot(tangents[0],edCross(rights.back(),rights[0])),edDot(rights.back(),rights[0]));
            for(int i=1;i<=settings.segments;i++){double angle=twist*i/settings.segments;EdVec r=rights[i];rights[i]=edAdd(edScale(r,cos(angle)),edScale(edCross(tangents[i],r),sin(angle)));ups[i]=unit(edCross(rights[i],tangents[i]));}
            centers.back()=centers.front();rights.back()=rights.front();ups.back()=ups.front();
        }
        if(settings.groupId){
            const EdSectorGroup &group=work.groups[settings.groupId-1];EdVec lo,hi;if(!edGroupBounds(&group,&lo,&hi))return 0;
            EdVec mid=edScale(edAdd(lo,hi),.5),forward=edGroupAxis(group.direction),up=group.direction/2==2?edV(0,1,0):edV(0,0,1),right=edCross(forward,up),size=edSub(hi,lo);
            double span=fabs(edDot(size,forward)),width=fabs(edDot(size,right)),height=fabs(edDot(size,up));if(span<ED_EPS||width<ED_EPS||height<ED_EPS)return 0;
            if((size_t)group.count*settings.segments>(size_t)ED_CELLS-work.count)return 0;
            bool tilted=false;for(const EdVec &u:ups)tilted|=length(edSub(u,edV(0,0,1)))>1e-8;
            if(tilted){
                // A shared tetrahedral partition makes the 3D bend piecewise
                // affine: neighboring source cells retain identical seam planes.
                const int tetrahedra[6][4]={{0,1,3,7},{0,1,5,7},{0,2,3,7},{0,2,6,7},{0,4,5,7},{0,4,6,7}};
                EdVec sourceCorners[8];for(int k=0;k<8;k++)sourceCorners[k]=edAdd(mid,edAdd(edAdd(edScale(right,(k&1?.5:-.5)*width),edScale(forward,(k&2?.5:-.5)*span)),edScale(up,(k&4?.5:-.5)*height)));
                for(int i=0;i<settings.segments;i++){
                    EdVec destination[8];for(int k=0;k<8;k++){int ring=i+((k&2)?1:0);destination[k]=edAdd(centers[ring],edAdd(edScale(rights[ring],(k&1?.5:-.5)*settings.width),edScale(ups[ring],(k&4?.5:-.5)*settings.height)));}
                    for(const auto &tet:tetrahedra){
                        EdVec origin=sourceCorners[tet[0]],a=edSub(sourceCorners[tet[1]],origin),b=edSub(sourceCorners[tet[2]],origin),c=edSub(sourceCorners[tet[3]],origin);
                        EdVec target=destination[tet[0]],da=edSub(destination[tet[1]],target),db=edSub(destination[tet[2]],target),dc=edSub(destination[tet[3]],target);
                        double det=edDot(a,edCross(b,c));if(edDot(da,edCross(db,dc))*det<=0)return 0;
                        for(int cellIndex=0;cellIndex<group.count;cellIndex++){
                            EdCell cell=group.cells[cellIndex];
                            for(int opposite=0;opposite<4&&cell.count;opposite++){
                                int ids[3],n=0;for(int k=0;k<4;k++)if(k!=opposite)ids[n++]=tet[k];
                                EdVec plane=sourceCorners[ids[0]],normal=unit(edCross(edSub(sourceCorners[ids[1]],plane),edSub(sourceCorners[ids[2]],plane)));
                                if(edDot(normal,edSub(sourceCorners[tet[opposite]],plane))<0)normal=edScale(normal,-1);
                                EdCell clipped;if(!cellclip(&cell,normal,edDot(normal,plane),&clipped))return 0;cell=std::move(clipped);
                            }
                            if(!cell.count)continue;cell.splineId=settings.id;cell.arrayId=0;
                            for(int f=0;f<cell.count;f++){cell.f[f].customUV=1;for(int v=0;v<cell.f[f].count;v++){
                                EdVec q=edSub(cell.f[f].v[v].p,origin);double x=edDot(q,edCross(b,c))/det,y=edDot(a,edCross(q,c))/det,z=edDot(a,edCross(b,q))/det;
                                cell.f[f].v[v].p=edAdd(target,edAdd(edAdd(edScale(da,x),edScale(db,y)),edScale(dc,z)));
                            }}
                            if(work.count>=ED_CELLS)return 0;work.c[work.count++]=std::move(cell);
                        }
                    }
                }
            }else
            for(int i=0;i<settings.segments;i++)for(int c=0;c<group.count;c++){
                // Use two affine halves of the segment's horizontal footprint.
                // Bilinear bending warps sloping shared walls; independent hulls
                // then disagree on those walls even for arbitrarily small bends.
                EdVec splitNormal=unit(edSub(edScale(right,1/width),edScale(forward,1/span)));
                int halves=length(edSub(rights[i],rights[i+1]))>1e-10?2:1;
                for(int half=0;half<halves;half++){
                    EdCell cell;
                    if(halves==1)cell=group.cells[c];
                    else {EdVec n=edScale(splitNormal,half?1:-1);if(!cellclip(&group.cells[c],n,edDot(n,mid),&cell))return 0;if(!cell.count)continue;}
                    cell.splineId=settings.id;cell.arrayId=0;
                    for(int f=0;f<cell.count;f++)cell.f[f].customUV=1;
                    EdVec a=edSub(centers[i],edScale(rights[i],settings.width*.5));
                    EdVec b=edAdd(centers[i],edScale(rights[i],settings.width*.5));
                    EdVec d=edSub(centers[i+1],edScale(rights[i+1],settings.width*.5));
                    EdVec e=edAdd(centers[i+1],edScale(rights[i+1],settings.width*.5));
                    for(int f=0;f<cell.count;f++)for(int v=0;v<cell.f[f].count;v++){
                        EdVec p=edSub(cell.f[f].v[v].p,mid);double t=edDot(p,forward)/span+.5,u=edDot(p,right)/width+.5,z=edDot(p,up)*settings.height/height;
                        EdVec q=half?edAdd(edAdd(edScale(a,1-u),edScale(b,u-t)),edScale(e,t)):edAdd(edAdd(edScale(a,1-t),edScale(d,t-u)),edScale(e,u));
                        q.z+=z;cell.f[f].v[v].p=q;
                    }
                    if(work.count>=ED_CELLS)return 0;work.c[work.count++]=std::move(cell);
                }
            }
        }else{
        std::vector<EdVec> ring((settings.segments+1)*settings.sides);
        for(int i=0;i<=settings.segments;i++)for(int v=0;v<settings.sides;v++){double a=2*3.141592653589793*v/settings.sides+(settings.sides==4?3.141592653589793/4:3.141592653589793/2),scale=settings.sides==4?sqrt(2.0):1;
            ring[i*settings.sides+v]=edAdd(centers[i],edAdd(edScale(rights[i],cos(a)*settings.width*.5*scale),edScale(ups[i],sin(a)*settings.height*.5*scale)));
        }
        std::vector<double> perimeter(settings.sides+1);
        for(int v=0;v<settings.sides;v++)perimeter[v+1]=perimeter[v]+length(edSub(ring[(v+1)%settings.sides],ring[v]));
        double wrap=fmax(1,round(perimeter.back()*ED_UV_SCALE/textureHeight))*textureHeight;
        for(int i=0;i<settings.segments;i++){EdCell cell={};cell.extraLight=.5;cell.splineId=settings.id;cell.count=2;
            for(int f=0;f<2;f++){cell.f[f].count=settings.sides;cell.f[f].neighbor=cell.f[f].mirror=-1;cell.f[f].material=settings.material;for(int v=0;v<settings.sides;v++)cell.f[f].v[v].p=ring[(i+f)*settings.sides+v];}
            if(preview){EdCell outline=cell;for(int v=0;v<settings.sides;v++){EdFace &f=outline.f[outline.count++];f.count=4;f.v[0].p=ring[i*settings.sides+v];f.v[1].p=ring[i*settings.sides+(v+1)%settings.sides];f.v[2].p=ring[(i+1)*settings.sides+(v+1)%settings.sides];f.v[3].p=ring[(i+1)*settings.sides+v];}(*preview)[preview->size()]=outline;}
            // Nearly straight spans contain almost coplanar supporting planes.
            // A fixed, very tight hull tolerance can omit/duplicate their sliver
            // faces after edge cleanup. Retry grouping those planes, accepting
            // only a closed, planar, convex boundary under the normal validator.
            EdCell caps=cell;bool built=false;
            for(double epsilon:{1e-8,1e-7,1e-6,ED_EPS}){
                cell=caps;if(!hull(&cell,epsilon)||!closedCell(&cell))continue;
                bool valid=true;for(int f=0;f<cell.count;f++){const EdFace &face=cell.f[f];EdVec n=edNormal(&face);if(length(n)<.5)valid=false;
                    for(int v=0;v<face.count;v++)if(!edContains(&cell,face.v[v].p)||fabs(edDot(n,edSub(face.v[v].p,face.v[0].p)))>4*ED_EPS)valid=false;
                }
                if(valid){built=true;break;}
            }
            if(!built)return 0;
            int faces=cell.count;for(int f=0;f<faces;f++)if(cell.f[f].count>ED_SURFACE_VERTS){EdFace polygon=cell.f[f];cell.f[f].count=3;for(int v=2;v<polygon.count-1;v++){EdFace triangle=polygon;triangle.count=3;triangle.v[0]=polygon.v[0];triangle.v[1]=polygon.v[v];triangle.v[2]=polygon.v[v+1];cell.f[cell.count++]=triangle;}}
            for(int f=0;f<cell.count;f++){EdFace &face=cell.f[f];int rings[ED_VERTS],corners[ED_VERTS],mask=0,first=0,last=0;
                for(int v=0;v<face.count;v++){int found=-1;for(int k=0;k<2*settings.sides;k++)if(same(face.v[v].p,ring[i*settings.sides+k])){found=k;break;}if(found<0)return 0;
                    rings[v]=found/settings.sides;corners[v]=found%settings.sides;mask|=1<<rings[v];first|=corners[v]==0;last|=corners[v]==settings.sides-1;}
                if(mask!=3)continue; // End caps retain their planar mapping.
                face.customUV=1;
                for(int v=0;v<face.count;v++){int corner=corners[v];if(first&&last&&corner==0)corner=settings.sides;
                    face.v[v].u=distances.back()*(i+rings[v])/settings.segments*ED_UV_SCALE;
                    face.v[v].v=perimeter[corner]/perimeter.back()*wrap;}
            }
            work.c[work.count++]=std::move(cell);
        }
    }
    }
    int primaryCount=work.count;
    for(int mask=1;mask<8;mask++)if((mask&settings.mirrorAxes)==mask)for(int c=0;c<primaryCount;c++)if(work.c[c].splineId==settings.id){
        if(work.count>=ED_CELLS)return 0;EdCell reflected=work.c[c];int reverse=((mask&1)!=0)^((mask&2)!=0)^((mask&4)!=0);
        for(int f=0;f<reflected.count;f++){EdFace &face=reflected.f[f];for(int v=0;v<face.count;v++){EdVec &p=face.v[v].p;if(mask&1)p.x=-p.x;if(mask&2)p.y=-p.y;if(mask&4)p.z=-p.z;}
            if(reverse)for(int v=0;v<face.count/2;v++)std::swap(face.v[v],face.v[face.count-1-v]);}
        work.c[work.count++]=std::move(reflected);
    }
    for(int c=0;c<work.count;c++)for(int f=0;f<work.c[c].count;f++){EdFace &face=work.c[c].f[f];face.attributes=edSurfaceAttributes(&face);face.hasAttributes=1;face.neighbor=face.mirror=-1;}
    if(preview&&preview->size()==0)for(int c=count;c<work.count;c++)(*preview)[preview->size()]=work.c[c];
    if(!edResolve(&work))return 0;
    for(int c=0;c<count&&c<work.count;c++)if(!splineSameCell(background[c],work.c[c])){
        size_t p=work.splines[index].cutBefore.size();work.splines[index].cutBefore[p]=background[c];work.splines[index].cutAfter[p]=work.c[c];
    }

    work.splines[index].geometryHash=splineHash(&work,settings.id);edSplineReconcile(&work);
    if(!edValidate(&work))return 0;edCopyModel(out,&work);return 1;
}
int edSplineRebuild(EdModel *out,const EdModel *source,const EdSpline *spline,int textureHeight){return splineBuild(out,source,spline,textureHeight,NULL);}
static void splineInvalidOutline(EdSpline &s){
    if(s.count<2)return;
    s.preview.clear();std::vector<EdVec> ring((s.segments+1)*s.sides);double spans=s.closed?s.count:s.count-1;
    for(int i=0;i<=s.segments;i++){
        double t=spans*i/s.segments;EdVec center=edSplinePosition(&s,t),forward=edSub(edSplinePosition(&s,t+.001),edSplinePosition(&s,t-.001));
        if(length(forward)<1e-12)forward=edV(0,1,0);forward=edScale(forward,1/length(forward));EdVec right=edCross(forward,edV(0,0,1));if(length(right)<1e-6)right=edCross(forward,edV(0,1,0));right=unit(right);EdVec up=unit(edCross(right,forward));
        for(int v=0;v<s.sides;v++){double a=2*3.141592653589793*v/s.sides+(s.sides==4?3.141592653589793/4:3.141592653589793/2),scale=s.sides==4?sqrt(2.0):1;ring[i*s.sides+v]=edAdd(center,edAdd(edScale(right,cos(a)*s.width*.5*scale),edScale(up,sin(a)*s.height*.5*scale)));}
    }
    for(int mask=0;mask<8;mask++)if((mask&s.mirrorAxes)==mask)for(int i=0;i<s.segments;i++){
        EdCell cell={};for(int v=0;v<s.sides;v++){EdFace &f=cell.f[cell.count++];f.count=4;f.v[0].p=ring[i*s.sides+v];f.v[1].p=ring[i*s.sides+(v+1)%s.sides];f.v[2].p=ring[(i+1)*s.sides+(v+1)%s.sides];f.v[3].p=ring[(i+1)*s.sides+v];for(int j=0;j<4;j++){EdVec &p=f.v[j].p;if(mask&1)p.x=-p.x;if(mask&2)p.y=-p.y;if(mask&4)p.z=-p.z;}}
        s.preview[s.preview.size()]=std::move(cell);
    }
}
static int splineUpdateOne(EdModel *out,const EdModel *source,const EdSpline *spline,int textureHeight){
    EdBuffer<EdCell> preview;
    if(splineBuild(out,source,spline,textureHeight,&preview))return 1;
    // Commit the controls, but remove their old live geometry. The invalid
    // preview is editor data only, never part of collision or the renderer.
    EdSpline empty=*spline;empty.count=1;empty.closed=0;
    EdModel draft;if(!splineBuild(&draft,source,&empty,textureHeight,NULL))return 0;
    for(int i=0;i<draft.splineCount;i++)if(draft.splines[i].id==spline->id){
        EdSpline replacement=*spline;replacement.invalid=1;replacement.textureHeight=textureHeight;replacement.cutBefore.clear();replacement.cutAfter.clear();replacement.preview=std::move(preview);if(replacement.preview.size()<(size_t)replacement.segments||replacement.mirrorAxes)splineInvalidOutline(replacement);draft.splines[i]=std::move(replacement);break;
    }
    if(!edSplineValidate(&draft)||!edValidate(&draft))return 0;edCopyModel(out,&draft);return 1;
}
int edSplineUpdate(EdModel *out,const EdModel *source,const EdSpline *spline,int textureHeight){
    // Unwind dependent cuts before regenerating controls. Otherwise two splines
    // touching the same wall can preserve each other's obsolete partitions.
    EdModel base=*source;int selected=-1;
    for(int i=0;i<base.splineCount;i++)if(base.splines[i].id==spline->id)selected=i;
    for(int pass=0;pass<=base.splineCount;pass++)for(int i=base.splineCount-1;i>=0;i--)splineRestoreCuts(&base,base.splines[i]);
    EdBuffer<int> remap;int count=0;
    for(int c=0;c<base.count;c++){remap[c]=base.c[c].splineId?-1:count;if(remap[c]>=0)base.c[count++]=base.c[c];}
    for(int c=0;c<base.cogCount;c++)for(int p=0;p<base.cogs[c].count;p++){EdCogParam &v=base.cogs[c].params[p];if((v.type==ED_COG_SECTOR||v.type==ED_COG_SURFACE)&&v.cell>=0&&v.cell<base.count){v.cell=remap[v.cell];if(v.cell<0)v.face=-1;}}
    base.count=count;base.c.resize(count);
    if(selected<0){if(base.splineCount>=ED_SPLINES)return 0;selected=base.splineCount++;}base.splines[selected]=*spline;
    if(!edSplineValidate(&base))return 0;
    for(int i=0;i<base.splineCount;i++){base.splines[i].cutBefore.clear();base.splines[i].cutAfter.clear();}
    for(int c=0;c<base.count;c++)for(int f=0;f<base.c[c].count;f++){EdFace &face=base.c[c].f[f];face.neighbor=face.mirror=-1;}
    if(!edResolve(&base)||!edValidate(&base))return 0;
    for(int i=0;i<base.splineCount;i++){EdSpline controls=base.splines[i];EdModel next;if(!splineUpdateOne(&next,&base,&controls,i==selected?textureHeight:controls.textureHeight>0?controls.textureHeight:64))return 0;base=std::move(next);}
    edSplineReconcile(&base);edCopyModel(out,&base);return 1;
}
void edSplineReadMetadata(EdModel *m,const char *text){
    EdBuffer<std::vector<unsigned char>> states;
    m->splineCount=0;for(int c=0;c<m->count;c++)m->c[c].splineId=0;
    for(const char *line=text;line&&*line;line=strchr(line,'\n')){if(*line=='\n')line++;EdSpline s={};int id,point,cell;EdSplinePoint p={};
        if(sscanf(line,"# GeoSith spline %d %d %d %d %lf %lf",&s.id,&s.sides,&s.segments,&s.material,&s.width,&s.height)==6&&m->splineCount<ED_SPLINES)m->splines[m->splineCount++]=s;
        int group;if(sscanf(line,"# GeoSith splinegroup %d %d",&id,&group)==2)for(int i=0;i<m->splineCount;i++)if(m->splines[i].id==id)m->splines[i].groupId=group;
        int axes;if(sscanf(line,"# GeoSith splinemirror %d %d",&id,&axes)==2)for(int i=0;i<m->splineCount;i++)if(m->splines[i].id==id)m->splines[i].mirrorAxes=axes;
        if(sscanf(line,"# GeoSith splineclosed %d",&id)==1)for(int i=0;i<m->splineCount;i++)if(m->splines[i].id==id)m->splines[i].closed=1;
        if(sscanf(line,"# GeoSith splinepoint %d %d %lf %lf %lf %lf %lf %lf",&id,&point,&p.position.x,&p.position.y,&p.position.z,&p.handle.x,&p.handle.y,&p.handle.z)==8)for(int i=0;i<m->splineCount;i++)if(m->splines[i].id==id&&point==m->splines[i].count&&point<ED_SPLINE_POINTS)m->splines[i].points[m->splines[i].count++]=p;
        int stateId;unsigned offset;char hex[257];
        if(sscanf(line,"# GeoSith splinestate %d %u %256s",&stateId,&offset,hex)==3){
            for(int i=0;i<m->splineCount;i++)if(m->splines[i].id==stateId){
                size_t n=strlen(hex);if(n%2||offset!=states[i].size()||offset>ED_NET_LIMIT/2-n/2)break;
                auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
                for(size_t b=0;b<n/2;b++){int a=digit(hex[b*2]),d=digit(hex[b*2+1]);if(a<0||d<0)break;states[i].push_back((unsigned char)(a*16+d));}
            }
        }
        if(sscanf(line,"# GeoSith splinecell %d %d",&cell,&id)==2&&cell>=0&&cell<m->count)m->c[cell].splineId=id;
    }
    for(int i=0;i<m->splineCount;i++)if(!states[i].empty()){EdSpline candidate=m->splines[i];EdWire w={states[i].data(),states[i].size(),0,1,1};edWireSplineState(&w,&candidate);if(w.ok&&w.at==w.size)m->splines[i]=std::move(candidate);}
    if(!edSplineValidate(m)){m->splineCount=0;for(int c=0;c<m->count;c++)m->c[c].splineId=0;return;}
    for(int i=0;i<m->splineCount;i++)m->splines[i].geometryHash=splineHash(m,m->splines[i].id);
}
