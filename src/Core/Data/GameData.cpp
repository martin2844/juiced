#include "Core/Data/GameData.h"

#include "Core/Career/CareerEvents.h"
#include "Core/Career/CareerInit.h"
#include "Core/Base/Log.h"
#include "Core/Saves/Profile.h"
#include "Core/Race/RaceSession.h"
#include "Core/Base/Script.h"
#include "Core/Base/Text.h"

#include <optional>
#include <string_view>
#include <utility>

namespace oj {

namespace {

std::optional<std::string> scriptText(const GameFiles& files, const char* name) {
    const auto data = files.read(std::filesystem::path("scripts") / name);
    if (!data) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(data->data()), data->size());
}

}  // namespace

GameData::GameData(std::filesystem::path gameFolder) : m_files(std::move(gameFolder)) {}

bool GameData::load(CrtRandom& random, std::string* error) {
    tables = ExeTables::stock();  // built in: no Juiced.exe is read
    const auto cardefs = scriptText(m_files, "cardefs.txt");
    if (!cardefs) {
        if (error) *error = "no scripts\\cardefs.txt";
        return false;
    }
    const uint64_t before = random.draws();
    defs.load(*cardefs, tables, random);
    log::info("game data: cardefs.txt drew {} rand() values", random.draws() - before);
    for (const char* file : PartCatalogue::kFiles) {
        if (const auto text = scriptText(m_files, file)) {
            catalogue.load(*text, tables);
        }
    }
    dynamics = std::make_unique<DynamicsStore>(m_files);
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    // The save system (0x0040A510) reads the arcade cars.
    if (const auto text = scriptText(m_files, "arcadecollection.txt")) {
        arcadeCars = loadCollection(*text, making);
    }
    career = std::make_unique<Career>();
    const uint64_t beforeCareer = random.draws();
    newCareer(random, std::string());
    log::info("game data: the start career drew {} rand() values", random.draws() - beforeCareer);
    if (const auto text = scriptText(m_files, "ArcadeRaces.txt")) {
        arcadeRaces = *text;
        arcade.load(arcadeRaces, tables);  // 0x0042A960
    }
    // 0x0042A640 (the game object's +0x84, then its +0x88).
    if (const auto text = scriptText(m_files, "ArcadeUnlock.txt")) {
        arcadeGroups.load(*text, tables);
        arcadeGroups2.load(*text, tables);
    }
    // The records object (0x0040A0E0, 0x1128 bytes) takes an id of four rand() values as a driver's.
    highScores.reset(tables);
    highScores.id = save::newProfileId(random);
    // The second save system object (0x0040A510 again: the second player's arcade cars) and its start driver
    // (0x0040AB80, an id from rand()).
    if (const auto text = scriptText(m_files, "arcadecollection.txt")) {
        arcadeCars2 = loadCollection(*text, making);
    }
    save::newProfileId(random);
    // 0x0040D0C0: the race to start at first, a custom race (mode 1) of type 5 on Angel SuperSpeedway's Speedway_r1,
    // heat 0, 99999 laps, automatic, one player, no opponents, not a replay's, dry (1), at night (4); its +0x64 1
    // (written, never read). The custom race's group 6 ([game + 0x84]+0x784).
    race.mode = RaceMode::Custom;
    race.type = RaceType::Circuit;
    race.heat = 0;
    race.track = "Angel SuperSpeedway";
    race.route = "Speedway_r1";
    race.laps = 99999;
    race.gearbox = 1;
    race.players = 1;
    race.opponents = 0;
    race.weather = 1;
    race.timeOfDay = 4;
    arcadeGroups.group = 6;
    // The music (0x0040D200): the bank's songs, all of them on in the settings of a new driver.
    if (const auto cfg = m_files.read(std::filesystem::path("audio") / "music" / "music.cfg")) {
        const std::string text(reinterpret_cast<const char*>(cfg->data()), cfg->size());
        for (const ScriptLine& line : parseScript(text)) {
            if (_stricmp(line.keyword.c_str(), "[BeginSS]") == 0) {
                ++songCount;
                songNames.emplace_back();
            } else if (_stricmp(line.keyword.c_str(), "displayname") == 0 && !songNames.empty()) {
                songNames.back() = widenSigned(line.word(0));
            }
        }
    }
    shuffleMusic(save::Settings::defaults(0), random);
    applyModUnlocks();
    m_loaded = true;
    return true;
}

