#pragma once
// The cars' materials: the body's paint, glass and the car's glass.

#include "Render/Materials/MaterialsInternal.h"

#include "Core/Base/X87.h"

#include <algorithm>
#include <bit>
#include <cfenv>
#include <cstdint>

namespace oj::render::materials {

// Car Body (id 0, vtable 0x006F94D8): Material_CarBody_Paint_NoReflect (+0x1C; the paint work on stage 0, the scratches
// the scratch texture's alpha x the vertex colour's alpha) with a paint work texture, else Material_CarBodyBase (+0x20;
// the scratch texture's alpha alone) (the constructor's lookups at 0x0052BA06, 0x0052BA28; 0x0052BAF0). Car Colour
// (+0x2C, r, g, b, a / 255), Metallic Colour (+0x3C), Pearlescent Colour (+0x6C), the shininesses (metallic +0x7C,
// pearlescent +0x78), the fresnels (front +0x58, side +0x68), Metallic (+0x80), Double-sided (+0x84), Z-Bias (+0x88,
// kept). With Metallic the metallic colour goes to +0x4C and the car colour x 0.8 to +0x5C; without it 0 and the car
// colour. Drawing (0x0052BB00): the renderer's +0x118 on stage 2, the scratch & reflect texture on stage 1, c55 = 0,
// the paint work on stage 0, the pass, pixel c1, c2 = +0x4C .. +0x68, c52, c53 = +0x5C .. +0x78, c39.w = the metallic
// shininess, ZENABLE on, the cull.
class CarBody final : public Material {
public:
    CarBody(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r),
          m_paintProgram(programs("Material_CarBody_Paint_NoReflect")),
          m_base(programs("Material_CarBodyBase")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        const uint32_t argb = asBits(value);
        auto rgb = [argb](float* out) {
            out[0] = static_cast<float>((argb >> 16) & 0xFF) * kOver255;
            out[1] = static_cast<float>((argb >> 8) & 0xFF) * kOver255;
            out[2] = static_cast<float>(argb & 0xFF) * kOver255;
        };
        switch (index) {
            case 0:
                rgb(m_colour);
                m_colour[3] = static_cast<float>(argb >> 24) * kOver255;
                mix();
                break;
            case 1: m_c[7] = asFloat(value); break;   // +0x68
            case 2: m_c[3] = asFloat(value); break;   // +0x58
            case 3:
                rgb(m_metallicColour);
                if (m_metallic != 0) std::copy_n(m_metallicColour, 3, m_c);
                break;
            case 4: rgb(m_c + 8); break;               // +0x6C
            case 5: m_shininess = asFloat(value); break;  // +0x7C
            case 6: m_c[11] = asFloat(value); break;  // +0x78
            case 7:
                m_metallic = argb;
                mix();
                break;
            case 8: m_doubleSided = argb; break;
            case 9: m_zBias = x87::chop(asFloat(value) * 16.0f); break;
            default: break;
        }
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot == 0) m_paint = texture;
        else if (slot == 1) m_scratch = texture;
        else return false;
        return true;
    }
    Program* program() override { return m_paint != nullptr ? m_paintProgram : m_base; }
    void bind(uint32_t pass) override {
        m_r.bindTexture(m_r.dynamicEnvironment, 2);
        m_r.bindTexture(m_scratch, 1);
        const float c55[4] = {};
        m_r.programs().managerVertexConstants(55, c55, 1);
        if (m_paint != nullptr) m_r.bindTexture(m_paint, 0);
        if (Program* p = program()) p->beginPass(pass);
        m_r.device().setPixelShaderConstantF(1, m_c, 2);
        m_r.programs().managerVertexConstants(52, m_c + 4, 2);
        // Stock problem: x, y, z of c39 are values left on the stack; 0 here.
        const float c39[4] = {0.0f, 0.0f, 0.0f, m_shininess};
        m_r.setVertexConstants(39, c39, 1);
        m_r.device().setRenderState(RenderState::ZEnable, 1);
        cull(m_base, m_doubleSided != 0);
    }

private:
    // The metallic colour (or 0) to +0x4C and the car colour (x 0.8 when metallic) to +0x5C.
    void mix() {
        const bool metallic = std::bit_cast<float>(m_metallic) != 0.0f;
        for (int i = 0; i < 3; ++i) {
            m_c[i] = metallic ? m_metallicColour[i] : 0.0f;
            m_c[4 + i] = metallic ? m_colour[i] * 0.8f : m_colour[i];
        }
    }

