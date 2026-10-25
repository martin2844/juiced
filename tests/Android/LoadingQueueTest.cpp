#include "Android/Render/GlesDevice.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>

int main() {
    using namespace std::chrono;
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    auto *window = SDL_CreateWindow("Loading queue test", 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) return 1;
    int result = 0;
    try {
        oj::render::GlesDevice device({reinterpret_cast<HWND>(window)});
        auto worker = std::async(std::launch::async, [&] {
            std::array<float, 12> vertices{};
            std::array<uint16_t, 3> indices{0, 1, 2};
            for (unsigned i = 0; i < 32; ++i) {
                auto vb = device.createVertexBuffer(vertices.data(), sizeof(vertices));
                auto ib = device.createIndexBuffer(indices.data(), indices.size());
                if (!vb || !ib) throw std::runtime_error("Worker buffer upload failed");
                device.updateVertexBuffer(vb, vertices.data(), sizeof(vertices));
                device.destroyBuffer(ib);
                device.destroyBuffer(vb);
            }
            device.waitForService();
        });
        unsigned services = 0;
        const auto start = steady_clock::now();
        while (worker.wait_for(milliseconds(0)) != std::future_status::ready) {
            device.letLoadingRun();
            ++services;
            SDL_Delay(16);
        }
        worker.get();
        auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start).count();
        std::cout << "161 sequential worker requests: " << services << " service frames, " << elapsed << " ms\n";
        if (services >= 80) throw std::runtime_error("Loading requests still wait one frame each");
        // Empty service must not spend the active-loading budget on ordinary frames.
        const auto idle = steady_clock::now();
        for (int i = 0; i < 100; ++i) device.letLoadingRun();
        if (steady_clock::now() - idle > milliseconds(100))
            throw std::runtime_error("Empty queue stalls rendering");
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
