#pragma once
#include "Core/Data/Controls.h"
#include "Linux/Paths.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <fstream>
#include <map>
namespace oj::linux_port {
inline void addCrewBindings(std::vector<ActionMap> &maps) {
    constexpr const char *names[] = {"DRIVER_TACTICS_UP",   "DRIVER_TACTICS_RIGHT", "DRIVER_TACTICS_DOWN",
                                     "DRIVER_TACTICS_LEFT", "DRIVER_TACTICS3_UP",   "DRIVER_TACTICS3_DOWN"};
    for (auto &map : maps) {
        if ((map.name != "DRIVER1" && map.name != "DRIVER2" && map.name != "DRIVER3" && map.name != "DRIVER4") ||
            (!map.device.empty() && map.device != "ACTION_MAP"))
            continue;
        for (int i = 0; i < 6; ++i) {
            const auto semantic = "OJ_CREW_" + std::to_string(i);
            std::erase_if(map.actions, [&](const ControlAction &a) { return a.control == semantic; });
            ControlAction a;
            a.name = names[i];
            a.oneShot = true;
            a.control = a.controlText = semantic;
            map.actions.push_back(std::move(a));
        }
    }
}
struct ControllerConfig {
    int deadzone = 12;
    std::map<int, int> remap;
    void load() {
        deadzone = 12;
        remap.clear();
        std::ifstream file(dataPath() / "controller.ini");
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
    return pad && SDL_GamepadHasAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) && SDL_GamepadHasAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
}
} // namespace oj::linux_port
