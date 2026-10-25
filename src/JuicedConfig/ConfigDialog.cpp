#include "JuicedConfig/ConfigDialog.h"

#include "JuicedConfig/EaxCheck.h"
#include "JuicedConfig/resource.h"

#include <commctrl.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <set>
#include <vector>

namespace oj::config {

namespace {

constexpr char kKey[] = "Software\\THQ\\Juiced";  // [0x0040E1C4]
constexpr wchar_t kTitle[] = L"Juiced Config";     // [0x0040E1D8]

// The texts' first ids (eight each, Language's).
enum TextId : uint32_t {
    kTextSettings = 110,
    kTextTroubleshooting = 118,
    kTextDisplay = 126,
    kTextDisplayAdapter = 134,
    kTextDisplayMode = 142,
    kTextAntialiasing = 150,
    kTextQualityLevel = 158,  // "Quality Level %d"
    kTextWindowed = 166,
    kTextWidescreen = 174,
    kTextOk = 182,
    kTextReset = 190,
    kTextCancel = 198,
    kTextVideo = 206,
    kTextShaders = 214,
    kTextAudio = 222,
    kTextSpeech = 230,
    kTextMusic = 238,
    kTextHardwareMixing = 246,
    kTextAllAudio = 254,
    kTextShader1 = 262,
    kTextShader2 = 270,
    kTextShader3 = 278,
    kTextNoShaders = 286,
    kTextValidate = 294,     // full screen: "Your selection needs to be validated, press OK to proceed."
    kTextNotValidated = 302,  // "Unable to validate your selection. ..."
    kTextValidated = 310,
    kTextInput = 326,
    kTextMultiTurn = 334,
    kTextMultiThreading = 342,
};

// D3DFORMATs the mode lists take.
constexpr D3DFORMAT kFormat32 = D3DFMT_X8R8G8B8;  // 0x16
constexpr D3DFORMAT kFormat16 = D3DFMT_R5G6B5;    // 0x17

LRESULT send(HWND window, int id, UINT message, WPARAM wParam = 0, LPARAM lParam = 0) {
    return SendMessageA(GetDlgItem(window, id), message, wParam, lParam);
}

// A message with a wide text.
LRESULT sendText(HWND window, int id, UINT message, WPARAM wParam, const wchar_t* text) {
    return SendMessageW(GetDlgItem(window, id), message, wParam, reinterpret_cast<LPARAM>(text));
}

// A list's item data as the stock keeps it (32 bits).
uint32_t itemData(HWND window, int id, LRESULT index) {
    return static_cast<uint32_t>(send(window, id, CB_GETITEMDATA, static_cast<WPARAM>(index)));
}

bool checked(HWND window, int id) { return send(window, id, BM_GETCHECK) == BST_CHECKED; }

const wchar_t* loadString(uint32_t id) {
    ToolState& t = tool();
    if (t.instance == nullptr) return nullptr;
    return LoadStringW(t.instance, id, t.text, static_cast<int>(std::size(t.text))) != 0 ? t.text : nullptr;
}

// 0x00401260: an id of the language's texts (110 .. 349) the language's own, any other as it is.
const wchar_t* rangedText(uint32_t id) {
    if (id >= kTextSettings && id < 350) id = textId(id, tool().language);
    return loadString(id);
}

// 0x004010A0: a line for the debugger.
void debugText(const char* format, ...) {
    static char line[0x400];  // [0x00412B98]
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    OutputDebugStringA(line);
}

// sprintf of a loaded text (its own format, as the stock gives it); a text that did not load leaves nothing.
void format(wchar_t (&out)[0x400], const wchar_t* text, uint32_t value) {
    out[0] = L'\0';
    if (text != nullptr) std::swprintf(out, std::size(out), text, value);
}
void format(wchar_t (&out)[0x400], const wchar_t* text) {
    out[0] = L'\0';
    if (text != nullptr) std::swprintf(out, std::size(out), L"%s", text);
}

// 0x004010A0 with "%s\n": a text's line for the debugger.
void debugLine(const wchar_t* text) {
    OutputDebugStringW(text);
    OutputDebugStringW(L"\n");
}

// 0x00401000: the adapter's shaders by its caps (the state keeps it). Stock problem: a failed GetDeviceCaps leaves the
// caps as the stack held them; zeros here.
int adapterShaderLevel(IDirect3D9* d3d, UINT adapter, D3DDEVTYPE type) {
    D3DCAPS9 caps{};
    d3d->GetDeviceCaps(adapter, type, &caps);
    tool().shaderLevel = shaderLevel(caps.VertexShaderVersion, caps.PixelShaderVersion);
    return tool().shaderLevel;
}

// 0x00401080: the stock's test of a mode, built to nothing: no mode fails it.
bool modeFails() { return false; }

// 0x004010D0: more than one processor in the system's affinity (its first 32).
bool multipleProcessors() {
    DWORD_PTR process = 0, system = 0;
    int count = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &process, &system)) {
        for (int i = 0; i < 32; ++i) {
            if ((static_cast<uint32_t>(system) & (1u << i)) != 0) ++count;
        }
    }
    return count > 1;
}

