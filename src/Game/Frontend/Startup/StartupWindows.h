#pragma once
// The startup windows (docs/spec/02-frontend.md, "Startup").

#include "Game/Frontend/Startup/StartupHost.h"
#include "Game/Media/Movies.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

// disclaimer: its boxes for 5 s; no state of its own (the window's countdown).
class Disclaimer {
public:
    // 0x00466D00
    void enter(StartupHost& host, ui::Screen& s);
    void update(StartupHost& host);
};

// legals: the notes for 9 s (a press after 5); no state of its own.
class Legals {
public:
    // 0x00466DF0
    void enter(StartupHost& host, ui::Screen& s);
    void update(StartupHost& host, bool pressed);
};

// splash: the THQ movie.
class Splash {
public:
    // 0x00466840
    void enter(ui::Screen& s);
    // 0x004668C0
    void update(StartupHost& host, ui::Screen& s, bool pressed);

private:
    float m_time = 0.0f;      // +0x0C: the time since a press
    bool m_pressed = false;   // +0x10: a press
};

// juicesplash: the Juice movie; no state of its own.
class JuiceSplash {
public:
    void enter(ui::Screen& s);
    // 0x00466C30
    void update(StartupHost& host, ui::Screen& s, bool pressed);
};

// skippable: the game's opening movie.
class Skippable {
public:
    // 0x00466A40
    void enter(StartupHost& host, ui::Screen& s);
    // 0x00466AD0
    void update(StartupHost& host, ui::Screen& s, bool pressed);

private:
    bool m_skipHeld = false;  // +0x08: the press that opened it still held
};

// start: the title (PRESS ENTER), the attract replay after 100 s.
class Title {
public:
    // 0x00467060
    void enter(StartupHost& host, ui::Screen& s);
    // 0x00467260
    void update(StartupHost& host, bool playerTaken);

private:
    float m_attractTime = 0.0f;  // start's +0x10: the time on the title (0x00467260)
};

// The interface's movie when it is SPLASH's (its name that of the movie playing, case aside); none otherwise.
const movie::Player* splashMovie(ui::Screen& s);

}  // namespace oj::frontend
