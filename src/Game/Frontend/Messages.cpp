// The message windows (msgbox, msgbox_network) and their answers.

#include "Game/Frontend/Frontend.h"
#include "Game/Frontend/FrontendInternal.h"

#include "Core/Base/Log.h"

namespace oj {

using namespace frontend;

// The message window (0x005A80E0): a message replaces the one open; its number (+0x108) 1 .. 9998, then 1 again;
// msgbox opened by the type (0x005A7170, Game/UI/MessageWindow.h), its state entered (0x005A9180) and its font
// (small); the windows under it wait for Continue and YesNo (+0x4C).
void Frontend::message(ui::MessageType type, const std::wstring& text, bool small, int* answer, bool startNo) {
    m_messageNumber = m_messageNumber + 1 < 9999 ? m_messageNumber + 1 : 1;
    Message m;
    m.screen = makeScreen("msgbox");
    m.state = {type, answer, nullptr, startNo};
    m.modal = type == ui::MessageType::Continue || type == ui::MessageType::YesNo;
    ui::openMessage(*m.screen, type, text);
    ui::enterMessage(*m.screen, startNo);
    ui::messageFont(*m.screen, small);
    log::info("frontend: message {} \"{}\"", static_cast<int>(type), toUtf8(text));
    m_message = std::move(m);
}

// 0x005A8330: with a message up its title element takes the text (the effect held).
void Frontend::messageTitle(const std::wstring& text) {
    if (m_message && m_message->screen) m_message->screen->setText("title", text);
}

// 0x005A9200 for msgbox: SELECT answers and closes it (CLICK).
void Frontend::updateMessage(uint32_t event) {
    Message& m = *m_message;
    if (ui::messageEvent(*m.screen, m.state, false, true, event) == ui::MessageClose::None) return;
    sound("CLICK");
    m.close = true;
}

int32_t Frontend::netMessage(ui::MessageType type, const std::wstring& text, int32_t* answer) {
    if (m_saveBusy != 0) return 0;
    Message m;
    m.screen = makeScreen("msgbox_network");
    m.state = {type, answer, nullptr, false};
    ui::openMessage(*m.screen, type, text);
    log::info("frontend: network message {} \"{}\"", static_cast<int>(type), toUtf8(text));
    m_netMessage = std::move(m);
    return m_messageNumber;
}

// 0x005A9200 for msgbox_network: a note's SELECT closes it (Continue) or the message (ContinueModeless: +0x48, the
// note stays);
// a question's SELECT and BACK answer by +0x28 and close it.
void Frontend::updateNetMessage(uint32_t event) {
    const ui::MessageClose close = ui::messageEvent(*m_netMessage->screen, m_netMessage->state, true, false, event);
    if (close == ui::MessageClose::None) return;
    sound("CLICK");
    if (close == ui::MessageClose::Network) {
        m_netMessage->close = true;
    } else if (m_message) {
        m_message->close = true;
    }
}

}  // namespace oj
