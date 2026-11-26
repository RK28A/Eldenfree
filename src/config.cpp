#include "config.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <mutex>

Config g_cfg;
std::wstring g_dir;

static std::wstring ini() { return g_dir + L"EldenFree.ini"; }

static int geti(const wchar_t* s, const wchar_t* k, int d) {
    wchar_t buf[64];
    GetPrivateProfileStringW(s, k, L"", buf, 64, ini().c_str());
    if (!buf[0]) return d;
    return (int)wcstol(buf, nullptr, 0);
}
static float getf(const wchar_t* s, const wchar_t* k, float d) {
    wchar_t buf[64];
    GetPrivateProfileStringW(s, k, L"", buf, 64, ini().c_str());
    if (!buf[0]) return d;
    return (float)_wtof(buf);
}
static std::string gets(const wchar_t* s, const wchar_t* k) {
    wchar_t buf[2048];
    GetPrivateProfileStringW(s, k, L"", buf, 2048, ini().c_str());
    std::string r;
    for (wchar_t* p = buf; *p; ++p) r += (char)*p;
    return r;
}
static void puts_(const wchar_t* s, const wchar_t* k, const std::string& v) {
    std::wstring w(v.begin(), v.end());
    WritePrivateProfileStringW(s, k, w.c_str(), ini().c_str());
}
static void puti(const wchar_t* s, const wchar_t* k, int v, bool hex = false) {
    wchar_t buf[64];
    if (hex) swprintf(buf, 64, v < 0 ? L"-0x%X" : L"0x%X", v < 0 ? -v : v);
    else swprintf(buf, 64, L"%d", v);
    WritePrivateProfileStringW(s, k, buf, ini().c_str());
}
static void putf(const wchar_t* s, const wchar_t* k, float v) {
    wchar_t buf[64];
    swprintf(buf, 64, L"%.2f", v);
    WritePrivateProfileStringW(s, k, buf, ini().c_str());
}

void cfg_load() {
    Config& c = g_cfg;
    c.toggleKey = geti(L"General", L"ToggleKey", c.toggleKey);
    c.greyollRespawn = geti(L"Boss", L"GreyollRespawn", c.greyollRespawn);
    c.respawnDelay = getf(L"Boss", L"RespawnDelay", c.respawnDelay);
    c.torrentSpeed = geti(L"Torrent", L"Enabled", c.torrentSpeed);
    c.torrentMult = getf(L"Torrent", L"Speed", c.torrentMult);
    c.noFallDamage = geti(L"Player", L"NoFallDamage", c.noFallDamage);
    c.fallSpeed = getf(L"Player", L"FallSpeed", c.fallSpeed);
    c.keepRunes = geti(L"Player", L"KeepRunes", c.keepRunes);
    c.mapCombat = geti(L"World", L"MapInCombat", c.mapCombat);
    c.rideAnywhere = geti(L"World", L"RideAnywhere", c.rideAnywhere);
    c.dungeonWarp = geti(L"World", L"DungeonWarp", c.dungeonWarp);
    c.torrentInfJump = geti(L"Torrent", L"InfiniteJump", c.torrentInfJump);
    c.fastCast = geti(L"Player", L"FastCast", c.fastCast);
    c.castSpeed = getf(L"Player", L"CastSpeed", c.castSpeed);
    c.castAnims = gets(L"Player", L"CastAnims");
    c.castAnimsNo = gets(L"Player", L"CastAnimsAttack");
    c.graceMenu = geti(L"World", L"GraceMenuButtons", c.graceMenu);
    c.offTimeActMod = geti(L"Offsets", L"TimeActModule", c.offTimeActMod);
    c.offPgdFp = geti(L"Offsets", L"PgdFp", c.offPgdFp);
    c.offStateInfo = geti(L"Offsets", L"SpEffectStateInfo", c.offStateInfo);
    c.offPlayerIns = geti(L"Offsets", L"PlayerIns", c.offPlayerIns);
    c.offPgd = geti(L"Offsets", L"PlayerGameData", c.offPgd);
    c.offHpPgd = geti(L"Offsets", L"PgdHp", c.offHpPgd);
    c.offRunes = geti(L"Offsets", L"Runes", c.offRunes);
    c.offModules = geti(L"Offsets", L"ChrModules", c.offModules);
    c.offDataMod = geti(L"Offsets", L"DataModule", c.offDataMod);
    c.offPhysMod = geti(L"Offsets", L"PhysicsModule", c.offPhysMod);
    c.offHp = geti(L"Offsets", L"Hp", c.offHp);
    c.offMaxHp = geti(L"Offsets", L"MaxHp", c.offMaxHp);
    c.offPos = geti(L"Offsets", L"Position", c.offPos);
    c.offBehaviorMod = geti(L"Offsets", L"BehaviorModule", c.offBehaviorMod);
    c.offAnimSpeed = geti(L"Offsets", L"AnimSpeed", c.offAnimSpeed);
    c.offFallMod = geti(L"Offsets", L"FallModule", c.offFallMod);
    c.offFallTimer = geti(L"Offsets", L"FallTimer", c.offFallTimer);
    c.offMaterialMod = geti(L"Offsets", L"MaterialModule", c.offMaterialMod);
    c.offMaterialNoFall = geti(L"Offsets", L"MaterialNoFallDamage", c.offMaterialNoFall);
    c.offRideMod = geti(L"Offsets", L"RideModule", c.offRideMod);
    c.offRideLastMounted = geti(L"Offsets", L"RideLastMounted", c.offRideLastMounted);
    c.offRideMounted = geti(L"Offsets", L"RideMounted", c.offRideMounted);
    c.offRideFallHeight = geti(L"Offsets", L"RideFallHeight", c.offRideFallHeight);
    c.offChrFlags = geti(L"Offsets", L"ChrFlags", c.offChrFlags);
    c.offChrId = geti(L"Offsets", L"ChrId", c.offChrId);
    c.offPhysFalling = geti(L"Offsets", L"PhysicsFalling", c.offPhysFalling);
    c.offSummonSet = geti(L"Offsets", L"SummonBuddySet", c.offSummonSet);
    c.itemGibDelta = geti(L"Offsets", L"ItemGibDelta", c.itemGibDelta);
    if (c.torrentMult < 1.f) c.torrentMult = 1.f;
    if (c.torrentMult > 3.f) c.torrentMult = 3.f;
}

