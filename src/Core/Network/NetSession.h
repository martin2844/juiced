#pragma once
// The game's network session over the transport (the code at 0x004E66E0 .. 0x004EBD60; docs/spec/28-network.md, "The
// session"): the machines and the players of the session, the host's setup of the race (place, route, laps, time of
// day, weather, type, difficulty) and its stages (the lobby, the setup's pages, the cars, loading, the count, the
// race), the cars' states each tenth of a second, the places and the finishes. The game's side (the windows, the
// race, the HUD) is reached through the hooks.

#include "Core/Network/NetTransport.h"
#include "Core/Network/SessionStage.h"
#include "Core/Race/RaceKinds.h"
#include "Core/Base/Random.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oj {
struct Car;
struct CarDef;
}
namespace oj::save {
struct Settings;
}

namespace oj::net {

// A player's car as the session passes it (0x74 bytes; the defaults 0x004E6920; made from a car by 0x00470A50): the
// CarDef's name, the body parts, the catalogue parts and the paint of the car record.
struct CarConfig {
    std::array<char, 24> name{};          // +0x00: the CarDef's name (+0x1C)
    std::array<uint8_t, 10> body{};       // +0x18: the record's +0x04
    std::array<uint8_t, 10> mods{};       // +0x22: the record's +0x0E
    uint32_t neon = 0xFFFFFFFF;           // +0x2C: +0x2C
    uint32_t base = 0xFFFFFFFF;           // +0x30: +0x18
    uint32_t metallic = 0xFFFFFFFF;       // +0x34: +0x1C
    uint32_t pearl = 0xFFFFFFFF;          // +0x38: +0x20
    uint32_t decalColour = 0xFFFFFFFF;    // +0x3C: +0x38
    uint32_t sponsorColour = 0xFF000000;  // +0x40: +0x4C
    uint32_t sponsorDecals = 1;           // +0x44: +0x48
    uint32_t metallicAmount = 0x3F000000; // +0x48: +0x24 (0.5)
    uint32_t pearlAmount = 0x3E800000;    // +0x4C: +0x28 (0.25)
    uint32_t dialColour = 0xC82627C2;     // +0x50: +0x30
    uint32_t dialTextColour = 0xC8A9F0FF; // +0x54: +0x34
    uint32_t wheelColour = 0xFFFFFFFF;    // +0x58: +0x3C
    uint32_t wheelsColoured = 0;          // +0x5C: +0x40
    uint32_t tint = 0;                    // +0x60: +0x44
    uint32_t lowering = 0;                // +0x64: +0x58
    uint32_t gearing = 0x3F000000;        // +0x68: +0x5C (0.5)
    uint32_t torqueFactor = 0x3F800000;   // +0x6C: +0x60 (1)
    uint32_t horn = 1;                    // +0x70: +0xB0

