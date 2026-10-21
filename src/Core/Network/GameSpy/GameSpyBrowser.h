#pragma once
// GameSpy's server browsing of the SDK linked in the stock game (the wrappers 0x0056CDB0 .. 0x0056D9C0, the list
// 0x0057C480 .. 0x0057E070, the query engine 0x0057E5C0 .. 0x0057EBA0; docs/spec/28-network.md): the list of a game's
// hosts asked of the master list server over TCP ("<game>.ms<hash % 20>.gamespy.com":28910, the reply encrypted by
// GOA), the hosts' keys asked of the master again or of the hosts themselves over UDP (QR2 queries, timed as the
// ping), the messages the master pushes after the list, a message (the NAT negotiation's cookie) sent to a host
// through the master.

#include "Core/Network/GameSpy/GameSpy.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace oj::net {

// A server's state (+0x14).
enum SbServerState : uint8_t {
    kSbStateBasicKeys = 1,
    kSbStateFullKeys = 2,
    kSbStatePendingBasicQuery = 4,
    kSbStatePendingFullQuery = 8,
    kSbStateFailed = 0x10,
};

// A server's flags (+0x15), as the list gives them.
enum SbServerFlag : uint8_t {
    kSbUnsolicitedUdp = 1,
    kSbPrivateIp = 2,
    kSbConnectNegotiate = 4,
    kSbIcmpIp = 8,
    kSbNonStandardPort = 0x10,
    kSbNonStandardPrivatePort = 0x20,
    kSbHasKeys = 0x40,
    kSbHasFullRules = 0x80,
};

// SBServer (0x24 bytes, made by 0x0056A6D0).
struct SbServer {
    uint32_t publicIp = 0;     // +0x00, network order
    uint16_t publicPort = 0;   // +0x04, network order
    uint32_t privateIp = 0;    // +0x08
    uint16_t privatePort = 0;  // +0x0C
    uint32_t icmpIp = 0;       // +0x10
    uint8_t state = 0;         // +0x14
    uint8_t flags = 0;         // +0x15
    // +0x18: the keys and their values (names without case, 0x0056E200; one entered again replaces).
    std::vector<std::pair<std::string, std::string>> keys;
    uint32_t updateTime = 0;   // +0x1C: the query's start (GetTickCount), then its time (the ping, ms)
    SbServer* next = nullptr;  // +0x20: the query engine's queues

    const std::string* find(std::string_view key) const;
    // SBServerAddKeyValue (0x0056A020) / SBServerAddIntKeyValue (0x0056A060, "%d").
    void addKeyValue(std::string_view key, std::string_view value);
    void addIntKeyValue(std::string_view key, int value);
    // SBServerGetStringValue (0x0056A0C0): the value, else `fallback`.
    std::string stringValue(std::string_view key, std::string_view fallback) const;
    // SBServerGetIntValue (0x0056A180): "ping" the query's time; else the value when it begins with a digit (atol),
    // else `fallback`.
    int intValue(std::string_view key, int fallback) const;
    bool hasBasicKeys() const { return (state & kSbStateBasicKeys) != 0; }
    bool hasFullKeys() const { return (state & kSbStateFullKeys) != 0; }
};

class ServerBrowser {
public:
    // SBCallbackReason.
    enum Reason : int {
        kServerAdded = 0,
        kServerUpdated = 1,
        kServerUpdateFailed = 2,
        kServerDeleted = 3,
        kUpdateComplete = 4,
        kQueryError = 5,
    };
    // SBError.
    enum Error : int {
        kNoError = 0,
        kSocketError = 1,
        kDnsError = 2,
        kConnectError = 3,
        kDataError = 4,
        kAllocError = 5,
        kParamError = 6,
    };
    // The list's update options (the request's u32 BE).
    enum Option : int {
        kSendFieldsForAll = 1,
        kNoServerList = 2,
        kPushUpdates = 4,
        kAlternateSource = 8,
        kSendGroups = 0x20,
        kNoListCache = 0x40,
        kLimitResultCount = 0x80,
    };

    using Callback = std::function<void(ServerBrowser& sb, int reason, SbServer* server)>;  // +0x628

