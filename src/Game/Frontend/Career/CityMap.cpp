// The city map of the windows' MAP3D elements (the map object 0x0045B6B0; docs/spec/02-frontend.md,
// "The city map"): the city's model, the routes' models UI_<route>_GUI over it, the camera on a city
// place.

#include "Game/Frontend/Career/CityMap.h"

#include "Core/Base/Log.h"
#include "Core/Base/Random.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

// The view looks at a model's "centre" node (its world place), the node hidden; false without one.
bool lookAtCentre(scene::View3d& view, scene::Model& model, const char* name) {
    render::Node* centre = model.find(name);
    if (centre == nullptr) return false;
    const Mat34& m = centre->world();
    if (auto* mesh = dynamic_cast<render::ModelNode*>(centre)) mesh->flags |= 4;
    view.lookAt({m[3], m[7], m[11], 0.0f}, false);
    return true;
}

}  // namespace

// The 3d view of the window's MAP3D element while the map is on (none else).
scene::View3d* CityMap::view3d(ui::Screen& s) const {
    return m_on ? dynamic_cast<scene::View3d*>(s.view("MAP3D")) : nullptr;
}

// 0x0045B6B0: MAP3D (+0x00) with its camera from 4000 to 5 000 000, its sky (0x005B4660), the rig's pitch within -1.5
// .. -0.15, the atmosphere "map" (0x005B4610); the height 0 (+0x04) and the model coming down 1 (+0x08).
void CityMap::start(ui::Screen& s) {
    m_on = s.find("MAP3D") >= 0;
    m_drop = 0.0f;
    m_dropModel = 1;
    if (scene::View3d* view = view3d(s)) {
        view->camera().setNear(4000.0f);
        view->camera().setFar(5000000.0f);
        view->buildSky();
        view->pitchLimits[0] = -1.5f;
        view->pitchLimits[1] = -0.15f;
        view->setAtmosphere("map");
    }
}

// 0x0045BE00: the view looks at the city place's point (0x00767E68 + 0x28 x place), the map mode on the arm (0, 0,
// -240000), the rig at the pitch with the yaw it had. Stock problem: a place past the table looks past it; here at
// (0, 0, 0).
void CityMap::look(WindowHost& host, ui::Screen& s, size_t place, float pitch) {
    GameData* data = host.gameData();
    scene::View3d* view = view3d(s);
    if (view == nullptr || data == nullptr) return;
    const std::vector<ExeTables::CityLocation>& places = data->tables.cityLocations;
    const oj::Vec4 at = place < places.size() ? oj::Vec4{places[place].x, places[place].y, places[place].z, 0.0f}
                                              : oj::Vec4{0.0f, 0.0f, 0.0f, 0.0f};
    view->lookAt(at, false);
    const float yaw = view->rig[1];
    const oj::Vec4 arm{0.0f, 0.0f, -240000.0f, 0.0f};
    view->setMode(scene::MoveMode::Map, &arm);
    view->rig[0] = pitch;
    view->rig[1] = yaw;
    view->rig[2] = 0.0f;
    view->rig[3] = 0.0f;
}

// 0x0045BC40, each frame: the model coming down (the map's +0x08 of the element's list, 0 the element's own) goes
// down by 15000 x the frame time ([game + 4] + 0x1C) within 250 .. 20000, placed (0, height, 0); the view's zoom and
// its goal 0.6 and its turn 0.3.
void CityMap::frame(WindowHost& host, ui::Screen& s) {
    scene::View3d* view = view3d(s);
    if (view == nullptr) return;
    const int index = m_dropModel - 1;
    if (index >= 0 && index < view->modelCount() && view->listedModel(index) != nullptr) {
        m_drop = std::clamp(m_drop - 15000.0f * host.gameFrameTime(), 250.0f, 20000.0f);
        if (render::Node* root = view->listedModel(index)->root) root->setPosition(0.0f, m_drop, 0.0f);
    }
    view->setZoomTarget(0.6f);
    view->setZoom(0.6f);
    view->autoSpeed = 0.3f;
}

