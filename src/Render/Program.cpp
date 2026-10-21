#include "Render/Program.h"

#include "Core/Base/Log.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string.h>

namespace oj::render {

namespace {

constexpr uint32_t kFixedFunction = 0x10000;

// [0x0075F12C]: the effects' debug prints ("%s begin", "%s end", ...); nothing sets it.
constexpr bool kEffectsDebug = false;

// [0x0079E884]: the detail's debug toggle, the level flipping every 32 frames by [0x0079E888]; nothing sets it.
constexpr bool kDetailToggle = false;
uint32_t g_detailToggleFrames = 0;  // [0x0079E888]

int32_t annotationInt(const fx::Parameter* a) {
    if (a == nullptr || a->cls != fx::Class::Scalar || a->type != fx::Type::Int || a->value.size() < 4) return 0;
    int32_t v = 0;
    std::memcpy(&v, a->value.data(), 4);
    return v;
}

// The words of a CONDITION annotation (0x0051C190): adapter numbers and vendor names.
bool conditionWord(const std::string& word, const AdapterInfo& adapter, uint32_t& out) {
    static const std::pair<const char*, uint32_t> kVendors[] = {
        {"intel", 0x8086}, {"ati", 0x1002},   {"matrox", 0x102B}, {"nvidia", 0x10DE},
        {"3dlabs", 0x3D3D}, {"canopus", 0x10DE}, {"dell", 0x10DE},  {"elsa", 0x10DE},
    };
    if (word == "driverproduct") out = adapter.driverProduct;
    else if (word == "driverversion") out = adapter.driverVersion;
    else if (word == "driversubversion") out = adapter.driverSubVersion;
    else if (word == "driverbuild") out = adapter.driverBuild;
    else if (word == "vendorid") out = adapter.vendorId;
    else if (word == "deviceid") out = adapter.deviceId;
    else if (word == "subsysid") out = adapter.subSysId;
    else if (word == "revision") out = adapter.revision;
    else {
        for (const auto& [name, id] : kVendors) {
            if (word == name) {
                out = id;
                return true;
            }
        }
        char* end = nullptr;
        out = static_cast<uint32_t>(std::strtoul(word.c_str(), &end, 0));
        return end != word.c_str();
    }
    return true;
}

// A CONDITION (0x0051A210): terms `<word> <op> <word>` joined by && / and / || / or, taken left to right.
bool evaluateCondition(const std::string& text, const AdapterInfo& adapter) {
    size_t i = 0;
    auto skip = [&] {
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    };
    auto word = [&] {
        skip();
        const size_t start = i;
        while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) ++i;
        return text.substr(start, i - start);
    };
    bool result = true;
    bool orMode = false;
    while (true) {
        const std::string left = word();
        if (left.empty()) break;
        skip();
        std::string op;
        while (i < text.size() && std::strchr("=!<>", text[i]) != nullptr) op += text[i++];
        const std::string right = word();
        uint32_t a = 0;
        uint32_t b = 0;
        conditionWord(left, adapter, a);
        conditionWord(right, adapter, b);
        bool term = false;
        if (op == "==") term = a == b;
        else if (op == "!=") term = a != b;
        else if (op == "<") term = a < b;
        else if (op == "<=") term = a <= b;
        else if (op == ">") term = a > b;
        else if (op == ">=") term = a >= b;
        result = orMode ? (result || term) : (result && term);
        skip();
        if (text.compare(i, 2, "&&") == 0) {
            i += 2;
            orMode = false;
        } else if (text.compare(i, 3, "and") == 0) {
            i += 3;
            orMode = false;
        } else if (text.compare(i, 2, "||") == 0 || text.compare(i, 2, "or") == 0) {
            i += 2;
            orMode = true;
        } else {
            break;
        }
    }
    return result;
}

// A parameter's value as stored: its numbers, an object's id, nothing for a sampler; members in order.
void appendValue(const fx::Parameter& p, std::vector<uint8_t>& out) {
    if (p.elements != 0 || p.cls == fx::Class::Struct) {
        for (const fx::Parameter& m : p.members) appendValue(m, out);
        return;
    }
    if (p.cls == fx::Class::Object) {
        if (!fx::isSampler(p.type)) {
            const uint8_t* b = reinterpret_cast<const uint8_t*>(&p.object);
            out.insert(out.end(), b, b + 4);
        }
        return;
    }
    out.insert(out.end(), p.value.begin(), p.value.end());
}

float number(const fx::Parameter& p, const uint8_t* data) {
    if (p.type == fx::Type::Float) {
        float f = 0.0f;
        std::memcpy(&f, data, 4);
        return f;
    }
    int32_t v = 0;
    std::memcpy(&v, data, 4);
    return p.type == fx::Type::Bool ? (v != 0 ? 1.0f : 0.0f) : static_cast<float>(v);
}

// The float4 registers of a constant from a parameter's value: the table's type gives the registers (a matrix by
// rows or by columns), the parameter its numbers (a matrix row after row).
void writeRegisters(const fx::ConstantType& t, const fx::Parameter& p, const uint8_t*& data, const uint8_t* end,
                    std::vector<float>& regs, uint32_t& reg) {
    if (t.elements > 1) {
        fx::ConstantType one = t;
        one.elements = 1;
        for (uint32_t e = 0; e < t.elements; ++e) {
            const fx::Parameter& element = p.elements != 0 && e < p.members.size() ? p.members[e] : p;
            writeRegisters(one, element, data, end, regs, reg);
        }
        return;
    }
    if (t.cls == fx::Class::Struct) {
        for (size_t i = 0; i < t.members.size(); ++i) {
            if (i >= p.members.size()) break;
            writeRegisters(t.members[i].second, p.members[i], data, end, regs, reg);
        }
        return;
    }
    if (p.cls == fx::Class::Object || p.cls == fx::Class::Struct || static_cast<size_t>(end - data) < p.bytes) {
        data += std::min<size_t>(p.bytes, static_cast<size_t>(end - data));
        return;
    }
    const uint32_t columns = std::max<uint32_t>(p.columns, 1);
    auto at = [&](uint32_t r, uint32_t c) -> float {
        const size_t k = static_cast<size_t>(r) * columns + c;
        return k * 4 + 4 <= p.bytes ? number(p, data + k * 4) : 0.0f;
    };
    auto put = [&](uint32_t comp, float v) {
        const size_t k = static_cast<size_t>(reg) * 4 + comp;
        if (k >= regs.size()) regs.resize(k + 1 + (3 - comp), 0.0f);
        regs[k] = v;
    };
    switch (t.cls) {
        case fx::Class::Scalar:
        case fx::Class::Vector: {
            const uint32_t n = std::min<uint32_t>(std::max<uint32_t>(t.columns, 1), 4);
            for (uint32_t c = 0; c < n; ++c) put(c, at(0, c));
            ++reg;
            break;
        }
        case fx::Class::MatrixRows:
            for (uint32_t r = 0; r < t.rows; ++r, ++reg) {
                for (uint32_t c = 0; c < t.columns && c < 4; ++c) put(c, at(r, c));
            }
            break;
        case fx::Class::MatrixColumns:
            for (uint32_t c = 0; c < t.columns; ++c, ++reg) {
                for (uint32_t r = 0; r < t.rows && r < 4; ++r) put(r, at(r, c));
            }
            break;
        default:
            break;
    }
    data += p.bytes;
}

uint32_t shaderVersion(const std::vector<uint8_t>& code) {
    uint32_t v = 0;
    if (code.size() >= 4) std::memcpy(&v, code.data(), 4);
    return v;
}

}  // namespace

