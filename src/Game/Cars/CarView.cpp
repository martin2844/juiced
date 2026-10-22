#include "Game/Cars/CarView.h"

#include "Core/Base/Log.h"
#include "Core/Base/Matrix.h"

#include <chrono>
#include <cmath>
#include <optional>
#include <thread>

namespace oj::scene {

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

// 0x00407910: an angle brought into (-pi, pi].
float wrap(float a) {
    while (a > kPi) a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}

// The first node whose name holds the text (0x00534EC0) hidden (+0xC0 | 4) when it is a mesh node.
void hideNode(CarModel& model, const char* text) {
    if (render::Node* node = model.node(text)) {
        if (auto* mesh = dynamic_cast<render::ModelNode*>(node)) mesh->flags |= 4;
    }
}

void turnNode(CarModel& model, const char* text, std::initializer_list<float> angles) {
    render::Node* node = model.node(text);
    if (node == nullptr) return;
    for (float a : angles) oj::rotateY(node->local, a);
    node->invalidate();
}

// 0x004590E0: the lights (_BRKLIGHT, _REVLIGHT, LIGHTSON) and the driver hidden; the front wheels steered by the
// draw (0 straight, 1 0.35, 2 -0.35; 0.5 otherwise), the right wheels turned by 3.14 first.
void poseCar(CarModel& model, uint32_t draw) {
    for (const char* name : {"_BRKLIGHT", "_REVLIGHT", "LIGHTSON", "DRIVER"}) hideNode(model, name);
    float steer = 0.5f;
    if (draw == 0) {
        steer = 0.0f;
    } else if (draw == 1) {
        steer = 0.35f;
    } else if (draw == 2) {
        steer = -0.35f;
    }
    turnNode(model, "WHEEL_FR", {3.14f, steer});
    turnNode(model, "WHEEL_FL", {steer});
    turnNode(model, "WHEEL_RR", {3.14f});
}

}  // namespace

bool meshBox(render::Node* node, std::array<float, 3>& low, std::array<float, 3>& high) {
    auto* mesh = dynamic_cast<render::ModelNode*>(node);
    if (mesh == nullptr || mesh->mesh == nullptr) return false;
    low = mesh->mesh->boundsMin();
    high = mesh->mesh->boundsMax();
    return true;
}

CarView::CarView(Engine& engine, ui::Screen& screen, const Setup& setup, const PartCatalogue& catalogue,
                 const ExeTables& tables)
    : m_engine(engine),
      m_screen(screen),
      m_screenAlive(screen.weak_from_this()),
      m_setup(setup),
      m_catalogue(catalogue),
      m_tables(tables) {}

CarView::~CarView() {
    if (!m_screenAlive.expired()) {
        clear();
    } else {
        drop();
    }
    // The loads still running read this view and write their jobs: waited for, the device let go to them.
    for (const std::shared_ptr<Job>& j : m_dropped) {
        loadingThread().wait(*j->task, [this] { m_engine.device().letLoadingRun(); });
    }
}

View3d* CarView::view() {
    return dynamic_cast<View3d*>(m_screen.view(m_setup.element));
}

void CarView::show(const Car& car, uint32_t* current, uint32_t place, float yaw, float rate, bool fade,
                   CrtRandom& loading) {
    drop();
    if (view() == nullptr) return;
    if (current != nullptr) *current = place;
    auto job = std::make_shared<Job>();
    job->car = &car;
    job->loaded = car;
    job->yaw = yaw;
    job->rate = rate;
    job->loading = &loading;
    // 0x004593A0: LOGO and ORIGIN hidden while the car loads.
    if (!m_setup.logo.empty()) m_screen.setHidden(m_setup.logo, true);
    if (!m_setup.origin.empty()) m_screen.setHidden(m_setup.origin, true);
    // LOGO "ui_<manufacturer>" (the names at 0x007162F8) for a make over 0; ORIGIN the flag of the region.
    if (car.def != nullptr) {
        const int make = car.def->make;
        if (!m_setup.logo.empty() && make > 0 && static_cast<size_t>(make) < m_tables.manufacturers.size()) {
            m_screen.setTexture(m_setup.logo, "ui_" + m_tables.manufacturers[static_cast<size_t>(make)]);
        }
        if (!m_setup.origin.empty()) {
            const int region = car.def->region;
            const char* flag = region == 1   ? "ui_flag_japan"
                               : region == 3 ? "ui_flag_euro"
                               : region == 4 ? "ui_flag_oz"
                                             : "ui_flag_usa";
            m_screen.setTexture(m_setup.origin, flag);
        }
        if (!m_setup.name.empty()) {
            std::wstring name;
            for (char c : car.def->displayName) name += static_cast<wchar_t>(static_cast<unsigned char>(c));
            m_screen.setText(m_setup.name, name);
        }
    }
    Job* const queued = job.get();
    job->task = loadingThread().queue([this, queued] { load(*queued); });
    m_job = std::move(job);
    if (!fade) {
        m_fade = 0.0f;
        emptyView();
    }
}

