// Unit tests: the fixed build's controls (Core/Input/PadControls): JuicedConfigExt's layout natively. The controls
// file with the layout's controls (ControlsFile), the controller's and the keys' values through the game's bindings
// (PadActions; the plugin's XInputBindings): DRIVER1's JCE_<ACTION> controls, the menus' GAMEPAD_* ones; a steering
// wheel of [WheelMap] on the same controls (WheelReader).

#include "Framework/Test.h"

#include "Core/Data/Controls.h"
#include "Core/Input/Input.h"
#include "Core/Input/PadControls.h"
#include "Core/Race/DriverControl.h"
#include "Core/Saves/SaveEntries.h"

#include <WheelReader.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace {

using oj::test::check;

// A controls file as the game's: the menus' keys, DRIVER1's keys and a DirectInput axis.
constexpr const char* kControls =
    "[BeginActionMap]\nName UI\nDevice ACTION_MAP\nGUID 0x1-0x2-0x3-0x4-0x5-0x6-0x7-0x8-0x9-0xA-0xB\n"
    "[BeginAction]\nName SELECT\nType DIGITAL\nOneShot\nControl \"DIKEYBOARD_RETURN 'Select'\"\n[EndAction]\n"
    "[BeginAction]\nName BACK\nType DIGITAL\nOneShot\nControl \"DIKEYBOARD_ESCAPE 'Back'\"\n[EndAction]\n"
    "[EndActionMap]\n"
    "[BeginActionMap]\nName DRIVER1\nDevice ACTION_MAP\nGUID 0x1-0x2-0x3-0x4-0x5-0x6-0x7-0x8-0x9-0xA-0xC\n"
    "[BeginAction]\nName STEERING\nType ANALOGUE\nControl \"DIKEYBOARD_LEFT 'Left' APPFIXED\"\n"
    "Control2 \"DIKEYBOARD_RIGHT 'Right' APPFIXED\"\nAttack 1.5\nDecay 3.0\n[EndAction]\n"
    "[BeginAction]\nName THROTTLE\nType ANALOGUE\nControl \"DIKEYBOARD_UP 'Throttle' APPFIXED\"\n[EndAction]\n"
    "[BeginAction]\nName PAUSE\nType DIGITAL\nOneShot\nControl \"DIKEYBOARD_ESCAPE 'Pause' APPFIXED\"\n[EndAction]\n"
    "[BeginAction]\nName STEERING\nType ANALOGUE\nControl \"DIAXIS_DRIVINGR_STEER 'Steering'\"\n[EndAction]\n"
    "[EndActionMap]\n";

const oj::ActionMap* mapOf(const std::vector<oj::ActionMap>& maps, const char* name) {
    for (const oj::ActionMap& m : maps) {
        if (m.name == name) return &m;
    }
    return nullptr;
}

size_t countControl(const oj::ActionMap& m, const char* action, const char* control) {
    size_t n = 0;
    for (const oj::ControlAction& a : m.actions) {
        if (a.name == action && a.control == control) ++n;
    }
    return n;
}

bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

oj::input::PadState pad(uint32_t buttons, float leftX, float rightTrigger) {
    oj::input::PadState s;
    s.connected = true;
    s.pad.buttons = buttons;
    s.pad.axes[PadActions::kLeftX] = leftX;
    s.pad.axes[PadActions::kRightTrigger] = rightTrigger;
    return s;
}

constexpr uint32_t kA = 0x1000;  // XINPUT_GAMEPAD_A

}  // namespace

