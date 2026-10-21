#pragma once
// The stock JuicedConfig.exe's window (DIALOGEX 102, its procedure 0x004049C0) and the tool's state: the settings of
// HKEY_CURRENT_USER\Software\THQ\Juiced the game reads (docs/spec/30-juiced-config.md). The stock's window speaks
// through the ANSI functions, so a language outside the system's code page comes out as question marks; this one
// takes its texts through the wide ones, every language whole on any system (the adapters' names and the modes'
// texts, ANSI, as the stock's).

#include "JuicedConfig/ConfigData.h"

#include <windows.h>
#include <d3d9.h>

#include <cstdint>

namespace oj::config {

// The tool's globals.
struct ToolState {
    HINSTANCE instance = nullptr;              // [0x00412B64]
    IDirect3D9* d3d = nullptr;                 // [0x00412B70]
    Language language = Language::kEnglish;    // [0x00412040] (the region build's 7): picked by WinMain
    uint32_t mode = kDefaultMode;              // [0x00412044]: the mode the list keeps nearest to
    AudioChecks audio;                         // [0x00412048] .. [0x00412058]
    uint32_t audioFlags = kDefaultAudioFlags;  // [0x0041205C]
    int shaderLevel = 0;                       // [0x00412B60]: the last adapter's (0x00401000)
    bool multipleProcessors = false;           // [0x00412B68]
    uint32_t threading = 0;                    // [0x00412B6C]
    bool directX9c = false;                    // [0x00412B74]: DirectX 9.0c or later
    int classUsers = 0;                        // [0x00412B78]: the window class "jet" registered while above 0
    int tab = 0;                               // [0x00412B80]: the tab shown (0 Settings, 1 Troubleshooting)
    int32_t antialiasing = 0;                  // [0x00412B84]: the registry's, selected as the list is made
    uint32_t shaders = 0;                      // [0x00412B88]
    uint32_t widescreen = 0;                   // [0x00412B8C]
    uint32_t multiTurn = 0;                    // [0x00412B90]
    bool tabsMade = false;                     // [0x00413198]
    bool adapterChanged = false;               // [0x0041319C]: written, read by nothing
    wchar_t text[0x200] = {};                  // [0x00412F98]: the last text loaded
};
ToolState& tool();

// The window's procedure (0x004049C0).
INT_PTR CALLBACK configDialog(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

// A text of the tool's language (LoadStringW into the state's buffer); nullptr when it does not load.
const wchar_t* toolText(uint32_t first);

}  // namespace oj::config
