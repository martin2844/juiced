#include "Audio/Voices.h"

#include "Audio/SoundSystem.h"
#include "Core/Base/Log.h"
#include "Core/Base/Random.h"

#include <windows.h>
#include <mmreg.h>
#include <dsound.h>

#include <algorithm>
#include <cmath>

namespace oj::audio {

namespace {

NodeMatrix g_nodeMatrix = nullptr;

constexpr float kCull = 0.01f;  // [0x0071E724]

float length3(float x, float y, float z) {
    return std::sqrt(x * x + y * y + z * z);  // 0x00407C80
}

// cvtss2si / fistp with the control word cut: the main thread's conversions of the library cut toward zero.
int32_t cut(float f) { return static_cast<int32_t>(f); }

// An unsigned value as the x87 loads it (fild, and 2^32 added below 0).
float unsignedFloat(uint32_t v) { return static_cast<float>(static_cast<double>(v)); }

}  // namespace

void setNodeMatrix(NodeMatrix fn) { g_nodeMatrix = fn; }

// --- Listener ---

Listener::Listener() {
    frontRow = {0.0f, 0.0f, 1.0f, 0.0f};
    upRow = {0.0f, 1.0f, 0.0f, 0.0f};
    IDirectSoundBuffer* primary = system().primary();
    if (primary != nullptr) {
        primary->QueryInterface(IID_IDirectSound3DListener, reinterpret_cast<void**>(&m_ds));
        if (m_ds != nullptr) {
            // 0x005C0870: the smallest distance factor, the largest rolloff, deferred. Windows' DirectSound refuses the
            // rolloff (above DS3D_MAXROLLOFFFACTOR) and keeps its 1; Wine's takes it, but nothing is worked out with it:
            // the voices' places are deferred, and the commit that applies them (each frame of the manager) gives 0.001.
            m_ds->SetDistanceFactor(FLT_MIN, DS3D_DEFERRED);
            m_ds->SetRolloffFactor(FLT_MAX, DS3D_DEFERRED);
        }
    }
}

Listener::~Listener() {
    if (m_ds != nullptr) m_ds->Release();
}

void Listener::set(const float f[4], const float u[4]) {
    frontRow = {f[0], f[1], f[2], f[3]};
    upRow = {u[0], u[1], u[2], u[3]};
    // 0x005C2460
    float rx = upRow.y * frontRow.z - upRow.z * frontRow.y;
    float ry = upRow.z * frontRow.x - frontRow.z * upRow.x;
    float rz = frontRow.y * upRow.x - upRow.y * frontRow.x;
    float n = 1.0f / (length3(rx, ry, rz) + 1.1920929e-07f);
    rx *= n, ry *= n, rz *= n;
    const float ux = rz * frontRow.y - ry * frontRow.z;
    const float uy = rx * frontRow.z - rz * frontRow.x;
    const float uz = ry * frontRow.x - rx * frontRow.y;
    const float un = 1.0f / (length3(ux, uy, uz) + 1.1920929e-07f);
    const float fn = 1.0f / (length3(frontRow.x, frontRow.y, frontRow.z) + 1.1920929e-07f);
    right[0] = rx, right[1] = ux * un, right[2] = frontRow.x * fn;
    up[0] = ry, up[1] = uy * un, up[2] = frontRow.y * fn;
    forward[0] = rz, forward[1] = uz * un, forward[2] = frontRow.z * fn;
}

void Listener::commit() {
    if (m_ds == nullptr) return;
    m_ds->SetDistanceFactor(0.001f, DS3D_DEFERRED);  // [0x00725850]
    m_ds->SetRolloffFactor(0.001f, DS3D_DEFERRED);   // [0x0072584C]
    m_ds->SetPosition(position.x, position.y, position.z, DS3D_DEFERRED);
    m_ds->SetOrientation(frontRow.x, frontRow.y, frontRow.z, upRow.x, upRow.y, upRow.z, DS3D_DEFERRED);
    m_ds->CommitDeferredSettings();
}

// --- Emitter ---

void Emitter::setCone(const Vec4& direction, float inner, float outer, float outside) {
    m_cone = true;
    m_coneDirection = direction;
    m_coneInner = inner;
    m_coneOuter = outer;
    m_coneOutside = outside;
}

void Emitter::update(const Listener& /*listener*/, int /*nearest*/) {
    // The relative place was given with the listener's data by the manager (Manager::emitterOf).
    float d = state != nullptr ? state->distance : 0.0f;
    if (d < m_min) d = m_min;
    if (m_max < d) d = m_max;
    float v = (m_gain * m_min) / ((d - m_min) * m_rolloff + m_min);
    m_volume = v;
    if (!m_cone && state == nullptr) return;
    if (state != nullptr) m_coneFactor = state->coneVolume;
    m_volume = v * m_coneFactor;
}

// --- Instance ---

void Instance::play() { m_manager->play(this); }

void Instance::attachTo(Instance* master) {
    m_attached = master;
    if (m_emitter != nullptr && master != nullptr && master->m_emitter != nullptr) {
        m_emitter->state = master->m_emitter->state;
    }
}
void Instance::stop() { m_manager->stop(this); }

void Instance::setSet(int bank, int set) {
    // 0x005C0DB0
    if (m_bank == bank && m_set == set) return;
    m_bank = bank;
    m_set = set;
    Player* hw = m_manager->hardware();
    if (m_handle != 0 && hw != nullptr) {
        int state = 0;
        if (hw->state(m_handle, state) == 0) {
            hw->fadeOut(m_handle);  // the 3D handle's +0x20
            hw->setUser(m_handle, 0);
        }
    }
    m_handle = 0;
    if (m_set == -1 || hw == nullptr) {
        m_voices = nullptr;
        return;
    }
    SoundSet* s = hw->setOf({m_bank, m_set});
    m_voices = s != nullptr ? s->voices() : nullptr;
    m_name = s != nullptr ? s->name() : std::string();
    if (m_voices != nullptr) {
        m_rate = s->rate2;
        m_size = static_cast<uint32_t>(cut(s->size + 0.5f));  // 0x005C3ED0: fistp of the size
        m_length = s->milliseconds;
    }
    m_position = 0;
    m_elapsed = 0;
    m_start = m_manager->nowMs();
}

void Instance::setPosition(const Vec4& p) {
    m_place = p;
    if (m_emitter != nullptr) m_emitter->setPosition(p);
}

void Instance::setOffset(const Vec4& v) {
    m_offset = v;
    m_hasOffset = (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f) ? 0 : 1;
}

void Instance::setVolume(float v) {
    m_gain = v;
    if (m_emitter != nullptr) m_emitter->setGain(v);
}

uint32_t Instance::randomFrequency(uint32_t range, CrtRandom& random) {
    if (range == 0) {
        m_random = 0;
        return 0;
    }
    const uint32_t r = static_cast<uint32_t>(random.next());
    m_random = static_cast<int32_t>(r % range - (range >> 1));
    return r / range;
}

void Instance::setMinDistance(float d) {
    m_minDistance = d;
    if (m_emitter != nullptr) m_emitter->setMinDistance(d);
}

void Instance::setMaxDistance(float d) {
    m_maxDistance = d;
    if (m_emitter != nullptr) m_emitter->setMaxDistance(d);
}

void Instance::setRolloff(float r) {
    m_rolloff = r;
    if (m_emitter != nullptr) m_emitter->setRolloff(r * m_rolloffScale);
}

void Instance::setRolloffScale(float r) {
    m_rolloffScale = r;
    if (m_emitter != nullptr) m_emitter->setRolloff(r * m_rolloff);
}

void Instance::setDoppler(float d) {
    if (m_emitter != nullptr) m_emitter->setDoppler(d);
}

void Instance::setPlayPosition(uint32_t bytes) {
    m_position = bytes;
    if (m_handle != 0 && m_manager->hardware() != nullptr) m_manager->hardware()->setPosition(m_handle, m_position);
}

bool Instance::sounding() const {
    return m_list == Manager::kHardware || m_list == Manager::kVirtual;
}

void Instance::setCone(const Vec4& dir, float innerDeg, float outerDeg, float outside) {
    // 0x005C1170: degrees to radians ([0x006FF9B0]).
    m_coneDirection = dir;
    m_coneInner = innerDeg * 0.017453292f;
    m_coneOutside = outside;
    m_coneOuter = outerDeg * 0.017453292f;
    m_hasCone = 1;
}

void Instance::setOcclusion(float v) {
    constexpr float kLowest = 0.15f;  // [0x00725854]
    m_occlusion = v < kLowest ? kLowest : (1.0f < v ? 1.0f : v);
}

void Instance::setLength(uint32_t ms) {
    if (ms != 0) {
        m_limit = ms;
        m_flags |= 2;
    } else {
        m_flags &= ~2u;
    }
}

uint32_t Instance::frequency() const {
    if (m_voices != nullptr && m_manager->hardware() != nullptr && m_handle != 0) {
        uint32_t hz = 0;
        if (m_manager->hardware()->frequency(m_handle, hz) == 0) return hz;
    }
    return m_frequency;
}

void Instance::release() {
    if (--m_refs > 0) return;
    m_manager->free(this);
}

// --- Manager ---

Manager::Manager(uint32_t channels, CrtRandom& random) : m_random(random) {
    m_pool.resize(channels);
    m_free.resize(channels);
    for (uint32_t i = 0; i < channels; ++i) m_free[i] = i;
    m_top = channels - 1;
    m_listener = std::make_unique<Listener>();
}

Manager::~Manager() = default;

uint32_t Manager::nowMs() const {
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    return static_cast<uint32_t>(static_cast<uint64_t>(c.QuadPart) * 1000u / static_cast<uint64_t>(f.QuadPart));
}

void Manager::setHardware(Player* player) {
    m_hardware = player;
    // 0x005C17D0: an emitter and its state for each instance of the pool.
    m_emitters.assign(m_pool.size(), Emitter{});
    m_states.assign(m_pool.size(), EmitterState{});
    m_hardwareSlots = player != nullptr ? player->slots() : 0;
}

void Manager::move(Instance* i, int l) {
    if (i->m_list == l) return;
    auto& from = m_lists[i->m_list];
    from.erase(std::find(from.begin(), from.end(), i));
    m_lists[l].push_front(i);
    i->m_list = l;
}

Instance* Manager::allocate(uint32_t flags, Instance** owner) {
    if (m_top == 0) {
        if (owner != nullptr) *owner = nullptr;
        return nullptr;
    }
    const uint32_t index = m_free[m_top];
    --m_top;
    auto inst = std::make_unique<Instance>();
    Instance* i = inst.get();
    m_pool[index] = std::move(inst);
    i->m_manager = this;
    i->m_index = index;
    // 0x005C0AC0 with the slot's emitter (the 3D player's +0xF8, 0x005C19A0): placed at the instance, min 1 (x2),
    // max 100000, rolloff 1 x 5, doppler 2.
    Emitter& e = m_emitters[index];
    e = Emitter{};
    e.state = &m_states[index];
    *e.state = EmitterState{};
    i->m_emitter = &e;
    e.setPosition(i->m_place);
    e.setMinDistance(i->m_minDistance);
    e.setMaxDistance(i->m_maxDistance);
    e.setRolloff(i->m_rolloffScale * i->m_rolloff);
    e.setDoppler(2.0f);
    i->m_flags = flags;
    if ((flags & 1) != 0) i->m_owner = owner;
    i->m_list = kIdle;
    m_lists[kIdle].push_front(i);
    if (owner != nullptr) *owner = i;
    return i;
}

void Manager::free(Instance* i) {
    // 0x005BC140: the instance's buffer faded out and given back, its slot back to the pool.
    if (m_hardware != nullptr && i->m_handle != 0) {
        int state = 0;
        if (m_hardware->state(i->m_handle, state) == 0) {
            m_hardware->fadeOut(i->m_handle);
            m_hardware->setUser(i->m_handle, 0);
        }
    }
    i->m_handle = 0;
    auto& l = m_lists[i->m_list];
    l.erase(std::find(l.begin(), l.end(), i));
    const uint32_t index = i->m_index;
    m_free[m_top + 1] = index;
    ++m_top;
    m_pool[index].reset();
}

void Manager::stopVoice(Instance* i) {
    // The 3D handle's +0x20 (0x005BD990) fades the voice out (Player::fadeOut): silent at once, the slot held in
    // state 5 until the player's next frame stops it; till then the oldest-voice steal of another instance is
    // refused (acquire).
    if (m_hardware != nullptr && i->m_handle != 0) {
        int state = 0;
        if (m_hardware->state(i->m_handle, state) == 0) {
            m_hardware->fadeOut(i->m_handle);
            m_hardware->setUser(i->m_handle, 0);
        }
    }
    i->m_handle = 0;
}

void Manager::play(Instance* i) {
    // 0x005BC1C0: a looping instance with a buffer plays on; else its buffer goes, it starts (to the starting list)
    // at the clock's now.
    if (i->m_handle != 0) {
        if (i->m_loop == 1) return;
        stopVoice(i);
    }
    move(i, kStarting);
    i->m_position = 0;
    i->m_elapsed = 0;
    i->m_start = nowMs();
    i->m_handle = 0;
    if (i->m_voices != nullptr) {
        SoundSet& s = i->m_voices->set();
        i->m_rate = s.rate2;
        i->m_size = static_cast<uint32_t>(cut(s.size + 0.5f));
        i->m_length = s.milliseconds;
        if ((i->m_flags & 2) == 0) i->m_limit = i->m_length;
    }
}

void Manager::stop(Instance* i) {
    // 0x005BC300: the buffer stopped and given back; idle.
    stopVoice(i);
    move(i, kIdle);
}

void Manager::pauseAll(bool paused) {
    if (!paused) {
        // 0x005BD320: the paused ones played again.
        m_paused = false;
        while (!m_lists[kPaused].empty()) play(m_lists[kPaused].front());
        return;
    }
    // 0x005BCFB0: every playing one paused (its buffer given back).
    m_paused = true;
    for (int l : {kStarting, kVirtual, kHardware, kStopping}) {
        while (!m_lists[l].empty()) {
            Instance* i = m_lists[l].front();
            stopVoice(i);
            move(i, kPaused);
        }
    }
}

void Manager::setPitch(float p) {
    m_pitch = p < 0.1f ? 0.1f : (1.5f < p ? 1.5f : p);
}

void Manager::ticks() {
    // 0x005BC3C0: the virtual ones go on by the clock.
    m_now = nowMs();
    auto& l = m_lists[kVirtual];
    for (auto it = l.begin(); it != l.end();) {
        Instance* i = *it++;
        const uint32_t elapsed = static_cast<uint32_t>(m_now) - i->m_start;
        i->m_elapsed = elapsed;
        if (elapsed < i->m_length && i->m_position < i->m_size && elapsed < i->m_limit) {
            i->m_position = static_cast<uint32_t>(cut(unsignedFloat(i->m_rate * elapsed) * 0.001f));
            if (i->m_loop == 0) {
                if ((i->m_flags & 1) != 0) {
                    if (i->m_owner != nullptr) *i->m_owner = nullptr;
                    i->release();
                } else {
                    move(i, kIdle);
                }
            } else {
                i->m_position %= i->m_size;
                move(i, kStarting);
            }
        } else if (i->m_loop == 0) {
            if ((i->m_flags & 1) != 0) {
                if (i->m_owner != nullptr) *i->m_owner = nullptr;
                i->release();
            } else {
                move(i, kIdle);
            }
        } else {
            const uint32_t over = (elapsed - i->m_length) * i->m_rate;
            i->m_position = static_cast<uint32_t>(cut(unsignedFloat(over) * 0.001f)) % i->m_size;
            move(i, kStarting);
        }
    }
}

void Manager::ends() {
    // 0x005BC650: the ones on buffers that ended (not looping).
    auto& l = m_lists[kHardware];
    for (auto it = l.begin(); it != l.end();) {
        Instance* i = *it++;
        i->m_elapsed = static_cast<uint32_t>(m_now) - i->m_start;
        if (i->m_loop != 0) continue;
        int state = 0;
        if (m_hardware != nullptr && m_hardware->state(i->m_handle, state) == 0) {
            if (state == 3 || state == 4) {
                if ((i->m_flags & 2) != 0 && i->m_limit <= i->m_elapsed) {
                    if ((i->m_flags & 1) != 0) {
                        if (i->m_owner != nullptr) *i->m_owner = nullptr;
                        i->release();
                    } else {
                        stopVoice(i);
                        move(i, kIdle);
                    }
                }
            } else if ((i->m_flags & 1) != 0) {
                if (i->m_owner != nullptr) *i->m_owner = nullptr;
                i->release();
            } else {
                stopVoice(i);
                move(i, kIdle);
            }
        } else if ((i->m_flags & 1) == 0) {
            // The buffer went to another instance: given back, the instance stopping (or idle).
            stopVoice(i);
            move(i, kStopping);
        } else {
            if (i->m_owner != nullptr) *i->m_owner = nullptr;
            i->release();
        }
    }
}

void Manager::emitterOf(Instance* i, const Listener& l) {
    // 0x005C14B0: the emitter placed (at the listener with flag 4; at the node it follows, with its offset turned by
    // the node's matrix; its cone), then updated (0x005C2B20).
    Emitter* e = i->m_emitter;
    if (e == nullptr) return;
    if (i->m_attached != nullptr) {
        emitterOf(i->m_attached, l);
    } else if (i->m_node == nullptr) {
        if ((i->m_flags & 4) != 0) e->setPosition(m_listeners.places[0]);
        if (i->m_hasCone != 0) e->setCone(i->m_coneDirection, i->m_coneInner, i->m_coneOuter, i->m_coneOutside);
    } else if (g_nodeMatrix != nullptr) {
        const float* m = g_nodeMatrix(i->m_node);
        i->m_place = {m[3], m[7], m[11], i->m_place.w};
        if (i->m_hasOffset != 0) {
            const Vec4& o = i->m_offset;
            i->m_place.x = (m[0] * o.x + m[1] * o.y + m[2] * o.z) + i->m_place.x;
            i->m_place.y = (m[4] * o.x + m[5] * o.y + m[6] * o.z) + i->m_place.y;
            i->m_place.z = (m[8] * o.x + m[9] * o.y + m[10] * o.z) + i->m_place.z;
        }
        e->setPosition(i->m_place);
        if (i->m_hasCone != 0) {
            const Vec4& d = i->m_coneDirection;
            const Vec4 turned{m[0] * d.x + m[1] * d.y + m[2] * d.z, m[4] * d.x + m[5] * d.y + m[6] * d.z,
                              m[8] * d.x + m[9] * d.y + m[10] * d.z, d.w};
            e->setCone(turned, i->m_coneInner, i->m_coneOuter, i->m_coneOutside);
        }
    }
    // 0x005C2B20 against the listener's data: once a frame the place relative to it and its change.
    EmitterState& s = *e->state;
    if (s.done == 0) {
        s.done = 1;
        s.previous = s.relative;
        // 0x005C2890: the instance's listener, else the nearest.
        size_t k = 0;
        if (i->m_nearest == -1) {
            float best = 3.4028235e+38f;
            for (size_t n = 0; n < m_listeners.count && n < m_listeners.places.size(); ++n) {
                const Vec4& at = m_listeners.places[n];
                const float x = s.position.x - at.x, y = s.position.y - at.y, z = s.position.z - at.z;
                const float d = y * y + x * x + z * z;
                if (d < best) {
                    k = n;
                    best = d;
                }
            }
        } else {
            k = static_cast<size_t>(i->m_nearest);
        }
        const Vec4& place = m_listeners.places[k];
        const std::array<float, 12>& m = m_listeners.matrices[k];
        const float dx = s.position.x - place.x;
        const float dy = s.position.y - place.y;
        const float dz = s.position.z - place.z;
        // 0x00407D80 with the listener's matrix: each row dotted with the offset.
        s.relative.x = m[0] * dx + m[1] * dy + m[2] * dz;
        s.relative.y = m[4] * dx + m[5] * dy + m[6] * dz;
        s.relative.z = m[8] * dx + m[9] * dy + m[10] * dz;
        s.relative.w = dz;
        if (e->m_newVoice != 0) s.previous = s.relative;
        s.velocity = {s.relative.x - s.previous.x, s.relative.y - s.previous.y, s.relative.z - s.previous.z,
                      s.relative.w};
        s.distance = length3(s.relative.x, s.relative.y, s.relative.z);
        e->m_newVoice = 0;
    }
    e->update(l, i->m_nearest);
    i->m_volume = e->volume();
}

void Manager::cull(const Listener& l) {
    // 0x005BCAF0: the ones on buffers and the starting ones: their emitters; quieter than 0.01 → virtual (their buffer
    // given back), the others candidates.
    m_candidates.clear();
    for (int which : {kHardware, kStarting}) {
        auto& list = m_lists[which];
        for (auto it = list.begin(); it != list.end();) {
            Instance* i = *it++;
            emitterOf(i, l);
            if (i->m_set == -1) continue;
            if (i->m_voices != nullptr) i->m_voices->set().users = 0;
            if (i->m_volume <= kCull) {
                move(i, kVirtual);
                stopVoice(i);
            } else {
                m_candidates.push_back(i);
            }
        }
    }
}

namespace {

// MSVC's heap functions of the time (the stock's 0x005BD410 .. 0x005BD580), ordered by priority x volume, the
// loudest first.
float score(const Instance* i, float priority, float volume) {
    (void)i;
    return priority * volume;
}

}  // namespace

int Manager::acquire(Instance* i) {
    // 0x005BCD10
    if (i->m_voices == nullptr) return 3;
    SoundSet& set = i->m_voices->set();
    if (set.users == set.instances()) return 3;
    Player& hw = *m_hardware;
    const SetRef ref{i->m_bank, i->m_set};
    uint32_t voice = 0;
    int how = 0;
    if (set.allBusy() || set.findFree(voice) != 0) {
        if (set.stealQuietest(voice) == 1) {
            how = 1;
        } else if (set.stealOldest(voice) == 2) {
            how = 2;
        } else {
            return 3;
        }
    }
    int result = 1;
    uint32_t handle = 0;
    if (how != 0) {
        handle = set.owner(voice);
        if (handle != 0) {
            int state = 0;
            hw.state(handle, state);
            if (state == 5 && how != 1) return 3;
            uint32_t user = 0;
            if (hw.user(handle, user) != 0) {
                log::info("sound: handle lost");
                return 3;
            }
            Instance* other = userInstance(user);
            if (other != nullptr) {
                i->m_handle = other->m_handle;
                other->m_handle = 0;
                move(other, kStarting);
            } else {
                i->m_handle = handle;
            }
            hw.renew(i->m_handle);
            hw.assign(i->m_handle, &set, voice);
            if (i->m_loop == 1) {
                set.setOwner(voice, i->m_handle);
                result = 2;
            }
            return result;
        }
    }
    int slotHow = 0;
    if (hw.findSlot(0xF, handle, slotHow) != 0) {
        set.release(voice);
        return 3;
    }
    uint32_t user = 0;
    hw.user(handle, user);
    if (user != 0) {
        // 0x005BCF45: the slot still has an instance: that one's voice stopped and it starting again (0x005BC360),
        // none for this one this frame. Stock problem: the voice findFree took stays busy; a set of one voice whose
        // start time is still 0 (never played) can then give none (stealOldest finds nothing) for the race. The fixed
        // build gives the voice back, as when no slot is found.
        if (Instance* other = userInstance(user)) {
            stopVoice(other);
            move(other, kStarting);
        }
        set.release(voice);
        return 3;
    }
    i->m_handle = handle;
    hw.renew(i->m_handle);
    hw.assign(i->m_handle, &set, voice);
    return result;
}

void Manager::start(Instance* i) {
    // 0x005C11F0, nothing without a set's voices: the buffer taken: the instance as its user, the set's users counted;
    // a loop's start drawn at random (rand() x 4 % its bytes; given by +0x50, which the 3D player passes over);
    // looping; the frequency (its own and the random part, times the pitch); the volume (its group's); placed.
    if (i->m_voices == nullptr) return;
    Player& hw = *m_hardware;
    hw.setUser(i->m_handle, i->m_index + 1);
    ++i->m_voices->set().users;
    if (i->m_loop == 1) {
        const uint32_t size = i->m_size;
        const int r = m_random.next();
        i->m_position = size != 0 ? static_cast<uint32_t>(r * 4) % size : 0;
    }
    hw.setPlayFlags(i->m_handle, static_cast<uint32_t>(i->m_loop));
    const float hz = unsignedFloat(static_cast<uint32_t>(i->m_random + static_cast<int32_t>(i->m_frequency))) * m_pitch;
    hw.setFrequency(i->m_handle, static_cast<uint32_t>(static_cast<int32_t>(std::nearbyint(hz))));
    float v = i->m_volume;
    v = v < 0.0f ? 0.0f : (1.0f < v ? 1.0f : v);
    i->m_volume = v;
    if (i->m_group != -1) v = m_groups[i->m_group] * i->m_volume;
    hw.setVolume(i->m_handle, v);
    hw.place3d(i->m_handle, *i->m_emitter, i->m_emitter->doppler());
}

void Manager::refresh(Instance* i) {
    // 0x005C1360, nothing without a set's voices: a buffer kept counts as a user of its set (as start does), so a
    // set's voices all kept, another instance of it gets none this frame (acquire) rather than taking one; its
    // volume, frequency, occlusion and place.
    if (i->m_voices == nullptr) return;
    ++i->m_voices->set().users;
    Player& hw = *m_hardware;
    float v = i->m_volume;
    v = v < 0.0f ? 0.0f : (1.0f < v ? 1.0f : v);
    i->m_volume = v;
    if (i->m_group != -1) v = m_groups[i->m_group] * i->m_volume;
    hw.setVolume(i->m_handle, v);
    const float hz = unsignedFloat(static_cast<uint32_t>(i->m_random + static_cast<int32_t>(i->m_frequency))) * m_pitch;
    hw.setFrequency(i->m_handle, static_cast<uint32_t>(static_cast<int32_t>(std::nearbyint(hz))));
    if (i->m_occlusionOn != 0) hw.setOcclusion(i->m_handle, i->m_occlusion);
    hw.place3d(i->m_handle, *i->m_emitter, i->m_emitter->doppler());
}

void Manager::update(const Listener& l, uint32_t flags) {
    (void)flags;
    if (m_paused) return;
    ticks();
    ends();
    cull(l);
    // 0x005BD410 over the candidates: the first (slots - 1) by priority x volume, loudest first.
    const size_t take = std::min(m_candidates.size(), static_cast<size_t>(m_hardwareSlots - 1));
    auto key = [](const Instance* i) { return i->m_priority * i->m_volume; };
    auto adjust = [&](Instance** first, ptrdiff_t hole, ptrdiff_t bottom, Instance* val) {
        const ptrdiff_t top = hole;
        ptrdiff_t idx = 2 * hole + 2;
        for (; idx < bottom; idx = 2 * idx + 2) {
            if (key(first[idx - 1]) < key(first[idx])) --idx;
            first[hole] = first[idx];
            hole = idx;
        }
        if (idx == bottom) {
            first[hole] = first[bottom - 1];
            hole = bottom - 1;
        }
        for (ptrdiff_t up = (hole - 1) / 2; top < hole && key(val) < key(first[up]); up = (hole - 1) / 2) {
            first[hole] = first[up];
            hole = up;
        }
        first[hole] = val;
    };
    Instance** first = m_candidates.data();
    const ptrdiff_t mid = static_cast<ptrdiff_t>(take);
    const ptrdiff_t last = static_cast<ptrdiff_t>(m_candidates.size());
    if (mid > 1) {
        for (ptrdiff_t hole = (mid - 2) / 2 + 1; hole > 0;) {
            --hole;
            adjust(first, hole, mid, first[hole]);
        }
    }
    for (ptrdiff_t next = mid; next < last; ++next) {
        if (mid > 0 && key(first[0]) < key(first[next])) {
            Instance* v = first[next];
            first[next] = first[0];
            adjust(first, 0, mid, v);
        }
    }
    for (ptrdiff_t end = mid; end > 1; --end) {
        Instance* v = first[end - 1];
        first[end - 1] = first[0];
        adjust(first, 0, end - 1, v);
    }
    m_hardware->unholdAll();  // the 3D player's +0x3C
    uint32_t given = 1;
    for (Instance* i : m_candidates) {
        if (given < m_hardwareSlots) {
            int state = 0;
            if (i->m_handle != 0 && m_hardware->state(i->m_handle, state) == 0) {
                i->m_failures = 0;
                refresh(i);
                m_hardware->hold(i->m_handle);
                ++given;
            } else {
                const int r = acquire(i);
                if (r == 3) {
                    move(i, kVirtual);
                    ++i->m_failures;
                } else {
                    start(i);
                    if (r != 1 || m_hardware->start(i->m_handle) == 0) {
                        m_hardware->setOcclusion(i->m_handle, i->m_occlusion);
                        move(i, kHardware);
                        m_hardware->hold(i->m_handle);
                        ++given;
                    } else {
                        // 0x005BC9F4: a voice that would not play: virtual, not held, not counted.
                        move(i, kVirtual);
                    }
                }
            }
        } else {
            move(i, kVirtual);
        }
    }
    m_candidates.clear();
    for (EmitterState& s : m_states) s.done = 0;  // the 3D player's +0x104
    m_listener->commit();
    while (!m_lists[kStopping].empty()) stop(m_lists[kStopping].front());
}

// --- World ---

World::World(const GameFiles& files, uint32_t channels, CrtRandom& random) {
    m_manager = std::make_unique<Manager>(channels, random);
    m_player = std::make_unique<Player>(files, std::filesystem::path("audio"), "3d");
    m_manager->setHardware(m_player.get());
    m_manager->setGroupVolume(0, 0.85f);
}

World::~World() = default;

Instance* World::create(const char* name, const void* node, uint32_t flags, Instance** owner) {
    SetRef ref{-1, -1};
    if (name != nullptr && m_player->find(name, ref) != 0) return nullptr;
    Instance* i = m_manager->allocate(flags, owner);
    if (i == nullptr) return nullptr;
    i->setNode(node);
    i->setVolume(1.0f);
    i->setRolloff(1.0f);
    if (name != nullptr) i->setSet(ref.bank, ref.set);
    return i;
}

Instance* World::createGrouped(const char* name, uint32_t flags) {
    SetRef ref{-1, -1};
    if (name == nullptr || m_player->find(name, ref) != 0) return nullptr;
    Instance* i = m_manager->allocate(flags, nullptr);
    if (i == nullptr) return nullptr;
    i->setSet(ref.bank, ref.set);
    i->setVolume(1.0f);
    i->setRolloff(1.0f);
    i->setGroup(0);
    return i;
}

Instance* World::createRef(const SetRef& ref, uint32_t flags) {
    Instance* i = m_manager->allocate(flags, nullptr);
    if (i == nullptr) return nullptr;
    i->setSet(ref.bank, ref.set);
    i->setVolume(1.0f);
    i->setRolloff(1.0f);
    i->setGroup(0);
    return i;
}

Instance* World::playOnce(const char* name, const Vec4& place, Instance** owner) {
    SetRef ref{-1, -1};
    if (m_player->find(name, ref) != 0) return nullptr;
    return playOnceRef(ref, place, owner);
}

Instance* World::playOnceRef(const SetRef& ref, const Vec4& place, Instance** owner) {
    Instance* i = m_manager->allocate(1, owner);
    if (i == nullptr) return nullptr;
    i->setSet(ref.bank, ref.set);
    i->setPosition(place);
    i->setVolume(1.0f);
    i->setRolloff(1.0f);
    i->setLooped(0);
    i->play();
    return i;
}

void World::update(float dt) {
    // 0x005B96C0
    float d = target - volume;
    if (0.0f < std::fabs(d)) {
        d = d / dt;
        if (0.0f <= d) {
            d = d / up;
            if (d < 1e-06f) d = 1e-06f;
        } else {
            d = d / down;
            if (std::fabs(d) < 1e-06f) d = -1e-06f;
        }
        volume = d + volume;
        float v = volume;
        constexpr float kLow = 0.001f;  // [0x0071E714]
        if (kLow <= v) {
            if (1.0f < v) v = 1.0f;
        } else {
            v = kLow;
        }
        volume = v;
        if (volume <= kLow) volume = 0.0f;
        m_player->setMasterVolume(volume);
    }
    m_manager->update(m_manager->listener(), 0);
}

}  // namespace oj::audio
