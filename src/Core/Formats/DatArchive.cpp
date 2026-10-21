#include "Core/Formats/DatArchive.h"

#include "Core/Formats/Inflate.h"

#include <fstream>
#include <string>

namespace oj {

namespace {

std::string archiveName(std::string_view name) {
    std::string s(name);
    for (char& c : s) {
        if (c == '/') c = '\\';
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

uint32_t u32(const uint8_t* p) {
    return uint32_t{p[0]} | (uint32_t{p[1]} << 8) | (uint32_t{p[2]} << 16) | (uint32_t{p[3]} << 24);
}

bool readAt(std::ifstream& in, uint64_t at, void* out, size_t n) {
    in.seekg(static_cast<std::streamoff>(at));
    return static_cast<bool>(in.read(static_cast<char*>(out), static_cast<std::streamsize>(n)));
}

}  // namespace

uint32_t datHash(std::string_view name) {
    const size_t n = name.size();
    if (n == 0) return 0;
    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < n; ++i) {
        const auto ci = static_cast<uint32_t>(static_cast<int32_t>(static_cast<signed char>(name[i])));
        for (size_t j = i; j < n; ++j) {
            const auto cj = static_cast<uint32_t>(static_cast<int32_t>(static_cast<signed char>(name[j])));
            sum += (cj + static_cast<uint32_t>(j)) * (cj + 0x13u) * (ci + 7u) * (ci + static_cast<uint32_t>(i));
        }
    }
    return sum % 4000000000u;
}

std::optional<DatArchive> DatArchive::open(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in) return std::nullopt;
    const auto length = static_cast<uint64_t>(in.tellg());
    uint8_t head[8];
    if (length < 8 || !readAt(in, 0, head, 8)) return std::nullopt;
    const uint32_t count = u32(head);
    const uint32_t table = u32(head + 4);
    if (uint64_t{table} + uint64_t{count} * 20 > length) return std::nullopt;
    std::vector<uint8_t> bytes(size_t{count} * 20);
    if (count != 0 && !readAt(in, table, bytes.data(), bytes.size())) return std::nullopt;
    DatArchive a;
    a.m_file = file;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* e = bytes.data() + size_t{i} * 20;
        a.m_entries[u32(e)] = {u32(e + 4), u32(e + 8), u32(e + 12), u32(e + 16)};
    }
    return a;
}

const DatArchive::Entry* DatArchive::find(std::string_view name) const {
    const auto it = m_entries.find(datHash(archiveName(name)));
    return it != m_entries.end() ? &it->second : nullptr;
}

bool DatArchive::contains(std::string_view name) const {
    return find(name) != nullptr;
}

std::optional<std::vector<std::byte>> DatArchive::read(std::string_view name) const {
    const Entry* e = find(name);
    if (e == nullptr) return std::nullopt;
    std::ifstream in(m_file, std::ios::binary);
    if (!in) return std::nullopt;
    std::vector<uint8_t> list(size_t{e->chunks} * 8);
    if (e->chunks != 0 && !readAt(in, e->offset, list.data(), list.size())) return std::nullopt;
    std::vector<std::byte> out;
    out.reserve(e->size);
    std::vector<uint8_t> packed;
    for (uint32_t k = 0; k < e->chunks; ++k) {
        const uint32_t at = u32(list.data() + size_t{k} * 8);
        const uint32_t n = u32(list.data() + size_t{k} * 8 + 4);
        packed.resize(n);
        if (n != 0 && !readAt(in, at, packed.data(), n)) return std::nullopt;
        if (!inflateZlib(packed.data(), packed.size(), out)) return std::nullopt;
    }
    if (out.size() != e->size) return std::nullopt;
    return out;
}

}  // namespace oj
