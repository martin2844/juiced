#pragma once
// The friends' and players' window online_players of GameSpy (docs/spec/28-network.md), over online_bg.

#include "Game/Frontend/Network/OnlineHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record of online_players (0x007219FC, vtable 0x006F2EDC).
class OnlinePlayers {
public:
    // 0x00479CF0
    void enter(OnlineHost& host, ui::Screen& s);
    // 0x00479EB0
    void leave(ui::Screen& s);
    // 0x00479F80
    void update(OnlineHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x004791B0: a user (none: nothing) not yet among the names (vt+0x18; Stock problem: one already there keeps the
    // reference its caller gave it) added to m_player_name: its nick (vt+0x1C) cut to the names' width less 10 pixels
    // (0x005A8F50; the width to the nearest with SSE2, 0x0079E5F8) with the user as the item's value; with the friends
    // an empty item on m_friend_icon, its icon "live_friendinvite_rec" for a request from the friend (vt+0x3C),
    // "live_friendinvite_sent" for ours (vt+0x40), else "live_friendonline" online (vt+0x50); the friend's reference
    // let go.
    void add(ui::Screen& s, int32_t user);
    // 0x004794A0, with the network game and the user object: m_player_name, m_friend_icon and m_voice_icon (all three
    // there, else nothing): the names' users let go (vt+0x14) and the three emptied (0x005AEEF0); each player of
    // another machine (its user referenced, 0x004EA100) and each recent player (referenced) added (0x004791B0); the
    // three menus' first item highlighted (with their help tickers); the user highlighted before (+0x10) highlighted
    // again where its name is (a place past the end: the first).
    void fillList(OnlineHost& host, ui::Screen& s);
    // 0x00479B40: with the statistics ([0x0075F030]) the highlighted user's record (0x004EDFD0) shown (showRecord).
    void stats(ui::Screen& s);

    float m_timer = 0.0f;     // +0x0C: the list made again each second
    int32_t m_selected = -1;  // +0x10: the user highlighted (referenced)
};

}  // namespace oj::frontend
