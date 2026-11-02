#include "Render/ShaderReplacements.h"

#include <windows.h>

#include <cstring>
#include <iterator>

#include <Shaders/CarPaintEditions.h>
#include <Shaders/TreeEditions.h>
#include <Shaders/WorldLightShaders.h>

namespace oj::render {

namespace {

constexpr size_t kMaxTokens = 0x10000;

// The light cases' bits are the shader table's (WorldLight.py's CASES).
static_assert(static_cast<unsigned>(LightCase::DayDry) == WorldLightShaders::DayDry &&
              static_cast<unsigned>(LightCase::DayWet) == WorldLightShaders::DayWet &&
              static_cast<unsigned>(LightCase::NightDry) == WorldLightShaders::NightDry &&
              static_cast<unsigned>(LightCase::NightWet) == WorldLightShaders::NightWet &&
              static_cast<unsigned>(LightCase::DayTunnel) == WorldLightShaders::DayTunnel);

}  // namespace

bool canonicalShader(const uint32_t* code, size_t& tokens, uint32_t& hash) {
    tokens = 0;
    hash = 0x811C9DC5u;
    const auto add = [&](uint32_t token) {
        for (int shift = 0; shift < 32; shift += 8) hash = (hash ^ ((token >> shift) & 0xFF)) * 0x01000193u;
        ++tokens;
    };
    if (code == nullptr) return false;
    add(code[0]);
    for (size_t i = 1; i < kMaxTokens;) {
        const uint32_t token = code[i];
        if ((token & 0xFFFF) == 0xFFFE) {  // a comment: its length in the high word
            i += 1 + ((token >> 16) & 0x7FFF);
            continue;
        }
        add(token);
        ++i;
        if (token == 0x0000FFFF) return true;
    }
    return false;
}

std::vector<uint32_t> replaceShader(const uint32_t* code, bool vertex, const ShaderReplacements& replacements,
                                    const char** name, LightCase lightCase) {
    if (!replacements.any() || code == nullptr) return {};
    const uint32_t type = vertex ? 0xFFFE0000u : 0xFFFF0000u;
    size_t tokens = 0;
    uint32_t hash = 0;
    if ((code[0] & 0xFFFF0000u) != type || !canonicalShader(code, tokens, hash)) return {};
    const auto kind = vertex ? WorldLightShaders::Kind::Vertex : WorldLightShaders::Kind::Pixel;
    const float strength =
        (replacements.features & static_cast<unsigned>(ShaderFeature::WorldLight)) != 0 ? replacements.worldLightStrength
                                                                                         : 0.0f;
    // Of the shader's variants with a feature on: the case's whose blocks are all on, else the first whose blocks are
    // all on (no block of a feature off: the tunnel's with the headlights off, the sun's and the headlights'), else
    // the first.
    const WorldLightShaders::Shader* exact = nullptr;
    const WorldLightShaders::Shader* whole = nullptr;
    const WorldLightShaders::Shader* first = nullptr;
    for (const WorldLightShaders::Shader& shader : WorldLightShaders::kShaders) {
        if (shader.kind != kind || shader.originalTokens != tokens || shader.originalHash != hash) continue;
        if ((shader.features & replacements.features) == 0 || (!shader.allLevels && !replacements.shaders20)) continue;
        if (first == nullptr) first = &shader;
        if ((shader.features & ~replacements.features) != 0) continue;
        if (whole == nullptr) whole = &shader;
        if ((shader.cases & static_cast<unsigned>(lightCase)) != 0) {
            exact = &shader;
            break;
        }
    }
    const WorldLightShaders::Shader* chosen = exact != nullptr ? exact : whole != nullptr ? whole : first;
    if (chosen != nullptr) {
        const WorldLightShaders::Shader& shader = *chosen;
        std::vector<uint32_t> replaced(shader.code, shader.code + shader.tokens);
        if (shader.skyLiteral != WorldLightShaders::kNoLiteral) {
            // c95: (sky, side, 1 - sky) of the strength.
            const float sky = WorldLightShaders::kSky * strength;
            const float side = WorldLightShaders::kSide * strength;
            const float up = 1.0f - sky;
            std::memcpy(&replaced[shader.skyLiteral], &sky, sizeof(float));
            std::memcpy(&replaced[shader.skyLiteral + 1], &side, sizeof(float));
            std::memcpy(&replaced[shader.skyLiteral + 2], &up, sizeof(float));
        }
        if (name != nullptr) *name = shader.name;
        return replaced;
    }
    return {};
}

EffectReplacement replaceEffect(const uint8_t* file, size_t size) {
    if (const CarPaintEditions::Edition* car = CarPaintEditions::Find(file, size)) {
        constexpr ReplacedMaterial kMaterials[] = {ReplacedMaterial::Paint, ReplacedMaterial::Glass,
                                                   ReplacedMaterial::Chrome};
        return {car->replacement, car->replacementSize, CarPaintEditions::kMaterialNames[car->material],
                kMaterials[car->material]};
    }
    if (const TreeEditions::Edition* tree = TreeEditions::Find(file, size)) {
        return {tree->replacement, tree->replacementSize, TreeEditions::kMaterialNames[tree->material],
                tree->material == TreeEditions::Sprite ? ReplacedMaterial::TreeSprite : ReplacedMaterial::Tree};
    }
    return {};
}

CarEffects carEffectsOf(bool carPaint, bool ultraReflections) {
    if (carPaint) return CarEffects::Replaced;
    return ultraReflections ? CarEffects::LitPaint : CarEffects::Game;
}

bool takesReplacement(const ReplacedEffects& effects, const EffectReplacement& r) {
    if (r.bytes == nullptr) return false;
    switch (r.material) {
        case ReplacedMaterial::Paint: return effects.car != CarEffects::Game;
        case ReplacedMaterial::Glass:
        case ReplacedMaterial::Chrome: return effects.car == CarEffects::Replaced;
        case ReplacedMaterial::Tree:
        case ReplacedMaterial::TreeSprite: return effects.trees;
    }
    return false;
}

std::vector<const char*> replacementNames(ShaderFeature feature) {
    std::vector<const char*> names;
    for (const WorldLightShaders::Shader& shader : WorldLightShaders::kShaders) {
        if ((shader.features & static_cast<unsigned>(feature)) != 0) names.push_back(shader.name);
    }
    return names;
}

}  // namespace oj::render
