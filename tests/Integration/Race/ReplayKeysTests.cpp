// Integration tests: the fixed build's replay keys (Game/Race/TrackView replayKeys, Core/Input Input::driverDown): the
// race's CHANGE_CAMERA and REAR_VIEW of the driver's map in hud_replay, on the game's controls file with
// JuicedConfigExt's layout merged in.

#include "Framework/Fixtures.h"
#include "Framework/Test.h"

#include "Core/Base/GameFiles.h"
#include "Core/Data/Controls.h"
#include "Core/Input/Input.h"
#include "Core/Input/PadControls.h"
#include "Game/Race/TrackView/TrackView.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

namespace {

using oj::test::check;

constexpr uint8_t kKeyV = 0x2F;      // DIK_V: Change View
constexpr uint8_t kKeySpace = 0x39;  // DIK_SPACE: Rear View

// The input manager on scripts\controls_dx9_e.txt merged with the default layout (Auto: the Xbox version's, Change
// View on LB), its first player's controller on the UI map as hud_replay has it.
bool loadInput(oj::input::Input& input) {
    const auto bytes = oj::GameFiles(std::filesystem::current_path())
                           .read(std::filesystem::path("scripts") / "controls_dx9_e.txt");
    check(bytes.has_value(), "scripts\\controls_dx9_e.txt read");
    if (!bytes) return false;
    ControllerLayout::KeySlot keys;
    input.load(oj::parseControls(oj::input::mergeLayout(
        std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()), true, keys)));
    input.pads().setGameKeys(keys);
    input.useUi();
    input.poll({}, true, 0.016f);
    return true;
}

}  // namespace

// Under the UI map the driver's slots read nothing, while the driver's map's controls are read as they are: V and
// Space of the keyboard, the controller's LB (Change View in the Auto layout).
OJ_INTEGRATION_TEST(ReplayKeys, DriverMapUnderUi) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::input::Input input;
    if (!loadInput(input)) return;
    input.poll({{kKeyV, true}}, true, 0.016f);
    const bool change = input.driverDown("CHANGE_CAMERA") && input.digital(5) == 0 && !input.driverDown("REAR_VIEW");
    input.poll({{kKeyV, false}, {kKeySpace, true}}, true, 0.016f);
    const bool rear = input.driverDown("REAR_VIEW") && !input.driverDown("CHANGE_CAMERA") && input.digital(2) == 0;
    input.poll({{kKeySpace, false}}, true, 0.016f);
    const bool none = !input.driverDown("REAR_VIEW") && !input.driverDown("CHANGE_CAMERA");
    oj::input::PadState pad;
    pad.connected = true;
    pad.pad.buttons = 0x0100;  // LB
    input.setPad(pad);
    input.poll({}, true, 0.016f);
    const bool lb = input.driverDown("CHANGE_CAMERA");
    input.setPad({});
    input.poll({}, true, 0.016f);
    check(change, "V down under the UI map: Change View (the driver's slot 0)");
    check(rear && none, "Space held: Rear View, none once let go");
    check(lb && !input.driverDown("CHANGE_CAMERA"), "the controller's LB: Change View (the Auto layout)");
}

// In hud_replay: Change View due once as V goes down (not while held), not in a frame of a key the window acts on
// (SELECT here), not while the race is paused, once the play ended or outside hud_replay; Rear View held into the
// player's actions there.
OJ_INTEGRATION_TEST(ReplayKeys, ChangeViewAndRearView) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::input::Input input;
    if (!loadInput(input)) return;
    auto view = std::make_unique<app::TrackView>();
    view->input = &input;
    view->frontEnd.window = oj::RaceWindowShown("hud_replay");
    view->replay.playing = 1;
    view->replay.state = oj::ReplayState::Playing;
    const auto press = [&](uint8_t key, bool down) { input.poll({{key, down}}, true, 0.016f); };
    press(kKeyV, true);
    const bool once = view->replayKeys() && !view->replayKeys();
    press(kKeyV, false);
    view->replayKeys();
    view->frontEnd.uiEvent = oj::input::kSelect;
    press(kKeyV, true);
    const bool windowFirst = !view->replayKeys();
    press(kKeyV, false);
    view->frontEnd.uiEvent = oj::input::kSelect5;  // a key the window does not act on (LB's in its UI map)
    view->replayKeys();
    press(kKeyV, true);
    const bool otherKey = view->replayKeys();
    press(kKeyV, false);
    view->frontEnd.uiEvent = oj::input::kNone;
    view->replayKeys();
    view->paused = true;
    press(kKeyV, true);
    const bool paused = !view->replayKeys();
    press(kKeyV, false);
    view->paused = false;
    view->replayKeys();
    press(kKeySpace, true);
    view->replayKeys();
    const bool rear = view->first().actions.rearView;
    press(kKeySpace, false);
    view->replayKeys();
    const bool rearLet = !view->first().actions.rearView;
    view->replay.state = oj::ReplayState::Stopped;  // the play ended
    press(kKeyV, true);
    const bool ended = !view->replayKeys();
    press(kKeyV, false);
    view->frontEnd.window = oj::RaceWindowShown("hud_race");
    view->first().actions.rearView = false;
    press(kKeySpace, true);
    press(kKeyV, true);
    const bool outside = !view->replayKeys() && !view->first().actions.rearView;
    check(once, "hud_replay: V going down a Change View, once while held");
    check(windowFirst && otherKey, "a frame of SELECT changes no view; one of a key the window passes over does");
    check(paused && ended, "the race paused, the play ended: no Change View");
    check(rear && rearLet, "Space held: the player's Rear View, none once let go");
    check(outside, "outside hud_replay: neither (the race reads its keys itself)");
}
