#pragma once
static const char edDestructibleScript[] = R"COG(# GeoSith generated destructible wall (GPL-2.0-or-later).
# Host-authoritative placed COG; ordinary JK verbs only.
symbols
thing wall mask=0x448
int pieces=0
template blast
template debris0=-1
template debris1=-1
template debris2=-1
template debris3=-1
template debris4=-1
template debris5=-1
template debris6=-1
template debris7=-1
template debris8=-1
template debris9=-1
template debris10=-1
template debris11=-1
template debris12=-1
template debris13=-1
template debris14=-1
template debris15=-1
template debris16=-1
template debris17=-1
template debris18=-1
template debris19=-1
template debris20=-1
template debris21=-1
template debris22=-1
template debris23=-1
template debris24=-1
template debris25=-1
template debris26=-1
template debris27=-1
template debris28=-1
template debris29=-1
template debris30=-1
template debris31=-1
vector offset0=(0/0/0)
vector offset1=(0/0/0)
vector offset2=(0/0/0)
vector offset3=(0/0/0)
vector offset4=(0/0/0)
vector offset5=(0/0/0)
vector offset6=(0/0/0)
vector offset7=(0/0/0)
vector offset8=(0/0/0)
vector offset9=(0/0/0)
vector offset10=(0/0/0)
vector offset11=(0/0/0)
vector offset12=(0/0/0)
vector offset13=(0/0/0)
vector offset14=(0/0/0)
vector offset15=(0/0/0)
vector offset16=(0/0/0)
vector offset17=(0/0/0)
vector offset18=(0/0/0)
vector offset19=(0/0/0)
vector offset20=(0/0/0)
vector offset21=(0/0/0)
vector offset22=(0/0/0)
vector offset23=(0/0/0)
vector offset24=(0/0/0)
vector offset25=(0/0/0)
vector offset26=(0/0/0)
vector offset27=(0/0/0)
vector offset28=(0/0/0)
vector offset29=(0/0/0)
vector offset30=(0/0/0)
vector offset31=(0/0/0)
sector room0=-1
sector room1=-1
sector room2=-1
sector room3=-1
sector room4=-1
sector room5=-1
sector room6=-1
sector room7=-1
sector room8=-1
sector room9=-1
sector room10=-1
sector room11=-1
sector room12=-1
sector room13=-1
sector room14=-1
sector room15=-1
sector room16=-1
sector room17=-1
sector room18=-1
sector room19=-1
sector room20=-1
sector room21=-1
sector room22=-1
sector room23=-1
sector room24=-1
sector room25=-1
sector room26=-1
sector room27=-1
sector room28=-1
sector room29=-1
sector room30=-1
sector room31=-1
int broken=0 local
int i local
int part local
int dummy local
vector delta local
vector position local
message damaged
end
code
damaged:
    if (broken || GetParam(0) <= 0) return;
    if (!(GetParam(1) & 0x14)) return;
    broken = 1;
    // Remove collision before spawning pieces, without losing the wall transform.
    SetThingFlags(wall, 0x80000);
    dummy = CreateThing(blast, wall);
    for (i=0; i<pieces; i=i+1)
    {
        delta = VectorAdd(VectorScale(GetThingRVec(wall), VectorX(offset0[i])), VectorAdd(VectorScale(GetThingLVec(wall), VectorY(offset0[i])), VectorScale(GetThingUVec(wall), VectorZ(offset0[i]))));
        position = VectorAdd(GetThingPos(wall), delta);
        part = CreateThingAtPos(debris0[i], room0[i], position, '0 0 0');
        if (part >= 0)
        {
            SetThingLook(part, GetThingLVec(wall));
            ApplyForce(part, VectorScale(VectorAdd(VectorNorm(delta), VectorSet(Rand()-0.5, Rand()-0.5, 0.8)), 30));
            SetThingRotVel(part, VectorSet(180*(Rand()-0.5), 180*(Rand()-0.5), 180*(Rand()-0.5)));
        }
    }
    DestroyThing(wall);
    return;
end
)COG";
