#pragma once
// A pop-up window over a window.

#include "Game/UI/UiScreen.h"

#include <memory>
#include <string>

namespace oj::frontend {

// A window over the current one (switch mode 5), taken off by mode 6.
struct Popup {
    std::string state;
    std::shared_ptr<ui::Screen> screen;
};

}  // namespace oj::frontend
