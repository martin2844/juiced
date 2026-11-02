#include "Framework/Test.h"
#include "Core/Physics/CarBody.h"

OJ_UNIT_TEST(Transmission, AddedSeventhGear) {
    oj::CarDynamics d;
    d.body.mass = 1500;
    d.forwardRatios = {3.077f, 2.185f, 1.626f, 1.286f, 1.028f, .839f, .693f};
    oj::CarBody car(d, {});
    car.setGear(7);
    car.shift(8);
    oj::test::check(car.gear == 8 && car.ratio == .693f, "seven-speed car reaches its seventh forward ratio");
    car.engineOn = true;
    car.clutch = 1;
    car.clutchIn = true;
    car.shift(9);
    oj::test::check(car.gear == 8 && car.clutch == 1 && car.clutchIn, "shifting past top gear does not disengage clutch");
    d.forwardRatios.pop_back();
    oj::CarBody retail(d, {});
    retail.setGear(8);
    oj::test::check(retail.gear == 7 && retail.ratio == .839f, "six-speed car remains limited to its actual sixth gear");
}
