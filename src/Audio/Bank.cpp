#include "Audio/Bank.h"

#include "Audio/Eax.h"
#include "Audio/SoundSystem.h"
#include "Audio/Stream.h"
#include "Core/Base/Log.h"
#include "Core/Base/Script.h"

#include <windows.h>
#include <mmreg.h>
#include <dsound.h>

#include <algorithm>
#include <cstring>

namespace oj::audio {

namespace {

uint32_t u32(const std::byte* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

// 0x005C2D80: a RIFF WAVE file: its fmt chunk (a PCM one's 16 bytes, another's 18 and its extra) and its data.
bool readWav(const std::vector<std::byte>& file, SampleEntry& e) {
    const std::byte* p = file.data();
    const std::byte* end = p + file.size();
    if (file.size() < 20 || u32(p) != 0x46464952 || u32(p + 8) != 0x45564157) return false;
    const std::byte* c = p + 12;
    while (u32(c) != 0x20746D66) {
        c = c + 8 + u32(c + 4);
        if (c + 8 > end) return false;
    }
    const uint32_t fmtSize = u32(c + 4);
    if (fmtSize < 16) return false;
    uint16_t tag;
    std::memcpy(&tag, c + 8, 2);
    uint16_t extra = 0;
    const std::byte* after = c + 8 + fmtSize;
    if (tag != WAVE_FORMAT_PCM) {
        std::memcpy(&extra, c + 8 + 16, 2);
        after = c + 8 + 18;
    }
    const uint32_t size = extra + 18u;
    std::memcpy(e.format, c + 8, std::min<size_t>(size, sizeof(e.format)));
    reinterpret_cast<WAVEFORMATEX*>(e.format)->cbSize = extra;
    e.formatSize = size;
    c = after + extra;
    while (u32(c) != 0x61746164) {
        c = c + 8 + u32(c + 4);
        if (c + 8 > end) return false;
    }
    e.size = u32(c + 4);
    e.data = c + 8;
    e.offset = -1;
    return true;
}

}  // namespace

uint32_t SampleEntry::rate() const { return wave().nSamplesPerSec; }
uint32_t SampleEntry::bytesPerSecond() const { return wave().nAvgBytesPerSec; }

// --- SoundSet ---

void SoundSet::setName(const std::string& name) {
    m_name = name.substr(0, 31);
    for (char& c : m_name) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
}

void SoundSet::setInstances(uint32_t count) {
    m_owner.assign(count, 0);
    m_start.assign(count, 0);
    m_volume.assign(count, 0.0f);
}

void SoundSet::acquire(uint32_t i) {
    const uint32_t bit = 1u << i;
    if ((m_busy & bit) != 0) return;
    ++m_busyCount;
    m_busy |= bit;
    markPlaying(i);
}

void SoundSet::release(uint32_t i) {
    const uint32_t bit = 1u << i;
    if ((m_busy & bit) != 0) {
        m_busy &= ~bit;
        if (m_busyCount != 0) --m_busyCount;
    }
    clearPlaying(i);
}

int SoundSet::findFree(uint32_t& voice) {
    for (uint32_t i = 0; i < instances(); ++i) {
        if (!busy(i)) {
            voice = i;
            acquire(i);
            return 0;
        }
    }
    return 4;
}

int SoundSet::stealQuietest(uint32_t& voice) const {
    // Scanned from the last voice down; the first of the lowest kept; the mean against the count as a float.
    const uint32_t n = instances();
    if (n == 0) return 4;
    float lowest = 1.0f;
    int found = -1;
    float sum = 0.0f;
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        if (m_volume[i] < lowest) {
            lowest = m_volume[i];
            found = i;
        }
        sum += m_volume[i];
    }
    if (lowest != (1.0f / static_cast<float>(n)) * sum && found != -1) {
        voice = static_cast<uint32_t>(found);
        return 1;
    }
    return 4;
}

int SoundSet::stealOldest(uint32_t& voice) const {
    uint32_t oldest = 0xFFFFFFFF;
    int found = -1;
    for (uint32_t i = 0; i < instances(); ++i) {
        if (m_start[i] != 0 && m_start[i] < oldest) {
            oldest = m_start[i];
            found = static_cast<int>(i);
        }
    }
    if (found == -1) return 4;
    voice = static_cast<uint32_t>(found);
    return 2;
}

// --- VoiceSet ---

VoiceSet::VoiceSet(SoundSet& set, const SampleEntry& entry) : m_set(set), m_entry(entry), m_type(set.type()) {
    m_voices.resize(set.instances());
    // 0x005C41D0: the sample's values given to the set.
    set.size = static_cast<float>(entry.size);
    set.rate = entry.rate();
    set.milliseconds = entry.bytesPerSecond() != 0 ? entry.size * 1000u / entry.bytesPerSecond() : 0;
    set.rate2 = entry.rate();
    set.bytesPerFrame = entry.rate() != 0 ? static_cast<float>(entry.bytesPerSecond()) / static_cast<float>(entry.rate())
                                          : 0.0f;
    if (m_type == 1 || m_type == 2) {
        // 0x005C3F10: a stream set's length (+0x08) and start: type 1 holds floats, type 2 whole seconds.
        if (m_type == 1) {
            std::memcpy(&set.size, &entry.size, 4);
            std::memcpy(&m_streamStart, &entry.start, 4);
        } else {
            m_streamStart = static_cast<float>(static_cast<int32_t>(entry.start));
        }
        return;
    }
    if (m_voices.empty()) return;
    System& sys = system();
    IDirectSound8* device = sys.device();
    if (device == nullptr) return;
    // 0x005C33C0: the first buffer (LOCDEFER, CTRLVOLUME, CTRLFREQUENCY, and CTRL3D for a 3D set else CTRLPAN) holds
    // the sample; the hardware that cannot play its 3D voices turns the software on (0x005C3150).
    if (!sys.hardwarePlays3d()) sys.setFlags((sys.flags() & ~kHardware) | kSoftware);
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = 0x400A0 | (m_type == 3 ? DSBCAPS_CTRL3D : DSBCAPS_CTRLPAN);
    desc.dwBufferBytes = entry.size;
    desc.lpwfxFormat = const_cast<WAVEFORMATEX*>(&entry.wave());
    IDirectSoundBuffer* first = nullptr;
    if (FAILED(device->CreateSoundBuffer(&desc, &first, nullptr)) || first == nullptr) {
        log::warning("sound: no buffer for {} ({} bytes)", set.name(), entry.size);
        return;
    }
    first->QueryInterface(IID_IDirectSoundBuffer8, reinterpret_cast<void**>(&m_voices[0].buffer));
    first->Release();
    if (m_voices[0].buffer == nullptr) return;
    refill(0);
    // 0x005C3582: a 3D set's voice, EAX not switched off (0x20), takes its buffer's property set (+0x28) and sends
    // its sound to FX slot 0 (EAXSOURCE_ACTIVEFXSLOTID: FX slot 0 and none); the answer is not looked at. The
    // duplicates likewise (0x005C36A1).
    const auto eaxSource = [this](HardwareVoice& v) {
        if (m_type != 3 || (system().flags() & kNoEax) != 0 || v.buffer == nullptr) return;
        if (v.buffer->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&v.properties)) < 0) return;
        GUID slots[2] = {eax::kFxSlot0, eax::kNull};
        v.properties->Set(eax::kSource, eax::kSourceActiveFxSlotId, nullptr, 0, slots, 0x20);
    };
    eaxSource(m_voices[0]);
    for (uint32_t i = 1; i < m_voices.size(); ++i) {
        IDirectSoundBuffer* copy = nullptr;
        if (FAILED(device->DuplicateSoundBuffer(m_voices[0].buffer, &copy)) || copy == nullptr) continue;
        copy->QueryInterface(IID_IDirectSoundBuffer8, reinterpret_cast<void**>(&m_voices[i].buffer));
        eaxSource(m_voices[i]);
        copy->Release();
    }
    if (m_type == 3) {
        // 0x005C1F40: every voice's 3D buffer, all its parameters deferred: at the listener, no cone, 1 .. 1e9.
        for (HardwareVoice& v : m_voices) {
            if (v.buffer == nullptr) continue;
            v.buffer->QueryInterface(IID_IDirectSound3DBuffer, reinterpret_cast<void**>(&v.buffer3d));
            if (v.buffer3d == nullptr) continue;
            DS3DBUFFER p{};
            p.dwSize = sizeof(p);
            p.dwInsideConeAngle = 360;
            p.dwOutsideConeAngle = 360;
            p.vConeOrientation = {0.0f, 0.0f, 1.0f};
            p.flMinDistance = 1.0f;
            p.flMaxDistance = 1e9f;
            p.dwMode = DS3DMODE_NORMAL;
            v.buffer3d->SetAllParameters(&p, DS3D_DEFERRED);
        }
    }
}

