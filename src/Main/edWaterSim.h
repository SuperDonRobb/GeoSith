#ifndef ED_WATER_SIM_H
#define ED_WATER_SIM_H
#include "edGeometry.h"
#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <cmath>

// Sparse, fractional-volume voxels. No simulation cells are added to the level.
struct EdWaterSim {
    struct Pool {int sector;double bottom,top,volume=0,capacity=0,level=0,area=0,pooledVolume=0,pooledLevel=0;EdVec center={},flow={};};
    struct Voxel {int pool,x,y,z;double bottom,top,volume=0,capacity=0;EdVec center;};
    struct Link {int a,b;bool vertical;EdVec direction;};
    struct Key {int x,y,z;bool operator==(const Key &b)const{return x==b.x&&y==b.y&&z==b.z;}};
    struct Hash {size_t operator()(Key p)const{return (size_t)(unsigned)p.x*73856093u^(size_t)(unsigned)p.y*19349663u^(size_t)(unsigned)p.z*83492791u;}};
    struct Plane {EdVec normal,point;int neighbor,face;bool open;};
    std::unique_ptr<EdModel> geometry;
    std::vector<Pool> pools;std::vector<Voxel> voxels;std::vector<Link> links;
    std::vector<std::vector<Plane>> planes;
    std::vector<int> injectors,supportOrder;
    std::vector<std::vector<int>> below;
    std::vector<unsigned char> supported;
    std::vector<double> delta,out,in,amounts;
    std::vector<EdVec> flow;
    int source=-1;double spacing=.0625,rate=0,stalled=0,added=0,initialVolume=0;bool finished=false;
    bool pathOpen(int sector,EdVec a,int destination,EdVec b)const{
        if(sector==destination)return true;
        EdVec direction=edSub(b,a);double start=-1e-7;
        for(int step=0;step<128;step++){
            if(sector==destination)return true;
            const Plane *exit=nullptr;double nearest=2;
            for(auto &p:planes[sector]){double velocity=edDot(p.normal,direction);if(velocity>=-1e-12)continue;
                double t=-edDot(p.normal,edSub(a,p.point))/velocity;
                if(t>=start-1e-8&&t<=nearest+1e-10){
                    EdVec hit=edAdd(a,edScale(direction,t));const auto &face=geometry->c[sector].f[p.face];bool contains=true;
                    for(int v=0;v<face.count;v++){EdVec edge=edSub(face.v[(v+1)%face.count].p,face.v[v].p);
                        if(edDot(edCross(edge,edSub(hit,face.v[v].p)),p.normal)<-1e-9*sqrt(edDot(edge,edge))){contains=false;break;}}
                    if(contains&&(t<nearest-1e-10||!exit||p.open)){nearest=t;exit=&p;}
                }
            }
            if(!exit||nearest>1+1e-8||!exit->open)return false;
            sector=exit->neighbor;start=nearest+1e-7;
        }return false;
    }
    // Preview only the highest wet surface at each XY grid coordinate, even
    // when several sectors/voxel columns are stacked there.
    std::vector<int> previewTops() const {
        std::unordered_map<Key,int,Hash> tops;
        for(int i=0;i<(int)pools.size();i++){const auto &p=pools[i];if(p.volume<=p.capacity*1e-6)continue;
            Key key={(int)std::floor(p.center.x/spacing),(int)std::floor(p.center.y/spacing),0};
            auto found=tops.find(key);if(found==tops.end()||p.level>pools[found->second].level)tops[key]=i;
        }
        std::vector<int> result;result.reserve(tops.size());for(auto &entry:tops)result.push_back(entry.second);
        std::sort(result.begin(),result.end());return result;
    }
    void summarize(){
        for(auto &p:pools){p.volume=p.pooledVolume=0;p.level=p.pooledLevel=p.bottom;}
        for(auto &v:voxels){auto &p=pools[v.pool];p.volume+=v.volume;
            if(v.volume>v.capacity*1e-5)p.level=fmax(p.level,v.bottom+v.volume/p.area);
        }
        // A falling voxel is not a pool surface. Trace support from solid
        // floors upward, including through horizontal sector adjoins.
        for(int i:supportOrder){auto &v=voxels[i];bool grounded=below[i].empty();
            for(int b:below[i])grounded|=supported[b]&&voxels[b].volume>=voxels[b].capacity*.98;
            supported[i]=grounded;
            if(grounded&&v.volume>v.capacity*1e-5){auto &p=pools[v.pool];p.pooledVolume+=v.volume;p.pooledLevel=fmax(p.pooledLevel,v.bottom+v.volume/p.area);}
        }
    }
    bool start(const EdModel &model,int sector,std::string &error){
        *this=EdWaterSim();if(sector<0||sector>=model.count){error="Select one source sector.";return false;}
        geometry=std::make_unique<EdModel>(model);source=sector;planes.resize(model.count);
        std::vector<int> queue{sector};std::vector<unsigned char> reachable(model.count);reachable[sector]=1;
        for(size_t i=0;i<queue.size();i++){
            auto &cell=model.c[queue[i]];if(cell.splineId){error="Convert connected editable splines to sectors before simulating water.";return false;}
            for(int f=0;f<cell.count;f++){auto &face=cell.f[f];auto attr=edSurfaceAttributes(&face);int n=face.neighbor;
                bool open=n>=0&&n<model.count&&(attr.adjoinFlags&2)&&!(attr.surfaceFlags&4);
                planes[queue[i]].push_back({edNormal(&face),face.v[0].p,n,f,open});
                if(open&&!reachable[n]){reachable[n]=1;queue.push_back(n);}
            }
        }
        struct Bounds {EdVec lo,hi;};std::vector<Bounds> bounds(model.count);double estimate=0;
        for(int s:queue){auto &box=bounds[s];box.lo=edV(1e30,1e30,1e30);box.hi=edV(-1e30,-1e30,-1e30);
            for(int f=0;f<model.c[s].count;f++)for(int v=0;v<model.c[s].f[f].count;v++){auto p=model.c[s].f[f].v[v].p;
                box.lo=edV(fmin(box.lo.x,p.x),fmin(box.lo.y,p.y),fmin(box.lo.z,p.z));box.hi=edV(fmax(box.hi.x,p.x),fmax(box.hi.y,p.y),fmax(box.hi.z,p.z));}
            EdVec size=edSub(box.hi,box.lo);estimate+=size.x*size.y*size.z;
        }
        spacing=fmax(.0625,cbrt(estimate/131072.0));double area=spacing*spacing;
        std::unordered_map<Key,std::vector<int>,Hash> grid;
        for(int s:queue){auto box=bounds[s];
            for(int x=(int)floor(box.lo.x/spacing);x<(int)ceil(box.hi.x/spacing);x++)for(int y=(int)floor(box.lo.y/spacing);y<(int)ceil(box.hi.y/spacing);y++){
                double px=(x+.5)*spacing,py=(y+.5)*spacing,lo=box.lo.z,hi=box.hi.z;bool inside=true;
                for(auto &p:planes[s]){double d=edDot(p.normal,edSub(edV(px,py,0),p.point));
                    if(fabs(p.normal.z)<1e-10){if(d<-1e-8){inside=false;break;}}
                    else if(p.normal.z>0)lo=fmax(lo,-d/p.normal.z);else hi=fmin(hi,-d/p.normal.z);
                }
                if(!inside||hi-lo<1e-8)continue;
                int pool=(int)pools.size();Pool p={};p.sector=s;p.bottom=p.level=lo;p.top=hi;p.area=area;p.capacity=(hi-lo)*area;p.center=edV(px,py,(lo+hi)*.5);pools.push_back(p);
                for(int z=(int)floor(lo/spacing);z<(int)ceil(hi/spacing);z++){
                    Voxel v={};v.pool=pool;v.x=x;v.y=y;v.z=z;v.bottom=fmax(lo,z*spacing);v.top=fmin(hi,(z+1)*spacing);
                    if(v.top-v.bottom<1e-8)continue;
                    v.center=edV(px,py,(v.bottom+v.top)*.5);v.capacity=(v.top-v.bottom)*area;
                    if(model.c[s].flags&2){v.volume=v.capacity;initialVolume+=v.volume;}
                    if(s==source)injectors.push_back((int)voxels.size());
                    grid[{x,y,z}].push_back((int)voxels.size());voxels.push_back(v);
                    if(voxels.size()>524288){error="Water simulation exceeds the voxel memory budget.";return false;}
                }
            }
        }
        if(injectors.empty()){error="Source is narrower than the water voxel grid.";return false;}
        auto connect=[&](int a,int b){auto &va=voxels[a];auto &vb=voxels[b];int sa=pools[va.pool].sector,sb=pools[vb.pool].sector;
            if(!pathOpen(sa,va.center,sb,vb.center)||!pathOpen(sb,vb.center,sa,va.center))return;
            EdVec d=edSub(vb.center,va.center);double length=sqrt(edDot(d,d));if(length<1e-10)return;
            bool vertical=va.x==vb.x&&va.y==vb.y;
            if(vertical&&va.center.z<vb.center.z){std::swap(a,b);d=edScale(d,-1);}
            links.push_back({a,b,vertical,edScale(d,1/length)});
        };
        for(auto &entry:grid){auto key=entry.first;auto &list=entry.second;
            for(size_t i=0;i<list.size();i++)for(size_t j=i+1;j<list.size();j++)connect(list[i],list[j]);
            for(Key next: {Key{key.x+1,key.y,key.z},Key{key.x,key.y+1,key.z},Key{key.x,key.y,key.z+1}}){
                auto it=grid.find(next);if(it==grid.end())continue;for(int a:list)for(int b:it->second)connect(a,b);
            }
        }
        std::sort(links.begin(),links.end(),[&](const Link&a,const Link&b){if(a.vertical!=b.vertical)return a.vertical>b.vertical;return voxels[a.b].bottom<voxels[b.b].bottom;});
        std::sort(injectors.begin(),injectors.end(),[&](int a,int b){return voxels[a].z<voxels[b].z;});
        double sourceVolume=0;for(int i:injectors)sourceVolume+=voxels[i].capacity;
        rate=fmax(.125,sourceVolume*2);delta.resize(voxels.size());out.resize(voxels.size());in.resize(voxels.size());flow.resize(pools.size());amounts.resize(links.size());
        below.resize(voxels.size());supported.resize(voxels.size());
        for(auto &link:links)if(link.vertical)below[link.a].push_back(link.b);
        for(int i=0;i<(int)voxels.size();i++)supportOrder.push_back(i);
        std::sort(supportOrder.begin(),supportOrder.end(),[&](int a,int b){return voxels[a].bottom<voxels[b].bottom;});
        summarize();return true;
    }
    void step(double dt){
        if(finished||source<0)return;dt=std::clamp(dt,0.0,.05);if(dt<=0)return;
        double budget=rate*dt,inject=0;
        // Supply the lowest available source layer uniformly, not every sector
        // column at once. Water elsewhere is moved only by neighboring voxels.
        for(size_t first=0;first<injectors.size()&&budget>1e-15;){size_t end=first;double space=0;int layer=voxels[injectors[first]].z;
            while(end<injectors.size()&&voxels[injectors[end]].z==layer){auto &v=voxels[injectors[end++]];space+=v.capacity-v.volume;}
            double amount=fmin(budget,space);if(space>0)for(size_t i=first;i<end;i++){auto &v=voxels[injectors[i]];v.volume+=amount*(v.capacity-v.volume)/space;}
            budget-=amount;inject+=amount;first=end;
        }added+=inject;
        double moved=0;std::fill(flow.begin(),flow.end(),edV(0,0,0));
        auto record=[&](const Link &link,double amount){EdVec d=edScale(link.direction,amount/fmax(dt*spacing*spacing,1e-12));
            flow[voxels[link.a].pool]=edAdd(flow[voxels[link.a].pool],d);flow[voxels[link.b].pool]=edAdd(flow[voxels[link.b].pool],d);moved+=fabs(amount);};
        // Downward transfers precede sideways equalization. Low-to-high order
        // limits falling water to one vertical voxel per relaxation sweep.
        for(int sweep=0;sweep<4;sweep++){
            for(auto &l:links)if(l.vertical){auto &a=voxels[l.a];auto &b=voxels[l.b];if(a.volume<=0||b.volume>=b.capacity)continue;double amount=fmin(a.volume,b.capacity-b.volume)*fmin(1.0,dt*40);
                a.volume-=amount;b.volume+=amount;if(amount>0)record(l,amount);}
            std::fill(delta.begin(),delta.end(),0);std::fill(out.begin(),out.end(),0);std::fill(in.begin(),in.end(),0);std::fill(amounts.begin(),amounts.end(),0);
            for(size_t i=0;i<links.size();i++){auto &l=links[i];if(l.vertical)continue;auto &a=voxels[l.a];auto &b=voxels[l.b];if((a.volume<=0&&b.volume<=0)||(a.volume>=a.capacity&&b.volume>=b.capacity))continue;
                double ha=a.bottom+a.volume/(spacing*spacing),hb=b.bottom+b.volume/(spacing*spacing);
                int donor=ha>hb?l.a:l.b,receiver=ha>hb?l.b:l.a;auto &d=voxels[donor];auto &r=voxels[receiver];
                double amount=fmin(d.volume,fmin(r.capacity-r.volume,fabs(ha-hb)*spacing*spacing*.5))*fmin(1.0,dt*20);
                amounts[i]=donor==l.a?amount:-amount;out[donor]+=amount;in[receiver]+=amount;
            }
            for(size_t i=0;i<links.size();i++)if(amounts[i]){auto &l=links[i];int d=amounts[i]>0?l.a:l.b,r=amounts[i]>0?l.b:l.a;
                double amount=fabs(amounts[i])*fmin(1.0,fmin(voxels[d].volume/fmax(out[d],1e-30),(voxels[r].capacity-voxels[r].volume)/fmax(in[r],1e-30)));
                delta[d]-=amount;delta[r]+=amount;record(l,amounts[i]>0?amount:-amount);
            }
            for(size_t i=0;i<voxels.size();i++)voxels[i].volume=std::clamp(voxels[i].volume+delta[i],0.0,voxels[i].capacity);
        }
        summarize();bool full=true;
        for(size_t i=0;i<pools.size();i++){auto &p=pools[i];full&=p.capacity-p.volume<=p.capacity*1e-5;
            if(edDot(flow[i],flow[i])>1e-12)p.flow=edAdd(edScale(p.flow,.85),edScale(flow[i],.15));}
        if(inject+moved<1e-10)stalled+=dt;else stalled=0;
        finished=full||stalled>=2;
    }
};
int edBakeWaterSim(EdModel *model,const EdWaterSim &sim,int material,const EdPlacedCog &conveyor,std::string &error,double standingHeight=.15);
#endif
