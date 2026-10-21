#include "Core/Network/GameSpy/GameSpyStats.h"

#include "Core/Network/GameSpy/GameSpy.h"
#include "Core/Network/GameSpy/GameSpyPresence.h"
#include "Core/Base/Log.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>

namespace oj::net::gstats {

namespace {

// The SDK's state (gstats.c's statics).
struct State {
    int32_t gamePort = 0;              // [0x0075F4B0]
    uint32_t timeout = 10000;          // [0x0071AC04]
    bool keysUnlocked = false;         // [0x0075F49C]
    int32_t connectionId = 0;          // [0x0075F498]
    int32_t sessionKey = 0;            // [0x0075F494]
    int32_t state = 0;                 // [0x0075F4AC]
    uint32_t connectStart = 0;         // [0x0075F4B4]
    std::vector<uint8_t> buffer;       // [0x0075F4A0], its size [0x0075F4A4], its length [0x0075F4A8]
    size_t bufferSize = 0;
    TcpConnection socket;              // [0x0071ABB0]
    std::unique_ptr<Game> game;        // [0x0075F490]
    std::string looseChallenge;        // [0x0075B358]
};

State& st() {
    static State s;
    return s;
}

// The keys as they sit in the data, their first letters 0 until the SDK first runs.
constexpr std::string_view kKeys[5] = {"GameSpy3D", "Industries", "ProjectAphex", "gstats.dat", "\\final\\"};

// The format strings XORed with "ProjectAphex" in the stock code (0x006FAAC8, 0x006FAB84, 0x006FAB60, 0x006FAB34,
// 0x006FAAFC), here as they read.
constexpr const char* kAuth = "\\auth\\\\gamename\\%s\\response\\%s\\port\\%d\\id\\1";
constexpr const char* kNewGame = "\\newgame\\\\connid\\%d\\sesskey\\%d";
constexpr const char* kNewGameOffline = "\\newgame\\\\sesskey\\%d\\\\challenge\\%d";
constexpr const char* kUpdate = "\\updgame\\\\sesskey\\%d\\done\\%d\\gamedata\\%s";
constexpr const char* kUpdateOffline = "\\updgame\\\\sesskey\\%d\\connid\\%d\\done\\%d\\gamedata\\%s\\dl\\1";

// The hash of the snapshots and the challenges: h = c + h x -0x63306CE7 over the signed bytes.
int32_t stringHash(const uint8_t* bytes, size_t size) {
    uint32_t h = 0;
    for (size_t i = 0; i < size; ++i) {
        h = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(bytes[i]))) + h * 0x9CCF9319u;
    }
    return static_cast<int32_t>(h);
}

void unlockKeys() {
    st().keysUnlocked = true;
}

// 0x00567B30: the value after "\<name>\" up to the next '\' (two buffers by turns, [0x0075B354]); none when the field
// is not there.
bool valueOf(std::string_view text, std::string_view name, std::string& out) {
    const std::string field = "\\" + std::string(name) + "\\";
    const size_t at = text.find(field);
    if (at == std::string_view::npos) return false;
    out.clear();
    for (size_t i = at + field.size(); i < text.size() && text[i] != '\0' && text[i] != '\\'; ++i) out += text[i];
    return true;
}

// 0x00567710: the bytes XORed with "GameSpy3D", "\final\" after them, all of it sent; the bytes sent, else the error.
int32_t sendEncrypted(std::string message) {
    std::vector<uint8_t> bytes(message.begin(), message.end());
    xorWith(bytes.data(), bytes.size(), kKeys[0]);
    for (char c : kKeys[4]) bytes.push_back(static_cast<uint8_t>(c));
    if (!st().socket.opened() || st().socket.state() == TcpConnection::State::Failed) return -1;
    st().socket.send(bytes.data(), bytes.size());
    st().socket.poll();
    if (st().socket.state() == TcpConnection::State::Failed) return -1;
    return static_cast<int32_t>(bytes.size());
}

