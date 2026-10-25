#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Core/Network/GameSpy/GameSpyUser.h"

#include "Core/Base/Log.h"
#include "Core/Network/GameSpy/Peerchat.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/VoiceChat.h"

#include <winsock2.h>

#include <cstdio>
#include <cstring>
#include <format>
#include <string>

namespace oj::net {

namespace {

const std::u16string kEmpty;  // [0x0079E8A8]

OnlineUser* g_user = nullptr;          // [0x0075F1E4]
OnlineUser* g_gameUser = nullptr;      // [0x0075F040]
OnlineFriends* g_friends = nullptr;    // [0x0075F284]
OnlineFriends* g_gameFriends = nullptr;  // [0x0075EF7C]

// The wide game name ([0x0075F1F0], 0x0055C600) as the location strings take it: the low byte of each character.
std::string gameName8() {
    return kGameName;
}

// A copy of a wide string with each byte sign-extended (0x0059C250, movsx).
std::u16string widenSigned(std::string_view text) {
    std::u16string out;
    for (char c : text) out.push_back(static_cast<char16_t>(static_cast<int16_t>(static_cast<signed char>(c))));
    return out;
}

// strtok of the C runtime over one string.
struct Tokens {
    std::string text;
    size_t at = 0;
    std::optional<std::string> next(std::string_view delimiters) {
        while (at < text.size() && delimiters.find(text[at]) != std::string_view::npos) ++at;
        if (at >= text.size()) return std::nullopt;
        const size_t start = at;
        while (at < text.size() && delimiters.find(text[at]) == std::string_view::npos) ++at;
        std::string token = text.substr(start, at - start);
        if (at < text.size()) ++at;
        return token;
    }
};

OnlineUserRecord g_noRecord;  // a record of none (-1): the stock code reads before its list
OnlineFriend g_noFriend;      // a friend of none (-1): the stock code writes at address 0

}  // namespace

std::u16string buildLocation(const OnlinePlayer& player) {
    if (player.playing == 0) return {};
    const std::string name = gameName8();
    std::u16string out;
    if (player.address) {
        in_addr a{};
        a.s_addr = player.address->ip;
        const std::string text = std::format("{}://{}:{}", name, inet_ntoa(a), ntohs(player.address->port));
        out = widenSigned(text);
    } else {
        out = std::u16string(name.begin(), name.end()) + widenSigned("://0.0.0.0:0");
    }
    out += widenSigned("/?");
    out += widenSigned(std::format("session={}", player.session));
    return out;
}

void parseLocation(std::u16string_view location, std::optional<Address>& address, int32_t& session, int32_t& same) {
    if (location.empty() || location[0] == 0) return;
    Tokens t;
    for (char16_t c : location) {
        if (c == 0) break;
        t.text.push_back(static_cast<char>(c));
    }
    std::string name;
    if (const auto token = t.next(":")) name = token->substr(0, 31);
    if (const auto token = t.next("?")) {
        int a = 0, b = 0, c = 0, d = 0, port = 0;
        if (sscanf_s(token->c_str(), "//%d.%d.%d.%d:%d/", &a, &b, &c, &d, &port) == 5) {
            Address entry;
            entry.ip = static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) | (static_cast<uint32_t>(c) << 16) |
                       (static_cast<uint32_t>(d) << 24);
            entry.port = htons(static_cast<u_short>(port));
            address = entry;
        }
    }
    for (auto token = t.next("?"); token; token = t.next("?")) {
        if (_strnicmp(token->c_str(), "session=", 8) == 0) sscanf_s(token->c_str(), "session=%d", &session);
    }
    if (name == gameName8()) same = 1;
}

// ---- The user object.

OnlineUser::OnlineUser() {
    // [0x0075B34C] 0: the SDK's one connection is the new object's to make; an earlier object's (one its friends
    // still hold) goes here, logged out (gpDestroy). The stock left it open, never destroyed, and the earlier
    // object's sign-out later destroyed the new one's connection (within the new login's callback when the friends
    // are made again); the fixed build's sign-out lets go only its own.
    gpConnection().reset();
}

OnlineUser::~OnlineUser() {
    signOut();
    baseRelease();
}

void OnlineUser::release() {
    if (--m_refs == 0) delete this;
}

