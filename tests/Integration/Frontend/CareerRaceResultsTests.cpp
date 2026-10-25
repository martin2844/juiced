// Integration tests: the career's race results window CMSRaceResults on a career of the game's data, the game's window
// and texts through a front end of the tests (Framework/Fakes/FakeCareerHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeCareerHost.h"

#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Core/Race/RaceSession.h"
#include "Game/Frontend/Career/CareerRaceResults.h"

#include <bit>
#include <memory>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// A place of the crews' order no crew took (-1, 0x004DA310): the stock read the odds before their table (the last
// racer's fields, the session's +0x264) and with a stake there (the last racer's level timer not 0) added winnings
// made of those bytes to the money, the row shown with them. The fixed build leaves the row hidden, nothing won.
OJ_INTEGRATION_TEST(CareerRaceResults, PlaceOfNoCrew) {
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
    oj::CalendarEvent* event = oj::currentEvent(career.calendar);
    if (event == nullptr) {
        check(false, "the first career's event on its day");
        return;
    }
    event->round.opponents = 1;
    event->round.prize = {0, 0, 0};
    oj::RaceSession& session = career.session;
    session.order.fill(-1);
    session.racers[5].levelTimer = 2.0f;  // a stake there
    session.racers[5].order = 0;
    session.racers[5].orderTime = 1.5f;
    session.needsSetup = std::bit_cast<int32_t>(500.0f);  // the winnings there
    const int32_t money = career.profile.money;
    oj::test::FakeCareerHost host;
    host.data = &data;
    const std::shared_ptr<oj::ui::Screen> screen = host.open("CMSRaceResults");
    if (screen == nullptr) return;
    oj::frontend::CareerRaceResults results;
    results.enter(host, *screen);
    const int crew = screen->find("crew1");
    check(career.profile.money == money, "race results (fixed): a place of no crew wins nothing (" +
                                             std::to_string(career.profile.money - money) + ")");
    check(crew < 0 || screen->hidden(crew), "race results (fixed): its row hidden");
}
