// The race's load: the track's scene, the players, the world, the cars, the sounds' banks and the start of the race.

#include "Game/Race/TrackView/TrackView.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Core/Base/X87.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Race/StartingGrid.h"
#include "Core/Network/OnlineRecords.h"

#include <windows.h>

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <string_view>
#include <utility>

namespace app {

bool TrackView::start(oj::scene::Engine& engine, const std::string& what) {
    oj::scene::TrackSetup setup;
    std::vector<std::string> parts;
    size_t from = 0;
    while (from <= what.size()) {
        const size_t slash = what.find('/', from);
        parts.push_back(what.substr(from, slash == std::string::npos ? std::string::npos : slash - from));
        if (slash == std::string::npos) break;
        from = slash + 1;
    }
    if (parts.size() < 2) {
        oj::log::warning("--track wants <Track>/<Route>");
        return false;
    }
    setup.track = parts[0];
    setup.route = parts[1];
    if (parts.size() > 2) setup.timeOfDay = static_cast<uint32_t>(std::atoi(parts[2].c_str()));
    if (parts.size() > 3) setup.weather = static_cast<uint32_t>(std::atoi(parts[3].c_str()));
    return start(engine, setup);
}

bool TrackView::start(oj::scene::Engine& engine, const oj::scene::TrackSetup& setup) {
    for (std::unique_ptr<RacePlayer>& p : players) p->raceCamera.release(p->camera);
    makePlayers();
    places.clear();
    frontEnd.hud.reset();
    frontEnd.complete.reset();
    frontEnd.results.reset();
    frontEnd.challengeResults.reset();
    frontEnd.sprintResults.reset();
    frontEnd.cruiseResults.reset();
    frontEnd.pauseWindow.reset();
    frontEnd.options.reset();
    frontEnd.message.reset();
    frontEnd.screenWindow.reset();
    frontEnd.closedownFrames = 0;
    frontEnd.unloadAsked = false;
    paused = false;
    frontEnd.window = {};
    frontEnd.windowAsked.clear();
    hudEngine = &engine;
    trackName = setup.track;
    routeLine = setup.route;
    timeOfDay = setup.timeOfDay;
    weather = setup.weather;
    frontEnd.hudReplay.reset();
    netDetach();
    race.reset();
    cars.clear();
    ownCars.clear();
    world.reset();
    scene = std::make_unique<oj::scene::TrackScene>(engine);
    scene->barrelCrash = [this](float frameTime) {
        if (rules.career != nullptr) rules.career->cruise.crash(2000.0f, nullptr, frameTime);
    };
    loading = oj::CrtRandom{};
    // 0x00461390: the 3D sounds' world first, then the effects of the race (the common ones, then those of its
    // weather) before the track. Two of the platform's timers (0x005A23E0) time the load and its parts, which go
    // to the debugger's output as it ends (beginRace).
    loadTotal.restart();
    loadPart.restart();
    startSound(setup);
    loadSoundSeconds = loadPart.lap();
    effects = oj::EffectLibrary{};
    const oj::GameFiles files(std::filesystem::current_path());
    auto readEffects = [&](const char* name) {
        if (const auto text = files.read(std::filesystem::path("scripts") / name)) {
            effects.read(std::string_view(reinterpret_cast<const char*>(text->data()), text->size()),
                         particleDensity, sound);
        }
    };
    readEffects("effectsdefs_com.txt");
    readEffects(setup.weather == 2 ? "effectsdefs_wet.txt" : "effectsdefs_dry.txt");
    loadEffectsSeconds = loadPart.lap();
    if (const auto text = files.read(std::filesystem::path("scripts") / "surfaceeffects.txt")) {
        surfaces = oj::parseSurfaceEffects(std::string_view(reinterpret_cast<const char*>(text->data()),
                                                            text->size()));
    }
    std::string why;
    oj::scene::TrackSetup withEffects = setup;
    withEffects.effects = &effects;
    withEffects.sound = sound;
    withEffects.soundWorld = sound3d.get();
    withEffects.density = particleDensity;
    if (!scene->load(withEffects, loading, &why)) {
        oj::log::warning("track not loaded: {}", why);
        scene.reset();
        return false;
    }
    // 0x0041DEC0 .. 0x0041DF1A, as each player is made: the view's clear colour (+0x0C) the atmosphere's sky colour
    // (0x005342C0: +0x90, mixed with the next atmosphere's by the blend while one comes), 0xFFAAC4DD without one;
    // nothing sets it again during the race. Then its camera (0x00401760) with the weather it carries.
    for (std::unique_ptr<RacePlayer>& p : players) {
        const oj::render::AtmosphereSet* set = scene->atmosphere();
        if (set != nullptr && set->current != nullptr) {
            p->viewClear = set->next != nullptr
                               ? oj::render::mixColour(set->next->skyColour, set->current->skyColour, set->blend)
                               : set->current->skyColour;
        } else {
            p->viewClear = 0xFFAAC4DDu;
        }
        makeCameraWeather(*p);
    }
    // 0x004254AF: the effects manager's lists sorted by type.
    if (oj::scene::ParticleSystem* system = scene->particles()) system->sortLists();
    // The end of the race track's making (0x00422AC0): the objects' first step; each atmosphere zone takes the
    // track's atmosphere of its name (0x00533A70: the first of the set's 20 whose name is the zone's, any case).
    scene->sceneObjects().finishTrack(loading);
    if (oj::render::AtmosphereSet* set = scene->atmosphere(); set != nullptr) {
        for (oj::SceneAtmosZone& z : scene->sceneObjects().atmosZones()) {
            z.atmosphere = nullptr;
            for (const oj::Atmosphere& a : set->list) {
                if (_stricmp(a.name.c_str(), z.name.c_str()) == 0) {
                    z.atmosphere = &a;
                    break;
                }
            }
        }
    }
    scene->startObjectLights();
    route = std::make_unique<oj::Route>(scene->lines());
    places = oj::scene::gridPlaces(scene->lines());
    const oj::SceneObjects& objects = scene->sceneObjects();
    oj::log::info("track: scene objects: {} mesh nodes, {} emitters, {} objects, {} physics objects, {} cameras, "
                  "{} riding splines, {} cruise zones, {} rand() values drawn by the load", objects.meshNodes(),
                  objects.emitters(), objects.objects().size(), objects.physics().size(), objects.cameras().size(),
                  objects.splines().size(), objects.moveZones().size(), loading.draws());
    for (std::unique_ptr<RacePlayer>& p : players) gridCamera(*p);
    return true;
}

void TrackView::makePlayers() {
    pausing = nullptr;
    players.clear();
    const int32_t count = gameData != nullptr ? std::clamp(gameData->race.players, 1, 4) : 1;
    static constexpr float kViews[4][4][4] = {
        {{0.0f, 0.0f, 1.0f, 1.0f}},
        {{0.0f, 0.0f, 1.0f, 0.5f}, {0.0f, 0.5f, 1.0f, 1.0f}},
        {{0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 1.0f, 0.5f}, {0.0f, 0.5f, 0.5f, 1.0f}},
        {{0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 1.0f, 0.5f}, {0.0f, 0.5f, 0.5f, 1.0f}, {0.5f, 0.5f, 1.0f, 1.0f}},
    };
    for (int32_t i = 0; i < count; ++i) {
        auto p = std::make_unique<RacePlayer>();
        p->index = i;
        p->controller = i;
        const float* v = kViews[count - 1][i];
        p->viewLeft = v[0];
        p->viewTop = v[1];
        p->viewRight = v[2];
        p->viewBottom = v[3];
        p->postView.glow = gameData == nullptr || gameData->race.mode != RaceMode::SplitScreen;
        p->raceCamera.setDirector(&p->director);
        p->raceCamera.setIntro(&p->intro);
        players.push_back(std::move(p));
    }
}

void TrackView::prepareRace(int32_t raceLaps, const oj::RaceRules& raceRules) {
    laps = raceLaps;
    rules = raceRules;
    // 0x004BEA60 on the race's HUD as the drivers' levels and the chatter ask.
    rules.ordersFlash = [this](int32_t racer, float frameTime) {
        if (frontEnd.hud != nullptr) frontEnd.hud->ordersFlash(hudRace(), racer, frameTime);
    };
    // The race's load (0x00425501): a replay not playing is made ready (0x00426B00) and left stopped, the race
    // not a replay's.
    if (replay.playing == 0) {
        replay.reset();
        replay.playing = 0;
        replay.restartAsked = 0;
        replay.state = oj::ReplayState::Stopped;
        replay.replayRace = 0;
    }
    rules.replay = replay.playing != 0;
    rules.replayRace = replay.replayRace != 0;
    replayBegun = false;
    race = std::make_unique<oj::RaceState>(*route, laps, rules);
    race->reset(loading, 0.0f);
    // The track's making (0x00421D50 -> 0x004B01D0): the cruise manager's zone of each Move* helper; at its end
    // (0x00422AC0) in a cruise each made ready (0x004B1510).
    if (rules.career != nullptr && frontEnd.hudTexts != nullptr) {
        oj::Cruise& cruise = rules.career->cruise;
        cruise.zones.clear();
        for (const oj::SceneMoveZone& z : scene->sceneObjects().moveZones()) cruise.addZone(z, *frontEnd.hudTexts);
        if (rules.type == RaceType::Cruise) {
            for (oj::CruiseZone& z : cruise.zones) oj::Cruise::resetZone(z);
        }
    }
}

void TrackView::addCar(const oj::CarDynamics& dynamics, const oj::Car& source, oj::Car* kept, const oj::GameData& data,
                       oj::scene::CarModel* model, const oj::Spl::GridPoint& place, oj::CarKind kind, int32_t crew,
                       int32_t record, bool refresh) {
    if (scene == nullptr || scene->collision() == nullptr || model == nullptr || source.def == nullptr) return;
    const auto renew = [refresh](oj::Car& c) {
        c.neonFlickers = 0;
        if (!refresh) return;
        c.tyreWear = 0;
        c.brakeWear = 0;
        c.nitro = oj::Car::bitsOf(15.0f);
    };
    if (kept != nullptr) renew(*kept);
    oj::Car car = source;
    renew(car);
    // 0x00485E5B, with -automod: N not 0 modifies the car automatically (0x0048F2C0 with the loading thread's
    // rand(); N 4 as a prototype of parts of any level, else of level N), then its +0x0F 1; both after the
    // model was made from the car as it was. The game's counter of modifications ([0x00717868]) moves on.
    if (automod) {
        if (automodValue != 0) {
            oj::CarMaking making{data.defs, data.catalogue, loading, const_cast<int&>(data.autoModifyCounter)};
            const int level = automodValue == 4 ? -1 : static_cast<int>(automodValue);
            oj::autoModify(car, automodValue == 4, level, making);
        }
        car.mods[oj::kExhaust] = 1;
        if (kept != nullptr) *kept = car;
    }
    oj::CarDynamics fitted = dynamics;
    oj::Car fittedCar = car;
    oj::fitParts(fitted, fittedCar, data.catalogue);
    oj::loadShapes(fitted.body, data.files(), std::filesystem::path("cars") / car.def->name);
    if (!world) makeWorld();
    auto slot = std::make_unique<RaceSlot>();
    slot->body = std::make_unique<oj::CarBody>(fitted, oj::Vec4{});
    world->add(*slot->body);
    slot->car.kind = kind;
    slot->car.body = slot->body.get();
    // 0x00486411: the car's tracker on the race's centre line (0x00419E10).
    slot->car.centreLine = scene->sceneObjects().centreLine();
    slot->car.crew = crew;
    slot->car.record = record;
    slot->car.model = car.def->id;
    slot->car.drive = oj::driveOf(car, data.tables);
    slot->car.hasNitro = car.mods[oj::kNitro] != oj::kNoMod;
    // 0x00485EE9: with a nitro part and none left the HUD's low nitro message counts as given (+0x198 1).
    if (slot->car.hasNitro) slot->car.nitroWarning = oj::Car::asFloat(car.nitro) == 0.0f ? 1 : 0;
    slot->car.hasTurbo = car.mods[oj::kTurbo] != oj::kNoMod || car.def->stockTurbo;
    // 0x00485A0E: the exhaust's nodes by the model's parts.
    if (model->hasNode("EXHAUST_C")) {
        slot->car.exhausts = {true, false};
    } else if (model->hasNode("EXHAUST_S") && model->hasNode("EXHAUST_O")) {
        slot->car.exhausts = {true, true};
    } else {
        slot->car.exhausts = {model->hasNode("EXHAUST_S"), false};
    }
    // 0x00485A0E: the nodes themselves, at their places.
    if (slot->car.exhausts[0]) {
        const char* name = slot->car.exhausts[1] || !model->hasNode("EXHAUST_C") ? "EXHAUST_S" : "EXHAUST_C";
        slot->exhaustNodes = {model->node(name), slot->car.exhausts[1] ? model->node("EXHAUST_O") : nullptr};
    }
    for (size_t i = 0; i < 2; ++i) {
        if (const oj::render::Node* n = slot->exhaustNodes[i]; n != nullptr) {
            slot->exhaustPlaces[i] = {n->local[3], n->local[7], n->local[11], 0.0f};
        }
    }
    slot->car.car = kept;
    // 0x00485CC5: the car's own grip (+0x78), which its tyres' wear lessens, is its last tyre's LateralMu.
    if (kept != nullptr) kept->grip = std::bit_cast<uint32_t>(slot->body->tyreGrip());
    // 0x00486431: the rear bumper's threshold by the model.
    const int32_t id = car.def->id;
    slot->car.rearThreshold = id == 5 || id == 7 || id == 11 || id == 19 || id == 35 ? 0.1f : 0.5f;
    slot->model = model;
    // 0x00485270 for the four wheels, then 0x00485D20 (Game/Cars/CarWheels).
    oj::makeWheelPoses(slot->wheels, *model, *slot->body);
    if (kind == oj::CarKind::Player) slot->body->weighAxles(slot->wheels[0].base[2], slot->wheels[2].base[2]);
    slot->body->raceTune(kind == oj::CarKind::Ai, slot->car.drive);
    slot->chassisRadius = model->nodeRadius("_CHASS");
    // 0x004858F9: the BONNET_CAM node's place (default (0, 90, 80)).
    if (const oj::render::Node* n = model->node("BONNET_CAM"); n != nullptr) {
        slot->bonnetCamera = {n->local[3], n->local[7], n->local[11], 0.0f};
    }
    // 0x00485200: the brake lights' and the reversing lights' nodes start hidden. 0x00486340: the lit lamps
    // (LIGHTSON) shown at night (the atmosphere's +0x170) or in the wet (+0x174), hidden otherwise.
    slot->chassis = model->node("_CHASS");
    slot->brakeLight = model->meshNode("_BRKLIGHT");
    if (slot->brakeLight != nullptr) slot->brakeLight->flags |= 4u;
    slot->reverseLight = model->meshNode("_REVLIGHT");
    if (slot->reverseLight != nullptr) slot->reverseLight->flags |= 4u;
    // The brake lights' flares start off; the headlights' are on at night or in the wet.
    slot->headFlares = {model->flare("HLIGHTON_R"), model->flare("HLIGHTON_L")};
    slot->brakeFlares = {model->flare("RBRKLIGHTON_R"), model->flare("RBRKLIGHTON_L")};
    for (oj::scene::CarFlare* f : slot->brakeFlares) {
        if (f != nullptr) f->on = false;
    }
    const oj::render::AtmosphereSet* set = scene->atmosphere();
    const bool lightsOn =
        set != nullptr && set->current != nullptr && (set->current->night != 0 || set->current->wet != 0);
    slot->lightsOn = lightsOn;
    for (oj::scene::CarFlare* f : slot->headFlares) {
        if (f != nullptr) f->on = lightsOn;
    }
    if (oj::render::ModelNode* lit = model->meshNode("LIGHTSON"); lit != nullptr) {
        if (lightsOn) {
            lit->flags &= ~4u;
        } else {
            lit->flags |= 4u;
        }
    }
    makeProjectors(*slot, car);
    // 0x00486408: the car's chassis stirs the particles' wind.
    if (scene->particles() != nullptr && slot->chassis != nullptr) {
        scene->particles()->wind().addSource(*slot->chassis);
    }
    slot->place = place;
    // The car's damage grid: the record's grid object (0x0048A500 adds the model's nodes to it), here its cells.
    slot->damage = std::make_unique<oj::DamageGrid>();
    if (car.grid) slot->damage->take(*car.grid);
    slot->car.damage = slot->damage.get();
    slot->record = car;
    slot->car.carRecord = &slot->record;
    slot->car.revs.make(loading, 0.0f);
    race->add(slot->car, loading, 0.0f);
    // 0x00425750 (0x0041E0B0): the players take the race's players' cars, player i the race's car i (a custom
    // race's players come first on the grid).
    if (kind == oj::CarKind::Player) {
        for (std::unique_ptr<RacePlayer>& p : players) {
            if (p->slot != nullptr) continue;
            p->slot = slot.get();
            if (p.get() == &first()) {
                race->setPlayer(&p->slot->car);
                cruiseReset();
            }
            if (autopilot) p->slot->car.kind = oj::CarKind::Ai;
            break;
        }
    }
    oj::log::info("race: car {} {} ({}) on grid place ({:.0f}, {:.0f}, {:.0f})", cars.size(), car.def->name,
                  kind == oj::CarKind::Player ? "player" : "AI", place.x, place.y, place.z);
    cars.push_back(std::move(slot));
}

void TrackView::beginRace() {
    if (cars.empty() || race == nullptr) return;
    // 0x0048FEA0: each car's effects object, from where the car is made.
    const oj::render::AtmosphereSet* atmosphere = scene->atmosphere();
    const bool wet = atmosphere != nullptr && atmosphere->current != nullptr && atmosphere->current->wet != 0;
    for (std::unique_ptr<RaceSlot>& s : cars) {
        oj::makeEffects(s->car, effects, surfaces, rules.mode, rules.type, wet, loading, sound);
        // 0x0048FD5C: the volume group 3 at 1; 0x0048FD79: the crunches' sets found once a run ([0x0079E7E0]
        // never cleared: later races keep the first's).
        if (sound3d) {
            sound3d->manager().setGroupVolume(3, 1.0f);
            if (!suspensionCrunchFound) {
                suspensionCrunchFound = true;
                const char* names[3] = {"suspcrunch1", "suspcrunch2", "suspcrunch3"};
                for (size_t k = 0; k < 3; ++k) sound3d->player().find(names[k], suspensionCrunch[k]);
            }
        }
        oj::startJolt(s->car);
        makeCarParticles(*s);
    }
    impactFrame = true;
    race->linkNeighbours();
    settleRace(loading);
    // 0x004972B9: the cars loaded (0x00495E48 began the line).
    oj::log::debugPrint("Done!\n");
    makeCarSounds();
    // 0x00461390: the race (0x00424270, with the cars) timed.
    const float trackSeconds = loadPart.lap();
    // The load goes on after the race track (0x00461390): the cruise manager's zones (0x004B0330), then the
    // race front end (0x004563E0) with its windows; the HUD's window of the race's type loads with its map
    // (0x0044A100: the models draw the loading thread's rand()).
    crowdSets();
    const std::vector<int32_t> crowdDraws = scene->sceneObjects().makeMoveZones(loading);
    // 0x0041A430: a sounder for each child of each zone, its level 1 and its first delay drawn (r x 30 / 32767
    // + 1), the cooldowns 0 (0x0041A170).
    crowd.clear();
    crowdTokens.fill(0.0f);
    size_t drawn = 0;
    const auto& helpers = scene->sceneObjects().moveZones();
    for (size_t z = 0; z < helpers.size(); ++z) {
        for (size_t c = 0; c < helpers[z].children && drawn < crowdDraws.size(); ++c) {
            CrowdSounder p;
            p.zone = z;
            p.timer = static_cast<float>(crowdDraws[drawn++]) * std::bit_cast<float>(0x3A7001E0u) + 1.0f;
            crowd.push_back(p);
        }
    }
    loadPart.lap();  // the cruise zones' time, not shown
    frontEnd.load(rules, hudEngine, wheels, effectsVolume(), loading);
    // 0x004618A9: the race front end timed, then the load's times to the debugger's output.
    const float frontEndSeconds = loadPart.lap();
    const float loadSeconds = loadTotal.lap();
    oj::log::debugPrint("TRACK LOAD TIME : %.2f secs\n", static_cast<double>(loadSeconds));
    oj::log::debugPrint("  SoundMgr      : %.2f secs\n", static_cast<double>(loadSoundSeconds));
    oj::log::debugPrint("  EffectsDefs   : %.2f secs\n", static_cast<double>(loadEffectsSeconds));
    oj::log::debugPrint("  RaceTrack     : %.2f secs\n", static_cast<double>(trackSeconds));
    oj::log::debugPrint("  InGameUI      : %.2f secs\n", static_cast<double>(frontEndSeconds));
    racing = true;
    if (first().slot != nullptr) {
        oj::log::info("race: the player's car settled at ({:.2f}, {:.2f}, {:.2f}), {} tyres down",
                      first().slot->body->position[0], first().slot->body->position[1],
                      first().slot->body->position[2], first().slot->body->tyreCount);
    }
    // Each player's camera without a fly-by (0x00401E80, oj::startView): the kept view of a solo race or a cruise
    // (the first player's, whichever the player) with the race's count, a sprint's later heat's sweep with the count
    // 8, the network's sweep, or (a route without a fly-by, 0x00405BE0) the count 6 and the sweep.
    for (std::unique_ptr<RacePlayer>& p : players) {
        if (p->introPlaying) continue;
        switch (oj::startView(rules.type, rules.heat, net != nullptr)) {
        case oj::StartView::Kept:
            p->raceCamera.take(viewKept(first()), true, cameraTarget(*p), cameraRace(*p), p->camera);
            for (const int asked : p->raceCamera.takeWindowAsks()) chooseWindow(asked != 0);
            break;
        case oj::StartView::LaterHeat:
            race->count = 8.0f;
            if (hasCamera()) {
                p->raceCamera.take(oj::scene::RaceCamera::kBeforeStart, true, cameraTarget(*p), cameraRace(*p),
                                   p->camera);
            }
            for (const int asked : p->raceCamera.takeWindowAsks()) chooseWindow(asked != 0);
            break;
        case oj::StartView::Sweep: netSweep(*p); break;
        case oj::StartView::FlyBy: countAndSweep(*p); break;
        }
    }
}

void TrackView::giveModelToGrid(RaceSlot& s, oj::CrtRandom& random) {
    if (s.damage) {
        oj::DamageGrid& grid = *s.damage;
        auto add = [&](const char* name, bool body) -> oj::DamageZone* {
            const auto points = s.model->damagePoints(name);
            return points ? grid.addNode(*points, body) : nullptr;
        };
        add("_CHASS", true);
        std::array<oj::DamageZone*, 11>& zones = s.car.zones;
        zones[0] = add("FBUMP", true);
        zones[1] = add("RBUMP", true);
        zones[2] = add("BONNET", true);
        zones[4] = add("SSKIRT_L", true);
        zones[5] = add("SSKIRT_R", true);
        zones[9] = add("DECAL", false);
        zones[10] = add("SPONS", false);
        grid.finish(random);
    }
    makeLightSteps(s);
}

void TrackView::finishLoad() {
    oj::CrtRandom& random = oj::gameRandom();
    for (std::unique_ptr<RaceSlot>& s : cars) {
        oj::DamageGrid& grid = *s->damage;
        giveModelToGrid(*s, random);
        const std::array<oj::DamageZone*, 11>& zones = s->car.zones;
        auto count = [](const oj::DamageZone* z) { return z != nullptr ? static_cast<int>(z->count) : -1; };
        oj::log::info("race: car {} damage grid: {} cells; vertices FBUMP {} RBUMP {} BONNET {} SSKIRT {} {} "
                      "DECAL {} SPONS {}", &s - cars.data(), grid.cellCount(), count(zones[0]), count(zones[1]),
                      count(zones[2]), count(zones[4]), count(zones[5]), count(zones[9]), count(zones[10]));
    }
    if (scene != nullptr) scene->loaded();
    if (route != nullptr) {
        int32_t location = 0;
        if (gameData != nullptr) {
            for (const oj::ExeTables::Route& r : gameData->tables.routes) {
                if (_stricmp(r.track.c_str(), trackName.c_str()) == 0) {
                    location = r.location;
                    break;
                }
            }
        }
        const char* name = location > 0 && (location <= 2 || location == 8) ? "Leaves" : "Litter";
        oj::scene::ParticleSystem* system = scene != nullptr ? scene->particles() : nullptr;
        size_t made = 0;
        for (const oj::RoutePoint& p : route->line(0)) {
            if (!(oj::x87::unit(random.next()) < 0.3f)) continue;
            if (oj::makeEmitter(effects, name, random, sound) == nullptr || system == nullptr) continue;
            const std::shared_ptr<oj::scene::Emitter> e = system->make(name);
            if (!e) continue;
            // Its place, then its slot 1 (LEAVES: cleared with this thread's rand()).
            e->local = {p.x, p.y, p.z, 0.0f};
            e->sampleLight();
            ++made;
        }
        oj::log::info("race: {} {} emitters on the fast line", made, name);
    }
    // 0x00425750 in the career: the player's car the racer of crew 0, member 0 (0x004BAE70, here the car of the
    // player's own racer); without one (0x0041E0B0) the camera on the car of 0x004BB1A0.
    watching = rules.mode == RaceMode::Career && rules.career != nullptr && first().slot == nullptr && !cars.empty();
    // 0x00425750 on the network: without a car of ours (only watching) the camera on the race's first car.
    if (net != nullptr && first().slot == nullptr) {
        watching = true;
        first().viewed = cars.empty() ? nullptr : cars[0].get();
        oj::log::info("race: the network game watched, the camera on the first car");
    } else if (watching) {
        first().viewed = racerCamera();
        oj::log::info("race: watched, the camera on car {}",
                      first().viewed != nullptr ? first().viewed->car.record : -1);
    }
}

RaceSlot* TrackView::racerCamera() const {
    if (rules.career == nullptr) return cars.empty() ? nullptr : cars[0].get();
    const oj::Career& career = *rules.career;
    const oj::RaceSession& session = career.session;
    const auto slotOf = [&](uint32_t i) -> RaceSlot* {
        for (const std::unique_ptr<RaceSlot>& c : cars) {
            if (c->car.record == static_cast<int32_t>(i)) return c.get();
        }
        return nullptr;
    };
    const uint32_t count = std::min<uint32_t>(session.count, static_cast<uint32_t>(session.racers.size()));
    for (uint32_t i = 0; i < count; ++i) {
        if (session.racers[i].crew == 0) return slotOf(i);
    }
    for (uint32_t i = 0; i < count; ++i) {
        const auto crew = static_cast<size_t>(session.racers[i].crew);
        if (crew < session.odds.size() && session.odds[crew].staked != 0) return slotOf(i);
    }
    if (career.bets.placed != 0) {
        for (uint32_t i = 0; i < count; ++i) {
            if (session.racers[i].crew == career.bets.crew) return slotOf(i);
        }
    }
    return slotOf(0);
}

oj::Spl::GridPoint TrackView::gridPlace(size_t i) const {
    const RaceSlot& s = *cars[i];
    const std::vector<oj::Spl::GridPoint>& grid = scene->lines().grid;
    if (net != nullptr || grid.empty()) return s.place;
    const size_t index = oj::startingGridIndex(rules.mode, rules.type, rules.heat, cars.size(), i);
    return grid[index % grid.size()];
}

void TrackView::settleRace(oj::CrtRandom& random) {
    race->reset(random, 0.0f);
    for (size_t i = 0; i < cars.size(); ++i) {
        std::unique_ptr<RaceSlot>& s = cars[i];
        const oj::Spl::GridPoint place = gridPlace(i);
        if (rules.type == RaceType::Sprint) s->body->nitroRate = 2.5f;
        s->car.steeringFault = false;
        s->car.turboLost = false;
        s->car.nitroLost = false;
        s->car.neonDamaged = false;
        oj::Competitor& c = *s->car.competitor;
        c.rollingStart = 0.0f;
        c.rollingDelay = 0.0f;
        c.gridSkid = false;
        if (s->car.hasNitro && s->car.carRecord != nullptr) {
            s->car.nitroWarning = oj::Car::asFloat(s->car.carRecord->nitro) == 0.0f ? 1 : 0;
        }
        oj::settleOnGrid(*s->body, world->pools(), {place.x, place.y, place.z, 0.0f}, place.heading, firstSettle,
                         world.get(), !rules.replayRace);
        oj::placeTracker(s->car);
        if (s->sound) s->sound->reset();  // +0x498's +0x34
    }
    placeCars();
}

void TrackView::countAndSweep(RacePlayer& player) {
    race->count = 6.0f;
    if (hasCamera()) {
        player.raceCamera.take(oj::scene::RaceCamera::kBeforeStart, true, cameraTarget(player), cameraRace(player),
                               player.camera);
    }
}

void TrackView::placeCars() {
    syncNodes();
    for (std::unique_ptr<RaceSlot>& s : cars) {
        s->model->root()->local = s->car.node;
        s->model->root()->invalidate();
    }
}

}  // namespace app
