// Unit tests: the fixed build's [Wheel] of controller.ini (Game/Platform/WheelInput readWheelSettings), as the
// plugin's ini takes it.

#include "Framework/Test.h"

#include "Game/Platform/WheelInput.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

using oj::test::check;

std::filesystem::path writeIni(const char* text) {
    const std::filesystem::path p = std::filesystem::temp_directory_path() / "oj_wheel_settings.ini";
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << text;
    return p;
}

}  // namespace

// No file, no [Wheel]: the defaults (no dead zone nor rotation, lock 240, the game's power and curve, the game's force
// feedback at 100%).
OJ_UNIT_TEST(WheelSettings, Defaults) {
    const std::filesystem::path p = writeIni("[Controller]\r\nenabled = true\r\n");
    const oj::WheelSettings s = oj::readWheelSettings(p.wstring());
    check(s.deadzone == 0.0f && s.rotation == 0.0f && s.lock == 240.0f && s.exponent == 0.0f && !s.linearInput &&
              s.ffb == oj::WheelSettings::Ffb::Game && s.scale.strength == 100 && s.scale.centering == 100 &&
              s.scale.effects == 100 && !s.scale.invert && s.gain() == 1.0f,
          "the defaults");
    std::filesystem::remove(p);
}

// Values up to a ';', within their ranges (else the default); an exponent under 0.3 the game's; ffb off, game, scaled
// (true and 1 of the plugin's 0.1.5 scaled); the steering's factor the rotation / the lock.
OJ_UNIT_TEST(WheelSettings, Values) {
    std::filesystem::path p = writeIni(
        "[Wheel]\r\ndeadzone = 0.1 ; a comment\r\nrotation = 900\r\nlock = 450\r\nexponent = 1.5\r\n"
        "linearInput = true\r\nffb = scaled\r\nffbStrength = 50\r\nffbCentering = 250\r\nffbEffects = 150\r\n"
        "ffbInvert = yes\r\n");
    oj::WheelSettings s = oj::readWheelSettings(p.wstring());
    check(std::fabs(s.deadzone - 0.1f) < 1e-6f && s.rotation == 900.0f && s.lock == 450.0f && s.exponent == 1.5f &&
              s.linearInput && std::fabs(s.gain() - 2.0f) < 1e-6f,
          "the steering's values; the factor 900 / 450");
    check(s.ffb == oj::WheelSettings::Ffb::Scaled && s.scale.strength == 50 && s.scale.centering == 100 &&
              s.scale.effects == 150 && s.scale.invert,
          "the force feedback's: centering 250 out of its range, the default");
    p = writeIni("[Wheel]\r\ndeadzone = 0.6\r\nexponent = 0.2\r\nffb = off\r\nlock = 10\r\n");
    s = oj::readWheelSettings(p.wstring());
    check(s.deadzone == 0.0f && s.exponent == 0.0f && s.ffb == oj::WheelSettings::Ffb::Off && s.lock == 240.0f,
          "out of range: the defaults; an exponent of 0.2: the game's; ffb off");
    p = writeIni("[Wheel]\r\nffb = true\r\n");
    check(oj::readWheelSettings(p.wstring()).ffb == oj::WheelSettings::Ffb::Scaled, "ffb = true: scaled");
    p = writeIni("[Wheel]\r\nffb = something\r\n");
    check(oj::readWheelSettings(p.wstring()).ffb == oj::WheelSettings::Ffb::Game, "another word: the game's");
    std::filesystem::remove(p);
}
