#pragma once
// What the main menu asks of the front end besides what every window does.

#include "Game/Frontend/WindowHost.h"

namespace oj::frontend {

class MainMenuHost : public virtual WindowHost {
public:
    // "Quit to Windows" answered yes: the application's method +0x40 (0x00513C50) clears its +0x14 bit 0 and the
    // game's loop (0x0040CCA0) ends.
    virtual void quitGame() = 0;
};

}  // namespace oj::frontend
