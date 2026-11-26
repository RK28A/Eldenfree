#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <cstdio>
#include <string>
#include <vector>

static void say(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}

static void fail(const char* m) {
    say("");
    say("  ERREUR : %s", m);
    say("");
    say("  Appuie sur Entrée pour fermer.");
    getchar();
    ExitProcess(1);
}

static std::wstring data_dir() {
    wchar_t p[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, p);
    std::wstring d = std::wstring(p) + L"\\EldenFree";
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

static bool exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

static std::string narrow(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}

static std::wstring widen(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

static bool extract_dll(const std::wstring& path) {
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(1), (LPCWSTR)RT_RCDATA);
    if (!r) return false;
    HGLOBAL h = LoadResource(nullptr, r);
    DWORD size = SizeofResource(nullptr, r);
    const void* data = LockResource(h);
    if (!data || !size) return false;
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return exists(path);
    DWORD w = 0;
    BOOL ok = WriteFile(f, data, size, &w, nullptr);
    CloseHandle(f);
    return ok && w == size;
}

static DWORD find_process(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe))
        do {
            if (!_wcsicmp(pe.szExeFile, name)) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    return pid;
}

static bool has_module(DWORD pid, const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    bool found = false;
    if (Module32FirstW(snap, &me))
        do {
            if (!_wcsicmp(me.szModule, name)) { found = true; break; }
        } while (Module32NextW(snap, &me));
    CloseHandle(snap);
    return found;
}

static bool inject(DWORD pid, const std::wstring& dll) {
    HANDLE p = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!p) return false;
    size_t bytes = (dll.size() + 1) * sizeof(wchar_t);
    void* mem = VirtualAllocEx(p, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    bool ok = false;
    if (mem && WriteProcessMemory(p, mem, dll.c_str(), bytes, nullptr)) {
        auto ll = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
        HANDLE t = CreateRemoteThread(p, nullptr, 0, ll, mem, 0, nullptr);
        if (t) {
            WaitForSingleObject(t, 15000);
            DWORD code = 0;
            GetExitCodeThread(t, &code);
            ok = code != 0;
            CloseHandle(t);
        }
    }
    if (mem) VirtualFreeEx(p, mem, 0, MEM_RELEASE);
    CloseHandle(p);
    return ok;
}

static std::wstring read_setting(const std::wstring& ini) {
    wchar_t buf[MAX_PATH] = {};
    GetPrivateProfileStringW(L"Launcher", L"GamePath", L"", buf, MAX_PATH, ini.c_str());
    return buf;
}

static std::vector<std::wstring> steam_libraries() {
    std::vector<std::wstring> libs;
    wchar_t sp[MAX_PATH] = {};
    DWORD sz = sizeof(sp);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, nullptr, sp, &sz) != ERROR_SUCCESS) {
        sz = sizeof(sp);
        RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath", RRF_RT_REG_SZ, nullptr, sp, &sz);
    }
    if (!sp[0]) return libs;
    std::wstring steam = sp;
    for (auto& c : steam)
        if (c == L'/') c = L'\\';
    libs.push_back(steam);
    FILE* f = _wfopen((steam + L"\\steamapps\\libraryfolders.vdf").c_str(), L"rb");
    if (!f) return libs;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        std::string line(buf);
        size_t k = line.find("\"path\"");
        if (k == std::string::npos) continue;
        size_t a = line.find('"', k + 6), b = a == std::string::npos ? a : line.find('"', a + 1);
        if (a == std::string::npos || b == std::string::npos) continue;
        std::string v = line.substr(a + 1, b - a - 1);
        std::string out;
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] == '\\' && i + 1 < v.size() && v[i + 1] == '\\') ++i;
            out += v[i];
        }
        libs.push_back(widen(out));
    }
    fclose(f);
    return libs;
}

