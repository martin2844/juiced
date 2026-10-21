#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Core/Network/GameSpy/GameSpyQr2.h"

#include "Core/Base/Random.h"

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <format>

namespace oj::net {

namespace {

// The registered keys' names (0x0071ADC0) as the stock table starts.
std::array<std::string, kQr2MaxKeys>& keyNames() {
    static std::array<std::string, kQr2MaxKeys> names = [] {
        std::array<std::string, kQr2MaxKeys> n;
        static const char* const kDefaults[] = {
            "",          "hostname",    "gamename",   "gamever",     "hostport",  "mapname",
            "gametype",  "gamevariant", "numplayers", "numteams",    "maxplayers", "gamemode",
            "teamplay",  "fraglimit",   "teamfraglimit", "timeelapsed", "timelimit", "roundtime",
            "roundelapsed", "password", "groupid",    "player_",     "score_",    "skill_",
            "ping_",     "team_",       "deaths_",    "pid_",        "team_t",    "score_t",
        };
        for (size_t i = 0; i < std::size(kDefaults); ++i) n[i] = kDefaults[i];
        return n;
    }();
    return names;
}

std::array<bool, kQr2MaxKeys>& keySet() {
    static std::array<bool, kQr2MaxKeys> set = [] {
        std::array<bool, kQr2MaxKeys> s{};
        for (int i = 0; i < 30; ++i) s[static_cast<size_t>(i)] = true;
        return s;
    }();
    return set;
}

// [0x0075F4C0] / [0x0075B5D4]: the local addresses, found once a process (five at most).
std::vector<uint32_t>& localIps() {
    static std::vector<uint32_t> ips;
    return ips;
}

constexpr uint8_t kNatNegMagic[6] = {0xFD, 0xFC, 0x1E, 0x66, 0x6A, 0xB2};

}  // namespace

const char* qr2KeyName(int key) {
    if (key < 0 || key >= kQr2MaxKeys || !keySet()[static_cast<size_t>(key)]) return nullptr;
    return keyNames()[static_cast<size_t>(key)].c_str();
}

void qr2RegisterKey(int key, std::string_view name) {
    if (key < 50 || key > kQr2MaxKeys) return;
    // Stock problem: the macro checks 254 itself, one past the table; it is left out here.
    if (key == kQr2MaxKeys) return;
    keyNames()[static_cast<size_t>(key)] = name;
    keySet()[static_cast<size_t>(key)] = true;
}

// ---- The buffers.

void Qr2Buffer::add(std::string_view text) {
    const std::string_view until = text.substr(0, std::min(text.find('\0'), text.size()));
    int n = static_cast<int>(until.size()) + 1;
    const int room = 0x800 - length;
    if (room < n) n = room;
    if (n <= 0) return;
    for (int i = 0; i < n - 1; ++i) bytes[static_cast<size_t>(length + i)] = static_cast<uint8_t>(until[i]);
    length += n;
    bytes[static_cast<size_t>(length - 1)] = 0;
}

void Qr2Buffer::addInt(int value) {
    add(std::format("{}", value));
}

bool Qr2KeyBuffer::add(int key) {
    if (count >= kQr2MaxKeys || key < 1 || key > kQr2MaxKeys) return false;
    keys[static_cast<size_t>(count++)] = static_cast<uint8_t>(key);
    return true;
}

// ---- The record.

Qr2& qr2Record() {
    static Qr2 record;
    return record;
}

int Qr2::initSocket(const GameSocket& socket, int port, std::string_view gameName, std::string_view secretKey,
                    bool isPublic, bool natNegotiate, ServerKeyCallback serverKey, PlayerKeyCallback playerKey,
                    PlayerKeyCallback teamKey, KeyListCallback keyList, CountCallback count,
                    AddErrorCallback addError) {
    // srand(GetTickCount()): the game's rand() goes on from here.
    CrtRandom& random = gameRandom();
    random.seed(GetTickCount());
    m_gameName = gameName;
    m_secretKey = secretKey;
    m_port = port;
    m_socket = socket;
    m_serverKey = std::move(serverKey);
    m_playerKey = std::move(playerKey);
    m_teamKey = std::move(teamKey);
    m_keyList = std::move(keyList);
    m_count = std::move(count);
    m_addError = std::move(addError);
    m_lastHeartbeat = 0;
    m_lastKeepAlive = 0;
    m_listedState = 1;
    natNegotiateCallback = nullptr;
    clientMessageCallback = nullptr;
    cdKeyCallback = nullptr;
    m_isPublic = isPublic;
    m_readSocket = false;
    m_natNegotiate = natNegotiate;
    m_publicIp = 0;
    m_publicPort = 0;
    publicAddressCallback = nullptr;
    m_stateChangeRequested = false;
    for (int i = 0; i < 4; ++i) m_instanceKey[static_cast<size_t>(i)] = static_cast<uint8_t>(random.next() % 0xFF);
    m_clientMessageKeys.fill(0xFFFFFFFFu);
    m_clientMessageIndex = 0;
    if (localIps().empty()) {
        char name[256] = {};
        gethostname(name, sizeof name);
        if (const hostent* h = gethostbyname(name); h != nullptr) {
            for (int i = 0; i < 5 && h->h_addr_list[i] != nullptr; ++i) {
                uint32_t ip = 0;
                std::memcpy(&ip, h->h_addr_list[i], 4);
                localIps().push_back(ip);
            }
        }
    }
    if (isPublic) {
        const ServiceHosts& hosts = serviceHosts();
        const std::string host =
            hosts.master.empty() ? std::format("{}.master.gamespy.com", gameName) : hosts.master;
        m_master.port = htons(hosts.masterPort);
        m_master.ip = inet_addr(host.c_str());
        if (m_master.ip == INADDR_NONE && host != "255.255.255.255") {
            const hostent* h = gethostbyname(host.c_str());
            if (h == nullptr) return kQr2DnsError;
            std::memcpy(&m_master.ip, h->h_addr_list[0], 4);
        }
    }
    return kQr2NoError;
}

void Qr2::think() {
    if (m_isPublic) checkSendHeartbeat();
    checkQueries();
}

void Qr2::checkSendHeartbeat() {
    const uint32_t now = GetTickCount();
    if (!m_socket.valid()) return;
    if (m_listedState > 0 && now - m_lastHeartbeat > 10000) {
        if (m_listedState >= 4) {
            m_listedState = 0;
            if (m_addError) m_addError(kQr2NoChallengeError, u"No challenge value was received from the master server.");
            return;
        }
        sendHeartbeat(3);
        ++m_listedState;
    } else if ((m_stateChangeRequested && now - m_lastHeartbeat > 10000) || now - m_lastHeartbeat > 60000 ||
               m_lastHeartbeat == 0 || now < m_lastHeartbeat) {
        sendHeartbeat(m_stateChangeRequested && now - m_lastHeartbeat > 10000 ? 1 : 0);
    }
    if (now - m_lastKeepAlive > 20000) sendKeepAlive();
}

void Qr2::checkQueries() {
    // Only a socket of qr2's own is read (+0xC4); none in this build.
    if (!m_readSocket) return;
}

void Qr2::sendStateChanged() {
    if (!m_isPublic) return;
    if (GetTickCount() - m_lastHeartbeat < 10000) {
        m_stateChangeRequested = true;
        return;
    }
    sendHeartbeat(1);
    m_stateChangeRequested = false;
}

void Qr2::shutdown() {
    if (m_isPublic) sendHeartbeat(2);
    m_socket = GameSocket{};
    m_lastHeartbeat = 0;
}

void Qr2::send(const Address& to, const Qr2Buffer& b) {
    if (m_socket.valid()) m_socket.send(to, b.bytes.data(), static_cast<size_t>(b.length));
}

void Qr2::sendHeartbeat(int stateChanged) {
    Qr2Buffer b;
    b.bytes[0] = 0x03;
    std::memcpy(&b.bytes[1], m_instanceKey.data(), 4);
    b.length = 5;
    for (size_t i = 0; i < localIps().size(); ++i) {
        b.add(std::format("localip{}", i));
        in_addr a{};
        a.s_addr = localIps()[i];
        b.add(inet_ntoa(a));
    }
    b.add("localport");
    b.addInt(m_port);
    b.add("natneg");
    b.add(m_natNegotiate ? "1" : "0");
    if (stateChanged != 0) {
        b.add("statechanged");
        b.addInt(stateChanged);
    }
    b.add("gamename");
    b.add(m_gameName);
    if (publicAddressCallback) {
        b.add("publicip");
        b.addInt(static_cast<int32_t>(m_publicIp));
        b.add("publicport");
        b.addInt(m_publicPort);
    }
    if (stateChanged == 2) {
        if (0x800 - b.length >= 1) b.bytes[static_cast<size_t>(b.length++)] = 0;
    } else {
        buildKeySection(b, kQr2ServerKey, 0xFF, nullptr);
        buildKeySection(b, kQr2PlayerKey, 0xFF, nullptr);
        buildKeySection(b, kQr2TeamKey, 0xFF, nullptr);
    }
    send(m_master, b);
    const uint32_t now = GetTickCount();
    m_lastHeartbeat = now;
    m_lastKeepAlive = now;
    if (stateChanged != 0) m_stateChangeRequested = false;
}

void Qr2::sendKeepAlive() {
    Qr2Buffer b;
    b.bytes[0] = 0x08;
    std::memcpy(&b.bytes[1], m_instanceKey.data(), 4);
    b.length = 5;
    send(m_master, b);
    m_lastKeepAlive = GetTickCount();
}

void Qr2::buildKeySection(Qr2Buffer& b, int type, int numKeys, const uint8_t* keys) {
    if (numKeys == 0) return;
    int count = 1;
    if (type == kQr2PlayerKey || type == kQr2TeamKey) {
        if (0x800 - b.length < 2) return;
        count = m_count ? m_count(type) : 0;
        b.bytes[static_cast<size_t>(b.length)] = static_cast<uint8_t>(count >> 8);
        b.bytes[static_cast<size_t>(b.length + 1)] = static_cast<uint8_t>(count);
        b.length += 2;
    }
    Qr2KeyBuffer all;
    if (numKeys == 0xFF) {
        if (m_keyList) m_keyList(type, all);
        for (int i = 0; i < all.count; ++i) {
            const char* name = qr2KeyName(all.keys[static_cast<size_t>(i)]);
            b.add(name != nullptr ? name : "unknown");
            if (type == kQr2ServerKey) {
                const int before = b.length;
                if (m_serverKey) m_serverKey(all.keys[static_cast<size_t>(i)], b);
                if (before == b.length) b.add("");
            }
        }
        if (0x800 - b.length < 1) return;
        b.bytes[static_cast<size_t>(b.length++)] = 0;
        if (type == kQr2ServerKey) return;
        keys = all.keys.data();
        numKeys = all.count;
    }
    for (int index = 0; index < count; ++index) {
        for (int k = 0; k < numKeys; ++k) {
            const int before = b.length;
            if (type == kQr2ServerKey) {
                if (m_serverKey) m_serverKey(keys[k], b);
            } else if (type == kQr2PlayerKey) {
                if (m_playerKey) m_playerKey(keys[k], index, b);
            } else if (type == kQr2TeamKey) {
                if (m_teamKey) m_teamKey(keys[k], index, b);
            }
            if (before == b.length) b.add("");
        }
    }
}

void Qr2::parseQueryRequest(const uint8_t* data, int size, Qr2Buffer& reply) {
    if (size < 3) return;
    const uint8_t* serverKeys = nullptr;
    const uint8_t* playerKeys = nullptr;
    const uint8_t* teamKeys = nullptr;
    const uint8_t numServer = *data++;
    --size;
    if (numServer != 0 && numServer != 0xFF) {
        serverKeys = data;
        data += numServer;
        size -= numServer;
    }
    if (size < 2) return;
    const uint8_t numPlayer = *data++;
    --size;
    if (numPlayer != 0 && numPlayer != 0xFF) {
        playerKeys = data;
        data += numPlayer;
        size -= numPlayer;
    }
    if (size < 1) return;
    const uint8_t numTeam = *data;
    --size;
    if (numTeam != 0 && numTeam != 0xFF) {
        teamKeys = data + 1;
        size -= numTeam;
    }
    if (size < 0) return;
    buildKeySection(reply, kQr2ServerKey, numServer, serverKeys);
    buildKeySection(reply, kQr2PlayerKey, numPlayer, playerKeys);
    buildKeySection(reply, kQr2TeamKey, numTeam, teamKeys);
}

void Qr2::legacySection(Qr2Buffer& b, int type) {
    int count = 1;
    if (type == kQr2PlayerKey || type == kQr2TeamKey) count = m_count ? m_count(type) : 0;
    Qr2KeyBuffer all;
    if (m_keyList) m_keyList(type, all);
    auto slash = [&b] { b.bytes[static_cast<size_t>(b.length - 1)] = '\\'; };
    for (int i = 0; i < all.count; ++i) {
        const int key = all.keys[static_cast<size_t>(i)];
        const char* name = qr2KeyName(key);
        if (name == nullptr) name = "unknown";
        if (type == kQr2ServerKey) {
            b.add(name);
            slash();
            const int before = b.length;
            if (m_serverKey) m_serverKey(key, b);
            if (before == b.length) b.add("");
            slash();
        } else {
            for (int index = 0; index < count; ++index) {
                b.add(std::format("{}{}", name, index));
                slash();
                const int before = b.length;
                if (type == kQr2PlayerKey) {
                    if (m_playerKey) m_playerKey(key, index, b);
                } else if (m_teamKey) {
                    m_teamKey(key, index, b);
                }
                if (before == b.length) b.add("");
                slash();
            }
        }
    }
}

void Qr2::buildLegacyReply(Qr2Buffer& b) {
    b.length = 1;
    b.bytes[0] = '\\';
    legacySection(b, kQr2ServerKey);
    legacySection(b, kQr2PlayerKey);
    legacySection(b, kQr2TeamKey);
    b.add("final\\\\queryid\\1.1");
    b.length -= 1;
}

void Qr2::challengeResponse(const uint8_t* challenge, int size, Qr2Buffer& reply) {
    if (size < 1 || size > 0x41 || challenge[size - 1] != 0) return;
    // strcpy of the challenge, then size - 1 bytes encrypted and encoded (past a 0 inside, what the copy left:
    // zeros here).
    uint8_t copy[0x44] = {};
    for (int i = 0; i < size && challenge[i] != 0; ++i) copy[i] = challenge[i];
    gsEncrypt(m_secretKey, copy, static_cast<size_t>(size - 1));
    reply.add(gsEncode(copy, static_cast<size_t>(size - 1)));
}

void Qr2::parsePublicAddress(const char* text) {
    unsigned int ip = 0;
    unsigned int port = 0;
    sscanf_s(text, "%08X%04X", &ip, &port);
    const uint32_t netIp = htonl(ip);
    if (netIp == 0 || static_cast<uint16_t>(port) == 0) return;
    if (m_publicIp == netIp && m_publicPort == static_cast<uint16_t>(port)) return;
    m_publicIp = netIp;
    m_publicPort = static_cast<uint16_t>(port);
    publicAddressCallback(netIp, static_cast<uint16_t>(port));
}

bool Qr2::hasClientMessageKey(uint32_t key) {
    for (uint32_t k : m_clientMessageKeys) {
        if (k == key) return true;
    }
    m_clientMessageIndex = (m_clientMessageIndex + 1) % 10;
    m_clientMessageKeys[static_cast<size_t>(m_clientMessageIndex)] = key;
    return false;
}

void Qr2::processClientMessage(const uint8_t* message, int size) {
    if (size >= 10 && std::memcmp(message, kNatNegMagic, 6) == 0) {
        if (natNegotiateCallback) {
            const uint32_t cookie = (static_cast<uint32_t>(message[6]) << 24) |
                                    (static_cast<uint32_t>(message[7]) << 16) |
                                    (static_cast<uint32_t>(message[8]) << 8) | message[9];
            natNegotiateCallback(static_cast<int32_t>(cookie));
        }
        return;
    }
    if (clientMessageCallback) {
        const std::string text(reinterpret_cast<const char*>(message), size > 0 ? static_cast<size_t>(size) : 0);
        clientMessageCallback(gsUtf8ToUcs2(text));
    }
}

void Qr2::parseQuery(const uint8_t* data, size_t size, const Address& from) {
    if (size == 0) return;
    if (data[0] == ';') {
        if (cdKeyCallback) cdKeyCallback(data, size, from);
        return;
    }
    if (data[0] == '\\') {
        Qr2Buffer reply;
        buildLegacyReply(reply);
        send(from, reply);
        return;
    }
    if (size < 7 || data[0] != 0xFE || data[1] != 0xFD) return;
    if (m_listedState > 0) m_listedState = 0;
    Qr2Buffer reply;
    reply.bytes[0] = data[2];
    std::memcpy(&reply.bytes[1], data + 3, 4);
    reply.length = 5;
    const uint8_t* payload = data + 7;
    int length = static_cast<int>(size) - 7;
    auto ourKey = [&] { return std::memcmp(data + 3, m_instanceKey.data(), 4) == 0; };
    switch (data[2]) {
    case 0x00:  // query
        parseQueryRequest(payload, length, reply);
        break;
    case 0x01:  // challenge
        if (length >= 13 && publicAddressCallback) {
            const std::string tail(reinterpret_cast<const char*>(payload + length - 13), 13);
            parsePublicAddress(tail.c_str());
        }
        challengeResponse(payload, length, reply);
        break;
    case 0x02: {  // echo
        const int n = std::min(length, 0x20);
        reply.bytes[0] = 0x05;
        std::memcpy(&reply.bytes[5], payload, static_cast<size_t>(n));
        reply.length += n;
        break;
    }
    case 0x04: {  // add error
        if (m_listedState == -1 || !ourKey() || length < 2) return;
        m_listedState = -1;
        const std::string text(reinterpret_cast<const char*>(payload + 1), static_cast<size_t>(length - 1));
        if (m_addError) m_addError(static_cast<int8_t>(payload[0]), gsUtf8ToUcs2(text));
        return;
    }
    case 0x06: {  // client message
        if (!ourKey() || length < 4) return;
        uint32_t key = 0;
        std::memcpy(&key, payload, 4);
        std::memcpy(&reply.bytes[5], payload, 4);
        reply.bytes[0] = 0x07;
        reply.length = 9;
        if (!hasClientMessageKey(key)) processClientMessage(payload + 4, length - 4);
        break;
    }
    default:
        return;
    }
    send(from, reply);
}

}  // namespace oj::net
