/* Voronoi fracture of convex sector volumes. Included after geometry helpers.
 * The document transaction owns all generated assets and COG references. */
#include "edDestructibleScript.h"
static EdVec destructibleCenter(const EdCell &cell){
    EdVec center={};int count=0;for(int f=0;f<cell.count;f++)for(int v=0;v<cell.f[f].count;v++){center=edAdd(center,cell.f[f].v[v].p);count++;}
    return edScale(center,1.0/count);
}
static void destructibleParam(EdTemplate &t,const char *key,const char *value){
    EdTemplateParam &p=t.params[t.count++];snprintf(p.key,sizeof(p.key),"%s",key);snprintf(p.value,sizeof(p.value),"%s",value);
}
static int destructibleAsset(EdModel *m,const char *name,EdVec origin){
    int id=m->assetCount++;EdAsset &a=m->assets[id];a={};snprintf(a.name,sizeof(a.name),"%s",name);a.saved=1;a.origin=origin;return id;
}
static void destructibleMesh(EdModel *m,int asset,const EdCell &cell,const EdBuffer<unsigned char> *selected=NULL){
    EdMesh mesh={};mesh.asset=asset;
    for(int f=0;f<cell.count;f++){
        EdFace face=cell.f[f];if(selected&&face.neighbor>=0&&(*selected)[face.neighbor])continue;
        face.neighbor=face.mirror=-1;reverse(&face);face.hasAttributes=0;
        mesh.geometry.f[mesh.geometry.count++]=face;
    }
    if(mesh.geometry.count)m->meshes[m->meshCount++]=std::move(mesh);
}
int edGenerateDestructible(EdModel *m,const EdBuffer<unsigned char>& selected,const char *name){
    auto fail=[](int line){if(getenv("GEOSITH_DESTRUCTIBLE_DIAG"))fprintf(stderr,"Destructible failure line %d\n",line);return 0;};
    std::vector<int> cells;for(int c=0;c<m->count;c++)if(selected[c]){if(m->c[c].splineId)return fail(__LINE__);cells.push_back(c);}
    if(cells.empty()||cells.size()>32||strlen(name)>20||!templateToken(name,21)||m->objectCount>=ED_OBJECTS||m->cogCount>=ED_COGS)return fail(__LINE__);
    int pieces=(std::max)(12,(int)cells.size());
    if(m->templateCount+pieces+2>ED_CUSTOM_TEMPLATES)return fail(__LINE__);
    char names[34][32];snprintf(names[0],32,"%s",name);snprintf(names[1],32,"%s_fx",name);
    for(int i=0;i<pieces;i++)snprintf(names[i+2],32,"%s_p%02d",name,i);
    for(int i=0;i<pieces+2;i++){
        if(edTemplateIndex(m,names[i])>=0)return fail(__LINE__);
        for(int a=0;a<m->assetCount;a++)if(!strcmp(m->assets[a].name,names[i]))return fail(__LINE__);
    }
    EdModel work=*m;EdVec origin=destructibleCenter(m->c[cells[0]]);
    int wall=destructibleAsset(&work,name,origin);
    for(int c:cells)destructibleMesh(&work,wall,m->c[c],&selected);
    EdTemplate t={};strcpy(t.name,name);strcpy(t.parent,"_walkstruct");char text[128];snprintf(text,sizeof(text),"%s.3do",name);destructibleParam(t,"model3d",text);
    destructibleParam(t,"thingflags","0x48");work.templates[work.templateCount++]=t;
    t={};strcpy(t.name,names[1]);strcpy(t.parent,"_explosion");
    destructibleParam(t,"sprite","rldx.spr");destructibleParam(t,"soundclass","exp_raildet.snd");destructibleParam(t,"timer","0.5");destructibleParam(t,"typeflags","0x0");destructibleParam(t,"damage","0");destructibleParam(t,"force","0");work.templates[work.templateCount++]=t;
    EdObject object={};strcpy(object.templateName,name);object.position=origin;int thing=work.spawnCount+work.objectCount;work.objects[work.objectCount++]=object;
    EdPlacedCog cog={};if(!edCogParseScript(&cog,"gs_destructible.cog",edDestructibleScript))return fail(__LINE__);
    snprintf(cog.params[0].value,32,"%d",thing);snprintf(cog.params[1].value,32,"%d",pieces);strcpy(cog.params[2].value,names[1]);
    std::vector<EdVec> seeds;std::vector<int> rooms;unsigned rng=0x524f434b;
    for(int i=0;i<pieces;i++){
        int c=cells[i%cells.size()];EdVec center=destructibleCenter(m->c[c]),seed=center;
        if(i>=(int)cells.size()){
            rng=rng*1664525u+1013904223u;const EdFace &face=m->c[c].f[(rng>>8)%m->c[c].count];
            rng=rng*1664525u+1013904223u;EdVec corner=face.v[(rng>>8)%face.count].p;
            double fraction=.25+.5*((rng&65535)/65535.0);seed=edAdd(center,edScale(edSub(corner,center),fraction));
        }
        seeds.push_back(seed);rooms.push_back(c);
    }
    for(int i=0;i<pieces;i++){
        int asset=destructibleAsset(&work,names[i+2],seeds[i]),start=work.meshCount;
        for(int c:cells){
            EdCell part=m->c[c];
            for(int j=0;j<pieces&&part.count;j++)if(i!=j){
                EdVec n=edSub(seeds[i],seeds[j]);double len=length(n);if(len<ED_EPS)return fail(__LINE__);n=edScale(n,1/len);
                EdCell clipped;if(!cellclip(&part,n,edDot(n,edScale(edAdd(seeds[i],seeds[j]),.5)),&clipped))return fail(__LINE__);part=std::move(clipped);
            }
            if(part.count){for(int f=0;f<part.count;f++)edMapFace(&part.f[f]);destructibleMesh(&work,asset,part);}
        }
        if(work.meshCount==start)return fail(__LINE__);
        t={};strcpy(t.name,names[i+2]);strcpy(t.parent,"_throwable");snprintf(text,sizeof(text),"%s.3do",t.name);destructibleParam(t,"model3d",text);
        destructibleParam(t,"timer","4");destructibleParam(t,"size","0.02");destructibleParam(t,"movesize","0.01");destructibleParam(t,"mass","10");destructibleParam(t,"vel","(0/0/0)");work.templates[work.templateCount++]=t;
        strcpy(cog.params[3+i].value,t.name);EdVec offset=edSub(seeds[i],origin);
        // A COG vector has only 31 text bytes. Tiny cancellation residues and
        // scientific notation can overflow it even for a small valid sector.
        // Bound each component to nine characters (3*9 + four delimiters).
        char components[3][32];double values[3]={offset.x,offset.y,offset.z};
        for(int axis=0;axis<3;axis++){
            if(fabs(values[axis])<1e-10)values[axis]=0;
            for(int precision=6;precision>=1;precision--){snprintf(components[axis],32,"%.*g",precision,values[axis]);if(strlen(components[axis])<=9)break;}
        }
        snprintf(text,sizeof(text),"(%s/%s/%s)",components[0],components[1],components[2]);if(!edCogSetValue(&work,&cog.params[35+i],text))return fail(__LINE__);
        snprintf(text,sizeof(text),"%d",rooms[i]);if(!edCogSetValue(&work,&cog.params[67+i],text))return fail(__LINE__);
    }
    work.cogs[work.cogCount++]=cog;
    // Passage through an existing adjoin is blocked by the new model, not by
    // a second invisible sector wall that would survive its destruction.
    for(int c:cells)for(int f=0;f<work.c[c].count;f++){
        EdFace &face=work.c[c].f[f];if(face.neighbor<0)continue;
        EdFace &other=work.c[face.neighbor].f[face.mirror];
        for(EdFace *p:{&face,&other}){p->attributes=edSurfaceAttributes(p);p->hasAttributes=1;p->attributes.geometryMode=0;p->attributes.surfaceFlags&=~4u;p->attributes.adjoinFlags|=3u;}
    }
    if(!edValidate(&work))return fail(__LINE__);*m=std::move(work);return 1;
}