    CarConfig();  // 0x004E6920: the body and the catalogue parts none (0xFF)
    void write(uint8_t* out) const;  // 0x74 bytes
    static CarConfig read(const uint8_t* in);
    // 0x00470A50: of a car record (its CarDef's name).
    static CarConfig of(const Car& car, const std::string& defName);
    // The record of the config (the CarDef found by the caller).
    void apply(Car& car) const;
};

// A machine of the session (0x0C bytes, [0x0076A1D0]): its peer, only watching (+4: none of its players in the race;
// its 0x0E's spectator flag) and loaded (+8, 0x0F).
struct NetNode {
    int32_t peer = 0;
    int32_t spectating = 1;
    int32_t loaded = 0;
};

// The 13 values a player's info passes (+0xBC .. +0xEC).
struct PlayerRace {
    int32_t notRacing = 1;   // +0xBC: 0 in the race
    int32_t ready = 0;       // +0xC0
    int32_t gridPlace = 0;   // +0xC4
    int32_t distance = 0;    // +0xC8: the race's distance (metres, its car's +0x494 +0x98 rounded)
    int32_t finished = 0;    // +0xCC
    int32_t finishTime = 0;     // +0xD0: the finish (0x13): the race time and the best lap, a float's bits each
    int32_t finishBestLap = 0;  // +0xD4
    int32_t place = 0;       // +0xD8 (0x17)
    int32_t left = 0;        // +0xDC: left during the race
    int32_t points = 0;      // +0xE0 (0x1B)
    int32_t races = 0;       // +0xE4
    int32_t cruiseScore = 0; // +0xE8 (0x19)
    int32_t leftHow = 0;     // +0xEC: the event's value when it left (1 silent, 0 left)
};

// A player (0x208 bytes, [0x0076A1E0]): of a peer (-1 this machine's), its index on its machine.
struct NetPlayer {
    int32_t peer = -1;               // +0x00
    int32_t localIndex = 0;          // +0x04
    int32_t gpPlayer = -1;           // +0x08: its GP player, the user object's (online, ours)
    int32_t profile = -1;            // +0x0C: its GP profile (online)
    int32_t infoKnown = 0;           // +0x10
    std::u16string name;             // +0x14: 26 characters
    CarConfig car;                   // +0x48
    PlayerRace race;                 // +0xBC
    void* raceCar = nullptr;         // +0xF0: its car in the race (the game's)
    int32_t reset = 0;               // +0xF4: its car was put back (bit 4 of its next state)
    std::array<uint8_t, 0x40> state{};  // +0xF8: the last state taken
    int32_t hasState = 0;            // +0x138
    float senderTime = 0.0f;         // +0x13C: its sender's race time of that state; ours for our cars
    float period = 0.1f;             // +0x140: our cars' states sent every this (0x004E8310)
    float sendTimer = 0.0f;          // +0x144
    float timeOffset = 0.0f;         // +0x148: how late its states come, within 0.1, x 0.96 each one
    // +0x14C .. +0x1BB, the voice's relay of the Xbox (filled by nothing on the PC): 0A 00 0D, +0x04 the player's
    // index, +0x08 the bytes that follow (+0x154, kept 0 here), then them; sent to every peer every 0.2 s while there
    // are any (0x004EB210). (+0x1BC 0x14 and +0x1C0 the index: the state's message with them, 0x004E9590.)
    uint16_t voiceBytes = 0;         // +0x154
    std::array<uint8_t, 0x66> voiceData{};  // +0x156
};

class NetSession {
public:
    // What the session asks of the game.
    struct Hooks {
        // The race's rules: the place's and the route's names, the race's type (0x004E8750, the session's own
        // numbering), laps, time of day, weather, difficulty.
        // Called with the message's id: 3 the place, 4 the route, 5 the laps (kept by the session alone), 6 the time
        // of day, 7 the weather, 9 the difficulty, 8 the race's type, 0x0A a stage and 0x0E the race (everything).
        std::function<void(uint8_t id)> setupChanged;
        // The HUD's notes (the race's front end there): a player joined the lobby (0x004E8B50), left (0x004E88E0),
        // finished first (0x004E86C0), took the lead (0x004E85C0, `fromLeader` 1 when told by the host's 0x18),
        // the countdown of the stage's wait (0x004EADE0, seconds).
        std::function<void(const NetPlayer&)> joined;
        std::function<void(const NetPlayer&)> left;
        std::function<void(const NetPlayer&)> firstFinished;
        std::function<void(const NetPlayer&, bool fromLeader)> leader;
        std::function<void(uint32_t seconds)> countdown;
        // The cruise's score (0x19) and trick (0x1A) of a player on the HUD.
        std::function<void(const NetPlayer&)> cruiseScore;
        std::function<void(const NetPlayer&, int32_t trick)> cruiseTrick;
        // A car of the race: its state as 0x40 bytes (0x004E7740), the state applied (0x004E73E0), smoothed towards
        // (0x004E6E30), carried on by its velocities for a time (0x004E6B60), put back on its line (0x00487080), the
        // race's distance (+0x494 +0x98), the model's byte sent each 5 s (+0x430 +0x170) and its setter, gone (the
        // car's +0x4E8 1, its +0x4EC 0).
        std::function<void(void* car, uint8_t* state)> carState;
        std::function<void(void* car, const uint8_t* state)> applyState;
        std::function<void(void* car, const uint8_t* state)> smoothState;
        std::function<void(uint8_t* state, float dt)> extrapolate;
        std::function<void(void* car)> putBack;
        std::function<float(void* car)> carDistance;
        std::function<int32_t(void* car)> carByte;
        std::function<void(void* car, int32_t value)> setCarByte;
        std::function<void(void* car)> carGone;
        // A remote car (the game's kind 2) or our own.
        std::function<bool(void* car)> remote;
        // The race's type (the race rules' +0x04) and whether the race is paused ([G+0x34]+0x20).
        std::function<RaceType()> raceType;
        std::function<bool()> racePaused;
        // The HUD shown ([G+0x3C] not null).
        std::function<bool()> hud;
        // 0x004EBBF0, at the reset's start: the online career's pink slip ended by the race (the game's).
        std::function<void()> resetting;
        // The race's first player ([G+0x34]'s list +0x0C, its +0x04 car) there; `first`: its car finished first (the
        // competitor's +0x118 and +0x104 1). Unset without a race.
        std::function<bool(bool& first)> firstPlayer;
        // The online statistics of the round's end (0x004E7EB0): the career's pink slip stakes set ([G+0x80]+0x90,
        // reported by the stakes' end instead); in a race of type 6 the race's first player's finish (+0x1C taken and
        // made 0; true when it and +0x18 were 0: the reports sent).
        std::function<bool()> careerPinkSlip;
        std::function<bool()> firstPlayerFinishCleared;
    };

