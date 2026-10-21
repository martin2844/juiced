// The loading phase's windows (arcade_loading, loader, custom_loader; the record 0x00721250): the tips.

#include "Game/Frontend/Loading/Loader.h"

#include "Core/Career/Career.h"
#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

namespace oj::frontend {

// 0x00462840, the enter of the record 0x00721250 (arcade_loading, loader, custom_loader): the tips. The two tip
// counters move (from a random start) when the record's +0x0C is set; TIP_MSG gets the race's tip.
void Loader::enter(WindowHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (data == nullptr) return;
    RaceSetup& race = data->race;
    if (race.mode == RaceMode::Attract) {
        s.hideContaining("TIP");
        return;
    }
    CrtRandom& random = gameRandom();
    if (m_tips) {
        data->careerTip =
            data->careerTip == -1 ? random.next() % 16 : (data->careerTip + 1 > 15 ? 0 : data->careerTip + 1);
        data->generalTip =
            data->generalTip == -1 ? random.next() % 13 : (data->generalTip + 1 > 12 ? 0 : data->generalTip + 1);
        s.showContaining("TIP", false);
    } else {
        s.hideContaining("TIP");
    }
    s.setMultiline("TIP_MSG", true);
    // [0x0075EF8C] and [0x0075EF9C] (a network spectator) are not set in OpenJuice.
    size_t line = 0;
    switch (race.type) {
    case RaceType::TimedLap: line = 76; break;  // complete a lap within the target time
    case RaceType::PerfectLap: line = 74; break;
    case RaceType::Speed: line = 75; break;
    case RaceType::Cruise: {
        const int pick = random.next() % 3;
        line = pick == 1 ? 1878 : pick == 2 ? 1879 : 77;
        break;
    }
    case RaceType::Sprint:
        line = 73;  // manual gears only in sprint mode
        race.gearbox = 0;
        break;
    default: {
        // 0x004BAEB0: a race with teams (the round's +0x17 above 1, not type 9).
        bool teams = false;
        if (race.type != RaceType::TestTrack && data->career) {
            if (CalendarEvent* event = currentEvent(data->career->calendar)) {
                const EventRound* round = roundOf(*data->career, *event);
                teams = round != nullptr && round->teamSize > 1;
            }
        }
        if (race.mode == RaceMode::Career && teams && (random.next() & 1) != 0) {
            line = 1822;  // the winning team
        } else if (race.mode == RaceMode::Career && (random.next() & 1) != 0) {
            line = 1862 + static_cast<size_t>(data->careerTip);
        } else {
            line = 1849 + static_cast<size_t>(data->generalTip);
        }
        break;
    }
    }
    s.setText("TIP_MSG", texts.at(line));
    if (!m_tips) {
        s.setHidden("TRACKPIC", true);
    } else {
        // 0x00462730: the picture of the city place named as the race's track.
        for (const ExeTables::CityLocation& place : data->tables.cityLocations) {
            if (_stricmp(place.name.c_str(), race.track.c_str()) == 0) {
                s.setTexture("TRACKPIC", place.texture);
                break;
            }
        }
        s.setHidden("TRACKPIC", false);
    }
    for (const char* text : {"TEXT1", "TEXT2", "TEXT3", "TEXT4", "TEXT5", "WHITE_BUT", "DPAD_UP"}) {
        s.setMultiline(text, true);
    }
}

}  // namespace oj::frontend
