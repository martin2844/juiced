// Integration tests: the fixed build's wide screens in the game's windows (Game/UI/WideScreen, Render/Display).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/Language.h"
#include "Game/UI/UiScreen.h"
#include "Render/Display.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using oj::test::check;
using oj::render::AspectMode;

// A back buffer of the size asked.
class SizedDevice : public oj::test::NullRenderDevice {
public:
    SizedDevice(uint32_t w, uint32_t h) : m_w(w), m_h(h) {}
    uint32_t width() const override { return m_w; }
    uint32_t height() const override { return m_h; }

private:
    uint32_t m_w, m_h;
};

// The quads drawn: their corners' extents and their colour.
class RecordingDevice : public SizedDevice {
public:
    using SizedDevice::SizedDevice;
    struct Quad {
        float x0, y0, x1, y1;
        uint32_t argb;
    };
    std::vector<Quad> quads;

    void drawQuad(const oj::render::QuadVertex (&c)[4], oj::render::TextureId, oj::render::Blend, bool) override {
        quads.push_back({c[0].x, c[0].y, c[3].x, c[3].y, c[0].argb});
    }
};

// The game's mode for the test, the stock's after it.
struct ModeFor {
    explicit ModeFor(AspectMode m) { oj::render::setAspectMode(m); }
    ~ModeFor() { oj::render::setAspectMode(AspectMode::Stock); }
};

struct Rect {
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
};

Rect rectOf(const oj::ui::Screen& s, const char* element) {
    Rect r;
    s.pixelRect(element, r.x0, r.y0, r.x1, r.y1);
    return r;
}

bool near(float a, float b) {
    return std::fabs(a - b) < 0.6f;
}

}  // namespace

// The race HUD on 16:9 (1920 x 1080): the 4:3 frame 1440 wide; the speedometer held to the right edge and square, the
// map to the left, the count in the frame's middle, the crew's orders' pictures with their texts.
OJ_INTEGRATION_TEST(WideScreen, HudAnchors) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    SizedDevice device(1920, 1080);
    oj::ui::Screen s(device, *oj::Language::byName("english"));
    if (!s.load("hud_race")) {
        check(false, "hud_race read");
        return;
    }
    const Rect dial = rectOf(s, "REVSBG1");
    check(near(dial.x0, 480.0f + 0.721f * 1440.0f) && near(dial.x1 - dial.x0, dial.y1 - dial.y0),
          "the speedometer at the right edge, square");
    check(near(rectOf(s, "MAPBG").x0, 0.076f * 1440.0f), "the map at the left edge");
    check(near(rectOf(s, "COUNTDOWN1").x0, 240.0f + 0.461f * 1440.0f), "the count in the frame's middle");
    check(near(rectOf(s, "DPADCONTROL01").x0, 240.0f + 0.273f * 1440.0f) &&
              near(rectOf(s, "DPAD1").x0, 240.0f + 0.341f * 1440.0f),
          "the crew's orders' pictures and texts together in the middle");
}

// 32:9 (3840 x 1080): the HUD within the middle 21:9 zone (2520 wide from 660).
OJ_INTEGRATION_TEST(WideScreen, HudZone) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    SizedDevice device(3840, 1080);
    oj::ui::Screen s(device, *oj::Language::byName("english"));
    if (!s.load("hud_race")) return;
    check(near(rectOf(s, "REVSBG1").x0, 660.0f + 2520.0f - 1440.0f + 0.721f * 1440.0f) &&
              near(rectOf(s, "MAPBG").x0, 660.0f + 0.076f * 1440.0f),
          "32:9: the speedometer and the map at the 21:9 zone's edges");
}

// A picture narrower than 4:3 (5:4, 1280 x 1024): the frame 960 high; the speedometer (low) to the bottom, the laps
// (high) to the top, the count in the middle.
OJ_INTEGRATION_TEST(WideScreen, HudTall) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    SizedDevice device(1280, 1024);
    oj::ui::Screen s(device, *oj::Language::byName("english"));
    if (!s.load("hud_race")) return;
    check(near(rectOf(s, "REVSBG1").y0, 64.0f + 0.658f * 960.0f) && near(rectOf(s, "LAP1").y0, 0.071f * 960.0f) &&
              near(rectOf(s, "COUNTDOWN1").y0, 32.0f + 0.258f * 960.0f),
          "5:4: the speedometer at the bottom, the laps at the top, the count in the middle");
}

