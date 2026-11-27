#include "game.h"
#include "mem.h"
#include "config.h"
#include <windows.h>
#include <cmath>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <cwctype>
#include "MinHook.h"

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
    if (!g.eventFlagMan) g.eventFlagMan = scan_rip("EventFlagMan2", "48 8B 3D ?? ?? ?? ?? 48 85 FF 74 ?? 48 8B 49", 3, 7);
    if (!g.soloParam) g.soloParam = scan_rip("SoloParamRepo", "48 8B 0D ?? ?? ?? ?? 48 85 C9 0F 84 ?? ?? ?? ?? 45 33 C0 BA 90", 3, 7);
    if (!g.mapItemMan) g.mapItemMan = scan_rip("MapItemMan", "48 8B 0D ?? ?? ?? ?? C7 44 24 50 FF FF FF FF", 3, 7);
    if (!g.msgRepo) g.msgRepo = scan_rip("MsgRepository", "48 8B 3D ?? ?? ?? ?? 44 0F B6 30 48 85 FF 75", 3, 7);
    if (!g.luaEventMan) g.luaEventMan = scan_rip("CSLuaEventMan", "48 83 3D ?? ?? ?? ?? 00 48 8B F9 0F 84 ?? ?? ?? ?? 48", 3, 8);
    if (!g.luaWarp) {
        uintptr_t a = aob("C3 ?? ?? ?? ?? ?? ?? 57 48 83 EC ?? 48 8B FA 44");
        g.luaWarp = a ? a + 2 : 0;
        elog("AOB %-14s %s -> %p", "LuaWarp", g.luaWarp ? "ok" : "INTROUVABLE", (void*)g.luaWarp);
    }
    if (!g.ezDispatch) {
        uintptr_t a = aob("48 8B DA 4C 8B F1 33 FF 89 7D");
        g.ezDispatch = a ? a - 0x47 : 0;
        elog("AOB %-14s %s -> %p", "EzStateDispatch", g.ezDispatch ? "ok" : "INTROUVABLE", (void*)g.ezDispatch);
    }
    if (!g.itemGib) {
        uintptr_t a = aob("8B 02 83 F8 0A");
        if (a) g.itemGib = a + g_cfg.itemGibDelta;
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
    int mode = rdv<int32_t>(res + 0x28);
    uintptr_t block = 0;
    if (mode == 1) {
        uintptr_t base = rdv<uintptr_t>(man + 0x28);
        if (base < 0x10000) return 0;
        block = base + (uintptr_t)rdv<uint32_t>(man + 0x20) * rdv<uint32_t>(res + 0x30);
    } else if (mode == 2) {
        block = rdv<uintptr_t>(res + 0x30);
    } else {
        return 0;
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
    bool ok = ph && wr(ph + g_cfg.offPos, x) && wr(ph + g_cfg.offPos + 8, z);
    if (ok) wr(ph + 0x91, (uint8_t)1);
    return ok;
}

bool chr_set_pos(uintptr_t chr, const float p[3]) {
    uintptr_t ph = phys_mod(chr);
    bool ok = ph && write_raw(ph + g_cfg.offPos, p, 12);
    if (ok) wr(ph + 0x91, (uint8_t)1);
    return ok;
}

uintptr_t chr_module(uintptr_t chr, int off) {
    uintptr_t mods = ptr(chr + g_cfg.offModules);
    return mods ? ptr(mods + off) : 0;
}

uint32_t chr_id(uintptr_t chr) { return rdv<uint32_t>(chr + g_cfg.offChrId, 0xFFFFFFFF); }

bool is_mounted(uintptr_t pl) {
    uintptr_t ride = chr_module(pl, g_cfg.offRideMod);
    return ride && rdv<uint8_t>(ride + g_cfg.offRideMounted) != 0;
}

static bool torrent_like(uintptr_t c, uintptr_t pl, bool rideFlag = false) {
    if (!c || c == pl || !is_chr(c)) return false;
    if (chr_id(c) == 8002 || chr_id(c) == 8000) return true;
    if (!rideFlag) return false;
    uintptr_t ride = chr_module(c, g_cfg.offRideMod);
    return ride && rdv<uint8_t>(ride + 0x33) != 0;
}

static uintptr_t scan_chr_set(uintptr_t set, uintptr_t pl) {
    uint64_t vt = rdv<uint64_t>(set);
    if (!in_image(vt)) return 0;
    uint32_t cap = rdv<uint32_t>(set + 0x10);
    uintptr_t entries = ptr(set + 0x18);
    if (!entries || !cap || cap > 4096) return 0;
    for (uint32_t i = 0; i < cap; ++i) {
        uintptr_t c = ptr(entries + (uintptr_t)i * 0x10);
        if (torrent_like(c, pl)) return c;
    }
    return 0;
}

uintptr_t find_torrent(uintptr_t pl) {
    if (!pl) return 0;
    uintptr_t ride = chr_module(pl, g_cfg.offRideMod);
    if (ride) {
        uintptr_t last = ptr(ride + g_cfg.offRideLastMounted);
        if (torrent_like(last, pl, true)) return last;
    }
    uintptr_t wcm = ptr(g.worldChrMan);
    if (!wcm) return 0;
    if (uintptr_t c = scan_chr_set(wcm + g_cfg.offSummonSet, pl)) return c;
    for (int off = 0x10E00; off < 0x11400; off += 8)
        if (uintptr_t c = scan_chr_set(wcm + off, pl)) return c;
    return 0;
}

bool anim_speed(uintptr_t chr, float& v) {
    uintptr_t b = chr_module(chr, g_cfg.offBehaviorMod);
    return b && rd(b + g_cfg.offAnimSpeed, v);
}

bool set_anim_speed(uintptr_t chr, float v) {
    uintptr_t b = chr_module(chr, g_cfg.offBehaviorMod);
    return b && wr(b + g_cfg.offAnimSpeed, v);
}

bool give_item(uint32_t id, int qty) {
    if (!g.mapItemMan) g.mapItemMan = singleton("MapItemMan");
    if (!g.itemGib || !g.mapItemMan) return false;
    uintptr_t mim = ptr(g.mapItemMan);
    if (!mim) return false;
    alignas(16) int32_t data[16] = {1, (int32_t)id, qty, id < 0x10000000 ? 0 : -1, -1};
    alignas(16) uint8_t out[0x200] = {};
    using Fn = void (*)(uintptr_t, void*, void*, uint64_t);
    ((Fn)g.itemGib)(mim, data, out, 0);
    return true;
}

struct CodePatch {
    const char* name;
    const char* aob;
    int off;
    std::vector<uint8_t> bytes;
    bool cave;
    uint8_t caveAl;
    uintptr_t addr;
    std::vector<uint8_t> orig;
    uintptr_t caveMem;
    bool on;
    bool scanned;
};

static CodePatch s_patches[P_COUNT] = {
    {"MapInCombat1", "E8 ?? ?? ?? ?? 84 C0 75 ?? 38 83 ?? ?? ?? ?? 75 ?? 83 E6 FE", 0, {0x48, 0x31, 0xC0, 0x90, 0x90}, false, 0, 0, {}, 0, false, false},
    {"MapInCombat2", "74 ?? C7 45 ?? ?? ?? ?? ?? C7 45 ?? ?? ?? ?? ?? C7 45 ?? ?? ?? ?? ?? 48 8D 05 ?? ?? ?? ?? 48 89 45 ?? 48 8D 4D ?? E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 48 83 BF", 0, {0xEB}, false, 0, 0, {}, 0, false, false},
    {"RideAnywhere", "48 8B 40 68 80 78 36 00 0F 95 C0", 0, {0x48, 0x8B, 0x40, 0x68, 0xC6, 0x40, 0x36, 0x00, 0x80, 0x78, 0x36, 0x00}, true, 0, 0, {}, 0, false, false},
    {"HorseJump1", "0F B6 81 ?? ?? ?? ?? 88 87 ?? ?? ?? ?? 48 8B 83 ?? ?? ?? ?? 48 8B 48 ?? 80 B9", 0, {0xB0, 0x01, 0x90, 0x90, 0x90, 0x90, 0x90}, false, 0, 0, {}, 0, false, false},
    {"HorseJump2", "88 87 ?? ?? ?? ?? 48 8B 83 ?? ?? ?? ?? 48 8B 48 ?? E8 ?? ?? ?? ?? 88 87", 0, {}, true, 1, 0, {}, 0, false, false},
    {"HorseAirJump", "E8 ?? ?? ?? ?? 88 87 ?? ?? ?? ?? 48 8B 83 ?? ?? ?? ?? 48 8B 48 ?? E8 ?? ?? ?? ?? 48 8B CF 88 87", 5, {}, true, 0, 0, {}, 0, false, false},
    {"RideSpecial", "80 79 36 00 0F 95 C0 48 83 C4 28 C3", 4, {0x30, 0xC0, 0x90}, false, 0, 0, {}, 0, false, false},
};

static bool code_write(uintptr_t a, const uint8_t* b, size_t n) {
    DWORD old;
    if (!VirtualProtect((void*)a, n, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy((void*)a, b, n);
    VirtualProtect((void*)a, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)a, n);
    return true;
}

static uintptr_t alloc_near(uintptr_t target) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uintptr_t gran = si.dwAllocationGranularity;
    uintptr_t lo = target > 0x70000000 ? target - 0x70000000 : 0x10000;
    for (uintptr_t a = (target & ~(gran - 1)) - gran; a > lo; a -= gran) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        if (mbi.State == MEM_FREE)
            if (void* p = VirtualAlloc((void*)a, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uintptr_t)p;
    }
    return 0;
}

static const char* kPatchAlt[P_COUNT] = {
    "E8 ?? ?? ?? ?? 84 C0 75 ?? 38 83 F2 3B 00 00",
    "74 2E C7 45 50 58 02 00 00 C7 45 54 02 00 00 00 C7 45 58 01 00 00 00",
    nullptr, nullptr, nullptr, nullptr, nullptr};

static void patch_scan(CodePatch& p) {
    if (p.scanned) return;
    p.scanned = true;
    int idx = (int)(&p - s_patches);
    if (idx == P_MAP2) {
        patch_scan(s_patches[P_MAP1]);
        uintptr_t ref = s_patches[P_MAP1].addr;
        static uintptr_t hits[256];
        size_t n = aob_all(p.aob, hits, 256);
        uintptr_t best = n == 1 ? hits[0] : 0;
        if (!best && kPatchAlt[idx] && aob_all(kPatchAlt[idx], hits, 256) == 1) best = hits[0];
        uint64_t bestD = best && ref ? (best > ref ? best - ref : ref - best) : 0;
        p.addr = best;
        elog("Patch %-13s %s -> %p (%zu candidats, distance 0x%llX)", p.name, p.addr ? "ok" : "INTROUVABLE", (void*)p.addr, n,
             (unsigned long long)(bestD == ~0ull ? 0 : bestD));
        return;
    }
    uintptr_t a = aob(p.aob);
    if (!a && kPatchAlt[idx]) a = aob(kPatchAlt[idx]);
    p.addr = a ? a + p.off : 0;
    elog("Patch %-13s %s -> %p", p.name, p.addr ? "ok" : "INTROUVABLE", (void*)p.addr);
}

bool patch_found(int id) {
    if (id < 0 || id >= P_COUNT) return false;
    patch_scan(s_patches[id]);
    return s_patches[id].addr != 0;
}

bool patch_on(int id) { return id >= 0 && id < P_COUNT && s_patches[id].on; }

uintptr_t patch_slot(int id) {
    if (id < 0 || id >= P_COUNT || !s_patches[id].on || !s_patches[id].caveMem) return 0;
    return *(volatile uintptr_t*)(s_patches[id].caveMem + 0x100);
}

void patch_flag(int id, bool on) {
    if (id < 0 || id >= P_COUNT || !s_patches[id].caveMem) return;
    *(volatile uint8_t*)(s_patches[id].caveMem + 0x100) = on ? 1 : 0;
}

static bool patch_intact(const CodePatch& p) {
    uint8_t b = 0;
    if (!rd(p.addr, b)) return false;
    return p.cave ? b == 0xE9 : (!p.bytes.empty() && b == p.bytes[0]);
}

bool patch_set(int id, bool on) {
    if (!patch_found(id)) return false;
    CodePatch& p = s_patches[id];
    if (p.on && on && !patch_intact(p)) {
        elog("Patch %s ecrase par le jeu, reapplication", p.name);
        p.on = false;
    }
    if (p.on == on) return true;
    if (on) {
        if (p.cave) {
            const size_t n = p.bytes.empty() ? 6 : 8;
            p.orig.resize(n);
            memcpy(p.orig.data(), (void*)p.addr, n);
            if (!p.caveMem) p.caveMem = alloc_near(p.addr);
            if (!p.caveMem) return false;
            uint8_t c[64];
            size_t k = 0;
            int pidx = (int)(&p - s_patches);
            auto rel_slot = [&]() {
                int32_t rel = (int32_t)((p.caveMem + 0x100) - (p.caveMem + k + 4));
                memcpy(c + k, &rel, 4);
                k += 4;
            };
            if (pidx == P_RIDE) {
                memcpy(c + k, p.bytes.data(), 4);
                k += 4;
                c[k++] = 0x48; c[k++] = 0x89; c[k++] = 0x05;
                rel_slot();
                memcpy(c + k, p.bytes.data() + 4, 8);
                k += 8;
            } else if (pidx == P_HFALL) {
                static const uint8_t want[6] = {0x88, 0x87, 0x36, 0x01, 0x00, 0x00};
                if (memcmp(p.orig.data(), want, 6) != 0) {
                    elog("Patch %s : instruction inattendue %02X %02X %02X %02X %02X %02X, non applique", p.name, p.orig[0], p.orig[1], p.orig[2], p.orig[3], p.orig[4], p.orig[5]);
                    p.addr = 0;
                    return false;
                }
                c[k++] = 0x0A; c[k++] = 0x05;
                rel_slot();
                memcpy(c + k, p.orig.data(), n);
                k += n;
            } else if (p.bytes.empty()) {
                c[k++] = 0xB0;
                c[k++] = p.caveAl;
                memcpy(c + k, p.orig.data(), n);
                k += n;
            } else {
                memcpy(c + k, p.bytes.data(), p.bytes.size());
                k += p.bytes.size();
            }
            c[k++] = 0xE9;
            int32_t back = (int32_t)((p.addr + n) - (p.caveMem + k + 4));
            memcpy(c + k, &back, 4);
            k += 4;
            memcpy((void*)p.caveMem, c, k);
            FlushInstructionCache(GetCurrentProcess(), (void*)p.caveMem, k);
            uint8_t j[8] = {0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90};
            int32_t to = (int32_t)(p.caveMem - (p.addr + 5));
            memcpy(j + 1, &to, 4);
            if (!code_write(p.addr, j, n)) return false;
        } else {
            p.orig.resize(p.bytes.size());
            memcpy(p.orig.data(), (void*)p.addr, p.bytes.size());
            if (!code_write(p.addr, p.bytes.data(), p.bytes.size())) return false;
        }
    } else {
        if (!code_write(p.addr, p.orig.data(), p.orig.size())) return false;
    }
    p.on = on;
    elog("Patch %s %s", p.name, on ? "active" : "desactive");
    return true;
}

static std::unordered_map<int, uint16_t> s_stateOrig;

int speffect_state_set(int row, bool zero) {
    auto t = param_tables("SP_EFFECT_PARAM_ST");
    if (t.empty()) return -1;
    uintptr_t d = param_find(t[0], row);
    if (!d) return -1;
    int idx = -1;
    for (int i = 0, n = param_rows(t[0]); i < n; ++i) {
        int32_t id;
        uintptr_t dd;
        if (param_row(t[0], i, id, dd) && id == row) { idx = i; break; }
    }
    int32_t nid;
    uintptr_t next = 0;
    if (idx < 0 || !param_row(t[0], idx + 1, nid, next) || next - d != 0x390) return -2;
    uintptr_t f = d + g_cfg.offStateInfo;
    uint16_t cur = rdv<uint16_t>(f);
    if (zero) {
        if (!s_stateOrig.count(row)) s_stateOrig[row] = cur;
        return wr(f, (uint16_t)0) ? 1 : -1;
    }
    auto it = s_stateOrig.find(row);
    if (it != s_stateOrig.end()) {
        wr(f, it->second);
        s_stateOrig.erase(it);
    }
    return 1;
}

int cur_anim(uintptr_t chr) {
    uintptr_t ta = chr_module(chr, g_cfg.offTimeActMod);
    if (!ta) return -1;
    return rdv<int32_t>(ta + 0x20, -1);
}

int anim_queue(uintptr_t chr, int out[10]) {
    uintptr_t ta = chr_module(chr, g_cfg.offTimeActMod);
    int n = 0;
    if (!ta) return 0;
    for (int i = 0; i < 10; ++i) {
        int v = rdv<int32_t>(ta + 0x20 + i * 0x10, -1);
        if (v > 0) out[n++] = v;
    }
    return n;
}

struct EzArg {
    union {
        float f;
        int32_t i;
        uint64_t u;
    } v;
    int32_t type;
    int32_t pad;
};

struct EzEvt;
struct EzVmt {
    void (*dtor)(EzEvt*);
    void (*unk08)(EzEvt*);
    int (*event_id)(EzEvt*);
    int (*arg_count)(EzEvt*);
    EzArg* (*arg_at)(EzEvt*, int);
};

struct EzEvt {
    EzVmt* vt;
    EzArg* args;
    int count;
};

struct EzMenuJob {
    uint32_t unk[4];
    uint32_t jobId;
    uint32_t pad;
    void* parent;
};

struct EzTalkIns {
    uint8_t unk0[0xC];
    uint32_t f1;
    uint32_t f2;
    uint8_t unk14[0x40 - 0x14];
    uint64_t handle;
    uint8_t unk48[0x98 - 0x48];
    EzMenuJob* job;
};

struct EzField08 {
    uint8_t unk[0x30];
    uint64_t handle;
};

struct EzTalkEvent {
    void** vt;
    EzField08* f08;
    EzTalkIns* talk;
    uint32_t unk18;
};

static void ez_stub(EzEvt*) {}
static int ez_id(EzEvt* e) { return e->count ? e->args[0].v.i : 0; }
static int ez_count(EzEvt* e) { return e->count; }
static EzArg* ez_at(EzEvt* e, int i) { return e->args + i; }
static EzVmt s_ezVmt = {ez_stub, ez_stub, ez_id, ez_count, ez_at};

bool ezstate_ready() { return g.ezDispatch != 0; }

bool ezstate_event(int id, const std::vector<int>& args) {
    if (!g.ezDispatch) return false;
    std::vector<EzArg> a(args.size() + 1);
    a[0].v.u = 0;
    a[0].v.i = id;
    a[0].type = 2;
    for (size_t k = 0; k < args.size(); ++k) {
        a[k + 1].v.u = 0;
        a[k + 1].v.i = args[k];
        a[k + 1].type = 2;
    }
    EzField08* f08 = (EzField08*)calloc(1, sizeof(EzField08));
    EzTalkIns* talk = (EzTalkIns*)calloc(1, sizeof(EzTalkIns));
    EzMenuJob* job = (EzMenuJob*)calloc(1, sizeof(EzMenuJob));
    uint64_t h = ~0ull;
    if (uintptr_t pl = player()) {
        uint64_t ph = rdv<uint64_t>(pl + 8, ~0ull);
        if (ph) h = ph;
    }
    f08->handle = h;
    talk->handle = h;
    talk->job = job;
    job->parent = talk;
    EzTalkEvent te{nullptr, f08, talk, 0};
    EzEvt ev{&s_ezVmt, a.data(), (int)a.size()};
    using Fn = void (*)(EzTalkEvent*, EzEvt*);
    ((Fn)g.ezDispatch)(&te, &ev);
    free(job);
    free(talk);
    free(f08);
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
    if (uintptr_t P = pgd()) {
        uintptr_t head = ptr(P + 0x418);
        uint32_t len = rdv<uint32_t>(P + 0x420), cap = rdv<uint32_t>(P + 0x414);
        if (head && cap && cap <= 8192 && len <= cap) {
            uint32_t seen = 0;
            for (uint32_t i = 0; i < cap && seen < len; ++i) {
                uint32_t e[3];
                if (!read_raw(head + (uintptr_t)i * 0x18, e, 12)) break;
                if (e[0] == 0 || e[0] == 0xFFFFFFFF || e[1] == 0xFFFFFFFF) continue;
                ++seen;
                if (e[2] >= 1 && e[2] <= 9999) out.push_back({e[0], e[1], e[2]});
            }
            if (!out.empty()) return true;
        }
    }
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
static std::vector<uintptr_t> s_allFmgs;
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
    return (soff >= 0x28 && soff < size) || (soff > p && soff < p + size);
}

static uintptr_t fmg_offsets(uintptr_t f) {
    uint64_t soff = rdv<uint64_t>(f + 0x18);
    return soff > f ? (uintptr_t)soff : f + (uintptr_t)soff;
}

static bool fmg_get(uintptr_t f, uint32_t id, std::wstring& out) {
    int32_t groups = rdv<int32_t>(f + 0xC);
    uintptr_t offs = fmg_offsets(f);
    int lo = 0, hi = groups - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int32_t gr[4];
        if (!read_raw(f + 0x28 + (uintptr_t)mid * 16, gr, 16)) return false;
        if ((int32_t)id < gr[1]) hi = mid - 1;
        else if ((int32_t)id > gr[2]) lo = mid + 1;
        else {
            int64_t so = rdv<int64_t>(offs + (uintptr_t)(gr[0] + ((int32_t)id - gr[1])) * 8);
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
    uintptr_t repo = ptr(g.msgRepo);
    if (!repo) return;
    {
        uintptr_t table = ptr(repo + 8);
        std::vector<uintptr_t> direct;
        if (table)
            for (int idx : {11, 115, 310, 410}) {
                uintptr_t f = ptr(table + (uintptr_t)idx * 8);
                if (f && fmg_header(f)) direct.push_back(f);
            }
        std::wstring test;
        bool ok = false;
        for (uintptr_t f : direct)
            if (fmg_get(f, 1000000, test)) ok = true;
        elog("Noms d'armes : %zu FMG par index (dague=%s)", direct.size(), ok ? "ok" : "non");
        if (ok) {
            std::lock_guard<std::mutex> lk(s_nm);
            s_weaponFmgs = direct;
            s_namesReady = true;
            s_names.clear();
        }
    }
    std::vector<uintptr_t> fmgs;
    std::unordered_set<uintptr_t> visited;
    std::vector<std::pair<uintptr_t, int>> stack{{repo, 0}};
    while (!stack.empty() && visited.size() < 1500000) {
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
            if (d < 5) stack.push_back({q, d + 1});
        }
    }
    {
        std::lock_guard<std::mutex> lk(s_nm);
        s_allFmgs = fmgs;
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
    if (!sel.empty() || s_weaponFmgs.empty()) s_weaponFmgs = sel;
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

namespace game {

static std::wstring lower(std::wstring w) {
    for (auto& c : w) c = (wchar_t)towlower(c);
    while (!w.empty() && iswspace(w.back())) w.pop_back();
    return w;
}

bool msgs_ready() {
    std::lock_guard<std::mutex> lk(s_nm);
    return !s_allFmgs.empty();
}

static uintptr_t s_lastHitFmg = 0;

static uint32_t fmg_custom_text(uintptr_t f, const std::wstring& text, uint32_t avoid) {
    int32_t groups = rdv<int32_t>(f + 0xC);
    uintptr_t offs = fmg_offsets(f);
    for (int i = 0; i < groups; ++i) {
        int32_t gr[4];
        if (!read_raw(f + 0x28 + (uintptr_t)i * 16, gr, 16)) return 0;
        for (int32_t id = gr[1]; id <= gr[2] && id - gr[1] < 100000; ++id) {
            uintptr_t slot = offs + (uintptr_t)(gr[0] + (id - gr[1])) * 8;
            if (rdv<int64_t>(slot, 1) != 0 || (uint32_t)id == avoid) continue;
            size_t bytes = (text.size() + 1) * sizeof(wchar_t);
            void* mem = VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (!mem) return 0;
            memcpy(mem, text.c_str(), bytes);
            int64_t rel = (int64_t)((uintptr_t)mem - f);
            if (!wr(slot, rel)) return 0;
            return (uint32_t)id;
        }
    }
    return 0;
}

std::vector<uint32_t> find_msgs(const std::vector<std::wstring>& texts, bool prefix) {
    std::vector<uintptr_t> fmgs;
    {
        std::lock_guard<std::mutex> lk(s_nm);
        fmgs = s_allFmgs;
    }
    std::vector<std::wstring> want;
    for (auto& t : texts) want.push_back(lower(t));
    std::vector<uint32_t> out;
    for (uintptr_t f : fmgs)
        fmg_ids(f, [&](uint32_t id) {
            std::wstring s;
            if (!fmg_get(f, id, s) || s.size() > 60) return;
            s = lower(s);
            for (auto& w : want)
                if (prefix ? s.rfind(w, 0) == 0 : s == w) {
                    if (out.empty()) s_lastHitFmg = f;
                    if (std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
                    break;
                }
        });
    return out;
}

uintptr_t rtti_vtable(const char* mangled) {
    size_t n = strlen(mangled);
    std::string pat;
    for (size_t i = 0; i < n; ++i) {
        char b[4];
        snprintf(b, sizeof(b), "%02X ", (unsigned char)mangled[i]);
        pat += b;
    }
    pat += "00";
    uintptr_t str = aob(pat.c_str());
    if (!str) return 0;
    uint32_t tdRva = (uint32_t)(str - 0x10 - g_base);
    uintptr_t col = 0;
    for (uintptr_t a = g_base; a < g_end && !col;) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (re > g_end) re = g_end;
        DWORD pr = mbi.Protect & 0xFF;
        if (mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_GUARD) && (pr == PAGE_READONLY || pr == PAGE_READWRITE || pr == PAGE_EXECUTE_READ || pr == PAGE_EXECUTE_READWRITE))
            for (uintptr_t p = rb + 0xC; p + 12 <= re; p += 4) {
                if (*(uint32_t*)p != tdRva) continue;
                uintptr_t c = p - 0xC;
                if (*(uint32_t*)c == 1 && *(uint32_t*)(c + 4) == 0 && *(uint32_t*)(c + 0x14) == (uint32_t)(c - g_base)) { col = c; break; }
            }
        a = re;
    }
    if (!col) return 0;
    for (uintptr_t a = g_base; a < g_end;) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (re > g_end) re = g_end;
        DWORD pr = mbi.Protect & 0xFF;
        if (mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_GUARD) && (pr == PAGE_READONLY || pr == PAGE_READWRITE))
            for (uintptr_t p = (rb + 7) & ~7ull; p + 16 <= re; p += 8)
                if (*(uint64_t*)p == col && in_image(*(uint64_t*)(p + 8))) return p + 8;
        a = re;
    }
    return 0;
}

using EzEventFn = void (*)(void*, void*);
using EzEnvFn = void* (*)(void*, void*, void*);
static EzEventFn s_oEvent = nullptr;
static EzEnvFn s_oEnv = nullptr;
static std::atomic<bool> s_esdReady{false};
static uint32_t s_msgRebirth = 0, s_msgForge = 0;
static std::vector<uint32_t> s_msgLevelUp, s_msgQuit;
static void (*s_esdCb)(int) = nullptr;
static thread_local bool tl_inject = false;
static void* s_listOwner = nullptr;
static bool s_isGrace = false, s_injected = false, s_armed = false;
static int s_quitIdx = -1, s_lastIdx = -1, s_arg3 = -1, s_argc = 4;
static constexpr int kIdRebirth = 9001, kIdForge = 9002;

static bool has_id(const std::vector<uint32_t>& v, uint32_t id) { return std::find(v.begin(), v.end(), id) != v.end(); }

static int s_evLog = 0;

static int ev_int(void* ext, int i) {
    void** vt = *(void***)ext;
    auto at = (EzArg * (*)(void*, int)) vt[4];
    EzArg* a = at(ext, i);
    if (!a) return 0;
    return a->type == 1 ? (int)a->v.f : a->v.i;
}

static void inject_entries(void* talkEvent) {
    if (s_injected) return;
    elog("ESD: ajout des boutons (quitter=%d, args=%d)", s_quitIdx, s_argc);
    s_injected = true;
    s_armed = true;
    tl_inject = true;
    for (auto [idx, msg] : {std::pair<int, uint32_t>{kIdRebirth, s_msgRebirth}, {kIdForge, s_msgForge}}) {
        if (!msg) continue;
        EzArg a[4] = {};
        a[0].v.i = 19; a[0].type = 2;
        a[1].v.i = idx; a[1].type = 2;
        a[2].v.i = (int32_t)msg; a[2].type = 2;
        a[3].v.i = s_arg3; a[3].type = 2;
        EzEvt ev{&s_ezVmt, a, s_argc > 4 ? 4 : s_argc};
        s_oEvent(talkEvent, &ev);
    }
    tl_inject = false;
}

static void hk_event(void* talkEvent, void* ext) {
    if (!tl_inject && ext && s_esdReady) {
        void** vt = *(void***)ext;
        int id = ((int (*)(void*))vt[2])(ext);
        int argc = ((int (*)(void*))vt[3])(ext);
        if ((id == 19 || id == 20 || id == 10 || id == 76) && s_evLog < 80) {
            ++s_evLog;
            elog("ESD ev talk=%d id=%d argc=%d a1=%d a2=%d", rdv<int32_t>((uintptr_t)talkEvent + 0x18), id, argc,
                 argc > 1 ? ev_int(ext, 1) : 0, argc > 2 ? ev_int(ext, 2) : 0);
        }
        if (id == 20) {
            s_listOwner = talkEvent;
            s_isGrace = s_injected = false;
            s_quitIdx = s_lastIdx = -1;
        } else if (id == 19 && argc >= 3) {
            if (talkEvent != s_listOwner) {
                s_listOwner = talkEvent;
                s_isGrace = s_injected = false;
            }
            int idx = ev_int(ext, 1);
            uint32_t msg = (uint32_t)ev_int(ext, 2);
            s_argc = argc;
            if (argc >= 4) s_arg3 = ev_int(ext, 3);
            if (has_id(s_msgLevelUp, msg) && !s_isGrace) {
                s_isGrace = true;
                elog("ESD: menu de grace detecte (talk %d)", rdv<int32_t>((uintptr_t)talkEvent + 0x18));
            }
            if (has_id(s_msgQuit, msg)) {
                s_quitIdx = idx;
                if (s_isGrace) inject_entries(talkEvent);
            }
            s_lastIdx = idx;
        } else if ((id == 10 || id == 76) && s_isGrace && talkEvent == s_listOwner) {
            if (s_quitIdx < 0) s_quitIdx = s_lastIdx;
            inject_entries(talkEvent);
        }
    }
    s_oEvent(talkEvent, ext);
}

static void* hk_env(void* env, void* result, void* query) {
    void* r = s_oEnv(env, result, query);
    if (s_armed && result) {
        EzArg* v = (EzArg*)result;
        if (v->type == 2 && (v->v.i == kIdRebirth || v->v.i == kIdForge)) {
            int which = v->v.i;
            v->v.i = s_quitIdx >= 0 ? s_quitIdx : 0;
            s_armed = false;
            if (s_esdCb) s_esdCb(which == kIdRebirth ? 1 : 0);
        }
    }
    return r;
}

void esd_set_callback(void (*cb)(int)) { s_esdCb = cb; }
bool esd_ready() { return s_esdReady; }

bool esd_install() {
    if (s_esdReady) return true;
    if (!msgs_ready()) return false;
    auto reb = find_msgs({L"Renaissance", L"Rebirth", L"Renacer", L"Rinascita", L"Wiedergeburt"}, false);
    auto forge = find_msgs({L"Renforcer l'armement", L"Renforcer l’armement", L"Strengthen armament", L"Strengthen armaments"}, false);
    if (forge.empty()) forge = find_msgs({L"Renforcer", L"Strengthen"}, true);
    s_msgLevelUp = find_msgs({L"Monter de niveau", L"Level Up", L"Level up"}, false);
    s_msgQuit = find_msgs({L"Quitter", L"Leave"}, false);
    s_msgRebirth = reb.empty() ? 0 : reb[0];
    s_msgForge = forge.empty() ? 0 : forge[0];
    uintptr_t envVt = rtti_vtable(".?AVCSEzStateTalkEnv@CS@@");
    uintptr_t evVt = rtti_vtable(".?AVCSEzStateTalkEvent@CS@@");
    uintptr_t envFn = envVt ? rdv<uintptr_t>(envVt + 8) : 0;
    uintptr_t evFn = evVt ? rdv<uintptr_t>(evVt + 8) : g.ezDispatch;
    elog("ESD: renaissance=%u forge=%u niveau=%zu quitter=%zu envVt=%p evVt=%p ev=%p dispatch=%p", s_msgRebirth, s_msgForge,
         s_msgLevelUp.size(), s_msgQuit.size(), (void*)envVt, (void*)evVt, (void*)evFn, (void*)g.ezDispatch);
    static bool hooked = false;
    if (!hooked && envFn && evFn) {
        MH_Initialize();
        if (MH_CreateHook((void*)evFn, (void*)hk_event, (void**)&s_oEvent) == MH_OK &&
            MH_CreateHook((void*)envFn, (void*)hk_env, (void**)&s_oEnv) == MH_OK &&
            MH_EnableHook((void*)evFn) == MH_OK && MH_EnableHook((void*)envFn) == MH_OK)
            hooked = true;
        elog("ESD: hooks %s", hooked ? "ok" : "ECHEC");
    }
    if (!hooked || !(s_msgRebirth || s_msgForge) || s_msgLevelUp.empty()) {
        elog("ESD: pas pret (hooks %d, renaissance %u, forge %u, niveau %zu)", (int)hooked, s_msgRebirth, s_msgForge, s_msgLevelUp.size());
        return false;
    }
    s_esdReady = true;
    elog("ESD: menu de grace etendu");
    return true;
}

static uintptr_t s_magicData = 0;

int selected_spell() {
    uintptr_t P = pgd();
    if (!P) return -1;
    if (!s_magicData || rdv<uintptr_t>(s_magicData + 8) != P + 0x2B0) {
        s_magicData = 0;
        uintptr_t m = ptr(P + 0x530);
        if (m && rdv<uintptr_t>(m + 8) == P + 0x2B0) s_magicData = m;
        for (int off = 0x2B0; off < 0x1000 && !s_magicData; off += 8) {
            uintptr_t m = ptr(P + off);
            if (m && rdv<uintptr_t>(m + 8) == P + 0x2B0) {
                int sel = rdv<int32_t>(m + 0x80, 99);
                if (sel >= -1 && sel < 14) s_magicData = m;
            }
        }
        if (!s_magicData) return -1;
    }
    int sel = rdv<int32_t>(s_magicData + 0x80, -1);
    if (sel < 0 || sel >= 14) return -1;
    return rdv<int32_t>(s_magicData + 0x10 + sel * 8, -1);
}

void speffects(uintptr_t chr, std::vector<std::pair<int, float>>& out) {
    out.clear();
    uintptr_t se = ptr(chr + 0x178);
    uintptr_t e = se ? ptr(se + 8) : 0;
    for (int i = 0; e && i < 256; ++i) {
        out.push_back({rdv<int32_t>(e + 8, -1), rdv<float>(e + 0x48, 0.f)});
        e = ptr(e + 0x30);
    }
}


uint32_t grace_entity(uint32_t graceFlag) {
    for (uintptr_t t : param_tables("BONFIRE_WARP_PARAM_ST"))
        for (int i = 0, n = param_rows(t); i < n; ++i) {
            int32_t id;
            uintptr_t d;
            if (param_row(t, i, id, d) && rdv<uint32_t>(d + 4) == graceFlag) return rdv<uint32_t>(d + 8);
        }
    return 0;
}

bool warp_to(uint32_t entity) {
    if (!g.luaWarp || !g.luaEventMan || !entity) return false;
    uintptr_t man = ptr(g.luaEventMan);
    if (!man) return false;
    uintptr_t proxy = ptr(man + 8), imit = ptr(man + 0x18);
    if (!proxy || !imit) return false;
    using Fn = void (*)(uintptr_t, uintptr_t, uint64_t);
    ((Fn)g.luaWarp)(imit, proxy, (uint64_t)entity);
    return true;
}


static std::unordered_map<std::string, uintptr_t> s_singletons;
static bool s_singletonsScanned = false;

static bool in_section(uintptr_t p, const char* sec) {
    auto dos = (IMAGE_DOS_HEADER*)g_base;
    auto nt = (IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    auto sh = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sh) {
        char name[9] = {};
        memcpy(name, sh->Name, 8);
        if (strcmp(name, sec)) continue;
        uintptr_t a = g_base + sh->VirtualAddress;
        if (p >= a && p < a + sh->Misc.VirtualSize) return true;
    }
    return false;
}

uintptr_t singleton(const char* name) {
    if (!s_singletonsScanned) {
        s_singletonsScanned = true;
        static uintptr_t hits[0x4000];
        size_t n = aob_all("48 8B ?? ?? ?? ?? ?? 48 85 ?? 75 2E 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 4C 8B C8 4C 8D 05 ?? ?? ?? ?? BA ?? ?? 00 00 48 8D 0D ?? ?? ?? ?? E8", hits, 0x4000);
        for (size_t i = 0; i < n; ++i) {
            uintptr_t c = hits[i];
            uintptr_t stat = c + 7 + *(int32_t*)(c + 3);
            uintptr_t rcls = c + 19 + *(int32_t*)(c + 15);
            uintptr_t fn = c + 24 + *(int32_t*)(c + 20);
            const char* path = (const char*)(c + 46 + *(int32_t*)(c + 42));
            if (!in_section(stat, ".data") || !in_section(rcls, ".data") || !in_section(fn, ".text") || !in_section((uintptr_t)path, ".rdata")) continue;
            size_t len = strnlen(path, 256);
            if (len < 14 || len == 256 || strcmp(path + len - 14, "FD4Singleton.h")) continue;
            const char* nm = ((const char* (*)(uintptr_t))fn)(rcls);
            if (!nm) continue;
            const char* simple = strrchr(nm, ':');
            simple = simple ? simple + 1 : nm;
            s_singletons.emplace(simple, stat);
        }
        elog("Singletons FD4 : %zu trouves", s_singletons.size());
    }
    auto it = s_singletons.find(name);
    return it == s_singletons.end() ? 0 : it->second;
}

bool menu_ui_states(uint8_t* out, size_t n) {
    if (!g.menuMan) g.menuMan = singleton("CSMenuMan");
    uintptr_t mm = ptr(g.menuMan);
    return mm && read_raw(mm + 0x90, out, n);
}

int talk_menu_id() {
    uintptr_t mm = ptr(g.menuMan);
    uintptr_t pop = mm ? ptr(mm + 0x80) : 0;
    return pop ? rdv<int32_t>(pop + 0x180, -1) : -1;
}


bool goods_name(uint32_t id, std::wstring& out) {
    uintptr_t repo = ptr(g.msgRepo);
    uintptr_t table = repo ? ptr(repo + 8) : 0;
    if (!table) return false;
    for (int idx : {10, 111, 319, 419}) {
        uintptr_t f = ptr(table + (uintptr_t)idx * 8);
        if (f && fmg_header(f) && fmg_get(f, id, out)) return true;
    }
    return false;
}


uint32_t find_goods(const std::vector<std::wstring>& needles) {
    uintptr_t repo = ptr(g.msgRepo);
    uintptr_t table = repo ? ptr(repo + 8) : 0;
    if (!table) return 0;
    std::vector<std::wstring> low;
    for (auto& n : needles) low.push_back(lower(n));
    uint32_t found = 0;
    for (int idx : {10, 111, 319, 419}) {
        uintptr_t f = ptr(table + (uintptr_t)idx * 8);
        if (!f || !fmg_header(f)) continue;
        fmg_ids(f, [&](uint32_t id) {
            if (found) return;
            std::wstring s;
            if (!fmg_get(f, id, s)) return;
            s = lower(s);
            for (auto& n : low)
                if (s.find(n) != std::wstring::npos) { found = id; return; }
        });
        if (found) break;
    }
    return found;
}


uint32_t find_name(const std::vector<int>& fmgIdx, const std::vector<std::vector<std::wstring>>& groups) {
    uintptr_t repo = ptr(g.msgRepo);
    uintptr_t table = repo ? ptr(repo + 8) : 0;
    if (!table) return 0;
    std::vector<std::vector<std::wstring>> low;
    for (auto& gr : groups) {
        low.emplace_back();
        for (auto& n : gr) low.back().push_back(lower(n));
    }
    uint32_t found = 0;
    for (int idx : fmgIdx) {
        uintptr_t f = ptr(table + (uintptr_t)idx * 8);
        if (!f || !fmg_header(f)) continue;
        fmg_ids(f, [&](uint32_t id) {
            if (found) return;
            std::wstring s;
            if (!fmg_get(f, id, s)) return;
            s = lower(s);
            for (auto& gr : low) {
                bool all = true;
                for (auto& n : gr)
                    if (s.find(n) == std::wstring::npos) { all = false; break; }
                if (all) { found = id; return; }
            }
        });
        if (found) break;
    }
    return found;
}

static uintptr_t s_spAdd = 0, s_spRemove = 0;
static bool s_spScanned = false;

static void sp_scan() {
    if (s_spScanned) return;
    s_spScanned = true;
    uintptr_t a = aob("0F 28 0D ?? ?? ?? ?? ?? 8D ?? ?? 0F 29 ?? ?? ?? 0F B6 D8");
    s_spAdd = a ? a - 0x1D : 0;
    s_spRemove = aob("48 83 EC 28 8B C2 48 8B 51 08 48 85 D2 ?? ?? 90");
    elog("SpEffect add %p remove %p", (void*)s_spAdd, (void*)s_spRemove);
}

bool speffect_ready() {
    sp_scan();
    return s_spAdd && s_spRemove;
}

bool speffect_add(uintptr_t chr, int id) {
    sp_scan();
    if (!s_spAdd || !chr) return false;
    using Fn = uint64_t (*)(uintptr_t, int64_t, int64_t);
    ((Fn)s_spAdd)(chr, id, 1);
    return true;
}

bool speffect_remove(uintptr_t chr, int id) {
    sp_scan();
    uintptr_t se = chr ? ptr(chr + 0x178) : 0;
    if (!s_spRemove || !se) return false;
    using Fn = uint64_t (*)(uintptr_t, int64_t);
    ((Fn)s_spRemove)(se, id);
    return true;
}

void speffects2(uintptr_t chr, std::vector<SpFx>& out) {
    out.clear();
    uintptr_t se = ptr(chr + 0x178);
    uintptr_t e = se ? ptr(se + 8) : 0;
    for (int i = 0; e && i < 256; ++i) {
        out.push_back({rdv<int32_t>(e + 8, -1), rdv<float>(e + 0x40, -1.f), rdv<float>(e + 0x48, -1.f)});
        e = ptr(e + 0x30);
    }
}

static uintptr_t s_gaitemImp = 0;
static bool s_gaitemTried = false;

uintptr_t gaitem(uint32_t h) {
    if (!s_gaitemTried) {
        s_gaitemTried = true;
        uintptr_t a = aob("48 8B 0D ?? ?? ?? ?? 4C 8B C6 48 8D 54");
        s_gaitemImp = a ? rip(a, 3, 7) : 0;
        if (!s_gaitemImp) s_gaitemImp = singleton("CSGaitem");
        elog("CSGaitemImp %s -> %p", s_gaitemImp ? "ok" : "INTROUVABLE", (void*)s_gaitemImp);
    }
    uintptr_t imp = ptr(s_gaitemImp);
    if (!imp || h == 0 || h == 0xFFFFFFFF) return 0;
    return ptr(imp + 8 + (uintptr_t)(h & 0xFFFF) * 8);
}

uint32_t hand_gem(bool right) {
    uintptr_t P = pgd();
    if (!P) return 0xFFFFFFFF;
    int slot = rdv<int32_t>(P + (right ? 0x32C : 0x328), 0);
    if (slot < 0 || slot > 2) slot = 0;
    uint32_t h = rdv<uint32_t>(P + (right ? 0x344 : 0x340) + slot * 8, 0);
    uintptr_t w = gaitem(h);
    if (!w) return 0xFFFFFFFF;
    uint32_t gh = rdv<uint32_t>(w + 0x28, 0);
    if (gh == 0 || gh == 0xFFFFFFFF) return 0;
    uintptr_t gobj = gaitem(gh);
    uint32_t id = gobj ? rdv<uint32_t>(gobj + 0xC, 0) : 0;
    if ((id >> 28) != 0x8) return 0xFFFFFFFF;
    return id & 0x0FFFFFFF;
}

int inv_qty(uint32_t itemId, int setTo) {
    uintptr_t P = pgd();
    if (!P) return -1;
    uintptr_t head = ptr(P + 0x418);
    uint32_t len = rdv<uint32_t>(P + 0x420), cap = rdv<uint32_t>(P + 0x414);
    if (!head || !cap || cap > 8192 || len > cap) return -1;
    uint32_t seen = 0;
    for (uint32_t i = 0; i < cap && seen < len; ++i) {
        uint32_t e[3];
        uintptr_t a = head + (uintptr_t)i * 0x18;
        if (!read_raw(a, e, 12)) return -1;
        if (e[0] == 0 || e[0] == 0xFFFFFFFF || e[1] == 0xFFFFFFFF) continue;
        ++seen;
        if (e[1] != itemId) continue;
        if (setTo > 0 && e[2] >= 1 && e[2] <= 9999) wr(a + 8, (uint32_t)setTo);
        return (int)e[2];
    }
    return 0;
}

bool fmg_text(int fmgIdx, uint32_t id, std::string& out) {
    uintptr_t repo = ptr(g.msgRepo);
    uintptr_t table = repo ? ptr(repo + 8) : 0;
    uintptr_t f = table ? ptr(table + (uintptr_t)fmgIdx * 8) : 0;
    std::wstring w;
    if (!f || !fmg_header(f) || !fmg_get(f, id, w)) return false;
    out = utf8(w);
    return !out.empty();
}

size_t param_row_size(uintptr_t t) {
    int32_t id;
    uintptr_t a = 0, b = 0;
    if (param_rows(t) < 2 || !param_row(t, 0, id, a) || !param_row(t, 1, id, b) || b <= a) return 0;
    return b - a;
}

}
