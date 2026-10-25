// The race's results' windows.

#include "Game/Race/Hud/RaceResults.h"
#include "Game/Race/Hud/HudInternal.h"

#include "Core/Data/GameTexts.h"
#include "Core/Input/Input.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/OnlineRecords.h"

#include <algorithm>
#include <bit>

namespace oj {

using namespace hud;

namespace {

}  // namespace

RaceResults::RaceResults(ui::WindowManager& windows, const TextTable& texts)
    : m_windows(windows), m_texts(texts) {}

// 0x00452E60: in the career with the profile's +0x00 at 0 counter shows the race's respect (the career's +0x34
// object's +0x30, cut: "+n" in 0xFF32FF32 over 0, else "n" in 0xFFFF3232), else every Respect_Icon and Counter
// hidden; each player's messages emptied; the rows' elements Pos%d, Name%d, Team%d, Car%d, Time%d and PosBar%d found
// (none: 0); the table filled; the controller let go; +0x14 set when the save system does not save; the lap's records
// taken as looked at (+0x10) in the career without its race cars chosen; the menu's RESTART taken out online and in
// the career, REPLAY online; QUIT selected; the time 1 s.
bool RaceResults::enter(const HudRace& race, RaceHud* hud) {
    m_screen = m_windows.open("hud_race_results");
    if (!m_screen->loaded()) {
        m_screen.reset();
        return false;
    }
    if (Speech* speech = gameSounds().speech) speech->holdWorld = true;  // +0x24
    ui::Screen& s = *m_screen;
    if (race.mode == RaceMode::Career && race.career != nullptr && race.career->profile.introduction == 0) {
        if (const int counter = s.find("counter"); counter >= 0) {
            const float respect = race.career->news.respect;
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), respect > 0.0f ? "+%d" : "%d", x87::chop(respect));
            s.setFontColour(counter, respect > 0.0f ? 0xFF32FF32u : 0xFFFF3232u);
            s.setText(counter, widen(buffer));
        }
    } else {
        for (int e = 0; e < s.size(); ++e) {
            const char* name = s.nameOf(e).c_str();
            if (_stricmp(name, "Respect_Icon") == 0 || _stricmp(name, "Counter") == 0) s.setHidden(e, true);
        }
    }
    if (hud != nullptr) {
        for (size_t i = 0; i < race.players.size(); ++i) hud->messages(i).clear();
    }
    for (size_t i = 0; i < 6; ++i) {
        const std::string n = std::to_string(i);
        m_pos[i] = s.find("Pos" + n);
        m_name[i] = s.find("Name" + n);
        m_team[i] = s.find("Team" + n);
        m_car[i] = s.find("Car" + n);
        m_raceTime[i] = s.find("Time" + n);
        m_bar[i] = s.find("PosBar" + n);
    }
    m_placeGiven.fill(0);
    fill(race);
    m_notSaving = race.saving ? 0 : 1;
    m_started = race.mode == RaceMode::Career && !race.raceCarsChosen ? 1 : 0;
    if (const int menu = s.find("Menu"); menu >= 0) {
        const std::string& m = s.nameOf(menu);
        s.restoreMenu(m);
        if (race.networked) s.removeMenuItem(m, "restart");
        if (race.mode == RaceMode::Career) s.removeMenuItem(m, "restart");
        if (race.networked) s.removeMenuItem(m, "replay");
        for (size_t i = 0; i < s.menuSize(m); ++i) {
            if (_stricmp(s.menuItemCommand(m, i).c_str(), "quit") == 0) {
                // On the network QUIT reads "End Race" (2007) for the host, "Leave Session" (2008) for a client.
                if (race.networked) {
                    s.setMenuItemText(m, i, m_texts.at(race.host ? kTextEndRace : kTextLeaveSessionItem));
                }
                s.selectMenuItem(m, i);
                break;
            }
        }
    }
    m_time = 1.0f;
    return true;
}

