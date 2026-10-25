#include "Core/Network/GameSpy/GameSpyVoice.h"

#include <windows.h>
#include <mmreg.h>
#include <dsound.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "speex.h"
#include "speex_bits.h"
}

#ifdef _MSC_VER
#pragma comment(lib, "dsound.lib")
#endif
#ifdef _MSC_VER
#pragma comment(lib, "dxguid.lib")
#endif
#ifdef _MSC_VER
#pragma comment(lib, "ole32.lib")
#endif

namespace oj::net::gv {

// A frame waiting to be mixed (GVIPendingFrame, the encoded size + 0x0B bytes): its stamp, the next and the encoded
// bytes.
struct PendingFrame {
    FrameStamp stamp = 0;
    PendingFrame* next = nullptr;
    uint8_t frame[1];
};

// A source of a playback device (0x24 bytes).
struct SourceEntry {
    int32_t inUse = 0;          // +0x00
    Source id = 0;              // +0x04
    int32_t talking = 0;        // +0x08: a frame of it was decoded
    FrameStamp end = 0;         // +0x0C: the stamp past its last frame
    FrameStamp offset = 0;      // +0x0E: added to its stamps (the clock against the sender's)
    float average = 0.0f;       // +0x10: the offset as it goes on, smoothed (wrapped into 0 .. 65535)
    void* decoder = nullptr;    // +0x14
    PendingFrame* head = nullptr;  // +0x1C: its frames by stamp
};

// GVISourceList (0x120 bytes).
struct SourceList {
    SourceEntry sources[8];
};

namespace {

// The SDK's globals.
struct Globals {
    CodecInfo codec;               // [0x0075D92C]
    int32_t samplesPerFrame = 0;   // [0x007A4330]
    int32_t encodedFrameSize = 0;  // [0x007A432C]
    int32_t bytesPerFrame = 0;     // [0x007A4328]
    HWND window = nullptr;         // [0x0075D948]
    bool listed = false;           // [0x0075D94C]: the list of devices made
    std::vector<Device*> devices;
    WAVEFORMATEX format{};         // [0x0075D950]
    PendingFrame* freeFrames = nullptr;  // [0x0075D964]
    int32_t latency = 0;           // [0x0075D998]: the frames of a playback buffer
};

Globals g;

// The Speex codec's globals.
struct SpeexGlobals {
    SpeexBits bits{};         // [0x0075D968]
    void* encoder = nullptr;  // [0x0075D98C]
    int32_t initialized = 0;  // [0x0075D990]
    int32_t encodedSize = 0;  // [0x0075D994]
};

SpeexGlobals sp;

// _ftol (0x005CA370): toward zero, 0x80000000 out of range.
int32_t ftol(double v) {
    if (!(v > -2147483649.0 && v < 2147483648.0)) return INT32_MIN;
    return static_cast<int32_t>(v);
}

PendingFrame* allocFrame() {
    return static_cast<PendingFrame*>(std::malloc(offsetof(PendingFrame, frame) + static_cast<size_t>(g.encodedFrameSize) + 3));
}

// 0x0057FC20: the free frames freed.
void freeFramePool() {
    while (g.freeFrames != nullptr) {
        PendingFrame* next = g.freeFrames->next;
        std::free(g.freeFrames);
        g.freeFrames = next;
    }
}

// 0x0057FBD0: those there freed, then 200 frames of the encoded size on the free list (fewer when one cannot be had).
void makeFramePool() {
    if (g.freeFrames != nullptr) freeFramePool();
    g.freeFrames = nullptr;
    for (int32_t i = 0; i < 200; ++i) {
        PendingFrame* f = allocFrame();
        if (f == nullptr) return;
        f->next = g.freeFrames;
        g.freeFrames = f;
    }
}

// 0x005805A0: a source in use: its frames back on the free list, its decoder freed, not in use.
void freeSource(SourceEntry& s) {
    if (s.inUse == 0) return;
    for (PendingFrame* f = s.head; f != nullptr;) {
        PendingFrame* next = f->next;
        f->next = g.freeFrames;
        g.freeFrames = f;
        f = next;
    }
    s.head = nullptr;
    if (g.codec.freeDecoder != nullptr) g.codec.freeDecoder(s.decoder);
    s.inUse = 0;
}

// 0x00580640: every source freed.
void freeSources(SourceList& list) {
    for (SourceEntry& s : list.sources) freeSource(s);
}

// 0x005805E0: the frames of a playback buffer counted once (3200 bytes rounded up to a frame's); eight sources
// zeroed.
SourceList* newSourceList() {
    if (g.latency == 0) {
        int32_t bytes = 0xC80;
        if (bytes % g.bytesPerFrame != 0) bytes = g.bytesPerFrame - bytes % g.bytesPerFrame + 0xC80;
        g.latency = bytes / g.bytesPerFrame;
    }
    return new SourceList();
}

// 0x00580700: the first source not in use, else the first in use without frames (freed first); its decoder made
// (none when that fails), nothing waiting, in use for the source.
SourceEntry* newSource(SourceList& list, Source id) {
    SourceEntry* s = nullptr;
    for (SourceEntry& e : list.sources) {
        if (e.inUse == 0) {
            s = &e;
            break;
        }
        if (s == nullptr && e.head == nullptr) s = &e;
    }
    if (s == nullptr) return nullptr;
    if (s->inUse != 0) freeSource(*s);
    if (g.codec.newDecoder == nullptr) {
        s->decoder = nullptr;
    } else if (!g.codec.newDecoder(&s->decoder)) {
        return nullptr;
    }
    s->offset = 0;
    s->average = 0.0f;
    s->talking = 0;
    s->end = 0;
    s->head = nullptr;
    s->inUse = 1;
    s->id = id;
    return s;
}

// 0x00580780: the packet's frames (len / the encoded size of them) put into the source's list from the stamp moved
// by its offset, in order of their stamps; the source's end moved past them when they go further; nothing when a
// frame of the first's stamp is there already. A frame is taken off the free list, or made.
void addFrames(SourceEntry& s, const uint8_t* packet, int32_t len, FrameStamp stamp) {
    const int32_t frameSize = g.encodedFrameSize;
    const int32_t frames = len / frameSize;
    FrameStamp at = static_cast<FrameStamp>(stamp + s.offset);
    const FrameStamp end = static_cast<FrameStamp>(at + frames);
    if (static_cast<uint16_t>(s.end - end) > 0x7FFF) s.end = end;
    PendingFrame** link = &s.head;
    for (PendingFrame* f = s.head; f != nullptr; f = f->next) {
        if (f->stamp == at) return;
        if (static_cast<uint16_t>(at - f->stamp) > 0x7FFF) break;
        link = &f->next;
    }
    for (int32_t i = 0; i < frames; ++i) {
        PendingFrame* f = g.freeFrames;
        if (f != nullptr) {
            g.freeFrames = f->next;
        } else {
            f = allocFrame();
        }
        if (f == nullptr) return;
        f->stamp = at;
        at = static_cast<FrameStamp>(at + 1);
        std::memcpy(f->frame, packet, static_cast<size_t>(frameSize));
        packet += frameSize;
        f->next = *link;
        *link = f;
        link = &f->next;
    }
}

// 0x00580880: a packet of a source to a playback device whose clock is `clock`. The difference of the clocks (the
// buffer's frames ahead) is d = latency - stamp + clock. A source new (0x00580700) takes it as its offset and average
// (the end its stamp's place). Else the average follows d by 1/32 (d taken a lap of 65535 up or down when it wrapped
// against the average), kept within 0 .. 65535; past the average by more than 10 the offset is one less and the
// packet's first frame dropped (the packet with it when it had one frame only), under it by more than 5 the offset
// one more. A packet whose stamp with the offset is behind the clock is dropped; the rest go to the source
// (0x00580780).
void addPacket(SourceList& list, Source id, const uint8_t* packet, int32_t len, FrameStamp stamp, FrameStamp clock) {
    const int32_t diff = g.latency - stamp + clock;
    SourceEntry* s = nullptr;
    for (SourceEntry& e : list.sources) {
        if (e.inUse != 0 && e.id == id) {
            s = &e;
            break;
        }
    }
    if (s == nullptr) {
        s = newSource(list, id);
        if (s == nullptr) return;
        s->offset = static_cast<FrameStamp>(diff);
        s->end = static_cast<FrameStamp>(diff + stamp);
        s->average = static_cast<float>(static_cast<uint16_t>(diff));
        addFrames(*s, packet, len, stamp);
        return;
    }
    int32_t d = static_cast<uint16_t>(diff);
    const float df = static_cast<float>(d);
    bool wrapped = false;
    if (df < s->average && static_cast<uint16_t>(ftol(s->average) - diff) > 0x7FFF) {
        d += 0xFFFF;
        wrapped = true;
    }
    if (!wrapped && df > s->average && static_cast<uint16_t>(diff - ftol(s->average)) > 0x7FFF) d -= 0xFFFF;
    // On the FPU's stack: the sum kept unrounded for the checks and the lap's correction.
    const double sum = static_cast<double>(s->average) * 0.96875 + static_cast<double>(d) * 0.03125;
    s->average = static_cast<float>(sum);
    if (sum < 0.0) {
        s->average = static_cast<float>(sum + 65535.0);
    } else if (!(sum < 65535.0)) {
        s->average = static_cast<float>(sum - 65535.0);
    }
    const int32_t ahead = static_cast<int32_t>(s->offset) - ftol(s->average);
    if (static_cast<uint16_t>(10 - ahead) > 0x7FFF) {
        s->offset = static_cast<FrameStamp>(s->offset - 1);
        if (len == g.encodedFrameSize) return;
        packet += g.encodedFrameSize;
        len -= g.encodedFrameSize;
    } else if (static_cast<uint16_t>(ahead + 5) > 0x7FFF) {
        s->offset = static_cast<FrameStamp>(s->offset + 1);
    }
    if (static_cast<uint16_t>(s->offset - clock + stamp) > 0x7FFF) return;
    addFrames(*s, packet, len, stamp);
}

// 0x00580A10: `frames` frames from the stamp `start` mixed into out (zeroed first): each source's frames up to the
// end decoded and added at their places (a frame before the start dropped undecoded; the source then talking), then
// back on the free list; a source with none left whose end is not past the mix's end freed. True.
bool mixSources(SourceList& list, int32_t frames, FrameStamp start, Sample* out) {
    const FrameStamp end = static_cast<FrameStamp>(start + frames);
    std::memset(out, 0, static_cast<size_t>(g.bytesPerFrame) * static_cast<size_t>(frames));
    for (SourceEntry& s : list.sources) {
        if (s.inUse == 0) continue;
        while (s.head != nullptr) {
            PendingFrame* f = s.head;
            if (static_cast<uint16_t>(end - f->stamp - 1) > 0x7FFF) break;
            if (static_cast<uint16_t>(start - f->stamp - 1) > 0x7FFF) {
                const uint16_t at = static_cast<uint16_t>(f->stamp - start);
                g.codec.decode(out + static_cast<size_t>(at) * static_cast<size_t>(g.samplesPerFrame), f->frame,
                               s.decoder);
                s.talking = 1;
            }
            s.head = f->next;
            f->next = g.freeFrames;
            g.freeFrames = f;
        }
        if (s.head == nullptr && static_cast<uint16_t>(s.end - end - 1) > 0x7FFF) freeSource(s);
    }
    return true;
}

// 0x005804C0: the greatest magnitude of the samples (as a short: -32768 stays where it is) over 32768.
double peak(const Sample* samples, int32_t count) {
    int16_t most = 0;
    for (int32_t i = 0; i < count; ++i) {
        int16_t v = samples[i];
        if (v < 0) v = static_cast<int16_t>(-v);
        if (most < v) most = v;
    }
    return static_cast<double>(most) * 3.0517578125e-05;
}

// 0x0057EDD0: a playback device's mix. Not playing: true. The play cursor (a frame's bytes down) against the last
// gives the bytes played since; a lag (the milliseconds since the last mix and the cursor's bytes against the
// milliseconds played: the stock's mix of units) of half the buffer or more moves the clock on by whole buffers. The
// bytes played are locked from the last cursor and mixed (0x00580A10, the filter after each frame), the clock moved
// on by their frames.
bool playbackThink(Device& device) {
    Device::Data& h = device.data;
    if (h.playing == 0) return true;
    DWORD play = 0;
    DWORD write = 0;
    if (FAILED(h.buffer->GetCurrentPosition(&play, &write))) return false;
    const int32_t aligned = static_cast<int32_t>(play) - static_cast<int32_t>(play) % g.bytesPerFrame;
    const uint32_t old = h.writePos;
    const uint32_t advanced = (h.bufferSize - old + static_cast<uint32_t>(aligned)) % h.bufferSize;
    h.writePos = static_cast<uint32_t>(aligned);
    const DWORD now = GetTickCount();
    const int32_t elapsed = static_cast<int32_t>(now - h.tick);
    h.tick = now;
    int32_t lag = elapsed + (aligned - static_cast<int32_t>(advanced * 1000 / 16000) - static_cast<int32_t>(play));
    if (lag >= static_cast<int32_t>(h.halfSize * 1000 / 16000)) {
        const int32_t whole = static_cast<int32_t>(h.bufferSize * 1000 / 16000);
        const int32_t r = lag % whole;
        if (r < whole / 2) {
            lag -= r;
        } else {
            lag = whole - r + lag;
        }
        const uint32_t frames = h.bufferSize / static_cast<uint32_t>(g.bytesPerFrame);
        h.clock = static_cast<FrameStamp>(h.clock + static_cast<FrameStamp>(frames * static_cast<uint32_t>(lag / whole)));
    }
    if (advanced == 0) return true;
    void* p1 = nullptr;
    void* p2 = nullptr;
    DWORD b1 = 0;
    DWORD b2 = 0;
    if (FAILED(h.buffer->Lock(old, advanced, &p1, &b1, &p2, &b2, 0))) return false;
    const auto part = [&](void* p, DWORD bytes) {
        const int32_t frames = static_cast<int32_t>(bytes / static_cast<uint32_t>(g.bytesPerFrame));
        Sample* out = static_cast<Sample*>(p);
        if (!mixSources(*h.sources, frames, h.clock, out)) std::memset(out, 0, bytes);
        if (device.playbackFilter != nullptr) {
            for (int32_t i = 0; i < frames; ++i) {
                device.playbackFilter(&device, out + static_cast<size_t>(g.samplesPerFrame) * static_cast<size_t>(i),
                                      static_cast<FrameStamp>(h.clock + i));
            }
        }
        h.clock = static_cast<FrameStamp>(h.clock + frames);
    };
    part(p1, b1);
    if (p2 != nullptr) part(p2, b2);
    return SUCCEEDED(h.buffer->Unlock(p1, b1, p2, b2));
}

// 0x0057F2C0: stopped, all of it zeroed, from the start, looping; the clock 0, the tick taken; playing.
bool startPlayback(Device::Data& h) {
    h.buffer->Stop();
    void* p1 = nullptr;
    void* p2 = nullptr;
    DWORD b1 = 0;
    DWORD b2 = 0;
    if (FAILED(h.buffer->Lock(0, 0, &p1, &b1, &p2, &b2, DSBLOCK_ENTIREBUFFER))) return false;
    std::memset(p1, 0, b1);
    if (FAILED(h.buffer->Unlock(p1, b1, p2, b2))) return false;
    if (FAILED(h.buffer->SetCurrentPosition(0))) return false;
    if (FAILED(h.buffer->Play(0, 0, DSBPLAY_LOOPING))) return false;
    h.clock = 0;
    h.tick = GetTickCount();
    h.playing = 1;
    return true;
}

// 0x0057F420: DirectSound's volume 2000 x ln(volume) (cut toward zero), -10000 for 0.
void setPlaybackVolume(Device::Data& h, double volume) {
    if (volume == 0.0) {
        h.buffer->SetVolume(-10000);
        return;
    }
    h.buffer->SetVolume(static_cast<LONG>(static_cast<int64_t>(std::log(volume) * 2000.0)));
}

// 0x0057EC70 (the list's free function): a playback device's sources freed, its buffer and DirectSound released; a
// capture device's buffer stopped and released, DirectSoundCapture released; the device freed.
void destroyDevice(Device* device) {
    Device::Data& h = device->data;
    if (device->types == kPlayback) {
        if (h.sources != nullptr) {
            freeSources(*h.sources);
            delete h.sources;
        }
        if (h.buffer != nullptr) h.buffer->Release();
        if (h.sound != nullptr) h.sound->Release();
    } else {
        if (h.captureBuffer != nullptr) {
            h.captureBuffer->Stop();
            h.captureBuffer->Release();
        }
        if (h.capture != nullptr) h.capture->Release();
    }
    delete device;
}

// 0x0057F8B0: DSSCL_NORMAL with the window; the buffer's bytes 3200 rounded up to two frames' bytes, half of them
// kept; the buffer (GETCURRENTPOSITION2 | GLOBALFOCUS | CTRLVOLUME) of the format.
bool makePlaybackBuffer(Device::Data& h) {
    if (FAILED(h.sound->SetCooperativeLevel(g.window, DSSCL_NORMAL))) return false;
    uint32_t bytes = 0xC80;
    const int32_t two = g.bytesPerFrame * 2;
    h.bufferSize = 0xC80;
    if (0xC80 % two != 0) bytes = static_cast<uint32_t>(two - 0xC80 % two + 0xC80);
    DSBUFFERDESC desc{};
    h.halfSize = bytes >> 1;
    h.bufferSize = bytes;
    desc.dwSize = 0x24;
    desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME;
    desc.dwBufferBytes = bytes;
    desc.lpwfxFormat = &g.format;
    return SUCCEEDED(h.sound->CreateSoundBuffer(&desc, &h.buffer, nullptr));
}

// 0x0057F980: the buffer's bytes 16000 rounded up to a frame's bytes; the capture buffer of the format, started
// looping (released when it does not start).
bool makeCaptureBuffer(Device::Data& h) {
    uint32_t bytes = 16000;
    h.captureSize = 16000;
    if (16000 % g.bytesPerFrame != 0) bytes = static_cast<uint32_t>(g.bytesPerFrame - 16000 % g.bytesPerFrame + 16000);
    DSCBUFFERDESC desc{};
    h.captureSize = bytes;
    desc.dwBufferBytes = bytes;
    desc.dwSize = 0x1C;
    desc.lpwfxFormat = &g.format;
    if (FAILED(h.capture->CreateCaptureBuffer(&desc, &h.captureBuffer, nullptr))) return false;
    if (SUCCEEDED(h.captureBuffer->Start(DSCBSTART_LOOPING))) return true;
    h.captureBuffer->Release();
    h.captureBuffer = nullptr;
    return false;
}

// The callback's context (0x0057F190's locals): the list, its room, and the kind enumerated (+0x0C).
struct ListContext {
    std::vector<DeviceInfo>* devices;
    int32_t max;
    bool capture;
};

// 0x0057F0B0 (LPDSENUMCALLBACKA).
BOOL CALLBACK onDevice(LPGUID guid, LPCSTR description, LPCSTR, LPVOID context) {
    ListContext& c = *static_cast<ListContext*>(context);
    if (guid == nullptr) return TRUE;
    DeviceInfo* entry = nullptr;
    for (DeviceInfo& d : *c.devices) {
        if (std::memcmp(d.id.data(), guid, 16) == 0) entry = &d;
    }
    if (entry == nullptr) {
        // Stock problem: the entry is added without a look at the room, so a playback device met with the list
        // already full (ten capture devices) is written past the voice object's array (over the count, +0x648).
        // Here it is not added.
        if (static_cast<int32_t>(c.devices->size()) >= c.max) return FALSE;
        c.devices->push_back(DeviceInfo{});
        entry = &c.devices->back();
        std::memcpy(entry->id.data(), guid, 16);
        // strncpy with 0x40, the last byte 0.
        size_t n = 0;
        while (n < sizeof(entry->name) && description != nullptr && description[n] != 0) {
            entry->name[n] = description[n];
            ++n;
        }
        entry->name[sizeof(entry->name) - 1] = 0;
    }
    entry->type = c.capture ? kCapture : kPlayback;
    entry->hardware = 0;
    return static_cast<int32_t>(c.devices->size()) == c.max ? FALSE : TRUE;
}

}  // namespace

std::vector<DeviceInfo> listDevices(int32_t max, int32_t types) {
    std::vector<DeviceInfo> devices;
    if (max < 1) return devices;
    devices.reserve(static_cast<size_t>(max));
    ListContext context{&devices, max, true};
    if ((types & kCapture) != 0) {
        context.capture = true;
        DirectSoundCaptureEnumerateA(onDevice, &context);
    }
    if ((types & kPlayback) != 0) {
        context.capture = false;
        DirectSoundEnumerateA(onDevice, &context);
    }
    GUID capture{};
    GUID playback{};
    GetDeviceID(&DSDEVID_DefaultVoiceCapture, &capture);
    GetDeviceID(&DSDEVID_DefaultVoicePlayback, &playback);
    for (DeviceInfo& d : devices) {
        if (std::memcmp(d.id.data(), &capture, 16) == 0) {
            d.isDefault = 1;
        } else if (std::memcmp(d.id.data(), &playback, 16) == 0) {
            d.isDefault = 2;
        }
    }
    return devices;
}

// ---- The devices' methods.

void Device::free() {
    if (!g.listed) return;
    for (size_t i = 0; i < g.devices.size(); ++i) {
        if (g.devices[i] == this) {
            // ArrayDeleteAt (0x0056D930): the list's free function, then the rest moved down.
            g.devices.erase(g.devices.begin() + static_cast<std::ptrdiff_t>(i));
            destroyDevice(this);
            return;
        }
    }
}

bool Device::start(int32_t type) {
    if (type == kPlayback) return startPlayback(data);
    data.capturing = 1;
    return true;
}

void Device::stop(int32_t type) {
    if (type == kPlayback) {
        data.buffer->Stop();
        data.playing = 0;
        freeSources(*data.sources);
        return;
    }
    data.capturing = 0;
    speexResetEncoder();
}

bool Device::isStarted(int32_t type) {
    if (type == kPlayback) {
        DWORD status = 0;
        if (FAILED(data.buffer->GetStatus(&status))) return false;
        return (status & DSBSTATUS_PLAYING) != 0;
    }
    return data.capturing != 0;
}

void Device::setVolume(int32_t type, double v) {
    if (type == kPlayback) {
        setPlaybackVolume(data, v);
        return;
    }
    data.volume = v;
}

// Playback: e^(DirectSound's volume x 0.0005) (0 when it cannot be had); capture: its own.
double Device::volume(int32_t type) {
    if (type == kPlayback) {
        LONG v = 0;
        if (FAILED(data.buffer->GetVolume(&v))) return 0.0;
        return std::exp(static_cast<double>(v) * 0.0005);
    }
    return data.volume;
}

void Device::setCaptureThreshold(double threshold) {
    data.threshold = threshold;
}

double Device::captureThreshold() const {
    return data.threshold;
}

// The read cursor against the last read: the frames there, as encoded bytes. A failure: the unplugged callback, the
// device freed, 0.
int32_t Device::availableCaptureBytes() {
    DWORD read = 0;
    if (FAILED(data.captureBuffer->GetCurrentPosition(nullptr, &read))) {
        if (unplugged != nullptr) {
            unplugged(this);
            free();
        }
        return 0;
    }
    const uint32_t bytes = (data.captureSize - data.readPos + read) % data.captureSize;
    return static_cast<int32_t>(bytes / static_cast<uint32_t>(g.bytesPerFrame)) * g.encodedFrameSize;
}

// The frames: those read (the read cursor against the last read) up to len's room in encoded frames; none: false.
// Not capturing they are passed over. Else locked: the peak of both parts as the volume; at the threshold or above
// (the stamp kept), or within 20 frames of the last stamp that was, each frame scaled by the volume when it is under
// 1, filtered and encoded at its place; unlocked. The read position and the clock move on by the frames, len their
// encoded bytes. A failure of DirectSound: the unplugged callback, the device freed, false.
bool Device::capturePacket(uint8_t* packet, int32_t* len, FrameStamp* stamp, double* volume) {
    const int32_t room = *len;
    *len = 0;
    *volume = 0.0;
    int32_t result = 0;
    Data& h = data;
    const auto fail = [&] {
        if (unplugged != nullptr) {
            unplugged(this);
            free();
        }
        return false;
    };
    DWORD read = 0;
    if (FAILED(h.captureBuffer->GetCurrentPosition(nullptr, &read))) return fail();
    int32_t frames = static_cast<int32_t>(((h.captureSize - h.readPos + read) % h.captureSize) /
                                          static_cast<uint32_t>(g.bytesPerFrame));
    const int32_t most = room / g.encodedFrameSize;
    if (frames >= most) frames = most;
    if (frames == 0) return false;
    const uint32_t lockBytes = static_cast<uint32_t>(g.bytesPerFrame * frames);
    if (h.capturing != 0) {
        void* p1 = nullptr;
        void* p2 = nullptr;
        DWORD b1 = 0;
        DWORD b2 = 0;
        if (FAILED(h.captureBuffer->Lock(h.readPos, lockBytes, &p1, &b1, &p2, &b2, 0))) return fail();
        const double peak1 = peak(static_cast<const Sample*>(p1), static_cast<int32_t>(b1 >> 1));
        const double peak2 = peak(static_cast<const Sample*>(p2), static_cast<int32_t>(b2 >> 1));
        const double loudest = peak1 <= peak2 ? peak2 : peak1;
        *volume = loudest;
        if (!(loudest < h.threshold)) {
            result = 1;
            h.lastAbove = h.captureClock;
        } else {
            result = static_cast<uint16_t>(h.captureClock - h.lastAbove) < 0x14 ? 1 : 0;
        }
        if (result != 0) {
            *stamp = h.captureClock;
            Sample* samples = static_cast<Sample*>(p1);
            uint32_t left = b1;
            for (int32_t i = 0; i < frames; ++i) {
                if (h.volume < 1.0) {
                    for (int32_t j = 0; j < g.samplesPerFrame; ++j) {
                        samples[j] = static_cast<Sample>(ftol(static_cast<double>(samples[j]) * h.volume));
                    }
                }
                if (captureFilter != nullptr) {
                    captureFilter(this, samples, static_cast<FrameStamp>(h.captureClock + i));
                }
                g.codec.encode(packet + static_cast<size_t>(g.encodedFrameSize) * static_cast<size_t>(i), samples);
                if (left > static_cast<uint32_t>(g.bytesPerFrame)) {
                    left -= static_cast<uint32_t>(g.bytesPerFrame);
                    samples += g.samplesPerFrame;
                } else {
                    left = b2;
                    samples = static_cast<Sample*>(p2);
                }
            }
        }
        if (FAILED(h.captureBuffer->Unlock(p1, b1, p2, b2))) return fail();
    }
    h.readPos = (h.readPos + lockBytes) % h.captureSize;
    h.captureClock = static_cast<FrameStamp>(h.captureClock + frames);
    *len = g.encodedFrameSize * frames;
    return result != 0;
}

void Device::playPacket(const uint8_t* packet, int32_t len, Source source, FrameStamp stamp) {
    if (data.playing == 0) return;
    addPacket(*data.sources, source, packet, len, stamp, data.clock);
}

// 0x005806A0: the first source in use of the id: whether it talked.
bool Device::isSourceTalking(Source source) {
    if (data.playing == 0) return false;
    for (const SourceEntry& s : data.sources->sources) {
        if (s.inUse != 0 && s.id == source) return s.talking != 0;
    }
    return false;
}

// 0x005806D0: the sources in use that talked, up to max; their count.
int32_t Device::listTalkingSources(Source* sources, int32_t max) {
    if (data.playing == 0) return 0;
    int32_t count = 0;
    for (const SourceEntry& s : data.sources->sources) {
        if (s.inUse != 0 && s.talking != 0) {
            sources[count] = s.id;
            ++count;
            if (count == max) break;
        }
    }
    return count;
}

// ---- The SDK.

bool startup(HWND window) {
    CoInitialize(nullptr);
    g.format = WAVEFORMATEX{};
    g.format.wFormatTag = WAVE_FORMAT_PCM;
    g.format.nChannels = 1;
    g.format.nSamplesPerSec = 8000;
    g.format.wBitsPerSample = 16;
    g.format.nBlockAlign = 2;
    g.format.nAvgBytesPerSec = 16000;
    if (window == nullptr) {
        window = GetForegroundWindow();
        if (window == nullptr) window = GetDesktopWindow();
    }
    g.window = window;
    g.devices.clear();
    g.devices.reserve(2);
    g.listed = true;
    return true;
}

void cleanup() {
    if (g.listed) {
        for (Device* d : g.devices) destroyDevice(d);
        g.devices.clear();
        g.listed = false;
    }
    CoUninitialize();
    freeFramePool();
}

void think() {
    if (!g.listed) return;
    for (int32_t i = static_cast<int32_t>(g.devices.size()) - 1; i >= 0; --i) {
        if (static_cast<size_t>(i) >= g.devices.size()) continue;
        Device* d = g.devices[static_cast<size_t>(i)];
        if (d->types != kPlayback) continue;
        if (!playbackThink(*d) && d->unplugged != nullptr) {
            d->unplugged(d);
            d->free();
        }
    }
}

void setCustomCodec(const CodecInfo& codec) {
    g.codec = codec;
    g.samplesPerFrame = codec.samplesPerFrame;
    g.encodedFrameSize = codec.encodedFrameSize;
    g.bytesPerFrame = codec.samplesPerFrame * 2;
    makeFramePool();
}

Device* newDevice(const std::array<uint8_t, 16>& id, int32_t type) {
    if (type != kCapture && type != kPlayback) return nullptr;
    // 0x00580500: the device and its data zeroed.
    auto* device = new Device();
    device->id = id;
    device->types = type;
    Device::Data& h = device->data;
    GUID guid{};
    std::memcpy(&guid, id.data(), 16);
    if (type == kPlayback) {
        h.sources = newSourceList();
        if (FAILED(DirectSoundCreate(&guid, &h.sound, nullptr))) {
            freeSources(*h.sources);
            delete h.sources;
            delete device;
            return nullptr;
        }
        if (!makePlaybackBuffer(h)) {
            freeSources(*h.sources);
            delete h.sources;
            h.sound->Release();
            delete device;
            return nullptr;
        }
    } else {
        if (FAILED(DirectSoundCaptureCreate(&guid, &h.capture, nullptr))) {
            delete device;
            return nullptr;
        }
        if (!makeCaptureBuffer(h)) {
            h.capture->Release();
            delete device;
            return nullptr;
        }
        h.captureClock = 0;
        h.volume = 1.0;
        h.lastAbove = static_cast<FrameStamp>(0xFFEB);
    }
    // ArrayAppend (0x0056D8A0).
    if (g.listed) g.devices.push_back(device);
    return device;
}

int32_t samplesPerFrame() {
    return g.samplesPerFrame;
}

int32_t encodedFrameSize() {
    return g.encodedFrameSize;
}

int32_t bytesPerFrame() {
    return g.bytesPerFrame;
}

// ---- Speex.

bool speexInitialize(int32_t quality) {
    if (sp.initialized != 0) return false;
    sp.encoder = speex_encoder_init(&speex_nb_mode);
    if (sp.encoder == nullptr) return false;
    speex_encoder_ctl(sp.encoder, SPEEX_SET_QUALITY, &quality);
    speex_bits_init(&sp.bits);
    int32_t bitrate = 0;
    speex_encoder_ctl(sp.encoder, SPEEX_GET_BITRATE, &bitrate);
    sp.encodedSize = bitrate / 50 / 8;
    sp.initialized = 1;
    return true;
}

void speexCleanup() {
    if (sp.initialized == 0) return;
    speex_encoder_destroy(sp.encoder);
    sp.encoder = nullptr;
    speex_bits_destroy(&sp.bits);
    sp.initialized = 0;
}

bool speexNewDecoder(void** data) {
    void* decoder = speex_decoder_init(&speex_nb_mode);
    if (decoder == nullptr) return false;
    int32_t enhance = 1;
    speex_decoder_ctl(decoder, SPEEX_SET_ENH, &enhance);
    *data = decoder;
    return true;
}

void speexFreeDecoder(void* data) {
    speex_decoder_destroy(data);
}

// The frame's 160 samples as floats, the bits reset, encoded and written (the encoded frame's bytes at most).
void speexEncode(uint8_t* out, const Sample* in) {
    float samples[160];
    for (int32_t i = 0; i < 160; ++i) samples[i] = static_cast<float>(in[i]);
    speex_bits_reset(&sp.bits);
    speex_encode(sp.encoder, samples, &sp.bits);
    speex_bits_write(&sp.bits, reinterpret_cast<char*>(out), sp.encodedSize);
}

// The encoded frame read and decoded; each sample (cut toward zero) added to out's in 16 bits.
void speexDecodeAdd(Sample* out, const uint8_t* in, void* data) {
    speex_bits_read_from(&sp.bits, reinterpret_cast<char*>(const_cast<uint8_t*>(in)), sp.encodedSize);
    float samples[160];
    speex_decode(data, &sp.bits, samples);
    for (int32_t i = 0; i < 160; ++i) {
        out[i] = static_cast<Sample>(out[i] + static_cast<Sample>(ftol(static_cast<double>(samples[i]))));
    }
}

void speexResetEncoder() {
    // Stock problem: the encoder is used as it is ([0x0075D98C]); none (the codec never made) fails at once.
    if (sp.encoder != nullptr) speex_encoder_ctl(sp.encoder, SPEEX_RESET_STATE, nullptr);
}

int32_t speexEncodedFrameSize() {
    return sp.encodedSize;
}

}  // namespace oj::net::gv