VoiceSet::~VoiceSet() {
    for (HardwareVoice& v : m_voices) {
        if (v.properties != nullptr) v.properties->Release();
        if (v.buffer3d != nullptr) v.buffer3d->Release();
        if (v.buffer != nullptr) {
            v.buffer->Stop();
            v.buffer->Release();
        }
    }
}

void VoiceSet::refill(uint32_t i) {
    IDirectSoundBuffer8* b = m_voices[i].buffer;
    if (b == nullptr || m_entry.data == nullptr) return;
    void* a = nullptr;
    void* a2 = nullptr;
    DWORD n = 0;
    DWORD n2 = 0;
    if (b->Lock(0, m_entry.size, &a, &n, &a2, &n2, 0) == DS_OK) {
        if (a != nullptr) std::memcpy(a, m_entry.data, std::min<DWORD>(n, m_entry.size));
        b->Unlock(a, n, a2, n2);
    }
}

int VoiceSet::play(uint32_t i) {
    if (isStream()) {
        // 0x005C3F80: the piece played from its start for the set's length.
        if (m_stream != nullptr) m_stream->play(m_streamStart, m_set.size);
        return 0;
    }
    HardwareVoice& v = m_voices[i];
    IDirectSoundBuffer8* b = v.buffer;
    if (b == nullptr) return 1;
    const uint32_t flags = system().flags();
    uint32_t play = v.playFlags;
    if (m_type == 3) {
        play |= (flags & kSoftware) == 0 ? DSBPLAY_LOCHARDWARE : DSBPLAY_LOCSOFTWARE;
    } else {
        b->SetPan(v.pan);
        play |= DSBPLAY_LOCSOFTWARE;
    }
    // Without a property set the voice asks for the hardware's resources; failing that, or with one and the hardware
    // off, it plays in the software.
    if (v.properties == nullptr) {
        if (FAILED(b->AcquireResources(DSBPLAY_LOCHARDWARE, 0, nullptr))) {
            play = (play & ~DSBPLAY_LOCHARDWARE) | DSBPLAY_LOCSOFTWARE;
        }
    } else if ((flags & kHardware) == 0) {
        play = (play & ~DSBPLAY_LOCHARDWARE) | DSBPLAY_LOCSOFTWARE;
    }
    if (b->SetCurrentPosition(0) == DSERR_BUFFERLOST) {
        log::info("sound: buffer lost");
        b->Restore();
        refill(i);
        b->SetCurrentPosition(0);
    }
    b->SetVolume(v.volume);
    if (m_type != 3) b->SetFrequency(v.frequency);
    HRESULT hr = b->Play(0, 0, play);
    if (hr == DSERR_BUFFERLOST) {
        b->Restore();
        refill(i);
        hr = b->Play(0, 0, play);
    }
    if (hr != DS_OK) {
        play = (play & ~DSBPLAY_LOCHARDWARE) | DSBPLAY_LOCSOFTWARE;
        hr = b->Play(0, 0, play);
        if (hr == DSERR_BUFFERLOST) {
            b->Restore();
            refill(i);
            hr = b->Play(0, 0, play);
        }
        if (hr != DS_OK) return 1;
    }
    return 0;
}

