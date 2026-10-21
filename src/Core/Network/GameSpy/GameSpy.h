#pragma once
// What the GameSpy SDK linked in the stock game shares between its parts (docs/spec/28-network.md): the game's name
// and secret key, the challenge's answer (gs_encrypt and its base64), the master list server's name (its hash), the
// list's cipher (GOA), the available check (the services up, asked once before a search or a negotiation), the
// services' names and the game's socket the parts send on.

#include "Core/Network/NetTcp.h"
#include "Core/Network/NetTransport.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace oj::net {

// 0x00471920: the game's name and secret key ([0x0075F1F0], [0x0075F230]) and its presence product ([0x0075F270]).
inline constexpr const char* kGameName = "juicedpc";
inline constexpr const char* kSecretKey = "g3J8df2";  // 7 characters, as the stock game builds it
inline constexpr int32_t kProductId = 10362;

// The services' names and ports as the stock game has them. The SDK keeps overrides of the names that nothing writes
// (the master's [0x007A4340], the list server's [0x0075F5E4], the available check's [0x007A43E0]) and the NAT
// negotiation servers' names behind pointers (0x0071B1C0, 0x0071B1C4). Only the tests point them (and the ports) at
// their stand-ins on the loopback; nothing in the game changes them.
struct ServiceHosts {
    std::string master;           // "" → "<game>.master.gamespy.com"
    uint16_t masterPort = 27900;  // 0x6CFC
    std::string list;             // "" → masterListHost(<game>)
    uint16_t listPort = 28910;    // 0x70EE
    std::string natneg1 = "natneg1.gamespy.com";
    std::string natneg2 = "natneg2.gamespy.com";
    uint16_t natnegPort = 27901;  // 0x6CFD
    std::string available;        // "" → "<game>.available.gamespy.com"
    uint16_t availablePort = 27900;
};
ServiceHosts& serviceHosts();

// The game's socket the SDK's parts are given (the transport's 8667): their packets go out of it as they are. The
// SDK never reads it; what comes to it reaches them through the transport's filters.
struct GameSocket {
    std::function<bool(const Address& to, const void* data, size_t size)> send;
    uint16_t port = 0;  // its own port, network order (getsockname)
    bool valid() const { return static_cast<bool>(send); }
};

// The SDK's own UTF-8 of the wide strings: 0x00568C70 (one to three bytes a character) and 0x00568D00 / 0x00568BD0 back
// (a sequence of two or three bytes, else the byte's character under 0x80, else '?'), both up to the first 0.
std::string gsUcs2ToUtf8(std::u16string_view text);
std::u16string gsUtf8ToUcs2(std::string_view text);

// 0x0056AE60: RC4 of a sort keyed by `key` over the bytes in place: the state's i-th swap with the key's byte, then
// for each byte x moves by the byte + 1, y by the state at x, the two swapped, the byte XORed with the state at their
// sum.
void gsEncrypt(std::string_view key, uint8_t* bytes, size_t size);
// 0x0056AD90: base64 of the standard alphabet in groups of three bytes padded with zeros, four characters each, no
// '=' (a short last group still gives four).
std::string gsEncode(const uint8_t* bytes, size_t size);
// 0x0056AFB0: the answer to a master's challenge: gsEncode(gsEncrypt(secret, challenge)).
std::string challengeAnswer(std::string_view challenge, std::string_view secret = kSecretKey);

// 0x0056A670: h = tolower(c) + h x 0x9CCF9319 over the name.
uint32_t masterHash(std::string_view gameName);
// The master list server of a game: "<game>.ms<hash % 20>.gamespy.com" (juicedpc.ms3.gamespy.com).
std::string masterListHost(std::string_view gameName);

// The server list's cipher (0x0057FFA0 .. 0x00580200: GOACryptInit, GOACrypt). Both ways for the tests' stand-ins.
class GoaCrypt {
public:
    void init(const uint8_t* key, size_t size);
    uint8_t decrypt(uint8_t cipher);
    uint8_t encrypt(uint8_t plain);
    void decrypt(uint8_t* bytes, size_t size) {
        for (size_t i = 0; i < size; ++i) bytes[i] = decrypt(bytes[i]);
    }

private:
    uint8_t keyRand(uint32_t limit, const uint8_t* key, size_t size, uint8_t& sum, size_t& position);
    uint8_t keyByte();

    std::array<uint8_t, 256> m_cards{};
    uint8_t m_rotor = 0;
    uint8_t m_ratchet = 0;
    uint8_t m_avalanche = 0;
    uint8_t m_lastPlain = 0;
    uint8_t m_lastCipher = 0;
};

// 0x0057D060: the list's key from the client's challenge (8 bytes), the secret and the server's key:
// challenge[(i x secret[i % len]) % 8] ^= challenge[i % 8] ^ serverKey[i] for each of the server key's bytes, then
// GOACryptInit with the 8 bytes.
void listCryptInit(GoaCrypt& crypt, std::array<uint8_t, 8> challenge, std::string_view secret,
                   const uint8_t* serverKey, size_t keySize);

// The available check (0x00569C00 / 0x00569E20; the result [0x007A4380]): "<game>.available.gamespy.com":27900/UDP
// asked `09 00 00 00 00 <game>\0`; an answer `FE FD 09` with the big-endian value at [3]: bit 0 unavailable (2),
// else available (1) or, with bit 1, temporarily unavailable (3); no answer: asked again each 2 s, after the eighth
// available. The server browser and NAT negotiation want 1.
class AvailableCheck {
public:
    enum Result : int32_t { Waiting = 0, Available = 1, Unavailable = 2, TemporarilyUnavailable = 3 };
    // False when the name does not resolve (the result then stays as it was).
    bool start(std::string_view gameName = kGameName);
    // Each frame while waiting; the result once known.
    Result think();
    Result result() const { return m_result; }
    // Its socket open (the stock's [0x0075B570] not -1).
    bool opened() const { return m_socket.opened(); }

private:
    UdpSocket m_socket;
    uint32_t m_ip = 0;
    uint16_t m_port = 0;
    std::string m_packet;
    uint32_t m_sentAt = 0;
    int m_resends = 0;
    Result m_result = Waiting;
};

// [0x007A4380]: the last available check's result, as the server browser and NAT negotiation read it.
int32_t& availableResult();
// [0x007A43A0]: the game's name the available check was started with (NAT negotiation's INIT carries it).
std::string& availableGameName();

// The SDK's unique id of the machine (GOAGetUniqueID, 0x0056E5B0; its pointer [0x0071B1C8] beside the NAT
// negotiation servers' names is read by nothing, so nothing calls it): the Crypt value of HKEY_CURRENT_USER's
// Software\GameSpy\GameSpy 3D\Registration when it holds 19 characters (the key made when it cannot be
// opened), else 19 made (0x0056E4F0) and written there; the dashes after the 4th, 9th and 14th taken out: 16
// characters. Stock problem: a key that cannot be opened is read through a handle never set.
std::string uniqueId();
// 0x0056E4F0: the generator ([0x0071B1CC]) seeded by the performance counter's halves, the time, clock() and the tick
// count XORed (0 as 1, else its low 31 bits); 19 characters of 0-9, a-z, A-Z by the next value modulo 62 (0x0056E6E0:
// x 16807 modulo 2^31 - 1 by halves), '-' at 4, 9 and 14.
std::string makeUniqueId(uint32_t seed);

}  // namespace oj::net
