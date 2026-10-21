#pragma once
// Car collections ([BeginCollection] files: CMSPlayersCrewCollection.txt, CMSCrewNNCollection.txt,
// arcadecollection.txt) and the automatic modification of cars (docs/spec/06-cars.md).

#include "Core/Cars/Car.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Base/Random.h"

#include <string_view>
#include <vector>

namespace oj {

// What making cars draws on: the models, the parts catalogue, the game's rand() and the counter the automatic
// modification hands out (0x00717868, 1 .. 6).
struct CarMaking {
    const CarDefs& defs;
    const PartCatalogue& catalogue;
    CrtRandom& random;
    int& counter;
};

// The CarDef of a model name (0x0048CC30): "_STOCK" is added to a name without it; compared without regard to case
// with the CarDefs' model names; the first CarDef when none matches.
const CarDef* findCarDef(const CarDefs& defs, std::string_view name);

// A new car of a model with the stock defaults (0x0048DB40).
Car makeCar(const CarDef* def);

// 0x0048F2C0: random body parts, catalogue parts of a level (-1 any; for a prototype car the first prototype part of
// each kind), a neon and a decal colour, a wheel.
void autoModify(Car& car, bool prototype, int level, CarMaking& making);

// The cars of a collection file in order (0x00489DA0).
std::vector<Car> loadCollection(std::string_view text, CarMaking& making);

}  // namespace oj
