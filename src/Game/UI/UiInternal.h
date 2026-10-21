#pragma once
// What the interface's element files share of their own helpers: colours, alphas, the properties' words and
// the pictures' samplers.

#include "Game/UI/UiScreen.h"

#include "Game/Media/Movies.h"
#include "Render/Renderer3d.h"

#include <algorithm>
#include <cstdlib>
#include <unordered_map>

namespace oj::ui::detail {

inline constexpr int kAlignCentre = 0;
inline constexpr int kAlignLeft = 1;
inline constexpr int kAlignRight = 2;

inline uint32_t argbOf(const std::vector<std::string>& v, size_t first) {
    auto at = [&](size_t i) { return i < v.size() ? static_cast<uint32_t>(std::atoi(v[i].c_str())) & 0xFF : 0u; };
    return (at(first) << 24) | (at(first + 1) << 16) | (at(first + 2) << 8) | at(first + 3);
}

// 0x0042E560: an alpha byte from 0 .. 1, x 255 cut toward zero.
inline uint8_t alphaByte(float alpha) {
    return static_cast<uint8_t>(static_cast<int>(alpha * 255.0f));
}

inline uint32_t withAlpha(uint32_t argb, uint8_t a) {
    return (argb & 0x00FFFFFF) | (uint32_t(a) << 24);
}

// The alpha of a fading element: the transition's, but never above the colour's own (0x0042E560 and the checks
// after it in the draws).
inline uint32_t fadeAlpha(uint32_t argb, const Transition* t) {
    if (t == nullptr || (t->type != Transition::kFadeIn && t->type != Transition::kGrow)) {
        return argb;
    }
    const auto own = static_cast<uint8_t>(argb >> 24);
    return withAlpha(argb, std::min(alphaByte(t->alpha), own));
}

inline float number(const Property* p, size_t i, float fallback) {
    return p != nullptr && i < p->values.size() ? std::strtof(p->values[i].c_str(), nullptr) : fallback;
}

inline bool flag(const Element& e, const char* property) {
    return e.has(property);
}

// Align LEFT 1, RIGHT 2, CENTRE 0 (0x005AC6F0); without the property CENTRE; another word LEFT.
inline int alignmentOf(const Element& e) {
    const Property* a = e.find("Align");
    if (a == nullptr || a->values.empty()) {
        return kAlignCentre;
    }
    const std::string& v = a->values[0];
    if (_stricmp(v.c_str(), "LEFT") == 0) return kAlignLeft;
    if (_stricmp(v.c_str(), "RIGHT") == 0) return kAlignRight;
    if (_stricmp(v.c_str(), "CENTRE") == 0) return kAlignCentre;
    return kAlignLeft;
}

// The sampler states of the interface's textures (their .ddt headers) by texture.
inline std::unordered_map<render::TextureId, render::SamplerParams>& pictureSamplers() {
    static std::unordered_map<render::TextureId, render::SamplerParams> samplers;
    return samplers;
}

// Quads that end within 2 pixels of the screen's edges are moved onto them (0x00517730, 0x005177C0).
inline float snapEdge(float v, float size) {
    if (v > -2.0f && v < 2.0f) return -0.5f;
    if (v > size - 2.0f && v < size + 2.0f) return size;
    return v;
}

}  // namespace oj::ui::detail
