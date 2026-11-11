/* Vertex paint is independent of the two UI slots. Named layers survive slot
   changes, undo, and network material-index remapping. */
int (*edPaintLoadImage)(const char*,EdPaintImage*)=NULL;
static int paintNameValid(const char *name){
    if(!memchr(name,0,128)||!*name)return 0;
    for(const char *p=name;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'||*p=='.'))return 0;
    return !strstr(name,"..");
}
int edPaintValid(const EdModel *m){
    if(m->assetCount<0||m->assetCount>ED_ASSETS||m->meshCount<0||m->meshCount>ED_MESHES)return 0;
    for(int a=0;a<m->assetCount;a++)for(int s=0;s<2;s++)if(m->assets[a].paintSlots[s][0]&&!paintNameValid(m->assets[a].paintSlots[s]))return 0;
    for(int i=0;i<m->meshCount;i++){
        const EdMesh &mesh=m->meshes[i];if(mesh.paint.size()>ED_PAINT_LAYERS)return 0;if(!mesh.paint.size())continue;
        if(mesh.geometry.count<1||mesh.geometry.count>170)return 0;
        for(int f=0;f<mesh.geometry.count;f++)if(mesh.geometry.f[f].count!=3)return 0;
        for(size_t l=0;l<mesh.paint.size();l++){
            const EdPaintLayer &layer=mesh.paint[l];if(!paintNameValid(layer.material)||layer.weights.size()!=(size_t)mesh.geometry.count*3)return 0;
            for(size_t j=0;j<l;j++)if(!strcmp(layer.material,mesh.paint[j].material))return 0;
        }
        for(int v=0;v<mesh.geometry.count*3;v++){double sum=0;for(size_t l=0;l<mesh.paint.size();l++){double w=mesh.paint[l].weights[v];if(!isfinite(w)||w<0||w>1)return 0;sum+=w;}if(fabs(sum-1)>1e-6)return 0;}
    }return 1;
}
// Exact comparison prevents the paint fast path from swallowing geometry,
// object, lighting, template, or multiplayer changes in the same transaction.
int edPaintOnlyChange(const EdModel *before,const EdModel *after){
    if(before==after||before->meshCount!=after->meshCount||before->assetCount!=after->assetCount)return 0;
    int changed=0;for(int a=0;a<after->assetCount;a++)changed|=edPaintHash(before,a)!=edPaintHash(after,a);
    if(!changed)return 0;
    EdModel geometry=*after;for(int i=0;i<geometry.meshCount;i++)geometry.meshes[i].paint=before->meshes[i].paint;
    return edModelsEqual(before,&geometry);
}
int edPainted(const EdModel *m,int asset){for(int i=0;i<m->meshCount;i++)if(m->meshes[i].asset==asset&&m->meshes[i].paint.size())return 1;return 0;}
unsigned int edPaintHash(const EdModel *m,int asset){
    unsigned int hash=2166136261u;
    for(int i=0;i<m->meshCount;i++)if(m->meshes[i].asset==asset)for(size_t l=0;l<m->meshes[i].paint.size();l++){
        const EdPaintLayer &p=m->meshes[i].paint[l];hash=(hash^edNetHash(p.material,128))*16777619u;
        for(size_t v=0;v<p.weights.size();v++)hash=(hash^edNetHash(&p.weights[v],sizeof(double)))*16777619u;
    }return hash;
}
void edPaintFilename(const EdModel *m,int asset,char *name){snprintf(name,32,"gsp_%08x.mat",edNetHash(m->assets[asset].name,strlen(m->assets[asset].name)));}
static int paintLayer(EdMesh &mesh,const char *name){
    for(size_t l=0;l<mesh.paint.size();l++)if(!strcmp(mesh.paint[l].material,name))return (int)l;
    if(mesh.paint.size()>=ED_PAINT_LAYERS)return -1;
    int id=(int)mesh.paint.size();EdPaintLayer &layer=mesh.paint[id];snprintf(layer.material,128,"%s",name);layer.weights.resize(mesh.geometry.count*3);return id;
}
int edTerrainPaint(EdModel *m,int asset,EdVec center,double radius,double amount,const char *material,const char*(*name)(int)){
    if(asset<0||asset>=m->assetCount||!m->assets[asset].terrain||!paintNameValid(material)||radius<=0||!isfinite(radius)||!isfinite(amount)||amount<=0||!name)return 0;
    EdModel draft=*m;int group=m->assets[asset].terrainGroup,changed=0;
    for(int i=0;i<draft.meshCount;i++){
        EdMesh &mesh=draft.meshes[i];if(mesh.asset!=asset&&(!group||!draft.assets[mesh.asset].terrain||draft.assets[mesh.asset].terrainGroup!=group))continue;
        if(!mesh.paint.size())for(int f=0;f<mesh.geometry.count;f++){
            const char *base=name(mesh.geometry.f[f].material);if(!base||!paintNameValid(base))return 0;
            int l=paintLayer(mesh,base);if(l<0)return 0;for(int v=0;v<3;v++)mesh.paint[l].weights[f*3+v]=1;
        }
        int layer=paintLayer(mesh,material);if(layer<0)return 0;
        for(int f=0;f<mesh.geometry.count;f++)for(int v=0;v<3;v++){
            double d=length(edSub(mesh.geometry.f[f].v[v].p,center))/radius;if(d>=1)continue;
            double alpha=fmin(1,amount)*(1-d*d)*(1-d*d);int index=f*3+v;
            if(mesh.paint[layer].weights[index]<1-1e-12)changed=1;
            for(size_t l=0;l<mesh.paint.size();l++)mesh.paint[l].weights[index]*=1-alpha;
            mesh.paint[layer].weights[index]+=alpha;
        }
    }
    if(!changed||!edPaintValid(&draft))return 0;*m=std::move(draft);return 1;
}
/* A padded triangle atlas avoids overlapping UV charts on folded terrain.
   The source UVs still tile each layer; the atlas UVs are render/export only. */
int edPaintBakeAsset(const EdModel *m,int asset,EdPaintBake *out,int incremental){
    if(!incremental)*out={};if(!edPainted(m,asset)||!edPaintLoadImage||!edPaintValid(m))return 0;
    auto hashBytes=[](uint64_t hash,const void *data,size_t size){const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<size;i++)hash=(hash^p[i])*1099511628211ull;return hash;};
    struct Source {std::string name;EdPaintImage image;uint64_t hash;};std::vector<Source> sources;
    int faces=0;for(int i=0;i<m->meshCount;i++)if(m->meshes[i].asset==asset){
        const EdMesh &mesh=m->meshes[i];if(!mesh.paint.size())return 0;faces+=mesh.geometry.count;
        for(size_t l=0;l<mesh.paint.size();l++){const char *name=mesh.paint[l].material;bool found=false;for(auto &s:sources)found|=s.name==name;if(found)continue;
            Source s;s.name=name;if(!edPaintLoadImage(name,&s.image)||s.image.width<1||s.image.height<1||s.image.width>4096||s.image.height>4096||s.image.rgba.size()!=(size_t)s.image.width*s.image.height*4)return 0;s.hash=hashBytes(14695981039346656037ull,s.image.rgba.data(),s.image.rgba.size());s.hash=hashBytes(s.hash,&s.image.width,sizeof(s.image.width));s.hash=hashBytes(s.hash,&s.image.height,sizeof(s.image.height));sources.push_back(std::move(s));
        }
    }
    if(faces<1||faces>2720)return 0;
    int columns=1;while(columns*columns<faces)columns*=2;
    int tile=std::min(64,2048/columns),size=columns*tile;
    if(out->size!=size||out->faceHashes.size()!=(size_t)faces||out->mat.size()!=140+(size_t)size*size*2)*out={};
    out->size=size;out->updatedFaces=0;out->faceHashes.resize(faces);out->faceAlpha.resize(faces);out->mat.resize(140+(size_t)size*size*2);out->uv.clear();
    // JK high-resolution materials retain a maximum 256-unit UV space.
    double uvScale=(double)std::min(size,256)/size;
    int faceIndex=0;bool transparent=false;
    for(int i=0;i<m->meshCount;i++)if(m->meshes[i].asset==asset){const EdMesh &mesh=m->meshes[i];std::vector<const EdPaintImage*> images;std::vector<uint64_t> imageHashes;
        for(size_t l=0;l<mesh.paint.size();l++)for(auto &s:sources)if(s.name==mesh.paint[l].material){images.push_back(&s.image);imageHashes.push_back(s.hash);break;}
        for(int f=0;f<mesh.geometry.count;f++,faceIndex++){
            const EdFace &face=mesh.geometry.f[f];int ox=(faceIndex%columns)*tile,oy=(faceIndex/columns)*tile;double span=tile-9;
            out->uv.push_back(edScale(edV(ox+4.5,oy+4.5,0),uvScale));out->uv.push_back(edScale(edV(ox+4.5+span,oy+4.5,0),uvScale));out->uv.push_back(edScale(edV(ox+4.5,oy+4.5+span,0),uvScale));
            uint64_t hash=14695981039346656037ull;
            for(int v=0;v<3;v++){hash=hashBytes(hash,&face.v[v].u,sizeof(double));hash=hashBytes(hash,&face.v[v].v,sizeof(double));}
            for(size_t l=0;l<images.size();l++){
                if(mesh.paint[l].weights[f*3]==0&&mesh.paint[l].weights[f*3+1]==0&&mesh.paint[l].weights[f*3+2]==0)continue;
                hash=hashBytes(hash,&imageHashes[l],sizeof(uint64_t));for(int v=0;v<3;v++)hash=hashBytes(hash,&mesh.paint[l].weights[f*3+v],sizeof(double));
            }
            if(out->faceHashes[faceIndex]==hash){transparent|=out->faceAlpha[faceIndex]!=0;continue;}
            out->faceHashes[faceIndex]=hash;out->faceAlpha[faceIndex]=0;out->updatedFaces++;
            for(int y=0;y<tile;y++)for(int x=0;x<tile;x++){
                double b=fmax(0,fmin(1,(x-4)/span)),c=fmax(0,fmin(1,(y-4)/span));if(b+c>1){double sum=b+c;b/=sum;c/=sum;}double bary[3]={1-b-c,b,c},u=0,v=0;
                for(int k=0;k<3;k++){u+=face.v[k].u*bary[k];v+=face.v[k].v*bary[k];}
                double rgba[4]={};for(size_t l=0;l<images.size();l++){
                    double weight=0;for(int k=0;k<3;k++)weight+=mesh.paint[l].weights[f*3+k]*bary[k];if(weight<1e-9)continue;
                    const EdPaintImage &im=*images[l];double sx=u*im.width/std::min(im.width,256)-.5,sy=v*im.height/std::min(im.height,256)-.5;int ix=(int)floor(fmod(sx,im.width)),iy=(int)floor(fmod(sy,im.height));double tx=sx-floor(sx),ty=sy-floor(sy);
                    for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
                        int px=((ix+dx)%im.width+im.width)%im.width,py=((iy+dy)%im.height+im.height)%im.height;const unsigned char *p=&im.rgba[((size_t)py*im.width+px)*4];double w=weight*(dx?tx:1-tx)*(dy?ty:1-ty);
                        for(int k=0;k<4;k++)rgba[k]+=p[k]*w;
                    }
                }
                unsigned short pixel;if(rgba[3]<.5){pixel=0xf81f;transparent=true;out->faceAlpha[faceIndex]=1;}else{pixel=((unsigned)fmin(255,rgba[0]+.5)>>3)<<11|((unsigned)fmin(255,rgba[1]+.5)>>2)<<5|((unsigned)fmin(255,rgba[2]+.5)>>3);if(pixel==0xf81f)pixel^=1;}
                size_t offset=140+((size_t)(oy+y)*size+ox+x)*2;out->mat[offset]=(unsigned char)pixel;out->mat[offset+1]=(unsigned char)(pixel>>8);
            }
        }
    }
    size_t header=4;auto put=[&](unsigned n){for(int b=0;b<4;b++)out->mat[header++]=(unsigned char)(n>>(b*8));};memcpy(out->mat.data(),"MAT ",4);
    for(unsigned n:{50u,2u,1u,1u,1u,16u,5u,6u,5u,11u,5u,0u,3u,2u,3u,0u,0u,0u,8u,0u,0u,0u,0u,0u,0u,0u,0u,0u})put(n);
    put(size);put(size);put(transparent?1:0);put(0);put(transparent?0xf81f:0);put(1);
    return 1;
}