// 0x00401120: the registry's Adapter (the identifier of the adapter last saved) is none of the list's adapters, or
// the key does not open. Stock problem: without the value the stock compares the stack's bytes; zeros here.
bool savedAdapterMissing(HWND window) {
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_QUERY_VALUE, nullptr, &key, nullptr) != 0) {
        return true;
    }
    GUID saved{};
    DWORD size = sizeof saved;
    RegQueryValueExA(key, "Adapter", nullptr, nullptr, reinterpret_cast<BYTE*>(&saved), &size);
    RegCloseKey(key);
    const LRESULT count = send(window, IDC_ADAPTER, CB_GETCOUNT);
    for (LRESULT i = 0; i < count; ++i) {
        D3DADAPTER_IDENTIFIER9 id{};
        tool().d3d->GetAdapterIdentifier(itemData(window, IDC_ADAPTER, i), 0, &id);
        if (std::memcmp(&id.DeviceIdentifier, &saved, sizeof saved) == 0) return false;
    }
    return true;
}

// 0x004012C0: the antialiasing list: "Quality Level n" for n from 0 through the quality levels the adapter's mode has
// (NONMASKABLE, HAL, 16 or 32 bits by the mode, windowed or not); the game asks for quality n - 1, 0 is none. The
// first item selected, then the one of the value kept: the list's own, else the registry's ([0x00412B84]), 0 with
// reset.
void fillAntialiasing(HWND window, bool reset) {
    const bool windowed = checked(window, IDC_WINDOWED);
    const LRESULT adapterItem = send(window, IDC_ADAPTER, CB_GETCURSEL);
    if (adapterItem == CB_ERR) return;
    const uint32_t adapter = itemData(window, IDC_ADAPTER, adapterItem);
    const LRESULT modeItem = send(window, IDC_MODE, CB_GETCURSEL);
    if (modeItem == CB_ERR) return;
    const uint32_t mode = itemData(window, IDC_MODE, modeItem);
    const LRESULT selected = send(window, IDC_ANTIALIASING, CB_GETCURSEL);
    int32_t kept = tool().antialiasing;
    if (selected != CB_ERR) kept = static_cast<int32_t>(itemData(window, IDC_ANTIALIASING, selected));
    if (reset) kept = 0;
    const D3DFORMAT format16or32 = modeDepth(mode) == 16 ? kFormat16 : kFormat32;
    send(window, IDC_ANTIALIASING, CB_RESETCONTENT);
    DWORD levels = 0;
    WPARAM first = 0;
    tool().d3d->CheckDeviceMultiSampleType(adapter, D3DDEVTYPE_HAL, format16or32, windowed,
                                           D3DMULTISAMPLE_NONMASKABLE, &levels);
    uint32_t level = 0;
    do {
        wchar_t text[0x400];
        format(text, loadString(textId(kTextQualityLevel, tool().language)), level);
        const LRESULT item = sendText(window, IDC_ANTIALIASING, CB_ADDSTRING, 0, text);
        send(window, IDC_ANTIALIASING, CB_SETITEMDATA, static_cast<WPARAM>(item), level);
        if (level == 0) first = static_cast<WPARAM>(item);
        ++level;
    } while (level <= levels);
    const LRESULT count = send(window, IDC_ANTIALIASING, CB_GETCOUNT);
    send(window, IDC_ANTIALIASING, CB_SETCURSEL, first);
    for (LRESULT i = 0; i < count; ++i) {
        if (static_cast<int32_t>(itemData(window, IDC_ANTIALIASING, i)) == kept) {
            send(window, IDC_ANTIALIASING, CB_SETCURSEL, static_cast<WPARAM>(i));
        }
    }
}