    explicit NetSession(Transport& transport);

    Hooks hooks;

    // 0x004E9C40: the session made on the transport (its handler), stage 0, the default place and route.
    void start();
    // 0x004EA4E0: the online career's pink slip ended (0x004EBBF0, `hooks.resetting`); the round's end (0x004E7EB0)
    // with `all` 0, else when our player finished; everything reset (the transport and the matchmaking left), the
    // players dropped.
    void reset(bool all);
    // 0x004EBCF0: hosting, a player (of a peer other than 0) left the race, not silently (+0xDC, +0xEC 0).
    bool playerLeftRace() const;
    // 0x004E9E40: our player added (the name, ":CPU%d" for a second one); false in a session. With the matchmaking
    // and GP's user object, `player` (EDI: the user's local player) is referenced (vt+0x2C) into +0x08 and its user
    // (vt+0x38) into +0x0C; then with the voice ([0x0075F044]) the player added to it (vt+0x18), its chat as the
    // settings' +0x48 ([G+0x64]+0x9C, vt+0x38) and its devices opened of the settings' names (+0x2D4, +0x314: the
    // save system's +0x328, +0x368; vt+0x10).
    bool addOwnPlayer(std::u16string_view name, int32_t cpu, int32_t player = -1,
                      const save::Settings* settings = nullptr);
    // 0x004EA450: host with our players as the slots and `max` the most.
    void host(std::u16string_view sessionName, uint32_t max);
    // 0x004EA4A0: join the session at the address with our players.
    void join(const Address& host);

    // The host's setup, sent to all (0x004EA680 .. 0x004EA820).
    void sendPlace(const std::string& place);
    void sendRoute(const std::string& route);
    void sendLaps(int32_t laps);
    void sendTimeOfDay(int32_t value);
    void sendWeather(int32_t value);
    void sendRaceType(int32_t value);
    void sendDifficulty(int32_t value);
    // 0x004EA850: the stage set and sent (0x0A) with the setup (the round's end first).
    void setStage(SessionStage stage);
    // 0x004EA2E0: our player's car (0x02 to all with its index on this machine).
    void sendCar(const CarConfig& car, int32_t localIndex = 0);
    // 0x004EA370: our player ready or not (0x0B to all with its car).
    void sendReady(int32_t ready, int32_t localIndex);
    // 0x004EA910: the race loaded (the physics' first frame).
    void loaded();
    // 0x004EA990: our player (of the index, Stock problem: the race passes the player's place in the players) finished
    // with its two values (0x13 to all).
    void finished(int32_t raceTimeBits, int32_t bestLapBits, int32_t localIndex = 0);
    // 0x004EAA40: our car put back (bit 4 of the next state).
    void carReset();
    // 0x004AB460, a network cruise's end: the scores (+0xE8) of the players in the race in six places (-1000 for
    // none), each place's "ours" by the player at that place in the players (Stock problem: not the player the score
    // came from when one not racing comes first), sorted, the most first (a bubble sort starting again after each
    // swap); the first place of ours gives our finish (0, 0; 0x004EA990) and its points (10, 6, 4, 3, 1, 0) to our
    // player's total, sent (0x1B).
    void cruiseEnd();
    // 0x004B0A50's places: the players in the race (six at most) with their cruise scores, sorted the most first as
    // above.
    std::vector<std::pair<std::u16string, int32_t>> cruiseBoard() const;
    // 0x19 / 0x1A / 0x1B of our player (0x1B: our player of the local index, its points set).
    void sendCruiseScore(int32_t score);
    void sendTrick(int32_t trick);
    void sendPoints(int32_t points, int32_t localIndex = 0);

