// Unit tests: OpenJuiceConfig's DirectX 9.0c check from DxDiag's answer (JuicedConfig/DirectXVersion.h).

#include "Framework/Test.h"

#include "JuicedConfig/DirectXVersion.h"

namespace {

using oj::test::check;
using oj::config::directX9cOrLater;

}  // namespace

// The stock's check (0x0040543B) took the letter whatever the major: DirectX 10 .. 12, whose letter is blank, failed
// it (Windows 11: 12.0 and a space). Here a major past 9 passes, 9 by its letter ('c' or later, either case), and no
// answer passes (the Direct3D 9 interface made with SDK version 32 is 9.0c's or later).
OJ_UNIT_TEST(JuicedConfigSystem, DirectX9cCheck) {
    check(directX9cOrLater(S_OK, 12, ' ') && directX9cOrLater(S_OK, 11, ' ') && directX9cOrLater(S_OK, 10, '\0'),
          "DirectX 10 .. 12 with a blank letter: 9.0c or later");
    check(directX9cOrLater(S_OK, 9, 'c') && directX9cOrLater(S_OK, 9, 'C') && !directX9cOrLater(S_OK, 9, 'b') &&
              !directX9cOrLater(S_OK, 9, ' '),
          "DirectX 9 by its letter: c or later only");
    check(!directX9cOrLater(S_OK, 8, 'c'), "DirectX 8: not");
    check(directX9cOrLater(E_FAIL, 0, 0), "no answer from DxDiag: Direct3D 9 made with SDK version 32 answers it");
}
