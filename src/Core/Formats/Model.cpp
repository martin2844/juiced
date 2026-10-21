#include "Core/Formats/Model.h"

#include <cstring>

namespace oj {

namespace {

class Reader {
public:
    explicit Reader(const std::vector<uint8_t>& data) : m_data(data) {}

    bool ok() const { return m_ok; }
    bool atEnd() const { return m_offset == m_data.size(); }
    size_t left() const { return m_data.size() - m_offset; }

    void bytes(void* out, size_t n) {
        if (!m_ok || n > left()) {
            m_ok = false;
            return;
        }
        std::memcpy(out, m_data.data() + m_offset, n);
        m_offset += n;
    }
    uint32_t u32() {
        uint32_t v = 0;
        bytes(&v, 4);
        return v;
    }
    float f32() {
        float v = 0.0f;
        bytes(&v, 4);
        return v;
    }
    std::string name(size_t size) {
        std::string s(size, '\0');
        bytes(s.data(), size);
        s.resize(strnlen(s.data(), size));
        return s;
    }

private:
    const std::vector<uint8_t>& m_data;
    size_t m_offset = 0;
    bool m_ok = true;
};

bool readNode(Reader& r, BohNode& node, int depth) {
    if (depth > 64) return false;
    node.name = r.name(32);
    for (float& v : node.matrix) v = r.f32();
    node.attach = r.u32();
    if (node.attach == 0) {
        node.mesh = r.name(32);
    } else if (node.attach == 2) {
        const uint32_t length = r.u32();
        if (!r.ok() || length > r.left()) return false;
        node.light.resize(length);
        r.bytes(node.light.data(), length);
    } else {
        return false;
    }
    const uint32_t children = r.u32();
    if (!r.ok() || children > 4096) return false;
    node.children.resize(children);
    for (BohNode& child : node.children) {
        if (!readNode(r, child, depth + 1)) return false;
    }
    return r.ok();
}

void readBounds(Reader& r, MeshBounds& b) {
    for (float& v : b.centre) v = r.f32();
    b.w = r.f32();
    b.radius = r.f32();
    for (float& v : b.min) v = r.f32();
    for (float& v : b.max) v = r.f32();
}

std::string trim(std::string_view s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return std::string(s.substr(a, b - a));
}

}  // namespace

std::optional<std::vector<BohNode>> parseBoh(const std::vector<uint8_t>& data) {
    Reader r(data);
    std::vector<BohNode> nodes;
    while (!r.atEnd()) {
        BohNode node;
        if (!readNode(r, node, 0)) return std::nullopt;
        nodes.push_back(std::move(node));
    }
    return nodes;
}

std::optional<Mesh> parseMesh(const std::vector<uint8_t>& data) {
    Reader r(data);
    Mesh mesh;
    const uint32_t indexCount = r.u32();
    if (!r.ok() || indexCount > r.left() / 2) return std::nullopt;
    mesh.indices.resize(indexCount);
    r.bytes(mesh.indices.data(), indexCount * 2);
    const uint32_t partCount = r.u32();
    if (!r.ok() || partCount > 65536) return std::nullopt;
    mesh.parts.resize(partCount);
    for (MeshPart& p : mesh.parts) {
        p.name = r.name(32);
        p.indexStart = r.u32();
        p.indexCount = r.u32();
        p.faceType = r.u32();
        readBounds(r, p.bounds);
        p.lodIn = r.f32();
        p.lodOut = r.f32();
        p.renderFlag = r.u32();
        const uint32_t declCount = r.u32();
        if (!r.ok() || declCount > 64) return std::nullopt;
        p.declaration.resize(declCount);
        for (VertexElement& e : p.declaration) {
            r.bytes(&e.stream, 2);
            r.bytes(&e.offset, 2);
            r.bytes(&e.type, 1);
            r.bytes(&e.method, 1);
            r.bytes(&e.usage, 1);
            r.bytes(&e.usageIndex, 1);
        }
        p.stride = r.u32();
        p.vertexCount = r.u32();
        if (!r.ok() || (p.stride != 0 && p.vertexCount > r.left() / p.stride)) return std::nullopt;
        p.vertices.resize(static_cast<size_t>(p.stride) * p.vertexCount);
        r.bytes(p.vertices.data(), p.vertices.size());
        if (!r.ok()) return std::nullopt;
    }
    readBounds(r, mesh.bounds);
    if (!r.ok() || !r.atEnd()) return std::nullopt;
    return mesh;
}

const std::string* Material::find(std::string_view key) const {
    for (const auto& [k, v] : values) {
        if (k == key) return &v;
    }
    return nullptr;
}

Material parseMaterial(std::string_view text) {
    Material m;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        const std::string_view line = text.substr(start, end - start);
        start = end + 1;
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key == "Material Program") {
            m.className = value;
        } else if (!key.empty()) {
            m.values.emplace_back(key, value);
        }
    }
    return m;
}

std::string textureName(std::string_view value) {
    const size_t slash = value.find_last_of("\\/");
    std::string_view name = slash == std::string_view::npos ? value : value.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string_view::npos) name = name.substr(0, dot);
    return std::string(name);
}

}  // namespace oj
