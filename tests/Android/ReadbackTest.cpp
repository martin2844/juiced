#include "Android/Render/GlesDevice.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    auto *window = SDL_CreateWindow("Deferred readback test", 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window)
        return 1;
    int result = 0;
    try {
        oj::render::GlesDevice device({reinterpret_cast<HWND>(window)});
        auto check = [&](unsigned width, unsigned height, uint32_t colour) {
            const auto target = device.createRenderTarget(width, height, false);
            device.setRenderTarget(target, 0);
            device.clear(1, colour, 1, 0);
            std::vector<uint8_t> pixels;
            uint32_t pitch = 0;
            bool ready = false;
            for (int attempt = 0; attempt < 120 && !ready; ++attempt) {
                ready = device.readTargetDeferred(target, pixels, &pitch);
                device.endFrame();
                SDL_Delay(1);
            }
            if (!ready || pitch != width * 4 || pixels.size() != size_t(width) * height * 4)
                throw std::runtime_error("Deferred read dimensions/readiness");
            for (size_t i = 0; i < pixels.size(); i += 4)
                for (unsigned channel = 0; channel < 4; ++channel)
                    if (pixels[i + channel] != ((colour >> (channel * 8)) & 255))
                        throw std::runtime_error("Deferred BGRA pixels");
            // A synchronous read must remain valid with no pixel pack buffer bound.
            if (!device.readTarget(target, pixels, &pitch))
                throw std::runtime_error("Synchronous read");
            device.setRenderTarget(0, 0);
            device.destroyTexture(target);
        };
        check(128, 64, 0xffe04020);
        check(32, 16, 0xff2070e0);
        std::cout << "Deferred readback, target replacement, dimensions and BGRA passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
