#include "edLevelIO.h"
#include "edWire.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

static int safeText(const char *text) {for(;*text;text++)if(*text=='"'||*text=='\n'||*text=='\r')return 0;return 1;}
const char *edCogTypeName(int type){static const char *names[]={"","","flex","thing","template","sector","surface","keyframe","sound","cog","material","vector","model","ai","int"};return type>=2&&type<=14?names[type]:"unknown";}
/* COG declarations and JKL arguments share a small token grammar. Keep quoted
 * strings and parenthesized vectors intact; comments only start outside them. */
static int cogToken(const char **cursor,char *out,size_t size){const char *p=*cursor;size_t n=0;while(isspace((unsigned char)*p))p++;if(!*p||*p=='#'){*cursor=p+strlen(p);return 0;}
    int quote=*p=='"',depth=0;if(quote)p++;
    if(!quote&&*p=='='){out[0]='=';out[1]=0;*cursor=p+1;return 1;}
    for(;*p;p++){if(quote&&*p=='"'){p++;break;}if(!quote&&!depth&&(isspace((unsigned char)*p)||*p=='#'||*p=='='))break;
        if(!quote&&*p=='(')depth++;if(!quote&&*p==')')depth--;if(n+1>=size)return -1;out[n++]=*p;
    }out[n]=0;*cursor=p;return 1;
}
int edCogParseScript(EdPlacedCog *out,const char *filename,const char *text){EdPlacedCog draft={0};int symbols=0,ended=0;snprintf(draft.file,sizeof(draft.file),"%s",filename);if(strlen(filename)>=sizeof(draft.file))return 0;
    for(const char *p=text;*p;){const char *end=strchr(p,'\n');size_t n=end?(size_t)(end-p):strlen(p);char line[2048];if(n>=sizeof(line))return 0;edCopy(line,p,n);line[n]=0;p=end?end+1:p+n;
        const char *cursor=line;char type[32],name[32],token[256];int result=cogToken(&cursor,type,sizeof(type));if(result<0)return 0;if(!result)continue;for(char *q=type;*q;q++)*q=(char)tolower((unsigned char)*q);
        if(!symbols){if(!strcmp(type,"symbols"))symbols=1;continue;}if(!strcmp(type,"end")){ended=1;break;}
        int kind=0;for(int i=2;i<=14;i++)if(!strcmp(type,edCogTypeName(i)))kind=i;if(!strcmp(type,"float"))kind=ED_COG_FLEX;if(!kind)continue;
        if(cogToken(&cursor,name,sizeof(name))!=1)return 0;EdCogParam param={0};snprintf(param.name,sizeof(param.name),"%s",name);param.type=kind;param.cell=param.face=-1;
        snprintf(param.value,sizeof(param.value),"%s",kind==ED_COG_INT||kind==ED_COG_FLEX?"0":kind==ED_COG_VECTOR?"(0/0/0)":"-1");int local=0;
        result=cogToken(&cursor,token,sizeof(token));if(result<0)return 0;
        if(result&&!strcmp(token,"=")){if(cogToken(&cursor,token,sizeof(token))!=1||strlen(token)>=sizeof(param.value))return 0;snprintf(param.value,sizeof(param.value),"%s",token);result=cogToken(&cursor,token,sizeof(token));}
        int attributeValue=0;while(result>0){for(char *q=token;*q;q++)*q=(char)tolower((unsigned char)*q);if(!strcmp(token,"="))attributeValue=1;else{if(!attributeValue&&!strcmp(token,"local"))local=1;attributeValue=0;}result=cogToken(&cursor,token,sizeof(token));}if(result<0)return 0;
        if(!local){if(draft.count>=ED_COG_PARAMS)return 0;draft.params[draft.count++]=param;}
    }if(!ended)return 0;*out=draft;return 1;
}
int edCogParamValue(const EdModel *m,const EdCogParam *p,char value[32]){
    if(p->type==ED_COG_SECTOR){snprintf(value,32,"%d",p->cell>=0&&p->cell<m->count?p->cell:-1);return 1;}
    if(p->type==ED_COG_SURFACE){int id=-1;if(p->cell>=0&&p->cell<m->count&&p->face>=0&&p->face<m->c[p->cell].count){id=p->face;for(int s=0;s<p->cell;s++)id+=m->c[s].count;}snprintf(value,32,"%d",id);return 1;}
    snprintf(value,32,"%s",p->value);return 1;
}
int edCogSetValue(const EdModel *m,EdCogParam *p,const char *value){EdCogParam draft=*p;draft.cell=draft.face=-1;if(!value[0]||strlen(value)>=32)return 0;
    if(p->type==ED_COG_VECTOR){double x,y,z;char tail;if(sscanf(value," ( %lf / %lf / %lf ) %c",&x,&y,&z,&tail)!=3||!isfinite(x)||!isfinite(y)||!isfinite(z))return 0;if(snprintf(draft.value,32,"(%.6g/%.6g/%.6g)",x,y,z)>=32)return 0;}
    else if(p->type==ED_COG_FLEX){char *end;double v=strtod(value,&end);if(end==value||*end||!isfinite(v)||fabs(v)>1e9)return 0;snprintf(draft.value,32,"%.9g",v);}
    else if(p->type==ED_COG_INT||p->type==ED_COG_THING||p->type==ED_COG_SURFACE||p->type==ED_COG_SECTOR||p->type==ED_COG_COG){char *end;long id=strtol(value,&end,0);if(end==value||*end||id< -2147483647L||id>2147483647L)return 0;
        if(p->type!=ED_COG_INT&&id< -1)return 0;
        if(p->type==ED_COG_SECTOR){if(id>=m->count)return 0;draft.cell=(int)id;}
        if(p->type==ED_COG_SURFACE&&id>=0){int s=0;long index=id;for(;s<m->count&&index>=m->c[s].count;s++)index-=m->c[s].count;if(s>=m->count)return 0;draft.cell=s;draft.face=(int)index;}
        if(p->type==ED_COG_THING&&id>=(m->spawnCount?m->spawnCount:1)+m->objectCount)return 0;if(p->type==ED_COG_COG&&id>=m->cogCount)return 0;snprintf(draft.value,32,"%ld",id);
    }else{for(const char *s=value;*s;s++)if(isspace((unsigned char)*s)||*s=='"'||*s=='#'||*s=='=')return 0;snprintf(draft.value,32,"%s",value);}
    *p=draft;return 1;
}
static void writePlacedCogs(FILE *f,const EdModel *m){fprintf(f,"\nSECTION: COGS\nWorld cogs %d\n",128+m->cogCount);for(int i=0;i<m->cogCount;i++){const EdPlacedCog *c=&m->cogs[i];fprintf(f,"%d: %s",i,c->file);for(int j=0;j<c->count;j++){char value[32];edCogParamValue(m,&c->params[j],value);fprintf(f," %s",value);}fputc('\n',f);}fputs("end\n",f);}
static void writeCustomTemplates(FILE *f,const EdModel *m){
    unsigned char done[ED_CUSTOM_TEMPLATES]={0};for(int pass=0;pass<m->templateCount;pass++)for(int i=0;i<m->templateCount;i++)if(!done[i]){
        const EdTemplate *t=&m->templates[i];int parent=edTemplateIndex(m,t->parent);if(parent>=0&&!done[parent])continue;
        fprintf(f,"%s %s",t->name,t->parent);for(int order=0;order<4;order++)for(int j=0;j<t->count;j++){const char *key=t->params[j].key;int priority=!strcmp(key,"type")?0:!strcmp(key,"move")?1:!strcmp(key,"numframes")?2:3;if(priority==order)fprintf(f," %s=%s",key,t->params[j].value);}fputc('\n',f);done[i]=1;
    }
}
static void writeResources(FILE *f,const EdModel *m,const char *resources){int skip=0,wrote=0,templates=0;
    for(const char *p=resources;*p;){const char *end=strchr(p,'\n');size_t n=end?(size_t)(end-p)+1:strlen(p);char section[64],line[256];size_t length=n<255?n:255;for(size_t i=0;i<length;i++)line[i]=(char)tolower((unsigned char)p[i]);line[length]=0;
        if(sscanf(line," section: %63s",section)==1){templates=!strcmp(section,"templates");skip=!strcmp(section,"header")||!strcmp(section,"cogs");if(!strcmp(section,"cogs")&&!wrote){writePlacedCogs(f,m);wrote=1;}if(!strcmp(section,"templates")&&!wrote){writePlacedCogs(f,m);wrote=1;}}
        if(templates&&!strncmp(line,"end",3)){writeCustomTemplates(f,m);templates=0;}
        char templateName[64];if(templates&&sscanf(line,"%63s",templateName)==1&&edTemplateIndex(m,templateName)>=0){p+=n;continue;}
        if(templates&&!strncmp(line,"world templates",15))fputs("World templates 2048\n",f);else if(!skip)fwrite(p,1,n,f);p+=n;
    }if(!wrote)writePlacedCogs(f,m);
}
static int equalText(const char *a,const char *b){while(*a&&*b){if(tolower((unsigned char)*a++)!=tolower((unsigned char)*b++))return 0;}return *a==*b;}
static int episodeFile(const char *file){size_t n=strlen(file);if(!n||n>31||file[0]=='.')return 0;for(const char *p=file;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='_'&&*p!='-'&&*p!='.')return 0;return strchr(file,'.')!=NULL&&strstr(file,"..")==NULL;}
void edEpisodeDefault(EdEpisode *e,const char *title,int multiplayer){edZero(e,0,sizeof(*e));snprintf(e->title,sizeof(e->title),"%s",title);e->type=multiplayer?2:1;e->count=1;e->rows[0]=EdEpisodeRow{.cd=1,.level=1,.gotoA=-1,.gotoB=-1};strcpy(e->rows[0].file,"sithed.jkl");}
int edEpisodeValidate(const EdEpisode *e){
    if(!e->title[0]||!safeText(e->title)||e->type<1||e->type>65535||e->count<1||e->count>ED_EPISODE_ROWS||e->current<0||e->current>=e->count||e->rows[e->current].cutscene)return 0;
    for(int i=0;i<e->count;i++){const EdEpisodeRow *r=&e->rows[i];size_t n=strlen(r->file);
        if(!episodeFile(r->file)||(!r->cutscene&&(n<5||!equalText(r->file+n-4,".jkl")))||r->cutscene<0||r->cutscene>1||r->line<0||r->line>9999||r->cd<0||r->cd>9||r->level<0||r->level>999||r->lightPower<0||r->lightPower>1000||r->darkPower<0||r->darkPower>1000||!safeText(r->title))return 0;
        for(int j=0;j<20;j++)if(!safeText(r->intro[j]))return 0;
        for(int j=0;j<i;j++)if(e->rows[j].line==r->line)return 0;
        int a=r->gotoA==-1,b=r->gotoB==-1;for(int j=0;j<e->count;j++){a|=r->gotoA==e->rows[j].line;b|=r->gotoB==e->rows[j].line;}if(!a||!b)return 0;
    }return 1;
}
int edReadEpisode(EdEpisode *out,const char *text,const char *currentFile){
    EdEpisode e={0};int stage=0,count=0,ended=0;e.current=-1;
    for(const char *p=text;*p;){const char *end=strchr(p,'\n');size_t length=end?(size_t)(end-p):strlen(p);char line[512];if(length>=sizeof(line))return 0;edCopy(line,p,length);line[length]=0;p=end?end+1:p+length;
        char *s=line;while(isspace((unsigned char)*s))s++;if(!*s||*s=='#')continue;
        if(stage==0){char *last=*s=='"'?strchr(s+1,'"'):NULL;if(!last||last-s-1>=sizeof(e.title))return 0;edCopy(e.title,s+1,last-s-1);stage++;}
        else if(stage==1){for(char *q=s;*q;q++)*q=(char)toupper((unsigned char)*q);if(sscanf(s,"TYPE %d",&e.type)!=1)return 0;stage++;}
        else if(stage==2){for(char *q=s;*q;q++)*q=(char)toupper((unsigned char)*q);if(sscanf(s,"SEQ %d",&e.count)!=1||e.count<1||e.count>ED_EPISODE_ROWS)return 0;stage++;}
        else{if(equalText(s,"end")||equalText(s,"end\r")){ended=1;break;}if(count>=e.count)return 0;
            EdEpisodeRow *r=&e.rows[count];char type[32];if(sscanf(s,"%d: %d %d %31s %31s %d %d %d %d",&r->line,&r->cd,&r->level,type,r->file,&r->lightPower,&r->darkPower,&r->gotoA,&r->gotoB)!=9)return 0;
            if(!equalText(type,"LEVEL")&&!equalText(type,"CUT"))return 0;r->cutscene=equalText(type,"CUT");if(!r->cutscene&&equalText(r->file,currentFile)&&e.current<0)e.current=count;count++;
        }
    }if(!ended||count!=e.count||!edEpisodeValidate(&e))return 0;*out=e;return 1;
}
int edReadTextureRotations(EdModel *m,const char *text){
    for(const char *line=text;line&&*line;line=strchr(line,'\n')){if(*line=='\n')line++;
        int sector,faceIndex;
        if(sscanf(line,"# GeoSith custom uv %d %d",&sector,&faceIndex)==2){if(sector<0||sector>=m->count||faceIndex<0||faceIndex>=m->c[sector].count)return 0;m->c[sector].f[faceIndex].customUV=1;continue;}
        if(strncmp(line,"# GeoSith texture rotation ",27))continue;
        int s,f;double angle,u,v,us,vs;
        if(sscanf(line,"# GeoSith texture rotation %d %d %lf %lf %lf %lf %lf",&s,&f,&angle,&u,&v,&us,&vs)!=7||s<0||s>=m->count||f<0||f>=m->c[s].count||!isfinite(angle)||!isfinite(u)||!isfinite(v)||!isfinite(us)||!isfinite(vs)||fabs(us)<.000001||fabs(vs)<.000001)return 0;
        EdFace *face=&m->c[s].f[f];face->attributes=edSurfaceAttributes(face);face->hasAttributes=1;face->attributes.uScale=us;face->attributes.vScale=vs;
        face->uvRotation=angle;face->uOffset=u;face->vOffset=v;edMapFace(face);
    }return 1;
}
int edReadLights(EdModel *m,const char *jkl) {
    EdBuffer<EdLight> lights;int section=0,expected=-1,count=0,ended=0;
    for(const char *p=jkl;*p;){
        const char *end=strchr(p,'\n');size_t length=end?(size_t)(end-p):strlen(p);char line[512];
        if(length>=sizeof(line)){if(section)return 0;p=end?end+1:p+length;continue;}
        for(size_t i=0;i<length;i++)line[i]=(char)tolower((unsigned char)p[i]);line[length]=0;p=end?end+1:p+length;
        char *s=line;while(isspace((unsigned char)*s))s++;if(!*s||*s=='#')continue;
        if(!section){char name[64];if(sscanf(s,"section: %63s",name)==1&&!strcmp(name,"lights"))section=1;continue;}
        if(expected<0){if(sscanf(s,"editor lights %d",&expected)!=1||expected<0||expected>ED_LIGHTS)return 0;continue;}
        if(!strncmp(s,"end",3)){ended=1;break;}
        int id;EdLight l={0};if(count>=expected||sscanf(s,"%d: %i %lf %lf %lf %lf %lf",&id,&l.type,&l.position.x,&l.position.y,&l.position.z,&l.intensity,&l.range)!=7)return 0;
        if(!isfinite(l.position.x)||!isfinite(l.position.y)||!isfinite(l.position.z)||!isfinite(l.intensity)||l.intensity<0||l.intensity>100||!isfinite(l.range)||l.range<ED_GRID||l.range>10000)return 0;
        lights[count++]=l;
    }
    if(section&&(!ended||count!=expected))return 0;
    m->lightCount=count;edZero(m->lights,0,sizeof(m->lights));edCopy(m->lights,lights,sizeof(EdLight)*count);return 1;
}
static void header(FILE *f,const EdLevelHeader *h) {
    fprintf(f,"\nSECTION: HEADER\nVersion 1\nWorld Gravity %.9g\nCeiling Sky Z %.9g\nHorizon Distance %.9g\nHorizon Pixels per Rev %.9g\nHorizon Sky Offset %.9g %.9g\nCeiling Sky Offset %.9g %.9g\nMipMap Distances %.9g %.9g %.9g %.9g\nLOD Distances %.9g %.9g %.9g %.9g\nPerspective distance %.9g\nGouraud distance %.9g\n",
        h->gravity,h->ceilingSkyZ,h->horizonDistance,h->horizonPixels,h->horizonOffset[0],h->horizonOffset[1],h->ceilingOffset[0],h->ceilingOffset[1],h->mip[0],h->mip[1],h->mip[2],h->mip[3],h->lod[0],h->lod[1],h->lod[2],h->lod[3],h->perspective,h->gouraud);
}

