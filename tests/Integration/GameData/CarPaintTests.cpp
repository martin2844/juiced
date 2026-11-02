// Integration tests: the fixed build's car paint, glass and chrome and trees (Render/ShaderReplacements replaceEffect,
// JuicedConfigExt's CarPaint and Trees) over every effect of effects\: the game's five files known, each replacement
// read, the car's with techniques of the shaders 2.0 (the per-pixel ones; the game's glass and chrome have none), the
// paint's with OpenJuice's shared parameters and CarPaint3 (shaders 3.0), the trees' with a technique of the shaders
// 3.0 and the sun's cascades' shared copy; any other effect left alone.

#include "Framework/Fixtures.h"
#include "Framework/Test.h"

#include "Core/Base/GameFiles.h"
#include "Core/Formats/Effect.h"
#include "Render/ShaderReplacements.h"

#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace {

using oj::test::bytesOf;
using oj::test::check;

// An effect has a technique of a shader class (its CLASSIFICATION annotation has the mask: 0x40000 the shaders 2.0,
// 0x80000 3.0).
bool hasClass(const oj::fx::Effect& effect, uint32_t mask) {
    for (const oj::fx::Technique& t : effect.techniques) {
        const oj::fx::Parameter* c = t.annotation("CLASSIFICATION");
        uint32_t classes = 0;
        if (c != nullptr && c->value.size() >= 4) std::memcpy(&classes, c->value.data(), 4);
        if ((classes & mask) != 0) return true;
    }
    return false;
}

// A shared parameter of an effect (D3DX_PARAMETER_SHARED) with that many elements (0: not an array).
bool sharedParameter(const oj::fx::Effect& effect, const char* name, uint32_t elements) {
    const oj::fx::Parameter* p = effect.parameter(name);
    return p != nullptr && (p->flags & 1u) != 0 && p->elements == elements;
}

}  // namespace

OJ_INTEGRATION_TEST(CarPaint, GameEffects) {
    const oj::GameFiles& game = oj::test::gameFiles();
    check(game.valid(), "the game's files");
    if (!game.valid()) return;
    const std::filesystem::path folder = game.root() / L"effects";
    std::set<std::string> found;
    size_t others = 0;
    bool readable = true, perPixel = true, glassChromeWithout = true, paintLit = false, treesLit = true;
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(folder, ec); it != std::filesystem::directory_iterator();
         it.increment(ec)) {
        if (ec || it->path().extension() != L".fxo") continue;
        const auto content = oj::GameFiles(folder).read(it->path().filename());
        if (!content) continue;
        const std::vector<uint8_t> bytes = bytesOf(*content);
        const oj::render::EffectReplacement r = oj::render::replaceEffect(bytes.data(), bytes.size());
        if (r.bytes == nullptr) {
            ++others;
            continue;
        }
        found.insert(r.name);
        const auto original = oj::fx::parseEffect(bytes);
        const auto replaced = oj::fx::parseEffect(std::vector<uint8_t>(r.bytes, r.bytes + r.size));
        readable = readable && original && replaced;
        const bool tree = r.material == oj::render::ReplacedMaterial::Tree ||
                          r.material == oj::render::ReplacedMaterial::TreeSprite;
        if (tree) {
            // The trees': the game's techniques, then one of the shaders 3.0 with the sun's cascades' copy.
            treesLit = treesLit && original && replaced &&
                       replaced->techniques.size() == original->techniques.size() + 1 &&
                       hasClass(*replaced, 0x80000u) && !hasClass(*original, 0x80000u) &&
                       sharedParameter(*replaced, "SunCascades", 17) && sharedParameter(*replaced, "SunShadowsOn", 0);
            continue;
        }
        perPixel = perPixel && replaced && hasClass(*replaced, 0x40000u);
        const bool paint = r.material == oj::render::ReplacedMaterial::Paint;
        if (!paint) glassChromeWithout = glassChromeWithout && original && !hasClass(*original, 0x40000u);
        if (paint && replaced) {
            paintLit = hasClass(*replaced, 0x80000u) && sharedParameter(*replaced, "StockPaint", 0) &&
                       sharedParameter(*replaced, "CarHeadlight", 25) && sharedParameter(*replaced, "CarLamp", 8);
        }
    }
    check(found == std::set<std::string>{"paint", "glass", "chrome", "tree", "tree sprite"},
          "the paint, the glass, the chrome and the trees known");
    check(treesLit, "the trees': the game's techniques and one of the shaders 3.0, the shared SunCascades[17] and "
                    "SunShadowsOn");
    check(readable, "each replacement read as an effect");
    check(perPixel && glassChromeWithout, "each with techniques of the shaders 2.0, the game's glass and chrome without");
    check(paintLit, "the paint's: CarPaint3 (shaders 3.0), the shared StockPaint, CarHeadlight[25] and CarLamp[8]");
    check(others > 50, "the other effects left alone (" + std::to_string(others) + ")");
}
