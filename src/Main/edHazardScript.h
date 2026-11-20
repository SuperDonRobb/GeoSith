#pragma once
// Generated sector hazards, GPL-2.0-or-later. Normal placed COG execution is host authoritative.
static const char edHazardScript[] = R"COG(# GeoSith sector hazard
symbols
sector hazard=-1
int kind=0
flex damage=20
flex interval=0.5
template emitter=ghost local
template lightning=+force_lightning local
sound zapsound=forcefieldhit01.wav local
thing victim local
thing next local
thing source local
int i local
int face local
int edge local
int floors local
int chosen local
int dummy local
vector normal local
vector point local
vector a local
vector b local
vector rotation local
flex blend local
message startup
message pulse
end
code
startup:
 if (hazard < 0) return;
 if (IsMulti() && !IsServer()) return;
 if (interval < 0.1) interval=0.1;
 SetPulse(interval);
 return;
pulse:
 if (hazard < 0) return;
 victim=FirstThingInSector(hazard);
 while (victim >= 0)
 {
  next=NextThingInSector(victim);
  if ((GetThingType(victim) == 10 || GetThingType(victim) == 2) && GetThingHealth(victim) > 0)
   dummy=DamageThing(victim,damage*interval,8,victim);
  victim=next;
 }
 if (kind != 2) return;
 // Reservoir sampling picks a random upward-facing solid floor polygon.
 floors=0;
 chosen=-1;
 for (i=0; i<GetNumSectorSurfaces(hazard); i=i+1)
 {
  face=GetSectorSurfaceRef(hazard,i);
  normal=GetSurfaceNormal(face);
  if (VectorZ(normal) > 0.25 && GetAdjoinFlags(face) < 0)
  {
   floors=floors+1;
   if (Rand()*floors < 1) chosen=face;
  }
 }
 if (chosen < 0) return;
 normal=GetSurfaceNormal(chosen);
 edge=Rand()*GetNumSurfaceVertices(chosen);
 a=GetSurfaceVertexPos(chosen,edge);
 b=GetSurfaceVertexPos(chosen,(edge+1)%GetNumSurfaceVertices(chosen));
 blend=Rand();
 point=VectorAdd(VectorScale(a,blend),VectorScale(b,1-blend));
 blend=Rand();
 point=VectorAdd(VectorScale(point,blend),VectorScale(GetSurfaceCenter(chosen),1-blend));
 point=VectorAdd(point,VectorScale(normal,0.03));
 rotation=VectorSet(0,Rand()*360,0);
 source=CreateThingAtPos(emitter,hazard,point,rotation);
 if (source >= 0)
 {
  SetThingLook(source,VectorNorm(VectorSub(b,a)));
  dummy=FireProjectile(source,lightning,-1,-1,'0 0 0','0 0 0',1,0,0,0);
  dummy=PlaySoundPos(zapsound,point,0.25,-1,-1,0);
  SetLifeLeft(source,0.1);
 }
 return;
end
)COG";
