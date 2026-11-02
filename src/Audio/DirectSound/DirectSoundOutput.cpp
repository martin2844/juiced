#include "Audio/DirectSound/DirectSoundOutput.h"

#include "Audio/Eax.h"
#include "Audio/SoundSystem.h"
#include "Core/Base/Log.h"

#include <windows.h>
#include <mmreg.h>
#include <dsound.h>
#include <dshow.h>

#include <algorithm>
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

Answer answerOf(HRESULT hr) {
    if (hr == DS_OK) return Answer::Ok;
    if (hr == DSERR_BUFFERLOST) return Answer::Lost;
    return SUCCEEDED(hr) ? Answer::Done : Answer::Failed;
}

WAVEFORMATEX waveOf(const SampleFormat& f) {
    WAVEFORMATEX w{};
    w.wFormatTag = f.tag;
    w.nChannels = f.channels;
    w.nSamplesPerSec = f.rate;
    w.nAvgBytesPerSec = f.bytesPerSecond;
    w.nBlockAlign = f.blockAlign;
    w.wBitsPerSample = f.bits;
    return w;
}

template <class T>
void release(T*& p) {
    if (p != nullptr) p->Release();
    p = nullptr;
}

// A voice's buffer: its IDirectSoundBuffer8, the 3D buffer and the property set asked for of it; let go, the
// property set and the 3D buffer released, the buffer stopped and released.
class DirectSoundBuffer final : public OutputBuffer {
public:
    explicit DirectSoundBuffer(IDirectSoundBuffer8* buffer) : m_buffer(buffer) {}
    ~DirectSoundBuffer() override {
        release(m_properties);
        release(m_3d);
        m_buffer->Stop();
        m_buffer->Release();
    }

    IDirectSoundBuffer8* buffer() const { return m_buffer; }

    Answer write(uint32_t at, const void* data, uint32_t bytes) override {
        void* a = nullptr;
        void* b = nullptr;
        DWORD n = 0;
        DWORD m = 0;
        const HRESULT hr = m_buffer->Lock(at, bytes, &a, &n, &b, &m, 0);
        if (hr != DS_OK) return answerOf(hr);
        const auto* from = static_cast<const std::byte*>(data);
        const DWORD first = std::min<DWORD>(n, bytes);
        if (a != nullptr) {
            if (from != nullptr) {
                std::memcpy(a, from, first);
            } else {
                std::memset(a, 0, n);
            }
        }
        if (b != nullptr) {
            if (from != nullptr) {
                std::memcpy(b, from + first, std::min<DWORD>(m, bytes - first));
            } else {
                std::memset(b, 0, m);
            }
        }
        m_buffer->Unlock(a, n, b, m);
        return Answer::Ok;
    }
    Answer restore() override { return answerOf(m_buffer->Restore()); }
    Answer acquireHardware() override { return answerOf(m_buffer->AcquireResources(DSBPLAY_LOCHARDWARE, 0, nullptr)); }
    Answer play(uint32_t flags) override { return answerOf(m_buffer->Play(0, 0, flags)); }
    Answer stop() override { return answerOf(m_buffer->Stop()); }
    Answer setPosition(uint32_t bytes) override { return answerOf(m_buffer->SetCurrentPosition(bytes)); }
    Answer cursors(uint32_t& play, uint32_t& write) override {
        DWORD p = 0;
        DWORD w = 0;
        const HRESULT hr = m_buffer->GetCurrentPosition(&p, &w);
        play = p;
        write = w;
        return answerOf(hr);
    }
    Answer setVolume(int32_t mb) override { return answerOf(m_buffer->SetVolume(mb)); }
    Answer setPan(int32_t mb) override { return answerOf(m_buffer->SetPan(mb)); }
    Answer setFrequency(uint32_t hz) override { return answerOf(m_buffer->SetFrequency(hz)); }
    Answer status(uint32_t& flags) override {
        DWORD s = 0;
        const HRESULT hr = m_buffer->GetStatus(&s);
        flags = s;
        return answerOf(hr);
    }
    bool open3d() override {
        if (m_3d == nullptr) m_buffer->QueryInterface(IID_IDirectSound3DBuffer, reinterpret_cast<void**>(&m_3d));
        return m_3d != nullptr;
    }
    void reset3d() override {
        if (m_3d == nullptr) return;
        DS3DBUFFER p{};
        p.dwSize = sizeof(p);
        p.dwInsideConeAngle = 360;
        p.dwOutsideConeAngle = 360;
        p.vConeOrientation = {0.0f, 0.0f, 1.0f};
        p.flMinDistance = 1.0f;
        p.flMaxDistance = 1e9f;
        p.dwMode = DS3DMODE_NORMAL;
        m_3d->SetAllParameters(&p, DS3D_DEFERRED);
    }
    void place3d(float x, float y, float z) override {
        if (m_3d != nullptr) m_3d->SetPosition(x, y, z, DS3D_DEFERRED);
    }
    bool openEax() override {
        if (m_properties != nullptr) return true;
        if (m_buffer->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&m_properties)) < 0) {
            m_properties = nullptr;
            return false;
        }
        return m_properties != nullptr;
    }
    bool hasEax() const override { return m_properties != nullptr; }
    Answer setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes) override {
        if (m_properties == nullptr) return Answer::Failed;
        return answerOf(m_properties->Set(set, id, nullptr, 0, const_cast<void*>(data), bytes));
    }

