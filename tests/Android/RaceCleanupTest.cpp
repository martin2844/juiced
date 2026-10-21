#include "Game/Race/TrackView/TrackView.h"
#include "Game/Scene/Particles/ParticleSystem.h"
#include "../Framework/Fakes/NullRenderDevice.h"

#include <cstdlib>
#include <iostream>

namespace {
struct CameraEffect : oj::scene::Emitter {
    app::TrackView& view;
    int& released;
    CameraEffect(oj::scene::ParticleSystem& system, const oj::EffectDef& def,
                 app::TrackView& v, int& count)
        : Emitter(system, def), view(v), released(count) {
        sound = view.sound3d->manager().allocate(0, nullptr);
    }
    ~CameraEffect() override {
        // Fail before the base destructor dereferences an already freed sound instance.
        if (!view.sound3d) {
            std::cerr << "Camera effect outlived its audio world\n";
            std::exit(1);
        }
        ++released;
    }
    bool frame(oj::render::Camera&) override { return true; }
};
}

int main() {
    oj::test::NullRenderDevice device;
    oj::ExeTables tables;
    oj::scene::Engine engine(device, tables, 0, false);
    oj::EffectLibrary effects;
    oj::EffectDef def;
    oj::scene::ParticleSystem particles(engine, effects);
    oj::CrtRandom random;
    app::TrackView view;
    view.players.push_back(std::make_unique<app::RacePlayer>());
    int released = 0;
    for (int race = 0; race != 3; ++race) {
        view.sound3d = std::make_unique<oj::audio::World>(oj::GameFiles("."), 32, random);
        for (auto& player : view.players) {
            player->cameraRain = std::make_shared<CameraEffect>(particles, def, view, released);
            player->cameraSplash = std::make_shared<CameraEffect>(particles, def, view, released);
            player->cameraBlobs = std::make_shared<CameraEffect>(particles, def, view, released);
        }
        view.endRace();
        if (released != (race + 1) * 6) {
            std::cerr << "Race exit retained camera effects after destroying audio\n";
            std::exit(1);
        }
    }
    std::cout << "Three two-player race cleanups passed\n";
}
