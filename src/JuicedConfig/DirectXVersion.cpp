#include "JuicedConfig/DirectXVersion.h"

#include <dxdiag.h>
#include <oleauto.h>

#include <cctype>

namespace oj::config {

HRESULT directXVersion(uint32_t* major, uint32_t* minor, char* letter) {
    bool gotMajor = false, gotMinor = false, gotLetter = false;
    const bool com = CoInitialize(nullptr) >= 0;
    IDxDiagProvider* provider = nullptr;
    if (CoCreateInstance(CLSID_DxDiagProvider, nullptr, CLSCTX_INPROC_SERVER, IID_IDxDiagProvider,
                         reinterpret_cast<void**>(&provider)) >= 0) {
        DXDIAG_INIT_PARAMS params{};
        params.dwSize = sizeof(params);
        params.dwDxDiagHeaderVersion = 111;  // DXDIAG_DX9_SDK_VERSION
        params.bAllowWHQLChecks = FALSE;
        params.pReserved = nullptr;
        IDxDiagContainer* root = nullptr;
        if (provider->Initialize(&params) >= 0 && provider->GetRootContainer(&root) >= 0) {
            IDxDiagContainer* system = nullptr;
            if (root->GetChildContainer(L"DxDiag_SystemInfo", &system) >= 0) {
                VARIANT value;
                VariantInit(&value);
                if (system->GetProp(L"dwDirectXVersionMajor", &value) >= 0 && value.vt == VT_UI4) {
                    if (major != nullptr) *major = value.ulVal;
                    gotMajor = true;
                }
                VariantClear(&value);
                if (system->GetProp(L"dwDirectXVersionMinor", &value) >= 0 && value.vt == VT_UI4) {
                    if (minor != nullptr) *minor = value.ulVal;
                    gotMinor = true;
                }
                VariantClear(&value);
                if (system->GetProp(L"szDirectXVersionLetter", &value) >= 0 && value.vt == VT_BSTR &&
                    value.bstrVal != nullptr) {
                    char text[10] = {};
                    WideCharToMultiByte(CP_ACP, 0, value.bstrVal, -1, text, 10, nullptr, nullptr);
                    if (letter != nullptr) *letter = text[0];
                    gotLetter = true;
                }
                VariantClear(&value);
                system->Release();
            }
            root->Release();
        }
        provider->Release();
    }
    if (com) CoUninitialize();
    return gotMajor && gotMinor && gotLetter ? S_OK : E_FAIL;
}

bool directX9cOrLater(HRESULT read, uint32_t major, char letter) {
    if (read != S_OK) return true;
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    return major > 9 || (major == 9 && lower >= 'c' && lower <= 'z');
}

}  // namespace oj::config