void cfg_save() {
    const Config& c = g_cfg;
    puti(L"General", L"ToggleKey", c.toggleKey, true);
    puti(L"Boss", L"GreyollRespawn", c.greyollRespawn);
    putf(L"Boss", L"RespawnDelay", c.respawnDelay);
    puti(L"Torrent", L"Enabled", c.torrentSpeed);
    putf(L"Torrent", L"Speed", c.torrentMult);
    puti(L"Player", L"NoFallDamage", c.noFallDamage);
    putf(L"Player", L"FallSpeed", c.fallSpeed);
    puti(L"Player", L"KeepRunes", c.keepRunes);
    puti(L"World", L"MapInCombat", c.mapCombat);
    puti(L"World", L"RideAnywhere", c.rideAnywhere);
    puti(L"World", L"DungeonWarp", c.dungeonWarp);
    puti(L"Torrent", L"InfiniteJump", c.torrentInfJump);
    puti(L"Player", L"FastCast", c.fastCast);
    putf(L"Player", L"CastSpeed", c.castSpeed);
    puts_(L"Player", L"CastAnims", c.castAnims);
    puts_(L"Player", L"CastAnimsAttack", c.castAnimsNo);
    puti(L"World", L"GraceMenuButtons", c.graceMenu);
    puti(L"Offsets", L"TimeActModule", c.offTimeActMod, true);
    puti(L"Offsets", L"PgdFp", c.offPgdFp, true);
    puti(L"Offsets", L"SpEffectStateInfo", c.offStateInfo, true);
    puti(L"Offsets", L"PlayerIns", c.offPlayerIns, true);
    puti(L"Offsets", L"PlayerGameData", c.offPgd, true);
    puti(L"Offsets", L"PgdHp", c.offHpPgd, true);
    puti(L"Offsets", L"Runes", c.offRunes, true);
    puti(L"Offsets", L"ChrModules", c.offModules, true);
    puti(L"Offsets", L"DataModule", c.offDataMod, true);
    puti(L"Offsets", L"PhysicsModule", c.offPhysMod, true);
    puti(L"Offsets", L"Hp", c.offHp, true);
    puti(L"Offsets", L"MaxHp", c.offMaxHp, true);
    puti(L"Offsets", L"Position", c.offPos, true);
    puti(L"Offsets", L"BehaviorModule", c.offBehaviorMod, true);
    puti(L"Offsets", L"AnimSpeed", c.offAnimSpeed, true);
    puti(L"Offsets", L"FallModule", c.offFallMod, true);
    puti(L"Offsets", L"FallTimer", c.offFallTimer, true);
    puti(L"Offsets", L"MaterialModule", c.offMaterialMod, true);
    puti(L"Offsets", L"MaterialNoFallDamage", c.offMaterialNoFall, true);
    puti(L"Offsets", L"RideModule", c.offRideMod, true);
    puti(L"Offsets", L"RideLastMounted", c.offRideLastMounted, true);
    puti(L"Offsets", L"RideMounted", c.offRideMounted, true);
    puti(L"Offsets", L"RideFallHeight", c.offRideFallHeight, true);
    puti(L"Offsets", L"ChrFlags", c.offChrFlags, true);
    puti(L"Offsets", L"ChrId", c.offChrId, true);
    puti(L"Offsets", L"PhysicsFalling", c.offPhysFalling, true);
    puti(L"Offsets", L"SummonBuddySet", c.offSummonSet, true);
    puti(L"Offsets", L"ItemGibDelta", c.itemGibDelta, true);
}

void elog(const char* fmt, ...) {
    static std::mutex m;
    std::lock_guard<std::mutex> lk(m);
    FILE* f = _wfopen((g_dir + L"EldenFree.log").c_str(), L"a");
    if (!f) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}
