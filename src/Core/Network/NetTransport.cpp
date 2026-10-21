#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Core/Network/NetTransport.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace oj::net {

namespace {

constexpr uint8_t kSearch = 0xDD;    // 8666: the search's question
constexpr uint8_t kFound = 0xDC;     // 8666: the host's answer
constexpr uint8_t kFiltered = 0xD2;  // 8667: a packet for the filters
constexpr uint8_t kAck = 0xD4;
constexpr uint8_t kReliable = 0xD5;
constexpr uint8_t kPong = 0xD6;
constexpr uint8_t kPing = 0xD7;
constexpr uint8_t kKeepAlive = 0xD8;
constexpr uint8_t kLeave = 0xD9;
constexpr uint8_t kRefused = 0xDA;
constexpr uint8_t kJoin = 0xDB;
constexpr uint8_t kPeerGone = 0xDE;  // an event
constexpr uint8_t kPeer = 0xDF;      // 8667 and an event

uint32_t u32(const uint8_t* p) {
    uint32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

void put32(uint8_t* p, uint32_t v) {
    std::memcpy(p, &v, 4);
}

// A sockaddr_in of 16 bytes as the wire keeps it (the family 2, the port, the address, 8 zeros).
void putAddress(uint8_t* p, const Address& a) {
    std::memset(p, 0, 16);
    p[0] = AF_INET;
    std::memcpy(p + 2, &a.port, 2);
    std::memcpy(p + 4, &a.ip, 4);
}

Address readAddress(const uint8_t* p) {
    Address a;
    std::memcpy(&a.port, p + 2, 2);
    std::memcpy(&a.ip, p + 4, 4);
    return a;
}

void putInfo(uint8_t* p, const SessionInfo& s) {
    std::memcpy(p, s.name.data(), 64);
    put32(p + 0x40, s.used);
    put32(p + 0x44, s.max);
}

SessionInfo readInfo(const uint8_t* p) {
    SessionInfo s;
    std::memcpy(s.name.data(), p, 64);
    s.used = u32(p + 0x40);
    s.max = u32(p + 0x44);
    return s;
}

SOCKET sock(uintptr_t s) {
    return static_cast<SOCKET>(s);
}

}  // namespace

Transport::~Transport() {
    close();
}

void Transport::code(uint8_t* data, size_t size) {
    uint8_t k = 0xD2;
    for (size_t i = 0; i < size; ++i) {
        k = static_cast<uint8_t>(k * 0x5D - 0x3D);
        data[i] ^= k;
    }
}

void Transport::reseed(CrtRandom& random) {
    ++seedCount();
    random.seed(seedCount() + static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)));
}

void Transport::randomBytes(uint8_t* out, size_t n, CrtRandom& random) {
    // Stock problem: the game's rand() is seeded again for each byte, with the count and the object's address; what
    // the game draws after it depends on where the object lies.
    for (size_t i = 0; i < n; ++i) {
        reseed(random);
        out[i] = static_cast<uint8_t>(random.next());
    }
}

bool Transport::open(const Options& options, CrtRandom& random, std::string* why) {
    close();
    m_options = options;
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        if (why != nullptr) *why = "WSAStartup failed";
        return false;
    }
    m_winsock = true;
    uint8_t counter[4];
    randomBytes(counter, 4, random);
    m_counter = u32(counter);
    m_errorTimer = -1.0f;
    auto make = [&](uint16_t port, bool lan) -> uintptr_t {
        const SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) return ~uintptr_t{0};
        if (lan && m_options.shareLanPort) {
            const BOOL on = TRUE;
            setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof on);
        }
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(port);
        if (bind(s, reinterpret_cast<const sockaddr*>(&a), sizeof a) != 0) {
            closesocket(s);
            return ~uintptr_t{0};
        }
        u_long nonBlocking = 1;
        ioctlsocket(s, FIONBIO, &nonBlocking);
        if (lan) {
            const BOOL on = TRUE;
            setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), sizeof on);
        }
        return static_cast<uintptr_t>(s);
    };
    m_lan = make(m_options.lanPort, true);
    m_game = make(m_options.gamePort, false);
    if (m_lan == ~uintptr_t{0} || m_game == ~uintptr_t{0}) {
        if (why != nullptr) *why = "the UDP ports cannot be taken";
        close();
        return false;
    }
    m_buffer.assign(m_options.bufferSize, 0);
    return true;
}

