// Unit tests: RaceStart.

#include "Framework/Test.h"

#include "Core/Race/RaceStart.h"
#include "Core/Race/Racer.h"

#include <string>

namespace {

using oj::test::check;

}  // namespace

// The count before the start: from 6 s in steps of 0.7 s the seconds 3, 2, 1 sound, then the start; in steps
// of 0.25 s it lands on 0 and the start never comes (a stock problem).
OJ_UNIT_TEST(RaceStart, Count) {
    {
        auto count = [](float step) {
            oj::RaceStart start;
            start.count = 6.0f;
            std::string heard;
            for (int i = 0; i < 40; ++i) {
                const oj::RaceStart::Events e = start.update(step);
                if (e.beep) heard += std::to_string(start.second + 1);
                if (e.go) heard += "go";
            }
            return start.count == 0.0f ? heard : heard + "?";
        };
        check(count(0.7f) == "321go" && count(0.25f) == "321", "race start: the count's seconds and the start");
    }
}
