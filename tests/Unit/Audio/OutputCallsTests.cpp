// Unit tests: the calls the sound library's logic makes of its output (Audio/Output.h), in the stock's order, on an
// output of the tests that writes them down and answers as it is told: a set's buffers made (Audio/Bank.cpp,
// 0x005C33C0), a voice played (0x005C3850: its lost buffer filled again, the hardware's play tried again in the
// software) and its occlusion given as EAX (0x005C22D0).

#include "Framework/Test.h"

#include "Audio/Bank.h"
#include "Audio/Eax.h"
#include "Audio/Output.h"
#include "Audio/SoundSystem.h"

#include <windows.h>
#include <mmreg.h>

#include <cstring>
#include <deque>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

using oj::test::check;
using oj::audio::Answer;

// The calls written down, one string each, and the answers given: a queue per buffer (its calls' first word, the
// buffer's number) in the order of its calls, Ok when empty.
struct Calls {
    std::vector<std::string> made;
    std::map<std::string, std::deque<Answer>> answers;
    Answer answer(const std::string& call) {
        made.push_back(call);
        const std::string name = call.substr(0, call.find(' '));
        auto& q = answers[name];
        if (q.empty()) return Answer::Ok;
        const Answer a = q.front();
        q.pop_front();
        return a;
    }
};

class RecordingBuffer final : public oj::audio::OutputBuffer {
public:
    RecordingBuffer(Calls& calls, int id) : m_calls(calls), m_id(id) {}
    ~RecordingBuffer() override { m_calls.made.push_back(std::format("{} released", m_id)); }
    Answer write(uint32_t at, const void*, uint32_t bytes) override { return call(std::format("write {} {}", at, bytes)); }
    Answer restore() override { return call("restore"); }
    Answer acquireHardware() override { return call("acquireHardware"); }
    Answer play(uint32_t flags) override { return call(std::format("play {}", flags)); }
    Answer stop() override { return call("stop"); }
    Answer setPosition(uint32_t bytes) override { return call(std::format("setPosition {}", bytes)); }
    Answer cursors(uint32_t& play, uint32_t& write) override {
        play = write = 0;
        return call("cursors");
    }
    Answer setVolume(int32_t mb) override { return call(std::format("setVolume {}", mb)); }
    Answer setPan(int32_t mb) override { return call(std::format("setPan {}", mb)); }
    Answer setFrequency(uint32_t hz) override { return call(std::format("setFrequency {}", hz)); }
    Answer status(uint32_t& flags) override {
        flags = 0;
        return call("status");
    }
    bool open3d() override {
        call("open3d");
        return true;
    }
    void reset3d() override { call("reset3d"); }
    void place3d(float x, float y, float z) override { call(std::format("place3d {} {} {}", x, y, z)); }
    bool openEax() override {
        m_eax = call("openEax") == Answer::Ok;
        return m_eax;
    }
    bool hasEax() const override { return m_eax; }
    Answer setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes) override {
        const bool source = std::memcmp(&set, &oj::audio::eax::kSource, sizeof set) == 0;
        int32_t value = 0;
        if (bytes == 4) std::memcpy(&value, data, 4);
        return call(std::format("setEax {} {} {}", source ? "source" : "other", id, bytes == 4 ? value : 0));
    }

private:
    Answer call(const std::string& what) { return m_calls.answer(std::format("{} {}", m_id, what)); }
    Calls& m_calls;
    int m_id;
    bool m_eax = false;
};

class RecordingOutput final : public oj::audio::Output {
public:
    RecordingOutput(Calls& calls, uint32_t eaxFlags) : m_calls(calls), m_eaxFlags(eaxFlags) {}
    bool start(HWND, uint32_t flags, oj::audio::OutputStart& result) override {
        result.flags = flags;
        result.eaxDevice = m_eaxFlags != 0;
        result.eaxFlags = m_eaxFlags;
        result.minFrequency = 100;
        result.hardware3dStreaming = 8;
        return true;
    }
    void stop() override {}
    bool hardwarePlays3d(uint32_t) override { return true; }
    bool setEax(const GUID& set, uint32_t id, const void* data, uint32_t bytes) override {
        const bool slot = std::memcmp(&set, &oj::audio::eax::kFxSlot0, sizeof set) == 0;
        int32_t value = 0;
        if (bytes == 4) std::memcpy(&value, data, 4);
        m_calls.made.push_back(std::format("listener setEax {} {} {}", slot ? "slot0" : "other", id, value));
        return true;
    }
    bool getEax(const GUID&, uint32_t, void*, uint32_t) override { return false; }
    std::unique_ptr<oj::audio::OutputBuffer> createBuffer(const oj::audio::SampleFormat& f, uint32_t bytes,
                                                          uint32_t caps) override {
        m_calls.made.push_back(std::format("createBuffer {} {} {:#x}", f.rate, bytes, caps));
        return std::make_unique<RecordingBuffer>(m_calls, m_next++);
    }
    std::unique_ptr<oj::audio::OutputBuffer> duplicate(oj::audio::OutputBuffer&) override {
        m_calls.made.push_back("duplicate");
        return std::make_unique<RecordingBuffer>(m_calls, m_next++);
    }
    std::unique_ptr<oj::audio::OutputListener> openListener() override { return nullptr; }
    std::unique_ptr<oj::audio::OutputStream> openStream(const std::filesystem::path&) override { return nullptr; }
    uintptr_t soundSystem() const override { return 1; }

private:
    Calls& m_calls;
    uint32_t m_eaxFlags;
    int m_next = 0;
};