void OnlineUser::resultToState(gp::Result result) {
    if (state != UserState::None) return;
    switch (result) {
    case gp::Result::MemoryError: state = UserState::MemoryError; break;
    case gp::Result::ParameterError: state = UserState::ParameterError; break;
    case gp::Result::NetworkError: state = UserState::NetworkError; break;
    case gp::Result::ServerError: state = UserState::ServerError; break;
    default:
        break;
    }
}

void OnlineUser::sendPresence(int32_t playerIndex) {
    if (m_guest) return;  // the fixed build's guest has no connection (else the state 1: the network lost)
    const OnlinePlayer& p = m_players[static_cast<size_t>(playerIndex)];
    int32_t status = 0;
    std::u16string text = u"Offline";
    if (p.online != 0) {
        status = p.playing != 0 ? 2 : 1;
        text = p.playing != 0 ? u"Playing" : u"Online";
    }
    const std::u16string& where = location(playerIndex);
    std::shared_ptr<gp::Connection> c = gpConnection();
    resultToState(c ? c->setStatus(status, text, where) : gp::Result::ParameterError);
}

int32_t OnlineUser::newUser() {
    size_t i = 0;
    while (i < m_records.size() && m_records[i].refs != 0) ++i;
    // A new record's profile and nick are what the stack held (the callers set them).
    if (i == m_records.size()) m_records.push_back(OnlineUserRecord{});
    userAddRef(static_cast<int32_t>(i));
    return static_cast<int32_t>(i);
}

int32_t OnlineUser::addUser(int32_t profile, std::u16string_view nick) {
    const int32_t i = newUser();
    m_records[static_cast<size_t>(i)].profile = profile;
    m_records[static_cast<size_t>(i)].nick = std::u16string(nick);
    return i;
}

void OnlineUser::userAddRef(int32_t user) {
    if (user != -1) ++m_records[static_cast<size_t>(user)].refs;
}

void OnlineUser::userRelease(int32_t user) {
    if (user != -1) --m_records[static_cast<size_t>(user)].refs;
}

bool OnlineUser::sameUser(int32_t a, int32_t b) const {
    if (a == -1) return b == -1;
    if (b == -1) return false;
    return m_records[static_cast<size_t>(a)].profile == m_records[static_cast<size_t>(b)].profile;
}

const std::u16string& OnlineUser::userNick(int32_t user) const {
    if (user == -1) return kEmpty;
    return m_records[static_cast<size_t>(user)].nick;
}

OnlineUserRecord& OnlineUser::record(int32_t user) {
    if (user < 0 || static_cast<size_t>(user) >= m_records.size()) {
        g_noRecord = OnlineUserRecord{};
        return g_noRecord;
    }
    return m_records[static_cast<size_t>(user)];
}

int32_t OnlineUser::newPlayer() {
    size_t i = 0;
    while (i < m_players.size() && m_players[i].refs != 0) ++i;
    if (i == m_players.size()) m_players.push_back(OnlinePlayer{});
    playerAddRef(static_cast<int32_t>(i));
    localPlayers.push_back(static_cast<int32_t>(i));
    return static_cast<int32_t>(i);
}

void OnlineUser::playerAddRef(int32_t player) {
    if (player != -1) ++m_players[static_cast<size_t>(player)].refs;
}

void OnlineUser::playerRelease(int32_t player) {
    if (player == -1) return;
    OnlinePlayer& p = m_players[static_cast<size_t>(player)];
    --p.refs;
    if (p.refs == 0) userRelease(p.user);
}

const std::u16string& OnlineUser::playerNick(int32_t player) const {
    if (player == -1) return kEmpty;
    return userNick(m_players[static_cast<size_t>(player)].user);
}

int32_t OnlineUser::playerUser(int32_t player) {
    if (player == -1) return -1;
    userAddRef(m_players[static_cast<size_t>(player)].user);
    return m_players[static_cast<size_t>(player)].user;
}

void OnlineUser::setOnline(int32_t player, int32_t online) {
    m_players[static_cast<size_t>(player)].online = online;
    if (presenceTimer < 59.0f) presenceTimer = 59.0f;
}

void OnlineUser::setPlaying(int32_t player, int32_t playing, int32_t /*isPublic: unused (0x0055F680)*/) {
    m_players[static_cast<size_t>(player)].playing = playing;
    if (presenceTimer < 59.0f) presenceTimer = 59.0f;
}

