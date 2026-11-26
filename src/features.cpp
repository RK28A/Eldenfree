#include "features.h"
#include "game.h"
#include "mem.h"
#include "config.h"
#include <windows.h>
#include <mutex>
#include <deque>
#include <map>
#include <cmath>
#include <algorithm>
#include <unordered_set>

namespace feat {

std::atomic<bool> g_menuOpen{false};
std::atomic<bool> g_wantWeapons{false};

static std::mutex s_m;
static Status s_st;
static std::vector<WeaponRow> s_weps;
static std::deque<std::pair<Act, uint32_t>> s_q;
static std::atomic<uintptr_t> s_torrent{0};
static std::atomic<uintptr_t> s_player{0};

double now() {
    static LARGE_INTEGER f = [] { LARGE_INTEGER x; QueryPerformanceFrequency(&x); return x; }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

void post(Act a, uint32_t arg) {
    std::lock_guard<std::mutex> lk(s_m);
    s_q.push_back({a, arg});
}

Status status() {
    std::lock_guard<std::mutex> lk(s_m);
    return s_st;
}

std::vector<WeaponRow> weapons() {
    std::lock_guard<std::mutex> lk(s_m);
    return s_weps;
}

static void msg(const std::string& m) {
    elog("%s", m.c_str());
    std::lock_guard<std::mutex> lk(s_m);
    s_st.msg = m;
    s_st.msgTime = now();
}

static int set_flags(const std::vector<uint32_t>& flags) {
    int ok = 0;
    for (uint32_t f : flags)
        if (game::flag_set(f, true)) ++ok;
    return ok;
}

static bool need_params() {
    if (game::params_ready()) return true;
    game::params_load();
    return game::params_ready();
}

static const uint32_t kGraces[] = {
    71190, 76101, 76100, 76111, 76120, 76103, 76104, 76105, 76106, 76108, 76110, 76113, 76114, 76116, 76119,
    73002, 73004, 73103, 73115, 73100, 73117, 73201, 71800, 71801, 76102, 71002, 71001, 76118, 76117, 73011,
    73410, 73412, 76150, 76151, 76152, 76153, 76154, 76155, 76156, 76162, 76157, 76158, 76159, 76160, 76161,
    73001, 73000, 73101, 73102, 73200, 71008, 71003, 71004, 71005, 71006, 71007, 71000, 76200, 76201, 76202,
    76203, 76204, 76205, 76206, 76244, 76221, 76222, 76217, 76223, 76247, 76245, 76234, 76224, 76225, 76226,
    76215, 76216, 76236, 76219, 76220, 76237, 76218, 76241, 76243, 76210, 76227, 76211, 76242, 76233, 76212,
    76213, 76214, 76230, 76231, 76232, 76228, 76238, 76235, 76229, 73006, 73003, 73005, 73104, 73105, 73106,
    73202, 73420, 73421, 73422, 76207, 76208, 76209, 76239, 76240, 73901, 73902, 73900, 76250, 76251, 76252,
    71402, 71403, 71401, 71400, 76300, 76302, 76301, 76303, 76304, 76306, 76308, 76313, 76307, 76305, 76320,
    76321, 76322, 73012, 73008, 73119, 73118, 73204, 73205, 76350, 76351, 76352, 76353, 76354, 76355, 76356,
    76357, 73007, 73009, 73107, 73109, 76309, 76310, 76312, 76311, 76314, 73013, 73010, 73431, 73432, 73430,
    71602, 71603, 71601, 71604, 71605, 71606, 71607, 71600, 71101, 71102, 71103, 71104, 71105, 71107, 71108,
    71109, 71100, 73501, 73502, 73503, 73504, 73500, 71122, 71123, 71121, 71124, 71125, 71120, 71900, 76400,
    76401, 76403, 76402, 76409, 76405, 76404, 76411, 76414, 76416, 76415, 76418, 76410, 76417, 76420, 76419,
    76422, 73014, 73015, 73016, 73120, 73121, 73207, 73257, 73208, 76406, 76407, 76412, 76413, 76450, 76451,
    76453, 76452, 76455, 76456, 76454, 73111, 73110, 73441, 73440, 73460, 76500, 76502, 73020, 73450, 73451,
    76501, 76503, 76504, 76505, 76520, 76521, 76522, 76523, 76524, 73122, 76506, 76507, 76508, 76509, 76510,
    73018, 73017, 76550, 76551, 76652, 76653, 73019, 73112, 73211, 71506, 71507, 71508, 71505, 71501, 71502,
    71503, 71504, 71500, 71211, 71212, 71213, 71210, 71240, 71214, 71215, 71219, 71216, 71218, 71271, 71221,
    71224, 71226, 71225, 71220, 71270, 71222, 71223, 71227, 71251, 71252, 71253, 71250, 71231, 71232, 71233,
    71234, 71235, 71230, 71303, 71304, 71305, 71306, 71307, 71302, 71308, 71309, 71310, 71301, 71300, 76800,
    76801, 76802, 76805, 76803, 76804, 76813, 76810, 76811, 76812, 76830, 74000, 74100, 74200, 74300, 74301,
    74351, 76821, 76822, 76823, 76831, 76832, 76835, 76833, 76834, 76841, 74102, 72201, 72202, 72203, 72204,
    72200, 76840, 76850, 76851, 76852, 76853, 72001, 72002, 72003, 72000, 72012, 72013, 72014, 72015, 72016,
    72010, 76940, 76941, 76942, 76944, 76943, 76945, 76912, 76913, 76914, 74001, 74203, 76900, 76907, 76902,
    76908, 76909, 76910, 76911, 76903, 76904, 76905, 76906, 72500, 76916, 76917, 76918, 74101, 74002, 74202,
    76862, 76863, 76864, 76860, 76861, 72801, 72802, 72803, 72800, 72102, 72101, 72106, 72107, 72108, 72109,
    72111, 72112, 72113, 72114, 72116, 72117, 72120, 72110, 76930, 76931, 76960, 76935, 76937, 76936,
};

static void act_map() {
    std::vector<uint32_t> flags = {62004, 62005, 62006, 62007, 62008, 62009, 62010, 62011, 62012, 62020, 62021,
                                   62022, 62030, 62031, 62032, 62040, 62041, 62050, 62051, 62052, 62060, 62061,
                                   62062, 62063, 62064, 82001, 62080, 62081, 62082, 62083, 62084, 82002};
    int ok = set_flags(flags);
    msg("Carte : " + std::to_string(ok) + "/" + std::to_string(flags.size()) + " fragments débloqués");
}

static void act_graces() {
    std::vector<uint32_t> flags(std::begin(kGraces), std::end(kGraces));
    int ok = set_flags(flags);
    msg("Grâces : " + std::to_string(ok) + "/" + std::to_string(flags.size()) + " débloquées");
}

static void act_bells() {
    std::vector<uint32_t> flags;
    for (uint32_t i = 1; i <= 9; ++i) {
        flags.push_back(11109750 + i);
        flags.push_back(60700 + i);
    }
    int ok = set_flags(flags);
    msg("Cloches de forge : " + std::to_string(ok) + "/" + std::to_string(flags.size()) + " flags activés (Sœurs jumelles)");
}

static bool dupable(const game::InvItem& it) {
    if ((it.handle >> 28) != 0x8 || it.id >= 0x10000000) return false;
    if (it.id >= 50000000 && it.id < 54000000) return false;
    if (it.id - it.id % 10000 == 110000) return false;
    return true;
}

static void refresh_weapons() {
    std::vector<game::InvItem> inv;
    if (!game::inventory(inv)) {
        std::lock_guard<std::mutex> lk(s_m);
        s_weps.clear();
        return;
    }
    std::map<uint32_t, int> counts;
    for (auto& it : inv)
        if (dupable(it)) counts[it.id]++;
    std::vector<WeaponRow> rows;
    for (auto& [id, c] : counts) rows.push_back({id, c, game::weapon_name(id)});
    std::sort(rows.begin(), rows.end(), [](const WeaponRow& a, const WeaponRow& b) { return a.name < b.name; });
    std::lock_guard<std::mutex> lk(s_m);
    s_weps = std::move(rows);
}

static int count_weapon(uint32_t id) {
    std::vector<game::InvItem> inv;
    if (!game::inventory(inv)) return -1;
    int c = 0;
    for (auto& it : inv)
        if (dupable(it) && it.id == id) ++c;
    return c;
}

static void act_dup(uint32_t id) {
    if (!game::g.itemGib || !game::g.mapItemMan) { msg("Duplication : fonction ItemGib introuvable"); return; }
    if (!game::player()) { msg("Duplication : charge ta partie d'abord"); return; }
    int c = count_weapon(id);
    if (c <= 0) { msg("Duplication : arme absente de l'inventaire"); return; }
    if (c >= 3) { msg("Duplication : déjà 3 exemplaires, limite atteinte"); return; }
    game::give_item(id, 1);
    Sleep(400);
    int c2 = count_weapon(id);
    if (c2 > c) msg(game::weapon_name(id) + " : " + std::to_string(c2) + "/3");
    else msg("Duplication envoyée (vérifie ton inventaire / coffre)");
    refresh_weapons();
}

const std::vector<GameMenu>& game_menus() {
    static const std::vector<GameMenu> m = {
        {"Renforcer une arme", "Forge", 24, {0}},
        {"Table de forge", "Forge", 24, {1}},
        {"Cendres de guerre", "Forge", 48, {}},
        {"Renaissance", "Rennala", 113, {}},
        {"Apparence", "Rennala", 81, {0}},
        {"Monter de niveau", "Site de gr\xC3\xA2" "ce", 31, {}},
        {"R\xC3\xA9partir les fioles", "Site de gr\xC3\xA2" "ce", 105, {}},
        {"M\xC3\xA9moriser des sorts", "Site de gr\xC3\xA2" "ce", 28, {0, 0}},
        {"Mixture physick", "Site de gr\xC3\xA2" "ce", 130, {}},
        {"Grandes runes", "Site de gr\xC3\xA2" "ce", 137, {}},
        {"Retoucher une tenue", "Site de gr\xC3\xA2" "ce", 142, {111000, 111399}},
        {"Coffre", "Site de gr\xC3\xA2" "ce", 30, {}},
        {"Accorder les esprits", "Table ronde", 136, {}},
    };
    return m;
}

static std::unordered_set<int> s_castAnims, s_castNo;
static void cast_save();

static bool greyoll_reset() {
    bool ok = game::flag_set(GREYOLL_FLAG, false);
    game::flag_set(1050407800, false);
    game::flag_set(1050400599, false);
    return ok;
}

static bool greyoll_respawn() {
    bool ok = greyoll_reset();
    uint32_t ent = game::grace_entity(76450);
    bool warped = ok && game::warp_to(ent);
    elog("Greyoll : reset %d, teleportation grace %u : %d", (int)ok, ent, (int)warped);
    msg(!ok ? "Greyoll : flags introuvables"
            : warped ? "Greyoll réapparaît : téléportation à Butte des dragons ouest"
                     : "Greyoll : flags remis à zéro, téléporte-toi à une grâce pour recharger la zone");
    return ok;
}

static std::mutex s_dm;
static std::vector<std::tuple<double, Act, uint32_t>> s_delayed;

static void post_delayed(Act a, uint32_t arg, double secs) {
    std::lock_guard<std::mutex> lk(s_dm);
    s_delayed.push_back({now() + secs, a, arg});
}

static void pump_delayed() {
    std::vector<std::pair<Act, uint32_t>> due;
    {
        std::lock_guard<std::mutex> lk(s_dm);
        double t = now();
        for (auto it = s_delayed.begin(); it != s_delayed.end();) {
            if (std::get<0>(*it) <= t) { due.push_back({std::get<1>(*it), std::get<2>(*it)}); it = s_delayed.erase(it); }
            else ++it;
        }
    }
    for (auto& d : due) post(d.first, d.second);
}

static void on_grace_pick(int which) { post_delayed(Act::OpenMenu, which == 1 ? 3u : 0u, 1.2); }

static void process_actions() {
    for (;;) {
        std::pair<Act, uint32_t> a;
        {
            std::lock_guard<std::mutex> lk(s_m);
            if (s_q.empty()) return;
            a = s_q.front();
            s_q.pop_front();
        }
        switch (a.first) {
        case Act::ResetGreyoll:
            greyoll_respawn();
            break;
        case Act::UnlockMap: act_map(); break;
        case Act::UnlockGraces: act_graces(); break;
        case Act::UnlockBells: act_bells(); break;
        case Act::DupWeapon: act_dup(a.second); break;
        case Act::RefreshInv: refresh_weapons(); break;
        case Act::SaveCfg: cfg_save(); break;
        case Act::ResetCast:
            s_castAnims.clear();
            s_castNo.clear();
            cast_save();
            msg("Sorts appris effac\xC3\xA9s");
            break;
        case Act::OpenMenu: {
            auto& m = game_menus();
            if (a.second >= m.size()) break;
            if (!game::player()) { msg("Charge ta partie d'abord"); break; }
            if (!game::ezstate_event(m[a.second].event, m[a.second].args)) msg("Menus : fonction du jeu introuvable");
            break;
        }
        }
    }
}

static double s_greyollOnSince = 0;

static void tick_boss(double t, Status& st) {
    st.greyoll = game::flag_get(GREYOLL_FLAG);
    if (!g_cfg.greyollRespawn || st.greyoll != 1) { s_greyollOnSince = 0; return; }
    if (s_greyollOnSince == 0) s_greyollOnSince = t;
    if (t - s_greyollOnSince >= g_cfg.respawnDelay) {
        s_greyollOnSince = 0;
        greyoll_respawn();
    }
}

enum class RS { Alive, Dead };
static RS s_rs = RS::Alive;
static uint32_t s_runesSaved = 0, s_lastRunes = 0;
static int s_lastHp = -1;
static double s_aliveSince = 0;

static void tick_runes(double t, uintptr_t pl, Status& st) {
    uintptr_t P = game::pgd();
    if (!P) return;
    uint32_t runes = 0;
    if (!mem::rd(P + g_cfg.offRunes, runes)) return;
    st.runes = runes;
    int hp = -1, mhp = 0;
    if (pl) game::chr_hp(pl, hp, mhp);
    if (!pl) { s_lastHp = -1; s_aliveSince = 0; return; }
    if (s_rs == RS::Alive) {
        if (hp == 0 && s_lastHp > 0 && g_cfg.keepRunes) {
            s_runesSaved = runes ? runes : s_lastRunes;
            mem::wr(P + g_cfg.offRunes, (uint32_t)0);
            s_rs = RS::Dead;
            s_aliveSince = 0;
            elog("Mort détectée : %u runes mises de côté", s_runesSaved);
        } else if (hp > 0) {
            s_lastRunes = runes;
        }
    } else {
        if (hp > 0) {
            if (s_aliveSince == 0) s_aliveSince = t;
            if (t - s_aliveSince > 2.5) {
                uint64_t total = (uint64_t)s_runesSaved + runes;
                if (total > 999999999) total = 999999999;
                mem::wr(P + g_cfg.offRunes, (uint32_t)total);
                msg("Runes conservées : " + std::to_string(s_runesSaved));
                s_lastRunes = (uint32_t)total;
                s_rs = RS::Alive;
            }
        } else {
            s_aliveSince = 0;
        }
    }
    s_lastHp = hp;
    st.runesPending = s_rs == RS::Dead;
    st.runesSaved = s_runesSaved;
}

static double s_lastFallT = 0, s_landedSince = 0;
static float s_lastY = 0, s_vy = 0;
static bool s_haveY = false, s_shield = false;
static uintptr_t s_shieldChr[2] = {0, 0};
static uint8_t s_shieldPrev[2] = {0, 0};

static void shield_on(uintptr_t chrs[2]) {
    for (int c = 0; c < 2; ++c) {
        if (!chrs[c] || s_shieldChr[c] == chrs[c]) continue;
        uintptr_t d = game::chr_module(chrs[c], g_cfg.offDataMod);
        if (!d) continue;
        uint8_t f = mem::rdv<uint8_t>(d + g_cfg.offChrFlags);
        s_shieldChr[c] = chrs[c];
        s_shieldPrev[c] = f & 3;
        mem::wr(d + g_cfg.offChrFlags, (uint8_t)(f | 3));
    }
    s_shield = true;
}

static void shield_off() {
    for (int c = 0; c < 2; ++c) {
        if (!s_shieldChr[c]) continue;
        if (game::is_chr(s_shieldChr[c]))
            if (uintptr_t d = game::chr_module(s_shieldChr[c], g_cfg.offDataMod)) {
                uint8_t f = mem::rdv<uint8_t>(d + g_cfg.offChrFlags);
                mem::wr(d + g_cfg.offChrFlags, (uint8_t)((f & ~3) | s_shieldPrev[c]));
            }
        s_shieldChr[c] = 0;
    }
    s_shield = false;
}

static void tick_fall(double t, uintptr_t pl, uintptr_t tor, bool mounted, Status& st) {
    if (!g_cfg.noFallDamage || !pl) {
        shield_off();
        s_haveY = false;
        st.fallProtect = false;
        return;
    }
    uintptr_t chrs[2] = {pl, mounted ? tor : 0};
    float p[3];
    if (!game::chr_pos(pl, p)) { shield_off(); s_haveY = false; return; }
    if (!s_haveY || t - s_lastFallT > 1.0) {
        s_haveY = true;
        s_lastY = p[1];
        s_lastFallT = t;
        s_vy = 0;
    } else {
        float dy = p[1] - s_lastY;
        double dt = t - s_lastFallT;
        if (dy != 0.f || dt >= 0.1) {
            if (std::fabs(dy) > 60.f) s_vy = 0;
            else if (dt > 0.002) s_vy = s_vy * 0.4f + (dy / (float)dt) * 0.6f;
            s_lastY = p[1];
            s_lastFallT = t;
        }
    }
    bool falling = s_vy < -g_cfg.fallSpeed;
    for (int c = 0; c < 2; ++c) {
        if (!chrs[c]) continue;
        if (uintptr_t m = game::chr_module(chrs[c], g_cfg.offMaterialMod)) mem::wr(m + g_cfg.offMaterialNoFall, (uint8_t)1);
        if (uintptr_t ph = game::chr_module(chrs[c], g_cfg.offPhysMod))
            if (mem::rdv<uint8_t>(ph + g_cfg.offPhysFalling)) falling = true;
    }
    if (falling) {
        for (int c = 0; c < 2; ++c)
            if (chrs[c])
                if (uintptr_t f = game::chr_module(chrs[c], g_cfg.offFallMod)) mem::wr(f + g_cfg.offFallTimer, 0.f);
        if (uintptr_t r = game::chr_module(pl, g_cfg.offRideMod)) mem::wr(r + g_cfg.offRideFallHeight, 0.f);
        shield_on(chrs);
        s_landedSince = 0;
    } else if (s_shield) {
        if (s_landedSince == 0) s_landedSince = t;
        if (t - s_landedSince > 0.6) shield_off();
    }
    st.fallProtect = s_shield;
}

static bool s_tHave = false, s_tTouched = false;
static uintptr_t s_tChr = 0;
static float s_tLast[3];
static double s_tLastT = 0;
static float s_tSpeed = 0, s_tMult = 1.f;

static void torrent_restore() {
    if (s_tTouched && s_tChr && game::is_chr(s_tChr)) game::set_anim_speed(s_tChr, 1.f);
    s_tTouched = false;
    s_tMult = 1.f;
}

static void tick_torrent(double t, uintptr_t tor, bool mounted, Status& st) {
    st.torrentMult = 1.f;
    if (tor != s_tChr) {
        torrent_restore();
        s_tChr = tor;
        s_tHave = false;
    }
    if (!g_cfg.torrentSpeed || !tor || !mounted) {
        torrent_restore();
        s_tHave = false;
        s_tSpeed = 0;
        st.torrentBase = 0;
        return;
    }
    float target = g_cfg.torrentMult;
    target = std::clamp(target, 1.f, 3.f);
    float cur = 1.f;
    game::anim_speed(tor, cur);
    if (std::fabs(cur - target) > 0.001f) game::set_anim_speed(tor, target);
    s_tMult = target;
    s_tTouched = true;
    st.torrentMult = target;
}

static void tick_patches(Status& st) {
    static int lastWarp = -1;
    st.pMap = game::patch_found(game::P_MAP1) && game::patch_found(game::P_MAP2);
    st.pRide = game::patch_found(game::P_RIDE);
    st.pJump = game::patch_found(game::P_HJUMP2);
    st.pAir = game::patch_found(game::P_HFALL);
    if (st.pMap) {
        game::patch_set(game::P_MAP1, g_cfg.mapCombat);
        game::patch_set(game::P_MAP2, g_cfg.mapCombat);
    }
    if (st.pRide) game::patch_set(game::P_RIDE, g_cfg.rideAnywhere);
    if (st.pJump) game::patch_set(game::P_HJUMP2, g_cfg.torrentInfJump);
    if (st.pAir) game::patch_set(game::P_HFALL, g_cfg.torrentInfJump && g_cfg.torrentAirJump);
    int want = g_cfg.dungeonWarp ? 1 : 0;
    if (st.params && want != lastWarp) {
        int r = game::speffect_state_set(4270, want == 1);
        if (r == 1) lastWarp = want;
        else if (r == -2) { lastWarp = want; elog("SpEffectParam : taille de ligne inattendue, teleportation donjons desactivee"); }
    }
    st.warpOk = lastWarp >= 0;
}

static bool s_castLoaded = false, s_castActive = false;
static uint32_t s_lastFp = 0;
static int s_lastCastHp = 0;
static int s_pendAnim = -1;
static double s_pendT = 0;
static std::vector<std::pair<int, float>> s_pendFx;

static void parse_set(const std::string& c, std::unordered_set<int>& out) {
    size_t i = 0;
    while (i < c.size()) {
        size_t k = c.find(',', i);
        int v = atoi(c.substr(i, k == std::string::npos ? std::string::npos : k - i).c_str());
        if (v > 0) out.insert(v);
        if (k == std::string::npos) break;
        i = k + 1;
    }
}

static std::string join_set(const std::unordered_set<int>& v) {
    std::string r;
    for (int a : v) r += (r.empty() ? "" : ",") + std::to_string(a);
    return r;
}

static void cast_save() {
    g_cfg.castAnims = join_set(s_castAnims);
    g_cfg.castAnimsNo = join_set(s_castNo);
    cfg_save();
}

static void tick_cast(uintptr_t pl, Status& st) {
    if (!s_castLoaded) {
        s_castLoaded = true;
        parse_set(g_cfg.castAnims, s_castAnims);
        parse_set(g_cfg.castAnimsNo, s_castNo);
    }
    st.castLearned = (int)s_castAnims.size();
    st.castAttack = (int)s_castNo.size();
    if (!pl) { s_castActive = false; s_pendAnim = -1; return; }
    uintptr_t P = game::pgd();
    uint32_t fp = P ? mem::rdv<uint32_t>(P + g_cfg.offPgdFp) : 0;
    int hp = 0, mhp = 0;
    game::chr_hp(pl, hp, mhp);
    int anim = game::cur_anim(pl);
    double t = now();
    if (g_cfg.fastCast && s_pendAnim < 0 && anim > 0 && s_lastFp && fp < s_lastFp && hp >= s_lastCastHp &&
        !s_castAnims.count(anim) && !s_castNo.count(anim)) {
        s_pendAnim = anim;
        s_pendT = t;
        game::speffects(pl, s_pendFx);
    }
    if (s_pendAnim > 0) {
        std::vector<std::pair<int, float>> fx;
        game::speffects(pl, fx);
        bool buff = false;
        for (auto& e : fx) {
            if (e.second < 8.f || e.second > 3600.f) continue;
            bool known = false;
            for (auto& o : s_pendFx)
                if (o.first == e.first) { known = true; break; }
            if (!known) { buff = true; break; }
        }
        if (buff || t - s_pendT > 2.5) {
            (buff ? s_castAnims : s_castNo).insert(s_pendAnim);
            elog("Animation %d apprise : %s", s_pendAnim, buff ? "buff" : "attaque");
            s_pendAnim = -1;
            cast_save();
        }
    }
    s_lastFp = fp;
    s_lastCastHp = hp;
    bool want = g_cfg.fastCast && anim > 0 && s_castAnims.count(anim);
    if (want) {
        game::set_anim_speed(pl, std::clamp(g_cfg.castSpeed, 1.f, 10.f));
        s_castActive = true;
    } else if (s_castActive) {
        game::set_anim_speed(pl, 1.f);
        s_castActive = false;
    }
    st.casting = s_castActive;
}

static DWORD WINAPI scan_thread(void*) {
    ULONGLONG lastNames = 0;
    for (;;) {
        Sleep(300);
        uintptr_t pl = s_player.load();
        ULONGLONG tk = GetTickCount64();
        if (pl && game::params_ready() && !game::msgs_ready() && (!lastNames || tk - lastNames > 30000)) {
            lastNames = tk;
            game::names_load();
        }
        if (g_cfg.graceMenu && game::msgs_ready() && !game::esd_ready()) {
            game::esd_set_callback(on_grace_pick);
            game::esd_install();
        }
        if (!pl || !(g_cfg.torrentSpeed || g_cfg.noFallDamage)) { s_torrent = 0; continue; }
        uintptr_t tor = s_torrent.load();
        if (tor && game::is_chr(tor)) continue;
        uintptr_t found = game::find_torrent(pl);
        if (found != tor) {
            if (found) elog("Torrent trouvé @ %p (chr %u)", (void*)found, game::chr_id(found));
            s_torrent = found;
        }
    }
}

static DWORD WINAPI tick_thread(void*) {
    double lastResolve = -100, lastParams = -100, lastWeps = -100;
    for (;;) {
        double t = now();
        Status st;
        {
            std::lock_guard<std::mutex> lk(s_m);
            st = s_st;
        }
        if (!st.core && t - lastResolve > 2.0) {
            lastResolve = t;
            st.core = game::resolve();
        }
        if (st.core) {
            if (!game::params_ready() && t - lastParams > 5.0) {
                lastParams = t;
                game::params_load();
            }
            st.params = game::params_ready();
            st.paramCount = game::params_count();
            st.itemGib = game::g.itemGib && game::g.mapItemMan;
            st.menus = game::ezstate_ready();
            process_actions();
            tick_boss(t, st);
            st.flags = st.greyoll >= 0;
            uintptr_t pl = game::player();
            s_player = pl;
            st.player = pl != 0;
            st.playerOff = game::player_offset();
            uintptr_t tor = s_torrent.load();
            bool mounted = false;
            if (pl && tor && !game::is_chr(tor)) { tor = 0; s_torrent = 0; }
            if (pl) mounted = game::is_mounted(pl);
            st.torrentFound = tor != 0;
            st.mounted = mounted;
            tick_runes(t, pl, st);
            tick_fall(t, pl, tor, mounted, st);
            tick_torrent(t, tor, mounted, st);
            tick_patches(st);
            pump_delayed();
            st.esd = game::esd_ready();
            st.warp = game::g.luaWarp && game::g.luaEventMan;
            tick_cast(pl, st);
            st.names = game::names_ready();
            if (pl && g_menuOpen && g_wantWeapons && t - lastWeps > 1.5) {
                lastWeps = t;
                refresh_weapons();
            }
        }
        {
            std::lock_guard<std::mutex> lk(s_m);
            std::string m = s_st.msg;
            double mt = s_st.msgTime;
            s_st = st;
            if (s_st.msgTime < mt) { s_st.msg = m; s_st.msgTime = mt; }
        }
        Sleep(5);
    }
}

void start() {
    CreateThread(nullptr, 0, tick_thread, nullptr, 0, nullptr);
    CreateThread(nullptr, 0, scan_thread, nullptr, 0, nullptr);
}

}
