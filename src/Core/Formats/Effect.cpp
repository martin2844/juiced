#include "Core/Formats/Effect.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace oj::fx {

namespace {

constexpr uint32_t kTag = 0xFEFF0901;
constexpr uint32_t kNone = 0xFFFFFFFF;

// Reads 32-bit words at offsets of the effect's data (the file after the 8-byte header).
class Data {
public:
    explicit Data(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

    bool ok() const { return m_ok; }
    size_t size() const { return m_size; }

    uint32_t at(size_t offset) {
        if (offset > m_size || m_size - offset < 4) {
            m_ok = false;
            return 0;
        }
        uint32_t v = 0;
        std::memcpy(&v, m_data + offset, 4);
        return v;
    }
    std::vector<uint8_t> bytes(size_t offset, size_t n) {
        if (offset > m_size || m_size - offset < n) {
            m_ok = false;
            return {};
        }
        return std::vector<uint8_t>(m_data + offset, m_data + offset + n);
    }
    // A name: its size (with the terminating zero), then the characters.
    std::string name(size_t offset) {
        const uint32_t n = at(offset);
        const std::vector<uint8_t> b = bytes(offset + 4, n);
        std::string s(b.begin(), b.end());
        s.resize(strnlen(s.c_str(), s.size()));
        return s;
    }
    void fail() { m_ok = false; }

private:
    const uint8_t* m_data;
    size_t m_size;
    bool m_ok = true;
};

// A sequential reader over Data.
struct Cursor {
    Data& data;
    size_t offset;

