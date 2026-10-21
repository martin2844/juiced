#include "Audio/SoundSystem.h"

#include "Audio/Eax.h"
#include "Audio/Player.h"
#include "Audio/Stream.h"
#include "Core/Base/Log.h"

#include <windows.h>
#include <mmreg.h>
#include <dsound.h>

#include <cmath>
#include <cstring>
#include <vector>

namespace oj::audio {

namespace {

// The interface of eax.dll's device the probe asks for (eax::kDeviceFlags): its method +0x10 writes the flags.
struct EaxDeviceFlags : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE notCalled() = 0;  // +0x0C: not called (the slot keeps the layout)
    virtual HRESULT STDMETHODCALLTYPE flags(DWORD* out) = 0;  // +0x10
};

// 0x005B9220: QuerySupport of a property; true when it is both read and written (KSPROPERTY_SUPPORT_GET | SET).
bool supportsEax(IKsPropertySet* properties, const GUID& set, uint32_t id) {
    if (properties == nullptr) return false;
    ULONG support = 0;
    if (properties->QuerySupport(set, id, &support) < 0) return false;
    return (support & 3) == 3;
}

}  // namespace

System& system() {
    static System s;
    return s;
}

namespace {
bool g_active = true;
}

void setActive(bool active) { g_active = active; }
bool active() { return g_active; }

void System::update() {
    Stream::focus(g_active);
    Player::updateAll();
}

int32_t volumeMb(float volume) {
    if (0.0f < volume) {
        const double v = volume <= 1.0f ? volume : 1.0;
        // x87: log2 (fyl2x), times log10(2) and 2000 in extended precision, stored as a float, then fistp.
        const float mb = static_cast<float>(0.3010299956639812 * std::log2(v) * 2000.0);
        const int32_t r = static_cast<int32_t>(std::nearbyint(mb));
        if (-10001 < r) return r > 0 ? 0 : r;
    }
    return DSBVOLUME_MIN;
}

uint32_t clockSeconds() {
    static const float frequency = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return static_cast<float>(f.QuadPart);
    }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    const float seconds = static_cast<float>(static_cast<double>(c.QuadPart) / frequency);
    return static_cast<uint32_t>(static_cast<int32_t>(seconds));
}

