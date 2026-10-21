// The race's HUD in the career (docs/spec/22-race-hud.md): the rivals and the driver aid, the race's news,
// the respect.

#include "Game/Race/Hud/RaceHud.h"
#include "Game/Race/Hud/HudInternal.h"

#include "Core/Career/Money.h"
#include "Core/Cars/Car.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/OnlineRecords.h"
#include "Core/Physics/CarBody.h"
#include "Core/Saves/SaveEntries.h"
#include "Render/Materials.h"
#include "Render/Model.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace oj {

using namespace hud;

namespace {

int32_t g_respectShown = 0;    // [0x0079E830]
float g_leadNewsTime = 0.0f;   // [0x0079E828]
float g_damageNewsTime = 0.0f; // [0x0079E82C]
constexpr uint32_t kNewsGood = 0xFF32FF32u;  // [0x007A2D50]
constexpr uint32_t kNewsBad = 0xFFFF3232u;   // [0x007A2D54]
float g_respectBlink = 0.0f;   // [0x0079E834]
float g_respectFlash = 0.0f;   // [0x0079E838]

}  // namespace

// 0x0044AA80 for a player: the arrows of the rivals close behind, then (with the driver aid on) the warning of a
// corner taken too fast and the corner's arrow.
void RaceHud::rivalsAndAid(const HudRace& race, Player& p, size_t index, const RaceCar& car, float dt) {
    (void)index;
    if (race.type == RaceType::Cruise) return;
    ui::Screen& s = m_screen;
    const Competitor& r = *car.competitor;
    uint32_t colour = 0xFFFFFFFF;
    if (race.mode != RaceMode::SplitScreen) {
        // A rival behind within 40 m back and 40 m aside, behind on the lap too: its arrow at the bottom, aside by
        // up to 0.4 of the screen for 6 m, in its paint, fading from 0.75 to 0 with the distance back.
        const float mine = r.distance();
        for (size_t k = 0; k < r.neighbours.size() && k < m_rivals.size(); ++k) {
            const int box = m_rivals[k];
            if (box < 0) continue;
            s.setHidden(box, true);
            const Neighbour& n = *r.neighbours[k];
            const float x = n.local[0];
            const float z = n.local[2];
            if (!(std::fabs(x) < 4000.0f) || !(-200.0f > z) || !(-4000.0f < z)) continue;
            if (n.other == nullptr || !(n.other->distance() < mine)) continue;
            s.setHidden(box, false);
            float side = std::clamp(std::fabs(x), 0.0f, 600.0f);
            side = side / 600.0f * 0.4f;
            const float centre = x > 0.0f ? side + 0.5f : 0.5f - side;
            float bx, by, bw, bh;
            s.placeOf(box, bx, by, bw, bh);
            s.setPlace(box, centre - bw * 0.5f, by, bw, bh);
            if (n.other->car != nullptr && n.other->car->carRecord != nullptr) {
                s.setColour(box, n.other->car->carRecord->base);
            }
            s.setAlpha(box, 0.8f - -(z / 4000.0f));
        }
    }
    const bool aid = race.settings != nullptr && race.settings->u32(save::Settings::kAids) != 0;
    if (!aid) {
        if (p.driverAid >= 0) s.clearText(p.driverAid);
        if (p.curveWarning >= 0) s.setHidden(p.curveWarning, true);
        return;
    }
    if (race.type != RaceType::Sprint) {
        // The corner's arrow fades out over its last half second.
        if (p.curveTimer > 0.0f) {
            p.curveTimer = p.curveTimer - dt;
            if (p.curveTimer < 0.5f) {
                p.curveAlpha = p.curveTimer;
                if (p.curveTimer < 0.0f) {
                    s.setHidden(p.curveWarning, true);
                    p.curveTimer = 0.0f;
                    p.curveAlpha = 0.0f;
                }
            }
            p.curveAlpha = std::clamp(p.curveAlpha, 0.0f, 0.5f);
            s.setAlpha(p.curveWarning, p.curveAlpha);
        }
        if (r.wrongWay) {
            if (p.driverAid >= 0) s.setTextAlpha(p.driverAid, 0.0f);
            return;
        }
    }
    if (race.type == RaceType::Sprint) return;
    if (p.driverAid >= 0) {
        if (race.type == RaceType::Speed) {
            s.clearText(p.driverAid);
        } else {
            // Above 10 m/s and faster than the next corner's speed: SLOW DOWN, or BRAKE NOW! (kept while the text
            // shows) beyond 28.5 m/s over it, flashing white every other frame; the tint reddens with the excess.
            const float speed = car.body->localVelocity[2];
            const float excess = speed - 1.0f * r.cornerSpeed;
            if (speed > 10.0f && excess > 0.0f) {
                const float e = std::clamp(excess, 0.0f, 30.0f);
                p.aidAlpha = dt * 0.7f + p.aidAlpha;
                if (0.95f * 30.0f < e) {
                    s.setText(p.driverAid, line(kTextBrakeNow));
                    p.aidBrake = true;
                } else if (!p.aidBrake) {
                    s.setText(p.driverAid, line(kTextSlowDown));
                }
                const auto red = static_cast<uint8_t>(std::nearbyint(e / 30.0f * 255.0f));
                const uint32_t fade = static_cast<uint8_t>(0xFF - red);
                colour = 0xFFFF0000u | (fade << 8) | fade;
                if (p.aidBrake) {
                    p.aidFlash = !p.aidFlash;
                    if (p.aidFlash) colour = 0xFFFFFFFF;
                }
            } else {
                p.aidAlpha = p.aidAlpha - dt * 2.5f;
            }
            p.aidAlpha = std::clamp(p.aidAlpha, 0.0f, 1.0f);
            if (p.aidAlpha == 0.0f) p.aidBrake = false;
            s.setFontColour(p.driverAid, colour);
            s.setTextAlpha(p.driverAid, p.aidAlpha);
        }
    }
    // The corner close ahead (the competitor's +0x148): its arrow, yellow, red or green, mirrored for 4 .. 6.
    const auto kind = static_cast<CornerWarning>(r.cornerWarning);
    if (kind == CornerWarning::None || p.curveWarning < 0) return;
    const char* texture = "ui_arrow01";
    uint32_t arrow = 0xFF159B02;
    bool mirrored = false;
    switch (kind) {
    case CornerWarning::Medium: texture = "ui_arrow02", arrow = 0xFFE8E003; break;
    case CornerWarning::Sharp: texture = "ui_arrow02", arrow = 0xFFB90505; break;
    case CornerWarning::GentleMirrored: mirrored = true; break;
    case CornerWarning::MediumMirrored: texture = "ui_arrow02", arrow = 0xFFE8E003, mirrored = true; break;
    case CornerWarning::SharpMirrored: texture = "ui_arrow02", arrow = 0xFFB90505, mirrored = true; break;
    default: break;
    }
    s.setTexture(p.curveWarning, texture);
    s.setColour(p.curveWarning, arrow);
    p.curveTimer = 0.5f;
    p.curveAlpha = 0.5f;
    s.setAlpha(p.curveWarning, 0.5f);
    s.setHidden(p.curveWarning, false);
    s.setUVs(p.curveWarning, mirrored ? 1.0f : 0.0f, mirrored ? 0.0f : 1.0f, 0.0f, 1.0f);
}

