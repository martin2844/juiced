// CMSGridWalk: the grid before a race and the bets on it (CMSBet over it).

#include "Game/Frontend/Career/GridWalk.h"

#include "Core/Base/Log.h"
#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cmath>

namespace oj::frontend {

namespace {

// The grid walk's globals.
int32_t g_loaderClosed = 0;    // [0x00728C68]: the loader over the window taken away
float g_betBlink = 0.0f;       // [0x0079E858]
float g_selectPulse2 = 1.0f;   // [0x00725610], speed 4 ([0x00725614])
bool g_goPlaceKept = false;    // [0x007A2EDC] bit 0
float g_goPlace = 0.0f;        // [0x007A2ED8]: go's x as the window file has it

// 0x004B9CC0: a crew of 1 .. 8 with money on its odds (+0x0C over 0).
bool oddsTaken(const RaceSession& session) {
    for (size_t c = 1; c < 9; ++c) {
        if (session.odds[c].staked > 0) return true;
    }
    return false;
}

// 0x004B1810: a bet with a crew: the bets' crew (+0x08); a pink slip race kind 1 at step 13, else step 3 and kind 2
// when asked, else kind 0 with +0x80 set; the bets on the crew (0x004B1870), its +0x5C set and no amount.
void startBet(Career& career, int crew, bool asked) {
    CareerBets& b = career.bets;
    b.crew = crew;
    if (b.pinkSlip != 0) {
        b.kind = BetKind::PinkSlip;
        b.firstStage = 13;
    } else {
        b.firstStage = 3;
        if (asked) {
            b.kind = BetKind::AmountAsked;
        } else {
            b.rivalAsked = 1;
            b.kind = BetKind::Money;
        }
    }
    setupBets(career, crew);
    b.talkedTo[static_cast<size_t>(crew)] = 1;
    b.amount = 0.0f;
}

// The amount's repeat ([0x007A2EA4] the time left, [0x007A2EA0] the delay).
float g_betRepeat = 0.0f;
float g_betDelay = 0.0f;

// The amount's step by its size: 1, 10, 100, 1000, from 25000 on 5000; `up` counts the step's own size into the lower
// one ("<"), down into the higher one ("<=").
int32_t betStep(int32_t amount, bool up) {
    int32_t step = 1;
    if (up ? amount >= 10 : amount > 10) step = 10;
    if (up ? amount >= 100 : amount > 100) step = 100;
    if (up ? amount >= 1000 : amount > 1000) step = 1000;
    if (up ? amount >= 25000 : amount > 25000) step = 5000;
    return step;
}

}  // namespace

// 0x004C9DC0: the racers' lines: the blinking list emptied (+0x58); for each place %dInfo2 the crew's odds ("%d:%d",
// with "($<money on them>)" when there is) for a crew's leader when betting on cars (the profile's +0x61C, not +0x614),
// else emptied; the leader of the crew bet with shows the bet. bet<n> hidden, then shown and blinking for a leader of
// another crew the player may still bet with when betting with drivers (the profile's +0x614, no +0x02 or +0x03, no
// bet asked, no amount; its bets made, 0x004B1870, with their middle amount under the money), and for a leader with no
// money on its odds when betting on cars.
void GridWalk::lines(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    Career& career = *data->career;
    const CareerProfile& p = career.profile;
    const RaceSession& session = career.session;
    const CalendarEvent* e = currentEvent(career.calendar);
    m_blinkCount = 0;
    for (uint32_t i = 0; i < 6; ++i) {
        const Racer& r = session.racers[i];
        const int info = s.find(std::format("{}Info2", i + 1));
        if (info >= 0) {
            if (r.member == 0 && i < session.count && p.watching != 0 && p.raceCarsChosen == 0) {
                std::string text = oddsText(session, r.crew);
                const int32_t money = session.odds[static_cast<size_t>(r.crew)].staked;
                if (money > 0) text = std::format("{} (${})", text, groupThousands(money, host.language().index));
                s.setText(info, widenSigned(text));
            } else {
                s.clearText(info);
            }
            if (r.member == 0 && i < session.count && career.bets.placed != 0 && r.crew == career.bets.crew) {
                s.setText(info, toWide(moneyText(x87::chop(career.bets.amount), host.language().index)));
            }
        }
        const int bet = s.find(std::format("bet{}", i + 1));
        if (bet < 0) continue;
        s.setHidden(bet, true);
        if (p.raceCarsChosen != 0 && e != nullptr && e->hostedByPlayer == 0 && e->pinkSlip == 0 &&
            career.bets.rivalAsked == 0 && r.member == 0 && r.crew != 0 &&
            career.bets.talkedTo[static_cast<size_t>(r.crew)] == 0 && x87::chop(career.bets.amount) == 0) {
            setupBets(career, r.crew);
            if (career.bets.amounts[3].amount < p.money && m_blinkCount < 6) {
                s.setHidden(bet, false);
                m_blink[static_cast<size_t>(m_blinkCount++)] = bet;
            }
        }
        if (p.watching != 0 && r.member == 0 && session.odds[static_cast<size_t>(r.crew)].staked == 0 &&
            m_blinkCount < 6) {
            s.setHidden(bet, false);
            m_blink[static_cast<size_t>(m_blinkCount++)] = bet;
        }
    }
}

// 0x004C7FA0: [0x00728C68], +0x58 and +0x60 0; the racers of the current round (0x004B8570); the cars' list (+0x24,
// 0x00489B80); back_text "LEAVE EVENT". With one racer the race starts at once (0x004A3CC0, the bet's share
// 0x004BC2E0). RaceInfo the round's type (140 + type), for a circuit "Laps" and its laps, and the team size's line (491
// + size). For places 1 .. 6: %dLogo the crew's logo (hidden past the racers); ps2_car%d hidden; %dInfo the member's
// short name and the car's display name; %dInfo2 the odds (as 0x004C9DC0); the car in Car3d%d (a view per racer, +0x0C
// .. +0x20, 0x00434D90, 0x0045A080); for a crewmate or another crew's member Recovery%d, Ability%dt, Recover%d,
// Ability%d, BOX0%d and %d shown, the meters Recover%d (recover or concentration x 0.01) and Ability%d (ability x
// 0.01); the player hides Recover%d, Ability%d, Recovery%d and Ability%dt; past the racers all six hidden; bph%d the
// car's rating and "bhp". Cash and Date; the button go highlighted; CMSLoader over the window (mode 8); the lines
// (0x004C9DC0); SELECT1 .. 6 (+0x28 ..) hidden; bet "BET WITH DRIVER" or "BET ON CAR"; Go and Bet shown; with nothing
// to bet on go at x 0.3125, bet hidden and go highlighted, else bet highlighted and go at its place; +0x64 0.
void GridWalk::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    const CareerProfile& p = career.profile;
    RaceSession& session = career.session;
    CalendarEvent* e = currentEvent(career.calendar);
    if (e == nullptr) return;
    const EventRound& round = *roundOf(career, *e);
    g_loaderClosed = 0;
    for (auto& view : m_views) view.reset();
    m_cars.clear();
    m_cars.reserve(6);
    m_blinkCount = 0;
    m_quitAnswer = 0;
    data->setupRace(gameRandom());
    s.setText("back_text", texts.at(kTextLeaveEventItem));
    if (session.count == 1) {
        data->startRace(gameRandom(), static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic)));
        host.request("loading", SwitchMode::Replace);
        updateBetShare(career);
        return;
    }
    {
        std::wstring info =
            texts.at(kTextRoundTypes + static_cast<size_t>(std::max(static_cast<int32_t>(round.type), 0))) + L"   ";
        if (round.type == EventKind::Circuit) {
            info += texts.at(kTextLaps) + toWide(std::format(" {}", static_cast<int>(round.laps))) + L"   ";
        }
        info += texts.at(kTextTeamSizes + static_cast<size_t>(std::max<int>(round.teamSize, 0)));
        s.setText("RaceInfo", info);
        if (const int ticker = s.find("RaceInfo"); ticker >= 0) s.measureTicker(ticker);
    }
    for (uint32_t i = 0; i < 6; ++i) {
        const std::string logo = std::format("{}Logo", i + 1);
        if (i >= session.count) {
            hideAll(s, logo.c_str());
        } else {
            s.show(logo, false);
            const auto crew = static_cast<size_t>(session.racers[i].crew);
            if (crew < career.flyers.crews.size()) s.setTexture(logo, career.flyers.crews[crew].logo);
        }
    }
    for (uint32_t i = 0; i < 6; ++i) {
        const Racer& r = session.racers[i];
        const auto n = i + 1;
        s.setHidden(std::format("ps2_car{}", n), true);
        const std::vector<Car>& cars = r.crew == 0 ? career.garage : career.opponentCars;
        const Car* car = i < session.count && static_cast<size_t>(r.car) < cars.size()
                             ? &cars[static_cast<size_t>(r.car)] : nullptr;
        if (const int el = s.find(std::format("{}Info", n)); el >= 0) {
            s.setMultiline(std::format("{}Info", n), true);
            if (i >= session.count) {
                s.clearText(el);
            } else {
                const Crew& crew = career.crews[static_cast<size_t>(r.crew)];
                const auto& name = crew.memberShortNames[static_cast<size_t>(r.member) % 4];
                std::wstring text = widenSigned(std::string_view(name.data(), name.size())) + L" - ";
                if (car != nullptr && car->def != nullptr) text += widenSigned(car->def->displayName);
                s.setText(el, text);
            }
        }
        // The racer's car in Car3d<n>: the crew's scheme on its cars (0x004A9080, not for crew 0), a copy of the car
        // in the cars' list (0x00434D90), a car view (made in place as 0x0045A2C0: the zoom 0.9) showing it
        // (0x0045A080: 2.35575, 0.5, fading) with the camera at (0, 110, -560).
        m_views[i].reset();
        if (const std::string element = std::format("Car3d{}", n); s.find(element) >= 0 && i < session.count) {
            if (r.crew != 0 && static_cast<size_t>(r.crew) < career.crews.size()) {
                applyCrewScheme(career, career.crews[static_cast<size_t>(r.crew)], data->defs, data->catalogue);
            }
            const std::vector<Car>& list = r.crew == 0 ? career.garage : career.opponentCars;
            if (static_cast<size_t>(r.car) < list.size() && host.engine() != nullptr) {
                m_cars.push_back(list[static_cast<size_t>(r.car)]);
                scene::CarView::Setup setup;
                setup.element = element;
                setup.zoom = 0.9f;
                m_views[i] = std::make_unique<scene::CarView>(*host.engine(), s, setup, data->catalogue, data->tables);
                m_views[i]->show(m_cars.back(), nullptr, static_cast<uint32_t>(m_cars.size() - 1),
                                 std::bit_cast<float>(0x4016C49Cu), 0.5f, true, host.viewLoading());
                m_views[i]->setCameraPlace({0.0f, 110.0f, -560.0f, 0.0f});
            }
        }
        const std::string recovery = std::format("Recovery{}", n);
        const std::string abilityT = std::format("Ability{}t", n);
        const std::string recover = std::format("Recover{}", n);
        const std::string ability = std::format("Ability{}", n);
        const std::string box = std::format("BOX0{}", n);
        const std::string number = std::format("{}", n);
        if (i < session.count) {
            const Crew& crew = career.crews[static_cast<size_t>(r.crew)];
            if (crew.id == 0 && r.member == 0) {
                for (const std::string& name : {recover, ability, recovery, abilityT}) s.setHidden(name, true);
            } else {
                for (const std::string& name : {recover, recovery, ability, abilityT, box, number}) {
                    s.setHidden(name, false);
                }
                if (crew.id == 0) {
                    // The crewmate of the member slot (the profile's +0x500).
                    const auto slot = static_cast<size_t>(r.member);
                    const auto mate = slot < 4 ? static_cast<size_t>(career.members[slot]) : career.crewmates.size();
                    if (mate < career.crewmates.size()) {
                        s.setMeter(recover, career.crewmates[mate].recover * 0.01f);
                        s.setMeter(ability, career.crewmates[mate].drivingAbility * 0.01f);
                    }
                } else {
                    const CrewMemberDef& m = crew.members[static_cast<size_t>(r.member) % 4];
                    s.setMeter(recover, m.concentration * 0.01f);
                    s.setMeter(ability, m.ability * 0.01f);
                }
            }
        } else {
            for (const std::string& name : {recover, ability, recovery, abilityT, box, number}) {
                s.setHidden(name, true);
            }
        }
        if (const int bph = s.find(std::format("bph{}", n)); bph >= 0) {
            if (i < session.count && car != nullptr) {
                s.setHidden(bph, false);
                s.setText(bph, toWide(groupThousands(x87::chop(std::bit_cast<float>(car->rating)),
                                                     host.language().index)) + texts.at(kTextBhp));
            } else {
                s.setHidden(bph, true);
            }
        }
    }
    careerCashTexts(s, career, host.language().index, texts);
    s.setCurrentButton("go");
    host.request("CMSLoader", SwitchMode::Overlay);
    lines(host, s);
    for (size_t i = 0; i < 6; ++i) {
        m_select[i] = s.find(std::format("SELECT{}", i + 1));
        if (m_select[i] >= 0) s.setHidden(m_select[i], true);
    }
    s.setText("bet", texts.at(p.raceCarsChosen != 0 ? kTextBetWithDriver : kTextBetOnCar));
    s.show("Go", false);
    s.show("Bet", false);
    const int go = s.find("go");
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    if (go >= 0) s.placeOf(go, x, y, w, h);
    if (!g_goPlaceKept) {
        g_goPlaceKept = true;
        g_goPlace = x;
    }
    if (m_blinkCount == 0) {
        if (go >= 0) s.setPlace(go, 0.3125f, y, w, h);
        s.setHidden("bet", true);
        s.setCurrentButton("go");
    } else {
        s.setCurrentButton("bet");
        if (go >= 0) s.setPlace(go, g_goPlace, y, w, h);
    }
    m_starting = 0;
}

