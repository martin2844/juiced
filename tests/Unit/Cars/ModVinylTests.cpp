#include "Framework/Test.h"
#include "Core/Cars/CarDefs.h"

OJ_UNIT_TEST(CarDefs, AdditiveOptionalVinyls) {
    oj::CarDefs defs;
    oj::CarDef car; car.name = "PEUGEOT_405_GLX";
    for (int i = 0; i < 31; ++i) {
        oj::CarPart p; p.model = "original_" + std::to_string(i); p.index = i;
        car.parts[oj::kDecal].parts.push_back(p);
    }
    defs.cars.push_back(car);
    std::string error;
    oj::test::check(defs.appendVinyl("peugeot_405_glx", "DECAL_NFS_AI_RACHEL", "Rachel", true, &error),
                    "optional registered car receives its vinyl case-insensitively");
    const auto& parts = defs.cars[0].parts[oj::kDecal].parts;
    bool original = parts.size() == 32 && parts.back().index == 31 && parts.back().model == "DECAL_NFS_AI_RACHEL";
    for (int i = 0; i < 31; ++i) original = original && parts[i].model == car.parts[oj::kDecal].parts[i].model && parts[i].index == i;
    oj::test::check(original, "appending a vinyl preserves every existing saved part index");
    oj::test::check(defs.appendVinyl("DISABLED_CAR", "DECAL_NFS_AI_RACHEL", "Rachel", true, &error) && parts.size() == 32,
                    "disabled optional car pack does not prevent startup or add a phantom car");
    oj::test::check(!defs.appendVinyl("DISABLED_CAR", "DECAL_NFS_AI_RACHEL", "Rachel", false, &error),
                    "required unknown car still fails");
    oj::test::check(!defs.appendVinyl("DISABLED_CAR", "../DECAL_NFS_BAD", "Bad", true, &error) &&
                    !defs.appendVinyl("../DISABLED_CAR", "DECAL_NFS_AI_RACHEL", "Bad", true, &error),
                    "optional entries retain name/path validation even when the car is absent");
    oj::test::check(!defs.appendVinyl(car.name, "DECAL_NFS_AI_RACHEL", "Duplicate", true, &error),
                    "optional entries cannot duplicate a saved catalogue entry");
    auto& growing = defs.cars[0].parts[oj::kDecal].parts;
    growing.resize(124);
    oj::test::check(defs.appendVinyl(car.name, "DECAL_NFS_LAST", "Last", true, &error) && growing.back().index == 124 &&
                    !defs.appendVinyl(car.name, "DECAL_NFS_OVERFLOW", "Overflow", true, &error),
                    "optional catalogue obeys the same byte-index capacity limit");
}
