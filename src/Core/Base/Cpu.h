#pragma once
// Processor features as the stock game classifies them, and its floating point mode.

#include <cstdint>

namespace oj::cpu {

enum Feature : uint32_t {
    MMX = 0x1,
    SSE = 0x2,
    SSE2 = 0x4,
    Now3D = 0x8,  // 3DNow!
    SSE3 = 0x10,
};

// CPUID leaf 1 (EDX bits 23, 25, 26; ECX bit 0), leaf 0x80000001 EDX bit 31.
uint32_t detectFeatures();

// The stock game sets x87 RC = chop and MXCSR.RC = toward zero at start when the processor has SSE. Direct3D 9 sets
// the x87 control word of the main thread to single precision and round to nearest when it makes the device, so the
// game's arithmetic rounds to nearest; only its SSE conversions (CVTSS2SI, on the paths the SSE flag picks) truncate.
// OpenJuice keeps round to nearest and truncates where it repeats those conversions.

}  // namespace oj::cpu
