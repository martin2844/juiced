#pragma once
// The fixed build's controls (docs/fixes.md Added): JuicedConfigExt's controller layout, natively, in place of
// DirectInput's game controllers. Its core (external/JuicedConfigExt/src/Shared, built as third_party/juicedconfigext's
// library) as its plugin uses it: the controls file takes the layout's controls (ControlsFile: the menus a
// controller's GAMEPAD_* controls and the second keys of the keyboard layout, the driving map one JCE_<ACTION> control
// a driving action, its keyboard actions taken out for the keyboard layout), and the device reads these controls
// (PadActions: a JCE_<ACTION> the strongest of its controller input, in controller.ini's layout for the kind of
// controller in use, and its keys); the game's bindings make the actions of them as of its own controls. The layout
// of all profiles is %APPDATA%\THQ\Juiced\JuicedConfigExt\controller.ini (Game/Platform/Pads). The steering wheels of
// its [WheelMap] act on the same actions (WheelReader, Game/Platform/WheelInput): the strongest of the controller, the
// keys and the wheel counts.

#include <ControllerLayout.h>
#include <PadActions.h>

#include <array>
#include <string>
#include <string_view>

namespace WheelReader {
class Reader;
}

namespace oj::input {

// The controller as the platform read it (Game/Platform/Pads: JuicedConfigExt's PadReader).
struct PadState {
    bool connected = false;
    bool playStation = false;  // a PlayStation controller (SDL's kind): the auto layout is the PlayStation 2 one
    PadActions::Pad pad;       // its buttons and its axes past their dead zones
};

class PadControls {
public:
    using Control = PadActions::Control;

    // The control of a semantic (any case): GAMEPAD_*, JCE_<ACTION>, JCE_MENU_<ACTION>; none for another.
    static const Control* find(std::string_view semantic);

    // [Controller] enabled: the controller read (else the keyboard layout alone).
    void setControllerEnabled(bool on) { m_controllerOn = on; }
    bool controllerEnabled() const { return m_controllerOn; }
    void setLayout(const ControllerLayout::Layout& layout);
    // The game's keyboard layout: the controls file's DRIVER1 keyboard actions (mergeLayout).
    void setGameKeys(const ControllerLayout::KeySlot& keys);
    // The wheels' reader (none: no wheel) and [Wheel] linearInput: the steering wheel's STEERING past the response
    // table (the curve's flag [0x0079E89C] set: its value x 0.01).
    void setWheel(WheelReader::Reader* wheel) { m_wheel = wheel; }
    void setLinearSteering(bool on) { m_linearSteering = on; }
    bool linearSteering() const { return m_linearSteering; }

    // A poll: the controller and the keys down (DirectInput keyboard codes, each key's place as the game reads it),
    // the time on by dt seconds (the analogue values' Attack and Decay).
    void update(const PadState& pad, const std::array<bool, 256>& keys, float dt);

    // PadActions' IsDown and Value of the control now (a JCE_<ACTION>'s analogue value moving on by the time since it
    // was read before).
    bool down(const Control& control);
    float value(const Control& control);
    // A or Start down, or the wheels' Enter (WheelReader's ConfirmDown: MENU_SELECT's input, else any button): the
    // controller's input as Return is the keyboard's (the player's device taken, the movies).
    bool confirmDown() const;
    // The layout's kind in use: the PlayStation 2 one (auto with a PlayStation controller).
    bool playStation() const { return m_kind == 1; }
    // Whether STEERING's last value (JCE_STEERING) was the controller's: not 0, stronger than the keys and the wheel
    // (PadActions::FromController; DriverControl's Xbox steering).
    bool steeringFromController() const;

private:
    void resolve();

    PadActions::Actions m_actions;
    ControllerLayout::Layout m_layout;
    ControllerLayout::KeySlot m_gameKeys;
    bool m_controllerOn = true;
    bool m_dirty = true;
    int m_kind = -1;
    PadState m_pad;
    WheelReader::Reader* m_wheel = nullptr;
    bool m_linearSteering = false;
    std::array<bool, 256> m_keys{};
    double m_seconds = 1.0;  // the time of the polls (from 1: PadActions takes 0 for none)
};

// The controls file's text with the layout's controls (ControlsFile: WithoutDriverKeys, then Merge of DefaultLayout):
// DRIVER1's keyboard actions to `gameKeys`; without the controller the menus take the second keys alone.
std::string mergeLayout(std::string_view controls, bool controller, ControllerLayout::KeySlot& gameKeys);

}  // namespace oj::input
