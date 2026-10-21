#include "Core/Tracks/Track.h"

#include "Core/Base/ByteReader.h"
#include "Core/Base/Script.h"

#include <algorithm>
#include <cstring>

namespace oj {

namespace {

constexpr size_t kMaxCount = 1u << 24;

bool readDeclaration(ByteReader& r, std::vector<VertexElement>& out) {
    const uint32_t count = r.u32();
    if (!r.fits(count, 8) || count == 0 || count > 64) return false;
    out.resize(count);
    for (VertexElement& e : out) {
        e.stream = r.u16();
        e.offset = r.u16();
        uint8_t b[4] = {};
        r.bytes(b, 4);
        e.type = b[0];
        e.method = b[1];
        e.usage = b[2];
        e.usageIndex = b[3];
    }
    return r.ok() && out.back().stream == 0xFF;
}

bool readMesh(ByteReader& r, uint32_t primitive, TrackMesh& m) {
    m.materialId = r.u32();
    for (float& v : m.centre) v = r.f32();
    m.radius = r.f32();
    for (float& v : m.range) v = r.f32();
    if (!readDeclaration(r, m.declaration)) return false;
    m.stride = r.u32();
    m.vertexCount = r.u32();
    if (!r.fits(m.vertexCount, m.stride)) return false;
    m.vertices.resize(static_cast<size_t>(m.vertexCount) * m.stride);
    r.bytes(m.vertices.data(), m.vertices.size());
    if (primitive != 3) {
        const uint32_t indexCount = r.u32();
        if (!r.fits(indexCount, 2)) return false;
        m.indices.resize(indexCount);
        r.bytes(m.indices.data(), m.indices.size() * 2);
    }
    const uint32_t nodeCount = r.u32();
    if (!r.fits(nodeCount, 64)) return false;
    m.nodes.resize(nodeCount);
    for (auto& node : m.nodes) {
        for (float& v : node) v = r.f32();
    }
    return r.ok();
}

}  // namespace

std::optional<Tdf> parseTdf(const std::vector<uint8_t>& data) {
    ByteReader r(data);
    Tdf tdf;
    tdf.track = r.name(32);
    const uint32_t routes = r.u32();
    if (!r.fits(routes, 36)) return std::nullopt;
    tdf.routes.resize(routes);
    for (TdfRoute& route : tdf.routes) {
        route.name = r.name(32);
        const uint32_t entries = r.u32();
        if (!r.fits(entries, 40)) return std::nullopt;
        route.entries.resize(entries);
        for (TdfEntry& e : route.entries) {
            e.hidden = r.u32();
            e.reflected = r.u32();
            e.name = r.name(32);
        }
    }
    if (!r.ok() || !r.atEnd()) return std::nullopt;
    return tdf;
}

uint32_t conditionBit(bool night, bool wet) {
    return (night ? 4u : 1u) << (wet ? 1 : 0);
}

std::optional<std::vector<TrackGroup>> parseD8g(const std::vector<uint8_t>& data) {
    ByteReader r(data);
    std::vector<TrackGroup> groups;
    if (data.empty()) return groups;
    const uint32_t count = r.u32();
    if (count > 16) return std::nullopt;
    groups.resize(count);
    for (TrackGroup& g : groups) {
        g.primitive = r.u32();
        const uint32_t meshes = r.u32();
        if (!r.ok() || meshes > kMaxCount || g.primitive < 1 || g.primitive > 3) return std::nullopt;
        g.meshes.resize(meshes);
        for (TrackMesh& m : g.meshes) {
            if (!readMesh(r, g.primitive, m)) return std::nullopt;
        }
    }
    if (!r.ok() || !r.atEnd()) return std::nullopt;
    return groups;
}

std::optional<RouteMaterials> parseRouteMaterials(const std::vector<uint8_t>& data) {
    ByteReader r(data);
    RouteMaterials out;
    const uint32_t count = r.u32();
    if (!r.fits(count, 12)) return std::nullopt;
    out.materials.resize(count);
    for (RouteMaterial& m : out.materials) {
        m.classId = r.u32();
        const uint32_t params = r.u32();
        if (!r.fits(params, 8)) return std::nullopt;
        m.params.resize(params);
        for (auto& p : m.params) {
            const uint32_t size = r.u32();
            if (!r.fits(size, 1) || size > 4096) return std::nullopt;
            p.resize(size);
            r.bytes(p.data(), size);
        }
        const uint32_t textures = r.u32();
        if (!r.fits(textures, 4)) return std::nullopt;
        m.textures.resize(textures);
        for (uint32_t& t : m.textures) t = r.u32();
    }
    const uint32_t ids = r.u32();
    if (!r.fits(ids, 4)) return std::nullopt;
    out.remap.resize(ids);
    for (int32_t& v : out.remap) v = r.i32();
    if (!r.ok() || !r.atEnd()) return std::nullopt;
    return out;
}

bool readTextureDirectory(ByteReader& r, TextureDirectory& out) {
    out.textures.clear();
    const uint32_t total = r.u32();
    const uint32_t tables = r.u32();
    r.u32();  // the sum of the tables' sizes
    if (!r.fits(tables, 12)) return false;
    uint32_t cursor = 0;
    for (uint32_t t = 0; t < tables; ++t) {
        const uint32_t skip = r.u32();
        const uint32_t size = r.u32();
        const uint32_t count = r.u32();
        if (!r.fits(count, 48)) return false;
        cursor += skip;
        const uint32_t tableStart = cursor;
        uint32_t at = tableStart;
        for (uint32_t i = 0; i < count; ++i) {
            TextureEntry e;
            e.offset = at;
            e.dataSize = r.u32();
            r.bytes(e.header.data(), e.header.size());
            at += e.dataSize;
            out.textures.push_back(e);
        }
        cursor = tableStart + size;
    }
    out.end = cursor;
    return r.ok() && out.textures.size() == total;
}

std::optional<TextureDirectory> parseShd(const std::vector<uint8_t>& data) {
    ByteReader r(data);
    TextureDirectory d;
    if (!readTextureDirectory(r, d) || !r.atEnd()) return std::nullopt;
    return d;
}

std::optional<D8w> parseD8w(const std::vector<uint8_t>& data, size_t instances) {
    ByteReader r(data);
    D8w w;
    if (!readTextureDirectory(r, w.textures)) return std::nullopt;
    const uint32_t sets = r.u32();
    if (!r.fits(sets, 32 + 4 * static_cast<uint64_t>(instances))) return std::nullopt;
    w.sets.resize(sets);
    for (D8w::LightSet& s : w.sets) {
        s.name = r.name(32);
        s.tiles.resize(instances);
        for (int32_t& t : s.tiles) t = r.i32();
    }
    for (float& v : w.area) v = r.f32();
    const uint32_t treeSize = r.u32();
    if (!r.fits(treeSize, 1)) return std::nullopt;
    w.tree.resize(treeSize);
    r.bytes(w.tree.data(), treeSize);
    r.u32();
    r.u32();
    if (!r.ok() || !r.atEnd()) return std::nullopt;
    return w;
}

namespace {

// One node and, for an inner node, its children after it; false when the bytes end first.
bool readTreeNode(ByteReader& r, std::vector<RenderTreeNode>& out, uint32_t depth) {
    if (depth > 64) return false;
    const size_t at = out.size();
    out.emplace_back();
    const uint32_t flags = r.u32();
    RenderTreeNode node;
    node.leaf = (flags & 1) != 0;
    if (node.leaf) {
        const uint32_t count = r.u32();
        r.u32();
        for (float& v : node.low) v = r.f32();
        for (float& v : node.high) v = r.f32();
        for (uint32_t& v : node.regions) v = r.u32();
        if (!r.fits(count, 0xCC)) return false;
        node.instances.resize(count);
        for (uint32_t& i : node.instances) {
            i = r.u32();
            r.skip(0xC8);
        }
        out[at] = std::move(node);
        return r.ok();
    }
    for (int i = 0; i < 4; ++i) r.u32();
    node.splitX = r.f32();
    node.splitZ = r.f32();
    for (float& v : node.low) v = r.f32();
    for (float& v : node.high) v = r.f32();
    for (uint32_t& v : node.regions) v = r.u32();
    if (!r.ok()) return false;
    for (uint32_t c = 0; c < 4; ++c) {
        node.children[c] = static_cast<uint32_t>(out.size());
        if (!readTreeNode(r, out, depth + 1)) return false;
    }
    out[at] = std::move(node);
    return true;
}

}  // namespace

std::optional<std::vector<RenderTreeNode>> parseRenderTree(const std::vector<uint8_t>& tree) {
    ByteReader r(tree);
    std::vector<RenderTreeNode> nodes;
    if (tree.empty() || !readTreeNode(r, nodes, 0) || !r.atEnd()) return std::nullopt;
    return nodes;
}

std::vector<std::byte> textureFile(const TextureEntry& entry, const std::vector<uint8_t>& pixels) {
    std::vector<std::byte> file(entry.header.size());
    std::memcpy(file.data(), entry.header.data(), entry.header.size());
    if (entry.offset < pixels.size()) {
        const size_t n = std::min<size_t>(entry.dataSize, pixels.size() - entry.offset);
        file.resize(entry.header.size() + n);
        std::memcpy(file.data() + entry.header.size(), pixels.data() + entry.offset, n);
    }
    return file;
}

uint32_t Ocl::region(float x, float y, float z) const {
    if (size[0] == 0) return 0;
    // A value below the box (or not a number) takes its low side, above it the high side.
    auto inside = [](float v, float low, float high) {
        if (!(low <= v)) return low;
        return v <= high ? v : high;
    };
    const float cx = inside(x, min[0], max[0]);
    const float cy = inside(y, min[1], max[1]);
    const float cz = inside(z, min[2], max[2]);
    // The cell: n x (v - low) / (high - low) cut toward zero; the high side is the last cell.
    auto cell = [](uint32_t n, float v, float low, float high) {
        const float at = (static_cast<float>(n) * (v - low)) / (high - low);
        auto i = static_cast<int64_t>(at);
        if (i == static_cast<int64_t>(n)) --i;
        return i;
    };
    const int64_t ix = cell(size[0], cx, min[0], max[0]);
    const int64_t iy = cell(size[1], cy, min[1], max[1]);
    const int64_t iz = cell(size[2], cz, min[2], max[2]);
    const int64_t at = (static_cast<int64_t>(size[1]) * iz + iy) * size[0] + ix;
    return at >= 0 && static_cast<uint64_t>(at) < grid.size() ? grid[static_cast<size_t>(at)] : 0;
}

std::optional<Ocl> parseOcl(const std::vector<uint8_t>& data) {
    ByteReader r(data);
    Ocl o;
    for (uint32_t& n : o.size) n = r.u32();
    for (float& v : o.min) v = r.f32();
    for (float& v : o.max) v = r.f32();
    const uint64_t cells = static_cast<uint64_t>(o.size[0]) * o.size[1] * o.size[2];
    if (!r.ok() || cells > r.left()) return std::nullopt;
    o.grid.resize(static_cast<size_t>(cells));
    r.bytes(o.grid.data(), o.grid.size());
    const uint32_t count = r.u32();
    if (!r.fits(count, 32)) return std::nullopt;
    o.records.resize(count);
    for (auto& record : o.records) {
        for (uint32_t& v : record) v = r.u32();
    }
    if (!r.ok() || !r.atEnd()) return std::nullopt;
    return o;
}

std::optional<std::vector<IntroShot>> parseIntroCam(const std::vector<uint8_t>& data) {
    ByteReader r(data);
    const uint32_t shots = r.u32();
    if (!r.fits(shots, 8)) return std::nullopt;
    std::vector<IntroShot> out(shots);
    for (IntroShot& shot : out) {
        shot.wait = r.f32();
        const uint32_t keys = r.u32();
        if (!r.fits(keys, 56)) return std::nullopt;
        shot.keys.resize(keys);
        for (IntroKey& k : shot.keys) {
            k.time = r.f32();
            k.fov = r.f32();
            k.position[3] = r.f32();
            k.position[0] = r.f32();
            k.position[1] = r.f32();
            k.position[2] = r.f32();
            for (float& v : k.rotation) v = r.f32();
            r.u32();
            k.pitch = r.f32();
            k.yaw = r.f32();
            r.u32();
        }
    }
    if (!r.ok() || !r.atEnd()) return std::nullopt;
    return out;
}

Spl parseSpl(const std::string& text) {
    Spl spl;
    std::vector<SplPoint>* line = nullptr;
    for (const ScriptLine& l : parseScript(text)) {
        const std::string& k = l.keyword;
        if (_stricmp(k.c_str(), "GRIDPOINT") == 0) {
            const Spl::GridPoint p{l.real(0), l.real(1), l.real(2), l.real(3)};
            if (spl.grid.size() < 6) {
                spl.grid.push_back(p);
            } else {
                // Stock problem: a seventh point's x is written over the count that follows the six slots (+0xAC);
                // its y, z and heading go to the slot of that new count, x's bits x 16 bytes on, and the count
                // becomes x's bits + 1 (0x004A1447 .. 0x004A14D7). An x of bits 0 .. 5 (0 and the smallest
                // denormals) lands in a slot of the six, which keeps its x; any other writes far past them (the
                // game stops) or leaves a count past the slots: dropped here.
                uint32_t bits;
                std::memcpy(&bits, &p.x, 4);
                if (bits < 6) {
                    spl.grid.resize(bits + 1);
                    Spl::GridPoint& slot = spl.grid[bits];
                    slot.y = p.y;
                    slot.z = p.z;
                    slot.heading = p.heading;
                }
            }
        } else if (_stricmp(k.c_str(), "ROUTETYPE") == 0) {
            if (_stricmp(l.word(0).c_str(), "POINT2POINT") == 0) {
                spl.pointToPoint = true;
                spl.lastPoint = l.integer(1);
            }
        } else if (_stricmp(k.c_str(), "MINCORNERSPEED") == 0) {
            spl.minCornerSpeed = l.real(0);
        } else if (_stricmp(k.c_str(), "BEGINSPLINE") == 0) {
            const std::string& name = l.word(0);
            line = _stricmp(name.c_str(), "FAST") == 0     ? &spl.fast
                   : _stricmp(name.c_str(), "ATTACK") == 0 ? &spl.attack
                   : _stricmp(name.c_str(), "CENTRE") == 0 ? &spl.centre
                                                           : nullptr;
        } else if (_stricmp(k.c_str(), "POINT") == 0) {
            if (line != nullptr) line->push_back({l.real(0), l.real(1), l.real(2)});
        } else if (_stricmp(k.c_str(), "ENDSPLINE") == 0) {
            line = nullptr;
        }
    }
    return spl;
}

}  // namespace oj
