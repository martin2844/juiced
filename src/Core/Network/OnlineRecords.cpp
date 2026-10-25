#include "Core/Network/OnlineRecords.h"

#include "Core/Data/ExeTables.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Base/Log.h"
#include "Core/Network/NetSession.h"

#include <array>
#include <cstring>

namespace oj::net::stats {

namespace {

struct State {
    std::map<uint32_t, UserRecord> records;  // [0x0076A240]
    TableReader reader;                      // [0x0076A250]
    bool localRecords = false;               // [0x0075F034]
    float refreshTimer = 5.0f;               // [0x007191D0]
    int32_t refreshDone = 0;                 // [0x007191D4]
    int32_t bestRoute = 0;                   // [0x0075F038]
    float bestTime = 0.0f;                   // [0x007191D8]
    int32_t bestTries = 0;                   // [0x0075F03C]
    int32_t notLocal = 0;                    // [0x00728D80]: the key 36 of a record of no local player
    const ExeTables* tables = nullptr;
};

State& st() {
    static State s;
    return s;
}

// 0x0055BA50's part of the user object (its base): the local player whose user has the same profile, referenced;
// -1. Each local player's user looked at let go again in the fixed build (the stock's 0x0055BA81 kept it referenced).
int32_t localPlayerOf(OnlineUser& users, int32_t user) {
    for (int32_t player : users.localPlayers) {
        const int32_t its = users.playerUser(player);
        const bool same = users.sameUser(user, its);
        users.userRelease(its);
        if (same) {
            users.playerAddRef(player);
            return player;
        }
    }
    return -1;
}

// The routes' table: a route of the showoff (type 3), its track's location (0x00426570: the first route of the same
// track's).
const ExeTables::Route* route(int32_t index) {
    const ExeTables* t = st().tables;
    if (t == nullptr || index < 0 || static_cast<size_t>(index) >= t->routes.size()) return nullptr;
    return &t->routes[static_cast<size_t>(index)];
}

bool showoffRoute(int32_t index) {
    const ExeTables::Route* r = index < 0x6D ? route(index) : nullptr;
    return r != nullptr && r->type == RouteType::Cruise;
}

int32_t locationOf(int32_t index) {
    const ExeTables::Route* r = route(index);
    return r != nullptr ? trackLocation(r->track) : 0;
}

std::vector<NetPlayer> noPlayers;

std::vector<NetPlayer>& sessionPlayers() {
    NetGame* game = netGame();
    return game != nullptr ? game->session.players() : noPlayers;
}

std::vector<uint32_t> noOrder;

const std::vector<uint32_t>& sessionOrder() {
    NetGame* game = netGame();
    return game != nullptr ? game->session.order() : noOrder;
}

uint32_t racing() {
    NetGame* game = netGame();
    return game != nullptr ? game->session.racing() : 0;
}

const char16_t* nickOf(int32_t user) {
    OnlineUser* users = onlineUser();
    return users != nullptr ? users->userNick(user).c_str() : u"Guest";
}

// The time in milliseconds made whole as the stock's conversions do (cut toward zero).
int32_t milliseconds(float seconds) {
    return static_cast<int32_t>(seconds * 1000.0f);
}

// The first of our players with a user (the session's players).
size_t ownPlayer(const std::vector<NetPlayer>& players) {
    size_t i = 0;
    while (i < players.size() && !(players[i].peer == -1 && players[i].profile != -1)) ++i;
    return i;
}

// 0x004EE120: a world record read (its argument the record's float), else the records' penalties.
void tableRead(void* arg) {
    State& s = st();
    if (arg == &s.bestTime) {
        s.reader.bestTimeRead(s.bestTime);
        log::debugPrint("** STATS.CPP: got world record: %f\n", static_cast<double>(s.bestTime));
        return;
    }
    penalties();
}

// 0x004EE160: the world record not read: asked again, six tries in all.
void bestTimeFailed(void* arg) {
    State& s = st();
    if (++s.bestTries >= 6) return;
    if (Leaderboards* lb = leaderboards()) lb->reset();
    const auto routeIndex = static_cast<int32_t>(reinterpret_cast<intptr_t>(arg));
    s.reader.bestTime(routeIndex, tableRead, &s.bestTime, bestTimeFailed, arg);
    s.bestRoute = routeIndex;
    s.bestTime = -1.0f;
}

// 0x004ED3B0: a user's row of the users' table (board 1 its rating 2, won 12, lost 13, disconnects 14, racing 36,
// rank 4; board 4 the pink slips 33, 34, 37).
bool usersTable(TableReader& reader, int32_t user, int32_t* rating, int32_t* won, int32_t* lost,
                int32_t* disconnects, int32_t* racingRead, int32_t* slipsWon, int32_t* slipsLost,
                int32_t* slipsMoney, int32_t* rank) {
    reader.reset();
    if (leaderboards() == nullptr) return false;  // the stock's call through none
    Leaderboards& lb = *leaderboards();
    bool ok = lb.userStats(kBoardRankings, user, false);
    ok = lb.bindRowInt32(kKeyRating, rating) && ok;
    ok = lb.bindRowInt32(kKeyWon, won) && ok;
    ok = lb.bindRowInt32(kKeyLost, lost) && ok;
    ok = lb.bindRowInt32(kKeyDisconnects, disconnects) && ok;
    ok = lb.bindRowInt32(kKeyRacing, racingRead) && ok;
    ok = lb.bindRowInt32(kKeyRank, rank) && ok;
    ok = lb.endQuery() && ok;
    ok = lb.userStats(kBoardPinkSlips, user, false) && ok;
    ok = lb.bindRowInt32(kKeySlipsWon, slipsWon) && ok;
    ok = lb.bindRowInt32(kKeySlipsLost, slipsLost) && ok;
    ok = lb.bindRowInt32(kKeySlipsMoney, slipsMoney) && ok;
    return lb.endQuery() && ok;
}

// The ratings against the race's other players in the places' order (0x004EC9F0's and 0x004ECD70's): each racer
// of another machine with a user and a rating; the sign by the places between (one who left without finishing: the
// racers for us, we leaving: the racers against us). Our new rating returned, theirs set.
int32_t ratingsAgainst(const PlayerRace& ours, int32_t own, int32_t step, bool placeTwoWins, int32_t limit,
                       int32_t over) {
    std::vector<NetPlayer>& players = sessionPlayers();
    const std::vector<uint32_t>& order = sessionOrder();
    int32_t rating = own;
    for (size_t k = 0; k < order.size(); ++k, --step) {
        const uint32_t index = order[k];
        if (index >= players.size()) continue;
        NetPlayer& other = players[index];
        if (other.peer == -1 || other.profile == -1 || other.race.notRacing != 0) continue;
        UserRecord* theirs = record(other.profile);
        if (theirs == nullptr || theirs->rating == 0) continue;
        int32_t sign = 0;
        if (ours.left != 0) {
            if (other.race.left != 0 && other.race.finished == 0) continue;
            sign = static_cast<int32_t>(racing());
        } else if (other.race.left != 0 && other.race.finished == 0) {
            sign = -static_cast<int32_t>(racing());
        } else {
            sign = step;
        }
        if (placeTwoWins && sign == 0) sign = 1;
        // The fixed build: a racer in our own place changes neither rating (0x004EC980 with 0 added its step to both:
        // a gain for each).
        if (sign == 0) continue;
        const int32_t theirsBefore = theirs->rating;
        rating = ratingAfter(sign, static_cast<uint32_t>(rating), theirsBefore);
        if (static_cast<uint32_t>(rating) > static_cast<uint32_t>(limit)) rating = over;
        theirs->rating = ratingAfter(-sign, static_cast<uint32_t>(theirsBefore), own);
    }
    return rating;
}

}  // namespace

void setRouteTable(const ExeTables* tables) {
    st().tables = tables;
}

int32_t trackLocation(const std::string& track) {
    if (st().tables == nullptr) return 0;
    for (const ExeTables::Route& r : st().tables->routes) {
        if (_stricmp(r.track.c_str(), track.c_str()) == 0) return r.location;
    }
    return 0;
}

std::map<uint32_t, UserRecord>& records() {
    return st().records;
}

TableReader& recordsReader() {
    return st().reader;
}

Statistics* gameStatistics() {
    return statistics();
}

int32_t& worldBestRoute() {
    return st().bestRoute;
}

float& worldBestTime() {
    return st().bestTime;
}

int32_t& worldBestTries() {
    return st().bestTries;
}

// ---- the table reader

TableReader::TableReader() {
    reset();
}

void TableReader::reset() {
    ranks.clear();
    names.clear();
    columns.clear();
    wide.clear();
    done = 0;
    best = -1.0f;
    problem() = 0;
}

bool TableReader::rowThere(uint32_t row, size_t read) const {
    if (done == 0 || columns.size() < read || row >= ranks.size() || row >= names.size()) return false;
    for (size_t c = 0; c < read; ++c) {
        if (row >= columns[c].size()) return false;
    }
    return read != 0;
}

bool TableReader::bestTime(int32_t routeIndex, Leaderboards::Callback success, void* successArg,
                           Leaderboards::Callback failure, void* failureArg) {
    reset();
    best = -1.0f;
    columns.resize(1);
    Leaderboards* lb = leaderboards();
    if (lb == nullptr) {
        log::debugPrint("** StatsRetriever system is NULL!!!\n");
        return false;
    }
    if (showoffRoute(routeIndex)) {
        if (!lb->query(locationOf(routeIndex) + kBoardShowoff, 1, -1, nullptr)) {
            log::debugPrint("Stats are already being retrieved! Must wait - however we're going into a race!");
            return false;
        }
        lb->bindInt32s(kKeyShowoffScore, &columns[0]);
        lb->endQuery();
    } else {
        if (!lb->query(routeIndex + kBoardLapTimes, 1, -1, nullptr)) return false;
        lb->bindInt32s(kKeyLapTime, &columns[0]);
        lb->endQuery();
    }
    route = routeIndex;
    return lb->request(&done, success, successArg, failure, failureArg);
}

bool TableReader::bestTimeRead(float& out) const {
    if (done == 0) return false;
    const std::vector<int32_t>& column = columns[0];
    if (column.empty()) {
        out = 0.0f;
        return true;
    }
    const auto value = static_cast<float>(static_cast<uint32_t>(column[0]));
    out = showoffRoute(route) ? value : value * 0.001f;
    return true;
}

bool TableReader::lapTimes(int32_t rows, int32_t routeIndex, int32_t user) {
    reset();
    columns.resize(5);
    if (leaderboards() == nullptr) return false;  // the stock's call through none
    Leaderboards& lb = *leaderboards();
    if (!lb.query(routeIndex + kBoardLapTimes, rows, user, &total)) return false;
    lb.bindInt32s(kKeyRank, &ranks);
    lb.bindNames(kKeyNick, &names);
    lb.bindInt32s(kKeyLapTime, &columns[0]);
    lb.bindInt32s(kKeyLapCar, &columns[1]);
    lb.bindInt32s(kKeyLapCarRating, &columns[2]);
    lb.bindInt32s(kKeyLapTopSpeed, &columns[3]);
    lb.endQuery();
    return lb.request(&done, nullptr, nullptr, nullptr, nullptr);
}

bool TableReader::lapTimeRow(uint32_t row, const char16_t*& name, int32_t& rank, float& time, int32_t& car,
                             int32_t& carRating, float& topSpeed, int32_t& totalRows) const {
    if (done == 0) return false;
    if (!rowThere(row, 4)) return false;
    name = names[row];
    rank = ranks[row];
    time = static_cast<float>(static_cast<uint32_t>(columns[0][row])) * 0.001f;
    car = columns[1][row];
    carRating = columns[2][row];
    topSpeed = static_cast<float>(static_cast<uint32_t>(columns[3][row])) * 0.001f;
    totalRows = total;
    return true;
}

bool TableReader::showoff(int32_t rows, int32_t location, int32_t user) {
    reset();
    columns.resize(3);
    if (leaderboards() == nullptr) return false;  // the stock's call through none
    Leaderboards& lb = *leaderboards();
    if (!lb.query(location + kBoardShowoff, rows, user, &total)) return false;
    lb.bindInt32s(kKeyRank, &ranks);
    lb.bindNames(kKeyNick, &names);
    lb.bindInt32s(kKeyShowoffScore, &columns[0]);
    lb.bindInt32s(kKeyShowoffCar, &columns[1]);
    lb.endQuery();
    return lb.request(&done, nullptr, nullptr, nullptr, nullptr);
}

bool TableReader::showoffRow(uint32_t row, int32_t& rank, const char16_t*& name, int32_t& points, int32_t& car,
                             int32_t& totalRows) const {
    if (done == 0) return false;
    if (!rowThere(row, 2)) return false;
    rank = ranks[row];
    name = names[row];
    points = columns[0][row];
    car = columns[1][row];
    totalRows = total;
    return true;
}

bool TableReader::pinkSlips(int32_t rows, int32_t user) {
    reset();
    columns.resize(3);
    if (leaderboards() == nullptr) return false;  // the stock's call through none
    Leaderboards& lb = *leaderboards();
    if (!lb.query(kBoardPinkSlips, rows, user, &total)) return false;
    lb.bindInt32s(kKeyRank, &ranks);
    lb.bindNames(kKeyNick, &names);
    lb.bindInt32s(kKeySlipsWon, &columns[0]);
    lb.bindInt32s(kKeySlipsLost, &columns[1]);
    lb.bindInt32s(kKeySlipsMoney, &columns[2]);
    lb.endQuery();
    return lb.request(&done, nullptr, nullptr, nullptr, nullptr);
}

bool TableReader::pinkSlipsRow(uint32_t row, int32_t& rank, const char16_t*& name, int32_t& won, int32_t& lost,
                               int32_t& money, int32_t& totalRows) const {
    if (done == 0) return false;
    if (!rowThere(row, 3)) return false;
    rank = ranks[row];
    name = names[row];
    won = columns[0][row];
    lost = columns[1][row];
    money = columns[2][row];
    totalRows = total;
    return true;
}

bool TableReader::worldRanking(int32_t rows, int32_t user) {
    reset();
    columns.resize(3);
    if (leaderboards() == nullptr) return false;  // the stock's call through none
    Leaderboards& lb = *leaderboards();
    if (!lb.query(kBoardRankings, rows, user, &total)) return false;
    lb.bindInt32s(kKeyRank, &ranks);
    lb.bindInt64s(kKeyRating, &wide);
    lb.bindNames(kKeyNick, &names);
    lb.bindInt32s(kKeyWon, &columns[0]);
    lb.bindInt32s(kKeyLost, &columns[1]);
    lb.bindInt32s(kKeyDisconnects, &columns[2]);
    lb.endQuery();
    return lb.request(&done, nullptr, nullptr, nullptr, nullptr);
}

bool TableReader::worldRankingRow(uint32_t row, const char16_t*& name, int32_t& rank, int32_t& rating, int32_t& won,
                                  int32_t& lost, int32_t& disconnects, int32_t& totalRows) const {
    if (done == 0) return false;
    if (!rowThere(row, 3) || row >= wide.size()) return false;
    name = names[row];
    rank = ranks[row];
    rating = static_cast<int32_t>(wide[row]);  // the 64-bit value's low half
    won = columns[0][row];
    lost = columns[1][row];
    disconnects = columns[2][row];
    totalRows = total;
    return true;
}

// ---- the records

UserRecord* record(int32_t user) {
    OnlineUser* users = onlineUser();
    if (users == nullptr) return nullptr;  // the stock's call through none
    for (auto& [key, value] : st().records) {
        if (users->sameUser(static_cast<int32_t>(key), user)) return &value;
    }
    return nullptr;
}

UserRecord* makeRecord(int32_t user) {
    if (UserRecord* there = record(user)) {
        there->asked = 0;
        return nullptr;
    }
    const auto [at, made] = st().records.emplace(static_cast<uint32_t>(user), UserRecord{});
    if (!made) return nullptr;
    if (OnlineUser* users = onlineUser()) users->userAddRef(user);
    return &at->second;
}

void releaseRecord(int32_t user) {
    const size_t erased = st().records.erase(static_cast<uint32_t>(user));
    if (erased == 1) {
        if (OnlineUser* users = onlineUser()) users->userRelease(user);
    }
}

void releaseRecords() {
    State& s = st();
    if (OnlineUser* users = onlineUser()) {
        for (const auto& [key, value] : s.records) users->userRelease(static_cast<int32_t>(key));
    }
    s.records.clear();
}

uint32_t refreshRecords() {
    State& s = st();
    OnlineUser* users = onlineUser();
    Leaderboards* lb = leaderboards();
    if (users == nullptr || lb == nullptr || lb->pending()) return 0;
    uint32_t asked = 0;
    for (auto& [key, r] : s.records) {
        if (r.asked != 0 || asked >= 6) continue;
        if (asked == 0) {
            if (Leaderboards* again = leaderboards()) again->reset();
        }
        ++asked;
        const int32_t user = static_cast<int32_t>(key);
        const int32_t player = localPlayerOf(*users, user);
        if (player != -1) {
            if (!usersTable(s.reader, user, &r.rating, &r.won, &r.lost, &r.disconnects, &r.racingRead, &r.slipsWon,
                            &r.slipsLost, &r.slipsMoney, &r.rank)) {
                users->playerRelease(player);
                return 0;
            }
        } else {
            r.racingRead = 0;
            if (!usersTable(s.reader, user, &r.rating, &r.won, &r.lost, &r.disconnects, &s.notLocal, &r.slipsWon,
                            &r.slipsLost, &r.slipsMoney, &r.rank)) {
                users->playerRelease(-1);
                return 0;
            }
        }
        users->playerRelease(player);
        r.asked = 1;
    }
    if (asked != 0) {
        s.refreshDone = 0;
        if (Leaderboards* again = leaderboards()) again->request(&s.refreshDone, tableRead, nullptr, nullptr, nullptr);
    }
    return asked;
}

bool penalties() {
    Reporter& reporter = *stats::reporter();
    bool any = false;
    for (auto& [key, r] : st().records) {
        if (r.asked == 0 || r.racingRead != 1) continue;
        r.racingRead = 0;
        if (r.racing == 0) continue;
        ++r.disconnects;
        ++r.lost;
        r.racing = 0;
        r.rating = static_cast<uint32_t>(r.rating) > 20 ? r.rating - 20 : 0;
        if (!any) reporter.reset();
        any = true;
        log::debugPrint("***\n*** StartedRace==1: updating disconnects value (%d)\n***\n", r.disconnects);
        reportPenalty(static_cast<int32_t>(key), r.disconnects, r.lost, r.rating);
    }
    if (!any) return false;
    return reporter.send();
}

int32_t ratingAfter(int32_t sign, uint32_t own, int32_t other) {
    // 0x00725630: the difference's bounds and their steps.
    static constexpr std::array<std::array<int32_t, 2>, 10> kSteps = {{{-3200, 0},
                                                                       {-1600, 1},
                                                                       {-800, 2},
                                                                       {-400, 3},
                                                                       {-150, 4},
                                                                       {150, 5},
                                                                       {400, 6},
                                                                       {800, 7},
                                                                       {1600, 8},
                                                                       {3200, 9}}};
    int32_t difference = static_cast<int32_t>(own) - other;
    if (sign < 0) difference = -difference;
    uint32_t step = 10;
    for (const auto& [bound, value] : kSteps) {
        if (difference < bound) {
            step = static_cast<uint32_t>(value);
            break;
        }
    }
    if (sign > 0) return own > step ? static_cast<int32_t>(own - step) : 0;
    return static_cast<int32_t>(own + step);
}

// ---- the reports

bool reportLapTime(int32_t user, int32_t routeIndex, float time, int32_t car, int32_t carRating, float topSpeed) {
    Reporter& reporter = *stats::reporter();
    const int32_t ms = milliseconds(time);
    const int32_t topSpeedThousandths = milliseconds(topSpeed);
    reporter.beginWith(user, routeIndex + kBoardLapTimes, ~ms, 0);
    bool ok = reporter.setText(kKeyNick, nickOf(user));
    ok = reporter.setInt(kKeyLapCar, car) && ok;
    ok = reporter.setInt(kKeyLapCarRating, carRating) && ok;
    ok = reporter.setInt(kKeyLapTime, ms) && ok;
    ok = reporter.setInt(kKeyLapTopSpeed, topSpeedThousandths) && ok;
    ok = reporter.setInt(kKeyLapRoute, routeIndex) && ok;
    reporter.end();
    return ok;
}

bool reportShowoff(int32_t user, int32_t location, int32_t score, int32_t car) {
    const char16_t* nick = nickOf(user);
    if (score == 0 || static_cast<uint32_t>(score) > 20000000u) return true;
    Reporter& reporter = *stats::reporter();
    reporter.beginWith(user, location + kBoardShowoff, score, 0);
    bool ok = reporter.setText(kKeyNick, nick);
    ok = reporter.setInt(kKeyShowoffScore, score) && ok;
    ok = reporter.setInt(kKeyShowoffCar, car) && ok;
    ok = reporter.setInt(kKeyShowoffLocation, location) && ok;
    reporter.end();
    return ok;
}

bool reportRaceBegun(int32_t user) {
    if (UserRecord* r = record(user)) r->racing = 1;
    Reporter& reporter = *stats::reporter();
    reporter.begin(user, kBoardRankings);
    const bool ok = reporter.setInt(kKeyRacing, 1);
    reporter.end();
    return ok;
}

bool reportCareerRaceBegun(int32_t user) {
    Reporter& reporter = *stats::reporter();
    reporter.begin(user, kBoardPinkSlips);
    reporter.setInt(kKeyCareerRaces, 1);
    reporter.end();
    if (UserRecord* r = record(user)) r->racing = 1;
    reporter.begin(user, kBoardRankings);
    const bool ok = reporter.setInt(kKeyRacing, 1);
    reporter.end();
    return ok;
}

bool finishedRace(int32_t user, int32_t place) {
    log::debugPrint("\n*** Called FinishedRace with paramaters %d and %d\n\n", user, place);
    const char16_t* nick = nickOf(user);
    // Our race: the first player of the user (the stock's 0x004ECA82 referenced each player's user looked at and kept
    // it; not the fixed build).
    std::vector<NetPlayer>& players = sessionPlayers();
    const PlayerRace* ours = nullptr;
    for (size_t i = 0; i < players.size() && ours == nullptr; ++i) {
        if (players[i].profile == user) ours = &players[i].race;
    }
    Reporter& reporter = *stats::reporter();
    reporter.begin(user, kBoardRankings);
    bool ok = reporter.setText(kKeyNick, nick);
    ok = reporter.setInt(kKeyRacing, 0) && ok;
    if (UserRecord* r = record(user)) r->racing = 0;
    UserRecord* own = record(user);
    if (own != nullptr) {
        // Stock problem: without our player among the session's (none) the first place reads at 0x10.
        if (place == 1 && ours != nullptr && ours->finished != 0) {
            ++own->won;
            ok = reporter.setInt(kKeyWon, own->won) && ok;
        } else {
            ++own->lost;
            ok = reporter.setInt(kKeyLost, own->lost) && ok;
        }
        const int32_t before = own->rating;
        const PlayerRace none{};
        const int32_t rating = ratingsAgainst(ours != nullptr ? *ours : none, before, place - 1, place == 2, 9999999,
                                              10000);
        log::debugPrint("Got new player rating of %d from %d\n", rating, before);
        if (rating != before) {
            ok = reporter.setInt(kKeyRating, rating) && ok;
            own->rating = rating;
        }
    }
    reporter.end();
    return ok;
}

bool raceFinished() {
    std::vector<NetPlayer>& players = sessionPlayers();
    const size_t index = ownPlayer(players);
    const PlayerRace* ours = index < players.size() ? &players[index].race : nullptr;
    const int32_t user = index < players.size() ? players[index].profile : -1;
    const char16_t* nick = nickOf(user);
    Reporter& reporter = *stats::reporter();
    reporter.begin(user, kBoardRankings);
    bool ok = reporter.setText(kKeyNick, nick);
    ok = reporter.setInt(kKeyRacing, 0) && ok;
    if (UserRecord* r = record(user)) r->racing = 0;
    UserRecord* own = record(user);
    if (own != nullptr) {
        // Stock problem: without our player the race read is at 0.
        const PlayerRace none{};
        const PlayerRace& race = ours != nullptr ? *ours : none;
        const int32_t place = race.place + 1;
        if (place == 1 && race.left == 0) {
            ++own->won;
            ok = reporter.setInt(kKeyWon, own->won) && ok;
        } else {
            ++own->lost;
            ok = reporter.setInt(kKeyLost, own->lost) && ok;
        }
        const int32_t before = own->rating;
        const int32_t rating = ratingsAgainst(race, before, place - 1, false, 999999, 999999);
        log::debugPrint("Got new player rating of %d from %d\n", rating, before);
        if (rating != before) {
            ok = reporter.setInt(kKeyRating, rating) && ok;
            own->rating = rating;
        }
    }
    reporter.end();
    return ok;
}

void reportPinkSlip(int32_t user, bool won, uint32_t money) {
    if (UserRecord* r = record(user)) {
        Reporter& reporter = *stats::reporter();
        reporter.begin(user, kBoardPinkSlips);
        if (won) {
            ++r->slipsWon;
            reporter.setInt(kKeySlipsWon, r->slipsWon);
            if (money != 0 && money < 999999) {
                r->slipsMoney += static_cast<int32_t>(money);
                reporter.setInt(kKeySlipsMoney, r->slipsMoney);
            }
        } else {
            ++r->slipsLost;
            reporter.setInt(kKeySlipsLost, r->slipsLost);
        }
        reporter.end();
    }
    finishedRace(user, won ? 1 : 2);
}

bool reportRaceLeft() {
    std::vector<NetPlayer>& players = sessionPlayers();
    const size_t index = ownPlayer(players);
    if (UserRecord* r = record(index < players.size() ? players[index].profile : -1)) r->racing = 0;
    const int32_t user = index < players.size() ? players[index].profile : -1;
    Reporter& reporter = *stats::reporter();
    reporter.begin(user, kBoardRankings);
    const bool ok = reporter.setInt(kKeyRacing, 0);
    reporter.end();
    return ok;
}

bool reportPenalty(int32_t user, int32_t disconnects, int32_t lost, int32_t rating) {
    Reporter& reporter = *stats::reporter();
    reporter.begin(user, kBoardRankings);
    bool ok = reporter.setInt(kKeyRacing, 0);
    ok = reporter.setInt(kKeyDisconnects, disconnects) && ok;
    ok = reporter.setInt(kKeyLost, lost) && ok;
    ok = reporter.setInt(kKeyRating, rating) && ok;
    reporter.end();
    return ok;
}

float worldBest(int32_t routeIndex, bool again) {
    State& s = st();
    if (routeIndex == s.bestRoute && !again) return s.bestTime;
    if (Leaderboards* lb = leaderboards()) lb->reset();
    s.reader.bestTime(routeIndex, tableRead, &s.bestTime, bestTimeFailed,
                      reinterpret_cast<void*>(static_cast<intptr_t>(routeIndex)));
    s.bestRoute = routeIndex;
    s.bestTime = -1.0f;
    return -1.0f;
}

void showoffResult(int32_t routeIndex, float score) {
    State& s = st();
    if (gameStatistics() == nullptr) return;
    if (routeIndex == s.bestRoute) {
        if (s.bestTime >= 0.0f && s.bestTime < score) s.bestTime = score;
        return;
    }
    if (Leaderboards* lb = leaderboards()) lb->reset();
    s.reader.bestTime(routeIndex, tableRead, &s.bestTime, bestTimeFailed,
                      reinterpret_cast<void*>(static_cast<intptr_t>(routeIndex)));
    s.bestTime = -1.0f;
    s.bestRoute = routeIndex;
}

// ---- the frame

void frame(float dt, bool frontEnd) {
    State& s = st();
    if (statistics() == nullptr) {
        s.refreshTimer = 5.0f;
        OnlineUser* users = onlineUser();
        // The fixed build's guest has no statistics (no GameSpy profile to report for).
        if (users == nullptr || users->loggedOn == 0 || users->guest()) return;
        if (!startStatistics()) return;
    }
    if (!s.localRecords) {
        OnlineUser& users = *onlineUser();
        for (size_t i = 0; i < users.localPlayers.size(); ++i) {
            const int32_t user = users.playerUser(users.localPlayers[i]);
            makeRecord(user);
            users.userRelease(user);
        }
        s.localRecords = true;
    }
    statistics()->frame();
    if (s.refreshTimer > 0.0f) s.refreshTimer -= dt;
    if (s.refreshTimer <= 0.0f && frontEnd) {
        refreshRecords();
        s.refreshTimer = 5.0f;
    }
}

void closeStatistics() {
    stopStatistics();
    st().localRecords = false;
    releaseRecords();
}

}  // namespace oj::net::stats
