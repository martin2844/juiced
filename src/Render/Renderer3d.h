#pragma once
// The stock renderer's 3D state (docs/spec/12-rendering.md): the lights, the fog and the time the shaders get as
// vertex constants, the material drawn last, and the texture sampler states.

#include "Core/Base/Random.h"
#include "Core/Base/Matrix.h"
#include "Render/Program.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstdint>

namespace oj::render {

class Material;

// The detail factor at the renderer's start (0x005A25F0): the detail 60 (%) as 60 x 0.01f, then x 1.5f + 0.66f,
// each step at the x87's 53 bits, stored as a float.
inline float startDetailFactor() {
    const double detail = 60.0 * static_cast<double>(0.01f);
    return static_cast<float>(detail * static_cast<double>(1.5f) + static_cast<double>(0.66f));
}

// [0x0075B298]: the set of sampler states last set on each of stages 0 .. 7 (internSampler's); null when unknown.
// One for the game as the stock's: the 3D renderer's textures and the interface's pictures share it.
std::array<const SamplerParams*, 8>& samplerCache();
// 0x0051EFA0: each set of sampler states kept once (the textures', the targets', the effects' all made through it), so
// that equal sets are one and the stages' cache knows a set, not a texture. The stock's table holds 64 (the 65th
// would be written over its count); the game makes far fewer.
const SamplerParams& internSampler(const SamplerParams& params);
// 0x0051EF30: a stage's sampler states (the filters, the addressing, the LOD bias) from a set of them, skipped when
// the stage has that set already.
void applySampler(IRenderDevice& device, const SamplerParams& params, uint32_t stage);

// The cubes the renderer makes at its start (0x00513807, 0x0051383B: 0x005C9270 with a texel function): per texel of
// a face the direction through it, normalised; the half-vector cube (+0x130, 0x005126F0) takes that minus (0, 0, 1),
// normalised, the normal cube (+0x134, 0x005127C0) that normalised again; the colour is (x, y, z) x 127 + 127
// as bytes (rounded for the half-vector cube, cut toward zero for the normal cube), alpha 1. R5G6B5 unless the
// display is A8R8G8B8; one level; linear filters, clamped addresses.
struct GameTexture;
GameTexture makeRendererCube(class IRenderDevice& device, uint32_t edge, bool halfVector);

// A texture of textures\: the device texture and its sampler states.
struct GameTexture {
    TextureId id = 0;
    SamplerParams sampler;
};

class Renderer3d {
public:
    Renderer3d(IRenderDevice& device, ProgramContext& programs);

    IRenderDevice& device() { return m_device; }
    ProgramContext& programs() { return m_programs; }
    uint32_t classes() const { return m_programs.classes(); }

    // The engine's vertex constant write (0x00504C60).
    void setVertexConstants(uint32_t start, const float* data, uint32_t count) {
        m_programs.setVertexConstants(start, data, count);
    }

    // 0x00514330: the constants and render states the device starts from, made (0x005145F0) or reset (0x00514090).
    void reset();

    void setAmbient(const Vec4& colour);           // 0x00514E10: c25, then fixedFunctionLights
    // 0x005463C0 (from setAmbient and the world's region draw 0x005469A0), only with the shader classes 0x10000 (no
    // shaders, or the Shaders setting 0): the material (0x00546320: diffuse and ambient white, specular white with
    // alpha 0, power 17), three directional lights from the light colours / 256 and directions (+0x40 ..), specular
    // 0.6, ambient the renderer's ambient x 0.01; LIGHTING on, AMBIENT the ambient as a colour,
    // AMBIENTMATERIALSOURCE material, LOCALVIEWER off.
    void fixedFunctionLights();
    void fixedFunctionMaterial();                  // 0x00546320
    void setLight1Colour(const Vec4& colour);      // 0x005150C0: c26
    void setLight2Colour(const Vec4& colour);      // 0x00515140: c28
    void writeLightColours();                      // 0x00514E80: c26 .. c28 at once (the fixed build: c26, c28, c30)
    void setLightDirection(const Vec4& direction); // 0x005151D0: c27, c43, c44, c45 .. c49
    void setLight2Direction(const Vec4& direction);  // c29
    void setLight3Direction(const Vec4& direction);  // c31
    void setSpecular(uint32_t argb, float highlight);  // 0x00515500: c34
    void setLightScale(float scale);               // 0x00515AF0
    void setRandom(const Vec4& value);             // 0x00515BB0: c40
    void setFogColour(uint32_t argb);              // 0x00514D60: FOGCOLOR, c50
    void applyFogDensity();                        // 0x00514CF0: FOGDENSITY, c51

