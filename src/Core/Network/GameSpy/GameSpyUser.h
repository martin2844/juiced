#pragma once
// The game's side of GameSpy Presence (docs/spec/28-network.md, "GameSpy Presence"): the user object (CJETUser_gs,
// 0x0055B000 .. 0x0055FB90): the account's login through the available check and the SDK's connection, its user
// records and local players, the presence (status and location string) and the GP callbacks; the friends object
// (CJETFriends_gs, 0x0055FE20 .. 0x00560B90): the buddies, their requests and invitations.

#include "Core/Network/GameSpy/GameSpy.h"
#include "Core/Network/GameSpy/GameSpyPresence.h"
#include "Core/Network/NetTransport.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oj::net {

// A user of the user object (0x48 bytes, +0x120's list): its references, its profile and its nick (32 wide
// characters, from the profile's info).
struct OnlineUserRecord {
    int32_t refs = 0;
    int32_t profile = 0;
    std::u16string nick;
};

// A player (0x24 bytes, +0x130's list).
struct OnlinePlayer {
    int32_t refs = 0;
    int32_t user = -1;      // +0x04: its user record
    int32_t unused = 0;     // +0x08: 0 when made, read by nothing
    int32_t online = 0;     // +0x0C
    int32_t playing = 0;    // +0x10
    // +0x14: its address entry (the transport's table [0x0079E2CC], counted references; -1 none), held here as the
    // address itself.
    std::optional<Address> address;
    int32_t session = 0;     // +0x18: the matchmaking's session
    int32_t friendNote = 0;  // +0x1C: a friend's request came
    int32_t inviteNote = 0;  // +0x20: a game invitation came
};

// 0x0055EF50's string: empty off a game; else "<game>://<ip>:<port>" of the player's address (or
// "<game>://0.0.0.0:0" without one), then "/?session=<n>".
std::u16string buildLocation(const OnlinePlayer& player);
// 0x0055E6B0: the game's name before the first ':' (at most 31 characters; the same game when it is ours, `same` 1),
// "//a.b.c.d:port/" up to the first '?' (a new address entry), "session=<n>" (case aside) of the parts after it. An
// empty string changes nothing.
void parseLocation(std::u16string_view location, std::optional<Address>& address, int32_t& session, int32_t& same);

// The user object's state (its +0x24): none, a GP call's result, the service unavailable or a GP error's code.
enum class UserState : int32_t {
    None = 0,
    ParameterError = 1,  // a call's parameter error, or an error of another code
    MemoryError = 2,
    NetworkError = 3,
    ServerError = 4,
    Unavailable = 5,       // the service unavailable
    ForcedDisconnect = 6,  // signed on elsewhere
    ConnectionClosed = 7,
    LoginTimeout = 8,
    BadNick = 9,
    BadEmail = 10,
    BadPassword = 11,  // and the database's error
    BadProfile = 12,
    ProfileDeleted = 13,
    ConnectionFailed = 14,
    AuthFailed = 15,  // the server's authentication failed
};

// The user object (vtable 0x006FA638, its base 0x006FA330; 0x1D0 bytes; made by 0x0055E060 into [0x0075F1E4]).
class OnlineUser {
public:
    OnlineUser();   // 0x0055E110
    ~OnlineUser();  // 0x0055DFE0: signed out
    OnlineUser(const OnlineUser&) = delete;
    OnlineUser& operator=(const OnlineUser&) = delete;

    // The references (+0x04): the object goes at the last.
    void addRef() { ++m_refs; }
    void release();

