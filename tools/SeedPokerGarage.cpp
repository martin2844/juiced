#include "Android/SaveEditor.h"
#include "Core/Career/CareerInit.h"
#include "Mods/Packages.h"
#include "Mods/Creative.h"
#include "Mods/Runtime.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

// Inspect by default. Writes only a NEW staging file, never the source save.
int main(int argc, char **argv) {
    using namespace oj;
    if (argc < 4) return 2;
    std::filesystem::path overlay;
    try {
        const auto root = std::filesystem::absolute(argv[1]);
        const auto source = std::filesystem::absolute(argv[3]);
        std::ifstream input(source, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot read source save");
        std::vector<char> raw((std::istreambuf_iterator<char>(input)), {});
        std::vector<std::byte> bytes(raw.size());
        std::memcpy(bytes.data(), raw.data(), raw.size());
        mods::packages::discover(argv[2]);
        overlay = mods::packages::assemble(root);
        GameFiles::setOverrideRoot(overlay);
        mods::runtime::load(argv[2]);
        std::cout << "Creative mods enabled: " << mods::enabled() << '\n';
        std::filesystem::current_path(root);
        CrtRandom random;
        GameData game(root);
        std::string error;
        if (!game.load(random, &error)) throw std::runtime_error(error);
        save::Archive inspection;
        if (!inspection.read(bytes, &error) || !game.career->read(inspection.entries.at("CMSGlobal.sav"), &error))
            throw std::runtime_error(error);
        for (const auto& car : game.career->garage) {
            const auto* def = game.defs.byModel(car.modelName());
            std::cout << "GARAGE " << car.modelName() << " matches " << (def ? def->model : "MISSING") << '\n';
        }
        std::cout << "BEFORE " << saveedit::describe(game, bytes) << '\n';
        auto archive = saveedit::load(game, bytes);
        if (argc == 4) {
            for (const auto &def : game.defs.cars)
                if (def.id) std::cout << def.model << " | " << def.displayName << '\n';
        } else {
            const auto destination = std::filesystem::absolute(argv[4]);
            if (std::filesystem::exists(destination)) throw std::runtime_error("Staging file already exists");
            auto &career = *game.career;
            if (career.garage.size() + argc - 5 > 55) throw std::runtime_error("Garage full");
            const uint32_t colours[] = {0xFF2876B5, 0xFFE8A12B, 0xFFB82B35, 0xFFE4E6EA, 0xFF6C3E95, 0xFF248463, 0xFF252833, 0xFFD3D5D8};
            for (int i = 5; i < argc; ++i) {
                const auto *def = game.defs.byModel(argv[i]);
                if (!def || !def->id) throw std::runtime_error("Unknown car model");
                auto car = makeCar(def);
                rateCar(car, game.dynamics->of(*def), game.catalogue);
                if (!(Car::asFloat(car.rating) > 0)) throw std::runtime_error("Car has invalid power");
                car.grid = GridDamage{};
                car.base = colours[(i - 5) % 8];
                career.garage.push_back(std::move(car));
            }
            career.padding = garagePadding(career, game.defs);
            archive.entries["CMSGlobal.sav"] = career.write();
            const auto staged = archive.write();
            std::vector<std::byte> check(staged.size());
            std::memcpy(check.data(), staged.data(), staged.size());
            const auto verified = saveedit::load(game, check);
            if (verified.entries != archive.entries) throw std::runtime_error("Archive roundtrip failed");
            std::cout << "AFTER " << saveedit::describe(game, check) << '\n';
            std::ofstream output(destination, std::ios::binary);
            output.write(reinterpret_cast<const char *>(staged.data()), staged.size());
            output.close();
            if (!output) throw std::runtime_error("Cannot write staging file");
        }
        GameFiles::setOverrideRoot({});
        if (!overlay.empty()) std::filesystem::remove_all(overlay);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        GameFiles::setOverrideRoot({});
        if (!overlay.empty()) std::filesystem::remove_all(overlay);
        return 1;
    }
}