// 0x00451C90: an online career session of teams (the matchmaking, [0x0075EF88] 1, the session's team over 1) has its
// own table (0x00452330: fillOnlineTeams), the network's another (0x00452030: fillNet). Off the network (a career
// round whose +0x17 is over 1 has its own table, 0x004529E0: fillCrews): for
// each of the six rows, a finisher in the race's order gives its driver's name, its crew (line 108 + the crew; empty
// in a split race), its race time (0x0044FBD0) and its car (line 211 + the model's id), the player's row highlighted;
// past the finishers Name and Car "----------", Team and Time "--:--:--" (the fixed build's Team "----------", as Name
// and Car); rows past the race's vehicles are hidden.
void RaceResults::fill(const HudRace& race) {
    ui::Screen& s = *m_screen;
    m_filled = 0;
    if (net::matchmaking() != nullptr && net::onlineHosted().mode == net::OnlineKind::Career &&
        1 < net::onlineHosted().career.team && race.net != nullptr) {
        fillOnlineTeams(race);
        return;
    }
    if (race.networked && race.net != nullptr) {
        fillNet(race);
        return;
    }
    if (race.mode == RaceMode::Career && race.type != RaceType::TestTrack && race.career != nullptr &&
        race.career->session.round != nullptr && race.career->session.round->teamSize > 1) {
        fillCrews(race);
        return;
    }
    const Competitor* own = !race.players.empty() && race.players[0].car != nullptr
                                ? race.players[0].car->competitor
                                : nullptr;
    const size_t finishers = race.race != nullptr ? race.race->finishers.size() : 0;
    const auto set = [&](int e, const std::wstring& text) {
        if (e >= 0) s.setText(e, text);
    };
    for (size_t i = 0; i < 6; ++i) {
        if (i < finishers) {
            const Competitor* c = race.race->finishers[i];
            if (c != nullptr) {
                const RaceCar* car = c->car;
                std::wstring name;
                if (race.career != nullptr && car != nullptr && car->record >= 0 &&
                    static_cast<size_t>(car->record) < race.career->session.racers.size()) {
                    name = race.career->session.racers[static_cast<size_t>(car->record)].name;
                }
                set(m_name[i], name);
                if (!race.networked && race.mode != RaceMode::SplitScreen && car != nullptr) {
                    set(m_team[i], m_texts.at(kTextCrewNames + static_cast<size_t>(car->crew)));
                } else if (m_team[i] >= 0) {
                    s.clearText(m_team[i]);
                }
                set(m_raceTime[i], widen(hudTime(c->raceTime)));
                if (race.networked) {
                    if (m_car[i] >= 0) s.clearText(m_car[i]);
                } else if (car != nullptr && car->carRecord != nullptr && car->carRecord->def != nullptr) {
                    set(m_car[i], m_texts.at(kTextModels + static_cast<size_t>(car->carRecord->def->id)));
                }
                if (c == own) {
                    for (const int e : {m_name[i], m_team[i], m_car[i], m_raceTime[i]}) {
                        if (e >= 0) s.setHighlighted(e, true);
                    }
                }
                ++m_filled;
            }
        } else {
            set(m_name[i], L"----------");
            // Stock problem: Team takes the time's dashes. The fixed build gives it the name's.
            set(m_team[i], L"----------");
            set(m_raceTime[i], L"--:--:--");
            set(m_car[i], L"----------");
        }
        if (i >= race.cars.size()) {
            for (const int e : {m_pos[i], m_name[i], m_team[i], m_car[i], m_raceTime[i], m_bar[i]}) {
                if (e >= 0) s.setHidden(e, true);
            }
        }
    }
}