    // +0x04 (0x0055EDC0): the player's presence: Offline (0), Online (1) or Playing (2) as the status and its text,
    // the location string (0x0055EF50) and gpSetStatusW; its result into the state.
    void sendPresence(int32_t player);
    // +0x08 (0x0055F220): a free record (or a new one) of the profile and the nick, referenced; its index.
    int32_t addUser(int32_t profile, std::u16string_view nick);
    // +0x0C (0x0055F140): a free record (or a new one) referenced; its index.
    int32_t newUser();
    void userAddRef(int32_t user);   // +0x10 (0x0055F330)
    void userRelease(int32_t user);  // +0x14 (0x0055F350)
    bool sameUser(int32_t a, int32_t b) const;  // +0x18 (0x0055F370): both none, or the same profile
    const std::u16string& userNick(int32_t user) const;  // +0x1C (0x0055F3C0): L"" for none
    OnlineUserRecord& record(int32_t user);              // +0x24 (0x0055F3F0)
    // +0x28 (0x0055F410): a free player (or a new one) referenced and put on the local players; its index.
    int32_t newPlayer();
    void playerAddRef(int32_t player);   // +0x2C (0x0055F570)
    void playerRelease(int32_t player);  // +0x30 (0x0055F590): at no references its user let go
    const std::u16string& playerNick(int32_t player) const;  // +0x34 (0x0055F5D0)
    int32_t playerUser(int32_t player);  // +0x38 (0x0055F600): referenced
    // +0x3C, +0x40, +0x44 (0x0055F640 .. 0x0055F6C0): the player online, playing, at an address (with the
    // matchmaking's session); the presence then sent within a second (the timer raised to 59).
    void setOnline(int32_t player, int32_t online);
    // +0x40 takes a third argument (0x004EEAF0: the session being public; ret 0xC) and does not use it.
    void setPlaying(int32_t player, int32_t playing, int32_t isPublic);
    void setAddress(int32_t player, std::optional<Address> address);
    int32_t friendNote(int32_t player) const { return m_players[static_cast<size_t>(player)].friendNote; }  // +0x4C
    int32_t inviteNote(int32_t player) const { return m_players[static_cast<size_t>(player)].inviteNote; }  // +0x50
    // The base's voice mail and feedback, nothing on the PC (the windows call them and never get further):
    int32_t voiceMailWaiting(int32_t /*player*/) const { return 0; }  // +0x54 (0x0051F960)
    int32_t feedbackOn() const { return 0; }                          // +0x58 (0x0055B910)
    void sendFeedback(int32_t /*player*/, int32_t /*user*/, int32_t /*kind*/) {}  // +0x5C (0x0055FE60)
    int32_t askVoiceMail(int32_t /*player*/) { return 1; }            // +0x60 (0x0042E470): the mail asked for
    int32_t voiceMail(int32_t /*player*/, int32_t /*user*/, int32_t /*index*/) { return -1; }  // +0x68 (0x0055B920)
    int32_t voiceMailNew(int32_t /*player*/, int32_t /*mail*/) { return 0; }   // +0x70 (0x005285D0)
    void readVoiceMail(int32_t /*player*/, int32_t /*mail*/) { voiceMailRead = 1; }  // +0x78 (0x0055B940)
    // +0x84 (0x0055F880): each frame (the game's loop, 0x0040CF48; the logon windows' loop with 50).
    void update(float dt);
    // +0x90 (0x0055B910, the body of +0x58): 0; the front end's and the race's network watches call it each frame
    // (0x0046C65B, 0x004581DD), the race keeping its answer.
    int32_t networkFrame() const { return 0; }
    // +0x88 (0x0055FA60): the available check stopped, the connection logged out and destroyed, the local players
    // let go, not logged on.
    void signOut();
    const std::u16string& password() const { return m_password; }  // +0x8C (0x0055DFD0)
    // 0x0055F090: the state 0, signed out, the account kept, the available check started; the next update logs in
    // (a new profile with newUser).
    void setCredentials(std::u16string_view nick, std::u16string_view email, std::u16string_view password,
                        bool newUser);
    // 0x0055EF50: the player's location string (+0x140).
    const std::u16string& location(int32_t player);
    // The fixed build's guest (the user, 2026-10-24: online without a GameSpy account, the matchmaking only): signed
    // out and the available check started (as a login's: the server browser is made only after its "available");
    // once it answers available the next frame logs on without a connection under the nick, a record of a profile of
    // its own below -1 (GameSpy's are above 0: the others' machines tell the guests apart by it) and a local player
    // online, the logged-on hook; unavailable, the state 5. A guest sends no presence and has no statistics; the
    // windows leave out the friends and the scoreboards.
    void setGuest(std::u16string_view nick);
    bool guest() const { return m_guest; }

