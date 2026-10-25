#ifdef OJ_LINUX
#include "Linux/Paths.h"
#endif
#include <fstream>
#include <filesystem>
#include "Core/Network/GameSpy/GameSpy.h"

#include "Core/Network/GameSpy/GameSpyPresence.h"

#include <winsock2.h>
#include <windows.h>

#include <cctype>
#include <cstring>
#include <ctime>
#include <format>
#include <utility>

namespace oj::net {

std::string gsUcs2ToUtf8(std::u16string_view text) {
    std::string out;
    for (char16_t c : text) {
        if (c == 0) break;
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            out.push_back(static_cast<char>((c >> 6) | 0xC0));
            out.push_back(static_cast<char>((c & 0x3F) | 0x80));
        } else {
            out.push_back(static_cast<char>((c >> 12) | 0xE0));
            out.push_back(static_cast<char>(((c >> 6) & 0x3F) | 0x80));
            out.push_back(static_cast<char>((c & 0x3F) | 0x80));
        }
    }
    return out;
}

std::u16string gsUtf8ToUcs2(std::string_view text) {
    std::u16string out;
    auto at = [&](size_t i) { return i < text.size() ? static_cast<uint8_t>(text[i]) : uint8_t{0}; };
    for (size_t i = 0; i < text.size() && at(i) != 0;) {
        const uint8_t c = at(i);
        if (c < 0x80) {
            out.push_back(c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && (at(i + 1) & 0xC0) == 0x80) {
            out.push_back(static_cast<char16_t>(((c & 0x1F) << 6) + (at(i + 1) & 0x3F)));
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && (at(i + 1) & 0xC0) == 0x80 && (at(i + 2) & 0xC0) == 0x80) {
            out.push_back(static_cast<char16_t>((((c << 6) + (at(i + 1) & 0x3F)) << 6) + (at(i + 2) & 0x3F)));
            i += 3;
        } else {
            out.push_back(u'?');
            i += 1;
        }
    }
    return out;
}

void gsEncrypt(std::string_view key, uint8_t* bytes, size_t size) {
    std::array<uint8_t, 256> state{};
    for (size_t i = 0; i < 256; ++i) state[i] = static_cast<uint8_t>(i);
    if (key.empty()) return;
    uint8_t x = 0;
    uint8_t y = 0;
    size_t k = 0;
    for (size_t i = 0; i < 256; ++i) {
        y = static_cast<uint8_t>(static_cast<uint8_t>(key[k]) + state[i] + y);
        k = (k + 1) % key.size();
        std::swap(state[i], state[y]);
    }
    x = 0;
    y = 0;
    for (size_t i = 0; i < size; ++i) {
        x = static_cast<uint8_t>(x + bytes[i] + 1);
        y = static_cast<uint8_t>(state[x] + y);
        std::swap(state[x], state[y]);
        bytes[i] ^= state[static_cast<uint8_t>(state[x] + state[y])];
    }
}

std::string gsEncode(const uint8_t* bytes, size_t size) {
    static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < size; i += 3) {
        uint8_t group[3] = {};
        for (size_t j = 0; j < 3 && i + j < size; ++j) group[j] = bytes[i + j];
        out.push_back(kAlphabet[group[0] >> 2]);
        out.push_back(kAlphabet[((group[0] & 3) << 4) | (group[1] >> 4)]);
        out.push_back(kAlphabet[((group[1] & 0xF) << 2) | (group[2] >> 6)]);
        out.push_back(kAlphabet[group[2] & 0x3F]);
    }
    return out;
}

