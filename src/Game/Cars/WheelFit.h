#pragma once
#include "Core/Formats/Model.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace oj::scene {
// Imported cars can opt into retaining their stock tyre envelope when using the
// shared wheel catalogue. Bake into the mesh so race steering/spin and blur use
// the same dimensions without accumulating changes in the wheel node transform.
inline bool fitWheelMesh(Mesh& mesh, const MeshBounds& stock) {
    // Some wheel packs pad culling boxes by 2 cm on every side. Fitting that
    // box makes the visible tyre too small. Measure the actual position stream
    // and validate all parts before changing any vertices.
    const auto measure = [](const MeshPart& part, MeshBounds& b) {
        const VertexElement* position = nullptr;
        for (const auto& element : part.declaration) {
            if (element.stream == 0 && element.usage == 0 && element.usageIndex == 0) {
                if (position || element.type != 2 || element.offset + 12 > part.stride) return false;
                position = &element;
            }
        }
        if (!position || !part.vertexCount ||
            uint64_t(part.stride) * part.vertexCount != part.vertices.size()) return false;
        b.min.fill(std::numeric_limits<float>::infinity());
        b.max.fill(-std::numeric_limits<float>::infinity());
        for (size_t offset = position->offset; offset + 12 <= part.vertices.size(); offset += part.stride) {
            float p[3]; std::memcpy(p, part.vertices.data() + offset, 12);
            for (int axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(p[axis])) return false;
                b.min[axis] = std::min(b.min[axis], p[axis]);
                b.max[axis] = std::max(b.max[axis], p[axis]);
            }
        }
        float radius2 = 0;
        for (int axis = 0; axis < 3; ++axis) {
            b.centre[axis] = b.min[axis] * .5f + b.max[axis] * .5f;
            const float half = (b.max[axis] - b.min[axis]) * .5f;
            radius2 += half * half;
        }
        b.radius = std::sqrt(radius2); // Conservative box sphere for culling.
        return true;
    };
    auto source = mesh.bounds;
    source.min.fill(std::numeric_limits<float>::infinity());
    source.max.fill(-std::numeric_limits<float>::infinity());
    bool found = false;
    for (const auto& part : mesh.parts) {
        if (!part.vertexCount && part.vertices.empty()) continue;
        auto measured = part.bounds;
        if (!measure(part, measured)) return false;
        for (int axis = 0; axis < 3; ++axis) {
            source.min[axis] = std::min(source.min[axis], measured.min[axis]);
            source.max[axis] = std::max(source.max[axis], measured.max[axis]);
        }
        found = true;
    }
    if (!found) return false;
    float radius2 = 0;
    for (int axis = 0; axis < 3; ++axis) {
        source.centre[axis] = source.min[axis] * .5f + source.max[axis] * .5f;
        const float half = (source.max[axis] - source.min[axis]) * .5f;
        radius2 += half * half;
    }
    source.radius = std::sqrt(radius2);
    const float width = source.max[0] - source.min[0];
    const float diameter = std::max(source.max[1] - source.min[1], source.max[2] - source.min[2]);
    const float targetWidth = stock.max[0] - stock.min[0];
    const float targetDiameter = std::max(stock.max[1] - stock.min[1], stock.max[2] - stock.min[2]);
    if (!(width > 0 && diameter > 0 && targetWidth > 0 && targetDiameter > 0)) return false;
    const float scale[] = {targetWidth / width, targetDiameter / diameter, targetDiameter / diameter};
    for (float value : scale) if (!std::isfinite(value) || value < .1f || value > 10.f) return false;
    auto position = [&](float* p) {
        for (int i = 0; i < 3; ++i)
            p[i] = (p[i] - (source.min[i] + source.max[i]) * .5f) * scale[i]
                + (stock.min[i] + stock.max[i]) * .5f;
    };
    auto bounds = [&](MeshBounds& b) {
        position(b.min.data()); position(b.max.data()); position(b.centre.data());
        b.radius *= std::max({scale[0], scale[1], scale[2]});
    };
    for (auto& part : mesh.parts) {
        for (const auto& element : part.declaration) {
            if (element.stream != 0 || element.type != 2 || element.usageIndex != 0 ||
                (element.usage != 0 && element.usage != 3) || element.offset + 12 > part.stride) continue;
            for (size_t offset = element.offset; offset + 12 <= part.vertices.size(); offset += part.stride) {
                float value[3]; std::memcpy(value, part.vertices.data() + offset, 12);
                if (element.usage == 0) position(value);
                else {
                    for (int i = 0; i < 3; ++i) value[i] /= scale[i];
                    const float length = std::sqrt(value[0]*value[0] + value[1]*value[1] + value[2]*value[2]);
                    if (length > 0) for (float& component : value) component /= length;
                }
                std::memcpy(part.vertices.data() + offset, value, 12);
            }
        }
        if (part.vertexCount) measure(part, part.bounds);
        else bounds(part.bounds);
    }
    mesh.bounds = source;
    bounds(mesh.bounds);
    return true;
}
}
