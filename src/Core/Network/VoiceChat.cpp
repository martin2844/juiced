#include "Core/Network/VoiceChat.h"

#include "Core/Network/GameSpy/GameSpyUser.h"
#include "Core/Network/NetSession.h"

#include <cstring>

namespace oj::net {

namespace {

VoiceChat* s_voice = nullptr;      // [0x0075F1E8]
VoiceChat* s_gameVoice = nullptr;  // [0x0075F044]
HWND s_window = nullptr;           // [0x0075F79C]+0x0C
int32_t s_codecSet = 0;            // [0x0075D944]: the voice set the Speex codec

const std::vector<uint8_t> kVoicePrefix = {0x56, 0x47};  // "VG" (0x4756)

void put32(uint8_t* at, int32_t v) {
    std::memcpy(at, &v, 4);
}

// strncpy of 0x40 (the rest zeroed), the last byte 0.
void copyName(char (&to)[0x40], const char* from) {
    size_t i = 0;
    for (; i < sizeof to && from[i] != 0; ++i) to[i] = from[i];
    for (; i < sizeof to; ++i) to[i] = 0;
    to[sizeof to - 1] = 0;
}

int32_t get32(const uint8_t* at) {
    int32_t v = 0;
    std::memcpy(&v, at, 4);
    return v;
}

}  // namespace

// ---- The base.

VoiceChat::VoiceChat() {
    m_user = userSingleton();
    if (m_user != nullptr) m_user->addRef();
}

VoiceChat::~VoiceChat() {
    if (m_user != nullptr) m_user->release();
}

void VoiceChat::release() {
    if (--m_refs == 0) delete this;
}

VoiceChat::RemoteUser* VoiceChat::findUser(int32_t user) {
    for (uint32_t i = 0; i < m_userCount; ++i) {
        if (m_user != nullptr && m_user->sameUser(m_users[i].user, user)) return &m_users[i];
    }
    return nullptr;
}

const VoiceChat::RemoteUser* VoiceChat::findUser(int32_t user) const {
    return const_cast<VoiceChat*>(this)->findUser(user);
}

VoiceChat::LocalPlayer* VoiceChat::findPlayer(int32_t player) {
    for (uint32_t i = 0; i < m_playerCount; ++i) {
        if (m_players[i].player == player) return &m_players[i];
    }
    return nullptr;
}

VoiceChat::UserEntry* VoiceChat::findEntry(RemoteUser* user, int32_t player) {
    if (user == nullptr) return nullptr;
    for (uint32_t i = 0; i < user->count; ++i) {
        if (user->entries[i].player == player) return &user->entries[i];
    }
    return nullptr;
}

void VoiceChat::setHasVoice(int32_t player, int32_t on) {
    LocalPlayer* p = findPlayer(player);
    if (p == nullptr) return;
    p->hasVoice = on;
    tell(player, -1, on != 0 ? 2 : 3);
}

bool VoiceChat::isMuted(int32_t player, int32_t user) const {
    for (uint32_t i = 0; i < m_playerCount; ++i) {
        if (m_players[i].player != player) continue;
        for (int32_t m : m_players[i].muted) {
            if (m_user != nullptr && m_user->sameUser(m, user)) return true;
        }
        return false;
    }
    return false;
}

int32_t VoiceChat::hasVoice(int32_t player) const {
    if (player == -1) return 0;
    for (uint32_t i = 0; i < m_playerCount; ++i) {
        if (m_players[i].player == player) return m_players[i].hasVoice;
    }
    return 0;
}

bool VoiceChat::addPlayer(int32_t player) {
    if (player == -1) return false;
    removePlayer(player);
    if (m_user == nullptr || m_user->voiceMailWaiting(player) != 0) return false;
    // Stock problem: no look at the room; a fifth player would be written over the users' count.
    if (m_playerCount == m_players.size()) return false;
    m_user->playerAddRef(player);
    LocalPlayer& p = m_players[m_playerCount];
    p.player = player;
    p.unused = 0;
    p.hasVoice = 0;
    p.chatOn = 1;
    ++m_playerCount;
    m_announceTimer = 1.0f;
    return true;
}

bool VoiceChat::removePlayer(int32_t player) {
    if (player == -1) return false;
    closeDevices(player);
    LocalPlayer* p = findPlayer(player);
    if (p == nullptr) return false;
    if (m_user != nullptr) {
        m_user->playerRelease(p->player);
        for (int32_t m : p->muted) m_user->userRelease(m);
    }
    p->muted.clear();
    LocalPlayer& last = m_players[m_playerCount - 1];
    if (p != &last) {
        // The player and its +0x08, +0x0C moved (not +0x04); the last's muted users pushed on, then emptied.
        p->player = last.player;
        p->hasVoice = last.hasVoice;
        p->chatOn = last.chatOn;
        for (int32_t m : last.muted) p->muted.push_back(m);
        last.muted.clear();
    }
    --m_playerCount;
    return true;
}

bool VoiceChat::addUser(int32_t user) {
    if (user == -1) return false;
    removeUser(user);
    if (m_userCount >= 5) return false;
    RemoteUser& r = m_users[m_userCount];
    ++m_userCount;
    if (m_user != nullptr) m_user->userAddRef(user);
    r.user = user;
    r.hasVoice = 0;
    r.count = m_playerCount;
    for (uint32_t i = 0; i < m_playerCount; ++i) {
        if (m_user != nullptr) m_user->playerAddRef(m_players[i].player);
        r.entries[i] = UserEntry{};
        r.entries[i].player = m_players[i].player;
        tell(r.entries[i].player, user, 0);
    }
    return true;
}

bool VoiceChat::removeUser(int32_t user) {
    if (user == -1) return false;
    RemoteUser* r = findUser(user);
    if (r == nullptr) return false;
    if (m_user != nullptr) {
        for (uint32_t i = 0; i < r->count; ++i) m_user->playerRelease(r->entries[i].player);
        m_user->userRelease(r->user);
    }
    *r = m_users[m_userCount - 1];
    --m_userCount;
    return true;
}

void VoiceChat::mute(int32_t player, int32_t user) {
    if (!isMuted(player, user)) {
        if (LocalPlayer* p = findPlayer(player)) {
            if (m_user != nullptr) m_user->userAddRef(user);
            p->muted.push_back(user);
        }
    }
    if (UserEntry* e = findEntry(findUser(user), player)) {
        e->muted = 1;
        tell(player, user, 4);
    }
}

void VoiceChat::unmute(int32_t player, int32_t user) {
    if (LocalPlayer* p = findPlayer(player)) {
        for (size_t i = 0; i < p->muted.size(); ++i) {
            if (m_user != nullptr && m_user->sameUser(p->muted[i], user)) {
                m_user->userRelease(p->muted[i]);
                p->muted[i] = p->muted.back();
                p->muted.pop_back();
                break;
            }
        }
    }
    if (UserEntry* e = findEntry(findUser(user), player)) {
        e->muted = 0;
        tell(player, user, 5);
    }
}

void VoiceChat::setChat(int32_t player, int32_t on) {
    if (player == -1) return;
    if (LocalPlayer* p = findPlayer(player)) p->chatOn = on;
}

void VoiceChat::userEvent(int32_t player, int32_t user, int32_t kind) {
    RemoteUser* r = findUser(user);
    if (r == nullptr || kind < 0 || kind > 5) return;
    switch (kind) {
    case 0:
        tell(player, user, 1);
        return;
    case 1: {
        UserEntry* e = findEntry(r, player);
        if (e == nullptr) return;
        LocalPlayer* p = findPlayer(player);
        if (p != nullptr && eventHook != nullptr) {
            if (p->hasVoice != 0) tell(player, user, 2);
            if (isMuted(player, user)) mute(player, user);
        }
        e->answered = 1;
        return;
    }
    case 2:
        r->hasVoice = 1;
        return;
    case 3:
        r->hasVoice = 0;
        return;
    case 4:
        if (UserEntry* e = findEntry(r, player)) e->mutedUs = 1;
        return;
    case 5:
        if (UserEntry* e = findEntry(r, player)) e->mutedUs = 0;
        return;
    }
}

void VoiceChat::update(float dt) {
    m_announceTimer = m_announceTimer + dt;
    if (m_announceTimer < 2.0f) return;  // [0x006FF898]
    m_announceTimer = 0.0f;
    for (uint32_t i = 0; i < m_userCount; ++i) {
        RemoteUser& r = m_users[i];
        for (uint32_t k = 0; k < r.count; ++k) {
            if (r.entries[k].answered == 0) tell(r.entries[k].player, r.user, 0);
        }
    }
}

// ---- GameSpy Voice's.

GameSpyVoiceChat::GameSpyVoiceChat(HWND window) {
    m_talkers.reserve(4);
    m_userVoices.reserve(15);
    if (!gv::startup(window)) {
        if (s_codecSet != 0) {
            gv::speexCleanup();
            s_codecSet = 0;
        }
    } else {
        s_codecSet = 0;
    }
    if (gv::speexInitialize(4)) {
        gv::CodecInfo codec;
        codec.samplesPerFrame = 160;
        codec.encodedFrameSize = gv::speexEncodedFrameSize();
        codec.newDecoder = gv::speexNewDecoder;
        codec.freeDecoder = gv::speexFreeDecoder;
        codec.encode = gv::speexEncode;
        codec.decode = gv::speexDecodeAdd;
        gv::setCustomCodec(codec);
        s_codecSet = 1;
    }
    m_samplesPerFrame = gv::samplesPerFrame();
    m_frameSize = gv::encodedFrameSize();
    if (gv::bytesPerFrame() != 0) {
        m_bitrate = static_cast<int32_t>(static_cast<uint32_t>(m_frameSize * 128000) /
                                         static_cast<uint32_t>(gv::bytesPerFrame()));
    }
}

GameSpyVoiceChat::~GameSpyVoiceChat() {
    for (size_t i = 0; i < m_talkers.size(); ++i) closeDevices(m_talkers[i].player);
    if (s_codecSet != 0) {
        gv::speexCleanup();
        s_codecSet = 0;
    }
    gv::cleanup();
    if (NetGame* game = netGame()) game->transport.removeFilter(kVoicePrefix);
}

void GameSpyVoiceChat::listDevices(int32_t playback) {
    const std::vector<gv::DeviceInfo> listed = gv::listDevices(10, playback != 0 ? gv::kPlayback : gv::kCapture);
    for (size_t i = 0; i < listed.size(); ++i) m_devices[i] = listed[i];
    m_deviceCount = static_cast<uint32_t>(listed.size());
}

const char* GameSpyVoiceChat::deviceName(uint32_t index) const {
    if (index >= m_deviceCount) return "";
    return m_devices[index].name;
}

GameSpyVoiceChat::Talker* GameSpyVoiceChat::findTalker(int32_t player) {
    for (Talker& t : m_talkers) {
        if (t.player == player) return &t;
    }
    return nullptr;
}

// The names kept (asked for again every 2 s while their devices are not there); every device listed (both kinds,
// ten at most); the first of each name (case aside) of the kind. Neither: false. One device of both names: the old
// devices freed, it made of its type, started, the threshold 0.15, the unplugged callback; the player's devices
// opened (told to the hook); both its. Else the capture device made (the old one freed) and started with the
// threshold and the callback, the player's devices opened; then the playback device made of the entry's type
// (the old one freed), started with the callback. A device that cannot be made or started: false (the rest not
// tried).
bool GameSpyVoiceChat::openDevices(const char* capture, const char* playback, int32_t player) {
    Talker* t = findTalker(player);
    if (t == nullptr) return false;
    t->wantCapture = 1;
    copyName(t->captureName, capture);
    t->wantPlayback = 1;
    copyName(t->playbackName, playback);
    const std::vector<gv::DeviceInfo> listed = gv::listDevices(10, gv::kCapture | gv::kPlayback);
    for (size_t i = 0; i < listed.size(); ++i) m_devices[i] = listed[i];
    m_deviceCount = static_cast<uint32_t>(listed.size());
    const gv::DeviceInfo* in = nullptr;
    const gv::DeviceInfo* out = nullptr;
    for (uint32_t i = 0; i < m_deviceCount; ++i) {
        const gv::DeviceInfo& d = m_devices[i];
        if (_stricmp(d.name, capture) == 0 && (d.type & gv::kCapture) != 0) in = &d;
        if (_stricmp(d.name, playback) == 0 && (d.type & gv::kPlayback) != 0) out = &d;
    }
    if (in == nullptr && out == nullptr) return false;
    if (in != nullptr && in == out) {
        if (t->capture != nullptr) {
            if (t->playback == t->capture) t->playback = nullptr;
            t->capture->free();
            t->capture = nullptr;
        }
        if (t->playback != nullptr) {
            t->playback->free();
            t->playback = nullptr;
        }
        gv::Device* d = gv::newDevice(in->id, in->type);
        if (d == nullptr || !d->start(in->type)) return false;
        d->unplugged = onUnplugged;
        d->setCaptureThreshold(static_cast<double>(0.15f));  // [0x006FFEC0]
        setHasVoice(t->player, 1);
        t->capture = d;
        t->playback = d;
        return true;
    }
    if (in != nullptr && (in->type & gv::kCapture) != 0) {
        if (t->capture != nullptr) {
            if (t->playback == t->capture) t->playback = nullptr;
            t->capture->free();
            t->capture = nullptr;
        }
        gv::Device* d = gv::newDevice(in->id, gv::kCapture);
        if (d == nullptr || !d->start(gv::kCapture)) return false;
        d->unplugged = onUnplugged;
        d->setCaptureThreshold(static_cast<double>(0.15f));
        setHasVoice(t->player, 1);
        t->capture = d;
    }
    if (out != nullptr && (out->type & gv::kPlayback) != 0) {
        if (t->playback != nullptr) {
            if (t->capture == t->playback) t->capture = nullptr;
            t->playback->free();
            t->playback = nullptr;
        }
        gv::Device* d = gv::newDevice(out->id, out->type);
        if (d == nullptr || !d->start(out->type)) return false;
        d->unplugged = onUnplugged;
        t->playback = d;
    }
    return true;
}

// Not asked for any more; one device of both stopped (both types) and freed, else each stopped and freed.
void GameSpyVoiceChat::closeDevices(int32_t player) {
    Talker* t = findTalker(player);
    if (t == nullptr) return;
    t->wantCapture = 0;
    t->captureName[0] = 0;
    t->wantPlayback = 0;
    t->playbackName[0] = 0;
    if (t->capture == nullptr && t->playback == nullptr) return;
    if (t->capture == t->playback) {
        t->capture->stop(gv::kCapture | gv::kPlayback);
        t->capture->free();
        t->capture = nullptr;
        t->playback = nullptr;
        return;
    }
    if (t->capture != nullptr) {
        t->capture->stop(gv::kCapture);
        t->capture->free();
        t->capture = nullptr;
    }
    if (t->playback != nullptr) {
        t->playback->stop(gv::kPlayback);
        t->playback->free();
        t->playback = nullptr;
    }
}

// The base's; then with the transport its filter of "VG" (0x005997A0, this as its context) and the talker.
bool GameSpyVoiceChat::addPlayer(int32_t player) {
    if (!VoiceChat::addPlayer(player)) return false;
    if (NetGame* game = netGame()) {
        game->transport.addFilter(kVoicePrefix, [this](const uint8_t* data, size_t size, const Address&) {
            // The transport's buffer (0x800 bytes) as the stock reads it, whatever came.
            uint8_t packet[0x800]{};
            std::memcpy(packet, data, size < sizeof packet ? size : sizeof packet);
            packetCame(packet);
        });
    }
    addTalker(player);
    return true;
}

// The player, "VG", the profile of the user object's first player's user (whatever the player), nothing captured.
void GameSpyVoiceChat::addTalker(int32_t player) {
    Talker t;
    t.player = player;
    t.packet[0] = 0x56;
    t.packet[1] = 0x47;
    int32_t profile = 0;
    if (m_user != nullptr && !m_user->players().empty()) {
        const int32_t user = m_user->players()[0].user;
        if (user >= 0 && static_cast<size_t>(user) < m_user->records().size()) {
            profile = m_user->records()[static_cast<size_t>(user)].profile;
        }
    }
    put32(t.packet.data() + 4, profile);
    m_talkers.push_back(t);
}

bool GameSpyVoiceChat::removePlayer(int32_t player) {
    if (!VoiceChat::removePlayer(player)) return false;
    removeTalker(player);
    return true;
}

// The talker taken off, the last moved into its place.
void GameSpyVoiceChat::removeTalker(int32_t player) {
    for (size_t i = 0; i < m_talkers.size(); ++i) {
        if (m_talkers[i].player == player) {
            m_talkers[i] = m_talkers.back();
            m_talkers.pop_back();
            return;
        }
    }
}

int32_t GameSpyVoiceChat::sending(int32_t player) {
    const Talker* t = findTalker(player);
    return t != nullptr ? t->sent : 0;
}

bool GameSpyVoiceChat::addUser(int32_t user) {
    if (!VoiceChat::addUser(user)) return false;
    if (user != -1) m_userVoices.push_back(user);
    return true;
}

bool GameSpyVoiceChat::removeUser(int32_t user) {
    if (!VoiceChat::removeUser(user)) return false;
    for (size_t i = 0; i < m_userVoices.size(); ++i) {
        if (m_userVoices[i] == user) {
            m_userVoices[i] = m_userVoices.back();
            m_userVoices.pop_back();
            break;
        }
    }
    return true;
}

// The user's record (the user object's vt+0x24): with a talker whose capture device is there, whether that device
// has the profile's source talking. Stock problem: the first talker's capture device is asked (+0x48, not its
// playback device +0x90); a capture device never plays, so no user ever talks.
bool GameSpyVoiceChat::userTalking(int32_t user) {
    if (user == -1 || m_user == nullptr) return false;
    if (user < 0 || static_cast<size_t>(user) >= m_user->records().size()) return false;
    const OnlineUserRecord& r = m_user->record(user);
    if (m_talkers.empty() || m_talkers[0].capture == nullptr) return false;
    return m_talkers[0].capture->isSourceTalking(r.profile);
}

// The base's update, GameSpy Voice's think, the devices asked for again, the capture.
void GameSpyVoiceChat::update(float dt) {
    VoiceChat::update(dt);
    gv::think();
    retryFrame(dt);
    captureFrame(dt);
}

// Past 2 s: 0, and each talker that asked for a device not there asks again (vt+0x10 with its names).
void GameSpyVoiceChat::retryFrame(float dt) {
    m_retryTimer = m_retryTimer + dt;
    if (!(m_retryTimer > 2.0f)) return;  // [0x006FF898]
    m_retryTimer = 0.0f;
    for (size_t i = 0; i < m_talkers.size(); ++i) {
        Talker& t = m_talkers[i];
        if ((t.wantCapture != 0 && t.capture == nullptr) || (t.wantPlayback != 0 && t.playback == nullptr)) {
            const std::string capture = t.captureName;
            const std::string playback = t.playbackName;
            openDevices(capture.c_str(), playback.c_str(), t.player);
        }
    }
}

// Past 0.15 s: 0, and each talker not sending; one with a capture device takes its packet (200 bytes of room); worth
// sending with frames: to every peer of the transport as it is ("VG" and its frames' bytes + 0x20), sending.
void GameSpyVoiceChat::captureFrame(float dt) {
    m_captureTimer = m_captureTimer + dt;
    if (!(m_captureTimer > 0.15f)) return;  // [0x006FFB04]
    m_captureTimer = 0.0f;
    for (size_t i = 0; i < m_talkers.size(); ++i) {
        Talker& t = m_talkers[i];
        t.sent = 0;
        if (t.capture == nullptr) continue;
        int32_t len = 200;
        gv::FrameStamp stamp = 0;
        double volume = 0.0;
        std::memcpy(&stamp, t.packet.data() + 8, 2);
        std::memcpy(&volume, t.packet.data() + 0x10, 8);
        const bool worth = t.capture->capturePacket(t.packet.data() + 0x1C, &len, &stamp, &volume);
        if (i >= m_talkers.size()) return;
        Talker& now = m_talkers[i];
        std::memcpy(now.packet.data() + 8, &stamp, 2);
        std::memcpy(now.packet.data() + 0x10, &volume, 8);
        put32(now.packet.data() + 0x18, len);
        if (!worth || len == 0) continue;
        if (NetGame* game = netGame()) {
            const size_t bytes = static_cast<size_t>(len) + 0x20;
            for (const Peer& p : game->transport.peers()) {
                game->transport.sendRaw(p.address, now.packet.data(), bytes < now.packet.size() ? bytes
                                                                                                : now.packet.size());
            }
        }
        now.sent = 1;
    }
}

// A packet of "VG": a user of its profile (+0x04, the user object's vt+0x0C and +0x24, no nick); each talker with a
// playback device plays it (its frames +0x1C, their bytes +0x18, the profile as the source, the stamp +0x08) unless
// the talker's player muted that user; the user let go.
void GameSpyVoiceChat::packetCame(const uint8_t* packet) {
    if (m_user == nullptr) return;
    const int32_t user = m_user->newUser();
    OnlineUserRecord& r = m_user->record(user);
    r.profile = get32(packet + 4);
    r.nick.clear();
    for (size_t i = 0; i < m_talkers.size(); ++i) {
        const int32_t player = m_talkers[i].player;
        if (const UserEntry* e = findEntry(findUser(user), player)) {
            if (e->muted != 0) continue;
        }
        if (gv::Device* d = m_talkers[i].playback) {
            int32_t len = get32(packet + 0x18);
            if (len < 0 || len > 0x800 - 0x1C) len = len < 0 ? 0 : 0x800 - 0x1C;
            gv::FrameStamp stamp = 0;
            std::memcpy(&stamp, packet + 8, 2);
            d->playPacket(packet + 0x1C, len, get32(packet + 4), stamp);
        }
    }
    m_user->userRelease(user);
}

// The device taken off the talkers: as a capture device the player's devices closed (told to the hook, 3); as a
// playback device only forgotten.
void GameSpyVoiceChat::unplugged(gv::Device* device) {
    for (Talker& t : m_talkers) {
        if (t.capture == device) {
            t.capture = nullptr;
            if (LocalPlayer* p = findPlayer(t.player)) {
                p->hasVoice = 0;
                tell(t.player, -1, 3);
            }
        }
        if (t.playback == device) t.playback = nullptr;
    }
}

// The singleton referenced (0x00565360) while the device is taken off, then let go.
void GameSpyVoiceChat::onUnplugged(gv::Device* device) {
    VoiceChat* voice = acquireVoice();
    if (voice == nullptr) return;
    static_cast<GameSpyVoiceChat*>(voice)->unplugged(device);
    voice->release();
}

// ---- The singleton and the game's.

VoiceChat* acquireVoice() {
    if (s_voice == nullptr) s_voice = new GameSpyVoiceChat(s_window);
    s_voice->addRef();
    return s_voice;
}

void setVoiceWindow(HWND window) {
    s_window = window;
}

VoiceChat* gameVoice() {
    return s_gameVoice;
}

void endGameVoice() {
    if (s_gameVoice != nullptr) {
        VoiceChat* v = s_gameVoice;
        s_gameVoice = nullptr;
        v->release();
    }
    if (s_voice != nullptr) {
        VoiceChat* v = s_voice;
        s_voice = nullptr;
        v->release();
    }
}

namespace {

VoiceIcons s_icons;
bool s_iconsLoaded = false;  // [0x0075F048]

}  // namespace

const VoiceIcons& voiceIcons() {
    return s_icons;
}

void loadVoiceIcons() {
    if (s_iconsLoaded) return;
    s_icons.off = "live_comm_off";
    s_icons.mute = "live_comm_mute";
    s_icons.on = "live_comm_on";
    s_icons.tv = "live_comm_tv";
    s_iconsLoaded = true;
}

const std::string& voiceIcon(VoiceChat& voice, int32_t player, int32_t user) {
    static const std::string none;
    if (voice.isMuted(player, user)) return s_icons.mute;
    const VoiceChat::RemoteUser* r = voice.findUser(user);
    if (r == nullptr || r->hasVoice == 0) return none;
    return voice.userTalking(user) ? s_icons.on : s_icons.off;
}

void makeGameVoice(std::function<void(int32_t on)> chatHook) {
    endGameVoice();
    s_gameVoice = acquireVoice();
    if (s_gameVoice != nullptr) s_gameVoice->chatHook = std::move(chatHook);
}

}  // namespace oj::net
