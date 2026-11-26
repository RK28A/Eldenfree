#include "overlay.h"
#include "features.h"
#include "config.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <dinput.h>
#include <xinput.h>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace overlay {

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT);
using Present1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using ResizeFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using Resize1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
using ExecFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
using DIStateFn = HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*, DWORD, LPVOID);
using DIDataFn = HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
using XIStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
using ClipFn = BOOL(WINAPI*)(const RECT*);
using SetPosFn = BOOL(WINAPI*)(int, int);

static PresentFn oPresent;
static Present1Fn oPresent1;
static ResizeFn oResize;
static Resize1Fn oResize1;
static ExecFn oExec;
static std::vector<std::pair<void*, DIStateFn>> oDIState;
static std::vector<std::pair<void*, DIDataFn>> oDIData;
static std::vector<std::pair<void*, XIStateFn>> oXI;
static ClipFn oClip;
static SetPosFn oSetPos;

struct Frame {
    ID3D12CommandAllocator* alloc = nullptr;
    ID3D12Resource* rt = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    UINT64 fence = 0;
};

static std::atomic<ID3D12CommandQueue*> g_queue{nullptr};
static ID3D12Device* g_dev = nullptr;
static ID3D12DescriptorHeap* g_rtvHeap = nullptr;
static ID3D12DescriptorHeap* g_srvHeap = nullptr;
static ID3D12GraphicsCommandList* g_cmd = nullptr;
static ID3D12Fence* g_fence = nullptr;
static HANDLE g_fenceEvent = nullptr;
static UINT64 g_fenceVal = 0;
static std::vector<Frame> g_frames;
static UINT g_rtvCap = 0;
static bool g_init = false;
static HWND g_hwnd = nullptr;
static WNDPROC g_origProc = nullptr;
static std::mutex g_rm;
static thread_local bool tl_in = false;

static std::atomic<long> g_dx{0}, g_dy{0}, g_wheel{0};
static std::atomic<int> g_rawBtn{0}, g_diBtn{0};
static POINT g_lastSys{-99999, -99999};
static ImVec2 g_vpos{0, 0};

HWND hwnd() { return g_hwnd; }

static void wait_gpu() {
    if (!g_fence || !g_queue) return;
    ++g_fenceVal;
    if (SUCCEEDED(g_queue.load()->Signal(g_fence, g_fenceVal)) && g_fence->GetCompletedValue() < g_fenceVal) {
        g_fence->SetEventOnCompletion(g_fenceVal, g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, 2000);
    }
}

static void release_frames() {
    for (auto& f : g_frames) {
        if (f.rt) f.rt->Release();
        if (f.alloc) f.alloc->Release();
    }
    g_frames.clear();
}

static bool create_frames(IDXGISwapChain3* sc) {
    DXGI_SWAP_CHAIN_DESC d{};
    if (FAILED(sc->GetDesc(&d))) return false;
    UINT n = d.BufferCount;
    if (n > g_rtvCap) {
        if (g_rtvHeap) g_rtvHeap->Release();
        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hd.NumDescriptors = n;
        if (FAILED(g_dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&g_rtvHeap)))) { g_rtvCap = 0; return false; }
        g_rtvCap = n;
    }
    UINT inc = g_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE h = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    g_frames.resize(n);
    for (UINT i = 0; i < n; ++i) {
        Frame& f = g_frames[i];
        if (FAILED(g_dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&f.alloc)))) return false;
        if (FAILED(sc->GetBuffer(i, IID_PPV_ARGS(&f.rt)))) return false;
        f.rtv.ptr = h.ptr + (SIZE_T)i * inc;
        g_dev->CreateRenderTargetView(f.rt, nullptr, f.rtv);
    }
    if (!g_cmd) {
        if (FAILED(g_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frames[0].alloc, nullptr, IID_PPV_ARGS(&g_cmd)))) return false;
        g_cmd->Close();
    }
    return true;
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l);
static void install_input_hooks();

