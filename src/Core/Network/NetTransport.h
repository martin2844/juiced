#pragma once
// The game's network transport ("jetNet", vtable 0x006FD728, the object [0x0075F758], the game's [0x0075EF8C];
// docs/spec/28-network.md): UDP, a socket on 8666 for the LAN's search (broadcast, plain) and one on 8667 for the
// game (every peer to every peer, each packet XOR-coded by a stream of keys). Reliable packets (0xD5) are answered
// (0xD4), sent again every 2 s up to 30 times and taken once; the peers keep alive (0xD8 every 1.5 s, 25 s of silence
// drops one); the host takes joins (0xDB) and tells everyone of everyone (0xDF). The game's own messages (ids under
// 0xD2) and the transport's events go to the handler.

#include "Core/Base/Random.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace oj::net {

// An IPv4 address and port, both in network order (a sockaddr_in's).
struct Address {
    uint32_t ip = 0;
    uint16_t port = 0;
    bool operator==(const Address& o) const { return ip == o.ip && port == o.port; }
};

// A session as the LAN's search sees it (0x48 bytes on the wire): its name, the slots taken and the most.
struct SessionInfo {
    std::array<char16_t, 32> name{};
    uint32_t used = 0;
    uint32_t max = 0;
};

// A session found by the search (+0x64, 0x50 bytes): the host's address, its info and the time since it answered
// (gone past 4 s).
struct FoundSession {
    Address address;
    SessionInfo info;
    float age = 0.0f;
};

// A peer (+0x74, 0x24 bytes): its id (the host's 0), address, slots (its players), the time since its last
// keep-alive and the ring of its last four ping times (0.05 at first).
struct Peer {
    int32_t id = 0;
    Address address;
    uint32_t slots = 0;
    float silence = 0.0f;
    std::array<float, 4> pings{0.05f, 0.05f, 0.05f, 0.05f};
    uint32_t ring = 0;
};

// The transport's state (+0xF4).
enum class State : int32_t {
    Normal = 0,
    JoinTimedOut = 1,  // no answer to a join in 15 s
    Refused = 2,       // the host answered 0xDA: no room
    HostLeft = 3,      // the host said it leaves (0xD9)
    HostTimedOut = 4,  // nothing from the host for 25 s
};

class Transport {
public:
    // The game's handler (+0xC4): a message of a peer (its id), or an event of the transport with peer 0: 0xDF a
    // peer came ([4] its id, [8] its slots), 0xDE a peer went ([4] its id, [8] 1 timed out, 0 it left).
    using Handler = std::function<void(const uint8_t* data, size_t size, int32_t peer)>;
    // A filter (+0xD4): the packets of 8667 beginning with its bytes (raw, before the code is taken off), or inside
    // a 0xD2 packet from its second byte, go to it as they are.
    using Filter = std::function<void(const uint8_t* data, size_t size, const Address& from)>;

    struct Options {
        uint16_t lanPort = 8666;
        uint16_t gamePort = 8667;
        // The address the host gives as its own (0: the first of gethostbyname(gethostname)), and where the search's
        // broadcasts go (0: 255.255.255.255) — other values for tests on one machine.
        uint32_t ownIp = 0;
        uint32_t broadcastIp = 0;
        uint16_t broadcastPort = 0;  // 0: the LAN's port
        // The LAN's port shared with other sockets (SO_REUSEADDR; tests).
        bool shareLanPort = false;
        size_t bufferSize = 0x800;  // the receiving buffer (0x004E9C40)
    };

    Transport() = default;
    ~Transport();
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    // 0x0059A3A0: the two sockets (non-blocking, 8666 with broadcast), the buffer; +0xF0 four random bytes
    // (0x00598710 with `random`). False when Winsock or a socket cannot be had.
    bool open(const Options& options, CrtRandom& random, std::string* why);
    void close();
    bool opened() const { return m_lan != ~uintptr_t{0}; }

    void setHandler(Handler h) { m_handler = std::move(h); }
    // +0xCC (its context +0xD0): a second handler the game sets (0x004E9980, the voice's relay of the Xbox) and the
    // transport never calls.
    void setRelayHandler(Handler h) { m_relayHandler = std::move(h); }
    // 0x005997A0 / 0x00599860 (a filter of the same bytes replaced or taken out).
    void addFilter(const std::vector<uint8_t>& prefix, Filter f);
    void removeFilter(const std::vector<uint8_t>& prefix);

