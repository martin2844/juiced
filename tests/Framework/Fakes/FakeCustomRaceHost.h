#pragma once
// A custom race's front end for the tests (the custom race windows' CustomRaceHost): off the network, one input
// device, no second player; netKeys counted and never taking an event.

#include "Framework/Fakes/FakeWindowHost.h"

#include "Core/Network/NetSession.h"
#include "Game/Frontend/CustomRace/CustomRaceHost.h"

namespace oj::test {

class FakeCustomRaceHost : public FakeWindowHost, public frontend::CustomRaceHost {
public:
    int netKeysAsked = 0;

    bool netWait(net::SessionStage, bool) override { return false; }
    bool netKeys(uint32_t, net::SessionStage, bool) override {
        ++netKeysAsked;
        return false;
    }
    void netCountdown() override {}
    bool onlineCareer() const override { return false; }
    void onlineCareerRaceSetup() override {}
    void netHook() override {}
    bool netDropped() override { return false; }
    bool onlineTopMoving() const override { return false; }
    bool onlineJoinSession(int32_t) override { return false; }
    void delayGridWalkTimeout() override {}
    bool racePause() const override { return false; }
    void netFollow() override {}
    void onlineHostIntoLobby() override {}
    uint32_t playerEvent(size_t) const override { return 0; }
    bool secondPlayer() const override { return false; }
    void askSecondPlayerRelease() override {}
    void askSecondPlayerTake() override {}
    void forgetSecondPlayerUi() override {}
    size_t inputDevices() const override { return 1; }
};

}  // namespace oj::test
