#include "Core/Network/NetSession.h"

#include "Core/Cars/Car.h"
#include "Core/Base/Log.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/OnlineRecords.h"
#include "Core/Network/OnlineSessions.h"
#include "Core/Saves/SaveEntries.h"
#include "Core/Network/VoiceChat.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace oj::net {

namespace {

// The game's messages (ids under 0xD2).
enum Message : uint8_t {
    kInfoAsked = 0x00, kInfo = 0x01, kCar = 0x02, kPlace = 0x03, kRoute = 0x04, kLaps = 0x05, kTimeOfDay = 0x06,
    kWeather = 0x07, kRaceType = 0x08, kDifficulty = 0x09, kStage = 0x0A, kReady = 0x0B, kVoiceStatus = 0x0C,
    kStart = 0x0E, kLoaded = 0x0F, kGo = 0x10, kClock = 0x11, kClockAnswer = 0x12, kFinish = 0x13, kState = 0x14,
    kModelByte = 0x15, kPlaceOf = 0x17, kLeader = 0x18, kCruiseScore = 0x19, kTrick = 0x1A, kPoints = 0x1B,
    kPeerGone = 0xDE, kPeerCame = 0xDF,
    // The fixed build's own (ids the stock's session passes over): a machine of the fixed build, a car's contact.
    kFixedHello = 0x30, kContact = 0x31,
};

// The fixed build's hello: its mark and the version of what its machines tell each other.
constexpr uint8_t kFixedMark[4] = {'O', 'J', 'F', 'X'};
constexpr uint32_t kFixedVersion = 1;

int32_t i32(const uint8_t* p) {
    int32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

uint32_t u32(const uint8_t* p) {
    uint32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

float f32(const uint8_t* p) {
    float v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

void put32(uint8_t* p, uint32_t v) {
    std::memcpy(p, &v, 4);
}

void putI(uint8_t* p, int32_t v) {
    std::memcpy(p, &v, 4);
}

void putF(uint8_t* p, float v) {
    std::memcpy(p, &v, 4);
}

// A narrow string of `n` bytes at most (the end in it): strncpy and the last byte 0.
std::string narrow(const uint8_t* p, size_t n) {
    size_t len = 0;
    while (len + 1 < n && p[len] != 0) ++len;
    return std::string(reinterpret_cast<const char*>(p), len);
}

void putNarrow(uint8_t* p, const std::string& s, size_t n) {
    std::memset(p, 0, n);
    std::memcpy(p, s.data(), std::min(s.size(), n - 1));
}

// A wide name of 26 characters at most from narrow bytes (each byte a character, signed as the stock widens).
std::u16string widen(const uint8_t* p, size_t n) {
    std::u16string s;
    for (size_t i = 0; i < n && p[i] != 0; ++i) {
        s.push_back(static_cast<char16_t>(static_cast<int16_t>(static_cast<int8_t>(p[i]))));
    }
    return s;
}

void putPlayerName(uint8_t* p, const std::u16string& name, size_t n) {
    std::memset(p, 0, n);
    for (size_t i = 0; i < name.size() && i + 1 < n; ++i) p[i] = static_cast<uint8_t>(name[i]);
}

void putRace(uint8_t* p, const PlayerRace& r) {
    const int32_t v[13] = {r.notRacing, r.ready,  r.gridPlace, r.distance, r.finished, r.finishTime, r.finishBestLap,
                           r.place,     r.left,   r.points,    r.races,    r.cruiseScore, r.leftHow};
    std::memcpy(p, v, sizeof v);
}

PlayerRace readRace(const uint8_t* p) {
    PlayerRace r;
    int32_t v[13];
    std::memcpy(v, p, sizeof v);
    r.notRacing = v[0];
    r.ready = v[1];
    r.gridPlace = v[2];
    r.distance = v[3];
    r.finished = v[4];
    r.finishTime = v[5];
    r.finishBestLap = v[6];
    r.place = v[7];
    r.left = v[8];
    r.points = v[9];
    r.races = v[10];
    r.cruiseScore = v[11];
    r.leftHow = v[12];
    return r;
}

// An online career's pink slip race (the matchmaking there, [0x0075EF88] 1 and [0x00728D00] set): two players, the
// host's readiness starts no wait (0x004EA370, 0x004E8FD0).
bool oneOnOne() {
    return matchmaking() != nullptr && onlineHosted().mode == OnlineKind::Career && onlineHosted().career.pinkslip != 0;
}

}  // namespace

// ---- The car's config.

CarConfig::CarConfig() {
    body.fill(0xFF);
    mods.fill(0xFF);
}

void CarConfig::write(uint8_t* out) const {
    std::memset(out, 0, 0x74);
    std::memcpy(out, name.data(), name.size());
    std::memcpy(out + 0x18, body.data(), 10);
    std::memcpy(out + 0x22, mods.data(), 10);
    const uint32_t v[18] = {neon,        base,     metallic,      pearl,     decalColour,    sponsorColour,
                            sponsorDecals, metallicAmount, pearlAmount, dialColour, dialTextColour, wheelColour,
                            wheelsColoured, tint,  lowering,      gearing,   torqueFactor, horn};
    std::memcpy(out + 0x2C, v, sizeof v);
}

CarConfig CarConfig::read(const uint8_t* in) {
    CarConfig c;
    std::memcpy(c.name.data(), in, c.name.size());
    std::memcpy(c.body.data(), in + 0x18, 10);
    std::memcpy(c.mods.data(), in + 0x22, 10);
    uint32_t v[18];
    std::memcpy(v, in + 0x2C, sizeof v);
    c.neon = v[0];
    c.base = v[1];
    c.metallic = v[2];
    c.pearl = v[3];
    c.decalColour = v[4];
    c.sponsorColour = v[5];
    c.sponsorDecals = v[6];
    c.metallicAmount = v[7];
    c.pearlAmount = v[8];
    c.dialColour = v[9];
    c.dialTextColour = v[10];
    c.wheelColour = v[11];
    c.wheelsColoured = v[12];
    c.tint = v[13];
    c.lowering = v[14];
    c.gearing = v[15];
    c.torqueFactor = v[16];
    c.horn = v[17];
    return c;
}

CarConfig CarConfig::of(const Car& car, const std::string& defName) {
    CarConfig c;
    c.name.fill(0);
    std::memcpy(c.name.data(), defName.data(), std::min(defName.size(), c.name.size() - 1));
    c.body = car.body;
    c.mods = car.mods;
    c.neon = car.neon;
    c.base = car.base;
    c.metallic = car.metallic;
    c.pearl = car.pearl;
    c.decalColour = car.decalColour;
    c.sponsorColour = car.sponsorColour;
    c.sponsorDecals = car.sponsorDecals;
    c.metallicAmount = car.metallicAmount;
    c.pearlAmount = car.pearlAmount;
    c.dialColour = car.dialColour;
    c.dialTextColour = car.dialTextColour;
    c.wheelColour = car.wheelColour;
    c.wheelsColoured = car.wheelsColoured;
    c.tint = car.tint;
    c.lowering = car.lowering;
    c.gearing = car.gearing;
    c.torqueFactor = car.torqueFactor;
    c.horn = car.horn;
    return c;
}

void CarConfig::apply(Car& car) const {
    car.body = body;
    car.mods = mods;
    car.neon = neon;
    car.base = base;
    car.metallic = metallic;
    car.pearl = pearl;
    car.decalColour = decalColour;
    car.sponsorColour = sponsorColour;
    car.sponsorDecals = sponsorDecals;
    car.metallicAmount = metallicAmount;
    car.pearlAmount = pearlAmount;
    car.dialColour = dialColour;
    car.dialTextColour = dialTextColour;
    car.wheelColour = wheelColour;
    car.wheelsColoured = wheelsColoured;
    car.tint = tint;
    car.lowering = lowering;
    car.gearing = gearing;
    car.torqueFactor = torqueFactor;
    car.horn = horn;
}

// ---- The session.

NetSession::NetSession(Transport& transport) : m_transport(transport) {}

void NetSession::start() {
    m_transport.setHandler([this](const uint8_t* data, size_t size, int32_t peer) { handle(data, size, peer); });
    // The fixed build pings the peers (their one way time for the cars' states; the stock's pings were never on).
    m_transport.setPinging(true);
    m_transport.setRelayHandler([this](const uint8_t* data, size_t size, int32_t peer) {
        if (size >= 0x0C) voiceRelayCame(data, peer);
    });
    m_voiceTimer = 0.0f;
    m_stage = SessionStage::Lobby;
    m_stageNumber = 0;
    m_place = "EAST ANGEL ISLAND";
    m_route = "EAST_ANGEL_R1";
    m_spectator = 0;
    m_laps = 0;
    m_modelTimer = m_placesTimer = m_leaderTimer = m_infoTimer = m_afterFinish = m_syncTimer = 0.0f;
    m_clock = m_raceTime = 0.0f;
    m_said60Left = m_said10Left = 0;
    m_waitTimer = 0.0f;
    m_waiting = 0;
    m_firstFinished = 0;
}

NetPlayer* NetSession::player(int32_t peer, int32_t localIndex) {
    for (NetPlayer& p : m_players) {
        if (p.peer == peer && p.localIndex == localIndex) return &p;
    }
    return nullptr;
}

NetNode* NetSession::node(int32_t peer) {
    for (NetNode& n : m_nodes) {
        if (n.peer == peer) return &n;
    }
    return nullptr;
}

uint32_t NetSession::racing() const {
    uint32_t n = 0;
    for (const NetPlayer& p : m_players) {
        if (p.race.notRacing == 0) ++n;
    }
    return n;
}

uint32_t NetSession::present() const {
    uint32_t n = 0;
    for (const NetPlayer& p : m_players) {
        if (p.race.left == 0) ++n;
    }
    return n;
}

float NetSession::waitLeft() const {
    if (m_waiting != 0) {
        if (m_stage == SessionStage::Cars) return 30.0f - m_waitTimer;
        if (m_stage == SessionStage::Racing) return 63.0f - m_waitTimer;
    }
    return 0.0f;
}

bool NetSession::addOwnPlayer(std::u16string_view name, int32_t cpu, int32_t player, const save::Settings* settings) {
    if (m_transport.inSession()) return false;
    NetPlayer p;
    p.peer = -1;
    p.localIndex = static_cast<int32_t>(m_players.size());
    std::u16string n(name.substr(0, 15));
    if (cpu != 0) {
        const std::string tag = ":CPU" + std::to_string(cpu);
        for (char c : tag) n.push_back(static_cast<char16_t>(c));
    }
    p.name = n.substr(0, 25);
    p.infoKnown = 1;
    m_players.push_back(std::move(p));
    if (matchmaking() != nullptr) {
        if (OnlineUser* user = onlineUser()) {
            NetPlayer& own = m_players.back();
            user->playerAddRef(player);
            own.gpPlayer = player;
            own.profile = user->playerUser(player);
            if (VoiceChat* voice = gameVoice()) {
                voice->addPlayer(player);
                const uint32_t chat = settings != nullptr ? settings->u32(save::Settings::kVoiceChat) : 0;
                voice->setChat(player, static_cast<int32_t>(chat));
                char capture[0x41]{};
                char playback[0x41]{};
                if (settings != nullptr) {
                    std::memcpy(capture, settings->object.data() + 0x2D4, 0x40);
                    std::memcpy(playback, settings->object.data() + 0x314, 0x40);
                }
                voice->openDevices(capture, playback, player);
            }
        }
    }
    return true;
}

void NetSession::host(std::u16string_view sessionName, uint32_t max) {
    m_transport.host(sessionName, static_cast<uint32_t>(m_players.size()), max);
}

void NetSession::join(const Address& hostAddress) {
    m_transport.join(hostAddress, static_cast<uint32_t>(m_players.size()));
}

void NetSession::reset(bool all) {
    if (hooks.resetting) hooks.resetting();
    const NetPlayer* own = ownPlayer();
    if (!all || (own != nullptr && own->race.finished != 0)) roundEnd();
    m_transport.leave();
    if (Matchmaking* mm = matchmaking()) mm->leave();
    m_nodes.clear();
    m_fixedPeers.clear();
    while (!m_players.empty()) removePlayer(0);
    m_spectator = 0;
    m_stage = SessionStage::Lobby;
    m_stageNumber = 0;
    m_modelTimer = m_placesTimer = m_leaderTimer = m_infoTimer = m_afterFinish = m_syncTimer = 0.0f;
    m_clock = m_raceTime = 0.0f;
    m_said60Left = m_said10Left = 0;
    m_waitTimer = 0.0f;
    m_waiting = 0;
    m_firstFinished = 0;
    // With the user object the recent players let go (each with its record first, 0x004EE050, with the statistics)
    // and the local players' state 0 (0x004EEAF0, not public).
    if (OnlineUser* user = onlineUser()) {
        RecentPlayers& recent = recentPlayers();
        for (uint32_t i = 0; i < recent.count; ++i) {
            if (stats::gameStatistics() != nullptr) stats::releaseRecord(recent.users[i]);
            user->userRelease(recent.users[i]);
        }
        recent.count = 0;
        setOnlineUsersState(0, 0);
    }
}

bool NetSession::playerLeftRace() const {
    if (!m_transport.hosting()) return false;
    bool left = false;
    for (const NetPlayer& p : m_players) {
        if (p.peer != 0 && p.race.left != 0 && p.race.leftHow == 0) left = true;
    }
    return left;
}

RecentPlayers& recentPlayers() {
    static RecentPlayers recent;
    return recent;
}

void recentRemove(int32_t user) {
    OnlineUser* u = onlineUser();
    if (u == nullptr || user == -1) return;
    RecentPlayers& r = recentPlayers();
    uint32_t i = 0;
    while (i < r.count && !u->sameUser(r.users[i], user)) ++i;
    if (i == r.count) return;
    u->userRelease(r.users[i]);
    for (uint32_t k = i; k + 1 < r.count; ++k) r.users[k] = r.users[k + 1];
    r.users[r.count - 1] = -1;
    --r.count;
}

void recentAdd(int32_t user) {
    OnlineUser* u = onlineUser();
    if (u == nullptr || user == -1) return;
    recentRemove(user);
    RecentPlayers& r = recentPlayers();
    if (r.count == 10) {
        if (stats::gameStatistics() != nullptr) stats::releaseRecord(r.users[9]);  // 0x004EE050
        u->userRelease(r.users[9]);
        --r.count;
    }
    for (uint32_t k = r.count; k != 0; --k) r.users[k] = r.users[k - 1];
    u->userAddRef(user);
    ++r.count;
    r.users[0] = user;
    if (9 < r.count) r.count = 10;
}

void NetSession::removePlayer(size_t index) {
    // 0x004E7DE0: with the user object a player of another machine with a user (+0x0C) goes to the recent players
    // (0x004E7D30); the voice ([0x0075F044]): ours taken off it with its GP player (vt+0x1C), another's user
    // (vt+0x28); with the user object its GP player (+0x08, vt+0x30) and its user (+0x0C, vt+0x14) let go; the last
    // moved into its place.
    const NetPlayer& p = m_players[index];
    OnlineUser* user = onlineUser();
    if (user != nullptr && p.peer != -1 && p.profile != -1) recentAdd(p.profile);
    if (VoiceChat* voice = gameVoice()) {
        if (p.peer == -1) {
            voice->removePlayer(p.gpPlayer);
        } else {
            voice->removeUser(p.profile);
        }
    }
    if (user != nullptr) {
        user->playerRelease(p.gpPlayer);
        user->userRelease(p.profile);
    }
    m_players[index] = std::move(m_players.back());
    m_players.pop_back();
    m_order.clear();
    reportStatus();
}

// 0x004E9050 (0x0C): the event of the sender's player ([4] its index) to ours ([8]): the voice's +0x3C with our
// player's GP player, the sender's user and the kind ([0x0C]).
void NetSession::voiceStatusCame(const uint8_t* data, int32_t peer) {
    NetPlayer* from = player(peer, i32(data + 4));
    NetPlayer* to = player(-1, i32(data + 8));
    VoiceChat* voice = gameVoice();
    if (voice != nullptr && from != nullptr && to != nullptr) voice->userEvent(to->gpPlayer, from->profile, i32(data + 0x0C));
}

// 0x004E9980, the transport's second handler (never called): [2] 0x0D a player's ([4] its index) voice bytes ([8] their
// count, from [0x0A]) to the voice's +0x40 with its user; 0x14 (0x004E9590) the state of the message from [2]
// (0x004E9430), then its voice bytes ([0x4C] their count, from [0x4E]) the same.
void NetSession::voiceRelayCame(const uint8_t* data, int32_t peer) {
    const auto give = [&](uint16_t count, const uint8_t* bytes) {
        NetPlayer* p = player(peer, i32(data + 4));
        VoiceChat* voice = gameVoice();
        if (p != nullptr && voice != nullptr && count != 0) voice->userData(p->profile, bytes, count);
    };
    if (data[2] == 0x0D) {
        give(static_cast<uint16_t>(data[8] | data[9] << 8), data + 0x0A);
    } else if (data[2] == 0x14) {
        stateCame(data + 2, peer);
        give(static_cast<uint16_t>(data[0x4C] | data[0x4D] << 8), data + 0x4E);
    }
}

// 0x004EB210, each frame outside the race: past 0.2 s ([0x006FF934]) 0, and each player's voice bytes (+0x154)
// relayed to every peer (0A 00 0D, its index, their count, them: their count + 0x0C bytes), none left. Nothing fills
// them on the PC.
void NetSession::voiceFrame(float dt) {
    m_voiceTimer = m_voiceTimer + dt;
    if (!(m_voiceTimer > 0.2f)) return;
    m_voiceTimer = 0.0f;
    for (NetPlayer& p : m_players) {
        if (p.voiceBytes == 0) continue;
        std::array<uint8_t, 0x70> relay{0x0A, 0x00, 0x0D};
        putI(relay.data() + 4, p.localIndex);
        relay[8] = static_cast<uint8_t>(p.voiceBytes);
        relay[9] = static_cast<uint8_t>(p.voiceBytes >> 8);
        std::memcpy(relay.data() + 0x0A, p.voiceData.data(), p.voiceData.size());
        m_transport.sendRelay(-2, relay.data(), std::min<size_t>(static_cast<size_t>(p.voiceBytes) + 0x0C, relay.size()));
        p.voiceBytes = 0;
    }
}

void NetSession::reportStatus() {
    // 0x004E66E0: hosting with a matchmaking, its info (the hosted session's name, playing from the stage 9, the mode)
    // and its counts (the players not gone, the most less them: 2 in a one-on-one career, else 6) with the attribute
    // 1 (playing), the change sent.
    Matchmaking* mm = matchmaking();
    if (mm == nullptr || !m_transport.hosting()) return;
    MatchAttribute playing;
    playing.id = 1;
    playing.type = 1;
    playing.value = m_stage >= SessionStage::Loading ? 1 : 0;
    const OnlineHosted& hosted = onlineHosted();
    const bool career = hosted.mode == OnlineKind::Career;
    mm->setInfo(nameText(career ? hosted.career.name : hosted.arcade.name), m_stage >= SessionStage::Loading ? 1 : 0,
                career ? u"Career" : u"Arcade");
    const uint32_t players = present();
    const uint32_t most = career && hosted.career.pinkslip != 0 ? 2 : 6;
    const MatchAttribute* attributes[1] = {&playing};
    mm->update(players, most - players, 1, attributes, m_stage < SessionStage::Loading ? 1 : 0);
}

// ---- The setup.

void NetSession::sendPlace(const std::string& place) {
    m_place = place.substr(0, 31);
    uint8_t q[0x21] = {kPlace};
    putNarrow(q + 1, m_place, 32);
    sendAll(q, sizeof q, true);
}

void NetSession::sendRoute(const std::string& route) {
    m_route = route.substr(0, 31);
    uint8_t q[0x21] = {kRoute};
    putNarrow(q + 1, m_route, 32);
    sendAll(q, sizeof q, true);
}

void NetSession::sendLaps(int32_t laps) {
    m_laps = laps;
    uint8_t q[8] = {kLaps};
    putI(q + 4, laps);
    sendAll(q, sizeof q, true);
}

void NetSession::sendTimeOfDay(int32_t value) {
    m_timeOfDay = value;
    uint8_t q[8] = {kTimeOfDay};
    putI(q + 4, value);
    sendAll(q, sizeof q, true);
}

void NetSession::sendWeather(int32_t value) {
    m_weather = value;
    uint8_t q[8] = {kWeather};
    putI(q + 4, value);
    sendAll(q, sizeof q, true);
}

void NetSession::sendRaceType(int32_t value) {
    m_raceType = value;
    uint8_t q[8] = {kRaceType};
    putI(q + 4, value);
    sendAll(q, sizeof q, true);
}

void NetSession::sendDifficulty(int32_t value) {
    m_difficulty = value;
    uint8_t q[8] = {kDifficulty};
    putI(q + 4, value);
    sendAll(q, sizeof q, true);
}

void NetSession::setStage(SessionStage stage) {
    roundEnd();
    ++m_stageNumber;
    m_stage = stage;
    sendStageTo(-2);
    reportStatus();
}

void NetSession::sendStageTo(int32_t peer) {
    uint8_t q[0x60] = {kStage};
    putI(q + 4, static_cast<int32_t>(m_stage));
    put32(q + 8, m_stageNumber);
    putNarrow(q + 0x0C, m_place, 32);
    putNarrow(q + 0x2C, m_route, 32);
    putI(q + 0x4C, m_laps);
    putI(q + 0x50, m_timeOfDay);
    putI(q + 0x54, m_weather);
    putI(q + 0x58, m_raceType);
    putI(q + 0x5C, m_difficulty);
    m_transport.send(peer, q, sizeof q, true);
}

void NetSession::sendCar(const CarConfig& car, int32_t localIndex) {
    NetPlayer* own = ownPlayer(localIndex);
    if (own == nullptr) return;
    own->car = car;
    uint8_t q[0x7C] = {kCar};
    putI(q + 4, localIndex);
    own->car.write(q + 8);
    sendAll(q, sizeof q, true);
}

void NetSession::sendReady(int32_t ready, int32_t localIndex) {
    NetPlayer* own = ownPlayer(localIndex);
    if (own == nullptr) return;
    if (hosting()) {
        // The host's readiness starts the wait of 30 s (not in an online career's pink slip race of two, [0x00728D00]).
        if (ready == 0 || oneOnOne()) {
            m_waiting = 0;
        } else if (own->race.ready == 0) {
            m_waiting = 1;
            m_waitTimer = 0.0f;
        }
    }
    own->race.ready = ready;
    uint8_t q[0x80] = {kReady};
    putI(q + 4, localIndex);
    putI(q + 8, ready);
    own->car.write(q + 0x0C);
    sendAll(q, sizeof q, true);
}

void NetSession::loaded() {
    if (m_stage <= SessionStage::Cars) return;
    if (m_spectator != 0) {
        m_stage = SessionStage::Racing;
        return;
    }
    m_stage = SessionStage::Loaded;
    if (!hosting()) {
        const uint8_t q[1] = {kLoaded};
        m_transport.send(0, q, sizeof q, true);
    }
    m_waiting = 1;
    m_waitTimer = 0.0f;
}

void NetSession::leaderNote(const NetPlayer& p, bool fromLeader) {
    // 0x004E85C0: the leader's timer (20 s) again; the note only with the HUD up, the player not gone, no one
    // finished, past 10 s of the race and not in a cruise.
    m_leaderTimer = 0.0f;
    if (!(hooks.hud && hooks.hud()) || p.race.left != 0 || m_firstFinished != 0 || !(10.0f < m_raceTime)) return;
    if (hooks.raceType && hooks.raceType() == RaceType::Cruise) return;
    if (hooks.leader) hooks.leader(p, fromLeader);
}

void NetSession::finished(int32_t raceTimeBits, int32_t bestLapBits, int32_t localIndex) {
    NetPlayer* own = ownPlayer(localIndex);
    if (own == nullptr) return;
    if (m_waiting == 0 && teamWon()) {
        m_waiting = 1;
        m_waitTimer = 0.0f;
    }
    own->race.finishTime = raceTimeBits;
    own->race.finishBestLap = bestLapBits;
    own->race.finished = 1;
    // Stock problem: [4] (the player's index the receivers look it up by) is never set, whatever the stack held;
    // 0 here (the one player of a machine).
    uint8_t q[0x10] = {kFinish};
    putI(q + 8, raceTimeBits);
    putI(q + 0x0C, bestLapBits);
    sendAll(q, sizeof q, true);
    if (m_firstFinished == 0) {
        if (hooks.firstFinished) hooks.firstFinished(*own);
        m_firstFinished = 1;
    }
}

void NetSession::carReset() {
    if (NetPlayer* own = ownPlayer()) own->reset = 1;
}

void NetSession::sendCruiseScore(int32_t score) {
    // 0x004AA730 / 0x004AAAB0: our player's score and 0x19 to all, not reliable, [4] 0.
    NetPlayer* own = ownPlayer();
    if (own == nullptr) return;
    own->race.cruiseScore = score;
    uint8_t q[0x0C] = {kCruiseScore};
    putI(q + 8, score);
    sendAll(q, sizeof q, false);
}

void NetSession::cruiseEnd() {
    std::array<int32_t, 6> scores{};
    std::array<int32_t, 6> ours{};
    scores.fill(-1000);
    for (size_t i = 0; i < 6 && i < m_players.size(); ++i) ours[i] = m_players[i].peer == -1 ? 1 : 0;
    size_t n = 0;
    for (const NetPlayer& p : m_players) {
        if (p.race.notRacing == 0 && n < 6) scores[n++] = p.race.cruiseScore;
    }
    for (size_t i = 0; i < 5; ++i) {
        if (scores[i] < scores[i + 1]) {
            std::swap(scores[i], scores[i + 1]);
            std::swap(ours[i], ours[i + 1]);
            i = static_cast<size_t>(-1);
        }
    }
    static constexpr int32_t kPoints[6] = {10, 6, 4, 3, 1, 0};
    for (size_t i = 0; i < 6; ++i) {
        if (ours[i] == 0) continue;
        finished(0, 0);
        if (NetPlayer* own = ownPlayer()) sendPoints(own->race.points + kPoints[i]);
        return;
    }
}

std::vector<std::pair<std::u16string, int32_t>> NetSession::cruiseBoard() const {
    std::vector<std::pair<std::u16string, int32_t>> board;
    for (const NetPlayer& p : m_players) {
        if (p.race.notRacing == 0 && board.size() < 6) board.emplace_back(p.name, p.race.cruiseScore);
    }
    while (board.size() < 6) board.emplace_back(std::u16string(), -1000);
    for (size_t i = 0; i < 5; ++i) {
        if (board[i].second < board[i + 1].second) {
            std::swap(board[i], board[i + 1]);
            i = static_cast<size_t>(-1);
        }
    }
    return board;
}

void NetSession::sendTrick(int32_t trick) {
    NetPlayer* own = ownPlayer();
    if (own == nullptr) return;
    uint8_t q[0x0C] = {kTrick};
    putI(q + 4, own->localIndex);
    putI(q + 8, trick);
    sendAll(q, sizeof q, true);
}

void NetSession::sendPoints(int32_t points, int32_t localIndex) {
    NetPlayer* own = ownPlayer(localIndex);
    if (own == nullptr) return;
    own->race.points = points;
    uint8_t q[0x0C] = {kPoints};
    putI(q + 4, own->localIndex);
    putI(q + 8, points);
    sendAll(q, sizeof q, true);
}

// ---- The messages.

void NetSession::handle(const uint8_t* data, size_t size, int32_t peer) {
    if (size == 0) return;
    switch (data[0]) {
    case kInfoAsked:
        if (size >= 8) infoAsked(data, peer);
        return;
    case kInfo:
        if (size >= 0x58) infoCame(data, size, peer);
        return;
    case kCar:
        if (size >= 0x7C) {
            if (NetPlayer* p = player(peer, i32(data + 4))) p->car = CarConfig::read(data + 8);
        }
        return;
    case kPlace:
        if (size >= 0x21) {
            m_place = narrow(data + 1, 32);
            if (hooks.setupChanged) hooks.setupChanged(data[0]);
        }
        return;
    case kRoute:
        if (size >= 0x21) {
            m_route = narrow(data + 1, 32);
            if (hooks.setupChanged) hooks.setupChanged(data[0]);
        }
        return;
    case kLaps:
        if (size >= 8) m_laps = i32(data + 4);
        if (hooks.setupChanged) hooks.setupChanged(data[0]);
        return;
    case kTimeOfDay:
        if (size >= 8) m_timeOfDay = i32(data + 4);
        if (hooks.setupChanged) hooks.setupChanged(data[0]);
        return;
    case kWeather:
        if (size >= 8) m_weather = i32(data + 4);
        if (hooks.setupChanged) hooks.setupChanged(data[0]);
        return;
    case kRaceType:
        if (size >= 8) raceTypeCame(i32(data + 4), kRaceType);
        return;
    case kDifficulty:
        if (size >= 8) m_difficulty = i32(data + 4);
        if (hooks.setupChanged) hooks.setupChanged(data[0]);
        return;
    case kStage:
        if (size >= 0x60) stageCame(data);
        return;
    case kReady:
        if (size >= 0x80) readyCame(data, peer);
        return;
    case kVoiceStatus:
        if (size >= 0x10) voiceStatusCame(data, peer);
        return;
    case kStart:
        if (size >= 0x444) startCame(data);
        return;
    case kLoaded:
        // 0x004E92E0: the machine loaded; past the count's start told to go.
        if (NetNode* n = node(peer)) {
            n->loaded = 1;
            if (m_stage > SessionStage::Loaded) {
                const uint8_t q[1] = {kGo};
                m_transport.send(n->peer, q, sizeof q, true);
            }
        }
        return;
    case kGo:
        m_stage = SessionStage::Countdown;
        m_waiting = 0;
        return;
    case kClock:
        // 0x004E9320: the host's clock back with the asker's.
        if (m_stage == SessionStage::Countdown && size >= 8) {
            uint8_t q[0x0C] = {kClockAnswer};
            std::memcpy(q + 4, data + 4, 4);
            putF(q + 8, m_clock);
            m_transport.send(peer, q, sizeof q, false);
        }
        return;
    case kClockAnswer:
        // 0x004E9370: half the way there taken as the trip.
        if (m_stage == SessionStage::Countdown && size >= 0x0C) {
            m_clock = ((m_clock - f32(data + 4)) * 0.5f + f32(data + 8) + m_clock) * 0.5f;
        }
        return;
    case kFinish:
        if (size >= 0x10) finishCame(data, peer);
        return;
    case kState:
        if (size >= 0x4C) stateCame(data, peer);
        return;
    case kModelByte:
        // 0x004E95E0
        if (size >= 0x0C) {
            if (NetPlayer* p = player(peer, i32(data + 4)); p != nullptr && p->raceCar != nullptr && hooks.setCarByte) {
                hooks.setCarByte(p->raceCar, data[8]);
            }
        }
        return;
    case kPlaceOf:
        // 0x004E9610: our player's place as the host tells it.
        if (size >= 0x0C) {
            if (NetPlayer* p = player(-1, i32(data + 4))) p->race.place = i32(data + 8);
        }
        return;
    case kLeader:
        // 0x004E9630: the leader, [4] its peer as the receiver knows it (-1 itself, 0 the host), [8] its index.
        if (size >= 0x0C) {
            if (NetPlayer* p = player(i32(data + 4), data[8])) leaderNote(*p, true);
        }
        return;
    case kCruiseScore:
        if (size >= 0x0C) {
            if (NetPlayer* p = player(peer, i32(data + 4))) {
                p->race.cruiseScore = i32(data + 8);
                if (hooks.cruiseScore) hooks.cruiseScore(*p);
            }
        }
        return;
    case kTrick:
        if (size >= 0x0C) {
            if (NetPlayer* p = player(peer, i32(data + 4)); p != nullptr && hooks.cruiseTrick) {
                hooks.cruiseTrick(*p, i32(data + 8));
            }
        }
        return;
    case kPoints:
        if (size >= 0x0C) {
            if (NetPlayer* p = player(peer, i32(data + 4))) p->race.points = i32(data + 8);
        }
        return;
    case kPeerGone:
        if (size >= 0x0C) peerWent(data);
        return;
    case kPeerCame:
        if (size >= 0x0C) peerCame(data);
        return;
    case kFixedHello:
        // The fixed build: a peer of the fixed build (the protocol's version ours); told so back once.
        if (size >= 0x0C && std::memcmp(data + 4, kFixedMark, 4) == 0 && u32(data + 8) == kFixedVersion &&
            !fixedPeer(peer)) {
            m_fixedPeers.push_back(peer);
            uint8_t q[0x0C] = {kFixedHello};
            std::memcpy(q + 4, kFixedMark, 4);
            put32(q + 8, kFixedVersion);
            m_transport.send(peer, q, sizeof q, true);
        }
        return;
    case kContact:
        if (size >= 0x20 && fixedPeer(peer) && hooks.contact) {
            const std::array<float, 3> linear = {f32(data + 8), f32(data + 0x0C), f32(data + 0x10)};
            const std::array<float, 3> angular = {f32(data + 0x14), f32(data + 0x18), f32(data + 0x1C)};
            hooks.contact(peer, i32(data + 4), linear, angular);
        }
        return;
    default:
        return;  // 0x0D (voice), 0x16 and the others: nothing
    }
}

bool NetSession::fixedPeer(int32_t peer) const {
    return std::find(m_fixedPeers.begin(), m_fixedPeers.end(), peer) != m_fixedPeers.end();
}

bool NetSession::sendContact(int32_t peer, int32_t localIndex, const std::array<float, 3>& linear,
                             const std::array<float, 3>& angular) {
    if (!fixedPeer(peer)) return false;
    uint8_t q[0x20] = {kContact};
    putI(q + 4, localIndex);
    for (size_t i = 0; i < 3; ++i) {
        putF(q + 8 + i * 4, linear[i]);
        putF(q + 0x14 + i * 4, angular[i]);
    }
    return m_transport.send(peer, q, sizeof q, false);
}

void NetSession::infoAsked(const uint8_t* data, int32_t peer) {
    // 0x004E8AE0 → 0x004E7B10: our player of the index asked: its name, its 13 values; with the matchmaking and the
    // user object, of a player with a user (+0x0C), +0x58 1 and the user's record (vt+0x24, its size vt+0x20: 0x48
    // bytes: the references, the profile, the nick of 32 characters) at +0x5C, else +0x58 0.
    const NetPlayer* own = player(-1, i32(data + 4));
    if (own == nullptr) return;
    uint8_t q[0xDC] = {kInfo};
    putI(q + 4, own->localIndex);
    putPlayerName(q + 8, own->name, 28);
    putRace(q + 0x24, own->race);
    OnlineUser* user = onlineUser();
    if (matchmaking() != nullptr && user != nullptr && own->profile != -1) {
        put32(q + 0x58, 1);
        const OnlineUserRecord& record = user->record(own->profile);
        putI(q + 0x5C, record.refs);
        putI(q + 0x60, record.profile);
        for (size_t i = 0; i < record.nick.size() && i < 32; ++i) {
            q[0x64 + i * 2] = static_cast<uint8_t>(record.nick[i]);
            q[0x65 + i * 2] = static_cast<uint8_t>(record.nick[i] >> 8);
        }
    } else {
        put32(q + 0x58, 0);
    }
    m_transport.send(peer, q, sizeof q, true);
}

void NetSession::infoCame(const uint8_t* data, size_t size, int32_t peer) {
    (void)size;
    NetPlayer* p = player(peer, i32(data + 4));
    if (p == nullptr || p->infoKnown != 0) return;
    p->infoKnown = 1;
    p->name = widen(data + 8, 26);
    p->race = readRace(data + 0x24);
    if (hosting()) {
        if (m_stage < SessionStage::Loading) {
            sendStageTo(p->peer);
        } else {
            sendStart(p->peer, 1);
        }
    }
    if (const NetPlayer* own = ownPlayer()) {
        uint8_t q[0x7C] = {kCar};
        own->car.write(q + 8);
        m_transport.send(p->peer, q, sizeof q, true);
    }
    // With the user object the player's user (+0x0C) let go; with a record in the message (+0x58) a user of its
    // profile and nick (vt+0x08), taken off the recent players (0x004E7C90), the voice's user (vt+0x24); with the
    // statistics its record (0x004EDF20).
    if (OnlineUser* user = onlineUser()) {
        user->userRelease(p->profile);
        if (i32(data + 0x58) != 0) {
            std::u16string nick;
            for (size_t i = 0; i < 32; ++i) {
                const auto c = static_cast<char16_t>(data[0x64 + i * 2] | data[0x65 + i * 2] << 8);
                if (c == 0) break;
                nick.push_back(c);
            }
            p->profile = user->addUser(i32(data + 0x60), nick);
            recentRemove(p->profile);
            if (VoiceChat* voice = gameVoice()) voice->addUser(p->profile);
            if (stats::gameStatistics() != nullptr) stats::makeRecord(p->profile);
        }
    }
    if (p->race.notRacing != 0 && hooks.hud && hooks.hud() && hooks.joined) hooks.joined(*p);
}

void NetSession::raceTypeCame(int32_t type, uint8_t id) {
    // 0x004E8750: off the online career the group of the custom race ([G+0x84]+0x784); the game takes it.
    m_raceType = type;
    if (hooks.setupChanged) hooks.setupChanged(id);
}

void NetSession::stageCame(const uint8_t* data) {
    if (u32(data + 8) <= m_stageNumber) return;
    roundEnd();
    m_stage = static_cast<SessionStage>(i32(data + 4));
    m_stageNumber = u32(data + 8);
    m_place = narrow(data + 0x0C, 32);
    m_route = narrow(data + 0x2C, 32);
    m_weather = i32(data + 0x54);
    m_timeOfDay = i32(data + 0x50);
    m_difficulty = i32(data + 0x5C);
    m_laps = i32(data + 0x4C);
    raceTypeCame(i32(data + 0x58), kStage);
}

void NetSession::readyCame(const uint8_t* data, int32_t peer) {
    NetPlayer* p = player(peer, i32(data + 4));
    if (p == nullptr) return;
    const int32_t ready = i32(data + 8);
    if (p->peer == 0) {
        if (ready == 0 || oneOnOne()) {
            m_waiting = 0;
        } else if (p->race.ready == 0) {
            m_waiting = 1;
            m_waitTimer = 0.0f;
        }
    }
    p->race.ready = ready;
    if (ready != 0) p->car = CarConfig::read(data + 0x0C);
}

void NetSession::sendStart(int32_t peer, int32_t spectator) {
    // 0x004E83C0: the setup and the players in the race: their peers as the receiver knows them (-1 itself, 0 the
    // host), their indices, names, cars, gone flags, finishes and places on the grid.
    std::vector<uint8_t> q(0x444, 0);
    q[0] = kStart;
    putNarrow(q.data() + 1, m_place, 32);
    putNarrow(q.data() + 0x21, m_route, 32);
    putI(q.data() + 0x44, m_laps);
    putI(q.data() + 0x48, m_timeOfDay);
    putI(q.data() + 0x4C, m_weather);
    putI(q.data() + 0x50, m_raceType);
    putI(q.data() + 0x54, m_difficulty);
    putI(q.data() + 0x58, spectator);
    // The cars of the six slots start as the defaults.
    for (int i = 0; i < 6; ++i) CarConfig().write(q.data() + 0x12C + i * 0x74);
    int32_t n = 0;
    for (const NetPlayer& p : m_players) {
        if (p.race.notRacing != 0 || n >= 6) continue;
        int32_t id = p.peer;
        if (id == -1) {
            id = 0;
        } else if (id == peer) {
            id = -1;
        }
        putI(q.data() + 0x60 + n * 4, id);
        putI(q.data() + 0x78 + n * 4, p.localIndex);
        putPlayerName(q.data() + 0x90 + n * 26, p.name, 26);
        p.car.write(q.data() + 0x12C + n * 0x74);
        putI(q.data() + 0x3E4 + n * 4, p.race.left);
        putI(q.data() + 0x3FC + n * 4, p.race.finishTime);
        putI(q.data() + 0x414 + n * 4, p.race.finishBestLap);
        putI(q.data() + 0x42C + n * 4, p.race.gridPlace);
        ++n;
    }
    putI(q.data() + 0x5C, n);
    m_transport.send(peer, q.data(), q.size(), true);
}

void NetSession::startCame(const uint8_t* data) {
    m_stage = SessionStage::Loading;
    m_place = narrow(data + 1, 32);
    m_route = narrow(data + 0x21, 32);
    m_weather = i32(data + 0x4C);
    m_timeOfDay = i32(data + 0x48);
    m_difficulty = i32(data + 0x54);
    m_laps = i32(data + 0x44);
    raceTypeCame(i32(data + 0x50), kStart);
    m_spectator = i32(data + 0x58);
    const uint32_t n = std::min<uint32_t>(u32(data + 0x5C), 6);
    for (uint32_t i = 0; i < n; ++i) {
        const int32_t peer = i32(data + 0x60 + i * 4);
        const int32_t index = i32(data + 0x78 + i * 4);
        NetPlayer* p = player(peer, index);
        if (p == nullptr) {
            NetPlayer added;
            added.peer = peer;
            added.localIndex = index;
            m_players.push_back(added);
            p = &m_players.back();
        }
        p->name = widen(data + 0x90 + i * 26, 26);
        p->car = CarConfig::read(data + 0x12C + i * 0x74);
        p->race.left = i32(data + 0x3E4 + i * 4);
        p->race.finishBestLap = i32(data + 0x414 + i * 4);
        p->race.finishTime = i32(data + 0x3FC + i * 4);
        p->race.gridPlace = i32(data + 0x42C + i * 4);
        p->race.notRacing = 0;
    }
    m_said60Left = 0;
    m_said10Left = 0;
    m_waiting = 0;
}

void NetSession::finishCame(const uint8_t* data, int32_t peer) {
    NetPlayer* p = player(peer, i32(data + 4));
    if (p == nullptr) return;
    if (m_waiting == 0 && teamWon()) {
        m_waiting = 1;
        m_waitTimer = 0.0f;
    }
    p->race.finishTime = i32(data + 8);
    p->race.finishBestLap = i32(data + 0x0C);
    p->race.finished = 1;
    if (m_firstFinished == 0) {
        if (hooks.firstFinished) hooks.firstFinished(*p);
        m_firstFinished = 1;
    }
}

void NetSession::stateCame(const uint8_t* data, int32_t peer) {
    NetPlayer* p = player(peer, i32(data + 4));
    if (p == nullptr || p->raceCar == nullptr) return;
    const uint8_t* state = data + 0x0C;
    const bool putBack = (data[0x49] & 0x10) != 0;  // bit 4 of its flags
    if (putBack && hooks.putBack) hooks.putBack(p->raceCar);
    const float sent = f32(data + 8);
    if (p->hasState == 0) {
        p->timeOffset = 0.0f;
        if (hooks.applyState) hooks.applyState(p->raceCar, state);
    } else {
        const float late = sent - p->senderTime;
        // A state not newer than the last is dropped.
        if (late < 0.0f || late == 0.0f) return;
        if (!putBack) {
            p->timeOffset = p->timeOffset - late;
        } else {
            p->timeOffset = 0.0f;
            if (hooks.applyState) hooks.applyState(p->raceCar, state);
        }
    }
    p->senderTime = sent;
    std::memcpy(p->state.data(), state, 0x40);
    p->hasState = 1;
    p->race.distance = static_cast<int32_t>(std::nearbyint(f32(data + 0x44)));
    float offset = p->timeOffset;
    if (offset < -0.1f) offset = -0.1f;
    if (0.1f < offset) offset = 0.1f;
    p->timeOffset = offset * 0.96f;
    // Carried on by how late it came and, in the fixed build, by the way here (the peer's one way time: the stock's
    // cars were that far behind, at 250 km/h and 60 ms some 4 m, so the cars met in other places on each machine).
    if ((p->state[0x3D] & 0x20) == 0 && hooks.extrapolate) {
        hooks.extrapolate(p->state.data(), p->timeOffset + oneWay(peer));
    }
}

float NetSession::oneWay(int32_t peer) const {
    const Peer* p = m_transport.peer(peer);
    if (p == nullptr) return 0.0f;
    float sum = 0.0f;
    for (float ping : p->pings) sum += ping;
    const float half = sum / static_cast<float>(p->pings.size()) * 0.5f;
    return half < 0.0f ? 0.0f : half > 0.25f ? 0.25f : half;
}

void NetSession::peerCame(const uint8_t* data) {
    // 0x004E87F0: a new machine and a player for each of its slots.
    const int32_t id = i32(data + 4);
    if (node(id) != nullptr) return;
    // The fixed build's hello to it (a stock machine passes over the id; a fixed one answers with its own).
    {
        uint8_t q[0x0C] = {kFixedHello};
        std::memcpy(q + 4, kFixedMark, 4);
        put32(q + 8, kFixedVersion);
        m_transport.send(id, q, sizeof q, true);
    }
    m_nodes.push_back({id, 1, 0});
    const auto slots = u32(data + 8);
    for (uint32_t i = 0; i < slots; ++i) {
        if (player(id, static_cast<int32_t>(i)) == nullptr) {
            NetPlayer p;
            p.peer = id;
            p.localIndex = static_cast<int32_t>(i);
            p.gpPlayer = -1;
            m_players.push_back(std::move(p));
        }
        reportStatus();
    }
    periods();
}

void NetSession::peerWent(const uint8_t* data) {
    // 0x004E88E0: the machine dropped; its players gone from the race (their cars stopped, the host's career told
    // when one had not finished) or dropped out of a race.
    const int32_t id = i32(data + 4);
    std::erase(m_fixedPeers, id);
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].peer == id) {
            m_nodes[i] = m_nodes.back();
            m_nodes.pop_back();
            break;
        }
    }
    for (size_t i = 0; i < m_players.size();) {
        NetPlayer& p = m_players[i];
        if (p.peer != id) {
            ++i;
            continue;
        }
        if (p.raceCar != nullptr && hooks.carGone) hooks.carGone(p.raceCar);
        if (hooks.hud && hooks.hud() && hooks.left) hooks.left(p);
        if (p.race.notRacing == 0) {
            p.race.left = 1;
            p.race.leftHow = i32(data + 8);
            if (VoiceChat* voice = gameVoice()) voice->removeUser(p.profile);
            reportStatus();
            ++i;
        } else {
            removePlayer(i);
        }
    }
    periods();
}