static std::wstring find_game(const std::wstring& ini) {
    std::wstring saved = read_setting(ini);
    if (!saved.empty() && exists(saved)) return saved;
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring dir(self);
    dir = dir.substr(0, dir.find_last_of(L"\\/"));
    for (const wchar_t* rel : {L"\\eldenring.exe", L"\\Game\\eldenring.exe"})
        if (exists(dir + rel)) return dir + rel;
    for (auto& lib : steam_libraries()) {
        std::wstring p = lib + L"\\steamapps\\common\\ELDEN RING\\Game\\eldenring.exe";
        if (exists(p)) return p;
    }
    say("  Jeu introuvable automatiquement : sélectionne eldenring.exe dans la fenêtre.");
    wchar_t file[MAX_PATH] = L"eldenring.exe";
    OPENFILENAMEW of{};
    of.lStructSize = sizeof(of);
    of.lpstrFilter = L"Elden Ring (eldenring.exe)\0eldenring.exe\0";
    of.lpstrFile = file;
    of.nMaxFile = MAX_PATH;
    of.lpstrTitle = L"Où est eldenring.exe ?";
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&of)) return file;
    return L"";
}

static void ensure_steam() {
    if (find_process(L"steam.exe")) return;
    say("  Steam n'est pas lancé, démarrage de Steam...");
    ShellExecuteW(nullptr, L"open", L"steam://open/main", nullptr, nullptr, SW_SHOWMINNOACTIVE);
    for (int i = 0; i < 60 && !find_process(L"steam.exe"); ++i) Sleep(1000);
    Sleep(8000);
}

static DWORD launch(const std::wstring& exe) {
    std::wstring dir = exe.substr(0, exe.find_last_of(L"\\/"));
    SetEnvironmentVariableW(L"SteamAppId", L"1245620");
    SetEnvironmentVariableW(L"SteamGameId", L"1245620");
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + exe + L"\"";
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) return 0;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return pi.dwProcessId;
}

int wmain() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"EldenFree");
    say("");
    say("  ELDENFREE  ·  lanceur hors ligne");
    say("  ---------------------------------");
    std::wstring dd = data_dir();
    std::wstring dll = dd + L"\\EldenFree.dll";
    std::wstring ini = dd + L"\\launcher.ini";

    DWORD pid = find_process(L"eldenring.exe");
    if (pid && has_module(pid, L"EldenFree.dll")) {
        say("  Le mod est déjà chargé dans le jeu. F10 en jeu.");
        Sleep(3000);
        return 0;
    }
    if (!extract_dll(dll)) fail("impossible d'écrire EldenFree.dll dans %LOCALAPPDATA%\\EldenFree");

    if (!pid) {
        if (find_process(L"start_protected_game.exe")) fail("le jeu tourne avec Easy Anti-Cheat. Ferme-le, puis relance EldenFree.exe.");
        std::wstring game = find_game(ini);
        if (game.empty()) fail("eldenring.exe non trouvé.");
        WritePrivateProfileStringW(L"Launcher", L"GamePath", game.c_str(), ini.c_str());
        say("  Jeu : %s", narrow(game).c_str());
        ensure_steam();
        say("  Lancement d'Elden Ring hors ligne (sans Easy Anti-Cheat)...");
        pid = launch(game);
        if (!pid) fail("impossible de lancer eldenring.exe.");
        for (int i = 0; i < 100 && !has_module(pid, L"kernel32.dll"); ++i) Sleep(100);
        Sleep(1500);
        if (!find_process(L"eldenring.exe")) fail("le jeu s'est fermé tout de suite (Steam doit être ouvert et connecté).");
    } else {
        say("  Jeu déjà lancé (PID %lu), injection directe.", pid);
    }

    say("  Injection du mod...");
    if (!inject(pid, dll)) fail("l'injection a échoué. Lance EldenFree.exe en administrateur si le jeu l'est.");
    say("  OK ! En jeu : F10, ou le bouton EldenFree dans le menu Échap / de grâce.");
    say("  Réglages et journal : %s", narrow(dd).c_str());
    say("  Joue hors ligne : ne te connecte pas en ligne avec le mod.");
    Sleep(5000);
    return 0;
}
