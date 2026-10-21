#pragma once
// The friends' and players' window online_player_options of GameSpy (docs/spec/28-network.md), over online_bg.

#include "Game/Frontend/Network/OnlineHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record of online_player_options (0x00721748, vtable 0x006F2E9C).
class OnlinePlayerOptions {
public:
    // 0x0047A380
    void enter(OnlineHost& host, ui::Screen& s);
    // 0x0047A5C0
    void update(OnlineHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x0047A540: the complaint's kind (+0x14) and "Send complaint.
    void complaint(OnlineHost& host, int32_t kind);

    int32_t m_voiceAnswer = -1;      // +0x0C (never asked)
    int32_t m_complaintAnswer = -1;  // +0x10: "Send complaint. Are you sure?"
    int32_t m_complaint = 0;         // +0x14: its kind
};

}  // namespace oj::frontend