uint32_t shaderClasses(uint32_t vertexShaderVersion, uint32_t pixelShaderVersion, uint32_t shadersSetting) {
    uint32_t classes = kFixedFunction;
    if (pixelShaderVersion >= 0xFFFF0100 && shadersSetting >= 1) {
        classes = 0x30000;
        if (pixelShaderVersion >= 0xFFFF0200 && shadersSetting >= 2) {
            classes = 0x70000;
            if (pixelShaderVersion >= 0xFFFF0300 && vertexShaderVersion >= 0xFFFE0300 && shadersSetting >= 3) {
                classes = 0xF0000;
            }
        }
    }
    return classes;
}

ProgramContext::ProgramContext(IRenderDevice& device, uint32_t classes)
    : m_device(device), m_classes(classes), m_vertexConstants(256 * 4, 0.0f) {}

void ProgramContext::setVertexConstants(uint32_t start, const float* data, uint32_t count) {
    if (start + count <= 256) {
        std::memcpy(m_vertexConstants.data() + start * 4, data, count * 16);
    }
    m_device.setVertexShaderConstantF(start, data, count);
}

void ProgramContext::endCurrent() {
    if (current != nullptr) current->end();
}

void ProgramContext::managerVertexConstants(uint32_t start, const float* data, uint32_t count) {
    // 0x005194A0.
    if (count > 29) {
        if (kEffectsDebug) log::debugPrint("ignoring block const set\n");
        return;
    }
    if (kEffectsDebug) log::debugPrint("SetVertexShaderConstantF r %d count %d\n", start, count);
    m_device.setVertexShaderConstantF(start, data, count);
}