static bool init(IDXGISwapChain3* sc) {
    if (FAILED(sc->GetDevice(IID_PPV_ARGS(&g_dev)))) return false;
    DXGI_SWAP_CHAIN_DESC d{};
    if (FAILED(sc->GetDesc(&d))) return false;
    g_hwnd = d.OutputWindow;
    D3D12_DESCRIPTOR_HEAP_DESC sd{};
    sd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    sd.NumDescriptors = 1;
    sd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(g_dev->CreateDescriptorHeap(&sd, IID_PPV_ARGS(&g_srvHeap)))) return false;
    if (FAILED(g_dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)))) return false;
    g_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!create_frames(sc)) return false;

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ui::setup();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX12_Init(g_dev, (int)d.BufferCount, d.BufferDesc.Format, g_srvHeap,
                        g_srvHeap->GetCPUDescriptorHandleForHeapStart(), g_srvHeap->GetGPUDescriptorHandleForHeapStart());
    g_origProc = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)wnd_proc);
    install_input_hooks();
    elog("Overlay initialise (%u buffers, format %d)", d.BufferCount, (int)d.BufferDesc.Format);
    return true;
}

static void toggle_check() {
    static bool prev = false;
    bool down = (GetAsyncKeyState(g_cfg.toggleKey) & 0x8000) != 0;
    bool fg = GetForegroundWindow() == g_hwnd;
    if (down && !prev && fg) {
        bool open = !feat::g_menuOpen;
        feat::g_menuOpen = open;
        if (open) {
            if (oClip) oClip(nullptr);
            RECT rc;
            GetClientRect(g_hwnd, &rc);
            g_vpos = ImVec2((float)(rc.right / 2), (float)(rc.bottom / 2));
            POINT c{rc.right / 2, rc.bottom / 2};
            ClientToScreen(g_hwnd, &c);
            if (oSetPos) oSetPos(c.x, c.y);
            g_lastSys = c;
            g_dx = g_dy = g_wheel = 0;
        }
    }
    prev = down;
}

static void feed_mouse() {
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = true;
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    POINT cp;
    long dx = g_dx.exchange(0), dy = g_dy.exchange(0);
    if (GetCursorPos(&cp) && (cp.x != g_lastSys.x || cp.y != g_lastSys.y)) {
        g_lastSys = cp;
        ScreenToClient(g_hwnd, &cp);
        g_vpos = ImVec2((float)cp.x, (float)cp.y);
    } else {
        g_vpos.x += (float)dx;
        g_vpos.y += (float)dy;
    }
    if (g_vpos.x < 0) g_vpos.x = 0;
    if (g_vpos.y < 0) g_vpos.y = 0;
    if (g_vpos.x > rc.right - 1) g_vpos.x = (float)(rc.right - 1);
    if (g_vpos.y > rc.bottom - 1) g_vpos.y = (float)(rc.bottom - 1);
    io.AddMousePosEvent(g_vpos.x, g_vpos.y);
    int raw = g_rawBtn.load(), di = g_diBtn.load();
    bool b[3] = {(GetAsyncKeyState(VK_LBUTTON) & 0x8000) || (raw & 1) || (di & 1),
                 (GetAsyncKeyState(VK_RBUTTON) & 0x8000) || (raw & 2) || (di & 2),
                 (GetAsyncKeyState(VK_MBUTTON) & 0x8000) || (raw & 4) || (di & 4)};
    for (int i = 0; i < 3; ++i) io.AddMouseButtonEvent(i, b[i]);
    long wh = g_wheel.exchange(0);
    if (wh) io.AddMouseWheelEvent(0, (float)wh / WHEEL_DELTA);
}

static bool queue_ok(ID3D12CommandQueue* q) {
    static ID3D12CommandQueue* verified = nullptr;
    if (q == verified) return true;
    ID3D12Device* qd = nullptr;
    bool ok = SUCCEEDED(q->GetDevice(IID_PPV_ARGS(&qd))) && qd == g_dev;
    if (qd) qd->Release();
    if (ok) verified = q;
    else g_queue = nullptr;
    return ok;
}

static void render(IDXGISwapChain3* sc) {
    ID3D12CommandQueue* q = g_queue.load();
    if (!q) return;
    std::lock_guard<std::mutex> lk(g_rm);
    if (!g_init) {
        if (!init(sc)) return;
        g_init = true;
    }
    if (!queue_ok(q)) return;
    if (g_frames.empty() && !create_frames(sc)) { release_frames(); return; }
    toggle_check();
    if (!feat::g_menuOpen) return;

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    feed_mouse();
    ImGui::NewFrame();
    ui::draw();
    ImGui::Render();

    UINT idx = sc->GetCurrentBackBufferIndex();
    if (idx >= g_frames.size()) return;
    Frame& f = g_frames[idx];
    if (g_fence->GetCompletedValue() < f.fence) {
        g_fence->SetEventOnCompletion(f.fence, g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, 1000);
    }
    f.alloc->Reset();
    g_cmd->Reset(f.alloc, nullptr);
    D3D12_RESOURCE_BARRIER br{};
    br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    br.Transition.pResource = f.rt;
    br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    br.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    br.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_cmd->ResourceBarrier(1, &br);
    g_cmd->OMSetRenderTargets(1, &f.rtv, FALSE, nullptr);
    g_cmd->SetDescriptorHeaps(1, &g_srvHeap);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_cmd);
    br.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    br.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_cmd->ResourceBarrier(1, &br);
    g_cmd->Close();
    ID3D12CommandList* lists[] = {g_cmd};
    oExec(q, 1, lists);
    q->Signal(g_fence, ++g_fenceVal);
    f.fence = g_fenceVal;
}

