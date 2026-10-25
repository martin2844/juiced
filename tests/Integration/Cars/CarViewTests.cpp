// Integration tests: the car view of the interface (Game/Cars/CarView.h) in the game's cmscarcollection2 window.

#include "Framework/Test.h"
#include "Framework/Fixtures.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Base/LoadingThread.h"
#include "Core/Base/Random.h"
#include "Core/Data/Language.h"
#include "Game/Cars/CarView.h"
#include "Game/Frontend/CustomRace/CustomRaceInternal.h"
#include "Game/Scene/Scene3d.h"
#include "Game/UI/UiScreen.h"
#include "Render/Materials.h"
#include "Render/Model.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

using oj::test::check;

// The vertex buffer of each indexed draw, in order.
class DrawOrderDevice : public oj::test::NullRenderDevice {
public:
    std::vector<oj::render::BufferId> drawn;

    void setStream(oj::render::BufferId buffer, uint32_t) override { m_stream = buffer; }
    void drawIndexed(uint32_t, uint32_t, uint32_t, uint32_t) override { drawn.push_back(m_stream); }

private:
    oj::render::BufferId m_stream = 0;
};

// The vertex buffers of the parts below a node, those of the materials drawn late (flags 0x24: the glass) apart.
void partsBelow(const oj::render::Node& node, std::set<oj::render::BufferId>& opaque,
                std::set<oj::render::BufferId>& late) {
    if (const auto* m = dynamic_cast<const oj::render::ModelNode*>(&node); m != nullptr && m->mesh != nullptr) {
        for (const oj::render::Mesh::Part& part : m->mesh->parts()) {
            if (part.material == nullptr) continue;
            ((part.material->flags() & 0x24) != 0 ? late : opaque).insert(part.vertices);
        }
    }
    for (const oj::render::Node* child : node.children()) partsBelow(*child, opaque, late);
}

}  // namespace

// The car asked for loads on the loading thread (0x0045A080 -> 0x00463D60) while the car shown fades out; a newer car
// takes the place of one whose load has not started (0x0045A0A7), and the view, faded out, stays empty until the load
// is done (0x00459EE0: +0x08 3): a list scrolled through faster than a car loads stays dark and loads only the car it
// stops on.
OJ_INTEGRATION_TEST(CarView, LoadsOnTheLoadingThread) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead || shared.defs.cars.size() < 3) {
        check(false, "cardefs.txt read");
        return;
    }
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, shared.tables, 3, false);
    oj::CrtRandom random;
    const oj::Language language = *oj::Language::byName("english");
    auto screen = std::make_shared<oj::ui::Screen>(device, language);
    screen->setView3dFactory([&](const oj::ui::Element& e) -> std::unique_ptr<oj::ui::View3d> {
        return std::make_unique<oj::scene::View3d>(engine, e, random);
    });
    if (!screen->load("cmscarcollection2")) {
        check(false, "cmscarcollection2.ui read");
        return;
    }
    oj::Car first;
    first.def = &shared.defs.cars[1];
    oj::frontend::customrace::clearParts(first);
    oj::Car second;
    second.def = &shared.defs.cars[2];
    oj::frontend::customrace::clearParts(second);
    oj::scene::CarView::Setup setup;
    setup.element = "CAR3D";
    oj::scene::CarView view(engine, *screen, setup, shared.catalogue, shared.tables);
    // The loading thread held busy, so that the cars asked for wait on its list.
    std::atomic<bool> release = false;
    const auto plug = oj::loadingThread().queue([&] {
        while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });
    oj::CrtRandom loading;
    view.show(first, nullptr, 0, 0.0f, 0.5f, true, loading);
    view.frame(1.0f, random);
    const bool firstWaits = view.busy() && !view.loaded() && !view.hasModel() && view.shown() == 0.0f;
    view.show(second, nullptr, 1, 0.0f, 0.5f, true, loading);
    view.frame(1.0f, random);
    const bool darkWhileLoading = view.busy() && !view.loaded() && !view.hasModel();
    release = true;
    view.finish(0.05f, random);
    check(firstWaits && darkWhileLoading, "faded out, the view empty while the cars asked for wait to load");
    check(!view.busy() && view.hasModel() && view.model()->name() == second.def->name,
          "the last car asked for loaded and put in");
    check(plug->done(), "the loading thread free again");
}

