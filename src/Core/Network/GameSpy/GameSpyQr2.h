#pragma once
// GameSpy's query and reporting (QR2) of the SDK linked in the stock game (0x0056A180 .. 0x0056C290;
// docs/spec/28-network.md): a host listed on the master server by heartbeats carrying its keys, the master's challenge
// answered (which also tells the host its public address), the queries of the master and of the browsers answered,
// the clients' messages taken (a NAT negotiation's cookie). The game's record is the static one; it sends out of the
// transport's 8667 and never reads it: the packets beginning FE FD reach parseQuery through the transport's filter.

#include "Core/Network/GameSpy/GameSpy.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace oj::net {

// qr2_registered_key_list (0x0071ADC0): the keys' names by their numbers (1 hostname .. 29 score_t; 0 the empty
// name), the game's own from 50 on.
inline constexpr int kQr2MaxKeys = 254;  // MAX_REGISTERED_KEYS
// The name of a key, nullptr when none.
const char* qr2KeyName(int key);
// qr2_register_key (inlined by the matchmaking, 0x005626D0 ..): a name for a key of 50 .. 254 (others are left).
void qr2RegisterKey(int key, std::string_view name);

enum Qr2KeyType : int { kQr2ServerKey = 0, kQr2PlayerKey = 1, kQr2TeamKey = 2 };

// e_qrerror.
enum Qr2Error : int {
    kQr2NoError = 0,
    kQr2WinsockError = 1,
    kQr2BindError = 2,
    kQr2DnsError = 3,
    kQr2ConnectError = 4,
    kQr2NoChallengeError = 5,
};

// qr2_buffer_t: a packet of 0x800 bytes being built and its length (+0x800).
struct Qr2Buffer {
    std::array<uint8_t, 0x800> bytes{};
    int length = 0;
    // qr2_buffer_add (0x0056ACE0): the text and its 0, cut to the room left (the last byte made 0).
    void add(std::string_view text);
    // qr2_buffer_add_int (0x0056ACA0): "%d".
    void addInt(int value);
};

// qr2_keybuffer_t: the keys a list holds (254 at most) and their count (+0x100).
struct Qr2KeyBuffer {
    std::array<uint8_t, kQr2MaxKeys> keys{};
    int count = 0;
    // qr2_keybuffer_add (inlined at 0x00561570): dropped when full or the key is not 1 .. 254.
    bool add(int key);
};

// The record (qr2_implementation_t, 0x118 bytes).
class Qr2 {
public:
    // The callbacks of qr2_init (the userdata is in their captures).
    using ServerKeyCallback = std::function<void(int key, Qr2Buffer& out)>;                   // +0x88
    using PlayerKeyCallback = std::function<void(int key, int index, Qr2Buffer& out)>;        // +0x8C, +0x90
    using KeyListCallback = std::function<void(int type, Qr2KeyBuffer& keys)>;                // +0x94
    using CountCallback = std::function<int(int type)>;                                       // +0x98
    using AddErrorCallback = std::function<void(int error, const std::u16string& message)>;   // +0x9C
    // Set by the caller after the init (the game sets the NAT negotiation's and the public address's).
    using NatNegotiateCallback = std::function<void(int32_t cookie)>;                         // +0xA0
    using ClientMessageCallback = std::function<void(const std::u16string& message)>;         // +0xA4
    using PublicAddressCallback = std::function<void(uint32_t ip, uint16_t port)>;            // +0xA8
    using CdKeyCallback = std::function<void(const uint8_t* data, size_t size, const Address& from)>;  // +0xDC

    // qr2_init_socket (0x0056AA40 → 0x0056A740): the socket and its port (as given, the heartbeat's localport), the
    // game's name and secret key, listed (ispublic), NAT negotiation, the callbacks. srand(GetTickCount()) (the game's
    // rand() seeded again) and four rand() % 0xFF as the instance key; the listed state 1; the local addresses (once
    // a process: gethostbyname(gethostname()), five at most); listed, the master "<game>.master.gamespy.com":27900
    // resolved (kQr2DnsError when it does not).
    int initSocket(const GameSocket& socket, int port, std::string_view gameName, std::string_view secretKey,
                   bool isPublic, bool natNegotiate, ServerKeyCallback serverKey, PlayerKeyCallback playerKey,
                   PlayerKeyCallback teamKey, KeyListCallback keyList, CountCallback count, AddErrorCallback addError);

