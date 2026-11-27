#include "looks.h"
#include <algorithm>
#include "builds.h"
#include "overlay.h"
#include "features.h"
#include "config.h"
#include "game.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

namespace ui {

static ImFont* g_body = nullptr;
static ImFont* g_title = nullptr;
static ImFont* g_small = nullptr;

static const ImVec4 C_BG(0.055f, 0.051f, 0.047f, 0.97f);
static const ImVec4 C_CARD(0.090f, 0.082f, 0.075f, 1.0f);
static const ImVec4 C_LINE(0.235f, 0.204f, 0.153f, 1.0f);
static const ImVec4 C_GOLD(0.784f, 0.631f, 0.353f, 1.0f);
static const ImVec4 C_GOLD_HI(0.875f, 0.733f, 0.459f, 1.0f);
static const ImVec4 C_GOLD_LO(0.478f, 0.380f, 0.212f, 1.0f);
static const ImVec4 C_TEXT(0.914f, 0.882f, 0.812f, 1.0f);
static const ImVec4 C_DIM(0.604f, 0.565f, 0.494f, 1.0f);
static const ImVec4 C_OK(0.502f, 0.690f, 0.420f, 1.0f);
static const ImVec4 C_BAD(0.784f, 0.333f, 0.239f, 1.0f);
static const ImVec4 C_WARN(0.851f, 0.596f, 0.290f, 1.0f);

static ImFont* try_font(const char* file, float size) {
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x0100, 0x017F, 0x2010, 0x205E, 0x2190, 0x21FF, 0x25A0, 0x25FF, 0};
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "C:\\Windows\\Fonts\\%s", file);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return nullptr;
    return ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size, nullptr, ranges);
}

void setup() {
    ImGuiIO& io = ImGui::GetIO();
    g_body = try_font("segoeui.ttf", 18.f);
    if (!g_body) g_body = io.Fonts->AddFontDefault();
    g_title = try_font("georgiab.ttf", 26.f);
    if (!g_title) g_title = try_font("seguisb.ttf", 26.f);
    if (!g_title) g_title = g_body;
    g_small = try_font("segoeui.ttf", 15.f);
    if (!g_small) g_small = g_body;
    io.FontDefault = g_body;

    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 12.f;
    s.ChildRounding = 9.f;
    s.FrameRounding = 6.f;
    s.GrabRounding = 6.f;
    s.PopupRounding = 8.f;
    s.TabRounding = 6.f;
    s.ScrollbarRounding = 8.f;
    s.WindowBorderSize = 1.f;
    s.ChildBorderSize = 1.f;
    s.FrameBorderSize = 0.f;
    s.WindowPadding = ImVec2(22, 18);
    s.FramePadding = ImVec2(12, 7);
    s.ItemSpacing = ImVec2(10, 10);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 10.f;
    s.GrabMinSize = 14.f;
    s.TabBarBorderSize = 1.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = C_TEXT;
    c[ImGuiCol_TextDisabled] = C_DIM;
    c[ImGuiCol_WindowBg] = C_BG;
    c[ImGuiCol_ChildBg] = C_CARD;
    c[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.065f, 0.06f, 0.98f);
    c[ImGuiCol_Border] = C_LINE;
    c[ImGuiCol_FrameBg] = ImVec4(0.135f, 0.122f, 0.106f, 1.f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.180f, 0.160f, 0.130f, 1.f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.210f, 0.185f, 0.145f, 1.f);
    c[ImGuiCol_CheckMark] = C_GOLD_HI;
    c[ImGuiCol_SliderGrab] = C_GOLD;
    c[ImGuiCol_SliderGrabActive] = C_GOLD_HI;
    c[ImGuiCol_Button] = ImVec4(0.160f, 0.143f, 0.118f, 1.f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.235f, 0.204f, 0.153f, 1.f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.300f, 0.255f, 0.180f, 1.f);
    c[ImGuiCol_Header] = ImVec4(0.160f, 0.143f, 0.118f, 1.f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.200f, 0.178f, 0.141f, 1.f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.240f, 0.208f, 0.160f, 1.f);
    c[ImGuiCol_Separator] = C_LINE;
    c[ImGuiCol_Tab] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TabHovered] = ImVec4(0.200f, 0.178f, 0.141f, 1.f);
    c[ImGuiCol_TabSelected] = ImVec4(0.160f, 0.143f, 0.118f, 1.f);
    c[ImGuiCol_TabSelectedOverline] = C_GOLD;
    c[ImGuiCol_TabDimmed] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TabDimmedSelected] = ImVec4(0.160f, 0.143f, 0.118f, 1.f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = C_LINE;
    c[ImGuiCol_ScrollbarGrabHovered] = C_GOLD_LO;
    c[ImGuiCol_ScrollbarGrabActive] = C_GOLD;
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.110f, 0.100f, 0.090f, 1.f);
    c[ImGuiCol_TableBorderLight] = ImVec4(0.16f, 0.14f, 0.11f, 1.f);
    c[ImGuiCol_TableBorderStrong] = C_LINE;
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.018f);
    c[ImGuiCol_NavCursor] = C_GOLD_HI;
}

static void save() { feat::post(feat::Act::SaveCfg); }

static void rs_scroll() {
    float ry = overlay::pad_rstick_y();
    if (std::fabs(ry) < 0.2f) return;
    float v = (std::fabs(ry) - 0.2f) / 0.8f * (ry > 0 ? 1.f : -1.f);
    ImGui::SetScrollY(ImGui::GetScrollY() - v * 1400.f * ImGui::GetIO().DeltaTime);
}