// 0x00401530: OK's check of the selection: full screen asks to go on (OK, Cancel); the mode tested (no mode fails),
// then full screen says the selection was validated. False keeps the window.
bool confirmSelection(HWND window) {
    const bool windowed = checked(window, IDC_WINDOWED);
    for (int id : {IDC_ADAPTER, IDC_MODE, IDC_ANTIALIASING, IDC_SHADERS}) {
        const LRESULT item = send(window, id, CB_GETCURSEL);
        if (item == CB_ERR) return false;
        itemData(window, id, item);
    }
    if (!windowed) {
        const wchar_t* text = loadString(textId(kTextValidate, tool().language));
        if (MessageBoxW(window, text, kTitle, MB_OKCANCEL) == IDCANCEL) return false;
    }
    GetDesktopWindow();
    if (!modeFails()) {
        if (!windowed) MessageBoxW(window, loadString(textId(kTextValidated, tool().language)), kTitle, MB_OK);
        return true;
    }
    MessageBoxW(window, loadString(textId(kTextNotValidated, tool().language)), kTitle, MB_OK);
    return false;
}

// 0x00401820: the check boxes into the state, and the audioflags they make.
void readChecks(HWND window) {
    ToolState& t = tool();
    t.audio.speech = checked(window, IDC_SPEECH);
    t.audio.music = checked(window, IDC_MUSIC);
    t.audio.hardwareMixing = checked(window, IDC_HARDWARE_MIXING);
    t.audio.allAudio = checked(window, IDC_ALL_AUDIO);
    t.audio.eax = checked(window, IDC_EAX);
    t.threading = t.multipleProcessors ? (checked(window, IDC_THREADING) ? 1u : 0u) : 0u;
    t.widescreen = checked(window, IDC_WIDESCREEN) ? 1u : 0u;
    t.multiTurn = checked(window, IDC_MULTI_TURN) ? 1u : 0u;
    t.audioFlags = audioFlags(t.audio);
}

// 0x00401950: the shaders' list by the selected adapter's level: none, then 1.0, 2.0, 3.0 as the level allows (the
// item data 0 .. 3); then the list's selection by the data of the item selected (none after the reset: kept), the
// data taken as the item's index.
void fillShaders(HWND window, uint32_t kept) {
    const LRESULT adapter = send(window, IDC_ADAPTER, CB_GETCURSEL);
    const int level = adapterShaderLevel(tool().d3d, static_cast<UINT>(adapter), D3DDEVTYPE_HAL);
    send(window, IDC_SHADERS, CB_RESETCONTENT);
    wchar_t text[0x400];
    format(text, loadString(textId(kTextNoShaders, tool().language)));
    LRESULT item = sendText(window, IDC_SHADERS, CB_ADDSTRING, 0, text);
    send(window, IDC_SHADERS, CB_SETITEMDATA, static_cast<WPARAM>(item), 0);
    const uint32_t names[3] = {kTextShader1, kTextShader2, kTextShader3};
    for (int k = 0; k < 3 && level >= k + 2; ++k) {
        format(text, loadString(textId(names[k], tool().language)));
        debugLine(text);
        item = sendText(window, IDC_SHADERS, CB_ADDSTRING, 0, text);
        send(window, IDC_SHADERS, CB_SETITEMDATA, static_cast<WPARAM>(item), static_cast<LPARAM>(k + 1));
    }
    const LRESULT selected = send(window, IDC_SHADERS, CB_GETCURSEL);
    const uint32_t value = selected != CB_ERR ? itemData(window, IDC_SHADERS, selected) : kept;
    send(window, IDC_SHADERS, CB_SETCURSEL, value);
}