void OnlineUser::setAddress(int32_t player, std::optional<Address> address) {
    OnlinePlayer& p = m_players[static_cast<size_t>(player)];
    p.address = address;
    p.session = matchmaking() != nullptr ? static_cast<int32_t>(matchmaking()->sessionId()) : 0;
    if (presenceTimer < 59.0f) presenceTimer = 59.0f;
}

const std::u16string& OnlineUser::location(int32_t player) {
    m_location = buildLocation(m_players[static_cast<size_t>(player)]);
    return m_location;
}

void OnlineUser::setCredentials(std::u16string_view nick, std::u16string_view email, std::u16string_view password,
                                bool newUser) {
    state = UserState::None;
    signOut();
    m_nick = std::u16string(nick);
    m_email = std::u16string(email);
    m_password = std::u16string(password);
    m_newUser = newUser ? 1 : 0;
    m_check = std::make_unique<AvailableCheck>();
    m_check->start(kGameName);
    m_connectAsked = 1;
}

// 0x0055F880: the service up (+0x1C8 1); the connection processed (gpProcess, its result into the state). A login
// asked: the available check's answer; none yet, wait; unavailable or temporarily, the state 5; available, the
// connection made (gpInitialize with the product and the namespace 0), its callbacks set (error, buddy status, buddy
// request, game invite) and the login (gpConnectNewUserW or gpConnectW, behind a firewall, not blocking), each result
// into the state, the login no longer asked. Else, with a connection, the periodic check (the frame's time at most
// 0.05). Then the base's frame (the presence's timer).
void OnlineUser::update(float dt) {
    m_serviceOk = 1;
    if (std::shared_ptr<gp::Connection> c = gpConnection()) {
        const gp::Result result = c->process();
        // Once logged on the fixed build leaves it to the error's callback (a fatal error's code); the stock took the
        // processing's result too, so a server's error that was not fatal still gave the state 4.
        if (loggedOn == 0) resultToState(result);
    }
    if (m_connectAsked != 0) {
        AvailableCheck::Result r = AvailableCheck::Available;
        if (m_check) {
            r = m_check->think();
        } else {
            availableResult() = AvailableCheck::Available;
        }
        if (r == AvailableCheck::Unavailable || r == AvailableCheck::TemporarilyUnavailable) {
            state = UserState::Unavailable;
            m_connectAsked = 0;
        } else if (r == AvailableCheck::Available && m_guest) {
            guestLoggedOn();
            m_connectAsked = 0;
        } else if (r == AvailableCheck::Available) {
            resultToState(gp::Connection::create(gpConnection(), 0, 0, server));
            m_connection = gpConnection();
            if (state == UserState::None) {
                std::shared_ptr<gp::Connection> c = gpConnection();
                if (c) {
                    c->onError = [this](const gp::ErrorArg& arg) { onError(arg); };
                    c->onBuddyStatus = [this](const gp::BuddyStatusArg& arg) { onBuddyStatus(arg); };
                    c->onBuddyRequest = [this](const gp::BuddyRequestArg& arg) { onBuddyRequest(arg); };
                    c->onGameInvite = [this](const gp::GameInviteArg& arg) { onGameInvite(arg); };
                }
                auto done = [this](const gp::ConnectResponse& response) { onConnect(response); };
                gp::Result result = gp::Result::ParameterError;
                if (c && m_newUser != 0) {
                    result = c->connectNewUser(m_nick, u"", m_email, m_password, u"", true, false, done);
                } else if (c) {
                    result = c->connect(m_nick, m_email, m_password, true, false, done);
                }
                resultToState(result);
            }
            m_connectAsked = 0;
        }
    } else if (gpConnection()) {
        m_serviceOk = periodicCheck(dt <= 0.05f ? dt : 0.05f);
    }
    baseUpdate(dt);
}

