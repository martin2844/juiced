#pragma once
// The platform start of the stock game (docs/spec/01-startup.md, "Game object: platform start"): settings and their
// adapter, the configuration tool, a single instance, the window class "jet" and the game window.

#include "Core/Data/Language.h"
#include "Core/Input/Input.h"
#include "Game/Platform/Settings.h"

#include <cstdint>
#include <string>
#include <vector>

struct HWND__;
using HWND = HWND__*;

namespace oj {

struct LaunchOptions;

// The window messages of the mouse the free camera reads (0x004016C0, a hook of the game window's messages that the
// camera's update puts on while its view is 14 and takes off else, [0x0075EC84]): WM_MOUSEMOVE the place ([0x007289B4]
// x, [0x007289B0] y; the moves [0x007289AC] x and [0x007289A8], Stock problem: both from x; read by nothing), the left
// button forward ([0x0075EC7C]), the right back ([0x0075EC80]); the flags stay as they are with the hook off.
struct FreeCameraMouse {
    int32_t x = 0;
    int32_t y = 0;
    int32_t moveX = 0;
    int32_t moveY = 0;
    int32_t forward = 0;
    int32_t back = 0;
};
void hookFreeCameraMouse(bool on);
FreeCameraMouse& freeCameraMouse();

// The game's language as the English build 1.0.1 picks it (0x0059E300): the settings' language id (JuicedConfig's
// `language`) when it is not negative, else the user's (GetUserDefaultLangID), by languageOfLangId; [Game] Language of
// OpenJuice.ini names one instead (an unknown name is logged and passed over).
Language chooseLanguage(int32_t settingsLanguage, const std::string& configLanguage);
// The language of the start's messages (startupText), set as the main function starts.
void setStartupLanguage(const Language& language);

// 0x0059BBE0: a text of the startup's messages: 0 .. 3 the jettext lines 114 .. 117 ([0x0075F0B8] .. [0x0075F0C4],
// stored by 0x004FAC20: "requires virtual memory", "Another user is running Juiced", "Please reinstall Juiced", "Cant
// find JuicedConfig"), 4 "Please reinstall Juiced" ([0x00725794]), another none. Asked before the game's language is
// loaded (0x00418345 index 1 for carmodels.dat, 0x0059C052 1 for another instance, 0x0059C846 3 for JuicedConfig), it
// mounts scripts of the game's folder and reads scripts\jettext<letter>.txt. The reference build reads them in its
// region's language whatever the game's (a constant, 0x0059BC2C); OpenJuice, the region binding removed, in the
// game's (setStartupLanguage). A file that does not read makes every index 4.
std::wstring startupText(int index);

// A message box of an error (MB_ICONHAND | MB_SETFOREGROUND | MB_TOPMOST) over the active window.
void errorBox(const std::wstring& text, const std::wstring& caption);

// The configuration tool the adapter check starts: JuicedConfig.exe of the current directory, waited for; when it
// does not start, startupText(3) ("Cant find JuicedConfig", 0x0059C846) and false.
bool runConfigurationTool();

class Platform {
public:
    Platform() = default;
    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;
    ~Platform();

    // false: the game ends (another instance, no settings after the configuration tool, no window).
    bool start(const char* title, const LaunchOptions& options, Settings& settings);

    HWND window() const { return m_window; }

    // Handles the waiting messages; false once the game was asked to quit.
    bool pumpMessages();

    bool active() const;
    bool closedFromSystemMenu() const;
    // Print Screen was pressed since the last call.
    bool takeScreenshotRequest();
    // The keys that went down or up in the window since the last call, in order (the game reads the keyboard through
    // DirectInput; the scan codes of the window messages are the same codes).
    std::vector<input::KeyEvent> takeKeyEvents();

private:
    bool findAdapter(int attempt, const LaunchOptions& options, bool& matched);
    bool registerClass(bool quiet);
    bool registerWindowClass();
    bool createWindow(const char* title, const Settings& settings, bool hidden);

    HWND m_window = nullptr;
    void* m_instanceMutex = nullptr;
    bool m_classRegistered = false;
    bool m_quit = false;
};

}  // namespace oj