// 0x00452030, the network's table: the controller let go; each of the six rows: Team hidden; a row of the session's
// order of the players by distance: a player finished gives its name, its race time (the finish's first value, a
// float's bits; 0x0044FBD0) and its race car's model (line 211 + its id), ours highlighted; one not finished
// "--------", "--:--:--", "--------"; each such row counted. Rows past the order have Pos, Name, Time and PosBar hidden
// (Stock problem: Car stays shown).
void RaceResults::fillNet(const HudRace& race) {
    ui::Screen& s = *m_screen;
    m_filled = 0;
    const std::vector<uint32_t>& order = race.net->order();
    const std::vector<net::NetPlayer>& players = race.net->players();
    const auto set = [&](int e, const std::wstring& text) {
        if (e >= 0) s.setText(e, text);
    };
    for (size_t i = 0; i < 6; ++i) {
        if (m_team[i] >= 0) s.setHidden(m_team[i], true);
        if (i < order.size()) {
            const net::NetPlayer* p = order[i] < players.size() ? &players[order[i]] : nullptr;
            if (p != nullptr && p->race.finished != 0) {
                set(m_name[i], std::wstring(p->name.begin(), p->name.end()));
                set(m_raceTime[i], widen(hudTime(std::bit_cast<float>(p->race.finishTime))));
                const auto* car = static_cast<const RaceCar*>(p->raceCar);
                if (car != nullptr && car->carRecord != nullptr && car->carRecord->def != nullptr) {
                    set(m_car[i], m_texts.at(kTextModels + static_cast<size_t>(car->carRecord->def->id)));
                }
                if (p->peer == -1) {
                    for (const int e : {m_name[i], m_raceTime[i], m_car[i]}) {
                        if (e >= 0) s.setHighlighted(e, true);
                    }
                }
            } else {
                set(m_name[i], L"--------");
                set(m_raceTime[i], L"--:--:--");
                set(m_car[i], L"--------");
            }
            ++m_filled;
        } else {
            for (const int e : {m_pos[i], m_name[i], m_raceTime[i], m_bar[i]}) {
                if (e >= 0) s.setHidden(e, true);
            }
        }
    }
}

