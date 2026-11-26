#include <windows.h>
#include "config.h"
#include "mem.h"
#include "features.h"
#include "overlay.h"

static HMODULE g_self;

static BOOL CALLBACK find_wnd(HWND h, LPARAM l) {
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(h)) return TRUE;
    RECT r;
    GetClientRect(h, &r);
    if (r.right < 320 || r.bottom < 200) return TRUE;
    *(HWND*)l = h;
    return FALSE;
}

static DWORD WINAPI boot(void*) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(g_self, path, MAX_PATH);
    std::wstring p(path);
    g_dir = p.substr(0, p.find_last_of(L"\\/") + 1);
    cfg_load();
    cfg_save();
    elog("EldenFree charge");
    mem::init();
    HWND w = nullptr;
    while (!w) {
        EnumWindows(find_wnd, (LPARAM)&w);
        if (!w) Sleep(500);
    }
    Sleep(3000);
    feat::start();
    for (int i = 0; i < 20 && !overlay::install(); ++i) Sleep(1000);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h;
        DisableThreadLibraryCalls(h);
        CreateThread(nullptr, 0, boot, nullptr, 0, nullptr);
    }
    return TRUE;
}