    // +0x1C (0x0059A590 → 0x00598A20): this machine hosts: its address (its own), its players (`slots`), the most
    // (`max`) and the session's name (32 characters at most); it answers the LAN's search.
    // `address` its fourth argument: an address entry of the transport's table taken as the host's (referenced), or
    // -1 (none here) for its own; every caller of the game passes -1.
    void host(std::u16string_view name, uint32_t slots, uint32_t max,
              const std::optional<Address>& address = std::nullopt);
    // +0x20 (0x00598AC0): the found sessions emptied, a new search id (8 random bytes) and its first 0xDD.
    void search(CrtRandom& random);
    // +0x24 (0x00598BC0): a join (0xDB with the slots, reliable) to the host; connecting until its 0xDF comes.
    void join(const Address& host, uint32_t slots);
    // +0x28 (0x00598C20): in a session 0xD9 to every peer; the peers, the reliable queue and the taken ones dropped.
    void leave();
    // +0x38 (0x00599920) each frame.
    void update(float dt);

    // 0x00599120: to a peer by its id, or every peer (-2); reliable ones in 0xD5. False when a peer is not found.
    bool send(int32_t peer, const void* data, size_t size, bool reliable);
    // A packet as it is to an address (the filters' users: QR2, NatNeg; vt+0x30, 0x0059A1C0).
    bool sendRaw(const Address& to, const void* data, size_t size);
    // 0x00599240: the voice's relay to a peer by its id, or every peer (-2; false when one fails): the bytes coded by
    // a stream of their own (0x005991E0: from 1234, k = k x 0xB275D + 0x269EC3 and each byte ^= k's low byte), sent
    // from their third byte as they are (vt+0x34, 0x00597AF0), then decoded again. False for a peer not found.
    bool sendRelay(int32_t peer, uint8_t* data, size_t size);
    // 0x00598FC0: a packet coded to an address that need not be a peer; reliable ones in 0xD5, queued for their
    // answer (the matchmaking's join messages).
    void sendPacket(const Address& to, const uint8_t* data, size_t size, bool reliable);
    // 0x00598E60: a peer added (one of the same address taken out first); hosting, false when the session has no
    // room for its slots; the host (id 0) ends the connecting. The game hears 0xDF.
    bool addPeer(int32_t id, const Address& address, uint32_t slots);
    // The count +0xF0 taken and moved on (the host's id for a peer that joins).
    int32_t takeId() { return static_cast<int32_t>(m_counter++); }
    // The online join (0x00563990): the slots, not hosting, not searching, the join's timer from 0 and connecting —
    // no 0xDB: the matchmaking sends its own, and the 15 s of the join's timer cover it all.
    void joinOnline(uint32_t slots);
    // 0x00562110: the host refused the online join: not connecting, the state Refused.
    void refused();

    // The XOR stream of 8667 (from 0xD2, k = k x 0x5D - 0x3D for each byte), both ways.
    static void code(uint8_t* data, size_t size);
    // rand() seeded again with the shared count ([0x0079E898], one more) and the transport's address, as the random
    // bytes (0x00598710) and the grid's shuffle (0x004E9B50) do.
    void reseed(CrtRandom& random);

    bool hosting() const { return m_hosting; }               // +0x0C
    bool answering() const { return m_answering; }           // +0x10
    void setAnswering(bool on) { m_answering = on; }
    bool searching() const { return m_searching; }           // +0x14
    bool connecting() const { return m_connecting; }         // +0x18
    bool inSession() const { return m_hostKnown; }           // +0xE4 not -1
    const Address& hostAddress() const { return m_hostAddress; }
    State state() const { return m_state; }                  // +0xF4
    void setPinging(bool on) { m_pinging = on; }             // +0x90
    uint32_t slots() const { return m_slots; }               // +0x08
    const SessionInfo& session() const { return m_session; } // +0x1C
    const std::vector<FoundSession>& sessions() const { return m_found; }
    const std::vector<Peer>& peers() const { return m_peers; }
    const Peer* peer(int32_t id) const;
    uint16_t gamePort() const { return m_options.gamePort; }

private:
    friend class LoadKeepAlive;

    struct Queued {  // a reliable packet waiting for its answer (+0xA4)
        uint32_t sequence = 0;
        float timer = 0.0f;
        uint32_t tries = 0;
        Address to;
        std::vector<uint8_t> bytes;  // coded
    };
    struct Taken {  // a reliable packet taken (+0xB4): its number and sender, 10 s
        uint32_t sequence = 0;
        Address from;
        float age = 0.0f;
    };
    struct FilterEntry {
        std::vector<uint8_t> prefix;
        Filter callback;
    };

