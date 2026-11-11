/* Linked terrain patches retain original vertex normals and shared seams.
   Each source sector owns a separate ordinary mesh asset and placed thing. */
static int terrainInside(const std::vector<EdFace>& faces,EdVec point,EdVec normal){
    for(const EdFace &face:faces){int inside=1;for(int v=0;v<face.count;v++)if(edDot(edCross(edSub(face.v[(v+1)%face.count].p,face.v[v].p),edSub(point,face.v[v].p)),normal)<-1e-9){inside=0;break;}if(inside)return 1;}return 0;
}
int edTerrainObject(const EdModel *m,int object){
    if(object<0||object>=m->objectCount)return -1;
    for(int a=0;a<m->assetCount;a++)if(m->assets[a].terrain&&!strcmp(m->assets[a].name,m->objects[object].templateName))return a;
    return -1;
}
// One welded UV chart for the entire terrain group. Relax projected edge
// lengths toward their 3D lengths, retaining seams and world texture density.
static void terrainMapUV(EdModel *m,int asset){
    struct Node {EdVec p,uv;};struct Corner {int mesh,face,vertex,node;};struct Edge {int a,b;double distance;};
    std::vector<Node> nodes;std::vector<Corner> corners;std::vector<Edge> edges;EdVec normal={};int group=m->assets[asset].terrainGroup;
    for(int i=0;i<m->meshCount;i++){
        EdMesh &mesh=m->meshes[i];if(mesh.asset!=asset&&(!group||!m->assets[mesh.asset].terrain||m->assets[mesh.asset].terrainGroup!=group))continue;
        for(int f=0;f<mesh.geometry.count;f++){
            int ids[3];for(int v=0;v<3;v++){
                EdVec p=mesh.geometry.f[f].v[v].p;int id=0;for(;id<(int)nodes.size();id++)if(length(edSub(p,nodes[id].p))<1e-8)break;
                if(id==(int)nodes.size()){nodes.push_back({p,{}});normal=edAdd(normal,mesh.terrainNormals.size()?mesh.terrainNormals[f*3+v]:m->assets[mesh.asset].terrainNormal);}
                ids[v]=id;corners.push_back({i,f,v,id});
            }
            for(int v=0;v<3;v++){int a=(std::min)(ids[v],ids[(v+1)%3]),b=(std::max)(ids[v],ids[(v+1)%3]);int found=0;for(const auto &edge:edges)if(edge.a==a&&edge.b==b){found=1;break;}if(!found&&a!=b)edges.push_back({a,b,length(edSub(nodes[a].p,nodes[b].p))});}
        }
    }
    if(nodes.empty())return;if(length(normal)<1e-8)normal=m->assets[asset].terrainNormal;normal=unit(normal);
    EdVec u=unit(edCross(fabs(normal.z)<.9?edV(0,0,1):edV(0,1,0),normal)),v=edCross(normal,u);
    for(auto &node:nodes)node.uv=edV(edDot(node.p,u),edDot(node.p,v),0);
    std::vector<EdVec> forces(nodes.size());std::vector<int> degree(nodes.size());
    for(const auto &edge:edges){degree[edge.a]++;degree[edge.b]++;}
    for(int iteration=0;iteration<80;iteration++){
        std::fill(forces.begin(),forces.end(),edV(0,0,0));
        for(const auto &edge:edges){EdVec d=edSub(nodes[edge.b].uv,nodes[edge.a].uv);double n=length(d);if(n<1e-10)continue;EdVec force=edScale(d,(n-edge.distance)/n);forces[edge.a]=edAdd(forces[edge.a],force);forces[edge.b]=edSub(forces[edge.b],force);}
        for(size_t i=1;i<nodes.size();i++)if(degree[i])nodes[i].uv=edAdd(nodes[i].uv,edScale(forces[i],.5/degree[i]));
    }
    for(const auto &corner:corners){EdFace &face=m->meshes[corner.mesh].geometry.f[corner.face];face.customUV=1;face.uOffset=face.vOffset=face.uvRotation=0;face.v[corner.vertex].u=nodes[corner.node].uv.x*ED_UV_SCALE;face.v[corner.vertex].v=nodes[corner.node].uv.y*ED_UV_SCALE;}
}
int edTerrainCreate(EdModel *out,const EdModel *source,const EdSelection *surfaces,const char *name){
    if(!surfaces||surfaces->count<1||surfaces->count>ED_SELECTIONS||source->assetCount>=ED_ASSETS||source->meshCount>=ED_MESHES||!name||strlen(name)>27)return 0;
    std::vector<EdFace> faces;std::vector<int> cells;
    for(int i=0;i<surfaces->count;i++){EdHit hit=surfaces->hits[i];if(hit.cell<0||hit.cell>=source->count||hit.face<0||hit.face>=source->c[hit.cell].count)return 0;
        const EdFace &face=source->c[hit.cell].f[hit.face];if(face.neighbor>=0||hit.vertex>=0||hit.edge>=0)return 0;
        faces.push_back(face);cells.push_back(hit.cell);
    }
    EdModel draft; edCopyModel(&draft,source);
    int group=1;for(;;group++){int used=0;for(int a=0;a<source->assetCount;a++)used|=source->assets[a].terrainGroup==group;if(!used)break;}
    std::vector<int> sectors,assets;
    for(size_t f=0;f<faces.size();f++)if(std::find(sectors.begin(),sectors.end(),cells[f])==sectors.end()){
        if(draft.assetCount>=ED_ASSETS)return 0;
        sectors.push_back(cells[f]);int asset=draft.assetCount++;assets.push_back(asset);EdAsset &a=draft.assets[asset];a={};
        if(assets.size()==1)snprintf(a.name,32,"%s",name);else snprintf(a.name,32,"%.20s_s%d",name,(int)assets.size());
        a.saved=a.terrain=1;a.terrainGroup=group;a.terrainNormal=edNormal(&faces[f]);
        EdVec center={};for(int v=0;v<faces[f].count;v++)center=edAdd(center,faces[f].v[v].p);
        center=edScale(center,1.0/faces[f].count);EdVec interior={};int corners=0;
        const EdCell &cell=source->c[cells[f]];for(int f=0;f<cell.count;f++)for(int v=0;v<cell.f[f].count;v++){interior=edAdd(interior,cell.f[f].v[v].p);corners++;}
        a.origin=edAdd(edScale(center,.999),edScale(interior,.001/corners));
    }
    double spacing=ED_GRID*4;int budget=2048; // Per-operation tessellation budget, independent of existing meshes.
    std::vector<EdFace> triangles;std::vector<int> owners;
    for(int attempt=0;attempt<16;attempt++,spacing*=2){triangles.clear();owners.clear();int overflow=0;
        for(size_t faceIndex=0;faceIndex<faces.size();faceIndex++){const EdFace &face=faces[faceIndex];EdVec normal=edNormal(&face),u=unit(edCross(fabs(normal.z)<.9?edV(0,0,1):edV(0,1,0),normal)),v=edCross(normal,u);double xmin=1e30,xmax=-1e30,ymin=1e30,ymax=-1e30;
            for(int i=0;i<face.count;i++){double x=edDot(face.v[i].p,u),y=edDot(face.v[i].p,v);xmin=fmin(xmin,x);xmax=fmax(xmax,x);ymin=fmin(ymin,y);ymax=fmax(ymax,y);}
            int x0=(int)floor(xmin/spacing),x1=(int)ceil(xmax/spacing),y0=(int)floor(ymin/spacing),y1=(int)ceil(ymax/spacing);
            if((double)(x1-x0)*(y1-y0)>budget*4){overflow=1;break;}
            for(int x=x0;x<x1&&!overflow;x++)for(int y=y0;y<y1&&!overflow;y++){
                EdFace piece=face,tmp;if(!clip(&piece,u,x*spacing,&tmp))return 0;piece=tmp;if(!piece.count)continue;
                if(!clip(&piece,edScale(u,-1),-(x+1)*spacing,&tmp))return 0;piece=tmp;if(!piece.count)continue;
                if(!clip(&piece,v,y*spacing,&tmp))return 0;piece=tmp;if(!piece.count)continue;
                if(!clip(&piece,edScale(v,-1),-(y+1)*spacing,&tmp))return 0;piece=tmp;
                for(int k=1;k+1<piece.count;k++){EdFace triangle=piece;triangle.count=3;triangle.neighbor=triangle.mirror=-1;triangle.customUV=1;triangle.v[0]=piece.v[0];triangle.v[1]=piece.v[k];triangle.v[2]=piece.v[k+1];if(!meshFaceValid(&triangle))continue;triangles.push_back(triangle);owners.push_back(cells[faceIndex]);if(triangles.size()>(size_t)budget){overflow=1;break;}}
            }
            if(overflow)break;
        }
        if(!overflow)break;if(attempt==15)return 0;
    }
    if(triangles.empty())return 0;
    // Existing coplanar faces can meet at T junctions. Split those triangle edges
    // before sculpting so both sides deform through the same shared positions.
    std::vector<EdVec> points;
    for(const auto &triangle:triangles)for(int k=0;k<3;k++){EdVec p=triangle.v[k].p;int found=0;for(const auto &q:points)if(length(edSub(p,q))<1e-8){found=1;break;}if(!found)points.push_back(p);}
    for(size_t t=0;t<triangles.size();t++){
        int split=0;
        for(int edge=0;edge<3&&!split;edge++){
            EdFace face=triangles[t];EdVertex a=face.v[edge],b=face.v[(edge+1)%3],c=face.v[(edge+2)%3];EdVec d=edSub(b.p,a.p);double square=edDot(d,d),best=1;EdVec point={};
            for(const auto &p:points){double fraction=edDot(edSub(p,a.p),d)/square;if(fraction<=1e-7||fraction>=best-1e-7)continue;if(length(edSub(p,edAdd(a.p,edScale(d,fraction))))<1e-8){best=fraction;point=p;}}
            if(best<1){if(triangles.size()>=(size_t)budget)return 0;EdVertex mid=a;mid.p=point;mid.u=a.u+(b.u-a.u)*best;mid.v=a.v+(b.v-a.v)*best;
                face.v[0]=a;face.v[1]=mid;face.v[2]=c;triangles[t]=face;face.v[0]=mid;face.v[1]=b;triangles.push_back(face);owners.push_back(owners[t]);split=1;}
        }
        if(split)t--; // Recheck both newly split edges, including multiple seams.
    }
    for(size_t sector=0;sector<sectors.size();sector++){
        int mesh=-1,asset=assets[sector];
        for(size_t t=0;t<triangles.size();t++)if(owners[t]==sectors[sector]){
            const EdFace &triangle=triangles[t];
            if(mesh<0||draft.meshes[mesh].geometry.count==160){if(draft.meshCount>=ED_MESHES)return 0;mesh=draft.meshCount++;draft.meshes[mesh]={};draft.meshes[mesh].asset=asset;}
            EdMesh &part=draft.meshes[mesh];int f=part.geometry.count++;part.geometry.f[f]=triangle;
            for(int k=0;k<3;k++){
                EdVec p=triangle.v[k].p,sum={};std::vector<EdVec> normals;
                for(const EdFace &face:faces){EdVec n=edNormal(&face);if(fabs(edDot(n,edSub(p,face.v[0].p)))>1e-7)continue;
                    std::vector<EdFace> one(1,face);if(!terrainInside(one,p,n))continue;
                    int duplicate=0;for(EdVec prev:normals)duplicate|=edDot(prev,n)>.999999;
                    if(!duplicate){normals.push_back(n);sum=edAdd(sum,n);}
                }
                part.terrainNormals[f*3+k]=length(sum)>1e-8?unit(sum):edNormal(&triangle);
                part.terrainPinned[f*3+k]=0;
            }
        }
    }
    // A small inward offset avoids coincident terrain/sector faces. Shared
    // corners use the same blended normal so seams stay welded.
    for(int i=source->meshCount;i<draft.meshCount;i++)for(int f=0;f<draft.meshes[i].geometry.count;f++)for(int v=0;v<3;v++){
        EdVec &p=draft.meshes[i].geometry.f[f].v[v].p;p=edAdd(p,edScale(draft.meshes[i].terrainNormals[f*3+v],.002));
    }
    terrainMapUV(&draft,assets[0]);
    if(!edValidate(&draft))return 0;edCopyModel(out,&draft);return 1;
}
int edTerrainSculpt(EdModel *m,int asset,EdVec center,double radius,double amount,EdVec normal){
    if(asset<0||asset>=m->assetCount||!m->assets[asset].terrain||!isfinite(radius)||radius<ED_GRID||!isfinite(amount)||fabs(amount)>1)return 0;
    if(length(normal)<1e-8){double nearest=1e30;for(int i=0;i<m->meshCount;i++)if(m->meshes[i].asset==asset)for(int f=0;f<m->meshes[i].geometry.count;f++){const EdFace &face=m->meshes[i].geometry.f[f];EdVec mid=edScale(edAdd(edAdd(face.v[0].p,face.v[1].p),face.v[2].p),1.0/3);double distance=length(edSub(mid,center));if(distance<nearest){nearest=distance;normal=edNormal(&face);}}}
    if(!isfinite(length(normal))||length(normal)<1e-8)return 0;normal=unit(normal);
    int changed=0,group=m->assets[asset].terrainGroup;
    struct Move {EdVec from,to;};std::vector<Move> moves;
    // Decide once per shared position, then apply to every duplicated corner.
    for(int i=0;i<m->meshCount;i++)if(m->meshes[i].asset==asset||(group&&m->assets[m->meshes[i].asset].terrain&&m->assets[m->meshes[i].asset].terrainGroup==group)){
        EdMesh &mesh=m->meshes[i];
        for(int f=0;f<mesh.geometry.count;f++)for(int v=0;v<3;v++){
            EdVertex &vertex=mesh.geometry.f[f].v[v];Move *found=NULL;
            for(auto &move:moves)if(length(edSub(move.from,vertex.p))<1e-8){found=&move;break;}
            if(found){vertex.p=found->to;continue;}
            EdVec delta=edSub(vertex.p,center),n=normal;
            if(!mesh.terrainNormals.size())delta=edSub(delta,edScale(normal,edDot(delta,normal)));
            double r=length(delta)/radius,step=r<1?amount*(1-r*r)*(1-r*r):0;
            EdVec next=edAdd(vertex.p,edScale(n,step));int inside=0;for(int s=0;s<m->count&&!inside;s++)inside=edContains(&m->c[s],next);
            if(!inside)next=vertex.p;
            changed|=length(edSub(next,vertex.p))>1e-12;moves.push_back({vertex.p,next});vertex.p=next;
        }
    }
    if(changed)terrainMapUV(m,asset);
    return changed;
}

