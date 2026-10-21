// Unit tests: CarParts.

#include "Framework/Test.h"

#include "Core/Base/Matrix.h"
#include "Core/Cars/CarDamage.h"
#include "Core/Tracks/SceneObjects.h"

#include <array>
#include <bit>
#include <cmath>

namespace {

using oj::test::check;

}  // namespace

// The car parts' objects (0x004191C0) by their nodes' names: the axes, the constants, the bits (the right skirt
// takes the left's, a stock problem) and the files' name of an exhaust; the chassis and the decal none.
OJ_UNIT_TEST(CarParts, PartObjects) {
    const oj::CarPartObject fbump = oj::CarPartObject::make("FBUMP");
    const oj::CarPartObject rbump = oj::CarPartObject::make("RBUMP");
    const oj::CarPartObject bonnet = oj::CarPartObject::make("BONNET");
    const oj::CarPartObject spoiler = oj::CarPartObject::make("SPOILER");
    const oj::CarPartObject skirt = oj::CarPartObject::make("SSKIRT_R");
    const oj::CarPartObject exhaust = oj::CarPartObject::make("EXHAUST_S");
    const oj::CarPartObject decal = oj::CarPartObject::make("DECAL");
    const oj::CarPartObject chassis = oj::CarPartObject::make("3000GT_CHASS");
    const bool named =
        fbump.axes == 7 && fbump.bit == 1 && fbump.amount == std::bit_cast<float>(0x399D4952u) &&
        fbump.softness == 1.2f && rbump.axes == 7 && rbump.bit == 2 &&
        rbump.amount == std::bit_cast<float>(0x3951B717u) && bonnet.axes == 1 && bonnet.bit == 4 &&
        spoiler.axes == 1 && spoiler.bit == 8 && spoiler.softness == 0.5f && skirt.axes == 5 && skirt.bit == 0x10 &&
        skirt.files == "SSKIRT_R" && exhaust.axes == 3 && exhaust.bit == 0 && exhaust.softness == 0.5f &&
        exhaust.files == "exhaust" && decal.axes == 0 && chassis.axes == 0 && chassis.attached;
    // The wobble (0x00419780): without a zone no turn, the phase moving on by dt x 4 / 0.0125; a zone's dents of
    // 20 (level 1) at a great speed turn every axis by -100 x the amount at most; a change across the ground over
    // 20 adds its y about x and is held to +100 x the amount; the bonnet holds only its turn about x.
    oj::CarPartObject still = fbump;
    still.wobble(0.005f, nullptr, 5000.0f, 0.0f, {});
    const bool none = still.angles[0] == 0.0f && still.angles[1] == 0.0f && still.angles[2] == 0.0f &&
                      still.phase == 0.005f * 4.0f / 0.0125f;
    oj::DamageZone zone;
    zone.impact = 20.0f;
    oj::CarPartObject fast = fbump;
    fast.phase = 1.0f;
    fast.wobble(0.02f, &zone, 1.0e6f, 0.0f, {});
    const float most = 100.0f * fbump.amount;
    const bool held = fast.angles[0] == -most && fast.angles[1] == -most && fast.angles[2] == -most;
    oj::CarPartObject slow = fbump;
    slow.phase = 1.0f;
    slow.wobble(0.02f, &zone, 10.0f, 0.0f, {});
    const float p = slow.phase;
    const float expected = -(std::fabs(static_cast<float>(std::sin(static_cast<double>(p))) +
                                       static_cast<float>(std::sin(static_cast<double>(p * 0.3f)))) *
                             (1.0f / 1.2f) * 1.0f * 0.02f) *
                           fbump.amount * 10.0f;
    const bool turned = slow.angles[0] == expected && slow.angles[0] < 0.0f && slow.angles[0] > -most;
    oj::CarPartObject shaken = bonnet;
    shaken.wobble(0.02f, &zone, 0.0f, 100.0f, {0.0f, 1.0e6f, 1.0e6f, 0.0f});
    const float bonnetMost = 100.0f * bonnet.amount;
    const bool across = shaken.angles[0] == bonnetMost && shaken.angles[1] > bonnetMost;
    // The turn (0x0041B050) about x of the angle: cos and sin in the y and z rows, the angle kept.
    std::array<float, 3> angles{0.25f, 0.0f, 0.0f};
    const oj::Mat34 t = oj::turnObject(1, angles, {0.0f, 0.0f, 0.0f}, 0.0f, 50.0f);
    const bool turn = angles[0] == 0.25f && t[0] == 1.0f && t[5] == std::cos(0.25f) && t[6] == -std::sin(0.25f) &&
                      t[9] == std::sin(0.25f) && t[10] == std::cos(0.25f);
    check(named && none && held && turned && across && turn,
          "car parts: the parts' objects by name, their wobble by the zone's dents, held, and the turn");
}
