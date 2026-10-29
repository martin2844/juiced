#include "Android/SaveEditor.h"
#include "Mods/Packages.h"
#include "Mods/Creative.h"
#include "Mods/Runtime.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace oj;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::vector<std::byte> bytes(const std::vector<uint8_t>& data) {
    std::vector<std::byte> out(data.size()); std::memcpy(out.data(), data.data(), data.size()); return out;
}
int main(int argc, char** argv) {
    if (argc < 2) return 2;
    std::filesystem::path overlay;
    std::cout << "Creative mods enabled: " << mods::enabled() << "\n";
    try {
        if (argc > 2) {
            mods::packages::discover(argv[2]); overlay = mods::packages::assemble(argv[1]);
            GameFiles::setOverrideRoot(overlay); mods::runtime::load(argv[2]);
        }
        CrtRandom random; GameData game(argv[1]); std::string error;
        check(game.load(random, &error), error.c_str());
        save::Archive original;
        original.entries["CMSGlobal.sav"] = game.career->write();
        original.entries["unknown-mod-data"] = {11, 22, 33};
        const auto input = bytes(original.write());
        auto originalGarage = game.career->garage.size();
        saveedit::Changes changes; changes.money = 123456; changes.respect.fill(1200);
        changes.addCars.push_back(game.defs.cars.at(1).model);
        auto result = saveedit::edit(game, input, changes);
        auto archive = saveedit::load(game, bytes(result));
        check(game.career->profile.money == 123456, "Money roundtrip");
        check(game.career->crews[1].respect == 1200 && game.career->crews[8].respect == 1200, "Respect roundtrip");
        check(game.career->garage.size() == originalGarage + 1, "Car addition");
        check(game.career->garage.back().modelName() == changes.addCars.front(), "Car model");
        check(game.career->garage.back().grid.has_value(), "New car damage grid initialized");
        check(Car::asFloat(game.career->garage.back().rating) > 0, "New car power initialized");
        for (const auto& def : game.defs.cars) {
            if (def.id == 0) continue;
            auto one = changes; one.addCars = {def.model};
            saveedit::edit(game, input, one);
            check(Car::asFloat(game.career->garage.back().rating) > 0, "Every catalogue car has positive power");
        }
        check(archive.entries["unknown-mod-data"] == original.entries["unknown-mod-data"], "Unknown entries preserved");
        check(game.career->garage.size() + game.career->padding.size() == 55, "Garage padding");
        auto rejected = [&](const auto& action) { try { action(); } catch (const std::exception&) { return; } throw std::runtime_error("Invalid edit was accepted"); };
        if (game.tables.carModels.size() == 64) {
            for (uint32_t storedCount : {0u, 4u}) {
                game.newCareer(random, "Migration");
                auto oldCareer = *game.career;
                const auto newCarDefaults = oldCareer.profile.modProgress.back();
                oldCareer.profile.modCount = storedCount;
                if (storedCount) oldCareer.profile.performance(56)[3] = 2;
                save::Archive oldArchive;
                oldArchive.entries["CMSGlobal.sav"] = oldCareer.write();
                oldArchive.entries["unknown-mod-data"] = {44, 55};
                const auto oldBytes = bytes(oldArchive.write());
                saveedit::load(game, oldBytes);
                check(game.career->profile.modCount == 11, "Older roster expands to eleven added cars");
                const auto& newCarProgress = game.career->profile.modProgress.back();
                check(newCarProgress.performance == newCarDefaults.performance &&
                          newCarProgress.body == newCarDefaults.body && newCarProgress.paint == newCarDefaults.paint &&
                          newCarProgress.step == newCarDefaults.step,
                      "Newly installed cars retain initialized unlock defaults");
                if (storedCount) check(game.career->profile.performance(56)[3] == 2, "Existing added-car upgrades preserved");
                auto migratedChanges = changes;
                migratedChanges.addCars.clear();
                const auto migratedBytes = saveedit::edit(game, oldBytes, migratedChanges);
                const auto migrated = saveedit::load(game, bytes(migratedBytes));
                check(game.career->profile.money == changes.money, "Older-roster save remains editable");
                check(migrated.entries.at("unknown-mod-data") == oldArchive.entries.at("unknown-mod-data"),
                      "Migration preserves unrelated archive entries");
                check(oldBytes == bytes(oldArchive.write()), "Migration leaves source archive untouched");
                auto corruptCareer = oldArchive;
                corruptCareer.entries["CMSGlobal.sav"].pop_back();
                rejected([&] { saveedit::load(game, bytes(corruptCareer.write())); });
                corruptCareer = oldArchive;
                corruptCareer.entries["CMSGlobal.sav"].push_back(0xFF);
                rejected([&] { saveedit::load(game, bytes(corruptCareer.write())); });
            }
            std::cout << "PASS: stock and four-car saves migrate to eleven added cars; malformed careers rejected\n";
        }
        auto invalid = changes; invalid.money = -1; rejected([&] { saveedit::edit(game, input, invalid); });
        invalid = changes; invalid.respect[0] = 1501; rejected([&] { saveedit::edit(game, input, invalid); });
        invalid = changes; invalid.addCars = {"DOES_NOT_EXIST"}; rejected([&] { saveedit::edit(game, input, invalid); });
        invalid = changes; invalid.addCars.resize(56, changes.addCars.front()); rejected([&] { saveedit::edit(game, input, invalid); });
        auto broken = input; broken[12] ^= std::byte{1}; rejected([&] { saveedit::load(game, broken); });
        check(bytes(original.write()) == input, "Original save remains unchanged");
        std::cout << "PASS: money, eight crews, stock car, grid, padding, unknown entries, invalid edits, corrupt archive; roster " << game.defs.cars.size() << '\n';
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    GameFiles::setOverrideRoot({});
    if (!overlay.empty()) std::filesystem::remove_all(overlay);
}
