// Integration tests: Career.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Random.h"
#include "Core/Career/Career.h"
#include "Core/Career/CareerEvents.h"
#include "Core/Career/CareerInit.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Data/ExeTables.h"
#include "Core/Data/GameData.h"
#include "Core/Network/GameSpy/GameSpyPresence.h"
#include "Core/Race/Arcade.h"
#include "Core/Race/RaceSession.h"
#include "Core/Saves/Profile.h"
#include "Core/Saves/Save.h"
#include "Core/Saves/SaveEntries.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// A new career from the game's files.
OJ_INTEGRATION_TEST(Career, NewCareer) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::CarDefs& defs = shared.defs;
    const oj::ExeTables& tables = shared.tables;
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    const oj::PartCatalogue& catalogue = shared.catalogue;
    oj::DynamicsStore& dynamics = *shared.dynamics;
    auto newOne = std::make_unique<oj::Career>();
    oj::CrtRandom careerRandom;
    careerRandom.seed(42);
    int counter = 1;
    int currentMate = 0;
    oj::CarMaking making{defs, catalogue, careerRandom, counter};
    oj::CareerContext context{tables, making, dynamics, game, currentMate};
    oj::newCareer(*newOne, context, "Tester");
    const oj::Career& c = *newOne;
    int pending = 0;
    for (const auto& e : c.calendar.events) pending += e.used && !e.done ? 1 : 0;
    const auto& first = c.calendar.events[0];
    bool sorted = true;
    for (int i = 0; i + 1 < c.dealer.offerCount; ++i) {
        sorted = sorted && c.dealer.offers[static_cast<size_t>(i)].rating >=
                               c.dealer.offers[static_cast<size_t>(i) + 1].rating &&
                 c.dealer.offers[static_cast<size_t>(i)].index == i;
    }
    const std::array<int8_t, 10> partsA{0, 0, -1, -1, 0, 0, 0, -1, 0, -1};
    const std::array<int8_t, 10> partsB{-1, -1, -1, 4, -1, -1, -1, -1, -1, 0};
    check(c.profile.money == 40000 && c.profile.memberCount == 1 && c.members[0] == 0 &&
              std::string(c.crews[0].memberNames[0].data()) == "Tester" &&
              std::string(c.crewmates[0].name.data()) == "Tester" &&
              c.crews[0].respect == 5000 && c.crews[1].respect == 400 &&
              c.crews[0].stage == std::array<int32_t, 4>{1, 1, 0, 1} &&
              c.crews[1].stage == std::array<int32_t, 4>{1, 1, 0, 0} &&
              c.crews[2].stage == std::array<int32_t, 4>{0, 0, 0, 0} &&
              c.profile.partsA[7] == partsA && c.profile.partsB[30] == partsB &&
              c.profile.carLevel[0] == 1 && c.profile.unlockStep[52] == 10 &&
              c.garage.size() == 2 && c.padding.size() == 53 && c.history.position == 1 &&
              c.history.series[0][0] == 40000.0f && c.history.series[1][0] == 400.0f,
          "new career: profile, crews' respect and steps, unlock tables, garage, history");
    check(first.used && first.month == 0 && first.dayOfMonth == 0 && first.crew == 1 &&
              first.type == oj::EventKind::Circuit && first.round.route == 0x31 && first.round.laps == 2 &&
              first.prizeTotal > 5000 && pending == 20 && c.dealer.offerCount >= 3 && sorted,
          "new career: the first event, 20 events waiting, the dealer's offers sorted");
    auto reread = std::make_unique<oj::Career>();
    std::string whyNot;
    const std::vector<uint8_t> written = c.write();
    check(reread->read(written, &whyNot) && reread->write() == written,
          "new career: CMSGlobal.sav written and read back" + whyNot);
    std::printf("     new career: %d offers, events:", c.dealer.offerCount);
    for (const auto& e : c.calendar.events) {
        if (e.used) std::printf(" %d/%d:c%d,t%d", e.month, e.dayOfMonth, e.crew, e.type);
    }
    std::printf("\n");
    // A pink slip's car (0x004B1A80): won, a copy of the leader's car of the crew bet with joins the
    // garage and the crew's +0x2D0 grows; lost, the garage's last car goes and +0x2D0 is cleared.
    {
        auto pink = std::make_unique<oj::Career>(c);
        pink->session.count = 2;
        pink->session.racers[0].crew = 0;
        pink->session.racers[0].member = 0;
        pink->session.racers[1].crew = 1;
        pink->session.racers[1].member = 0;
        pink->session.racers[1].car = 0;
        if (pink->opponentCars.empty()) pink->opponentCars.push_back(pink->garage[0]);
        const size_t before = pink->garage.size();
        pink->bets.crew = 1;
        pink->bets.won = 1;
        oj::PartCatalogue none;
        oj::pinkSlipCar(*pink, none);
        const bool won = pink->garage.size() == before + 1 && pink->crews[1].pinkSlipsWon == 1 &&
                         pink->garage.back().def == pink->opponentCars[0].def && pink->garage.back().grid;
        pink->bets.won = 0;
        oj::pinkSlipCar(*pink, none);
        check(won && pink->garage.size() == before && pink->crews[1].pinkSlipsWon == 0,
              "pink slip: the rival's car won into the garage, the stake lost from it");
        // The online pink slip's stakes (0x004A49A0): a copy of each car (a new grid), +0x90 1 and
        // +0x94, +0x9C, +0xAC 0.
        pink->onlineSettled = 1;
        pink->onlineAutosave = 1;
        pink->valueAC = 1;
        oj::onlineStakes(*pink, pink->garage[0], pink->opponentCars[0]);
        check(pink->onlineOwnStake.size() == 1 && pink->onlineRivalStake.size() == 1 &&
                  pink->onlineOwnStake[0].def == pink->garage[0].def &&
                  pink->onlineRivalStake[0].def == pink->opponentCars[0].def &&
                  pink->onlineOwnStake[0].grid && pink->onlinePinkSlip == 1 &&
                  pink->onlineSettled == 0 && pink->onlineAutosave == 0 && pink->valueAC == 0,
              "online pink slip: the stakes copied, the flags set");
        // Its end (0x004A4C60): a tie keeps the garage; won adds the rival's car; lost drops the last
        // car; the lists and +0x90, +0xAC cleared each time; nothing without the stakes.
        const size_t garage = pink->garage.size();
        oj::onlineSettle(*pink, true, true, none, nullptr);
        const bool tie = pink->garage.size() == garage && pink->onlinePinkSlip == 0 &&
                         pink->onlineOwnStake.empty() && pink->onlineRivalStake.empty();
        oj::onlineSettle(*pink, true, false, none, nullptr);
        const bool unset = pink->garage.size() == garage;
        oj::onlineStakes(*pink, pink->garage[0], pink->opponentCars[0]);
        pink->valueAC = 1;
        oj::onlineSettle(*pink, true, false, none, nullptr);
        const bool wonCar = pink->garage.size() == garage + 1 &&
                            pink->garage.back().def == pink->opponentCars[0].def && pink->valueAC == 0;
        oj::onlineStakes(*pink, pink->garage[0], pink->opponentCars[0]);
        oj::onlineSettle(*pink, false, false, none, nullptr);
        check(tie && unset && wonCar && pink->garage.size() == garage && pink->onlinePinkSlip == 0,
              "online pink slip: a tie, a win and a loss");
    }
    // A session's results with no crew first (0x004B9E20 at 0x004BA195): the stock's odds before the table
    // were the last racer's order, order time and level timer (the stake) and the session's +0x264; the
    // fixed build reads and writes nothing then.
    {
        auto cruise = std::make_unique<oj::Career>(c);
        oj::RaceSession& s = cruise->session;
        s.event = &cruise->calendar.events[0];
        s.round = &cruise->calendar.events[0].round;
        s.count = 0;
        s.order.fill(-1);
        oj::RaceSetup race;
        race.mode = oj::RaceMode::Career;
        race.type = oj::RaceType::Cruise;
        const auto results = [&](float levelTimer, int32_t order, float orderTime) {
            s.racers[5].levelTimer = levelTimer;
            s.racers[5].order = order;
            s.racers[5].orderTime = orderTime;
            s.needsSetup = 0;
            oj::sessionResults(*cruise, context, race);
            return s.needsSetup;
        };
        const int32_t staked = results(2.0f, 0, 1.5f);
        const int32_t none = results(0.0f, 0, 1.5f);
        const int32_t nan = results(2.0f, 0, 0.0f);
        check(staked == 0 && none == 0 && nan == 0,
              "session results without a first crew (fixed): nothing won into the session's +0x264");
    }
    // The cheats (0x004A6E50) on the new career: money, the full crew, the cars (its two and one of each
    // model 1 .. 52), the respect.
    oj::Career& cheated = *newOne;
    const int16_t level = cheated.profile.respectLevel;
    oj::applyCheat(cheated, oj::Cheat::Money, context);
    oj::applyCheat(cheated, oj::Cheat::Crewmates, context);
    oj::applyCheat(cheated, oj::Cheat::CarsAndParts, context);
    oj::applyCheat(cheated, oj::Cheat::Respect, context);
    oj::applyCheat(cheated, oj::Cheat::WinAllRaces, context);
    const std::string contact(cheated.phone.contacts[0].name.data());
    std::printf("     cheats: money %d, members %d, cars %zu, level %d (from %d), respect %d, contacts %d"
                " (%s), notes %d\n", cheated.profile.money, cheated.profile.memberCount,
                cheated.garage.size(), cheated.profile.respectLevel, level, cheated.crews[1].respect,
                cheated.phone.contactCount, contact.c_str(), cheated.respectChanges.count);
    check(cheated.profile.money == 10000000 && cheated.profile.memberCount == 4 &&
              cheated.garage.size() == 54 && cheated.profile.respectLevel == level + 53 + 16 &&
              cheated.crews[1].respect == 2000 && cheated.crews[1].stage[3] == 1 &&
              cheated.phone.contactCount >= 8 && contact.find(" (") != std::string::npos &&
              cheated.profile.partsA[5][0] == 3 && cheated.profile.partsB[5][0] == 10 &&
              cheated.cheatFlags[1] == 1 && cheated.respectChanges.count == 0,
          "cheats: the money, the full crew, the cars, everyone's respect, win all races");
}
OJ_INTEGRATION_TEST(Career, ArcadeProgress) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::ExeTables& tables = shared.tables;
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    if (const auto races = game.read(std::filesystem::path("scripts") / "ArcadeRaces.txt")) {
        const oj::save::ArcadeUnlock arcade = oj::newArcadeProgress(
            std::string_view(reinterpret_cast<const char*>(races->data()), races->size()), tables);
        std::vector<int> openCars;
        for (size_t i = 0; i < 53; ++i) {
            uint32_t locked = 1;
            std::memcpy(&locked, arcade.carFlags.data() + i * 4, 4);
            if (locked == 0) openCars.push_back(static_cast<int>(i));
        }
        int openRoutes = 0;
        for (size_t i = 0; i < 109; ++i) {
            uint32_t locked = 1;
            std::memcpy(&locked, arcade.routeFlags.data() + i * 4, 4);
            openRoutes += locked == 0 ? 1 : 0;
        }
        const std::vector<int> expected{1, 16, 29, 42};
        std::printf("     arcade: %zu championships, cars", arcade.series.size());
        for (int car : openCars) std::printf(" %d", car);
        std::printf(", %d routes\n", openRoutes);
        check(arcade.series.size() == 15 && arcade.series[0][3].locked == 0 && arcade.series[0][4].locked == 1 &&
                  openCars == expected && openRoutes == 6 && arcade.write().size() == 1256,
              "arcade progress of a new driver: 15 championships, 4 races, 4 cars and 6 routes open");
        // The first championship won race by race (0x0042B350): after the fourth the final race opens, after
        // the third the next championship.
        auto a = std::make_unique<oj::Arcade>();
        a->load(std::string_view(reinterpret_cast<const char*>(races->data()), races->size()), tables);
        bool ok = a->i32(oj::Arcade::race(0, 0)) == 4 && a->i32(oj::Arcade::race(0, 0) + 0x18) > 0 &&
                  a->i32(oj::Arcade::race(0, 0) + 0x28) > 1;
        for (int32_t r = 0; r < 5; ++r) {
            a->put(oj::Arcade::kChampionshipIndex, 0);
            a->put(oj::Arcade::kRaceIndex, r);
            a->put(oj::Arcade::kFinalUnlocked, 0);
            a->put(oj::Arcade::kSeriesComplete, 0);
            a->put(oj::Arcade::kNextSeries, 0);
            a->complete(-1);
            const bool next = a->i32(oj::Arcade::kNextSeries) == 1;
            const bool final = a->i32(oj::Arcade::kFinalUnlocked) == 1;
            if (r == 2) ok = ok && next && a->i32(oj::Arcade::race(1, 0) + 0xCC) == 0;
            if (r == 3) ok = ok && final && a->i32(oj::Arcade::race(0, 4) + 0xCC) == 0;
            if (r == 4) ok = ok && a->i32(oj::Arcade::kSeriesComplete) == 1;
        }
        ok = ok && a->i32(oj::Arcade::kResultWaits) == 1 && !a->finished();
        check(ok, "arcade: a championship won race by race opens the next championship and the final race");
    }
}
// The start of the game (a career already) and a new driver written into a folder of the tests. The
// draws are those of a run of the stock game with the timer seed 1823537005 (rand() traced): seed 42
// and one draw, the seed of the game object, cardefs.txt 1613, the start career 124505, the records
// and the start driver 4 each, the music 24, the front end's music 1, "start" 24, the new driver 4,
// its career 108181.
OJ_INTEGRATION_TEST(Career, StartAndNewDriver) {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    oj::GameData data(oj::test::gameFiles().root());
    oj::CrtRandom startRandom;
    startRandom.seed(static_cast<uint32_t>(startRandom.next() + 1));
    startRandom.next();
    startRandom.seed(1823537005);
    std::string loadError;
    const bool loaded = data.load(startRandom, &loadError);
    check(loaded && data.career->garage.size() == 2 && data.career->profile.memberCount == 1 &&
              startRandom.draws() == 126152,
          "game data: tables, cars, parts and a first career at start, the draws of the stock game" +
              (loaded ? "" : ": " + loadError));
    if (loaded) {
        startRandom.next();
        data.shuffleMusic(oj::save::Settings::defaults(7), startRandom);
        const uint64_t beforeDriver = startRandom.draws();
        oj::save::Profile driver = oj::save::Profile::create(L"Test Driver", 7, startRandom);
        data.newCareer(startRandom, oj::crewName8(driver.name));
        check(beforeDriver == 126177 && startRandom.draws() == 126177 + 4 + 108181,
              "a new driver's career draws as the stock game's");
        oj::save::Archive archive;
        archive.entries["settings.dat"] = driver.settings.write();
        archive.entries["ArcadeUnlock.sav"] = oj::newArcadeProgress(data.arcadeRaces, data.tables).write();
        archive.entries["UIHelp.sav"] = driver.help.write();
        archive.entries["CMSGlobal.sav"] = data.career->write();
        const std::filesystem::path testSaves =
            std::filesystem::path(exePath).parent_path() / L"TestSaves" / L"SAVE";
        std::error_code ec;
        std::filesystem::remove_all(testSaves.parent_path(), ec);
        const bool written = oj::save::writeNewDriver(testSaves, driver.name, archive, &loadError);
        const bool twice = oj::save::writeNewDriver(testSaves, driver.name, archive, nullptr);
        const std::vector<oj::save::Driver> listed = oj::save::listDrivers(testSaves);
        bool readBack = false;
        if (listed.size() == 1) {
            const auto file = oj::GameFiles(listed[0].folder).read(L"juicey.dat");
            oj::save::Archive back;
            auto career = std::make_unique<oj::Career>();
            readBack = file && back.read(*file, nullptr) && back.entries == archive.entries &&
                       career->read(back.entries["CMSGlobal.sav"], nullptr) &&
                       std::string(career->crews[0].memberNames[0].data()) == "Test Driver";
        }
        check(written && !twice && listed.size() == 1 && listed[0].folder.filename() == L"PTest_Driver" &&
                  listed[0].userData.name == L"Test Driver" && readBack,
              "a new driver written: P<name> folder, USERDATA, juicey.dat read back; no writing over it");
        std::filesystem::remove_all(testSaves.parent_path(), ec);

        // DemoFlyby's way into the career: a new career again, the bets on crew 1 (2000 for the first
        // event) and the race of the first event. The draws are OpenJuice's count; a stock trace of this
        // step is still to be compared.
        const uint64_t beforeCareer = startRandom.draws();
        data.enterCareer(startRandom, oj::crewName8(driver.name));
        const oj::Career& c = *data.career;
        const oj::RaceSession& s = c.session;
        bool crews01 = s.count == 2;
        for (uint32_t i = 0; i < s.count && i < s.racers.size(); ++i) {
            crews01 = crews01 && (s.racers[i].crew == 0 || s.racers[i].crew == 1) &&
                      s.racers[i].member == 0 && s.racers[i].place == static_cast<int32_t>(i);
        }
        std::array<int32_t, 7> amounts{};
        for (size_t i = 0; i < amounts.size(); ++i) amounts[i] = c.bets.amounts[i].amount;
        const std::array<int32_t, 7> expectedAmounts{800, 1200, 1600, 2000, 2400, 2800, 3200};
        std::printf("     into the career: %llu rand() values, racers",
                    static_cast<unsigned long long>(startRandom.draws() - beforeCareer));
        for (uint32_t i = 0; i < s.count && i < s.racers.size(); ++i) {
            std::printf(" crew %d car %d", s.racers[i].crew, s.racers[i].car);
        }
        std::printf(", bet racer %u\n", s.betRacer);
        check(data.race.mode == oj::RaceMode::Career && data.careerScreen == oj::CareerScreen::Character &&
                  c.bets.crew == 1 && c.bets.kind == oj::BetKind::Money && c.bets.firstStage == 3 &&
                  c.bets.rivalMoney == 2000.0f && c.bets.amountCount == 7 && amounts == expectedAmounts && crews01 &&
                  s.needsSetup == 0 && c.profile.raceCarsChosen == 1 && c.head == 0,
              "into the career: the first event's bets on crew 1 and its race of two");
    }
}

