// Unit tests: instant_selcar's "AUTO-MOD CAR?" (Game/Frontend/CustomRace/CustomRaceInternal.h, unmodified).

#include "Framework/Test.h"

#include "Core/Base/Random.h"
#include "Core/Cars/Car.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Cars/CarDefs.h"
#include "Game/Frontend/CustomRace/CustomRaceInternal.h"

namespace {

using oj::test::check;

}  // namespace

// A mod's car whose model has no body parts (no bumpers, bonnets, spoilers or skirts) keeps its first four body parts
// 0 through the automatic modification: the stock asked "AUTO-MOD CAR?" again at each SELECT. The fixed build takes the
// neon the modification always fits: asked before, not after; the parts cleared ask again.
OJ_UNIT_TEST(AutoModQuestion, AskedUntilModified) {
    oj::CarDefs defs;
    oj::CarDef plainModel;  // no lists of body or catalogue parts
    oj::PartCatalogue catalogue;
    oj::CrtRandom random;
    int counter = 1;
    oj::CarMaking making{defs, catalogue, random, counter};
    oj::Car car;
    car.def = &plainModel;
    oj::frontend::customrace::clearParts(car);
    const bool asked = oj::frontend::customrace::unmodified(car);
    oj::autoModify(car, false, 2, making);
    const bool stillZero = car.body[0] == 0 && car.body[1] == 0 && car.body[2] == 0 && car.body[3] == 0;
    const bool askedAgain = oj::frontend::customrace::unmodified(car);
    oj::frontend::customrace::clearParts(car);
    const bool askedCleared = oj::frontend::customrace::unmodified(car);
    check(asked && stillZero, "auto-mod: a car of no body parts asked, its four parts still 0 after the modification");
    check(!askedAgain, "auto-mod (fixed): the modification's neon ends the question");
    check(askedCleared, "auto-mod: the parts cleared ask again");
}
