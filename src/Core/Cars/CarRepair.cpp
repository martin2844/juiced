#include "Core/Cars/CarRepair.h"

#include "Core/Race/RaceSession.h"
#include "Core/Base/X87.h"

#include <bit>
#include <cmath>
#include <limits>

namespace oj {

namespace {

constexpr uint32_t kFullNitro = 0x41700000;  // 15.0

float asFloat(uint32_t bits) {
    return std::bit_cast<float>(bits);
}

// The COST of the car's part of a kind (none for -1, the list's first past its end), held to `most`.
float partCost(const Car& car, const PartCatalogue& catalogue, size_t kind, float most) {
    const auto index = static_cast<int8_t>(car.mods[kind]);
    int32_t cost = 0;
    const std::vector<CarPart>& parts = catalogue.lists[kind].parts;
    if (index != -1 && !parts.empty()) {
        // Stock problem: an index below -1 reads before the list; the first part here.
        cost = index >= 0 && index < static_cast<int>(parts.size()) ? parts[static_cast<size_t>(index)].cost
                                                                    : parts.front().cost;
    }
    float v = static_cast<float>(cost);
    if (v > most) v = most;
    return v;
}

// 0x0048EE70 / 0x0048EDD0: a worn part's cost x 0.3 (`none` when that is 0); 0 while the wear is 0.
float wornCost(const Car& car, const PartCatalogue& catalogue, uint32_t wear, size_t kind, float none) {
    if (asFloat(wear) == 0.0f) return 0.0f;
    const float v = partCost(car, catalogue, kind, 5000.0f) * 0.3f;
    return v == 0.0f ? none : v;
}

// 0x0048ED50: the nitro part's cost x 0.1; 0 with the nitro full or without a nitro part.
float nitroCost(const Car& car, const PartCatalogue& catalogue) {
    if (car.nitro == kFullNitro || car.mods[kNitro] == kNoMod) return 0.0f;
    return partCost(car, catalogue, kNitro, 15000.0f) * 0.1f;
}

}  // namespace

float damageLevel(const GridDamage& grid) {
    float sum = 0.0f;
    int32_t count = 0;
    for (size_t k = 0; k < 256; ++k) {
        if (!grid.made[k]) continue;
        const float v = std::fabs(grid.damage[k]) + std::fabs(grid.saved[k]);
        if (v > 0.0f) {
            ++count;
            sum = sum + v;
        }
    }
    if (count == 0) return sum;
    float level = sum / static_cast<float>(count) * 0.05f;
    if (level < 0.0f) return 0.0f;
    if (level > 1.0f) level = 1.0f;
    return level;
}

float repairCost(const Car& car, const PartCatalogue& catalogue) {
    const float level = car.grid ? damageLevel(*car.grid) : 0.0f;
    float cost = static_cast<float>(cappedCarValue(car, catalogue)) * level * 0.1f;
    if (asFloat(car.brakeWear) > 0.1f) {
        cost = static_cast<float>(x87::chop(wornCost(car, catalogue, car.brakeWear, kBrake, 100.0f))) + cost;
    }
    if (asFloat(car.tyreWear) > 0.1f) {
        cost = static_cast<float>(x87::chop(wornCost(car, catalogue, car.tyreWear, kTyre, 200.0f))) + cost;
    }
    if (car.nitro != kFullNitro) cost = static_cast<float>(x87::chop(nitroCost(car, catalogue))) + cost;
    if (cost > 0.0f && cost < 500.0f) return 500.0f;
    if (cost > 25000.0f) return 25000.0f;
    return cost;
}

void refreshCar(Car& car) {
    if (car.grid) car.grid->damage.fill(0.0f);
    car.nitro = kFullNitro;
    car.brakeWear = 0;
    car.tyreWear = 0;
    car.steeringPulls = 0;
    car.neonFlickers = 0;
    car.nitroLeaks = 0;
}

void repairCar(Car& car) {
    if (car.grid) {
        car.grid->saved.fill(0.0f);
        car.grid->damage.fill(0.0f);
    }
    car.nitro = kFullNitro;
    car.brakeWear = 0;
    car.tyreWear = 0;
    car.steeringPulls = 0;
    car.nitroLeaks = 0;
    if (car.neonFlickers != 0) car.body[9] = 0;
    car.neonFlickers = 0;
}

int saleWorth(const Car& car, const PartCatalogue& catalogue) {
    float worth = static_cast<float>(carValue(car, catalogue)) * 0.7f - repairCost(car, catalogue) * 3.0f;
    const float least = static_cast<float>(carValue(car, catalogue)) * 0.2f;
    if (least > worth) worth = least;
    return x87::chop(worth);
}

}  // namespace oj