Program::Program(ProgramContext& context, std::string name, fx::Effect effect, TextureLoader textures)
    : m_context(context), m_name(std::move(name)), m_effect(std::move(effect)), m_loader(std::move(textures)) {
    m_values.resize(m_effect.parameters.size());
    for (size_t i = 0; i < m_effect.parameters.size(); ++i) {
        const fx::Parameter& p = m_effect.parameters[i];
        std::vector<uint8_t> value;
        appendValue(p, value);
        if ((p.flags & 1) != 0) {
            m_context.shared.try_emplace(p.name, std::move(value));
        } else {
            m_values[i] = std::move(value);
        }
    }
    loadTextures();
    chooseTechnique();
}

void Program::loadTextures() {
    for (const fx::Parameter& p : m_effect.parameters) {
        // 0x0051AAC0: a texture with annotations, read in order (a texture without any has none).
        if (fx::isTexture(p.type) && m_loader && !p.annotations.empty()) {
            TextureRequest request;
            request.parameter = p.name;
            for (const fx::Parameter& a : p.annotations) {
                const char* key = a.name.c_str();
                const std::string text = a.type == fx::Type::String ? std::string(m_effect.string(a.object)) : "";
                if (_stricmp(key, "name") == 0 || _stricmp(key, "resourcename") == 0) {
                    request.file = text;
                } else if (_stricmp(key, "TextureType") == 0) {
                    log::debugPrint("procedural TextureType\n");
                    if (_stricmp("volume", text.c_str()) == 0) {
                        request.type = 8;
                    } else if (_stricmp("cube", text.c_str()) == 0) {
                        request.type = 9;
                    } else if (_stricmp("texture", text.c_str()) == 0) {
                        request.type = 7;
                    } else {
                        // Stock problem: then the stock game stops at a breakpoint (0x0059AF30); here the type stays.
                        log::debugPrint("unknown procedural texture type in fx %s\n", text.c_str());
                    }
                } else if (_stricmp(key, "function") == 0) {
                    request.function = text;
                } else if (_stricmp(key, "width") == 0) {
                    request.width = static_cast<uint32_t>(annotationInt(&a));
                } else if (_stricmp(key, "height") == 0) {
                    request.height = static_cast<uint32_t>(annotationInt(&a));
                } else if (_stricmp(key, "miplevels") == 0) {
                    request.mipLevels = static_cast<uint32_t>(annotationInt(&a));
                } else if (_stricmp(key, "depth") == 0) {
                    request.depth = static_cast<uint32_t>(annotationInt(&a));
                }
            }
            m_textures[p.name] = m_loader(*this, request);
        }
    }
}

bool Program::recreate() {
    end();
    for (size_t i = 0; i < m_effect.parameters.size(); ++i) {
        const fx::Parameter& p = m_effect.parameters[i];
        if ((p.flags & 1) != 0) continue;
        m_values[i].clear();
        appendValue(p, m_values[i]);
    }
    loadTextures();
    chooseTechnique();
    return m_technique >= 0 && m_techniques[static_cast<size_t>(m_technique)].usable;
}

