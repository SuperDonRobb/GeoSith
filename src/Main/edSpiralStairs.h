// Floating annular treads and invisible, solid floor ramps.
// Keep air below each tread so subsequent turns remain traversable.
int edCreateSpiralStairs(EdModel *out,const EdModel *source,int sector,int steps,double rotation,int post,double gap){
    if(!std::isfinite(gap)||gap<0||gap>=ED_GRID-ED_EPS||!std::isfinite(rotation)||!out||!source||sector<0||sector>=source->count||steps<1||steps>64)return 0;
    EdCell room=source->c[sector];if(room.count!=6||room.splineId)return 0;
    int floor=-1,ceiling=-1;EdVec center=edV(0,0,0);
    for(int f=0;f<6;f++){EdVec n=edNormal(&room.f[f]);if(n.z>.99999)floor=f;else if(n.z<-.99999)ceiling=f;else if(fabs(n.z)>ED_EPS)return 0;}
    if(floor<0||ceiling<0)return 0;
    const EdFace bottom=room.f[floor];for(int v=0;v<bottom.count;v++)center=edAdd(center,bottom.v[v].p);center=edScale(center,1.0/bottom.count);
    double low=center.z,high=room.f[ceiling].v[0].p.z,radius=1e30;
    if(high-low<=.25+ED_GRID)return 0;
    for(int f=0;f<6;f++)if(f!=floor&&f!=ceiling)radius=fmin(radius,edDot(edNormal(&room.f[f]),edSub(center,room.f[f].v[0].p)));
    if(radius<ED_GRID*2)return 0;
    std::swap(room.f[0],room.f[floor]);
    EdModel work=*source;std::vector<EdCell> pieces;
    struct Ramp { EdVec n;double d;EdCell column; };std::vector<Ramp> ramps;
    // Fixed pitch and rise allow successive revolutions above earlier treads.
    const int perTurn=16;const double angle=6.2831853071795864769/perTurn;
    const double rise=ED_GRID,thickness=ED_GRID-gap,innerRadius=radius*.2;
    if(low+steps*rise+.25>high+ED_EPS)return 0;
    auto append=[&](const EdCell &cell){if(cell.count)pieces.push_back(cell);};
    for(int slot=0;slot<perTurn;slot++){
        double a=rotation+slot*angle,b=a+angle,mid=(a+b)*.5;
        EdVec left=edV(-sin(a),cos(a),0),right=edV(sin(b),-cos(b),0),radial=edV(cos(mid),sin(mid),0);
        EdCell first,wedge,outside,inside,core,ring;
        if(!cellclip(&room,left,edDot(left,center),&first)||!cellclip(&first,right,edDot(right,center),&wedge))return 0;
        double d=edDot(radial,center)+radius*cos(angle*.5),innerD=edDot(radial,center)+innerRadius*cos(angle*.5);
        if(!cellclip(&wedge,radial,d,&outside)||!cellclip(&wedge,edScale(radial,-1),-d,&inside)||
           !cellclip(&inside,radial,innerD,&ring)||!cellclip(&inside,edScale(radial,-1),-innerD,&core))return 0;
        append(outside);if(!post)append(core);
        EdVec corners[4]={edAdd(center,edV(innerRadius*cos(a),innerRadius*sin(a),0)),edAdd(center,edV(radius*cos(a),radius*sin(a),0)),edAdd(center,edV(radius*cos(b),radius*sin(b),0)),edAdd(center,edV(innerRadius*cos(b),innerRadius*sin(b),0))};
        EdVec diagonal=unit(edCross(edSub(corners[3],corners[1]),edV(0,0,1)));
        // A rising annular quad is twisted. Two planar triangles share exactly
        // the same diagonal and end heights, avoiding a collision crack.
        for(int triangle=0;triangle<2;triangle++){
            int ids[3]={triangle?1:0,triangle?2:1,3};EdVec n=diagonal;
            if(edDot(n,edSub(corners[ids[1]],corners[1]))<0||(!triangle&&edDot(n,edSub(corners[0],corners[1]))<0))n=edScale(n,-1);
            EdCell column,remaining;if(!cellclip(&ring,n,edDot(n,corners[1]),&column)||!column.count)return 0;remaining=column;
            for(int step=slot;step<steps;step+=perTurn){
                double height=low+step*rise;EdCell below,above,tread;
                if(!cellclip(&remaining,edV(0,0,-1),-(height-thickness),&below))return 0;append(below);
                if(!cellclip(&remaining,edV(0,0,1),height,&above))return 0;
                EdVec points[3];for(int k=0;k<3;k++){points[k]=corners[ids[k]];points[k].z=height+(ids[k]>=2?rise:0);}
                EdVec ramp=unit(edCross(edSub(points[1],points[0]),edSub(points[2],points[0])));if(ramp.z<0)ramp=edScale(ramp,-1);double rampD=edDot(ramp,points[0]);
                if(!cellclip(&above,edScale(ramp,-1),-rampD,&tread)||!cellclip(&above,ramp,rampD,&remaining))return 0;
                append(tread);ramps.push_back({ramp,rampD,column});
            }
            append(remaining);
        }
    }
    work.c[sector]=pieces[0];for(size_t i=1;i<pieces.size();i++)work.c[work.count++]=pieces[i];
    // Preserve authored boundary properties; newly cut caps use the floor material.
    for(int s=0;s<work.count;s++)for(int f=0;f<work.c[s].count;f++){EdFace &face=work.c[s].f[f];face.attributes=edSurfaceAttributes(&face);face.hasAttributes=1;face.neighbor=face.mirror=-1;}
    if(!edResolve(&work)||!edValidate(&work))return 0;
    // Ramp adjoins stay invisible but are solid floors on both sides.
    for(int c=0;c<work.count;c++)if(c==sector||c>=source->count)for(int f=0;f<work.c[c].count;f++){
        EdFace &face=work.c[c].f[f];if(face.neighbor<0)continue;
        for(const auto &r:ramps){if(fabs(edDot(edNormal(&face),r.n))<.99999)continue;int matches=1;
            for(int v=0;v<face.count;v++)if(fabs(edDot(r.n,face.v[v].p)-r.d)>ED_EPS*4||!edContains(&r.column,face.v[v].p)){matches=0;break;}
            if(!matches)continue;face.attributes=edSurfaceAttributes(&face);face.hasAttributes=1;face.attributes.surfaceFlags|=5u;face.attributes.geometryMode=0;face.attributes.adjoinFlags=(face.attributes.adjoinFlags|5u)&~2u;break;
        }
    }
    for(int i=0;i<work.spawnCount+work.objectCount;i++){
        EdVec &p=i<work.spawnCount?work.spawns[i].position:work.objects[i-work.spawnCount].position;
        if(!edContains(&room,p))continue;int found=0;for(int s=0;s<work.count;s++)if(edContains(&work.c[s],p)){found=1;break;}
        if(!found)p=edAdd(center,edV(radius*.65*cos(rotation+angle*.5),radius*.65*sin(rotation+angle*.5),.12));
    }
    if(work.spawnCount)work.spawn=work.spawns[0].position;
    for(int i=0;i<work.cogCount;i++)for(int j=0;j<work.cogs[i].count;j++){const EdCogParam &p=source->cogs[i].params[j];if(p.type==ED_COG_SURFACE&&p.cell>=0&&p.cell<source->count&&p.face>=0&&p.face<source->c[p.cell].count)remapCogSurface(&work.cogs[i].params[j],&source->c[p.cell].f[p.face],&work);}
    edMapWorld(&work);*out=std::move(work);return 1;
}
