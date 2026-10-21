#include "Render/Gl.h"
#include <SDL3/SDL.h>

#include "Core/Base/Log.h"
#include "Core/Data/GameData.h"
#include "Gamepad.h"
#include "TouchControls.h"
#include <cstdlib>
#include <filesystem>
#include <future>
#include <string>

int runDiagnostics(const std::filesystem::path &support, const std::filesystem::path &game) {
    setenv("JUICED_SUPPORT_DIR", support.c_str(), 1);
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        return 1;
    oj::log::open(support / "Juiced-android.log");
#ifdef __ANDROID__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
    SDL_Window *window =
        SDL_CreateWindow("Juiced controls check", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if (!window) {
        SDL_Quit();
        return 1;
    }
    auto context = SDL_GL_CreateContext(window);
    std::string gpuStatus = context ? std::string("Graphics: ") + (const char *)glGetString(GL_VERSION)
                                    : std::string("OpenGL unavailable: ") + SDL_GetError();
    oj::log::info("{}", gpuStatus);
    if (context)
        SDL_GL_DestroyContext(context);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, 960, 540, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    auto load = std::async(std::launch::async, [game] {
        try {
            oj::CrtRandom random;
            oj::GameData data(game);
            std::string error;
            if (!data.load(random, &error))
                return "Data load failed: " + error;
            return "Native engine loaded " + std::to_string(data.defs.cars.size()) + " car definitions.";
        } catch (const std::exception &e) {
            return std::string("Data load failed: ") + e.what();
        }
    });
    std::string status = "Loading game archives through the native engine...";
    oj::android::TouchControls touch;
    SDL_Gamepad *pad = nullptr;
    auto findGamepad = [&] { oj::android::connectGamepad(pad); };
    findGamepad();
    bool running = true, foreground = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            if (event.type == SDL_EVENT_GAMEPAD_ADDED)
                findGamepad();
            if (event.type == SDL_EVENT_GAMEPAD_REMOVED && pad && event.gdevice.which == SDL_GetGamepadID(pad)) {
                SDL_CloseGamepad(pad);
                pad = nullptr;
                findGamepad();
            }
            if (event.type == SDL_EVENT_KEY_DOWN && (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_AC_BACK))
                running = false;
            if (event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_MOTION) {
                SDL_ConvertEventToRenderCoordinates(renderer, &event);
                touch.update(event.tfinger.fingerID, event.tfinger.x / 960.f, event.tfinger.y / 540.f);
            }
            if (event.type == SDL_EVENT_FINGER_UP || event.type == SDL_EVENT_FINGER_CANCELED)
                touch.release(event.tfinger.fingerID);
            if (event.type == SDL_EVENT_DID_ENTER_BACKGROUND || event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                touch.clear();
                foreground = false;
            }
            if (event.type == SDL_EVENT_DID_ENTER_FOREGROUND || event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
                foreground = true;
        }
        if (load.valid() && load.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            status = load.get();
            oj::log::info("{}", status);
        }
        if (!foreground) {
            SDL_Delay(100);
            continue;
        }
        SDL_SetRenderDrawColor(renderer, 12, 17, 25, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 235, 241, 248, 255);
        SDL_RenderDebugText(renderer, 200, 30, "JUICED ANDROID - NATIVE ENGINE CHECK");
        SDL_RenderDebugText(renderer, 200, 70, "Test controls and game data.");
        SDL_RenderDebugText(renderer, 24, 110, gpuStatus.c_str());
        SDL_RenderDebugText(renderer, 24, 150, status.c_str());
        SDL_RenderDebugText(renderer, 24, 200, "Touch the controls below to check multitouch. Android Back exits.");
        auto held = touch.held();
        if (pad) {
            float steer = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32768.f;
            float throttle = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.f;
            float brake = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.f;
            held[0] = held[0] || steer < -.2f || SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
            held[1] = held[1] || steer > .2f || SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
            held[2] = held[2] || throttle > .1f;
            held[3] = held[3] || brake > .1f;
            held[4] = held[4] || SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH) ||
                      SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START);
            SDL_RenderDebugText(renderer, 24, 240, SDL_GetGamepadName(pad));
            SDL_RenderDebugTextFormat(renderer, 24, 270, "Stick %.2f   RT %.2f   LT %.2f", steer, throttle, brake);
        } else
            SDL_RenderDebugText(renderer, 24, 240, "No gamepad detected. Touch controls are available.");
        struct Button {
            float x, y, w, h;
            const char *name;
            int key;
        };
        Button buttons[] = {{0, 351, 173, 189, "LEFT", 0},       {173, 351, 173, 189, "RIGHT", 1},
                            {787, 351, 173, 189, "THROTTLE", 2}, {614, 351, 173, 189, "BRAKE", 3},
                            {787, 0, 173, 108, "SELECT", 4},     {0, 0, 173, 108, "BACK", 5}};
        for (auto &b : buttons) {
            SDL_SetRenderDrawColor(renderer, held[b.key] ? 225 : 45, held[b.key] ? 100 : 60, 60, 255);
            SDL_FRect rect{b.x + 4, b.y + 4, b.w - 8, b.h - 8};
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDebugText(renderer, b.x + 16, b.y + b.h / 2, b.name);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    if (load.valid())
        load.wait();
    if (pad)
        SDL_CloseGamepad(pad);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    oj::log::close();
    SDL_Quit();
    return 0;
}
