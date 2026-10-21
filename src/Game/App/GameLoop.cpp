// The game's main function and its loop: the platform, the front end, the race and the frames between them
// (docs/spec/01-startup.md).

#include "Game/App/GameLoop.h"
#include "Game/Race/RaceGrids.h"
#include "Game/Race/ReplaySaves.h"

#include "Audio/SoundSystem.h"
#include "Audio/Voices.h"
#include "Core/Base/GameFiles.h"
#include "Core/Base/Random.h"
#include "Core/Base/Text.h"
#include "Core/Base/X87.h"
#include "Core/Data/Controls.h"
#include "Core/Data/EffectDefs.h"
#include "Core/Data/GameTexts.h"
#include "Core/Data/Language.h"
#include "Core/Input/Input.h"
#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/OnlineRecords.h"
#include "Core/Network/OnlineSessions.h"
#include "Core/Network/VoiceChat.h"
#include "Core/Race/RaceSounds.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Core/Saves/Save.h"
#include "Core/Saves/SaveEntries.h"
#include "Game/App/DebugPages.h"
#include "Game/Cars/CarSound.h"
#include "Game/Media/Movies.h"
#include "Game/Media/Music.h"
#include "Game/Platform/InputDevices.h"
#include "Game/Race/RaceCamera.h"
#include "Game/Scene/TrackScene.h"
#include "Render/RenderDevice.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace app {

