#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace looks {

struct Piece {
    uint32_t id;
    std::string name;
};

struct Set {
    std::string name;
    uint32_t ids[4];
};

int state();
std::string status_text();
bool catalog_ready();
const std::vector<Piece>& catalog(int cat);
std::string piece_name(uint32_t id);
void tick(double t, uintptr_t pl);

std::vector<Set> sets();
void add_set(const std::string& name);
void load_set(size_t i);
void remove_set(size_t i);

}