// The socket's readable check (0x0056E460): bytes waiting.
bool readable() {
    st().socket.poll();
    return !st().socket.received().empty() || st().socket.lost();
}

// recv: up to `size` of the bytes come (0 for a closed peer).
size_t receive(uint8_t* into, size_t size) {
    std::vector<uint8_t>& in = st().socket.received();
    const size_t n = std::min(size, in.size());
    std::copy(in.begin(), in.begin() + static_cast<std::ptrdiff_t>(n), into);
    in.erase(in.begin(), in.begin() + static_cast<std::ptrdiff_t>(n));
    return n;
}

void clearBuffer() {
    st().bufferSize += st().buffer.size();
    st().buffer.assign(st().buffer.size(), 0);
    st().buffer.clear();
}

// 0x005674A0: the server's "\challenge\" hashed, "%d%s" with the secret key, its MD5 the response of "\auth\" sent
// with the game's name and port; kDataError without a challenge, kNoConnect when the send fails.
int32_t answerChallenge(std::string_view text, int32_t port) {
    std::string challenge;
    if (!valueOf(text, "challenge", challenge)) {
        st().socket.close();
        return kDataError;
    }
    const int32_t h = stringHash(reinterpret_cast<const uint8_t*>(challenge.data()), challenge.size());
    const std::string response = gp::md5Hex(std::format("{}{}", h, secretKey()));
    char message[128];
    const int n = std::snprintf(message, sizeof(message), kAuth, gameName().c_str(), response.c_str(), port);
    if (sendEncrypted(std::string(message, static_cast<size_t>(std::max(n, 0)))) < 1) {
        st().socket.close();
        return kNoConnect;
    }
    return kNoError;
}

// 0x00567600: the answer (0x80 bytes at most) XORed with "GameSpy3D", its "\sesskey\" the session key.
int32_t readSessionKey() {
    uint8_t answer[0x81];
    const size_t n = receive(answer, 0x80);
    answer[n] = 0;
    xorWith(answer, n, kKeys[0]);
    std::string key;
    if (valueOf(std::string_view(reinterpret_cast<const char*>(answer), n), "sesskey", key)) {
        st().sessionKey = std::atol(key.c_str());
        return kNoError;
    }
    st().socket.close();
    return kDataError;
}

// 0x00567A10: a message kept in gstats.dat ("ab"): its hash, its length ^ 0x70F33A5F, the bytes XORed with
// "Industries" (in place, as the stock's).
void keepOffline(std::string& message) {
    std::ofstream file(std::string(kKeys[3]), std::ios::binary | std::ios::app);
    if (!file) return;
    const auto size = static_cast<uint32_t>(message.size());
    const int32_t hash = stringHash(reinterpret_cast<const uint8_t*>(message.data()), size);
    const uint32_t length = size ^ 0x70F33A5Fu;
    xorWith(reinterpret_cast<uint8_t*>(message.data()), size, kKeys[1]);
    file.write(reinterpret_cast<const char*>(&hash), 4);
    file.write(reinterpret_cast<const char*>(&length), 4);
    file.write(message.data(), size);
}

// 0x005677D0: gstats.dat's messages ("rb"), each checked by its hash (a wrong one ends the read), XORed back to the
// server's key and each with "\final\" after it, sent at once; sent, the file removed (not sent: the socket closed).
void sendOffline() {
    const std::string name(kKeys[3]);
    std::ifstream file(name, std::ios::binary);
    if (!file) return;
    std::string all;
    for (;;) {
        int32_t hash = 0;
        uint32_t length = 0;
        if (!file.read(reinterpret_cast<char*>(&hash), 4) || !file.read(reinterpret_cast<char*>(&length), 4)) break;
        length ^= 0x70F33A5Fu;
        std::string bytes(length, '\0');
        if (!file.read(bytes.data(), length)) break;
        xorWith(reinterpret_cast<uint8_t*>(bytes.data()), length, kKeys[1]);
        if (hash != stringHash(reinterpret_cast<const uint8_t*>(bytes.data()), length)) break;
        xorWith(reinterpret_cast<uint8_t*>(bytes.data()), length, kKeys[0]);
        all += bytes;
        all += std::string(kKeys[4]);
    }
    file.close();
    st().socket.send(all.data(), all.size());
    st().socket.poll();
    if (all.empty() || st().socket.state() == TcpConnection::State::Failed) {
        st().socket.close();
        return;
    }
    std::error_code ec;
    std::filesystem::remove(name, ec);
}

