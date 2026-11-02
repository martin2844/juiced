// Integration tests: the graphics options' glow (opts_graphics) and the game options' BACK (opts_game_pc), the
// game's windows and texts through a front end of the tests (Framework/Fakes/FakeWindowHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeWindowHost.h"

#include "Core/Data/GameData.h"
#include "Core/Data/GameTexts.h"
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
#include <vector>

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
    void keepEffects(const oj::FixedEffects& e) override { keptEffects.push_back(e); }
    void applyEffects(const oj::FixedEffects& e, bool programs) override {
        applied.push_back(e);
        if (programs) ++remade;
    }
    oj::render::QualityCaps qualityCaps() const override { return caps; }
    oj::render::QualityCaps caps{{2, 4, 6, 8}, true, 16};  // an adapter of MSAA 2x .. 8x, shaders 3.0, 16x
    std::vector<oj::FixedEffects> keptEffects;
    std::vector<oj::FixedEffects> applied;
    int remade = 0;
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

// The glow's row (LIGHT FLARES): OFF, STOCK (on is 1) and the fixed build's CUSTOM (on with the effects' LightFlares),
// LEFT and RIGHT step round them, BACK puts them back; the window's own elements (its two arrows, no meter; the
// ADVANCED page's added). Without shaders GLOW "-" and it stays off.
OJ_INTEGRATION_TEST(GraphicsOptions, GlowYesNo) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::ui::Window stock;
    if (!stock.load(std::filesystem::path("ui") / "windows" / "opts_graphics.ui")) {
        check(false, "opts_graphics.ui read from the game's folder");
        return;
    }
    oj::ui::Window fixed = stock;
    oj::ui::fixWindow("opts_graphics", fixed);
    check(fixed.elements.size() > stock.elements.size() && arrows(fixed) == 4 && named(fixed, "GLOW_METER") == nullptr,
          "opts_graphics: the glow row's arrows, no meter");
    check(named(fixed, "DOF_BUT") == nullptr && named(fixed, "DOF") == nullptr && named(stock, "DOF_BUT") != nullptr,
          "opts_graphics: the depth of field's row out (its button, its value, its left arrow)");

    KeptGraphics kept;
    FakeOptionsHost host;
    oj::GameData data(oj::test::gameFiles().root());
    host.data = &data;
    oj::GraphicsConfig& c = oj::graphicsConfig();
    c.noShaders = 0;
    c.shaders = 1;
    c.glow = 50;    // a percent the fixed build's meter kept: on
    const std::shared_ptr<oj::ui::Screen> screen = host.open("opts_graphics");
    if (screen == nullptr) return;
    oj::frontend::GraphicsOptions window;
    window.enter(host, *screen);
    screen->setCurrentButton("GLOW_BUT");
    c.effects = oj::FixedEffects{};
    window.update(host, *screen, 0);
    const std::wstring stockText = screen->textOf("GLOW");
    window.update(host, *screen, oj::input::kLeft);
    window.update(host, *screen, 0);
    const std::wstring offText = screen->textOf("GLOW");
    check(stockText == L"STOCK" && offText == host.gameTexts().at(oj::kTextOffLowMedHi) && c.glow == 0,
          "GLOW STOCK, LEFT turns it off: OFF");
    window.update(host, *screen, oj::input::kRight);
    window.update(host, *screen, 0);
    check(c.glow == 1 && c.effects.lightFlares == 0 && screen->textOf("GLOW") == stockText,
          "RIGHT turns it on: 1, STOCK");
    window.update(host, *screen, oj::input::kRight);
    window.update(host, *screen, 0);
    check(c.glow == 1 && c.effects.lightFlares == 1 && screen->textOf("GLOW") == L"CUSTOM",
          "RIGHT again: CUSTOM (on, the effects' LightFlares)");
    window.update(host, *screen, oj::input::kRight);
    check(c.glow == 0 && c.effects.lightFlares == 0, "RIGHT again: round to off");
    window.update(host, *screen, oj::input::kLeft);
    check(c.glow == 1 && c.effects.lightFlares == 1, "LEFT from off: CUSTOM");
    window.update(host, *screen, oj::input::kBack);
    check(c.glow == 50 && c.effects.lightFlares == 0, "BACK: the glow as it was, STOCK");

    c.noShaders = 1;
    c.shaders = 0;
    c.glow = 0;
    const std::shared_ptr<oj::ui::Screen> plain = host.open("opts_graphics");
    if (plain == nullptr) return;
    oj::frontend::GraphicsOptions shaderless;
    shaderless.enter(host, *plain);
    plain->setCurrentButton("GLOW_BUT");
    shaderless.update(host, *plain, 0);
    shaderless.update(host, *plain, oj::input::kRight);
    check(plain->textOf("GLOW") == L"-" && c.glow == 0, "without shaders GLOW - and off");
}