// 0x0044A4C0, a race of one player of the types 5 and 7 (not networked) from 5 s on: a car's finish, a new leader
// (or the player's first place) and a car spun out, as the first player's message for 2 s.
void RaceHud::raceNews(const HudRace& race, const RaceCar& car) {
    if (race.networked || (race.type != RaceType::Circuit && race.type != RaceType::PointToPoint) ||
        race.race == nullptr)
        return;
    RaceState& state = *race.race;
    if (car.competitor->raceTime < 5.0f) return;
    // A racer's short name and its crew's name.
    auto member = [&](const Competitor* c) {
        std::wstring name;
        if (race.career == nullptr || c == nullptr || c->car == nullptr) return name;
        const int32_t crew = c->car->crew;
        const int32_t record = c->car->record;
        if (crew < 0 || static_cast<size_t>(crew) >= race.career->crews.size() || record < 0 ||
            static_cast<size_t>(record) >= race.career->session.racers.size()) {
            return name;
        }
        const int32_t m = race.career->session.racers[static_cast<size_t>(record)].member;
        if (m < 0 || m > 3) return name;
        const auto& chars = race.career->crews[static_cast<size_t>(crew)].memberShortNames[static_cast<size_t>(m)];
        for (char ch : chars) {
            if (ch == 0) break;
            name.push_back(static_cast<wchar_t>(static_cast<signed char>(ch)));
        }
        return name;
    };
    auto crewName = [&](const Competitor* c) {
        return c != nullptr && c->car != nullptr ? line(kTextCrewNamesUpper + static_cast<size_t>(c->car->crew))
                                                 : line(kTextCrewNamesUpper);
    };
    std::wstring text;
    if (static_cast<int32_t>(state.finishers.size()) != state.finishersShown) {
        const Competitor* f = state.finishers.empty() ? nullptr : state.finishers.back();
        state.finishersShown = static_cast<int32_t>(state.finishers.size());
        text = member(f) + L" " + crewName(f) + L" " + line(kTextHasFinished);
    } else if (state.leaderChanged) {
        const Competitor* l = state.leader;
        if (l != nullptr && l->car == &car) {
            text = line(kTextFirstPlace);
        } else {
            text = line(kTextNewLeader) + L": " + member(l) + L" - " + crewName(l);
        }
    } else if (state.spinEvent) {
        const Competitor* sp = state.spinner;
        text = crewName(sp) + L" " + member(sp) + L": " + line(kTextWipedOut);
    } else {
        return;
    }
    m_messages[0].push(text, 2.0f);
    if (m_screen.sound) m_screen.sound("CLICK");  // 0x0044A794
}

