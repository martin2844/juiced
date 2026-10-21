// The race's sounds: the cars', the crowd's, the airhorn, the road's, the camera's listener and the pauses.

#include "Game/Race/TrackView/TrackView.h"

#include "Audio/SoundSystem.h"
#include "Core/Base/GameFiles.h"
#include "Core/Base/X87.h"

#include <windows.h>

#include <bit>
#include <cmath>
#include <utility>

namespace app {

using DriverSettings = oj::save::Settings;

void TrackView::makeCarSounds() {
    if (!sound3d) return;
    // The wheels' instances (0x00485270, as each car was made): "silence", group 1, rolloff 1, not looping, on
    // the wheel's node.
    for (std::unique_ptr<RaceSlot>& s : cars) {
        for (size_t i = 0; i < 4; ++i) {
            oj::audio::Instance* w = sound3d->createGrouped("silence", 0);
            s->wheelSounds[i] = w;
            if (w == nullptr) continue;
            const oj::render::Node* node = s->model != nullptr ? s->model->wheelNode(i) : nullptr;
            w->setNode(node);
            w->setRolloff(1.0f);
            w->setLooped(0);
            w->setGroup(1);
        }
    }
    const oj::GameFiles files(std::filesystem::current_path());
    std::vector<std::string> banks;
    for (const std::unique_ptr<RaceSlot>& s : cars) {
        if (s->record.def == nullptr) continue;
        std::string name = s->record.def->engineSound;
        if (s->record.mods[oj::kExhaust] != 0xFF) name += "b";
        bool known = false;
        for (const std::string& b : banks) known = known || b == name;
        if (!known) banks.push_back(name);
    }
    for (const std::string& name : banks) addSoundBank(name.c_str(), std::filesystem::path("cars") / name);
    oj::audio::Player& p = sound3d->player();
    const bool wet = gameData != nullptr && gameData->race.weather == 2;
    for (std::unique_ptr<RaceSlot>& s : cars) {
        const void* node = s->model != nullptr ? static_cast<const void*>(s->model->root()) : nullptr;
        if (s->record.def != nullptr && s->car.body != nullptr) {
            s->sound = std::make_unique<oj::CarSound>(*sound3d, files, s->car, s->record, gameData->catalogue,
                                                      node, -1, automod, automodValue);
        }
        p.find("tarmacdry", s->roadDry);
        s->roadNow = s->roadDry;
        if (wet) {
            p.find("tarmac", s->roadWet);
            s->roadNow = s->roadWet;
        }
        p.find("sand", s->roadSand);
        s->road = sound3d->create("tarmacdry", node, 0);
        if (s->road != nullptr) s->road->setLooped(1);
        const std::string horn = "carhorn" + std::to_string(s->record.horn);
        s->horn = sound3d->create(horn.c_str(), node, 0);
        if (s->horn != nullptr) {
            s->horn->setLooped(1);
            s->horn->setVolume(1.0f);
            s->horn->setRolloffScale(1.0f);
            s->horn->setRolloff(1.0f);
            s->horn->setMinDistance(1300.0f);  // [0x0071796C]
        }
    }
    raceSounds.view = this;
    oj::raceSounds() = &raceSounds;
    // 0x0041E0B0, each player in turn: its camera takes its car (0x00401DB0) and the car's sounds are turned to it
    // again (the second player's camera turns the first's car's away: Stock problem).
    for (std::unique_ptr<RacePlayer>& each : players) {
        RacePlayer& player = *each;
        player.soundCamera = nullptr;
        player.director.sounds = [this, &player](bool started) { directorSounds(player, started); };
        RaceSlot* followed = followedSlot(player);
        cameraTakes(player, followed);
        if (followed != nullptr && followed->sound) {
            followed->sound->setFocus(true, followed->road, followed->horn, soundHolders(*followed),
                                      squealOf(*followed));
        }
    }
}

void TrackView::directorSounds(RacePlayer& player, bool started) {
    if (sound3d) {
        sound3d->manager().setGroupVolume(0, 0.25f);
        sound3d->manager().setGroupVolume(1, 1.0f);
        sound3d->manager().setGroupVolume(2, 0.95f);
    }
    RaceSlot* car = player.soundCamera;
    if (car == nullptr || !car->sound) return;
    const bool on = started || first().slot == nullptr || car == first().slot;
    car->sound->setFocus(on, car->road, car->horn, soundHolders(*car), squealOf(*car));
}

oj::audio::Instance* TrackView::squealOf(RaceSlot& s) {
    const CarParticles& p = s.particles;
    const oj::CarEffects& e = s.car.effects;
    if (e.squeal == nullptr) return nullptr;
    const oj::scene::Emitter* squeal = e.squeal == e.sandSqueal ? p.sandSqueal.get() : p.tarmacSqueal.get();
    return squeal != nullptr ? squeal->sound : nullptr;
}

void TrackView::crowdSets() {
    static const char* const kTricks[18] = {"", "hk_highspeed", "", "hk_doughnut", "hk_doughnut", "hk_doughnut",
                                            "hk_doughnut", "hk_uturn", "hk_360", "hk_360", "hk_180", "hk_180",
                                            "hk_Boomerang", "hk_JTurn", "hk_JTurn", "hk_Drift", "", ""};
    static const char* const kMoods[6][8] = {
        {"hk_whistle1", "hk_whistle3", "hk_airhorn", "", "", "", "", ""},
        {"hk_come_on", "hk_whistle1", "hk_ohh2", "", "", "", "", ""},
        {"hk_yousuck", "hk_an_ass", "hk_boo", "hk_boo2", "", "", "", ""},
        {"hk_wahoo", "hk_wahoo2", "hk_airhorn", "hk_nice", "hk_oh_yeah", "hk_way_to_go", "", ""},
        {"hk_wahoo", "hk_airhorn", "hk_whistle2", "hk_whistle3", "hk_wahoo3", "hk_nice", "hk_oh_yeah",
         "hk_way_to_go"},
        {"hk_yousuck", "hk_yousuck2", "hk_momma", "hk_an_ass", "hk_boo", "hk_boo2", "hk_yousuck3", ""}};
    if (rules.type == RaceType::Cruise && net == nullptr) {
        for (size_t k = 0; k < crowdTricks.size(); ++k) {
            const size_t row = k / 8, column = k % 8;
            if (row == 4 && column == 7) continue;  // a null name
            const char* name = column == 0 ? kTricks[row] : "";
            if (sound3d) sound3d->player().find(name, crowdTricks[k]);
        }
    } else {
        crowdTricks.fill({-1, -1});
    }
    for (size_t k = 0; k < crowdMoods.size(); ++k) {
        if (sound3d) sound3d->player().find(kMoods[k / 8][k % 8], crowdMoods[k]);
    }
}

void TrackView::crowdFrame(float dt) {
    crowdInside = false;
    for (CrowdSounder& p : crowd) crowdSounder(p, dt);
    for (float& t : crowdTokens) t = t - dt;
}

void TrackView::crowdSounder(CrowdSounder& p, float dt) {
    if (paused || rules.career == nullptr) return;
    oj::Cruise& cruise = rules.career->cruise;
    if (p.zone >= cruise.zones.size()) return;
    const oj::CruiseZone& zone = cruise.zones[p.zone];
    oj::RaceCar* car = cruise.car;
    oj::Vec4 at{};
    if (car != nullptr) {
        const oj::Vec4 o = car->body->origin();
        at = {o[0] * 100.0f, o[1] * 100.0f, o[2] * 100.0f, o[3]};
        if (zone.contains(at)) {
            crowdInside = true;
            p.state = zone.state;
        }
    }
    if (!sound3d || car == nullptr || !zone.contains(at)) {
        p.level = -p.level * 0.01f + p.level;
        return;
    }
    p.level = (1.0f - p.level) * 0.3f + p.level;
    p.timer = p.timer - dt;
    if (!(p.timer < 0.0f)) return;
    oj::CrtRandom& random = oj::gameRandom();
    p.timer = oj::x87::unit(random.next()) * 4.0f + 0.5f;
    oj::audio::SetRef ref = crowdTricks[static_cast<size_t>((random.next() & 7) + zone.kind * 8) % 144];
    if (zone.done != 0 || ref.set == -1) {
        do {
            // The moods' table runs on into the kinds' ([0x00767718] + 0x180 is [0x00767898]).
            const auto k = static_cast<size_t>((random.next() & 7) + zone.state * 8);
            ref = k < 48 ? crowdMoods[k] : crowdTricks[(k - 48) % 144];
        } while (ref.set == -1);
    }
    // 0x0041A7E0: the first cooldown below 0 taken (1 s).
    bool token = false;
    for (float& t : crowdTokens) {
        if (t < 0.0f) {
            t = 1.0f;
            token = true;
            break;
        }
    }
    if (!token || ref.set == -1 || p.slots[p.ring] != nullptr) return;
    sound3d->playOnceRef(ref, {zone.matrix[3], zone.matrix[7], zone.matrix[11], 0.0f}, &p.slots[p.ring]);
    if (oj::audio::Instance* i = p.slots[p.ring]) {
        if (rules.type == RaceType::Cruise) {
            i->setRolloff(0.01f);  // [0x0072529C]
            i->setVolume(1.0f);    // [0x007116E0]
        } else {
            i->setRolloff(0.1f);
            i->setVolume(0.5f);    // [0x007116DC]
        }
        i->setLooped(0);
        i->setDoppler(0.0f);
        i->randomFrequency(6000, random);
        i->setMinDistance(1000.0f);
    }
    p.ring = (p.ring + 1) & 3;
}

void TrackView::airhorn(uint32_t hz) {
    if (!sound3d) return;
    if (airhornSound != nullptr) {
        airhornSound->release();
        airhornSound = nullptr;
    }
    oj::audio::Vec4 place{};
    if (race != nullptr && !race->route().grid.empty()) {
        const oj::Spl::GridPoint& g = race->route().grid.front();
        place = {g.x, g.y, g.z, g.heading};
    }
    sound3d->playOnce("hk_airhorn", place, &airhornSound);
    if (airhornSound == nullptr) return;
    airhornSound->setFrequency(hz);
    airhornSound->setVolume(2.0f);
    airhornSound->setRolloff(std::bit_cast<float>(0x3727C5ACu));  // 1e-5
    airhornSound->setRolloffScale(1.0f);
    airhornSound->setDoppler(0.0f);
}

void TrackView::roadSound(RaceSlot& s) {
    if (s.road == nullptr) return;
    const oj::Vec4& v = s.body->localVelocity;
    float speed = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    speed = speed < 0.0f ? 0.0f : (speed > 37.0f ? 37.0f : speed);  // [0x0072545C]
    speed = speed / 37.0f;
    bool sand = false;
    for (const oj::WheelLook& w : s.car.wheels) {
        sand = sand || w.surface == 1 || w.surface == 2 || w.surface == 4 || w.surface == 5;
    }
    float mul = 1.2f;
    if (sand) {
        s.road->setSet(s.roadSand.bank, s.roadSand.set);
        mul = 2.0f;
    } else {
        s.road->setSet(s.roadNow.bank, s.roadNow.set);
    }
    s.road->setRolloff(0.1f);
    s.road->setVolume(speed * mul);
    s.road->setMinDistance(450.0f);
}

void TrackView::pauseSounds() {
    if (soundsHeld || paused == soundsPaused) return;
    soundsPaused = paused;
    oj::Jukebox& music = oj::jukebox();
    if (music.player() != nullptr) {
        music.paused = paused;
        if (music.handle != 0) {
            if (paused) {
                music.player()->pause(music.handle);
            } else {
                music.player()->resume(music.handle);
            }
        }
    }
    if (oj::Speech* speech = oj::gameSounds().speech; speech != nullptr && speech->player() != nullptr) {
        speech->paused = paused;
        if (paused) {
            speech->player()->pauseAll();
        } else {
            speech->player()->resumeAll();
        }
    }
    if (sound3d) sound3d->manager().pauseAll(paused);
}

void TrackView::squealSound(RaceSlot& s) {
    CarParticles& p = s.particles;
    const oj::CarEffects& e = s.car.effects;
    oj::scene::Emitter* squeal = e.squeal == nullptr         ? nullptr
                                 : e.squeal == e.sandSqueal ? p.sandSqueal.get()
                                                            : p.tarmacSqueal.get();
    // A wheel's surface changing puts the squeal the car had to 0 before the car takes that surface's (0x004908B3).
    if (squeal != p.squeal) {
        if (p.squeal != nullptr) p.squeal->setLevel(0.0f);
        p.squeal = squeal;
    }
    if (squeal == nullptr) return;
    if (e.fastForward) {
        squeal->setLevel(0.0f);
        return;
    }
    auto levelOf = [&p](size_t i) { return p.skid[i] != nullptr ? p.skid[i]->level : 0.0f; };
    float m = levelOf(0) > 0.0f ? levelOf(0) : 0.0f;
    size_t w = levelOf(1) > m ? 1 : 0;
    if (levelOf(1) > m) m = levelOf(1);
    if (levelOf(2) > m) {
        w = 2;
        m = levelOf(2);
    }
    if (levelOf(3) > m) w = 3;
    const oj::WheelLook& look = s.car.wheels[w];
    squeal->setLevel(4.0f * look.slide + look.smoke + look.skid);
    if (squeal->sound != nullptr) squeal->sound->setMinDistance(500.0f);
    const oj::Vec4& v = s.body->localVelocity;
    const float speed = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    squeal->setFrequency(speed * 200.0f + e.squealPitch + 30000.0f);
}

float TrackView::effectsVolume() const {
    const DriverSettings& set =
        settings != nullptr && settings->has_value() ? **settings : DriverSettings::defaults(0);
    const uint32_t v = set.u32(DriverSettings::kEffectsVolume);
    return std::bit_cast<float>(v);
}

void TrackView::addSoundBank(const char* name, const std::filesystem::path& folder) {
    if (!sound3d) return;
    oj::audio::Player& p = sound3d->player();
    for (size_t i = 0; i < p.banks(); ++i) {
        if (_stricmp(p.bank(static_cast<int>(i))->name().c_str(), name) == 0) return;
    }
    const oj::GameFiles files(std::filesystem::current_path());
    if (auto bank = oj::audio::Bank::load(files, std::filesystem::path("audio") / folder, name, nullptr)) {
        p.addBank(bank.get());
        sound3dBanks.push_back(std::move(bank));
    }
}

void TrackView::startSound(const oj::scene::TrackSetup& setup) {
    // The race's world takes [G+0x50]: a window's before it released.
    oj::freeWindowWorld();
    first().rainSound = nullptr;
    airhornSound = nullptr;
    oj::raceSounds() = nullptr;
    oj::gameSounds().world = nullptr;
    sound3d.reset();
    sound3dBanks.clear();
    if (!oj::audio::system().on()) return;
    const oj::GameFiles files(std::filesystem::current_path());
    sound3d = std::make_unique<oj::audio::World>(files, 350, oj::gameRandom());
    oj::gameSounds().world = sound3d.get();
    const float v = 1.0f * effectsVolume();
    sound3d->target = v;
    sound3d->volume = v;
    sound3d->down = 20.0f;
    sound3d->up = 50.0f;
    sound3d->player().setMasterVolume(v);
    sound3d->manager().pauseAll(false);
    sound3d->manager().setGroupVolume(0, 0.25f);
    sound3d->manager().setGroupVolume(1, 1.0f);
    sound3d->manager().setGroupVolume(2, 0.95f);
    // 0x00461390: a cruise's bank only outside a network game ([0x0075EF8C] 0).
    if (gameData != nullptr && gameData->race.type == RaceType::Cruise && oj::net::netGame() == nullptr)
        addSoundBank("cruise");
    if (setup.weather == 2) addSoundBank("wet");
}

void TrackView::soundListener() {
    if (!sound3d) return;
    sound3d->manager().listeners().count = static_cast<uint32_t>(players.size());
    for (size_t i = 0; i < players.size() && i < 4; ++i) {
        RacePlayer& player = *players[i];
        // 0x0042068C: the place the camera's world matrix's (+0x38); the matrix the camera's view (its +0x48,
        // 0x00552A50: +0xC8, the world matrix's inverse, made as the camera was last drawn), whose rows take the
        // listener's offset to the world and the voices' places to the listener's axes.
        const oj::Mat34& w = player.camera.world();
        const oj::Mat34& v = player.camera.view();
        float x = 0.0f, z = 0.0f;
        player.raceCamera.listenerOffset(x, z);
        const oj::audio::Vec4 place{w[3] + (z * v[8] + x * v[0] + v[4] * 0.0f),
                                    w[7] + (x * v[1] + z * v[9] + v[5] * 0.0f),
                                    w[11] + (x * v[2] + z * v[10] + v[6] * 0.0f), 0.0f};
        sound3d->manager().setListenerData(place, v.data(), i);
    }
}

void TrackView::quietSkids(RaceSlot& s) {
    CarParticles& p = s.particles;
    const oj::CarEffects& e = s.car.effects;
    oj::scene::Emitter* squeal = e.squeal == nullptr ? nullptr
                                 : e.squeal == e.sandSqueal ? p.sandSqueal.get()
                                                            : p.tarmacSqueal.get();
    auto* tracks = dynamic_cast<oj::scene::TracksEmitter*>(p.tracks.get());
    for (size_t i = 0; i < 4; ++i) {
        for (oj::scene::Emitter* quiet : {squeal, p.skid[i]}) {
            if (quiet == nullptr) continue;
            quiet->setLevel(0.0f);
            if (auto* particles = dynamic_cast<oj::scene::ParticleEmitter*>(quiet)) particles->clear();
        }
        if (tracks != nullptr) {
            tracks->mark(i, nullptr, 0.0f, 0.0f);
            tracks->setLevel(0.0f);
            tracks->resetTracks();
        }
    }
}

}  // namespace app
