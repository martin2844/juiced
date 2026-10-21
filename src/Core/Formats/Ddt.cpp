#include "Core/Formats/Ddt.h"

#include <algorithm>
#include <cstring>

namespace oj {

namespace {

bool knownFormat(uint32_t f) {
    return isDxt(f) || f == Ddt::kA8R8G8B8 || f == Ddt::kX8R8G8B8;
}

uint32_t argb(uint32_t a, uint32_t r, uint32_t g, uint32_t b) {
    return a << 24 | r << 16 | g << 8 | b;
}

// A 5:6:5 colour as 8-bit channels (the high bits repeated in the low ones).
void expand565(uint32_t c, uint32_t rgb[3]) {
    const uint32_t r = c >> 11, g = (c >> 5) & 0x3F, b = c & 0x1F;
    rgb[0] = (r << 3) | (r >> 2);
    rgb[1] = (g << 2) | (g >> 4);
    rgb[2] = (b << 3) | (b >> 2);
}

// The colour part of a block: 16 pixels with alpha 0xFF (0 for DXT1's transparent index).
void decodeColours(const uint8_t* block, bool dxt1, uint32_t out[16]) {
    const uint32_t c0 = block[0] | block[1] << 8;
    const uint32_t c1 = block[2] | block[3] << 8;
    uint32_t a[3], b[3];
    expand565(c0, a);
    expand565(c1, b);
    uint32_t colours[4] = {argb(0xFF, a[0], a[1], a[2]), argb(0xFF, b[0], b[1], b[2])};
    if (!dxt1 || c0 > c1) {
        colours[2] = argb(0xFF, (2 * a[0] + b[0] + 1) / 3, (2 * a[1] + b[1] + 1) / 3, (2 * a[2] + b[2] + 1) / 3);
        colours[3] = argb(0xFF, (a[0] + 2 * b[0] + 1) / 3, (a[1] + 2 * b[1] + 1) / 3, (a[2] + 2 * b[2] + 1) / 3);
    } else {
        colours[2] = argb(0xFF, (a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2);
        colours[3] = 0;
    }
    const uint32_t bits = block[4] | block[5] << 8 | block[6] << 16 | static_cast<uint32_t>(block[7]) << 24;
    for (int i = 0; i < 16; ++i) out[i] = colours[(bits >> (2 * i)) & 3];
}

// The alpha part of a DXT2 / DXT3 block: 4 bits a pixel.
void decodeExplicitAlpha(const uint8_t* block, uint32_t out[16]) {
    for (int i = 0; i < 16; ++i) {
        const uint32_t a = (block[i / 2] >> (4 * (i & 1))) & 0xF;
        out[i] = (out[i] & 0x00FFFFFF) | (a * 17) << 24;
    }
}

// The alpha part of a DXT4 / DXT5 block: two ends and 3-bit indices.
void decodeInterpolatedAlpha(const uint8_t* block, uint32_t out[16]) {
    const uint32_t a0 = block[0], a1 = block[1];
    uint32_t alphas[8] = {a0, a1};
    if (a0 > a1) {
        for (uint32_t i = 1; i < 7; ++i) alphas[i + 1] = ((7 - i) * a0 + i * a1 + 3) / 7;
    } else {
        for (uint32_t i = 1; i < 5; ++i) alphas[i + 1] = ((5 - i) * a0 + i * a1 + 2) / 5;
        alphas[6] = 0;
        alphas[7] = 255;
    }
    uint64_t bits = 0;
    for (int i = 0; i < 6; ++i) bits |= static_cast<uint64_t>(block[2 + i]) << (8 * i);
    for (int i = 0; i < 16; ++i) out[i] = (out[i] & 0x00FFFFFF) | alphas[(bits >> (3 * i)) & 7] << 24;
}

uint32_t toByte(float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    return static_cast<uint32_t>(v * 255.0f + 0.5f);
}

// The next level down: the box average of four pixels in floats, the colours of a premultiplied format divided by
// alpha before and multiplied by the average alpha after.
std::vector<uint32_t> boxLevel(const std::vector<uint32_t>& above, uint32_t aw, uint32_t ah, bool premultiplied) {
    const uint32_t w = std::max(1u, aw / 2), h = std::max(1u, ah / 2);
    std::vector<uint32_t> out(size_t(w) * h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float sum[4] = {};
            for (uint32_t k = 0; k < 4; ++k) {
                const uint32_t sx = std::min(x * 2 + (k & 1), aw - 1);
                const uint32_t sy = std::min(y * 2 + k / 2, ah - 1);
                const uint32_t p = above[size_t(sy) * aw + sx];
                const float a = static_cast<float>(p >> 24) / 255.0f;
                for (int c = 0; c < 3; ++c) {
                    float v = static_cast<float>((p >> (16 - 8 * c)) & 0xFF) / 255.0f;
                    if (premultiplied && a > 0.0f) v /= a;
                    sum[c] += v;
                }
                sum[3] += a;
            }
            for (float& v : sum) v *= 0.25f;
            if (premultiplied) {
                for (int c = 0; c < 3; ++c) sum[c] *= sum[3];
            }
            out[size_t(y) * w + x] = argb(toByte(sum[3]), toByte(sum[0]), toByte(sum[1]), toByte(sum[2]));
        }
    }
    return out;
}

}  // namespace

bool isDxt(uint32_t f) {
    return f == Ddt::kDxt1 || f == Ddt::kDxt2 || f == Ddt::kDxt3 || f == Ddt::kDxt4 || f == Ddt::kDxt5;
}

size_t levelBytes(uint32_t format, uint32_t width, uint32_t height) {
    if (isDxt(format)) {
        return size_t((width + 3) / 4) * ((height + 3) / 4) * (format == Ddt::kDxt1 ? 8 : 16);
    }
    return size_t(width) * height * 4;
}

std::vector<uint32_t> decodeLevel(uint32_t format, uint32_t width, uint32_t height, const uint8_t* data) {
    std::vector<uint32_t> out(size_t(width) * height);
    if (!isDxt(format)) {
        std::memcpy(out.data(), data, out.size() * 4);
        if (format == Ddt::kX8R8G8B8) {
            for (uint32_t& p : out) p |= 0xFF000000u;
        }
        return out;
    }
    const size_t blockBytes = format == Ddt::kDxt1 ? 8 : 16;
    const uint8_t* block = data;
    for (uint32_t by = 0; by < (height + 3) / 4; ++by) {
        for (uint32_t bx = 0; bx < (width + 3) / 4; ++bx, block += blockBytes) {
            uint32_t pixels[16];
            if (format == Ddt::kDxt1) {
                decodeColours(block, true, pixels);
            } else {
                decodeColours(block + 8, false, pixels);
                if (format == Ddt::kDxt2 || format == Ddt::kDxt3) {
                    decodeExplicitAlpha(block, pixels);
                } else {
                    decodeInterpolatedAlpha(block, pixels);
                }
            }
            for (uint32_t i = 0; i < 16; ++i) {
                const uint32_t x = bx * 4 + (i & 3), y = by * 4 + i / 4;
                if (x < width && y < height) out[size_t(y) * width + x] = pixels[i];
            }
        }
    }
    return out;
}

std::vector<uint32_t> fullMipChain(const Ddt& t, uint32_t& levels) {
    levels = 1;
    for (uint32_t s = std::max(t.width, t.height); s > 1; s >>= 1) ++levels;
    const bool premultiplied = t.format == Ddt::kDxt2 || t.format == Ddt::kDxt4;
    std::vector<uint32_t> out;
    size_t at = 0;
    for (uint32_t face = 0; face < t.faces; ++face) {
        std::vector<uint32_t> level;
        for (uint32_t l = 0; l < std::max(levels, t.mipLevels); ++l) {
            const uint32_t w = std::max(1u, t.width >> l), h = std::max(1u, t.height >> l);
            if (l < t.mipLevels) {
                const size_t bytes = levelBytes(t.format, w, h);
                if (at + bytes > t.pixels.size()) return {};
                if (l < levels) level = decodeLevel(t.format, w, h, t.pixels.data() + at);
                at += bytes;
            } else {
                level = boxLevel(level, std::max(1u, t.width >> (l - 1)), std::max(1u, t.height >> (l - 1)),
                                 premultiplied);
            }
            if (l < levels) out.insert(out.end(), level.begin(), level.end());
        }
    }
    return out;
}

bool Ddt::parse(const std::vector<std::byte>& file) {
    constexpr size_t kHeader = 44;
    if (file.size() < kHeader) {
        return false;
    }
    uint32_t h[11];
    std::memcpy(h, file.data(), kHeader);
    format = h[0];
    width = h[1];
    height = h[2];
    mipLevels = h[3];
    minFilter = h[4];
    magFilter = h[5];
    mipFilter = h[6];
    addressU = h[7];
    addressV = h[8];
    std::memcpy(&mipLodBias, &h[9], sizeof(float));
    maxAnisotropy = h[10];
    const auto* begin = reinterpret_cast<const uint8_t*>(file.data()) + kHeader;
    pixels.assign(begin, begin + (file.size() - kHeader));
    return width != 0 && height != 0 &&
           (format == kDxt1 || format == kDxt3 || format == kDxt5 || format == kA8R8G8B8 || format == kX8R8G8B8);
}

bool Ddt::parseDds(const std::vector<std::byte>& file) {
    constexpr size_t kHeader = 128;
    if (file.size() < kHeader || std::memcmp(file.data(), "DDS ", 4) != 0) {
        return false;
    }
    uint32_t h[32];
    std::memcpy(h, file.data(), kHeader);
    height = h[3];
    width = h[4];
    mipLevels = h[7] != 0 ? h[7] : 1;
    const uint32_t pixelFlags = h[20];
    const uint32_t fourCc = h[21];
    const uint32_t bits = h[22];
    const uint32_t alphaMask = h[26];
    if ((pixelFlags & 4) != 0) {
        format = fourCc;
    } else if ((pixelFlags & 0x40) != 0 && bits == 32) {
        format = (pixelFlags & 1) != 0 && alphaMask != 0 ? kA8R8G8B8 : kX8R8G8B8;
    } else {
        return false;
    }
    minFilter = magFilter = mipFilter = 0;
    addressU = addressV = 0;
    mipLodBias = 0.0f;
    maxAnisotropy = 0;
    // DDSCAPS2_CUBEMAP with all six faces.
    faces = (h[28] & 0xFE00) == 0xFE00 ? 6 : 1;
    const auto* begin = reinterpret_cast<const uint8_t*>(file.data()) + kHeader;
    pixels.assign(begin, begin + (file.size() - kHeader));
    return width != 0 && height != 0 && knownFormat(format);
}

}  // namespace oj