void CarView::drop() {
    freeDropped();
    if (!m_job) return;
    if (!loadingThread().cancel(m_job->task) && !m_job->task->done()) m_dropped.push_back(m_job);
    m_job.reset();
}

void CarView::freeDropped() {
    std::erase_if(m_dropped, [](const std::shared_ptr<Job>& j) { return j->task->done(); });
}

// The load (0x00459730, on the loading thread: its own rand()): after a first 10 ms (its wait for the car loads'
// token [0x00715C3C], 0x00459750 .. 0x004597FE, which the one loading thread here makes a single wait),
// cars\<car>_ui\models\<model>.boh, gen_neon, the mirror image (0x00459070: the model's copy, every mesh node
// mirrored, +0xC0 | 1, turned upside down about Y), the car's model objects (0x0048A0A0) and arcade_shadow, with the
// cars' folders in use (the engine's loads held the while) and the mesh cache off (0x004598B3: [G+0x1C]+0x14 0, left
// off till a scene car or a part is read; here for these loads alone): gen_neon and arcade_shadow read anew with
// materials of their own, so two views at once (the betting window's, the grid walk's) colour each its own neon.
void CarView::load(Job& j) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const CarFolders folders(m_engine);
    j.model = CarModel::load(m_engine, j.loaded, m_catalogue, *j.loading, true);
    j.neon = m_engine.loadModel("gen_neon", *j.loading, true);
    j.mirror = CarModel::load(m_engine, j.loaded, m_catalogue, *j.loading, true);
    if (j.mirror) j.mirror->mirror();
    j.shadow = m_engine.loadModel("arcade_shadow", *j.loading, true);
}

void CarView::frame(float dt, CrtRandom& random) {
    freeDropped();
    if (m_job) {
        if (m_fade > 0.0f) {
            m_fade = m_fade - (dt + dt);
            if (m_fade < 0.0f) m_fade = 0.0f;
        }
        if (m_fade == 0.0f) {
            emptyView();
            if (m_job->task->done()) attach(random);
        }
    } else if (m_fade < 1.0f) {
        m_fade = (dt + dt) + m_fade;
        if (!(m_fade <= 1.0f)) m_fade = 1.0f;
    }
    if (refitting()) putRefit(random);
}

void CarView::setCameraPlace(const oj::Vec4& place) {
    m_camera = place;
    if (View3d* v = view()) v->setCameraPlace(place);
}