    // 0x004EBAE0 each frame: the transport's update, then by the stage.
    void frame(float dt);

    SessionStage stage() const { return m_stage; }
    int32_t stageNumber() const { return m_stageNumber; }
    bool spectator() const { return m_spectator != 0; }
    const std::string& place() const { return m_place; }
    const std::string& route() const { return m_route; }
    int32_t laps() const { return m_laps; }
    int32_t timeOfDay() const { return m_timeOfDay; }
    int32_t weather() const { return m_weather; }
    int32_t raceType() const { return m_raceType; }
    int32_t difficulty() const { return m_difficulty; }
    float raceTime() const { return m_raceTime; }
    float countdownClock() const { return m_clock; }
    // 0x004EAA60: the seconds left of the stage's wait (8: 30 s after the first ready; 12: 63 s after the first
    // finish), 0 without one.
    float waitLeft() const;
    // [0x0075F024]: a stage's wait running.
    bool waiting() const { return m_waiting != 0; }

    std::vector<NetPlayer>& players() { return m_players; }
    const std::vector<NetPlayer>& players() const { return m_players; }
    const std::vector<NetNode>& nodes() const { return m_nodes; }
    NetPlayer* player(int32_t peer, int32_t localIndex);  // 0x004E7C30
    NetPlayer* ownPlayer(int32_t localIndex = 0) { return player(-1, localIndex); }
    uint32_t racing() const;           // 0x004EA020: the players in the race
    uint32_t present() const;          // 0x004EA070: the players not gone
    // 0x004E66E0: the hosted online session's state told to the matchmaking (at the stage's change, the race's start,
    // a machine come or gone, a player dropped).
    void reportStatus();
    // The places of the race's players (0x004EB5B0, [0x00728D40]): their indices by distance, the first first.
    const std::vector<uint32_t>& order() const { return m_order; }

    Transport& transport() { return m_transport; }

private:
    void handle(const uint8_t* data, size_t size, int32_t peer);  // 0x004E96A0
    void infoAsked(const uint8_t* data, int32_t peer);            // 0x004E8AE0
    void infoCame(const uint8_t* data, size_t size, int32_t peer);  // 0x004E8B50
    void stageCame(const uint8_t* data);                          // 0x004E8EE0
    void readyCame(const uint8_t* data, int32_t peer);            // 0x004E8FD0
    void startCame(const uint8_t* data);                          // 0x004E90A0
    void finishCame(const uint8_t* data, int32_t peer);           // 0x004E93A0
    void stateCame(const uint8_t* data, int32_t peer);            // 0x004E9430
    void voiceStatusCame(const uint8_t* data, int32_t peer);      // 0x004E9050
    void voiceRelayCame(const uint8_t* data, int32_t peer);       // 0x004E9980
    void voiceFrame(float dt);                                    // 0x004EB210
    void peerCame(const uint8_t* data);                           // 0x004E87F0
    void peerWent(const uint8_t* data);                           // 0x004E88E0
    void raceTypeCame(int32_t type, uint8_t id);
    void leaderNote(const NetPlayer& p, bool fromLeader);           // 0x004E85C0                  // 0x004E8750
    void sendStart(int32_t peer, int32_t spectator);              // 0x004E83C0
    void sendStageTo(int32_t peer);                               // the 0x0A of 0x004E8B50
    void roundEnd();                                              // 0x004E7EB0
    void periods();                                               // 0x004E8310
    void removePlayer(size_t index);                              // 0x004E7DE0
    void shuffleGrid();                                           // 0x004E9B50
    void startRace();                                             // 0x004EAAA0
    void go();                                                    // 0x004EAD70
    void backToLobby();                                           // 0x004EADB0
    bool teamWon();                                               // 0x004EBD60 (online careers: always 1)
    void carsFrame(float dt);        // 0x004EAF10
    void modelFrame(float dt);       // 0x004EB130
    void placesFrame(float dt);      // 0x004EB5B0
    void infoFrame(float dt);        // 0x004EB2D0
    void waitFrame(float dt);        // 0x004EB400
    void hostFrame(float dt);        // 0x004EB870
    void clientFrame(float dt);      // 0x004EBA00
    void waitNote(int32_t& said, uint32_t seconds);  // 0x004EADE0
    NetNode* node(int32_t peer);     // 0x004E7BF0
    bool hosting() const { return m_transport.hosting(); }
    void sendAll(const uint8_t* data, size_t size, bool reliable) { m_transport.send(-2, data, size, reliable); }