bool System::start(HWND window, uint32_t configFlags) {
    if ((configFlags & kNoSound) != 0) {
        m_flags = kNoSound;
        return false;
    }
    if (m_on) return true;
    m_flags = configFlags;
    CoInitialize(nullptr);
    if ((m_flags & kSoftware) != 0) m_flags &= ~kNoEax;
    m_eaxFlags = 0;
    HRESULT made = E_FAIL;
    // 0x005B84B0: eax.dll of the current folder (the game's) makes the device. A 32-bit library: the 64-bit build
    // goes on as the stock game does without it.
    if ((m_flags & kNoEax) == 0) {
        m_eax = LoadLibraryA("eax.dll");
        if (m_eax != nullptr) {
            using Create = HRESULT(WINAPI*)(LPGUID, IDirectSound8**, IUnknown*);
            const auto create = reinterpret_cast<Create>(GetProcAddress(m_eax, "EAXDirectSoundCreate8"));
            if (create != nullptr && create(nullptr, &m_device, nullptr) == S_OK && m_device != nullptr) made = S_OK;
        }
    }
    if (made != S_OK) {
        if (FAILED(CoCreateInstance(CLSID_DirectSound8, nullptr, CLSCTX_INPROC_SERVER, IID_IDirectSound8,
                                    reinterpret_cast<void**>(&m_device)))) {
            log::warning("sound: no DirectSound 8");
            return false;
        }
        if (m_device->Initialize(nullptr) != DS_OK) {
            m_device->Release();
            m_device = nullptr;
            log::warning("sound: DirectSound 8 not initialized");
            return false;
        }
    } else {
        m_eaxDevice = true;
    }
    DSCAPS caps{};
    caps.dwSize = sizeof(caps);
    m_device->GetCaps(&caps);
    if (caps.dwMaxHw3DStreamingBuffers == 0) m_flags = (m_flags & ~kHardware) | kSoftware;
    HRESULT hr = m_device->SetCooperativeLevel(window, DSSCL_PRIORITY);
    if (hr == DS_OK) {
        caps = {};
        caps.dwSize = sizeof(caps);
        hr = m_device->GetCaps(&caps);
    }
    m_minFrequency = caps.dwMinSecondarySampleRate;
    m_hw3d = caps.dwMaxHw3DStreamingBuffers;
    if (hr == DS_OK) {
        DSBUFFERDESC desc{};
        desc.dwSize = sizeof(desc);
        desc.dwFlags = 0x100D1;  // PRIMARYBUFFER | CTRL3D | CTRLPAN | CTRLVOLUME | GETCURRENTPOSITION2
        hr = m_device->CreateSoundBuffer(&desc, &m_primary, nullptr);
    }
    if (hr == DS_OK) {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = 22050;
        format.nAvgBytesPerSec = 88200;
        format.nBlockAlign = 4;
        format.wBitsPerSample = 16;
        hr = m_primary->SetFormat(&format);
    }
    if (hr == DS_OK) hr = m_device->SetCooperativeLevel(window, DSSCL_NORMAL);
    if (hr == DS_OK) hr = m_primary->Play(0, 0, DSBPLAY_LOOPING);
    if (hr != DS_OK) {
        // 0x005B8BE0 writes the failure to the debugger and the start returns no system.
        log::warning("sound: the primary buffer failed ({:#x})", static_cast<uint32_t>(hr));
        return false;
    }
    if (m_eaxDevice) {
        // 0x005B88A6: the probe, then the reverb loaded in FX slot 0.
        probeEax();
        eax::FxSlotProperties slot{eax::kReverbEffect, 0, 0, 0};
        if (!setEax(eax::kFxSlot0, eax::kFxSlotAllParameters, &slot, 0x1C)) {
            GUID effect = eax::kNull;
            getEax(eax::kFxSlot0, eax::kFxSlotLoadEffect, &effect, 0x10);
            if (std::memcmp(&effect, &eax::kReverbEffect, sizeof effect) == 0) {
                const uint32_t environment = 0x15;
                if (!setEax(eax::kFxSlot0, eax::kReverbEnvironment, &environment, 4)) {
                    log::debugPrint("Reverb type could not be set\n");
                }
                setEax(eax::kContext, eax::kContextPrimaryFxSlotId, &eax::kFxSlot0, 0x10);
            }
        }
    }
    m_on = true;
    log::info("sound: DirectSound 8{} on, flags {:#x}, {} hardware 3D streaming buffers", m_eaxDevice ? " (eax.dll)" : "",
              m_flags, m_hw3d);
    return true;
}

void System::probeEax() {
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
    if (m_device->CreateSoundBuffer(&desc, &m_eaxBuffer, nullptr) >= 0 &&
        m_eaxBuffer->QueryInterface(IID_IDirectSoundBuffer8, reinterpret_cast<void**>(&m_eaxBuffer8)) >= 0 &&
        m_eaxBuffer8->QueryInterface(IID_IDirectSound3DBuffer, reinterpret_cast<void**>(&m_eaxBuffer3d)) >= 0) {
        if (m_eaxBuffer3d->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&m_eaxProperties)) < 0) {
            log::debugPrint("No eax property set\n");
        } else {
            m_eaxOn = true;
            // Stock problem: the device and the primary buffer are AddRef'd here and never released for it.
            m_device->AddRef();
            if (m_primary != nullptr) m_primary->AddRef();
            m_eaxOn = supportsEax(m_eaxProperties, eax::kContext, eax::kContextAllParameters) && m_eaxOn;
            m_eaxOn = supportsEax(m_eaxProperties, eax::kFxSlot0, eax::kFxSlotAllParameters) && m_eaxOn;
            m_eaxOn = supportsEax(m_eaxProperties, eax::kFxSlot1, eax::kFxSlotAllParameters) && m_eaxOn;
            m_eaxOn = supportsEax(m_eaxProperties, eax::kFxSlot2, eax::kFxSlotAllParameters) && m_eaxOn;
            m_eaxOn = supportsEax(m_eaxProperties, eax::kFxSlot3, eax::kFxSlotAllParameters) && m_eaxOn;
            m_eaxOn = supportsEax(m_eaxProperties, eax::kSource, eax::kSourceAllParameters) && m_eaxOn;
            EaxDeviceFlags* info = nullptr;
            if (m_device->QueryInterface(eax::kDeviceFlags, reinterpret_cast<void**>(&info)) == S_OK) {
                DWORD flags = m_eaxFlags;
                info->flags(&flags);
                m_eaxFlags = flags;
                info->Release();
                if ((m_eaxFlags & 0x10) == 0) {
                    m_eaxFlags = 0;
                    m_eaxOn = false;
                    releaseEax();
                    return;
                }
            }
        }
    }
    if (!m_eaxOn) releaseEax();
}