void NetSession::periods() {
    // 0x004E8310: the states' period by the number of players: the stock's 0.1 s up to three, 0.12 four, 0.16 five,
    // 0.2 more (2005's lines); the fixed build's 0.05 s for any (a state 76 bytes: some 10 KB/s each way with six).
    for (NetPlayer& p : m_players) p.period = 0.05f;
}

void NetSession::roundEnd() {
    // 0x004E7EB0: from an online race of type 5 or 6 of more than one racer, not watching, with the statistics, not
    // the career's pink slip (its stakes' end reports it): a client's race nobody finished left (0x004ED120), else
    // (a host's only once someone finished) each of our players with a user by its place (0x004EC9F0; not in a
    // career's crew race); reported, sent (type 6 only when the first player's finish, taken, was none).
    const RaceType type = hooks.raceType ? hooks.raceType() : RaceType::TimedLap;
    if (m_stage == SessionStage::Racing && m_spectator == 0 && matchmaking() != nullptr &&
        stats::gameStatistics() != nullptr && (type == RaceType::Circuit || type == RaceType::Sprint) && racing() > 1 &&
        !(hooks.careerPinkSlip && hooks.careerPinkSlip())) {
        const auto byPlaces = [this] {
            bool any = false;
            for (size_t k = 0; k < m_order.size(); ++k) {
                const uint32_t at = m_order[k];
                if (at >= m_players.size() || m_players[at].peer != -1) continue;  // 0x004EA0C0: ours
                const int32_t user = m_players[at].profile;                          // 0x004EA160
                if (user == -1) continue;
                const bool career =
                    matchmaking() != nullptr && onlineHosted().mode == OnlineKind::Career;  // 0x004E1AC0
                if (!career || onlineHosted().career.team < 2) {
                    stats::finishedRace(user, static_cast<int32_t>(k) + 1);
                    any = true;
                }
            }
            return any;
        };
        bool reported = false;
        if (!m_transport.hosting()) {
            reported = m_firstFinished == 0 ? stats::reportRaceLeft() : byPlaces();
        } else if (m_firstFinished != 0) {
            reported = byPlaces();
        }
        if (reported &&
            (type != RaceType::Sprint || (hooks.firstPlayerFinishCleared && hooks.firstPlayerFinishCleared()))) {
            stats::reporter()->send();
        }
    }
    // In the race, the points of the places (not in a showoff nor an online career's crew race): 10, 6, 4, 3, 1 to
    // those who finished; a race more to those in it.
    if (m_stage == SessionStage::Racing) {
        const bool showoff = type == RaceType::Cruise;
        const bool crews =
            matchmaking() != nullptr && onlineHosted().mode == OnlineKind::Career && onlineHosted().career.team > 1;
        if (!showoff && !crews) {
            static constexpr int32_t kPoints[5] = {10, 6, 4, 3, 1};
            for (size_t i = 0; i < m_order.size(); ++i) {
                const uint32_t at = m_order[i];
                if (at >= m_players.size()) continue;
                if (m_players[at].race.finished != 0) m_players[at].race.points += i < 5 ? kPoints[i] : 0;
            }
        }
        for (NetPlayer& p : m_players) {
            if (p.race.notRacing == 0) ++p.race.races;
        }
    }
    for (NetNode& n : m_nodes) {
        n.spectating = 1;
        n.loaded = 0;
    }
    for (size_t i = 0; i < m_players.size();) {
        if (m_players[i].race.left != 0) {
            removePlayer(i);
        } else {
            ++i;
        }
    }
    for (NetPlayer& p : m_players) {
        p.race.notRacing = 1;
        p.race.ready = 0;
        p.race.finished = 0;
        p.race.finishTime = 0;
        p.race.finishBestLap = 0;
        p.race.place = 0;
        p.raceCar = nullptr;
        p.hasState = 0;
        p.senderTime = 0.0f;
    }
    m_waiting = 0;
    m_firstFinished = 0;
}

