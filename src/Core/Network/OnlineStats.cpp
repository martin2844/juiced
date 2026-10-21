#include "Core/Network/OnlineStats.h"

#include "Core/Network/GameSpy/GameSpy.h"
#include "Core/Network/GameSpy/GameSpyHttp.h"
#include "Core/Network/GameSpy/GameSpyPresence.h"
#include "Core/Network/GameSpy/GameSpyStats.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Base/Log.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <format>

namespace oj::net::stats {

namespace {

struct Objects {
    std::unique_ptr<Statistics> statistics;      // [0x0075F274]
    std::unique_ptr<Leaderboards> leaderboards;  // [0x0075F280]
    std::unique_ptr<Reporter> reporter;          // [0x0075F28C]
    int32_t connecting = 0;                      // [0x0075F278]
    int32_t problem = 0;                         // [0x0075F27C]
};

Objects& objects() {
    static Objects o;
    return o;
}

// [0x0075F1F0] narrowed (each wide character's low byte), as the leaderboards' URLs take it.
std::string gameName() {
    return kGameName;
}

// The answer's field skipped up to its 0 and past it; at the answer's last byte a field not ended stays.
void skipField(char*& p, const char* end) {
    while (*p != '\0') {
        if (end <= p) return;
        ++p;
    }
    ++p;
}

}  // namespace

int32_t& problem() {
    return objects().problem;
}

int32_t& connecting() {
    return objects().connecting;
}

Statistics* statistics() {
    return objects().statistics.get();
}

Leaderboards* leaderboards() {
    return objects().leaderboards.get();
}

Reporter* reporter() {
    return objects().reporter.get();
}

bool startStatistics() {
    Objects& o = objects();
    if (o.statistics != nullptr) return true;
    o.statistics = std::make_unique<Statistics>();
    return o.statistics != nullptr;
}

void stopStatistics() {
    Objects& o = objects();
    if (o.statistics == nullptr) return;
    o.statistics.release()->release();
}

// ---- the leaderboards

Leaderboards::Leaderboards() {
    m_read = false;
    m_pending = false;
    m_started = false;
    m_begun = false;
    m_leaderboard = 0;
    problem() = 0;
    m_names.clear();
    m_rows.clear();
    m_rows.resize(6);
    m_profiles.clear();
    m_profile = 0;
    m_nick.clear();
    m_total = nullptr;
    m_success = nullptr;
    m_successArg = nullptr;
    m_failure = nullptr;
    m_failureArg = nullptr;
    m_pidsInUrl = true;
}

Leaderboards::~Leaderboards() {
    if (m_started) {
        ghttp::cleanup();
        m_started = false;
    }
}

void Leaderboards::reset() {
    m_columns.clear();
    m_names.clear();
    m_profiles.clear();
    m_pending = false;
    m_read = false;
    m_players = 0;
    m_leaderboard = 0;
    problem() = 0;
}

bool Leaderboards::query(int32_t leaderboard, int32_t rows, int32_t user, int32_t* total) {
    if (m_pending) return false;
    reset();
    m_pidsInUrl = false;
    m_profile = 0;
    m_nick.clear();
    m_total = total;
    OnlineUser* users = onlineUser();
    if (user != -1 && users != nullptr) {
        // The user's record (never none: the records' list read at its number).
        const OnlineUserRecord& record = users->record(user);
        m_url = std::format("http://{}/{}/leaderboard.asp?leaderboard={}&pid={}&rows={}&v=3", m_host, gameName(),
                            leaderboard, record.profile, rows);
        m_profile = record.profile;
        m_nick = record.nick;
        // The record asked for again and the URL made again the same.
        users = onlineUser();
        if (users == nullptr) return false;
        const OnlineUserRecord& again = users->record(user);
        m_url = std::format("http://{}/{}/leaderboard.asp?leaderboard={}&pid={}&rows={}&v=3", m_host, gameName(),
                            leaderboard, again.profile, rows);
    } else if (rows == 1) {
        m_url = std::format("http://{}/{}/records.asp?v=2&leaderboard={}", m_host, gameName(), leaderboard);
    } else {
        m_url = std::format("http://{}/{}/leaderboard_{}.html", m_host, gameName(), leaderboard);
    }
    m_leaderboard = leaderboard;
    m_players = 0;
    m_begun = true;
    return true;
}

bool Leaderboards::endQuery() {
    if (!m_begun) return false;
    m_begun = false;
    return true;
}

bool Leaderboards::userStats(int32_t leaderboard, int32_t user, bool doReset) {
    if (m_pending) return false;
    if (doReset) reset();
    if (m_leaderboard == 0) {
        m_pidsInUrl = true;
        m_url = leaderboard == 1
                    ? std::format("http://{}/{}/userstats.asp?v=2&ps=1&lb={}", m_host, gameName(), leaderboard)
                    : std::format("http://{}/{}/userstats.asp?lb={}", m_host, gameName(), leaderboard);
        m_players = 0;
        for (size_t i = 0; i < 6; ++i) m_rows[i].clear();
        m_leaderboard = leaderboard;
    } else if (m_leaderboard != 1 || (leaderboard != 1 && leaderboard != 4)) {
        m_begun = true;
        return true;
    }
    OnlineUser* users = onlineUser();
    if (users == nullptr) return false;
    const OnlineUserRecord& record = users->record(user);
    if (std::find(m_profiles.begin(), m_profiles.end(), record.profile) == m_profiles.end()) {
        log::debugPrint("*** adding user (gsid %d) (uid %d)\n", record.profile, user);
        m_profiles.push_back(record.profile);
        ++m_players;
    }
    m_begun = true;
    return true;
}

bool Leaderboards::bindNames(int32_t, std::vector<const char16_t*>* out) {
    if (!m_begun) return false;
    m_columns.push_back({2, out});
    return true;
}

bool Leaderboards::bindInt64s(int32_t, std::vector<int64_t>* out) {
    if (!m_begun) return false;
    m_columns.push_back({1, out});
    return true;
}

bool Leaderboards::bindInt32s(int32_t, std::vector<int32_t>* out) {
    if (!m_begun) return false;
    m_columns.push_back({0, out});
    return true;
}

void Leaderboards::addRowBinding(const Binding& binding) {
    // The last player's row (Stock problem: with none yet the row before the first, outside the rows: not kept).
    if (static_cast<uint32_t>(m_players) > m_rows.size() || m_players == 0) return;
    m_rows[static_cast<size_t>(m_players) - 1].push_back(binding);
}

bool Leaderboards::bindRowInt64(int32_t, int64_t* out) {
    if (!m_begun) return false;
    addRowBinding({1, out});
    return true;
}

bool Leaderboards::bindRowInt32(int32_t, int32_t* out) {
    if (!m_begun) return false;
    addRowBinding({0, out});
    return true;
}

bool Leaderboards::request(int32_t* done, Callback success, void* successArg, Callback failure, void* failureArg) {
    m_success = success;
    m_successArg = successArg;
    m_failure = failure;
    m_failureArg = failureArg;
    if (m_pidsInUrl) {
        for (size_t i = 0; i < m_profiles.size(); ++i) m_url += std::format("&pid{}={}", i, m_profiles[i]);
    }
    log::debugPrint("** Requesting stat: %s\n", m_url.c_str());
    ghttp::get(m_url.c_str(), false, completed, this);
    log::debugPrint("** Requesting stat: %s\n", m_url.c_str());
    m_started = true;
    m_pending = true;
    m_done = done;
    return true;
}

void Leaderboards::think() {
    if (m_pending) {
        ghttp::think();
        return;
    }
    if (m_started) {
        ghttp::cleanup();
        m_started = false;
    }
}

bool Leaderboards::completed(int32_t, int32_t result, char* buffer, int64_t length, void* param) {
    auto* self = static_cast<Leaderboards*>(param);
    if (result == ghttp::kSuccess && self != nullptr) {
        self->parse(buffer, static_cast<uint32_t>(length));
        if (self->m_success != nullptr) self->m_success(self->m_successArg);
        return true;
    }
    self->m_pending = false;
    self->m_read = true;
    *self->m_done = 1;
    log::debugPrint("** STATS.CPP Failed to get HTTP (result %d)\n", result);
    problem() = 3;
    if (self->m_failure != nullptr) self->m_failure(self->m_failureArg);
    return true;
}

void Leaderboards::parse(char* buffer, uint32_t length) {
    if (buffer == nullptr) return;
    for (uint32_t i = 0; i < length; ++i) {
        if (buffer[i] == '\n') buffer[i] = '\0';
    }
    bool found = m_profile == 0;
    char* const end = buffer + (static_cast<int64_t>(length) - 1);
    char* p = buffer;
    if (m_players == 0) {
        const size_t columns = m_columns.size();
        const long count = std::atol(p);
        while (*p != '\0' && p < end) ++p;
        ++p;
        const long total = std::atol(p);
        if (m_total != nullptr) *m_total = total;
        while (*p != '\0' && p < end) ++p;
        ++p;
        m_names.clear();
        // A count below 0 (the stock's resize fails on it): none.
        m_names.resize(count > 0 ? static_cast<size_t>(count) : 0);
        size_t stored = 0;
        while (p < end) {
            for (size_t c = 0; c < columns; ++c) {
                if (p != nullptr && p < end) {
                    const Binding& b = m_columns[c];
                    if (b.type == 0) {
                        const long value = std::atol(p);
                        if (b.out != nullptr) static_cast<std::vector<int32_t>*>(b.out)->push_back(value);
                    } else if (b.type == 1) {
                        const int64_t value = _atoi64(p);
                        if (b.out != nullptr) static_cast<std::vector<int64_t>*>(b.out)->push_back(value);
                    } else if (b.type == 2) {
                        const size_t n = std::strlen(p);
                        // Stock problem: the names past the count are written after the names' storage; not here.
                        if (n != 0 && p + n < end && n < 0x20 && stored++ < m_names.size()) {
                            std::array<char16_t, 32>& name = m_names[stored - 1];
                            const std::u16string wide = gp::fromUtf8(p);
                            name.fill(u'\0');
                            std::copy_n(wide.begin(), std::min<size_t>(wide.size(), 31), name.begin());
                            if (b.out != nullptr) {
                                static_cast<std::vector<const char16_t*>*>(b.out)->push_back(name.data());
                                if (m_nick == name.data()) found = true;
                            }
                        }
                    }
                }
                skipField(p, end);
            }
            if (columns == 0) break;  // the stock's loop goes on without end
        }
    } else {
        const size_t fields = m_rows[0].size();  // the first row's count for every row
        if (p < end) {
            uint32_t row = 0;
            do {
                if (static_cast<uint32_t>(m_players) <= row) break;
                for (size_t i = 0; i < fields; ++i) {
                    if (*p != '\0' && p < end && row < m_rows.size()) {
                        const std::vector<Binding>& bindings = m_rows[row];
                        if (i < bindings.size() && bindings[i].type == 0) {
                            const long value = std::atol(p);
                            if (bindings[i].out != nullptr) *static_cast<int32_t*>(bindings[i].out) = value;
                        }
                    }
                    skipField(p, end);
                }
                ++row;
            } while (p < end);
        }
    }
    m_pending = false;
    m_read = true;
    *m_done = 1;
    if (!found) problem() = 6;
}

// ---- the reporter

Reporter::Reporter() {
    m_users.clear();
    m_started = false;
    reset();
}

Reporter::~Reporter() = default;

void Reporter::reset() {
    if (m_started) {
        gstats::freeGame();
        m_started = false;
    }
    m_users.clear();
    m_users.reserve(2);
    m_begun = false;
}

bool Reporter::begin(int32_t user, int32_t value) {
    if (!m_started) {
        gstats::newGame(1);
        m_started = true;
    }
    if (std::find(m_users.begin(), m_users.end(), user) == m_users.end()) {
        newPlayer(user);
        m_users.push_back(user);
    }
    m_user = user;
    m_begun = true;
    setInt(3, value);
    return true;
}

bool Reporter::end() {
    m_begun = false;
    return true;
}

bool Reporter::beginWith(int32_t user, int32_t value, int32_t, int32_t) {
    return begin(user, value);
}

void Reporter::newPlayer(int32_t user) {
    gstats::newPlayer(nullptr, user, gp::toUtf8(u""));
    OnlineUser* users = onlineUser();
    if (users == nullptr) return;
    const OnlineUserRecord& record = users->record(user);
    const gstats::Game* game = gstats::currentGame();
    std::string challenge;
    if (game == nullptr) {
        gstats::looseChallenge() =
            gstats::challengeOf(static_cast<uint32_t>(gstats::sessionKey()) ^ 0x38F371E6u);
        challenge = gstats::looseChallenge();
    } else {
        challenge = game->challenge;
    }
    const std::string auth = gstats::generateAuth(challenge, gp::toUtf8(users->password()));
    gstats::playerIntOp(nullptr, "pid", gstats::Operation::Set, record.profile, user);
    gstats::playerStringOp(nullptr, "auth", gstats::Operation::Set, auth, user);
}

bool Reporter::setText(int32_t key, const char16_t* text) {
    const std::string name = key == 1 ? std::string("player") : std::to_string(key);
    const std::string value = text != nullptr ? gp::toUtf8(text) : std::string();
    gstats::playerStringOp(nullptr, name, gstats::Operation::Set, value, m_user);
    return true;
}

bool Reporter::setFloat(int32_t key, uint64_t value) {
    gstats::playerFloatOp(nullptr, std::to_string(key), gstats::Operation::Set, static_cast<double>(value), m_user);
    return true;
}

bool Reporter::setInt(int32_t key, int32_t value) {
    if (key == 1) return false;
    gstats::playerIntOp(nullptr, std::to_string(key), gstats::Operation::Set, value, m_user);
    return true;
}

bool Reporter::send() {
    if (!m_started) return false;
    if (!gstats::connection().opened()) return false;
    int32_t failed = 0;
    if (!m_users.empty()) {
        const gstats::Game* game = gstats::currentGame();
        if (game == nullptr) {
            failed = 5;
        } else {
            gstats::sendSnapshot(game->buckets != nullptr ? game->buckets->dump() : std::string());
        }
    }
    gstats::freeGame();
    m_started = false;
    if (failed != 0) return false;
    reset();
    return true;
}

// ---- the statistics object

Statistics::Statistics() {
    gstats::gameName() = kGameName;
    gstats::secretKey() = kSecretKey;
    if (gstats::initStatsAsync(0, 0) == gstats::kConnecting) connecting() = 1;
    Objects& o = objects();
    if (o.reporter == nullptr) o.reporter = std::make_unique<Reporter>();
    if (o.leaderboards == nullptr) o.leaderboards = std::make_unique<Leaderboards>();
}

Statistics::~Statistics() {
    Objects& o = objects();
    o.reporter.reset();
    o.leaderboards.reset();
    if (gstats::connection().opened()) gstats::closeStatsConnection();
}

void Statistics::release() {
    if (--m_refs == 0) delete this;
}

void Statistics::frame() {
    Objects& o = objects();
    if (o.leaderboards != nullptr) o.leaderboards->think();
    if (o.connecting == 0) {
        if (!gstats::isStatsConnected()) {
            gstats::initStatsAsync(0, 0);
            o.connecting = 1;
        }
    } else {
        const int32_t result = gstats::initStatsThink();
        if (result != gstats::kNoError) {
            switch (result) {
            case gstats::kNoSocket:
            case gstats::kNoDns:
            case gstats::kNoConnect:
            case gstats::kDataError:
            case gstats::kTimedOut:
                log::debugPrint("Failed while processing async stats connection!!! (%d) Retrying...\n", result);
                gstats::initStatsAsync(0, 0);
                return;
            default: return;
            }
        }
        o.connecting = 0;
    }
    if (o.reporter != nullptr) o.reporter->think();
}

}  // namespace oj::net::stats
