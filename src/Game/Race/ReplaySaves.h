#pragma once
// The replays' object of the replays' save device (unused on the PC, kept as the stock game has it).

#include "Core/Base/Log.h"
#include "Core/Data/GameData.h"
#include "Core/Race/Racer.h"
#include "Core/Race/Replay.h"
#include "Core/Saves/SaveDevice.h"
#include "Game/Race/RaceGrids.h"
#include "Game/Race/Hud/RaceWindows.h"

#include <string>

namespace app {

// The replays' object of the replays' save device (its +0x0C, given by 0x004FA8B0; Stock problem: nothing on the PC
// gives it one, so the stock game's savereplay and loadreplay would call through address 0 had they a way in): the
// game object's replay. A write's source takes its files (slot 13, 0x004292C0) with the driver's name ([G+0x64]
// slot 7); a read's (slot 14, 0x00429550) gives it its blocks and header, the race of the header (0x004269A0) and
// "menu" as the window after the race ([0x0075ED18]).
struct ReplaySaves : oj::save::SaveReplays {
    oj::Replay* replay = nullptr;
    oj::GameData* data = nullptr;
    bool save(oj::save::SaveSource& source, const std::string& date) override {
        return replay->saveTo(source, date, data->profileName);
    }
    bool take(const oj::save::SaveSource& source) override {
        if (!replay->loadFrom(source)) return false;
        const oj::ReplayHeader& h = replay->header;
        setReplayRace(data->race, h);
        data->afterRaceWindow = "menu";
        oj::log::info("frontend: the saved replay of {} {} read ({} cars, {:.2f} s)", h.track, h.route, h.cars,
                      h.length);
        return true;
    }
};

}  // namespace app