bool Program::chooseLevel(uint32_t level) {
    // 0x00719364; a level past the table reads what follows it (only the request no one makes, 0x00519F62).
    static const uint32_t kLevelMasks[] = {0x10000, 0x20000, 0x40000, 0x80000};
    const uint32_t mask = level < std::size(kLevelMasks) ? kLevelMasks[level] : 0;
    end();
    int last = -1;
    for (int i = static_cast<int>(m_techniques.size()) - 1; i >= 0; --i) {
        const Technique& t = m_techniques[static_cast<size_t>(i)];
        if (!t.usable) continue;
        if (last < 0) last = i;
        if ((t.classification & mask) != 0) {
            select(i);
            log::debugPrint("(a)%s %s %x %x\n", m_name.c_str(), m_effect.techniques[static_cast<size_t>(i)].name.c_str(),
                            t.classification, t.usable ? 1u : 0u);
            return true;
        }
    }
    if (last < 0) return false;
    select(last);
    return true;
}

void Program::select(int technique) {
    m_technique = technique;
    m_constantMemory = m_techniques[static_cast<size_t>(technique)].constantMemory;
    createShaders();
}

void Program::chooseTechnique() {
    const uint32_t classes = m_context.classes();
    const AdapterInfo adapter = m_context.device().adapterInfo();
    const uint32_t vs = m_context.device().vertexShaderVersion();
    const uint32_t ps = m_context.device().pixelShaderVersion();
    m_techniques.assign(m_effect.techniques.size(), Technique{});
    for (size_t i = 0; i < m_effect.techniques.size(); ++i) {
        const fx::Technique& t = m_effect.techniques[i];
        Technique& r = m_techniques[i];
        for (const fx::Parameter& a : t.annotations) {
            const char* key = a.name.c_str();
            if (_stricmp(key, "CLASSIFICATION") == 0) r.classification = static_cast<uint32_t>(annotationInt(&a));
            else if (_stricmp(key, "USERANDOM") == 0) r.useRandom = annotationInt(&a) != 0;
            else if (_stricmp(key, "USEFFV") == 0) r.useFfv = annotationInt(&a) != 0;
            else if (_stricmp(key, "USECONSTANTMEM") == 0) r.constantMemory = annotationInt(&a) != 0;
            else if (_stricmp(key, "CONDITION") == 0 && a.type == fx::Type::String) {
                if (!evaluateCondition(std::string(m_effect.string(a.object)), adapter)) {
                    r.condition = false;
                    break;
                }
            }
        }
        if (!r.condition || (r.classification & classes & 0xFFFF0000u) == 0) continue;
        if ((r.classification & kFixedFunction) == 0) {
            // The device's validation: the technique's shaders within the device's versions. A 1.x technique that
            // fails is still taken, a 2.x / 3.x one is not.
            bool valid = true;
            for (const fx::Pass& pass : t.passes) {
                for (const fx::State& s : pass.states) {
                    const fx::StateInfo* info = fx::stateInfo(s.operation);
                    if (info == nullptr) continue;
                    if (info->cls != fx::StateClass::VertexShader && info->cls != fx::StateClass::PixelShader) continue;
                    uint32_t object = s.value.object;
                    if (s.kind == fx::StateValue::Parameter) {
                        const fx::Parameter* p = m_effect.parameter(s.text);
                        object = p != nullptr ? p->object : 0;
                    } else if (s.value.cls != fx::Class::Object) {
                        continue;
                    }
                    if (object >= m_effect.objects.size()) continue;
                    const uint32_t version = shaderVersion(m_effect.objects[object]);
                    const uint32_t limit = info->cls == fx::StateClass::VertexShader ? vs : ps;
                    if ((version & 0xFFFF) > (limit & 0xFFFF)) valid = false;
                }
            }
            r.usable = valid || (r.classification & 0xC0000u) == 0;
        } else {
            r.usable = classes == kFixedFunction;
        }
    }
    // 0x0051A0B0: the pass ended, the last usable technique; without one the technique stays.
    end();
    for (int i = static_cast<int>(m_techniques.size()) - 1; i >= 0; --i) {
        if (m_techniques[static_cast<size_t>(i)].usable) {
            select(i);
            break;
        }
    }
}

