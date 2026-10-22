#ifdef OJ_MACOS
#include "Core/Cars/CarCollection.h"
#include "Core/Race/RaceSession.h"
#include "Framework/Test.h"
#include "Mods/Creative.h"
#include "Mods/Runtime.h"

OJ_UNIT_TEST(CreativeDecals, PreserveCrewIndices) {
    using oj::test::check;
    oj::CarDefs defs;
    defs.cars.resize(2);
    auto &stock = defs.cars[0];
    stock.model = "TEST_STOCK_DECALS";
    stock.parts[oj::kDecal].parts.resize(31);
    auto &addon = defs.cars[1];
    addon.model = "TEST_ADDON_DECALS";
    addon.parts[oj::kDecal].parts.resize(3);
    addon.parts[oj::kDecal].parts.back().model = "TEST_DECAL_CUSTOMLOGO";
    oj::mods::runtime::load(JUICED_TEST_MODS);
    oj::mods::configure(std::filesystem::temp_directory_path() / "juiced-creative-decal-unit");
    oj::mods::addLogoSlots(defs);
    check(stock.parts[oj::kDecal].parts.size() == 32 && oj::mods::decalCount(stock, 32) == 31,
          "appended custom logo retains original crew livery extent");
    oj::PartCatalogue catalogue;
    auto career = std::make_unique<oj::Career>();
    career->opponentCars.push_back(oj::makeCar(&stock));
    career->opponentCars.push_back(oj::makeCar(&addon));
    oj::Crew crew;
    crew.id = 4; // The final retail crew decal.
    oj::applyCrewScheme(*career, crew, defs, catalogue);
    check(career->opponentCars[0].body[8] == 30,
          "crew scheme keeps the final stock decal, not the appended custom logo");
    check(career->opponentCars[1].body[8] == 0x7F, "addon without eight crew decals leaves the crew livery empty");
    oj::CrtRandom random;
    int counter = 1;
    oj::CarMaking making{defs, catalogue, random, counter};
    bool personalLogo = false;
    for (int i = 0; i < 128; ++i) {
        auto car = oj::makeCar(&addon);
        oj::autoModify(car, false, 0, making);
        personalLogo |= car.body[8] == 2;
    }
    check(!personalLogo, "automatic upgrades never select an addon's personal-image slot");
    oj::mods::runtime::load(JUICED_TEST_MODS);
    oj::mods::configure({});
}
#endif