private:
    IDirectSoundBuffer8* m_buffer = nullptr;
    IDirectSound3DBuffer* m_3d = nullptr;
    IKsPropertySet* m_properties = nullptr;
};

// The primary buffer's 3D listener.
class DirectSoundListener final : public OutputListener {
public:
    explicit DirectSoundListener(IDirectSound3DListener* listener) : m_listener(listener) {}
    ~DirectSoundListener() override { m_listener->Release(); }
    void setDistanceFactor(float factor) override { m_listener->SetDistanceFactor(factor, DS3D_DEFERRED); }
    void setRolloffFactor(float factor) override { m_listener->SetRolloffFactor(factor, DS3D_DEFERRED); }
    void setPosition(float x, float y, float z) override { m_listener->SetPosition(x, y, z, DS3D_DEFERRED); }
    void setOrientation(float fx, float fy, float fz, float ux, float uy, float uz) override {
        m_listener->SetOrientation(fx, fy, fz, ux, uy, uz, DS3D_DEFERRED);
    }
    void commit() override { m_listener->CommitDeferredSettings(); }

private:
    IDirectSound3DListener* m_listener = nullptr;
};

// A stream of the stock library (0x138 bytes, vtable 0x006FEA04): its DirectShow filter graph. 0x005BB290 →
// 0x005BB300: the graph (COM apartment threaded), its control, seeking and audio; the file added as a source
// (0x005BB380), its pin "Output" (or "Raw Audio 0") rendered, media time, at 0.
class DirectShowStream final : public OutputStream {
public:
    explicit DirectShowStream(const std::filesystem::path& file) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IGraphBuilder,
                                    reinterpret_cast<void**>(&m_graph)))) {
            log::warning("sound: no filter graph for {}", file.string());
            return;
        }
        if (FAILED(m_graph->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&m_control))) ||
            FAILED(m_graph->QueryInterface(IID_IMediaSeeking, reinterpret_cast<void**>(&m_seeking))) ||
            FAILED(m_graph->QueryInterface(IID_IBasicAudio, reinterpret_cast<void**>(&m_audio)))) {
            return;
        }
        IBaseFilter* source = nullptr;
        const std::wstring name = file.wstring();
        if (SUCCEEDED(m_graph->AddSourceFilter(name.c_str(), name.c_str(), &source))) {
            IPin* pin = nullptr;
            if (FAILED(source->FindPin(L"Output", &pin)) && FAILED(source->FindPin(L"Raw Audio 0", &pin))) {
                pin = nullptr;
            }
            if (pin != nullptr) {
                if (SUCCEEDED(m_control->Stop())) {
                    if (FAILED(m_graph->Render(pin))) log::warning("sound: {} not rendered", file.string());
                }
                pin->Release();
            } else {
                log::warning("sound: no pin of {}", file.string());
            }
            source->Release();
        } else {
            log::warning("sound: {} not opened", file.string());
        }
        if (SUCCEEDED(m_seeking->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME))) {
            LONGLONG zero = 0;
            m_seeking->SetPositions(&zero, AM_SEEKING_AbsolutePositioning, &zero, AM_SEEKING_NoPositioning);
        }
    }
    ~DirectShowStream() override {
        if (m_control != nullptr) m_control->Stop();
        release(m_audio);
        release(m_seeking);
        release(m_control);
        release(m_graph);
    }

    void stop() override {
        if (m_control != nullptr) m_control->Stop();
    }
    void run() override {
        if (m_control != nullptr) m_control->Run();
    }
    bool setPositions(int64_t current, int64_t stop) override {
        LONGLONG c = current;
        LONGLONG s = stop;
        return m_seeking != nullptr &&
               SUCCEEDED(m_seeking->SetPositions(&c, AM_SEEKING_AbsolutePositioning | AM_SEEKING_ReturnTime, &s,
                                                 AM_SEEKING_AbsolutePositioning));
    }
    bool seekable() const override { return m_seeking != nullptr; }
    void positions(int64_t& current, int64_t& end) override {
        LONGLONG c = 0;
        LONGLONG e = 0;
        m_seeking->GetPositions(&c, &e);
        current = c;
        end = e;
    }
    void setVolume(int32_t mb) override {
        if (m_audio != nullptr) m_audio->put_Volume(mb);
    }

