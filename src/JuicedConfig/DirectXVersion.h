#pragma once
// The DirectX version DxDiag reports, as the stock JuicedConfig.exe asks it (0x004021B0).

#include <windows.h>

#include <cstdint>

namespace oj::config {

// 0x004021B0: COM started (and stopped again when it started here); DxDiag's provider (header version 111, no WHQL
// checks) and its DxDiag_SystemInfo: dwDirectXVersionMajor and dwDirectXVersionMinor (VT_UI4) and the first letter
// of szDirectXVersionLetter (a BSTR, through the ANSI code page). S_OK when all three were read, else E_FAIL; each
// value read is given where it was asked for even then.
HRESULT directXVersion(uint32_t* major, uint32_t* minor, char* letter);

// DirectX 9.0c or later from directXVersion's answer. The stock (0x0040543B) asks for the major 9 or more and the
// letter 'c' or later whatever the major: DirectX 10 .. 12 report a blank letter (Windows 11: 12.0 and a space), so it
// took none of them for one (a stock problem; OpenJuiceConfig departs from it). Here a major past 9 is enough, the
// letter counts only for 9 (either case); without an answer (read not S_OK) the Direct3D 9 interface the tool made with
// D3D_SDK_VERSION (32, which runtimes before 9.0c refuse) answers it: true.
bool directX9cOrLater(HRESULT read, uint32_t major, char letter);

}  // namespace oj::config
