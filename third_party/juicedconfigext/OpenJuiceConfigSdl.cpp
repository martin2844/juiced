// OpenJuiceConfig's SDL3: the one OpenJuice links in (third_party/sdl3-3.4.16), for the settings program's controllers
// (Host::LinkedSdl).

#include "Core/Controllers.h"
#include "Core/Host.h"

#include <SDL3/SDL.h>

namespace Host
{
    namespace
    {
        bool __cdecl SetHint(const char* name, const char* value) { return SDL_SetHint(name, value); }
        bool __cdecl Init(std::uint32_t flags) { return SDL_Init(flags); }
        void __cdecl UpdateGamepads() { SDL_UpdateGamepads(); }
        std::uint32_t* __cdecl GetGamepads(int* count) { return SDL_GetGamepads(count); }
        const char* __cdecl GamepadNameForId(std::uint32_t id) { return SDL_GetGamepadNameForID(id); }
        int __cdecl GamepadTypeForId(std::uint32_t id) { return static_cast<int>(SDL_GetGamepadTypeForID(id)); }
        void* __cdecl OpenGamepad(std::uint32_t id) { return SDL_OpenGamepad(id); }
        void __cdecl CloseGamepad(void* pad) { SDL_CloseGamepad(static_cast<SDL_Gamepad*>(pad)); }
        bool __cdecl GamepadButton(void* pad, int button)
        {
            return SDL_GetGamepadButton(static_cast<SDL_Gamepad*>(pad), static_cast<SDL_GamepadButton>(button));
        }
        std::int16_t __cdecl GamepadAxis(void* pad, int axis)
        {
            return SDL_GetGamepadAxis(static_cast<SDL_Gamepad*>(pad), static_cast<SDL_GamepadAxis>(axis));
        }
        bool __cdecl RumbleGamepad(void* pad, std::uint16_t low, std::uint16_t high, std::uint32_t ms)
        {
            return SDL_RumbleGamepad(static_cast<SDL_Gamepad*>(pad), low, high, ms);
        }
        void __cdecl Free(void* memory) { SDL_free(memory); }
    }

    bool LinkedSdl(Controllers::SdlFunctions& f)
    {
        f.SetHint = &SetHint;
        f.Init = &Init;
        f.UpdateGamepads = &UpdateGamepads;
        f.GetGamepads = &GetGamepads;
        f.GetGamepadNameForID = &GamepadNameForId;
        f.GetGamepadTypeForID = &GamepadTypeForId;
        f.OpenGamepad = &OpenGamepad;
        f.CloseGamepad = &CloseGamepad;
        f.GetGamepadButton = &GamepadButton;
        f.GetGamepadAxis = &GamepadAxis;
        f.RumbleGamepad = &RumbleGamepad;
        f.Free = &Free;
        return true;
    }
}