int VoiceSet::replay(uint32_t i) {
    HardwareVoice& v = m_voices[i];
    IDirectSoundBuffer8* b = v.buffer;
    if (b == nullptr) return 0;
    if (b->SetCurrentPosition(v.position) == DSERR_BUFFERLOST) {
        b->Restore();
        refill(i);
        b->SetCurrentPosition(v.position);
    }
    b->SetVolume(v.volume);
    if (m_type != 3) b->SetPan(v.pan);
    b->SetFrequency(v.frequency);
    if (b->Play(0, 0, v.playFlags) == DSERR_BUFFERLOST) {
        b->Restore();
        refill(i);
        b->Play(0, 0, v.playFlags);
    }
    return 0;
}

int VoiceSet::stop(uint32_t i) {
    if (isStream()) {
        if (m_stream != nullptr) m_stream->stop();  // 0x005C3FC0
        return 0;
    }
    if (m_voices[i].buffer != nullptr) m_voices[i].buffer->Stop();
    return 0;
}

int VoiceSet::pause(uint32_t i) {
    if (isStream()) {
        if (m_stream != nullptr) m_stream->pause(1);  // 0x005C3FE0
        return 0;
    }
    return stop(i);
}

int VoiceSet::resume(uint32_t i) {
    if (isStream()) {
        if (m_stream != nullptr) m_stream->pause(0);  // 0x005C4000
        return 0;
    }
    if (m_voices[i].buffer != nullptr) m_voices[i].buffer->Play(0, 0, m_voices[i].playFlags);
    return 0;
}

