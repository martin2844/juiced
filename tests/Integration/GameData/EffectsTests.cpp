// Integration tests: Effects.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Formats/Ddt.h"
#include "Core/Formats/Effect.h"

#include <cstdio>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using oj::test::check;
using oj::test::bytesOf;

}  // namespace

// Every effect of effects\: the file read to its end, each shader with its constant table, each
// expression and preshader a known program.
OJ_INTEGRATION_TEST(Effects, EveryEffect) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    const std::filesystem::path folder = game.root() / L"effects";
    size_t effects = 0, bad = 0, shaders = 0, withTable = 0, programs = 0, programsBad = 0, preshaders = 0;
    std::map<std::string, oj::fx::Effect> byName;
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(folder, ec);
         it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (ec || it->path().extension() != L".fxo") continue;
        const auto content = oj::GameFiles(folder).read(it->path().filename());
        if (!content) continue;
        ++effects;
        const auto effect = oj::fx::parseEffect(bytesOf(*content));
        if (!effect) {
            ++bad;
            continue;
        }
        for (const oj::fx::Technique& t : effect->techniques) {
            for (const oj::fx::Pass& p : t.passes) {
                for (const oj::fx::State& s : p.states) {
                    if (s.kind == oj::fx::StateValue::Expression) {
                        ++programs;
                        if (!oj::fx::parseProgram(s.code)) ++programsBad;
                    }
                }
            }
        }
        for (const std::vector<uint8_t>& object : effect->objects) {
            const std::vector<uint32_t> code = oj::fx::words(object);
            if (code.empty() || (code[0] & 0xFFFE0000u) != 0xFFFE0000u) continue;
            ++shaders;
            if (oj::fx::constantTable(code)) ++withTable;
            const auto pres = oj::fx::comment(code, "PRES");
            if (pres && !pres->empty()) {
                ++preshaders;
                if (!oj::fx::parseProgram(*pres)) ++programsBad;
            }
        }
        byName[it->path().stem().string()] = std::move(*effect);
    }
    std::printf("     effects: %zu (%zu not read), %zu shaders (%zu with a constant table), %zu expressions, "
                "%zu preshaders (%zu not read)\n",
                effects, bad, shaders, withTable, programs, preshaders, programsBad);
    check(effects == 74 && bad == 0 && programsBad == 0 && programs == 15 && preshaders > 0,
          "effects: every file, expression and preshader read");

    bool known = false;
    const auto opaque = byName.find("material_stdopaqueshad");
    const auto night = byName.find("material_stdopaqueshadnight");
    const auto tarmac = byName.find("material_tarmacnoshadow");
    if (opaque != byName.end() && night != byName.end() && tarmac != byName.end()) {
        const oj::fx::Effect& e = opaque->second;
        const oj::fx::Parameter* cls =
            e.techniques.size() == 6 ? e.techniques[0].annotation("CLASSIFICATION") : nullptr;
        const oj::fx::Parameter* cloud = e.parameter("cloudCastTexture");
        const oj::fx::Parameter* name = cloud ? cloud->annotation("name") : nullptr;
        const oj::fx::Parameter* sampler = e.parameter("cloudSampler");
        // tech3's pass: the pixel shader's constant table puts g_psAmbientColor at c0.
        const oj::fx::Pass& pass = e.techniques[5].passes[0];
        const oj::fx::State& ps = pass.states.back();
        const auto table = ps.kind == oj::fx::StateValue::Constant
                               ? oj::fx::constantTable(oj::fx::words(e.objects[ps.value.object]))
                               : std::nullopt;
        bool ambientAt0 = false;
        if (table) {
            for (const oj::fx::Constant& c : table->constants) {
                if (c.name == "g_psAmbientColor") ambientAt0 = c.registerIndex == 0;
            }
        }
        // The night 1.x pass sets c0 = vAmbUnit * 0.5 by an expression.
        const oj::fx::State* c0 = nullptr;
        for (const oj::fx::State& s : night->second.techniques[0].passes[0].states) {
            if (s.kind == oj::fx::StateValue::Expression) c0 = &s;
        }
        oj::fx::Registers regs;
        regs.input = {0.2f, 0.4f, 0.6f, 0.8f};
        const auto program = c0 ? oj::fx::parseProgram(c0->code) : std::nullopt;
        if (program) oj::fx::run(*program, regs);
        const bool half = program && program->inputs.constants.size() == 1 &&
                          program->inputs.constants[0].name == "vAmbUnit" && regs.output.size() == 4 &&
                          regs.output[0] == 0.1f && regs.output[3] == 0.4f;
        // tarmacnoshadow's 2.x pixel shaders: c2.x = cNoiseyVolume * 0.4 by the preshader.
        bool scaled = false;
        for (const std::vector<uint8_t>& object : tarmac->second.objects) {
            const auto pres = oj::fx::comment(oj::fx::words(object), "PRES");
            if (!pres || pres->empty()) continue;
            const auto p = oj::fx::parseProgram(*pres);
            if (!p) continue;
            oj::fx::Registers r;
            r.input = {2.0f, 0.0f, 0.0f, 0.0f, 7.0f, 0.0f, 0.0f, 0.0f};  // cNoiseyVolume, cNoiseScaler1
            oj::fx::run(*p, r);
            scaled = r.output.size() >= 25 && r.output[8] == 0.8f && r.output[20] == 1.0f &&
                     r.output[24] == 0.1f;
            break;
        }
        known = cls && cls->value.size() == 4 && *reinterpret_cast<const int32_t*>(cls->value.data()) ==
                                                      131072 &&
                e.techniques[5].name == "tech3" && name && e.string(name->object) == "cloudcast.dds" &&
                sampler && sampler->samplerStates.size() == 6 &&
                sampler->samplerStates[0].kind == oj::fx::StateValue::Parameter &&
                sampler->samplerStates[0].text == "cloudCastTexture" && ambientAt0 && half && scaled;
    }
    check(known, "effects: techniques, annotations, sampler states, constant tables and expressions of "
                 "stdopaqueshad, stdopaqueshadnight, tarmacnoshadow");
}
// The environment cube of the city map: six DXT4 faces of 128 without mip levels; D3DX's default
// chain has 8 levels a face.
OJ_INTEGRATION_TEST(Effects, EnvironmentCube) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    const auto cube = game.read(std::filesystem::path("textures") / "arcade_env.dds");
    oj::Ddt dds;
    uint32_t levels = 0;
    const bool read = cube && dds.parseDds(*cube) && dds.faces == 6 && dds.format == oj::Ddt::kDxt4 &&
                      dds.width == 128 && dds.mipLevels == 1;
    const std::vector<uint32_t> chain = read ? oj::fullMipChain(dds, levels) : std::vector<uint32_t>{};
    check(read && levels == 8 && chain.size() == 6 * 21845, "arcade_env.dds: a cube, the full mip chain");
}
