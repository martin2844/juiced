#include "Mac/Controls.h"
#include "Core/Base/GameFiles.h"
#include "Core/Input/Input.h"
#include "Core/Race/DriverControl.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using namespace oj;
using namespace oj::mac;
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    const auto folder = std::filesystem::temp_directory_path() / ("juiced-controls-" + std::to_string(getpid()));
    try {
        auto values = defaultBindings();
        require(validBindings(values), "Default bindings must be complete and conflict-free");
        for (const auto &k : bindableKeys())
            require(gameScanCode(k.scan) == keyboardCode(k.semantic).value_or(0) && gameScanCode(k.scan) != 0,
                    "Editor key must produce the game's matching scan code");
        auto changed = values;
        rebind(changed, 0, "DIKEYBOARD_S");
        require(changed[0] == "DIKEYBOARD_S" && changed[1] == "DIKEYBOARD_W" && validBindings(changed),
                "Conflicts swap assignments");
        rebind(changed, 0, "DIKEYBOARD_ESCAPE");
        require(changed[0] == "DIKEYBOARD_S", "Escape cannot replace a driving binding");
        std::string error;
        const auto file = folder / "controls.ini";
        require(saveBindings(file, changed, error), "Save custom bindings");
        require(loadBindings(file) == changed, "Load custom bindings across sessions");
        auto invalid = values;
        invalid[0] = invalid[1];
        require(!saveBindings(file, invalid, error) && loadBindings(file) == changed,
                "Invalid save preserves last good configuration");
        require(!saveBindings(file / "blocked", values, error), "Filesystem errors reported");
        require(saveBindings(file, values, error) && loadBindings(file) == values, "Restore defaults persists");
        {
            std::ofstream(file) << "throttle=DIKEYBOARD_ESCAPE\n";
        }
        require(loadBindings(file) == values, "Malformed config falls back to safe defaults");
        std::vector<ActionMap> maps;
        if (argc == 2) {
            const auto bytes = GameFiles(argv[1]).read("scripts/controls_dx9_e.txt");
            require(bytes.has_value(), "Retail controls fixture exists");
            maps = parseControls(std::string_view(reinterpret_cast<const char *>(bytes->data()), bytes->size()));
        } else {
            maps = parseControls(
                "[BeginActionMap]\nName DRIVER1\nDevice ACTION_MAP\n"
                "[BeginAction]\nName STEERING\nType ANALOGUE\nControl \"DIKEYBOARD_LEFT 'Left'\"\nControl2 "
                "\"DIKEYBOARD_RIGHT 'Right'\"\nAttack 1.5\nDecay 3\n[EndAction]\n"
                "[BeginAction]\nName THROTTLE\nType ANALOGUE\nControl \"DIKEYBOARD_UP 'Throttle'\"\n[EndAction]\n"
                "[BeginAction]\nName BRAKE\nType ANALOGUE\nControl \"DIKEYBOARD_DOWN 'Brake'\"\n[EndAction]\n"
                "[BeginAction]\nName HANDBRAKE\nType ANALOGUE\nControl \"DIKEYBOARD_B 'Handbrake'\"\n[EndAction]\n"
                "[BeginAction]\nName NITRO\nType DIGITAL\nControl \"DIKEYBOARD_RSHIFT 'Nitro'\"\n[EndAction]\n"
                "[BeginAction]\nName REAR_VIEW\nType DIGITAL\nControl \"DIKEYBOARD_SPACE "
                "'Rear'\"\n[EndAction]\n[EndActionMap]\n");
        }
        const auto original = maps;
        applyBindings(maps, values);
        if (argc == 2) {
            size_t found = 0;
            for (size_t i = 0; i < bindingActions().size(); ++i) {
                const auto &row = bindingActions()[i];
                bool present = false;
                for (const auto &map : maps)
                    if (map.name == "DRIVER1" && map.device == "ACTION_MAP")
                        for (const auto &a : map.actions)
                            if (a.name == row.action)
                                present |= (row.second ? a.control2 : a.control) == values[i];
                if (present)
                    ++found;
            }
            require(found == bindingActions().size(), "Every editor action must bind a real retail action");
            for (size_t i = 0; i < maps.size(); ++i)
                if (maps[i].name == "UI")
                    for (size_t j = 0; j < maps[i].actions.size(); ++j)
                        require(maps[i].actions[j].control == original[i].actions[j].control,
                                "Menus retain their bindings");
        }
        auto sample = [&](std::initializer_list<SDL_Scancode> keys) {
            input::Input input;
            input.load(maps);
            input.useDriver();
            input.poll({}, true, 0.1f);
            std::vector<input::KeyEvent> events;
            for (auto key : keys)
                events.push_back({gameScanCode(key), true});
            input.poll(events, true, 0.1f);
            for (int n = 0; n < 5; ++n)
                input.poll({}, true, 0.1f);
            return DriverKeys{}.update(input);
        };
        auto a = sample({SDL_SCANCODE_W, SDL_SCANCODE_A});
        require(a.throttle == 1 && a.steering < 0 && a.brake == 0, "W accelerates and A steers left simultaneously");
        require(sample({SDL_SCANCODE_S, SDL_SCANCODE_D}).brake == 1 && sample({SDL_SCANCODE_D}).steering > 0,
                "S brakes and D steers right");
        a = sample({SDL_SCANCODE_LSHIFT, SDL_SCANCODE_SPACE});
        require(a.nitro && a.handbrake == 1 && !a.rearView,
                "Shift boosts and Space handbrakes without activating rear view");
        applyBindings(maps, changed);
        require(sample({SDL_SCANCODE_S}).throttle == 1 && sample({SDL_SCANCODE_W}).brake == 1,
                "Edited bindings drive real actions immediately");
        std::filesystem::remove_all(folder);
        std::cout << "Controls persistence, conflict, scan-code and driver-action checks passed\n";
    } catch (const std::exception &e) {
        std::filesystem::remove_all(folder);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
