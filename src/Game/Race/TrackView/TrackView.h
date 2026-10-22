#pragma once
// The race (and the view of a route of --track): its scene, players, cars, world, sounds, windows and network, made
// as the stock game's race loads (0x00424270) and run each frame (docs/spec/16-physics.md, 17-race-camera.md,
// 22-race-hud.md). The member functions are in the TrackView*.cpp files beside this header, by what they do.

#include "Audio/Voices.h"
#include "Core/Base/Random.h"
#include "Core/Cars/CarDamage.h"
#include "Core/Data/EffectDefs.h"
#include "Core/Data/GameData.h"
#include "Core/Data/GameTexts.h"
#include "Core/Data/Language.h"
#include "Core/Input/Input.h"
#include "Core/Network/NetCar.h"
#include "Core/Network/NetSession.h"
#include "Core/Network/OnlineSessions.h"
#include "Core/Physics/CarBody.h"
#include "Core/Physics/PhysicsWorld.h"
#include "Core/Race/DriverControl.h"
#include "Core/Race/RaceSession.h"
#include "Core/Race/RaceSounds.h"
#include "Core/Race/Racer.h"
#include "Core/Race/Replay.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Core/Saves/SaveDevice.h"
#include "Core/Saves/SaveEntries.h"
#include "Core/Tracks/Route.h"
#include "Game/App/DebugPages.h"
#include "Game/Cars/CarSound.h"
#include "Game/Cars/CarWheels.h"
#include "Game/Frontend/Frontend.h"
#include "Game/Media/Movies.h"
#include "Game/Media/Music.h"
#include "Game/Platform/Settings.h"
#include "Game/Race/RaceCamera.h"
#include "Game/Race/Hud/RaceSaves.h"
#include "Game/Race/Hud/RaceWindows.h"
#include "Game/Race/FrontEnd/RaceFrontEnd.h"
#include "Game/Race/RaceIntro.h"
#include "Game/Scene/PostEffects.h"
#include "Game/Scene/Scene3d.h"
#include "Game/Scene/TrackScene.h"
#include "Game/UI/UiSound.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace app {

using oj::RaceMode;
using oj::RaceType;
using oj::RaceWindowId;

// A car of the race: its body in the physics, what the race knows of it, its model.
// A race wheel's record (0x9C bytes from the car's +0x1A0, made by 0x00485270): its slot node, the slot's place and
// scale as the model made it, and how it is turned.
using oj::WheelPose;

// The emitters of a car's effects object (0x0048FEA0), each made where its definition was (the ones of a type not made
// here stay empty).
struct CarParticles {
    std::shared_ptr<oj::scene::Emitter> tracks, heatHaze, explodeSparks, explodeSmoke, chassisSmoke, scrapeSparks;
    std::shared_ptr<oj::scene::Emitter> flakes;
    std::array<std::shared_ptr<oj::scene::Emitter>, 2> flames;
    std::array<std::shared_ptr<oj::scene::Emitter>, 4> tarmacSkid, sandSkid, spray;
    std::shared_ptr<oj::scene::Emitter> tarmacSqueal, sandSqueal;
    std::array<oj::scene::Emitter*, 4> skid{};  // +0x70: the wheels' skid of their surface
    oj::scene::Emitter* squeal = nullptr;       // +0x90: the tyres' squeal of the surface
};

struct RaceSlot {
    std::unique_ptr<oj::CarBody> body;
    oj::RaceCar car;
    oj::scene::CarModel* model = nullptr;
    oj::Spl::GridPoint place{};
    std::unique_ptr<oj::DamageGrid> damage;
    oj::Car record;  // the car as the race started it (its HUD colours, +0x4E4)
    float chassisRadius = 0.0f;  // its _CHASS node's sphere radius (+0x10, +0x74)
    oj::render::ModelNode* brakeLight = nullptr;    // +0x3C: _BRKLIGHT
    oj::render::ModelNode* reverseLight = nullptr;  // +0x54: _REVLIGHT
    oj::render::Node* chassis = nullptr;            // the model's +0x0C: _CHASS
    // +0x14, +0x18: the exhausts' nodes (EXHAUST_C, else EXHAUST_S with EXHAUST_O, else EXHAUST_S) and their places
    // as the race made the car (+0x1C, +0x2C).
    std::array<oj::render::Node*, 2> exhaustNodes{};
    oj::Vec4 bonnetCamera{0.0f, 90.0f, 80.0f, 0.0f};  // +0x484 .. +0x48C: BONNET_CAM's place; read by nothing
    std::array<oj::Vec4, 2> exhaustPlaces{};
    bool lightsOn = false;                          // +0x458: the headlights on (night or wet)
    std::array<WheelPose, 4> wheels{};              // +0x1A0
    oj::scene::Projector* shadow = nullptr;         // +0x4C4: gen_shadow, with a neon gen_neon
    oj::scene::Projector* headBeam = nullptr;       // +0x4C8: headlight_b, at night
    // +0x4CC, +0x4D0 (0x00531380): the silhouette shadows; by day the sun's in the first, else the nearest lights'.
    std::array<oj::scene::Silhouette*, 2> silhouettes{};
    // The light steps of the model's nodes (0x0048A644): the chassis (the model's +0x90), the wheels, the parts.
    std::vector<oj::scene::LightStep> lights;
    CarParticles particles;
    std::array<oj::render::ModelNode*, 2> flameNodes{};  // the effects' +0x68: exhaust_flame under each exhaust
    // The lamps' flares: RBRKLIGHTON_R, _L (+0x40, +0x44), HLIGHTON_R, _L (+0x48, +0x4C).
    std::array<oj::scene::CarFlare*, 2> brakeFlares{};
    std::array<oj::scene::CarFlare*, 2> headFlares{};
    // With the sound on (0x00498410): the engine's sounds (+0x498), the road's (+0x4BC) with its sets (+0x49C the
    // current, +0x4A4 tarmacdry, +0x4AC tarmac in the wet, +0x4B4 sand; never found: bank 0, set 0) and the horn
    // (+0x4C0).
    std::unique_ptr<oj::CarSound> sound;
    // Each wheel's instance (+0x238 of its record, 0x00485270: "silence", group 1, not looping), the crunch's.
    std::array<oj::audio::Instance*, 4> wheelSounds{};
    oj::audio::Instance* road = nullptr;
    oj::audio::Instance* horn = nullptr;
    oj::audio::SetRef roadNow{0, 0};
    oj::audio::SetRef roadDry{0, 0};
    oj::audio::SetRef roadWet{0, 0};
    oj::audio::SetRef roadSand{0, 0};
};

