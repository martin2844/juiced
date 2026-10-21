#include "Render/HlslCompiler.h"

#include "Core/Base/Log.h"

#include <windows.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <memory>

namespace oj::render::hlsl {

namespace {

using CompileFunction = HRESULT(WINAPI*)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR,
                                         LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

CompileFunction compiler() {
    static const CompileFunction function = [] {
        HMODULE module = LoadLibraryW(L"d3dcompiler_47.dll");
        if (module == nullptr) {
            log::warning("d3dcompiler_47.dll not loaded: the effects' sources do not compile");
            return static_cast<CompileFunction>(nullptr);
        }
        return reinterpret_cast<CompileFunction>(GetProcAddress(module, "D3DCompile"));
    }();
    return function;
}

// The include object of the effects' compile (vtable 0x006F792C): Open (0x0051B0D0) reads the name from the
// developers' folder and keeps the file; Close (0x0051B1F0) lets go of the one whose data it is given, or writes
// "unable to resolve a Close of include file" and gives 1.
class Include : public ID3DInclude {
public:
    explicit Include(const IncludeReader& reader) : m_reader(reader) {}

    HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE, LPCSTR name, LPCVOID, LPCVOID* data, UINT* bytes) override {
        // Stock problem: a file that does not open is read through address 0 (a crash); here the compile fails.
        std::optional<std::vector<std::byte>> file = m_reader ? m_reader(name) : std::nullopt;
        if (!file) return E_FAIL;
        m_files.push_back(std::make_unique<std::vector<std::byte>>(std::move(*file)));
        *data = m_files.back()->data();
        *bytes = static_cast<UINT>(m_files.back()->size());
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Close(LPCVOID data) override {
        const auto it = std::find_if(m_files.begin(), m_files.end(),
                                     [data](const auto& f) { return f->data() == data; });
        if (it == m_files.end()) {
            log::debugPrint("unable to resolve a Close of include file");
            return 1;
        }
        m_files.erase(it);
        return S_OK;
    }

private:
    const IncludeReader& m_reader;
    std::vector<std::unique_ptr<std::vector<std::byte>>> m_files;
};

}  // namespace

std::optional<std::vector<uint8_t>> compile(const std::vector<std::byte>& source, const char* entry,
                                            const char* profile, uint32_t flags, const IncludeReader& include) {
    const CompileFunction function = compiler();
    if (function == nullptr) return std::nullopt;
    Include handler(include);
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT hr = function(source.data(), source.size(), nullptr, nullptr, &handler, entry, profile, flags, 0,
                                &code, &errors);
    if (errors != nullptr) errors->Release();
    if (FAILED(hr) || code == nullptr) {
        if (code != nullptr) code->Release();
        return std::nullopt;
    }
    const auto* bytes = static_cast<const uint8_t*>(code->GetBufferPointer());
    std::vector<uint8_t> out(bytes, bytes + code->GetBufferSize());
    code->Release();
    return out;
}

}  // namespace oj::render::hlsl
