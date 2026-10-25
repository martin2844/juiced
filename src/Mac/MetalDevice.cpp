#include <mutex>
#include <thread>
#include <chrono>
#include "Mac/MetalDevice.h"
#include "Core/Base/Log.h"
#include "Core/Formats/Ddt.h"
#include "Mac/Shader.h"
#include "Render/HlslCompiler.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <map>
#include <mojoshader.h>
#include <stdexcept>
namespace oj::render {
namespace {
void check(bool ok) {
    if (!ok)
        throw std::runtime_error(SDL_GetError());
}
SDL_GPUBlendFactor blend(uint32_t n) {
    static SDL_GPUBlendFactor v[] = {SDL_GPU_BLENDFACTOR_ZERO,
                                     SDL_GPU_BLENDFACTOR_ZERO,
                                     SDL_GPU_BLENDFACTOR_ONE,
                                     SDL_GPU_BLENDFACTOR_SRC_COLOR,
                                     SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR,
                                     SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                                     SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                     SDL_GPU_BLENDFACTOR_DST_ALPHA,
                                     SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA,
                                     SDL_GPU_BLENDFACTOR_DST_COLOR,
                                     SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR,
                                     SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE};
    return n < std::size(v) ? v[n] : SDL_GPU_BLENDFACTOR_ONE;
}
SDL_GPUPrimitiveType primitive(uint32_t v) {
    switch (v) {
    case 1:
        return SDL_GPU_PRIMITIVETYPE_POINTLIST;
    case 2:
        return SDL_GPU_PRIMITIVETYPE_LINELIST;
    case 3:
        return SDL_GPU_PRIMITIVETYPE_LINESTRIP;
    case 5:
        return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
    default:
        return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    }
}
uint32_t vertexCount(uint32_t type, uint32_t count) {
    switch (type) {
    case 1:
        return count;
    case 2:
        return count * 2;
    case 3:
        return count + 1;
    case 5:
    case 6:
        return count + 2;
    default:
        return count * 3;
    }
}
SDL_GPUVertexElementFormat element(uint8_t v) {
    switch (v) {
    case 0:
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
    case 1:
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    case 2:
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    case 3:
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    case 4:
        return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    case 5:
        return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4;
    case 6:
        return SDL_GPU_VERTEXELEMENTFORMAT_SHORT2;
    case 7:
        return SDL_GPU_VERTEXELEMENTFORMAT_SHORT4;
    case 8:
        return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    case 9:
        return SDL_GPU_VERTEXELEMENTFORMAT_SHORT2_NORM;
    case 10:
        return SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM;
    case 11:
        return SDL_GPU_VERTEXELEMENTFORMAT_USHORT2_NORM;
    case 12:
        return SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM;
    case 15:
        return SDL_GPU_VERTEXELEMENTFORMAT_HALF2;
    case 16:
        return SDL_GPU_VERTEXELEMENTFORMAT_HALF4;
    default:
        throw std::runtime_error("Unsupported vertex declaration element " + std::to_string(v));
    }
}
const char *fixedSource = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct V {float4 pos [[attribute(0)]];float2 uv [[attribute(1)]];float4 color [[attribute(2)]];};
struct O {float4 pos [[position]];float2 uv [[user(texcoord0)]];float4 color [[user(color0)]];};
struct U {float4x4 mvp;};
vertex O fixed_vs(V v [[stage_in]],constant U& u [[buffer(0)]]){O o;o.pos=u.mvp*v.pos;o.uv=v.uv;o.color=v.color.bgra;return o;}
fragment float4 fixed_ps(O v [[stage_in]],texture2d<float> t [[texture(0)]],sampler s [[sampler(0)]],constant float4& alpha [[buffer(0)]]){float4 c=t.sample(s,v.uv)*v.color;if(alpha.x>0){float a=c.a,r=alpha.y;int f=int(alpha.z);bool ok=f==1?false:f==2?a<r:f==3?a==r:f==4?a<=r:f==5?a>r:f==6?a!=r:f==7?a>=r:true;if(!ok)discard_fragment();}return c;}
)MSL";
struct FixedVertex {
    float x, y, z, w, u, v;
    uint32_t color;
};
std::array<float, 16> identity() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
std::array<float, 16> multiply(const std::array<float, 16> &a, const std::array<float, 16> &b) {
    std::array<float, 16> c{};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                c[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
    return c;
}
} // namespace
struct MetalDevice::Impl {
    // Asset jobs share resource maps with rendering, but SDL command buffers stay on their creating thread.
    std::recursive_mutex deviceLock;
    const std::thread::id deviceThread = std::this_thread::get_id();
    bool inFrame = false;
    bool onDeviceThread() const { return std::this_thread::get_id() == deviceThread; }
    SDL_Window *window = nullptr;
    SDL_GPUDevice *gpu = nullptr;
    oj::mac::Registers vertexRegisters, pixelRegisters;
    SDL_GPUCommandBuffer *cb = nullptr;
    SDL_GPURenderPass *pass = nullptr;
    SDL_GPUTexture *swap = nullptr;
    SDL_GPUTexture *backColor = nullptr;
    SDL_GPUTexture *backDepth = nullptr;
    SDL_GPUTextureFormat swapFormat = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    uint32_t w = 800, h = 600, depthW = 0, depthH = 0;
    uint32_t materialClass = UINT32_MAX;
    struct Texture {
        SDL_GPUTexture *gpu = nullptr;
        SDL_GPUTexture *depth = nullptr;
        uint32_t w = 0, h = 0, layers = 1;
        SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        std::vector<uint8_t> pixels;
    };
    struct Buffer {
        SDL_GPUBuffer *gpu = nullptr;
        std::vector<uint8_t> bytes;
    };
    std::map<uint32_t, Texture> textures;
    std::map<uint32_t, Buffer> buffers;
    std::map<uint32_t, std::unique_ptr<oj::mac::Shader>> shaders;
    std::map<uint32_t, std::vector<VertexElement>> declarations;
    std::map<std::string, SDL_GPUGraphicsPipeline *> pipelines;
    std::map<std::string, SDL_GPUSampler *> samplers;
    uint32_t next = 1, target = 0, face = 0, vs = 0, ps = 0, decl = 0, stream = 0, indices = 0, stride = 0, fvf = 0,
             white = 0;
    std::array<uint32_t, 256> states{};
    std::array<std::array<uint32_t, 33>, 8> stages{};
    std::map<unsigned, SDL_GPUShader *> fixedFragments;
    std::array<std::array<uint32_t, 16>, 16> sampling{};
    std::array<uint32_t, 16> bound{};
    std::map<uint32_t, std::array<float, 16>> transforms;
    SDL_GPUShader *fixedVS = nullptr;
    SDL_GPUShader *fixedPS = nullptr;
    Viewport viewport{};
    ClipRect clip{};
    bool clipped = false;
    uint64_t draws = 0;
    void endPass() {
        if (pass) {
            SDL_EndGPURenderPass(pass);
            pass = nullptr;
        }
    }
    SDL_GPUCommandBuffer *commands() {
        if (!cb)
            cb = SDL_AcquireGPUCommandBuffer(gpu);
        check(cb);
        return cb;
    }
    void flush() {
        endPass();
        if (cb) {
            check(SDL_SubmitGPUCommandBuffer(cb));
            cb = nullptr;
        }
    }
    SDL_GPUTexture *color() {
        return target ? textures.at(target).gpu : backColor;
    }
    SDL_GPUTexture *depth() {
        return target ? textures.at(target).depth : backDepth;
    }
    uint32_t width() {
        return target ? textures.at(target).w : w;
    }
    uint32_t height() {
        return target ? textures.at(target).h : h;
    }
    SDL_GPUTexture *makeDepth(uint32_t width, uint32_t height) {
        SDL_GPUTextureCreateInfo ci{};
        ci.type = SDL_GPU_TEXTURETYPE_2D;
        ci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
        ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        ci.width = width;
        ci.height = height;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        auto t = SDL_CreateGPUTexture(gpu, &ci);
        check(t);
        return t;
    }
    void beginPass(uint32_t flags = 0, SDL_FColor clear = {}, float z = 1, uint8_t stencil = 0) {
        if (pass && flags == 0)
            return;
        endPass();
        if (!color())
            return;
        SDL_GPUColorTargetInfo c{};
        c.texture = color();
        c.layer_or_depth_plane = face;
        c.load_op = (flags & 1) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        c.store_op = SDL_GPU_STOREOP_STORE;
        c.clear_color = clear;
        SDL_GPUDepthStencilTargetInfo d{};
        d.texture = depth();
        d.load_op = (flags & 2) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        d.store_op = SDL_GPU_STOREOP_STORE;
        d.stencil_load_op = (flags & 4) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        d.stencil_store_op = SDL_GPU_STOREOP_STORE;
        d.clear_depth = z;
        d.clear_stencil = stencil;
        pass = SDL_BeginGPURenderPass(commands(), &c, 1, d.texture ? &d : nullptr);
        check(pass);
    }
    void uploadTexture(Texture &t, const uint8_t *bytes, size_t size, uint32_t level, uint32_t layer, uint32_t tw,
                       uint32_t th) {
        if (onDeviceThread()) endPass();
        SDL_GPUTransferBufferCreateInfo ti{};
        ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        ti.size = (uint32_t)size;
        auto transfer = SDL_CreateGPUTransferBuffer(gpu, &ti);
        check(transfer);
        void *map = SDL_MapGPUTransferBuffer(gpu, transfer, false);
        check(map);
        memcpy(map, bytes, size);
        SDL_UnmapGPUTransferBuffer(gpu, transfer);
        auto upload = onDeviceThread() ? commands() : SDL_AcquireGPUCommandBuffer(gpu);
        check(upload);
        auto cp = SDL_BeginGPUCopyPass(upload);
        SDL_GPUTextureTransferInfo src{};
        src.transfer_buffer = transfer;
        SDL_GPUTextureRegion dst{};
        dst.texture = t.gpu;
        dst.mip_level = level;
        dst.layer = layer;
        dst.w = tw;
        dst.h = th;
        dst.d = 1;
        SDL_UploadToGPUTexture(cp, &src, &dst, false);
        SDL_EndGPUCopyPass(cp);
        if (!onDeviceThread()) check(SDL_SubmitGPUCommandBuffer(upload));
        SDL_ReleaseGPUTransferBuffer(gpu, transfer);
    }
    uint32_t makeBuffer(const void *bytes, uint32_t size, bool index) {
        Buffer b;
        b.bytes.assign((const uint8_t *)bytes, (const uint8_t *)bytes + size);
        SDL_GPUBufferCreateInfo ci{};
        ci.usage = index ? SDL_GPU_BUFFERUSAGE_INDEX : SDL_GPU_BUFFERUSAGE_VERTEX;
        ci.size = std::max(size, 1u);
        b.gpu = SDL_CreateGPUBuffer(gpu, &ci);
        check(b.gpu);
        uploadBuffer(b, bytes, size);
        uint32_t id = next++;
        buffers.emplace(id, std::move(b));
        return id;
    }
    void uploadBuffer(Buffer &b, const void *bytes, uint32_t size) {
        if (!size)
            return;
        if (onDeviceThread()) endPass();
        SDL_GPUTransferBufferCreateInfo ci{};
        ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        ci.size = size;
        auto t = SDL_CreateGPUTransferBuffer(gpu, &ci);
        check(t);
        auto m = SDL_MapGPUTransferBuffer(gpu, t, false);
        check(m);
        memcpy(m, bytes, size);
        SDL_UnmapGPUTransferBuffer(gpu, t);
        auto upload = onDeviceThread() ? commands() : SDL_AcquireGPUCommandBuffer(gpu);
        check(upload);
        auto cp = SDL_BeginGPUCopyPass(upload);
        SDL_GPUTransferBufferLocation src{t, 0};
        SDL_GPUBufferRegion dst{b.gpu, 0, size};
        SDL_UploadToGPUBuffer(cp, &src, &dst, true);
        SDL_EndGPUCopyPass(cp);
        if (!onDeviceThread()) check(SDL_SubmitGPUCommandBuffer(upload));
        SDL_ReleaseGPUTransferBuffer(gpu, t);
    }
    SDL_GPUSampler *sampler(uint32_t slot) {
        auto &s = sampling[slot];
        std::string key((char *)s.data(), sizeof s);
        if (auto it = samplers.find(key); it != samplers.end())
            return it->second;
        SDL_GPUSamplerCreateInfo ci{};
        ci.min_filter = s[6] == 1 ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
        ci.mag_filter = s[5] == 1 ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
        ci.mipmap_mode = s[7] == 1 ? SDL_GPU_SAMPLERMIPMAPMODE_NEAREST : SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        auto addr = [](uint32_t a) {
            return a == 1   ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT
                   : a == 2 ? SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT
                            : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        };
        ci.address_mode_u = addr(s[1]);
        ci.address_mode_v = addr(s[2]);
        ci.address_mode_w = addr(s[3]);
        ci.min_lod = 0;
        ci.max_lod = s[7] ? 1000 : 0;
        ci.mip_lod_bias = std::bit_cast<float>(s[8]);
        auto result = SDL_CreateGPUSampler(gpu, &ci);
        check(result);
        samplers[key] = result;
        return result;
    }
    SDL_GPUShader *fixedFragment() {
        auto source = std::string(shaders.at(vs)->parsed->output);
        bool color = source.find("[[user(color0)]]") != std::string::npos,
             tex = source.find("[[user(texcoord0)]]") != std::string::npos;
        unsigned mask = (color ? 1 : 0) | (tex ? 2 : 0);
        if (fixedFragments.contains(mask))
            return fixedFragments.at(mask);
        std::string msl = "#include <metal_stdlib>\nusing namespace metal;\nstruct V {float4 position [[position]];";
        if (color)
            msl += "float4 color [[user(color0)]];";
        if (tex)
            msl += "float4 uv [[user(texcoord0)]];";
        msl += "};\nstruct U {float4 factor;uint4 ops;uint4 args;};\n";
        msl +=
            R"(float4 arg(uint a,float4 d,float4 t,float4 f){float4 v=(a&15)==2?t:(a&15)==3?f:d;if(a&32)v=v.aaaa;return (a&16)?1-v:v;}
float4 op(uint o,float4 a,float4 b){switch(o){case 2:return a;case 3:return b;case 4:return a*b;case 5:return 2*a*b;case 6:return 4*a*b;case 7:return a+b;case 8:return a+b-.5;case 9:return 2*(a+b-.5);case 10:return a-b;case 11:return a+b-a*b;default:return a*b;}}
fragment float4 ff_ps(V v [[stage_in]],texture2d<float> tex [[texture(0)]],sampler s [[sampler(0)]],constant U& u [[buffer(0)]],constant float4& alpha [[buffer(1)]]){)";
        msl += "float4 d=" + std::string(color ? "v.color" : "float4(1)") +
               ";float2 uv=" + std::string(tex ? "v.uv.xy" : "float2(0)") + ";float4 t=tex.sample(s,uv);";
        msl += "if(u.ops.x==1)return d;float4 a=arg(u.args.x,d,t,u.factor),b=arg(u.args.y,d,t,u.factor);float4 "
               "c=op(u.ops.x,a,b);c.a=op(u.ops.y,arg(u.args.z,d,t,u.factor),arg(u.args.w,d,t,u.factor)).a;"
               "if(alpha.x>0){float a=c.a,r=alpha.y;int f=int(alpha.z);bool "
               "ok=f==1?false:f==2?a<r:f==3?a==r:f==4?a<=r:f==5?a>r:f==6?a!=r:f==7?a>=r:true;if(!ok)discard_fragment();"
               "}return saturate(c);}";
        SDL_GPUShaderCreateInfo ci{};
        ci.code = (const Uint8 *)msl.data();
        ci.code_size = msl.size();
        ci.entrypoint = "ff_ps";
        ci.format = SDL_GPU_SHADERFORMAT_MSL;
        ci.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        ci.num_samplers = 1;
        ci.num_uniform_buffers = 2;
        auto f = SDL_CreateGPUShader(gpu, &ci);
        check(f);
        fixedFragments[mask] = f;
        return f;
    }
    void fixedUniforms() {
        struct U {
            float factor[4];
            uint32_t ops[4], args[4];
        } u{};
        uint32_t f = states[60];
        u.factor[0] = ((f >> 16) & 255) / 255.f;
        u.factor[1] = ((f >> 8) & 255) / 255.f;
        u.factor[2] = (f & 255) / 255.f;
        u.factor[3] = (f >> 24) / 255.f;
        u.ops[0] = stages[0][1];
        u.ops[1] = stages[0][4];
        u.args[0] = stages[0][2];
        u.args[1] = stages[0][3];
        u.args[2] = stages[0][5];
        u.args[3] = stages[0][6];
        SDL_PushGPUFragmentUniformData(cb, 0, &u, sizeof u);
    }
    SDL_GPUGraphicsPipeline *pipeline(uint32_t type, bool fixed) {
        SDL_GPUShader *v = fixedVS, *f = fixedPS;
        std::vector<SDL_GPUVertexAttribute> attrs;
        SDL_GPUVertexBufferDescription vb{};
        vb.slot = 0;
        vb.pitch = fixed ? sizeof(FixedVertex) : stride;
        vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        if (!fixed) {
            if (!shaders.contains(vs) || (ps && !shaders.contains(ps)))
                throw std::runtime_error("Missing shader: vs=" + std::to_string(vs) + " ps=" + std::to_string(ps) +
                                         " decl=" + std::to_string(decl) + " fvf=" + std::to_string(fvf));
            v = shaders.at(vs)->gpu;
            uint32_t cubeMask = 0;
            for (unsigned i = 0; i < bound.size(); ++i) {
                auto texture = textures.find(bound[i]);
                if (texture != textures.end() && texture->second.layers == 6)
                    cubeMask |= 1u << i;
            }
            f = ps ? shaders.at(ps)->forCubeSamplers(cubeMask) : fixedFragment();
            for (auto &e : declarations.at(decl)) {
                int location = shaders.at(vs)->attribute((MOJOSHADER_usage)e.usage, e.usageIndex);
                if (location >= 0)
                    attrs.push_back({(uint32_t)location, 0, element(e.type), e.offset});
            }
        } else {
            attrs = {{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0},
                     {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 16},
                     {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, 24}};
        }
        SDL_GPUGraphicsPipelineCreateInfo ci{};
        ci.vertex_shader = v;
        ci.fragment_shader = f;
        ci.vertex_input_state = {&vb, 1, attrs.data(), (uint32_t)attrs.size()};
        ci.primitive_type = primitive(type);
        ci.rasterizer_state.fill_mode = states[8] == 2 ? SDL_GPU_FILLMODE_LINE : SDL_GPU_FILLMODE_FILL;
        ci.rasterizer_state.cull_mode = states[22] == 1 ? SDL_GPU_CULLMODE_NONE : SDL_GPU_CULLMODE_BACK;
        ci.rasterizer_state.front_face =
            states[22] == 2 ? SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE : SDL_GPU_FRONTFACE_CLOCKWISE;
        ci.depth_stencil_state.enable_depth_test = states[7];
        ci.depth_stencil_state.enable_depth_write = states[14];
        ci.depth_stencil_state.compare_op = (SDL_GPUCompareOp)std::clamp(states[23], 1u, 8u);
        ci.depth_stencil_state.enable_stencil_test = states[52];
        ci.depth_stencil_state.compare_mask = states[58];
        ci.depth_stencil_state.write_mask = states[59];
        auto &st = ci.depth_stencil_state.front_stencil_state;
        st.fail_op = (SDL_GPUStencilOp)std::clamp(states[53], 1u, 8u);
        st.depth_fail_op = (SDL_GPUStencilOp)std::clamp(states[54], 1u, 8u);
        st.pass_op = (SDL_GPUStencilOp)std::clamp(states[55], 1u, 8u);
        st.compare_op = (SDL_GPUCompareOp)std::clamp(states[56], 1u, 8u);
        ci.depth_stencil_state.back_stencil_state = st;
        SDL_GPUColorTargetDescription ct{};
        ct.format = target ? textures.at(target).format : swapFormat;
        auto &b = ct.blend_state;
        b.enable_blend = states[27];
        b.src_color_blendfactor = blend(states[19]);
        b.dst_color_blendfactor = blend(states[20]);
        b.color_blend_op = SDL_GPU_BLENDOP_ADD;
        b.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        b.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        b.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        b.enable_color_write_mask = true;
        b.color_write_mask = states[168];
        ci.target_info.color_target_descriptions = &ct;
        ci.target_info.num_color_targets = 1;
        ci.target_info.has_depth_stencil_target = true;
        ci.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
        std::string key;
        auto append = [&](const auto &a) { key.append((const char *)&a, sizeof a); };
        append(v);
        append(f);
        append(type);
        append(vb);
        append(ci.rasterizer_state);
        append(ci.depth_stencil_state);
        append(ct);
        for (auto &a : attrs)
            append(a);
        if (auto it = pipelines.find(key); it != pipelines.end())
            return it->second;
        auto pipe = SDL_CreateGPUGraphicsPipeline(gpu, &ci);
        check(pipe);
        pipelines[key] = pipe;
        return pipe;
    }
    void prepare(uint32_t type, bool fixed) {
        auto pipe = pipeline(type, fixed);
        beginPass();
        if (!pass)
            return;
        SDL_BindGPUGraphicsPipeline(pass, pipe);
        SDL_GPUViewport vp{(float)viewport.x,      (float)viewport.y, (float)viewport.width,
                           (float)viewport.height, viewport.minZ,     viewport.maxZ};
        SDL_SetGPUViewport(pass, &vp);
        SDL_Rect rect{0, 0, (int)width(), (int)height()};
        if (clipped) {
            rect.x = std::max(0, (int)clip.x0);
            rect.y = std::max(0, (int)clip.y0);
            rect.w = std::max(0, (int)clip.x1 - rect.x);
            rect.h = std::max(0, (int)clip.y1 - rect.y);
        }
        SDL_SetGPUScissor(pass, &rect);
        SDL_SetGPUStencilReference(pass, (Uint8)states[57]);
        unsigned count = (fixed || !ps) ? 1 : shaders.at(ps)->slots;
        std::vector<SDL_GPUTextureSamplerBinding> binds;
        for (unsigned i = 0; i < count; i++) {
            auto id = bound[i];
            if (!textures.contains(id))
                id = white;
            binds.push_back({textures.at(id).gpu, sampler(i)});
        }
        if (count)
            SDL_BindGPUFragmentSamplers(pass, 0, binds.data(), count);
        if (!fixed) {
            unsigned vertexSlots = shaders.at(vs)->slots;
            std::vector<SDL_GPUTextureSamplerBinding> vertexBinds(vertexSlots, {textures.at(white).gpu, sampler(0)});
            if (vertexSlots)
                SDL_BindGPUVertexSamplers(pass, 0, vertexBinds.data(), vertexSlots);
            // Paint needs more fill light on the native renderer. Keep the scene,
            // decals' source colours, and specular power unchanged.
            shaders.at(vs)->uniforms(cb, vertexRegisters, materialClass == 0 ? 1.65f : 1.0f);
            if (ps) {
                shaders.at(ps)->uniforms(cb, pixelRegisters);
                // Reduce reflected RGB on car glass, retaining the reflection alpha
                // used by the original shader for opacity and Fresnel calculations.
                const float material[4] = {materialClass == 49 ? 0.35f : 1.0f, 0, 0, 0};
                SDL_PushGPUFragmentUniformData(cb, 2, material, sizeof material);
            } else
                fixedUniforms();
            float alpha[4] = {(float)states[15], states[24] / 255.f, (float)states[25], 0};
            SDL_PushGPUFragmentUniformData(cb, 1, alpha, sizeof alpha);
        }
    }
    void fixedDraw(const FixedVertex *data, uint32_t count, uint32_t type, const std::array<float, 16> &mvp,
                   bool overlay = true) {
        if (!count)
            return;
        uint32_t b = makeBuffer(data, count * sizeof(FixedVertex), false);
        auto oldZ = states[7], oldWrite = states[14], oldCull = states[22];
        if (overlay) {
            states[7] = 0;
            states[14] = 0;
            states[22] = 1;
        }
        prepare(type, true);
        states[7] = oldZ;
        states[14] = oldWrite;
        states[22] = oldCull;
        float alpha[4] = {(float)states[15], states[24] / 255.f, (float)states[25], 0};
        SDL_PushGPUFragmentUniformData(cb, 0, alpha, sizeof alpha);
        SDL_PushGPUVertexUniformData(cb, 0, mvp.data(), 64);
        SDL_GPUBufferBinding binding{buffers.at(b).gpu, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
        SDL_DrawGPUPrimitives(pass, count, 1, 0, 0);
        SDL_ReleaseGPUBuffer(gpu, buffers.at(b).gpu);
        buffers.erase(b);
        ++draws;
    }
};
MetalDevice::MetalDevice(const DeviceDesc &desc) : p(std::make_unique<Impl>()) {
    p->window = reinterpret_cast<SDL_Window *>(desc.window);
    p->gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, true, "metal");
    check(p->gpu);
    check(SDL_ClaimWindowForGPUDevice(p->gpu, p->window));
    p->swapFormat = SDL_GetGPUSwapchainTextureFormat(p->gpu, p->window);
    SDL_GPUShaderCreateInfo ci{};
    ci.code = (const Uint8 *)fixedSource;
    ci.code_size = strlen(fixedSource);
    ci.format = SDL_GPU_SHADERFORMAT_MSL;
    ci.entrypoint = "fixed_vs";
    ci.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    ci.num_uniform_buffers = 1;
    p->fixedVS = SDL_CreateGPUShader(p->gpu, &ci);
    check(p->fixedVS);
    ci.entrypoint = "fixed_ps";
    ci.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    ci.num_uniform_buffers = 1;
    ci.num_samplers = 1;
    p->fixedPS = SDL_CreateGPUShader(p->gpu, &ci);
    check(p->fixedPS);
    p->stages[0][1] = 4;
    p->stages[0][2] = 2;
    p->stages[0][3] = 0;
    p->stages[0][4] = 2;
    p->stages[0][5] = 2;
    p->stages[0][6] = 0;
    p->states[60] = 0xffffffff;
    p->states[7] = 1;
    p->states[14] = 1;
    p->states[19] = 5;
    p->states[20] = 6;
    p->states[22] = 3;
    p->states[23] = 4;
    p->states[168] = 15;
    p->states[58] = p->states[59] = 255;
    for (auto &s : p->sampling) {
        s[1] = s[2] = s[3] = 1;
        s[5] = s[6] = s[7] = 2;
    }
    p->transforms[2] = p->transforms[3] = p->transforms[256] = identity();
    uint32_t white = 0xffffffff;
    p->white = createTexture({PixelFormat::A8R8G8B8, 1, 1, 1, (const uint8_t *)&white, 4});
    p->flush();
}
MetalDevice::~MetalDevice() {
    p->flush();
    SDL_WaitForGPUIdle(p->gpu);
    for (auto &[_, v] : p->pipelines)
        SDL_ReleaseGPUGraphicsPipeline(p->gpu, v);
    for (auto &[_, v] : p->samplers)
        SDL_ReleaseGPUSampler(p->gpu, v);
    for (auto &[_, v] : p->textures) {
        SDL_ReleaseGPUTexture(p->gpu, v.gpu);
        if (v.depth)
            SDL_ReleaseGPUTexture(p->gpu, v.depth);
    }
    for (auto &[_, v] : p->buffers)
        SDL_ReleaseGPUBuffer(p->gpu, v.gpu);
    p->shaders.clear();
    for (auto &[_, f] : p->fixedFragments)
        SDL_ReleaseGPUShader(p->gpu, f);
    SDL_ReleaseGPUShader(p->gpu, p->fixedVS);
    SDL_ReleaseGPUShader(p->gpu, p->fixedPS);
    if (p->backDepth)
        SDL_ReleaseGPUTexture(p->gpu, p->backDepth);
    if (p->backColor)
        SDL_ReleaseGPUTexture(p->gpu, p->backColor);
    SDL_ReleaseWindowFromGPUDevice(p->gpu, p->window);
    SDL_DestroyGPUDevice(p->gpu);
}
Backend MetalDevice::backend() const {
    return Backend::Metal;
}
std::string MetalDevice::adapterName() const {
    return "Apple Metal";
}
std::string MetalDevice::description() const {
    return "SDL GPU / Metal, native ARM64";
}
void MetalDevice::setMaterialClass(uint32_t id) {
    p->materialClass = id;
}
bool MetalDevice::beginFrame() {
    p->deviceLock.lock();
    p->target = 0;
    p->face = 0;
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(p->window, &w, &h);
    if (w <= 0 || h <= 0) {
        p->deviceLock.unlock();
        return false;
    }
    p->w = w;
    p->h = h;
    bool resized = p->depthW != p->w || p->depthH != p->h;
    if (resized) {
        p->endPass();
        if (p->backDepth)
            SDL_ReleaseGPUTexture(p->gpu, p->backDepth);
        if (p->backColor)
            SDL_ReleaseGPUTexture(p->gpu, p->backColor);
        p->backDepth = p->makeDepth(p->w, p->h);
        SDL_GPUTextureCreateInfo ci{};
        ci.type = SDL_GPU_TEXTURETYPE_2D;
        ci.format = p->swapFormat;
        ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        ci.width = p->w;
        ci.height = p->h;
        ci.layer_count_or_depth = 1;
        ci.num_levels = 1;
        p->backColor = SDL_CreateGPUTexture(p->gpu, &ci);
        check(p->backColor);
        p->depthW = p->w;
        p->depthH = p->h;
    }
    p->viewport = {0, 0, p->w, p->h, 0, 1};
    // Keep prior colour for menu fades; new backing textures start black.
    p->beginPass(resized ? 7 : 6, {0, 0, 0, 1});
    p->inFrame = true;
    return true;
}
void MetalDevice::endFrame() {
    p->endPass();
    uint32_t w = 0, h = 0;
    check(SDL_WaitAndAcquireGPUSwapchainTexture(p->commands(), p->window, &p->swap, &w, &h));
    if (p->swap) {
        SDL_GPUBlitInfo info{};
        info.source.texture = p->backColor;
        info.source.w = p->w;
        info.source.h = p->h;
        info.destination.texture = p->swap;
        info.destination.w = w;
        info.destination.h = h;
        info.load_op = SDL_GPU_LOADOP_DONT_CARE;
        info.filter = SDL_GPU_FILTER_LINEAR;
        SDL_BlitGPUTexture(p->cb, &info);
    }
    p->flush();
    p->swap = nullptr;
    p->inFrame = false;
    p->deviceLock.unlock();
}
void MetalDevice::letLoadingRun() {
    if (!p->onDeviceThread() || !p->inFrame) return;
    p->endPass();
    p->deviceLock.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    p->deviceLock.lock();
}
void MetalDevice::resize(uint32_t, uint32_t) {}
uint32_t MetalDevice::width() const {
    return p->w;
}
uint32_t MetalDevice::height() const {
    return p->h;
}
uint32_t MetalDevice::vertexShaderVersion() const {
    return 0xfffe0300;
}
uint32_t MetalDevice::pixelShaderVersion() const {
    return 0xffff0300;
}
AdapterInfo MetalDevice::adapterInfo() const {
    AdapterInfo a;
    a.vendorId = 0x106b;
    return a;
}
uint32_t MetalDevice::backBufferFormat() const {
    return 21;
}
uint32_t MetalDevice::displayFormat() const {
    return 21;
}
uint32_t MetalDevice::sourceBlendCaps() const {
    return 0x1fff;
}
bool MetalDevice::fullScreenGamma() const {
    return false;
}
void MetalDevice::setGammaRamp(uint32_t, const uint16_t *, const uint16_t *, const uint16_t *) {}
uint32_t MetalDevice::textureCaps() const {
    return 0x10800;
}
bool MetalDevice::textureFormatOk(uint32_t, uint32_t format) const {
    return format == 21 || format == 22 || format == 23 || format == 0x31545844 || format == 0x33545844 ||
           format == 0x35545844;
}
TextureId MetalDevice::createTexture(const TextureData &d) {
    const std::lock_guard lock(p->deviceLock);
    if (!d.width || !d.height || !d.pixels)
        return 0;
    Impl::Texture t;
    t.w = d.width;
    t.h = d.height;
    SDL_GPUTextureCreateInfo ci{};
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = t.format;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = d.width;
    ci.height = d.height;
    ci.layer_count_or_depth = 1;
    ci.num_levels = std::max(1u, d.levels);
    t.gpu = SDL_CreateGPUTexture(p->gpu, &ci);
    check(t.gpu);
    size_t offset = 0;
    for (uint32_t l = 0; l < ci.num_levels; l++) {
        uint32_t w = std::max(1u, d.width >> l), h = std::max(1u, d.height >> l);
        uint32_t fmt = d.format == PixelFormat::DXT1       ? Ddt::kDxt1
                       : d.format == PixelFormat::DXT3     ? Ddt::kDxt3
                       : d.format == PixelFormat::DXT5     ? Ddt::kDxt5
                       : d.format == PixelFormat::X8R8G8B8 ? 22
                                                           : 21;
        size_t bytes = d.format == PixelFormat::R5G6B5 ? w * h * 2 : levelBytes(fmt, w, h);
        if (offset + bytes > d.size) {
            SDL_ReleaseGPUTexture(p->gpu, t.gpu);
            return 0;
        }
        std::vector<uint32_t> pixels;
        if (d.format == PixelFormat::R5G6B5) {
            pixels.resize(w * h);
            for (size_t i = 0; i < pixels.size(); i++) {
                uint16_t v;
                memcpy(&v, d.pixels + offset + i * 2, 2);
                pixels[i] = 0xff000000 | (((v >> 11) * 255 / 31) << 16) | ((((v >> 5) & 63) * 255 / 63) << 8) |
                            ((v & 31) * 255 / 31);
            }
        } else
            pixels = decodeLevel(fmt, w, h, d.pixels + offset);
        p->uploadTexture(t, (const uint8_t *)pixels.data(), pixels.size() * 4, l, 0, w, h);
        offset += bytes;
    }
    auto id = p->next++;
    p->textures.emplace(id, std::move(t));
    return id;
}
TextureId MetalDevice::createCubeTexture(const TextureData &d) {
    const std::lock_guard lock(p->deviceLock);
    if (!d.width || d.width != d.height || !d.pixels)
        return 0;
    Impl::Texture t;
    t.w = d.width;
    t.h = d.height;
    t.layers = 6;
    SDL_GPUTextureCreateInfo ci{};
    ci.type = SDL_GPU_TEXTURETYPE_CUBE;
    ci.format = t.format;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = d.width;
    ci.height = d.height;
    ci.layer_count_or_depth = 6;
    ci.num_levels = std::max(1u, d.levels);
    t.gpu = SDL_CreateGPUTexture(p->gpu, &ci);
    check(t.gpu);
    size_t off = 0;
    uint32_t fmt = d.format == PixelFormat::DXT1   ? Ddt::kDxt1
                   : d.format == PixelFormat::DXT3 ? Ddt::kDxt3
                   : d.format == PixelFormat::DXT5 ? Ddt::kDxt5
                                                   : 21;
    for (uint32_t face = 0; face < 6; face++)
        for (uint32_t l = 0; l < ci.num_levels; l++) {
            uint32_t w = std::max(1u, d.width >> l), h = std::max(1u, d.height >> l);
            size_t size = levelBytes(fmt, w, h);
            if (off + size > d.size) {
                SDL_ReleaseGPUTexture(p->gpu, t.gpu);
                return 0;
            }
            auto pixels = decodeLevel(fmt, w, h, d.pixels + off);
            p->uploadTexture(t, (uint8_t *)pixels.data(), pixels.size() * 4, l, face, w, h);
            off += size;
        }
    auto id = p->next++;
    p->textures.emplace(id, std::move(t));
    return id;
}
void MetalDevice::destroyTexture(TextureId id) {
    const std::lock_guard lock(p->deviceLock);
    if (auto it = p->textures.find(id); it != p->textures.end()) {
        if (p->onDeviceThread()) p->endPass();
        SDL_ReleaseGPUTexture(p->gpu, it->second.gpu);
        if (it->second.depth)
            SDL_ReleaseGPUTexture(p->gpu, it->second.depth);
        p->textures.erase(it);
    }
}
TextureId MetalDevice::createRenderTarget(uint32_t w, uint32_t h, bool cube) {
    const std::lock_guard lock(p->deviceLock);
    Impl::Texture t;
    t.w = w;
    t.h = h;
    t.layers = cube ? 6 : 1;
    SDL_GPUTextureCreateInfo ci{};
    ci.type = cube ? SDL_GPU_TEXTURETYPE_CUBE : SDL_GPU_TEXTURETYPE_2D;
    ci.format = t.format;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    ci.width = w;
    ci.height = h;
    ci.layer_count_or_depth = t.layers;
    ci.num_levels = 1;
    t.gpu = SDL_CreateGPUTexture(p->gpu, &ci);
    check(t.gpu);
    t.depth = p->makeDepth(w, h);
    auto id = p->next++;
    p->textures.emplace(id, std::move(t));
    return id;
}
void MetalDevice::setRenderTarget(TextureId id, uint32_t face) {
    p->endPass();
    p->target = id;
    p->face = id ? face : 0;
    p->viewport = {0, 0, p->width(), p->height(), 0, 1};
}
uint32_t MetalDevice::targetWidth(TextureId id) const {
    return id ? p->textures.at(id).w : p->w;
}
uint32_t MetalDevice::targetHeight(TextureId id) const {
    return id ? p->textures.at(id).h : p->h;
}
uint32_t MetalDevice::resets() const {
    return 0;
}
bool MetalDevice::readTarget(TextureId id, std::vector<uint8_t> &pixels, uint32_t *pitch) {
    const std::lock_guard lock(p->deviceLock);
    auto tex = id ? p->textures.at(id).gpu : p->backColor;
    if (!tex)
        return false;
    auto w = targetWidth(id), h = targetHeight(id);
    p->endPass();
    SDL_GPUTransferBufferCreateInfo ci{};
    ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    ci.size = w * h * 4;
    auto buffer = SDL_CreateGPUTransferBuffer(p->gpu, &ci);
    if (!buffer)
        return false;
    auto pass = SDL_BeginGPUCopyPass(p->commands());
    SDL_GPUTextureRegion src{};
    src.texture = tex;
    src.w = w;
    src.h = h;
    src.d = 1;
    SDL_GPUTextureTransferInfo dst{};
    dst.transfer_buffer = buffer;
    SDL_DownloadFromGPUTexture(pass, &src, &dst);
    SDL_EndGPUCopyPass(pass);
    auto fence = SDL_SubmitGPUCommandBufferAndAcquireFence(p->cb);
    p->cb = nullptr;
    if (!fence) {
        SDL_ReleaseGPUTransferBuffer(p->gpu, buffer);
        return false;
    }
    SDL_WaitForGPUFences(p->gpu, true, &fence, 1);
    auto data = SDL_MapGPUTransferBuffer(p->gpu, buffer, false);
    if (!data)
        return false;
    pixels.assign((uint8_t *)data, (uint8_t *)data + w * h * 4);
    SDL_UnmapGPUTransferBuffer(p->gpu, buffer);
    SDL_ReleaseGPUFence(p->gpu, fence);
    SDL_ReleaseGPUTransferBuffer(p->gpu, buffer);
    if (pitch)
        *pitch = w * 4;
    return true;
}
TextureId MetalDevice::createDynamicTexture(uint32_t w, uint32_t h, uint32_t format) {
    const std::lock_guard lock(p->deviceLock);
    if (format != 21 && format != 22)
        return 0;
    std::vector<uint8_t> pixels(w * h * 4);
    auto id = createTexture({PixelFormat::A8R8G8B8, w, h, 1, pixels.data(), pixels.size()});
    p->textures.at(id).pixels = std::move(pixels);
    return id;
}
bool MetalDevice::lockTexture(TextureId id, void **bits, int32_t *pitch) {
    const std::lock_guard lock(p->deviceLock);
    auto it = p->textures.find(id);
    if (it == p->textures.end())
        return false;
    auto &t = it->second;
    t.pixels.resize(t.w * t.h * 4);
    *bits = t.pixels.data();
    *pitch = t.w * 4;
    return true;
}
void MetalDevice::unlockTexture(TextureId id) {
    const std::lock_guard lock(p->deviceLock);
    auto &t = p->textures.at(id);
    p->uploadTexture(t, t.pixels.data(), t.pixels.size(), 0, 0, t.w, t.h);
}
ShaderId MetalDevice::createVertexShader(const uint32_t *code) {
    const std::lock_guard lock(p->deviceLock);
    try {
        auto shader = std::make_unique<oj::mac::Shader>(p->gpu, code);
        auto id = p->next++;
        p->shaders[id] = std::move(shader);
        return id;
    } catch (const std::exception &e) {
        log::warning("shader: {}", e.what());
        return 0;
    }
}
ShaderId MetalDevice::createPixelShader(const uint32_t *code) {
    return createVertexShader(code);
}
DeclarationId MetalDevice::createDeclaration(const VertexElement *es) {
    const std::lock_guard lock(p->deviceLock);
    std::vector<VertexElement> v;
    for (int i = 0; i < 64 && es[i].stream != 0xff && es[i].type != 17; i++)
        v.push_back(es[i]);
    auto id = p->next++;
    p->declarations[id] = std::move(v);
    return id;
}
BufferId MetalDevice::createVertexBuffer(const void *d, uint32_t n) {
    const std::lock_guard lock(p->deviceLock);
    return p->makeBuffer(d, n, false);
}
BufferId MetalDevice::createIndexBuffer(const uint16_t *d, uint32_t n) {
    const std::lock_guard lock(p->deviceLock);
    return p->makeBuffer(d, n * 2, true);
}
void MetalDevice::destroyBuffer(BufferId id) {
    const std::lock_guard lock(p->deviceLock);
    if (auto it = p->buffers.find(id); it != p->buffers.end()) {
        SDL_ReleaseGPUBuffer(p->gpu, it->second.gpu);
        p->buffers.erase(it);
    }
}
void MetalDevice::updateVertexBuffer(BufferId id, const void *d, uint32_t n) {
    const std::lock_guard lock(p->deviceLock);
    auto &b = p->buffers.at(id);
    if (n > b.bytes.size())
        throw std::runtime_error("Vertex update exceeds allocation");
    memcpy(b.bytes.data(), d, n);
    p->uploadBuffer(b, d, n);
}
bool MetalDevice::occlusionQueries() const {
    return false;
}
QueryId MetalDevice::createOcclusionQuery() {
    return 0;
}
void MetalDevice::issueQuery(QueryId, bool) {}
int MetalDevice::queryResult(QueryId, bool, uint32_t *) {
    return -1;
}
void MetalDevice::setRenderState(RenderState state, uint32_t value) {
    p->states.at((size_t)state) = value;
}
void MetalDevice::setTextureStageState(uint32_t stage, TextureStageState state, uint32_t value) {
    if (stage < 8)
        p->stages[stage].at((size_t)state) = value;
}
void MetalDevice::setSamplerState(uint32_t slot, SamplerState state, uint32_t v) {
    if (slot < 16)
        p->sampling[slot].at((size_t)state) = v;
}
void MetalDevice::setTexture(uint32_t slot, TextureId id) {
    if (slot < 16)
        p->bound[slot] = id;
}
void MetalDevice::setTransform(TransformState state, const float *m) {
    std::copy(m, m + 16, p->transforms[(uint32_t)state].begin());
}
void MetalDevice::setMaterial(const FixedMaterial &) {}
void MetalDevice::setLight(uint32_t, const FixedLight &) {}
void MetalDevice::lightEnable(uint32_t, bool) {}
void MetalDevice::setFvf(uint32_t v) {
    p->fvf = v;
}
void MetalDevice::setNPatchMode(float) {}
void MetalDevice::setVertexShader(ShaderId id) {
    p->vs = id;
}
void MetalDevice::setPixelShader(ShaderId id) {
    p->ps = id;
}
void MetalDevice::setVertexShaderConstantF(uint32_t start, const float *d, uint32_t count) {
    auto &regs = p->vertexRegisters.floats;
    if ((start + count) * 4 > regs.size())
        throw std::runtime_error("Shader constant range");
    std::copy(d, d + count * 4, regs.begin() + start * 4);
}
void MetalDevice::setPixelShaderConstantF(uint32_t start, const float *d, uint32_t count) {
    auto &regs = p->pixelRegisters.floats;
    if ((start + count) * 4 > regs.size())
        throw std::runtime_error("Shader constant range");
    std::copy(d, d + count * 4, regs.begin() + start * 4);
}
void MetalDevice::setVertexShaderConstantI(uint32_t start, const int32_t *d, uint32_t count) {
    auto &regs = p->vertexRegisters.ints;
    if ((start + count) * 4 > regs.size())
        throw std::runtime_error("Shader constant range");
    std::copy(d, d + count * 4, regs.begin() + start * 4);
}
void MetalDevice::setPixelShaderConstantI(uint32_t start, const int32_t *d, uint32_t count) {
    auto &regs = p->pixelRegisters.ints;
    if ((start + count) * 4 > regs.size())
        throw std::runtime_error("Shader constant range");
    std::copy(d, d + count * 4, regs.begin() + start * 4);
}
void MetalDevice::setVertexShaderConstantB(uint32_t start, const int32_t *d, uint32_t count) {
    auto &regs = p->vertexRegisters.bools;
    if ((start + count) * 1 > regs.size())
        throw std::runtime_error("Shader constant range");
    std::copy(d, d + count * 1, regs.begin() + start * 1);
}
void MetalDevice::setPixelShaderConstantB(uint32_t start, const int32_t *d, uint32_t count) {
    auto &regs = p->pixelRegisters.bools;
    if ((start + count) * 1 > regs.size())
        throw std::runtime_error("Shader constant range");
    std::copy(d, d + count * 1, regs.begin() + start * 1);
}
void MetalDevice::setDeclaration(DeclarationId id) {
    p->decl = id;
}
void MetalDevice::setStream(BufferId id, uint32_t stride) {
    p->stream = id;
    p->stride = stride;
}
void MetalDevice::setIndices(BufferId id) {
    p->indices = id;
}
void MetalDevice::drawIndexed(uint32_t type, uint32_t, uint32_t start, uint32_t count) {
    if (!count)
        return;
    p->prepare(type, false);
    SDL_GPUBufferBinding vb{p->buffers.at(p->stream).gpu, 0}, ib{p->buffers.at(p->indices).gpu, 0};
    SDL_BindGPUVertexBuffers(p->pass, 0, &vb, 1);
    SDL_BindGPUIndexBuffer(p->pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_DrawGPUIndexedPrimitives(p->pass, vertexCount(type, count), 1, start, 0, 0);
    ++p->draws;
}
void MetalDevice::draw(uint32_t type, uint32_t start, uint32_t count) {
    if (!count)
        return;
    p->prepare(type, false);
    SDL_GPUBufferBinding vb{p->buffers.at(p->stream).gpu, 0};
    SDL_BindGPUVertexBuffers(p->pass, 0, &vb, 1);
    SDL_DrawGPUPrimitives(p->pass, vertexCount(type, count), 1, start, 0);
    ++p->draws;
}
void MetalDevice::setViewport(const Viewport &v) {
    p->viewport = v;
}
void MetalDevice::clear(uint32_t flags, uint32_t argb, float z, uint32_t stencil) {
    p->beginPass(
        flags, {((argb >> 16) & 255) / 255.f, ((argb >> 8) & 255) / 255.f, (argb & 255) / 255.f, (argb >> 24) / 255.f},
        z, stencil);
}
void MetalDevice::setClip(const ClipRect *r) {
    p->clipped = r != nullptr;
    if (r)
        p->clip = *r;
}
void MetalDevice::drawQuad(const QuadVertex (&q)[4], TextureId id, Blend mode, bool) {
    FixedVertex v[6];
    int order[] = {0, 1, 2, 2, 1, 3};
    for (int i = 0; i < 6; i++) {
        auto &a = q[order[i]];
        v[i] = {(a.x + .5f) / p->w * 2 - 1, 1 - (a.y + .5f) / p->h * 2, 0, 1, a.u, a.v, a.argb};
    }
    auto old = p->bound[0];
    auto oldBlend = p->states[27], oldSrc = p->states[19], oldDst = p->states[20];
    p->bound[0] = id;
    p->states[27] = 1;
    p->states[19] = 5;
    p->states[20] = mode == Blend::Additive ? 2 : 6;
    p->fixedDraw(v, 6, 4, identity());
    p->bound[0] = old;
    p->states[27] = oldBlend;
    p->states[19] = oldSrc;
    p->states[20] = oldDst;
}
void MetalDevice::drawDebugTriangles(const DebugVertex *v, uint32_t count) {
    std::vector<FixedVertex> out;
    for (uint32_t i = 0; i < count; i++)
        out.push_back({v[i].x / p->w * 2 - 1, 1 - v[i].y / p->h * 2, 0, 1, 0, 0, v[i].argb});
    auto old = p->bound[0];
    p->bound[0] = 0;
    p->fixedDraw(out.data(), count, 4, identity());
    p->bound[0] = old;
}
void MetalDevice::drawLine(float x0, float y0, float x1, float y1, uint32_t argb) {
    FixedVertex v[] = {{x0 / p->w * 2 - 1, 1 - y0 / p->h * 2, 0, 1, 0, 0, argb},
                       {x1 / p->w * 2 - 1, 1 - y1 / p->h * 2, 0, 1, 0, 0, argb}};
    auto old = p->bound[0];
    p->bound[0] = 0;
    p->fixedDraw(v, 2, 2, identity());
    p->bound[0] = old;
}
void MetalDevice::drawUp(uint32_t type, uint32_t count, const void *data, uint32_t stride) {
    uint32_t n = vertexCount(type, count);
    std::vector<FixedVertex> out;
    bool screen = (p->fvf & 0xe) == Fvf::XyzRhw;
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *b = (const uint8_t *)data + i * stride;
        FixedVertex v{};
        memcpy(&v.x, b, 12);
        b += 12;
        v.w = 1;
        if (screen) {
            float rhw;
            memcpy(&rhw, b, 4);
            b += 4;
            v.w = rhw ? 1 / rhw : 1;
            v.x = ((v.x + .5f) / p->width() * 2 - 1) * v.w;
            v.y = (1 - (v.y + .5f) / p->height() * 2) * v.w;
            v.z *= v.w;
        }
        if (p->fvf & Fvf::Normal)
            b += 12;
        if (p->fvf & Fvf::PointSize)
            b += 4;
        v.color = 0xffffffff;
        if (p->fvf & Fvf::Diffuse) {
            memcpy(&v.color, b, 4);
            b += 4;
        }
        if (p->fvf & Fvf::Specular)
            b += 4;
        if (p->fvf & 0xf00)
            memcpy(&v.u, b, 8);
        out.push_back(v);
    }
    if (type == 6) {
        std::vector<FixedVertex> fan;
        for (uint32_t i = 1; i + 1 < n; i++) {
            fan.push_back(out[0]);
            fan.push_back(out[i]);
            fan.push_back(out[i + 1]);
        }
        out = std::move(fan);
        type = 4;
    }
    auto matrix = screen ? identity() : multiply(multiply(p->transforms[256], p->transforms[2]), p->transforms[3]);
    p->fixedDraw(out.data(), out.size(), type, matrix, screen);
}
bool MetalDevice::capture(const std::wstring &path) {
    const std::lock_guard lock(p->deviceLock);
    std::vector<uint8_t> pixels;
    uint32_t pitch;
    if (!readTarget(0, pixels, &pitch))
        return false;
    auto surface = SDL_CreateSurfaceFrom(p->w, p->h, SDL_PIXELFORMAT_BGRA32, pixels.data(), pitch);
    if (!surface)
        return false;
    bool ok = SDL_SaveBMP(surface, std::filesystem::path(path).c_str());
    SDL_DestroySurface(surface);
    return ok;
}
const char *backendName(Backend b) {
    return b == Backend::Metal ? "metal" : "d3d9";
}
std::unique_ptr<IRenderDevice> createDevice(Backend, const DeviceDesc &desc, std::string *error) {
    try {
        return std::make_unique<MetalDevice>(desc);
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return {};
    }
}
} // namespace oj::render
namespace oj::render::hlsl {
std::optional<std::vector<uint8_t>> compile(const std::vector<std::byte> &, const char *, const char *, uint32_t,
                                            const IncludeReader &) {
    log::warning("Runtime HLSL compilation is unavailable on macOS; use the game's compiled effects");
    return {};
}
} // namespace oj::render::hlsl
