#pragma once
// The game's voice (docs/spec/28-network.md, "Voice"): its base (vtable 0x006FA3C8, 0x0055BBE0 .. 0x0055C600, the
// interface the Xbox's voice shared: the local players and the users of others, a handshake of events the game's hook
// carries between the machines) and GameSpy Voice's (vtable 0x006FA9B8, 0x00565360 .. 0x00566970; 0x64C bytes, the
// singleton [0x0075F1E8]): each local player's devices (Core/Network/GameSpy/GameSpyVoice.h), its frames captured every 0.15 s and
// sent to every peer of the transport as "VG" packets, those that come played on its playback device.

#include "Core/Network/GameSpy/GameSpyVoice.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace oj::net {

class OnlineUser;

class VoiceChat {
public:
    // The game's hook of the events (+0x0C, __fastcall, its context +0x10): of a local player's own (`user` -1) or
    // with another's user. 0 the user is there to be heard (when added, then every 2 s until it answers), 1 the
    // answer, 2 the player's devices opened, 3 closed, 4 the user muted, 5 unmuted.
    using EventHook = void (*)(int32_t player, int32_t user, int32_t kind, void* context);

    // The local player (0x20 bytes, +0x24, four at most).
    struct LocalPlayer {
        int32_t player = -1;   // +0x00: the user object's player (referenced)
        int32_t unused = 0;    // +0x04: 0 when added, read by nothing
        int32_t hasVoice = 0;  // +0x08: its devices opened
        int32_t chatOn = 1;    // +0x0C: the settings' +0x48 (vt+0x38)
        std::vector<int32_t> muted;  // +0x10: the users it muted (referenced)
    };
    // A local player's state with another's user (0x14 bytes).
    struct UserEntry {
        int32_t player = -1;   // +0x00 (referenced)
        int32_t answered = 0;  // +0x04: the user answered (an event 1)
        int32_t muted = 0;     // +0x08: the player muted the user
        int32_t mutedUs = 0;   // +0x0C: the user muted the player (events 4, 5)
        int32_t unused = 0;    // +0x10: read by nothing
    };
    // Another's user (0x5C bytes, +0xA8, five at most).
    struct RemoteUser {
        int32_t user = -1;     // +0x00 (referenced)
        int32_t hasVoice = 0;  // +0x04: its devices opened (events 2, 3)
        uint32_t count = 0;    // +0x08: the local players when it came
        std::array<UserEntry, 4> entries{};  // +0x0C
    };

    // 0x0055BD00: one reference, the user object ([0x0075F1E4]) referenced.
    VoiceChat();
    virtual ~VoiceChat();  // +0x00 (0x0055BDA0, 0x0055BDC0): the user object let go

    // The references (+0x04; at none the object goes).
    void addRef() { ++m_refs; }
    void release();

    virtual void listDevices(int32_t /*playback*/) {}                       // +0x04 (0x005A10E0)
    virtual uint32_t deviceCount() const { return 0; }                     // +0x08 (0x0055B910)
    virtual const char* deviceName(uint32_t /*index*/) const { return ""; }  // +0x0C (0x0055BBE0: [0x0079E8AA])
    virtual bool openDevices(const char* /*capture*/, const char* /*playback*/, int32_t /*player*/) {
        return false;  // +0x10 (0x005192F0)
    }
    virtual void closeDevices(int32_t /*player*/) {}                       // +0x14 (0x005A10E0)
    // +0x18 (0x0055BE30): not -1: taken off first (vt+0x1C); without the user object's voice mail (+0x54, nothing
    // on the PC) its player referenced and put last (chat on); the announcing timer at 1 s.
    virtual bool addPlayer(int32_t player);
    // +0x1C (0x0055BEB0): its devices closed (vt+0x14), its player and muted users let go, the last moved into its
    // place.
    virtual bool removePlayer(int32_t player);
    virtual int32_t sending(int32_t /*player*/) { return 0; }  // +0x20 (0x0051F960)
    // +0x24 (0x0055C050): not -1: taken off first (vt+0x28); with room for it (five) the user referenced and put
    // last with an entry of each local player (referenced), each told to the hook (0).
    virtual bool addUser(int32_t user);
    // +0x28 (0x0055C100): its entries' players and the user let go, the last moved into its place.
    virtual bool removeUser(int32_t user);
    virtual bool userTalking(int32_t /*user*/) { return false; }  // +0x2C (0x0051F960)
    // +0x30 (0x0055C190): not muted yet: the user referenced and put on the player's muted users; the player's entry
    // of the user muted and told to the hook (4).
    virtual void mute(int32_t player, int32_t user);
    // +0x34 (0x0055C260): the first muted user of the same profile let go and taken off (the last in its place); the
    // entry not muted and told to the hook (5).
    virtual void unmute(int32_t player, int32_t user);
    virtual void setChat(int32_t player, int32_t on);  // +0x38 (0x0055C3F0): not -1: its +0x0C
    // +0x3C (0x0055C440): an event of the user's machine to the local player (another machine's hook carried it): 0
    // answered to the hook (1); 1 the entry answered (with the player's devices open told to the hook (2) first, and
    // a user muted muted again); 2, 3 the user's devices; 4, 5 the user muted the player or not.
    virtual void userEvent(int32_t player, int32_t user, int32_t kind);
    virtual void userData(int32_t /*user*/, const uint8_t* /*data*/, uint32_t /*size*/) {}  // +0x40 (0x0055FE60)
    // +0x44 .. +0x70: the console's voice mail, which online_voice uses; none does anything on the PC (0x005A10E0
    // nothing, 0x0055B910 and 0x0051F960 0). The player's mail opened and closed (the window's enter and leave), the
    // recording's length in milliseconds (+0x58), its play, record (not 0: started) and stop. The others (+0x4C ..
    // +0x54, +0x5C (0), +0x60, +0x64 (nothing)) are called by nothing.
    virtual void openMail(int32_t /*player*/) {}                  // +0x44
    virtual void closeMail(int32_t /*player*/) {}                 // +0x48
    virtual int32_t mailLength() { return 0; }                    // +0x58
    virtual int32_t playMail(int32_t /*player*/) { return 0; }    // +0x68
    virtual int32_t recordMail(int32_t /*player*/) { return 0; }  // +0x6C
    virtual int32_t stopMail(int32_t /*player*/) { return 0; }    // +0x70
    // +0x74 (0x0055C560): the announcing timer; at 2 s, 0 and every entry not answered told to the hook again (0).
    virtual void update(float dt);

