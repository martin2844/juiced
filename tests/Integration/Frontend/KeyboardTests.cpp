// Integration tests: the name entry window (keyboard), the game's window through a front end of the tests
// (Framework/Fakes/FakeWindowHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeWindowHost.h"

#include "Core/Input/Input.h"
#include "Game/Frontend/Profile/Keyboard.h"
#include "Game/Frontend/Profile/ProfileHost.h"

#include <memory>
#include <optional>
#include <string>

namespace {

using oj::test::check;

class FakeProfileHost : public oj::test::FakeWindowHost, public oj::frontend::ProfileHost {
public:
    bool nameRefused(const std::wstring&) const override { return false; }
    std::optional<oj::save::Profile>& profile() override { return m_profile; }
    void setUpDriverName(const std::wstring&) override {}
    oj::frontend::KeyboardSetup& keyboardSetup() override { return m_setup; }

private:
    std::optional<oj::save::Profile> m_profile;
    oj::frontend::KeyboardSetup m_setup;
};

}  // namespace

// SELECT3 (the symbols page, a setup that allows it) puts the symbols on the letters' keys (0x00460450's page 2:
// BQ " .. BM ~); again, the letters come back (lower case: no shift, no caps).
OJ_INTEGRATION_TEST(Keyboard, SymbolsPage) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeProfileHost host;
    const std::shared_ptr<oj::ui::Screen> screen = host.open("keyboard");
    if (screen == nullptr) return;
    oj::frontend::Keyboard window;
    window.setup().symbols = true;
    for (char c = 0x20; c < 0x7F; ++c) window.setup().allowed.push_back(c);
    window.setup().maxLength = 20;
    window.enter(host, *screen);
    check(screen->textOf("BJ") == L"j", "the letters' page: BJ j");
    window.update(host, *screen, oj::input::kSelect3, 0, false);
    check(screen->textOf("BQ") == L"\"" && screen->textOf("BJ") == L"?" && screen->textOf("BM") == L"~",
          "the symbols page: BQ \", BJ ?, BM ~");
    screen->setCurrentButton("BJ");
    window.update(host, *screen, oj::input::kSelect, 0, false);
    check(window.setup().text == L"?", "BJ on the symbols page types ?");
    window.update(host, *screen, oj::input::kSelect3, 0, false);
    check(screen->textOf("BJ") == L"j" && screen->textOf("BQ") == L"q", "the letters back: BJ j, BQ q");
}
