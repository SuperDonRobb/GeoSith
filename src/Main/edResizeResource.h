#ifndef ED_RESIZE_RESOURCE_H
#define ED_RESIZE_RESOURCE_H
#include <string>
#include <vector>
#include <sstream>
#include <functional>
#include <cmath>
#include <stdexcept>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstdint>
#include <cstring>
namespace EdResize {
// Match JK floor attachment: explicit height, otherwise model insert offset
// with a move-size minimum. The engine's .005 clearance stays unscaled.
inline double groundHeight(double height,double insertOffset,double moveSize,double factor){
 return height!=0?height*factor:std::fmax(insertOffset*factor,moveSize*factor+.005);
}
inline double blastScale(double factor){return factor>4.0?4.0:factor;}
inline double timingScale(double factor){return factor>4.0?4.0:factor;}
inline std::string lower(std::string s){for(char &c:s)c=(char)std::tolower((unsigned char)c);return s;}
inline std::vector<std::string> tokens(const std::string &line){std::istringstream in(line.substr(0,line.find('#')));std::vector<std::string> t;std::string s;while(in>>s)t.push_back(s);return t;}
inline std::string number(double v){if(!std::isfinite(v)||std::fabs(v)>1e30)throw std::runtime_error("Resize exceeds numeric range");char s[64];snprintf(s,sizeof(s),"%.9g",v);return s;}
inline void scale(std::vector<std::string>&t,size_t at,size_t count,double factor){if(at+count>t.size())throw std::runtime_error("Incomplete resize resource");for(size_t i=at;i<at+count;i++){char *end;double v=strtod(t[i].c_str(),&end);if(end==t[i].c_str()||*end)throw std::runtime_error("Invalid coordinate in resize resource");t[i]=number(v*factor);}}
// Keep generated template names recognizable and within the engine's 64-byte field.
inline std::string templateName(const std::string &base,double factor,unsigned duplicate=0){
 std::string value=number(factor);for(char &c:value)if(c=='.')c='p';else if(c=='+')c='p';else if(c=='-')c='m';
 std::string suffix="_"+value+"size";if(duplicate)suffix+="_"+std::to_string(duplicate);
 std::string stem=base;for(char &c:stem)if(!std::isalnum((unsigned char)c)&&c!='_')c='_';
 return stem.substr(0,63-suffix.size())+suffix;
}
// AI arguments mix distances, angles, probabilities and milliseconds. Only
// documented spatial arguments are scaled; unknown instincts remain intact.
inline std::string ai(const std::string &source,double factor){
 if(!std::isfinite(factor)||factor<=0)throw std::runtime_error("Invalid AI scale");
 std::istringstream input(source);std::string line,out;
 while(std::getline(input,line)){
  std::string data=line.substr(0,line.find('#'));for(char &c:data)if(c==',')c=' ';
  auto t=tokens(data);bool changed=false;
  if(!t.empty()){
   for(auto &s:t){auto eq=s.find('=');if(eq==std::string::npos)continue;auto key=lower(s.substr(0,eq));
    if(key=="maxstep"||key=="sightdist"||key=="heardist"){std::vector<std::string> value{s.substr(eq+1)};scale(value,0,1,factor);s=s.substr(0,eq+1)+value[0];changed=true;}}
   auto name=lower(t[0]);std::vector<int> positions;
   if(name=="primaryfire")positions={3,5};
   else if(name=="follow")positions={1,2,3};
   else if(name=="lobfire")positions={3,4};
   else if(name=="turretfire")positions={5};
   for(int index:positions)if(index<(int)t.size()){scale(t,index,1,factor);changed=true;}
  }
  if(changed){for(size_t i=0;i<t.size();i++){if(i)out+=' ';out+=t[i];}auto comment=line.find('#');if(comment!=std::string::npos)out+=' '+line.substr(comment);out+='\n';}else out+=line+"\n";
 }return out;
}
// SND has no pitch field. Keep the PCM samples and change their declared
// playback rate; stock JK then plays the copied WAV at the desired pitch/speed.
inline std::string wave(std::string bytes,double factor){
 if(!std::isfinite(factor)||factor<=0||bytes.size()<44||memcmp(bytes.data(),"RIFF",4)||memcmp(bytes.data()+8,"WAVE",4))throw std::runtime_error("Invalid resize WAV");
 auto get=[&](size_t p,int n){uint32_t v=0;for(int i=0;i<n;i++)v|=uint32_t((unsigned char)bytes.at(p+i))<<(8*i);return v;};
 auto put=[&](size_t p,uint32_t v){for(int i=0;i<4;i++)bytes.at(p+i)=(char)(v>>(8*i));};
 size_t end=size_t(get(4,4))+8;if(end>bytes.size())throw std::runtime_error("Truncated resize WAV");bool format=false,data=false;
 for(size_t p=12;p+8<=end;){size_t n=get(p+4,4),start=p+8;if(n>end-start)throw std::runtime_error("Invalid WAV chunk");
  if(!memcmp(bytes.data()+p,"fmt ",4)){
   if(format||p!=12||n<16||get(start,2)!=1)throw std::runtime_error("Resizing requires a PCM WAV with a leading format chunk");
   auto channels=get(start+2,2),rate=get(start+4,4),align=get(start+12,2),bits=get(start+14,2);
   if((channels!=1&&channels!=2)||(bits!=8&&bits!=16)||align!=channels*bits/8||!rate)throw std::runtime_error("Unsupported resize WAV format");
   double scaled=std::round(rate/timingScale(factor));
   // JK's duration calculation divides by sampleRate/1000.
   if(scaled<1000||scaled>384000)throw std::runtime_error("Sound scale exceeds supported WAV playback rate (1000-384000 Hz)");
   put(start+4,(uint32_t)scaled);put(start+8,(uint32_t)scaled*align);format=true;
  }else if(!memcmp(bytes.data()+p,"data",4))data=n>0;
  p=start+n+(n&1);
 }
 if(!format||!data)throw std::runtime_error("Missing WAV format or audio data");return bytes;
}
inline std::string soundClass(const std::string &source,const std::function<std::string(const std::string&)> &copy,double factor=1.0){
 std::istringstream input(source);std::string line,out;
 while(std::getline(input,line)){auto t=tokens(line);if(t.size()<2||lower(t[1])=="none"){out+=line+"\n";continue;}
  t[1]=copy(t[1]);if(factor!=1.0){if(t.size()<3)t.push_back("0x40");if(t.size()<4)t.push_back("0.5");if(t.size()<5)t.push_back("2.5");scale(t,3,2,factor);}for(size_t i=0;i<t.size();i++){if(i)out+=' ';out+=t[i];}auto comment=line.find('#');if(comment!=std::string::npos)out+=' '+line.substr(comment);out+='\n';
 }return out;
}
// Rewrite initialized template symbols only; keep actor behavior and non-item
// dependencies untouched. Works for death drops and Force Pull alike.
inline std::string pickupCog(const std::string &source,const std::function<std::string(const std::string&)> &copy){
 std::istringstream input(source);std::string line,out;bool symbols=false;
 while(std::getline(input,line)){
  auto t=tokens(line);
  if(!t.empty()){
   auto type=lower(t[0]);if(type=="symbols")symbols=true;else if(type=="end")symbols=false;
   else if(symbols&&type=="template"){
    auto eq=line.find('=');auto comment=line.find('#');
    if(eq!=std::string::npos&&eq<comment){auto begin=line.find_first_not_of(" \t",eq+1);
     if(begin!=std::string::npos){auto end=line.find_first_of(" \t\r#",begin);if(end==std::string::npos)end=line.size();
      std::string name=line.substr(begin,end-begin);line.replace(begin,end-begin,copy(name));}
    }
   }
  }out+=line+"\n";
 }return out;
}
// Preserve UVs, normals and rotations. Spatial animation derivatives are per
// frame; scale them with position, and divide FPS to slow larger animations.
inline std::string transform(const std::string &source,const std::string &ext,double factor,const std::function<std::string(const std::string&)> &key={}){
 if(!std::isfinite(factor)||factor<=0)throw std::runtime_error("Scale must be greater than zero");
 std::istringstream input(source);std::string line,out;int rows=0,kind=0;bool velocity=false,sprite=false,skipGeoset=false;int changed=0;
 while(std::getline(input,line)){auto t=tokens(line);if(t.empty()){out+=line+"\n";continue;}auto a=lower(t[0]);bool edit=false;
 // Retain full detail on enlarged models, preserving hierarchy and materials.
 if(ext==".3do"&&factor>1.0){
  if(a=="section:")skipGeoset=false;
  if(a=="geosets"&&t.size()==2){t[1]="1";edit=true;}
  if(a=="geoset"&&t.size()==2)skipGeoset=std::stoi(t[1])!=0;
  if(skipGeoset)continue;
 }

 if(ext==".pup"){for(auto &s:t)if(lower(s).ends_with(".key")){if(!key)throw std::runtime_error("Missing animation copier");s=key(s);edit=true;}}
 else if(ext==".spr"&&!sprite){scale(t,2,2,factor);scale(t,8,3,factor);sprite=edit=true;}
 else if(ext==".key"){
  if(velocity){scale(t,0,3,factor);velocity=false;rows--;edit=true;}
  else if(rows){if(a.back()!=':')throw std::runtime_error("Invalid animation entry");scale(t,3,3,factor);velocity=true;edit=true;}
  else if(a=="fps"){scale(t,1,1,1.0/timingScale(factor));edit=true;}
  else if(a=="entries"&&t.size()==2){rows=std::stoi(t[1]);if(rows<0)throw std::runtime_error("Invalid animation count");}
 }else if(ext==".3do"||ext==".par"){
  if(rows){if(a.back()!=':')throw std::runtime_error("Invalid geometry entry");scale(t,kind==1?1:8,3,factor);if(kind==2)scale(t,14,3,factor);rows--;edit=true;}
  else if(a=="radius"||(ext==".par"&&a=="size")){scale(t,1,1,factor);edit=true;}
  else if(a=="insert"&&t.size()>1&&lower(t[1])=="offset"){scale(t,2,3,factor);edit=true;}
  else if(a=="vertices"&&t.size()==2){rows=std::stoi(t[1]);kind=1;}
  else if(a=="hierarchy"&&t.size()==3&&lower(t[1])=="nodes"){rows=std::stoi(t[2]);kind=2;}
  if(rows<0)throw std::runtime_error("Invalid geometry count");
 }
 if(edit){changed++;for(size_t i=0;i<t.size();i++){if(i)out+=' ';out+=t[i];}auto comment=line.find('#');if(comment!=std::string::npos)out+=' '+line.substr(comment);out+='\n';}else out+=line+"\n";
 }
 if(rows||velocity||(!changed&&ext!=".pup"&&ext!=".key"))throw std::runtime_error("Incomplete or unsupported resize resource");return out;
}
}
#endif