// The fixed build's ADVANCED page (Game/UI/WindowFixes, GraphicsOptions): its rows hidden on the stock's page and
// inside the window's background; SELECT on ADVANCED turns to it, its rows step (OFF, the values round; ANTI-ALIASING
// OFF, MSAA 2X, 4X, 8X, SMAA, SMAA + MSAA 2X, TEXTURE FILTER OFF, TRILINEAR, ANISO 2X .. 16X; HEADLIGHTS MED (the
// stock's), HI (the fixed build's) and ULTRA (with their shadows)), each change in effect at once and the programs made again as the page is left
// with the car paint changed; BACK on it turns back, BACK on the window puts the effects back (in effect again), DONE
// keeps them.
OJ_INTEGRATION_TEST(GraphicsOptions, AdvancedPage) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    KeptGraphics kept;
    FakeOptionsHost host;
    oj::GameData data(oj::test::gameFiles().root());
    host.data = &data;
    oj::GraphicsConfig& c = oj::graphicsConfig();
    c.noShaders = 0;
    c.shaders = 1;
    c.effects = oj::FixedEffects{};
    const std::shared_ptr<oj::ui::Screen> screen = host.open("opts_graphics");
    if (screen == nullptr) return;
    oj::ui::Screen& s = *screen;
    bool inside = true;
    for (int i = 0; i < oj::ui::kEffectsRows; ++i) {
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
        const std::string value =
            i == oj::ui::kEffectsMeterRow ? std::string(oj::ui::kEffectsPlus) : oj::ui::effectsValue(i);
        s.placeOf(s.find(value), x, y, w, h);
        inside = inside && y > 0.17f && y + h < 0.86f && x + w < 0.92f;
    }
    check(s.find(oj::ui::kMoreButton) >= 0 && inside, "the ADVANCED page's values inside the window's background");
    // The rows keep clear of the ADVANCED button (the user, 2026-10-30: its label was drawn over HEAT HAZE in the main
    // menu and in a race's pause alike; the stock rows' 0.0625 step put the ninth row on it).
    float rowX = 0.0f, rowY = 0.0f, rowW = 0.0f, rowH = 0.0f;
    float backX = 0.0f, backY = 0.0f, backW = 0.0f, backH = 0.0f;
    s.placeOf(s.find(oj::ui::effectsButton(oj::ui::kEffectsRows - 1)), rowX, rowY, rowW, rowH);
    s.placeOf(s.find(oj::ui::kMoreButton), backX, backY, backW, backH);
    check(rowY + rowH < backY && rowY > 0.17f, "the last row (HEAT HAZE) above the ADVANCED button");
    oj::frontend::GraphicsOptions window;
    window.enter(host, s);
    check(s.hidden(oj::ui::effectsButton(0)) && s.hidden(oj::ui::kEffectsMeter) && !s.hidden("GLOW_BUT") &&
              s.textOf(oj::ui::kMoreButton) == L"ADVANCED",
          "the stock's page first, the ADVANCED page's rows hidden");
    // The graphics presets (the stock's SHADOWS row's place, the user 2026-10-29: its elements under their stock names,
    // the button's label ours): MED the stock's look (a fresh install is MED), HI and ULTRA the fixed build's; a row
    // changed by hand leaves CUSTOM, and RIGHT from CUSTOM is MED again.
    s.setCurrentButton("SHADOWS_BUT");
    window.update(host, s, oj::input::kRight);
    window.update(host, s, 0);
    check(s.textOf("SHADOWS_BUT") == L"GRAPHICS" && s.textOf("SHADOWS") == L"MED" && c.effects == oj::FixedEffects{},
          "the presets' row: RIGHT to MED (the stock's, the effects off)");
    window.update(host, s, oj::input::kRight);
    window.update(host, s, 0);
    check(s.textOf("SHADOWS") == L"HI" && c.effects.worldLight == 1 && c.effects.ambientOcclusion == 1 &&
              c.effects.sky == 1 && c.effects.headlights == 1 && c.effects.carPaint == 1 && c.effects.shadows == 1,
          "RIGHT: HI, the rows set together");
    window.update(host, s, oj::input::kRight);
    window.update(host, s, 0);
    check(s.textOf("SHADOWS") == L"ULTRA" && c.effects.worldLight == 2 && c.effects.sky == 2 && c.effects.headlights == 2 &&
              c.effects.shadows == 2 && c.effects.ambientOcclusion == 1 && c.effects.reflections == 1,
          "RIGHT: ULTRA");
    c.effects.sky = 0;  // a row changed by hand
    s.setCurrentButton("GLOW_BUT");
    window.update(host, s, 0);
    check(s.textOf("SHADOWS") == L"CUSTOM", "a row changed by hand: the presets' row CUSTOM");
    s.setCurrentButton("SHADOWS_BUT");
    window.update(host, s, oj::input::kRight);
    window.update(host, s, 0);
    check(s.textOf("SHADOWS") == L"MED" && c.effects.sky == 0 && c.effects.worldLight == 0,
          "RIGHT from CUSTOM: MED (the stock's), the rows set again");
    // HEAT HAZE is the player's own small setting (the user, 2026-10-29): a preset neither sets it nor counts it, so it
    // leaves the row at its preset and survives the step.
    c.effects.heatHaze = 1;
    c.effects.heatHazeStrength = 25;
    s.setCurrentButton("GLOW_BUT");
    window.update(host, s, 0);
    check(s.textOf("SHADOWS") == L"MED", "HEAT HAZE is no preset's: the row stays MED");
    s.setCurrentButton("SHADOWS_BUT");
    window.update(host, s, oj::input::kRight);
    window.update(host, s, 0);
    check(s.textOf("SHADOWS") == L"HI" && c.effects.heatHaze == 1 && c.effects.heatHazeStrength == 25,
          "HEAT HAZE kept through a preset (HI)");
    window.update(host, s, oj::input::kLeft);
    window.update(host, s, 0);
    check(s.textOf("SHADOWS") == L"MED" && c.effects.worldLight == 0 && c.effects.heatHaze == 1 &&
              c.effects.heatHazeStrength == 25,
          "back to MED, HEAT HAZE still the player's");
    c.effects.heatHaze = 0;
    c.effects.heatHazeStrength = 50;
    s.setCurrentButton(oj::ui::kMoreButton);
    window.update(host, s, oj::input::kSelect);
    check(!s.hidden(oj::ui::effectsButton(0)) && s.hidden("GLOW_BUT") && s.hidden("DETAIL") &&
              s.currentButton() == oj::ui::effectsButton(0) && s.textOf(oj::ui::effectsButton(2)) == L"CAR MATERIALS" &&
              s.textOf(oj::ui::effectsValue(1)) == L"ANISO 16X",
          "ADVANCED: its page from ANTI-ALIASING, the stock's hidden, TEXTURE FILTER ANISO 16X");

    const auto press = [&](int row, uint32_t event) {
        s.setCurrentButton(oj::ui::effectsButton(row));
        window.update(host, s, event);
    };
    const std::wstring off = s.textOf(oj::ui::effectsValue(0));
    press(0, oj::input::kRight);
    check(c.effects.msaa == 2 && c.effects.smaa == 0 && s.textOf(oj::ui::effectsValue(0)) == L"MSAA 2X" &&
              !host.applied.empty() && host.applied.back().msaa == 2 && host.remade == 0,
          "ANTI-ALIASING: OFF, RIGHT MSAA 2X (6x not offered), in effect at once");
    press(0, oj::input::kLeft);
    press(0, oj::input::kLeft);
    check(c.effects.msaa == 2 && c.effects.smaa == 1 && s.textOf(oj::ui::effectsValue(0)) == L"SMAA + MSAA 2X",
          "ANTI-ALIASING: LEFT round to SMAA + MSAA 2X");
    press(0, oj::input::kLeft);
    check(c.effects.msaa == 0 && c.effects.smaa == 1 && s.textOf(oj::ui::effectsValue(0)) == L"SMAA",
          "ANTI-ALIASING: LEFT SMAA");
    press(0, oj::input::kLeft);
    check(c.effects.msaa == 8 && c.effects.smaa == 0, "ANTI-ALIASING: LEFT MSAA 8X");
    c.effects.msaa = 16;
    press(0, oj::input::kRight);
    check(c.effects.msaa == 0 && c.effects.smaa == 1, "ANTI-ALIASING: 16 samples (OpenJuiceConfig's), RIGHT SMAA");
    c.effects.smaa = 0;
    press(1, oj::input::kRight);
    check(c.effects.textureFilter == 0 && s.textOf(oj::ui::effectsValue(1)) == off &&
              host.applied.back().textureFilter == 0,
          "TEXTURE FILTER: ANISO 16X, RIGHT round to OFF, in effect at once");
    press(1, oj::input::kRight);
    check(c.effects.textureFilter == 1 && s.textOf(oj::ui::effectsValue(1)) == L"TRILINEAR",
          "TEXTURE FILTER: RIGHT TRILINEAR");
    host.caps.maxAnisotropy = 4;
    c.effects.textureFilter = 5;
    window.enter(host, s);
    s.setCurrentButton(oj::ui::kMoreButton);
    window.update(host, s, oj::input::kSelect);
    press(1, oj::input::kLeft);
    check(c.effects.textureFilter == 3 && s.textOf(oj::ui::effectsValue(1)) == L"ANISO 4X",
          "TEXTURE FILTER: the adapter's 4x at most, ANISO 16X (OpenJuiceConfig's) LEFT ANISO 4X");
    c.effects.textureFilter = 5;
    check(s.meterShown(oj::ui::kEffectsMeter) == 0.0f && !s.hidden(oj::ui::kEffectsMinus), "HEAT HAZE: empty, off");
    press(8, oj::input::kRight);
    check(c.effects.heatHaze == 1 && c.effects.heatHazeStrength == 5 && s.meterShown(oj::ui::kEffectsMeter) == 0.1f &&
              !host.applied.empty() && host.applied.back().heatHaze == 1 && host.remade == 0,
          "HEAT HAZE: RIGHT a step of the ten (5 %), in effect at once");
    for (int k = 0; k < 12; ++k) press(8, oj::input::kRight);
    check(c.effects.heatHazeStrength == 50 && s.meterShown(oj::ui::kEffectsMeter) == 1.0f,
          "HEAT HAZE: full at ten steps (50 %), no further");
    for (int k = 0; k < 10; ++k) press(8, oj::input::kLeft);
    check(c.effects.heatHaze == 0 && s.meterShown(oj::ui::kEffectsMeter) == 0.0f, "HEAT HAZE: LEFT down to off");
    press(8, oj::input::kLeft);
    check(c.effects.heatHaze == 0, "HEAT HAZE: LEFT at off stays off");
    const std::wstring skyMed = s.textOf(oj::ui::effectsValue(6));
    press(6, oj::input::kRight);
    check(skyMed == host.gameTexts().at(oj::kTextOffLowMedHi + 2) && c.effects.sky == 1 &&
              s.textOf(oj::ui::effectsButton(6)) == L"SKY" && host.applied.back().sky == 1 && host.remade == 0,
          "SKY: MED (the stock's), RIGHT HI, in effect at once, no program made again");
    press(6, oj::input::kRight);
    check(c.effects.sky == 2 && s.textOf(oj::ui::effectsValue(6)) == L"ULTRA", "SKY: RIGHT ULTRA");
    press(6, oj::input::kRight);
    check(c.effects.sky == 0, "SKY: RIGHT round to MED");
    const std::wstring worldMed = s.textOf(oj::ui::effectsValue(4));
    press(4, oj::input::kRight);
    check(worldMed == host.gameTexts().at(oj::kTextOffLowMedHi + 2) && c.effects.worldLight == 1 &&
              s.textOf(oj::ui::effectsValue(4)) == host.gameTexts().at(oj::kTextOffLowMedHi + 3),
          "WORLD LIGHT: MED (the stock's), RIGHT HI");
    press(4, oj::input::kLeft);
    press(4, oj::input::kLeft);
    check(c.effects.worldLight == 2 && s.textOf(oj::ui::effectsValue(4)) == L"ULTRA",
          "WORLD LIGHT: LEFT round to ULTRA");
    press(4, oj::input::kRight);
    // AMBIENT OCCLUSION (its own row, the user 2026-10-29: SSAO out of WORLD LIGHT HI): OFF or HI; the depth the other
    // passes read is drawn either way (TrackView::ambientOcclusion against ::occlusion).
    press(5, oj::input::kRight);
    check(c.effects.ambientOcclusion == 1 && s.textOf(oj::ui::effectsButton(5)) == L"AMBIENT OCCLUSION" &&
              s.textOf(oj::ui::effectsValue(5)) == host.gameTexts().at(oj::kTextOffLowMedHi + 3),
          "AMBIENT OCCLUSION: RIGHT HI");
    press(5, oj::input::kRight);
    check(c.effects.ambientOcclusion == 0 &&
              s.textOf(oj::ui::effectsValue(5)) == host.gameTexts().at(oj::kTextOffLowMedHi),
          "AMBIENT OCCLUSION: RIGHT round to OFF");
    // SHADOWS (the stock's own row, moved here): OFF, MED (the stock's silhouettes), HI (the cars' shadows from the
    // street lamps), ULTRA (the sun's cascades); the stock's field keeps its off/on, ours the levels above it.
    oj::setShadowLevel(c, 0);
    window.update(host, s, 0);
    const std::wstring shadowsOff = s.textOf(oj::ui::effectsValue(3));
    press(3, oj::input::kRight);
    check(shadowsOff == host.gameTexts().at(oj::kTextOffLowMedHi) && c.shadows == 2 && c.effects.shadows == 0 &&
              s.textOf(oj::ui::effectsButton(3)) == L"SHADOWS" &&
              s.textOf(oj::ui::effectsValue(3)) == host.gameTexts().at(oj::kTextOffLowMedHi + 2),
          "SHADOWS: OFF, RIGHT MED (the stock's silhouettes)");
    press(3, oj::input::kRight);
    press(3, oj::input::kRight);
    check(c.effects.shadows == 2 && s.textOf(oj::ui::effectsValue(3)) == L"ULTRA", "SHADOWS: RIGHT ULTRA");
    press(3, oj::input::kRight);
    check(oj::shadowLevel(c) == 3, "SHADOWS: at ULTRA no further");
    press(3, oj::input::kLeft);
    press(3, oj::input::kLeft);
    press(3, oj::input::kLeft);
    check(c.shadows == 1 && c.effects.shadows == 0 && oj::shadowLevel(c) == 0, "SHADOWS: LEFT down to OFF");
    press(3, oj::input::kRight);
    press(2, oj::input::kRight);
    check(c.effects.carPaint == 1 && host.remade == 0, "CAR MATERIALS HI, the programs not yet made again");
    press(2, oj::input::kBack);
    check(host.remade == 1 && s.hidden(oj::ui::effectsButton(0)) && !s.hidden("GLOW_BUT") &&
              s.currentButton() == oj::ui::kMoreButton,
          "BACK on the page: the stock's page, the programs made again for the car paint");
    window.update(host, s, oj::input::kBack);
    check(c.effects == oj::FixedEffects{} && host.applied.back() == oj::FixedEffects{} && host.remade == 2 &&
              host.keptEffects.empty(),
          "BACK on the window: the effects put back and in effect, nothing kept");

    window.enter(host, s);
    s.setCurrentButton(oj::ui::kMoreButton);
    window.update(host, s, oj::input::kSelect);
    const std::wstring med = s.textOf(oj::ui::effectsValue(7));
    press(7, oj::input::kRight);
    check(med == host.gameTexts().at(oj::kTextOffLowMedHi + 2) && c.effects.headlights == 1 &&
              s.textOf(oj::ui::effectsValue(7)) == host.gameTexts().at(oj::kTextOffLowMedHi + 3),
          "HEADLIGHTS: MED (the stock's), RIGHT HI");
    press(7, oj::input::kRight);
    check(c.effects.headlights == 2 && s.textOf(oj::ui::effectsValue(7)) == L"ULTRA", "HEADLIGHTS: RIGHT ULTRA");
    press(7, oj::input::kRight);
    const bool roundMed = c.effects.headlights == 0;
    press(7, oj::input::kLeft);
    check(roundMed && c.effects.headlights == 2, "HEADLIGHTS: RIGHT round to MED, LEFT to ULTRA again");
    s.setCurrentButton("DONE_BUT");
    window.update(host, s, oj::input::kSelect);
    check(host.keptEffects.size() == 1 && host.keptEffects[0].headlights == 2 && host.remade == 3,
          "DONE on the page: HEADLIGHTS ULTRA kept, the programs made again");
    // HI and ULTRA share their shaders: leaving the page after one to the other makes no program again.
    window.enter(host, s);
    s.setCurrentButton(oj::ui::kMoreButton);
    window.update(host, s, oj::input::kSelect);
    press(7, oj::input::kLeft);
    press(7, oj::input::kBack);
    check(c.effects.headlights == 1 && host.remade == 3, "ULTRA to HI: the programs as they are");
}