// A player of the race (0x0041DE30, 0x58 bytes; the race track's list +0x0C, made by 0x00424270 for each of the race's
// players, [G+0x6C]+0x58): its number (+0x00), its car (+0x04; none watching), its controller (+0x08), its camera
// (+0x0C, 0x00401760: the car it follows, the weather it carries, its fly-by), the camera's director (+0x10,
// 0x004085A0), its view (+0x14, 0x00512130: its part of the screen, its clear colour, its glow), its finish (+0x18
// finished, +0x1C the race front end takes hud_race_complete) and the steering's rumble timer (+0x28); the driver's
// control (0x0041E200) and its keys.
struct RacePlayer {
    int32_t index = 0;           // +0x00
    RaceSlot* slot = nullptr;    // +0x04: its car
    // +0x08: its controller, the input manager's player whose device's it is (0 its +0x28, 1 its +0x2C); in a replay
    // (mode 4) and for a third or a fourth player the stock takes the controller of the player's number ([G+0xAC]).
    int32_t controller = 0;
    oj::scene::RaceCamera raceCamera;    // +0x0C
    oj::scene::CameraDirector director;  // +0x10
    oj::render::Camera camera;           // the camera's view of the world
    // Its view (+0x14, 0x0041DE99: 0x00512130 with the flags 0x0C | 2): its glow off in a split screen race
    // (0x0041DF28); its rectangle on the screen, its corners in pixels of the screen's width and height at the making
    // ([G+4]+0x208 / +0x20C), kept here as shares of the screen (the stock screen keeps one size).
    oj::scene::PostEffects::View postView;
    uint32_t viewClear = 0xFFAAC4DDu;  // the view's clear colour (+0x0C), taken as the player is made
    float viewLeft = 0.0f, viewTop = 0.0f, viewRight = 1.0f, viewBottom = 1.0f;
    // The camera's fly-by (its +0x1D0, 0x0041C350; played in its mode 9 by 0x00405BE0): played (the camera in mode 9)
    // and its first frame done.
    oj::scene::RaceIntro intro;
    bool introPlaying = false;
    bool introStarted = false;
    // The player's view: the settings' +0x8C (its save system object's), kept here without a driver's settings.
    int cameraMode = oj::scene::RaceCamera::kChase;
    // The race camera's weather (0x00401760, 0x00401AE0): the atmosphere's Wet (+0xD8) and LightBlur (+0xDC) as the
    // camera was made, the camera in a tunnel (+0x1A0) and the emitters it carries (+0x18C, +0x190, +0x194); the rain's
    // sound (+0x0C) and its levels (+0x10 the target, +0x14 now).
    bool cameraWet = false;
    float cameraLightBlur = 0.5f;
    bool cameraInTunnel = false;
    std::shared_ptr<oj::scene::Emitter> cameraRain, cameraSplash, cameraBlobs;
    oj::audio::Instance* rainSound = nullptr;
    float rainTarget = 0.0f;
    float rainLevel = 0.0f;
    std::vector<oj::scene::CameraRace::Car> cameraCars;
    RaceSlot* viewed = nullptr;       // the car the camera follows (+0x08), its own but in a replay
    RaceSlot* soundCamera = nullptr;  // the camera's car as 0x00401DB0 last took it (its sounds turned to it)
    oj::DriverControl control;
    oj::DriverKeys keys;
    oj::DriverActions actions;  // the frame's actions of its controller
    float rumbleTimer = 0.0f;   // +0x28: the pulling steering's rumble's time
    bool finished = false;      // +0x18
    bool raceComplete = false;  // +0x1C: the race front end takes hud_race_complete
};

// The route's scene of --track and of the race: the route loaded as the race loads it (the loading thread's
// rand() starts at 1), the camera 9 m behind the first grid place and 3 m up, looking 20 m ahead of it.
struct TrackView {
    // [game + 0x50]: the 3D sounds' world of the race (0x005B9260 in the load), its banks added (0x005B9430). First,
    // so that the scene's emitters and the cars give their instances back before it goes.
    std::vector<std::unique_ptr<oj::audio::Bank>> sound3dBanks;
    std::unique_ptr<oj::audio::World> sound3d;
    std::unique_ptr<oj::scene::TrackScene> scene;
    // The race's players (the race track's +0x0C, 0x00424270); the first is always there (a race watched has one
    // without a car).
    std::vector<std::unique_ptr<RacePlayer>> players = [] {
        std::vector<std::unique_ptr<RacePlayer>> v;
        v.push_back(std::make_unique<RacePlayer>());
        return v;
    }();
    RacePlayer& first() { return *players.front(); }
    const RacePlayer& first() const { return *players.front(); }
    // 0x00420570: every player has finished (their +0x18).
    bool allFinished() const;
    RacePlayer* pausing = nullptr;  // the race track's +0x10: the player who paused the race (0x0041F6EB)
    std::string cameraOffset;  // --track-camera
    bool alwaysDepthOfField = false;  // -rf ([0x0075F14C])
    // The world's nearest lights to a point for the night's silhouettes (0x0054C8E0 → 0x0054E850): the grid the race's
    // load builds (the world's slot 9 0x00548780 → 0x0054DF20, 0x0042435x) from the world's lights (+0xF4, slot 8
    // 0x00548760), which nothing adds to: with fewer than 2 there is no grid and never a light (Stock problem: the
    // night's silhouettes are made and never cast).
    oj::scene::Silhouette::NearestLights nearestLights;
    bool wideScreen = false;
    bool multiTurn = false;  // [0x0075F78C]: the registry's MultiTurn (0x0059C9EA)
    oj::CrtRandom loading;  // the loading thread's rand(): the track, then the cars (0x00424270)
    // The race load's two timers (0x00461390: the whole and its parts) and the parts' seconds (to the debugger).
    oj::debug::Stopwatch loadTotal;
    oj::debug::Stopwatch loadPart;
    float loadSoundSeconds = 0.0f;
    float loadEffectsSeconds = 0.0f;
    float particleDensity = 0.0f;   // [0x0075F794]
    bool sound = false;             // [0x0075F860]: the game's sound is on
    oj::EffectLibrary effects;      // the effects manager's definitions of the race
    oj::SurfaceEffects surfaces;    // [0x0075EF34]

