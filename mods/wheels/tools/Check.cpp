// Optional integration check; link like tests/Mac/CarModTest.cpp, without a production dependency.
#include "Core/Base/GameFiles.h"
#include "Core/Data/GameData.h"
#include "Game/Cars/CarModel.h"
#include "Game/Frontend/WindowTools.h"
#include "Framework/Fakes/NullRenderDevice.h"
#include <algorithm>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const auto game = std::filesystem::absolute(argv[1]);
    const auto overlay = std::filesystem::absolute(argv[2]);
    std::filesystem::current_path(game);
    oj::GameFiles::setOverrideRoot(overlay);
    oj::CrtRandom random;
    oj::GameData data(game);
    std::string error;
    if (!data.load(random, &error)) { std::cerr << error; return 1; }
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, data.tables, 3, false);
    oj::Car car;
    car.def = data.defs.byId(46);
    car.setModel(car.def->model);
    auto model = oj::scene::CarModel::load(engine, car, data.catalogue, random, true);
    if (!model) return 1;
    auto& list = data.catalogue.lists[oj::kWheel];
    int checked = 0, failures = 0;
    for (const auto& wheel : list.parts) {
        if (wheel.model.find("AddWH_") != 0) continue;
        const auto visible = oj::frontend::modBrandParts(list, wheel.brand, wheel.level);
        if (wheel.brand < 0 || std::find(visible.begin(), visible.end(), &wheel) == visible.end()) ++failures;
        model->fillWheels(wheel.model, oj::scene::CarModel::Folders::Shared);
        for (bool blurred : {false, true}) {
            for (size_t i = 0; i < 4; ++i) {
                model->blurWheel(i, blurred);
                auto* node = model->wheelNode(i);
                if (!node || !node->mesh || node->mesh->parts().empty()) { ++failures; continue; }
                for (const auto& part : node->mesh->parts()) if (!part.material || !part.vertexCount) ++failures;
            }
        }
        car.mods[oj::kWheel] = static_cast<uint8_t>(wheel.index);
        std::vector<uint8_t> saved;
        car.write(saved);
        oj::Car restored;
        size_t offset = 0;
        if (!restored.read(saved, offset) || restored.mods[oj::kWheel] != wheel.index) ++failures;
        ++checked;
    }
    std::cout << "Wheel catalogue " << list.parts.size() << ", imported wheels fitted/blurred/selectable/saved "
              << checked << ", failures " << failures << '\n';
    return failures == 0 && checked == 128 ? 0 : 1;
}