static HRESULT STDMETHODCALLTYPE hk_present(IDXGISwapChain3* sc, UINT sync, UINT flags) {
    if (!tl_in && !(flags & DXGI_PRESENT_TEST)) {
        tl_in = true;
        render(sc);
        HRESULT hr = oPresent(sc, sync, flags);
        tl_in = false;
        return hr;
    }
    return oPresent(sc, sync, flags);
}

static HRESULT STDMETHODCALLTYPE hk_present1(IDXGISwapChain3* sc, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS* p) {
    if (!tl_in && !(flags & DXGI_PRESENT_TEST)) {
        tl_in = true;
        render(sc);
        HRESULT hr = oPresent1(sc, sync, flags, p);
        tl_in = false;
        return hr;
    }
    return oPresent1(sc, sync, flags, p);
}

static void before_resize() {
    std::lock_guard<std::mutex> lk(g_rm);
    if (!g_init) return;
    wait_gpu();
    release_frames();
}

static HRESULT STDMETHODCALLTYPE hk_resize(IDXGISwapChain3* sc, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT fl) {
    before_resize();
    return oResize(sc, n, w, h, f, fl);
}

static HRESULT STDMETHODCALLTYPE hk_resize1(IDXGISwapChain3* sc, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT fl, const UINT* m, IUnknown* const* q) {
    before_resize();
    return oResize1(sc, n, w, h, f, fl, m, q);
}

static void STDMETHODCALLTYPE hk_exec(ID3D12CommandQueue* q, UINT n, ID3D12CommandList* const* l) {
    if (!g_queue.load()) {
        D3D12_COMMAND_QUEUE_DESC d = q->GetDesc();
        if (d.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            bool ok = true;
            if (g_dev) {
                ID3D12Device* qd = nullptr;
                if (SUCCEEDED(q->GetDevice(IID_PPV_ARGS(&qd)))) {
                    ok = qd == g_dev;
                    qd->Release();
                }
            }
            if (ok) g_queue = q;
        }
    }
    oExec(q, n, l);
}

static std::mutex g_dim;
static std::unordered_map<void*, int> g_diType;

static int di_type(IDirectInputDevice8W* dev) {
    std::lock_guard<std::mutex> lk(g_dim);
    auto it = g_diType.find(dev);
    if (it != g_diType.end()) return it->second;
    DIDEVCAPS caps{};
    caps.dwSize = sizeof(caps);
    int t = 0;
    if (SUCCEEDED(dev->GetCapabilities(&caps))) t = GET_DIDEVICE_TYPE(caps.dwDevType);
    g_diType[dev] = t;
    return t;
}

static DIStateFn di_state_orig(IDirectInputDevice8W* dev) {
    void* target = (*(void***)dev)[9];
    for (auto& p : oDIState)
        if (p.first == target) return p.second;
    return oDIState.empty() ? nullptr : oDIState[0].second;
}

static DIDataFn di_data_orig(IDirectInputDevice8W* dev) {
    void* target = (*(void***)dev)[10];
    for (auto& p : oDIData)
        if (p.first == target) return p.second;
    return oDIData.empty() ? nullptr : oDIData[0].second;
}

