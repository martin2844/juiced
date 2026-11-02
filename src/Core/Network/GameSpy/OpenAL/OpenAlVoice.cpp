#include "Core/Network/GameSpy/OpenAL/OpenAlVoice.h"

#include <windows.h>
#include <mmreg.h>

#define AL_LIBTYPE_STATIC
#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace oj::net::gv {

namespace {

constexpr char kPrefix[] = "OpenAL Soft on ";

// The device's description as DirectSound gives the same endpoint's (so the names the game keeps in the profile stay
// the same): OpenAL Soft's name without its prefix, from its UTF-8 into the system's code page (DirectSoundEnumerateA's).
std::string describe(const std::string& name) {
    const std::string utf8 = name.rfind(kPrefix, 0) == 0 ? name.substr(sizeof kPrefix - 1) : name;
    const int wide = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wide <= 0) return utf8;
    std::wstring w(static_cast<size_t>(wide), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), wide);
    const int bytes = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return utf8;
    std::string ansi(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, ansi.data(), bytes, nullptr, nullptr);
    ansi.resize(static_cast<size_t>(bytes) - 1);
    return ansi;
}

// A GUID of the device's name: two FNV-1a hashes of 64 bits (two offsets).
DeviceId idOf(const std::string& name) {
    uint64_t a = 0xCBF29CE484222325ull;
    uint64_t b = 0x6C62272E07BB0142ull;
    for (const char c : name) {
        a = (a ^ static_cast<uint8_t>(c)) * 0x100000001B3ull;
        b = (b ^ static_cast<uint8_t>(c)) * 0x100000001B3ull;
    }
    DeviceId id{};
    std::memcpy(id.data(), &a, 8);
    std::memcpy(id.data() + 8, &b, 8);
    return id;
}

// The devices' names of the kind (OpenAL's list: strings one after another, an empty one at the end).
std::vector<std::string> namesOf(bool capture) {
    std::vector<std::string> names;
    const ALCchar* list = alcGetString(nullptr, capture ? ALC_CAPTURE_DEVICE_SPECIFIER : ALC_ALL_DEVICES_SPECIFIER);
    for (const ALCchar* p = list; p != nullptr && *p != 0; p += std::strlen(p) + 1) names.emplace_back(p);
    return names;
}

std::string nameOf(const DeviceId& id, bool capture) {
    for (const std::string& name : namesOf(capture)) {
        if (idOf(name) == id) return name;
    }
    return {};
}

// A device gone (unplugged): ALC_EXT_disconnect's ALC_CONNECTED.
bool connected(ALCdevice* device) {
    ALCint on = ALC_TRUE;
    alcGetIntegerv(device, ALC_CONNECTED, 1, &on);
    return on != ALC_FALSE;
}

// The voice's calls made on its own context, this thread's only (the game's sound keeps the process's).
class UseContext {
public:
    explicit UseContext(ALCcontext* context) : m_old(alcGetThreadContext()) { alcSetThreadContext(context); }
    ~UseContext() { alcSetThreadContext(m_old); }
    UseContext(const UseContext&) = delete;
    UseContext& operator=(const UseContext&) = delete;

private:
    ALCcontext* m_old;
};

// A looping buffer's bytes, locked as DirectSound locks them: the bytes from the place asked for, the rest from the
// start past the end (0 bytes: all of it).
Locked lockOf(std::vector<uint8_t>& bytes, uint32_t at, uint32_t count) {
    const auto size = static_cast<uint32_t>(bytes.size());
    Locked locked;
    if (count == 0) {
        at = 0;
        count = size;
    }
    at %= size;
    count = std::min(count, size);
    locked.first = bytes.data() + at;
    locked.firstBytes = std::min(count, size - at);
    if (count > locked.firstBytes) {
        locked.second = bytes.data();
        locked.secondBytes = count - locked.firstBytes;
    }
    return locked;
}

// The playback buffer: the mixer reads it through the callback from the play cursor on, around and around.
struct Ring {
    std::vector<uint8_t> bytes;
    std::atomic<uint32_t> readAt{0};
};

ALsizei AL_APIENTRY readRing(ALvoid* user, ALvoid* out, ALsizei count) noexcept {
    Ring& ring = *static_cast<Ring*>(user);
    const auto size = static_cast<uint32_t>(ring.bytes.size());
    auto* to = static_cast<uint8_t*>(out);
    uint32_t at = ring.readAt.load(std::memory_order_relaxed);
    auto left = static_cast<uint32_t>(count);
    while (left != 0) {
        const uint32_t n = std::min(left, size - at);
        std::memcpy(to, ring.bytes.data() + at, n);
        to += n;
        left -= n;
        at = (at + n) % size;
    }
    ring.readAt.store(at, std::memory_order_relaxed);
    return count;
}

ALenum formatOf(const WAVEFORMATEX& format) {
    if (format.wBitsPerSample == 8) return format.nChannels == 2 ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
    return format.nChannels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
}