int edTerrainSmooth(EdModel *m,int asset,EdVec center,double radius,double strength){
    if(asset<0||asset>=m->assetCount||!m->assets[asset].terrain||!isfinite(radius)||radius<ED_GRID||!isfinite(strength)||strength<=0||strength>1||!isfinite(length(center)))return 0;
    struct Node {EdVec position,normal,next;std::vector<int> neighbors;};
    struct Corner {int mesh,face,vertex,node;};
    std::vector<Node> nodes;std::vector<Corner> corners;int group=m->assets[asset].terrainGroup;
    for(int i=0;i<m->meshCount;i++){
        const EdMesh &mesh=m->meshes[i];const EdAsset &a=m->assets[mesh.asset];
        if(mesh.asset!=asset&&(!group||!a.terrain||a.terrainGroup!=group))continue;
        for(int f=0;f<mesh.geometry.count;f++){
            int ids[3];for(int v=0;v<3;v++){
                EdVec p=mesh.geometry.f[f].v[v].p;int id=0;for(;id<(int)nodes.size();id++)if(length(edSub(p,nodes[id].position))<1e-8)break;
                if(id==(int)nodes.size())nodes.push_back({p,mesh.terrainNormals.size()?mesh.terrainNormals[f*3+v]:a.terrainNormal,p,{}});
                ids[v]=id;corners.push_back({i,f,v,id});
            }
            for(int v=0;v<3;v++)for(int w=0;w<3;w++)if(ids[v]!=ids[w]){
                auto &neighbors=nodes[ids[v]].neighbors;if(std::find(neighbors.begin(),neighbors.end(),ids[w])==neighbors.end())neighbors.push_back(ids[w]);
            }
        }
    }
    int changed=0;
    // Average unique connected vertices from the unchanged stroke snapshot.
    // Relax in all directions to untangle pinches, including lateral bunching.
    for(auto &node:nodes){
        double r=length(edSub(node.position,center))/radius;if(r>=1||node.neighbors.empty())continue;
        EdVec average={};for(int neighbor:node.neighbors)average=edAdd(average,nodes[neighbor].position);average=edScale(average,1.0/node.neighbors.size());
        EdVec movement=edScale(edSub(average,node.position),strength*(1-r*r));
        double step=length(movement);EdVec next=edAdd(node.position,movement);int inside=0;for(int s=0;s<m->count&&!inside;s++)inside=edContains(&m->c[s],next);
        if(inside){node.next=next;changed|=fabs(step)>1e-12;}
    }
    if(!changed)return 0;
    EdModel draft;edCopyModel(&draft,m);for(const auto &corner:corners)draft.meshes[corner.mesh].geometry.f[corner.face].v[corner.vertex].p=nodes[corner.node].next;
    terrainMapUV(&draft,asset);
    if(!edMeshValidate(&draft))return 0;edCopyModel(m,&draft);return 1;
}

