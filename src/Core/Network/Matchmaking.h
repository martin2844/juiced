#pragma once
// The game's matchmaking over GameSpy (CJETMatchmaking_gs: vtable 0x006FA880, 0x63C bytes, [0x0075EF84]; 0x00560CF0 ..
// 0x00564740; docs/spec/28-network.md): a hosted session listed by QR2 with the game's keys, the search through the
// server browser and its results, the join (directly, or after a NAT negotiation) by the D2 C8 2A messages that make
// the transports peers.

#include "Core/Network/GameSpy/GameSpyBrowser.h"
#include "Core/Network/GameSpy/GameSpyQr2.h"
#include "Core/Network/NetTransport.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oj::net {

struct NetGame;

// An attribute of a hosted session or of a result ({id, type, length, 1, value}, the allocator's records): type 1 an
// int, 2 a wide string of `length` characters, 3 `length` bytes.
struct MatchAttribute {
    int32_t id = 0;
    int32_t type = 0;
    int32_t length = 0;
    int32_t value = 0;            // type 1
    std::u16string text;          // type 2
    std::vector<uint8_t> bytes;   // type 3
};

// A search's attribute (16 bytes: the id, the type, the length, a fourth unused).
struct SearchAttribute {
    int32_t id = 0;
    int32_t type = 0;
    int32_t length = 0;
    int32_t unused = 0;
};

// A result of a search (0x30 bytes).
struct SearchResult {
    int32_t valid = 0;                        // +0x00: 0 < key50 < 6 and 0 < key51 < 6
    std::optional<Address> address;           // +0x04: the server's public address (an index of the address table)
    uint32_t sessionId = 0;                   // +0x08: key53
    SbServer* server = nullptr;               // +0x0C
    uint32_t players = 0;                     // +0x10: key50
    uint32_t openSlots = 0;                   // +0x14: key51
    uint32_t closed = 0;                      // +0x18: key52 (private)
    std::vector<MatchAttribute> attributes;   // +0x1C: one per search attribute
    float quality = 0.0f;                     // +0x2C: 1 - ping x 0.0033 within 0 .. 1
};

class Matchmaking {
public:
    // 0x00562360: the transport ([0x0075F758], its reference kept here by `keep`) and, with one, its filters: the
    // NAT negotiation's magic and C8 2A inside a 0xD2 packet.
    explicit Matchmaking(Transport* transport, std::shared_ptr<NetGame> keep = nullptr);
    // 0x00562500.
    ~Matchmaking();
    Matchmaking(const Matchmaking&) = delete;
    Matchmaking& operator=(const Matchmaking&) = delete;

    // +0x04 (0x00562640): hosting: stopped first; the session id (srand(++count + this) twice, rand() + rand() << 16),
    // the flags and counts, the keys key50 .. key53 and key<54 + i> of the i-th attribute registered, the attributes
    // copied; QR2 on the transport's socket (false and stopped when it fails); the FE FD filter, the NAT negotiation's
    // and the public address's callbacks.
    bool host(uint32_t privateFlag, uint32_t numPlayers, uint32_t openSlots, uint32_t attributeCount,
              const MatchAttribute* const* attributes);
    // +0x08 (0x00562A40): the counts, the values of the attributes of the same ids; then qr2_send_statechanged.
    bool update(uint32_t numPlayers, uint32_t openSlots, uint32_t attributeCount,
                const MatchAttribute* const* attributes, int32_t unused);
    // +0x0C (0x00562B10): the session's name, playing, the mode (the keys 1/2, 11 and 6).
    void setInfo(std::u16string_view name, int32_t playing, std::u16string_view mode);
    // +0x10 (0x00562B80).
    void stopHosting();
    // +0x14 (0x00562C90): key50 .. key53 and key<54 + id> registered, the attributes kept, the most results.
    void setSearchAttributes(uint32_t maxResults, uint32_t count, const SearchAttribute* attributes);
    // +0x18 (0x00562F60): the arguments unused.
    bool startSearch(int32_t a, int32_t b, int32_t c);
    // +0x1C (0x00563200): a friend's address (none: the friends' object absent or the index -1) asked through the
    // master.
    bool searchFriend(int32_t a, int32_t friendIndex);
    // +0x20 (0x00563530): the result at that sorted position asked again (the friends' object needed).
    bool refreshResult(int32_t a, uint32_t sortedIndex);
    // +0x24 (0x00563820): an address (the stock's index of the address table; none: -1) asked through the master (the
    // friends' object needed).
    bool searchAddress(int32_t a, const std::optional<Address>& address);
    // +0x28 (0x00563950).
    void stopSearch();
    // +0x2C (0x00563990): the result at `index` of the results themselves (Stock problem: not a sorted position as
    // everything else takes) joined with our `slots`.
    void join(uint32_t index, uint32_t slots);
    // +0x30 (0x00563D00) each frame.
    void update(float dt);
    // +0x34 (0x00563F40): hosting stopped, the presence cleared.
    void leave();

    // 0x005645E0: the sorted positions made again (CRT qsort, 0x00564550: the better quality first).
    void sortResults();
    // 0x00564500: the search's filter (narrow, under 0x200 characters).
    void setFilter(std::string_view filter);
    // The record at a sorted position (the windows' readers), null past them.
    const SearchResult* sortedResult(uint32_t position) const;

