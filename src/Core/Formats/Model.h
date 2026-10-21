#pragma once
// The model files of the stock game (docs/spec/10-models.md): the node tree (.boh), the meshes (.d8m of a model) and
// the materials (.pmd).

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oj {

// A node of a .boh: a mesh (attach 0, "NONE" is a slot the game fills) or a light (attach 2, a text of keys).
struct BohNode {
    std::string name;
    std::array<float, 16> matrix{};  // row-major, the position in [3], [7], [11]; relative to the parent
    uint32_t attach = 0;
    std::string mesh;   // attach 0
    std::string light;  // attach 2
    std::vector<BohNode> children;
};

// The top nodes of a .boh (one in the shipped files); none when the data does not parse to its end.
std::optional<std::vector<BohNode>> parseBoh(const std::vector<uint8_t>& data);

// A D3DVERTEXELEMENT9 as the file stores it.
struct VertexElement {
    uint16_t stream = 0;
    uint16_t offset = 0;
    uint8_t type = 0;
    uint8_t method = 0;
    uint8_t usage = 0;
    uint8_t usageIndex = 0;
};

struct MeshBounds {
    std::array<float, 3> centre{};
    float w = 0.0f;
    float radius = 0.0f;
    std::array<float, 3> min{};
    std::array<float, 3> max{};
};

// A part of a model mesh: its material's name, its triangles (indices into the file's pool, local to the part's
// vertices) and its vertices.
struct MeshPart {
    std::string name;
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
    uint32_t faceType = 0;  // 0 triangle strip (0xFFFF restarts), 1 triangle list
    MeshBounds bounds;
    float lodIn = 0.0f;     // squared distances (cm²) between which the part is drawn
    float lodOut = 0.0f;
    uint32_t renderFlag = 0;
    std::vector<VertexElement> declaration;  // with the D3DDECL_END record
    uint32_t stride = 0;
    uint32_t vertexCount = 0;
    std::vector<uint8_t> vertices;
};

struct Mesh {
    std::vector<uint16_t> indices;
    std::vector<MeshPart> parts;
    MeshBounds bounds;  // of the whole model
};

// A model .d8m of the release layout; none when it does not parse to its end (the early layout of some files in
// mesh\, a route's .d8m).
std::optional<Mesh> parseMesh(const std::vector<uint8_t>& data);

// A .pmd: "Material Program = <class>", then "<key> = <value>" lines. A texture value is a path of which the name
// counts.
struct Material {
    std::string className;
    std::vector<std::pair<std::string, std::string>> values;  // in the file's order

    const std::string* find(std::string_view key) const;
};

Material parseMaterial(std::string_view text);

// The name a texture value gives (the file name without folders and extension).
std::string textureName(std::string_view value);

}  // namespace oj
