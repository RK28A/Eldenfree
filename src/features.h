#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace feat {

constexpr uint32_t GREYOLL_FLAG = 1050400800;

enum class Act { ResetGreyoll, UnlockMap, UnlockGraces, UnlockBells, DupWeapon, RefreshInv, SaveCfg, OpenMenu, ResetCast, ResetJump };

struct Status {
    bool menus = false;
    bool pMap = false, pRide = false, pJump = false, pAir = false, warpOk = false;
    int castLearned = 0;
    bool casting = false;
    bool esd = false;
    bool warp = false;
    int jumpLearned = 0;
    int castAttack = 0;
    bool core = false, params = false, player = false, itemGib = false, names = false, flags = false;
    int playerOff = -1;
    size_t paramCount = 0;
    bool torrentFound = false, mounted = false;
    float torrentBase = 0.f;
    float torrentMult = 1.f;
    int greyoll = -1;
    uint32_t runes = 0;
    bool fallProtect = false;
    bool runesPending = false;
    uint32_t runesSaved = 0;
    std::string msg;
    double msgTime = 0;
};

struct WeaponRow {
    uint32_t id;
    int count;
    std::string name;
};

extern std::atomic<bool> g_menuOpen;
extern std::atomic<bool> g_wantWeapons;

void start();
void post(Act a, uint32_t arg = 0);
Status status();
std::vector<WeaponRow> weapons();
double now();

struct GameMenu {
    const char* label;
    const char* group;
    int event;
    std::vector<int> args;
};
const std::vector<GameMenu>& game_menus();

}
