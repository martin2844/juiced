// A sprint's results.

#include "Game/Race/Hud/SprintResults.h"
#include "Game/Race/Hud/HudInternal.h"

#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Core/Network/OnlineRecords.h"

namespace oj {

using namespace hud;

namespace {

// A competitor's name (+0x14): here its car's racer of the session.
std::wstring racerName(const HudRace& race, const Competitor& c) {
    const RaceCar* car = c.car;
    if (race.career == nullptr || car == nullptr || car->record < 0 ||
        static_cast<size_t>(car->record) >= race.career->session.racers.size()) {
        return {};
    }
    return race.career->session.racers[static_cast<size_t>(car->record)].name;
}

// The lap's records at the end of a sprint (0x00454DB0's first frame; in the career only with its race cars
// chosen): the best lap of the players' cars (from 999999) to the online statistics (0x00454EDF), then against the
// route's record (0x0040A1C0), a better one kept with the driver's name and the car's model (0x0040A220);
// newhighscore_cms is not asked here.
void sprintRecords(const HudRace& race) {
    if (race.mode == RaceMode::Career && !race.raceCarsChosen) return;
    float best = 999999.0f;
    size_t player = 0;
    for (size_t i = 0; i < race.players.size(); ++i) {
        const RaceCar* car = race.players[i].car;
        if (car != nullptr && car->competitor != nullptr && car->competitor->bestLap < best) {
            best = car->competitor->bestLap;
            player = i;
        }
    }
    if (race.lapTimeStatistics) race.lapTimeStatistics(player, false);
    if (race.highScores == nullptr || race.tables == nullptr) return;
    if (!race.highScores->better(race.routeIndex, best, *race.tables)) return;
    const HudCar& p = race.players[player];
    const int32_t model = p.record != nullptr && p.record->def != nullptr ? p.record->def->id : 0;
    race.highScores->keep(race.routeIndex, race.driverName, best, model, *race.tables);
    log::info("race: a new record of route {}: {:.2f} s", race.routeIndex, best);
}

// The sprint's order (0x00454DB0 and 0x004542D0): the indices given bubble-sorted by the points (+0x138), more first,
// then by the time of the heats (+0x134), less first.
void sprintOrder(std::vector<const Competitor*>& order) {
    for (size_t i = 0; i + 1 < order.size();) {
        const Competitor& a = *order[i];
        const Competitor& b = *order[i + 1];
        if (a.points < b.points || (a.points == b.points && b.totalTime < a.totalTime)) {
            std::swap(order[i], order[i + 1]);
            i = 0;
            continue;
        }
        ++i;
    }
}

}  // namespace

SprintResults::SprintResults(ui::WindowManager& windows, const TextTable& texts)
    : m_windows(windows), m_texts(texts) {}

// 0x00454760: the career's respect in counter (careerCounter); the 3D world ([G+0x50]) taken down to a quarter of its
// player's master, kept (+0x2C); the sounds' +0x24 set; each player's messages emptied; the rows' elements Pos%d,
// Name%d, Team%d, Car%d, Time%d and PosBar%d (0 .. 3) found; Large "HEAT n / heats"; SelectText and SelectIcon hidden;
// +0x20 and +0x24 0; the rows filled (0x00453BA0; 0x00453FB0 online); the controller let go; +0x1C 1, +0x10 0, +0x14 1,
// +0x18 0, +0x28 -1; nextheat "LEADERBOARD" ("Leave Session" / "End Race" online); the time 1 s.
bool SprintResults::enter(const HudRace& race, RaceHud* hud) {
    m_screen = m_windows.open("hud_sprint_results");
    if (!m_screen->loaded()) {
        m_screen.reset();
        return false;
    }
    // The 3D world's player's master kept (+0x2C) and the world going down to a quarter of it (down 400, up 50, the
    // master at its volume); the speech leaves the world alone (+0x24).
    if (audio::World* world = gameSounds().world) {
        m_master = world->player().masterVolume();
        world->target = 0.25f * m_master;
        world->down = 400.0f;
        world->up = 50.0f;
        world->player().setMasterVolume(world->volume);
    }
    if (Speech* speech = gameSounds().speech) speech->holdWorld = true;
    ui::Screen& s = *m_screen;
    careerCounter(s, race);
    if (hud != nullptr) {
        for (size_t i = 0; i < race.players.size(); ++i) hud->messages(i).clear();
    }
    for (size_t i = 0; i < 4; ++i) {
        const std::string n = std::to_string(i);
        m_pos[i] = s.find("Pos" + n);
        m_name[i] = s.find("Name" + n);
        m_team[i] = s.find("Team" + n);
        m_car[i] = s.find("Car" + n);
        m_heatTime[i] = s.find("Time" + n);
        m_bar[i] = s.find("PosBar" + n);
    }
    if (const int large = s.find("Large"); large >= 0) {
        s.setText(large, m_texts.at(kTextHeat) + widen(std::format(" {} / {}", race.heat + 1, race.heats)));
    }
    for (int e = 0; e < s.size(); ++e) {
        const char* name = s.nameOf(e).c_str();
        if (_stricmp(name, "SelectText") == 0 || _stricmp(name, "SelectIcon") == 0) s.setHidden(e, true);
    }
    m_allFinished = 0.0f;
    m_board = 0;
    fill(race);
    m_notSaving = 1;
    m_recordsDone = 0;
    m_settled = 1;
    m_closing = 0;
    m_last = -1;
    if (const int next = s.find("nextheat"); next >= 0) s.setText(next, m_texts.at(kTextNextHeat + 1));
    m_time = 1.0f;
    return true;
}

// 0x00453BA0 off the network: +0xC0 0; the controller let go; for each of the four rows a finisher of the heat in its
// order gives its name (+0x14), its crew (line 108 + the car's +0x47C), "<points> Pts  (<race time>)" with the points
// 6, 4, 2, 0 of the row (0x0044FBD0) and its car (line 211 + the model's id), the player's row highlighted; +0xC0
// counts the finishers' rows. Past the finishers Name, Team and Car "----------", Time "--:--:--"; rows from the
// vehicles' count on are hidden. Every competitor finished: +0x20 0.1.
void SprintResults::fill(const HudRace& race) {
    ui::Screen& s = *m_screen;
    m_filled = 0;
    const Competitor* own = !race.players.empty() && race.players[0].car != nullptr
                                ? race.players[0].car->competitor
                                : nullptr;
    const size_t finishers = race.race != nullptr ? race.race->finishers.size() : 0;
    const auto set = [&](int e, const std::wstring& text) {
        if (e >= 0) s.setText(e, text);
    };
    int32_t points = 6;
    for (size_t i = 0; i < 4; ++i, points -= 2) {
        if (i < finishers) {
            const Competitor* c = race.race->finishers[i];
            if (c != nullptr) {
                const RaceCar* car = c->car;
                set(m_name[i], racerName(race, *c));
                if (car != nullptr) set(m_team[i], m_texts.at(kTextCrewNames + static_cast<size_t>(car->crew)));
                set(m_heatTime[i], widen(std::format("{} ", points)) + m_texts.at(kTextPts) +
                                       widen(std::format("  ({})", hudTime(c->raceTime))));
                if (car != nullptr && car->carRecord != nullptr && car->carRecord->def != nullptr) {
                    set(m_car[i], m_texts.at(kTextModels + static_cast<size_t>(car->carRecord->def->id)));
                }
                if (c == own) {
                    for (const int e : {m_name[i], m_team[i], m_heatTime[i], m_car[i]}) {
                        if (e >= 0) s.setHighlighted(e, true);
                    }
                }
            }
            ++m_filled;
        } else {
            set(m_name[i], L"----------");
            set(m_team[i], L"----------");
            set(m_heatTime[i], L"--:--:--");
            set(m_car[i], L"----------");
        }
        if (i >= race.cars.size()) {
            for (const int e : {m_pos[i], m_name[i], m_heatTime[i], m_team[i], m_car[i], m_bar[i]}) {
                if (e >= 0) s.setHidden(e, true);
            }
        }
    }
    if (race.race != nullptr && race.race->competitors().size() == finishers) m_allFinished = 0.1f;
}

// 0x004542D0: SelectText and SelectIcon shown; Large "sprint leaderboard"; the heat's first four finishers in the
// sprint's order (sprintOrder) give their names, crews, points ("%d") and cars, the player's row highlighted (its car
// not) and, in the last heat, its place to the arcade's result (0x0042B290); past the finishers Name and Team
// "----------", Time "--:--:--"; rows from the vehicles' count on are hidden but for Car. +0xC0 counts the rows on.
void SprintResults::leaderboard(const HudRace& race) {
    ui::Screen& s = *m_screen;
    for (int e = 0; e < s.size(); ++e) {
        const char* name = s.nameOf(e).c_str();
        if (_stricmp(name, "SelectText") == 0 || _stricmp(name, "SelectIcon") == 0) s.setHidden(e, false);
    }
    if (const int large = s.find("Large"); large >= 0) s.setText(large, m_texts.at(kTextSprintLeaderboard));
    std::vector<const Competitor*> order;
    if (race.race != nullptr) {
        for (size_t i = 0; i < race.race->finishers.size() && i < 4; ++i) order.push_back(race.race->finishers[i]);
    }
    sprintOrder(order);
    const Competitor* own = !race.players.empty() && race.players[0].car != nullptr
                                ? race.players[0].car->competitor
                                : nullptr;
    const auto set = [&](int e, const std::wstring& text) {
        if (e >= 0) s.setText(e, text);
    };
    for (size_t i = 0; i < 4; ++i) {
        if (i < order.size()) {
            const Competitor* c = order[i];
            const RaceCar* car = c->car;
            set(m_name[i], racerName(race, *c));
            if (car != nullptr) set(m_team[i], m_texts.at(kTextCrewNames + static_cast<size_t>(car->crew)));
            set(m_heatTime[i], widen(std::format("{}", x87::chop(c->points))));
            if (car != nullptr && car->carRecord != nullptr && car->carRecord->def != nullptr) {
                set(m_car[i], m_texts.at(kTextModels + static_cast<size_t>(car->carRecord->def->id)));
            }
            if (c == own) {
                for (const int e : {m_name[i], m_team[i], m_heatTime[i]}) {
                    if (e >= 0) s.setHighlighted(e, true);
                }
                if (race.heat == race.heats - 1 && race.race != nullptr) {
                    race.race->arcadeResult(static_cast<int32_t>(i) + 1);
                }
            }
            ++m_filled;
        } else {
            set(m_name[i], L"----------");
            set(m_team[i], L"----------");
            set(m_heatTime[i], L"--:--:--");
        }
        if (i >= race.cars.size()) {
            for (const int e : {m_pos[i], m_name[i], m_team[i], m_heatTime[i], m_bar[i]}) {
                if (e >= 0) s.setHidden(e, true);
            }
        }
    }
}

// 0x00454DB0: the 3D world's voices paused once its player's master is down to a quarter of the one kept. +0x18 has the
// race's respect worked out (0x004BCD90 with its collection flag) and asks for closedown. The controller let go each 4
// s. +0x14 0 (the sprint's end taken): outside the career or with its race cars chosen (the online tables not
// followed), while the records were not written ([G+0xA8]+0x08), newhighscore_cms over the window (mode 5); +0x14 1 and
// with +0x1C +0x18 set. Else +0x1C 0 sets +0x18; the first frame (+0x10 0) looks at the records (sprintRecords) and
// ends there. Then off the network the rows filled again while more have finished than they show. SELECT (CLICK) once
// every competitor finished: first the leaderboard (nextheat "NEXT HEAT", "CONTINUE" after the last heat; +0x24); then
// the speech stopped (0x0040B600) and before the last heat CLICK and restart_race; after it the end (+0x28 1): the
// vehicles in the sprint's order, the career's race session's first place cleared (+0x368 -1) and in the career each
// racer finished in that order with its heats' time (0x004B9CF0), the replay's play flag cleared (+0x828; its [11]
// after, 0x0055FE30, is empty), +0x1C 1 when the save system does not save, +0x14 0, +0x28 -1. Stock problem: the order
// is of four indices kept on the stack, the first four vehicles; with more the sort reads past them.
RaceWindowFrame SprintResults::frame(const HudRace& race, float dt, uint32_t uiEvent) {
    RaceWindowFrame out;
    if (m_screen == nullptr) return out;
    // The 3D world's voices paused once its master is down to a quarter of the one kept.
    if (audio::World* world = gameSounds().world; world != nullptr) {
        if (world->player().masterVolume() <= 0.25f * m_master) world->manager().pauseAll(true);
    }
    ui::Screen& s = *m_screen;
    if (uiEvent != input::kNone) s.finishTransitions();
    if (m_closing != 0) {
        raceRespectOf(race, false, true);
        out.window = "closedown";
        return out;
    }
    m_time = m_time - dt;
    if (m_time < 0.0f) {
        out.useUi = true;
        m_time = 4.0f;
    }
    if (m_settled == 0) {
        if ((race.mode != RaceMode::Career || race.raceCarsChosen) && race.highScores != nullptr &&
            race.highScores->written == 0) {
            out.highScores = true;
        }
        m_settled = 1;
        if (m_notSaving != 0) m_closing = 1;
        return out;
    }
    if (m_notSaving == 0) m_closing = 1;
    if (m_recordsDone == 0) {
        sprintRecords(race);
        m_recordsDone = 1;
        return out;
    }
    const auto finishers = race.race != nullptr ? static_cast<int32_t>(race.race->finishers.size()) : 0;
    if (m_filled < finishers) {
        fill(race);
        out.useUi = true;  // 0x00453BDA
    }
    if (uiEvent == input::kSelect && m_allFinished != 0.0f) {
        if (s.sound) s.sound("CLICK");  // 0x004553A4
        if (m_board == 0) {
            if (const int next = s.find("nextheat"); next >= 0) {
                s.setText(next, m_texts.at(race.heat + 1 == race.heats ? kTextContinue : kTextNextHeat));
            }
            m_board = 1;
            leaderboard(race);
            return out;
        }
        if (Speech* speech = gameSounds().speech) speech->stop();  // 0x0040B600
        if (race.heat + 1 < race.heats) {
            if (s.sound) s.sound("CLICK");  // 0x0045544B
            out.window = "restart_race";
            out.stopRecord = true;  // 0x0045545B
        } else if (m_last != 1) {
            m_last = 1;
        }
    }
    if (m_last == 1 && race.race != nullptr) {
        std::vector<const Competitor*> order;
        for (const RaceCar* car : race.cars) {
            if (car != nullptr && car->competitor != nullptr) order.push_back(car->competitor);
        }
        sprintOrder(order);
        if (race.career != nullptr) {
            Career& career = *const_cast<Career*>(race.career);
            career.session.winner = -1;
            if (race.mode == RaceMode::Career) {
                for (const Competitor* c : order) {
                    racerFinished(career, c->car->record, c->totalTime, c->bestLap, false);
                }
            }
        }
        m_notSaving = race.saving ? 0 : 1;
        m_settled = 0;
        m_last = -1;
    }
    return out;
}

void SprintResults::draw(float seconds, float dt) {
    if (m_screen == nullptr) return;
    m_screen->update(dt);
    m_screen->draw(seconds, dt);
}

}  // namespace oj
