#include "builds.h"
#include "config.h"
#include "game.h"
#include "mem.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace builds {

using mem::ptr;
using mem::rdv;

static std::mutex s_m;
static std::vector<Build> s_list;

static std::wstring file() { return g_dir + L"EldenFreeBuilds.txt"; }

static void save_file_locked() {
    FILE* f = _wfopen(file().c_str(), L"wb");
    if (!f) return;
    for (auto& b : s_list) {
        fprintf(f, "%s\t%s\t", b.name.c_str(), b.summary.c_str());
        for (int i = 0; i < 8; ++i) fprintf(f, "%s%d", i ? "," : "", b.stats[i]);
        fputc('\t', f);
        for (int i = 0; i < 22; ++i) fprintf(f, "%s%u:%u:%u", i ? "," : "", b.gear[i], b.gearH[i], (unsigned)b.gearHas[i]);
        fputc('\t', f);
        for (int i = 0; i < 16; ++i) fprintf(f, "%s%u:%u", i ? "," : "", b.goods[i], (unsigned)b.goodsHas[i]);
        fputc('\t', f);
        for (int i = 0; i < 14; ++i) fprintf(f, "%s%d", i ? "," : "", b.spells[i]);
        fputc('\t', f);
        for (int i = 0; i < 6; ++i) fprintf(f, "%s%d", i ? "," : "", b.gems[i]);
        fputc('\n', f);
    }
    fclose(f);
}

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> r;
    size_t i = 0;
    for (;;) {
        size_t k = s.find(sep, i);
        r.push_back(s.substr(i, k == std::string::npos ? std::string::npos : k - i));
        if (k == std::string::npos) break;
        i = k + 1;
    }
    return r;
}

void load_file() {
    std::lock_guard<std::mutex> lk(s_m);
    s_list.clear();
    FILE* f = _wfopen(file().c_str(), L"rb");
    if (!f) return;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        auto p = split(s, '\t');
        if (p.size() < 6) continue;
        Build b;
        b.name = p[0];
        b.summary = p[1];
        auto st = split(p[2], ','), ge = split(p[3], ','), go = split(p[4], ','), sp = split(p[5], ',');
        if (st.size() != 8 || ge.size() != 22 || go.size() != 16 || sp.size() != 14) continue;
        for (int i = 0; i < 8; ++i) b.stats[i] = atoi(st[i].c_str());
        bool ok = true;
        for (int i = 0; i < 22; ++i) {
            unsigned a = 0, h = 0, has = 0;
            if (sscanf(ge[i].c_str(), "%u:%u:%u", &a, &h, &has) != 3) ok = false;
            b.gear[i] = a;
            b.gearH[i] = h;
            b.gearHas[i] = (uint8_t)has;
        }
        for (int i = 0; i < 16; ++i) {
            unsigned a = 0, has = 0;
            if (sscanf(go[i].c_str(), "%u:%u", &a, &has) != 2) ok = false;
            b.goods[i] = a;
            b.goodsHas[i] = (uint8_t)has;
        }
        for (int i = 0; i < 14; ++i) b.spells[i] = atoi(sp[i].c_str());
        if (p.size() >= 7) {
            auto gm = split(p[6], ',');
            if (gm.size() == 6)
                for (int i = 0; i < 6; ++i) b.gems[i] = atoi(gm[i].c_str());
        }
        if (ok) s_list.push_back(b);
    }
    fclose(f);
}

std::vector<Build> list() {
    std::lock_guard<std::mutex> lk(s_m);
    return s_list;
}

size_t count() {
    std::lock_guard<std::mutex> lk(s_m);
    return s_list.size();
}

bool get(size_t i, Build& b) {
    std::lock_guard<std::mutex> lk(s_m);
    if (i >= s_list.size()) return false;
    b = s_list[i];
    return true;
}

void add(const Build& b) {
    std::lock_guard<std::mutex> lk(s_m);
    s_list.push_back(b);
    save_file_locked();
}

void overwrite(size_t i, const Build& b) {
    std::lock_guard<std::mutex> lk(s_m);
    if (i >= s_list.size()) return;
    s_list[i] = b;
    save_file_locked();
}

