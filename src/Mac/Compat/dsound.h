#pragma once
#include "mmreg.h"
#include <memory>
#include <vector>
#define DS_OK 0
#define DSERR_BUFFERLOST (-2)
#define DSBVOLUME_MIN (-10000)
#define DSBCAPS_CTRL3D 16
#define DSBCAPS_CTRLPAN 64
#define DSBCAPS_CTRLVOLUME 128
#define DSBCAPS_GETCURRENTPOSITION2 65536
#define DSBCAPS_GLOBALFOCUS 32768
#define DSBPLAY_LOOPING 1
#define DSBPLAY_LOCHARDWARE 2
#define DSBPLAY_LOCSOFTWARE 4
#define DSBSTATUS_PLAYING 1
#define DSBSTATUS_LOOPING 4
#define DS3D_DEFERRED 1
#define DS3DMODE_NORMAL 0
inline constexpr int IID_IDirectSoundBuffer8 = 1, IID_IDirectSound3DBuffer = 2, IID_IDirectSound3DListener = 3,
                     IID_IKsPropertySet = 4;
struct D3DVECTOR {
    float x = 0, y = 0, z = 0;
};
struct DS3DBUFFER {
    DWORD dwSize = 0;
    D3DVECTOR vPosition{}, vVelocity{};
    DWORD dwInsideConeAngle = 360, dwOutsideConeAngle = 360;
    D3DVECTOR vConeOrientation{};
    LONG lConeOutsideVolume = 0;
    float flMinDistance = 1, flMaxDistance = 1e9;
    DWORD dwMode = 0;
};
struct DSBUFFERDESC {
    DWORD dwSize = 0, dwFlags = 0, dwBufferBytes = 0, dwReserved = 0;
    WAVEFORMATEX *lpwfxFormat = nullptr;
    GUID guid3DAlgorithm{};
};
struct IKsPropertySet {
    int Release() {
        delete this;
        return 0;
    }
    HRESULT Set(const GUID &, uint32_t, void *, uint32_t, void *, uint32_t) {
        return E_FAIL;
    }
};
struct IDirectSoundBuffer {
    virtual ~IDirectSoundBuffer();
    int refs = 1;
    WAVEFORMATEX format{};
    std::vector<uint8_t> raw;
    std::shared_ptr<std::vector<float>> samples;
    double cursor = 0;
    float frequency = 48000, gain = 1, pan = 0;
    bool playing = false, loop = false;
    int AddRef();
    int Release();
    HRESULT QueryInterface(int, void **);
    HRESULT Lock(DWORD, DWORD, void **, DWORD *, void **, DWORD *, DWORD);
    HRESULT Unlock(void *, DWORD, void *, DWORD);
    HRESULT Play(DWORD, DWORD, DWORD);
    HRESULT Stop();
    HRESULT SetCurrentPosition(DWORD);
    HRESULT GetCurrentPosition(DWORD *, DWORD *);
    HRESULT GetStatus(DWORD *);
    HRESULT SetVolume(LONG);
    HRESULT SetFrequency(DWORD);
    HRESULT SetPan(LONG);
    HRESULT Restore() {
        return S_OK;
    }
    HRESULT AcquireResources(DWORD, DWORD, void *) {
        return E_FAIL;
    }
};
struct IDirectSoundBuffer8 : IDirectSoundBuffer {};
struct IDirectSound3DBuffer {
    IDirectSoundBuffer *parent;
    explicit IDirectSound3DBuffer(IDirectSoundBuffer *p) : parent(p) {
        p->AddRef();
    }
    int Release() {
        parent->Release();
        delete this;
        return 0;
    }
    HRESULT SetPosition(float, float, float, DWORD);
    HRESULT SetAllParameters(DS3DBUFFER *p, DWORD flags) {
        return SetPosition(p->vPosition.x, p->vPosition.y, p->vPosition.z, flags);
    }
    HRESULT SetMinDistance(float, DWORD) {
        return S_OK;
    }
};
struct IDirectSound3DListener {
    int Release() {
        delete this;
        return 0;
    }
    HRESULT SetDistanceFactor(float, DWORD) {
        return S_OK;
    }
    HRESULT SetRolloffFactor(float, DWORD) {
        return S_OK;
    }
    HRESULT SetPosition(float, float, float, DWORD) {
        return S_OK;
    }
    HRESULT SetOrientation(float, float, float, float, float, float, DWORD) {
        return S_OK;
    }
    HRESULT CommitDeferredSettings() {
        return S_OK;
    }
};
struct IDirectSound8 {
    HRESULT CreateSoundBuffer(DSBUFFERDESC *, IDirectSoundBuffer **, void *);
    HRESULT DuplicateSoundBuffer(IDirectSoundBuffer *, IDirectSoundBuffer **);
    int Release() {
        delete this;
        return 0;
    }
};