private:
    IGraphBuilder* m_graph = nullptr;     // +0x04
    IBasicAudio* m_audio = nullptr;       // +0x08
    IMediaControl* m_control = nullptr;   // +0x0C
    IMediaSeeking* m_seeking = nullptr;   // +0x10
};

}  // namespace

// Let go as the stock's system is at the process's end: what stop did not release stays (no COM calls then).
DirectSoundOutput::~DirectSoundOutput() = default;

bool DirectSoundOutput::start(HWND window, uint32_t flags, OutputStart& result) {
    result.flags = flags;
    CoInitialize(nullptr);
    if ((result.flags & kSoftware) != 0) result.flags &= ~kNoEax;
    result.eaxFlags = 0;
    HRESULT made = E_FAIL;
    // 0x005B84B0: eax.dll of the current folder (the game's) makes the device. A 32-bit library: the 64-bit build
    // goes on as the stock game does without it.
    if ((result.flags & kNoEax) == 0) {
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
        result.eaxDevice = true;
    }
    DSCAPS caps{};
    caps.dwSize = sizeof(caps);
    m_device->GetCaps(&caps);
    if (caps.dwMaxHw3DStreamingBuffers == 0) result.flags = (result.flags & ~kHardware) | kSoftware;
    HRESULT hr = m_device->SetCooperativeLevel(window, DSSCL_PRIORITY);
    if (hr == DS_OK) {
        caps = {};
        caps.dwSize = sizeof(caps);
        hr = m_device->GetCaps(&caps);
    }
    result.minFrequency = caps.dwMinSecondarySampleRate;
    result.hardware3dStreaming = caps.dwMaxHw3DStreamingBuffers;
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
    if (result.eaxDevice) {
        // 0x005B88A6: the probe, then the reverb loaded in FX slot 0.
        probeEax(result.eaxFlags);
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
    log::info("sound: DirectSound 8{} on, flags {:#x}, {} hardware 3D streaming buffers",
              result.eaxDevice ? " (eax.dll)" : "", result.flags, result.hardware3dStreaming);
    return true;
}

void DirectSoundOutput::probeEax(uint32_t& eaxFlags) {
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
            // Stock problem: the device and the primary buffer are AddRef'd here and never released for it. The
            // fixed build takes no such reference: stop() lets the device go before eax.dll is freed.
            m_eaxOn = supportsEax(m_eaxProperties, eax::kContext, eax::kContextAllParameters) && m_eaxOn;
            m_eaxOn = supportsEax(m_eaxProperties, eax::kFxSlot0, eax::kFxSlotAllParameters) && m_eaxOn;
            m_eaxOn = supportsEax(m_eaxProperties, eax::kFxSlot1, eax::kFxSlotAllParameters) && m_eaxOn;
            m_eaxOn = supportsEax(m_eaxProperties, eax::kFxSlot2, eax::kFxSlotAllParameters) && m_eaxOn;
            m_eaxOn = supportsEax(m_eaxProperties, eax::kFxSlot3, eax::kFxSlotAllParameters) && m_eaxOn;
            m_eaxOn = supportsEax(m_eaxProperties, eax::kSource, eax::kSourceAllParameters) && m_eaxOn;
            EaxDeviceFlags* info = nullptr;
            if (m_device->QueryInterface(eax::kDeviceFlags, reinterpret_cast<void**>(&info)) == S_OK) {
                DWORD flags = eaxFlags;
                info->flags(&flags);
                eaxFlags = flags;
                info->Release();
                if ((eaxFlags & 0x10) == 0) {
                    eaxFlags = 0;
                    m_eaxOn = false;
                    releaseEax();
                    return;
                }
            }
        }
    }
    if (!m_eaxOn) releaseEax();
}