void remove(size_t i) {
    std::lock_guard<std::mutex> lk(s_m);
    if (i >= s_list.size()) return;
    s_list.erase(s_list.begin() + (ptrdiff_t)i);
    save_file_locked();
}

static uintptr_t s_fnGear = 0, s_fnGoods = 0, s_fnMagic = 0;
static bool s_tried = false;

static uintptr_t unique_exec(const char* pat) {
    uintptr_t hits[16];
    size_t n = mem::aob_all(pat, hits, 16);
    uintptr_t r = 0;
    int c = 0;
    for (size_t i = 0; i < n; ++i) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery((void*)hits[i], &mbi, sizeof(mbi))) continue;
        DWORD pr = mbi.Protect & 0xFF;
        if (pr == PAGE_EXECUTE || pr == PAGE_EXECUTE_READ || pr == PAGE_EXECUTE_READWRITE || pr == PAGE_EXECUTE_WRITECOPY) {
            ++c;
            r = hits[i];
        }
    }
    return c == 1 ? r : 0;
}

bool ready() {
    if (!s_tried) {
        s_tried = true;
        uintptr_t g = unique_exec("?? 8B F1 ?? 8B D8 ?? 63 EA ?? 8B F9");
        s_fnGear = g ? g - 0x17 : 0;
        s_fnGoods = unique_exec("?? FA ?? ?? 0F ?? 81 C1 ?? ?? ?? ?? ?? 8B C1 E9");
        s_fnMagic = unique_exec("?? 89 5C ?? ?? ?? 89 74 ?? ?? 57 ?? 83 EC ?? ?? 8B C2 8B F9 ?? 8B C8");
        elog("Builds : equiper %p, objets %p, sorts %p", (void*)s_fnGear, (void*)s_fnGoods, (void*)s_fnMagic);
    }
    return s_fnGear && s_fnGoods && s_fnMagic;
}

struct Inv {
    uintptr_t normHead = 0;
    uint32_t keyCap = 0, keyLen = 0, normCap = 0, normLen = 0;
    std::vector<uint32_t> key, norm;
};

static bool inv_read(Inv& v) {
    uintptr_t P = game::pgd();
    if (!P) return false;
    v.normCap = rdv<uint32_t>(P + 0x414);
    v.normLen = rdv<uint32_t>(P + 0x420);
    v.keyCap = rdv<uint32_t>(P + 0x424);
    v.keyLen = rdv<uint32_t>(P + 0x430);
    uintptr_t nh = ptr(P + 0x418), kh = ptr(P + 0x428);
    if (!nh || !kh || !v.normCap || v.normCap > 8192 || !v.keyCap || v.keyCap > 4096 || v.normLen > v.normCap || v.keyLen > v.keyCap) return false;
    v.normHead = nh;
    v.norm.resize((size_t)v.normCap * 6);
    v.key.resize((size_t)v.keyCap * 6);
    return mem::read_raw(nh, v.norm.data(), v.norm.size() * 4) && mem::read_raw(kh, v.key.data(), v.key.size() * 4);
}

static bool scan_list(const std::vector<uint32_t>& a, uint32_t cap, uint32_t len, uint32_t item, uint32_t handle, bool exact, uint32_t& pos) {
    uint32_t seen = 0;
    for (uint32_t i = 0; i < cap && seen < len; ++i) {
        uint32_t h = a[(size_t)i * 6], id = a[(size_t)i * 6 + 1];
        if (h == 0 || h == 0xFFFFFFFF || id == 0xFFFFFFFF) continue;
        ++seen;
        if (id == item && (!exact || h == handle)) { pos = i; return true; }
    }
    return false;
}

static bool inv_find(const Inv& v, uint32_t item, uint32_t handle, uint32_t& index, uint32_t* normPos = nullptr) {
    if (item == 0xFFFFFFFF) return false;
    uint32_t pos = 0;
    for (int pass = handle ? 0 : 1; pass < 2; ++pass) {
        bool exact = pass == 0;
        if (scan_list(v.norm, v.normCap, v.normLen, item, handle, exact, pos)) {
            index = v.keyCap + pos;
            if (normPos) *normPos = pos;
            return true;
        }
        if (!normPos && scan_list(v.key, v.keyCap, v.keyLen, item, handle, exact, pos)) {
            index = pos;
            return true;
        }
    }
    return false;
}

