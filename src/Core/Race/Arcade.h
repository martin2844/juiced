#pragma once
// The arcade (docs/spec/09-arcade.md): the arcade object of ArcadeRaces.txt with the driver's progress, and the cars of
// the custom race by performance group (ArcadeUnlock.txt).

#include "Core/Career/EventKinds.h"
#include "Core/Data/ExeTables.h"
#include "Core/Saves/SaveEntries.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace oj {

// The arcade object (0x00728AF4, [game + 0x8C]; 0x53D8 bytes) in the stock layout, its fields read and written by
// offset as the stock code does. Championship c's race r is at (c x 5 + r) x 0xD4.
class Arcade {
public:
    static constexpr size_t kSize = 0x53D8;
    static constexpr size_t kRace = 0xD4;
    static constexpr size_t kChampionship = 0x424;
    static constexpr size_t kCount = 0x4664;          // the championships read
    static constexpr size_t kTracks = 0x4668;         // per championship 20 routes [UnlockTracks] opens
    static constexpr size_t kBonusTracks = 0x4BB8;    // per championship 20 bonus routes
    static constexpr size_t kRaceIndex = 0x511C;
    static constexpr size_t kChampionshipIndex = 0x5120;
    static constexpr size_t kUnused = 0x5124;         // cleared with a race chosen (0x0042D7D0), read by nothing
    static constexpr size_t kOpenedFlag = 0x512C;     // a car was just opened
    static constexpr size_t kOpenedCar = 0x5130;      // and which
    static constexpr size_t kExtreme = 0x5134;        // the extreme nitrous championships open
    static constexpr size_t kDevelopers = 0x5138;     // the developers' championships open
    static constexpr size_t kRouteFlags = 0x513C;     // 109 route flags, 0 = open
    static constexpr size_t kCarFlags = 0x52F0;       // 53 car flags by car id, 0 = open
    static constexpr size_t kResultWaits = 0x53C4;
    static constexpr size_t kFinalUnlocked = 0x53C8;  // the results' four news: the final race unlocked,
    static constexpr size_t kSeriesComplete = 0x53CC;  // the series complete,
    static constexpr size_t kNextSeries = 0x53D0;     // the next series unlocked,
    static constexpr size_t kArcadeComplete = 0x53D4;  // the arcade complete

    // A race's fields (kRace bytes).
    enum RaceField : size_t {
        kRaceKind = 0x00,            // EventKind: Circuit, PointToPoint, Sprint or Challenge
        kRaceExtreme = 0x04,         // of the extreme nitrous championships
        kRaceDevelopers = 0x08,      // of the developers' championships
        kRaceChallenge = 0x0C,       // ChallengeType: LapTime, PerfectLap, Speed or Showoff
        kRaceChallengeValue = 0x10,  // float
        kRaceRoute = 0x14,
        kRaceLaps = 0x18,
        kRaceWeather = 0x1C,    // 1 dry, 2 wet
        kRaceTimeOfDay = 0x20,  // 1 .. 4
        kRaceSkill = 0x24,      // the AI drivers' (float)
        kRaceCarCount = 0x28,
        kRaceCars = 0x2C,  // the car entries (kEntrySize bytes each)
        kRaceBase = 0xA4,  // the player's paint
        kRaceMetallic = 0xA8,
        kRacePearl = 0xAC,
        kRaceMetallicAmount = 0xB0,  // float
        kRacePearlAmount = 0xB4,     // float
        kRaceWheelColour = 0xB8,
        kRaceWheel = 0xBC,
        kRaceNeonColour = 0xC0,
        kRaceDecal = 0xC4,
        kRaceDecalColour = 0xC8,
        kRaceLocked = 0xCC,
        kRaceCompleted = 0xD0,
    };
    // A car entry of a race.
    enum EntryField : size_t {
        kEntryIndex = 0x00,     // its number in the race
        kEntryPlayer = 0x04,    // the player's car
        kEntryOpponent = 0x08,  // the opponent's number
        kEntryCar = 0x0C,       // the car id
        kEntryLevel = 0x10,     // its parts' level
        kEntrySize = 0x14,
    };

