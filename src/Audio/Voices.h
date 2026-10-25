#pragma once
// The 3D sounds of the stock game's sound library (docs/spec/26-sound.md, "Virtual voices"): a world ([0x0075F868],
// 0x7C bytes, vtable 0x006FE838, made by 0x005B9260) holds a manager of virtual voices ([0x0075F88C], 0xA0 bytes,
// vtable 0x006FEA90, 0x005BBB20) whose instances (256 bytes each, vtable 0x006FEFF0) are given the buffers of the
// bank audio\3d (a player of 33 slots, Audio/Player.h) by their loudness every frame (0x005BC8E0); the others go on
// by the clock. An instance's emitter (0xA4 bytes, vtable 0x006FF1B0) works out its distance and volume against the
// listener (0x88 bytes, vtable 0x006FEFB0, [0x0075F8AC]); DirectSound's own 3D (distance and rolloff factors 0.001)
// only places it.

#include "Audio/Player.h"

#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

struct IDirectSound3DListener;

namespace oj {
class CrtRandom;
}

namespace oj::audio {

class Manager;

// The world matrix (3 rows of 4 floats, the place in the fourth column) of a node of the game's scene that an
// instance follows (+0xCC; 0x00534790 makes it). Given by the game (setNodeMatrix).
using NodeMatrix = const float* (*)(const void* node);
void setNodeMatrix(NodeMatrix fn);

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
};

// The listener's frame ([0x0075F8AC]: 0x005C0870; set from the camera, 0x005C0970).
class Listener {
public:
    Listener();
    ~Listener();
    // 0x005C0970: the camera's two rows (forward, up) through the camera's matrix (0x00420820), the basis worked out
    // (0x005C2460: right = up x forward, up again = forward x right, all normalised).
    void set(const float forward[4], const float up[4]);
    // +0x04 (0x005C0A10): distance and rolloff factors 0.001, the position (+0x58), the orientation, committed.
    void commit();
    Vec4 position;         // +0x58
    float right[3]{};      // +0x28, +0x38, +0x48 (rows of the basis)
    float up[3]{};         // +0x2C, +0x3C, +0x4C
    float forward[3]{};    // +0x30, +0x40, +0x50
    Vec4 frontRow;         // +0x08
    Vec4 upRow;            // +0x18
private:
    IDirectSound3DListener* m_ds = nullptr;  // +0x84
};

// The emitter's state of a frame (0x4C bytes, 0x005C2650, one an instance): worked out once a frame.
struct EmitterState {
    int done = 0;                // +0x00
    float distance = 0.0f;       // +0x04
    Vec4 position;               // +0x08: in the world
    float coneVolume = 1.0f;     // +0x18
    Vec4 relative;               // +0x1C: against the listener
    Vec4 previous;               // +0x2C
    Vec4 velocity;               // +0x3C
};

// 0x005C2670 (vtable 0x006FF1B0).
class Emitter {
public:
    void setPosition(const Vec4& p) { state->position = p; }                           // +0x04 (0x005C27B0)
    void setMinDistance(float d) { m_min = 2.0f * d; }                                 // +0x08 ([0x00725860] 2)
    void setMaxDistance(float d) { m_max = d; }                                        // +0x0C
    float volume() const { return m_volume; }                                          // +0x10: +0x70
    void setGain(float v) { m_gain = v; }                                              // +0x14: +0x6C
    void setRolloff(float r) { m_rolloff = r; }                                        // +0x18: +0x74
    void setDoppler(float d) { m_doppler = d; }                                        // +0x1C: +0x78
    // +0x24 (0x005C2840): a cone along the direction, its angles (radians) and the volume outside.
    void setCone(const Vec4& direction, float inner, float outer, float outside);
    // +0x28 (0x005C2B20): once a frame the relative position, its change and the distance; then the volume:
    // gain x min / ((clamp(distance, min, max) - min) x rolloff + min), times the cone's.
    void update(const Listener& listener, int nearest);
    float doppler() const { return m_doppler; }
    void resetFrame() { m_newVoice = 2; }  // +0x30
    EmitterState* state = nullptr;   // +0x1C
private:
    friend class Manager;
    int m_newVoice = 2;          // +0x04
    float m_min = 2.0f;          // +0x60
    float m_max = 100000.0f;     // +0x64
    float m_gain = 0.0f;         // +0x6C
    float m_volume = 0.0f;       // +0x70
    float m_rolloff = 0.0f;      // +0x74
    float m_doppler = 0.0f;      // +0x78
    bool m_cone = false;         // +0x8C
    Vec4 m_coneDirection;        // +0x7C
    float m_coneInner = 6.2831855f;  // +0x90
    float m_coneOuter = 6.2831855f;  // +0x94
    float m_coneOutside = 1.0f;  // +0x98
    float m_coneFactor = 1.0f;   // +0x9C
    float m_angle = 0.0f;        // +0xA0
};