// 0x0055F780: not checking, a countdown (10 s after the object was made, then 2 s after each answer) before the next
// check; checking, its answer: available 1, unavailable 0 for this frame; no answer for 15 s, 0 until one.
int32_t OnlineUser::periodicCheck(float dt) {
    if (m_checkRunning == 0) {
        if (m_checkTimer <= 0.0f) {
            m_check = std::make_unique<AvailableCheck>();
            m_check->start(kGameName);
            m_checkTimer = 0.0f;
            m_checkRunning = 1;
            return m_checkOk;
        }
        m_checkTimer -= dt;
        return m_checkOk;
    }
    AvailableCheck::Result r = AvailableCheck::Available;
    if (m_check) {
        r = m_check->think();
    } else {
        availableResult() = AvailableCheck::Available;
    }
    switch (r) {
    case AvailableCheck::Waiting:
        m_checkTimer += dt;
        if (m_checkTimer > 15.0f) m_checkOk = 0;
        return m_checkOk;
    case AvailableCheck::Available:
        m_checkOk = 1;
        m_checkRunning = 0;
        m_checkTimer = 2.0f;
        return 1;
    default:
        m_checkRunning = 0;
        m_checkOk = 1;
        m_checkTimer = 2.0f;
        return 0;
    }
}

// 0x0055BAF0: the presence's timer; past 60 s, 0 and the first local player's presence sent (the stock referenced
// the player each time and never let it go; not the fixed build).
void OnlineUser::baseUpdate(float dt) {
    const double sum = static_cast<double>(dt) + static_cast<double>(presenceTimer);
    presenceTimer = static_cast<float>(sum);
    if (!(sum > 60.0)) return;
    presenceTimer = 0.0f;
    if (localPlayers.empty()) return;
    sendPresence(localPlayers[0]);
}

// 0x0055BB60: each local player let go, the list emptied, +0x08 and the logged-on flag 0.
void OnlineUser::baseRelease() {
    for (size_t i = 0; i < localPlayers.size(); ++i) playerRelease(localPlayers[i]);
    localPlayers.clear();
    loggedOn = 0;
}

// 0x0055FA60: a login asked: its available check's socket closed (the check's result 0 when it was open). The
// connection: logged in, logged out (gpDisconnect) and reset; destroyed (gp.info written when its state is not 0:
// never after the reset, kept so in the fixed build: a cache written would give stale nicks in later sessions). The
// fixed build lets go only the connection this object made (the stock's, whichever the SDK held). The service flag
// 0; the base's release.
void OnlineUser::signOut() {
    if (m_connectAsked != 0) {
        if (m_check && m_check->opened()) {
            m_check.reset();
            availableResult() = AvailableCheck::Waiting;
        }
        m_connectAsked = 0;
    }
    std::shared_ptr<gp::Connection> c = gpConnection();
    if (c && c == m_connection.lock()) {
        resultToState(gp::Result::NoError);
        if (c->state() == gp::State::LoggedIn) c->disconnect();
        gpConnection().reset();
    }
    c.reset();
    m_connection.reset();
    m_guest = false;
    m_serviceOk = 0;
    baseRelease();
}

void OnlineUser::setGuest(std::u16string_view nick) {
    state = UserState::None;
    signOut();
    m_nick = std::u16string(nick);
    m_guest = true;
    // The available check as an account's login runs it (the server browser and the reporting are made only when it
    // said available); the next frames log on once it answers.
    m_check = std::make_unique<AvailableCheck>();
    m_check->start(kGameName);
    m_connectAsked = 1;
}

void OnlineUser::guestLoggedOn() {
    const auto mixed = static_cast<uint32_t>(GetTickCount()) ^ (static_cast<uint32_t>(GetCurrentProcessId()) << 16);
    const int32_t u = newUser();
    m_records[static_cast<size_t>(u)].profile = -2 - static_cast<int32_t>(mixed & 0x3FFFFFFF);
    m_records[static_cast<size_t>(u)].nick = m_nick;
    const int32_t p = newPlayer();
    OnlinePlayer& pl = m_players[static_cast<size_t>(p)];
    pl.user = u;
    pl.unused = 0;
    pl.online = 1;
    pl.playing = 0;
    pl.address.reset();
    pl.session = 0;
    pl.friendNote = 0;
    pl.inviteNote = 0;
    m_serviceOk = 1;
    loggedOn = 1;
    if (loggedOnHook) loggedOnHook();
}

