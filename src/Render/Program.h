#pragma once
// A material program of the stock engine (docs/spec/11-effects.md): an effect of effects\ with its chosen technique,
// run on the render device the way the game's static effect library runs it under the game's state manager.

#include "Core/Formats/Effect.h"
#include "Render/RenderDevice.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace oj::render {

// The shader classes of a device (0x0051C060): 0x10000 fixed function, 0x30000 with pixel shaders 1.x and the
// Shaders setting at least 1, 0x70000 with 2.0 and at least 2, 0xF0000 with vertex and pixel shaders 3.0 and at
// least 3.
uint32_t shaderClasses(uint32_t vertexShaderVersion, uint32_t pixelShaderVersion, uint32_t shadersSetting);

class Program;

// What the programs share: the device, the values of the shared parameters (the effect pool), the program whose
// pass is open, the engine's copy of the vertex shader constants and the fixed-function material and lights the
// passes change member by member.
class ProgramContext {
public:
    ProgramContext(IRenderDevice& device, uint32_t classes);

    IRenderDevice& device() { return m_device; }
    uint32_t classes() const { return m_classes; }

    // The engine's vertex constant write (0x00504C60): its copy, then the device.
    void setVertexConstants(uint32_t start, const float* data, uint32_t count);
    const float* vertexConstants() const { return m_vertexConstants.data(); }

    // Ends the open pass of the current program (0x0051BDA0).
    void endCurrent();

    // The game's state manager (0x00722EB4): every call goes to the device as it is, but a vertex shader constant
    // block of more than 29 registers is dropped.
    void managerVertexConstants(uint32_t start, const float* data, uint32_t count);

    Program* current = nullptr;
    std::map<std::string, std::vector<uint8_t>> shared;
    FixedMaterial material;
    std::array<FixedLight, 8> lights{};

private:
    IRenderDevice& m_device;
    uint32_t m_classes;
    std::vector<float> m_vertexConstants;  // 256 registers
};

class Program {
public:
    // A texture parameter of the effect with annotations (0x0051AAC0, its types 5 .. 9): the file of its name or
    // resourcename annotation (the last one; empty when that one is not a string), else a procedural texture of the
    // effect's function (TextureType: 7 texture, 8 volume, 9 cube; width and height 16, depth and miplevels 1 unless
    // given; format read and left).
    struct TextureRequest {
        std::string parameter;
        std::string file;
        std::string function;  // empty: none (a null name in the stock game)
        uint32_t width = 16;
        uint32_t height = 16;
        uint32_t depth = 1;
        uint32_t mipLevels = 1;
        uint32_t format = 0x15;  // D3DFMT_A8R8G8B8, never changed
        uint32_t type = 7;
    };
    // The texture of a request, 0 when there is none.
    using TextureLoader = std::function<TextureId(const Program& program, const TextureRequest& request)>;

    Program(ProgramContext& context, std::string name, fx::Effect effect, TextureLoader textures);

    const std::string& name() const { return m_name; }
    // A technique is in use (the stock game keeps a program without one, the materials then leave it out).
    bool usable() const { return m_technique >= 0; }
    const fx::Effect& effect() const { return m_effect; }
    int technique() const { return m_technique; }
    bool useConstantMemory() const { return m_constantMemory; }
    // The chosen technique's USEFFV (the vertices go through the fixed-function transforms) and USERANDOM (c40 is
    // the object's random value) annotations: +0x24 and +0x28 of the technique record.
    bool usesFixedVertices() const {
        return m_technique >= 0 && m_techniques[static_cast<size_t>(m_technique)].useFfv;
    }
    bool usesRandom() const { return m_technique >= 0 && m_techniques[static_cast<size_t>(m_technique)].useRandom; }
    // The passes of the chosen technique (its record's +0x08).
    uint32_t passCount() const {
        return m_technique >= 0
                   ? static_cast<uint32_t>(m_effect.techniques[static_cast<size_t>(m_technique)].passes.size())
                   : 0;
    }