// An instance (vtable 0x006FEFF0, made by the manager's +0x0C 0x005BC060 in its pool [0x0075F894]).
class Instance {
public:
    void play();                                   // +0x04 → the manager's +0x18
    void stop();                                   // +0x08: the manager's +0x1C
    void setSet(int bank, int set);                // +0x0C (0x005C0DB0)
    void setPosition(const Vec4& p);               // +0x10 (0x005C0ED0)
    void setOffset(const Vec4& v);                 // +0x14 (0x005C0FF0): nonzero → +0xA4
    void setNode(const void* node) { m_node = node; }  // +0x18 (0x004191A0): +0xCC, the node followed
    // +0x3C (0x005C0F00): the instance follows another's place: its emitter takes the other's state of the frame.
    void attachTo(Instance* master);
    void addRef() { ++m_refs; }
    void setVolume(float v);                       // +0x1C (0x005C0F40): +0xD0 → the emitter's gain
    void setFrequency(uint32_t hz) { m_frequency = hz; }  // +0x20: +0x5C
    // +0x24 (0x005C0FB0): rand() % range - range / 2 added to the frequency (+0x60); 0 for none. rand() / range.
    uint32_t randomFrequency(uint32_t range, CrtRandom& random);
    void setLooped(int loop) { m_loop = loop; }    // +0x28: +0x68
    void setMinDistance(float d);                  // +0x2C (0x005C1070)
    void setMaxDistance(float d);                  // +0x30 (0x005C1090)
    void setRolloff(float r);                      // +0x34 (0x005C10B0): times +0xE4 (5)
    void setRolloffScale(float r);                 // +0x38 (0x005C10E0)
    void setDoppler(float d);                      // +0x40 (0x005C1110): the emitter's +0x1C (its +0x78)
    // +0x44 (0x005C1130): the play position (+0x64); with a buffer (+0xE8) also its position (the player's +0x50).
    void setPlayPosition(uint32_t bytes);
    bool sounding() const;                         // +0x48 (0x005C1490): playing or virtual
    void setNearest(int v) { m_nearest = v; }      // +0x50: +0xF0
    void setCone(const Vec4& dir, float innerDeg, float outerDeg, float outside);  // +0x54 (0x005C1170)
    void setOcclusion(float v);                    // +0x58 (0x005C15F0): [0.15, 1]
    void setOcclusionOn(int v) { m_occlusionOn = v; }  // +0x5C: +0xF8
    void setLength(uint32_t ms);                   // +0x60 (0x005C1640): 0 clears flag 2
    void setGroup(int group) { m_group = group; }  // +0x64: +0xF4
    void setPriority(float p) { m_priority = p; }  // +0x68: +0x0C
    uint32_t frequency() const;                    // +0x6C (0x005C0F70)
    Emitter* emitter() const { return m_emitter; }  // +0x70
    void release();  // the refcount's last: back to the pool

