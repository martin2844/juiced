#pragma once
// What the custom race's windows ask of the front end besides what every window does: the network game's waits and
// keys in a window (NetworkHost), and the second player's device.

#include "Game/Frontend/Network/NetworkHost.h"

#include <cstddef>
#include <cstdint>

namespace oj::frontend {

class CustomRaceHost : public virtual NetworkHost {
public:
    // A player's event this frame (0 the first, 1 the second).
    virtual uint32_t playerEvent(size_t player) const = 0;
    // The second player's device present (the input manager's +0x2C not -1).
    virtual bool secondPlayer() const = 0;
    // The second player's device let go next frame (the input manager's +0x2C -1: 0x0046CAA0, 0x0045E490).
    virtual void askSecondPlayerRelease() = 0;
    // The second player's device asked for next frame (the input manager's vt+0x3C with 1: split_selcar, 0x0046DA60).
    virtual void askSecondPlayerTake() = 0;
    // The front end's +0x20 and +0x24 cleared: the second player's UI map not registered, a note of its controller as
    // if up (split_selcar's BACK).
    virtual void forgetSecondPlayerUi() = 0;
    // The input manager's devices ([G+0x18]'s list, each counted by its +0x20: the keyboard's 1).
    virtual size_t inputDevices() const = 0;
};

}  // namespace oj::frontend
