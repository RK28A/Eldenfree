#include "game.h"
#include "mem.h"
#include "config.h"
#include <windows.h>
#include <cmath>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

using namespace mem;

namespace game {

Addrs g;

static uintptr_t scan_rip(const char* name, const char* pat, int dispOff, int len) {
    uintptr_t a = aob(pat);
    uintptr_t r = a ? rip(a, dispOff, len) : 0;
    elog("AOB %-14s %s -> %p", name, a ? "ok" : "INTROUVABLE", (void*)r);
    return r;
}

static uintptr_t fn_start_check(uintptr_t fn) {
    uint8_t prev = 0;
    if (!fn || (fn & 0xF) || !rd(fn - 1, prev)) return 0;
    return (prev == 0xCC || prev == 0xC3 || prev == 0x90) ? fn : 0;
}

bool resolve() {
    if (!g.gameDataMan) g.gameDataMan = scan_rip("GameDataMan", "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 05 48 8B 40 58 C3 C3", 3, 7);
    if (!g.worldChrMan) g.worldChrMan = scan_rip("WorldChrMan", "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 0F 48 39 88", 3, 7);
    if (!g.eventFlagMan) g.eventFlagMan = scan_rip("EventFlagMan", "48 8B 3D ?? ?? ?? ?? 48 85 FF ?? ?? 32 C0 E9", 3, 7);
    if (!g.soloParam) g.soloParam = scan_rip("SoloParamRepo", "48 8B 0D ?? ?? ?? ?? 48 85 C9 0F 84 ?? ?? ?? ?? 45 33 C0 BA 90", 3, 7);
    if (!g.chrDbg) g.chrDbg = scan_rip("ChrDbgFlags", "80 3D ?? ?? ?? ?? 00 0F 85 ?? ?? ?? ?? 32 C0 48", 2, 7);
    if (!g.mapItemMan) g.mapItemMan = scan_rip("MapItemMan", "48 8B 0D ?? ?? ?? ?? C7 44 24 50 FF FF FF FF C7 45 A0 FF FF FF FF C7 45 A4", 3, 7);
    if (!g.msgRepo) g.msgRepo = scan_rip("MsgRepository", "48 8B 3D ?? ?? ?? ?? 44 0F B6 30 48 85 FF 75", 3, 7);
    if (!g.itemGib) {
        uintptr_t a = aob("8B 02 83 F8 0A");
        if (a) {
            uintptr_t fn = fn_start_check(a + g_cfg.itemGibDelta);
            for (int k = 1; !fn && k < 0x300; ++k) fn = fn_start_check(a - k);
            g.itemGib = fn;
        }
        elog("AOB %-14s %s -> %p", "ItemGib", g.itemGib ? "ok" : "INTROUVABLE", (void*)g.itemGib);
    }
    return g.gameDataMan && g.worldChrMan && g.eventFlagMan;
}

static uintptr_t flag_loc(uint32_t id, uint8_t& mask) {
    uintptr_t man = ptr(g.eventFlagMan);
    if (!man) return 0;
    uint32_t div = rdv<uint32_t>(man + 0x1C);
    if (!div || div > 1000000) return 0;
    uint32_t cat = id / div, rem = id % div;
    uintptr_t head = ptr(man + 0x38);
    if (!head) return 0;
    uintptr_t res = head, node = ptr(head + 0x8);
    for (int it = 0; node && it < 256; ++it) {
        if (rdv<uint8_t>(node + 0x19, 1)) break;
        if (rdv<uint32_t>(node + 0x20) < cat) node = ptr(node + 0x10);
        else { res = node; node = ptr(node); }
    }
    if (res == head || cat < rdv<uint32_t>(res + 0x20)) return 0;
    int type = rdv<int32_t>(res + 0x28);
    uintptr_t block = 0;
    if (type == 1) {
        uintptr_t base = rdv<uintptr_t>(man + 0x28);
        if (base < 0x10000) return 0;
        block = (uintptr_t)rdv<uint32_t>(man + 0x20) * rdv<uint32_t>(res + 0x30) + base;
    } else if (type == 2) {
        return 0;
    } else {
        block = rdv<uintptr_t>(res + 0x30);
    }
    if (block < 0x10000) return 0;
    mask = (uint8_t)(1u << (7 - rem % 8));
    return block + rem / 8;
}

int flag_get(uint32_t id) {
    uint8_t m = 0, b = 0;
    uintptr_t a = flag_loc(id, m);
    if (!a || !rd(a, b)) return -1;
    return (b & m) ? 1 : 0;
}

bool flag_set(uint32_t id, bool on) {
    uint8_t m = 0, b = 0;
    uintptr_t a = flag_loc(id, m);
    if (!a || !rd(a, b)) return false;
    uint8_t nb = on ? (b | m) : (b & ~m);
    return nb == b || wr(a, nb);
}

struct PInfo { uintptr_t tbl; std::string type; };
static std::vector<PInfo> s_params;
static std::mutex s_pm;

static bool table_type(uintptr_t t, std::string& type) {
    if (!plausible(t)) return false;
    uint16_t rows = 0;
    if (!rd(t + 0xA, rows) || !rows || rows > 60000) return false;
    auto ok = [](const char* s) {
        int n = 0;
        for (; n < 63 && s[n]; ++n) {
            char c = s[n];
            if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
        }
        return n >= 4 && n < 63;
    };
    char buf[64] = {};
    int64_t toff = 0;
    bool found = false;
    if (rd(t + 0x10, toff) && toff >= 0x40 && toff < 0x8000000) {
        if (read_raw(t + toff, buf, 64) || read_raw(t + toff, buf, 32)) {
            buf[63] = 0;
            if (ok(buf)) found = true;
        }
    }
    if (!found) {
        memset(buf, 0, sizeof(buf));
        if (read_raw(t + 0xC, buf, 0x20) && ok(buf)) found = true;
    }
    if (!found) return false;
    int64_t d0 = 0;
    if (!rd(t + 0x48, d0) || d0 < 0x40 || d0 > 0x10000000) return false;
    type = buf;
    return true;
}

static void add_table(uintptr_t t, const std::string& type) {
    for (auto& p : s_params)
        if (p.tbl == t) return;
    s_params.push_back({t, type});
}

static bool try_cap(uintptr_t cap, bool deep) {
    std::string type;
    uintptr_t a = ptr(cap + 0x80);
    if (a) {
        uintptr_t t = ptr(a + 0x80);
        if (t && table_type(t, type)) { add_table(t, type); return true; }
    }
    if (!deep) return false;
    uint64_t l1[32];
    if (!read_raw(cap, l1, sizeof(l1))) return false;
    for (uint64_t p1 : l1) {
        if (!plausible(p1)) continue;
        if (table_type(p1, type)) { add_table(p1, type); return true; }
        uint64_t l2[32];
        if (!read_raw(p1, l2, sizeof(l2))) continue;
        for (uint64_t p2 : l2)
            if (plausible(p2) && table_type(p2, type)) { add_table(p2, type); return true; }
    }
    return false;
}

bool params_load() {
    std::lock_guard<std::mutex> lk(s_pm);
    if (s_params.size() >= 20) return true;
    uintptr_t repo = ptr(g.soloParam);
    if (!repo) return false;
    s_params.clear();
    for (int i = 0; i < 400; ++i) {
        uintptr_t cap = ptr(repo + 0x88 + i * 0x48);
        if (cap) try_cap(cap, false);
    }
    if (s_params.size() < 20) {
        for (uintptr_t off = 0; off < 0x10000; off += 8) {
            uintptr_t cap = ptr(repo + off);
            if (cap) try_cap(cap, true);
        }
    }
    elog("Params: %zu tables", s_params.size());
    return s_params.size() >= 20;
}

bool params_ready() {
    std::lock_guard<std::mutex> lk(s_pm);
    return s_params.size() >= 20;
}

size_t params_count() {
    std::lock_guard<std::mutex> lk(s_pm);
    return s_params.size();
}

std::vector<uintptr_t> param_tables(const char* type) {
    std::lock_guard<std::mutex> lk(s_pm);
    std::vector<uintptr_t> r;
    for (auto& p : s_params)
        if (p.type == type) r.push_back(p.tbl);
    return r;
}

int param_rows(uintptr_t t) { return rdv<uint16_t>(t + 0xA); }

bool param_row(uintptr_t t, int i, int32_t& id, uintptr_t& data) {
    uintptr_t e = t + 0x40 + (uintptr_t)i * 0x18;
    int64_t off = 0;
    if (!rd(e, id) || !rd(e + 8, off) || off < 0x40) return false;
    data = t + off;
    return true;
}

uintptr_t param_find(uintptr_t t, int32_t id) {
    int lo = 0, hi = param_rows(t) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int32_t rid;
        uintptr_t d;
        if (!param_row(t, mid, rid, d)) return 0;
        if (rid == id) return d;
        if (rid < id) lo = mid + 1;
        else hi = mid - 1;
    }
    return 0;
}