void NetSession::shuffleGrid() {
    // 0x004E9B50: the places of the grid shuffled; each draw seeds rand() again with the count and the transport's
    // address (Stock problem: as the transport's random bytes, 0x00598710).
    const uint32_t n = racing();
    std::array<uint32_t, 6> slots{};
    for (uint32_t i = 0; i < n && i < 6; ++i) slots[i] = i;
    CrtRandom& random = gameRandom();
    for (uint32_t i = 0; i < n && i < 6; ++i) {
        m_transport.reseed(random);
        const auto r = static_cast<uint32_t>(random.next()) % n;
        std::swap(slots[i], slots[r]);
    }
    size_t k = 0;
    for (NetPlayer& p : m_players) {
        if (p.race.notRacing == 0 && k < 6) p.race.gridPlace = static_cast<int32_t>(slots[k++]);
    }
}

void NetSession::startRace() {
    // 0x004EAAA0: the ready ones in the race (their machines racing), the grid shuffled, the places from it, each
    // machine its 0x0E, the stage 9.
    for (NetPlayer& p : m_players) {
        if (p.race.ready != 0) {
            p.race.notRacing = 0;
        }
    }
    for (NetNode& n : m_nodes) {
        bool any = false;
        for (const NetPlayer& p : m_players) {
            if (p.peer == n.peer && p.race.notRacing == 0) any = true;
        }
        if (any) n.spectating = 0;
    }
    shuffleGrid();
    for (NetPlayer& p : m_players) p.race.place = p.race.gridPlace;
    for (const NetNode& n : m_nodes) sendStart(n.peer, n.spectating);
    m_said60Left = 0;
    m_said10Left = 0;
    m_waiting = 0;
    m_stage = SessionStage::Loading;
    reportStatus();
}

