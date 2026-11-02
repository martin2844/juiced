// Visual and career integration check, using a synthetic profile only. Never opens user saves.
#if defined(OJ_LINUX)
#include "Linux/Render/GlesDevice.h"
#else
#include "Android/Render/GlesDevice.h"
#endif
#include "Core/Input/Input.h"
#include "Framework/Fakes/FakeCareerHost.h"
#include "Game/Frontend/Career/CareerMenu.h"
#include "Game/Frontend/Career/Gamble.h"
#include "Game/Media/Movies.h"
#include "Game/UI/MessageWindow.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>

namespace oj::test {
void check(bool value, const std::string &what) {
    if (!value)
        throw std::runtime_error(what);
    std::cout << "PASS " << what << '\n';
}
} // namespace oj::test

int main(int argc, char **argv) {
    using namespace oj;
    using test::check;
    if (argc != 3)
        return 2;
    try {
        const auto root = std::filesystem::absolute(argv[1]);
        const auto captures = std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(captures);
        std::filesystem::current_path(root);
        CrtRandom random;
        GameData data(root);
        std::string error;
        check(data.load(random, &error), "Retail game data loads: " + error);
        auto &career = *data.career;
        career.profile.introduction = 0;
        career.profile.racing = 0;
        career.profile.money = 50000;
        career.garage = {makeCar(data.defs.byId(1)), makeCar(data.defs.byId(2))};
        career.garage[0].grid = GridDamage{};
        career.garage[0].base = 0xFF487CA0;
        career.garage[1].grid = GridDamage{};
        career.garage[1].base = 0xFFB54332;
        data.saving = 1;
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        auto *window = SDL_CreateWindow("Gamble validation", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        render::DeviceDesc desc;
        desc.window = reinterpret_cast<HWND>(window);
        render::GlesDevice gpu(desc);
        movie::Game movieSetup;
        movieSetup.folder = root;
        movieSetup.audioFlags = 1;
        movie::System movies(gpu, movieSetup);
        scene::Engine sceneEngine(gpu, data.tables, 3, true);
        struct PreviewHost : test::FakeCareerHost {
            scene::Engine *scene = nullptr;
            scene::Engine *engine() override {
                return scene;
            }
        } host;
        host.scene = &sceneEngine;
        host.data = &data;
        host.dt = .5f;
        host.texts.load("scripts/english.txt");
        frontend::Gamble gamble(123456);
        auto gallery = frontend::makePokerCardGallery(gpu, gamble);
        gpu.beginFrame();
        gallery->draw(0, 0, 1280, 720, 0, 0);
        gpu.endFrame();
        check(gpu.capture((captures / "all-52-cards.bmp").wstring()), "Capture all 52 gameplay card faces");
        ui::Screen screen(gpu, host.lang);
        screen.setView3dFactory([&](const ui::Element &element) -> std::unique_ptr<ui::View3d> {
            if (element.name == "POKER_TABLE")
                return frontend::makePokerView(gpu, gamble);
            return std::make_unique<scene::View3d>(sceneEngine, element, host.viewLoading());
        });
        check(screen.load("CMSGamble"), "Built-in Gamble screen loads");
        host.base = &screen;
        const auto capture = [&](const char *name) {
            screen.finishTransitions();
            for (int i = 0; i < 70; ++i) {
                movies.newFrame();
                gpu.letLoadingRun();
                gamble.framePreview(host, screen);
                gpu.beginFrame();
                screen.update(1.f / 60);
                screen.draw(i / 60.f, 1.f / 60);
                gpu.endFrame();
            }
            check(gpu.capture((captures / name).wstring()), "Capture " + std::string(name));
        };
        gamble.enter(host, screen);
        check(screen.hidden("RESULT_BACK"), "Result banner stays hidden while setting up");
        capture("setup.bmp");
        check(movies.current && !movies.current->noMovie(), "Original talking-head clip decodes in the table");
        const auto choose = [&](const char *command) {
            for (size_t i = 0; i < screen.menuSize("Menu"); ++i)
                if (screen.menuItemCommand("Menu", i) == command) {
                    check(screen.menuItemEnabled("Menu", i), std::string("Action enabled: ") + command);
                    screen.selectMenuItem("Menu", i);
                    gamble.update(host, screen, input::kSelect);
                    return;
                }
            throw std::runtime_error(std::string("Missing command ") + command);
        };
        const auto waitPreview = [&] {
            const auto start = SDL_GetTicks();
            for (int i = 0; i < 1000 && !gamble.previewReady(); ++i) {
                gpu.letLoadingRun();
                gamble.framePreview(host, screen);
                SDL_Delay(16); // Approximate one renderer service per 60 Hz frame.
            }
            std::cout << "PREVIEW_MS " << SDL_GetTicks() - start << '\n';
        };
        choose("title");
        waitPreview();
        check(gamble.previewReady(), "Selected garage car loads through the native 3D preview");
        check(!screen.hidden("TITLE_CAR_VIEW"), "Car preview visible before wagering");
        capture("selected-title-car.bmp");
        const auto firstCarName = screen.textOf("TITLE_CAR_NAME");
        choose("title");
        waitPreview();
        check(gamble.previewReady() && screen.textOf("TITLE_CAR_NAME") != firstCarName,
              "Changing the garage selection loads the other car and updates its caption");
        capture("second-title-car.bmp");
        choose("title");
        check(screen.hidden("TITLE_CAR_VIEW"), "No-title selection hides the preview");
        choose("title");
        waitPreview();
        check(gamble.previewReady() && screen.textOf("TITLE_CAR_NAME") == firstCarName,
              "Restoring a title restores the matching model and name");
        choose("deal");
        check(!host.messages.empty() && host.messages.back().startNo, "Cash and title confirmation defaults to No");
        check(!gamble.game(), "No hand starts without confirmation");
        {
            ui::Screen message(gpu, host.lang);
            check(message.load("msgbox"), "Native confirmation loads");
            ui::openMessage(message, ui::MessageType::YesNo, host.messages.back().text);
            ui::enterMessage(message, true);
            ui::messageFont(message, true);
            message.finishTransitions();
            for (int i = 0; i < 70; ++i) {
                gpu.beginFrame();
                screen.draw(i * .016f, .016f);
                message.update(.016f);
                message.draw(i * .016f, .016f);
                gpu.endFrame();
            }
            check(gpu.capture((captures / "confirm.bmp").wstring()), "Capture exact stake confirmation");
        }
        *host.messages.back().answer = 1;
        host.messageOpen = false;
        gamble.update(host, screen, 0);
        check(gamble.game() != nullptr, "Confirmed mixed cash/title hand starts");
        check(gamble.game()->stakes().size() == 5, "Five real car stakes available");
        capture("preflop.bmp");
        bool flop = false, titleWager = false;
        for (int frame = 0; frame < 2000 && !gamble.game()->hand.finished(); ++frame) {
            if (gamble.game()->hand.actor() == 0) {
                if (!titleWager) {
                    choose("wager");
                    choose("wager_title");
                    capture("mixed-wager.bmp");
                    choose("submit");
                    check(host.messages.back().startNo, "Explicit title wager defaults to No");
                    check(!gamble.game()->pledged(0), "Selected title is safe until wager confirmation");
                    const int previousPot = gamble.game()->hand.pot();
                    *host.messages.back().answer = 0;
                    host.messageOpen = false;
                    gamble.update(host, screen, 0);
                    check(!gamble.game()->pledged(0) && gamble.game()->hand.pot() == previousPot,
                          "Declining title wager preserves the car and pot");
                    choose("submit");
                    *host.messages.back().answer = 1;
                    host.messageOpen = false;
                    gamble.update(host, screen, 0);
                    check(gamble.game()->pledged(0), "Confirmed cash-plus-title wager reaches the pot");
                    titleWager = true;
                } else
                    choose("call");
            } else
                gamble.update(host, screen, 0);
            if (!flop && gamble.game()->hand.board().size() >= 3) {
                capture("flop.bmp");
                flop = true;
            }
        }
        check(titleWager, "Integration exercises an actual valued title wager");
        check(gamble.game()->settled(), "Mixed hand completes and settles into synthetic career");
        check(!host.requests.empty() && host.requests.back().first == "autosave",
              "Completed hand requests career autosave");
        check(screen.hidden("TITLE_CAR_VIEW"), "Car preview hides when settlement finishes");
        capture("result.bmp");
        check(!screen.hidden("RESULT_BACK") && !screen.hidden("RESULT_VALUE"),
              "Finished hand shows large result and value banner");
        Career restored;
        check(restored.read(career.write(), &error), "Result reads as legacy career save: " + error);
        gamble.leave(host);
        ui::Screen menu(gpu, host.lang);
        check(menu.load("CMSMainMenu"), "Retail Career menu loads");
        frontend::CareerMenu careerMenu;
        const auto stockItems = menu.menuSize("Menu");
        const auto stockBackgrounds = menu.menuSize("BackMenu");
        for (int visit = 0; visit < 20; ++visit) {
            // Frontend::baseEnter restores the retained list on every real return.
            menu.restoreMenu("Menu");
            menu.restoreMenu("BackMenu");
            careerMenu.enter(host, menu);
            check(menu.menuSize("Menu") == stockItems + 1 &&
                  menu.menuSize("BackMenu") == stockBackgrounds + 1,
                  "Career restore/re-entry keeps exactly one extra menu/background item");
        }
        size_t count = 0;
        for (size_t i = 0; i < menu.menuSize("Menu"); ++i)
            if (menu.menuItemCommand("Menu", i) == "Gamble")
                ++count;
        check(count == 1, "Career entry remains unique after re-entry");
        for (size_t i = 0; i < menu.menuSize("Menu"); ++i)
            if (menu.menuItemCommand("Menu", i) == "Gamble")
                menu.selectMenuItem("Menu", i);
        careerMenu.update(host, menu, input::kSelect);
        check(host.requests.back().first == "CMSGamble", "Career opens Gamble");
        menu.finishTransitions();
        for (int i = 0; i < 100; ++i) {
            movies.newFrame();
            gpu.beginFrame();
            menu.update(.016f);
            menu.draw(i * .016f, .016f);
            gpu.endFrame();
        }
        check(gpu.capture((captures / "career-entry.bmp").wstring()), "Capture native Career Gamble entry");
        std::cout << "PASS synthetic poker integration; no user saves accessed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
