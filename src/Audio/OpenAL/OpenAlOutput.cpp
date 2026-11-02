#include "Audio/OpenAL/OpenAlOutput.h"

#include "Audio/Eax.h"
#include "Audio/SoundSystem.h"
#include "Core/Base/Log.h"

#define AL_LIBTYPE_STATIC
#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>
#include <AL/efx-presets.h>

// JuicedConfigExt's decoder, linked in (third_party/wmadec): its functions the library's own.
#define WMADEC_BUILD
#include "wmadec.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace oj::audio {

// The output's device, context and EFX reverb; the 3D sources the listener's rolloff goes to.
struct OpenAlOutput::State {
    ALCdevice* device = nullptr;
    ALCcontext* context = nullptr;
    bool efx = false;  // EAX on: the reverb in slot
    ALuint slot = 0;
    ALuint reverb = 0;
    float rolloff = 1.0f;
    std::mutex sources;
    std::vector<ALuint> sources3d;
};

namespace {

// The reverb's level: halfway from OpenAL Soft's EAX (through DSOAL) to the native EAX of Creative's cards, which
// recordings of the same race show much wetter (the stereo width, side against mid, -17.9 dB against -7.8 in the
// street). Here on Downtown_R1: -19.6 dB as OpenAL Soft's, -8.0 dB (the native's) at +16 dB, -13.4 dB at +10 dB.
// The Generic preset's reflections and late reverb times it, the room's gain (the game's EAX room) as it is.
constexpr float kReverbLevel = 3.1623f;  // +10 dB

float gainOf(int32_t mb) {
    if (mb <= -10000) return 0.0f;
    return std::pow(10.0f, static_cast<float>(std::min(mb, 0)) / 2000.0f);
}

ALenum formatOf(const SampleFormat& f) {
    if (f.tag != 1) return 0;
    if (f.channels == 1) return f.bits == 8 ? AL_FORMAT_MONO8 : f.bits == 16 ? AL_FORMAT_MONO16 : 0;
    if (f.channels == 2) return f.bits == 8 ? AL_FORMAT_STEREO8 : f.bits == 16 ? AL_FORMAT_STEREO16 : 0;
    return 0;
}

bool same(const GUID& a, const GUID& b) {
    return std::memcmp(&a, &b, sizeof a) == 0;
}

// A buffer's samples, shared by its duplicates (DuplicateSoundBuffer shares the memory). A ring is a buffer the game
// writes while it plays (DSBCAPS_GETCURRENTPOSITION2: the movies' sound): the mixer reads it through a callback.
struct Samples {
    ALuint buffer = 0;
    SampleFormat format;
    uint32_t bytes = 0;
    bool loaded = false;
    bool ring = false;
    std::vector<std::byte> data;
    std::atomic<uint32_t> readAt{0};
    ~Samples() {
        if (buffer != 0) alDeleteBuffers(1, &buffer);
    }
};

ALsizei AL_APIENTRY ringRead(ALvoid* user, ALvoid* out, ALsizei count) noexcept {
    auto* s = static_cast<Samples*>(user);
    auto* to = static_cast<std::byte*>(out);
    if (s->bytes == 0) {
        std::memset(out, 0, static_cast<size_t>(count));
        return count;
    }
    uint32_t at = s->readAt.load(std::memory_order_relaxed);
    auto left = static_cast<uint32_t>(count);
    while (left != 0) {
        const uint32_t n = std::min(left, s->bytes - at);
        std::memcpy(to, s->data.data() + at, n);
        to += n;
        left -= n;
        at = (at + n) % s->bytes;
    }
    s->readAt.store(at, std::memory_order_relaxed);
    return count;
}

// A voice's buffer: a source over the shared samples (2D: at the listener, relative; 3D: in the world).
class OpenAlBuffer final : public OutputBuffer {
public:
    OpenAlBuffer(std::shared_ptr<OpenAlOutput::State> state, std::shared_ptr<Samples> samples)
        : m_state(std::move(state)), m_samples(std::move(samples)) {
        alGenSources(1, &m_source);
        if (m_source == 0) {
            static bool told = false;
            if (!told) log::warning("sound: no OpenAL source left for a voice's buffer (it stays silent)");
            told = true;
        }
        alSourcei(m_source, AL_SOURCE_RELATIVE, AL_TRUE);
        alSource3f(m_source, AL_POSITION, 0.0f, 0.0f, 0.0f);
        alSourcef(m_source, AL_ROLLOFF_FACTOR, 0.0f);
    }
    ~OpenAlBuffer() override {
        alSourceStop(m_source);
        alSourcei(m_source, AL_BUFFER, 0);
        alDeleteSources(1, &m_source);
        if (m_filter != 0) alDeleteFilters(1, &m_filter);
        if (m_3d) {
            std::lock_guard lock(m_state->sources);
            std::erase(m_state->sources3d, m_source);
        }
    }