static uintptr_t wep_obj(uint32_t h) {
    if ((h >> 28) != 0x8) return 0;
    uintptr_t o = game::gaitem(h);
    return (o && rdv<uint32_t>(o + 8) == h && (rdv<uint32_t>(o + 0xC) >> 28) == 0) ? o : 0;
}

static uintptr_t gem_obj(uint32_t h) {
    if ((h >> 28) != 0xC) return 0;
    uintptr_t o = game::gaitem(h);
    return (o && rdv<uint32_t>(o + 8) == h && (rdv<uint32_t>(o + 0xC) >> 28) == 0x8) ? o : 0;
}

static uint32_t gem_of(uintptr_t wobj) {
    uintptr_t g = gem_obj(rdv<uint32_t>(wobj + 0x28));
    return g ? (rdv<uint32_t>(g + 0xC) & 0x0FFFFFFF) : 0;
}

struct GemLayout {
    bool ok = false, haveNoneG = false, haveNoneW = false;
    uint32_t noneG = 0, noneW = 0;
};

static GemLayout gem_layout(const Inv& v) {
    GemLayout L;
    int pairs = 0, bad = 0;
    uint32_t seen = 0;
    for (uint32_t i = 0; i < v.normCap && seen < v.normLen; ++i) {
        uint32_t h = v.norm[(size_t)i * 6], id = v.norm[(size_t)i * 6 + 1];
        if (h == 0 || h == 0xFFFFFFFF || id == 0xFFFFFFFF) continue;
        ++seen;
        if (uintptr_t w = wep_obj(h)) {
            uint32_t gh = rdv<uint32_t>(w + 0x28);
            if ((gh >> 28) == 0xC) {
                uintptr_t g = gem_obj(gh);
                if (g && rdv<uint32_t>(g + 0x10) == h) ++pairs;
                else ++bad;
            } else if (!L.haveNoneG) {
                L.haveNoneG = true;
                L.noneG = gh;
            }
        } else if (uintptr_t g = gem_obj(h)) {
            uint32_t wh = rdv<uint32_t>(g + 0x10);
            if ((wh >> 28) != 0x8 && !L.haveNoneW) {
                L.haveNoneW = true;
                L.noneW = wh;
            }
        }
    }
    L.ok = bad == 0 && (pairs > 0 || (L.haveNoneG && L.haveNoneW)) && (!L.haveNoneG || L.noneG == 0 || L.noneG == 0xFFFFFFFF) && (!L.haveNoneW || L.noneW == 0 || L.noneW == 0xFFFFFFFF);
    elog("Builds : cendres %s (%d paires, %d incoherentes, vide arme %s0x%X, vide cendre %s0x%X)", L.ok ? "ok" : "structure non reconnue", pairs, bad, L.haveNoneG ? "" : "?", L.noneG,
         L.haveNoneW ? "" : "?", L.noneW);
    return L;
}

static void set_entry_id(Inv& v, uint32_t pos, uint32_t id) {
    mem::wr(v.normHead + (uintptr_t)pos * 0x18 + 4, id);
    v.norm[(size_t)pos * 6 + 1] = id;
}

static bool find_handle(const Inv& v, uint32_t handle, uint32_t& pos) {
    uint32_t seen = 0;
    for (uint32_t i = 0; i < v.normCap && seen < v.normLen; ++i) {
        uint32_t h = v.norm[(size_t)i * 6], id = v.norm[(size_t)i * 6 + 1];
        if (h == 0 || h == 0xFFFFFFFF || id == 0xFFFFFFFF) continue;
        ++seen;
        if (h == handle) { pos = i; return true; }
    }
    return false;
}

static bool find_weapon(const Inv& v, uint32_t item, uint32_t handle, uint32_t& pos) {
    if ((item >> 28) != 0) return false;
    if (find_handle(v, handle, pos)) {
        uint32_t id = v.norm[(size_t)pos * 6 + 1];
        if ((id >> 28) == 0 && id / 10000 == item / 10000) return true;
    }
    for (int pass = 0; pass < 2; ++pass) {
        uint32_t seen = 0;
        for (uint32_t i = 0; i < v.normCap && seen < v.normLen; ++i) {
            uint32_t h = v.norm[(size_t)i * 6], id = v.norm[(size_t)i * 6 + 1];
            if (h == 0 || h == 0xFFFFFFFF || id == 0xFFFFFFFF) continue;
            ++seen;
            if ((h >> 28) != 0x8 || (id >> 28) != 0) continue;
            if (pass == 0 ? id == item : id / 10000 == item / 10000) { pos = i; return true; }
        }
    }
    return false;
}

