#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Core/Network/GameSpy/GameSpyNatNeg.h"

#include <winsock2.h>
#include <windows.h>

#include <array>
#include <cstring>
#include <vector>

namespace oj::net {

namespace {

constexpr uint8_t kMagic[6] = {0xFD, 0xFC, 0x1E, 0x66, 0x6A, 0xB2};  // 0x0071B1B8
constexpr uint8_t kVersion = 2;
constexpr uint8_t kInit = 0;
constexpr uint8_t kInitAck = 1;
constexpr uint8_t kErtTest = 2;
constexpr uint8_t kErtAck = 3;
constexpr uint8_t kConnect = 5;
constexpr uint8_t kConnectAck = 6;
constexpr uint8_t kConnectPing = 7;

// A negotiator (0x40 bytes).
struct Negotiator {
    std::unique_ptr<UdpSocket> socket;         // +0x00 (null: -1)
    GameSocket gameSocket;                     // +0x04 (none: -1)
    int32_t cookie = 0;                        // +0x08
    int clientIndex = 0;                       // +0x0C
    int state = kNnInitSent;                   // +0x10
    std::array<bool, 3> initAcked{};           // +0x14 .. +0x1C
    int retries = 0;                           // +0x20
    int maxRetries = 0;                        // +0x24
    uint32_t retryTime = 0;                    // +0x28
    uint32_t guessedIp = 0;                    // +0x2C (network order)
    uint16_t guessedPort = 0;                  // +0x30 (host order)
    bool gotRemoteData = false;                // +0x32
    NatNegProgressCallback progress;           // +0x34
    NatNegCompletedCallback complete;          // +0x38
};

// [0x0075F4C8]: the negotiators (null until the first).
std::unique_ptr<std::vector<std::unique_ptr<Negotiator>>>& negotiators() {
    static std::unique_ptr<std::vector<std::unique_ptr<Negotiator>>> list;
    return list;
}

// [0x0075F4CC], [0x0075F4D0]: the two servers' addresses, resolved once.
uint32_t g_natneg1 = 0;
uint32_t g_natneg2 = 0;

void put32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v >> 24);
    p[1] = static_cast<uint8_t>(v >> 16);
    p[2] = static_cast<uint8_t>(v >> 8);
    p[3] = static_cast<uint8_t>(v);
}

uint32_t get32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

void header(uint8_t* p, uint8_t type, int32_t cookie) {
    std::memcpy(p, kMagic, 6);
    p[6] = kVersion;
    p[7] = type;
    put32(p + 8, static_cast<uint32_t>(cookie));
}

Negotiator* find(int32_t cookie) {  // 0x0056C290
    if (!negotiators()) return nullptr;
    for (auto& n : *negotiators()) {
        if (n->cookie == cookie) return n.get();
    }
    return nullptr;
}

