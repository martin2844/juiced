// Integration tests: the fixed build's renderer settings (Game/Platform/Settings readSettings), the registry's values
// JuicedConfigExt's settings program writes, against a key of the test's own (HKEY_CURRENT_USER of the process
// redirected, the real settings not touched).

#include "Framework/Test.h"

#include "Game/Platform/Settings.h"

#include <windows.h>

#include <string>

namespace {

using oj::test::check;

// HKEY_CURRENT_USER redirected to a key of its own while it lives, the key deleted after.
class RedirectedUser {
public:
    RedirectedUser() {
        m_path = L"Software\\OpenJuiceTests\\Registry" + std::to_wstring(GetCurrentProcessId());
        RegCreateKeyExW(HKEY_CURRENT_USER, m_path.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &m_key, nullptr);
        if (m_key != nullptr) RegOverridePredefKey(HKEY_CURRENT_USER, m_key);
    }
    ~RedirectedUser() {
        RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
        if (m_key != nullptr) RegCloseKey(m_key);
        RegDeleteTreeW(HKEY_CURRENT_USER, m_path.c_str());
    }
    bool valid() const { return m_key != nullptr; }

    void dword(const wchar_t* name, DWORD value) {
        HKEY key = game();
        RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
    }
    void text(const wchar_t* name, const std::wstring& value) {
        HKEY key = game();
        RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                       static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
    }

private:
    HKEY game() {
        HKEY key = nullptr;
        RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\THQ\\Juiced", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
        return key;
    }

    std::wstring m_path;
    HKEY m_key = nullptr;
};

}  // namespace

// Without the settings program's values: as the program shows them (the mode by Windowed; no MSAA, 16x anisotropic,
// LOD bias 0, VSync on, no counter).
OJ_INTEGRATION_TEST(RendererSettings, Missing) {
    RedirectedUser user;
    check(user.valid(), "a key of the test's own");
    if (!user.valid()) return;
    user.dword(L"Windowed", 0);
    oj::Settings s;
    oj::readSettings(s);
    check(s.windowed == 0 && s.displayMode == 0 && s.borderlessScale == 0 && s.monitor.empty() && s.refreshRate == 0 &&
              s.msaa == 0 && s.vsync == 1 && s.maxFps == 0 && s.textureFilter == 5 && s.lodBias == 3 && s.showFps == 0,
          "Windowed 0: full screen, the rest as the settings program shows it (16x, LOD bias 0)");
    user.dword(L"Windowed", 1);
    oj::readSettings(s);
    check(s.displayMode == 2, "Windowed 1: the window");
}

// The settings program's values as it writes them; out of their ranges as it reads them (a mode past 2 borderless, a
// scale past 1 kept, MaxFPS at most 1000, a filter past 5 16x, a LOD bias past 4 0).
OJ_INTEGRATION_TEST(RendererSettings, Values) {
    RedirectedUser user;
    if (!user.valid()) return;
    user.dword(L"Windowed", 1);
    user.dword(L"DisplayMode", 1);
    user.dword(L"BorderlessScale", 1);
    user.text(L"Monitor", L"\\\\.\\DISPLAY2");
    user.dword(L"RefreshRate", 144);
    user.dword(L"MSAA", 8);
    user.dword(L"VSync", 0);
    user.dword(L"MaxFPS", 240);
    user.dword(L"TextureFilter", 4);
    user.dword(L"LodBias", 2);
    user.dword(L"ShowFps", 1);
    oj::Settings s;
    oj::readSettings(s);
    check(s.displayMode == 1 && s.borderlessScale == 1 && s.monitor == L"\\\\.\\DISPLAY2" && s.refreshRate == 144 &&
              s.msaa == 8 && s.vsync == 0 && s.maxFps == 240 && s.textureFilter == 4 && s.lodBias == 2 && s.showFps == 1,
          "every value read");
    user.dword(L"DisplayMode", 7);
    user.dword(L"BorderlessScale", 3);
    user.dword(L"MaxFPS", 5000);
    user.dword(L"TextureFilter", 9);
    user.dword(L"LodBias", 9);
    oj::readSettings(s);
    check(s.displayMode == 1 && s.borderlessScale == 0 && s.maxFps == 1000 && s.textureFilter == 5 && s.lodBias == 3,
          "out of range: as the settings program reads them");
}

// The world light and the car paint: missing off; as written (the world light MED, HI or ULTRA); a level past 2 the
// default.
OJ_INTEGRATION_TEST(RendererSettings, WorldLight) {
    RedirectedUser user;
    check(user.valid(), "a key of the test's own");
    if (!user.valid()) return;
    oj::Settings s;
    oj::readSettings(s);
    check(s.worldLight == 0 && s.sky == 0 && s.carPaint == 0 && s.headlights == 0, "missing: off");
    user.dword(L"Headlights", 1);
    user.dword(L"HeatHaze", 1);
    user.dword(L"HeatHazeStrength", 35);
    user.dword(L"CarPaint", 1);
    user.dword(L"WorldLight", 2);
    user.dword(L"Sky", 2);
    oj::readSettings(s);
    check(s.worldLight == 2 && s.sky == 2 && s.carPaint == 1 && s.headlights == 1 && s.heatHaze == 1 &&
              s.heatHazeStrength == 35,
          "the world light ULTRA, the car paint on, the headlights on, the heat haze at 35 %");
    oj::Settings fresh;
    user.dword(L"WorldLight", 3);
    user.dword(L"Sky", 3);
    user.dword(L"HeatHazeStrength", 300);
    oj::readSettings(fresh);
    check(fresh.worldLight == 0 && fresh.sky == 0 && fresh.heatHazeStrength == 50,
          "the world light 3, the sky 3, a former heat haze of 300 %: the defaults (the haze at its meter's end)");
}

// The fixed build's audio flags: music, speech and all the audio off no more from the registry (OpenJuiceConfig has no
// Audio page, the game's options set the volumes); EAX off (0x20) and the rest as written; readSetting a value.
OJ_INTEGRATION_TEST(RendererSettings, AudioFlags) {
    RedirectedUser user;
    check(user.valid(), "a key of the test's own");
    if (!user.valid()) return;
    user.dword(L"audioflags", 0x01 | 0x02 | 0x08 | 0x10 | 0x20);
    oj::Settings s;
    oj::readSettings(s);
    check(s.audioFlags == (0x02 | 0x20), "all the audio, the music and the speech off left out, EAX off kept");
    check(oj::readSetting("audioflags") == 0x3Bu && !oj::readSetting("nothing so named"),
          "readSetting: the registry's value as written, none when it is not there");
}