// The fixed build's REFLECTION ULTRA after the stock's HI (the reflections 3 in reg.cf1, the effects' Reflections 1):
// RIGHT from HI ULTRA, RIGHT again round to OFF (the effects' 0), LEFT from OFF ULTRA; BACK puts both back; DONE
// keeps the effects' Reflections and makes the programs again (the car's effects: the paint's replacement lights the
// cars); RESET takes ULTRA off; without shaders "-".
OJ_INTEGRATION_TEST(GraphicsOptions, ReflectionUltra) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    KeptGraphics kept;
    FakeOptionsHost host;
    oj::GameData data(oj::test::gameFiles().root());
    host.data = &data;
    oj::GraphicsConfig& c = oj::graphicsConfig();
    c.noShaders = 0;
    c.shaders = 1;
    c.reflections = 3;
    c.effects = oj::FixedEffects{};
    const std::shared_ptr<oj::ui::Screen> screen = host.open("opts_graphics");
    if (screen == nullptr) return;
    oj::ui::Screen& s = *screen;
    oj::frontend::GraphicsOptions window;
    window.enter(host, s);
    const auto press = [&](uint32_t event) {
        s.setCurrentButton("REFLECTION_BUT");
        window.update(host, s, event);
        window.update(host, s, 0);
    };
    press(0);
    const std::wstring hi = s.textOf("REFLECTION");
    press(oj::input::kRight);
    check(hi == host.gameTexts().at(oj::kTextOffLowMedHi + 3) && c.reflections == 3 && c.effects.reflections == 1 &&
              s.textOf("REFLECTION") == L"ULTRA" && oj::ultraReflections(c),
          "HI, RIGHT ULTRA: the reflections 3 and the effects' Reflections");
    press(oj::input::kRight);
    check(c.reflections == 0 && c.effects.reflections == 0 &&
              s.textOf("REFLECTION") == host.gameTexts().at(oj::kTextOffLowMedHi),
          "ULTRA, RIGHT round to OFF");
    press(oj::input::kLeft);
    check(c.reflections == 3 && c.effects.reflections == 1, "OFF, LEFT ULTRA");
    press(oj::input::kLeft);
    check(c.reflections == 3 && c.effects.reflections == 0 && s.textOf("REFLECTION") == hi, "ULTRA, LEFT HI");
    press(oj::input::kRight);
    window.update(host, s, oj::input::kBack);
    check(c.reflections == 3 && c.effects.reflections == 0 && host.keptEffects.empty() && host.remade == 0,
          "BACK: HI as it was, nothing kept, the programs as they are");
    window.enter(host, s);
    press(oj::input::kRight);
    s.setCurrentButton("DONE_BUT");
    window.update(host, s, oj::input::kSelect);
    check(host.keptEffects.size() == 1 && host.keptEffects[0].reflections == 1 && host.remade == 1,
          "DONE: the effects' Reflections kept, the programs made again");
    window.enter(host, s);
    s.setCurrentButton("RESET_BUT");
    window.update(host, s, oj::input::kSelect);
    check(c.effects.reflections == 0 && !oj::ultraReflections(c), "RESET: ULTRA off");
    // RESET is the stock's own look every time (the user, 2026-10-29: it took whatever the effects were, so from ULTRA
    // a mixture of the two was left and from HI a CUSTOM): the adapter's values and the fixed build's effects off.
    c.effects.worldLight = 2;
    c.effects.sky = 2;
    c.effects.shadows = 2;
    c.effects.carPaint = 1;
    window.enter(host, s);
    s.setCurrentButton("RESET_BUT");
    window.update(host, s, oj::input::kSelect);
    window.update(host, s, 0);
    check(c.effects == oj::FixedEffects{} && !oj::ultraReflections(c) && s.textOf("SHADOWS") == L"MED",
          "RESET: the same stock's look from any state (MED)");
    c.noShaders = 1;
    c.shaders = 0;
    window.enter(host, s);
    press(oj::input::kRight);
    check(s.textOf("REFLECTION") == L"-" && c.reflections == 0 && c.effects.reflections == 0,
          "without shaders: - and none");
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
