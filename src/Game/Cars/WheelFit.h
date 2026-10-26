#pragma once
#include "Core/Formats/Model.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace oj::scene {
// Imported cars can opt into retaining their stock tyre envelope when using the
// shared wheel catalogue. Bake into the mesh so race steering/spin and blur use
// the same dimensions without accumulating changes in the wheel node transform.
inline bool fitWheelMesh(Mesh& mesh, const MeshBounds& stock) {
    const auto source = mesh.bounds;
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
        bounds(part.bounds);
    }
    bounds(mesh.bounds);
    return true;
}
}
