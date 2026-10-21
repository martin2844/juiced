#pragma once
// The DirectInput side of the input manager (0x00591050): its DirectInput, the game controllers it found (each a
// device's controller, Core/Input/Input's ControllerPort), the keyboard as far as the maps go (Core/Input/Input's
// KeyboardPort: its keys are read from the game window's messages, Platform) and the mouse it makes and never reads.

#include "Core/Input/ForceFeedback.h"
#include "Core/Input/Input.h"

#include <memory>
#include <vector>

struct HWND__;
using HWND = HWND__*;

namespace oj {

class InputDevices {
public:
    InputDevices();
    ~InputDevices();
    InputDevices(const InputDevices&) = delete;
    InputDevices& operator=(const InputDevices&) = delete;

    // 0x00591050: CoInitialize, DirectInput8Create, the game controllers attached (EnumDevices, DI8DEVCLASS_GAMECTRL,
    // 0x00591650), the mouse (GUID_SysMouse, c_dfDIMouse, cooperative level 0x0A). False: no DirectInput (the input
    // manager then keeps no device, its +0x0B flag 0x40; here one device of the keyboard alone).
    bool start(HWND window);
    // The controllers found, in the order DirectInput listed them.
    std::vector<input::ControllerPort*> controllers() const;
    // The keyboard's maps (GUID_SysKeyboard); none without DirectInput.
    input::KeyboardPort* keyboard() const;
    // 0x0059563F .. 0x0059571C: the controls' mapping window, IDirectInput8::ConfigureDevices with DICD_EDIT and no
    // callback, its parameters: no user names, the one format of the map (the configuring device's), the desktop as
    // the window, the colours text 0xFF9CC4CE, highlight, callouts and border 0xFFECEC01, the control and highlight
    // fills 0xFF18536D, the area 0xFF253E4A, no target. True when it gives DI_OK (no window since Windows Vista: an
    // error; DIMapper on Windows 7 and Wine have one). The user's map it keeps comes back with the next BuildActionMap.
    bool configure(const input::MapFormat& format);

    // 0x00591DF0, the FF manager ([0x0075F5F8]) made with a DirectInput of its own ([0x0075F748]): the first two game
    // controllers attached opened (0x00596790), its channels, each with c_dfDIJoystick2 and the cooperative level 5
    // (DISCL_EXCLUSIVE | DISCL_FOREGROUND, 0x005969E0) and its axes' range -10000 .. 10000 (0x00596A50); then the
    // wheels' groups and tables (Core/Input/ForceFeedback). Without a window (a test run) no channel.
    void startWheels(HWND window);
    // 0x00590F40 / 0x00591500: the FF manager stopped (0x00593C80), its groups' effects released, its devices and its
    // DirectInput released (0x00596CA0).
    void stopWheels();
    ff::Wheels& wheels() { return m_wheels; }

    struct State;

private:
    std::unique_ptr<State> m_state;
    ff::Wheels m_wheels;
    bool m_wheelsStarted = false;
};

}  // namespace oj
