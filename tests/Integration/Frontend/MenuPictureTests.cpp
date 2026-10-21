// Integration tests: the menus' pictures (Game/UI/Components/Menu.cpp) over the game's hudcolourmenu.ui.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/Language.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <vector>

namespace {

using oj::test::check;

// The quads drawn: their texture and their first corner's colour.
struct RecordingDevice : oj::test::NullRenderDevice {
    struct Quad {
        oj::render::TextureId texture;
        uint32_t colour;
    };
    std::vector<Quad> quads;

    void drawQuad(const oj::render::QuadVertex (&corners)[4], oj::render::TextureId texture, oj::render::Blend,
                  bool) override {
        quads.push_back({texture, corners[0].argb});
    }
};

}  // namespace

// COLOURMENU as the HUD colour window fills it (0x0043ED70): ui_dials_off, and ui_dial_choose for the highlighted
// item, a name the game's files do not have. The highlighted dial takes ui_dials_off as the stock's menu does
// (0x005B0964); no dial is drawn as a quad of its colour without a texture.
OJ_INTEGRATION_TEST(MenuPicture, HighPictureMissing) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    RecordingDevice device;
    const oj::Language language = *oj::Language::byName("english");
    oj::ui::Screen s(device, language);
    if (!s.load("hudcolourmenu")) {
        check(false, "hudcolourmenu.ui read from the game's folder");
        return;
    }
    const uint32_t colours[3] = {0xFF2020C0, 0xFFC02020, 0xFF20C020};
    s.clearMenu("COLOURMENU");
    for (size_t i = 0; i < 3; ++i) {
        s.addMenuItem("COLOURMENU", L"", 0.0f, "ui_dials_off", "ui_dial_choose");
        s.setMenuItemColour("COLOURMENU", i, colours[i]);
    }
    s.selectMenuItem("COLOURMENU", 1);
    s.finishTransitions();
    s.draw(1.0f, 0.0f);
    const oj::render::TextureId off = s.texture("ui_dials_off");
    const auto rgb = [](uint32_t c) { return c & 0x00FFFFFFu; };
    bool highlighted = false;
    bool untextured = false;
    for (const RecordingDevice::Quad& q : device.quads) {
        if (rgb(q.colour) == rgb(colours[1]) && q.texture == off) highlighted = true;
        for (uint32_t c : colours) {
            if (q.texture == 0 && rgb(q.colour) == rgb(c)) untextured = true;
        }
    }
    check(off != 0 && s.texture("ui_dial_choose") == 0, "ui_dials_off in the game's files, ui_dial_choose not");
    check(highlighted && !untextured, "the highlighted dial drawn with ui_dials_off, no dial without a texture");
}
