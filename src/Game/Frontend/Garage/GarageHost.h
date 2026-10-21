#pragma once
// What the garage's windows (carbuy, carsel, the dealer's, modshop, the repair and the customising windows over
// carsel) ask of the front end besides what every window does.

#include "Game/Frontend/WindowHost.h"

namespace oj::frontend {

class CarStats;
class Garage;

class GarageHost : public virtual WindowHost {
public:
    // What the garage's windows share: the car shown, its scene, the prices and the customising windows' globals.
    virtual Garage& garage() = 0;
    // The records of Car_Stats2 and Car_Stats2purple, whose garage flag the windows that open them set.
    virtual CarStats& carStats() = 0;
    virtual CarStats& carStatsPurple() = 0;
};

}  // namespace oj::frontend
