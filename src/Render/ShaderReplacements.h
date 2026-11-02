#pragma once
// The fixed build's replacements of the game's shaders (docs/fixes.md Added): JuicedConfigExt's WorldLight (the sky
// light of the world by the surface's direction) and Headlights, the shaders of the lit materials as its
// Shared/Shaders/WorldLight.py patched them (Shared/Shaders/WorldLightShaders.h), found by the length and the FNV-1a of
// the game's code without its comments. The device makes the replacement in the game's place; the effects keep reading
// the game's code (its constant table).

#include "Render/LightCase.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace oj::render {

enum class ShaderFeature : unsigned { WorldLight = 1, Headlights = 2, SunShadows = 4 };

struct ShaderReplacements {
    unsigned features = 0;          // ShaderFeature bits that are on
    float worldLightStrength = 1.0f;  // 0 .. 2 (the registry's WorldLightStrength / 100)
    bool shaders20 = false;         // the game's shader classes have 2.0 (0x40000): its shaders 1.x change too

    bool any() const { return features != 0; }
};

// The length (tokens) and FNV-1a of a shader's code without its comments; false when it does not end within 64K
// tokens.
bool canonicalShader(const uint32_t* code, size_t& tokens, uint32_t& hash);

// The replacement of `code` (a vertex shader, else a pixel shader) for the light case `lightCase` when it is a shader
// of the table one of whose features is on (a shader 1.x only with `shaders20`), the world light's literals at its
// strength; empty when it stays the game's. Of a shader's variants (Render/LightCase) the one serving the case (a dry
// day's: the sun's block alone; a tunnel's by a dry day: both; the other cases': the headlights') when its features are
// all on, else the first whose features are all on, else the first with one on. `name`: the table's name of the
// variant.
std::vector<uint32_t> replaceShader(const uint32_t* code, bool vertex, const ShaderReplacements& replacements,
                                    const char** name = nullptr, LightCase lightCase = kBaseLightCase);

// The table's shaders with a feature (for the tests): their names.
std::vector<const char*> replacementNames(ShaderFeature feature);

// The materials whose effects the fixed build replaces.
enum class ReplacedMaterial : uint8_t { Paint, Glass, Chrome, Tree, TreeSprite };

// JuicedConfigExt's CarPaint and Trees: an effect file of the car paint, glass or chrome or of the trees of a known
// edition (its size and FNV-1a; Shared/Shaders/CarPaintEditions.h, TreeEditions.h) and its replacement, the game's
// techniques with a per-pixel one at shaders 2.0 and up (Shared/Shaders/CarPaint.fx, CarGlass.fx, Chrome.fx; Trees.fx
// at 3.0). `size` 0 and no bytes when it is no such file; `name` the material's ("paint", "glass", "chrome", "tree",
// "tree sprite").
struct EffectReplacement {
    const uint8_t* bytes = nullptr;
    size_t size = 0;
    const char* name = nullptr;
    ReplacedMaterial material = ReplacedMaterial::Paint;
};
EffectReplacement replaceEffect(const uint8_t* file, size_t size);

// Which of the car's effects the fixed build replaces.
enum class CarEffects : uint8_t {
    Game,      // none: the game's
    LitPaint,  // REFLECTIONS ULTRA with CAR PAINT off: the paint alone, drawn as the game's (its StockPaint), for its
               // CarPaint3 lighting the cars; the glass and the chrome the game's
    Replaced,  // CAR PAINT: the paint, the glass and the chrome
};
CarEffects carEffectsOf(bool carPaint, bool ultraReflections);

// What the fixed build replaces of the game's effects: the car's, and the trees' with WORLD LIGHT ULTRA (their leaves
// in the sun's cascaded shadow maps).
struct ReplacedEffects {
    CarEffects car = CarEffects::Game;
    bool trees = false;

    bool operator==(const ReplacedEffects&) const = default;
};
// Whether `r` (a replacement found) is taken with `effects`.
bool takesReplacement(const ReplacedEffects& effects, const EffectReplacement& r);

}  // namespace oj::render