// A menu (opts_graphics, opts_bg) on 16:9: laid out across the whole width (DONE's button at 0.112 of 1920), its
// background box across it, the OK hint's icon (SELECTA, 0.509 .. 0.559) the frame's size (72 pixels) in the middle
// of its box; on 21:9 (2560 x 1080) across the 16:9 zone in the middle (1920 from 320).
OJ_INTEGRATION_TEST(WideScreen, MenuSpread) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    SizedDevice device(1920, 1080);
    SizedDevice wide(2560, 1080);
    oj::ui::Screen s(device, *oj::Language::byName("english"));
    oj::ui::Screen hints(device, *oj::Language::byName("english"));
    oj::ui::Screen sWide(wide, *oj::Language::byName("english"));
    if (!s.load("opts_graphics") || !hints.load("opts_bg") || !sWide.load("opts_graphics")) return;
    const Rect back = rectOf(s, "PICBG");
    check(near(rectOf(s, "DONE_BUT").x0, 0.112f * 1920.0f), "the buttons across the width");
    check(near(back.x0, 0.0f) && near(back.x1, 1920.0f) && near(back.y0, 0.177f * 1080.0f),
          "the background box across the width");
    const Rect icon = rectOf(hints, "SELECTA");
    check(near(icon.x1 - icon.x0, 72.0f) && near((icon.x0 + icon.x1) * 0.5f, 0.534f * 1920.0f) &&
              near(icon.y1 - icon.y0, 0.067f * 1080.0f) && near(rectOf(hints, "SEL_TEXT").x0, 0.564f * 1920.0f),
          "the hint's icon in its shape in the middle of its box, its text across");
    check(near(rectOf(sWide, "DONE_BUT").x0, 320.0f + 0.112f * 1920.0f) && near(rectOf(sWide, "PICBG").x0, 0.0f),
          "21:9: across the 16:9 zone, the background to the edges");
}

// A picture's overlay on 16:9: cmscalendar's event flyer (FLYERMAIN, 0.548 .. 0.909) keeps its shape, its middle at
// 0.7285 of 1920, and its texts go with it (FLYERLAPS at 0.587 of the frame's 1440 from the flyer's place); the HUD's
// dial in hudcolourmenu stays in the frame with its texts.
OJ_INTEGRATION_TEST(WideScreen, PictureHolders) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    SizedDevice device(1920, 1080);
    oj::ui::Screen calendar(device, *oj::Language::byName("english"));
    oj::ui::Screen colours(device, *oj::Language::byName("english"));
    if (!calendar.load("cmscalendar") || !colours.load("hudcolourmenu")) return;
    const float offset = 0.7285f * 1920.0f - 0.7285f * 1440.0f;
    const Rect flyer = rectOf(calendar, "FLYERMAIN");
    check(near(flyer.x0, 0.548f * 1440.0f + offset) && near(flyer.x1 - flyer.x0, 0.361f * 1440.0f) &&
              near(rectOf(calendar, "FLYERLAPS").x0, 0.587f * 1440.0f + offset),
          "the flyer in its shape, its texts with it");
    check(near(rectOf(colours, "REVSBG").x0, 240.0f + 0.406f * 1440.0f) &&
              near(rectOf(colours, "SPEED").x0, 240.0f + 0.508f * 1440.0f),
          "hudcolourmenu's dial and texts in the frame");
}

// Small pictures holding others on 16:9: cmscrew's crew slot (BORDER3, 0.122 .. 0.263) and the plate under its
// portrait (EMPTY1, 0.130 .. 0.255) the frame's size in the slot's middle; split_selcar's two hint arrows of a size
// (RIGHTARROW1 at 0.466, LEFTARROW2 at 0.486) in their order; the calendar's day cursor (SELECT, ui_calendar_select)
// across as its days.
OJ_INTEGRATION_TEST(WideScreen, SmallHolders) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    SizedDevice device(1920, 1080);
    oj::ui::Screen crew(device, *oj::Language::byName("english"));
    oj::ui::Screen split(device, *oj::Language::byName("english"));
    oj::ui::Screen calendar(device, *oj::Language::byName("english"));
    if (!crew.load("cmscrew") || !split.load("split_selcar") || !calendar.load("cmscalendar")) return;
    const float middle = (0.122f + 0.141f * 0.5f) * 1920.0f;
    const Rect slot = rectOf(crew, "BORDER3");
    const Rect plate = rectOf(crew, "EMPTY1");
    check(near(slot.x1 - slot.x0, 0.141f * 1440.0f) && near(plate.x1 - plate.x0, 0.125f * 1440.0f) &&
              near((plate.x0 + plate.x1) * 0.5f, middle) &&
              near(plate.x0, middle - (0.1925f - 0.130f) * 1440.0f),
          "the crew slot's plate the frame's size with its slot");
    check(rectOf(split, "RIGHTARROW1").x0 < rectOf(split, "LEFTARROW2").x0, "the hint's arrows in their order");
    const Rect cursor = rectOf(calendar, "SELECT");
    check(near(cursor.x1 - cursor.x0, 0.063f * 1920.0f), "the day cursor across as its days");
}

