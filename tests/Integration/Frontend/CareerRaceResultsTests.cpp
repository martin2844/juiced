// Integration tests: the career's race results window CMSRaceResults on a career of the game's data, the game's window
// and texts through a front end of the tests (Framework/Fakes/FakeCareerHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeCareerHost.h"

#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Core/Race/RaceSession.h"
#include "Game/Frontend/Career/CareerRaceResults.h"
#include "Core/Saves/Save.h"
#include "Core/Input/Input.h"

#include <bit>
#include <memory>
#include <string>
#include <chrono>
#include <filesystem>

namespace {

using oj::test::check;

}  // namespace

// Exercise the real results controller and on-disk archive. Finishing order is
// supplied by the fixture: this is not a physics-driven end-to-end race test.
OJ_INTEGRATION_TEST(CareerRaceResults, PrizePersistsAfterReload) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::GameData data(oj::test::gameFiles().root());
    oj::CrtRandom random;
    std::string error;
    if (!data.load(random, &error) || !data.career) {
        check(false, "prize round trip: game data loaded: " + error);
        return;
    }
    auto& career = *data.career;
    auto* event = oj::currentEvent(career.calendar);
    if (!event) { check(false, "prize round trip: current event exists"); return; }
    auto& round = *const_cast<oj::EventRound*>(oj::roundOf(career, *event));
    round.opponents = 2;
    round.prize = {1000, 250, 0};
    career.session.order.fill(-1);
    career.session.order[0] = 0;
    career.session.order[1] = 1;
    career.session.odds = {};
    career.session.count = 0;
    career.session.winner = -1;
    career.profile.raceCarsChosen = 0;
    career.profile.racing = 0;
    const auto beforeMoney = career.profile.money;
    const auto beforePrizes = career.profile.crewPrizes;
    const auto beforeReceived = career.stats.moneyReceived;
    oj::test::FakeCareerHost host;
    host.data = &data;
    auto screen = host.open("CMSRaceResults");
    if (!screen) { check(false, "prize round trip: results screen loaded"); return; }
    oj::frontend::CareerRaceResults results;
    results.enter(host, *screen);
    check(career.profile.money == beforeMoney + 1000 &&
              career.profile.crewPrizes == beforePrizes + 1000 &&
              career.stats.moneyReceived == beforeReceived + 1000,
          "results: winner receives first-place prize, not opponent's prize");
    results.update(host, *screen, oj::input::kSelect);
    check(event->done == 1 && round.ended == 1, "results: accepting marks event and round finished");
    oj::save::Archive archive;
    archive.entries["CMSGlobal.sav"] = career.write();
    struct Scratch {
        std::filesystem::path path;
        ~Scratch() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    } scratch{std::filesystem::temp_directory_path() /
              ("openjuice-prize-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
    const auto save = scratch.path / "SAVE";
    if (!oj::save::writeNewDriver(save, L"Prize Test", archive, &error)) {
        check(false, "prize round trip: write driver: " + error); return;
    }
    const auto drivers = oj::save::listDrivers(save);
    check(drivers.size() == 1, "prize round trip: saved driver listed");
    if (drivers.size() != 1) return;
    const auto bytes = oj::GameFiles(drivers[0].folder).read(L"juicey.dat");
    oj::save::Archive reloaded;
    auto restored = std::make_unique<oj::Career>();
    const bool valid = bytes && reloaded.read(*bytes, &error) &&
        reloaded.entries == archive.entries && restored->read(reloaded.entries.at("CMSGlobal.sav"), &error);
    check(valid, "prize round trip: disk archive and career decode without changes");
    if (!valid) return;
    const auto* restoredEvent = oj::currentEvent(restored->calendar);
    check(restored->profile.money == career.profile.money, "prize round trip: money survives reload");
    check(restored->profile.crewPrizes == career.profile.crewPrizes, "prize round trip: crew prizes survive reload");
    // Career::stats is explicitly transient (Career.h, +0x58); the persisted
    // balances live in profile. Do not extend the retail archive for this.
    check(restored->stats.moneyReceived == 0, "prize round trip: transient received counter is not serialized");
    check(restoredEvent && restoredEvent->done == 1, "prize round trip: completed event survives reload");
    check(restoredEvent && oj::roundOf(*restored, *restoredEvent)->ended == 1,
          "prize round trip: completed round survives reload");
}

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