    const std::string& name() const { return m_name; }

private:
    friend class Manager;
    Manager* m_manager = nullptr;     // +0x3C
    Emitter* m_emitter = nullptr;     // +0x40
    uint32_t m_index = 0;             // pool slot
    int m_refs = 1;                   // +0x04
    float m_volume = 1.0f;            // +0x08: the emitter's volume of the frame
    float m_priority = 1.0f;          // +0x0C
    std::string m_name;               // +0x10
    int m_list = 0;                   // +0x34: which list (Manager::List)
    Instance* m_attached = nullptr;   // +0x44
    Instance** m_owner = nullptr;     // +0x48: cleared as a flag-1 instance ends
    int m_bank = -1;                  // +0x4C
    int m_set = -1;                   // +0x50
    VoiceSet* m_voices = nullptr;     // +0x58
    uint32_t m_frequency = 48000;     // +0x5C
    int32_t m_random = 0;             // +0x60
    uint32_t m_position = 0;          // +0x64
    int m_loop = 1;                   // +0x68
    Vec4 m_place;                     // +0x6C
    uint32_t m_start = 0;             // +0x7C (ms)
    uint32_t m_elapsed = 0;           // +0x80
    uint32_t m_length = 0;            // +0x84 (ms)
    uint32_t m_limit = 0;             // +0x88 (ms)
    uint32_t m_size = 0;              // +0x8C (bytes)
    uint32_t m_rate = 0;              // +0x90
    Vec4 m_offset;                    // +0x94
    int m_hasOffset = 0;              // +0xA4
    Vec4 m_coneDirection;             // +0xA8
    int m_hasCone = 0;                // +0xB8
    float m_coneInner = 6.2831855f;   // +0xBC
    float m_coneOuter = 6.2831855f;   // +0xC0
    float m_coneOutside = 1.0f;       // +0xC4
    float m_occlusion = 1.0f;         // +0xC8
    const void* m_node = nullptr;     // +0xCC
    float m_gain = 0.0f;              // +0xD0
    float m_minDistance = 1.0f;       // +0xD8
    float m_maxDistance = 100000.0f;  // +0xDC
    float m_rolloff = 1.0f;           // +0xE0
    float m_rolloffScale = 5.0f;      // +0xE4 ([0x0071E750])
    uint32_t m_handle = 0;            // +0xE8: the 3D player's handle
    uint32_t m_flags = 0;             // +0xEC: 1 freed at its end, 2 a length of its own, 4 at the listener
    int m_nearest = -1;               // +0xF0
    int m_group = -1;                 // +0xF4
    int m_occlusionOn = 0;            // +0xF8
    uint32_t m_failures = 0;          // +0xFC
};

// The manager (vtable 0x006FEA90).
class Manager {
public:
    enum List { kIdle, kStarting, kVirtual, kHardware, kPaused, kStopping };  // +0x50 .. +0x64

    // 0x005BBB20: a pool of instances (channels), the listener, the lists, the volume groups at 1.
    Manager(uint32_t channels, CrtRandom& random);
    ~Manager();
    // +0x04 (0x005BC010): the 3D player whose slots are the buffers; its emitters made (one an instance).
    void setHardware(Player* player);
    Player* hardware() const { return m_hardware; }
    // +0x0C (0x005BC060): an instance from the pool (none once one is left), at the front of the idle list; with flag 1
    // owner is cleared as it ends.
    Instance* allocate(uint32_t flags, Instance** owner);
    void free(Instance* i);              // +0x10 (0x005BC140)
    void play(Instance* i);              // +0x18 (0x005BC1C0)
    void stop(Instance* i);              // +0x1C (0x005BC300)
    void pauseAll(bool paused);          // +0x20 (0x005BCFB0) / +0x24 (0x005BD320)
    bool paused() const { return m_paused; }
    // +0x28 (0x005BC8E0): the clock (0x005BC3C0), the ends (0x005BC650), the emitters and the culling below 0.01
    // (0x005BCAF0), the loudest (priority x volume) given buffers, the listener committed, the stopping stopped.
    void update(const Listener& listener, uint32_t flags);
    // +0x2C (0x005BBFC0): every instance's emitter starts its frame anew (+0x30).
    void resetEmitters() {
        for (Emitter& e : m_emitters) e.resetFrame();
    }
    void setPitch(float p);                                          // +0x30 (0x005BD350): [0.1, 1.5]
    void setGroupVolume(int g, float v) { m_groups[g] = v; }         // +0x34 (0x005BC000)
    Listener& listener() { return *m_listener; }
    CrtRandom& random() { return m_random; }
    // The listeners' data of the frame (the world's +0x28, given by the players' cameras, 0x004205A0): how many
    // (+0x00), each one's matrix (+0x04 .., kept by pointer; 0x00407D80 dots each row with an offset) and place
    // (+0x14 .., 0x10 bytes each); four at most. An emitter takes the listener its instance names (+0xF0), else the
    // nearest (0x005C2890: the first of the least squared distance, the first listener with none counted); one at
    // the listener (flag 4) the first's place.
    struct Listeners {
        uint32_t count = 0;
        std::array<std::array<float, 12>, 4> matrices = [] {
            std::array<std::array<float, 12>, 4> m{};
            for (auto& rows : m) rows = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
            return m;
        }();
        std::array<Vec4, 4> places{};
    };
    Listeners& listeners() { return m_listeners; }
    // A listener's place and the rows of its matrix (the first's by default): the camera's view matrix (the world
    // matrix's inverse, the camera's +0x48), whose rows give an offset in the listener's axes.
    void setListenerData(const Vec4& place, const float rows[12], size_t index = 0) {
        m_listeners.places[index] = place;
        for (int k = 0; k < 12; ++k) m_listeners.matrices[index][static_cast<size_t>(k)] = rows[k];
    }

private:
    friend class Instance;
    // The slot's user value (+0x80): the stock keeps the instance's address; here its pool place + 1 (0 none).
    Instance* userInstance(uint32_t user) const {
        return user != 0 && user <= m_pool.size() ? m_pool[user - 1].get() : nullptr;
    }
    void move(Instance* i, int list);     // 0x005C0D00: to the list's front
    std::list<Instance*>& list(int l) { return m_lists[l]; }
    void ticks();                         // 0x005BC3C0
    void ends();                          // 0x005BC650
    void cull(const Listener& l);         // 0x005BCAF0
    int acquire(Instance* i);             // 0x005BCD10
    void start(Instance* i);              // 0x005C11F0
    void refresh(Instance* i);            // 0x005C1360
    void stopVoice(Instance* i);          // 0x005BC360 part
    void emitterOf(Instance* i, const Listener& l);  // 0x005C14B0
    uint32_t nowMs() const;               // +0x88's counter x 1000 / frequency

