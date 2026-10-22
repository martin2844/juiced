// The race's cameras: the car each player's camera takes, its target and the fly-by.

#include "Game/Race/TrackView/TrackView.h"

#include <cmath>

namespace app {

using DriverSettings = oj::save::Settings;

void TrackView::keepView(RacePlayer& p, int mode) {
    p.cameraMode = mode;
    const uint32_t at = DriverSettings::kView + 4u * static_cast<uint32_t>(p.index);
    if (settings != nullptr && settings->has_value()) (*settings)->setU32(at, static_cast<uint32_t>(mode));
}

void TrackView::cameraTakes(RacePlayer& player, RaceSlot* car) {
    if (car == player.soundCamera) return;
    if (car != nullptr && car->sound) {
        car->sound->setFocus(true, car->road, car->horn, soundHolders(*car), squealOf(*car));
    }
    for (std::unique_ptr<RaceSlot>& s : cars) {
        if (s.get() != car && s->sound) s->sound->setFocus(false, s->road, s->horn, soundHolders(*s), squealOf(*s));
    }
    player.soundCamera = car;
}

void TrackView::cameraOcclusion(RacePlayer& player) {
    RaceSlot* car = player.soundCamera;
    if (car == nullptr || scene == nullptr) return;
    const oj::Vec4 o = car->body->origin();
    float v = 0.0f;
    bool inside = scene->sceneObjects().inReverb({o[0] * 100.0f, o[1] * 100.0f, o[2] * 100.0f, o[3]});
    if (inside) v = 0.5f;
    const oj::Mat34& w = player.camera.world();
    if (scene->sceneObjects().inTunnel({w[3], w[7], w[11], 0.0f})) {
        inside = true;
        v = 0.75f;
    }
    if (!inside) v = 0.0f;
    if (car->sound) car->sound->setOcclusion(v);
    if (car->car.effects.made) {
        const CarParticles& p = car->particles;
        const oj::CarEffects& e = car->car.effects;
        oj::scene::Emitter* squeal = e.squeal == nullptr        ? nullptr
                                     : e.squeal == e.sandSqueal ? p.sandSqueal.get()
                                                                : p.tarmacSqueal.get();
        for (oj::scene::Emitter* em : {p.explodeSparks.get(), p.scrapeSparks.get(), p.flakes.get(), squeal}) {
            if (em == nullptr || em->sound == nullptr) continue;
            em->sound->setOcclusionOn(1);
            em->sound->setOcclusion(v);
        }
    }
    if (car->horn != nullptr) {
        car->horn->setOcclusionOn(1);
        car->horn->setOcclusion(v);
    }
}

void TrackView::watchedCar() {
    const oj::Career* career = gameData != nullptr && gameData->career ? &*gameData->career : nullptr;
    if (career == nullptr || career->profile.raceCarsChosen != 0 || cars.empty()) return;
    if (first().introPlaying || first().raceCamera.mode() == oj::scene::RaceCamera::kBeforeStart) return;
    for (const int step : {-1, 1}) {
        if (!(step < 0 ? frameActions.nextCar : frameActions.select)) continue;
        namesTime() = 2.0f;
        RaceSlot* current = followedSlot(first());
        const int32_t at = current->car.competitor != nullptr ? current->car.competitor->place : -1;
        RaceSlot* next = current;
        if (at == -1) {
            size_t i = 0;
            while (i < cars.size() && cars[i].get() != current) ++i;
            next = cars[i + 1 < cars.size() ? i + 1 : 0].get();
        } else {
            const auto count = static_cast<int32_t>(cars.size());
            int32_t place = at + step;
            if (place == 0) place = count;
            if (place > count) place = 1;
            for (const std::unique_ptr<RaceSlot>& s : cars) {
                if (s->car.competitor != nullptr && s->car.competitor->place == place) {
                    next = s.get();
                    break;
                }
            }
        }
        if (next != current) {
            first().viewed = next == first().slot ? nullptr : next;
            first().raceCamera.newCar();
            cameraTakes(first(), followedSlot(first()));
        }
    }
}

oj::scene::CameraTarget TrackView::cameraTarget(const RacePlayer& player) const {
    oj::scene::CameraTarget target;
    const RaceSlot* followed = followedSlot(player);
    const oj::CarBody& body = *followed->body;
    target.node = followed->model->root();
    target.matrix = body.modelMatrix();
    target.velocity = body.velocity;
    target.localVelocity = body.localVelocity;
    target.localAngularVelocity = body.localAngularVelocity;
    target.forwardAcceleration = body.localAcceleration[2];
    // The nitro (+0x190, +0x194: 1 while it is asked, 0x004888B0), the wheels' effects of the frame (+0x41C ..
    // +0x424) and what is left of the last hit (+0x174): the shakes (0x00402F90, 0x00403320) and the blur read
    // them.
    target.nitro = followed->car.nitroAsked;
    target.nitroAmount = followed->car.nitroLook;
    target.wheelEffects = {followed->car.maxSkid, followed->car.maxSmoke, followed->car.maxSlide, 0.0f};
    target.impact = followed->car.impact;
    target.replayRace = rules.replayRace;
    target.competitor = followed->car.competitor;
    if (const oj::RaceCar* f = followed->car.followed; f != nullptr) {
        const oj::Mat34 fm = f->body->modelMatrix();
        target.followed = true;
        target.followedOrigin = {fm[3], fm[7], fm[11], 0.0f};
    }
    if (const oj::RaceCar* f = followed->car.follower; f != nullptr) {
        const oj::Mat34 fm = f->body->modelMatrix();
        target.follower = true;
        target.followerOrigin = {fm[3], fm[7], fm[11], 0.0f};
    }
    return target;
}

oj::scene::CameraRace TrackView::cameraRace(RacePlayer& player) {
    player.cameraCars.clear();
    for (const std::unique_ptr<RaceSlot>& s : cars) {
        const oj::Mat34 m = s->body->modelMatrix();
        const oj::Vec4& v = s->body->velocity;
        player.cameraCars.push_back({{m[3], m[7], m[11], 0.0f}, s->chassisRadius,
                              std::sqrt((v[2] * v[2] + v[1] * v[1]) + v[0] * v[0])});
    }
    oj::scene::CameraRace r;
    for (size_t i = 0; i < cars.size(); ++i) {
        if (cars[i].get() == followedSlot(player)) r.self = static_cast<int>(i);
    }
    r.raceType = rules.type;
    r.paused = paused;
    r.replayFast = replay.playing != 0 && replay.fast != 0;
    r.replayFastSet = replay.fast != 0;
    r.cars = &player.cameraCars;
    r.collision = &world->collision();
    r.wideScreen = wideScreen;
    r.players = rules.players;  // [G+0x6C]+0x58
    r.count = race ? race->count : 0.0f;
    r.playerMode = viewKept(player);
    r.netSpectator = net != nullptr && rules.spectator;
    r.places = &places;
    return r;
}

void TrackView::startIntro(const std::string& routeName, bool wide) {
    wideScreen = wide;
    const bool two = rules.players == 2;
    for (std::unique_ptr<RacePlayer>& p : players) {
        p->camera.setAspect(wide ? (two ? 0.28125f : 0.5625f) : (two ? 0.375f : 0.75f));
        p->introPlaying =
            oj::startView(rules.type, rules.heat, net != nullptr) == oj::StartView::FlyBy && p->intro.load(routeName);
        p->introStarted = false;
    }
    if (first().introPlaying) chooseWindow(false);
}

}  // namespace app
