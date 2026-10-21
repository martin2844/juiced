#include "Audio/Player.h"

#include "Audio/SoundSystem.h"
#include "Audio/Voices.h"
#include "Core/Base/Log.h"

#include <windows.h>

#include <cstdio>

namespace oj::audio {

namespace {
std::vector<Player*>& players() {
    static std::vector<Player*> p;
    return p;
}
}  // namespace

Player::Player() { players().push_back(this); }

Player::~Player() {
    auto& all = players();
    for (size_t i = 0; i < all.size(); ++i) {
        if (all[i] == this) {
            all.erase(all.begin() + static_cast<ptrdiff_t>(i));
            break;
        }
    }
}

void Player::updateAll() {
    for (Player* p : players()) p->update();
}

Player::Player(const GameFiles& files, const std::filesystem::path& folder, const std::string& name) : Player() {
    uint32_t channels = 0;
    std::unique_ptr<Bank> bank = Bank::load(files, folder / name, name, &channels);
    if (!bank) return;
    // banktype (0x005BE9B0): the bank added to the player and the player's slots made (CHANNELS + 1, set before).
    m_banks.push_back(bank.get());
    m_own.push_back(std::move(bank));
    initSlots(channels);
}

void Player::initSlots(uint32_t count) {
    m_slots.assign(count, Slot{});
    for (uint32_t i = 1; i < count; ++i) {
        m_slots[i].id = i;
        m_slots[i].previous = i;
        m_slots[i].voice = -1;
    }
    m_held = 0;
    m_maxId = 0xFFFFFFFF;
}

SoundSet* Player::setOf(const SetRef& ref) const {
    Bank* b = bank(ref.bank);
    return b != nullptr ? b->set(static_cast<uint32_t>(ref.set)) : nullptr;
}

Player::Slot* Player::slotOf(uint32_t handle, int& result) {
    if (m_slots.empty()) {
        result = kStale;
        return nullptr;
    }
    const uint32_t i = handle % static_cast<uint32_t>(m_slots.size());
    Slot& s = m_slots[i];
    if (handle != s.id) {
        result = kStale;
        return nullptr;
    }
    if (i == 0) {
        result = kSlotZero;
        return nullptr;
    }
    if (s.set == nullptr) {
        result = kStopped;
        return nullptr;
    }
    result = kOk;
    return &s;
}

const Player::Slot* Player::slotOf(uint32_t handle, int& result) const {
    return const_cast<Player*>(this)->slotOf(handle, result);
}

int Player::find(const char* name, SetRef& ref) const {
    for (size_t b = 0; b < m_banks.size(); ++b) {
        const int s = m_banks[b]->findSet(name);
        if (s != -1) {
            ref.bank = static_cast<int>(b);
            ref.set = s;
            return kOk;
        }
    }
    ref = {};
    return kNoSet;
}

void Player::renew(uint32_t& handle) {
    const uint32_t n = static_cast<uint32_t>(m_slots.size());
    Slot& s = m_slots[handle % n];
    s.previous = s.id;
    uint32_t id = s.id;
    if (m_maxId < n + id) {
        id %= n;
    } else {
        id += n;
    }
    s.id = id;
    handle = id;
    s.fades = 0;
    s.target = 0.0f;
    s.volume = 0.0f;
}

int Player::findSlot(uint32_t mask, uint32_t& id, int& how) {
    const uint32_t n = static_cast<uint32_t>(m_slots.size());
    for (uint32_t strategy = 0; strategy < 5; ++strategy) {
        if ((mask & (1u << strategy)) == 0) continue;
        uint32_t found = 0;
        switch (strategy) {
            case 0:  // 0x005BF3E0: the first slot not held whose state is below 3
                for (uint32_t i = 1; i < n && found == 0; ++i) {
                    if ((m_held & (1u << i)) == 0 && m_slots[i].state < 3) found = i;
                }
                break;
            case 1: {  // 0x005BF630: the slot whose set has the highest priority (the last of equals)
                uint32_t best = 0;
                for (uint32_t i = 1; i < n; ++i) {
                    if ((m_held & (1u << i)) == 0 && m_slots[i].set != nullptr &&
                        best <= static_cast<uint32_t>(m_slots[i].set->priority)) {
                        best = static_cast<uint32_t>(m_slots[i].set->priority);
                        found = i;
                    }
                }
                break;
            }
            case 2: {  // 0x005BF490: the quietest slot unless all are as quiet as their mean
                float lowest = 1.0f;
                float sum = 0.0f;
                for (uint32_t i = 1; i < n; ++i) {
                    if ((m_held & (1u << i)) == 0 && m_slots[i].volume < lowest) {
                        lowest = m_slots[i].volume;
                        found = i;
                    }
                    sum += m_slots[i].volume;
                }
                if (lowest == (1.0f / static_cast<float>(n)) * sum) found = 0;
                break;
            }
            case 3: {  // 0x005BF430: the slot started first
                uint32_t oldest = 0xFFFFFFFF;
                for (uint32_t i = 1; i < n; ++i) {
                    if ((m_held & (1u << i)) == 0 && m_slots[i].started < oldest) {
                        oldest = m_slots[i].started;
                        found = i;
                    }
                }
                break;
            }
            default:  // [0x0071E738]: the table holds four
                break;
        }
        if (found != 0) {
            id = m_slots[found].id;
            how = static_cast<int>(strategy);
            return kOk;
        }
    }
    id = 0;
    return kNoSlot;
}

int Player::findVoice(const SetRef& ref, uint32_t mask, uint32_t& voice, int& how) {
    SoundSet* set = setOf(ref);
    if (set == nullptr) return 1;
    for (uint32_t strategy = 0; strategy < 3; ++strategy) {
        if ((mask & (1u << strategy)) == 0) continue;
        int r = 4;
        switch (strategy) {
            case 0:  // 0x005BF340
                r = set->allBusy() ? 4 : set->findFree(voice);
                break;
            case 1:  // 0x005BF380
                r = set->stealQuietest(voice);
                break;
            case 2:  // 0x005BF3B0
                r = set->stealOldest(voice);
                break;
        }
        if (r != 4 && r != 5) {
            how = r;
            return 0;
        }
    }
    return 1;
}

int Player::prepare(const SetRef& ref, uint32_t slotMask, uint32_t voiceMask, int& how, uint32_t& handle) {
    handle = 0;
    if (ref.set == -1) return kSlotZero;
    uint32_t voice = 0;
    int voiceHow = 0;
    if (findVoice(ref, voiceMask, voice, voiceHow) != 0) return 1;
    how = voiceHow;
    SoundSet* set = setOf(ref);
    uint32_t id = 0;
    if (voiceHow == 0) {
        int slotHow = 0;
        const int r = findSlot(slotMask, id, slotHow);
        how = slotHow;
        if (r != kOk) {
            set->release(voice);
            return r;
        }
        if (slotHow != 0) stop(id);
    } else {
        // A voice taken from another slot: that slot stops and its place is taken.
        const uint32_t owner = set->owner(voice);
        stop(owner);
        id = m_slots[owner % m_slots.size()].id;
    }
    handle = id;
    const uint32_t index = id % static_cast<uint32_t>(m_slots.size());
    m_slots[index].unused = 0;
    renew(handle);
    m_slots[index].set = set;
    m_slots[index].voice = static_cast<int32_t>(voice);
    return kOk;
}

int Player::start(uint32_t handle) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    VoiceSet* v = s->set->voices();
    const uint32_t voice = static_cast<uint32_t>(s->voice);
    if (v == nullptr) return 1;
    v->setVolume(voice, s->volume * m_master);
    r = v->play(voice);
    if (r == 0) {
        s->set->setOwner(voice, s->id);
        s->set->acquire(voice);
        s->state = 3;
        const uint32_t now = clockSeconds();
        s->started = now;
        s->set->setStart(voice, now);
        return 0;
    }
    s->state = 1;
    s->set->setOwner(voice, 0);
    s->set->release(voice);
    return r;
}