    const std::shared_ptr<Samples>& samples() const { return m_samples; }

    Answer write(uint32_t at, const void* data, uint32_t bytes) override {
        Samples& s = *m_samples;
        if (s.ring) {
            if (s.bytes == 0) return Answer::Ok;
            const auto* from = static_cast<const std::byte*>(data);
            uint32_t to = at % s.bytes;
            for (uint32_t done = 0; done < bytes;) {
                const uint32_t n = std::min(bytes - done, s.bytes - to);
                if (from != nullptr) {
                    std::memcpy(s.data.data() + to, from + done, n);
                } else {
                    std::memset(s.data.data() + to, 0, n);
                }
                done += n;
                to = (to + n) % s.bytes;
            }
            return Answer::Ok;
        }
        // A sample's buffer is written whole once, before a source plays it (refill after a lost buffer: no such
        // loss here).
        if (s.loaded || at != 0) return Answer::Ok;
        std::vector<std::byte> whole(s.bytes);
        if (data != nullptr) std::memcpy(whole.data(), data, std::min(bytes, s.bytes));
        alBufferData(s.buffer, formatOf(s.format), whole.data(), static_cast<ALsizei>(whole.size()),
                     static_cast<ALsizei>(s.format.rate));
        s.loaded = alGetError() == AL_NO_ERROR;
        return Answer::Ok;
    }
    Answer restore() override { return Answer::Ok; }
    Answer acquireHardware() override { return Answer::Ok; }
    Answer play(uint32_t flags) override {
        attach();
        m_looping = (flags & kPlayLooping) != 0;
        if (!m_samples->ring) alSourcei(m_source, AL_LOOPING, m_looping ? AL_TRUE : AL_FALSE);
        ALint state = 0;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);
        // DirectSound's Play of a playing buffer goes on (its flags taken); OpenAL's would start it again.
        if (state != AL_PLAYING) alSourcePlay(m_source);
        return Answer::Ok;
    }
    // DirectSound's Stop keeps the position (the next Play goes on from it): OpenAL's pause.
    Answer stop() override {
        alSourcePause(m_source);
        return Answer::Ok;
    }
    Answer setPosition(uint32_t bytes) override {
        if (m_samples->ring) {
            if (m_samples->bytes != 0) m_samples->readAt.store(bytes % m_samples->bytes);
            return Answer::Ok;
        }
        attach();
        alSourcei(m_source, AL_BYTE_OFFSET, static_cast<ALint>(bytes));
        return Answer::Ok;
    }
    Answer cursors(uint32_t& play, uint32_t& write) override {
        const Samples& s = *m_samples;
        if (s.ring) {
            play = s.readAt.load();
            // The write cursor a little ahead of the play cursor, as DirectSound's (20 ms).
            const uint32_t ahead = s.format.bytesPerSecond / 50 / std::max<uint32_t>(s.format.blockAlign, 1) *
                                   std::max<uint32_t>(s.format.blockAlign, 1);
            write = s.bytes != 0 ? (play + ahead) % s.bytes : 0;
            return Answer::Ok;
        }
        ALint offset = 0;
        alGetSourcei(m_source, AL_BYTE_OFFSET, &offset);
        play = write = static_cast<uint32_t>(std::max(offset, 0));
        return Answer::Ok;
    }
    Answer setVolume(int32_t mb) override {
        m_volume = gainOf(mb);
        applyGain();
        return Answer::Ok;
    }
    // A 2D voice's pan (DirectSound's: the other side attenuated by the hundredths of a decibel) as a place ahead of
    // the listener, to the side of the louder channel.
    Answer setPan(int32_t mb) override {
        if (m_3d) return Answer::Failed;
        const float left = mb > 0 ? gainOf(-mb) : 1.0f;
        const float right = mb < 0 ? gainOf(mb) : 1.0f;
        const float x = std::clamp(right - left, -1.0f, 1.0f);
        alSource3f(m_source, AL_POSITION, x, 0.0f, -std::sqrt(std::max(0.0f, 1.0f - x * x)));
        return Answer::Ok;
    }
    Answer setFrequency(uint32_t hz) override {
        const float rate = static_cast<float>(std::max<uint32_t>(m_samples->format.rate, 1));
        alSourcef(m_source, AL_PITCH, std::max(static_cast<float>(hz) / rate, 0.001f));
        return Answer::Ok;
    }
    Answer status(uint32_t& flags) override {
        ALint state = 0;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);
        flags = 0;
        if (state == AL_PLAYING) flags = kStatusPlaying | (m_looping ? kStatusLooping : 0u);
        return Answer::Ok;
    }
    bool open3d() override {
        if (!m_3d) {
            m_3d = true;
            alSourcei(m_source, AL_SOURCE_RELATIVE, AL_FALSE);
            std::lock_guard lock(m_state->sources);
            m_state->sources3d.push_back(m_source);
            alSourcef(m_source, AL_ROLLOFF_FACTOR, m_state->rolloff);
        }
        return true;
    }
    void reset3d() override {
        alSourcef(m_source, AL_REFERENCE_DISTANCE, 1.0f);
        alSourcef(m_source, AL_MAX_DISTANCE, 1e9f);
        alSource3f(m_source, AL_POSITION, 0.0f, 0.0f, 0.0f);
    }
    // DirectSound's places are left-handed (z ahead), OpenAL's right-handed (-z ahead).
    void place3d(float x, float y, float z) override { alSource3f(m_source, AL_POSITION, x, y, -z); }
    bool openEax() override {
        if (!m_state->efx) return false;
        if (m_filter == 0) {
            alGenFilters(1, &m_filter);
            alFilteri(m_filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
            alFilterf(m_filter, AL_LOWPASS_GAIN, 1.0f);
            alFilterf(m_filter, AL_LOWPASS_GAINHF, 1.0f);
        }
        return true;
    }
    bool hasEax() const override { return m_filter != 0; }
    // EAXSOURCE_ACTIVEFXSLOTID: the send to FX slot 0 (the reverb) or none; EAXSOURCE_ROOM: the send's gain.
    Answer setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes) override {
        if (m_filter == 0 || !same(set, eax::kSource)) return Answer::Failed;
        if (id == eax::kSourceActiveFxSlotId && bytes >= sizeof(GUID)) {
            GUID first{};
            std::memcpy(&first, data, sizeof first);
            m_sent = same(first, eax::kFxSlot0);
            applySend();
            return Answer::Ok;
        }
        if (id == eax::kSourceRoom && bytes == 4) {
            int32_t mb = 0;
            std::memcpy(&mb, data, 4);
            alFilterf(m_filter, AL_LOWPASS_GAIN, gainOf(mb));
            applySend();
            return Answer::Ok;
        }
        return Answer::Failed;
    }

