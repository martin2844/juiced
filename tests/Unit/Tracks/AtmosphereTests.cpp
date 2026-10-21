// Unit tests: Atmosphere.

#include "Framework/Test.h"

#include "Core/Base/Random.h"
#include "Core/Tracks/Atmosphere.h"

#include <cstdio>
#include <vector>

namespace {

using oj::test::check;

}  // namespace

// The rain of a wet atmosphere: its timer within the times as read, then the cycle (0x00534300).
OJ_UNIT_TEST(Atmosphere, Rain) {
    std::vector<oj::Atmosphere> list = oj::parseAtmospheres(
        "[BeginAtmosphere]\t0\n\tWet\n\tMinRoadWetness 0.0\n\tMaxRoadWetness 1.0\n\tRoadWetnessTime 200\n"
        "\tMinRainOnTime 0\n\tMaxRainOnTime 60\n\tMinRainOffTime 0\n\tMaxRainOffTime 30\n[EndAtmosphere]\n");
    bool ok = list.size() == 1 && list[0].raining == 1 && list[0].rainTimer == 60.0f;
    if (ok) {
        oj::Atmosphere& a = list[0];
        oj::CrtRandom random;
        int steps = 0;
        while (a.raining == 1 && steps < 4000) {
            oj::stepRain(a, 0.02f, random);
            ++steps;
        }
        std::printf("     rain: stops after %d steps of 0.02 s, off for %.2f s, road wetness %.4f, rain %.2f\n", steps,
                    a.rainTimer, a.roadWetness, a.rain);
        ok = a.raining == 0 && a.rainTimer >= 0.0f && a.rainTimer <= 30.0f && a.roadWetness > 0.29f &&
             a.roadWetness < 0.31f && a.rain < 1.0f;
    }
    check(ok, "atmosphere: the rain's times as read, the rain stopping and the road getting wetter");
}
