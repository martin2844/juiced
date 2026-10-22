// Integration tests: the race HUD's challenge targets (Game/Race/Hud/RaceHud.cpp) over the game's hud_solo.ui and
// texts.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Base/Text.h"
#include "Core/Data/Language.h"
#include "Core/Saves/SaveEntries.h"
#include "Game/Race/Hud/RaceHud.h"

#include <filesystem>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// The speed challenge's target (0x00449C77) is its speed in miles an hour, or x 1.6093 in kilometres an hour, cut:
// 50 is " 50 MPH" or " 80 KPH" (it was shown x 3.6, " 180 KPH", for a target of 80).
OJ_INTEGRATION_TEST(RaceHudTarget, SpeedInTheSettingsUnits) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::test::NullRenderDevice device;
    const oj::Language language = *oj::Language::byName("english");
    oj::TextTable texts;
    texts.load(std::filesystem::path("scripts") / (std::string(language.name) + ".txt"));
    const auto target = [&](bool mph) {
        oj::RaceHud hud(device, language, texts);
        if (!hud.load("hud_solo", nullptr)) return std::wstring(L"hud_solo.ui not read");
        oj::save::Settings settings = oj::save::Settings::defaults(language.index);
        settings.setU32(oj::save::Settings::kMph, mph ? 1 : 0);
        oj::HudRace race;
        race.players.resize(1);
        race.type = oj::RaceType::Speed;
        race.challengeSpeed = 50.0f;
        race.settings = &settings;
        hud.enter(race);
        return hud.screen().textOf("LR_TIME1");
    };
    const std::wstring kph = target(false);
    const std::wstring mph = target(true);
    check(kph.starts_with(L" 80 "), "kilometres an hour: 50 x 1.6093, cut (" + oj::toUtf8(kph) + ")");
    check(mph.starts_with(L" 50 "), "miles an hour: the speed itself (" + oj::toUtf8(mph) + ")");
}
