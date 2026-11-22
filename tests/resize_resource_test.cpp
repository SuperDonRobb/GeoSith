#include "../engine/src/Main/edResizeResource.h"
#undef NDEBUG
#include <cassert>
#include <iostream>
int main(){using namespace EdResize;
 assert(templateName("kelldragon",8)=="kelldragon_8size");
 assert(templateName("kelldragon",.5)=="kelldragon_0p5size");
 assert(templateName("kelldragon",8,2)=="kelldragon_8size_2");
 assert(templateName(std::string(63,'a'),.5,2).size()==63);
 auto cog=pickupCog("symbols\ntemplate drop = +gun local # keep\ntemplate limb=+limb local\nend\ncode\nCreateThing(drop, victim);\nend\n",[](const std::string &s){return s=="+gun"?"_gun_8size":s;});
 assert(cog.find("template drop = _gun_8size local # keep")!=std::string::npos);
 assert(cog.find("template limb=+limb local")!=std::string::npos);
 assert(cog.find("CreateThing(drop, victim);")!=std::string::npos);
 assert(blastScale(8)==4&&blastScale(2)==2&&blastScale(.5)==.5);
 assert(std::fabs(groundHeight(0,.1,.05,8)-.8)<1e-9);
 assert(std::fabs(groundHeight(0,0,.05,8)-.405)<1e-9);
 assert(std::fabs(groundHeight(.2,0,.05,.5)-.1)<1e-9);
 std::string wav(48,'\0');auto put=[&](int p,unsigned v,int n){for(int i=0;i<n;i++)wav[p+i]=char(v>>(8*i));};
 memcpy(wav.data(),"RIFF",4);put(4,40,4);memcpy(wav.data()+8,"WAVEfmt ",8);put(16,16,4);put(20,1,2);put(22,1,2);put(24,22050,4);put(28,44100,4);put(32,2,2);put(34,16,2);memcpy(wav.data()+36,"data",4);put(40,4,4);wav[44]=1;wav[45]=2;wav[46]=3;wav[47]=4;
 auto read=[](const std::string &b,int p){unsigned n=0;for(int i=0;i<4;i++)n|=unsigned((unsigned char)b[p+i])<<(8*i);return n;};
 auto slow=wave(wav,2),fast=wave(wav,.5);assert(read(slow,24)==11025&&read(slow,28)==22050&&read(fast,24)==44100);assert(slow.substr(32)==wav.substr(32));
 assert(soundClass("fire1 laser.wav 0x40 .5 2.5 .8 # volume\nidle none\n",[](const std::string&){return "scaled.wav";})=="fire1 scaled.wav 0x40 .5 2.5 .8 # volume\nidle none\n");
 assert(wave(wav,8)==wave(wav,4));
 assert(transform("FPS 32\n",".key",8)=="FPS 8\n");
 assert(soundClass("fire1 laser.wav\n",[](const std::string&){return "scaled.wav";},8)=="fire1 scaled.wav 0x40 4 20\n");
 assert(soundClass("fire1 laser.wav 0x40 .5 2.5 .8\n",[](const std::string&){return "scaled.wav";},.5)=="fire1 scaled.wav 0x40 0.25 1.25 .8\n");
 std::string lod="RADIUS 1\nGEOSETS 2\nGEOSET 0\nVERTICES 1\n0: 1 2 3\nGEOSET 1\nVERTICES 1\n0: 9 9 9\nSECTION: HIERARCHYDEF\nHIERARCHY NODES 0\n";
 auto enlarged=transform(lod,".3do",2);assert(enlarged.find("GEOSETS 1")!=std::string::npos&&enlarged.find("GEOSET 1")==std::string::npos&&enlarged.find("SECTION: HIERARCHYDEF")!=std::string::npos&&enlarged.find("0: 2 4 6")!=std::string::npos);
 assert(transform(lod,".3do",.5).find("GEOSET 1")!=std::string::npos);
 bool badWave=false;try{wave(wav.substr(0,40),2);}catch(...){badWave=true;}assert(badWave);

 auto dragonAI=ai("alignment=-1, maxstep=0.4, sightdist=4, heardist=2\nPrimaryFire 1500, 0.7, 0.25, 0, 0, 200\nFollow 0, 0.10\nTalk 10000, 0.10\n",8);
 assert(dragonAI.find("PrimaryFire 1500 0.7 2 0 0 200")!=std::string::npos);
 assert(dragonAI.find("Follow 0 0.8")!=std::string::npos);
 assert(dragonAI.find("Talk 10000, 0.10")!=std::string::npos);
 assert(dragonAI.find("sightdist=32")!=std::string::npos);
 auto m=transform("RADIUS 2\nINSERT OFFSET 1 2 3\nVERTICES 1\n0: 1 2 3 .5\nTEXTURE VERTICES 1\n0: 4 5\nVERTEX NORMALS\n0: 0 0 1\nHIERARCHY NODES 1\n0: 0 0 0 -1 -1 -1 0 1 2 3 10 20 30 4 5 6 root\n",".3do",2);
 assert(m.find("0: 2 4 6 .5")!=std::string::npos);assert(m.find("0: 4 5\nVERTEX NORMALS\n0: 0 0 1")!=std::string::npos);assert(m.find("0 2 4 6 10 20 30 8 10 12 root")!=std::string::npos);
 assert(transform("FPS 30\n",".key",.5)=="FPS 60\n");
 auto k=transform("FPS 30\nENTRIES 1\n0: 5 0x3 1 2 3 90 45 10\n.1 .2 .3 1 2 3\n",".key",2);
 assert(k=="FPS 15\nENTRIES 1\n0: 5 0x3 2 4 6 90 45 10\n0.2 0.4 0.6 1 2 3\n");
 assert(transform("a.mat 0 1 2 4 3 1 0 3 4 5\n",".spr",.5)=="a.mat 0 0.5 1 4 3 1 0 1.5 2 2.5\n");
 assert(transform("mode=0\nstand a.key 0 0 1 # keep\njoints\n0=1\nend\n",".pup",2,[](const std::string&){return "new.key";}).find("stand new.key 0 0 1 # keep")!=std::string::npos);
 auto p=transform("SIZE 1\nRADIUS 2\nVERTICES 1\n0: 1 2 3 7\n",".par",3);assert(p.find("0: 3 6 9 7")!=std::string::npos);
 for(auto input:{"VERTICES 2\n0: 1 2 3\n","VERTICES 1\n0: NaN 2 3\n"}){bool rejected=false;try{transform(input,".3do",2);}catch(...){rejected=true;}assert(rejected);}
 bool rejected=false;try{transform("SIZE 1\n",".par",0);}catch(...){rejected=true;}assert(rejected);
 std::cout<<"Resize spatial transforms, UVs, animation timing, sprites, particles and malformed input passed\n";
}
