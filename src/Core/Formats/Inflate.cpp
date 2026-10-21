#include "Core/Formats/Inflate.h"

#include <array>

namespace oj {

namespace {

constexpr int kMaxBits = 15;

// The bits of the deflate data, least significant first.
class Bits {
public:
    Bits(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

    // n bits (n <= 16) as a number, the first the lowest; false past the end.
    bool get(int n, uint32_t& v) {
        while (m_count < n) {
            if (m_at >= m_size) return false;
            m_buffer |= static_cast<uint32_t>(m_data[m_at++]) << m_count;
            m_count += 8;
        }
        v = m_buffer & ((1u << n) - 1u);
        m_buffer >>= n;
        m_count -= n;
        return true;
    }
    // To the next byte (a stored block starts there).
    void align() {
        m_buffer = 0;
        m_count = 0;
    }
    size_t at() const { return m_at; }
    void skip(size_t n) { m_at += n; }
    const uint8_t* data() const { return m_data; }
    size_t size() const { return m_size; }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_at = 0;
    uint32_t m_buffer = 0;
    int m_count = 0;
};

// A canonical Huffman code: the count of codes of each length and the symbols in the order of their codes.
struct Huffman {
    std::array<uint16_t, kMaxBits + 1> counts{};
    std::array<uint16_t, 288> symbols{};

    // False for a set of lengths that is over-subscribed (an incomplete one is allowed, as zlib allows a single code).
    bool build(const uint8_t* lengths, int n) {
        counts.fill(0);
        for (int i = 0; i < n; ++i) ++counts[lengths[i]];
        if (counts[0] == n) return true;
        int left = 1;
        for (int len = 1; len <= kMaxBits; ++len) {
            left = left * 2 - counts[static_cast<size_t>(len)];
            if (left < 0) return false;
        }
        std::array<uint16_t, kMaxBits + 1> offsets{};
        for (int len = 1; len < kMaxBits; ++len) {
            offsets[static_cast<size_t>(len + 1)] =
                static_cast<uint16_t>(offsets[static_cast<size_t>(len)] + counts[static_cast<size_t>(len)]);
        }
        for (int i = 0; i < n; ++i) {
            if (lengths[i] != 0) symbols[offsets[lengths[i]]++] = static_cast<uint16_t>(i);
        }
        return true;
    }

