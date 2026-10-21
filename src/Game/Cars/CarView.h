#pragma once
// The car view of the interface (docs/spec/14-career-windows.md, "The car view"): a car's model of cars\<car>_ui\
// turning in a Begin3d element, with its mirror image under the floor, the shadow and the neon, faded out and in when
// the car changes.

#include "Core/Cars/Car.h"
#include "Core/Cars/CarDefs.h"
#include "Core/Career/Career.h"
#include "Core/Data/ExeTables.h"
#include "Core/Base/Random.h"
#include "Game/Cars/CarModel.h"
#include "Game/Scene/Scene3d.h"
#include "Game/UI/UiScreen.h"

#include <memory>
#include <string>

namespace oj::scene {

// The box of a node's mesh (its node's +4 method): the mesh's bounds; none without a mesh.
bool meshBox(render::Node* node, std::array<float, 3>& low, std::array<float, 3>& high);

class CarView {
public:
    // 0x0045A2C0: the Begin3d element (+0x04), the name's text (+0x08), LOGO (+0x0C) and ORIGIN (+0x10) elements
    // (empty for none), the camera's zoom (+0x14), the mirror image (+0x30), the car's folder kept (+0x34) and the
    // shadow (+0x38). keepFolder: the load's +0x48 (0x0045FC80); without it the load lets go of the car's folder
    // (cars\<car>_ui, the load's +0x44) at its end (0x00459B24); with it the load holds the folder until it is freed
    // (0x004596AD) and the view takes it on too (its +0x28, 0x00459C06). The folders are not followed here.
    struct Setup {
        std::string element;
        std::string name;
        std::string logo;
        std::string origin;
        float zoom = 1.0f;
        bool mirror = false;
        bool keepFolder = false;
        bool shadow = false;
    };

    CarView(Engine& engine, ui::Screen& screen, const Setup& setup, const PartCatalogue& catalogue,
            const ExeTables& tables);
    // 0x0045A340: the load dropped and the view emptied (0x0045AE90) first.
    ~CarView();

    // 0x0045A080: the car of a list shown (the list's +0x10, `current`, takes its place): turned to `yaw`, turning by
    // `rate` a second about Y. Without `fade` the car shown goes at once.
    void show(const Car& car, uint32_t* current, uint32_t place, float yaw, float rate, bool fade,
              CrtRandom& loading);
    // 0x00459EE0, each frame: the fade and the car of the load put in the view.
    void frame(float dt, CrtRandom& random);
    // 0x0045A880: a car still loading.
    bool busy() const { return m_job != nullptr; }
    // The view's model object (+0x00) made.
    bool hasModel() const { return m_shown && m_shown->model; }
    // The car shown's model; none without one.
    CarModel* model() const { return m_shown ? m_shown->model.get() : nullptr; }
    // 0x0045AE90: the load dropped and the view emptied.
    void clear();
    // 0x0045A6F0: the car shown painted again (0x0048BF90), then the gen_neon material (the model object's +0xF0,
    // 0x0048A3AC) its colour (parameter 0, the car's +0x2C) and its parameter 4 1, then its mirror image painted.
    void repaint(const Car& car);
    // 0x0045A8A0 after the car's parts changed: the parts of the model and its mirror image (CarModel::refitParts: a
    // part taken off, as the decal of the parts cleared, empties its slots), their wheel colour, decal colour, tint and
    // sponsor decals made again at once. The stock code puts the model's parts again and loads the
    // car anew on the loading thread (+0x40), the windows showing MODDING meanwhile; here no second load is busy.
    // Its last job's main-thread part (0x0045A790, the job's slot 2): with a neon (+0x0D over -1) the element's
    // gen_neon shown and the car's neon colour (+0x2C) a random one of [NeonColour] (the defs' +0x20: rand() % their
    // count, the main thread's), the material coloured (0x0048BEB0); without one gen_neon hidden.
    void refit(Car& car, const CarDefs& defs, CrtRandom& random);
    // +0x4C: how much the car is shown (0 .. 1).
    float shown() const { return m_fade; }
    // 0x0045B050: the camera's place (+0x18 .. +0x20) and the element's camera there at once.
    void setCameraPlace(const oj::Vec4& place);
    // The enters' wait: frames (0x00459EE0) at the frame time until the car is in (the loading thread pumped and
    // 5 ms slept between them in the stock code).
    void finish(float dt, CrtRandom& random);

private:
    // The load (0x004593A0, run 0x00459730): the car's model, its mirror image, the neon and the shadow.
    struct Job {
        const Car* car = nullptr;
        float yaw = 0.0f;   // +0x34
        float rate = 0.0f;  // +0x38
        std::unique_ptr<CarModel> model;       // +0x1C, +0x2C
        std::unique_ptr<CarModel> mirror;      // +0x20, +0x30
        std::unique_ptr<Model> shadow;         // +0x24: arcade_shadow
        std::unique_ptr<Model> neon;           // +0x28: gen_neon
    };

    View3d* view();
    // 0x0048BEB0 on the car shown: the gen_neon material's colour (parameter 0) and its parameter 4 1.
    void colourNeon(uint32_t argb);
    // The shown car's gen_neon shown or hidden (+0xC0 bit 4).
    void showNeon(bool shown);
    void emptyView();     // 0x0045ACE0
    void attach(CrtRandom& random);  // 0x00459BB0
    void place();         // 0x0045AEF0
    void dropHeight(CarModel& model, const Car& car);