// 0x00401C70: the controls' texts in the tool's language.
void setTexts(HWND window) {
    static constexpr struct {
        int control;
        uint32_t text;
    } kTexts[] = {
        {IDC_DISPLAY_GROUP, kTextDisplay},       {IDC_ADAPTER_LABEL, kTextDisplayAdapter},
        {IDC_MODE_LABEL, kTextDisplayMode},      {IDC_ANTIALIASING_LABEL, kTextAntialiasing},
        {IDC_WINDOWED, kTextWindowed},           {IDC_WIDESCREEN, kTextWidescreen},
        {IDC_VIDEO_GROUP, kTextVideo},           {IDC_SHADERS_LABEL, kTextShaders},
        {IDC_AUDIO_GROUP, kTextAudio},           {IDC_SPEECH, kTextSpeech},
        {IDC_MUSIC, kTextMusic},                 {IDC_HARDWARE_MIXING, kTextHardwareMixing},
        {IDC_ALL_AUDIO, kTextAllAudio},          {IDC_THREADING, kTextMultiThreading},
        {IDC_OK, kTextOk},                       {IDC_RESET, kTextReset},
        {IDC_CANCEL, kTextCancel},               {IDC_INPUT_GROUP, kTextInput},
        {IDC_MULTI_TURN, kTextMultiTurn},
    };
    for (const auto& t : kTexts) {
        sendText(window, t.control, WM_SETTEXT, 0, rangedText(t.text));
    }
}

// 0x00401EB0: the Troubleshooting tab's controls on with it (threading only with several processors; speech, music
// and hardware mixing only with all audio; EAX only with all audio and eax.dll's EAX), the Settings tab's otherwise.
void enableControls(HWND window, bool troubleshooting) {
    ToolState& t = tool();
    t.multipleProcessors = multipleProcessors();
    const BOOL on = troubleshooting ? TRUE : FALSE;
    for (int id : {IDC_THREADING_GROUP, IDC_AUDIO_GROUP, IDC_VIDEO_GROUP, IDC_SHADERS_LABEL, IDC_SHADERS,
                   IDC_INPUT_GROUP}) {
        EnableWindow(GetDlgItem(window, id), on);
    }
    EnableWindow(GetDlgItem(window, IDC_THREADING), t.multipleProcessors && troubleshooting);
    EnableWindow(GetDlgItem(window, IDC_ALL_AUDIO), on);
    for (int id : {IDC_SPEECH, IDC_MUSIC, IDC_HARDWARE_MIXING}) {
        EnableWindow(GetDlgItem(window, id), t.audio.allAudio ? on : FALSE);
    }
    EnableWindow(GetDlgItem(window, IDC_EAX), t.audio.allAudio && eaxAvailable() ? on : FALSE);
    EnableWindow(GetDlgItem(window, IDC_MULTI_TURN), on);
    const BOOL settings = troubleshooting ? FALSE : TRUE;
    for (int id : {IDC_DISPLAY_GROUP, IDC_ADAPTER, IDC_MODE, IDC_WINDOWED, IDC_WIDESCREEN, IDC_ANTIALIASING,
                   IDC_ANTIALIASING_LABEL, IDC_MODE_LABEL, IDC_ADAPTER_LABEL}) {
        EnableWindow(GetDlgItem(window, id), settings);
    }
}