int VoiceSet::setVolume(uint32_t i, float volume) {
    const int32_t mb = volumeMb(volume);
    HardwareVoice& v = m_voices[i];
    if (isStream()) {
        // 0x005C4020: given every time.
        v.volume = mb;
        if (m_stream != nullptr) m_stream->setVolume(mb);
        return 0;
    }
    if (v.volume != mb) {
        v.volume = mb;
        if (v.buffer != nullptr) v.buffer->SetVolume(mb);
    }
    return 0;
}

int VoiceSet::setFrequency(uint32_t i, uint32_t request) {
    // fVar3 * fVar2 * (1/48000) in single precision, rounded (fistp).
    const float hz = static_cast<float>(request) * static_cast<float>(m_set.rate) * (1.0f / 48000.0f);
    uint32_t f = static_cast<uint32_t>(static_cast<int64_t>(std::nearbyint(hz)));
    HardwareVoice& v = m_voices[i];
    if (v.frequency == f) return 0;
    if (m_type == 3) {
        // 0x005C2220: kept as asked; the place (0x005C20B0) gives it.
        v.frequency = f;
        return 0;
    }
    if (isStream()) {
        // 0x005C4070: kept, nothing given.
        const uint32_t lo = system().minFrequency();
        v.frequency = f < lo ? lo : (f > 48000 ? 48000 : f);
        return 0;
    }
    if (m_type != 3) {
        const uint32_t lo = system().minFrequency();
        f = f < lo ? lo : (f > 48000 ? 48000 : f);
    }
    v.frequency = f;
    if (v.buffer != nullptr) v.buffer->SetFrequency(f);
    return 0;
}

int VoiceSet::status(uint32_t i) {
    if (isStream()) return m_stream != nullptr ? m_stream->state() : 0;  // 0x005C41B0
    DWORD s = 0;
    IDirectSoundBuffer8* b = m_voices[i].buffer;
    if (b == nullptr || b->GetStatus(&s) != DS_OK) return 0;
    if ((s & DSBSTATUS_LOOPING) != 0) return 4;
    if ((s & DSBSTATUS_PLAYING) != 0) return 3;
    return 0;
}

