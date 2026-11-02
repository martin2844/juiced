// Review actual imported cars through CarView/GLES. No saves or physics are changed.
#include "Core/Cars/CarCollection.h"
#include "Core/Data/GameData.h"
#include "Game/Cars/CarView.h"
#include "Linux/Render/GlesDevice.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>

int main(int argc, char **argv) {
    if (argc < 4) {
        std::cerr << "Usage: juiced_car_gallery GAME_DATA ASSEMBLED_OVERLAY EMPTY_OUTPUT [--vinyls|--kits|--wings] [--close] [MODEL ...]\n";
        return 2;
    }
    try {
        bool vinylSweep = false;
        bool kitSweep = false;
        bool wingSweep = false;
        bool closeReview = false;
        std::vector<std::string> names;
        for (int i = 4; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--vinyls")
                vinylSweep = true;
            else if (arg == "--kits")
                kitSweep = true;
            else if (arg == "--wings")
                wingSweep = true;
            else if (arg == "--close")
                closeReview = true;
            else if (arg.starts_with('-'))
                throw std::runtime_error("Unknown option: " + arg);
            else
                names.push_back(arg);
        }
        if (int(vinylSweep) + int(kitSweep) + int(wingSweep) > 1)
            throw std::runtime_error("Select one of vinyl, kit or wing review");
        const auto game = std::filesystem::absolute(argv[1]);
        const auto overlay = std::filesystem::absolute(argv[2]);
        const auto output = std::filesystem::absolute(argv[3]);
        if (!std::filesystem::is_directory(game) ||
            !std::filesystem::is_regular_file(overlay / "scripts/cardefs.txt"))
            throw std::runtime_error("Game folder and assembled overlay catalogue must exist");
        if (std::filesystem::exists(output) && !std::filesystem::is_empty(output))
            throw std::runtime_error("Output must be empty; retain previous captures separately");
        std::filesystem::create_directories(output);
        oj::GameFiles::setOverrideRoot(overlay);
        std::filesystem::current_path(game);
        if (!SDL_Init(SDL_INIT_VIDEO))
            throw std::runtime_error(SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        auto *window = SDL_CreateWindow("Native car visual review", 1280, 960, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        if (!window)
            throw std::runtime_error(SDL_GetError());
        {
            oj::CrtRandom random;
            random.seed(42);
            oj::GameData data(game);
            std::string error;
            if (!data.load(random, &error))
                throw std::runtime_error(error);
            oj::render::DeviceDesc desc;
            desc.window = reinterpret_cast<HWND>(window);
            oj::render::GlesDevice gpu(desc);
            SDL_GL_SetSwapInterval(0); // Offline captures, not a frame-rate benchmark.
            oj::scene::Engine engine(gpu, data.tables, 3, false);
            oj::ui::Screen screen(gpu, *oj::Language::byName("english"));
            screen.setView3dFactory([&](const oj::ui::Element &element) -> std::unique_ptr<oj::ui::View3d> {
                return std::make_unique<oj::scene::View3d>(engine, element, random);
            });
            if (!screen.load("instant_selcar"))
                throw std::runtime_error("Selector UI missing");
            for (auto name : {"Locked", "Locked_Text", "CARFADE"})
                screen.setHidden(name, true);
            for (auto name : {"MODDING", "CARLOGO", "CARORIGIN", "PROT_"})
                screen.hideContaining(name);
            auto *scene = dynamic_cast<oj::scene::View3d *>(screen.view("FRAME_CAR3D"));
            if (!scene)
                throw std::runtime_error("Car viewport missing");
            screen.finishTransitions();
            if (closeReview) {
                // The normal selector buttons occlude lower bumper geometry at
                // this distance. Keep its real 3D viewport, hide only the 2D UI.
                screen.hideContaining("");
                screen.setHidden("FRAME_CAR3D", false);
            }
            std::ofstream manifest(output / "captures.csv");
            manifest << "model,style,bonnet,decal,front_bumper,rear_bumper,skirt,yaw_degrees,file,load_or_refit_ms\n";
            std::vector<const oj::CarDef *> cars;
            if (names.empty()) {
                cars.push_back(data.defs.byModel("FOCUS_STOCK"));
                for (int id = 53; id <= 63; ++id)
                    cars.push_back(data.defs.byId(id));
            } else {
                for (std::string model : names) {
                    if (!model.ends_with("_STOCK"))
                        model += "_STOCK";
                    cars.push_back(data.defs.byModel(model));
                }
            }
            size_t captures = 0;
            for (const auto *def : cars) {
                if (!def)
                    throw std::runtime_error("Expected car definition absent");
                auto car = oj::makeCar(def);
                car.base = 0xff1950a5;
                car.decalColour = 0xffe0e0e0;
                oj::scene::CarView::Setup setup;
                setup.element = "FRAME_CAR3D";
                setup.zoom = 1;
                setup.mirror = setup.shadow = setup.keepFolder = true;
                oj::scene::CarView view(engine, screen, setup, data.catalogue, data.tables);
                struct State {
                    std::string label;
                    uint8_t bonnet, decal;
                    bool maximumKit;
                    uint8_t kitLevel = 0;
                    uint8_t spoiler = 0; uint32_t paint = 0xff1950a5;
                };
                std::vector<State> states;
                if (vinylSweep) {
                    states.push_back({"stock", 0, 0x7f, false});
                    const auto count = def->parts[oj::kDecal].parts.size();
                    if (count == 0 || count >= 0x7f)
                        throw std::runtime_error("Unexpected decal catalogue: " + def->name);
                    for (size_t i = 0; i < count; ++i)
                        states.push_back({"vinyl" + std::to_string(i), 0, static_cast<uint8_t>(i), false});
                } else if (wingSweep) {
                    const auto count = def->parts[oj::kSpoiler].parts.size();
                    if (count < 4)
                        throw std::runtime_error("Three custom wing choices absent: " + def->name);
                    states = {{"stock", 0, 0x7f, false, 0, 0},
                              {"wing1", 0, 0x7f, false, 0, 1},
                              {"wing2", 0, 0x7f, false, 0, 2},
                              {"wing3", 0, 0x7f, false, 0, 3}};
                } else if (kitSweep) {
                    states = {{"stock",0,0x7f,false,0,0}, {"no-bumpers",0,0x7f,false,0,0}, {"kit1",0,0x7f,false,1,0}, {"kit2",0,0x7f,false,2,0}, {"kit3",0,0x7f,false,3,0}, {"stock-return",0,0x7f,false,0,0}};
                } else {
                    states = {{"stock", 0, 0x7f, false}, {"carbon", 1, 0x7f, false},
                              {"vinyl8", 0, 8, false}, {"kit-max-carbon-vinyl8", 1, 8, true}};
                }
                const std::vector<int> angles = vinylSweep ? std::vector<int>{2, 6}
                                               : closeReview ? std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}
                                                             : std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7};
                for (size_t style = 0; style < states.size(); ++style) {
                    const auto &state = states[style];
                    car.body[oj::kBonnet] = state.bonnet;
                    car.body[oj::kDecal] = state.decal;
                    if (wingSweep || kitSweep) car.body[oj::kSpoiler] = state.spoiler;
                    car.base = state.paint;
                    for (int part : {oj::kFrontBumper, oj::kRearBumper, oj::kSkirt}) {
                        const auto count = def->parts[part].parts.size();
                        if (!count)
                            throw std::runtime_error("Body part catalogue absent");
                        if (state.kitLevel >= count)
                            throw std::runtime_error("Requested kit absent: " + def->name);
                        car.body[part] = state.maximumKit ? std::min<size_t>(3, count - 1) : state.kitLevel;
                    }
                    if (state.label == "no-bumpers") car.body[oj::kFrontBumper] = car.body[oj::kRearBumper] = 0x7f;
                    const auto start = std::chrono::steady_clock::now();
                    if (style == 0)
                        view.show(car, nullptr, 0, 0, 0, false, random);
                    else
                        view.refit(car, data.defs);
                    do {
                        gpu.letLoadingRun();
                        view.frame(1.f / 60, random);
                        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(30))
                            throw std::runtime_error("Car load/refit timed out: " + def->name);
                        if (view.busy())
                            SDL_Delay(1);
                    } while (view.busy());
                    if (!view.hasModel())
                        throw std::runtime_error("Car load completed without a model");
                    const auto elapsed =
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                    for (int f = 0; f < 70; ++f)
                        view.frame(1.f / 60, random);
                    scene->setMode(oj::scene::MoveMode::CameraAuto, nullptr);
                    scene->autoSpeed = 0;
                    scene->setZoom(1);
                    scene->setZoomTarget(1);
                    for (int angle : angles) {
                        const float yaw = angle * std::numbers::pi_v<float> / 4;
                        // Explicit camera placement avoids interpolation changing
                        // the intended view between successive angles or cars.
                        const float radius = closeReview ? 620.f : 800.f;
                        scene->setCameraPlace({radius * std::sin(yaw), closeReview ? 185.f : 245.f,
                                               -radius * std::cos(yaw), 0});
                        scene->lookAt({0, 70, 0, 0}, true);
                        for (int f = 0; f < 4; ++f) {
                            gpu.beginFrame();
                            gpu.clear(3, 0, 1, 0);
                            screen.update(0);
                            screen.draw(0, 0);
                            gpu.endFrame();
                        }
                        const auto file = def->name + "-" + state.label + "-yaw" + std::to_string(angle * 45) + ".bmp";
                        if (!gpu.capture((output / file).wstring()))
                            throw std::runtime_error("Capture failed");
                        manifest << def->name << ',' << state.label << ',' << int(car.body[oj::kBonnet]) << ','
                                 << int(car.body[oj::kDecal]) << ',' << int(car.body[oj::kFrontBumper]) << ','
                                 << int(car.body[oj::kRearBumper]) << ',' << int(car.body[oj::kSkirt]) << ','
                                 << angle * 45 << ',' << file << ',' << elapsed << '\n';
                        ++captures;
                    }
                    std::cout << def->name << ' ' << state.label << " load/refit " << elapsed << " ms\n" << std::flush;
                }
            }
            std::cout << "CAPTURES " << captures << '\n';
        }
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        SDL_Quit();
        return 1;
    }
}
