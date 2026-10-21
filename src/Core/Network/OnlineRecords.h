#pragma once
// The game's online records over the statistics objects (0x004EC590 .. 0x004EE1C0, docs/spec/28-network.md, "The
// statistics"): each user's record ([0x0076A240], a map by the user's number), read from the users' tables and kept
// up with what the game reports (a race begun, finished, a pink slip, a lap time, a showoff score); the players'
// ratings after an online race (0x004EC980 over the table 0x00725630); the table reader the windows and the race's
// world record read the leaderboards with (0x004ED270, [0x0076A250] and online_stats' own).

#include "Core/Network/OnlineStats.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace oj {
struct ExeTables;
}

namespace oj::net::stats {

// The statistics' keys: the leaderboards' columns and the fields the reports set.
enum StatKey : int32_t {
    kKeyNick = 1,
    kKeyRating = 2,
    kKeyRank = 4,
    kKeyLapCar = 6,         // a lap time's car model
    kKeyLapCarRating = 7,   // its performance rating
    kKeyLapRoute = 8,
    kKeyLapTime = 9,        // milliseconds
    kKeyWon = 12,           // the online races won
    kKeyLost = 13,
    kKeyDisconnects = 14,
    kKeyShowoffScore = 15,
    kKeyShowoffCar = 16,
    kKeyShowoffLocation = 17,
    kKeyLapTopSpeed = 26,   // the top speed of the lap's race x 1000
    kKeyCareerRaces = 31,   // a career race begun
    kKeySlipsWon = 33,      // the pink slips won
    kKeySlipsLost = 34,
    kKeyRacing = 36,        // a race begun, not finished
    kKeySlipsMoney = 37,
};

// The statistics' boards (tables): the users' rankings and pink slips, a location's showoff scores (+ the location)
// and a route's lap times (+ the route).
enum StatBoard : int32_t {
    kBoardRankings = 1,
    kBoardPinkSlips = 4,
    kBoardShowoff = 10,
    kBoardLapTimes = 20,
};

// A user's record (0x2C bytes). The keys are the reporter's and the users' tables'.
struct UserRecord {
    int32_t rating = 10000;    // +0x00: key 2
    int32_t rank = 0;          // +0x04: key 4
    int32_t won = 0;           // +0x08: key 12, the online races won
    int32_t lost = 0;          // +0x0C: key 13
    int32_t disconnects = 0;   // +0x10: key 14
    int32_t slipsWon = 0;      // +0x14: key 33, the pink slips won
    int32_t slipsLost = 0;     // +0x18: key 34
    int32_t slipsMoney = 0;    // +0x1C: key 37
    int32_t racingRead = 0;    // +0x20: key 36 as the table had it (a race begun, not finished)
    int32_t racing = -1;       // +0x24: key 36 as we sent it (-1 not yet)
    int32_t asked = 0;         // +0x28: its table asked for
};

// The table reader (0x50 bytes, 0x004ED270): the leaderboards' columns of a window's table.
class TableReader {
public:
    TableReader();  // 0x004ED270
    // 0x004ED320: the ranks, 64-bit values and names emptied, the columns none; not read; the best time -1; the
    // statistics' problem 0.
    void reset();
    // 0x004ED490: a route's record: a showoff route (type 3) the board of its location + 10 (key 15), else the
    // route's + 20 (key 9), one row; false without the leaderboards ("** StatsRetriever system is NULL!!!") or while
    // a table is pending.
    bool bestTime(int32_t route, Leaderboards::Callback success, void* successArg, Leaderboards::Callback failure,
                  void* failureArg);
    // 0x004ED590: the record read: none 0; a showoff's points as they are, else the time's milliseconds in seconds
    // (none: 300); false while not read.
    bool bestTimeRead(float& out) const;
    // The tables' asks return the leaderboards' query (false while one is pending) and request.
    // 0x004ED630 / 0x004ED6F0: a route's lap times (board route + 20: ranks 4, names 1, times 9, cars 6, their
    // ratings 7, the top speeds 26).
    bool lapTimes(int32_t rows, int32_t route, int32_t user);
    bool lapTimeRow(uint32_t row, const char16_t*& name, int32_t& rank, float& time, int32_t& car, int32_t& carRating,
                    float& topSpeed, int32_t& total) const;
    // 0x004ED7B0 / 0x004ED850: a location's showoff scores (board location + 10: ranks, names, points 15, cars 16).
    bool showoff(int32_t rows, int32_t location, int32_t user);
    bool showoffRow(uint32_t row, int32_t& rank, const char16_t*& name, int32_t& points, int32_t& car,
                    int32_t& total) const;
    // 0x004ED8D0 / 0x004ED980: the pink slips (board 4: ranks, names, won 33, lost 34, money 37).
    bool pinkSlips(int32_t rows, int32_t user);
    bool pinkSlipsRow(uint32_t row, int32_t& rank, const char16_t*& name, int32_t& won, int32_t& lost,
                      int32_t& money, int32_t& total) const;
    // 0x004EDA10 / 0x004EDAC0: the world ranking (board 1: ranks, ratings 2 as 64-bit, names, won 12, lost 13,
    // disconnects 14).
    bool worldRanking(int32_t rows, int32_t user);
    bool worldRankingRow(uint32_t row, const char16_t*& name, int32_t& rank, int32_t& rating, int32_t& won,
                         int32_t& lost, int32_t& disconnects, int32_t& total) const;