static bool apply_gem(Inv& v, const GemLayout& L, uint32_t pos, int gemId, uint32_t savedItem) {
    uint32_t wh = v.norm[(size_t)pos * 6], wid = v.norm[(size_t)pos * 6 + 1];
    uintptr_t w = wep_obj(wh);
    if (!w || rdv<uint32_t>(w + 0xC) != wid) return true;
    uint32_t newId = wid - wid % 10000 + ((savedItem % 10000) / 100) * 100 + wid % 100;
    uint32_t curGh = rdv<uint32_t>(w + 0x28);
    uintptr_t curG = gem_obj(curGh);
    uint32_t curGem = curG ? (rdv<uint32_t>(curG + 0xC) & 0x0FFFFFFF) : 0;
    bool ok = true;
    if ((uint32_t)gemId != curGem) {
        uint32_t newGh = 0;
        uintptr_t newG = 0;
        if (gemId > 0) {
            uint32_t want = 0x80000000u | (uint32_t)gemId, attachedH = 0;
            uintptr_t attachedG = 0;
            uint32_t seen = 0;
            for (uint32_t i = 0; i < v.normCap && seen < v.normLen && !newG; ++i) {
                uint32_t h = v.norm[(size_t)i * 6], id = v.norm[(size_t)i * 6 + 1];
                if (h == 0 || h == 0xFFFFFFFF || id == 0xFFFFFFFF) continue;
                ++seen;
                if (id != want) continue;
                uintptr_t g = gem_obj(h);
                if (!g) continue;
                if ((rdv<uint32_t>(g + 0x10) >> 28) != 0x8) { newG = g; newGh = h; }
                else if (!attachedG) { attachedG = g; attachedH = h; }
            }
            if (!newG && attachedG) {
                uint32_t w2h = rdv<uint32_t>(attachedG + 0x10), p2 = 0;
                uintptr_t w2 = wep_obj(w2h);
                if (!w2 || rdv<uint32_t>(w2 + 0x28) != attachedH || !L.haveNoneG) return false;
                mem::wr(w2 + 0x28, L.noneG);
                uint32_t id2 = rdv<uint32_t>(w2 + 0xC), std2 = id2 - id2 % 10000 + id2 % 100;
                mem::wr(w2 + 0xC, std2);
                if (find_handle(v, w2h, p2)) set_entry_id(v, p2, std2);
                elog("Builds : cendre %d retiree de l'arme %u", gemId, id2);
                newG = attachedG;
                newGh = attachedH;
            }
            if (!newG) {
                elog("Builds : cendre %d absente de l'inventaire", gemId);
                return false;
            }
        } else if (!L.haveNoneG) {
            return false;
        }
        if (curG) {
            if (!L.haveNoneW) return false;
            mem::wr(curG + 0x10, L.noneW);
        }
        mem::wr(w + 0x28, newG ? newGh : L.noneG);
        if (newG) mem::wr(newG + 0x10, wh);
        elog("Builds : arme %u, cendre %u -> %d", wid, curGem, gemId);
    }
    if (newId != wid) {
        mem::wr(w + 0xC, newId);
        set_entry_id(v, pos, newId);
        elog("Builds : arme %u -> %u (affinite)", wid, newId);
    }
    return ok;
}

static uintptr_t magic_data(uintptr_t P) {
    uintptr_t m = ptr(P + 0x530);
    return (m && rdv<uintptr_t>(m + 8) == P + 0x2B0) ? m : 0;
}

