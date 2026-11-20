#pragma once
// Generated only for levels using a door generator. GPL-2.0-or-later.
static const char edDoorScript[] = R"COG(# GeoSith generated sliding / split door
symbols
thing door0=-1 linkid=1 mask=0x405
thing door1=-1 linkid=1 mask=0x405
surface switch0=-1 linkid=2
int activation=1
int keybin=46
flex speed=8
flex wait=2
flex radius=0.4
sound click=set_hi2.wav local
sound locked=df_door2-3.wav local
vector home local
int state=0 local
int player local
int i local
int dummy local
message startup
message activated
message touched
message pulse
message arrived
message timer
message blocked
end
code
startup:
 home=GetThingPos(door0);
 if (switch0 >= 0) SetWallCel(switch0,0);
 if (activation == 0) SetPulse(0.2);
 return;
pulse:
 for (i=0; i<GetMaxPlayers(); i=i+1)
 {
  player=GetPlayerThing(i);
  if (player >= 0)
   if (!(GetThingFlags(player) & 0x80200))
   if (VectorDist(GetThingPos(player),home) < radius) call open;
 }
 return;
touched:
 if (activation == 0 && GetThingType(GetSourceRef()) == 10) call open;
 return;
activated:
 if (activation == 2 && GetSenderID() != 2) return;
 if (activation == 3)
 {
  player=GetSourceRef();
  if (GetThingType(player) != 10) return;
  if (GetInv(player,keybin) < 1)
  {
   dummy=PlaySoundThing(locked,door0,1,-1,-1,0);
   return;
  }
 }
 if (activation == 2) dummy=PlaySoundThing(click,door0,1,-1,-1,0);
 call open;
 return;
open:
 if (state == 1) return;
 if (state == 2) { SetTimer(wait); return; }
 state=1;
 if (switch0 >= 0) SetWallCel(switch0,1);
 MoveToFrame(door0,1,speed);
 if (door1 >= 0) MoveToFrame(door1,1,speed);
 return;
arrived:
 if (IsThingMoving(door0)) return;
 if (door1 >= 0) if (IsThingMoving(door1)) return;
 if (GetCurFrame(door0) == 1) { state=2; SetTimer(wait); }
 else { state=0; if (switch0 >= 0) SetWallCel(switch0,0); }
 return;
timer:
 if (state != 2) return;
 state=3;
 MoveToFrame(door0,0,speed);
 if (door1 >= 0) MoveToFrame(door1,0,speed);
 return;
blocked:
 if (state == 3) call open;
 return;
end
)COG";