    Transport& m_transport;
    std::vector<NetNode> m_nodes;
    std::vector<NetPlayer> m_players;
    std::vector<uint32_t> m_order;      // [0x00728D40], [0x0075F02C] its count
    SessionStage m_stage = SessionStage::Lobby;  // [0x0075EF90]
    uint32_t m_stageNumber = 0;         // [0x0075EF94]
    int32_t m_spectator = 0;            // [0x0075EF9C]
    std::string m_place;                // [0x0075EFA0]
    std::string m_route;                // [0x0075EFC0]
    int32_t m_laps = 0;                 // [0x0075EFE0]
    int32_t m_timeOfDay = 0;            // [0x0075EFE4]
    int32_t m_weather = 0;              // [0x0075EFE8]
    int32_t m_raceType = 0;             // [0x007191CC]
    int32_t m_difficulty = 0;           // [0x0075EFEC]
    float m_voiceTimer = 0.0f;          // [0x0075EFF4]
    float m_modelTimer = 0.0f;          // [0x0075EFF8]
    float m_placesTimer = 0.0f;         // [0x0075EFFC]
    float m_leaderTimer = 0.0f;         // [0x0075F000]
    float m_infoTimer = 0.0f;           // [0x0075F004]
    float m_afterFinish = 0.0f;         // [0x0075F008]
    float m_syncTimer = 0.0f;           // [0x0075F00C]
    float m_clock = 0.0f;               // [0x0075F010]
    float m_raceTime = 0.0f;            // [0x0075F014]
    int32_t m_said60Left = 0;           // [0x0075F018]: the race's end's note at 60 s left said
    int32_t m_said10Left = 0;           // [0x0075F01C]: and at 10 s left
    float m_waitTimer = 0.0f;           // [0x0075F020]
    int32_t m_waiting = 0;              // [0x0075F024]
    int32_t m_firstFinished = 0;        // [0x0075F028]
};

// The game's network ([0x0075EF8C] and the session's globals): the transport ([0x0075F758], 0x00599ED0) and the
// session on it.
struct NetGame {
    Transport transport;
    NetSession session{transport};
};

// The network game; none outside one ([0x0075EF8C] 0).
NetGame* netGame();

// [0x00728D58] .. [0x00728D7C] and [0x0075EF98]: the players met lately (the user object's user records, each
// referenced), the latest first, ten at most; the session's reset lets them go.
struct RecentPlayers {
    std::array<int32_t, 10> users{};
    uint32_t count = 0;
};
RecentPlayers& recentPlayers();
// 0x004E7C90: with the user object, the one of the user's profile (vt+0x18) taken off the list (the rest moved up,
// the last place -1) and let go (vt+0x14).
void recentRemove(int32_t user);
// 0x004E7D30: with the user object, the user taken off (0x004E7C90); with ten the last let go (the statistics'
// 0x004EE050 first); the others moved down and the user referenced (vt+0x10) first.
void recentAdd(int32_t user);
// The same as a reference kept (the matchmaking's of the transport).
std::shared_ptr<NetGame> sharedNetGame();
// The transport's options the game opens with: the stock ports, the machine's own address and the broadcast
// (OpenJuice: other ports may be given, for two games on one machine).
Transport::Options& netOptions();
// 0x004E9C40: a network game made anew (the one before ended, 0x004E9DD0): the transport opened (0x00599ED0) and the
// session started (its handler, the stage 0, the default place and route). A transport that cannot open (no Winsock,
// a port taken) is kept closed: it sends and hears nothing.
NetGame& startNetGame(CrtRandom& random);
// 0x004E9DD0: the session reset (0x004EA4E0 with 0) and the transport let go.
void endNetGame();

}  // namespace oj::net