int VoiceSet::setPan(uint32_t i, float pan) {
    int32_t mb = 0;
    if (pan < 0.0f) {
        mb = volumeMb(1.0f - std::fabs(pan));
    } else if (pan > 0.0f) {
        mb = -volumeMb(1.0f - std::fabs(pan));
    }
    m_voices[i].pan = mb;
    if (m_voices[i].buffer != nullptr) m_voices[i].buffer->SetPan(mb);
    return 0;
}

int VoiceSet::place(uint32_t i, const float relative[3], const float change[3], float distance, float doppler) {
    HardwareVoice& v = m_voices[i];
    if (v.buffer3d != nullptr) {
        v.buffer3d->SetPosition(relative[0], relative[1], relative[2], DS3D_DEFERRED);
        // The stock then asks for the min distance [0x0079E9C8] (0x005C211E), which nothing writes: 0. Windows'
        // DirectSound refuses it (DSERR_INVALIDPARAM) and the buffer keeps its 1; Wine's takes it, and its distance
        // attenuation divides by it: every 3D voice (the cars, the world) silent under Proton. Not asked for, the
        // min distance stays 1 there as on Windows.
    }
    uint32_t f = v.frequency;
    if (doppler != 0.0f && distance != 0.0f) {
        const float radial = (relative[0] * change[0] + relative[1] * change[1] + relative[2] * change[2]) * doppler /
                             (3430.0f * distance);  // [0x0071E71C]
        const float c = radial < -0.3f ? -0.3f : (0.3f < radial ? 0.3f : radial);  // [0x0071E758]
        f = static_cast<uint32_t>(static_cast<int64_t>(static_cast<float>(v.frequency) / (c + 1.0f)));
        v.frequency = f;
    }
    const uint32_t lo = system().minFrequency();
    f = f < lo ? lo : (48000 < f ? 48000 : f);
    v.frequency = f;
    if (v.given != f) {
        v.given = f;
        if (v.buffer != nullptr) v.buffer->SetFrequency(f);
    }
    return 0;
}

void VoiceSet::setPosition(uint32_t i, uint32_t position) {
    m_voices[i].position = position;
    if (m_voices[i].buffer != nullptr) m_voices[i].buffer->SetCurrentPosition(position);
}

int VoiceSet::setOcclusion(uint32_t i, float v) {
    // The 2D set's slot (0x005285D0) does nothing. Stock problem: a stream set has no slot +0x88 (its table ends at
    // +0x84; the next table's first entry, a destructor, would be called), never reached: streams are not 3D voices.
    if (m_type != 3) return 0;
    // [0x00725858]: 0.5 while the device gives each voice its own EAX, else [0x0072585C] 0.1 from the first call on.
    static float scale = 0.5f;
    const System& sys = system();
    if ((sys.eaxFlags() & 0x10) == 0) scale = 0.1f;
    // x87: the product of two floats is exact in double as in the extended register.
    const double value = static_cast<double>(scale) * static_cast<double>(v);
    HardwareVoice& h = m_voices[i];
    if (h.properties == nullptr) return 0;
    // Steps of 0.25 toward the value, not held at it; equal (or not a number) unchanged.
    if (value < static_cast<double>(h.occlusion)) {
        h.occlusion = h.occlusion - 0.25f;  // [0x006FFBEC]
    } else if (value > static_cast<double>(h.occlusion)) {
        h.occlusion = h.occlusion + 0.25f;
    }
    int32_t mb = volumeMb(h.occlusion);
    if ((sys.eaxFlags() & 0x10) != 0) {
        h.properties->Set(eax::kSource, eax::kSourceRoom, nullptr, 0, &mb, 4);
    } else if (IKsPropertySet* listener = sys.eaxProperties()) {
        // 0x005C2392: the device without voices' own EAX: the reverb's room of FX slot 0 for every voice.
        listener->Set(eax::kFxSlot0, eax::kReverbRoom, nullptr, 0, &mb, 4);
    }
    return 0;
}

