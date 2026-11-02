// Integration tests: the fixed build's lobby chat on the game's keyboard window (Frontend/Network/LobbyChat).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeWindowHost.h"

#include "Core/Input/Input.h"
#include "Game/Frontend/Network/LobbyChat.h"
#include "Game/UI/WindowFixes.h"

#include <memory>
#include <string>

namespace {

using oj::test::check;

float topOf(const oj::ui::Screen& s, const std::string& name) {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    s.placeOf(s.find(name), x, y, w, h);
    return y;
}

float bottomOf(const oj::ui::Screen& s, const std::string& name) {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    s.placeOf(s.find(name), x, y, w, h);
    return y + h;
}

}  // namespace

// The chat's layout: its 12 lines above the text, the text above the keys, the keys above the bottom bar (0.846);
// BLANK1, BLANK4 and BLANK5 type ! ? and , and the pad reaches them (left of 1, right of P, right of .).
OJ_INTEGRATION_TEST(LobbyChat, Layout) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::test::FakeWindowHost host;
    const std::shared_ptr<oj::ui::Screen> screen = host.open("keyboard");
    if (screen == nullptr) return;
    oj::ui::Screen& s = *screen;
    oj::frontend::lobbyChatLayout(s, true);
    const std::string last = oj::ui::chatLineName(oj::ui::kChatLines - 1);
    check(oj::ui::kChatLines == 12 && s.find(last) >= 0, "12 chat lines");
    check(bottomOf(s, last) <= topOf(s, "STRING_SMALL") && bottomOf(s, "STRING_SMALL") <= topOf(s, "B1") &&
              bottomOf(s, "SPACE") <= 0.846f,
          "the lines above the text, the text above the keys, the keys above the bottom bar");
    check(s.textOf("BLANK1") == L"!" && s.textOf("BLANK4") == L"?" && s.textOf("BLANK5") == L",",
          "BLANK1 !, BLANK4 ?, BLANK5 ,");
    s.setCurrentButton("B1");
    s.moveHighlight(oj::input::kLeft);
    const bool left = s.currentButton() == "BLANK1";
    s.setCurrentButton("BP");
    s.moveHighlight(oj::input::kRight);
    const bool right = s.currentButton() == "BLANK4";
    s.setCurrentButton("B.");
    s.moveHighlight(oj::input::kRight);
    check(left && right && s.currentButton() == "BLANK5", "the pad reaches !, ? and ,");
}