int Player::restart(uint32_t handle) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    s->started = clockSeconds();
    s->set->setOwner(static_cast<uint32_t>(s->voice), s->id);
    if (VoiceSet* v = s->set->voices()) v->replay(static_cast<uint32_t>(s->voice));
    s->set->acquire(static_cast<uint32_t>(s->voice));
    s->state = 3;
    return 0;
}

void Player::freeSlot(Slot& s, uint32_t index) {
    const uint32_t voice = static_cast<uint32_t>(s.voice);
    if (VoiceSet* v = s.set->voices()) v->stop(voice);
    s.set->release(voice);
    s.set->clearPlaying(voice);
    s.state = 1;
    s.set->setOwner(voice, 0);
    s.set = nullptr;
    (void)index;
}

int Player::stop(uint32_t handle) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    unhold(handle);
    freeSlot(*s, handle % static_cast<uint32_t>(m_slots.size()));
    return 0;
}

int Player::pause(uint32_t handle) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    if (VoiceSet* v = s->set->voices()) v->pause(static_cast<uint32_t>(s->voice));
    s->state = 2;
    return 0;
}

int Player::resume(uint32_t handle) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    if (VoiceSet* v = s->set->voices()) v->resume(static_cast<uint32_t>(s->voice));
    s->state = 3;
    return 0;
}