    uint32_t next() {
        const uint32_t v = data.at(offset);
        offset += 4;
        return v;
    }
};

bool readValue(Data& d, Parameter& p, size_t& at, int depth);

bool readType(Data& d, Parameter& p, size_t& at, const Parameter* parent, int depth) {
    if (depth > 16) return false;
    if (parent == nullptr) {
        p.type = static_cast<Type>(d.at(at));
        p.cls = static_cast<Class>(d.at(at + 4));
        const uint32_t name = d.at(at + 8);
        const uint32_t semantic = d.at(at + 12);
        p.elements = d.at(at + 16);
        at += 20;
        if (name != 0) p.name = d.name(name);
        if (semantic != 0) p.semantic = d.name(semantic);
        switch (p.cls) {
            case Class::Vector:
                p.columns = d.at(at);
                p.rows = d.at(at + 4);
                at += 8;
                p.bytes = 4 * p.rows * p.columns;
                break;
            case Class::Scalar:
            case Class::MatrixRows:
            case Class::MatrixColumns:
                p.rows = d.at(at);
                p.columns = d.at(at + 4);
                at += 8;
                p.bytes = 4 * p.rows * p.columns;
                break;
            case Class::Struct: {
                const uint32_t count = d.at(at);
                at += 4;
                if (count > 1024) return false;
                p.members.resize(count);
                break;
            }
            case Class::Object:
                p.bytes = isSampler(p.type) ? 0 : 4;
                break;
            default:
                return false;
        }
        if (p.rows > 4 || p.columns > 4 || p.elements > 65536) return false;
    } else {
        // An element of an array: the array's type without the element count.
        p.type = parent->type;
        p.cls = parent->cls;
        p.name = parent->name;
        p.semantic = parent->semantic;
        p.rows = parent->rows;
        p.columns = parent->columns;
        p.bytes = parent->bytes;
        p.members.resize(parent->cls == Class::Struct ? parent->members.size() : 0);
    }
    if (!d.ok()) return false;

    if (parent == nullptr && p.elements != 0) {
        // The element type is read again for each element from the same place.
        const size_t start = at;
        std::vector<Parameter> elements(p.elements);
        uint32_t bytes = 0;
        const Parameter shape = p;
        for (Parameter& e : elements) {
            at = start;
            if (!readType(d, e, at, &shape, depth + 1)) return false;
            bytes += e.bytes;
        }
        p.members = std::move(elements);
        p.bytes = bytes;
    } else if (!p.members.empty()) {
        uint32_t bytes = 0;
        for (Parameter& m : p.members) {
            if (!readType(d, m, at, nullptr, depth + 1)) return false;
            bytes += m.bytes;
        }
        p.bytes = bytes;
    }
    return d.ok();
}

bool readState(Data& d, State& s, size_t& at) {
    s.operation = d.at(at);
    s.index = d.at(at + 4);
    size_t type = d.at(at + 8);
    size_t value = d.at(at + 12);
    at += 16;
    if (!d.ok() || stateInfo(s.operation) == nullptr) return false;
    return readType(d, s.value, type, nullptr, 0) && readValue(d, s.value, value, 0);
}

bool readValue(Data& d, Parameter& p, size_t& at, int depth) {
    if (depth > 16) return false;
    if (p.elements != 0 || p.cls == Class::Struct) {
        for (Parameter& m : p.members) {
            if (!readValue(d, m, at, depth + 1)) return false;
        }
        return true;
    }
    switch (p.cls) {
        case Class::Scalar:
        case Class::Vector:
        case Class::MatrixRows:
        case Class::MatrixColumns:
            p.value = d.bytes(at, p.bytes);
            at += p.bytes;
            return d.ok();
        case Class::Object:
            if (isSampler(p.type)) {
                const uint32_t count = d.at(at);
                at += 4;
                if (!d.ok() || count > 256) return false;
                p.samplerStates.resize(count);
                for (State& s : p.samplerStates) {
                    if (!readState(d, s, at)) return false;
                }
                return true;
            }
            p.object = d.at(at);
            at += 4;
            return d.ok();
        default:
            return false;
    }
}

bool readAnnotation(Data& d, Cursor& c, Parameter& a) {
    size_t type = c.next();
    size_t value = c.next();
    return d.ok() && readType(d, a, type, nullptr, 0) && readValue(d, a, value, 0);
}

bool readAnnotations(Data& d, Cursor& c, std::vector<Parameter>& out, uint32_t count) {
    if (count > 1024) return false;
    out.resize(count);
    for (Parameter& a : out) {
        if (!readAnnotation(d, c, a)) return false;
    }
    return true;
}

// The object ids of a parameter tree (strings, textures, shaders), for sizing the object table.
void maxObject(const Parameter& p, uint32_t& max) {
    if (p.cls == Class::Object && !isSampler(p.type) && p.object > max) max = p.object;
    for (const Parameter& m : p.members) maxObject(m, max);
    for (const State& s : p.samplerStates) maxObject(s.value, max);
    for (const Parameter& a : p.annotations) maxObject(a, max);
}

const StateInfo kStates[] = {
    {StateClass::RenderState, 7, "ZENABLE"},
    {StateClass::RenderState, 8, "FILLMODE"},
    {StateClass::RenderState, 9, "SHADEMODE"},
    {StateClass::RenderState, 14, "ZWRITEENABLE"},
    {StateClass::RenderState, 15, "ALPHATESTENABLE"},
    {StateClass::RenderState, 16, "LASTPIXEL"},
    {StateClass::RenderState, 19, "SRCBLEND"},
    {StateClass::RenderState, 20, "DESTBLEND"},
    {StateClass::RenderState, 22, "CULLMODE"},
    {StateClass::RenderState, 23, "ZFUNC"},
    {StateClass::RenderState, 24, "ALPHAREF"},
    {StateClass::RenderState, 25, "ALPHAFUNC"},
    {StateClass::RenderState, 26, "DITHERENABLE"},
    {StateClass::RenderState, 27, "ALPHABLENDENABLE"},
    {StateClass::RenderState, 28, "FOGENABLE"},
    {StateClass::RenderState, 29, "SPECULARENABLE"},
    {StateClass::RenderState, 34, "FOGCOLOR"},
    {StateClass::RenderState, 35, "FOGTABLEMODE"},
    {StateClass::RenderState, 36, "FOGSTART"},
    {StateClass::RenderState, 37, "FOGEND"},
    {StateClass::RenderState, 38, "FOGDENSITY"},
    {StateClass::RenderState, 48, "RANGEFOGENABLE"},
    {StateClass::RenderState, 52, "STENCILENABLE"},
    {StateClass::RenderState, 53, "STENCILFAIL"},
    {StateClass::RenderState, 54, "STENCILZFAIL"},
    {StateClass::RenderState, 55, "STENCILPASS"},
    {StateClass::RenderState, 56, "STENCILFUNC"},
    {StateClass::RenderState, 57, "STENCILREF"},
    {StateClass::RenderState, 58, "STENCILMASK"},
    {StateClass::RenderState, 59, "STENCILWRITEMASK"},
    {StateClass::RenderState, 60, "TEXTUREFACTOR"},
    {StateClass::RenderState, 128, "WRAP0"},
    {StateClass::RenderState, 129, "WRAP1"},
    {StateClass::RenderState, 130, "WRAP2"},
    {StateClass::RenderState, 131, "WRAP3"},
    {StateClass::RenderState, 132, "WRAP4"},
    {StateClass::RenderState, 133, "WRAP5"},
    {StateClass::RenderState, 134, "WRAP6"},
    {StateClass::RenderState, 135, "WRAP7"},
    {StateClass::RenderState, 198, "WRAP8"},
    {StateClass::RenderState, 199, "WRAP9"},
    {StateClass::RenderState, 200, "WRAP10"},
    {StateClass::RenderState, 201, "WRAP11"},
    {StateClass::RenderState, 202, "WRAP12"},
    {StateClass::RenderState, 203, "WRAP13"},
    {StateClass::RenderState, 204, "WRAP14"},
    {StateClass::RenderState, 205, "WRAP15"},
    {StateClass::RenderState, 136, "CLIPPING"},
    {StateClass::RenderState, 137, "LIGHTING"},
    {StateClass::RenderState, 139, "AMBIENT"},
    {StateClass::RenderState, 140, "FOGVERTEXMODE"},
    {StateClass::RenderState, 141, "COLORVERTEX"},
    {StateClass::RenderState, 142, "LOCALVIEWER"},
    {StateClass::RenderState, 143, "NORMALIZENORMALS"},
    {StateClass::RenderState, 145, "DIFFUSEMATERIALSOURCE"},
    {StateClass::RenderState, 146, "SPECULARMATERIALSOURCE"},
    {StateClass::RenderState, 147, "AMBIENTMATERIALSOURCE"},
    {StateClass::RenderState, 148, "EMISSIVEMATERIALSOURCE"},
    {StateClass::RenderState, 151, "VERTEXBLEND"},
    {StateClass::RenderState, 152, "CLIPPLANEENABLE"},
    {StateClass::RenderState, 154, "POINTSIZE"},
    {StateClass::RenderState, 155, "POINTSIZE_MIN"},
    {StateClass::RenderState, 166, "POINTSIZE_MAX"},
    {StateClass::RenderState, 156, "POINTSPRITEENABLE"},
    {StateClass::RenderState, 157, "POINTSCALEENABLE"},
    {StateClass::RenderState, 158, "POINTSCALE_A"},
    {StateClass::RenderState, 159, "POINTSCALE_B"},
    {StateClass::RenderState, 160, "POINTSCALE_C"},
    {StateClass::RenderState, 161, "MULTISAMPLEANTIALIAS"},
    {StateClass::RenderState, 162, "MULTISAMPLEMASK"},
    {StateClass::RenderState, 163, "PATCHEDGESTYLE"},
    {StateClass::RenderState, 165, "DEBUGMONITORTOKEN"},
    {StateClass::RenderState, 167, "INDEXEDVERTEXBLENDENABLE"},
    {StateClass::RenderState, 168, "COLORWRITEENABLE"},
    {StateClass::RenderState, 170, "TWEENFACTOR"},
    {StateClass::RenderState, 171, "BLENDOP"},
    {StateClass::RenderState, 172, "POSITIONDEGREE"},
    {StateClass::RenderState, 173, "NORMALDEGREE"},
    {StateClass::RenderState, 174, "SCISSORTESTENABLE"},
    {StateClass::RenderState, 175, "SLOPESCALEDEPTHBIAS"},
    {StateClass::RenderState, 176, "ANTIALIASEDLINEENABLE"},
    {StateClass::RenderState, 178, "MINTESSELLATIONLEVEL"},
    {StateClass::RenderState, 179, "MAXTESSELLATIONLEVEL"},
    {StateClass::RenderState, 180, "ADAPTIVETESS_X"},
    {StateClass::RenderState, 181, "ADAPTIVETESS_Y"},
    {StateClass::RenderState, 182, "ADAPTIVETESS_Z"},
    {StateClass::RenderState, 183, "ADAPTIVETESS_W"},
    {StateClass::RenderState, 184, "ENABLEADAPTIVETESSELLATION"},
    {StateClass::RenderState, 185, "TWOSIDEDSTENCILMODE"},
    {StateClass::RenderState, 186, "CCW_STENCILFAIL"},
    {StateClass::RenderState, 187, "CCW_STENCILZFAIL"},
    {StateClass::RenderState, 188, "CCW_STENCILPASS"},
    {StateClass::RenderState, 189, "CCW_STENCILFUNC"},
    {StateClass::RenderState, 190, "COLORWRITEENABLE1"},
    {StateClass::RenderState, 191, "COLORWRITEENABLE2"},
    {StateClass::RenderState, 192, "COLORWRITEENABLE3"},
    {StateClass::RenderState, 193, "BLENDFACTOR"},
    {StateClass::RenderState, 194, "SRGBWRITEENABLE"},
    {StateClass::RenderState, 195, "DEPTHBIAS"},
    {StateClass::RenderState, 206, "SEPARATEALPHABLENDENABLE"},
    {StateClass::RenderState, 207, "SRCBLENDALPHA"},
    {StateClass::RenderState, 208, "DESTBLENDALPHA"},
    {StateClass::RenderState, 209, "BLENDOPALPHA"},
    {StateClass::TextureStage, 1, "COLOROP"},
    {StateClass::TextureStage, 26, "COLORARG0"},
    {StateClass::TextureStage, 2, "COLORARG1"},
    {StateClass::TextureStage, 3, "COLORARG2"},
    {StateClass::TextureStage, 4, "ALPHAOP"},
    {StateClass::TextureStage, 27, "ALPHAARG0"},
    {StateClass::TextureStage, 5, "ALPHAARG1"},
    {StateClass::TextureStage, 6, "ALPHAARG2"},
    {StateClass::TextureStage, 28, "RESULTARG"},
    {StateClass::TextureStage, 7, "BUMPENVMAT00"},
    {StateClass::TextureStage, 8, "BUMPENVMAT01"},
    {StateClass::TextureStage, 9, "BUMPENVMAT10"},
    {StateClass::TextureStage, 10, "BUMPENVMAT11"},
    {StateClass::TextureStage, 11, "TEXCOORDINDEX"},
    {StateClass::TextureStage, 22, "BUMPENVLSCALE"},
    {StateClass::TextureStage, 23, "BUMPENVLOFFSET"},
    {StateClass::TextureStage, 24, "TEXTURETRANSFORMFLAGS"},
    {StateClass::TextureStage, 32, "CONSTANT"},
    {StateClass::NPatchMode, 0, "NPatchMode"},
    {StateClass::Fvf, 0, "FVF"},
    {StateClass::Transform, 3, "PROJECTION"},
    {StateClass::Transform, 2, "VIEW"},
    {StateClass::Transform, 256, "WORLD"},
    {StateClass::Transform, 16, "TEXTURE0"},
    {StateClass::Material, MaterialDiffuse, "MaterialDiffuse"},
    {StateClass::Material, MaterialAmbient, "MaterialAmbient"},
    {StateClass::Material, MaterialSpecular, "MaterialSpecular"},
    {StateClass::Material, MaterialEmissive, "MaterialEmissive"},
    {StateClass::Material, MaterialPower, "MaterialPower"},
    {StateClass::Light, LightType, "LightType"},
    {StateClass::Light, LightDiffuse, "LightDiffuse"},
    {StateClass::Light, LightSpecular, "LightSpecular"},
    {StateClass::Light, LightAmbient, "LightAmbient"},
    {StateClass::Light, LightPosition, "LightPosition"},
    {StateClass::Light, LightDirection, "LightDirection"},
    {StateClass::Light, LightRange, "LightRange"},
    {StateClass::Light, LightFalloff, "LightFallOff"},
    {StateClass::Light, LightAttenuation0, "LightAttenuation0"},
    {StateClass::Light, LightAttenuation1, "LightAttenuation1"},
    {StateClass::Light, LightAttenuation2, "LightAttenuation2"},
    {StateClass::Light, LightTheta, "LightTheta"},
    {StateClass::Light, LightPhi, "LightPhi"},
    {StateClass::LightEnable, 0, "LightEnable"},
    {StateClass::VertexShader, 0, "VertexShader"},
    {StateClass::PixelShader, 0, "PixelShader"},
    {StateClass::ShaderConstant, VertexFloat, "VertexShaderConstantF"},
    {StateClass::ShaderConstant, VertexBool, "VertexShaderConstantB"},
    {StateClass::ShaderConstant, VertexInt, "VertexShaderConstantI"},
    {StateClass::ShaderConstant, VertexFloat, "VertexShaderConstant"},
    {StateClass::ShaderConstant, VertexFloat, "VertexShaderConstant1"},
    {StateClass::ShaderConstant, VertexFloat, "VertexShaderConstant2"},
    {StateClass::ShaderConstant, VertexFloat, "VertexShaderConstant3"},
    {StateClass::ShaderConstant, VertexFloat, "VertexShaderConstant4"},
    {StateClass::ShaderConstant, PixelFloat, "PixelShaderConstantF"},
    {StateClass::ShaderConstant, PixelBool, "PixelShaderConstantB"},
    {StateClass::ShaderConstant, PixelInt, "PixelShaderConstantI"},
    {StateClass::ShaderConstant, PixelFloat, "PixelShaderConstant"},
    {StateClass::ShaderConstant, PixelFloat, "PixelShaderConstant1"},
    {StateClass::ShaderConstant, PixelFloat, "PixelShaderConstant2"},
    {StateClass::ShaderConstant, PixelFloat, "PixelShaderConstant3"},
    {StateClass::ShaderConstant, PixelFloat, "PixelShaderConstant4"},
    {StateClass::Texture, 0, "Texture"},
    {StateClass::SamplerState, 1, "AddressU"},
    {StateClass::SamplerState, 2, "AddressV"},
    {StateClass::SamplerState, 3, "AddressW"},
    {StateClass::SamplerState, 4, "BorderColor"},
    {StateClass::SamplerState, 5, "MagFilter"},
    {StateClass::SamplerState, 6, "MinFilter"},
    {StateClass::SamplerState, 7, "MipFilter"},
    {StateClass::SamplerState, 8, "MipMapLodBias"},
    {StateClass::SamplerState, 9, "MaxMipLevel"},
    {StateClass::SamplerState, 10, "MaxAnisotropy"},
    {StateClass::SamplerState, 11, "SRGBTexture"},
    {StateClass::SamplerState, 12, "ElementIndex"},
    {StateClass::SamplerState, 13, "DMAPOffset"},
    {StateClass::Sampler, 0, "Sampler"},
};

}  // namespace

bool isSampler(Type type) {
    return type >= Type::Sampler && type <= Type::SamplerCube;
}

bool isTexture(Type type) {
    return type >= Type::Texture && type <= Type::TextureCube;
}

const Parameter* Parameter::annotation(std::string_view key) const {
    for (const Parameter& a : annotations) {
        if (a.name == key) return &a;
    }
    return nullptr;
}

const Parameter* Technique::annotation(std::string_view key) const {
    for (const Parameter& a : annotations) {
        if (a.name == key) return &a;
    }
    return nullptr;
}

const Parameter* Effect::parameter(std::string_view key) const {
    for (const Parameter& p : parameters) {
        if (p.name == key) return &p;
    }
    return nullptr;
}

std::string_view Effect::string(uint32_t object) const {
    if (object >= objects.size()) return {};
    const std::vector<uint8_t>& o = objects[object];
    const char* s = reinterpret_cast<const char*>(o.data());
    return std::string_view(s, strnlen(s, o.size()));
}

const StateInfo* stateInfo(uint32_t operation) {
    return operation < std::size(kStates) ? &kStates[operation] : nullptr;
}

std::optional<Effect> parseEffect(const std::vector<uint8_t>& file) {
    if (file.size() < 8) return std::nullopt;
    uint32_t tag = 0;
    uint32_t start = 0;
    std::memcpy(&tag, file.data(), 4);
    std::memcpy(&start, file.data() + 4, 4);
    if (tag != kTag) return std::nullopt;
    Data d(file.data() + 8, file.size() - 8);
    Cursor c{d, start};

    Effect e;
    const uint32_t parameterCount = c.next();
    const uint32_t techniqueCount = c.next();
    c.next();
    const uint32_t objectCount = c.next();
    if (!d.ok() || parameterCount > 4096 || techniqueCount > 4096 || objectCount > 65536) return std::nullopt;

    e.parameters.resize(parameterCount);
    for (Parameter& p : e.parameters) {
        size_t type = c.next();
        size_t value = c.next();
        const uint32_t flags = c.next();
        const uint32_t annotations = c.next();
        if (!d.ok() || !readType(d, p, type, nullptr, 0) || !readValue(d, p, value, 0)) return std::nullopt;
        p.flags = flags;
        if (!readAnnotations(d, c, p.annotations, annotations)) return std::nullopt;
    }
    e.techniques.resize(techniqueCount);
    for (Technique& t : e.techniques) {
        const uint32_t name = c.next();
        const uint32_t annotations = c.next();
        const uint32_t passes = c.next();
        if (!d.ok() || passes > 256) return std::nullopt;
        if (name != 0) t.name = d.name(name);
        if (!readAnnotations(d, c, t.annotations, annotations)) return std::nullopt;
        t.passes.resize(passes);
        for (Pass& p : t.passes) {
            const uint32_t passName = c.next();
            const uint32_t passAnnotations = c.next();
            const uint32_t states = c.next();
            if (!d.ok() || states > 4096) return std::nullopt;
            if (passName != 0) p.name = d.name(passName);
            if (!readAnnotations(d, c, p.annotations, passAnnotations)) return std::nullopt;
            p.states.resize(states);
            for (State& s : p.states) {
                size_t at = c.offset;
                if (!readState(d, s, at)) return std::nullopt;
                c.offset = at;
            }
        }
    }

    uint32_t maxId = objectCount > 0 ? objectCount - 1 : 0;
    for (const Parameter& p : e.parameters) maxObject(p, maxId);
    for (const Technique& t : e.techniques) {
        for (const Pass& p : t.passes) {
            for (const State& s : p.states) maxObject(s.value, maxId);
        }
    }
    e.objects.resize(static_cast<size_t>(maxId) + 1);

    // Strings and shaders of parameters: an object id, then the data.
    const uint32_t stringCount = c.next();
    const uint32_t resourceCount = c.next();
    if (!d.ok()) return std::nullopt;
    for (uint32_t i = 0; i < stringCount; ++i) {
        const uint32_t id = c.next();
        const uint32_t size = c.next();
        if (!d.ok() || id >= e.objects.size()) return std::nullopt;
        e.objects[id] = d.bytes(c.offset, size);
        c.offset += (static_cast<size_t>(size) + 3) & ~size_t{3};
    }
    // The data of states: a pass's state (technique, pass, state) or a sampler parameter's (no technique, parameter,
    // element, state); the usage says what the data is.
    for (uint32_t i = 0; i < resourceCount; ++i) {
        const uint32_t technique = c.next();
        const uint32_t index = c.next();
        const uint32_t element = c.next();
        const uint32_t stateIndex = c.next();
        const uint32_t usage = c.next();
        const uint32_t size = c.next();
        if (!d.ok()) return std::nullopt;
        std::vector<uint8_t> data = d.bytes(c.offset, size);
        c.offset += (static_cast<size_t>(size) + 3) & ~size_t{3};
        if (!d.ok()) return std::nullopt;

        State* state = nullptr;
        if (technique == kNone) {
            if (index >= e.parameters.size()) return std::nullopt;
            Parameter* p = &e.parameters[index];
            // The element counts for an array only (a single sampler comes with element 0).
            if (element != kNone && p->elements != 0) {
                if (element >= p->members.size()) return std::nullopt;
                p = &p->members[element];
            }
            if (stateIndex >= p->samplerStates.size()) return std::nullopt;
            state = &p->samplerStates[stateIndex];
        } else {
            if (technique >= e.techniques.size() || index >= e.techniques[technique].passes.size()) return std::nullopt;
            Pass& pass = e.techniques[technique].passes[index];
            if (stateIndex >= pass.states.size() || element != kNone) return std::nullopt;
            state = &pass.states[stateIndex];
        }

        const Type type = state->value.type;
        if (usage == 0 && (type == Type::VertexShader || type == Type::PixelShader)) {
            state->kind = StateValue::Constant;
            if (state->value.object >= e.objects.size()) return std::nullopt;
            e.objects[state->value.object] = std::move(data);
        } else if (usage == 0 && (type == Type::Bool || type == Type::Int || type == Type::Float)) {
            state->kind = StateValue::Expression;
            state->code = words(data);
        } else if (usage == 1) {
            state->kind = StateValue::Parameter;
            state->text.assign(data.begin(), data.end());
            state->text.resize(strnlen(state->text.c_str(), state->text.size()));
        } else {
            return std::nullopt;
        }
    }
    if (c.offset != d.size()) return std::nullopt;
    return e;
}

std::vector<uint32_t> words(const std::vector<uint8_t>& bytes) {
    std::vector<uint32_t> w(bytes.size() / 4);
    if (!w.empty()) std::memcpy(w.data(), bytes.data(), w.size() * 4);
    return w;
}

std::optional<std::vector<uint32_t>> comment(const std::vector<uint32_t>& code, std::string_view tag) {
    uint32_t want = 0;
    if (tag.size() != 4) return std::nullopt;
    std::memcpy(&want, tag.data(), 4);
    size_t i = 1;
    while (i < code.size() && (code[i] & 0xFFFF) == 0xFFFE) {
        const size_t n = code[i] >> 16;
        if (n > code.size() - i - 1) return std::nullopt;
        if (n >= 1 && code[i + 1] == want) {
            return std::vector<uint32_t>(code.begin() + static_cast<ptrdiff_t>(i) + 2,
                code.begin() + static_cast<ptrdiff_t>(i + 1 + n));
        }
        i += 1 + n;
    }
    return std::nullopt;
}

namespace {

class Bytes {
public:
    explicit Bytes(const std::vector<uint32_t>& w) : m_size(w.size() * 4), m_data(reinterpret_cast<const uint8_t*>(w.data())) {}