void NetSession::go() {
    const uint8_t q[1] = {kGo};
    sendAll(q, sizeof q, true);
    m_stage = SessionStage::Countdown;
    m_waiting = 0;
}

void NetSession::backToLobby() {
    // 0x004EADB0 (unless a message is up on the race's front end): the stage 0 to all.
    setStage(SessionStage::Lobby);
    m_waiting = 0;
}

bool NetSession::teamWon() {
    // 0x004EBD60: off the online career (or with fewer than two in a team) always.
    return true;
}

// ---- Each frame.

void NetSession::frame(float dt) {
    m_transport.update(dt);
    if (m_stage == SessionStage::Racing) {
        m_raceTime = m_raceTime + dt;
        carsFrame(dt);
        modelFrame(dt);
        placesFrame(dt);
    }
    if (m_stage != SessionStage::Racing) voiceFrame(dt);
    infoFrame(dt);
    waitFrame(dt);
    if (hosting()) {
        hostFrame(dt);
    } else {
        clientFrame(dt);
    }
}

void NetSession::carsFrame(float dt) {
    // 0x004EAF10: the others' cars carried on and smoothed towards their last state (applied as it is with bit 5,
    // their race paused); ours sent each period (unreliable, 0x4C bytes).
    for (NetPlayer& p : m_players) {
        if (p.raceCar == nullptr) continue;
        const bool remote = hooks.remote && hooks.remote(p.raceCar);
        if (remote) {
            const bool running = !(hooks.racePaused && hooks.racePaused());
            if (running && p.hasState != 0 && p.race.left == 0) {
                if ((p.state[0x3D] & 0x20) == 0) {
                    if (hooks.extrapolate) hooks.extrapolate(p.state.data(), dt);
                    if (hooks.smoothState) hooks.smoothState(p.raceCar, p.state.data(), dt);
                } else if (hooks.applyState) {
                    hooks.applyState(p.raceCar, p.state.data());
                }
                p.timeOffset = dt + p.timeOffset;
            }
            continue;
        }
        p.race.distance =
            static_cast<int32_t>(std::nearbyint(hooks.carDistance ? hooks.carDistance(p.raceCar) : 0.0f));
        p.senderTime = dt + p.senderTime;
        p.sendTimer = dt + p.sendTimer;
        if (p.period < p.sendTimer) {
            p.sendTimer = 0.0f;
            uint8_t q[0x4C] = {kState};
            putI(q + 4, p.localIndex);
            putF(q + 8, p.senderTime);
            if (hooks.carState) hooks.carState(p.raceCar, q + 0x0C);
            q[0x49] = static_cast<uint8_t>((q[0x49] & ~0x10) | ((p.reset & 1) << 4));
            sendAll(q, sizeof q, false);
            p.reset = 0;
        }
    }
}

