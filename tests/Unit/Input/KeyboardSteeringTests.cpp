// Unit tests: KeyboardSteering.

#include "Framework/Test.h"

#include "Core/Data/Controls.h"
#include "Core/Input/Input.h"
#include "Core/Race/DriverControl.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// The keys' steering of DRIVER1 (an ANALOGUE action of two keys, 0x00590360): Attack 1.5 a second while a key
// is down, Decay 3 a second toward 0; the throttle's one key (0x00590180) with the default 1000 at once.
OJ_UNIT_TEST(KeyboardSteering, Driver1Keys) {
    const std::vector<oj::ActionMap> maps = oj::parseControls(
        "[BeginActionMap]\nName DRIVER1\nDevice ACTION_MAP\n"
        "[BeginAction]\nName STEERING\nType ANALOGUE\nControl \"DIKEYBOARD_LEFT 'L'\"\n"
        "Control2 \"DIKEYBOARD_RIGHT 'R'\"\nAttack 1.5\nDecay 3.0\n[EndAction]\n"
        "[BeginAction]\nName THROTTLE\nType ANALOGUE\nControl \"DIKEYBOARD_UP 'T'\"\n[EndAction]\n"
        "[EndActionMap]\n");
    oj::DriverKeys keys;
    oj::input::Input device;
    device.load(maps);
    device.useDriver();
    const uint8_t left = *oj::keyboardCode("DIKEYBOARD_LEFT");
    const uint8_t up = *oj::keyboardCode("DIKEYBOARD_UP");
    std::vector<float> steer;
    float throttle = 0.0f;
    for (int i = 0; i < 6; ++i) {
        // One buffered key event a poll: the throttle's key goes down at the second.
        std::vector<oj::input::KeyEvent> events;
        if (i == 0) events.push_back({left, true});
        if (i == 0) events.push_back({up, true});
        if (i == 4) events.push_back({left, false});
        device.poll(events, true, 0.1f);
        const oj::DriverActions a = keys.update(device);
        steer.push_back(a.steering);
        if (i == 1) throttle = a.throttle;
    }
    const auto same = [](float a, float b) { return std::fabs(a - b) < 1e-5f; };
    std::printf("     keys' steering: %.3f %.3f %.3f %.3f %.3f %.3f\n", steer[0], steer[1], steer[2], steer[3],
                steer[4], steer[5]);
    check(same(steer[0], -0.15f) && same(steer[3], -0.6f) && same(steer[4], -0.3f) && same(steer[5], 0.0f) &&
              throttle == 1.0f,
          "driver keys: the steering's keys rise by Attack and fall by Decay; the throttle's key at once");
}
