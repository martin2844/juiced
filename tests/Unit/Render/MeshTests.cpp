// Unit tests: the render mesh's buffers (Render/Model.h).

#include "Framework/Test.h"
#include "Framework/Fakes/NullRenderDevice.h"

#include "Core/Formats/Model.h"
#include "Render/Model.h"

#include <cstdint>
#include <map>
#include <vector>

namespace {

using oj::test::check;

// A device that keeps the vertex buffers' bytes.
class BufferDevice : public oj::test::NullRenderDevice {
public:
    std::map<oj::render::BufferId, std::vector<uint8_t>> buffers;

    oj::render::BufferId createVertexBuffer(const void* data, uint32_t bytes) override {
        const oj::render::BufferId id = NullRenderDevice::createVertexBuffer(data, bytes);
        const auto* p = static_cast<const uint8_t*>(data);
        buffers[id].assign(p, p + bytes);
        return id;
    }
    void updateVertexBuffer(oj::render::BufferId buffer, const void* data, uint32_t bytes) override {
        const auto* p = static_cast<const uint8_t*>(data);
        std::vector<uint8_t>& b = buffers[buffer];
        if (b.size() < bytes) b.resize(bytes);
        std::copy(p, p + bytes, b.begin());
    }
};

oj::Mesh twoParts(uint8_t first, uint8_t second) {
    oj::Mesh m;
    m.indices = {0, 1, 2};
    for (uint8_t v : {first, second}) {
        oj::MeshPart p;
        p.stride = 4;
        p.vertexCount = 2;
        p.vertices.assign(8, v);
        m.parts.push_back(p);
    }
    return m;
}

}  // namespace

// 0x005582B0: the file's vertices read again into each part's buffer, stride x count bytes of the mesh's own; a file
// with fewer parts fails.
OJ_UNIT_TEST(RenderMesh, ReloadVertices) {
    BufferDevice device;
    oj::render::Mesh mesh(device, twoParts(1, 2), {});
    const bool made = device.buffers.size() == 2 && device.buffers.begin()->second == std::vector<uint8_t>(8, 1);
    const bool reloaded = mesh.reloadVertices(device, twoParts(7, 9));
    std::vector<uint8_t> parts;
    for (const auto& [id, bytes] : device.buffers) parts.push_back(bytes[0]);
    check(made && reloaded && parts == std::vector<uint8_t>{7, 9}, "mesh: the file's vertices into each part's buffer");

    oj::Mesh one = twoParts(5, 5);
    one.parts.pop_back();
    check(!mesh.reloadVertices(device, one), "mesh: a file with fewer parts fails");
}