    void randomBytes(uint8_t* out, size_t n, CrtRandom& random);  // 0x00598710
    bool sendTo(const Address& to, const uint8_t* data, size_t size);  // 0x0059A1C0
    bool broadcast(const uint8_t* data, size_t size);  // 0x0059A100
    void receiveLan();   // 0x0059A270
    void receiveGame();  // 0x0059A2C0 → 0x005985B0
    void dispatch(const uint8_t* data, size_t size, const Address& from);  // 0x00598460
    bool filters(const uint8_t* data, size_t size, const Address& from, size_t skip);  // 0x00597C40 / 0x00597CF0
    void answerSearch(const uint8_t* request, size_t size);  // 0x00597DA0
    void sessionFound(const uint8_t* reply, size_t size);    // 0x00597E10
    void reliable(const uint8_t* data, size_t size, const Address& from);  // 0x00598200
    void acknowledged(const uint8_t* data, size_t size);  // 0x005983E0
    void joinAsked(const uint8_t* data, size_t size, const Address& from);  // 0x00597F80
    void removePeer(size_t index, bool timedOut);  // 0x00597B90
    Peer* peerAt(const Address& address);  // 0x00598670
    void tellPeer(const Address& to, int32_t id, uint32_t slots, const Address& about);  // 0xDF (0x0059A010)
    void searchFrame(float dt);  // 0x00599300
    void peersFrame(float dt);   // 0x00599470
    void pingFrame(float dt);    // 0x00599560
    void queueFrame(float dt);   // 0x005995C0
    void takenFrame(float dt);   // 0x005996C0
    void event(uint8_t kind, int32_t id, int32_t value);

    Options m_options;
    uintptr_t m_lan = ~uintptr_t{0};   // +0xFC
    uintptr_t m_game = ~uintptr_t{0};  // +0x100
    bool m_winsock = false;
    std::vector<uint8_t> m_buffer;     // +0x104, +0x108
    Handler m_handler;
    Handler m_relayHandler;
    std::vector<FilterEntry> m_filters;
    uint32_t m_slots = 0;
    bool m_hosting = false;
    bool m_answering = false;
    bool m_searching = false;
    bool m_connecting = false;
    SessionInfo m_session;
    std::vector<FoundSession> m_found;
    std::vector<Peer> m_peers;
    float m_searchTimer = 0.0f;   // +0x84
    float m_joinTimer = 0.0f;     // +0x88
    float m_keepAlive = 0.0f;     // +0x8C
    bool m_pinging = false;       // +0x90
    float m_pingTimer = 0.0f;     // +0x94
    uint32_t m_pingNumber = 0;    // +0x98
    std::vector<Queued> m_queue;
    std::vector<Taken> m_taken;
    bool m_hostKnown = false;     // +0xE4 not -1
    Address m_hostAddress;
    std::array<uint8_t, 8> m_searchId{};  // +0xE8
    uint32_t m_counter = 0;       // +0xF0
    State m_state = State::Normal;
    float m_errorTimer = -1.0f;   // +0xF8
};

// Not of the game: the race loads on the main thread here, the stock's on its loading thread (0x00463BB0 queued on
// 0x0059A6F0) while the main loop's frames went on, the transport's among them: its keep-alive every 1.5 s, so a
// long load never reached the peers' 25 s of silence (0x00599470) that drop a machine. While one of these lives (the
// load's blocking part), a thread of its own says the keep-alive to the peers as the frame would; nothing else is
// sent or read meanwhile (what comes waits in the socket, the reliable packets are sent again).
class LoadKeepAlive {
public:
    explicit LoadKeepAlive(const Transport* transport);
    ~LoadKeepAlive();
    LoadKeepAlive(const LoadKeepAlive&) = delete;
    LoadKeepAlive& operator=(const LoadKeepAlive&) = delete;
    // The keep-alives said so far (for the tests).
    uint32_t sent() const;

private:
    struct Worker;
    std::unique_ptr<Worker> m_worker;
};

// 0x00597690: Winsock started again (2.2; the one before let go), [0x0075F754] set and the network's loss
// ([0x0075F74C]) cleared; false when it cannot start. (The transport also starts its own.)
bool startWinsock();
// 0x005D9EE4 where the windows let the online services go: WSACleanup when Winsock was started here, [0x0075F754] 0.
void stopWinsock();
// [0x0075F754]: Winsock started by startWinsock (the main menu's multi, the link windows, the online log on).
bool winsockStarted();
// [0x0075F74C]: the network lost: a transport's sends failing for 3 s set it (its frame, 0x00599A21); only Winsock's
// start clears it (0x005976F2), so it outlives the transport.
bool& networkLost();
// 0x00597730: the network down: [0x0075F74C], or the link object [0x0075F750] in its state 4 (0x0059A690; nothing
// sets [0x0075F750] on the PC, so never that).
bool networkDown();

}  // namespace oj::net