static void pill(const char* text, const ImVec4& col) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushFont(g_small);
    ImVec2 ts = ImGui::CalcTextSize(text);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ts.y + 6, w = ts.x + 26;
    ImVec4 bg = col;
    bg.w = 0.14f;
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(bg), h * 0.5f);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(ImVec4(col.x, col.y, col.z, 0.45f)), h * 0.5f);
    dl->AddCircleFilled(ImVec2(p.x + 10, p.y + h * 0.5f), 3.2f, ImGui::GetColorU32(col));
    dl->AddText(ImVec2(p.x + 18, p.y + 3), ImGui::GetColorU32(col), text);
    ImGui::Dummy(ImVec2(w, h));
    ImGui::PopFont();
}

static float s_cardTitleY = 0;

static void inline_right_pill(const char* text, const ImVec4& col) {
    ImGui::PushFont(g_small);
    float w = ImGui::CalcTextSize(text).x + 26;
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - w);
    pill(text, col);
}

static void right_pill(const char* text, const ImVec4& col) {
    ImGui::PushFont(g_small);
    float w = ImGui::CalcTextSize(text).x + 26;
    ImGui::PopFont();
    ImVec2 back = ImGui::GetCursorPos();
    float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPos(ImVec2(right - w, s_cardTitleY));
    pill(text, col);
    ImGui::SetCursorPos(back);
}

static void hint(const char* text) {
    ImGui::PushFont(g_small);
    ImGui::PushStyleColor(ImGuiCol_Text, C_DIM);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

static bool card_begin(const char* id, const char* title, const char* sub = nullptr) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    bool open = ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_NavFlattened);
    ImGui::PopStyleVar();
    s_cardTitleY = ImGui::GetCursorPosY();
    ImGui::PushStyleColor(ImGuiCol_Text, C_GOLD_HI);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    if (sub) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 190.f);
        ImGui::PushFont(g_small);
        ImGui::PushStyleColor(ImGuiCol_Text, C_DIM);
        ImGui::TextWrapped("%s", sub);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::PopTextWrapPos();
    }
    return open;
}

static void card_end() {
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 2));
}

static bool primary(const char* label, bool enabled = true, float w = 0) {
    if (!enabled) ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, C_GOLD_LO);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, C_GOLD);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, C_GOLD_HI);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.07f, 0.06f, 0.05f, 1.f));
    bool r = ImGui::Button(label, ImVec2(w, 0));
    ImGui::PopStyleColor(4);
    if (!enabled) ImGui::EndDisabled();
    return r;
}

static bool toggle(const char* label, bool* v) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetFrameHeight() * 0.78f, w = h * 1.85f;
    float y = p.y + (ImGui::GetFrameHeight() - h) * 0.5f;
    bool changed = false;
    if (ImGui::InvisibleButton(label, ImVec2(w + 12 + ImGui::CalcTextSize(label, nullptr, true).x, ImGui::GetFrameHeight()), ImGuiButtonFlags_EnableNav)) {
        *v = !*v;
        changed = true;
    }
    ImGuiStorage* st = ImGui::GetStateStorage();
    ImGuiID id = ImGui::GetItemID();
    float t = st->GetFloat(id, *v ? 1.f : 0.f);
    t += ((*v ? 1.f : 0.f) - t) * std::min(1.f, ImGui::GetIO().DeltaTime * 14.f);
    st->SetFloat(id, t);
    ImVec4 off = ImVec4(0.20f, 0.18f, 0.15f, 1.f);
    ImVec4 bg = ImLerp(off, C_GOLD_LO, t);
    if (ImGui::IsItemHovered() || ImGui::IsItemFocused()) bg = ImLerp(bg, C_GOLD, 0.25f);
    dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w, y + h), ImGui::GetColorU32(bg), h * 0.5f);
    float r = h * 0.5f - 3;
    dl->AddCircleFilled(ImVec2(p.x + h * 0.5f + t * (w - h), y + h * 0.5f), r, ImGui::GetColorU32(ImLerp(C_DIM, C_GOLD_HI, t)));
    const char* end = ImGui::FindRenderedTextEnd(label);
    dl->AddText(ImVec2(p.x + w + 12, p.y + (ImGui::GetFrameHeight() - ImGui::GetFontSize()) * 0.5f), ImGui::GetColorU32(C_TEXT), label, end);
    return changed;
}

static bool mult_slider(const char* label, float* v) {
    ImGui::PushItemWidth(-1);
    char fmt[32];
    snprintf(fmt, sizeof(fmt), "%s  x%%.2f", label);
    bool c = ImGui::SliderFloat((std::string("##") + label).c_str(), v, 1.f, 3.f, fmt, ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopItemWidth();
    return c;
}

static void status_row(const char* name, bool ok, const char* detail = nullptr) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(name);
    ImGui::TableNextColumn();
    pill(ok ? "OK" : "Introuvable", ok ? C_OK : C_BAD);
    ImGui::TableNextColumn();
    if (detail) {
        ImGui::PushStyleColor(ImGuiCol_Text, C_DIM);
        ImGui::TextUnformatted(detail);
        ImGui::PopStyleColor();
    }
}

static void tab_boss(const feat::Status& st) {
    card_begin("greyoll", "Dragon ancien Greyoll", "Butte des dragons de Greyoll  \xC2\xB7  flags 1050400800 / 1050407800 / 1050400599");
    right_pill(st.greyoll == 1 ? "Vaincu" : st.greyoll == 0 ? "En vie" : "Inconnu",
               st.greyoll == 1 ? C_WARN : st.greyoll == 0 ? C_OK : C_DIM);
    ImGui::Dummy(ImVec2(0, 2));
    if (toggle("R\xC3\xA9" "apparition infinie", &g_cfg.greyollRespawn)) save();
    ImGui::PushItemWidth(-1);
    if (!g_cfg.greyollRespawn) ImGui::BeginDisabled();
    ImGui::SliderFloat("##delay", &g_cfg.respawnDelay, 3.f, 60.f, "R\xC3\xA9" "apparition %.0f s apr\xC3\xA8s sa mort");
    if (ImGui::IsItemDeactivatedAfterEdit()) save();
    if (!g_cfg.greyollRespawn) ImGui::EndDisabled();
    ImGui::PopItemWidth();
    if (primary("Faire r\xC3\xA9" "appara\xC3\xAEtre maintenant", st.flags)) feat::post(feat::Act::ResetGreyoll);
    hint("Remet les 3 flags de Greyoll \xC3\xA0 z\xC3\xA9ro puis te t\xC3\xA9l\xC3\xA9porte \xC3\xA0 la gr\xC3\xA2" "ce du Fort Faroth pour recharger la zone : le dragon revient couch\xC3\xA9, avec sa barre de vie et son butin.");
    card_end();
}

