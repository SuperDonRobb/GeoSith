#include <string>
/* Standard 3DO 2.1 output plus versioned editable metadata. */
#include "edWire.h"
int edReadMeshData(EdModel *model,const unsigned char *bytes,unsigned int size){
    if(size<8||edCompare(bytes,"G3DO",4)||edNetLoad32(bytes+4)<1||edNetLoad32(bytes+4)>7)return 0;
    unsigned version=edNetLoad32(bytes+4);
    for(int legacy=0;legacy<=(version<5?1:0);legacy++){
        EdModel draft={};EdWire w={(unsigned char*)bytes,size,8,1,1,legacy};
        if(version>=3)edWireAssets(&w,&draft);else{edWireAssetsLegacy(&w,&draft);for(int a=0;w.ok&&a<draft.assetCount;a++)draft.assets[a].saved=1;}
        if(version>=2)edWireTemplates(&w,&draft);if(version>=4)edWireMeshRotations(&w,&draft);if(version>=5)edWireTerrain(&w,&draft);if(version>=6)edWireTerrainGroups(&w,&draft);if(version>=7)edWirePaint(&w,&draft);
        if(w.ok&&w.at==size&&edMeshValidate(&draft)){model->templateCount=draft.templateCount;model->templates=draft.templates;model->assetCount=draft.assetCount;model->meshCount=draft.meshCount;model->assets=draft.assets;model->meshes=draft.meshes;return 1;}
    }return 0;
}
static int meshMaterialName(const char *resources,int id,char name[128]){
    const char *p=strstr(resources,"World materials");if(!p)p=strstr(resources,"world materials");if(!p)return 0;
    p=strchr(p,'\n');if(!p)return 0;p++;
    while(*p&&strncmp(p,"end",3)){int index;char value[128];if(sscanf(p," %d: %127s",&index,value)==2&&index==id){strcpy(name,value);return 1;}p=strchr(p,'\n');if(!p)break;p++;}return 0;
}
static int mesh3do(FILE *out,const EdModel *m,int asset,const char *resources,const EdPaintBake *bake=NULL){
    if(asset<0||asset>=m->assetCount||!edMeshValidate(m))return 0;
    EdBuffer<int> ids;int materials=0,meshes=0;double radius=.001;
    for(int i=0;i<m->meshCount;i++)if(m->meshes[i].asset==asset){meshes++;const EdCell *c=&m->meshes[i].geometry;
        for(int f=0;f<c->count;f++){int id=c->f[f].decalId?-c->f[f].decalId:c->f[f].material,j;for(j=0;j<materials&&ids[j]!=id;j++);if(j==materials)ids[materials++]=id;
            for(int v=0;v<c->f[f].count;v++){EdVec p=edSub(c->f[f].v[v].p,m->assets[asset].origin);radius=fmax(radius,sqrt(edDot(p,p)));}}}
    if(!meshes)return 0;
    fprintf(out,"SECTION: HEADER\n3DO 2.1\nSECTION: MODELRESOURCE\nMATERIALS %d\n",bake?1:materials);
    if(bake){char name[32];edPaintFilename(m,asset,name);fprintf(out,"0: %s\n",name);}
    else for(int i=0;i<materials;i++){char name[128];if(ids[i]<0)snprintf(name,sizeof(name),"%s",m->decals[-ids[i]-1].name);else if(!meshMaterialName(resources,ids[i],name))return 0;fprintf(out,"%d: %s\n",i,name);}
    fprintf(out,"SECTION: GEOMETRYDEF\nRADIUS %.9f\nINSERT OFFSET 0 0 0\nGEOSETS 1\nGEOSET 0\nMESHES %d\n",radius,meshes);
    int index=0,bakeVertex=0;for(int i=0;i<m->meshCount;i++)if(m->meshes[i].asset==asset){const EdCell *c=&m->meshes[i].geometry;int count=0;for(int f=0;f<c->count;f++)count+=c->f[f].count;
        fprintf(out,"MESH %d\nNAME mesh%d\nRADIUS %.9f\nGEOMETRYMODE 4\nLIGHTINGMODE 3\nTEXTUREMODE 1\nVERTICES %d\n",index,index,radius,count);index++;
        int vnum=0;for(int f=0;f<c->count;f++)for(int v=0;v<c->f[f].count;v++){EdVec p=edSub(c->f[f].v[v].p,m->assets[asset].origin);fprintf(out,"%d: %.9f %.9f %.9f 0\n",vnum++,p.x,p.y,p.z);}
        fprintf(out,"TEXTURE VERTICES %d\n",count);vnum=0;for(int f=0;f<c->count;f++)for(int v=0;v<c->f[f].count;v++){EdVec uv=bake?bake->uv[bakeVertex++]:edV(c->f[f].v[v].u,c->f[f].v[v].v,0);fprintf(out,"%d: %.9f %.9f\n",vnum++,uv.x,uv.y);}
        fputs("VERTEX NORMALS\n",out);vnum=0;for(int f=0;f<c->count;f++){EdVec n=edNormal(&c->f[f]);for(int v=0;v<c->f[f].count;v++)fprintf(out,"%d: %.9f %.9f %.9f\n",vnum++,n.x,n.y,n.z);}
        fprintf(out,"FACES %d\n",c->count);vnum=0;for(int f=0;f<c->count;f++){const EdFace *face=&c->f[f];int mat=0;while(!bake&&mat<materials&&ids[mat]!=(face->decalId?-face->decalId:face->material))mat++;fprintf(out,"%d: %d 0x0 4 3 1 0 %d",f,mat,face->count);for(int v=0;v<face->count;v++,vnum++)fprintf(out," %d,%d",vnum,vnum);fputc('\n',out);}
        fputs("FACE NORMALS\n",out);for(int f=0;f<c->count;f++){EdVec n=edNormal(&c->f[f]);fprintf(out,"%d: %.9f %.9f %.9f\n",f,n.x,n.y,n.z);}
    }
    fprintf(out,"SECTION: HIERARCHYDEF\nHIERARCHY NODES %d\n",meshes);
    for(int i=0;i<meshes;i++)fprintf(out,"%d: 0x0 0x0 %d %d %d %d %d 0 0 0 0 0 0 0 0 0 mesh%d\n",i,i,i?0:-1,i==0&&meshes>1?1:-1,i>0&&i+1<meshes?i+1:-1,i==0?meshes-1:0,i);
    return !ferror(out);
}
int edWrite3do(const EdModel *m,int asset,const char *path,const char *resources){FILE *f=fopen(path,"wb");if(!f)return 0;EdPaintBake bake;int painted=edPainted(m,asset);int ok=(!painted||edPaintBakeAsset(m,asset,&bake))&&mesh3do(f,m,asset,resources,painted?&bake:NULL);if(ok&&painted){char name[32];edPaintFilename(m,asset,name);std::string destination=path;size_t slash=destination.find_last_of("/\\");destination=slash==std::string::npos?name:destination.substr(0,slash+1)+name;FILE *mat=fopen(destination.c_str(),"wb");ok=mat&&fwrite(bake.mat.data(),1,bake.mat.size(),mat)==bake.mat.size();if(mat&&fclose(mat))ok=0;}if(ok)for(int d=0;d<m->decalCount;d++){std::string destination=path;size_t slash=destination.find_last_of("/\\");destination=slash==std::string::npos?m->decals[d].name:destination.substr(0,slash+1)+m->decals[d].name;FILE *mat=fopen(destination.c_str(),"wb");const auto &bytes=*m->decals[d].data;ok=mat&&fwrite(bytes.data(),1,bytes.size(),mat)==bytes.size();if(mat&&fclose(mat))ok=0;if(!ok)break;}if(fclose(f))ok=0;if(!ok)remove(path);return ok;}
static int writeMeshAssets(const EdModel *m,const char *path,const char *resources){
    if(!m->assetCount&&!m->templateCount)return 1;EdWire measure={NULL,ED_NET_LIMIT/2,8,0,1};edWireAssets(&measure,(EdModel*)m);edWireTemplates(&measure,(EdModel*)m);edWireMeshRotations(&measure,(EdModel*)m);edWireTerrain(&measure,(EdModel*)m);edWireTerrainGroups(&measure,(EdModel*)m);edWirePaint(&measure,(EdModel*)m);if(!measure.ok)return 0;unsigned char *bytes=edAlloc(measure.at);if(!bytes)return 0;edCopy(bytes,"G3DO",4);edNetStore32(bytes+4,7);EdWire w={bytes,measure.at,8,0,1};edWireAssets(&w,(EdModel*)m);edWireTemplates(&w,(EdModel*)m);edWireMeshRotations(&w,(EdModel*)m);edWireTerrain(&w,(EdModel*)m);edWireTerrainGroups(&w,(EdModel*)m);edWirePaint(&w,(EdModel*)m);
    int ok=w.ok&&edGobAddFile(path,"misc\\geosith_models.bin",bytes,(unsigned int)w.at);edFree(bytes);
    for(int a=0;a<m->assetCount&&ok;a++){if(!m->assets[a].saved)continue;int count=0;for(int i=0;i<m->meshCount;i++)count+=m->meshes[i].asset==a;if(!count)continue;
        EdPaintBake bake;int painted=edPainted(m,a);if(painted){if(!edPaintBakeAsset(m,a,&bake))return 0;char file[32],entry[64];edPaintFilename(m,a,file);snprintf(entry,sizeof(entry),"mat\\%s",file);if(!edGobAddFile(path,entry,bake.mat.data(),(unsigned)bake.mat.size()))return 0;}
        FILE *f=tmpfile();if(!f)return 0;ok=mesh3do(f,m,a,resources,painted?&bake:NULL);long size=ftell(f);unsigned char *data=size>0?(unsigned char*)edAlloc(size):NULL;
        if(ok&&data&&!fseek(f,0,SEEK_SET)&&fread(data,1,size,f)==size){char name[128];snprintf(name,sizeof(name),"3do\\%s.3do",m->assets[a].name);ok=edGobAddFile(path,name,data,(unsigned int)size);}else ok=0;edFree(data);fclose(f);
    }return ok;
}


