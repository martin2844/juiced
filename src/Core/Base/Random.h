#pragma once
// The C runtime's rand() of the stock game's compiler: a linear congruential generator, state * 214013 + 2531011,
// values (state >> 16) & 0x7FFF; the state starts at 1.

#include <cstdint>

namespace oj {

class CrtRandom {
public:
    static constexpr int kMax = 0x7FFF;

    void seed(uint32_t value) { m_state = value; }
    int next() {
        ++m_draws;
        m_state = m_state * 214013u + 2531011u;
        return static_cast<int>((m_state >> 16) & 0x7FFF);
    }
    // The values drawn so far (to compare with a trace of the stock game's rand()).
    uint64_t draws() const { return m_draws; }

private:
    uint32_t m_state = 1;
    uint64_t m_draws = 0;
};

// The generator every part of the game draws from.
CrtRandom& gameRandom();
// [0x0079E898]: the count rand() is seeded with in several places, one for the whole game: the transport's random
// bytes and the grid's shuffle, the race director's cuts, the matchmaking's session ids and cookies.
uint32_t& seedCount();

// 0x00509B70: a lagged generator of 17 words (0x0075B2D8, its two places 0x0075B31C and 0x0075B320), all zero until
// it is seeded (0x00509BE0), a float of 0 .. 1 from the high 23 bits of each sum. One for the whole game: the
// particles draw from it and every dent's vertex seeds it anew (0x005556C0).
struct LaggedRandom {
    uint32_t words[17] = {};
    int32_t first = 0;
    int32_t second = 0;

    void seed(uint32_t value);
    float next();
};

LaggedRandom& laggedRandom();

}  // namespace oj
