#pragma once
// The sprites' materials: texture sheets shown by time, scrolling signs, the crowd and the light flares.

#include "Render/Materials/MaterialsInternal.h"

#include <array>
#include <cfenv>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace oj::render::materials {

// A texture sheet of x by y cells shown by time (the sprite and sign classes): the counts, the frames a second and
// the playlist.
struct SpriteSheet {
    uint32_t x = 1;
    uint32_t y = 1;
    float fps = 0.0f;
    std::vector<uint32_t> playlist;

    // A count of cells (0 taken as 1) into `count`; its inverse is returned.
    static float cells(int32_t n, uint32_t& count) {
        if (n == 0) n = 1;
        count = static_cast<uint32_t>(n);
        const float f = static_cast<float>(count);
        return 1.0f / f;
    }
    // "1,3,5-8,2*4" (0x0052DA40, 0x005282B0): single frames, ranges (either way), repeats; each kept less one (the
    // frames counted from 1, the cells from 0); 0 is never added.
    void setPlaylist(const char* text) {
        playlist.clear();
        if (*text == '\0') return;
        uint32_t first = 0;
        uint32_t second = 0;
        int mode = 0;
        while (true) {
            const char ch = *text;
            if (ch >= '0' && ch <= '9') {
                if (mode == 0) first = first * 10 + static_cast<uint32_t>(ch - '0');
                else second = second * 10 + static_cast<uint32_t>(ch - '0');
            } else if (ch == '-') {
                mode = 1;
            } else if (ch == '*') {
                mode = 2;
            } else {
                if (mode == 0) {
                    if (first != 0) playlist.push_back(first - 1);
                } else if (mode == 1) {
                    if (first != 0) {
                        if (static_cast<int32_t>(first) < static_cast<int32_t>(second)) {
                            for (int32_t v = static_cast<int32_t>(first); v <= static_cast<int32_t>(second); ++v) {
                                playlist.push_back(static_cast<uint32_t>(v - 1));
                            }
                        } else {
                            for (int32_t v = static_cast<int32_t>(first); v >= static_cast<int32_t>(second); --v) {
                                playlist.push_back(static_cast<uint32_t>(v - 1));
                            }
                        }
                    }
                } else if (first != 0 && static_cast<int32_t>(second) > 0) {
                    for (uint32_t k = 0; k < second; ++k) playlist.push_back(first - 1);
                }
                first = 0;
                second = 0;
                mode = 0;
            }
            if (*text == '\0') break;
            ++text;
        }
    }
    // The cell of the frame at `time` (the frame cut toward zero, through the playlist): (column x the inverse of x,
    // row x the inverse of y, 0, 0).
    void frame(float time, const float* inverse, float* out) const {
        uint32_t f = static_cast<uint32_t>(static_cast<int32_t>(time * fps));
        if (!playlist.empty()) f = playlist[f % playlist.size()];
        const uint32_t cell = f % (x * y);
        const uint32_t column = cell % x;
        const uint32_t row = (cell - column) / x;
        out[0] = static_cast<float>(column) * inverse[0];
        out[1] = static_cast<float>(row) * inverse[1];
        out[2] = 0.0f;
        out[3] = 0.0f;
    }
};

// Sprite (Unlit) (id 13, 0x005293D0) and Sprite (Lit) (id 16, 0x0052E190): cells of a texture sheet by time.
class Sprite final : public Material {
public:
    Sprite(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs, bool lit)
        : Material(c.id, c.flags, r), m_program(programs(lit ? "Material_LitSprite" : "Material_UnlitSprite")),
          m_lit(lit) {
        if (lit) m_scale[0] = m_scale[1] = 0.0f;
    }

