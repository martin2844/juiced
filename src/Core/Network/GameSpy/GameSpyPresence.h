#pragma once
// GameSpy Presence ("GP") as the stock game links it (docs/spec/28-network.md, "GameSpy Presence"): the connection
// manager (gpcm.gamespy.com:29900/TCP, 0x00573930) and what the game asks of it: the login with the challenges'
// proofs, a new profile, the profiles' info and its cache (gp.info), the status, the buddies (their status, their
// requests, the game invitations, their pings), the errors and the logout. Messages are "\key\value...\final\".
//
// What the executable keeps of the SDK that nothing reaches (the linker dropped the calls that start it): the search
// manager (gpsp.gamespy.com:29901, 0x00571500 .. 0x00573170: its searches and their answers, only a profile search's
// next page starting one), the answers of a new profile (\npr\) and of a unique nick (\rn\), the profile's updates
// (\updatepro\, \updateui\: nothing fills them); the peers (0x0057AEF0 .. 0x0057C020: TCP straight between two
// players, ours listening only without the firewall flag, which the game always gives, theirs reached only for a
// buddy with a port, which no player of the game has) and the files sent over them (0x00576E70 .. 0x00578FE0: the
// transfers, whose callback the game never sets); the simulation (+0x108, never set).

#include "Core/Network/GameSpy/GameSpy.h"
#include "Core/Network/NetTcp.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oj::net::gp {

// GPResult.
enum class Result : int32_t { NoError = 0, MemoryError = 1, ParameterError = 2, NetworkError = 3, ServerError = 4 };

// GPErrorCode: the SDK's own and the server's (\err\) that the game tells apart.
inline constexpr int32_t kParse = 0x0001;
inline constexpr int32_t kDatabase = 0x0004;
inline constexpr int32_t kNetwork = 0x0005;
inline constexpr int32_t kForcedDisconnect = 0x0006;
inline constexpr int32_t kConnectionClosed = 0x0007;
inline constexpr int32_t kLoginTimeout = 0x0101;
inline constexpr int32_t kLoginBadNick = 0x0102;
inline constexpr int32_t kLoginBadEmail = 0x0103;
inline constexpr int32_t kLoginBadPassword = 0x0104;
inline constexpr int32_t kLoginBadProfile = 0x0105;
inline constexpr int32_t kLoginProfileDeleted = 0x0106;
inline constexpr int32_t kLoginConnectionFailed = 0x0107;
inline constexpr int32_t kLoginServerAuthFailed = 0x0108;
inline constexpr int32_t kNewUserBadNick = 0x0201;
inline constexpr int32_t kNewUserBadPassword = 0x0202;

// The connection's states (+0x1F4).
enum class State : int32_t { None = 0, Connecting = 1, Connected = 2, LoggedIn = 3, Disconnected = 4 };