    bool ok() const { return m_ok; }
    template <typename T>
    T at(size_t offset) {
        T v{};
        if (offset > m_size || m_size - offset < sizeof(T)) {
            m_ok = false;
            return v;
        }
        std::memcpy(&v, m_data + offset, sizeof(T));
        return v;
    }
    std::string text(size_t offset) {
        if (offset >= m_size) {
            m_ok = false;
            return {};
        }
        const char* s = reinterpret_cast<const char*>(m_data + offset);
        return std::string(s, strnlen(s, m_size - offset));
    }

private:
    size_t m_size;
    const uint8_t* m_data;
    bool m_ok = true;
};

bool readConstantType(Bytes& b, size_t at, ConstantType& t, int depth) {
    if (depth > 16) return false;
    t.cls = static_cast<Class>(b.at<uint16_t>(at));
    t.type = static_cast<Type>(b.at<uint16_t>(at + 2));
    t.rows = b.at<uint16_t>(at + 4);
    t.columns = b.at<uint16_t>(at + 6);
    t.elements = b.at<uint16_t>(at + 8);
    const uint16_t members = b.at<uint16_t>(at + 10);
    const uint32_t memberInfo = b.at<uint32_t>(at + 12);
    if (!b.ok()) return false;
    t.members.resize(members);
    for (uint16_t i = 0; i < members; ++i) {
        const uint32_t name = b.at<uint32_t>(memberInfo + i * 8u);
        const uint32_t type = b.at<uint32_t>(memberInfo + i * 8u + 4);
        if (!b.ok()) return false;
        t.members[i].first = b.text(name);
        if (!readConstantType(b, type, t.members[i].second, depth + 1)) return false;
    }
    return b.ok();
}

}  // namespace

std::optional<ConstantTable> constantTable(const std::vector<uint32_t>& code) {
    const std::optional<std::vector<uint32_t>> body = comment(code, "CTAB");
    if (!body) return std::nullopt;
    Bytes b(*body);
    ConstantTable t;
    const uint32_t creator = b.at<uint32_t>(4);
    t.version = b.at<uint32_t>(8);
    const uint32_t count = b.at<uint32_t>(12);
    const uint32_t info = b.at<uint32_t>(16);
    const uint32_t target = b.at<uint32_t>(24);
    if (!b.ok() || count > 4096) return std::nullopt;
    t.creator = b.text(creator);
    if (target != 0) t.target = b.text(target);
    t.constants.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        const size_t at = info + i * 20u;
        Constant& c = t.constants[i];
        c.name = b.text(b.at<uint32_t>(at));
        c.set = static_cast<RegisterSet>(b.at<uint16_t>(at + 4));
        c.registerIndex = b.at<uint16_t>(at + 6);
        c.registerCount = b.at<uint16_t>(at + 8);
        if (!b.ok() || !readConstantType(b, b.at<uint32_t>(at + 12), c.type, 0)) return std::nullopt;
        if (const uint32_t value = b.at<uint32_t>(at + 16); value != 0) {
            c.defaults.resize(static_cast<size_t>(c.registerCount) * 4);
            for (size_t k = 0; k < c.defaults.size(); ++k) c.defaults[k] = b.at<float>(value + k * 4);
            if (!b.ok()) return std::nullopt;
        }
    }
    return t;
}

