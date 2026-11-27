#include "features.h"
#include "game.h"
#include "mem.h"
#include "config.h"
#include "overlay.h"
#include <windows.h>
#include <xinput.h>
#include <mutex>
#include <deque>
#include <map>
#include <cmath>
#include <cwctype>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>

namespace feat {

std::atomic<bool> g_menuOpen{false};
std::atomic<bool> g_wantWeapons{false};
std::atomic<bool> g_gameMenu{false};
std::atomic<bool> g_fromGameMenu{false};

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

static const uint32_t kGracesBase[] = {
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
    71234, 71235, 71230, 71303, 71304, 71305, 71306, 71307, 71302, 71308, 71309, 71310, 71301, 71300,
};

static const uint32_t kGracesDlc[] = {
    76800, 76801, 76802, 76805, 76803, 76804, 76813, 76810, 76811, 76812, 76830, 74000, 74100, 74200, 74300,
    74301, 74351, 76821, 76822, 76823, 76831, 76832, 76835, 76833, 76834, 76841, 74102, 72201, 72202, 72203,
    72204, 72200, 76840, 76850, 76851, 76852, 76853, 72001, 72002, 72003, 72000, 72012, 72013, 72014, 72015,
    72016, 72010, 76940, 76941, 76942, 76944, 76943, 76945, 76912, 76913, 76914, 74001, 74203, 76900, 76907,
    76902, 76908, 76909, 76910, 76911, 76903, 76904, 76905, 76906, 72500, 76916, 76917, 76918, 74101, 74002,
    74202, 76862, 76863, 76864, 76860, 76861, 72801, 72802, 72803, 72800, 72102, 72101, 72106, 72107, 72108,
    72109, 72111, 72112, 72113, 72114, 72116, 72117, 72120, 72110, 76930, 76931, 76960, 76935, 76937, 76936,
};

static const uint32_t kMapBase[] = {62004, 62005, 62006, 62007, 62008, 62009, 62010, 62011, 62012, 62020, 62021, 62022, 62030,
                                    62031, 62032, 62040, 62041, 62050, 62051, 62052, 62060, 62061, 62062, 62063, 62064, 82001};
static const uint32_t kMapDlc[] = {62080, 62081, 62082, 62083, 62084, 82002};

static bool is_map_goods(uint32_t id) {
    std::wstring n;
    if (!game::goods_name(id, n) || n.size() < 5) return false;
    for (auto& c : n) c = (wchar_t)towlower(c);
    if (n.rfind(L"map", 0) == 0 && n.find(L':') != std::wstring::npos && n.find(L':') < 6) return true;
    if (n.rfind(L"carte", 0) == 0 && n.find(L':') != std::wstring::npos && n.find(L':') < 9) return true;
    return false;
}

static void map_extra(bool dlc, bool on) {
    if (!need_params()) return;
    int pieces = 0, lots = 0;
    for (uintptr_t t : game::param_tables("WORLD_MAP_PIECE_PARAM_ST"))
        for (int i = 0, n = game::param_rows(t); i < n; ++i) {
            int32_t id;
            uintptr_t d;
            if (!game::param_row(t, i, id, d)) continue;
            uint32_t open = mem::rdv<uint32_t>(d + 0x4), acq = mem::rdv<uint32_t>(d + 0x18);
            if (open < 62000 || open >= 63000) continue;
            if ((open >= 62080 && open < 62100) != dlc) continue;
            game::flag_set(open, on);
            if (acq) game::flag_set(acq, on);
            ++pieces;
        }
    std::unordered_map<uint32_t, bool> cache;
    for (uintptr_t t : game::param_tables("ITEMLOT_PARAM_ST"))
        for (int i = 0, n = game::param_rows(t); i < n; ++i) {
            int32_t id;
            uintptr_t d;
            if (!game::param_row(t, i, id, d)) continue;
            uint32_t flag = mem::rdv<uint32_t>(d + 0x80);
            if (!flag) continue;
            for (int k = 0; k < 8; ++k) {
                int32_t item = mem::rdv<int32_t>(d + k * 4), cat = mem::rdv<int32_t>(d + 0x20 + k * 4);
                if (item <= 0 || cat != 1) continue;
                if (((uint32_t)item >= 2000000) != dlc) continue;
                auto it = cache.find(item);
                bool m = it != cache.end() ? it->second : (cache[item] = is_map_goods(item));
                if (!m) continue;
                game::flag_set(flag, on);
                ++lots;
                break;
            }
        }
    elog("Carte %s : %d pieces, %d fragments au sol %s", dlc ? "DLC" : "base", pieces, lots, on ? "ramasses" : "remis");
}

template <size_t N> static bool all_set(const uint32_t (&f)[N]) {
    for (uint32_t x : f)
        if (game::flag_get(x) != 1) return false;
    return true;
}

template <size_t N> static void set_group(const uint32_t (&f)[N], bool on, const char* what) {
    int ok = 0;
    for (uint32_t x : f)
        if (game::flag_set(x, on)) ++ok;
    msg(std::string(what) + (on ? " : d\xC3\xA9" "bloqu\xC3\xA9" "es (" : " : verrouill\xC3\xA9" "es (") + std::to_string(ok) + "/" + std::to_string(N) + ")");
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

static std::unordered_set<int> s_castAnims, s_castNo, s_aowBuff, s_aowNo, s_flailAnims;
static void cast_save();
static void flail_save();


static bool greyoll_reset() {
    bool ok = game::flag_set(GREYOLL_FLAG, false);
    game::flag_set(1050407800, false);
    game::flag_set(1050400599, false);
    return ok;
}

static bool greyoll_respawn() {
    bool ok = greyoll_reset();
    uint32_t ent = game::grace_entity(76453);
    if (!ent || ent / 1000 != 1051391) ent = 1051391950;
    bool warped = ok && game::warp_to(ent);
    elog("Greyoll : reset %d, teleportation grace %u : %d", (int)ok, ent, (int)warped);
    msg(!ok ? "Greyoll : flags introuvables"
            : warped ? "Greyoll réapparaît : téléportation au Fort Faroth"
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

static void act_bloodboil() {
    if (!game::player()) { msg("Charge ta partie d'abord"); return; }
    if (!game::g.itemGib || !game::g.mapItemMan) { msg("Fonction d'obtention introuvable"); return; }
    uint32_t id = 3550;
    auto gt = game::param_tables("EQUIP_PARAM_GOODS_ST");
    uintptr_t row = gt.empty() ? 0 : game::param_find(gt[0], (int32_t)id);
    if (!row) {
        id = game::find_name({10, 111, 319, 419}, {{L"bloodboil"}, {L"ar\u00f4me", L"bouillesang"}, {L"ar\u00f4me", L"bouill"}, {L"arome", L"bouill"}});
        row = (id && !gt.empty()) ? game::param_find(gt[0], (int32_t)id) : 0;
    }
    if (!row) { msg("Ar\xC3\xB4me : param\xC3\xA8tres introuvables"); return; }
    int maxNum = mem::rdv<int16_t>(row + 0x3A);
    if (maxNum <= 0 || maxNum > 999) maxNum = 10;
    std::vector<game::InvItem> inv;
    int held = 0;
    if (game::inventory(inv))
        for (auto& it : inv)
            if ((it.id & 0x0FFFFFFF) == id && (it.id >> 28) == 0x4) held += (int)it.qty;
    int give = maxNum - held;
    if (give <= 0) { msg("Ar\xC3\xB4mes de sang bouillant : d\xC3\xA9j\xC3\xA0 au maximum (" + std::to_string(held) + "/" + std::to_string(maxNum) + ")"); return; }
    game::give_item(0x40000000u | id, give);
    elog("Arome sang bouillant %u : +%d (max %d)", id, give, maxNum);
    msg("Ar\xC3\xB4mes de sang bouillant : +" + std::to_string(give) + " (" + std::to_string(maxNum) + "/" + std::to_string(maxNum) + ")");
}

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
        case Act::MapBase: set_group(kMapBase, a.second != 0, "Carte (jeu de base)"); map_extra(false, a.second != 0); break;
        case Act::MapDlc: set_group(kMapDlc, a.second != 0, "Carte (DLC)"); map_extra(true, a.second != 0); break;
        case Act::GracesBase: set_group(kGracesBase, a.second != 0, "Gr\xC3\xA2" "ces (jeu de base)"); break;
        case Act::GracesDlc: set_group(kGracesDlc, a.second != 0, "Gr\xC3\xA2" "ces (DLC)"); break;
        case Act::UnlockMap: break;
        case Act::UnlockGraces: break;
        case Act::UnlockBells: act_bells(); break;
        case Act::DupWeapon: act_dup(a.second); break;
        case Act::RefreshInv: refresh_weapons(); break;
        case Act::SaveCfg: cfg_save(); break;
        case Act::GiveBloodboil: act_bloodboil(); break;
        case Act::ResetFlail:
            s_flailAnims.clear();
            flail_save();
            msg("Animations du fl\xC3\xA9" "au oubli\xC3\xA9" "es");
            break;
        case Act::ResetJump:
            g_cfg.jumpFlags.clear();
            cfg_save();
            msg("Calibrage du double saut effac\xC3\xA9");
            break;
        case Act::ResetCast:
            s_castAnims.clear();
            s_aowBuff.clear();
            s_aowNo.clear();
            g_cfg.aowBuff.clear();
            g_cfg.aowNo.clear();
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
    float target = std::clamp(g_cfg.torrentMult, 1.f, 3.f);
    uintptr_t tph = game::chr_module(tor, g_cfg.offPhysMod);
    bool grounded = tph && mem::rdv<uint8_t>(tph + 0x1D1) != 0;
    if (!grounded) target = 1.f;
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
    if (game::patch_found(game::P_RIDE2)) game::patch_set(game::P_RIDE2, g_cfg.rideAnywhere);
    st.pJump = game::patch_found(game::P_HJUMP1) && game::patch_found(game::P_HJUMP2);
    st.pAir = false;
    if (st.pMap) {
        game::patch_set(game::P_MAP1, g_cfg.mapCombat);
        game::patch_set(game::P_MAP2, g_cfg.mapCombat);
    }
    if (st.pRide) game::patch_set(game::P_RIDE, g_cfg.rideAnywhere);
    if (st.pJump) {
        game::patch_set(game::P_HJUMP1, g_cfg.torrentInfJump);
        game::patch_set(game::P_HJUMP2, g_cfg.torrentInfJump);
    }
    int want = g_cfg.dungeonWarp ? 1 : 0;
    if (st.params && want != lastWarp) {
        int r = game::speffect_state_set(4270, want == 1);
        if (r == 1) lastWarp = want;
        else if (r == -2) { lastWarp = want; elog("SpEffectParam : taille de ligne inattendue, teleportation donjons desactivee"); }
    }
    st.warpOk = lastWarp >= 0;
}

static bool s_castLoaded = false, s_castActive = false;

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

static int s_animCur = -1;
static double s_animCurT = 0;

static bool selected_spell_is_buff() {
    int spell = game::selected_spell();
    auto mt = game::param_tables("MAGIC_PARAM_ST");
    uintptr_t row = (spell > 0 && !mt.empty()) ? game::param_find(mt[0], spell) : 0;
    return row && (mem::rdv<uint8_t>(row + 0x1E) == 2 || (mem::rdv<uint8_t>(row + 0x31) & 0x0C));
}

static int s_aowPend = -1;
static double s_aowPendT = 0;
static float s_aowPos[3], s_aowMaxDisp = 0;
static std::vector<std::pair<int, float>> s_aowFx;

static bool is_flail_spin(int anim) { return anim == 625040050 || anim == 625040051 || anim == 625040053; }

static void aow_save() {
    g_cfg.aowBuff = join_set(s_aowBuff);
    g_cfg.aowNo = join_set(s_aowNo);
    cfg_save();
}

static void tick_cast(uintptr_t pl, Status& st) {
    if (!s_castLoaded) {
        s_castLoaded = true;
        parse_set(g_cfg.aowBuff, s_aowBuff);
        s_aowBuff.insert(733040000);
        parse_set(g_cfg.aowNo, s_aowNo);
    }
    st.castLearned = 0;
    st.castAttack = (int)s_aowBuff.size();
    if (!pl) { s_castActive = false; return; }
    double t = now();
    int q[10];
    int qn = game::anim_queue(pl, q);
    bool spellAnim = false, physick = false;
    int aowAnim = -1, buffAnim = -1;
    for (int i = 0; i < qn; ++i) {
        int pre = q[i] / 1000000;
        if (pre >= 400 && pre < 500) spellAnim = true;
        if (q[i] % 1000000 == 50210) physick = true;
        if (s_aowBuff.count(q[i])) buffAnim = q[i];
        else if (pre >= 600 && pre < 900 && !is_flail_spin(q[i])) aowAnim = q[i];
    }
    int anim = buffAnim > 0 ? buffAnim : aowAnim;
    if (aowAnim > 0 && aowAnim != s_animCur) {
        s_animCur = aowAnim;
        s_animCurT = t;
        if (s_aowPend < 0) {
            s_aowPend = aowAnim;
            s_aowPendT = t;
            s_aowMaxDisp = 0;
            game::chr_pos(pl, s_aowPos);
            game::speffects(pl, s_aowFx);
        }
    }
    if (aowAnim <= 0 && buffAnim <= 0 && t - s_animCurT > 1.0) s_animCur = -1;
    if (s_aowPend > 0) {
        float p[3];
        if (game::chr_pos(pl, p)) {
            float dx = p[0] - s_aowPos[0], dz = p[2] - s_aowPos[2];
            s_aowMaxDisp = std::max(s_aowMaxDisp, std::sqrt(dx * dx + dz * dz));
        }
    }
    if (s_aowPend > 0 && t - s_aowPendT > 3.0) {
        std::vector<std::pair<int, float>> fx;
        game::speffects(pl, fx);
        bool buff = false;
        for (auto& e : fx) {
            if (e.second >= 0.f && (e.second < 5.f || e.second > 900.f)) continue;
            bool known = false;
            for (auto& o : s_aowFx)
                if (o.first == e.first) { known = true; break; }
            if (!known) { buff = true; break; }
        }
        if (s_aowMaxDisp > 3.f) buff = false;
        if (buff) { s_aowBuff.insert(s_aowPend); s_aowNo.erase(s_aowPend); }
        else s_aowNo.insert(s_aowPend);
        elog("Cendre de guerre anim %d : %s (deplacement %.1f m)", s_aowPend, buff ? "buff" : "attaque", s_aowMaxDisp);
        s_aowPend = -1;
        aow_save();
    }
    static int aowRun = -1;
    static float aowRunPos[3];
    if (anim > 0 && s_aowBuff.count(anim)) {
        float p[3];
        if (aowRun != anim) { aowRun = anim; game::chr_pos(pl, aowRunPos); }
        else if (game::chr_pos(pl, p)) {
            float dx = p[0] - aowRunPos[0], dz = p[2] - aowRunPos[2];
            if (dx * dx + dz * dz > 4.f * 4.f) {
                s_aowBuff.erase(anim);
                s_aowNo.insert(anim);
                aow_save();
                elog("Cendre de guerre anim %d retiree des buffs (deplacement)", anim);
            }
        }
    } else {
        aowRun = -1;
    }
    bool want = g_cfg.fastCast && ((spellAnim && selected_spell_is_buff()) || buffAnim > 0 || physick);
    if (want) {
        game::set_anim_speed(pl, std::clamp(g_cfg.castSpeed, 1.f, 10.f));
        s_castActive = true;
    } else if (s_castActive) {
        game::set_anim_speed(pl, 1.f);
        s_castActive = false;
    }
    st.casting = s_castActive;
}

static bool s_fly = false, s_flyHave = false, s_flyGrav = false;
static float s_flyPos[3], s_flyPrevGrav = 1.f;
static double s_jumpHeldSince = 0, s_flyLastT = 0;
static bool s_jumpConsumed = false;
static uintptr_t s_flyTor = 0;

static void fly_stop() {
    if (s_flyGrav && s_flyTor && game::is_chr(s_flyTor))
        if (uintptr_t ph = game::chr_module(s_flyTor, g_cfg.offPhysMod)) mem::wr(ph + 0x1CC, s_flyPrevGrav);
    s_flyGrav = false;
    s_fly = false;
    s_flyHave = false;
}

static float wrap_pi(float a) {
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}

static void tick_fly(double t, uintptr_t pl, uintptr_t tor, bool mounted, Status& st) {
    st.flying = s_fly;
    if (!g_cfg.torrentFly || !mounted || !tor || g_menuOpen) {
        if (s_fly && (!mounted || !tor || !g_cfg.torrentFly)) { fly_stop(); msg("Vol de Torrent d\xC3\xA9sactiv\xC3\xA9"); }
        s_jumpHeldSince = 0;
        if (!s_fly || g_menuOpen) return;
    }
    bool fg = GetForegroundWindow() == overlay::hwnd();
    uint32_t pb = overlay::pad_buttons();
    bool jump = fg && ((GetAsyncKeyState(g_cfg.jumpKey) & 0x8000) || (pb & XINPUT_GAMEPAD_A));
    if (jump) {
        if (s_jumpHeldSince == 0) { s_jumpHeldSince = t; s_jumpConsumed = false; }
        if (!s_jumpConsumed && t - s_jumpHeldSince > 0.5) {
            s_jumpConsumed = true;
            if (s_fly) { fly_stop(); msg("Vol de Torrent d\xC3\xA9sactiv\xC3\xA9"); }
            else { s_fly = true; s_flyHave = false; msg("Vol de Torrent activ\xC3\xA9 (maintiens saut pour arr\xC3\xAAter)"); }
        }
    } else {
        s_jumpHeldSince = 0;
    }
    st.flying = s_fly;
    if (!s_fly) return;
    s_flyTor = tor;
    uintptr_t ph = game::chr_module(tor, g_cfg.offPhysMod);
    if (ph && !s_flyGrav) {
        s_flyPrevGrav = mem::rdv<float>(ph + 0x1CC, 1.f);
        if (s_flyPrevGrav == 0.f) s_flyPrevGrav = 1.f;
        s_flyGrav = true;
    }
    if (ph) {
        mem::wr(ph + 0x1CC, 0.f);
        float zv[3] = {0.f, 0.f, 0.f};
        mem::write_raw(ph + 0x120, zv, 12);
    }
    for (uintptr_t c : {pl, tor})
        if (uintptr_t f = game::chr_module(c, g_cfg.offFallMod)) mem::wr(f + g_cfg.offFallTimer, 0.f);
    float p[3];
    if (!game::chr_pos(tor, p)) return;
    double dt = t - s_flyLastT;
    s_flyLastT = t;
    float rx = p[0] - s_flyPos[0], ry = p[1] - s_flyPos[1], rz = p[2] - s_flyPos[2];
    if (!s_flyHave || dt > 0.25 || rx * rx + ry * ry + rz * rz > 16.f) {
        memcpy(s_flyPos, p, 12);
        s_flyHave = true;
        return;
    }
    uintptr_t wcm = mem::ptr(game::g.worldChrMan);
    uintptr_t cam = wcm ? mem::ptr(wcm + 0x1ECF0) : 0;
    float fwd[3] = {0, 0, 0}, cpos[3] = {0, 0, 0};
    if (cam) {
        mem::read_raw(cam + 0x30, fwd, 12);
        mem::read_raw(cam + 0x40, cpos, 12);
    }
    (void)cpos;
    float fl = std::sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
    if (fl > 1e-4f) { fwd[0] /= fl; fwd[1] /= fl; fwd[2] /= fl; }
    float iy = 0;
    if (fg) {
        auto key = [](UINT sc) { return (GetAsyncKeyState(MapVirtualKeyW(sc, MAPVK_VSC_TO_VK)) & 0x8000) != 0; };
        if (key(0x11)) iy += 1;
        if (key(0x1F)) iy -= 1;
        float sx, sy;
        overlay::pad_stick(sx, sy);
        if (std::fabs(sy) > 0.25f) iy += sy;
    }
    iy = std::clamp(iy, -1.f, 1.f);
    float speed = 10.35f * (g_cfg.torrentSpeed ? std::clamp(g_cfg.torrentMult, 1.f, 3.f) : 1.f);
    for (int i = 0; i < 3; ++i) s_flyPos[i] += fwd[i] * iy * speed * (float)dt;
    game::chr_set_pos(tor, s_flyPos);
    if (ph && (std::fabs(fwd[0]) + std::fabs(fwd[2]) > 1e-3f)) {
        float yaw = wrap_pi(std::atan2(-fwd[0], -fwd[2]));
        float q[4] = {0.f, std::sin(yaw * 0.5f), 0.f, std::cos(yaw * 0.5f)};
        mem::write_raw(ph + 0x50, q, 16);
        mem::write_raw(ph + 0x60, q, 16);
    }
}

static int s_wSp = -1;
static float s_wOrig = 0.f;
static bool s_wTried = false, s_wApplied = false;

static void tick_weight(uintptr_t pl, Status& st) {
    uintptr_t P = game::pgd();
    if (P) st.equipLoad = mem::rdv<float>(P + 0x98C, 0.f);
    if (!pl || !game::names_ready() || !game::params_ready()) return;
    if (!s_wTried) {
        s_wTried = true;
        uint32_t acc = 1032;
        s_wSp = 310320;
        auto stb = game::param_tables("SP_EFFECT_PARAM_ST");
        uintptr_t sp = (s_wSp > 0 && !stb.empty()) ? game::param_find(stb[0], s_wSp) : 0;
        if (sp) s_wOrig = mem::rdv<float>(sp + 0xE4, 1.f);
        elog("Charge : talisman %u, speffect %d, taux d'origine %.2f", acc, s_wSp, s_wOrig);
        if (!sp) s_wSp = -1;
    }
    st.equipLoadBase = s_wSp;
    if (s_wSp <= 0 || !game::speffect_ready()) return;
    auto stb = game::param_tables("SP_EFFECT_PARAM_ST");
    uintptr_t sp = game::param_find(stb[0], s_wSp);
    if (!sp) return;
    float mult = std::clamp(g_cfg.weightMult, 1.f, 10.f);
    if (mult > 1.001f) {
        if (std::fabs(mem::rdv<float>(sp + 0xE4) - mult) > 0.001f) mem::wr(sp + 0xE4, mult);
        std::vector<std::pair<int, float>> fx;
        game::speffects(pl, fx);
        bool has = false;
        for (auto& e : fx)
            if (e.first == s_wSp) { has = true; break; }
        if (!has) game::speffect_add(pl, s_wSp);
        s_wApplied = true;
    } else if (s_wApplied) {
        mem::wr(sp + 0xE4, s_wOrig);
        game::speffect_remove(pl, s_wSp);
        s_wApplied = false;
    }
}

static bool s_flailLoaded = false, s_flailOn = false;
static float s_flailApplied = 1.f;
static uint32_t s_flailFp = 0;
static double s_flailLearnUntil = 0;
static uint32_t s_rwId = 0;
static bool s_rwFlail = false;

static void flail_save() {
    g_cfg.flailAnims = join_set(s_flailAnims);
    cfg_save();
}

static bool tick_flail(double t, uintptr_t pl, Status& st) {
    if (!s_flailLoaded) { s_flailLoaded = true; parse_set(g_cfg.flailAnims, s_flailAnims); }
    st.flailLearned = (int)s_flailAnims.size();
    uintptr_t P = game::pgd();
    if (!pl || !P) { s_flailOn = false; return false; }
    int slot = mem::rdv<int32_t>(P + 0x32C, 0);
    if (slot < 0 || slot > 2) slot = 0;
    uint32_t rw = mem::rdv<uint32_t>(P + 0x5FC + slot * 8, 0);
    if (rw != s_rwId) {
        s_rwId = rw;
        std::string n = (rw && rw < 0x10000000) ? game::weapon_name(rw) : "";
        st.rightWeapon = n;
        s_rwFlail = rw >= 13000000 && rw < 14000000;
        if (!game::names_ready() && n.empty()) st.rightWeapon.clear();
    }
    st.flailHeld = s_rwFlail;
    uint32_t fp = mem::rdv<uint32_t>(P + g_cfg.offPgdFp);
    int q[10];
    int qn = game::anim_queue(pl, q);
    int anim = -1;
    if (g_cfg.flailSpin && s_rwFlail && s_flailFp && fp < s_flailFp) s_flailLearnUntil = t + 3.0;
    s_flailFp = fp;
    for (int i = 0; i < qn; ++i) {
        int pre = q[i] / 1000000;
        bool skill = pre >= 600 && pre < 900;
        if (s_rwFlail && skill && t < s_flailLearnUntil && s_flailAnims.insert(q[i]).second) {
            elog("Fleau : animation %d apprise", q[i]);
            flail_save();
        }
        if (skill && (is_flail_spin(q[i]) || s_flailAnims.count(q[i]))) anim = q[i];
    }
    static uintptr_t cleanedChr = 0;
    uintptr_t dm = game::chr_module(pl, g_cfg.offDataMod);
    if (dm && cleanedChr != pl) {
        uint8_t f = mem::rdv<uint8_t>(dm + g_cfg.offChrFlags);
        if (f & 0x0C) mem::wr(dm + g_cfg.offChrFlags, (uint8_t)(f & ~0x0C));
        cleanedChr = pl;
    }
    float mult = std::clamp(g_cfg.flailMult, 1.f, 25.f);
    bool want = g_cfg.flailSpin && s_rwFlail && anim > 0;
    bool moving = false;
    if (GetForegroundWindow() == overlay::hwnd()) {
        auto key = [](UINT sc) { return (GetAsyncKeyState(MapVirtualKeyW(sc, MAPVK_VSC_TO_VK)) & 0x8000) != 0; };
        float sx, sy;
        overlay::pad_stick(sx, sy);
        moving = key(0x11) || key(0x1F) || key(0x1E) || key(0x20) || sx * sx + sy * sy > 0.06f;
    }
    if (want && moving) mult = 1.f;
    s_flailApplied = want ? mult : 1.f;
    if (want && dm) {
        int32_t fpMax = mem::rdv<int32_t>(dm + 0x14C), stMax = mem::rdv<int32_t>(dm + 0x158);
        if (fpMax > 0 && fpMax < 10000 && mem::rdv<int32_t>(dm + 0x148) < fpMax) {
            mem::wr(dm + 0x148, fpMax);
            mem::wr(P + g_cfg.offPgdFp, (uint32_t)fpMax);
        }
        if (stMax > 0 && stMax < 10000 && mem::rdv<int32_t>(dm + 0x154) < stMax) mem::wr(dm + 0x154, stMax);
    }
    if (want) {
        game::set_anim_speed(pl, mult);
        s_flailOn = true;
    } else if (s_flailOn) {
        game::set_anim_speed(pl, 1.f);
        s_flailOn = false;
    }
    st.flailActive = s_flailOn;
    return s_flailOn;
}

static bool s_jumpPrev = false;
static double s_vCancelUntil = 0, s_jumpPressT = -10, s_vBoostUntil = 0, s_vPeakUntil = 0;
static float s_vPeak = 10.f, s_vPeakCur = 0.f;

static bool s_jumpPress = false;

static void tick_jumpkey(double t) {
    bool fg = GetForegroundWindow() == overlay::hwnd();
    bool jump = fg && !g_menuOpen && ((GetAsyncKeyState(g_cfg.jumpKey) & 0x8000) || (overlay::pad_buttons() & XINPUT_GAMEPAD_A));
    s_jumpPress = jump && !s_jumpPrev;
    s_jumpPrev = jump;
    if (s_jumpPress) s_jumpPressT = t;
}

static void tick_fallvel(double t, uintptr_t tor, bool mounted, Status& st) {
    st.fallVelLearned = true;
    if (!g_cfg.torrentInfJump || !mounted || !tor) return;
    uintptr_t ph = game::chr_module(tor, g_cfg.offPhysMod);
    bool airborne = ph && mem::rdv<uint8_t>(ph + 0x1D1) == 0;
    bool press = s_jumpPress;
    if (airborne) {
        if (uintptr_t af = game::chr_module(tor, 0x08)) {
            uint8_t f = mem::rdv<uint8_t>(af + 0x214);
            if (f & 0x20) mem::wr(af + 0x214, (uint8_t)(f & ~0x20));
        }
    }
    if (!ph) return;
    float v = mem::rdv<float>(ph + 0x124, 0.f);
    if (press && airborne) {
        if (v < -1.f) {
            s_vBoostUntil = t + 0.15;
        } else {
            s_vPeakUntil = t + 0.5;
            s_vPeakCur = 0.f;
            s_vCancelUntil = t + 0.4;
        }
    }
    if (t < s_vPeakUntil) {
        s_vPeakCur = std::max(s_vPeakCur, v);
    } else if (s_vPeakCur > 4.f) {
        s_vPeak = std::clamp(s_vPeakCur, 6.f, 20.f);
        s_vPeakCur = 0.f;
    }
    if (t < s_vBoostUntil) {
        if (v < s_vPeak) {
            mem::wr(ph + 0x124, s_vPeak);
            mem::wr(ph + 0x134, s_vPeak);
        }
    } else if (t < s_vCancelUntil) {
        if (v < 0.f) {
            mem::wr(ph + 0x124, 0.f);
            mem::wr(ph + 0x134, 0.f);
        } else {
            s_vCancelUntil = 0;
        }
    }
}

static bool s_jhHave = false;
static float s_jhLastY = 0;

static void tick_jumpheight(uintptr_t tor, bool mounted) {
    float mult = std::clamp(g_cfg.jumpHeight, 1.f, 10.f);
    if (!g_cfg.jumpHeightOn || mult <= 1.001f || !mounted || !tor || s_fly) { s_jhHave = false; return; }
    uintptr_t ph = game::chr_module(tor, g_cfg.offPhysMod);
    float p[3];
    if (!ph || !game::chr_pos(tor, p)) { s_jhHave = false; return; }
    bool airborne = mem::rdv<uint8_t>(ph + 0x1D1) == 0;
    if (!s_jhHave || !airborne || now() - s_jumpPressT > 1.0) { s_jhLastY = p[1]; s_jhHave = true; return; }
    float dy = p[1] - s_jhLastY;
    if (dy > 0.f && dy < 1.5f) {
        float np[3] = {p[0], p[1] + dy * (mult - 1.f), p[2]};
        game::chr_set_pos(tor, np);
        s_jhLastY = np[1];
    } else {
        s_jhLastY = p[1];
    }
}

static bool s_mcHave = false;
static float s_mcLast[3];

static void move_comp(uintptr_t pl, float mult, bool active) {
    if (!active || mult <= 1.001f || !pl) { s_mcHave = false; return; }
    float p[3];
    if (!game::chr_pos(pl, p)) { s_mcHave = false; return; }
    if (!s_mcHave) { memcpy(s_mcLast, p, 12); s_mcHave = true; return; }
    float dx = p[0] - s_mcLast[0], dz = p[2] - s_mcLast[2];
    if (dx * dx + dz * dz > 25.f) { memcpy(s_mcLast, p, 12); return; }
    if (dx == 0.f && dz == 0.f) return;
    float nx = s_mcLast[0] + dx / mult, nz = s_mcLast[2] + dz / mult;
    game::chr_set_xz(pl, nx, nz);
    s_mcLast[0] = nx;
    s_mcLast[1] = p[1];
    s_mcLast[2] = nz;
}

static DWORD WINAPI scan_thread(void*) {
    ULONGLONG lastNames = 0;
    for (;;) {
        Sleep(300);
        uintptr_t pl = s_player.load();
        ULONGLONG tk = GetTickCount64();
        static int esdTries = 0;
        bool needMsgs = !game::msgs_ready() || (g_cfg.graceMenu && !game::esd_ready() && esdTries < 8);
        if (pl && game::params_ready() && needMsgs && (!lastNames || tk - lastNames > 20000)) {
            lastNames = tk;
            game::names_load();
            (void)esdTries;
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
            if (!game::g.mapItemMan) game::g.mapItemMan = game::singleton("MapItemMan");
            st.itemGib = game::g.itemGib && game::g.mapItemMan;
            st.menus = game::ezstate_ready();
            process_actions();
            tick_boss(t, st);
            st.flags = st.greyoll >= 0;
            static double lastFlagsT = -10;
            static bool fb[4];
            if (t - lastFlagsT > 1.0) {
                lastFlagsT = t;
                fb[0] = all_set(kMapBase);
                fb[1] = all_set(kMapDlc);
                fb[2] = all_set(kGracesBase);
                fb[3] = all_set(kGracesDlc);
            }
            st.mapBase = fb[0];
            st.mapDlc = fb[1];
            st.gracesBase = fb[2];
            st.gracesDlc = fb[3];
            uintptr_t gdm = mem::ptr(game::g.gameDataMan);
            st.deaths = gdm ? (int)mem::rdv<uint32_t>(gdm + 0x94) : -1;
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
            tick_jumpkey(t);
            tick_fallvel(t, tor, mounted, st);
            tick_jumpheight(tor, mounted);
            tick_fly(t, pl, tor, mounted, st);
            pump_delayed();
            st.esd = game::esd_ready();
            st.warp = game::g.luaWarp && game::g.luaEventMan;
            if (tick_flail(t, pl, st)) move_comp(pl, s_flailApplied, true);
            else {
                tick_cast(pl, st);
                move_comp(pl, std::clamp(g_cfg.castSpeed, 1.f, 10.f), s_castActive);
            }
            tick_weight(pl, st);

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
