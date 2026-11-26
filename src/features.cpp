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

static void act_map() {
    std::vector<uint32_t> flags = {62010, 62011, 62012, 62020, 62021, 62022, 62030, 62031, 62032, 62040,
                                   62041, 62050, 62051, 62052, 62060, 62061, 62062, 62063, 62064};
    if (need_params())
        for (uintptr_t t : game::param_tables("WORLD_MAP_PIECE_PARAM_ST"))
            for (int i = 0, n = game::param_rows(t); i < n; ++i) {
                int32_t id;
                uintptr_t d;
                if (!game::param_row(t, i, id, d)) continue;
                uint32_t f = mem::rdv<uint32_t>(d);
                if (f >= 62000 && f < 63000 && std::find(flags.begin(), flags.end(), f) == flags.end()) flags.push_back(f);
            }
    int ok = set_flags(flags);
    msg("Carte : " + std::to_string(ok) + "/" + std::to_string(flags.size()) + " fragments débloqués");
}

static void act_graces() {
    if (!need_params()) { msg("Grâces : params introuvables"); return; }
    std::vector<uint32_t> flags;
    for (uintptr_t t : game::param_tables("BONFIRE_WARP_PARAM_ST"))
        for (int i = 0, n = game::param_rows(t); i < n; ++i) {
            int32_t id;
            uintptr_t d;
            if (!game::param_row(t, i, id, d)) continue;
            uint32_t f = mem::rdv<uint32_t>(d + 4);
            if (f >= 70000 && f < 80000) flags.push_back(f);
        }
    if (flags.empty()) { msg("Grâces : aucune trouvée dans BonfireWarpParam"); return; }
    int ok = set_flags(flags);
    msg("Grâces : " + std::to_string(ok) + "/" + std::to_string(flags.size()) + " débloquées");
}

static void act_bells() {
    if (!need_params()) { msg("Cloches : params introuvables"); return; }
    auto isStone = [](int32_t g) { return (g >= 10100 && g <= 10107) || (g >= 10160 && g <= 10168); };
    std::vector<uint32_t> flags;
    for (int pass = 0; pass < 2 && flags.empty(); ++pass)
        for (uintptr_t t : game::param_tables("SHOP_LINEUP_PARAM"))
            for (int i = 0, n = game::param_rows(t); i < n; ++i) {
                int32_t id;
                uintptr_t d;
                if (!game::param_row(t, i, id, d)) continue;
                int32_t equip = mem::rdv<int32_t>(d);
                uint32_t rel = mem::rdv<uint32_t>(d + 0x10);
                int16_t qty = mem::rdv<int16_t>(d + 0x14);
                if (!isStone(equip) || !rel || rel == 0xFFFFFFFF) continue;
                if (pass == 0 && qty != -1) continue;
                if (std::find(flags.begin(), flags.end(), rel) == flags.end()) flags.push_back(rel);
            }
    if (flags.empty()) { msg("Cloches : aucune entrée de boutique trouvée"); return; }
    int ok = set_flags(flags);
    msg("Cloches de forge : " + std::to_string(ok) + "/" + std::to_string(flags.size()) + " débloquées (Sœurs jumelles)");
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

static void fall_reset();

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
            msg(game::flag_set(GREYOLL_FLAG, false) ? "Greyoll : flag remis à zéro, recharge la zone (grâce / téléportation)"
                                                    : "Greyoll : flag introuvable");
            break;
        case Act::UnlockMap: act_map(); break;
        case Act::UnlockGraces: act_graces(); break;
        case Act::UnlockBells: act_bells(); break;
        case Act::DupWeapon: act_dup(a.second); break;
        case Act::RefreshInv: refresh_weapons(); break;
        case Act::SaveCfg: cfg_save(); break;
        case Act::ResetFall: fall_reset(); break;
        }
    }
}

static double s_greyollOnSince = 0;