    // The race's cars against the route's collision (the physics manager 0x00425D40), in the vehicles' manager's
    // order; the player's driven by the keys (0x0041E200), the others by the AI.
    std::unique_ptr<oj::PhysicsWorld> world;
    std::vector<std::unique_ptr<RaceSlot>> cars;
    // The race's own car records (the car list of the race's state, +0x18: a custom race's opponents).
    std::vector<std::unique_ptr<oj::Car>> ownCars;
    std::unique_ptr<oj::Route> route;
    std::unique_ptr<oj::RaceState> race;
    oj::RaceRules rules;
    int32_t laps = 1;
    oj::input::Input* input = nullptr;  // the input manager ([game + 0x18]) and the players' controllers
    oj::ff::Wheels* wheels = nullptr;   // its FF manager ([0x0075F5F8])
    float forceLoadMax = 0.0f;          // [0x0079E790]: the steering load's highest (read by nothing else)
    bool racing = false;
    uint64_t framesRaced = 0;
    int firstSettle = 75;  // [0x00725540]
    // The game's +0x98: set while the race's load runs, then flipped at the end of every frame; the cars' impacts
    // (0x004916F0) run on the frames it is set, the first frame of the race being one.
    bool impactFrame = true;
    std::optional<oj::save::Settings>* settings = nullptr;
    // The player's view in the first driver's settings: +0x8C + 4 x its number (0x00406BAE: by the players' manager's
    // current player, [[G+0x34]+0x0C]+0x18; without one +0x8C).
    int viewKept(const RacePlayer& p) const {
        const uint32_t at = oj::save::Settings::kView + 4u * static_cast<uint32_t>(p.index);
        return settings != nullptr && settings->has_value() ? static_cast<int>((*settings)->u32(at)) : p.cameraMode;
    }
    // 0x00406BAE: CHANGE_CAMERA's view kept in the settings.
    void keepView(RacePlayer& p, int mode);
    std::filesystem::path saveFolder;  // the options' front end writes reg.cf1 there
    oj::GameData* gameData = nullptr;  // the records are kept in it
    // What the race front end's windows ask of the race.
    struct FrontEndHost final : oj::RaceFrontEndHost {
        explicit FrontEndHost(TrackView* v) : view(v) {}
        TrackView* view;
        oj::HudRace hudRace() override { return view->hudRace(); }
        oj::RaceState* raceState() override { return view->race.get(); }
        oj::GameData* gameData() override { return view->gameData; }
        const std::filesystem::path& saveFolder() const override { return view->saveFolder; }
        int32_t players() const override { return view->rules.players; }
        int32_t pausingPlayer() const override { return view->pausing != nullptr ? view->pausing->index : -1; }
        const oj::net::NetSession* netSession() const override { return view->net; }
        void useUi() override { view->input->useUi(); }
        void followWindow(const oj::RaceWindowFrame& f) override { view->followWindow(f); }
        void followReplayWindow(const oj::ReplayWindowFrame& f) override { view->followReplayWindow(f); }
        void restartRace() override { view->restartRace(); }
        void startReplay(bool fromStart) override { view->startReplay(fromStart); }
        oj::Replay& replay() override { return view->replay; }
        void syncReplayFlags() override { view->syncReplayFlags(); }
        bool paused() const override { return view->paused; }
        void setPaused(bool paused) override { view->paused = paused; }
        void setSoundsHeld(bool held) override { view->soundsHeld = held; }
        void setSoundsPaused(bool paused) override { view->soundsPaused = paused; }
        void worldSoundsAtOnce() override {
            if (view->sound3d) {
                const float v = 1.0f * view->effectsVolume();
                view->sound3d->down = 20.0f;
                view->sound3d->up = 50.0f;
                view->sound3d->target = v;
                view->sound3d->volume = v;
                view->sound3d->player().setMasterVolume(v);
            }
        }
        void pauseWorldVoices() override {
            if (view->sound3d) view->sound3d->manager().pauseAll(true);
        }
        void netCruiseBoard() override { view->netCruiseBoard(); }
    } frontEndHost{this};
    // The race front end (0x004563E0): its windows, messages and notes over the race.
    oj::RaceFrontEnd frontEnd{frontEndHost};
    int32_t netNoteAnswer = 0;   // [0x0079E7D0]: the network note's answer (the host lost or gone)
    // The race front end's watch of the network (0x00457180): +0x24 the frames the network was seen down, +0x28 the
    // answer of its notes (the network's error, the account signed on elsewhere), +0x2C quitting, +0x30 the answer of
    // a client's note under a lost controller, +0x34 read before the error's note and set by nothing but to 0, +0x3C
    // and +0x40 the host's wait (9 s) before the session's reset, +0x5C and +0x60 -1 as the network comes back, read
    // by nothing.
    int32_t netLostFrames = 0;
    int32_t netErrorAnswer = 0;
    bool netQuitting = false;
    int32_t netClientAnswer = 0;
    int32_t netNoteHeld = 0;
    bool netHostWait = false;
    float netHostTime = 0.0f;
    std::array<int32_t, 2> netBack = {-1, -1};
    // This frame ended in the network's watch (0x00457AB6, 0x0045804B, 0x004581C8): no invitation's note drawn.
    bool netFrameEnded = false;
    // [[G+0x80]+0x74]+0x64: the racers' names' time (0x004BDF00): 2 s after a tactics key (or, watching, NEXTCAR or
    // SELECT) went down, running down by the frame time; the network's overlay (0x00456BA0) shows the cars' names while
    // it is above 0, the labels off the network (0x004BF320) the racers' names, else their "!". It is the career's
    // drivers' (RaceDrivers::namesTime, cleared by their set up 0x004BDCC0); this one without a career.
    float netNames = 0.0f;
    float& namesTime() { return gameData != nullptr && gameData->career ? gameData->career->drivers.namesTime : netNames; }
    bool driversParity = false;  // [0x0075EF64]: turned over by each frame of the drivers' levels (0x004BDF00)
    std::unique_ptr<oj::ui::Screen> netOverlay;  // the race front end's 2D drawing with goodtime11 (+0x48, +0x4C)
    // The game object's replay (+0x9C, kept from race to race), its window and the windows that start it.
    oj::Replay replay;
    bool replayBegun = false;   // the race manager's +0x24: the record began under the count's 4.005 s
    float clockScale = 1.0f;    // the game clock's speed ([G+4] slot 6): the replay's +0x30 while it plays
    float drawScale = 1.0f;     // the speed of this frame's clock, for the draw
    oj::audio::Instance* airhornSound = nullptr;  // the race state's +0x68
    bool soundsPaused = false;  // the race state's +0x20 as the sounds last followed it
    bool soundsHeld = false;    // paused by restart_race's enter (0x00455940) until its restart (0x00455A90)
    // The cruise's crowd: a sounder (0x30 bytes, vtable 0x006EEE80) for each child of each zone: its zone (+0x0C), the
    // zone's state seen (+0x10), a level (+0x14), its delay (+0x2C), a ring of four one-shots (+0x18) and its place
    // (+0x28); the four cooldowns [0x007289C0], [0x0075ECD4] a car in a zone; the sets of the kinds ([0x00767898],
    // kept from race to race, 0 before any) and of the moods ([0x00767718]).
    struct CrowdSounder {
        size_t zone = 0;
        int32_t state = 0;
        float level = 1.0f;
        float timer = 0.0f;
        std::array<oj::audio::Instance*, 4> slots{};
        uint32_t ring = 0;
    };
    std::vector<CrowdSounder> crowd;
    std::array<float, 4> crowdTokens{};
    bool crowdInside = false;
    static inline std::array<oj::audio::SetRef, 144> crowdTricks = [] {
        std::array<oj::audio::SetRef, 144> a;
        a.fill({0, 0});
        return a;
    }();
    static inline std::array<oj::audio::SetRef, 48> crowdMoods = [] {
        std::array<oj::audio::SetRef, 48> a;
        a.fill({0, 0});
        return a;
    }();
    // The race's sounds the race's code asks for (Core/Race/RaceSounds.h).
    struct Sounds final : oj::RaceSounds {
        TrackView* view = nullptr;
        bool gridSkid(const oj::RaceCar& car, oj::CrtRandom& random) override {
            if (!view->sound3d) return false;
            const oj::Vec4 o = car.body->origin();
            static oj::audio::Instance* owner = nullptr;  // [0x0079E804], one for every car
            view->sound3d->playOnce("gridskid", {o[0] * 100.0f, o[1] * 100.0f, o[2] * 100.0f, o[3]}, &owner);
            if (owner == nullptr) return false;
            owner->randomFrequency(8000, random);
            return true;
        }
        bool hasSpeech() override { return oj::gameSounds().speech != nullptr; }
        bool speaking() override { return oj::gameSounds().speech != nullptr && oj::gameSounds().speech->speaking(); }
        void say(int a, int b) override {
            if (oj::Speech* speech = oj::gameSounds().speech) speech->say(a, b);
        }
        void say2(int a, int b) override {
            if (oj::Speech* speech = oj::gameSounds().speech) speech->say2(a, b);
        }
        void raceInterface(const char* name) override {
            if (view->frontEnd.hud && !view->paused) view->frontEnd.raceUi.play(name);
        }
        void cruiseMusic(float volume) override {
            oj::GameSounds& g = oj::gameSounds();
            if (!(g.effects() > 0.5f)) return;
            const float v = g.music() * volume;
            oj::Jukebox& music = oj::jukebox();
            if (music.player() == nullptr || music.handle == 0) return;
            music.player()->setVolume(music.handle, v);
            music.volume = v;
            music.target = v;
        }
        void horn(const oj::RaceCar& car, bool on) override {
            for (std::unique_ptr<RaceSlot>& s : view->cars) {
                if (&s->car != &car || s->horn == nullptr) continue;
                s->horn->setMinDistance(1300.0f);
                if (on) {
                    s->horn->play();
                } else {
                    s->horn->stop();
                }
            }
        }
    };
    Sounds raceSounds;
    // [0x00721C48]: suspcrunch1 .. 3, found once a run ([0x0079E7E0]); -1 before.
    static inline std::array<oj::audio::SetRef, 3> suspensionCrunch{};
    static inline bool suspensionCrunchFound = false;
    // A race the player watches: the first player (0x0041DE30) without a car (+0x04 0, 0x00425750), its camera on a
    // racer's car (0x004BB1A0).
    bool watching = false;
    // The network game's session (none off it, [0x0075EF8C]) and the race's first frame told to it (0x00425D40's
    // +0x7C).
    oj::net::NetSession* net = nullptr;
    bool netLoaded = false;
    bool hasCamera() const { return first().slot != nullptr || watching; }
    uint32_t timeOfDay = 2;     // [G+0x6C]+0x6C
    uint32_t weather = 0;       // [G+0x6C]+0x70
    bool paused = false;        // the race state's +0x20 (0x004263E0)
    oj::DriverActions frameActions;  // the frame's driver actions
    bool autopilot = false;       // --autopilot
    bool automod = false;         // -automod N ([0x0075EF40], [0x0075EF44])
    long automodValue = 0;
    oj::scene::Engine* hudEngine = nullptr;
    std::string trackName;
    std::string routeLine;
    std::vector<oj::Vec4> places;                     // the route's grid points (race +0x30: +0x4C, +0xAC)

