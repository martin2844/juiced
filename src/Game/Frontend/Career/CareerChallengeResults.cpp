// CMSChallengeResults: a challenge's prize counted.

#include "Game/Frontend/Career/CareerChallengeResults.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

// 0x004D9960: the current event's crew's challenge (its +0x06): +0x0C 0, the money shown (+0x18) the player's;
// raceName the crew's name, ": " and the challenge's type (372 + type), over several lines. By the type: lap time
// and perfect lap criteria "Time", criteria1 the result (+0x18 as a time, "-" for none) and required1 the value as a
// time; speed "Speed", criteria1 empty and required1 the value with "MPH" (the settings' +0x40) or x 1.6093 with
// "KPH"; cruise "Score", criteria1 the race's money (the career's +0x60 object's +0x30) and required1 the value, both
// cut with thousands marked. Met (0x004BACB0) and not done before: winnings1 the prize (+0x0C), the window's money
// the prize and the challenge done; else winnings1 $0. outcome "CHALLENGE COMPLETED" or "CHALLENGE FAILED". The
// prize paid (0x004B8200's lines), its 0.05 the step (+0x10), 3 s (+0x1C) before the count; each race car's +0xAC and
// the profile's +0x4EC take the prize; the money after (+0x14); RACENAME over several lines; +0x20 and +0x24 0.
void CareerChallengeResults::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    CareerProfile& p = career.profile;
    CalendarEvent* e = currentEvent(career.calendar);
    if (e == nullptr || e->crew < 0 || e->crew >= 9 || e->challengeIndex < 0 || e->challengeIndex >= 3) return;
    CrewChallenge& c = career.crews[static_cast<size_t>(e->crew)].challenges[static_cast<size_t>(e->challengeIndex)];
    m_won = 0.0f;
    m_shown = static_cast<float>(p.money);
    s.setText("raceName",
              texts.at(kTextCrewNames + static_cast<size_t>(e->crew)) + L": " +
                  texts.at(kTextChallengeTypes + static_cast<size_t>(std::max(static_cast<int32_t>(c.type), 0))));
    s.setMultiline("raceName", true);
    const auto number = [&](int32_t value) { return toWide(groupThousands(value, host.language().index)); };
    switch (c.type) {
    case ChallengeType::LapTime:
    case ChallengeType::PerfectLap: {
        s.setText("criteria", texts.at(kTextTime));
        const float result = std::bit_cast<float>(c.result);
        s.setText("criteria1", result > 0.0f ? toWide(hudTime(result)) : L"-");
        s.setText("required1", toWide(hudTime(static_cast<float>(c.value))));
        break;
    }
    case ChallengeType::Speed:
        s.setText("criteria", texts.at(kTextSpeed));
        s.setText("criteria1", L"");
        if (host.settingsValue(save::Settings::kMph) != 0) {
            s.setText("required1", toWide(std::format("{} ", c.value)) + texts.at(kTextMph));
        } else {
            const int32_t kph = x87::chop(static_cast<float>(c.value) * 1.6093f);
            s.setText("required1", toWide(std::format("{} ", kph)) + texts.at(kTextKph));
        }
        break;
    case ChallengeType::Showoff:
        s.setText("criteria", texts.at(kTextScore));
        s.setText("criteria1", number(x87::chop(raceMoney(career))));
        s.setText("required1", number(c.value));
        break;
    default:
        break;
    }
    const bool met = challengeMet(career, data->race);
    if (met && c.done == 0) {
        s.setText("winnings1", toWide(moneyText(c.prize, host.language().index)));
        m_won = static_cast<float>(c.prize);
        c.done = 1;
    } else {
        s.setText("winnings1", toWide(moneyText(0, host.language().index)));
    }
    s.setText("outcome", texts.at(met ? kTextChallengeCompleted : kTextChallengeFailed));
    pay(career, -x87::chop(m_won));
    m_step = m_won * 0.05f;
    m_delay = 3.0f;
    for (int32_t k = 0; k < p.raceCarCount && k < 3; ++k) {
        const auto index = static_cast<size_t>(p.raceCars[static_cast<size_t>(k)]);
        if (index >= career.garage.size()) continue;
        Car& car = career.garage[index];
        car.winnings = std::bit_cast<uint32_t>(std::bit_cast<float>(car.winnings) + m_won);
        const auto prizes = static_cast<float>(static_cast<int32_t>(p.prizesWon));
        p.prizesWon = static_cast<uint32_t>(x87::chop(prizes + m_won));
    }
    m_after = m_shown + m_won;
    s.setMultiline("RACENAME", true);
    m_finished = 0;
    m_autosave = 0;
}

// 0x004D9FB0: +0x24: the autosave over the window when saving, +0x24 0 and +0x20 set. totalWinnings "TOTAL WINNINGS"
// and the money left to count, cash the money shown; after 3 s each frame the step moves (SWOOSH) until none is left
// (then the money after). +0x20: the event's end (0x004A6870), CLICK, the current round's and the event's +0x01 set and
// the next window (0x004BAEE0). SELECT: a challenge met gives the crew's respect message of kind 8 (0x004BC7B0) and
// sets +0x24, else +0x20.
void CareerChallengeResults::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    if (m_autosave != 0) {
        if (data->saving != 0) host.request("autosave", SwitchMode::Push);
        m_autosave = 0;
        m_finished = 1;
        return;
    }
    s.setText("totalWinnings",
              texts.at(kTextTotalWinnings) + toWide(" " + moneyText(x87::chop(m_won), host.language().index)));
    s.setText("cash", toWide(moneyText(x87::chop(m_shown), host.language().index)));
    if (m_delay > 0.0f) {
        m_delay = m_delay - host.frameTime();
    } else if (!(m_won > 0.0f)) {
        m_shown = m_after;
        m_won = 0.0f;
    } else {
        m_shown = m_step + m_shown;
        m_won = m_won - m_step;
        host.sound("SWOOSH");
    }
    if (m_finished != 0) {
        CalendarEvent* current = currentEvent(career.calendar);
        if (current != nullptr) endEvent(career, *current, data->tables);
        host.sound("CLICK");
        if (current != nullptr) {
            const_cast<EventRound*>(roundOf(career, *current))->ended = 1;
            current->done = 1;
        }
        host.afterRaceResults();
        return;
    }
    if (event != input::kSelect) return;
    if (challengeMet(career, data->race)) {
        const CalendarEvent* current = currentEvent(career.calendar);
        crewRespect(host, career, current != nullptr ? current->crew : 0, RespectReason::ChallengeCompleted, 0,
                    host.language().index, texts);
        m_autosave = 1;
    } else {
        m_finished = 1;
    }
}

}  // namespace oj::frontend