// 0x0056F340: the MD5 digest of the bytes in lowercase hex (32 characters).
std::string md5Hex(std::string_view bytes);
// MD5 by parts (RFC 1321): MD5Init (0x0056E8C0, a new context), MD5Update (0x0056E8F0), MD5Final (0x0056E9B0, the
// context zeroed after) with the digest in lowercase hex (0x0056F310).
class Md5Context {
public:
    void update(const void* data, size_t size);
    std::string finalHex();

private:
    void block(const uint8_t* p);
    uint32_t m_state[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
    uint64_t m_bits = 0;
    uint8_t m_buffer[64] = {};
};
// The login's response and the server's proof (0x00573F00, 0x00574690): md5hex(pwhash + 48 spaces + user + first +
// second + pwhash); the response has the client's challenge first, the proof the server's.
std::string loginHash(std::string_view passwordHash, std::string_view user, std::string_view first,
                      std::string_view second);
// 0x005742E0: a password (or a CD key) for \newuser\: each byte XORed with the SDK's generator (seeded 0x79707367,
// "gspy", for each string; the seed x 16807 mod 2^31 - 1 by halves; the byte ^ seed % 255), then base64 with '[' and
// ']' for 62 and 63 and '_' for the padding (0x0056E7D0 mode 1).
std::string passwordEncode(std::string_view password);
// 0x00568C70 / 0x00568D00: UCS-2 to UTF-8 and back (one to three bytes; a byte of no sequence gives '?').
std::string toUtf8(std::u16string_view text);
std::u16string fromUtf8(std::string_view text);

// The callbacks' arguments (the SDK's wide ones, GSI_UNICODE).
struct ErrorArg {  // GPErrorArg
    Result result = Result::NoError;
    int32_t errorCode = 0;
    std::u16string errorString;
    bool fatal = false;
};
struct ConnectResponse {  // GPConnectResponseArg
    Result result = Result::NoError;
    int32_t profile = 0;
    std::u16string uniquenick;
};
struct GetInfoResponse {  // GPGetInfoResponseArg (0x3F0 bytes, 0x0057A0B0)
    Result result = Result::NoError;
    int32_t profile = 0;
    std::u16string nick, uniquenick, email, firstname, lastname, homepage;
    int32_t icquin = 0;
    std::u16string zipcode, countrycode;
    float longitude = 0.0f;
    float latitude = 0.0f;
    std::u16string place;
    int32_t birthday = 0, birthmonth = 0, birthyear = 0;
    int32_t sex = 0;
    int32_t publicmask = 0;
    std::u16string aimname;
    int32_t pic = 0, occupationid = 0, industryid = 0, incomeid = 0, marriedid = 0, childcount = 0, interests1 = 0,
            ownership1 = 0, conntypeid = 0;
};
struct BuddyRequestArg {  // GPRecvBuddyRequestArg
    int32_t profile = 0;
    uint32_t date = 0;
    std::u16string reason;
};
struct BuddyStatusArg {  // GPRecvBuddyStatusArg
    int32_t profile = 0;
    uint32_t date = 0;
    int32_t index = 0;
};
struct BuddyMessageArg {  // GPRecvBuddyMessageArg
    int32_t profile = 0;
    uint32_t date = 0;
    std::u16string message;
};
struct GameInviteArg {  // GPRecvGameInviteArg
    int32_t profile = 0;
    int32_t productId = 0;
    std::u16string location;
};
struct BuddyStatus {  // GPBuddyStatus (gpGetBuddyStatus)
    int32_t profile = 0;
    int32_t status = 0;
    std::u16string statusString;
    std::u16string locationString;
    uint32_t ip = 0;    // network order
    uint16_t port = 0;  // network order
};

// A transfer's event (GPTransferCallbackArg, 0x14 bytes): the transfer's id, the event, the file, a number (the
// files of a request, a file's bytes so far, a failed file's reason, the rate) and a message (a request's, a reply's).
inline constexpr int32_t kTransferSendRequest = 0x800;
inline constexpr int32_t kTransferAccepted = 0x801;
inline constexpr int32_t kTransferRejected = 0x802;
inline constexpr int32_t kTransferNotAccepting = 0x803;
inline constexpr int32_t kTransferNoConnection = 0x804;
inline constexpr int32_t kTransferDone = 0x805;
inline constexpr int32_t kTransferCancelled = 0x806;
inline constexpr int32_t kTransferLostConnection = 0x807;
inline constexpr int32_t kTransferError = 0x808;
inline constexpr int32_t kTransferThrottle = 0x809;
inline constexpr int32_t kFileBegin = 0x80A;
inline constexpr int32_t kFileProgress = 0x80B;
inline constexpr int32_t kFileEnd = 0x80C;
inline constexpr int32_t kFileDirectory = 0x80D;
inline constexpr int32_t kFileSkip = 0x80E;
inline constexpr int32_t kFileFailed = 0x80F;
inline constexpr int32_t kFileReadError = 0x900;
inline constexpr int32_t kFileWriteError = 0x901;
inline constexpr int32_t kFileDataError = 0x902;
struct TransferArg {
    int32_t transfer = 0;
    int32_t type = 0;
    int32_t index = 0;
    int32_t num = 0;
    std::u16string message;
};

// The search manager's answers (the searches' callbacks).
inline constexpr int32_t kMore = 0x600;  // GP_MORE
inline constexpr int32_t kDone = 0x601;  // GP_DONE
struct ProfileMatch {  // GPProfileSearchMatch (0x18C bytes)
    int32_t profile = 0;
    std::u16string nick, uniquenick, firstname, lastname, email;
};
struct ProfileSearchResponse {  // GPProfileSearchResponseArg
    Result result = Result::NoError;
    int32_t numMatches = 0;
    int32_t more = 0;  // kMore when more come (the callback may make it kDone to stop)
    std::vector<ProfileMatch> matches;
};
struct IsValidEmailResponse {  // GPIsValidEmailResponseArg (0x70 bytes)
    Result result = Result::NoError;
    std::u16string email;
    int32_t isValid = 0;
};
struct UserNicksResponse {  // GPGetUserNicksResponseArg (0x78 bytes)
    Result result = Result::NoError;
    std::u16string email;
    int32_t numNicks = 0;
    std::vector<std::u16string> nicks;
    std::vector<std::u16string> uniquenicks;  // one for each nick that had one after it
};
struct FoundPlayer {  // GPFindPlayerMatch (0x248 bytes)
    int32_t profile = 0;
    std::u16string nick;
    int32_t status = 1;
    std::u16string statusString;
};
struct FindPlayersResponse {  // GPFindPlayersResponseArg
    Result result = Result::NoError;
    int32_t productId = 0;
    int32_t numMatches = 0;
    std::vector<FoundPlayer> matches;
};
struct ProfileResponse {  // GPCheckResponseArg, GPNewUserResponseArg, GPNewProfileResponseArg (8 bytes)
    Result result = Result::NoError;
    int32_t profile = 0;
};
struct ReverseBuddiesResponse {  // GPGetReverseBuddiesResponseArg
    Result result = Result::NoError;
    int32_t numProfiles = 0;
    std::vector<ProfileMatch> profiles;
};
struct SuggestUniqueNickResponse {  // GPSuggestUniqueNickResponseArg
    Result result = Result::NoError;
    int32_t numSuggestedNicks = 0;
    std::vector<std::u16string> suggestedNicks;
};
struct ResultResponse {  // GPRegisterUniqueNickResponseArg (4 bytes)
    Result result = Result::NoError;
};

// A search of the search manager (GPISearchData, 0x160 bytes): its kind, what it asks and its callback.
struct Search {
    // 1 a profile search (\search\), 2 an email's validity (\valid\), 3 an email's nicks (\nicks\), 4 players of a
    // product (\pmatch\), 5 a user's check (\check\), 6 a new user (\newuser\), 7 the profiles that have us as a
    // buddy (\others\), 8 unique nicks suggested (\uniquesearch\).
    int32_t type = 0;
    std::string nick;        // +0x28 (31)
    std::string uniquenick;  // +0x47 (51)
    std::string email;       // +0x7A (51, lowercase)
    std::string firstname;   // +0xAD (31)
    std::string lastname;    // +0xCC (31)
    std::string password;    // +0xEB (31)
    std::string cdkey;       // +0x10A (66)
    int32_t icquin = 0;      // +0x14C
    int32_t skip = 0;        // +0x150
    int32_t productId = 0;   // +0x154
    std::function<void(ProfileSearchResponse&)> onProfiles;  // 1
    std::function<void(const IsValidEmailResponse&)> onValidEmail;  // 2
    std::function<void(const UserNicksResponse&)> onNicks;   // 3
    std::function<void(const FindPlayersResponse&)> onPlayers;  // 4
    std::function<void(const ProfileResponse&)> onProfile;   // 5, 6
    std::function<void(const ReverseBuddiesResponse&)> onOthers;  // 7
    std::function<void(const SuggestUniqueNickResponse&)> onSuggested;  // 8
    bool hasCallback() const;
};

// A profile's info as the cache keeps it (GPIInfoCache, 0xF0 bytes) and gp.info writes it.
struct InfoCache {
    std::string nick, uniquenick, email, firstname, lastname, homepage;
    int32_t icquin = 0;
    std::string zipcode, countrycode;
    float longitude = 0.0f;
    float latitude = 0.0f;
    std::string place;
    int32_t birthday = 0, birthmonth = 0, birthyear = 0;
    int32_t sex = 0;
    int32_t publicmask = 0;
    std::string aimname;
    int32_t pic = 0, occupationid = 0, industryid = 0, incomeid = 0, marriedid = 0, childcount = 0, interests1 = 0,
            ownership1 = 0, conntypeid = 0;
};

// A buddy's part of a profile (0x18 bytes): its place in the buddy list and its last status.
struct Buddy {
    int32_t index = 0;
    int32_t status = 0;
    std::string statusString;
    std::string locationString;
    uint32_t ip = 0;
    uint16_t port = 0;
};

// A profile the connection knows (GPIProfile, 0x1C bytes): its ids, its buddy part, its cached info, a buddy
// request's signature and count, the peer signature.
struct Profile {
    int32_t profileId = 0;
    int32_t userId = 0;
    std::optional<Buddy> buddy;
    std::optional<InfoCache> cache;
    std::optional<std::string> authSig;
    int32_t requestCount = 0;
    std::optional<std::string> peerSig;
};

// The SDK's GPConnection (0xBF4 bytes; the game's [0x0075B34C]).
class Connection : public std::enable_shared_from_this<Connection> {
public:
    // Where the connection manager is: the SDK's [0x0071B258] "gpcm.gamespy.com" (the fixed build's on
    // kServiceDomain) and 29900, the profile cache gp.info
    // of the current directory ([0x0071B298]) and the search manager ([0x0071B218] "gpsp.gamespy.com", 29901). The
    // game never changes them; only the tests give others (their stand-ins on the loopback and a file of their own).
    struct Server {
        std::string host = std::string("gpcm.") + kServiceDomain;
        uint16_t port = 29900;
        std::string infoFile = "gp.info";
        std::string searchHost = std::string("gpsp.") + kServiceDomain;
        uint16_t searchPort = 29901;
    };