// A system of the test's output for the test's length (the process's own back after it).
struct TestSystem {
    TestSystem(Calls& calls, uint32_t eaxFlags, uint32_t flags)
        : system(std::make_unique<RecordingOutput>(calls, eaxFlags)) {
        oj::audio::setSystem(&system);
        system.start(nullptr, flags);
    }
    ~TestSystem() { oj::audio::setSystem(nullptr); }
    oj::audio::System system;
};

// A sample of 1000 bytes of 22050 Hz mono 16 bits.
oj::audio::SampleEntry sample(std::vector<std::byte>& data) {
    oj::audio::SampleEntry e;
    data.assign(1000, std::byte{0});
    e.size = 1000;
    WAVEFORMATEX w{};
    w.wFormatTag = WAVE_FORMAT_PCM;
    w.nChannels = 1;
    w.nSamplesPerSec = 22050;
    w.nAvgBytesPerSec = 44100;
    w.nBlockAlign = 2;
    w.wBitsPerSample = 16;
    std::memcpy(e.format, &w, sizeof w);
    e.formatSize = sizeof w;
    e.data = data.data();
    return e;
}

bool same(const std::vector<std::string>& made, const std::vector<std::string>& expected) {
    if (made == expected) return true;
    for (const std::string& m : made) std::printf("     made: %s\n", m.c_str());
    return false;
}

}  // namespace

// A 2D set of two voices: the first buffer (0x400A0 with CTRLPAN) filled with the sample, the second its duplicate.
// The first played while its buffer is lost: the pan given, the hardware's resources asked for (no EAX), the position
// lost so the buffer restored, filled again and the position given again, then the volume, the frequency and the
// play in the software.
OJ_UNIT_TEST(OutputCalls, PlayLostBuffer) {
    Calls calls;
    std::vector<std::byte> data;
    oj::audio::SampleEntry entry = sample(data);
    {
        TestSystem test(calls, 0, 0x20);
        oj::audio::SoundSet set;
        set.setType(0);
        set.setInstances(2);
        oj::audio::VoiceSet voices(set, entry);
        const bool made = same(calls.made, {"createBuffer 22050 1000 0x400e0", "0 write 0 1000", "duplicate"});
        calls.made.clear();
        // A buffer's answers in the order of its calls: setPan, acquireHardware, setPosition lost.
        calls.answers["0"] = {Answer::Ok, Answer::Ok, Answer::Lost};
        voices.play(0);
        check(made, "the set's buffers: the first made and filled, the second duplicated");
        check(same(calls.made, {"0 setPan 0", "0 acquireHardware", "0 setPosition 0", "0 restore", "0 write 0 1000",
                                "0 setPosition 0", "0 setVolume 0", "0 setFrequency 48000", "0 play 4"}),
              "the lost buffer restored and filled again, then played in the software");
        calls.made.clear();
    }
    check(same(calls.made, {"0 released", "1 released"}) || same(calls.made, {"1 released", "0 released"}),
          "both buffers let go with the set");
}

// A 3D set of one voice on an EAX device of the voices' own EAX (0x10): the buffer's property set taken and its sound
// sent to FX slot 0, its 3D buffer given a new voice's parameters. Played with the hardware on, the hardware's play
// failing is tried again in the software; its occlusion goes to the voice's EAX room (EAXSOURCE_ROOM).
OJ_UNIT_TEST(OutputCalls, ThreeDimensional) {
    Calls calls;
    std::vector<std::byte> data;
    oj::audio::SampleEntry entry = sample(data);
    TestSystem test(calls, 0x10, 0x02);
    oj::audio::SoundSet set;
    set.setType(3);
    set.setInstances(1);
    oj::audio::VoiceSet voices(set, entry);
    check(same(calls.made, {"createBuffer 22050 1000 0x400b0", "0 write 0 1000", "0 openEax", "0 setEax source 27 0",
                            "0 open3d", "0 reset3d"}),
          "the 3D set's buffer: filled, its EAX to FX slot 0, its 3D parameters");
    calls.made.clear();
    calls.answers["0"] = {Answer::Ok, Answer::Ok, Answer::Failed};  // setPosition, setVolume, play in the hardware
    check(voices.play(0) == 0, "played");
    check(same(calls.made, {"0 setPosition 0", "0 setVolume 0", "0 play 2", "0 play 4"}),
          "the hardware's play failing, tried in the software");
    calls.made.clear();
    voices.setOcclusion(0, 1.0f);
    check(same(calls.made, {"0 setEax source 7 -1204"}), "the occlusion a step of 0.25: the voice's EAX room");
}

// The same on an EAX device without the voices' own EAX: the occlusion goes to FX slot 0's reverb room through the
// listener's property set (0x005C2392), a step toward a tenth (the fixed build holds it there: -2000).
OJ_UNIT_TEST(OutputCalls, OcclusionWithoutVoicesEax) {
    Calls calls;
    std::vector<std::byte> data;
    oj::audio::SampleEntry entry = sample(data);
    TestSystem test(calls, 0x08, 0x02);
    oj::audio::SoundSet set;
    set.setType(3);
    set.setInstances(1);
    oj::audio::VoiceSet voices(set, entry);
    calls.made.clear();
    voices.setOcclusion(0, 1.0f);
    check(same(calls.made, {"listener setEax slot0 5 -2000"}), "the reverb room of FX slot 0 for every voice");
}
