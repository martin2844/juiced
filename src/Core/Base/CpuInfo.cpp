#include "Core/Base/CpuInfo.h"

#include <cstring>
#include <intrin.h>

#include <emmintrin.h>
#include <pmmintrin.h>
#include <xmmintrin.h>

#include <windows.h>

namespace oj {

namespace {

// 0x005A1900: one instruction of the feature run under an exception handler; 1 when it ran. MMX and 3DNow! cannot be
// run on x64 (no MMX instructions there): the system that runs the port runs MMX, and 3DNow! is only tried when CPUID
// reports it, which no processor of an x64 Windows that drops it does.
int runs(uint32_t feature) {
    __try {
        switch (feature) {
        case kCpuSse: {
            volatile __m128 v = _mm_setzero_ps();  // xorps
            (void)v;
            break;
        }
        case kCpuSse2: {
            volatile __m128d v = _mm_setzero_pd();  // xorpd
            (void)v;
            break;
        }
        case kCpuSse3: {
            static volatile int line = 0;
            _mm_monitor(const_cast<int*>(&line), 0, 0);  // monitor
            break;
        }
        default:
            break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return 1;
}

void put(std::array<char, 31>& name, const char* text) {
    name.fill('\0');
    std::memcpy(name.data(), text, std::strlen(text) + 1);
}

}  // namespace

void cpuName(const char* vendor, uint32_t family, uint32_t model, std::array<char, 31>& name) {
    name[0] = '\0';
    const char* text = nullptr;
    if (std::strncmp("AuthenticAMD", vendor, 12) == 0) {
        if (family == 4) {
            text = "AMD Am486";
        } else if (family == 5) {
            if (model <= 3) text = "AMD K5";
            else if (model == 6 || model == 7) text = "AMD K6";
            else if (model == 8) text = "AMD K6-2";
            else if (model >= 9 && model <= 15) text = "AMD K6-3";
        } else if (family == 6) {
            text = "AMD ATHLON";
        }
    } else if (std::strncmp("GenuineIntel", vendor, 12) == 0) {
        if (family == 4) {
            switch (model) {
            case 0:
            case 1: text = "INTEL 486DX"; break;
            case 2: text = "INTEL 486SX"; break;
            case 3: text = "INTEL 486DX2"; break;
            case 4: text = "INTEL 486SL"; break;
            case 5: text = "INTEL 486SX2"; break;
            case 7: text = "INTEL 486DX2E"; break;
            case 8: text = "INTEL 486DX4"; break;
            default: break;
            }
        } else if (family == 5) {
            if (static_cast<int32_t>(model) > 0 && model < 4) text = "INTEL Pentium";
            else if (model == 4) text = "INTEL Pentium-MMX";
        } else if (family == 6) {
            switch (model) {
            case 1: text = "INTEL Pentium-Pro"; break;
            case 3:
            case 5: text = "INTEL Pentium-II"; break;
            case 6: text = "INTEL Celeron"; break;
            case 7:
            case 8:
            case 10: text = "INTEL Pentium-III"; break;
            default: break;
            }
        } else if (family == 0xF && model == 0) {
            text = "INTEL Pentium-4";
        }
    } else if (std::strncmp("CyrixInstead", vendor, 12) == 0) {
        text = "Cyrix";
    } else if (std::strncmp("CentaurHauls", vendor, 12) == 0) {
        text = "Centaur";
    }
    if (text != nullptr) put(name, text);
    if (name[0] == '\0') put(name, "Unknown");
}

uint32_t queryCpu(CpuInfo* info) {
    // 0x005A1880: CPUID present (the flags' ID bit toggles on every processor x64 runs on).
    int regs[4] = {};
    __cpuid(regs, 0);
    char vendor[13] = {};
    std::memcpy(vendor, &regs[1], 4);
    std::memcpy(vendor + 4, &regs[3], 4);
    std::memcpy(vendor + 8, &regs[2], 4);
    int version[4] = {};
    __cpuid(version, 1);
    const auto eax = static_cast<uint32_t>(version[0]);
    const auto ecx = static_cast<uint32_t>(version[2]);
    const auto edx = static_cast<uint32_t>(version[3]);
    uint32_t extended = 0;
    int ext[4] = {};
    __cpuid(ext, static_cast<int>(0x80000000u));
    if (static_cast<uint32_t>(ext[0]) > 0x7FFFFFFFu) {
        __cpuid(ext, static_cast<int>(0x80000001u));
        extended = static_cast<uint32_t>(ext[3]);
    }
    uint32_t found = 0;
    uint32_t usable = 0;
    if ((edx & 0x800000u) != 0) {
        found = kCpuMmx;
        usable = kCpuMmx;  // pxor, emms
    }
    if ((extended & 0x80000000u) != 0) {
        found |= kCpu3dNow;
        usable |= kCpu3dNow;  // pfrcp, emms
    }
    if ((edx & 0x2000000u) != 0) {
        found |= kCpuSse;
        if (runs(kCpuSse) != 0) usable |= kCpuSse;
    }
    if ((edx & 0x4000000u) != 0) {
        found |= kCpuSse2;
        if (runs(kCpuSse2) != 0) usable |= kCpuSse2;
    }
    if ((ecx & 1u) != 0 && (ecx & 8u) != 0) {
        found |= kCpuSse3;
        if (runs(kCpuSse3) != 0) usable |= kCpuMmx;
    }
    if (info != nullptr) {
        *info = CpuInfo{};
        info->found = found;
        info->usable = usable;
        info->family = (eax >> 8) & 0xF;
        if (info->family == 0xF) info->family = ((eax & 0xFF00000u) | 0xF0000u) >> 16;
        info->model = (eax >> 4) & 0xF;
        info->stepping = eax & 0xF;
        std::memcpy(info->vendor.data(), vendor, sizeof vendor);
        cpuName(info->vendor.data(), info->family, info->model, info->name);
        info->mask = 0x1F;
    }
    return found;
}

}  // namespace oj
