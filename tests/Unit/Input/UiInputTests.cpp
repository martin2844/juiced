// Unit tests: UiInput.

#include "Framework/Test.h"

#include "Core/Data/Controls.h"
#include "Core/Input/Input.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// The UI input: a held DOWN gives an event, then again once 0.25 s have run out and every 0.075 s (0x004082F0; frames
// of 1/60 s: 17 and 6 frames), RETURN selects.
OJ_UNIT_TEST(UiInput, KeyboardSequence) {
    const char* controls =
        "[BeginActionMap]\r\nName UI\r\nDevice ACTION_MAP\r\n"
        "[BeginAction]\r\nName DOWN\r\nType DIGITAL\r\n;OneShot\r\nControl \"DIKEYBOARD_DOWN 'Down'\"\r\n"
        "[EndAction]\r\n"
        "[BeginAction]\r\nName SELECT\r\nType DIGITAL\r\nOneShot\r\nControl \"DIKEYBOARD_RETURN 'Select'\"\r\n"
        "[EndAction]\r\n[EndActionMap]\r\n";
    const auto maps = oj::parseControls(controls);
    check(maps.size() == 1 && maps[0].actions.size() == 2 && !maps[0].actions[0].oneShot &&
              maps[0].actions[1].oneShot && maps[0].actions[1].control == "DIKEYBOARD_RETURN",
          "controls file: maps, actions, OneShot, the semantic of a control");
    oj::input::Input in;
    in.load(maps);
    // Raw at first: RETURN held takes the device as the first player's at the next update (0x0058FE50 takes it by the
    // poll before, 0x005918E0, 0x00594E20).
    in.poll({});
    const bool takenBefore = in.playerTaken();
    in.poll({{0x1C, true}});
    const bool takenAtPoll = in.playerTaken();
    in.poll({});
    const bool taken = in.playerTaken();
    // The UI map registered: RETURN held through it gives nothing until it goes down again.
    in.useUi();
    in.poll({});
    const uint32_t afterMap = in.uiFrame(1.0f / 60.0f).event;
    in.poll({{0x1C, false}});
    in.uiFrame(1.0f / 60.0f);
    check(!takenBefore && !takenAtPoll && taken && afterMap == 0, "keyboard: RETURN takes the first player; a held key after the map");
    std::string events;
    for (int frame = 0; frame < 30; ++frame) {
        std::vector<oj::input::KeyEvent> keys;
        if (frame == 0) keys.push_back({0xD0, true});
        if (frame == 29) keys.push_back({0x1C, true});
        in.poll(keys);
        events += std::to_string(in.uiFrame(1.0f / 60.0f).event);
    }
    std::printf("     events %s\n", events.c_str());
    check(events == "200000000000000002000002000005", "UI input: repeat of a held direction, select");
    // One event a poll: DOWN's and RETURN's ups first, then the shift key, then a letter.
    in.poll({{0xD0, false}, {0x1C, false}, {0x2A, true}});
    in.poll({});
    in.poll({});
    const char shift = in.typed();
    in.poll({{0x14, true}});
    check(shift == 0x0F && in.held(0x0F) && in.typed() == 'T', "typed characters: shift, a letter, a key a poll");
    in.poll({{0x2A, false}});
    in.poll({{0x14, false}});
    // A tap within a frame: down this poll, up the next; a key of no map is not read; an Alt key keeps a key's value
    // 0 (0x00594D95).
    in.poll({{0x1C, true}, {0x1C, false}});
    const uint32_t tap = in.uiFrame(0.0f).event;
    in.poll({});
    in.uiFrame(0.0f);
    in.poll({{0x3F, true}, {0xD0, true}});
    const uint32_t past = in.uiFrame(0.0f).event;
    in.poll({{0x3F, false}, {0xD0, false}});
    in.poll({});
    in.uiFrame(0.0f);
    in.poll({{0x38, true}});
    in.poll({{0x1C, true}});
    const uint32_t withAlt = in.uiFrame(0.0f).event;
    in.poll({{0x1C, false}});
    in.poll({{0x38, false}});
    // Alt+Tab: Alt's press taken, its release while the window is not active lost with the buffer; acquired again the
    // keyboard reports it, and RETURN is read.
    in.poll({{0x38, true}});
    in.poll({{0x38, false}}, false);
    in.poll({}, false);
    in.poll({});
    in.poll({{0x1C, true}});
    const uint32_t afterTab = in.uiFrame(0.0f).event;
    in.poll({{0x1C, false}});
    in.poll({});
    check(afterTab == oj::input::kSelect, "keyboard: Alt's release reported as the focus comes back (Alt+Tab)");

    // Let go (0x0058FEA0): raw again, no event, an arrow no input, RETURN held input.
    in.release();
    in.poll({{0xD0, true}});
    const bool arrowInput = in.anyInput();
    const uint32_t rawEvent = in.uiFrame(0.0f).event;
    in.poll({{0xD0, false}, {0x1C, true}});
    in.poll({});
    const bool returnInput = in.anyInput() && in.playerTaken();
    check(tap == oj::input::kSelect && past == oj::input::kDown && withAlt == 0 && !arrowInput && rawEvent == 0 &&
              returnInput,
          "keyboard: a tap seen, a key of no map passed over, Alt, the raw device after its let go");
}
