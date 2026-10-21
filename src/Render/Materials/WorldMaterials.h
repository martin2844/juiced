#pragma once
// The world's materials: the road, buildings, the sky, trees, the sea, water and the blurred shadow.

#include "Render/Materials/MaterialsInternal.h"

#include <cfenv>
#include <cmath>
#include <cstdint>

namespace oj::render::materials {

// Tarmac (id 11, 0x0052F8B0): the road. Shininess (+0x28) is c39.w, Specular Colour (+0x20, 0xFF000000) and
// Specular Highlight (+0x24) go to the renderer's specular (c34); Mask Alpha PS2 is not kept.
class Tarmac final : public Material {
public:
    Tarmac(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r),
          m_shadow(programs("Material_Tarmac")),
          m_noShadow(programs("Material_TarmacNoShadow")),
          m_night(programs("Material_TarmacNight")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index == 0) m_shininess = asFloat(value);
        else if (index == 1) m_colour = asBits(value);
        else if (index == 2) m_highlight = asFloat(value);
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    // 0x0052FA10: the default cull, fog on.
    void setup(uint32_t) override {
        m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull);
        m_r.device().setRenderState(RenderState::FogEnable, 1);
    }
    Program* program() override {
        if (m_r.lightTile == nullptr) return m_noShadow;
        return m_r.night ? m_night : m_shadow;
    }
    // 0x0052FA40: the texture on stage 0, the renderer's half-vector cube on stage 1 (none while the environment
    // cube is drawn), the light tile on stage 2; at night vertex c52 = (0.5, 0.5, 1, 1) and the renderer's +0x164 on
    // stage 3; pixel c0 the ambient / 511 (at night c2, and c0.w the night road shininess), c1 light 1's colour / 511.
    void bind(uint32_t pass) override {
        m_r.bindTexture(m_texture, 0);
        if (m_r.drawingCube) {
            m_r.device().setTexture(1, 0);
        } else {
            m_r.bindTexture(m_r.halfVectorCube, 1);
        }
        Program* p = m_noShadow;
        if (m_r.lightTile != nullptr) {
            m_r.bindTexture(m_r.lightTile, 2);
            p = m_r.night ? m_night : m_shadow;
        }
        if (p != nullptr) p->beginPass(pass);
        if (m_r.lightTile != nullptr && m_r.night) {
            const float c52[4] = {0.5f, 0.5f, 1.0f, 1.0f};
            m_r.device().setVertexShaderConstantF(52, c52, 1);
            m_r.bindTexture(m_r.reflection, 3);
        }
        const Vec4& a = m_r.ambient;
        const float ambient[4] = {a[0] * kOver511, a[1] * kOver511, a[2] * kOver511, a[3]};
        if (m_r.night) {
            // Stock problem: x, y, z of pixel c0 at night are values left on the stack; 0 here.
            const float c0[4] = {0.0f, 0.0f, 0.0f, m_r.nightRoadShininess};
            m_r.device().setPixelShaderConstantF(0, c0, 1);
            m_r.device().setPixelShaderConstantF(2, ambient, 1);
        } else {
            m_r.device().setPixelShaderConstantF(0, ambient, 1);
        }
        const Vec4& l = m_r.light1Colour;
        const float c1[4] = {l[0] * kOver511, l[1] * kOver511, l[2] * kOver511, l[3]};
        m_r.device().setPixelShaderConstantF(1, c1, 1);
        // Stock problem: x, y, z of c39 are values left on the stack; 0 here.
        const float c39[4] = {0.0f, 0.0f, 0.0f, m_shininess};
        m_r.setVertexConstants(39, c39, 1);
        m_r.setSpecular(m_colour, m_highlight);
    }

private:
    Program* m_shadow;
    Program* m_noShadow;
    Program* m_night;
    const GameTexture* m_texture = nullptr;
    float m_shininess = 0.0f;
    uint32_t m_colour = 0xFF000000;
    float m_highlight = 0.0f;
};

