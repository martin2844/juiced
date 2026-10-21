#pragma once
// scripts\cardefs.txt (docs/spec/06-cars.md): the car models with their parts, and the colour lists of the game.

#include "Core/Data/ExeTables.h"
#include "Core/Base/Random.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace oj {

// The part categories of cardefs.txt (0x00499290); 9 holds the neon.
enum CarPartCategory {
    kFrontBumper = 0, kRearBumper = 1, kBonnet = 2, kSpoiler = 3, kSkirt = 4, kHeadlights = 5, kRearLights = 6,
    kWingMirror = 7, kDecal = 8, kNeon = 9, kCarPartCategories = 10,
};

// A part (0x2C bytes, 0x0049A8D0): of a car (cardefs.txt) or of the parts catalogue (gearboxes.txt ...).
struct CarPart {
    std::string model;      // the part's mesh (focus_2_FBUMP_VS1, EXHAUST_BORLA_VS5); "nono" for the neon
    std::string label;      // "Standard", "Option", "neon"; a catalogue part's TEXT
    int brand = -1;         // +0x2A (a byte): supplier id (ExeTables::brands), -1 for a standard part
    uint16_t index = 0;     // +0x24, its place in the list
    // The catalogue's fields
    int cost = 0;                     // +0x20, COST
    float dataValue = 0.0f;           // +0x1C, DATAVAL
    int level = 0;                    // +0x2B, LEVEL: street 0, sports 1, other 2, prototype 3
    std::array<uint8_t, 4> physics{}; // +0x26 .. +0x29, PHYSICS a b c d as a x 255 .. (cut to whole numbers)
    std::string uiOn;                 // UIONTEX
    std::string uiOff;                // UIOFFTEX
};

struct CarPartList {
    std::vector<CarPart> parts;
    bool allowNone = false;  // ALLOW_NONE: the car may go without (+0x10 of the list)
    // +0x14: the first part (not "NONE") was a STANDARD one (0x0049A8D0's last argument: 1 for STANDARD, 0 for
    // CUSTOM, the catalogue's parts and the neon's "nono"). A car with none chosen fits part 0 only then
    // (0x0048C2E0), so a list of CUSTOM parts alone (the decals) leaves the car without one.
    bool standard = false;
};

struct CarDef {
    int id = 0;                   // +0x000, by the name (ExeTables::carModelId)
    int make = 0;                 // +0x004, MANUFACTURER <make> <region>
    int region = 0;               // +0x008
    int value = 0;                // +0x00C, VALUE
    int state = 0;                // +0x010, STATE: 0 NEW, 1 otherwise
    bool stockTurbo = false;      // +0x014, STOCKTURBO
    float maxDropHeight = 6.0f;   // +0x018, MAXDROPHEIGHT
    std::string name;             // +0x01C, [BeginCar] <name> "<display name>"
    std::string displayName;      // +0x05C
    std::string model;            // +0x09C, <name>_STOCK: the name the saves keep
    std::string wheels;           // +0x0DC, STANDARDWHEELS
    std::string exhaust;          // +0x11C, STANDARDEXHAUST
    std::string engineSound;      // +0x1DC, ENGINE_SOUND
    std::array<CarPartList, kCarPartCategories> parts;  // +0x3E0 + category * 0x18
};

// A paint of [BeginColour] .. [EndColour]: BaseColour r g b (also the third colour), MetallicColour r g b <f>,
// PearlescentColour r g b <f>; colours 0xFFRRGGBB.
struct CarPaint {
    uint32_t base = 0xFFFFFFFF;
    uint32_t metallic = 0xFF000000;
    uint32_t pearl = 0xFFFFFFFF;
    float metallicAmount = 0.5f;
    float pearlAmount = 0.1f;
};

// The parts catalogue (0x00498BC0): lists by kind, read from gearboxes.txt, suspensions.txt, exhausts.txt,
// wheels.txt, nitros.txt, turbos.txt, brakes.txt, tyres.txt, ice.txt, induction.txt in that order.
enum ModKind {
    kSuspension = 0, kExhaust = 1, kNitro = 2, kTurbo = 3, kWheel = 4, kBrake = 5, kTyre = 6, kIce = 7,
    kInduction = 8, kGearbox = 9, kModKinds = 10,
};
// A car's part of a kind not chosen (the stock one).
constexpr uint8_t kNoMod = 0xFF;

struct PartCatalogue {
    static constexpr const char* kFiles[] = {"gearboxes.txt", "suspensions.txt", "exhausts.txt", "wheels.txt",
                                             "nitros.txt",    "turbos.txt",      "brakes.txt",   "tyres.txt",
                                             "ice.txt",       "induction.txt"};
    std::array<CarPartList, kModKinds> lists;

    // One file of the catalogue, read into the lists of its [BeginMod] kinds (0x00498D20).
    void load(std::string_view text, const ExeTables& tables);
};

struct CarDefs {
    std::vector<CarDef> cars;                 // in the file's order
    std::vector<CarPaint> paints;             // [BeginColour] blocks
    std::vector<uint32_t> neons;              // [NeonColour] r g b
    std::vector<std::array<uint32_t, 2>> dials;  // [DialColour] r g b r g b
    std::vector<uint32_t> decals;             // [DecalColour] r g b
    std::vector<uint32_t> wheels;             // [WheelColour] r g b

    // Reads the text the stock game's way (0x0048C650); a CUSTOM part with no brand gets AERO or NUUK by rand().
    void load(std::string_view text, const ExeTables& tables, CrtRandom& random);
    const CarDef* byId(int id) const;
    const CarDef* byModel(std::string_view model) const;
};

}  // namespace oj
