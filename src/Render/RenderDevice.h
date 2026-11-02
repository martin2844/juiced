#pragma once
// The engine's interface to the graphics API. The game code draws through IRenderDevice only; the backend
// (Render/D3D9: Direct3D 9, the stock game's API) implements it.

#include "Render/LightCase.h"
#include "Render/RenderStates.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct HWND__;
using HWND = HWND__*;

namespace oj::render {

enum class Backend { D3D9, Metal, GLES };

const char* backendName(Backend backend);

// The fixed build's anti-aliasing and texture filter the adapter offers (the graphics options' ADVANCED page).
struct QualityCaps {
    std::vector<uint32_t> multisampleCounts;  // MSAA's sample counts (2 .. 16) the frame's colour and depth formats take
    bool smaa = false;                         // SMAA: shaders 3.0
    uint32_t maxAnisotropy = 0;                // the texture filter's most anisotropy (0: trilinear only)
};

// What the stock renderer takes from the settings and the command line (docs/spec/01-startup.md, "Renderer").
struct DeviceDesc {
    HWND window = nullptr;           // the back buffer takes its client size
    // 0x00512B30 ([0x0075F104]): the last adapter whose DeviceIdentifier is the settings' Adapter, else the first;
    // the device and every question of the adapter take it (one past the adapters: the first).
    uint32_t adapter = 0;
    bool windowed = true;
    uint32_t depthBits = 32;         // RezDepth: 16 | 32
    uint32_t antialiasing = 0;       // 0: none, else the multisample level asked for
    bool presentIntervalOne = false;  // -r
    bool pureDevice = true;          // cleared by -nonpured3d
    bool multithreaded = false;      // -mtd3d
    bool softwareVertexProcessing = false;  // -s, -debugfx
    // The fixed build's renderer (Render/FrameOptions; the registry's TextureFilter, LodBias, VSync, MSAA, ShowFps):
    // the texture filter and LOD bias over the game's sampler states, VSync off an immediate present, MSAA its
    // samples (0: the stock's antialiasing), the frame rate counter in the corner.
    uint32_t textureFilter = 0;
    uint32_t lodBias = 0;
    bool vsync = true;
    uint32_t msaa = 0;
    bool showFps = false;
    // The fixed build's screen modes (Game/Platform/Screen): the full screen mode's refresh rate asked for
    // (RefreshRate; the back buffer format's mode of the resolution at it, else at its highest); the picture of the
    // window without a frame (its resolution, 0: the back buffer's), drawn into a surface
    // of its size and put into the back buffer (the monitor's) as the frame ends, its shape kept or stretched.
    uint32_t refreshRate = 0;
    uint32_t pictureWidth = 0;
    uint32_t pictureHeight = 0;
    bool stretch = false;
    // The world light (Render/ShaderReplacements, the registry's WorldLight and WorldLightStrength): the lit
    // materials' shaders made from JuicedConfigExt's; their shaders 1.x only when the game's shader classes have 2.0
    // (the registry's Shaders, `shaders`, and the adapter's versions, as Render/Program's shaderClasses).
    bool worldLight = false;
    float worldLightStrength = 1.0f;  // the sky 0.2 and the side 0.15 of the shaders' literals times this
    // The headlights (Game/Scene/Headlights, the registry's Headlights): the night shaders that read the cars' lights
    // made from JuicedConfigExt's.
    bool headlights = false;
    // WORLD LIGHT ULTRA's sun shadows (Game/Scene/SunShadows): the day shaders that read the sun's cascades (kind S).
    bool sunShadows = false;
    // The heat haze (Game/Scene/HeatHaze, the registry's HeatHaze and HeatHazeStrength); on, the device is not pure
    // (the pass keeps the game's states in a state block).
    bool heatHaze = false;
    float heatHazeStrength = 1.0f;  // 0 .. 3
    // The brightness drawn over the finished frame (setBrightness; JuicedConfigExt's Shared/BrightnessPass) in place of
    // the stock's gamma ramp; on, the device is not pure (the pass keeps the game's states in a state block).
    bool brightness = false;
    // SMAA (the registry's SMAA; JuicedConfigExt's Shared/SmaaPass) over each camera view of the screen at shaders 3.0
    // (antialias); on, the device is not pure.
    bool smaa = false;
    uint32_t shaders = 3;
};

struct Color {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
};

// A vertex of the debug triangles: position in pixels of the back buffer (0,0 top left), colour 0xAARRGGBB.
struct DebugVertex {
    float x = 0.0f, y = 0.0f;
    uint32_t argb = 0xFFFFFFFF;
};

enum class PixelFormat { DXT1, DXT3, DXT5, A8R8G8B8, X8R8G8B8, R5G6B5 };

// Mip levels largest first, as in a DDS file (DXT in 4 x 4 blocks).
struct TextureData {
    PixelFormat format = PixelFormat::A8R8G8B8;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t levels = 1;
    const uint8_t* pixels = nullptr;
    size_t size = 0;
};

using TextureId = uint32_t;  // 0: none

// The sampler states a texture brings (its .ddt header).
struct SamplerParams {
    uint32_t minFilter = 2;
    uint32_t magFilter = 2;
    uint32_t mipFilter = 2;
    uint32_t addressU = 1;
    uint32_t addressV = 1;
    float lodBias = 0.0f;
};

enum class Blend { Alpha, Additive };

// A rectangle of the back buffer in pixels; drawing outside it is cut off.
struct ClipRect {
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
};

// A vertex of a screen quad: position in pixels of the back buffer, texture coordinates, colour 0xAARRGGBB.
struct QuadVertex {
    float x = 0.0f, y = 0.0f;
    float u = 0.0f, v = 0.0f;
    uint32_t argb = 0xFFFFFFFF;
};

// The 3D pipeline as the stock engine drives it: Direct3D 9's states, shaders from byte code and constants; the
// state, type and enum values are Direct3D 9's.
using ShaderId = uint32_t;       // 0: none (the fixed-function stage)
using DeclarationId = uint32_t;  // 0: none
using QueryId = uint32_t;        // 0: none
using BufferId = uint32_t;       // 0: none

// A D3DVERTEXELEMENT9; a declaration ends with the record {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0}.
struct VertexElement {
    uint16_t stream = 0;
    uint16_t offset = 0;
    uint8_t type = 0;
    uint8_t method = 0;
    uint8_t usage = 0;
    uint8_t usageIndex = 0;
};

// D3DMATERIAL9: diffuse, ambient, specular, emissive (r, g, b, a each), power.
struct FixedMaterial {
    float values[17] = {};
};

// D3DLIGHT9.
struct FixedLight {
    uint32_t type = 3;  // D3DLIGHT_DIRECTIONAL
    float diffuse[4] = {};
    float specular[4] = {};
    float ambient[4] = {};
    float position[3] = {};
    float direction[3] = {};
    float range = 0.0f;
    float falloff = 0.0f;
    float attenuation0 = 0.0f;
    float attenuation1 = 0.0f;
    float attenuation2 = 0.0f;
    float theta = 0.0f;
    float phi = 0.0f;
};

struct Viewport {
    uint32_t x = 0, y = 0, width = 0, height = 0;
    float minZ = 0.0f, maxZ = 1.0f;
};

// D3DADAPTER_IDENTIFIER9's numbers (the CONDITION annotations of effects test them).
struct AdapterInfo {
    uint16_t driverProduct = 0, driverVersion = 0, driverSubVersion = 0, driverBuild = 0;
    uint32_t vendorId = 0, deviceId = 0, subSysId = 0, revision = 0;
};

// The view of an ambient occlusion pass (IRenderDevice::ambientOcclusion; JuicedConfigExt's
// AmbientOcclusionPass::View).
struct AmbientOcclusionView {
    float tanX = 1.0f;          // the tangents of the view's half angles across and up
    float tanY = 1.0f;
    float range = 1.0f;         // the depth's 1
    float radius = 60.0f;       // the half sphere tested over each point
    float strength = 1.0f;      // the occlusion of a point hidden every way
    float bias = 2.0f;          // a nearer depth by less than this hides nothing
    float fadeFrom = 6000.0f;   // the occlusion fades out from this depth
    float fadeLength = 4000.0f;
    // A tree's leaves (Renderer3d::kFoliageMark): the sphere tested about each point, the strength, the distance their
    // varying part fades from and over.
    float foliageRadius = 200.0f;
    float foliageStrength = 0.55f;
    float foliageFadeFrom = 6000.0f;
    float foliageFadeLength = 6000.0f;
};

// The view of a pass of the lights reflected in the road (IRenderDevice::lightReflections; JuicedConfigExt's
// LightReflectionPass::View).
struct LightReflectionView {
    float tanX = 1.0f;           // the tangents of the view's half angles across and up
    float tanY = 1.0f;
    float range = 1.0f;          // the depth's 1
    float reach = 20000.0f;      // the rays' length
    std::array<float, 3> up{0.0f, 1.0f, 0.0f};  // the world's up in the view (x right, y up, z ahead)
    float wetness = 0.0f;        // the road's, 0 .. 1
    float fromDry = 0.55f;       // the brightness kept from: the lights alone on a dry road
    float fromWet = 0.25f;       // more of the world in the wet
    float strengthDry = 2.0f;
    float strengthWet = 3.0f;
    float streak = 1.5f;         // the long blur's spread, in texels of the depth
    float roughness = 0.25f;     // the road's normal turned by the noise up to this
    float asphalt = 0.3f;        // the reflections' share on the asphalt (Renderer3d::kGroundMark), 0 .. 4
    float water = 1.9f;          // and on the water (kWaterMark: the puddles, the sea)
    // The street lamps' streaks (16 at most): each one's mirror point's place on the view (0 .. 1 across and down), its
    // half width and its length toward the view past the point (of the view), its colour by its strength and the
    // lamp's foot's place down the view (Game/Scene/AmbientOcclusion's lampStreak).
    int32_t streakCount = 0;
    std::array<std::array<float, 8>, 16> streaks{};
};

// A street lamp of a pass of the cars' shadows from the lamps (IRenderDevice::lampShadows; JuicedConfigExt's
// LampShadowPass::Lamp): its map's world to clip rows x, y and w, its place, its look, its map's range and its shadow's
// share (0 .. 1: a lamp fading in or out).
struct LampShadowLamp {
    std::array<std::array<float, 4>, 3> clip{};
    std::array<float, 3> place{};
    std::array<float, 3> look{0.0f, -1.0f, 0.0f};
    float range = 2000.0f;
    float strength = 1.0f;
};

// The view of that pass: the tangents of the view's half angles, the depth's range, the view to the world (rows), a
// lamp's share of the light at its foot, the bias (of a map's range), a map's texel, the lamps.
struct LampShadowView {
    float tanX = 1.0f;
    float tanY = 1.0f;
    float range = 1.0f;
    std::array<std::array<float, 4>, 3> camera{{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f},
                                                {0.0f, 0.0f, 1.0f, 0.0f}}};
    float strength = 0.75f;
    float bias = 0.01f;
    float texel = 1.0f / 512.0f;
    int32_t count = 0;
    std::array<LampShadowLamp, 4> lamps{};
};

