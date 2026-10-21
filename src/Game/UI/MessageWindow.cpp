// The message windows' state: what a type shows, the font, SELECT and BACK.

#include "Game/UI/MessageWindow.h"

#include "Core/Input/Input.h"
#include "Game/UI/UiScreen.h"

#include <cstring>
#include <initializer_list>

namespace oj::ui {

namespace {

// Every element of the name (case aside) hidden, and every element whose name holds a part (case counts).
void hide(Screen& s, std::initializer_list<const char*> names, std::initializer_list<const char*> parts) {
    for (const char* name : names) {
        for (int e = 0; e < s.size(); ++e) {
            if (_stricmp(s.nameOf(e).c_str(), name) == 0) s.setHidden(e, true);
        }
    }
    for (const char* part : parts) s.hideContaining(part);
}

// 0x005AF920: the shown item of the name (case aside) selected; nothing without one.
void selectNamed(Screen& s, const char* menu, const char* item) {
    for (size_t i = 0; i < s.menuSize(menu); ++i) {
        if (_stricmp(s.menuItemCommand(menu, i).c_str(), item) == 0) {
            s.selectMenuItem(menu, i);
            return;
        }
    }
}

}  // namespace

void openMessage(Screen& s, MessageType type, const std::wstring& text) {
    for (int e = 0; e < s.size(); ++e) s.setHidden(e, false);
    switch (type) {
    case MessageType::Note:
        hide(s, {"MSG_CANCEL", "MSG_CONTINUE", "MSG_CONTINUE_ICON", "PROGRESS_BAR", "PROGRESS_BDR"},
             {"MSG_MENU", "SEL", "BACK_", "MSG_BOTTOMPANEL", "MSG_ANI", "ANIM"});
        break;
    case MessageType::Continue:
    case MessageType::ContinueModeless:
        hide(s, {"MSG_CANCEL", "PROGRESS_BAR", "PROGRESS_BDR"},
             {"MSG_MENU", "SEL_", "BACK_", "MSG_BOTTOMPANEL", "MSG_ANI", "ANIM"});
        break;
    case MessageType::YesNo:
    case MessageType::YesNoModeless:
        hide(s, {"MSG_CANCEL", "MSG_CONTINUE", "MSG_CONTINUE_ICON", "PROGRESS_BAR", "PROGRESS_BDR", "SEL_TEXT2",
                 "SEL_RECOVERY"},
             {"ANIM"});
        break;
    case MessageType::Progress:
        hide(s, {"MSG_CANCEL", "MSG_CONTINUE", "MSG_CONTINUE_ICON"},
             {"MSG_MENU", "SEL_", "BACK_", "MSG_BOTTOMPANEL", "MSG_ANI", "ANIM"});
        break;
    case MessageType::NoteAnimated:
        hide(s, {"MSG_CANCEL", "MSG_CONTINUE", "MSG_CONTINUE_ICON", "PROGRESS_BAR", "PROGRESS_BDR"},
             {"MSG_MENU", "SEL", "BACK_", "MSG_BOTTOMPANEL", "MSG_ANI"});
        break;
    default:
        break;
    }
    s.setMultiline("MSG_DATA", true);
    s.setText("MSG_DATA", text);
    s.setText("title", L"");
}

void enterMessage(Screen& s, bool startNo) {
    if (!s.isMenu("MSG_MENU")) return;
    s.setMenuSecondPlayer("MSG_MENU", true);
    selectNamed(s, "MSG_MENU", startNo ? "no" : "yes");
    if (s.find("MSG_MENU_BACK") >= 0) s.selectMenuItem("MSG_MENU_BACK", s.selectedMenuItem("MSG_MENU"));
}

void messageFont(Screen& s, bool small) {
    if (s.find("MSG_DATA") < 0) return;
    const std::string data = s.fontOf("MSG_DATA");
    if (small) {
        if (s.find("SMALL_FONT") < 0) return;
        const std::string smallFont = s.fontOf("SMALL_FONT");
        if (_stricmp(data.c_str(), smallFont.c_str()) != 0 && s.find("ORIGINAL_FONT") >= 0) {
            s.setFont("ORIGINAL_FONT", data);
        }
        if (!smallFont.empty()) s.setFont("MSG_DATA", smallFont);
        return;
    }
    if (s.find("ORIGINAL_FONT") < 0) return;
    const std::string original = s.fontOf("ORIGINAL_FONT");
    if (!original.empty() && _stricmp(original.c_str(), data.c_str()) != 0) s.setFont("MSG_DATA", original);
}

MessageClose messageEvent(Screen& s, const MessageState& m, bool network, bool entered, uint32_t event) {
    if (!s.transitionsDone()) return MessageClose::None;
    if (entered && s.find("MSG_MENU_BACK") >= 0 && s.isMenu("MSG_MENU")) {
        s.selectMenuItem("MSG_MENU_BACK", s.selectedMenuItem("MSG_MENU"));
    }
    switch (m.type) {
    case MessageType::Continue:
        if (event != input::kSelect) return MessageClose::None;
        if (m.answer != nullptr) *m.answer = 1;
        return network ? MessageClose::Network : MessageClose::Message;
    case MessageType::ContinueModeless:
        if (event != input::kSelect) return MessageClose::None;
        if (m.answer != nullptr) *m.answer = 1;
        return MessageClose::Message;
    case MessageType::YesNo:
    case MessageType::YesNoModeless:
        if (network) {
            if (event == input::kSelect) {
                if (m.answer != nullptr) *m.answer = m.startNo ? 0 : 1;
            } else if (event == input::kBack) {
                if (m.answer != nullptr) *m.answer = m.startNo ? 1 : 0;
            } else {
                return MessageClose::None;
            }
            return MessageClose::Network;
        }
        if (event != input::kSelect) return MessageClose::None;
        if (entered && s.isMenu("MSG_MENU") && m.answer != nullptr) {
            const std::string item = s.menuItemCommand("MSG_MENU", s.selectedMenuItem("MSG_MENU"));
            *m.answer = _stricmp(item.c_str(), "yes") == 0 ? 1 : 0;
        }
        return MessageClose::Message;
    case MessageType::Progress:
        if (m.progress != nullptr) {
            if (const int bar = s.find("PROGRESS_BAR"); bar >= 0) s.setMeter(bar, *m.progress);
        }
        return MessageClose::None;
    default:
        return MessageClose::None;
    }
}

}  // namespace oj::ui