// 0x00452330, an online career session of teams. +0xB8 0; the machines (the players' peers) of the players in the
// race, three at most in their order; the controller let go (0x00407FB0). Stock problem: each player in the race is
// looked at as the n-th player of the list, n counting those in the race before it (the same while every player
// races); so too below. The race's finishers in order, one for each player in the race (+0xB8 counting those there):
// the player of the finisher's car counts for its machine's team; the team reaching the session's team size
// ([0x00728CEC]) takes the next of the three places (the player's place, +0xD8) and, the first time this window gives
// the place (+0xBC .. +0xC4, cleared by the enter), 10, 6 or 4 points go to each of our local players (0x004E7C30 for
// the local indices 0 .. 2, +0xE0, sent 0x1B) when the team is our machine's. Each of the three rows: a place taken
// shows the team's player of index 0 on its machine: Name the competitor's name (+0x14, the player's), Car the model's
// line (211 + the id of its car's record, +0x4E4), Team emptied; Pos, Name, Team, PosBar and Car shown (Time as the
// window has it); Name, Team and Car highlighted for the race's first player's car. A place not taken hides its row;
// the rows 4 .. 6 are hidden. (A player not found is read at 0: a crash; here nothing.)
void RaceResults::fillOnlineTeams(const HudRace& race) {
    ui::Screen& s = *m_screen;
    net::NetSession& session = *race.net;
    std::vector<net::NetPlayer>& players = session.players();
    const size_t count = players.size();
    constexpr int32_t kNone = 1000;
    std::array<int32_t, 3> teams{kNone, kNone, kNone};
    std::array<int32_t, 3> places{kNone, kNone, kNone};
    std::array<int32_t, 3> finished{};
    const auto peerOf = [&](size_t n) { return n < count ? players[n].peer : 0; };
    m_filled = 0;
    size_t n = 0;
    for (size_t i = 0; i < count; ++i) {
        if (players[i].race.notRacing != 0) continue;
        for (size_t k = 0; k < 3; ++k) {
            if (teams[k] == kNone) {
                teams[k] = peerOf(n);
                break;
            }
            if (teams[k] == peerOf(n)) break;
        }
        ++n;
    }
    static const std::vector<Competitor*> kNoFinishers;
    const std::vector<Competitor*>& finishers = race.race != nullptr ? race.race->finishers : kNoFinishers;
    const int32_t teamSize = net::onlineHosted().career.team;
    size_t next = 0;
    for (size_t i = 0; i < count; ++i) {
        if (players[i].race.notRacing != 0) continue;
        const size_t at = next++;
        if (finishers.size() <= at || finishers[at] == nullptr) continue;
        const Competitor* finisher = finishers[at];
        ++m_filled;
        size_t u = 0;
        for (size_t j = 0; j < count; ++j) {
            if (players[j].race.notRacing != 0) continue;
            if ((u < count ? players[u].raceCar : nullptr) == static_cast<const void*>(finisher->car)) break;
            ++u;
        }
        for (size_t k = 0; k < 3; ++k) {
            if (teams[k] != peerOf(u)) continue;
            if (++finished[k] != teamSize) break;
            for (size_t place = 0; place < 3; ++place) {
                if (places[place] != kNone) continue;
                places[place] = peerOf(u);
                if (u < count) players[u].race.place = static_cast<int32_t>(place);
                if (m_placeGiven[place] == 0) {
                    m_placeGiven[place] = 1;
                    const int32_t points = place == 0 ? 10 : place == 1 ? 6 : 4;
                    if (u < count && players[u].peer == -1) {
                        for (int32_t local = 0; local < 3; ++local) {
                            if (const net::NetPlayer* own = session.ownPlayer(local)) {
                                session.sendPoints(own->race.points + points, local);
                            }
                        }
                    }
                }
                break;
            }
            break;
        }
    }
    const Competitor* own = !race.players.empty() && race.players[0].car != nullptr
                                ? race.players[0].car->competitor
                                : nullptr;
    const auto hideRow = [&](size_t row) {
        for (const int e : {m_pos[row], m_name[row], m_team[row], m_raceTime[row], m_bar[row], m_car[row]}) {
            if (e >= 0) s.setHidden(e, true);
        }
    };
    for (size_t row = 0; row < 3; ++row) {
        if (places[row] == kNone) {
            hideRow(row);
            continue;
        }
        size_t u = 0;
        for (size_t j = 0; j < count; ++j) {
            if (players[j].race.notRacing != 0) continue;
            if (peerOf(u) == places[row] && (count <= u || players[u].localIndex == 0)) break;
            ++u;
        }
        const auto* car = u < count ? static_cast<const RaceCar*>(players[u].raceCar) : nullptr;
        if (car == nullptr) continue;
        const Competitor* competitor = car->competitor;
        if (m_name[row] >= 0) s.setText(m_name[row], std::wstring(players[u].name.begin(), players[u].name.end()));
        if (m_car[row] >= 0 && car->carRecord != nullptr && car->carRecord->def != nullptr) {
            s.setText(m_car[row], m_texts.at(kTextModels + static_cast<size_t>(car->carRecord->def->id)));
        }
        if (m_team[row] >= 0) s.clearText(m_team[row]);
        for (const int e : {m_pos[row], m_name[row], m_team[row], m_bar[row], m_car[row]}) {
            if (e >= 0) s.setHidden(e, false);
        }
        if (own != nullptr && own == competitor) {
            for (const int e : {m_name[row], m_team[row], m_car[row]}) {
                if (e >= 0) s.setHighlighted(e, true);
            }
        }
    }
    for (size_t row = 3; row < 6; ++row) hideRow(row);
}

