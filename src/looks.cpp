#include "looks.h"
#include "config.h"
#include "game.h"
#include "mem.h"
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace looks {

using mem::rdv;

static const uint32_t kBare[4] = {10000, 10100, 10200, 10300};

static std::atomic<int> s_state{0};
static std::atomic<bool> s_catReady{false};
static std::vector<Piece> s_cat[4];
static std::unordered_map<uint32_t, std::string> s_names;
static std::mutex s_em;
static std::string s_err = "initialisation";

static uintptr_t s_prot = 0, s_vfx = 0, s_sp = 0;
static size_t s_protSize = 0, s_vfxSize = 0, s_spSize = 0;
static int s_catOff = 0xD6, s_tpOff = 0x20;
static int s_vfxId[2] = {0, 0}, s_spId[2] = {0, 0}, s_spRef[2] = {0, 0};
static bool s_headClearFull = false;
static uintptr_t s_vfxRow[2] = {0, 0}, s_spRow[2] = {0, 0};
static std::vector<uint8_t> s_vfxOrig[2], s_spOrig[2], s_spTemplate;
static uint32_t s_carrier = 0;
static uintptr_t s_carRow[3] = {0, 0, 0};
static std::vector<uint8_t> s_carOrig[3];
static bool s_hijacked = false;

static bool s_applied[2] = {false, false};
static uint32_t s_cur[4] = {0, 0, 0, 0};
static bool s_curOn = false;
static double s_reapplyAt = 0, s_checkAt = 0;
static uintptr_t s_lastPl = 0;

int state() { return s_state.load(); }
bool catalog_ready() { return s_catReady.load(); }
const std::vector<Piece>& catalog(int cat) { return s_cat[cat & 3]; }

std::string status_text() {
    std::lock_guard<std::mutex> lk(s_em);
    return s_err;
}

static bool fail(const std::string& m) {
    elog("Cosmetique : %s", m.c_str());
    std::lock_guard<std::mutex> lk(s_em);
    s_err = m;
    return false;
}

std::string piece_name(uint32_t id) {
    if (!s_catReady.load()) return "";
    auto it = s_names.find(id);
    return it == s_names.end() ? "" : it->second;
}

static bool prot(uint32_t id) { return id >= 10000 && game::param_find(s_prot, (int32_t)id) != 0; }

struct TV {
    int id;
    uintptr_t row;
    uint32_t tp;
    bool full, head;
    int spId, spOff;
};

static bool init() {
    auto pt = game::param_tables("EQUIP_PARAM_PROTECTOR_ST");
    auto vt = game::param_tables("SP_EFFECT_VFX_PARAM_ST");
    auto st = game::param_tables("SP_EFFECT_PARAM_ST");
    if (pt.empty() || vt.empty() || st.empty()) return fail("tables de param\xC3\xA8tres introuvables");
    s_prot = pt[0];
    s_vfx = vt[0];
    s_sp = st[0];
    s_protSize = game::param_row_size(s_prot);
    s_vfxSize = game::param_row_size(s_vfx);
    s_spSize = game::param_row_size(s_sp);
    elog("Cosmetique : tailles armure 0x%zX, vfx 0x%zX, effet 0x%zX", s_protSize, s_vfxSize, s_spSize);
    if (s_protSize < 0x100 || s_protSize > 0x400 || s_vfxSize < 0x40 || s_vfxSize > 0x200 || s_spSize < 0x200 || s_spSize > 0x800) return fail("tailles de lignes inattendues");
    uintptr_t bare[4];
    for (int c = 0; c < 4; ++c)
        if (!(bare[c] = game::param_find(s_prot, (int32_t)kBare[c]))) return fail("lignes d'armure de base introuvables");
    auto catOk = [&](int o) {
        for (int c = 0; c < 4; ++c)
            if (rdv<uint8_t>(bare[c] + o, 0xFF) != c) return false;
        return true;
    };
    if (!catOk(s_catOff)) {
        int found = -1, n = 0;
        for (int o = 0; o < (int)s_protSize; ++o)
            if (catOk(o)) { found = o; ++n; }
        if (n != 1) return fail("cat\xC3\xA9gorie d'armure introuvable (" + std::to_string(n) + " candidats)");
        s_catOff = found;
    }
    elog("Cosmetique : categorie d'armure a +0x%X", s_catOff);

    int nv = game::param_rows(s_vfx);
    auto countTp = [&](int o) {
        int c = 0;
        for (int i = 0; i < nv; ++i) {
            int32_t id;
            uintptr_t d;
            if (!game::param_row(s_vfx, i, id, d)) continue;
            int32_t v = rdv<int32_t>(d + o, -1);
            if (v >= 10000 && prot((uint32_t)v + 100) && prot((uint32_t)v + 200) && prot((uint32_t)v + 300)) ++c;
        }
        return c;
    };
    if (countTp(s_tpOff) == 0) {
        int best = -1, bestC = 0;
        for (int o = 0; o + 4 <= (int)s_vfxSize; o += 4) {
            int c = countTp(o);
            if (c > bestC) { bestC = c; best = o; }
        }
        if (best < 0) return fail("aucune transformation d'armure dans les effets visuels");
        s_tpOff = best;
    }
    std::vector<TV> tv;
    for (int i = 0; i < nv; ++i) {
        int32_t id;
        uintptr_t d;
        if (!game::param_row(s_vfx, i, id, d)) continue;
        int32_t v = rdv<int32_t>(d + s_tpOff, -1);
        if (v < 10000) continue;
        bool full = prot((uint32_t)v + 100) && prot((uint32_t)v + 200) && prot((uint32_t)v + 300);
        uintptr_t h = game::param_find(s_prot, v);
        bool head = h && rdv<uint8_t>(h + s_catOff, 0xFF) == 0;
        if (!full && !head) continue;
        tv.push_back({id, d, (uint32_t)v, full, head, 0, 0});
    }
    for (int i = 0, n = game::param_rows(s_sp); i < n; ++i) {
        int32_t id;
        uintptr_t d;
        if (!game::param_row(s_sp, i, id, d)) continue;
        int32_t buf[0x40];
        if (!mem::read_raw(d + 0x100, buf, sizeof(buf))) continue;
        for (int k = 0; k < 0x40; ++k) {
            if (buf[k] <= 0) continue;
            for (auto& t : tv)
                if (t.id == buf[k] && id >= t.spId) { t.spId = id; t.spOff = 0x100 + k * 4; }
        }
    }
    for (size_t i = 0; i < tv.size() && i < 24; ++i)
        elog("Cosmetique : vfx %d -> armure %u (%s%s), octets %02X %02X %02X %02X, effet %d (+0x%X)", tv[i].id, tv[i].tp, tv[i].full ? "corps" : "", tv[i].head ? " tete" : "",
             rdv<uint8_t>(tv[i].row + 0x2C), rdv<uint8_t>(tv[i].row + 0x2D), rdv<uint8_t>(tv[i].row + 0x2E), rdv<uint8_t>(tv[i].row + 0x2F), tv[i].spId, tv[i].spOff);
    const TV* body = nullptr;
    const TV* headRow = nullptr;
    for (auto& t : tv)
        if (t.full && t.spId && (rdv<uint8_t>(t.row + 0x2E) & 0x80)) body = &t;
    if (!body)
        for (auto& t : tv)
            if (t.full && t.spId) body = &t;
    if (!body) return fail("aucune transformation compl\xC3\xA8te utilisable (" + std::to_string(tv.size()) + " vfx)");
    for (auto& t : tv)
        if (&t != body && t.spId && t.spId != body->spId && t.head && !t.full) headRow = &t;
    s_headClearFull = false;
    if (!headRow)
        for (auto& t : tv)
            if (&t != body && t.spId && t.spId != body->spId) { headRow = &t; s_headClearFull = true; }
    if (!headRow) return fail("pas de deuxi\xC3\xA8me transformation pour le casque");
    uintptr_t tpl = game::param_find(s_sp, 310320);
    if (!tpl) return fail("effet mod\xC3\xA8le 310320 introuvable");
    s_spTemplate.resize(s_spSize);
    mem::read_raw(tpl, s_spTemplate.data(), s_spSize);
    float one = 1.f;
    memcpy(s_spTemplate.data() + 0xE4, &one, 4);
    const TV* pick[2] = {headRow, body};
    for (int k = 0; k < 2; ++k) {
        s_vfxId[k] = pick[k]->id;
        s_vfxRow[k] = pick[k]->row;
        s_spId[k] = pick[k]->spId;
        s_spRef[k] = pick[k]->spOff;
        s_spRow[k] = game::param_find(s_sp, s_spId[k]);
        s_vfxOrig[k].resize(s_vfxSize);
        s_spOrig[k].resize(s_spSize);
        mem::read_raw(s_vfxRow[k], s_vfxOrig[k].data(), s_vfxSize);
        mem::read_raw(s_spRow[k], s_spOrig[k].data(), s_spSize);
    }
    s_carrier = body->tp;
    for (int k = 0; k < 3; ++k) {
        s_carRow[k] = game::param_find(s_prot, (int32_t)s_carrier + 100 * (k + 1));
        s_carOrig[k].resize(s_protSize);
        mem::read_raw(s_carRow[k], s_carOrig[k].data(), s_protSize);
    }
    elog("Cosmetique : pret (casque vfx %d effet %d%s, corps vfx %d effet %d, porteur %u)", s_vfxId[0], s_spId[0], s_headClearFull ? " adapte" : "", s_vfxId[1], s_spId[1], s_carrier);
    {
        std::lock_guard<std::mutex> lk(s_em);
        s_err = "pr\xC3\xAAt";
    }
    return true;
}

static void build_catalog() {
    std::vector<std::pair<uint32_t, int>> ids;
    for (int i = 0, n = game::param_rows(s_prot); i < n; ++i) {
        int32_t id;
        uintptr_t d;
        if (!game::param_row(s_prot, i, id, d) || id < 10000) continue;
        int c = rdv<uint8_t>(d + s_catOff, 0xFF);
        if (c > 3 || (uint32_t)id == kBare[c]) continue;
        ids.push_back({(uint32_t)id, c});
    }
    static std::vector<int> fmgs;
    if (fmgs.empty()) {
        std::vector<std::pair<int, int>> score;
        for (int f = 0; f < 700; ++f) {
            int hits = 0;
            size_t len = 0;
            std::string nm;
            for (size_t k = 0; k < ids.size(); k += std::max<size_t>(1, ids.size() / 80)) {
                if (game::fmg_text(f, ids[k].first, nm)) { ++hits; len += nm.size(); }
            }
            if (hits >= 3 && len / hits < 60) score.push_back({hits, f});
        }
        std::sort(score.begin(), score.end(), [](auto& a, auto& b) { return a.first > b.first; });
        std::string l;
        for (auto& sc : score) {
            fmgs.push_back(sc.second);
            l += " " + std::to_string(sc.second) + "(" + std::to_string(sc.first) + ")";
        }
        elog("Cosmetique : %zu armures, textes de noms :%s", ids.size(), l.empty() ? " aucun" : l.c_str());
    }
    std::vector<Piece> cat[4];
    std::unordered_map<uint32_t, std::string> names;
    for (auto& e : ids) {
        std::string name;
        for (int f : fmgs)
            if (game::fmg_text(f, e.first, name) && !name.empty()) break;
        if (name.empty() || name[0] == '[' || name[0] == '%' || name[0] == '?') continue;
        cat[e.second].push_back({e.first, name});
        names[e.first] = name;
    }
    size_t total = 0;
    for (int c = 0; c < 4; ++c) {
        std::sort(cat[c].begin(), cat[c].end(), [](const Piece& a, const Piece& b) { return a.name < b.name; });
        total += cat[c].size();
    }
    if (total < 20) {
        fmgs.clear();
        std::lock_guard<std::mutex> lk(s_em);
        s_err = "noms d'armures introuvables dans les textes du jeu";
        return;
    }
    for (int c = 0; c < 4; ++c) s_cat[c] = std::move(cat[c]);
    s_names = std::move(names);
    s_catReady = true;
    elog("Cosmetique : %zu pieces d'armure listees", total);
}

static void restore_rows() {
    if (!s_hijacked) return;
    for (int k = 0; k < 2; ++k) {
        mem::write_raw(s_vfxRow[k], s_vfxOrig[k].data(), s_vfxSize);
        mem::write_raw(s_spRow[k], s_spOrig[k].data(), s_spSize);
    }
    for (int k = 0; k < 3; ++k) mem::write_raw(s_carRow[k], s_carOrig[k].data(), s_protSize);
    s_hijacked = false;
}

static void write_vfx(int k, uint32_t protBase) {
    std::vector<uint8_t> v = s_vfxOrig[k];
    int32_t none = -1, tp = (int32_t)protBase;
    for (int o = 0; o < 24; o += 4) memcpy(v.data() + o, &none, 4);
    memcpy(v.data() + s_tpOff, &tp, 4);
    if (k == 0 && s_headClearFull) v[0x2E] &= 0x7F;
    mem::write_raw(s_vfxRow[k], v.data(), s_vfxSize);
}

static void write_sp(int k) {
    std::vector<uint8_t> v = s_spTemplate;
    int32_t id = s_vfxId[k];
    memcpy(v.data() + s_spRef[k], &id, 4);
    mem::write_raw(s_spRow[k], v.data(), s_spSize);
}

static bool has_effect(uintptr_t pl, int id) {
    std::vector<std::pair<int, float>> fx;
    game::speffects(pl, fx);
    for (auto& e : fx)
        if (e.first == id) return true;
    return false;
}

static void drop_effects(uintptr_t pl) {
    for (int k = 0; k < 2; ++k) {
        if (pl && (s_applied[k] || (s_hijacked && has_effect(pl, s_spId[k])))) game::speffect_remove(pl, s_spId[k]);
        s_applied[k] = false;
    }
}

static uint32_t worn(int cat) {
    uintptr_t P = game::pgd();
    uint32_t v = P ? rdv<uint32_t>(P + 0x5F8 + (12 + cat) * 4, 0xFFFFFFFF) : 0xFFFFFFFF;
    if ((v >> 28) != 1) return kBare[cat];
    v &= 0x0FFFFFFF;
    return prot(v) ? v : kBare[cat];
}

void tick(double t, uintptr_t pl) {
    static double tryAt = 0;
    if (s_state == 0) {
        if (!game::params_ready() || t < tryAt) return;
        tryAt = t + 5.0;
        static int tries = 0;
        if (init()) s_state = 1;
        else if (++tries >= 3) s_state = -1;
        return;
    }
    if (s_state != 1) return;
    if (!s_catReady && game::msgs_ready() && t > tryAt) {
        tryAt = t + 6.0;
        build_catalog();
    }
    if (!pl || !game::speffect_ready()) return;
    if (pl != s_lastPl) {
        s_lastPl = pl;
        s_applied[0] = s_applied[1] = false;
        s_reapplyAt = t + 2.0;
    }
    uint32_t want[4];
    bool on = g_cfg.lookOn;
    for (int c = 0; c < 4; ++c) {
        want[c] = on ? g_cfg.look[c] : 0;
        if (want[c] && !prot(want[c])) want[c] = 0;
    }
    bool body = want[1] || want[2] || want[3];
    uint32_t eff[4] = {want[0], 0, 0, 0};
    if (body)
        for (int c = 1; c < 4; ++c) eff[c] = want[c] ? want[c] : worn(c);
    bool any = eff[0] || body;
    bool changed = on != s_curOn || memcmp(eff, s_cur, sizeof(eff)) != 0;
    if (changed) {
        drop_effects(pl);
        s_curOn = on;
        memcpy(s_cur, eff, sizeof(eff));
        restore_rows();
        if (!any) return;
        s_hijacked = true;
        if (eff[0]) {
            write_vfx(0, eff[0]);
            write_sp(0);
        }
        if (body) {
            for (int c = 1; c < 4; ++c) {
                uintptr_t src = game::param_find(s_prot, (int32_t)eff[c]);
                std::vector<uint8_t> row(s_protSize);
                if (src && mem::read_raw(src, row.data(), s_protSize)) mem::write_raw(s_carRow[c - 1], row.data(), s_protSize);
            }
            write_vfx(1, s_carrier);
            write_sp(1);
        }
        s_reapplyAt = t + 0.35;
        return;
    }
    if (!any || t < s_reapplyAt || t < s_checkAt) return;
    s_checkAt = t + 0.5;
    bool need[2] = {eff[0] != 0, body};
    for (int k = 0; k < 2; ++k) {
        if (!need[k]) continue;
        if (!has_effect(pl, s_spId[k])) {
            game::speffect_add(pl, s_spId[k]);
            if (!s_applied[k]) elog("Cosmetique : effet %d applique", s_spId[k]);
        }
        s_applied[k] = true;
    }
}

static std::mutex s_sm;
static std::vector<Set> s_sets;
static bool s_setsLoaded = false;

static std::wstring file() { return g_dir + L"EldenFreeTenues.txt"; }

static void load_locked() {
    if (s_setsLoaded) return;
    s_setsLoaded = true;
    FILE* f = _wfopen(file().c_str(), L"rb");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char* tab = strchr(line, '\t');
        if (!tab) continue;
        Set s;
        s.name.assign(line, tab);
        if (sscanf(tab + 1, "%u,%u,%u,%u", &s.ids[0], &s.ids[1], &s.ids[2], &s.ids[3]) == 4) s_sets.push_back(s);
    }
    fclose(f);
}

