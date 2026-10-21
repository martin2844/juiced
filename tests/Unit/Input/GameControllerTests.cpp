// Unit tests: GameController.

#include "Framework/Test.h"

#include "Core/Data/Controls.h"
#include "Core/Input/Input.h"
#include "Core/Race/DriverControl.h"
#include "Core/Saves/SaveEntries.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <utility>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// A game controller's device (0x00594950 with a controller): DRIVER1 of pad semantics registered (0x0058F890,
// 0x005954D0) gives the controller its DIACTIONFORMAT (one DIACTION a semantic, uAppData the value's index); its
// buffered data set the values; the bindings step at the poll: an axis's ANALOGUE action past the dead zone,
// scaled and followed by Attack (0x00590260), the second axis less the first (0x0058EA50), a button's DIGITAL
// action (0x005900F0) and an axis's DIGITAL one past the dead zone (0x00590130; OneShot, made as held, so no
// edge until it is let go).
OJ_UNIT_TEST(GameController, Driver1Pad) {
    struct FakePort : oj::input::ControllerPort {
        oj::input::MapFormat format;
        bool mapped = false;
        bool raw = false;
        std::vector<std::pair<uint32_t, uint32_t>> next;
        std::array<uint8_t, 128> buttons{};
        void unacquire() override {}
        void setActionMap(const oj::input::MapFormat& f) override {
            format = f;
            mapped = true;
        }
        void setRawFormat() override { raw = true; }
        oj::input::ControllerRead read(bool asMapped) override {
            oj::input::ControllerRead r;
            if (asMapped) r.events = std::exchange(next, {});
            r.buttons = buttons;
            return r;
        }
        void recreate() override {}
    };
    const std::vector<oj::ActionMap> padMaps = oj::parseControls(
        "[BeginActionMap]\nName DRIVER1\nDevice ACTION_MAP\nGUID 0x1-0x2-0x3-0x4-0x5-0x6-0x7-0x8-0x9-0xA-0xB\n"
        "[BeginAction]\nName STEERING\nType ANALOGUE\nControl \"DIAXIS_DRIVINGR_STEER 'Steering'\"\n"
        "Deadzone 0.1\nAttack 5\nDecay 5\n[EndAction]\n"
        "[BeginAction]\nName THROTTLE\nType ANALOGUE\nSign NEGATIVE\n"
        "Control \"DIAXIS_DRIVINGR_ACCEL_AND_BRAKE 'Throttle'\"\n[EndAction]\n"
        "[BeginAction]\nName BRAKE\nType ANALOGUE\nControl \"DIAXIS_DRIVINGR_ACCEL_AND_BRAKE 'Brake'\"\n[EndAction]\n"
        "[BeginAction]\nName DRIVER_CAMERA_PAN\nType ANALOGUE\nControl \"DIAXIS_ANY_X_2 'Left'\"\n"
        "Control2 \"DIAXIS_ANY_Y_2 'Right'\"\n[EndAction]\n"
        "[BeginAction]\nName NITRO\nType DIGITAL\nControl \"DIBUTTON_ANY(2) 'Nitro'\"\n[EndAction]\n"
        "[BeginAction]\nName CHANGE_CAMERA\nType DIGITAL\nOneShot\nControl \"DIAXIS_ANY_Y_1 'View'\"\n"
        "Deadzone 0.5\n[EndAction]\n"
        "[BeginAction]\nName PAUSE\nType DIGITAL\nControl \"DIKEYBOARD_ESCAPE 'Pause' APPFIXED\"\n[EndAction]\n"
        "[EndActionMap]\n");
    FakePort port;
    oj::input::Input pad;
    pad.load(padMaps);
    pad.setControllers({&port});
    // Raw: a button down takes the device as the first player's at the next update (0x00594E20's buttons).
    port.buttons[3] = 0x80;
    pad.poll({});
    pad.poll({});
    const bool padTaken = pad.playerTaken();
    port.buttons[3] = 0;
    pad.useDriver();
    const oj::input::MapFormat& f = port.format;
    const bool format = port.mapped && f.name == "DRIVER1" && f.actions.size() == 7 && f.genre == 0x01000000 &&
                        f.axisMin == -100 && f.axisMax == 100 && f.bufferSize == 32 && f.guid[0] == 1 &&
                        f.guid[4] == 2 && f.guid[15] == 0x0B && f.actions[0].semantic == 0x01008A01u &&
                        f.actions[0].label == "Steering" && f.actions[1].semantic == 0x01014A04u &&
                        f.actions[6].semantic == 0x81000401u && f.actions[6].flags == 0x10 &&
                        f.actions[6].appData == 6;
    // Steering at -60 of -100 .. 100: past the dead zone 0.1 by 0.5, scaled by 1 / 0.9, reached at 5 a second.
    port.next = {{0, static_cast<uint32_t>(-60)}, {1, static_cast<uint32_t>(-80)}, {4, 0x80},
                 {2, static_cast<uint32_t>(-20)}, {3, 30}, {5, 80}};
    oj::DriverKeys keys;
    pad.poll({}, true, 0.05f);
    const oj::DriverActions a1 = keys.update(pad);
    pad.poll({}, true, 0.05f);
    const oj::DriverActions a2 = keys.update(pad);
    std::printf("     pad: steering %.4f %.4f, throttle %.3f, brake %.3f, pan %.2f, nitro %d, view %d %d\n",
                a1.steering, a2.steering, a2.throttle, a2.brake, a2.cameraPan, a2.nitro ? 1 : 0,
                a1.changeView ? 1 : 0, a2.changeView ? 1 : 0);
    const auto closeTo = [](float a, float b) { return std::fabs(a - b) < 1e-4f; };
    check(padTaken && format && closeTo(a1.steering, -0.25f) && closeTo(a2.steering, -0.5f) &&
              closeTo(a2.throttle, 0.75f) && closeTo(a2.brake, -0.75f) && closeTo(a2.cameraPan, 0.5f) &&
              a2.nitro && !a1.changeView && !a2.changeView && pad.device(0).connected == 1,
          "game controller: the map's format, its events, the pad's bindings and actions");
    // The steering wheel's setting (+0x78): STEERING's axis through the response table (+0xAC 0: -20).
    std::optional<oj::save::Settings> settings = oj::save::Settings::defaults(7);
    settings->setU32(oj::save::Settings::kSteeringWheel, 1);
    pad.setSettings(&settings);
    pad.useDriver();
    port.next = {{0, static_cast<uint32_t>(-100)}};
    pad.poll({}, true, 1.0f);
    const float full = keys.update(pad).steering;
    // Inactive: nothing read, not connected, the actions keep their values.
    port.next = {{0, 0}};
    pad.poll({}, false, 1.0f);
    const bool inactive = pad.device(0).connected == 0 && keys.update(pad).steering == full;
    check(pad.wheel() && closeTo(full, -1.0f) && inactive,
          "game controller: the wheel's steering through the response table; nothing read while inactive");
}