// 0x0055E230: the error's code as the state (0x0104, 0x0202 11; 6 6; 7 7; 0x0101 8; 0x0102 and 0x0201 9; 0x0103 10;
// 0x0105 12; 0x0106 13; 0x0107 14; 0x0108 15; any other 1) and not logged on. The fixed build: once logged on, only a
// fatal error (the connection gone) does it, and the database's error (4) is as any other. The stock took every error
// (a refused buddy request: the network lost, logged off) and gave 4 the state 11, "Invalid password for this email
// account".
void OnlineUser::onError(const gp::ErrorArg& arg) {
    // The fixed build's log line of each error (the server's text with it), for the players' reports.
    log::info("online: GameSpy error {:#06x}{}: {}", arg.errorCode, arg.fatal ? " (fatal)" : "",
              gp::toUtf8(arg.errorString));
    if (loggedOn != 0 && !arg.fatal) return;
    switch (arg.errorCode) {
    case gp::kLoginBadPassword:
    case gp::kNewUserBadPassword: state = UserState::BadPassword; break;
    case gp::kForcedDisconnect: state = UserState::ForcedDisconnect; break;
    case gp::kConnectionClosed: state = UserState::ConnectionClosed; break;
    case gp::kLoginTimeout: state = UserState::LoginTimeout; break;
    case gp::kLoginBadNick:
    case gp::kNewUserBadNick: state = UserState::BadNick; break;
    case gp::kLoginBadEmail: state = UserState::BadEmail; break;
    case gp::kLoginBadProfile: state = UserState::BadProfile; break;
    case gp::kLoginProfileDeleted: state = UserState::ProfileDeleted; break;
    case gp::kLoginConnectionFailed: state = UserState::ConnectionFailed; break;
    case gp::kLoginServerAuthFailed: state = UserState::AuthFailed; break;
    default: state = UserState::ParameterError; break;
    }
    loggedOn = 0;
}

// 0x0055FB10: the profile's nick into the record the call was made for, the record's reference let go (the user
// object of [0x0075F1E4], whichever asked).
gp::Connection::InfoCallback OnlineUser::infoCallback(int32_t user) {
    return [user](const gp::GetInfoResponse& response) {
        OnlineUser* u = userSingleton();
        if (u == nullptr) return;
        u->addRef();
        u->record(user).nick = response.nick;
        u->userRelease(user);
        u->release();
    };
}

// 0x0055ECB0: logged in: a record of our profile (its nick asked, from the cache or the server), a local player of it
// online, logged on, invitable to the game's product (10362), the logged-on hook. The profiles' nicks are asked not
// blocking in the fixed build (here and for the buddies, their requests and invitations: the stock processed the
// connection inside the frame until the answer, without end while the server did not answer), our own the login's
// nick until the answer (the profile logged in by it).
void OnlineUser::onConnect(const gp::ConnectResponse& response) {
    if (response.result != gp::Result::NoError) return;
    const int32_t u = newUser();
    m_records[static_cast<size_t>(u)].profile = response.profile;
    m_records[static_cast<size_t>(u)].nick = m_nick;
    userAddRef(u);
    if (std::shared_ptr<gp::Connection> c = gpConnection()) {
        c->getInfo(m_records[static_cast<size_t>(u)].profile, true, false, infoCallback(u));
    }
    const int32_t p = newPlayer();
    OnlinePlayer& pl = m_players[static_cast<size_t>(p)];
    pl.user = u;
    pl.unused = 0;
    pl.online = 1;
    pl.playing = 0;
    pl.address.reset();
    pl.session = 0;
    pl.friendNote = 0;
    pl.inviteNote = 0;
    loggedOn = 1;
    if (std::shared_ptr<gp::Connection> c = gpConnection()) c->setInvitableGames({kProductId});
    if (loggedOnHook) loggedOnHook();
}