void NetSession::modelFrame(float dt) {
    // 0x004EB130: each 5 s our cars' byte (+0x430 +0x170).
    m_modelTimer = m_modelTimer + dt;
    if (!(5.0f < m_modelTimer)) return;
    m_modelTimer = 0.0f;
    for (const NetPlayer& p : m_players) {
        if (p.peer != -1 || p.raceCar == nullptr || !hooks.carByte) continue;
        uint8_t q[0x0C] = {kModelByte};
        putI(q + 4, p.localIndex);
        q[8] = static_cast<uint8_t>(hooks.carByte(p.raceCar));
        sendAll(q, sizeof q, false);
    }
}

void NetSession::placesFrame(float dt) {
    // 0x004EB5B0: each second the players in the race ordered by distance; the host tells each its place (0x17) and,
    // past 10 s of the race with no one finished, everyone of a new leader (0x18) and the HUD; each 20 s the leader
    // said again.
    m_placesTimer = m_placesTimer + dt;
    if (1.0f < m_placesTimer) {
        m_placesTimer = 0.0f;
        m_order.clear();
        for (uint32_t i = 0; i < m_players.size(); ++i) {
            if (m_players[i].race.notRacing == 0) m_order.push_back(i);
        }
        // 0x004EB500: the farther first.
        std::stable_sort(m_order.begin(), m_order.end(), [this](uint32_t a, uint32_t b) {
            return m_players[b].race.distance < m_players[a].race.distance;
        });
        if (hosting() && !m_order.empty()) {
            for (uint32_t place = 1; place <= m_order.size(); ++place) {
                NetPlayer& p = m_players[m_order[place - 1]];
                if (p.race.place == static_cast<int32_t>(place)) continue;
                p.race.place = static_cast<int32_t>(place);
                uint8_t q[0x0C] = {kPlaceOf};
                putI(q + 4, p.localIndex);
                putI(q + 8, static_cast<int32_t>(place));
                if (p.peer != -1 && p.race.left == 0) m_transport.send(p.peer, q, sizeof q, true);
                if (place == 1 && m_firstFinished == 0 && 10.0f < m_raceTime) {
                    for (const NetPlayer& o : m_players) {
                        if (o.peer == -1 || o.race.left != 0) continue;
                        uint8_t l[0x0C] = {kLeader};
                        int32_t id = p.peer == -1 ? 0 : p.peer;
                        if (o.peer == p.peer) id = -1;
                        putI(l + 4, id);
                        l[8] = static_cast<uint8_t>(p.localIndex);
                        m_transport.send(o.peer, l, sizeof l, true);
                    }
                    leaderNote(p, true);
                }
            }
        }
    }
    m_leaderTimer = m_leaderTimer + dt;
    if (20.0f < m_leaderTimer) {
        m_leaderTimer = 0.0f;
        if (!m_order.empty() && m_order[0] < m_players.size()) leaderNote(m_players[m_order[0]], false);
    }
}

