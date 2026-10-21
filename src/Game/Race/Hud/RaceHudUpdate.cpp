// The race's HUD following the race each frame (docs/spec/22-race-hud.md): its panels, the cruise's.

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

constexpr float kNoRecord = 300.0f;      // a route's lap record before any (0x0040A110)

// [0x00725370]: the limiter's flicker, kept across races (1 at the start of the game).
int32_t g_limiterFlicker = 1;

// [0x0072536C]
constexpr float kRevMeterScale = 0.725f;

// 0x004F7460: the speed at the end of the torque curve in the highest gear (the speedometer's end).
float speedometerEnd(const CarDynamics& d) {
    const float last = d.torqueSpeeds.empty() ? 0.0f : d.torqueSpeeds.back();
    const float ratio = d.forwardRatios.empty() ? 0.0f : d.forwardRatios.back();
    const WheelDef wheel = d.wheels.empty() ? WheelDef{} : d.wheels.front();
    return last / ratio / wheel.finalDrive * wheel.radius;
}

}  // namespace

void RaceHud::update(const HudRace& race, float dt) {
    ui::Screen& s = m_screen;
    m_frameTime = dt;
    if (race.race == nullptr) return;
    respect(race, dt);
    RaceState& state = *race.race;
    const bool mph = race.settings != nullptr && race.settings->u32(save::Settings::kMph) != 0;
    for (size_t i = 0; i < race.players.size() && i < m_players.size(); ++i) {
        Player& p = m_players[i];
        RaceCar* car = const_cast<RaceCar*>(race.players[i].car);
        if (car == nullptr || car->competitor == nullptr || car->body == nullptr) continue;
        Competitor& r = *car->competitor;
        const CarBody& body = *car->body;
        // The player in their own car (P+4), not a camera's car ([esp+0x70], 0x0044B42E).
        const bool ownCar = race.players[i].own;
        const CarDynamics& def = body.dynamics();
        // The records' panels fade once the race runs: out below 1 s, in above 3 s.
        if (r.raceTime > 0.0f) {
            p.fade = p.fade - dt;
            const float t = p.fade;
            if (t < 1.0f) {
                fadePanels(p, t);
            } else if (t > 3.0f && t < 4.0f) {
                fadePanels(p, 4.0f - t);
            } else {
                fadePanels(p, 1.0f);
            }
        }
        if (r.finished) {
            for (int e : {p.winnerBox, p.winner}) {
                if (e >= 0 && s.hidden(e)) s.show(s.nameOf(e), true);
            }
        }
        if (p.wrongWay >= 0) {
            p.wrongWayFade = r.wrongWay ? p.wrongWayFade + dt : p.wrongWayFade - dt;
            p.wrongWayFade = std::clamp(p.wrongWayFade, 0.0f, 1.0f);
            s.setTextAlpha(p.wrongWay, p.wrongWayFade);
            s.setHidden(p.wrongWay, p.wrongWayFade == 0.0f);
        }
        const float powerMax = def.powerRangeMax;
        const int32_t lap = r.lap;
        // The rev counter: the engine's speed over the top of the power range, or past the torque curve's end the
        // limiter's flicker between 1.1 and 1.15 of the dial.
        if (p.revs >= 0) {
            const float rpm = body.engineSpeed;
            const float last = def.torqueSpeeds.empty() ? 0.0f : def.torqueSpeeds.back();
            const float lo = s.dialMin(p.revs);
            const float hi = s.dialMax(p.revs);
            float share = 0.0f;
            if (last * 9.3105640f >= rpm * 9.5492964f) {
                share = std::clamp(rpm / powerMax * 9.5492964f, 0.0f, 1.2f);
                s.setDial(p.revs, (hi - lo) * share + lo);
            } else {
                share = g_limiterFlicker != 0 ? 1.1f : 1.15f;
                g_limiterFlicker = 1 - g_limiterFlicker;
                s.setDial(p.revs, (hi - lo) * share + lo, true);
            }
            if (p.gearLight >= 0) s.setHidden(p.gearLight, true);
            if (m_revMeter >= 0) s.setMeter(m_revMeter, kRevMeterScale * share);
        }
        rivalsAndAid(race, p, i, *car, dt);
        // The speed (m/s along the car), shown in miles or kilometres an hour, cut toward zero.
        float lights = body.localVelocity[2];
        if (p.speed >= 0) {
            const float v = std::fabs(body.localVelocity[2]);
            if (p.speedo >= 0) {
                if (p.topSpeed == -1.0f) p.topSpeed = speedometerEnd(def);
                const float lo = s.dialMin(p.speedo);
                const float hi = s.dialMax(p.speedo);
                s.setDial(p.speedo, v / p.topSpeed * (hi - lo) + lo);
            }
            lights = v * (mph ? 2.235f : 3.6f);
            const std::string shown = format("%03d", static_cast<int>(lights));
            text(p.speed, shown);
            text(m_speedChallenge, shown);
        }
        if (p.absLight >= 0) s.setHidden(p.absLight, !(body.absActive && lights > 5.0f));
        if (p.tcLight >= 0) s.setHidden(p.tcLight, !body.asrActive);
        if (p.gear >= 0 && body.gear <= 7) {
            static const char* const kGears[] = {"R", "N", "1", "2", "3", "4", "5", "6"};
            text(p.gear, kGears[body.gear]);
        }
        // The place among the competitors once the lap counts, else "-".
        if (p.pos >= 0 && p.posNum >= 0 && p.posTh >= 0) {
            if (lap > 0) {
                const int32_t place = r.place != 0 ? r.place : 1;
                text(p.pos, format("%d", place));
                if (place >= 1 && place <= 6) {
                    s.setText(p.posTh, line(kTextOrdinals + static_cast<size_t>(std::min(place, 4) - 1)));
                } else {
                    s.clearText(p.posTh);
                }
            } else {
                text(p.pos, "-");
                s.clearText(p.posTh);
            }
            text(p.posNum, format("/%d", static_cast<int>(state.competitors().size())));
        }
        if (p.lap >= 0 && p.lapNum >= 0) {
            const int32_t shown = std::min(lap, state.laps);
            text(p.lap, shown > 0 ? format("%d", shown) : std::string("-"));
            text(p.lapNum, format("/%d", state.laps));
        }
        if (p.time >= 0 && race.type == RaceType::Cruise && race.cruise != nullptr) {
            // 0x0044BD9A, a cruise: a crash (the cruise's +0x430, cleared) plays the sound WIPEOUT (at [0x007187DC], 1);
            // TIME the time left; while it runs the score over the record (+0x68) becomes it; TOPSCORE that, cut;
            // WR_TOPSCORE (with the online records) the world's likewise, else "-".
            Cruise& c = *race.cruise;
            const bool crashed = c.crashed != 0;
            c.crashed = 0;
            if (crashed && s.sound) s.sound("WIPEOUT");
            text(p.time, hudTime(c.time));
            if (c.started != 0) {
                const auto score = static_cast<float>(x87::chop(c.total));
                if (score > m_lapRecord) m_lapRecord = score;
            }
            if (m_topScore >= 0) text(m_topScore, format("%d", x87::chop(m_lapRecord)));
            if (m_worldRecord < 0.0f && m_wrTopScore >= 0) text(m_wrTopScore, std::string("-"));
        } else if (p.time >= 0 && race.type != RaceType::Cruise) {
            text(p.time, hudTime(r.lapTime));
            if (race.type != RaceType::PerfectLap && race.type != RaceType::TimedLap && race.type != RaceType::Speed) {
                if (p.lrTime >= 0) {
                    text(p.lrTime, m_lapRecord == kNoRecord ? std::string("--:--:--") : hudTime(m_lapRecord));
                }
                if (!(m_worldRecord < 0.0f)) {
                    if (p.wrTime >= 0) {
                        text(p.wrTime, hudTime(m_worldRecord));
                        s.setHidden(p.wrTime, false);
                    }
                    if (p.digitalWorld >= 0) s.setHidden(p.digitalWorld, false);
                    if (p.wrLapTime >= 0) s.setHidden(p.wrLapTime, false);
                }
            }
        }
        // The count's pictures: each whole second the count drops below, from where it was when the window came;
        // then GO when it is 0. Each grows from the point (0.5, 0.25) and fades once past an eighth of the screen.
        if (!m_countdownDone) {
            if (static_cast<float>(m_countdown) > state.count) {
                static const char* const kCount[] = {"ui_start_01", "ui_start_02", "ui_start_03"};
                if (m_countdown >= 1 && m_countdown <= 3 && p.countdown >= 0) {
                    s.setTexture(p.countdown, kCount[m_countdown - 1]);
                    s.setPlace(p.countdown, 0.5f, 0.25f, 0.0f, 0.0f);
                    s.setHidden(p.countdown, false);
                }
                if (i == race.players.size() - 1) --m_countdown;
            }
            if (m_countdown == 0 && state.count == 0.0f) {
                if (p.countdown >= 0) {
                    s.setTexture(p.countdown, "ui_start_go");
                    s.setPlace(p.countdown, 0.5f, 0.25f, 0.0f, 0.0f);
                    s.setHidden(p.countdown, false);
                }
                if (i == race.players.size() - 1) m_countdownDone = true;
            }
        }
        if (p.countdown >= 0 && !s.hidden(p.countdown)) {
            float x, y, w, h;
            s.placeOf(p.countdown, x, y, w, h);
            const float grow = dt * 0.5f;
            w = w + grow;
            h = grow + h;
            const float move = dt * 0.25f;
            s.setPlace(p.countdown, x - move, y - move, w, h);
            uint32_t colour = 0xFFFFFFFF;
            if (w > 0.125f) {
                const float a = std::clamp(1.0f - (w - 0.125f) * 2.0f, 0.0f, 1.0f);
                colour = (colour & 0x00FFFFFF) | (static_cast<uint32_t>(static_cast<int>(a * 255.0f)) << 24);
            }
            s.setColour(p.countdown, colour);
            if (w > 0.375f) s.setHidden(p.countdown, true);
        }
        // The first of the player's messages.
        if (p.msg >= 0) {
            HudMessages& m = m_messages[i];
            if (m.changed && !m.list.empty()) {
                m.changed = false;
                s.setText(p.msg, m.list.front().text);
                s.setFontColour(p.msg, m.list.front().colour);
            }
            if (m.list.empty()) s.clearText(p.msg);
            s.setTextAlpha(p.msg, m.alpha);
            s.setAlpha(p.msg, 0.0f);
        }
        // 0x0044C61D: in a sprint the race track's first player's race complete (taken, made 0): the car's best lap
        // takes the world record (the statistics' too, 0x0044C65E) and the lap record when better.
        if (race.type == RaceType::Sprint && race.takeFirstComplete && race.takeFirstComplete()) {
            const float best = r.bestLap;
            if (best < m_worldRecord) {
                m_worldRecord = best;
                net::stats::worldBestTime() = best;
            }
            if (best < m_lapRecord) m_lapRecord = best;
        }
        if (p.nosMeter >= 0) s.setMeter(p.nosMeter, body.nitroLeft * 0.06666667f);
        if (race.type != RaceType::Cruise) {
            if (r.newLap) {
                // Every new lap: the first player's messages cleared, the panels in again, the lap's message.
                m_messages[0].clear();
                p.fade = keepsPanels(race.type) || race.type == RaceType::TimedLap ? kNeverFades : 4.0f;
                HudMessages& m = m_messages[i];
                const float best = r.bestLap;
                if (best < m_worldRecord) {
                    m_worldRecord = best;
                    net::stats::worldBestTime() = best;  // 0x0044C737
                    if (best < m_lapRecord) m_lapRecord = best;
                    m.push(line(kTextNewWorldRecord), 2.0f);
                } else if (best < m_lapRecord) {
                    m_lapRecord = best;
                    m.push(line(kTextNewLapRecord), 2.0f);
                } else if (r.lap == state.laps) {
                    m.push(line(kTextFinalLap), 2.0f);
                } else if (r.newRecord && ownCar) {
                    m.push(line(kTextFastestLap), 2.0f);
                } else if (r.newBestLap) {
                    m.push(line(kTextNewBestLap), 2.0f);
                } else {
                    m.push(line(kTextLapComplete), 2.0f);
                }
            } else if (r.newSector && r.gapAhead > 0.0f && m_carAhead >= 0) {
                s.setText(m_carAhead, line(kTextCarAhead) + L" " + widen(hudTime(r.gapAhead)));
                s.setFontColour(m_carAhead, 0xFFFFFFFF);
                s.setTextAlpha(m_carAhead, 1.0f);
                m_carAheadTimer = 3.0f;
                r.gapAhead = 0.0f;
            }
            // The split at each sector: green when ahead of the best, red behind; 2 s, fading in the last.
            if ((r.newSector || r.newLap) && race.type != RaceType::Sprint && p.split >= 0 && r.sectorDelta != 0.0f) {
                const float d = r.sectorDelta;
                const std::string signed_ = d >= 0.0f ? "+" + hudTime(d) : "-" + hudTime(-d);
                s.setText(p.split, line(kTextSplit) + L" " + widen(signed_));
                s.setFontColour(p.split, d > 0.0f ? 0xFFFF5050 : 0xFF50FF50);
                s.setTextAlpha(p.split, 1.0f);
                p.splitTimer = 2.0f;
            }
        }
        if (p.split >= 0) {
            p.splitTimer = std::clamp(p.splitTimer - dt, 0.0f, 2.0f);
            if (p.splitTimer < 1.0f) s.setTextAlpha(p.split, p.splitTimer);
        }
        // The nitro running low, then out (each once a race).
        if (race.type != RaceType::Cruise && ownCar && p.nosMeter >= 0) {
            HudMessages& m = m_messages[i];
            // Each with the sound WARNING (0x0044CCAC).
            if (body.nitroLeft < 2.0f && car->nitroWarning < 1) {
                m.push(line(kTextNitrousLow), 2.0f);
                car->nitroWarning = 1;
                if (s.sound) s.sound("WARNING");
            } else if (body.nitroLeft < 0.01f && car->nitroWarning < 2) {
                m.push(line(kTextNitrousEmpty), 2.0f);
                car->nitroWarning = 2;
                if (s.sound) s.sound("WARNING");
            }
        }
        if (r.gapBehind != 0.0f && m_carBehind >= 0) {
            s.setText(m_carBehind, line(kTextCarBehind) + L" " + widen(hudTime(r.gapBehind)));
            s.setFontColour(m_carBehind, 0xFFFFFFFF);
            s.setTextAlpha(m_carBehind, 1.0f);
            m_carBehindTimer = 3.0f;
            r.gapBehind = 0.0f;
        }
        for (auto [element, timer] :
             {std::pair{m_carAhead, &m_carAheadTimer}, std::pair{m_carBehind, &m_carBehindTimer}}) {
            if (element < 0) continue;
            if (*timer > 0.0f) {
                *timer = std::clamp(*timer - dt, 0.0f, 10.0f);
                if (*timer < 1.0f) s.setTextAlpha(element, *timer);
            } else {
                *timer = 0.0f;
                s.clearText(element);
            }
        }
        // The player's own car: what the damage did this frame.
        if (ownCar) {
            HudMessages& m = m_messages[i];
            // Each with the sound WARNING (0x0044CF86, 0x0044CFCF, 0x0044D031).
            const auto warn = [&s] {
                if (s.sound) s.sound("WARNING");
            };
            if (car->steeringFault) {
                m.push(line(kTextSteeringDamaged), 2.0f);
                warn();
            }
            if (car->turboLost) {
                m.push(line(kTextTurboDamaged), 2.0f);
                warn();
            }
            if (car->nitroLost && body.nitroLeft > 0.0f) {
                m.push(line(kTextNitrousLeak), 2.0f);
                warn();
            }
        }
    }
    cruise(race);
    const HudCar* first = race.players.empty() ? nullptr : &race.players.front();
    const RaceCar* firstCar = first != nullptr ? first->car : nullptr;
    if (m_best >= 0 && race.type != RaceType::PerfectLap && race.type != RaceType::TimedLap &&
        race.type != RaceType::Speed && firstCar != nullptr && firstCar->competitor != nullptr) {
        text(m_best, hudTime(firstCar->competitor->bestLap));
    }
    // A sprint (type 6) after the count: a gear up by hand this frame (the player's +0x30) empties the first player's
    // messages and rates the change by the engine's revs (+0x358 x 9.549296) against the car's top of the power range
    // (+0x160): "EARLY CHANGE" (line 79) below -150, "LATE CHANGE" (80) above 150, else "OPTIMUM CHANGE" (78), for 1 s.
    if (m_countdownDone && race.type == RaceType::Sprint && race.shiftedUp && firstCar != nullptr &&
        firstCar->body != nullptr && !race.players.empty()) {
        const CarBody& body = *firstCar->body;
        const float off = body.engineSpeed * std::bit_cast<float>(0x4118C9EBu) - body.dynamics().powerRangeMax;
        HudMessages& m = m_messages[0];
        m.clear();
        m.push(line(off < -150.0f ? kTextChange + 1 : off > 150.0f ? kTextChange + 2 : kTextChange), 1.0f);
    }
    if (m_turbo >= 0 && firstCar != nullptr && firstCar->body != nullptr) {
        const CarBody& body = *firstCar->body;
        const float lo = s.dialMin(m_turbo);
        const float hi = s.dialMax(m_turbo);
        const float boost = firstCar->hasTurbo && body.gear != 1 ? body.turboBoost() : 0.0f;
        s.setDial(m_turbo, (hi - lo) * boost + lo);
    }
    // A speed challenge (type 2), the first player's messages (each with the sound WARNING): the trap's speed
    // reached this frame (+0x130) "SPEED ACHIEVED" for 2 s; before it is reached (+0x12C), with no message waiting,
    // "ACHIEVE TARGET SPEED" for 1.5 s while the race time is within 5 .. 6, 7 .. 8 or 9 .. 10 s; after it, below the
    // challenge's speed + 15 (the car's speed x 2.235, whatever the units), "DRIVE FASTER" for 1.5 s.
    if (race.type == RaceType::Speed && firstCar != nullptr && firstCar->competitor != nullptr &&
        firstCar->body != nullptr && !race.players.empty()) {
        const Competitor& c = *firstCar->competitor;
        HudMessages& m = m_messages[0];
        const auto warn = [&s] {
            if (s.sound) s.sound("WARNING");
        };
        if (c.trapNow) {
            m.push(line(kTextSpeedAchieved), 2.0f);
            warn();
        }
        const float t = c.raceTime;
        for (const float from : {5.0f, 7.0f, 9.0f}) {
            if (t > from && t < from + 1.0f && !c.trapPassed && m.list.empty()) {
                m.push(line(kTextSpeedAchieved + 1), 1.5f);
                warn();
            }
        }
        const Vec4& v = firstCar->body->localVelocity;
        const float speed = std::sqrt((v[2] * v[2] + v[1] * v[1]) + v[0] * v[0]) * 2.235f;
        if (c.trapPassed && speed < race.challengeSpeed + 15.0f && m.list.empty()) {
            m.push(line(kTextSpeedAchieved + 2), 1.5f);
            warn();
        }
    }
    // The map's setting (the settings' +0x2C) shows or hides every element with MAP in its name.
    const bool map = race.settings != nullptr && race.settings->u32(save::Settings::kMap) != 0;
    if (map != m_mapShown) {
        m_mapShown = map;
        if (map) {
            s.showContaining("MAP", false);
        } else {
            s.hideContaining("MAP");
        }
    }
    if (m_mapShown) moveMap(race);
    if (race.players.size() == 1 && firstCar != nullptr && firstCar->competitor != nullptr) raceNews(race, *firstCar);
}

