#include "JuicedConfig/EaxCheck.h"

#include "Audio/Eax.h"

#include <windows.h>
#include <mmreg.h>
#include <dsound.h>

#include <cstdint>

namespace oj::config {

namespace eax = audio::eax;

namespace {

// The interface of eax.dll's device the probe asks for (eax::kDeviceFlags): its method +0x10 writes the flags.
struct EaxDeviceFlags : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE notCalled() = 0;        // +0x0C: not called (the slot keeps the layout)
    virtual HRESULT STDMETHODCALLTYPE flags(DWORD* out) = 0;  // +0x10
};

using EaxCreate = HRESULT(WINAPI*)(LPCGUID, IDirectSound8**, IUnknown*);

// The tool's globals of the probe ([0x004131A0] ..).
HMODULE g_library = nullptr;           // [0x004131A0]
IDirectSound8* g_device = nullptr;     // [0x004131A4]
IDirectSoundBuffer* g_primary = nullptr;  // [0x004131A8]: never set
bool g_eaxOn = false;                  // [0x004131BC]: worked out, read by nothing
DWORD g_flags = 0;                     // [0x004131C0]: the device's EAX flags

// 0x00405510: QuerySupport of a property; true when it is both read and written (KSPROPERTY_SUPPORT_GET | SET).
bool supports(IKsPropertySet* properties, const GUID& set, uint32_t id) {
    if (properties == nullptr) return false;
    ULONG support = 0;
    if (properties->QuerySupport(set, id, &support) < 0) return false;
    return (support & 3) == 3;
}

// 0x00405550: a buffer of 64 bytes (mono, 22050 Hz, 16 bits, in the hardware, 3D) and its property set; with one the
// device (and the primary buffer, none here) AddRef'd for good (Stock problem: never released; not in the fixed
// build), the EAX 4.0 context,
// FX slots and source asked for, then the device's flags (cleared without 0x10). The buffer's interfaces released.
void probe() {
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = 22050;
    format.nAvgBytesPerSec = 44100;
    format.nBlockAlign = 2;
    format.wBitsPerSample = 16;
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_LOCHARDWARE | DSBCAPS_CTRL3D;  // 0x14
    desc.dwBufferBytes = 0x40;
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer* buffer = nullptr;
    IDirectSoundBuffer8* buffer8 = nullptr;
    IDirectSound3DBuffer* buffer3d = nullptr;
    IKsPropertySet* properties = nullptr;
    if (g_device->CreateSoundBuffer(&desc, &buffer, nullptr) >= 0 &&
        buffer->QueryInterface(IID_IDirectSoundBuffer8, reinterpret_cast<void**>(&buffer8)) >= 0 &&
        buffer8->QueryInterface(IID_IDirectSound3DBuffer, reinterpret_cast<void**>(&buffer3d)) >= 0 &&
        buffer3d->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&properties)) >= 0) {
        g_eaxOn = true;  // the fixed build takes no reference of the device for good (eaxAvailable lets it go)
        g_eaxOn = supports(properties, eax::kContext, eax::kContextAllParameters) && g_eaxOn;
        g_eaxOn = supports(properties, eax::kFxSlot0, eax::kFxSlotAllParameters) && g_eaxOn;
        g_eaxOn = supports(properties, eax::kFxSlot1, eax::kFxSlotAllParameters) && g_eaxOn;
        g_eaxOn = supports(properties, eax::kFxSlot2, eax::kFxSlotAllParameters) && g_eaxOn;
        g_eaxOn = supports(properties, eax::kFxSlot3, eax::kFxSlotAllParameters) && g_eaxOn;
        g_eaxOn = supports(properties, eax::kSource, eax::kSourceAllParameters) && g_eaxOn;
        EaxDeviceFlags* info = nullptr;
        if (g_device->QueryInterface(eax::kDeviceFlags, reinterpret_cast<void**>(&info)) == S_OK) {
            info->flags(&g_flags);
            info->Release();
            if ((g_flags & 0x10) == 0) {
                g_flags = 0;
                g_eaxOn = false;
            }
        }
    }
    if (properties != nullptr) properties->Release();
    if (buffer3d != nullptr) buffer3d->Release();
    if (buffer8 != nullptr) buffer8->Release();
    if (buffer != nullptr) buffer->Release();
}

}  // namespace

// The fixed build lets the device and the library go after each question (the answer is the same).
bool eaxAvailable() {
    g_library = LoadLibraryA("eax.dll");
    if (g_library == nullptr) return false;
    bool answer = false;
    const auto create = reinterpret_cast<EaxCreate>(GetProcAddress(g_library, "EAXDirectSoundCreate8"));
    if (create != nullptr && create(nullptr, &g_device, nullptr) == S_OK && g_device != nullptr) {
        probe();
        answer = ((g_flags >> 4) & 1) != 0;
    }
    if (g_device != nullptr) g_device->Release();
    g_device = nullptr;
    FreeLibrary(g_library);
    g_library = nullptr;
    return answer;
}

}  // namespace oj::config
