#pragma once
// The unlit materials: opaque, alpha tested and blended, the vertex coloured ones, the two textures' and the classes of
// no parameters.

#include "Render/Materials/MaterialsInternal.h"

#include <cfenv>
#include <cstdint>

namespace oj::render::materials {

// Unlit Opaque (id 14, vtable 0x006F98E0): Material_UnlitOpaque (+0x24); Double-sided (+0x20). Drawing
// (0x00530B80): the pass, the texture on stage 0, the cull. Setup (0x00529530): fog on.
class UnlitOpaque final : public Material {
public:
    UnlitOpaque(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_UnlitOpaque")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index == 0) m_doubleSided = asBits(value);
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
        cull(m_program, m_doubleSided != 0);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    uint32_t m_doubleSided = 0;
};

// Unlit ATest (id 29, vtable 0x006F97D8): Material_UnlitATest (+0x20), no parameters. Drawing (0x0052F870): the
// texture on stage 0, then the pass. Setup (0x0052F850): fog on.
class UnlitAlphaTest final : public Material {
public:
    UnlitAlphaTest(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_UnlitATest")) {}

    void setParam(uint32_t, const void*) override {}
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
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
};

// Unlit ABlend (id 15, vtable 0x006F9870): Material_UnlitABlend (+0x20); Disable Fog (+0x24). Drawing
// (0x005303A0): the pass, the texture on stage 0, FOGENABLE unless fog is disabled. Setup (0x0052A530): the default
// cull.
class UnlitAlphaBlend final : public Material {
public:
    UnlitAlphaBlend(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs)
        : Material(c.id, c.flags, r), m_program(programs("Material_UnlitABlend")) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index == 0) m_noFog = asBits(value);
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
        m_r.device().setRenderState(RenderState::FogEnable, m_noFog == 0 ? 1u : 0u);
    }

private:
    Program* m_program;
    const GameTexture* m_texture = nullptr;
    uint32_t m_noFog = 0;
};

// Vert-Col Alpha Test (id 21, 0x0052AE40, vtable 0x006F93DC): Material_VertColATest (+0x20), no parameters, the
// texture (slot 0, +0x1C). Setup (0x00529530): fog on. Drawing (0x0052AF30): the pass, the texture on stage 0.
// Vert-Col Alpha Blend (id 22, 0x00530C00, vtable 0x006F9904): Material_VertColABlend (+0x20); Additive (+0x24). Setup
// (0x00530CF0): the default cull, fog on. Drawing (0x00530D20): the pass, DESTBLEND ONE when additive, else
// INVSRCALPHA, the texture on stage 0.
class VertexColour final : public Material {
public:
    VertexColour(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs, bool blend)
        : Material(c.id, c.flags, r),
          m_program(programs(blend ? "Material_VertColABlend" : "Material_VertColATest")),
          m_blend(blend) {}

    void setParam(uint32_t index, const void* value) override {
        if (!m_blend) return;
        touched();
        if (index == 0) m_additive = asBits(value);
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override {
        if (m_blend) m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull);
        m_r.device().setRenderState(RenderState::FogEnable, 1);
    }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        if (m_blend) {
            m_r.device().setRenderState(RenderState::DestBlend,
                                        m_additive != 0 ? BlendFactor::One : BlendFactor::InvSrcAlpha);
        }
        m_r.bindTexture(m_texture, 0);
    }

private:
    Program* m_program;
    bool m_blend;
    const GameTexture* m_texture = nullptr;
    uint32_t m_additive = 0;
};

// 2-Texture Modulate (id 7, 0x00527B50, vtable 0x006F9074, Material_2TexModulate) and Vert-Col 2-Texture (id 23,
// 0x00529670, vtable 0x006F926C, Material_VertCol2Tex), named by no material of the game's files: the program at
// +0x34; X-Scale (+0x24) and Y-Scale (+0x28), +0x2C 0, +0x30 1 (the scales 1 until set); the textures of slots 0 and
// 1 (+0x1C, +0x20; 0x0052EE80). Drawing (0x00527CA0, 0x005297A0): the pass, (Vert-Col 2-Texture: the default cull,)
// the textures on stages 0 and 1, vertex c38 = +0x24 .. +0x30 by the state manager. Setup: the default cull
// (0x00527C80) / fog on (0x00529530). 2-Texture Modulate Specular (id 8, 0x0052EC00, vtable 0x006F970C,
// Material_2TexModulateSpec at +0x38; also no material of the game's files) adds Shininess (+0x34), Specular Colour
// (+0x3C, 0xFF000000 at first) and Specular Highlight (+0x40): drawn (0x0052ED60) as 2-Texture Modulate, then c39.w the
// shininess on the device and the renderer's specular (0x00515500); setup the default cull (0x0052ED40).
class TwoTexture final : public Material {
public:
    enum class Kind { Modulate, VertexColour, Specular };
    TwoTexture(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs, Kind kind)
        : Material(c.id, c.flags, r),
          // Stock problem: the stock asks for Material_2TexModulate(Spec), the game's files and the programs loaded
          // at the start are Material_2TexModulare(Spec), so the classes had no program. The fixed build asks for the
          // files' names (no material of the game's files is of these classes: nothing changes for the game itself).
          m_program(programs(kind == Kind::VertexColour ? "Material_VertCol2Tex"
                             : kind == Kind::Specular   ? "Material_2TexModulareSpec"
                                                        : "Material_2TexModulare")),
          m_vertexColour(kind == Kind::VertexColour),
          m_specular(kind == Kind::Specular) {}

