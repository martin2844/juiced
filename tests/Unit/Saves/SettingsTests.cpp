// Unit tests: Settings.

#include "Framework/Test.h"

#include "Core/Base/Random.h"
#include "Core/Saves/Profile.h"
#include "Core/Saves/SaveEntries.h"

namespace {

using oj::save::Settings;
using oj::test::check;

}  // namespace

// A new driver: the stock id of four rand() values, the settings of a new driver.
OJ_UNIT_TEST(Settings, NewProfileId) {
    oj::CrtRandom random;
    random.seed(42);
    check(oj::save::newProfileId(random) == 0x00AF019045CD7568ull, "new driver id from rand()");
}
OJ_UNIT_TEST(Settings, NewDriverSettings) {
    oj::save::Settings settings = oj::save::Settings::defaults(7);
    settings.setCrewName(L"Crew");
    oj::save::Settings settingsRead;
    check(settings.write().size() == 781 && settingsRead.read(settings.write()) &&
              settingsRead.write() == settings.write() && settingsRead.crewName() == "Crew" &&
              settingsRead.u32(Settings::kCustomTimeOfDay) == 2 && settingsRead.u32(Settings::kMph) == 0,
          "settings of a new driver: 781 bytes, crew name");
}
