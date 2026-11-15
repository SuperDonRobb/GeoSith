#include <cstdio>
#include <cstring>
#include <cctype>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#define __strcmpi _stricmp
#else
#include <strings.h>
#define __strcmpi strcasecmp
#endif
struct StdConffileEntry { int marker; };
static StdConffileEntry stdConffile_g_entry{42};
struct World { int numThingTemplates=0,sizeThingTemplates=3; } world;
static World *sithWorld_g_pCurrentWorld=&world;
static std::map<std::string,std::string> templates;
static std::vector<std::string> args;
static void edCopy(void *d,const void *s,size_t n){memcpy(d,s,n);}
static void stdString_CStrToLower(char *s){for(;*s;s++)*s=(char)tolower((unsigned char)*s);}
static const char *templateBlock(const char *s){const char *p=strstr(s,"World templates");return p?strchr(p,'\n'):nullptr;}
static void *sithTemplate_GetTemplate(const char *s){return templates.count(s)?&world:nullptr;}
static void stdConffile_ReadArgsFromStr(char *s){args.clear();for(char *p=strtok(s," \t\r\n");p;p=strtok(nullptr," \t\r\n"))args.emplace_back(p);stdConffile_g_entry.marker=7;}
static void sithTemplate_Parse(World *w){
    if(args.size()<2||templates.count(args[0]))return;
    if(args[1]!="none"&&!templates.count(args[1]))return;
    templates[args[0]]=args.size()>2?args[2]:"";++w->numThingTemplates;
}
#include "manifest_templates_production.inc"
int main(){
    const char *manifest="World templates 3\n_WalkStruct none type=cog\nKalima_Statue _WalkStruct model3d=Kalima_Statue.3do # comment\nChild KALIMA_STATUE model3d=Child.3DO\nKALIMA_STATUE _WalkStruct\nend\n";
    if(!loadMissingManifestTemplates(manifest)||world.numThingTemplates!=3||
       !sithTemplate_GetTemplate("kalima_statue")||!sithTemplate_GetTemplate("child")||
       templates["kalima_statue"]!="model3d=kalima_statue.3do"||stdConffile_g_entry.marker!=42)return 1;
    // A fresh snapshot must reuse canonical names even with no capacity left.
    if(!loadMissingManifestTemplates(manifest)||world.numThingTemplates!=3||stdConffile_g_entry.marker!=42)return 2;
    puts("PASS: mixed-case manifest names, parents, resources and repeated imports");
}
