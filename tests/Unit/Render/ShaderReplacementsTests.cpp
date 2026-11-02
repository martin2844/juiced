// Unit tests: the fixed build's shader replacements (Render/ShaderReplacements.h): a shader's canonical length and hash
// without its comments; nothing replaced when off or for a shader not in the table; the car's effects replaced by
// CAR PAINT and REFLECTIONS ULTRA.

#include "Framework/Test.h"

#include "Render/ShaderReplacements.h"

#include <cstdint>

namespace {

using oj::test::check;

}  // namespace

OJ_UNIT_TEST(ShaderReplacements, Canonical) {
    // vs_1_1, mov oPos, v0, end; the same with a comment of two words (CTAB) after the version.
    const uint32_t plain[] = {0xFFFE0101, 0x00000001, 0xC00F0000, 0x90E40000, 0x0000FFFF};
    const uint32_t commented[] = {0xFFFE0101, 0x0002FFFE, 0x42415443, 0x00000000,
                                  0x00000001, 0xC00F0000, 0x90E40000, 0x0000FFFF};
    size_t tokensA = 0, tokensB = 0;
    uint32_t hashA = 0, hashB = 0;
    check(oj::render::canonicalShader(plain, tokensA, hashA) && oj::render::canonicalShader(commented, tokensB, hashB),
          "both end");
    check(tokensA == 5 && tokensB == 5 && hashA == hashB, "the comment left out: 5 tokens, the same hash");
}

// CAR PAINT replaces the paint, the glass and the chrome; REFLECTIONS ULTRA alone the paint (drawn as the game's, lit
// by the cars' lights); neither none. WORLD LIGHT ULTRA replaces the trees', whatever the car's.
OJ_UNIT_TEST(ShaderReplacements, CarEffects) {
    using oj::render::CarEffects;
    using oj::render::carEffectsOf;
    using oj::render::ReplacedEffects;
    using oj::render::ReplacedMaterial;
    using oj::render::takesReplacement;
    check(carEffectsOf(false, false) == CarEffects::Game && carEffectsOf(false, true) == CarEffects::LitPaint &&
              carEffectsOf(true, false) == CarEffects::Replaced && carEffectsOf(true, true) == CarEffects::Replaced,
          "CAR PAINT and REFLECTIONS ULTRA to the car's effects");
    const uint8_t byte = 0;
    const oj::render::EffectReplacement paint{&byte, 1, "paint", ReplacedMaterial::Paint};
    const oj::render::EffectReplacement glass{&byte, 1, "glass", ReplacedMaterial::Glass};
    const oj::render::EffectReplacement tree{&byte, 1, "tree", ReplacedMaterial::Tree};
    const oj::render::EffectReplacement none{};
    const ReplacedEffects game{}, lit{CarEffects::LitPaint, false}, replaced{CarEffects::Replaced, false};
    const ReplacedEffects trees{CarEffects::Game, true};
    check(!takesReplacement(game, paint) && !takesReplacement(game, glass) && !takesReplacement(game, tree),
          "the game's: none taken");
    check(takesReplacement(lit, paint) && !takesReplacement(lit, glass) && !takesReplacement(lit, tree),
          "REFLECTIONS ULTRA alone: the paint's, not the glass's nor the chrome's");
    check(takesReplacement(replaced, paint) && takesReplacement(replaced, glass) && !takesReplacement(replaced, tree) &&
              !takesReplacement(replaced, none),
          "CAR PAINT: each of the car's found");
    check(takesReplacement(trees, tree) && !takesReplacement(trees, paint) && !takesReplacement(trees, glass),
          "WORLD LIGHT ULTRA: the trees' alone");
}

OJ_UNIT_TEST(ShaderReplacements, NotInTable) {
    const uint32_t plain[] = {0xFFFE0101, 0x00000001, 0xC00F0000, 0x90E40000, 0x0000FFFF};
    oj::render::ShaderReplacements r;
    check(oj::render::replaceShader(plain, true, r).empty(), "off: none");
    r.features = static_cast<unsigned>(oj::render::ShaderFeature::WorldLight);
    r.shaders20 = true;
    check(oj::render::replaceShader(plain, true, r).empty(), "a shader not in the table: none");
    check(oj::render::replaceShader(plain, false, r).empty(), "a vertex shader asked as a pixel shader: none");
}