// ControlsLayout: DRIVER1's keyboard actions out to the game's keyboard layout (their keys, Attack and Decay), the
// DirectInput action left; every driving action as its JCE_<ACTION> (an analogue one Deadzone 0); the menus the
// controller's (A select, B back) and the keyboard layout's second keys; the game's keys of the menus stay.
OJ_UNIT_TEST(PadControls, MergedControlsFile) {
    ControllerLayout::KeySlot keys;
    const std::vector<oj::ActionMap> maps = oj::parseControls(oj::input::mergeLayout(kControls, true, keys));
    const oj::ActionMap* ui = mapOf(maps, "UI");
    const oj::ActionMap* driver = mapOf(maps, "DRIVER1");
    check(ui != nullptr && driver != nullptr, "both maps");
    if (ui == nullptr || driver == nullptr) return;
    check(countControl(*driver, "STEERING", "DIKEYBOARD_LEFT") == 0 && countControl(*driver, "THROTTLE", "DIKEYBOARD_UP") == 0 &&
              countControl(*driver, "PAUSE", "DIKEYBOARD_ESCAPE") == 0,
          "DRIVER1's keyboard actions taken out");
    check(countControl(*driver, "STEERING", "DIAXIS_DRIVINGR_STEER") == 1, "the DirectInput action left");
    check(countControl(*driver, "STEERING", "JCE_STEERING") == 1 && countControl(*driver, "PAUSE", "JCE_PAUSE") == 1 &&
              countControl(*driver, "NEXTCAR", "JCE_NEXTCAR") == 1,
          "every driving action as its JCE_<ACTION>");
    bool zone = false;
    for (const oj::ControlAction& a : driver->actions) {
        if (a.control == "JCE_THROTTLE") zone = a.analogue && a.deadzone == 0.0f;
    }
    check(zone, "an analogue one with Deadzone 0");
    check(keys.key[0][0] == "LEFT" && keys.key[0][1] == "RIGHT" && keys.attack[0] == "1.5" && keys.decay[0] == "3.0",
          "the steering's keys, Attack and Decay to the keyboard layout");
    check(keys.key[1][0] == "UP" && keys.attack[1].empty() && keys.key[13][0] == "ESCAPE", "the throttle's, the pause's");
    check(countControl(*ui, "SELECT", "GAMEPAD_A") == 1 && countControl(*ui, "BACK", "GAMEPAD_B") == 1 &&
              countControl(*ui, "SELECT", "JCE_MENU_SELECT") == 1 && countControl(*ui, "SELECT", "DIKEYBOARD_RETURN") == 1,
          "the menus: the controller's, the second keys and the game's");
    ControllerLayout::KeySlot noPadKeys;
    const std::vector<oj::ActionMap> noPad = oj::parseControls(oj::input::mergeLayout(kControls, false, noPadKeys));
    const oj::ActionMap* uiNoPad = mapOf(noPad, "UI");
    check(uiNoPad != nullptr && countControl(*uiNoPad, "SELECT", "GAMEPAD_A") == 0 &&
              countControl(*uiNoPad, "SELECT", "JCE_MENU_SELECT") == 1,
          "the controller off: the second keys alone");
}

// DRIVER1 of the merged file on the device of the keyboard: the auto layout of an Xbox controller (STEERING the left
// stick, THROTTLE the right trigger, each past the game's dead zone of 0.2 rescaled to 1), the keys of the game's
// layout (the throttle's up arrow the stronger), PAUSE the escape key; a PlayStation controller the PlayStation 2 layout.
OJ_UNIT_TEST(PadControls, DrivingActions) {
    ControllerLayout::KeySlot keys;
    oj::input::Input input;
    input.load(oj::parseControls(oj::input::mergeLayout(kControls, true, keys)));
    input.pads().setGameKeys(keys);
    input.pads().setLayout(ControllerLayout::Layout{});
    input.useDriver();
    input.setPad(pad(0, -0.5f, 0.6f));
    input.poll({}, true, 0.1f);
    check(near(input.analogue(1), 0.5f), "THROTTLE: the right trigger 0.6 past 0.2, rescaled: 0.5");
    check(near(input.analogue(0), -0.375f), "STEERING: the left stick -0.5: -0.375");
    check(input.pads().steeringFromController() && oj::DriverKeys{}.update(input).steeringFromController,
          "the stick steers: the controller's (DriverControl's Xbox steering)");
    input.poll({{0xC8, true}}, true, 0.1f);
    check(near(input.analogue(1), 1.0f), "the up arrow down: the keys' 1, the stronger");
    check(input.digital(0) == 0, "PAUSE off");
    input.poll({{0xC8, false}, {0x01, true}}, true, 0.1f);
    check(input.digital(0) == 1 && near(input.analogue(1), 0.5f), "the escape key: PAUSE; the trigger again");
    // The left arrow: the steering's keys from 0 at their Attack (1.5 a second), the stick's -0.375 the stronger yet.
    input.poll({{0x01, false}, {0xCB, true}}, true, 0.1f);
    check(near(input.analogue(0), -0.375f), "the left arrow's -0.15 after 0.1 s, the stick's -0.375 stronger");
    input.setPad(pad(0, 0.0f, 0.0f));
    input.poll({}, true, 0.3f);
    check(near(input.analogue(0), -0.6f), "the stick at rest: the keys' -0.6 after 0.4 s");
    check(!input.pads().steeringFromController() && !oj::DriverKeys{}.update(input).steeringFromController,
          "the keys steer: not the controller's");
    input.poll({{0xCB, false}}, true, 0.1f);
    check(near(input.analogue(0), -0.3f), "the key let go: back at the Decay (3 a second)");
    oj::input::PadState ps = pad(0, 0.0f, 0.6f);
    ps.playStation = true;
    input.setPad(ps);
    input.poll({}, true, 0.1f);
    check(near(input.analogue(1), 0.0f) && input.pads().playStation(),
          "a PlayStation controller: the PlayStation 2 layout (THROTTLE on cross), the trigger nothing");
}

