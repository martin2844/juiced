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

}  // namespace oj::config
