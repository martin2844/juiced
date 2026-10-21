#include "Core/Base/Cpu.h"

#include <intrin.h>

namespace oj::cpu {

uint32_t detectFeatures() {
    int regs[4] = {};
    __cpuid(regs, 0);
    if (regs[0] < 1) {
        return 0;
    }
    __cpuid(regs, 1);
    const auto ecx = static_cast<uint32_t>(regs[2]);
    const auto edx = static_cast<uint32_t>(regs[3]);
    uint32_t features = 0;
    if (edx & (1u << 23)) features |= MMX;
    if (edx & (1u << 25)) features |= SSE;
    if (edx & (1u << 26)) features |= SSE2;
    if (ecx & 1u) features |= SSE3;
    __cpuid(regs, static_cast<int>(0x80000000u));
    if (static_cast<uint32_t>(regs[0]) >= 0x80000001u) {
        __cpuid(regs, static_cast<int>(0x80000001u));
        if (static_cast<uint32_t>(regs[3]) & 0x80000000u) features |= Now3D;
    }
    return features;
}

}  // namespace oj::cpu