void GameData::applyModUnlocks() {
    const auto text = scriptText(m_files, "mod_unlocks.txt");
    if (!text) return;
    for (const ScriptLine& line : parseScript(*text)) {
        if (_stricmp(line.keyword.c_str(), "Car") != 0) continue;
        const int id = tables.carModelId(line.word(0));
        const CarDef* def = defs.byId(id);
        // carModelId preserves the retail fallback to RX-7 for unknown names.
        if (id <= 0 || def == nullptr || _stricmp(def->name.c_str(), line.word(0).c_str()) != 0) continue;
        arcade.openCar(id);
        for (size_t g = 0; g + 1 < arcadeGroups.ids.size(); ++g) {
            const auto& group = arcadeGroups.ids[g];
            for (size_t i = 0; i < group.size() && group[i] != 0; ++i) {
                if (group[i] == id) {
                    arcadeGroups.group = static_cast<int32_t>(g);
                    arcadeGroups.index = static_cast<int32_t>(i);
                }
            }
        }
    }
}

void GameData::newCareer(CrtRandom& random, const std::string& driverName) {
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    if (settings) context.logo = settings->logo();
    career->head = 0;  // [[game + 0x7C]], cleared before a new career
    oj::newCareer(*career, context, driverName);
}

void GameData::setupRace(CrtRandom& random) {
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    oj::setupRace(*career, context);
}

void GameData::enterCareer(CrtRandom& random, const std::string& driverName) {
    // 0x00435434: the race's mode 2 (its type and laps stay as they are until 0x004A3CC0); the career's screen 2.
    race.mode = RaceMode::Career;
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    if (settings) context.logo = settings->logo();
    oj::enterCareer(*career, context, driverName);
    careerScreen = CareerScreen::Character;
}

void GameData::newGameCareer(CrtRandom& random, const std::string& driverName) {
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    if (settings) context.logo = settings->logo();
    oj::enterCareer(*career, context, driverName);
}

void GameData::applyCheat(CrtRandom& random, Cheat cheat) {
    if (cheat == Cheat::Arcade) arcade.openAll();
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    oj::applyCheat(*career, cheat, context);
}

namespace {

// 0x0042B720: the race's paint (+0xA4 ..) on the car; the race's wheel (+0xBC, not -1) with its colour (+0xB8: none
// when its red and green bytes are 1); the neon of the race's colour (+0xC0; none when its red and green bytes are 1);
// the race's decal (+0xC4, not -1) with its colour (+0xC8).
void paintArcadeCar(Car& car, const Arcade& arcade, size_t race) {
    const auto byte = [&](size_t at) { return static_cast<uint8_t>(arcade.i32(at & ~size_t{3}) >> ((at & 3) * 8)); };
    car.base = static_cast<uint32_t>(arcade.i32(race + Arcade::kRaceBase));
    car.metallic = static_cast<uint32_t>(arcade.i32(race + Arcade::kRaceMetallic));
    car.pearl = static_cast<uint32_t>(arcade.i32(race + Arcade::kRacePearl));
    car.metallicAmount = static_cast<uint32_t>(arcade.i32(race + Arcade::kRaceMetallicAmount));
    car.pearlAmount = static_cast<uint32_t>(arcade.i32(race + Arcade::kRacePearlAmount));
    if (arcade.i32(race + Arcade::kRaceWheel) != -1) {
        car.mods[kWheel] = static_cast<uint8_t>(arcade.i32(race + Arcade::kRaceWheel));
        if (byte(race + Arcade::kRaceWheelColour + 2) == 1 && byte(race + Arcade::kRaceWheelColour + 1) == 1) {
            car.wheelsColoured = 0;
        } else {
            car.wheelColour = static_cast<uint32_t>(arcade.i32(race + Arcade::kRaceWheelColour));
            car.wheelsColoured = 1;
        }
    }
    if (byte(race + Arcade::kRaceNeonColour + 2) == 1 && byte(race + Arcade::kRaceNeonColour + 1) == 1) {
        car.body[9] = 0xFF;
    } else {
        car.body[9] = 1;
        car.neon = static_cast<uint32_t>(arcade.i32(race + Arcade::kRaceNeonColour));
    }
    if (arcade.i32(race + Arcade::kRaceDecal) != -1) {
        car.body[8] = static_cast<uint8_t>(arcade.i32(race + Arcade::kRaceDecal));
        car.decalColour = static_cast<uint32_t>(arcade.i32(race + Arcade::kRaceDecalColour));
    }
}

// The automatic modification of an arcade entry's level: 4 as a prototype of any level, another of `level`.
void modifyArcadeCar(Car& car, int32_t entryLevel, int32_t level, CarMaking& making) {
    if (entryLevel == 0) return;
    autoModify(car, entryLevel == 4, entryLevel == 4 ? -1 : level, making);
}

}  // namespace