// 0x0055E470: the buddy's status (gpGetBuddyStatus): a new record of its profile (its nick asked), its location
// string read; its friend found or made, its record the new one, online (any status), playing (2), playing the game
// (2 and the same game), its address and session, the same game, our request no longer waiting.
void OnlineUser::onBuddyStatus(const gp::BuddyStatusArg& arg) {
    OnlineFriends* friends = referenceFriends();
    gp::BuddyStatus st;
    std::shared_ptr<gp::Connection> c = gpConnection();
    if (c && c->getBuddyStatus(arg.index, st) == gp::Result::NoError) {
        const int32_t u = newUser();
        m_records[static_cast<size_t>(u)].profile = arg.profile;
        m_records[static_cast<size_t>(u)].nick.clear();
        userAddRef(u);
        c->getInfo(m_records[static_cast<size_t>(u)].profile, true, false, infoCallback(u));
        std::optional<Address> address;
        int32_t session = 0;
        int32_t same = 0;
        parseLocation(st.locationString, address, session, same);
        int32_t e = friends->find(arg.profile);
        if (e == -1) e = friends->newEntry();
        OnlineFriend& f = e == -1 ? g_noFriend : friends->entries()[static_cast<size_t>(e)];
        // The fixed build: the friend's nick kept until the new record's answer (its info not asked blocking).
        if (f.user != -1) m_records[static_cast<size_t>(u)].nick = record(f.user).nick;
        userRelease(f.user);
        f.user = u;
        f.online = st.status != 0 ? 1 : 0;
        f.playingJuiced = st.status == 2 && same != 0 ? 1 : 0;
        f.playing = st.status == 2 ? 1 : 0;
        f.address = address;
        f.session = session;
        f.sameGame = same;
        f.requestSent = 0;
    }
    friends->release();
}

// 0x0055E8F0: a buddy request: its friend found, or a temporary one made (a new record of the profile, its nick
// asked); their request noted; our first local player's note (the stock referenced the player, never let go).
void OnlineUser::onBuddyRequest(const gp::BuddyRequestArg& arg) {
    OnlineFriends* friends = referenceFriends();
    int32_t e = friends->find(arg.profile);
    if (e == -1) {
        e = friends->newEntry();
        const int32_t u = newUser();
        m_records[static_cast<size_t>(u)].profile = arg.profile;
        m_records[static_cast<size_t>(u)].nick.clear();
        userAddRef(u);
        if (std::shared_ptr<gp::Connection> c = gpConnection()) {
            c->getInfo(m_records[static_cast<size_t>(u)].profile, true, false, infoCallback(u));
        }
        OnlineFriend& f = e == -1 ? g_noFriend : friends->entries()[static_cast<size_t>(e)];
        f.user = u;
        f.temporary = 1;
    }
    (e == -1 ? g_noFriend : friends->entries()[static_cast<size_t>(e)]).requestFrom = 1;
    friends->release();
    if (localPlayers.empty()) return;
    m_players[static_cast<size_t>(localPlayers[0])].friendNote = 1;
}

// 0x0055EA70: an invitation to the game's product only: its friend found or made (temporary), the location's address,
// session and same game, their invitation noted; our first local player's note (the stock's referenced, never let
// go).
void OnlineUser::onGameInvite(const gp::GameInviteArg& arg) {
    if (arg.productId != kProductId) return;
    OnlineFriends* friends = referenceFriends();
    int32_t e = friends->find(arg.profile);
    if (e == -1) {
        e = friends->newEntry();
        const int32_t u = newUser();
        m_records[static_cast<size_t>(u)].profile = arg.profile;
        m_records[static_cast<size_t>(u)].nick.clear();
        userAddRef(u);
        if (std::shared_ptr<gp::Connection> c = gpConnection()) {
            c->getInfo(m_records[static_cast<size_t>(u)].profile, true, false, infoCallback(u));
        }
        OnlineFriend& f = e == -1 ? g_noFriend : friends->entries()[static_cast<size_t>(e)];
        f.user = u;
        f.temporary = 1;
    }
    std::optional<Address> address;
    int32_t session = 0;
    int32_t same = 0;
    parseLocation(arg.location, address, session, same);
    OnlineFriend& f = e == -1 ? g_noFriend : friends->entries()[static_cast<size_t>(e)];
    f.address = address;
    f.session = session;
    f.sameGame = same;
    f.inviteFrom = 1;
    friends->release();
    if (localPlayers.empty()) return;
    m_players[static_cast<size_t>(localPlayers[0])].inviteNote = 1;
}

// ---- The friends object.

OnlineFriends::OnlineFriends() : m_user(g_user) {
    if (m_user != nullptr) m_user->addRef();
}

OnlineFriends::~OnlineFriends() {
    if (m_user != nullptr) m_user->release();
}

void OnlineFriends::release() {
    if (--m_refs == 0) delete this;
}

