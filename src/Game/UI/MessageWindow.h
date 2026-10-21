#pragma once
// The message windows of the interface manager (docs/spec/02-frontend.md "The message window"): msgbox (the
// manager's +0x44, 0x005A80E0), msgbox_network (+0x3C, 0x005A7F90) and msgbox_critical (+0x34, 0x005A8030). The three
// share one state class (vtable 0x006FDDA0: enter 0x005A9180, update 0x005A9200; the records 0x00722FAC, 0x00723004
// and 0x00722FD8): what a type shows as the window opens and what SELECT and BACK do. The front end and the race's
// front end keep their windows; this is what they do alike.

#include <cstdint>
#include <string>

namespace oj::ui {

class Screen;

// A message's type (the state's +0x0C): what the window shows and what SELECT and BACK do. The front end's windows
// under a message wait while it is Continue or YesNo (its +0x4C); the modeless ones leave them going.
enum class MessageType : int32_t {
    Note = 0,              // the text without buttons
    Continue = 1,          // a note with CONTINUE
    ContinueModeless = 2,  // a note with CONTINUE; in msgbox_network SELECT closes msgbox, not the note
    YesNo = 3,             // a yes / no question
    YesNoModeless = 4,     // a yes / no question
    Progress = 5,          // a progress bar
    NoteAnimated = 6,      // the text without buttons, its animation kept
};

// A message's state record: its type (+0x0C), its answer (+0x18 for a question, +0x20 for a note: one pointer here,
// the type says which), Progress's value (+0x1C) and the start on "no" (+0x28).
struct MessageState {
    MessageType type = MessageType::Note;
    int32_t* answer = nullptr;
    const float* progress = nullptr;
    bool startNo = false;
};

// 0x005A7170 (the window one of the manager's kept ones, its transitions started again, 0x005AA0A0): every element
// shown (+0x58 0), then those of the type hidden; MSG_DATA takes the text over several lines, `title` is emptied.
void openMessage(Screen& s, MessageType type, const std::wstring& text);

// The state's enter (0x005A9180), which only msgbox's opening calls (0x005A80E0; msgbox_network's 0x005A7F90 and
// msgbox_critical's 0x005A8030 set the state without it): MSG_MENU (the state's +0x10) takes the second player's events
// too (+0x1A0) and is on "yes", or on "no" with startNo; MSG_MENU_BACK on the same place.
void enterMessage(Screen& s, bool startNo);

// 0x005A80E0's font (msgbox only): small, MSG_DATA in SMALL_FONT's font with its own kept in ORIGINAL_FONT's when the
// two differ; not small, MSG_DATA back in ORIGINAL_FONT's when that has one and it differs.
void messageFont(Screen& s, bool small);

// What SELECT or BACK closes: the manager's message (its +0x48 set) or its network message (+0x40 set).
enum class MessageClose { None, Message, Network };

// 0x005A9200: nothing until the window's transitions are over (0x005AA120); MSG_MENU_BACK follows MSG_MENU's
// highlight (entered: msgbox only), jumping to it (0x005AF8A0 with 0: the brackets do not glide as a menu's highlight
// does); then by the type, with the event of the player the state reads (+0x24):
//   Continue: SELECT writes 1 to the answer; closes the window it is (msgbox_network: its own +0x40, else +0x48);
//   ContinueModeless: SELECT writes 1 to the answer; closes the message (+0x48) even from msgbox_network, whose note
//      stays up for the one who reads the answer to close;
//   YesNo, YesNoModeless: msgbox_network: SELECT answers 1, BACK 0 (with startNo the other way round), whatever the
//      item; else SELECT answers 1 on the item "yes" and 0 on another (entered: MSG_MENU there); either closes the
//      window it is;
//   Progress: PROGRESS_BAR's value (+0xF0) the progress (no answer, nothing closes).
// The caller plays CLICK (the manager's 0x005A8860) when something closes.
// entered: the state's enter ran (its +0x10, MSG_MENU, found).
MessageClose messageEvent(Screen& s, const MessageState& m, bool network, bool entered, uint32_t event);

}  // namespace oj::ui
