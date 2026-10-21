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
        // Upstream menu transitions need the previous frame's colour preserved.
        if (!device.beginFrame()) throw std::runtime_error("begin frame failed");
        device.clear(1,0xff123456,1,0);
        device.endFrame();
        if (!device.beginFrame()) throw std::runtime_error("second frame failed");
        std::vector<uint8_t> retained; uint32_t retainedPitch;
        if (!device.readTarget(0,retained,&retainedPitch)) throw std::runtime_error("frame readback failed");
        uint32_t retainedColour;
        std::memcpy(&retainedColour,retained.data()+16*retainedPitch+16*4,4);
        if (retainedColour != 0xff123456) throw std::runtime_error("menu colour not retained");
        device.endFrame();
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
        for(int iteration=0;iteration<24;++iteration) {
            bool useCube=iteration%2==0;
            int face=(iteration%12)/2;
            device.setMaterialClass(iteration < 12 ? UINT32_MAX : 49);
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
            if (iteration >= 12 && useCube) {
                expected = (expected & 0xff000000) | ((expected & 0x00ffffff) & 0x595959);
            }
            if(actual!=expected) {
                std::cerr<<"sample "<<iteration<<": expected "<<std::hex<<expected<<" got "<<actual<<'\n';
                throw std::runtime_error("reflection sample mismatch");
            }
        }
        // Paint calibration affects only light RGB; alpha and subsequent world draws stay intact.
        const uint32_t litVs[] = {0xfffe0101,1,0xc00f0000,0x90e40000,1,0xd00f0000,0xa0e40019,0xffff};
        const uint32_t litPs[] = {0xffff0101,1,0x800f0000,0x90e40000,0xffff};
        device.setVertexShader(device.createVertexShader(litVs));
        device.setPixelShader(device.createPixelShader(litPs));
        const float ambient[] = {0.2f,0.3f,0.4f,0.5f};
        device.setVertexShaderConstantF(25,ambient,1);
        for (unsigned material : {0u, UINT32_MAX}) {
            device.setMaterialClass(material);
            device.clear(7,0xff000000,1,0);
            device.draw(4,0,1);
            std::vector<uint8_t> pixels; uint32_t pitch;
            if (!device.readTarget(target,pixels,&pitch)) throw std::runtime_error("readback failed");
            const uint8_t* actual = pixels.data()+16*pitch+16*4;
            float gain=material==0 ? 1.65f : 1.0f;
            const int expected[] = {int(0.4f*gain*255+0.5f),int(0.3f*gain*255+0.5f),
                                    int(0.2f*gain*255+0.5f),128};
            for (int channel=0;channel<4;++channel)
                if (std::abs(int(actual[channel])-expected[channel])>1)
                    throw std::runtime_error("paint lighting or material reset mismatch");
        }
        std::cout<<"Cube/2D reflections, glass attenuation, paint lighting and material reset passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; result=1; }
    SDL_DestroyWindow(window); SDL_Quit(); return result;
}