void removeNegotiator(Negotiator* n) {  // 0x0056C360
    auto& list = *negotiators();
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i].get() == n) {
            list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

// 0x0056C720.
bool resolveServers() {
    const ServiceHosts& hosts = serviceHosts();
    auto resolve = [](uint32_t& cached, const std::string& name) {
        if (cached != 0) return;
        uint32_t ip = inet_addr(name.c_str());
        if (ip == INADDR_NONE) {
            const hostent* h = gethostbyname(name.c_str());
            ip = 0;
            if (h != nullptr) std::memcpy(&ip, h->h_addr_list[0], 4);
        }
        cached = ip;
    };
    resolve(g_natneg1, hosts.natneg1);
    resolve(g_natneg2, hosts.natneg2);
    return g_natneg1 != 0 && g_natneg2 != 0;
}

// 0x0056C430: the first private address of gethostbyname(gethostname()) (10/8, 172.16/12, 192.168/16), else the last
// one not the loopback (network order); 0 when none.
uint32_t localIp() {
    char name[256] = {};
    gethostname(name, sizeof name);
    const hostent* h = gethostbyname(name);
    if (h == nullptr) return 0;
    uint32_t found = 0;
    for (int i = 0; h->h_addr_list[i] != nullptr; ++i) {
        uint32_t ip = 0;
        std::memcpy(&ip, h->h_addr_list[i], 4);
        if (ip == htonl(0x7F000001)) continue;
        const uint32_t host = ntohl(ip);
        const uint32_t a = host >> 24;
        const uint32_t b = (host >> 16) & 0xFF;
        if (a == 10) return ip;
        found = ip;
        if (a == 172) {
            if (b > 15 && b < 32) return ip;
        } else if (a == 192 && b == 168) {
            return ip;
        }
    }
    return found;
}

void sendTo(UdpSocket* s, uint32_t ip, uint16_t hostPort, const uint8_t* data, size_t size) {  // 0x0056C3D0
    if (s != nullptr) s->sendTo(ip, htons(hostPort), data, size);
}

// 0x0056C4C0: the INITs not acknowledged yet: porttype 0 from the game's socket to natneg1, 1 from our own to natneg1
// (both with the local port 0), then the local port (the game socket's when it is used, else our own's) and 2 from our
// own to natneg2. 500 ms, 30 times.
void sendInitPackets(Negotiator& n) {
    const std::string& gameName = availableGameName();
    std::vector<uint8_t> p(21 + gameName.size() + 1, 0);
    header(p.data(), kInit, n.cookie);
    p[13] = static_cast<uint8_t>(n.clientIndex);
    const bool useGamePort = n.gameSocket.valid();
    p[14] = useGamePort ? 1 : 0;
    put32(p.data() + 15, ntohl(localIp()));
    p[19] = 0;
    p[20] = 0;
    std::memcpy(p.data() + 21, gameName.c_str(), gameName.size() + 1);
    const uint16_t port = serviceHosts().natnegPort;
    if (useGamePort && !n.initAcked[0]) {
        p[12] = 0;
        Address to{g_natneg1, htons(port)};
        n.gameSocket.send(to, p.data(), p.size());
    }
    if (!n.initAcked[1]) {
        p[12] = 1;
        sendTo(n.socket.get(), g_natneg1, port, p.data(), p.size());
    }
    const uint16_t local = ntohs(useGamePort ? n.gameSocket.port : (n.socket ? n.socket->localPort() : uint16_t{0}));
    p[19] = static_cast<uint8_t>(local >> 8);
    p[20] = static_cast<uint8_t>(local);
    if (!n.initAcked[2]) {
        p[12] = 2;
        sendTo(n.socket.get(), g_natneg2, port, p.data(), p.size());
    }
    n.retryTime = GetTickCount() + 500;
    n.maxRetries = 30;
}

// 0x0056C650: the guessed address, gotRemoteData and finished (not in the state 2), from the game's socket when there
// is one. 700 ms, 20 times.
void sendPing(Negotiator& n) {
    uint8_t p[20] = {};
    header(p, kConnectPing, n.cookie);
    std::memcpy(p + 12, &n.guessedIp, 4);
    p[16] = static_cast<uint8_t>(n.guessedPort >> 8);
    p[17] = static_cast<uint8_t>(n.guessedPort);
    p[18] = n.gotRemoteData ? 1 : 0;
    p[19] = n.state != kNnConnectPing ? 1 : 0;
    const Address to{n.guessedIp, htons(n.guessedPort)};
    if (n.gameSocket.valid()) {
        n.gameSocket.send(to, p, sizeof p);
    } else if (n.socket) {
        n.socket->sendTo(to.ip, to.port, p, sizeof p);
    }
    n.retryTime = GetTickCount() + 700;
    n.maxRetries = 20;
}

// 0x0056CA60: CONNECT_ACK (21 bytes) to the sender from our own socket. Stock problem: the porttype [12] and the
// bytes 14 .. 20 are what the stack held; zeros here.
void sendConnectAck(Negotiator& n, const Address& to) {
    uint8_t p[21] = {};
    header(p, kConnectAck, n.cookie);
    p[13] = static_cast<uint8_t>(n.clientIndex);
    if (n.socket) n.socket->sendTo(to.ip, to.port, p, sizeof p);
}

void finish(Negotiator& n, int result, std::unique_ptr<UdpSocket> socket, const Address* remote) {
    if (n.complete) n.complete(result, std::move(socket), remote);
}

// 0x0056CB00.
void processConnect(Negotiator& n, const uint8_t* p, const Address& from) {
    if (p[19] == 0) sendConnectAck(n, from);
    if (n.state >= kNnConnectPing) return;
    if (p[19] != 0) {
        const int result = p[19] == 1 ? kNnDeadBeatPartner : p[19] == 2 ? kNnInitTimeout : kNnUnknownError;
        const int32_t cookie = n.cookie;
        finish(n, result, nullptr, nullptr);
        nnCancel(cookie);
        return;
    }
    std::memcpy(&n.guessedIp, p + 12, 4);
    n.guessedPort = static_cast<uint16_t>((p[16] << 8) | p[17]);
    n.retries = 0;
    n.state = kNnConnectPing;
    if (n.progress) n.progress(n.state);
    sendPing(n);
}

// 0x0056CB90.
void processPing(Negotiator& n, const uint8_t* p, const Address& from) {
    if (n.state < kNnConnectPing) return;
    n.guessedIp = from.ip;
    n.guessedPort = ntohs(from.port);
    n.gotRemoteData = true;
    if (p[18] != 0) {
        if (n.state == kNnConnectPing) {
            n.state = kNnFinished;
            n.retryTime = GetTickCount() + 5000;
            if (!n.gameSocket.valid()) return;
            finish(n, kNnSuccess, nullptr, &from);
            return;
        }
        if (p[19] != 0) return;
    }
    sendPing(n);
}

// 0x0056CC00.
void processInitAckOrErtTest(Negotiator& n, uint8_t* p, const Address& from) {
    if (p[7] == kInitAck) {
        const uint8_t portType = p[12];
        if (portType >= 3) return;
        n.initAcked[portType] = true;
        if (n.state == kNnInitSent && n.initAcked[1] && n.initAcked[2] &&
            (!n.gameSocket.valid() || n.initAcked[0])) {
            n.state = kNnInitAcked;
            n.retryTime = GetTickCount() + 60000;
            if (n.progress) n.progress(n.state);
        }
    } else if (p[7] == kErtTest) {
        p[7] = kErtAck;
        sendTo(n.socket.get(), from.ip, ntohs(from.port), p, 21);
    }
}

// Still in the list (a callback may have freed it).
bool alive(const Negotiator* n) {
    if (!negotiators()) return false;
    for (const auto& m : *negotiators()) {
        if (m.get() == n) return true;
    }
    return false;
}

// 0x0056C890. A negotiator cancelled by a callback (the state 4) takes none of the later steps, as in the stock.
void think(Negotiator& n) {
    if (n.state == kNnCanceled) {
        removeNegotiator(&n);
        return;
    }
    if (n.socket) {
        uint8_t buffer[0x200];
        for (;;) {
            uint32_t ip = 0;
            uint16_t port = 0;
            const int got = n.socket->receive(buffer, sizeof buffer, ip, port);
            if (got < 0) break;
            nnProcessData(buffer, static_cast<size_t>(got), Address{ip, port});
            if (!alive(&n)) return;
            if (n.state == kNnCanceled || !n.socket) break;
        }
    }
    if (n.state == kNnInitSent || n.state == kNnConnectPing) {
        if (n.retryTime < GetTickCount()) {
            if (n.retries > n.maxRetries) {
                const int32_t cookie = n.cookie;
                finish(n, kNnInitTimeout, nullptr, nullptr);
                nnCancel(cookie);
                return;
            }
            ++n.retries;
            if (n.state == kNnInitSent) {
                sendInitPackets(n);
            } else {
                sendPing(n);
            }
        }
    }
    if (n.state == kNnFinished && n.retryTime < GetTickCount()) {
        const int32_t cookie = n.cookie;
        if (!n.gameSocket.valid()) {
            // The negotiation's socket handed over (none left to close).
            const Address remote{n.guessedIp, htons(n.guessedPort)};
            finish(n, kNnSuccess, std::move(n.socket), &remote);
        }
        nnCancel(cookie);
        return;
    }
    if (n.state == kNnInitAcked && n.retryTime < GetTickCount()) {
        const int32_t cookie = n.cookie;
        finish(n, kNnDeadBeatPartner, nullptr, nullptr);
        nnCancel(cookie);
    }
}

}  // namespace

