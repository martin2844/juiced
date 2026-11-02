#include "Game/Platform/Pads.h"

#include "Core/Base/Log.h"

#include <LayoutFile.h>
#include <PadReader.h>

#include <SDL3/SDL.h>

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace oj {

namespace {

// SDL's functions as PadReader calls them (its enums as ints).
bool __cdecl setHint(const char* name, const char* value) { return SDL_SetHint(name, value); }
bool __cdecl init(std::uint32_t flags) { return SDL_Init(flags); }
void __cdecl updateGamepads() { SDL_UpdateGamepads(); }
std::uint32_t* __cdecl getGamepads(int* count) { return SDL_GetGamepads(count); }
void __cdecl freeMemory(void* memory) { SDL_free(memory); }
void* __cdecl openGamepad(std::uint32_t id) { return SDL_OpenGamepad(id); }
void __cdecl closeGamepad(void* pad) { SDL_CloseGamepad(static_cast<SDL_Gamepad*>(pad)); }
bool __cdecl gamepadConnected(void* pad) { return SDL_GamepadConnected(static_cast<SDL_Gamepad*>(pad)); }
std::int16_t __cdecl gamepadAxis(void* pad, int axis) {
    return SDL_GetGamepadAxis(static_cast<SDL_Gamepad*>(pad), static_cast<SDL_GamepadAxis>(axis));
}
bool __cdecl gamepadButton(void* pad, int button) {
    return SDL_GetGamepadButton(static_cast<SDL_Gamepad*>(pad), static_cast<SDL_GamepadButton>(button));
}
const char* __cdecl gamepadName(void* pad) { return SDL_GetGamepadName(static_cast<SDL_Gamepad*>(pad)); }
int __cdecl gamepadType(void* pad) { return static_cast<int>(SDL_GetGamepadType(static_cast<SDL_Gamepad*>(pad))); }
const char* __cdecl gamepadNameForId(std::uint32_t id) { return SDL_GetGamepadNameForID(id); }
std::uint16_t __cdecl gamepadVendorForId(std::uint32_t id) { return SDL_GetGamepadVendorForID(id); }
std::uint16_t __cdecl gamepadProductForId(std::uint32_t id) { return SDL_GetGamepadProductForID(id); }
bool __cdecl rumbleGamepad(void* pad, std::uint16_t low, std::uint16_t high, std::uint32_t ms) {
    return SDL_RumbleGamepad(static_cast<SDL_Gamepad*>(pad), low, high, ms);
}
std::uint32_t* __cdecl getJoysticks(int* count) { return SDL_GetJoysticks(count); }
std::uint16_t __cdecl joystickVendorForId(std::uint32_t id) { return SDL_GetJoystickVendorForID(id); }
std::uint16_t __cdecl joystickProductForId(std::uint32_t id) { return SDL_GetJoystickProductForID(id); }
int __cdecl joystickTypeForId(std::uint32_t id) { return static_cast<int>(SDL_GetJoystickTypeForID(id)); }

bool resolveSdl(PadReader::SdlApi& s) {
    s.SetHint = &setHint;
    s.Init = &init;
    s.UpdateGamepads = &updateGamepads;
    s.GetGamepads = &getGamepads;
    s.Free = &freeMemory;
    s.OpenGamepad = &openGamepad;
    s.CloseGamepad = &closeGamepad;
    s.GamepadConnected = &gamepadConnected;
    s.GetGamepadAxis = &gamepadAxis;
    s.GetGamepadButton = &gamepadButton;
    s.GetGamepadName = &gamepadName;
    s.GetGamepadType = &gamepadType;
    s.GetGamepadNameForID = &gamepadNameForId;
    s.GetGamepadVendorForID = &gamepadVendorForId;
    s.GetGamepadProductForID = &gamepadProductForId;
    s.RumbleGamepad = &rumbleGamepad;
    s.GetJoysticks = &getJoysticks;
    s.GetJoystickVendorForID = &joystickVendorForId;
    s.GetJoystickProductForID = &joystickProductForId;
    s.GetJoystickTypeForID = &joystickTypeForId;
    return true;
}

void readerLog(const char* text) {
    log::info("controls: {}", text);
}

std::wstring readValue(const std::wstring& path, const wchar_t* key, const wchar_t* missing) {
    wchar_t value[256]{};
    GetPrivateProfileStringW(L"Controller", key, missing, value, 256, path.c_str());
    return value;
}

std::string narrow(const std::wstring& text) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n > 0 ? n : 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, out.data(), n, nullptr, nullptr);
    out.resize(std::char_traits<char>::length(out.c_str()));
    return out;
}

