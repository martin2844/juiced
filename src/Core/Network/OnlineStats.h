#pragma once
// The game's statistics objects over GameSpy's (STATS.CPP, docs/spec/28-network.md, "The statistics"): the statistics
// object ([0x0075F274], vtable 0x006FA440: 0x0055C680 .. 0x0055C960) that starts the gstats connection and owns the
// other two, the leaderboards ([0x0075F280], vtable 0x006FA444: the tables read over ghttp from
// gamestats.gamespy.com, 0x0055C9F0 .. 0x0055D730) and the reporter ([0x0075F28C], vtable 0x006FA97C: each player's
// keys of a game sent to gstats, 0x00564B20 .. 0x00565070).

#include "Core/Network/GameSpy/GameSpy.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace oj::net::stats {

// [0x0075F27C]: the statistics' problem for the windows: 0 none, 3 a table not read (its HTTP failed), 6 the player
// asked for not in the table read.
int32_t& problem();

// The leaderboards (0x304 bytes, 0x0055C9F0).
class Leaderboards {
public:
    // A binding (8 bytes): the type (0 a 32-bit number, 1 a 64-bit one, 2 a name) and where its values go: a
    // column's vector (std::vector<int32_t>, std::vector<int64_t>, std::vector<const char16_t*> of the names read)
    // or a row's value (int32_t, int64_t).
    struct Binding {
        int32_t type = 0;
        void* out = nullptr;
    };
    using Callback = void (*)(void* arg);

    Leaderboards();   // 0x0055C9F0
    ~Leaderboards();  // 0x0055CB90: ghttp cleaned up when it was started
    Leaderboards(const Leaderboards&) = delete;
    Leaderboards& operator=(const Leaderboards&) = delete;

    // +0x04 (0x0055CC00): the columns, names, players and rows' bindings emptied; nothing pending or read, no table;
    // the problem 0.
    void reset();
    // +0x08 (0x0055CE60): a table's query (none while one is pending): reset; with a user (not -1, the user object
    // there) "leaderboard.asp?leaderboard=<n>&pid=<its profile>&rows=<rows>&v=3" around it (the profile and nick kept
    // for the read), else "records.asp?v=2&leaderboard=<n>" for one row, "leaderboard_<n>.html" for more; the total
    // read into `total` (may be none).
    bool query(int32_t leaderboard, int32_t rows, int32_t user, int32_t* total);
    // +0x0C (0x0055D060): the query's bindings ended (false when none was begun).
    bool endQuery();
    // +0x10 (0x0055CC80): a users' table (none while one is pending; `doReset` first): the first table
    // "userstats.asp?v=2&ps=1&lb=<n>" (1) or "userstats.asp?lb=<n>", the six rows emptied; then the user's profile
    // (once) on the players (a row more) while the table is the first's 0, or 1 with 1 or 4 again.
    bool userStats(int32_t leaderboard, int32_t user, bool doReset);
    // +0x14, +0x18, +0x1C (0x0055D220, 0x0055D1B0, 0x0055D150): a column's values (names, 64-bit, 32-bit) while a
    // query is begun; the key not used.
    bool bindNames(int32_t key, std::vector<const char16_t*>* out);
    bool bindInt64s(int32_t key, std::vector<int64_t>* out);
    bool bindInt32s(int32_t key, std::vector<int32_t>* out);
    // +0x20, +0x24 (0x0055D110, 0x0055D0D0): the last player's row's value (64-bit: never read; 32-bit) while a
    // query is begun (0x0055D080: with no row yet the one before the first, Stock problem).
    bool bindRowInt64(int32_t key, int64_t* out);
    bool bindRowInt32(int32_t key, int32_t* out);
    // +0x28 (0x0055D290): the callbacks kept, the players' "&pid<i>=<profile>" added (users' tables), the URL
    // asked for (ghttpGet, not blocking, 0x0055D730); pending, ghttp started, `done` set at the end.
    bool request(int32_t* done, Callback success, void* successArg, Callback failure, void* failureArg);
    // +0x2C (0x0055D390): ghttp's think while pending, else its cleanup once (when started).
    void think();
    bool read() const { return m_read; }        // +0x30 (0x0055C660): +0x48
    bool pending() const { return m_pending; }  // +0x34 (0x0055C670): +0x58

    // 0x0055D3C0: the answer read ('\n' made 0): a table's row count (the names kept that many), its total, then
    // each row's values a line each in the columns' order (names of 1 .. 31 bytes, UTF-8, the player's own found:
    // else the problem 6 when the query named one); a users' table each player's row's 32-bit values. Not pending,
    // read, `done` 1.
    void parse(char* buffer, uint32_t length);

