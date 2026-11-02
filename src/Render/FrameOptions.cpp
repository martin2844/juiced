#include "Render/FrameOptions.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <string>
#include <utility>

namespace oj::render {

TextureQuality TextureQuality::make(uint32_t textureFilter, uint32_t lodBias, bool anisotropicCaps,
                                    uint32_t maxAnisotropy) {
    TextureQuality q;
    q.filter = textureFilter != 0;
    const uint32_t wanted = textureFilter >= 2 && textureFilter <= 5 ? 1u << (textureFilter - 1) : 0u;
    q.anisotropy = anisotropicCaps ? std::min(wanted, maxAnisotropy) : 0u;
    if (q.anisotropy < 2) q.anisotropy = 0;
    static constexpr std::array<float, 5> kBias = {0.0f, -1.0f, -0.5f, 0.0f, 0.5f};
    q.bias = lodBias >= 1 && lodBias <= 4;
    q.lodBias = q.bias ? kBias[lodBias] : 0.0f;
    return q;
}

uint32_t TextureQuality::adjust(SamplerState type, uint32_t value) const {
    switch (type) {
    case SamplerState::MinFilter:
        if (filter && (value == TextureFilter::Point || value == TextureFilter::Linear)) {
            return anisotropy != 0 ? TextureFilter::Anisotropic : TextureFilter::Linear;
        }
        return value;
    case SamplerState::MagFilter:
        return filter && value == TextureFilter::Point ? TextureFilter::Linear : value;
    case SamplerState::MipFilter:
        return filter && (value == TextureFilter::Point || value == TextureFilter::None) ? TextureFilter::Linear
                                                                                         : value;
    case SamplerState::MaxAnisotropy:
        return filter && anisotropy != 0 ? anisotropy : value;
    case SamplerState::MipMapLodBias:
        return bias ? std::bit_cast<uint32_t>(lodBias) : value;
    default:
        return value;
    }
}

void bleedTransparent(uint32_t* texels, uint32_t width, uint32_t height) {
    const size_t count = static_cast<size_t>(width) * height;
    std::vector<uint8_t> seen(count);
    for (size_t i = 0; i < count; ++i) seen[i] = (texels[i] >> 24) != 0 ? 1 : 0;
    std::vector<std::pair<size_t, uint32_t>> coloured;
    for (int pass = 0; pass < 16; ++pass) {
        coloured.clear();
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                const size_t i = static_cast<size_t>(y) * width + x;
                if (seen[i] != 0) continue;
                uint32_t r = 0, g = 0, b = 0, n = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int64_t nx = static_cast<int64_t>(x) + dx, ny = static_cast<int64_t>(y) + dy;
                        if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
                        const size_t j = static_cast<size_t>(ny) * width + static_cast<size_t>(nx);
                        if (seen[j] == 0) continue;
                        r += (texels[j] >> 16) & 0xFF;
                        g += (texels[j] >> 8) & 0xFF;
                        b += texels[j] & 0xFF;
                        ++n;
                    }
                }
                if (n == 0) continue;
                coloured.push_back({i, (texels[i] & 0xFF000000u) | ((r / n) << 16) | ((g / n) << 8) | (b / n)});
            }
        }
        if (coloured.empty()) break;
        for (const auto& [i, c] : coloured) {
            texels[i] = c;
            seen[i] = 1;
        }
    }
}

std::vector<FpsRect> fpsCounterRects(uint32_t value, uint32_t backBufferHeight) {
    // The segments a .. g (bit 0 the top, bit 6 the middle) of 0 .. 9.
    static constexpr std::array<uint8_t, 10> kDigits = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    constexpr uint32_t kBackground = 0xFF101010;
    constexpr uint32_t kSegment = 0xFFFFC800;
    const std::string text = std::to_string(std::min<uint32_t>(value, 9999));
    const int32_t u = std::max<int32_t>(1, static_cast<int32_t>(backBufferHeight / 360));
    const int32_t x0 = 4 * u, y0 = 4 * u;
    const int32_t w = 4 * u, h = 7 * u, t = u, gap = 2 * u;
    const auto length = static_cast<int32_t>(text.size());
    std::vector<FpsRect> rects;
    rects.push_back({x0 - 2 * u, y0 - 2 * u, x0 + length * (w + gap) - gap + 2 * u, y0 + h + 2 * u, kBackground});
    for (int32_t i = 0; i < length; ++i) {
        const uint8_t mask = kDigits[static_cast<size_t>(text[static_cast<size_t>(i)] - '0')];
        const int32_t x = x0 + i * (w + gap), y = y0;
        const std::array<FpsRect, 7> segments = {{
            {x, y, x + w, y + t, kSegment},                                    // a
            {x + w - t, y, x + w, y + h / 2 + 1, kSegment},                    // b
            {x + w - t, y + h / 2, x + w, y + h, kSegment},                    // c
            {x, y + h - t, x + w, y + h, kSegment},                            // d
            {x, y + h / 2, x + t, y + h, kSegment},                            // e
            {x, y, x + t, y + h / 2 + 1, kSegment},                            // f
            {x, y + h / 2 - t / 2, x + w, y + h / 2 - t / 2 + t, kSegment},    // g
        }};
        for (size_t s = 0; s < segments.size(); ++s) {
            if ((mask & (1u << s)) != 0) rects.push_back(segments[s]);
        }
    }
    return rects;
}

void FrameRateCounter::frame(double seconds) {
    if (m_start < 0.0) {
        m_start = seconds;
        m_frames = 0;
        return;
    }
    ++m_frames;
    const double elapsed = seconds - m_start;
    if (elapsed >= 0.5) {
        m_shown = std::min<uint32_t>(static_cast<uint32_t>(m_frames / elapsed + 0.5), 9999);
        m_start = seconds;
        m_frames = 0;
    }
}

uint32_t frameRateLimit(bool vsync, uint32_t maxFps, uint32_t refreshRate) {
    if (!vsync && maxFps > 0) return std::min<uint32_t>(maxFps, 1000);
    return refreshRate > 60 ? std::min<uint32_t>(refreshRate, 1000) : 0;
}

double frameInterval(uint32_t limit, bool presentIntervalOne) {
    if (presentIntervalOne) return 0.0;
    return limit != 0 ? 1.0 / (static_cast<double>(limit) + 0.1) : 1.0 / 60.1;
}

int64_t waitFrame(int64_t last, double interval) {
    LARGE_INTEGER frequency{}, now{};
    QueryPerformanceFrequency(&frequency);
    do {
        Sleep(0);
        QueryPerformanceCounter(&now);
    } while (static_cast<double>(now.QuadPart - last) / static_cast<double>(frequency.QuadPart) < interval);
    return now.QuadPart;
}

PictureRect placePicture(uint32_t windowWidth, uint32_t windowHeight, uint32_t width, uint32_t height, bool stretch) {
    auto w = static_cast<int32_t>(windowWidth);
    auto h = static_cast<int32_t>(windowHeight);
    if (!stretch && width != 0 && height != 0) {
        const double fit = std::min(static_cast<double>(windowWidth) / width, static_cast<double>(windowHeight) / height);
        w = std::min(w, static_cast<int32_t>(std::lround(width * fit)));
        h = std::min(h, static_cast<int32_t>(std::lround(height * fit)));
    }
    const int32_t x = (static_cast<int32_t>(windowWidth) - w) / 2;
    const int32_t y = (static_cast<int32_t>(windowHeight) - h) / 2;
    return {x, y, x + w, y + h};
}

}  // namespace oj::render
