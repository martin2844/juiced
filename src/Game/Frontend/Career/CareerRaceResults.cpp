// The race's results in the career (CMSRaceResults).

#include "Game/Frontend/Career/CareerRaceResults.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/Garage/CarStats.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// 0x004DA310: for the rows 1 .. 3 pos, time, driver 1 .. 3 and bet emptied, crew, timetitle, drivers, winnings and box0
// hidden; then for each of the round's crews up to three, in the crews' order (the session's +0x340): the row shown;
// pos "<n><st / nd / rd>" (lines 37 ..); crew shown (its texture the crew's logo of the flyers, the career's +0x00
// object's +0x58 + crew x 0x24); time the crew's time (its odds' +0x00) for the first, else "+" the time behind the
// first; the drivers of the crew in the session's order as driver<row><k> (their crew members' names); bet the
// round's prize of the place (+0x2C + 4 place, as money). The player's crew (0) wins the prize (the record's +0x0C,
// the profile's +0x14); a crew with a bet (its odds' +0x0C) adds the bet's winnings (+0x10) to it and to the
// career's money counter +0x0C (cut toward zero), and the prize to the profile's +0x10. Then the money: cut toward
// zero, the career's money counters (+0x04 for a gain, +0x00 less a loss), the profile's money (none below 0); the
// count's step 0.05 of it and 3 s before the count; with the race cars chosen (or the profile's +0x618) each race car
// of the garage +0xAC and the profile's +0x4EC take it; Cash and Date (0x004A4130); the money after. With the
// session's first finisher (+0x368) the winning car ([0x00728C60]: of the garage for the player's crew, else of the
// opponents' cars; [0x00721F30] 0) and SEL2 shown, else every element with SEL2 hidden.
void CareerRaceResults::enter(CareerHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    RaceSession& session = career.session;
    const CalendarEvent* event = currentEvent(career.calendar);
    if (event == nullptr) return;
    const EventRound& round = *roundOf(career, *event);
    CareerProfile& profile = career.profile;
    *this = CareerRaceResults{};
    m_shown = static_cast<float>(profile.money);
    const auto text = [&](const std::string& name, const std::wstring& value) {
        if (const int e = s.find(name); e >= 0) s.setText(e, value);
    };
    const auto hidden = [&](const std::string& name, bool hide) {
        if (const int e = s.find(name); e >= 0) s.setHidden(e, hide);
    };
    for (int row = 1; row <= 3; ++row) {
        text(std::format("pos{}", row), L"");
        hidden(std::format("crew{}", row), true);
        text(std::format("time{}", row), L"");
        for (int k = 1; k <= 3; ++k) text(std::format("driver{}{}", row, k), L"");
        text(std::format("bet{}", row), L"");
        for (const char* name : {"timetitle", "drivers", "winnings", "box0"}) {
            hidden(std::format("{}{}", name, row), true);
        }
    }
    const int language = host.language().index;
    // A crew's odds (the session's +0x268 + 0x14 x crew). Stock problem: a place no crew took (-1) reads the 0x14 bytes
    // before the table: the last racer's +0x54 .. +0x60 (its nerves, order, order's time and level's timer) and the
    // session's +0x264, and its row is shown with them (0x004DA783, 0x004DAA9C, 0x004DAD43), its logo the bytes before
    // the crews' (the player's colours 13 .. 20 as a name), and a stake there (the last racer's level timer not 0) adds
    // winnings made of those bytes to the money. The fixed build leaves such a row hidden and counts nothing of it.
    const auto oddsOf = [&](int32_t crew) {
        if (crew >= 0 && static_cast<size_t>(crew) < session.odds.size()) {
            return session.odds[static_cast<size_t>(crew)];
        }
        return CrewOdds{};
    };
    for (int i = 0; i < round.opponents && i < 3; ++i) {
        const int row = i + 1;
        const int32_t crew = session.order[static_cast<size_t>(i)];
        // The order holds crews 0 .. 8 or -1; nothing of the row for another value (none), nor for -1 in the fixed
        // build.
        if (crew < 0 || static_cast<size_t>(crew) >= session.odds.size()) continue;
        const CrewOdds odds = oddsOf(crew);
        for (const char* name : {"timetitle", "drivers", "winnings", "box0"}) {
            hidden(std::format("{}{}", name, row), false);
        }
        hidden(std::format("pos{}", row), false);
        text(std::format("pos{}", row), toWide(std::format("{}", row)) + texts.at(37 + static_cast<size_t>(i)));
        hidden(std::format("crew{}", row), false);
        s.setTexture(std::format("crew{}", row), career.flyers.crews[static_cast<size_t>(crew)].logo);
        if (i == 0) {
            text(std::format("time{}", row), toWide(hudTime(odds.raceTime)));
        } else {
            const float first = oddsOf(session.order[0]).raceTime;
            text(std::format("time{}", row), toWide("+" + hudTime(odds.raceTime - first)));
        }
        int k = 1;
        for (uint32_t r = 0; r < session.count && r < session.racers.size(); ++r) {
            const Racer& racer = session.racers[r];
            if (racer.crew != crew || crew < 0) continue;
            const auto& name = career.crews[static_cast<size_t>(crew)].memberNames[static_cast<size_t>(racer.member)];
            const std::string driver(name.data(), strnlen(name.data(), name.size()));
            text(std::format("driver{}{}", row, k), toWide(driver));
            ++k;
        }
        const int32_t prize = round.prize[static_cast<size_t>(i)];
        text(std::format("bet{}", row), toWide(moneyText(prize, language)));
        if (crew == 0) {
            m_won = static_cast<float>(prize) + m_won;
            profile.crewPrizes += prize;
        }
        if (odds.staked != 0) {
            m_won = odds.winnings + m_won;
            career.stats.oddsWon = x87::chop(static_cast<float>(career.stats.oddsWon) + odds.winnings);
            profile.backedPrizes += prize;
        }
    }
    const int32_t won = x87::chop(m_won);
    if (won > 0) {
        career.stats.moneyReceived += won;
    } else {
        career.stats.moneyPaid -= won;
    }
    profile.money += won;
    if (profile.money < 0) profile.money = 0;
    m_step = m_won * 0.05f;
    m_delay = 3.0f;
    if (profile.raceCarsChosen != 0 || profile.racing != 0) {
        for (int32_t k = 0; k < profile.raceCarCount && k < 3; ++k) {
            const auto index = static_cast<size_t>(profile.raceCars[static_cast<size_t>(k)]);
            if (index < career.garage.size()) {
                Car& car = career.garage[index];
                car.winnings = std::bit_cast<uint32_t>(std::bit_cast<float>(car.winnings) + m_won);
            }
            const auto total = static_cast<int32_t>(profile.prizesWon);
            profile.prizesWon = static_cast<uint32_t>(x87::chop(static_cast<float>(total) + m_won));
        }
    }
    careerCashTexts(s, career, language, texts);
    m_after = m_won + m_shown;
    // With the cheat "win all races" ([career + 0x64] +0x04) the car shown stays the one before.
    if (session.winner != -1 && career.cheatFlags[kCheatWinAllRaces] == 0) {
        std::vector<Car>& cars = session.winnerCrew == 0 ? career.garage : career.opponentCars;
        const auto racer = static_cast<size_t>(session.winner);
        const auto car = racer < session.racers.size() ? static_cast<size_t>(session.racers[racer].car) : SIZE_MAX;
        host.carStatsPurple().setGarage(0);
        g_statsCar = car < cars.size() ? &cars[car] : nullptr;
    }
    if (g_statsCar != nullptr) {
        s.showContaining("SEL2", false);
    } else {
        s.hideContaining("SEL2");
    }
}

