#pragma once
// The standard lit materials: opaque, alpha tested and blended, specular, shiny, chrome, coloured specular, the fade-
// out and the constant colours.

#include "Render/Materials/MaterialsInternal.h"

#include "Core/Base/X87.h"

#include <cfenv>
#include <cstdint>

namespace oj::render::materials {

// Standard Opaque (id 3, 0x0052CB70).
class StandardOpaque final : public Material {
public:
    StandardOpaque(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r),
          m_shad(programs("Material_StdOpaqueShad")),
          m_noShad(programs("Material_StdOpaqueNoShad")),
          m_night(programs("Material_StdOpaqueShadNight")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index == 0) m_doubleSided = asBits(value);
        else if (index == 1) m_zBias = x87::chop(asFloat(value) * 16.0f);
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override { m_r.device().setRenderState(RenderState::FogEnable, 1); }
    Program* program() override {
        if (m_r.lightTile == nullptr) return m_noShad;
        return m_r.night ? m_night : m_shad;
    }
    void bind(uint32_t pass) override {
        m_r.bindTexture(m_texture, 0);
        if (m_r.lightTile != nullptr) m_r.bindTexture(m_r.lightTile, 1);
        if (Program* p = program()) p->beginPass(pass);
        const Vec4& a = m_r.ambient;
        const float c0[4] = {a[0] * kOver511, a[1] * kOver511, a[2] * kOver511, a[3]};
        m_r.device().setPixelShaderConstantF(0, c0, 1);
        cull(m_shad, m_doubleSided != 0);
    }

private:
    Program* m_shad;
    Program* m_noShad;
    Program* m_night;
    const GameTexture* m_texture = nullptr;
    uint32_t m_doubleSided = 0;
    int32_t m_zBias = 0;  // kept, not used when drawing
};

// Standard Alpha Test (id 5, 0x0052B4A0).
class StandardAlphaTest final : public Material {
public:
    StandardAlphaTest(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r),
          m_shadow(programs("Material_StdATestShadow")),
          m_noShadow(programs("Material_StdATestNoShadow")),
          m_night(programs("Material_StdATestShadowNight")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index == 0) m_singleSided = asBits(value);
        else if (index == 1) m_zBias = x87::chop(asFloat(value) * 16.0f);
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override { m_r.device().setRenderState(RenderState::FogEnable, 1); }
    Program* program() override {
        if (m_r.lightTile == nullptr) return m_noShadow;
        return m_r.night ? m_night : m_shadow;
    }
    void bind(uint32_t pass) override {
        m_r.bindTexture(m_texture, 0);
        if (m_r.lightTile != nullptr) m_r.bindTexture(m_r.lightTile, 1);
        if (Program* p = program()) p->beginPass(pass);
        const Vec4& a = m_r.ambient;
        const float c0[4] = {a[0] * kOver511, a[1] * kOver511, a[2] * kOver511, a[3]};
        m_r.device().setPixelShaderConstantF(0, c0, 1);
        cull(m_shadow, m_singleSided == 0);
    }

private:
    Program* m_shadow;
    Program* m_noShadow;
    Program* m_night;
    const GameTexture* m_texture = nullptr;
    uint32_t m_singleSided = 0;
    int32_t m_zBias = 0;
};

// Specular Opaque (id 6, 0x0052B270) and SpecMap Opaque (id 44, 0x00530710): one program, a specular colour and
// highlight, the shininess in c39.w; SpecMap adds the specular map on stage 1. Specular ATest (id 43, 0x0052E400,
// vtable 0x006F96BC, Material_SpecATest; drawing 0x0052E500) and SpecMap ATest (id 45, 0x0052F570, vtable
// 0x006F979C, Material_SpecMapATest; drawing 0x0052F6C0), named by no material of the game's files, are the same
// without Z-Bias; Specular ATest begins its pass first.
class Specular final : public Material {
public:
    Specular(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs, bool map,
             bool alphaTest = false)
        : Material(c.id, c.flags, r),
          m_program(programs(alphaTest ? (map ? "Material_SpecMapATest" : "Material_SpecATest")
                                       : (map ? "Material_SpecMapOpaque" : "Material_SpecOpaque"))),
          m_map(map),
          m_alphaTest(alphaTest) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        switch (index) {
            case 0: m_shininess = asFloat(value); break;
            case 1: m_colour = asBits(value); break;
            case 2: m_highlight = asFloat(value); break;
            case 3: m_doubleSided = asBits(value); break;
            case 4:
                if (!m_map && !m_alphaTest) m_zBias = x87::chop(asFloat(value) * 16.0f);
                break;
            default: break;
        }
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot == 0) m_texture = texture;
        else if (slot == 1 && m_map) m_specularMap = texture;
        else return false;
        return true;
    }
    void setup(uint32_t) override { m_r.device().setRenderState(RenderState::FogEnable, 1); }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        const bool passFirst = m_alphaTest && !m_map;
        if (passFirst && m_program != nullptr) m_program->beginPass(pass);
        m_r.bindTexture(m_texture, 0);
        if (m_map) m_r.bindTexture(m_specularMap, 1);
        // Stock problem: x, y, z of c39 are stack values of the stock code; 0 here.
        const float c39[4] = {0.0f, 0.0f, 0.0f, m_shininess};
        m_r.setVertexConstants(39, c39, 1);
        cull(m_program, m_doubleSided != 0);
        m_r.setSpecular(m_colour, m_highlight);
        if (!passFirst && m_program != nullptr) m_program->beginPass(pass);
    }

