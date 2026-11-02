// Integration tests: the fixed build's field of view in the game options (opts_game_pc, opts_ingame_game), the game's
// windows through a front end of the tests (Framework/Fakes/FakeWindowHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeWindowHost.h"

#include "Core/Input/Input.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Game/Frontend/Options/GameOptions.h"
#include "Game/Frontend/Options/OptionsHost.h"
#include "Game/UI/WindowFixes.h"

#include <cmath>
#include <filesystem>
#include <memory>
#include <string>

namespace {

using oj::test::check;

class FakeOptionsHost : public oj::test::FakeWindowHost, public oj::frontend::OptionsHost {
public:
    const char* optionsMenu() const override { return "opts_menu"; }
    bool racePause() const override { return false; }
    bool secondPlayerPaused() const override { return false; }
    uint32_t secondPlayerEvent() const override { return 0; }
    void setEffectsVolume(float) override {}
    void controlsSetUp() override {}
    void setDetailFactor(float) override {}
    void restorePlaylistChoice() override {}
};

float yOf(const oj::ui::Screen& s, const char* element) {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    s.placeOf(s.find(element), x, y, w, h);
    return y;
}

bool near(float a, float b) {
    return std::fabs(a - b) < 0.002f;
}

}  // namespace

// The rows below the hidden VIB row up a place (AUTO in VIB's, ..., SENSITIVITY in DIFF's), the field of view's row
// in SENSITIVITY's place (its meter, its signs and its degrees); DONE where it was.
OJ_INTEGRATION_TEST(GameOptionsFov, Rows) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeOptionsHost host;
    const std::shared_ptr<oj::ui::Screen> pc = host.open("opts_game_pc");
    const std::shared_ptr<oj::ui::Screen> race = host.open("opts_ingame_game");
    if (pc == nullptr || race == nullptr) return;
    check(near(yOf(*pc, "AUTO_BUT"), 0.181f) && near(yOf(*pc, "SENSITIVITY_BUT"), 0.633f) &&
              near(yOf(*pc, oj::ui::kFovButton), 0.724f) && near(yOf(*pc, oj::ui::kFovMeter), 0.769f) &&
              near(yOf(*pc, "DON_BUT"), 0.804f) && pc->find(oj::ui::kFovValue) >= 0,
          "opts_game_pc: the rows up a place, FOV in SENSITIVITY's, DONE kept");
    check(near(yOf(*race, "MAP_BUT"), 0.208f) && near(yOf(*race, "SENSITIVITY_BUT"), 0.609f) &&
              near(yOf(*race, oj::ui::kFovButton), 0.710f) && near(yOf(*race, "DON_BUT"), 0.798f),
          "opts_ingame_game: the same");
}

// The degrees (FOV_VALUE) aligned to the left of their box, just after the plus (0.012 after it), in both windows.
OJ_INTEGRATION_TEST(GameOptionsFov, ValueByThePlus) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    bool left = true;
    for (const char* name : {"opts_game_pc", "opts_ingame_game"}) {
        oj::ui::Window w;
        if (!w.load(std::filesystem::path("ui") / "windows" / (std::string(name) + ".ui"))) {
            left = false;
            continue;
        }
        oj::ui::fixWindow(name, w);
        const oj::ui::Element* value = nullptr;
        const oj::ui::Element* plus = nullptr;
        for (const oj::ui::Element& e : w.elements) {
            if (e.name == oj::ui::kFovValue) value = &e;
            if (e.name == oj::ui::kFovPlus) plus = &e;
        }
        const oj::ui::Property* align = value != nullptr ? value->find("Align") : nullptr;
        left = left && plus != nullptr && align != nullptr && align->values.size() == 1 && align->values[0] == "Left" &&
               near(value->x, plus->x + plus->w + 0.012f);
    }
    check(left, "the degrees left-aligned just after the plus");
}

// The row: "FIELD OF VIEW", the stock's 67 degrees on a 4:3 screen; the meter at its end the most view (1.8, 100
// degrees) at once; BACK gives the kept view back.
OJ_INTEGRATION_TEST(GameOptionsFov, Meter) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeOptionsHost host;
    const std::shared_ptr<oj::ui::Screen> screen = host.open("opts_game_pc");
    if (screen == nullptr) return;
    oj::ui::Screen& s = *screen;
    oj::GraphicsConfig& c = oj::graphicsConfig();
    const int32_t before = c.fov;
    c.fov = 0;
    oj::frontend::GameOptions window;
    window.enter(host, s);
    check(s.textOf(oj::ui::kFovButton) == L"FIELD OF VIEW" && s.textOf(oj::ui::kFovValue) == L"67",
          "FIELD OF VIEW at the stock's 67 degrees");
    s.setCurrentButton(oj::ui::kFovButton);
    s.setMeter(oj::ui::kFovMeter, 1.0f, true);
    window.update(host, s, 0);
    check(c.fov == 1800 && s.textOf(oj::ui::kFovValue) == L"100", "the meter's end: the most view, 100 degrees");
    window.update(host, s, oj::input::kBack);
    check(c.fov == 0, "BACK: the kept view back");
    c.fov = before;
}