void Program::createShaders() {
    if (m_technique < 0) return;
    IRenderDevice& device = m_context.device();
    for (const fx::Pass& pass : m_effect.techniques[static_cast<size_t>(m_technique)].passes) {
        for (const fx::State& s : pass.states) {
            const fx::StateInfo* info = fx::stateInfo(s.operation);
            if (info == nullptr ||
                (info->cls != fx::StateClass::VertexShader && info->cls != fx::StateClass::PixelShader)) {
                continue;
            }
            uint32_t object = 0;
            if (s.kind == fx::StateValue::Parameter) {
                const fx::Parameter* p = m_effect.parameter(s.text);
                if (p == nullptr) continue;
                object = p->object;
            } else if (s.value.cls == fx::Class::Object) {
                object = s.value.object;
            } else {
                continue;
            }
            if (object >= m_effect.objects.size() || m_shaders.contains(object)) continue;
            const std::vector<uint32_t> code = fx::words(m_effect.objects[object]);
            if (code.empty()) continue;
            const ShaderId id = info->cls == fx::StateClass::VertexShader ? device.createVertexShader(code.data())
                                                                         : device.createPixelShader(code.data());
            if (id == 0) log::warning("{}: shader {} not created", m_name, object);
            m_shaders[object] = id;
            if (auto table = fx::constantTable(code)) m_tables[object] = std::move(*table);
            if (auto pres = fx::comment(code, "PRES"); pres && !pres->empty()) {
                if (auto program = fx::parseProgram(*pres)) m_preshaders[object] = std::move(*program);
            }
        }
    }
}

const fx::Parameter* Program::findParameter(const std::string& name, size_t* index) const {
    for (size_t i = 0; i < m_effect.parameters.size(); ++i) {
        if (m_effect.parameters[i].name == name) {
            if (index != nullptr) *index = i;
            return &m_effect.parameters[i];
        }
    }
    return nullptr;
}

std::vector<uint8_t>& Program::valueOf(size_t parameter) {
    const fx::Parameter& p = m_effect.parameters[parameter];
    if ((p.flags & 1) != 0) return m_context.shared[p.name];
    return m_values[parameter];
}

bool Program::setValue(const std::string& parameter, const void* data, size_t bytes) {
    size_t index = 0;
    if (findParameter(parameter, &index) == nullptr) return false;
    std::vector<uint8_t>& v = valueOf(index);
    std::memcpy(v.data(), data, std::min(bytes, v.size()));
    return true;
}

TextureId Program::textureOf(const std::string& parameter) {
    const auto it = m_textures.find(parameter);
    return it != m_textures.end() ? it->second : 0;
}

void Program::fillInputs(const fx::ConstantTable& table, std::vector<float>& input) {
    for (const fx::Constant& c : table.constants) {
        size_t index = 0;
        const fx::Parameter* p = findParameter(c.name, &index);
        if (p == nullptr || c.set == fx::RegisterSet::Sampler) continue;
        const std::vector<uint8_t>& value = valueOf(index);
        const uint8_t* data = value.data();
        std::vector<float> regs;
        uint32_t reg = 0;
        writeRegisters(c.type, *p, data, value.data() + value.size(), regs, reg);
        const size_t count = std::min<size_t>(regs.size(), static_cast<size_t>(c.registerCount) * 4);
        const size_t at = static_cast<size_t>(c.registerIndex) * 4;
        if (input.size() < at + count) input.resize(at + count, 0.0f);
        std::copy(regs.begin(), regs.begin() + static_cast<ptrdiff_t>(count), input.begin() + static_cast<ptrdiff_t>(at));
    }
}

uint32_t Program::stateValue(const fx::State& state, std::vector<float>* values) {
    std::vector<uint8_t> bytes;
    if (state.kind == fx::StateValue::Expression) {
        const auto program = fx::parseProgram(state.code);
        fx::Registers regs;
        if (program) {
            fillInputs(program->inputs, regs.input);
            fx::run(*program, regs);
        }
        const bool isFloat = state.value.type == fx::Type::Float;
        const size_t n = std::max<size_t>(state.value.bytes / 4, 1);
        bytes.resize(n * 4);
        for (size_t i = 0; i < n; ++i) {
            const float f = i < regs.output.size() ? regs.output[i] : 0.0f;
            if (isFloat) {
                std::memcpy(bytes.data() + i * 4, &f, 4);
            } else {
                const int32_t v = static_cast<int32_t>(f);
                std::memcpy(bytes.data() + i * 4, &v, 4);
            }
        }
    } else if (state.kind == fx::StateValue::Parameter) {
        size_t index = 0;
        if (findParameter(state.text, &index) != nullptr) bytes = valueOf(index);
    } else {
        appendValue(state.value, bytes);
    }
    if (values != nullptr) {
        values->assign(bytes.size() / 4, 0.0f);
        if (!bytes.empty()) std::memcpy(values->data(), bytes.data(), values->size() * 4);
    }
    uint32_t word = 0;
    if (bytes.size() >= 4) std::memcpy(&word, bytes.data(), 4);
    return word;
}

