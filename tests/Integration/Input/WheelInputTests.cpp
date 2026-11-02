// Integration tests: the fixed build's steering wheels on this machine's DirectInput (Game/Platform/WheelInput,
// InputDevices::startWheels): the reader's thread started before the FF manager's listing, the listing given the
// controllers JuicedConfigExt's way, both let go again. The devices attached change nothing checked.

#include "Framework/Test.h"

#include "Game/Platform/InputDevices.h"
#include "Game/Platform/WheelInput.h"

#include <WheelReader.h>

#include <windows.h>

#include <filesystem>
#include <fstream>

namespace {

using oj::test::check;

}  // namespace

// A controller.ini of the game's force feedback and no [WheelMap]: the reader reads (the default map looks for a
// wheel), the FF manager lists the controllers on a hidden window (its exclusive foreground level refused or not) and
// reads its channels, then stops; the reader's thread ends.
OJ_INTEGRATION_TEST(WheelInput, StartsAndStops) {
    const std::filesystem::path ini = std::filesystem::temp_directory_path() / "oj_wheel_input.ini";
    {
        std::ofstream f(ini, std::ios::binary | std::ios::trunc);
        f << "[Wheel]\r\nffb = game\r\n";
    }
    HWND window = CreateWindowExW(0, L"STATIC", L"OpenJuice wheel test", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);
    check(window != nullptr, "a hidden window");
    oj::WheelInput wheels;
    wheels.load(ini.wstring());
    wheels.start();
    const bool active = wheels.reader().Active();
    {
        oj::InputDevices devices;
        devices.startWheels(window, &wheels);
        for (int i = 0; i < 10; ++i) {
            devices.wheels().read();
            Sleep(10);
        }
        devices.stopWheels();
    }
    wheels.stop();
    check(active && !wheels.reader().Active(), "the reader started, then stopped with its thread");
    if (window != nullptr) DestroyWindow(window);
    std::filesystem::remove(ini);
}
