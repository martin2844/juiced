// Unit tests: the calls the voice SDK makes of its sound (Core/Network/GameSpy/VoiceSound.h), in the stock's order, on
// a sound of the tests that writes them down: the devices listed (0x0057F190), a playback device made, started and
// mixed behind its play cursor (0x0057FA20, 0x0057F2C0, 0x0057EDD0), a capture device made and read (0x0057F590).

#include "Framework/Test.h"

#include "Core/Network/GameSpy/GameSpyVoice.h"
#include "Core/Network/GameSpy/VoiceSound.h"

#include <windows.h>
#include <mmreg.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <string>
#include <vector>

namespace {

using oj::test::check;
namespace gv = oj::net::gv;

bool same(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    return a == b;
}

gv::DeviceId idOf(uint8_t first) {
    gv::DeviceId id{};
    id[0] = first;
    return id;
}

// A looping buffer's bytes, locked as DirectSound locks them (the rest from the start past the end).
struct Ring {
    std::vector<uint8_t> bytes;
    bool lock(uint32_t at, uint32_t count, gv::Locked& locked) {
        const auto size = static_cast<uint32_t>(bytes.size());
        if (count == 0) {
            at = 0;
            count = size;
        }
        locked.first = bytes.data() + at;
        locked.firstBytes = std::min(count, size - at);
        locked.second = count > locked.firstBytes ? bytes.data() : nullptr;
        locked.secondBytes = count - locked.firstBytes;
        return true;
    }
};

class RecordingPlayback final : public gv::PlaybackBuffer {
public:
    RecordingPlayback(std::vector<std::string>& calls, uint32_t bytes) : m_calls(calls) { ring.bytes.assign(bytes, 0x55); }
    ~RecordingPlayback() override { m_calls.push_back("playback released"); }
    bool playCursor(uint32_t& play) override {
        m_calls.push_back("playCursor");
        play = cursor;
        return true;
    }
    bool lock(uint32_t at, uint32_t bytes, gv::Locked& locked) override {
        m_calls.push_back(std::format("lock {} {}", at, bytes));
        return ring.lock(at, bytes, locked);
    }
    bool unlock(const gv::Locked&) override {
        m_calls.push_back("unlock");
        return true;
    }
    void stop() override { m_calls.push_back("stop"); }
    bool rewind() override {
        m_calls.push_back("rewind");
        return true;
    }
    bool playLooping() override {
        m_calls.push_back("playLooping");
        return true;
    }
    bool playing() override { return true; }
    void setVolume(int32_t mb) override { m_calls.push_back(std::format("setVolume {}", mb)); }
    bool volume(int32_t& mb) override {
        mb = 0;
        return true;
    }

    Ring ring;
    uint32_t cursor = 0;

private:
    std::vector<std::string>& m_calls;
};

class RecordingCapture final : public gv::CaptureBuffer {
public:
    RecordingCapture(std::vector<std::string>& calls, uint32_t bytes) : m_calls(calls) { ring.bytes.assign(bytes, 0); }
    ~RecordingCapture() override { m_calls.push_back("capture released"); }
    bool readCursor(uint32_t& read) override {
        m_calls.push_back("readCursor");
        read = cursor;
        return true;
    }
    bool lock(uint32_t at, uint32_t bytes, gv::Locked& locked) override {
        m_calls.push_back(std::format("lock {} {}", at, bytes));
        return ring.lock(at, bytes, locked);
    }
    bool unlock(const gv::Locked&) override {
        m_calls.push_back("unlock");
        return true;
    }

    Ring ring;
    uint32_t cursor = 0;

private:
    std::vector<std::string>& m_calls;
};

// Two devices: 1 a microphone (capture and playback, the default capture device), 2 speakers (the default playback
// device); each enumeration starts with the primary driver (no GUID).
class RecordingSound final : public gv::VoiceSound {
public:
    RecordingSound() { gv::setVoiceSound(this); }
    ~RecordingSound() override { gv::setVoiceSound(nullptr); }

    void list(bool capture, const Listed& each) override {
        calls.push_back(capture ? "list capture" : "list playback");
        if (!each(nullptr, "Primary")) return;
        const gv::DeviceId mic = idOf(1);
        if (!each(&mic, "Microphone")) return;
        if (capture) return;
        const gv::DeviceId speakers = idOf(2);
        each(&speakers, "Speakers");
    }
    gv::DeviceId defaultDevice(bool capture) override {
        calls.push_back(capture ? "default capture" : "default playback");
        return idOf(capture ? 1 : 2);
    }
    std::unique_ptr<gv::PlaybackBuffer> openPlayback(const gv::DeviceId& id, HWND, const tWAVEFORMATEX& format,
                                                     uint32_t bytes) override {
        calls.push_back(std::format("openPlayback {} {} {}", id[0], format.nSamplesPerSec, bytes));
        auto b = std::make_unique<RecordingPlayback>(calls, bytes);
        playback = b.get();
        return b;
    }
    std::unique_ptr<gv::CaptureBuffer> openCapture(const gv::DeviceId& id, const tWAVEFORMATEX& format,
                                                   uint32_t bytes) override {
        calls.push_back(std::format("openCapture {} {} {}", id[0], format.nSamplesPerSec, bytes));
        auto b = std::make_unique<RecordingCapture>(calls, bytes);
        captured = b.get();
        return b;
    }