    using ConnectCallback = std::function<void(const ConnectResponse&)>;
    using InfoCallback = std::function<void(const GetInfoResponse&)>;

    // gpInitialize (0x00578FE0): a connection of the product and the namespace (the game's 0, 0); the available
    // check's result must be "available" (else a parameter error); Winsock 1.1 started, the C runtime's rand() seeded
    // with GetTickCount(), gp.info read.
    static Result create(std::shared_ptr<Connection>& out, int32_t productId, int32_t namespaceId,
                         const Server& server);
    static Result create(std::shared_ptr<Connection>& out, int32_t productId, int32_t namespaceId) {
        return create(out, productId, namespaceId, Server{});
    }
    // gpDestroy (0x00579210): disconnected (the logout when logged in), gp.info written when the state is not 0.
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // gpSetCallback: error (+0x1C0), buddy request (+0x1C8), buddy status (+0x1D0), buddy message (+0x1D8), game
    // invite (+0x1E0), transfer (+0x1E8: the game sets none, so a peer's request of files is answered "not accepting").
    std::function<void(const ErrorArg&)> onError;
    std::function<void(const BuddyRequestArg&)> onBuddyRequest;
    std::function<void(const BuddyStatusArg&)> onBuddyStatus;
    std::function<void(const BuddyMessageArg&)> onBuddyMessage;
    std::function<void(const GameInviteArg&)> onGameInvite;
    std::function<void(const TransferArg&)> onTransfer;