    // ServerBrowserNew (0x0056D040 → 0x0056CFB0): none when the available check's result is not 1. The list
    // disconnected, srand(GetTickCount()) (the game's rand() seeded again); the query engine's UDP socket.
    static std::unique_ptr<ServerBrowser> create(std::string_view queryForGame, std::string_view queryFromGame,
                                                 std::string_view secretKey, int queryFromVersion,
                                                 int maxConcurrentUpdates, int queryVersion, Callback callback);
    // ServerBrowserFree (0x0056D140).
    ~ServerBrowser();
    ServerBrowser(const ServerBrowser&) = delete;
    ServerBrowser& operator=(const ServerBrowser&) = delete;

    // ServerBrowserUpdate (0x0056D1B0): the fields "\<name>" of the keys (255 characters at most; the first 20 kept
    // as the basic keys of the direct queries), the list asked; not async: run until it is in.
    int update(bool async, bool disconnectOnComplete, const uint8_t* basicFields, int numBasicFields,
               const char* serverFilter, int updateOptions = 0, int maxServers = 0);
    // ServerBrowserAuxUpdateIP (0x0056D6D0 → 0x0056D4E0): a server's keys asked again, of the master
    // (SERVER_INFO_REQUEST) or of the server itself (added when not listed); not async: run until it answered.
    int auxUpdateIp(const char* ip, uint16_t port, bool viaMaster, bool async, bool fullUpdate);
    // ServerBrowserSendNatNegotiateCookieToServer (0x0056D450 → 0x0057E000): the cookie to a server through the master
    // (its QR2 gets it as a client message).
    int sendNatNegotiateCookieToServer(const char* ip, uint16_t port, int32_t cookie);
    // ServerBrowserSendMessageToServer (0x0057DF50): SEND_MESSAGE_REQUEST, reconnecting with NO_SERVER_LIST when
    // disconnected.
    int sendMessageToServer(uint32_t ip, uint16_t port, const uint8_t* data, int size);
    // ServerBrowserThink (0x0056D760): the queries (answers, 2.5 s timeouts, the queued started; none left: the
    // update complete), the dead servers freed, the list's socket read.
    int think();
    // ServerBrowserClear (0x0056D7D0): disconnected, the queries forgotten, the servers gone.
    void clear();
    // ServerBrowserHalt (inlined by the matchmaking at 0x00563969): disconnected, the queries forgotten.
    void halt();
    // SBServerListDisconnect (0x0057D000).
    void disconnect();

    int count() const { return static_cast<int>(m_servers.size()); }
    SbServer* getServer(int index) const;
    int state() const { return m_state; }           // +0x48: 0 LAN, 1 disconnected, 2 connected, 3 the list coming
    uint32_t myPublicIp() const { return m_myPublicIp; }  // +0x4E0: our address as the master saw it
    int querying() const { return m_queryList.count; }    // +0x10
    int pending() const { return m_pendingList.count; }   // +0x1C
    const std::string& listError() const { return m_error; }
    const std::array<uint8_t, 8>& challenge() const { return m_challenge; }

    // The list's MAPLOOP and PLAYERSEARCH callbacks (+0x484, +0x488): none in the game.
    std::function<void(SbServer* server, uint32_t mapIndex, const std::vector<std::string>& maps)> mapLoopCallback;
    std::function<void(const char* nick, uint32_t ip, uint16_t port, uint32_t extra, const char* data)>
        playerSearchCallback;

private:
    struct Queue {  // a query engine's queue of servers through their next
        SbServer* head = nullptr;
        SbServer* tail = nullptr;
        int count = 0;
    };
    struct ListKey {  // the list's keys (+0x08 of the list): a name and its type (0 string, 1 byte, 2 short)
        std::string name;
        int type = 0;
    };

    ServerBrowser() = default;

