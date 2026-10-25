#pragma once
// The material classes of the stock engine (docs/spec/11-effects.md, 12-rendering.md): an object per material of a
// model part, made by its class from the .pmd values, that picks a program and binds its textures and constants.

#include "Core/Data/ExeTables.h"
#include "Render/Program.h"
#include "Render/Renderer3d.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oj::render {

// The programs by name (`Material_<Name>`), each loaded once; nullptr when the effect has no usable technique.
using ProgramLookup = std::function<Program*(const std::string& name)>;

class Material {
public:
    Material(uint32_t classId, uint32_t flags, Renderer3d& renderer)
        : m_class(classId), m_flags(flags), m_r(renderer) {}
    virtual ~Material() = default;

    uint32_t classId() const { return m_class; }
    uint32_t flags() const { return m_flags; }  // the class record's flags

    // vtable + 0x00: a parameter by its index in the class; the value as the .pmd reader stores it (4 bytes, or a
    // string).
    virtual void setParam(uint32_t index, const void* value) = 0;
    // vtable + 0x04
    virtual bool setTexture(uint32_t slot, const GameTexture* texture) = 0;
    virtual const GameTexture* texture(uint32_t) const { return nullptr; }
    // vtable + 0x10: once per class and pass.
    virtual void setup(uint32_t pass) { (void)pass; }
    // vtable + 0x14: binds the textures, begins the program's pass, sets the constants.
    virtual void bind(uint32_t pass) = 0;
    // vtable + 0x18
    virtual Program* program() = 0;
    // vtable + 0x1C
    virtual uint32_t passes() { return 1; }

    // The material manager's description of the material (the manager's +0x14, 0x0048AED8): the parameters' values
    // in the class's order as its file gives them.
    const std::vector<std::vector<uint8_t>>& description() const { return m_description; }
    void setDescription(std::vector<std::vector<uint8_t>> values) { m_description = std::move(values); }

protected:
    // The current material is forgotten when its parameters change.
    void touched();
    void cull(Program* program, bool none);

    uint32_t m_class;
    uint32_t m_flags;
    Renderer3d& m_r;
    std::vector<std::vector<uint8_t>> m_description;
};

// A material of a class (the executable's class record), or nullptr for a class past the table's 53: the
// object its creator (the registry's +0x0C) makes; `tables` gives the record of the class whose object a creator makes
// for another class.
std::unique_ptr<Material> createMaterial(const ExeTables::MaterialClass& info, Renderer3d& renderer,
                                         const ProgramLookup& programs, const ExeTables& tables);

}  // namespace oj::render
