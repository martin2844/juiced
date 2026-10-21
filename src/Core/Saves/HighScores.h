#pragma once
// The routes' records (docs/spec/03-saves.md): the save system object of the game object's +0xA8 (0x0040A0E0, 0x1128
// bytes) keeps one record per route of the routes' table (0x00711A3C, 109), written to SAVE\HS\USERDATA.

#include "Core/Data/ExeTables.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace oj {

// A route's record (0x28 bytes).
struct RouteRecord {
    std::array<uint8_t, 0x20> name{};  // +0x00: the driver's name, UTF-16, at most 15 characters and a 0
    float time = 0.0f;                 // +0x20: the best lap (of a route of type 3 the most)
    int32_t car = 0;                   // +0x24: the car model's id
};

struct HighScores {
    static constexpr size_t kRoutes = 109;
    static constexpr size_t kFileSize = 8 + kRoutes * 0x28;  // SAVE\HS\USERDATA: the id, then the records

    int32_t written = 1;                     // +0x08: cleared by a new record until the records are written
    std::array<RouteRecord, kRoutes> routes{};  // +0x0C
    uint64_t id = 0;                         // +0x1118: four rand() values (0x0040A110)
    int32_t loaded = 0;                      // +0x1120: set by loadhighscores once read

    // 0x0040A110 without the id: each route's name "-", its record 300 s (0 for a route of type 3), its car 0.
    void reset(const ExeTables& tables);
    // The route's record (0 past the table).
    float time(int route) const;
    // 0x0040A1C0: a route of type 3 takes a greater value, any other a lap under its record of at least 1 s. A route
    // past the table reads past it (stock problem): none here.
    bool better(int route, float time, const ExeTables& tables) const;
    // 0x0040A220 / 0x0040A290: a better lap becomes the route's record with the driver's name (its first 15
    // characters) and the car model's id; the records are then not written. True when kept.
    bool keep(int route, const std::wstring& driver, float time, int32_t car, const ExeTables& tables);
    // 0x0040A440 / 0x0040A370: SAVE\HS\USERDATA, the id (8 bytes) then the records as they are in memory. Reading
    // or writing sets `written`.
    bool read(const std::vector<uint8_t>& data);
    std::vector<uint8_t> write();
};

}  // namespace oj
