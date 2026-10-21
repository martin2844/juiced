// Integration tests: the race's sound listener (Game/Race/TrackView, TrackView::soundListener) on the 3D world of the
// game's banks and the system's DirectSound.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"

#include "Audio/SoundSystem.h"
#include "Audio/Voices.h"
#include "Core/Base/Random.h"
#include "Game/Race/TrackView/TrackView.h"

#include <windows.h>

#include <cmath>
#include <memory>

namespace {

using oj::test::check;

}  // namespace

// The listener takes the camera's view matrix (0x004205A0 with the camera's +0x48, its world matrix's inverse): a voice
// on the right of a camera turned a quarter round is placed on the listener's right, ahead nowhere. With the world
// matrix in its place the voice went to the left, so a car spun in place swung its sounds from side to side.
OJ_INTEGRATION_TEST(SoundListener, CameraTurned) {
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
        auto view = std::make_unique<app::TrackView>();
        oj::CrtRandom random;
        view->sound3d = std::make_unique<oj::audio::World>(oj::test::gameFiles(), 256, random);
        view->sound3d->player().setMasterVolume(0.0f);  // heard by no one
        // A quarter turn about y at (1000, 0, 0): right (0, 0, -1), up (0, 1, 0), forward (1, 0, 0).
        oj::render::Camera& camera = view->first().camera;
        camera.local = {0.0f, 0.0f, 1.0f, 1000.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f};
        camera.invalidate();
        camera.update();
        view->soundListener();
        oj::audio::Instance* voice = view->sound3d->create("SKID_ROAD_DRY", nullptr, 0);
        if (voice == nullptr) {
            check(false, "the 3d bank's SKID_ROAD_DRY");
        } else {
            voice->setPosition({1000.0f, 0.0f, -500.0f, 0.0f});  // 5 m to the camera's right
            voice->setVolume(1.0f);
            voice->play();
            view->sound3d->update(1.0f / 60.0f);
            const oj::audio::Vec4& r = voice->emitter()->state->relative;
            check(std::fabs(r.x - 500.0f) < 0.01f && std::fabs(r.y) < 0.01f && std::fabs(r.z) < 0.01f,
                  "the voice 5 m on the listener's right (x 500, y and z 0)");
            voice->release();
        }
        view->sound3d.reset();
    }
    sound.stop();
}
