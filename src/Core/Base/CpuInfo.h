#pragma once
// The engine's processor query (0x005A1F40), which nothing in the PC game calls.

#include <array>
#include <cstdint>

namespace oj {

// What the query fills (0x44 bytes).
struct CpuInfo {
    std::array<char, 13> vendor{};  // +0x00: CPUID 0's vendor string
    std::array<char, 31> name{};    // +0x0D: the processor's name (0x005A1A60)
    uint32_t family = 0;            // +0x2C
    uint32_t model = 0;             // +0x30
    uint32_t stepping = 0;          // +0x34
    uint32_t found = 0;             // +0x38: the features CPUID reports
    uint32_t usable = 0;            // +0x3C: those the system runs
    uint32_t mask = 0;              // +0x40: 0x1F
};

// The features' bits.
constexpr uint32_t kCpuMmx = 1;
constexpr uint32_t kCpuSse = 2;
constexpr uint32_t kCpuSse2 = 4;
constexpr uint32_t kCpu3dNow = 8;
constexpr uint32_t kCpuSse3 = 0x10;

// 0x005A1F40: with CPUID (0x005A1880) the features found: MMX (CPUID 1's EDX bit 23), 3DNow! (0x80000001's EDX bit
// 31, when the extended leaves go that far), SSE (EDX bit 25), SSE2 (EDX bit 26), and SSE3 with MONITOR (ECX bits 0
// and 3); each tried by running one of its instructions (0x005A1900: pxor and emms, xorps, xorpd, pfrcp and emms,
// monitor; a fault leaves it out) - Stock problem: SSE3 runs adds bit 1 (MMX's) to the usable ones, not 0x10. With
// `info` it is cleared and filled: the vendor, the family (an extended one 0xF as its byte x 16 | 0xF, not added),
// the model (an extended one left 0xF), the stepping, the name, the found and usable bits and the mask 0x1F. The
// found bits come back (0 without CPUID).
uint32_t queryCpu(CpuInfo* info);

// 0x005A1A60: the processor's name by its vendor, family and model ("Unknown" when none fits).
void cpuName(const char* vendor, uint32_t family, uint32_t model, std::array<char, 31>& name);

}  // namespace oj