    // gpConnectW (0x00568F50): the nick, email and password narrowed (each character's low byte); \login\ on the
    // server's challenge.
    Result connect(std::u16string_view nick, std::u16string_view email, std::u16string_view password, bool firewall,
                   bool blocking, ConnectCallback callback);
    // gpConnectNewUserW (0x00569160): \newuser\, then the login. The email whole in the fixed build (the stock
    // narrowed it into 32 bytes followed by the password's: an email of 32 characters or more was its first 32 then
    // the password).
    Result connectNewUser(std::u16string_view nick, std::u16string_view uniquenick, std::u16string_view email,
                          std::u16string_view password, std::u16string_view cdkey, bool firewall, bool blocking,
                          ConnectCallback callback);
    // gpProcess (0x00579850 with no operation blocking).
    Result process();
    // gpDisconnect: \logout\ when logged in, the socket closed (0x00574E00), the connection reset (0x005792C0).
    void disconnect();
    // gpGetInfo (0x005692C0): the profile's info from the cache (checkCache) or \getprofile\; blocking: processed here
    // until the answer (Stock problem: with Sleep(10) and no end while the server does not answer).
    Result getInfo(int32_t profile, bool checkCache, bool blocking, InfoCallback callback);
    // gpSetStatusW (0x00569900 -> 0x00569670): \status\, dropped when the status and both strings are the last sent
    // (the stock never kept the strings sent, so only the status with both strings empty was dropped).
    Result setStatus(int32_t status, std::u16string_view statusString, std::u16string_view locationString);
    // gpSetInvitableGames (0x00569970): \inviteto\.
    Result setInvitableGames(const std::vector<int32_t>& products);
    // gpSendBuddyRequestW (0x00569380): \addbuddy\ with the reason (UTF-8, '\' made '/').
    Result sendBuddyRequest(int32_t profile, std::u16string_view reason);
    // gpAuthBuddyRequest (0x005694E0): \authadd\ with the request's signature.
    Result authBuddyRequest(int32_t profile);
    // gpDenyBuddyRequest (0x00569520): the request forgotten, nothing sent.
    Result denyBuddyRequest(int32_t profile);
    // gpDeleteBuddy (0x00575C00): \delbuddy\, the buddy forgotten, the others' places closed up.
    Result deleteBuddy(int32_t profile);
    // gpInvitePlayerW (0x00569B50): \pinvite\ (the location narrowed).
    Result invitePlayer(int32_t profile, int32_t productId, std::u16string_view location);
    // gpGetBuddyStatus (0x005695B0): the buddy at that place of the list.
    Result getBuddyStatus(int32_t index, BuddyStatus& out);

