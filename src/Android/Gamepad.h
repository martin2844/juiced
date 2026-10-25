#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>

namespace oj::android {
void overlayInput(bool enabled);
void openControllerSettings();
struct ControllerConfig {
    int deadzone = 12;
    std::map<int, int> remap;
    void load() {
        deadzone = 12;
        remap.clear();
        const char *root = getenv("JUICED_SUPPORT_DIR");
        if (!root)
            return;
        std::ifstream file(std::filesystem::path(root) / "controller.ini");
        const std::map<std::string, int> names = {{"select", 0},  {"back", 1},         {"action1", 3},
                                                  {"action2", 2}, {"leftstick", 7},    {"rightstick", 8},
                                                  {"pause", 6},   {"leftshoulder", 9}, {"rightshoulder", 10}};
        std::string line;
        while (std::getline(file, line))
            try {
                auto eq = line.find('=');
                auto key = line.substr(0, eq);
                int value = std::stoi(line.substr(eq + 1));
                if (key == "deadzone")
                    deadzone = std::clamp(value, 0, 30);
                else if (names.contains(key) && value >= 0 && value <= 10)
                    remap[names.at(key)] = value;
            } catch (...) {
            }
    }
    SDL_GamepadButton button(SDL_GamepadButton b) const {
        auto i = remap.find(b);
        return i == remap.end() ? b : static_cast<SDL_GamepadButton>(i->second);
    }
};
inline ControllerConfig &controllerConfig() {
    static ControllerConfig config = [] {
        ControllerConfig c;
        c.load();
        return c;
    }();
    return config;
}
inline bool hasDrivingAxes(SDL_Gamepad *pad) {
    return pad && SDL_GamepadHasAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) &&
           SDL_GamepadHasAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
}
// Android also exposes D-pad keyboards as gamepads. Prefer a real analog pad,
// including when it is connected after a keyboard-only device was selected.
inline void connectGamepad(SDL_Gamepad *&pad) {
    if (pad && !SDL_GamepadConnected(pad)) {
        SDL_CloseGamepad(pad);
        pad = nullptr;
    }
    if (hasDrivingAxes(pad))
        return;
    int count = 0;
    auto ids = SDL_GetGamepads(&count);
    for (int i = 0; i < count; ++i) {
        if (pad && ids[i] == SDL_GetGamepadID(pad))
            continue;
        auto candidate = SDL_OpenGamepad(ids[i]);
        if (!candidate)
            continue;
        if (!pad || hasDrivingAxes(candidate)) {
            if (pad)
                SDL_CloseGamepad(pad);
            pad = candidate;
            if (hasDrivingAxes(pad))
                break;
        } else
            SDL_CloseGamepad(candidate);
    }
    SDL_free(ids);
}
} // namespace oj::android
