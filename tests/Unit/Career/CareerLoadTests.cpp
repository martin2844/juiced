// Unit tests: careerload and garageload (Game/Frontend/Career/CareerLoad.cpp) through a front end of the tests.

#include "Framework/Test.h"
#include "Framework/Fakes/FakeCareerHost.h"

#include "Game/Frontend/Career/CareerLoad.h"

namespace {

using oj::test::check;

}  // namespace

// The enter of careerload's and garageload's record (0x004358A0) clears the front end's +0x38: the players'
// controllers are not watched while the career's next window comes (its leave, 0x00435930, watches them again).
OJ_UNIT_TEST(CareerLoad, ControllersNotWatched) {
    oj::test::FakeCareerHost host;
    oj::ui::Screen s(host.nullDevice, host.lang);
    oj::frontend::CareerLoad load;
    load.enter(host, s);
    check(!host.watching, "careerload's enter: the controllers no longer watched");
}