static void patch_toggle(const char* label, bool* v, bool found) {
    if (!found) ImGui::BeginDisabled();
    if (toggle(label, v)) save();
    if (!found) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        pill("Signature introuvable", C_BAD);
    }
}

static void tab_world(const feat::Status& st) {
    card_begin("travel", "D\xC3\xA9placements", "Carte et voyage rapide m\xC3\xAAme avec des ennemis proches, Torrent dans les donjons, t\xC3\xA9l\xC3\xA9portation depuis les mini-donjons.");
    patch_toggle("Carte et voyage rapide en combat", &g_cfg.mapCombat, st.pMap);
    patch_toggle("Torrent partout (donjons)", &g_cfg.rideAnywhere, st.pRide);
    if (toggle("Voyage rapide depuis les mini-donjons", &g_cfg.dungeonWarp)) save();
    card_end();

    card_begin("map", "Carte", "R\xC3\xA9v\xC3\xA8le les fragments de carte. D\xC3\xA9sactiver les cache et fait r\xC3\xA9" "appara\xC3\xAEtre les fragments au pied des st\xC3\xA8les (recharge la zone pour les voir).");
    bool mb = st.mapBase, md = st.mapDlc;
    if (toggle("Carte du jeu de base (surface + souterrain)", &mb)) feat::post(feat::Act::MapBase, mb ? 1 : 0);
    if (toggle("Carte du DLC (Royaume de l'Ombre)", &md)) feat::post(feat::Act::MapDlc, md ? 1 : 0);
    card_end();

    card_begin("graces", "Sites de gr\xC3\xA2" "ce", "Active les sites de gr\xC3\xA2" "ce pour la t\xC3\xA9l\xC3\xA9portation. D\xC3\xA9sactiver les reverrouille tous : recharge ensuite la zone (menu principal puis recharger) pour pouvoir les red\xC3\xA9" "couvrir.");
    bool gb = st.gracesBase, gd = st.gracesDlc;
    if (toggle("Gr\xC3\xA2" "ces du jeu de base (314)", &gb)) feat::post(feat::Act::GracesBase, gb ? 1 : 0);
    if (toggle("Gr\xC3\xA2" "ces du DLC (105)", &gd)) feat::post(feat::Act::GracesDlc, gd ? 1 : 0);
    hint("Fais une copie de ta sauvegarde avant : ces changements sont enregistr\xC3\xA9s dans ta partie.");
    card_end();

    card_begin("bells", "Table ronde  \xC2\xB7  S\xC5\x93urs jumelles", "Les 9 cloches de mineur (pierres de forge [1-4] + pierres sombres [1-5]), comme si tu les avais donn\xC3\xA9" "es.");
    if (primary("D\xC3\xA9" "bloquer la vente des pierres de forge", st.flags)) feat::post(feat::Act::UnlockBells);
    if (ImGui::Button("Mettre toutes les pierres de forge \xC3\xA0 99", ImVec2(-1, 0))) feat::post(feat::Act::GiveStones);
    hint("Pierres de forge [1-8], sombres [1-9] et les deux pierres de dragon ancien (pour +25 et +10).");
    card_end();
}

static char g_filter[64] = "";