// 0x004C91B0: with +0x64 the race starts (0x004A3CC0). The bet blinking (+0x40 .., +0x58): hidden over 0.75 of each
// second. The racers' views turned (0x00459EE0), each looked at before its frame: while one is loading nothing more
// (a load ended by that frame counts the next); then once the loader over the window goes ([0x00728C68]). Cash and Date. Quitting answered "yes" (+0x60): in the first event the main menu; else
// the profile's +0x630 set, the racers again (0x004B8570), the session's end (0x004BA8A0, 0x004B9E20), the round and
// the event raced (+0x01), the bets cleared, CLICK and CMSCalendar. BACK on Go or Bet (CLICK): the first event "QUIT
// CAREER / ARE YOU SURE?", else "QUIT RACE AND FINISH LAST?" when betting with drivers, "QUIT RACE AND LOSE BET?" with
// money on the odds when betting on cars, else "Are you sure you want to leave the race event? / The event will be
// closed .." (into +0x60); BACK elsewhere: Go and Bet shown, back_text "LEAVE EVENT", CLICK and the button Bet. PGDN:
// CLICK and the help (0x004850E0). SELECT (CLICK): Bet hides Go and Bet, back_text "BACK" and highlights the button 1.
// Go: a pink slip race when betting with drivers takes the first other crew racing (the bets' pink slip, placed,
// kind 1, step 13, its bets 0x004B1870, its +0x5C, no amount), careerload and the betting window (the career's +0xA4
// 2). Then betting with drivers, no bet placed or asked, rand() / 32767 under 0.333 and no +0x02: up to ten tries of a
// racer at random (rand() % racers) whose crew is not 0, not yet bet with, past its second step and with twice its
// money under the player's: a bet asked by that crew (0x004B1810 with 0), careerload and the betting window; none:
// the race starts with the bet's share. Otherwise the bet's share (0x004BC2E0) and with a share over 0 +0x64 set and
// the autosave over the window when saving, else the race starts. A racer's button (not in the first event): betting
// on cars on a leader with no money on its odds CMSBet over the window ([0x007221BC] the place), else "No"; betting
// with drivers, no +0x02 or +0x03, no bet asked, the leader of another crew not yet bet with and no amount: a bet with
// it (0x004B1810 with 1) and, its middle amount under the money, careerload and the betting window. The SELECT<n> of
// the highlighted racer's button shown with the pulse |p| (p - 4 a second, from -1 back to 1); +0x5C asks for the
// lines again (0x004C9DC0).
void GridWalk::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    RaceSession& session = career.session;
    CalendarEvent* e = currentEvent(career.calendar);
    if (e == nullptr) return;
    if (m_starting != 0) {
        data->startRace(gameRandom(), static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic)));
        host.request("loading", SwitchMode::Replace);
        return;
    }
    g_betBlink = g_betBlink + host.gameFrameTime();
    if (g_betBlink > 1.0f) g_betBlink = g_betBlink - 1.0f;
    for (int32_t i = 0; i < m_blinkCount; ++i) s.setHidden(m_blink[static_cast<size_t>(i)], g_betBlink > 0.75f);
    bool loaded = true;
    for (auto& view : m_views) {
        if (!view) continue;
        // Its load (+0x3C) and parts (+0x40) looked at before its frame, which may end them.
        if (view->busy()) loaded = false;
        view->frame(host.frameTime(), gameRandom());
    }
    if (!loaded) return;
    if (g_loaderClosed == 0) {
        host.closeOverlay();
        g_loaderClosed = 1;
    }
    careerCashTexts(s, career, host.language().index, texts);
    if (m_quitAnswer == 1) {
        if (p.introduction != 0) {
            host.request("menu", SwitchMode::Replace);
            return;
        }
        p.outOfRace = 1;
        data->setupRace(gameRandom());
        data->endSession(gameRandom());
        auto& round = const_cast<EventRound&>(*roundOf(career, *e));
        round.ended = 1;
        e->done = 1;
        clearBets(career.bets);
        host.sound("CLICK");
        host.request("CMSCalendar", SwitchMode::Replace);
        return;
    }
    const std::string button = s.currentButton();
    const auto named = [&](const char* name) { return _stricmp(button.c_str(), name) == 0; };
    if (event == input::kBack) {
        if (button.empty() || (!named("Bet") && !named("Go"))) {
            s.show("Go", false);
            s.show("Bet", false);
            s.setText("back_text", texts.at(kTextLeaveEventItem));
            host.sound("CLICK");
            s.setCurrentButton("Bet");
            return;
        }
        if (p.introduction == 0) {
            host.sound("CLICK");
            std::wstring text;
            if (p.raceCarsChosen != 0) {
                text = texts.at(kTextQuitFinishLast);
            } else if (p.watching != 0 && oddsTaken(session)) {
                text = texts.at(kTextQuitLoseBet);
            } else {
                text = texts.at(kTextLeaveEvent) + L"\n" + texts.at(kTextEventClosed);
            }
            host.message(MessageType::YesNo, text, false, &m_quitAnswer, true);
        }
        if (p.introduction != 0) {
            host.message(MessageType::YesNo, texts.at(kTextQuitCareer) + L"\n" + texts.at(kTextAreYouSure), false,
                         &m_quitAnswer, true);
        }
    } else if (event == input::kSelect2) {
        host.sound("CLICK");
        host.helpPage("CMSGridWalk", true);
    } else if (event == input::kSelect) {
        host.sound("CLICK");
        if (named("Bet")) {
            hideAll(s, "Go");
            hideAll(s, "Bet");
            s.setText("back_text", texts.at(kTextBack));
            s.setCurrentButton("1");
            return;
        }
        if (named("Go")) {
            CareerBets& bets = career.bets;
            if (p.raceCarsChosen != 0 && e->pinkSlip != 0) {
                int crew = 0;
                for (uint32_t i = 0; i < session.count && i < 6; ++i) {
                    crew = session.racers[i].crew;
                    if (crew != 0) break;
                }
                bets.pinkSlip = 1;
                bets.placed = 1;
                bets.crew = crew;
                bets.kind = BetKind::PinkSlip;
                bets.firstStage = 13;
                setupBets(career, crew);
                bets.talkedTo[static_cast<size_t>(crew)] = 1;
                bets.amount = 0.0f;
                host.request("careerload", SwitchMode::Replace);
                data->careerScreen = CareerScreen::Character;
            }
            if (p.raceCarsChosen != 0 && bets.placed == 0 && bets.rivalAsked == 0 &&
                x87::unit(static_cast<int>(gameRandom().next())) < 0.333f && e->hostedByPlayer == 0) {
                for (int tries = 0; tries < 10; ++tries) {
                    const uint32_t at = static_cast<uint32_t>(gameRandom().next()) % session.count;
                    const int crew = session.racers[at].crew;
                    if (crew == 0 || bets.talkedTo[static_cast<size_t>(crew)] != 0) continue;
                    const Crew& c = career.crews[static_cast<size_t>(crew)];
                    if (c.stage[1] == 0) continue;
                    if (!(c.money + c.money < static_cast<float>(p.money))) continue;
                    startBet(career, crew, false);
                    host.request("careerload", SwitchMode::Replace);
                    data->careerScreen = CareerScreen::Character;
                    selectMarkers(host, s);
                    return;
                }
                data->startRace(gameRandom(), static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic)));
                host.request("loading", SwitchMode::Replace);
                updateBetShare(career);
            } else {
                updateBetShare(career);
                if (p.betShare > 0.0f) {
                    m_starting = 1;
                    if (data->saving != 0) host.request("autosave", SwitchMode::Push);
                } else {
                    data->startRace(gameRandom(), static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic)));
                    host.request("loading", SwitchMode::Replace);
                }
            }
        } else if (p.introduction == 0) {
            // Stock problem: a button whose name is not a place reads the racer before the first; here the first.
            const int at = std::atoi(button.c_str()) - 1;
            if (p.watching != 0) {
                m_betRacer = at;
                const Racer& r = session.racers[static_cast<size_t>(std::clamp(at, 0, 5))];
                if (r.member == 0 && session.odds[static_cast<size_t>(r.crew)].staked == 0) {
                    host.request("CMSBet", SwitchMode::Push);
                } else {
                    host.sound("No");
                }
            }
            if (p.raceCarsChosen != 0 && e->hostedByPlayer == 0 && e->pinkSlip == 0 && career.bets.rivalAsked == 0) {
                const Racer& r = session.racers[static_cast<size_t>(std::clamp(at, 0, 5))];
                const int crew = r.crew;
                if (r.member == 0 && crew != 0 && career.bets.talkedTo[static_cast<size_t>(crew)] == 0 &&
                    x87::chop(career.bets.amount) == 0) {
                    startBet(career, crew, true);
                    if (career.bets.amounts[3].amount < p.money) {
                        host.request("careerload", SwitchMode::Replace);
                        data->careerScreen = CareerScreen::Character;
                    }
                }
            }
        }
    }
    selectMarkers(host, s);
}

