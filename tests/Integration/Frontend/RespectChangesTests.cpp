// Integration tests: the respect changes window CMSRespectChanges on a career of the game's data, the game's window
// and texts through a front end of the tests (Framework/Fakes/FakeCareerHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeCareerHost.h"

#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Core/Input/Input.h"
#include "Game/Frontend/Career/RespectChanges.h"

#include <format>
#include <memory>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// An entry shown (0x004D1050) colours type and counter (green for a rise, red for a fall); comment, the crew's name
// and what it thinks, keeps the colour of its Font line (it was coloured too).
OJ_INTEGRATION_TEST(RespectChanges, CommentKeepsItsColour) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    if (!oj::test::sharedGameData().carDefsRead) return;
    oj::GameData data(oj::test::gameFiles().root());
    oj::CrtRandom random;
    std::string error;
    if (!data.load(random, &error) || !data.career) {
        check(false, "game data and a first career loaded: " + error);
        return;
    }
    const auto colours = [&](int32_t kind) {
        data.career->respectChanges = oj::CareerNotes{};
        data.career->respectChanges.add(kind, kind < 10 ? 25.0f : -25.0f, 2.0f, 0.0f);
        oj::test::FakeCareerHost host;
        host.data = &data;
        const std::shared_ptr<oj::ui::Screen> screen = host.open("CMSRespectChanges");
        if (screen == nullptr) return std::string("no window");
        oj::frontend::RespectChanges changes;
        changes.enter(host, *screen);
        changes.update(host, *screen, oj::input::kSelect);
        return std::format("{:08X} {:08X} {:08X}", screen->fontColourOf(screen->find("type")),
                           screen->fontColourOf(screen->find("counter")),
                           screen->fontColourOf(screen->find("comment")));
    };
    const std::string rise = colours(0);
    const std::string fall = colours(10);
    check(rise == "FF32FF32 FF32FF32 00000000", "a rise: type and counter green, comment its own (" + rise + ")");
    check(fall == "FFFF3232 FFFF3232 00000000", "a fall: type and counter red, comment its own (" + fall + ")");
}
