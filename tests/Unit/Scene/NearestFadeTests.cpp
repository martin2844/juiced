// Unit tests: the nearest lights kept as they come and go (Game/Scene/NearestFade.h): the lights taken weighed by their
// lead over the nearest left out, all of them with none left out; one passed by the next going out as it is passed; the
// distance's fade.

#include "Framework/Test.h"

#include "Game/Scene/NearestFade.h"

#include <cmath>
#include <vector>

namespace {

using oj::test::check;

bool close(float a, float b) { return std::fabs(a - b) < 1e-5f; }

}  // namespace

OJ_UNIT_TEST(NearestFade, Ranks) {
    const std::vector<float> w = oj::scene::rankFade({100.0f, 200.0f, 450.0f, 500.0f}, 3, 100.0f);
    check(w.size() == 3 && w[0] == 1.0f && w[1] == 1.0f && close(w[2], 0.5f),
          "the three taken: those a band nearer than the fourth whole, the third half way");
    check(oj::scene::rankFade({100.0f, 200.0f}, 3, 100.0f) == std::vector<float>{1.0f, 1.0f} &&
              oj::scene::rankFade({}, 3, 100.0f).empty(),
          "all whole with none left out");
    // The third and the fourth about to trade places, then level: the third all but out, then out.
    const std::vector<float> before = oj::scene::rankFade({100.0f, 200.0f, 299.0f, 301.0f}, 3, 100.0f);
    const std::vector<float> level = oj::scene::rankFade({100.0f, 200.0f, 300.0f, 300.0f}, 3, 100.0f);
    check(close(before[2], 0.02f) && level[2] == 0.0f && level[0] == 1.0f,
          "a light passed by the next out as they pass, no jump");
    check(oj::scene::rankFade({100.0f, 200.0f}, 1, 0.0f) == std::vector<float>{1.0f}, "no band: whole");
}

OJ_UNIT_TEST(NearestFade, Distance) {
    check(oj::scene::distanceFade(10.0f, 50.0f, 70.0f) == 1.0f && oj::scene::distanceFade(50.0f, 50.0f, 70.0f) == 1.0f,
          "whole within the full distance");
    check(close(oj::scene::distanceFade(60.0f, 50.0f, 70.0f), 0.5f) &&
              oj::scene::distanceFade(70.0f, 50.0f, 70.0f) == 0.0f &&
              oj::scene::distanceFade(90.0f, 50.0f, 70.0f) == 0.0f,
          "linear to 0 at the reach, 0 past it");
}
