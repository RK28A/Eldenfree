#include "mem.h"
#include <vector>
#include <string>

extern "C" int ef_safe_copy(void* dst, const void* src, size_t n);
extern "C" char ef_safe_copy_fault[];
extern "C" char ef_safe_copy_end[];

asm(R"(
.intel_syntax noprefix
.text
.globl ef_safe_copy
.globl ef_safe_copy_fault
.globl ef_safe_copy_end
ef_safe_copy:
    push rdi
    push rsi
    mov rdi, rcx
    mov rsi, rdx
    mov rcx, r8
    cld
    rep movsb
    mov eax, 1
    pop rsi
    pop rdi
    ret
ef_safe_copy_fault:
    xor eax, eax
    pop rsi
    pop rdi
    ret
ef_safe_copy_end:
    nop
.att_syntax prefix
)");

namespace mem {

uintptr_t g_base = 0;
uintptr_t g_end = 0;

static LONG CALLBACK veh(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
        ep->ExceptionRecord->ExceptionCode != STATUS_GUARD_PAGE_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    uintptr_t ip = ep->ContextRecord->Rip;
    if (ip >= (uintptr_t)&ef_safe_copy && ip < (uintptr_t)ef_safe_copy_fault) {
        ep->ContextRecord->Rip = (DWORD64)ef_safe_copy_fault;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

bool init() {
    HMODULE h = GetModuleHandleW(nullptr);
    auto dos = (IMAGE_DOS_HEADER*)h;
    auto nt = (IMAGE_NT_HEADERS64*)((uint8_t*)h + dos->e_lfanew);
    g_base = (uintptr_t)h;
    g_end = g_base + nt->OptionalHeader.SizeOfImage;
    AddVectoredExceptionHandler(1, veh);
    return true;
}

bool read_raw(uintptr_t addr, void* out, size_t n) {
    if (addr < 0x10000 || addr + n > 0x7FFFFFFFFFFFull) return false;
    return ef_safe_copy(out, (const void*)addr, n) != 0;
}

bool write_raw(uintptr_t addr, const void* in, size_t n) {
    if (addr < 0x10000 || addr + n > 0x7FFFFFFFFFFFull) return false;
    return ef_safe_copy((void*)addr, in, n) != 0;
}

static bool parse(const char* p, std::vector<int>& out) {
    out.clear();
    while (*p) {
        if (*p == ' ') { ++p; continue; }
        if (*p == '?') { out.push_back(-1); ++p; if (*p == '?') ++p; continue; }
        auto hex = [](char c) { return c >= '0' && c <= '9' ? c - '0' : (c | 32) - 'a' + 10; };
        if (!p[1]) return false;
        out.push_back(hex(p[0]) * 16 + hex(p[1]));
        p += 2;
    }
    return !out.empty();
}

uintptr_t aob(const char* pattern) {
    uintptr_t r = 0;
    return aob_all(pattern, &r, 1) ? r : 0;
}

size_t aob_all(const char* pattern, uintptr_t* out, size_t max) {
    std::vector<int> pat;
    size_t found = 0;
    if (!parse(pattern, pat) || !g_base || !max) return 0;
    size_t n = pat.size();
    uintptr_t a = g_base;
    while (a < g_end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (re > g_end) re = g_end;
        DWORD pr = mbi.Protect & 0xFF;
        bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_GUARD) &&
                        (pr == PAGE_EXECUTE_READ || pr == PAGE_EXECUTE_READWRITE || pr == PAGE_READONLY ||
                         pr == PAGE_READWRITE || pr == PAGE_EXECUTE_WRITECOPY || pr == PAGE_WRITECOPY);
        if (readable && re - rb >= n) {
            const uint8_t* b = (const uint8_t*)rb;
            size_t len = re - rb - n;
            int first = pat[0];
            for (size_t i = 0; i <= len; ++i) {
                if (first >= 0 && b[i] != first) continue;
                size_t j = 1;
                for (; j < n; ++j)
                    if (pat[j] >= 0 && b[i + j] != pat[j]) break;
                if (j == n) {
                    out[found++] = rb + i;
                    if (found >= max) return found;
                }
            }
        }
        a = re;
    }
    return found;
}

}