// The persistent data's answers (0x00567FA0: "\pauthr\", "\getpidr\", "\getpdr\", "\setpdr\"), each looked up in the
// requests waiting ([0x0075F4B8], 0x00567C90) and given to its callback (0x005680C0): the game asks for none, so none
// waits and the answers go.
void persistAnswer(std::string_view /*message*/) {}

// 0x00568030: each "\final\"-ended message of the buffer XORed with "GameSpy3D" and answered; the bytes used.
size_t answerMessages(std::vector<uint8_t>& bytes) {
    size_t used = 0;
    for (;;) {
        const std::string_view text(reinterpret_cast<const char*>(bytes.data()) + used, bytes.size() - used);
        const size_t end = text.find(kKeys[4]);
        if (end == std::string_view::npos) break;
        xorWith(bytes.data() + used, end, kKeys[0]);
        persistAnswer(std::string_view(reinterpret_cast<const char*>(bytes.data()) + used, end));
        used += end + kKeys[4].size();
        if (used >= bytes.size()) break;
    }
    return used;
}

void freeGameRecord() {
    st().game.reset();
}

}  // namespace

Service& service() {
    static Service s;
    return s;
}

std::string& gameName() {
    static std::string s = kGameName;
    return s;
}

std::string& secretKey() {
    static std::string s = kSecretKey;
    return s;
}

std::string_view key(int which) {
    return kKeys[which];
}

void xorWith(uint8_t* bytes, size_t size, std::string_view k) {
    if (k.empty()) return;
    size_t j = 0;
    for (size_t i = 0; i < size; ++i) {
        bytes[i] ^= static_cast<uint8_t>(k[j]);
        if (++j == k.size()) j = 0;
    }
}

TcpConnection& connection() {
    return st().socket;
}

int32_t connectionState() {
    return st().state;
}

int32_t initStatsAsync(int32_t gamePort, uint32_t timeoutMs) {
    State& s = st();
    s.gamePort = gamePort;
    if (timeoutMs != 0) s.timeout = timeoutMs;
    if (availableResult() != 1) return kNoSocket;
    if (!s.keysUnlocked) unlockKeys();
    s.connectionId = static_cast<int32_t>(GetTickCount());
    if (s.socket.opened()) closeStatsConnection();
    s.buffer.clear();
    // "gamestats.gamespy.com" is no address: the game's name and a dot before it.
    const std::string host = service().host.empty() ? gameName() + ".gamestats.gamespy.com" : service().host;
    const uint32_t ip = resolveHost(host);
    if (ip == 0) return kNoDns;
    if (!s.socket.connect(ip, service().port)) return kNoSocket;
    if (s.socket.state() == TcpConnection::State::Failed) {
        s.state = 1;
        s.socket.close();
        return kNoConnect;
    }
    s.buffer.reserve(0x40);
    s.bufferSize = 0x40;
    s.connectStart = GetTickCount();
    s.state = 2;
    return kConnecting;
}