    std::vector<std::string> calls;
    RecordingPlayback* playback = nullptr;
    RecordingCapture* captured = nullptr;
};

// A codec of the tests: 160 samples a frame, 20 bytes encoded (the first sample's low byte in each).
bool newDecoder(void** data) {
    *data = nullptr;
    return true;
}
void freeDecoder(void*) {}
void encode(uint8_t* out, const gv::Sample* in) {
    std::memset(out, in[0] & 0xFF, 20);
}
void decode(gv::Sample* out, const uint8_t* in, void*) {
    for (int i = 0; i < 160; ++i) out[i] = static_cast<gv::Sample>(out[i] + in[0]);
}

void startWithCodec() {
    gv::startup(GetDesktopWindow());
    gv::CodecInfo codec;
    codec.samplesPerFrame = 160;
    codec.encodedFrameSize = 20;
    codec.newDecoder = newDecoder;
    codec.freeDecoder = freeDecoder;
    codec.encode = encode;
    codec.decode = decode;
    gv::setCustomCodec(codec);
}

}  // namespace

// The capture devices enumerated, then the playback devices (the primary driver passed over; the microphone, listed
// by both, takes the playback type of the second), then the default devices marked.
OJ_UNIT_TEST(VoiceCalls, ListDevices) {
    RecordingSound sound;
    const std::vector<gv::DeviceInfo> devices = gv::listDevices(10, gv::kCapture | gv::kPlayback);
    check(same(sound.calls, {"list capture", "list playback", "default capture", "default playback"}),
          "both enumerations, then both defaults");
    check(devices.size() == 2, "two devices");
    if (devices.size() == 2) {
        check(devices[0].id == idOf(1) && std::strcmp(devices[0].name, "Microphone") == 0 &&
                  devices[0].type == gv::kPlayback && devices[0].isDefault == 1,
              "the microphone: the second enumeration's type, the default capture device");
        check(devices[1].id == idOf(2) && std::strcmp(devices[1].name, "Speakers") == 0 &&
                  devices[1].type == gv::kPlayback && devices[1].isDefault == 2,
              "the speakers: the default playback device");
    }
    check(gv::listDevices(1, gv::kCapture | gv::kPlayback).size() == 1, "the enumeration stops at max");
}

// A playback device: its buffer of 3200 bytes (ten frames of 8000 Hz) made; started stopped, zeroed whole, rewound and
// played looping; a mix writes the bytes played since the last (the play cursor, a frame's bytes down) with the
// sources' frames (none: silence); freed, its buffer released.
OJ_UNIT_TEST(VoiceCalls, Playback) {
    RecordingSound sound;
    startWithCodec();
    gv::Device* device = gv::newDevice(idOf(2), gv::kPlayback);
    check(device != nullptr && same(sound.calls, {"openPlayback 2 8000 3200"}), "the buffer made of the device's GUID");
    if (device == nullptr || sound.playback == nullptr) {
        gv::cleanup();
        return;
    }
    sound.calls.clear();
    check(device->start(gv::kPlayback), "started");
    check(same(sound.calls, {"stop", "lock 0 0", "unlock", "rewind", "playLooping"}), "stopped, zeroed, played looping");
    check(std::all_of(sound.playback->ring.bytes.begin(), sound.playback->ring.bytes.end(),
                      [](uint8_t b) { return b == 0; }),
          "the whole buffer zeroed");

    std::fill(sound.playback->ring.bytes.begin(), sound.playback->ring.bytes.end(), uint8_t{0x55});
    sound.playback->cursor = 1000;  // 3 frames and some played
    sound.calls.clear();
    gv::think();
    check(same(sound.calls, {"playCursor", "lock 0 960", "unlock"}), "the three frames played written again");
    const auto& bytes = sound.playback->ring.bytes;
    check(std::all_of(bytes.begin(), bytes.begin() + 960, [](uint8_t b) { return b == 0; }) && bytes[960] == 0x55,
          "silence mixed into them, the rest left");

    sound.calls.clear();
    device->free();
    check(same(sound.calls, {"playback released"}), "freed: the buffer released");
    gv::cleanup();
}

// A capture device: its buffer of 16000 bytes made; a packet of the frames read since the last (the read cursor):
// locked from the read position, the peak at the threshold or above, each frame encoded; unlocked.
OJ_UNIT_TEST(VoiceCalls, Capture) {
    RecordingSound sound;
    startWithCodec();
    gv::Device* device = gv::newDevice(idOf(1), gv::kCapture);
    check(device != nullptr && same(sound.calls, {"openCapture 1 8000 16000"}),
          "the capture buffer made of the device's GUID");
    if (device == nullptr || sound.captured == nullptr) {
        gv::cleanup();
        return;
    }
    check(device->start(gv::kCapture), "capturing");
    device->setCaptureThreshold(0.15);
    auto* samples = reinterpret_cast<gv::Sample*>(sound.captured->ring.bytes.data());
    for (int i = 0; i < 320; ++i) samples[i] = static_cast<gv::Sample>(i < 160 ? 0x2001 : 0x2002);
    sound.captured->cursor = 700;  // two frames and some
    sound.calls.clear();
    uint8_t packet[100] = {};
    int32_t len = sizeof packet;
    gv::FrameStamp stamp = 0xFFFF;
    double volume = 0.0;
    const bool worth = device->capturePacket(packet, &len, &stamp, &volume);
    check(same(sound.calls, {"readCursor", "lock 0 640", "unlock"}), "the two frames locked from the read position");
    check(worth && len == 40 && stamp == 0 && packet[0] == 0x01 && packet[20] == 0x02,
          "loud enough: both frames encoded, the first's stamp");
    check(volume > 0.25 && volume < 0.26, "the volume the peak of the samples");

    sound.calls.clear();
    device->free();
    check(same(sound.calls, {"capture released"}), "freed: the buffer released");
    gv::cleanup();
}
