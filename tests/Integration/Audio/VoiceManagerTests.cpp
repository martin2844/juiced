// Integration tests: the voice manager (Audio/Voices.cpp) on the game's banks and the system's DirectSound.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Audio/Bank.h"
#include "Audio/Player.h"
#include "Audio/SoundSystem.h"
#include "Audio/Voices.h"
#include "Core/Base/Random.h"

#include <windows.h>

#include <filesystem>
#include <memory>

namespace {

using oj::test::check;

}  // namespace

// Two instances of a set of one voice (FTOB_HIGH_ON, as the player's FTO and an opponent's of arcade race 1 have it),
// both heard: the louder takes the voice and keeps it frame after frame. Keeping it counts as a user of the set
// (0x005C1360), so the other's acquire answers 3 instead of taking the voice (the engine's sound passed between the
// two cars every other frame).
OJ_INTEGRATION_TEST(VoiceManager, KeptVoiceNotTaken) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::audio::System& sound = oj::audio::system();
    if (!sound.start(
#ifdef OJ_MACOS
            nullptr
#else
            GetDesktopWindow()
#endif
, 0x20)) {
        check(false, "the sound system started (the tests need a DirectSound device)");
        return;
    }
    {
        std::unique_ptr<oj::audio::Bank> bank;
        oj::CrtRandom random;
        oj::audio::World world(oj::test::gameFiles(), 256, random);
        bank = oj::audio::Bank::load(oj::test::gameFiles(), std::filesystem::path("audio") / "cars" / "ftob", "ftob",
                                     nullptr);
        if (bank != nullptr) world.player().addBank(bank.get());
        world.player().setMasterVolume(0.0f);  // heard by no one
        oj::audio::SetRef ref;
        oj::audio::SoundSet* set = world.player().find("FTOB_HIGH_ON", ref) == 0 ? world.player().setOf(ref) : nullptr;
        oj::audio::Instance* loud = world.create("FTOB_HIGH_ON", nullptr, 0);
        oj::audio::Instance* quiet = world.create("FTOB_HIGH_ON", nullptr, 0);
        if (set == nullptr || set->instances() != 1 || loud == nullptr || quiet == nullptr) {
            check(false, "the ftob bank's FTOB_HIGH_ON, a set of one voice, and two instances of it");
        } else {
            loud->setVolume(1.0f);
            quiet->setVolume(0.5f);
            loud->play();
            quiet->play();
            world.update(1.0f / 60.0f);
            oj::audio::Player::updateAll();
            const uint32_t first = set->owner(0);
            check(first != 0, "the louder instance plays the voice");
            int kept = 0;
            for (int frame = 0; frame < 6; ++frame) {
                world.update(1.0f / 60.0f);
                oj::audio::Player::updateAll();
                if (set->owner(0) == first) ++kept;
            }
            check(kept == 6, "the voice kept by its slot six frames on, not taken by the other instance");
        }
        if (quiet != nullptr) quiet->release();
        if (loud != nullptr) loud->release();
    }
    sound.stop();
}

// Two instances of a set of one voice (FTOB_HIGH_ON, as two cars of the same engine bank have it): the louder takes
// the voice. Silenced, it is culled and its voice faded out (the 3D handle's +0x20, 0x005C1A90): in that frame the
// other's oldest-voice steal is refused, the voice kept by the fading slot; the player's frame stops it, and the next
// frame gives the voice to the other.
OJ_INTEGRATION_TEST(VoiceManager, FadingVoiceKept) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    oj::audio::System& sound = oj::audio::system();
    if (!sound.start(
#ifdef OJ_MACOS
            nullptr
#else
            GetDesktopWindow()
#endif
, 0x20)) {
        check(false, "the sound system started (the tests need a DirectSound device)");
        return;
    }
    {
        std::unique_ptr<oj::audio::Bank> bank;
        oj::CrtRandom random;
        oj::audio::World world(oj::test::gameFiles(), 256, random);
        bank = oj::audio::Bank::load(oj::test::gameFiles(), std::filesystem::path("audio") / "cars" / "ftob", "ftob",
                                     nullptr);
        if (bank != nullptr) world.player().addBank(bank.get());
        world.player().setMasterVolume(0.0f);  // heard by no one
        oj::audio::SetRef ref;
        oj::audio::SoundSet* set = world.player().find("FTOB_HIGH_ON", ref) == 0 ? world.player().setOf(ref) : nullptr;
        oj::audio::Instance* loud = world.create("FTOB_HIGH_ON", nullptr, 0);
        oj::audio::Instance* quiet = world.create("FTOB_HIGH_ON", nullptr, 0);
        if (set == nullptr || set->instances() != 1 || loud == nullptr || quiet == nullptr) {
            check(false, "the ftob bank's FTOB_HIGH_ON, a set of one voice, and two instances of it");
        } else {
            loud->setVolume(1.0f);
            quiet->setVolume(0.5f);
            loud->play();
            quiet->play();
            world.update(1.0f / 60.0f);
            oj::audio::Player::updateAll();
            const uint32_t first = set->owner(0);
            int state = 0;
            check(first != 0 && world.player().state(first, state) == 0 && (state == 3 || state == 4),
                  "the louder instance plays the voice");

            loud->setVolume(0.0f);
            world.update(1.0f / 60.0f);
            state = 0;
            check(set->owner(0) == first && world.player().state(first, state) == 0 &&
                      state == oj::audio::kSoundFadingOut,
                  "the silenced one's voice fading out, kept from the other in that frame");

            oj::audio::Player::updateAll();
            check(set->owner(0) == 0, "the player's frame stops the faded voice");
            world.update(1.0f / 60.0f);
            const uint32_t next = set->owner(0);
            state = 0;
            check(next != 0 && next != first && world.player().state(next, state) == 0 && (state == 3 || state == 4),
                  "the next frame gives the voice to the other");
        }
        if (quiet != nullptr) quiet->release();
        if (loud != nullptr) loud->release();
    }
    sound.stop();
}
