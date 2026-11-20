/* Convex edge chamfers and expanded portal collars. Work on a private document. */
static bool bevelFaceEdge(const EdFace &face,EdVec a,EdVec b){
    for(int e=0;e<face.count;e++)if(onSegment(a,face.v[e].p,face.v[(e+1)%face.count].p)&&onSegment(b,face.v[e].p,face.v[(e+1)%face.count].p))return true;
    return false;
}
static void bevelFreshFace(EdFace &face,const EdFace &material){
    face.material=material.material;face.neighbor=face.mirror=-1;
    face.attributes=edSurfaceAttributes(&material);face.hasAttributes=1;face.attributes.portal=0;face.attributes.geometryMode=4;
    face.attributes.surfaceFlags=(face.attributes.surfaceFlags&~5u)|4u|(edNormal(&face).z>.5?1u:0u);
    face.customUV=face.decalId=face.bakedLight=0;face.uOffset=face.vOffset=face.uvRotation=0;edMapFace(&face);
}
int edBevel(EdModel *out,const EdModel *source,EdHit hit,double width){
    if(!out||!source||!validHit(source,hit)||hit.edge<0||hit.vertex>=0||!isfinite(width)||width<=ED_EPS)return 0;
    const EdCell &cell=source->c[hit.cell];if(cell.splineId)return 0;
    const EdFace &picked=cell.f[hit.face];EdVec a=picked.v[hit.edge].p,b=picked.v[(hit.edge+1)%picked.count].p;
    if(length(edSub(b,a))<ED_EPS)return 0;
    int portal=-1,other=-1;
    for(int f=0;f<cell.count;f++)if(bevelFaceEdge(cell.f[f],a,b)){
        if(cell.f[f].neighbor>=0&&(portal<0||f==hit.face))portal=f;
        if(f!=hit.face&&edDot(edNormal(&picked),edNormal(&cell.f[f]))<.99999)other=f;
    }
    auto storage=std::make_unique<EdModel>(*source);EdModel &work=*storage;
    if(portal<0){
        if(other<0)return 0;
        EdVec n1=edNormal(&picked),n2=edNormal(&cell.f[other]),n=edAdd(n1,n2);double dot=edDot(n1,n2);
        if(length(n)<ED_EPS)return 0;n=unit(n);double d=edDot(n,a)+width*sqrt(fmax(0,(1-dot)*.5));
        EdCell clipped;if(!cellclip(&cell,n,d,&clipped)||!clipped.count||clipped.count<=cell.count-2)return 0;
        int caps=0;for(int f=0;f<clipped.count;f++)if(edDot(edNormal(&clipped.f[f]),n)>.99999&&fabs(edDot(n,clipped.f[f].v[0].p)-d)<ED_EPS){bevelFreshFace(clipped.f[f],picked);caps++;}
        if(!caps)return 0;work.c[hit.cell]=std::move(clipped);
    }else{
        const EdFace &opening=cell.f[portal];int neighbor=opening.neighbor;
        if(neighbor<0||neighbor>=source->count||source->c[neighbor].splineId)return 0;
        EdVec n=edNormal(&opening);EdFace base=opening,far=opening;
        EdVec edgeNormals[ED_VERTS];double nearOffset[ED_VERTS],farOffset[ED_VERTS];int sharedPlane[ED_VERTS];
        for(int e=0;e<opening.count;e++){
            EdVec a=opening.v[e].p,b=opening.v[(e+1)%opening.count].p;
            edgeNormals[e]=unit(edCross(n,edSub(b,a)));nearOffset[e]=-width;farOffset[e]=0;sharedPlane[e]=-1;
            // A continuous floor/wall is not a corner. Keep both collar rings
            // on that plane, including when it slopes along the tunnel.
            for(int f=0;f<cell.count&&sharedPlane[e]<0;f++)if(f!=portal&&bevelFaceEdge(cell.f[f],a,b)){
                EdVec normal=edNormal(&cell.f[f]);double projected=edDot(normal,edgeNormals[e]);if(projected<ED_EPS)continue;
                const EdCell &adjacent=source->c[neighbor];
                for(int g=0;g<adjacent.count;g++)if(bevelFaceEdge(adjacent.f[g],a,b)&&edDot(normal,edNormal(&adjacent.f[g]))>1-1e-8&&fabs(edDot(normal,edSub(adjacent.f[g].v[0].p,a)))<ED_EPS){
                    sharedPlane[e]=f;nearOffset[e]=0;farOffset[e]=width*edDot(normal,n)/projected;break;
                }
            }
        }
        for(int v=0;v<opening.count;v++){
            int prev=(v+opening.count-1)%opening.count;EdVec p=opening.v[v].p,e1=edgeNormals[prev],e2=edgeNormals[v];double dot=edDot(e1,e2),det=1-dot*dot;
            auto offset=[&](double a,double b,EdVec &result){
                if(det<1e-10){if(dot<0||fabs(a-b)>ED_EPS)return false;result=edScale(e1,a);return true;}
                result=edAdd(edScale(e1,(a-dot*b)/det),edScale(e2,(b-dot*a)/det));return true;
            };
            EdVec nearDelta,farDelta;if(!offset(nearOffset[prev],nearOffset[v],nearDelta)||!offset(farOffset[prev],farOffset[v],farDelta))return 0;
            base.v[v].p=edAdd(p,nearDelta);far.v[v].p=edAdd(edSub(p,edScale(n,width)),farDelta);
            if(!edContains(&source->c[neighbor],far.v[v].p))return 0;
        }
        EdCell collar=source->c[neighbor];collar.count=0;collar.f.clear();collar.arrayId=0;
        EdFace near=base;reverse(&near);bevelFreshFace(near,picked);bevelFreshFace(far,picked);addface(&collar,near);addface(&collar,far);
        for(int v=0;v<base.count;v++){
            int j=(v+1)%base.count;EdFace side={};side.count=4;side.v[0]=base.v[v];side.v[1]=base.v[j];side.v[2]=far.v[j];side.v[3]=far.v[v];
            bevelFreshFace(side,sharedPlane[v]>=0?cell.f[sharedPlane[v]]:picked);if(!addface(&collar,side))return 0;
        }
        // Remove only the collar's volume from the neighbor, retaining any
        // surrounding geometry when the opening covers only part of its wall.
        EdCell remaining=source->c[neighbor];std::vector<EdCell> pieces;
        for(int f=0;f<collar.count&&remaining.count;f++){
            EdVec normal=edNormal(&collar.f[f]);double d=edDot(normal,collar.f[f].v[0].p);EdCell inside,outside;
            if(!cellclip(&remaining,normal,d,&inside)||!cellclip(&remaining,edScale(normal,-1),-d,&outside))return 0;
            if(outside.count)pieces.push_back(std::move(outside));remaining=std::move(inside);
        }
        if(!remaining.count||pieces.empty())return 0;
        work.c[neighbor]=std::move(pieces[0]);for(size_t i=1;i<pieces.size();i++)work.c[work.count++]=std::move(pieces[i]);
        work.c[work.count++]=std::move(collar);
    }
    for(int i=0;i<work.cogCount;i++)for(int j=0;j<work.cogs[i].count;j++){
        EdCogParam &p=work.cogs[i].params[j];if(p.type==ED_COG_SURFACE&&p.cell>=0&&p.cell<source->count&&p.face>=0&&p.face<source->c[p.cell].count)remapCogSurface(&p,&source->c[p.cell].f[p.face],&work);
    }
    if(!edResolve(&work)||!edValidate(&work))return 0;
    edCopyModel(out,&work);return 1;
}

