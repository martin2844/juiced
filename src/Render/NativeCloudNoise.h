// Native noise generation ported from the pinned JuicedConfigExt CloudPass.cpp.
// Required Notice: Copyright 2026 Sidewinder (https://gitlab.com/sidecr3w/juicedconfig-extended)
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace oj::render::cloud_noise {
    uint32_t Hash(int x, int y, uint32_t seed)
    {
        uint32_t h = static_cast<uint32_t>(x) * 0x8DA6B343u ^ static_cast<uint32_t>(y) * 0xD8163841u ^
                     seed * 0xCB1AB31Fu;
        h ^= h >> 13;
        h *= 0x5BD1E995u;
        h ^= h >> 15;
        return h;
    }

    int Wrap(int i, int period)
    {
        return ((i % period) + period) % period;
    }

    float Smooth(float t)
    {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    // Gradient noise of `period` cells across the tile at (u, v) of 0 .. 1, tiling; about -0.7 .. 0.7.
    float Gradient(float u, float v, int period, uint32_t seed)
    {
        const float x = u * static_cast<float>(period), y = v * static_cast<float>(period);
        const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
        const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
        const auto corner = [&](int cx, int cy, float dx, float dy) {
            const float angle =
                static_cast<float>(Hash(Wrap(cx, period), Wrap(cy, period), seed) & 0xFFFFu) * (6.2831853f / 65536.0f);
            return std::cos(angle) * dx + std::sin(angle) * dy;
        };
        const float sx = Smooth(fx), sy = Smooth(fy);
        const float a = corner(x0, y0, fx, fy) + (corner(x0 + 1, y0, fx - 1.0f, fy) - corner(x0, y0, fx, fy)) * sx;
        const float b = corner(x0, y0 + 1, fx, fy - 1.0f) +
                        (corner(x0 + 1, y0 + 1, fx - 1.0f, fy - 1.0f) - corner(x0, y0 + 1, fx, fy - 1.0f)) * sx;
        return a + (b - a) * sy;
    }

    // `octaves` of it from `period` cells, each twice as fine at half the amplitude, over their sum.
    float Octaves(float u, float v, int period, int octaves, uint32_t seed)
    {
        float sum = 0.0f, amplitude = 0.5f, total = 0.0f;
        for (int o = 0; o < octaves; ++o)
        {
            sum += amplitude * Gradient(u, v, period << o, seed + static_cast<uint32_t>(o));
            total += amplitude;
            amplitude *= 0.5f;
        }
        return sum / total;
    }

    // The distance to the nearest of one point a cell, `period` cells across, over a cell's width (0 .. about 1).
    float Cells(float u, float v, int period, uint32_t seed)
    {
        const float x = u * static_cast<float>(period), y = v * static_cast<float>(period);
        const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
        float best = 4.0f;
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int cx = x0 + dx, cy = y0 + dy;
                const uint32_t h = Hash(Wrap(cx, period), Wrap(cy, period), seed);
                const float px = static_cast<float>(cx) + static_cast<float>(h & 0xFFFFu) / 65536.0f;
                const float py = static_cast<float>(cy) + static_cast<float>(h >> 16) / 65536.0f;
                best = std::min(best, (px - x) * (px - x) + (py - y) * (py - y));
            }
        }
        return std::sqrt(best);
    }

    uint32_t Hash3(int x, int y, int z, uint32_t seed)
    {
        uint32_t h = static_cast<uint32_t>(x) * 0x8DA6B343u ^ static_cast<uint32_t>(y) * 0xD8163841u ^
                     static_cast<uint32_t>(z) * 0x6C8E9CF5u ^ seed * 0xCB1AB31Fu;
        h ^= h >> 13;
        h *= 0x5BD1E995u;
        h ^= h >> 15;
        return h;
    }

    // Gradient noise of `period` cells across the tile at (u, v, w) of 0 .. 1, tiling; about -1 .. 1.
    float Gradient3(float u, float v, float w, int period, uint32_t seed)
    {
        static const float kDirections[12][3] = { { 1, 1, 0 }, { -1, 1, 0 }, { 1, -1, 0 }, { -1, -1, 0 },
                                                  { 1, 0, 1 }, { -1, 0, 1 }, { 1, 0, -1 }, { -1, 0, -1 },
                                                  { 0, 1, 1 }, { 0, -1, 1 }, { 0, 1, -1 }, { 0, -1, -1 } };
        const float p[3] = { u * static_cast<float>(period), v * static_cast<float>(period),
                             w * static_cast<float>(period) };
        int c[3];
        float f[3], s[3];
        for (int k = 0; k < 3; ++k)
        {
            c[k] = static_cast<int>(std::floor(p[k]));
            f[k] = p[k] - static_cast<float>(c[k]);
            s[k] = Smooth(f[k]);
        }
        float corner[8];
        for (int i = 0; i < 8; ++i)
        {
            const int dx = i & 1, dy = (i >> 1) & 1, dz = i >> 2;
            const float* g = kDirections[Hash3(Wrap(c[0] + dx, period), Wrap(c[1] + dy, period),
                                               Wrap(c[2] + dz, period), seed) % 12];
            corner[i] = g[0] * (f[0] - dx) + g[1] * (f[1] - dy) + g[2] * (f[2] - dz);
        }
        const auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
        const float x00 = mix(corner[0], corner[1], s[0]), x10 = mix(corner[2], corner[3], s[0]);
        const float x01 = mix(corner[4], corner[5], s[0]), x11 = mix(corner[6], corner[7], s[0]);
        return mix(mix(x00, x10, s[1]), mix(x01, x11, s[1]), s[2]);
    }

    // The nearness (1 at a point .. 0 a cell's width or more away) to the nearest of one point a cell, `period` cells
    // across the tile, tiling.
    float Cells3(float u, float v, float w, int period, uint32_t seed)
    {
        const float p[3] = { u * static_cast<float>(period), v * static_cast<float>(period),
                             w * static_cast<float>(period) };
        const int c[3] = { static_cast<int>(std::floor(p[0])), static_cast<int>(std::floor(p[1])),
                           static_cast<int>(std::floor(p[2])) };
        float best = 4.0f;
        for (int dz = -1; dz <= 1; ++dz)
        {
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int x = c[0] + dx, y = c[1] + dy, z = c[2] + dz;
                    const uint32_t h = Hash3(Wrap(x, period), Wrap(y, period), Wrap(z, period), seed);
                    const float px = static_cast<float>(x) + static_cast<float>(h & 0x3FFu) / 1024.0f - p[0];
                    const float py = static_cast<float>(y) + static_cast<float>((h >> 10) & 0x3FFu) / 1024.0f - p[1];
                    const float pz = static_cast<float>(z) + static_cast<float>((h >> 20) & 0x3FFu) / 1024.0f - p[2];
                    best = std::min(best, px * px + py * py + pz * pz);
                }
            }
        }
        return std::max(0.0f, 1.0f - std::sqrt(best));
    }

    uint32_t Byte(float value)
    {
        return static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    using Texel = std::array<float, 4>;

    // The noise's channels at texel (x, y), each 0 .. 1 (see Clouds.hlsl).
    Texel NoiseTexel(uint32_t x, uint32_t y)
    {
        constexpr float n = static_cast<float>(256);
        const float u = (static_cast<float>(x) + 0.5f) / n, v = (static_cast<float>(y) + 0.5f) / n;
        const auto unit = [](float value) { return std::clamp(value, 0.0f, 1.0f); };
        return { unit(0.5f + 0.9f * Octaves(u, v, 4, 5, 1)),
                 unit(1.0f - 1.1f * (0.625f * Cells(u, v, 4, 2) + 0.25f * Cells(u, v, 8, 5) +
                                     0.125f * Cells(u, v, 16, 6))),
                 unit(0.5f + 0.9f * Octaves(u, v, 16, 3, 3)), unit(1.0f - 1.2f * Cells(u, v, 32, 4)) };
    }

    // The noise's texels, A8R8G8B8, made once for every device.
    const std::vector<uint32_t>& Baked()
    {
        static std::vector<uint32_t> texels;
        if (!texels.empty())
        {
            return texels;
        }
        constexpr uint32_t n = 256;
        texels.resize(static_cast<size_t>(n) * n);
        for (uint32_t y = 0; y < n; ++y)
        {
            for (uint32_t x = 0; x < n; ++x)
            {
                const Texel t = NoiseTexel(x, y);
                texels[static_cast<size_t>(y) * n + x] =
                    (Byte(t[3]) << 24) | (Byte(t[0]) << 16) | (Byte(t[1]) << 8) | Byte(t[2]);
            }
        }
        return texels;
    }

    // The volume's weather: the noise's shapes (r 0.6, g 0.4) as 16-bit values, made once for every device (eight
    // bits steepened by the weather's sharpness stepped the clouds' density: the user, 2026-10-29, ribbed clouds).
    const std::vector<uint16_t>& BakedWeather()
    {
        static std::vector<uint16_t> texels;
        if (!texels.empty())
        {
            return texels;
        }
        constexpr uint32_t n = 256;
        texels.resize(static_cast<size_t>(n) * n);
        for (uint32_t y = 0; y < n; ++y)
        {
            for (uint32_t x = 0; x < n; ++x)
            {
                const Texel t = NoiseTexel(x, y);
                texels[static_cast<size_t>(y) * n + x] =
                    static_cast<uint16_t>(std::clamp(t[0] * 0.6f + t[1] * 0.4f, 0.0f, 1.0f) * 65535.0f + 0.5f);
            }
        }
        return texels;
    }

    float ShapeOf(uint32_t texel)
    {
        const float r = static_cast<float>((texel >> 16) & 0xFF) / 255.0f;
        const float g = static_cast<float>((texel >> 8) & 0xFF) / 255.0f;
        return r * 0.6f + g * 0.4f;
    }

    // The volume noise's texels (x fastest, then y, then z), made once for every device: r the shapes (three octaves
    // of gradient noise from four cells, rounded by the cells of four, eight and sixteen: billowing), g, b and a the
    // cells of four .. sixteen, eight .. thirty-two and sixteen .. thirty-two (the details).
    const std::vector<Texel>& BakedVolume()
    {
        static std::vector<Texel> texels;
        if (!texels.empty())
        {
            return texels;
        }
        constexpr uint32_t n = 64;
        texels.resize(static_cast<size_t>(n) * n * n);
        for (uint32_t z = 0; z < n; ++z)
        {
            for (uint32_t y = 0; y < n; ++y)
            {
                for (uint32_t x = 0; x < n; ++x)
                {
                    const float u = (static_cast<float>(x) + 0.5f) / n, v = (static_cast<float>(y) + 0.5f) / n,
                                w = (static_cast<float>(z) + 0.5f) / n;
                    const float c4 = Cells3(u, v, w, 4, 11), c8 = Cells3(u, v, w, 8, 12),
                                c16 = Cells3(u, v, w, 16, 13), c32 = Cells3(u, v, w, 32, 14);
                    const float gradient = 0.5f + 0.5f * (0.57f * Gradient3(u, v, w, 4, 21) +
                                                          0.29f * Gradient3(u, v, w, 8, 22) +
                                                          0.14f * Gradient3(u, v, w, 16, 23));
                    const float cells = 0.625f * c4 + 0.25f * c8 + 0.125f * c16;
                    // The gradient noise remapped from (cells - 1) .. 1 to 0 .. 1: its lows cut by the cells.
                    const float shapes = (gradient - (cells - 1.0f)) / (2.0f - cells);
                    texels[(static_cast<size_t>(z) * n + y) * n + x] = {
                        std::clamp(shapes, 0.0f, 1.0f), std::clamp(cells, 0.0f, 1.0f),
                        std::clamp(0.625f * c8 + 0.25f * c16 + 0.125f * c32, 0.0f, 1.0f),
                        std::clamp(0.75f * c16 + 0.25f * c32, 0.0f, 1.0f) };
                }
            }
        }
        return texels;
    }


}
