#include "Core/Base/Random.h"

#include <bit>

namespace oj {

CrtRandom& gameRandom() {
    static CrtRandom random;
    return random;
}

uint32_t& seedCount() {
    static uint32_t count = 0;
    return count;
}

void LaggedRandom::seed(uint32_t value) {
    uint32_t c = value;
    for (uint32_t& w : words) {
        c = 1u - c * 0x53A9B4FBu;
        w = c;
    }
    first = 0;
    second = 10;
    next();
}

float LaggedRandom::next() {
    const int32_t i = first;
    const int32_t j = second;
    const uint32_t x = std::rotl(words[j], 13) + std::rotl(words[i], 9);
    words[i] = x;
    first = i - 1 < 0 ? 16 : i - 1;
    second = j - 1 < 0 ? 16 : j - 1;
    return std::bit_cast<float>((x >> 9) | 0x3F800000u) - 1.0f;
}

LaggedRandom& laggedRandom() {
    static LaggedRandom random;
    return random;
}

}  // namespace oj
