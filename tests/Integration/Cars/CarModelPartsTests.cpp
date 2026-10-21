// Integration tests: a car model's parts (Game/Cars/CarModel.cpp) with the game's cars.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Base/Random.h"
#include "Game/Cars/CarModel.h"
#include "Game/Frontend/CustomRace/CustomRaceInternal.h"
#include "Game/Scene/Scene3d.h"

#include <memory>

namespace {

using oj::test::check;

}  // namespace

// The custom race's car with the automatic modification's decal, then its parts cleared (0x004600C0: the decal -1) and
// the model's parts put again (0x0045A8A0): the decal's slot is emptied (0x0048BC30), as the model makes none of a
// decal not chosen.
OJ_INTEGRATION_TEST(CarModelParts, DecalTakenOff) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead) {
        check(false, "cardefs.txt read");
        return;
    }
    const oj::CarDef* def = nullptr;
    for (const oj::CarDef& d : shared.defs.cars) {
        if (!d.parts[8].parts.empty()) {
            def = &d;
            break;
        }
    }
    if (def == nullptr) {
        check(false, "a car with decals");
        return;
    }
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, shared.tables, 3, false);
    oj::CrtRandom random;
    oj::Car car;
    car.def = def;
    oj::frontend::customrace::clearParts(car);
    car.body[8] = 0;
    const std::unique_ptr<oj::scene::CarModel> model =
        oj::scene::CarModel::load(engine, car, shared.catalogue, random, true);
    if (!model) {
        check(false, "the car's model loaded");
        return;
    }
    const bool decalOn = model->slotMesh(oj::scene::CarModel::kDecal) != nullptr;
    oj::frontend::customrace::clearParts(car);
    model->refitParts(car, shared.catalogue);
    const bool decalOff = model->slotMesh(oj::scene::CarModel::kDecal) == nullptr;
    check(decalOn && decalOff && car.body[8] == 0xFF,
          "the decal fitted, then gone with the parts cleared (the record's -1 kept)");
}
