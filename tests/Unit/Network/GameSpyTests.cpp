// Unit tests: GameSpy.

#include "Framework/Test.h"

#include "Core/Network/GameSpy/GameSpy.h"

#include <cstring>

namespace {

using oj::test::check;

}  // namespace

// The SDK's unique id nothing calls (0x0056E4F0): 19 characters by the generator, the dashes at 4, 9 and 14; a
// seed of 0 taken as 1.
OJ_UNIT_TEST(GameSpy, UniqueId) {
    check(oj::net::makeUniqueId(1) == "5pn2-QMwQ-bLq9-WqJ9" && oj::net::makeUniqueId(0) == "5pn2-QMwQ-bLq9-WqJ9" &&
              oj::net::makeUniqueId(0x12345678) == "eZ8L-cdBp-jra0-tC9B",
          "gp: the SDK's unique id (GOAGetUniqueID's generator)");
}
// GameSpy's shared parts: the master list's name, the challenge's answer, the GOA cipher both ways.
OJ_UNIT_TEST(GameSpy, SharedParts) {
    uint8_t plain[16] = {};
    for (int i = 0; i < 16; ++i) plain[i] = static_cast<uint8_t>(i * 7);
    oj::net::GoaCrypt a, b;
    oj::net::listCryptInit(a, {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'}, oj::net::kSecretKey,
                           reinterpret_cast<const uint8_t*>("server"), 6);
    oj::net::listCryptInit(b, {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'}, oj::net::kSecretKey,
                           reinterpret_cast<const uint8_t*>("server"), 6);
    uint8_t coded[16];
    for (int i = 0; i < 16; ++i) coded[i] = a.encrypt(plain[i]);
    b.decrypt(coded, 16);
    check(oj::net::masterHash("juicedpc") == 575721383u &&
              oj::net::masterListHost("juicedpc") == "juicedpc.ms3.gamespy.com" &&
              std::memcmp(coded, plain, 16) == 0 && oj::net::challengeAnswer("abc").size() == 4 &&
              oj::net::gsEncode(reinterpret_cast<const uint8_t*>("Man"), 3) == "TWFu",
          "GameSpy: the master list's name, the answer's base64, the list's cipher both ways");
}
