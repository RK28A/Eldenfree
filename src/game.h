#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace game {

struct Addrs {
    uintptr_t gameDataMan = 0;
    uintptr_t worldChrMan = 0;
    uintptr_t eventFlagMan = 0;
    uintptr_t soloParam = 0;
    uintptr_t mapItemMan = 0;
    uintptr_t itemGib = 0;
    uintptr_t msgRepo = 0;
    uintptr_t ezDispatch = 0;
    uintptr_t luaEventMan = 0;
    uintptr_t luaWarp = 0;
    uintptr_t menuMan = 0;
};
extern Addrs g;

bool resolve();

int flag_get(uint32_t id);
bool flag_set(uint32_t id, bool on);

bool params_load();
bool params_ready();
size_t params_count();
std::vector<uintptr_t> param_tables(const char* type);
int param_rows(uintptr_t t);
bool param_row(uintptr_t t, int i, int32_t& id, uintptr_t& data);
uintptr_t param_find(uintptr_t t, int32_t id);

uintptr_t pgd();
uintptr_t player();
int player_offset();
bool is_chr(uintptr_t p);
bool chr_hp(uintptr_t chr, int& hp, int& maxhp);
bool chr_set_hp(uintptr_t chr, int hp);
bool chr_pos(uintptr_t chr, float p[3]);
bool chr_set_xz(uintptr_t chr, float x, float z);
bool chr_set_pos(uintptr_t chr, const float p[3]);
uintptr_t chr_module(uintptr_t chr, int off);
uint32_t chr_id(uintptr_t chr);
bool is_mounted(uintptr_t player);
uintptr_t find_torrent(uintptr_t player);
bool anim_speed(uintptr_t chr, float& v);
bool set_anim_speed(uintptr_t chr, float v);

bool give_item(uint32_t id, int qty);
bool ezstate_event(int id, const std::vector<int>& args);
bool ezstate_ready();

enum PatchId { P_MAP1, P_MAP2, P_RIDE, P_HJUMP1, P_HJUMP2, P_HFALL, P_COUNT };
bool patch_found(int id);
bool patch_set(int id, bool on);
bool patch_on(int id);
int speffect_state_set(int row, bool zero);
int cur_anim(uintptr_t chr);
bool msgs_ready();
std::vector<uint32_t> find_msgs(const std::vector<std::wstring>& texts, bool prefix);
uintptr_t rtti_vtable(const char* mangled);
bool esd_install();
bool esd_ready();
void esd_set_callback(void (*cb)(int));
int selected_spell();
uint32_t grace_entity(uint32_t graceFlag);
bool warp_to(uint32_t bonfireEntity);
uintptr_t singleton(const char* name);
bool menu_ui_states(uint8_t* out, size_t n);
int talk_menu_id();
void speffects(uintptr_t chr, std::vector<std::pair<int, float>>& out);

struct InvItem { uint32_t handle, id, qty; };
bool inventory(std::vector<InvItem>& out);

void names_load();
bool names_ready();
std::string weapon_name(uint32_t id);
bool goods_name(uint32_t id, std::wstring& out);
uint32_t find_goods(const std::vector<std::wstring>& needles);

}
