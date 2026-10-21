#pragma once
// The city map of the windows' MAP3D elements (the map object 0x0045B6B0; docs/spec/02-frontend.md,
// "The city map"): the city's model, the routes' models UI_<route>_GUI over it, the camera on a city
// place.

#include "Game/Frontend/WindowHost.h"
#include "Game/Scene/Scene3d.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>
#include <string>

namespace oj::frontend {

// A MAP3D element's map object (0x0C bytes), kept in a window's record; its element is the
// window's screen's.
class CityMap {
public:
    // The 3d view of the window's MAP3D element while the map is on (none else).
    scene::View3d* view3d(ui::Screen& s) const;
    // 0x0045B6B0
    void start(ui::Screen& s);
    // 0x0045BE00
    void look(WindowHost& host, ui::Screen& s, size_t place, float pitch);
    // 0x0045BC40
    void frame(WindowHost& host, ui::Screen& s);
    // 0x0045BD10
    void clear(ui::Screen& s);
    // 0x0045B720
    void route(WindowHost& host, ui::Screen& s, const std::string& route, bool add);
    // 0x0045BB40
    void addRoute(WindowHost& host, ui::Screen& s, const std::string& route);
    // 0x0045BEC0
    void lookAt(WindowHost& host, ui::Screen& s, const std::wstring& location);
    // The map freed (a window's leave): its element no longer taken.
    void release() { m_on = false; }

private:
    bool m_on = false;        // +0x00: the element
    float m_drop = 0.0f;      // +0x04: the height the model coming down comes from
    int32_t m_dropModel = 1;  // +0x08: that model's place in the element's list (0 the element's own)
};

// The English name of a location ([0x00711A10 + 4 x location]: "None", then the city places and Super
// Speedway).
std::wstring englishLocation(WindowHost& host, int32_t location);

}  // namespace oj::frontend