static void tab_weapons(const feat::Status& st) {
    feat::g_wantWeapons = true;
    card_begin("dupinfo", "Duplication d'armes", "Seulement les armes pr\xC3\xA9sentes dans ton inventaire, 3 exemplaires maximum par arme (m\xC3\xAAme niveau, m\xC3\xAAme affinit\xC3\xA9).");
    if (!st.itemGib) pill("Fonction d'obtention introuvable", C_BAD);
    ImGui::PushItemWidth(-110);
    ImGui::InputTextWithHint("##f", "Rechercher une arme\xE2\x80\xA6", g_filter, sizeof(g_filter));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Actualiser", ImVec2(-1, 0))) feat::post(feat::Act::RefreshInv);
    card_end();

    auto rows = feat::weapons();
    if (rows.empty()) {
        hint(st.player ? "Lecture de l'inventaire\xE2\x80\xA6" : "Charge ta partie pour voir ton inventaire.");
        return;
    }
    std::string f = g_filter;
    for (auto& ch : f) ch = (char)tolower((unsigned char)ch);
    ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY | ImGuiTableFlags_PadOuterX;
    if (ImGui::BeginTable("weps", 3, tf, ImVec2(0, ImGui::GetContentRegionAvail().y - 4))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Arme", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Exemplaires", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableHeadersRow();
        rs_scroll();
        for (auto& r : rows) {
            if (!f.empty()) {
                std::string n = r.name;
                for (auto& ch : n) ch = (char)tolower((unsigned char)ch);
                if (n.find(f) == std::string::npos) continue;
            }
            ImGui::PushID((int)r.id);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(r.name.c_str());
            ImGui::TableNextColumn();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            float cy = p.y + ImGui::GetFrameHeight() * 0.5f;
            for (int i = 0; i < 3; ++i) {
                ImVec2 c(p.x + 10 + i * 20, cy);
                if (i < r.count) dl->AddCircleFilled(c, 5.5f, ImGui::GetColorU32(r.count >= 3 ? C_WARN : C_GOLD));
                else dl->AddCircle(c, 5.5f, ImGui::GetColorU32(C_LINE), 0, 1.5f);
            }
            ImGui::Dummy(ImVec2(70, ImGui::GetFrameHeight()));
            ImGui::TableNextColumn();
            bool can = r.count < 3 && st.itemGib;
            if (primary(r.count >= 3 ? "Max" : "Dupliquer", can, -1)) feat::post(feat::Act::DupWeapon, r.id);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

static void tab_menus(const feat::Status& st) {
    auto& menus = feat::game_menus();
    if (!st.menus) pill("Fonction des menus du jeu introuvable", C_BAD);
    ImGui::Dummy(ImVec2(0, 4));
    const char* group = nullptr;
    int col = 0;
    float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    for (size_t i = 0; i < menus.size(); ++i) {
        auto& m = menus[i];
        if (!group || strcmp(group, m.group) != 0) {
            if (group) ImGui::Dummy(ImVec2(0, 4));
            group = m.group;
            col = 0;
            ImGui::PushStyleColor(ImGuiCol_Text, C_GOLD_HI);
            ImGui::TextUnformatted(m.group);
            ImGui::PopStyleColor();
        }
        if (col == 1) ImGui::SameLine();
        ImGui::PushID((int)i);
        bool first = i == 0 || i == 4;
        bool clicked = first ? primary(m.label, st.menus && st.player, w) : (([&] {
            if (!(st.menus && st.player)) ImGui::BeginDisabled();
            bool r = ImGui::Button(m.label, ImVec2(w, 0));
            if (!(st.menus && st.player)) ImGui::EndDisabled();
            return r;
        })());
        if (clicked) {
            feat::post(feat::Act::OpenMenu, (uint32_t)i);
            feat::g_menuOpen = false;
        }
        ImGui::PopID();
        col ^= 1;
    }
    ImGui::Dummy(ImVec2(0, 4));
    hint("Le panneau se ferme et le menu du jeu s'ouvre, comme \xC3\xA0 un site de gr\xC3\xA2" "ce. La renaissance demande une Larme larvaire, comme chez Rennala.");
    ImGui::Dummy(ImVec2(0, 4));
    card_begin("dupe", "Duplication de cendres gratuite", "Dupliquer une cendre de guerre ne consomme plus de Cendres de guerre perdues.");
    right_pill(st.dupeRows > 0 ? (st.dupeFree ? "Gratuit" : "Co\xC3\xBBt normal") : "Liste introuvable", st.dupeRows > 0 ? (st.dupeFree ? C_OK : C_DIM) : C_BAD);
    if (toggle("Duplication gratuite", &g_cfg.freeAshDupe)) save();
    card_end();
}

static void tab_builds(const feat::Status& st) {
    static char name[48] = "";
    auto list = builds::list();
    card_begin("bsave", "Enregistrer", "Sauvegarde tes stats, ton armure (casque, plastron, gants, jambi\xC3\xA8res) et tes sorts m\xC3\xA9moris\xC3\xA9s.");
    right_pill(st.buildsReady ? "Pr\xC3\xAAt" : "Fonctions du jeu introuvables", st.buildsReady ? C_OK : C_BAD);
    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("##bn", "Nom du build (optionnel)", name, sizeof(name));
    ImGui::PopItemWidth();
    if (primary("Enregistrer le build actuel", st.player)) {
        feat::build_save(name, -1);
        name[0] = 0;
    }
    if (toggle("Restaurer aussi les stats au chargement", &g_cfg.buildStats)) save();
    hint("Au chargement, seules les pi\xC3\xA8" "ces et sorts pr\xC3\xA9sents dans ton inventaire sont \xC3\xA9quip\xC3\xA9s : rien n'est cr\xC3\xA9\xC3\xA9. Les armes, talismans et objets ne sont pas touch\xC3\xA9s.");
    card_end();

    if (list.empty()) hint("Aucun build enregistr\xC3\xA9.");
    static const char* sn[8] = {"VIG", "ESP", "END", "FOR", "DEX", "INT", "FOI", "ARC"};
    for (size_t i = 0; i < list.size(); ++i) {
        auto& b = list[i];
        ImGui::PushID((int)i);
        char sub[256], lv[32];
        int o = 0;
        for (int k = 0; k < 8; ++k) o += snprintf(sub + o, sizeof(sub) - o, "%s%s %d", k ? "  " : "", sn[k], b.stats[k]);
        snprintf(lv, sizeof(lv), "Niveau %d", b.level());
        card_begin("b", b.name.c_str(), sub);
        right_pill(lv, C_DIM);
        if (!b.summary.empty()) hint(b.summary.c_str());
        float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.f;
        if (primary("Charger", st.player && st.buildsReady && !st.buildBusy, w)) feat::post(feat::Act::BuildLoad, (uint32_t)i);
        ImGui::SameLine();
        if (!st.player) ImGui::BeginDisabled();
        if (ImGui::Button("\xC3\x89" "craser", ImVec2(w, 0))) feat::build_save(b.name, (int)i);
        if (!st.player) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Supprimer", ImVec2(w, 0))) feat::post(feat::Act::BuildDelete, (uint32_t)i);
        card_end();
        ImGui::PopID();
    }
}

static std::string look_label(int cat) {
    static const char* slot[4] = {"Casque", "Plastron", "Gants", "Jambi\xC3\xA8res"};
    static const uint32_t bare[4] = {10000, 10100, 10200, 10300};
    uint32_t id = g_cfg.look[cat];
    std::string n = id == 0 ? "R\xC3\xA9" "el" : id == bare[cat] ? "Invisible" : looks::piece_name(id);
    if (n.empty()) n = "#" + std::to_string(id);
    if (n.size() > 30) n = n.substr(0, 28) + "\xE2\x80\xA6";
    return std::string(slot[cat]) + "\n" + n;
}

static void tab_cosmetic(const feat::Status& st) {
    static int pickCat = -1;
    static char filter[48] = "";
    static const uint32_t bare[4] = {10000, 10100, 10200, 10300};
    int ls = looks::state();
    card_begin("look", "Apparence de l'armure", "Change seulement ce qu'on voit : tes stats, ton poids et ton inventaire restent ceux de l'armure r\xC3\xA9" "ellement port\xC3\xA9" "e. L'apparence est remise automatiquement \xC3\xA0 chaque lancement.");
    right_pill(ls == 1 ? (g_cfg.lookOn ? "Actif" : "Inactif") : ls == 0 ? "Initialisation" : "Indisponible sur cette version", ls == 1 ? (g_cfg.lookOn ? C_OK : C_DIM) : ls == 0 ? C_WARN : C_BAD);
    if (toggle("Apparence personnalis\xC3\xA9" "e", &g_cfg.lookOn)) save();
    ImGui::Dummy(ImVec2(0, 6));
    float W = ImGui::GetContentRegionAvail().x, H = 400;
    ImVec2 o = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float cx = o.x + W * 0.5f;
    auto col = [&](int cat) { return ImGui::GetColorU32(g_cfg.lookOn && g_cfg.look[cat] ? C_GOLD : C_GOLD_LO); };
    ImU32 line = ImGui::GetColorU32(C_GOLD_HI);
    dl->AddCircleFilled(ImVec2(cx, o.y + 92), 27, col(0), 32);
    dl->AddCircle(ImVec2(cx, o.y + 92), 27, line, 32, 1.5f);
    dl->AddRectFilled(ImVec2(cx - 40, o.y + 126), ImVec2(cx + 40, o.y + 226), col(1), 10);
    dl->AddRect(ImVec2(cx - 40, o.y + 126), ImVec2(cx + 40, o.y + 226), line, 10, 0, 1.5f);
    for (int sgn : {-1, 1}) {
        float x0 = cx + sgn * 46, x1 = cx + sgn * 68;
        dl->AddRectFilled(ImVec2(std::min(x0, x1), o.y + 130), ImVec2(std::max(x0, x1), o.y + 222), col(2), 8);
        dl->AddRect(ImVec2(std::min(x0, x1), o.y + 130), ImVec2(std::max(x0, x1), o.y + 222), line, 8, 0, 1.5f);
        float l0 = cx + sgn * 5, l1 = cx + sgn * 37;
        dl->AddRectFilled(ImVec2(std::min(l0, l1), o.y + 232), ImVec2(std::max(l0, l1), o.y + 338), col(3), 8);
        dl->AddRect(ImVec2(std::min(l0, l1), o.y + 232), ImVec2(std::max(l0, l1), o.y + 338), line, 8, 0, 1.5f);
    }
    float bw = std::min(230.f, (W - 170.f) * 0.5f);
    if (bw < 120.f) bw = 120.f;
    ImVec2 pos[4] = {ImVec2(cx - bw * 0.5f, o.y + 4), ImVec2(cx - 84 - bw, o.y + 150), ImVec2(cx + 84, o.y + 150), ImVec2(cx - bw * 0.5f, o.y + 348)};
    for (int cat = 0; cat < 4; ++cat) {
        ImGui::SetCursorScreenPos(pos[cat]);
        ImGui::PushID(cat);
        if (ls != 1) ImGui::BeginDisabled();
        if (ImGui::Button(look_label(cat).c_str(), ImVec2(bw, 48))) {
            pickCat = cat;
            filter[0] = 0;
            ImGui::OpenPopup("pick");
        }
        if (ls != 1) ImGui::EndDisabled();
        ImGui::SetNextWindowSize(ImVec2(460, 500));
        if (ImGui::BeginPopup("pick")) {
            ImGui::PushItemWidth(-1);
            ImGui::InputTextWithHint("##pf", "Rechercher une pi\xC3\xA8" "ce\xE2\x80\xA6", filter, sizeof(filter));
            ImGui::PopItemWidth();
            auto choose = [&](uint32_t id) {
                g_cfg.look[cat] = id;
                if (id) g_cfg.lookOn = true;
                save();
                ImGui::CloseCurrentPopup();
            };
            if (ImGui::Selectable("R\xC3\xA9" "el (pas de changement)")) choose(0);
            if (ImGui::Selectable("Invisible")) choose(bare[cat]);
            ImGui::Separator();
            ImGui::BeginChild("plist", ImVec2(0, 0));
            if (!looks::catalog_ready()) hint("Liste des armures en cours de chargement\xE2\x80\xA6");
            else {
                std::string f = filter;
                for (auto& ch : f) ch = (char)tolower((unsigned char)ch);
                for (auto& p : looks::catalog(cat)) {
                    if (!f.empty()) {
                        std::string n = p.name;
                        for (auto& ch : n) ch = (char)tolower((unsigned char)ch);
                        if (n.find(f) == std::string::npos) continue;
                    }
                    ImGui::PushID((int)p.id);
                    if (ImGui::Selectable(p.name.c_str(), g_cfg.look[cat] == p.id)) choose(p.id);
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(o);
    ImGui::Dummy(ImVec2(W, H));
    hint("Le dessin est un sch\xC3\xA9ma, pas ton vrai personnage : le r\xC3\xA9sultat se voit directement en jeu. Tant que c'est actif, les transformations du DLC (formes de dragon...) peuvent ne plus s'afficher.");
    card_end();

    card_begin("lsets", "Tenues enregistr\xC3\xA9" "es", "Enregistre l'apparence actuelle sous un nom pour la remettre en un clic.");
    static char name[48] = "";
    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("##ln", "Nom de la tenue (optionnel)", name, sizeof(name));
    ImGui::PopItemWidth();
    if (primary("Enregistrer la tenue actuelle", true)) {
        looks::add_set(name);
        name[0] = 0;
    }
    auto sets = looks::sets();
    for (size_t i = 0; i < sets.size(); ++i) {
        ImGui::PushID((int)i);
        ImGui::Separator();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(sets[i].name.c_str());
        float w = 120;
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, ImGui::GetContentRegionAvail().x - w * 2 - ImGui::GetStyle().ItemSpacing.x));
        if (ImGui::Button("Charger", ImVec2(w, 0))) looks::load_set(i);
        ImGui::SameLine();
        if (ImGui::Button("Supprimer", ImVec2(w, 0))) looks::remove_set(i);
        ImGui::PopID();
    }
    card_end();
    (void)st;
}

static void tab_torrent(const feat::Status& st) {
    card_begin("tor", "Vitesse de Torrent", "Un seul multiplicateur pour toutes les allures de Torrent (marche, trot, galop). Limit\xC3\xA9 \xC3\xA0 x3.");
    right_pill(!st.player ? "Hors partie" : st.mounted ? "\xC3\x80 cheval" : "\xC3\x80 pied", st.mounted ? C_OK : C_DIM);
    if (toggle("Activer", &g_cfg.torrentSpeed)) save();
    ImGui::Dummy(ImVec2(0, 2));
    mult_slider("Vitesse", &g_cfg.torrentMult);
    if (ImGui::IsItemDeactivatedAfterEdit()) save();
    card_end();

    card_begin("torfly", "Vol", "Sur Torrent : maintiens Saut (F / A) pour voler, maintiens encore pour atterrir. Torrent regarde l\xC3\xA0 o\xC3\xB9 regarde la cam\xC3\xA9ra : la cam\xC3\xA9ra choisit la direction, le stick de d\xC3\xA9placement sert seulement \xC3\xA0 avancer ou reculer.");
    right_pill(st.flying ? "En vol" : "Au sol", st.flying ? C_OK : C_DIM);
    if (toggle("Vol sur Torrent", &g_cfg.torrentFly)) save();
    hint("Vitesse : course normale de Torrent (x le multiplicateur de vitesse s'il est activ\xC3\xA9).");
    card_end();

    card_begin("torjump", "Saut infini", "Torrent peut sauter \xC3\xA0 nouveau en l'air, sans limite.");
    patch_toggle("Saut infini sur Torrent", &g_cfg.torrentInfJump, st.pJump);
    hint("Sauter pendant une chute annule la vitesse de chute.");
    card_end();

    card_begin("torjh", "Hauteur de saut", "Multiplie la hauteur des sauts de Torrent. Ind\xC3\xA9pendant du saut infini.");
    if (toggle("Hauteur de saut augment\xC3\xA9" "e", &g_cfg.jumpHeightOn)) save();
    ImGui::PushItemWidth(-1);
    ImGui::SliderFloat("##jh", &g_cfg.jumpHeight, 1.f, 10.f, "Hauteur de saut  x%.1f");
    if (ImGui::IsItemDeactivatedAfterEdit()) save();
    ImGui::PopItemWidth();
    card_end();

}

static void tab_player(const feat::Status& st) {
    card_begin("fall", "D\xC3\xA9g\xC3\xA2ts de chute", "Aucun d\xC3\xA9g\xC3\xA2t de chute, \xC3\xA0 pied et sur Torrent, sans limite de hauteur.");
    right_pill(st.fallProtect ? "En chute" : "Pr\xC3\xAAt", st.fallProtect ? C_WARN : C_DIM);
    if (toggle("Aucun d\xC3\xA9g\xC3\xA2t de chute", &g_cfg.noFallDamage)) save();
    hint("Le minuteur de chute du jeu est remis \xC3\xA0 z\xC3\xA9ro en continu, le sol est trait\xC3\xA9 comme sans d\xC3\xA9g\xC3\xA2t de chute, et le personnage (et Torrent) est intouchable pendant la chute et l'atterrissage.");
    card_end();

    card_begin("cast", "Cast rapide des buffs", "Acc\xC3\xA9l\xC3\xA8re uniquement les buffs : sorts / incantations de buff, cendres de guerre de buff (hurlements...) et la Fiole merveilleuse. Les attaques gardent leur vitesse normale.");
    char cb[64];
    snprintf(cb, sizeof(cb), "%d cendre(s) buff", st.castAttack);
    right_pill(st.casting ? "Cast en cours" : cb, st.casting ? C_WARN : C_DIM);
    if (toggle("Cast rapide", &g_cfg.fastCast)) save();
    ImGui::PushItemWidth(-1);
    ImGui::SliderFloat("##cs", &g_cfg.castSpeed, 2.f, 10.f, "Vitesse du cast  x%.1f");
    if (ImGui::IsItemDeactivatedAfterEdit()) save();
    ImGui::PopItemWidth();
    hint("Sorts : type lu dans les donn\xC3\xA9" "es du jeu. Cendres de guerre : au 1er usage, si un effet durable appara\xC3\xAEt sur toi c'est un buff. Rapide d\xC3\xA8s le 2e lancement.");
    if (ImGui::Button("Oublier les sorts appris")) feat::post(feat::Act::ResetCast);
    card_end();

    card_begin("runes", "Runes \xC3\xA0 la mort", "Tu gardes tes runes quand tu meurs. La tache de sang ne contient rien : pas de duplication.");
    char buf[64];
    snprintf(buf, sizeof(buf), "%u runes", st.runes);
    right_pill(buf, C_GOLD);
    if (toggle("Garder les runes \xC3\xA0 la mort", &g_cfg.keepRunes)) save();
    if (st.runesPending) {
        snprintf(buf, sizeof(buf), "%u runes en attente de restitution\xE2\x80\xA6", st.runesSaved);
        hint(buf);
    }
    card_end();
}

static void tab_plus(const feat::Status& st) {
    card_begin("flail", "Fl\xC3\xA9" "au : rotation acc\xC3\xA9l\xC3\xA9r\xC3\xA9" "e", "Uniquement avec un fl\xC3\xA9" "au en main droite : rotation beaucoup plus rapide (plus de coups par seconde) sans changer ta vitesse de marche, et aucune consommation de PC ni d'endurance. Tout redevient normal d\xC3\xA8s que tu le ranges.");
    right_pill(st.flailActive ? "En rotation" : st.flailHeld ? "Fl\xC3\xA9" "au en main" : "Pas de fl\xC3\xA9" "au", st.flailActive ? C_WARN : st.flailHeld ? C_OK : C_DIM);
    if (toggle("Activer", &g_cfg.flailSpin)) save();
    ImGui::PushItemWidth(-1);
    ImGui::SliderFloat("##fm", &g_cfg.flailMult, 1.f, 20.f, "Vitesse de rotation  x%.1f");
    if (ImGui::IsItemDeactivatedAfterEdit()) save();
    ImGui::PopItemWidth();
    hint("Rotation du Fl\xC3\xA9" "au d'armes reconnue directement ; les autres fl\xC3\xA9" "aux sont appris \xC3\xA0 la 1re utilisation.");
    if (ImGui::Button("Oublier les animations du fl\xC3\xA9" "au")) feat::post(feat::Act::ResetFlail);
    card_end();

    card_begin("bloodboil", "Ar\xC3\xB4me de sang bouillant", "Remplit ton inventaire au maximum autoris\xC3\xA9 par le jeu. Rappuie apr\xC3\xA8s en avoir utilis\xC3\xA9 pour recompl\xC3\xA9ter.");
    if (primary("Obtenir le maximum", st.itemGib && st.player)) feat::post(feat::Act::GiveBloodboil);
    card_end();

    card_begin("weight", "Charge d'\xC3\xA9quipement", "Augmente ton poids d'\xC3\xA9quipement maximal (roulade l\xC3\xA9g\xC3\xA8re avec des armures lourdes).");
    char b[64];
    snprintf(b, sizeof(b), "%.1f max", st.equipLoad);
    right_pill(b, C_GOLD);
    ImGui::PushItemWidth(-1);
    ImGui::SliderFloat("##wm", &g_cfg.weightMult, 1.f, 10.f, "Multiplicateur  x%.1f");
    if (ImGui::IsItemDeactivatedAfterEdit()) save();
    ImGui::PopItemWidth();
    hint(st.equipLoadBase > 0 ? "Appliqu\xC3\xA9 comme un vrai effet de jeu (m\xC3\xAAme m\xC3\xA9" "canisme que le talisman de la Jarre g\xC3\xA9" "ante)." : "Effet du talisman de la Jarre g\xC3\xA9" "ante introuvable pour l'instant (charge ta partie).");
    card_end();
}

static void tab_diag(const feat::Status& st) {
    if (ImGui::BeginTable("diag", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 130);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        char b[64];
        {
            char db[48];
            snprintf(db, sizeof(db), "%d", st.deaths);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Nombre de morts");
            ImGui::TableNextColumn();
            pill(st.deaths >= 0 ? db : "?", C_GOLD);
            ImGui::TableNextColumn();
        }
        status_row("Signatures de base", st.core);
        status_row("Event flags", st.flags);
        snprintf(b, sizeof(b), "%zu tables", st.paramCount);
        status_row("Params", st.params, b);
        snprintf(b, sizeof(b), "WorldChrMan+0x%X", st.playerOff < 0 ? 0 : st.playerOff);
        status_row("Personnage", st.player, st.playerOff >= 0 ? b : "charge ta partie");
        status_row("ItemGib", st.itemGib);
        status_row("Menus du jeu", st.menus);
        status_row("T\xC3\xA9l\xC3\xA9portation", st.warp);
        status_row("Patch carte en combat", st.pMap);
        status_row("Patch Torrent partout", st.pRide);
        status_row("Patch Torrent zones sp\xC3\xA9" "ciales", st.pRide2);
        status_row("Patch Torrent sur objets (monde ouvert)", st.pRide3);
        char rg[48];
        snprintf(rg, sizeof(rg), "%d acc\xC3\xA8s rep\xC3\xA9r\xC3\xA9s", st.rideGuardHits);
        status_row("Garde anti-disparition de Torrent", st.rideGuard, st.rideGuard ? rg : "d\xC3\xA9sactiv\xC3\xA9" "e (plus n\xC3\xA9" "cessaire)");
        status_row("Patch saut infini", st.pJump);
        char gm[64];
        snprintf(gm, sizeof(gm), "droite %d  \xC2\xB7  gauche %d", (int)st.gemR, (int)st.gemL);
        status_row("Cendres en main", st.gemR != 0xFFFFFFFF, gm);
        status_row("Duplication de cendres", st.dupeRows > 0);
        status_row("Builds (fonctions d'\xC3\xA9quipement)", st.buildsReady);
        status_row("Cosm\xC3\xA9tique", looks::state() == 1, looks::state() == 0 ? "initialisation" : nullptr);
        status_row("Noms d'armes", st.names);
        status_row("Torrent", st.torrentFound, st.mounted ? "mont\xC3\xA9" : "appelle Torrent pour le d\xC3\xA9tecter");
        ImGui::EndTable();
    }
    if (ImGui::Button("Copier le log dans le presse-papiers", ImVec2(-1, 0))) {
        std::string all = "EldenFree " EF_VERSION "\n";
        for (auto& l : log_tail()) all += l + "\n";
        ImGui::SetClipboardText(all.c_str());
    }
    hint("Journal (EldenFree.log dans %LOCALAPPDATA%\\EldenFree) :");
    ImGui::PushFont(g_small);
    ImGui::BeginChild("logtail", ImVec2(0, 0), ImGuiChildFlags_Borders);
    for (auto& l : log_tail()) ImGui::TextWrapped("%s", l.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4) ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
    ImGui::PopFont();
}

bool draw_menu_button() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 sz(300, 52);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.075f, io.DisplaySize.y * 0.885f));
    ImGui::SetNextWindowSize(sz);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##efbtn", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton("##open", sz, ImGuiButtonFlags_EnableNav);
    bool hov = ImGui::IsItemHovered();
    ImU32 l = ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, hov ? 0.55f : 0.35f));
    ImU32 r = ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, 0.f));
    dl->AddRectFilledMultiColor(p, ImVec2(p.x + sz.x, p.y + sz.y), l, r, r, l);
    dl->AddLine(ImVec2(p.x, p.y + sz.y - 1), ImVec2(p.x + sz.x * 0.8f, p.y + sz.y - 1), ImGui::GetColorU32(ImVec4(0.6f, 0.52f, 0.36f, 0.45f)));
    ImGui::PushFont(g_title);
    dl->AddText(ImVec2(p.x + 14, p.y + (sz.y - ImGui::GetFontSize()) * 0.5f), ImGui::GetColorU32(hov ? C_GOLD_HI : C_TEXT), "EldenFree");
    ImGui::PopFont();
    ImGui::PushFont(g_small);
    dl->AddText(ImVec2(p.x + 170, p.y + (sz.y - ImGui::GetFontSize()) * 0.5f), ImGui::GetColorU32(C_DIM), "R3 / clic");
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(2);
    return clicked;
}

