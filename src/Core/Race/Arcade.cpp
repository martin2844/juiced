#include "Core/Race/Arcade.h"

#include "Core/Base/Script.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace oj {

namespace {

bool same(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

// The route of a route name alone (0x004264E0), 0 when none.
int32_t routeByName(const ExeTables& tables, const std::string& name) {
    for (size_t i = 0; i < tables.routes.size(); ++i) {
        if (_stricmp(tables.routes[i].name.c_str(), name.c_str()) == 0) return static_cast<int32_t>(i);
    }
    return 0;
}

// atol of a word; a line short of it marks the reading's error (+0x24) and gives 0.
int32_t number(const ScriptLine& line, size_t i) {
    return i < line.words.size() ? static_cast<int32_t>(std::atol(line.words[i].c_str())) : 0;
}

// atof of a word, kept as a float (the main thread's x87 unit rounds to single precision); 0 when the line is short.
float real(const ScriptLine& line, size_t i) {
    return i < line.words.size() ? static_cast<float>(std::atof(line.words[i].c_str())) : 0.0f;
}

}  // namespace

int32_t Arcade::i32(size_t offset) const {
    int32_t v = 0;
    if (offset + 4 <= kSize) std::memcpy(&v, m_bytes.data() + offset, 4);
    return v;
}

float Arcade::f32(size_t offset) const {
    float v = 0.0f;
    if (offset + 4 <= kSize) std::memcpy(&v, m_bytes.data() + offset, 4);
    return v;
}

void Arcade::put(size_t offset, int32_t value) {
    if (offset + 4 <= kSize) std::memcpy(m_bytes.data() + offset, &value, 4);
}

void Arcade::load(std::string_view arcadeRaces, const ExeTables& tables) {
    m_bytes.fill(0);
    put(kCount, 0);
    put(kResultWaits, 0);
    put(kExtreme, 0);
    put(kDevelopers, 0);
    for (size_t i = 0; i < 17 * 20; ++i) {
        put(kTracks + i * 4, 0);
        put(kBonusTracks + i * 4, 0);
    }
    // The reader (0x006EF7B0): no race; the race counter -1, championship 0, the count 1.
    size_t race = 0;
    bool inRace = false;
    put(kRaceIndex, -1);
    put(kChampionshipIndex, 0);
    put(kCount, 1);
    int32_t unlockChampionship = 0;
    int32_t tracks = 0;
    int32_t bonusTracks = 0;
    const auto putByte = [&](size_t offset, int32_t value) {
        if (offset < kSize) m_bytes[offset] = static_cast<uint8_t>(value);
    };
    const auto putFloat = [&](size_t offset, float value) {
        int32_t bits = 0;
        std::memcpy(&bits, &value, 4);
        put(offset, bits);
    };
    // A colour of three words r g b into the bytes +2, +1, +0 and 0x80 at +3.
    const auto colour = [&](const ScriptLine& line, size_t first, size_t at) {
        putByte(race + at + 2, number(line, first));
        putByte(race + at + 1, number(line, first + 1));
        putByte(race + at, number(line, first + 2));
        putByte(race + at + 3, 0x80);
    };

    for (const ScriptLine& line : parseScript(arcadeRaces)) {
        const std::string& k = line.keyword;
        if (same(k, "[Race]")) {
            // 0x0042BA70
            put(kRaceIndex, i32(kRaceIndex) + 1);
            if (i32(kRaceIndex) == 5) {
                put(kChampionshipIndex, i32(kChampionshipIndex) + 1);
                put(kCount, i32(kCount) + 1);
                put(kRaceIndex, 0);
            }
            race = current();
            inRace = true;
            for (size_t i = 0; i < kRace; i += 4) put(race + i, 0);
            put(race + kRaceBase, -1);
            put(race + kRaceMetallic, -1);
            put(race + kRacePearl, -1);
            put(race + kRaceDecalColour, static_cast<int32_t>(0xFF000000));
            put(race + kRaceNeonColour, static_cast<int32_t>(0xFFFF0000));
            put(race + kRaceWheelColour, static_cast<int32_t>(0xFF000000));
            put(race + kRaceWheel, 0);
            put(race + kRaceDecal, 0);
            put(race + kRaceExtreme, 0);
            put(race + kRaceDevelopers, 0);
        } else if (same(k, "[EndRace]")) {
            // 0x0042C430: no race. Stock problem: the race's keywords after it write through address 0; ignored here.
            inRace = false;
        } else if (same(k, "[UnlockTracks]")) {
            // 0x0042C440
            unlockChampionship = number(line, 0) - 1;
            tracks = 0;
            bonusTracks = 0;
        } else if (same(k, "Track")) {
            // 0x0042C490. Stock problem: more than 20 run into the next championship's list.
            put(kTracks + static_cast<size_t>(tracks + unlockChampionship * 20) * 4, routeByName(tables, line.word(0)));
            ++tracks;
        } else if (same(k, "BonusTrack")) {
            // 0x0042C4D0
            put(kBonusTracks + static_cast<size_t>(bonusTracks + unlockChampionship * 20) * 4,
                routeByName(tables, line.word(0)));
            ++bonusTracks;
        } else if (!inRace) {
            continue;
        } else if (same(k, "Type")) {
            // 0x0042BBF0: Circuit, P2P, Sprint; the challenges ShowOff, Speed, PerfectLap, TimedLap.
            const std::string& w = line.word(0);
            const auto kind = [&](EventKind k) { put(race + kRaceKind, static_cast<int32_t>(k)); };
            const auto challenge = [&](ChallengeType c) {
                put(race + kRaceKind, static_cast<int32_t>(EventKind::Challenge));
                put(race + kRaceChallenge, static_cast<int32_t>(c));
            };
            if (same(w, "Circuit")) kind(EventKind::Circuit);
            if (same(w, "P2P")) kind(EventKind::PointToPoint);
            if (same(w, "Sprint")) kind(EventKind::Sprint);
            if (same(w, "ShowOff")) challenge(ChallengeType::Showoff);
            if (same(w, "Speed")) challenge(ChallengeType::Speed);
            if (same(w, "PerfectLap")) challenge(ChallengeType::PerfectLap);
            if (same(w, "TimedLap")) challenge(ChallengeType::LapTime);
        } else if (same(k, "ExtremeNitro")) {
            put(race + kRaceExtreme, 1);  // 0x0042BBD0
        } else if (same(k, "DevChallenge")) {
            put(race + kRaceDevelopers, 1);  // 0x0042BBE0
        } else if (same(k, "Location")) {
            put(race + kRaceRoute, routeByName(tables, line.word(0)));  // 0x0042BD30
        } else if (same(k, "NumLaps")) {
            put(race + kRaceLaps, number(line, 0));  // 0x0042C120
        } else if (same(k, "Car")) {
            // 0x0042C170: an entry of 5 dwords from +0x2C (stock problem: more than 6 run into the race's paint).
            const int32_t count = i32(race + kRaceCarCount);
            const size_t entry = race + kRaceCars + static_cast<size_t>(count) * kEntrySize;
            const bool player = same(line.word(0), "player");
            put(entry + kEntryPlayer, player ? 1 : 0);
            put(entry + kEntryOpponent, player ? 0 : static_cast<int32_t>(std::atoi(line.word(0).c_str())));
            put(entry + kEntryCar, tables.carModelId(line.word(1)));
            put(entry + kEntryLevel, number(line, 2));
            put(entry + kEntryIndex, count);
            put(race + kRaceCarCount, count + 1);
        } else if (same(k, "Time")) {
            // 0x0042C240: Morning 1, Afternoon 2, Evening 3, Night 4, else 0.
            const std::string& w = line.word(0);
            put(race + kRaceTimeOfDay, 0);
            if (same(w, "Morning")) put(race + kRaceTimeOfDay, 1);
            if (same(w, "Afternoon")) put(race + kRaceTimeOfDay, 2);
            if (same(w, "Evening")) put(race + kRaceTimeOfDay, 3);
            if (same(w, "Night")) put(race + kRaceTimeOfDay, 4);
        } else if (same(k, "Weather")) {
            // 0x0042C310: Dry 1, Wet 2, else 0.
            const std::string& w = line.word(0);
            put(race + kRaceWeather, 0);
            if (same(w, "Dry")) put(race + kRaceWeather, 1);
            if (same(w, "Wet")) put(race + kRaceWeather, 2);
        } else if (same(k, "Challenge")) {
            putFloat(race + kRaceChallengeValue, real(line, 0));  // 0x0042C3E0
        } else if (same(k, "AILevel")) {
            putFloat(race + kRaceSkill, real(line, 0) * 0.01f);  // 0x0042C3A0
        } else if (same(k, "CarColour")) {
            // 0x0042BD60: base, metallic, its amount, pearl, its amount.
            colour(line, 0, 0xA4);
            colour(line, 3, 0xA8);
            putFloat(race + kRaceMetallicAmount, real(line, 6));
            colour(line, 7, 0xAC);
            putFloat(race + kRacePearlAmount, real(line, 10));
        } else if (same(k, "CarDecal")) {
            put(race + kRaceDecal, number(line, 0));  // 0x0042BF30
            colour(line, 1, 0xC8);
        } else if (same(k, "CarWheel")) {
            put(race + kRaceWheel, number(line, 0));  // 0x0042BFE0
            colour(line, 1, 0xB8);
        } else if (same(k, "CarNeon")) {
            colour(line, 0, 0xC0);  // 0x0042C090
        }
    }

    // After the file: every race locked and not completed, the first four of championship 0 open, all cars and
    // routes locked.
    const int32_t championships = count();
    for (int32_t c = 0; c < championships; ++c) {
        for (int32_t r = 0; r < 5; ++r) {
            put(Arcade::race(c, r) + kRaceLocked, 1);
            put(Arcade::race(c, r) + kRaceCompleted, 0);
        }
    }
    for (int32_t r = 0; r < 4; ++r) put(Arcade::race(0, r) + kRaceLocked, 0);
    for (size_t i = 0; i < 53; ++i) put(kCarFlags + i * 4, 1);
    for (size_t i = 0; i < 109; ++i) put(kRouteFlags + i * 4, 1);
    openRoutes(0);
    openCars(0);
}

void Arcade::read(const save::ArcadeUnlock& progress) {
    for (size_t c = 0; c < progress.series.size(); ++c) {
        for (size_t r = 0; r < 5; ++r) {
            const size_t at = race(static_cast<int32_t>(c), static_cast<int32_t>(r));
            put(at + kRaceLocked, static_cast<int32_t>(progress.series[c][r].locked));
            put(at + kRaceCompleted, static_cast<int32_t>(progress.series[c][r].completed));
        }
    }
    using save::ArcadeUnlock;
    if (progress.carFlags.size() == ArcadeUnlock::kCarFlagsSize) {
        std::memcpy(m_bytes.data() + kCarFlags, progress.carFlags.data(), ArcadeUnlock::kCarFlagsSize);
    }
    if (progress.routeFlags.size() == ArcadeUnlock::kRouteFlagsSize) {
        std::memcpy(m_bytes.data() + kRouteFlags, progress.routeFlags.data(), ArcadeUnlock::kRouteFlagsSize);
    }
    put(kDevelopers, static_cast<int32_t>(progress.developersOpen));
    put(kExtreme, static_cast<int32_t>(progress.extremeOpen));
}

save::ArcadeUnlock Arcade::progress() const {
    save::ArcadeUnlock p;
    p.series.resize(static_cast<size_t>(count() > 0 ? count() : 0));
    for (size_t c = 0; c < p.series.size(); ++c) {
        for (size_t r = 0; r < 5; ++r) {
            const size_t at = race(static_cast<int32_t>(c), static_cast<int32_t>(r));
            p.series[c][r].locked = static_cast<uint32_t>(i32(at + kRaceLocked));
            p.series[c][r].completed = static_cast<uint32_t>(i32(at + kRaceCompleted));
        }
    }
    p.carFlags.assign(m_bytes.begin() + kCarFlags, m_bytes.begin() + kCarFlags + save::ArcadeUnlock::kCarFlagsSize);
    p.routeFlags.assign(m_bytes.begin() + kRouteFlags,
                        m_bytes.begin() + kRouteFlags + save::ArcadeUnlock::kRouteFlagsSize);
    p.developersOpen = static_cast<uint32_t>(i32(kDevelopers));
    p.extremeOpen = static_cast<uint32_t>(i32(kExtreme));
    return p;
}

bool Arcade::finished() const {
    for (int32_t c = 0; c < count(); ++c) {
        for (int32_t r = 0; r < 5; ++r) {
            const size_t at = race(c, r);
            if (i32(at + kRaceCompleted) == 0 && i32(at + kRaceDevelopers) == 0 && i32(at + kRaceExtreme) == 0) {
                return false;
            }
        }
    }
    return true;
}

void Arcade::openCar(int32_t id) {
    const size_t flag = kCarFlags + static_cast<size_t>(id) * 4;
    if (i32(flag) == 0) return;
    put(flag, 0);
    put(kOpenedCar, id);
    if (id == 0x30) put(kCarFlags + 22 * 4, 0);
    put(kOpenedFlag, 1);
}

size_t Arcade::playerEntry(size_t raceAt) const {
    for (int32_t i = 0; i < i32(raceAt + kRaceCarCount); ++i) {
        const size_t entry = raceAt + kRaceCars + static_cast<size_t>(i) * kEntrySize;
        if (i32(entry + kEntryPlayer) != 0) return entry;
    }
    return raceAt + kRaceCars;
}

void Arcade::openRoutes(int32_t championship) {
    if (championship >= 17) return;
    for (int32_t r = 0; r < 4; ++r) {
        put(kRouteFlags + static_cast<size_t>(i32(race(championship, r) + kRaceRoute)) * 4, 0);
    }
    for (size_t t = 0; t < 20; ++t) {
        const int32_t route = i32(kTracks + (static_cast<size_t>(championship) * 20 + t) * 4);
        if (route != 0) put(kRouteFlags + static_cast<size_t>(route) * 4, 0);
    }
}

void Arcade::openCars(int32_t championship) {
    if (championship >= 17) return;
    for (int32_t r = 0; r < 4; ++r) openCar(i32(playerEntry(race(championship, r)) + kEntryCar));
}

void Arcade::openBonusRoutes(int32_t championship) {
    if (championship >= 17) return;
    for (size_t t = 0; t < 20; ++t) {
        const int32_t route = i32(kBonusTracks + (static_cast<size_t>(championship) * 20 + t) * 4);
        if (route != 0) put(kRouteFlags + static_cast<size_t>(route) * 4, 0);
    }
}

void Arcade::openDevelopers() {
    put(kDevelopers, 1);
    for (int32_t c = 0; c < count(); ++c) {
        if (i32(race(c, 0) + kRaceDevelopers) == 0) continue;
        for (int32_t r = 0; r < 4; ++r) put(race(c, r) + kRaceLocked, 0);
    }
}

void Arcade::openExtreme() {
    put(kExtreme, 1);
    for (int32_t c = 0; c < count(); ++c) {
        if (i32(race(c, 0) + kRaceExtreme) == 0) continue;
        for (int32_t r = 0; r < 4; ++r) put(race(c, r) + kRaceLocked, 0);
    }
}

void Arcade::openAll() {
    for (size_t i = 0; i < 53; ++i) put(kCarFlags + i * 4, 0);
    for (size_t i = 0; i < 109; ++i) put(kRouteFlags + i * 4, 0);
    for (int32_t c = 0; c < count(); ++c) {
        for (int32_t r = 0; r < 5; ++r) put(race(c, r) + kRaceLocked, 0);
    }
    openDevelopers();
    openExtreme();
}

void Arcade::complete(int32_t playerCar) {
    const bool wasFinished = finished();
    put(kResultWaits, 1);
    const int32_t c = i32(kChampionshipIndex);
    if (i32(race(c, 4) + kRaceCompleted) == 0 && i32(kRaceIndex) == 4) {
        put(kSeriesComplete, 1);
        openBonusRoutes(c);
    }
    put(current() + kRaceCompleted, 1);
    if (i32(race(c, 4) + kRaceLocked) != 0) {
        bool firstFour = true;
        for (int32_t r = 0; r < 4; ++r) firstFour = firstFour && i32(race(c, r) + kRaceCompleted) != 0;
        if (firstFour) {
            put(race(c, 4) + kRaceLocked, 0);
            put(kFinalUnlocked, 1);
            if (c < 17) {
                openCar(i32(playerEntry(race(c, 4)) + kEntryCar));
                put(kRouteFlags + static_cast<size_t>(i32(race(c, 4) + kRaceRoute)) * 4, 0);
            }
        }
    }
    if (c < count() - 1 && i32(race(c + 1, 0) + kRaceLocked) != 0 && i32(race(c + 1, 0) + kRaceDevelopers) == 0 &&
        i32(race(c + 1, 0) + kRaceExtreme) == 0) {
        int32_t done = 0;
        for (int32_t r = 0; r < 5; ++r) done += i32(race(c, r) + kRaceCompleted) != 0 ? 1 : 0;
        if (done >= 3) {
            put(kNextSeries, 1);
            openRoutes(c + 1);
            openCars(c + 1);
            for (int32_t r = 0; r < 4; ++r) put(race(c + 1, r) + kRaceLocked, 0);
        }
    }
    if (playerCar >= 0) openCar(playerCar);
    if (!wasFinished && finished()) {
        put(kArcadeComplete, 1);
        openDevelopers();
    }
}

save::ArcadeUnlock newArcadeProgress(std::string_view arcadeRaces, const ExeTables& tables) {
    auto arcade = std::make_unique<Arcade>();
    arcade->load(arcadeRaces, tables);
    return arcade->progress();
}

void ArcadeCarGroups::load(std::string_view text, const ExeTables& tables) {
    *this = ArcadeCarGroups{};
    int32_t reading = -1;
    int32_t place = 0;
    for (const ScriptLine& line : parseScript(text)) {
        if (same(line.keyword, "Level")) {
            ++reading;
            place = 0;
        } else if (same(line.keyword, "Car")) {
            // The stock list is one block of 0x1E0 values: a place past 60 runs into the next group.
            const int32_t at = reading * 60 + place;
            if (at >= 0 && at < 8 * 60) {
                ids[static_cast<size_t>(at / 60)][static_cast<size_t>(at % 60)] = tables.carModelId(line.word(0));
            }
            ++place;
        }
    }
}

int32_t ArcadeCarGroups::current() const {
    const int32_t at = group * 60 + index;
    if (at < 0 || at >= 8 * 60) return 0;
    return ids[static_cast<size_t>(at / 60)][static_cast<size_t>(at % 60)];
}

void ArcadeCarGroups::next() {
    ++index;
    if (current() == 0) index = 0;
}

void ArcadeCarGroups::previous() {
    if (--index >= 0) return;
    for (int32_t k = 59; k >= 0; --k) {
        if (group >= 0 && group < 8 && ids[static_cast<size_t>(group)][static_cast<size_t>(k)] != 0) {
            index = k;
            return;
        }
    }
}

bool carLocked(const Arcade& arcade, int32_t id) {
    if (id < 0 || id >= 53) return false;
    return arcade.i32(Arcade::kCarFlags + static_cast<size_t>(id) * 4) != 0;
}

bool routeLocked(const Arcade& arcade, const ExeTables& tables, const std::string& fileName) {
    const std::string name = fileName.size() >= 4 ? fileName.substr(0, fileName.size() - 4) : std::string();
    return arcade.i32(Arcade::kRouteFlags + static_cast<size_t>(routeByName(tables, name)) * 4) != 0;
}

bool locationOpen(const Arcade& arcade, const ExeTables& tables, int32_t location, RouteType type) {
    for (size_t i = 0; i < tables.routes.size() && i < 109; ++i) {
        const ExeTables::Route& r = tables.routes[i];
        if (r.location != location || arcade.i32(Arcade::kRouteFlags + i * 4) != 0) continue;
        if (type == RouteType::Any ? (r.type == RouteType::Circuit || r.type == RouteType::PointToPoint)
                                   : r.type == type) {
            return true;
        }
    }
    return false;
}

}  // namespace oj
