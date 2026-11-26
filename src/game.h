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
    uintptr_t chrDbg = 0;
    uintptr_t mapItemMan = 0;
    uintptr_t itemGib = 0;
    uintptr_t msgRepo = 0;
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
bool mount_geom(const float pp[3], const float tp[3]);
uintptr_t find_mount(uintptr_t player, bool deep);

uintptr_t nodead_addr();

bool give_item(uint32_t id, int qty);

struct InvItem { uint32_t handle, id, qty; };
bool inventory(std::vector<InvItem>& out);

void names_load();
bool names_ready();
std::string weapon_name(uint32_t id);

}