// Building (id 10, 0x0052D0E0, vtable 0x006F95E4), named by no material of the game's files: Material_Building
// (+0x24) and Material_BuildingNight (+0x28), the night's in use with the renderer's night (+0x44's +0x38). Side
// Fresnel (+0x48), Front Fresnel (+0x38), Opacity (+0x34); Fade Start and Fade End x ([0x0071E5CC] (60) x 0.005 + 0.5)
// (+0x4C, +0x54) and 1 / (end - start) (+0x50) after either; Light-Tile-X and -Y as 1 / the value (0 taken as 1;
// +0x5C, +0x60; +0x64, +0x68 1); textures: Texture (+0x1C), Night-Light (+0x20). Setup (0x0052D260): at night the
// opacity 0; the default cull. Drawing (0x0052D2B0): the texture on stage 0; at night the night pass and the night
// light on stage 1, else the day pass and the renderer's environment texture (+0x11C) on stage 1; on the day
// program's state manager pixel c0, c1 = +0x2C .. +0x48, vertex c52 = +0x4C .. +0x58 and c38 = +0x5C .. +0x68.
class Building final : public Material {
public:
    Building(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r),
          m_day(programs("Material_Building")),
          m_nightProgram(programs("Material_BuildingNight")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        const float v = asFloat(value);
        const float detail = static_cast<float>(60) * 0.005f + 0.5f;  // [0x0071E5CC], never written
        switch (index) {
            case 0: m_c0c1[7] = v; break;
            case 1: m_c0c1[3] = v; break;
            case 2: m_c0c1[2] = v; break;
            case 3:
                m_fade[0] = detail * v;
                m_fade[1] = 1.0f / (m_fade[2] - m_fade[0]);
                break;
            case 4:
                m_fade[2] = detail * v;
                m_fade[1] = 1.0f / (m_fade[2] - m_fade[0]);
                break;
            case 5: m_tile[0] = 1.0f / (v == 0.0f ? 1.0f : v); break;
            case 6: m_tile[1] = 1.0f / (v == 0.0f ? 1.0f : v); break;
            default: break;
        }
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot == 0) m_texture = texture;
        else if (slot == 1) m_nightLight = texture;
        else return false;
        return true;
    }
    void setup(uint32_t) override {
        if (m_r.night) m_c0c1[2] = 0.0f;
        m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull);
    }
    Program* program() override { return m_r.night ? m_nightProgram : m_day; }
    void bind(uint32_t pass) override {
        m_r.bindTexture(m_texture, 0);
        if (m_r.night) {
            if (m_nightProgram != nullptr) m_nightProgram->beginPass(pass);
            m_r.bindTexture(m_nightLight, 1);
        } else {
            if (m_day != nullptr) m_day->beginPass(pass);
            m_r.bindTexture(m_r.environment, 1);
        }
        m_r.device().setPixelShaderConstantF(0, m_c0c1, 2);
        m_r.programs().managerVertexConstants(52, m_fade, 1);
        m_r.programs().managerVertexConstants(38, m_tile, 1);
    }

private:
    Program* m_day;
    Program* m_nightProgram;
    const GameTexture* m_texture = nullptr;
    const GameTexture* m_nightLight = nullptr;
    float m_c0c1[8] = {};                        // +0x2C .. +0x48
    float m_fade[4] = {};                        // +0x4C .. +0x58
    float m_tile[4] = {1.0f, 1.0f, 1.0f, 1.0f};  // +0x5C .. +0x68
};

