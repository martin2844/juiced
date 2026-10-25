#pragma once
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace oj::linux_port {
inline std::filesystem::path xdg(const char* variable, const char* fallback) {
    if (const char* override = std::getenv("JUICED_SUPPORT_DIR"); override && *override)
        return std::filesystem::absolute(override);
    if (const char* value = std::getenv(variable); value && *value && std::filesystem::path(value).is_absolute())
        return std::filesystem::path(value) / "openjuice";
    const char* home = std::getenv("HOME");
    if (!home || !*home) throw std::runtime_error("HOME must be set for OpenJuice user data");
    return std::filesystem::path(home) / fallback / "openjuice";
}
inline std::filesystem::path dataPath() { return xdg("XDG_DATA_HOME", ".local/share"); }
inline std::filesystem::path configPath() { return xdg("XDG_CONFIG_HOME", ".config"); }
inline std::filesystem::path statePath() { return xdg("XDG_STATE_HOME", ".local/state"); }
}