void Transport::close() {
    if (m_lan != ~uintptr_t{0}) closesocket(sock(m_lan));
    if (m_game != ~uintptr_t{0}) closesocket(sock(m_game));
    m_lan = ~uintptr_t{0};
    m_game = ~uintptr_t{0};
    if (m_winsock) WSACleanup();
    m_winsock = false;
    m_peers.clear();
    m_found.clear();
    m_queue.clear();
    m_taken.clear();
    m_hosting = m_answering = m_searching = m_connecting = m_hostKnown = false;
    m_state = State::Normal;
}

void Transport::addFilter(const std::vector<uint8_t>& prefix, Filter f) {
    removeFilter(prefix);
    if (prefix.size() >= 9) return;
    m_filters.push_back({prefix, std::move(f)});
}

void Transport::removeFilter(const std::vector<uint8_t>& prefix) {
    for (size_t i = 0; i < m_filters.size(); ++i) {
        if (m_filters[i].prefix == prefix) {
            m_filters[i] = std::move(m_filters.back());
            m_filters.pop_back();
            return;
        }
    }
}

const Peer* Transport::peer(int32_t id) const {
    for (const Peer& p : m_peers) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

Peer* Transport::peerAt(const Address& address) {
    for (Peer& p : m_peers) {
        if (p.address == address) return &p;
    }
    return nullptr;
}

void Transport::host(std::u16string_view name, uint32_t slots, uint32_t max, const std::optional<Address>& address) {
    // 0x0059A590: the address given, else the host's own: the first of its name's addresses and the game's port.
    Address own;
    own.port = htons(m_options.gamePort);
    own.ip = m_options.ownIp;
    if (address) {
        own = *address;
    } else if (own.ip == 0) {
        char hostName[128] = {};
        gethostname(hostName, sizeof hostName);
        if (const hostent* h = gethostbyname(hostName); h != nullptr && h->h_addr_list[0] != nullptr) {
            std::memcpy(&own.ip, h->h_addr_list[0], 4);
        }
    }
    m_slots = slots;
    m_hosting = true;
    m_answering = true;
    m_searching = false;
    m_connecting = false;
    m_hostKnown = true;
    m_hostAddress = own;
    // The name: 32 characters at most, the last always the end.
    m_session.name.fill(0);
    for (size_t i = 0; i < 31 && i < name.size() && name[i] != 0; ++i) m_session.name[i] = name[i];
    m_session.used = slots;
    m_session.max = max;
}

void Transport::search(CrtRandom& random) {
    m_found.clear();
    randomBytes(m_searchId.data(), m_searchId.size(), random);
    uint8_t q[9] = {kSearch};
    std::memcpy(q + 1, m_searchId.data(), 8);
    broadcast(q, sizeof q);
    m_searching = true;
}

void Transport::join(const Address& hostAddress, uint32_t slots) {
    m_state = State::Normal;
    uint8_t q[8] = {kJoin};
    put32(q + 4, slots);
    sendPacket(hostAddress, q, sizeof q, true);
    m_slots = slots;
    m_hosting = false;
    m_searching = false;
    m_joinTimer = 0.0f;
    m_connecting = true;
}

void Transport::joinOnline(uint32_t slots) {
    m_slots = slots;
    m_hosting = false;
    m_searching = false;
    m_joinTimer = 0.0f;
    m_connecting = true;
}

void Transport::refused() {
    m_connecting = false;
    m_state = State::Refused;
}

void Transport::leave() {
    if (m_hostKnown) {
        const uint8_t q[1] = {kLeave};
        send(-2, q, sizeof q, false);
    }
    m_hostKnown = false;
    m_hostAddress = {};
    m_slots = 0;
    m_hosting = false;
    m_connecting = false;
    m_state = State::Normal;
    m_peers.clear();
    m_queue.clear();
    m_taken.clear();
}

void Transport::update(float dt) {
    if (m_searching) searchFrame(dt);
    if (m_connecting) {
        m_joinTimer = dt + m_joinTimer;
        if (15.0f < m_joinTimer) {
            m_connecting = false;
            m_state = State::JoinTimedOut;
        }
    }
    if (m_hostKnown) {
        m_keepAlive = dt + m_keepAlive;
        if (1.5f < m_keepAlive) {
            m_keepAlive = 0.0f;
            const uint8_t q[1] = {kKeepAlive};
            send(-2, q, sizeof q, false);
        }
        peersFrame(dt);
        if (m_pinging) pingFrame(dt);
    }
    queueFrame(dt);
    takenFrame(dt);
    if (opened()) {
        for (int i = 0; i < 30; ++i) {
            receiveLan();
            receiveGame();
        }
    }
    if (0.0f <= m_errorTimer) m_errorTimer = dt + m_errorTimer;
    if (3.0f < m_errorTimer) networkLost() = true;
}

bool Transport::send(int32_t peerId, const void* data, size_t size, bool reliable) {
    if (peerId == -2) {
        bool all = true;
        // A copy: a send may not change the list, but the handler's events could.
        const std::vector<Peer> peers = m_peers;
        for (const Peer& p : peers) {
            if (!send(p.id, data, size, reliable)) all = false;
        }
        return all;
    }
    const Peer* p = peer(peerId);
    if (p == nullptr) return false;
    sendPacket(p->address, static_cast<const uint8_t*>(data), size, reliable);
    return true;
}

bool Transport::sendRaw(const Address& to, const void* data, size_t size) {
    return sendTo(to, static_cast<const uint8_t*>(data), size);
}

bool Transport::sendRelay(int32_t peerId, uint8_t* data, size_t size) {
    if (peerId == -2) {
        bool all = true;
        const std::vector<Peer> peers = m_peers;
        for (const Peer& p : peers) {
            if (!sendRelay(p.id, data, size)) all = false;
        }
        return all;
    }
    const Peer* p = peer(peerId);
    if (p == nullptr) return false;
    const auto stream = [&] {
        uint32_t k = 0x4D2;
        for (size_t i = 0; i < size; ++i) {
            k = k * 0xB275D + 0x269EC3;
            data[i] ^= static_cast<uint8_t>(k);
        }
    };
    stream();
    if (size >= 2) sendTo(p->address, data + 2, size - 2);
    stream();
    return true;
}

bool Transport::sendTo(const Address& to, const uint8_t* data, size_t size) {
    if (m_game == ~uintptr_t{0}) return false;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = to.port;
    a.sin_addr.s_addr = to.ip;
    const int sent = sendto(sock(m_game), reinterpret_cast<const char*>(data), static_cast<int>(size), 0,
                            reinterpret_cast<const sockaddr*>(&a), sizeof a);
    if (sent == SOCKET_ERROR) {
        if (m_errorTimer < 0.0f) m_errorTimer = 0.0f;
        return false;
    }
    m_errorTimer = -1.0f;
    return true;
}

bool Transport::broadcast(const uint8_t* data, size_t size) {
    if (m_lan == ~uintptr_t{0}) return false;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(m_options.broadcastPort != 0 ? m_options.broadcastPort : m_options.lanPort);
    a.sin_addr.s_addr = m_options.broadcastIp != 0 ? m_options.broadcastIp : INADDR_BROADCAST;
    const int sent = sendto(sock(m_lan), reinterpret_cast<const char*>(data), static_cast<int>(size), 0,
                            reinterpret_cast<const sockaddr*>(&a), sizeof a);
    if (sent == SOCKET_ERROR) {
        if (m_errorTimer < 0.0f) m_errorTimer = 0.0f;
        return false;
    }
    m_errorTimer = -1.0f;
    return true;
}

void Transport::sendPacket(const Address& to, const uint8_t* data, size_t size, bool reliable) {
    if (!reliable) {
        std::vector<uint8_t> coded(data, data + size);
        code(coded.data(), coded.size());
        sendTo(to, coded.data(), coded.size());
        return;
    }
    // 0xD5, its number at [4] and the packet at [8]; three bytes more after it (the stock's are whatever the heap
    // held; zeros here), taken off by the receiver.
    Queued q;
    q.sequence = m_counter++;
    q.to = to;
    q.bytes.assign(size + 11, 0);
    q.bytes[0] = kReliable;
    put32(q.bytes.data() + 4, q.sequence);
    if (size != 0) std::memcpy(q.bytes.data() + 8, data, size);
    code(q.bytes.data(), q.bytes.size());
    sendTo(q.to, q.bytes.data(), q.bytes.size());
    m_queue.push_back(std::move(q));
}

void Transport::receiveLan() {
    const int n = recv(sock(m_lan), reinterpret_cast<char*>(m_buffer.data()), static_cast<int>(m_buffer.size()), 0);
    if (n == SOCKET_ERROR || n <= 0) return;
    if (m_buffer[0] == kFound) {
        sessionFound(m_buffer.data(), static_cast<size_t>(n));
    } else if (m_buffer[0] == kSearch) {
        answerSearch(m_buffer.data(), static_cast<size_t>(n));
    }
}

void Transport::receiveGame() {
    sockaddr_in a{};
    int length = sizeof a;
    const int n = recvfrom(sock(m_game), reinterpret_cast<char*>(m_buffer.data()), static_cast<int>(m_buffer.size()),
                           0, reinterpret_cast<sockaddr*>(&a), &length);
    if (n == SOCKET_ERROR || n <= 0) return;
    const Address from{a.sin_addr.s_addr, a.sin_port};
    const auto size = static_cast<size_t>(n);
    if (filters(m_buffer.data(), size, from, 0)) return;
    code(m_buffer.data(), size);
    dispatch(m_buffer.data(), size, from);
}

bool Transport::filters(const uint8_t* data, size_t size, const Address& from, size_t skip) {
    for (const FilterEntry& f : m_filters) {
        if (size < skip + f.prefix.size()) continue;
        if (std::memcmp(data + skip, f.prefix.data(), f.prefix.size()) != 0) continue;
        if (f.callback) f.callback(data, size, from);
        return true;
    }
    return false;
}

void Transport::dispatch(const uint8_t* data, size_t size, const Address& from) {
    if (size == 0) return;
    switch (data[0]) {
    case kFiltered:
        filters(data, size, from, 1);
        return;
    case kAck:
        acknowledged(data, size);
        return;
    case kReliable:
        reliable(data, size, from);
        return;
    case kPong:
        // 0x005981C0: the answer to the last ping: its time into the peer's ring.
        if (size >= 8 && u32(data + 4) == m_pingNumber) {
            if (Peer* p = peerAt(from)) {
                p->pings[p->ring] = m_pingTimer;
                p->ring = (p->ring + 1) & 3;
            }
        }
        return;
    case kPing: {
        // 0x00598150: 0xD6 with the ping's number back to whoever asked.
        uint8_t q[8] = {kPong};
        if (size >= 8) std::memcpy(q + 4, data + 4, 4);
        code(q, sizeof q);
        sendTo(from, q, sizeof q);
        return;
    }
    case kKeepAlive:
        if (Peer* p = peerAt(from)) p->silence = 0.0f;
        return;
    case kLeave:
        // 0x005980F0: the peer goes (0x00597B90 with 0).
        for (size_t i = 0; i < m_peers.size(); ++i) {
            if (m_peers[i].address == from) {
                removePeer(i, false);
                break;
            }
        }
        return;
    case kRefused:
        m_connecting = false;
        m_state = State::Refused;
        return;
    case kJoin:
        joinAsked(data, size, from);
        return;
    case kPeer:
        // 0x00598110: a peer of the session at the address the packet gives.
        if (size >= 0x1C) addPeer(static_cast<int32_t>(u32(data + 4)), readAddress(data + 0x0C), u32(data + 8));
        return;
    default:
        if (m_handler) {
            if (const Peer* p = peerAt(from)) m_handler(data, size, p->id);
        }
        return;
    }
}

void Transport::reliable(const uint8_t* data, size_t size, const Address& from) {
    if (size < 8) return;
    const uint32_t sequence = u32(data + 4);
    uint8_t ack[8] = {kAck};
    put32(ack + 4, sequence);
    code(ack, sizeof ack);
    sendTo(from, ack, sizeof ack);
    for (const Taken& t : m_taken) {
        if (t.from == from && t.sequence == sequence) return;
    }
    m_taken.push_back({sequence, from, 0.0f});
    dispatch(data + 8, size >= 11 ? size - 11 : 0, from);
}

void Transport::acknowledged(const uint8_t* data, size_t size) {
    if (size < 8) return;
    const uint32_t sequence = u32(data + 4);
    // As the stock's loop: the entry moved into a removed one's place is passed over.
    for (size_t i = 0; i < m_queue.size(); ++i) {
        if (m_queue[i].sequence == sequence) {
            m_queue[i] = std::move(m_queue.back());
            m_queue.pop_back();
        }
    }
}

void Transport::answerSearch(const uint8_t* request, size_t size) {
    if (!m_hosting || !m_answering || size < 9) return;
    uint8_t a[100] = {kFound};
    std::memcpy(a + 1, request + 1, 8);
    putInfo(a + 0x0C, m_session);
    putAddress(a + 0x54, m_hostAddress);
    broadcast(a, sizeof a);
}

void Transport::sessionFound(const uint8_t* reply, size_t size) {
    if (!m_searching || size < 100) return;
    if (std::memcmp(reply + 1, m_searchId.data(), 8) != 0) return;
    const Address address = readAddress(reply + 0x54);
    const SessionInfo info = readInfo(reply + 0x0C);
    for (FoundSession& f : m_found) {
        if (f.address == address) {
            f.info = info;
            f.age = 0.0f;
            return;
        }
    }
    m_found.push_back({address, info, 0.0f});
}

void Transport::tellPeer(const Address& to, int32_t id, uint32_t slots, const Address& about) {
    uint8_t q[0x1C] = {kPeer};
    put32(q + 4, static_cast<uint32_t>(id));
    put32(q + 8, slots);
    putAddress(q + 0x0C, about);
    sendPacket(to, q, sizeof q, true);
}

void Transport::joinAsked(const uint8_t* data, size_t size, const Address& from) {
    if (!m_hosting || size < 8) return;
    const auto id = static_cast<int32_t>(m_counter++);
    const uint32_t slots = u32(data + 4);
    if (!addPeer(id, from, slots)) {
        const uint8_t q[1] = {kRefused};
        sendPacket(from, q, sizeof q, true);
        return;
    }
    // The new peer hears of the host, then each peer before it hears of it and it of each.
    tellPeer(from, 0, m_slots, m_hostAddress);
    for (size_t i = 0; i + 1 < m_peers.size(); ++i) {
        const Peer old = m_peers[i];
        tellPeer(old.address, id, slots, from);
        tellPeer(from, old.id, old.slots, old.address);
    }
}

bool Transport::addPeer(int32_t id, const Address& address, uint32_t slots) {
    for (size_t i = 0; i < m_peers.size(); ++i) {
        if (m_peers[i].address == address) {
            removePeer(i, false);
            break;
        }
    }
    if (m_hosting) {
        const uint32_t used = m_session.used + slots;
        if (m_session.max < used) return false;
        m_session.used = used;
    }
    Peer p;
    p.id = id;
    p.address = address;
    p.slots = slots;
    m_peers.push_back(p);
    if (id == 0) {
        m_hosting = false;
        m_connecting = false;
        m_state = State::Normal;
        m_hostKnown = true;
        m_hostAddress = address;
    }
    event(kPeer, id, static_cast<int32_t>(slots));
    return true;
}

void Transport::removePeer(size_t index, bool timedOut) {
    const Peer p = m_peers[index];
    if (m_hosting) m_session.used -= p.slots;
    if (p.id == 0) {
        m_state = timedOut ? State::HostTimedOut : State::HostLeft;
        m_hostKnown = false;
    }
    event(kPeerGone, p.id, timedOut ? 1 : 0);
    // The list may have changed under the handler; the peer is looked for again.
    for (size_t i = 0; i < m_peers.size(); ++i) {
        if (m_peers[i].id == p.id && m_peers[i].address == p.address) {
            m_peers[i] = m_peers.back();
            m_peers.pop_back();
            break;
        }
    }
}

void Transport::event(uint8_t kind, int32_t id, int32_t value) {
    if (!m_handler) return;
    uint8_t e[12] = {kind};
    put32(e + 4, static_cast<uint32_t>(id));
    put32(e + 8, static_cast<uint32_t>(value));
    m_handler(e, sizeof e, 0);
}

void Transport::searchFrame(float dt) {
    m_searchTimer = dt + m_searchTimer;
    if (1.0f < m_searchTimer) {
        m_searchTimer = 0.0f;
        uint8_t q[9] = {kSearch};
        std::memcpy(q + 1, m_searchId.data(), 8);
        broadcast(q, sizeof q);
    }
    if (dt <= 2.0f) {
        for (size_t i = 0; i < m_found.size();) {
            m_found[i].age = dt + m_found[i].age;
            if (m_found[i].age <= 4.0f) {
                ++i;
            } else {
                m_found[i] = m_found.back();
                m_found.pop_back();
            }
        }
    } else {
        for (FoundSession& f : m_found) f.age = 0.0f;
    }
}

void Transport::peersFrame(float dt) {
    if (dt <= 2.0f) {
        for (size_t i = 0; i < m_peers.size();) {
            m_peers[i].silence = dt + m_peers[i].silence;
            if (m_peers[i].silence <= 25.0f) {
                ++i;
            } else {
                removePeer(i, true);
            }
        }
    } else {
        for (Peer& p : m_peers) p.silence = 0.0f;
    }
}

void Transport::pingFrame(float dt) {
    m_pingTimer = dt + m_pingTimer;
    if (5.0f < m_pingTimer) {
        ++m_pingNumber;
        m_pingTimer = 0.0f;
        uint8_t q[8] = {kPing};
        put32(q + 4, m_pingNumber);
        send(-2, q, sizeof q, false);
    }
}

void Transport::queueFrame(float dt) {
    for (size_t i = 0; i < m_queue.size();) {
        Queued& q = m_queue[i];
        q.timer = dt + q.timer;
        if (q.timer <= 2.0f) {
            ++i;
            continue;
        }
        q.timer = 0.0f;
        sendTo(q.to, q.bytes.data(), q.bytes.size());
        ++q.tries;
        if (q.tries < 30) {
            ++i;
        } else {
            m_queue[i] = std::move(m_queue.back());
            m_queue.pop_back();
        }
    }
}

void Transport::takenFrame(float dt) {
    for (size_t i = 0; i < m_taken.size();) {
        m_taken[i].age = dt + m_taken[i].age;
        if (m_taken[i].age <= 10.0f) {
            ++i;
        } else {
            m_taken[i] = m_taken.back();
            m_taken.pop_back();
        }
    }
}

namespace {

bool g_winsockStarted = false;  // [0x0075F754]
bool g_networkLost = false;     // [0x0075F74C]

}  // namespace

bool startWinsock() {
    if (g_winsockStarted) {
        WSACleanup();
        g_winsockStarted = false;
    }
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    g_winsockStarted = true;
    g_networkLost = false;
    return true;
}

void stopWinsock() {
    if (!g_winsockStarted) return;
    WSACleanup();
    g_winsockStarted = false;
}

bool winsockStarted() {
    return g_winsockStarted;
}

bool& networkLost() {
    return g_networkLost;
}

bool networkDown() {
    return g_networkLost;
}

}  // namespace oj::net
