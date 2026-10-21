// Unit tests: CpuInfo.

#include "Framework/Test.h"

#include "Core/Base/CpuInfo.h"

#include <array>
#include <cstdio>
#include <string>

namespace {

using oj::test::check;

}  // namespace

// The processor query nothing calls (0x005A1F40): the record's mask, a vendor, a name, usable bits among the
// found ones (or MMX's, the SSE3 run's); some names (0x005A1A60).
OJ_UNIT_TEST(CpuInfo, QueryAndNames) {
    oj::CpuInfo info;
    const uint32_t found = oj::queryCpu(&info);
    std::array<char, 31> p4{}, k63{}, none{}, cyrix{};
    oj::cpuName("GenuineIntel", 0xF, 0, p4);
    oj::cpuName("AuthenticAMD", 5, 9, k63);
    oj::cpuName("GenuineIntel", 6, 4, none);
    oj::cpuName("CyrixInstead", 0, 0, cyrix);
    std::printf("     cpu %s %s family %u model %u found 0x%X usable 0x%X\n", info.vendor.data(), info.name.data(),
                info.family, info.model, info.found, info.usable);
    check(info.mask == 0x1F && found == info.found && info.vendor[0] != '\0' && info.name[0] != '\0' &&
              (info.usable & ~info.found & ~oj::kCpuMmx) == 0 && std::string(p4.data()) == "INTEL Pentium-4" &&
              std::string(k63.data()) == "AMD K6-3" && std::string(none.data()) == "Unknown" &&
              std::string(cyrix.data()) == "Cyrix",
          "the processor query: the record, the names");
}
