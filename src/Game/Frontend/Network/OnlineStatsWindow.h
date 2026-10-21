#pragma once
// online_stats: the leaderboards.

#include "Core/Network/OnlineRecords.h"
#include "Game/Frontend/WindowHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <memory>

namespace oj::frontend {

// The table online_stats shows (its +0x2C; M_CATEGORY's items laptimes, worldranking, topcruisescore, pinkslips).
enum class StatsCategory : int32_t { None = 0, LapTimes = 1, WorldRanking = 2, TopCruiseScore = 3, PinkSlips = 4 };

// The window of the record of online_stats (0x0047E3F0's +0x0C ..).
class OnlineStats {
public:
    // 0x0047E3F0
    void enter(WindowHost& host, ui::Screen& s);
    // 0x0047E6B0
    void leave();
    // 0x0047F720
    void update(WindowHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x0047E6E0: the columns and M_RANK emptied, M_RANK's items cut to its box (+0x16C).
    void clearTable(ui::Screen& s);
    // 0x0047E760: H_RANK "RANK"; the headers of the category: none; lap times NICKNAME, LAP TIME, CAR (M_COL3 left);
    // world ranking NICKNAME, SCORE, WINS, RACES (centred); top cruise score NICKNAME, SCORE, CAR and H_COL4 "" (left);
    // pink slips NICKNAME, WINS, RACES, WINNINGS (centred).
    void headers(WindowHost& host, ui::Screen& s, StatsCategory category);
    // 0x0047E940: the headers each frame; a table read not shown yet: the columns emptied, M_COL1, M_COL2 and M_RANK
    // left, M_COL3's items cut to its box; each row read (the reader's; ours found by our first local player's nick
    // highlighted, else the sixth) its name (fitted to M_COL1's width less 10) and by the category: lap times (with a
    // car) the time and the car's name; world ranking the score, wins and races; top cruise score (with a car) the
    // points with their thousands marked and the car's name; pink slips the wins, races and "$" winnings; the rank.
    void fill(WindowHost& host, ui::Screen& s, StatsCategory category);
    // 0x0047F340: the category of M_CATEGORY's highlighted item (laptimes 1, worldranking 2, topcruisescore 3,
    // pinkslips 4, else 0); a new one: not shown, kept, the reader reset (its total 0) and the columns emptied.
    void options(WindowHost& host, ui::Screen& s);
    // 0x0047F590: not busy and the second between asks over: the table asked for (the next ask the other way: our
    // position, 20 rows of our user, or the top, 20 rows): lap times of the route highlighted (none for 0), the world
    // ranking, the top cruise score of the location highlighted, the pink slips; asked: not shown, busy.
    bool ask(ui::Screen& s);
    // 0x0047F6C0: SELECT: the top cruise score's categories go on to the locations, the lap times' to the locations and
    // the routes; else the table asked for.
    bool select(ui::Screen& s);

    std::unique_ptr<net::stats::TableReader> m_reader;  // +0x0C (0x004ED270)
    int32_t m_filled = 0;        // +0x10: the table read shown
    int32_t m_busy = 0;          // +0x14: a table asked for
    int32_t m_aroundMe = 1;      // +0x18: the next ask around our player, else the table's top
    int32_t m_unused = -1;       // +0x1C: -1 by the enter, read by nothing
    int32_t m_level = 0;         // +0x20: the menu chosen in: 0 M_CATEGORY, 1 M_OPTION1, 2 M_OPTION2
    int32_t m_levelShown = 1;    // +0x24
    int32_t m_user = -1;         // +0x28: our last local player's user
    StatsCategory m_category = StatsCategory::WorldRanking;  // +0x2C
    float m_timer = -1.0f;       // +0x30: a second between asks
    int32_t m_highlighted = -1;  // +0x38: the level's menu's highlight seen
};

}  // namespace oj::frontend
