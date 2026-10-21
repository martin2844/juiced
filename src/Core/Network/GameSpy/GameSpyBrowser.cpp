#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Core/Network/GameSpy/GameSpyBrowser.h"

#include "Core/Network/GameSpy/GameSpyQr2.h"
#include "Core/Base/Random.h"

#include <winsock2.h>
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <format>

namespace oj::net {

namespace {

constexpr int kInSize = 0x1000;
// Zeros past the list's buffer: the stock's reads past a message (a push not checked) stay in its 0x1000 bytes or run
// into what follows them; here they stop on these.
constexpr int kInPad = 0x1000;

uint32_t raw32(const uint8_t* p) {
    uint32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

uint16_t raw16(const uint8_t* p) {
    uint16_t v = 0;
    std::memcpy(&v, p, 2);
    return v;
}

// The SDK's check of a string within `size` bytes: its length with the 0, -1 when the 0 is not there.
int stringWithin(const uint8_t* p, int size) {
    if (size < 1) return -1;
    for (int i = 0; i < size; ++i) {
        if (p[i] == 0) return i + 1;
    }
    return -1;
}

const char* text(const uint8_t* p) {
    return reinterpret_cast<const char*>(p);
}

bool sameKey(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// 0x0056A2B0 (mytok): the next token up to `delim` (nullptr when it is empty), the delimiter made 0.
char* g_tokenAt = nullptr;
char* token(char* start, char delim) {
    if (start != nullptr) g_tokenAt = start;
    char* begin = g_tokenAt;
    char* result = nullptr;
    if (*g_tokenAt != 0) {
        while (*g_tokenAt != delim && *g_tokenAt != 0) ++g_tokenAt;
        if (g_tokenAt != begin) result = begin;
    }
    if (*g_tokenAt != 0) {
        *g_tokenAt = 0;
        ++g_tokenAt;
    }
    return result;
}

// 0x0056A300: \key\value pairs of a GameSpy 1 answer (queryid and final left out, 0x0056A240).
void parseGs1Keys(SbServer& server, char* keyValues) {
    char* key = token(keyValues + 1, '\\');
    while (key != nullptr) {
        char* value = token(nullptr, '\\');
        if (value == nullptr) value = const_cast<char*>("");
        if (std::strcmp(key, "queryid") != 0 && std::strcmp(key, "final") != 0) server.addKeyValue(key, value);
        key = token(nullptr, '\\');
    }
}

// 0x0056A3E0: a full QR2 answer: name\0value\0 .. \0, then the players and the teams: u16 BE count, the keys' names
// (100 bytes each at most) and a 0, then each one's values (as "<name><index>").
void parseFullKeys(SbServer& server, const uint8_t* p, int size) {
    while (*p != 0) {
        const int nameLength = stringWithin(p, size);
        if (nameLength < 0) return;
        size -= nameLength;
        const int valueLength = stringWithin(p + nameLength, size);
        if (valueLength < 0) return;
        size -= valueLength;
        server.addKeyValue(text(p), text(p + nameLength));
        p += nameLength + valueLength;
    }
    ++p;
    --size;
    for (int section = 0; section < 2; ++section) {
        if (size < 2) return;
        const int count = (p[0] << 8) | p[1];
        p += 2;
        size -= 2;
        const uint8_t* names = p;
        int numNames = 0;
        while (*p != 0) {
            const int length = stringWithin(p, size);
            if (length < 0 || length > 100) return;
            ++numNames;
            p += length;
            size -= length;
        }
        --size;
        ++p;
        for (int index = 0; index < count; ++index) {
            const uint8_t* name = names;
            for (int k = 0; k < numNames; ++k) {
                const int length = stringWithin(p, size);
                if (length < 0) return;
                server.addKeyValue(std::format("{}{}", text(name), index), text(p));
                p += length;
                size -= length;
                name += std::strlen(text(name)) + 1;
            }
        }
    }
}

}  // namespace

// ---- The servers.

const std::string* SbServer::find(std::string_view key) const {
    for (const auto& [k, v] : keys) {
        if (sameKey(k, key)) return &v;
    }
    return nullptr;
}

void SbServer::addKeyValue(std::string_view key, std::string_view value) {
    for (auto& [k, v] : keys) {
        if (sameKey(k, key)) {
            k = key;
            v = value;
            return;
        }
    }
    keys.emplace_back(std::string(key), std::string(value));
}

void SbServer::addIntKeyValue(std::string_view key, int value) {
    addKeyValue(key, std::format("{}", value));
}

std::string SbServer::stringValue(std::string_view key, std::string_view fallback) const {
    const std::string* v = find(key);
    return v != nullptr ? *v : std::string(fallback);
}

int SbServer::intValue(std::string_view key, int fallback) const {
    if (key == "ping") return static_cast<int>(updateTime);
    const std::string* v = find(key);
    if (v != nullptr && !v->empty() && std::isdigit(static_cast<unsigned char>((*v)[0]))) {
        return static_cast<int>(std::atol(v->c_str()));
    }
    return fallback;
}

// ---- The browser.

std::unique_ptr<ServerBrowser> ServerBrowser::create(std::string_view queryForGame, std::string_view queryFromGame,
                                                     std::string_view secretKey, int queryFromVersion,
                                                     int maxConcurrentUpdates, int queryVersion, Callback callback) {
    if (availableResult() != AvailableCheck::Available) return nullptr;
    std::unique_ptr<ServerBrowser> sb(new ServerBrowser());
    sb->m_callback = std::move(callback);
    sb->m_dontUpdate = false;
    // SBServerListInit (0x0057C800).
    sb->m_state = 1;
    sb->m_queryForGame = queryForGame;
    sb->m_queryFromGame = queryFromGame;
    sb->m_queryFromKey = secretKey;
    sb->m_myPublicIp = 0;
    sb->m_in.clear();
    sb->m_inLength = 0;
    sb->m_hasKeyList = false;
    sb->m_expected = -1;
    sb->m_popular.clear();
    sb->m_alternateIp = 0;
    sb->m_error.clear();
    sb->m_queryFromVersion = queryFromVersion;
    gameRandom().seed(GetTickCount());
    // SBQueryEngineInit (0x0057E720).
    sb->m_queryVersion = queryVersion;
    sb->m_maxUpdates = maxConcurrentUpdates;
    sb->m_numBasicKeys = 0;
    sb->m_engineIp = 0;
    sb->m_querySocket.open();
    return sb;
}

ServerBrowser::~ServerBrowser() {
    disconnect();
    clearServers();
    m_querySocket.close();
    m_queryList = {};
    m_pendingList = {};
}

SbServer* ServerBrowser::getServer(int index) const {
    if (index < 0 || index >= count()) return nullptr;
    return m_servers[static_cast<size_t>(index)].get();
}

void ServerBrowser::freeDeadList() {
    m_dead.clear();
}

void ServerBrowser::clearServers() {
    for (auto& s : m_servers) m_dead.push_back(std::move(s));
    m_servers.clear();
    freeDeadList();
}

int ServerBrowser::findServer(uint32_t ip, uint16_t port) const {
    for (size_t i = 0; i < m_servers.size(); ++i) {
        if (m_servers[i]->publicIp == ip && m_servers[i]->publicPort == port) return static_cast<int>(i);
    }
    return -1;
}

void ServerBrowser::addServer(std::unique_ptr<SbServer> server) {
    SbServer* s = server.get();
    m_servers.push_back(std::move(server));
    listCallback(0, s);
}

void ServerBrowser::removeServer(int index) {
    SbServer* s = m_servers[static_cast<size_t>(index)].get();
    listCallback(2, s);
    m_dead.push_back(std::move(m_servers[static_cast<size_t>(index)]));
    m_servers.erase(m_servers.begin() + index);
}

void ServerBrowser::freeKeyList() {
    m_keyList.clear();
    m_hasKeyList = false;
}

void ServerBrowser::freePopularValues() {
    m_popular.clear();
}

void ServerBrowser::disconnect() {
    m_in.clear();
    m_inLength = 0;
    m_socket.close();
    m_state = 1;
    freeKeyList();
    m_expected = -1;
    freePopularValues();
}

void ServerBrowser::clear() {
    disconnect();
    m_pendingList = {};
    m_queryList = {};
    clearServers();
}

void ServerBrowser::halt() {
    disconnect();
    m_pendingList = {};
    m_queryList = {};
}

int ServerBrowser::connectToMaster() {
    const ServiceHosts& hosts = serviceHosts();
    const std::string host =
        hosts.list.empty() ? std::format("{}.ms{}.gamespy.com", m_queryForGame, masterHash(m_queryForGame) % 20)
                           : hosts.list;
    uint32_t ip = inet_addr(host.c_str());
    if (ip == INADDR_NONE) {
        const hostent* h = gethostbyname(host.c_str());
        if (h == nullptr) return kDnsError;
        std::memcpy(&ip, h->h_addr_list[0], 4);
    }
    // A socket still there is connected already: connect() fails on it (closed then).
    if (m_socket.opened()) {
        m_socket.close();
        return kConnectError;
    }
    if (!m_socket.connectBlocking(ip, hosts.listPort)) return kConnectError;
    return kNoError;
}

void ServerBrowser::makeChallenge() {
    CrtRandom& random = gameRandom();
    uint8_t* c = m_challenge.data();
    c[0] = static_cast<uint8_t>(random.next() % 0x5D + 0x21);
    uint32_t parity = 0;
    for (uint32_t i = 1; i < 8; ++i) {
        parity ^= ((i ^ c[i - 1] ^ c[0]) & 1) ^ (static_cast<int8_t>(c[i - 1]) < static_cast<int8_t>(c[0]) ? 1u : 0u) ^
                  (static_cast<int8_t>(c[0]) < 'O' ? 1u : 0u);
        const int v = random.next() % 0x5D;
        c[i] = static_cast<uint8_t>(v + 0x21);
        if (parity == 0) {
            if ((c[i] & 1) != 0) c[i] = static_cast<uint8_t>(v + 0x22);
        } else if ((c[i] & 1) == 0) {
            c[i] = static_cast<uint8_t>(v + 0x22);
        }
    }
}

int ServerBrowser::connectAndQuery(const char* fieldList, const char* serverFilter, int options, int maxServers) {
    if (fieldList == nullptr) fieldList = "";
    if (serverFilter == nullptr) serverFilter = "";
    if (std::strlen(fieldList) > 0x100 || std::strlen(serverFilter) > 0x100) return kParamError;
    if (const int error = connectToMaster(); error != kNoError) return error;
    m_options = options;
    makeChallenge();
    std::vector<uint8_t> request(2, 0);
    request.push_back(0x00);  // SERVER_LIST_REQUEST
    request.push_back(0x01);  // the list's protocol
    request.push_back(0x03);  // its encoding
    const auto putRaw = [&request](uint32_t v) {
        const auto* b = reinterpret_cast<const uint8_t*>(&v);
        request.insert(request.end(), b, b + 4);
    };
    const auto putString = [&request](const std::string& s) {
        request.insert(request.end(), s.begin(), s.end());
        request.push_back(0);
    };
    putRaw(static_cast<uint32_t>(m_queryFromVersion));
    putString(m_queryForGame);
    putString(m_queryFromGame);
    request.insert(request.end(), m_challenge.begin(), m_challenge.end());
    putString(serverFilter);
    putString(fieldList);
    putRaw(htonl(static_cast<uint32_t>(options)));
    if ((options & kAlternateSource) != 0) putRaw(m_alternateIp);
    // Stock problem: the most servers goes as it lies in memory, not in network order.
    if ((options & kLimitResultCount) != 0) putRaw(static_cast<uint32_t>(maxServers));
    request[0] = static_cast<uint8_t>(request.size() >> 8);
    request[1] = static_cast<uint8_t>(request.size());
    m_socket.send(request.data(), request.size());
    if (!m_socket.connected()) {
        disconnect();
        return kConnectError;
    }
    m_state = 3;
    m_parseState = 0;
    if (m_in.empty()) {
        m_in.assign(kInSize + kInPad, 0);
        m_inLength = 0;
    }
    return kNoError;
}

int ServerBrowser::sendList(const uint8_t* data, int size) {
    int tries = 1;
    for (;;) {
        --tries;
        m_socket.send(data, static_cast<size_t>(size));
        if (m_socket.connected()) return kNoError;
        if (tries < 0) break;
        // Lost: disconnected, then connected again for messages only (NO_SERVER_LIST).
        disconnect();
        const int error = connectAndQuery(nullptr, nullptr, kNoServerList, 0);
        if (error != kNoError) {
            queryError();
            return error;
        }
    }
    return kConnectError;
}

int ServerBrowser::serverInfoRequest(uint32_t ip, uint16_t port) {
    if (m_state == 1) {
        connectAndQuery(nullptr, nullptr, kNoServerList, 0);
        if (m_state == 1) return kConnectError;
    }
    uint8_t request[9] = {0x00, 0x09, 0x01};
    std::memcpy(request + 3, &ip, 4);
    std::memcpy(request + 7, &port, 2);
    return sendList(request, sizeof request);
}

int ServerBrowser::sendMessageToServer(uint32_t ip, uint16_t port, const uint8_t* data, int size) {
    if (m_state == 1) {
        connectAndQuery(nullptr, nullptr, kNoServerList, 0);
        if (m_state == 1) return kConnectError;
    }
    uint8_t request[9];
    request[0] = static_cast<uint8_t>((size + 9) >> 8);
    request[1] = static_cast<uint8_t>(size + 9);
    request[2] = 0x02;
    std::memcpy(request + 3, &ip, 4);
    std::memcpy(request + 7, &port, 2);
    int error = sendList(request, sizeof request);
    if (error == kNoError) {
        m_socket.send(data, static_cast<size_t>(size));
        error = m_socket.connected() ? kNoError : kConnectError;
    }
    return error;
}

int ServerBrowser::sendNatNegotiateCookieToServer(const char* ip, uint16_t port, int32_t cookie) {
    uint8_t message[10] = {0xFD, 0xFC, 0x1E, 0x66, 0x6A, 0xB2};
    const uint32_t c = htonl(static_cast<uint32_t>(cookie));
    std::memcpy(message + 6, &c, 4);
    return sendMessageToServer(inet_addr(ip), htons(port), message, sizeof message);
}

void ServerBrowser::queryError() {
    static const char kPrefix[] = "Query Error: ";  // 0x006FC038
    const int length = static_cast<int>(std::strlen(kPrefix));
    if (m_inLength > 0 && m_inLength > length && !m_in.empty() &&
        std::strncmp(text(m_in.data()), kPrefix, static_cast<size_t>(length)) == 0) {
        m_error = text(m_in.data() + length);
        listCallback(5, nullptr);
    }
    listCallback(4, nullptr);
    disconnect();
}

int ServerBrowser::readList() {
    if (!m_socket.opened()) return kNoError;
    m_socket.poll();
    std::vector<uint8_t>& got = m_socket.received();
    if (got.empty()) {
        if (!m_socket.lost()) return kNoError;
        queryError();  // recv gave 0 (closed) or failed
        return kConnectError;
    }
    // Stock problem: with the buffer full the recv asks for 0 bytes, gets 0 and takes it as the connection closed.
    const int room = kInSize - m_inLength;
    const int n = std::min(static_cast<int>(got.size()), room);
    if (n <= 0) {
        queryError();
        return kConnectError;
    }
    std::memcpy(m_in.data() + m_inLength, got.data(), static_cast<size_t>(n));
    got.erase(got.begin(), got.begin() + n);
    const int before = m_inLength;
    m_inLength += n;
    if (m_state == 2 || m_parseState > 0) m_crypt.decrypt(m_in.data() + before, static_cast<size_t>(n));
    if (m_state == 3) {
        if (const int error = parseList(); error != kNoError) return error;
    }
    if (m_state == 2 && m_inLength > 0) return processMessages();
    return kNoError;
}

int ServerBrowser::parseList() {
    uint8_t* p = m_in.data();
    int size = m_inLength;
    const auto keep = [&]() {
        if (!m_in.empty()) {
            if (size != 0) std::memmove(m_in.data(), p, static_cast<size_t>(size));
            m_inLength = size;
        }
        return kNoError;
    };
    switch (m_parseState) {
    case 0: {  // the crypt's header: [n ^ 0xEC] .. [key length ^ 0xEA] [the server's key]
        if (size < 1) return keep();
        const int header = (p[0] ^ 0xEC) + 2;
        if (size < header) return keep();
        const int keyLength = p[header - 1] ^ 0xEA;
        if (size < header + keyLength) return keep();
        listCryptInit(m_crypt, m_challenge, m_queryFromKey, p + header, static_cast<size_t>(keyLength));
        p += header + keyLength;
        size -= header + keyLength;
        m_parseState = 1;
        m_crypt.decrypt(p, static_cast<size_t>(size));
        [[fallthrough]];
    }
    case 1: {  // our public address, the default port
        if (size < 6) return keep();
        m_myPublicIp = raw32(p);
        listCallback(6, nullptr);
        m_defaultPort = raw16(p + 4);
        if (m_defaultPort == 0xFFFF) {
            if (stringWithin(p + 6, size - 6) < 0) return keep();
            m_error = text(p + 6);
            listCallback(5, nullptr);
            if (m_in.empty()) return keep();
        }
        p += 6;
        size -= 6;
        if ((m_options & kNoServerList) != 0 || m_defaultPort == 0xFFFF) {
            m_parseState = 5;
            m_state = 2;
            return keep();
        }
        m_parseState = 2;
        m_expected = -1;
        [[fallthrough]];
    }
    case 2: {  // the keys: [n] then [type][name\0] each
        if (m_expected == -1) {
            if (size < 1) return keep();
            m_expected = p[0];
            m_keyList.clear();
            m_hasKeyList = true;
            ++p;
            --size;
        }
        while (static_cast<int>(m_keyList.size()) < m_expected && size > 1) {
            const int length = stringWithin(p + 1, size - 1);
            if (length < 0) break;
            m_keyList.push_back({text(p + 1), p[0]});
            p += length + 1;
            size -= length + 1;
        }
        if (static_cast<int>(m_keyList.size()) < m_expected) return keep();
        m_parseState = 3;
        m_expected = -1;
        [[fallthrough]];
    }
    case 3: {  // the popular values: [n] then string\0 each
        if (m_expected == -1) {
            if (size < 1) return keep();
            m_expected = p[0];
            ++p;
            --size;
            m_popular.clear();
        }
        while (static_cast<int>(m_popular.size()) < m_expected && size > 0) {
            const int length = stringWithin(p, size);
            if (length < 0) break;
            m_popular.emplace_back(text(p));
            p += length;
            size -= length;
        }
        if (static_cast<int>(m_popular.size()) < m_expected) return keep();
        m_parseState = 4;
        [[fallthrough]];
    }
    case 4: {  // the servers, until the one of the address FF FF FF FF
        if (size > 4) {
            for (;;) {
                const int used = parseListServer(p, size);
                if (used == -2) return kAllocError;
                if (used == -1) break;
                p += used;
                size -= used;
                if (m_in.empty() || used == 0) return keep();
            }
            size -= 5;
            p += 5;
            m_parseState = 5;
            m_state = 2;
            listCallback(3, nullptr);
        }
        return keep();
    }
    default:
        return keep();
    }
}

bool ServerBrowser::checkKeys(const uint8_t* p, int size) const {
    for (const ListKey& key : m_keyList) {
        if (key.type == 0) {
            if (size < 1) return false;
            const uint8_t index = *p++;
            --size;
            if (index == 0xFF) {
                const int length = stringWithin(p, size);
                if (length < 0) return false;
                p += length;
                size -= length;
            }
        } else if (key.type == 1) {
            ++p;
            --size;
        } else if (key.type == 2) {
            p += 2;
            size -= 2;
        } else {
            return false;
        }
        if (size < 0) return false;
    }
    return true;
}

namespace {

// 0x0057D0D0: name\0value\0 pairs within `size` up to a 0.
bool checkRules(const uint8_t* p, int size) {
    for (;;) {
        if (size < 1 || *p == 0) return size != 0 && *p == 0;
        const int nameLength = stringWithin(p, size);
        if (nameLength < 0) return false;
        size -= nameLength;
        const int valueLength = stringWithin(p + nameLength, size);
        if (valueLength < 0) return false;
        size -= valueLength;
        p += nameLength + valueLength;
    }
}

}  // namespace

int ServerBrowser::parseListServer(const uint8_t* data, int size) {
    if (size < 1) return 0;
    const uint8_t flags = data[0];
    int fixed = (flags & kSbPrivateIp) != 0 ? 9 : 5;
    if ((flags & kSbIcmpIp) != 0) fixed += 4;
    if ((flags & kSbNonStandardPort) != 0) fixed += 2;
    if ((flags & kSbNonStandardPrivatePort) != 0) fixed += 2;
    if (size < fixed) return 0;
    if ((flags & kSbHasKeys) != 0 && !checkKeys(data + fixed, size - fixed)) return 0;
    // Stock problem: the rules are checked from where the keys begin.
    if ((flags & kSbHasFullRules) != 0 && !checkRules(data + fixed, size - fixed)) return 0;
    if (raw32(data + 1) == 0xFFFFFFFFu) return -1;
    // 0x0057D1C0: the address and the port (the default one without NONSTANDARD_PORT).
    auto server = std::make_unique<SbServer>();
    server->publicIp = raw32(data + 1);
    server->publicPort = (flags & kSbNonStandardPort) != 0 ? raw16(data + 5) : m_defaultPort;
    const int used = parseServerData(*server, data, size, true);
    addServer(std::move(server));
    return used;
}

int ServerBrowser::parseServerData(SbServer& server, const uint8_t* data, int size, bool usePopular) {
    const uint8_t flags = data[0];
    const uint8_t* p = data + 5;
    int left = size - 5;
    server.flags = flags;
    if ((flags & kSbNonStandardPort) != 0) {
        p = data + 7;
        left = size - 7;
    }
    uint32_t privateIp = 0;
    if ((flags & kSbPrivateIp) != 0) {
        privateIp = raw32(p);
        p += 4;
        left -= 4;
    }
    uint16_t privatePort = m_defaultPort;
    if ((flags & kSbNonStandardPrivatePort) != 0) {
        privatePort = raw16(p);
        p += 2;
        left -= 2;
    }
    server.privateIp = privateIp;
    server.privatePort = privatePort;
    if ((flags & kSbIcmpIp) != 0) {
        server.icmpIp = raw32(p);
        p += 4;
        left -= 4;
    }
    if ((flags & kSbHasKeys) != 0) {
        for (const ListKey& key : m_keyList) {
            if (key.type == 0) {
                if (usePopular) {
                    const uint8_t index = *p++;
                    --left;
                    if (index != 0xFF) {
                        if (index < m_popular.size()) server.addKeyValue(key.name, m_popular[index]);
                        continue;
                    }
                }
                server.addKeyValue(key.name, text(p));
                const int length = static_cast<int>(std::strlen(text(p))) + 1;
                p += length;
                left -= length;
            } else if (key.type == 1) {
                server.addIntKeyValue(key.name, *p);
                ++p;
                --left;
            } else if (key.type == 2) {
                server.addIntKeyValue(key.name, (p[0] << 8) | p[1]);
                p += 2;
                left -= 2;
            }
        }
        server.state |= kSbStateBasicKeys;
    }
    if ((flags & kSbHasFullRules) != 0) {
        while (*p != 0 && left > 0) {
            const int nameLength = static_cast<int>(std::strlen(text(p))) + 1;
            const uint8_t* value = p + nameLength;
            const int valueLength = static_cast<int>(std::strlen(text(value))) + 1;
            server.addKeyValue(text(p), text(value));
            p = value + valueLength;
            left -= nameLength + valueLength;
        }
        --left;
        server.state |= kSbStateFullKeys;
    }
    return size - left;
}

int ServerBrowser::processMessages() {
    int error = kNoError;
    if (m_inLength < 3) return kNoError;
    for (;;) {
        const int length = (m_in[0] << 8) | m_in[1];
        if (length > 0x1000) {
            error = kDataError;
            break;
        }
        if (m_inLength < length) return kNoError;
        uint8_t* buffer = m_in.data();
        switch (buffer[2]) {
        case 1:  // PUSH_KEYS
            error = pushKeys(buffer + 3, length - 3);
            break;
        case 2:  // PUSH_SERVER
            error = pushServer(buffer + 3, length - 3);
            break;
        case 3:  // KEEPALIVE: sent back as it is
            m_socket.send(buffer, static_cast<size_t>(length));
            if (!m_socket.connected()) return kConnectError;
            break;
        case 4:  // DELETE_SERVER
            if (length - 3 < 6) {
                error = kDataError;
            } else {
                const int index = findServer(raw32(buffer + 3), raw16(buffer + 7));
                if (index != -1) removeServer(index);
                error = kNoError;
            }
            break;
        case 5:  // MAPLOOP
            error = mapLoop(buffer + 3, length - 3);
            break;
        case 6:  // PLAYERSEARCH
            error = playerSearch(buffer + 3, length - 3);
            break;
        default:
            break;
        }
        // Stock problem: a message of a length under 3 is never taken off (0: the loop runs for ever).
        m_inLength -= length;
        if (m_inLength != 0 && !m_in.empty()) {
            std::memmove(m_in.data(), m_in.data() + length, static_cast<size_t>(m_inLength));
        }
        if (error != kNoError) break;
        if (m_inLength < 3) return kNoError;
    }
    queryError();
    return error;
}

int ServerBrowser::pushKeys(const uint8_t* data, int size) {
    const int count = data[0];
    --size;
    const uint8_t* p = data + 1;
    freeKeyList();
    m_hasKeyList = true;
    for (int i = 0; i < count; ++i) {
        if (size < 2) return kDataError;
        const int length = stringWithin(p + 1, size - 1);
        if (length < 0) return kDataError;
        m_keyList.push_back({text(p + 1), p[0]});
        size -= length + 1;
        p += length + 1;
    }
    return kNoError;
}

int ServerBrowser::pushServer(const uint8_t* data, int size) {
    if (size < 5) return kDataError;
    const uint32_t ip = raw32(data + 1);
    uint16_t port = m_defaultPort;
    if ((data[0] & kSbNonStandardPort) != 0) {
        // Stock problem: too short for the port, the message's length is taken as it.
        port = size - 5 >= 2 ? raw16(data + 5) : static_cast<uint16_t>(size);
    }
    const int index = findServer(ip, port);
    std::unique_ptr<SbServer> made;
    SbServer* server = nullptr;
    if (index == -1) {
        made = std::make_unique<SbServer>();
        made->publicIp = ip;
        made->publicPort = port;
        server = made.get();
    } else {
        server = m_servers[static_cast<size_t>(index)].get();
    }
    if (parseServerData(*server, data, size, false) < 0) return kDataError;
    if (made) addServer(std::move(made));
    listCallback(1, server);
    return kNoError;
}

int ServerBrowser::mapLoop(const uint8_t* data, int size) {
    if (size < 11) return kDataError;
    const int index = findServer(raw32(data), raw16(data + 4));
    if (index == -1) return kNoError;
    SbServer* server = m_servers[static_cast<size_t>(index)].get();
    const uint32_t mapIndex = ntohl(raw32(data + 6));
    const int count = data[10];
    const uint8_t* p = data + 11;
    int left = size - 11;
    std::vector<std::string> maps;
    for (int i = 0; i < count; ++i) {
        if (i > 15 || left < 1) break;
        const int length = stringWithin(p, left);
        if (length < 0) return kDataError;
        maps.emplace_back(text(p));
        p += length;
        left -= length;
    }
    if (mapLoopCallback) mapLoopCallback(server, mapIndex, maps);
    return kNoError;
}

int ServerBrowser::playerSearch(const uint8_t* data, int size) {
    if (size < 2) return kDataError;
    const uint8_t more = data[0];
    const int count = data[1];
    const uint8_t* p = data + 2;
    int left = size - 2;
    for (int i = 0; i < count; ++i) {
        const int nickLength = stringWithin(p, left);
        if (nickLength < 0) return kDataError;
        const uint8_t* q = p + nickLength;
        if (left - nickLength < 11) return kDataError;
        const uint32_t ip = raw32(q);
        const uint16_t port = raw16(q + 4);
        const uint32_t extra = ntohl(raw32(q + 6));
        left = left - nickLength - 10;
        q += 10;
        const int dataLength = stringWithin(q, left);
        if (dataLength < 0) return kDataError;
        // Stock problem: the callback is called unchecked and never set (what the browser's memory held).
        if (playerSearchCallback) playerSearchCallback(text(p), ip, port, extra, text(q));
        p = q + dataLength;
        left -= dataLength;
    }
    if (more != 0 && playerSearchCallback) playerSearchCallback(nullptr, 0, 0, 0, nullptr);
    return kNoError;
}

int ServerBrowser::readLan() {
    // The LAN's answers (list state 0); nothing in this build sets the state 0 or opens the socket.
    for (;;) {
        uint8_t buffer[0x5DB];
        uint32_t ip = 0;
        uint16_t port = 0;
        if (m_lanSocket.receive(buffer, sizeof buffer, ip, port) < 0) {
            if (GetTickCount() - m_lanStart > 2000) {
                m_lanSocket.close();
                m_state = 1;
                listCallback(3, nullptr);
            }
            return kNoError;
        }
        if (findServer(ip, port) != -1) continue;
        auto server = std::make_unique<SbServer>();
        server->publicIp = ip;
        server->publicPort = port;
        server->flags = kSbUnsolicitedUdp | kSbNonStandardPort;
        addServer(std::move(server));
    }
}

// ---- The query engine.

bool ServerBrowser::removeFrom(Queue& q, SbServer* server) {
    SbServer* previous = nullptr;
    for (SbServer* s = q.head; s != nullptr; s = s->next) {
        if (s == server) {
            if (previous != nullptr) previous->next = s->next;
            if (q.head == s) q.head = s->next;
            if (q.tail == s) q.tail = previous;
            --q.count;
            return true;
        }
        previous = s;
    }
    return false;
}

void ServerBrowser::dequeue(SbServer* server) {
    if (!removeFrom(m_queryList, server)) removeFrom(m_pendingList, server);
}

void ServerBrowser::queryServer(SbServer* server) {
    if (m_queryList.tail != nullptr) m_queryList.tail->next = server;
    m_queryList.tail = server;
    server->next = nullptr;
    if (m_queryList.head == nullptr) m_queryList.head = server;
    ++m_queryList.count;
    const uint32_t now = GetTickCount();
    server->updateTime = now;
    uint8_t packet[0x40] = {};
    int size = 0;
    if (m_queryVersion == 1) {
        packet[0] = 0xFE;
        packet[1] = 0xFD;
        packet[2] = 0x00;
        std::memcpy(packet + 3, &now, 4);  // the request's id: the query's start
        if ((server->state & kSbStatePendingBasicQuery) == 0) {
            packet[7] = 0xFF;
            packet[8] = 0xFF;
            packet[9] = 0xFF;
            size = 10;
        } else {
            packet[7] = static_cast<uint8_t>(m_numBasicKeys);
            std::memcpy(packet + 8, m_basicKeys.data(), static_cast<size_t>(m_numBasicKeys));
            packet[8 + m_numBasicKeys] = 0;
            packet[9 + m_numBasicKeys] = 0;
            size = m_numBasicKeys + 10;
        }
    } else if ((server->state & kSbStatePendingBasicQuery) == 0) {
        static const char kStatus[] = "\\status\\";
        std::memcpy(packet, kStatus, 8);
        size = 8;
    } else {
        static const char kBasic[] = "\\basic\\\\info\\";
        std::memcpy(packet, kBasic, 13);
        size = 13;
    }
    uint32_t ip = server->publicIp;
    uint16_t port = server->publicPort;
    if (server->publicIp == m_engineIp && (server->flags & kSbPrivateIp) != 0) {
        ip = server->privateIp;
        port = server->privatePort;
    }
    m_querySocket.sendTo(ip, port, packet, static_cast<size_t>(size));
}

void ServerBrowser::updateServer(SbServer* server, bool addFront, int queryType) {
    server->state &= 0xE3;
    if (queryType == 0) {
        server->state |= kSbStatePendingBasicQuery;
    } else if (queryType == 1) {
        server->state |= kSbStatePendingFullQuery;
    }
    if (m_queryList.count < m_maxUpdates) {
        queryServer(server);
        return;
    }
    if (!addFront) {
        if (m_pendingList.tail != nullptr) m_pendingList.tail->next = server;
        m_pendingList.tail = server;
        server->next = nullptr;
        if (m_pendingList.head == nullptr) m_pendingList.head = server;
    } else {
        server->next = m_pendingList.head;
        m_pendingList.head = server;
        if (m_pendingList.tail == nullptr) m_pendingList.tail = server;
    }
    ++m_pendingList.count;
}

void ServerBrowser::processQr2Reply(SbServer* server, const uint8_t* data, int size) {
    if (data[0] != 0) return;
    size -= 5;
    const uint8_t* p = data + 5;
    if ((server->state & kSbStatePendingBasicQuery) == 0) {
        parseFullKeys(*server, p, size);
        server->state |= kSbStateBasicKeys | kSbStateFullKeys;
    } else {
        for (int i = 0; i < m_numBasicKeys; ++i) {
            if (size < 1) break;
            const int length = stringWithin(p, size);
            if (length < 0) break;
            const char* name = qr2KeyName(m_basicKeys[static_cast<size_t>(i)]);
            server->addKeyValue(name != nullptr ? name : "", text(p));
            size -= length;
            p += length;
        }
        server->state |= kSbStateBasicKeys;
    }
    server->state &= static_cast<uint8_t>(~(kSbStatePendingBasicQuery | kSbStatePendingFullQuery));
    server->updateTime = GetTickCount() - server->updateTime;
    removeFrom(m_queryList, server);
    engineCallback(0, server);
}

void ServerBrowser::processGs1Reply(SbServer* server, char* data) {
    const bool final = std::strstr(data, "\\final\\") != nullptr;
    parseGs1Keys(*server, data);
    if (!final) return;
    server->state |= (server->state & kSbStatePendingBasicQuery) == 0 ? kSbStateFullKeys : kSbStateBasicKeys;
    server->state &= static_cast<uint8_t>(~(kSbStatePendingBasicQuery | kSbStatePendingFullQuery));
    server->updateTime = GetTickCount() - server->updateTime;
    removeFrom(m_queryList, server);
    engineCallback(0, server);
}

void ServerBrowser::readQueries() {
    for (;;) {
        char buffer[0x800];
        uint32_t ip = 0;
        uint16_t port = 0;
        const int got = m_querySocket.receive(buffer, 0x7FF, ip, port);
        if (got < 0) return;
        buffer[got] = 0;
        for (SbServer* s = m_queryList.head; s != nullptr; s = s->next) {
            const bool direct = s->publicIp == ip && s->publicPort == port;
            const bool privately = s->publicIp == m_engineIp && (s->flags & kSbPrivateIp) != 0 &&
                                   s->privateIp == ip && s->privatePort == port;
            if (direct || privately) {
                if (m_queryVersion == 1) {
                    processQr2Reply(s, reinterpret_cast<const uint8_t*>(buffer), got);
                } else {
                    processGs1Reply(s, buffer);
                }
                break;
            }
        }
    }
}

void ServerBrowser::timeoutQueries() {
    const uint32_t now = GetTickCount();
    while (m_queryList.head != nullptr && m_queryList.head->updateTime + 2500u < now) {
        // Stock problem: the failed state goes into the flags (+0x15: NONSTANDARD_PORT set, CONNECT_NEGOTIATE and
        // ICMP_IP cleared), not the state; its pending bits stay.
        m_queryList.head->flags |= 0x10;
        m_queryList.head->flags &= 0xF3;
        engineCallback(1, m_queryList.head);
        if (m_queryList.head != nullptr) {
            m_queryList.head = m_queryList.head->next;
            if (m_queryList.head == nullptr) m_queryList.tail = nullptr;
            --m_queryList.count;
        }
    }
}

void ServerBrowser::startPending() {
    while (m_queryList.count < m_maxUpdates) {
        if (m_pendingList.count < 1) return;
        SbServer* s = m_pendingList.head;
        if (s != nullptr) {
            m_pendingList.head = s->next;
            if (m_pendingList.head == nullptr) m_pendingList.tail = nullptr;
            --m_pendingList.count;
        }
        queryServer(s);
    }
}

// ---- The wrappers.

void ServerBrowser::clearTrigger(SbServer* server) {
    if (server != nullptr && server->publicIp == m_triggerIp && server->publicPort == m_triggerPort) m_triggerIp = 0;
}

void ServerBrowser::listCallback(int reason, SbServer* server) {
    switch (reason) {
    case 0:  // slc_serveradded: queried itself when it came without keys (basic keys, unless the LAN's)
        if (m_callback) m_callback(*this, kServerAdded, server);
        if ((server->state & 0x0F) == 0 && !m_dontUpdate) {
            if (m_state != 0 && m_numBasicKeys != 0) {
                updateServer(server, false, 0);
            } else {
                updateServer(server, false, 1);
            }
        }
        break;
    case 1:  // slc_serverupdated
        if (m_callback) {
            m_callback(*this, (server->state & 3) == 0 ? kServerUpdateFailed : kServerUpdated, server);
        }
        break;
    case 2:  // slc_serverdeleted
        if ((server->state & 0x0C) != 0) dequeue(server);
        if (m_callback) m_callback(*this, kServerDeleted, server);
        break;
    case 3:  // slc_initiallistcomplete
        if (m_disconnectOnComplete) disconnect();
        if ((m_servers.empty() || m_queryList.count == 0) && m_callback) m_callback(*this, kUpdateComplete, nullptr);
        break;
    case 4:  // slc_disconnected
        break;
    case 5:  // slc_queryerror
        if (m_callback) m_callback(*this, kQueryError, nullptr);
        break;
    case 6:  // slc_publicipdetermined
        m_engineIp = m_myPublicIp;
        break;
    default:
        break;
    }
    clearTrigger(server);
}

void ServerBrowser::engineCallback(int reason, SbServer* server) {
    if (m_callback) {
        if (reason == 0) {
            m_callback(*this, kServerUpdated, server);
        } else if (reason == 1) {
            m_callback(*this, kServerUpdateFailed, server);
        } else if (reason == 2) {
            m_callback(*this, kUpdateComplete, server);
        }
    }
    clearTrigger(server);
}

void ServerBrowser::queriesFrame() {
    if (m_queryList.count == 0) return;
    readQueries();
    timeoutQueries();
    if (m_pendingList.count > 0) startPending();
    if (m_queryList.count == 0) engineCallback(2, nullptr);
}

int ServerBrowser::think() {
    queriesFrame();
    freeDeadList();
    if (m_state == 0) return readLan();
    if (m_state > 1 && m_state < 4) return readList();
    return kNoError;
}

void ServerBrowser::runUntilListed() {
    int error = kNoError;
    while (m_state == 3 || (m_queryList.count > 0 && error == kNoError)) {
        Sleep(10);
        queriesFrame();
        freeDeadList();
        error = m_state == 0 ? readLan() : (m_state > 1 && m_state < 4) ? readList() : kNoError;
    }
}

int ServerBrowser::update(bool async, bool disconnectOnComplete, const uint8_t* basicFields, int numBasicFields,
                          const char* serverFilter, int updateOptions, int maxServers) {
    m_disconnectOnComplete = disconnectOnComplete;
    m_numBasicKeys = 0;
    std::string fields;
    for (int i = 0; i < numBasicFields; ++i) {
        const char* name = qr2KeyName(basicFields[i]);
        if (name == nullptr) name = "";
        if (static_cast<int>(fields.size() + std::strlen(name)) + 1 > 0xFF) break;
        fields += '\\';
        fields += name;
        if (m_numBasicKeys < 20) m_basicKeys[static_cast<size_t>(m_numBasicKeys++)] = basicFields[i];
    }
    const int error = connectAndQuery(fields.c_str(), serverFilter, updateOptions, maxServers);
    if (error == kNoError && !async) runUntilListed();
    return error;
}

int ServerBrowser::auxUpdateIp(const char* ip, uint16_t port, bool viaMaster, bool async, bool fullUpdate) {
    m_dontUpdate = true;
    int error = kNoError;
    const uint32_t address = inet_addr(ip);
    const uint16_t netPort = htons(port);
    if (!viaMaster) {
        const int index = findServer(address, netPort);
        SbServer* server = nullptr;
        std::unique_ptr<SbServer> made;
        if (index == -1) {
            made = std::make_unique<SbServer>();
            made->publicIp = address;
            made->publicPort = netPort;
            made->flags = kSbUnsolicitedUdp;
            server = made.get();
        } else {
            server = m_servers[static_cast<size_t>(index)].get();
            dequeue(server);
        }
        server->state &= 0xE3;
        server->state |= fullUpdate ? kSbStatePendingFullQuery : kSbStatePendingBasicQuery;
        if (m_queryList.count < m_maxUpdates) {
            queryServer(server);
        } else {
            server->next = m_pendingList.head;
            m_pendingList.head = server;
            if (m_pendingList.tail == nullptr) m_pendingList.tail = server;
            ++m_pendingList.count;
        }
        if (made) addServer(std::move(made));
    } else {
        error = serverInfoRequest(address, netPort);
    }
    if (!async && error == kNoError) {
        m_triggerIp = address;
        m_triggerPort = netPort;
        if (m_triggerIp != 0) {
            // 0x0056D380: via the master, the LAN's state ends the wait too.
            int e = kNoError;
            while (e == kNoError) {
                Sleep(10);
                queriesFrame();
                freeDeadList();
                e = m_state == 0 ? readLan() : (m_state > 1 && m_state < 4) ? readList() : kNoError;
                if (viaMaster && m_state == 0) break;
                if (m_triggerIp == 0) break;
            }
        }
    }
    m_dontUpdate = false;
    return error;
}

}  // namespace oj::net