// 0x004529E0, a career round of crews with more than one driver: a row for each crew of the session's round, in the
// order the crews finished (the session's +0x340, -1 for none yet): Team the crew's name (line 108 + the crew),
// Time the latest race time of its drivers (Stock problem: kept from row to row, 0 only before the first: a row shows
// the latest of its crew and of the crews above it; the fixed build's its crew's alone), Name and Car emptied and Car
// hidden; the player's crew's row (by its leader's car, else its first driver's) has Team and Time highlighted; a crew
// not finished shows "----------", "--:--:--" and "----------" in Team, Time and Car; rows from the round's crews on
// are hidden.
void RaceResults::fillCrews(const HudRace& race) {
    ui::Screen& s = *m_screen;
    const Career& career = *race.career;
    const RaceSession& session = career.session;
    const int32_t crews = session.event != nullptr ? roundOf(career, *session.event)->opponents : 0;
    const Competitor* own = !race.players.empty() && race.players[0].car != nullptr
                                ? race.players[0].car->competitor
                                : nullptr;
    const auto carOf = [&](uint32_t racer) -> const RaceCar* {
        for (const RaceCar* c : race.cars) {
            if (c != nullptr && c->record == static_cast<int32_t>(racer)) return c;
        }
        return nullptr;
    };
    const auto set = [&](int e, const std::wstring& text) {
        if (e >= 0) s.setText(e, text);
    };
    // 0x00452A2C: the latest (a stack slot) 0 once; each row only raises it (0x00452A9A ..). The fixed build starts
    // it at 0 for each row.
    for (int32_t i = 0; i < crews && i < 6; ++i) {
        float latest = 0.0f;
        const auto k = static_cast<size_t>(i);
        const int32_t crew = static_cast<size_t>(i) < session.order.size() ? session.order[k] : -1;
        if (crew == -1) {
            set(m_team[k], L"----------");
            set(m_raceTime[k], L"--:--:--");
            set(m_car[k], L"----------");
            continue;
        }
        for (uint32_t r = 0; r < session.count && r < session.racers.size(); ++r) {
            if (session.racers[r].crew != crew) continue;
            const RaceCar* car = carOf(r);
            if (car != nullptr && car->competitor != nullptr && car->competitor->raceTime > latest) {
                latest = car->competitor->raceTime;
            }
        }
        uint32_t lead = session.count;
        for (uint32_t r = 0; r < session.count && r < session.racers.size(); ++r) {
            if (session.racers[r].crew == crew && session.racers[r].member == 0) {
                lead = r;
                break;
            }
        }
        if (lead == session.count) {
            for (uint32_t r = 0; r < session.count && r < session.racers.size(); ++r) {
                if (session.racers[r].crew == crew) {
                    lead = r;
                    break;
                }
            }
        }
        const RaceCar* car = lead < session.count ? carOf(lead) : nullptr;
        if (m_name[k] >= 0) s.clearText(m_name[k]);
        if (car != nullptr) set(m_team[k], m_texts.at(kTextCrewNames + static_cast<size_t>(car->crew)));
        if (m_car[k] >= 0) s.clearText(m_car[k]);
        set(m_raceTime[k], widen(hudTime(latest)));
        for (const int e : {m_pos[k], m_name[k], m_team[k], m_raceTime[k], m_bar[k]}) {
            if (e >= 0) s.setHidden(e, false);
        }
        if (m_car[k] >= 0) s.setHidden(m_car[k], true);
        if (car != nullptr && own != nullptr && car->competitor == own) {
            for (const int e : {m_team[k], m_raceTime[k]}) {
                if (e >= 0) s.setHighlighted(e, true);
            }
        }
    }
    for (size_t k = static_cast<size_t>(std::max(crews, 0)); k < 6; ++k) {
        for (const int e : {m_pos[k], m_name[k], m_team[k], m_raceTime[k], m_bar[k], m_car[k]}) {
            if (e >= 0) s.setHidden(e, true);
        }
    }
}

