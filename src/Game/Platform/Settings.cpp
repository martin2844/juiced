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

}  // namespace

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
    readDword(key, "widescreen", s.widescreen);
    readDword(key, "MultiTurn", s.multiTurn);
    readDword(key, "threading", s.threading);
    RegCloseKey(key);
}

}  // namespace oj