// 0x004BCB60 at the HUD frame's start: in a career race (not of type 9) with the race cars chosen or the profile's
// +0x618, the profile's +0x00 at 0, once the first racer's race time is 10 s (5 s in a sprint): the respect so far
// (0x004BCD90) in RESPECT as "+n" in 0xFF28FF28 from 0, else "n" in 0xFFFF2828, with its icon shown; a new value
// flashes for 2 s (RESPECT and the icon hidden over the last 0.1 s of each 0.2 s), RESPECT_TEXT shown while it
// flashes at the flash's time as its alpha (at most 1). Then the respect's messages (respectNews).
void RaceHud::respect(const HudRace& race, float dt) {
    ui::Screen& s = m_screen;
    if (m_respect < 0 || race.mode != RaceMode::Career || race.type == RaceType::TestTrack || race.career == nullptr ||
        race.race == nullptr)
        return;
    Career& career = *const_cast<Career*>(race.career);
    if (career.profile.raceCarsChosen == 0 && career.profile.racing == 0) return;
    const std::vector<const RaceCar*> cars = sessionCars(race, career);
    if (career.profile.introduction != 0) {
        respectNews(career, race, cars, dt);
        return;
    }
    if (!cars.empty() && cars[0] != nullptr && cars[0]->competitor != nullptr) {
        const float time = cars[0]->competitor->raceTime;
        if (time < (race.type == RaceType::Sprint ? 5.0f : 10.0f)) return;
    }
    const auto shown = static_cast<int32_t>(raceRespectOf(race, false, false));
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), shown > 0 ? "+%d" : "%d", shown);
    s.setText(m_respect, widen(buffer));
    s.setHidden(m_respect, false);
    if (m_respectIcon >= 0) s.setHidden(m_respectIcon, false);
    s.setFontColour(m_respect, shown >= 0 ? 0xFF28FF28u : 0xFFFF2828u);
    bool flashing = g_respectFlash > 0.0f;
    if (shown != g_respectShown) {
        g_respectShown = shown;
        g_respectFlash = 2.0f;
        flashing = true;
    }
    if (flashing) {
        g_respectFlash = g_respectFlash - dt;
        g_respectBlink = g_respectBlink + dt;
        if (g_respectBlink > 0.2f) g_respectBlink = g_respectBlink - 0.2f;
        if (g_respectBlink > 0.1f) {
            s.setHidden(m_respect, true);
            if (m_respectIcon >= 0) s.setHidden(m_respectIcon, true);
        }
    }
    if (m_respectText >= 0) {
        if (g_respectFlash > 0.0f) {
            s.setHidden(m_respectText, false);
            s.setTextAlpha(m_respectText, g_respectFlash > 1.0f ? 1.0f : g_respectFlash);
        } else {
            s.setHidden(m_respectText, true);
        }
    }
    respectNews(career, race, cars, dt);
}

