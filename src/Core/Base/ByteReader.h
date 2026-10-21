#pragma once
// Little-endian reading of a file's bytes; a read past the end marks the reader bad and gives zeros.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace oj {

class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}
    explicit ByteReader(const std::vector<uint8_t>& data) : m_data(data.data()), m_size(data.size()) {}

    bool ok() const { return m_ok; }
    bool atEnd() const { return m_offset == m_size; }
    size_t left() const { return m_ok ? m_size - m_offset : 0; }
    size_t offset() const { return m_offset; }
    const uint8_t* here() const { return m_data + m_offset; }

    void bytes(void* out, size_t n) {
        if (!m_ok || n > m_size - m_offset) {
            m_ok = false;
            std::memset(out, 0, n);
            return;
        }
        std::memcpy(out, m_data + m_offset, n);
        m_offset += n;
    }
    void skip(size_t n) {
        if (!m_ok || n > m_size - m_offset) {
            m_ok = false;
            return;
        }
        m_offset += n;
    }
    uint8_t u8() {
        uint8_t v = 0;
        bytes(&v, 1);
        return v;
    }
    uint16_t u16() {
        uint16_t v = 0;
        bytes(&v, 2);
        return v;
    }
    uint32_t u32() {
        uint32_t v = 0;
        bytes(&v, 4);
        return v;
    }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    float f32() {
        float v = 0.0f;
        bytes(&v, 4);
        return v;
    }
    // A NUL-padded name of `size` bytes.
    std::string name(size_t size) {
        std::string s(size, '\0');
        bytes(s.data(), size);
        s.resize(strnlen(s.data(), size));
        return s;
    }
    // `count` items of `itemSize` bytes may follow.
    bool fits(uint64_t count, uint64_t itemSize) const { return m_ok && count * itemSize <= m_size - m_offset; }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_offset = 0;
    bool m_ok = true;
};

}  // namespace oj