void draw() {
    ImGuiIO& io = ImGui::GetIO();
    static bool wasPad = false;
    bool padNow = overlay::pad_mode();
    if (padNow && !wasPad) {
        ImGui::SetNextWindowFocus();
        ImGui::SetNavCursorVisible(true);
    }
    wasPad = padNow;
    feat::Status st = feat::status();
    feat::g_wantWeapons = false;
    ImVec2 size(std::clamp(io.DisplaySize.x * 0.55f, 900.f, 1150.f), std::clamp(io.DisplaySize.y * 0.78f, 640.f, 1000.f));
    if (size.x > io.DisplaySize.x * 0.96f) size.x = io.DisplaySize.x * 0.96f;
    if (size.y > io.DisplaySize.y * 0.94f) size.y = io.DisplaySize.y * 0.94f;
    ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(480, 420), ImVec2(1800, 1400));
    ImGui::Begin("EldenFree", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
    dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + ws.x, wp.y + 90), ImGui::GetColorU32(ImVec4(0.16f, 0.12f, 0.06f, 0.55f)),
                                ImGui::GetColorU32(ImVec4(0.10f, 0.08f, 0.05f, 0.25f)), ImGui::GetColorU32(ImVec4(0, 0, 0, 0)),
                                ImGui::GetColorU32(ImVec4(0, 0, 0, 0)));

    ImGui::PushFont(g_title);
    ImGui::PushStyleColor(ImGuiCol_Text, C_GOLD_HI);
    ImGui::TextUnformatted("EldenFree");
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextColored(C_DIM, EF_VERSION);
    inline_right_pill(st.player ? "En jeu" : st.core ? "En attente de la partie" : "Initialisation", st.player ? C_OK : C_WARN);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 8);
    hint("F10 ou maintenir Menu (manette) pour ouvrir/fermer  \xC2\xB7  LB/RB : onglets  \xC2\xB7  A : valider  \xC2\xB7  B : fermer");
    ImVec2 lp = ImGui::GetCursorScreenPos();
    dl->AddLine(ImVec2(lp.x, lp.y), ImVec2(lp.x + ImGui::GetContentRegionAvail().x, lp.y), ImGui::GetColorU32(C_GOLD_LO), 1.f);
    ImGui::Dummy(ImVec2(0, 4));

    float footer = 34;
    ImGui::BeginChild("body", ImVec2(0, -footer), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
    static int curTab = 0, wantTab = -1;
    const int tabCount = 10;
    if (!ImGui::IsKeyDown(ImGuiKey_GamepadBack)) {
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false)) wantTab = (curTab + tabCount - 1) % tabCount;
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)) wantTab = (curTab + 1) % tabCount;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false) && !ImGui::IsAnyItemActive()) {
        feat::g_menuOpen = false;
        if (!feat::g_fromGameMenu) overlay::block_inputs_until_release();
    }
    int tabIdx = 0;
    if (ImGui::BeginTabBar("tabs")) {
        auto tab = [&](const char* name, void (*fn)(const feat::Status&)) {
            int me = tabIdx++;
            ImGuiTabItemFlags fl = (wantTab == me) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(name, nullptr, fl)) {
                curTab = me;
                ImGui::Dummy(ImVec2(0, 4));
                ImGui::BeginChild("scroll", ImVec2(0, 0), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
                if (me != 3) rs_scroll();
                fn(st);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
        };
        tab("Boss", tab_boss);
        tab("Monde", tab_world);
        tab("Menus", tab_menus);
        tab("Armes", tab_weapons);
        tab("Builds", tab_builds);
        tab("Cosm\xC3\xA9tique", tab_cosmetic);
        tab("Torrent", tab_torrent);
        tab("Joueur", tab_player);
        tab("Plus", tab_plus);
        tab("Diagnostic", tab_diag);
        ImGui::EndTabBar();
        wantTab = -1;
    }
    ImGui::EndChild();

    double age = feat::now() - st.msgTime;
    if (!st.msg.empty() && age < 8.0) {
        float a = age > 6.0 ? (float)(1.0 - (age - 6.0) / 2.0) : 1.f;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, a);
        ImGui::PushStyleColor(ImGuiCol_Text, C_GOLD_HI);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("\xE2\x97\x86");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextUnformatted(st.msg.c_str());
        ImGui::PopStyleVar();
    }
    ImGui::End();
}

}