// viewhiscores' CAR (H_COL5, right-aligned over the left-aligned cars, 0.730 .. 0.918) on 16:9: from the column's
// start (0.730 of 1920) its 4:3 width (0.188 of 1440); TIME/SCORE (left-aligned) at its column's start.
OJ_INTEGRATION_TEST(WideScreen, ColumnHeads) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    SizedDevice device(1920, 1080);
    oj::ui::Screen s(device, *oj::Language::byName("english"));
    if (!s.load("viewhiscores")) return;
    const Rect car = rectOf(s, "H_COL5");
    check(near(car.x0, 0.730f * 1920.0f) && near(car.x1, 0.730f * 1920.0f + 0.188f * 1440.0f) &&
              near(rectOf(s, "H_COL4").x0, 0.541f * 1920.0f) && near(rectOf(s, "M_COL4").x0, 0.730f * 1920.0f),
          "the cars' header from its column's start, the 4:3 width");
}

// The pause (hud_pause) on 16:9 laid out as the menus, its menu to the left of where the 4:3 frame put it (off the
// car in the middle): 0.106 of 1920.
OJ_INTEGRATION_TEST(WideScreen, Pause) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    SizedDevice device(1920, 1080);
    oj::ui::Screen s(device, *oj::Language::byName("english"));
    if (!s.load("hud_pause")) return;
    check(near(rectOf(s, "MENU").x0, 0.106f * 1920.0f), "the pause's menu across the width, left");
}

// A full screen background texture (link_lobby's ui_custom_static) covers the 16:9 screen in its 4:3 shape (1920 x
// 1440, cut 180 above and below); a loading picture (loader's ui_background_01) covers it too, but on 21:9 (2560 x
// 1080) no wider than 16:9 (1920 in the middle).
OJ_INTEGRATION_TEST(WideScreen, CoverAndPicture) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    SizedDevice device(1920, 1080);
    SizedDevice wide(2560, 1080);
    oj::ui::Screen lobby(device, *oj::Language::byName("english"));
    oj::ui::Screen loader(device, *oj::Language::byName("english"));
    oj::ui::Screen loaderWide(wide, *oj::Language::byName("english"));
    if (!lobby.load("link_lobby") || !loader.load("loader") || !loaderWide.load("loader")) return;
    const Rect bg = rectOf(lobby, "FRAME_BG");
    check(near(bg.x0, 0.0f) && near(bg.x1, 1920.0f) && near(bg.y0, -180.0f) && near(bg.y1, 1260.0f),
          "the background covering the screen in its 4:3 shape");
    const Rect picture = rectOf(loader, "FRAME_BG01");
    const Rect pictureWide = rectOf(loaderWide, "FRAME_BG01");
    check(near(picture.x0, 0.0f) && near(picture.x1, 1920.0f) && near(picture.y0, -180.0f),
          "the loading picture covering 16:9");
    check(near(pictureWide.x0, 320.0f) && near(pictureWide.x1, 2240.0f) && near(pictureWide.y0, -180.0f),
          "the loading picture on 21:9 no wider than 16:9");
}

// The city map's districts' bar (cmsrespect's MENU, a horizontal menu across the window, its box black) on 21:9
// (2560 x 1080): its box across the whole screen, as the boxes at the window's edges.
OJ_INTEGRATION_TEST(WideScreen, MenuBoxAtEdges) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    RecordingDevice device(2560, 1080);
    oj::ui::Screen s(device, *oj::Language::byName("english"));
    if (!s.load("cmsrespect")) return;
    s.finishTransitions();
    s.update(0.01f);
    s.draw(1.0f, 0.0f);
    bool bar = false;
    for (const RecordingDevice::Quad& q : device.quads) {
        bar = bar || (q.argb == 0xFF000000u && near(q.x0, 0.0f) && near(q.x1, 2560.0f) && near(q.y0, 0.806f * 1080.0f));
    }
    check(bar, "the districts' bar across the screen");
}

