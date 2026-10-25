#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
#include "Mods/Creative.h"
#endif
#include "Core/Cars/CarCollection.h"

#include "Core/Base/Script.h"

#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

namespace oj {

namespace {

bool same(std::string_view a, const char* b) {
    return _stricmp(std::string(a).c_str(), b) == 0;
}

uint32_t next(CrtRandom& random) {
    return static_cast<uint32_t>(random.next());
}

// r g b as whole numbers (atol), opaque.
uint32_t colourOf(const ScriptLine& line) {
    auto channel = [&](size_t i) { return static_cast<uint32_t>(line.integer(i)) & 0xFF; };
    return 0xFF000000u | (channel(0) << 16) | (channel(1) << 8) | channel(2);
}

}  // namespace

const CarDef* findCarDef(const CarDefs& defs, std::string_view name) {
    std::string model(name);
    if (model.find("_STOCK") == std::string::npos) {
        model += "_STOCK";
    }
    for (const CarDef& d : defs.cars) {
        if (same(model, d.model.c_str())) {
            return &d;
        }
    }
    return defs.cars.empty() ? nullptr : &defs.cars.front();
}

Car makeCar(const CarDef* def) {
    Car car;
    car.def = def;
    if (def != nullptr) {
        car.setModel(def->model);
    }
    return car;
}

void autoModify(Car& car, bool prototype, int level, CarMaking& making) {
    car.lowering = Car::bitsOf(prototype ? 1.0f : 0.8f);
    car.horn = static_cast<uint32_t>(making.counter);
    if (++making.counter > 6) {
        making.counter = 1;
    }
    car.tint = Car::bitsOf(prototype ? 0.8f : 0.5f);

    // Body parts: the neon is always 1; a category with more than one part gets one of the parts after the first;
    // a decal list of n gets 1 .. n - 9: neither its first (the prototypes', decal 0) nor its last eight (the
    // crews', 0x004A9080).
    for (size_t c = 0; c < kCarPartCategories; ++c) {
        if (c == kNeon) {
            car.body[c] = static_cast<uint8_t>(next(making.random) % 1 + 1);
            continue;
        }
        auto count = static_cast<uint32_t>(car.def != nullptr ? car.def->parts[c].parts.size() : 0);
#if defined(OJ_MACOS) || defined(OJ_LINUX) || defined(OJ_ANDROID)
        if (c == kDecal && car.def != nullptr) {
            count = static_cast<uint32_t>(mods::decalCount(*car.def, count));
            // Small add-on catalogues may already include a final user-image slot.
            // AI/auto-mod must not select the active player's personal artwork.
            const int custom = mods::logoIndex(car.modelName());
            if (mods::enabled() && count <= 9 && custom >= 0 && custom == int(count) - 1) --count;
        }
#endif
        const uint32_t choices = count > 1 ? count - 1 : count;
        if (choices == 0) {
            continue;
        }
        if (c == kDecal) {
            // Stock problem: (choices - 8) is not checked (0x0048F392, unsigned): a list of 9 decals divides by zero
            // and the game stops (0 here); a list of 1 .. 8 gives an unsigned wrap, so the decal is rand() + 1 cut
            // to a byte, past the list. The game's cars have 31 decals each. The fixed build gives a list of 9 or
            // fewer the decal 0, as the 9 (no rand() drawn).
            const uint32_t span = choices > 8 ? choices - 8 : 0;
            car.body[c] = prototype || span == 0 ? 0 : static_cast<uint8_t>(next(making.random) % span + 1);
        } else if (count > 1) {
            car.body[c] = static_cast<uint8_t>(next(making.random) % choices + 1);
        } else {
            car.body[c] = 0;
        }
    }

    // Catalogue parts.
    for (size_t k = 0; k < kModKinds; ++k) {
        const std::vector<CarPart>& parts = making.catalogue.lists[k].parts;
        const auto n = static_cast<uint32_t>(parts.size());
        if (n == 0) {
            continue;
        }
        if (prototype) {
            uint32_t i = 0;
            do {
                car.mods[k] = static_cast<uint8_t>(i);
            } while (parts[i].level != 3 && ++i < n);
        } else if (level == -1) {
            car.mods[k] = static_cast<uint8_t>(next(making.random) % n);
        } else {
            uint32_t r = 0;
            int tries = 0;
            while (true) {
                r = next(making.random);
                ++tries;
                if (tries > 100) break;
                if (parts[r % n].level == level) break;
            }
            car.mods[k] = static_cast<uint8_t>(r % n);
        }
    }

    const std::vector<uint32_t>& neons = making.defs.neons;
    if (!neons.empty()) {
        car.neon = neons[next(making.random) % neons.size()];
    }
    const std::vector<uint32_t>& decals = making.defs.decals;
    if (!decals.empty()) {
        car.decalColour = decals[next(making.random) % decals.size()];
    }
    car.nitro = Car::bitsOf(15.0f);
    const auto wheels = static_cast<uint32_t>(making.catalogue.lists[kWheel].parts.size());
    if (wheels != 0) {
        car.mods[kWheel] = static_cast<uint8_t>(next(making.random) % wheels);
    }
}

// [BeginCollection], BeginCar <model> (white base and pearl, black metallic), BaseColour r g b (also the pearl),
// MetallicColour r g b, PearlescentColour r g b, Modification <category> <n>, Prototype, AutoMod, EndCar,
// [EndCollection] (0x0049A380 .. 0x0049A7E0).
std::vector<Car> loadCollection(std::string_view text, CarMaking& making) {
    std::vector<Car> cars;
    std::optional<Car> car;
    bool prototype = false;
    bool autoMod = false;
    for (const ScriptLine& line : parseScript(text)) {
        const std::string& k = line.keyword;
        if (same(k, "BeginCar")) {
            car = makeCar(findCarDef(making.defs, line.word(0)));
            car->base = 0xFFFFFFFF;
            car->metallic = 0xFF000000;
            car->pearl = 0xFFFFFFFF;
            prototype = false;
            autoMod = false;
        } else if (!car) {
            continue;
        } else if (same(k, "BaseColour")) {
            car->base = colourOf(line);
            car->pearl = car->base;
        } else if (same(k, "MetallicColour")) {
            car->metallic = colourOf(line);
        } else if (same(k, "PearlescentColour")) {
            car->pearl = colourOf(line);
        } else if (same(k, "Modification")) {
            static const char* const kCategories[] = {"FBUMP",      "RBUMP",      "BONNET", "SPOILER", "SSKIRT",
                                                      "HEADLIGHTS", "REARLIGHTS", "WMIR",   "DECAL"};
            const std::string& what = line.word(0);
            const auto n = static_cast<uint8_t>(line.integer(1));
            if (same(what, "WHEELS")) {
                car->mods[kWheel] = n;
            } else {
                for (size_t c = 0; c < 9; ++c) {
                    if (same(what, kCategories[c])) car->body[c] = n;
                }
            }
        } else if (same(k, "Prototype")) {
            prototype = true;
        } else if (same(k, "AutoMod")) {
            autoMod = true;
        } else if (same(k, "EndCar")) {
            if (autoMod || prototype) {
                autoModify(*car, prototype, -1, making);
            }
            cars.push_back(std::move(*car));
            car.reset();
        }
    }
    return cars;
}

}  // namespace oj
