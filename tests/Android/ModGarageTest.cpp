#include "Core/Data/GameData.h"
#include "Mods/Packages.h"
#include "Mods/Runtime.h"
#include "Mods/Creative.h"
#include "Game/Cars/CarModel.h"
#include "Framework/Fakes/NullRenderDevice.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    std::filesystem::path overlay;
    try {
        const auto game = std::filesystem::absolute(argv[1]);
        const auto packages = std::filesystem::absolute(argv[2]);
        oj::mods::packages::discover(packages);
        overlay = oj::mods::packages::assemble(game);
        oj::GameFiles::setOverrideRoot(overlay);
        oj::mods::runtime::load(packages);
        std::cout << "Creative mods " << oj::mods::enabled() << '\n';
        std::filesystem::current_path(game);
        oj::CrtRandom random; oj::GameData data(game); std::string error;
        check(data.load(random, &error), error.c_str());
        const auto* def = data.defs.byModel("FERRARI_458_STOCK");
        check(def && def->matchStockWheels, "Ferrari enables stock wheel envelope");
        check(def->parts[8].parts.size() == 34, "Three existing decals plus 31 retail styles");
        check(def->parts[8].parts[2].model == "DECAL_F458_CUSTOMLOGO", "Custom image save index preserved");
        oj::test::NullRenderDevice device;
        oj::scene::Engine engine(device, data.tables, 3, false);
        for (bool garage : {true, false}) {
            auto car = oj::makeCar(def);
            check(oj::rateCar(car, data.dynamics->of(*def), data.catalogue) > 500.f, "Ferrari has expected stock BHP");
            auto model = oj::scene::CarModel::load(engine, car, data.catalogue, random, garage);
            check(bool(model), "Ferrari model loads");
            auto* stock = model->wheelNode(0)->mesh;
            const float width = stock->boundsMax()[0] - stock->boundsMin()[0];
            const float diameter = std::max(stock->boundsMax()[1] - stock->boundsMin()[1], stock->boundsMax()[2] - stock->boundsMin()[2]);
            size_t checked = 0;
            for (const auto& wheel : data.catalogue.lists[oj::kWheel].parts) {
                model->fillWheels(wheel.model, oj::scene::CarModel::Folders::Shared);
                for (bool blur : {false, true}) {
                    model->blurWheel(0, blur);
                    const auto* mesh = model->wheelNode(0)->mesh;
                    check(mesh && !mesh->parts().empty(), "Wheel and blur load");
                    check(std::abs(mesh->boundsMax()[0] - mesh->boundsMin()[0] - width) < .02f, "Wheel width preserved");
                    const float size = std::max(mesh->boundsMax()[1] - mesh->boundsMin()[1], mesh->boundsMax()[2] - mesh->boundsMin()[2]);
                    check(std::abs(size - diameter) < .02f, "Tyre diameter preserved");
                    for (const auto& part : mesh->parts()) check(part.material != nullptr, "Wheel material present");
                }
                ++checked;
            }
            car.body[2] = 1;
            for (size_t index = 0; index < def->parts[8].parts.size(); ++index) {
                car.body[8] = uint8_t(index); model->refitParts(car, data.catalogue); model->colourDecal(car);
                const auto* decal = model->slotMesh(oj::scene::CarModel::kDecal);
                check(decal && !decal->parts().empty(), "Decal fitted");
                for (const auto& part : decal->parts())
                    check(part.material && part.material->texture(0), "Retail decal material and texture present");
            }
            const auto* bonnet = model->slotMesh(oj::scene::CarModel::kBonnet);
            check(bonnet && !bonnet->parts().empty(), "Carbon bonnet fitted");
            bool texturedCarbon = false;
            for (const auto& part : bonnet->parts()) {
                check(part.material != nullptr, "Carbon and painted border materials loaded");
                texturedCarbon |= part.material->classId() == oj::kColouredSpecOpaque && part.material->texture(0) != nullptr;
            }
            check(texturedCarbon, "Carbon weave loaded");
            std::cout << "PASS " << (garage ? "garage" : "race") << ": " << checked << " wheel/blur sizes, 34 decals, carbon, stock power\n";
        }
        oj::GameFiles::setOverrideRoot({}); std::filesystem::remove_all(overlay);
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
