#pragma once
// The compiled effects of the stock game (effects\material_*.fxo, docs/spec/11-effects.md): the fx_2_0 binary the
// game hands to the Direct3D extension library it links statically. OpenJuice reads the file itself: parameters,
// techniques with their passes and states, the shaders' byte code, the constant tables of the shaders and the
// expressions (preshaders) that compute states and constants from parameters.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oj::fx {

// D3DXPARAMETER_CLASS
enum class Class : uint32_t { Scalar, Vector, MatrixRows, MatrixColumns, Object, Struct };

// D3DXPARAMETER_TYPE
enum class Type : uint32_t {
    Void,
    Bool,
    Int,
    Float,
    String,
    Texture,
    Texture1D,
    Texture2D,
    Texture3D,
    TextureCube,
    Sampler,
    Sampler1D,
    Sampler2D,
    Sampler3D,
    SamplerCube,
    PixelShader,
    VertexShader,
    PixelFragment,
    VertexFragment,
    Unsupported,
};

bool isSampler(Type type);
bool isTexture(Type type);

struct State;

// A parameter, an annotation, a member of a struct, an element of an array, or the value of a state.
struct Parameter {
    std::string name;
    std::string semantic;
    Class cls = Class::Scalar;
    Type type = Type::Void;
    uint32_t rows = 0;
    uint32_t columns = 0;
    uint32_t elements = 0;
    uint32_t flags = 0;  // of a top-level parameter (D3DX_PARAMETER_SHARED 1, _LITERAL 2, _ANNOTATION 4)
    uint32_t bytes = 0;  // the size of the value: 4 per number, 4 for a string, texture or shader, 0 for a sampler
    // The elements of an array, else the members of a struct.
    std::vector<Parameter> members;
    // A number's (scalar, vector, matrix) value: rows x columns 32-bit values, row after row.
    std::vector<uint8_t> value;
    // A string's, texture's or shader's object (Effect::objects).
    uint32_t object = 0;
    // A sampler's states.
    std::vector<State> samplerStates;
    // A top-level parameter's annotations.
    std::vector<Parameter> annotations;

    const Parameter* annotation(std::string_view key) const;
};

enum class StateValue {
    Constant,    // the value parameter's own value (a shader: its object)
    Parameter,   // the value of the parameter named by `text`
    Expression,  // the result of the expression `code`
};

// One assignment of a pass or a sampler: the operation (stateInfo) for an index (a stage, a register, a light).
struct State {
    uint32_t operation = 0;
    uint32_t index = 0;
    Parameter value;
    StateValue kind = StateValue::Constant;
    std::string text;
    std::vector<uint32_t> code;
};

struct Pass {
    std::string name;
    std::vector<Parameter> annotations;
    std::vector<State> states;
};

struct Technique {
    std::string name;
    std::vector<Parameter> annotations;
    std::vector<Pass> passes;

    const Parameter* annotation(std::string_view key) const;
};

struct Effect {
    std::vector<Parameter> parameters;
    std::vector<Technique> techniques;
    // By object id: a string's characters (with its terminating zero) or a shader's byte code; empty when the file
    // gives no data.
    std::vector<std::vector<uint8_t>> objects;

    const Parameter* parameter(std::string_view key) const;
    std::string_view string(uint32_t object) const;
};

// None when the data is not an fx_2_0 effect or does not read to its end.
std::optional<Effect> parseEffect(const std::vector<uint8_t>& data);

// What a state operation sets (Direct3D 9 values).
enum class StateClass {
    RenderState,     // value: D3DRENDERSTATETYPE
    TextureStage,    // value: D3DTEXTURESTAGESTATETYPE, index: the stage
    NPatchMode,
    Fvf,
    Transform,       // value: D3DTRANSFORMSTATETYPE (D3DTS_TEXTURE0: + index)
    Material,        // value: MaterialMember
    Light,           // value: LightMember, index: the light
    LightEnable,     // index: the light
    VertexShader,
    PixelShader,
    ShaderConstant,  // value: ShaderConstant, index: the first register
    Texture,         // a sampler's texture
    SamplerState,    // value: D3DSAMPLERSTATETYPE
    Sampler,         // a sampler parameter for the sampler register index
};