void CarView::finish(float dt, CrtRandom& random) {
    // A frame time of 0 would wait for ever in the stock code; here the fade ends after many frames. Faded out, the
    // load is waited for 5 ms at a time, the device let go to it.
    for (int frames = 0; busy(); ++frames) {
        if (frames == 100000) m_fade = 0.0f;
        frame(dt, random);
        if (busy() && m_fade == 0.0f) {
            m_engine.device().letLoadingRun();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

void CarView::repaint(const Car& car) {
    if (!m_shown) return;
    if (m_shown->model) {
        m_shown->model->paint(car);
        colourNeon(car.neon);
    }
    if (m_shown->mirror) m_shown->mirror->paint(car);
}

void CarView::refit(Car& car, const CarDefs& defs) {
    if (busy() || !m_shown || !m_shown->model) return;
    m_shown->model->tint(car);
    m_refit = {&car, &defs};
}

void CarView::putRefit(CrtRandom& random) {
    const Refit r = m_refit;
    m_refit = {};
    if (!m_shown) return;
    Car& car = *r.car;
    for (CarModel* model : {m_shown->model.get(), m_shown->mirror.get()}) {
        if (model == nullptr) continue;
        model->paint(car);
        model->refitParts(car, m_catalogue);
        // 0x0045A9D9, 0x0045A9E7: the primer's job after the parts'.
        model->primerToBody();
        model->colourWheels(car);
        model->colourDecal(car);
        if (model == m_shown->mirror.get()) model->tint(car);
        model->sponsorDecals(car, m_catalogue);
    }
    if (m_shown->neon) {
        if (static_cast<int8_t>(car.body[9]) > -1) {
            showNeon(true);
            // Stock problem: without a [NeonColour] the count divides by 0; here the colour stays.
            if (!r.defs->neons.empty()) {
                const auto count = static_cast<uint32_t>(r.defs->neons.size());
                const uint32_t at = static_cast<uint32_t>(random.next()) % count;
                car.neon = r.defs->neons[at];
            }
            colourNeon(car.neon);
        } else {
            showNeon(false);
        }
    }
    // 0x0045A027: the chassis of the model and of its mirror image at the car's lowering, as after a car is put in
    // (0x00459F82): the automatic modification's 0.8 (1.0 for a prototype) and the parts cleared's 0 show.
    if (m_shown->model) dropHeight(*m_shown->model, car);
    if (m_setup.mirror && m_shown->mirror) dropHeight(*m_shown->mirror, car);
}

void CarView::colourNeon(uint32_t argb) {
    if (!m_shown || !m_shown->neon) return;
    for (auto& node : m_shown->neon->meshNodes) {
        if (node->mesh == nullptr) continue;
        for (const auto& part : node->mesh->parts()) {
            if (part.material == nullptr) continue;
            const int32_t one = 1;
            part.material->setParam(0, &argb);
            part.material->setParam(4, &one);
        }
    }
}

void CarView::showNeon(bool shown) {
    if (!m_shown || !m_shown->neon || m_shown->neon->root == nullptr) return;
    for (auto& node : m_shown->neon->meshNodes) {
        if (shown) {
            node->flags &= ~4u;
        } else {
            node->flags |= 4u;
        }
    }
}

void CarView::clear() {
    drop();
    emptyView();
}

void CarView::emptyView() {
    if (View3d* v = view()) {
        v->clearListed();
        v->worldDraws.clear();
    }
    m_shown.reset();
    m_car = nullptr;
    if (!m_setup.logo.empty()) m_screen.setHidden(m_setup.logo, true);
    if (!m_setup.origin.empty()) m_screen.setHidden(m_setup.origin, true);
}

void CarView::dropHeight(CarModel& model, const Car& car) {
    render::Node* chassis = model.node("_CHASS");
    if (chassis == nullptr || car.def == nullptr) return;
    chassis->local[3] = 0.0f;
    chassis->local[11] = 0.0f;
    chassis->local[7] = -(car.def->maxDropHeight * Car::asFloat(car.lowering));
    chassis->invalidate();
}

void CarView::attach(CrtRandom& random) {
    View3d* v = view();
    std::shared_ptr<Job> job = std::move(m_job);
    const auto draw = static_cast<uint32_t>(random.next()) % 3;
    if (v == nullptr || !job->model) {
        m_shown = std::move(job);
        return;
    }
    Job& j = *job;
    std::optional<uint32_t> neonColour;
    const auto listCar = [&](CarModel& model) {
        render::Node* root = model.root();
        if (root == nullptr) return;
        const int at = v->listNode(root, false);
        oj::rotateY(root->local, wrap(j.yaw));
        root->invalidate();
        v->setTurnRates(at, 0.0f, j.rate, 0.0f);
        poseCar(model, draw);
        v->worldDraws.push_back(
            [&model](render::Renderer3d& r, render::Camera& c, uint32_t mode) { model.draw(r, c, mode); });
    };
    if (m_setup.mirror && j.mirror) listCar(*j.mirror);
    listCar(*j.model);
    int next = v->listedCount();
    if (j.shadow && j.shadow->root != nullptr && m_setup.shadow) {
        v->listNode(j.shadow->root, false);
        Model& shadow = *j.shadow;
        v->worldDraws.push_back([&shadow](render::Renderer3d& r, render::Camera& c, uint32_t mode) {
            for (auto& node : shadow.meshNodes) node->draw(r, c, mode);
        });
    }
    if (next < v->listedCount()) {
        render::Node* node = v->listedNode(next);
        oj::rotateY(node->local, wrap(j.yaw));
        node->invalidate();
        v->setTurnRates(next, 0.0f, j.rate, 0.0f);
    }
    if (m_setup.shadow) {
        // 0x0045A560: the neon listed to turn about the parent's axes and made the size of the chassis: x by its
        // width x 1.25, z by its length x 1.1, each over the neon's.
        if (j.neon && j.neon->root != nullptr) {
            Model& neon = *j.neon;
            v->listNode(neon.root, true);
            v->worldDraws.push_back([&neon](render::Renderer3d& r, render::Camera& c, uint32_t mode) {
                for (auto& node : neon.meshNodes) node->draw(r, c, mode);
            });
            std::array<float, 3> carLow{}, carHigh{}, neonLow{}, neonHigh{};
            render::Node* chassis = j.model->node("_CHASS");
            render::Node* neonMesh = neon.meshNodes.empty() ? neon.root : neon.meshNodes.front().get();
            if (meshBox(chassis, carLow, carHigh) && meshBox(neonMesh, neonLow, neonHigh)) {
                neon.root->local[10] = static_cast<float>(static_cast<double>(carHigh[2] - carLow[2]) * 1.1 /
                                                          static_cast<double>(neonHigh[2] - neonLow[2]));
                neon.root->local[0] = static_cast<float>(static_cast<double>(carHigh[0] - carLow[0]) * 1.25 /
                                                         static_cast<double>(neonHigh[0] - neonLow[0]));
                neon.root->invalidate();
            }
        }
        ++next;
        if (next < v->listedCount()) {
            render::Node* node = v->listedNode(next);
            oj::rotateYParent(node->local, j.yaw);
            node->invalidate();
            v->setTurnRates(next, 0.0f, j.rate, 0.0f);
            // The neon shown for a car with one (+0x0D over -1) in its colour (0x0048BEB0), else hidden.
            if (auto* mesh = dynamic_cast<render::ModelNode*>(node)) {
                if (static_cast<int8_t>(j.car->body[9]) > -1) {
                    mesh->flags &= ~4u;
                    neonColour = j.car->neon;
                } else {
                    mesh->flags |= 4;
                }
            }
        }
    }
    if (!m_setup.logo.empty()) m_screen.setHidden(m_setup.logo, j.car->def == nullptr || j.car->def->make <= 0);
    if (!m_setup.origin.empty()) m_screen.setHidden(m_setup.origin, false);
    m_car = j.car;
    m_shown = std::move(job);
    if (neonColour) colourNeon(*neonColour);
    place();
    dropHeight(*m_shown->model, *m_car);
    if (m_setup.mirror && m_shown->mirror) dropHeight(*m_shown->mirror, *m_car);
}

// 0x0045AEF0: in the map mode (4) the camera looks at (0, 20, 0), the mode 3 then 4 with the arm (0, 0, -725), the
// rig's own turn -0.25, the zoom 0 going to 0.7; else the camera at its place (+0x18 .. +0x20) looking at (0, 20, 0)
// and the zoom and its target +0x14.
void CarView::place() {
    View3d* v = view();
    if (v == nullptr) return;
    const oj::Vec4 at{0.0f, 20.0f, 0.0f, 0.0f};
    if (v->mode() == MoveMode::Map) {
        v->lookAt(at, true);
        v->setMode(MoveMode::CameraManual, nullptr);
        const oj::Vec4 arm{0.0f, 0.0f, -725.0f, 0.0f};
        v->setMode(MoveMode::Map, &arm);
        v->autoSpeed = -0.25f;
        v->setZoomTarget(0.0f);
        v->setZoom(0.0f);
        v->setZoomTarget(0.7f);
        return;
    }
    v->setCameraPlace(m_camera);
    v->lookAt(at, true);
    v->setZoomTarget(m_setup.zoom);
    v->setZoom(m_setup.zoom);
}

GarageView::GarageView(Engine& engine, ui::Screen& screen, std::string element, const PartCatalogue& catalogue)
    : m_engine(engine),
      m_screen(screen),
      m_screenAlive(screen.weak_from_this()),
      m_element(std::move(element)),
      m_catalogue(catalogue) {}

// The window's view emptied of the models freed with it.
GarageView::~GarageView() {
    if (!m_screenAlive.expired()) clear();
}

View3d* GarageView::view() {
    return dynamic_cast<View3d*>(m_screen.view(m_element));
}

void GarageView::clear() {
    if (View3d* v = view()) {
        v->clearListed();
        v->worldDraws.clear();
    }
    m_scene.reset();
    m_sceneMirror.reset();
    m_carDamage.reset();
    m_mirrorDamage.reset();
    m_car.reset();
    m_carMirror.reset();
    m_shadow.reset();
    m_neon.reset();
    m_plinth = nullptr;
    m_sign = nullptr;
    m_garage = false;
}

// 0x00437BB0: cars\<car>_ui\models\<model>.boh in the element's world, the lights (_BRKLIGHT, _REVLIGHT, LIGHTSON) and
// the driver hidden, WHEEL_FR and WHEEL_RR turned by 3.14; its model object (0x0048A0A0, 0x0048A500).
std::unique_ptr<CarModel> GarageView::loadCar(const Car& car, CrtRandom& loading,
                                              std::unique_ptr<DamageGrid>* damage) {
    std::unique_ptr<CarModel> model = CarModel::load(m_engine, car, m_catalogue, loading, true);
    if (!model) return model;
    for (const char* name : {"_BRKLIGHT", "_REVLIGHT", "LIGHTSON", "DRIVER"}) hideNode(*model, name);
    turnNode(*model, "WHEEL_FR", {3.14f});
    turnNode(*model, "WHEEL_RR", {3.14f});
    if (damage != nullptr) {
        // 0x0048A500 as the race's: the body's nodes (their vertices' alpha and dents) and the decals', the cells
        // linked, the damage the car keeps dented again.
        auto grid = std::make_unique<DamageGrid>();
        if (car.grid) grid->take(*car.grid);
        for (const char* name : {"_CHASS", "FBUMP", "RBUMP", "BONNET", "SSKIRT_L", "SSKIRT_R"}) {
            if (const auto points = model->damagePoints(name)) grid->addNode(*points, true);
        }
        for (const char* name : {"DECAL", "SPONS"}) {
            if (const auto points = model->damagePoints(name)) grid->addNode(*points, false);
        }
        grid->finish(loading);
        *damage = std::move(grid);
    }
    return model;
}

int GarageView::listModel(Model& model, bool parent) {
    View3d* v = view();
    if (v == nullptr || model.root == nullptr) return -1;
    v->worldDraws.push_back([&model](render::Renderer3d& r, render::Camera& c, uint32_t mode) {
        for (auto& node : model.meshNodes) node->draw(r, c, mode);
    });
    return v->listNode(model.root, parent);
}

int GarageView::listCar(CarModel& model) {
    View3d* v = view();
    if (v == nullptr || model.root() == nullptr) return -1;
    v->worldDraws.push_back(
        [&model](render::Renderer3d& r, render::Camera& c, uint32_t mode) { model.draw(r, c, mode); });
    return v->listNode(model.root(), false);
}

// 0x00436080: a model the size of the car's chassis (_CHASS), x by its width x `xScale`, z by its length x `zScale`,
// each over the model's.
void GarageView::fitToChassis(Model& model, float xScale, float zScale) {
    if (!m_car || model.root == nullptr) return;
    std::array<float, 3> carLow{}, carHigh{}, low{}, high{};
    render::Node* mesh = model.meshNodes.empty() ? model.root : model.meshNodes.front().get();
    if (!meshBox(m_car->node("_CHASS"), carLow, carHigh) || !meshBox(mesh, low, high)) return;
    model.root->local[10] = static_cast<float>(static_cast<double>(carHigh[2] - carLow[2]) * zScale /
                                               static_cast<double>(high[2] - low[2]));
    model.root->local[0] = static_cast<float>(static_cast<double>(carHigh[0] - carLow[0]) * xScale /
                                              static_cast<double>(high[0] - low[0]));
    model.root->invalidate();
}

void GarageView::enterBuy(const Car& car, const DealerOffer* offer, bool buying, CrtRandom& loading) {
    clear();
    View3d* v = view();
    if (v == nullptr) return;
    const bool used = buying && offer != nullptr && offer->used != 0;
    const bool turning = buying && !used;
    m_plinthTurns = turning;
    if (buying) {
        if (used && offer->damage != 0) {
            m_scene = m_engine.loadModel("scrapyard", loading);
            v->setAtmosphere("scrapyard");
            v->buildSky();
        } else if (used) {
            m_scene = m_engine.loadModel("showroom3", loading);
            v->setAtmosphere("dealer");
        } else {
            m_scene = m_engine.loadModel("showroom2", loading);
        }
        // The scene's mirror image under the floor, without the plinth, the floors and the trees.
        m_sceneMirror = m_scene ? m_engine.loadModel(used && offer->damage != 0 ? "scrapyard"
                                                     : used ? "showroom3" : "showroom2", loading)
                                : nullptr;
        if (m_sceneMirror) {
            m_sceneMirror->mirror();
            listModel(*m_sceneMirror, false);
            for (const char* name : {"show_plinth", "resessft", "show_plinth_bot", "show_floor", "scrapfloor1",
                                     "ssb_tree_1", "ssb_tree_2", "ssb_tree_03", "ssb_tree_04", "ssb_tree_05",
                                     "ssb_tree_06", "ssb_tree_07"}) {
                if (auto* node = dynamic_cast<render::ModelNode*>(m_sceneMirror->find(name))) node->flags |= 4;
            }
        }
    } else {
        m_scene = m_engine.loadModel("don_garage1", loading);
    }
    if (m_scene) {
        m_plinth = m_scene->find("show_plinth");
        m_sign = m_scene->find("SHOW3_SIGN");
        if (m_sign != nullptr) {
            m_sign->local[3] = 0.0f;
            m_sign->local[7] = 0.0f;
            m_sign->local[11] = 300.0f;
            m_sign->invalidate();
        }
    }
    m_car = loadCar(car, loading, &m_carDamage);
    // 0x00438080 (buying): the car's mirror image listed, turning 0.35 a second about Y for a new car.
    if (buying && m_car) {
        m_carMirror = loadCar(car, loading, &m_mirrorDamage);
        if (m_carMirror) {
            m_carMirror->mirror();
            const int at = listCar(*m_carMirror);
            if (at >= 0 && turning) v->setTurnRates(at, 0.0f, 0.35f, 0.0f);
        }
    }
    if (m_scene) listModel(*m_scene, false);
    // 0x00437FF0: the car listed at the middle, turning as its mirror image.
    if (m_car) {
        const int at = listCar(*m_car);
        if (at >= 0 && turning) v->setTurnRates(at, 0.0f, 0.35f, 0.0f);
    }
    // gen_shadow at the middle, turning for a new car (the last listed), of the cars' folders (0x00438629 ..
    // 0x0043887A).
    {
        const CarFolders folders(m_engine);
        m_shadow = m_engine.loadModel("gen_shadow", loading);
    }
    if (m_shadow) {
        const int at = listModel(*m_shadow, false);
        if (at >= 0 && turning) v->setTurnRates(at, 0.0f, 0.35f, 0.0f);
    }
    // The camera on its arm (0, 0, -425) looking at (0, 50, 0); for a new car the rig's yaw within -0.5 .. 0.5, its
    // own turn 0 and its angles (-0.2, 0, 0).
    if (v->mode() != MoveMode::Map) {
        v->lookAt({0.0f, 50.0f, 0.0f, 0.0f}, true);
        const oj::Vec4 arm{0.0f, 0.0f, -425.0f, 0.0f};
        v->setMode(MoveMode::Map, &arm);
        if (turning) {
            v->yawLimits[0] = -0.5f;
            v->yawLimits[1] = 0.5f;
            v->autoSpeed = 0.0f;
            v->rig[0] = -0.2f;
            v->rig[1] = 0.0f;
            v->rig[2] = 0.0f;
            v->rig[3] = 0.0f;
        } else {
            v->setMode(MoveMode::Map, nullptr);
        }
    }
}

void GarageView::cameraTo(const oj::Vec4& offset) {
    View3d* v = view();
    if (v == nullptr) return;
    render::Node* root = m_car ? m_car->root() : nullptr;
    const float x = root != nullptr ? root->local[3] : 0.0f;
    const float y = root != nullptr ? root->local[7] : 0.0f;
    const float z = root != nullptr ? root->local[11] : 0.0f;
    v->setCameraGoal({x + offset[0], -15.0f + (y + offset[1]), z + offset[2], 0.0f});
    v->lookAt({x, 50.0f, z, 0.0f}, false);
}

void GarageView::orbit() {
    View3d* v = view();
    if (v == nullptr) return;
    v->autoSpeed = -0.1f;
    if (v->mode() == MoveMode::Map) return;
    v->lookAt({0.0f, 50.0f, 0.0f, 0.0f}, true);
    v->setMode(MoveMode::Map, nullptr);
    v->autoSpeed = -0.1f;
}

void GarageView::mapMode() {
    View3d* v = view();
    if (v == nullptr) return;
    v->lookAt({0.0f, 50.0f, 0.0f, 0.0f}, true);
    v->setMode(MoveMode::Map, nullptr);
    v->autoSpeed = -0.1f;
}

void GarageView::holdRig() {
    setRig(0.0f, 0.0f);
}

void GarageView::setRig(float pitch, float yaw) {
    View3d* v = view();
    if (v == nullptr) return;
    v->rig[0] = pitch;
    v->rig[1] = yaw;
    v->autoSpeed = 0.0f;
    v->inputDelay = 0.25f;
}

void GarageView::zoomTo(float zoom) {
    if (View3d* v = view()) v->setZoomTarget(zoom);
}

void GarageView::turnPlinth(float angle) {
    if (m_plinth == nullptr || !m_plinthTurns) return;
    oj::rotateY(m_plinth->local, angle);
    m_plinth->invalidate();
}

void GarageView::dropChassis(const Car& car) {
    if (!m_car) return;
    render::Node* chassis = m_car->node("_CHASS");
    if (chassis == nullptr || car.def == nullptr) return;
    chassis->local[3] = 0.0f;
    chassis->local[11] = 0.0f;
    chassis->local[7] = -(car.def->maxDropHeight * Car::asFloat(car.lowering));
    chassis->invalidate();
}

void GarageView::enterSel(const Car& car, CrtRandom& loading) {
    clear();
    m_garage = true;
    View3d* v = view();
    if (v == nullptr) return;
    m_scene = m_engine.loadModel("don_garage1", loading);
    m_car = loadCar(car, loading, &m_carDamage);
    if (m_scene) listModel(*m_scene, false);
    if (m_car) listCar(*m_car);
    // 0x00436200: the camera at the car's place + 50 each way + (-233.37, 115.84, 493.664) at once, looking at the
    // car's place + 50.
    render::Node* root = m_car ? m_car->root() : nullptr;
    const float x = root != nullptr ? root->local[3] : 0.0f;
    const float y = root != nullptr ? root->local[7] : 0.0f;
    const float z = root != nullptr ? root->local[11] : 0.0f;
    const oj::Vec4 at{x + 50.0f, y + 50.0f, z + 50.0f, 0.0f};
    v->setCameraPlace({-233.37f + at[0], 115.84f + at[1], 493.664f + at[2], 0.0f});
    v->lookAt(at, true);
    // 0x00435FB0: gen_shadow sized to the chassis (x 1.25, z 1); a car with a neon gets gen_neon so sized, its colour
    // the car's (+0x2C; the material of the car's model object, 0x00436160). The cars' folders in use (0x00436599 ..
    // 0x00436720).
    {
        const CarFolders folders(m_engine);
        m_shadow = m_engine.loadModel("gen_shadow", loading);
    }
    if (m_shadow) {
        listModel(*m_shadow, false);
        fitToChassis(*m_shadow, 1.25f, 1.0f);
    }
    if (static_cast<int8_t>(car.body[9]) > -1) showNeon(car, loading);
}

void GarageView::listNeon() {
    View3d* v = view();
    if (v == nullptr || !m_neon || m_neon->root == nullptr) return;
    Model* neon = m_neon.get();
    v->worldDraws.push_back([this, neon](render::Renderer3d& r, render::Camera& c, uint32_t mode) {
        if (!m_neonShown) return;
        for (auto& node : neon->meshNodes) node->draw(r, c, mode);
    });
    v->listNode(neon->root, false);
}

void GarageView::showNeon(const Car& car, CrtRandom& loading) {
    if (!m_neon) {
        const CarFolders folders(m_engine);
        m_neon = m_engine.loadModel("gen_neon", loading);
        if (!m_neon) return;
        listNeon();
    }
    fitToChassis(*m_neon, 1.25f, 1.0f);
    m_neonShown = true;
    colourNeon(car.neon);
}

void GarageView::hideNeon() {
    m_neonShown = false;
}

void GarageView::colourNeon(uint32_t argb) {
    if (!m_neon) return;
    for (auto& node : m_neon->meshNodes) {
        if (node->mesh == nullptr) continue;
        for (const auto& part : node->mesh->parts()) {
            if (part.material == nullptr) continue;
            const int32_t one = 1;
            part.material->setParam(0, &argb);
            part.material->setParam(4, &one);
        }
    }
}

}  // namespace oj::scene