uintptr_t pgd() {
    uintptr_t gdm = ptr(g.gameDataMan);
    return gdm ? ptr(gdm + g_cfg.offPgd) : 0;
}

static bool s_ownerCheck = false;

static bool chr_check(uintptr_t p, bool owner) {
    if (!plausible(p)) return false;
    uint64_t vt = 0;
    if (!rd(p, vt) || !in_image(vt)) return false;
    uintptr_t mods = ptr(p + g_cfg.offModules);
    if (!mods) return false;
    uintptr_t data = ptr(mods + g_cfg.offDataMod), phys = ptr(mods + g_cfg.offPhysMod);
    if (!data || !phys) return false;
    if (!in_image(rdv<uint64_t>(data)) || !in_image(rdv<uint64_t>(phys))) return false;
    if (owner && rdv<uint64_t>(data + 8) != p) return false;
    int32_t hp = -1, mhp = -1;
    if (!rd(data + g_cfg.offHp, hp) || !rd(data + g_cfg.offMaxHp, mhp)) return false;
    if (mhp <= 0 || mhp > 5000000 || hp < 0 || hp > mhp * 2) return false;
    float pos[3];
    if (!read_raw(phys + g_cfg.offPos, pos, 12)) return false;
    for (float f : pos)
        if (!std::isfinite(f) || std::fabs(f) > 100000.f) return false;
    return true;
}