// Building Opaque (id 42, 0x00529E60).
class BuildingOpaque final : public Material {
public:
    BuildingOpaque(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r),
          m_day(programs("Material_BuildingOpaque")),
          m_nightProgram(programs("Material_BuildingOpaqueNight")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        const float v = asFloat(value);
        switch (index) {
            case 0: m_c0c1[7] = v; break;  // Side Fresnel (+0x48)
            case 1: m_c0c1[3] = v; break;  // Front Fresnel (+0x38)
            case 2: m_tile[0] = 1.0f / (v == 0.0f ? 1.0f : v); break;
            case 3: m_tile[1] = 1.0f / (v == 0.0f ? 1.0f : v); break;
            default: break;
        }
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot == 0) m_texture = texture;
        else if (slot == 1) m_nightLight = texture;
        else return false;
        return true;
    }
    void setup(uint32_t) override { cull(m_day, false); }
    Program* program() override { return m_r.night ? m_nightProgram : m_day; }
    void bind(uint32_t pass) override {
        m_r.bindTexture(m_texture, 0);
        if (m_r.night) {
            if (m_nightProgram != nullptr) m_nightProgram->beginPass(pass);
            m_r.bindTexture(m_nightLight, 1);
            const float c7[4] = {0.0f, 0.0f, 0.0f, m_r.dimNightLights ? 0.25f : 1.0f};
            m_r.device().setPixelShaderConstantF(7, c7, 1);
        } else {
            if (m_day != nullptr) m_day->beginPass(pass);
            m_r.bindTexture(m_r.environment, 1);
        }
        // Stock problem: x, y, z of c0 and c1 are fields the object never sets; 0 here.
        m_r.device().setPixelShaderConstantF(0, m_c0c1, 2);
        m_r.programs().managerVertexConstants(38, m_tile, 1);
    }

private:
    Program* m_day;
    Program* m_nightProgram;
    const GameTexture* m_texture = nullptr;
    const GameTexture* m_nightLight = nullptr;
    float m_c0c1[8] = {};                        // +0x2C .. +0x48
    float m_tile[4] = {1.0f, 1.0f, 1.0f, 1.0f};  // +0x4C .. +0x58
};

// Sky (id 41, 0x00528490): no parameters or textures.
class Sky final : public Material {
public:
    Sky(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_Sky")) {}

    void setParam(uint32_t, const void*) override {}
    bool setTexture(uint32_t, const GameTexture*) override { return false; }
    // 0x00528560, through the program's state manager: the default cull mode and c52 = (256, 256, 256, 1), the
    // dome's scale.
    void setup(uint32_t) override {
        if (m_program == nullptr) return;
        cull(m_program, false);
        static const float kScale[4] = {256.0f, 256.0f, 256.0f, 1.0f};
        m_r.programs().managerVertexConstants(52, kScale, 1);
    }
    Program* program() override { return m_program; }
    // 0x0052B840
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
    }

private:
    Program* m_program;
};

// Tree-Standard (id 30) and Tree-Sprite (id 31) (vtable 0x006F9674; 0x0052AA40, 0x0052DC50): Material_TreeStandard /
// Material_TreeSprite (+0x20). Diffuse 2x Min/Max (r, g, b / 128, a / 255) and Ambient 4x Min/Max (/ 64) are kept as
// the minimum and the maximum less it (+0x24, +0x34, +0x44, +0x54); the XZ / Y magnitudes (x 0.01) and the speeds
// give four sway vectors of random amplitude, speed and phase (0x0052A840: 36 rand() values), made when the object is
// made and again at each of these six parameters. The loading thread makes them: its x87 works at 53 bits.
// Drawing (0x0052AB60): the texture, the pass, c52 .. c55 the colours and c56 .. c59 the sway at the time
// (sin(time x speed + phase) x amplitude) on the device. Setup (0x00529530): fog on.
class Tree final : public Material {
public:
    Tree(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs, bool sprite)
        : Material(c.id, c.flags, r), m_program(programs(sprite ? "Material_TreeSprite" : "Material_TreeStandard")) {
        sway();
    }

