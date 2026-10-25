#include "Core/Saves/GraphicsConfig.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <initializer_list>

namespace oj {

namespace {

bool among(uint32_t value, std::initializer_list<uint32_t> list) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

bool within(uint32_t value, uint32_t first, uint32_t last) {
    return value >= first && value <= last;
}

// The NVIDIA cards (vendor 0x10DE) by their device id.
int nvidiaClass(uint32_t d) {
    if (among(d, {0x40, 0x41, 0x42, 0x43, 0x45, 0xC8, 0xC9, 0xF0, 0x144, 0x147, 0x148, 0x149, 0x164, 0x167, 0x169,
                  0x16B, 0x16C, 0x16D, 0x211, 0x212, 0x215, 0x228})) {
        return 4;
    }
    if (among(d, {0x301, 0x302, 0x308, 0x309, 0x311, 0x312, 0x313, 0x314, 0x334, 0x338, 0x33F, 0x341, 0x342, 0x343,
                  0x344, 0x345, 0x34E, 0x34F})) {
        return 2;
    }
    if (among(d, {0x20, 0x28, 0x29, 0x2C, 0x2D, 0x49, 0x4E, 0xA0, 0xC0, 0xC1, 0xC2, 0xCC, 0xCD, 0xCE, 0xFD, 0xFE, 0xFF,
                  0x100, 0x101, 0x103, 0x110, 0x111, 0x112, 0x113, 0x145, 0x160, 0x161, 0x17C, 0x17D, 0x1A0, 0x1F0,
                  0x200, 0x201, 0x202, 0x203, 0x250, 0x251, 0x252, 0x253, 0x258, 0x259, 0x25B, 0x280, 0x281, 0x282,
                  0x286, 0x288, 0x289, 0x28C, 0x316, 0x317, 0x32F, 0x347, 0x348}) ||
        within(d, 0x140, 0x143) || within(d, 0x14B, 0x153) || within(d, 0x170, 0x17A) || within(d, 0x181, 0x183) ||
        within(d, 0x185, 0x188) || within(d, 0x18A, 0x18D) || within(d, 0x31A, 0x32D)) {
        return 0;
    }
    // F1 .. F3, F8 .. FC, 162, 163, 165, 16E, 220 .. 222, 330 .. 333, 349, 34B, 34C and the others not listed.
    return 3;
}

void noShaderLimits(GraphicsConfig& c) {
    c.shadows = 1;
    c.cubeSize = 0x40;
    c.reflections = 0;
    c.antialiasing = 0;
    c.sceneHeight = 0;
    c.sceneWidth = 0;
    c.motionBlur = 0;
    c.glow = 0;
    c.depthOfField = 0;
}

}  // namespace

GraphicsConfig& graphicsConfig() {
    static GraphicsConfig config = [] {
        GraphicsConfig c;
        graphicsDefaults(c);
        return c;
    }();
    return config;
}

// Stock problem: the ATI cards' device ids are cases of the vendor's switch, so an ATI card (vendor 0x1002) never
// matches them and is of class 4 (ATI's own case), while another vendor of one of those numbers would. The fixed
// build compares the ATI card's device id; an ATI card not listed stays of class 4, any other vendor's of class 3.
int graphicsClass(uint32_t vendor, uint32_t device) {
    if (vendor == 0x10DE) return nvidiaClass(device);
    if (vendor != 0x1002) return 3;
    const uint32_t v = device;
    if (among(v, {0x4A70, 0x5548, 0x554A, 0x5568, 0x556A, 0x5D4D, 0x5D4F, 0x5D52, 0x5D57, 0x5D6D, 0x5D6F, 0x5D72,
                  0x5D77}) ||
        within(v, 0x4A48, 0x4A4E) || within(v, 0x4A68, 0x4A6E)) {
        return 4;
    }
    if (among(v, {0x3E50, 0x3E54, 0x3E70, 0x3E74, 0x4145, 0x4146, 0x4148, 0x4150, 0x4151, 0x4152, 0x4168, 0x4E44,
                  0x4E45, 0x4E46, 0x4E48, 0x4E49, 0x4E4A, 0x4E50, 0x4E52, 0x4E64, 0x4E65, 0x4E66, 0x4E68, 0x4E69,
                  0x4E6A}) ||
        within(v, 0x4170, 0x4172)) {
        return 2;
    }
    if (among(v, {0x4144, 0x4149, 0x4164, 0x4169, 0x4242, 0x4336, 0x4337, 0x4437, 0x4742, 0x4744, 0x4749, 0x4752,
                  0x4759, 0x475A, 0x4966, 0x4967, 0x496E, 0x496F, 0x4C42, 0x4C49, 0x4C4D, 0x4C50, 0x4C52, 0x4C57,
                  0x4C59, 0x4C5A, 0x4C66, 0x4C67, 0x4C6F, 0x4D46, 0x4D4C, 0x516D, 0x5157, 0x5159, 0x515A, 0x5245,
                  0x5246, 0x524B, 0x524C, 0x5446, 0x544C, 0x5656, 0x5834, 0x5835, 0x5940, 0x5941, 0x5960, 0x5961,
                  0x5964, 0x5C61, 0x5C63, 0x5D44, 0x7834, 0x7835}) ||
        within(v, 0x474C, 0x4750) || within(v, 0x4754, 0x4757) || within(v, 0x4C45, 0x4C47) ||
        within(v, 0x5041, 0x5058) || within(v, 0x5144, 0x5147) || within(v, 0x514C, 0x514F) ||
        within(v, 0x5345, 0x5348) || within(v, 0x534B, 0x534E) || within(v, 0x5452, 0x5455)) {
        return 0;
    }
    return 4;
}

void graphicsDefaults(GraphicsConfig& c) {
    // The table 0x00725798 by class: shadows, reflections, detail (bits), motion blur, glow, depth of field.
    static constexpr uint32_t kRows[5][6] = {
        {1, 0, 0x00000000, 0, 0, 1},
        {2, 1, 0x00000000, 1, 1, 1},
        {2, 0, 0x3E800000, 0, 0, 0},
        {2, 2, 0x3F400000, 1, 1, 1},
        {2, 2, 0x3F7D70A4, 1, 1, 1},
    };
    const auto& row = kRows[graphicsClass(c.vendor, c.device)];
    // 0x005A2490 with 1024 x 768 x 16, 0x400 and 0x200, the texture size 0x40 << [0x0071E5DC] (1).
    c.width = 0x400;
    c.height = 0x300;
    c.depth = 0x10;
    c.windowed = 0;
    c.cubeSize = 0x80;
    c.worldCubeSize = 0x80;
    c.shadowSize = 0x100;
    c.wideBlurWidth = 0x200;
    c.wideBlurHeight = 0x100;
    c.blurWidth = 0x100;
    c.blurHeight = 0x100;
    c.reflectionWidth = 0x100;
    c.reflectionHeight = 0x100;
    c.sceneWidth = 0x400;
    c.sceneHeight = 0x200;
    c.unusedFlags = {1, 1};
    c.antialiasing = 0;
    c.shadows = static_cast<int32_t>(row[0]);
    c.reflections = c.noShaders != 0 ? 0 : static_cast<int32_t>(row[1]);
    c.detail = c.noShaders != 0 ? 0.4f : std::bit_cast<float>(row[2]);
    c.motionBlur = static_cast<int32_t>(row[3]);
    c.glow = static_cast<int32_t>(row[4]);
    c.depthOfField = static_cast<int32_t>(row[5]);
    c.brightness = 0.5f;
    if (c.noShaders != 0 || c.shaders == 0) {
        c.motionBlur = 0;
        c.glow = 0;
        c.depthOfField = 0;
    }
    if (c.noShaders != 0) {
        c.shadows = 1;
        c.cubeSize = 0x40;
        c.reflections = 0;
        c.antialiasing = 0;
        c.sceneHeight = 0;
        c.sceneWidth = 0;
    }
}

void graphicsStart(GraphicsConfig& c, const GraphicsStart& start) {
    c.width = start.width;
    c.height = start.height;
    c.sceneWidth = start.width;
    c.sceneHeight = start.height;
    c.depth = start.depth;
    c.windowed = start.windowed;
    c.antialiasing = start.antialiasing;
    const double detail = 60.0 * static_cast<double>(0.01f);
    c.detail = static_cast<float>(detail);
    c.lodBias = static_cast<float>(detail * static_cast<double>(1.5f) + static_cast<double>(0.66f));
    c.reflections = c.noShaders != 0 ? 0 : 2;
    if (c.noShaders != 0 || c.shaders == 0) {
        c.motionBlur = 0;
        c.glow = 0;
        c.depthOfField = 0;
    }
    if (c.noShaders != 0) {
        c.shadows = 1;
        c.cubeSize = 0x40;
        c.antialiasing = 0;
        c.sceneHeight = 0;
        c.sceneWidth = 0;
    }
}

// The adapter's texture limits (+0x34, +0x38 to powers of two and the largest texture) are not followed.
void graphicsAdapter(GraphicsConfig& c, uint32_t vendor, uint32_t device, bool shaders, bool fullScreenGamma) {
    c.vendor = vendor & 0xFFFF;
    c.device = device & 0xFFFF;
    c.noShaders = shaders ? 0 : 1;
    c.shaders = shaders ? 1 : 0;
    c.gamma = fullScreenGamma && c.windowed == 0 ? 1 : 0;
    c.cubeSize = std::max<uint32_t>(c.cubeSize, 0x40);
    if (c.noShaders != 0) {
        noShaderLimits(c);
        c.cubeSize = 0x10;
    }
}

void graphicsPostEffects(GraphicsConfig& c) {
    const int32_t on = c.noShaders == 0 && c.shaders != 0 ? 1 : 0;
    c.motionBlur = on;
    c.glow = on;
    c.depthOfField = on;
}

void graphicsFromRegCf1(GraphicsConfig& c, const save::RegCf1& file) {
    c.shadows = static_cast<int32_t>(file.values[0]);
    c.reflections = static_cast<int32_t>(file.values[1]);
    c.detail = std::bit_cast<float>(file.values[2]);
    c.motionBlur = static_cast<int32_t>(file.values[3]);
    c.glow = static_cast<int32_t>(file.values[4]);
    c.depthOfField = static_cast<int32_t>(file.values[5]);
    c.brightness = std::bit_cast<float>(file.values[6]);
    if (c.noShaders != 0) noShaderLimits(c);
}

save::RegCf1 graphicsRegCf1(const GraphicsConfig& c) {
    save::RegCf1 file;
    file.values[0] = static_cast<uint32_t>(c.shadows);
    file.values[1] = static_cast<uint32_t>(c.reflections);
    file.values[2] = std::bit_cast<uint32_t>(c.detail);
    file.values[3] = static_cast<uint32_t>(c.motionBlur);
    file.values[4] = static_cast<uint32_t>(c.glow);
    file.values[5] = static_cast<uint32_t>(c.depthOfField);
    file.values[6] = std::bit_cast<uint32_t>(c.brightness);
    return file;
}

float detailFactor(float detail) {
    return static_cast<float>(static_cast<double>(detail) * static_cast<double>(1.5f) + static_cast<double>(0.66f));
}

float glowStrength(int32_t glow) {
    if (glow == 0) return 0.0f;
    if (glow >= 2 && glow <= 100) return static_cast<float>(glow) / 100.0f;
    return 0.5f;
}

int32_t glowSetting(float strength) {
    const auto percent = static_cast<int32_t>(std::lround(std::clamp(strength, 0.0f, 1.0f) * 100.0f));
    return percent == 1 ? 2 : percent;
}

}  // namespace oj