bool is_chr(uintptr_t p) { return chr_check(p, s_ownerCheck); }

static int s_playerOff = -1;
static ULONGLONG s_lastPlayerScan = 0;

int player_offset() { return g_cfg.offPlayerIns ? g_cfg.offPlayerIns : s_playerOff; }

uintptr_t player() {
    uintptr_t wcm = ptr(g.worldChrMan);
    if (!wcm) return 0;
    if (g_cfg.offPlayerIns) {
        uintptr_t p = ptr(wcm + g_cfg.offPlayerIns);
        return chr_check(p, false) ? p : 0;
    }
    if (s_playerOff >= 0) {
        uintptr_t p = ptr(wcm + s_playerOff);
        return chr_check(p, false) ? p : 0;
    }
    ULONGLONG now = GetTickCount64();
    if (now - s_lastPlayerScan < 3000) return 0;
    s_lastPlayerScan = now;
    uintptr_t P = pgd();
    int32_t php = P ? rdv<int32_t>(P + g_cfg.offHpPgd, -1) : -1;
    int32_t pmhp = P ? rdv<int32_t>(P + g_cfg.offHpPgd + 4, -1) : -1;
    auto match = [&](uintptr_t p) {
        if (!chr_check(p, false)) return false;
        int hp, mhp;
        return php > 0 && chr_hp(p, hp, mhp) && hp == php && mhp == pmhp;
    };
    const int known[] = {0x1E508, 0x18468, 0x1E1A8, 0x10EF8};
    for (int off : known) {
        uintptr_t p = ptr(wcm + off);
        if (match(p)) { s_playerOff = off; break; }
    }
    if (s_playerOff < 0 && php > 0) {
        static uint64_t buf[0x1000];
        for (int base = 0; base < 0x40000 && s_playerOff < 0; base += sizeof(buf)) {
            if (!read_raw(wcm + base, buf, sizeof(buf))) break;
            for (int i = 0; i < 0x1000; ++i)
                if (plausible(buf[i]) && match(buf[i])) { s_playerOff = base + i * 8; break; }
        }
    }
    if (s_playerOff < 0) {
        for (int off : known) {
            uintptr_t p = ptr(wcm + off);
            if (chr_check(p, true)) { s_playerOff = off; break; }
        }
    }
    if (s_playerOff < 0) return 0;
    uintptr_t p = ptr(wcm + s_playerOff);
    s_ownerCheck = chr_check(p, true);
    elog("PlayerIns @ WorldChrMan+0x%X (owner check %d)", s_playerOff, (int)s_ownerCheck);
    return chr_check(p, false) ? p : 0;
}