    // 0x0055C360: the player muted a user of the user's profile.
    bool isMuted(int32_t player, int32_t user) const;
    // 0x0055C010: the player's devices opened (+0x08); 0 for -1 or a player not here.
    int32_t hasVoice(int32_t player) const;
    // 0x0055BBF0: the user of the same profile (the user object's vt+0x18), or null.
    RemoteUser* findUser(int32_t user);
    const RemoteUser* findUser(int32_t user) const;
    // 0x0055BC50, 0x0055BC80.
    LocalPlayer* findPlayer(int32_t player);
    static UserEntry* findEntry(RemoteUser* user, int32_t player);

    EventHook eventHook = nullptr;  // +0x0C
    void* eventContext = nullptr;   // +0x10
    // +0x14, +0x18: a second hook and its context; the network game sets it to nothing (0x0055FE70), nothing calls it.
    void (*unusedHook)(int32_t, int32_t) = nullptr;
    void* unusedHookContext = nullptr;
    // +0x1C: the chat switched on or off (0x004EEB50: the settings' +0x48 the other way); called by nothing on the PC.
    std::function<void(int32_t on)> chatHook;
    int32_t recordingMail = 0;      // +0x278: online_voice's recording going on (set by nothing on the PC)

protected:
    // 0x0055BCB0: the player's devices opened or not, told to the hook (2 or 3) with no user.
    void setHasVoice(int32_t player, int32_t on);
    void tell(int32_t player, int32_t user, int32_t kind) {
        if (eventHook != nullptr) eventHook(player, user, kind, eventContext);
    }

    OnlineUser* m_user = nullptr;  // +0x08
    uint32_t m_playerCount = 0;    // +0x20
    std::array<LocalPlayer, 4> m_players{};  // +0x24
    uint32_t m_userCount = 0;      // +0xA4
    std::array<RemoteUser, 5> m_users{};      // +0xA8
    float m_announceTimer = 0.0f;  // +0x274

private:
    int32_t m_refs = 1;  // +0x04
};

class GameSpyVoiceChat : public VoiceChat {
public:
    // A local player's devices and its packet (0x188 bytes, +0x27C's list).
    struct Talker {
        int32_t player = -1;            // +0x00
        int32_t wantCapture = 0;        // +0x04: a capture device asked for (opened again every 2 s while it is not)
        char captureName[0x40]{};       // +0x08
        gv::Device* capture = nullptr;  // +0x48
        int32_t wantPlayback = 0;       // +0x4C
        char playbackName[0x40]{};      // +0x50
        gv::Device* playback = nullptr; // +0x90
        // +0x98 .. +0x17F, the packet sent: "VG", +0x04 the first local player's profile, +0x08 the first frame's
        // stamp, +0x10 the volume (double), +0x18 the frames' bytes, +0x1C the frames (200 bytes at most).
        std::array<uint8_t, 0xE8> packet{};
        int32_t sent = 0;               // +0x180: a packet went at the last capture
    };

