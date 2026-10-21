#include "Core/Saves/HighScores.h"

#include <cstring>

namespace oj {

namespace {

bool typeThree(int route, const ExeTables& tables) {
    return route >= 0 && static_cast<size_t>(route) < tables.routes.size() &&
           tables.routes[static_cast<size_t>(route)].type == RouteType::Cruise;
}

}  // namespace

void HighScores::reset(const ExeTables& tables) {
    for (size_t r = 0; r < kRoutes; ++r) {
        RouteRecord& record = routes[r];
        record.name.fill(0);
        record.name[0] = '-';
        record.car = 0;
        record.time = typeThree(static_cast<int>(r), tables) ? 0.0f : 300.0f;
    }
    written = 1;
}

float HighScores::time(int route) const {
    return route >= 0 && static_cast<size_t>(route) < kRoutes ? routes[static_cast<size_t>(route)].time : 0.0f;
}

bool HighScores::better(int route, float lap, const ExeTables& tables) const {
    if (route < 0 || static_cast<size_t>(route) >= kRoutes) return false;
    const float record = routes[static_cast<size_t>(route)].time;
    if (typeThree(route, tables)) return lap > record;
    if (!(lap < record)) return false;
    return !(lap < 1.0f);
}

bool HighScores::keep(int route, const std::wstring& driver, float lap, int32_t car, const ExeTables& tables) {
    if (!better(route, lap, tables)) return false;
    RouteRecord record;
    for (size_t i = 0; i < driver.size() && i < 15 && driver[i] != 0; ++i) {
        const auto c = static_cast<uint16_t>(driver[i]);
        record.name[i * 2] = static_cast<uint8_t>(c & 0xFF);
        record.name[i * 2 + 1] = static_cast<uint8_t>(c >> 8);
    }
    record.time = lap;
    record.car = car;
    routes[static_cast<size_t>(route)] = record;
    written = 0;
    return true;
}

bool HighScores::read(const std::vector<uint8_t>& data) {
    if (data.size() < kFileSize) return false;
    std::memcpy(&id, data.data(), 8);
    for (size_t r = 0; r < kRoutes; ++r) {
        const uint8_t* at = data.data() + 8 + r * 0x28;
        RouteRecord& record = routes[r];
        std::memcpy(record.name.data(), at, 0x20);
        std::memcpy(&record.time, at + 0x20, 4);
        std::memcpy(&record.car, at + 0x24, 4);
    }
    written = 1;
    return true;
}

std::vector<uint8_t> HighScores::write() {
    std::vector<uint8_t> data(kFileSize);
    std::memcpy(data.data(), &id, 8);
    for (size_t r = 0; r < kRoutes; ++r) {
        uint8_t* at = data.data() + 8 + r * 0x28;
        const RouteRecord& record = routes[r];
        std::memcpy(at, record.name.data(), 0x20);
        std::memcpy(at + 0x20, &record.time, 4);
        std::memcpy(at + 0x24, &record.car, 4);
    }
    written = 1;
    return data;
}

}  // namespace oj