    // The list (the server list +0x48).
    int connectAndQuery(const char* fieldList, const char* serverFilter, int options, int maxServers);  // 0x0057CD30
    int connectToMaster();                                   // 0x0057CA20
    void makeChallenge();                                    // 0x0057CBB0
    int sendList(const uint8_t* data, int size);             // 0x0057CC40
    void freeKeyList();                                      // 0x0057CF80
    void freePopularValues();                                // 0x0057CF40
    void clearServers();                                     // 0x0057C5D0
    void freeDeadList();                                     // 0x0057C590
    int findServer(uint32_t ip, uint16_t port) const;        // 0x0057C4C0
    void addServer(std::unique_ptr<SbServer> server);        // 0x0057C480
    void removeServer(int index);                            // 0x0057C500
    void queryError();                                       // 0x0057C970
    int readList();                                          // 0x0057DE10
    int parseList();                                         // 0x0057D570
    int parseListServer(const uint8_t* data, int size);      // 0x0057D420
    int parseServerData(SbServer& server, const uint8_t* data, int size, bool usePopular);  // 0x0057D200
    bool checkKeys(const uint8_t* data, int size) const;     // 0x0057D130
    int processMessages();                                   // 0x0057DCC0
    int pushKeys(const uint8_t* data, int size);             // 0x0057D8F0
    int pushServer(const uint8_t* data, int size);           // 0x0057DBF0
    int mapLoop(const uint8_t* data, int size);              // 0x0057DB10
    int playerSearch(const uint8_t* data, int size);         // 0x0057D9E0
    int serverInfoRequest(uint32_t ip, uint16_t port);       // 0x0057DED0
    int readLan();                                           // 0x0057E070
    void listCallback(int reason, SbServer* server);         // 0x0056CDB0
    // The query engine (+0x00).
    void queryServer(SbServer* server);                      // 0x0057E5C0
    void updateServer(SbServer* server, bool addFront, int queryType);  // 0x0057E7B0
    void processQr2Reply(SbServer* server, const uint8_t* data, int size);  // 0x0057E830
    void processGs1Reply(SbServer* server, char* data);      // 0x0057E980
    void readQueries();                                      // 0x0057E9F0
    void timeoutQueries();                                   // 0x0057EAF0
    void startPending();                                     // 0x0057EB50
    void dequeue(SbServer* server);                          // 0x0057EBA0
    static bool removeFrom(Queue& q, SbServer* server);      // 0x0057E570
    void engineCallback(int reason, SbServer* server);       // 0x0056CF30
    void clearTrigger(SbServer* server);
    void runUntilListed();                                   // 0x0056D1B0's loop (not async)
    void queriesFrame();                                     // the query engine's part of the think

    // The query engine.
    int m_queryVersion = 1;                  // +0x00
    int m_maxUpdates = 0;                    // +0x04
    Queue m_queryList;                       // +0x08
    Queue m_pendingList;                     // +0x14
    UdpSocket m_querySocket;                 // +0x20
    uint32_t m_engineIp = 0;                 // +0x24: our public address for the queries
    std::array<uint8_t, 20> m_basicKeys{};   // +0x28
    int m_numBasicKeys = 0;                  // +0x3C
    // The server list.
    int m_state = 1;                                  // +0x48
    std::vector<std::unique_ptr<SbServer>> m_servers; // +0x4C
    bool m_hasKeyList = false;                        // +0x50 (the key list made)
    std::vector<ListKey> m_keyList;
    std::string m_queryForGame;                       // +0x54
    std::string m_queryFromGame;                      // +0x74
    std::string m_queryFromKey;                       // +0x94
    std::array<uint8_t, 8> m_challenge{};             // +0xB4
    std::vector<uint8_t> m_in;                        // +0xBC: 0x1000 bytes when the list is asked (none: empty)
    int m_inLength = 0;                               // +0xC0
    std::vector<std::string> m_popular;               // +0xC4, its count +0x4C0
    int m_expected = -1;                              // +0x4C4
    std::string m_error;                              // +0x4EC
    uint32_t m_myPublicIp = 0;                        // +0x4E0
    uint32_t m_alternateIp = 0;                       // +0x4E4
    uint16_t m_defaultPort = 0;                       // +0x4E8, network order
    TcpConnection m_socket;                           // +0x4F4
    UdpSocket m_lanSocket;                            // the LAN's (state 0; nothing opens it in this build)
    uint32_t m_lanStart = 0;                          // +0x4F8
    int m_queryFromVersion = 0;                       // +0x4FC
    GoaCrypt m_crypt;                                 // +0x500
    int m_options = 0;                                // +0x608
    int m_parseState = 0;                             // +0x60C
    std::vector<std::unique_ptr<SbServer>> m_dead;    // +0x614
    // The browser.
    bool m_disconnectOnComplete = false;              // +0x618
    bool m_dontUpdate = false;                        // +0x61C
    uint32_t m_triggerIp = 0;                         // +0x620
    uint16_t m_triggerPort = 0;                       // +0x624
    Callback m_callback;                              // +0x628
};

}  // namespace oj::net
