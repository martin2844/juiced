#pragma once
// CMSMeetingSummary: the meeting's entries after a race.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x007222B0: the entry of the career's +0x38 list shown.
class MeetingSummary {
public:
    // 0x004CEAE0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004CECD0
    void update(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004CEC30
    void leave(CareerHost& host);

private:
    // 0x004CEE50: the entry shown: logo, leader, leaderBorder, carLogo, crewPic, crewBorder, sponsorName and
    // sponsorType hidden, then by the kind: 0x1C a prototype part won: sponsorName the supplier's UI_<name>_ON,
    // sponsorType the part kind's icon, "WON PROTOTYPE PART", the car's make and model; 0x1D mods unlocked: carLogo the
    // make's logo, "UNLOCKED NEW MODS", "Unlocked mods for " and the make and model; 0x1E a crew challenge won: leader
    // the crew leader's picture and logo the crew's logo of the flyers (career +0x00), "WON CREW CHALLENGE", "MINIMUM
    // RESPECT VALUE +50"; 0x1F and 0x20 a crewmate's skills (the crewmate of that number, of that member slot for
    // 0x20): crewPic its ui_crewmember_<number>, "DRIVER SKILLS IMPROVED" / "WORSE" and its short name with what
    // changed.
    void showEntry(CareerHost& host, ui::Screen& s);

    int32_t m_step = 0;       // +0x0C
    int32_t m_kind = 0;       // +0x10
    float m_a = 0.0f;         // +0x14
    float m_b = 0.0f;         // +0x18
    float m_c = 0.0f;         // +0x1C
    bool m_finished = false;  // +0x50
};

}  // namespace oj::frontend