bool capture(Build& b) {
    uintptr_t P = game::pgd();
    Inv v;
    if (!P || !inv_read(v)) return false;
    uint32_t ent[38];
    if (!mem::read_raw(P + 0x3C, b.stats, sizeof(b.stats)) || !mem::read_raw(P + 0x5F8, ent, sizeof(ent)) || !mem::read_raw(P + 0x340, b.gearH, sizeof(b.gearH))) return false;
    for (int s : b.stats)
        if (s < 1 || s > 99) return false;
    uint32_t idx;
    for (int i = 0; i < 22; ++i) {
        b.gear[i] = ent[i];
        b.gearHas[i] = inv_find(v, ent[i], b.gearH[i], idx) ? 1 : 0;
    }
    for (int i = 0; i < 16; ++i) {
        b.goods[i] = ent[22 + i];
        b.goodsHas[i] = inv_find(v, ent[22 + i], 0, idx) ? 1 : 0;
    }
    uintptr_t md = magic_data(P);
    for (int i = 0; i < 14; ++i) b.spells[i] = md ? rdv<int32_t>(md + 0x10 + i * 8, -1) : -1;
    for (int i = 0; i < 6; ++i) {
        b.gems[i] = -1;
        if (!b.gearHas[i]) continue;
        uintptr_t w = wep_obj(b.gearH[i]);
        if (w) b.gems[i] = (int)gem_of(w);
    }
    std::string r = b.gearHas[1] ? game::weapon_name(b.gear[1]) : "", l = b.gearHas[0] ? game::weapon_name(b.gear[0]) : "";
    b.summary = r;
    if (!l.empty()) b.summary += (b.summary.empty() ? "" : "  +  ") + l;
    for (auto& c : b.summary)
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return true;
}

using GearFn = uint64_t (*)(uintptr_t, uint32_t, const uint32_t*, uint32_t, uint32_t, uint32_t, uint32_t);
using GoodsFn = uint64_t (*)(uintptr_t, uint32_t, const uint32_t*, uint32_t);
using MagicFn = uint64_t (*)(uint32_t, uint8_t*);

