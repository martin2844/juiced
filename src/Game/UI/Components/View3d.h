#pragma once
// What a Begin3d element shows: a 3D view drawn in the element's rectangle.

#include "Game/UI/UiFile.h"

#include <functional>
#include <memory>

namespace oj::ui {

// What a Begin3d element shows (Game/Scene/Scene3d): drawn after the element's box, in its rectangle (pixels).
class View3d {
public:
    virtual ~View3d() = default;
    // seconds: the game's clock; dt: the frame time.
    virtual void draw(float x0, float y0, float x1, float y1, float seconds, float dt) = 0;
    // The view's flag 0x10: given to the 3d elements of a window whose name holds "HUD_" (case aside; 0x005AB3DC).
    virtual void setRound(bool) {}
};
using View3dFactory = std::function<std::unique_ptr<View3d>(const Element&)>;

}  // namespace oj::ui