private:
    void attach() {
        if (m_attached) return;
        alSourcei(m_source, AL_BUFFER, static_cast<ALint>(m_samples->buffer));
        m_attached = true;
    }
    void applyGain() { alSourcef(m_source, AL_GAIN, m_volume); }
    void applySend() {
        alSource3i(m_source, AL_AUXILIARY_SEND_FILTER, m_sent ? static_cast<ALint>(m_state->slot) : AL_EFFECTSLOT_NULL,
                   0, static_cast<ALint>(m_filter));
    }

    std::shared_ptr<OpenAlOutput::State> m_state;
    std::shared_ptr<Samples> m_samples;
    ALuint m_source = 0;
    ALuint m_filter = 0;
    bool m_attached = false;
    bool m_looping = false;
    bool m_3d = false;
    bool m_sent = false;
    float m_volume = 1.0f;
};

// The primary buffer's listener: its rolloff to every 3D source (DirectSound's is the listener's); a factor above
// DS3D_MAXROLLOFFFACTOR (10) refused as Windows' DirectSound refuses it.
class OpenAlListener final : public OutputListener {
public:
    explicit OpenAlListener(std::shared_ptr<OpenAlOutput::State> state) : m_state(std::move(state)) {}
    void setDistanceFactor(float) override {}  // the voices have no velocities: nothing it changes
    void setRolloffFactor(float factor) override {
        if (!(factor >= 0.0f && factor <= 10.0f)) return;
        std::lock_guard lock(m_state->sources);
        m_state->rolloff = factor;
        for (ALuint s : m_state->sources3d) alSourcef(s, AL_ROLLOFF_FACTOR, factor);
    }
    void setPosition(float x, float y, float z) override { alListener3f(AL_POSITION, x, y, -z); }
    void setOrientation(float fx, float fy, float fz, float ux, float uy, float uz) override {
        const ALfloat o[6] = {fx, fy, -fz, ux, uy, -uz};
        alListenerfv(AL_ORIENTATION, o);
    }
    void commit() override {}

private:
    std::shared_ptr<OpenAlOutput::State> m_state;
};