int edTerrainGrab(EdModel *out,const EdModel *source,int asset,EdVec center,double radius,EdVec delta){
    if(asset<0||asset>=source->assetCount||!source->assets[asset].terrain||!isfinite(radius)||radius<ED_GRID||!isfinite(length(delta)))return 0;
    EdModel draft;edCopyModel(&draft,source);int group=source->assets[asset].terrainGroup;
    for(int i=0;i<draft.meshCount;i++)if(draft.meshes[i].asset==asset||(group&&draft.assets[draft.meshes[i].asset].terrain&&draft.assets[draft.meshes[i].asset].terrainGroup==group)){
        EdMesh &mesh=draft.meshes[i];for(int f=0;f<mesh.geometry.count;f++)for(int v=0;v<3;v++){
            EdVec &p=mesh.geometry.f[f].v[v].p;double r=length(edSub(p,center))/radius;if(r>=1)continue;
            EdVec next=edAdd(p,edScale(delta,(1-r*r)*(1-r*r)));int inside=0;for(int s=0;s<draft.count&&!inside;s++)inside=edContains(&draft.c[s],next);if(inside)p=next;
        }
    }
    terrainMapUV(&draft,asset);
    if(!edMeshValidate(&draft))return 0;edCopyModel(out,&draft);return 1;
}
