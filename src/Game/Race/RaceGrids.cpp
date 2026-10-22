// The race's grids: a custom race's, an arcade race's, the network's and a replay's, and the window after a race.

#include "Game/Race/RaceGrids.h"

#include "Audio/Voices.h"
#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Core/Base/Random.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/OnlineSessions.h"
#include "Core/Race/RaceSounds.h"
#include "Core/Race/Racer.h"
#include "Game/Cars/CarSound.h"
#include "Game/Media/Movies.h"
#include "Game/Race/RaceCamera.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

namespace app {

// 0x00495DE0 for a custom race (modes 1 and 3; off line, not a replay): the session of the players and opponents
// (0x004B9150, then the drivers 0x004BDCC0); the opponents automatically modified when the player's car has a catalogue
// part (the first player's). Each player's car is its arcade car (its save system object's +0x50): its grid made
// (0x005562A0), rated (0x0048E8A0), on grid place i as a player's car of crew 0, its competitor named by the driver.
// [0x0075EC98] (only cleared, by start's enter) would take the group by the place: 0 3, 1 4, 2 6. Each opponent k: a
// new car (0x0048DB40) of a car id drawn from the chosen group (rand() % 60 until not empty) with the CarDef of the
// first arcade car of that model (the first arcade car's when none); with the modification: as a prototype (0x0048DAE0:
// the player's car has a prototype part) of any level, else of level rand() % 3 (0x0048F2C0); a paint of the CarDefs'
// list (rand() % its size); rated, and drawn again (20 times at most in all) while its rating passes the last
// player's by more than 1; on the place after the players as an AI car of crew and number players + k, its competitor
// named by the short name of the first member of crew k + 1 ([career + 0x0C + 4 k] + 0xDE). The loading thread's rand(). The stock
// car keeps its name (+0x14); here the session's racer does (the HUD reads it there).
void customGrid(TrackView& view, oj::GameData& data, oj::scene::Engine& engine) {
    const oj::RaceSetup& race = data.race;
    oj::Career& career = *data.career;
    oj::CrtRandom& random = view.loading;
    oj::setupCustomSession(career, race.players, race.opponents, random);
    oj::RaceRules rules;
    rules.mode = race.mode;
    rules.type = race.type;
    rules.lapTime = race.lapTime;
    rules.speed = race.speed;
    rules.heat = race.heat;
    rules.heats = race.heats;
    rules.players = race.players;
    rules.minutes = race.difficulty;
    rules.session = &career.session;
    rules.career = &career;
    rules.settings = &data.settings;
    view.prepareRace(race.laps, rules);
    const oj::Spl& lines = view.scene->lines();
    if (lines.grid.empty() || data.arcadeCar < 0 || static_cast<size_t>(data.arcadeCar) >= data.arcadeCars.size()) {
        return;
    }
    const auto ratingOf = [&](oj::Car& car) {
        const oj::CarDynamics* dynamics = data.dynamics && car.def != nullptr ? data.dynamics->of(*car.def) : nullptr;
        return oj::rateCar(car, dynamics, data.catalogue);
    };
    const auto place = [&](size_t i) { return lines.grid[i % lines.grid.size()]; };
    const auto addCar = [&](oj::Car& car, oj::Car* kept, size_t number, oj::CarKind kind) {
        const oj::Spl::GridPoint& at = place(number);
        auto model = oj::scene::CarModel::load(engine, car, data.catalogue, random, false);
        oj::scene::CarModel* shown = view.scene->addCar(std::move(model), at);
        const oj::CarDynamics* dynamics = data.dynamics && car.def != nullptr ? data.dynamics->of(*car.def) : nullptr;
        if (dynamics != nullptr) {
            view.addCar(*dynamics, car, kept, data, shown, at, kind, static_cast<int32_t>(number),
                        static_cast<int32_t>(number), true);
        }
    };
    oj::Car& own = data.arcadeCars[static_cast<size_t>(data.arcadeCar)];
    bool modify = false;
    for (uint8_t part : own.mods) modify = modify || part != 0xFF;
    float rating = 0.0f;
    // 0x00496C70: player i's car of its save system object ([game + 0x64 + 4 i]: +0x40 [+0x50]), its grid made, rated
    // (the last player's rating the opponents' limit) and named by the object's driver (+0x20).
    const auto players = static_cast<size_t>(std::max(race.players, 0));
    for (size_t i = 0; i < players; ++i) {
        std::vector<oj::Car>& cars = i == 0 ? data.arcadeCars : data.arcadeCars2;
        const int32_t at = i == 0 ? data.arcadeCar : data.arcadeCar2;
        if (i > 1 || at < 0 || static_cast<size_t>(at) >= cars.size()) continue;
        oj::Car& car = cars[static_cast<size_t>(at)];
        if (!car.grid) car.grid.emplace();
        rating = ratingOf(car);
        if (i < career.session.racers.size()) {
            career.session.racers[i].name = i == 0 ? data.profileName : data.profileName2;
        }
        addCar(car, &car, i, oj::CarKind::Player);
    }
    oj::ArcadeCarGroups& groups = data.arcadeGroups;
    constexpr bool kGroupByPlace = false;  // [0x0075EC98]
    if (kGroupByPlace) {
        if (groups.index == 0) {
            groups.group = 3;
        } else if (groups.index == 1) {
            groups.group = 4;
        } else if (groups.index == 2) {
            groups.group = 6;
        }
    }
    const int32_t group = std::clamp(groups.group, 0, 7);
    int32_t retries = 20;
    size_t number = players;
    for (int32_t k = 0; k < race.opponents; ++k) {
        auto car = std::make_unique<oj::Car>(oj::makeCar(nullptr));
        int32_t id = 0;
        do {
            const auto r = static_cast<uint32_t>(random.next()) % 60;
            id = groups.ids[static_cast<size_t>(group)][r];
        } while (id == 0);
        size_t index = 0;
        for (size_t c = 0; c < data.arcadeCars.size(); ++c) {
            if (data.arcadeCars[c].def != nullptr && data.arcadeCars[c].def->id == id) {
                index = c;
                break;
            }
        }
        if (index < data.arcadeCars.size()) {
            car->def = data.arcadeCars[index].def;
            if (car->def != nullptr) car->setModel(car->def->model);
        }
        if (modify) {
            bool prototype = false;
            for (size_t kind = 0; kind < own.mods.size(); ++kind) {
                const auto chosen = static_cast<int8_t>(own.mods[kind]);
                if (chosen == -1) continue;
                const std::vector<oj::CarPart>& parts = data.catalogue.lists[kind].parts;
                if (parts.empty()) continue;
                const size_t at = static_cast<size_t>(chosen) < parts.size() ? static_cast<size_t>(chosen) : 0;
                const oj::CarPart& part = parts[at];
                if (part.level == 3) {
                    prototype = true;
                    break;
                }
            }
            oj::CarMaking making{data.defs, data.catalogue, random, data.autoModifyCounter};
            const int level = prototype ? -1 : static_cast<int>(static_cast<uint32_t>(random.next()) % 3);
            oj::autoModify(*car, prototype, level, making);
        }
        if (!data.defs.paints.empty()) {
            const auto r = static_cast<uint32_t>(random.next()) % static_cast<uint32_t>(data.defs.paints.size());
            const oj::CarPaint& p = data.defs.paints[r];
            car->base = p.base;
            car->metallic = p.metallic;
            car->pearl = p.pearl;
            car->metallicAmount = oj::Car::bitsOf(p.metallicAmount);
            car->pearlAmount = oj::Car::bitsOf(p.pearlAmount);
        }
        const float opponentRating = ratingOf(*car);
        if (!(opponentRating <= rating + 1.0f) && retries > 0) {
            --retries;
            --k;
            continue;
        }
        if (!car->grid) car->grid.emplace();
        if (number < career.session.racers.size()) {
            const size_t crew = static_cast<size_t>(k) + 1;
            const auto& shortName = career.crews[crew < career.crews.size() ? crew : 0].memberShortNames[0];
            std::wstring name;
            for (size_t c = 0; c < shortName.size() && shortName[c] != 0; ++c) {
                name += static_cast<wchar_t>(static_cast<unsigned char>(shortName[c]));
            }
            career.session.racers[number].name = name;
        }
        oj::Car& kept = *car;
        view.ownCars.push_back(std::move(car));
        addCar(kept, &kept, number, oj::CarKind::Ai);
        ++number;
    }
}

// 0x004269A0, the race's part: the race a replay's (the replay's play has set it), the header's type (a sprint's heat
// 0 of 3, of 1 in a network game), laps, track, route, time of day and weather; the race's mode is left as it is.
void setReplayRace(oj::RaceSetup& race, const oj::ReplayHeader& h) {
    race.type = static_cast<oj::RaceType>(h.type);
    if (race.type == oj::RaceType::Sprint) {
        race.heat = 0;
        race.heats = oj::net::netGame() == nullptr ? 3 : 1;
    }
    race.laps = static_cast<int32_t>(h.laps);
    race.track = h.track;
    race.route = h.route;
    race.timeOfDay = static_cast<int32_t>(h.timeOfDay);
    race.weather = static_cast<int32_t>(h.weather);
}

// 0x00461390 for a replay's race (mode 4): the replay reads its files (0x00429720: <name>.hdr and <name>.dat) and plays
// from the start; its header gives the race (0x004269A0). False when a file does not read.
bool loadReplayRace(TrackView& view, oj::GameData& data) {
    const oj::GameFiles files(std::filesystem::current_path());
    const auto hdr = files.read(data.replayName + ".hdr");
    const auto dat = files.read(data.replayName + ".dat");
    if (!hdr || !dat) return false;
    const auto bytes = [](const std::vector<std::byte>& f) {
        const auto* p = reinterpret_cast<const uint8_t*>(f.data());
        return std::vector<uint8_t>(p, p + f.size());
    };
    if (!view.replay.load(bytes(*hdr), bytes(*dat))) return false;
    const oj::ReplayHeader& h = view.replay.header;
    setReplayRace(data.race, h);
    oj::log::info("race: the replay {} of {} {} ({} cars, {:.2f} s)", data.replayName, h.track, h.route, h.cars,
                  h.length);
    return true;
}

// 0x00495DE0 on the network (in place of every mode's): the session of the players in the race and not gone
// (0x004B8C90), the race's state with the session's laps ([0x0075EFE0]); each such player a new car (0x0048DB40) of
// the CarDef of its config's name ("_STOCK" added, the first CarDef when none, 0x0048CC30) with its config applied
// (0x0048E7D0), its grid made, on its grid place (+0xC4) as ours of index 0 (kind 0), ours of another index (the AI,
// 1) or another machine's (2: Stock problem, kind 0 for a player before the race's player count, 1 here, whoever's it
// is), crew 0, the k-th car made; the player's car (+0xF0) it. The session's hooks on the race's cars.
void netGrid(TrackView& view, oj::GameData& data, oj::scene::Engine& engine, oj::net::NetSession& session) {
    const oj::RaceSetup& race = data.race;
    oj::Career& career = *data.career;
    oj::CrtRandom& random = view.loading;
    std::vector<oj::NetEntry> entries;
    for (const oj::net::NetPlayer& p : session.players()) {
        if (p.race.notRacing != 0 || p.race.left != 0) continue;
        entries.push_back({p.peer == -1, p.localIndex, p.race.gridPlace, std::wstring(p.name.begin(), p.name.end())});
    }
    oj::setupNetSession(career, entries, random);
    oj::RaceRules rules;
    rules.mode = race.mode;
    rules.type = race.type;
    rules.lapTime = race.lapTime;
    rules.speed = race.speed;
    rules.heat = race.heat;
    rules.heats = race.heats;
    rules.players = race.players;
    rules.minutes = race.difficulty;
    rules.session = &career.session;
    rules.career = &career;
    rules.settings = &data.settings;
    rules.network = true;
    rules.spectator = session.spectator();
    rules.netStage = [] {
        oj::net::NetGame* n = oj::net::netGame();
        return n != nullptr ? n->session.stage() : oj::net::SessionStage::Racing;
    };
    // 0x0049FFB1: the race time and the best lap as they are (their bits); the player's place in the players as its
    // index (0x004EBBA0).
    rules.netFinished = [](const oj::Competitor& c) {
        oj::net::NetGame* n = oj::net::netGame();
        if (n == nullptr) return;
        int32_t index = 0;
        const std::vector<oj::net::NetPlayer>& players = n->session.players();
        for (size_t i = 0; i < players.size(); ++i) {
            if (players[i].raceCar == c.car) {
                index = static_cast<int32_t>(i);
                break;
            }
        }
        n->session.finished(std::bit_cast<int32_t>(c.raceTime), std::bit_cast<int32_t>(c.bestLap), index);
    };
    view.netHooks(session);
    view.prepareRace(session.laps(), rules);
    const oj::Spl& lines = view.scene->lines();
    if (lines.grid.empty()) return;
    int32_t made = 0;
    std::vector<oj::net::NetPlayer>& players = session.players();
    for (size_t i = 0; i < players.size(); ++i) {
        oj::net::NetPlayer& p = players[i];
        if (p.race.notRacing != 0 || p.race.left != 0) continue;
        auto car = std::make_unique<oj::Car>(oj::makeCar(oj::findCarDef(data.defs, std::string(p.car.name.data()))));
        p.car.apply(*car);
        if (!car->grid) car->grid.emplace();
        oj::Car* raced = car.get();
        view.ownCars.push_back(std::move(car));
        const oj::Spl::GridPoint& at = lines.grid[static_cast<size_t>(std::max(p.race.gridPlace, 0)) % lines.grid.size()];
        auto model = oj::scene::CarModel::load(engine, *raced, data.catalogue, random, false);
        oj::scene::CarModel* shown = view.scene->addCar(std::move(model), at);
        const oj::CarDynamics* dynamics = data.dynamics && raced->def != nullptr ? data.dynamics->of(*raced->def) : nullptr;
        if (dynamics == nullptr) continue;
        const oj::CarKind kind =
            p.peer == -1 ? (p.localIndex == 0 ? oj::CarKind::Player : oj::CarKind::Ai)
                         : (static_cast<int32_t>(i) < race.players ? oj::CarKind::Player : oj::CarKind::Remote);
        const size_t before = view.cars.size();
        view.addCar(*dynamics, *raced, raced, data, shown, at, kind, 0, made, true);
        if (view.cars.size() > before) p.raceCar = &view.cars.back()->car;
        ++made;
    }
}

// 0x00495DE0 for a replay's race: each car the replay read (0x00426A50; its grid object made when it has none,
// 0x005562A0) on the grid place of its number, all driven by the replay; the first player's camera on the first car.
void replayGrid(TrackView& view, oj::GameData& data, oj::scene::Engine& engine) {
    const oj::RaceSetup& race = data.race;
    oj::RaceRules rules;
    rules.mode = race.mode;
    rules.type = race.type;
    rules.heat = race.heat;
    rules.heats = race.heats;
    rules.players = race.players;
    rules.minutes = race.difficulty;
    rules.settings = &data.settings;
    if (data.career) {
        rules.session = &data.career->session;
        rules.career = data.career.get();
    }
    view.prepareRace(race.laps, rules);
    const oj::Spl& lines = view.scene->lines();
    if (lines.grid.empty()) return;
    oj::CrtRandom& random = view.loading;
    for (size_t i = 0; i < view.replay.savedCars.size(); ++i) {
        oj::Car car = view.replay.savedCars[i];
        car.def = oj::findCarDef(data.defs, car.modelName());
        if (car.def == nullptr) continue;
        if (!car.grid) car.grid.emplace();
        const oj::Spl::GridPoint& at = lines.grid[i % lines.grid.size()];
        auto model = oj::scene::CarModel::load(engine, car, data.catalogue, random, false);
        oj::scene::CarModel* shown = view.scene->addCar(std::move(model), at);
        const oj::CarDynamics* dynamics = data.dynamics ? data.dynamics->of(*car.def) : nullptr;
        if (dynamics != nullptr) {
            view.addCar(*dynamics, car, nullptr, data, shown, at, oj::CarKind::Ai, static_cast<int32_t>(i),
                        static_cast<int32_t>(i), true);
        }
    }
    if (!view.cars.empty()) {
        view.first().slot = view.cars[0].get();
        view.cruiseReset();
    }
}

// 0x00495DE0 for an arcade race (mode 0, off line, not a replay): the session of the race's entries (0x004B93C0), the
// challenge's outcome (+0x7C) 0, the race's cars made (0x0042B000, the loading thread's rand()); each entry's car on
// grid place k as the player's (kind 0) or an AI's, of crew its number, its competitor named by the short name of the
// first member of that crew ([career + 0x08 + 4 x number] + 0xDE). The AI drivers' skill and the nitro by the race's
// +0x24 and +0x04 (the race's rules). The stock car keeps its name; here the session's racer does.
void arcadeGrid(TrackView& view, oj::GameData& data, oj::scene::Engine& engine) {
    oj::RaceSetup& race = data.race;
    oj::Career& career = *data.career;
    oj::Arcade& arcade = data.arcade;
    oj::CrtRandom& random = view.loading;
    const size_t at = arcade.current();
    std::vector<oj::ArcadeEntry> entries;
    for (int32_t k = 0; k < arcade.i32(at + oj::Arcade::kRaceCarCount); ++k) {
        const size_t entry = at + oj::Arcade::kRaceCars + static_cast<size_t>(k) * oj::Arcade::kEntrySize;
        entries.push_back(
            {arcade.i32(entry + oj::Arcade::kEntryPlayer) != 0, arcade.i32(entry + oj::Arcade::kEntryOpponent)});
    }
    oj::setupArcadeSession(career, entries, random);
    race.challenge = oj::ChallengeOutcome::None;
    data.makeArcadeRaceCars(random);
    oj::RaceRules rules;
    rules.mode = race.mode;
    rules.type = race.type;
    rules.lapTime = race.lapTime;
    rules.speed = race.speed;
    rules.heat = race.heat;
    rules.heats = race.heats;
    rules.players = race.players;
    rules.minutes = race.difficulty;
    rules.session = &career.session;
    rules.career = &career;
    rules.settings = &data.settings;
    rules.arcade = &arcade;
    rules.arcadeSkill = arcade.f32(at + oj::Arcade::kRaceSkill);
    rules.arcadeNitro = arcade.i32(at + oj::Arcade::kRaceExtreme) != 0;
    view.prepareRace(race.laps, rules);
    const oj::Spl& lines = view.scene->lines();
    if (lines.grid.empty()) return;
    for (size_t k = 0; k < entries.size() && k < data.arcadeRaceCars.size(); ++k) {
        oj::Car& car = data.arcadeRaceCars[k];
        const oj::Spl::GridPoint& place = lines.grid[k % lines.grid.size()];
        const auto crew = static_cast<size_t>(entries[k].player ? 0 : entries[k].number);
        if (k < career.session.racers.size() && crew < career.crews.size()) {
            const auto& shortName = career.crews[crew].memberShortNames[0];
            std::wstring name;
            for (size_t c = 0; c < shortName.size() && shortName[c] != 0; ++c) {
                name += static_cast<wchar_t>(static_cast<unsigned char>(shortName[c]));
            }
            career.session.racers[k].name = name;
        }
        auto model = oj::scene::CarModel::load(engine, car, data.catalogue, random, false);
        oj::scene::CarModel* shown = view.scene->addCar(std::move(model), place);
        const oj::CarDynamics* dynamics = data.dynamics && car.def != nullptr ? data.dynamics->of(*car.def) : nullptr;
        if (dynamics != nullptr) {
            view.addCar(*dynamics, car, &car, data, shown, place,
                        entries[k].player ? oj::CarKind::Player : oj::CarKind::Ai, entries[k].number,
                        static_cast<int32_t>(k), true);
        }
    }
}

// 0x00461A10: the front end's first window after a race, by the race's mode: an arcade race Arcade_Race (Arcade_Results
// while any of the arcade's news +0x53C8 .. +0x53D4 is set), a custom race instant_manu, a split screen
// race multi, mode 4 start; a career race: the dealer's +0x10 set; type 9 garageload (the dealer's +0x0C and the
// career's +0xA4 cleared); else the session's end (0x004A3A40: 0x004BA8A0, 0x004B9E20) and
// by the current event: a cruise (type 2) the event's end (0x004A6870) and CMSMeetingSummary while the career's +0x38
// list has entries, CMSRespectChanges while the +0x3C list has, else the history's step (0x004BD8A0) and CMSMainMenu;
// a challenge (+0x05) CMSChallengeResults, else CMSRaceResults. In a network game (0x00461E71, 0x00462028) an arcade or
// a custom race goes to link_lobby while the session has players (0x004EA000), else link_search (online_main
// online). Before all that a window named by [0x0075ED18] (a saved replay's read sets "menu"; the network's quits
// set it in the stock game, not here), emptied.
std::string windowAfterRace(oj::GameData& data) {
    if (!data.afterRaceWindow.empty()) {
        std::string named = data.afterRaceWindow;
        data.afterRaceWindow.clear();
        return named;
    }
    const oj::RaceSetup& race = data.race;
    if (oj::net::NetGame* net = oj::net::netGame();
        net != nullptr && (race.mode == RaceMode::Arcade || race.mode == RaceMode::Custom)) {
        return net->session.players().empty() ? "link_search" : "link_lobby";
    }
    switch (race.mode) {
    case RaceMode::Arcade: {
        const oj::Arcade& a = data.arcade;
        const bool news = a.i32(oj::Arcade::kFinalUnlocked) != 0 || a.i32(oj::Arcade::kSeriesComplete) != 0 ||
                          a.i32(oj::Arcade::kNextSeries) != 0 || a.i32(oj::Arcade::kArcadeComplete) != 0;
        return news ? "Arcade_Results" : "Arcade_Race";
    }
    case RaceMode::Custom: return "instant_manu";
    case RaceMode::SplitScreen: return "multi";
    case RaceMode::Attract: return "start";
    default:
        break;
    }
    if (race.mode != RaceMode::Career || !data.career) return "start";
    oj::Career& career = *data.career;
    career.dealer.afterRace = 1;
    if (race.type == RaceType::TestTrack) {
        career.dealer.fromStats = 0;
        data.careerScreen = oj::CareerScreen::CarSel;
        return "garageload";
    }
    // The session's end (0x004A3A40): 0x004BA8A0 with the job thread's rand() (a new thread's, from 1) and the route's
    // record, then 0x004B9E20.
    oj::CrtRandom job;
    data.endSession(job);
    oj::CalendarEvent* event = oj::currentEvent(career.calendar);
    if (event != nullptr && event->type == oj::EventKind::Showoff) {
        oj::endEvent(career, *event, data.tables);
        if (career.meeting.count != 0) return "CMSMeetingSummary";
        if (career.respectChanges.count != 0) return "CMSRespectChanges";
        oj::recordHistory(career);
        return "CMSMainMenu";
    }
    if (event != nullptr && event->challenge != 0) return "CMSChallengeResults";
    return "CMSRaceResults";
}

}  // namespace app