void System::releaseEax() {
    eax::FxSlotProperties slot{eax::kReverbEffect, -10000, 0, 0};
    if (m_eaxProperties == nullptr ||
        m_eaxProperties->Set(eax::kFxSlot0, eax::kFxSlotAllParameters, nullptr, 0, &slot, 0x1C) < 0) {
        GUID effect = eax::kNull;
        ULONG returned = 0;
        if (m_eaxOn) m_eaxProperties->Get(eax::kFxSlot0, eax::kFxSlotLoadEffect, nullptr, 0, &effect, 0x10, &returned);
        if (std::memcmp(&effect, &eax::kReverbEffect, sizeof effect) == 0) {
            uint32_t environment = 0;
            if (m_eaxProperties == nullptr ||
                m_eaxProperties->Set(eax::kFxSlot0, eax::kReverbEnvironment, nullptr, 0, &environment, 4) < 0) {
                log::debugPrint("Reverb type could not be set\n");
            }
            if (m_eaxProperties != nullptr) {
                GUID primary = eax::kFxSlot0;
                m_eaxProperties->Set(eax::kContext, eax::kContextPrimaryFxSlotId, nullptr, 0, &primary, 0x10);
            }
        }
    }
    if (m_eaxProperties != nullptr) m_eaxProperties->Release();
    m_eaxProperties = nullptr;
    if (m_eaxBuffer3d != nullptr) m_eaxBuffer3d->Release();
    m_eaxBuffer3d = nullptr;
    if (m_eaxBuffer8 != nullptr) m_eaxBuffer8->Release();
    m_eaxBuffer8 = nullptr;
    if (m_eaxBuffer != nullptr) m_eaxBuffer->Release();
    m_eaxBuffer = nullptr;
    m_eaxOn = false;
}

bool System::setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes) {
    if (m_eaxProperties == nullptr) return false;
    return m_eaxProperties->Set(set, id, nullptr, 0, const_cast<void*>(data), bytes) >= 0;
}

bool System::getEax(const GUID& set, uint32_t id, void* data, uint32_t bytes) {
    if (!m_eaxOn) return false;
    ULONG returned = 0;
    return m_eaxProperties->Get(set, id, nullptr, 0, data, bytes, &returned) >= 0;
}

void System::stop() {
    releaseEax();
    if (m_primary != nullptr) m_primary->Release();
    m_primary = nullptr;
    if (m_device != nullptr) m_device->Release();
    m_device = nullptr;
    if (m_eax != nullptr) FreeLibrary(m_eax);
    m_eax = nullptr;
    m_on = false;
}

bool System::hardwarePlays3d() {
    if (m_hardwareAsked >= 0) return m_hardwareAsked != 0;
    bool fails = false;
    if (m_device != nullptr && m_hw3d != 0) {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 1;
        format.nSamplesPerSec = 22050;
        format.nAvgBytesPerSec = 44100;
        format.nBlockAlign = 2;
        format.wBitsPerSample = 16;
        DSBUFFERDESC desc{};
        desc.dwSize = sizeof(desc);
        desc.dwFlags = 0x400B0;  // LOCDEFER | CTRLVOLUME | CTRLFREQUENCY | CTRL3D
        desc.dwBufferBytes = 0x80;
        desc.lpwfxFormat = &format;
        std::vector<IDirectSoundBuffer*> made;
        for (uint32_t i = 0; i < m_hw3d && !fails; ++i) {
            IDirectSoundBuffer* b = nullptr;
            if (FAILED(m_device->CreateSoundBuffer(&desc, &b, nullptr))) continue;
            made.push_back(b);
            b->SetVolume(DSBVOLUME_MIN);
            if (b->Play(0, 8, DSBPLAY_LOCHARDWARE) != DS_OK) fails = true;
        }
        for (IDirectSoundBuffer* b : made) b->Release();
    }
    m_hardwareAsked = fails ? 0 : 1;
    return !fails;
}

}  // namespace oj::audio