int nnBeginNegotiationWithSocket(const GameSocket& gameSocket, int32_t cookie, int clientIndex,
                                 NatNegProgressCallback progress, NatNegCompletedCallback complete) {
    if (availableResult() != AvailableCheck::Available) return kNnSocketError;
    if (!negotiators()) negotiators() = std::make_unique<std::vector<std::unique_ptr<Negotiator>>>();
    negotiators()->push_back(std::make_unique<Negotiator>());
    Negotiator* n = negotiators()->back().get();
    // Stock problem: the negotiator stays in the list zeroed when the servers do not resolve; the stock then calls its
    // null callback at its timeout (a crash), here nothing is called and it goes.
    if (!resolveServers()) return kNnDnsError;
    n->gameSocket = gameSocket;
    n->clientIndex = clientIndex;
    n->cookie = cookie;
    n->progress = std::move(progress);
    n->complete = std::move(complete);
    n->socket = std::make_unique<UdpSocket>();
    n->retries = 0;
    n->gotRemoteData = false;
    n->guessedIp = 0;
    n->guessedPort = 0;
    n->maxRetries = 0;
    if (!n->socket->open()) {
        removeNegotiator(n);
        return kNnSocketError;
    }
    sendInitPackets(*n);
    return kNnNoError;
}

void nnThink() {
    if (!negotiators()) return;
    for (size_t i = negotiators()->size(); i-- > 0;) {
        if (!negotiators() || i >= negotiators()->size()) break;
        think(*(*negotiators())[i]);
    }
}

void nnProcessData(const uint8_t* data, size_t size, const Address& from) {
    if (size < 20 || std::memcmp(data, kMagic, 6) != 0) return;
    const uint8_t type = data[7];
    if (type == kConnect || type == kConnectPing) {
        Negotiator* n = find(static_cast<int32_t>(get32(data + 8)));
        if (n == nullptr) return;
        if (type == kConnect) {
            processConnect(*n, data, from);
        } else {
            processPing(*n, data, from);
        }
    } else if (size >= 21) {
        Negotiator* n = find(static_cast<int32_t>(get32(data + 8)));
        if (n == nullptr) return;
        // The ERTTEST is answered from the packet itself, changed in place.
        uint8_t copy[0x200];
        const size_t n21 = size < sizeof copy ? size : sizeof copy;
        std::memcpy(copy, data, n21);
        processInitAckOrErtTest(*n, copy, from);
    }
}

void nnCancel(int32_t cookie) {
    Negotiator* n = find(cookie);
    if (n == nullptr) return;
    n->socket.reset();
    n->state = kNnCanceled;
}

void nnFreeNegotiateList() {
    negotiators().reset();
}

bool nnNegotiateListExists() {
    return negotiators() != nullptr;
}

size_t nnNegotiatorCount() {
    return negotiators() ? negotiators()->size() : 0;
}

}  // namespace oj::net