void NetSession::infoFrame(float dt) {
    // 0x004EB2D0: each 2 s the info of the players not known asked for; choosing the cars, our readiness again.
    m_infoTimer = m_infoTimer + dt;
    if (!(2.0f < m_infoTimer)) return;
    m_infoTimer = 0.0f;
    for (const NetPlayer& p : m_players) {
        if (p.infoKnown != 0) continue;
        uint8_t q[8] = {kInfoAsked};
        putI(q + 4, p.localIndex);
        m_transport.send(p.peer, q, sizeof q, true);
    }
    if (m_stage == SessionStage::Cars) {
        for (size_t i = 0; i < m_players.size(); ++i) {
            if (m_players[i].peer == -1) sendReady(m_players[i].race.ready, m_players[i].localIndex);
        }
    }
}

void NetSession::waitNote(int32_t& said, uint32_t seconds) {
    if (said != 0) return;
    if (waitLeft() < static_cast<float>(seconds) && hooks.hud && hooks.hud()) {
        if (hooks.countdown) hooks.countdown(seconds);
        said = 1;
    }
}

void NetSession::waitFrame(float dt) {
    // 0x004EB400: the stage's wait: the host starts the race 30 s after the first ready; 60 s for the machines to
    // load (the host says go, a client goes itself); the race's end 63 s after the first finish (the notes at 60
    // and 10 s left).
    if (m_waiting == 0) return;
    m_waitTimer = m_waitTimer + dt;
    if (m_stage == SessionStage::Cars) {
        if (hosting() && 30.0f < m_waitTimer) startRace();
    } else if (m_stage == SessionStage::Loaded) {
        if (60.0f < m_waitTimer) {
            if (hosting()) {
                go();
            } else {
                m_stage = SessionStage::Countdown;
                m_waiting = 0;
            }
        }
    } else if (m_stage == SessionStage::Racing) {
        waitNote(m_said60Left, 60);
        waitNote(m_said10Left, 10);
        if (hosting() && 63.0f < m_waitTimer) backToLobby();
    }
}