    // gpiStartSearch (0x005718B0): the search counted, its operation (blocking or not) and its connection to the
    // search manager (0x00571590: gpsp.gamespy.com:29901, non-blocking; its buffers of 0x1000 bytes); blocking,
    // processed until its answer. Nothing of the executable calls it but a profile search's next page.
    Result startSearch(Search search, bool blocking);
    // gpiProfileSearch (0x00571910): a profile search (1): no criteria (nick, email, uniquenick, first or last name,
    // icquin) is the parameter error "No search criteria."; the strings cut to their fields, the email in lowercase, a
    // skip under 0 made 0.
    Result profileSearch(std::string_view nick, std::string_view uniquenick, std::string_view email,
                         std::string_view firstname, std::string_view lastname, int32_t icquin, int32_t skip,
                         bool blocking, std::function<void(ProfileSearchResponse&)> callback);
    // The operations a new profile's (1, \npr\) and a unique nick's (4, \rn\) answers go to: nothing of the
    // executable makes them (gpNewProfile, gpRegisterUniqueNick were not linked); for the tests.
    int32_t addAnswerOperation(int32_t type, std::function<void(const ProfileResponse&)> onProfile,
                               std::function<void(const ResultResponse&)> onResult);
    // The profile's updates (gpSetInfo*: not linked): "\key\value" pairs gathered for \updatepro\ and \updateui\.
    std::string& profileUpdates() { return m_updatePro; }
    std::string& userUpdates() { return m_updateUi; }
    int32_t searchCount() const { return m_searchCount; }
    // gpSendFiles was not linked: a transfer of ours to the profile (0x00576F80, the sender's side) with its files
    // (0x00577190: each name with '\' made '/' and the path read from), waiting for the profile's pong (the state 0),
    // the message going with the request; for the tests. Its id.
    int32_t addSendingTransfer(int32_t profile, std::string_view message,
                               const std::vector<std::pair<std::string, std::string>>& files);
    // The peers' listening port (+0x224, network order; 0 behind the firewall).
    uint16_t peerPort() const { return m_peerPort; }

    State state() const { return m_state; }
    int32_t profileId() const { return m_profileId; }
    int32_t userId() const { return m_userId; }
    int32_t sessionKey() const { return m_sessKey; }
    int32_t buddyCount() const { return m_buddyCount; }
    int32_t errorCode() const { return m_errorCode; }
    const std::u16string& errorString() const { return m_errorStringW; }
    // The profiles known (for the tests).
    const Profile* findProfile(int32_t profileId) const;

private:
    // A search's own part (GPISearchData's: its socket and buffers, its round's marks).
    struct SearchState {
        Search search;
        TcpConnection socket;  // +0x04
        std::string input;     // +0x08
        std::string output;    // +0x18
        int32_t processing = 0;  // +0x158: taken in this round
        int32_t done = 0;        // +0x15C: answered (or failed): freed at the round's end
    };
    struct Operation {
        int32_t type = 0;  // 0 connect, 1 a new profile, 2 get info, 3 a search, 4 a unique nick
        bool blocking = false;
        ConnectCallback connectCallback;
        InfoCallback infoCallback;
        std::function<void(const ProfileResponse&)> profileCallback;  // 1
        std::function<void(const ResultResponse&)> resultCallback;    // 4
        std::unique_ptr<SearchState> search;                          // 3: its data (+0x04)
        int32_t state = 0;  // +0x14: the connect's step (1 \lc\1, 2 \lc\2, 3 \nur\)
        int32_t id = 0;     // +0x18
        Result result = Result::NoError;  // +0x1C
        // The connect's data (0x308 bytes).
        std::string serverChallenge;   // +0x000
        std::string clientChallenge;   // +0x080
        std::string passwordHash;      // +0x0A1
        std::string authToken;         // +0x0C2
        std::string partnerChallenge;  // +0x1C2
        std::string cdkey;             // +0x2C2
        bool newUser = false;          // +0x304
    };
    struct Pending {  // a callback waiting (0x0057C090)
        int32_t type = 0;
        int32_t operationId = 0;
        std::function<void()> call;
    };

