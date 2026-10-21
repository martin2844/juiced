#include "Core/Career/CareerFiles.h"

#include "Core/Physics/Dynamics.h"
#include "Core/Base/RawFields.h"
#include "Core/Base/Script.h"

#include <cstdlib>
#include <cstring>
#include <string>

namespace oj {

namespace {

bool same(const std::string& a, const char* b) {
    return _stricmp(a.c_str(), b) == 0;
}

double number(const ScriptLine& line, size_t i) {
    return i < line.words.size() ? std::atof(line.words[i].c_str()) : 0.0;
}

// The handlers write at offsets of the object as the stock game does, also past a table's end when a file has more
// lines than slots (stock problem: the next fields take the values; see RawFields.h).
using raw::put;
using raw::putText;

template <class Object>
void increment(Object& object, size_t offset) {
    put(object, offset, raw::get<int32_t>(object, offset) + 1);
}

// The crew a WillAttendAt word names (0x004A92E0); -1 for another word.
int attendIndex(const std::string& word) {
    static const char* const kCrews[] = {"Players Crew", "Urban Maulerz", "Angel West Brotherhood", "Vixens",
                                         "The Wild Cats", "Omega Tau", "Public Chaos", "The Lordz", "Legion"};
    for (int i = 0; i < 9; ++i) {
        if (same(word, kCrews[i])) return i;
    }
    return -1;
}

uint8_t byteOf(const ScriptLine& line, size_t i) {
    return static_cast<uint8_t>(line.integer(i));
}

}  // namespace

// The lines of [BeginCrew] <the crew's id> (0x004A7A10 .. 0x004A87E0); the reader's counters (one reading per crew)
// number the repeated keys.
void readCrewFile(std::string_view text, Crew& crew, const ExeTables& tables) {
    int current = -1;  // 0x007187A4
    size_t names = 0, shortNames = 0, respect = 0, cruise = 0, circuit = 0, p2p = 0, sprint = 0, illegal = 0;
    for (const ScriptLine& line : parseScript(text)) {
        const std::string& k = line.keyword;
        if (same(k, "[BeginCrew]")) {
            current = line.integer(0);
            continue;
        }
        if (same(k, "[EndCrew]")) {
            current = -1;
            continue;
        }
        if (crew.id != current) {
            continue;
        }
        if (same(k, "CarColours")) {
            // r g b, r g b amount, r g b amount: the colours as bytes B G R with alpha 0x80.
            put(crew, 0x1A6, byteOf(line, 0));
            put(crew, 0x1A5, byteOf(line, 1));
            put(crew, 0x1A4, byteOf(line, 2));
            put(crew, 0x1A7, uint8_t{0x80});
            put(crew, 0x1AA, byteOf(line, 3));
            put(crew, 0x1A9, byteOf(line, 4));
            put(crew, 0x1A8, byteOf(line, 5));
            put(crew, 0x1AB, uint8_t{0x80});
            put(crew, 0x1B0, line.real(6));
            put(crew, 0x1AE, byteOf(line, 7));
            put(crew, 0x1AD, byteOf(line, 8));
            put(crew, 0x1AC, byteOf(line, 9));
            put(crew, 0x1AF, uint8_t{0x80});
            put(crew, 0x1B4, line.real(10));
        } else if (same(k, "WheelColor")) {
            crew.wheelColour = line.integer(0);
        } else if (same(k, "Wheels")) {
            crew.wheels = line.integer(0);
        } else if (same(k, "NeonColour")) {
            crew.neonColour = line.integer(0);
        } else if (same(k, "EntranceFee")) {
            crew.entranceFee = line.integer(0);
        } else if (same(k, "PrizeMoney")) {
            for (size_t i = 0; i < 3; ++i) crew.prizeMoney[i] = line.integer(i);
        } else if (same(k, "DriverName")) {
            putText(crew, 0x7A + names * 25, line.word(0));
            put(crew, 0x144 + names * 0x18, line.real(1));
            put(crew, 0x14C + names * 0x18, line.real(2));
            ++names;
        } else if (same(k, "ShortDriverName")) {
            putText(crew, 0xDE + shortNames * 25, line.word(0));
            ++shortNames;
        } else if (same(k, "WillAttendAt")) {
            // Stock problem: a word that names no crew gives -1 and sets the field before the table (+0x260).
            const int index = attendIndex(line.word(0));
            put(crew, static_cast<size_t>(0x264 + index * 4), int32_t{1});
        } else if (same(k, "RespectFactor")) {
            put(crew, 0x04 + respect * 4, productToFloat(number(line, 0), 0.01f));
            ++respect;
        } else if (same(k, "Circuit")) {
            put(crew, 0x1C4 + circuit * 4, int32_t{tables.routeIndex(line.word(0), line.word(1))});
            put(crew, 0x1E4 + circuit * 4, line.real(2));
            put(crew, 0x204 + circuit * 4, int32_t{line.integer(3)});
            ++circuit;
            increment(crew, 0x228);
        } else if (same(k, "P2P") || same(k, "Cruise") || same(k, "Sprint") || same(k, "Illegal")) {
            // Each has a slot of route and km (P2P two); stock problem: a further line writes over the fields after
            // the slot (the km and the count of the slot before).
            size_t base = 0x22C, kmBase = 0x234, count = 0x23C;
            size_t* n = &p2p;
            if (same(k, "Cruise")) {
                base = 0x240, kmBase = 0x244, count = 0x248, n = &cruise;
            } else if (same(k, "Sprint")) {
                base = 0x24C, kmBase = 0x250, count = 0x254, n = &sprint;
            } else if (same(k, "Illegal")) {
                base = 0x258, kmBase = 0x25C, count = 0x260, n = &illegal;
            }
            put(crew, base + *n * 4, int32_t{tables.routeIndex(line.word(0), line.word(1))});
            put(crew, kmBase + *n * 4, line.real(2));
            ++*n;
            increment(crew, count);
        } else if (same(k, "ProbMix")) {
            crew.probMix = productToFloat(number(line, 0), 0.01f);
        } else if (same(k, "ProbCircuit")) {
            crew.probCircuit = productToFloat(number(line, 0), 0.01f);
        } else if (same(k, "ProbSprint")) {
            crew.probSprint = productToFloat(number(line, 0), 0.01f);
        } else if (same(k, "ProbP2P")) {
            crew.probP2P = productToFloat(number(line, 0), 0.01f);
        } else if (same(k, "ProbCruising")) {
            crew.probCruising = productToFloat(number(line, 0), 0.01f);
        } else if (same(k, "ProbTerritory")) {
            crew.probTerritory = productToFloat(number(line, 0), 0.01f);
        } else if (same(k, "ProbMake")) {
            crew.probMake = productToFloat(number(line, 0), 0.01f);
        } else if (same(k, "ProbModel")) {
            crew.probModel = productToFloat(number(line, 0), 0.01f);
        } else if (same(k, "CarPref")) {
            // The word is looked up as a region and as a make (0 when not found).
            const std::string& word = line.word(0);
            crew.preferredRegion = 0;
            for (size_t i = 0; i < tables.regions.size(); ++i) {
                if (same(word, tables.regions[i].c_str())) {
                    crew.preferredRegion = static_cast<int32_t>(i);
                    break;
                }
            }
            crew.preferredMake = 0;
            for (size_t i = 0; i < tables.manufacturers.size(); ++i) {
                if (same(word, tables.manufacturers[i].c_str())) {
                    crew.preferredMake = static_cast<int32_t>(i);
                    break;
                }
            }
        }
    }
}

// 0x004A9FE0 .. 0x004AA370.
void readCrewmateFile(std::string_view text, Crewmate& mate, int& currentMate) {
    for (const ScriptLine& line : parseScript(text)) {
        const std::string& k = line.keyword;
        if (same(k, "[BeginCrewmate]")) {
            currentMate = line.integer(0);
        } else if (same(k, "[EndCrewmate]")) {
            currentMate = -1;
        } else if (same(k, "PhoneNumber")) {
            // Stock problem: no check of the crewmate, so every crewmate takes the file's last number.
            mate.phoneNumber = line.integer(0);
        } else if (mate.number != currentMate) {
            continue;
        } else if (same(k, "Name")) {
            putText(mate, 0x08, line.word(0));
        } else if (same(k, "ShortName")) {
            putText(mate, 0x48, line.word(0));
        } else if (same(k, "JoinThreshold")) {
            mate.joinThreshold = line.integer(0);
        } else if (same(k, "JoinCrewIdx")) {
            mate.joinCrew = line.integer(0);
        } else if (same(k, "DrivingAbility")) {
            mate.drivingAbility = line.real(0);
        } else if (same(k, "DrivingLimit")) {
            mate.drivingLimit = 100.0f;  // stock problem: the value of the file is read and not used
        } else if (same(k, "Recover")) {
            mate.recover = line.real(0);
        } else if (same(k, "RecoverLimit")) {
            mate.recoverLimit = 100.0f;  // stock problem: as DrivingLimit
        } else if (same(k, "LearnRate")) {
            mate.learnRate = line.real(0);
        } else if (same(k, "ForgetRate")) {
            mate.forgetRate = line.real(0);
        }
    }
}

Flyers readFlyersFile(std::string_view text) {
    Flyers flyers;
    int crew = -1;  // 0x00728C48
    const auto colour = [](const ScriptLine& line) {
        const auto r = static_cast<uint32_t>(byteOf(line, 0));
        const auto g = static_cast<uint32_t>(byteOf(line, 1));
        return 0xFF000000u | r << 16 | g << 8 | static_cast<uint32_t>(byteOf(line, 2));
    };
    for (const ScriptLine& line : parseScript(text)) {
        const std::string& k = line.keyword;
        // Stock problem: a FontColour or Logo before the first Crew writes into the player's colours, more than 21
        // player's colours into the first crew, and a logo of 32 characters or more into the next crew; not kept.
        if (same(k, "Crew")) {
            ++crew;
        } else if (same(k, "FontColour")) {
            if (crew >= 0 && crew < 9) flyers.crews[static_cast<size_t>(crew)].colour = colour(line);
        } else if (same(k, "PlayerFontColour")) {
            if (flyers.playerCount < 21) flyers.playerColours[flyers.playerCount++] = colour(line);
        } else if (same(k, "Logo")) {
            if (crew >= 0 && crew < 9) flyers.crews[static_cast<size_t>(crew)].logo = line.word(0);
        }
    }
    return flyers;
}

// 0x004A8890 .. 0x004A8900.
void readChallengeFile(std::string_view text, std::array<Crew, 9>& crews, const ExeTables& tables) {
    Crew* crew = nullptr;
    size_t count = 0;
    for (const ScriptLine& line : parseScript(text)) {
        const std::string& k = line.keyword;
        if (same(k, "[BeginCrew]")) {
            const int n = line.integer(0);
            crew = n >= 0 && n < 9 ? &crews[static_cast<size_t>(n)] : nullptr;
            count = 0;
        } else if (same(k, "[EndCrew]")) {
            crew = nullptr;
        } else if (same(k, "Challenge") && crew != nullptr) {
            const size_t at = 0x2DC + count * 0x1C;
            put(*crew, at + 0x14, int32_t{0});
            int32_t type = 4;
            for (size_t i = 0; i < tables.challengeTypes.size(); ++i) {
                if (same(line.word(0), tables.challengeTypes[i].c_str())) {
                    type = static_cast<int32_t>(i);
                    break;
                }
            }
            put(*crew, at + 0x00, type);
            put(*crew, at + 0x04, int32_t{line.integer(1)});
            put(*crew, at + 0x08, int32_t{tables.carModelId(line.word(2))});
            put(*crew, at + 0x0C, int32_t{line.integer(3)});
            put(*crew, at + 0x10, int32_t{line.integer(4)});
            ++count;
        }
    }
}

}  // namespace oj
