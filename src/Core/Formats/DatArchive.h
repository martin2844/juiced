#pragma once
// A .dat archive of the game (the class of 0x004FCEA0, vtable 0x006F5F18 in 1.0; docs/spec/27-archives.md): a folder
// packed into one file. {count, the table's offset}; the table count entries of 20 bytes {the name's hash, the
// offset of its chunks, their count, the packed size, the size}; at the offset the chunks {offset, size}, each a
// zlib stream.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oj {

// 0x004FCF80: the hash of a name in the archive (its path inside, '\' between the folders, lower case): the sum over
// i < n - 1 and j >= i of (c[j] + j)(c[j] + 19)(c[i] + 7)(c[i] + i) in 32 bits (the bytes signed), mod 4000000000;
// 0 for an empty name.
uint32_t datHash(std::string_view name);

class DatArchive {
public:
    // The table of the file read; none for a file that is not an archive.
    static std::optional<DatArchive> open(const std::filesystem::path& file);

    // `name` as the game asks it (any case, '/' or '\').
    bool contains(std::string_view name) const;
    // The file inflated; none when it is not in the archive or does not inflate to its size.
    std::optional<std::vector<std::byte>> read(std::string_view name) const;

    size_t size() const { return m_entries.size(); }

private:
    struct Entry {
        uint32_t offset = 0;
        uint32_t chunks = 0;
        uint32_t packed = 0;
        uint32_t size = 0;
    };
    const Entry* find(std::string_view name) const;

    std::filesystem::path m_file;
    std::unordered_map<uint32_t, Entry> m_entries;
};

}  // namespace oj