    // "<Track>/<Route>[/<time of day>[/<weather>]]"
    bool start(oj::scene::Engine& engine, const std::string& what);

    bool start(oj::scene::Engine& engine, const oj::scene::TrackSetup& setup);

    // 0x00424965, as the race track is made: a player for each of the race's players ([G+0x6C]+0x58; OpenJuice keeps
    // one for a race of none, as for --track) made by 0x0041DE30: its number, its controller (the first player's the
    // input manager's +0x28, the second's its +0x2C), its view's corners (the screen W x H: one player (0, 0) ..
    // (W, H); two the top half and the bottom half; three (0, 0) .. (W/2, H/2), (W/2, 0) .. (W, H/2), (0, H/2) ..
    // (W/2, H); four the four quarters), the view's number (+0x2C), its glow off in a split screen race (mode 3); the
    // camera with its director and its fly-by.
    void makePlayers();

    // 0x00495DE0 before the cars: the race's state (0x0049FA40: the route, the laps, its reset 0x0049FDF0).
    void prepareRace(int32_t raceLaps, const oj::RaceRules& raceRules);

    // The race's physics world (the scene objects' manager's +0x58) with the physics objects of the route's scene
    // objects, which joined it as the load made them (0x0053E450), before the cars.
    void makeWorld() {
        world = std::make_unique<oj::PhysicsWorld>(*scene->collision());
        for (oj::ScenePhysics& p : scene->sceneObjects().physics()) world->add(*p.body);
    }

    // A car of the race on its grid place (0x00485530): its physics from the car's dynamics (with its parts fitted,
    // 0x0048E380) and the meshes of its folder, its grid throttle (0x0049F500) and its competitor (0x0049FCE0), the
    // loading thread's rand(); the player's (kind 0) or the AI's (kind 1).
    // kept: the car of the record (its parts and the damage it keeps go on after the race), none for a car of --car.
    // 0x00485600 first: the car's flickering neon (+0x6C) cleared and, refreshed (the call's tenth argument), its
    // tyres' wear (+0x74) and its brakes' (+0x7C) 0 and the nitro 15 (0x0048F210), before the parts give the body
    // its nitro.
    void addCar(const oj::CarDynamics& dynamics, const oj::Car& source, oj::Car* kept, const oj::GameData& data,
                oj::scene::CarModel* model, const oj::Spl::GridPoint& place, oj::CarKind kind, int32_t crew,
                int32_t record, bool refresh);

    // The end of 0x00495DE0: every competitor knows the others (0x004A0030), the cars go to the grid (0x004975A0);
    // the race runs from here, through the fly-by. Without a fly-by the count of 6 s and the sweep before the start
    // follow at once.
    void beginRace();

    // 0x00401E80 on the network: no fly-by, the sweep before the start (mode 10) at once with the count as the race's
    // reset left it (4.005, a spectator's 0.005), for a race not a solo race nor a cruise. True when taken.
    bool netSweep(RacePlayer& player);

    // 0x00497310 → 0x004EA270: the players' cars let go before the race's cars are (every +0xF0 0).
    void netDetach();

    // 0x00456BA0, the network's overlay in goodtime11, opaque yellow: in the stages 10 and 11 "Waiting for players..."
    // (54) in the middle of the first player's view; while the names are on (the tactics keys' 2 s) or only watching,
    // each car of the AI or another machine (kinds 1, 2) not parked: its player's name (15 characters at most) and
    // " (PAUSED)" (53) for one paused, centred on its place 1.2 m above the car (the camera's projection), fading by
    // its depth to nothing at 100 m; the watched car's at the top of the view (0.16 of its height), opaque. With the
    // voice ([0x0075F044]) the car's player's user (0x004EA100: referenced, Stock problem: never let go, one more a
    // frame) and its icon for the user object's first local player (mute; for a user whose devices are opened on
    // while it talks, off): 25 x 25 pixels from 5 right of the name, its bottom at the name's top, in the name's
    // colour.
    void drawNetOverlay();

    // 0x004BF320, from the race front end's frame off the network (0x004574C1), in goodtime11: outside the attract
    // replay (mode 4), mode 3 and a replay's race. With the racers' names' time below 0 their "!" (0x004BF8B0, below);
    // else for each racer of the career's session (+0x30) whose car (its +0x2C) is not the player's (0x004BAE70: the
    // car of the first racer of crew 0, member 0): white for the player's crew, else 0xFFE6..00 with the green (1 -
    // (nerves - 1)) x 236 held within 0 .. 236 (236 above 1.9 nerves on the frames [0x0075EF64] is set: it blinks),
    // rounded; its place 1.35 m above the car's centre through the camera (alpha 0, then 1 - depth / 150 m in front
    // within 150 m); the car's crew's name (108 + crew) centred there, 15 pixels lower the racer's member's short name
    // (the car's crew's +0xDE + 25 x the member); with the player's money on the car's crew (the session's odds +0x0C)
    // "BET" (1929), a space and the money 30 pixels above that; for a crew leader (member 0) of the crew bet with (the
    // bets' +0x08, a bet placed) with an amount over 0 (rounded) the amount 30 pixels above that again.
    void drawRacerLabels();

    // 0x00457845, the race front end's watch of the first player's controller: not in the attract replay (mode 4)
    // and while it watches (+0x38: set as it is made, 0x004563E0; cleared by restart_race's, start_replay's and
    // restart_replay's enter, set by their leave, 0x00455F50). By the event's +0x18 (the device read at its poll;
    // not while the game's window is inactive): connected again with its note up, DRIVER1 registered (0x004080D0),
    // the note closed (0x005A7F70 with 1), no note up, the UI map registered (0x00407FB0; and with a replay's race
    // hud_replay asked and the UI map registered again). Lost with no note up: a replay playing at 1 x its speed
    // ([G+0x9C]+0x828: +0x30 1), the note opened at priority 1 with no text; opened: a note up, the window asked
    // dropped, outside every player's finish (0x00420570), a replay's race and a pause the race paused (0x004263E0)
    // with hud_pause asked, else the music's song paused ([G+0x48]+0x58); a replay's race with the replay not done:
    // stopped (+0x7C 0, +0x30 1, +0x820 2); unless the second player paused the race its UI map registered and the
    // first player the pausing player (the race track's +0x10). Connected again: both notes counted as closed (+0x14,
    // +0x18), the UI map registered unless the second player paused; the second player's note then waits a frame.
    // 0x00457BB4, with more than one player the second player's (its device +0x2C, its note +0x18): connected with its
    // note up the note closed and both counted as closed; lost with no note up the note "<device + 1>." (Stock problem:
    // only the number) and as above, the second player's UI map registered and it the pausing player unless the first
    // player paused. The first player's controller lost with its note up, a client of a network game before the race
    // (the stage under 9) gets the network's note "The connection to the host has failed." (1645, its answer +0x30)
    // and the frame ends there (0x00457A47 .. 0x00457AB6: true).
    bool controllerWatch();

