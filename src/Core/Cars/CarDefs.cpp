#include "Core/Cars/CarDefs.h"

#include "Core/Base/Script.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <optional>

namespace oj {

namespace {

bool same(std::string_view a, const char* b) {
    return _stricmp(std::string(a).c_str(), b) == 0;
}

// 0x00499290
int category(const std::string& name) {
    static const char* const kNames[] = {"FBUMP", "RBUMP", "BONNET", "SPOILER", "SKIRT",
                                         "HEADLIGHTS", "REARLIGHTS", "WMIRROR", "DECAL"};
    for (int i = 0; i < 9; ++i) {
        if (same(name, kNames[i])) return i;
    }
    return -1;
}

int indexOf(const std::vector<std::string>& names, const std::string& name) {
    for (size_t i = 0; i < names.size(); ++i) {
        if (same(name, names[i].c_str())) return static_cast<int>(i);
    }
    return -1;
}

// r g b as numbers, cut to whole bytes (the game runs with the FPU rounding toward zero), opaque (0x004998E0).
uint32_t colourOf(const ScriptLine& line, size_t first) {
    auto channel = [&](size_t i) { return static_cast<uint32_t>(static_cast<int>(line.real(i))) & 0xFF; };
    return 0xFF000000u | (channel(first) << 16) | (channel(first + 1) << 8) | channel(first + 2);
}

// 0x0049A8D0 (label, brand, name, standard): the part gets the next index and its brand (+0x2A); the list's +0x14
// is `standard` when it is the first part and not "NONE".
void addPart(CarPartList& list, CarPart part, bool standard) {
    part.index = static_cast<uint16_t>(list.parts.size());
    list.parts.push_back(std::move(part));
    if (list.parts.size() == 1 && !same(list.parts[0].model, "NONE")) {
        list.standard = standard;
    }
}

}  // namespace

void CarDefs::load(std::string_view text, const ExeTables& tables, CrtRandom& random) {
    cars.clear();
    paints.clear();
    neons.clear();
    dials.clear();
    decals.clear();
    wheels.clear();
    std::optional<CarDef> car;
    std::optional<CarPaint> paint;
    for (const ScriptLine& line : parseScript(text)) {
        const std::string& k = line.keyword;
        if (same(k, "[BeginCar]")) {
            car.emplace();
            car->name = line.word(0);
            car->displayName = line.word(1);
            car->id = tables.carModelId(car->name);
            car->model = car->name + "_STOCK";
            // 0x0049AB10: the neon's one part, "nono" (brand -1, not standard).
            CarPart neon;
            neon.model = "nono";
            neon.label = "neon";
            addPart(car->parts[kNeon], neon, false);
        } else if (same(k, "[EndCar]")) {
            if (car) {
                cars.push_back(std::move(*car));
                car.reset();
            }
        } else if (same(k, "[BeginColour]")) {
            paint.emplace();
        } else if (same(k, "BaseColour") && paint) {
            paint->base = colourOf(line, 0);
            paint->pearl = (paint->pearl & 0xFF000000) | (paint->base & 0x00FFFFFF);
            paint->pearlAmount = 0.05f;
            paint->metallicAmount = 0.5f;
        } else if (same(k, "MetallicColour") && paint) {
            paint->metallic = colourOf(line, 0);
            paint->metallicAmount = line.real(3);
        } else if (same(k, "PearlescentColour") && paint) {
            paint->pearl = colourOf(line, 0);
            paint->pearlAmount = line.real(3);
        } else if (same(k, "[EndColour]")) {
            if (paint) {
                paints.push_back(*paint);
                paint.reset();
            }
        } else if (same(k, "[DialColour]")) {
            dials.push_back({colourOf(line, 0), colourOf(line, 3)});
        } else if (same(k, "[NeonColour]")) {
            neons.push_back(colourOf(line, 0));
        } else if (same(k, "[DecalColour]")) {
            decals.push_back(colourOf(line, 0));
        } else if (same(k, "[WheelColour]")) {
            wheels.push_back(colourOf(line, 0));
        } else if (!car) {
            continue;
        } else if (same(k, "MANUFACTURER")) {
            car->make = std::max(0, indexOf(tables.manufacturers, line.word(0)));
            car->region = std::max(0, indexOf(tables.regions, line.word(1)));
        } else if (same(k, "VALUE")) {
            car->value = line.integer(0);
        } else if (same(k, "STATE")) {
            car->state = same(line.word(0), "NEW") ? 0 : 1;
        } else if (same(k, "STOCKTURBO")) {
            car->stockTurbo = true;
        } else if (same(k, "STANDARD")) {
            const int c = category(line.word(0));
            if (c >= 0) {
                CarPart part;
                part.model = line.word(1);
                part.label = "Standard";
                addPart(car->parts[static_cast<size_t>(c)], part, true);
            }
        } else if (same(k, "CUSTOM")) {
            const int c = category(line.word(0));
            if (c >= 0) {
                CarPart part;
                part.model = line.word(1);
                part.label = "Option";
                if (line.words.size() < 3) {
                    // 0x004995F0: rand() % 3 == 2 gives NUUK (48), anything else AERO (47) (Stock problem: compared
                    // with 3 for SWEET, 49, which never comes). The fixed build gives each a third: 1 SWEET.
                    const uint32_t r = static_cast<uint32_t>(random.next()) % 3;
                    part.brand = r == 2 ? 0x30 : r == 1 ? 0x31 : 0x2F;
                } else {
                    part.brand = -1;
                    for (const ExeTables::Brand& b : tables.brands) {
                        if (same(line.word(2), b.name.c_str())) {
                            part.brand = b.id;
                            break;
                        }
                    }
                }
                addPart(car->parts[static_cast<size_t>(c)], part, false);
            }
        } else if (same(k, "ALLOW_NONE")) {
            const int c = category(line.word(0));
            if (c >= 0) {
                car->parts[static_cast<size_t>(c)].allowNone = true;
            }
        } else if (same(k, "STANDARDWHEELS")) {
            car->wheels = line.word(0);
        } else if (same(k, "STANDARDEXHAUST")) {
            car->exhaust = line.word(0);
        } else if (same(k, "ENGINE_SOUND")) {
            car->engineSound = line.word(0);
        } else if (same(k, "MAXDROPHEIGHT")) {
            car->maxDropHeight = line.real(0);
        }
    }
}

// [BeginMod] <kind> .. [EndMod] (0x00499F30 .. 0x0049A270). [BeginMod] clears the level, the texts and the
// numbers of PHYSICS and DATAVAL; the shape, the supplier and the cost carry over from the part before.
void PartCatalogue::load(std::string_view text, const ExeTables& tables) {
    static const char* const kKinds[] = {"SUSPENSION", "EXHAUST", "NITRO", "TURBO", "WHEEL",
                                         "BRAKE",      "TYRE",    "ICE",   "INDUCTION", "GEARBOX"};
    int kind = -1;
    CarPart part;
    float physics[4] = {};
    for (const ScriptLine& line : parseScript(text)) {
        const std::string& k = line.keyword;
        if (same(k, "[BeginMod]")) {
            kind = -1;
            for (int i = 0; i < kModKinds; ++i) {
                if (same(line.word(0), kKinds[i])) kind = i;
            }
            part.level = 0;
            part.label.clear();
            part.uiOn.clear();
            part.uiOff.clear();
            part.dataValue = 0.0f;
            std::fill(std::begin(physics), std::end(physics), 0.0f);
        } else if (same(k, "LEVEL")) {
            const std::string& level = line.word(0);
            part.level = same(level, "prototype") ? 3 : same(level, "street") ? 0 : same(level, "sports") ? 1 : 2;
        } else if (same(k, "SUPPLIER")) {
            part.brand = -1;
            for (const ExeTables::Brand& b : tables.brands) {
                if (same(line.word(0), b.name.c_str())) part.brand = b.id;
            }
        } else if (same(k, "TEXT")) {
            part.label = line.word(0);
        } else if (same(k, "SHAPE")) {
            part.model = line.word(0);
        } else if (same(k, "COST")) {
            part.cost = line.integer(0);
        } else if (same(k, "PHYSICS")) {
            for (size_t i = 0; i < 4; ++i) physics[i] = line.real(i);
        } else if (same(k, "UIONTEX")) {
            part.uiOn = line.word(0);
        } else if (same(k, "UIOFFTEX")) {
            part.uiOff = line.word(0);
        } else if (same(k, "DATAVAL")) {
            part.dataValue = line.real(0);
        } else if (same(k, "[EndMod]")) {
            if (kind < 0) {
                continue;
            }
            CarPart added = part;
            for (size_t i = 0; i < 4; ++i) {
                added.physics[i] = static_cast<uint8_t>(static_cast<long>(physics[i] * 255.0f));
            }
            addPart(lists[static_cast<size_t>(kind)], added, false);
        }
    }
}

const CarDef* CarDefs::byId(int id) const {
    for (const CarDef& c : cars) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

const CarDef* CarDefs::byModel(std::string_view model) const {
    for (const CarDef& c : cars) {
        if (same(c.model, std::string(model).c_str())) return &c;
    }
    return nullptr;
}

}  // namespace oj