void Program::applySampler(const fx::Parameter& sampler, uint32_t index) {
    IRenderDevice& device = m_context.device();
    for (const fx::State& s : sampler.samplerStates) {
        const fx::StateInfo* info = fx::stateInfo(s.operation);
        if (info == nullptr) continue;
        if (info->cls == fx::StateClass::Texture) {
            device.setTexture(index, s.kind == fx::StateValue::Parameter ? textureOf(s.text) : 0);
        } else if (info->cls == fx::StateClass::SamplerState) {
            device.setSamplerState(index, static_cast<SamplerState>(info->value), stateValue(s, nullptr));
        }
    }
}

void Program::setShaderConstants(uint32_t object, bool vertex) {
    IRenderDevice& device = m_context.device();
    const auto table = m_tables.find(object);
    if (table != m_tables.end()) {
        for (const fx::Constant& c : table->second.constants) {
            size_t index = 0;
            const fx::Parameter* p = findParameter(c.name, &index);
            if (p == nullptr) continue;
            if (c.set == fx::RegisterSet::Sampler) {
                if (fx::isSampler(p->type)) applySampler(*p, vertex ? 257u + c.registerIndex : c.registerIndex);
                continue;
            }
            const std::vector<uint8_t>& value = valueOf(index);
            const uint8_t* data = value.data();
            std::vector<float> regs;
            uint32_t reg = 0;
            writeRegisters(c.type, *p, data, value.data() + value.size(), regs, reg);
            const uint32_t count = std::min<uint32_t>(c.registerCount, static_cast<uint32_t>(regs.size() / 4));
            if (count == 0) continue;
            if (c.set == fx::RegisterSet::Float4) {
                if (vertex) m_context.managerVertexConstants(c.registerIndex, regs.data(), count);
                else device.setPixelShaderConstantF(c.registerIndex, regs.data(), count);
            } else {
                std::vector<int32_t> ints(regs.size());
                for (size_t i = 0; i < regs.size(); ++i) ints[i] = static_cast<int32_t>(regs[i]);
                if (c.set == fx::RegisterSet::Int4) {
                    if (vertex) device.setVertexShaderConstantI(c.registerIndex, ints.data(), count);
                    else device.setPixelShaderConstantI(c.registerIndex, ints.data(), count);
                } else {
                    std::vector<int32_t> bools(count);
                    for (uint32_t i = 0; i < count; ++i) bools[i] = regs[i * 4] != 0.0f ? 1 : 0;
                    if (vertex) device.setVertexShaderConstantB(c.registerIndex, bools.data(), count);
                    else device.setPixelShaderConstantB(c.registerIndex, bools.data(), count);
                }
            }
        }
    }
    const auto pres = m_preshaders.find(object);
    if (pres != m_preshaders.end()) {
        fx::Registers regs;
        fillInputs(pres->second.inputs, regs.input);
        fx::run(pres->second, regs);
        const uint32_t count = static_cast<uint32_t>((regs.output.size() + 3) / 4);
        regs.output.resize(static_cast<size_t>(count) * 4, 0.0f);
        // The registers the preshader writes, one at a time (an untouched register in between keeps its value).
        for (uint32_t r = 0; r < count; ++r) {
            bool written = false;
            for (const fx::Instruction& ins : pres->second.instructions) {
                if (ins.output.table == fx::Operand::Output && ins.output.offset / 4 == r) written = true;
            }
            if (!written) continue;
            if (vertex) m_context.managerVertexConstants(r, regs.output.data() + r * 4, 1);
            else device.setPixelShaderConstantF(r, regs.output.data() + r * 4, 1);
        }
    }
}

