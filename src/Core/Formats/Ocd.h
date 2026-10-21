#pragma once
// A collision mesh of a car or a physics object (.ocd; the layout is the Blender add-on's docs/technical/OCD): the
// vertices, the triangles with their normals, and a tree of spheres over them (loader 0x00535840).

#include "Core/Base/Matrix.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace oj {

struct Ocd {
    struct Face {
        std::array<uint32_t, 3> vertices{};
        Vec4 normal{};  // outward
    };
    struct Node {
        Vec4 centre{};
        float radius = 0.0f;
        int32_t sibling = -1;  // the next child of the same parent, -1 the last
        int32_t child = -1;    // the first child, -1 for a leaf
        uint32_t first = 0;    // the leaf's faces in the face map
        uint32_t count = 0;
    };
    std::vector<Vec4> vertices;  // centimetres of the model
    std::vector<Face> faces;
    std::vector<uint32_t> faceMap;
    std::vector<Node> nodes;
    float lowestY = 3.4028235e38f;  // +0x28: the lowest vertex's y (FLT_MAX without vertices)
};

// None when the data does not read; files written by old tools may end after the face map's count of 0.
std::optional<Ocd> parseOcd(const uint8_t* data, size_t size);

}  // namespace oj