// 0x004024A0: the check boxes from the state, then the controls by the tab.
void setChecks(HWND window) {
    const ToolState& t = tool();
    send(window, IDC_SPEECH, BM_SETCHECK, t.audio.speech ? 1 : 0);
    send(window, IDC_MUSIC, BM_SETCHECK, t.audio.music ? 1 : 0);
    send(window, IDC_HARDWARE_MIXING, BM_SETCHECK, t.audio.hardwareMixing ? 1 : 0);
    send(window, IDC_EAX, BM_SETCHECK, t.audio.eax ? 1 : 0);
    // Stock problem: the processors' flag and the value ANDed, so a registry's threading of 2 leaves the box clear; the
    // fixed build checks it for any value but 0 with several processors.
    send(window, IDC_THREADING, BM_SETCHECK, t.multipleProcessors && t.threading != 0 ? 1 : 0);
    send(window, IDC_ALL_AUDIO, BM_SETCHECK, t.audio.allAudio ? 1 : 0);
    send(window, IDC_WIDESCREEN, BM_SETCHECK, t.widescreen != 0 ? 1 : 0);
    send(window, IDC_MULTI_TURN, BM_SETCHECK, t.multiTurn != 0 ? 1 : 0);
    enableControls(window, t.tab != 0);
}

// A registry value of 4 bytes; true when read. Stock problem: the stock's reads share one size, set to 4 or 16 before
// some only, so a value bigger than 4 bytes can be read past its variable; each read here has its own size of 4.
bool readDword(HKEY key, const char* name, uint32_t& value) {
    DWORD size = 4;
    return RegQueryValueExA(key, name, nullptr, nullptr, reinterpret_cast<BYTE*>(&value), &size) == 0;
}

