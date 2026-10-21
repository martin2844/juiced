// The front end's drawing of its windows, its notes and the screen's fades.

#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/FrontendInternal.h"

#include <algorithm>

namespace oj {

using namespace frontend;

void Frontend::draw() {
    // 0x005A9D20 for each window of the stack, then the window over the windows (+0x30) and the message (+0x44): with
    // a message up the others draw only their FRAME_ elements ([0x0071E710] set, as it is but while CMSRespectMsg is
    // up) or nothing.
    // With the note of a lost controller (+0x34) or the network's message (+0x3C) up every element draws (0x005A9D48).
    const ui::Screen::Under under = !m_message || m_critical || m_netMessage ? ui::Screen::Under::kAll
                                    : m_framesUnderMessage                   ? ui::Screen::Under::kFramesOnly
                                                                             : ui::Screen::Under::kNone;
    if (m_screen) m_screen->setUnderMessage(under);
    for (Popup& popup : m_popups) popup.screen->setUnderMessage(under);
    if (m_overlay) m_overlay->screen->setUnderMessage(under);
    const bool waiting = m_nextMode == SwitchMode::ReplaceNow;
    if (m_screen) m_screen->setSwitchWaiting(waiting);
    for (Popup& popup : m_popups) popup.screen->setSwitchWaiting(waiting);
    if (m_overlay) m_overlay->screen->setSwitchWaiting(waiting);
    if (m_message) m_message->screen->setSwitchWaiting(waiting);
    if (m_netMessage) m_netMessage->screen->setSwitchWaiting(waiting);
    if (m_critical) m_critical->screen->setSwitchWaiting(waiting);
    if (m_screen) {
        m_screen->update(m_frameTime, m_popups.empty() ? m_scroll : 0);
        m_screen->draw(m_clock, m_frameTime);
        // The window's draw of its state (the record's method +0x08).
        if (is(m_state, "CMSStats")) m_stats.draw(*this, *m_screen);
        if (is(m_state, "car_rolling")) m_rolling.draw(*this, *m_screen);
    }
    for (Popup& popup : m_popups) {
        popup.screen->update(m_frameTime, &popup == &m_popups.back() ? m_scroll : 0);
        popup.screen->draw(m_clock, m_frameTime);
    }
    if (m_overlay) {
        m_overlay->screen->update(m_frameTime);
        m_overlay->screen->draw(m_clock, m_frameTime);
    }
    if (m_message) {
        m_message->screen->update(m_frameTime);
        m_message->screen->draw(m_clock, m_frameTime);
    }
    // The front end's black over the whole screen (+0x10C), clearing by the frame time.
    if (m_screenFade > 0.0f) {
        const auto alpha = static_cast<uint32_t>(x87::chop(m_screenFade * 255.0f)) & 0xFF;
        const float w = static_cast<float>(m_device.width());
        const float h = static_cast<float>(m_device.height());
        const render::QuadVertex corners[4] = {
            {0.0f, 0.0f, 0.0f, 0.0f, alpha << 24},
            {w, 0.0f, 1.0f, 0.0f, alpha << 24},
            {0.0f, h, 0.0f, 1.0f, alpha << 24},
            {w, h, 1.0f, 1.0f, alpha << 24},
        };
        m_device.drawQuad(corners, 0, render::Blend::Alpha);
        m_screenFade = std::clamp(m_screenFade - m_frameTime, 0.0f, 1.0f);
    }
    // 0x005A6D15: over the black the network's message (+0x3C), then the note of a lost controller (+0x34).
    if (m_netMessage) {
        m_netMessage->screen->update(m_frameTime);
        m_netMessage->screen->draw(m_clock, m_frameTime);
    }
    if (m_critical) {
        m_critical->screen->update(m_frameTime);
        m_critical->screen->draw(m_clock, m_frameTime);
    }
    // 0x0046C80D: after the window manager's frame the note of an invitation while the game's friends are made (not
    // on the frame the network's watch went back from the network, 0x0046C64E).
    if (m_screen && !m_presenceSkipped) frontend::drawPresenceIcon(m_device, *m_screen, topWindowName(), false);
}

}  // namespace oj
