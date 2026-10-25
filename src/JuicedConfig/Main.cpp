// OpenJuiceConfig's entry: the stock JuicedConfig.exe's WinMain (0x004053C0).

#include "JuicedConfig/ConfigDialog.h"
#include "JuicedConfig/DirectXVersion.h"
#include "JuicedConfig/resource.h"

#include <windows.h>
#include <commctrl.h>
#include <d3d9.h>

#include <clocale>
#include <iterator>
#include <string>
#ifdef _DEBUG
#include <sstream>
#endif

namespace {

using oj::config::tool;
using oj::config::ToolState;

constexpr char kClassName[] = "jet";  // the game's window class

// 0x004020A0: the class's window procedure; the tool makes no window of it.
LRESULT CALLBACK classWindow(HWND, UINT, WPARAM, LPARAM) { return 0; }

// 0x004020B0: the game's window class "jet" registered as the game registers it (Platform::registerWindowClass), its
// window never made; a second user of the class only counts. With the game's window up the tool does not open: that
// window is brought forward (enabled, focused, shown) instead. Stock problem: a failed registration, or the game
// found, leaves the count raised.
bool registerToolClass() {
    ToolState& t = tool();
    if (t.classUsers++ != 0) return true;
    WNDCLASSA wc{};
    wc.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = &classWindow;
    wc.hInstance = t.instance;
    wc.hIcon = static_cast<HICON>(LoadImageA(t.instance, "game.ico", IMAGE_ICON, 64, 64, LR_LOADFROMFILE));
    if (wc.hIcon == nullptr) wc.hIcon = LoadIconA(t.instance, "AppIcon");
    wc.hCursor = LoadCursorA(nullptr, MAKEINTRESOURCEA(32512));  // IDC_ARROW
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszMenuName = "AppMenu";
    wc.lpszClassName = kClassName;
    if (HWND game = FindWindowA(kClassName, nullptr); game != nullptr) {
        EnableWindow(game, TRUE);
        SetFocus(game);
        ShowWindow(game, SW_SHOWNORMAL);
        UpdateWindow(game);
        return false;
    }
    return RegisterClassA(&wc) != 0;
}

// OpenJuiceConfig's own (the region build fixes its language, [0x00412040]): the language as OpenJuice's game picks
// its own (Game/Platform/Platform.h, chooseLanguage): the registry's `language` (a Windows language id) when it is
// there and not negative, else the user's (GetUserDefaultLangID); [Game] Language of OpenJuice.ini beside the tool
// names one instead.
oj::config::Language pickLanguage() {
    int32_t settings = -1;
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\THQ\\Juiced", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
        DWORD value = 0;
        DWORD size = sizeof value;
        if (RegQueryValueExA(key, "language", nullptr, nullptr, reinterpret_cast<BYTE*>(&value), &size) == 0) {
            settings = static_cast<int32_t>(value);
        }
        RegCloseKey(key);
    }
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring ini(path);
    ini = ini.substr(0, ini.find_last_of(L"\\/") + 1) + L"OpenJuice.ini";
    wchar_t name[32] = {};
    GetPrivateProfileStringW(L"Game", L"Language", L"", name, static_cast<DWORD>(std::size(name)), ini.c_str());
    std::string ascii;
    for (const wchar_t* c = name; *c != L'\0'; ++c) ascii.push_back(*c < 0x80 ? static_cast<char>(*c) : '?');
    return oj::config::chooseLanguage(settings, GetUserDefaultLangID(), ascii);
}

#ifdef _DEBUG
// The debug builds' switch --skip-directx-check (OpenJuice's; the stock reads no command line): the window opens
// whatever the DirectX check finds.
bool skipDirectXCheck(const char* commandLine) {
    std::istringstream words(commandLine != nullptr ? commandLine : "");
    std::string word;
    while (words >> word) {
        if (word == "--skip-directx-check") return true;
    }
    return false;
}
#endif

// DirectX 9.0c or later as DxDiag reports it (DirectXVersion.h: a departure from the stock's check).
bool directX9cOrLater() {
    uint32_t major = 0, minor = 0;
    char letter = 0;
    const HRESULT read = oj::config::directXVersion(&major, &minor, &letter);
    return oj::config::directX9cOrLater(read, major, letter);
}

}  // namespace

// 0x004053C0: the C library's locale Russian; the language picked (OpenJuiceConfig's, pickLanguage); the class
// registered (else nothing more); the tab control's class; the window (DIALOGEX 102) when Direct3D 9 is there and
// DirectX 9.0c or later (directX9cOrLater), else the message that DirectX 9.0c is needed (text 318). The class
// unregistered with its last user. The window and the messages take wide texts (ConfigDialog.h).
// Stock problem: without DirectX 9.0c the interface made is never released, nor the class unregistered (the fixed
// build does both).
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, [[maybe_unused]] LPSTR commandLine, int) {
    std::setlocale(LC_ALL, "Russian");
    ToolState& t = tool();
    t.instance = instance;
    t.language = pickLanguage();
    if (!registerToolClass()) return 0;
    INITCOMMONCONTROLSEX controls{sizeof controls, ICC_TAB_CLASSES};
    InitCommonControlsEx(&controls);
    t.d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (t.d3d != nullptr) {
        t.directX9c = directX9cOrLater();
#ifdef _DEBUG
        if (skipDirectXCheck(commandLine)) t.directX9c = true;
#endif
        if (t.directX9c) {
            DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_CONFIG), nullptr, &oj::config::configDialog, 0);
            t.d3d->Release();
            if (--t.classUsers == 0) UnregisterClassA(kClassName, t.instance);
            return 0;
        }
    }
    MessageBoxW(nullptr, oj::config::toolText(318), L"Juiced", MB_OK);
    // The fixed build lets the interface and the class go here too.
    if (t.d3d != nullptr) t.d3d->Release();
    t.d3d = nullptr;
    if (--t.classUsers == 0) UnregisterClassA(kClassName, t.instance);
    return 0;
}
