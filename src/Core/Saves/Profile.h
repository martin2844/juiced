#pragma once
// The driver being played: the save system object of the stock game (the game object's +0x64, vtable 0x006EE950)
// with the entries of its juicey.dat (docs/spec/03-saves.md).

#include "Core/Base/Random.h"
#include "Core/Saves/SaveEntries.h"

#include <cstdint>
#include <string>

namespace oj::save {

struct Profile {
    std::wstring name;     // +0x20
    uint64_t id = 0;       // +0x30
    bool unsaved = false;  // +0x04: not written yet; saving over a driver of the same name asks first
    Settings settings;     // +0x54
    UiHelp help;           // the help pages seen (the table at 0x00715D48)

    // A new driver (0x0040AB80): the name, a new id, the settings of a new driver, no help page seen.
    static Profile create(std::wstring name, int languageIndex, CrtRandom& random);
};

// The id of a new driver (0x0040B0E0): four rand() values r1 .. r4,
// high = ((r2 ^ r1 << 16) & 0xFFFF0000) | ((r3 ^ r2 << 16) >> 16), low = r4 ^ r3 << 16.
uint64_t newProfileId(CrtRandom& random);

}  // namespace oj::save