    // 0x004571C6, the race front end's frame before its window manager's, in a network game: the host's wait (+0x40)
    // runs down by the game's frame time while +0x3C; the host's note answered ([0x0079E7D0]) asks for closedown (mode
    // 2) with the message closed, unless closedown is up; no friend's session being joined ([0x0075EEF4]), in the
    // session with the host's stage back under 9 (the lobby: the round ended, or the host took everyone back) with no
    // note nor network message up: the message closed, and unless autosave is up the UI map registered and closedown;
    // else the host silent (the transport's state 4) with no network message: the window asked dropped, the message
    // closed, hud_pause asked outside every player's finish, a replay's race and a pause, everything reset (0x004EA4E0
    // with 0), the UI map registered and the network's note "The connection to the host has failed." (1645); then the
    // host gone (3) with no network message: everything reset, the UI map registered and "Host has left the game"
    // (49). The overlay (0x00456BA0) is drawn with the HUD.
    void netBeforeWindows(float dt);
    // 0x00457A47: the client's note under a lost controller (controllerWatch).
    bool netClientNote();
    // 0x00457E28, after the window manager's frame. The network's notes answered (+0x28), once (+0x2C): with the
    // matchmaking "Quitting online race", everything reset (0x004EA4E0 with 0), the network game ended and every
    // online object let go; in a network game "Quitting link-up race", everything reset (with 1), the network game
    // ended, every online object let go online, else Winsock; else "Closing network during single player race", every
    // online object let go online (+0x28 0 then). The network's message closed; but in the last case the UI map
    // registered, menu the window after the race ([0x0075ED18]) and closedown asked (mode 2, not over closedown): the
    // frame ends. The client's note answered (+0x30) once its controller is back: with the matchmaking everything reset
    // (0), the network game ended, the matchmaking let go and online_main after the race; in a network game
    // everything reset (1), the network game ended and link_search after the race; else nothing more. The network's
    // message closed; but off the network the message closed, the UI map registered and closedown asked: the frame
    // ends. Then with Winsock started while the controllers are watched (+0x38): the network down (0x00597730; the
    // user object's +0x90 0) unless +0x34: from its second frame (+0x24) with no network message nor answer the window
    // asked dropped and the network's note "A network error has occurred." (1678, type 2, its answer +0x28), and when
    // it opened (its number not 0) hud_pause outside the first player's finish and a replay's race, the UI map
    // registered; the host starts its wait (9 s, +0x40) once; else the frames counted. The network up: past the host's
    // wait everything reset (0); off line or in a network game without the matchmaking +0x5C and +0x60 -1, +0x24,
    // +0x34 and the wait 0. Then online, with no message up (or its number 0) while the controllers are watched, the
    // user object's state 6 (signed on elsewhere) with no network message: the window asked dropped, hud_pause outside
    // every player's finish, a replay's race and a pause, a replay's race's replay not done stopped, the UI map
    // registered for the first player (the pausing player) unless it paused, and the network's note of jet 107 (its
    // answer +0x28). True when the frame ended.
    bool netAfterWindows();

    // 0x0041F703 for a network game only watched: the player of the camera's car (the first whose car it is, else the
    // first), when gone or with SELECT, gives way to the next player in the race and not gone (from the one after it,
    // round): the camera takes its car (0x00401DB0); none: the director on mode 13 for good (FLT_MAX s).
    void netWatchNext(bool select);

    // The horn of another machine's car as its state says (0x004E73E0: from 1300, played or stopped).
    static void netHorn(oj::RaceCar& car, bool on) {
        if (oj::RaceSounds* sounds = oj::raceSounds()) sounds->horn(car, on);
    }

    // The session's hooks on the race's cars (0x004EAF10's calls): a car's state made (0x004E7740, the race paused
    // [G+0x34]+0x20 and the horn sounding), applied (0x004E73E0), smoothed towards (0x004E6E30), carried on
    // (0x004E6B60), put back on its line (0x00487080), its race distance (+0x494 +0x98), the model's byte (+0x430
    // +0x170), gone (+0x4E8 1, +0x4EC 0), another machine's (kind 2); the race's type and pause.
    void netHooks(oj::net::NetSession& session);

    // 0x004B0A50: the players in the race by their cruise scores, the first three with a score of 0 or more on the
    // window's mplay1 .. 3: "<n><st|nd|rd> <name> <score with its thousands marked>".
    void netCruiseBoard();

    // 0x00498190 with the sound on: each car's bank cars\<engine>[b] ("b" with an exhaust part) added once, its
    // instances twice the cars of it (the bank's instance count not followed); then 0x00498410 for each car: its
    // engine's sounds (0x00492D20, on the car's node), the road's sets and its instance (tarmacdry, looping, not
    // played) and the horn carhorn<n> (looping, volume 1, rolloff 1, its scale 1, from 1300; not played).
    void makeCarSounds();

    // 0x00401DB0's sounds: a car the camera had not taken turns its sounds to it (+0x28 with 1) and every other race
    // car's away (with 0).
    void cameraTakes(RacePlayer& player, RaceSlot* car);

    // 0x00408670 / 0x00408750: the director started or stopped: the volume groups 0.25, 1, 0.95 ([0x00717918] ..);
    // started, the camera's car's sounds turned to it; stopped, to it when it is the first player's car (or there is
    // none), else away.
    void directorSounds(RacePlayer& player, bool started);

    // The sound of a car's effects' squeal of the surface (+0x90), as 0x00494520 takes it.
    static oj::audio::Instance* squealOf(RaceSlot& s);

    // The sounds of a car's effects' scrape sparks and flakes (+0xD4, +0xE0), as 0x00492820 takes them.
    static std::array<oj::audio::Instance*, 2> soundHolders(RaceSlot& s) {
        const CarParticles& p = s.particles;
        return {p.scrapeSparks ? p.scrapeSparks->sound : nullptr, p.flakes ? p.flakes->sound : nullptr};
    }

    // 0x004065B4 .. 0x0040668E: the camera's car inside a reverb* helper (0x00422D60 at its place x 100, +0x1A4) gives
    // 0.5, the camera inside a tunnel* (0x00422D20, +0x1A0 and +0x1A4) 0.75, else 0: that car's sounds' occlusion
    // (0x00487320: the engine's; the effects' explosion sparks, scrape sparks, flakes and squeal of the surface
    // (+0xD0, +0xD4, +0xE0, +0x90), each with its use on; the horn's with its use on).
    void cameraOcclusion(RacePlayer& player);

    // 0x0041A300: the crowd's sets: in a cruise off line (and off the network) each kind's (the table 0x007117A8, 18 rows of 8, only the
    // first of each named; one is a null name, its set left as it was: 0, 0 in a run's first cruise), else all -1;
    // always each mood's (0x007116E8, 6 rows of 8). Found in the 3D player with the sound (a name not found -1). The
    // moods' idle loops (0x007119E8) are found and never played.
    void crowdSets();

    // 0x0041A1E0, each frame at the end of the race's (after the camera's listener), paused or not: [0x0075ECD4] 0,
    // each sounder (0x0041A560), then the four cooldowns less the frame's time.
    void crowdFrame(float dt);

    // 0x0041A560: not while paused. The cruise's car inside the zone (at its place x 100): [0x0075ECD4] 1 and the
    // zone's state kept. With the sound, the car and it inside: the level up by 0.3 of the way to 1 and the delay
    // down; past it a new one (r / 32767 x 4 + 0.5), the set of the zone's kind (rand() & 7), or after its bonus or
    // with none a mood's (rand() & 7 until one is); a free cooldown (0x0041A7E0) and a free slot of the ring: played
    // once at the zone's place, in a cruise rolloff 0.01 and volume 1, else 0.1 and 0.5, not looping, no doppler, its
    // frequency drawn within 6000, from 1000; the ring on. Else the level falls by 0.01 of itself.
    void crowdSounder(CrowdSounder& p, float dt);

