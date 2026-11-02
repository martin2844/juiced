// Unit tests: the fixed build's screen modes (Game/Platform/Screen.h): the adapter of the monitor chosen, the full
// screen mode's refresh rate, the window's client.

#include "Framework/Test.h"

#include "Game/Platform/Screen.h"

namespace {

using oj::test::check;
namespace screen = oj::screen;

}  // namespace

// The adapter whose monitor is the one named; none named or not there: the settings' adapter (the first past them).
OJ_UNIT_TEST(Screen, MonitorAdapter) {
    const std::vector<std::wstring> reported = {L"\\\\.\\DISPLAY1", L"\\\\.\\DISPLAY2"};
    const std::vector<std::wstring> desktop = {L"\\\\.\\DISPLAY1", L"\\\\.\\DISPLAY2"};
    check(screen::monitorAdapter(reported, desktop, L"\\\\.\\DISPLAY2", 0) == 1 &&
              screen::monitorAdapter(reported, desktop, L"\\\\.\\display2", 0) == 1,
          "DISPLAY2: adapter 1, the case aside");
    check(screen::monitorAdapter(reported, desktop, L"", 1) == 1 &&
              screen::monitorAdapter(reported, desktop, L"\\\\.\\DISPLAY9", 1) == 1 &&
              screen::monitorAdapter(reported, desktop, L"", 5) == 0,
          "no name or no such monitor: the settings' adapter; past the adapters the first");
}

// A wrapper answering the same monitor for every adapter (DXVK): the adapter of the monitor's place on the desktop.
OJ_UNIT_TEST(Screen, MonitorAdapterWrapper) {
    const std::vector<std::wstring> same = {L"\\\\.\\DISPLAY1", L"\\\\.\\DISPLAY1"};
    const std::vector<std::wstring> desktop = {L"\\\\.\\DISPLAY2", L"\\\\.\\DISPLAY1"};
    check(screen::monitorAdapter(same, desktop, L"\\\\.\\DISPLAY1", 0) == 1 &&
              screen::monitorAdapter(same, desktop, L"\\\\.\\DISPLAY2", 1) == 0,
          "by the desktop's order (the primary DISPLAY2 first)");
}

// The rate asked for when the resolution has it, else its highest; no mode of the resolution: 0.
OJ_UNIT_TEST(Screen, FullScreenRate) {
    const std::vector<screen::DisplayMode> modes = {
        {1920, 1080, 60}, {1920, 1080, 144}, {1920, 1080, 120}, {1280, 720, 240}};
    check(screen::fullScreenRate(modes, 1920, 1080, 120) == 120, "120 asked, there: 120");
    check(screen::fullScreenRate(modes, 1920, 1080, 0) == 144 && screen::fullScreenRate(modes, 1920, 1080, 75) == 144,
          "none asked or 75 not there: the highest, 144");
    check(screen::fullScreenRate(modes, 1024, 768, 60) == 0, "1024 x 768 not there: 0");
}

// The resolution when it and the frame fit the work area; smaller, its shape kept, when not.
OJ_UNIT_TEST(Screen, WindowClient) {
    const screen::Size fits = screen::windowClient(1920, 1040, 16, 39, 1280, 720);
    check(fits.width == 1280 && fits.height == 720, "1280 x 720 in 1920 x 1040: as it is");
    const screen::Size big = screen::windowClient(1920, 1040, 16, 39, 1920, 1080);
    check(big.height == 1001 && big.width == 1780, "1920 x 1080 in 1920 x 1040: 1780 x 1001");
    const screen::Size tiny = screen::windowClient(50, 60, 16, 39, 1920, 1080);
    check(tiny.width == 64 && tiny.height == 48, "at least 64 x 48");
}