    // The next symbol, bit by bit; -1 for a code that is not in the set or the data's end.
    int decode(Bits& b) const {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len <= kMaxBits; ++len) {
            uint32_t bit = 0;
            if (!b.get(1, bit)) return -1;
            code |= static_cast<int>(bit);
            const int count = counts[static_cast<size_t>(len)];
            if (code - count < first) return symbols[static_cast<size_t>(index + (code - first))];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }
};

constexpr std::array<uint16_t, 29> kLengthBase = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                                  31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<uint8_t, 29> kLengthExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                  2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<uint16_t, 30> kDistanceBase = {1,    2,    3,    4,    5,    7,     9,     13,    17,  25,
                                                    33,   49,   65,   97,   129,  193,   257,   385,   513, 769,
                                                    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<uint8_t, 30> kDistanceExtra = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

// The symbols of a block by its two codes, into out (whose bytes the distances reach back into).
bool codes(Bits& b, const Huffman& lengths, const Huffman& distances, std::vector<std::byte>& out, size_t start) {
    for (;;) {
        const int symbol = lengths.decode(b);
        if (symbol < 0) return false;
        if (symbol < 256) {
            out.push_back(static_cast<std::byte>(symbol));
            continue;
        }
        if (symbol == 256) return true;
        const int l = symbol - 257;
        if (l >= 29) return false;
        uint32_t extra = 0;
        if (!b.get(kLengthExtra[static_cast<size_t>(l)], extra)) return false;
        const size_t length = kLengthBase[static_cast<size_t>(l)] + extra;
        const int d = distances.decode(b);
        if (d < 0 || d >= 30) return false;
        if (!b.get(kDistanceExtra[static_cast<size_t>(d)], extra)) return false;
        const size_t distance = kDistanceBase[static_cast<size_t>(d)] + extra;
        if (distance > out.size() - start) return false;
        const size_t from = out.size() - distance;
        for (size_t i = 0; i < length; ++i) out.push_back(out[from + i]);
    }
}

bool fixedBlock(Bits& b, std::vector<std::byte>& out, size_t start) {
    static const auto tables = [] {
        std::array<uint8_t, 288> l{};
        for (int i = 0; i < 144; ++i) l[static_cast<size_t>(i)] = 8;
        for (int i = 144; i < 256; ++i) l[static_cast<size_t>(i)] = 9;
        for (int i = 256; i < 280; ++i) l[static_cast<size_t>(i)] = 7;
        for (int i = 280; i < 288; ++i) l[static_cast<size_t>(i)] = 8;
        std::array<uint8_t, 30> d{};
        d.fill(5);
        std::pair<Huffman, Huffman> t;
        t.first.build(l.data(), 288);
        t.second.build(d.data(), 30);
        return t;
    }();
    return codes(b, tables.first, tables.second, out, start);
}

bool dynamicBlock(Bits& b, std::vector<std::byte>& out, size_t start) {
    uint32_t nlen = 0, ndist = 0, ncode = 0;
    if (!b.get(5, nlen) || !b.get(5, ndist) || !b.get(4, ncode)) return false;
    nlen += 257;
    ndist += 1;
    ncode += 4;
    if (nlen > 286 || ndist > 30) return false;
    static constexpr std::array<uint8_t, 19> kOrder = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    std::array<uint8_t, 320> lengths{};
    for (uint32_t i = 0; i < ncode; ++i) {
        uint32_t v = 0;
        if (!b.get(3, v)) return false;
        lengths[kOrder[i]] = static_cast<uint8_t>(v);
    }
    Huffman lencode;
    if (!lencode.build(lengths.data(), 19)) return false;
    uint32_t i = 0;
    while (i < nlen + ndist) {
        const int symbol = lencode.decode(b);
        if (symbol < 0) return false;
        if (symbol < 16) {
            lengths[i++] = static_cast<uint8_t>(symbol);
            continue;
        }
        uint8_t value = 0;
        uint32_t repeat = 0;
        if (symbol == 16) {
            if (i == 0) return false;
            value = lengths[i - 1];
            if (!b.get(2, repeat)) return false;
            repeat += 3;
        } else if (symbol == 17) {
            if (!b.get(3, repeat)) return false;
            repeat += 3;
        } else {
            if (!b.get(7, repeat)) return false;
            repeat += 11;
        }
        if (i + repeat > nlen + ndist) return false;
        while (repeat-- > 0) lengths[i++] = value;
    }
    if (lengths[256] == 0) return false;
    Huffman lens, dists;
    if (!lens.build(lengths.data(), static_cast<int>(nlen))) return false;
    if (!dists.build(lengths.data() + nlen, static_cast<int>(ndist))) return false;
    return codes(b, lens, dists, out, start);
}

}  // namespace

bool inflateZlib(const uint8_t* data, size_t size, std::vector<std::byte>& out) {
    if (size < 6) return false;
    const unsigned cmf = data[0];
    const unsigned flg = data[1];
    if ((cmf & 0x0F) != 8 || (cmf >> 4) > 7 || ((cmf << 8) | flg) % 31 != 0 || (flg & 0x20) != 0) return false;
    const size_t start = out.size();
    Bits b(data + 2, size - 2);
    for (;;) {
        uint32_t last = 0, type = 0;
        if (!b.get(1, last) || !b.get(2, type)) return false;
        if (type == 0) {
            b.align();
            if (b.at() + 4 > b.size()) return false;
            const uint8_t* p = b.data() + b.at();
            const unsigned len = p[0] | (p[1] << 8);
            const unsigned nlen = p[2] | (p[3] << 8);
            if ((len ^ 0xFFFFu) != nlen || b.at() + 4 + len > b.size()) return false;
            for (unsigned k = 0; k < len; ++k) out.push_back(static_cast<std::byte>(p[4 + k]));
            b.skip(4 + len);
        } else if (type == 1) {
            if (!fixedBlock(b, out, start)) return false;
        } else if (type == 2) {
            if (!dynamicBlock(b, out, start)) return false;
        } else {
            return false;
        }
        if (last != 0) break;
    }
    // The Adler-32 sum of what was made, big-endian after the data's last byte.
    b.align();
    if (b.at() + 4 > b.size()) return false;
    const uint8_t* p = b.data() + b.at();
    const uint32_t want = (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
    uint32_t s1 = 1, s2 = 0;
    for (size_t k = start; k < out.size(); ++k) {
        s1 = (s1 + static_cast<uint8_t>(out[k])) % 65521u;
        s2 = (s2 + s1) % 65521u;
    }
    return ((s2 << 16) | s1) == want;
}

}  // namespace oj