void GameData::makeArcadeCars(CrtRandom& random) {
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    arcadeRaceCars.clear();
    for (int32_t c = 0; c < arcade.count(); ++c) {
        for (int32_t r = 0; r < 5; ++r) {
            const size_t raceAt = Arcade::race(c, r);
            const size_t entry = arcade.playerEntry(raceAt);
            Car car = makeCar(defs.byId(arcade.i32(entry + Arcade::kEntryCar)));
            const int32_t level = arcade.i32(entry + Arcade::kEntryLevel);
            modifyArcadeCar(car, level, level - 1, making);
            rateCar(car, car.def != nullptr && dynamics ? dynamics->of(*car.def) : nullptr, catalogue);
            if (!car.grid) car.grid.emplace();
            paintArcadeCar(car, arcade, raceAt);
            if (car.def != nullptr && !car.def->parts[2].parts.empty()) {
                const auto bonnets = static_cast<int32_t>(car.def->parts[2].parts.size());
                car.body[2] = static_cast<uint8_t>(((r + 1) * (c + 1)) % bonnets);
            }
            arcadeRaceCars.push_back(car);
        }
    }
}

void GameData::makeArcadeRaceCars(CrtRandom& random) {
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    arcadeRaceCars.clear();
    const int32_t c = arcade.i32(Arcade::kChampionshipIndex);
    const int32_t r = arcade.i32(Arcade::kRaceIndex);
    const size_t raceAt = arcade.current();
    for (int32_t k = 0; k < arcade.i32(raceAt + Arcade::kRaceCarCount); ++k) {
        const size_t entry = raceAt + Arcade::kRaceCars + static_cast<size_t>(k) * Arcade::kEntrySize;
        Car car = makeCar(defs.byId(arcade.i32(entry + Arcade::kEntryCar)));
        const size_t paints = defs.paints.size();
        const int32_t level = arcade.i32(entry + Arcade::kEntryLevel);
        modifyArcadeCar(car, level, level, making);
        if (arcade.i32(entry + Arcade::kEntryPlayer) == 0) {
            if (paints != 0) {
                const CarPaint& p = defs.paints[static_cast<size_t>(static_cast<uint32_t>((c + k) * 5 + r) % paints)];
                car.base = p.base;
                car.metallic = p.metallic;
                car.pearl = p.pearl;
                car.metallicAmount = Car::bitsOf(p.metallicAmount);
                car.pearlAmount = Car::bitsOf(p.pearlAmount);
            }
        } else {
            paintArcadeCar(car, arcade, raceAt);
            if (car.def != nullptr && !car.def->parts[2].parts.empty()) {
                const auto bonnets = static_cast<int32_t>(car.def->parts[2].parts.size());
                car.body[2] = static_cast<uint8_t>(((c + 1) * (r + 1)) % bonnets);
            }
        }
        rateCar(car, car.def != nullptr && dynamics ? dynamics->of(*car.def) : nullptr, catalogue);
        if (!car.grid) car.grid.emplace();
        arcadeRaceCars.push_back(car);
    }
}

void GameData::makeDealerCars(CrtRandom& random) {
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    oj::makeDealerCars(*career, context);
}

int32_t GameData::dealerTake(CrtRandom& random, int32_t place) {
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    return oj::dealerTake(*career, place, context);
}

void GameData::startRace(CrtRandom& random, int32_t automaticGearbox) {
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    // [0x0075EF8C], the network game (Core/Network/NetSession.h), is none in the career: a sprint has its three heats.
    oj::startRace(*career, context, race, automaticGearbox, 0);
}

void GameData::endSession(CrtRandom& random) {
    // 0x00426490: the route of the race's track (+0x10) and route (+0x30); its record (0 for none).
    const float routeRecord = highScores.time(tables.routeIndex(race.track, race.route));
    finishSession(*career, race.type, routeRecord, random);
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    sessionResults(*career, context, race);
}

void GameData::phoneEvent(CrtRandom& random, int crew, int month, int day, bool host) {
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    oj::phoneEvent(*career, context, crew, month, day, host);
}

void GameData::challengeEvent(CrtRandom& random, int crew, int month, int day, int challenge) {
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    oj::challengeEvent(*career, context, crew, month, day, challenge);
}

void GameData::nextDay(CrtRandom& random) {
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    oj::nextDay(*career, context);
}

void GameData::topUpCalendar(CrtRandom& random) {
    CarMaking making{defs, catalogue, random, autoModifyCounter};
    CareerContext context{tables, making, *dynamics, m_files, currentCrewmate};
    oj::topUpCalendar(*career, context);
}

void GameData::shuffleMusic(const save::Settings& driverSettings, CrtRandom& random) {
    playlist.clear();
    for (uint32_t song = 0; song < songCount && song < 128; ++song) {
        const uint32_t bits = driverSettings.u32(save::Settings::kSongs + (song / 32) * 4);
        if (bits & (1u << (song % 32))) playlist.push_back(song);
    }
    for (size_t i = 1; i < playlist.size(); ++i) {
        const size_t j = static_cast<size_t>(random.next()) % (i + 1);
        std::swap(playlist[i], playlist[j]);
    }
}

std::string crewName8(const std::wstring& name) {
    std::string out;
    for (wchar_t c : name) {
        const auto low = static_cast<char>(c & 0xFF);
        if (low == 0) break;
        out.push_back(low);
    }
    return out;
}

}  // namespace oj
