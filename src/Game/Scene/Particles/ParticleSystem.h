#pragma once
// The particles of a race (the effects manager [0x0075F0C8], docs/spec/20-effects.md): the emitters by definition,
// made, stepped and drawn.

#include "Audio/Voices.h"
#include "Core/Base/Random.h"
#include "Core/Data/EffectDefs.h"
#include "Core/Formats/Cld.h"
#include "Core/Tracks/Atmosphere.h"
#include "Core/Tracks/SunMap.h"
#include "Game/Scene/Particles/Emitter.h"
#include "Render/Camera.h"
#include "Render/Materials.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace oj::scene {

class ParticleSystem {
public:
    // +0x80: the race's 3D world, given to the emitters made while it is (none without the sound).
    audio::World* soundWorld = nullptr;
    ParticleSystem(Engine& engine, const EffectLibrary& library);
    ~ParticleSystem();

    // 0x00503850: none for a name not defined or of the types Flakes, Flat and Grass (6, 8, 12: the stock code makes
    // none of them, 0x0050387B).
    // `random`: the rand() of the thread that makes it (SPLASH draws its splashes' first frames); the game's when not
    // given.
    std::shared_ptr<Emitter> make(const std::string& name, CrtRandom* random = nullptr);
    // 0x005339A0 → 0x00503480: the sun's colour on particles (+0xA0) and the shade's (+0xB0) from an atmosphere.
    void setColours(const Atmosphere& a);
    // 0x00505030 with the race's clock: the frame's time, the axes, the wind, each list's emitters stepped and drawn.
    void frame(render::Camera& camera, float clock);
    // 0x005043A0 (0x004254AF, the end of the race track's making): each list (+0x18, +0x28, +0x38, +0x48, +0x08,
    // +0x58) sorted by the effects' BASE_TYPE, the higher first (0x00504370; the same type by the emitters' addresses,
    // here the order they were made).
    void sortLists();
    // Slot 2 (0x00504ED0), called by nothing found: unless [0x0075F0D8], the vertex constants c0 .. c2 and c4 .. c6
    // the identity and c18 .. c21 the camera's (its slot 0x4C), CULLMODE none, the declaration, the FRAMEBUFFER
    // emitters' frames (0x00504290) and the default cull again.
    void drawFramebuffer(render::Camera& camera);
    // Slot 1 (0x005041F0) from the view's end with the screen's smears (0x005224E0): the view's corners kept (+0x100
    // .. +0x10C) and each SMEAR emitter's frame.
    void drawSmears(float x0, float y0, float x1, float y1, render::Camera& camera);
    std::array<float, 4> screen{};           // +0x100 .. +0x10C

    ParticleWind& wind() { return m_wind; }
    ParticleRandom& random() { return laggedRandom(); }
    const SunMap* sunMap = nullptr;          // +0x140
    // The world's collision (the manager's +0x9C world's +0x24) the road's height comes from (0x00504630).
    const Cld* collision = nullptr;
    // 0x00504630: the height of the highest face under (x, z); false when none.
    bool groundBelow(float x, float z, float& y) const;
    float quality = 1.0f;                    // +0xC8 (the renderer's +0x188)
    float density = 0.0f;                    // [0x0075F794]: the particle density
    // The job thread's rand() (the loading thread's): the leaves' steps run there (0x00509870).
    CrtRandom* jobRandom = nullptr;

    Engine& engine() { return m_engine; }
    render::Material* material(const std::string& name);
    render::DeclarationId declaration() const { return m_declaration; }
    render::BufferId quadIndices(uint32_t quads);

    float time = 0.0f;                       // +0xC0
    float dt = 0.0f;                         // +0xC4
    Vec4 sunColour{};                        // +0xA0
    Vec4 shadeColour{};                      // +0xB0
    std::array<Vec4, 3> billboard{};         // +0x110, +0x120, +0x130
    std::array<Vec4, 3> cameraAxes{};        // +0xD0, +0xE0, +0xF0: the camera's x, y and z

private:
    Engine& m_engine;
    const EffectLibrary& m_library;
    ParticleWind m_wind;
    using List = std::vector<std::shared_ptr<Emitter>>;
    // 0x00504290: each emitter's frame; the last that answers false leaves the list (its sound's place, slot 7, is
    // not made here).
    static void frameList(List& list, render::Camera& camera);

    List m_framebuffer;  // +0x08: FRAMEBUFFER
    List m_smoke;        // +0x18: SMOKE, WHEELSPIN
    List m_sparks;       // +0x28: SPARKS
    List m_scenery;      // +0x38: LEAVES, GROUNDRUSH, RAIN, SPLASH
    List m_tracks;       // +0x48: TRACKS
    List m_smears;       // +0x58: SMEAR
    std::map<std::string, std::unique_ptr<render::Material>> m_materials;
    render::DeclarationId m_declaration = 0;  // +0x144
    render::BufferId m_indices = 0;           // [0x0075F0E0]
    uint32_t m_indexQuads = 0;
    float m_load = 0.0f;                      // [0x0075F0D0]
};

}  // namespace oj::scene