std::string challengeAnswer(std::string_view challenge, std::string_view secret) {
    std::string bytes(challenge);
    gsEncrypt(secret, reinterpret_cast<uint8_t*>(bytes.data()), bytes.size());
    return gsEncode(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
}

uint32_t masterHash(std::string_view gameName) {
    uint32_t h = 0;
    for (char c : gameName) {
        h = static_cast<uint32_t>(std::tolower(static_cast<unsigned char>(c))) + h * 0x9CCF9319u;
    }
    return h;
}

std::string masterListHost(std::string_view gameName) {
    return std::format("{}.ms{}.gamespy.com", gameName, masterHash(gameName) % 20);
}

// ---- GOA.

uint8_t GoaCrypt::keyRand(uint32_t limit, const uint8_t* key, size_t size, uint8_t& sum, size_t& position) {
    if (limit == 0) return 0;
    uint32_t mask = 1;
    while (mask < limit) mask = mask * 2 + 1;
    uint32_t tries = 0;
    uint32_t u = 0;
    do {
        sum = static_cast<uint8_t>(m_cards[sum] + key[position++]);
        if (position >= size) {
            position = 0;
            sum = static_cast<uint8_t>(sum + size);
        }
        u = mask & sum;
        if (++tries > 11) u %= limit;
    } while (u > limit);
    return static_cast<uint8_t>(u);
}

void GoaCrypt::init(const uint8_t* key, size_t size) {
    for (size_t i = 0; i < 256; ++i) m_cards[i] = static_cast<uint8_t>(i);
    uint8_t sum = 0;
    size_t position = 0;
    if (size == 0) {
        for (size_t i = 0; i < 256; ++i) m_cards[i] = static_cast<uint8_t>(255 - i);
        m_rotor = 1;
        m_ratchet = 3;
        m_avalanche = 5;
        m_lastPlain = 7;
        m_lastCipher = 11;
        return;
    }
    for (int i = 255; i >= 0; --i) {
        const uint8_t j = keyRand(static_cast<uint32_t>(i), key, size, sum, position);
        std::swap(m_cards[static_cast<size_t>(i)], m_cards[j]);
    }
    m_rotor = m_cards[1];
    m_ratchet = m_cards[3];
    m_avalanche = m_cards[5];
    m_lastPlain = m_cards[7];
    m_lastCipher = m_cards[sum];
}

uint8_t GoaCrypt::keyByte() {
    m_ratchet = static_cast<uint8_t>(m_ratchet + m_cards[m_rotor++]);
    const uint8_t t = m_cards[m_lastCipher];
    m_cards[m_lastCipher] = m_cards[m_ratchet];
    m_cards[m_ratchet] = m_cards[m_lastPlain];
    m_cards[m_lastPlain] = m_cards[m_rotor];
    m_cards[m_rotor] = t;
    m_avalanche = static_cast<uint8_t>(m_avalanche + m_cards[t]);
    return static_cast<uint8_t>(
        m_cards[static_cast<uint8_t>(m_cards[m_avalanche] + m_cards[m_rotor])] ^
        m_cards[m_cards[static_cast<uint8_t>(m_cards[m_lastPlain] + m_cards[m_lastCipher] + m_cards[m_ratchet])]]);
}

uint8_t GoaCrypt::decrypt(uint8_t cipher) {
    const uint8_t plain = static_cast<uint8_t>(cipher ^ keyByte());
    m_lastPlain = plain;
    m_lastCipher = cipher;
    return plain;
}

uint8_t GoaCrypt::encrypt(uint8_t plain) {
    const uint8_t cipher = static_cast<uint8_t>(plain ^ keyByte());
    m_lastPlain = plain;
    m_lastCipher = cipher;
    return cipher;
}

void listCryptInit(GoaCrypt& crypt, std::array<uint8_t, 8> challenge, std::string_view secret,
                   const uint8_t* serverKey, size_t keySize) {
    for (size_t i = 0; i < keySize; ++i) {
        // Signed characters in the stock code; all ASCII here.
        const auto s = static_cast<size_t>(static_cast<int8_t>(secret[i % secret.size()]));
        challenge[(i * s) % 8] ^= static_cast<uint8_t>(challenge[i % 8] ^ serverKey[i]);
    }
    crypt.init(challenge.data(), challenge.size());
}

// ---- The services.

ServiceHosts& serviceHosts() {
    static ServiceHosts hosts;
    return hosts;
}

// ---- The available check.

int32_t& availableResult() {
    static int32_t result = AvailableCheck::Waiting;
    return result;
}

std::string& availableGameName() {
    static std::string name;
    return name;
}

bool AvailableCheck::start(std::string_view gameName) {
    // The name kept for NAT negotiation and for GameSpy Presence's \gamename\ ([0x007A43A0]); the socket none
    // first (-1): a name that does not resolve or no socket leaves it so, and the think then says available at once.
    availableGameName() = gameName;
    m_socket.close();
    m_result = Waiting;
    const ServiceHosts& hosts = serviceHosts();
    m_ip = resolveHost(hosts.available.empty() ? std::format("{}.available.gamespy.com", gameName) : hosts.available);
    if (m_ip == 0) return false;
    m_port = htons(hosts.availablePort);
    if (!m_socket.open()) return false;
    m_packet.assign(5, '\0');
    m_packet[0] = '\x09';
    m_packet.append(gameName);
    m_packet.push_back('\0');
    m_socket.sendTo(m_ip, m_port, m_packet.data(), m_packet.size());
    m_sentAt = GetTickCount();
    m_resends = 0;
    return true;
}

AvailableCheck::Result AvailableCheck::think() {
    if (m_result != Waiting) return m_result;
    if (!m_socket.opened()) {
        m_result = Available;
        availableResult() = m_result;
        return m_result;
    }
    // One datagram a call (0x40 bytes at most).
    uint8_t buffer[64];
    uint32_t ip = 0;
    uint16_t port = 0;
    const int got = m_socket.receive(buffer, sizeof buffer, ip, port);
    if (got >= 7 && ip == m_ip && port == m_port && buffer[0] == 0xFE && buffer[1] == 0xFD && buffer[2] == 0x09) {
        const uint32_t v = (static_cast<uint32_t>(buffer[3]) << 24) | (static_cast<uint32_t>(buffer[4]) << 16) |
                           (static_cast<uint32_t>(buffer[5]) << 8) | buffer[6];
        m_result = (v & 1) != 0 ? Unavailable : static_cast<Result>(1 | (v & 2));
        availableResult() = m_result;
        m_socket.close();
        return m_result;
    }
    const uint32_t now = GetTickCount();
    if (m_sentAt + 2000u < now) {
        if (m_resends == 8) {
            m_result = Available;
            availableResult() = m_result;
            m_socket.close();
            return m_result;
        }
        m_socket.sendTo(m_ip, m_port, m_packet.data(), m_packet.size());
        m_sentAt = now;
        ++m_resends;
    }
    return m_result;
}


// ---- The unique id (nothing calls it).

namespace {

uint32_t g_uniqueSeed = 0;  // [0x0071B1CC]

// 0x0056E6E0: x 16807 modulo 2^31 - 1, by halves.
uint32_t nextUnique() {
    const uint32_t hi = (g_uniqueSeed >> 16) * 16807u;
    uint32_t lo = (g_uniqueSeed & 0xFFFF) * 16807u + ((hi & 0x7FFF) << 16);
    if (lo > 0x7FFFFFFFu) lo = (lo & 0x7FFFFFFFu) + 1;
    g_uniqueSeed = lo + (hi >> 15);
    if (g_uniqueSeed > 0x7FFFFFFFu) g_uniqueSeed = (g_uniqueSeed & 0x7FFFFFFFu) + 1;
    return g_uniqueSeed;
}

}  // namespace

std::string makeUniqueId(uint32_t seed) {
    static constexpr char kCharacters[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    g_uniqueSeed = seed == 0 ? 1 : seed & 0x7FFFFFFFu;
    std::string id(19, '\0');
    for (size_t i = 0; i < id.size(); ++i) {
        id[i] = i == 4 || i == 9 || i == 14 ? '-' : kCharacters[static_cast<int32_t>(nextUnique()) % 62];
    }
    return id;
}

#if defined(OJ_MACOS) || defined(OJ_LINUX)
std::string uniqueId() {
    const char* home = std::getenv("HOME");
#ifdef OJ_LINUX
    const auto folder = oj::linux_port::dataPath();
#else
    const auto folder = std::filesystem::path(home ? home : ".") / "Library/Application Support/Juiced";
#endif
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    std::string id;
    std::ifstream input(folder / "network-id");
    input >> id;
    if (id.size() != 19) {
        id = makeUniqueId(GetTickCount() ^ static_cast<uint32_t>(std::time(nullptr)));
        std::ofstream(folder / "network-id") << id;
    }
    return id.substr(0,4)+id.substr(5,4)+id.substr(10,4)+id.substr(15,4);
}
#else
std::string uniqueId() {
    constexpr const char* kKey = "Software\\GameSpy\\GameSpy 3D\\Registration";
    HKEY key = nullptr;
    const LSTATUS opened = RegOpenKeyExA(HKEY_CURRENT_USER, kKey, 0, KEY_ALL_ACCESS, &key);
    char value[0x104] = {};
    DWORD type = REG_SZ;
    DWORD size = sizeof(value);
    const LSTATUS read =
        RegQueryValueExA(key, "Crypt", nullptr, &type, reinterpret_cast<BYTE*>(value), &size);
    std::string id;
    if (read == ERROR_SUCCESS && std::strlen(value) == 19) {
        id = value;
    } else {
        LARGE_INTEGER counter{};
        const uint32_t clockBits = QueryPerformanceCounter(&counter) != 0
                                       ? static_cast<uint32_t>(counter.HighPart) ^ counter.LowPart
                                       : 0;
        const uint32_t seed = static_cast<uint32_t>(std::time(nullptr)) ^ static_cast<uint32_t>(std::clock()) ^
                              GetTickCount() ^ clockBits;
        id = makeUniqueId(seed);
        if (opened != ERROR_SUCCESS) {
            DWORD disposition = 0;
            RegCreateKeyExA(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &key, &disposition);
        }
        RegSetValueExA(key, "Crypt", 0, REG_SZ, reinterpret_cast<const BYTE*>(id.c_str()),
                       static_cast<DWORD>(id.size() + 1));
    }
    RegCloseKey(key);
    return id.substr(0, 4) + id.substr(5, 4) + id.substr(10, 4) + id.substr(15, 4);
}
#endif

}  // namespace oj::net