namespace {

struct OpInfo {
    uint32_t code;
    uint32_t inputs;
    Op op;
};

const OpInfo kOps[] = {
    {0x000, 0, Op::Nop},
    {0x100, 1, Op::Mov},
    {0x101, 1, Op::Neg},
    {0x103, 1, Op::Rcp},
    {0x104, 1, Op::Frc},
    {0x105, 1, Op::Exp},
    {0x106, 1, Op::Log},
    {0x107, 1, Op::Rsq},
    {0x108, 1, Op::Sin},
    {0x109, 1, Op::Cos},
    {0x10A, 1, Op::Asin},
    {0x10B, 1, Op::Acos},
    {0x10C, 1, Op::Atan},
    {0x200, 2, Op::Min},
    {0x201, 2, Op::Max},
    {0x202, 2, Op::Lt},
    {0x203, 2, Op::Ge},
    {0x204, 2, Op::Add},
    {0x205, 2, Op::Mul},
    {0x206, 2, Op::Atan2},
    {0x208, 2, Op::Div},
    {0x300, 3, Op::Cmp},
    {0x500, 2, Op::Dot},
    {0x70E, 9, Op::DotSwiz6},
    {0x70E, 11, Op::DotSwiz8},
};

bool validTable(uint32_t t) {
    return t == Operand::Literal || t == Operand::Input || t == Operand::TexelInput || t == Operand::Output ||
           t == Operand::OutputBool || t == Operand::OutputInt || t == Operand::Temp;
}

bool readOperand(const std::vector<uint32_t>& w, size_t& p, Operand& o) {
    if (p >= w.size()) return false;
    const uint32_t relative = w[p++];
    if (relative > 1) return false;
    if (relative == 1) {
        if (p + 2 > w.size() || !validTable(w[p])) return false;
        o.relative = true;
        o.indexTable = static_cast<Operand::Table>(w[p]);
        o.indexOffset = w[p + 1];
        p += 2;
    }
    if (p + 2 > w.size() || !validTable(w[p])) return false;
    o.table = static_cast<Operand::Table>(w[p]);
    o.offset = w[p + 1];
    p += 2;
    return true;
}

}  // namespace