private:
    Program* m_program;
    bool m_map;
    bool m_alphaTest;
    const GameTexture* m_texture = nullptr;
    const GameTexture* m_specularMap = nullptr;
    float m_shininess = 0.0f;
    uint32_t m_colour = 0xFF000000;
    float m_highlight = 0.0f;
    uint32_t m_doubleSided = 0;
    int32_t m_zBias = 0;
};

// Standard Alpha Blend (id 4, vtable 0x006F91A0): Material_StdABlend (+0x20); Z-Write (+0x24), Disable Fog (+0x28),
// Z-Bias (+0x2C, x 16 rounded, kept). Drawing (0x00528FB0): the pass, ZWRITEENABLE = Z-Write, the texture on stage 0,
// FOGENABLE unless fog is disabled. Setup (0x0052A530): the default cull.
class StandardAlphaBlend final : public Material {
public:
    StandardAlphaBlend(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_StdABlend")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index == 0) m_zWrite = asBits(value);
        else if (index == 1) m_noFog = asBits(value);
        else if (index == 2) m_zBias = x87::chop(asFloat(value) * 16.0f);
    }
    const GameTexture* texture(uint32_t slot) const override { return slot == 0 ? m_texture : nullptr; }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override { m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull); }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        m_r.device().setRenderState(RenderState::ZWriteEnable, m_zWrite);
        m_r.bindTexture(m_texture, 0);
        m_r.device().setRenderState(RenderState::FogEnable, m_noFog == 0 ? 1u : 0u);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    uint32_t m_zWrite = 0;
    uint32_t m_noFog = 0;
    int32_t m_zBias = 0;  // kept, not used when drawing
};

// Constant Colour (id 2, vtable 0x006F9124): Material_ConstColour (+0x1C); Colour as r, g, b / 255 (+0x20 ..
// +0x28; +0x2C never set, 0); no textures. Drawing (0x005286B0): the pass, the default cull, TEXTUREFACTOR = the
// four values x 255 rounded (alpha from +0x2C). No setup.
class ConstantColour final : public Material {
public:
    ConstantColour(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_ConstColour")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index != 0) return;
        const uint32_t argb = asBits(value);
        m_colour[0] = static_cast<float>((argb >> 16) & 0xFF) * kOver255;
        m_colour[1] = static_cast<float>((argb >> 8) & 0xFF) * kOver255;
        m_colour[2] = static_cast<float>(argb & 0xFF) * kOver255;
    }
    bool setTexture(uint32_t, const GameTexture*) override { return false; }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull);
        auto byteOf = [](float v) { return static_cast<uint32_t>(roundNearest(v * 255.0f)) & 0xFF; };
        const uint32_t factor = (byteOf(m_colour[3]) << 24) | (byteOf(m_colour[0]) << 16) |
                                (byteOf(m_colour[1]) << 8) | byteOf(m_colour[2]);
        m_r.device().setRenderState(RenderState::TextureFactor, factor);
    }

