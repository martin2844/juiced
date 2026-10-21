#pragma once
// Reading and writing a field of a stock-layout object by its offset, as the stock code does when it indexes a
// table past its end (a stock problem: the following fields are read or written). Offsets past the object (the next
// heap block in the stock game) read 0 and are not written.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace oj::raw {

template <class T, class Object>
T get(const Object& object, size_t offset) {
    T value{};
    if (offset + sizeof(T) <= sizeof(Object)) {
        std::memcpy(&value, reinterpret_cast<const uint8_t*>(&object) + offset, sizeof(T));
    }
    return value;
}

template <class Object, class T>
void put(Object& object, size_t offset, const T& value) {
    if (offset + sizeof(T) <= sizeof(Object)) {
        std::memcpy(reinterpret_cast<uint8_t*>(&object) + offset, &value, sizeof(T));
    }
}

// strcpy of a text into a field: a longer text runs into the next fields.
template <class Object>
void putText(Object& object, size_t offset, const std::string& text) {
    for (size_t i = 0; i <= text.size(); ++i) {
        put(object, offset + i, i < text.size() ? text[i] : '\0');
    }
}

}  // namespace oj::raw
