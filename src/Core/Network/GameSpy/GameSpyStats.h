#pragma once
// GameSpy's game statistics of the SDK linked in the stock game (gstats, docs/spec/28-network.md, "The statistics"):
// the connection to "<game>.gamestats.gamespy.com":29920 (InitStatsAsync 0x00566A90, InitStatsThink 0x00566D30,
// IsStatsConnected 0x00567360, CloseStatsConnection 0x00566F70), its messages XORed with "GameSpy3D" and ended by
// "\final\", the games reported (NewGame 0x00566FD0, NewPlayer 0x00567270) with their buckets (0x00568300 ..
// 0x005687A0 over the table 0x0056DA70 .. 0x0056E250), the snapshots kept in gstats.dat while there is no connection
// (0x00567A10, sent at the next connection 0x005677D0) and the persistent data's answers (0x00567FA0, which the game
// never asks for).

#include "Core/Network/NetTcp.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace oj::net::gstats {

// GE results of the connection.
enum Result : int32_t {
    kNoError = 0,
    kNoSocket = 1,
    kNoDns = 2,
    kNoConnect = 3,
    kBusy = 4,
    kDataError = 5,
    kConnecting = 6,
    kTimedOut = 7,
};

// A bucket of a game's set (0x18 bytes: its name +0x00, its type +0x04, its value +0x10): 0 an integer, 1 a float,
// 2 a string ('\' made '/', 0x0056E120).
struct Bucket {
    std::string name;
    int32_t type = 0;
    int32_t integer = 0;
    double real = 0.0;
    std::string text;
    bool hasText = false;
};

// 0x0056DA70: a game's buckets, a hash table of 32 lists (0x0057E200 with 0x18-byte elements): the hash
// tolower(c) - h x 0x63306CE7 over the name (0x0056E1A0) modulo 32, names alike without case (0x0056E200); a name
// entered again replaces its bucket (0x0057E320), a new one goes at its list's end.
class BucketSet {
public:
    static constexpr size_t kLists = 32;
    static size_t hash(std::string_view name);
    Bucket* find(std::string_view name);           // 0x0057E430
    Bucket& enter(Bucket bucket);                   // 0x0057E320
    // 0x0056DAB0 with 0x0056E020: each bucket in the lists' order "\name\value" ("%d", "%f", the string).
    std::string dump() const;

private:
    std::array<std::vector<Bucket>, kLists> m_lists;
};

// The bucket operations (bo_set 0x0056DB90, bo_add 0x0056DBE0): the bucket's value set or added to (a string's
// appended, 0x0056DE60); the bucket when it is there, none for a new one.
enum class Operation { Set, Add };

// A game reported (0x30 bytes, 0x00566FD0).
struct Game {
    int32_t sessionKey = 0;          // +0x00: [0x0075F494] as the game was made, 0 without a connection
    int32_t connectionId = 0;        // +0x04: [0x0075F498], counted up
    int32_t useBuckets = 0;          // +0x08
    std::unique_ptr<BucketSet> buckets;  // +0x0C
    std::string challenge;           // +0x10: "%.8x" of the session key, each character + its place + 0x11
    std::vector<int32_t> playerNumbers;  // +0x1C: each player's number in the game (-1 none)
    std::vector<int32_t> teamNumbers;    // +0x20
    int32_t totalPlayers = 0;        // +0x24
    int32_t totalTeams = 0;          // +0x28
    uint32_t startTime = 0;          // +0x2C: GetTickCount
};

// The service's host and port: "<game>.gamestats.gamespy.com" (s_gamestats_gamespy_com 0x0071AC08 after the game's
// name and a dot) and 29920. Only the tests point them elsewhere.
struct Service {
    std::string host;  // "" → "<game>.gamestats.gamespy.com"
    uint16_t port = 29920;
};
Service& service();

// [0x0075F290], [0x0075F390]: the game's name and secret key as the statistics object copied them (0x0055C760).
std::string& gameName();
std::string& secretKey();