    // The matchmaking's frame (0x00563D22): checkSendHeartbeat when listed, then checkQueries.
    void think();
    // 0x0056ABD0: the socket none (-1), nothing. Waiting for the challenge (the listed state over 0) and 10 s past the
    // last heartbeat: a heartbeat with statechanged 3 (the state one more), or past three of them the state 0 and
    // the add error 5 "No challenge value was received from the master server." (and no keep-alive this time);
    // else a heartbeat (1) when a change is asked and 10 s passed, a heartbeat (0) each 60 s (or none yet, or the
    // clock gone back). A keep-alive each 20 s.
    void checkSendHeartbeat();
    // 0x0056AB40: reads the record's socket only when qr2 opened it (+0xC4); never in this build (the init on a given
    // socket clears it), the packets come through parseQuery.
    void checkQueries();
    // qr2_parse_query (0x0056B8F0): a packet come to the socket (`;` the cd key's, `\` a GameSpy 1 query, else
    // FE FD [type] [key 4] ..., at least 7 bytes), answered to `from`.
    void parseQuery(const uint8_t* data, size_t size, const Address& from);
    // qr2_send_statechanged (inlined at 0x00562AB9): listed, a heartbeat (1) now when 10 s passed since the last, else
    // asked for the think.
    void sendStateChanged();
    // qr2_shutdown (inlined at 0x00562B95): listed, a heartbeat (2) (the master drops the host); the socket closed when
    // qr2 opened it (never here), none (-1), the last heartbeat 0.
    void shutdown();

    // 0x0056BC10: 03 [key 4], localip<n> of each local address, localport, natneg, statechanged (not 0), gamename,
    // publicip / publicport (with a public address callback), then (2) one 0 byte or the three sections of all keys.
    void sendHeartbeat(int stateChanged);
    // 0x0056BBA0: 08 [key 4].
    void sendKeepAlive();

    bool valid() const { return m_socket.valid(); }
    bool isPublic() const { return m_isPublic; }
    int listedState() const { return m_listedState; }
    uint32_t publicIp() const { return m_publicIp; }       // network order
    uint16_t publicPort() const { return m_publicPort; }   // as the master's hex gave it
    const std::array<uint8_t, 4>& instanceKey() const { return m_instanceKey; }
    const Address& master() const { return m_master; }

    NatNegotiateCallback natNegotiateCallback;
    ClientMessageCallback clientMessageCallback;
    PublicAddressCallback publicAddressCallback;
    CdKeyCallback cdKeyCallback;

private:
    void send(const Address& to, const Qr2Buffer& b);
    // 0x0056B0E0: a section of keys (0 server, 1 players, 2 teams): the count (u16 BE) for players and teams; 0xFF
    // keys: the key list's names (and a server's values) then a 0 byte; then each index's values ("" when none).
    void buildKeySection(Qr2Buffer& b, int type, int numKeys, const uint8_t* keys);
    // 0x0056B3D0: [n][ids] for the server, the players and the teams (0xFF all, 0 none).
    void parseQueryRequest(const uint8_t* data, int size, Qr2Buffer& reply);
    // 0x0056B780 / 0x0056B480: the GameSpy 1 answer \key\value... \final\\queryid\1.1.
    void buildLegacyReply(Qr2Buffer& b);
    void legacySection(Qr2Buffer& b, int type);
    // 0x0056AFB0: base64(gs_encrypt(secret, challenge)) and its 0.
    void challengeResponse(const uint8_t* challenge, int size, Qr2Buffer& reply);
    // 0x0056B070: "%08X%04X" of the challenge's last 13 bytes.
    void parsePublicAddress(const char* text);
    // 0x0056B8B0: true when the key is among the last ten, else it goes in.
    bool hasClientMessageKey(uint32_t key);
    // 0x0056B7C0: a message beginning with NAT negotiation's magic (10 bytes at least): its cookie; else the client
    // message's callback.
    void processClientMessage(const uint8_t* message, int size);

    GameSocket m_socket;                     // +0x00
    std::string m_gameName;                  // +0x04
    std::string m_secretKey;                 // +0x44
    std::array<uint8_t, 4> m_instanceKey{};  // +0x84
    ServerKeyCallback m_serverKey;           // +0x88
    PlayerKeyCallback m_playerKey;           // +0x8C
    PlayerKeyCallback m_teamKey;             // +0x90
    KeyListCallback m_keyList;               // +0x94
    CountCallback m_count;                   // +0x98
    AddErrorCallback m_addError;             // +0x9C
    uint32_t m_lastHeartbeat = 0;            // +0xAC
    uint32_t m_lastKeepAlive = 0;            // +0xB0
    bool m_stateChangeRequested = false;     // +0xB4
    int m_listedState = 0;                   // +0xB8
    bool m_isPublic = false;                 // +0xBC
    int m_port = 0;                          // +0xC0
    bool m_readSocket = false;               // +0xC4
    bool m_natNegotiate = false;             // +0xC8
    Address m_master;                        // +0xCC
    std::array<uint32_t, 10> m_clientMessageKeys{};  // +0xE0
    int m_clientMessageIndex = 0;            // +0x108
    uint32_t m_publicIp = 0;                 // +0x10C
    uint16_t m_publicPort = 0;               // +0x110
};

// 0x0071ACA0 ([0x0071ADB8]): the record qr2_init fills when it is given none, the game's.
Qr2& qr2Record();

}  // namespace oj::net
