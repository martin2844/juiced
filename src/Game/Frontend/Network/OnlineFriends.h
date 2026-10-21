#pragma once
// The friends' and players' window online_friends of GameSpy (docs/spec/28-network.md), over online_bg.

#include "Game/Frontend/Network/OnlineHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record of online_friends (0x00721A10, vtable 0x006F2E7C).
class OnlineFriends {
public:
    // 0x0047B4C0
    void enter(OnlineHost& host, ui::Screen& s);
    // 0x0047B870
    void leave(OnlineHost& host, ui::Screen& s);
    // 0x0047B9D0
    void update(OnlineHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x0047AC70: GAMESPY shown; with the user object and the friends, m_friend_name, m_state, m_friend_icon and
    // m_voice_icon (all four there): the friends updated (vt+0x18); the names' users let go and the four emptied; each
    // friend of the list (referenced) and its user (vt+0x1C, referenced): with the statistics and +0x18 its record made
    // (0x004EDF20) or asked for again (+0x28 0); its nick's low bytes, and with +0x10 the one named before
    // ([0x0075EEB0]) becomes the highlighted user (+0x14); the nick cut to the names' width (0x005B3260) less 10 pixels
    // with the user as the value and the low bytes as the item's name; m_state "PLAYING" (vt+0x54), "ONLINE" (vt+0x50)
    // or "OFFLINE"; m_friend_icon an empty item with its icon: "live_gameinvite_rec" (vt+0x44), "live_gameinvite_sent"
    // (vt+0x48), "live_friendinvite_rec" (vt+0x3C), "live_friendinvite_sent" (vt+0x40), "live_friendonline" (vt+0x50);
    // m_voice_icon an empty item, with the voice ([0x0075F044]) its icon: mute for a user our first local player muted,
    // else "live_comm_off" for a friend's voice mail (vt+0x5C: none on the PC; 0x004475D0 by the name); the friend let
    // go.
    void fillList(OnlineHost& host, ui::Screen& s);
    // 0x0047B2C0: with the statistics ([0x0075F030]) GAME the highlighted friend's game (the friends' entry of the
    // user, vt+0x20 referenced, its +0x64: empty, let go) and the user's record as 0x00479B40 shows it.
    void stats(ui::Screen& s);

    float m_timer = 0.0f;      // +0x0C
    int32_t m_again = 0;       // +0x10: the friend named before ([0x0075EEB0]) highlighted again
    int32_t m_selected = -1;   // +0x14: the user highlighted (referenced)
    int32_t m_statsAgain = 0;  // +0x18: the statistics' records asked again
    int32_t m_searching = 0;   // +0x1C: a friend's session asked
};

}  // namespace oj::frontend
