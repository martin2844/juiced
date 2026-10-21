#pragma once
// A career car's repair (docs/spec/08-career.md): how damaged it is, what its repair costs and the repair.

#include "Core/Cars/Car.h"
#include "Core/Cars/CarDefs.h"

namespace oj {

// 0x00557E80: the grid's damage: the mean over the cells with damage (each its kept and its race's damage as sizes)
// x 0.05, within 0 .. 1; 0 without such cells.
float damageLevel(const GridDamage& grid);

// 0x0048EBC0: the repair's cost: the car's worth (0x0048D430) x its damage level x 0.1, then (each cut toward zero)
// the brakes' part (the car's +0x7C over 0.1: the brake part's COST at most 5000 x 0.3, 100 for none), the tyres'
// (+0x74 over 0.1: the tyre part's, 200 for none) and the nitro's (its time not 15: the nitro part's COST at most
// 15000 x 0.1, 0 without a nitro part); a cost over 0 at least 500, at most 25000.
float repairCost(const Car& car, const PartCatalogue& catalogue);

// 0x0048DA40: what the dealer pays for the car: its value (0x0048CFE0) x 0.7 less its repair's cost x 3, at least its
// value x 0.2, cut.
int saleWorth(const Car& car, const PartCatalogue& catalogue);

// 0x0048EA80: the grid's race damage cleared (0x00557C90), the nitro time 15 (0x0048F210, held within 0 .. 15),
// +0x7C, +0x74 (each held within 0 .. 1), +0x64, +0x6C and +0x70 cleared.
void refreshCar(Car& car);

// 0x0048EB20: the grid's every damage cleared (0x00557C00), the nitro time 15, +0x7C, +0x74, +0x64, +0x70 and +0x6C
// cleared (with +0x6C set the neon part, +0x0D, 0).
void repairCar(Car& car);

}  // namespace oj