    void setParam(uint32_t index, const void* value) override {
        touched();
        switch (index) {
            case 0: m_scale[0] = SpriteSheet::cells(roundNearest(asFloat(value)), m_sheet.x); break;
            case 1: m_scale[1] = SpriteSheet::cells(roundNearest(asFloat(value)), m_sheet.y); break;
            case 2: m_sheet.fps = asFloat(value); break;
            case 3: m_sheet.setPlaylist(static_cast<const char*>(value)); break;
            default: break;
        }
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override {
        if (!m_lit) m_r.device().setRenderState(RenderState::FogEnable, 1);
    }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        if (!m_lit) {
            if (m_program != nullptr) m_program->beginPass(pass);
            m_r.bindTexture(m_texture, 0);
            float c[8];
            m_sheet.frame(m_r.time, m_scale, c);
            std::memcpy(c + 4, m_scale, 16);
            m_r.programs().managerVertexConstants(35, c, 2);
        } else {
            m_r.bindTexture(m_texture, 0);
            m_r.setVertexConstants(36, m_scale, 1);
            float c[4];
            m_sheet.frame(m_r.time, m_scale, c);
            m_r.setVertexConstants(35, c, 1);
            if (m_program != nullptr) m_program->beginPass(pass);
        }
    }

private:
    Program* m_program;
    bool m_lit;
    const GameTexture* m_texture = nullptr;
    float m_scale[4] = {1.0f, 1.0f, 0.0f, 1.0f};  // +0x24 .. +0x30
    SpriteSheet m_sheet;
};

// Anim Alpha-Add (id 33, vtable 0x006F90DC): Material_AnimAlphaAdd (+0x20); X-Cells, Y-Cells (cut toward zero, 0 as
// 1; their inverses +0x24, +0x28), FPS (+0x34), Playlist. Drawing (0x00528180): the pass, the texture, c35 = the cell
// of the frame and c36 = (+0x24 .. +0x30; 1, 1, 1, 1 at first) through the state manager. No setup.
class AnimAlphaAdd final : public Material {
public:
    AnimAlphaAdd(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_AnimAlphaAdd")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        switch (index) {
            case 0: m_scale[0] = SpriteSheet::cells(static_cast<int32_t>(asFloat(value)), m_sheet.x); break;
            case 1: m_scale[1] = SpriteSheet::cells(static_cast<int32_t>(asFloat(value)), m_sheet.y); break;
            case 2: m_sheet.fps = asFloat(value); break;
            case 3: m_sheet.setPlaylist(static_cast<const char*>(value)); break;
            default: break;
        }
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        m_r.bindTexture(m_texture, 0);
        float c[8];
        m_sheet.frame(m_r.time, m_scale, c);
        std::memcpy(c + 4, m_scale, 16);
        m_r.programs().managerVertexConstants(35, c, 2);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    float m_scale[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    SpriteSheet m_sheet;
};

// Unlit Sign Anim (id 19, vtable 0x006F9650): Material_UnlitSignAnim (+0x20); X-Cells, Y-Cells (rounded, 0 as 1),
// FPS, Playlist as Anim Alpha-Add. Drawing (0x0052D930): the pass, the texture, c36 = (+0x24 .. +0x30; 0, 0, 0, 1 at
// first), then c35 = the cell of the frame. Setup (0x00530CF0): the default cull, fog on.
class UnlitSignAnim final : public Material {
public:
    UnlitSignAnim(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_UnlitSignAnim")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        switch (index) {
            case 0: m_scale[0] = SpriteSheet::cells(roundNearest(asFloat(value)), m_sheet.x); break;
            case 1: m_scale[1] = SpriteSheet::cells(roundNearest(asFloat(value)), m_sheet.y); break;
            case 2: m_sheet.fps = asFloat(value); break;
            case 3: m_sheet.setPlaylist(static_cast<const char*>(value)); break;
            default: break;
        }
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override {
        m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull);
        m_r.device().setRenderState(RenderState::FogEnable, 1);
    }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        m_r.bindTexture(m_texture, 0);
        m_r.programs().managerVertexConstants(36, m_scale, 1);
        float c[4];
        m_sheet.frame(m_r.time, m_scale, c);
        m_r.programs().managerVertexConstants(35, c, 1);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    float m_scale[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    SpriteSheet m_sheet;
};

// Unlit Sign Scroll (id 18, vtable 0x006F9408): Material_UnlitSignScroll (+0x20); X-Speed (+0x24), Y-Speed (+0x28).
// Setup (0x0052B050): the default cull, c36 = (1, 1, 0, 0), fog on. Drawing (0x0052B0D0): the pass, the texture,
// c35 = (time x speed less its whole sixteens, for x and y; 0, 0).
class UnlitSignScroll final : public Material {
public:
    UnlitSignScroll(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_UnlitSignScroll")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index == 0) m_speed[0] = asFloat(value);
        else if (index == 1) m_speed[1] = asFloat(value);
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override {
        m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull);
        const float c36[4] = {1.0f, 1.0f, 0.0f, 0.0f};
        m_r.programs().managerVertexConstants(36, c36, 1);
        m_r.device().setRenderState(RenderState::FogEnable, 1);
    }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        m_r.bindTexture(m_texture, 0);
        // The sixteens are cut toward zero and made a float as an unsigned number.
        // Stock problem: a negative offset (a negative speed) takes away 2^32 more than its sixteens.
        auto wrap = [](float v) {
            const auto sixteens = static_cast<int32_t>(0.0625f * v) << 4;
            return v - static_cast<float>(static_cast<uint32_t>(sixteens));
        };
        const float c35[4] = {wrap(m_r.time * m_speed[0]), wrap(m_r.time * m_speed[1]), 0.0f, 0.0f};
        m_r.programs().managerVertexConstants(35, c35, 1);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    float m_speed[2] = {};
};

// Crowd Sprite (id 35, vtable 0x006F90B8) and Crowd Sprite Anim (id 47, vtable 0x006F9754): Material_CrowdSprite
// (+0x20), Material_CrowdSpriteAnim; parameters cut toward zero (0 as 1). The sprite: X-Cells → +0x24 = 1/x, +0x44 =
// x, +0x48 = 1/x; Y-Cells → +0x28 = 1/y, +0x40 = 1/y; Part 0, 1, 2 puts 1 / +0x40 in +0x34, +0x38 or +0x3C (the
// others 0), +0x50 = Part x 4; Ambient-Multiplier → +0x4C (0 as 1). The anim swaps the cells' roles (X-Cells → +0x24,
// +0x40; Y-Cells → +0x28, +0x44 = y, +0x48 = 1/y) and has Min-FPS (+0x54), Max-FPS (+0x58), their difference (+0x5C)
// and the time (+0x60). Drawing (0x00527E10, 0x0052F070): the pass, the texture, c36 = +0x24 .., c52 = +0x34 ..
// (two registers; the anim three, the time set first). Passes: the technique's. No setup.
class CrowdSprite final : public Material {
public:
    CrowdSprite(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs, bool anim)
        : Material(c.id, c.flags, r),
          m_program(programs(anim ? "Material_CrowdSpriteAnim" : "Material_CrowdSprite")),
          m_anim(anim) {
        // +0x24 .. +0x60 as the constructors leave them.
        m_values = {1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0};
        if (anim) m_values[0] = m_values[1] = m_values[2] = 0.0f;
    }

