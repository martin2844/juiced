#pragma once
// What the career's windows ask of the front end besides what every window does.

#include "Core/Cars/Car.h"
#include "Game/Frontend/WindowHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <string>

namespace oj::frontend {

class CarStats;
class HostMeeting;

class CareerHost : public virtual WindowHost {
public:
    // A crew's respect change shown in CMSRespectMsg (the record 0x007227E8): crew, its respect before, the change,
    // the comment.
    virtual void respectMessage(int crew, float respect, float change, const std::wstring& comment) = 0;
    // The car Car_Stats2purple shows ([0x00728C60]) and whether it is of the player's garage (the window's +0x10,
    // [0x00721F30]).
    virtual void statsCar(Car* car, int32_t garage) = 0;
    // The record of Car_Stats2purple, whose garage flag the windows that open it set.
    virtual CarStats& carStatsPurple() = 0;
    // 0x004BAEE0: the career's next window after a race.
    virtual void afterRaceResults() = 0;
    // The dealer after the first event (as in 0x004BAEE0): the profile's +0x4E4 the money, the dealer's offers and
    // CMSCarDealership.
    virtual void firstEventDealer() = 0;
    // 0x004B60F0: the phone's frame in a career window (a call rings in, CMSMobilePhone over it).
    virtual void phoneFrame(ui::Screen& screen) = 0;
    // The front end's +0x38: its players' controllers watched (the way into the career clears it, 0x0043542B).
    virtual void watchControllers(bool on) = 0;
    // 0x0040B600 on the speech object when there is one: its line stopped (the system stepped), nothing waiting.
    virtual void stopSpeech() = 0;
    // The hosted meeting's windows (the calendar asks for the setup's defaults).
    virtual HostMeeting& hostMeeting() = 0;
    // 0x004C27A0: to the meeting of the event chosen (the days passed to it, the cars it takes), cmsAtMeeting.
    virtual void goToMeeting() = 0;
    // The phone's window (CMSMobilePhone) when it is open over another, else none.
    virtual ui::Screen* phoneScreen() = 0;
    // The front end's black over the whole screen (+0x10C, 0 .. 1; it fades out by the frame time).
    virtual void setScreenFade(float fade) = 0;
};

}  // namespace oj::frontend
