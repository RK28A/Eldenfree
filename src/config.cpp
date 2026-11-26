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
    c.torrentWalk = getf(L"Torrent", L"Walk", c.torrentWalk);
    c.torrentRun = getf(L"Torrent", L"Run", c.torrentRun);
    c.gallopThreshold = getf(L"Torrent", L"GallopThreshold", c.gallopThreshold);
    c.noFallDamage = geti(L"Player", L"NoFallDamage", c.noFallDamage);
    c.fallSpeed = getf(L"Player", L"FallSpeed", c.fallSpeed);
    c.keepRunes = geti(L"Player", L"KeepRunes", c.keepRunes);
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
    c.offNoDead = geti(L"Offsets", L"PlayerNoDead", c.offNoDead);
    c.itemGibDelta = geti(L"Offsets", L"ItemGibDelta", c.itemGibDelta);
    if (c.torrentWalk < 1.f) c.torrentWalk = 1.f;
    if (c.torrentWalk > 3.f) c.torrentWalk = 3.f;
    if (c.torrentRun < 1.f) c.torrentRun = 1.f;
    if (c.torrentRun > 3.f) c.torrentRun = 3.f;
}

void cfg_save() {
    const Config& c = g_cfg;
    puti(L"General", L"ToggleKey", c.toggleKey, true);
    puti(L"Boss", L"GreyollRespawn", c.greyollRespawn);
    putf(L"Boss", L"RespawnDelay", c.respawnDelay);
    puti(L"Torrent", L"Enabled", c.torrentSpeed);
    putf(L"Torrent", L"Walk", c.torrentWalk);
    putf(L"Torrent", L"Run", c.torrentRun);
    putf(L"Torrent", L"GallopThreshold", c.gallopThreshold);
    puti(L"Player", L"NoFallDamage", c.noFallDamage);
    putf(L"Player", L"FallSpeed", c.fallSpeed);
    puti(L"Player", L"KeepRunes", c.keepRunes);
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
    puti(L"Offsets", L"PlayerNoDead", c.offNoDead, true);
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
