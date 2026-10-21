// Unit tests: Voice.

#include "Framework/Test.h"

#include "Core/Network/GameSpy/GameSpyVoice.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace {

using oj::test::check;

}  // namespace

// GameSpy's Speex codec (0x00580230 ..): quality 4 of the narrowband, 20 bytes a frame of 160 samples; made
// once only; a tone encoded, then decoded and added into a frame once and twice.
OJ_UNIT_TEST(Voice, SpeexCodec) {
    namespace gv = oj::net::gv;
    const bool codecMade = gv::speexInitialize(4);
    const bool codecAgain = gv::speexInitialize(4);
    const int32_t size = gv::speexEncodedFrameSize();
    std::array<int16_t, 160> tone{};
    for (size_t i = 0; i < tone.size(); ++i) {
        tone[i] = static_cast<int16_t>(8000.0 * std::sin(static_cast<double>(i) * 2.0 * 3.14159265358979 * 440.0 / 8000.0));
    }
    std::array<uint8_t, 20> frame{};
    gv::speexEncode(frame.data(), tone.data());
    void* decoder = nullptr;
    void* decoder2 = nullptr;
    const bool newDecoders = gv::speexNewDecoder(&decoder) && gv::speexNewDecoder(&decoder2);
    std::array<int16_t, 160> once{};
    std::array<int16_t, 160> twice{};
    gv::speexDecodeAdd(once.data(), frame.data(), decoder);
    gv::speexDecodeAdd(twice.data(), frame.data(), decoder2);
    gv::speexDecodeAdd(twice.data(), frame.data(), decoder2);
    int32_t energy = 0;
    for (int16_t v : once) energy = std::max<int32_t>(energy, std::abs(v));
    gv::speexFreeDecoder(decoder);
    gv::speexFreeDecoder(decoder2);
    gv::speexCleanup();
    check(codecMade && !codecAgain && size == 20 && newDecoders && energy > 0 && twice != once,
          "voice: the Speex codec at quality 4 (20 bytes a frame), its decode added into the frame");
}
