#pragma once
static const char edWindScript[] = R"COG(# GeoSith wind tunnel (GPL-2.0-or-later).
symbols
sector tunnel=-1
vector thrust=(0/0/0)
message startup
end
code
startup:
 if (tunnel >= 0) SetSectorThrust(tunnel,thrust,1);
 return;
end
)COG";