int edBevelSelection(EdModel *out,const EdModel *source,const EdSelection *selection,double width){
    if(!out||!source||!selection||selection->count<1||selection->count>ED_SELECTIONS||!isfinite(width)||width<=ED_EPS)return 0;
    struct Cut {int cell;EdVec a,b,n;double d;EdFace material;};
    struct Portal {int cell,face,other,mirror;EdFace outline;};
    std::vector<Cut> cuts;std::vector<Portal> portals;
    for(int i=0;i<selection->count;i++){
        EdHit h=selection->hits[i];if(!validHit(source,h)||h.edge<0||h.vertex>=0||source->c[h.cell].splineId)return 0;
        const EdCell &cell=source->c[h.cell];const EdFace &face=cell.f[h.face];EdVec a=face.v[h.edge].p,b=face.v[(h.edge+1)%face.count].p;
        int portal=-1,other=-1;for(int f=0;f<cell.count;f++)if(bevelFaceEdge(cell.f[f],a,b)){
            if(cell.f[f].neighbor>=0&&(portal<0||f==h.face))portal=f;
            if(f!=h.face&&edDot(edNormal(&face),edNormal(&cell.f[f]))<.99999)other=f;
        }
        if(portal>=0){
            const EdFace &opening=cell.f[portal];bool duplicate=false;
            for(const Portal &p:portals)if((p.cell==h.cell&&p.face==portal)||(p.other==h.cell&&p.mirror==portal))duplicate=true;
            if(!duplicate)portals.push_back({h.cell,portal,opening.neighbor,opening.mirror,opening});
        }else{
            if(other<0)return 0;bool duplicate=false;
            for(const Cut &cut:cuts)if(cut.cell==h.cell&&((same(cut.a,a)&&same(cut.b,b))||(same(cut.a,b)&&same(cut.b,a))))duplicate=true;
            if(duplicate)continue;
            EdVec n1=edNormal(&face),n2=edNormal(&cell.f[other]),n=edAdd(n1,n2);if(length(n)<ED_EPS)return 0;n=unit(n);
            cuts.push_back({h.cell,a,b,n,edDot(n,a)+width*sqrt(fmax(0,(1-edDot(n1,n2))*.5)),face});
        }
    }
    auto work=std::make_unique<EdModel>(*source);
    // All planes come from the original selection, so adjacent bevels cannot
    // accidentally select a newly created edge or change one another's width.
    for(const Cut &cut:cuts){
        EdCell clipped;if(!cellclip(&work->c[cut.cell],cut.n,cut.d,&clipped)||!clipped.count)return 0;
        int caps=0;for(int f=0;f<clipped.count;f++)if(edDot(edNormal(&clipped.f[f]),cut.n)>.99999&&fabs(edDot(cut.n,clipped.f[f].v[0].p)-cut.d)<ED_EPS){bevelFreshFace(clipped.f[f],cut.material);caps++;}
        if(!caps)return 0;work->c[cut.cell]=std::move(clipped);
    }
    if(!cuts.empty()){
        for(int i=0;i<work->cogCount;i++)for(int j=0;j<work->cogs[i].count;j++){
            EdCogParam &p=work->cogs[i].params[j];if(p.type==ED_COG_SURFACE&&p.cell>=0&&p.cell<source->count&&p.face>=0&&p.face<source->c[p.cell].count)remapCogSurface(&p,&source->c[p.cell].f[p.face],work.get());
        }
        if(!edResolve(work.get()))return 0;
    }
    for(const Portal &portal:portals){
        // Earlier bevels can renumber or trim the opening. Find it by its
        // original plane and overlapping area, never by stale face indices.
        EdVec n=edNormal(&portal.outline);EdHit target={-1,-1,-1,-1};double best=ED_EPS*ED_EPS;
        for(int c=0;c<work->count;c++)for(int f=0;f<work->c[c].count;f++){
            const EdFace &face=work->c[c].f[f];if(face.neighbor<0||edDot(edNormal(&face),n)<.99999||fabs(edDot(n,edSub(face.v[0].p,portal.outline.v[0].p)))>ED_EPS)continue;
            EdFace overlap=face,tmp;for(int e=0;e<portal.outline.count&&overlap.count;e++){
                EdVec a=portal.outline.v[e].p,b=portal.outline.v[(e+1)%portal.outline.count].p,en=unit(edCross(n,edSub(b,a)));
                if(!clip(&overlap,en,edDot(en,a),&tmp))return 0;overlap=tmp;
            }
            double area=0;for(int v=1;v+1<overlap.count;v++)area+=length(edCross(edSub(overlap.v[v].p,overlap.v[0].p),edSub(overlap.v[v+1].p,overlap.v[0].p)));
            if(area>best){best=area;target={c,f,-1,0};}
        }
        if(target.cell<0)return 0;auto next=std::make_unique<EdModel>();if(!edBevel(next.get(),work.get(),target,width))return 0;work=std::move(next);
    }
    if(!edValidate(work.get()))return 0;edCopyModel(out,work.get());return 1;
}
