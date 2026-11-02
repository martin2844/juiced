// Run manually on a real GL driver: deterministic regression without game assets.
#ifdef OJ_ANDROID
#include "Android/Render/GlesDevice.h"
#else
#include "Linux/Render/GlesDevice.h"
#endif
#include <SDL3/SDL.h>
#include <array>
#include <cstring>
#include <future>
#include <iostream>
#include <stdexcept>
using namespace oj::render;
int runRenderCheck() {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
#ifdef __ANDROID__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
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
        auto sample = [&](TextureId texture, const float* direction, uint32_t expected,
                          bool additive = false, uint32_t background = 0,
                          uint32_t sourceFactor = 5, uint32_t destinationFactor = 2,
                          uint32_t constant = 0xffffffff) {
            device.beginFrame();
            device.clear(3, background, 1, 0);
            device.setRenderState(RenderState::AlphaBlendEnable, additive ? 1 : 0);
            device.setRenderState(RenderState::AlphaTestEnable, 0);
            // Retail Material_ColouredSpecABlendAdditive's blend/depth state.
            device.setRenderState(RenderState::SrcBlend, sourceFactor);
            device.setRenderState(RenderState::DestBlend, destinationFactor);
            device.setRenderState(RenderState::BlendFactor, constant);
            device.setRenderState(RenderState::ZWriteEnable, additive ? 0 : 1);
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
        // Nonzero RGB with zero alpha models the compressed neon's border.
        // It must leave the background untouched; half-alpha green adds half
        // its intensity. Repeat the transition to detect stale blend state.
        const uint32_t neonPixels[] = {0x00050205, 0x80008000};
        const float uv[] = {0.5f,0.5f,0.0f};
        for (uint32_t pixel : neonPixels) {
            const auto tex = device.createTexture({PixelFormat::A8R8G8B8,1,1,1,
                reinterpret_cast<const uint8_t*>(&pixel),4});
            for (int repeat=0;repeat<2;++repeat) {
                sample(tex,uv,pixel); // opaque control: same shader/texture is observable
                sample(tex,uv,pixel==neonPixels[0] ? ordinary : 0xff208080,true,ordinary);
                sample(flat,uv,ordinary);
            }
            device.destroyTexture(tex);
        }
        // D3DBLEND_BLENDFACTOR/INVBLENDFACTOR must preserve independent ARGB
        // components, including alpha. The glow history uses constant blending.
        const uint32_t whitePixel = 0xffffffff;
        const auto white = device.createTexture({PixelFormat::A8R8G8B8,1,1,1,
            reinterpret_cast<const uint8_t*>(&whitePixel),4});
        for (uint32_t factor : {0u, 0xffffffffu, 0xffff0000u, 0x0000ffffu}) {
            sample(white,uv,factor,true,0,14,1,factor);
            sample(white,uv,~factor,true,0,15,1,factor);
            sample(white,uv,factor,true,whitePixel,1,14,factor);
            sample(white,uv,~factor,true,whitePixel,1,15,factor);
            sample(flat,uv,ordinary);
        }
        device.destroyTexture(white);
        // Buffered XYZRHW must use transformed vertices, even with a stale
        // programmable vertex shader/declaration. Depth must remain enabled.
        const uint32_t colourCode[] = {0xffff0101, 1, 0x800f0000, 0x90e40000, 0xffff};
        auto colourShader = device.createPixelShader(colourCode);
        struct ScreenVertex { float x,y,z,rhw; uint32_t colour; float uv[8]; };
        ScreenVertex quad[5]{};
        quad[1]={0,0,.25f,1,0xffff0000}; quad[2]={64,0,.25f,1,0xffff0000};
        quad[3]={0,64,.25f,1,0xffff0000}; quad[4]={64,64,.25f,1,0xffff0000};
        auto stream = device.createVertexBuffer(quad,sizeof(quad));
        device.beginFrame(); device.clear(3,0xff000000,1,0);
        device.setPixelShader(colourShader);
        device.setFvf(Fvf::XyzRhw | Fvf::Diffuse | Fvf::Tex4);
        device.setStream(stream,sizeof(ScreenVertex));
        device.setRenderState(RenderState::AlphaBlendEnable,0);
        device.setRenderState(RenderState::ZEnable,1);
        device.setRenderState(RenderState::ZWriteEnable,1);
        device.setRenderState(RenderState::ZFunc,4);
        device.setRenderState(RenderState::CullMode,1);
        device.draw(5,1,2);
        for(auto& v:quad){v.z=.75f;v.colour=0xff00ff00;}
        device.updateVertexBuffer(stream,quad,sizeof(quad));
        device.draw(5,1,2);
        std::vector<uint8_t> streamed; uint32_t streamedPitch=0;
        if(!device.readTarget(0,streamed,&streamedPitch)) throw std::runtime_error("buffered FVF readback failed");
        uint32_t streamedColour=0; std::memcpy(&streamedColour,streamed.data()+32*streamedPitch+32*4,4);
        if(streamedColour!=0xffff0000) throw std::runtime_error("buffered FVF/depth mismatch");
        device.endFrame(); device.destroyBuffer(stream);
        sample(flat,uv,ordinary); // Stream mode must not leak into DrawPrimitiveUP.
        device.destroyTexture(cube); device.destroyTexture(flat);
        // D3D9 Clear is restricted by the viewport and active scissor. Verify
        // logical top-left coordinates on both the back buffer and textures.
        const auto clearRect = [&](TextureId target) {
            const uint32_t width=device.targetWidth(target), height=device.targetHeight(target);
            device.setRenderTarget(target,0);
            device.setClip(nullptr);
            device.clear(3,0xff0000ff,1,0);
            device.setViewport({0,0,width,height/2,0,1});
            device.clear(3,0xffff0000,1,0);
            device.setViewport({0,height/2,width,height-height/2,0,1});
            device.clear(3,0xff00ff00,1,0);
            const auto pixelsMatch = [&](auto expected) {
                std::vector<uint8_t> pixels; uint32_t pitch=0;
                if(!device.readTarget(target,pixels,&pitch)) throw std::runtime_error("viewport clear readback failed");
                for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x){
                    uint32_t got=0;std::memcpy(&got,pixels.data()+y*pitch+x*4,4);
                    if(got!=expected(x,y)) throw std::runtime_error("viewport/scissor clear mismatch target="+
                        std::to_string(target)+" x="+std::to_string(x)+" y="+std::to_string(y));
                }
            };
            pixelsMatch([&](uint32_t,uint32_t y){return y<height/2?0xffff0000u:0xff00ff00u;});
            device.setViewport({0,0,width,height,0,1});
            device.clear(3,0xff0000ff,1,0);
            device.setViewport({7,9,19,23,0,1});
            ClipRect clip{12,5,30,20};device.setClip(&clip);
            device.clear(3,0xffff0000,1,0);
            pixelsMatch([](uint32_t x,uint32_t y){return x>=12&&x<26&&y>=9&&y<20?0xffff0000u:0xff0000ffu;});
            // A disjoint clip must not turn a partial clear into a full clear.
            clip={0,0,3,3};device.setClip(&clip);device.clear(3,0xffffffff,1,0);
            pixelsMatch([](uint32_t x,uint32_t y){return x>=12&&x<26&&y>=9&&y<20?0xffff0000u:0xff0000ffu;});
            device.setClip(nullptr);
        };
        device.beginFrame();
        clearRect(0);
        const auto partialTarget=device.createRenderTarget(48,40,false);
        clearRect(partialTarget);
        const auto partialCube=device.createRenderTarget(32,32,true);
        clearRect(partialCube);
        device.setRenderTarget(0,0);device.destroyTexture(partialTarget);device.destroyTexture(partialCube);
        device.endFrame();
        // Partial depth clear must preserve the neighboring player's depth.
        for(bool buffered : {false,true}) {
            device.beginFrame();device.setClip(nullptr);
            const uint32_t width=device.width(),height=device.height(),half=height/2;
            device.clear(3,0xff0000ff,.25f,0);
            device.setViewport({0,half,width,height-half,0,1});device.clear(3,0xff0000ff,1,0);
            device.setViewport({0,0,width,height,0,1});
            const float fw=static_cast<float>(width),fh=static_cast<float>(height);
            ScreenVertex depthQuad[]={{0,0,.5f,1,0xff00ff00},{fw,0,.5f,1,0xff00ff00},
                                      {0,fh,.5f,1,0xff00ff00},{fw,fh,.5f,1,0xff00ff00}};
            device.setPixelShader(colourShader);device.setFvf(Fvf::XyzRhw|Fvf::Diffuse|Fvf::Tex4);
            device.setRenderState(RenderState::AlphaBlendEnable,0);
            device.setRenderState(RenderState::ZEnable,1);device.setRenderState(RenderState::ZWriteEnable,0);
            device.setRenderState(RenderState::ZFunc,2);device.setRenderState(RenderState::CullMode,1);
            if(buffered){
                const auto vb=device.createVertexBuffer(depthQuad,sizeof(depthQuad));
                device.setStream(vb,sizeof(ScreenVertex));device.draw(5,0,2);device.destroyBuffer(vb);
            }else device.drawUp(5,2,depthQuad,sizeof(ScreenVertex));
            std::vector<uint8_t> depthPixels;uint32_t depthPitch=0;
            if(!device.readTarget(0,depthPixels,&depthPitch))throw std::runtime_error("partial depth clear readback failed");
            // Avoid rasterizer half-pixel edges; clear-rectangle boundaries were
            // checked exhaustively above without drawing geometry.
            for(unsigned y=2;y+2<height;++y)for(unsigned x=2;x+2<width;++x){
                if(y+1>=half&&y<=half+1)continue;
                uint32_t got=0;std::memcpy(&got,depthPixels.data()+y*depthPitch+x*4,4);
                if(got!=(y<half?0xff0000ffu:0xff00ff00u))throw std::runtime_error("neighbor depth clear mismatch path="+
                    std::to_string(buffered)+" x="+std::to_string(x)+" y="+std::to_string(y)+" color="+std::to_string(got));
            }
            device.endFrame();
        }
        std::cout << "PASS: six cube faces, repeated cube/2D shader switching, loading-thread texture creation\n";
        std::cout << "PASS: transparent neon border, half-alpha additive blend, opaque/additive transitions\n";
        std::cout << "PASS: source/destination constant and inverse constant ARGB blending\n";
        std::cout << "PASS: buffered XYZRHW, start vertex, diffuse colour and preserved depth test\n";
        std::cout << "PASS: viewport/scissor clear isolation on backbuffer, non-square target and cube face\n";
        std::cout << "PASS: partial clear preserves neighboring viewport depth\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; result=1; }
    SDL_DestroyWindow(window); SDL_Quit(); return result;
}
#ifndef OJ_RENDER_CHECK_LIBRARY
int main() { return runRenderCheck(); }
#endif
