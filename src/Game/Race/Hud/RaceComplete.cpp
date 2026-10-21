// hud_race_complete.

#include "Game/Race/Hud/RaceComplete.h"
#include "Game/Race/Hud/HudInternal.h"

#include "Core/Career/Money.h"
#include "Core/Data/GameTexts.h"

#include <algorithm>
#include <cmath>

namespace oj {

using namespace hud;

namespace {

}  // namespace

RaceComplete::RaceComplete(ui::WindowManager& windows, const TextTable& texts)
    : m_windows(windows), m_texts(texts) {}

// 0x0044FD20: the players' messages emptied, 5 s; MSG by the race's type: a challenge (0 .. 2) CHALLENGE FAILED for
// the outcomes 2 .. 5, else CHALLENGE COMPLETED with the best lap in POS; 3 and 4 TIME UP (a cruise running: POS
// "Score <score>"); 9 Completed with POS emptied; else, off the network with the player's car (in the career
// not in a round whose +0x17 is over 1), WINNER for the first place or RACE OVER, the place in POS ("1st place"),
// else RACE OVER. A place or nothing shown in POS makes POS the text that fades. In the career with the profile's
// +0x00 at 0 Counter takes the race's respect (the career's +0x34 object's +0x30, cut) with its text emptied, then the
// garage's respect not applied: the best car (0x004BB3D0, crew 5) and the garage's worth (0x004BB2F0, crew 6; the
// sound CLICK with the second), each not 0 counted in after a wait of 1 s, the window held 1.5 s; the race's respect
// becomes the three's sum; RespMsg and RespCounter emptied. Else Counter, RespMsg, RespCounter and every Respect_Icon are
// hidden.
bool RaceComplete::enter(const HudRace& race, RaceHud* hud) {
    m_screen = m_windows.open("hud_race_complete");
    if (!m_screen->loaded()) {
        m_screen.reset();
        return false;
    }
    // The speech leaves the 3D world's volume alone (+0x24); the world's player's master kept (+0x14).
    if (Speech* speech = gameSounds().speech) speech->holdWorld = true;
    if (audio::World* world = gameSounds().world) m_master = world->player().masterVolume();
    ui::Screen& s = *m_screen;
    if (hud != nullptr) {
        for (size_t i = 0; i < race.players.size(); ++i) hud->messages(i).clear();
    }
    m_time = 5.0f;
    m_fading = s.find("MSG");
    const int pos = s.find("POS");
    const RaceCar* car = race.players.empty() ? nullptr : race.players[0].car;
    const Competitor* c = car != nullptr ? car->competitor : nullptr;
    const auto line = [&](size_t index) -> const std::wstring& { return m_texts.at(index); };
    if (m_fading >= 0) {
        const RaceType type = race.type;
        if (type >= RaceType::TimedLap && type <= RaceType::Speed) {
            if (race.challenge >= ChallengeOutcome::OutOfTime && race.challenge <= ChallengeOutcome::SpeedNotReached) {
                s.setText(m_fading, line(kTextChallengeFailed));
            } else {
                s.setText(m_fading, line(kTextChallengeCompleted));
                if (pos >= 0 && c != nullptr) s.setText(pos, widen(hudTime(c->bestLap)));
            }
        } else if (type == RaceType::Solo || type == RaceType::Cruise) {
            s.setText(m_fading, line(kTextTimeUp));
            // A cruise running: POS "Score <score>" with the thousands marked.
            if (type == RaceType::Cruise && pos >= 0 && race.cruise != nullptr && race.cruise->started != 0) {
                s.setText(pos, line(kTextScore) + L" " + widen(groupThousands(race.cruise->score(), m_windows.language().index)));
            }
        } else if (type == RaceType::TestTrack) {
            s.setText(m_fading, line(kTextCompleted));
            m_fading = pos;
            if (pos >= 0) s.clearText(pos);
        } else {
            bool placed = car != nullptr;
            if (race.mode == RaceMode::Career && race.session != nullptr && race.session->round != nullptr &&
                race.session->round->teamSize > 1) {
                placed = false;
            }
            if (!race.networked && placed && c != nullptr) {
                const int32_t place = c->finishPlace;
                s.setText(m_fading, line(place == 1 ? kTextWinner : kTextRaceOver));
                m_fading = pos;
                if (pos >= 0) {
                    const int32_t shown = place >= 2 && place <= 6 ? place : 1;
                    const size_t suffix = kTextOrdinals + static_cast<size_t>(std::min(shown, 4) - 1);
                    s.setText(pos, widen(std::to_string(shown)) + line(suffix) + line(kTextPlace));
                }
            } else {
                s.setText(m_fading, line(kTextRaceOver));
            }
        }
    }
    m_counter = s.find("Counter");
    m_respectMessage = s.find("RespMsg");
    m_respectCounter = s.find("RespCounter");
    m_countingBest = 0;
    m_countingCollection = 0;
    m_hold = 0.0f;
    if (race.mode == RaceMode::Career && race.career != nullptr && race.career->profile.introduction == 0 &&
        race.catalogue != nullptr) {
        Career& career = *const_cast<Career*>(race.career);
        m_respect = static_cast<float>(x87::chop(career.news.respect));
        if (m_counter >= 0) s.clearText(m_counter);
        m_bestCar = static_cast<float>(respectByBestCar(career, career.crews[5], *race.catalogue, false));
        if (m_bestCar != 0.0f) {
            m_bestCarWait = 1.0f;
            m_countingBest = 1;
            m_hold = 1.5f;
        }
        m_collection = static_cast<float>(respectByCollection(career, career.crews[6], *race.catalogue, false));
        if (m_collection != 0.0f) {
            m_collectionWait = 1.0f;
            m_countingCollection = 1;
            if (s.sound) s.sound("CLICK");  // 0x004503A8
            m_hold = 1.5f;
        }
        const float sum = m_bestCar + m_collection + m_respect;
        career.news.respect = sum;
        if (m_respectMessage >= 0) s.clearText(m_respectMessage);
        if (m_respectCounter >= 0) s.clearText(m_respectCounter);
        return true;
    }
    if (m_counter >= 0) s.setHidden(m_counter, true);
    if (m_respectMessage >= 0) s.setHidden(m_respectMessage, true);
    if (m_respectCounter >= 0) s.setHidden(m_respectCounter, true);
    for (int e = 0; e < s.size(); ++e) {
        if (_stricmp(s.nameOf(e).c_str(), "Respect_Icon") == 0) s.setHidden(e, true);
    }
    return true;
}

// 0x004504A0: Counter shows the respect ("+n" green over 0, else "n" red) and the window waits while held. Counting the
// best car: RespCounter shows what is left of it the same way, RespMsg "Best Car", the first player's camera director
// held at 2.5 s; after its wait 3 x the step of what is left goes from it into the respect each frame (the sound swoosh
// each frame) until at most 0.5 is left. Then the garage's worth the same with "Car Collection". RespMsg blinks off
// from 0.15 s of each 0.3 s while counting. Then the respect is the career's again, RespMsg and RespCounter hidden, the
// time runs down, the fading text's alpha is time - 1 over the last 2 s; below 0 the results' window of the race's type
// is asked for (challenge 0 .. 2, cruise 4, else the race's). Over the last 4 s the 3D world goes down to a quarter of
// its player's master kept, its voices paused there.
std::string RaceComplete::update(const HudRace& race, float dt) {
    if (m_screen == nullptr) return {};
    ui::Screen& s = *m_screen;
    m_blink = m_blink + dt;
    if (m_blink > 0.3f) m_blink = 0.0f;
    const auto shown = static_cast<int32_t>(m_respect);
    if (m_counter >= 0) {
        s.setFontColour(m_counter, m_respect > 0.0f ? 0xFF32FF32u : 0xFFFF3232u);
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), m_respect > 0.0f ? "+%d" : "%d", shown);
        s.setText(m_counter, widen(buffer));
    }
    if (m_hold > 0.0f) {
        m_hold = m_hold - dt;
        return {};
    }
    if (m_countingBest != 0 || m_countingCollection != 0) {
        const bool best = m_countingBest != 0;
        float& left = best ? m_bestCar : m_collection;
        float& wait = best ? m_bestCarWait : m_collectionWait;
        if (race.directorTime != nullptr) *race.directorTime = 2.5f;
        if (m_respectCounter >= 0) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), left > 0.0f ? "+%d" : "%d", x87::chop(left));
            s.setFontColour(m_respectCounter, left > 0.0f ? 0xFF32FF32u : 0xFFFF3232u);
            s.setText(m_respectCounter, widen(buffer));
        }
        if (m_respectMessage >= 0) {
            s.setText(m_respectMessage, m_texts.at(best ? kTextBestCar : kTextCarCollection));
            s.setHidden(m_respectMessage, false);
        }
        if (wait <= 0.0f) {
            if (std::abs(left) > 0.5f) {
                m_respect = dt * left * 3.0f + m_respect;
                left = left - left * dt * 3.0f;
                if (s.sound) s.sound("swoosh");
                return {};
            }
            (best ? m_countingBest : m_countingCollection) = 0;
            return {};
        }
        wait = wait - dt;
        if (m_blink > 0.15f && m_respectMessage >= 0) s.setHidden(m_respectMessage, true);
        return {};
    }
    m_respect = race.career != nullptr ? race.career->news.respect : 0.0f;
    if (m_respectMessage >= 0) s.setHidden(m_respectMessage, true);
    if (m_respectCounter >= 0) s.setHidden(m_respectCounter, true);
    m_time = m_time - dt;
    if (m_fading >= 0 && m_time < 2.0f) s.setTextAlpha(m_fading, std::clamp(m_time - 1.0f, 0.0f, 1.0f));
    // Over the last 4 s the 3D world goes down to a quarter of the master kept (down 400, up 50, its player's master
    // at its volume) and its voices pause once the master is there; at the end they pause.
    audio::World* world = gameSounds().world;
    if (world != nullptr) {
        if (m_time < 4.0f && m_time * 0.25f < m_master) {
            world->target = 0.25f * m_master;  // [0x00715C38]
            world->down = 400.0f;              // [0x00715C34]
            world->up = 50.0f;
            world->player().setMasterVolume(world->volume);
        }
        if (world->player().masterVolume() <= 0.25f * m_master) world->manager().pauseAll(true);
    }
    if (!(m_time < 0.0f)) return {};
    if (world != nullptr) world->manager().pauseAll(true);
    const RaceType type = race.type;
    if (type >= RaceType::TimedLap && type <= RaceType::Speed) return "hud_challenge_results";
    if (type == RaceType::Cruise) return "hud_cruise_results";
    return "hud_race_results";
}

void RaceComplete::draw(float seconds, float dt) {
    if (m_screen == nullptr) return;
    m_screen->update(dt);
    m_screen->draw(seconds, dt);
}

}  // namespace oj
