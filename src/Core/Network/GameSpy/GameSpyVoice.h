#pragma once
// GameSpy Voice 2 as the stock game links it (0x0057EBC0 .. 0x0057FC50, 0x005804C0 .. 0x00580A10; docs/spec/
// 28-network.md, "Voice"): the sound devices of DirectSound and DirectSoundCapture, each capture device a looping
// buffer of 16000 bytes read a frame at a time, each playback device a looping buffer of ten frames written behind
// its play cursor with the frames of up to eight sources (each a list of frames in order of their stamps, a little
// jitter buffer of its own) mixed in; the frames of the codec the game gives (Speex 1.0.3's narrowband at quality 4,
// the gviSpeex functions 0x00580230 .. 0x00580450; third_party/speex-1.0.3).

#include <array>
#include <cstdint>
#include <vector>

typedef struct HWND__* HWND;
struct IDirectSound;
struct IDirectSoundBuffer;
struct IDirectSoundCapture;
struct IDirectSoundCaptureBuffer;

namespace oj::net::gv {

using Sample = int16_t;       // GVSample
using FrameStamp = uint16_t;  // GVFrameStamp
using Source = int32_t;       // GVSource (the game gives a sender's profile)

// GVDeviceType.
enum DeviceTypes : int32_t {
    kCapture = 1,   // GV_CAPTURE
    kPlayback = 2,  // GV_PLAYBACK
};

// GVDeviceInfo (0x5C bytes).
struct DeviceInfo {
    std::array<uint8_t, 16> id{};  // +0x00: the device's GUID
    char name[0x40]{};             // +0x10: its description, cut to 63 characters
    int32_t type = 0;              // +0x50: 1 capture, 2 playback
    int32_t isDefault = 0;         // +0x54: 1 the default voice capture device, 2 the default voice playback one
    int32_t hardware = 0;          // +0x58
};

// gvListDevices (0x0057F190): with max above 0, DirectSoundCaptureEnumerateA for types & 1, then
// DirectSoundEnumerateA for types & 2, each device through the callback 0x0057F0B0 (the NULL GUID of the primary
// driver passed over; a GUID already listed only takes the type of this enumeration, else a new entry at the end with
// the description; the enumeration stops at max); then the devices of DSDEVID_DefaultVoiceCapture's and
// DSDEVID_DefaultVoicePlayback's GUIDs marked 1 and 2. At most max entries.
std::vector<DeviceInfo> listDevices(int32_t max, int32_t types);

// GVCustomCodecInfo (0x18 bytes).
struct CodecInfo {
    int32_t samplesPerFrame = 0;   // +0x00
    int32_t encodedFrameSize = 0;  // +0x04
    bool (*newDecoder)(void** data) = nullptr;                          // +0x08
    void (*freeDecoder)(void* data) = nullptr;                          // +0x0C
    void (*encode)(uint8_t* out, const Sample* in) = nullptr;           // +0x10
    void (*decode)(Sample* out, const uint8_t* in, void* data) = nullptr;  // +0x14: added into out
};

struct Device;
// The device's callbacks: unplugged (+0x4C: the device failed; it is freed right after), the filters of a frame
// captured (+0x50) or mixed for playback (+0x54) with its stamp.
using UnpluggedCallback = void (*)(Device* device);
using FilterCallback = void (*)(Device* device, Sample* frame, FrameStamp stamp);

struct PendingFrame;
struct SourceList;

// A device (GVIDevice, 0x5C bytes; its methods +0x18 .. +0x48 the same for every device of the PC's DirectSound).
struct Device {
    std::array<uint8_t, 16> id{};  // +0x00: the GUID it was made of
    int32_t types = 0;             // +0x14

    void free();                                // +0x18 (0x0057F280): gvFreeDevice
    bool start(int32_t type);                   // +0x1C (0x0057F380)
    void stop(int32_t type);                    // +0x20 (0x0057F3A0)
    bool isStarted(int32_t type);               // +0x24 (0x0057F3F0)
    void setVolume(int32_t type, double volume);  // +0x28 (0x0057F490)
    double volume(int32_t type);                // +0x2C (0x0057F4C0)
    void setCaptureThreshold(double threshold);  // +0x30 (0x0057F510)
    double captureThreshold() const;            // +0x34 (0x0057F520)
    // +0x38 (0x0057F590): the frames available (as many as len's bytes hold) encoded into packet; len their bytes,
    // stamp the first one's, volume the peak of the samples read. True when they are worth sending (the peak at the
    // threshold or above, or within 20 frames of the last that was).
    bool capturePacket(uint8_t* packet, int32_t* len, FrameStamp* stamp, double* volume);
    int32_t availableCaptureBytes();            // +0x3C (0x0057F530)
    void playPacket(const uint8_t* packet, int32_t len, Source source, FrameStamp stamp);  // +0x40 (0x0057F830)
    bool isSourceTalking(Source source);        // +0x44 (0x0057F860)
    int32_t listTalkingSources(Source* sources, int32_t max);  // +0x48 (0x0057F880)

