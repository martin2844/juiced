// CMSCalendarHelp: the calendar's help over it.

#include "Game/Frontend/Career/CalendarHelp.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <iterator>

namespace oj::frontend {

// CMSCalendarHelp (0x004C2980, over CMSCalendar): Back_Text "OK" (line 161); OPENEVENTS, ATTENDEVENTS,
// CLOSEDEVENTS, SPECIALEVENTS, PAYDAY and FLYERTEXT over several lines.
void CalendarHelp::enter(CareerHost& host, ui::Screen& s) {
    const TextTable& texts = host.gameTexts();
    s.setText("Back_Text", texts.at(kTextOk));
    for (const char* name : {"OPENEVENTS", "ATTENDEVENTS", "CLOSEDEVENTS", "SPECIALEVENTS", "PAYDAY", "FLYERTEXT"}) {
        s.setMultiline(name, true);
    }
}

// 0x004C2A60: SELECT (CLICK) takes the window off (mode 6).
void CalendarHelp::update(CareerHost& host, uint32_t event) {
    if (event != input::kSelect) return;
    host.sound("CLICK");
    host.request("", SwitchMode::Pop);
}

}  // namespace oj::frontend