static void save_locked() {
    FILE* f = _wfopen(file().c_str(), L"wb");
    if (!f) return;
    for (auto& s : s_sets) fprintf(f, "%s\t%u,%u,%u,%u\n", s.name.c_str(), s.ids[0], s.ids[1], s.ids[2], s.ids[3]);
    fclose(f);
}

std::vector<Set> sets() {
    std::lock_guard<std::mutex> lk(s_sm);
    load_locked();
    return s_sets;
}

void add_set(const std::string& name) {
    std::lock_guard<std::mutex> lk(s_sm);
    load_locked();
    Set s;
    s.name = name.empty() ? "Tenue " + std::to_string(s_sets.size() + 1) : name;
    for (auto& c : s.name)
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    for (int c = 0; c < 4; ++c) s.ids[c] = g_cfg.look[c];
    s_sets.push_back(s);
    save_locked();
}

void load_set(size_t i) {
    std::lock_guard<std::mutex> lk(s_sm);
    load_locked();
    if (i >= s_sets.size()) return;
    for (int c = 0; c < 4; ++c) g_cfg.look[c] = s_sets[i].ids[c];
    g_cfg.lookOn = true;
    cfg_save();
}

void remove_set(size_t i) {
    std::lock_guard<std::mutex> lk(s_sm);
    load_locked();
    if (i >= s_sets.size()) return;
    s_sets.erase(s_sets.begin() + (ptrdiff_t)i);
    save_locked();
}

}
