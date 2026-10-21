#pragma once
#include <SDL3/SDL.h>

namespace oj::android {
inline bool hasDrivingAxes(SDL_Gamepad *pad) {
    return pad && SDL_GamepadHasAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) && SDL_GamepadHasAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
}
// Android also exposes D-pad keyboards as gamepads. Prefer a real analog pad,
// including when it is connected after a keyboard-only device was selected.
inline void connectGamepad(SDL_Gamepad *&pad) {
    if (pad && !SDL_GamepadConnected(pad)) {
        SDL_CloseGamepad(pad);
        pad = nullptr;
    }
    if (hasDrivingAxes(pad))
        return;
    int count = 0;
    auto ids = SDL_GetGamepads(&count);
    for (int i = 0; i < count; ++i) {
        if (pad && ids[i] == SDL_GetGamepadID(pad))
            continue;
        auto candidate = SDL_OpenGamepad(ids[i]);
        if (!candidate)
            continue;
        if (!pad || hasDrivingAxes(candidate)) {
            if (pad)
                SDL_CloseGamepad(pad);
            pad = candidate;
            if (hasDrivingAxes(pad))
                break;
        } else
            SDL_CloseGamepad(candidate);
    }
    SDL_free(ids);
}
} // namespace oj::android