    // The fields the windows read.
    int32_t publicAddressKnown = 0;                // +0x08
    int32_t listIn = 0;                            // +0x0C: the search over (SEARCHING ...)
    int32_t detailsIn = 0;                         // +0x10: set and cleared with +0x0C
    int32_t stoppedHosting = 0;                    // +0x18
    uint32_t maxResults = 0;                       // +0x24
    std::vector<SearchResult> results;             // +0x28
    std::vector<uint32_t> sorted;                  // +0x38: the results' indices by quality
    int32_t selected = -1;                         // +0x48: never replaced by a better result
    std::optional<Address> presenceAddress;        // +0x50
    std::u16string filter;                         // +0x54

    // The rest, for the tests and the game's checks.
    bool hostingUp() const { return m_qr2Up; }                // +0x458
    ServerBrowser* browser() const { return m_browser.get(); }  // +0x45C
    Transport* transport() const { return m_transport; }      // +0x454
    bool negotiating() const { return m_negotiating; }        // +0x48C
    uint32_t sessionId() const { return m_sessionId; }        // +0x5C0
    const std::optional<Address>& hostAddress() const { return m_hostAddress; }  // +0x5AC
    size_t clients() const { return m_clients.size(); }      // +0x5B0

private:
    struct Request {  // a NAT negotiation asked for (16 bytes)
        int32_t cookie = 0;
        int32_t peerId = 0;
        uint32_t slots = 0;
        uint32_t index = 0;
    };
    struct Client {  // a client of the host (0x14 bytes)
        int32_t id = 0;
        uint32_t slots = 0;
        std::optional<Address> address;
        std::optional<Address> publicAddress;
        std::optional<Address> privateAddress;
    };

    GameSocket gameSocket() const;
    void clearNegotiations();                                              // 0x00560EC0
    const MatchAttribute* hostAttribute(int32_t id) const;                 // 0x00560E70
    int32_t makeCookie();                                                  // 0x00561020
    bool pushRequest(const Request& r);                                    // 0x00561070
    bool popRequest(Request& r);                                           // 0x005610C0
    void pruneClients();                                                   // 0x00561120
    Address publicSockaddr() const;                                        // 0x005612F0
    Address privateSockaddr() const;                                       // 0x00561340
    void serverKey(int key, Qr2Buffer& out);                               // 0x005613C0
    void keyList(int type, Qr2KeyBuffer& keys);                            // 0x00561570
    void searchCallback(int reason, SbServer* server);                     // 0x00561750
    void negotiated(int result, const Address* remote);                    // 0x00561B00
    void joinAsked(const uint8_t* packet, size_t size, const Address& from);  // 0x00561D10
    void message(const uint8_t* packet, size_t size, const Address& from);    // 0x00562130
    void publicAddress(uint32_t ip, uint16_t port);                        // 0x00562230
    void clearResults();                                                   // 0x00564370
    bool insertResult(SearchResult& record);                               // 0x005640A0
    void clearPresence();                                                  // 0x00564740
    void publishPresence();  // [0x0075F1E4]->vt+0x44 of the first local player (GP)
    void ensureBrowser();
    void sendJoin(uint32_t direct, uint32_t slots);
    void sendTo(const std::optional<Address>& to, const uint8_t* data, size_t size);
    void searchThrough(const std::optional<Address>& address);

    Transport* m_transport = nullptr;          // +0x454
    std::shared_ptr<NetGame> m_keep;
    bool m_qr2Up = false;                      // +0x458
    std::unique_ptr<ServerBrowser> m_browser;  // +0x45C
    std::vector<SearchAttribute> m_searchAttributes;  // +0x460
    std::vector<MatchAttribute> m_hostAttributes;     // +0x470
    bool m_negotiating = false;                // +0x48C
    float m_negotiationTime = 0.0f;            // +0x490
    std::array<Request, 16> m_ring{};          // +0x494
    uint32_t m_ringRead = 0;                   // +0x594
    uint32_t m_ringWrite = 0;                  // +0x598
    Request m_request;                         // +0x59C
    std::optional<Address> m_hostAddress;      // +0x5AC
    std::vector<Client> m_clients;             // +0x5B0
    uint32_t m_sessionId = 0;                  // +0x5C0
    uint32_t m_private = 0;                    // +0x5C4
    uint32_t m_players = 0;                    // +0x5C8
    uint32_t m_openSlots = 0;                  // +0x5CC
    float m_searchTime = 0.0f;                 // +0x5D0
    float m_requeryTime = 0.0f;                // +0x5D4
    std::u16string m_sessionName;              // +0x5D8
    int32_t m_playing = 0;                     // +0x618
    std::u16string m_mode;                     // +0x61C
};

// [0x0075EF84]: the game's matchmaking, none outside the online windows.
Matchmaking* matchmaking();
// 0x004E1A00: the matchmaking made anew (0x00560D50; the one before released) on the network game's transport.
void makeMatchmaking();
// 0x004E1A20: released.
void releaseMatchmaking();

// The windows' readers (of [0x0075EF84]): 0x004E6300 the valid results; 0x004E6360 the quick match's position (the
// first valid one not playing (its attribute 1 0), else the first valid one, else -1); 0x004E6650 the quality marks of
// a position (1 + 5 x quality truncated, 1 .. 5; 0 for none or a quality 0); 0x00564060 a result's attribute.
int32_t countValidResults();
int32_t quickMatchResult();
int32_t qualityMarks(uint32_t position);
const MatchAttribute* findAttribute(const SearchResult& result, int32_t id);

}  // namespace oj::net