private:
    Program* m_program;
    float m_colour[4] = {};
};

// AmbientLit (id 38, vtable 0x006F93B8): Material_AmbientLit (+0x20); Ambient 4x Mult as r, g, b / 64 and a / 255
// (+0x24 .. +0x30). Drawing (0x0052AD40): the pass, the texture on stage 0, vertex c52 = those four values on the
// state manager. No setup.
class AmbientLit final : public Material {
public:
    AmbientLit(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_AmbientLit")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index != 0) return;
        const uint32_t argb = asBits(value);
        m_multiplier[0] = static_cast<float>((argb >> 16) & 0xFF) * kOver64;
        m_multiplier[1] = static_cast<float>((argb >> 8) & 0xFF) * kOver64;
        m_multiplier[2] = static_cast<float>(argb & 0xFF) * kOver64;
        m_multiplier[3] = static_cast<float>(argb >> 24) * kOver255;
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
        m_r.programs().managerVertexConstants(52, m_multiplier, 1);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    // Stock problem: the object leaves these unset until the parameter comes (the memory it was made in); 0 here.
    float m_multiplier[4] = {};
};

// Fade To Const Colour (id 36, 0x0052A180, vtable 0x006F930C): Material_FadeToConstColour (+0x20); Colour as r, g, b,
// a / 255 (+0x28 .. +0x34; 0, 0, 0, 1 at first; read by nothing found), Additive (+0x24); the texture of slot 0
// (+0x1C). Drawing (0x0052A280): the pass, the texture on stage 0, the default cull; additive: ALPHATESTENABLE off,
// blend ONE, ONE; else ALPHATESTENABLE on, SRCALPHA, INVSRCALPHA. No setup. (Its only material, heathaze01, is the
// FRAMEBUFFER effect's, which is never drawn.)
class FadeToConstColour final : public Material {
public:
    FadeToConstColour(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_FadeToConstColour")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index == 1) {
            m_additive = asBits(value) != 0;
            return;
        }
        if (index != 0) return;
        const uint32_t argb = asBits(value);
        m_colour[0] = static_cast<float>((argb >> 16) & 0xFF) * kOver255;
        m_colour[1] = static_cast<float>((argb >> 8) & 0xFF) * kOver255;
        m_colour[2] = static_cast<float>(argb & 0xFF) * kOver255;
        m_colour[3] = static_cast<float>(argb >> 24) * kOver255;
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
        IRenderDevice& d = m_r.device();
        d.setRenderState(RenderState::CullMode, m_r.defaultCull);
        d.setRenderState(RenderState::AlphaTestEnable, m_additive ? 0 : 1);
        d.setRenderState(RenderState::SrcBlend, m_additive ? BlendFactor::One : BlendFactor::SrcAlpha);
        d.setRenderState(RenderState::DestBlend, m_additive ? BlendFactor::One : BlendFactor::InvSrcAlpha);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    bool m_additive = false;
    float m_colour[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

// Shiny Opaque (id 24, vtable 0x006F95B8) and Shiny Unlit Opaque (id 39, vtable 0x006F962C): Material_ShinyOpaque /
// Material_ShinyUnlitOpaque (+0x20); Side Fresnel (+0x40), Front Fresnel (+0x30); Z-Bias not kept. Drawing (0x0052D5A0,
// 0x0052D760): the pass, the texture on stage 0, the renderer's environment texture (+0x11C) on stage 1, pixel c0, c1
// = +0x24 .. +0x40. Setup: none (Shiny Opaque), the default cull (Shiny Unlit Opaque). Shiny Unlit ATest (id 40,
// 0x0052A440, vtable 0x006F9338, Material_ShinyUnlitATest), which no material of the game's files names, is Shiny
// Unlit Opaque with its own program (setup 0x0052A530, drawing 0x0052D5A0).
class Shiny final : public Material {
public:
    Shiny(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs, bool unlit,
          const char* name = nullptr)
        : Material(c.id, c.flags, r),
          m_program(programs(name != nullptr ? name : unlit ? "Material_ShinyUnlitOpaque" : "Material_ShinyOpaque")),
          m_unlit(unlit) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index == 0) m_c0c1[7] = asFloat(value);
        else if (index == 1) m_c0c1[3] = asFloat(value);
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override {
        if (m_unlit) m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull);
    }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        m_r.bindTexture(m_texture, 0);
        m_r.bindTexture(m_r.environment, 1);
        // Stock problem: x, y, z of c0 and c1 are fields the object never sets; 0 here.
        m_r.device().setPixelShaderConstantF(0, m_c0c1, 2);
    }

private:
    Program* m_program;
    bool m_unlit;
    const GameTexture* m_texture = nullptr;
    float m_c0c1[8] = {};  // +0x24 .. +0x40
};

