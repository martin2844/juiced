#pragma once
// What the custom race's windows share of their own helpers.

#include "Game/Frontend/CustomRace/CustomRaceHost.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Core/Base/Random.h"
#include "Core/Base/Text.h"
#include "Core/Base/X87.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Cars/CarRepair.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Race/Arcade.h"
#include "Core/Race/RaceSession.h"
#include "Core/Tracks/Track.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>

namespace oj::frontend::customrace {

inline bool same(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

// An 8-bit text widened character by character (0x00409370).
inline std::wstring latin(const std::string& text) {
    std::wstring wide;
    for (char c : text) wide += static_cast<wchar_t>(static_cast<unsigned char>(c));
    return wide;
}

// The network game ([0x0075EF8C]), this machine hosting it (its +0x0C) or only joined.
inline net::NetGame* network() {
    return net::netGame();
}

inline bool hosting() {
    return net::netGame() != nullptr && net::netGame()->transport.hosting();
}

inline bool client() {
    return net::netGame() != nullptr && !net::netGame()->transport.hosting();
}

// Each element of that name (case aside) hidden.
inline void hideNamed(ui::Screen& s, const char* name) {
    for (int e = 0; e < s.size(); ++e) {
        if (_stricmp(s.nameOf(e).c_str(), name) == 0) s.setHidden(e, true);
    }
}

// The car id at a place of a group of the custom race's list (within the list).
inline int32_t carAt(const ArcadeCarGroups& groups, int32_t group, int32_t place) {
    return groups.ids[static_cast<size_t>(std::clamp(group, 0, 7))][static_cast<size_t>(std::clamp(place, 0, 59))];
}

// The arcade car of a car id: the place of the first one of that model in the list, 0 when none (0x00489D70).
inline uint32_t arcadeCarOf(const std::vector<Car>& cars, int32_t id) {
    for (size_t i = 0; i < cars.size(); ++i) {
        if (cars[i].def != nullptr && cars[i].def->id == id) return static_cast<uint32_t>(i);
    }
    return 0;
}

// 0x004600C0: the car's parts cleared: +0x58 and +0x44 0, the body parts 0 but the decal and the neon (-1), every
// catalogue part -1.
inline void clearParts(Car& car) {
    car.lowering = 0;
    car.tint = 0;
    for (size_t i = 0; i < car.body.size(); ++i) car.body[i] = i == 8 || i == 9 ? 0xFF : 0;
    car.mods.fill(0xFF);
}

// 0x0045FFA0: a car record back as it was: its CarDef, its 20 part bytes and its fields +0x18 .. +0x70.
inline void restoreCar(Car& car, const Car& from) {
    car.def = from.def;
    car.body = from.body;
    car.mods = from.mods;
    car.base = from.base;
    car.metallic = from.metallic;
    car.pearl = from.pearl;
    car.metallicAmount = from.metallicAmount;
    car.pearlAmount = from.pearlAmount;
    car.neon = from.neon;
    car.dialColour = from.dialColour;
    car.dialTextColour = from.dialTextColour;
    car.decalColour = from.decalColour;
    car.wheelColour = from.wheelColour;
    car.wheelsColoured = from.wheelsColoured;
    car.tint = from.tint;
    car.sponsorDecals = from.sponsorDecals;
    car.sponsorColour = from.sponsorColour;
    car.metallicBought = from.metallicBought;
    car.pearlBought = from.pearlBought;
    car.lowering = from.lowering;
    car.gearing = from.gearing;
    car.torqueFactor = from.torqueFactor;
    car.steeringPulls = from.steeringPulls;
    car.turboLost = from.turboLost;
    car.neonFlickers = from.neonFlickers;
    car.nitroLeaks = from.nitroLeaks;
}

}  // namespace oj::frontend::customrace
