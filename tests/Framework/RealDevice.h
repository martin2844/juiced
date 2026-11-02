#pragma once
// A Direct3D 9 device of the renderer (Render/D3D9) on a window never shown, for the integration tests of the fixed
// build's renderer; and the bitmap a capture writes, read back.

#include "Render/RenderDevice.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace oj::test {

// A window of the client given, never shown.
class HiddenWindow {
public:
    HiddenWindow(int width, int height) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = kClass;
        RegisterClassA(&wc);
        m_window = CreateWindowExA(0, kClass, "", WS_POPUP, 0, 0, width, height, nullptr, nullptr, wc.hInstance,
                                   nullptr);
    }
    ~HiddenWindow() {
        if (m_window != nullptr) DestroyWindow(m_window);
        UnregisterClassA(kClass, GetModuleHandleA(nullptr));
    }
    HiddenWindow(const HiddenWindow&) = delete;
    HiddenWindow& operator=(const HiddenWindow&) = delete;
    HWND get() const { return m_window; }

private:
    static constexpr const char* kClass = "oj-tests-device";
    HWND m_window = nullptr;
};

// A device's desc for the hidden window: windowed, 32 bits, no antialiasing, not pure (as the fixed build's passes
// need).
inline render::DeviceDesc realDesc(HWND window) {
    render::DeviceDesc desc;
    desc.window = window;
    desc.windowed = true;
    desc.depthBits = 32;
    desc.antialiasing = 0;
    desc.pureDevice = false;
    return desc;
}

// A capture's bitmap (24 bits, bottom-up): its size and the pixel (x, y from the top) as 0xRRGGBB.
struct Bitmap {
    int32_t width = 0, height = 0;
    std::vector<uint8_t> bytes;
    uint32_t pixel(int32_t x, int32_t y) const {
        const size_t row = (static_cast<size_t>(width) * 3 + 3) & ~size_t(3);
        const size_t at = 54 + static_cast<size_t>(height - 1 - y) * row + static_cast<size_t>(x) * 3;
        if (at + 2 >= bytes.size()) return 0;
        return (uint32_t(bytes[at + 2]) << 16) | (uint32_t(bytes[at + 1]) << 8) | bytes[at];
    }
};

inline Bitmap readBitmap(const std::filesystem::path& file) {
    Bitmap bmp;
    std::ifstream in(file, std::ios::binary);
    bmp.bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (bmp.bytes.size() < 54) return bmp;
    std::memcpy(&bmp.width, bmp.bytes.data() + 18, 4);
    std::memcpy(&bmp.height, bmp.bytes.data() + 22, 4);
    return bmp;
}

// The frame `draw` draws, captured before the present (an empty bitmap when the frame or the capture fails).
template <class Draw>
Bitmap captureFrame(render::IRenderDevice& device, Draw draw) {
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "oj-tests-frame.bmp";
    std::filesystem::remove(file);
    if (!device.beginFrame()) return {};
    draw();
    const bool captured = device.capture(file.wstring());
    device.endFrame();
    Bitmap bmp = captured ? readBitmap(file) : Bitmap{};
    std::filesystem::remove(file);
    return bmp;
}

}  // namespace oj::test
