// Run manually on a real GL driver: deterministic regression without game assets.
#include "Linux/Render/GlesDevice.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstring>
#include <future>
#include <iostream>
#include <stdexcept>
using namespace oj::render;
int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    auto* window = SDL_CreateWindow("OpenJuice GL regression", 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) return 1;
    int result = 0;
    try {
        DeviceDesc desc; desc.window = reinterpret_cast<HWND>(window);
        GlesDevice device(desc);
        const uint32_t ps[] = {0xffff0101, 66, 0xb00f0000, 1, 0x800f0000, 0xb0e40000, 0xffff};
        auto shader = device.createPixelShader(ps);
        const uint32_t colors[] = {0xffff0000,0xff00ff00,0xff0000ff,0xffffff00,0xffff00ff,0xff00ffff};
        const auto cube = device.createCubeTexture({PixelFormat::A8R8G8B8,1,1,1,reinterpret_cast<const uint8_t*>(colors),sizeof(colors)});
        const uint32_t ordinary = 0xff204080;
        auto pending = std::async(std::launch::async, [&] {
            return device.createTexture({PixelFormat::A8R8G8B8,1,1,1,reinterpret_cast<const uint8_t*>(&ordinary),4});
        });
        while (pending.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) device.letLoadingRun();
        const auto flat = pending.get();
        const float directions[][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        auto sample = [&](TextureId texture, const float* direction, uint32_t expected) {
            device.beginFrame();
            device.clear(3, 0, 1, 0);
            device.setRenderState(RenderState::AlphaBlendEnable, 0);
            device.setRenderState(RenderState::AlphaTestEnable, 0);
            device.setTexture(0, texture);
            device.setPixelShader(shader);
            device.setFvf(0x104 | (1u << 16)); // XYZRHW, TEX1, three coordinates
            struct V {float x,y,z,rhw,u,v,w;};
            V vertices[]={{0,0,0,1},{64,0,0,1},{0,64,0,1},{64,64,0,1}};
            for(auto& v:vertices){v.u=direction[0];v.v=direction[1];v.w=direction[2];}
            device.drawUp(5,2,vertices,sizeof(V));
            std::vector<uint8_t> pixels; uint32_t pitch;
            if(!device.readTarget(0,pixels,&pitch)) throw std::runtime_error("readback failed");
            uint32_t actual; std::memcpy(&actual,pixels.data()+32*pitch+32*4,4);
            if(actual!=expected) throw std::runtime_error("cube/2D pixel mismatch: " + std::to_string(actual) + " != " + std::to_string(expected));
            device.endFrame();
        };
        for (unsigned i=0;i<6;++i) { sample(cube,directions[i],colors[i]); sample(flat,directions[i],ordinary); }
        device.destroyTexture(cube); device.destroyTexture(flat);
        std::cout << "PASS: six cube faces, repeated cube/2D shader switching, loading-thread texture creation\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; result=1; }
    SDL_DestroyWindow(window); SDL_Quit(); return result;
}
