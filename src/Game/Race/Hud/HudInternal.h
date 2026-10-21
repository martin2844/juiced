#pragma once
// What the race's HUD files share of their own helpers.

#include "Core/Data/GameTexts.h"
#include "Game/Race/Hud/RaceHud.h"

#include "Audio/SoundSystem.h"
#include "Core/Base/Log.h"
#include "Core/Base/X87.h"
#include "Core/Career/Career.h"
#include "Core/Network/OnlineSessions.h"
#include "Core/Race/Cruise.h"
#include "Core/Race/RaceSession.h"
#include "Core/Race/Racer.h"
#include "Core/Race/Replay.h"
#include "Game/Media/Music.h"
#include "Game/Race/Hud/RaceWindows.h"
#include "Game/Scene/Scene3d.h"

#include <cstdio>

namespace oj::hud {

constexpr float kNeverFades = 1.0e7f;  // the panels' fade timer of the races that keep them

inline std::string format(const char* f, int v) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), f, v);
    return buffer;
}

// The race types whose HUD keeps the records' panels (and leaves out the local and world times).
inline bool keepsPanels(RaceType type) {
    return type == RaceType::PerfectLap || type == RaceType::Speed || type == RaceType::Sprint ||
           type == RaceType::Cruise || type == RaceType::Solo;
}

                                       // TARGET SPEED NOT REACHED

inline std::wstring widen(const std::string& s) {
    std::wstring w;
    w.reserve(s.size());
    for (char c : s) w.push_back(static_cast<wchar_t>(static_cast<signed char>(c)));
    return w;
}

// The session's racers' cars by their records (the racer's +0x2C).
inline std::vector<const RaceCar*> sessionCars(const HudRace& race, const Career& career) {
    std::vector<const RaceCar*> cars(career.session.racers.size(), nullptr);
    for (const RaceCar* c : race.cars) {
        if (c != nullptr && c->record >= 0 && static_cast<size_t>(c->record) < cars.size()) {
            cars[static_cast<size_t>(c->record)] = c;
        }
    }
    return cars;
}

// 0x004BCD90 as the windows call it: the career's +0x34 object's +0x30 cleared, then only in a career race ([G+0x6C]
// mode 2) not of type 9 the race's respect (raceRespect).
inline float raceRespectOf(const HudRace& race, bool lastPlace, bool collection) {
    if (race.career == nullptr) return 0.0f;
    Career& career = *const_cast<Career*>(race.career);
    career.news.respect = 0.0f;
    if (race.mode != RaceMode::Career || race.type == RaceType::TestTrack || race.catalogue == nullptr) return 0.0f;
    const size_t finishers = race.race != nullptr ? race.race->finishers.size() : 0;
    return raceRespect(career, sessionCars(race, career), finishers, lastPlace, collection, *race.catalogue);
}

// 0x00450B60 / 0x00451270: with a speech player its +0x0C set and its slots paused, the 3D world's voices paused, the
// sound system stepped.
inline void resultsQuiet() {
    if (Speech* speech = gameSounds().speech; speech != nullptr && speech->player() != nullptr) {
        speech->paused = true;
        speech->player()->pauseAll();
    }
    if (audio::World* world = gameSounds().world) world->manager().pauseAll(true);
    if (audio::system().on()) audio::system().update();
}

// The lap's records looked at in the first frame of the pause (0x0044EEA0; not in the career without the race cars
// chosen) and of the results (0x00453450): the best lap of the players' cars (from 999999; the results leave out a
// sprint's) against the route's record (0x0040A1C0);
// a better one is kept with the driver's name and the car's model (0x0040A220) and newhighscore_cms is asked over the
// window (by the pause also while a record is not written): true. The pause's best lap above 0 goes to the online
// statistics first (0x0044F199, the game's).
inline bool lookAtRecords(const HudRace& race, bool results) {
    if (!results && race.mode == RaceMode::Career && !race.raceCarsChosen) return false;
    float best = 999999.0f;
    size_t player = 0;
    for (size_t i = 0; i < race.players.size(); ++i) {
        const RaceCar* car = race.players[i].car;
        if (car == nullptr || car->competitor == nullptr || (results && race.type == RaceType::Sprint)) continue;
        if (best > car->competitor->bestLap) {
            best = car->competitor->bestLap;
            player = i;
        }
    }
    if (!results && best > 0.0f && race.lapTimeStatistics) race.lapTimeStatistics(player, true);
    if (race.highScores == nullptr || race.tables == nullptr) return false;
    HighScores& records = *race.highScores;
    bool asked = false;
    if (records.better(race.routeIndex, best, *race.tables)) {
        const HudCar* p = player < race.players.size() ? &race.players[player] : nullptr;
        if (p != nullptr && p->car != nullptr) {
            const int32_t model = p->record != nullptr && p->record->def != nullptr ? p->record->def->id : 0;
            records.keep(race.routeIndex, race.driverName, best, model, *race.tables);
            log::info("race: a new record of route {}: {:.2f} s", race.routeIndex, best);
            asked = true;
        }
    } else if (!results && records.written == 0) {
        asked = true;
    }
    return asked;
}

// The career's respect of the race so far (its +0x34 object's +0x30) in counter: "+n" green over 0, else "n" red
// (0xFF32FF32, 0xFFFF3232 as its colour and its highlight's); outside the career every Respect_Icon and Counter
// hidden.
inline void careerCounter(ui::Screen& s, const HudRace& race) {
    if (race.mode == RaceMode::Career) {
        const int counter = s.find("counter");
        if (counter < 0) return;
        const float respect = race.career != nullptr ? race.career->news.respect : 0.0f;
        const auto shown = x87::chop(respect);
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), respect > 0.0f ? "+%d" : "%d", shown);
        s.setFontColour(counter, respect > 0.0f ? 0xFF32FF32u : 0xFFFF3232u);
        s.setText(counter, widen(buffer));
        return;
    }
    for (int e = 0; e < s.size(); ++e) {
        const char* name = s.nameOf(e).c_str();
        if (_stricmp(name, "Respect_Icon") == 0 || _stricmp(name, "Counter") == 0) s.setHidden(e, true);
    }
}

}  // namespace oj::hud