    // A message for a peer waiting for it to connect (0x18 bytes): "\m\<type>\len\<n>\msg\" and a line's end, the
    // text, a 0.
    struct PeerMessage {
        std::string bytes;  // +0x00
        int32_t type = 0;   // +0x10
        size_t start = 0;   // +0x14: where the text begins
        bool handed = false;  // given to the socket, gone once it took it all
    };
    // A peer (GPIPeer, 0x40 bytes; the list +0x450, the newest first): a TCP connection straight to a player.
    struct Peer {
        // 0x64 new, 0x65 its signature asked of the server, 0x66 to connect, 0x67 connecting, 0x68 authorizing (ours
        // sent, or theirs awaited), 0x69 connected, 0x6A closed.
        int32_t state = 0x64;     // +0x00
        bool initiated = false;   // +0x04: ours (we connected)
        TcpConnection socket;     // +0x08
        int32_t profile = 0;      // +0x0C (-1 until an incoming one tells its \pid\)
        int64_t timeout = 0;      // +0x10: time() + 300 at its last bytes
        int32_t nacks = 0;        // +0x14
        std::string input;        // +0x18
        std::string output;       // +0x28 (with the bytes the socket holds yet)
        std::vector<PeerMessage> messages;  // +0x38
    };
    // A transfer's id on both sides (+0x08): "<profile> <count> <time>".
    struct TransferKey {
        int32_t profile = 0;
        uint32_t count = 0;
        uint32_t time = 0;
        bool operator==(const TransferKey&) const = default;
    };
    // A file of a transfer (GPIFile, 0x84 bytes).
    struct TransferFile {
        std::string path;        // +0x00: the sender's file, the receiver's temporary one
        std::string name;        // +0x04 ('/' for '\'; +0x08 / +0x0C its wide name and path)
        int32_t progress = 0;    // +0x10
        int32_t size = -1;       // +0x14
        std::FILE* file = nullptr;  // +0x1C (not closed when the transfer goes)
        int32_t flags = 0;       // +0x20: 2 a directory, 4 to skip, 8 failed, 0x10 done
        uint32_t modTime = 0;    // +0x24
        Md5Context md5;          // +0x28
        int32_t reason = 0;      // +0x80: a failed one's kFile*Error
    };
    // A transfer (GPITransfer, 0x44 bytes; the list +0x480).
    struct Transfer {
        // 0 waiting for the profile's pong, 1 waiting (for the reply, for our answer), 2 transferring, 3 done,
        // 4 cancelled, 5 no connection.
        int32_t state = 0;              // +0x00
        std::vector<TransferFile> files;  // +0x04
        TransferKey key;                // +0x08
        int32_t id = 0;                 // +0x14 (+0x484, counted)
        bool sender = false;            // +0x18
        int32_t profile = 0;            // +0x1C
        Peer* peer = nullptr;           // +0x20 (left dangling when it goes)
        int32_t currentFile = -1;       // +0x24
        int32_t throttle = -1;          // +0x28: 0 paused
        std::string baseDirectory;      // +0x2C: the current directory of a received one
        uint32_t lastSend = 0;          // +0x30 (GetTickCount)
        std::string message;            // +0x34
        int32_t totalSize = 0;          // +0x38
        int32_t progress = 0;           // +0x3C
    };
    // A peer's message read (0x00573810): its type, its header (to the line's end) and its text.
    struct PeerRead {
        bool found = false;
        int32_t type = 0;
        std::string header;
        std::string text;
        size_t used = 0;
    };

    explicit Connection(const Server& server);

