// Pixel regressions on a real native GPU, independent of game data and saves.
#if defined(OJ_MACOS)
#include "Mac/MetalDevice.h"
using Device = oj::render::MetalDevice;
#elif defined(OJ_ANDROID)
#include "Android/Render/GlesDevice.h"
using Device = oj::render::GlesDevice;
#else
#include "Linux/Render/GlesDevice.h"
using Device = oj::render::GlesDevice;
#endif
#include "Render/NativeHalf.h"
#include <BrightnessCurve.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace oj::render;
namespace {
constexpr uint32_t W=128,H=96;
void require(bool ok,const char* reason) { if(!ok) throw std::runtime_error(reason); }
struct Pixels {
    std::vector<uint8_t> bytes; uint32_t pitch;
    explicit Pixels(Device& d,TextureId id) { require(d.readTarget(id,bytes,&pitch),"readback"); }
    int channel(uint32_t x,uint32_t y,int c=2) const { return bytes.at(y*pitch+x*4+c); }
    double mean(int c=2) const {
        double sum=0; for(unsigned y=2;y<H-2;++y)for(unsigned x=2;x<W-2;++x)sum+=channel(x,y,c);
        return sum/((W-4)*(H-4));
    }
};
}
int runNativeEffectsCheck(const char* directory) {
    if(!SDL_Init(SDL_INIT_VIDEO)) {std::cerr<<SDL_GetError()<<'\n';return 1;}
#if !defined(OJ_MACOS)
#ifdef __ANDROID__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
#endif
#endif
    auto* window=SDL_CreateWindow("Native effects regression",W,H,
#if !defined(OJ_MACOS)
        SDL_WINDOW_OPENGL |
#endif
        SDL_WINDOW_HIDDEN);
    int failures=0;
    try {
        const std::filesystem::path output=directory;
        std::filesystem::create_directories(output);
        DeviceDesc desc;desc.window=reinterpret_cast<HWND>(window);
        Device d(desc);require(d.beginFrame(),"begin frame");
        const auto target=d.createRenderTarget(W,H,false);
        const auto reset=[&](uint32_t colour=0xff808080) {
            d.setRenderTarget(target,0);d.setClip(nullptr);d.setViewport({0,0,W,H,0,1});
            d.setRenderState(RenderState::ColourWriteEnable,15);d.clear(7,colour,1,0);
        };
        auto check=[&](const char* name,auto fn) {
            try {reset();fn();require(d.captureTarget(target,(output/(std::string(name)+".bmp")).wstring()),"capture");
                const auto restored=d.nativeTarget();require(restored.id==target && restored.viewport.width==W,"state restore");
                std::cout<<"PASS "<<name<<std::endl;}
            catch(const std::exception& e) {++failures;std::cerr<<"FAIL "<<name<<": "<<e.what()<<std::endl;}
        };
        std::vector<float> plane(W*H,.5f),occluder=plane;
        for(unsigned y=30;y<66;++y)for(unsigned x=45;x<83;++x)occluder[y*W+x]=.45f;
        const auto depth=d.createNativeScalarTexture(W,H,plane.data());
        const auto steps=d.createNativeScalarTexture(W,H,occluder.data());
        require(depth && steps,"scalar textures");
        check("opaque-screenshot",[&] {
            // The backbuffer alpha contains lighting/exposure data, not window
            // transparency. Exporting it creates black holes when viewers scale.
            d.setRenderTarget(0,0);d.setViewport({0,0,d.width(),d.height(),0,1});
            d.setRenderState(RenderState::ColourWriteEnable,15);
            d.clear(7,0x00a04020,1,0);
            const auto file=output/"opaque-backbuffer.bmp";
            require(d.capture(file.wstring()),"backbuffer screenshot");
            auto* loaded=SDL_LoadBMP(file.string().c_str());
            require(loaded!=nullptr,"load backbuffer screenshot");
            auto* bgra=SDL_ConvertSurface(loaded,SDL_PIXELFORMAT_BGRA32);
            SDL_DestroySurface(loaded);require(bgra!=nullptr,"convert screenshot");
            bool opaque=true,rgb=true;
            for(int y=0;y<bgra->h;++y)for(int x=0;x<bgra->w;++x) {
                const auto* b=static_cast<const uint8_t*>(bgra->pixels)+y*bgra->pitch+x*4;
                opaque &= b[3]==255;rgb &= b[0]==32 && b[1]==64 && b[2]==160;
            }
            SDL_DestroySurface(bgra);
            // Raw render-target reads must keep the shader's original alpha.
            std::vector<uint8_t> raw;uint32_t pitch=0;
            const bool kept=d.readTarget(0,raw,&pitch) && raw.size()>=4 && raw[3]==0;
            d.setRenderTarget(target,0);d.setViewport({0,0,W,H,0,1});
            require(opaque,"screenshots must export opaque alpha");
            require(rgb,"screenshot RGB must remain unchanged");
            require(kept,"screenshot must preserve GPU alpha data");
        });
        check("texcoord-alpha",[&] {
            const uint32_t vs[]={0xfffe0101,1,0xc00f0000,0x90e40000,1,0xe00f0000,0xa0e40000,0xffff};
            const uint32_t ps[]={0xffff0101,64,0xb00f0000,1,0x800f0000,0xb0e40000,0xffff};
            const VertexElement decl[]={{0,0,3,0,0,0},{255,0,17,0,0,0}};
            const float q[]={-1,1,.5f,1,1,1,.5f,1,-1,-1,.5f,1,1,-1,.5f,1};
            const auto vertex=d.createVertexShader(vs),pixel=d.createPixelShader(ps);
            const auto buffer=d.createVertexBuffer(q,sizeof(q));
            d.setVertexShader(vertex);d.setPixelShader(pixel);d.setDeclaration(d.createDeclaration(decl));
            d.setStream(buffer,16);d.setFvf(0);d.setRenderState(RenderState::CullMode,1);
            d.setRenderState(RenderState::ZEnable,0);d.setRenderState(RenderState::AlphaTestEnable,0);
            d.setRenderState(RenderState::AlphaBlendEnable,0);
            // TEXCOORD always writes w=1, regardless of the interpolator's w.
            const float uv[]={.25f,.5f,.75f,0};d.setVertexShaderConstantF(0,uv,1);d.draw(5,0,2);
            Pixels p(d,target);require(p.channel(64,48,3)==255 && std::abs(p.channel(64,48)-64)<=1,"TEXCOORD must force alpha one");
            d.setRenderState(RenderState::AlphaBlendEnable,1);d.setRenderState(RenderState::SrcBlend,5);
            d.setRenderState(RenderState::DestBlend,6);d.draw(5,0,2);
            require(std::abs(Pixels(d,target).channel(64,48)-64)<=1,"TEXCOORD alpha controls source blending");
            d.setRenderState(RenderState::AlphaBlendEnable,0);
            d.destroyShader(vertex);d.destroyShader(pixel);d.destroyBuffer(buffer);
        });
        check("signed-logarithm",[&] {
            // D3D LOG takes abs(source). GLSL/MSL log2 of a negative source is NaN.
            const uint32_t vs[]={0xfffe0101,1,0xc00f0000,0x90e40000,0xffff};
            const uint32_t ps[]={0xffff0200,0x05000051,0xa00f0000,0xbf000000,0,0,0,
                0x0200000f,0x800f0000,0xa0000000,0x0200000e,0x800f0000,0x80e40000,
                0x02000001,0x800f0800,0x80e40000,0xffff};
            const VertexElement decl[]={{0,0,3,0,0,0},{255,0,17,0,0,0}};
            const float q[]={-1,1,.5f,1,1,1,.5f,1,-1,-1,.5f,1,1,-1,.5f,1};
            const auto vertex=d.createVertexShader(vs),pixel=d.createPixelShader(ps);
            const auto buffer=d.createVertexBuffer(q,sizeof(q));
            d.setVertexShader(vertex);d.setPixelShader(pixel);d.setDeclaration(d.createDeclaration(decl));
            d.setStream(buffer,16);d.setFvf(0);d.setRenderState(RenderState::CullMode,1);
            d.setRenderState(RenderState::ZEnable,0);d.setRenderState(RenderState::AlphaTestEnable,0);
            d.setRenderState(RenderState::AlphaBlendEnable,0);d.draw(5,0,2);
            Pixels p(d,target);require(std::abs(p.channel(64,48)-128)<=1,"exp(log(-0.5)) must be 0.5");
            d.destroyShader(vertex);d.destroyShader(pixel);d.destroyBuffer(buffer);
        });
        check("unorm-blend",[&] {
            const uint32_t vs[]={0xfffe0101,1,0xc00f0000,0x90e40000,0xffff};
            const uint32_t ps[]={0xffff0200,0x02000001,0x800f0800,0xa0e40000,0xffff};
            const VertexElement decl[]={{0,0,3,0,0,0},{255,0,17,0,0,0}};
            const float q[]={-1,1,.5f,1,1,1,.5f,1,-1,-1,.5f,1,1,-1,.5f,1};
            const auto vertex=d.createVertexShader(vs),pixel=d.createPixelShader(ps);
            const auto buffer=d.createVertexBuffer(q,sizeof(q));
            d.setVertexShader(vertex);d.setPixelShader(pixel);d.setDeclaration(d.createDeclaration(decl));
            d.setStream(buffer,16);d.setFvf(0);d.setRenderState(RenderState::CullMode,1);
            d.setRenderState(RenderState::ZEnable,0);d.setRenderState(RenderState::AlphaTestEnable,0);
            d.setRenderState(RenderState::AlphaBlendEnable,1);d.setRenderState(RenderState::SrcBlend,2);
            d.setRenderState(RenderState::DestBlend,2);d.setRenderState(RenderState::BlendOp,1);
            const float negative[]={-.25f,0,0,0};d.setPixelShaderConstantF(0,negative,1);d.draw(5,0,2);
            Pixels p(d,target);require(std::abs(p.channel(64,48)-128)<=1,"negative source must not subtract on UNORM");
            d.destroyShader(vertex);d.destroyShader(pixel);d.destroyBuffer(buffer);
            d.setRenderState(RenderState::AlphaBlendEnable,0);
        });
        check("depth-bias",[&] {
            const uint32_t vs[]={0xfffe0101,1,0xc00f0000,0x90e40000,0xffff};
            const uint32_t ps[]={0xffff0200,0x02000001,0x800f0800,0xa0e40000,0xffff};
            const VertexElement decl[]={{0,0,3,0,0,0},{255,0,17,0,0,0}};
            const float q[]={-1,1,.5f,1,1,1,.5f,1,-1,-1,.5f,1,1,-1,.5f,1};
            const auto vertex=d.createVertexShader(vs),pixel=d.createPixelShader(ps);
            const auto buffer=d.createVertexBuffer(q,sizeof(q));
            d.setVertexShader(vertex);d.setPixelShader(pixel);d.setDeclaration(d.createDeclaration(decl));
            d.setStream(buffer,16);d.setFvf(0);d.setRenderState(RenderState::CullMode,1);
            d.setRenderState(RenderState::ZEnable,1);d.setRenderState(RenderState::ZWriteEnable,1);
            d.setRenderState(RenderState::ZFunc,2);d.setRenderState(RenderState::AlphaTestEnable,0);
            d.setRenderState(RenderState::AlphaBlendEnable,0);
            const float red[]={1,0,0,1},green[]={0,1,0,1};
            d.setPixelShaderConstantF(0,red,1);d.draw(5,0,2);
            d.setPixelShaderConstantF(0,green,1);d.draw(5,0,2);
            require(Pixels(d,target).channel(64,48)>250,"coplanar LESS control rejected");
            d.setRenderState(RenderState::DepthBias,std::bit_cast<uint32_t>(-.0001f));d.draw(5,0,2);
            require(Pixels(d,target).channel(64,48,1)>250,"negative depth bias wins the coplanar draw");
            d.setRenderState(RenderState::DepthBias,0);d.setRenderState(RenderState::ZEnable,0);
            d.destroyShader(vertex);d.destroyShader(pixel);d.destroyBuffer(buffer);
        });
        check("clamped-colour-input",[&] {
            const uint32_t vs[]={0xfffe0101,1,0xc00f0000,0x90e40000,1,0xd00f0000,0xa0e40000,0xffff};
            const uint32_t ps[]={0xffff0101,2,0x800f0000,0x90e40000,0xa0e40000,0xffff};
            const VertexElement decl[]={{0,0,3,0,0,0},{255,0,17,0,0,0}};
            const float q[]={-1,1,.5f,1,1,1,.5f,1,-1,-1,.5f,1,1,-1,.5f,1};
            const auto vertex=d.createVertexShader(vs),pixel=d.createPixelShader(ps);
            const auto buffer=d.createVertexBuffer(q,sizeof(q));
            d.setVertexShader(vertex);d.setPixelShader(pixel);d.setDeclaration(d.createDeclaration(decl));
            d.setStream(buffer,16);d.setFvf(0);d.setRenderState(RenderState::CullMode,1);
            d.setRenderState(RenderState::ZEnable,0);d.setRenderState(RenderState::AlphaTestEnable,0);
            d.setRenderState(RenderState::AlphaBlendEnable,0);
            const float colour[]={-.5f,.5f,1.5f,1},offset[]={.5f,.5f,.5f,.5f};
            d.setVertexShaderConstantF(0,colour,1);d.setPixelShaderConstantF(0,offset,1);d.draw(5,0,2);
            require(std::abs(Pixels(d,target).channel(64,48)-128)<=1,"legacy colour saturated before pixel math");
            d.destroyShader(vertex);d.destroyShader(pixel);d.destroyBuffer(buffer);
        });
        check("blend-factor",[&] {
            const uint32_t vs[]={0xfffe0101,1,0xc00f0000,0x90e40000,0xffff};
            const uint32_t ps[]={0xffff0200,0x02000001,0x800f0800,0xa0e40000,0xffff};
            const VertexElement decl[]={{0,0,3,0,0,0},{255,0,17,0,0,0}};
            const float q[]={-1,1,.5f,1,1,1,.5f,1,-1,-1,.5f,1,1,-1,.5f,1};
            const auto vertex=d.createVertexShader(vs),pixel=d.createPixelShader(ps);
            const auto buffer=d.createVertexBuffer(q,sizeof(q));
            d.setVertexShader(vertex);d.setPixelShader(pixel);d.setDeclaration(d.createDeclaration(decl));
            d.setStream(buffer,16);d.setFvf(0);d.setRenderState(RenderState::CullMode,1);
            d.setRenderState(RenderState::ZEnable,0);d.setRenderState(RenderState::AlphaTestEnable,0);
            d.setRenderState(RenderState::AlphaBlendEnable,1);d.setRenderState(RenderState::SrcBlend,14);
            d.setRenderState(RenderState::DestBlend,1);d.setRenderState(RenderState::BlendOp,1);
            d.setRenderState(RenderState::BlendFactor,0x80ff0000);
            const float white[]={1,1,1,1};d.setPixelShaderConstantF(0,white,1);d.draw(5,0,2);
            Pixels p(d,target);require(p.channel(64,48)>250 && p.channel(64,48,1)<2 && std::abs(p.channel(64,48,3)-128)<=1,"independent RGBA blend factor");
            d.setRenderState(RenderState::BlendFactor,0xffffffff);d.setRenderState(RenderState::SrcBlend,15);d.draw(5,0,2);
            require(Pixels(d,target).channel(64,48)<2,"inverse blend factor");
            d.setRenderState(RenderState::AlphaBlendEnable,0);
            d.destroyShader(vertex);d.destroyShader(pixel);d.destroyBuffer(buffer);
        });
        check("ambient-occlusion",[&] {
            AmbientOcclusionView v;v.range=1000;v.radius=100;v.bias=1;
            require(d.ambientOcclusion(steps,v),"AO declined");
            Pixels p(d,target);require(p.mean()<127.5 && p.mean()>70,"AO must darken occluder surroundings");
        });
        check("point-light",[&] {
            PointLightView v;v.range=1000;v.count=1;
            v.lights[0].place={0,0,400};v.lights[0].colour={.4f,0,0};
            require(d.pointLights(depth,v),"light declined");Pixels p(d,target);
            require(p.channel(64,48)>160 && std::abs(p.channel(64,48,1)-128)<2,"red light on plane");
        });
        check("volumetric-light",[&] {
            VolumetricLightView v;v.range=1000;v.count=1;v.density=1;v.metre=100;
            auto& l=v.lights[0];l.place={0,0,50};l.axis={0,0,1};l.side={1,0,0};l.up={0,1,0};
            l.reach=1000;l.colour={.5f,.1f,0};
            require(d.volumetricLight(depth,v),"scatter declined");Pixels p(d,target);
            require(p.mean()>128.5,"scatter adds light");
        });
        check("lamp-shadow",[&] {
            std::vector<float> zero(W*H,0);const auto map=d.createNativeScalarTexture(W,H,zero.data());
            LampShadowView v;v.range=1000;v.count=1;auto& l=v.lamps[0];
            l.clip={{{.001f,0,0,0},{0,.001f,0,0},{0,0,0,1}}};
            l.place={0,0,0};l.look={0,0,1};l.range=1000;
            require(d.lampShadows(depth,{map,0,0,0},v),"shadow declined");
            Pixels p(d,target);require(p.mean()<125,"shadow darkens plane");d.destroyTexture(map);
        });
        check("reflections",[&] {
            std::vector<float> road(W*H,4.5f);const auto map=d.createNativeScalarTexture(W,H,road.data());
            LightReflectionView v;v.range=1000;v.wetness=1;v.streakCount=1;
            v.streaks[0]={.5f,.15f,.08f,.7f,1,.2f,.1f,.95f};
            require(d.lightReflections(map,v),"reflections declined");
            Pixels p(d,target);require(p.mean()>128.1,"reflection contributes pixels");d.destroyTexture(map);
        });
        check("depth-dilation",[&] {
            const auto map=d.createFloatTarget(W,H);
            d.setRenderTarget(map,0);d.setViewport({0,0,W,H,0,1});d.clear(1,0xffffffff,1,0);
            d.setViewport({63,47,1,1,0,1});d.clear(1,0xff000000,1,0);
            d.setRenderTarget(target,0);d.setViewport({0,0,W,H,0,1});
            require(d.dilateDepth(map,{0,0,W,H,0,1},.1f),"dilation declined");
            Pixels p(d,map);require(p.channel(62,46)==0 && p.channel(61,45)>250,"caster grows one texel");
            d.destroyTexture(map);
        });
        check("smaa",[&] {
            reset(0xff000000);
            const DebugVertex triangle[]={{0,0,0xffffffff},{float(W),0,0xffffffff},{0,float(H),0xffffffff}};
            d.drawDebugTriangles(triangle,3);Pixels before(d,target);
            d.setAntialiasing(0,true);d.antialias();d.setAntialiasing(0,false);Pixels after(d,target);
            unsigned changed=0;for(size_t i=0;i<before.bytes.size();i+=4)changed+=before.bytes[i]!=after.bytes[i];
            require(changed>20 && changed<W*H/3,"SMAA changes edges and retains interiors");
        });
        check("cloud-map",[&] {
            const auto map=d.createRenderTarget(W,H,false);CloudView v;
            v.project[0][0]=v.project[1][2]=1.f/1000000;
            v.cover=0;require(d.cloudMap(map,v),"clear cloud map");const auto clear=Pixels(d,map).mean();
            v.cover=1;require(d.cloudMap(map,v),"covered cloud map");const auto cloudy=Pixels(d,map).mean();
            require(clear>cloudy+40,"cloud cover changes shadow map");
            v.volume=true;v.steps=8;v.lightSteps=1;
            require(d.cloudMap(map,v),"volume cloud map");require(d.cloudView(v),"volume cloud view");
            v.seconds=.016f;require(d.cloudView(v),"temporal cloud view");d.destroyTexture(map);
        });
        check("heat-haze",[&] {
            reset(0xff000000);
            const DebugVertex triangle[]={{0,0,0xffffffff},{float(W),0,0xffffffff},{0,float(H),0xffffffff}};
            d.drawDebugTriangles(triangle,3);Pixels before(d,target);
            const float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            d.setVertexShaderConstantF(18,matrix,4);
            const uint32_t shift=0xffff8080;
            const auto tex=d.createTexture({PixelFormat::A8R8G8B8,1,1,1,reinterpret_cast<const uint8_t*>(&shift),4});
            d.setTexture(0,tex);
            struct Particle {float x,y,z;uint32_t colour;float u,v;};
            const Particle q[]={{-1,1,.5f,0xff808080,0,0},{1,1,.5f,0xff808080,1,0},
                                {-1,-1,.5f,0xff808080,0,1},{1,-1,.5f,0xff808080,1,1}};
            require(d.beginHeatHaze()!=0,"begin heat haze");d.drawHeatHaze(q,1,1);d.endHeatHaze();
            Pixels after(d,target);unsigned changed=0;
            for(size_t i=0;i<before.bytes.size();i+=4)changed+=before.bytes[i]!=after.bytes[i];
            require(changed>20 && changed<W*H/3,"haze distorts scene locally");d.destroyTexture(tex);
        });
        check("brightness",[&] {
            d.setBrightness(1);d.endFrame();Pixels p(d,target);
            const auto wanted=int(std::lround(255*BrightnessCurve::Apply(128.f/255,1)));
            require(std::abs(p.channel(64,48)-wanted)<=2,"brightness curve matches upstream");
            d.setBrightness(.5f);require(d.beginFrame(),"next frame");
            d.setRenderTarget(target,0);d.setViewport({0,0,W,H,0,1});
        });
        check("half-precision",[&] {
            for(unsigned h=0;h<65536;++h) {
                const auto f=nativeFloat(uint16_t(h));if(std::isnan(f))continue;
                require(nativeHalf(f)==h,"half conversion round trip");
            }
            require(nativeHalf(1e10f)==0x7c00,"finite half overflow is infinity");
        });
        d.setRenderTarget(0,0);d.endFrame();
    } catch(const std::exception& e) {++failures;std::cerr<<e.what()<<'\n';}
    SDL_DestroyWindow(window);SDL_Quit();return failures?1:0;
}
#ifndef OJ_EFFECTS_CHECK_LIBRARY
int main(int argc,char** argv) {return runNativeEffectsCheck(argc>1?argv[1]:"native-effects-captures");}
#endif
