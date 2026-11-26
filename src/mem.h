#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>

namespace mem {

extern uintptr_t g_base;
extern uintptr_t g_end;

bool init();
bool read_raw(uintptr_t addr, void* out, size_t n);
bool write_raw(uintptr_t addr, const void* in, size_t n);

inline bool plausible(uintptr_t p) { return p >= 0x10000 && p < 0x7FFFFFFFFFFFull && (p & 7) == 0; }
inline bool in_image(uintptr_t p) { return p >= g_base && p < g_end; }

template <class T> bool rd(uintptr_t a, T& v) { return read_raw(a, &v, sizeof(T)); }
template <class T> T rdv(uintptr_t a, T def = T{}) { T v; return read_raw(a, &v, sizeof(T)) ? v : def; }
template <class T> bool wr(uintptr_t a, const T& v) { return write_raw(a, &v, sizeof(T)); }

inline uintptr_t ptr(uintptr_t a) {
    if (!a) return 0;
    uintptr_t v = 0;
    if (!read_raw(a, &v, 8)) return 0;
    return plausible(v) ? v : 0;
}

uintptr_t aob(const char* pattern);
inline uintptr_t rip(uintptr_t instr, int dispOff, int len) {
    if (!instr) return 0;
    int32_t d = 0;
    if (!rd(instr + dispOff, d)) return 0;
    return instr + len + d;
}

}
