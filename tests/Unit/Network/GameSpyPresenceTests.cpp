// Unit tests: GameSpyPresence.

#include "Framework/Test.h"

#include "Core/Base/Text.h"
#include "Core/Network/GameSpy/GameSpyPresence.h"

#include <string>

namespace {

using oj::test::check;

}  // namespace

// GameSpy Presence's pieces: MD5 (RFC 1321's vectors), the login's hash, the password's encoding (its base64
// of '[', ']' and the padding '_'), UTF-8 both ways.
OJ_UNIT_TEST(GameSpyPresence, Pieces) {
    namespace gp = oj::net::gp;
    const bool md5 = gp::md5Hex("") == "d41d8cd98f00b204e9800998ecf8427e" &&
                     gp::md5Hex("abc") == "900150983cd24fb0d6963f7d28e17f72" &&
                     gp::md5Hex("12345678901234567890123456789012345678901234567890123456789012345678901234567890") ==
                         "57edf4a22be3c955ac49da2e2107b67a";
    const std::string hash = gp::md5Hex("secret");
    const bool login = gp::loginHash(hash, "a@b", "x", "y") ==
                       gp::md5Hex(hash + std::string(48, ' ') + "a@b" + "x" + "y" + hash);
    const std::string one = gp::passwordEncode("a");
    const std::string two = gp::passwordEncode("ab");
    const std::string three = gp::passwordEncode("abc");
    const bool encoded = one.size() == 4 && one.substr(2) == "__" && two.size() == 4 && two[3] == '_' &&
                         three.size() == 4 && three.find('_') == std::string::npos &&
                         gp::passwordEncode("abc") == three && gp::passwordEncode("abcd").substr(0, 4) == three;
    const bool utf8 = gp::toUtf8(u"é€") == "\xc3\xa9\xe2\x82\xac" &&
                      gp::fromUtf8("\xc3\xa9\xe2\x82\xac" "\xff") == u"é€?";
    check(md5 && login && encoded && utf8, "GameSpy Presence: MD5, the login's hash, the password's encoding, UTF-8");
}
