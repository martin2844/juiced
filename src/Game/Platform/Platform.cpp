#include "Game/Platform/Platform.h"

#include "Audio/SoundSystem.h"

#include "Core/Base/GameFiles.h"
#include "Core/Data/Language.h"
#include "Core/Base/Log.h"
#include "Game/Platform/CommandLine.h"
#include "Game/Platform/Screen.h"

#include <windows.h>
#include <d3d9.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

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
    std::string chars;  // WM_CHAR's printable characters (Platform::takeTypedText)
    std::array<bool, 256> down{};
    HCURSOR savedCursor = nullptr;   // the cursor hidden while the game is active
    bool cursorHidden = false;
    bool configuring = false;        // [0x0075F770]: the controls' mapping window up
    // The fixed build's window without a frame: minimized when it loses the focus, not again within a second of a
    // restore.
    bool borderless = false;
    bool minimized = false;
    DWORD restoreTime = 0;
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
        // 0x0059BDBB / 0x0059BDDE: the cursor left alone while the controls' mapping window is up.
        if (g_state.active) {
            if (!g_state.configuring) hideCursor();
        } else {
            if (!g_state.configuring) restoreCursor();
            // Keys held while the game loses the focus are let go.
            for (size_t code = 0; code < g_state.down.size(); ++code) {
                if (g_state.down[code]) {
                    g_state.down[code] = false;
                    g_state.keys.push_back({static_cast<uint8_t>(code), false});
                }
            }
            // The fixed build's window without a frame: minimized as a full screen game is when another program or
            // Windows itself takes the focus (the Windows key, the taskbar, Alt+Tab); not within a second of a
            // restore, so that a window being brought back stays.
            if (hwnd == g_state.gameWindow && g_state.borderless && !IsIconic(hwnd) &&
                GetTickCount() - g_state.restoreTime > 1000) {
                ShowWindowAsync(hwnd, SW_MINIMIZE);
            }
        }
        break;
    case WM_SIZE:
        if (hwnd == g_state.gameWindow) {
            if (wparam == SIZE_MINIMIZED) {
                g_state.minimized = true;
            } else if (g_state.minimized) {
                g_state.minimized = false;
                g_state.restoreTime = GetTickCount();
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
    case WM_CHAR: {
        const auto c = static_cast<unsigned char>(wparam & 0xFF);
        if (c >= 0x20 && c < 0x7F && g_state.chars.size() < 64) g_state.chars.push_back(static_cast<char>(c));
        break;
    }
    case WM_SYSCOMMAND:
        switch (systemCommand(wparam, lparam)) {
        case SystemCommand::kMinimize:
            ShowWindow(hwnd, SW_MINIMIZE);
            return 0;
        case SystemCommand::kClose:
            g_state.closedFromSystemMenu = true;
            PostQuitMessage(0);
            return 0;
        case SystemCommand::kDrop:
            return 0;
        case SystemCommand::kDefault:
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

// Work area of the monitor of the settings' adapter, for centering the window; the fixed build's window without a
// frame covers the whole monitor (area).
struct MonitorArea {
    bool found = false;
    RECT work{};
    RECT area{};
};
MonitorArea g_monitor;

// The fixed build's screen: the window's and the monitor's sizes in pixels, not scaled by Windows (Windows 10 1703's
// per-monitor awareness, else Vista's).
void becomeDpiAware() {
    HMODULE user = GetModuleHandleA("user32.dll");
    if (user == nullptr) return;
    using SetContext = BOOL(WINAPI*)(HANDLE);
    const auto setContext = reinterpret_cast<SetContext>(GetProcAddress(user, "SetProcessDpiAwarenessContext"));
    if (setContext != nullptr && setContext(reinterpret_cast<HANDLE>(-4))) return;  // PER_MONITOR_AWARE_V2
    using SetAware = BOOL(WINAPI*)();
    const auto setAware = reinterpret_cast<SetAware>(GetProcAddress(user, "SetProcessDPIAware"));
    if (setAware != nullptr) setAware();
}

// The desktop's monitors by their GDI names, the primary first (the order a wrapper's adapters follow).
std::vector<std::wstring> desktopMonitors() {
    std::vector<std::wstring> names;
    DISPLAY_DEVICEW device{};
    device.cb = sizeof(device);
    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &device, 0); ++i) {
        if ((device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) != 0) {
            const bool primary = (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;
            names.insert(primary ? names.begin() : names.end(), device.DeviceName);
        }
        device.cb = sizeof(device);
    }
    return names;
}

// The monitor of the GDI name given.
bool monitorByName(const std::wstring& name, MONITORINFOEXW& info) {
    struct Search {
        const std::wstring* name;
        MONITORINFOEXW* info;
        bool found;
    } search{&name, &info, false};
    EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
            auto* s = reinterpret_cast<Search*>(data);
            MONITORINFOEXW candidate{};
            candidate.cbSize = sizeof(candidate);
            if (GetMonitorInfoW(monitor, &candidate) && _wcsicmp(candidate.szDevice, s->name->c_str()) == 0) {
                *s->info = candidate;
                s->found = true;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.found;
}

}  // namespace

SystemCommand systemCommand(uintptr_t command, intptr_t parameter) {
    switch (command & 0xFFF0) {
    case SC_KEYMENU: return parameter == VK_RETURN ? SystemCommand::kMinimize : SystemCommand::kDrop;
    case SC_CLOSE: return SystemCommand::kClose;
    case SC_SCREENSAVE:
    case SC_HOTKEY:
    case SC_MONITORPOWER: return SystemCommand::kDrop;
    default: return SystemCommand::kDefault;
    }
}

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
        loaded = jet.load(jetTextFile(GameFiles(std::filesystem::current_path()), g_startupLanguage));
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

void errorBox(const std::wstring& text, const std::wstring& caption, bool overDesktop) {
    MessageBoxW(overDesktop ? GetDesktopWindow() : GetActiveWindow(), text.c_str(), caption.c_str(),
                MB_ICONHAND | MB_SETFOREGROUND | MB_TOPMOST);
}

void Platform::beginControlsWindow() {
    ShowWindow(m_window, SW_HIDE);
    pumpMessages();
    restoreCursor();
    g_state.configuring = true;
}

void Platform::endControlsWindow() {
    g_state.configuring = false;
    hideCursor();
    ShowWindow(m_window, SW_SHOW);
    SetActiveWindow(m_window);
    pumpMessages();
    ShowWindow(m_window, SW_SHOWNORMAL);
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
            m_adapter = i;
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

bool runSettingsProgram(const std::wstring& arguments) {
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    const std::filesystem::path folder = std::filesystem::path(self).parent_path();
    // OpenJuiceConfig is 32-bit, for both executables of the game.
    std::vector<std::wstring> names;
#if defined(_DEBUG)
    names.push_back(L"OpenJuiceConfig_d.exe");
#endif
    names.push_back(L"OpenJuiceConfig.exe");
    for (const std::wstring& name : names) {
        const std::filesystem::path program = folder / name;
        std::error_code error;
        if (!std::filesystem::is_regular_file(program, error)) continue;
        std::wstring commandLine = L"\"" + program.wstring() + L"\"" + (arguments.empty() ? L"" : L" " + arguments);
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(program.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
                            folder.c_str(), &si, &pi)) {
            log::warning("{} did not start", program.string());
            return false;
        }
        log::info("{} {}: waited for", program.filename().string(), std::filesystem::path(arguments).string());
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    return false;
}

// <title>Config.exe from the current directory.
bool runConfigurationTool(bool firstStart) {
    if (runSettingsProgram(firstStart ? L"--first-run" : L"")) {
        return true;
    }
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
    // The fixed build's screen modes (JuicedConfigExt's renderer): full screen at the monitor's corner, the window
    // without a frame over the whole monitor, the window with a frame at the resolution (smaller when it does not fit
    // the work area) in the middle of the work area.
    const screen::Mode mode = screen::modeOf(settings.displayMode);
    const DWORD style = mode == screen::Mode::Window ? (WS_CAPTION | WS_SYSMENU) : WS_POPUP;
    RECT rect{0, 0, static_cast<LONG>(settings.width), static_cast<LONG>(settings.height)};
    screen::Size client{static_cast<int32_t>(settings.width), static_cast<int32_t>(settings.height)};
    if (mode == screen::Mode::Window && g_monitor.found && !hidden) {  // a test run: the resolution as it is
        RECT frame{0, 0, 0, 0};
        AdjustWindowRect(&frame, style, FALSE);
        const RECT& work = g_monitor.work;
        client = screen::windowClient(work.right - work.left, work.bottom - work.top, frame.right - frame.left,
                                      frame.bottom - frame.top, settings.width, settings.height);
    }
    if (mode == screen::Mode::Window) {
        rect = {0, 0, client.width, client.height};
        AdjustWindowRect(&rect, style, FALSE);
    } else if (mode == screen::Mode::Borderless && g_monitor.found) {
        rect = {0, 0, g_monitor.area.right - g_monitor.area.left, g_monitor.area.bottom - g_monitor.area.top};
    }
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    if (g_monitor.found) {
        if (mode == screen::Mode::Window) {
            x = std::max(g_monitor.work.left, (g_monitor.work.right - rect.right + g_monitor.work.left + rect.left) / 2);
            y = std::max(g_monitor.work.top, (g_monitor.work.bottom - rect.bottom + g_monitor.work.top + rect.top) / 2);
        } else {
            x = g_monitor.area.left;
            y = g_monitor.area.top;
        }
    }
    m_window = CreateWindowExA(WS_EX_APPWINDOW, kClassName, title, style, x, y, rect.right - rect.left,
                               rect.bottom - rect.top, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (m_window == nullptr) {
        return false;
    }
    if (mode == screen::Mode::Window) {
        // The frame of a monitor of another DPI differs from AdjustWindowRect's: the client corrected.
        RECT real{};
        if (GetClientRect(m_window, &real) && (real.right != client.width || real.bottom != client.height)) {
            SetWindowPos(m_window, nullptr, 0, 0, rect.right - rect.left + client.width - real.right,
                         rect.bottom - rect.top + client.height - real.bottom,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    g_state.gameWindow = m_window;
    g_state.borderless = mode == screen::Mode::Borderless;
    if (!hidden) {
        ShowWindow(m_window, SW_SHOWNORMAL);
        UpdateWindow(m_window);
    }
    return true;
}

void Platform::chooseScreen(Settings& settings) {
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    MONITORINFOEXW monitor{};
    monitor.cbSize = sizeof(monitor);
    bool found = false;
    if (d3d != nullptr) {
        std::vector<std::wstring> reported;
        for (UINT a = 0; a < d3d->GetAdapterCount(); ++a) {
            MONITORINFOEXW info{};
            info.cbSize = sizeof(info);
            HMONITOR handle = d3d->GetAdapterMonitor(a);
            reported.push_back(handle != nullptr && GetMonitorInfoW(handle, &info) ? info.szDevice : L"");
        }
        const uint32_t adapter = screen::monitorAdapter(reported, desktopMonitors(), settings.monitor, m_adapter);
        if (adapter != m_adapter) log::info("screen: adapter {} of the monitor of the settings", adapter);
        m_adapter = adapter;
        HMONITOR handle = d3d->GetAdapterMonitor(m_adapter);
        found = handle != nullptr && GetMonitorInfoW(handle, &monitor);
    }
    // The monitor named, else the adapter's, else the primary.
    if (!settings.monitor.empty() && monitorByName(settings.monitor, monitor)) found = true;
    if (!found) {
        found = GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitor) != 0;
    }
    if (found) {
        g_monitor.found = true;
        g_monitor.work = monitor.rcWork;
        g_monitor.area = monitor.rcMonitor;
    }
    uint32_t refreshRate = 0;
    if (screen::modeOf(settings.displayMode) == screen::Mode::FullScreen && d3d != nullptr) {
        std::vector<screen::DisplayMode> modes;
        for (const D3DFORMAT format : {D3DFMT_X8R8G8B8, D3DFMT_R5G6B5}) {
            for (UINT i = 0; i < d3d->GetAdapterModeCount(m_adapter, format); ++i) {
                D3DDISPLAYMODE mode{};
                if (SUCCEEDED(d3d->EnumAdapterModes(m_adapter, format, i, &mode))) {
                    modes.push_back({mode.Width, mode.Height, mode.RefreshRate});
                }
            }
        }
        refreshRate = screen::fullScreenRate(modes, settings.width, settings.height, settings.refreshRate);
        if (refreshRate == 0) {
            log::warning("screen: the adapter has no full screen mode {}x{}, the window without a frame",
                         settings.width, settings.height);
            settings.displayMode = static_cast<uint32_t>(screen::Mode::Borderless);
        }
    }
    if (d3d != nullptr) d3d->Release();
    settings.windowed = screen::modeOf(settings.displayMode) == screen::Mode::FullScreen ? 0 : 1;
    static constexpr const char* kModes[] = {"full screen", "window without a frame", "window"};
    log::info("screen: {} on {}x{}{}", kModes[static_cast<uint32_t>(screen::modeOf(settings.displayMode))],
              g_monitor.area.right - g_monitor.area.left, g_monitor.area.bottom - g_monitor.area.top,
              refreshRate != 0 ? std::format(", {} Hz", refreshRate) : "");
}

bool Platform::start(const char* title, const LaunchOptions& options, Settings& settings) {
    becomeDpiAware();
    bool matched = false;
    for (int attempt = 1; !matched; ++attempt) {
        if (!findAdapter(attempt, options, matched)) {
            return false;
        }
    }
    readSettings(settings);
    // The command line's audio switches over the registry's flags (-x the music off, -n all the audio).
    settings.audioFlags |= options.audioFlagsOr;
    if (options.audioFlagsOne) settings.audioFlags = 1;
    if (options.test && options.userSettings) {
        settings.windowed = 1;
        settings.displayMode = 2;
        settings.monitor.clear();
    } else if (options.test) {
        settings.windowed = 1;
        // The fixed build's renderer as the stock's: a test run's pictures the same on every machine.
        settings.displayMode = 2;
        settings.monitor.clear();
        settings.msaa = 0;
        settings.smaa = 0;
        settings.vsync = 1;
        settings.maxFps = 0;
        settings.textureFilter = 0;
        settings.lodBias = 0;
        settings.showFps = 0;
        settings.worldLight = 0;
        settings.sky = 0;
        settings.carPaint = 0;
        settings.headlights = 0;
        settings.heatHaze = 0;
        settings.reflections = 0;
        settings.lightFlares = 0;
    }
    if (options.headlights && *options.headlights <= 2) settings.headlights = *options.headlights;
    if (options.worldLight && *options.worldLight <= 2) settings.worldLight = *options.worldLight;
    if (options.sky && *options.sky <= 2) settings.sky = *options.sky;
    if (options.carPaint && *options.carPaint <= 1) settings.carPaint = *options.carPaint;
    if (options.reflections && *options.reflections <= 4) settings.reflections = *options.reflections == 4 ? 1 : 0;
    chooseScreen(settings);
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

std::string Platform::takeTypedText() {
    std::string chars;
    chars.swap(g_state.chars);
    return chars;
}

bool Platform::takeScreenshotRequest() {
    const bool requested = g_state.screenshotRequested;
    g_state.screenshotRequested = false;
    return requested;
}

}  // namespace oj
