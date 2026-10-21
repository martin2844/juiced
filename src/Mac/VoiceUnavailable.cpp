// GameSpy microphone capture is not part of the macOS single-player port.
// Report it unavailable so the engine never advertises working voice devices.
#include "Core/Base/Log.h"
#include "Core/Network/GameSpy/GameSpyVoice.h"
namespace oj::net::gv {
bool startup(HWND) {
    log::info("GameSpy voice capture is unavailable on macOS");
    return false;
}
void cleanup() {}
void think() {}
void setCustomCodec(const CodecInfo &) {}
std::vector<DeviceInfo> listDevices(int32_t, int32_t) {
    return {};
}
Device *newDevice(const std::array<uint8_t, 16> &, int32_t) {
    return nullptr;
}
int32_t samplesPerFrame() {
    return 0;
}
int32_t encodedFrameSize() {
    return 0;
}
int32_t bytesPerFrame() {
    return 0;
}
bool speexInitialize(int32_t) {
    return false;
}
void speexCleanup() {}
bool speexNewDecoder(void **) {
    return false;
}
void speexFreeDecoder(void *) {}
void speexEncode(uint8_t *, const Sample *) {}
void speexDecodeAdd(Sample *, const uint8_t *, void *) {}
void speexResetEncoder() {}
int32_t speexEncodedFrameSize() {
    return 0;
}
void Device::free() {
    delete this;
}
bool Device::start(int32_t) {
    return false;
}
void Device::stop(int32_t) {}
bool Device::isStarted(int32_t) {
    return false;
}
void Device::setVolume(int32_t, double) {}
double Device::volume(int32_t) {
    return 0;
}
void Device::setCaptureThreshold(double) {}
double Device::captureThreshold() const {
    return 0;
}
bool Device::capturePacket(uint8_t *, int32_t *n, FrameStamp *, double *) {
    if (n)
        *n = 0;
    return false;
}
int32_t Device::availableCaptureBytes() {
    return 0;
}
void Device::playPacket(const uint8_t *, int32_t, Source, FrameStamp) {}
bool Device::isSourceTalking(Source) {
    return false;
}
int32_t Device::listTalkingSources(Source *, int32_t) {
    return 0;
}
} // namespace oj::net::gv
