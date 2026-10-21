#pragma once
// CMSCalendar: the career's calendar, its days, events and flyers.

#include "Core/Career/Career.h"
#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00721CF8; kept while the game runs.
class CalendarWindow {
public:
    // 0x004C11D0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004C1320
    void update(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x004C1A20: the calendar's five weeks from its first day (the record's +0x0C): the selection, the hosting and the
    // flyer cleared (FLYER and the crew's details hidden), EnterEventText hidden when a day is highlighted,
    // HostEventText hidden, Sel_but hidden when a day is highlighted; Cash and Date; EventFee and EventPlayerFee
    // emptied; then each button D<column>-<row> its day's number, in white (odd months) or blue, grey on black for a
    // past day (a past day past the first row moves the calendar on, +0x14); an event's day by the player's entry
    // (attended white, closed dark red, attend only or not enough money yellow, open green); the advice's event's
    // button kept (+0x1C) and, with the advice just given, highlighted.
    void build(CareerHost& host, ui::Screen& s, bool today);
    // 0x004C2490: the advice's day brought into the calendar: for the hosting advice (12) the first free day
    // (0x004A6A10), else the advice's event's day; the first day moves on a week at a time until that day is within its
    // first 14 days; for hosting that day is selected.
    void toAdvice(CareerHost& host);

    int32_t m_start = 0;         // +0x0C: the first day shown (a day count)
    int32_t m_mayScroll = 0;     // +0x10: the highlight rested on the first or last row
    int32_t m_advance = 0;       // +0x14: a past day past the first row: a week on
    int32_t m_answer = -1;       // +0x18: the answer to "GO TO EVENT?"
    int m_adviceButton = -1;     // +0x1C: the advice's event's button
    int m_select = -1;           // +0x20: SELECT
    float m_x = 0.0f;            // +0x24: SELECT's place
    float m_y = 0.0f;            // +0x28
    float m_targetX = 0.0f;      // +0x2C: the highlighted button's place
    float m_targetY = 0.0f;      // +0x30
};

// 0x004B3F10: the player's own flyer (FlyerMain by the chosen logo, FlyerPlayerLogo the crew's logo).
void playerFlyer(ui::Screen& s, const Flyers& flyers, int main);
// 0x004B2E20: an event's flyer (the calendar's and the meeting's).
void eventFlyer(WindowHost& host, ui::Screen& s, Career& career, const CalendarEvent& e);
// 0x004C2570: a crew's details (none for the player's crew).
void crewDetails(WindowHost& host, ui::Screen& s, const Career& career, int crew);

}  // namespace oj::frontend
