/* Named snapshots are independent of live sector indices. */
int edGroupDelete(EdModel *m,int id){
    if(id<1||id>m->groupCount)return 0;
    // Keep placed geometry. Splines using the removed snapshot become sectors.
    for(int i=m->splineCount-1;i>=0;i--)if(m->splines[i].groupId==id)edSplineConvert(m,m->splines[i].id);
    for(int i=0;i<m->splineCount;i++)if(m->splines[i].groupId>id)m->splines[i].groupId--;
    for(int c=0;c<m->count;c++){int &owner=m->c[c].arrayId;if(owner==id)owner=0;else if(owner>id)owner--;}
    for(int i=id;i<m->groupCount;i++)m->groups[i-1]=std::move(m->groups[i]);
    m->groups.resize(--m->groupCount);return 1;
}
EdVec edGroupAxis(int direction){EdVec result={};double *axis=&result.x;axis[direction/2]=direction%2?-1:1;return result;}
int edGroupBounds(const EdSectorGroup *group,EdVec *lo,EdVec *hi){
    *lo=edV(1e30,1e30,1e30);*hi=edV(-1e30,-1e30,-1e30);int vertices=0;
    for(int c=0;c<group->count;c++)for(int f=0;f<group->cells[c].count;f++)for(int v=0;v<group->cells[c].f[f].count;v++){
        EdVec p=group->cells[c].f[f].v[v].p;lo->x=fmin(lo->x,p.x);lo->y=fmin(lo->y,p.y);lo->z=fmin(lo->z,p.z);hi->x=fmax(hi->x,p.x);hi->y=fmax(hi->y,p.y);hi->z=fmax(hi->z,p.z);vertices++;
    }return vertices>0;
}
int edGroupsValidate(const EdModel *m){
    if(m->groupCount<0||m->groupCount>256)return 0;
    for(int c=0;c<m->count;c++)if(m->c[c].arrayId<0||m->c[c].arrayId>m->groupCount)return 0;
    for(int i=0;i<m->groupCount;i++){
        const EdSectorGroup &g=m->groups[i];if(!memchr(g.name,0,64)||!g.name[0]||g.direction<0||g.direction>5||g.copies<0||g.copies>256||g.count<1)return 0;
        for(const char *p=g.name;*p;p++)if((unsigned char)*p<32||*p=='"')return 0;
        EdModel geometry={};geometry.count=g.count;geometry.c=g.cells;geometry.decalCount=m->decalCount;geometry.decals=m->decals;
        for(int c=0;c<g.count;c++)if(g.cells[c].splineId||g.cells[c].arrayId)return 0;
        if(!edValidate(&geometry))return 0;
    }return 1;
}
int edGroupCapture(EdModel *m,const EdBuffer<unsigned char>& selected,const char *name){
    if(m->groupCount>=256||!name||!name[0]||strlen(name)>=64)return 0;
    EdSectorGroup group;snprintf(group.name,64,"%s",name);
    for(int c=0;c<m->count;c++)if(selected[c]){
        EdCell cell=m->c[c];cell.splineId=cell.arrayId=0;
        for(int f=0;f<cell.count;f++){EdFace &face=cell.f[f];face.attributes=edSurfaceAttributes(&face);face.hasAttributes=1;face.neighbor=face.mirror=-1;face.customUV=1;}
        group.cells[group.count++]=std::move(cell);
    }
    if(!group.count)return 0;m->groups[m->groupCount++]=std::move(group);return edGroupsValidate(m);
}
static void groupDetach(EdModel *m){for(int c=0;c<m->count;c++)for(int f=0;f<m->c[c].count;f++){EdFace &face=m->c[c].f[f];face.attributes=edSurfaceAttributes(&face);face.hasAttributes=1;face.neighbor=face.mirror=-1;}}
int edGroupArray(EdModel *out,const EdModel *source,int groupId,int direction,int copies){
    if(groupId<1||groupId>source->groupCount||direction<0||direction>5||copies<0||copies>256)return 0;
    EdModel work=*source;EdSectorGroup &group=work.groups[groupId-1];group.direction=direction;group.copies=copies;EdVec lo,hi;if(!edGroupBounds(&group,&lo,&hi))return 0;
    EdVec axis=edGroupAxis(direction);double spacing=fabs(edDot(edSub(hi,lo),axis));if(spacing<ED_EPS)return 0;
    EdBuffer<int> remap;int count=0;
    for(int c=0;c<work.count;c++){remap[c]=work.c[c].arrayId==groupId?-1:count;if(remap[c]>=0)work.c[count++]=work.c[c];}
    for(int c=0;c<work.cogCount;c++)for(int p=0;p<work.cogs[c].count;p++){EdCogParam &v=work.cogs[c].params[p];if((v.type==ED_COG_SECTOR||v.type==ED_COG_SURFACE)&&v.cell>=0&&v.cell<work.count){v.cell=remap[v.cell];if(v.cell<0)v.face=-1;}}
    work.count=count;work.c.resize(count);if(copies&&(size_t)group.count*copies>(size_t)ED_CELLS-work.count)return 0;
    for(int copy=1;copy<=copies;copy++)for(int c=0;c<group.count;c++){
        EdCell cell=group.cells[c];cell.arrayId=groupId;EdVec delta=edScale(axis,spacing*copy);
        for(int f=0;f<cell.count;f++)for(int v=0;v<cell.f[f].count;v++)cell.f[f].v[v].p=edAdd(cell.f[f].v[v].p,delta);
        work.c[work.count++]=std::move(cell);
    }
    groupDetach(&work);if(!edResolve(&work)||!edValidate(&work))return 0;*out=std::move(work);return 1;
}