    const std::string& url() const { return m_url; }
    std::string& host() { return m_host; }  // +0x26C: "gamestats.<domain>"

private:
    // 0x0055D730: ghttp's completion: success parsed and the success callback; else not pending, read, `done` 1,
    // the problem 3 and the failure callback. The buffer always freed (1).
    static bool completed(int32_t request, int32_t result, char* buffer, int64_t length, void* param);
    void addRowBinding(const Binding& binding);  // 0x0055D080

    std::vector<std::vector<Binding>> m_rows;       // +0x08: six rows of the users' table
    std::vector<Binding> m_columns;                 // +0x18
    std::vector<int32_t> m_profiles;                // +0x28: the users' table's players
    std::vector<std::array<char16_t, 32>> m_names;  // +0x38: the names read (0x40 bytes each)
    bool m_read = false;                            // +0x48
    bool m_begun = false;                           // +0x4C
    bool m_started = false;                         // +0x50: ghttp started
    bool m_pidsInUrl = true;                        // +0x54: the users' "&pid" added
    bool m_pending = false;                         // +0x58
    int32_t* m_done = nullptr;                      // +0x5C
    int32_t m_players = 0;                          // +0x60: the users' table's rows (0: a table)
    int32_t m_leaderboard = 0;                      // +0x64
    int32_t* m_total = nullptr;                     // +0x68
    std::string m_url;                              // +0x6C
    std::string m_host = std::string("gamestats.") + kServiceDomain;  // +0x26C (the fixed build's on kServiceDomain)
    std::u16string m_nick;                          // +0x2AC: the query's player's
    Callback m_success = nullptr;                   // +0x2EC
    void* m_successArg = nullptr;                   // +0x2F0
    Callback m_failure = nullptr;                   // +0x2F4
    void* m_failureArg = nullptr;                   // +0x2F8
    int32_t m_profile = 0;                          // +0x2FC: the query's player's
};

// The reporter (0x24 bytes, 0x00564BD0).
class Reporter {
public:
    Reporter();   // 0x00564BD0
    ~Reporter();  // 0x00564B20
    Reporter(const Reporter&) = delete;
    Reporter& operator=(const Reporter&) = delete;

    // +0x04 (0x00564C30): the game freed when made, the players emptied (two kept room for), not begun.
    void reset();
    // +0x08 (0x00564CF0): a gstats game made at first (with buckets); the user a player of it once (0x00564DA0);
    // its keys begun, key 3 the value.
    bool begin(int32_t user, int32_t value);
    // +0x0C (0x00564D90).
    bool end();
    // +0x10 (0x00564CD0): +0x08 with the first two (the others not used).
    bool beginWith(int32_t user, int32_t value, int32_t unused1, int32_t unused2);
    // +0x14 (0x00564FC0): key 1 "player", else its number; the text UTF-8 (none: none).
    bool setText(int32_t key, const char16_t* text);
    // +0x18 (0x00564F30): the unsigned 64-bit value as a float.
    bool setFloat(int32_t key, uint64_t value);
    // +0x1C (0x00564EC0): not key 1.
    bool setInt(int32_t key, int32_t value);
    // +0x20 (0x00565070): with a game and the connection: its buckets sent (gstats.dat kept without the
    // connection); the game freed; reset (true) unless there was no game record (5, false).
    bool send();
    void think() {}                     // +0x24
    int32_t unused() const { return 0; }  // +0x28: called by nothing

private:
    void newPlayer(int32_t user);  // 0x00564DA0: the player "" with its "pid" and its "auth"

    bool m_started = false;          // +0x08: the gstats game made
    bool m_begun = false;            // +0x0C
    int32_t m_user = 0;              // +0x10
    std::vector<int32_t> m_users;    // +0x14
};

// The statistics object (8 bytes, 0x0055C760).
class Statistics {
public:
    // 0x0055C760: the game's name and secret key narrowed for gstats, its connection begun (connecting: +0x0075F278
    // 1), the reporter and the leaderboards made.
    Statistics();
    // 0x0055C820: the reporter and leaderboards let go, the gstats connection closed when open.
    ~Statistics();
    Statistics(const Statistics&) = delete;
    Statistics& operator=(const Statistics&) = delete;

    void addRef() { ++m_refs; }
    void release();  // 0x0055C710's part

    // 0x0055C8C0: the leaderboards' think; the connection kept (lost: begun again; connecting: its think, a failure
    // other than waiting begun again with "Failed while processing async stats connection!!! (%d) Retrying..."); the
    // reporter's think.
    void frame();

private:
    int32_t m_refs = 1;
};

// [0x0075F274], [0x0075F280], [0x0075F28C].
Statistics* statistics();
Leaderboards* leaderboards();
Reporter* reporter();
// 0x0055C680: the statistics object made when there is none.
bool startStatistics();
// 0x0055C710: let go.
void stopStatistics();
// [0x0075F278]: the gstats connection being made.
int32_t& connecting();

}  // namespace oj::net::stats
