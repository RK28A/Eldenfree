#pragma once
#include <string>
#include <cstdint>

struct Config {
    int toggleKey = 0x79;
    bool greyollRespawn = false;
    float respawnDelay = 10.0f;
    bool torrentSpeed = false;
    float torrentMult = 1.0f;
    bool noFallDamage = false;
    float fallSpeed = 4.0f;
    bool keepRunes = false;
    bool mapCombat = false;
    bool rideAnywhere = false;
    bool dungeonWarp = false;
    bool torrentInfJump = false;
    bool fastCast = false;
    float castSpeed = 6.0f;
    std::string castAnims;
    std::string castAnimsNo;
    bool graceMenu = true;

    int offPlayerIns = 0;
    int offPgd = 0x8;
    int offHpPgd = 0x10;
    int offRunes = 0x6C;
    int offModules = 0x190;
    int offDataMod = 0x0;
    int offPhysMod = 0x68;
    int offHp = 0x138;
    int offMaxHp = 0x13C;
    int offPos = 0x70;
    int offBehaviorMod = 0x28;
    int offAnimSpeed = 0x17C8;
    int offFallMod = 0x70;
    int offFallTimer = 0x18;
    int offMaterialMod = 0xA0;
    int offMaterialNoFall = 0x1B;
    int offRideMod = 0xE8;
    int offRideLastMounted = 0x18;
    int offRideMounted = 0x163;
    int offRideFallHeight = 0x130;
    int offChrFlags = 0x19B;
    int offChrId = 0x188;
    int offPhysFalling = 0x1D0;
    int offSummonSet = 0x10F90;
    int offTimeActMod = 0x18;
    int offPgdFp = 0x1C;
    int offStateInfo = 0x156;
    int itemGibDelta = -0x52;
};

extern Config g_cfg;
extern std::wstring g_dir;

void cfg_load();
void cfg_save();
void elog(const char* fmt, ...);