static void u32(FILE *f,uint32_t n) { unsigned char b[4]={(unsigned char)n,(unsigned char)(n>>8),(unsigned char)(n>>16),(unsigned char)(n>>24)}; fwrite(b,1,4,f); }
static uint32_t readU32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int edGobAddFile(const char *path,const char *name,const void *data,unsigned int size){if(strlen(name)>=128||!size)return 0;FILE *f=fopen(path,"r+b");if(!f)return 0;unsigned char header[12],countData[4];int ok=fread(header,1,12,f)==12&&!edCompare(header,"GOB ",4);uint32_t directory=ok?readU32(header+8):0;
    ok=ok&&!fseek(f,directory,SEEK_SET)&&fread(countData,1,4,f)==4;uint32_t count=ok?readU32(countData):0;if(count>16384)ok=0;unsigned char *entries=ok?(unsigned char*)edCalloc(count+1,136):NULL;ok=ok&&entries&&fread(entries,136,count,f)==count;
    if(ok){int index=(int)count;for(uint32_t i=0;i<count;i++)if(equalText((char*)entries+i*136+8,name)){index=(int)i;break;}if(index==(int)count)count++;
        ok=!fseek(f,directory,SEEK_SET)&&fwrite(data,1,size,f)==size;uint32_t next=(uint32_t)ftell(f);unsigned char *entry=entries+index*136;edZero(entry,0,136);
        for(int i=0;i<4;i++){entry[i]=(unsigned char)(directory>>(i*8));entry[4+i]=(unsigned char)(size>>(i*8));}strcpy((char*)entry+8,name);u32(f,count);ok=ok&&fwrite(entries,136,count,f)==count&&!fseek(f,8,SEEK_SET);u32(f,next);
    }edFree(entries);ok=ok&&!ferror(f);if(fclose(f))ok=0;return ok;
}
static int jkl(FILE *f,const EdModel *m,const char *resources) {
    header(f,&m->header);writeResources(f,m,resources);
    fprintf(f,"\nSection: Lights\nEditor lights %d\n\n#num:\ttype:\tX:\tY:\tZ:\tintensity:\trange:\n",m->lightCount);
    for(int i=0;i<m->lightCount;i++){const EdLight *l=&m->lights[i];fprintf(f,"%d: %d %.9f %.9f %.9f %.9f %.9f\n",i,l->type,l->position.x,l->position.y,l->position.z,l->intensity,l->range);}fputs("end\n",f);
    int vertices=0,surfaces=0,adjoins=0;
    EdBuffer<int> first;EdBuffer<EdBuffer<int>> index,adj;
    for(int s=0;s<m->count;s++) { first[s]=surfaces; for(int p=0;p<m->c[s].count;p++) {
        const EdFace *face=&m->c[s].f[p]; index[s][p]=surfaces++; vertices+=face->count; adj[s][p]=face->neighbor>=0?adjoins++:-1;
    } }
    EdVec *positions=(EdVec*)edAlloc(sizeof(EdVec)*vertices);int *positionIndices=(int*)edAlloc(sizeof(int)*vertices);
    if(!positions||!positionIndices){edFree(positions);edFree(positionIndices);return 0;}
    int unique=0,k=0;
    for(int s=0;s<m->count;s++) for(int p=0;p<m->c[s].count;p++) for(int v=0;v<m->c[s].f[p].count;v++) {
        EdVec point=m->c[s].f[p].v[v].p;int i=0;for(;i<unique;i++){EdVec delta=edSub(point,positions[i]);if(edDot(delta,delta)<ED_EPS*ED_EPS)break;}
        if(i==unique)positions[unique++]=point;positionIndices[k++]=i;
    }
    std::vector<const char*> maps={m->header.colormap};EdBuffer<int> sectorMaps;
    for(int s=0;s<m->count;s++){const char *name=m->c[s].colormap[0]?m->c[s].colormap:m->header.colormap;int i=0;for(;i<(int)maps.size();i++)if(!strcmp(name,maps[i]))break;if(i==(int)maps.size())maps.push_back(name);sectorMaps[s]=i;}
    fprintf(f,"\nSECTION: GEORESOURCE\nWorld colormaps %d\n",(int)maps.size());for(int i=0;i<(int)maps.size();i++)fprintf(f,"%d: %s\n",i,maps[i]);fprintf(f,"World vertices %d\n",unique);
    for(int i=0;i<unique;i++)fprintf(f,"%d: %.9f %.9f %.9f\n",i,positions[i].x,positions[i].y,positions[i].z);
    fprintf(f,"World texture vertices %d\n",vertices); k=0;
    for(int s=0;s<m->count;s++) for(int p=0;p<m->c[s].count;p++) for(int v=0;v<m->c[s].f[p].count;v++) {
        const EdVertex *q=&m->c[s].f[p].v[v]; fprintf(f,"%d: %.9f %.9f\n",k++,q->u,q->v);
    }
    fprintf(f,"World adjoins %d\n",adjoins);
    for(int s=0;s<m->count;s++) for(int p=0;p<m->c[s].count;p++) { const EdFace *face=&m->c[s].f[p];
        if(face->neighbor>=0){EdSurfaceAttributes a=edSurfaceAttributes(face);fprintf(f,"%d: 0x%x %d %.9f\n",adj[s][p],a.adjoinFlags,adj[face->neighbor][face->mirror],a.adjoinDistance);}
    }
    fprintf(f,"World surfaces %d\n",surfaces); k=0;
    for(int s=0;s<m->count;s++) for(int p=0;p<m->c[s].count;p++) { const EdFace *face=&m->c[s].f[p];
        EdSurfaceAttributes a=edSurfaceAttributes(face);fprintf(f,"%d: %d 0x%x 0x%x %d %d %d %d %.9f %d",index[s][p],face->material,a.surfaceFlags,a.faceFlags,a.geometryMode,m->lightCount?3:a.lightingMode,a.textureMode,adj[s][p],a.extraLight,face->count);
        for(int v=0;v<face->count;v++,k++) fprintf(f," %d,%d",positionIndices[k],k);
        for(int v=0;v<face->count;v++)fprintf(f," %.9f",edVertexLight(m,s,p,v));fputc('\n',f);
    }
    for(int s=0;s<m->count;s++)for(int p=0;p<m->c[s].count;p++){const EdFace *face=&m->c[s].f[p];if(face->uvRotation){EdSurfaceAttributes a=edSurfaceAttributes(face);fprintf(f,"# GeoSith texture rotation %d %d %.17g %.17g %.17g %.17g %.17g\n",s,p,face->uvRotation,face->uOffset,face->vOffset,a.uScale,a.vScale);}}
    for(int s=0;s<m->count;s++)for(int face=0;face<m->c[s].count;face++)if(m->c[s].f[face].customUV)fprintf(f,"# GeoSith custom uv %d %d\n",s,face);
    for(int i=0;i<m->splineCount;i++){const EdSpline &s=m->splines[i];fprintf(f,"# GeoSith spline %d %d %d %d %.17g %.17g\n",s.id,s.sides,s.segments,s.material,s.width,s.height);if(s.groupId)fprintf(f,"# GeoSith splinegroup %d %d\n",s.id,s.groupId);for(int p=0;p<s.count;p++){const EdSplinePoint &v=s.points[p];fprintf(f,"# GeoSith splinepoint %d %d %.17g %.17g %.17g %.17g %.17g %.17g\n",s.id,p,v.position.x,v.position.y,v.position.z,v.handle.x,v.handle.y,v.handle.z);}}
    for(int i=0;i<m->splineCount;i++){EdSpline state=m->splines[i];EdWire measure={NULL,ED_NET_LIMIT/2,0,0,1};edWireSplineState(&measure,&state);if(!measure.ok)return 0;std::vector<unsigned char> bytes(measure.at);EdWire w={bytes.data(),bytes.size(),0,0,1};edWireSplineState(&w,&state);for(size_t offset=0;offset<bytes.size();offset+=128){fprintf(f,"# GeoSith splinestate %d %u ",state.id,(unsigned)offset);for(size_t b=offset;b<bytes.size()&&b<offset+128;b++)fprintf(f,"%02x",bytes[b]);fprintf(f,"\n");}}
    for(int i=0;i<m->splineCount;i++)if(m->splines[i].closed)fprintf(f,"# GeoSith splineclosed %d\n",m->splines[i].id);
    for(int i=0;i<m->splineCount;i++)if(m->splines[i].mirrorAxes)fprintf(f,"# GeoSith splinemirror %d %d\n",m->splines[i].id,m->splines[i].mirrorAxes);
    for(int c=0;c<m->count;c++)if(m->c[c].splineId)fprintf(f,"# GeoSith splinecell %d %d\n",c,m->c[c].splineId);
    if(m->lightCount)for(int s=0;s<m->count;s++)for(int p=0;p<m->c[s].count;p++)for(int v=0;v<m->c[s].f[p].count;v++)if(m->c[s].f[p].v[v].light)fprintf(f,"# GeoSith base light %d %d %d %.9f\n",s,p,v,m->c[s].f[p].v[v].light);
    /* JK stores one normal per surface after the indexed polygon list. */
    for(int s=0;s<m->count;s++) for(int p=0;p<m->c[s].count;p++) { EdVec n=edNormal(&m->c[s].f[p]); fprintf(f,"%d: %.9f %.9f %.9f\n",index[s][p],n.x,n.y,n.z); }
    fputs("end\n\nSECTION: SECTORS\n",f); fprintf(f,"World sectors %d\n",m->count); k=0;
    for(int s=0;s<m->count;s++) {
        EdVec lo=edV(1e9,1e9,1e9),hi=edV(-1e9,-1e9,-1e9); int count=0;
        for(int p=0;p<m->c[s].count;p++) for(int v=0;v<m->c[s].f[p].count;v++) {
            EdVec q=m->c[s].f[p].v[v].p; if(q.x<lo.x)lo.x=q.x;if(q.y<lo.y)lo.y=q.y;if(q.z<lo.z)lo.z=q.z;
            if(q.x>hi.x)hi.x=q.x;if(q.y>hi.y)hi.y=q.y;if(q.z>hi.z)hi.z=q.z; count++;
        }
        EdVec center=edScale(edAdd(lo,hi),.5),d=edSub(hi,lo);
        const EdCell *cell=&m->c[s];
        if(cell->layer[0])fprintf(f,"# GeoSith layer %d %s\n",s,cell->layer);
        if(cell->tolerance)fprintf(f,"# GeoSith tolerance %d %.9f\n",s,cell->tolerance);
        fprintf(f,"Sector %d\nFlags 0x%x\nAmbient light %.9f\nExtra light %.9f\nColormap %d\nTint %.9f %.9f %.9f\nBoundbox %.9f %.9f %.9f %.9f %.9f %.9f\n",s,cell->flags,cell->ambientLight,cell->extraLight,sectorMaps[s],cell->tint.x,cell->tint.y,cell->tint.z,lo.x,lo.y,lo.z,hi.x,hi.y,hi.z);
        if(cell->sound[0])fprintf(f,"Sound %s %.9f\n",cell->sound,cell->soundVolume);
        fprintf(f,"Center %.9f %.9f %.9f\nRadius %.9f\nVertices %d\n",center.x,center.y,center.z,(d.x*d.x+d.y*d.y+d.z*d.z)+1,count);
        for(int v=0;v<count;v++) fprintf(f,"%d: %d\n",v,positionIndices[k++]);
        fprintf(f,"Surfaces %d %d\n",first[s],m->c[s].count);
    }
    fprintf(f,"end\n\nSECTION: THINGS\nWorld things %d\n",640+m->objectCount+m->spawnCount);
    edFree(positions);edFree(positionIndices);
    int count=m->spawnCount?m->spawnCount:1;
    if(count<1||count>ED_SPAWNS)return 0;
    for(int i=0;i<count;i++) {
        EdVec p=m->spawnCount?m->spawns[i].position:m->spawn;
        EdVec r=m->spawnCount?m->spawns[i].rotation:edV(0,0,0);
        int sector=-1;for(int s=0;s<m->count;s++)if(edContains(&m->c[s],p)){sector=s;break;}
        if(sector<0)return 0;
        fprintf(f,"%d: walkplayer walkplayer %.9f %.9f %.9f %.9f %.9f %.9f %d",i,p.x,p.y,p.z,r.x,r.y,r.z,sector);
        const EdThingAttributes *a=&m->spawns[i].attributes;
        if(a->hasFlags)fprintf(f," thingflags=0x%x",a->flags);
        if(a->aiClass[0])fprintf(f," aiclass=%s",a->aiClass);
        if(a->hasVelocity)fprintf(f," vel=(%.9f/%.9f/%.9f)",a->velocity.x,a->velocity.y,a->velocity.z);
        if(a->aiFrames){fprintf(f," numframes=%d",a->frameCount);for(int j=0;j<a->frameCount;j++){EdVec q=a->frames[j].position;fprintf(f," frame=(%.9f/%.9f/%.9f)",q.x,q.y,q.z);}}

        else if(a->frameCount){fprintf(f," move=path numframes=%d",a->frameCount);for(int j=0;j<a->frameCount;j++){EdFrame q=a->frames[j];fprintf(f," frame=(%.9f/%.9f/%.9f:%.9f/%.9f/%.9f)",q.position.x,q.position.y,q.position.z,q.rotation.x,q.rotation.y,q.rotation.z);}}
        else if(a->pathOverride)fputs(" move=none",f);
        fputc('\n',f);
    }
    if(m->objectCount<0||m->objectCount>ED_OBJECTS)return 0;
    for(int i=0;i<m->objectCount;i++){
        const EdObject *object=&m->objects[i];int sector=-1;
        for(int s=0;s<m->count;s++)if(edContains(&m->c[s],object->position)){sector=s;break;}
        if(sector<0)return 0;
        for(const char *p=object->templateName;*p;p++)if(isspace((unsigned char)*p)||*p=='"')return 0;
        fprintf(f,"%d: %s object%d %.9f %.9f %.9f %.9f %.9f %.9f %d",count+i,object->templateName,i,object->position.x,object->position.y,object->position.z,object->rotation.x,object->rotation.y,object->rotation.z,sector);
        const EdThingAttributes *a=&object->attributes;
        if(a->hasFlags)fprintf(f," thingflags=0x%x",a->flags);
        if(a->aiClass[0])fprintf(f," aiclass=%s",a->aiClass);
        if(a->hasVelocity)fprintf(f," vel=(%.9f/%.9f/%.9f)",a->velocity.x,a->velocity.y,a->velocity.z);
        if(a->aiFrames){fprintf(f," numframes=%d",a->frameCount);for(int j=0;j<a->frameCount;j++){EdVec q=a->frames[j].position;fprintf(f," frame=(%.9f/%.9f/%.9f)",q.x,q.y,q.z);}}

        else if(a->frameCount){fprintf(f," move=path numframes=%d",a->frameCount);for(int j=0;j<a->frameCount;j++){EdFrame q=a->frames[j];fprintf(f," frame=(%.9f/%.9f/%.9f:%.9f/%.9f/%.9f)",q.position.x,q.position.y,q.position.z,q.rotation.x,q.rotation.y,q.rotation.z);}}
        else if(a->pathOverride)fputs(" move=none",f);
        fputc('\n',f);
    }
    fputs("end\n",f);
    return !ferror(f);
}
#include "edMeshIO.h"
int edWriteGob(const EdModel *m,const char *path,const char *title,int multiplayer,const char *resources) {
    if(!m||!resources||!edValidate(m)) return 0;
    if(m->header.title[0])title=m->header.title;
    if(!safeText(title)||!m->header.colormap[0]||!safeText(m->header.colormap))return 0;
    for(const char *p=m->header.colormap;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'||*p=='.'))return 0;
    for(int i=0;i<20;i++)if(!safeText(m->header.intro[i]))return 0;
    EdEpisode episode=m->episode;if(!episode.count)edEpisodeDefault(&episode,title,multiplayer);if(!edEpisodeValidate(&episode))return 0;
    FILE *f=fopen(path,"wb"); if(!f) return 0;
    fwrite("GOB ",1,4,f); u32(f,20);u32(f,0);
    uint32_t offsets[5],sizes[5]; offsets[0]=(uint32_t)ftell(f);
    fprintf(f,"\"%s\"\nTYPE %d\nSEQ %d\n",episode.title,episode.type,episode.count);
    for(int i=0;i<episode.count;i++){const EdEpisodeRow *r=&episode.rows[i];fprintf(f,"%d: %d %d %s %s %d %d %d %d\n",r->line,r->cd,r->level,r->cutscene?"CUT":"LEVEL",r->file,r->lightPower,r->darkPower,r->gotoA,r->gotoB);}fputs("end\n",f);
    sizes[0]=(uint32_t)ftell(f)-offsets[0]; offsets[1]=(uint32_t)ftell(f);
    int ok=jkl(f,m,resources); sizes[1]=(uint32_t)ftell(f)-offsets[1];offsets[2]=(uint32_t)ftell(f);
    int extra=0;for(int i=0;i<episode.count;i++){int unique=1;for(int j=0;j<i;j++)if(equalText(episode.rows[i].file,episode.rows[j].file))unique=0;if(unique&&!equalText(episode.rows[i].file,"sithed.jkl"))extra++;}
    int legacy=-1;for(int i=0;i<episode.count;i++)if(equalText(episode.rows[i].file,"sithed.jkl")){legacy=i;break;}
    const EdEpisodeRow *legacyRow=legacy>=0&&legacy!=episode.current?&episode.rows[legacy]:NULL;
    fprintf(f,"MSGS %d\n\"SITHED\" 0 \"%s\"\n",21+extra*21,legacyRow?(legacyRow->title[0]?legacyRow->title:legacyRow->file):title);
    for(int i=0;i<20;i++)fprintf(f,"\"SITHED_TEXT_%02d\" 0 \"%s\"\n",i,legacyRow?legacyRow->intro[i]:m->header.intro[i]);
    for(int i=0;i<episode.count;i++){const EdEpisodeRow *r=&episode.rows[i];int unique=1;for(int j=0;j<i;j++)if(equalText(r->file,episode.rows[j].file))unique=0;if(!unique||equalText(r->file,"sithed.jkl"))continue;
        char key[32];snprintf(key,sizeof(key),"%s",r->file);char *dot=strrchr(key,'.');if(dot)*dot=0;for(char *p=key;*p;p++)*p=(char)toupper((unsigned char)*p);
        fprintf(f,"\"%s\" 0 \"%s\"\n",key,i==episode.current?title:r->title[0]?r->title:r->file);
        for(int j=0;j<20;j++)fprintf(f,"\"%s_TEXT_%02d\" 0 \"%s\"\n",key,j,i==episode.current?m->header.intro[j]:r->intro[j]);
    }
    fputs("END\n",f);sizes[2]=(uint32_t)ftell(f)-offsets[2];offsets[3]=(uint32_t)ftell(f);
    const char *templates=strstr(resources,"World templates");if(templates){templates=strchr(templates,'\n');const char *end=templates?strstr(templates,"\nend"):NULL;if(end&&end>templates+1)fwrite(templates+1,1,end-templates-1,f);}
    fputc('\n',f);sizes[3]=(uint32_t)ftell(f)-offsets[3];offsets[4]=(uint32_t)ftell(f);fprintf(f,"%s\n",episode.rows[episode.current].file);sizes[4]=(uint32_t)ftell(f)-offsets[4];uint32_t directory=(uint32_t)ftell(f);
    char levelPath[64];snprintf(levelPath,sizeof(levelPath),"jkl\\%s",episode.rows[episode.current].file);
    u32(f,5); const char *names[5]={"episode.jk",levelPath,"misc\\cogstrings.uni","misc\\sithed_templates.tpl","misc\\sithed_editor.txt"};
    for(int i=0;i<5;i++) { char name[128]={0}; strcpy(name,names[i]); u32(f,offsets[i]);u32(f,sizes[i]);fwrite(name,1,128,f); }
    if(fseek(f,8,SEEK_SET)) ok=0; u32(f,directory); if(ferror(f)) ok=0; if(fclose(f)) ok=0;
    if(ok)ok=writeMeshAssets(m,path,resources);
    if(ok)ok=writeGroups(m,path);
    for(int i=0;ok&&i<m->decalCount;i++){char name[64];snprintf(name,sizeof(name),"mat\\%s",m->decals[i].name);ok=edGobAddFile(path,name,m->decals[i].data->data(),(unsigned)m->decals[i].data->size());}
    if(!ok) remove(path); return ok;
}

