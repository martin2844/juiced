#pragma once
// What the network game's windows ask of the front end besides what every window does: its waits, keys and countdown
// in a window, the session's hooks and loss, the matchmaking's join and the online career (the network game's own
// parts stay the front end's: docs/spec/28-network.md).

#include "Core/Network/SessionStage.h"
#include "Game/Frontend/WindowHost.h"

#include <cstdint>
#include <functional>

namespace oj::frontend {

class NetworkHost : public virtual WindowHost {
public:
    // 0x0046DFE0: the network game's wait of a window at its stage (loading: past the cars' stage the race loads);
    // true when the window is left.
    virtual bool netWait(net::SessionStage stage, bool loading) = 0;
    // The network game's keys of a window (BACK's question, the online keys), then its wait (0x0046DFE0); true when
    // the window is left.
    virtual bool netKeys(uint32_t event, net::SessionStage stage, bool loading) = 0;
    // 0x00471740: the network game's countdown (COUNTDOWN, COUNTDOWNBOX) while the stage's wait runs.
    virtual void netCountdown() = 0;
    // 0x004E1AC0: the matchmaking there and the online career ([0x0075EF88] 1).
    virtual bool onlineCareer() const = 0;
    // 0x004A44E0: the online career's race setup.
    virtual void onlineCareerRaceSetup() = 0;
    // The session's setup taken by this front end (its game data), while it lives.
    virtual void netHook() = 0;
    // 0x00471860: the session lost (the transport's state 1 .. 4): everything reset, a message up closed and a note
    // of why; true then.
    virtual bool netDropped() = 0;
    // The message (the manager's +0x44), else the network's message (+0x3C), still moving in (0x005AA120; the
    // network windows' wait before a join, 0x0046E8C2, 0x0047623B, 0x00476FA7, ..).
    virtual bool onlineTopMoving() const = 0;
    // 0x004734E0: the matchmaking's session at the position joined (the arcade's, or the career's when the driver's
    // career and garage take it); true when the join is under way.
    virtual bool onlineJoinSession(int32_t position) = 0;
    // 0x004D40B0: the network's delayed call (50 s) of the grid walk's timeout (0x004D4090), the pink slip's autosave's.
    virtual void delayGridWalkTimeout() = 0;
    // The windows run from a race's pause ([game + 0x3C]).
    virtual bool racePause() const = 0;
    // 0x0046DE30: a client goes to the window of the host's stage (link_lobby .. link_selcar, the online career's).
    virtual void netFollow() = 0;
    // 0x00473600 / 0x00473760: the session made and hosted (the arcade's or the online career's), link_lobby.
    virtual void onlineHostIntoLobby() = 0;
};

// 0x004EEB50, the voice object's +0x1C: the settings' +0x48 (the voice chat) 1 for 0 and 0 for anything else.
std::function<void(int32_t on)> voiceChatHook(GameData* data);

}  // namespace oj::frontend