// The menus (UI) of the merged file: the controller's A selects (OneShot: once), its press takes the player's
// device as Return does (the plugin's PadKeys); the keys of the game stay.
OJ_UNIT_TEST(PadControls, MenuActions) {
    ControllerLayout::KeySlot keys;
    oj::input::Input input;
    input.load(oj::parseControls(oj::input::mergeLayout(kControls, true, keys)));
    input.pads().setGameKeys(keys);
    input.setPad(pad(kA, 0.0f, 0.0f));
    input.poll({}, true, 0.016f);
    input.poll({}, true, 0.016f);
    check(input.playerTaken(), "A takes the player's device");
    input.setPad(pad(0, 0.0f, 0.0f));
    input.useUi();
    input.poll({}, true, 0.016f);
    const bool idle = input.uiFrame(0.016f).event == oj::input::kNone;
    input.setPad(pad(kA, 0.0f, 0.0f));
    input.poll({}, true, 0.016f);
    const auto first = input.uiFrame(0.016f).event;
    input.poll({}, true, 0.016f);
    const auto held = input.uiFrame(0.016f).event;
    check(idle && first == oj::input::kSelect && held == oj::input::kNone, "A: SELECT once while held");
    input.setPad(pad(0, 0.0f, 0.0f));
    input.poll({{0x1C, true}}, true, 0.016f);
    check(input.uiFrame(0.016f).event == oj::input::kSelect, "Return still selects");
    input.setPad(pad(0, -0.9f, 0.0f));
    input.poll({{0x1C, false}}, true, 0.016f);
    check(input.uiFrame(0.016f).event == oj::input::kLeft, "the left stick past the half: LEFT");
}

// A steering wheel of controller.ini's [WheelMap] (WheelReader, its states given): DRIVER1's JCE_STEERING its X axis
// (the strongest with no controller, -1 .. 1 around the centre), JCE_THROTTLE its Y axis from its rest (1) to its end
// (-1); with the game's steering wheel setting STEERING through the response table (+0xAC 0: -20), past it with
// [Wheel] linearInput; the menus' JCE_MENU_SELECT its button 1, which takes the player's device as A does.
#if defined(_WIN32) // The upstream reader exercises DirectInput; SDL has separate controller tests.
OJ_UNIT_TEST(PadControls, WheelActions) {
    const std::filesystem::path ini = std::filesystem::temp_directory_path() / "oj_padcontrols_wheel.ini";
    {
        std::ofstream f(ini, std::ios::binary | std::ios::trunc);
        f << "[WheelMap]\r\nSTEERING = axis X 0.000 1.000 | Wheel\r\nTHROTTLE = axis Y 1.000 -1.000 | Wheel\r\n"
             "MENU_SELECT = button 1 | Wheel\r\n";
    }
    WheelReader::Reader wheel;
    wheel.SetMapForTests(ini.wstring().c_str());
    float axes[8] = {0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    bool buttons[128] = {};
    wheel.SetStateForTests("Wheel", axes, buttons, nullptr);

    ControllerLayout::KeySlot keys;
    oj::input::Input input;
    input.load(oj::parseControls(oj::input::mergeLayout(kControls, true, keys)));
    input.pads().setGameKeys(keys);
    input.pads().setWheel(&wheel);
    input.setPad(oj::input::PadState{});
    input.poll({}, true, 0.016f);
    check(!input.playerTaken(), "the wheel at rest takes no device");
    buttons[0] = true;
    wheel.SetStateForTests("Wheel", nullptr, buttons, nullptr);
    input.poll({}, true, 0.016f);
    input.poll({}, true, 0.016f);
    check(input.playerTaken(), "its button 1 (MENU_SELECT) takes the player's device");
    input.useUi();
    buttons[0] = false;
    wheel.SetStateForTests("Wheel", nullptr, buttons, nullptr);
    input.poll({}, true, 0.016f);
    const bool idle = input.uiFrame(0.016f).event == oj::input::kNone;
    buttons[0] = true;
    wheel.SetStateForTests("Wheel", nullptr, buttons, nullptr);
    input.poll({}, true, 0.016f);
    check(idle && input.uiFrame(0.016f).event == oj::input::kSelect, "the menus: button 1 selects");
    buttons[0] = false;
    wheel.SetStateForTests("Wheel", nullptr, buttons, nullptr);

    input.useDriver();
    input.poll({}, true, 0.1f);
    check(near(input.analogue(0), 0.5f) && near(input.analogue(1), 1.0f),
          "DRIVER1: STEERING the X axis' 0.5, THROTTLE the Y axis at its end: 1");
    std::optional<oj::save::Settings> settings = oj::save::Settings::defaults(7);
    settings->setU32(oj::save::Settings::kSteeringWheel, 1);
    input.setSettings(&settings);
    input.useDriver();
    input.poll({}, true, 0.1f);
    const float curved = input.analogue(0);
    input.pads().setLinearSteering(true);
    input.poll({}, true, 0.1f);
    const float linear = input.analogue(0);
    std::printf("     wheel: steering %.4f through the table, %.4f linear\n", curved, linear);
    // 0.5 x 100 + 100, x 5.12: the table's entry 768 (x = 257 / 511).
    const float entry = static_cast<float>(oj::input::responseTable(-20)[768]) * 0.0001f;
    check(near(curved, entry) && near(linear, 0.5f),
          "the steering wheel's STEERING through the response table (-20), linear past it");
    std::filesystem::remove(ini);
}

#endif
