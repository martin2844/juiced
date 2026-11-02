// Integration tests: the fixed build's controls (Core/Input/PadControls) on the game's controls file: the stock
// scripts\controls_dx9_e.txt with JuicedConfigExt's layout merged in, its keys as the game reads them from the window.

#include "Framework/Fixtures.h"
#include "Framework/Test.h"

#include "Core/Base/GameFiles.h"
#include "Core/Data/Controls.h"
#include "Core/Input/Input.h"
#include "Core/Input/PadControls.h"

#include <cmath>
#include <filesystem>
#include <string_view>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// The menus keep the game's keys (the arrows, Return, Escape), the controller's A selects; DRIVER1's keyboard keys are
// the game's keyboard layout's: the up arrow the throttle, Escape the pause.
OJ_INTEGRATION_TEST(PadControls, StockControlsFile) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const auto bytes = oj::GameFiles(std::filesystem::current_path())
                           .read(std::filesystem::path("scripts") / "controls_dx9_e.txt");
    check(bytes.has_value(), "scripts\\controls_dx9_e.txt read");
    if (!bytes) return;
    ControllerLayout::KeySlot keys;
    oj::input::Input input;
    input.load(oj::parseControls(oj::input::mergeLayout(
        std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()), true, keys)));
    input.pads().setGameKeys(keys);
    check(keys.key[1][0] == "UP" && keys.key[13][0] == "ESCAPE" && keys.key[0][0] == "LEFT" && keys.key[0][1] == "RIGHT",
          "the game's driving keys: the up arrow the throttle, Escape the pause, the arrows the steering");

    input.useUi();
    input.poll({}, true, 0.016f);
    input.uiFrame(0.016f);
    input.poll({{0xD0, true}}, true, 0.016f);
    const auto down = input.uiFrame(0.016f).event;
    input.poll({{0xD0, false}}, true, 0.016f);
    input.uiFrame(0.016f);
    input.poll({{0x1C, true}}, true, 0.016f);
    const auto select = input.uiFrame(0.016f).event;
    input.poll({{0x1C, false}}, true, 0.016f);
    check(down == oj::input::kDown && select == oj::input::kSelect, "the menus: the down arrow, Return");
    oj::input::PadState pad;
    pad.connected = true;
    pad.pad.buttons = 0x1000;  // A
    input.setPad(pad);
    input.poll({}, true, 0.016f);
    check(input.uiFrame(0.016f).event == oj::input::kSelect, "the controller's A selects");
    input.setPad({});

    input.useDriver();
    input.poll({}, true, 0.1f);
    input.poll({{0xC8, true}}, true, 0.1f);
    const float throttle = input.analogue(1);
    input.poll({{0xC8, false}, {0x01, true}}, true, 0.1f);
    check(std::fabs(throttle - 1.0f) < 0.001f && input.digital(0) == 1, "driving: the up arrow the throttle, Escape the pause");
}