// The SELECT<n> of the highlighted racer's button (its name's number) shown with the pulse, the others hidden; the
// pulse; the lines again when asked (+0x5C).
void GridWalk::selectMarkers(CareerHost& host, ui::Screen& s) {
    if (!s.currentButton().empty()) {
        const int at = std::atoi(s.currentButton().c_str()) - 1;
        for (int i = 0; i < 6; ++i) {
            const int el = s.find(std::format("SELECT{}", i + 1));
            m_select[static_cast<size_t>(i)] = el;
            if (el < 0) continue;
            if (at == i) {
                s.setHidden(el, false);
                s.setAlpha(el, std::fabs(g_selectPulse2));
            } else {
                s.setHidden(el, true);
            }
        }
    }
    g_selectPulse2 = g_selectPulse2 - 4.0f * host.gameFrameTime();
    if (g_selectPulse2 < -1.0f) g_selectPulse2 = 1.0f;
    if (m_refresh != 0) {
        m_refresh = 0;
        lines(host, s);
    }
}

// 0x004C9130: the state keeps a Menu element's highlighted item; the racers' views and the cars' list freed.
void GridWalk::leave() {
    for (auto& view : m_views) {
        if (view) view->clear();
        view.reset();
    }
    m_cars.clear();
}

// 0x004CA5F0: the racer's crew ([0x007221BC] the place): odds "ODDS" and its odds (0x004B9C50); cash "CASH" and the
// money; amount "BET" and the amount; winnings (when the window has it) "POTENTIAL WIN" and $ the amount / the stake
// x against, cut, thousands marked.
void GridWalk::betTexts(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    Career& career = *data->career;
    const RaceSession& session = career.session;
    const int crew = session.racers[static_cast<size_t>(std::clamp(m_betRacer, 0, 5))].crew;
    s.setText("odds", texts.at(kTextOdds) + widenSigned(std::format(" {}", oddsText(session, crew))));
    s.setText("cash", texts.at(kTextCash) + L" " +
                          toWide(moneyText(career.profile.money, host.language().index)));
    s.setText("amount", texts.at(kTextBetWord) + L" " + toWide(moneyText(m_bet.amount, host.language().index)));
    const CrewOdds& o = session.odds[static_cast<size_t>(crew)];
    if (s.find("winnings") >= 0) {
        const int32_t win = x87::chop(static_cast<float>(m_bet.amount) / o.stake * o.against);
        s.setText("winnings", texts.at(kTextPotentialWin) + L" " +
                                  toWide(std::format("${}", groupThousands(win, host.language().index))));
    }
}

