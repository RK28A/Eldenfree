#pragma once
#include <string>
#include <cstdint>

struct Config {
    int toggleKey = 0x79;
    bool greyollRespawn = false;
    float respawnDelay = 10.0f;
    bool torrentSpeed = false;
    float torrentWalk = 1.0f;
    float torrentRun = 1.0f;
    float gallopThreshold = 11.0f;
    bool noFallDamage = false;
    float fallSpeed = 7.0f;
    bool keepRunes = false;

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
    int offNoDead = 0x0;
    int itemGibDelta = -0x52;
};

extern Config g_cfg;
extern std::wstring g_dir;

void cfg_load();
void cfg_save();
void elog(const char* fmt, ...);
