// Integration tests: the fixed build's sound output (Audio/OpenAL/OpenAlOutput) on OpenAL Soft's null device (nothing
// is heard): a buffer for every voice, as DirectSound gives them.

#include "Framework/Test.h"

#include "Audio/OpenAL/OpenAlOutput.h"
#include "Audio/Output.h"

#include <windows.h>

#include <memory>
#include <vector>

namespace {

using oj::test::check;
using namespace oj::audio;

}  // namespace

// The game makes a buffer for each voice of every set it loads (a race some 300 and more): each plays. OpenAL Soft's
// default of 256 sources left the ones past it silent (the race's banks, the pause's menu sounds, the AI's engines).
OJ_INTEGRATION_TEST(OpenAlOutput, EveryVoicePlays) {
    // The null device when OpenAL Soft starts here (a process that started it before keeps its device).
    SetEnvironmentVariableW(L"ALSOFT_DRIVERS", L"null");
    OpenAlOutput output;
    OutputStart started;
    if (!output.start(nullptr, 0x20, started)) {
        check(false, "the output started");
        return;
    }
    SampleFormat format;  // 22050 Hz, mono, 16 bits
    std::vector<int16_t> samples(2205, 1000);
    std::vector<std::unique_ptr<OutputBuffer>> buffers;
    constexpr int kVoices = 600;
    for (int i = 0; i < kVoices; ++i) {
        std::unique_ptr<OutputBuffer> b =
            i % 2 == 0 || buffers.empty() ? output.createBuffer(format, 4410, kCapsCtrlVolume | kCapsCtrlFrequency)
                                          : output.duplicate(*buffers.back());
        if (b == nullptr) break;
        if (i % 2 == 0) b->write(0, samples.data(), 4410);
        buffers.push_back(std::move(b));
    }
    check(buffers.size() == kVoices, "600 buffers made (300 and their duplicates)");
    int playing = 0;
    for (std::unique_ptr<OutputBuffer>& b : buffers) {
        b->play(kPlayLooping);
        uint32_t status = 0;
        b->status(status);
        if ((status & kStatusPlaying) != 0) ++playing;
    }
    check(playing == kVoices, "every one plays");
    for (std::unique_ptr<OutputBuffer>& b : buffers) b->stop();
    buffers.clear();
    output.stop();
}
