// The race front end's message windows.

#include "Game/Race/Hud/RaceMessage.h"
#include "Game/Race/Hud/HudInternal.h"

#include "Core/Base/Text.h"
#include "Core/Input/Input.h"

#include <cstring>

namespace oj {

using namespace hud;

RaceMessage::RaceMessage(ui::WindowManager& windows) : m_windows(windows) {}

bool RaceMessage::open(ui::MessageType type, const std::wstring& text, int32_t* answer, bool startNo,
                       const char* window) {
    m_screen = m_windows.open(window);
    if (!m_screen->loaded()) {
        m_screen.reset();
        return false;
    }
    m_state = {type, answer, nullptr, startNo};
    m_network = _stricmp(window, "msgbox_network") == 0;
    m_entered = _stricmp(window, "msgbox") == 0;
    ui::openMessage(*m_screen, type, text);
    if (m_entered) {
        ui::enterMessage(*m_screen, startNo);
        ui::messageFont(*m_screen, false);
    }
    log::info("race: {} {} \"{}\"", window, static_cast<int>(type), toUtf8(text));
    return true;
}

ui::MessageClose RaceMessage::event(uint32_t uiEvent, uint32_t secondEvent) {
    if (m_screen == nullptr) return ui::MessageClose::None;
    ui::Screen& s = *m_screen;
    if (uiEvent != input::kNone) s.finishTransitions();
    s.menuEvent(uiEvent, secondEvent);
    const ui::MessageClose close = ui::messageEvent(s, m_state, m_network, m_entered, uiEvent);
    if (close != ui::MessageClose::None && s.sound) s.sound("CLICK");
    return close;
}

void RaceMessage::draw(float seconds, float dt) {
    if (m_screen == nullptr) return;
    m_screen->update(dt);
    m_screen->draw(seconds, dt);
}

}  // namespace oj
