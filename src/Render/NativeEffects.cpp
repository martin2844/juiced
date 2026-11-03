#include "Render/NativeEffects.h"
#include "Core/Base/Log.h"
#include "Render/NativeCloudNoise.h"
#include <windows.h>
#include <Shaders/AmbientOcclusionShaders.h>
#include <Shaders/PointLightShaders.h>
#include <Shaders/VolumetricLightShaders.h>
#include <Shaders/LampShadowShaders.h>
#include <Shaders/LightReflectionShaders.h>
#include <Shaders/DepthDilateShaders.h>
#include <Shaders/BrightnessShaders.h>
#include <Shaders/SmaaShaders.h>
#include <Shaders/Smaa/AreaTex.h>
#include <Shaders/Smaa/SearchTex.h>
#include <Shaders/CloudShaders.h>
#include <Shaders/HeatHazeShaders.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <BrightnessCurve.h>
#include <cstring>
#include <map>
#include <stdexcept>

namespace oj::render {
namespace {
using Row = std::array<float,4>;
struct EffectQuadVertex { float x,y,z,w,u,v; };
constexpr VertexElement quadElements[]={{0,0,3,0,0,0},{0,16,1,0,5,0},{255,0,17,0,0,0}};
constexpr VertexElement rayElements[]={{0,0,3,0,0,0},{0,16,2,0,5,1},{255,0,17,0,0,0}};
struct Scratch { TextureId id=0;uint32_t w=0,h=0; };
}

class NativeEffects {
public:
    NativeRenderDevice& d;
    std::map<const void*,ShaderId> shaders;
    std::map<std::string,Scratch> targets;
    DeclarationId declaration=0,rayDeclaration=0;
    BufferId vertices=0,rayVertices=0,hazeVertices=0,hazeIndices=0;
    uint32_t hazeCapacity=0;
    TextureId area=0,search=0,noise=0,weather=0,volume=0;
    float brightness=.5f,hazeStrength=1.f;
    bool smaa=false;
    std::unique_ptr<NativeRenderDevice::SavedState> hazeState;
    NativeRenderDevice::Target hazeTarget{};
    uint32_t hazeDraws=0;
    DeclarationId hazeDeclaration=0;
    std::array<Row,3> hazeParams{};
    bool hazeDepth=false;
    struct CloudHistory { int index=-1; CloudView previous{}; std::array<float,3> centre{}; uint64_t frame=0; };
    std::map<std::string,CloudHistory> cloudHistories;
    void cloudTextures();
    std::array<Row,19> cloudConstants(const CloudView& v);
    std::string cloudKey(const NativeRenderDevice::Target& out) {
        return std::to_string(out.id)+"-"+std::to_string(out.viewport.x)+"-"+std::to_string(out.viewport.y)+"-"+
               std::to_string(out.viewport.width)+"-"+std::to_string(out.viewport.height);
    }
    explicit NativeEffects(NativeRenderDevice& device):d(device) {}
    ~NativeEffects() {
        hazeState.reset();
        for(const auto& [key,t]:targets) if(t.id) d.destroyTexture(t.id);
        for(auto t:{area,search,noise,weather,volume}) if(t) d.destroyTexture(t);
        for(const auto& [key,id]:shaders) if(id) d.destroyShader(id);
        for(auto id:{vertices,rayVertices,hazeVertices,hazeIndices}) if(id) d.destroyBuffer(id);
    }
    template<size_t N> ShaderId shader(const BYTE (&bytes)[N],bool vertex=false) {
        if(auto it=shaders.find(bytes);it!=shaders.end()) return it->second;
        static_assert(N%4==0);
        std::vector<uint32_t> words(N/4);std::memcpy(words.data(),bytes,N);
        const auto id=vertex?d.createVertexShader(words.data()):d.createPixelShader(words.data());
        if(!id) throw std::runtime_error("Native effect shader could not be created");
        shaders[bytes]=id;return id;
    }
    TextureId target(const char* name,uint32_t w,uint32_t h,
                     NativeRenderDevice::TargetFormat format=NativeRenderDevice::TargetFormat::Colour) {
        if(!w || !h) throw std::runtime_error("Empty effect target");
        auto& t=targets[name];
        if(t.id && (t.w!=w || t.h!=h)) { d.destroyTexture(t.id);t={}; }
        if(!t.id) {
            t={d.createNativeTarget(w,h,format),w,h};
            if(!t.id) throw std::runtime_error("Native effect target unavailable");
        }
        return t.id;
    }
    void sampler(uint32_t stage,TextureId texture,bool linear=false,bool wrap=false) {
        d.setTexture(stage,texture);
        for(auto axis:{SamplerState::AddressU,SamplerState::AddressV,SamplerState::AddressW})
            d.setSamplerState(stage,axis,wrap?1:3);
        d.setSamplerState(stage,SamplerState::MinFilter,linear?2:1);
        d.setSamplerState(stage,SamplerState::MagFilter,linear?2:1);
        d.setSamplerState(stage,SamplerState::MipFilter,0);
        d.setSamplerState(stage,SamplerState::MipMapLodBias,0);
        d.setSamplerState(stage,SamplerState::MaxMipLevel,0);
    }
    void setup() {
        d.setClip(nullptr);
        for(auto state:{RenderState::ZEnable,RenderState::ZWriteEnable,RenderState::AlphaTestEnable,
                        RenderState::AlphaBlendEnable,RenderState::StencilEnable,RenderState::FogEnable})
            d.setRenderState(state,0);
        d.setRenderState(RenderState::CullMode,1);
        d.setRenderState(RenderState::ColourWriteEnable,15);
        d.setRenderState(RenderState::BlendOp,1);
        d.setMaterialClass(UINT32_MAX);
        for(uint32_t i=0;i<16;++i) d.setTexture(i,0);
    }
    void blend(bool multiply) {
        d.setRenderState(RenderState::AlphaBlendEnable,1);
        d.setRenderState(RenderState::SrcBlend,multiply?1:2);
        d.setRenderState(RenderState::DestBlend,multiply?3:2);
        d.setRenderState(RenderState::ColourWriteEnable,7);
    }
    void quad(TextureId into,const Viewport& view,ShaderId pixel,ShaderId vertex=0) {
        if(!declaration) declaration=d.createDeclaration(quadElements);
        // D3D's half-pixel shift is already accounted for by the native raster
        // convention: clip-space pixel centres meet texel centres without it.
        const EffectQuadVertex q[4]={{-1,1,.5f,1,0,0},{1,1,.5f,1,1,0},
                               {-1,-1,.5f,1,0,1},{1,-1,.5f,1,1,1}};
        if(!vertices) vertices=d.createVertexBuffer(q,sizeof(q));
        else d.updateVertexBuffer(vertices,q,sizeof(q));
        d.setRenderTarget(into,0);d.setViewport(view);
        d.setFvf(0);d.setDeclaration(declaration);d.setStream(vertices,sizeof(EffectQuadVertex));
        d.setVertexShader(vertex?vertex:shader(g_VsAmbientOcclusion,true));d.setPixelShader(pixel);
        d.draw(5,0,2);
    }
    void quad(TextureId into,uint32_t w,uint32_t h,ShaderId pixel,ShaderId vertex=0) {
        quad(into,{0,0,w,h,0,1},pixel,vertex);
    }
    template<size_t N> void constants(const std::array<Row,N>& rows) {
        d.setPixelShaderConstantF(0,rows[0].data(),N);
    }
    TextureId copyScene(const char* name,const NativeRenderDevice::Target& view) {
        const auto copy=target(name,view.viewport.width,view.viewport.height);
        if(!d.copyNativeTarget(view.id,view.face,view.viewport,copy))
            throw std::runtime_error("Cannot copy the effect's scene");
        return copy;
    }
    bool ao(TextureId depth,const AmbientOcclusionView& v) {
        if(!depth) return false;
        const auto out=d.nativeTarget();auto state=d.saveNativeState();setup();
        const auto w=d.targetWidth(depth),h=d.targetHeight(depth);
        const auto a=target("ao",w,h),b=target("ao-blur",w,h);
        const std::array<Row,5> c={Row{1.f/w,1.f/h,float(w),float(h)},
            Row{v.tanX,v.tanY,v.range,v.radius},Row{v.strength,v.bias,v.fadeFrom,v.fadeLength>0?1.f/v.fadeLength:0},
            Row{},Row{v.foliageRadius,v.foliageStrength,v.foliageFadeFrom,v.foliageFadeLength>0?1.f/v.foliageFadeLength:0}};
        constants(c);sampler(0,depth);
        quad(a,w,h,shader(g_PsAmbientOcclusionOcclude));
        Row step{1.f/w,0,0,0};d.setPixelShaderConstantF(3,step.data(),1);sampler(1,a);
        quad(b,w,h,shader(g_PsAmbientOcclusionBlur));
        step={0,1.f/h,0,0};d.setPixelShaderConstantF(3,step.data(),1);sampler(1,b);
        quad(a,w,h,shader(g_PsAmbientOcclusionBlur));
        sampler(1,a,true);blend(true);quad(out.id,out.viewport,shader(g_PsAmbientOcclusionComposite));return true;
    }
    bool point(TextureId depth,const PointLightView& v) {
        if(!depth || v.count<=0) return false;
        const auto out=d.nativeTarget();auto state=d.saveNativeState();setup();
        const auto w=d.targetWidth(depth),h=d.targetHeight(depth),t=target("point",w,h);
        const int count=std::clamp(v.count,0,8);std::array<Row,21> c{};
        c[0]={1.f/w,1.f/h,float(w),float(h)};c[1]={v.tanX,v.tanY,v.range,float(count)};
        std::copy(v.camera.begin(),v.camera.end(),c.begin()+2);
        for(int i=0;i<count;++i) {
            std::copy(v.lights[i].place.begin(),v.lights[i].place.end(),c[5+i].begin());c[5+i][3]=v.lights[i].reach;
            std::copy(v.lights[i].colour.begin(),v.lights[i].colour.end(),c[13+i].begin());c[13+i][3]=v.lights[i].falloff;
        }
        constants(c);sampler(0,depth);quad(t,w,h,shader(g_PsPointLightsShade));
        sampler(1,t,true);blend(false);quad(out.id,out.viewport,shader(g_PsPointLightsComposite));return true;
    }
    bool volumetric(TextureId depth,const VolumetricLightView& v) {
        if(!depth || v.count<=0) return false;
        const auto out=d.nativeTarget();auto state=d.saveNativeState();setup();
        const auto w=d.targetWidth(depth),h=d.targetHeight(depth),t=target("volumetric",w,h);
        const int count=std::clamp(v.count,0,24);std::array<Row,126> c{};
        c[0]={1.f/w,1.f/h,float(w),float(h)};c[1]={v.tanX,v.tanY,v.range,float(count)};
        std::copy(v.camera.begin(),v.camera.end(),c.begin()+2);c[5]={v.density,0,0,v.metre>0?1.f/v.metre:0};
        for(int i=0;i<count;++i) {
            const auto& l=v.lights[i];
            for(int axis=0;axis<3;++axis) {
                c[6+i][axis]=l.place[axis];c[30+i][axis]=l.axis[axis];
                c[54+i][axis]=l.side[axis];c[78+i][axis]=l.up[axis];c[102+i][axis]=l.colour[axis];
            }
            c[6+i][3]=l.reach;c[30+i][3]=float(std::clamp(l.steps,1,8));
            c[54+i][3]=l.forward;c[102+i][3]=l.falloff;
        }
        constants(c);sampler(0,depth);quad(t,w,h,shader(g_PsVolumetricLightScatter));
        sampler(1,t,true);blend(false);quad(out.id,out.viewport,shader(g_PsVolumetricLightComposite));return true;
    }
    bool lamps(TextureId depth,const std::array<TextureId,4>& maps,const LampShadowView& v) {
        if(!depth || v.count<=0 || !maps[0]) return false;
        const auto out=d.nativeTarget();auto state=d.saveNativeState();setup();
        const auto w=d.targetWidth(depth),h=d.targetHeight(depth),t=target("lamps",w,h);
        const int count=std::clamp(v.count,0,4);std::array<Row,26> c{};
        c[0]={1.f/w,1.f/h,float(w),float(h)};c[1]={v.tanX,v.tanY,v.range,float(count)};
        std::copy(v.camera.begin(),v.camera.end(),c.begin()+2);c[5]={v.strength,v.bias,v.texel,v.texel*2};
        for(int i=0;i<count;++i) {
            const auto& l=v.lamps[i];std::copy(l.clip.begin(),l.clip.end(),c.begin()+6+5*i);
            c[9+5*i]={l.place[0],l.place[1],l.place[2],std::clamp(l.strength,0.f,1.f)};
            c[10+5*i]={l.look[0],l.look[1],l.look[2],l.range>0?1.f/l.range:0};
        }
        constants(c);sampler(0,depth);for(int i=0;i<4;++i) sampler(1+i,i<count?maps[i]:maps[0]);
        quad(t,w,h,shader(g_PsLampShadowsShade));sampler(5,t,true);blend(true);
        quad(out.id,out.viewport,shader(g_PsLampShadowsComposite));return true;
    }
    bool reflections(TextureId depth,const LightReflectionView& v) {
        if(!depth) return false;
        const auto out=d.nativeTarget();auto state=d.saveNativeState();
        const auto scene=copyScene("reflection-scene",out);setup();
        const auto w=d.targetWidth(depth),h=d.targetHeight(depth),a=target("reflection",w,h),b=target("reflection-blur",w,h);
        std::array<Row,38> c{};const auto count=std::clamp(v.streakCount,0,16);
        c[0]={1.f/w,1.f/h,float(w),float(h)};c[1]={v.tanX,v.tanY,v.range,v.reach};
        c[2]={v.up[0],v.up[1],v.up[2],v.wetness};c[3]={v.fromDry,v.fromWet,v.strengthDry,v.strengthWet};
        c[4]={0,0,v.streak,v.roughness};c[5]={float(count),v.asphalt,v.water,0};
        for(int i=0;i<count;++i) { std::copy_n(v.streaks[i].begin(),4,c[6+i].begin());std::copy_n(v.streaks[i].begin()+4,4,c[22+i].begin()); }
        constants(c);sampler(0,depth);sampler(1,scene,true);
        quad(a,w,h,shader(g_PsLightReflectionsReflect));
        Row step{0,1.f/h,v.streak,v.roughness};d.setPixelShaderConstantF(4,step.data(),1);sampler(2,a,true);
        quad(b,w,h,shader(g_PsLightReflectionsStreak));
        step={1.f/w,0,.6f,v.roughness};d.setPixelShaderConstantF(4,step.data(),1);sampler(2,b,true);
        quad(a,w,h,shader(g_PsLightReflectionsStreak));
        sampler(2,a,true);blend(false);quad(out.id,out.viewport,shader(g_PsLightReflectionsComposite));return true;
    }
    bool dilate(TextureId map,const Viewport& region,float gap) {
        if(!map || !region.width || !region.height) return false;
        auto state=d.saveNativeState();
        const auto copy=target("dilate",region.width,region.height,NativeRenderDevice::TargetFormat::Scalar);
        if(!d.copyNativeTarget(map,0,region,copy)) return false;
        setup();sampler(0,copy);Row metrics{1.f/region.width,1.f/region.height,gap,0};
        d.setPixelShaderConstantF(0,metrics.data(),1);quad(map,region,shader(g_PsDepthDilate),shader(g_VsDepthDilate,true));return true;
    }
    void aa() {
        if(!smaa) return;
        const auto out=d.nativeTarget();if(out.viewport.width<16 || out.viewport.height<16) return;
        auto state=d.saveNativeState();const auto scene=copyScene("smaa-scene",out);setup();
        if(!area) {
            std::vector<uint32_t> rgba(AREATEX_WIDTH*AREATEX_HEIGHT);
            for(size_t i=0;i<rgba.size();++i) rgba[i]=(uint32_t(areaTexBytes[2*i+1])<<24)|uint32_t(areaTexBytes[2*i])*0x010101u;
            area=d.createTexture({PixelFormat::A8R8G8B8,AREATEX_WIDTH,AREATEX_HEIGHT,1,reinterpret_cast<const uint8_t*>(rgba.data()),rgba.size()*4});
            rgba.resize(SEARCHTEX_WIDTH*SEARCHTEX_HEIGHT);
            for(size_t i=0;i<rgba.size();++i) rgba[i]=0xff000000u|uint32_t(searchTexBytes[i])*0x010101u;
            search=d.createTexture({PixelFormat::A8R8G8B8,SEARCHTEX_WIDTH,SEARCHTEX_HEIGHT,1,reinterpret_cast<const uint8_t*>(rgba.data()),rgba.size()*4});
            if(!area || !search) throw std::runtime_error("SMAA lookup texture unavailable");
        }
        const auto w=out.viewport.width,h=out.viewport.height,a=target("smaa-edges",w,h),b=target("smaa-weights",w,h);
        Row metrics{1.f/w,1.f/h,float(w),float(h)};
        d.setVertexShaderConstantF(0,metrics.data(),1);d.setPixelShaderConstantF(0,metrics.data(),1);
        sampler(0,scene,true);d.setRenderTarget(a,0);d.setViewport({0,0,w,h,0,1});d.clear(1,0,1,0);
        quad(a,w,h,shader(g_PsSmaaEdgePS),shader(g_VsSmaaEdgeVS,true));
        sampler(0,a,true);sampler(1,area,true);sampler(2,search);
        d.setRenderTarget(b,0);d.setViewport({0,0,w,h,0,1});d.clear(1,0,1,0);quad(b,w,h,shader(g_PsSmaaWeightPS),shader(g_VsSmaaWeightVS,true));
        sampler(0,scene,true);sampler(1,b,true);sampler(2,0);
        d.setRenderState(RenderState::ColourWriteEnable,7);
        quad(out.id,out.viewport,shader(g_PsSmaaBlendPS),shader(g_VsSmaaBlendVS,true));
    }
    void brighten() {
        if(std::abs(brightness-.5f)<.001f) return;
        const auto out=d.nativeTarget();auto state=d.saveNativeState();const auto scene=copyScene("brightness",out);setup();
        sampler(0,scene,true);
        const float gamma=BrightnessCurve::Gamma(brightness);
        const Row curve{gamma,BrightnessCurve::Gain(gamma),0,0};
        d.setPixelShaderConstantF(0,curve.data(),1);quad(out.id,out.viewport,shader(g_PsBrightness));
    }
    bool cloudsMap(TextureId map,const CloudView& v);
    bool cloudsView(const CloudView& v);
    bool cloudsSky(uint32_t primitives,const CloudView& v);
    uint32_t beginHaze();
    void drawHaze(const void* data,uint32_t quads,float boost);
    void endHaze();
    bool capture(const char* name,const std::wstring& file) {
        const auto it=targets.find(name);return it!=targets.end() && d.captureTarget(it->second.id,file);
    }
};

void NativeEffects::cloudTextures() {
    if(noise) return;
    const auto& pixels=cloud_noise::Baked();
    noise=d.createTexture({PixelFormat::A8R8G8B8,256,256,1,reinterpret_cast<const uint8_t*>(pixels.data()),pixels.size()*4});
    const auto& wide=cloud_noise::BakedWeather();std::vector<float> red(wide.size());
    std::transform(wide.begin(),wide.end(),red.begin(),[](uint16_t n){return n/65535.f;});
    weather=d.createNativeScalarTexture(256,256,red.data());
    const auto& voxels=cloud_noise::BakedVolume();volume=d.createNativeVolume(64,64,64,voxels[0].data());
    if(!noise || !weather || !volume) throw std::runtime_error("Native cloud noise textures unavailable");
}

std::array<Row,19> NativeEffects::cloudConstants(const CloudView& v) {
    static const auto sorted=[] {
        std::vector<float> values;for(auto p:cloud_noise::Baked()) values.push_back(cloud_noise::ShapeOf(p));
        std::sort(values.begin(),values.end());return values;
    }();
    const auto at=std::min(sorted.size()-1,size_t((1.f-std::clamp(v.cover,0.f,1.f))*sorted.size()));
    const float threshold=v.cover>=1?sorted.front()-.01f:v.cover<=0?sorted.back()+.01f:sorted[at];
    const float ux=v.project[0][0],uz=v.project[0][2],vx=v.project[1][0],vz=v.project[1][2];
    const float det=ux*vz-uz*vx,inv=std::abs(det)>1e-20f?1.f/det:0;
    return {Row{v.height,v.fade,threshold,v.sharpness},v.project[0],v.project[1],
        Row{v.eye[0],v.eye[1],v.eye[2],v.detailScale},v.wind,
        Row{v.toSun[0],v.toSun[1],v.toSun[2],v.scatter},Row{v.sun[0],v.sun[1],v.sun[2],v.absorption},
        Row{v.ambient[0],v.ambient[1],v.ambient[2],v.opacity},Row{v.clearLight,v.cloudLight,v.sunStep[0],v.sunStep[1]},
        Row{float(v.volume),v.top,v.extinction,std::min(v.steps,64.f)},
        Row{v.farthest,v.weatherScale,v.shapeScale,v.erosionScale},Row{vz*inv,-uz*inv,-vx*inv,ux*inv},
        Row{v.shapeDrift[0],v.shapeDrift[1],v.shapeDrift[2],std::min(v.lightSteps,6.f)},
        Row{v.erosionDrift[0],v.erosionDrift[1],v.erosionDrift[2],v.overcast},Row{},Row{},Row{0,0,0,1},Row{},Row{0,0,1,1}};
}

bool NativeEffects::cloudsMap(TextureId map,const CloudView& v) {
    if(!map) return false;
    auto state=d.saveNativeState();cloudTextures();setup();constants(cloudConstants(v));
    sampler(0,noise,true,true);sampler(1,volume,true,true);sampler(2,weather,true,true);
    quad(map,d.targetWidth(map),d.targetHeight(map),v.volume?shader(g_PsCloudsMapVolume):shader(g_PsCloudsMap),shader(g_VsCloudsQuad,true));
    return true;
}

bool NativeEffects::cloudsView(const CloudView& v) {
    if(!v.volume) return false;
    const auto out=d.nativeTarget();const auto w=(out.viewport.width+1)/2,h=(out.viewport.height+1)/2;
    if(!w || !h) return false;
    auto state=d.saveNativeState();cloudTextures();setup();
    const auto key=cloudKey(out);auto& history=cloudHistories[key];
    const auto& c=v.clip;
    const float det=c[0][0]*(c[1][1]*c[2][2]-c[1][2]*c[2][1])-c[0][1]*(c[1][0]*c[2][2]-c[1][2]*c[2][0])+c[0][2]*(c[1][0]*c[2][1]-c[1][1]*c[2][0]);
    if(std::abs(det)<1e-30f) return false;
    const float inv[3][3]={
        {(c[1][1]*c[2][2]-c[1][2]*c[2][1])/det,(c[0][2]*c[2][1]-c[0][1]*c[2][2])/det,(c[0][1]*c[1][2]-c[0][2]*c[1][1])/det},
        {(c[1][2]*c[2][0]-c[1][0]*c[2][2])/det,(c[0][0]*c[2][2]-c[0][2]*c[2][0])/det,(c[0][2]*c[1][0]-c[0][0]*c[1][2])/det},
        {(c[1][0]*c[2][1]-c[1][1]*c[2][0])/det,(c[0][1]*c[2][0]-c[0][0]*c[2][1])/det,(c[0][0]*c[1][1]-c[0][1]*c[1][0])/det}};
    std::array<float,3> centre{inv[0][2],inv[1][2],inv[2][2]};
    const float length=std::max(std::sqrt(centre[0]*centre[0]+centre[1]*centre[1]+centre[2]*centre[2]),1e-20f);
    for(auto& n:centre) n/=length;
    const bool keep=history.index>=0 && std::abs(v.seconds-history.previous.seconds)<.3f &&
        centre[0]*history.centre[0]+centre[1]*history.centre[1]+centre[2]*history.centre[2]>.9f;
    const auto a=target(("cloud-view-"+key+"-0").c_str(),w,h,NativeRenderDevice::TargetFormat::Hdr);
    const auto b=target(("cloud-view-"+key+"-1").c_str(),w,h,NativeRenderDevice::TargetFormat::Hdr);
    const int next=keep?1-history.index:0;
    auto rows=cloudConstants(v);
    for(int r=0;r<3;++r) std::copy(history.previous.clip[r].begin(),history.previous.clip[r].end(),rows[14+r].begin());
    rows[16][3]=keep?.12f:1.f;rows[17][0]=float(std::fmod(history.frame*.6180339887,1.));constants(rows);
    sampler(0,noise,true,true);sampler(1,volume,true,true);sampler(2,weather,true,true);sampler(3,next?a:b,true);
    struct Ray { float x,y,z,w,rx,ry,rz; };
    Ray rays[4]={{-1,1,.5f,1},{1,1,.5f,1},{-1,-1,.5f,1},{1,-1,.5f,1}};
    for(auto& ray:rays) {
        ray.rx=inv[0][0]*ray.x+inv[0][1]*ray.y+inv[0][2];
        ray.ry=inv[1][0]*ray.x+inv[1][1]*ray.y+inv[1][2];
        ray.rz=inv[2][0]*ray.x+inv[2][1]*ray.y+inv[2][2];
    }
    if(!rayDeclaration) rayDeclaration=d.createDeclaration(rayElements);
    if(!rayVertices) rayVertices=d.createVertexBuffer(rays,sizeof(rays));else d.updateVertexBuffer(rayVertices,rays,sizeof(rays));
    d.setRenderTarget(next?b:a,0);d.setViewport({0,0,w,h,0,1});d.setFvf(0);
    d.setDeclaration(rayDeclaration);d.setStream(rayVertices,sizeof(Ray));d.setVertexShader(shader(g_VsCloudsQuadRays,true));
    d.setPixelShader(shader(g_PsCloudsViewVolume));d.draw(5,0,2);
    history.index=next;history.previous=v;history.centre=centre;++history.frame;return true;
}

bool NativeEffects::cloudsSky(uint32_t primitives,const CloudView& v) {
    if(!primitives) return false;
    const auto out=d.nativeTarget();auto state=d.saveNativeState();cloudTextures();
    auto c=cloudConstants(v);c[18]={float(out.viewport.x),float(out.viewport.y),1.f/std::max(out.viewport.width,1u),1.f/std::max(out.viewport.height,1u)};
    constants(c);sampler(0,noise,true,true);sampler(1,volume,true,true);sampler(2,weather,true,true);
    TextureId view=0;
    if(v.screen) if(auto it=cloudHistories.find(cloudKey(out));it!=cloudHistories.end() && it->second.index>=0)
        view=targets.at("cloud-view-"+cloudKey(out)+"-"+std::to_string(it->second.index)).id;
    if(view) sampler(3,view,true);
    d.setVertexShader(shader(g_VsCloudsDome,true));
    d.setPixelShader(!v.volume?shader(g_PsCloudsSky):view?shader(g_PsCloudsSkyView):shader(g_PsCloudsSkyVolume));
    d.draw(5,0,primitives);return true;
}

uint32_t NativeEffects::beginHaze() {
    if(hazeState || hazeStrength<=0) return 0;
    hazeTarget=d.nativeTarget();
    const auto w=d.targetWidth(hazeTarget.id),h=d.targetHeight(hazeTarget.id);
    if(w<16 || h<16) return 0;
    hazeState=std::make_unique<NativeRenderDevice::SavedState>(d.saveNativeState());
    const auto copy=target("haze-scene",w,h),shift=target("haze-shift",w,h);
    if(!d.copyNativeTarget(hazeTarget.id,hazeTarget.face,{0,0,w,h,0,1},copy)) { hazeState.reset();return 0; }
    hazeDepth=d.copyNativeDepth(hazeTarget.id,shift);
    d.setRenderTarget(shift,0);d.setViewport({0,0,w,h,0,1});d.clear(1,0,1,0);d.setViewport(hazeTarget.viewport);
    const float vw=float(hazeTarget.viewport.width),vh=float(hazeTarget.viewport.height);
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    hazeParams={Row{.5f,-.5f,0,0},Row{float(std::fmod(seconds,1000.)),.5f,vw/vh,.03f},Row{.03f,1.f/.03f,0,0}};
    hazeDraws=0;return hazeTarget.viewport.width;
}

void NativeEffects::drawHaze(const void* data,uint32_t quads,float boost) {
    if(!hazeState || !data || !quads) return;
    quads=std::min(quads,2048u);
    if(quads>hazeCapacity) {
        if(hazeVertices) d.destroyBuffer(hazeVertices);if(hazeIndices) d.destroyBuffer(hazeIndices);
        std::vector<uint16_t> indices(quads*6);
        constexpr uint16_t order[]={0,1,2,2,1,3};
        for(uint32_t q=0;q<quads;++q) for(int i=0;i<6;++i) indices[6*q+i]=uint16_t(q*4+order[i]);
        hazeIndices=d.createIndexBuffer(indices.data(),indices.size());
        hazeVertices=d.createVertexBuffer(data,quads*4*24);hazeCapacity=quads;
    } else d.updateVertexBuffer(hazeVertices,data,quads*4*24);
    if(!hazeDeclaration) {
        const VertexElement elements[]={{0,0,2,0,0,0},{0,12,4,0,10,0},{0,16,1,0,5,0},{255,0,17,0,0,0}};
        hazeDeclaration=d.createDeclaration(elements);
    }
    d.setRenderState(RenderState::ZEnable,hazeDepth?1:0);d.setRenderState(RenderState::ZWriteEnable,0);
    d.setRenderState(RenderState::CullMode,1);d.setRenderState(RenderState::AlphaBlendEnable,1);
    d.setRenderState(RenderState::SrcBlend,2);d.setRenderState(RenderState::DestBlend,2);
    d.setRenderState(RenderState::BlendOp,1);d.setRenderState(RenderState::AlphaTestEnable,0);
    d.setRenderState(RenderState::ColourWriteEnable,15);d.setRenderState(RenderState::FogEnable,0);
    d.setSamplerState(0,SamplerState::MipFilter,2);
    const Row params{6.f*hazeStrength*boost,50.f,600.f,0};d.setVertexShaderConstantF(90,params.data(),1);constants(hazeParams);
    d.setDeclaration(hazeDeclaration);d.setFvf(0);d.setStream(hazeVertices,24);d.setIndices(hazeIndices);
    d.setVertexShader(shader(g_VsHeatHaze,true));d.setPixelShader(shader(g_PsHeatHazeOffset));
    d.drawIndexed(4,quads*4,0,quads*2);++hazeDraws;
}

void NativeEffects::endHaze() {
    if(!hazeState) return;
    if(hazeDraws) {
        setup();sampler(0,targets.at("haze-shift").id);sampler(1,targets.at("haze-scene").id,true);
        d.setRenderState(RenderState::ColourWriteEnable,7);
        // The copies use the full target, so split-screen viewports need the
        // corresponding subrectangle rather than the whole image's UVs.
        const auto copy=target("haze-view",hazeTarget.viewport.width,hazeTarget.viewport.height);
        if(d.copyNativeTarget(targets.at("haze-scene").id,0,hazeTarget.viewport,copy)) sampler(1,copy,true);
        const auto shifts=target("haze-view-shift",hazeTarget.viewport.width,hazeTarget.viewport.height);
        if(d.copyNativeTarget(targets.at("haze-shift").id,0,hazeTarget.viewport,shifts)) sampler(0,shifts);
        d.setPixelShaderConstantF(2,hazeParams[2].data(),1);
        quad(hazeTarget.id,hazeTarget.viewport,shader(g_PsHeatHazeComposite));
    }
    hazeState.reset();
}

NativeRenderDevice::NativeRenderDevice():m_nativeEffects(std::make_unique<NativeEffects>(*this)) {}
NativeRenderDevice::~NativeRenderDevice()=default;
void NativeRenderDevice::configureNativeShaders(const DeviceDesc& desc) {
    m_replacements.shaders20=desc.shaders>=2;
    setShaderFeatures((desc.worldLight?1u:0u)|(desc.headlights?2u:0u)|(desc.sunShadows?4u:0u),desc.worldLightStrength);
}
void NativeRenderDevice::setShaderFeatures(unsigned features,float strength) {
    m_replacements.features=features;m_replacements.worldLightStrength=strength;
    auto variants=std::move(m_pixelVariants);m_pixelVariants.clear();
    for(const auto& [key,id]:variants) if(id) destroyShader(id);
}
void NativeRenderDevice::setLightCase(LightCase lightCase) { m_lightCase=lightCase; }
std::vector<uint32_t> NativeRenderDevice::nativeShaderCode(const uint32_t* code,bool vertex) {
    const char* name=nullptr;
    auto replacement=replaceShader(code,vertex,m_replacements,&name,kBaseLightCase);
    if(!replacement.empty()) { ++m_replacedShaders;log::info("native shader replacement: {}",name?name:"unnamed"); }
    return replacement;
}
void NativeRenderDevice::rememberNativeShader(ShaderId id,const uint32_t* code,bool vertex) {
    if(vertex || m_creatingVariant) return;
    // Keep the comment-free original, just as canonicalShader fingerprints it.
    // This also preserves the original across a light-case change.
    std::vector<uint32_t> words{code[0]};
    for(size_t i=1;i<0x10000;) {
        const auto token=code[i++];
        if((token&0xffffu)==0xfffeu) { i+=(token>>16)&0x7fffu;continue; }
        words.push_back(token);
        if(token==0x0000ffffu) { m_originalPixels[id]=std::move(words);return; }
    }
    throw std::runtime_error("Unterminated native shader");
}
ShaderId NativeRenderDevice::resolveNativePixelShader(ShaderId id) {
    if(!id || m_lightCase==kBaseLightCase || !m_replacements.any()) return id;
    const auto original=m_originalPixels.find(id);if(original==m_originalPixels.end()) return id;
    const auto key=std::make_pair(id,m_lightCase);
    if(auto it=m_pixelVariants.find(key);it!=m_pixelVariants.end()) return it->second?it->second:id;
    auto code=replaceShader(original->second.data(),false,m_replacements,nullptr,m_lightCase);
    if(code.empty()) { m_pixelVariants[key]=0;return id; }
    m_creatingVariant=true;
    ShaderId result=0;
    try { result=createPixelShader(code.data()); }
    catch(...) { m_creatingVariant=false;throw; }
    m_creatingVariant=false;
    if(!result) throw std::runtime_error("Native light-case shader unavailable");
    m_pixelVariants[key]=result;return result;
}
void NativeRenderDevice::forgetNativeShader(ShaderId id) {
    m_originalPixels.erase(id);std::vector<ShaderId> released;
    for(auto it=m_pixelVariants.begin();it!=m_pixelVariants.end();) {
        if(it->first.first==id) { if(it->second) released.push_back(it->second);it=m_pixelVariants.erase(it); }
        else ++it;
    }
    for(auto variant:released) destroyShader(variant);
}
void NativeRenderDevice::shutdownNativeEffects() { m_nativeEffects.reset(); }
TextureId NativeRenderDevice::createFloatTarget(uint32_t w,uint32_t h) { return createNativeTarget(w,h,TargetFormat::Scalar); }
bool NativeRenderDevice::ambientOcclusion(TextureId t,const AmbientOcclusionView& v) { return m_nativeEffects->ao(t,v); }
bool NativeRenderDevice::pointLights(TextureId t,const PointLightView& v) { return m_nativeEffects->point(t,v); }
bool NativeRenderDevice::volumetricLight(TextureId t,const VolumetricLightView& v) { return m_nativeEffects->volumetric(t,v); }
bool NativeRenderDevice::lampShadows(TextureId t,const std::array<TextureId,4>& maps,const LampShadowView& v) { return m_nativeEffects->lamps(t,maps,v); }
bool NativeRenderDevice::lightReflections(TextureId t,const LightReflectionView& v) { return m_nativeEffects->reflections(t,v); }
bool NativeRenderDevice::dilateDepth(TextureId t,const Viewport& r,float gap) { return m_nativeEffects->dilate(t,r,gap); }
void NativeRenderDevice::setBrightness(float b) { m_nativeEffects->brightness=std::clamp(b,0.f,1.f); }
void NativeRenderDevice::finishNativeFrame() { m_nativeEffects->brighten(); }
void NativeRenderDevice::setAntialiasing(uint32_t,bool on) { m_nativeEffects->smaa=on; }
void NativeRenderDevice::antialias() { m_nativeEffects->aa(); }
void NativeRenderDevice::setHeatHazeStrength(float strength) { m_nativeEffects->hazeStrength=std::clamp(strength,0.f,3.f); }
bool NativeRenderDevice::cloudMap(TextureId t,const CloudView& v) { return m_nativeEffects->cloudsMap(t,v); }
bool NativeRenderDevice::cloudView(const CloudView& v) { return m_nativeEffects->cloudsView(v); }
bool NativeRenderDevice::cloudSky(uint32_t n,const CloudView& v) { return m_nativeEffects->cloudsSky(n,v); }
uint32_t NativeRenderDevice::beginHeatHaze() { return m_nativeEffects->beginHaze(); }
void NativeRenderDevice::drawHeatHaze(const void* data,uint32_t n,float boost) { m_nativeEffects->drawHaze(data,n,boost); }
void NativeRenderDevice::endHeatHaze() { m_nativeEffects->endHaze(); }
bool NativeRenderDevice::captureAmbientOcclusion(const std::wstring& file) { return m_nativeEffects->capture("ao",file); }
bool NativeRenderDevice::capturePointLights(const std::wstring& file) { return m_nativeEffects->capture("point",file); }
bool NativeRenderDevice::captureVolumetricLight(const std::wstring& file) { return m_nativeEffects->capture("volumetric",file); }
bool NativeRenderDevice::captureLampShadows(const std::wstring& file) { return m_nativeEffects->capture("lamps",file); }
bool NativeRenderDevice::captureLightReflections(const std::wstring& file) { return m_nativeEffects->capture("reflection",file); }
}
