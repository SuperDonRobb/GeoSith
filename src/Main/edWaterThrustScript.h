#pragma once
static const char edWaterThrustScript[] = R"COG(# GeoSith generated water current (GPL-2.0-or-later).
symbols
sector water=-1
vector current=(0/0/0)
message startup
end
code
startup:
    SetSectorThrust(water, current, 1);
    return;
end
)COG";
