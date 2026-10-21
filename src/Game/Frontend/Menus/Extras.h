#pragma once
// The extras window (extras) and the credits (credits).

#include "Game/Frontend/WindowHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// The window of the record 0x00721484 (vtable 0x006F2630).
class Extras {
public:
    // 0x0046A920
    void enter(WindowHost& host, ui::Screen& s);
    // 0x0046AA00
    void update(WindowHost& host, ui::Screen& s, uint32_t event);

private:
    int32_t m_refresh = 0;  // +0x0C: frames before the save item's text is written
    int32_t m_answer = -1;  // +0x10: the save question's answer
};

// The credits' page (the game's lines: Juice Games 780 .., THQ 874 .., the licensing 1100 .., the music 1311 ..).
enum class CreditsPage : int32_t { JuiceGames = 0, Thq = 1, Licensing = 2, Music = 3 };

// The window of the record 0x00721558 (vtable 0x006F2600).
class Credits {
public:
    // 0x00469D80: the first page, not written.
    void enter();
    // 0x00469D90
    void update(WindowHost& host, ui::Screen& s, uint32_t event);

private:
    CreditsPage m_page = CreditsPage::JuiceGames;  // +0x0C
    int32_t m_shown = 0;  // +0x10: the page's text written
};

}  // namespace oj::frontend
