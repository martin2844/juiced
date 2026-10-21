#pragma once
// A zlib stream (RFC 1950: the two header bytes, the deflate data of RFC 1951, the Adler-32 sum) inflated: the
// game's .dat archives keep their files as zlib streams (docs/spec/27-archives.md).

#include <cstddef>
#include <cstdint>
#include <vector>

namespace oj {

// The bytes of the stream added to `out`; false for a broken stream (a bad header, code or distance, a stream cut
// short or a sum that does not match), `out` then holding what was made before it.
bool inflateZlib(const uint8_t* data, size_t size, std::vector<std::byte>& out);

}  // namespace oj