    // +0x24: while None a call's GP result (0x0055E1E0); Unavailable the service; an error's code (0x0055E230: before
    // the login any error, then a fatal one).
    UserState state = UserState::None;
    int32_t loggedOn = 0;  // +0x0C
    // +0x10: called once logged on (the game's 0x004EE990).
    std::function<void()> loggedOnHook;
    std::vector<int32_t> localPlayers;  // +0x14
    int32_t voiceMailRead = 0;          // +0x28: a voice mail read (the base's +0x78 sets it)
    float presenceTimer = 0.0f;         // +0x30
    // +0x1C8: 1 while the periodic available check says the service is up (0 a frame on its "unavailable", 0 after
    // 15 s without its answer).
    int32_t serviceOk() const { return m_serviceOk; }

    const std::vector<OnlineUserRecord>& records() const { return m_records; }
    const std::vector<OnlinePlayer>& players() const { return m_players; }
    OnlinePlayer& player(int32_t index) { return m_players[static_cast<size_t>(index)]; }

    // Not of the game: the connection manager the login reaches (the stock's gpcm.gamespy.com:29900 and gp.info;
    // the tests give their stand-in).
    gp::Connection::Server server;

private:
    void resultToState(gp::Result result);  // 0x0055E1E0
    int32_t periodicCheck(float dt);        // 0x0055F780
    void guestLoggedOn();                   // the fixed build's guest, the available check answered
    void baseUpdate(float dt);              // 0x0055BAF0
    void baseRelease();                     // 0x0055BB60
    void onError(const gp::ErrorArg& arg);                   // 0x0055E230
    void onBuddyStatus(const gp::BuddyStatusArg& arg);       // 0x0055E470
    void onBuddyRequest(const gp::BuddyRequestArg& arg);     // 0x0055E8F0
    void onGameInvite(const gp::GameInviteArg& arg);         // 0x0055EA70
    void onConnect(const gp::ConnectResponse& response);     // 0x0055ECB0
    gp::Connection::InfoCallback infoCallback(int32_t user);  // 0x0055FB10

    int32_t m_refs = 1;
    std::u16string m_nick;      // +0x34
    std::u16string m_email;     // +0x72
    std::u16string m_password;  // +0xD8
    int32_t m_newUser = 0;      // +0x118
    int32_t m_connectAsked = 0;  // +0x11C
    std::vector<OnlineUserRecord> m_records;  // +0x120
    std::vector<OnlinePlayer> m_players;      // +0x130
    std::u16string m_location;                // +0x140
    std::unique_ptr<AvailableCheck> m_check;  // the SDK's one check ([0x0075B570] ..)
    // Not of the game: the connection this object made (the fixed build's sign-out lets go only its own).
    std::weak_ptr<gp::Connection> m_connection;
    bool m_guest = false;  // the fixed build's guest
    int32_t m_checkRunning = 0;  // +0x1C0
    float m_checkTimer = 10.0f;  // +0x1C4
    int32_t m_serviceOk = 1;     // +0x1C8
    int32_t m_checkOk = 1;       // +0x1CC
};

