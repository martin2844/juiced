// What the friends' and players' windows of GameSpy share (docs/spec/28-network.md): the note of an invitation over
// the windows, the online keys of the network game's windows, the options' target. The windows (online_players,
// online_player_options, online_friends, online_friend_options, online_voice) are in their own files; the front end
// runs them over online_bg with their screen as its current one.

#include "Game/Frontend/Network/OnlineFriendWindows.h"

#include "Game/Frontend/Network/OnlineFriendInternal.h"

namespace oj::frontend {

using namespace onlinefriends;

// 0x004E1860, after the window manager's frame (0x005A7F90) while the game's friends are made ([0x0075EF7C]): the
// game's user object there and the top window not closedown; the last local player's note: a game invitation (vt+0x50)
// live_gameinvite_rec, else a friend request (vt+0x4C) live_friendinvite_rec, else none; drawn white in a square 0.04
// of the screen's width, at (0.878, 0.089) of the screen, in a race (the race front end there, [G+0x3C]) at (0.1,
// 0.853). The icons are made once (0x004E1670, [0x0075EF80]) from the front end's first frame.
void drawPresenceIcon(render::IRenderDevice& device, ui::Screen& loader, const std::string& topWindow, bool race) {
    static render::TextureId gameInvite = 0;
    static render::TextureId friendInvite = 0;
    static bool made = false;
    if (!made) {
        made = true;
        friendInvite = loader.loadTexture(kIconFriendRequestFrom);
        gameInvite = loader.loadTexture(kIconInviteFrom);
    }
    if (net::onlineFriends() == nullptr) return;
    net::OnlineUser* user = net::onlineUser();
    if (user == nullptr || _stricmp(topWindow.c_str(), "closedown") == 0) return;
    render::TextureId icon = 0;
    for (const int32_t player : user->localPlayers) {
        if (user->inviteNote(player) != 0) {
            icon = gameInvite;
        } else {
            icon = user->friendNote(player) != 0 ? friendInvite : 0;
        }
    }
    if (icon == 0) return;
    const auto w = static_cast<float>(device.width());
    const auto h = static_cast<float>(device.height());
    const float x0 = race ? w * 0.1f : w * 0.878f;
    const float y0 = race ? h * 0.853f : h * 0.089f;
    const float side = w * 0.04f;
    const render::QuadVertex corners[4] = {
        {x0, y0, 0.0f, 0.0f, 0xFFFFFFFFu},
        {x0 + side, y0, 1.0f, 0.0f, 0xFFFFFFFFu},
        {x0, y0 + side, 0.0f, 1.0f, 0xFFFFFFFFu},
        {x0 + side, y0 + side, 1.0f, 1.0f, 0xFFFFFFFFu},
    };
    ui::drawPicture(device, corners, icon, render::Blend::Alpha);
}

// 0x0046E218: with the matchmaking, 8 (players) or 9 (friends): our readiness 0 sent (the local indices 0 .. 2),
// CLICK, online_bg's window ([0x0072182C], 32 characters at most) online_players or online_friends and online_bg.
bool netOnlineKeys(WindowHost& host, uint32_t event) {
    net::NetGame* net = net::netGame();
    if (net == nullptr || net::matchmaking() == nullptr) return false;
    // The fixed build's guest has no friends' windows.
    if (const net::OnlineUser* u = net::onlineUser(); u != nullptr && u->guest()) return false;
    const char* window = event == input::kSelect4   ? "online_players"
                         : event == input::kSelect5 ? "online_friends"
                                                    : nullptr;
    if (window == nullptr) return false;
    for (int32_t j = 0; j < 3; ++j) net->session.sendReady(0, j);
    host.sound("CLICK", 1.0f, 0xF);
    host.shared().onlineForms.background = window;
    host.request("online_bg", SwitchMode::Replace);
    return true;
}

// 0x0047A330: the options' local player ([0x00715D2C]) and user ([0x00715D30]), each referenced, the ones before let
// go (the user object unchecked; none here: only kept).
void onlineOptionsTarget(WindowHost& host, int32_t player, int32_t user) {
    net::OnlineUser* u = net::onlineUser();
    if (u != nullptr) {
        u->playerRelease(host.shared().onlineForms.optionsPlayer);
        u->playerAddRef(player);
    }
    host.shared().onlineForms.optionsPlayer = player;
    if (u != nullptr) {
        u->userRelease(host.shared().onlineForms.optionsUser);
        u->userAddRef(user);
    }
    host.shared().onlineForms.optionsUser = user;
}

}  // namespace oj::frontend