    // 0x0051BCD0: ends the current program's pass, then Begin and BeginPass(pass).
    void beginPass(uint32_t pass);
    // 0x0051BDA0: EndPass and End when a pass is open; a technique with USECONSTANTMEM puts the engine's vertex
    // constants c0 .. c51 back.
    void end();

    // Parameter values (all the parameter's bytes, members in order); a shared parameter changes for all programs.
    bool setValue(const std::string& parameter, const void* data, size_t bytes);

    // One round of 0x0051B620: the program made again from its effect (0x0051B7D0): its pass ended, its values the
    // effect's again (the shared ones stay in the pool), its techniques' records anew and the last usable one chosen
    // (0x0051A0B0). True when it has a usable technique.
    bool recreate();
    // 0x00519FF0: the technique of a level (0 .. 3: the masks 0x10000, 0x20000, 0x40000, 0x80000 of 0x00719364):
    // the pass ended, then from the last technique back the first usable one whose CLASSIFICATION has the mask
    // (printed through the debug print, "(a)%s %s %x %x"), else the last usable one; false without one.
    bool chooseLevel(uint32_t level);

private:
    struct Technique {
        uint32_t classification = 0;
        bool useRandom = false;
        bool useFfv = false;
        bool constantMemory = false;
        bool condition = true;
        bool usable = false;
    };

    void loadTextures();
    void chooseTechnique();
    void select(int technique);
    void createShaders();
    std::vector<uint8_t>& valueOf(size_t parameter);
    const fx::Parameter* findParameter(const std::string& name, size_t* index) const;
    void applyState(const fx::State& state, uint32_t index);
    void applySampler(const fx::Parameter& sampler, uint32_t index);
    void setShaderConstants(uint32_t object, bool vertex);
    uint32_t stateValue(const fx::State& state, std::vector<float>* values);
    TextureId textureOf(const std::string& parameter);
    void fillInputs(const fx::ConstantTable& table, std::vector<float>& input);

    ProgramContext& m_context;
    std::string m_name;
    fx::Effect m_effect;
    TextureLoader m_loader;
    std::vector<std::vector<uint8_t>> m_values;     // by parameter, unless shared
    std::vector<Technique> m_techniques;
    int m_technique = -1;
    bool m_constantMemory = false;
    std::map<uint32_t, ShaderId> m_shaders;          // by object
    std::map<uint32_t, fx::ConstantTable> m_tables;  // by object
    std::map<uint32_t, fx::Program> m_preshaders;    // by object
    std::map<std::string, TextureId> m_textures;     // by texture parameter
    bool m_open = false;
};

// The effects' manager (0x24 bytes, 0x00519D00, [0x0075B294]; made by the material manager 0x005226E0): the
// programs made at its start (0x00519840), in that order, and its request (+0x20).
class ProgramManager {
public:
    void add(Program* program) { m_programs.push_back(program); }
    const std::vector<Program*>& programs() const { return m_programs; }
    // 0x00519CB0: the program of a name, case aside; nullptr when it has none.
    Program* find(const std::string& name) const;
    // 0x00519F70: every program's open pass ended (its EndPass and End).
    void endAll();
    // 0x00519DA0: endAll, then each program's technique of the level (0x00519FF0).
    void chooseLevel(uint32_t level);
    // 0x00519E80, each frame of the renderer after endAll (0x00514C61); see the .cpp.
    void frame(uint32_t classes, float detail);
    // 0x00519700, after each reset of the device (0x005142F3); see the .cpp.
    void restore(uint32_t classes, float detail);

private:
    std::vector<Program*> m_programs;  // +0x00 .. +0x10
    uint32_t m_request = 0x1000;       // +0x20: 0x1000 none (0x00519D7F); nothing asks another
    bool m_highDetail = true;          // [0x00725784]: the detail was over 0.1 at the last frame; 1 at the start
};

}  // namespace oj::render