// 0x0045BD10: the element's models after its own taken out.
void CityMap::clear(ui::Screen& s) {
    if (scene::View3d* view = view3d(s)) {
        while (view->modelCount() > 0) view->removeModel(0);
    }
}

// 0x0045B720 with the route UI_<route>_GUI. add (1): the element's models after its own are taken out up to one of
// that name: then the view looks at its "centre" (hidden) and that is all; without one it is added at (0, 0, 0) and
// the view looks at its "centre". Not add (0): each model of the element's list (its own first) of that name shown and
// made the one coming down (+0x08), every other hidden (its top node: the city's own keeps its buildings below it);
// the one of that name placed at (0, 0, 0) and the view at its "centre". Either way the height 20000.
void CityMap::route(WindowHost& host, ui::Screen& s, const std::string& route, bool add) {
    scene::View3d* view = view3d(s);
    if (view == nullptr) return;
    const std::string name = "UI_" + route + "_GUI";
    m_drop = 20000.0f;
    if (!add) {
        scene::Model* found = nullptr;
        for (int i = -1; i < view->modelCount(); ++i) {
            scene::Model* model = i < 0 ? view->ownModel() : view->listedModel(i);
            if (model == nullptr) continue;
            if (_stricmp(model->name.c_str(), name.c_str()) == 0) {
                model->setHidden(false);
                m_dropModel = i + 1;
                if (found == nullptr) found = model;
            } else {
                model->setHidden(true);
            }
        }
        if (found == nullptr || found->root == nullptr) return;
        found->root->setPosition(0.0f, 0.0f, 0.0f);
        lookAtCentre(*view, *found, "centre");
        return;
    }
    while (view->modelCount() > 0) {
        scene::Model* first = view->listedModel(0);
        if (first != nullptr && _stricmp(first->name.c_str(), name.c_str()) == 0) {
            lookAtCentre(*view, *first, "centre");
            return;
        }
        view->removeModel(0);
    }
    const int added = view->addModel(name, {0.0f, 0.0f, 0.0f, 0.0f}, host.viewLoading());
    if (added >= 0) lookAtCentre(*view, *view->listedModel(added), "centre");
}

// 0x0045BB40: UI_<route>_GUI added to the element's models at (0, 0, 0) hidden; the view looks at its "centre0"
// (hidden); the height 20000.
void CityMap::addRoute(WindowHost& host, ui::Screen& s, const std::string& route) {
    scene::View3d* view = view3d(s);
    if (view == nullptr) return;
    const int added = view->addModel("UI_" + route + "_GUI", {0.0f, 0.0f, 0.0f, 0.0f}, host.viewLoading());
    if (added >= 0) {
        scene::Model& model = *view->listedModel(added);
        model.setHidden(true);
        lookAtCentre(*view, model, "centre0");
    }
    m_drop = 20000.0f;
}

// 0x0045BEC0: the city place of the name (0x004627D0: against the nine English names [0x00767DC0 ..], exactly; none
// matching: place 0), the view looking at it as 0x0045BE00 at the pitch -0.4.
void CityMap::lookAt(WindowHost& host, ui::Screen& s, const std::wstring& location) {
    GameData* data = host.gameData();
    size_t place = 0;
    if (data != nullptr) {
        const std::vector<std::wstring>& names = data->tables.locationNames;
        for (size_t k = 0; k < names.size() && k < 9; ++k) {
            if (names[k] == location) {
                place = k;
                break;
            }
        }
    }
    look(host, s, place, -0.4f);
}

// The English name of a location ([0x00711A10 + 4 x location]: "None", then the city places and Super Speedway).
std::wstring englishLocation(WindowHost& host, int32_t location) {
    GameData* data = host.gameData();
    if (location == 0) return L"None";
    const std::vector<std::wstring>& names = data->tables.locationNames;
    // Stock problem: a location past 9 reads past the table; none here.
    return location > 0 && static_cast<size_t>(location) <= names.size() ? names[static_cast<size_t>(location) - 1]
                                                                         : std::wstring();
}

}  // namespace oj::frontend
