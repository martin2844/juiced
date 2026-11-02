#include "Core/Network/GameSpy/VoiceSound.h"

#include "Core/Network/GameSpy/OpenAL/OpenAlVoice.h"

namespace oj::net::gv {

namespace {
VoiceSound* g_sound = nullptr;
}

// The fixed build's sound: OpenAL Soft (DirectSoundVoice not built).
std::unique_ptr<VoiceSound> makeVoiceSound() {
    return std::make_unique<OpenAlVoice>();
}

VoiceSound& voiceSound() {
    static const std::unique_ptr<VoiceSound> own = makeVoiceSound();
    return g_sound != nullptr ? *g_sound : *own;
}

void setVoiceSound(VoiceSound* sound) {
    g_sound = sound;
}

}  // namespace oj::net::gv