    // 0x005659C0: the lists (room for 4 players and 15 users); GameSpy Voice started with the game's window
    // ([0x0075F79C]+0x0C) and the Speex codec at quality 4 set (160 samples a frame) when it can be made.
    explicit GameSpyVoiceChat(HWND window);
    // 0x00565BD0: each player's devices closed; the codec let go (when this set it), every device, COM, the frames;
    // the transport's filter of "VG" taken out.
    ~GameSpyVoiceChat() override;

    void listDevices(int32_t playback) override;                          // +0x04 (0x00565D70)
    uint32_t deviceCount() const override { return m_deviceCount; }        // +0x08 (0x00565350)
    const char* deviceName(uint32_t index) const override;                // +0x0C (0x00565DB0)
    bool openDevices(const char* capture, const char* playback, int32_t player) override;  // +0x10 (0x00565DE0)
    void closeDevices(int32_t player) override;                           // +0x14 (0x00566220)
    bool addPlayer(int32_t player) override;                              // +0x18 (0x005662C0)
    bool removePlayer(int32_t player) override;                           // +0x1C (0x00566360)
    int32_t sending(int32_t player) override;                             // +0x20 (0x00566390)
    bool addUser(int32_t user) override;                                  // +0x24 (0x005663B0)
    bool removeUser(int32_t user) override;                               // +0x28 (0x005663E0)
    bool userTalking(int32_t user) override;                              // +0x2C (0x00566410)
    void update(float dt) override;                                       // +0x74 (0x00566770)

    const std::vector<Talker>& talkers() const { return m_talkers; }
    int32_t samplesPerFrame() const { return m_samplesPerFrame; }  // +0x2A4
    int32_t frameSize() const { return m_frameSize; }              // +0x2A8
    int32_t bitrate() const { return m_bitrate; }                  // +0x2AC

private:
    Talker* findTalker(int32_t player);         // 0x00565870
    void addTalker(int32_t player);             // 0x00565720
    void removeTalker(int32_t player);          // 0x005657D0
    void retryFrame(float dt);                  // 0x00566480
    void captureFrame(float dt);                // 0x00566540
    void packetCame(const uint8_t* packet);     // 0x005654A0
    void unplugged(gv::Device* device);         // 0x005655E0
    static void onUnplugged(gv::Device* device);  // 0x005656D0

    std::vector<Talker> m_talkers;        // +0x27C
    std::vector<int32_t> m_userVoices;    // +0x28C: the users added (16 bytes each, the user first)
    float m_captureTimer = 0.0f;          // +0x29C
    float m_retryTimer = 0.0f;            // +0x2A0
    int32_t m_samplesPerFrame = 0;        // +0x2A4
    int32_t m_frameSize = 0;              // +0x2A8
    int32_t m_bitrate = 0;                // +0x2AC: the encoded bits a second
    std::array<gv::DeviceInfo, 10> m_devices{};  // +0x2B0
    uint32_t m_deviceCount = 0;           // +0x648
};

// 0x00565360: the singleton ([0x0075F1E8]) made when there is none (with its own reference), then referenced; null
// when it cannot be made.
VoiceChat* acquireVoice();
// The game's window GameSpy Voice is started with ([0x0075F79C]+0x0C).
void setVoiceWindow(HWND window);
// [0x0075F044]: the game's voice (a reference of the singleton), none outside a login or opts_voice.
VoiceChat* gameVoice();
// 0x004EECA0: the game's reference and the singleton's let go, both none.
void endGameVoice();
// The login's hook and opts_voice's enter: 0x004EECA0, the singleton referenced (0x00565360) as the game's, its +0x1C
// the hook.
void makeGameVoice(std::function<void(int32_t on)> chatHook);

// [0x00728D84] .. [0x00728D90] and [0x0075F048]: the voice's icons, the front end's textures live_comm_off,
// live_comm_mute, live_comm_on and live_comm_tv, taken once (0x004EEB70, the game's loop with a front end); empty
// before.
struct VoiceIcons {
    std::string off;   // [0x00728D84]: a user whose devices are opened, not talking
    std::string mute;  // [0x00728D88]: a user the player muted
    std::string on;    // [0x00728D8C]: talking
    std::string tv;    // [0x00728D90]: (shown by nothing)
};
const VoiceIcons& voiceIcons();
void loadVoiceIcons();
// The windows' icon of a user for a local player: muted (0x0055C360) mute; else with the user's devices opened
// (0x0055BBF0, +0x04) on while it talks (vt+0x2C), else off; else none (an empty name).
const std::string& voiceIcon(VoiceChat& voice, int32_t player, int32_t user);

}  // namespace oj::net