    void setParam(uint32_t index, const void* value) override {
        touched();
        const float v = asFloat(value);
        // A count cut toward zero, 0 as 1, made a float as an unsigned number.
        auto count = [](float f) {
            int32_t n = static_cast<int32_t>(f);
            if (n == 0) n = 1;
            return static_cast<float>(static_cast<uint32_t>(n));
        };
        if (index == 0 || index == 1) {
            const float n = count(v);
            const float inverse = 1.0f / n;
            field(index == 0 ? 0x24 : 0x28) = inverse;
            if ((index == 0) != m_anim) {
                field(0x44) = n;
                field(0x48) = inverse;
            } else {
                field(0x40) = inverse;
            }
        } else if (index == 2) {
            const int32_t part = static_cast<int32_t>(v);
            if (part >= 0 && part <= 2) {
                const float cells = 1.0f / field(0x40);
                field(0x34) = part == 0 ? cells : 0.0f;
                field(0x38) = part == 1 ? cells : 0.0f;
                field(0x3C) = part == 2 ? cells : 0.0f;
            }
            field(0x50) = v * 4.0f;
        } else if (index == (m_anim ? 5u : 3u)) {
            field(0x4C) = v == 0.0f ? 1.0f : v;
        } else if (m_anim && index == 3) {
            field(0x54) = v;
            field(0x5C) = field(0x58) - v;
        } else if (m_anim && index == 4) {
            field(0x58) = v;
            field(0x5C) = v - field(0x54);
        }
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    Program* program() override { return m_program; }
    uint32_t passes() override { return m_program != nullptr ? m_program->passCount() : 0; }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        m_r.bindTexture(m_texture, 0);
        m_r.programs().managerVertexConstants(36, m_values.data(), 1);
        if (m_anim) m_values[15] = m_r.time;
        m_r.programs().managerVertexConstants(52, m_values.data() + 4, m_anim ? 3 : 2);
    }

private:
    float& field(uint32_t offset) { return m_values[(offset - 0x24) / 4]; }