// --- Bank ---

Bank::Bank(int type, std::filesystem::path folder) : m_type(type), m_folder(std::move(folder)) {}

Bank::~Bank() = default;

int Bank::findSet(const char* name) const {
    for (size_t i = 0; i < m_sets.size(); ++i) {
        if (m_sets[i] && _stricmp(name, m_sets[i]->name().c_str()) == 0) return static_cast<int>(i);
    }
    return -1;
}

int Bank::findSample(const char* name) const {
    for (size_t i = 0; i < m_declared.size(); ++i) {
        if (_stricmp(name, m_declared[i].c_str()) == 0) return static_cast<int>(i);
    }
    return -1;
}

std::unique_ptr<Bank> Bank::load(const GameFiles& files, const std::filesystem::path& folder, const std::string& name,
                                 uint32_t* channels) {
    const auto cfg = files.read(folder / (name + ".cfg"));
    if (!cfg) return nullptr;
    const std::string_view text(reinterpret_cast<const char*>(cfg->data()), cfg->size());
    // The two tables of keywords, the same handlers: a player's banks (0x005BEE80, its loader's table 0x00725160:
    // [Begin], CHANNELS, .., [End]) and those added to the 3D player (0x005B9430 with 0x005BAF00, the table 0x00725060:
    // [BeginBank], .., [EndBank]); a keyword not of the file's table ("trigger", the other table's ends) is passed
    // over, so the samples are made once.
    const bool playerTable = channels != nullptr;
    int bankSize = 0;
    int instanceCount = 0;
    std::string bankName;
    std::unique_ptr<Bank> bank;
    uint32_t setIndex = 0;
    SoundSet* current = nullptr;
    for (const ScriptLine& line : parseScript(text)) {
        const std::string& k = line.keyword;
        auto is = [&](const char* s) { return _stricmp(k.c_str(), s) == 0; };
        if (is("[Begin]") && playerTable) {
            setIndex = 0;
        } else if (is("CHANNELS") && playerTable) {
            if (channels != nullptr) *channels = (static_cast<uint32_t>(line.integer(0)) & 0xFFFF) + 1;
        } else if (is("bank_size")) {
            bankSize = line.integer(0);
        } else if (is("instance_count")) {
            instanceCount = line.integer(0);
        } else if (is("bank_name")) {
            bankName = line.word(0);
        } else if (is("banktype")) {
            // 0x005BE9B0 → 0x005BA370: the bank of the type, its sets (bank_size), the first current.
            if (!bank) bank = std::make_unique<Bank>(line.integer(0), folder);
            bank->m_type = line.integer(0);
            bank->m_name = bankName;
            bank->m_file = name;
            bank->m_sets.clear();
            for (int i = 0; i < bankSize; ++i) bank->m_sets.push_back(std::make_unique<SoundSet>());
            current = bank->set(0);
            // The bank's arrays of instances (vt+0x08 = 0x005BA790: instance_count x the loader's +0x18 — 1 for a
            // player's bank, cruise, wet and the dyno's, twice the cars of the bank in a race, 0x0049835D) are only a
            // pool the sets take from without a check (0x005BA900 ..); each set keeps its own "instances".
            (void)instanceCount;
        } else if (is("declare_sample") || is("declare_stream")) {
            // 0x005BAB40: a name declared once.
            if (bank && bank->findSample(line.word(0).c_str()) < 0) bank->m_declared.push_back(line.word(0));
        } else if (is("[BeginSS]")) {
            if (current != nullptr) current->setBank(bank.get());
        } else if (is("name")) {
            if (current != nullptr) current->setName(line.word(0));
        } else if (is("displayname")) {
            if (current != nullptr) current->setDisplayName(line.word(0));
        } else if (is("sample")) {
            if (current != nullptr && bank) current->setSample(bank->findSample(line.word(0).c_str()));
        } else if (is("instances")) {
            if (current != nullptr) current->setInstances(static_cast<uint32_t>(line.integer(0)));
        } else if (is("type")) {
            if (current != nullptr) current->setType(line.integer(0));
        } else if (is("[EndSS]")) {
            ++setIndex;
            current = bank ? bank->set(setIndex) : nullptr;
        } else if ((is("[EndBank]") && !playerTable) || (is("[End]") && playerTable)) {
            if (bank) {
                bank->makeVoices(files);
                log::info("sound: bank {} (type {}): {} sets, {} samples", bank->m_name, bank->m_type, bank->count(),
                          bank->m_entries.size());
            }
        }
    }
    return bank;
}