void DirectSoundOutput::releaseEax() {
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
    release(m_eaxProperties);
    release(m_eaxBuffer3d);
    release(m_eaxBuffer8);
    release(m_eaxBuffer);
    m_eaxOn = false;
}

bool DirectSoundOutput::setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes) {
    if (m_eaxProperties == nullptr) return false;
    return m_eaxProperties->Set(set, id, nullptr, 0, const_cast<void*>(data), bytes) >= 0;
}

bool DirectSoundOutput::getEax(const GUID& set, uint32_t id, void* data, uint32_t bytes) {
    if (!m_eaxOn) return false;
    ULONG returned = 0;
    return m_eaxProperties->Get(set, id, nullptr, 0, data, bytes, &returned) >= 0;
}

void DirectSoundOutput::stop() {
    releaseEax();
    release(m_primary);
    release(m_device);
    if (m_eax != nullptr) FreeLibrary(m_eax);
    m_eax = nullptr;
}

bool DirectSoundOutput::hardwarePlays3d(uint32_t buffers) {
    bool fails = false;
    if (m_device != nullptr && buffers != 0) {
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
        for (uint32_t i = 0; i < buffers && !fails; ++i) {
            IDirectSoundBuffer* b = nullptr;
            if (FAILED(m_device->CreateSoundBuffer(&desc, &b, nullptr))) continue;
            made.push_back(b);
            b->SetVolume(DSBVOLUME_MIN);
            if (b->Play(0, 8, DSBPLAY_LOCHARDWARE) != DS_OK) fails = true;
        }
        for (IDirectSoundBuffer* b : made) b->Release();
    }
    return !fails;
}

std::unique_ptr<OutputBuffer> DirectSoundOutput::createBuffer(const SampleFormat& format, uint32_t bytes,
                                                              uint32_t caps) {
    if (m_device == nullptr) return nullptr;
    WAVEFORMATEX wave = waveOf(format);
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = caps;
    desc.dwBufferBytes = bytes;
    desc.lpwfxFormat = &wave;
    IDirectSoundBuffer* first = nullptr;
    if (FAILED(m_device->CreateSoundBuffer(&desc, &first, nullptr)) || first == nullptr) return nullptr;
    IDirectSoundBuffer8* buffer = nullptr;
    first->QueryInterface(IID_IDirectSoundBuffer8, reinterpret_cast<void**>(&buffer));
    first->Release();
    if (buffer == nullptr) return nullptr;
    return std::make_unique<DirectSoundBuffer>(buffer);
}

std::unique_ptr<OutputBuffer> DirectSoundOutput::duplicate(OutputBuffer& original) {
    if (m_device == nullptr) return nullptr;
    IDirectSoundBuffer* copy = nullptr;
    if (FAILED(m_device->DuplicateSoundBuffer(static_cast<DirectSoundBuffer&>(original).buffer(), &copy)) ||
        copy == nullptr) {
        return nullptr;
    }
    IDirectSoundBuffer8* buffer = nullptr;
    copy->QueryInterface(IID_IDirectSoundBuffer8, reinterpret_cast<void**>(&buffer));
    copy->Release();
    if (buffer == nullptr) return nullptr;
    return std::make_unique<DirectSoundBuffer>(buffer);
}

std::unique_ptr<OutputListener> DirectSoundOutput::openListener() {
    if (m_primary == nullptr) return nullptr;
    IDirectSound3DListener* listener = nullptr;
    m_primary->QueryInterface(IID_IDirectSound3DListener, reinterpret_cast<void**>(&listener));
    if (listener == nullptr) return nullptr;
    return std::make_unique<DirectSoundListener>(listener);
}

std::unique_ptr<OutputStream> DirectSoundOutput::openStream(const std::filesystem::path& file) {
    return std::make_unique<DirectShowStream>(file);
}

}  // namespace oj::audio
