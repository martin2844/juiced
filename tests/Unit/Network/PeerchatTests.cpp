// Unit tests: Peerchat (the fixed build's lobby chat).

#include "Framework/Test.h"

#include "Core/Network/GameSpy/Peerchat.h"

#include <array>
#include <cstdint>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// The cipher: the bytes OpenSpy's Peerchat took (a challenge and juicedpc's key, checked against the server), and the
// same cipher both ways gives the text back.
OJ_UNIT_TEST(Peerchat, Cipher) {
    using oj::net::peerchat::Cipher;
    std::string text = "PRIVMSG #a :hi\r\n";
    const std::array<uint8_t, 16> expected = {0xCD, 0x9E, 0x70, 0x23, 0xD9, 0x5D, 0x16, 0x6F,
                                              0xE0, 0x53, 0x6E, 0x88, 0xC4, 0x99, 0x46, 0x1A};
    Cipher out;
    out.start("0123456789abcdef", "g3J8df2");
    out.run(reinterpret_cast<uint8_t*>(text.data()), text.size());
    bool same = text.size() == expected.size();
    for (size_t i = 0; same && i < expected.size(); ++i) same = static_cast<uint8_t>(text[i]) == expected[i];
    Cipher back;
    back.start("0123456789abcdef", "g3J8df2");
    back.run(reinterpret_cast<uint8_t*>(text.data()), text.size());
    check(same && text == "PRIVMSG #a :hi\r\n", "peerchat: the cipher's bytes, and back");
}

// A nick of Peerchat's characters (others '_', not beginning with a digit, 20 at most) and the session's channel.
OJ_UNIT_TEST(Peerchat, NickAndChannel) {
    using oj::net::peerchat::chatNick;
    check(chatNick(u"Diman") == "Diman" && chatNick(u"1st Racer") == "J1st_Racer" && chatNick(u"Иван") == "____" &&
              chatNick(u"") == "Juiced" && chatNick(u"ABCDEFGHIJKLMNOPQRSTUVWXYZ").size() == 20 &&
              oj::net::peerchat::sessionChannel("juicedpc", 0x1234ABu) == "#GSP!juicedpc!001234AB",
          "peerchat: the nick and the session's channel");
}