static void tick_boss(double t, Status& st) {
    st.greyoll = game::flag_get(GREYOLL_FLAG);
    if (!g_cfg.greyollRespawn || st.greyoll != 1) { s_greyollOnSince = 0; return; }
    if (s_greyollOnSince == 0) s_greyollOnSince = t;
    if (t - s_greyollOnSince >= g_cfg.respawnDelay) {
        game::flag_set(GREYOLL_FLAG, false);
        s_greyollOnSince = 0;
        msg("Greyoll vaincu : il réapparaîtra au prochain rechargement de la zone");
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

struct FField { int mi, off; };
struct FCand { int chr, mi, off, hits; };

static std::vector<FField> s_fh, s_ft;
static bool s_fieldsLoaded = false;
static std::vector<FCand> s_gc, s_tc;
static std::unordered_set<uint64_t> s_prevRaw, s_tracked;
static float s_prevGroundY = 0;
static bool s_havePrevGround = false;

static bool s_protect = false;
static uint8_t s_prevNoDead = 0;
static double s_lastFallT = 0, s_groundSince = 0, s_leaveT = 0, s_landedSince = 0;
static float s_lastY = 0, s_vy = 0, s_groundY = 0, s_apexY = 0;
static bool s_haveY = false, s_grounded = false, s_airborne = false;
static float s_chrGroundY[2], s_chrApexY[2];

static constexpr int kMods = 40;

static std::string fields_str(const std::vector<FField>& v) {
    std::string r;
    for (auto& f : v) r += (r.empty() ? "" : ",") + std::to_string(f.mi) + ":" + std::to_string(f.off);
    return r;
}

static std::vector<FField> fields_parse(const std::string& s) {
    std::vector<FField> r;
    size_t i = 0;
    while (i < s.size()) {
        size_t c = s.find(',', i);
        std::string tok = s.substr(i, c == std::string::npos ? std::string::npos : c - i);
        size_t k = tok.find(':');
        if (k != std::string::npos) r.push_back({atoi(tok.c_str()), atoi(tok.c_str() + k + 1)});
        if (c == std::string::npos) break;
        i = c + 1;
    }
    return r;
}

static void fields_save() {
    g_cfg.fallH = fields_str(s_fh);
    g_cfg.fallT = fields_str(s_ft);
    cfg_save();
}

static bool field_addr(uintptr_t chr, int mi, int off, uintptr_t& a) {
    size_t sz = 0;
    uintptr_t base = game::chr_region(chr, mi, sz);
    if (!base || off < 0 || (size_t)off + 4 > sz) return false;
    a = base + off;
    return true;
}

static void collect_ground(uintptr_t chrs[2], const float ys[2]) {
    std::vector<FCand> raw;
    s_tc.clear();
    static uint8_t buf[0x1000];
    for (int c = 0; c < 2; ++c) {
        if (!chrs[c]) continue;
        for (int mi = -1; mi < kMods; ++mi) {
            size_t sz = 0;
            uintptr_t base = game::chr_region(chrs[c], mi, sz);
            if (!base || !mem::read_raw(base, buf, sz)) continue;
            for (size_t off = 0; off + 4 <= sz; off += 4) {
                float f;
                uint32_t r;
                memcpy(&f, buf + off, 4);
                memcpy(&r, buf + off, 4);
                if (r == 0) s_tc.push_back({c, mi, (int)off, 0});
                else if (std::isfinite(f) && std::fabs(f - ys[c]) < 0.3f) raw.push_back({c, mi, (int)off, 0});
            }
        }
    }
    auto key = [](const FCand& c) { return ((uint64_t)(c.chr & 1) << 48) | ((uint64_t)(uint16_t)(c.mi + 1) << 32) | (uint32_t)c.off; };
    std::unordered_set<uint64_t> now;
    for (auto& g : raw) now.insert(key(g));
    for (auto it = s_tracked.begin(); it != s_tracked.end();) it = now.count(*it) ? std::next(it) : s_tracked.erase(it);
    if (s_havePrevGround && std::fabs(ys[0] - s_prevGroundY) > 1.0f)
        for (auto& g : raw)
            if (s_prevRaw.count(key(g))) s_tracked.insert(key(g));
    s_prevRaw = std::move(now);
    s_prevGroundY = ys[0];
    s_havePrevGround = true;
    s_gc.clear();
    for (auto& g : raw)
        if (s_tracked.count(key(g))) s_gc.push_back(g);
}

static bool has_field(const std::vector<FField>& v, int mi, int off) {
    for (auto& f : v)
        if (f.mi == mi && f.off == off) return true;
    return false;
}

static void learn_fall(double t, uintptr_t chrs[2], const float ys[2]) {
    double e = t - s_leaveT;
    bool deep = s_chrGroundY[0] - ys[0] > 2.0f;
    if (!deep) return;
    bool changed = false;
    for (auto it = s_gc.begin(); it != s_gc.end();) {
        uintptr_t a;
        float f;
        bool keep = false;
        if (it->hits >= 0 && chrs[it->chr] && field_addr(chrs[it->chr], it->mi, it->off, a) && mem::rd(a, f)) {
            float g = s_chrGroundY[it->chr], ap = s_chrApexY[it->chr];
            keep = std::fabs(f - g) < 0.5f || std::fabs(f - ap) < 0.5f;
        }
        if (!keep) { it = s_gc.erase(it); continue; }
        if (++it->hits >= 30 && s_fh.size() < 4 && !has_field(s_fh, it->mi, it->off)) {
            s_fh.push_back({it->mi, it->off});
            changed = true;
        }
        ++it;
    }
    if (e > 0.4) {
        for (auto it = s_tc.begin(); it != s_tc.end();) {
            uintptr_t a;
            float f;
            bool keep = false;
            if (chrs[it->chr] && field_addr(chrs[it->chr], it->mi, it->off, a) && mem::rd(a, f))
                keep = std::isfinite(f) && f > 0.2f && std::fabs(f - (float)e) < 0.35f + 0.15f * (float)e;
            if (!keep) { it = s_tc.erase(it); continue; }
            if (++it->hits >= 30 && s_ft.size() < 4 && !has_field(s_ft, it->mi, it->off)) {
                s_ft.push_back({it->mi, it->off});
                changed = true;
            }
            ++it;
        }
    }
    if (changed) {
        fields_save();
        elog("Chute calibree : hauteur [%s] minuteur [%s]", g_cfg.fallH.c_str(), g_cfg.fallT.c_str());
        msg("D\xC3\xA9g\xC3\xA2ts de chute : calibrage enregistr\xC3\xA9");
    }
}

static void apply_fields(uintptr_t chrs[2], const float ys[2]) {
    for (int c = 0; c < 2; ++c) {
        if (!chrs[c]) continue;
        uintptr_t a;
        for (auto& f : s_fh)
            if (field_addr(chrs[c], f.mi, f.off, a)) mem::wr(a, ys[c]);
        for (auto& f : s_ft)
            if (field_addr(chrs[c], f.mi, f.off, a)) mem::wr(a, 0.f);
    }
}

static void fall_reset() {
    s_fh.clear();
    s_ft.clear();
    s_gc.clear();
    s_tc.clear();
    s_havePrevGround = false;
    s_prevRaw.clear();
    s_tracked.clear();
    fields_save();
    msg("Calibrage des chutes r\xC3\xA9initialis\xC3\xA9");
}

static void protect_end() {
    if (!s_protect) return;
    if (uintptr_t nd = game::nodead_addr()) mem::wr(nd, s_prevNoDead);
    s_protect = false;
}

static void tick_fall(double t, uintptr_t pl, uintptr_t tor, bool mounted, Status& st) {
    if (!s_fieldsLoaded) {
        s_fh = fields_parse(g_cfg.fallH);
        s_ft = fields_parse(g_cfg.fallT);
        s_fieldsLoaded = true;
    }
    st.fallH = (int)s_fh.size();
    st.fallT = (int)s_ft.size();
    if (!g_cfg.noFallDamage || !pl) {
        protect_end();
        s_haveY = s_grounded = s_airborne = false;
        st.fallProtect = false;
        return;
    }
    uintptr_t chrs[2] = {pl, mounted ? tor : 0};
    float ys[2] = {0, 0}, p[3];
    if (!game::chr_pos(pl, p)) { protect_end(); s_haveY = false; return; }
    ys[0] = p[1];
    if (chrs[1]) {
        float tp[3];
        if (game::chr_pos(chrs[1], tp)) ys[1] = tp[1];
        else chrs[1] = 0;
    }

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

    bool still = std::fabs(s_vy) < 0.3f;
    if (still && !s_airborne) {
        if (s_groundSince == 0) s_groundSince = t;
        if (!s_grounded && t - s_groundSince > 0.5) {
            s_grounded = true;
            s_groundY = p[1];
            for (int c = 0; c < 2; ++c) s_chrGroundY[c] = s_chrApexY[c] = ys[c];
            collect_ground(chrs, ys);
        }
    } else {
        s_groundSince = 0;
    }

    if (s_grounded && !s_airborne && (s_vy < -4.5f || p[1] - s_groundY > 0.6f)) {
        s_airborne = true;
        s_grounded = false;
        s_leaveT = s_vy < -4.5f ? t + s_vy / 9.81f : t;
        s_landedSince = 0;
    }

    if (s_airborne) {
        for (int c = 0; c < 2; ++c)
            if (chrs[c] && ys[c] > s_chrApexY[c]) s_chrApexY[c] = ys[c];
        learn_fall(t, chrs, ys);
        if (s_vy < -1.0f) apply_fields(chrs, ys);
        if (s_vy < -g_cfg.fallSpeed && !s_protect) {
            if (uintptr_t nd = game::nodead_addr()) {
                s_prevNoDead = mem::rdv<uint8_t>(nd);
                mem::wr(nd, (uint8_t)1);
                s_protect = true;
            }
        }
        if (still) {
            if (s_landedSince == 0) s_landedSince = t;
            if (t - s_landedSince > 0.6) {
                s_airborne = false;
                protect_end();
            }
        } else {
            s_landedSince = 0;
        }
    } else if (!s_grounded && s_vy < -1.0f) {
        apply_fields(chrs, ys);
    }
    st.fallProtect = s_airborne;
}

static bool s_tHave = false;
static float s_tLast[3];
static double s_tLastT = 0;
static float s_tSpeed = 0;

static void tick_torrent(double t, uintptr_t tor, bool mounted, Status& st) {
    st.torrentMult = 1.f;
    if (!g_cfg.torrentSpeed || !tor || !mounted) { s_tHave = false; s_tSpeed = 0; st.torrentBase = 0; return; }
    float p[3];
    if (!game::chr_pos(tor, p)) { s_tHave = false; return; }
    double dt = t - s_tLastT;
    if (!s_tHave || dt > 0.5) {
        memcpy(s_tLast, p, 12);
        s_tLastT = t;
        s_tHave = true;
        return;
    }
    float dx = p[0] - s_tLast[0], dz = p[2] - s_tLast[2];
    float d = std::sqrt(dx * dx + dz * dz);
    if (d > 8.f) { memcpy(s_tLast, p, 12); s_tLastT = t; return; }
    if (d < 1e-4f) {
        if (dt > 0.1) s_tSpeed *= 0.8f;
        st.torrentBase = s_tSpeed;
        return;
    }
    float v = d / (float)dt;
    s_tSpeed = s_tSpeed * 0.7f + v * 0.3f;
    st.torrentBase = s_tSpeed;
    float mult = s_tSpeed >= g_cfg.gallopThreshold ? g_cfg.torrentRun : g_cfg.torrentWalk;
    mult = std::clamp(mult, 1.f, 3.f);
    st.torrentMult = mult;
    float nx = p[0] + dx * (mult - 1.f), nz = p[2] + dz * (mult - 1.f);
    if (mult > 1.001f) game::chr_set_xz(tor, nx, nz);
    s_tLast[0] = nx;
    s_tLast[1] = p[1];
    s_tLast[2] = nz;
    s_tLastT = t;
}

static DWORD WINAPI scan_thread(void*) {
    ULONGLONG lastDeep = 0, lastNames = 0;
    for (;;) {
        Sleep(400);
        uintptr_t pl = s_player.load();
        ULONGLONG tk = GetTickCount64();
        if (pl && game::params_ready() && !game::names_ready() && (!lastNames || tk - lastNames > 30000)) {
            lastNames = tk;
            game::names_load();
        }
        if (!pl || !(g_cfg.torrentSpeed || g_cfg.noFallDamage)) { s_torrent = 0; continue; }
        uintptr_t tor = s_torrent.load();
        float pp[3], tp[3];
        if (tor && game::is_chr(tor) && game::chr_pos(pl, pp) && game::chr_pos(tor, tp) && game::mount_geom(pp, tp)) continue;
        ULONGLONG n = GetTickCount64();
        bool deep = n - lastDeep > 4000;
        if (deep) lastDeep = n;
        uintptr_t found = game::find_mount(pl, deep);
        if (found != tor) {
            if (found) elog("Torrent trouve @ %p", (void*)found);
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
            st.noDead = game::nodead_addr() != 0;
            process_actions();
            tick_boss(t, st);
            st.flags = st.greyoll >= 0;
            uintptr_t pl = game::player();
            s_player = pl;
            st.player = pl != 0;
            st.playerOff = game::player_offset();
            uintptr_t tor = s_torrent.load();
            bool mounted = false;
            if (pl && tor && game::is_chr(tor)) {
                float pp[3], tp[3];
                mounted = game::chr_pos(pl, pp) && game::chr_pos(tor, tp) && game::mount_geom(pp, tp);
            }
            st.torrentFound = tor != 0;
            st.mounted = mounted;
            tick_runes(t, pl, st);
            tick_fall(t, pl, tor, mounted, st);
            tick_torrent(t, tor, mounted, st);
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
