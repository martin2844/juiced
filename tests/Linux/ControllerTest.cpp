#include "Core/Base/GameFiles.h"
#include "Game/Platform/InputDevices.h"
#include "Linux/Gamepad.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    try {
        require(argc == 2, "Supply game-data directory");
        require(SDL_Init(SDL_INIT_GAMEPAD), SDL_GetError());
        SDL_VirtualJoystickDesc desc{};
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.name = "Linux port test controller";
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
        desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
        auto id = SDL_AttachVirtualJoystick(&desc);
        require(id != 0, SDL_GetError());
        SDL_VirtualJoystickDesc keys{};
        SDL_INIT_INTERFACE(&keys);
        keys.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        keys.name = "D-pad keyboard";
        keys.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        keys.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
        auto keyboardId = SDL_AttachVirtualJoystick(&keys);
        require(keyboardId != 0, SDL_GetError());
        auto *joystick = SDL_OpenJoystick(id);
        require(joystick, SDL_GetError());
        SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768);
        SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
        SDL_UpdateJoysticks();
        oj::InputDevices devices;
        devices.start(nullptr);
        oj::input::Input input;
        input.setControllers(devices.controllers());
        auto bytes = oj::GameFiles(argv[1]).read("scripts/controls_dx9_e.txt");
        require(bytes.has_value(), "Missing controls");
        auto maps = oj::parseControls(std::string_view((const char *)bytes->data(), bytes->size()));
        oj::linux_port::addCrewBindings(maps);
        oj::linux_port::addCrewBindings(maps);
        input.load(maps);
        auto poll = [&] {
            SDL_UpdateJoysticks();
            SDL_PumpEvents();
            input.poll({}, true, 1.f / 30);
        };
        // Normal title-screen entry has no manually assigned player or UI map.
        // Keyboard Enter must own port zero even with an idle pad and empty
        // hot-plug slots present. Otherwise the last empty slot claims it and
        // the frontend opens its blank controller-loss modal over Driver.
        const auto keyboardEntry = [&] {
            input.release();
            poll();
            input.poll({{28, true}}, true, 1.f / 30);
            poll();
            require(input.playerDevice(0) == 0,
                    "Title Enter must acquire the keyboard, not a gamepad/hot-plug slot");
            input.useUi();
            input.poll({{28, false}}, true, 1.f / 30);
            poll();
            require(input.uiFrame(1.f / 30).connected,
                    "Keyboard must stay connected when entering the driver selector");
            input.poll({{208, true}}, true, 1.f / 30);
            require(input.uiFrame(1.f / 30).event == oj::input::kDown,
                    "Keyboard must navigate the driver selector after automatic acquisition");
            input.poll({{208, false}}, true, 1.f / 30);
            input.poll({{28, true}}, true, 1.f / 30);
            require(input.uiFrame(1.f / 30).event == oj::input::kSelect,
                    "Keyboard must select a driver after automatic acquisition");
            input.poll({{28, false}}, true, 1.f / 30);
            input.release();
            poll();
        };
        keyboardEntry();
        poll();
        SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true);
        poll();
        poll();
        require(input.playerDevice(0) == 1, "Gamepad button must take the player without manual device assignment");
        input.useUi();
        SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false);
        poll();
        input.uiFrame(1.f / 30);
        SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true);
        poll();
        require(input.uiFrame(1.f / 30).event == oj::input::kSelect, "A must select");
        SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false);
        poll();
        input.uiFrame(1.f / 30);
        SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_EAST, true);
        poll();
        require(input.uiFrame(1.f / 30).event == oj::input::kBack, "B must go back");
        SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_EAST, false);
        poll();
        input.uiFrame(1.f / 30);
        SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_DOWN, true);
        poll();
        require(input.uiFrame(1.f / 30).event == oj::input::kDown, "D-pad must navigate");
        SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_DOWN, false);
        input.useDriver();
        poll();
        require(input.analogue(1) == 0 && input.analogue(2) == 0, "Released triggers must be neutral");
        SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
        poll();
        require(input.analogue(1) > .95f && input.analogue(2) == 0, "RT must accelerate independently");
        SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
        SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767);
        poll();
        require(input.analogue(2) > .95f && input.analogue(1) == 0, "LT must brake independently");
        SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, -32768);
        poll();
        require(input.analogue(0) < -.9f, "Left stick must steer left");
        SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, 2000);
        poll();
        require(input.analogue(0) == 0, "Small stick drift must be dead-zoned");
        const SDL_GamepadButton crewButtons[] = {SDL_GAMEPAD_BUTTON_DPAD_UP,   SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
                                                 SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
                                                 SDL_GAMEPAD_BUTTON_DPAD_UP,   SDL_GAMEPAD_BUTTON_DPAD_DOWN};
        for (int slot = 0; slot < 6; ++slot) {
            SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_BACK, slot >= 4);
            SDL_SetJoystickVirtualButton(joystick, crewButtons[slot], true);
            poll();
            for (int action = 0; action < 6; ++action)
                require(bool(input.digital(10 + action)) == (action == slot),
                        "Crew command must target only its teammate/direction");
            require(input.analogue(0) == 0, "Crew D-pad must not steer");
            poll();
            require(input.digital(10 + slot) == 0, "Held crew button must not repeat every frame");
            SDL_SetJoystickVirtualButton(joystick, crewButtons[slot], false);
            SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_BACK, false);
            poll();
        }
        input.useUi();
        poll();
        input.uiFrame(1.f / 30);
        oj::linux_port::controllerConfig().remap[SDL_GAMEPAD_BUTTON_SOUTH] = SDL_GAMEPAD_BUTTON_NORTH;
        SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_NORTH, true);
        poll();
        require(input.uiFrame(1.f / 30).event == oj::input::kSelect, "Remapped north button must select");
        SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_NORTH, false);
        oj::linux_port::controllerConfig().remap.clear();
        input.useDriver();
        poll();
        const auto secondId = SDL_AttachVirtualJoystick(&desc);
        require(secondId != 0, SDL_GetError());
        auto *second = SDL_OpenJoystick(secondId);
        require(second, SDL_GetError());
        poll(); // Open SDL's gamepad handle before injecting its trigger state.
        SDL_SetJoystickVirtualAxis(second, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768);
        SDL_SetJoystickVirtualAxis(second, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
        input.setPlayerDevice(1, 2);
        input.useDriver(1);
        poll();
        std::cout << "Two-pad triggers: " << input.analogue(1, 0) << ',' << input.analogue(2, 0) << " / "
                  << input.analogue(1, 1) << ',' << input.analogue(2, 1) << " connected " << input.device(2).connected
                  << " map " << input.playerFormat(1)->name << " SDL "
                  << SDL_GetGamepadAxis(SDL_GetGamepadFromID(secondId), SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) << '\n';
        require(input.analogue(1, 1) > .95f && input.analogue(2, 1) == 0, "Second pad must have independent triggers");
        require(input.analogue(1, 0) == 0 && input.analogue(2, 0) > .95f,
                "Second pad must not replace or control first pad");
        SDL_CloseJoystick(second);
        SDL_DetachVirtualJoystick(secondId);
        poll();
        poll();
        require(input.analogue(1, 1) == 0 && input.analogue(2, 0) > .95f,
                "Second-pad removal must neutralize it and preserve the first");
        const auto reconnectedId = SDL_AttachVirtualJoystick(&desc);
        second = SDL_OpenJoystick(reconnectedId);
        require(second, SDL_GetError());
        poll();
        SDL_SetJoystickVirtualAxis(second, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
        poll();
        require(input.analogue(1, 1) > .95f, "Reconnected pad must occupy the available stable slot");
        SDL_CloseJoystick(second);
        SDL_DetachVirtualJoystick(reconnectedId);
        poll();
        poll();
        SDL_CloseJoystick(joystick);
        SDL_DetachVirtualJoystick(id);
        poll();
        poll();
        require(input.analogue(1) == 0 && input.analogue(2) == 0, "Disconnect must clear triggers");
        require(!input.uiFrame(.1f).connected, "Removed controller must be reported disconnected");
        keyboardEntry(); // No driving pads left: all four hot-plug slots empty.
        input.setPlayerDevice(0, 0);
        input.useDriver();
        input.poll({{200, true}}, true, .1f);
        require(input.analogue(1) > .95f, "Keyboard device must work without controller");
        SDL_DetachVirtualJoystick(keyboardId);
        std::cout << "PASS: retail menu/driving/crew bindings, remap, two pads, disconnect/reconnect and keyboard\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_Quit();
    return 0;
}
