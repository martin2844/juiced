#pragma once
// The fixed build's screen modes (docs/fixes.md Added), JuicedConfigExt's renderer as the registry gives it
// (Game/Platform/Settings: DisplayMode, Monitor, RefreshRate): what they decide apart from Windows and Direct3D: the
// adapter of the monitor chosen, the full screen mode's refresh rate, the window's client size.

#include <cstdint>
#include <string>
#include <vector>

namespace oj::screen {

// DisplayMode: an exclusive full screen; a window without a frame over the whole monitor (the picture in it by
// BorderlessScale); a window with a frame.
enum class Mode : uint32_t { FullScreen = 0, Borderless = 1, Window = 2 };

// The settings' DisplayMode (Settings reads 0 .. 2).
inline Mode modeOf(uint32_t displayMode) {
    return displayMode == 0 ? Mode::FullScreen : displayMode == 1 ? Mode::Borderless : Mode::Window;
}

// The adapter of the monitor named (its GDI name, \\.\DISPLAY2): the adapter whose monitor it is (`reported`, each
// adapter's monitor as Direct3D answers it); when the adapters answer the same monitor (a wrapper: DXVK), the one of
// its place among the desktop's monitors (`desktop`, the primary first). No name, no such monitor: `gameAdapter`
// (the settings' Adapter, Platform), the first when it is past the adapters.
uint32_t monitorAdapter(const std::vector<std::wstring>& reported, const std::vector<std::wstring>& desktop,
                        const std::wstring& monitor, uint32_t gameAdapter);

struct DisplayMode {
    uint32_t width = 0, height = 0, refreshRate = 0;
};

// The full screen mode's refresh rate: the one asked for (RefreshRate) when the adapter has it at the resolution, else
// the highest at the resolution; 0 when the adapter has no mode of the resolution (then the window without a frame).
uint32_t fullScreenRate(const std::vector<DisplayMode>& modes, uint32_t width, uint32_t height, uint32_t wanted);

struct Size {
    int32_t width = 0, height = 0;
};

// The client of the window with a frame: the resolution, smaller (its shape kept, 64 x 48 at least) when it and the
// frame (frameWidth x frameHeight, AdjustWindowRect's) do not fit the monitor's work area.
Size windowClient(int32_t workWidth, int32_t workHeight, int32_t frameWidth, int32_t frameHeight, uint32_t width,
                  uint32_t height);

}  // namespace oj::screen
