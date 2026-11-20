/* Stock COGs keep generated belts playable outside the editor. */
int edGenerateConveyors(EdModel *m,const EdSelection &selection,const EdVec *directions,const EdPlacedCog &script,double speed){
    if(!directions||selection.count<1||selection.count>ED_SELECTIONS||strcmp(script.file,"00_conveyor.cog")||!isfinite(speed)||speed<=0)return 0;
    EdModel work=*m;
    for(int i=0;i<selection.count;i++){
        EdHit h=selection.hits[i];if(h.cell<0||h.cell>=work.count||h.face<0||h.face>=work.c[h.cell].count||h.vertex>=0||h.edge>=0||work.c[h.cell].splineId)return 0;
        for(int j=0;j<i;j++)if(selection.hits[j].cell==h.cell&&selection.hits[j].face==h.face)return 0;
        EdVec n=edNormal(&work.c[h.cell].f[h.face]),d=directions[i];d=edSub(d,edScale(n,edDot(d,n)));
        double len=length(d);if(!isfinite(len)||len<ED_EPS)return 0;d=edScale(d,1/len);
        int slot=-1;
        for(int c=0;c<work.cogCount;c++)if(!strcmp(work.cogs[c].file,"00_conveyor.cog")){
            int refs=0,match=0;for(int p=0;p<work.cogs[c].count;p++){const auto &v=work.cogs[c].params[p];if(v.type==ED_COG_SURFACE&&v.cell>=0){refs++;match|=v.cell==h.cell&&v.face==h.face;}}
            if(match){if(refs!=1||slot>=0)return 0;slot=c;}
        }
        EdPlacedCog cog=script;int refs=0,vec=0,velocity=0;
        for(int p=0;p<cog.count;p++){
            auto &v=cog.params[p];
            if(v.type==ED_COG_SURFACE){v.cell=refs?-1:h.cell;v.face=refs?-1:h.face;strcpy(v.value,refs?"-1":"0");refs++;}
            else if(!strcmp(v.name,"vec0")&&v.type==ED_COG_VECTOR){snprintf(v.value,32,"(%.4g/%.4g/%.4g)",d.x,d.y,d.z);vec=1;}
            else if(!strcmp(v.name,"speed")&&v.type==ED_COG_FLEX){snprintf(v.value,32,"%.6g",speed);velocity=1;}
        }
        if(!refs||!vec||!velocity)return 0;
        if(slot<0){if(work.cogCount>=ED_COGS)return 0;slot=work.cogCount++;}work.cogs[slot]=cog;
    }
    if(!edValidate(&work))return 0;*m=std::move(work);return 1;
}