    Engine& m_engine;
    ui::Screen& m_screen;
    std::weak_ptr<ui::Screen> m_screenAlive;  // the window, while it lives (a kept window outlives its views)
    Setup m_setup;
    const PartCatalogue& m_catalogue;
    const ExeTables& m_tables;
    std::unique_ptr<Job> m_job;            // +0x3C
    // The car shown (+0x00, +0x2C), the neon and the shadow.
    std::unique_ptr<Job> m_shown;
    const Car* m_car = nullptr;
    oj::Vec4 m_camera{0.0f, 180.0f, -760.0f, 0.0f};  // +0x18 .. +0x20
    float m_fade = 0.6f;                   // +0x4C
};

// The garage's scene of carbuy and carsel (the car's model object [0x0075ED00], its folder [0x0075ED04]): the showroom,
// the scrapyard or the garage, the car, its mirror image, its shadow and its neon in the window's Begin3d element.
class GarageView {
public:
    GarageView(Engine& engine, ui::Screen& screen, std::string element, const PartCatalogue& catalogue);
    ~GarageView();

    // carbuy's scene (0x00438150): buying (`offer` the dealer's entry) the showroom of a new car (showroom2), of a used
    // one (showroom3, the atmosphere dealer) or of a damaged one (scrapyard, its atmosphere and sky); selling
    // don_garage1.
    void enterBuy(const Car& car, const DealerOffer* offer, bool buying, CrtRandom& loading);
    // carsel's scene (0x00436380): don_garage1, the car, the camera (0x00436200) and the shadow and neon
    // (0x00435FB0).
    void enterSel(const Car& car, CrtRandom& loading);
    // The views freed.
    void clear();
    CarModel* car() { return m_car.get(); }
    // 0x004362C0: the camera's goal the car's place + the offset (y less 15), looking at the car's place at 50 high.
    void cameraTo(const oj::Vec4& offset);
    // 0x00436010: the view's own turn -0.1 and, out of the map mode, looking at (0, 50, 0) at once and the map mode.
    void orbit();
    // 0x004369B6 (carsel's enter after 0x00436010): looking at (0, 50, 0) at once, the map mode (its arm (0, 0, -500),
    // the rig (-0.12, -1.57)) and the view's own turn -0.1.
    void mapMode();
    // Each frame of the customising windows: the chassis lowered by MAXDROPHEIGHT x the car's +0x58.
    void dropChassis(const Car& car);
    // carbuy's frame (0x00438BE6): the scene's show_plinth (carbuy's +0x18) turned about Y by the angle while the offer
    // shown is not a used car (+0x20, the offer's +0x08); none without a plinth.
    void turnPlinth(float angle);
    // exhaustmenu's enter: the rig's pitch and yaw 0 ([0x0079E798], [0x0079E79C], never written), the view's own
    // turn 0 and 0.25 s before input moves it.
    void holdRig();
    // 0x00436340: the rig's pitch and yaw (+0x160, +0x164), the view's own turn 0 and 0.25 s before input moves it.
    void setRig(float pitch, float yaw);
    // 0x00436160: gen_neon shown under the car (loaded the first time), the size of its chassis (x 1.25, z 1), in the
    // car's neon colour; the unlisting of 0x005B42A0 hides it.
    void showNeon(const Car& car, CrtRandom& loading);
    void hideNeon();
    // The neon material's colour (parameter 0) and its parameter 4 1 (0x0048BEB0).
    void colourNeon(uint32_t argb);
    // The view's zoom goal (+0x18C).
    void zoomTo(float zoom);

private:
    View3d* view();
    // 0x00437BB0 / 0x00435AE0; with `damage` the record's grid object given the model's nodes and finished
    // (0x0048A500: the kept damage dented again, three rand() of the loading thread a cell).
    std::unique_ptr<CarModel> loadCar(const Car& car, CrtRandom& loading, std::unique_ptr<DamageGrid>* damage);
    int listModel(Model& model, bool parent);
    int listCar(CarModel& model);
    void fitToChassis(Model& model, float xScale, float zScale);  // 0x00436080 / 0x0045A560

    Engine& m_engine;
    ui::Screen& m_screen;
    std::weak_ptr<ui::Screen> m_screenAlive;
    std::string m_element;
    const PartCatalogue& m_catalogue;
    std::unique_ptr<Model> m_scene;
    std::unique_ptr<Model> m_sceneMirror;
    std::unique_ptr<CarModel> m_car;
    std::unique_ptr<CarModel> m_carMirror;
    std::unique_ptr<DamageGrid> m_carDamage;     // the grids that dent the models' vertices
    std::unique_ptr<DamageGrid> m_mirrorDamage;
    std::unique_ptr<Model> m_shadow;
    std::unique_ptr<Model> m_neon;
    bool m_garage = false;             // carsel's scene
    bool m_neonShown = false;
    void listNeon();
    render::Node* m_plinth = nullptr;  // +0x18
    bool m_plinthTurns = false;        // +0x20 0: a new car bought
    render::Node* m_sign = nullptr;    // +0x1C
};

}  // namespace oj::scene
