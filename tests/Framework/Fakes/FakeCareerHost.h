#pragma once
// A career's front end for the tests (the career windows' CareerHost): no respect messages, phone, speech or fade; the
// hosted meeting's windows its own, the meetings gone to counted.

#include "Framework/Fakes/FakeWindowHost.h"

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/Frontend/Career/HostMeeting.h"
#include "Game/Frontend/Garage/CarStats.h"

namespace oj::test {

class FakeCareerHost : public FakeWindowHost, public frontend::CareerHost {
public:
    frontend::HostMeeting meeting;
    frontend::CarStats stats;
    int meetings = 0;
    bool watching = true;  // the front end's +0x38: its players' controllers watched

    void respectMessage(int, float, float, const std::wstring&) override {}
    void statsCar(Car*, int32_t) override {}
    frontend::CarStats& carStatsPurple() override { return stats; }
    void afterRaceResults() override {}
    void firstEventDealer() override {}
    void phoneFrame(ui::Screen&) override {}
    void watchControllers(bool on) override { watching = on; }
    void stopSpeech() override {}
    frontend::HostMeeting& hostMeeting() override { return meeting; }
    void goToMeeting() override { ++meetings; }
    ui::Screen* phoneScreen() override { return nullptr; }
    void setScreenFade(float) override {}
};

}  // namespace oj::test