int Player::state(uint32_t handle, int& state) {
    state = 0;
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    const int before = s->state;
    if (VoiceSet* v = s->set->voices()) state = v->status(static_cast<uint32_t>(s->voice));
    if (before == 5 && (state == 3 || state == 4)) state = 5;
    return 0;
}

int Player::hold(uint32_t handle) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    m_held |= 1u << (handle % m_slots.size());
    s->set->markPlaying(static_cast<uint32_t>(s->voice));
    return 0;
}

int Player::unhold(uint32_t handle) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    m_held &= ~(1u << (handle % m_slots.size()));
    if (s->state != 5) s->set->clearPlaying(static_cast<uint32_t>(s->voice));
    return 0;
}

void Player::unholdAll() {
    for (uint32_t i = 1; i < m_slots.size(); ++i) {
        Slot& s = m_slots[i];
        if (s.state == 5) continue;
        m_held &= ~(1u << i);
        if (s.set != nullptr) s.set->clearPlaying(static_cast<uint32_t>(s.voice));
    }
}

int Player::setVolume(uint32_t handle, float volume) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    s->set->setVolume(static_cast<uint32_t>(s->voice), volume);
    if (VoiceSet* v = s->set->voices()) v->setVolume(static_cast<uint32_t>(s->voice), volume * m_master);
    s->volume = volume;
    s->target = volume;
    return 0;
}

int Player::valid(uint32_t handle) const {
    int r;
    slotOf(handle, r);
    return r;
}

int Player::setFrequency(uint32_t handle, uint32_t hz) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    if (VoiceSet* v = s->set->voices()) v->setFrequency(static_cast<uint32_t>(s->voice), hz);
    return 0;
}

int Player::frequency(uint32_t handle, uint32_t& hz) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    if (VoiceSet* v = s->set->voices()) hz = v->frequency(static_cast<uint32_t>(s->voice));
    return 0;
}

int Player::setPosition(uint32_t handle, uint32_t bytes) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    if (VoiceSet* v = s->set->voices()) v->setPosition(static_cast<uint32_t>(s->voice), bytes);
    return 0;
}

int Player::setPlayFlags(uint32_t handle, uint32_t flags) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    if (VoiceSet* v = s->set->voices()) v->setPlayFlags(static_cast<uint32_t>(s->voice), flags);
    return 0;
}

int Player::setPan(uint32_t handle, float pan) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    if (VoiceSet* v = s->set->voices()) v->setPan(static_cast<uint32_t>(s->voice), pan);
    return 0;
}

int Player::setUser(uint32_t handle, uint32_t value) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    s->user = value;
    return 0;
}

int Player::user(uint32_t handle, uint32_t& value) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    value = s->user;
    return 0;
}

void Player::assign(uint32_t handle, SoundSet* set, uint32_t voice) {
    Slot& s = m_slots[handle % m_slots.size()];
    s.set = set;
    s.voice = static_cast<int32_t>(voice);
}

void Player::place3d(uint32_t handle, const Emitter& emitter, float doppler) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr || s->set->voices() == nullptr) return;
    const EmitterState& st = *emitter.state;
    const float rel[3] = {st.relative.x, st.relative.y, st.relative.z};
    const float change[3] = {st.velocity.x, st.velocity.y, st.velocity.z};
    s->set->voices()->place(static_cast<uint32_t>(s->voice), rel, change, st.distance, doppler);
}

int Player::setOcclusion(uint32_t handle, float v) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    if (VoiceSet* voices = s->set->voices()) voices->setOcclusion(static_cast<uint32_t>(s->voice), v);
    return kOk;
}