// 0x004025E0: the window filled (on WM_INITDIALOG, and by Reset To Defaults with defaults set). The region build sets
// its language here (7); with the region binding removed the language stays the one WinMain picked. The adapters by
// their descriptions (the item data their ordinals), the first selected; a saved adapter not among them takes the
// defaults. The shaders' list by the adapter (kept the level - 1, else 0). Then with the key open: the registry's
// settings, else (defaults, or no "threading" value) the defaults: no antialiasing, every sound on but EAX by eax.dll,
// threading with several processors, no widescreen or multi-turn wheel. The mode kept (800 x 600 x 32 unless read),
// windowed as read; the mode list made (a CBN_SELCHANGE of the adapter), the antialiasing's; the tabs made once
// (Settings first); the check boxes and the texts.
void initDialog(HWND window, bool defaults) {
    ToolState& t = tool();
    const UINT adapters = t.d3d->GetAdapterCount();
    send(window, IDC_ADAPTER, CB_RESETCONTENT);
    for (UINT i = 0; i < adapters; ++i) {
        D3DADAPTER_IDENTIFIER9 id{};
        t.d3d->GetAdapterIdentifier(i, 0, &id);
        const LRESULT item = send(window, IDC_ADAPTER, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(id.Description));
        send(window, IDC_ADAPTER, CB_SETITEMDATA, static_cast<WPARAM>(item), i);
    }
    send(window, IDC_ADAPTER, CB_SETCURSEL, 0);
    if (savedAdapterMissing(window)) {
        t.adapterChanged = true;
        defaults = true;
    }
    t.shaders = static_cast<uint32_t>(
                    adapterShaderLevel(t.d3d, static_cast<UINT>(send(window, IDC_ADAPTER, CB_GETCURSEL)),
                                       D3DDEVTYPE_HAL)) -
                1;
    if (t.shaders > 3) t.shaders = 0;
    fillShaders(window, t.shaders);
    HKEY key = nullptr;
    const LSTATUS opened =
        RegCreateKeyExA(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_QUERY_VALUE, nullptr, &key, nullptr);
    uint32_t width = 800, height = 600, depth = 32, windowed = 0, antialiasing = 0;
    uint32_t shaders = static_cast<uint32_t>(
                           adapterShaderLevel(t.d3d, static_cast<UINT>(send(window, IDC_ADAPTER, CB_GETCURSEL)),
                                              D3DDEVTYPE_HAL)) -
                       1;
    if (shaders > 3) shaders = 3;
    t.shaders = shaders;
    debugText("g_iShaders %x\n", shaders);
    if (opened == 0) {
        if (!defaults) {
            readDword(key, "RezWidth", width);
            readDword(key, "RezHeight", height);
            readDword(key, "RezDepth", depth);
            readDword(key, "Antialiasing", antialiasing);
            uint32_t saved = 0;
            if (readDword(key, "Shaders", saved)) shaders = saved;
            const uint32_t items = static_cast<uint32_t>(send(window, IDC_SHADERS, CB_GETCOUNT));
            // Stock problem: the count itself is past the last item: no selection, OK refused until one is picked.
            // The fixed build takes the last item.
            if (items != 0 && shaders >= items) shaders = items - 1;
            send(window, IDC_SHADERS, CB_SETCURSEL, shaders);
            readDword(key, "Windowed", windowed);
            GUID adapter{};
            DWORD size = sizeof adapter;
            RegQueryValueExA(key, "Adapter", nullptr, nullptr, reinterpret_cast<BYTE*>(&adapter), &size);
            const LRESULT count = send(window, IDC_ADAPTER, CB_GETCOUNT);
            LRESULT found = 0;
            for (LRESULT i = 0; i < count; ++i) {
                D3DADAPTER_IDENTIFIER9 id{};
                t.d3d->GetAdapterIdentifier(itemData(window, IDC_ADAPTER, i), 0, &id);
                if (std::memcmp(&id.DeviceIdentifier, &adapter, sizeof adapter) == 0) found = i;
            }
            send(window, IDC_ADAPTER, CB_SETCURSEL, static_cast<WPARAM>(found));
            t.antialiasing = static_cast<int32_t>(antialiasing);
            t.shaders = shaders;
            debugText("g_iShaders %x\n", shaders);
            uint32_t flags = 0, widescreen = 0, multiTurn = 0, threading = 0;
            if (readDword(key, "audioflags", flags)) t.audioFlags = flags;
            if (readDword(key, "widescreen", widescreen)) t.widescreen = widescreen;
            t.audio = audioChecks(t.audioFlags);
            if (!eaxAvailable()) t.audio.eax = false;
            if (readDword(key, "MultiTurn", multiTurn)) t.multiTurn = multiTurn;
            if (readDword(key, "threading", threading)) {
                t.threading = t.multipleProcessors ? threading : 0;
            } else {
                // Stock problem: no "threading" takes the defaults over what was read, the antialiasing's list made
                // as by the reset too. The fixed build gives the threading alone its default.
                t.threading = t.multipleProcessors ? 1u : 0u;
            }
        }
        if (defaults) {
            t.antialiasing = 0;
            t.audio.speech = true;
            t.audio.music = true;
            t.audio.hardwareMixing = true;
            t.audio.eax = eaxAvailable();
            t.multipleProcessors = multipleProcessors();
            t.audio.allAudio = true;
            t.audioFlags = kDefaultAudioFlags;
            t.widescreen = 0;
            t.multiTurn = 0;
            t.threading = t.multipleProcessors ? 1u : 0u;
        }
        setChecks(window);
        RegCloseKey(key);
    }
    t.mode = modeKey(width, height, depth == 16);
    send(window, IDC_WINDOWED, BM_SETCHECK, windowed != 0 ? 1 : 0);
    SendMessageA(window, WM_COMMAND, MAKEWPARAM(IDC_ADAPTER, CBN_SELCHANGE),
                 reinterpret_cast<LPARAM>(GetDlgItem(window, IDC_ADAPTER)));
    fillAntialiasing(window, defaults);
    if (!t.tabsMade) {
        t.tabsMade = true;
        send(window, IDC_TABS, TCM_DELETEALLITEMS);
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<wchar_t*>(toolText(kTextTroubleshooting));
        SendMessageW(GetDlgItem(window, IDC_TABS), TCM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
        item.pszText = const_cast<wchar_t*>(toolText(kTextSettings));
        SendMessageW(GetDlgItem(window, IDC_TABS), TCM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
        send(window, IDC_TABS, TCM_SETCURSEL, 0);
    }
    setChecks(window);
    setTexts(window);
}

// 0x00404AC1 .. 0x00404F8B, the adapter's CBN_SELCHANGE: the mode list anew. Windowed: the windowed modes within the
// desktop's display mode, at its depth (16 for X1R5G5B5, A1R5G5B5 and R5G6B5, else 32). Full screen: the adapter's
// X8R8G8B8 then R5G6B5 modes from 640 x 480 (from the last mode down, each once), sorted (0x00402030). Then the item
// nearest the mode kept selected and the antialiasing's list made.
void fillModes(HWND window) {
    ToolState& t = tool();
    char text[0x400];
    if (checked(window, IDC_WINDOWED)) {
        const LRESULT adapterItem = send(window, IDC_ADAPTER, CB_GETCURSEL);
        if (adapterItem != CB_ERR) {
            send(window, IDC_MODE, CB_RESETCONTENT);
            const uint32_t adapter = itemData(window, IDC_ADAPTER, adapterItem);
            D3DDISPLAYMODE desktop{};
            t.d3d->GetAdapterDisplayMode(adapter, &desktop);
            const bool sixteen = desktop.Format == D3DFMT_X1R5G5B5 || desktop.Format == D3DFMT_A1R5G5B5 ||
                                 desktop.Format == D3DFMT_R5G6B5;
            for (const WindowedMode& m : kWindowedModes) {
                if (m.width <= desktop.Width && m.height <= desktop.Height && !modeFails()) {
                    std::snprintf(text, sizeof text, "%dx%d %d-bit", m.width, m.height, sixteen ? 16 : 32);
                    const LRESULT item = send(window, IDC_MODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
                    send(window, IDC_MODE, CB_SETITEMDATA, static_cast<WPARAM>(item),
                         modeKey(m.width, m.height, sixteen));
                }
            }
        }
    } else {
        const LRESULT adapterItem = send(window, IDC_ADAPTER, CB_GETCURSEL);
        if (adapterItem != CB_ERR) {
            std::set<uint32_t> seen;
            std::vector<uint32_t> modes;
            const uint32_t adapter = itemData(window, IDC_ADAPTER, adapterItem);
            send(window, IDC_MODE, CB_RESETCONTENT);
            for (const D3DFORMAT format16or32 : {kFormat32, kFormat16}) {
                const bool sixteen = format16or32 == kFormat16;
                for (int i = static_cast<int>(t.d3d->GetAdapterModeCount(adapter, format16or32)) - 1; i >= 0; --i) {
                    D3DDISPLAYMODE m{};
                    t.d3d->EnumAdapterModes(adapter, format16or32, static_cast<UINT>(i), &m);
                    if (m.Width < 640 || m.Height < 480) continue;
                    const uint32_t key = modeKey(m.Width, m.Height, sixteen);
                    if (seen.find(key) != seen.end() || modeFails()) continue;
                    seen.insert(key);
                    modes.push_back(key);
                }
            }
            std::sort(modes.begin(), modes.end(), modeLess);
            for (const uint32_t key : modes) {
                std::snprintf(text, sizeof text, "%dx%d %d-bit", modeWidth(key), modeHeight(key), modeDepth(key));
                const LRESULT item = send(window, IDC_MODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
                send(window, IDC_MODE, CB_SETITEMDATA, static_cast<WPARAM>(item), key);
            }
        }
    }
    const LRESULT count = send(window, IDC_MODE, CB_GETCOUNT);
    std::vector<uint32_t> keys;
    for (LRESULT i = 0; i < count; ++i) keys.push_back(itemData(window, IDC_MODE, i));
    send(window, IDC_MODE, CB_SETCURSEL, nearestMode(keys, t.mode));
    fillAntialiasing(window, false);
}

// 0x00405170 .. 0x00405300, OK: the selection checked (0x00401530), then saved: RezWidth, RezHeight, RezDepth of the
// mode, the adapter's identifier (Adapter, 16 bytes), Windowed, Antialiasing and Shaders (the lists' data), the check
// boxes' audioflags, widescreen, MultiTurn and threading.
bool saveSettings(HWND window) {
    if (!confirmSelection(window)) return false;
    ToolState& t = tool();
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_SET_VALUE | KEY_CREATE_SUB_KEY, nullptr, &key,
                        nullptr) == 0) {
        const auto setDword = [&](const char* name, uint32_t value) {
            RegSetValueExA(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), 4);
        };
        const uint32_t adapter = itemData(window, IDC_ADAPTER, send(window, IDC_ADAPTER, CB_GETCURSEL));
        const uint32_t mode = itemData(window, IDC_MODE, send(window, IDC_MODE, CB_GETCURSEL));
        setDword("RezWidth", modeWidth(mode));
        setDword("RezHeight", modeHeight(mode));
        setDword("RezDepth", modeDepth(mode));
        D3DADAPTER_IDENTIFIER9 id{};
        t.d3d->GetAdapterIdentifier(adapter, 0, &id);
        RegSetValueExA(key, "Adapter", 0, REG_BINARY, reinterpret_cast<const BYTE*>(&id.DeviceIdentifier), 0x10);
        const uint32_t windowed = checked(window, IDC_WINDOWED) ? 1u : 0u;
        const uint32_t antialiasing =
            itemData(window, IDC_ANTIALIASING, send(window, IDC_ANTIALIASING, CB_GETCURSEL));
        setDword("Windowed", windowed);
        setDword("Antialiasing", antialiasing);
        setDword("Shaders", itemData(window, IDC_SHADERS, send(window, IDC_SHADERS, CB_GETCURSEL)));
        readChecks(window);
        setDword("audioflags", t.audioFlags);
        setDword("widescreen", t.widescreen);
        setDword("MultiTurn", t.multiTurn);
        setDword("threading", t.threading);
        RegCloseKey(key);
    }
    return true;
}

}  // namespace