void Program::applyState(const fx::State& s, uint32_t) {
    const fx::StateInfo* info = fx::stateInfo(s.operation);
    if (info == nullptr) return;
    IRenderDevice& device = m_context.device();
    std::vector<float> values;
    const uint32_t word = stateValue(s, &values);
    float f = 0.0f;
    std::memcpy(&f, &word, 4);
    switch (info->cls) {
        case fx::StateClass::RenderState:
            device.setRenderState(static_cast<RenderState>(info->value), word);
            break;
        case fx::StateClass::TextureStage:
            device.setTextureStageState(s.index, static_cast<TextureStageState>(info->value), word);
            break;
        case fx::StateClass::NPatchMode:
            device.setNPatchMode(f);
            break;
        case fx::StateClass::Fvf:
            device.setFvf(word);
            break;
        case fx::StateClass::Transform: {
            float m[16] = {};
            std::copy_n(values.begin(), std::min<size_t>(values.size(), 16), m);
            const uint32_t state = info->value == 256 || info->value == 16 ? info->value + s.index : info->value;
            device.setTransform(static_cast<TransformState>(state), m);
            break;
        }
        case fx::StateClass::Material: {
            FixedMaterial& m = m_context.material;
            if (info->value == fx::MaterialPower) {
                m.values[16] = f;
            } else {
                const size_t at = static_cast<size_t>(info->value) * 4;
                for (size_t i = 0; i < 4 && i < values.size(); ++i) m.values[at + i] = values[i];
            }
            device.setMaterial(m);
            break;
        }
        case fx::StateClass::Light: {
            if (s.index >= m_context.lights.size()) break;
            FixedLight& l = m_context.lights[s.index];
            auto copy = [&](float* to, size_t n) {
                for (size_t i = 0; i < n && i < values.size(); ++i) to[i] = values[i];
            };
            switch (info->value) {
                case fx::LightType: l.type = word; break;
                case fx::LightDiffuse: copy(l.diffuse, 4); break;
                case fx::LightSpecular: copy(l.specular, 4); break;
                case fx::LightAmbient: copy(l.ambient, 4); break;
                case fx::LightPosition: copy(l.position, 3); break;
                case fx::LightDirection: copy(l.direction, 3); break;
                case fx::LightRange: l.range = f; break;
                case fx::LightFalloff: l.falloff = f; break;
                case fx::LightAttenuation0: l.attenuation0 = f; break;
                case fx::LightAttenuation1: l.attenuation1 = f; break;
                case fx::LightAttenuation2: l.attenuation2 = f; break;
                case fx::LightTheta: l.theta = f; break;
                case fx::LightPhi: l.phi = f; break;
                default: break;
            }
            device.setLight(s.index, l);
            break;
        }
        case fx::StateClass::LightEnable:
            device.lightEnable(s.index, word != 0);
            break;
        case fx::StateClass::VertexShader:
        case fx::StateClass::PixelShader: {
            const bool vertex = info->cls == fx::StateClass::VertexShader;
            uint32_t object = 0;
            bool hasShader = false;
            if (s.kind == fx::StateValue::Parameter) {
                const fx::Parameter* p = m_effect.parameter(s.text);
                if (p != nullptr) {
                    object = p->object;
                    hasShader = true;
                }
            } else if (s.value.cls == fx::Class::Object) {
                object = s.value.object;
                hasShader = true;
            }
            const auto it = hasShader ? m_shaders.find(object) : m_shaders.end();
            const ShaderId id = it != m_shaders.end() ? it->second : 0;
            if (vertex) device.setVertexShader(id);
            else device.setPixelShader(id);
            if (id != 0) setShaderConstants(object, vertex);
            break;
        }
        case fx::StateClass::ShaderConstant: {
            const uint32_t count = static_cast<uint32_t>((values.size() + 3) / 4);
            values.resize(static_cast<size_t>(count) * 4, 0.0f);
            std::vector<int32_t> ints(values.size());
            std::memcpy(ints.data(), values.data(), values.size() * 4);
            switch (info->value) {
                case fx::VertexFloat: m_context.managerVertexConstants(s.index, values.data(), count); break;
                case fx::VertexInt: device.setVertexShaderConstantI(s.index, ints.data(), count); break;
                case fx::VertexBool: device.setVertexShaderConstantB(s.index, ints.data(), count); break;
                case fx::PixelFloat: device.setPixelShaderConstantF(s.index, values.data(), count); break;
                case fx::PixelInt: device.setPixelShaderConstantI(s.index, ints.data(), count); break;
                case fx::PixelBool: device.setPixelShaderConstantB(s.index, ints.data(), count); break;
                default: break;
            }
            break;
        }
        case fx::StateClass::Texture:
            device.setTexture(s.index, s.kind == fx::StateValue::Parameter ? textureOf(s.text) : 0);
            break;
        case fx::StateClass::SamplerState:
            device.setSamplerState(s.index, static_cast<SamplerState>(info->value), word);
            break;
        case fx::StateClass::Sampler:
            if (s.kind == fx::StateValue::Parameter) {
                if (const fx::Parameter* p = m_effect.parameter(s.text)) applySampler(*p, s.index);
            }
            break;
    }
}