std::optional<Program> parseProgram(const std::vector<uint32_t>& code) {
    Program program;
    if (const auto lit = comment(code, "CLIT")) {
        if (lit->empty()) return std::nullopt;
        const uint32_t n = (*lit)[0];
        if (n > (lit->size() - 1) / 2) return std::nullopt;
        program.literals.resize(n);
        if (n != 0) std::memcpy(program.literals.data(), lit->data() + 1, n * sizeof(double));
    }
    if (const auto table = constantTable(code)) program.inputs = *table;
    const auto fxlc = comment(code, "FXLC");
    if (!fxlc) return program;
    const std::vector<uint32_t>& w = *fxlc;
    if (w.empty()) return std::nullopt;
    const uint32_t count = w[0];
    size_t p = 1;
    for (uint32_t i = 0; i < count; ++i) {
        if (p + 2 > w.size()) return std::nullopt;
        const uint32_t word = w[p];
        const uint32_t inputs = w[p + 1];
        p += 2;
        Instruction ins;
        const uint32_t opcode = (word >> 20) & 0x7FF;
        ins.components = word & 0xFFFF;
        ins.scalar = (word & 0x80000000u) != 0;
        if (ins.components < 1 || ins.components > 4) return std::nullopt;
        const OpInfo* info = nullptr;
        for (const OpInfo& o : kOps) {
            if (o.code == opcode && o.inputs == inputs) info = &o;
        }
        if (info == nullptr) return std::nullopt;
        ins.op = info->op;
        ins.inputs.resize(inputs);
        for (Operand& o : ins.inputs) {
            if (!readOperand(w, p, o)) return std::nullopt;
        }
        if (!readOperand(w, p, ins.output) || ins.output.relative) return std::nullopt;
        program.instructions.push_back(std::move(ins));
    }
    return program;
}