void apply_gear(const Build& b, bool stats, Result& r) {
    uintptr_t P = game::pgd();
    Inv v;
    if (!ready() || !P || !game::player() || !inv_read(v)) { r.failed = true; return; }
    if (stats) {
        int sum = 0;
        bool ok = true;
        for (int s : b.stats) {
            if (s < 1 || s > 99) ok = false;
            sum += s;
        }
        if (ok) {
            mem::write_raw(P + 0x3C, b.stats, sizeof(b.stats));
            mem::wr(P + 0x68, (int32_t)(sum - 79));
        }
    }
    uintptr_t egd = P + 0x2B0;
    bool anyGem = false;
    for (int i = 0; i < 6; ++i)
        if (b.gearHas[i] && b.gems[i] >= 0) anyGem = true;
    GemLayout L;
    if (anyGem) L = gem_layout(v);
    static const int order[] = {17, 18, 19, 20, 21, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    for (int slot : order) {
        uint32_t cur = rdv<uint32_t>(P + 0x5F8 + slot * 4), curH = rdv<uint32_t>(P + 0x340 + slot * 4);
        uint32_t item = b.gear[slot];
        if (!b.gearHas[slot]) {
            if (cur == item) continue;
            uint32_t none = 0xFFFFFFFF;
            ((GearFn)s_fnGear)(egd, (uint32_t)slot, &none, 0xFFFFFFFF, 1, 1, 0);
            elog("Builds : emplacement %d vide", slot);
            continue;
        }
        if (slot < 6) {
            uint32_t pos = 0;
            if (!find_weapon(v, item, b.gearH[slot], pos)) {
                elog("Builds : emplacement %d, arme %u absente", slot, item);
                ++r.missing;
                continue;
            }
            if (L.ok && b.gems[slot] >= 0 && !apply_gem(v, L, pos, b.gems[slot], item)) ++r.missing;
            uint32_t h = v.norm[(size_t)pos * 6], id = v.norm[(size_t)pos * 6 + 1];
            if (cur != id || curH != h || L.ok) ((GearFn)s_fnGear)(egd, (uint32_t)slot, &id, v.keyCap + pos, 1, 1, 0);
            elog("Builds : emplacement %d, arme %u (index %u)", slot, id, v.keyCap + pos);
            ++r.equipped;
            continue;
        }
        uint32_t index = 0;
        if (!inv_find(v, item, b.gearH[slot], index)) {
            elog("Builds : emplacement %d, objet 0x%X absent", slot, item);
            ++r.missing;
            continue;
        }
        if (cur == item && (curH == b.gearH[slot] || slot >= 6)) { ++r.equipped; continue; }
        ((GearFn)s_fnGear)(egd, (uint32_t)slot, &item, index, 1, 1, 0);
        ++r.equipped;
    }
}

void apply_rest(const Build& b, Result& r) {
    uintptr_t P = game::pgd();
    Inv v;
    if (!ready() || !P || !game::player() || !inv_read(v)) { r.failed = true; return; }
    uintptr_t egd = P + 0x2B0;
    for (int k = 0; k < 16; ++k) {
        uint32_t cur = rdv<uint32_t>(P + 0x5F8 + (22 + k) * 4), item = b.goods[k];
        if (cur == item) {
            if (b.goodsHas[k]) ++r.equipped;
            continue;
        }
        if (!b.goodsHas[k]) {
            uint32_t none = 0xFFFFFFFF;
            ((GoodsFn)s_fnGoods)(egd, (uint32_t)k, &none, 0xFFFFFFFF);
            continue;
        }
        uint32_t index = 0;
        if (!inv_find(v, item, 0, index)) { ++r.missing; continue; }
        ((GoodsFn)s_fnGoods)(egd, (uint32_t)k, &item, index);
        ++r.equipped;
    }
    uintptr_t md = magic_data(P);
    if (!md) return;
    int cur[14];
    bool same = true;
    for (int i = 0; i < 14; ++i) {
        cur[i] = rdv<int32_t>(md + 0x10 + i * 8, -1);
        if (cur[i] != b.spells[i]) same = false;
    }
    if (same) {
        for (int s : b.spells)
            if (s > 0) ++r.spells;
        return;
    }
    uint32_t slots = rdv<uint32_t>(P + 0xA74);
    elog("Builds : sorts, emplacements lus %u", slots);
    if (slots < 1 || slots > 14) slots = 14;
    for (uint32_t i = 0; i < 14; ++i)
        if (b.spells[i] > 0 && i + 1 > slots) slots = i + 1;
    auto call = [&](uint32_t slot, uint32_t index, uint32_t a, uint32_t c) {
        alignas(16) uint8_t args[128] = {};
        memcpy(args + 0x08, &slot, 4);
        memcpy(args + 0x48, &index, 4);
        memcpy(args + 0x4C, &a, 4);
        memcpy(args + 0x50, &c, 4);
        ((MagicFn)s_fnMagic)(slot, args);
    };
    for (uint32_t i = 0; i < 14; ++i)
        if (cur[i] > 0) call(i, 0xFFFFFFFF, 0x4FFFFFFF, 0xBFFFFFFF);
    for (uint32_t i = 0; i < 14; ++i) {
        int sp = b.spells[i];
        if (sp <= 0) continue;
        uint32_t index = 0, pos = 0;
        if (i >= slots || !inv_find(v, 0x40000000u | (uint32_t)sp, 0, index, &pos)) {
            elog("Builds : sort %d absent de l'inventaire", sp);
            ++r.spellsMissing;
            continue;
        }
        call(i, (v.keyCap + pos) - 1, 0x40000000u + (uint32_t)sp, 0xB0000000u + (uint32_t)sp);
        elog("Builds : sort %d en emplacement %u (index %u)", sp, i, (v.keyCap + pos) - 1);
        ++r.spells;
    }
}

void nudge(int step) {
    static int32_t sl = -1, sr = -1;
    uintptr_t P = game::pgd();
    if (!P || !game::player()) return;
    if (step == 0) {
        sl = rdv<int32_t>(P + 0x328, -1);
        sr = rdv<int32_t>(P + 0x32C, -1);
        if (sl < 0 || sl > 2 || sr < 0 || sr > 2) { sl = sr = -1; return; }
        mem::wr(P + 0x328, (int32_t)((sl + 1) % 3));
        mem::wr(P + 0x32C, (int32_t)((sr + 1) % 3));
        elog("Builds : rafraichissement des armes (emplacements %d/%d)", sl, sr);
    } else if (sl >= 0) {
        mem::wr(P + 0x328, sl);
        mem::wr(P + 0x32C, sr);
        sl = sr = -1;
    }
}

}
