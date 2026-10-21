// Integration tests: the audio options (opts_audio), the game's window and texts through a front end of the tests
// (Framework/Fakes/FakeWindowHost.h).

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/FakeWindowHost.h"

#include "Core/Data/GameData.h"
#include "Game/Frontend/Options/AudioOptions.h"
#include "Game/Frontend/Options/OptionsHost.h"

#include <memory>
#include <string>

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
