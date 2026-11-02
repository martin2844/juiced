#include "Framework/Test.h"
#include "Core/Cars/CarPhysics.h"
#include <cmath>
#include <limits>

namespace {
using oj::test::check;
constexpr const char* kBase = "[BeginCar]\nTorqueCurveX 1000 3000 6000 7000\n"
                            "TorqueCurveY .5 1 .8 0\nMaxTorque 153\n";
}

OJ_UNIT_TEST(TuningPreparation, ParsingAndStockIsolation) {
    const auto retail = oj::parseDynamics(std::string(kBase) + "[EndCar]\n");
    const auto prepared = oj::parseDynamics(std::string(kBase) +
        "TurboUpgradeScale 1.10 1.40 1.75 1.85409\n[EndCar]\n");
    check(retail && prepared, "retail and prepared definitions parse");
    if (!retail || !prepared) return;
    check(retail->turboUpgradeScale == std::array<float,4>{1,1,1,1}, "absent curve retains retail scaling");
    for (const char* invalid : {"1 2 3", "1 2 3 4 5", "1 0 2 3", "1 -1 2 3", "1 nan 2 3", "1 inf 2 3"}) {
        const auto bad = oj::parseDynamics(std::string(kBase) + "TurboUpgradeScale " + invalid + "\n[EndCar]\n");
        check(bad && bad->turboUpgradeScale == retail->turboUpgradeScale,
              "malformed preparation curve is ignored as a whole");
    }
    oj::PartCatalogue catalogue;
    auto stock = oj::Car();
    auto oldDynamics = *retail, newDynamics = *prepared;
    oj::fitParts(oldDynamics, stock, catalogue);
    oj::fitParts(newDynamics, stock, catalogue);
    check(oldDynamics.maxTorque == newDynamics.maxTorque && oldDynamics.turbo == newDynamics.turbo &&
          oj::peakPower(oldDynamics) == oj::peakPower(newDynamics), "preparation does not change the factory engine");
}

OJ_UNIT_TEST(TuningPreparation, FittedTurboAndRating) {
    const auto plain = oj::parseDynamics(std::string(kBase) + "[EndCar]\n");
    const auto prepared = oj::parseDynamics(std::string(kBase) +
        "TurboUpgradeScale 1.10 1.40 1.75 1.85409\n[EndCar]\n");
    if (!plain || !prepared) { check(false, "test engine parses"); return; }
    oj::PartCatalogue catalogue;
    for (int level = 0; level < 4; ++level) {
        oj::CarPart part; part.level = level; part.dataValue = 1.25f + level*.05f;
        catalogue.lists[oj::kTurbo].parts.push_back(part);
    }
    float previousPower = oj::peakPower(*prepared);
    for (int level = 0; level < 4; ++level) {
        oj::Car car; car.mods[oj::kTurbo] = static_cast<uint8_t>(level);
        auto fitted = *prepared; oj::fitParts(fitted, car, catalogue);
        const auto partValue = catalogue.lists[oj::kTurbo].parts[level].dataValue;
        check(fitted.turbo == partValue * prepared->turboUpgradeScale[level], "installed turbo level selects its preparation");
        check(oj::peakPower(fitted) > previousPower, "each preparation stage raises actual engine power");
        previousPower = oj::peakPower(fitted);
        const float rating = oj::rateCar(car, &*prepared, catalogue);
        check(std::abs(rating - (oj::peakPower(fitted)*.00048103448f-.5f)) < 1e-3f,
              "workshop rating uses the same fitted engine as driving");
        auto unconfigured = *plain; oj::fitParts(unconfigured, car, catalogue);
        check(unconfigured.turbo == partValue, "other models keep the exact original catalogue boost");
    }
    oj::Car removed;
    auto reset = *prepared; oj::fitParts(reset, removed, catalogue);
    check(reset.turbo == prepared->turbo, "removing the turbo removes the special preparation");
    catalogue.lists[oj::kTurbo].parts[0].level = 99;
    removed.mods[oj::kTurbo] = 0; reset = *prepared; oj::fitParts(reset, removed, catalogue);
    check(reset.turbo == catalogue.lists[oj::kTurbo].parts[0].dataValue, "unknown part levels never index outside the curve");
}
