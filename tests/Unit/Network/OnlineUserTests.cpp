// Unit tests: the game's user object (Core/Network/GameSpy/GameSpyUser, OnlineUser).

#include "Framework/Test.h"

#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/NetTransport.h"

namespace {

using oj::test::check;

}  // namespace

// A player's address for the presence (0x0055F6C0): a local player takes it; a player of none (-1, 0x0055BA00's with
// no local player, as the matchmaking's end once signed out) writes nothing (the stock's wrote before the players'
// table: the heap it broke crashed a test run now and then, in OnlineLogin.Guest's end); either way the presence goes
// within a second (the timer raised to 59).
OJ_UNIT_TEST(OnlineUser, AddressOfNoPlayer) {
    oj::net::OnlineUser user;
    const int32_t player = user.newPlayer();
    const oj::net::Address at{0x0100007Fu, 0x1234};
    user.setAddress(player, at);
    const bool ours = user.player(player).address == at && user.presenceTimer == 59.0f;
    user.presenceTimer = 0.0f;
    user.localPlayers.clear();
    user.setAddress(-1, oj::net::Address{0x0200007Fu, 0x5678});
    const bool none =
        user.players().size() == 1 && user.player(player).address == at && user.presenceTimer == 59.0f;
    check(ours, "a local player's address for the presence, sent within a second");
    check(none, "a player of none: no player's address written, the presence still sent within a second");
}
