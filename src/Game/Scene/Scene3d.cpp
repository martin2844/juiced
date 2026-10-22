#include "Game/Scene/Scene3d.h"

#include "Core/Tracks/Atmosphere.h"
#include "Core/Formats/Ddt.h"
#include "Core/Formats/Effect.h"
#include "Core/Base/GameFiles.h"
#include "Core/Saves/GraphicsConfig.h"
#include "Core/Base/Log.h"
#include "Core/Formats/Model.h"
#include "Core/Input/Input.h"
#include "Game/Scene/PostEffects.h"
#include "Render/HlslCompiler.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>

namespace oj::scene {

using render::ClearFlags;
using render::ColourWrite;
using render::CompareFunc;
using render::Cull;
using render::Fvf;
using render::RenderState;
using render::StencilOp;

namespace {

const float kPi = std::bit_cast<float>(0x40490FDBu);
const float kTwoPi = std::bit_cast<float>(0x40C90FDBu);
const float kFovPi = std::bit_cast<float>(0x40490FD0u);   // 0x006FFDF8
const float kRandomScale = std::bit_cast<float>(0x38000100u);  // 1 / 32767 (0x006FF7EC)
const float kNoLimit = 9999.0f;  // a rig limit that is not set

// 0x00407910
float wrapAngle(float a) {
    while (a > kPi) a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}

// 0x00407EC0
float clampValue(float v, float low, float high) {
    if (v < low) return low;
    if (!(v <= high)) return high;
    return v;
}

std::vector<uint8_t> bytesOf(const std::vector<std::byte>& b) {
    std::vector<uint8_t> out(b.size());
    if (!b.empty()) std::memcpy(out.data(), b.data(), b.size());
    return out;
}

// The developers' sources of -localfx: the folder \src\jet\jetMaterials\dx9\fx (0x00719378) mounted under the file
// system's root "" (0x006EE8D8), so the current drive's root. The files are read from the folder above it as
// "fx\<file>", so that a fx.dat beside the folder is looked for as the mount (0x004F9970) does.
const char* const kSourceRoot = "\\src\\jet\\jetMaterials\\dx9";

// The include object's Open (0x0051B0D0): the file from the developers' folder, -localfx or not.
std::optional<std::vector<std::byte>> readSourceInclude(const std::string& name) {
    return GameFiles(std::filesystem::path(kSourceRoot)).read(std::filesystem::path("fx") / name);
}

// A procedural texture's texels: A8R8G8B8 (bytes b, g, r, a), level after level (a volume's slices one after another
// in each), a cube's faces +X, -X, +Y, -Y, +Z, -Z each with all its levels.
struct ProceduralTexture {
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t levels = 1;
    std::vector<uint8_t> pixels;
};

// D3DX's levels of a texture asked for (D3DXCheckTextureRequirements): 0, or more than the size allows, the whole
// chain down to 1.
uint32_t levelsOf(uint32_t requested, uint32_t width, uint32_t height, uint32_t depth) {
    uint32_t full = 1;
    for (uint32_t m = std::max({width, height, depth}); m > 1; m >>= 1) ++full;
    return requested == 0 || requested > full ? full : requested;
}

// A texel of a texture shader (the TX callbacks 0x005DB6E4 / 0x005DB74F into the shader's machine 0x005F69FF): the
// texel's position and size in its input registers 0 and 1 (Stock problem: a flat texture's z and w are values of
// the stock code's stack; 0 here), the shader's constants; the colour (x, y, z, w) as r, g, b, a held to 0 .. 1,
// x 255 and rounded (the converter of D3DX's fill, 0x005F6073, not followed further).
void shadeTexel(const fx::Program& shader, const std::vector<float>& constants, const float position[3],
                const float size[3], uint8_t* out) {
    fx::Registers r;
    r.input = constants;
    r.texel = {position[0], position[1], position[2], 0.0f, size[0], size[1], size[2], 0.0f};
    fx::run(shader, r);
    r.output.resize(4, 0.0f);
    auto byteOf = [](float c) {
        const float v = c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : (c == c ? c : 0.0f));
        return static_cast<uint8_t>(v * 255.0f + 0.5f);
    };
    out[0] = byteOf(r.output[2]);
    out[1] = byteOf(r.output[1]);
    out[2] = byteOf(r.output[0]);
    out[3] = byteOf(r.output[3]);
}

// D3DXFillTextureTX / FillVolumeTextureTX / FillCubeTextureTX (0x005DB944 / 0x005DB970 / 0x005DB95A → 0x005DB7C6,
// D3DXFillTexture 0x005DAB3E, D3DXFillVolumeTexture 0x005DB47D, D3DXFillCubeTexture 0x005DAD54) on the texture
// 0x0051B400 makes: a flat one width x height with the height as its levels (Stock problem: the miplevels annotation
// is not used for it), a volume width x height x depth with miplevels, a cube of the width with miplevels; a size 0
// is 1. Each level's texel at ((x + 0.5) / w, (y + 0.5) / h[, (z + 0.5) / d]), its size (1 / w, 1 / h[, 1 / d]); a
// cube's texel the direction through its centre on its face, (2i + 1) / size - 1 across, its size 2 / size on the
// face's two axes.
ProceduralTexture fillProcedural(const fx::Program& shader, const std::vector<float>& constants,
                                 const render::Program::TextureRequest& request) {
    ProceduralTexture t;
    t.width = std::max(request.width, 1u);
    t.height = request.type == 9 ? t.width : std::max(request.height, 1u);
    t.depth = request.type == 8 ? std::max(request.depth, 1u) : 1;
    t.levels = request.type == 7 ? levelsOf(request.height, t.width, t.height, 1)
                                 : levelsOf(request.mipLevels, t.width, t.height, t.depth);
    uint8_t texel[4];
    auto put = [&t, &texel] { t.pixels.insert(t.pixels.end(), texel, texel + 4); };
    if (request.type == 9) {
        for (uint32_t face = 0; face < 6; ++face) {
            for (uint32_t level = 0; level < t.levels; ++level) {
                const uint32_t n = std::max(t.width >> level, 1u);
                const float k = 1.0f / static_cast<float>(n);
                const float two = k + k;
                for (uint32_t y = 0; y < n; ++y) {
                    const float sy = (static_cast<float>(y) + static_cast<float>(y) + 1.0f) * k;
                    for (uint32_t x = 0; x < n; ++x) {
                        const float sx = (static_cast<float>(x) + static_cast<float>(x) + 1.0f) * k;
                        float p[3] = {};
                        float s[3] = {two, two, two};
                        switch (face) {
                            case 0: p[0] = 1.0f; p[1] = 1.0f - sy; p[2] = 1.0f - sx; s[0] = 0.0f; break;
                            case 1: p[0] = -1.0f; p[1] = 1.0f - sy; p[2] = sx - 1.0f; s[0] = 0.0f; break;
                            case 2: p[0] = sx - 1.0f; p[1] = 1.0f; p[2] = sy - 1.0f; s[1] = 0.0f; break;
                            case 3: p[0] = sx - 1.0f; p[1] = -1.0f; p[2] = 1.0f - sy; s[1] = 0.0f; break;
                            case 4: p[0] = sx - 1.0f; p[1] = 1.0f - sy; p[2] = 1.0f; s[2] = 0.0f; break;
                            default: p[0] = 1.0f - sx; p[1] = 1.0f - sy; p[2] = -1.0f; s[2] = 0.0f; break;
                        }
                        shadeTexel(shader, constants, p, s, texel);
                        put();
                    }
                }
            }
        }
        return t;
    }
    for (uint32_t level = 0; level < t.levels; ++level) {
        const uint32_t w = std::max(t.width >> level, 1u);
        const uint32_t h = std::max(t.height >> level, 1u);
        const uint32_t d = std::max(t.depth >> level, 1u);
        const float s[3] = {1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h),
                            request.type == 8 ? 1.0f / static_cast<float>(d) : 0.0f};
        for (uint32_t z = 0; z < d; ++z) {
            for (uint32_t y = 0; y < h; ++y) {
                for (uint32_t x = 0; x < w; ++x) {
                    const float p[3] = {(static_cast<float>(x) + 0.5f) * s[0], (static_cast<float>(y) + 0.5f) * s[1],
                                        request.type == 8 ? (static_cast<float>(z) + 0.5f) * s[2] : 0.0f};
                    shadeTexel(shader, constants, p, s, texel);
                    put();
                }
            }
        }
    }
    return t;
}

