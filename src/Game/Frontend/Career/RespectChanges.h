#pragma once
// CMSRespectChanges: the crews' respect changes after a race.

#include "Core/Career/Career.h"
#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00722308: the entry shown and the components of
// crews 1 .. 8 (by crew x 2 + column).
class RespectChanges {
public:
    // 0x004CF5A0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004D00F0
    void update(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004D0000
    void leave(CareerHost& host);

private:
    // A respect figure: "+n" in green above 0, else "n" in red (n cut toward zero).
    void changeText(CareerHost& host, ui::Screen& s, int element, float value, bool aboveZero);
    // The entry of the step into the record: its kind, change, crew and the crew's respect now; the crew's respect
    // becomes that plus the change (none below 0).
    void take(CareerHost& host, Career& career);
    // 0x004D1050: the entry shown: of kinds 0 .. 19 (a rise, then a fall) the fades of the other crews' rows shown,
    // type "RESPECT INCREASE" or "RESPECT DECREASE" and counter green or red (comment keeps its .ui colour), comment
    // the crew's name and what it thinks (lines 277 + kind or 287 + kind - 10), the crew's component highlighted (the
    // second for kind 13); of kinds 20 .. 27 by the crew of its +0x14 "GAINED PRIVILEGE" (269 + kind - 20) or "LOST
    // PRIVILEGE" (273 + kind - 24).
    void show(CareerHost& host, ui::Screen& s);
    // 0x004D0D40: the next entry: counter and the highlighted component emptied; an entry of kinds 0 .. 19 leaves its
    // crew's val showing the respect and its meter at it (jumping).
    void next(CareerHost& host, ui::Screen& s, Career& career);
    // 0x004D06D0: each frame of an entry: type shown, blinking (0.25 s) while it waits to be shown, with the
    // privileges' titles and numbers; then the count: of kinds 0 .. 19, once its delay is over, 0.06 of the change left
    // moves into the respect shown each frame (SWOOSH while more than 1 is left), and with less than 1 left the respect
    // is the target (the respect and the change when the count began; a fall none below 0); counter and the highlighted
    // component show the change left, val and meter the crew's respect (the meter jumping on the entry's first frame).
    void count(CareerHost& host, ui::Screen& s);

    int32_t m_step = 0;           // +0x0C: the entry of the career's +0x3C list
    int32_t m_kind = 0;           // +0x10
    float m_change = 0.0f;        // +0x14: left to count
    float m_crew = 0.0f;          // +0x18
    float m_respect = 0.0f;       // +0x1C: the crew's respect shown
    float m_target = 0.0f;        // +0x20
    bool m_first = false;         // +0x24: the meter jumps
    float m_countDelay = 0.0f;    // +0x28
    float m_showDelay = 0.0f;     // +0x2C
    float m_textDelay = 0.0f;     // +0x30
    bool m_canSkip = false;       // +0x34
    int m_highlighted = -1;       // +0x48: the component of the entry
    std::array<std::array<float, 2>, 18> m_places{};  // +0x144: the components' places (x, y)
    std::array<float, 18> m_changes{};   // +0x1D4
    float m_iconX = 0.0f;         // +0x21C: where the components fly from
    float m_iconY = 0.0f;         // +0x220
    std::array<float, 18> m_progress{};  // +0x224: 0 .. 1 on the way, below 0 not started
    float m_iconTimer = 0.0f;     // +0x270
    float m_counter = 0.0f;       // +0x274
    bool m_finished = false;      // +0x278
    bool m_done = false;          // +0x27C: every entry passed
};

}  // namespace oj::frontend