// 0x004DAF80: totalWinnings "TOTAL WINNINGS" (171) and the money left to count, cash the money shown; 3 s, then each
// frame 0.05 of the race's money moves from the one to the other (SWOOSH) until none is left (the shown money then
// the money after). SELECT: the event's end (0x004A6870), CLICK, the current round's +0x01 and the
// event's +0x01 set; with a bet placed careerload (the career's +0xA4 3: characterPostRace), else the next window
// (0x004BAEE0). The winning car's PGDN (event 6): car_stats2purple over the window.
void CareerRaceResults::update(CareerHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr || !data->career) return;
    Career& career = *data->career;
    const int language = host.language().index;
    s.setText("totalWinnings",
              texts.at(kTextTotalWinnings) + toWide(" " + moneyText(x87::chop(m_won), language)));
    s.setText("cash", toWide(" " + moneyText(x87::chop(m_shown), language)));
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
    if (event == input::kSelect) {
        CalendarEvent* current = currentEvent(career.calendar);
        if (current != nullptr) endEvent(career, *current, data->tables);
        host.sound("CLICK");
        if (current != nullptr) {
            const_cast<EventRound*>(roundOf(career, *current))->ended = 1;
            current->done = 1;
        }
        if (career.bets.placed != 0) {
            host.request("careerload", SwitchMode::Replace);
            data->careerScreen = CareerScreen::CharacterPostRace;
        } else {
            host.sound("CLICK");
            host.afterRaceResults();
        }
    } else if (event == input::kSelect2 && g_statsCar != nullptr) {
        host.request("car_stats2purple", SwitchMode::Push);
    }
}

}  // namespace oj::frontend
