#pragma once
// A race front end window shown as its file has it (closedown, restart_race).

#include "Game/Race/Hud/HudRace.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/WindowManager.h"

#include <memory>
#include <string>

namespace oj {

// A race front end window shown as its file has it, without a frame of its own here (closedown and restart_race: the
// LOADING text, restart_race's animated boxes).
class RaceScreenWindow {
public:
    explicit RaceScreenWindow(ui::WindowManager& windows);
    bool load(const std::string& name);
    bool transitionsDone() const;
    void draw(float seconds, float dt);
    // Elements of the name (case aside) hidden.
    void hide(const std::string& name);

private:
    ui::WindowManager& m_windows;
    std::shared_ptr<ui::Screen> m_screen;
};

}  // namespace oj