// A light of a pass of the light in the night's haze (IRenderDevice::volumetricLight; JuicedConfigExt's
// VolumetricLightPass::Light): its place and reach, its cone's axis, side and up over the tangents of its half angles,
// its colour by its strength, the distance its light falls off over, its steps along a ray within its reach (1 .. 8),
// its scattering's g (0 even: a lamp; a headlight's beam forward, as rain's drops scatter it).
struct VolumetricLight {
    std::array<float, 3> place{};
    float reach = 1000.0f;
    std::array<float, 3> axis{0.0f, -1.0f, 0.0f};
    int32_t steps = 8;
    std::array<float, 3> side{1.0f, 0.0f, 0.0f};
    std::array<float, 3> up{0.0f, 0.0f, 1.0f};
    std::array<float, 3> colour{1.0f, 1.0f, 1.0f};
    float falloff = 500.0f;
    float forward = 0.0f;
};

// The view of that pass: the tangents of the view's half angles, the depth's range, the view to the world (rows), the
// haze's density over a metre, a metre in the world's units, the lights (24 at most).
struct VolumetricLightView {
    float tanX = 1.0f;
    float tanY = 1.0f;
    float range = 1.0f;
    std::array<std::array<float, 4>, 3> camera{{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f},
                                                {0.0f, 0.0f, 1.0f, 0.0f}}};
    float density = 0.02f;
    float metre = 100.0f;
    int32_t count = 0;
    std::array<VolumetricLight, 24> lights{};
};