    void setParam(uint32_t index, const void* value) override {
        touched();
        const uint32_t argb = asBits(value);
        const float v = asFloat(value);
        // A colour's minimum (keeping the maximum) or maximum, as r, g, b x `scale` and a / 255.
        auto colour = [argb](float* out, float scale) {
            out[0] = static_cast<float>((argb >> 16) & 0xFF) * scale;
            out[1] = static_cast<float>((argb >> 8) & 0xFF) * scale;
            out[2] = static_cast<float>(argb & 0xFF) * scale;
            out[3] = static_cast<float>(argb >> 24) * kOver255;
        };
        auto minimum = [&](uint32_t at, float scale) {
            for (int i = 0; i < 4; ++i) m_colours[at + 4 + i] = m_colours[at + i] + m_colours[at + 4 + i];
            colour(m_colours + at, scale);
            for (int i = 0; i < 4; ++i) m_colours[at + 4 + i] = m_colours[at + 4 + i] - m_colours[at + i];
        };
        auto maximum = [&](uint32_t at, float scale) {
            colour(m_colours + at + 4, scale);
            for (int i = 0; i < 4; ++i) m_colours[at + 4 + i] = m_colours[at + 4 + i] - m_colours[at + i];
        };
        switch (index) {
            case 0: minimum(0, kOver128); break;
            case 1: maximum(0, kOver128); break;
            case 2: minimum(8, kOver64); break;
            case 3: maximum(8, kOver64); break;
            case 4: m_low[0] = m_low[2] = v * 0.01f; sway(); break;
            case 5: m_high[0] = m_high[2] = v * 0.01f; sway(); break;
            case 6: m_low[1] = v * 0.01f; sway(); break;
            case 7: m_high[1] = v * 0.01f; sway(); break;
            case 8: m_speed[0] = v; sway(); break;
            case 9: m_speed[1] = v; sway(); break;
            default: break;
        }
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override { m_r.device().setRenderState(RenderState::FogEnable, 1); }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        m_r.bindTexture(m_texture, 0);
        if (m_program != nullptr) m_program->beginPass(pass);
        m_r.device().setVertexShaderConstantF(52, m_colours, 4);
        // Stock problem: w of c56 .. c59 are values left on the stack; 0 here.
        float sway[16] = {};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 3; ++j) {
                const int k = i * 3 + j;
                const float angle = m_r.time * m_rate[k] + m_phase[k];
                sway[i * 4 + j] =
                    static_cast<float>(std::sin(static_cast<double>(angle)) * static_cast<double>(m_amplitude[k]));
            }
        }
        m_r.device().setVertexShaderConstantF(56, sway, 4);
    }

private:
    // 0x0052A840: per sway vector the amplitudes (low + (high - low) x r / 32767), the speeds (low + r x range /
    // 32767, the range kept as a float) and the phases (r x 2 pi / 32767), nine values each.
    void sway() {
        CrtRandom& random = m_r.materialRandom != nullptr ? *m_r.materialRandom : gameRandom();
        const auto range = static_cast<float>(static_cast<double>(m_speed[1]) - m_speed[0]);
        const double k = kRandomScale;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 3; ++j) {
                const double r = random.next();
                m_amplitude[i * 3 + j] =
                    static_cast<float>(((static_cast<double>(m_high[j]) - m_low[j]) * r) * k + m_low[j]);
            }
            for (int j = 0; j < 3; ++j) {
                const double r = random.next();
                m_rate[i * 3 + j] = static_cast<float>((r * range) * k + m_speed[0]);
            }
            for (int j = 0; j < 3; ++j) {
                const double r = random.next();
                m_phase[i * 3 + j] = static_cast<float>(r * static_cast<double>(kPhaseScale));
            }
        }
    }

    Program* m_program;
    const GameTexture* m_texture = nullptr;
    float m_colours[16] = {};  // +0x24 .. +0x60
    float m_amplitude[12] = {};  // +0x64
    float m_rate[12] = {};       // +0x94
    float m_phase[12] = {};      // +0xC4
    float m_low[3] = {};         // +0xF4: XZ, Y, XZ
    float m_high[3] = {};        // +0x100
    float m_speed[2] = {};       // +0x10C, +0x110
};