// Chrome (id 26, vtable 0x006F9778): Material_Chrome (+0x1C); Colour as r, g, b, a / 255 (+0x24); Z-Bias not kept; its
// texture (slot 0) at +0x20. Drawing (0x0052F390): the pass, the renderer's environment texture (+0x118) on stage 0;
// without a texture stages 1 .. 3 emptied, with one stages 2, 3 emptied and the texture on stage 1; pixel c0 = the
// colour. No setup.
class Chrome final : public Material {
public:
    Chrome(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_Chrome")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index != 0) return;
        const uint32_t argb = asBits(value);
        m_colour[0] = static_cast<float>((argb >> 16) & 0xFF) * kOver255;
        m_colour[1] = static_cast<float>((argb >> 8) & 0xFF) * kOver255;
        m_colour[2] = static_cast<float>(argb & 0xFF) * kOver255;
        m_colour[3] = static_cast<float>(argb >> 24) * kOver255;
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
        m_r.bindTexture(m_r.dynamicEnvironment, 0);
        for (uint32_t stage = m_texture == nullptr ? 1u : 2u; stage < 4; ++stage) m_r.device().setTexture(stage, 0);
        if (m_texture != nullptr) m_r.bindTexture(m_texture, 1);
        m_r.device().setPixelShaderConstantF(0, m_colour, 1);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    float m_colour[4] = {};
};

// Standard Fade-Out (id 32, vtable 0x006F9820): Material_StdFadeOut (+0x24); Double-sided (+0x20), Fade Start (+0x28),
// Fade End (+0x2C), +0x30 = 1 / (end - start). Drawing (0x0052FE30): the pass, the texture, pixel c0 = the ambient /
// 511 (its fourth value kept), vertex c52 = (start, end, 1 / (end - start), 0), the cull. Setup (0x00529530): fog on.
class StandardFadeOut final : public Material {
public:
    StandardFadeOut(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_StdFadeOut")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        const float v = asFloat(value);
        if (index == 0) {
            m_doubleSided = asBits(value);
        } else if (index == 1) {
            m_fade[0] = v;
            m_fade[2] = 1.0f / (m_fade[1] - v);
        } else if (index == 2) {
            m_fade[1] = v;
            m_fade[2] = 1.0f / (v - m_fade[0]);
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
        if (m_program != nullptr) m_program->beginPass(pass);
        m_r.bindTexture(m_texture, 0);
        const Vec4& a = m_r.ambient;
        const float c0[4] = {a[0] * kOver511, a[1] * kOver511, a[2] * kOver511, a[3]};
        m_r.device().setPixelShaderConstantF(0, c0, 1);
        m_r.programs().managerVertexConstants(52, m_fade, 1);
        cull(m_program, m_doubleSided != 0);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    uint32_t m_doubleSided = 0;
    float m_fade[4] = {};  // +0x28 .. +0x34
};

// Coloured Spec Opaque (id 27, vtable 0x006F9894) and Coloured Spec ABlend (id 28, vtable 0x006F954C): Colour (r, g,
// b, a / 255), Specular Colour, Specular Highlight, Shininess; the opaque one Z-Bias (kept), the blend one Additive.
// Setup (0x0052C7B0): the default cull. Drawing (0x005304E0, 0x0052C7E0): the pass and the texture on stage 0; pixel
// c0 = the colour (TEXTUREFACTOR for a fixed-function technique); the additive blend through Material_ColouredSpec-
// ABlendAdditive with stage 0's sampler states linear, linear, point, wrap, wrap; c39.w = the shininess; the
// specular colour and highlight.
class ColouredSpec final : public Material {
public:
    ColouredSpec(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs, bool blend)
        : Material(c.id, c.flags, r),
          m_program(programs(blend ? "Material_ColouredSpecABlend" : "Material_ColouredSpecOpaque")),
          m_additiveProgram(blend ? programs("Material_ColouredSpecABlendAdditive") : nullptr),
          m_blend(blend) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        const uint32_t argb = asBits(value);
        switch (index) {
            case 0:
                m_colour[0] = static_cast<float>((argb >> 16) & 0xFF) * kOver255;
                m_colour[1] = static_cast<float>((argb >> 8) & 0xFF) * kOver255;
                m_colour[2] = static_cast<float>(argb & 0xFF) * kOver255;
                m_colour[3] = static_cast<float>(argb >> 24) * kOver255;
                break;
            case 1: m_specular = argb; break;
            case 2: m_highlight = asFloat(value); break;
            case 3: m_shininess = asFloat(value); break;
            case 4:
                if (m_blend) m_additive = argb;
                else m_zBias = x87::chop(asFloat(value) * 16.0f);
                break;
            default: break;
        }
    }
    const GameTexture* texture(uint32_t slot) const override { return slot == 0 ? m_texture : nullptr; }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override { m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull); }
    Program* program() override { return m_additive != 0 ? m_additiveProgram : m_program; }
    void bind(uint32_t pass) override {
        Program* p = program();
        const bool fixed = p != nullptr && p->usesFixedVertices();
        if (p != nullptr) p->beginPass(pass);
        if (m_additive != 0) {
            m_r.bindTexture(m_texture, 0);
            if (fixed) {
                m_r.device().setRenderState(RenderState::TextureFactor, factorOf(m_colour));
                for (uint32_t stage : {1u, 2u, 3u}) m_r.bindTexture(m_texture, stage);
            } else {
                m_r.device().setPixelShaderConstantF(0, m_colour, 1);
                static const SamplerParams kAdditive{2, 2, 1, 1, 1, 0.0f};
                m_r.applySampler(kAdditive, 0);
            }
        } else if (m_blend) {
            m_r.device().setPixelShaderConstantF(0, m_colour, 1);
            m_r.device().setRenderState(RenderState::TextureFactor, factorOf(m_colour));
            m_r.bindTexture(m_texture, 0);
        } else {
            m_r.bindTexture(m_texture, 0);
            if (fixed) m_r.device().setRenderState(RenderState::TextureFactor, factorOf(m_colour));
            else m_r.device().setPixelShaderConstantF(0, m_colour, 1);
        }
        // Stock problem: x, y, z of c39 are values left on the stack; 0 here.
        const float c39[4] = {0.0f, 0.0f, 0.0f, m_shininess};
        m_r.setVertexConstants(39, c39, 1);
        m_r.setSpecular(m_specular, m_highlight);
    }

private:
    Program* m_program;
    Program* m_additiveProgram;
    bool m_blend;
    const GameTexture* m_texture = nullptr;
    float m_colour[4] = {};
    uint32_t m_specular = 0xFF000000;
    float m_highlight = 0.0f;
    float m_shininess = 0.0f;
    uint32_t m_additive = 0;
    int32_t m_zBias = 0;  // kept
};

}  // namespace oj::render::materials
