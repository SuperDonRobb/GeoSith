#pragma once
// Generated ghost-pair proximity teleporters, GPL-2.0-or-later.
static const char edTeleporterScript[] = R"COG(# GeoSith teleporter: ghost orientation controls the exit facing.
symbols
thing entrance=-1 nolink
thing destination=-1 nolink
int twoway=1
flex radius=0.25
template teleport_particles=+telesparks local
sound teleportsnd=ForceThrow01.wav local
int effect local
int player local
int i local
int nearA local
int nearB local
int blocked0=0 local
int blocked1=0 local
int blocked2=0 local
int blocked3=0 local
int blocked4=0 local
int blocked5=0 local
int blocked6=0 local
int blocked7=0 local
int blocked8=0 local
int blocked9=0 local
int blocked10=0 local
int blocked11=0 local
int blocked12=0 local
int blocked13=0 local
int blocked14=0 local
int blocked15=0 local
int blocked16=0 local
int blocked17=0 local
int blocked18=0 local
int blocked19=0 local
int blocked20=0 local
int blocked21=0 local
int blocked22=0 local
int blocked23=0 local
int blocked24=0 local
int blocked25=0 local
int blocked26=0 local
int blocked27=0 local
int blocked28=0 local
int blocked29=0 local
int blocked30=0 local
int blocked31=0 local
message startup
message pulse
end
code
startup:
 SetPulse(0.1);
 return;
pulse:
 if (entrance < 0 || destination < 0) return;
 for (i=0; i<GetMaxPlayers(); i=i+1)
 {
  player=GetPlayerThing(i);
  if (player >= 0)
  {
   if (GetThingFlags(player) & 0x80200) { blocked0[i]=0; }
   else
   {
    nearA=0;
    nearB=0;
    if (GetThingSector(player) == GetThingSector(entrance))
     nearA=(VectorDist(GetThingPos(player),GetThingPos(entrance)) < radius);
    if (GetThingSector(player) == GetThingSector(destination))
     nearB=(VectorDist(GetThingPos(player),GetThingPos(destination)) < radius);
    if (!nearA && !nearB) blocked0[i]=0;
    else if (!blocked0[i])
    {
     if (nearA)
     {
      blocked0[i]=1;
      StopThing(player);
      effect=CreateThing(teleport_particles,player);
      effect=PlaySoundThing(teleportsnd,entrance,1.0,-1,-1,0x80);
      TeleportThing(player,destination);
      effect=CreateThing(teleport_particles,player);
      effect=PlaySoundThing(teleportsnd,destination,1.0,-1,-1,0x80);
     }
     else if (nearB && twoway)
     {
      blocked0[i]=1;
      StopThing(player);
      effect=CreateThing(teleport_particles,player);
      effect=PlaySoundThing(teleportsnd,destination,1.0,-1,-1,0x80);
      TeleportThing(player,entrance);
      effect=CreateThing(teleport_particles,player);
      effect=PlaySoundThing(teleportsnd,entrance,1.0,-1,-1,0x80);
     }
    }
   }
  }
 }
 return;
end
)COG";
