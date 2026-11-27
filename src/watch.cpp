#include "watch.h"
#include "mem.h"
#include <windows.h>
#include <tlhelp32.h>
#include <atomic>
#include <unordered_map>

namespace watch {

static std::atomic<uintptr_t> s_addr[2], s_vt[2];
static std::atomic<uintptr_t> s_hits[32];
static std::atomic<int> s_nh{0};
static std::atomic<unsigned long long> s_count{0};
static DWORD s_tls = TLS_OUT_OF_INDEXES;
static uintptr_t s_cur[2] = {0, 0};
static std::unordered_map<DWORD, uint64_t> s_applied;
static uint64_t s_gen = 0;
static ULONGLONG s_lastScan = 0;

static void record(uintptr_t rip) {
    int n = s_nh.load();
    for (int i = 0; i < n && i < 32; ++i)
        if (s_hits[i].load() == rip) return;
    if (n >= 32) return;
    s_hits[n].store(rip);
    s_nh.store(n + 1);
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
        if (mem::read_raw(a, &b, 1) && b) {
            b = 0;
            mem::write_raw(a, &b, 1);
        }
        record((uintptr_t)c->Rip);
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

int hits(uintptr_t* out, int max) {
    int n = s_nh.load();
    if (n > max) n = max;
    for (int i = 0; i < n; ++i) out[i] = s_hits[i].load();
    return n;
}

}
