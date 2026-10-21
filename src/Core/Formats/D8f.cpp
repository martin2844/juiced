#include "Core/Formats/D8f.h"

#include <cstring>

namespace oj {

namespace {

class Reader {
public:
    explicit Reader(const std::vector<std::byte>& data) : m_data(data) {}
    bool has(size_t bytes) const { return m_offset + bytes <= m_data.size(); }
    template <class T>
    T read() {
        T value{};
        std::memcpy(&value, m_data.data() + m_offset, sizeof(T));
        m_offset += sizeof(T);
        return value;
    }
    const uint8_t* take(size_t bytes) {
        const auto* p = reinterpret_cast<const uint8_t*>(m_data.data()) + m_offset;
        m_offset += bytes;
        return p;
    }

private:
    const std::vector<std::byte>& m_data;
    size_t m_offset = 0;
};

}  // namespace

bool D8f::parse(const std::vector<std::byte>& file) {
    Reader r(file);
    if (!r.has(1024 + 12)) {
        return false;
    }
    for (uint32_t& colour : palette) {
        const uint8_t* bgra = r.take(4);
        colour = (uint32_t(bgra[3]) << 24) | (uint32_t(bgra[2]) << 16) | (uint32_t(bgra[1]) << 8) | bgra[0];
    }
    const auto count = r.read<uint32_t>();
    r.read<uint32_t>();
    spaceAdvance = r.read<uint32_t>();
    if (!r.has(size_t(count) * 48)) {
        return false;
    }
    glyphs.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        Glyph& g = glyphs[i];
        g.code = r.read<uint16_t>();
        r.read<uint16_t>();
        g.width = r.read<uint32_t>();
        g.height = r.read<uint32_t>();
        g.xOffset = r.read<int32_t>();
        g.yOffset = r.read<int32_t>();
        g.advance = r.read<uint32_t>();
        r.read<uint32_t>();
        g.u0 = r.read<float>();
        g.v0 = r.read<float>();
        g.u1 = r.read<float>();
        g.v1 = r.read<float>();
        g.page = r.read<uint32_t>();
        byCode[g.code] = i;
    }
    if (!r.has(4)) {
        return false;
    }
    const auto pageCount = r.read<uint32_t>();
    pages.resize(pageCount);
    for (Page& p : pages) {
        if (!r.has(8)) {
            return false;
        }
        p.width = r.read<uint32_t>();
        p.height = r.read<uint32_t>();
        const size_t bytes = size_t(p.width) * p.height;
        if (!r.has(bytes)) {
            return false;
        }
        const uint8_t* src = r.take(bytes);
        p.indices.assign(src, src + bytes);
    }
    return true;
}

const D8f::Glyph* D8f::find(wchar_t c) const {
    auto it = byCode.find(static_cast<uint16_t>(c));
    if (it == byCode.end()) {
        it = byCode.find(0xFFFF);
    }
    return it == byCode.end() ? nullptr : &glyphs[it->second];
}

}  // namespace oj
