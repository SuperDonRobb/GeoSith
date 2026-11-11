#ifndef JK_EDITOR_H
#define JK_EDITOR_H
#include "types.h"
#define JKEDITOR_CREATE 321
#define JKEDITOR_NET_MESSAGE 64
#define JKEDITOR_NET_FLAG 0x10000000
void jkEditor_NetTick(void);
void jkEditor_NetProgress(char *text,int size);
int jkEditor_NetFailed(void);
int jkEditor_NetSafeDialog(int safe);
int jkEditor_NetMessage(SithMessage *message);
int jkEditor_NetJoinRequest(int player);
int jkEditor_NetPrepareJoin(char *episode,const char *map,int flags);
int jkEditor_NetEpisode(const char *episode);
void jkEditor_NetPreparePlayers(SithWorld *world);
int jkEditor_NetOwnsGuid(int guid);
int jkEditor_NetThingSlot(int requested,int guid);
int jkEditor_NetIgnoreMessage(const SithMessage *message);
void jkEditor_NetPlayerLeft(int player);
int jkEditor_Create(int multiplayer);
int jkEditor_Control(SithThing *player,flex_t delta);
int jkEditor_IsLevel(void);
int jkEditor_Active(void);
int jkEditor_MapActive(void);
void jkEditor_Draw(void);
void jkEditor_DrawWorld(void);
void jkEditor_SaveDialog(void);
void jkEditor_HeaderDialog(void);
void jkEditor_EpisodeDialog(void);
void jkEditor_Close(void);
void jkEditor_SelfTest(void);
int jkEditor_ResourceCapacity(SithWorld *world,int requested,int minimum);
int jkEditor_HandleEscape(void);
int jkEditor_ModelBlue(void);
int jkEditor_TerrainLockedThing(const SithThing *thing);
int jkEditor_SurfaceLocked(const SithSurface *surface);
#endif
