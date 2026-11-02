// Integration tests: the audio options (opts_audio), the game's window and texts through a front end of the tests
// (Framework/Fakes/FakeWindowHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeWindowHost.h"

#include "Core/Data/GameData.h"
#include "Game/Frontend/Options/AudioOptions.h"
#include "Game/Frontend/Options/OptionsHost.h"

#include "Core/Input/Input.h"
#include "Game/UI/WindowFixes.h"

#include <memory>
#include <string>
#include <vector>

namespace {

using oj::test::check;

class FakeOptionsHost : public oj::test::FakeWindowHost, public oj::frontend::OptionsHost {
public:
    const char* optionsMenu() const override { return "opts_menu"; }
    bool racePause() const override { return false; }
    bool secondPlayerPaused() const override { return false; }
    uint32_t secondPlayerEvent() const override { return 0; }
    void setEffectsVolume(float) override {}
    void controlsSetUp() override {}
    void setDetailFactor(float) override {}
    void restorePlaylistChoice() override {}
    bool eax() const override { return eaxOn; }
    void setEax(bool on) override {
        eaxOn = on;
        set.push_back(on);
    }
    void keepEax(bool on) override { kept.push_back(on); }
    bool eaxOn = false;
    std::vector<bool> set;
    std::vector<bool> kept;
};

// The mask the sound of the name was asked with the last time; 0 when it was not asked.
uint32_t maskOf(const FakeOptionsHost& host, const char* name) {
    uint32_t mask = 0;
    for (size_t i = 0; i < host.sounds.size(); ++i) {
        if (host.sounds[i] == name) mask = host.soundMasks[i];
    }
    return mask;
}

}  // namespace

// A meter moved plays its sound with the masks 1 (0x00480F34, 0x00480FBB): only a free voice of the set, so the
// sound still playing is not started again from its start on each step.
OJ_INTEGRATION_TEST(AudioOptions, MeterSoundsWait) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    FakeOptionsHost host;
    oj::GameData data(oj::test::gameFiles().root());
    data.race.players = 1;
    host.data = &data;
    const std::shared_ptr<oj::ui::Screen> screen = host.open("opts_audio");
    if (screen == nullptr) return;
    oj::frontend::AudioOptions window;
    window.enter(host, *screen);
    screen->setMeter("SPEECH", screen->meterShown("SPEECH") < 0.5f ? 0.9f : 0.1f, true);
    screen->setMeter("SFX", screen->meterShown("SFX") < 0.5f ? 0.9f : 0.1f, true);
    window.update(host, *screen, 0);
    check(maskOf(host, "SPEECH") == 1, "SPEECH's meter moved: its sound with the masks 1");
    check(maskOf(host, "Warning") == 1, "SFX's meter moved: Warning with the masks 1");
}

// The fixed build's EAX row (opts_audio): under PLAYLIST, DONE below it, its neighbours; OFF or ON as the sound is;
// LEFT and RIGHT switch it at once, BACK puts it back, DONE keeps it. A race's opts_ingame_audio has none.
OJ_INTEGRATION_TEST(AudioOptions, EaxRow) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    {
        FakeOptionsHost host;
        oj::GameData data(oj::test::gameFiles().root());
        data.race.players = 1;
        host.data = &data;
        const std::shared_ptr<oj::ui::Screen> screen = host.open("opts_ingame_audio");
        if (screen == nullptr) return;
        check(screen->find(oj::ui::kEaxButton) < 0 && screen->find(oj::ui::kEaxValue) < 0,
              "opts_ingame_audio: no EAX row (a race's banks were made with or without it)");
    }
    {
        const char* name = "opts_audio";
        FakeOptionsHost host;
        oj::GameData data(oj::test::gameFiles().root());
        data.race.players = 1;
        host.data = &data;
        const std::shared_ptr<oj::ui::Screen> screen = host.open(name);
        if (screen == nullptr) return;
        oj::ui::Screen& s = *screen;
        float bx = 0.0f, by = 0.0f, bw = 0.0f, bh = 0.0f, dx = 0.0f, dy = 0.0f, dw = 0.0f, dh = 0.0f;
        s.placeOf(s.find(oj::ui::kEaxButton), bx, by, bw, bh);
        s.placeOf(s.find("DON_BUT"), dx, dy, dw, dh);
        check(s.find(oj::ui::kEaxValue) >= 0 && s.find(oj::ui::kEaxLeft) >= 0 && s.find(oj::ui::kEaxRight) >= 0 &&
                  dy > by + bh && dy + dh < 0.86f,
              std::string(name) + ": the EAX row, DONE below it within the window");
        oj::frontend::AudioOptions window;
        window.enter(host, s);
        check(s.textOf(oj::ui::kEaxButton) == L"EAX" && s.textOf(oj::ui::kEaxValue) != L"ON",
              std::string(name) + ": EAX, OFF as the sound is");
        s.setCurrentButton(oj::ui::kEaxButton);
        window.update(host, s, oj::input::kRight);
        check(host.eaxOn && s.textOf(oj::ui::kEaxValue) == L"ON", std::string(name) + ": RIGHT ON at once");
        window.update(host, s, oj::input::kBack);
        check(!host.eaxOn && host.kept.empty(), std::string(name) + ": BACK puts it back, nothing kept");
        window.enter(host, s);
        s.setCurrentButton(oj::ui::kEaxButton);
        window.update(host, s, oj::input::kLeft);
        s.setCurrentButton("DON_BUT");
        window.update(host, s, oj::input::kSelect);
        check(host.eaxOn && host.kept.size() == 1 && host.kept[0], std::string(name) + ": DONE keeps it");
    }
}