static uintptr_t data_mod(uintptr_t chr) { return ptr(ptr(chr + g_cfg.offModules) + g_cfg.offDataMod); }
static uintptr_t phys_mod(uintptr_t chr) { return ptr(ptr(chr + g_cfg.offModules) + g_cfg.offPhysMod); }

bool chr_hp(uintptr_t chr, int& hp, int& maxhp) {
    uintptr_t d = data_mod(chr);
    return d && rd(d + g_cfg.offHp, hp) && rd(d + g_cfg.offMaxHp, maxhp);
}

bool chr_set_hp(uintptr_t chr, int hp) {
    uintptr_t d = data_mod(chr);
    return d && wr(d + g_cfg.offHp, (int32_t)hp);
}

bool chr_pos(uintptr_t chr, float p[3]) {
    uintptr_t ph = phys_mod(chr);
    return ph && read_raw(ph + g_cfg.offPos, p, 12);
}

bool chr_set_xz(uintptr_t chr, float x, float z) {
    uintptr_t ph = phys_mod(chr);
    return ph && wr(ph + g_cfg.offPos, x) && wr(ph + g_cfg.offPos + 8, z);
}

bool mount_geom(const float pp[3], const float tp[3]) {
    float dx = pp[0] - tp[0], dz = pp[2] - tp[2], dy = pp[1] - tp[1];
    float h = std::sqrt(dx * dx + dz * dz);
    if (h < 0.15f && std::fabs(dy) < 0.15f) return true;
    return h < 0.8f && dy > 0.3f && dy < 3.0f;
}

static int s_mountWcmOff = -1;

uintptr_t find_mount(uintptr_t pl, bool deep) {
    float pp[3];
    if (!pl || !chr_pos(pl, pp)) return 0;
    auto test = [&](uint64_t q) -> bool {
        if (q == pl || !plausible(q)) return false;
        uint64_t vt;
        if (!rd(q, vt) || !in_image(vt)) return false;
        float tp[3];
        return is_chr(q) && chr_pos(q, tp) && mount_geom(pp, tp);
    };
    auto sub = [&](uint64_t base, size_t qwords) -> uint64_t {
        static thread_local uint64_t b[0x200];
        if (qwords > 0x200) qwords = 0x200;
        if (!read_raw(base, b, qwords * 8)) return 0;
        for (size_t i = 0; i < qwords; ++i)
            if (test(b[i])) return b[i];
        return 0;
    };
    uintptr_t wcm = ptr(g.worldChrMan);
    if (wcm && s_mountWcmOff >= 0) {
        uint64_t q = ptr(wcm + s_mountWcmOff);
        if (test(q)) return q;
        if (q) if (uint64_t r = sub(q, 0x200)) return r;
    }
    uintptr_t mods = ptr(pl + g_cfg.offModules);
    if (mods) {
        uint64_t m[64];
        if (read_raw(mods, m, sizeof(m))) {
            for (uint64_t mod : m) {
                if (!plausible(mod)) continue;
                uint64_t b[0x80];
                if (!read_raw(mod, b, sizeof(b))) continue;
                for (uint64_t q : b) {
                    if (test(q)) return q;
                    if (plausible(q))
                        if (uint64_t r = sub(q, 0x10)) return r;
                }
            }
        }
    }
    if (!deep || !wcm) return 0;
    static uint64_t buf[0x1000];
    for (int base = 0; base < 0x40000; base += sizeof(buf)) {
        if (!read_raw(wcm + base, buf, sizeof(buf))) break;
        for (int i = 0; i < 0x1000; ++i) {
            uint64_t q = buf[i];
            if (!plausible(q) || q == pl) continue;
            if (test(q)) { s_mountWcmOff = base + i * 8; return q; }
            if (uint64_t r = sub(q, 0x200)) { s_mountWcmOff = base + i * 8; return r; }
        }
    }
    return 0;
}