float number(const std::wstring& text, float missing) {
    if (text.empty()) return missing;
    wchar_t* end = nullptr;
    const float v = std::wcstof(text.c_str(), &end);
    return end != text.c_str() ? v : missing;
}

}  // namespace

struct Pads::State {
    std::wstring path = LayoutFile::Path();
    ControllerLayout::Layout layout;
    bool enabled = true;
    bool rumble = false;
    float rumbleStrength = 100.0f;  // %
    bool consoleSteering = true;
    PadReader::Settings settings;
    PadReader::Reader reader;
    bool read = false;
    DWORD checked = 0;
    FILETIME time{};
};

Pads::Pads() : m(std::make_unique<State>()) {
    m->reader.SetLog(&readerLog);
    m->reader.SetSdl(&resolveSdl);
}

Pads::~Pads() {
    m->reader.StopMotors();
    m->reader.Reconnect();
    if (SDL_WasInit(SDL_INIT_GAMEPAD) != 0) SDL_Quit();
}

bool Pads::refresh() {
    const DWORD now = GetTickCount();
    if (m->read && now - m->checked < 1000) return false;
    m->checked = now;
    WIN32_FILE_ATTRIBUTE_DATA data{};
    FILETIME time{};
    if (!m->path.empty() && GetFileAttributesExW(m->path.c_str(), GetFileExInfoStandard, &data)) {
        time = data.ftLastWriteTime;
    }
    if (m->read && CompareFileTime(&time, &m->time) == 0) return false;
    m->time = time;
    m->read = true;
    m->layout = LayoutFile::Load(m->path);
    const std::wstring enabled = readValue(m->path, L"enabled", L"true");
    m->enabled = _wcsicmp(enabled.c_str(), L"false") != 0 && enabled != L"0";
    PadReader::Settings settings;
    settings.device = narrow(readValue(m->path, L"device", L""));
    settings.stickDeadzone = number(readValue(m->path, L"stickDeadzone", L""), 0.24f);
    settings.triggerDeadzone = number(readValue(m->path, L"triggerDeadzone", L""), 0.12f);
    if (settings.device != m->settings.device) m->reader.Reconnect();
    m->settings = settings;
    m->reader.SetSettings(settings);
    const std::wstring rumble = readValue(m->path, L"rumble", L"false");
    m->rumble = _wcsicmp(rumble.c_str(), L"true") == 0 || rumble == L"1";
    const float strength = number(readValue(m->path, L"rumbleStrength", L""), 100.0f);
    m->rumbleStrength = strength >= 10.0f && strength <= 200.0f ? strength : 100.0f;
    if (!m->rumble) m->reader.StopMotors();
    const std::wstring console = readValue(m->path, L"consoleSteering", L"true");
    m->consoleSteering = _wcsicmp(console.c_str(), L"false") != 0 && console != L"0";
    log::info("controls: controller.ini read: layout {}, keyboard {}, controller {} ({}), dead zones {:.2f} {:.2f}, "
              "vibration {} ({:.0f} %), steering as on the {}",
              ControllerLayout::kPresetNames[m->layout.preset], ControllerLayout::KeyboardName(m->layout.keyboard),
              m->enabled ? "on" : "off", settings.device.empty() ? "the first connected" : settings.device,
              settings.stickDeadzone, settings.triggerDeadzone, m->rumble ? "on" : "off", m->rumbleStrength,
              m->consoleSteering ? "Xbox" : "PC");
    return true;
}

const ControllerLayout::Layout& Pads::layout() const {
    return m->layout;
}

bool Pads::controllerEnabled() const {
    return m->enabled;
}

bool Pads::consoleSteering() const {
    return m->consoleSteering;
}

void Pads::setMotors(const input::PadRumble& rumble) {
    if (!m->enabled || !m->rumble) {
        if (m->reader.MotorsOn()) m->reader.StopMotors();
        return;
    }
    const input::Motors levels = rumble.levels(input::PadRumble::now(), m->rumbleStrength / 100.0f);
    const auto speed = [](float level) { return static_cast<std::uint16_t>(std::lround(level * 65535.0f)); };
    m->reader.SetMotors(speed(levels.heavy), speed(levels.light));
}

input::PadState Pads::read() {
    input::PadState s;
    if (!m->enabled) return s;
    m->reader.Poll();
    s.connected = m->reader.Connected();
    s.playStation = m->reader.PlayStation();
    s.pad = m->reader.Pad();
    return s;
}

}  // namespace oj