// 0x004BD130 (not in a round of type 2, with the first racer's race time from 10 s and every crew of the round with
// its session's +0x340 set): the first player's messages, in green or red, about the crew's place (crewPlace, by the
// points in a round of type 5) against the last (the career's +0x34 object +0x00): the messages emptied, "Gained a
// place!" / "Lost a place!" and "Player's crew <place><ending>"; every 5 s at most the lead's step (the lead / 100,
// / 20 in a round of type 5, against +0x04): ahead "Improving lead!" / "Pack gaining!", behind "Catching up!" /
// "Dropping back!"; a bet's state against +0x2C ("Winning the bet!" / "Losing the bet!"); a rival crew's rams changed
// (+0x0C ..) "Damage caused!" at most every 3 s.
void RaceHud::respectNews(Career& career, const HudRace& race, const std::vector<const RaceCar*>& cars, float dt) {
    const CalendarEvent* event = currentEvent(career.calendar);
    if (event == nullptr || race.players.empty()) return;
    const EventRound& round = *roundOf(career, *event);
    if (round.type == EventKind::Showoff) return;
    const RaceSession& session = career.session;
    if (!cars.empty() && cars[0] != nullptr && cars[0]->competitor != nullptr &&
        cars[0]->competitor->raceTime < 10.0f) {
        return;
    }
    const int32_t crews = session.event != nullptr ? roundOf(career, *session.event)->opponents : 0;
    for (int32_t i = 0; i < crews; ++i) {
        const auto k = static_cast<size_t>(i);
        if (k >= session.order.size() || session.order[k] == 0) return;
    }
    HudMessages& news = m_messages[0];
    RaceNews& kept = career.news;
    const auto line = [&](size_t index) -> const std::wstring& { return m_texts.at(index); };
    static const size_t kEndings[10] = {40, 37, 38, 39, 40, 40, 40, 40, 40, 40};
    const int32_t place = round.type == EventKind::Sprint ? -1 : crewPlace(career, cars, 0);
    // A round of type 5 places by the points (0x004A5670), which gives no -1.
    const int32_t shown = round.type == EventKind::Sprint ? raceRespectPlace(career, cars) : place;
    if (shown != -1) {
        const int32_t last = kept.place;
        if (last != -1 && shown != last) {
            const bool gained = shown < last;
            const uint32_t colour = gained ? kNewsGood : kNewsBad;
            news.clear();
            news.push(line(gained ? kTextGainedPlace : kTextGainedPlace + 1), 1.0f, colour);
            const size_t ending = kEndings[static_cast<size_t>(shown + 1) % 10];
            news.push(line(kTextCrewNames) + widen(" " + std::to_string(shown + 1)) + line(ending), 1.0f, colour);
        }
        kept.place = shown;
    }
    g_leadNewsTime = g_leadNewsTime - dt;
    if (g_leadNewsTime < 0.0f) {
        const int32_t lead = raceRespectLead(career, cars);
        const int32_t step = round.type == EventKind::Sprint ? lead / 20 : lead / 100;
        const int32_t before = kept.leadStep;
        if (step != before) {
            if (before < 0 || step < 0) {
                const bool better = step < before;
                news.push(line(better ? kTextGainedPlace + 2 : kTextGainedPlace + 3), 1.0f,
                          better ? kNewsGood : kNewsBad);
                g_leadNewsTime = 5.0f;
            }
            if (before > 0 || step > 0) {
                const bool better = step < before;
                news.push(line(better ? kTextGainedPlace + 4 : kTextGainedPlace + 5), 1.0f,
                          better ? kNewsGood : kNewsBad);
                g_leadNewsTime = 5.0f;
            }
            kept.leadStep = step;
        }
    }
    if (career.bets.placed != 0) {
        const int32_t rival = crewPlace(career, cars, career.bets.crew);
        const int32_t own = crewPlace(career, cars, 0);
        const int32_t winning = own < rival ? 1 : 0;
        if (winning != kept.winningBet) {
            kept.winningBet = winning;
            news.push(line(winning != 0 ? kTextGainedPlace + 6 : kTextGainedPlace + 7), 1.0f,
                      winning != 0 ? kNewsGood : kNewsBad);
        }
    }
    g_damageNewsTime = g_damageNewsTime - dt;
    for (size_t k = 1; k < 9; ++k) {
        int32_t& rams = kept.rams[k - 1];
        const float now = career.crews[k].rams;
        if (static_cast<float>(rams) == now) continue;
        rams = static_cast<int32_t>(now);
        if (g_damageNewsTime < 0.0f) {
            news.push(line(kTextGainedPlace + 8), 1.0f, kNewsBad);
            g_damageNewsTime = 3.0f;
        }
    }
}

}  // namespace oj
