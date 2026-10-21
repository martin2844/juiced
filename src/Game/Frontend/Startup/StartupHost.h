#pragma once
// What the startup windows ask of the front end besides what every window does: the window's countdown, the keyboard's
// first player let go, the players' UI maps and the controllers watched.

#include "Game/Frontend/WindowHost.h"

namespace oj::frontend {

class StartupHost : public virtual WindowHost {
public:
    // The window's countdown (the front end's: 0 at each window's enter, run down by the frame time each frame).
    virtual float windowTimer() const = 0;
    virtual void setWindowTimer(float seconds) = 0;
    // The keyboard's first player let go this frame (0x0058FEA0).
    virtual void releaseInput() = 0;
    // The front end's +0x18 .. +0x24 0: the players' UI maps to be registered, their controllers' notes counted as up.
    virtual void forgetPlayersUi() = 0;
    // The front end's +0x38: its players' controllers watched (start's way on to the driver sets it, 0x00467350).
    virtual void watchControllers(bool on) = 0;
};

}  // namespace oj::frontend
