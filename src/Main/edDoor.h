/* Included after the convex cell and generated model helpers. */
#include "edDoorScript.h"
int edDoorSwitchOutline(const EdFace *face,EdVec center,EdFace *outline){
    if(!face||face->neighbor>=0||fabs(edNormal(face).z)>.9)return 0;
    EdVec points[32];if(!edShapePoints(face,center,ED_GRID*2,4,0,points)||!edClipShape(face,center,ED_GRID*2,4,0,outline)||outline->count!=4)return 0;
    // Require the entire square to fit; the preview and committed cut match.
    for(int i=0;i<4;i++){int match=0;for(int j=0;j<4;j++)match|=length(edSub(points[i],outline->v[j].p))<ED_EPS;if(!match)return 0;}
    EdVec u,v;decalBasis(face,&u,&v);center=edSnapFacePoint(face,center);double width=ED_GRID*2*sqrt(2.0);
    for(int i=0;i<4;i++){EdVec d=edSub(outline->v[i].p,center);outline->v[i].u=.5+edDot(d,u)/width;outline->v[i].v=.5-edDot(d,v)/width;}
    return 1;
}
int edDoorSwitch(EdModel *m,int c,int f,EdVec point,int material,int width,int height){
    if(c<0||c>=m->count||f<0||f>=m->c[c].count||material<0||width<=0||height<=0||m->c[c].splineId)return 0;
    EdFace outline;if(!edDoorSwitchOutline(&m->c[c].f[f],point,&outline))return 0;
    EdModel work=*m;if(!edCleaveShape(&work,c,f,point,ED_GRID*2,4))return 0;
    EdFace &face=work.c[c].f[f];if(face.count!=4)return 0;
    face.material=material;face.decalId=0;face.customUV=1;face.uOffset=face.vOffset=face.uvRotation=0;
    face.attributes=edSurfaceAttributes(&face);face.hasAttributes=1;face.attributes.geometryMode=4;face.attributes.textureMode=1;face.attributes.uScale=face.attributes.vScale=1;
    for(int v=0;v<face.count;v++)for(int j=0;j<4;j++)if(length(edSub(face.v[v].p,outline.v[j].p))<ED_EPS){face.v[v].u=outline.v[j].u*width;face.v[v].v=outline.v[j].v*height;}
    if(!edValidate(&work))return 0;*m=std::move(work);return 1;
}
int edGenerateDoor(EdModel *m,int sector,const char *name,int split,EdVec axis,int activation,int key,int switchCell,int switchFace){
    int parts=split?2:1;
    if(sector<0||sector>=m->count||m->c[sector].splineId||!templateToken(name,21)
       ||m->templateCount+parts>ED_CUSTOM_TEMPLATES||m->objectCount+parts>ED_OBJECTS||m->cogCount>=ED_COGS
       ||activation<0||activation>3||key<0||key>2||!isfinite(length(axis))||length(axis)<.99)return 0;
    if(activation==2&&(switchCell<0||switchCell>=m->count||switchFace<0||switchFace>=m->c[switchCell].count))return 0;
    axis=unit(axis);const EdCell &cell=m->c[sector];double lo=1e30,hi=-1e30;
    for(int f=0;f<cell.count;f++)for(int v=0;v<cell.f[f].count;v++){double d=edDot(cell.f[f].v[v].p,axis);lo=fmin(lo,d);hi=fmax(hi,d);}
    if(hi-lo<ED_GRID)return 0;
    EdModel work=*m;int first=work.spawnCount+work.objectCount;double radius=0;EdVec home={};
    for(int part=0;part<parts;part++){
        char id[32];snprintf(id,sizeof(id),"%s%s",name,part?"_b":"");
        if(edTemplateIndex(&work,id)>=0)return 0;
        for(int a=0;a<work.assetCount;a++)if(!strcmp(work.assets[a].name,id))return 0;
        EdCell panel=cell;
        if(split&&!cellclip(&cell,edScale(axis,part?-1:1),(lo+hi)*.5*(part?-1:1),&panel))return 0;
        if(!panel.count)return 0;
        // The cap's corners come from different source faces, so their
        // interpolated UVs do not form a coherent mapping on the new plane.
        // Map only the new seam in world space; retain the outer faces' UVs.
        if(split)for(int f=0;f<panel.count;f++){
            EdFace &face=panel.f[f];bool seam=true;
            for(int v=0;v<face.count;v++)seam=seam&&fabs(edDot(face.v[v].p,axis)-(lo+hi)*.5)<ED_EPS*2;
            if(seam){face.customUV=0;face.decalId=0;face.uOffset=face.vOffset=face.uvRotation=0;edMapFace(&face);}
        }
        EdVec origin=destructibleCenter(panel);if(!part)home=origin;
        for(int f=0;f<cell.count;f++)for(int v=0;v<cell.f[f].count;v++)radius=fmax(radius,length(edSub(cell.f[f].v[v].p,home)));
        int asset=destructibleAsset(&work,id,origin);destructibleMesh(&work,asset,panel);
        EdTemplate t={};strcpy(t.name,id);strcpy(t.parent,"_walkstruct");char text[64];snprintf(text,sizeof(text),"%s.3do",id);
        destructibleParam(t,"model3d",text);destructibleParam(t,"move","path");destructibleParam(t,"soundclass","sm_door.snd");work.templates[work.templateCount++]=t;
        EdObject object={};strcpy(object.templateName,id);object.position=origin;object.attributes.pathOverride=1;object.attributes.frameCount=2;
        object.attributes.frames[0].position=origin;
        object.attributes.frames[1].position=edAdd(origin,edScale(axis,(hi-lo)/parts*(part?-1:1)));
        work.objects[work.objectCount++]=object;
    }
    EdPlacedCog cog={};if(!edCogParseScript(&cog,"gs_door.cog",edDoorScript))return 0;
    for(int p=0;p<cog.count;p++){
        EdCogParam &param=cog.params[p];
        if(!strcmp(param.name,"door0"))snprintf(param.value,32,"%d",first);
        else if(!strcmp(param.name,"door1"))snprintf(param.value,32,"%d",split?first+1:-1);
        else if(!strcmp(param.name,"activation"))snprintf(param.value,32,"%d",activation);
        else if(!strcmp(param.name,"keybin"))snprintf(param.value,32,"%d",46+key);
        else if(!strcmp(param.name,"radius"))snprintf(param.value,32,"%.6g",radius+.25);
        else if(!strcmp(param.name,"switch0")&&activation==2){param.cell=switchCell;param.face=switchFace;strcpy(param.value,"0");}
    }
    work.cogs[work.cogCount++]=cog;
    for(int f=0;f<work.c[sector].count;f++){
        EdFace &face=work.c[sector].f[f];if(face.neighbor<0)continue;
        EdFace &back=work.c[face.neighbor].f[face.mirror];
        for(EdFace *p:{&face,&back}){p->attributes=edSurfaceAttributes(p);p->hasAttributes=1;p->attributes.geometryMode=0;p->attributes.surfaceFlags&=~4u;p->attributes.adjoinFlags|=3u;}
    }
    if(!edValidate(&work))return 0;*m=std::move(work);return 1;
}