// 0x004CA1A0: the amount (+0x10) the money already on the racer's crew's odds (+0x0C), the money left (+0x14) the
// player's, +0x18 0; the texts (0x004CA5F0).
void GridWalk::enterBet(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    const int crew = career.session.racers[static_cast<size_t>(std::clamp(m_betRacer, 0, 5))].crew;
    m_bet.amount = career.session.odds[static_cast<size_t>(crew)].staked;
    m_bet.left = static_cast<float>(career.profile.money);
    m_bet.answer = 0;
    betTexts(host, s);
}

// 0x004CA210: while RIGHT or UP is held the amount goes up and while LEFT or DOWN down, a step at a time on a repeat
// whose delay starts at 0.2 s and shortens by 0.9 each step (CLICK; up only below 500000 and with more money left
// than the step, else "NO"; down only above 0). SELECT asks "Place Bet? / ARE YOU SURE?" (into +0x18); a "yes": the
// amount on the crew's odds (+0x0C), the money paid down to what is left (0x004B8200's lines), the career's money
// counter +0x08 up by the amount, CLICK, the window off (mode 6) and the grid walk's lines again ([0x007221A4]). BACK:
// CLICK, the window off and the lines again. The texts again.
void GridWalk::updateBet(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    const uint32_t scroll = host.scrollHeld();
    const float dt = host.gameFrameTime();
    if (scroll != input::kRight && scroll != input::kUp && scroll != input::kLeft && scroll != input::kDown) {
        g_betRepeat = 0.0f;
        g_betDelay = 0.2f;
    }
    if (scroll == input::kRight || scroll == input::kUp) {
        g_betRepeat = g_betRepeat - dt;
        if (g_betRepeat < 0.0f) {
            g_betDelay = g_betDelay * 0.9f;
            g_betRepeat = g_betDelay;
            const int32_t step = betStep(m_bet.amount, true);
            if (m_bet.amount < 500000 && m_bet.left > static_cast<float>(step)) {
                host.sound("CLICK");
                m_bet.left = m_bet.left - static_cast<float>(step);
                m_bet.amount += step;
            } else {
                host.sound("NO");
            }
        }
    }
    if (scroll == input::kLeft || scroll == input::kDown) {
        g_betRepeat = g_betRepeat - dt;
        if (g_betRepeat < 0.0f) {
            g_betDelay = g_betDelay * 0.9f;
            g_betRepeat = g_betDelay;
            const int32_t step = betStep(m_bet.amount, false);
            if (m_bet.amount > 0) {
                host.sound("CLICK");
                m_bet.amount -= step;
                m_bet.left = m_bet.left + static_cast<float>(step);
            } else {
                host.sound("NO");
            }
        }
    }
    if (event == input::kSelect) {
        host.message(MessageType::YesNo, texts.at(kTextPlaceBet) + L"\n" + texts.at(kTextAreYouSure), false,
                     &m_bet.answer, true);
    }
    if (m_bet.answer == 1) {
        RaceSession& session = career.session;
        const int crew = session.racers[static_cast<size_t>(std::clamp(m_betRacer, 0, 5))].crew;
        session.odds[static_cast<size_t>(crew)].staked = m_bet.amount;
        const int32_t change = x87::chop(m_bet.left - static_cast<float>(p.money));
        if (change > 0) {
            career.stats.moneyReceived += change;
        } else {
            career.stats.moneyPaid -= change;
        }
        p.money += change;
        if (p.money < 0) p.money = 0;
        career.stats.oddsStaked += m_bet.amount;
        host.sound("CLICK");
        host.request("", SwitchMode::Pop);
        m_refresh = 1;
    }
    if (event == input::kBack) {
        host.sound("CLICK");
        host.request("", SwitchMode::Pop);
        m_refresh = 1;
    }
    betTexts(host, s);
}

}  // namespace oj::frontend
