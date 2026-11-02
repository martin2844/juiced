#include "Game/Platform/Screen.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace oj::screen {

namespace {

bool sameName(const std::wstring& a, const std::wstring& b) {
    return a.size() == b.size() && _wcsicmp(a.c_str(), b.c_str()) == 0;
}

}  // namespace

uint32_t monitorAdapter(const std::vector<std::wstring>& reported, const std::vector<std::wstring>& desktop,
                        const std::wstring& monitor, uint32_t gameAdapter) {
    const auto count = static_cast<uint32_t>(reported.size());
    if (!monitor.empty()) {
        bool distinct = true;
        for (size_t a = 0; a < reported.size() && distinct; ++a) {
            for (size_t b = a + 1; b < reported.size() && distinct; ++b) distinct = !sameName(reported[a], reported[b]);
        }
        const std::vector<std::wstring>& names = distinct ? reported : desktop;
        for (uint32_t adapter = 0; adapter < count && adapter < names.size(); ++adapter) {
            if (sameName(names[adapter], monitor)) return adapter;
        }
    }
    return gameAdapter < count ? gameAdapter : 0;
}

uint32_t fullScreenRate(const std::vector<DisplayMode>& modes, uint32_t width, uint32_t height, uint32_t wanted) {
    uint32_t highest = 0;
    bool exact = false;
    for (const DisplayMode& mode : modes) {
        if (mode.width != width || mode.height != height) continue;
        highest = std::max(highest, mode.refreshRate);
        exact = exact || mode.refreshRate == wanted;
    }
    if (highest == 0) return 0;
    return exact && wanted != 0 ? wanted : highest;
}

Size windowClient(int32_t workWidth, int32_t workHeight, int32_t frameWidth, int32_t frameHeight, uint32_t width,
                  uint32_t height) {
    if (width == 0 || height == 0) return {64, 48};
    const double fit = std::min(1.0, std::min(static_cast<double>(workWidth - frameWidth) / width,
                                              static_cast<double>(workHeight - frameHeight) / height));
    return {std::max<int32_t>(64, static_cast<int32_t>(std::lround(width * fit))),
            std::max<int32_t>(48, static_cast<int32_t>(std::lround(height * fit)))};
}

}  // namespace oj::screen
