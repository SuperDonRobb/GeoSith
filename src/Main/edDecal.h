/* Decals become ordinary RGB565 materials. Immutable byte buffers are shared
   by document snapshots; painting never changes an undo snapshot's pixels. */
static int decalMatSize(const EdDecalMaterial &d,int *width,int *height){
    if(!d.data||d.data->size()<142||memcmp(d.data->data(),"MAT ",4)||!memchr(d.name,0,32))return 0;
    const unsigned char *p=d.data->data();*width=edNetLoad32(p+116);*height=edNetLoad32(p+120);
    if(*width<1||*height<1||*width>2048||*height>2048||(*width&(*width-1))||(*height&(*height-1)))return 0;
    const unsigned header[]={50,2,1,1,1,16,5,6,5,11,5,0,3,2,3,0,0,0,8,0,0,0,0,0,0,0,0,0};
    for(int i=0;i<28;i++)if(edNetLoad32(p+4+i*4)!=header[i])return 0;
    return edNetLoad32(p+124)<=1&&edNetLoad32(p+128)==0&&edNetLoad32(p+132)<=65535&&edNetLoad32(p+136)==1&&d.data->size()==140+(size_t)(*width)*(*height)*2;
}
int edDecalsValid(const EdModel *m){
    if(m->decalCount<0||m->decalCount>256)return 0;size_t total=0;
    for(int i=0;i<m->decalCount;i++){int w,h;if(!decalMatSize(m->decals[i],&w,&h)||!paintNameValid(m->decals[i].name))return 0;total+=m->decals[i].data->size();}
    if(total>32*1024*1024)return 0;
    for(int g=0;g<m->groupCount;g++)for(int c=0;c<m->groups[g].count;c++)for(int f=0;f<m->groups[g].cells[c].count;f++)if(m->groups[g].cells[c].f[f].decalId<0||m->groups[g].cells[c].f[f].decalId>m->decalCount)return 0;
    for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++)if(m->c[s].f[f].decalId<0||m->c[s].f[f].decalId>m->decalCount)return 0;
    for(int i=0;i<m->meshCount;i++)for(int f=0;f<m->meshes[i].geometry.count;f++)if(m->meshes[i].geometry.f[f].decalId<0||m->meshes[i].geometry.f[f].decalId>m->decalCount)return 0;
    return 1;
}
int edDecalImage(const EdModel *m,int id,EdPaintImage *image){
    if(id<1||id>m->decalCount)return 0;const EdDecalMaterial &d=m->decals[id-1];int w,h;if(!decalMatSize(d,&w,&h))return 0;
    image->width=w;image->height=h;image->rgba.resize((size_t)w*h*4);const unsigned char *p=d.data->data();unsigned alpha=edNetLoad32(p+124),key=edNetLoad32(p+132);
    for(size_t i=0;i<(size_t)w*h;i++){unsigned n=p[140+i*2]|unsigned(p[141+i*2])<<8;image->rgba[i*4]=(n>>11)*255/31;image->rgba[i*4+1]=((n>>5)&63)*255/63;image->rgba[i*4+2]=(n&31)*255/31;image->rgba[i*4+3]=alpha&&n==key?0:255;}return 1;
}
void edCompactDecals(EdModel *m){
    std::vector<int> map(m->decalCount+1);EdBuffer<EdDecalMaterial> used;int count=0;
    auto remap=[&](EdFace &f){if(f.decalId<1||f.decalId>m->decalCount)return;if(!map[f.decalId]){used[count++]=m->decals[f.decalId-1];map[f.decalId]=count;}f.decalId=map[f.decalId];};
    for(int s=0;s<m->count;s++)for(int f=0;f<m->c[s].count;f++)remap(m->c[s].f[f]);
    for(int i=0;i<m->meshCount;i++)for(int f=0;f<m->meshes[i].geometry.count;f++)remap(m->meshes[i].geometry.f[f]);for(int g=0;g<m->groupCount;g++)for(int c=0;c<m->groups[g].count;c++)for(int f=0;f<m->groups[g].cells[c].count;f++)remap(m->groups[g].cells[c].f[f]);m->decals=std::move(used);m->decalCount=count;
}
static void decalBasis(const EdFace *face,EdVec *u,EdVec *v){EdVec n=edNormal(face);*u=fabs(n.z)>.9?unit(edCross(edV(0,1,0),n)):unit(edCross(edV(0,0,1),n));*v=edCross(n,*u);}
int edDecalOutline(const EdFace *face,EdVec center,double width,double aspect,EdFace *outline){
    if(!face||!isfinite(center.x)||!isfinite(center.y)||!isfinite(center.z)||face->count<3||!isfinite(width)||!isfinite(aspect)||width<=0||aspect<=0)return 0;
    EdVec u,v;decalBasis(face,&u,&v);EdFace polygon={};polygon.count=4;double x[4]={-.5,.5,.5,-.5},y[4]={-.5,-.5,.5,.5};
    for(int i=0;i<4;i++){polygon.v[i].p=edAdd(center,edAdd(edScale(u,x[i]*width),edScale(v,y[i]*width*aspect)));polygon.v[i].u=x[i]+.5;polygon.v[i].v=.5-y[i];}
    EdVec n=edNormal(face);
    for(int e=0;e<face->count;e++){
        EdFace next={};EdVec a=face->v[e].p,plane=edCross(edSub(face->v[(e+1)%face->count].p,a),n);
        for(int i=0;i<polygon.count;i++){EdVertex p=polygon.v[i],q=polygon.v[(i+1)%polygon.count];double dp=edDot(edSub(p.p,a),plane),dq=edDot(edSub(q.p,a),plane);int inside=dp<=1e-9,nextInside=dq<=1e-9;
            if(inside){if(next.count>=ED_VERTS)return 0;next.v[next.count++]=p;}
            if(inside!=nextInside){double t=dp/(dp-dq);EdVertex cut=p;cut.p=edAdd(p.p,edScale(edSub(q.p,p.p),t));cut.u=p.u+(q.u-p.u)*t;cut.v=p.v+(q.v-p.v)*t;if(next.count>=ED_VERTS)return 0;next.v[next.count++]=cut;}
        }polygon=next;if(polygon.count<3)return 0;
    }*outline=polygon;return 1;
}
static void decalSample(const EdPaintImage &im,double u,double v,double *out,int tile){
    double x=u-.5,y=v-.5;int ix=(int)floor(fmod(x,im.width)),iy=(int)floor(fmod(y,im.height));double tx=x-floor(x),ty=y-floor(y);
    for(int k=0;k<4;k++)out[k]=0;
    for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){int px=tile?((ix+dx)%im.width+im.width)%im.width:(std::max)(0,(std::min)(im.width-1,(int)floor(x)+dx));int py=tile?((iy+dy)%im.height+im.height)%im.height:(std::max)(0,(std::min)(im.height-1,(int)floor(y)+dy));double w=(dx?tx:1-tx)*(dy?ty:1-ty);const unsigned char *p=&im.rgba[((size_t)py*im.width+px)*4];for(int k=0;k<4;k++)out[k]+=p[k]*w;}
}
int edPlaceDecal(EdModel *m,EdFace *face,EdVec center,double width,const EdPaintImage *base,const EdPaintImage *decal,double paintStrength){
    auto valid=[](const EdPaintImage *i){return i&&i->width>0&&i->height>0&&i->width<=4096&&i->height<=4096&&i->rgba.size()==(size_t)i->width*i->height*4;};
    if(!isfinite(paintStrength)||paintStrength<0||paintStrength>1||!face||face->count<3||face->count>ED_SURFACE_VERTS||!valid(base)||!valid(decal)||!isfinite(width)||width<ED_GRID/4||width>128)return 0;
    EdFace outline;if(!edDecalOutline(face,center,width,paintStrength?1:(double)decal->height/decal->width,&outline))return 0;
    EdFace tiled=*face;tiled.customUV=0;tiled.uOffset=tiled.vOffset=tiled.uvRotation=0;edMapFace(&tiled);
    EdVec u,v;decalBasis(face,&u,&v);EdVec origin=face->v[0].p;double minU=1e30,maxU=-1e30,minV=1e30,maxV=-1e30;
    for(int i=0;i<face->count;i++){EdVec p=edSub(face->v[i].p,origin);double x=edDot(p,u),y=edDot(p,v);minU=fmin(minU,x);maxU=fmax(maxU,x);minV=fmin(minV,y);maxV=fmax(maxV,y);}if(maxU-minU<ED_EPS||maxV-minV<ED_EPS)return 0;
    int w=16,h=16;while(w<2048&&w<(maxU-minU)*ED_UV_SCALE)w*=2;while(h<2048&&h<(maxV-minV)*ED_UV_SCALE)h*=2;
    auto bytes=std::make_shared<std::vector<unsigned char>>(140+(size_t)w*h*2);bool transparent=false,changed=false;
    int direct=paintStrength&&face->decalId>0&&face->decalId<=m->decalCount;
    if(direct){int oldW,oldH;direct=decalMatSize(m->decals[face->decalId-1],&oldW,&oldH)&&oldW==w&&oldH==h;
        for(int i=0;direct&&i<face->count;i++){EdVec p=edSub(face->v[i].p,origin);double expectedU=(2.5+(edDot(p,u)-minU)/(maxU-minU)*(w-5))*(std::min)(w,256)/w,expectedV=(2.5+(edDot(p,v)-minV)/(maxV-minV)*(h-5))*(std::min)(h,256)/h;direct=fabs(face->v[i].u-expectedU)<1e-7&&fabs(face->v[i].v-expectedV)<1e-7;}}
    int x0=0,y0=0,x1=w,y1=h;
    if(direct){*bytes=*m->decals[face->decalId-1].data;transparent=edNetLoad32(bytes->data()+124)!=0;EdVec d=edSub(center,origin);
        x0=(std::max)(0,(int)floor(2+(edDot(d,u)-width*.5-minU)/(maxU-minU)*(w-5)));x1=(std::min)(w,1+(int)ceil(2+(edDot(d,u)+width*.5-minU)/(maxU-minU)*(w-5)));
        y0=(std::max)(0,(int)floor(2+(edDot(d,v)-width*.5-minV)/(maxV-minV)*(h-5)));y1=(std::min)(h,1+(int)ceil(2+(edDot(d,v)+width*.5-minV)/(maxV-minV)*(h-5)));
    }
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){
        EdVec point=edAdd(origin,edAdd(edScale(u,minU+(x-2.0)/(w-5)*(maxU-minU)),edScale(v,minV+(y-2.0)/(h-5)*(maxV-minV))));double tu=0,tv=0,pu=0,pv=0;
        for(int f=1;f+1<face->count;f++){
            EdVec a=edSub(face->v[f].p,origin),b=edSub(face->v[f+1].p,origin),p=edSub(point,origin);double aa=edDot(a,a),ab=edDot(a,b),bb=edDot(b,b),ap=edDot(a,p),bp=edDot(b,p),det=aa*bb-ab*ab;if(fabs(det)<1e-18)continue;double j=(ap*bb-bp*ab)/det,k=(bp*aa-ap*ab)/det,i=1-j-k;
            tu=face->v[0].u*i+face->v[f].u*j+face->v[f+1].u*k;tv=face->v[0].v*i+face->v[f].v*j+face->v[f+1].v*k;pu=tiled.v[0].u*i+tiled.v[f].u*j+tiled.v[f+1].u*k;pv=tiled.v[0].v*i+tiled.v[f].v*j+tiled.v[f+1].v*k;if(i>=-1e-7&&j>=-1e-7&&k>=-1e-7)break;
        }
        double color[4];decalSample(*base,tu*base->width/(std::min)(base->width,256),tv*base->height/(std::min)(base->height,256),color,1);
        EdVec d=edSub(point,center);double du=edDot(d,u)/width+.5,dv=.5-edDot(d,v)/(width*decal->height/decal->width);
        double distance2=edDot(d,d)/(width*width*.25);
        if(paintStrength?distance2<1:(du>=0&&du<=1&&dv>=0&&dv<=1)){double top[4];
            if(paintStrength)decalSample(*decal,pu*decal->width/(std::min)(decal->width,256),pv*decal->height/(std::min)(decal->height,256),top,1);
            else decalSample(*decal,du*decal->width,dv*decal->height,top,0);
            double a=top[3]/255*(paintStrength?paintStrength*pow(1-distance2,2):1),b=color[3]/255,result=a+b*(1-a);if(a>0)changed=true;for(int k=0;k<3;k++)color[k]=result>0?(top[k]*a+color[k]*b*(1-a))/result:0;color[3]=result*255;}
        unsigned pixel;if(color[3]<.5){pixel=0xf81f;transparent=true;}else{pixel=((unsigned)fmin(255,color[0]+.5)>>3)<<11|((unsigned)fmin(255,color[1]+.5)>>2)<<5|((unsigned)fmin(255,color[2]+.5)>>3);if(pixel==0xf81f)pixel^=1;}size_t offset=140+((size_t)y*w+x)*2;(*bytes)[offset]=(unsigned char)pixel;(*bytes)[offset+1]=(unsigned char)(pixel>>8);
    }
    if(!changed)return 0;size_t at=4;auto put=[&](unsigned n){edNetStore32(bytes->data()+at,n);at+=4;};memcpy(bytes->data(),"MAT ",4);
    for(unsigned n:{50u,2u,1u,1u,1u,16u,5u,6u,5u,11u,5u,0u,3u,2u,3u,0u,0u,0u,8u,0u,0u,0u,0u,0u,0u,0u,0u,0u})put(n);put(w);put(h);put(transparent?1:0);put(0);put(transparent?0xf81f:0);put(1);
    int id=0;for(;id<m->decalCount;id++)if(*m->decals[id].data==*bytes)break;if(id==m->decalCount){if(id>=256)return 0;EdDecalMaterial &material=m->decals[m->decalCount++];material.data=bytes;snprintf(material.name,32,"gsd_%08x_%08x.mat",edNetHash(bytes->data(),(unsigned)bytes->size()),edNetHash(bytes->data()+140,(unsigned)bytes->size()-140));}
    face->decalId=id+1;face->customUV=1;face->uOffset=face->vOffset=face->uvRotation=0;
    for(int i=0;i<face->count;i++){EdVec p=edSub(face->v[i].p,origin);face->v[i].u=(2.5+(edDot(p,u)-minU)/(maxU-minU)*(w-5))*(std::min)(w,256)/w;face->v[i].v=(2.5+(edDot(p,v)-minV)/(maxV-minV)*(h-5))*(std::min)(h,256)/h;}
    edCompactDecals(m);return edDecalsValid(m);
}