// Sea (id 48, vtable 0x006F96E8): Material_SeaShadow (+0x20), Material_SeaNoShadow (+0x24), Material_SeaShadowNight
// (+0x28), chosen as Standard Opaque's; U/V-Scroll1, U/V-Scroll2 (+0x2C .. +0x38), U-Tile, V-Tile (+0x3C, +0x40; 0 as
// 1; +0x44, +0x48 are 1). Setup (0x0052E750): the default cull, the specular colour white and highlight 255.
// Drawing (0x0052E790): the texture on stages 0 and 1, the light tile on stage 3, the pass, the half-vector cube on
// stage 2, the renderer's +0x118 on stage 4; pixel c0 = the ambient / 255 (its fourth value kept); c52, c53 = the
// two scrolls' fractions (time x scroll less its whole part cut toward zero; 0, 0); pixel c1 = the zenith colour /
// 511; c38 = the tiles.
class Sea final : public Material {
public:
    Sea(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r),
          m_shadow(programs("Material_SeaShadow")),
          m_noShadow(programs("Material_SeaNoShadow")),
          m_night(programs("Material_SeaShadowNight")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        const float v = asFloat(value);
        if (index < 4) m_scroll[index] = v;
        else if (index == 4) m_tile[0] = v == 0.0f ? 1.0f : v;
        else if (index == 5) m_tile[1] = v == 0.0f ? 1.0f : v;
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override {
        m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull);
        m_r.setSpecular(0xFFFFFFFFu, 255.0f);
    }
    Program* program() override {
        if (m_r.lightTile == nullptr) return m_noShadow;
        return m_r.night ? m_night : m_shadow;
    }
    void bind(uint32_t pass) override {
        m_r.bindTexture(m_texture, 0);
        m_r.bindTexture(m_texture, 1);
        if (m_r.lightTile != nullptr) m_r.bindTexture(m_r.lightTile, 3);
        if (Program* p = program()) p->beginPass(pass);
        m_r.bindTexture(m_r.halfVectorCube, 2);
        m_r.bindTexture(m_r.dynamicEnvironment, 4);
        const Vec4& a = m_r.ambient;
        const float c0[4] = {a[0] * kOver255, a[1] * kOver255, a[2] * kOver255, a[3]};
        m_r.device().setPixelShaderConstantF(0, c0, 1);
        auto fraction = [this](float scroll) {
            const float v = m_r.time * scroll;
            return v - static_cast<float>(static_cast<int32_t>(v));
        };
        const float scrolls[8] = {fraction(m_scroll[0]), fraction(m_scroll[1]), 0.0f, 0.0f,
                                  fraction(m_scroll[2]), fraction(m_scroll[3]), 0.0f, 0.0f};
        m_r.programs().managerVertexConstants(52, scrolls, 2);
        // Stock problem: w of pixel c1 is a value left on the stack; 0 here.
        const uint32_t z = m_r.zenith;
        const float c1[4] = {static_cast<float>((z >> 16) & 0xFF) * kOver511,
                             static_cast<float>((z >> 8) & 0xFF) * kOver511,
                             static_cast<float>(z & 0xFF) * kOver511, 0.0f};
        m_r.device().setPixelShaderConstantF(1, c1, 1);
        m_r.programs().managerVertexConstants(38, m_tile, 1);
    }

private:
    Program* m_shadow;
    Program* m_noShadow;
    Program* m_night;
    const GameTexture* m_texture = nullptr;
    float m_scroll[4] = {};
    float m_tile[4] = {1.0f, 1.0f, 1.0f, 1.0f};  // +0x3C .. +0x48
};