namespace {

constexpr char kTitle[] = "Juiced";

// The application's +0x30 (0x00513C60): the back buffer after the present as <My Documents>\THQ\Juiced\Screenshots
// \PCScreenshot<n>.jpg (the folders made, 0x005A12E0). n ([0x0079E870], from 0 as the game starts) passes every file
// of which 4 bytes can be read (Stock problem: an empty file of the name is written over); it counts one more after
// the shot.
void screenshot(oj::render::IRenderDevice& device) {
    static int number = 0;
    wchar_t documents[MAX_PATH] = {};
    SHGetSpecialFolderPathW(nullptr, documents, CSIDL_PERSONAL, TRUE);
    const std::filesystem::path folder = std::filesystem::path(documents) / L"THQ" / L"Juiced" / L"Screenshots";
    std::error_code ignored;
    std::filesystem::create_directories(folder, ignored);
    auto name = [&] { return folder / std::format(L"PCScreenshot{}.jpg", number); };
    for (;;) {
        HANDLE f = CreateFileW(name().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) break;
        uint32_t word = 0;
        DWORD read = 0;
        ReadFile(f, &word, 4, &read, nullptr);
        CloseHandle(f);
        if (read == 0) break;
        ++number;
    }
    const std::filesystem::path file = name();
    if (device.capture(file.wstring())) oj::log::info("screenshot {}", oj::toUtf8(file.wstring()));
    ++number;
}

// carmodels.dat opens and gives at least one of the 4 bytes read.
bool carModelsReadable() {
    HANDLE file = CreateFileA("carmodels.dat", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    char head[4] = {};
    DWORD read = 0;
    ReadFile(file, head, sizeof(head), &read, nullptr);
    CloseHandle(file);
    return read != 0;
}

// 0x00512920 at the start of the renderer's frame (0x00514A40): with the gamma of the settings (+0x68) and the
// brightness changed since it was last taken ([0x00722EB0]: -1 at first and after the device's reset, 0x005142E9), the
// gamma g = (brightness - 0.5) x 0.15 + 1 within 0.01 .. 2 and each of the 256 steps i pow(i x 256, g) within 0 ..
// 65535, rounded to the nearest, for red, green and blue (SetGammaRamp, flags 1).
void brightnessGamma(oj::render::IRenderDevice& device) {
    static float last = -1.0f;
    static uint32_t resets = 0;
    if (device.resets() != resets) {
        resets = device.resets();
        last = -1.0f;
    }
    const oj::GraphicsConfig& c = oj::graphicsConfig();
    if (c.gamma == 0 || c.brightness == last) return;
    last = c.brightness;
    const double t = (static_cast<double>(c.brightness) - 0.5) * static_cast<double>(0.15f) + 1.0;
    float g = static_cast<float>(t);
    if (t < static_cast<double>(0.01f)) {
        g = 0.01f;
    } else if (t > 2.0) {
        g = 2.0f;
    }
    std::array<uint16_t, 256> ramp{};
    for (int i = 0; i < 256; ++i) {
        double v = std::pow(static_cast<double>(i) * 256.0, static_cast<double>(g));
        if (v < 0.0) v = 0.0;
        if (v > 65535.0) v = 65535.0;
        ramp[static_cast<size_t>(i)] = static_cast<uint16_t>(std::nearbyint(v));
    }
    device.setGammaRamp(1, ramp.data(), ramp.data(), ramp.data());
}

// The game object's start: platform, then the renderer on its window.
int runGame(const oj::LaunchOptions& options, const oj::Config& config) {
    oj::Settings settings = oj::Settings::defaults(options);
    oj::Platform platform;
    if (!platform.start(kTitle, options, settings)) {
        return 0;
    }

    const oj::render::Backend backend = oj::render::Backend::D3D9;
    oj::render::DeviceDesc desc;
    desc.window = platform.window();
    desc.windowed = settings.windowed != 0;
    desc.depthBits = settings.depth;
    desc.antialiasing = settings.antialiasing;
    desc.presentIntervalOne = options.presentIntervalOne;
    desc.pureDevice = options.pureDevice;
    desc.multithreaded = options.multithreadedD3D;
    desc.softwareVertexProcessing = options.softwareVertexProcessing || options.debugEffects;
    std::string error;
    auto device = oj::render::createDevice(backend, desc, &error);
    if (!device) {
        oj::log::error("renderer {}: {}", oj::render::backendName(backend), error);
        return 0;
    }
    oj::log::info("renderer {} on {}: {}", oj::render::backendName(device->backend()), device->adapterName(),
                  device->description());
    // 0x005132AC, the renderer's start: the processors for the game ([0x0079E5C0]) to the debugger's output.
    oj::log::debugPrint("numCores %d\n", g_gameProcessors);

    oj::Frontend::skipIntroMovies = config.skipIntroMovies;
    // The game object's start (0x0040BAC0) makes the interface's manager with the sound (0x005A5F90 → 0x005B8500):
    // DirectSound 8 by the configuration's audioflags.
    oj::audio::system().start(platform.window(), settings.audioFlags);
    // GameSpy Voice's window ([0x0075F79C]+0x0C).
    oj::net::setVoiceWindow(platform.window());
    // The library's instances follow the scene's nodes by their world matrices.
    oj::audio::setNodeMatrix([](const void* node) -> const float* {
        return const_cast<oj::render::Node*>(static_cast<const oj::render::Node*>(node))->world().data();
    });
    // 0x0040D200 (once, [0x0079E78C]): the game's music, a player of audio\music ([G+0x48], 0x00408B60).
    oj::jukebox().open(oj::GameFiles(std::filesystem::current_path()));
    // 0x0040C68D .. 0x0040C6D4: the debug pages' manager ([G+0x78]), the profiler and the memory page, on the game's 2D
    // view with its font (+0x74, 0) and the platform (+4). Nothing shows them (Game/App/DebugPages.h).
    oj::debug::makeDebugObjects(*device, &platform);

    // As the English build 1.0.1: the settings' language id, else the user's (0x0059E300); [Game] Language names one.
    const oj::Language language = oj::chooseLanguage(settings.language, config.language);
    oj::log::info("language {}", language.name);
    // The file system's localized names (0x004F9DB3) by the language object's letter.
    oj::GameFiles::setLanguageLetter(language.controlsLetter);
    oj::TextTable gameTexts;
    if (!gameTexts.load(std::filesystem::path("scripts") / (std::string(language.name) + ".txt"))) {
        oj::log::warning("no scripts\\{}.txt", language.name);
    }
    // 0x00591050: the input manager with its DirectInput: a device for each game controller found (test runs take
    // none, so that a controller attached does not change them), the keyboard read from the window's messages.
    oj::InputDevices inputDevices;
    oj::input::Input input;
    if (!options.test && inputDevices.start(platform.window())) input.setControllers(inputDevices.controllers());
    // Then its FF manager (0x00591DF0; a test run opens no channel).
    inputDevices.startWheels(options.test ? nullptr : platform.window());
    input.setWheels(&inputDevices.wheels());
    std::vector<oj::ActionMap> controlMaps;
    {
        const std::filesystem::path controls =
            std::filesystem::path("scripts") / (std::string("controls_dx9_") + language.controlsLetter + ".txt");
        const auto bytes = oj::GameFiles(std::filesystem::current_path()).read(controls);
        if (!bytes) {
            oj::log::warning("no {}", controls.string());
        } else {
            controlMaps =
                oj::parseControls(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
            input.load(controlMaps);
        }
    }
    // The game object seeds rand() again from the platform timer's low 32 bits (0x0040BC25).
    {
        LARGE_INTEGER counter{};
        QueryPerformanceCounter(&counter);
        const uint32_t seed = options.seed ? *options.seed : static_cast<uint32_t>(counter.QuadPart);
        oj::gameRandom().seed(seed);
        oj::log::info("rand() seed {}", seed);
    }
    // The game object's data (0x0040BAC0): car models, parts, a first career, the start driver.
    oj::GameData data(std::filesystem::current_path());
    {
        std::string why;
        if (!data.load(oj::gameRandom(), &why)) {
            oj::log::warning("game data not loaded: {}", why);
        }
        oj::log::info("game data loaded, {} rand() values drawn", oj::gameRandom().draws());
    }
    // 0x0040C56E: the second save system object's start driver named by the game's text line 168.
    data.profileName2 = gameTexts.at(oj::kTextPlayer2);
    // The controllers' making reads the driver's settings ([G+0x64]: the steering wheel, its speed sensitivity, the
    // driver's map).
    input.setSettings(&data.settings);
    std::filesystem::path saveFolder = config.saveFolder.empty() ? oj::save::root() : config.saveFolder;
    if (!options.saves.empty()) {
        saveFolder = oj::toWide(options.saves);
    }
    oj::log::info("save folder {}", oj::toUtf8(saveFolder.wstring()));
    // The save device's file object (0x00502E60, made with its manager at the game's start): THQ\Juiced\SAVE made.
    oj::save::makeFolders(saveFolder / L"SAVE");
    // The statistics' routes (0x00711A3C) and their end with the online objects' (0x004EC590 when there,
    // [0x0075F030]).
    oj::net::stats::setRouteTable(&data.tables);
    oj::net::onlineHooks().statsClosed = [] {
        if (oj::net::stats::gameStatistics() != nullptr) oj::net::stats::closeStatistics();
    };
    // The renderer's start (0x005A25F0) and the adapter's numbers (0x00512A40) in the graphics settings.
    oj::graphicsStart(oj::graphicsConfig(),
                      {settings.width, settings.height, settings.depth, settings.windowed, settings.antialiasing});
    {
        const auto adapter = device->adapterInfo();
        oj::graphicsAdapter(oj::graphicsConfig(), adapter.vendorId, adapter.deviceId, true, device->fullScreenGamma());
        oj::graphicsPostEffects(oj::graphicsConfig());
    }
    std::unique_ptr<oj::scene::Engine> scene;
    if (data.loaded()) {
        scene = std::make_unique<oj::scene::Engine>(*device, data.tables, settings.shaders, settings.widescreen != 0,
                                                    options.localEffects, options.debugEffects);
    }
    // The movies (Game/Media/Movies.h): the game's binkw32.dll, the language's sound track, the audio flags, rand().
    oj::movie::System movies(*device, {std::filesystem::current_path(), language.index, settings.audioFlags,
                                       &oj::gameRandom()});
    movies.setAfterDraw([&scene] {
        if (scene) scene->renderer().endMaterial();
    });
    auto frontend = std::make_unique<oj::Frontend>(*device, language, gameTexts, &data, saveFolder, scene.get());
    // 0x0046B5A0 outside a replay's race: the window startup over three frames, then the windows the front end keeps
    // (made on the loading thread in the stock code, the startup window drawn meanwhile; here the frame waits).
    if (data.race.mode != oj::RaceMode::Attract) {
        frontend->showStartup();
        for (int i = 0; i < 3 && platform.pumpMessages(); ++i) {
            if (!device->beginFrame({0.0f, 0.0f, 0.0f, 1.0f})) continue;
            frontend->draw();
            device->endFrame();
        }
    }
    frontend->keepGameWindows();
    // -i (0x004184D2): the instant race's driver and race, and loading for the first window.
    std::string firstWindow = options.screen;
    if (options.instantRace) {
        input.setPlayerDevice(0, 0);
        frontend->instantRace(options.m, options.wet);
        if (firstWindow == "disclaimer") firstWindow = "loading";
    }
    // 0x00418300 with the sound on: the front end's music bank ("music1", 0x0046BAC0: one rand() value), its music
    // (0x0046BDD0) when the front end's +0x28 is set: not for a first window of disclaimer or bootupcheck (0x0046B5A0;
    // the intro's music then starts as skippable is left, 0x00466AB0); the front end's first window comes on its first
    // frame.
    // The career's code stops the speech through Core's hook.
    oj::speechStop() = [] {
        if (oj::Speech* speech = oj::gameSounds().speech) speech->stop();
    };
    // The driver's settings' volumes ([G+0x64]+0x54 ..), a new driver's without one.
    oj::gameSounds().setting = [&data](uint32_t offset) {
        const oj::save::Settings& set = data.settings ? *data.settings : oj::save::Settings::defaults(0);
        return std::bit_cast<float>(set.u32(offset));
    };
    if (oj::audio::system().on()) {
        frontend->loadMusic();
        if (_stricmp(firstWindow.c_str(), "disclaimer") != 0 && _stricmp(firstWindow.c_str(), "bootupcheck") != 0) {
            frontend->startMusic();
        }
        // 0x0040D330 with 1: the interface's speech.
        oj::makeSpeech(1, language.index, oj::GameFiles(std::filesystem::current_path()));
    }
    if (options.players > 0) {
        data.race.players = options.players;
        if (options.players > 1) data.race.mode = oj::RaceMode::SplitScreen;
    }
    frontend->start(firstWindow);
    TrackView trackView;
    // Stock problem: the replays' save device ([game + 0xA0], made after the replay at 0x0040C597) is given no
    // replays' object on the PC (0x004FA8B0 is called by nothing), so savereplay and loadreplay, if reached, would
    // call through address 0; here they find none (no files written, a read counts as corrupt). The replay's slots
    // 13 and 14 (ReplaySaves) are kept for it as the stock has them.
    trackView.multiTurn = settings.multiTurn != 0;
    if (scene) {
        trackView.particleDensity =
            oj::particleDensity(options.scale, scene->shaderClasses(), settings.threading, g_gameProcessors);
        oj::log::info("particle density {}", trackView.particleDensity);
    }
    // Sound as the stock game has it: on unless switched off (audioflags 8).
    trackView.sound = oj::audio::system().on();
    trackView.cameraOffset = options.trackCamera;
    trackView.alwaysDepthOfField = options.rf;
    trackView.autopilot = options.autopilot;
    // The online switches of the command line for the main menu's enter.
    oj::Frontend::sharedState().onlineCommandLine.host = options.host;
    oj::Frontend::sharedState().onlineCommandLine.connect = options.connect;
    oj::Frontend::sharedState().onlineCommandLine.address = options.connectAddress;
    oj::Frontend::sharedState().onlineCommandLine.name = options.playerName;
    oj::Frontend::sharedState().onlineCommandLine.email = options.email;
    oj::Frontend::sharedState().onlineCommandLine.password = options.password;
    if (!options.net.empty()) {
        // --net <lan>,<game>,<to>[,<ip>]: the network's ports, the search's broadcasts' port and address.
        oj::net::Transport::Options& net = oj::net::netOptions();
        std::vector<std::string> parts;
        for (size_t start = 0; start <= options.net.size();) {
            const size_t end = std::min(options.net.find(',', start), options.net.size());
            parts.push_back(options.net.substr(start, end - start));
            start = end + 1;
        }
        if (parts.size() > 0) net.lanPort = static_cast<uint16_t>(std::strtoul(parts[0].c_str(), nullptr, 10));
        if (parts.size() > 1) net.gamePort = static_cast<uint16_t>(std::strtoul(parts[1].c_str(), nullptr, 10));
        if (parts.size() > 2) net.broadcastPort = static_cast<uint16_t>(std::strtoul(parts[2].c_str(), nullptr, 10));
        if (parts.size() > 3) {
            unsigned a = 0, b = 0, c = 0, d = 0;
            if (sscanf_s(parts[3].c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
                net.broadcastIp = a | (b << 8) | (c << 16) | (d << 24);
                net.ownIp = net.broadcastIp;
            }
        }
        net.shareLanPort = true;
    }
    trackView.settings = &data.settings;
    trackView.automod = options.automod;
    trackView.automodValue = options.automodValue;
    trackView.frontEnd.hudDevice = device.get();
    trackView.frontEnd.hudLanguage = &language;
    trackView.frontEnd.hudTexts = &gameTexts;
    trackView.saveFolder = saveFolder;
    data.audioFlags = settings.audioFlags;
    trackView.gameData = &data;
    trackView.input = &input;
    trackView.wheels = &inputDevices.wheels();
    const bool showTrack = !options.track.empty() && scene && trackView.start(*scene, options.track);
    if (showTrack) input.useDriver();
    if (showTrack && !options.car.empty()) {
        for (const oj::CarDef& def : data.defs.cars) {
            if (_stricmp(def.name.c_str(), options.car.c_str()) != 0) continue;
            oj::Car car;
            car.def = &def;
            car.setModel(def.model);
            const oj::CarDynamics* dynamics = data.dynamics ? data.dynamics->of(def) : nullptr;
            int32_t number = 0;
            // --car is not a race of the game: no drivers' records, two laps.
            oj::RaceRules rules;
            rules.mode = oj::RaceMode::Custom;
            rules.type = oj::RaceType::Circuit;
            trackView.prepareRace(2, rules);
            for (const oj::Spl::GridPoint& place : trackView.scene->lines().grid) {
                oj::scene::CarModel* model = trackView.scene->addCar(
                    oj::scene::CarModel::load(*scene, car, data.catalogue, trackView.loading, false), place);
                trackView.places.push_back({place.x, place.y, place.z, place.heading});
                // The car of the first place is the player's, the others the AI's.
                if (dynamics != nullptr) {
                    trackView.addCar(*dynamics, car, nullptr, data, model, place,
                                     number == 0 ? oj::CarKind::Player : oj::CarKind::Ai, number, -1, true);
                }
                ++number;
            }
            trackView.beginRace();
            trackView.finishLoad();
        }
    }
    // The game object's phases (0x0040CCA0): the front end (+0x38), the race's load (+0x40) with its own front end
    // for the loading window, then the race (+0x34).
    enum class Phase { Frontend, Load, Race, Unload };
    Phase phase = Phase::Frontend;
    std::unique_ptr<oj::Frontend> loader;
    int loaderFrames = 0;
    bool loadFailed = false;

    // The stock frame limiter: after Present the game yields until 1/60.1 s have passed since the last frame
    // (no limit with -r).
    const double frameInterval = options.presentIntervalOne ? 0.0 : 1.0 / 60.1;
    LARGE_INTEGER frequency{};
    LARGE_INTEGER start{};
    LARGE_INTEGER last{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    last = start;
    LARGE_INTEGER previousUpdate = start;
    auto seconds = [&](const LARGE_INTEGER& from, const LARGE_INTEGER& to) {
        return static_cast<double>(to.QuadPart - from.QuadPart) / static_cast<double>(frequency.QuadPart);
    };

    uint64_t rendered = 0;
    bool frontendUi = false;  // the front end's +0x18: the UI map registered
    while (platform.pumpMessages()) {
        RECT client{};
        GetClientRect(platform.window(), &client);
        device->resize(static_cast<uint32_t>(client.right), static_cast<uint32_t>(client.bottom));
        if (!device->beginFrame({0.0f, 0.0f, 0.0f, 1.0f})) {
            Sleep(16);
            continue;
        }
        brightnessGamma(*device);
        // 0x00514B28: the profiler's frame timer (0) ended; 0x00514B5A: the debug pages' current page drawn while
        // they are shown (never); 0x00514C4B: timer 0 started again.
        if (oj::debug::Profiler* p = oj::debug::profiler()) p->stopFrame();
        if (const oj::debug::Pages* pages = oj::debug::pages()) pages->drawCurrent();
        if (oj::debug::Profiler* p = oj::debug::profiler()) p->start(0);
        if (scene) scene->frameParameters();
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        std::vector<oj::input::KeyEvent> keys = platform.takeKeyEvents();
        for (const oj::LaunchOptions::Press& p : options.presses) {
            if (p.down == rendered) {
                keys.push_back({p.code, true});
                oj::log::info("test: frame {} key 0x{:02X} down", rendered, p.code);
            } else if (p.up == rendered) {
                keys.push_back({p.code, false});
            }
        }
        const float dt = static_cast<float>(seconds(previousUpdate, now));
        // 0x0040CCD9: the movies' hold and drawn flags cleared; [0x0075F780] the window's activation.
        movies.newFrame();
        movies.setActive(platform.active());
        // 0x0040CCF0: the input manager's update (0x005918C0) by the frame's time; [0x0075F780] the window's
        // activation (a test run's hidden window counts as active).
        input.poll(keys, platform.active() || options.test, dt);
        oj::FrontendInput in;
        in.typed = input.typed();
        in.shiftHeld = input.held(0x0F);
        in.playerTaken = input.playerTaken();
        in.devices = input.deviceCount();
        in.ui.anyInput = input.anyInput();
        if (showTrack || phase == Phase::Race) {
            // 0x00457180: the race front end makes the first player's event each frame.
            in.ui = input.uiFrame(dt);
        } else if (phase == Phase::Frontend) {
            // 0x0046C00F: with the first player's device taken and no switch waiting (+0x100 0) the front end
            // registers the UI map once (again after the device was let go; no note of its controller up) and makes
            // the event; while it watches the controllers (+0x38) the event's +0x18 opens or closes the note of a
            // lost one (0x0046C085, the UI map registered again as it closes). Without it no event; a note of
            // priority 1 or more closes (0x0046C393). A note the first player's closes keeps the second player's
            // from opening this frame ([esp+0x10]).
            bool noteOpens = true;
            if (input.playerTaken() && frontend != nullptr && frontend->switchWait() == 0.0f) {
                if (!frontendUi) {
                    input.useUi();
                    frontendUi = true;
                    frontend->playerUi(0);
                }
                in.ui = input.uiFrame(dt);
                if (frontend->watchingControllers() &&
                    frontend->watchController(0, in.ui.connected)) {
                    input.useUi();
                    noteOpens = false;
                }
            } else {
                frontendUi = false;
                if (frontend != nullptr) frontend->controllerUnwatched();
            }
            // 0x0046C18B, whatever the first player's part did: with the second player's device (+0x2C) its UI map
            // registered once (the front end's +0x20; again only once that is cleared: split_selcar's BACK, a new
            // driver), its frame and its controller watched (0x0046C208, the front end's +0x38 aside).
            if (frontend != nullptr && input.playerDevice(1) != -1) {
                if (!frontend->secondPlayerUi()) {
                    input.useUi(1);
                    frontend->playerUi(1);
                }
                in.ui2 = input.uiFrame(dt, 1);
                in.second = true;
                frontend->watchSecondController(in.ui2.connected, input.playerDevice(1), noteOpens);
            }
        }
        previousUpdate = now;
        // The race's loading front end by its mode: the window and over it its animation.
        const auto loaderWindows = [&]() {
            const oj::RaceSetup& race = data.race;
            const char* window = race.mode == oj::RaceMode::Arcade   ? "arcade_loading"
                                 : race.mode == oj::RaceMode::Custom ? "custom_loader"
                                                                     : "loader";
            const char* animation = race.mode == oj::RaceMode::Arcade   ? "arcade_loader_anim"
                                    : race.mode == oj::RaceMode::Custom ? "custom_loader_anim"
                                                                        : "loader_anim";
            return std::pair<std::string, std::string>(window, animation);
        };
        // 0x0040CECA: after the 3D world (0x0040CE14) the game's music ([G+0x48], 0x004090D0), the next song unless
        // a song's preview runs ([0x0075EF28]).
        // Before it the speech's frame (0x0040CEB2, 0x0040B4D0) when there is a speech object.
        // Out of a race the 3D world of a window ([G+0x50]: the garage's cut scene, car_rolling) steps here first.
        const auto musicFrame = [&]() {
            if (phase != Phase::Race && !showTrack) {
                if (oj::audio::World* world = oj::gameSounds().world) world->update(dt);
            }
            if (oj::Speech* speech = oj::gameSounds().speech) speech->update(dt);
            oj::jukebox().update(data.playlist, dt, !oj::jukebox().previewing);
        };
        if (showTrack || phase == Phase::Race) {
            trackView.frontEnd.uiEvent = in.ui.event;
            trackView.frontEnd.uiFrame = in.ui;
            // 0x00457BB4: with more than one player the race front end makes the second player's frame too (the
            // controller of the input manager's +0x2C) for the windows' +0x8C.
            trackView.frontEnd.uiFrame2 = trackView.rules.players > 1 ? input.uiFrame(dt, 1) : oj::input::UiFrame{};
            trackView.frontEnd.uiEvent2 = trackView.frontEnd.uiFrame2.event;
            trackView.drive(dt);
            // 0x0040CD7A: the profiler's timer "Audio - 3d" (taken at the game's first race frame, [0x007A2F64]; -1
            // without a profiler then) runs over the 3D world's frame.
            static const int audioTimer =
                oj::debug::profiler() != nullptr ? oj::debug::profiler()->add("Audio - 3d") : -1;
            if (oj::debug::Profiler* p = oj::debug::profiler()) p->start(audioTimer);
            // 0x0040CE14: the 3D world with the frame's time.
            if (trackView.sound3d) trackView.sound3d->update(dt);
            if (oj::debug::Profiler* p = oj::debug::profiler()) p->stop();
            musicFrame();
            trackView.draw(*device, static_cast<float>(seconds(start, now)), dt);
            if (!showTrack && trackView.frontEnd.unloadAsked) {
                // 0x00462C30 with 1, then 0x004634B0: the race's objects go; the loading front end shows its window
                // while the job (0x00461A10) makes the front end with its first window. The game's music first stops
                // (0x00408FA0 with 1: 300 ms slept before and after).
                if (oj::audio::system().on()) oj::jukebox().stop(true);
                // The race's speech freed (unless the configuration has no speech).
                oj::makeSpeech(0, language.index, oj::GameFiles(std::filesystem::current_path()));
                oj::gameSounds().raceFrontEnd = false;
                trackView.endRace();
                const auto [window, animation] = loaderWindows();
                loader = std::make_unique<oj::Frontend>(*device, language, gameTexts, &data, saveFolder, scene.get());
                loader->keepLoaderWindows();
                loader->startLoader(window, animation);
                phase = Phase::Unload;
                loaderFrames = 0;
                oj::log::info("race: unloaded");
            }
        } else if (phase == Phase::Unload) {
            musicFrame();
            loader->update(dt, in);
            loader->draw();
            if (++loaderFrames >= 3) {
                // The job done: the front end with its first window; its music (0x0046BA90: with the sound on,
                // 0x0046BAC0 draws one rand() value).
                const std::string first = windowAfterRace(data);
                frontend = std::make_unique<oj::Frontend>(*device, language, gameTexts, &data, saveFolder, scene.get());
                // 0x0046B5A0: the race no longer a replay's ([G+0x6C]+0x68); outside a replay's race the stock code
                // draws its startup window from the job's thread while this thread draws the loading window (not
                // followed: the loading window stays); the windows kept.
                trackView.replay.replayRace = 0;
                frontend->keepGameWindows();
                if (oj::audio::system().on()) {
                    frontend->loadMusic();
                    frontend->startMusic();
                    oj::makeSpeech(1, language.index, oj::GameFiles(std::filesystem::current_path()));
                }
                frontend->start(first);
                frontendUi = false;
                loader.reset();
                phase = Phase::Frontend;
                oj::log::info("frontend: back from the race to {}", first);
            }
        } else if (phase == Phase::Load) {
            // 0x00462D90: three frames of the loading front end, then the load (on the main thread here: the
            // loading window does not move while the track loads). Its first (+0x10 0), outside a replay (mode 4 or
            // [G+0x6C]+0x68), with the statistics, not watching: an online race of more than one racer (not the
            // career's pink slip) has each local player's user's race begun (0x004EC8C0, referenced and never let
            // go) and sent; the route's world record asked again (0x004EE0C0 with 1), its tries 0.
            if (loaderFrames == 0 && data.race.mode != oj::RaceMode::Attract && trackView.replay.replayRace == 0 &&
                oj::net::stats::gameStatistics() != nullptr) {
                oj::net::NetGame* netGame = oj::net::netGame();
                if (netGame == nullptr || !netGame->session.spectator()) {
                    oj::net::OnlineUser* users = oj::net::onlineUser();
                    if (oj::net::matchmaking() != nullptr && !(data.career && data.career->onlinePinkSlip != 0) &&
                        netGame != nullptr && netGame->session.racing() > 1 && users != nullptr) {
                        uint32_t begun = 0;
                        for (size_t i = 0; i < users->localPlayers.size(); ++i) {
                            ++begun;
                            oj::net::stats::reportRaceBegun(users->playerUser(users->localPlayers[i]));
                        }
                        if (begun != 0) oj::net::stats::reporter()->send();
                    }
                    oj::net::stats::worldBest(data.tables.routeIndex(data.race.track, data.race.route), true);
                    oj::net::stats::worldBestTries() = 0;
                }
            }
            musicFrame();
            loader->update(dt, in);
            loader->draw();
            ++loaderFrames;
        } else {
            musicFrame();
            frontend->update(dt, in);
            if (frontend->takeSecondPlayerRelease()) input.releasePlayer(1);
            if (frontend->takeSecondPlayerTake()) input.takePlayer(1);
            if (frontend->takeInputConfigure()) {
                input.useDriver();
                // 0x00590F40: the FF manager stopped and made anew (its response table not copied again until the
                // next DRIVER1, 0x004080D0), then the maps registered again.
                inputDevices.stopWheels();
                inputDevices.startWheels(options.test ? nullptr : platform.window());
                input.table().fill(0);
                input.recreateControllers();
            }
            if (frontend->takeInputUi()) input.useUi();
            if (frontend->takeInputRelease()) input.release();
            frontend->draw();
            if (frontend->quitAsked()) PostQuitMessage(0);
            if (frontend->raceLoadAsked() && scene) {
                // 0x0046AE50 → 0x00462D90: the front end goes; the load's front end shows the window by the race's
                // mode and over it its animation.
                const oj::RaceSetup& race = data.race;
                const auto [window, animation] = loaderWindows();
                frontend.reset();
                loader = std::make_unique<oj::Frontend>(*device, language, gameTexts, &data, saveFolder, scene.get());
                loader->keepLoaderWindows();
                loader->startLoader(window, animation);
                phase = Phase::Load;
                loaderFrames = 0;
                oj::log::info("race: {} {} (time of day {}, weather {})", race.track, race.route, race.timeOfDay,
                              race.weather);
            }
        }
        // 0x0040CF0A: the sound system stepped (0x005B8AB0: the streams follow the focus, every player's slots go to
        // their volumes and the ended are freed) after the front ends' frames, unless a job runs ([G+0x40]: the
        // race's unload making the front end).
        if (phase != Phase::Unload) oj::audio::system().update();
        // 0x0040CF30: the network game's frame (0x004EBAE0) at the game's frame time, after the race's, the front
        // end's, the movie's and the sound's.
        if (oj::net::NetGame* net = oj::net::netGame()) net->session.frame(dt);
        // 0x0040CF48: then the online user object's frame (GameSpy Presence: its login, its presence).
        if (oj::net::OnlineUser* user = oj::net::onlineUser()) user->update(dt);
        // 0x0040CF84: the matchmaking's frame (+0x30) after the network game's and GP's.
        if (oj::net::Matchmaking* mm = oj::net::matchmaking()) mm->update(dt);
        // 0x0040CF87: the statistics' frame (0x004EC5B0: made at the login, the records asked again every 5 s with a
        // front end, [G+0x38] or [G+0x3C]).
        oj::net::stats::frame(dt, phase == Phase::Frontend || phase == Phase::Race);
        // 0x0040CF8C: with a front end ([G+0x38]) the voice's icons taken once (0x004EEB70); the voice's frame (+0x74)
        // at the game's frame time.
        if (phase == Phase::Frontend) oj::net::loadVoiceIcons();
        if (oj::net::VoiceChat* voice = oj::net::gameVoice()) voice->update(dt);
        const bool lastFrame = options.frames != 0 && rendered + 1 >= options.frames;
        if (lastFrame && !options.capture.empty()) {
            // A relative file name is taken from the executable's folder (the current directory is the game's).
            const std::filesystem::path file(oj::toWide(options.capture));
            device->capture((file.is_absolute() ? file : executableFolder() / file).wstring());
        }
        device->endFrame();
        // 0x0040CFD9: Print Screen ([0x0075F77C], set by the window procedure) after the present.
        if (platform.takeScreenshotRequest()) screenshot(*device);
        do {
            Sleep(0);
            QueryPerformanceCounter(&now);
        } while (seconds(last, now) < frameInterval);
        last = now;
        ++rendered;
        if (phase == Phase::Load && loaderFrames >= 3 && !loadFailed && data.race.mode == oj::RaceMode::Attract &&
            !trackView.replay.playing && !loadReplayRace(trackView, data)) {
            oj::log::warning("race: the replay {} does not read", data.replayName);
            loadFailed = true;
        }
        if (phase == Phase::Load && loaderFrames >= 3 && !loadFailed) {
            const oj::RaceSetup& race = data.race;
            oj::scene::TrackSetup setup;
            setup.track = race.track;
            setup.route = race.route;
            setup.timeOfDay = static_cast<uint32_t>(race.timeOfDay);
            setup.weather = static_cast<uint32_t>(race.weather);
            // 0x004632F0: the loader's frames drawn, the race's speech made (0x0040D330 with 2) before the job.
            oj::makeSpeech(2, language.index, oj::GameFiles(std::filesystem::current_path()));
            if (trackView.start(*scene, setup)) {
                // The racers' cars on their grid places (the loading thread's rand() goes on after the track).
                // Off the network 0x00495DE0 takes a replay's race ([G+0x6C]+0x68: the attract's, mode 4, or a saved
                // replay's read by loadreplay, mode 0) before the race's mode.
                // 0x00495E48: its start to the debugger's output ("Done!" as it ends, TrackView::beginRace).
                oj::log::debugPrint("LOADING PROFILE : Loading cars... ");
                oj::net::NetGame* const netGame = oj::net::netGame();
                const bool replayed = trackView.replay.replayRace != 0;
                if (netGame != nullptr && data.career) netGrid(trackView, data, *scene, netGame->session);
                if (netGame == nullptr && !replayed && data.career && race.mode == oj::RaceMode::Career) {
                    const oj::RaceSession& session = data.career->session;
                    const oj::Spl& lines = trackView.scene->lines();
                    oj::RaceRules rules;
                    rules.mode = race.mode;
                    rules.type = race.type;
                    rules.lapTime = race.lapTime;
                    rules.speed = race.speed;
                    rules.heat = race.heat;
                    rules.heats = race.heats;
                    rules.players = race.players;
                    rules.minutes = race.difficulty;
                    rules.session = &data.career->session;
                    rules.career = data.career.get();
                    rules.settings = &data.settings;
                    trackView.prepareRace(race.laps, rules);
                    for (uint32_t i = 0; i < session.count && i < session.racers.size(); ++i) {
                        const oj::Racer& racer = session.racers[i];
                        if (racer.cars == nullptr || racer.car < 0 ||
                            static_cast<size_t>(racer.car) >= racer.cars->size()) {
                            continue;
                        }
                        const size_t place = static_cast<size_t>(racer.place) < lines.grid.size()
                                                 ? static_cast<size_t>(racer.place)
                                                 : i % std::max<size_t>(lines.grid.size(), 1);
                        if (lines.grid.empty()) break;
                        // 0x004969AA: another crew's racer races a copy of its car (0x00434D90) with its crew's
                        // scheme put on every opponents' car first (0x004A9080), refreshed; the player's crew its
                        // garage's car, as it is. Each gets its grid when it has none (0x005562A0).
                        const bool copied = racer.crew != 0;
                        oj::Car* raced = const_cast<oj::Car*>(&(*racer.cars)[static_cast<size_t>(racer.car)]);
                        if (copied) {
                            if (static_cast<size_t>(racer.crew) < data.career->crews.size()) {
                                oj::applyCrewScheme(*data.career, data.career->crews[static_cast<size_t>(racer.crew)],
                                                    data.defs, data.catalogue);
                            }
                            trackView.ownCars.push_back(std::make_unique<oj::Car>(oj::copyCar(*raced)));
                            raced = trackView.ownCars.back().get();
                        }
                        if (!raced->grid) raced->grid.emplace();
                        const oj::Car& racerCar = *raced;
                        auto car =
                            oj::scene::CarModel::load(*scene, racerCar, data.catalogue, trackView.loading, false);
                        oj::scene::CarModel* model = trackView.scene->addCar(std::move(car), lines.grid[place]);
                        const oj::Spl::GridPoint& at = lines.grid[place];
                        trackView.places.push_back({at.x, at.y, at.z, at.heading});
                        const oj::CarDynamics* dynamics =
                            data.dynamics && racerCar.def != nullptr ? data.dynamics->of(*racerCar.def) : nullptr;
                        if (dynamics != nullptr) {
                            trackView.addCar(*dynamics, racerCar, raced, data, model, lines.grid[place],
                                             racer.driver != 0 ? oj::CarKind::Player : oj::CarKind::Ai, racer.crew,
                                             static_cast<int32_t>(i), copied);
                        }
                    }
                }
                if (netGame == nullptr && !replayed && data.career &&
                    (race.mode == oj::RaceMode::Custom || race.mode == oj::RaceMode::SplitScreen)) {
                    customGrid(trackView, data, *scene);
                }
                if (netGame == nullptr && !replayed && data.career && race.mode == oj::RaceMode::Arcade) {
                    arcadeGrid(trackView, data, *scene);
                }
                if (netGame == nullptr && replayed) replayGrid(trackView, data, *scene);
                trackView.startIntro(race.route, scene->wideScreen());
                // 0x0041DE76: each player's controller made with DRIVER1.
                for (const std::unique_ptr<RacePlayer>& p : trackView.players) input.useDriver(p->controller);
                trackView.beginRace();
                trackView.finishLoad();
                loader.reset();
                // 0x00463454: the job done, the game's music plays its next song (0x00408EC0 with 1, 1: half a second
                // slept) at the settings' music volume (+0x18, +0x1C and the player's).
                {
                    oj::Jukebox& music = oj::jukebox();
                    music.play(data.playlist, true, true);
                    const float volume = std::bit_cast<float>(
                        (data.settings ? *data.settings : oj::save::Settings::defaults(0))
                            .u32(oj::save::Settings::kMusicVolume));
                    if (music.player() != nullptr && music.handle != 0) {
                        music.player()->setVolume(music.handle, volume);
                        music.volume = volume;
                        music.target = volume;
                    }
                }
                // 0x0046348E: the pool's statistics to the debugger's output.
                oj::debug::poolStats();
                phase = Phase::Race;
                // The stock game loads on its own thread with the frames going on; the load here is not a frame's time.
                QueryPerformanceCounter(&previousUpdate);
                last = previousUpdate;
            } else {
                loadFailed = true;  // the loading window stays
            }
        }
        if (options.frames != 0 && rendered >= options.frames) {
            break;
        }
    }
    oj::log::info("{} frame(s) rendered", rendered);
    // The front ends closed (their windows' exits) and the views the windows' records still hold freed before the
    // engine goes.
    loader.reset();
    frontend.reset();
    oj::Frontend::releaseRecords();
    return 0;
}

}  // namespace

std::filesystem::path executableFolder() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

// The game's main function: random seed, the carmodels.dat check, the game object.
int gameMain(const oj::LaunchOptions& options, const oj::Config& config) {
    oj::CrtRandom& random = oj::gameRandom();
    random.seed(static_cast<uint32_t>(random.next() + 1));
    random.next();
    // The start's messages in the game's language (the reference build's are its region's).
    oj::setStartupLanguage(oj::chooseLanguage(oj::readLanguageSetting(), config.language));

    // -setup (OpenJuice's): the game's configuration tool only, the one the first start opens.
    if (options.setup) {
        oj::runConfigurationTool();
        return 0;
    }
    if (!carModelsReadable()) {
        oj::log::error("carmodels.dat is not readable");
        if (!options.test) {
            MessageBoxW(nullptr, oj::startupText(1).c_str(), L"Juiced", MB_OK);
        }
        return 0;
    }
    return runGame(options, config);
}

}  // namespace app