int32_t initStatsThink() {
    State& s = st();
    const auto fail = [&](int32_t result) {
        s.state = 1;
        closeStatsConnection();
        return result;
    };
    const auto waiting = [&] {
        if (GetTickCount() - s.connectStart <= s.timeout) return static_cast<int32_t>(kConnecting);
        return fail(kTimedOut);
    };
    switch (s.state) {
    case 2:
        s.socket.poll();
        if (s.socket.state() == TcpConnection::State::Failed) return fail(kNoConnect);
        if (s.socket.state() != TcpConnection::State::Connected) return waiting();
        s.state = 3;
        [[fallthrough]];
    case 3: {
        if (!readable()) return waiting();
        uint8_t chunk[0x40];
        const size_t n = receive(chunk, s.bufferSize - s.buffer.size());
        s.buffer.insert(s.buffer.end(), chunk, chunk + n);
        s.bufferSize -= n;
        if (s.buffer.size() < 0x26) return kConnecting;
        s.state = 4;
        xorWith(s.buffer.data(), s.buffer.size(), kKeys[0]);
        const int32_t result =
            answerChallenge(std::string_view(reinterpret_cast<const char*>(s.buffer.data()), s.buffer.size()),
                            s.gamePort);
        if (result != kNoError) return fail(result);
        clearBuffer();
        s.state = 4;
        [[fallthrough]];
    }
    case 4: {
        if (!readable()) return waiting();
        const int32_t result = readSessionKey();
        if (result != kNoError) return fail(result);
        clearBuffer();
        sendOffline();
        s.state = 5;
        return kNoError;
    }
    case 5:
        return kNoError;
    default:
        return kNoConnect;
    }
}

bool isStatsConnected() {
    State& s = st();
    if (!s.socket.opened() || s.state != 5) return false;
    for (;;) {
        s.socket.poll();
        if (s.socket.received().empty() && !s.socket.lost()) return s.socket.opened();
        uint8_t chunk[0x100];
        const size_t n = receive(chunk, sizeof(chunk));
        if (n < 1) {
            closeStatsConnection();
            return false;
        }
        s.buffer.insert(s.buffer.end(), chunk, chunk + n);
        const size_t used = answerMessages(s.buffer);
        s.buffer.erase(s.buffer.begin(), s.buffer.begin() + static_cast<std::ptrdiff_t>(used));
    }
}

void closeStatsConnection() {
    State& s = st();
    s.socket.close();
    // 0x005681D0: the persistent data's requests failed ("Connection lost" to their callbacks): none waits.
    s.buffer.clear();
    s.bufferSize = 0;
}

std::string generateAuth(std::string_view challenge, std::string_view keyText) {
    if (challenge.size() + keyText.size() + 0x14 > 0x7F) return "CD Key or challenge too long";
    return gp::md5Hex(std::string(challenge) + std::string(keyText));
}

int32_t sessionKey() {
    return st().sessionKey;
}

Game* currentGame() {
    return st().game.get();
}

std::string& looseChallenge() {
    return st().looseChallenge;
}

std::string challengeOf(uint32_t value) {
    std::string text = std::format("{:08x}", value);
    for (size_t i = 0; i < text.size(); ++i) text[i] = static_cast<char>(text[i] + static_cast<char>(i) + 0x11);
    return text;
}

Game* newGame(int32_t useBuckets) {
    State& s = st();
    auto game = std::make_unique<Game>();
    if (!s.keysUnlocked) unlockKeys();
    game->connectionId = s.connectionId++;
    game->sessionKey = s.sessionKey;
    game->useBuckets = useBuckets;
    bool sent = false;
    if (s.socket.opened()) {
        // The session key goes into the connection's field and the id into the key's (the stock's order).
        char message[256];
        const int n = std::snprintf(message, sizeof(message), kNewGame, s.sessionKey, game->connectionId);
        if (sendEncrypted(std::string(message, static_cast<size_t>(std::max(n, 0)))) < 1) closeStatsConnection();
        game->challenge = challengeOf(static_cast<uint32_t>(game->sessionKey) ^ 0x38F371E6u);
        sent = s.socket.opened();
    }
    if (!sent) {
        char message[256];
        const int32_t id = game->connectionId;
        const int n = std::snprintf(message, sizeof(message), kNewGameOffline, id,
                                    static_cast<int32_t>(static_cast<uint32_t>(id) ^ 0x38F371E6u));
        std::string kept(message, static_cast<size_t>(std::max(n, 0)));
        keepOffline(kept);
        game->sessionKey = 0;
        game->challenge = challengeOf(static_cast<uint32_t>(id) ^ 0x38F371E6u);
    }
    if (game->useBuckets != 0) game->buckets = std::make_unique<BucketSet>();
    game->startTime = GetTickCount();
    s.game = std::move(game);
    return s.game.get();
}