    // gpiConnect (0x00573C80).
    Result connectInternal(const std::string& nick, const std::string& uniquenick, const std::string& email,
                           const std::string& password, const std::string& authToken,
                           const std::string& partnerChallenge, const std::string& cdkey, bool firewall, bool newUser,
                           bool blocking, ConnectCallback callback);
    Result connectToServer(Operation& op);           // 0x00573930
    Result checkConnect();                          // 0x00574CD0
    Result processInternal(int32_t blockingId);     // 0x00579850
    Result processConnectionManager();              // 0x005794B0
    Result processConnect(Operation& op, const std::string& message);   // 0x00574690
    Result processGetInfo(Operation& op, const std::string& message);   // 0x0057A2C0
    Result processBuddyMessage(const std::string& message);            // 0x00575140
    void sendLogin(Operation& op);    // 0x00573F00
    void sendNewUser(Operation& op);  // 0x005742E0
    void sendBuddyMessage(int32_t profile, int32_t type, const std::string& text);  // 0x00575910
    Result processNewProfile(Operation& op, const std::string& message);    // 0x00576A10
    Result processUniqueNick(Operation& op, const std::string& message);    // 0x0057C3A0
    void sendInfoUpdates();                                                // 0x0057AAE0
    Result connectSearchManager(Operation& op);                            // 0x00571590
    Result processSearches();                                              // 0x00572FA0
    Result processSearch(Operation& op);                                   // 0x00571A80
    Result searchAnswered(Operation& op);                                  // 0x005720E6 ..
    // gpiReadKeyAndValue (0x00579C70): at the index a '\', the key to the next '\' and the value to the next '\' or
    // the end (511 characters each), the index past them. Else the parse error (1, fatal): the network error.
    Result readPair(const std::string& text, size_t& index, std::string& key, std::string& value);
    void searchError(std::string_view text);  // the read error of a search's answer
    // The peers (Core/Network/GameSpy/GameSpyPresencePeers.cpp).
    Result processPeers();                                   // 0x0057B980
    Result processInitiating(Peer& peer);                    // 0x0057AEF0
    Result processIncoming(Peer& peer);                      // 0x0057B160
    Result processConnected(Peer& peer);                     // 0x0057B410
    Result sendPeerMessages(Peer& peer);                     // 0x0057B370
    Result receivePeer(Peer& peer, size_t& read, bool& closed);  // 0x005735D0
    Result readPeerMessage(Peer& peer, PeerRead& out);       // 0x00573810
    Result peerSend(Peer& peer, std::string_view bytes);     // 0x00573430 / 0x005733C0
    void removePeer(Peer* peer);                             // 0x0057B780, 0x0057B720
    Peer& addPeer(int32_t profile, bool initiated);          // 0x0057BAB0
    Result askPeerSig(Peer& peer);                           // 0x0057BB50
    Result connectPeer(Peer& peer);                          // 0x0057BBA0
    Result queuePeerMessage(Peer& peer, int32_t type, std::string_view text);  // 0x0057BDE0
    Result routeBuddyMessage(int32_t profile, int32_t type, std::string_view text);  // 0x00575A00
    Result startTransferMessage(Peer& peer, int32_t type, const TransferKey& key);  // 0x0057BEF0
    Result finishTransferMessage(Peer& peer, std::string_view text);                // 0x0057BF60
    static size_t outputLength(const Peer& peer);
    // The transfers (Core/Network/GameSpy/GameSpyPresencePeers.cpp).
    Transfer& createTransfer(int32_t profile, bool sender);            // 0x00576F80
    Transfer& addReceivingTransfer(const TransferKey& key, int32_t profile);  // 0x00577040
    void removeTransfer(const Transfer* transfer);                     // 0x005770D0
    void transferEvent(const Transfer& t, int32_t type, int32_t index, int32_t num, std::u16string message = {});
    TransferFile& addTransferFile(Transfer& t, const std::string& name, const std::string* path);  // 0x00577190
    Result sendTransferRequest(Transfer& t);                           // 0x005772F0
    Transfer* findTransfer(const TransferKey& key);                    // 0x005774E0
    void sendFileReason(Transfer& t, int32_t file, int32_t reason);   // 0x00577540
    void failCurrentFile(Transfer& t, int32_t reason);                 // 0x005775E0
    bool processTransferRequest(Peer& peer, const TransferKey& key, const std::string& header,
                                const std::string& text);              // 0x00577600
    bool processTransferReply(Transfer& t, const std::string& header, const std::string& text);  // 0x00577910
    bool processFileBegin(Transfer& t, const std::string& header);     // 0x00577A50
    bool processFileEnd(Transfer& t, const std::string& header);       // 0x00577CD0
    bool processFileData(Transfer& t, const std::string& header, std::string_view text);  // 0x00577F50
    bool processFileSkip(Transfer& t, const std::string& header);      // 0x00578120
    bool processThrottle(Transfer& t, const std::string& header);      // 0x005782D0
    bool processCancel(Transfer& t);                                   // 0x005783F0
    Result sendFileEnd(Transfer& t, TransferFile& f);                  // 0x00578450
    Result sendFileBegin(Transfer& t, TransferFile& f);                // 0x00578530
    Result sendFileData(Transfer& t, std::string_view bytes);          // 0x00578640
    Result sendCurrentFile(Transfer& t);                               // 0x005786A0
    Result processTransfer(Transfer& t);                               // 0x00578A70
    Result processTransfers();                                         // 0x00578C10
    void transfersLost();                                              // 0x00578C60
    void transfersPong(int32_t profile, Peer* peer);                   // 0x00578D00
    void sendTransferReply(Peer& peer, const TransferKey& key, int32_t result, std::string_view message);  // 0x00578DC0
    void processTransferMessage(Peer& peer, int32_t type, const std::string& header, const std::string& text);  // 0x00578E60
    bool checkForError(const std::string& message, bool callback);  // 0x00579A10
    void disconnectInternal(bool logout);  // 0x00574E00
    void reset();                          // 0x005792C0
    void callError(Result result, bool fatal);  // 0x0057C020
    void addCallback(int32_t type, int32_t operationId, std::function<void()> call);  // 0x0057C090
    void processCallbacks(int32_t blockingId);  // 0x0057C280
    void failedCallback(const Operation& op);   // 0x0057FC50
    Operation& addOperation(int32_t type, bool blocking);  // 0x0057FDD0
    Operation* findOperation(int32_t id);                   // 0x0057FF60
    void removeOperation(const Operation* op);              // 0x0057FF10
    void setError(std::string_view text);                   // 0x00579F30
    void setError(std::string_view text, int32_t code);     // 0x00579EF0
    // The profiles' table (0x0057E200: 32 lists by the profile id modulo 32, looked through from the last of each).
    Profile* profile(int32_t profileId);
    Profile* addProfile(int32_t profileId);  // 0x00576BA0
    void removeProfile(int32_t profileId);
    template <typename F>
    Profile* mapProfiles(F&& keepGoing);  // 0x0057E510: the first profile the function stops at
    Profile* buddyAt(int32_t index);     // 0x00576E20
    void readInfoFile();   // 0x00576640
    void writeInfoFile();  // 0x00576980
    void flush();

