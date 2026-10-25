#ifdef OJ_LINUX
#include "Linux/Paths.h"
#endif
#include "Mac/Controls.h"
#include "Core/Base/Log.h"
#include "Mods/Packages.h"
#include "Mods/Runtime.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#include <utility>

namespace oj::mac {
namespace {
bool requested = false;
#define KEY(name, label, scan) {"DIKEYBOARD_" #name, label, SDL_SCANCODE_##scan}
const BindableKey keys[] = {KEY(A, "A", A),
                            KEY(B, "B", B),
                            KEY(C, "C", C),
                            KEY(D, "D", D),
                            KEY(E, "E", E),
                            KEY(F, "F", F),
                            KEY(G, "G", G),
                            KEY(H, "H", H),
                            KEY(I, "I", I),
                            KEY(J, "J", J),
                            KEY(K, "K", K),
                            KEY(L, "L", L),
                            KEY(M, "M", M),
                            KEY(N, "N", N),
                            KEY(O, "O", O),
                            KEY(P, "P", P),
                            KEY(Q, "Q", Q),
                            KEY(R, "R", R),
                            KEY(S, "S", S),
                            KEY(T, "T", T),
                            KEY(U, "U", U),
                            KEY(V, "V", V),
                            KEY(W, "W", W),
                            KEY(X, "X", X),
                            KEY(Y, "Y", Y),
                            KEY(Z, "Z", Z),
                            KEY(1, "1", 1),
                            KEY(2, "2", 2),
                            KEY(3, "3", 3),
                            KEY(4, "4", 4),
                            KEY(5, "5", 5),
                            KEY(6, "6", 6),
                            KEY(7, "7", 7),
                            KEY(8, "8", 8),
                            KEY(9, "9", 9),
                            KEY(0, "0", 0),
                            KEY(SPACE, "Space", SPACE),
                            KEY(LSHIFT, "Left Shift", LSHIFT),
                            KEY(RSHIFT, "Right Shift", RSHIFT),
                            KEY(LCONTROL, "Left Control", LCTRL),
                            KEY(RCONTROL, "Right Control", RCTRL),
                            KEY(TAB, "Tab", TAB),
                            KEY(BACK, "Backspace", BACKSPACE),
                            KEY(UP, "Up", UP),
                            KEY(DOWN, "Down", DOWN),
                            KEY(LEFT, "Left", LEFT),
                            KEY(RIGHT, "Right", RIGHT),
                            KEY(MINUS, "−", MINUS),
                            KEY(EQUALS, "=", EQUALS),
                            KEY(LBRACKET, "[", LEFTBRACKET),
                            KEY(RBRACKET, "]", RIGHTBRACKET),
                            KEY(HOME, "Home", HOME),
                            KEY(END, "End", END),
                            KEY(PRIOR, "Page Up", PAGEUP),
                            KEY(NEXT, "Page Down", PAGEDOWN),
                            KEY(DELETE, "Delete", DELETE),
                            KEY(F1, "F1", F1),
                            KEY(F2, "F2", F2),
                            KEY(F3, "F3", F3),
                            KEY(F4, "F4", F4),
                            KEY(F5, "F5", F5),
                            KEY(F6, "F6", F6),
                            KEY(F7, "F7", F7),
                            KEY(F8, "F8", F8),
                            KEY(F9, "F9", F9),
                            KEY(F10, "F10", F10),
                            KEY(F11, "F11", F11),
                            KEY(F12, "F12", F12)};
#undef KEY
const BindingAction actions[] = {
    {"throttle", "Accelerate", "Driving", "THROTTLE", false, "W"},
    {"brake", "Brake", "Driving", "BRAKE", false, "S"},
    {"left", "Steer left", "Driving", "STEERING", false, "A"},
    {"right", "Steer right", "Driving", "STEERING", true, "D"},
    {"nitro", "Nitrous (NOS)", "Driving", "NITRO", false, "LSHIFT"},
    {"handbrake", "Handbrake", "Driving", "HANDBRAKE", false, "SPACE"},
    {"reverse", "Reverse", "Driving", "REVERSE", false, "R"},
    {"gear_up", "Shift up (manual)", "Gears & camera", "GEAR_UP", false, "E"},
    {"gear_down", "Shift down (manual)", "Gears & camera", "GEAR_DOWN", false, "Q"},
    {"camera", "Change camera", "Gears & camera", "CHANGE_CAMERA", false, "C"},
    {"rear", "Look behind", "Gears & camera", "REAR_VIEW", false, "B"},
    {"pan_left", "Look left", "Gears & camera", "DRIVER_CAMERA_PAN", false, "Z"},
    {"pan_right", "Look right", "Gears & camera", "DRIVER_CAMERA_PAN", true, "X"},
    {"horn", "Horn", "Gears & camera", "DRIVER_HORN", false, "H"},
    {"crew1_up", "Driver 1: push harder", "Crew commands", "DRIVER_TACTICS_UP", false, "1"},
    {"crew1_down", "Driver 1: ease off", "Crew commands", "DRIVER_TACTICS_DOWN", false, "2"},
    {"crew2_up", "Driver 2: push harder", "Crew commands", "DRIVER_TACTICS_RIGHT", false, "3"},
    {"crew2_down", "Driver 2: ease off", "Crew commands", "DRIVER_TACTICS_LEFT", false, "4"},
    {"crew3_up", "Driver 3: push harder", "Crew commands", "DRIVER_TACTICS3_UP", false, "5"},
    {"crew3_down", "Driver 3: ease off", "Crew commands", "DRIVER_TACTICS3_DOWN", false, "6"},
    {"next_car", "Next car (spectator)", "Crew commands", "NEXTCAR", false, "NEXT"}};
} // namespace
void requestControlsEditor() {
    requested = !mods::packages::available() || mods::runtime::feature("controls");
}
bool takeControlsRequest() {
    return std::exchange(requested, false);
}
std::span<const BindableKey> bindableKeys() {
    return keys;
}
std::span<const BindingAction> bindingActions() {
    return actions;
}
const BindableKey *bindingKey(std::string_view semantic) {
    for (const auto &key : keys)
        if (semantic == key.semantic)
            return &key;
    return nullptr;
}
uint8_t gameScanCode(SDL_Scancode scan) {
    for (const auto &key : keys)
        if (key.scan == scan)
            return keyboardCode(key.semantic).value_or(0);
    switch (scan) {
    case SDL_SCANCODE_ESCAPE:
        return 1;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        return 28;
    case SDL_SCANCODE_CAPSLOCK:
        return 58;
    case SDL_SCANCODE_LALT:
        return 56;
    case SDL_SCANCODE_RALT:
        return 184;
    default:
        return 0;
    }
}
Bindings defaultBindings() {
    Bindings result;
    for (const auto &action : actions)
        result.push_back(mods::runtime::text("control_default", {std::string(action.id)},
                                             std::string("DIKEYBOARD_") + action.defaultKey));
    if (!validBindings(result)) {
        result.clear();
        for (const auto &action : actions)
            result.push_back(std::string("DIKEYBOARD_") + action.defaultKey);
    }
    return result;
}
bool validBindings(const Bindings &values) {
    if (values.size() != std::size(actions))
        return false;
    std::set<std::string> seen;
    for (const auto &key : values)
        if (!bindingKey(key) || !keyboardCode(key) || !seen.insert(key).second)
            return false;
    return true;
}
std::string rebind(Bindings &values, size_t action, std::string_view key) {
    if (action >= values.size() || !bindingKey(key))
        return "Choose a letter, number, arrow, Shift, Control or function key.";
    std::string message = std::string(actions[action].label) + " -> " + bindingKey(key)->label;
    const auto conflict = std::find(values.begin(), values.end(), key);
    if (conflict != values.end() && static_cast<size_t>(conflict - values.begin()) != action) {
        const size_t other = conflict - values.begin();
        values[other] = values[action];
        message += std::string(". Swapped with ") + actions[other].label + ".";
    }
    values[action] = key;
    return message;
}
std::filesystem::path bindingsPath() {
#ifdef OJ_ANDROID
    const char* root=std::getenv("JUICED_SUPPORT_DIR");
    return std::filesystem::path(root?root:".") / "controls.ini";
#elif defined(OJ_LINUX)
    return oj::linux_port::configPath() / "controls.ini";
#else
    const char *home = std::getenv("HOME");
    return std::filesystem::path(home ? home : ".") / "Library/Application Support/Juiced/controls.ini";
#endif
}
Bindings loadBindings(const std::filesystem::path &path) {
    Bindings result = defaultBindings();
    std::ifstream file(path);
    if (!file)
        return result;
    std::set<std::string> found;
    std::string line;
    bool bad = false;
    while (std::getline(file, line)) {
        if (line.empty() || line.front() == '#')
            continue;
        const auto equal = line.find('=');
        if (equal == std::string::npos) {
            bad = true;
            break;
        }
        const auto id = line.substr(0, equal);
        auto key = line.substr(equal + 1);
        if (!key.empty() && key.back() == '\r')
            key.pop_back();
        auto at = std::find_if(std::begin(actions), std::end(actions), [&](const auto &a) { return id == a.id; });
        if (at == std::end(actions) || !found.insert(id).second) {
            bad = true;
            break;
        }
        result[at - std::begin(actions)] = key;
    }
    if (bad || file.bad() || found.size() != std::size(actions) || !validBindings(result)) {
        log::warning("controls: invalid file {}; using modern defaults", path.string());
        return defaultBindings();
    }
    return result;
}
bool saveBindings(const std::filesystem::path &path, const Bindings &values, std::string &error) {
    if (!validBindings(values)) {
        error = "Some bindings are invalid or duplicated.";
        return false;
    }
    const auto temporary = std::filesystem::path(path.string() + ".tmp");
    try {
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path());
        std::ofstream file(temporary, std::ios::trunc);
        file.exceptions(std::ios::badbit | std::ios::failbit);
        file << "# Juiced keyboard controls v1\n";
        for (size_t i = 0; i < values.size(); ++i)
            file << actions[i].id << '=' << values[i] << '\n';
        file.close();
        std::filesystem::rename(temporary, path);
        return true;
    } catch (const std::exception &e) {
        error = std::string("Could not save controls: ") + e.what();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
}
void applyBindings(std::vector<ActionMap> &maps, const Bindings &values) {
    if (!validBindings(values))
        return;
    for (auto &map : maps) {
        if (map.device != "ACTION_MAP" || map.name.rfind("DRIVER", 0) != 0)
            continue;
        for (auto &action : map.actions) {
            if (!keyboardCode(action.control))
                continue;
            for (size_t i = 0; i < values.size(); ++i) {
                if (action.name != actions[i].action)
                    continue;
                auto &control = actions[i].second ? action.control2 : action.control;
                auto &text = actions[i].second ? action.control2Text : action.controlText;
                if (control.empty())
                    continue;
                control = values[i];
                auto label = splitControl(text).label;
                text = control + " '" + label + "'";
            }
        }
    }
}
} // namespace oj::mac
