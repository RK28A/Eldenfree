#include "watch.h"
#include "mem.h"
#include <windows.h>
#include <tlhelp32.h>
#include <atomic>
#include <unordered_map>

namespace watch {

static std::atomic<uintptr_t> s_addr[2], s_vt[2];
static std::atomic<uintptr_t> s_hits[32];
static std::atomic<bool> s_hitW[32];
static std::atomic<unsigned long long> s_writes{0};
static std::atomic<uintptr_t> s_src[16];
static std::atomic<int> s_nsrc{0};
static std::atomic<int> s_nh{0};
static std::atomic<unsigned long long> s_count{0};
static DWORD s_tls = TLS_OUT_OF_INDEXES;
static uintptr_t s_cur[2] = {0, 0};
static std::unordered_map<DWORD, uint64_t> s_applied;
static uint64_t s_gen = 0;
static ULONGLONG s_lastScan = 0;

static void record(uintptr_t rip, bool write) {
    int n = s_nh.load();
    for (int i = 0; i < n && i < 32; ++i)
        if (s_hits[i].load() == rip) {
            if (write) s_hitW[i].store(true);
            return;
        }
    if (n >= 32) return;
    s_hits[n].store(rip);
    s_hitW[n].store(write);
    s_nh.store(n + 1);
}

static void zero_reg8(CONTEXT* c, int reg, bool rex) {
    DWORD64* r64[16] = {&c->Rax, &c->Rcx, &c->Rdx, &c->Rbx, nullptr, &c->Rbp, &c->Rsi, &c->Rdi,
                        &c->R8, &c->R9, &c->R10, &c->R11, &c->R12, &c->R13, &c->R14, &c->R15};
    if (reg >= 4 && reg < 8 && !rex) {
        *r64[reg - 4] &= ~0xFF00ull;
        return;
    }
    if (r64[reg]) *r64[reg] &= ~0xFFull;
}

static DWORD64* reg64(CONTEXT* c, int r) {
    DWORD64* t[16] = {&c->Rax, &c->Rcx, &c->Rdx, &c->Rbx, &c->Rsp, &c->Rbp, &c->Rsi, &c->Rdi,
                      &c->R8, &c->R9, &c->R10, &c->R11, &c->R12, &c->R13, &c->R14, &c->R15};
    return t[r & 15];
}

static void clear_origin(CONTEXT* c, uintptr_t watched) {
    uint8_t p[40];
    if (!mem::read_raw((uintptr_t)c->Rip - 40, p, 40)) return;
    for (int i = 39 - 3; i >= 1; --i) {
        int modrm = -1, rex = 0;
        if (p[i] == 0x8A && (p[i + 1] & 0xC0) == 0x40 && (p[i + 1] & 7) != 4 && p[i + 2] == 0x36) {
            modrm = p[i + 1];
            if ((p[i - 1] & 0xF0) == 0x40) rex = p[i - 1];
        } else if (i >= 2 && p[i - 1] == 0x0F && p[i] == 0xB6 && (p[i + 1] & 0xC0) == 0x40 && (p[i + 1] & 7) != 4 && p[i + 2] == 0x36) {
            modrm = p[i + 1];
            if ((p[i - 2] & 0xF0) == 0x40) rex = p[i - 2];
        }
        if (modrm < 0) continue;
        uintptr_t base = (uintptr_t)*reg64(c, (modrm & 7) | ((rex & 1) ? 8 : 0));
        if (!mem::plausible(base) || base + 0x36 == watched) return;
        uint8_t b = 0;
        if (!mem::read_raw(base + 0x36, &b, 1) || b != 1) return;
        b = 0;
        mem::write_raw(base + 0x36, &b, 1);
        int n = s_nsrc.load();
        if (n < 16) {
            s_src[n].store(base);
            s_nsrc.store(n + 1);
        }
        return;
    }
}

static void zero_source(CONTEXT* c) {
    uint8_t p[8];
    if (!mem::read_raw((uintptr_t)c->Rip - 8, p, 8)) return;
    int modrm = -1, rex = 0;
    if (p[5] == 0x88 && (p[6] & 0xC0) == 0x40 && (p[6] & 7) != 4 && p[7] == 0x36) {
        modrm = p[6];
        if ((p[4] & 0xF0) == 0x40) rex = p[4];
    } else if (p[4] == 0x88 && (p[5] & 0xC7) == 0x44 && p[7] == 0x36) {
        modrm = p[5];
        if ((p[3] & 0xF0) == 0x40) rex = p[3];
    }
    if (modrm < 0) return;
    zero_reg8(c, ((modrm >> 3) & 7) | ((rex & 4) ? 8 : 0), rex != 0);
}

static LONG CALLBACK veh(EXCEPTION_POINTERS* ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT* c = ep->ContextRecord;
    DWORD64 hit = c->Dr6 & 3;
    if (!hit) return EXCEPTION_CONTINUE_SEARCH;
    c->Dr6 = 0;
    s_count.fetch_add(1, std::memory_order_relaxed);
    if (TlsGetValue(s_tls)) return EXCEPTION_CONTINUE_EXECUTION;
    TlsSetValue(s_tls, (LPVOID)1);
    for (int i = 0; i < 2; ++i) {
        if (!(hit & (1ull << i))) continue;
        uintptr_t a = s_addr[i].load(), vt = s_vt[i].load();
        if (!a || a != (i ? c->Dr1 : c->Dr0)) continue;
        uintptr_t first = 0;
        if (!mem::read_raw(a - 0x36, &first, 8) || first != vt) continue;
        uint8_t b = 0;
        bool w = mem::read_raw(a, &b, 1) && b;
        if (w) {
            b = 0;
            mem::write_raw(a, &b, 1);
            zero_source(c);
            clear_origin(c, a);
            s_writes.fetch_add(1, std::memory_order_relaxed);
        }
        record((uintptr_t)c->Rip, w);
    }
    TlsSetValue(s_tls, nullptr);
    return EXCEPTION_CONTINUE_EXECUTION;
}

void init() {
    s_tls = TlsAlloc();
    AddVectoredExceptionHandler(1, veh);
}

static void apply_thread(DWORD tid, uintptr_t a0, uintptr_t a1) {
    HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, tid);
    if (!h) return;
    if (SuspendThread(h) != (DWORD)-1) {
        CONTEXT c;
        memset(&c, 0, sizeof(c));
        c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(h, &c)) {
            DWORD64 d7 = c.Dr7 & ~(0x5ull | (0xFFull << 16));
            if (a0) d7 |= 0x1ull | (3ull << 16);
            if (a1) d7 |= 0x4ull | (3ull << 20);
            c.Dr0 = a0;
            c.Dr1 = a1;
            c.Dr7 = d7;
            c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            SetThreadContext(h, &c);
        }
        ResumeThread(h);
    }
    CloseHandle(h);
}

