// Integration tests: EffectDefs.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Data/EffectDefs.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

using oj::test::check;

}  // namespace

// The effects of a dry race: effectsdefs_com.txt then effectsdefs_dry.txt, at the density of shaders 2.0
// with two threads (8) and at none. A smoke's points double and its growth halves; an effect made the
// same as another takes its scaled values and is scaled again unless it gives its own.
OJ_INTEGRATION_TEST(EffectDefs, DryRace) {
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    const auto com = game.read(std::filesystem::path("scripts") / "effectsdefs_com.txt");
    const auto dry = game.read(std::filesystem::path("scripts") / "effectsdefs_dry.txt");
    bool read = false;
    if (com && dry) {
        auto view = [](const auto& bytes) {
            return std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size());
        };
        oj::EffectLibrary dense;
        dense.read(view(com), 8.0f, false);
        dense.read(view(dry), 8.0f, false);
        oj::EffectLibrary plain;
        plain.read(view(com), 0.0f, false);
        plain.read(view(dry), 0.0f, false);
        const oj::EffectDef* skid = dense.find("SKID_BASE");
        const oj::EffectDef* sand = dense.find("skid_sand");
        const oj::EffectDef* plainSand = plain.find("skid_sand");
        const oj::EffectDef* tarmac = dense.find("skid_Tarmac");
        const oj::EffectDef* sparks = dense.find("chassis_sparks");
        std::printf("     effects: %zu of a dry race; skid_base %u points, growth %.2f; chassis_sparks %u points, "
                    "size %.3f\n", dense.defs().size(), skid ? skid->points : 0, skid ? skid->growthRate : 0.0f,
                    sparks ? sparks->points : 0, sparks ? sparks->startSize : 0.0f);
        read = skid && skid->points == 60 && skid->growthRate == 75.0f && sand && sand->points == 60 &&
               sand->growthRate == 125.0f && sand->gravity == 60.0f && plainSand &&
               plainSand->points == 30 && tarmac && tarmac->points == 120 && tarmac->growthRate == 37.5f &&
               sparks && sparks->type == oj::EffectType::Sparks && sparks->points == 520 &&
               !sparks->hasSound;
    }
    check(read, "effects: definitions of a dry race at the particle density 8 and at none");
}
