#include "Audio/Output.h"

#include "Audio/OpenAL/OpenAlOutput.h"

namespace oj::audio {

// The fixed build's output: OpenAL Soft (DirectSoundOutput not built).
std::unique_ptr<Output> makeOutput() {
    return std::make_unique<OpenAlOutput>();
}

}  // namespace oj::audio
