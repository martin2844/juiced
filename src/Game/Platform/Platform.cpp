#include "Game/Platform/Platform.h"

#include "Audio/SoundSystem.h"

#include "Core/Data/Language.h"
#include "Core/Base/Log.h"
#include "Game/Platform/CommandLine.h"

#include <windows.h>
#include <d3d9.h>

#include <array>
#include <cstring>

namespace oj {

namespace {

constexpr char kClassName[] = "jet";

// State shared with the window procedure.
struct WindowState {
    HWND gameWindow = nullptr;
    bool active = false;             // WM_ACTIVATEAPP
    bool closedFromSystemMenu = false;
    bool screenshotRequested = false;
    std::vector<input::KeyEvent> keys;
    std::array<bool, 256> down{};
    HCURSOR savedCursor = nullptr;   // the cursor hidden while the game is active
    bool cursorHidden = false;
};
WindowState g_state;
bool g_freeCameraHook = false;  // [0x0075EC84]
FreeCameraMouse g_freeCameraMouse;

void hideCursor() {
    if (g_state.cursorHidden) {
        return;
    }
    g_state.savedCursor = GetCursor();
    g_state.cursorHidden = true;
    SetCursor(nullptr);
    ShowCursor(FALSE);
}

void restoreCursor() {
    if (!g_state.cursorHidden) {
        return;
    }
    ShowCursor(TRUE);
    SetCursor(g_state.savedCursor);
    g_state.savedCursor = nullptr;
    g_state.cursorHidden = false;
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (g_freeCameraHook) {
        FreeCameraMouse& m = g_freeCameraMouse;
        switch (message) {
        case WM_MOUSEMOVE: {
            const auto x = static_cast<int32_t>(lparam & 0xFFFF);
            m.moveX = m.x - x;
            m.moveY = m.y - x;
            m.y = static_cast<int32_t>((static_cast<uint32_t>(lparam) >> 16) & 0xFFFF);
            m.x = x;
            break;
        }
        case WM_LBUTTONDOWN: m.forward = 1; break;
        case WM_LBUTTONUP: m.forward = 0; break;
        case WM_RBUTTONDOWN: m.back = 1; break;
        case WM_RBUTTONUP: m.back = 0; break;
        default: break;
        }
    }
    switch (message) {
    case WM_CLOSE:
        if (hwnd == g_state.gameWindow) {
            PostQuitMessage(0);
            return 0;
        }
        break;
    case WM_ACTIVATEAPP:
        g_state.active = wparam != 0;
        oj::audio::setActive(g_state.active);  // [0x0075F780]
        if (g_state.active) {
            hideCursor();
        } else {
            restoreCursor();
            // Keys held while the game loses the focus are let go.
            for (size_t code = 0; code < g_state.down.size(); ++code) {
                if (g_state.down[code]) {
                    g_state.down[code] = false;
                    g_state.keys.push_back({static_cast<uint8_t>(code), false});
                }
            }
        }
        break;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
        if ((wparam & 0xFF) == VK_SNAPSHOT && message != WM_SYSKEYUP) {
            g_state.screenshotRequested = true;
            return 0;
        }
        // The DirectInput keyboard code: the scan code, + 0x80 for the extended keys.
        const auto code = static_cast<uint8_t>(((lparam >> 16) & 0x7F) | ((lparam & (1 << 24)) != 0 ? 0x80 : 0));
        const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        // A release always passes (only a held key's repeats are dropped): DirectInput, acquired again with the focus,
        // reports the release of a key held across it — Alt after Alt+Tab, whose press the keyboard device took
        // before the focus went (held, it zeroes every mapped key, 0x00594D95).
        if (code != 0 && (g_state.down[code] != down || !down)) {
            g_state.down[code] = down;
            g_state.keys.push_back({code, down});
        }
        break;
    }
    case WM_SYSCOMMAND:
        switch (wparam & 0xFFF0) {
        case SC_KEYMENU:
            if (lparam == VK_RETURN) {  // Alt+Enter
                ShowWindow(hwnd, SW_MINIMIZE);
                return 0;
            }
            break;
        case SC_CLOSE:
            g_state.closedFromSystemMenu = true;
            PostQuitMessage(0);
            return 0;
        case SC_SCREENSAVE:
        case SC_HOTKEY:
        case SC_MONITORPOWER:
            return 0;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

void bringForward(HWND window) {
    EnableWindow(window, TRUE);
    SetFocus(window);
    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);
}

// Work area of the monitor of the settings' adapter, for centering the window.
struct MonitorArea {
    bool found = false;
    RECT work{};
};
MonitorArea g_monitor;

}  // namespace

void hookFreeCameraMouse(bool on) {
    g_freeCameraHook = on;
}

FreeCameraMouse& freeCameraMouse() {
    return g_freeCameraMouse;
}

namespace {

Language g_startupLanguage;

}  // namespace

Language chooseLanguage(int32_t settingsLanguage, const std::string& configLanguage) {
    const uint32_t langId = settingsLanguage >= 0 ? static_cast<uint32_t>(settingsLanguage) : GetUserDefaultLangID();
    Language language = *Language::byIndex(languageOfLangId(langId));
    if (!configLanguage.empty()) {
        if (const auto chosen = Language::byName(configLanguage)) {
            language = *chosen;
        } else {
            log::warning("unknown language '{}'", configLanguage);
        }
    }
    return language;
}

void setStartupLanguage(const Language& language) {
    g_startupLanguage = language;
}

std::wstring startupText(int index) {
    // Read again at each call while it has not read (the stock game asks while [0x0075F0B8] is 0).
    static TextTable jet;
    static bool loaded = false;
    if (!loaded) {
        loaded = jet.load(std::filesystem::path("scripts") /
                          ("jettext" + std::string(1, g_startupLanguage.controlsLetter) + ".txt"));
    }
    if (!loaded) index = 4;
    switch (index) {
        case 0:
        case 1:
        case 2:
        case 3: {
            const size_t line = 114 + static_cast<size_t>(index);
            return line < jet.size() ? jet.at(line) : std::wstring();
        }
        case 4:
            return L"Please reinstall Juiced";  // [0x00725794]
        default:
            return std::wstring();
    }
}

void errorBox(const std::wstring& text, const std::wstring& caption) {
    MessageBoxW(GetActiveWindow(), text.c_str(), caption.c_str(), MB_ICONHAND | MB_SETFOREGROUND | MB_TOPMOST);
}

Platform::~Platform() {
    restoreCursor();
    if (m_window != nullptr) {
        DestroyWindow(m_window);
    }
    if (m_classRegistered) {
        UnregisterClassA(kClassName, GetModuleHandleA(nullptr));
    }
    if (m_instanceMutex != nullptr) {
        CloseHandle(m_instanceMutex);
    }
}

// One pass over the adapters: the one whose DeviceIdentifier equals the "Adapter" setting.
bool Platform::findAdapter(int attempt, const LaunchOptions& options, bool& matched) {
    matched = false;
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (d3d == nullptr) {
        log::error("Direct3DCreate9 failed");
        return false;
    }
    const auto id = readAdapterId();
    if (id) {
        const UINT count = d3d->GetAdapterCount();
        for (UINT i = 0; i < count; ++i) {
            D3DADAPTER_IDENTIFIER9 adapter{};
            if (FAILED(d3d->GetAdapterIdentifier(i, 0, &adapter)) ||
                std::memcmp(&adapter.DeviceIdentifier, id->data(), id->size()) != 0) {
                continue;
            }
            matched = true;
            MONITORINFO info{sizeof(MONITORINFO)};
            if (GetMonitorInfoA(d3d->GetAdapterMonitor(i), &info)) {
                g_monitor.found = true;
                g_monitor.work = info.rcWork;
            }
            log::info("adapter {}: {}", i, adapter.Description);
        }
    }
    d3d->Release();
    if (matched) {
        return true;
    }
    log::warning("no adapter of the settings (attempt {})", attempt);
    if (options.test) {
        // Test runs start no configuration tool: the first adapter stands in for the one of the settings.
        IDirect3D9* first = Direct3DCreate9(D3D_SDK_VERSION);
        if (first != nullptr) {
            MONITORINFO info{sizeof(MONITORINFO)};
            if (GetMonitorInfoA(first->GetAdapterMonitor(0), &info)) {
                g_monitor.found = true;
                g_monitor.work = info.rcWork;
            }
            first->Release();
        }
        matched = true;
        return true;
    }
    if (attempt >= 2) {
        return false;
    }
    runConfigurationTool();
    return true;
}

// <title>Config.exe from the current directory.
bool runConfigurationTool() {
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    char commandLine[1] = "";
    if (!CreateProcessA("JuicedConfig.exe", commandLine, nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr, nullptr,
                        &si, &pi)) {
        MessageBoxW(nullptr, startupText(3).c_str(), L"Error", MB_OK);
        return false;
    }
    DWORD code = STILL_ACTIVE;
    do {
        GetExitCodeProcess(pi.hProcess, &code);
        Sleep(1000);
    } while (code == STILL_ACTIVE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

// quiet (test runs): no single instance check, so that a test runs beside a game the user started.
bool Platform::registerClass(bool quiet) {
    if (quiet) {
        return registerWindowClass();
    }
    if (HWND setup = FindWindowA("setup.exe", nullptr); setup != nullptr) {
        if (!quiet) {
            bringForward(setup);
        }
        return false;
    }
    HWND existing = FindWindowA(kClassName, nullptr);
    HANDLE mutex = CreateMutexA(nullptr, FALSE, "Global\\JuicedTheGame");
    if (mutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        mutex = nullptr;
    }
    if (mutex == nullptr || existing != nullptr) {
        if (mutex != nullptr) {
            CloseHandle(mutex);
        }
        if (quiet) {
        } else if (existing != nullptr) {
            bringForward(existing);
        } else {
            MessageBoxW(nullptr, startupText(1).c_str(), L"Juiced", MB_SETFOREGROUND | MB_TOPMOST);
        }
        log::info("another instance is running");
        return false;
    }
    m_instanceMutex = mutex;
    return registerWindowClass();
}

bool Platform::registerWindowClass() {
    const HINSTANCE instance = GetModuleHandleA(nullptr);
    WNDCLASSA wc{};
    wc.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = &windowProc;
    wc.hInstance = instance;
    wc.hIcon = static_cast<HICON>(LoadImageA(instance, "game.ico", IMAGE_ICON, 64, 64, LR_LOADFROMFILE));
    if (wc.hIcon == nullptr) {
        wc.hIcon = LoadIconA(instance, "AppIcon");
    }
    wc.hCursor = LoadCursorA(nullptr, MAKEINTRESOURCEA(32512));  // IDC_ARROW
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszMenuName = "AppMenu";
    wc.lpszClassName = kClassName;
    m_classRegistered = RegisterClassA(&wc) != 0;
    return m_classRegistered;
}

bool Platform::createWindow(const char* title, const Settings& settings, bool hidden) {
    const bool fullScreen = settings.windowed == 0;
    const DWORD style = fullScreen ? WS_POPUP : (WS_CAPTION | WS_SYSMENU);
    RECT rect{0, 0, static_cast<LONG>(settings.width), static_cast<LONG>(settings.height)};
    AdjustWindowRect(&rect, style, FALSE);
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    if (g_monitor.found) {
        x = (g_monitor.work.right - rect.right + g_monitor.work.left + rect.left) / 2;
        y = (g_monitor.work.bottom - rect.bottom + g_monitor.work.top + rect.top) / 2;
    }
    m_window = CreateWindowExA(WS_EX_APPWINDOW, kClassName, title, style, x, y, rect.right - rect.left,
                               rect.bottom - rect.top, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (m_window == nullptr) {
        return false;
    }
    g_state.gameWindow = m_window;
    if (!hidden) {
        ShowWindow(m_window, SW_SHOWNORMAL);
        UpdateWindow(m_window);
    }
    return true;
}

bool Platform::start(const char* title, const LaunchOptions& options, Settings& settings) {
    bool matched = false;
    for (int attempt = 1; !matched; ++attempt) {
        if (!findAdapter(attempt, options, matched)) {
            return false;
        }
    }
    readSettings(settings);
    if (options.test) {
        settings.windowed = 1;
    }
    log::info("settings: {}x{} {} bit, {}, antialiasing {}, shaders {}, language {}, threading {}", settings.width,
              settings.height, settings.depth, settings.windowed ? "windowed" : "full screen",
              settings.antialiasing, settings.shaders, settings.language, settings.threading);
    if (!registerClass(options.test)) {
        return false;
    }
    return createWindow(title, settings, options.test);
}

bool Platform::pumpMessages() {
    MSG msg{};
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_quit = true;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return !m_quit;
}

bool Platform::active() const {
    return g_state.active;
}

bool Platform::closedFromSystemMenu() const {
    return g_state.closedFromSystemMenu;
}

std::vector<input::KeyEvent> Platform::takeKeyEvents() {
    std::vector<input::KeyEvent> keys;
    keys.swap(g_state.keys);
    return keys;
}

bool Platform::takeScreenshotRequest() {
    const bool requested = g_state.screenshotRequested;
    g_state.screenshotRequested = false;
    return requested;
}

}  // namespace oj