    void setParam(uint32_t index, const void* value) override {
        touched();
        if (index == 0) {
            m_scale[0] = asFloat(value);
        } else if (index == 1) {
            m_scale[1] = asFloat(value);
        } else if (m_specular) {
            if (index == 2) m_shininess = asFloat(value);
            else if (index == 3) m_colour = asBits(value);
            else if (index == 4) m_highlight = asFloat(value);
        }
    }
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        touched();
        if (slot > 1) return false;
        m_textures[slot] = texture;
        return true;
    }
    void setup(uint32_t) override {
        if (m_vertexColour) {
            m_r.device().setRenderState(RenderState::FogEnable, 1);
        } else {
            m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull);
        }
    }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        if (m_vertexColour) m_r.device().setRenderState(RenderState::CullMode, m_r.defaultCull);
        m_r.bindTexture(m_textures[0], 0);
        m_r.bindTexture(m_textures[1], 1);
        m_r.programs().managerVertexConstants(38, m_scale, 1);
        if (!m_specular) return;
        // Stock problem: x, y, z of c39 are stack values of the stock code; 0 here.
        const float c39[4] = {0.0f, 0.0f, 0.0f, m_shininess};
        m_r.setVertexConstants(39, c39, 1);
        m_r.setSpecular(m_colour, m_highlight);
    }

private:
    Program* m_program;
    bool m_vertexColour;
    bool m_specular;
    float m_shininess = 0.0f;
    uint32_t m_colour = 0xFF000000;
    float m_highlight = 0.0f;
    const GameTexture* m_textures[2] = {};
    float m_scale[4] = {1.0f, 1.0f, 0.0f, 1.0f};
};

// Classes of no parameters that no material of the game's files names. Their programs are kept at +0x20 (+0x1C for
// the two Vertex Coloured classes), their texture of slot 0 at +0x1C. Drawing: the pass and the texture on stage 0
// (0x0052AF30); the Vertex Coloured classes have no texture (slot calls refused, 0x005285D0) and draw only the pass
// (0x0052B840). Setup, on the program's state manager:
// - Grass (id 12, 0x00529210, vtable 0x006F91E8, Material_Grass): the default cull (0x0052A530);
// - Vert-Col Opaque (id 20, 0x00529850, vtable 0x006F9290, Material_VertColOpaque): the default cull, fog on
//   (0x00529940);
// - Cloud Sprite (id 37, 0x0052A730, vtable 0x006F9388, Material_CloudSprite): CULLMODE none (0x0052A820);
// - Vertex Coloured (id 1, 0x0052B860) and Vertex Coloured Unlit (id 25, 0x0052B770), vtable 0x006F94AC,
//   Material_VertColoured / Material_VertColouredUnlit: the default cull, fog on (0x0052B920).
class PlainClass final : public Material {
public:
    enum class Setup { DefaultCull, DefaultCullFog, NoCull };
    PlainClass(const ExeTables::MaterialClass& c, Renderer3d& r, const ProgramLookup& programs, const char* program,
               bool textured, Setup setup)
        : Material(c.id, c.flags, r), m_program(programs(program)), m_textured(textured), m_setup(setup) {}

    void setParam(uint32_t, const void*) override {}
    bool setTexture(uint32_t slot, const GameTexture* texture) override {
        if (!m_textured) return false;
        touched();
        if (slot != 0) return false;
        m_texture = texture;
        return true;
    }
    void setup(uint32_t) override {
        IRenderDevice& d = m_r.device();
        d.setRenderState(RenderState::CullMode, m_setup == Setup::NoCull ? Cull::None : m_r.defaultCull);
        if (m_setup == Setup::DefaultCullFog) d.setRenderState(RenderState::FogEnable, 1);
    }
    Program* program() override { return m_program; }
    void bind(uint32_t pass) override {
        if (m_program != nullptr) m_program->beginPass(pass);
        if (m_textured) m_r.bindTexture(m_texture, 0);
    }

private:
    Program* m_program;
    bool m_textured;
    Setup m_setup;
    const GameTexture* m_texture = nullptr;
};

}  // namespace oj::render::materials