// The movies: the main menu's looped background (hub_01) covers a 16:9 screen; the THQ logo (splash, played once)
// covers it too, but on 32:9 (3840 x 1080) no wider than 16:9 (1920 in the middle); a tutorial (less than the whole
// window) stays in the frame.
OJ_INTEGRATION_TEST(WideScreen, Movies) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    SizedDevice device(1920, 1080);
    SizedDevice wide(3840, 1080);
    oj::ui::Screen menu(device, *oj::Language::byName("english"));
    oj::ui::Screen splash(device, *oj::Language::byName("english"));
    oj::ui::Screen splashWide(wide, *oj::Language::byName("english"));
    oj::ui::Screen tutorial(device, *oj::Language::byName("english"));
    if (!menu.load("menu") || !splash.load("splash") || !splashWide.load("splash") || !tutorial.load("vid_tutorial")) {
        return;
    }
    const Rect loop = rectOf(menu, "FRAME_HUB");
    const Rect logo = rectOf(splash, "SPLASH");
    const Rect logoWide = rectOf(splashWide, "SPLASH");
    check(near(loop.x0, 0.0f) && near(loop.x1, 1920.0f) && near(loop.y0, -180.0f), "the looped background covering");
    check(near(logo.x0, 0.0f) && near(logo.x1, 1920.0f) && near(logo.y0, -180.0f), "the logo covering 16:9");
    check(near(logoWide.x0, 960.0f) && near(logoWide.x1, 2880.0f) && near(logoWide.y0, -180.0f),
          "the logo on 32:9 no wider than 16:9");
    check(near(rectOf(tutorial, "MOVIE").x0, 240.0f), "the tutorial in the frame");
}

// A menu's pieces at the window's edges on 16:9: the top banner's halves (FRAME_CORNER_BLUETOP 0 .. 0.505,
// FRAME_CORNER_REDTOP 0.5 .. 1.005) meet across the whole screen, carsel's bar filler (BLUEBOX, 0 .. 0.111) reaches the
// left edge; an icon parked past the window (cmsracesetup's CAR1ICON at 1.028) stays past the screen's.
OJ_INTEGRATION_TEST(WideScreen, EdgePieces) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    SizedDevice device(1920, 1080);
    oj::ui::Screen menu(device, *oj::Language::byName("english"));
    oj::ui::Screen carsel(device, *oj::Language::byName("english"));
    oj::ui::Screen setup(device, *oj::Language::byName("english"));
    if (!menu.load("menu") || !carsel.load("carsel") || !setup.load("cmsracesetup")) return;
    const Rect blue = rectOf(menu, "FRAME_CORNER_BLUETOP");
    const Rect red = rectOf(menu, "FRAME_CORNER_REDTOP");
    check(near(blue.x0, 0.0f) && near(blue.x1, 0.505f * 1920.0f) && near(red.x0, 0.5f * 1920.0f) &&
              near(red.x1, 1920.0f),
          "the banner's halves across the screen");
    const Rect filler = rectOf(carsel, "BLUEBOX");
    check(near(filler.x0, 0.0f) && near(filler.x1, 0.111f * 1920.0f), "the bar's filler at the left edge");
    check(rectOf(setup, "CAR1ICON").x0 >= 1920.0f, "the parked icon past the screen's edge");
}

// A picture taller than 4:3 (5:4, 1280 x 1024, the frame 960 high from 32): the main menu's black strip over its top
// (0 .. 0.042, a plain bar across the window) reaches the screen's top, so the background movie does not show above it
// (the tester, 1280 x 1024).
OJ_INTEGRATION_TEST(WideScreen, TallMenuTop) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const ModeFor mode(AspectMode::Auto);
    RecordingDevice device(1280, 1024);
    oj::ui::Screen s(device, *oj::Language::byName("english"));
    if (!s.load("menu")) return;
    s.finishTransitions();
    s.update(0.01f);
    s.draw(1.0f, 0.0f);
    bool strip = false;
    for (const RecordingDevice::Quad& q : device.quads) {
        strip = strip || (q.argb == 0xFF000000u && near(q.x0, 0.0f) && near(q.x1, 1280.0f) && near(q.y0, 0.0f) &&
                          near(q.y1, 32.0f + 0.042f * 960.0f));
    }
    check(strip, "the black strip from the screen's top to its place in the frame");
}

// The stock's mode: the stock's whole screen (the speedometer at 0.721 of 1920).
OJ_INTEGRATION_TEST(WideScreen, Stock) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    SizedDevice device(1920, 1080);
    oj::ui::Screen s(device, *oj::Language::byName("english"));
    if (!s.load("hud_race")) return;
    check(near(rectOf(s, "REVSBG1").x0, 0.721f * 1920.0f), "stock: stretched as the stock's");
}