// 0x00453450: the controller let go each 4 s; the first frame looks at the lap's records (lookAtRecords) and ends
// there. A quit (+0xA8) with
// the answer (+0xB4) 1 asks for closedown (the replay's recording stopped; on the network a client leaving the session,
// the host taking everyone back to the lobby) and has the records looked at
// again; a restart (+0xAC) with it asks for restart_race; both flags and the answer cleared. The table filled again
// when more have finished than it shows or each second ([0x007A2EB0], counted by the game's frame time). SELECT
// (CLICK): QUIT quits, asking "QUIT RACE AND FINISH LAST?" / "ALL GAME PROGRESS WILL BE LOST." / "ARE YOU SURE?" in a
// career's team round (0x004BAEB0) with the race cars chosen while no crew of the round's order (+0x340) is the
// player's, else at once; REPLAY asks for start_replay; RESTART asks "RESTART RACE?" / "ARE YOU SURE?".
RaceWindowFrame RaceResults::frame(const HudRace& race, float dt, uint32_t uiEvent) {
    RaceWindowFrame out;
    if (m_screen == nullptr) return out;
    ui::Screen& s = *m_screen;
    if (uiEvent != input::kNone) {
        s.finishTransitions();
        s.menuEvent(uiEvent);
    }
    m_time = m_time - dt;
    if (m_time < 0.0f) {
        out.useUi = true;
        m_time = 4.0f;
    }
    if (m_started == 0) {
        out.highScores = lookAtRecords(race, true);
        m_started = 1;
        return out;
    }
    if (m_quit != 0) {
        if (m_answer == 1) {
            m_started = 0;
            out.window = "closedown";
            out.replayOff = true;  // 0x00453732
            // On the network a client leaves the session, the host takes everyone back to the lobby.
            if (race.networked) out.netEnd = race.host ? 2 : 1;
        }
        m_quit = 0;
        m_answer = 0;
    }
    if (m_restart != 0) {
        if (m_answer == 1) {
            out.window = "restart_race";
            out.stopRecord = true;  // 0x004537BB
        }
        m_restart = 0;
        m_answer = 0;
    }
    m_refresh = m_refresh - dt;
    const auto finishers = race.race != nullptr ? static_cast<int32_t>(race.race->finishers.size()) : 0;
    if (m_filled < finishers || m_refresh < 0.0f) {
        m_refresh = 1.0f;
        fill(race);
    }
    if (uiEvent != input::kSelect) return out;
    if (m_screen->sound) m_screen->sound("CLICK");  // 0x00453876
    const int menu = s.find("Menu");
    if (menu < 0) return out;
    const std::string& m = s.nameOf(menu);
    const std::string command = s.menuItemCommand(m, s.selectedMenuItem(m));
    const auto line = [&](size_t index) -> const std::wstring& { return m_texts.at(index); };
    if (_stricmp(command.c_str(), "quit") == 0) {
        m_quit = 1;
        m_answer = 1;
        if (race.networked) {
            // 0x004538AB: "End Race" (the host) or "Leave game session?" (a client) and "ARE YOU SURE?".
            m_answer = 0;
            out.question = line(race.host ? kTextEndRace : kTextLeaveSession) + L"\n" + line(kTextAreYouSure);
            out.answer = &m_answer;
        } else if (race.mode == RaceMode::Career && race.career != nullptr && race.raceCarsChosen) {
            const RaceSession& session = race.career->session;
            const bool team = race.type != RaceType::TestTrack && session.round != nullptr &&
                              static_cast<int8_t>(session.round->teamSize) > 1;
            if (team) {
                const int32_t crews = session.event != nullptr ? roundOf(*race.career, *session.event)->opponents : 0;
                bool crewDone = false;
                for (int32_t i = 0; i < crews; ++i) {
                    const auto k = static_cast<size_t>(i);
                    if (k < session.order.size() && session.order[k] == 0) {
                        crewDone = true;
                        break;
                    }
                }
                if (!crewDone) {
                    m_answer = 0;
                    out.question =
                        line(kTextQuitFinishLast) + L"\n" + line(kTextProgressLost) + L"\n" + line(kTextAreYouSure);
                    out.answer = &m_answer;
                }
            }
        }
    } else if (_stricmp(command.c_str(), "replay") == 0) {
        out.window = "start_replay";
        out.stopRecord = true;  // 0x00453A7E
    } else if (_stricmp(command.c_str(), "restart") == 0) {
        m_restart = 1;
        out.question = line(kTextRestartRace) + L"\n" + line(kTextAreYouSure);
        out.answer = &m_answer;
    }
    return out;
}

void RaceResults::draw(float seconds, float dt) {
    if (m_screen == nullptr) return;
    m_screen->update(dt);
    m_screen->draw(seconds, dt);
}

}  // namespace oj