// A friend of the friends object (0x3C bytes, +0x24's list).
struct OnlineFriend {
    int32_t refs = 0;
    int32_t user = -1;          // +0x04: its user record
    int32_t online = 0;         // +0x08
    int32_t playing = 0;        // +0x0C
    int32_t playingJuiced = 0;  // +0x10
    std::optional<Address> address;  // +0x14
    int32_t session = 0;        // +0x18
    int32_t sameGame = 0;       // +0x1C
    int32_t requestFrom = 0;    // +0x20: a buddy request from them
    int32_t requestSent = 0;    // +0x24: our request waiting
    int32_t requestAge = 0;     // +0x28: the refreshes since
    int32_t inviteFrom = 0;     // +0x2C: a game invitation from them
    int32_t inviteSent = 0;     // +0x30: our invitation
    int32_t inviteAge = 0;      // +0x34
    int32_t temporary = 0;      // +0x38: not on the buddy list
};

// The friends object (vtable 0x006FA7B0, its base 0x006FA730; 0x34 bytes; the one of [0x0075F284]).
class OnlineFriends {
public:
    OnlineFriends();   // 0x005600A0 (0x0055FED0): the user ([0x0075F1E4]) referenced
    ~OnlineFriends();  // 0x005600F0: the user's reference let go
    OnlineFriends(const OnlineFriends&) = delete;
    OnlineFriends& operator=(const OnlineFriends&) = delete;

    void addRef() { ++m_refs; }
    void release();

    int32_t newEntry();                   // +0x04 (0x00560120): referenced and on the list
    void entryAddRef(int32_t entry);      // +0x08 (0x00560300)
    void entryRelease(int32_t entry);     // +0x0C (0x00560320): at no references its user let go
    bool sameUser(int32_t a, int32_t b);  // +0x10 (0x00560360)
    void askList() { listAsked = 1; }     // +0x14 (0x0055FE20)
    // +0x18 (0x005603C0): the local players' notes cleared; each friend's request waiting ages, gone after 10 (the
    // stock's refresh then ended, the friends after it not aged); an invitation waiting ages, off after 10.
    void update();
    int32_t entryUser(int32_t entry);     // +0x1C (0x00560570): referenced
    int32_t findUser(int32_t user);       // +0x20 (0x005605F0): the friend of that user (its entry referenced)
    // +0x24 (0x0055FE30): nothing (called as the friends' windows are left).
    void windowsLeft() {}
    // +0x28 (0x00560690): a new friend of the user: our request sent (\addbuddy\, no reason); the second argument
    // taken and unused (ret 8).
    void addFriend(int32_t user, int32_t unused);
    // +0x2C (0x00560760): a request answered (\authadd\, or denied when `deny` is not 0: 1, or 2 from the block
    // question), a temporary friend gone.
    void answer(int32_t entry, int32_t deny);
    // +0x30 (0x005607E0): \delbuddy\ (unless disconnected) and the friend gone.
    void remove(int32_t entry);
    int32_t canCancelRequest() const { return 0; }  // +0x34 (0x0055B910): 0
    int32_t canBlock() const { return 0; }          // +0x38 (0x0055B910): 0
    int32_t requestFrom(int32_t entry) const { return entry == -1 ? 0 : at(entry).requestFrom; }  // +0x3C
    int32_t requestSent(int32_t entry) const { return entry == -1 ? 0 : at(entry).requestSent; }  // +0x40
    int32_t inviteFrom(int32_t entry) const { return entry == -1 ? 0 : at(entry).inviteFrom; }    // +0x44
    int32_t inviteSent(int32_t entry) const { return entry == -1 ? 0 : at(entry).inviteSent; }    // +0x48
    int32_t playingJuiced(int32_t entry) const { return entry == -1 ? 0 : at(entry).playingJuiced; }  // +0x4C
    int32_t online(int32_t entry) const { return entry == -1 ? 0 : at(entry).online; }            // +0x50
    int32_t playing(int32_t entry) const { return entry == -1 ? 0 : at(entry).playing; }          // +0x54
    int32_t temporary(int32_t entry) const { return entry == -1 ? 0 : at(entry).temporary; }      // +0x58
    int32_t voiceMail(int32_t /*entry*/) const { return 0; }  // +0x5C (0x0051F960)
    std::optional<Address> address(int32_t entry) const;  // +0x60 (0x005609E0)
    const std::u16string& gameName(int32_t entry) const;  // +0x64 (0x0055FE50): L""
    int32_t sameGame(int32_t entry) const { return entry == -1 ? 0 : at(entry).sameGame; }        // +0x68
    // +0x6C (0x00560A60): \pinvite\ with our first player's location string; our invitation waiting. The address
    // and the third argument taken and unused (ret 0xC).
    void invite(int32_t entry, const std::optional<Address>& address, int32_t unused);
    // +0x70 (0x00560B00): their invitation dealt with, a temporary friend gone; the second argument unused (ret 8).
    void clearInvite(int32_t entry, int32_t unused);
    // +0x74 (0x0055FE70): nothing (an invitation is not taken back).
    void cancelInvite(int32_t /*entry*/, const std::optional<Address>& /*address*/) {}
    int32_t canCancelInvite() const { return 0; }  // +0x78 (0x0055B910): 0
    // 0x00560B40: the friend of the profile on the list; -1 none.
    int32_t find(int32_t profile);

