#pragma once
// The particle effects' definitions (docs/spec/20-effects.md): scripts\effectsdefs_com.txt, then effectsdefs_dry.txt
// or effectsdefs_wet.txt, read into the effects manager's list ([0x0075F0C8]+0x88) as a race loads (0x00461390).

#include "Core/Base/Matrix.h"
#include "Core/Base/Random.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oj {

// BASE_TYPE (0x00507350).
enum class EffectType : int32_t {
    Smoke = 0,
    Framebuffer = 1,
    Wheelspin = 2,
    Leaves = 3,
    Tracks = 4,
    Sparks = 5,
    Flakes = 6,
    GroundRush = 7,
    Flat = 8,
    Rain = 9,
    Splash = 10,
    Smear = 11,
    Grass = 12,
};

// A sound of an effect (SOUND_EFFECT <name> [<value>], 12 bytes from +0x140): the sound bank's ids of the name and
// the value (1 when not given).
struct EffectSound {
    std::string name;
    int32_t value = 1;
};

// An effect's definition (0x160 bytes, 0x00508630; the defaults of [BeginEffect], 0x00506BE0).
struct EffectDef {
    std::string name;                         // +0x04 (64 bytes)
    std::string material = "smoke_tarmac";    // +0x44 (64 bytes): MATERIAL_NAME
    EffectType type = EffectType::Smoke;      // +0x88
    uint32_t points = 50;                     // +0x8C: NUM_POINTS (scaled by the particle density)
    Vec4 colourStart{128.0f, 128.0f, 128.0f, 35.0f};  // +0x90
    Vec4 colourEnd{128.0f, 128.0f, 128.0f, 0.0f};     // +0xA0
    Vec4 colourVary{10.0f, 10.0f, 10.0f, 0.0f};       // +0xB0
    float brightnessRange = 0.25f;            // +0xC0
    float emitInterval = 0.1f;                // +0xC4
    float ageRange = 0.0f;                    // +0xC8
    float startSize = 20.0f;                  // +0xCC
    float growthRate = 5.0f;                  // +0xD0
    float gravity = 0.0f;                     // +0xD4
    float followFrac = 0.0f;                  // +0xD8
    float followFade = 1.0f;                  // +0xDC
    float rotationRate = 0.3f;                // +0xE0
    float fadeinTime = 0.2f;                  // +0xE4
    float initialTemp = 0.7f;                 // +0xE8
    float coolingRate = 0.02f;                // +0xEC
    float windResistance = 0.95f;             // +0xF0
    Vec4 emitDirection{0.0f, 1.0f, 0.0f, 0.0f};  // +0xF4 (w a value of the stock code's stack)
    float emitSpeed = 10.0f;                  // +0x104
    float speedRange = 3.0f;                  // +0x108
    float randomDrift = 24.0f;                // +0x10C
    float triggerLevel = 0.6f;                // +0x110
    float minAmount = 0.0f;                   // +0x114
    float lifetime = 0.0f;                    // +0x118
    float soundVolume = 0.5f;                 // +0x11C
    float soundRolloff = 0.001f;              // +0x120
    float soundMinDist = 1.0f;                // +0x124
    float soundPitchMul = 1.0f;               // +0x128
    float soundTriggerLevel = 0.3f;           // +0x12C
    float soundMinVol = 0.0f;                 // +0x130
    float soundDoppler = 0.0f;                // +0x134
    int32_t soundTrigger = 1;                 // +0x138: 0 once, 1 a loop
    bool hasSound = false;                    // +0x13C
    std::vector<EffectSound> sounds;          // +0x140
    float lodOffMul = 1000000.0f;             // +0x150: LOD_OFF_MUL x 1000000
    int32_t textures = 1;                     // +0x154: NUM_TEXTURES
    float bounciness = 0.3f;                  // +0x158
    float maxVisSize = 0.1f;                  // +0x15C
};

// The effects of a race: their definitions in the order read (a name is found by its first definition, 0x00503800).
class EffectLibrary {
public:
    // 0x00503610 with the reader's handler (vtable 0x006F70B8): the file's [BeginEffect] .. [EndEffect] blocks added
    // at the end. density: [0x0075F794], the particle density (0: none applied); sound: [0x0075F860], whether the
    // game's sound is on (SOUND_EFFECT is only read then).
    void read(std::string_view text, float density, bool sound);

