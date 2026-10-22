// Unit tests: RaceStart.

#include "Framework/Test.h"

#include "Core/Race/RaceKinds.h"
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

// 0x00401E80: a race's first view. A solo race and a cruise start in the kept view, on the network too (they played
// the fly-by, and skipping it brought the black over the start); a sprint's later heat with the sweep and the count at
// 8; the other races with the fly-by off the network and the sweep on it.
OJ_UNIT_TEST(RaceStart, FirstView) {
    using oj::RaceType;
    using oj::StartView;
    using oj::startView;
    check(startView(RaceType::Solo, 0, false) == StartView::Kept &&
              startView(RaceType::Cruise, 0, false) == StartView::Kept &&
              startView(RaceType::Solo, 0, true) == StartView::Kept &&
              startView(RaceType::Cruise, 0, true) == StartView::Kept,
          "a solo race and a cruise: the kept view, no fly-by");
    check(startView(RaceType::Sprint, 1, false) == StartView::LaterHeat &&
              startView(RaceType::Sprint, 0, false) == StartView::FlyBy,
          "a sprint: the fly-by for its first heat, the sweep with the count 8 for a later one");
    check(startView(RaceType::Circuit, 0, false) == StartView::FlyBy &&
              startView(RaceType::TimedLap, 0, false) == StartView::FlyBy &&
              startView(RaceType::TestTrack, 0, false) == StartView::FlyBy &&
              startView(RaceType::Circuit, 0, true) == StartView::Sweep,
          "the other races: the fly-by off the network, the sweep on it");
}
