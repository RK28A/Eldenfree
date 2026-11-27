#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace builds {

struct Build {
    std::string name, summary;
    int stats[8] = {};
    uint32_t gear[22] = {}, gearH[22] = {};
    uint8_t gearHas[22] = {};
    uint32_t goods[16] = {};
    uint8_t goodsHas[16] = {};
    int spells[14] = {};
    int gems[6] = {-1, -1, -1, -1, -1, -1};
    int level() const {
        int s = 0;
        for (int v : stats) s += v;
        return s - 79;
    }
};

struct Result {
    int equipped = 0, missing = 0, spells = 0, spellsMissing = 0;
    bool failed = false;
};

void load_file();
std::vector<Build> list();
size_t count();
bool get(size_t i, Build& b);
void add(const Build& b);
void overwrite(size_t i, const Build& b);
void remove(size_t i);

bool ready();
bool capture(Build& b);
void apply_gear(const Build& b, bool stats, Result& r);
void apply_rest(const Build& b, Result& r);
void nudge(int step);

}
