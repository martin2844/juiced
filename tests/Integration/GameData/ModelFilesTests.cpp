// Integration tests: ModelFiles.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Formats/Model.h"

#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

using oj::test::check;
using oj::test::bytesOf;

}  // namespace

// Every model file of the game folder: .boh, .d8m (release layout), .pmd.
OJ_INTEGRATION_TEST(ModelFiles, EveryModel) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    size_t boh = 0, bohBad = 0, d8m = 0, d8mBad = 0, pmd = 0, pmdEmpty = 0;
    std::error_code ec;
    const std::filesystem::path gameRoot = game.root();
    std::set<std::filesystem::path> files;
    // Optional test-fixture inventory, independently resolved against DAT hashes.
    // The production game does not consume this file. Keep loose discovery too.
    std::ifstream inventory(gameRoot / ".oj-model-inventory.txt");
    std::string line;
    while (std::getline(inventory, line)) {
        const std::filesystem::path path(line);
        if (path.empty() || path.is_absolute() ||
            std::any_of(path.begin(), path.end(), [](const auto& part) { return part == ".."; })) {
            check(false, "model inventory: relative paths inside game root only");
            continue;
        }
        files.insert(path);
    }
    for (auto it = std::filesystem::recursive_directory_iterator(gameRoot, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (ec || !it->is_regular_file()) continue;
        files.insert(it->path().lexically_relative(gameRoot));
    }
    for (const auto& file : files) {
        std::wstring ext = file.extension().wstring();
        for (wchar_t& c : ext) c = static_cast<wchar_t>(towlower(c));
        const std::wstring folder = file.parent_path().filename().wstring();
        if (ext != L".boh" && ext != L".d8m" && ext != L".pmd") continue;
        const auto content = game.read(file);
        if (!content) {
            check(false, "model inventory: listed file readable");
            std::printf("     missing model: %s\n", file.string().c_str());
            continue;
        }
        const std::vector<uint8_t> data = bytesOf(*content);
        if (ext == L".boh") {
            ++boh;
            if (!oj::parseBoh(data)) {
                ++bohBad;
                std::printf("     BOH parse failed: %s\n", file.string().c_str());
            }
        } else if (ext == L".d8m" && _wcsicmp(folder.c_str(), L"mesh") == 0 &&
                   file.parent_path().parent_path().filename() != L"tracks") {
            ++d8m;
            if (!oj::parseMesh(data)) {
                ++d8mBad;
                std::printf("     D8M parse failed: %s (%zu bytes)\n", file.string().c_str(), data.size());
            }
        } else if (ext == L".pmd") {
            ++pmd;
            const oj::Material m = oj::parseMaterial(
                std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
            if (m.className.empty()) ++pmdEmpty;
        }
    }
    std::printf("     models: %zu .boh (%zu not read), %zu .d8m of meshes (%zu not read), %zu .pmd (%zu "
                "without a class)\n",
                boh, bohBad, d8m, d8mBad, pmd, pmdEmpty);
    // Unknown/legacy meshes are diagnostics to investigate, not a numeric error
    // allowance that could silently accept a regression in a different mesh.
    check(boh > 400 && bohBad == 0 && d8m > 7000 && d8mBad == 0 && pmd > 10000 && pmdEmpty == 0,
          "model files: node trees, meshes and materials read");
}

// Follow the actual CarModel::load BOH paths and first-readable mesh search
// order for both race and interface variants. A parse failure must not fall
// through to another directory and accidentally conceal a broken override.
OJ_INTEGRATION_TEST(ModelFiles, StockCarTrees) {
    const auto& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const auto& game = oj::test::gameFiles();
    size_t trees = 0, references = 0, bad = 0, global = 0, cars = 0;
    for (const auto& def : shared.defs.cars) {
        // Dealer generation starts at model 1; table entry 0 / "NONE" is
        // the empty-car sentinel, with no model or standard components.
        if (def.id == 0 && _stricmp(def.name.c_str(), "none") == 0) continue;
        ++cars;
        for (bool interface : {false, true}) {
            const auto folder = std::filesystem::path("cars") / (def.name + (interface ? "_ui" : ""));
            const auto boh = folder / "models" / (def.name + "_stock.boh");
            const auto bytes = game.read(boh);
            const auto tree = bytes ? oj::parseBoh(bytesOf(*bytes)) : std::nullopt;
            if (!tree || tree->empty()) {
                ++bad;
                std::printf("     stock tree missing/invalid: %s\n", boh.string().c_str());
                continue;
            }
            ++trees;
            std::function<void(const oj::BohNode&)> visit = [&](const oj::BohNode& node) {
                if (node.attach == 0 && _stricmp(node.mesh.c_str(), "NONE") != 0) {
                    ++references;
                    bool found = false;
                    for (const auto& base : {folder / "mesh", std::filesystem::path("carmesh"), std::filesystem::path("mesh")}) {
                        const auto path = base / (node.mesh + ".d8m");
                        const auto content = game.read(path);
                        if (!content) continue;
                        found = true;
                        if (base == std::filesystem::path("mesh")) ++global;
                        if (!oj::parseMesh(bytesOf(*content))) {
                            ++bad;
                            std::printf("     stock tree mesh invalid: %s -> %s\n", boh.string().c_str(), path.string().c_str());
                        }
                        break;
                    }
                    if (!found) {
                        ++bad;
                        std::printf("     stock tree mesh missing: %s -> %s\n", boh.string().c_str(), node.mesh.c_str());
                    }
                }
                for (const auto& child : node.children) visit(child);
            };
            for (const auto& root : *tree) visit(root);
        }
    }
    std::printf("     stock trees: %zu trees, %zu mesh references, %zu global selections, %zu failures\n", trees, references, global, bad);
    check(cars > 0 && trees == cars*2 && references > 0 && bad == 0,
          "stock car trees: race and interface references resolve and parse in loader order");
}

// Caller-derived resources: GarageView, RollingRoad, CutScene, Respect,
// CityMap and RaceHud, plus the retail UI SetModel Angel_City properties.
// This complements EveryModel; it does not exempt its rejected legacy meshes.
OJ_INTEGRATION_TEST(ModelFiles, FrontendAndRouteModels) {
    const auto& game = oj::test::gameFiles();
    if (!game.valid()) return;
    std::set<std::string> names = {
        "angel_city", "don_garage1", "scrapyard", "showroom2", "showroom3",
        "outsidegarage", "rollingarage", "rolling_road", "gen_shadow", "ui_map_car",
        "ui_westanderson", "ui_sanricardo", "ui_campbellhills", "ui_downtown",
        "ui_angelnorth", "ui_southside", "ui_eastangel", "ui_engstrom"};
    std::set<std::string> optionalCruiseModels;
    for (const auto& route : oj::test::sharedGameData().tables.routes) {
        if (route.name.empty()) continue;
        names.insert("UI_" + route.name + "_GUI");
        names.insert("UI_" + route.name + "_MAP");
        // Retail cruise/showoff routes have no GUI/MAP models. CityMap tolerates
        // load failure; RaceHud::prepareMap hides MAP if loading fails. If a file
        // is supplied, still require it and its meshes to parse successfully.
        if (route.type == oj::RouteType::Cruise) {
            optionalCruiseModels.insert("UI_" + route.name + "_GUI");
            optionalCruiseModels.insert("UI_" + route.name + "_MAP");
        }
    }
    size_t models = 0, bad = 0, optionalAbsent = 0;
    std::set<std::string> meshes;
    for (const auto& name : names) {
        const auto bytes = game.read(std::filesystem::path("models") / (name + ".boh"));
        if (!bytes && optionalCruiseModels.contains(name)) {
            ++optionalAbsent;
            continue;
        }
        const auto tree = bytes ? oj::parseBoh(bytesOf(*bytes)) : std::nullopt;
        if (!tree || tree->empty()) {
            ++bad;
            std::printf("     frontend model missing/invalid: %s\n", name.c_str());
            continue;
        }
        ++models;
        std::function<void(const oj::BohNode&)> visit = [&](const auto& node) {
            if (node.attach == 0 && _stricmp(node.mesh.c_str(), "NONE") != 0) {
                meshes.insert(node.mesh);
            }
            for (const auto& child : node.children) visit(child);
        };
        for (const auto& node : *tree) visit(node);
    }
    for (const auto& name : meshes) {
        const auto bytes = game.read(std::filesystem::path("mesh") / (name + ".d8m"));
        if (!bytes || !oj::parseMesh(bytesOf(*bytes))) {
            ++bad;
            std::printf("     frontend mesh missing/invalid: %s\n", name.c_str());
        }
    }
    std::printf("     frontend/routes: %zu models, %zu mesh names, %zu failures, %zu absent optional cruise models\n",
                models, meshes.size(), bad, optionalAbsent);
    check(models >= 18 && !meshes.empty() && bad == 0,
          "frontend/routes: caller-derived BOHs and their global meshes parse");
}

// Selectable custom geometry, not the optional absent factory parts audited
// separately. Follow CarModel's first-readable-file rule without parse fallback.
OJ_INTEGRATION_TEST(ModelFiles, CustomCarParts) {
    const auto& shared = oj::test::sharedGameData();
    const auto& game = oj::test::gameFiles();
    if (!game.valid() || !shared.carDefsRead) return;
    size_t requests = 0, missing = 0, invalid = 0, global = 0, noGeometry = 0;
    auto inspect = [&](const std::string& name, const std::vector<std::filesystem::path>& folders) {
        ++requests;
        for (const auto& folder : folders) {
            const auto path = folder / (name + ".d8m");
            const auto data = game.read(path);
            if (!data) continue;
            if (folder == "mesh") ++global;
            if (!oj::parseMesh(bytesOf(*data))) {
                ++invalid;
                std::printf("     custom part invalid: %s\n", path.string().c_str());
            }
            return;
        }
        ++missing;
        std::printf("     custom part missing: %s in %s\n", name.c_str(), folders.front().string().c_str());
    };
    for (const auto& car : shared.defs.cars) {
        for (bool ui : {false, true}) {
            const std::vector<std::filesystem::path> folders = {
                std::filesystem::path("cars") / (car.name + (ui ? "_ui" : "")) / "mesh",
                "carmesh", "mesh"};
            for (int category : {0, 1, 2, 3, 4, 7, 8}) {
                const auto& list = car.parts[category];
                for (size_t i = list.standard ? 1 : 0; i < list.parts.size(); ++i) {
                    const auto& name = list.parts[i].model;
                    // A literal NONE entry means no geometry (retail Corolla
                    // and Golf skirts); it is not a missing custom asset.
                    if (_stricmp(name.c_str(), "NONE") == 0) {
                        ++noGeometry;
                        continue;
                    }
                    if (category == 4 || category == 7) {
                        inspect(name + "_L", folders);
                        inspect(name + "_R", folders);
                    } else {
                        inspect(name, folders);
                    }
                }
            }
        }
    }
    for (int kind : {oj::kWheel, oj::kExhaust}) {
        for (const auto& part : shared.catalogue.lists[kind].parts) inspect(part.model, {"carmesh"});
    }
    std::printf("     custom parts: %zu requests, %zu missing, %zu invalid, %zu global selections\n",
                requests, missing, invalid, global);
    std::printf("     custom parts: %zu explicit NONE choices\n", noGeometry);
    check(requests > 0 && missing == 0 && invalid == 0,
          "custom parts: race/UI body choices and shared wheel/exhaust meshes parse");
}