    CrtRandom& m_random;
    Player* m_hardware = nullptr;         // +0x2C
    uint32_t m_hardwareSlots = 0;         // +0x08
    float m_groups[8] = {1, 1, 1, 1, 1, 1, 1, 1};  // +0x0C
    std::unique_ptr<Listener> m_listener; // +0x34
    bool m_paused = false;                // +0x38
    uint64_t m_now = 0;                   // +0x40
    float m_pitch = 1.0f;                 // +0x4C
    std::list<Instance*> m_lists[6];
    std::vector<Instance*> m_candidates;  // +0x68
    std::vector<std::unique_ptr<Instance>> m_pool;
    std::vector<Emitter> m_emitters;
    std::vector<EmitterState> m_states;
    std::vector<uint32_t> m_free;         // [0x0075F898]
    uint32_t m_top = 0;                   // [0x0075F89C]
    Listeners m_listeners;  // the world's +0x28
};

// The world [0x0075F868] (0x005B9330): the system, the manager (+0x1C), the 3D player of audio\3d (+0x20) with the
// volume group 0 at 0.85, the listener's object (+0x24), the volume (+0x08) going to its target (+0x0C) by +0x10 a
// second down and +0x14 up.
class World {
public:
    World(const GameFiles& files, uint32_t channels, CrtRandom& random);
    ~World();
    // 0x005B9910: an instance of the set of the name (none: no set), following node (none: its own place), flags
    // as allocate's, at volume 1 and rolloff 1.
    Instance* create(const char* name, const void* node, uint32_t flags, Instance** owner = nullptr);
    // 0x005B9A20 / 0x005B99B0: the same with its +0x64 (volume group) 0.
    Instance* createGrouped(const char* name, uint32_t flags);
    // 0x005B99B0: an instance of the set (bank, set), flags as allocate's, its set, volume 1, rolloff 1, group 0.
    Instance* createRef(const SetRef& ref, uint32_t flags);
    // 0x005B9B00 / 0x005B9A80: a one-shot (flag 1) of the set at the position given, played.
    Instance* playOnce(const char* name, const Vec4& place, Instance** owner);
    // 0x005B9A80: the same by the set (bank, set): allocate (flag 1, owner), set, position, volume 1, rolloff 1, not
    // looping, played.
    Instance* playOnceRef(const SetRef& ref, const Vec4& place, Instance** owner);
    // 0x005B96C0: the volume to its target, the player's master, then the manager's update.
    void update(float dt);
    Manager& manager() { return *m_manager; }
    Player& player() { return *m_player; }
    // +0x08 .. +0x14
    float volume = 1.0f;
    float target = 1.0f;
    float down = 20.0f;
    float up = 50.0f;
private:
    std::unique_ptr<Player> m_player;
    std::unique_ptr<Manager> m_manager;
};

}  // namespace oj::audio