// D3DXSaveTextureToFile / D3DXSaveVolumeToFile as DDS (D3DXIFF_DDS, 4): a DDS header of 32-bit A8R8G8B8 texels and
// the levels (Stock problem: a volume is given to D3DXSaveVolumeToFile as its texture, not its volume; here its
// levels are written).
void saveDds(const std::string& path, const ProceduralTexture& t, uint32_t type) {
    uint32_t header[32] = {};
    header[0] = 0x20534444;  // "DDS "
    header[1] = 124;
    header[2] = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000 | 0x20000 | (type == 8 ? 0x800000u : 0u);
    header[3] = t.height;
    header[4] = t.width;
    header[5] = t.width * 4;
    header[6] = type == 8 ? t.depth : 0;
    header[7] = t.levels;
    header[19] = 32;
    header[20] = 0x41;  // DDPF_RGB | DDPF_ALPHAPIXELS
    header[22] = 32;
    header[23] = 0x00FF0000;
    header[24] = 0x0000FF00;
    header[25] = 0x000000FF;
    header[26] = 0xFF000000;
    header[27] = 0x1000 | (t.levels > 1 || type != 7 ? 0x8u : 0u) | (t.levels > 1 ? 0x400000u : 0u);
    header[28] = type == 9 ? 0xFE00u : (type == 8 ? 0x200000u : 0u);
    std::ofstream out(std::filesystem::path(path), std::ios::binary);
    if (!out) return;
    out.write(reinterpret_cast<const char*>(header), sizeof header);
    out.write(reinterpret_cast<const char*>(t.pixels.data()), static_cast<std::streamsize>(t.pixels.size()));
}

render::PixelFormat pixelFormat(uint32_t format) {
    switch (format) {
        case Ddt::kDxt1: return render::PixelFormat::DXT1;
        case Ddt::kDxt3: return render::PixelFormat::DXT3;
        case Ddt::kDxt5: return render::PixelFormat::DXT5;
        case Ddt::kX8R8G8B8: return render::PixelFormat::X8R8G8B8;
        default: return render::PixelFormat::A8R8G8B8;
    }
}

render::TextureId createTexture(render::IRenderDevice& device, const Ddt& ddt) {
    render::TextureData data;
    data.format = pixelFormat(ddt.format);
    data.width = ddt.width;
    data.height = ddt.height;
    data.levels = ddt.mipLevels;
    data.pixels = ddt.pixels.data();
    data.size = ddt.pixels.size();
    return device.createTexture(data);
}

// The first word of a value (the .pmd reader ends a number at a space, tab or line end).
std::string word(const std::string& s, size_t& at) {
    while (at < s.size() && (s[at] == ' ' || s[at] == '\t')) ++at;
    const size_t start = at;
    while (at < s.size() && s[at] != ' ' && s[at] != '\t' && s[at] != '\r' && s[at] != '\n') ++at;
    return s.substr(start, at - start);
}

// A .pmd value as the reader stores it for the parameter's type (0x00523760).
std::vector<uint8_t> parseValue(MaterialParamType type, const std::string& text) {
    std::vector<uint8_t> out;
    auto put = [&out](const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        out.insert(out.end(), b, b + n);
    };
    size_t at = 0;
    switch (type) {
    case MaterialParamType::Int: {
        const int32_t v = std::atoi(word(text, at).c_str());
        put(&v, 4);
        break;
    }
    case MaterialParamType::Float:
    case MaterialParamType::Fraction: {
        const float v = static_cast<float>(std::atof(word(text, at).c_str()));
        put(&v, 4);
        break;
    }
    case MaterialParamType::Bool: {
        const int32_t v = _stricmp(word(text, at).c_str(), "FALSE");
        put(&v, 4);
        break;
    }
    case MaterialParamType::Colour: {
        const uint32_t r = static_cast<uint32_t>(std::atoi(word(text, at).c_str()));
        const uint32_t g = static_cast<uint32_t>(std::atoi(word(text, at).c_str()));
        const uint32_t b = static_cast<uint32_t>(std::atoi(word(text, at).c_str()));
        const uint32_t v = (((r | 0xFFFFFF00u) << 8 | g) << 8) | b;
        put(&v, 4);
        break;
    }
    case MaterialParamType::Float3: {
        for (int i = 0; i < 3; ++i) {
            const float v = static_cast<float>(std::atof(word(text, at).c_str()));
            put(&v, 4);
        }
        break;
    }
    case MaterialParamType::Text: {
        std::string s = text.substr(0, 255);
        s.resize(256, '\0');
        put(s.data(), s.size());
        break;
    }
        default:
            break;
    }
    return out;
}

// The size a parameter's value takes in the material description (0x00719520), zeros when the .pmd has no line.
size_t valueSize(MaterialParamType type) {
    static const size_t kSizes[] = {0, 4, 4, 4, 4, 4, 12, 256};  // by the type
    const auto at = static_cast<uint32_t>(type);
    return at < std::size(kSizes) ? kSizes[at] : 0;
}

// 0x004FB590: vsprintf into the game's buffer ([0x0079EDF8]), then its characters written to the stream.
void printTo(std::string& out, const char* format, ...) {
    static char buffer[4096];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    out += buffer;
}

}  // namespace