enum MaterialMember : uint32_t { MaterialDiffuse, MaterialAmbient, MaterialSpecular, MaterialEmissive, MaterialPower };

enum LightMember : uint32_t {
    LightType,
    LightDiffuse,
    LightSpecular,
    LightAmbient,
    LightPosition,
    LightDirection,
    LightRange,
    LightFalloff,
    LightAttenuation0,
    LightAttenuation1,
    LightAttenuation2,
    LightTheta,
    LightPhi,
};

enum ShaderConstant : uint32_t { VertexFloat, VertexBool, VertexInt, PixelFloat, PixelBool, PixelInt };

struct StateInfo {
    StateClass cls;
    uint32_t value;
    const char* name;
};

// nullptr for an operation past the table.
const StateInfo* stateInfo(uint32_t operation);

// The constant table (CTAB) of a shader's byte code or of an expression.
enum class RegisterSet : uint16_t { Bool, Int4, Float4, Sampler };

struct ConstantType {
    Class cls = Class::Scalar;
    Type type = Type::Void;
    uint16_t rows = 0;
    uint16_t columns = 0;
    uint16_t elements = 0;
    std::vector<std::pair<std::string, ConstantType>> members;
};

struct Constant {
    std::string name;
    RegisterSet set = RegisterSet::Float4;
    uint16_t registerIndex = 0;
    uint16_t registerCount = 0;
    ConstantType type;
    // The default value (4 floats per register) when the table gives one (a texture shader's globals).
    std::vector<float> defaults;
};

struct ConstantTable {
    std::string creator;
    uint32_t version = 0;
    std::string target;
    std::vector<Constant> constants;
};

// The words of a shader's or an expression's byte code; the comment blocks follow the version word.
std::vector<uint32_t> words(const std::vector<uint8_t>& bytes);

// The body of the first comment block with the tag (the words after the tag); none when there is none.
std::optional<std::vector<uint32_t>> comment(const std::vector<uint32_t>& code, std::string_view tag);

std::optional<ConstantTable> constantTable(const std::vector<uint32_t>& code);

// An expression program (FXLC): a state's value (StateValue::Expression) or a shader's preshader (the PRES comment).
// Its inputs are parameters by its constant table; it writes output registers.
struct Operand {
    // TexelInput: a texture shader's (tx_1_0) position (register 0) and texel size (register 1).
    enum Table : uint32_t { Literal = 1, Input = 2, TexelInput = 3, Output = 4, OutputBool = 5, OutputInt = 6, Temp = 7 };
    Table table = Literal;
    uint32_t offset = 0;  // in components (4 per register), in registers for OutputBool
    bool relative = false;
    Table indexTable = Literal;
    uint32_t indexOffset = 0;
};

enum class Op {
    Nop,
    Mov,
    Neg,
    Rcp,
    Frc,
    Exp,
    Log,
    Rsq,
    Sin,
    Cos,
    Asin,
    Acos,
    Atan,
    Min,
    Max,
    Lt,
    Ge,
    Add,
    Mul,
    Atan2,
    Div,
    Cmp,
    Dot,
    DotSwiz6,
    DotSwiz8,
};

struct Instruction {
    Op op = Op::Nop;
    uint32_t components = 1;
    bool scalar = false;  // the first input is one component for all
    std::vector<Operand> inputs;
    Operand output;
};

struct Program {
    std::vector<double> literals;
    std::vector<Instruction> instructions;
    ConstantTable inputs;
};

// `code`: an expression's words (the version word first) or a shader's PRES comment body. None when an instruction
// is not known; a program without instructions when there is no FXLC block.
std::optional<Program> parseProgram(const std::vector<uint32_t>& code);

// The registers of a program run: the inputs (filled by the caller from the parameters, by the program's constant
// table) and the outputs it writes. Values are floats as the shader constants take them.
struct Registers {
    std::vector<float> input;   // 4 per register
    std::vector<float> texel;   // a texture shader's: the texel's position, then its size (4 each)
    std::vector<float> output;  // 4 per register
    std::vector<int32_t> outputBool;
    std::vector<int32_t> outputInt;  // 4 per register
    std::vector<float> temp;
};

void run(const Program& program, Registers& registers);

}  // namespace oj::fx
