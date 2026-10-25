#pragma once
// A garage's front end for the tests (the garage windows' GarageHost): its shared garage and the two stats records.

#include "Framework/Fakes/FakeWindowHost.h"

#include "Game/Frontend/Garage/CarStats.h"
#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/Garage/GarageHost.h"

namespace oj::test {

class FakeGarageHost : public FakeWindowHost, public frontend::GarageHost {
public:
    frontend::Garage shared;
    frontend::CarStats stats;
    frontend::CarStats statsPurple;

    frontend::Garage& garage() override { return shared; }
    frontend::CarStats& carStats() override { return stats; }
    frontend::CarStats& carStatsPurple() override { return statsPurple; }
};

}  // namespace oj::test