uintptr_t nodead_addr() { return g.chrDbg ? g.chrDbg + g_cfg.offNoDead : 0; }

bool give_item(uint32_t id, int qty) {
    if (!g.itemGib || !g.mapItemMan) return false;
    uintptr_t mim = ptr(g.mapItemMan);
    if (!mim) return false;
    alignas(16) int32_t data[16] = {1, (int32_t)id, qty, -1, -1};
    alignas(16) uint8_t out[0x200] = {};
    using Fn = void (*)(uintptr_t, void*, void*);
    ((Fn)g.itemGib)(mim, data, out);
    return true;
}

static bool cat_match(uint32_t handle, uint32_t id) {
    uint32_t hc = handle >> 28, ic = id >> 28;
    switch (hc) {
    case 0x8: return ic == 0x0;
    case 0x9: return ic == 0x1;
    case 0xA: return ic == 0x2;
    case 0xB: return ic == 0x4;
    case 0xC: return ic == 0x8;
    default: return false;
    }
}

struct InvCand { uintptr_t arr = 0; int stride = 0; };
static InvCand s_inv;

static void score_array(uintptr_t a, int stride, int& valid, int& junk, std::vector<uint32_t>* wh) {
    valid = junk = 0;
    uint8_t b[0x20 * 128];
    if (!read_raw(a, b, (size_t)stride * 128)) return;
    for (int i = 0; i < 128; ++i) {
        uint32_t h, id, q;
        memcpy(&h, b + i * stride, 4);
        memcpy(&id, b + i * stride + 4, 4);
        memcpy(&q, b + i * stride + 8, 4);
        if (h == 0 || h == 0xFFFFFFFF) continue;
        if (cat_match(h, id) && q >= 1 && q <= 9999) {
            ++valid;
            if (wh && (h >> 28) == 0x8) wh->push_back(h);
        } else {
            ++junk;
        }
    }
}