// Water (id 46, vtable 0x006F9528): Material_Water (+0x24); Front-Opacity (+0x34, 1), Side-Opacity (+0x30), U-Tile
// (+0x38), V-Tile (+0x3C, 1); the texture (slot 0) and the noise (slot 1). Setup (0x0052C3D0): the pass, the default
// cull, fog on. Drawing (0x0052C410): the texture on stage 0; with the renderer's +0x164: it on stage 1, c54 = (0.002,
// 0.002, 0.002, a value growing 0.0005 a draw), pixel c2 = (sin g x 30 + 70, cos g x 40 + 60, 0, 0) with g a value
// growing 0.0348889 a draw (both kept for the whole game); without it stage 1 emptied; the noise on stage 2, the
// normal cube on stage 3, c52 = (0.5, 0.5, 1, 1), pixel c0 = (99, 99, wetness x side, wetness x front), c53 = (+0x38
// .. +0x44).
class Water final : public Material {
public:
    Water(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_Water")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        const float v = asFloat(value);
        if (index == 0) m_front = v;
        else if (index == 1) m_side = v;
        else if (index == 2) m_tile[0] = v;
        else if (index == 3) m_tile[1] = v;
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot == 0) m_texture = texture;
        else if (slot == 1) m_noise = texture;
        else return false;
        return true;
    }
    void setup(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull);
        m_r.device().setRenderState(RenderState::FogEnable, 1);
    }
    Program* program() override { return m_program; }
    void bind(uint32_t) override {
        m_r.bindTexture(m_texture, 0);
        if (m_r.reflection != nullptr) {
            m_r.bindTexture(m_r.reflection, 1);
            static float ripple[4] = {0.002f, 0.002f, 0.002f, 0.0f};  // 0x007A0B3C
            ripple[3] += 0.0005f;
            m_r.setVertexConstants(54, ripple, 1);
            static float phase = 0.0f;  // 0x0079E88C
            phase += 0.0348888896f;
            // The main thread's x87 rounds the product and the sum to floats.
            const float c2[4] = {static_cast<float>(std::sin(static_cast<double>(phase)) * 30.0) + 70.0f,
                                 static_cast<float>(std::cos(static_cast<double>(phase)) * 40.0) + 60.0f, 0.0f, 0.0f};
            m_r.device().setPixelShaderConstantF(2, c2, 1);
        } else {
            m_r.device().setTexture(1, 0);
        }
        m_r.bindTexture(m_noise, 2);
        m_r.bindTexture(m_r.normalCube, 3);
        const float c52[4] = {0.5f, 0.5f, 1.0f, 1.0f};
        m_r.setVertexConstants(52, c52, 1);
        const float c0[4] = {99.0f, 99.0f, m_r.roadWetness * m_side, m_r.roadWetness * m_front};
        m_r.device().setPixelShaderConstantF(0, c0, 1);
        m_r.setVertexConstants(53, m_tile, 1);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    const GameTexture* m_noise = nullptr;
    float m_side = 0.0f;
    float m_front = 1.0f;
    float m_tile[4] = {0.0f, 1.0f, 0.0f, 1.0f};  // +0x38 .. +0x44
};

// Blurred Shadow (id 34, 0x0052A590, vtable 0x006F935C), named by no material of the game's files:
// Material_BlurredShadow (+0x20); Offset as both +0x24 and +0x28 (+0x2C, +0x30 0); its texture (slot 0, +0x1C).
// Drawing (0x0052A680): the pass, the texture on stages 0 .. 3, vertex c52 = +0x24 .. +0x30 by the state manager. No
// setup.
class BlurredShadow final : public Material {
public:
    BlurredShadow(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_BlurredShadow")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index != 0) return;
        m_offset[0] = asFloat(value);
        m_offset[1] = m_offset[0];
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
        for (uint32_t stage = 0; stage < 4; ++stage) m_r.bindTexture(m_texture, stage);
        m_r.programs().managerVertexConstants(52, m_offset, 1);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    float m_offset[4] = {};
};

}  // namespace oj::render::materials
