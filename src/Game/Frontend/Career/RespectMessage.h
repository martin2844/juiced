#pragma once
// CMSRespectMsg: a crew's respect change over a window.

#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <string>

namespace oj::frontend {

// The window of the record 0x007227E8.
class RespectMessage {
public:
    void open(CareerHost& host, int crew, float respect, float change, const std::wstring& comment);
    // 0x004DC9E0
    void enter(CareerHost& host, ui::Screen& screen);
    // 0x004DCBD0
    void update(CareerHost& host, ui::Screen& screen, uint32_t event);

private:
    int m_crew = 0;          // +0x0C
    float m_respect = 0.0f;  // +0x10: shown, moves towards the target
    float m_change = 0.0f;   // +0x14: what is left to move
    float m_step = 0.0f;     // +0x18: the change x 0.02
    float m_target = 0.0f;   // +0x1C
    std::wstring m_comment;  // +0x20
    float m_delay = 0.0f;    // +0x30
    float m_timer = 0.0f;    // +0x34
    bool m_out = false;      // +0x38
};

}  // namespace oj::frontend