void NetSession::hostFrame(float dt) {
    // 0x004EB870
    switch (m_stage) {
    case SessionStage::Cars: {
        bool all = true;
        for (const NetPlayer& p : m_players) {
            if (p.race.ready == 0) all = false;
        }
        if (all) startRace();
        break;
    }
    case SessionStage::Loaded: {
        bool all = true;
        for (const NetNode& n : m_nodes) {
            if (n.spectating == 0 && n.loaded == 0) all = false;
        }
        if (all) go();
        break;
    }
    case SessionStage::Countdown:
        m_clock = m_clock + dt;
        if (3.0f < m_clock) {
            m_afterFinish = 0.0f;
            m_clock = 0.0f;
            m_raceTime = 0.0f;
            m_stage = SessionStage::Racing;
        }
        break;
    case SessionStage::Racing: {
        bool all = true;
        for (const NetPlayer& p : m_players) {
            if (p.race.notRacing == 0 && p.race.left == 0 && p.race.finished == 0) all = false;
        }
        if (!all) break;
        m_afterFinish = m_afterFinish + dt;
        if (10.0f < m_afterFinish) backToLobby();
        break;
    }
    default:
        break;
    }
}

void NetSession::clientFrame(float dt) {
    // 0x004EBA00: in the count the clock asked of the host each 0.4 s (coded by hand: its first byte 0x11 ^ 0x0D);
    // the race after 3 s of it.
    if (m_stage != SessionStage::Countdown) return;
    m_syncTimer = m_syncTimer + dt;
    if (0.4f < m_syncTimer) {
        m_syncTimer = 0.0f;
        uint8_t q[8] = {kClock};
        putF(q + 4, m_clock);
        m_transport.send(0, q, sizeof q, false);
    }
    m_clock = m_clock + dt;
    if (3.0f < m_clock) {
        m_clock = 0.0f;
        m_raceTime = 0.0f;
        m_stage = SessionStage::Racing;
    }
}