// The parts changed on the car shown (0x0045A8A0) wait for the view's next frame (+0x40, the view busy), which puts
// them in and the chassis at the car's lowering (0x0045A027): the automatic modification's 0.8 x MAXDROPHEIGHT down.
OJ_INTEGRATION_TEST(CarView, RefitLowersTheChassis) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead || shared.defs.cars.size() < 2) {
        check(false, "cardefs.txt read");
        return;
    }
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, shared.tables, 3, false);
    oj::CrtRandom random;
    const oj::Language language = *oj::Language::byName("english");
    auto screen = std::make_shared<oj::ui::Screen>(device, language);
    screen->setView3dFactory([&](const oj::ui::Element& e) -> std::unique_ptr<oj::ui::View3d> {
        return std::make_unique<oj::scene::View3d>(engine, e, random);
    });
    if (!screen->load("cmscarcollection2")) {
        check(false, "cmscarcollection2.ui read");
        return;
    }
    oj::Car car;
    car.def = &shared.defs.cars[1];
    oj::frontend::customrace::clearParts(car);
    oj::scene::CarView::Setup setup;
    setup.element = "CAR3D";
    oj::scene::CarView view(engine, *screen, setup, shared.catalogue, shared.tables);
    view.show(car, nullptr, 0, 0.0f, 0.5f, false, random);
    view.finish(0.05f, random);
    oj::render::Node* chassis = view.hasModel() ? view.model()->node("_CHASS") : nullptr;
    if (chassis == nullptr) {
        check(false, std::string("the car shown, its chassis (") + (view.hasModel() ? "no _CHASS" : "no model") + ")");
        return;
    }
    const bool levelFirst = chassis->local[7] == 0.0f;
    car.lowering = oj::Car::bitsOf(0.8f);
    view.refit(car, shared.defs);
    const bool waits = view.refitting() && view.busy() && chassis->local[7] == 0.0f;
    view.frame(0.0f, random);
    check(levelFirst && waits, "the parts changed: the view busy (+0x40) until its next frame");
    check(!view.busy() && chassis->local[7] == -(car.def->maxDropHeight * 0.8f),
          "the next frame: the chassis down by MAXDROPHEIGHT x 0.8");
}

// The view's world draws each tree with the parts of opaque classes first, then the others (0x0054C6B8: 0x00550500,
// modes 1 and 2), so the car's glass goes over what is behind it: drawn in the parts' order, a spoiler showed through
// the windscreen.
OJ_INTEGRATION_TEST(CarView, GlassDrawnLast) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    if (!shared.carDefsRead || shared.defs.cars.size() < 2) {
        check(false, "cardefs.txt read");
        return;
    }
    DrawOrderDevice device;
    oj::scene::Engine engine(device, shared.tables, 3, false);
    oj::CrtRandom random;
    const oj::Language language = *oj::Language::byName("english");
    auto screen = std::make_shared<oj::ui::Screen>(device, language);
    screen->setView3dFactory([&](const oj::ui::Element& e) -> std::unique_ptr<oj::ui::View3d> {
        return std::make_unique<oj::scene::View3d>(engine, e, random);
    });
    if (!screen->load("cmscarcollection2")) {
        check(false, "cmscarcollection2.ui read");
        return;
    }
    oj::Car car;
    car.def = &shared.defs.cars[1];
    oj::frontend::customrace::clearParts(car);
    oj::scene::CarView::Setup setup;
    setup.element = "CAR3D";
    oj::scene::CarView view(engine, *screen, setup, shared.catalogue, shared.tables);
    view.show(car, nullptr, 0, 0.0f, 0.5f, false, random);
    view.finish(0.05f, random);
    if (!view.hasModel() || view.model()->root() == nullptr) {
        check(false, "the car shown");
        return;
    }
    std::set<oj::render::BufferId> opaque, late;
    partsBelow(*view.model()->root(), opaque, late);
    device.drawn.clear();
    screen->draw(1.0f, 0.02f);
    bool lateSeen = false, opaqueAfter = false;
    size_t lateDrawn = 0;
    for (const oj::render::BufferId b : device.drawn) {
        if (late.count(b) != 0 && opaque.count(b) == 0) {
            lateSeen = true;
            ++lateDrawn;
        } else if (opaque.count(b) != 0 && late.count(b) == 0 && lateSeen) {
            opaqueAfter = true;
        }
    }
    check(lateDrawn > 0 && !opaqueAfter, "the car's glass drawn after all its other parts");
}

// The car views' loads read gen_neon with the mesh cache off (0x004598B3): each view's neon a mesh and materials of its
// own, so two views at once (the betting window's leader and player) colour each its own (through the cache the two
// shared one material: the leader's neon took the player's colour).
OJ_INTEGRATION_TEST(CarView, NeonOfItsOwn) {
    oj::test::InGameFolder folder;
    if (!folder.valid()) return;
    const oj::test::SharedGameData& shared = oj::test::sharedGameData();
    oj::test::NullRenderDevice device;
    oj::scene::Engine engine(device, shared.tables, 3, false);
    oj::CrtRandom random;
    const auto mesh = [](const std::unique_ptr<oj::scene::Model>& m) -> const oj::render::Mesh* {
        return m && !m->meshNodes.empty() ? m->meshNodes.front()->mesh : nullptr;
    };
    const std::unique_ptr<oj::scene::Model> a = engine.loadModel("gen_neon", random, true);
    const std::unique_ptr<oj::scene::Model> b = engine.loadModel("gen_neon", random, true);
    const std::unique_ptr<oj::scene::Model> c = engine.loadModel("gen_neon", random);
    const std::unique_ptr<oj::scene::Model> d = engine.loadModel("gen_neon", random);
    const bool own = mesh(a) != nullptr && mesh(b) != nullptr && mesh(a) != mesh(b) && !mesh(a)->parts().empty() &&
                     !mesh(b)->parts().empty() && mesh(a)->parts()[0].material != mesh(b)->parts()[0].material;
    check(own, "gen_neon read for each view: a mesh and a material of its own");
    check(mesh(c) != nullptr && mesh(c) == mesh(d), "through the mesh cache: one mesh for both");
}