// A point light on the view's surfaces (IRenderDevice::pointLights; JuicedConfigExt's PointLightPass::Light): its
// place and reach, its colour by its strength, the distance its light falls off over.
struct PointLight {
    std::array<float, 3> place{};
    float reach = 600.0f;
    std::array<float, 3> colour{1.0f, 1.0f, 1.0f};
    float falloff = 150.0f;
};

// The view of that pass: the tangents of the view's half angles, the depth's range, the view to the world (rows), the
// lights (eight at most).
struct PointLightView {
    float tanX = 1.0f;
    float tanY = 1.0f;
    float range = 1.0f;
    std::array<std::array<float, 4>, 3> camera{{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f},
                                                {0.0f, 0.0f, 1.0f, 0.0f}}};
    int32_t count = 0;
    std::array<PointLight, 8> lights{};
};

// The view of the fixed build's procedural clouds (IRenderDevice::cloudMap, cloudSky; JuicedConfigExt's
// CloudPass::View, Game/Scene/Clouds): the layer's height above the eye, 1 / its fading distance, the share of the sky
// it covers and the sharpness of its edges, the world to the map's coordinates (the effects' cCloudProject rows), the
// eye, the details' coordinates over the shapes', the wind's offsets, the direction toward the sun and its forward
// scattering, a step toward the sun in the map's coordinates, the sun's light and its absorption, the clouds' shade
// and opacity, the map's light under a clear sky and under a whole cloud; with `volume` (SKY ULTRA) the clouds a
// volume from `height` to `top` above the eye: its extinction, the view's steps through it and each step's toward the
// sun (0 none), the farthest distance along the view, the world to the weather's, the shapes' and the details'
// coordinates and the two's offsets (the wind), the overcast (the weather's least: a deck under a covered sky); the
// main view's (`screen`: IRenderDevice::cloudView made its clouds) world-to-clip rows x, y and w (their x, y, z) and
// the world's time.
struct CloudView {
    float height = 150000.0f;
    float fade = 1.0f / 2000000.0f;
    float cover = 0.4f;
    float sharpness = 4.0f;
    std::array<std::array<float, 4>, 2> project{{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}}};
    std::array<float, 3> eye{};
    float detailScale = 8.0f;
    std::array<float, 4> wind{};
    std::array<float, 3> toSun{0.0f, 1.0f, 0.0f};
    float scatter = 0.6f;
    std::array<float, 2> sunStep{};
    std::array<float, 3> sun{1.0f, 1.0f, 1.0f};
    float absorption = 1.5f;
    std::array<float, 3> ambient{0.6f, 0.65f, 0.7f};
    float opacity = 1.0f;
    float clearLight = 0.62f;
    float cloudLight = 0.08f;
    bool volume = false;
    float top = 400000.0f;
    float extinction = 0.0004f;
    float steps = 40.0f;
    float lightSteps = 5.0f;
    float farthest = 4000000.0f;
    float weatherScale = 1.0f / 1000000.0f;
    float shapeScale = 1.0f / 300000.0f;
    float erosionScale = 1.0f / 40000.0f;
    std::array<float, 3> shapeDrift{};
    std::array<float, 3> erosionDrift{};
    float overcast = 0.0f;
    bool screen = false;
    std::array<std::array<float, 3>, 3> clip{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    float seconds = 0.0f;
};

