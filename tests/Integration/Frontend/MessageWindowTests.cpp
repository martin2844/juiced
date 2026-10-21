// Integration tests: the message windows' state (Game/UI/MessageWindow.h) over the game's msgbox.ui and
// msgbox_network.ui.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Data/Language.h"
#include "Core/Input/Input.h"
#include "Game/UI/MessageWindow.h"
#include "Game/UI/UiScreen.h"

#include <memory>
#include <string>

namespace {

using oj::test::check;
using oj::ui::MessageClose;
using oj::ui::MessageState;
using oj::ui::MessageType;

struct Window {
    oj::test::NullRenderDevice device;
    oj::Language language = *oj::Language::byName("english");
    std::shared_ptr<oj::ui::Screen> screen;

    explicit Window(const char* name) {
        screen = std::make_shared<oj::ui::Screen>(device, language);
        if (!screen->load(name)) {
            check(false, std::string(name) + ".ui read from the game's folder");
            screen.reset();
        }
    }
    // The transitions over, as a few seconds of drawing would end them.
    void settle() { screen->finishTransitions(); }
    bool shown(const char* element) const { return !screen->hidden(element); }
};

}  // namespace

// A kept window opened again with another type shows what the type before hid (0x005A7170 shows every element first).
OJ_INTEGRATION_TEST(MessageWindow, OpenShowsEveryElementFirst) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    Window w("msgbox");
    if (!w.screen) return;
    oj::ui::openMessage(*w.screen, MessageType::Continue, L"note");
    const bool noteHidesMenu = !w.shown("MSG_MENU") && w.shown("MSG_CONTINUE");
    oj::ui::openMessage(*w.screen, MessageType::YesNo, L"question");
    const bool questionShowsMenu = w.shown("MSG_MENU") && !w.shown("MSG_CONTINUE");
    check(noteHidesMenu && questionShowsMenu && w.screen->textOf("MSG_DATA") == L"question",
          "msgbox: a question after a note shows its menu again");
}

// msgbox's question (entered): SELECT answers by the item, "no" with startNo; BACK does nothing; it closes msgbox.
OJ_INTEGRATION_TEST(MessageWindow, QuestionAnswersByItem) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    Window w("msgbox");
    if (!w.screen) return;
    int32_t answer = -1;
    const MessageState question{MessageType::YesNo, &answer, nullptr, true};
    oj::ui::openMessage(*w.screen, MessageType::YesNo, L"sure?");
    oj::ui::enterMessage(*w.screen, true);
    w.settle();
    const MessageClose back = oj::ui::messageEvent(*w.screen, question, false, true, oj::input::kBack);
    const bool backNothing = back == MessageClose::None && answer == -1;
    const MessageClose select = oj::ui::messageEvent(*w.screen, question, false, true, oj::input::kSelect);
    check(backNothing && select == MessageClose::Message && answer == 0, "msgbox: SELECT on \"no\" answers 0");
}

// msgbox_network's question answers by +0x28 whatever the item, BACK too, and closes itself; its ContinueModeless
// note answers 1 and closes msgbox, not itself; its Continue note closes itself.
OJ_INTEGRATION_TEST(MessageWindow, NetworkRules) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    Window w("msgbox_network");
    if (!w.screen) return;
    int32_t answer = -1;
    oj::ui::openMessage(*w.screen, MessageType::YesNo, L"leave?");
    w.settle();
    const MessageClose select = oj::ui::messageEvent(
        *w.screen, MessageState{MessageType::YesNo, &answer, nullptr, false}, true, false, oj::input::kSelect);
    const bool selectYes = select == MessageClose::Network && answer == 1;
    const MessageClose back = oj::ui::messageEvent(*w.screen, MessageState{MessageType::YesNo, &answer, nullptr, true},
                                                   true, false, oj::input::kBack);
    const bool backYes = back == MessageClose::Network && answer == 1;
    check(selectYes && backYes, "msgbox_network: a question answers by +0x28, BACK too");

    answer = 0;
    oj::ui::openMessage(*w.screen, MessageType::ContinueModeless, L"lost");
    w.settle();
    const MessageClose note2 =
        oj::ui::messageEvent(*w.screen, MessageState{MessageType::ContinueModeless, &answer, nullptr, false}, true,
                             false, oj::input::kSelect);
    check(note2 == MessageClose::Message && answer == 1, "msgbox_network: a ContinueModeless note closes msgbox");
    answer = 0;
    const MessageClose note1 = oj::ui::messageEvent(
        *w.screen, MessageState{MessageType::Continue, &answer, nullptr, false}, true, false, oj::input::kSelect);
    check(note1 == MessageClose::Network && answer == 1, "msgbox_network: a Continue note closes itself");
}