    std::vector<int32_t> ranks;                 // +0x00
    std::vector<int64_t> wide;                  // +0x10
    std::vector<const char16_t*> names;         // +0x20
    std::vector<std::vector<int32_t>> columns;  // +0x30
    int32_t route = 0;                          // +0x40
    float best = -1.0f;                         // +0x44
    int32_t total = 0;                          // +0x48
    int32_t done = 0;                           // +0x4C

private:
    bool rowThere(uint32_t row) const;
};

// The routes' table (0x00711A3C) the records read a route's type and location in; the game's.
void setRouteTable(const ExeTables* tables);
// 0x00426570: the location of the first route of the track (case aside); 0.
int32_t trackLocation(const std::string& track);
// [0x0076A240]: the records by user.
std::map<uint32_t, UserRecord>& records();
// [0x0076A250]: the reader of the records' tables and the race's world record (made at the start, 0x006BFF13).
TableReader& recordsReader();
// [0x0075F030]: the game's statistics (the object made at the login).
Statistics* gameStatistics();

// 0x004EC5B0, each frame (0x0040CF87): logged on, the statistics made; once each local player's user's record made;
// the statistics' frame; every 5 s with a front end ([G+0x38] or [G+0x3C]) the records' tables asked again.
void frame(float dt, bool frontEnd);
// 0x004EC590: the statistics let go, the records' users let go and the records emptied.
void closeStatistics();

// 0x004EDFD0: the record of the user (by the same profile); none.
UserRecord* record(int32_t user);
// 0x004EDF20: the user's record made (10000, the rest 0, racing -1) and the user referenced; one already there asked
// again (none returned).
UserRecord* makeRecord(int32_t user);
// 0x004EE050: the user's record taken out (the user let go with it).
void releaseRecord(int32_t user);
// 0x004EDDF0: the records' users let go and the records emptied.
void releaseRecords();
// 0x004EDB50: up to six records not asked for asked for in one users' table (board 1 and 4 bindings, 0x004ED3B0;
// one of no local player's not its key 36), done with 0x004EE120; their number.
uint32_t refreshRecords();
// 0x004EDD10: each record asked for whose table had it racing while we had it racing (or unknown): a disconnect and
// a race lost more, the rating 20 less (not below 0), reported (0x004ED200) and sent.
bool penalties();

// 0x004EC980: the rating after a race against another: their difference (own - other, the other way for a sign
// below 0) on the table 0x00725630; a sign above 0 takes its step off (not below 0), else adds it.
int32_t ratingAfter(int32_t sign, uint32_t own, int32_t other);

// The reports (each begun and ended on the reporter; true when every key took).
// 0x004EC6D0: a lap time (the route's board + 20; keys 6 the car, 7 its rating, 9 the time in ms, 26 the race's top
// speed x 1000, 8 the route).
bool reportLapTime(int32_t user, int32_t route, float time, int32_t car, int32_t carRating, float topSpeed);
// 0x004EC810: a showoff score of 1 .. 20000000 (the location's board + 10; keys 15 the score, 16, 17 the location).
bool reportShowoff(int32_t user, int32_t location, int32_t score, int32_t car);
// 0x004EC8C0: a race begun (key 36 1; the record racing).
bool reportRaceBegun(int32_t user);
// 0x004EC910: an online career's race begun (board 4 key 31 1), then as 0x004EC8C0.
bool reportCareerRaceBegun(int32_t user);
// 0x004EC9F0: "FinishedRace": key 36 0; won (place 1 and finished) or lost; the ratings against each other racer in
// the places' order (a step each by the places between; one who left without finishing a loss for it, we leaving
// a loss for us), ours over 9999999 made 10000.
bool finishedRace(int32_t user, int32_t place);
// 0x004ECD70: the same for our first player by its race's place (ours over 999999 made 999999).
bool raceFinished();
// 0x004ED090: a pink slip (board 4: won 33 with its money 37 under 999999, or lost 34), then 0x004EC9F0 with place
// 1 or 2.
void reportPinkSlip(int32_t user, bool won, uint32_t money);
// 0x004ED120: our first player's race left (key 36 0).
bool reportRaceLeft();
// 0x004ED200: a penalty's keys (36 0, 14, 13, 2).
bool reportPenalty(int32_t user, int32_t disconnects, int32_t lost, int32_t rating);

// 0x004EE0C0: the world record of a route: the one read when it is this route's and not `again`, else asked for
// (0x004ED490 with 0x004EE120 and 0x004EE160) and -1.
float worldBest(int32_t route, bool again);
// 0x00451894 (the showoff's results): with the statistics, the route's world record read raised to the score when it
// is below; another route's asked for (-1 until read).
void showoffResult(int32_t route, float score);
// [0x0075F038], [0x007191D8], [0x0075F03C]: the route asked for, its record (-1 not read), the tries.
int32_t& worldBestRoute();
float& worldBestTime();
int32_t& worldBestTries();

}  // namespace oj::net::stats