void set(uintptr_t a0, uintptr_t a1) {
    if (s_tls == TLS_OUT_OF_INDEXES) return;
    uintptr_t a[2] = {a0, a1}, vt[2] = {0, 0};
    for (int i = 0; i < 2; ++i) {
        if (!a[i]) continue;
        if (!mem::read_raw(a[i] - 0x36, &vt[i], 8) || !mem::in_image(vt[i])) a[i] = 0;
    }
    bool changed = a[0] != s_cur[0] || a[1] != s_cur[1];
    ULONGLONG now = GetTickCount64();
    if (!changed && (!(a[0] || a[1]) || now - s_lastScan < 2000)) return;
    s_lastScan = now;
    if (changed) {
        for (int i = 0; i < 2; ++i) {
            s_addr[i].store(0);
            s_vt[i].store(vt[i]);
            s_addr[i].store(a[i]);
            s_cur[i] = a[i];
        }
        ++s_gen;
    }
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    DWORD pid = GetCurrentProcessId(), self = GetCurrentThreadId();
    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    std::unordered_map<DWORD, uint64_t> seen;
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
            auto it = s_applied.find(te.th32ThreadID);
            bool had = it != s_applied.end();
            if (!had && !(a[0] || a[1])) continue;
            if (!had || it->second != s_gen) apply_thread(te.th32ThreadID, a[0], a[1]);
            if (a[0] || a[1]) seen[te.th32ThreadID] = s_gen;
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
    s_applied.swap(seen);
}

bool active() { return s_cur[0] || s_cur[1]; }

unsigned long long count() { return s_count.load(); }

bool hit_write(int i) { return i >= 0 && i < 32 && s_hitW[i].load(); }

unsigned long long writes() { return s_writes.load(); }

int sources(uintptr_t* out, int max) {
    int n = s_nsrc.load();
    if (n > max) n = max;
    for (int i = 0; i < n; ++i) out[i] = s_src[i].load();
    return n;
}

int hits(uintptr_t* out, int max) {
    int n = s_nh.load();
    if (n > max) n = max;
    for (int i = 0; i < n; ++i) out[i] = s_hits[i].load();
    return n;
}

}
