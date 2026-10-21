#pragma once
// The friends' and players' window online_voice of GameSpy (docs/spec/28-network.md), over online_bg.

#include "Core/Network/NetSession.h"
#include "Game/Frontend/Network/OnlineHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <optional>

namespace oj::frontend {

// What the voice mail window does (its +0x10): a voice mail heard, an invitation's voice recorded, a friend request's.
enum class VoiceMailMode : int32_t { Heard = 0, Invitation = 1, FriendRequest = 2 };

// The window of the record of online_voice (0x007217A4, vtable 0x006F2E1C; the window is the voice's;
// +0x0C PROGRESS_BAR). The windows that open it set what it is for (askRequest, askMail, askInvite).
class OnlineVoice {
public:
    // 0x0047DD50
    void enter(OnlineHost& host, ui::Screen& s);
    // 0x0047E020
    void update(OnlineHost& host, ui::Screen& s, uint32_t event);
    // 0x0047DEA0
    void leave(OnlineHost& host);
    // 0x0047DC20
    void askMail(WindowHost& host, int32_t user, int32_t mail);
    // 0x0047DC60
    void askInvite(WindowHost& host, int32_t entry, int32_t user, const std::optional<net::Address>& address);
    // A friend request's voice (online_player_options sets it before opening the window).
    void askRequest() { m_mode = VoiceMailMode::FriendRequest; }

private:
    // 0x0047DCF0: a recording: with the voice and the user object its +0x70 with our first local player (referenced);
    // the progress 0, PROGRESS_BAR at 0; not recorded.
    void stop(ui::Screen& s);

    VoiceMailMode m_mode = VoiceMailMode::Heard;  // +0x10
    int32_t m_recorded = 0;      // +0x14
    int32_t m_sending = 0;       // +0x18: the invitation or request sent, "SENDING..." up
    float m_progress = 0.0f;     // +0x1C: PROGRESS_BAR's
    int32_t m_unused = -1;       // +0x20: -1 by the enter, read by nothing
};

}  // namespace oj::frontend
