// Integration tests: what OpenJuiceConfig asks the system before its window opens (JuicedConfig/DirectXVersion.h):
// DxDiag's DirectX version and the 9.0c check made of it.

#include "Framework/Test.h"

#include "JuicedConfig/DirectXVersion.h"

#include <string>

namespace {

using oj::test::check;

}  // namespace

// DxDiag reports the version: all three values read, a major version of 9 or more.
OJ_INTEGRATION_TEST(JuicedConfigSystem, DirectXVersion) {
    uint32_t major = 0, minor = 0;
    char letter = 0;
    const HRESULT result = oj::config::directXVersion(&major, &minor, &letter);
    check(result == S_OK, "DxDiag's major, minor and letter read");
    check(major >= 9, "DirectX " + std::to_string(major) + "." + std::to_string(minor) + ", letter " +
                          std::to_string(static_cast<int>(static_cast<unsigned char>(letter))) + ": 9 or later");
    // OpenJuiceConfig's check passes on this system (the stock's fails from DirectX 10 on: a blank letter).
    check(oj::config::directX9cOrLater(result, major, letter), "this system's DirectX taken for 9.0c or later");
}