// 0x00566A90: the connection begun (the available check's result [0x007A4380] must be 1, else kNoSocket); the
// previous closed; the connection's id [0x0075F498] the tick count; timeoutMs (0: kept, 10 s at first) the wait for
// the server. kConnecting while it goes on.
int32_t initStatsAsync(int32_t gamePort, uint32_t timeoutMs);
// 0x00566D30: the connection's frame: connected, the server's challenge (0x26 bytes at least) answered with "\auth\"
// (0x005674A0), the session key read (0x00567600), the snapshots of gstats.dat sent (0x005677D0): kNoError once
// connected; kConnecting while waiting; else the failure (the connection closed).
int32_t initStatsThink();
// 0x00567360: connected; the bytes come read, each "\final\"-ended message answered (persistent data's answers).
bool isStatsConnected();
// 0x00566F70: the socket closed, the persistent data's requests failed, the buffer freed.
void closeStatsConnection();
// The connection's state ([0x0075F4AC]: 1 failed, 2 connecting, 3 connected waiting for the challenge, 4 waiting for
// the session key, 5 connected).
int32_t connectionState();
// [0x0075F494]: the session key the server gave (0 before it).
int32_t sessionKey();

// 0x005669C0: the auth of a CD key or a hash for a challenge: md5hex(challenge + key), "CD Key or challenge too long"
// when the two pass 108 characters.
std::string generateAuth(std::string_view challenge, std::string_view key);

// [0x0075F490]: the game being reported.
Game* currentGame();
// 0x00566FD0: a new game; connected, "\newgame\\connid\<id>\sesskey\<key>" sent (a failed send closes the
// connection); without a connection the session key 0 and "\newgame\\sesskey\<id>\\challenge\<id ^ 0x38F371E6>"
// kept in gstats.dat. With buckets the set and the players' and teams' numbers made.
Game* newGame(int32_t useBuckets);
// 0x00567270: a player of the game numbered (the next of totalPlayers), its "ctime" (seconds since the game began)
// and "player" set.
void newPlayer(Game* game, int32_t player, std::string_view name);
// 0x00564C30's / 0x00565070's end: the game freed.
void freeGame();
// [0x0075B358]: the challenge made without a game (0x00564DA0).
std::string& looseChallenge();
// 0x00567AF0: "%.8x" of a value, each character + its place + 0x11.
std::string challengeOf(uint32_t value);

// The buckets of the game (none: [0x0075F490]; none of either: the value given). Player and team buckets are named
// "<name>_<number>" / "<name>_t<number>" by the player's or team's number in the game.
int32_t bucketIntOp(Game* game, std::string_view name, Operation op, int32_t value);        // 0x00568300
double bucketFloatOp(Game* game, std::string_view name, Operation op, double value);       // 0x00568350
std::string bucketStringOp(Game* game, std::string_view name, Operation op, std::string_view value);  // 0x005683A0
int32_t teamIntOp(Game* game, std::string_view name, Operation op, int32_t value, int32_t team);        // 0x005683F0
double teamFloatOp(Game* game, std::string_view name, Operation op, double value, int32_t team);       // 0x005684B0
std::string teamStringOp(Game* game, std::string_view name, Operation op, std::string_view value, int32_t team);
int32_t playerIntOp(Game* game, std::string_view name, Operation op, int32_t value, int32_t player);    // 0x00568620
double playerFloatOp(Game* game, std::string_view name, Operation op, double value, int32_t player);   // 0x005686E0
std::string playerStringOp(Game* game, std::string_view name, Operation op, std::string_view value, int32_t player);

// 0x00565070's send of the game's data ('\' made 0x01): "\updgame\\sesskey\%d\done\%d\gamedata\%s" (0x006FAB34
// decoded) with the game's connection id as the session key and done 1; when the send closes the connection, kept in
// gstats.dat as "\updgame\\sesskey\%d\connid\%d\done\%d\gamedata\%s\dl\1" (0x006FAAFC: the id, then the session
// key). False without a game or a connection (nothing kept then).
bool sendSnapshot(const std::string& data);

// The XOR keys (0x0071ABB4 ..): their first letters written as the SDK first runs (0x0075F49C).
std::string_view key(int which);  // 0 "GameSpy3D", 1 "Industries", 2 "ProjectAphex", 3 "gstats.dat", 4 "\final\"
// 0x00567AC0 (the key [0x0071AC00] chosen): each byte XORed with the key's, round.
void xorWith(uint8_t* bytes, size_t size, std::string_view key);

// For the tests: the connection's socket.
TcpConnection& connection();

}  // namespace oj::net::gstats