    // The airhorn (the race state's +0x68): the last one released (refcount), hk_airhorn played once (0x005B9B00) at
    // the route's +0x4C (the race state's +0x30, 0x004A02A2): its first grid point's x, y, z (the six slots of
    // 0x004A1410 lie there; every route of the game's files has four or six). Then its frequency, volume 2, rolloff
    // 1e-5, its scale 1, no doppler.
    void airhorn(uint32_t hz);

    // 0x00487490, each frame of a car with its road sound: by the body's speed (0 .. 37 m/s over 37), sand's set and
    // twice that on a wheel over sand or grass (dry or wet: 1, 2, 4, 5), else the current set and 1.2 times; rolloff
    // 0.1, from 450.
    void roadSound(RaceSlot& s);

    // 0x004255F0, on the main thread once the load is done: the sky dome (0x005315A0, the track scene's own), each
    // car's damage grid from its model's nodes (0x0048A500), the world's slot 10 (0x005485A0: its buffers unlocked,
    // then the world's reflection cube 0x00548450), each player's camera on its car (0x00425750: the career's player,
    // the arcade's car of the player, else car i for player i: the player's car here), then a draw for each point of
    // the fast line, below 0.3 an emitter there (0x00503850): Leaves on a track whose first entry of the routes'
    // table (0x00426570, its name) has the location 1, 2 or 8, else Litter; placed at the point (+0x0C) and cleared
    // ([1] 0x0050E2A0: its leaves drawn by the main thread's rand()), the manager's list keeping it.
    // 0x0048A500: the car's model given to its grid object: the body's nodes (flag 1: their vertices' alpha and dents
    // by their blue) and the decals' (0), the zones kept by the car, then the grid finished (the kept damage dented
    // again, three rand() a cell); then the scene objects of the model's nodes (0x0048A644, makeLightSteps).
    void giveModelToGrid(RaceSlot& s, oj::CrtRandom& random);

    void finishLoad();

    // 0x004BB1A0: the car of the session's first racer of crew 0, else of the first whose crew has the player's money
    // (the session's odds +0x0C), else, with a bet made (the bets' +0x00), of the first of the bets' crew, else of the
    // first racer.
    RaceSlot* racerCamera() const;

    // 0x004975A0: the race's state reset (0x0049FDF0), then each car on its grid place: in a race of type 6 its nitro
    // burns at 2.5 a second (+0x380); put back for the start (0x00486E90, the frame's damage events cleared too), its
    // tank full outside a replay's race, the gearbox by hand, a standing start, the world's grid made anew, placed;
    // with a nitro part the low nitro message counts as given when the car has none (+0x198); then let settle.
    void settleRace(oj::CrtRandom& random);
    // 0x004976A5: the grid point of car i of the race's cars: on the network the session's grid place the car was made
    // with (+0x4F0); in a career race (mode 2) the i-th; else (an arcade, custom, split screen or attract race) the
    // count less i less 1, so the first cars made (the players') start at the back; a sprint's (type 6) moved on by its
    // heat (-1 as 0) within the count (on the network one heat: none). Stock problem: a count past the route's grid
    // points reads past them; here within them.
    oj::Spl::GridPoint gridPlace(size_t i) const;

    // The fly-by's end (0x00405BE0): the count of 6 s and the player's camera on the sweep before the start (mode 10).
    void countAndSweep(RacePlayer& player);

    // The nodes follow the bodies (0x004F01E0 after each physics step), the models the nodes.
    void syncNodes() {
        for (std::unique_ptr<RaceSlot>& s : cars) s->car.node = s->body->modelMatrix();
    }
    void placeCars();

    // A frame of the race (0x00425D40): the physics with the controls of the last frame; the cars (0x00497C50: each
    // car's frame, its wheels and contacts, a respawn, its competitor's controls), the count and the competitors
    // (0x004A0220); then the players (0x004205A0: each player's frame 0x0041FE50, its camera, its controls, the
    // driver's control).
    void drive(float dt);

    // 0x004263E0's sounds (and the common resume of 0x00426B80, 0x00426C40, 0x00426CF0, 0x00426DF0, 0x0044ED90,
    // 0x00406AB0): the race's pause changing (+0x20) pauses or resumes the music's song (+0x58) and the speech (+0x0C,
    // its player's slots), and the 3D world's voices follow it (its manager's +0x20 / +0x24).
    void pauseSounds();

    // 0x0048FEA0's emitters, in its order, under the car's node (the vehicle's +0x0C): the tyre tracks, the flames at
    // the exhausts, the heat haze (level 1), the explosion's sparks and smoke, the chassis's smoke, the scrape's sparks
    // and flakes (each with its light taken), each wheel's skids of tarmac and sand (level 0) and spray, the squeals of
    // tarmac and sand (level 0).
    void makeCarParticles(RaceSlot& s);

    // 0x004916F0's emitters, on the frames it ran. A hit: the explosion's sparks (unless they still burst), smoke and
    // flakes at the contacts' mean arm with the explosion's level, the scrape's sparks and the chassis's smoke there
    // with the scrape's; none: the explosion's at 0 (unless the sparks still burst), the scrape's at its level. Each
    // takes the car's origin and velocity; the sparks of both and the flakes take the car's lowest touch.
    void impactParticles(RaceSlot& s);

    // 0x004912D0's emitters: each exhaust's flame, while it is on, at (0, 0, -30) under its node with the level x the
    // fade in, the car's origin and velocity; off, at 0. The flames and the heat haze (at (0, 70, 180)) take the car's
    // lowest touch.
    void flameParticles(RaceSlot& s);

    // 0x004907D0's emitters: a wheel's skid of the surface it took (the one it left put to 0); a wheel down in the race
    // gives its skid its level at the wheel's slot 25 below, the car's origin and velocity, and its spray its level at
    // the same place with the wheel node's move; else its skid is put to 0. The tyre tracks: a wheel of a changed
    // surface asks for its TRACKS, the ring advances (0x00506800), the current entry takes the car's origin, a wheel
    // not in the race puts its mark off (the tracks' level 0), then each wheel down marks its touch.
    void carParticles(RaceSlot& s);

    // 0x004907D0's squeal (+0x90, after the wheels): the squeal of another surface taken puts the one before to 0
    // (0x004908B3); no fast forward, the wheel of the largest skid level (the first of equals) gives 4 x its slide +
    // its smoke + its skid; its sound from 500 ([0x0071786C]) at |the body's velocity| x 200 + the drawn pitch +
    // 30000. Fast forward: 0.
    void squealSound(RaceSlot& s);

    // 0x0048A644: the scene objects of the model's nodes (0x00420FF0), each started at once (its slot 3): the chassis
    // (_CHASS, the model's +0x90), the wheels and the parts of the table 0x00717538 that are model nodes (DRIVER makes
    // none). The chassis and the parts are 0x004191C0's objects (their node's +0xC0 bit 0x10 cleared, the node's
    // parent kept): a part takes its zone of the grid (slot 6, 0x004191A0: the model's +0x5C + 4 i, none for the
    // spoiler and the exhausts), the chassis's object (+0xD0, its light) and the model (+0xF0, its byte of parts
    // knocked off); one that wobbles reads its hulls and makes its body (CarPartObject, makePartPhysics).
    void makeLightSteps(RaceSlot& s);

    // 0x0048A700's objects (0x00420F30 for the chassis, the wheels and the parts): each goes, the chassis's and the
    // parts' giving their nodes their own matrices back (0x004196D0).
    void dropLightSteps(RaceSlot& s) {
        for (oj::scene::LightStep& l : s.lights) l.release();
        s.lights.clear();
    }

    // The box of the chassis's mesh (the model node's slot 1, 0x00556AC0, gives its mesh's 0x005595B0: the file's
    // lowest corner +0x2C and highest +0x3C), none without a mesh.
    static bool chassisBox(const RaceSlot& s, oj::Vec4& low, oj::Vec4& high);