static bool inv_find() {
    uintptr_t P = pgd();
    if (!P) return false;
    static uint8_t pg[0x2000];
    memset(pg, 0, sizeof(pg));
    if (!read_raw(P, pg, sizeof(pg)) && !read_raw(P, pg, 0x1000)) return false;
    std::unordered_set<uint32_t> pgdDwords;
    for (size_t i = 0; i + 4 <= sizeof(pg); i += 4) {
        uint32_t v;
        memcpy(&v, pg + i, 4);
        if ((v >> 28) == 0x8) pgdDwords.insert(v);
    }
    const int strides[] = {0x18, 0x14, 0x10, 0x1C, 0x20};
    InvCand best;
    long bestScore = 0;
    std::unordered_set<uintptr_t> seen;
    auto consider = [&](uintptr_t a) {
        if (!plausible(a) || !seen.insert(a).second) return;
        uint32_t pre[64];
        if (!read_raw(a, pre, sizeof(pre))) return;
        bool any = false;
        for (int k = 0; k + 2 < 64 && !any; ++k)
            any = pre[k] && pre[k] != 0xFFFFFFFF && cat_match(pre[k], pre[k + 1]) && pre[k + 2] >= 1 && pre[k + 2] <= 9999;
        if (!any) return;
        for (int s : strides) {
            int v, j;
            std::vector<uint32_t> wh;
            score_array(a, s, v, j, &wh);
            if (v < 5 || j * 10 > v) continue;
            long eq = 0;
            for (uint32_t h : wh)
                if (pgdDwords.count(h)) ++eq;
            long sc = v + eq * 100;
            if (sc > bestScore) { bestScore = sc; best = {a, s}; }
        }
    };
    for (size_t i = 0; i + 8 <= sizeof(pg); i += 8) {
        uint64_t p1;
        memcpy(&p1, pg + i, 8);
        if (!plausible(p1)) continue;
        consider(p1);
        uint64_t l2[0x40];
        if (!read_raw(p1, l2, sizeof(l2))) continue;
        for (uint64_t p2 : l2) consider(p2);
    }
    if (!best.arr) return false;
    s_inv = best;
    elog("Inventaire @ %p stride 0x%X score %ld", (void*)best.arr, best.stride, bestScore);
    return true;
}

bool inventory(std::vector<InvItem>& out) {
    out.clear();
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!s_inv.arr && !inv_find()) return false;
        int v, j;
        score_array(s_inv.arr, s_inv.stride, v, j, nullptr);
        if (v >= 3 && j * 10 <= v) break;
        s_inv = {};
        if (attempt == 1) return false;
    }
    int junk = 0;
    for (int i = 0; i < 3000; ++i) {
        uint32_t e[3];
        if (!read_raw(s_inv.arr + (uintptr_t)i * s_inv.stride, e, 12)) break;
        if (e[0] == 0 || e[0] == 0xFFFFFFFF) continue;
        if (cat_match(e[0], e[1]) && e[2] >= 1 && e[2] <= 9999) out.push_back({e[0], e[1], e[2]});
        else if (++junk > 16) break;
    }
    return true;
}

static std::vector<uintptr_t> s_weaponFmgs;
static std::unordered_map<uint32_t, std::string> s_names;
static std::mutex s_nm;
static volatile bool s_namesReady = false;

static bool fmg_header(uintptr_t p) {
    uint32_t h[10];
    if (!plausible(p) || !read_raw(p, h, sizeof(h))) return false;
    if (h[0] != 0x00020000 || h[2] != 1 || h[5] != 0xFF) return false;
    if (h[8] || h[9]) return false;
    uint32_t size = h[1], groups = h[3], strings = h[4];
    uint64_t soff = (uint64_t)h[6] | ((uint64_t)h[7] << 32);
    if (!groups || groups > 100000 || strings > 400000 || size > 0x4000000) return false;
    return soff >= 0x28 && soff < size;
}

static bool fmg_get(uintptr_t f, uint32_t id, std::wstring& out) {
    int32_t groups = rdv<int32_t>(f + 0xC);
    int64_t soff = rdv<int64_t>(f + 0x18);
    int lo = 0, hi = groups - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int32_t gr[4];
        if (!read_raw(f + 0x28 + (uintptr_t)mid * 16, gr, 16)) return false;
        if ((int32_t)id < gr[1]) hi = mid - 1;
        else if ((int32_t)id > gr[2]) lo = mid + 1;
        else {
            int64_t so = rdv<int64_t>(f + soff + (uintptr_t)(gr[0] + ((int32_t)id - gr[1])) * 8);
            if (so <= 0) return false;
            wchar_t buf[96] = {};
            if (!read_raw(f + so, buf, sizeof(buf)) && !read_raw(f + so, buf, 32)) return false;
            buf[95] = 0;
            out.assign(buf);
            return !out.empty();
        }
    }
    return false;
}

