// custmenu: the customising menu over carsel.

#include "Game/Frontend/Garage/Customise/CustMenu.h"

#include "Core/Data/GameTexts.h"
#include "Game/Frontend/WindowTools.h"

#include <algorithm>

namespace oj::frontend {

namespace {

constexpr oj::Vec4 kCameraSide{-500.0f, 100.0f, 0.0f, 0.0f};           // 0x00720408
constexpr oj::Vec4 kCameraEngine{130.0f, 100.0f, 350.0f, 0.0f};        // 0x00720488
constexpr oj::Vec4 kCameraWheel{-200.0f, 30.0f, 230.0f, 0.0f};         // 0x00720498
constexpr oj::Vec4 kCameraExhaust{40.0f, 50.0f, -460.0f, 0.0f};        // 0x007204B8

}  // namespace

// 0x00439050: the base enter; the camera to the car's side (0x004362C0 with 0x00720408); +0x0C 0, +0x10 carsel's
// FRAME_CAR3D, +0x14 0; carsel's view in the map mode (0x00436010).
void CustMenu::enter(GarageHost& host, ui::Screen& s) {
    Garage& garage = host.garage();
    host.baseEnter(s, "custmenu");
    m_returning = 0;
    if (garage.view) {
        garage.view->cameraTo(kCameraSide);
        garage.view->orbit();
    }
}

// 0x004390A0: [0x0075F108] (the clouds' mode) 2; back from a window (+0x14) menu shown with its transition and the
// camera to the side. SELECT on Menu's item (CLICK): tuning tuningmenu, styling stylingmenu, paint the car's paint
// kept (0x00767D90 .. 0x00767D98), 0x00728AF8 .. 0x00728B00 0, [0x00715C24] 1 and colourmenu (each in place of
// this window, mode 7). BACK (CLICK): the window closed and the camera to the garage's place (0x007203E8). The chassis
// lowered by MAXDROPHEIGHT x the car's +0x58.
void CustMenu::update(GarageHost& host, ui::Screen& s, uint32_t event) {
    Garage& garage = host.garage();
    if (m_returning != 0) {
        s.show("menu", true);
        m_returning = 0;
        if (garage.view) garage.view->cameraTo(kCameraSide);
    }
    if (event == input::kSelect) {
        if (s.find("Menu") >= 0 && s.menuSize("Menu") != 0) {
            host.sound("CLICK");
            const std::string command = s.menuItemCommand("Menu", s.selectedMenuItem("Menu"));
            if (_stricmp(command.c_str(), "tuning") == 0) {
                host.request("tuningmenu", SwitchMode::Swap);
            } else if (_stricmp(command.c_str(), "styling") == 0) {
                host.request("stylingmenu", SwitchMode::Swap);
            } else if (_stricmp(command.c_str(), "paint") == 0) {
                if (const Car* car = garage.car(host); car != nullptr) garage.paintKept = {car->base, car->metallic, car->pearl};
                garage.paintCharged = {};
                garage.paintReset = 1;
                host.request("colourmenu", SwitchMode::Swap);
            }
        }
    } else if (event == input::kBack) {
        host.sound("CLICK");
        host.request("", SwitchMode::Pop);
        if (garage.view) garage.view->cameraTo(kCameraGarage);
    }
    if (Car* car = garage.car(host); car != nullptr && garage.view) garage.view->dropChassis(*car);
}

}  // namespace oj::frontend