// 0x00560120: the first friend with no references (or a new one): user and address none, the rest 0 (Stock problem:
// the session and the same-game flag are whatever the stack held; 0 here); referenced and on the list.
int32_t OnlineFriends::newEntry() {
    size_t i = 0;
    while (i < m_entries.size() && m_entries[i].refs != 0) ++i;
    if (i == m_entries.size()) {
        m_entries.push_back(OnlineFriend{});
    } else {
        m_entries[i] = OnlineFriend{};
    }
    entryAddRef(static_cast<int32_t>(i));
    m_list.push_back(static_cast<int32_t>(i));
    return static_cast<int32_t>(i);
}

void OnlineFriends::entryAddRef(int32_t entry) {
    if (entry != -1) ++at(entry).refs;
}

void OnlineFriends::entryRelease(int32_t entry) {
    if (entry == -1) return;
    OnlineFriend& f = at(entry);
    --f.refs;
    if (f.refs == 0 && m_user != nullptr) m_user->userRelease(f.user);
}

bool OnlineFriends::sameUser(int32_t a, int32_t b) {
    if (a == -1) return b == -1;
    if (b == -1 || m_user == nullptr) return false;
    return m_user->sameUser(at(a).user, at(b).user);
}

// The fixed build: the local players not referenced (the stock referenced each at every refresh, never let go), the
// refresh goes on after a friend gone (in its place the list's last, not yet aged).
void OnlineFriends::update() {
    if (m_user == nullptr) return;
    for (size_t k = 0; k < m_user->localPlayers.size(); ++k) {
        const int32_t p = m_user->localPlayers[k];
        m_user->player(p).friendNote = 0;
        m_user->player(p).inviteNote = 0;
    }
    for (size_t k = 0; k < m_list.size();) {
        OnlineFriend& f = m_list[k] == -1 ? g_noFriend : at(m_list[k]);
        if (f.requestSent != 0) {
            ++f.requestAge;
            if (static_cast<uint32_t>(f.requestAge) >= 10) {
                f.requestSent = 0;
                listRemove(m_list[k]);
                continue;
            }
        }
        if (f.inviteSent != 0) {
            ++f.inviteAge;
            if (static_cast<uint32_t>(f.inviteAge) >= 10) f.inviteSent = 0;
        }
        ++k;
    }
}

// 0x00560040: the friend taken off the list (its reference let go; the last friend in its place).
void OnlineFriends::listRemove(int32_t entry) {
    for (size_t i = 0; i < m_list.size(); ++i) {
        if (m_list[i] == entry) {
            entryRelease(entry);
            m_list[i] = m_list.back();
            m_list.pop_back();
            return;
        }
    }
}

int32_t OnlineFriends::entryUser(int32_t entry) {
    if (static_cast<uint32_t>(entry) >= m_entries.size()) return -1;
    if (m_user != nullptr) m_user->userAddRef(at(entry).user);
    return at(entry).user;
}

// 0x005605F0: the last friend on the list of the same user, referenced.
int32_t OnlineFriends::findUser(int32_t user) {
    if (user == -1) return -1;
    int32_t found = -1;
    for (int32_t e : m_list) {
        const int32_t u = e == -1 ? g_noFriend.user : at(e).user;
        if (m_user != nullptr && m_user->sameUser(u, user)) found = e;
    }
    entryAddRef(found);
    return found;
}

int32_t OnlineFriends::find(int32_t profile) {
    for (int32_t e : m_list) {
        const int32_t u = e == -1 ? g_noFriend.user : at(e).user;
        if (m_user != nullptr && m_user->record(u).profile == profile) return e;
    }
    return -1;
}

void OnlineFriends::addFriend(int32_t user, int32_t /*unused*/) {
    if (m_user == nullptr) return;
    const int32_t profile = m_user->record(user).profile;
    if (find(profile) != -1) return;
    const int32_t e = newEntry();
    m_user->userAddRef(user);
    OnlineFriend& f = e == -1 ? g_noFriend : at(e);
    f.user = user;
    f.requestSent = 1;
    if (std::shared_ptr<gp::Connection> c = gpConnection()) c->sendBuddyRequest(profile, u"");
    changed = 1;
}

void OnlineFriends::answer(int32_t entry, int32_t deny) {
    if (entry == -1 || m_user == nullptr) return;
    const int32_t profile = m_user->record(at(entry).user).profile;
    if (std::shared_ptr<gp::Connection> c = gpConnection()) {
        if (deny != 0) {
            c->denyBuddyRequest(profile);
        } else {
            c->authBuddyRequest(profile);
        }
    }
    at(entry).requestFrom = 0;
    if (at(entry).temporary != 0) listRemove(entry);
}