template <class F> static void fmg_ids(uintptr_t f, F&& fn) {
    int32_t groups = rdv<int32_t>(f + 0xC);
    for (int i = 0; i < groups; ++i) {
        int32_t gr[4];
        if (!read_raw(f + 0x28 + (uintptr_t)i * 16, gr, 16)) return;
        for (int32_t id = gr[1]; id <= gr[2] && id - gr[1] < 100000; ++id) fn((uint32_t)id);
    }
}

void names_load() {
    if (s_namesReady) return;
    uintptr_t repo = ptr(g.msgRepo);
    if (!repo) return;
    std::vector<uintptr_t> fmgs;
    std::unordered_set<uintptr_t> visited;
    std::vector<std::pair<uintptr_t, int>> stack{{repo, 0}};
    while (!stack.empty() && visited.size() < 400000) {
        auto [n, d] = stack.back();
        stack.pop_back();
        if (!visited.insert(n).second) continue;
        uint64_t b[0x80];
        size_t cnt = d == 0 ? 0x80 : 0x40;
        if (!read_raw(n, b, cnt * 8)) continue;
        for (size_t i = 0; i < cnt; ++i) {
            uint64_t q = b[i];
            if (!plausible(q) || in_image(q) || visited.count(q)) continue;
            if (fmg_header(q)) {
                if (std::find(fmgs.begin(), fmgs.end(), q) == fmgs.end()) fmgs.push_back(q);
                continue;
            }
            if (d < 4) stack.push_back({q, d + 1});
        }
    }
    auto weps = param_tables("EQUIP_PARAM_WEAPON_ST");
    std::vector<uintptr_t> sel;
    for (uintptr_t f : fmgs) {
        int total = 0, hits = 0, longs = 0, sampled = 0;
        fmg_ids(f, [&](uint32_t id) {
            ++total;
            if (!weps.empty() && param_find(weps[0], (int32_t)id)) ++hits;
            if (sampled < 40) {
                std::wstring s;
                if (fmg_get(f, id, s)) {
                    ++sampled;
                    if (s.size() > 48 || s.find(L'\n') != std::wstring::npos) ++longs;
                }
            }
        });
        bool nameLike = sampled > 0 && longs * 5 < sampled;
        bool weaponIds = weps.empty() ? false : (hits >= 10 && hits * 10 >= total * 6);
        if (weps.empty()) {
            std::wstring s;
            weaponIds = fmg_get(f, 1000000, s) && fmg_get(f, 2000000, s);
        }
        if (nameLike && weaponIds) sel.push_back(f);
    }
    std::lock_guard<std::mutex> lk(s_nm);
    s_weaponFmgs = sel;
    elog("FMG: %zu trouvés, %zu noms d'armes", fmgs.size(), sel.size());
    if (!sel.empty()) s_namesReady = true;
}

bool names_ready() { return s_namesReady; }

static std::string utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::string weapon_name(uint32_t id) {
    std::lock_guard<std::mutex> lk(s_nm);
    auto it = s_names.find(id);
    if (it != s_names.end()) return it->second;
    static const char* inf[] = {"", "Lourde ", "Aiguisée ", "Précise ", "Ardente ", "Flamme sacrée ", "Foudre ",
                                "Sacrée ", "Magique ", "Glacée ", "Poison ", "Sang ", "Occulte "};
    uint32_t lvl = id % 100, infusion = (id % 10000) / 100, base = id - id % 10000;
    std::wstring w;
    std::string name;
    for (uintptr_t f : s_weaponFmgs)
        if (fmg_get(f, id - lvl, w)) { name = utf8(w); break; }
    if (name.empty())
        for (uintptr_t f : s_weaponFmgs)
            if (fmg_get(f, base, w)) { name = (infusion < 13 ? inf[infusion] : "") + utf8(w); break; }
    if (name.empty()) name = "Arme #" + std::to_string(id - lvl);
    if (lvl) name += " +" + std::to_string(lvl);
    if (s_namesReady) s_names[id] = name;
    return name;
}

}