// Texture-only transactions can update existing renderer surfaces without
// rebuilding sectors, collision, AI or player attachments.
int edSurfacePaintOnlyChange(const EdModel *before,const EdModel *after){
    if(before->count!=after->count||(!before->decalCount&&!after->decalCount))return 0;
    if(before->meshCount!=after->meshCount)return 0;
    for(int i=0;i<before->meshCount;i++){
        if(before->meshes[i].geometry.count!=after->meshes[i].geometry.count)return 0;
        for(int f=0;f<before->meshes[i].geometry.count;f++){
            int a=before->meshes[i].geometry.f[f].decalId,b=after->meshes[i].geometry.f[f].decalId;
            if(a!=b||(a&&*before->decals[a-1].data!=*after->decals[b-1].data))return 0;
        }
    }
    EdModel normalized=*after;normalized.decalCount=before->decalCount;normalized.decals=before->decals;
    for(int c=0;c<before->count;c++){
        if(before->c[c].count!=after->c[c].count)return 0;
        for(int f=0;f<before->c[c].count;f++){
            const EdFace &a=before->c[c].f[f];EdFace &b=normalized.c[c].f[f];if(a.count!=b.count)return 0;
            if(!a.decalId&&!b.decalId)continue;
            b.decalId=a.decalId;b.customUV=a.customUV;b.uOffset=a.uOffset;b.vOffset=a.vOffset;b.uvRotation=a.uvRotation;
            for(int v=0;v<a.count;v++){b.v[v].u=a.v[v].u;b.v[v].v=a.v[v].v;}
        }
    }return edModelsEqual(before,&normalized);
}