    const std::vector<int32_t>& list() const { return m_list; }  // +0x10: the friends shown (entries)
    std::vector<OnlineFriend>& entries() { return m_entries; }   // +0x24
    int32_t listAsked = 0;  // +0x0C: the friends' windows asked for the list (askList)
    int32_t changed = 0;  // +0x20: the list to be shown again

private:
    const OnlineFriend& at(int32_t entry) const { return m_entries[static_cast<size_t>(entry)]; }
    OnlineFriend& at(int32_t entry) { return m_entries[static_cast<size_t>(entry)]; }
    void listRemove(int32_t entry);  // 0x00560040

    int32_t m_refs = 1;
    OnlineUser* m_user = nullptr;          // +0x08
    std::vector<int32_t> m_list;           // +0x10
    std::vector<OnlineFriend> m_entries;   // +0x24
};

// The SDK's connection ([0x0075B34C]; one for whatever user object).
std::shared_ptr<gp::Connection>& gpConnection();
// [0x0075F1E4]: the user object; [0x0075F040]: the game's reference to it.
OnlineUser* userSingleton();
OnlineUser* onlineUser();
// 0x00471920's part: [0x0075F040] none: 0x004EEA10, a new user object (0x0055E060) and the game's reference.
void makeOnlineUser();
// 0x004EEA10: both references let go (the last signs out).
void endOnlineUser();
// [0x0075F284]: the friends object; 0x0055FFC0 makes it when there is none and references it; 0x0055FE80 lets the
// singleton's reference go.
OnlineFriends* friendsSingleton();
OnlineFriends* referenceFriends();
void releaseFriendsSingleton();
// [0x0075EF7C]: the game's friends; 0x004E17C0 makes them again (the logged-on hook), 0x004E1810 lets them go (the
// singleton's reference too with `singleton`).
OnlineFriends* onlineFriends();
void remakeOnlineFriends();
void endOnlineFriends(bool singleton);
// [0x0075F1EC]: the online services up (0x00471920: Winsock started and the game's name, key and product set).
bool& onlineUp();

// The game's online parts the user object's code reaches through hooks: the statistics' let go.
struct OnlineHooks {
    // 0x004EC590: the statistics ([0x0075F030]) let go.
    std::function<void()> statsClosed;
};
OnlineHooks& onlineHooks();

// The online objects let go as the windows do it (online_main's sign-out 0x004743AA, the race's quit 0x00457EA4, the
// driver's BACK 0x0046834C, the way back to the title 0x004688A4): the friends (both references, 0x004E1810 with 1),
// the matchmaking (0x004E1A20), the voice (0x004EECA0), the statistics (0x004EC590), the user object (0x004EEA10),
// Winsock when the online services were up (the flag 0).
void endOnline();

}  // namespace oj::net
