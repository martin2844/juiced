#pragma once
// A car of the game (the car object, 0xBC bytes, made by 0x0048DB40; docs/spec/06-cars.md): the model, the parts
// fitted, the paint and the other fields the saves keep (0x0048DD40 writes, 0x0048E020 reads).

#include "Core/Cars/CarDamage.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace oj {

struct CarDef;

// The car's career races counted (its raceCounts, +0x98 .. +0xA6).
enum class CarRaces : size_t {
    Sprints = 0,      // +0x98
    SprintWins = 1,   // +0x9A
    Circuits = 2,     // +0x9C: circuit and point-to-point races
    CircuitWins = 3,  // +0x9E
    Showoffs = 4,     // +0xA0
    PinkSlips = 5,    // +0xA2
    Wins = 6,         // +0xA4
    Races = 7,        // +0xA6: all but showoffs
};

struct Car {
    const CarDef* def = nullptr;        // the model (not saved: the saves keep its model name)
    // +0x00: its CarDef; the saves keep the CarDef's model name field (+0x9C, "ECLIPSE_STOCK"), 64 bytes as they are
    // in memory (bytes after the name's end are kept).
    std::array<char, 0x40> model{};
    std::array<uint8_t, 10> body;       // +0x04: the body part of each category (0xFF: none chosen)
    std::array<uint8_t, 10> mods;       // +0x0E: the catalogue part of each kind (ModKind; kNoMod: none chosen)
    uint32_t base = 0xFF8B8B8B;         // +0x18: paint
    uint32_t metallic = 0xFFC3C3C3;     // +0x1C
    uint32_t pearl = 0xFF8B8B8B;        // +0x20
    uint32_t metallicAmount;            // +0x24: float 0.5
    uint32_t pearlAmount;               // +0x28: float 0.1
    uint32_t neon = 0xFFFF0000;         // +0x2C
    // +0x30, +0x34: the HUD's dial colours (hudcolourmenu): the dials' backs (REVSBG, TIME_BG, MAPGLOW) and their
    // digits (SPEED, GEAR, TIME).
    uint32_t dialColour = 0xC82627C2;
    uint32_t dialTextColour = 0xFFB6B6FD;
    uint32_t decalColour = 0xFF808080;  // +0x38: the decal's (body part 8) colour (DECAL_COLOUR_CHART)
    uint32_t wheelColour = 0xFF808080;  // +0x3C: the wheels' colour, with wheelsColoured
    uint32_t wheelsColoured = 0;        // +0x40: the wheels take the colour +0x3C (0x0048C446)
    uint32_t tint = 0;                  // +0x44: float, the windows' tint; the saves keep it x 100 (to the nearest)
    uint32_t sponsorDecals = 1;         // +0x48: the sponsors' decals shown (sponsordecalmenu)
    uint32_t sponsorColour = 0xFFFFFFFF;  // +0x4C: their colour (the decal paint; class 28's first parameter)
    uint32_t metallicBought = 0;        // +0x50: a metallic paint bought (the paint windows), or one unlike the base
    uint32_t pearlBought = 0;           // +0x54: the pearl paint likewise
    // +0x58: float 0 .. 1, the chassis lowered by it x MAXDROPHEIGHT (the suspension's height slider; 1.0 for a
    // prototype car, else 0.8, when modified automatically).
    uint32_t lowering = 0;
    uint32_t gearing;                   // +0x5C: float 0.5; the forward ratios x (1 + (gearing - 0.5) x 0.2)
    uint32_t torqueFactor;              // +0x60: float 1.0, multiplies MaxTorque
    // What the race's damage did to the car (0x00488B50), kept until it is repaired.
    uint32_t steeringPulls = 0;         // +0x64: the steering pulls to a side
    uint32_t turboLost = 0;             // +0x68: the turbo is lost
    uint32_t neonFlickers = 0;          // +0x6C: the neon flickers
    uint32_t nitroLeaks = 0;            // +0x70: the nitro leaks
    uint32_t tyreWear = 0;              // +0x74: float, the tyres' wear, 0 .. 1 (0x0048EF70)
    uint32_t grip = 0;                  // +0x78: float, its last tyre's LateralMu as the race made it (0x00485CC5)
    uint32_t brakeWear = 0;             // +0x7C: float, the brakes' wear, 0 .. 1 (0x0048F1C0)
    uint32_t brake = 0;                 // +0x80: float, the BrakeMu of the last wheel with the brake part
    uint32_t nitro = 0;                 // +0x84: float, the nitro time (15 when auto modified)
    uint32_t rating = 0;                // +0x88: float, the performance rating (0x0048E8A0)
    std::array<uint32_t, 3> dynoRun;    // +0x8C .. +0x94: floats, the last dyno run's best torque, power and speed
                                        // (car_rolling), -1 before any
    // +0x98 .. +0xA6: the career's races with the car (0x004BA4F0; CMSStatsCarCollection's filters 5 .. 9): [0] sprint
    // races, [1] sprint wins, [2] circuit races, [3] circuit wins, [4] showoffs, [5] pink slip races, [6] wins, [7]
    // races (but showoffs).
    std::array<uint16_t, 8> raceCounts{};
    uint16_t& races(CarRaces k) { return raceCounts[static_cast<size_t>(k)]; }
    uint16_t races(CarRaces k) const { return raceCounts[static_cast<size_t>(k)]; }
    uint32_t spent = 0;                 // +0xA8: float, the money spent on the car (its "SPENT")
    uint32_t winnings = 0;              // +0xAC: float, the money it won (its "WINNINGS")
    uint32_t horn = 1;                  // +0xB0: the horn (hornmenu; the auto modification counter's value)
    // +0xB4: the 16 x 16 grid object, when made; the race dents it, and writing the car adds the race's damage to the
    // kept damage (0x00557B80).
    mutable std::optional<GridDamage> grid;

    Car();

    std::string modelName() const { return std::string(model.data(), strnlen(model.data(), model.size())); }
    void setModel(const std::string& name) {
        model.fill(0);
        std::memcpy(model.data(), name.data(), std::min(name.size(), model.size() - 1));
    }

    static float asFloat(uint32_t bits) {
        float f;
        std::memcpy(&f, &bits, 4);
        return f;
    }
    static uint32_t bitsOf(float f) {
        uint32_t u;
        std::memcpy(&u, &f, 4);
        return u;
    }

    // The save record (0x0048DD40 / 0x0048E020).
    void write(std::vector<uint8_t>& out) const;
    // false when the data ends early; offset moves past the record.
    bool read(const std::vector<uint8_t>& data, size_t& offset);
};

}  // namespace oj
