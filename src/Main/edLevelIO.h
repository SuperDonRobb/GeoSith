#ifndef ED_LEVEL_IO_H
#define ED_LEVEL_IO_H
#include "edGeometry.h"
/* resources contains the unmodified JK resource/template sections. */
int edWriteGob(const EdModel *model,const char *path,const char *title,int multiplayer,const char *resources);
int edReadLights(EdModel *model,const char *jkl);
int edReadTextureRotations(EdModel *model,const char *jkl);
void edEpisodeDefault(EdEpisode *episode,const char *title,int multiplayer);
int edEpisodeValidate(const EdEpisode *episode);
int edReadEpisode(EdEpisode *episode,const char *text,const char *currentFile);
int edGobAddFile(const char *path,const char *name,const void *data,unsigned int size);
int edReadMeshData(EdModel *model,const unsigned char *bytes,unsigned int size);
int edWrite3do(const EdModel *model,int asset,const char *path,const char *resources);
int edEncode3do(const EdModel *model,int asset,const char *resources,std::vector<unsigned char> &bytes);
#endif