std::string writeMaterialText(const ExeTables::MaterialClass& info, const std::vector<std::vector<uint8_t>>& values,
                              const std::vector<bool>& textured) {
    std::string out;
    printTo(out, "Material Program = %s\r\n", info.name.c_str());
    for (size_t i = 0; i < info.params.size(); ++i) {
        const ExeTables::MaterialParam& p = info.params[i];
        printTo(out, "%s = ", p.name.c_str());
        std::vector<uint8_t> v = i < values.size() ? values[i] : std::vector<uint8_t>();
        v.resize(std::max<size_t>(v.size(), 257), 0);
        int32_t word = 0;
        std::memcpy(&word, v.data(), 4);
        float f[3] = {};
        std::memcpy(f, v.data(), 12);
        switch (p.type) {
        case MaterialParamType::Int: printTo(out, "%d\r\n", word); break;
        case MaterialParamType::Float:
        case MaterialParamType::Fraction: printTo(out, "%f\r\n", static_cast<double>(f[0])); break;
        case MaterialParamType::Bool: printTo(out, "%s\r\n", word == 0 ? "FALSE" : "TRUE"); break;
        case MaterialParamType::Colour: {
            const auto c = static_cast<uint32_t>(word);
            printTo(out, "%d %d %d\r\n", (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
            break;
        }
        case MaterialParamType::Float3:
            printTo(out, "%f %f %f\r\n", static_cast<double>(f[0]), static_cast<double>(f[1]),
                    static_cast<double>(f[2]));
            break;
        case MaterialParamType::Text: printTo(out, "%s\r\n", reinterpret_cast<const char*>(v.data())); break;
        default: break;
        }
    }
    for (size_t i = 0; i < info.textures.size(); ++i) {
        if (i >= textured.size() || !textured[i]) continue;
        // Stock problem: the texture's object is given as the text (its bytes until a 0); here an empty text.
        printTo(out, "%s = %s\r\n", info.textures[i].second.c_str(), "");
    }
    return out;
}

Engine::Engine(render::IRenderDevice& device, const ExeTables& tables, uint32_t shadersSetting, bool wideScreen,
               bool localEffects, bool debugEffects)
    : m_device(device),
      m_tables(tables),
      m_wideScreen(wideScreen),
      m_programs(device, render::shaderClasses(device.vertexShaderVersion(), device.pixelShaderVersion(),
                                               shadersSetting)),
      m_renderer(device, m_programs) {
    m_localEffects = localEffects;
    m_debugEffects = debugEffects;
    log::info("3d: shader classes 0x{:X}", m_programs.classes());
    m_renderer.reset();
    // The edge is the settings' +0x10: 64 << [0x0071E5DC] (1).
    m_halfVectorCube = render::makeRendererCube(device, 128, true);
    m_renderer.halfVectorCube = m_halfVectorCube.id != 0 ? &m_halfVectorCube : nullptr;
    m_normalCube = render::makeRendererCube(device, 128, false);
    m_renderer.normalCube = m_normalCube.id != 0 ? &m_normalCube : nullptr;
    // The material manager's start (0x005226E0 → 0x00519D00): the programs of 0x00519840, in its order.
    static const char* const kStartPrograms[] = {
        "Material_2DBlur", "Material_BuildingOpaque", "Material_Tarmac", "Material_2TexModulare",
        "Material_2TexModulareSpec", "Material_AmbientLit", "Material_AnimAlphaAdd", "Material_Antialias",
        "Material_BlurredShadow", "Material_Building", "Material_BuildingNight", "Material_BuildingOpaqueNight",
        "Material_CarBodyBase", "Material_CarBody_Paint_NoReflect", "Material_CarGlass", "Material_Chrome",
        "Material_CloudSprite", "Material_ColouredSpecABlend", "Material_ColouredSpecABlendAdditive",
        "Material_ColouredSpecOpaque", "Material_ConstColour", "Material_CrowdSprite", "Material_CrowdSpriteAnim",
        "Material_Exposure", "Material_Font", "Material_Glass", "Material_Grass", "Material_LightFlare",
        "Material_Line", "Material_LitSprite", "Material_MotionBlur", "Material_SeaNoShadow", "Material_SeaShadow",
        "Material_SeaShadowNight", "Material_ShinyOpaque", "Material_ShinyUnlitATest", "Material_ShinyUnlitOpaque",
        "Material_Sky", "Material_SpecATest", "Material_SpecMapOpaque", "Material_SpecOpaque", "Material_StdABlend",
        "Material_StdATestNoShadow", "Material_StdATestShadow", "Material_StdATestShadowNight",
        "Material_StdFadeOut", "Material_StdOpaqueNoShad", "Material_StdOpaqueShad", "Material_StdOpaqueShadNight",
        "Material_TarmacNight", "Material_TarmacNoShadow", "Material_TexShape2D", "Material_TreeSprite",
        "Material_TreeStandard", "Material_UnlitABlend", "Material_UnlitATest", "Material_UnlitOpaque",
        "Material_UnlitOpaque2nd", "Material_UnlitSignAnim", "Material_UnlitSignScroll", "Material_UnlitSprite",
        "Material_VertCol2Tex", "Material_VertColABlend", "Material_VertColATest", "Material_VertColOpaque",
        "Material_VertColoured", "Material_VertColouredUnlit", "Material_Water", "Material_WaterTool",
        "Material_HeatHaze", "Material_SceneColorMatrix", "Material_VertColABlendRefract",
        "Material_FadeToConstColour",
    };
    for (const char* name : kStartPrograms) makeProgram(name);
    m_effectsResets = device.resets();
}

Engine::~Engine() = default;

FlareDevice& Engine::flareDevice() {
    if (!m_flareDevice) m_flareDevice = std::make_unique<FlareDevice>();
    return *m_flareDevice;
}

PostEffects& Engine::postEffects() {
    if (!m_postEffects) m_postEffects = std::make_unique<PostEffects>(*this);
    return *m_postEffects;
}

Engine::ReflectionTargets& Engine::reflectionTargets() {
    if (!m_reflections) {
        m_reflections = std::make_unique<ReflectionTargets>();
        const GraphicsConfig& c = graphicsConfig();
        const render::SamplerParams clamped{2, 2, 2, 3, 3, 0.0f};
        m_reflections->carCube.id = m_device.createRenderTarget(c.cubeSize, c.cubeSize, true);
        m_reflections->road.id = m_device.createRenderTarget(c.reflectionWidth, c.reflectionHeight, false);
        m_reflections->worldCube.id = m_device.createRenderTarget(c.worldCubeSize, c.worldCubeSize, true);
        m_reflections->carCube.sampler = clamped;
        m_reflections->road.sampler = clamped;
        m_reflections->worldCube.sampler = clamped;
        m_reflections->resets = m_device.resets();
        log::info("3d: reflection targets: cars' cube {} ({}), road {}x{} ({}), world's cube {} ({})", c.cubeSize,
                  m_reflections->carCube.id != 0 ? "made" : "none", c.reflectionWidth, c.reflectionHeight,
                  m_reflections->road.id != 0 ? "made" : "none", c.worldCubeSize,
                  m_reflections->worldCube.id != 0 ? "made" : "none");
    }
    if (m_reflections->resets != m_device.resets()) {
        m_reflections->resets = m_device.resets();
        m_reflections->worldCubeDue = true;
    }
    return *m_reflections;
}

GameFiles Engine::effectFiles() const {
    return GameFiles(m_localEffects ? std::filesystem::path(kSourceRoot) : std::filesystem::current_path());
}

std::filesystem::path Engine::effectFile(const std::string& file) const {
    return std::filesystem::path(m_localEffects ? "fx" : "effects") / file;
}

// 0x0051B7D0: the effect of <name>.fxo, with -localfx of <name>.fx (0x006F7970 / 0x006F7968), made (D3DXCreateEffect)
// with the flags 0x100 (D3DXSHADER_NO_PRESHADER; 0x141 with -debugfx: D3DXSHADER_DEBUG and FORCE_VS_SOFTWARE_NOOPT
// as well), the programs' pool and the include object; a source is compiled (fx_2_0) here by d3dcompiler_47.dll.
void Engine::makeProgram(const std::string& name) {
    const auto data = effectFiles().read(effectFile(name + (m_localEffects ? ".fx" : ".fxo")));
    std::optional<fx::Effect> effect;
    if (data && m_localEffects) {
        const uint32_t flags = 0x100u | (m_debugEffects ? 0x41u : 0u);
        if (const auto code = render::hlsl::compile(*data, nullptr, "fx_2_0", flags, readSourceInclude)) {
            effect = fx::parseEffect(*code);
        }
    } else if (data) {
        effect = fx::parseEffect(bytesOf(*data));
    }
    if (!effect) log::warning("3d: effect {} not read", name);
    auto program = std::make_unique<render::Program>(
        m_programs, name, effect ? std::move(*effect) : fx::Effect{},
        [this](const render::Program& p, const render::Program::TextureRequest& r) { return effectTexture(p, r); });
    if (effect && !program->usable()) log::warning("3d: effect {} has no usable technique", name);
    m_effects.add(program.get());
    m_programList.push_back(std::move(program));
}

// The end of 0x0051AAC0: the file of the name annotation; else with -localfx the texture made from the function
// (0x0051B400), else the saved one, proc_<program>_<function>.dds (0x0051B2C0).
render::TextureId Engine::effectTexture(const render::Program& program,
                                        const render::Program::TextureRequest& request) {
    if (!request.file.empty()) return loadEffectTexture(request.file);
    if (m_localEffects) return makeProceduralTexture(program, request);
    return loadEffectTexture(proceduralName(program, request));
}

render::TextureId Engine::loadEffectTexture(const std::string& file) {
    const auto cached = m_effectTextures.find(file);
    if (cached != m_effectTextures.end()) return cached->second;
    render::TextureId id = 0;
    if (const auto bytes = effectFiles().read(effectFile(file))) {
        // D3DXCreateTextureFromFileInMemory (0x005DCC14).
        Ddt dds;
        if (dds.parseDds(*bytes)) id = createTexture(m_device, dds);
        if (id == 0) log::warning("3d: effect texture {} not loaded", file);
    } else {
        // D3DXCreateTexture (0x005DA9EB): 16 x 16, its full chain of levels, A8R8G8B8, managed, its texels never
        // written (0 here).
        const std::vector<uint8_t> pixels((16 * 16 + 8 * 8 + 4 * 4 + 2 * 2 + 1) * 4, 0);
        render::TextureData data;
        data.format = render::PixelFormat::A8R8G8B8;
        data.width = 16;
        data.height = 16;
        data.levels = 5;
        data.pixels = pixels.data();
        data.size = pixels.size();
        id = m_device.createTexture(data);
        log::debugPrint("missing texture %s\n", file.c_str());
        log::warning("3d: effect texture {} missing", file);
    }
    m_effectTextures[file] = id;
    return id;
}

std::string Engine::proceduralName(const render::Program& program,
                                   const render::Program::TextureRequest& request) const {
    // Stock problem: a texture without a function annotation copies its name from address 0 (a crash); here the
    // name is empty.
    const std::string folder = m_localEffects ? std::string(kSourceRoot) + "\\fx\\" : std::string();
    return folder + "proc_" + program.name() + "_" + request.function + ".dds";
}

render::TextureId Engine::makeProceduralTexture(const render::Program& program,
                                                const render::Program::TextureRequest& request) {
    // The function compiled as a texture shader (tx_1_0, flags 0) from the program's source (its .fx again).
    const auto source = effectFiles().read(effectFile(program.name() + ".fx"));
    std::optional<std::vector<uint8_t>> code;
    if (source && !m_localEffects) {
        // Never: 0x0051AAC0 calls this only with -localfx. The function through an effect compiler of the file
        // (D3DXCreateEffectCompiler 0x005DD680, its +0x44 and +0xF8), without the include object.
        log::debugPrint("procedural texture generation expected only with -l");
        code = render::hlsl::compile(*source, request.function.c_str(), "tx_1_0", 0, {});
    } else if (source) {
        // D3DXCompileShader (0x005DCC72) with the include object.
        code = render::hlsl::compile(*source, request.function.c_str(), "tx_1_0", 0, readSourceInclude);
    }
    // Stock problem: a function that does not compile leaves the shader's buffer at 0 and the texture shader is made
    // from address 0 (a crash); here there is no texture.
    std::optional<fx::Program> shader = code ? fx::parseProgram(fx::words(*code)) : std::nullopt;
    if (!shader) {
        log::warning("3d: procedural texture {} of {} not made", request.function, program.name());
        return 0;
    }
    // D3DXCreateTextureShader (0x005DD5CE): the constants at their defaults.
    std::vector<float> constants;
    for (const fx::Constant& c : shader->inputs.constants) {
        const size_t at = static_cast<size_t>(c.registerIndex) * 4;
        if (constants.size() < at + c.defaults.size()) constants.resize(at + c.defaults.size(), 0.0f);
        std::copy(c.defaults.begin(), c.defaults.end(), constants.begin() + static_cast<ptrdiff_t>(at));
    }
    const ProceduralTexture made = fillProcedural(*shader, constants, request);
    render::TextureId id = 0;
    if (request.type == 7 || request.type == 9) {
        render::TextureData data;
        data.format = render::PixelFormat::A8R8G8B8;
        data.width = made.width;
        data.height = made.height;
        data.levels = made.levels;
        data.pixels = made.pixels.data();
        data.size = made.pixels.size();
        id = request.type == 9 ? m_device.createCubeTexture(data) : m_device.createTexture(data);
    }
    // A volume (8) is filled and saved; OpenJuice's device has no volume textures, the parameter gets none.
    // D3DXSaveTextureToFile / D3DXSaveVolumeToFile (0x005DCBF8 / 0x005DA5C9): DDS, at 0x0051B2C0's path.
    if (request.type >= 7 && request.type <= 9) saveDds(proceduralName(program, request), made, request.type);
    return id;
}

render::Program* Engine::program(const std::string& name) {
    render::Program* p = m_effects.find(name);
    return p != nullptr && p->usable() ? p : nullptr;
}

std::string Engine::cacheKey(const std::filesystem::path& folder, const char* own, const std::string& name) {
    return folder == own ? name : folder.string() + "|" + name;
}

void Engine::useCarFolders() {
    const LoadLock lock(*this);
    m_keptFolders = m_folders;
    m_folders.models = "carmodels";
    m_folders.mesh = "carmesh";
    m_folders.mats = std::filesystem::path("carmesh") / "mats";
    m_folders.textures = "cartextures";
}

void Engine::restoreFolders() {
    const LoadLock lock(*this);
    m_folders = m_keptFolders;
}

Engine::LoadLock::LoadLock(Engine& engine) : m_engine(engine) {
    while (!engine.m_loadLock.try_lock()) {
        engine.m_device.letLoadingRun();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

Engine::LoadLock::~LoadLock() { m_engine.m_loadLock.unlock(); }

const render::GameTexture* Engine::texture(const std::string& name) {
    const LoadLock lock(*this);
    const std::string key = cacheKey(m_folders.textures, "textures", name);
    const auto it = m_textures.find(key);
    if (it != m_textures.end()) return it->second.get();
    const GameFiles files(std::filesystem::current_path());
    const auto bytes = files.read(m_folders.textures / (name + ".ddt"));
    Ddt ddt;
    std::unique_ptr<render::GameTexture> texture;
    if (bytes && ddt.parse(*bytes)) {
        texture = std::make_unique<render::GameTexture>();
        texture->id = createTexture(m_device, ddt);
        texture->sampler = {ddt.minFilter, ddt.magFilter, ddt.mipFilter, ddt.addressU, ddt.addressV, ddt.mipLodBias};
    } else {
        log::warning("3d: texture {} not loaded", name);
    }
    const render::GameTexture* result = texture.get();
    m_textures[key] = std::move(texture);
    return result;
}

const render::GameTexture* Engine::cubeTexture(const std::string& name) {
    const LoadLock lock(*this);
    const auto it = m_cubeTextures.find(name);
    if (it != m_cubeTextures.end()) return it->second.get();
    const GameFiles files(std::filesystem::current_path());
    const auto bytes = files.read(std::filesystem::path("textures") / (name + ".dds"));
    Ddt dds;
    std::unique_ptr<render::GameTexture> texture;
    if (bytes && dds.parseDds(*bytes) && dds.faces == 6 && dds.width == dds.height) {
        // D3DX keeps the file's format and fills the mip levels down to 1 x 1 (box filter); OpenJuice keeps the
        // filtered levels as 32-bit pixels where D3DX compresses them again.
        uint32_t levels = 0;
        const std::vector<uint32_t> pixels = fullMipChain(dds, levels);
        render::TextureData data;
        data.format = render::PixelFormat::A8R8G8B8;
        data.width = dds.width;
        data.height = dds.height;
        data.levels = levels;
        data.pixels = reinterpret_cast<const uint8_t*>(pixels.data());
        data.size = pixels.size() * 4;
        const render::TextureId id = pixels.empty() ? 0 : m_device.createCubeTexture(data);
        if (id != 0) {
            texture = std::make_unique<render::GameTexture>();
            texture->id = id;
            texture->sampler = {2, 2, 2, 3, 3, 0.0f};
        }
    }
    if (!texture) log::warning("3d: cube texture {} not loaded", name);
    const render::GameTexture* result = texture.get();
    m_cubeTextures[name] = std::move(texture);
    return result;
}

render::Material* Engine::material(const std::string& part) {
    auto material = loadMaterial(m_folders.mats, part, {});
    if (!material) return nullptr;
    m_materials.push_back(std::move(material));
    return m_materials.back().get();
}

const render::GameTexture* Engine::texture(const std::string& name, const std::filesystem::path& folder,
                                           const std::filesystem::path& shared) {
    const LoadLock lock(*this);
    if (folder.empty()) return shared.empty() ? texture(name) : texture(name, shared);
    const std::string key = folder.string() + "|" + name;
    const auto it = m_textures.find(key);
    if (it != m_textures.end()) return it->second ? it->second.get() : texture(name, shared);
    const GameFiles files(std::filesystem::current_path());
    const auto bytes = files.read(folder / (name + ".ddt"));
    std::unique_ptr<render::GameTexture> made = bytes ? makeTexture(*bytes) : nullptr;
    const render::GameTexture* result = made.get();
    m_textures[key] = std::move(made);
    return result != nullptr ? result : texture(name, shared);
}

std::unique_ptr<render::Material> Engine::loadMaterial(const std::filesystem::path& mats, const std::string& part,
                                                       const std::filesystem::path& textures,
                                                       const std::filesystem::path& shared) {
    const LoadLock lock(*this);
    const GameFiles files(std::filesystem::current_path());
    const auto bytes = files.read(mats / (part + ".pmd"));
    if (!bytes) {
        log::warning("3d: material {} not found", part);
        return nullptr;
    }
    const Material pmd = parseMaterial(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
    const ExeTables::MaterialClass* info = m_tables.materialClass(pmd.className);
    if (info == nullptr) {
        log::warning("3d: material {}: no class {}", part, pmd.className);
        return nullptr;
    }
    auto lookup = [this](const std::string& n) { return program(n); };
    auto material = render::createMaterial(*info, m_renderer, lookup, m_tables);
    if (!material) {
        log::warning("3d: material {}: class {} has no material", part, pmd.className);
        return nullptr;
    }
    // The description keeps every parameter (zeros for a key the file leaves out); the object takes them in the
    // class's order.
    std::vector<std::vector<uint8_t>> description;
    for (const ExeTables::MaterialParam& p : info->params) {
        std::vector<uint8_t> value(valueSize(p.type), 0);
        for (const auto& [key, text] : pmd.values) {
            if (_stricmp(key.c_str(), p.name.c_str()) == 0) {
                std::vector<uint8_t> parsed = parseValue(p.type, text);
                if (!parsed.empty()) value = std::move(parsed);
            }
        }
        if (!value.empty()) material->setParam(p.index, value.data());
        description.push_back(std::move(value));
    }
    material->setDescription(std::move(description));
    for (const auto& [slot, slotName] : info->textures) {
        for (const auto& [key, text] : pmd.values) {
            if (_stricmp(key.c_str(), slotName.c_str()) != 0) continue;
            const std::string name = textureName(text);
            if (name.empty() || _stricmp(name.c_str(), "(null)") == 0) continue;
            material->setTexture(slot, texture(name, textures, shared));
        }
    }
    return material;
}

render::Mesh* Engine::mesh(const std::string& name) {
    const LoadLock lock(*this);
    const std::string key = cacheKey(m_folders.mesh, "mesh", name);
    const auto it = m_meshes.find(key);
    if (it != m_meshes.end()) return it->second.get();
    std::unique_ptr<render::Mesh> mesh = readMesh(name);
    render::Mesh* result = mesh.get();
    m_meshes[key] = std::move(mesh);
    return result;
}

std::unique_ptr<render::Mesh> Engine::readMesh(const std::string& name) {
    const GameFiles files(std::filesystem::current_path());
    const auto bytes = files.read(m_folders.mesh / (name + ".d8m"));
    const auto parsed = bytes ? parseMesh(bytesOf(*bytes)) : std::nullopt;
    if (!parsed) {
        log::warning("3d: mesh {} not read", name);
        return nullptr;
    }
    std::vector<render::Material*> materials;
    for (const MeshPart& p : parsed->parts) materials.push_back(material(p.name));
    return std::make_unique<render::Mesh>(m_device, *parsed, materials);
}

bool Engine::reloadMesh(render::Mesh& mesh, const std::string& name) {
    const LoadLock lock(*this);
    const GameFiles files(std::filesystem::current_path());
    const auto bytes = files.read(m_folders.mesh / (name + ".d8m"));
    const auto parsed = bytes ? parseMesh(bytesOf(*bytes)) : std::nullopt;
    if (!parsed) return false;
    return mesh.reloadVertices(m_device, *parsed);
}

void Model::setHidden(bool on) {
    if (auto* node = dynamic_cast<render::ModelNode*>(root)) {
        node->flags = on ? (node->flags | 4u) : (node->flags & ~4u);
    }
}

bool Model::hidden() const {
    const auto* node = dynamic_cast<const render::ModelNode*>(root);
    return node != nullptr && (node->flags & 4u) != 0;
}

render::Node* Model::find(const char* text) const {
    for (const auto& [nodeName, node] : named) {
        if (std::strstr(nodeName.c_str(), text) != nullptr) return node;
    }
    return nullptr;
}

void Model::mirror() {
    if (root == nullptr) return;
    for (auto& node : meshNodes) node->flags |= 1;
    Mat34& m = root->local;
    m[1] = -m[1];
    m[5] = -m[5];
    m[9] = -m[9];
    m[7] = -m[7];
    root->invalidate();
}

std::unique_ptr<Model> Engine::loadModel(const std::string& name, CrtRandom& random, bool ownMeshes) {
    const LoadLock lock(*this);
    const GameFiles files(std::filesystem::current_path());
    const auto bytes = files.read(m_folders.models / (name + ".boh"));
    const auto tree = bytes ? parseBoh(bytesOf(*bytes)) : std::nullopt;
    if (!tree || tree->empty()) {
        log::warning("3d: model {} not read", name);
        return nullptr;
    }
    auto model = std::make_unique<Model>();
    // 0x00534F50: depth first; a mesh node draws its four random values when it is made, before its mesh loads.
    std::function<render::Node*(const BohNode&, render::Node*)> build = [&](const BohNode& b,
                                                                            render::Node* parent) -> render::Node* {
        render::Node* node = nullptr;
        if (b.attach == 0) {
            render::Mesh* m = nullptr;
            if (_stricmp(b.mesh.c_str(), "NONE") != 0) {
                if (m_meshCaching && !ownMeshes) {
                    m = mesh(b.mesh);
                } else if (std::unique_ptr<render::Mesh> own = readMesh(b.mesh)) {
                    m = own.get();
                    model->ownMeshes.push_back(std::move(own));
                }
            }
            oj::Vec4 r{};
            for (float& v : r) v = static_cast<float>(random.next()) * kRandomScale;
            auto n = std::make_unique<render::ModelNode>(r);
            n->mesh = m;
            node = n.get();
            model->meshNodes.push_back(std::move(n));
        } else {
            auto n = std::make_unique<render::Node>();
            node = n.get();
            model->otherNodes.push_back(std::move(n));
        }
        std::copy_n(b.matrix.begin(), 12, node->local.begin());
        if (parent != nullptr) {
            parent->attach(node);
            model->named.emplace_back(b.name, node);
        }
        node->invalidate();
        for (const BohNode& child : b.children) build(child, node);
        return node;
    };
    for (const BohNode& top : *tree) {
        render::Node* n = build(top, nullptr);
        if (model->root == nullptr) model->root = n;
    }
    log::info("3d: model {}: {} mesh nodes", name, model->meshNodes.size());
    return model;
}

std::unique_ptr<render::Material> Engine::newMaterial(uint32_t classId, const ExeTables::MaterialClass** info) {
    const LoadLock lock(*this);
    const ExeTables::MaterialClass* c = m_tables.materialClass(classId);
    if (info != nullptr) *info = c;
    if (c == nullptr) return nullptr;
    return render::createMaterial(*c, m_renderer, [this](const std::string& n) { return program(n); }, m_tables);
}

std::unique_ptr<render::GameTexture> Engine::makeTexture(const std::vector<std::byte>& bytes) {
    const LoadLock lock(*this);
    Ddt ddt;
    if (!ddt.parse(bytes)) return nullptr;
    auto texture = std::make_unique<render::GameTexture>();
    texture->id = createTexture(m_device, ddt);
    texture->sampler = {ddt.minFilter, ddt.magFilter, ddt.mipFilter, ddt.addressU, ddt.addressV, ddt.mipLodBias};
    if (texture->id == 0) return nullptr;
    return texture;
}

std::unique_ptr<render::Material> Engine::newMaterial(const std::string& className) {
    const LoadLock lock(*this);
    const ExeTables::MaterialClass* info = m_tables.materialClass(className);
    if (info == nullptr) return nullptr;
    return render::createMaterial(*info, m_renderer, [this](const std::string& n) { return program(n); }, m_tables);
}

void Engine::frameParameters() {
    const uint32_t classes = m_programs.classes();
    const float detail = graphicsConfig().detail;
    // After the device's reset (0x00514090): the constants and render states every frame starts from (0x00514330:
    // c24, c39 are written nowhere else), then every program made again (0x00519700).
    if (m_device.resets() != m_effectsResets) {
        m_effectsResets = m_device.resets();
        m_renderer.reset();
        m_effects.restore(classes, detail);
    }
    m_effects.endAll();
    m_effects.frame(classes, detail);
    auto set = [this](const char* name, const float (&v)[4]) {
        const auto it = m_programs.shared.find(name);
        if (it == m_programs.shared.end()) return;
        std::memcpy(it->second.data(), v, std::min(it->second.size(), sizeof v));
    };
    const Vec4& a = m_renderer.ambient;
    const float k = 0.003921569f;
    set("vAmbUnit", {a[0] * k, a[1] * k, a[2] * k, a[3]});
    set("vOne", {1.0f, 1.0f, 1.0f, 1.0f});
}

void setViewConstants(render::Renderer3d& r, render::Camera& camera) {
    r.setVertexConstants(7, camera.view().data(), 3);
    r.setVertexConstants(10, camera.projection().data(), 4);
    r.setVertexConstants(14, camera.worldToClip().data(), 4);
    const oj::Mat34& c = camera.world();
    const float position[4] = {c[3], c[7], c[11], 0.0f};
    r.setVertexConstants(32, position, 1);
    // Stock problem: w of c33 is a stack value of the stock code; 0 here.
    const float direction[4] = {c[2], c[6], c[10], 0.0f};
    r.setVertexConstants(33, direction, 1);
    float sprite[8];
    spriteAxes(c[2], c[6], c[10], sprite);
    r.setVertexConstants(41, sprite, 2);
}

void identityObject(render::Renderer3d& r, render::Camera& camera) {
    const float identity[12] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    r.setVertexConstants(0, identity, 3);
    r.setVertexConstants(4, identity, 3);
    r.setVertexConstants(18, camera.worldToClip().data(), 4);
    const float ones[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    r.setVertexConstants(3, ones, 1);
}

void spriteAxes(float fx, float fy, float fz, float out[8]) {
    auto normal = [](float x, float y, float z, float* to) {
        const float w = 0.0f;
        const float squares = ((w * w + z * z) + y * y) + x * x;
        const float k = 1.0f / (std::sqrt(squares) + 1.1920929e-07f);
        to[0] = k * x;
        to[1] = k * y;
        to[2] = k * z;
        to[3] = k * w;
    };
    normal(1.0f * fz - 0.0f * fy, 0.0f * fx - 0.0f * fz, 0.0f * fy - 1.0f * fx, out);
    normal(out[2] * fy - out[1] * fz, out[0] * fz - out[2] * fx, out[1] * fx - out[0] * fy, out + 4);
}

View3d::View3d(Engine& engine, const ui::Element& element, CrtRandom& random) : m_engine(engine), m_flares(engine) {
    const bool wide = engine.wideScreen();
    if (wide) m_camera.lodFactor = 0.5f;
    m_camera.setFov(0.35f * kFovPi, 0.0f, wide);
    m_camera.setFar(10000000.0f);
    m_camera.setAspect(wide ? 0.5625f : 0.75f);
    m_snap = true;

    auto number = [](const ui::Property* p, size_t i) {
        return p != nullptr && i < p->values.size() ? static_cast<float>(std::atof(p->values[i].c_str())) : 0.0f;
    };
    if (const ui::Property* p = element.find("MoveMode"); p != nullptr && !p->values.empty()) {
        const std::string& m = p->values[0];
        if (_stricmp(m.c_str(), "MODEL_MANUAL") == 0)
            m_mode = MoveMode::ModelManual;
        else if (_stricmp(m.c_str(), "CAMERA_AUTO") == 0)
            m_mode = MoveMode::CameraAuto;
        else if (_stricmp(m.c_str(), "CAMERA_MANUAL") == 0)
            m_mode = MoveMode::CameraManual;
        else
            m_mode = MoveMode::ModelAuto;
    }
    if (const ui::Property* p = element.find("CameraPosition")) {
        m_target = {number(p, 0), number(p, 1), number(p, 2), 0.0f};
        m_camera.setPosition(m_target[0], m_target[1], m_target[2]);
    }
    if (const ui::Property* p = element.find("CameraLookAt")) {
        lookAt({number(p, 0), number(p, 1), number(p, 2), 0.0f}, true);
    }
    if (const ui::Property* p = element.find("Ambient")) {
        m_world.ambient = {number(p, 0), number(p, 1), number(p, 2), m_world.ambient[3]};
    }
    if (const ui::Property* p = element.find("Directional")) {
        m_world.light1Colour = {number(p, 0), number(p, 1), number(p, 2), m_world.light1Colour[3]};
        m_world.direction1 = {number(p, 3), number(p, 4), number(p, 5), m_world.direction1[3]};
    }
    if (const ui::Property* p = element.find("Atmosphere"); p != nullptr && !p->values.empty()) {
        setAtmosphere(p->values[0]);
    }
    if (const ui::Property* p = element.find("SetModel"); p != nullptr && p->values.size() >= 4) {
        m_model = engine.loadModel(p->values[3], random);
        if (m_model) m_model->name = p->values[3];
        if (m_model && m_model->root != nullptr) {
            m_model->root->setPosition(number(p, 0), number(p, 1), number(p, 2));
        }
    }
    // ModelOrientation x y z (0x005ACC70; 0 0 0 after SetModel in eight windows): the list's last model (the
    // element's own here) placed by 0x005B3520: its node the holder's scale (1, 0x005B3420) on the diagonal, at the
    // holder's place (SetModel's), turned about y, x and z by the angles (+0x50).
    if (const ui::Property* p = element.find("ModelOrientation"); p != nullptr && m_model && m_model->root != nullptr) {
        render::Node* node = m_model->root;
        const ui::Property* place = element.find("SetModel");
        node->local = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
        node->invalidate();
        node->setPosition(number(place, 0), number(place, 1), number(place, 2));
        oj::rotateY(node->local, number(p, 1));
        oj::rotateX(node->local, number(p, 0));
        oj::rotateZ(node->local, number(p, 2));
        node->invalidate();
    }
    // ModelRotation x y z (0x005ACD50; 0 0 0 in the same windows): the last model's turns a second about x, y and z
    // (the holder's +0x60, +0x64, +0x68), stepped each frame (0x005B3660): the own model's node turns by them.
    if (const ui::Property* p = element.find("ModelRotation"); p != nullptr && m_model && m_model->root != nullptr) {
        const float rates[3] = {number(p, 0), number(p, 1), number(p, 2)};
        if (rates[0] != 0.0f || rates[1] != 0.0f || rates[2] != 0.0f) {
            Listed entry;
            entry.node = m_model->root;
            std::copy_n(rates, 3, entry.rates);
            m_listed.push_back(entry);
        }
    }
    // CameraOrientation x y z (0x005ACFD0; no window has one): the camera's angles (+0x140 ..) and at once (+0x150 ..).
    if (const ui::Property* p = element.find("CameraOrientation")) {
        m_targetAngles[0] = number(p, 0);
        m_targetAngles[1] = number(p, 1);
        m_targetAngles[2] = number(p, 2);
        std::copy_n(m_targetAngles, 4, m_angles);
    }
}

int View3d::addModel(const std::string& name, const oj::Vec4& place, CrtRandom& random) {
    std::unique_ptr<Model> model = m_engine.loadModel(name, random);
    if (!model || model->root == nullptr) return -1;
    model->name = name;
    model->root->setPosition(place[0], place[1], place[2]);
    m_models.push_back(std::move(model));
    return static_cast<int>(m_models.size()) - 1;
}

void View3d::removeModel(int index) {
    if (index >= 0 && index < static_cast<int>(m_models.size())) m_models.erase(m_models.begin() + index);
}

void View3d::placeModel(int index, const oj::Vec4& place, const oj::Vec4& angles, float scale) {
    render::Node* node = m_models[static_cast<size_t>(index)]->root;
    node->local = {scale, 0.0f, 0.0f, 0.0f, 0.0f, scale, 0.0f, 0.0f, 0.0f, 0.0f, scale, 0.0f};
    node->invalidate();
    node->setPosition(place[0], place[1], place[2]);
    oj::rotateY(node->local, angles[1]);
    oj::rotateX(node->local, angles[0]);
    oj::rotateZ(node->local, angles[2]);
    node->invalidate();
}

void View3d::setCameraNow(const oj::Vec4& place, float pitch, float yaw) {
    m_target = place;
    m_camera.setPosition(place[0], place[1], place[2]);
    m_targetAngles[0] = pitch;
    m_targetAngles[1] = yaw;
    m_targetAngles[2] = 0.0f;
    std::copy_n(m_targetAngles, 4, m_angles);
}

void View3d::setCameraPlace(const oj::Vec4& place) {
    m_target = place;
    m_camera.setPosition(place[0], place[1], place[2]);
}

int View3d::listNode(render::Node* node, bool parent) {
    Listed entry;
    entry.node = node;
    entry.parent = parent;
    m_listed.push_back(entry);
    return static_cast<int>(m_listed.size()) - 1;
}

void View3d::setTurnRates(int index, float x, float y, float z) {
    Listed& e = m_listed[static_cast<size_t>(index)];
    e.rates[0] = x;
    e.rates[1] = y;
    e.rates[2] = z;
}

void View3d::setAtmosphere(const std::string& name) {
    m_world.atmosphere.reset();
    const GameFiles files(std::filesystem::current_path());
    const auto bytes = files.read(std::filesystem::path("ui") / "atmospheres" / (name + ".atm"));
    if (!bytes) {
        log::info("3d: atmosphere {} not found", name);
        return;
    }
    auto set = std::make_unique<render::AtmosphereSet>();
    set->list = parseAtmospheres(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
    if (set->list.empty()) return;
    set->current = &set->list.back();
    // 0x00533950: the set's loading gives its atmosphere in use +0x1C0 = 1 (the renderer's +0x188 too).
    set->current->drawQuality = 1.0f;
    m_world.direction1 = render::atmosphereLightDirection(*set);
    // 0x0054DBC0: the atmosphere's environment map, else arcade_env.
    m_world.environment = m_engine.cubeTexture(set->current->envmap.empty() ? "arcade_env" : set->current->envmap);
    m_world.atmosphere = std::move(set);
}

void View3d::buildSky() {
    if (!m_world.atmosphere) return;
    const Atmosphere& a = *m_world.atmosphere->current;
    if (!a.sunFlare.empty() && m_engine.texture(a.sunFlare) != nullptr) {
        auto sun = std::make_unique<SceneFlare>();
        sun->texture = a.sunFlare;
        sun->size = a.sunFlareSize * 2000.0f;
        sun->cosMin = 9.0f;
        sun->cosMax = 10.0f;
        sun->offset = 0.0f;
        sun->screen = 200000.0f;
        sun->sunAngle = a.sunAngle;
        sun->roadWidth = 100.0f;
        sun->roadHeight = 200.0f;
        sun->roadAlphaTop = 128.0f;
        sun->roadAlphaBottom = 64.0f;
        sun->reflection = a.sunReflection.empty() ? std::string() : a.sunFlare;
        sun->reflectionSize = a.sunReflection.empty() ? 300.0f : 200000.0f * a.sunReflectionSize * 0.01f;
        LensFlares::Flare f;
        f.def = sun.get();
        f.batch = m_flares.batch(a.sunFlare);
        m_flares.makeReflectionBatches(f);
        m_flares.flares().push_back(f);
        m_flares.makeDevice();
        m_suns.push_back(std::move(sun));
        log::info("3d: sun flare {}", a.sunFlare);
    }
    m_world.sky = std::make_unique<render::SkyDome>(m_engine.renderer(), m_engine.newMaterial("Sky"),
                                                    render::skyColours(m_world.atmosphere.get()));
    log::info("3d: sky dome of {} vertices", m_world.sky->vertices().size());
}

void View3d::lookAt(const oj::Vec4& point, bool now) {
    m_lookAt = point;
    const float dx = point[0] - m_target[0];
    const float dy = point[1] - m_target[1];
    const float dz = point[2] - m_target[2];
    // fpatan keeps the extended precision; the other operations round to single precision (the main thread's x87).
    const float yaw = static_cast<float>(-std::atan2(static_cast<double>(dx), static_cast<double>(dz)));
    const float across = std::sqrt(dz * dz + dx * dx);
    const double pitch = -std::atan2(-static_cast<double>(dy), static_cast<double>(across));
    m_targetAngles[1] = yaw < 0.0f ? yaw + kTwoPi : yaw;
    m_targetAngles[0] = static_cast<float>(pitch < 0.0 ? pitch + static_cast<double>(kTwoPi) : pitch);
    m_targetAngles[2] = 0.0f;
    if (now) std::copy_n(m_targetAngles, 4, m_angles);
}

void View3d::setMode(MoveMode mode, const oj::Vec4* arm) {
    if (m_mode == mode) return;
    if (m_mode == MoveMode::Map) {
        // Out of the map mode the camera stays where it is and the arm leaves the pivot (keeping its place).
        const oj::Mat34& w = m_camera.world();
        const float x = w[3], y = w[7], z = w[11];
        m_arm.detach();
        lookAt(m_lookAt, false);
        m_camera.setPosition(x, y, z);
        m_zoomTarget = 1.0f;
    }
    m_mode = mode;
    m_snap = true;
    if (mode != MoveMode::Map) return;
    m_pivot.attach(&m_arm);
    m_pivot.setPosition(m_lookAt[0], m_lookAt[1], m_lookAt[2]);
    if (arm != nullptr) {
        m_arm.setPosition((*arm)[0], (*arm)[1], (*arm)[2]);
    } else {
        m_arm.setPosition(0.0f, 0.0f, -500.0f);
    }
    rig[0] = -0.12f;  // 0x00725830
    rig[1] = -1.57f;  // 0x0072582C
    rig[2] = 0.0f;
}

// The map mode (4): the stick turns the rig after the input delay, else the rig turns by itself (paused for five
// seconds after the stick); the pivot at the look-at point takes the rig's angles and the camera goes to the arm.
void View3d::moveRig(float dt) {
    if (inputDelay == 0.0f) {
        m_speed[0] = stick[0] * dt * 0.2f + m_speed[0];
        m_speed[1] = stick[1] * dt * 0.2f + m_speed[1];
    } else {
        inputDelay = clampValue(inputDelay - dt, 0.0f, kNoLimit);
    }
    const bool moved = stick[0] != 0.0f || stick[1] != 0.0f;
    if (moved && inputDelay == 0.0f) {
        m_pause = 5.0f;
    } else if (m_pause == 0.0f) {
        m_speed[0] = dt * autoSpeed;
    } else {
        m_pause -= dt;
        if (m_pause < 0.0f) m_pause = 0.0f;
    }
    m_speed[0] = clampValue(m_speed[0], -0.2f, 0.2f);
    m_speed[1] = clampValue(m_speed[1], -0.2f, 0.2f);
    rig[1] = wrapAngle(m_speed[0] + rig[1]);
    rig[0] = wrapAngle(rig[0] - m_speed[1]);
    m_speed[0] *= 0.85f;
    m_speed[1] *= 0.85f;
    if (pitchLimits[0] != kNoLimit && rig[0] < pitchLimits[0]) rig[0] = pitchLimits[0];
    if (pitchLimits[1] != kNoLimit && !(rig[0] <= pitchLimits[1])) rig[0] = pitchLimits[1];
    if (yawLimits[0] != kNoLimit && rig[1] < yawLimits[0]) rig[1] = yawLimits[0];
    if (yawLimits[1] != kNoLimit && !(rig[1] <= yawLimits[1])) rig[1] = yawLimits[1];

    m_pivot.resetMatrices();
    m_pivot.setPosition(m_lookAt[0], m_lookAt[1], m_lookAt[2]);
    oj::rotateY(m_pivot.local, rig[1]);
    oj::rotateX(m_pivot.local, rig[0]);
    m_pivot.invalidate();
    const oj::Mat34& w = m_arm.world();
    m_target[0] = w[3];
    m_target[1] = w[7];
    m_target[2] = w[11];
    lookAt(m_lookAt, false);
}

void View3d::moveCamera(float dt) {
    const oj::Vec4 at{m_camera.local[3], m_camera.local[7], m_camera.local[11], 0.0f};
    switch (m_mode) {
    case MoveMode::ModelManual:
        // MODEL_MANUAL: the stick turns the model.
        if (m_model && m_model->root != nullptr) {
            oj::rotateY(m_model->root->local, stick[0] * 0.1f);
            oj::rotateX(m_model->root->local, stick[1] * 0.1f);
            m_model->root->invalidate();
        }
        break;
    case MoveMode::CameraManual: {
        // CAMERA_MANUAL: the stick turns the camera, the pad moves it by 1250 (0x00725828) along z or x.
        oj::rotateY(m_camera.local, stick[0] * -0.1f);
        oj::rotateX(m_camera.local, stick[1] * 0.1f);
        float x = at[0], z = at[2];
        switch (pad) {
        case input::kUp: z = 1250.0f + z; break;
        case input::kDown: z = z - 1250.0f; break;
        case input::kLeft: x = x - 1250.0f; break;
        case input::kRight: x = 1250.0f + x; break;
        default: break;
        }
        m_camera.setPosition(x, at[1], z);
        break;
    }
    case MoveMode::Map:
    case MoveMode::CameraAuto: {
        if (m_mode == MoveMode::Map) moveRig(dt);
        // CAMERA_AUTO: the camera moves 10 % of the way to its position and turns 10 % of the way to its angles
        // (all of it on the first frame).
        m_camera.resetMatrices();
        const oj::Vec4 next = oj::lerp(at, m_target, 0.1f);
        m_camera.setPosition(next[0], next[1], next[2]);
        float d[3] = {m_targetAngles[0] - m_angles[0], m_targetAngles[1] - m_angles[1],
                      m_targetAngles[2] - m_angles[2]};
        for (int i : {1, 0}) {
            if (d[i] > kPi)
                d[i] -= kTwoPi;
            else if (d[i] < -kPi)
                d[i] += kTwoPi;
        }
        const float f = m_snap ? 1.0f : 0.1f;
        for (int i = 0; i < 3; ++i) m_angles[i] = d[i] * f + m_angles[i];
        turnCamera();
        break;
    }
        default: {
            // MODEL_AUTO: the camera moves 2 % of the way to its position every frame and turns to its angles.
            m_camera.resetMatrices();
            const oj::Vec4 next = oj::lerp(at, m_target, 0.02f);
            m_camera.setPosition(next[0], next[1], next[2]);
            turnCamera();
            break;
        }
    }
    m_fade = (m_fadeTarget - m_fade) * dt * 2.5f + m_fade;
    if (m_fade < 0.0f) m_fade = 0.0f;
    else if (!(m_fade <= 4.0f)) m_fade = 4.0f;
    m_world.fade = m_fade;
    m_zoom = (m_zoomTarget - m_zoom) * 0.02f + m_zoom;
    m_zoom = clampValue(m_zoom, 0.4f, 1.0f);
    m_zoomTarget = clampValue(m_zoomTarget, 0.4f, 1.0f);
    m_camera.setFov(0.35f * m_zoom * kFovPi, 0.0f, m_engine.wideScreen());
}

// The angles brought into (-pi, pi], then the camera turns by Y, X and Z.
void View3d::turnCamera() {
    for (int i : {1, 0}) {
        if (m_angles[i] > kPi) m_angles[i] -= kTwoPi;
        else if (m_angles[i] < -kPi) m_angles[i] += kTwoPi;
    }
    oj::rotateY(m_camera.local, m_angles[1]);
    oj::rotateX(m_camera.local, m_angles[0]);
    oj::rotateZ(m_camera.local, m_angles[2]);
    m_camera.invalidate();
}

void View3d::drawView(float x0, float y0, float x1, float y1) {
    render::IRenderDevice& device = m_engine.device();
    render::Renderer3d& r = m_engine.renderer();
    m_camera.update();

    render::Viewport viewport;
    viewport.x = static_cast<uint32_t>(std::fmax(x0, 0.0f));
    viewport.y = static_cast<uint32_t>(std::fmax(y0, 0.0f));
    viewport.width = static_cast<uint32_t>(std::fmax(x1 - x0, 1.0f));
    viewport.height = static_cast<uint32_t>(std::fmax(y1 - y0, 1.0f));
    device.setViewport(viewport);
    // 0x00516880: the element's view (flags with bit 0) clears the depth and the stencil only ([0x00719360]: 2, and
    // 4 with a stencil); the colour 0x88888888 it keeps (0x005B3978) is not used.
    device.clear(ClearFlags::ZBuffer | ClearFlags::Stencil, 0x88888888, 1.0f, 0);
    if (m_round) roundMask(viewport);

    setViewConstants(r, m_camera);
    device.setRenderState(RenderState::FogEnable, 1);
    device.setRenderState(RenderState::ZEnable, 1);

    // The world (0x0054C5C0): the time, the lights, then the nodes.
    if (m_world.atmosphere) {
        render::applyAtmosphere(r, *m_world.atmosphere, m_camera);
    } else {
        r.setLight1Colour(m_world.light1Colour);
        r.setLight2Colour(m_world.light2Colour);
        r.light3Colour = m_world.light3Colour;
        r.writeLightColours();
        r.setLightDirection(m_world.direction1);
        r.setLight2Direction(m_world.direction2);
        r.setLight3Direction(m_world.direction3);
        r.setAmbient(m_world.ambient);
        r.fogMin = 0.0f;
        r.fogMax = 0.0f;
        r.fogNear = 3.40282347e+38f;
        r.fogFar = 3.40282347e+38f;
        r.applyFogDensity();
    }
    r.time = m_world.time;
    r.endMaterial();
    r.dynamicEnvironment = m_world.environment;
    r.environment = m_world.environment;
    if (m_world.sky) m_world.sky->draw(m_camera);
    // The world's roots (0x00550500, as the race's): each tree with the parts of opaque classes (mode 1), then again
    // with the others (mode 2), so a car's glass goes over what is behind it (a spoiler). Stock problem: the sponsors'
    // decals (Coloured Spec ABlend) are of the late classes too and drawn in the parts' order with the glass, so the
    // far side's decals show through the rear window.
    const auto drawTree = [&](Model& model) {
        for (uint32_t mode = 1; mode <= 2; ++mode) {
            for (auto& node : model.meshNodes) node->draw(r, m_camera, mode);
        }
    };
    if (m_model) drawTree(*m_model);
    for (const std::unique_ptr<Model>& model : m_models) drawTree(*model);
    for (const auto& draw : worldDraws) {
        draw(r, m_camera, 1);
        draw(r, m_camera, 2);
    }
    r.endMaterial();
    // The object constants of no object (0x0054D8D0), the world's lens flares (0x0054FC60).
    identityObject(r, m_camera);
    m_flares.draw(m_camera);
    // 0x00516D34: a round view's stencil off first.
    if (m_round) device.setRenderState(RenderState::StencilEnable, 0);
    device.setRenderState(RenderState::ZEnable, 0);
    device.setRenderState(RenderState::FogEnable, 0);
    render::Viewport full;
    full.width = device.width();
    full.height = device.height();
    device.setViewport(full);
}

// 0x00516450 (the view's begin 0x00516880, with its flag 0x10): no culling, no colour written, the stencil set to 1
// where drawn (ALWAYS, REPLACE, reference 1); for each corner of the view a fan from 2 pixels past the corner along the
// quarter of the ellipse within the view (nine points, pi / 16 apart from the side; 0xFF00FF00 and 0xFFFF0000, unseen);
// then culling back ([0x0071935C] 3), red, green and blue written, and the view drawn where the stencil is 0 (EQUAL,
// KEEP, reference 0): round. The stock draws the fans with its 2D material (pixels of the viewport); here as
// transformed vertices.
void View3d::roundMask(const render::Viewport& viewport) {
    render::IRenderDevice& device = m_engine.device();
    device.setRenderState(RenderState::CullMode, Cull::None);
    device.setRenderState(RenderState::ColourWriteEnable, ColourWrite::None);
    device.setRenderState(RenderState::StencilEnable, 1);
    device.setRenderState(RenderState::StencilFunc, CompareFunc::Always);
    device.setRenderState(RenderState::StencilPass, StencilOp::Replace);
    device.setRenderState(RenderState::StencilRef, 1);
    device.setVertexShader(0);
    device.setPixelShader(0);
    device.setTexture(0, 0);
    device.setFvf(Fvf::XyzRhw | Fvf::Diffuse);
    struct Vertex {
        float x, y, z, rhw;
        uint32_t colour;
    };
    const float w = static_cast<float>(viewport.width);
    const float h = static_cast<float>(viewport.height);
    const float left = static_cast<float>(viewport.x);
    const float top = static_cast<float>(viewport.y);
    for (uint32_t corner = 0; corner < 4; ++corner) {
        const float sx = corner == 1 || corner == 3 ? 1.0f : 0.0f;
        const float sy = corner == 2 || corner == 3 ? 1.0f : 0.0f;
        Vertex v[10];
        v[0] = {left + (sx == 0.0f ? w * sx - 2.0f : w * sx + 2.0f), top + (sy == 0.0f ? h * sy - 2.0f : h * sy + 2.0f),
                0.0f, 1.0f, 0xFF00FF00u};
        for (uint32_t k = 0; k <= 8; ++k) {
            const float angle = static_cast<float>(k) * 0.19634375f;  // [0x006FFCF4]
            v[k + 1] = {left + w * (std::cos(angle) * (sx - 0.5f) + 0.5f),
                        top + h * (std::sin(angle) * (sy - 0.5f) + 0.5f), 0.0f, 1.0f, 0xFFFF0000u};
        }
        device.drawUp(6, 8, v, sizeof(Vertex));  // TRIANGLEFAN
    }
    device.setRenderState(RenderState::CullMode, Cull::CounterClockwise);  // [0x0071935C]
    device.setRenderState(RenderState::ColourWriteEnable, ColourWrite::Rgb);
    device.setRenderState(RenderState::StencilEnable, 1);
    device.setRenderState(RenderState::StencilFunc, CompareFunc::Equal);
    device.setRenderState(RenderState::StencilPass, StencilOp::Keep);
    device.setRenderState(RenderState::StencilRef, 0);
}

void View3d::draw(float x0, float y0, float x1, float y1, float seconds, float dt) {
    // 0x005B5100 (0x005B3660 for each listed node): the turns by the frame time; the parent's X and Z turns
    // (0x00534C20, 0x00534CD0) no listing here asks for.
    for (Listed& e : m_listed) {
        if (e.node == nullptr) continue;
        if (e.parent) {
            oj::rotateYParent(e.node->local, dt * e.rates[1]);
        } else {
            oj::rotateY(e.node->local, dt * e.rates[1]);
            oj::rotateX(e.node->local, dt * e.rates[0]);
            oj::rotateZ(e.node->local, dt * e.rates[2]);
        }
        e.node->invalidate();
    }
    m_world.time = seconds;
    moveCamera(dt);
    drawView(x0, y0, x1, y1);
    m_snap = false;
}

}  // namespace oj::scene