void Bank::makeVoices(const GameFiles& files) {
    // 0x005BE000: <the .cfg's name>.dat (+0x50; the bank_name of music.cfg is "Juiced", of cruise.cfg "3d"), its
    // entries' data pointers set (+0x8C = the file + +0x88); without it every set's sample from <sample>.wav
    // (0x005C1CE0).
    const auto dat = files.read(m_folder / (m_file + ".dat"));
    if (dat && dat->size() >= 4) {
        m_dat = *dat;
        const uint32_t count = u32(m_dat.data());
        m_entries.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            const std::byte* p = m_dat.data() + 4 + i * 0xA4;
            if (p + 0xA4 > m_dat.data() + m_dat.size()) break;
            SampleEntry& e = m_entries[i];
            e.size = u32(p);
            e.formatSize = u32(p + 4);
            std::memcpy(e.format, p + 8, sizeof(e.format));
            e.offset = static_cast<int32_t>(u32(p + 0x88));
            e.start = u32(p + 0x90);
            if (e.offset >= 0 && static_cast<size_t>(e.offset) <= m_dat.size()) e.data = m_dat.data() + e.offset;
            const char* n = reinterpret_cast<const char*>(p + 0x94);
            e.name.assign(n, strnlen(n, 0xA4 - 0x94));
        }
    }
    if (m_type == 1 || m_type == 2) {
        // 0x005BE7A0: a stream bank's sets of type 1 and 2, then the stream of <name>.dsb (0x005BB1F0).
        bool any = false;
        for (auto& s : m_sets) {
            if (!s || (s->type() != 1 && s->type() != 2)) continue;
            if (s->sample() < 0 || static_cast<size_t>(s->sample()) >= m_entries.size()) continue;
            s->setVoices(std::make_unique<VoiceSet>(*s, m_entries[s->sample()]));
            any = true;
        }
        if (any) {
            m_stream = std::make_unique<Stream>(files.resolve(m_folder / (m_file + ".dsb")));
            for (auto& s : m_sets) {
                if (s && s->voices() != nullptr) s->voices()->setStream(m_stream.get());
            }
        }
        return;
    }
    const bool fromDat = !m_entries.empty();
    if (!fromDat) {
        m_entries.reserve(m_sets.size());
        m_wavs.reserve(m_sets.size());
    }
    for (auto& s : m_sets) {
        if (!s) continue;
        const SampleEntry* entry = nullptr;
        if (fromDat) {
            if (s->sample() >= 0 && static_cast<size_t>(s->sample()) < m_entries.size()) entry = &m_entries[s->sample()];
        } else if (s->sample() >= 0 && static_cast<size_t>(s->sample()) < m_declared.size()) {
            const auto wav = files.read(m_folder / (m_declared[s->sample()] + ".wav"));
            if (wav) {
                m_wavs.push_back(*wav);
                SampleEntry e;
                if (readWav(m_wavs.back(), e)) {
                    e.name = m_declared[s->sample()];
                    m_entries.push_back(e);
                    entry = &m_entries.back();
                }
            }
        }
        if (entry == nullptr) continue;
        s->setVoices(std::make_unique<VoiceSet>(*s, *entry));
    }
}

}  // namespace oj::audio