    Program* m_program;
    bool m_anim;
    const GameTexture* m_texture = nullptr;
    std::array<float, 16> m_values{};  // +0x24 .. +0x60
};

// Light Flare (id 17, 0x00528980, vtable 0x006F9174): Material_LightFlare (+0x20), the texture (slot 0, +0x1C); the
// scale +0x24 .. +0x30 (1, 1, 0, 1), FPS +0x34 (0), the cells +0x38, +0x3C (1, 1) and the playlist +0x40 as the
// sprites'. Its parameters (0x00528CD0): X-Cells and Y-Cells (to the nearest, 0 as 1; their inverses +0x24, +0x28),
// FPS, Playlist (as Anim Alpha-Add's) and the 4th +0x2C (the vertex program's pull toward the camera, c36.z); no
// material file gives the flares' class any (they are made by the code). Drawing (0x00528B10): the pass, the texture, c36 = the scale; the
// frame: the time x FPS cut toward zero, plus the node's random c40.x x the playlist's length (with one) or the cells'
// count (without), cut toward zero; c35 = (its column x +0x24, its row x +0x28, 0, 0), both through the state
// manager. Setup (0x00528AE0): stages 1 .. 3 emptied.
class LightFlare final : public Material {
public:
    LightFlare(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_LightFlare")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        switch (index) {
            case 0: m_scale[0] = SpriteSheet::cells(roundNearest(asFloat(value)), m_cells[0]); break;
            case 1: m_scale[1] = SpriteSheet::cells(roundNearest(asFloat(value)), m_cells[1]); break;
            case 2: m_fps = asFloat(value); break;
            case 3: {
                SpriteSheet sheet;
                sheet.setPlaylist(static_cast<const char*>(value));
                m_playlist = std::move(sheet.playlist);
                break;
            }
            case 4: m_scale[2] = asFloat(value); break;
            default: break;
        }
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override {
        for (uint32_t stage = 1; stage < 4; ++stage) m_r.device().setTexture(stage, 0);
    }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        m_r.bindTexture(m_texture, 0);
        m_r.programs().managerVertexConstants(36, m_scale, 1);
        const auto frame = static_cast<uint32_t>(static_cast<int32_t>(m_r.time * m_fps));
        const uint32_t cells = m_cells[0] * m_cells[1];
        uint32_t f = 0;
        if (!m_playlist.empty()) {
            const auto n = static_cast<uint32_t>(m_playlist.size());
            const auto at = static_cast<uint32_t>(static_cast<int64_t>(
                static_cast<float>(n) * m_r.random[0] + static_cast<float>(frame)));
            f = m_playlist[at % n];
        } else {
            f = static_cast<uint32_t>(static_cast<int64_t>(
                static_cast<float>(cells) * m_r.random[0] + static_cast<float>(frame)));
        }
        const uint32_t cell = f % cells;
        const uint32_t column = cell % m_cells[0];
        const uint32_t row = (cell - column) / m_cells[0];
        const float c[4] = {static_cast<float>(column) * m_scale[0], static_cast<float>(row) * m_scale[1], 0.0f,
                            0.0f};
        m_r.programs().managerVertexConstants(35, c, 1);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    float m_scale[4] = {1.0f, 1.0f, 0.0f, 1.0f};
    float m_fps = 0.0f;
    uint32_t m_cells[2] = {1, 1};
    std::vector<uint32_t> m_playlist;
};

}  // namespace oj::render::materials
