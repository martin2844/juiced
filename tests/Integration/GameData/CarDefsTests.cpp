// Integration tests: CarDefs.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Random.h"
#include "Core/Cars/Car.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Cars/CarDefs.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

OJ_INTEGRATION_TEST(CarDefs, Cars) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::CarDefs& defs = shared.defs;
    bool ids = defs.cars.size() == 53;
    std::vector<bool> seen(53, false);
    for (const oj::CarDef& c : defs.cars) {
        if (c.id < 0 || c.id >= 53 || seen[static_cast<size_t>(c.id)]) ids = false;
        else seen[static_cast<size_t>(c.id)] = true;
    }
    const oj::CarDef* focus = defs.byModel("focus_2_STOCK");
    check(ids && focus != nullptr && focus->id == 52 && focus->value == 19995 && focus->state == 0 &&
              focus->parts[oj::kFrontBumper].parts.size() == 4 &&
              focus->parts[oj::kFrontBumper].standard && !focus->parts[oj::kDecal].standard &&
              focus->parts[oj::kDecal].parts.size() == 31 && focus->parts[oj::kNeon].parts.size() == 1 &&
              focus->parts[oj::kNeon].parts[0].model == "nono" && !focus->parts[oj::kNeon].standard &&
              !defs.paints.empty(),
          "cardefs.txt: 53 cars with their own ids, focus_2 parts (standard lists as the running stock's), "
          "paints");

    std::printf("     cardefs: %zu cars, %zu paints, %zu neon, %zu dial, %zu decal, %zu wheel colours\n",
                defs.cars.size(), defs.paints.size(), defs.neons.size(), defs.dials.size(),
                defs.decals.size(), defs.wheels.size());
}
OJ_INTEGRATION_TEST(CarDefs, PartsCatalogue) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::PartCatalogue& catalogue = shared.catalogue;
    const auto& exhausts = catalogue.lists[oj::kExhaust].parts;
    check(exhausts.size() == 18 && catalogue.lists[oj::kWheel].parts.size() == 74 &&
              catalogue.lists[oj::kGearbox].parts.size() == 3 && exhausts[0].model == "EXHAUST_BORLA_VS5" &&
              exhausts[0].cost == 90 && exhausts[0].level == 0 && exhausts[0].physics[0] == 63 &&
              exhausts[0].brand == 8,
          "parts catalogue: the ten files, kinds, costs, levels, PHYSICS x 255");
}
OJ_INTEGRATION_TEST(CarDefs, PlayersCrewAndAutoModify) {
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) return;
    const oj::CarDefs& defs = shared.defs;
    const oj::GameFiles& game = oj::test::gameFiles();
    if (!game.valid()) return;
    const oj::PartCatalogue& catalogue = shared.catalogue;
    if (const auto players = game.read(std::filesystem::path("scripts") / "CMSPlayersCrewCollection.txt")) {
        oj::CrtRandom carRandom;
        int counter = 1;
        oj::CarMaking making{defs, catalogue, carRandom, counter};
        std::vector<oj::Car> crew = oj::loadCollection(
            std::string_view(reinterpret_cast<const char*>(players->data()), players->size()), making);
        check(crew.size() == 2 && crew[0].modelName() == "ECLIPSE_STOCK" && crew[0].base == 0xFFBF960D &&
                  crew[0].pearl == 0xFFBF6E01 && crew[1].modelName() == "GTR_STOCK" &&
                  crew[1].metallic == 0xFFDCDCDC && crew[0].body[0] == 0xFF,
              "CMSPlayersCrewCollection.txt: the player's two cars and their paint");
        oj::autoModify(crew[0], false, -1, making);
        bool fitted = crew[0].body[oj::kNeon] == 1 && crew[0].mods[oj::kWheel] != 0xFF && counter == 2;
        for (size_t k = 0; k < oj::kModKinds; ++k) {
            fitted = fitted && crew[0].mods[k] < catalogue.lists[k].parts.size();
        }
        check(fitted, "automatic modification: parts within their lists, neon 1, the counter moves on");
    }
}