    Program* m_paintProgram;  // +0x1C
    Program* m_base;          // +0x20
    const GameTexture* m_paint = nullptr;
    const GameTexture* m_scratch = nullptr;
    float m_colour[4] = {};          // +0x2C
    float m_metallicColour[3] = {};  // +0x3C
    float m_c[12] = {};              // +0x4C .. +0x78
    float m_shininess = 0.0f;        // +0x7C
    uint32_t m_metallic = 0;         // +0x80
    uint32_t m_doubleSided = 0;      // +0x84
    int32_t m_zBias = 0;             // +0x88, kept
};

// Glass (id 9, vtable 0x006F9608): Material_Glass (+0x20); Side Fresnel (+0x40), Front Fresnel (+0x30), Z-Bias (+0x44,
// kept). Drawing as Shiny Opaque (0x0052D5A0); setup: the default cull.
class Glass final : public Material {
public:
    Glass(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_Glass")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index == 0) m_c0c1[7] = asFloat(value);
        else if (index == 1) m_c0c1[3] = asFloat(value);
        else if (index == 2) m_zBias = x87::chop(asFloat(value) * 16.0f);
    }
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
        m_r.bindTexture(m_texture, 0);
        m_r.bindTexture(m_r.environment, 1);
        // Stock problem: x, y, z of c0 and c1 are fields the object never sets; 0 here.
        m_r.device().setPixelShaderConstantF(0, m_c0c1, 2);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    float m_c0c1[8] = {};
    int32_t m_zBias = 0;  // kept
};

// Car Glass (id 49, vtable 0x006F984C): Material_CarGlass (+0x24); Side Fresnel (+0x44, 1), Front Fresnel (+0x34, 1),
// Base Opacity (+0x54), Start Fade (+0x48), End Fade (+0x50; +0x4C = 1 / (end - start)), Colour (+0x28, r, g, b / 255;
// 0.5 grey at first); the texture and the scratch. Setup (0x005300B0): the default cull. Drawing (0x005300D0): the
// pass, the texture on stage 0, the scratch on stage 2, the renderer's +0x118 on stage 1; a program of fixed vertices
// takes TEXTUREFACTOR only, else pixel c0 = (colour, front), c1 = (0, 0, 0, side), c52 = (start, 1 / (end - start),
// end, opacity).
class CarGlass final : public Material {
public:
    CarGlass(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_CarGlass")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        const float v = asFloat(value);
        const uint32_t argb = asBits(value);
        switch (index) {
            case 0: m_c[7] = v; break;
            case 1: m_c[3] = v; break;
            case 2: m_fade[3] = v; break;
            case 3:
                m_fade[0] = v;
                m_fade[1] = 1.0f / (m_fade[2] - v);
                break;
            case 4:
                m_fade[2] = v;
                m_fade[1] = 1.0f / (v - m_fade[0]);
                break;
            case 5:
                m_c[0] = static_cast<float>((argb >> 16) & 0xFF) * kOver255;
                m_c[1] = static_cast<float>((argb >> 8) & 0xFF) * kOver255;
                m_c[2] = static_cast<float>(argb & 0xFF) * kOver255;
                break;
            default: break;
        }
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot == 0) m_texture = texture;
        else if (slot == 1) m_scratch = texture;
        else return false;
        return true;
    }
    void setup(uint32_t) override { m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull); }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        m_r.bindTexture(m_texture, 0);
        m_r.bindTexture(m_scratch, 2);
        m_r.bindTexture(m_r.dynamicEnvironment, 1);
        if (m_program != nullptr && m_program->usesFixedVertices()) {
            // 0x00530133: TEXTUREFACTOR grey (0x80) with the alpha the low byte of round(the low byte of
            // round(opacity x 120) + 80).
            const auto low = static_cast<uint32_t>(roundNearest(m_fade[3] * 120.0f)) & 0xFF;
            const auto alpha = static_cast<uint32_t>(roundNearest(static_cast<float>(low) + 80.0f)) & 0xFF;
            m_r.device().setRenderState(RenderState::TextureFactor, (alpha << 24) | 0x00808080u);
            return;
        }
        m_r.device().setPixelShaderConstantF(0, m_c, 2);
        m_r.programs().managerVertexConstants(52, m_fade, 1);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    const GameTexture* m_scratch = nullptr;
    float m_c[8] = {0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};  // +0x28 .. +0x44
    float m_fade[4] = {};  // +0x48 .. +0x54
};

}  // namespace oj::render::materials