void OnlineFriends::remove(int32_t entry) {
    if (entry == -1 || m_user == nullptr) return;
    const int32_t profile = m_user->record(at(entry).user).profile;
    if (std::shared_ptr<gp::Connection> c = gpConnection()) {
        if (c->state() == gp::State::Disconnected) {
            listRemove(entry);
            return;
        }
        c->deleteBuddy(profile);
    }
    listRemove(entry);
}

std::optional<Address> OnlineFriends::address(int32_t entry) const {
    if (entry == -1) return std::nullopt;
    return at(entry).address;
}

void OnlineFriends::invite(int32_t entry, const std::optional<Address>& /*address*/, int32_t /*unused*/) {
    if (entry == -1 || m_user == nullptr) return;
    const int32_t profile = m_user->record(at(entry).user).profile;
    // Our first player's location (the stock referenced the player, never let go; not the fixed build).
    const int32_t p = m_user->localPlayers.empty() ? -1 : m_user->localPlayers[0];
    std::u16string where;
    if (p != -1) where = m_user->location(p);
    if (std::shared_ptr<gp::Connection> c = gpConnection()) c->invitePlayer(profile, kProductId, where);
    at(entry).inviteSent = 1;
    at(entry).inviteAge = 0;
    changed = 1;
}

const std::u16string& OnlineFriends::gameName(int32_t /*entry*/) const {
    return kEmpty;
}

void OnlineFriends::clearInvite(int32_t entry, int32_t /*unused*/) {
    if (entry == -1) return;
    at(entry).inviteFrom = 0;
    if (at(entry).temporary != 0) listRemove(entry);
}

// ---- The globals.

std::shared_ptr<gp::Connection>& gpConnection() {
    static std::shared_ptr<gp::Connection> connection;
    return connection;
}

OnlineUser* userSingleton() {
    return g_user;
}

OnlineUser* onlineUser() {
    return g_gameUser;
}

void endOnlineUser() {
    if (g_gameUser != nullptr) {
        OnlineUser* u = g_gameUser;
        g_gameUser = nullptr;
        u->release();
    }
    if (g_user != nullptr) {
        OnlineUser* u = g_user;
        g_user = nullptr;
        u->release();
    }
}

void makeOnlineUser() {
    if (g_gameUser != nullptr) return;
    endOnlineUser();
    // 0x0055E060: the user object before let go, a new one.
    if (g_user != nullptr) {
        OnlineUser* u = g_user;
        g_user = nullptr;
        u->release();
    }
    g_user = new OnlineUser();
    g_user->addRef();
    g_gameUser = g_user;
}

OnlineFriends* friendsSingleton() {
    return g_friends;
}

OnlineFriends* referenceFriends() {
    if (g_friends == nullptr) g_friends = new OnlineFriends();
    g_friends->addRef();
    return g_friends;
}

void releaseFriendsSingleton() {
    if (g_friends == nullptr) return;
    OnlineFriends* f = g_friends;
    g_friends = nullptr;
    f->release();
}

OnlineFriends* onlineFriends() {
    return g_gameFriends;
}

void remakeOnlineFriends() {
    if (g_gameFriends != nullptr) {
        OnlineFriends* f = g_gameFriends;
        g_gameFriends = nullptr;
        f->release();
    }
    releaseFriendsSingleton();
    g_gameFriends = referenceFriends();
}

void endOnlineFriends(bool singleton) {
    if (g_gameFriends != nullptr) {
        OnlineFriends* f = g_gameFriends;
        g_gameFriends = nullptr;
        f->release();
    }
    if (singleton) releaseFriendsSingleton();
}

bool& onlineUp() {
    static bool up = false;
    return up;
}

OnlineHooks& onlineHooks() {
    static OnlineHooks hooks;
    return hooks;
}

void endOnline() {
    peerchat::end();  // the fixed build's lobby chat
    endOnlineFriends(true);
    OnlineHooks& h = onlineHooks();
    releaseMatchmaking();
    if (gameVoice() != nullptr) endGameVoice();
    if (h.statsClosed) h.statsClosed();
    endOnlineUser();
    if (onlineUp()) {
        stopWinsock();
        onlineUp() = false;
    }
}

}  // namespace oj::net