int edReadGroupData(EdModel *model,const unsigned char *bytes,unsigned int size){
    if(size<12||memcmp(bytes,"GGRP",4)||edNetLoad32(bytes+4)!=1||edNetLoad32(bytes+8)!=(unsigned)model->count)return 0;
    EdModel draft=*model;EdWire w={(unsigned char*)bytes,size,12,1,1};edWireGroups(&w,&draft);
    if(!w.ok||w.at!=size||!edGroupsValidate(&draft))return 0;
    model->groupCount=draft.groupCount;model->groups=std::move(draft.groups);for(int c=0;c<model->count;c++)model->c[c].arrayId=draft.c[c].arrayId;return 1;
}
static int writeGroups(const EdModel *model,const char *path){
    if(!model->groupCount)return 1;
    EdWire measure={NULL,ED_NET_LIMIT/2,12,0,1};edWireGroups(&measure,(EdModel*)model);if(!measure.ok)return 0;
    std::vector<unsigned char> bytes(measure.at);memcpy(bytes.data(),"GGRP",4);edNetStore32(bytes.data()+4,1);edNetStore32(bytes.data()+8,model->count);
    EdWire w={bytes.data(),bytes.size(),12,0,1};edWireGroups(&w,(EdModel*)model);
    return w.ok&&edGobAddFile(path,"misc\\geosith_groups.bin",bytes.data(),(unsigned)bytes.size());
}