// A playback device: its device and context, a source over the ring. DirectSound's Stop keeps the place (OpenAL's
// pause); the volume in hundredths of a decibel as the source's gain.
class OpenAlPlayback final : public PlaybackBuffer {
public:
    OpenAlPlayback(ALCdevice* device, ALCcontext* context, const WAVEFORMATEX& format, uint32_t bytes)
        : m_device(device), m_context(context) {
        m_ring.bytes.assign(bytes, 0);
        UseContext use(m_context);
        alGenBuffers(1, &m_buffer);
        alBufferCallbackSOFT(m_buffer, formatOf(format), static_cast<ALsizei>(format.nSamplesPerSec), readRing,
                             &m_ring);
        alGenSources(1, &m_source);
        alSourcei(m_source, AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcef(m_source, AL_ROLLOFF_FACTOR, 0.0f);
        alSourcei(m_source, AL_BUFFER, static_cast<ALint>(m_buffer));
    }
    ~OpenAlPlayback() override {
        {
            UseContext use(m_context);
            alSourceStop(m_source);
            alSourcei(m_source, AL_BUFFER, 0);
            alDeleteSources(1, &m_source);
            alDeleteBuffers(1, &m_buffer);
        }
        alcDestroyContext(m_context);
        alcCloseDevice(m_device);
    }

    bool playCursor(uint32_t& play) override {
        if (!connected(m_device)) return false;
        play = m_ring.readAt.load(std::memory_order_relaxed);
        return true;
    }
    bool lock(uint32_t at, uint32_t bytes, Locked& locked) override {
        locked = lockOf(m_ring.bytes, at, bytes);
        return true;
    }
    bool unlock(const Locked&) override { return true; }
    void stop() override {
        UseContext use(m_context);
        alSourcePause(m_source);
    }
    bool rewind() override {
        m_ring.readAt.store(0, std::memory_order_relaxed);
        return true;
    }
    bool playLooping() override {
        UseContext use(m_context);
        ALint state = 0;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) alSourcePlay(m_source);
        return true;
    }
    bool playing() override {
        UseContext use(m_context);
        ALint state = 0;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);
        return state == AL_PLAYING;
    }
    void setVolume(int32_t mb) override {
        m_volume = std::clamp(mb, -10000, 0);
        UseContext use(m_context);
        alSourcef(m_source, AL_GAIN, m_volume <= -10000 ? 0.0f : std::pow(10.0f, static_cast<float>(m_volume) / 2000.0f));
    }
    bool volume(int32_t& mb) override {
        mb = m_volume;
        return true;
    }

private:
    ALCdevice* m_device;
    ALCcontext* m_context;
    ALuint m_buffer = 0;
    ALuint m_source = 0;
    Ring m_ring;
    int32_t m_volume = 0;
};

// A capture device: OpenAL's capture, its samples taken into the looping buffer (the read cursor where they end) as
// the cursor is asked for.
class OpenAlCapture final : public CaptureBuffer {
public:
    OpenAlCapture(ALCdevice* device, uint32_t bytes, uint32_t frameBytes) : m_device(device), m_frameBytes(frameBytes) {
        m_bytes.assign(bytes, 0);
        alcCaptureStart(m_device);
    }
    ~OpenAlCapture() override {
        alcCaptureStop(m_device);
        alcCaptureCloseDevice(m_device);
    }

    bool readCursor(uint32_t& read) override {
        if (!connected(m_device)) return false;
        ALCint frames = 0;
        alcGetIntegerv(m_device, ALC_CAPTURE_SAMPLES, 1, &frames);
        const auto size = static_cast<uint32_t>(m_bytes.size());
        while (frames > 0) {
            const auto room = static_cast<ALCint>((size - m_writeAt) / m_frameBytes);
            const ALCint n = std::min(frames, room);
            alcCaptureSamples(m_device, m_bytes.data() + m_writeAt, n);
            m_writeAt = (m_writeAt + static_cast<uint32_t>(n) * m_frameBytes) % size;
            frames -= n;
        }
        read = m_writeAt;
        return true;
    }
    bool lock(uint32_t at, uint32_t bytes, Locked& locked) override {
        locked = lockOf(m_bytes, at, bytes);
        return true;
    }
    bool unlock(const Locked&) override { return true; }

private:
    ALCdevice* m_device;
    uint32_t m_frameBytes;
    std::vector<uint8_t> m_bytes;
    uint32_t m_writeAt = 0;
};

}  // namespace

void OpenAlVoice::list(bool capture, const Listed& each) {
    for (const std::string& name : namesOf(capture)) {
        const DeviceId id = idOf(name);
        if (!each(&id, describe(name).c_str())) return;
    }
}

DeviceId OpenAlVoice::defaultDevice(bool capture) {
    const ALCchar* name =
        alcGetString(nullptr, capture ? ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER : ALC_DEFAULT_ALL_DEVICES_SPECIFIER);
    if (name == nullptr || *name == 0) return {};
    return idOf(name);
}

std::unique_ptr<PlaybackBuffer> OpenAlVoice::openPlayback(const DeviceId& id, HWND, const tWAVEFORMATEX& format,
                                                          uint32_t bytes) {
    const std::string name = nameOf(id, false);
    if (name.empty() || bytes == 0) return nullptr;
    ALCdevice* device = alcOpenDevice(name.c_str());
    if (device == nullptr) return nullptr;
    ALCcontext* context = alcCreateContext(device, nullptr);
    if (context == nullptr) {
        alcCloseDevice(device);
        return nullptr;
    }
    return std::make_unique<OpenAlPlayback>(device, context, format, bytes);
}

std::unique_ptr<CaptureBuffer> OpenAlVoice::openCapture(const DeviceId& id, const tWAVEFORMATEX& format,
                                                        uint32_t bytes) {
    const std::string name = nameOf(id, true);
    const uint32_t frameBytes = std::max<uint32_t>(format.nBlockAlign, 1);
    if (name.empty() || bytes < frameBytes) return nullptr;
    ALCdevice* device = alcCaptureOpenDevice(name.c_str(), format.nSamplesPerSec, formatOf(format),
                                             static_cast<ALCsizei>(bytes / frameBytes));
    if (device == nullptr) return nullptr;
    return std::make_unique<OpenAlCapture>(device, bytes, frameBytes);
}

}  // namespace oj::net::gv
