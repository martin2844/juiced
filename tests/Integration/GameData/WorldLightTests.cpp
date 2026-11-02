// Integration tests: the fixed build's world light (Render/ShaderReplacements, JuicedConfigExt's WorldLight) over the
// shaders of every effect of effects\: each shader of its table found in the game's and replaced; the shaders 1.x left
// alone below the game's shaders 2.0; the strength in the replacement's literals; the light cases' variants of the
// pixel shaders they share.

#include "Framework/Fixtures.h"
#include "Framework/Test.h"

#include "Core/Base/GameFiles.h"
#include "Core/Formats/Effect.h"
#include "Render/ShaderReplacements.h"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace {

using oj::test::bytesOf;
using oj::test::check;
using oj::render::LightCase;
using oj::render::ShaderFeature;
using oj::render::ShaderReplacements;

// Every shader of the game's effects (vertex and pixel).
std::vector<std::vector<uint32_t>> gameShaders() {
    std::vector<std::vector<uint32_t>> shaders;
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return shaders;
    const std::filesystem::path folder = game.root() / L"effects";
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(folder, ec); it != std::filesystem::directory_iterator();
         it.increment(ec)) {
        if (ec || it->path().extension() != L".fxo") continue;
        const auto content = oj::GameFiles(folder).read(it->path().filename());
        if (!content) continue;
        const auto effect = oj::fx::parseEffect(bytesOf(*content));
        if (!effect) continue;
        for (const std::vector<uint8_t>& object : effect->objects) {
            std::vector<uint32_t> code = oj::fx::words(object);
            if (!code.empty() && (code[0] & 0xFFFE0000u) == 0xFFFE0000u) shaders.push_back(std::move(code));
        }
    }
    return shaders;
}

// The names of the table's shaders replaced among the game's, in every light case.
std::set<std::string> replaced(const std::vector<std::vector<uint32_t>>& shaders, const ShaderReplacements& r) {
    std::set<std::string> names;
    for (const std::vector<uint32_t>& code : shaders) {
        for (const LightCase c : oj::render::kLightCases) {
            const char* name = nullptr;
            if (!oj::render::replaceShader(code.data(), (code[0] & 0xFFFF0000u) == 0xFFFE0000u, r, &name, c)
                     .empty()) {
                names.insert(name);
            }
        }
    }
    return names;
}

}  // namespace

OJ_INTEGRATION_TEST(WorldLight, GameShaders) {
    const auto shaders = gameShaders();
    check(!shaders.empty(), "the game's effects read");
    if (shaders.empty()) return;
    ShaderReplacements r;
    r.features = static_cast<unsigned>(ShaderFeature::WorldLight);
    r.shaders20 = true;
    const std::set<std::string> all = replaced(shaders, r);
    const std::vector<const char*> table = oj::render::replacementNames(ShaderFeature::WorldLight);
    bool every = !table.empty();
    for (const char* name : table) every = every && all.contains(name);
    check(every && all.size() == std::set<std::string>(table.begin(), table.end()).size(),
          "at shaders 2.0: every one of the table's " + std::to_string(table.size()) + " shaders found and replaced");
    r.shaders20 = false;
    const std::set<std::string> some = replaced(shaders, r);
    check(!some.empty() && some.size() < all.size(), "below shaders 2.0: the shaders 1.x left alone (" +
                                                         std::to_string(some.size()) + " of " +
                                                         std::to_string(all.size()) + ")");
    r.features = 0;
    check(replaced(shaders, r).empty(), "off: nothing replaced");
}

// The strength in the literals: 0 and 2 give other codes of the same length.
OJ_INTEGRATION_TEST(WorldLight, Strength) {
    const auto shaders = gameShaders();
    ShaderReplacements weak, strong;
    weak.features = strong.features = static_cast<unsigned>(ShaderFeature::WorldLight);
    weak.shaders20 = strong.shaders20 = true;
    weak.worldLightStrength = 0.0f;
    strong.worldLightStrength = 2.0f;
    bool differs = false;
    for (const std::vector<uint32_t>& code : shaders) {
        const bool vertex = (code[0] & 0xFFFF0000u) == 0xFFFE0000u;
        const auto a = oj::render::replaceShader(code.data(), vertex, weak);
        const auto b = oj::render::replaceShader(code.data(), vertex, strong);
        if (!a.empty() && a.size() == b.size() && a != b) differs = true;
    }
    check(differs, "the strength changes the replacements' literals");
}