class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    virtual Backend backend() const = 0;
    virtual std::string adapterName() const = 0;
    // A line on the chosen formats and flags, for the log.
    virtual std::string description() const = 0;

    // The frame begun as the window manager's view begins it (0x00516880 with the view's flags 5, 0x005A6B37): the
    // depth and the stencil cleared, the colour kept from the frames before (the interface draws over its last frame:
    // a window's black fading in shows the window before it under it); a view that clears its colour (the race's)
    // does it itself. The colour is cleared only as the device is made or reset (0x005145F0: cleared, presented,
    // cleared again). false: the device is lost or not restored yet; the frame is skipped (no endFrame).
    virtual bool beginFrame() = 0;
    // Optional backend material calibration; UINT32_MAX marks non-material/UI draws.
    virtual void setMaterialClass(uint32_t) {}
    virtual void endFrame() = 0;

    // New size of the window's client area; the back buffer follows at the next beginFrame.
    virtual void resize(uint32_t width, uint32_t height) = 0;

    virtual void drawDebugTriangles(const DebugVertex* vertices, uint32_t count) = 0;

    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;
    // D3DCAPS9 VertexShaderVersion / PixelShaderVersion (0xFFFE0300 / 0xFFFF0300 for 3.0).
    virtual uint32_t vertexShaderVersion() const = 0;
    virtual uint32_t pixelShaderVersion() const = 0;
    virtual AdapterInfo adapterInfo() const = 0;
    // The back buffer's D3DFORMAT.
    virtual uint32_t backBufferFormat() const = 0;
    // D3DCAPS9's SrcBlendCaps.
    virtual uint32_t sourceBlendCaps() const = 0;
    // D3DCAPS9's Caps2 has D3DCAPS2_FULLSCREENGAMMA.
    virtual bool fullScreenGamma() const = 0;
    // The display mode's refresh rate in Hz (the desktop's in a window), 0 when it cannot be read.
    virtual uint32_t refreshRate() const { return 0; }
    // The depth buffers have a stencil (D24S8 and the like).
    virtual bool hasStencilBuffer() const { return false; }
    // The fixed build's anti-aliasing and texture filter changed while the game runs (the graphics options' ADVANCED
    // page): MSAA's samples as DeviceDesc::msaa (a change of the frame's samples resets the device as the next frame
    // begins), SMAA on or off, the texture filter as DeviceDesc::textureFilter (put on the samplers' states the game set
    // last at once); what the adapter offers of them.
    virtual void setAntialiasing(uint32_t msaa, bool smaa) {
        (void)msaa;
        (void)smaa;
    }
    virtual void setTextureFilter(uint32_t filter) { (void)filter; }
    virtual QualityCaps qualityCaps() const { return {}; }
    // The fixed build's SMAA (DeviceDesc::smaa) over the viewport of target 0 at the end of a camera view's scene;
    // nothing when it is off, another target is drawn into or the adapter has no shaders 3.0.
    virtual void antialias() {}
    // The fixed build's heat haze (DeviceDesc::heatHaze, Game/Scene/HeatHaze) over the viewport of target 0 (or of a
    // scene target, multisampleTarget's, where the stock's effects draw the scene) after a camera view's particles:
    // begun (the frame copied, the particles' target bound; the view's width in pixels, 0 when it is off or cannot
    // be), the particles' quads drawn into it (24 bytes a vertex: place, colour, u v; the
    // declaration and the distortion map on stage 0 as the particles' material set them; their shift x `boost`),
    // ended (the copy drawn over the view moved by the shifts, the device's states as before the pass).
    virtual uint32_t beginHeatHaze() { return 0; }
    virtual void drawHeatHaze(const void* vertices, uint32_t quads, float boost) {
        (void)vertices;
        (void)quads;
        (void)boost;
    }
    virtual void endHeatHaze() {}
    // The fixed build's brightness (DeviceDesc::brightness): the settings' brightness (+0x60, 0 .. 1) through the stock
    // gamma ramp's curve (JuicedConfigExt's Shared/BrightnessCurve) over each frame as it ends, the middle drawing
    // nothing.
    virtual void setBrightness(float brightness) { (void)brightness; }
    // SetGammaRamp on the swap chain 0: the flags and the red, green and blue ramps of 256 values each.
    virtual void setGammaRamp(uint32_t flags, const uint16_t* red, const uint16_t* green, const uint16_t* blue) = 0;

    // The loading thread's work on the device (the stock renderer's queue [renderer+0x204], run by the main thread at
    // its frame, 0x00514C96, and while it waits for a job, 0x0045A35B): a frame holds the device from beginFrame to
    // endFrame, and the loading thread's calls making, freeing, locking or writing resources wait for the device
    // between frames. The device's thread waiting for that work (a job, the loads' lock) lets the device go for a
    // moment here, and the turn to a job that takes turns with the frames (Core/Base/LoadingThread.h); elsewhere it
    // does nothing.
    virtual void letLoadingRun();
    // 0x0051D270 at a job's end on the loading thread: until the device's thread has run the renderer's queue once
    // more (a frame begun, or letLoadingRun while it waits), 10 ms sleeps; on the device's thread nothing.
    virtual void waitForService() {}

    // 0 when the data is not usable (the levels that fit are taken).
    virtual TextureId createTexture(const TextureData& data) = 0;
    // A cube: width x width faces +X, -X, +Y, -Y, +Z, -Z one after another, each with all its levels; 0 when the data
    // is not usable.
    virtual TextureId createCubeTexture(const TextureData& data) = 0;
    virtual void destroyTexture(TextureId texture) = 0;
    // A texture drawn into (the stock code's 0x0051F2A0 for a flat one, 0x005C8F00 for a cube): 32-bit colour, one
    // level, a depth buffer of its size kept by the device; its contents are lost with the device. 0 when it cannot
    // be made.
    virtual TextureId createRenderTarget(uint32_t width, uint32_t height, bool cube) = 0;
    // The fixed build's MSAA with the stock's effects: a flat target of the scene drawn with the frame's samples (a
    // multisampled colour and depth of its size), resolved into its texture when another target is taken, it is read
    // or the frame ends; as any target without MSAA.
    virtual void multisampleTarget(TextureId id) { (void)id; }
    // The fixed build's headlights' shadow maps (Game/Scene/HeadlightShadows): a flat target of one 32-bit float
    // channel, its depth buffer kept by the device as any target's; 0 when the adapter takes no such target.
    virtual TextureId createFloatTarget(uint32_t width, uint32_t height) {
        (void)width;
        (void)height;
        return 0;
    }
    // The shadow maps on the pixel samplers 8 .. 8 + count - 1 (the night shaders' headlights read them), point
    // sampled and clamped whatever the texture filter; 0 for none.
    virtual void setShadowMaps(const TextureId* maps, uint32_t count) {
        (void)maps;
        (void)count;
    }
    // A float target's values 0 .. 1 as grey in a 24-bit BMP (the debug builds' --shadow-map); false when it cannot.
    virtual bool captureFloatTarget(TextureId target, const std::wstring& file) {
        (void)target;
        (void)file;
        return false;
    }
    // A face of a cube target copied into the same face of another cube target (the fixed build's REFLECTIONS ULTRA:
    // the stock's cube from the larger one), scaled with a linear filter where the adapter has one; false when it
    // cannot.
    virtual bool copyCubeFace(TextureId from, TextureId to, uint32_t face) {
        (void)from;
        (void)to;
        (void)face;
        return false;
    }
    // The fixed build's ambient occlusion (WORLD LIGHT HI and ULTRA; JuicedConfigExt's Shared/AmbientOcclusionPass at
    // shaders 3.0): the current target's viewport multiplied by the occlusion worked out from `depth` (a float target
    // of the depth along the view over the range, Game/Scene/AmbientOcclusion); false when it cannot.
    virtual bool ambientOcclusion(TextureId depth, const AmbientOcclusionView& view) {
        (void)depth;
        (void)view;
        return false;
    }
    // The fixed build's lights reflected in the road (WORLD LIGHT ULTRA by night; JuicedConfigExt's
    // Shared/LightReflectionPass at shaders 3.0): the bright part of what the road's mirrored rays meet in the current
    // target's viewport, streaked and added over its road with the street lamps' streaks of `view`, from `depth` (the
    // ambient occlusion's view depth, the road marked); false when it cannot.
    virtual bool lightReflections(TextureId depth, const LightReflectionView& view) {
        (void)depth;
        (void)view;
        return false;
    }
    // The fixed build's cars' shadows from the street lamps (WORLD LIGHT ULTRA by night; JuicedConfigExt's
    // Shared/LampShadowPass at shaders 3.0): the current target's viewport multiplied where a car hides a lamp, from
    // `depth` (the ambient occlusion's view depth) and the lamps' maps (float targets, `view.count` of them); false
    // when it cannot.
    virtual bool lampShadows(TextureId depth, const std::array<TextureId, 4>& maps, const LampShadowView& view) {
        (void)depth;
        (void)maps;
        (void)view;
        return false;
    }
    // The sun's cascades' atlas on the pixel sampler 14 (WORLD LIGHT ULTRA's day shaders read it), point filtered and
    // clamped; 0 takes it off.
    virtual void setSunShadowMap(TextureId map) { (void)map; }
    // The last lights' reflection in the road as a 24-bit BMP (the debug builds' --ao-map, beside the occlusion);
    // false when it cannot.
    virtual bool captureLightReflections(const std::wstring& file) {
        (void)file;
        return false;
    }
    // The fixed build's light in the night's haze (WORLD LIGHT ULTRA by night; JuicedConfigExt's
    // Shared/VolumetricLightPass at shaders 3.0): the lights' scattering along the view's rays to their surfaces (from
    // `depth`, the ambient occlusion's view depth) added over the current target's viewport; false when it cannot.
    virtual bool volumetricLight(TextureId depth, const VolumetricLightView& view) {
        (void)depth;
        (void)view;
        return false;
    }
    // The fixed build's point lights on the view's surfaces (the cars' flames, WORLD LIGHT HI and ULTRA;
    // JuicedConfigExt's Shared/PointLightPass at shaders 3.0): each light's light by the distance and the surface's
    // facing (from `depth`, the ambient occlusion's view depth) added over the current target's viewport; false when it
    // cannot.
    virtual bool pointLights(TextureId depth, const PointLightView& view) {
        (void)depth;
        (void)view;
        return false;
    }
    // The fixed build's procedural clouds (WORLD LIGHT ULTRA; JuicedConfigExt's Shared/CloudPass at shaders 3.0): the
    // map of the clouds' light drawn into `map` (a render target, the effects' cloudcast texture in its place); false
    // when it cannot.
    virtual bool cloudMap(TextureId map, const CloudView& view) {
        (void)map;
        (void)view;
        return false;
    }
    // SKY ULTRA: the volume's clouds of the main view in the current viewport (JuicedConfigExt's CloudPass::DrawView:
    // half its size, blended with the last frame's), for the dome's draw in the same viewport (`view.screen`); false
    // when they cannot be (the dome then marches them itself).
    virtual bool cloudView(const CloudView& view) {
        (void)view;
        return false;
    }
    // In the sky dome's draw in place of its triangle strip of `primitives` (the dome's stream, declaration, states and
    // vertex constants bound, the Sky material's): the dome drawn with the clouds over its colours; false when it
    // cannot (the dome then drawn as it is).
    virtual bool cloudSky(uint32_t primitives, const CloudView& view) {
        (void)primitives;
        (void)view;
        return false;
    }
    // A float target's `region` widened about its casters (JuicedConfigExt's Shared/DepthDilatePass: each texel the
    // least of itself and of its eight neighbours nearer than it by more than `gap`; the sun's near cascade, a wire a
    // texel wide no longer stepped over by its soft edge's tests); false when it cannot.
    virtual bool dilateDepth(TextureId target, const Viewport& region, float gap) {
        (void)target;
        (void)region;
        (void)gap;
        return false;
    }
    // A colour target as a 24-bit BMP (the debug builds' --ao-map: the clouds' map); false when it cannot.
    virtual bool captureTarget(TextureId target, const std::wstring& file) {
        (void)target;
        (void)file;
        return false;
    }
    // The last point lights' light as a 24-bit BMP (the debug builds' --ao-map, beside the occlusion); false when it
    // cannot.
    virtual bool capturePointLights(const std::wstring& file) {
        (void)file;
        return false;
    }
    // The last light in the haze as a 24-bit BMP (the debug builds' --ao-map, beside the occlusion); false when it
    // cannot.
    virtual bool captureVolumetricLight(const std::wstring& file) {
        (void)file;
        return false;
    }
    // The last street lamps' shadows' shade as a 24-bit BMP (the debug builds' --ao-map, beside the occlusion);
    // false when it cannot.
    virtual bool captureLampShadows(const std::wstring& file) {
        (void)file;
        return false;
    }
    // The last ambient occlusion as a 24-bit BMP (the debug builds' --ao-map); false when it cannot.
    virtual bool captureAmbientOcclusion(const std::wstring& file) {
        (void)file;
        return false;
    }
    // A face of a cube target as a 24-bit BMP (the debug builds' --cube-map); false when it cannot.
    virtual bool captureCubeFace(TextureId target, uint32_t face, const std::wstring& file) {
        (void)target;
        (void)face;
        (void)file;
        return false;
    }
    // The drawing goes into a render target's colour (a cube's face 0 .. 5: +X, -X, +Y, -Y, +Z, -Z) and its depth
    // buffer, or with 0 into the back buffer again; the viewport becomes the whole target.
    virtual void setRenderTarget(TextureId target, uint32_t face) = 0;
    // A render target's or the back buffer's (0) size.
    virtual uint32_t targetWidth(TextureId target) const = 0;
    virtual uint32_t targetHeight(TextureId target) const = 0;
    // The resets of the device so far: each loses what the render targets held.
    virtual uint32_t resets() const = 0;
    // A flat render target's pixels read back (32-bit, rows of `pitch` bytes); false when they cannot be read.
    virtual bool readTarget(TextureId target, std::vector<uint8_t>& pixels, uint32_t* pitch) = 0;
    // Exposure tolerates older pixels. Deferred backends may return false while a read is pending.
    virtual bool readTargetDeferred(TextureId target, std::vector<uint8_t>& pixels, uint32_t* pitch) {
        return readTarget(target, pixels, pitch);
    }

    // The display mode's D3DFORMAT (IDirect3D9::GetAdapterDisplayMode); 0 when it cannot be read.
    virtual uint32_t displayFormat() const = 0;
    // D3DCAPS9's TextureCaps.
    virtual uint32_t textureCaps() const = 0;
    // IDirect3D9::CheckDeviceFormat of a texture of no usage in `format` (a D3DFORMAT) on a display in `display`.
    virtual bool textureFormatOk(uint32_t display, uint32_t format) const = 0;
    // A texture of one level whose pixels the code writes, in a D3DFORMAT: dynamic in the default pool, else in the
    // managed one; 0 when neither can be made. The default pool's comes back after a reset with its pixels undefined.
    virtual TextureId createDynamicTexture(uint32_t width, uint32_t height, uint32_t format) = 0;
    // Its whole level locked (no flags): the first byte and the bytes from a row to the next; false when it cannot be.
    virtual bool lockTexture(TextureId texture, void** bits, int32_t* pitch) = 0;
    virtual void unlockTexture(TextureId texture) = 0;
    // DrawPrimitiveUP: primitives of a D3DPRIMITIVETYPE from vertices in memory, in the FVF set last.
    virtual void drawUp(uint32_t primitive, uint32_t primitiveCount, const void* vertices, uint32_t stride) = 0;

    // Corners top left, top right, bottom left, bottom right, in pixels of the back buffer as Direct3D 9 takes
    // transformed vertices (pixel centres at whole numbers); texture 0 draws the colours only. A texture is sampled
    // with linear filters and wrapping, or with the sampler states its caller set on stage 0 (samplerSet).
    virtual void drawQuad(const QuadVertex (&corners)[4], TextureId texture, Blend blend, bool samplerSet = false) = 0;
    // A line of one colour between two points in pixels of the back buffer (a line list of transformed vertices).
    virtual void drawLine(float x0, float y0, float x1, float y1, uint32_t argb) = 0;
    // nullptr: no clipping.
    virtual void setClip(const ClipRect* rect) = 0;

    // The back buffer (of the frame being drawn before endFrame; after it, what the swap left) written as a 24-bit
    // BMP, or a JPEG for a name ending in .jpg.
    virtual bool capture(const std::wstring& file) = 0;

    // 3D. Byte code ends with its end word 0x0000FFFF; 0 when the device refuses it.
    virtual ShaderId createVertexShader(const uint32_t* code) = 0;
    virtual ShaderId createPixelShader(const uint32_t* code) = 0;
    virtual DeclarationId createDeclaration(const VertexElement* elements) = 0;
    virtual BufferId createVertexBuffer(const void* data, uint32_t bytes) = 0;
    virtual BufferId createIndexBuffer(const uint16_t* indices, uint32_t count) = 0;
    virtual void destroyBuffer(BufferId buffer) = 0;
    // A vertex buffer's bytes from its start written anew.
    virtual void updateVertexBuffer(BufferId buffer, const void* data, uint32_t bytes) = 0;

    // Occlusion queries (D3DQUERYTYPE_OCCLUSION): whether the device has them, one made, its begin or end issued, and
    // its result: 0 with the pixels counted, 1 while not ready, -1 on an error. flush: D3DGETDATA_FLUSH.
    virtual bool occlusionQueries() const = 0;
    virtual QueryId createOcclusionQuery() = 0;
    virtual void issueQuery(QueryId query, bool begin) = 0;
    virtual int queryResult(QueryId query, bool flush, uint32_t* pixels) = 0;

    // The values: Render/RenderStates.h's (Cull, BlendFactor, ...) or the numbers the effects' files give.
    virtual void setRenderState(RenderState state, uint32_t value) = 0;
    virtual void setTextureStageState(uint32_t stage, TextureStageState type, uint32_t value) = 0;
    virtual void setSamplerState(uint32_t sampler, SamplerState type, uint32_t value) = 0;
    virtual void setTexture(uint32_t sampler, TextureId texture) = 0;
    virtual void setTransform(TransformState state, const float* matrix) = 0;
    virtual void setMaterial(const FixedMaterial& material) = 0;
    virtual void setLight(uint32_t index, const FixedLight& light) = 0;
    virtual void lightEnable(uint32_t index, bool enable) = 0;
    virtual void setFvf(uint32_t fvf) = 0;
    virtual void setNPatchMode(float segments) = 0;
    virtual void setVertexShader(ShaderId shader) = 0;
    virtual void setPixelShader(ShaderId shader) = 0;
    // A shader freed (its id then binds none); the fixed build's programs made again (Program::rebuild).
    virtual void destroyShader(ShaderId shader) { (void)shader; }
    // The fixed build's shader replacements changed in the game (ShaderFeature bits; the world light's strength
    // 0 .. 2): the shaders made from now on.
    // The race's light case (Render/LightCase): a pixel shader with a variant for it binds that variant in its place
    // from now on (kBaseLightCase's the shader itself).
    virtual void setLightCase(LightCase lightCase) { (void)lightCase; }
    // The frame's layers timed (the debug builds' --profile, Render/D3D9/D3D9Profiler): on or off from the next frame,
    // false without the device's timestamps; a layer's start, named by a literal (kept as given), ending at the next
    // mark or the frame's end; the layers' times per frame on the GPU and the CPU, the longest on the GPU first.
    virtual bool setProfiling(bool on) {
        (void)on;
        return false;
    }
    virtual void profileMark(const char* layer) { (void)layer; }
    virtual std::string profileReport() { return {}; }
    virtual void setShaderFeatures(unsigned features, float worldLightStrength) {
        (void)features;
        (void)worldLightStrength;
    }
    // The fixed build's heat haze's strength (0 .. 3) changed in the game.
    virtual void setHeatHazeStrength(float strength) { (void)strength; }
    virtual void setVertexShaderConstantF(uint32_t start, const float* data, uint32_t count) = 0;
    virtual void setVertexShaderConstantI(uint32_t start, const int32_t* data, uint32_t count) = 0;
    virtual void setVertexShaderConstantB(uint32_t start, const int32_t* data, uint32_t count) = 0;
    virtual void setPixelShaderConstantF(uint32_t start, const float* data, uint32_t count) = 0;
    virtual void setPixelShaderConstantI(uint32_t start, const int32_t* data, uint32_t count) = 0;
    virtual void setPixelShaderConstantB(uint32_t start, const int32_t* data, uint32_t count) = 0;

    virtual void setDeclaration(DeclarationId declaration) = 0;
    virtual void setStream(BufferId buffer, uint32_t stride) = 0;
    virtual void setIndices(BufferId buffer) = 0;
    // Primitive; the vertices of the stream from 0, the indices from startIndex.
    virtual void drawIndexed(uint32_t primitive, uint32_t vertexCount, uint32_t startIndex,
                             uint32_t primitiveCount) = 0;
    // Primitive; the vertices of the stream from startVertex.
    virtual void draw(uint32_t primitive, uint32_t startVertex, uint32_t primitiveCount) = 0;
    virtual void setViewport(const Viewport& viewport) = 0;
    // ClearFlags.
    virtual void clear(uint32_t flags, uint32_t argb, float z, uint32_t stencil) = 0;
};

// nullptr with *error set when the backend cannot start.
std::unique_ptr<IRenderDevice> createDevice(Backend backend, const DeviceDesc& desc, std::string* error);

}  // namespace oj::render