    // 0x00485420: gen_shadow under the chassis, with a neon (the car's +0xD not 0xFF) gen_neon too in the colour of the
    // car's +0x2C; the box the chassis's x 1.25 across and x 1.1 along. At night (the atmosphere's +0x170,
    // 0x00486245) headlight_b on the glow's place (v turned over) in 0xFF62607C: the box the chassis's x 7 across and
    // from 0.8 of its front out to three of its lengths before it (a first box read there, x 1.2 across, gives only
    // its front).
    void makeProjectors(RaceSlot& s, const oj::Car& car);

    // 0x004890B0: the neon's colour: the car's (+0x2C) with, under an atmosphere, alpha 0x80 and by day r, g and b
    // x (0.4 - 0.1 x the chassis's light over 0.7, at most 1), then x (1 - the tilt), the tilt (1 - the car's up y)
    // x 4 within 0 .. 1; black without a neon or on a flickering neon's dark draw. The shadow then the beam laid
    // again.
    void projectorsFrame(RaceSlot& s, oj::CrtRandom& random);

    // 0x00487670 (Game/Cars/CarWheels), each wheel's speed kept by the car (+0x1D8).
    void wheelsFrame(RaceSlot& s, float dt);

    // 0x00406300 -> 0x00422C40: the atmosphere zone nearest the camera's place in the world (the first of the
    // nearest); within its radius (0x00418BF0) the track's first atmosphere goes to the zone's by 1 up to 0.75 of the
    // radius, then down to 0 at the radius (the set's +0x58, +0x5C, +0x64).
    // Stock problem: out of every zone's radius nothing is set, so the last blend stays as the camera leaves a zone.
    void atmosZonesFrame(RacePlayer& player);

    // 0x00487220: each exhaust's node at its place moved in y by -0.2 x the body's acceleration in its own y (+0x2AC),
    // within -6 .. 6 (on the stand, +0x19C, past 2000 rpm a rand() of -0.5 .. 0.49 instead).
    static void exhaustsFrame(RaceSlot& s);

    // 0x00487C70: the brake lights shown while the brake is applied, the reversing lights in reverse (gear 0); each
    // left as it is once the damage put it out (+0x44C, +0x454).
    static void carLights(RaceSlot& s);

    void raceFrame(float dt, bool impacts);

    // The race's flags of the replay: [G+0x9C]+0x828 and [G+0x6C]+0x68, as the race and its rules read them.
    void syncReplayFlags();

    // What the replay reads of the race: the cars' manager's list, the race's mode and type, every competitor
    // finished, the cruise's time left.
    oj::ReplayRace replayRace() const;

    // 0x00426EA0 and its header (0x00426870): the record begins; the cars' records written to its stream
    // (0x0048DD40 each, the career's car or the race's copy).
    void beginReplay(const oj::ReplayRace& r);

    // start_replay's (or restart_replay's) end: 0x004973E0 (start_replay only: each car's damage and its kept
    // values as the record began, from its stream), 0x00426160 (the players' cameras reset and off their cars, each
    // car made ready again, the cars settled on the grid, the camera on the first car), the cruise's car forgotten
    // (start_replay), the play from the start (0x00426FE0, 0x00426C40, 0x00426B80) and the race front end's black 1.
    void startReplay(bool fromStart);

    // What hud_replay asked for: the race paused or on, the next car followed (0x00497530 with the camera's car,
    // 0x00408AC0: the camera on it, the director active with view 8 for 0 s, its frame), each player's next view
    // (0x004069F0; on the bumper's view the director's trackside view for 10000.94 s), its windows and question.
    void followReplayWindow(const oj::ReplayWindowFrame& f);

    // 0x0041E110 for a cruise (type 4): the cruise set up for the race's minutes (+0x60, cut) and running (+0x10).
    void cruiseReset();

    // 0x0041E200's first call (off the network): the cruise's frame (0x004AAAB0) with the first player's car, its
    // messages to the first player's.
    void cruiseFrame(const oj::DriverActions& actions, float dt);

    // 0x0048E340 / 0x0048E300: DATAVAL of the car's catalogue part of a kind (the car's +0x0E for kind 0, +0x10 for
    // kind 2): none (-1) 0; past the list's end the first part's (Stock problem: another negative index reads before
    // the list; here the first part's too).
    float partValue(const oj::Car& car, size_t kind) const;

    // 0x0041F861, the player's car being the player's: the controller's motors (+0x10C the large one, 0x0041DD40;
    // +0x110 the small one within 0 .. 1, 0x0041DD60) by the wheels' smoke and skid over 0.3 ([0x007252B8]) and
    // slide over 0.05 (+ 0.2); without the steering wheel a contact (the forward speed x 0.025) and the hit's damage
    // and a landing (1); the nitro's look (x 0.2 with the arcade's endless nitro); without the steering wheel a gear
    // change by hand above 2 m/s (1), the pulling steering above 10 m/s for 1.5 s of every 3 ((speed - 10) x 0.04
    // within 0 .. 0.7), rough ground (its share; a jolt 1). Then both cleared: nothing on the PC reads them (the
    // competitor's +0xF0 is compared with 0.25 for nothing).
    void playerRumble(RacePlayer& player, float dt, bool wheel);

    // The player's frame (0x0041FE50): the camera, its controls, the driver's control, the finish.
    void playerFrame(RacePlayer& player, const oj::DriverActions& actions, float step, float dt);

    // 0x0041FC49: with the statistics, not a replay's race, one player: a race of type 5, 3 or 9 its lap time
    // (0x004EC6D0: the route 0x00426490, the best lap, the car's model, its performance rating cut toward zero (0:
    // rated first, 0x0048E8A0), the top speed), sent unless online or in a sprint with more than one racer; a showoff
    // its score (0x00420390, cut) of the track's location (0x00426570) above 0, sent. The user is the one of the
    // player's local GP player (0x0055BA30, vt+0x38: referenced and never let go).
    // 0x004EC6D0 for a player's car (the finish's and the pause's): the route (0x00426490), its competitor's best
    // lap, the car's model, its performance rating cut toward zero (0: rated first, 0x0048E8A0), the top speed; the
    // user of the player's local GP player (0x0055BA30, vt+0x38: referenced, never let go). False without the user
    // object (the stock's call through none).
    bool lapTimeStatistics(const RacePlayer& player);

    void finishStatistics(const RacePlayer& player);

    // The race front end after the race's frame: the window asked for comes in, then the window's frame. A message
    // window takes the events and keeps the windows under it waiting. The frame's event reaches a window from the
    // frame after it came in (the stock game reads the window events after the race's frame, the driver's actions
    // before it).
    void frontEndFrame(float dt);

    // The end of a player's frame: 0x0041E200 (0x0041F3F3): after a respawn the player's car passes through the others
    // while its ghost time lasts; the cars as its camera shows them.
    void playerFrameEnd(RacePlayer& player);

    // 0x0041FE50: every car shown (0x00489370 with 1), then under the views on the car the car the player's camera
    // follows not drawn (0x00489370 with 0: its shadow off with it and its exhausts' flames at 0, 0x004916A0; Stock
    // problem: the beam's +0x10 is set off too but never read, so the beam is drawn), before the player's view is drawn
    // (0x00511CA0).
    void hideFollowed(const RacePlayer& player);

    // 0x0044ED90, the race going on: the pause ends; the director's period is cut to 2 s from its time (0 past 2 s)
    // with its cut due; the window of the views asked for (0x004584C0 with 1); the player's controller (BACK of a
    // player) or each player's (CONTINUE) made again (0x004080D0).
    void goOn(int player = -1);

    // What a window's frame asks of the race front end.
    void followWindow(const oj::RaceWindowFrame& f);

