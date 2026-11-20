#include "edWindScript.h"
// Reuse assignments rather than leaving disabled scripts after spline rebuilds.
static void windPrune(EdModel *m){
    for(int i=m->cogCount-1;i>=0;i--)if(!strcmp(m->cogs[i].file,"gs_wind.cog")&&m->cogs[i].params[0].cell<0){
        for(int c=0;c<m->cogCount;c++)for(int p=0;p<m->cogs[c].count;p++){auto &v=m->cogs[c].params[p];if(v.type==ED_COG_COG){int id=atoi(v.value);if(id>=i)snprintf(v.value,32,"%d",id==i?-1:id-1);}}for(int j=i+1;j<m->cogCount;j++)m->cogs[j-1]=m->cogs[j];m->cogCount--;
    }
}
int edSetWind(EdModel *m,int sector,EdVec thrust){
    if(sector<0||sector>=m->count||!isfinite(thrust.x)||!isfinite(thrust.y)||!isfinite(thrust.z)||length(thrust)>100)return 0;
    int slot=-1;for(int i=0;i<m->cogCount;i++)if(!strcmp(m->cogs[i].file,"gs_wind.cog")&&m->cogs[i].params[0].cell==sector){slot=i;break;}
    if(slot<0){if(m->cogCount>=ED_COGS)return 0;slot=m->cogCount;}
    EdPlacedCog cog={};if(!edCogParseScript(&cog,"gs_wind.cog",edWindScript))return 0;
    cog.params[0].cell=sector;cog.params[0].face=-1;
    snprintf(cog.params[1].value,32,"(%.5g/%.5g/%.5g)",thrust.x,thrust.y,thrust.z);
    m->cogs[slot]=cog;if(slot==m->cogCount)m->cogCount++;
    m->c[sector].thrust=thrust;if(length(thrust)>1e-8)m->c[sector].flags|=8u;else m->c[sector].flags&=~8u;
    return 1;
}
