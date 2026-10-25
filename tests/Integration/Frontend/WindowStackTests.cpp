// Integration tests: the windows open together (Game/UI/Components/Transition.cpp) over the game's carsel.ui and
// schememenu.ui.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/Language.h"
#include "Game/UI/UiScreen.h"

#include <vector>

namespace {

using oj::test::check;

}  // namespace

// An event while the top window still moves ends the transitions of the windows under it (0x005A9E80): the garage's
// menu, going out under schememenu (0x00436D40), is hidden at once, so that it is shown again as schememenu closes;
// with schememenu at rest the event leaves it going out (shown again before its end, it would end hidden).
OJ_INTEGRATION_TEST(WindowStack, EventEndsTransitionsUnder) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::test::NullRenderDevice device;
    const oj::Language language = *oj::Language::byName("english");
    oj::ui::Screen carsel(device, language);
    oj::ui::Screen scheme(device, language);
    if (!carsel.load("carsel") || !scheme.load("schememenu")) {
        check(false, "carsel.ui and schememenu.ui read from the game's folder");
        return;
    }
    const std::vector<oj::ui::Screen*> open{&carsel, &scheme};
    const auto menuGoingOut = [&] {
        carsel.finishTransitions();
        carsel.show("menu", false);
        carsel.restartTransition("menu", true);
        carsel.update(0.1f, 0);
    };
    menuGoingOut();
    scheme.restartTransitions();
    const bool goingOut = !carsel.hidden("menu");
    oj::ui::endTransitions(scheme, open);
    const bool hiddenAtOnce = carsel.hidden("menu") && scheme.transitionsDone();
    carsel.show("menu", true);
    const bool shownAgain = !carsel.hidden("menu") && carsel.transitionOf("menu") != nullptr &&
                            !carsel.transitionOf("menu")->out;
    check(goingOut && hiddenAtOnce && shownAgain,
          "schememenu moving: the event hides the garage's menu going out under it; shown again as it closes");
    menuGoingOut();
    scheme.finishTransitions();
    oj::ui::endTransitions(scheme, open);
    const bool stillGoing = !carsel.hidden("menu") && carsel.transitionOf("menu")->out;
    check(stillGoing, "schememenu at rest: the event leaves the menu going out");
}