    // 0x00503800: the first definition of the name (without regard to case), none when there is none.
    const EffectDef* find(std::string_view name) const;

    const std::vector<EffectDef>& defs() const { return m_defs; }

private:
    std::vector<EffectDef> m_defs;  // +0x88
};

// A surface's effects (0x188 bytes, scripts\surfaceeffects.txt, read by the first car's effects object 0x0048FA40):
// the effects' names; the record is kept by its name's place in TARMAC_DRY, SAND_DRY, GRASS_DRY, TARMAC_WET,
// SAND_WET, GRASS_WET, CONCRETE, METAL, WOOD, GLASS (0x0071AB70; an unknown name takes the first place).
struct SurfaceEffect {
    std::string name;      // +0x00
    std::string wheelspin;  // +0x40: WHEELSPIN
    std::string tracks;    // +0x80: TRACKS
    std::string skid;      // +0xC0: SKID
    std::string sparks;    // +0x100: SPARKS
    std::string flakes;    // +0x140: FLAKES
    int32_t rush = 0;      // +0x180: RUSH ground_rush_street 0, ground_rush_scrub 1
};

// [0x0075EF34]: the records by their places; a place no record took is empty.
using SurfaceEffects = std::array<std::optional<SurfaceEffect>, 10>;
SurfaceEffects parseSurfaceEffects(std::string_view text);

// The leaves' tables (0x0050DD60, [0x0075F0F0] once a run), 3000 entries each, made by the first LEAVES emitter on
// the thread that makes it: three draws an entry, the angles of two turns (rand() x 2 pi / 32767), the rate of the
// leaf's tumble (rand() / 65534 + 0.25); its texture's corners by entry % NUM_TEXTURES in an atlas of four across
// (NUM_TEXTURES of that first emitter's definition; read by nothing found); the leaf's two axes when it lies flat,
// turned by the first angle. The airborne leaf's light (0x00757CE0) is written by each frame of every LEAVES
// emitter (0x0050E3D0) and read by the next one's step.
struct LeafTables {
    static constexpr size_t kEntries = 3000;
    bool made = false;                             // [0x0075F0F0]
    std::array<float, kEntries> turnA{};           // 0x00790160 (16-byte entries with B and the rate)
    std::array<float, kEntries> turnB{};           // 0x00790164
    std::array<float, kEntries> rate{};            // 0x00790168
    std::array<float, kEntries> light{};           // 0x00757CE0
    std::array<float, kEntries> u0{}, u1{};        // 0x00754E00, 0x0074C160
    std::array<float, kEntries> v0{}, v1{};        // 0x00751F20, 0x0074F040
    std::array<Vec4, kEntries> flatA{};            // 0x00778A40: (cos A, 0, sin A; w a value of the stack)
    std::array<Vec4, kEntries> flatB{};            // 0x007845D0: (-sin A, 0, cos A)
};
LeafTables& leafTables();

// 0x00503850: an emitter of the effect of the name, held by its definition: none for a name not defined; FLAKES,
// FLAT and GRASS have no emitter of their own (the stock code takes the pointer it was given). The draws of its making
// on the thread that makes it: every emitter starts with 0x0050CE70 (with the sound on, an effect whose sounds loop
// starts one, drawn by rand()); the first LEAVES emitter of the game's run fills the leaves' tables.
// draw: the value drawn for the loop's set (-1 none drawn), for the game's sound of the emitter.
const EffectDef* makeEmitter(const EffectLibrary& library, std::string_view name, CrtRandom& random, bool sound,
                             int32_t* draw = nullptr);

// [0x0075F794] at the device's making (0x00513136): a density given on the command line (-1 .. -7) stays; else 8
// with pixel shaders 2.0 (the classes' 0x40000), threading on and two processors for the game, 0.5 with fixed
// function only (no 0xE0000 class), else 0.
float particleDensity(float commandLine, uint32_t shaderClasses, uint32_t threading, uint32_t processors);

}  // namespace oj
