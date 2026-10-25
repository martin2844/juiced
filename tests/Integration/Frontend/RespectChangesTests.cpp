// Integration tests: the respect changes window CMSRespectChanges on a career of the game's data, the game's window
// and texts through a front end of the tests (Framework/Fakes/FakeCareerHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeCareerHost.h"

#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Core/Input/Input.h"
#include "Game/Frontend/Career/MeetingSummary.h"
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

// A privilege's entry (kinds 20 .. 27, 0x004BBFC0) holds its crew in the change and 0 as the crew: the stock's take
// gave the player's crew that crew's number of respect. The fixed build leaves the respect of such an entry be.
OJ_INTEGRATION_TEST(RespectChanges, PrivilegeLeavesRespect) {
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
    oj::Career& career = *data.career;
    career.respectChanges = oj::CareerNotes{};
    career.respectChanges.add(20, 3.0f, 0.0f, 0.0f);  // crew 3's privilege gained
    const int32_t before = career.crews[0].respect;
    oj::test::FakeCareerHost host;
    host.data = &data;
    const std::shared_ptr<oj::ui::Screen> screen = host.open("CMSRespectChanges");
    if (screen == nullptr) return;
    oj::frontend::RespectChanges changes;
    changes.enter(host, *screen);
    check(career.crews[0].respect == before, "respect changes (fixed): a privilege's entry gives no respect (" +
                                                 std::to_string(career.crews[0].respect - before) + ")");
}

// The history's step (0x004BD8A0) is the event's at 0x004BAEE0 (afterRaceResults), once the summaries' lists are
// empty. The stock's leaves of CMSMeetingSummary and CMSRespectChanges took one more each, two or three points of the
// stats' graph for one event; the fixed build's leaves take none.
OJ_INTEGRATION_TEST(RespectChanges, LeavesTakeNoHistoryStep) {
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
    oj::Career& career = *data.career;
    oj::test::FakeCareerHost host;
    host.data = &data;
    const int32_t before = career.history.position;
    oj::frontend::RespectChanges changes;
    changes.leave(host);
    oj::frontend::MeetingSummary summary;
    summary.leave(host);
    check(career.history.position == before, "summaries' leaves (fixed): no history step (" +
                                                 std::to_string(career.history.position - before) + ")");
}
