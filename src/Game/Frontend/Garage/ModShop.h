#pragma once
// modshop: the upgrades to the next classes.

#include "Game/Frontend/Garage/Garage.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace oj::frontend {

// An upgrade of modshop (+0x10 + n x 0x4C): the class reached, its rating, cost, the four meters and the
// catalogue part of each kind (-1 none).
struct ModShopEntry {
    int32_t unused = 0;       // +0x00: 0 by the enter (0x00442AB4), not read
    int32_t classNumber = 0;  // +0x04: the class + 1
    int32_t found = 0;        // +0x08
    float rating = 0.0f;      // +0x0C
    float cost = 0.0f;        // +0x10
    float speed = 0.0f;       // +0x14
    float accel = 0.0f;       // +0x18
    float braking = 0.0f;     // +0x1C
    float handling = 0.0f;    // +0x20
    std::array<int32_t, 10> parts{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};  // +0x24
};

// The window of the record 0x0071FFE8.
class ModShop {
public:
    // 0x00442A80
    void enter(GarageHost& host, ui::Screen& s);
    // 0x00444690
    void update(GarageHost& host, ui::Screen& s, uint32_t event);

private:
    // A supplier drawn for a kind's parts of a level and its first part (into the catalogue's list).
    int32_t drawPart(GarageHost& host, size_t kind, int level);

    int32_t m_count = 0;                    // +0x0C
    std::array<ModShopEntry, 10> m_entries;  // +0x10
    int m_shown = -1;                       // +0x310: the item last shown (its place in menu)
    float m_timer = 3.0f;                   // +0x3AC: the analysis's seconds
    int32_t m_beeps = 3;                    // +0x3B0
    int32_t m_answer = -1;                  // +0x3B4
};

}  // namespace oj::frontend
