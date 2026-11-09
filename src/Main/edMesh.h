/* Template strings use the engine's token grammar; never allow extra tokens. */
static int templateToken(const char *s,size_t capacity){if(!memchr(s,0,capacity)||!*s)return 0;for(;*s;s++)if((unsigned char)*s<=32||*s=='#'||*s=='='||*s=='"'||*s==',')return 0;return 1;}
int edTemplateIndex(const EdModel *m,const char *name){for(int i=0;i<m->templateCount;i++)if(!strncmp(m->templates[i].name,name,64))return i;return -1;}
int edTemplatesValidate(const EdModel *m){
    if(m->templateCount<0||m->templateCount>ED_CUSTOM_TEMPLATES)return 0;
    for(int i=0;i<m->templateCount;i++){const EdTemplate *t=&m->templates[i];if(!templateToken(t->name,64)||!templateToken(t->parent,64)||t->count<0||t->count>ED_TEMPLATE_PARAMS)return 0;
        for(const char *p=t->name;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9'))&&*p!='_'&&*p!='-')return 0;
        for(int j=0;j<i;j++)if(!strcmp(t->name,m->templates[j].name))return 0;
        int parent=edTemplateIndex(m,t->parent),depth=0;while(parent>=0){if(parent==i||++depth>m->templateCount)return 0;if(!memchr(m->templates[parent].parent,0,64))return 0;parent=edTemplateIndex(m,m->templates[parent].parent);}
        for(int j=0;j<t->count;j++){const EdTemplateParam *p=&t->params[j];if(!templateToken(p->key,32)||!templateToken(p->value,128))return 0;
            for(const char *k=p->key;*k;k++)if(!((*k>='a'&&*k<='z')||(*k>='0'&&*k<='9')))return 0;
            if(strstr(p->value,"..")||strchr(p->value,'\\'))return 0;
            if(!strcmp(p->key,"numframes")){char *end;long n=strtol(p->value,&end,10);if(*end||n<0||n>ED_FRAMES)return 0;}
            {char *end;double n=strtod(p->value,&end);if(end!=p->value&&!*end&&(!isfinite(n)||fabs(n)>3.402823e38))return 0;}
            if(p->value[0]=='('){const char *q=p->value+1;int count=0;for(;;){char *end;double n=strtod(q,&end);if(end==q||!isfinite(n)||fabs(n)>3.402823e38||++count>6)return 0;if(*end==')'&&!end[1])break;if(*end!='/'&&*end!=':')return 0;q=end+1;}}
        }
    }return 1;
}
/* Mesh operations use outward normals and never invoke sector CSG. Included
   by edGeometry.c so clipping uses the same UV-preserving polygon math. */
static int meshFaceValid(const EdFace *f){
    if(f->count<3||!edSurfaceVertexCountAllowed(f->count))return 0;
    EdVec n=edNormal(f);if(length(n)<.9)return 0;
    for(int v=0;v<f->count;v++){
        EdVertex p=f->v[v];if(!isfinite(p.p.x)||!isfinite(p.p.y)||!isfinite(p.p.z)||fabs(p.p.x)>1e6||fabs(p.p.y)>1e6||fabs(p.p.z)>1e6||!isfinite(p.u)||!isfinite(p.v))return 0;
        if(fabs(edDot(n,edSub(p.p,f->v[0].p)))>ED_EPS*4)return 0;
        EdVec edge=edSub(f->v[(v+1)%f->count].p,p.p);if(length(edge)<ED_EPS)return 0;
        for(int j=0;j<f->count;j++)if(edDot(edCross(edge,edSub(f->v[j].p,p.p)),n)<-ED_EPS)return 0;
    }return 1;
}
int edMeshValidate(const EdModel *m){
    if(!edTemplatesValidate(m))return 0;
    if(m->assetCount<0||m->assetCount>ED_ASSETS||m->meshCount<0||m->meshCount>ED_MESHES)return 0;
    for(int a=0;a<m->assetCount;a++){
        const EdAsset *asset=&m->assets[a];if(asset->terrainGroup<0)return 0;if((asset->saved!=0&&asset->saved!=1)||(asset->terrain!=0&&asset->terrain!=1))return 0;if(asset->terrain&&(!asset->saved||!isfinite(length(asset->terrainNormal))||fabs(length(asset->terrainNormal)-1)>.0001))return 0;if(!memchr(asset->name,0,32)||!asset->name[0]||strlen(asset->name)>27||length(asset->origin)>1e6||!isfinite(length(asset->origin)))return 0;
        for(const char *p=asset->name;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'))return 0;
        for(int b=0;b<a;b++)if(!strcmp(asset->name,m->assets[b].name))return 0;
    }
    for(int i=0;i<m->meshCount;i++){
        const EdMesh *mesh=&m->meshes[i];const EdCell *c=&mesh->geometry;int vertices=0;
        if(mesh->asset<0||mesh->asset>=m->assetCount||c->count<1||c->count>ED_FACES)return 0;
        for(int f=0;f<c->count;f++){if(!isfinite(c->f[f].uvRotation)||!meshFaceValid(&c->f[f])||c->f[f].material<0||c->f[f].neighbor!=-1)return 0;vertices+=c->f[f].count;}
        if(vertices>512)return 0;if(mesh->terrainNormals.size()){if(mesh->terrainNormals.size()!=(size_t)c->count*3)return 0;for(size_t j=0;j<mesh->terrainNormals.size();j++){double n=length(mesh->terrainNormals[j]);if(!isfinite(n)||fabs(n-1)>.0001)return 0;}}if(m->assets[mesh->asset].terrain){if(mesh->terrainPinned.size()!=(size_t)c->count*3)return 0;for(int f=0;f<c->count;f++){if(c->f[f].count!=3)return 0;for(int v=0;v<3;v++)if(mesh->terrainPinned[f*3+v]>1)return 0;}}
    }return 1;
}
static void meshPrimitiveFace(EdCell *c,EdVec *points,int count,EdVec center,int material){
    EdFace *f=&c->f[c->count++];edZero(f,0,sizeof(*f));f->count=count;f->material=material;f->neighbor=f->mirror=-1;
    EdVec average=edV(0,0,0);for(int i=0;i<count;i++){f->v[i].p=points[i];average=edAdd(average,points[i]);}
    if(edDot(edNormal(f),edSub(edScale(average,1.0/count),center))<0)for(int i=0;i<count/2;i++){EdVertex v=f->v[i];f->v[i]=f->v[count-i-1];f->v[count-i-1]=v;}edMapFace(f);
}
int edMeshPrimitiveDetail(EdModel *m,int asset,int primitive,EdVec center,int material,int detail){
    if(asset<0||asset>=m->assetCount||m->meshCount>=ED_MESHES||primitive<0||primitive>3||detail<0||detail>2)return 0;
    EdMesh *mesh=&m->meshes[m->meshCount];edZero(mesh,0,sizeof(*mesh));mesh->asset=asset;EdCell *c=&mesh->geometry;double r=.25;
    if(primitive==0||primitive==3){
        EdVec p[8];for(int i=0;i<8;i++)p[i]=edAdd(center,edV(i&1?r:-r,i&2?r:-r,i&4?r:-r));
        int faces[6][4]={{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
        for(int f=0;f<(primitive==0?6:1);f++){EdVec q[4];for(int v=0;v<4;v++)q[v]=p[faces[f][v]];meshPrimitiveFace(c,q,4,center,material);}
        if(primitive==3)for(int i=0;i<4;i++){EdVec q[3]={p[faces[0][i]],p[faces[0][(i+1)%4]],edAdd(center,edV(0,0,r))};meshPrimitiveFace(c,q,3,center,material);}
    }else if(primitive==2){
        int sides=detail==0?12:detail==1?16:24;EdVec bottom[24],top[24];for(int i=0;i<sides;i++){double a=i*6.283185307179586/sides;bottom[i]=edAdd(center,edV(cos(a)*r,sin(a)*r,-r));top[i]=edAdd(bottom[i],edV(0,0,2*r));}
        meshPrimitiveFace(c,bottom,sides,center,material);meshPrimitiveFace(c,top,sides,center,material);
        for(int i=0;i<sides;i++){int j=(i+1)%sides;EdVec q[4]={bottom[i],bottom[j],top[j],top[i]};meshPrimitiveFace(c,q,4,center,material);}
    }else {
        int sides=8+detail*4,latitudes=4+detail*2;EdVec ring[7][16];for(int j=0;j<latitudes-1;j++)for(int i=0;i<sides;i++){double a=i*6.283185307179586/sides,b=(j+1)*3.141592653589793/latitudes;ring[j][i]=edAdd(center,edV(sin(b)*cos(a)*r,sin(b)*sin(a)*r,cos(b)*r));}
        for(int i=0;i<sides;i++){int next=(i+1)%sides;EdVec t[3]={edAdd(center,edV(0,0,r)),ring[0][i],ring[0][next]},b[3]={edAdd(center,edV(0,0,-r)),ring[latitudes-2][next],ring[latitudes-2][i]};meshPrimitiveFace(c,t,3,center,material);meshPrimitiveFace(c,b,3,center,material);
            for(int j=0;j<latitudes-2;j++){EdVec q[4]={ring[j][i],ring[j+1][i],ring[j+1][next],ring[j][next]};meshPrimitiveFace(c,q,4,center,material);}}
    }
    m->meshCount++;if(!edMeshValidate(m)){m->meshCount--;edZero(mesh,0,sizeof(*mesh));return 0;}return 1;
}
int edMeshPrimitive(EdModel *m,int asset,int primitive,EdVec center,int material){return edMeshPrimitiveDetail(m,asset,primitive,center,material,0);}
int edMeshPrimitiveTransformedDetail(EdModel *m,int asset,int primitive,EdVec center,int material,double size,double degrees,int detail){
    if(!isfinite(size)||size<ED_GRID||size>32||!isfinite(degrees))return 0;
    int index=m->meshCount;if(!edMeshPrimitiveDetail(m,asset,primitive,center,material,detail))return 0;
    double angle=degrees*3.141592653589793/180,c=cos(angle),s=sin(angle);EdCell *cell=&m->meshes[index].geometry;
    for(int f=0;f<cell->count;f++){EdFace *face=&cell->f[f];for(int v=0;v<face->count;v++){EdVec p=edScale(edSub(face->v[v].p,center),size/.5);EdVec placed=edAdd(center,edV(p.x*c-p.y*s,p.x*s+p.y*c,p.z));face->v[v].p=primitive==0?edSnapPoint(placed):placed;}edMapFace(face);}
    int faces=cell->count;for(int f=0;f<faces;f++)if(!meshFaceValid(&cell->f[f])){
        EdFace original=cell->f[f];if(original.count<4||cell->count+original.count-3>ED_FACES){m->meshCount=index;return 0;}
        for(int v=1;v<original.count-1;v++){EdFace triangle=original;triangle.count=3;triangle.v[0]=original.v[0];triangle.v[1]=original.v[v];triangle.v[2]=original.v[v+1];if(v==1)cell->f[f]=triangle;else cell->f[cell->count++]=triangle;}
    }
    if(!edMeshValidate(m)){m->meshCount=index;return 0;}return 1;
}
int edMeshPrimitiveTransformed(EdModel *m,int asset,int primitive,EdVec center,int material,double size,double degrees){return edMeshPrimitiveTransformedDetail(m,asset,primitive,center,material,size,degrees,0);}
int edMeshRay(const EdModel *m,int asset,EdVec o,EdVec d,int handles,EdHit *h){
    *h=EdHit{-1,-1,-1,-1,1e30};
    for(int s=0;s<m->meshCount;s++)if(asset<0||m->meshes[s].asset==asset)for(int f=0;f<m->meshes[s].geometry.count;f++){
        const EdFace *face=&m->meshes[s].geometry.f[f];EdVec n=edNormal(face);double den=edDot(n,d);if(den>=-ED_EPS)continue;
        double t=edDot(n,edSub(face->v[0].p,o))/den;if(t<0||t>=h->distance)continue;EdVec p=edAdd(o,edScale(d,t));int inside=1;
        for(int i=0;i<face->count;i++)if(edDot(edCross(edSub(face->v[(i+1)%face->count].p,face->v[i].p),edSub(p,face->v[i].p)),n)<-ED_EPS)inside=0;
        if(inside)*h=EdHit{s,f,-1,-1,t,p};
    }
    if(h->cell<0||!handles)return h->cell>=0;
    const EdFace *f=&m->meshes[h->cell].geometry.f[h->face];double best=fmin(.04,h->distance*.018);
    for(int v=0;v<f->count;v++){double dist=length(edSub(h->point,f->v[v].p));if(dist<best){best=dist;h->vertex=v;}}
    if(h->vertex<0)for(int v=0;v<f->count;v++){EdVec a=f->v[v].p,e=edSub(f->v[(v+1)%f->count].p,a);double t=fmax(0,fmin(1,edDot(edSub(h->point,a),e)/edDot(e,e)));double dist=length(edSub(h->point,edAdd(a,edScale(e,t))));if(dist<best){best=dist;h->edge=v;}}
    return 1;
}
static int meshHitValid(const EdModel *m,EdHit h){return h.cell>=0&&h.cell<m->meshCount&&h.face>=0&&h.face<m->meshes[h.cell].geometry.count&&h.vertex<m->meshes[h.cell].geometry.f[h.face].count&&h.edge<m->meshes[h.cell].geometry.f[h.face].count;}
int edMeshCleave(EdModel *m,EdHit h,EdVec a,EdVec b){
    if(!meshHitValid(m,h))return 0;EdCell *c=&m->meshes[h.cell].geometry;EdFace original=c->f[h.face],left,right;
    a=edSnapMeshFacePoint(&original,a);b=edSnapMeshFacePoint(&original,b);
    EdVec n=unit(edCross(edNormal(&original),edSub(b,a)));double d=edDot(n,a);
    if(length(n)<.9||c->count>=ED_FACES||!clip(&original,n,d,&left)||!clip(&original,edScale(n,-1),-d,&right)||!left.count||!right.count)return 0;
    EdCell before=*c;c->f[h.face]=left;c->f[c->count++]=right;
    if(connectCellEdges(c)&&edMeshValidate(m))return 1;*c=std::move(before);return 0;
}
int edMeshExtrude(EdModel *m,EdHit h){
    if(!meshHitValid(m,h)||h.vertex>=0||h.edge>=0)return 0;EdCell *c=&m->meshes[h.cell].geometry;EdFace base=c->f[h.face];
    if(c->count+base.count>ED_FACES)return 0;EdVec delta=edScale(edNormal(&base),ED_GRID);
    for(int i=0;i<base.count;i++)c->f[h.face].v[i].p=edAdd(base.v[i].p,delta);edMapFace(&c->f[h.face]);
    for(int i=0;i<base.count;i++){int j=(i+1)%base.count;EdFace *side=&c->f[c->count++];*side=base;side->count=4;side->v[0]=base.v[i];side->v[1]=base.v[j];side->v[2]=c->f[h.face].v[j];side->v[3]=c->f[h.face].v[i];edMapFace(side);}
    return edMeshValidate(m);
}
int edMeshMoveConstrained(EdModel *m,const EdModel *source,const EdSelection *selection,EdVec delta,int freeMovement){
    if(!selection->count)return 0;
    for(int i=0;i<selection->count;i++)if(!meshHitValid(source,selection->hits[i]))return 0;
    int vertex=selection->count==1&&selection->hits[0].vertex>=0;EdVec target={0};
    // A grab is not an edit. In particular, don't snap curved/off-grid
    // primitives, connect edges or weld corners until the drag actually moves.
    if(length(delta)<ED_GRID*.05){edCopyModel(m,source);return 1;}
    if(!vertex){delta=edSnapPoint(delta);if(length(delta)<ED_EPS){edCopyModel(m,source);return 1;}}

    if(vertex){EdHit h=selection->hits[0];const EdCell *cell=&source->meshes[h.cell].geometry;EdVec point=cell->f[h.face].v[h.vertex].p;
        // Added: keep the large edge-scratch model off the stack
        if(!freeMovement){static EdModel edges;edges.count=1;edges.c[0]=*cell;h.cell=0;target=edAdd(point,edVertexEdgeDelta(&edges,h,delta));}
        else{target=edSnapPoint(edAdd(point,delta));double nearest=ED_GRID*.55;
            for(int f=0;f<cell->count;f++)for(int v=0;v<cell->f[f].count;v++){EdVec other=cell->f[f].v[v].p;double distance=length(edSub(other,target));if(distance<nearest){target=other;nearest=distance;}}}
    }
    if(vertex){EdHit h=selection->hits[0];if(same(target,source->meshes[h.cell].geometry.f[h.face].v[h.vertex].p)){edCopyModel(m,source);return 1;}}
    edCopyModel(m,source);EdBuffer<unsigned char> affected;for(int i=0;i<selection->count;i++)affected[selection->hits[i].cell]=1;
    for(int s=0;s<m->meshCount;s++)if(affected[s]){
        EdCell *c=&m->meshes[s].geometry;if(!connectCellEdges(c))return 0;
        for(int f=0;f<c->count;f++)for(int v=0;v<c->f[f].count;v++){
            EdVertex *p=&c->f[f].v[v];int move=0;
            for(int i=0;i<selection->count&&!move;i++){EdHit h=selection->hits[i];if(h.cell!=s)continue;const EdFace *face=&source->meshes[s].geometry.f[h.face];
                if(h.vertex<0&&h.edge<0&&pointOnSurface(face,p->p)){move=1;break;}
                for(int j=0;j<face->count;j++)if((h.vertex<0||h.vertex==j)&&(h.edge<0||h.edge==j||(h.edge+1)%face->count==j)&&same(p->p,face->v[j].p)){move=1;break;}}
            if(move)p->p=vertex?target:edAdd(p->p,delta);
        }
        /* Weld coincident corners in every incident face before validating or
           triangulating. Collapsed triangles disappear; their neighbors remain. */
        for(int f=0;f<c->count;){EdFace *face=&c->f[f];
            for(int v=0;v<face->count&&face->count>1;){int next=(v+1)%face->count;if(same(face->v[v].p,face->v[next].p)){memmove(face->v+next,face->v+next+1,(face->count-next-1)*sizeof(EdVertex));face->count--;v=0;}else v++;}
            if(face->count<3||length(edNormal(face))<.5){for(int j=f;j+1<c->count;j++)c->f[j]=c->f[j+1];c->count--;}else f++;
        }
        int count=c->count;for(int f=0;f<count;f++){EdFace original=c->f[f];if(!meshFaceValid(&original)){
            if(original.count<4||original.count-3>ED_FACES-c->count)return 0;
            for(int i=1;i<original.count-1;i++){EdFace triangle=original;triangle.count=3;triangle.v[0]=original.v[0];triangle.v[1]=original.v[i];triangle.v[2]=original.v[i+1];if(!meshFaceValid(&triangle))return 0;if(i==1)c->f[f]=triangle;else c->f[c->count++]=triangle;}
        }}
        for(int f=0;f<c->count;f++)edMapFace(&c->f[f]);
    }
    return edMeshValidate(m);
}
int edMeshMove(EdModel *m,const EdModel *source,const EdSelection *selection,EdVec delta){return edMeshMoveConstrained(m,source,selection,delta,1);}
