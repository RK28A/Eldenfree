#pragma once
#include <cstdint>

namespace watch {

void init();
void set(uintptr_t a0, uintptr_t a1);
bool active();
int hits(uintptr_t* out, int max);
unsigned long long count();

}
