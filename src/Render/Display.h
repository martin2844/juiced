#pragma once
// The fixed build's wide screens (docs/fixes.md, Added): the shape the picture takes on the screen, the 4:3 frame the
// interface's pictures and texts keep the size of, the zone the menus are laid out across, the zone the race's HUD
// keeps to and the widening of the 3D views (Hor+).

#include <cstdint>

namespace oj::render {

// The settings' widescreen (JuicedConfig's box, the registry's `widescreen`): 0 the stock's (the 4:3 picture
// stretched over any screen; the stock's own wide screen, 1, never set here), 1 the screen's own shape (its pixels
// square), 2 .. 6 a picture of 5:4, 16:10, 16:9, 21:9 or 32:9 (a screen showing another mode's pixels).
enum class AspectMode : uint32_t {
    Stock = 0,
    Auto = 1,
    Ratio5x4 = 2,
    Ratio16x10 = 3,
    Ratio16x9 = 4,
    Ratio21x9 = 5,
    Ratio32x9 = 6,
};
// A settings' value past the list is the screen's own shape.
AspectMode aspectModeOf(uint32_t setting);

// The picture on a back buffer of width x height pixels.
struct Display {
    float width = 640.0f;
    float height = 480.0f;
    bool stock = true;         // the stock's: no frame, no zone, no widening
    float aspect = 4.0f / 3;   // the picture's width over its height as the screen shows it
    // The 4:3 frame: the largest 4:3 part of the picture, centred (pixels; a 5:4 picture's is shorter than it).
    float frameX = 0.0f, frameY = 0.0f, frameW = 640.0f, frameH = 480.0f;
    // The race HUD's zone across: the picture's width, at most a 21:9 one's, centred (pixels).
    float zoneX = 0.0f, zoneW = 640.0f;
    // The menus' zone across: the picture's width, at most a 16:9 one's, centred; the frame's on a picture narrower
    // than 4:3 (pixels).
    float menuX = 0.0f, menuW = 640.0f;
    // The 3D views' widening (the tangents of the half angles): across (Hor+) the picture's width over a 4:3 one's of
    // its height, the height's angle kept; down (Vert+) for a picture narrower than 4:3 (5:4), the angle across kept.
    float widen = 1.0f;
    float widenY = 1.0f;
};
Display displayOf(AspectMode mode, uint32_t width, uint32_t height);

// The race camera's angle across at rest (the stock's 1.1623551 radians on a 4:3 picture) on this picture, widened
// and with the view's scale (the tangents' factor), in degrees; and the scale of an angle across.
float restFovDegrees(const Display& d, float scale);
float fovScaleOf(const Display& d, float degrees);

// The game's mode (set once the settings are read) and the picture on the device's back buffer.
void setAspectMode(AspectMode mode);
AspectMode aspectMode();
Display display(uint32_t width, uint32_t height);

}  // namespace oj::render