// ---- The game's network.

namespace {

// 0x004E99E0, the voice's event hook: in a session, from the player of ours whose GP player it is (its index, 0
// without one): an event of our own (no user) as 0x0C ([4] that index, [8] the receiver's index, [0x0C] the kind) to
// every player of another machine, reliable but for the kinds 0 and 1; one with a user to that user's player,
// reliable.
void voiceEvent(int32_t player, int32_t user, int32_t kind, void*) {
    NetGame* game = netGame();
    if (game == nullptr || !game->transport.inSession()) return;
    const std::vector<NetPlayer>& players = game->session.players();
    int32_t from = 0;
    for (const NetPlayer& p : players) {
        if (p.gpPlayer == player) {
            from = p.localIndex;
            break;
        }
    }
    const auto message = [&](const NetPlayer& to, bool reliable) {
        uint8_t q[0x10] = {kVoiceStatus};
        putI(q + 4, from);
        putI(q + 8, to.localIndex);
        putI(q + 0x0C, kind);
        game->transport.send(to.peer, q, sizeof q, reliable);
    };
    if (user == -1) {
        // A copy: a send's events could change the list.
        const std::vector<NetPlayer> all = players;
        for (const NetPlayer& p : all) {
            if (p.peer != -1) message(p, kind != 0 && kind != 1);
        }
        return;
    }
    for (const NetPlayer& p : players) {
        if (p.profile == user) {
            message(p, true);
            return;
        }
    }
}

// Shared: the matchmaking keeps a reference to the transport ([0x0075F758]'s count) past the game's.
std::shared_ptr<NetGame>& theNetGame() {
    static std::shared_ptr<NetGame> game;
    return game;
}

}  // namespace

NetGame* netGame() {
    return theNetGame().get();
}

std::shared_ptr<NetGame> sharedNetGame() {
    return theNetGame();
}

Transport::Options& netOptions() {
    static Transport::Options options;
    return options;
}

NetGame& startNetGame(CrtRandom& random) {
    endNetGame();
    auto game = std::make_shared<NetGame>();
    std::string why;
    if (!game->transport.open(netOptions(), random, &why)) log::warning("network: the transport cannot open: {}", why);
    game->session.start();
    theNetGame() = std::move(game);
    // The voice's hooks: its events carried to the machines (0x004E99E0), +0x14 nothing (0x0055FE70).
    if (VoiceChat* voice = gameVoice()) {
        voice->eventHook = voiceEvent;
        voice->eventContext = nullptr;
        voice->unusedHook = [](int32_t, int32_t) {};
        voice->unusedHookContext = nullptr;
    }
    return *theNetGame();
}

void endNetGame() {
    if (!theNetGame()) return;
    theNetGame()->session.reset(false);
    theNetGame()->transport.close();
    theNetGame().reset();
    if (VoiceChat* voice = gameVoice()) {
        voice->eventHook = nullptr;
        voice->eventContext = nullptr;
        voice->unusedHook = nullptr;
        voice->unusedHookContext = nullptr;
    }
}

}  // namespace oj::net
