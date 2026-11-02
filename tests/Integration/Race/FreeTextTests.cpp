// Integration tests: the race front end's own texts (Screen::drawFreeText, 0x005186C0) in the game's fonts.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/Language.h"
#include "Game/UI/UiScreen.h"

#include <algorithm>
#include <vector>

namespace {

using oj::test::check;

// The quads drawn: every corner's y.
struct RecordingDevice : oj::test::NullRenderDevice {
    std::vector<float> ys;

    void drawQuad(const oj::render::QuadVertex (&corners)[4], oj::render::TextureId, oj::render::Blend,
                  bool) override {
        for (const oj::render::QuadVertex& c : corners) ys.push_back(c.y);
    }
};

}  // namespace

// The labels over the cars (0x004BF8B0's "!", 0x004BF320's crew and member) and the network's names (0x00456BA0)
// give 0x005186C0 the place projected above the car as the pen: its y is the baseline, the glyphs stand on it. The
// port drew the line's top there (the font's ascent lower), a nervous racer's "!" inside its car.
OJ_INTEGRATION_TEST(FreeText, StandsOnItsPlace) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    RecordingDevice device;
    const oj::Language language = *oj::Language::byName("english");
    oj::ui::Screen s(device, language);
    s.drawFreeText("goodtime11", L"!", 100.0f, 200.0f, 0xFFFFFFFFu);
    if (device.ys.empty()) {
        check(false, "the \"!\" of goodtime11 drawn");
        return;
    }
    const float top = *std::min_element(device.ys.begin(), device.ys.end());
    const float bottom = *std::max_element(device.ys.begin(), device.ys.end());
    // goodtime11's "!" at 640 x 480: 15 pixels high, 13 above the baseline (its edge's two below).
    check(top == 187.0f && bottom == 202.0f, "the \"!\" above its place, its foot on the baseline");
}
