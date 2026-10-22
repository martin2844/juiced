#ifdef OJ_MACOS
#include "Mods/Creative.h"
#endif
// Boxes and lines: an element's colours, picture and texture coordinates, and its quad drawn.

#include "Game/UI/UiScreen.h"
#include "Game/UI/UiInternal.h"

#include "Core/Base/GameFiles.h"
#include "Core/Base/Log.h"
#include "Core/Base/X87.h"
#include "Core/Formats/Ddt.h"

#include <filesystem>

namespace oj::ui {

using render::SamplerState;
using render::TextureAddress;

using namespace detail;

namespace {

render::PixelFormat pixelFormat(uint32_t format) {
    switch (format) {
    case Ddt::kDxt1: return render::PixelFormat::DXT1;
    case Ddt::kDxt3: return render::PixelFormat::DXT3;
    case Ddt::kDxt5: return render::PixelFormat::DXT5;
    case Ddt::kX8R8G8B8: return render::PixelFormat::X8R8G8B8;
    default: return render::PixelFormat::A8R8G8B8;
    }
}

}  // namespace

void drawPicture(render::IRenderDevice& device, const render::QuadVertex (&corners)[4], render::TextureId texture,
                 render::Blend blend) {
    const auto it = texture != 0 ? pictureSamplers().find(texture) : pictureSamplers().end();
    if (it == pictureSamplers().end()) {
        device.drawQuad(corners, texture, blend);
        if (texture != 0) render::samplerCache()[0] = nullptr;
        return;
    }
    device.setSamplerState(0, SamplerState::AddressU, TextureAddress::Clamp);
    device.setSamplerState(0, SamplerState::AddressV, TextureAddress::Clamp);
    render::applySampler(device, it->second, 0);
    device.drawQuad(corners, texture, blend, true);
}

void Screen::setColour(int element, uint32_t argb) {
    m_states[static_cast<size_t>(element)].corners.fill(argb);
}

void Screen::setAlpha(int element, float alpha) {
    const uint8_t a = alphaByte(alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha);
    for (uint32_t& c : m_states[static_cast<size_t>(element)].corners) c = withAlpha(c, a);
}

uint32_t Screen::colourOf(int element) const {
    return m_states[static_cast<size_t>(element)].corners[0];
}

void Screen::setTexture(int element, const std::string& texture) {
    State& s = m_states[static_cast<size_t>(element)];
    s.hasTexture = true;
    s.texture = texture;
}

void Screen::setUVs(int element, float u0, float u1, float v0, float v1) {
    State& s = m_states[static_cast<size_t>(element)];
    s.hasUVs = true;
    s.u0 = u0, s.u1 = u1, s.v0 = v0, s.v1 = v1;
}

void Screen::setTexture(const std::string& element, const std::string& texture) {
    const int i = indexOf(element);
    if (i >= 0) {
        m_states[static_cast<size_t>(i)].hasTexture = true;
        m_states[static_cast<size_t>(i)].texture = texture;
    }
}

void Screen::setBoxColour(const std::string& element, uint32_t argb) {
    const int i = indexOf(element);
    if (i >= 0) {
        m_states[static_cast<size_t>(i)].corners.fill(argb);
    }
}

render::TextureId Screen::texture(const std::string& name) {
#ifdef OJ_MACOS
    auto custom=mods::uiTexture(name);if(custom!=name)return texture(custom);
#endif
    const auto found = m_textures.find(name);
    if (found != m_textures.end()) {
        return found->second;
    }
    const render::TextureId id = loadTexture(name);
    m_textures[name] = id;
    return id;
}

render::TextureId Screen::loadTexture(const std::string& name) {
    render::TextureId id = 0;
    const GameFiles files(std::filesystem::current_path());
    // Localized pictures (button hints) exist as <name>_<language letter>.ddt only: the file system looks for them
    // when the name is not there (0x004F9DB3).
    auto bytes = files.read(std::filesystem::path("ui") / "textures" / (name + ".ddt"));
#ifdef OJ_MACOS
    if(auto image=mods::imageBytes(name))bytes=std::move(image);
#endif
    Ddt ddt;
    if (bytes && ddt.parse(*bytes)) {
        render::TextureData data;
        data.format = pixelFormat(ddt.format);
        data.width = ddt.width;
        data.height = ddt.height;
        data.levels = ddt.mipLevels;
        data.pixels = ddt.pixels.data();
        data.size = ddt.pixels.size();
        id = m_device.createTexture(data);
        if (id != 0) {
            pictureSamplers()[id] = {ddt.minFilter, ddt.magFilter, ddt.mipFilter, ddt.addressU, ddt.addressV,
                                     ddt.mipLodBias};
        }
    }
    if (id == 0) {
        log::warning("ui: texture {} not loaded", name);
    }
    return id;
}

void Screen::drawFreePicture(const std::string& textureName, float x0, float y0, float x1, float y1, uint32_t colour) {
    const uint32_t colours[4] = {colour, colour, colour, colour};
    quad(x0, y0, x1, y1, colours, texture(textureName), 0.0f, 1.0f, 0.0f, 1.0f, false);
}

void Screen::quad(float x0, float y0, float x1, float y1, const uint32_t (&colours)[4], render::TextureId tex,
                  float u0, float u1, float v0, float v1, bool additive) {
    const float sw = static_cast<float>(m_device.width());
    const float sh = static_cast<float>(m_device.height());
    x0 = snapEdge(x0, sw);
    x1 = snapEdge(x1, sw);
    y0 = snapEdge(y0, sh);
    y1 = snapEdge(y1, sh);
    const render::QuadVertex corners[4] = {
        {x0, y0, u0, v0, colours[0]},
        {x1, y0, u1, v0, colours[1]},
        {x0, y1, u0, v1, colours[2]},
        {x1, y1, u1, v1, colours[3]},
    };
    drawPicture(m_device, corners, tex, additive ? render::Blend::Additive : render::Blend::Alpha);
}

// A box (0x005B2BE0): Colour or the four VertexColours, Texture with a cell of Animated (0x005184B0), UVScroll
// (0x005185D0) or UVs (u0 u1 v0 v1), Additive; a fading transition sets the corners' alpha (not above their own), GROW
// shrinks the box to its centre, FALSH lays a white quad over a Box (not over an Animated or a UVScroll one).
void Screen::drawBox(const Element& e, const State& s, float seconds) {
    float x0, y0, x1, y1;
    elementRect(e, s, x0, y0, x1, y1);
    const Transition* t = s.hasTransition ? &s.transition : nullptr;
    if (t != nullptr) {
        x0 += t->offsetX, x1 += t->offsetX;
        y0 += t->offsetY, y1 += t->offsetY;
        if (t->type == Transition::kGrow) {
            const float k = (1.0f - t->grow) * (1.0f - t->grow);
            const float dx = (x1 - x0) * k * 0.5f;
            const float dy = (y1 - y0) * k * 0.5f;
            x0 += dx, x1 -= dx, y0 += dy, y1 -= dy;
        }
    }

    uint32_t colours[4] = {s.corners[0], s.corners[1], s.corners[2], s.corners[3]};
    for (uint32_t& c : colours) {
        c = fadeAlpha(c, t);
    }

    render::TextureId tex = 0;
    float u0 = 0.0f, u1 = 1.0f, v0 = 0.0f, v1 = 1.0f;
    // A textured box drawn by the Animated or the UVScroll branch has no white flash over it (both go to the end).
    bool flashOver = true;
    const Property* tp = e.find("Texture");
    if (s.hasTexture || (tp != nullptr && !tp->values.empty())) {
        tex = s.hasTexture ? (s.texture.empty() ? 0 : texture(s.texture)) : texture(tp->values[0]);
        if (const Property* anim = e.find("Animated")) {
            // "Animated <columns> <rows> <rate>" (0x005ACA10: +0x78 1, the columns +0x7C and the rows +0x80 at least 1,
            // the rate +0x84 of frames a second, 0 when missing; the box's own 25 only without the line). 0x005184B0:
            // the cell of the game's clock x the rate cut toward zero, modulo the cells, counted along the rows.
            const auto whole = [&](size_t i) { return i < anim->values.size() ? std::atoi(anim->values[i].c_str()) : 0; };
            const auto columns = static_cast<uint32_t>(std::max(whole(0), 1));
            const auto rows = static_cast<uint32_t>(std::max(whole(1), 1));
            const float rate = x87::asUnsigned(static_cast<uint32_t>(whole(2)));
            const uint32_t frame = static_cast<uint32_t>(x87::chop(rate * seconds)) % (columns * rows);
            const uint32_t column = frame % columns;
            const uint32_t row = (frame - column) / columns;
            u0 = x87::asUnsigned(column) / x87::asUnsigned(columns);
            u1 = x87::asUnsigned(column + 1) / x87::asUnsigned(columns);
            v0 = x87::asUnsigned(row) / x87::asUnsigned(rows);
            v1 = x87::asUnsigned(row + 1) / x87::asUnsigned(rows);
            flashOver = false;
        } else if (const Property* scroll = e.find("UVScroll")) {
            // 0x005185D0: the whole picture once, moved by the game's clock: v less (v x 1/16 cut toward zero) x 16,
            // loaded back as unsigned (Stock problem: a negative multiple gains 2^32).
            auto wrap = [](float v) {
                const auto whole = static_cast<uint32_t>(x87::chop(v * 0.0625f)) << 4;
                return v - x87::asUnsigned(whole);
            };
            u0 = wrap(number(scroll, 0, 0.0f) * seconds);
            v0 = wrap(number(scroll, 1, 0.0f) * seconds);
            u1 = u0 + 1.0f;
            v1 = v0 + 1.0f;
            flashOver = false;
        } else if (s.hasUVs) {
            u0 = s.u0, u1 = s.u1, v0 = s.v0, v1 = s.v1;
        } else if (const Property* uv = e.find("UVs")) {
            u0 = number(uv, 0, 0.0f);
            u1 = number(uv, 1, 1.0f);
            v0 = number(uv, 2, 0.0f);
            v1 = number(uv, 3, 1.0f);
        }
    }
    if ((colours[0] | colours[1] | colours[2] | colours[3]) >> 24 != 0) {
        quad(x0, y0, x1, y1, colours, tex, u0, u1, v0, v1, flag(e, "Additive"));
    }
    if (flashOver && t != nullptr && t->type == Transition::kFlash && e.kind == ElementKind::Box && t->flash > 0.0f) {
        const uint32_t white = withAlpha(0x00FFFFFF, alphaByte(t->flash));
        const uint32_t flash[4] = {white, white, white, white};
        quad(x0, y0, x1, y1, flash, 0, 0.0f, 1.0f, 0.0f, 1.0f, false);
    }
}

// BeginLine <name> x0 y0 x1 y1 (0x005AACB0, the line 0x005B32B0; no window has one): its draw 0x005B3340, shown, a
// line from (x0, y0) to (x1, y1) of the screen moved by its transition's offsets in its Colour (+0x10, 0x00517A60); a
// FADEIN or GROW writes the transition's alpha into that colour (0x0042E560), where it stays.
void Screen::drawLineElement(const Element& e, State& s) {
    const float sw = static_cast<float>(m_device.width());
    const float sh = static_cast<float>(m_device.height());
    float x0 = sw * s.x, y0 = sh * s.y;
    float x1 = sw * s.w, y1 = sh * s.h;
    if (s.hasTransition) {
        const Transition& t = s.transition;
        x0 += t.offsetX, x1 += t.offsetX;
        y0 += t.offsetY, y1 += t.offsetY;
        if (t.type == Transition::kFadeIn || t.type == Transition::kGrow) {
            s.corners[0] = withAlpha(s.corners[0], alphaByte(t.alpha));
        }
    }
    (void)e;
    m_device.drawLine(x0, y0, x1, y1, s.corners[0]);
}

}  // namespace oj::ui
