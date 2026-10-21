#pragma once
// HLSL compiled while the game runs, for the developers' effect sources (-localfx, docs/spec/11-effects.md "The
// developers' sources"): the stock game compiles them with the D3DX 9 it links (D3DXCreateEffect of the .fx text,
// D3DXCompileShader of a tx_1_0 function); OpenJuice loads d3dcompiler_47.dll's D3DCompile, which still takes the
// fx_2_0 and tx_1_0 profiles and gives the same layouts (an fx_2_0 effect as effects\*.fxo holds; a texture shader's
// CTAB, CLIT and FXLC).

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace oj::render::hlsl {

// The bytes of a file the source includes; none when it does not open.
using IncludeReader = std::function<std::optional<std::vector<std::byte>>(const std::string& name)>;

// The compiled code, none when the source does not compile or d3dcompiler_47.dll is missing. entry: the function
// (none for an effect); flags: D3DCOMPILE_* (the same values as D3DXSHADER_*). The compiler's messages are dropped,
// as the stock game drops its error buffer.
std::optional<std::vector<uint8_t>> compile(const std::vector<std::byte>& source, const char* entry,
                                            const char* profile, uint32_t flags, const IncludeReader& include);

}  // namespace oj::render::hlsl