    // 0x004584C0 for one player off the network, without a replay or the pause: asked 0 before the finish gives
    // blank; else by the race's type, the player's +0x1C taken (cleared) first: a challenge (0 .. 3, 9)
    // hud_race_complete, else after the finish hud_race_results for 3 and hud_challenge_results for the others, else
    // hud_solo; a cruise (4) hud_race_complete, else after the finish hud_cruise_results, else hud_cruise; a race (5,
    // 7, 8) hud_race_complete, else after the finish hud_race_results, else hud_race in place of any window waiting; a
    // sprint (6) hud_sprint_results once it finished, else hud_sprint in place of any window waiting.
    void chooseWindow(bool asked);

    // What the HUD reads of the race.
    oj::HudRace hudRace();

    // What the race camera reads of the player's car.
    // The car the first player's camera follows (+0x08).
    // 0x004BE04B, 0x004BE13D (the drivers' levels' frame 0x004BDF00): in the career with the race's cars not chosen
    // (the profile's +0x614 0: a watched race), not during the fly-by or the sweep (modes 9, 10), NEXTCAR follows the
    // car of the place before the followed car's (the last after the first) and SELECT the car of the place after it
    // (the first after the last); a car without a place (-1) gives the race's next car (0x00497530 with 1). The camera
    // takes it (0x00401DB0: a new car resets the view, +0xA4 and +0xB0; the cars' engine sounds turned to it:
    // cameraTakes). Either key sets the racers' names' time (+0x64) to 2 s.
    void watchedCar();

    RaceSlot* followedSlot(const RacePlayer& player) const {
        return player.viewed != nullptr ? player.viewed : player.slot;
    }

    oj::scene::CameraTarget cameraTarget(const RacePlayer& player) const;

    // What the race camera reads of the race.
    oj::scene::CameraRace cameraRace(RacePlayer& player);

    // The fly-by of the route (the race's camera step 0x00405BE0); the grid camera after it. The player's camera takes
    // the height's share of the width (0x00424965 → 0x0041DE30 → 0x00401760): on a wide screen 0.5625 (0.28125 with
    // two players), else 0.75 (0.375).
    void startIntro(const std::string& routeName, bool wide);

    // 0x00401A39 .. 0x00401BF1, as the player's camera is made (0x0041DE30 in the race track's making, on the loading
    // thread, before the race's cars): the camera keeps the atmosphere's Wet and LightBlur; in the wet rain1 (its
    // value 0) and rainsplash1 (1) follow it with rainblob1, in the dry ground_rush_street (0) and ground_rush_scrub
    // (1); each takes the Night of the atmosphere. The rain's sound (+0x0C) below.
    // The settings' effects volume (+0x58 of the profile) of the driver, a new driver's without one.
    float effectsVolume() const;

    // 0x005B9430: audio/<name>/<name>.cfg added to the 3D world's player unless a bank of the name is loaded.
    void addSoundBank(const char* name) { addSoundBank(name, std::filesystem::path(name)); }
    void addSoundBank(const char* name, const std::filesystem::path& folder);

    // 0x00461390's start with the sound on: the world of audio/3d with 350 voices, its volume (and its player's) the
    // settings' effects volume, fading down by 20 and up by 50, the manager not paused, the volume groups 0.25, 1,
    // 0.95; off line in a cruise the bank cruise, in the wet the bank wet (the bank dry is never loaded).
    void startSound(const oj::scene::TrackSetup& setup);

    // 0x004205A0's end: the 3D world's listener data (+0x28): the players' count, for each player its camera's place
    // (and the offset of its view) and its world matrix.
    void soundListener();

    void makeCameraWeather(RacePlayer& player);
    // The camera's rain, splash and screen smears let go. The stock's camera holds them counted and they free only
    // their own as they go (0x0050EC60); these reach their particle system's device and the 3D world's voices, so
    // they go before the scene and the world, not with the players made again for the next race.
    void dropCameraWeather(RacePlayer& player);

    // 0x004066A3: the atmosphere's LightBlur: under the followed car's nitro its amount x 0.7, else the camera's kept
    // value.
    void lightBlurFrame(RacePlayer& player);

    // 0x004066EE .. 0x0040686C: in the wet, in a tunnel (0x00422D20 at the camera's place, +0x1A0) no rain, splashes
    // nor blobs; else the rain and the splashes by how much it rains (the atmosphere's +0x19C), the blobs by 0.2 of
    // it plus, behind a car, (1 - the gap) x its effects' smoke x the road's wetness x 0.8. The rain's sound by the
    // camera's height over +0x168 below; the car's sounds in a tunnel (+0x49C ..) by cameraOcclusion.
    void weatherFrame(RacePlayer& player);

    void weatherEmitters(RacePlayer& player);

    // A player's view (the end of its frame, 0x0041FE50): its camera's fly-by (played here at the draw's time), the
    // cars as its camera shows them, then 0x00511CA0: the world's slot 14 for the camera (the cars' reflection cube)
    // before the view begins, 0x00516880 .. 0x00516D20: the view in its part of the screen with its post effects; a new
    // shot of the fly-by comes in from black over it.
    void drawView(oj::render::IRenderDevice& device, RacePlayer& player, float seconds, float dt);

    void gridCamera(RacePlayer& player);

    void draw(oj::render::IRenderDevice& device, float seconds, float dt);

    // restart_race at its time (0x00455A90): the career's +0x60 +0x3C0 cleared, a sprint's heat counted (the race's
    // config +0x08); 0x004261F0: each car made ready again (0x004894C0), the cars settled on the grid (0x004975A0, the
    // game's rand()), then the player (0x0041E110): the director stopped (0x00408750), the camera's mode by the race's
    // type (0x00401E80: 3 and 4 the player's view, a sprint's later heats the sweep before the start with the race's
    // count 8, else the fly-by again), the car the player's again, the player's flags cleared, its controller made
    // again; the effects' +0x98 cleared and the race going on (its sounds given back as restart_race held them).
    // 0x00492710 for a car, each wheel: the squeal's emitter and the wheel's skid put at 0 (their sounds with their
    // level, [3]) and emptied; the wheel's track put off, the tracks' level 0 and every track reset (0x0050C440).
    static void quietSkids(RaceSlot& s);

    // 0x00426300, a replay's restart (+0x824): the cars to the grid (0x004975A0), the record's flag (+0x24) cleared,
    // each player's camera by the race's type (0x00401E80, its [G+4]+0x18 0, a field nothing else is seen to read) on
    // its car (0x00401DB0) with the director on the TV view for 9999 s (0x00408670 with 11), with the sound every
    // emitter of the 3D world anew (its manager's +0x2C), the speech's +0x24 cleared, each car's skids quiet
    // (0x00492710) and the next 50 frames on their own time (+0x78 0).
    void replayRestart();

    void restartRace();

    // 0x004894C0: the car made ready again: with the car's record (the career's or the arcade's), a neon that flickered
    // makes the neon part 0 (+0x0D); the record repaired (0x0048EB20 in an arcade or custom race off line: every
    // cell's damage, else 0x0048EA80: the race's): the nitro 15 (0x0048F210), +0x64, +0x6C, +0x70, +0x74 and +0x7C
    // cleared; the model's grid's vertex lists dropped and its meshes made again (0x0048A700, 0x00489440), then given
    // to the grid again (0x0048A500); a lost turbo back (+0x180 set, +0x68 cleared, the body's Turbo 1.3 - Stock
    // problem: whatever the car's own); the lights back (+0x44C, +0x450, +0x454 cleared) with the headlights' flares on
    // when the lights are on.
    void resetCar(RaceSlot& s);

    // 0x004634B0's first call: the race's objects go (the scene, the cars, the physics world, the front end's
    // windows).
    void endRace();
};

}  // namespace app
