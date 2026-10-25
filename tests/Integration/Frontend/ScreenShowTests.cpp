// Integration tests: showing an element again (Game/UI/UiScreen.cpp) over the game's cmsatmeeting.ui.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/Language.h"
#include "Game/UI/UiScreen.h"

namespace {

using oj::test::check;

}  // namespace

// 0x005A9BB0 shows a hidden element and leaves its transition as it is: PRIZETITLE, hidden and shown again by the race
// event's details (0x004C0B60) while the window comes in, goes on fading in (it was shown at once, at the end of its
// transition). 0x005A9C20 (showContaining) without its transitions started again shows it at their end.
OJ_INTEGRATION_TEST(ScreenShow, KeepsTheTransition) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::test::NullRenderDevice device;
    const oj::Language language = *oj::Language::byName("english");
    oj::ui::Screen s(device, language);
    if (!s.load("cmsatmeeting") || s.transitionOf("prizetitle") == nullptr) {
        check(false, "cmsatmeeting.ui read from the game's folder, PRIZETITLE with a transition");
        return;
    }
    const oj::ui::Transition& t = *s.transitionOf("prizetitle");
    s.restartTransitions();
    s.setHidden("prizetitle", true);
    s.show("prizetitle", false);
    const bool comingIn = !s.hidden("prizetitle") && t.progress < 1.0f && t.remaining > 0.0f;
    s.setHidden("prizetitle", true);
    s.showContaining("PRIZETITLE", false);
    const bool atEnd = !s.hidden("prizetitle") && t.progress == 1.0f && t.remaining == 0.0f;
    check(comingIn, "shown by name without animation: still coming in with the window");
    check(atEnd, "shown by a part of the name without animation: at the end of its transition");
}
