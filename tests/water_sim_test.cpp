#include "edWaterSim.h"
#include <cstdio>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static EdPlacedCog conveyor(){
    EdPlacedCog c={};strcpy(c.file,"00_conveyor.cog");c.count=10;
    strcpy(c.params[0].name,"vec0");c.params[0].type=ED_COG_VECTOR;strcpy(c.params[0].value,"(1/0/0)");
    strcpy(c.params[1].name,"speed");c.params[1].type=ED_COG_FLEX;strcpy(c.params[1].value,"2");
    for(int p=2;p<10;p++){snprintf(c.params[p].name,32,"convey%d",p-2);c.params[p].type=ED_COG_SURFACE;c.params[p].cell=c.params[p].face=-1;strcpy(c.params[p].value,"-1");}return c;
}

static double total(const EdWaterSim &sim){double result=0;for(auto &v:sim.voxels)result+=v.volume;return result;}
static void fill(EdWaterSim &sim,double (*height)(const EdWaterSim::Voxel &)){
    for(auto &v:sim.voxels)v.volume=std::clamp((height(v)-v.bottom)*sim.spacing*sim.spacing,0.0,v.capacity);
    sim.summarize();
}
int main(){
    // Stacked wet columns draw only the top surface; dry columns draw nothing.
    {EdWaterSim preview;preview.spacing=.0625;EdWaterSim::Pool p={};p.capacity=1;p.volume=.5;p.center=edV(.03125,.03125,0);p.level=.5;
     preview.pools.push_back(p);p.level=1.5;preview.pools.push_back(p);p.center.x+=.0625;p.volume=0;preview.pools.push_back(p);
     auto tops=preview.previewTops();CHECK(tops.size()==1&&tops[0]==1);}

    auto model=std::make_unique<EdModel>();edDefault(model.get());EdWaterSim sim;std::string error;
    CHECK(sim.start(*model,0,error));CHECK(sim.voxels.size()>1000&&sim.spacing==.0625);
    for(int i=0;i<5;i++)sim.step(.05);
    CHECK(fabs(total(sim)-2)<1e-8&&!sim.finished);
    for(auto &p:sim.pools){CHECK(fabs(p.level-.5)<1e-7);p.flow=edV(.2,.1,0);}
    CHECK(edBakeWaterSim(model.get(),sim,0,conveyor(),error)&&edValidate(model.get())&&model->count==2&&model->cogCount==2);
    int wet=0;for(int s=0;s<model->count;s++)if(model->c[s].flags&2){wet++;CHECK((model->c[s].flags&8)&&fabs(model->c[s].thrust.x-.4)<1e-9&&fabs(model->c[s].thrust.y-.2)<1e-9);CHECK(!strcmp(model->c[s].sound,"00underwater.wav")&&model->c[s].soundVolume==.25&&model->c[s].tint.z==1);}CHECK(wet==1);
    // Wading depths replace metal footsteps with the matching water flag.
    for(int band=0;band<4;band++){
        edDefault(model.get());for(int f=0;f<model->c[0].count;f++){auto &face=model->c[0].f[f];face.attributes=edSurfaceAttributes(&face);face.hasAttributes=1;face.attributes.surfaceFlags|=0x10000;}
        CHECK(sim.start(*model,0,error));double depth=.025+.05*band;
        for(auto &p:sim.pools){p.level=p.pooledLevel=p.bottom+depth;p.volume=p.pooledVolume=depth*p.area;}
        CHECK(edBakeWaterSim(model.get(),sim,0,conveyor(),error,.15));int floors=0;
        for(int c=0;c<model->count;c++)if(model->c[c].flags&2)for(int f=0;f<model->c[c].count;f++){
            auto &face=model->c[c].f[f];if(face.neighbor>=0||edNormal(&face).z<.5)continue;floors++;
            unsigned flags=edSurfaceAttributes(&face).surfaceFlags;
            unsigned expected=band==0?0x40000:band==1?0x20000:band==2?0x100000:0x10000;
            CHECK((flags&0x1f0000)==expected);CHECK(flags&4);
        }CHECK(floors==1);
    }
    // Real voxel gravity: unsupported water falls instead of staying in a
    // sector-wide reservoir or instantly appearing at the floor.
    edDefault(model.get());for(int f=0;f<6;f++)for(int v=0;v<4;v++){model->c[0].f[f].v[v].p.x*=.25;model->c[0].f[f].v[v].p.y*=.25;}
    CHECK(sim.start(*model,0,error));sim.rate=0;
    for(auto &v:sim.voxels)if(v.bottom>=.75)v.volume=v.capacity;
    double before=total(sim),initialMoment=0;for(auto &v:sim.voxels)initialMoment+=v.volume*v.center.z;
    sim.step(.05);double moment=0;for(auto &v:sim.voxels)moment+=v.volume*v.center.z;
    CHECK(moment<initialMoment&&fabs(total(sim)-before)<1e-9);
    for(int i=0;i<50;i++)sim.step(.05);
    for(auto &p:sim.pools)CHECK(fabs(p.level-.25)<.001);
    // A high mound beside low water settles to a level reservoir, conserving
    // volume. Test both X and Y to catch directional sweep bias.
    for(int axis=0;axis<2;axis++){
        CHECK(sim.start(*model,0,error));sim.rate=0;
        for(auto &v:sim.voxels){double h=(axis?v.center.y:v.center.x)<0?.75:.25;v.volume=std::clamp((h-v.bottom)*sim.spacing*sim.spacing,0.0,v.capacity);}
        before=total(sim);
        for(int i=0;i<500;i++)sim.step(.05);
        CHECK(fabs(total(sim)-before)<1e-9);
        for(auto &p:sim.pools)CHECK(fabs(p.level-.5)<.01);
    }
    // A falling stream above a pool must not be used as its surface height.
    edDefault(model.get());CHECK(sim.start(*model,0,error));
    fill(sim,[](const EdWaterSim::Voxel &v){return .25;});
    for(auto &v:sim.voxels)if(v.center.x<-.5&&v.center.y<-.5&&v.bottom>=.75)v.volume=v.capacity;
    sim.summarize();double pooled=0;for(auto &p:sim.pools)pooled+=p.pooledVolume;CHECK(fabs(pooled-1)<1e-8);
    CHECK(edBakeWaterSim(model.get(),sim,0,conveyor(),error)&&model->count==2);
    for(int c=0;c<model->count;c++)if(model->c[c].flags&2)for(int f=0;f<model->c[c].count;f++){
        auto &face=model->c[c].f[f];if(face.neighbor>=0){CHECK(fabs(edNormal(&face).z)>.999999);for(int v=0;v<face.count;v++)CHECK(fabs(face.v[v].p.z-.25)<1e-6);}
    }
    // Closed adjoins remain barriers to voxel neighbors.
    edDefault(model.get());CHECK(edExtrudeStep(model.get(),0,4));
    for(int c=0;c<model->count;c++)for(int f=0;f<model->c[c].count;f++)if(model->c[c].f[f].neighbor>=0){auto &face=model->c[c].f[f];face.attributes=edSurfaceAttributes(&face);face.hasAttributes=1;face.attributes.adjoinFlags&=~2u;}
    CHECK(sim.start(*model,0,error));for(auto &p:sim.pools)CHECK(p.sector==0);
    model->c[0].splineId=1;CHECK(!sim.start(*model,0,error));
    // Two adjoin polygons share the same wall plane. Voxel paths must use
    // the polygon containing their crossing, not the first plane match.
    edDefault(model.get());EdCell portalBox=model->c[0];model->count=3;model->spawnCount=0;
    for(int c=0;c<3;c++){model->c[c]=portalBox;for(int f=0;f<6;f++)for(int v=0;v<4;v++){
        auto &p=model->c[c].f[f].v[v].p;double x=(p.x+1)*.5;p.x=c?x:x-1;p.y*=.25;if(c)p.z=p.z*.5+(c==2?.5:0);
    }}CHECK(edResolve(model.get())&&sim.start(*model,0,error));
    CHECK(sim.pathOpen(0,edV(-.1,0,.25),1,edV(.1,0,.25)));
    CHECK(sim.pathOpen(0,edV(-.1,0,.75),2,edV(.1,0,.75)));
    // High source -> sloped channel -> wider low basin. No remote injection.
    edDefault(model.get());EdCell box=model->c[0];model->count=3;
    for(int c=0;c<3;c++){model->c[c]=box;for(int f=0;f<box.count;f++)for(int v=0;v<box.f[f].count;v++){
        auto &p=model->c[c].f[f].v[v].p;double x=p.x,y=(p.y+1)*.5,z=p.z;
        if(c==0)p=edV(x*.125,-.25+y*.25,1+z*.25);
        if(c==1)p=edV(x*.125,y*2,1-y+z*.25);
        if(c==2)p=edV(x,2+y,-.5+z*.75);
    }}model->spawnCount=0;CHECK(edResolve(model.get())&&edValidate(model.get()));
    CHECK(sim.start(*model,0,error));int firstChannel=-1,firstRoom=-1;
    for(int tick=0;tick<1000&&firstRoom<0;tick++){
        sim.step(.05);for(auto &p:sim.pools)if(p.volume>p.capacity*.001){
            if(p.sector==1&&firstChannel<0)firstChannel=tick;
            if(p.sector==2&&firstRoom<0)firstRoom=tick;
        }
    }
    CHECK(firstChannel>=0&&firstRoom>firstChannel+2);CHECK(fabs(total(sim)-sim.added)<1e-8);
    for(int i=0;i<100;i++)sim.step(.05);
    auto aqueduct=std::make_unique<EdModel>(*model);
    CHECK(edBakeWaterSim(model.get(),sim,0,conveyor(),error)&&edValidate(model.get())&&model->count<=6);
    *model=*aqueduct;CHECK(sim.start(*model,0,error));
    for(auto &v:sim.voxels){auto &p=sim.pools[v.pool];double h=p.sector==2?.125:p.bottom+.1;
        v.volume=std::clamp((h-v.bottom)*sim.spacing*sim.spacing,0.0,v.capacity);}
    sim.summarize();CHECK(edBakeWaterSim(model.get(),sim,0,conveyor(),error)&&edValidate(model.get())&&model->count<=6);
    bool ramp=false,basin=false;
    for(int c=0;c<model->count;c++)if(model->c[c].flags&2)for(int f=0;f<model->c[c].count;f++){
        auto &face=model->c[c].f[f];if(face.neighbor<0||model->c[face.neighbor].flags&2)continue;
        EdVec center={};for(int v=0;v<face.count;v++)center=edAdd(center,face.v[v].p);center=edScale(center,1.0/face.count);EdVec n=edNormal(&face);
        if(center.y>2){CHECK(fabs(n.z)>.999999);basin=true;}
        if(center.y>0&&center.y<2&&fabs(n.z)>.5&&fabs(n.z)<.999)ramp=true;
    }CHECK(ramp&&basin);
    // Inclined portal: a stream above it and a lower pool must retain
    // different planes. Matching the entire portal would erase the stream.
    edDefault(model.get());EdBuffer<unsigned char> selected;selected[0]=1;
    CHECK(edCleaveSectorsPlane(model.get(),selected,edV(.5,0,1),edV(0,0,.5)));
    CHECK(sim.start(*model,0,error));
    for(auto &p:sim.pools){double h=p.sector==0?p.bottom+.06:.25;
        p.level=std::clamp(h,p.bottom,p.top);p.pooledLevel=p.level;p.volume=p.pooledVolume=(p.level-p.bottom)*p.area;
    }
    CHECK(edBakeWaterSim(model.get(),sim,0,conveyor(),error)&&edValidate(model.get())&&model->count==4);
    bool inclinedStream=false;
    for(int c=0;c<model->count;c++)if(model->c[c].flags&2)for(int f=0;f<model->c[c].count;f++){
        auto &face=model->c[c].f[f];if(face.neighbor<0||model->c[face.neighbor].flags&2)continue;
        double planeHeight=0;for(int v=0;v<face.count;v++)planeHeight+=face.v[v].p.z+.5*face.v[v].p.x;planeHeight/=face.count;
        EdVec n=edNormal(&face);if(fabs(n.z)>.5&&fabs(n.z)<.99&&planeHeight>.5+ED_EPS){
            for(int v=0;v<face.count;v++)CHECK(face.v[v].p.z+.5*face.v[v].p.x>.5+ED_EPS);
            inclinedStream=true;
        }
    }CHECK(inclinedStream);
    // Surface fitting still makes only one cleave per original sector and no
    // vertical riser on a shared portal, despite initially unequal heights.
    edDefault(model.get());CHECK(edExtrude(model.get(),0,4,edV(0,2,0)));CHECK(sim.start(*model,0,error));
    fill(sim,[](const EdWaterSim::Voxel &v){return v.center.y<1?.65:.35;});
    CHECK(edBakeWaterSim(model.get(),sim,0,conveyor(),error)&&edValidate(model.get())&&model->count==4);
    for(int c=0;c<model->count;c++)if(model->c[c].flags&2)for(int f=0;f<model->c[c].count;f++){
        auto &face=model->c[c].f[f];if(face.neighbor>=0&&!(model->c[face.neighbor].flags&2))CHECK(fabs(edNormal(&face).z)>.5);
    }
    // Thin water on two ramps: sampled planes may extrapolate below the
    // floor at their join. Baking must cover that join without a dry strip.
    edDefault(model.get());box=model->c[0];model->count=2;model->spawnCount=0;
    for(int c=0;c<2;c++){model->c[c]=box;for(int f=0;f<6;f++)for(int v=0;v<4;v++){
        auto &p=model->c[c].f[f].v[v].p;double x=(p.x+1)*.5+c,bottom=c?-.4-1.6*(x-1):-.4*x;
        p=edV(x,p.y*.5,bottom+p.z*(2-bottom));
    }}CHECK(edResolve(model.get())&&sim.start(*model,0,error));
    for(auto &p:sim.pools){double depth=.003+.008*(p.center.x-(p.sector+.5));
        p.pooledLevel=p.level=p.bottom+fmax(.0001,depth);p.volume=p.pooledVolume=(p.level-p.bottom)*p.area;p.flow=edV(.1,0,-.1);
    }
    CHECK(edBakeWaterSim(model.get(),sim,0,conveyor(),error)&&edValidate(model.get())&&model->count==4);
    int joins=0;double joinHeight=0;
    for(int c=0;c<model->count;c++)if(model->c[c].flags&2)for(int f=0;f<model->c[c].count;f++){
        auto &face=model->c[c].f[f];if(face.neighbor<0||model->c[face.neighbor].flags&2)continue;
        CHECK(face.customUV);
        EdVec edge=edSub(face.v[1].p,face.v[0].p),across=edCross(edNormal(&face),edge);
        for(int v=2;v<face.count;v++){
            auto &a=face.v[0];auto &b=face.v[1];auto &d=face.v[v];
            double determinant=(b.u-a.u)*(d.v-a.v)-(b.v-a.v)*(d.u-a.u);
            CHECK(determinant*edDot(edSub(d.p,a.p),across)>=-1e-8);
        }
        for(int v=0;v<face.count;v++)if(fabs(face.v[v].p.x-1)<ED_EPS){
            CHECK(face.v[v].p.z>-.4);if(joins)CHECK(fabs(face.v[v].p.z-joinHeight)<ED_EPS);joinHeight=face.v[v].p.z;joins++;
        }
    }CHECK(joins>=4);
    edDefault(model.get());CHECK(sim.start(*model,0,error));
    for(int i=0;i<30&&!sim.finished;i++)sim.step(.05);CHECK(sim.finished&&fabs(total(sim)-4)<1e-8);
    puts("PASS: voxel gravity, X/Y leveling, conservation, blocked portals, aqueduct traversal, completion and seam-matched single-plane baking");return 0;
}