ToolState& tool() {
    static ToolState state;
    return state;
}

const wchar_t* toolText(uint32_t first) { return loadString(textId(first, tool().language)); }

INT_PTR CALLBACK configDialog(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    // Every message first: the tab shown changed turns its controls on.
    const int tab = static_cast<int>(send(window, IDC_TABS, TCM_GETCURSEL));
    if (tab != tool().tab) {
        tool().tab = tab;
        enableControls(window, tab != 0);
    }
    switch (message) {
    case WM_CLOSE:
        EndDialog(window, 0);
        return FALSE;
    case WM_INITDIALOG:
        initDialog(window, false);
        return FALSE;
    case WM_COMMAND:
        break;
    default:
        return FALSE;
    }
    const int control = LOWORD(wParam);
    const int notification = HIWORD(wParam);
    if (notification != 0) {
        if (notification != CBN_SELCHANGE) return FALSE;
        if (control == IDC_ADAPTER) {
            fillModes(window);
        } else if (control == IDC_MODE) {
            const LRESULT item = send(window, IDC_MODE, CB_GETCURSEL);
            if (item != CB_ERR) tool().mode = itemData(window, IDC_MODE, item);
            fillAntialiasing(window, false);
        }
        return FALSE;
    }
    switch (control) {
    case IDC_WINDOWED:
        SendMessageA(window, WM_COMMAND, MAKEWPARAM(IDC_ADAPTER, CBN_SELCHANGE),
                     reinterpret_cast<LPARAM>(GetDlgItem(window, IDC_ADAPTER)));
        fillAntialiasing(window, false);
        break;
    case IDC_OK:
        if (saveSettings(window)) EndDialog(window, 1);
        break;
    case IDC_CANCEL:
        EndDialog(window, 0);
        break;
    case IDC_RESET:
        initDialog(window, true);
        break;
    case IDC_ALL_AUDIO:
        readChecks(window);
        setChecks(window);
        break;
    default: break;
    }
    return FALSE;
}

}  // namespace oj::config