    Server m_server;
    std::string m_errorString;      // +0x000
    std::u16string m_errorStringW;  // +0x4AA
    bool m_infoCaching = true;      // +0x100
    // +0x108: the simulation (gpEnable's, not linked; 0 from gpInitialize): the calls answered at once with nothing
    // (the connect's and the info's callbacks with zeros, a buddy's status zeros), nothing sent.
    bool m_simulation = false;
    bool m_firewall = false;        // +0x10C
    std::string m_nick;             // +0x110
    std::string m_uniquenick;       // +0x12F
    std::string m_email;            // +0x162
    std::string m_password;         // +0x195
    int32_t m_sessKey = 0;          // +0x1B4
    int32_t m_userId = 0;           // +0x1B8
    int32_t m_profileId = 0;        // +0x1BC
    TcpConnection m_socket;         // +0x1F0
    State m_state = State::None;    // +0x1F4
    std::string m_input;            // +0x1F8
    std::string m_output;           // +0x210
    uint16_t m_peerPort = 0;        // +0x224 (network order; 0 behind the firewall, as the game always is)
    int32_t m_nextOperationId = 2;  // +0x228
    int32_t m_lastStatus = -1;      // +0x230
    std::string m_lastStatusString;    // +0x234
    std::string m_lastLocationString;  // +0x334
    int32_t m_errorCode = 0;        // +0x434
    bool m_fatal = false;           // +0x438
    std::vector<std::unique_ptr<Operation>> m_operations;  // +0x440 (the newest first)
    std::array<std::vector<std::unique_ptr<Profile>>, 32> m_profiles;  // +0x444
    int32_t m_profileCount = 0;     // +0x448
    int32_t m_buddyCount = 0;       // +0x44C
    std::vector<Pending> m_callbacks;  // +0x454
    int32_t m_productId = 0;        // +0x488
    int32_t m_namespaceId = 0;      // +0x48C
    std::string m_loginTicket;      // +0x490
    int32_t m_searchCount = 0;      // +0x22C
    std::string m_updatePro;        // +0x45C .. +0x464
    std::string m_updateUi;         // +0x46C .. +0x474
    TcpListener m_peerListener;     // +0x220
    std::vector<std::unique_ptr<Peer>> m_peers;          // +0x450 (the newest first)
    std::vector<std::unique_ptr<Transfer>> m_transfers;  // +0x480
    int32_t m_nextTransferId = 0;   // +0x484
};

// The \gamename\ of the login and of \newuser\ is net::availableGameName() ([0x007A43A0], Core/Network/GameSpy/GameSpy.h).

}  // namespace oj::net::gp
