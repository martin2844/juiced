// Unit tests: the fixed build's bounds of CMSCrews.txt and CMSCrewChallenges.txt (Core/Career/CareerFiles.cpp).

#include "Framework/Test.h"

#include "Core/Career/Career.h"
#include "Core/Career/CareerFiles.h"
#include "Core/Data/ExeTables.h"

#include <array>
#include <cstring>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// The stock writes a further line past its table (a second Cruise over the cruise km and count, a fifth DriverName
// over the first short name and the colours, a WillAttendAt of no crew into the Illegal count before the table); the
// fixed build keeps each table to its slots, cuts a name to its field and passes a word of no crew over.
OJ_UNIT_TEST(CrewFileBounds, TablesKeepTheirSlots) {
    const std::string text =
        "[BeginCrew] 3\n"
        "ShortDriverName ABCDEFGHIJKLMNOPQRSTUVWXYZ0123\n"
        "DriverName A 1 2\nDriverName B 1 2\nDriverName C 1 2\nDriverName D 1 2\nDriverName E 7 8\n"
        "Cruise Downtown r1 5\nCruise Downtown r2 7\n"
        "WillAttendAt Nobody\n"
        "[EndCrew]\n";
    oj::Crew crew;
    crew.id = 3;
    oj::readCrewFile(text, crew, oj::ExeTables::stock());
    check(crew.cruiseCount == 1 && crew.cruiseKm == 5.0f, "crews (fixed): a second Cruise passed over");
    check(std::strcmp(crew.memberNames[3].data(), "D") == 0 && crew.baseColour == 0,
          "crews (fixed): a fifth DriverName passed over (the colours kept)");
    check(crew.memberShortNames[0][24] == '\0' &&
              std::strncmp(crew.memberShortNames[0].data(), "ABCDEFGHIJKLMNOPQRSTUVWX", 24) == 0 &&
              crew.memberShortNames[1][0] == '\0',
          "crews (fixed): a name cut to its 25-byte field");
    check(crew.illegalCount == 0, "crews (fixed): WillAttendAt of no crew passed over");
}

// A fourth challenge: the stock writes it over the fields after the three (+0x330: rams, money, the race cars).
OJ_UNIT_TEST(CrewFileBounds, ThreeChallenges) {
    std::string text = "[BeginCrew] 2\n";
    for (int i = 0; i < 4; ++i) text += "Challenge LapTime " + std::to_string(i + 10) + " none 1 2\n";
    text += "[EndCrew]\n";
    std::array<oj::Crew, 9> crews{};
    oj::readChallengeFile(text, crews, oj::ExeTables::stock());
    check(crews[2].challenges[2].value == 12 && crews[2].rams == 0.0f && crews[2].money == 0.0f &&
              crews[2].raceCars[2] == 0,
          "challenges (fixed): three kept, a fourth not written past the table");
}

// CMSCrewmates.txt's PhoneNumber: the stock read it for every crewmate (the file's last one); the fixed build reads the
// crewmate's own.
OJ_UNIT_TEST(CrewFileBounds, CrewmatePhoneNumber) {
    const std::string text =
        "[BeginCrewmate] 0\nPhoneNumber 111\n[EndCrewmate]\n[BeginCrewmate] 1\nPhoneNumber 222\n[EndCrewmate]\n";
    oj::Crewmate first;
    first.number = 0;
    int current = -1;
    oj::readCrewmateFile(text, first, current);
    oj::Crewmate second;
    second.number = 1;
    current = -1;
    oj::readCrewmateFile(text, second, current);
    check(first.phoneNumber == 111 && second.phoneNumber == 222, "each crewmate its own phone number");
}
