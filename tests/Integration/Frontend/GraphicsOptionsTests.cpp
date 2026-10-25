// Integration tests: the graphics options (opts_graphics) with the fixed build's glow meter (Game/UI/WindowFixes.h)
// and the game options' BACK (opts_game_pc), the game's windows and texts through a front end of the tests
// (Framework/Fakes/FakeWindowHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeWindowHost.h"

#include "Core/Data/GameData.h"
#include "Core/Input/Input.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Game/Frontend/Options/GameOptions.h"
#include "Game/Frontend/Options/GraphicsOptions.h"
#include "Game/Frontend/Options/OptionsHost.h"
#include "Game/UI/UiFile.h"
#include "Game/UI/WindowFixes.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

namespace {

using oj::test::check;
using oj::ui::Element;
using oj::ui::ElementKind;

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

const Element* named(const oj::ui::Window& w, const char* name) {
    for (const Element& e : w.elements) {
        if (_stricmp(e.name.c_str(), name) == 0) return &e;
    }
    return nullptr;
}

size_t arrows(const oj::ui::Window& w) {
    size_t n = 0;
    for (const Element& e : w.elements) n += e.kind == ElementKind::Box && e.name == "ARROW" ? 1 : 0;
    return n;
}

bool near(float a, float b) {
    return std::fabs(a - b) < 0.0005f;
}

// The settings of the game kept and put back.
struct KeptGraphics {
    oj::GraphicsConfig kept = oj::graphicsConfig();
    ~KeptGraphics() { oj::graphicsConfig() = kept; }
};

}  // namespace

// The glow's row: the detail's meter and its minus and plus boxes moved down by GLOW_BUT's place below DETAIL_BUT,
// in place of the row's two arrows; once.
OJ_INTEGRATION_TEST(GraphicsOptions, GlowMeterInPlaceOfArrows) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::ui::Window stock;
    if (!stock.load(std::filesystem::path("ui") / "windows" / "opts_graphics.ui")) {
        check(false, "opts_graphics.ui read from the game's folder");
        return;
    }
    oj::ui::Window fixed = stock;
    oj::ui::fixWindow("OPTS_GRAPHICS", fixed);
    const Element* meter = named(fixed, oj::ui::kGlowMeter);
    const Element* minus = named(fixed, oj::ui::kGlowMinus);
    const Element* plus = named(fixed, oj::ui::kGlowPlus);
    const Element* detail = named(stock, "DETAIL");
    check(meter != nullptr && minus != nullptr && plus != nullptr && detail != nullptr, "the glow's meter and boxes");
    if (meter == nullptr || minus == nullptr || plus == nullptr || detail == nullptr) return;
    check(meter->kind == ElementKind::Meter && near(meter->x, 0.143f) && near(meter->y, 0.629f) &&
              near(meter->w, detail->w) && meter->properties.size() == detail->properties.size(),
          "GLOW_METER as DETAIL, 0.317 lower");
    check(minus->kind == ElementKind::Box && near(minus->x, 0.113f) && near(minus->y, 0.629f) &&
              plus->kind == ElementKind::Box && near(plus->x, 0.334f) && near(plus->y, 0.627f),
          "GLOW_MINUS and GLOW_PLUS as the detail's");
    check(arrows(stock) == 5 && arrows(fixed) == 3 && fixed.elements.size() == stock.elements.size() + 1,
          "the glow row's two arrows gone");
    oj::ui::Window again = fixed;
    oj::ui::fixWindow("opts_graphics", again);
    check(again.elements.size() == fixed.elements.size(), "a window with the meter is left as it is");
    oj::ui::Window other = stock;
    oj::ui::fixWindow("opts_audio", other);
    check(other.elements.size() == stock.elements.size(), "other windows as they are");
}

// The meter shows the glow's strength (the stock's on 50 %) and, while GLOW_BUT is highlighted, the glow follows it;
// LEFT and RIGHT no longer turn it over; BACK puts it back. Without shaders the meter is hidden and GLOW "-".
OJ_INTEGRATION_TEST(GraphicsOptions, GlowMeter) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    KeptGraphics kept;
    FakeOptionsHost host;
    oj::GameData data(oj::test::gameFiles().root());
    host.data = &data;
    oj::GraphicsConfig& c = oj::graphicsConfig();
    c.noShaders = 0;
    c.shaders = 1;
    c.glow = 1;
    const std::shared_ptr<oj::ui::Screen> screen = host.open("opts_graphics");
    if (screen == nullptr) return;
    oj::frontend::GraphicsOptions window;
    window.enter(host, *screen);
    check(screen->meterShown(oj::ui::kGlowMeter) == 0.5f, "the stock's on at 50 %");
    screen->setCurrentButton("GLOW_BUT");
    window.update(host, *screen, 0);
    check(screen->textOf("GLOW").empty() && !screen->hidden(oj::ui::kGlowMeter) &&
              !screen->hidden(oj::ui::kGlowPlus),
          "the meter in place of YES");
    check(c.glow == 50, "the glow at the meter's 50 %");
    screen->setMeterShown(oj::ui::kGlowMeter, 0.8f);
    window.update(host, *screen, 0);
    check(c.glow == 80, "the glow follows the meter");
    window.update(host, *screen, oj::input::kLeft);
    check(c.glow == 80, "LEFT leaves the glow");
    window.update(host, *screen, oj::input::kBack);
    check(c.glow == 1, "BACK: the glow as it was");

    c.noShaders = 1;
    c.shaders = 0;
    c.glow = 0;
    const std::shared_ptr<oj::ui::Screen> plain = host.open("opts_graphics");
    if (plain == nullptr) return;
    oj::frontend::GraphicsOptions shaderless;
    shaderless.enter(host, *plain);
    shaderless.update(host, *plain, 0);
    check(plain->hidden(oj::ui::kGlowMeter) && plain->hidden(oj::ui::kGlowMinus) &&
              plain->textOf("GLOW") == L"-",
          "without shaders the meter hidden and GLOW -");
}

// opts_game_pc's BACK (0x00481770): the stock gave AUTO2 AUTO's kept value and left the difficulty as changed; the
// fixed build gives each its own kept value back.
OJ_INTEGRATION_TEST(GameOptions, BackKeepsEachValue) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeOptionsHost host;
    oj::GameData data(oj::test::gameFiles().root());
    host.data = &data;
    oj::save::Settings& set = host.optionSettings();
    set.setU32(oj::save::Settings::kAutomatic, 1);
    set.setU32(oj::save::Settings::kAutomatic + 4, 0);
    set.setU32(oj::save::Settings::kDifficulty, 0);
    const std::shared_ptr<oj::ui::Screen> screen = host.open("opts_game_pc");
    if (screen == nullptr) return;
    oj::frontend::GameOptions window;
    window.enter(host, *screen);
    set.setU32(oj::save::Settings::kAutomatic + 4, 1);
    set.setU32(oj::save::Settings::kDifficulty, 1);
    window.update(host, *screen, oj::input::kBack);
    check(set.u32(oj::save::Settings::kAutomatic) == 1 && set.u32(oj::save::Settings::kDifficulty) == 0 &&
              (screen->find("AUTO2") < 0 || set.u32(oj::save::Settings::kAutomatic + 4) == 0),
          "BACK: AUTO, AUTO2 and the difficulty as they were");
}