namespace {

double readRegister(const Program& program, Registers& r, Operand::Table table, uint32_t offset) {
    switch (table) {
        case Operand::Literal:
            return offset < program.literals.size() ? program.literals[offset] : 0.0;
        case Operand::Input:
            return offset < r.input.size() ? r.input[offset] : 0.0;
        case Operand::TexelInput:
            return offset < r.texel.size() ? r.texel[offset] : 0.0;
        case Operand::Output:
            return offset < r.output.size() ? r.output[offset] : 0.0;
        case Operand::OutputBool:
            return offset < r.outputBool.size() ? r.outputBool[offset] : 0.0;
        case Operand::OutputInt:
            return offset < r.outputInt.size() ? r.outputInt[offset] : 0.0;
        case Operand::Temp:
            return offset < r.temp.size() ? r.temp[offset] : 0.0;
    }
    return 0.0;
}

double input(const Program& program, Registers& r, const Operand& o, uint32_t component) {
    uint32_t base = 0;
    if (o.relative) {
        base = static_cast<uint32_t>(std::lrint(readRegister(program, r, o.indexTable, o.indexOffset)));
        base *= o.table == Operand::OutputBool ? 1u : 4u;
    }
    return readRegister(program, r, o.table, base + o.offset + component);
}

template <typename T>
void store(std::vector<T>& table, uint32_t offset, T value) {
    if (offset >= table.size()) table.resize(offset + 1);
    table[offset] = value;
}

void output(Registers& r, const Operand& o, uint32_t component, double value) {
    const uint32_t at = o.offset + component;
    switch (o.table) {
        case Operand::Output:
            store(r.output, at, static_cast<float>(value));
            break;
        case Operand::OutputBool:
            store(r.outputBool, at, value != 0.0 ? 1 : 0);
            break;
        case Operand::OutputInt:
            store(r.outputInt, at, static_cast<int32_t>(std::lrint(value)));
            break;
        case Operand::Temp:
            store(r.temp, at, static_cast<float>(value));
            break;
        default:
            break;
    }
}

double compute(Op op, const double* a) {
    switch (op) {
        case Op::Mov: return a[0];
        case Op::Neg: return -a[0];
        case Op::Rcp: return 1.0 / a[0];
        case Op::Frc: return a[0] - std::floor(a[0]);
        case Op::Exp: return std::pow(2.0, a[0]);
        case Op::Log: {
            const double v = std::fabs(a[0]);
            return v == 0.0 ? 0.0 : std::log2(v);
        }
        case Op::Rsq: {
            const double v = std::fabs(a[0]);
            return v == 0.0 ? std::numeric_limits<double>::infinity() : 1.0 / std::sqrt(v);
        }
        case Op::Sin: return std::sin(a[0]);
        case Op::Cos: return std::cos(a[0]);
        case Op::Asin: return std::asin(a[0]);
        case Op::Acos: return std::acos(a[0]);
        case Op::Atan: return std::atan(a[0]);
        case Op::Min: return std::fmin(a[0], a[1]);
        case Op::Max: return std::fmax(a[0], a[1]);
        case Op::Lt: return a[0] < a[1] ? 1.0 : 0.0;
        case Op::Ge: return a[0] >= a[1] ? 1.0 : 0.0;
        case Op::Add: return a[0] + a[1];
        case Op::Mul: return a[0] * a[1];
        case Op::Atan2: return std::atan2(a[0], a[1]);
        case Op::Div: return a[0] / a[1];
        case Op::Cmp: return a[0] >= 0.0 ? a[1] : a[2];
        case Op::DotSwiz6: return a[0] * a[3] + a[1] * a[4] + a[2] * a[5];
        case Op::DotSwiz8: return a[0] * a[4] + a[1] * a[5] + a[2] * a[6] + a[3] * a[7];
        default: return 0.0;
    }
}

}  // namespace

void run(const Program& program, Registers& r) {
    for (const Instruction& ins : program.instructions) {
        double a[12] = {};
        switch (ins.op) {
            case Op::Nop:
                break;
            case Op::Dot: {
                // One result: the sum over the components; a scalar first input is one value for all.
                double sum = 0.0;
                for (uint32_t j = 0; j < ins.components; ++j) {
                    const double x = input(program, r, ins.inputs[0], ins.scalar ? 0 : j);
                    const double y = input(program, r, ins.inputs[1], j);
                    sum += x * y;
                }
                output(r, ins.output, 0, sum);
                break;
            }
            default:
                for (uint32_t j = 0; j < ins.components; ++j) {
                    for (size_t k = 0; k < ins.inputs.size(); ++k) {
                        a[k] = input(program, r, ins.inputs[k], ins.scalar && k == 0 ? 0 : j);
                    }
                    output(r, ins.output, j, compute(ins.op, a));
                }
                break;
        }
    }
}

}  // namespace oj::fx
