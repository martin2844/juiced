#pragma once
// The fixed function states the engine sets through IRenderDevice, by their Direct3D 9 numbers (d3d9types.h): which
// state (RenderState, TextureStageState, SamplerState, TransformState) and the values they take.
//
// The states are scoped enums: a state is never a bare number at a call. The values are plain enumerators in a
// struct (Cull::None, BlendFactor::SrcAlpha, ...) that convert to the device's uint32_t: a state's value is also read
// from the effects' files and the game's data as a number.

#include <cstdint>

namespace oj::render {

// D3DRENDERSTATETYPE.
enum class RenderState : uint32_t {
    ZEnable = 7,
    FillMode = 8,
    ShadeMode = 9,
    ZWriteEnable = 14,
    AlphaTestEnable = 15,
    LastPixel = 16,
    SrcBlend = 19,
    DestBlend = 20,
    CullMode = 22,
    ZFunc = 23,
    AlphaRef = 24,
    AlphaFunc = 25,
    DitherEnable = 26,
    AlphaBlendEnable = 27,
    FogEnable = 28,
    SpecularEnable = 29,
    FogColour = 34,
    FogTableMode = 35,
    FogStart = 36,
    FogEnd = 37,
    FogDensity = 38,
    RangeFogEnable = 48,
    StencilEnable = 52,
    StencilFail = 53,
    StencilZFail = 54,
    StencilPass = 55,
    StencilFunc = 56,
    StencilRef = 57,
    StencilMask = 58,
    StencilWriteMask = 59,
    TextureFactor = 60,
    Wrap0 = 128,
    Clipping = 136,
    Lighting = 137,
    Ambient = 139,
    FogVertexMode = 140,
    ColourVertex = 141,
    LocalViewer = 142,
    NormalizeNormals = 143,
    DiffuseMaterialSource = 145,
    SpecularMaterialSource = 146,
    AmbientMaterialSource = 147,
    EmissiveMaterialSource = 148,
    VertexBlend = 151,
    ClipPlaneEnable = 152,
    PointSize = 154,
    PointSizeMin = 155,
    PointSpriteEnable = 156,
    PointScaleEnable = 157,
    PointScaleA = 158,
    PointScaleB = 159,
    PointScaleC = 160,
    MultisampleAntialias = 161,
    MultisampleMask = 162,
    PatchEdgeStyle = 163,
    DebugMonitorToken = 165,
    PointSizeMax = 166,
    IndexedVertexBlendEnable = 167,
    ColourWriteEnable = 168,
    TweenFactor = 170,
    BlendOp = 171,
    PositionDegree = 172,
    NormalDegree = 173,
    ScissorTestEnable = 174,
    SlopeScaleDepthBias = 175,
    AntialiasedLineEnable = 176,
    MinTessellationLevel = 178,
    MaxTessellationLevel = 179,
    AdaptiveTessX = 180,
    AdaptiveTessY = 181,
    AdaptiveTessZ = 182,
    AdaptiveTessW = 183,
    EnableAdaptiveTessellation = 184,
    TwoSidedStencilMode = 185,
    CcwStencilFail = 186,
    CcwStencilZFail = 187,
    CcwStencilPass = 188,
    CcwStencilFunc = 189,
    ColourWriteEnable1 = 190,
    ColourWriteEnable2 = 191,
    ColourWriteEnable3 = 192,
    BlendFactor = 193,
    SrgbWriteEnable = 194,
    DepthBias = 195,
    Wrap8 = 198,
    SeparateAlphaBlendEnable = 206,
    SrcBlendAlpha = 207,
    DestBlendAlpha = 208,
    BlendOpAlpha = 209,
};

// D3DTEXTURESTAGESTATETYPE.
enum class TextureStageState : uint32_t {
    ColourOp = 1,
    ColourArg1 = 2,
    ColourArg2 = 3,
    AlphaOp = 4,
    AlphaArg1 = 5,
    AlphaArg2 = 6,
    BumpEnvMat00 = 7,
    BumpEnvMat01 = 8,
    BumpEnvMat10 = 9,
    BumpEnvMat11 = 10,
    TexCoordIndex = 11,
    BumpEnvLScale = 22,
    BumpEnvLOffset = 23,
    TextureTransformFlags = 24,
    ColourArg0 = 26,
    AlphaArg0 = 27,
    ResultArg = 28,
    Constant = 32,
};

// D3DSAMPLERSTATETYPE.
enum class SamplerState : uint32_t {
    AddressU = 1,
    AddressV = 2,
    AddressW = 3,
    BorderColour = 4,
    MagFilter = 5,
    MinFilter = 6,
    MipFilter = 7,
    MipMapLodBias = 8,
    MaxMipLevel = 9,
    MaxAnisotropy = 10,
    SrgbTexture = 11,
    ElementIndex = 12,
    DMapOffset = 13,
};

// D3DTRANSFORMSTATETYPE; a texture's or a world's matrix by its index from Texture0 or World.
enum class TransformState : uint32_t {
    View = 2,
    Projection = 3,
    Texture0 = 16,
    World = 256,
};

// The values.

// D3DCULL (CullMode).
struct Cull {
    enum : uint32_t { None = 1, Clockwise = 2, CounterClockwise = 3 };
};

// D3DBLEND (SrcBlend, DestBlend).
struct BlendFactor {
    enum : uint32_t {
        Zero = 1,
        One = 2,
        SrcColour = 3,
        InvSrcColour = 4,
        SrcAlpha = 5,
        InvSrcAlpha = 6,
        DestAlpha = 7,
        InvDestAlpha = 8,
        DestColour = 9,
        InvDestColour = 10,
        SrcAlphaSat = 11,
        BothSrcAlpha = 12,
        BothInvSrcAlpha = 13,
        Constant = 14,     // D3DBLEND_BLENDFACTOR: RenderState::BlendFactor
        InvConstant = 15,  // D3DBLEND_INVBLENDFACTOR
    };
};

// D3DCMPFUNC (ZFunc, AlphaFunc, StencilFunc).
struct CompareFunc {
    enum : uint32_t { Never = 1, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };
};

// D3DSTENCILOP (StencilFail, StencilZFail, StencilPass).
struct StencilOp {
    enum : uint32_t { Keep = 1, Zero, Replace, IncrementClamp, DecrementClamp, Invert, Increment, Decrement };
};

// D3DCOLORWRITEENABLE bits (ColourWriteEnable).
struct ColourWrite {
    enum : uint32_t { None = 0, Red = 1, Green = 2, Blue = 4, Alpha = 8, Rgb = Red | Green | Blue, All = Rgb | Alpha };
};

// D3DFOGMODE (FogTableMode, FogVertexMode).
struct FogMode {
    enum : uint32_t { None = 0, Exp = 1, Exp2 = 2, Linear = 3 };
};

// D3DMATERIALCOLORSOURCE (DiffuseMaterialSource .. EmissiveMaterialSource).
struct MaterialSource {
    enum : uint32_t { Material = 0, Colour1 = 1, Colour2 = 2 };
};

// D3DTEXTUREOP (ColourOp, AlphaOp).
struct TextureOp {
    enum : uint32_t {
        Disable = 1,
        SelectArg1 = 2,
        SelectArg2 = 3,
        Modulate = 4,
        Modulate2x = 5,
        Modulate4x = 6,
        Add = 7,
        AddSigned = 8,
        AddSigned2x = 9,
        Subtract = 10,
        AddSmooth = 11,
        BlendDiffuseAlpha = 12,
        BlendTextureAlpha = 13,
        BlendFactorAlpha = 14,
        BlendTextureAlphaPremultiplied = 15,
        BlendCurrentAlpha = 16,
        Premodulate = 17,
        ModulateAlphaAddColour = 18,
        ModulateColourAddAlpha = 19,
        ModulateInvAlphaAddColour = 20,
        ModulateInvColourAddAlpha = 21,
        BumpEnvMap = 22,
        BumpEnvMapLuminance = 23,
        DotProduct3 = 24,
        MultiplyAdd = 25,
        Lerp = 26,
    };
};

// D3DTA_* (ColourArg1, ColourArg2, AlphaArg1, AlphaArg2): a source and its modifiers.
struct TextureArg {
    enum : uint32_t {
        Diffuse = 0,
        Current = 1,
        Texture = 2,
        Factor = 3,  // RenderState::TextureFactor
        Specular = 4,
        Temp = 5,
        Constant = 6,
        Complement = 0x10,
        AlphaReplicate = 0x20,
    };
};

// D3DTEXTURETRANSFORMFLAGS (TextureTransformFlags).
struct TextureTransform {
    enum : uint32_t { Disable = 0, Count1 = 1, Count2 = 2, Count3 = 3, Count4 = 4, Projected = 256 };
};

// D3DTEXTUREADDRESS (AddressU, AddressV, AddressW).
struct TextureAddress {
    enum : uint32_t { Wrap = 1, Mirror = 2, Clamp = 3, Border = 4, MirrorOnce = 5 };
};

// D3DTEXTUREFILTERTYPE (MagFilter, MinFilter, MipFilter).
struct TextureFilter {
    enum : uint32_t { None = 0, Point = 1, Linear = 2, Anisotropic = 3 };
};

// D3DPRIMITIVETYPE (IRenderDevice::draw, drawIndexed).
struct Primitive {
    enum : uint32_t {
        PointList = 1,
        LineList = 2,
        LineStrip = 3,
        TriangleList = 4,
        TriangleStrip = 5,
        TriangleFan = 6,
    };
};

// D3DCLEAR_* (IRenderDevice::clear).
struct ClearFlags {
    enum : uint32_t { Target = 1, ZBuffer = 2, Stencil = 4 };
};

// D3DFVF_* (IRenderDevice::setFvf): a vertex's parts.
struct Fvf {
    enum : uint32_t {
        Xyz = 0x002,
        XyzRhw = 0x004,
        Normal = 0x010,
        PointSize = 0x020,
        Diffuse = 0x040,
        Specular = 0x080,
        Tex1 = 0x100,
        Tex2 = 0x200,
        Tex3 = 0x300,
        Tex4 = 0x400,
    };
};

}  // namespace oj::render