int Player::debugSlots(const DebugLine& line, uint32_t fontHeight) const {
    if (!line) return 0;
    float x = 25.0f;
    float y = 90.0f;
    const uint32_t colour = 0xFFFF0000;
    char text[260];
    for (uint32_t i = 1; i < m_slots.size(); ++i) {
        const Slot& s = m_slots[i];
        const uint32_t index = s.id % static_cast<uint32_t>(m_slots.size());
        if (s.set == nullptr) {
            std::snprintf(text, sizeof text, "%02d", index);
        } else {
            std::snprintf(text, sizeof text, "%02d : %1d : %12s %0.3f : %0.3f", index, s.state,
                          s.set->name().c_str(), static_cast<double>(s.target),
                          static_cast<double>(s.volume) * static_cast<double>(m_master));
        }
        line(text, x, y, colour);
        y = static_cast<float>(static_cast<double>(fontHeight * 2 / 3) + static_cast<double>(y));
        if (i % 24 == 0) {
            x = 380.0f;
            y = 90.0f;
        }
    }
    return 0;
}

void Player::setMasterVolume(float volume) {
    volume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
    m_master = volume;
    for (uint32_t i = 1; i < m_slots.size(); ++i) {
        Slot& s = m_slots[i];
        if (s.set == nullptr) continue;
        if (VoiceSet* v = s.set->voices()) v->setVolume(static_cast<uint32_t>(s.voice), volume * s.volume);
    }
}

void Player::pauseAll() {
    for (uint32_t i = 1; i < m_slots.size(); ++i) {
        Slot& s = m_slots[i];
        if (s.set == nullptr) continue;
        if (VoiceSet* v = s.set->voices()) v->pause(static_cast<uint32_t>(s.voice));
        s.state = 2;
    }
}

void Player::resumeAll() {
    for (uint32_t i = 1; i < m_slots.size(); ++i) {
        Slot& s = m_slots[i];
        if (s.set == nullptr || s.set->voices() == nullptr) continue;
        VoiceSet* v = s.set->voices();
        if (v->status(static_cast<uint32_t>(s.voice)) != 3) {
            v->resume(static_cast<uint32_t>(s.voice));
            s.state = 3;
        }
    }
}

int Player::fadeOut(uint32_t handle) {
    int r;
    Slot* s = slotOf(handle, r);
    if (s == nullptr) return r;
    setVolume(handle, 0.0f);
    s->state = 5;
    m_held |= 1u << (handle % m_slots.size());
    return 0;
}

void Player::update() {
    constexpr float kMin = 0.0001f;  // [0x0071E74C]
    for (uint32_t i = 1; i < m_slots.size(); ++i) {
        Slot& s = m_slots[i];
        const int before = s.state;
        if (s.set == nullptr) {
            s.state = 0;
            continue;
        }
        VoiceSet* v = s.set->voices();
        const uint32_t voice = static_cast<uint32_t>(s.voice);
        s.state = v != nullptr ? v->status(voice) : 0;
        const float was = s.volume;
        if (s.target == s.volume) {
            s.volume = s.target;
        } else {
            const float d = s.target - s.volume;
            if (0.0f <= d) {
                s.volume = d * 1.0f + s.volume;  // [0x0071E744]
                if (s.target < s.volume) s.volume = s.target;
            } else {
                s.volume = d * 0.3f + s.volume;  // [0x0071E748]
                if (s.volume <= kMin) s.volume = 0.0f;
            }
        }
        if (was != s.volume && v != nullptr) v->setVolume(voice, m_master * s.volume);
        if (before == 5) {
            if (s.volume <= kMin && was != s.volume) {
                hold(s.id);
                ++s.fades;
                s.state = 5;
                continue;
            }
        } else if (before == 2 && (s.state == 0 || s.state == 1)) {
            s.state = 2;
        }
        if (s.state == 0 || s.state == 1) {
            if (v != nullptr) v->stop(voice);
            s.set->release(voice);
            s.set->clearPlaying(voice);
            s.set->setOwner(voice, 0);
            m_held &= ~(1u << i);
            s.user = 0;
            s.set = nullptr;
            s.voice = -1;
        } else if (before == 5) {
            if (s.volume > kMin) {
                s.state = 5;
                hold(s.id);
            } else {
                if (v != nullptr) v->stop(voice);
                s.set->release(voice);
                s.set->clearPlaying(voice);
                s.set->setOwner(voice, 0);
                s.state = 1;
                m_held &= ~(1u << i);
                s.set = nullptr;
                s.user = 0;
                s.voice = -1;
            }
        }
    }
}

}  // namespace oj::audio