// A stream bank's file: decoded by the WMA decoder into a streaming source, fed by a thread of its own (the race's
// load holds the main thread for seconds). Positions in 100 ns units of the file's time.
class OpenAlStream final : public OutputStream {
public:
    explicit OpenAlStream(const std::filesystem::path& file) {
        m_wma = wmadec_open(file.wstring().c_str());
        if (m_wma == nullptr) {
            log::warning("sound: {} not decoded", file.string());
            return;
        }
        int rate = 0;
        int channels = 0;
        wmadec_format(m_wma, &rate, &channels, &m_duration);
        m_rate = static_cast<uint32_t>(std::max(rate, 1));
        m_channels = static_cast<uint32_t>(std::clamp(channels, 1, 2));
        alGenSources(1, &m_source);
        alSourcei(m_source, AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcef(m_source, AL_ROLLOFF_FACTOR, 0.0f);
        alGenBuffers(kBuffers, m_buffers);
        m_free.assign(m_buffers, m_buffers + kBuffers);
        m_thread = std::thread([this] { feed(); });
    }
    ~OpenAlStream() override {
        m_quit = true;
        if (m_thread.joinable()) m_thread.join();
        if (m_source != 0) {
            alSourceStop(m_source);
            alSourcei(m_source, AL_BUFFER, 0);
            alDeleteSources(1, &m_source);
            alDeleteBuffers(kBuffers, m_buffers);
        }
        if (m_wma != nullptr) wmadec_close(m_wma);
    }

    void stop() override {
        std::lock_guard lock(m_lock);
        m_running = false;
        if (m_source != 0) alSourcePause(m_source);
    }
    void run() override {
        std::lock_guard lock(m_lock);
        if (m_wma == nullptr) return;
        m_running = true;
        step();
    }
    bool setPositions(int64_t current, int64_t stop) override {
        std::lock_guard lock(m_lock);
        if (m_wma == nullptr) return false;
        alSourceStop(m_source);
        alSourcei(m_source, AL_BUFFER, 0);
        m_free.assign(m_buffers, m_buffers + kBuffers);
        m_queued.clear();
        wmadec_seek(m_wma, current);
        m_base = current;
        m_stopAt = stop;
        m_decoded = 0;
        m_played = 0;
        m_ended = false;
        m_hitEnd = false;
        return true;
    }
    bool seekable() const override { return m_wma != nullptr; }
    void positions(int64_t& current, int64_t& end) override {
        std::lock_guard lock(m_lock);
        end = m_duration;
        ALint offset = 0;
        alGetSourcei(m_source, AL_SAMPLE_OFFSET, &offset);
        const uint64_t frames = m_played + static_cast<uint64_t>(std::max(offset, 0));
        current = m_base + static_cast<int64_t>(frames * 10000000ull / m_rate);
        // Played to the end of what was asked for (or of the file): there, as DirectShow's position would be.
        if (m_ended && m_queued.empty()) current = m_hitEnd ? m_duration : std::max(current, m_stopAt);
    }
    void setVolume(int32_t mb) override {
        if (m_source != 0) alSourcef(m_source, AL_GAIN, gainOf(mb));
    }

private:
    static constexpr int kBuffers = 4;
    static constexpr int kFrames = 8192;  // a buffer's frames (some 0.19 s of 44.1 kHz)

    void feed() {
        while (!m_quit) {
            {
                std::lock_guard lock(m_lock);
                if (m_running) step();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    // The played buffers taken back, the free ones filled up to the stop, the source played again when it ran dry.
    void step() {
        ALint processed = 0;
        alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);
        while (processed-- > 0 && !m_queued.empty()) {
            ALuint b = 0;
            alSourceUnqueueBuffers(m_source, 1, &b);
            m_played += m_queued.front();
            m_queued.pop_front();
            m_free.push_back(b);
        }
        std::vector<int16_t> pcm(static_cast<size_t>(kFrames) * m_channels);
        while (!m_free.empty() && !m_ended) {
            int64_t want = kFrames;
            if (m_stopAt > m_base) {
                const int64_t lastFrame = (m_stopAt - m_base) * static_cast<int64_t>(m_rate) / 10000000;
                want = std::min<int64_t>(want, lastFrame - static_cast<int64_t>(m_decoded));
            }
            if (want <= 0) {
                m_ended = true;
                break;
            }
            const int got = wmadec_read(m_wma, pcm.data(), static_cast<int>(want));
            if (got <= 0) {
                m_ended = true;
                m_hitEnd = true;
                break;
            }
            const ALuint b = m_free.back();
            m_free.pop_back();
            alBufferData(b, m_channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, pcm.data(),
                         static_cast<ALsizei>(static_cast<size_t>(got) * m_channels * sizeof(int16_t)),
                         static_cast<ALsizei>(m_rate));
            alSourceQueueBuffers(m_source, 1, &b);
            m_queued.push_back(static_cast<uint64_t>(got));
            m_decoded += static_cast<uint64_t>(got);
        }
        ALint state = 0;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING && !m_queued.empty()) alSourcePlay(m_source);
    }

    WmaStream* m_wma = nullptr;
    uint32_t m_rate = 44100;
    uint32_t m_channels = 2;
    int64_t m_duration = 0;
    ALuint m_source = 0;
    ALuint m_buffers[kBuffers]{};
    std::vector<ALuint> m_free;
    std::deque<uint64_t> m_queued;  // the frames of each queued buffer
    std::mutex m_lock;
    std::thread m_thread;
    std::atomic<bool> m_quit{false};
    bool m_running = false;
    bool m_ended = false;
    bool m_hitEnd = false;
    int64_t m_base = 0;
    int64_t m_stopAt = 0;
    uint64_t m_decoded = 0;
    uint64_t m_played = 0;
};

// An output stream that plays nothing (no device).
class NoStream final : public OutputStream {
public:
    void stop() override {}
    void run() override {}
    bool setPositions(int64_t, int64_t) override { return false; }
    bool seekable() const override { return false; }
    void positions(int64_t& current, int64_t& end) override { current = end = 0; }
    void setVolume(int32_t) override {}
};

}  // namespace

OpenAlOutput::OpenAlOutput() : m(std::make_shared<State>()) {}

// Let go as the stock's system is at the process's end: the device stays (its thread ends with the process).
OpenAlOutput::~OpenAlOutput() = default;

bool OpenAlOutput::start(HWND, uint32_t flags, OutputStart& result) {
    result.flags = flags;
    if ((result.flags & kSoftware) != 0) result.flags &= ~kNoEax;
    result.flags = (result.flags & ~kHardware) | kSoftware;
    result.eaxFlags = 0;
    result.minFrequency = 100;
    result.hardware3dStreaming = 0;
    m->device = alcOpenDevice(nullptr);
    if (m->device == nullptr) {
        log::warning("sound: no OpenAL device");
        return false;
    }
    // A source for each buffer of a voice, as DirectSound's buffers: the game makes one for every voice of every set
    // it loads (a race some 300 and more, a set's instances each one), far past OpenAL Soft's default of 256 (the
    // buffers past it were silent: the race's own banks, the pause's menu sounds, the AI cars' engines); room for
    // mods and bigger grids (a source is small; the mixing's voices are made only for the ones playing).
    const ALCint attributes[] = {ALC_MAX_AUXILIARY_SENDS, 2, ALC_MONO_SOURCES, 16384, ALC_STEREO_SOURCES, 256, 0};
    m->context = alcCreateContext(m->device, attributes);
    if (m->context == nullptr || alcMakeContextCurrent(m->context) == ALC_FALSE) {
        log::warning("sound: no OpenAL context");
        if (m->context != nullptr) alcDestroyContext(m->context);
        alcCloseDevice(m->device);
        m->context = nullptr;
        m->device = nullptr;
        return false;
    }
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    alDopplerFactor(0.0f);
    if (alcIsExtensionPresent(m->device, "ALC_EXT_EFX") == ALC_TRUE) {
        // The game's EAX device with the voices' own EAX: FX slot 0 holds the reverb (EAX 4's Generic) at full volume,
        // silent with EAX off (the game's audio options switch it while it runs; the voices then made send nothing).
        alGetError();
        alGenEffects(1, &m->reverb);
        alEffecti(m->reverb, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
        const EFXEAXREVERBPROPERTIES g = EFX_REVERB_PRESET_GENERIC;
        alEffectf(m->reverb, AL_EAXREVERB_DENSITY, g.flDensity);
        alEffectf(m->reverb, AL_EAXREVERB_DIFFUSION, g.flDiffusion);
        alEffectf(m->reverb, AL_EAXREVERB_GAIN, g.flGain);
        alEffectf(m->reverb, AL_EAXREVERB_GAINHF, g.flGainHF);
        alEffectf(m->reverb, AL_EAXREVERB_GAINLF, g.flGainLF);
        alEffectf(m->reverb, AL_EAXREVERB_DECAY_TIME, g.flDecayTime);
        alEffectf(m->reverb, AL_EAXREVERB_DECAY_HFRATIO, g.flDecayHFRatio);
        alEffectf(m->reverb, AL_EAXREVERB_DECAY_LFRATIO, g.flDecayLFRatio);
        alEffectf(m->reverb, AL_EAXREVERB_REFLECTIONS_GAIN,
                  std::min(g.flReflectionsGain * kReverbLevel, AL_EAXREVERB_MAX_REFLECTIONS_GAIN));
        alEffectf(m->reverb, AL_EAXREVERB_REFLECTIONS_DELAY, g.flReflectionsDelay);
        alEffectf(m->reverb, AL_EAXREVERB_LATE_REVERB_GAIN,
                  std::min(g.flLateReverbGain * kReverbLevel, AL_EAXREVERB_MAX_LATE_REVERB_GAIN));
        alEffectf(m->reverb, AL_EAXREVERB_LATE_REVERB_DELAY, g.flLateReverbDelay);
        alEffectf(m->reverb, AL_EAXREVERB_ECHO_TIME, g.flEchoTime);
        alEffectf(m->reverb, AL_EAXREVERB_ECHO_DEPTH, g.flEchoDepth);
        alEffectf(m->reverb, AL_EAXREVERB_MODULATION_TIME, g.flModulationTime);
        alEffectf(m->reverb, AL_EAXREVERB_MODULATION_DEPTH, g.flModulationDepth);
        alEffectf(m->reverb, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, g.flAirAbsorptionGainHF);
        alEffectf(m->reverb, AL_EAXREVERB_HFREFERENCE, g.flHFReference);
        alEffectf(m->reverb, AL_EAXREVERB_LFREFERENCE, g.flLFReference);
        alEffectf(m->reverb, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, g.flRoomRolloffFactor);
        alEffecti(m->reverb, AL_EAXREVERB_DECAY_HFLIMIT, g.iDecayHFLimit);
        alGenAuxiliaryEffectSlots(1, &m->slot);
        alAuxiliaryEffectSloti(m->slot, AL_EFFECTSLOT_EFFECT, static_cast<ALint>(m->reverb));
        alAuxiliaryEffectSlotf(m->slot, AL_EFFECTSLOT_GAIN, (result.flags & kNoEax) == 0 ? 1.0f : 0.0f);
        m->efx = alGetError() == AL_NO_ERROR;
        if (m->efx) {
            result.eaxDevice = true;
            result.eaxFlags = 0x10;
        }
    }
    const ALCchar* name = alcGetString(m->device, ALC_ALL_DEVICES_SPECIFIER);
    log::info("sound: OpenAL Soft {} on {}{}, flags {:#x}", alGetString(AL_VERSION), name != nullptr ? name : "?",
              !m->efx ? "" : (result.flags & kNoEax) == 0 ? " (EAX reverb)" : " (EAX reverb, off)", result.flags);
    return true;
}

void OpenAlOutput::stop() {
    if (m->context == nullptr) return;
    if (m->slot != 0) alDeleteAuxiliaryEffectSlots(1, &m->slot);
    if (m->reverb != 0) alDeleteEffects(1, &m->reverb);
    m->slot = m->reverb = 0;
    m->efx = false;
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(m->context);
    alcCloseDevice(m->device);
    m->context = nullptr;
    m->device = nullptr;
}

void OpenAlOutput::enableEax(bool on) {
    if (!m->efx || m->slot == 0) return;
    alAuxiliaryEffectSlotf(m->slot, AL_EFFECTSLOT_GAIN, on ? 1.0f : 0.0f);
}

bool OpenAlOutput::hardwarePlays3d(uint32_t) {
    return true;
}

// The listener's EAX (the game's calls through the probe's property set, made when the device has no voices' own
// EAX): FX slot 0's volume and its reverb's room.
bool OpenAlOutput::setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes) {
    if (!m->efx || !same(set, eax::kFxSlot0)) return false;
    if (id == eax::kFxSlotAllParameters && bytes >= sizeof(eax::FxSlotProperties)) {
        eax::FxSlotProperties p{};
        std::memcpy(&p, data, sizeof p);
        alAuxiliaryEffectSlotf(m->slot, AL_EFFECTSLOT_GAIN, gainOf(p.volume));
        return true;
    }
    if (id == eax::kReverbRoom && bytes == 4) {
        int32_t mb = 0;
        std::memcpy(&mb, data, 4);
        alEffectf(m->reverb, AL_EAXREVERB_GAIN, gainOf(mb));
        alAuxiliaryEffectSloti(m->slot, AL_EFFECTSLOT_EFFECT, static_cast<ALint>(m->reverb));
        return true;
    }
    return false;
}

bool OpenAlOutput::getEax(const GUID&, uint32_t, void*, uint32_t) {
    return false;
}

std::unique_ptr<OutputBuffer> OpenAlOutput::createBuffer(const SampleFormat& format, uint32_t bytes, uint32_t caps) {
    if (m->context == nullptr || formatOf(format) == 0 || bytes == 0) return nullptr;
    auto samples = std::make_shared<Samples>();
    samples->format = format;
    samples->bytes = bytes;
    alGenBuffers(1, &samples->buffer);
    if ((caps & kCapsGetCurrentPosition2) != 0) {
        samples->ring = true;
        samples->data.assign(bytes, std::byte{0});
        alBufferCallbackSOFT(samples->buffer, formatOf(format), static_cast<ALsizei>(format.rate), ringRead,
                             samples.get());
        samples->loaded = true;
    }
    return std::make_unique<OpenAlBuffer>(m, std::move(samples));
}

std::unique_ptr<OutputBuffer> OpenAlOutput::duplicate(OutputBuffer& buffer) {
    if (m->context == nullptr) return nullptr;
    return std::make_unique<OpenAlBuffer>(m, static_cast<OpenAlBuffer&>(buffer).samples());
}

std::unique_ptr<OutputListener> OpenAlOutput::openListener() {
    if (m->context == nullptr) return nullptr;
    return std::make_unique<OpenAlListener>(m);
}

std::unique_ptr<OutputStream> OpenAlOutput::openStream(const std::filesystem::path& file) {
    if (m->context == nullptr) return std::make_unique<NoStream>();
    return std::make_unique<OpenAlStream>(file);
}

uintptr_t OpenAlOutput::soundSystem() const {
    return reinterpret_cast<uintptr_t>(m->context);
}

}  // namespace oj::audio