// The cruise on the HUD. With Menu, MenuPoints and Points (0x004ABE70): once the time is out both lists emptied, then
// (0x004AB640) MenuPoints "Score <score>" (176, the thousands marked) selected and the car's competitor finished; else
// both lists given the eight lines (the names in Menu, the points in MenuPoints): a crash last runs its time down and,
// out, its line goes from both; a last line no longer going on and out of time adds an empty line to both and glides
// to it; else its time runs down and the eighth line is selected; Points the combo and the bank ("%d", cut). With the
// cruise running: score " <score>" (cut); combo " x<multiplier>" (0x004AC390): steady, else (the multiplier's change
// up with CLICK, down with NO) blinking each 0.25 s while its +0x4C runs down; regionmessage the name of the zone
// the camera was last in (0x004B15D0), blinking each 0.25 s, empty without one.
void RaceHud::cruise(const HudRace& race) {
    if (race.cruise == nullptr) return;
    Cruise& c = *race.cruise;
    ui::Screen& s = m_screen;
    if (m_menu >= 0 && m_menuPoints >= 0 && m_points >= 0) {
        const std::string& names = s.nameOf(m_menu);
        const std::string& points = s.nameOf(m_menuPoints);
        if (c.time <= 0.0f) {
            s.clearMenu(names);
            s.clearMenu(points);
            s.addMenuItem(points, line(kTextScore) + L" " + widen(groupThousands(c.score(), m_languageIndex)));
            s.selectMenuItem(points, 0);
            c.timeUp();
        } else {
            s.clearMenu(names);
            s.clearMenu(points);
            for (const CruiseMessage& m : c.messages) {
                s.addMenuItem(names, m.name);
                s.addMenuItem(points, m.points);
            }
            CruiseMessage& last = c.messages.back();
            if (last.trick == 17) {
                last.time = last.time - c.dt;
                if (last.time < 0.0f) {
                    s.removeMenuItemAt(names, 7);
                    s.removeMenuItemAt(points, 7);
                } else {
                    s.selectMenuItem(names, 7);
                    s.selectMenuItem(points, 7);
                }
            } else if (last.active == 0 && last.time < 0.0f) {
                s.addMenuItem(names, L"");
                s.addMenuItem(points, L"");
                s.selectMenuItem(names, 8, true);
                s.selectMenuItem(points, 8, true);
            } else {
                last.time = last.time - c.dt;
                s.selectMenuItem(names, 7);
                s.selectMenuItem(points, 7);
            }
            text(m_points, std::format("{}", x87::chop(c.combo + c.bank)));
        }
    }
    if (c.started == 0) return;
    if (m_score >= 0) text(m_score, std::format(" {}", x87::chop(c.total)));
    if (m_combo >= 0) {
        const std::wstring combo = widen(std::format(" x{}", c.multiplier));
        if (c.flash <= 0.0f) {
            s.setText(m_combo, combo);
            c.comboBlink = 0.25f;
        } else {
            // The multiplier's change: up CLICK, down NO (0x004AC434).
            if (c.multiplierShown < c.multiplier) {
                if (s.sound) s.sound("CLICK");
            } else if (c.multiplier < c.multiplierShown) {
                if (s.sound) s.sound("NO");
            }
            c.multiplierShown = c.multiplier;
            c.flash = c.flash - c.dt;
            c.comboBlink = c.comboBlink - m_frameTime;
            if (c.comboBlink < 0.0f) c.comboBlink = 0.25f;
            if (c.comboBlink < 0.125f) {
                s.clearText(m_combo);
            } else {
                s.setText(m_combo, combo);
            }
        }
    }
    if (m_regionMessage >= 0) {
        const std::wstring name = c.lastZone != nullptr ? c.lastZone->name : std::wstring();
        if (name.empty()) {
            s.clearText(m_regionMessage);
            m_regionBlink = 0.5f;
        } else {
            m_regionBlink = m_regionBlink - m_frameTime;
            if (m_regionBlink <= 0.0f) m_regionBlink = 0.5f;
            if (m_regionBlink > 0.25f) {
                s.setText(m_regionMessage, name);
            } else {
                s.clearText(m_regionMessage);
            }
        }
    }
}

void RaceHud::fadePanels(Player& p, float alpha) {
    ui::Screen& s = m_screen;
    for (int box : {p.digitalRecord, p.digitalWorld}) {
        if (box >= 0) s.setAlpha(box, alpha);
    }
    for (int t : {p.lrTime, p.wrTime, p.lrLapTime, p.wrLapTime, m_wrTopScore, m_topScore}) {
        if (t >= 0) s.setTextAlpha(t, alpha);
    }
}

}  // namespace oj