// The phone's events (0x004A2DC0 the host rights race, 0x004A2F80 the pink slip race) take the class of the player's
// best car: 0x004A9A70 on the career's first crew, the player's, whose list is the garage (not the calling crew's,
// whose list is every opponent's car: class 1 for a new career's class 8 garage).
OJ_INTEGRATION_TEST(Career, PhoneEventsClass) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    auto career = std::make_unique<oj::Career>();
    oj::CrtRandom random;
    random.seed(42);
    int counter = 1;
    int currentMate = 0;
    oj::CarMaking making{shared.defs, shared.catalogue, random, counter};
    oj::CareerContext context{shared.tables, making, *shared.dynamics, game, currentMate};
    oj::newCareer(*career, context, "Tester");
    oj::Career& c = *career;
    const auto bestClass = [](const std::vector<oj::Car>& cars) {
        float best = 0.0f;
        for (const oj::Car& car : cars) best = std::max(best, oj::Car::asFloat(car.rating));
        return oj::ratingClass(best);
    };
    const int garageClass = bestClass(c.garage);
    const int opponentsClass = bestClass(c.opponentCars);
    const auto made = [&](int crew, bool host) -> const oj::CalendarEvent* {
        int day = 0;
        int month = 0;
        oj::nextFreeDay(c.calendar, day, month);
        oj::phoneEvent(c, context, crew, month, day, host);
        for (const oj::CalendarEvent& e : c.calendar.events) {
            if (e.used != 0 && e.month == month && e.dayOfMonth == day) return &e;
        }
        return nullptr;
    };
    const oj::CalendarEvent* hosted = made(1, true);
    const oj::CalendarEvent* pinks = made(2, false);
    check(garageClass != opponentsClass, "a new career: the garage's best class (" + std::to_string(garageClass + 1) +
                                             ") is not the opponents' (" + std::to_string(opponentsClass + 1) + ")");
    check(hosted != nullptr && hosted->hostedByPlayer == 1 && hosted->crew == 1 && hosted->carClass == garageClass,
          "the host rights race: the class of the player's best car (" +
              std::to_string(hosted != nullptr ? hosted->carClass + 1 : 0) + ")");
    check(pinks != nullptr && pinks->pinkSlip == 1 && pinks->crew == 2 && pinks->carClass == garageClass,
          "the pink slip race: the class of the player's best car (" +
              std::to_string(pinks != nullptr ? pinks->carClass + 1 : 0) + ")");
}
