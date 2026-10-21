// Exercise ps_1_1's runtime texture dimensionality on the actual Metal device.
#include "Mac/MetalDevice.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace oj::render;
int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_Window* window = SDL_CreateWindow("Reflection regression", 32, 32, SDL_WINDOW_HIDDEN);
    int result = 0;
    try {
        DeviceDesc desc; desc.window = reinterpret_cast<HWND>(window);
        MetalDevice device(desc);
        // vs_1_1: mov oPos, v0; mov oT1, c0
        const uint32_t vs[] = {0xfffe0101, 1, 0xc00f0000, 0x90e40000,
                              1, 0xe00f0001, 0xa0e40000, 0xffff};
        // ps_1_1: tex t1; mov r0, t1 (stage zero intentionally unused).
        const uint32_t ps[] = {0xffff0101, 66, 0xb00f0001, 1, 0x800f0000, 0xb0e40001, 0xffff};
        device.setVertexShader(device.createVertexShader(vs));
        device.setPixelShader(device.createPixelShader(ps));
        const VertexElement declaration[] = {{0,0,3,0,0,0}, {0xff,0,17,0,0,0}};
        device.setDeclaration(device.createDeclaration(declaration));
        const float vertices[] = {-1,-1,0,1, 3,-1,0,1, -1,3,0,1};
        device.setStream(device.createVertexBuffer(vertices,sizeof vertices),16);
        device.setRenderState(RenderState::CullMode,1);
        device.setRenderState(RenderState::ZEnable,0);
        device.setRenderState(RenderState::AlphaBlendEnable,0);
        const std::array<uint32_t,6> colours{0xffff0000,0xff00ff00,0xff0000ff,
                                              0xffffff00,0xffff00ff,0xff00ffff};
        auto cube=device.createCubeTexture({PixelFormat::A8R8G8B8,1,1,1,
            reinterpret_cast<const uint8_t*>(colours.data()),sizeof colours});
        uint32_t flatColour=0xff4488cc;
        auto flat=device.createTexture({PixelFormat::A8R8G8B8,1,1,1,
            reinterpret_cast<const uint8_t*>(&flatColour),4});
        auto target=device.createRenderTarget(32,32,false);
        const float directions[6][4]={{1,0,0,1},{-1,0,0,1},{0,1,0,1},
                                     {0,-1,0,1},{0,0,1,1},{0,0,-1,1}};
        // Switch dimensions repeatedly with the same shader ID to cover cached pipelines.
        for(int iteration=0;iteration<12;++iteration) {
            bool useCube=iteration%2==0;
            int face=iteration/2;
            device.setRenderTarget(target,0);
            device.clear(7,0xff000000,1,0);
            device.setTexture(1,useCube?cube:flat);
            device.setVertexShaderConstantF(0,directions[face],1);
            device.draw(4,0,1);
            std::vector<uint8_t> pixels; uint32_t pitch;
            if(!device.readTarget(target,pixels,&pitch)) throw std::runtime_error("readback failed");
            uint32_t actual;
            std::memcpy(&actual,pixels.data()+16*pitch+16*4,4);
            uint32_t expected=useCube?colours[face]:flatColour;
            if(actual!=expected) {
                std::cerr<<"sample "<<iteration<<": expected "<<std::hex<<expected<<" got "<<actual<<'\n';
                throw std::runtime_error("reflection sample mismatch");
            }
        }
        std::cout<<"Six cube faces and switching between cube/2D reflections passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; result=1; }
    SDL_DestroyWindow(window); SDL_Quit(); return result;
}
