// CMSRespectMsg: a crew's respect change over a window.

#include "Game/Frontend/Career/RespectMessage.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace oj::frontend {

void RespectMessage::open(CareerHost& host, int crew, float respect, float change, const std::wstring& comment) {
    *this = RespectMessage{};
    m_crew = crew;
    m_respect = respect;
    m_change = change;
    m_step = change * 0.02f;
    m_target = respect + change;
    m_comment = comment;
    host.request("CMSRespectMsg", SwitchMode::Push);
}

// CMSRespectMsg (0x004DC9E0): the crew's leader, the title by the change (533 up, 534 down), the comment, the
// respect on the meter (1500 fills it), logo the crew's logo of the flyers (career +0x00).
void RespectMessage::enter(CareerHost& host, ui::Screen& screen) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    screen.setMultiline("comment", true);
    if (data != nullptr && m_crew >= 0 && m_crew < 9) {
        screen.setTexture("logo", data->career->flyers.crews[static_cast<size_t>(m_crew)].logo);
        const auto id = static_cast<size_t>(data->career->crews[static_cast<size_t>(m_crew)].id);
        if (id < std::size(kLeaderPictures)) screen.setTexture("leader", kLeaderPictures[id]);
    }
    screen.setText("title", texts.at(m_change <= 0.0f ? kTextRespectDown : kTextRespectIncrease));
    screen.setText("comment", m_comment);
    m_delay = 1.5f;
    m_timer = 2.5f;
    m_out = false;
    screen.setMeter("meter", std::min(1.0f, m_respect * 0.00066666666f), true);
}

// 0x004DCBD0: after 1.5 s the respect moves to the new value, 10 a frame while more than 50 is left, else 0.5 a
// frame (with the sound SWOOSH each frame); then the window waits 2.5 s (going out at 0.75 s) or SELECT / BACK.
void RespectMessage::update(CareerHost& host, ui::Screen& screen, uint32_t event) {
    if (((event == input::kSelect || event == input::kBack) && m_change == 0.0f) || m_timer < 0.0f) {
        host.sound("CLICK");
        host.request("", SwitchMode::Pop);
    }
    if (m_delay >= 0.0f) {
        m_delay -= host.frameTime();
    } else if (m_change == 0.0f) {
        if (m_timer < 0.75f && !m_out) {
            screen.transitionsOut();
            m_out = true;
        }
        m_respect = m_target;
        m_timer -= host.frameTime();
    } else {
        host.sound("SWOOSH");
        if (m_change >= 0.0f) {
            const float step = m_change <= 50.0f ? 0.5f : 10.0f;
            m_change -= step;
            m_respect += step;
        } else {
            const float step = m_change >= -50.0f ? 0.5f : 10.0f;
            m_change += step;
            m_respect -= step;
        }
        if (std::fabs(m_change) < 0.6f) m_change = 0.0f;
    }
    screen.setMeter("meter", std::min(1.0f, m_respect * 0.00066666666f));
    const auto change = static_cast<int>(m_change);
    screen.setText("change", toWide(m_change <= 0.0f ? std::format("{}", change) : std::format("+{}", change)));
    const int language = host.language().index;
    screen.setText("value", toWide(groupThousands(static_cast<int>(std::max(m_respect, 0.0f)), language)));
}

}  // namespace oj::frontend
