#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Core/Network/Matchmaking.h"

#include "Core/Network/GameSpy/GameSpyUser.h"

#include "Core/Network/GameSpy/GameSpyNatNeg.h"
#include "Core/Network/NetSession.h"
#include "Core/Base/Random.h"

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>

namespace oj::net {

namespace {

constexpr uint8_t kJoin = 0;
constexpr uint8_t kRefused = 1;
constexpr uint8_t kNegotiate = 2;
constexpr uint8_t kPeer = 3;

void put32(uint8_t* p, uint32_t v) {
    std::memcpy(p, &v, 4);
}

uint32_t get32(const uint8_t* p) {
    uint32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

// A sockaddr_in of 16 bytes (the family 2, the port and the address in network order, 8 zeros); none: all 0.
void putSockaddr(uint8_t* p, const std::optional<Address>& a) {
    std::memset(p, 0, 16);
    if (!a) return;
    p[0] = AF_INET;
    std::memcpy(p + 2, &a->port, 2);
    std::memcpy(p + 4, &a->ip, 4);
}

Address readSockaddr(const uint8_t* p) {
    Address a;
    std::memcpy(&a.port, p + 2, 2);
    std::memcpy(&a.ip, p + 4, 4);
    return a;
}

// 0x005977E0: two addresses of the table the same (both none, or both there with the same IP; the ports not looked at).
bool sameIp(const std::optional<Address>& a, const std::optional<Address>& b) {
    if (!a || !b) return !a && !b;
    return a->ip == b->ip;
}

std::string dotted(uint32_t ip) {
    in_addr a{};
    a.s_addr = ip;
    const char* text = inet_ntoa(a);
    return text != nullptr ? text : "";
}

// _qsort (0x005CAE90, the CRT's: the median of three, shortsort up to 8) over the indices.
template <class Compare>
void crtQsort(uint32_t* base, size_t num, Compare comp) {
    if (num < 2) return;
    uint32_t* lostk[30];
    uint32_t* histk[30];
    int stkptr = 0;
    uint32_t* lo = base;
    uint32_t* hi = base + (num - 1);
    for (;;) {
        const size_t size = static_cast<size_t>(hi - lo) + 1;
        if (size <= 8) {
            // shortsort (0x005CAE20).
            while (hi > lo) {
                uint32_t* max = lo;
                for (uint32_t* p = lo + 1; p <= hi; ++p) {
                    if (comp(*p, *max) > 0) max = p;
                }
                std::swap(*max, *hi);
                --hi;
            }
        } else {
            uint32_t* mid = lo + size / 2;
            if (comp(*lo, *mid) > 0) std::swap(*lo, *mid);
            if (comp(*lo, *hi) > 0) std::swap(*lo, *hi);
            if (comp(*mid, *hi) > 0) std::swap(*mid, *hi);
            uint32_t* loguy = lo;
            uint32_t* higuy = hi;
            for (;;) {
                if (mid > loguy) {
                    do {
                        ++loguy;
                    } while (loguy < mid && comp(*loguy, *mid) <= 0);
                }
                if (mid <= loguy) {
                    do {
                        ++loguy;
                    } while (loguy <= hi && comp(*loguy, *mid) <= 0);
                }
                do {
                    --higuy;
                } while (higuy > mid && comp(*higuy, *mid) > 0);
                if (higuy < loguy) break;
                std::swap(*loguy, *higuy);
                if (mid == higuy) mid = loguy;
            }
            ++higuy;
            if (mid < higuy) {
                do {
                    --higuy;
                } while (higuy > mid && comp(*higuy, *mid) == 0);
            }
            if (mid >= higuy) {
                do {
                    --higuy;
                } while (higuy > lo && comp(*higuy, *mid) == 0);
            }
            if (higuy - lo >= hi - loguy) {
                if (lo < higuy) {
                    lostk[stkptr] = lo;
                    histk[stkptr] = higuy;
                    ++stkptr;
                }
                if (loguy < hi) {
                    lo = loguy;
                    continue;
                }
            } else {
                if (loguy < hi) {
                    lostk[stkptr] = loguy;
                    histk[stkptr] = hi;
                    ++stkptr;
                }
                if (lo < higuy) {
                    hi = higuy;
                    continue;
                }
            }
        }
        --stkptr;
        if (stkptr < 0) return;
        lo = lostk[stkptr];
        hi = histk[stkptr];
    }
}

// Not destroyed at the process's end (the statics it calls may be gone by then); released by the game.
Matchmaking*& theMatchmaking() {
    static Matchmaking* mm = nullptr;
    return mm;
}

}  // namespace

// ---- Made and released.

Matchmaking::Matchmaking(Transport* transport, std::shared_ptr<NetGame> keep)
    : m_transport(transport), m_keep(std::move(keep)) {
    if (m_transport != nullptr) {
        m_transport->addFilter({0xFD, 0xFC, 0x1E, 0x66, 0x6A, 0xB2},
                               [](const uint8_t* d, size_t n, const Address& from) { nnProcessData(d, n, from); });
        m_transport->addFilter({0xC8, 0x2A},
                               [this](const uint8_t* d, size_t n, const Address& from) { message(d, n, from); });
    }
}

Matchmaking::~Matchmaking() {
    m_hostAddress.reset();
    m_browser.reset();
    if (nnNegotiateListExists()) nnFreeNegotiateList();
    stopHosting();
    clearPresence();
    if (m_transport != nullptr) {
        m_transport->removeFilter({0xFD, 0xFC, 0x1E, 0x66, 0x6A, 0xB2});
        // Stock problem: the stock leaves the C8 2A filter with the freed object; taken out here.
        m_transport->removeFilter({0xC8, 0x2A});
    }
    clearResults();
    clearPresence();
}

Matchmaking* matchmaking() {
    return theMatchmaking();
}

void makeMatchmaking() {
    releaseMatchmaking();
    std::shared_ptr<NetGame> net = sharedNetGame();
    theMatchmaking() = new Matchmaking(net ? &net->transport : nullptr, net);
}

void releaseMatchmaking() {
    delete theMatchmaking();
    theMatchmaking() = nullptr;
}

GameSocket Matchmaking::gameSocket() const {
    GameSocket s;
    if (m_transport == nullptr) return s;
    Transport* t = m_transport;
    s.send = [t](const Address& to, const void* data, size_t size) { return t->sendRaw(to, data, size); };
    s.port = htons(t->gamePort());
    return s;
}

// ---- The helpers.

void Matchmaking::clearNegotiations() {
    m_ringRead = 0;
    m_ringWrite = 0;
    if (m_negotiating) {
        nnCancel(m_request.cookie);
        m_negotiating = false;
    }
    if (nnNegotiateListExists()) nnFreeNegotiateList();
    m_clients.clear();
}

const MatchAttribute* Matchmaking::hostAttribute(int32_t id) const {
    for (const MatchAttribute& a : m_hostAttributes) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

int32_t Matchmaking::makeCookie() {
    // srand(++count + this) twice; the second's two draws.
    CrtRandom& random = gameRandom();
    const auto self = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    random.seed(++seedCount() + self);
    random.seed(++seedCount() + self);
    const int first = random.next();
    const int second = random.next();
    return second * 0x10000 + first;
}

bool Matchmaking::pushRequest(const Request& r) {
    const uint32_t next = m_ringWrite == 15 ? 0 : m_ringWrite + 1;
    if (next == m_ringRead) return false;
    m_ring[m_ringWrite] = r;
    m_ringWrite = next;
    return true;
}

bool Matchmaking::popRequest(Request& r) {
    if (m_ringRead == m_ringWrite) return false;
    r = m_ring[m_ringRead];
    m_ringRead = m_ringRead == 15 ? 0 : m_ringRead + 1;
    return true;
}

void Matchmaking::pruneClients() {
    // The clients no longer among the transport's peers (by id) go, the last moved into their place.
    if (m_transport == nullptr) return;
    for (size_t i = 0; i < m_clients.size();) {
        bool found = false;
        for (const Peer& p : m_transport->peers()) {
            if (p.id == m_clients[i].id) found = true;
        }
        if (found) {
            ++i;
            continue;
        }
        m_clients[i] = m_clients.back();
        m_clients.pop_back();
    }
}

Address Matchmaking::publicSockaddr() const {
    // Our address as the master saw it and our game port (Stock problem: the port taken as the NAT kept it).
    Address a;
    if (m_transport == nullptr) return a;
    a.ip = m_browser ? m_browser->myPublicIp() : 0;
    a.port = htons(m_transport->gamePort());
    return a;
}

Address Matchmaking::privateSockaddr() const {
    Address a;
    if (m_transport == nullptr) return a;
    char name[128] = {};
    gethostname(name, sizeof name);
    if (const hostent* h = gethostbyname(name); h != nullptr && h->h_addr_list[0] != nullptr) {
        std::memcpy(&a.ip, h->h_addr_list[0], 4);
    }
    a.port = htons(m_transport->gamePort());
    return a;
}

void Matchmaking::sendTo(const std::optional<Address>& to, const uint8_t* data, size_t size) {
    if (m_transport != nullptr && to) m_transport->sendPacket(*to, data, size, true);
}

void Matchmaking::sendJoin(uint32_t direct, uint32_t slots) {
    uint8_t q[0x2C] = {0xD2, 0xC8, 0x2A, kJoin};
    put32(q + 4, direct);
    putSockaddr(q + 8, publicSockaddr());
    putSockaddr(q + 0x18, privateSockaddr());
    put32(q + 0x28, slots);
    sendTo(m_hostAddress, q, sizeof q);
}

// ---- QR2's callbacks.

void Matchmaking::serverKey(int key, Qr2Buffer& out) {
    switch (key) {
    case 1:  // hostname
    case 2:  // gamename (Stock problem: the session's name as well)
        out.add(gsUcs2ToUtf8(m_sessionName));
        return;
    case 6:  // gametype
        out.add(gsUcs2ToUtf8(m_mode));
        return;
    case 8:  // numplayers
        out.addInt(static_cast<int>(m_players));
        return;
    case 10:  // maxplayers
        out.addInt(static_cast<int>(m_openSlots + m_players));
        return;
    case 11:  // gamemode
        if (m_private == 0) {
            out.add(m_playing != 0 ? "openplaying" : "openwaiting");
        } else {
            out.add(m_playing != 0 ? "closedplaying" : "closedwaiting");
        }
        return;
    case 50:
        out.addInt(static_cast<int>(m_players));
        return;
    case 51:
        out.addInt(static_cast<int>(m_openSlots));
        return;
    case 52:
        out.addInt(static_cast<int>(m_private));
        return;
    case 53:
        out.addInt(static_cast<int>(m_sessionId));
        return;
    default:
        break;
    }
    if (key < 50) return;
    if (const MatchAttribute* a = hostAttribute(key - 54); a != nullptr) {
        if (a->type == 1) {
            out.addInt(a->value);
        } else if (a->type == 2) {
            out.add(gsUcs2ToUtf8(a->text));
        }
    }
}

void Matchmaking::keyList(int type, Qr2KeyBuffer& keys) {
    if (type != kQr2ServerKey) return;
    for (int k : {1, 2, 8, 10, 6, 11, 50, 51, 52, 53}) keys.add(k);
    for (const MatchAttribute& a : m_hostAttributes) keys.add(a.id + 54);
}

// [0x0075F1E4]->vt+0x44 for the user's first local player (0x0055BA00: that player's +0x2C first; -1 with none).
void Matchmaking::publishPresence() {
    OnlineUser* user = onlineUser();
    if (user == nullptr) return;
    int32_t player = -1;
    if (!user->localPlayers.empty()) {
        user->playerAddRef(user->localPlayers[0]);
        player = user->localPlayers[0];
    }
    user->setAddress(player, presenceAddress);
}

void Matchmaking::publicAddress(uint32_t ip, uint16_t port) {
    presenceAddress = Address{ip, htons(port)};
    publishPresence();
    publicAddressKnown = 1;
}

void Matchmaking::clearPresence() {
    presenceAddress.reset();
    publishPresence();
}

// ---- The virtual methods.

bool Matchmaking::host(uint32_t privateFlag, uint32_t numPlayers, uint32_t openSlots, uint32_t attributeCount,
                       const MatchAttribute* const* attributes) {
    if (m_transport == nullptr) return false;
    stopHosting();
    CrtRandom& random = gameRandom();
    const auto self = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    random.seed(++seedCount() + self);
    random.seed(++seedCount() + self);
    const int low = random.next();
    m_sessionId = static_cast<uint32_t>((random.next() << 16) + low);
    m_private = privateFlag;
    m_players = numPlayers;
    m_openSlots = openSlots;
    for (int k = 50; k <= 53; ++k) qr2RegisterKey(k, std::format("key{}", k));
    for (uint32_t i = 0; i < attributeCount; ++i) {
        // Stock problem: the i-th attribute's key is key<54 + i>, where everything else takes 54 + its id.
        qr2RegisterKey(static_cast<int>(54 + i), std::format("key{}", 54 + i));
        const MatchAttribute& from = *attributes[i];
        MatchAttribute copy;
        copy.id = from.id;
        copy.type = from.type;
        if (from.type == 1) {
            copy.value = from.value;
        } else if (from.type == 2) {
            copy.length = from.length;
            copy.text = from.text.substr(0, std::min(from.text.size(), from.text.find(u'\0')));
        } else if (from.type == 3) {
            copy.length = from.length;
            copy.bytes = from.bytes;
        } else {
            continue;
        }
        m_hostAttributes.push_back(std::move(copy));
    }
    Qr2& qr2 = qr2Record();
    const int error = qr2.initSocket(
        gameSocket(), m_transport->gamePort(), kGameName, kSecretKey, true, true,
        [this](int key, Qr2Buffer& out) { serverKey(key, out); }, [](int, int, Qr2Buffer&) {},
        [](int, int, Qr2Buffer&) {}, [this](int type, Qr2KeyBuffer& keys) { keyList(type, keys); },
        [](int) { return 0; }, [](int, const std::u16string&) {});
    if (error != kQr2NoError) {
        stopHosting();
        return false;
    }
    m_transport->addFilter({0xFE, 0xFD}, [](const uint8_t* d, size_t n, const Address& from) {
        qr2Record().parseQuery(d, n, from);
    });
    qr2.natNegotiateCallback = [this](int32_t cookie) { pushRequest({cookie, -3, 0, 0}); };
    qr2.publicAddressCallback = [this](uint32_t ip, uint16_t port) { publicAddress(ip, port); };
    m_qr2Up = true;
    return true;
}

bool Matchmaking::update(uint32_t numPlayers, uint32_t openSlots, uint32_t attributeCount,
                         const MatchAttribute* const* attributes, int32_t) {
    m_players = numPlayers;
    m_openSlots = openSlots;
    for (uint32_t i = 0; i < attributeCount; ++i) {
        const MatchAttribute& from = *attributes[i];
        for (MatchAttribute& a : m_hostAttributes) {
            if (a.id != from.id) continue;
            if (a.type == 1) {
                a.value = from.value;
            } else if (a.type == 2) {
                a.text = from.text.substr(0, std::min(from.text.size(), from.text.find(u'\0')));
            }
            break;
        }
    }
    // qr2_send_statechanged on the static record (listed or not).
    qr2Record().sendStateChanged();
    return true;
}

void Matchmaking::setInfo(std::u16string_view name, int32_t playing, std::u16string_view mode) {
    m_sessionName = name.substr(0, std::min(name.size(), name.find(u'\0')));
    m_playing = playing;
    m_mode = mode.substr(0, std::min(mode.size(), mode.find(u'\0')));
}

void Matchmaking::stopHosting() {
    clearNegotiations();
    if (m_qr2Up) {
        qr2Record().shutdown();
        m_qr2Up = false;
    }
    if (m_transport != nullptr) m_transport->removeFilter({0xFE, 0xFD});
    m_hostAttributes.clear();
    stoppedHosting = 1;
}

void Matchmaking::setSearchAttributes(uint32_t maxResultCount, uint32_t count, const SearchAttribute* attributes) {
    for (int k = 50; k <= 53; ++k) qr2RegisterKey(k, std::format("key{}", k));
    m_searchAttributes.clear();
    for (uint32_t i = 0; i < count; ++i) {
        qr2RegisterKey(attributes[i].id + 54, std::format("key{}", attributes[i].id + 54));
        m_searchAttributes.push_back(attributes[i]);
    }
    maxResults = maxResultCount;
}

void Matchmaking::ensureBrowser() {
    if (m_browser) return;
    m_browser = ServerBrowser::create(kGameName, kGameName, kSecretKey, 0, 10, 1,
                                      [this](ServerBrowser&, int reason, SbServer* server) {
                                          searchCallback(reason, server);
                                      });
}

bool Matchmaking::startSearch(int32_t, int32_t, int32_t) {
    clearNegotiations();
    stopSearch();
    listIn = 0;
    detailsIn = 0;
    m_searchTime = 0.0f;
    m_requeryTime = 0.0f;
    clearResults();
    ensureBrowser();
    std::vector<uint8_t> fields = {50, 51, 52, 53};
    for (const SearchAttribute& a : m_searchAttributes) fields.push_back(static_cast<uint8_t>(a.id + 54));
    // Stock problem: without a browser (the available check not 1) the stock clears and updates through a null one.
    if (!m_browser) return true;
    m_browser->clear();
    const std::string filterText = gsUcs2ToUtf8(filter);
    m_browser->update(true, true, fields.data(), static_cast<int>(fields.size()), filterText.c_str(), 0, 0);
    return true;
}

void Matchmaking::searchThrough(const std::optional<Address>& address) {
    if (!m_browser || !address) return;
    const std::string ip = dotted(address->ip);
    m_browser->clear();
    m_browser->auxUpdateIp(ip.c_str(), ntohs(address->port), true, true, true);
}

bool Matchmaking::searchFriend(int32_t, int32_t friendIndex) {
    OnlineFriends* friends = onlineFriends();
    if (friends == nullptr || friendIndex == -1) return false;
    clearNegotiations();
    stopSearch();
    listIn = 0;
    detailsIn = 0;
    m_searchTime = 0.0f;
    m_requeryTime = 0.0f;
    clearResults();
    ensureBrowser();
    // (The basic fields are made here too and left unused.)
    searchThrough(friends->address(friendIndex));
    return true;
}

bool Matchmaking::refreshResult(int32_t, uint32_t sortedIndex) {
    if (onlineFriends() == nullptr) return false;
    // The position checked against the results (not the sorted positions).
    if (sortedIndex >= results.size()) return false;
    clearNegotiations();
    stopSearch();
    listIn = 0;
    detailsIn = 0;
    m_searchTime = 0.0f;
    m_requeryTime = 0.0f;
    const std::optional<Address> address = results[sorted[sortedIndex]].address;
    clearResults();
    ensureBrowser();
    searchThrough(address);
    return true;
}

bool Matchmaking::searchAddress(int32_t, const std::optional<Address>& address) {
    if (onlineFriends() == nullptr || !address) return false;
    clearNegotiations();
    stopSearch();
    listIn = 0;
    detailsIn = 0;
    m_searchTime = 0.0f;
    m_requeryTime = 0.0f;
    clearResults();
    ensureBrowser();
    searchThrough(address);
    return true;
}

void Matchmaking::stopSearch() {
    if (!m_browser) return;
    listIn = 1;
    detailsIn = 1;
    m_browser->halt();
}

void Matchmaking::join(uint32_t index, uint32_t slots) {
    if (m_transport == nullptr || index >= results.size() || !m_browser) return;
    clearNegotiations();
    stopSearch();
    clearNegotiations();
    m_transport->joinOnline(slots);
    const SearchResult& r = results[index];
    m_sessionId = r.sessionId;
    presenceAddress = r.address;
    publishPresence();
    const SbServer* s = r.server;
    if ((s->flags & kSbPrivateIp) != 0 && m_browser->myPublicIp() == s->publicIp) {
        // Behind the same NAT: its private address with our game port.
        m_hostAddress = Address{s->privateIp, htons(m_transport->gamePort())};
        sendJoin(1, slots);
    } else if ((s->flags & kSbUnsolicitedUdp) != 0) {
        m_hostAddress = Address{s->publicIp, s->publicPort};
        sendJoin(1, slots);
    } else {
        // A NAT negotiation: the cookie through the master to the host's QR2, ours as the client 1.
        const std::string ip = dotted(s->publicIp);
        const int32_t cookie = makeCookie();
        m_request = {cookie, 0, slots, 0};
        m_browser->sendNatNegotiateCookieToServer(ip.c_str(), ntohs(s->publicPort), cookie);
        nnBeginNegotiationWithSocket(gameSocket(), cookie, 1, [](int) {},
                                     [this](int result, std::unique_ptr<UdpSocket>, const Address* remote) {
                                         negotiated(result, remote);
                                     });
        m_negotiating = true;
        m_negotiationTime = 0.0f;
    }
}

void Matchmaking::update(float dt) {
    if (m_qr2Up) {
        Qr2& qr2 = qr2Record();
        if (qr2.isPublic()) qr2.checkSendHeartbeat();
        qr2.checkQueries();
    }
    if (m_browser) m_browser->think();
    nnThink();
    if (m_negotiating) {
        m_negotiationTime += dt;
        if (m_negotiationTime > 20.0f) {
            nnCancel(m_request.cookie);
            m_negotiating = false;
            if (nnNegotiateListExists()) nnFreeNegotiateList();
        }
    } else if (popRequest(m_request) && m_transport != nullptr) {
        nnBeginNegotiationWithSocket(gameSocket(), m_request.cookie, static_cast<uint8_t>(m_request.index),
                                     [](int) {},
                                     [this](int result, std::unique_ptr<UdpSocket>, const Address* remote) {
                                         negotiated(result, remote);
                                     });
        m_negotiating = true;
        m_negotiationTime = 0.0f;
    }
    if (m_browser && listIn == 0) {
        m_searchTime += dt;
        if (m_searchTime > 10.0f) {
            listIn = 1;
            detailsIn = 1;
            m_searchTime = 0.0f;
            m_browser->halt();
        } else {
            m_requeryTime += dt;
            if (m_requeryTime > 1.0f) {
                m_requeryTime = 0.0f;
                // The results without their keys asked again through the master.
                for (size_t i = 0; i < results.size(); ++i) {
                    if (results[i].valid != 0 || !results[i].address) continue;
                    const std::string ip = dotted(results[i].address->ip);
                    m_browser->auxUpdateIp(ip.c_str(), ntohs(results[i].address->port), true, true, true);
                }
            }
        }
    }
}

void Matchmaking::leave() {
    stopHosting();
    clearPresence();
}

// ---- The search.

void Matchmaking::clearResults() {
    results.clear();
    sorted.clear();
}

void Matchmaking::searchCallback(int reason, SbServer* server) {
    if (reason == ServerBrowser::kUpdateComplete) {
        listIn = 1;
        detailsIn = 1;
        return;
    }
    if (reason != ServerBrowser::kServerAdded && reason != ServerBrowser::kServerUpdated) return;
    SearchResult r;
    r.address = Address{server->publicIp, server->publicPort};
    r.server = server;
    float quality = 1.0f - static_cast<float>(static_cast<int32_t>(server->updateTime)) * 0.0033f;
    if (quality < 0.0f) {
        quality = 0.0f;
    } else if (quality > 1.0f) {
        quality = 1.0f;
    }
    r.quality = quality;
    if ((server->state & (kSbStateBasicKeys | kSbStateFullKeys)) != 0) {
        r.players = static_cast<uint32_t>(server->intValue("key50", 0));
        r.openSlots = static_cast<uint32_t>(server->intValue("key51", 0));
        r.closed = static_cast<uint32_t>(server->intValue("key52", 0));
        r.sessionId = static_cast<uint32_t>(server->intValue("key53", 0));
        for (const SearchAttribute& s : m_searchAttributes) {
            const std::string key = std::format("key{}", s.id + 54);
            MatchAttribute a;
            a.id = s.id;
            if (s.type == 1) {
                a.type = 1;
                a.value = server->intValue(key, 0);
            } else if (s.type == 2) {
                // The value's first `length` characters (Stock problem: copied whatever its length; past its end
                // whatever followed it).
                a.type = 2;
                a.length = s.length;
                const std::u16string text = gsUtf8ToUcs2(server->stringValue(key, ""));
                a.text = text.substr(0, static_cast<size_t>(std::max(s.length, 0)));
            } else {
                continue;
            }
            r.attributes.push_back(std::move(a));
        }
        if (r.players != 0 && r.players < 6 && r.openSlots != 0 && r.openSlots < 6) r.valid = 1;
    }
    insertResult(r);
}

bool Matchmaking::insertResult(SearchResult& record) {
    // One of the same address (both none, or the same IP and port) is replaced.
    for (SearchResult& r : results) {
        const bool same = !r.address || !record.address
                              ? !r.address && !record.address
                              : r.address->ip == record.address->ip && r.address->port == record.address->port;
        if (same) {
            r = std::move(record);
            return true;
        }
    }
    if (results.size() < maxResults) {
        sorted.push_back(static_cast<uint32_t>(results.size()));
        results.push_back(std::move(record));
        return true;
    }
    // Full: the first valid one of a lower quality than a valid new one (never the selected).
    for (size_t i = 0; i < results.size(); ++i) {
        SearchResult& r = results[i];
        if (r.valid != 0 && record.valid != 0 && r.quality < record.quality && static_cast<int32_t>(i) != selected) {
            r = std::move(record);
            return true;
        }
    }
    return false;
}

void Matchmaking::sortResults() {
    if (sorted.empty()) return;
    for (size_t i = 0; i < sorted.size(); ++i) sorted[i] = static_cast<uint32_t>(i);
    // 0x00564550: past the results 1; the same quality 1; the better first.
    crtQsort(sorted.data(), sorted.size(), [this](uint32_t a, uint32_t b) {
        if (a >= results.size() || b >= results.size()) return 1;
        const float qa = results[a].quality;
        const float qb = results[b].quality;
        if (qa == qb) return 1;
        return qa > qb ? -1 : 1;
    });
}

void Matchmaking::setFilter(std::string_view text) {
    if (text.size() >= 0x200) return;
    filter.clear();
    for (char c : text) {
        if (c == 0) break;
        filter.push_back(static_cast<char16_t>(static_cast<int8_t>(c)));
    }
}

const SearchResult* Matchmaking::sortedResult(uint32_t position) const {
    if (position >= sorted.size() || sorted[position] >= results.size()) return nullptr;
    return &results[sorted[position]];
}

// ---- The join.

void Matchmaking::negotiated(int result, const Address* remote) {
    if (m_transport == nullptr || !m_negotiating) return;
    if (result == kNnSuccess) {
        m_hostAddress = *remote;
        // The host waits for the JOIN that follows (the negotiation still on).
        if (m_transport->hosting()) return;
        if (m_request.peerId == 0) {
            sendJoin(0, m_transport->slots());
        } else {
            m_transport->addPeer(m_request.peerId, *m_hostAddress, m_request.slots);
        }
    }
    m_negotiating = false;
}

void Matchmaking::message(const uint8_t* packet, size_t size, const Address& from) {
    if (m_transport == nullptr || size < 4) return;
    switch (packet[3]) {
    case kJoin:
        if (size >= 0x2C) joinAsked(packet, size, from);
        break;
    case kRefused:
        m_transport->refused();
        break;
    case kNegotiate:
        if (size >= 0x14) {
            Request r;
            r.cookie = static_cast<int32_t>(get32(packet + 4));
            r.peerId = static_cast<int32_t>(get32(packet + 8));
            r.slots = get32(packet + 0x0C);
            r.index = get32(packet + 0x10);
            pushRequest(r);
        }
        break;
    case kPeer:
        if (size >= 0x1C) {
            const int32_t id = static_cast<int32_t>(get32(packet + 0x14));
            const uint32_t slots = get32(packet + 0x18);
            if (id == 0) {
                if (m_hostAddress) m_transport->addPeer(0, *m_hostAddress, slots);
            } else {
                m_transport->addPeer(id, readSockaddr(packet + 4), slots);
            }
        }
        break;
    default:
        break;
    }
}

void Matchmaking::joinAsked(const uint8_t* packet, size_t, const Address& from) {
    if (!m_transport->hosting()) return;
    const uint32_t direct = get32(packet + 4);
    if (direct == 0) {
        if (!m_negotiating) return;
        m_negotiating = false;
    }
    const int32_t id = m_transport->takeId();
    const uint32_t slots = get32(packet + 0x28);
    const std::optional<Address> address = direct != 0 ? std::optional<Address>(from) : m_hostAddress;
    const std::optional<Address> publicAddress = readSockaddr(packet + 8);
    const std::optional<Address> privateAddress = readSockaddr(packet + 0x18);
    if (!address || !m_transport->addPeer(id, *address, slots)) {
        const uint8_t refused[4] = {0xD2, 0xC8, 0x2A, kRefused};
        sendTo(address, refused, sizeof refused);
        return;
    }
    pruneClients();
    // The host to the new client (the address none: its own), then each client before it and it of each other: their
    // private addresses behind the same NAT, else a NAT negotiation of both on a new cookie.
    uint8_t peer[0x1C] = {0xD2, 0xC8, 0x2A, kPeer};
    putSockaddr(peer + 4, std::nullopt);
    put32(peer + 0x14, 0);
    put32(peer + 0x18, m_transport->slots());
    sendTo(address, peer, sizeof peer);
    for (const Client& e : m_clients) {
        if (sameIp(e.publicAddress, publicAddress)) {
            uint8_t toOld[0x1C] = {0xD2, 0xC8, 0x2A, kPeer};
            putSockaddr(toOld + 4, privateAddress);
            put32(toOld + 0x14, static_cast<uint32_t>(id));
            put32(toOld + 0x18, slots);
            sendTo(e.address, toOld, sizeof toOld);
            uint8_t toNew[0x1C] = {0xD2, 0xC8, 0x2A, kPeer};
            putSockaddr(toNew + 4, e.privateAddress);
            put32(toNew + 0x14, static_cast<uint32_t>(e.id));
            put32(toNew + 0x18, e.slots);
            sendTo(address, toNew, sizeof toNew);
        } else {
            // Stock problem: the bytes 0x11 .. 0x13 are what the stack held; zeros here.
            const int32_t cookie = makeCookie();
            uint8_t toOld[0x14] = {0xD2, 0xC8, 0x2A, kNegotiate};
            put32(toOld + 4, static_cast<uint32_t>(cookie));
            put32(toOld + 8, static_cast<uint32_t>(id));
            put32(toOld + 0x0C, slots);
            toOld[0x10] = 0;
            sendTo(e.address, toOld, sizeof toOld);
            uint8_t toNew[0x14] = {0xD2, 0xC8, 0x2A, kNegotiate};
            put32(toNew + 4, static_cast<uint32_t>(cookie));
            put32(toNew + 8, static_cast<uint32_t>(e.id));
            put32(toNew + 0x0C, e.slots);
            toNew[0x10] = 1;
            sendTo(address, toNew, sizeof toNew);
        }
    }
    m_clients.push_back({id, slots, address, publicAddress, privateAddress});
}

// ---- The windows' readers.

int32_t countValidResults() {
    const Matchmaking* mm = matchmaking();
    if (mm == nullptr) return 0;
    int32_t n = 0;
    for (uint32_t i = 0; i < mm->results.size(); ++i) {
        const SearchResult* r = mm->sortedResult(i);
        if (r != nullptr && r->valid != 0) ++n;
    }
    return n;
}

const MatchAttribute* findAttribute(const SearchResult& result, int32_t id) {
    for (const MatchAttribute& a : result.attributes) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

int32_t quickMatchResult() {
    const Matchmaking* mm = matchmaking();
    if (mm == nullptr) return -1;
    for (uint32_t i = 0; i < mm->results.size(); ++i) {
        const SearchResult* r = mm->sortedResult(i);
        if (r == nullptr || r->valid == 0) continue;
        // 0x004E6440's playing (attribute 1); Stock problem: without it, what the window's stack held (0 here).
        const MatchAttribute* playing = findAttribute(*r, 1);
        if (playing == nullptr || playing->value == 0) return static_cast<int32_t>(i);
    }
    for (uint32_t i = 0; i < mm->results.size(); ++i) {
        const SearchResult* r = mm->sortedResult(i);
        if (r != nullptr && r->valid != 0) return static_cast<int32_t>(i);
    }
    return -1;
}

int32_t qualityMarks(uint32_t position) {
    const Matchmaking* mm = matchmaking();
    if (mm == nullptr) return 0;
    const SearchResult* r = mm->sortedResult(position);
    if (r == nullptr || r->quality == 0.0f) return 0;
    const int32_t marks = 1 - static_cast<int32_t>(std::trunc(r->quality * -5.0f));
    if (marks < 1) return 1;
    return marks > 5 ? 5 : marks;
}

}  // namespace oj::net
