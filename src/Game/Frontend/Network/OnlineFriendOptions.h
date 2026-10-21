#pragma once
// The friends' and players' window online_friend_options of GameSpy (docs/spec/28-network.md), over online_bg.

#include "Game/Frontend/Network/OnlineHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record of online_friend_options (0x00721760, vtable 0x006F2E5C).
class OnlineFriendOptions {
public:
    // 0x0047C0B0
    void enter(OnlineHost& host, ui::Screen& s);
    // 0x0047C670
    void leave(OnlineHost& host);
    // 0x0047C870
    void update(OnlineHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x0047C6D0 (accepting an invitation): the friend (vt+0x20); in the career with the current event's +0x03 the
    // career's +0xB0 1; their invitation dealt with (vt+0x70); a friend in the same game: the network game made when
    // there is none ([0x0075EEF0] 1), the matchmaking made anew when there is none (0x004E1A20, 0x00560D50,
    // [0x0075EEA8] 1); in a session during a race +0x40 1; everything reset (0x004EA4E0 with 1); the friend's session
    // asked (0x004E4460); +0x0C 1 and "JOINING GAME..." (type 0).
    void acceptInvite(OnlineHost& host);
    // The join of a friend's session (0x0047C6D0's and the join question's): the network game and the matchmaking made
    // when there are none (their flags), +0x40 in a session during a race, everything reset, the session asked, +0x0C 1
    // and "JOINING GAME..." (type 0).
    void joinFriendSession(OnlineHost& host, int32_t entry);

    int32_t m_searching = 0;            // +0x0C: the friend's session asked
    int32_t m_joining = 0;              // +0x10: joining it
    int32_t m_removeAnswer = -1;        // +0x14
    int32_t m_declineAnswer = -1;       // +0x18: remove the invitation
    int32_t m_voicePlaying = 0;         // +0x1C: the voice mail awaited
    int32_t m_voiceInviteAnswer = -1;   // +0x20 (never asked)
    int32_t m_blockAnswer = -1;         // +0x24
    int32_t m_addAnswer = -1;           // +0x28: our request to a temporary friend
    int32_t m_acceptInviteAnswer = -1;  // +0x2C
    int32_t m_complaintAnswer = -1;     // +0x30
    int32_t m_joinAnswer = -1;          // +0x34
    int32_t m_complaint = 0;            // +0x38
    int32_t m_mail = -1;                // +0x3C: the voice mail
    int32_t m_inRace = 0;               // +0x40: joining from a race
};

}  // namespace oj::frontend
