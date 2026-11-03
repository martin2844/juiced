#include "Core/Input/PadPrompts.h"

#include "Core/Formats/Inflate.h"

#include <atomic>
#include <cstdint>

namespace oj::input {

namespace {

// Xelu's Free Controllers & Keyboard Prompts (CC0), Xbox Series X.
#include "Core/Input/PadPromptPictures.inc"

struct Prompt {
    const char* key;
    const char* pad;
    const unsigned char* packed;
    size_t size;
};
constexpr Prompt kPrompts[] = {
    {"ui_selecta", "oj_pad_a", kXboxA, sizeof kXboxA},
    {"ui_back", "oj_pad_b", kXboxB, sizeof kXboxB},
    {"ui_selectb", "oj_pad_y", kXboxY, sizeof kXboxY},
    {"ui_selectc", "oj_pad_x", kXboxX, sizeof kXboxX},
};

std::atomic<bool> g_pad{false};

}  // namespace

void usedPad(bool pad) { g_pad = pad; }

const char* promptTexture(const std::string& name) {
    if (g_pad) {
        for (const Prompt& p : kPrompts)
            if (name == p.key) return p.pad;
    }
    return nullptr;
}

std::optional<std::vector<std::byte>> promptBytes(const std::string& name) {
    for (const Prompt& p : kPrompts) {
        if (name != p.pad) continue;
        std::vector<std::byte> bytes;
        if (!inflateZlib(p.packed, p.size, bytes)) return std::nullopt;
        return bytes;
    }
    return std::nullopt;
}

}  // namespace oj::input
