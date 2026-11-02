#pragma once
// The fixed build's controller (docs/fixes.md Added): JuicedConfigExt's PadReader (an XInput controller, else one of
// SDL3, linked in from third_party/sdl3-3.4.16) and the layout of all profiles, %APPDATA%\THQ\Juiced\JuicedConfigExt\
// controller.ini: its [Layout], [Custom1..3] and [Keys1..3] (JuicedConfigExt's), its [Controller] enabled (true),
// device (the first connected), stickDeadzone (0.24), triggerDeadzone (0.12), rumble (false), rumbleStrength (100,
// 10 .. 200 %) and consoleSteering (true) as its plugin's settings; the file looked at once a second, so that a change
// applies while the game runs.

#include "Core/Input/PadControls.h"
#include "Core/Input/PadRumble.h"

#include <ControllerLayout.h>

#include <memory>

namespace oj {

class Pads {
public:
    Pads();
    ~Pads();
    Pads(const Pads&) = delete;
    Pads& operator=(const Pads&) = delete;

    // controller.ini read again when it changed (at most once a second): true when it did; the layout and the
    // settings then those of the file.
    bool refresh();
    const ControllerLayout::Layout& layout() const;
    bool controllerEnabled() const;
    // [Controller] consoleSteering: the controller steers as on the Xbox (DriverControl).
    bool consoleSteering() const;

    // The controller now (none with [Controller] enabled = false).
    input::PadState read();
    // The controller's motors (Core/Input/PadRumble's levels, each 0 .. 1) with [Controller] rumble on, else still.
    void setMotors(const input::PadRumble& rumble);

    struct State;

private:
    std::unique_ptr<State> m;
};

}  // namespace oj