    UnpluggedCallback unplugged = nullptr;  // +0x4C
    FilterCallback captureFilter = nullptr;   // +0x50
    FilterCallback playbackFilter = nullptr;  // +0x54

    // +0x58: the hardware's data (GVIHardwareData, 0x58 bytes; the playback's part, then the capture's).
    struct Data {
        int32_t playing = 0;            // +0x00
        IDirectSound* sound = nullptr;          // +0x04
        IDirectSoundBuffer* buffer = nullptr;   // +0x08
        uint32_t bufferSize = 0;        // +0x0C: 3200 bytes rounded up to two frames' bytes
        uint32_t halfSize = 0;          // +0x10
        uint32_t writePos = 0;          // +0x14: the play cursor last seen, a frame's bytes down
        FrameStamp clock = 0;           // +0x18: the stamp of the next frame mixed
        SourceList* sources = nullptr;  // +0x1C
        uint32_t tick = 0;              // +0x20: GetTickCount at the last mix
        int32_t capturing = 0;          // +0x24
        IDirectSoundCapture* capture = nullptr;              // +0x28
        IDirectSoundCaptureBuffer* captureBuffer = nullptr;  // +0x2C
        uint32_t captureSize = 0;       // +0x30: 16000 bytes rounded up to a frame's bytes
        uint32_t readPos = 0;           // +0x34
        double volume = 0.0;            // +0x38: the capture's volume (1.0)
        FrameStamp captureClock = 0;    // +0x40
        double threshold = 0.0;         // +0x48
        FrameStamp lastAbove = 0;       // +0x50: the stamp last at the threshold (-21 at first)
    } data;
};

// gvStartup (0x0057ED00): COM (CoInitialize), the format (PCM, one channel, 8000 Hz, 16 bits), the window (the
// foreground one without it, else the desktop's) and the list of devices (a GameSpy darray: none, room for 2,
// growing by 2). True.
bool startup(HWND window);
// The voice object's destructor's part (0x00565BD0): every device freed with the list, CoUninitialize, the free
// frames freed.
void cleanup();
// gvThink (0x0057F060): each playback device from the last mixed (0x0057EDD0); one that fails with an unplugged
// callback has it called and is freed.
void think();
// gvSetCustomCodec (0x0057EC10): the codec, its frame's samples, encoded bytes and samples' bytes; the free frames
// made again (0x0057FBD0: those there freed, then 200 of the encoded size).
void setCustomCodec(const CodecInfo& codec);
// gvNewDevice (0x0057FA20): a capture (1) or playback (2) device of the GUID (any other type none): DirectSound's
// object and its buffer (0x0057F8B0: the cooperative level DSSCL_NORMAL with the window, a buffer of 3200 bytes with
// GETCURRENTPOSITION2, GLOBALFOCUS and CTRLVOLUME) and its eight sources, or DirectSoundCapture's and its looping
// capture buffer of 16000 bytes started at once (0x0057F980; the volume 1, the last stamp at the threshold -21); put
// on the list. Null when any of it fails.
Device* newDevice(const std::array<uint8_t, 16>& id, int32_t type);

// The frame's samples, encoded bytes and samples' bytes ([0x007A4330], [0x007A432C], [0x007A4328]).
int32_t samplesPerFrame();
int32_t encodedFrameSize();
int32_t bytesPerFrame();

// GameSpy's Speex codec (gvSpeex: the encoder [0x0075D98C], its bits [0x0075D968]).
// 0x00580230: the narrowband encoder (none twice: 0 when it is made already), its quality, the bits (2000 bytes) and
// the encoded frame's bytes (the bitrate / 50 / 8).
bool speexInitialize(int32_t quality);
// 0x0057EBC0: made: the encoder destroyed and the bits freed.
void speexCleanup();
bool speexNewDecoder(void** data);                          // 0x005802C0: with the enhancer
void speexFreeDecoder(void* data);                          // 0x00580300
void speexEncode(uint8_t* out, const Sample* in);           // 0x00580310
void speexDecodeAdd(Sample* out, const uint8_t* in, void* data);  // 0x00580450: the samples added into out
// SPEEX_RESET_STATE on the encoder (a capture device's stop, 0x0057F3C7).
void speexResetEncoder();
int32_t speexEncodedFrameSize();  // [0x0075D994]

}  // namespace oj::net::gv
