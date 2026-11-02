#include "Game/Platform/Settings.h"

#include "Game/Platform/CommandLine.h"

#include <windows.h>

namespace oj {

namespace {

constexpr char kKey[] = "Software\\THQ\\Juiced";

// The key is opened as the stock game does: created when missing, with query access.
HKEY openKey() {
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_QUERY_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return nullptr;
    }
    return key;
}

bool readDword(HKEY key, const char* name, uint32_t& out) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegQueryValueExA(key, name, nullptr, nullptr, reinterpret_cast<BYTE*>(&value), &size) != ERROR_SUCCESS) {
        return false;
    }
    out = value;
    return true;
}

// A value within [low, high], else `out` as it is (JuicedConfigExt's plugin reads its options so).
void readInRange(HKEY key, const char* name, uint32_t low, uint32_t high, uint32_t& out) {
    uint32_t value = 0;
    if (readDword(key, name, value) && value >= low && value <= high) out = value;
}

// The registry's audio flags left out: all the audio off, the music off, the speech off.
constexpr uint32_t kAudioOffFlags = 0x01 | 0x08 | 0x10;

}  // namespace

std::optional<uint32_t> readSetting(const char* name) {
    HKEY key = openKey();
    if (key == nullptr) return std::nullopt;
    uint32_t value = 0;
    const bool read = readDword(key, name, value);
    RegCloseKey(key);
    return read ? std::optional<uint32_t>(value) : std::nullopt;
}

Settings Settings::defaults(const LaunchOptions& options) {
    Settings s;
    s.audioFlags |= options.audioFlagsOr;
    if (options.audioFlagsOne) {
        s.audioFlags = 1;
    }
    return s;
}

std::optional<std::array<uint8_t, 16>> readAdapterId() {
    HKEY key = openKey();
    if (key == nullptr) {
        return std::nullopt;
    }
    std::array<uint8_t, 16> id{};
    DWORD size = static_cast<DWORD>(id.size());
    const LSTATUS status = RegQueryValueExA(key, "Adapter", nullptr, nullptr, id.data(), &size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return id;
}

bool writeSetting(const char* name, uint32_t value) {
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return false;
    }
    const DWORD data = value;
    const bool written = RegSetValueExA(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&data),
                                        sizeof(data)) == ERROR_SUCCESS;
    RegCloseKey(key);
    return written;
}

int32_t readLanguageSetting() {
    Settings s;
    readSettings(s);
    return s.language;
}

void readSettings(Settings& s) {
    HKEY key = openKey();
    if (key == nullptr) {
        return;
    }
    readDword(key, "RezWidth", s.width);
    readDword(key, "RezHeight", s.height);
    readDword(key, "RezDepth", s.depth);
    readDword(key, "Antialiasing", s.antialiasing);
    readDword(key, "Shaders", s.shaders);
    readDword(key, "Windowed", s.windowed);
    uint32_t language = 0;
    s.language = readDword(key, "language", language) ? static_cast<int32_t>(language) : -1;
    readDword(key, "audioflags", s.audioFlags);
    // The fixed build: music, speech and all the audio off (1, 8, 0x10) no more from the registry (OpenJuiceConfig
    // has no Audio page; the game's audio options set the volumes; the command line's -x and -n still do).
    s.audioFlags &= ~kAudioOffFlags;
    readDword(key, "widescreen", s.widescreen);
    readDword(key, "MultiTurn", s.multiTurn);
    readDword(key, "threading", s.threading);
    // The fixed build's renderer (JuicedConfigExt's GameSettings::Load).
    s.displayMode = s.windowed != 0 ? 2 : 0;
    if (uint32_t mode = 0; readDword(key, "DisplayMode", mode)) s.displayMode = mode > 2 ? 1 : mode;
    readDword(key, "BorderlessScale", s.borderlessScale);
    if (s.borderlessScale > 1) s.borderlessScale = 0;
    readDword(key, "RefreshRate", s.refreshRate);
    readDword(key, "MSAA", s.msaa);
    readInRange(key, "SMAA", 0, 1, s.smaa);
    readDword(key, "VSync", s.vsync);
    readDword(key, "MaxFPS", s.maxFps);
    if (s.maxFps > 1000) s.maxFps = 1000;
    readDword(key, "TextureFilter", s.textureFilter);
    if (s.textureFilter > 5) s.textureFilter = 5;
    readDword(key, "LodBias", s.lodBias);
    if (s.lodBias > 4) s.lodBias = 3;
    readDword(key, "ShowFps", s.showFps);
    readInRange(key, "CarPaint", 0, 1, s.carPaint);
    readInRange(key, "WorldLight", 0, 2, s.worldLight);
    readInRange(key, "Sky", 0, 2, s.sky);
    readInRange(key, "Headlights", 0, 2, s.headlights);
    readInRange(key, "HeatHaze", 0, 1, s.heatHaze);
    readInRange(key, "HeatHazeStrength", 0, 50, s.heatHazeStrength);
    readInRange(key, "Reflections", 0, 1, s.reflections);
    readInRange(key, "LightFlares", 0, 1, s.lightFlares);
    readInRange(key, "Shadows", 0, 2, s.shadows);
    readInRange(key, "AmbientOcclusion", 0, 1, s.ambientOcclusion);
    wchar_t monitor[64] = {};
    DWORD size = sizeof(monitor) - sizeof(wchar_t);
    DWORD type = 0;
    if (RegQueryValueExW(key, L"Monitor", nullptr, &type, reinterpret_cast<BYTE*>(monitor), &size) == ERROR_SUCCESS &&
        type == REG_SZ) {
        s.monitor = monitor;
    }
    RegCloseKey(key);
}

}  // namespace oj