static HRESULT STDMETHODCALLTYPE hk_di_state(IDirectInputDevice8W* dev, DWORD cb, LPVOID data) {
    HRESULT hr = di_state_orig(dev)(dev, cb, data);
    if (feat::g_menuOpen && SUCCEEDED(hr) && data) {
        if (di_type(dev) == DI8DEVTYPE_MOUSE && cb >= sizeof(DIMOUSESTATE)) {
            auto* ms = (DIMOUSESTATE*)data;
            g_dx += ms->lX;
            g_dy += ms->lY;
            g_wheel += ms->lZ;
            g_diBtn = ((ms->rgbButtons[0] & 0x80) ? 1 : 0) | ((ms->rgbButtons[1] & 0x80) ? 2 : 0) | ((ms->rgbButtons[2] & 0x80) ? 4 : 0);
        }
        memset(data, 0, cb);
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE hk_di_data(IDirectInputDevice8W* dev, DWORD cbObj, LPDIDEVICEOBJECTDATA rg, LPDWORD inout, DWORD fl) {
    HRESULT hr = di_data_orig(dev)(dev, cbObj, rg, inout, fl);
    if (feat::g_menuOpen && SUCCEEDED(hr) && inout) {
        if (rg && di_type(dev) == DI8DEVTYPE_MOUSE) {
            int btn = g_diBtn;
            for (DWORD i = 0; i < *inout; ++i) {
                auto* e = (DIDEVICEOBJECTDATA*)((uint8_t*)rg + (size_t)i * cbObj);
                switch (e->dwOfs) {
                case DIMOFS_X: g_dx += (LONG)e->dwData; break;
                case DIMOFS_Y: g_dy += (LONG)e->dwData; break;
                case DIMOFS_Z: g_wheel += (LONG)e->dwData; break;
                case DIMOFS_BUTTON0: btn = (e->dwData & 0x80) ? (btn | 1) : (btn & ~1); break;
                case DIMOFS_BUTTON1: btn = (e->dwData & 0x80) ? (btn | 2) : (btn & ~2); break;
                case DIMOFS_BUTTON2: btn = (e->dwData & 0x80) ? (btn | 4) : (btn & ~4); break;
                }
            }
            g_diBtn = btn;
        }
        *inout = 0;
    }
    return hr;
}

template <int N> static DWORD WINAPI hk_xi(DWORD idx, XINPUT_STATE* st) {
    DWORD r = oXI[N].second(idx, st);
    if (feat::g_menuOpen && r == ERROR_SUCCESS && st) memset(&st->Gamepad, 0, sizeof(st->Gamepad));
    return r;
}

static BOOL WINAPI hk_clip(const RECT* r) {
    if (feat::g_menuOpen) return TRUE;
    return oClip(r);
}

static BOOL WINAPI hk_setpos(int x, int y) {
    if (feat::g_menuOpen) return TRUE;
    return oSetPos(x, y);
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (feat::g_menuOpen) {
        switch (m) {
        case WM_INPUT: {
            RAWINPUT ri;
            UINT sz = sizeof(ri);
            if (GetRawInputData((HRAWINPUT)l, RID_INPUT, &ri, &sz, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
                ri.header.dwType == RIM_TYPEMOUSE) {
                auto& mo = ri.data.mouse;
                if (!(mo.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                    g_dx += mo.lLastX;
                    g_dy += mo.lLastY;
                }
                int b = g_rawBtn;
                USHORT f = mo.usButtonFlags;
                if (f & RI_MOUSE_LEFT_BUTTON_DOWN) b |= 1;
                if (f & RI_MOUSE_LEFT_BUTTON_UP) b &= ~1;
                if (f & RI_MOUSE_RIGHT_BUTTON_DOWN) b |= 2;
                if (f & RI_MOUSE_RIGHT_BUTTON_UP) b &= ~2;
                if (f & RI_MOUSE_MIDDLE_BUTTON_DOWN) b |= 4;
                if (f & RI_MOUSE_MIDDLE_BUTTON_UP) b &= ~4;
                if (f & RI_MOUSE_WHEEL) g_wheel += (SHORT)mo.usButtonData;
                g_rawBtn = b;
            }
            return DefWindowProcW(h, m, w, l);
        }
        case WM_MOUSEWHEEL:
            g_wheel += GET_WHEEL_DELTA_WPARAM(w);
            return 0;
        case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK: case WM_MBUTTONDOWN:
        case WM_MBUTTONUP: case WM_MBUTTONDBLCLK: case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        case WM_MOUSEHWHEEL:
            return 0;
        case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: case WM_CHAR:
            if (g_init) ImGui_ImplWin32_WndProcHandler(h, m, w, l);
            return 0;
        case WM_SETCURSOR:
            SetCursor(nullptr);
            return TRUE;
        }
    } else {
        g_rawBtn = 0;
        g_diBtn = 0;
    }
    return CallWindowProcW(g_origProc, h, m, w, l);
}

static void hook_di_device(IDirectInputDevice8W* dev) {
    void** vt = *(void***)dev;
    for (auto& p : oDIState)
        if (p.first == vt[9]) return;
    DIStateFn os = nullptr;
    DIDataFn od = nullptr;
    if (MH_CreateHook(vt[9], (void*)hk_di_state, (void**)&os) == MH_OK) oDIState.push_back({vt[9], os});
    if (MH_CreateHook(vt[10], (void*)hk_di_data, (void**)&od) == MH_OK) oDIData.push_back({vt[10], od});
}

static void install_input_hooks() {
    IDirectInput8W* diw = nullptr;
    if (SUCCEEDED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&diw, nullptr))) {
        IDirectInputDevice8W* kb = nullptr;
        if (SUCCEEDED(diw->CreateDevice(GUID_SysKeyboard, &kb, nullptr))) {
            hook_di_device(kb);
            kb->Release();
        }
        IDirectInputDevice8W* ms = nullptr;
        if (SUCCEEDED(diw->CreateDevice(GUID_SysMouse, &ms, nullptr))) {
            hook_di_device(ms);
            ms->Release();
        }
        diw->Release();
    }
    IDirectInput8A* dia = nullptr;
    if (SUCCEEDED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A, (void**)&dia, nullptr))) {
        IDirectInputDevice8A* kb = nullptr;
        if (SUCCEEDED(dia->CreateDevice(GUID_SysKeyboard, &kb, nullptr))) {
            hook_di_device((IDirectInputDevice8W*)kb);
            kb->Release();
        }
        dia->Release();
    }
    static const wchar_t* xmods[] = {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
    static void* xhooks[] = {(void*)hk_xi<0>, (void*)hk_xi<1>, (void*)hk_xi<2>};
    oXI.reserve(3);
    for (const wchar_t* name : xmods) {
        HMODULE m = GetModuleHandleW(name);
        if (!m) continue;
        void* fn = (void*)GetProcAddress(m, "XInputGetState");
        if (!fn || oXI.size() >= 3) continue;
        XIStateFn o = nullptr;
        oXI.push_back({fn, nullptr});
        if (MH_CreateHook(fn, xhooks[oXI.size() - 1], (void**)&o) == MH_OK) oXI.back().second = o;
        else oXI.pop_back();
    }
    MH_CreateHookApi(L"user32", "ClipCursor", (void*)hk_clip, (void**)&oClip);
    MH_CreateHookApi(L"user32", "SetCursorPos", (void*)hk_setpos, (void**)&oSetPos);
    MH_EnableHook(MH_ALL_HOOKS);
    elog("Hooks d'entree : DInput %zu, XInput %zu", oDIState.size(), oXI.size());
}

bool install() {
    MH_STATUS ms = MH_Initialize();
    if (ms != MH_OK && ms != MH_ERROR_ALREADY_INITIALIZED) return false;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"EldenFreeDummy";
    RegisterClassExW(&wc);
    HWND dummy = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    if (!dummy) return false;

    bool ok = false;
    ID3D12Device* dev = nullptr;
    ID3D12CommandQueue* q = nullptr;
    IDXGIFactory2* fac = nullptr;
    IDXGISwapChain1* sc = nullptr;
    do {
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) break;
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (FAILED(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&q)))) break;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&fac)))) break;
        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width = 64;
        sd.Height = 64;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        if (FAILED(fac->CreateSwapChainForHwnd(q, dummy, &sd, nullptr, nullptr, &sc))) break;
        void** vt = *(void***)sc;
        void** qvt = *(void***)q;
        ok = MH_CreateHook(vt[8], (void*)hk_present, (void**)&oPresent) == MH_OK &&
             MH_CreateHook(vt[13], (void*)hk_resize, (void**)&oResize) == MH_OK &&
             MH_CreateHook(qvt[10], (void*)hk_exec, (void**)&oExec) == MH_OK;
        MH_CreateHook(vt[22], (void*)hk_present1, (void**)&oPresent1);
        MH_CreateHook(vt[39], (void*)hk_resize1, (void**)&oResize1);
    } while (false);
    if (sc) sc->Release();
    if (fac) fac->Release();
    if (q) q->Release();
    if (dev) dev->Release();
    DestroyWindow(dummy);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    if (!ok) return false;
    MH_EnableHook(MH_ALL_HOOKS);
    elog("Hooks DX12 installes");
    return true;
}

}
