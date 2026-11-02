#include "Game/Platform/WheelInput.h"

#include "Core/Base/Log.h"

#include <WheelDetect.h>
#include <WheelReader.h>

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <atomic>
#include <cwchar>

namespace oj {

namespace {

// The settings the reader's steering takes (its adjustment is a plain function; one WheelInput loads them) and
// whether the FF manager's listing is still to come (the reader then waits to open a device it may take).
const WheelSettings* g_settings = nullptr;
std::atomic<bool> g_listing{false};

bool listing() {
    return g_listing;
}

void readerLog(const char* text) {
    log::info("wheel: {}", text);
}

float steering(float value) {
    return g_settings != nullptr ? WheelReader::AdjustSteering(value, g_settings->deadzone, g_settings->gain()) : value;
}

// A value of [Wheel] as the plugin's ReadFloat takes it: up to a ';', a number within its range, else the default.
std::wstring text(const std::wstring& path, const wchar_t* key) {
    wchar_t value[256]{};
    GetPrivateProfileStringW(L"Wheel", key, L"", value, 256, path.c_str());
    std::wstring t = value;
    t = t.substr(0, t.find(L';'));
    while (!t.empty() && (t.back() == L' ' || t.back() == L'\t')) t.pop_back();
    while (!t.empty() && (t.front() == L' ' || t.front() == L'\t')) t.erase(t.begin());
    return t;
}

float number(const std::wstring& path, const wchar_t* key, float minimum, float maximum, float fallback) {
    const std::wstring t = text(path, key);
    if (t.empty()) return fallback;
    wchar_t* end = nullptr;
    const float v = std::wcstof(t.c_str(), &end);
    return end != t.c_str() && *end == L'\0' && v >= minimum && v <= maximum ? v : fallback;
}

bool flag(const std::wstring& path, const wchar_t* key, bool fallback) {
    const std::wstring t = text(path, key);
    if (_wcsicmp(t.c_str(), L"true") == 0 || t == L"1" || _wcsicmp(t.c_str(), L"yes") == 0 ||
        _wcsicmp(t.c_str(), L"on") == 0) {
        return true;
    }
    if (_wcsicmp(t.c_str(), L"false") == 0 || t == L"0" || _wcsicmp(t.c_str(), L"no") == 0 ||
        _wcsicmp(t.c_str(), L"off") == 0) {
        return false;
    }
    return fallback;
}

std::string utf8FromAnsi(const char* text) {
    const int n = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    std::wstring wide(static_cast<size_t>(n > 0 ? n : 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, wide.data(), n);
    const int m = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(m > 0 ? m : 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, out.data(), m, nullptr, nullptr);
    out.resize(std::char_traits<char>::length(out.c_str()));
    return out;
}

const char* ffbName(WheelSettings::Ffb ffb) {
    switch (ffb) {
    case WheelSettings::Ffb::Off: return "off";
    case WheelSettings::Ffb::Scaled: return "scaled";
    default: return "game";
    }
}

}  // namespace

WheelSettings readWheelSettings(const std::wstring& path) {
    WheelSettings s;
    s.deadzone = number(path, L"deadzone", 0.0f, 0.5f, s.deadzone);
    s.rotation = number(path, L"rotation", 0.0f, 3600.0f, s.rotation);
    s.lock = number(path, L"lock", 30.0f, 3600.0f, s.lock);
    s.exponent = number(path, L"exponent", 0.0f, 4.0f, s.exponent);
    if (s.exponent > 0.0f && s.exponent < 0.3f) s.exponent = 0.0f;
    s.linearInput = flag(path, L"linearInput", s.linearInput);
    // off | game | scaled; true / false of the plugin's 0.1.5: scaled / game.
    const std::wstring ffb = text(path, L"ffb");
    s.ffb = _wcsicmp(ffb.c_str(), L"off") == 0 ? WheelSettings::Ffb::Off
            : _wcsicmp(ffb.c_str(), L"scaled") == 0 || _wcsicmp(ffb.c_str(), L"true") == 0 || ffb == L"1" ||
                    _wcsicmp(ffb.c_str(), L"yes") == 0 || _wcsicmp(ffb.c_str(), L"on") == 0
                ? WheelSettings::Ffb::Scaled
                : WheelSettings::Ffb::Game;
    s.scale.strength = static_cast<unsigned>(number(path, L"ffbStrength", 10.0f, 100.0f, 100.0f));
    s.scale.centering = static_cast<unsigned>(number(path, L"ffbCentering", 0.0f, 200.0f, 100.0f));
    s.scale.effects = static_cast<unsigned>(number(path, L"ffbEffects", 0.0f, 200.0f, 100.0f));
    s.scale.invert = flag(path, L"ffbInvert", false);
    return s;
}

WheelInput::WheelInput() : m_reader(std::make_unique<WheelReader::Reader>()) {}

WheelInput::~WheelInput() {
    stop();
    if (g_settings == &m_settings) g_settings = nullptr;
}

void WheelInput::load(const std::wstring& path) {
    m_path = path;
    m_settings = readWheelSettings(path);
    g_settings = &m_settings;
    m_reader->SetLog(&readerLog);
    m_reader->SetSteering(&steering);
    m_reader->SetForceFeedbackChooses(&listing);
    m_reader->SetDefaultMap(true);
    log::info("wheel: controller.ini [Wheel]: dead zone {:.2f}, rotation {:.0f}, lock {:.0f}, exponent {:.2f}{}, "
              "ffb {} (strength {}%, centering {}%, vibrations {}%{})",
              m_settings.deadzone, m_settings.rotation, m_settings.lock, m_settings.exponent,
              m_settings.linearInput ? ", linear input" : "", ffbName(m_settings.ffb), m_settings.scale.strength,
              m_settings.scale.centering, m_settings.scale.effects, m_settings.scale.invert ? ", inverted" : "");
}

bool WheelInput::forceFeedbackTakes(const DIDEVICEINSTANCEA& instance) const {
    if (m_settings.ffb == WheelSettings::Ffb::Off) {
        log::info("force feedback: \"{}\" not given (ffb = off)", instance.tszProductName);
        return false;
    }
    const char* mark = nullptr;
    if (WheelDetect::IsWheel(instance.guidProduct.Data1, instance.dwDevType, &mark)) {
        log::info("force feedback: \"{}\", a steering wheel by {}", instance.tszProductName, mark);
        return true;
    }
    if (IsEqualGUID(instance.guidFFDriver, GUID_NULL)) {
        log::info("force feedback: \"{}\" not given (no force feedback)", instance.tszProductName);
        return false;
    }
    return true;
}

int WheelInput::channel(const DIDEVICEINSTANCEA& instance, bool opened) {
    return m_reader->ForceFeedbackDevice(instance.guidInstance, utf8FromAnsi(instance.tszProductName), opened);
}

void WheelInput::channelState(int slot, const void* state) {
    m_reader->SharedState(slot, state);
}

void WheelInput::start() {
    g_listing = true;
    m_reader->Start(m_path);
}

void WheelInput::listed() {
    g_listing = false;
}

void WheelInput::stop() {
    m_reader->Stop();
}

}  // namespace oj