void Program::beginPass(uint32_t pass) {
    if (kEffectsDebug) log::debugPrint("%s pass %d\n", m_name.c_str(), pass);
    m_context.endCurrent();
    m_context.current = this;
    if (kEffectsDebug) log::debugPrint("%s begin\n", m_name.c_str());  // 0x0051BD40
    if (m_technique < 0) return;
    const fx::Technique& t = m_effect.techniques[static_cast<size_t>(m_technique)];
    m_open = true;
    if (kEffectsDebug) log::debugPrint("%s BeginPass %d\n", m_name.c_str(), pass);
    if (pass >= t.passes.size()) return;
    for (const fx::State& s : t.passes[pass].states) applyState(s, pass);
}

void Program::end() {
    if (!m_open) return;
    if (kEffectsDebug) log::debugPrint("%s end\n", m_name.c_str());
    m_open = false;
    if (m_constantMemory) {
        m_context.device().setVertexShaderConstantF(0, m_context.vertexConstants(), 52);
    }
}

Program* ProgramManager::find(const std::string& name) const {
    for (Program* p : m_programs) {
        if (_stricmp(p->name().c_str(), name.c_str()) == 0) return p;
    }
    return nullptr;
}

void ProgramManager::endAll() {
    for (Program* p : m_programs) p->end();
}

void ProgramManager::chooseLevel(uint32_t level) {
    endAll();
    for (Program* p : m_programs) p->chooseLevel(level);
}

// With the shader classes 0x70000 and up, the detail ([0x0079E650]) over 0.1 or not (the debug toggle instead,
// never set) against the last frame's ([0x00725784], 1 at the start): crossing to over 0.1 the level 3 on a 3.0
// device (0xF0000) else 2, crossing to 0.1 or under the level 1 (the 1.x techniques); the programs keep the last
// usable technique until a crossing or a reset. Then the request (+0x20): 0x1001 makes every program again
// (0x0051B620), another value re-picks at that value as a level (0x00519DA0); none is ever made.
void ProgramManager::frame(uint32_t classes, float detail) {
    if (classes >= 0x70000) {
        bool high = detail > 0.1f;
        if (kDetailToggle) high = (g_detailToggleFrames++ & 0x20) != 0;
        if (high) {
            if (!m_highDetail) {
                m_highDetail = true;
                chooseLevel(classes >= 0xF0000 ? 3 : 2);
            }
        } else if (m_highDetail) {
            m_highDetail = false;
            chooseLevel(1);
        }
    }
    if (m_request == 0x1000) return;
    if (m_request == 0x1001) {
        for (Program* p : m_programs) {
            while (!p->recreate()) {
            }
        }
    } else {
        chooseLevel(m_request);
    }
    m_request = 0x1000;
}

// Every program made again from its effect (0x0051B620), each until it has a usable technique: Stock problem: a
// program with none (its file not read, or no technique for the device's classes) is made again for ever and the
// game hangs at the reset. Then the shared parameters' handles (vOne, vAmbUnit, 0x00719478) are taken again from the
// first program (OpenJuice finds them by name). With the shader classes 0x70000 and up the technique of the level:
// 1 with the detail 0.1 or under, else 3 on a 3.0 device (0xF0000), else 2; [0x00725784] is left as it is.
void ProgramManager::restore(uint32_t classes, float detail) {
    for (Program* p : m_programs) {
        while (!p->recreate()) {
        }
    }
    if (classes < 0x70000) return;
    if (!(detail > 0.1f)) {
        chooseLevel(1);
    } else if (classes < 0xF0000) {
        chooseLevel(2);
    } else {
        chooseLevel(3);
    }
}

}  // namespace oj::render
