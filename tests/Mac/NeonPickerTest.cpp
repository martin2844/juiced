#include "Core/Input/Input.h"
#include "Framework/Fakes/FakeWindowHost.h"
#include "Game/Frontend/Garage/CarStats.h"
#include "Game/Frontend/Garage/Customise/Accessories.h"
#include "Game/Frontend/Garage/Customise/Paint.h"
#include "Game/Frontend/Garage/Customise/Tuning.h"
#include "Game/Frontend/Garage/GarageHost.h"
#include "Game/Scene/Scene3d.h"
#include "Mac/MetalDevice.h"
#include "Mods/Creative.h"
#include "Mods/Runtime.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <unistd.h>
using namespace oj;
struct Host : test::FakeWindowHost, frontend::GarageHost {
    frontend::Garage g;
    frontend::CarStats stats;
    render::IRenderDevice *gpu = nullptr;
    render::IRenderDevice &device() override {
        return gpu ? *gpu : nullDevice;
    }
    frontend::Garage &garage() override {
        return g;
    }
    frontend::CarStats &carStats() override {
        return stats;
    }
    frontend::CarStats &carStatsPurple() override {
        return stats;
    }
};
void check(bool yes, const char *message) {
    if (!yes)
        throw std::runtime_error(message);
    std::cout << "PASS " << message << '\n';
}
int main(int argc, char **argv) {
    if (argc < 3)
        return 2;
    auto saves = std::filesystem::temp_directory_path() / ("juiced-neon-picker-" + std::to_string(getpid()));
    try {
        auto game = std::filesystem::absolute(argv[1]), overlay = std::filesystem::absolute(argv[2]);
        std::filesystem::current_path(game);
        GameFiles::setOverrideRoot(overlay);
        mods::runtime::load(JUICED_TEST_MODS);
        mods::configure(saves);
        CrtRandom random;
        GameData data(game);
        std::string error;
        check(data.load(random, &error), "game assets load");
        Host h;
        h.data = &data;
        auto car = makeCar(data.defs.byId(53));
        car.neon = 0xff20ff80;
        car.body[9] = 0xff;
        data.career->garage.clear();
        data.career->garage.push_back(car);
        data.career->dealer.chosen = 0;
        data.career->dealer.mode = DealerMode::Garage;
        auto &live = data.career->garage[0];
        data.career->profile.bodyParts(53)[9] = 0;
        data.career->profile.money = 5000;
        h.g.styleCategory = 9;
        SDL_Window *window = nullptr;
        std::unique_ptr<render::MetalDevice> gpu;
        std::unique_ptr<scene::Engine> scene;
        if (argc > 3) {
            SDL_Init(SDL_INIT_VIDEO);
            window = SDL_CreateWindow("Neon picker test", 1280, 960, SDL_WINDOW_METAL | SDL_WINDOW_HIDDEN);
            render::DeviceDesc desc;
            desc.window = reinterpret_cast<HWND>(window);
            gpu = std::make_unique<render::MetalDevice>(desc);
            h.gpu = gpu.get();
            scene = std::make_unique<scene::Engine>(*gpu, data.tables, 3, false);
        }
        ui::Screen garage(h.device(), h.language());
        if (scene)
            garage.setView3dFactory([&](const ui::Element &e) -> std::unique_ptr<ui::View3d> {
                return std::make_unique<scene::View3d>(*scene, e, random);
            });
        garage.load("carsel");
        h.base = &garage;
        h.g.texts.priceText = "Price";
        h.g.texts.boxText = "PriceBox";
        if (scene) {
            h.g.view = std::make_unique<scene::GarageView>(*scene, garage, "FRAME_CAR3D", data.catalogue);
            h.g.view->enterSel(live, random);
            for (auto name : {"menu", "backmenu", "infobox", "aniline", "animline2", "menufade"})
                garage.setHidden(name, true);
        }
        ui::Screen palette(h.device(), h.language()), chart(h.device(), h.language());
        palette.load("neonmenu");
        chart.load("colourchoose");
        frontend::NeonMenu neon;
        frontend::ColourChoose picker;
        neon.enter(h, palette);
        palette.selectMenuItem("COLOURMENU", data.defs.neons.size());
        neon.update(h, palette, input::kSelect);
        check(h.requests.back().first == "colourchoose", "custom neon opens original paint chart");
        const auto original = live.neon;
        picker.enter(h, chart);
        chart.finishTransitions();
        check(chart.find("SELCOLOUR") >= 0, "retail RGB colour chart exists");
        for (int i = 0; i < 3; ++i) {
            h.device().beginFrame();
            chart.update(1.f / 60);
            chart.draw(i / 60.f, 1.f / 60);
            h.device().endFrame();
            picker.update(h, chart, 0);
        }
        const auto initial = live.neon;
        chart.chartFind("SELCOLOUR", 0xffff0000);
        for (int i = 0; i < 3; ++i) {
            h.device().beginFrame();
            chart.update(1.f / 60);
            chart.draw(i / 60.f, 1.f / 60);
            h.device().endFrame();
            picker.update(h, chart, 0);
        }
        picker.update(h, chart, input::kSelect2);
        check(live.neon != initial && mods::neon(live).effect == 1, "chart colour and effect preview before purchase");
        check(mods::neonColour(live, 0) != mods::neonColour(live, .5), "preview animates while chooser is open");
        check(data.career->profile.money == 5000, "preview costs nothing");
        if (gpu) {
            for (int i = 0; i < 120; ++i) {
                gpu->beginFrame();
                scene->frameParameters();
                garage.update(1.f / 60);
                chart.update(1.f / 60);
                picker.update(h, chart, 0);
                garage.draw(i / 60.f, 1.f / 60);
                chart.draw(i / 60.f, 1.f / 60);
                gpu->endFrame();
                SDL_Delay(16);
            }
            gpu->capture(std::filesystem::path(argv[3]).wstring());
            auto draft = mods::neon(live);
            draft.rgb = 0x00ff00;
            draft.effect = 0;
            live.neon = 0xff00ff00;
            mods::previewNeon(live, draft);
            for (int i = 0; i < 30; ++i) {
                gpu->beginFrame();
                scene->frameParameters();
                garage.update(1.f / 60);
                garage.draw(i / 60.f, 1.f / 60);
                chart.draw(i / 60.f, 1.f / 60);
                gpu->endFrame();
            }
            gpu->capture(std::filesystem::path(std::string(argv[3]) + "-green.bmp").wstring());
        }
        picker.update(h, chart, input::kBack);
        check(live.neon == original && mods::neon(live).effect == 0, "cancel restores original colour and effect");
        // Frontend::leavePopup calls this when colourchoose is popped/swapped.
        auto leavePicker = [&] {
            h.g.leavePriced(h, chart, "colourchoose");
            garage.finishTransitions();
        };
        auto priceVisible = [&] {
            garage.finishTransitions();
            return !garage.hidden("Price") && !garage.hidden("PriceBox");
        };
        leavePicker();
        check(!priceVisible(), "leaving priced popup hides its price");
        neon.update(h, palette, 0);
        check(priceVisible(), "cancelled picker restores palette price");
        neon.update(h, palette, input::kSelect);
        picker.enter(h, chart);
        picker.update(h, chart, input::kSelect);
        leavePicker();
        frontend::BuyMod declined;
        declined.enter(h, palette);
        *h.messages.back().answer = 0;
        declined.update(h);
        neon.update(h, palette, 0);
        check(priceVisible() && data.career->profile.money == 5000,
              "declined purchase restores price without charging");
        data.career->profile.money = 0;
        neon.update(h, palette, input::kSelect);
        picker.enter(h, chart);
        picker.update(h, chart, input::kSelect);
        leavePicker();
        frontend::BuyMod insufficient;
        insufficient.enter(h, palette);
        neon.update(h, palette, 0);
        check(priceVisible() && data.career->profile.money == 0, "insufficient cash restores palette price");
        data.career->profile.money = 5000;
        neon.update(h, palette, input::kSelect);
        picker.enter(h, chart);
        picker.update(h, chart, input::kSelect2);
        picker.update(h, chart, input::kSelect);
        check(h.requests.back().first == "buymod", "confirm uses normal purchase window");
        frontend::BuyMod buy;
        buy.enter(h, palette);
        check(!h.messages.empty() && h.messages.back().answer, "normal confirmation asks before charging");
        *h.messages.back().answer = 1;
        buy.update(h);
        neon.update(h, palette, 0);
        check(data.career->profile.money == 4000 && mods::neon(live).effect == 1,
              "purchase charges exactly once and stores effect");
        mods::profile(0);
        check(mods::neon(live).effect == 1, "purchased effect survives profile reload");
        h.g.paintMode = frontend::PaintMode::Base;
        picker.enter(h, chart);
        float x, y, w, height;
        chart.placeOf(chart.find("GRIDX"), x, y, w, height);
        check(x < .3f && w < .46f, "ordinary paint keeps its original label layout");
        h.g.view.reset();
        std::filesystem::remove_all(saves);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        std::filesystem::remove_all(saves);
        return 1;
    }
}