// The headlights' shaders: each of the table's found among the game's and replaced at shaders 2.0 (with the sun's
// shadows too: the tunnel's by a dry day).
OJ_INTEGRATION_TEST(WorldLight, HeadlightShaders) {
    const auto shaders = gameShaders();
    ShaderReplacements r;
    r.features = static_cast<unsigned>(ShaderFeature::Headlights) | static_cast<unsigned>(ShaderFeature::SunShadows);
    r.shaders20 = true;
    const std::set<std::string> all = replaced(shaders, r);
    const std::vector<const char*> table = oj::render::replacementNames(ShaderFeature::Headlights);
    bool every = !table.empty();
    for (const char* name : table) every = every && all.contains(name);
    check(every, "every one of the table's " + std::to_string(table.size()) + " headlight shaders found and replaced");
}

// The pixel shaders with both blocks, Standard Opaque 3.0's (the day and the night share it) and the day's road (its
// lights on in the rain): with the headlights and the sun's shadows on a dry day's variant (the sun's alone), a
// tunnel's by a dry day (both) and the other cases' (the headlights'); with one of them that one's in every case (no
// block of the other).
OJ_INTEGRATION_TEST(WorldLight, LightCaseVariants) {
    const auto shaders = gameShaders();
    ShaderReplacements both, sun, headlights;
    both.features = static_cast<unsigned>(ShaderFeature::Headlights) | static_cast<unsigned>(ShaderFeature::SunShadows);
    sun.features = static_cast<unsigned>(ShaderFeature::SunShadows);
    headlights.features = static_cast<unsigned>(ShaderFeature::Headlights);
    both.shaders20 = sun.shaders20 = headlights.shaders20 = true;
    std::set<std::string> split;
    bool cases = true, alone = true;
    for (const std::vector<uint32_t>& code : shaders) {
        if ((code[0] & 0xFFFF0000u) != 0xFFFF0000u) continue;
        const char* dry = nullptr;
        const char* other = nullptr;
        const auto day = oj::render::replaceShader(code.data(), false, both, &dry, LightCase::DayDry);
        const auto night = oj::render::replaceShader(code.data(), false, both, &other, LightCase::NightDry);
        if (day.empty() || day == night) continue;
        const std::string base = std::string(other).substr(0, std::string(other).find("_daywet"));
        split.insert(base);
        cases = cases && std::string(dry) == base + "_daydry" &&
                std::string(other) == base + "_daywet_nightdry_nightwet";
        for (const LightCase c : {LightCase::DayWet, LightCase::NightWet}) {
            cases = cases && oj::render::replaceShader(code.data(), false, both, nullptr, c) == night;
        }
        const char* tunnel = nullptr;
        const auto inTunnel = oj::render::replaceShader(code.data(), false, both, &tunnel, LightCase::DayTunnel);
        cases = cases && std::string(tunnel) == base + "_daytunnel" && inTunnel != day && inTunnel != night;
        for (const LightCase c : oj::render::kLightCases) {
            const char* only = nullptr;
            alone = alone && !oj::render::replaceShader(code.data(), false, sun, &only, c).empty() &&
                    std::string(only) == dry;
            alone = alone && !oj::render::replaceShader(code.data(), false, headlights, &only, c).empty() &&
                    std::string(only) == other;
        }
    }
    check(split == std::set<std::string>{"material_stdopaqueshad_2", "material_tarmac_0"},
          "both on: a dry day's and the others' (Standard Opaque 3.0, the day's road)");
    check(cases, "a dry day the sun's, a tunnel's day both, a wet day and the nights the headlights'");
    check(alone, "one on: its variant in every case");
}
