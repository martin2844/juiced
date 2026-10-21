// closedown's and restart_race's window.

#include "Game/Race/Hud/RaceScreenWindow.h"
#include "Game/Race/Hud/HudInternal.h"

namespace oj {

using namespace hud;

RaceScreenWindow::RaceScreenWindow(ui::WindowManager& windows) : m_windows(windows) {}

void RaceScreenWindow::hide(const std::string& name) {
    if (m_screen == nullptr) return;
    for (int e = 0; e < m_screen->size(); ++e) {
        if (_stricmp(m_screen->nameOf(e).c_str(), name.c_str()) == 0) m_screen->setHidden(e, true);
    }
}

bool RaceScreenWindow::load(const std::string& name) {
    m_screen = m_windows.open(name);
    if (!m_screen->loaded()) {
        m_screen.reset();
        return false;
    }
    return true;
}

bool RaceScreenWindow::transitionsDone() const {
    return m_screen == nullptr || m_screen->transitionsDone();
}

void RaceScreenWindow::draw(float seconds, float dt) {
    if (m_screen == nullptr) return;
    m_screen->update(dt);
    m_screen->draw(seconds, dt);
}

}  // namespace oj
