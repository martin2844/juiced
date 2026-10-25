#pragma once
#include "Core/Data/Controls.h"
#include <array>

namespace oj::android {
// Resolve logical touch actions against the effective DRIVER1 keyboard bindings,
// after stock maps, optional mods and saved keyboard settings have been applied.
inline std::array<uint8_t, 12> resolveTouchBindings(const std::vector<ActionMap> &maps) {
    std::array<uint8_t, 12> result{203, 205, 200, 208, 28, 1, 42, 57, 18, 16, 46, 19};
    constexpr const char *names[]{"STEERING", "STEERING",  "THROTTLE", "BRAKE",     "SELECT",        "PAUSE",
                                  "NITRO",    "HANDBRAKE", "GEAR_UP",  "GEAR_DOWN", "CHANGE_CAMERA", "REVERSE"};
    for (const auto &map : maps) {
        if (map.name != "DRIVER1" || map.device != "ACTION_MAP")
            continue;
        for (const auto &action : map.actions)
            for (size_t i = 0; i < result.size(); ++i)
                if (action.name == names[i])
                    if (auto key = keyboardCode(i == 1 ? action.control2 : action.control))
                        result[i] = *key;
    }
    return result;
}
} // namespace oj::android