void newPlayer(Game* game, int32_t player, std::string_view name) {
    if (game == nullptr) game = st().game.get();
    if (game == nullptr) return;
    while (static_cast<int32_t>(game->playerNumbers.size()) <= player) game->playerNumbers.push_back(-1);
    game->playerNumbers[static_cast<size_t>(player)] = game->totalPlayers++;
    const auto seconds = static_cast<int32_t>(GetTickCount() - game->startTime) / 1000;
    playerIntOp(game, "ctime", Operation::Set, seconds, player);
    playerStringOp(game, "player", Operation::Set, name, player);
}

void freeGame() {
    freeGameRecord();
}

// ---- the buckets

size_t BucketSet::hash(std::string_view name) {
    uint32_t h = 0;
    for (char c : name) {
        h = static_cast<uint32_t>(std::tolower(static_cast<unsigned char>(c))) - h * 0x63306CE7u;
    }
    return h % kLists;
}

Bucket* BucketSet::find(std::string_view name) {
    for (Bucket& b : m_lists[hash(name)]) {
        if (_stricmp(b.name.c_str(), std::string(name).c_str()) == 0) return &b;
    }
    return nullptr;
}

Bucket& BucketSet::enter(Bucket bucket) {
    std::vector<Bucket>& list = m_lists[hash(bucket.name)];
    for (Bucket& b : list) {
        if (_stricmp(b.name.c_str(), bucket.name.c_str()) == 0) {
            b = std::move(bucket);
            return b;
        }
    }
    list.push_back(std::move(bucket));
    return list.back();
}

std::string BucketSet::dump() const {
    std::string out;
    for (const std::vector<Bucket>& list : m_lists) {
        for (const Bucket& b : list) {
            if (b.type == 0) {
                out += std::format("\\{}\\{}", b.name, b.integer);
            } else if (b.type == 1) {
                char text[64];
                std::snprintf(text, sizeof(text), "%f", b.real);
                out += "\\" + b.name + "\\" + text;
            } else if (b.type == 2) {
                out += "\\" + b.name + "\\" + (b.hasText ? b.text : std::string("(null)"));
            }
        }
    }
    return out;
}

namespace {

// 0x0056E140 with 0x0056E120: a string value copied, its '\' made '/'.
std::string slashed(std::string_view text) {
    std::string s(text);
    for (char& c : s) {
        if (c == '\\') c = '/';
    }
    return s;
}

BucketSet* setOf(Game*& game) {
    if (game == nullptr) game = st().game.get();
    return game != nullptr ? game->buckets.get() : nullptr;
}

int32_t intOp(Game* game, const std::string& name, Operation op, int32_t value) {
    BucketSet* set = setOf(game);
    if (game == nullptr) return value;
    if (set == nullptr) return value;
    if (Bucket* b = set->find(name)) {
        if (op == Operation::Add) {
            if (b->type == 0) b->integer = b->integer + value;
            if (b->type == 1) b->real = b->real + static_cast<double>(value);
        } else if (b->type == 0) {
            b->integer = value;
        }
        return b->type == 0 ? b->integer : value;
    }
    Bucket b;
    b.name = name;
    b.type = 0;
    b.integer = value;
    return set->enter(std::move(b)).integer;
}

double floatOp(Game* game, const std::string& name, Operation op, double value) {
    BucketSet* set = setOf(game);
    if (game == nullptr || set == nullptr) return value;
    if (Bucket* b = set->find(name)) {
        if (op == Operation::Add && b->type == 1) {
            b->real = b->real + value;
        } else if (b->type == 1) {
            b->real = value;
        }
        return b->type == 1 ? b->real : value;
    }
    Bucket b;
    b.name = name;
    b.type = 1;
    b.real = value;
    return set->enter(std::move(b)).real;
}

std::string stringOp(Game* game, const std::string& name, Operation op, std::string_view value) {
    BucketSet* set = setOf(game);
    if (game == nullptr || set == nullptr) return std::string(value);
    if (Bucket* b = set->find(name)) {
        if (b->type == 2) {
            b->text = op == Operation::Add ? b->text + std::string(value) : slashed(value);
            b->hasText = true;
        }
        return b->text;
    }
    Bucket b;
    b.name = name;
    b.type = 2;
    b.text = slashed(value);
    b.hasText = true;
    return set->enter(std::move(b)).text;
}

int32_t numberOf(Game* game, int32_t index, bool team) {
    if (game == nullptr) game = st().game.get();
    if (game == nullptr) return index;
    const std::vector<int32_t>& numbers = team ? game->teamNumbers : game->playerNumbers;
    return index >= 0 && static_cast<size_t>(index) < numbers.size() ? numbers[static_cast<size_t>(index)] : -1;
}

}  // namespace