    // Binds a texture on a stage with its sampler states (0x00510E90; the states are skipped when the stage still
    // has this texture's states, 0x0051EF30).
    void bindTexture(const GameTexture* texture, uint32_t stage);
    // A stage's sampler states from a set of them (0x0051EF30), skipped when the stage has them already.
    void applySampler(const SamplerParams& params, uint32_t stage);
    // The state manager marks the cache unknown when an effect sets a sampler state.
    void forgetSamplers() { samplerCache().fill(nullptr); }

    // The material drawn last and the pass (0x005240E0, 0x00524170).
    void beginMaterialPass(Material& material, uint32_t pass);
    void endMaterial();
    // A material whose parameters change is no longer the one drawn last.
    void forgetMaterial(const Material* material) {
        if (m_current == material) m_current = nullptr;
    }
    const Material* currentMaterial() const { return m_current; }

    // The renderer's fields.
    Vec4 ambient{};           // +0x28
    Vec4 lightDirection{};    // +0x40
    Vec4 light2Direction{};   // +0x50
    Vec4 light3Direction{};   // +0x60
    Vec4 light1Colour{};      // +0x70
    Vec4 light2Colour{};      // +0x80
    Vec4 light3Colour{};      // +0x90
    float fogMin = 0.0f;      // +0x100
    float fogMax = 0.0f;      // +0x104
    float fogNear = 0.0f;     // +0x108
    float fogFar = 0.0f;      // +0x10C
    float time = 0.0f;        // +0x110
    float timeStep = 0.0f;    // +0x114
    const GameTexture* lightTile = nullptr;   // +0x120
    // +0x118: the environment the cars (and Tarmac, Chrome) reflect: the car's cube drawn each frame with the
    // reflections on, else the world's environment texture.
    const GameTexture* dynamicEnvironment = nullptr;
    const GameTexture* environment = nullptr; // +0x11C
    const GameTexture* halfVectorCube = nullptr;  // +0x130: made at the start (makeHalfVectorCube)
    // The rand() of the thread that makes materials (a track's are made by the loading thread); the game's
    // generator when not set.
    CrtRandom* materialRandom = nullptr;
    const GameTexture* normalCube = nullptr;      // +0x134
    // +0x164: the wet road's reflection the world's pass draws (Tarmac Night's stage 3, Water's stage 1), set during
    // the pass only.
    const GameTexture* reflection = nullptr;
    // [0x0075F1B4]: a world's reflection cube is being drawn (Tarmac leaves out the half-vector cube).
    bool drawingCube = false;
    float lightScale = 1.0f;  // +0x12C
    bool night = false;       // +0x38
    bool dimNightLights = false;  // +0x170: set by nothing; Building Opaque's night lights at a quarter (c7.w 0.25)
    uint32_t passKind = 0;    // +0x128
    Material* override = nullptr;  // +0x124
    Vec4 random{};            // +0x14C
    uint32_t cloudMode = 0;   // [0x0075F108]
    uint32_t defaultCull = 3; // [0x0071935C]
    Vec4 objectOffset{};      // +0x138: the node drawn minus the camera
    DeclarationId lastDeclaration = 0;  // [0x0075F144]
    // [0x0079E670]: the detail factor. The renderer's start (0x005A25F0) makes it from the detail 60 (%):
    // 60 x 0.01 x 1.5 + 0.66 (x87 at 53 bits); the graphics options window (0x004820A0) from its slider.
    float lodBias = startDetailFactor();
    // Values of the atmosphere (0x00533B80) that the map's materials do not read.
    uint32_t zenith = 0;             // +0x168
    float lightBlur = 0.0f;          // +0x16C
    float nightRoadShininess = 0.0f; // +0x174
    float roadWetness = 0.0f;        // +0x178
    // +0x188: the atmosphere's +0x1C0 (1): the distances scaled by 2 / (it + 1), the lens flares' reach, the particles'
    // amount, the reflection cubes drawn from 0.8.
    float drawQuality = 0.0f;

private:
    IRenderDevice& m_device;
    ProgramContext& m_programs;
    Material* m_current = nullptr;  // [0x0075F16C]
    uint32_t m_lastClass = 0x10009; // [0x00719540]
    uint32_t m_lastPass = 0;        // [0x0075B2B8]
    std::array<float, 16> m_cloud{};  // 0x007A0B50: c45 .. c48 (the stock's c49 its guard, 0x007A0B90: not set here)
};

}  // namespace oj::render
