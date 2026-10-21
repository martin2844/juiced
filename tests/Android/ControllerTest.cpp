#include "Android/Gamepad.h"
#include "Core/Base/GameFiles.h"
#include "Game/Platform/InputDevices.h"
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
        desc.name = "Android port test controller";
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
        auto *selected = SDL_OpenGamepad(keyboardId);
        require(selected, SDL_GetError());
        oj::android::connectGamepad(selected);
        require(SDL_GetGamepadID(selected) == id, "Analog controller must replace D-pad-only keyboard");
        SDL_CloseGamepad(selected);
        SDL_DetachVirtualJoystick(keyboardId);
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
        input.load(oj::parseControls(std::string_view((const char *)bytes->data(), bytes->size())));
        input.setPlayerDevice(0, 0);
        input.useUi();
        auto poll = [&] {
            SDL_UpdateJoysticks();
            SDL_PumpEvents();
            input.poll({}, true, 1.f / 30);
        };
        poll();
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
        SDL_CloseJoystick(joystick);
        SDL_DetachVirtualJoystick(id);
        poll();
        require(input.analogue(1) == 0 && input.analogue(2) == 0, "Disconnect must clear triggers");
        input.poll({{200, true}}, true, .1f);
        require(input.analogue(1) > .95f, "Keyboard/touch must work without controller");
        std::cout << "Actual retail bindings: menu, steering, triggers, disconnect and keyboard passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_Quit();
    return 0;
}