int32_t bucketIntOp(Game* game, std::string_view name, Operation op, int32_t value) {
    return intOp(game, std::string(name), op, value);
}

double bucketFloatOp(Game* game, std::string_view name, Operation op, double value) {
    return floatOp(game, std::string(name), op, value);
}

std::string bucketStringOp(Game* game, std::string_view name, Operation op, std::string_view value) {
    return stringOp(game, std::string(name), op, value);
}

int32_t teamIntOp(Game* game, std::string_view name, Operation op, int32_t value, int32_t team) {
    return intOp(game, std::format("{}_t{}", name, numberOf(game, team, true)), op, value);
}

double teamFloatOp(Game* game, std::string_view name, Operation op, double value, int32_t team) {
    return floatOp(game, std::format("{}_t{}", name, numberOf(game, team, true)), op, value);
}

std::string teamStringOp(Game* game, std::string_view name, Operation op, std::string_view value, int32_t team) {
    return stringOp(game, std::format("{}_t{}", name, numberOf(game, team, true)), op, value);
}

int32_t playerIntOp(Game* game, std::string_view name, Operation op, int32_t value, int32_t player) {
    return intOp(game, std::format("{}_{}", name, numberOf(game, player, false)), op, value);
}

double playerFloatOp(Game* game, std::string_view name, Operation op, double value, int32_t player) {
    return floatOp(game, std::format("{}_{}", name, numberOf(game, player, false)), op, value);
}

std::string playerStringOp(Game* game, std::string_view name, Operation op, std::string_view value, int32_t player) {
    return stringOp(game, std::format("{}_{}", name, numberOf(game, player, false)), op, value);
}

bool sendSnapshot(const std::string& data) {
    State& s = st();
    Game* game = s.game.get();
    if (game == nullptr || !s.socket.opened()) return false;
    std::string sent = data;
    for (char& c : sent) {
        if (c == '\\') c = '\x01';
    }
    std::string message;
    const int n = std::snprintf(nullptr, 0, kUpdate, game->connectionId, 1, sent.c_str());
    message.resize(static_cast<size_t>(std::max(n, 0)) + 1);
    std::snprintf(message.data(), message.size(), kUpdate, game->connectionId, 1, sent.c_str());
    message.resize(static_cast<size_t>(std::max(n, 0)));
    if (sendEncrypted(message) < 1) closeStatsConnection();
    if (!s.socket.opened()) {
        const int m = std::snprintf(nullptr, 0, kUpdateOffline, game->connectionId, game->sessionKey, 1, sent.c_str());
        std::string kept(static_cast<size_t>(std::max(m, 0)) + 1, '\0');
        std::snprintf(kept.data(), kept.size(), kUpdateOffline, game->connectionId, game->sessionKey, 1, sent.c_str());
        kept.resize(static_cast<size_t>(std::max(m, 0)));
        keepOffline(kept);
    }
    return true;
}

}  // namespace oj::net::gstats