int edGenerateLift(EdModel *m,int sector,const char *name,const EdPlacedCog &script,int startAtTop){
    if(sector<0||sector>=m->count||m->c[sector].splineId||!templateToken(name,21)
        ||m->templateCount>=ED_CUSTOM_TEMPLATES||m->objectCount>=ED_OBJECTS||m->cogCount>=ED_COGS
        ||edTemplateIndex(m,name)>=0)return 0;
    for(int a=0;a<m->assetCount;a++)if(!strcmp(m->assets[a].name,name))return 0;
    // The walking surface starts one step above the selected sector's
    // lower boundary. The solid model extends into the shaft beneath it.
    const EdCell &cell=m->c[sector];EdVec origin=destructibleCenter(cell);
    double low=1e30,high=-1e30;
    for(int f=0;f<cell.count;f++)for(int v=0;v<cell.f[f].count;v++){
        double z=cell.f[f].v[v].p.z;low=fmin(low,z);high=fmax(high,z);
    }
    double shift=low+.03125-high;
    if(!isfinite(shift)||shift>=-ED_EPS)return 0;
    EdVec bottom=origin;bottom.z+=shift;int inside=0;
    for(int s=0;s<m->count;s++)if(edContains(&m->c[s],bottom)){inside=1;break;}if(!inside)return 0;
    EdModel work=*m;int asset=destructibleAsset(&work,name,origin);destructibleMesh(&work,asset,cell);
    EdTemplate t={};strcpy(t.name,name);strcpy(t.parent,"_walkstruct");char text[128];snprintf(text,sizeof(text),"%s.3do",name);
    destructibleParam(t,"model3d",text);destructibleParam(t,"move","path");destructibleParam(t,"soundclass","sm_elev.snd");work.templates[work.templateCount++]=t;
    EdObject object={};strcpy(object.templateName,name);object.position=startAtTop?origin:bottom;object.attributes.pathOverride=1;object.attributes.frameCount=2;
    object.attributes.frames[0].position=object.position;object.attributes.frames[1].position=startAtTop?bottom:origin;
    int thing=work.spawnCount+work.objectCount;work.objects[work.objectCount++]=object;
    EdPlacedCog cog=script;int assigned=0;
    for(int p=0;p<cog.count;p++){
        EdCogParam &param=cog.params[p];
        if(!strcmp(param.name,"elevator")&&param.type==ED_COG_THING){snprintf(param.value,32,"%d",thing);assigned|=1;}
        else if(!strcmp(param.name,"topSector")&&param.type==ED_COG_SECTOR){strcpy(param.value,"-1");param.cell=-1;assigned|=2;}
    }
    for(int p=0;p<cog.count;p++)if(!strcmp(cog.params[p].name,"speed"))strcpy(cog.params[p].value,"8");
    if(assigned!=3)return 0;work.cogs[work.cogCount++]=cog;
    for(int f=0;f<work.c[sector].count;f++){
        EdFace &face=work.c[sector].f[f];if(face.neighbor<0)continue;
        EdFace &other=work.c[face.neighbor].f[face.mirror];
        for(EdFace *p:{&face,&other}){p->attributes=edSurfaceAttributes(p);p->hasAttributes=1;p->attributes.geometryMode=0;p->attributes.surfaceFlags&=~4u;p->attributes.adjoinFlags|=3u;}
    }
    if(!edValidate(&work))return 0;*m=std::move(work);return 1;
}