    int32_t i32(size_t offset) const;
    float f32(size_t offset) const;
    void put(size_t offset, int32_t value);
    // A race's kind and its challenge's type.
    EventKind raceKind(size_t race) const { return static_cast<EventKind>(i32(race + kRaceKind)); }
    ChallengeType raceChallenge(size_t race) const { return static_cast<ChallengeType>(i32(race + kRaceChallenge)); }
    // The race (c, r) and the race being raced (0x0042A930: +0x5120, +0x511C).
    static size_t race(int32_t championship, int32_t index) {
        return static_cast<size_t>(championship * 5 + index) * kRace;
    }
    size_t current() const { return race(i32(kChampionshipIndex), i32(kRaceIndex)); }
    int32_t count() const { return i32(kCount); }

    // 0x0042A960: the count, the waiting result, +0x5134, +0x5138 and both route lists cleared; ArcadeRaces.txt
    // (the reader 0x006EF7B0, its keywords 0x00723070); every race of the championships read locked and not completed
    // but the first four of championship 0, every car and route locked; then 0x0042B8A0 and 0x0042B960 for
    // championship 0.
    void load(std::string_view arcadeRaces, const ExeTables& tables);
    // ArcadeUnlock.sav (0x0042ABF0): the races' locked and completed flags, the car and route flags, +0x5138, +0x5134.
    void read(const save::ArcadeUnlock& progress);
    save::ArcadeUnlock progress() const;

    // 0x0042B9F0: every race completed but those of the developers' and the extreme nitrous championships.
    bool finished() const;
    // A car id opened (the flag 0, +0x5130 the id, +0x512C 1; car 48 opens car 22 too) unless open already.
    void openCar(int32_t id);
    // 0x0042AFB0: the player's car entry (+0x2C ..) of a race, the race's first entry when none is the player's.
    size_t playerEntry(size_t raceAt) const;
    // 0x0042B8A0 (championships under 17): the routes of the first four races and of the championship's list opened.
    void openRoutes(int32_t championship);
    // 0x0042B960 (under 17): the player's car of each of the first four races opened.
    void openCars(int32_t championship);
    // 0x0042B920 (under 17): the championship's bonus routes opened.
    void openBonusRoutes(int32_t championship);
    // 0x0042B5F0 / 0x0042B650: the developers' (+0x08) / the extreme nitrous (+0x04) championships' first four races
    // opened; +0x5138 / +0x5134 set.
    void openDevelopers();
    void openExtreme();
    // 0x0042B6B0 (the cheat): every car, route and race opened, then both.
    void openAll();
    // 0x0042B350: the race being raced won (playerCar: the model id of the first player's car, -1 none).
    void complete(int32_t playerCar);

private:
    std::array<uint8_t, kSize> m_bytes{};
};

// A new driver's progress: ArcadeRaces.txt read into a new object (0x0042A960), its ArcadeUnlock.sav.
save::ArcadeUnlock newArcadeProgress(std::string_view arcadeRaces, const ExeTables& tables);

// The cars of the custom race by performance group (the game's +0x84 object, 0x788 bytes, [0x00728AF0] while it
// reads): 8 groups of 60 car ids (0 ends a group), the place in the group (+0x780) and the group (+0x784).
// Native added-car rosters expand the groups; this object is not serialized in the stock layout.
struct ArcadeCarGroups {
    std::array<std::vector<int32_t>, 8> ids;
    ArcadeCarGroups();
    int32_t index = 0;  // +0x780
    int32_t group = 0;  // +0x784

    // 0x0042A640: everything 0, then ArcadeUnlock.txt: Level takes the next group (from -1) at its place 0, Car puts
    // the id of the name (0x0042A8F0, 11 when unknown) at the next place.
    void load(std::string_view text, const ExeTables& tables);
    // The id at the place in the group.
    int32_t current() const;
    // 0x0042A800: the next place, back to 0 past the group's last car.
    void next();
    // 0x0042A830: the place before, from 0 the group's last car (the last place of the 60 with an id; none: -1 kept).
    void previous();
};

// The car flag (+0x52F0 + id x 4): the car locked.
bool carLocked(const Arcade& arcade, int32_t id);
// 0x0042B810: the route's flag (+0x513C + route x 4) of a route file name less its last four characters (the
// route by its name alone, 0x004264E0); off line.
bool routeLocked(const Arcade& arcade, const ExeTables& tables, const std::string& fileName);
// 0x0042B7B0: a route of the city place (its number from 1) open (+0x513C) of the type (Any: a circuit or a
// point-to-point).
bool locationOpen(const Arcade& arcade, const ExeTables& tables, int32_t location, RouteType type);

}  // namespace oj
