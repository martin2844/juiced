// The online career's grid walk (docs/spec/28-network.md, "The online career"): CMSOnlineGridWalk, the session's
// players and their cars after the race setup, the readiness and a pink slip's stakes.

#include "Game/Frontend/Network/OnlineCareerWindows.h"

#include "Core/Base/Random.h"
#include "Core/Base/Text.h"
#include "Core/Career/Career.h"
#include "Core/Cars/CarCollection.h"
#include "Core/Cars/CarPhysics.h"
#include "Core/Data/GameTexts.h"
#include "Core/Network/Matchmaking.h"
#include "Core/Network/NetSession.h"
#include "Core/Race/RaceSession.h"
#include "Game/Frontend/WindowTools.h"

#include <cstring>
#include <format>

namespace oj::frontend {

namespace {

constexpr size_t kGarageMost = 55;            // the garage list's 0xDC bytes

std::wstring wide(const std::u16string& text) {
    return std::wstring(text.begin(), text.end());
}

// The paint of a player's config (its +0x2C .. +0x6C) into a car record, each frame (0x004D38F0).
void paint(Car& car, const net::CarConfig& c) {
    car.neon = c.neon;
    car.base = c.base;
    car.metallic = c.metallic;
    car.pearl = c.pearl;
    car.decalColour = c.decalColour;
    car.sponsorColour = c.sponsorColour;
    car.sponsorDecals = c.sponsorDecals;
    car.metallicAmount = c.metallicAmount;
    car.pearlAmount = c.pearlAmount;
    car.dialColour = c.dialColour;
    car.dialTextColour = c.dialTextColour;
    car.wheelColour = c.wheelColour;
    car.wheelsColoured = c.wheelsColoured;
    car.tint = c.tint;
    car.lowering = c.lowering;
    car.gearing = c.gearing;
    car.torqueFactor = c.torqueFactor;
}

}  // namespace

// 0x004D3310. The windows' enter; [0x0075EE6C] 1 (the grid walk entered); the views' pointers 0 and a new list of
// cars; our readiness 0 sent (0x004EA370, the local index 0); +0x748 and +0x754 0. Each slot: its car view on
// Car3d<n> (0x0045A2C0's fields: no name, logo or origin element, the zoom 1, no mirror, the shadow; +0x4C 0.6) with
// the camera at (0, 150, -560), the PS2_ elements hidden, the config seen emptied; <n>Info over several lines, the
// player's name or emptied; bph<n> hidden; ready<n> and notready<n> hidden past the players. +0x758 and +0x75C -1,
// +0x760 0; COUNTDOWNBOX hidden. (An element missing is written at 0 + its offset; here nothing.)
void OnlineGridWalk::enter(NetworkHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    host.shared().onlineForms.gridWalked = 1;
    for (Slot& slot : m_slots) slot.view.reset();
    net::NetGame* net = net::netGame();
    if (net != nullptr) net->session.sendReady(0, 0);
    m_ready = 0;
    m_staked = 0;
    const size_t players = net != nullptr ? net->session.players().size() : 0;
    for (size_t i = 0; i < m_slots.size(); ++i) {
        Slot& slot = m_slots[i];
        const size_t n = i + 1;
        const std::string element = std::format("Car3d{}", n);
        if (host.engine() != nullptr && data != nullptr && s.find(element) >= 0) {
            scene::CarView::Setup setup;
            setup.element = element;
            setup.zoom = 1.0f;
            setup.shadow = true;
            slot.view = std::make_unique<scene::CarView>(*host.engine(), s, setup, data->catalogue, data->tables);
            slot.view->setCameraPlace({0.0f, 150.0f, -560.0f, 0.0f});
        }
        s.hideContaining("PS2_");
        slot.seen.fill(0);
        const std::string info = std::format("{}Info", n);
        if (const int e = s.find(info); e >= 0) {
            s.setMultiline(info, true);
            if (i < players) {
                s.setText(e, wide(net->session.players()[i].name));
            } else {
                s.clearText(e);
            }
        }
        s.setHidden(std::format("bph{}", n), true);
        s.setHidden(std::format("ready{}", n), players <= i);
        s.setHidden(std::format("notready{}", n), players <= i);
    }
    m_unused = -1;
    m_answer = -1;
    m_left = 0;
    for (int e = 0; e < s.size(); ++e) {
        if (_stricmp(s.nameOf(e).c_str(), "COUNTDOWNBOX") == 0) s.setHidden(e, true);
    }
}

// 0x004D38F0, each frame. ready<n> and notready<n> hidden past the players, else shown; Car3d<n> shown for a player,
// else it and bph<n> hidden. Each slot of a player: the paint of its config into the slot's car and into the config
// seen (+0x40, the decal colour, left out of the seen one), the car shown painted again (0x0045A6F0's lines); a config
// unlike the one seen: <n>Info the player's name, and with the view loading nothing and a name the slot's car made
// anew (0x0048E7D0), with a pink slip ours (the player -1) or the rival's; the car put in the list, shown turned
// 3 pi / 4 and turning 0.4 a second (0x0045A080), rated (0x0048E8A0); bph<n> shown, its text "<rating> bhp" and then
// the car's display name, " (P)" after a prototype (0x004A8F50), over several lines; the car taken out of the list and
// the config seen the player's. A slot without a player: <n>Info emptied.
void OnlineGridWalk::slots(NetworkHost& host, ui::Screen& s) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    net::NetGame* net = net::netGame();
    if (net == nullptr || data == nullptr) return;
    GameData& d = *data;
    const std::vector<net::NetPlayer>& players = net->session.players();
    for (size_t i = 0; i < m_slots.size(); ++i) {
        const size_t n = i + 1;
        s.setHidden(std::format("ready{}", n), players.size() <= i);
        s.setHidden(std::format("notready{}", n), players.size() <= i);
        if (i < players.size()) {
            s.setHidden(std::format("Car3d{}", n), false);
        } else {
            s.setHidden(std::format("bph{}", n), true);
            s.setHidden(std::format("Car3d{}", n), true);
        }
    }
    const bool pinkSlip = net::onlineHosted().career.pinkslip != 0;
    for (size_t i = 0; i < m_slots.size(); ++i) {
        Slot& slot = m_slots[i];
        const size_t n = i + 1;
        const int info = s.find(std::format("{}Info", n));
        if (players.size() <= i) {
            if (info >= 0) s.clearText(info);
            continue;
        }
        const net::NetPlayer& p = players[i];
        const net::CarConfig& config = p.car;
        std::array<uint8_t, 0x74> bytes{};
        config.write(bytes.data());
        paint(slot.car, config);
        std::memcpy(slot.seen.data() + 0x2C, bytes.data() + 0x2C, 0x40 - 0x2C);
        std::memcpy(slot.seen.data() + 0x44, bytes.data() + 0x44, 0x70 - 0x44);
        if (slot.view && slot.view->hasModel()) slot.view->repaint(slot.car);
        if (slot.seen == bytes) continue;
        if (info >= 0) s.setText(info, wide(p.name));
        if (!slot.view || slot.view->busy() || config.name[0] == 0) continue;
        slot.car = makeCar(findCarDef(d.defs, std::string(config.name.data())));
        config.apply(slot.car);
        if (pinkSlip) {
            if (p.peer == -1) {
                m_own = static_cast<int32_t>(i);
            } else {
                m_rival = static_cast<int32_t>(i);
            }
        }
        slot.view->show(slot.car, &m_current, 0, std::bit_cast<float>(0x4016CBE4u), std::bit_cast<float>(0x3ECCCCCDu),
                        true, host.viewLoading());
        if (slot.car.def != nullptr) {
            rateCar(slot.car, d.dynamics ? d.dynamics->of(*slot.car.def) : nullptr, d.catalogue);
        }
        const std::string bph = std::format("bph{}", n);
        s.setHidden(bph, false);
        if (const int e = s.find(bph); e >= 0) {
            s.setText(e, std::format(L"{:.0f} ", std::bit_cast<float>(slot.car.rating)) + texts.at(kTextBhp));
            std::wstring name = slot.car.def != nullptr ? widenSigned(slot.car.def->displayName) : std::wstring();
            if (heavilyTuned(slot.car, d.catalogue)) name += L" (P)";
            s.setText(e, name);
            s.setMultiline(bph, true);
        }
        slot.seen = bytes;
    }
}

// 0x004D40B0. The session left (+0x760): "Host has left the game" (type 1) and online_main. With a pink slip and
// not two players our readiness taken back (0x004EA370 for the local indices 0 .. 2); the question answered yes
// (+0x75C): our readiness turned. BACK past the cars' stage: nothing more. The shared checks (0x0046E0F0 with the
// stage 8, not loading; what they return not looked at); the views' frames; the slots (0x004D38F0). SELECT: not ready
// with a pink slip, a garage of 55 cars "Cannot enter a pinkslip race. .." (type 1), two players the question
// "WARNING! This is a pink slip event. .." and "Accidental power loss .." (type 3, starting on no), else "You must
// have 2 players for a pink slip race" (type 1), and nothing more; else the readiness turned. BACK: not ready. Either
// sent. ready<n> / notready<n> by each player's readiness. Past the cars' stage: without a pink slip "loading"
// (mode 2); with one, the stakes set (+0x754) first: a copy of our car (0x00434D90; its grid made when none,
// 0x005562A0) at the garage's end, rated (0x0048E8A0), the profile's +0x600 the new car, "loading" (mode 2) and
// nothing more (Stock problem: a request already waiting makes the copy again each frame); else the stakes
// (0x004A49A0 with our car and the rival's), the profile's +0x600 car taken out of the garage (0x00489CD0), with the
// saving on autosave (mode 5) and the network's delayed call (50, 0x004D4090), +0x754 1. Then COUNTDOWN (0x00471740).
void OnlineGridWalk::update(NetworkHost& host, ui::Screen& s, uint32_t event) {
    GameData* data = host.gameData();
    const TextTable& texts = host.gameTexts();
    if (m_left != 0) {
        host.message(MessageType::Continue, texts.at(kTextHostLeft), false, nullptr);
        host.request("online_main", SwitchMode::Replace);
        return;
    }
    net::NetGame* net = net::netGame();
    if (net == nullptr || data == nullptr) return;
    GameData& d = *data;
    net::NetSession& session = net->session;
    const bool pinkSlip = net::onlineHosted().career.pinkslip != 0;
    const auto sendReady = [&] {
        for (int32_t j = 0; j < 3; ++j) session.sendReady(m_ready, j);
    };
    if (pinkSlip && session.players().size() != 2 && m_ready != 0) {
        m_ready = 1 - m_ready;
        sendReady();
    }
    if (m_answer == 1) {
        m_answer = -1;
        m_ready = 1 - m_ready;
        sendReady();
    }
    if (event == input::kBack && session.stage() > net::SessionStage::Cars) return;
    host.netKeys(event, net::SessionStage::Cars, false);
    for (Slot& slot : m_slots) {
        if (slot.view) slot.view->frame(host.frameTime(), gameRandom());
    }
    slots(host, s);
    if (event == input::kSelect) {
        if (m_ready == 0 && pinkSlip) {
            if (d.career && d.career->garage.size() == kGarageMost) {
                host.message(MessageType::Continue, texts.at(kTextGarageFullForPinkSlip), false, nullptr);
            } else if (session.players().size() == 2) {
                host.message(MessageType::YesNo, texts.at(kTextPinkWarning) + L"\n" + texts.at(kTextPinkLoss), false,
                             &m_answer, true);
            } else {
                host.message(MessageType::Continue, texts.at(kTextPinkPlayers), false, nullptr);
            }
            return;
        }
        m_ready = 1 - m_ready;
        sendReady();
    } else if (event == input::kBack) {
        m_ready = 0;
        sendReady();
    }
    const std::vector<net::NetPlayer>& players = session.players();
    for (size_t i = 0; i < players.size(); ++i) {
        s.setHidden(std::format("ready{}", i + 1), players[i].race.ready == 0);
        s.setHidden(std::format("notready{}", i + 1), players[i].race.ready != 0);
    }
    if (!pinkSlip) {
        if (session.stage() > net::SessionStage::Cars) host.request("loading", SwitchMode::ReplaceNow);
    } else if (session.stage() > net::SessionStage::Cars && d.career) {
        Career& career = *d.career;
        // Stock problem: our car and the rival's are the slots' cars of the last ones made, none (0) before any; the
        // stock code copies from 0 then. Here an empty car stands for none.
        const Car none{};
        const Car& own = m_own >= 0 ? m_slots[static_cast<size_t>(m_own)].car : none;
        const Car& rival = m_rival >= 0 ? m_slots[static_cast<size_t>(m_rival)].car : none;
        if (m_staked != 0) {
            Car car = copyCar(own);
            if (!car.grid) car.grid.emplace();
            if (car.def != nullptr) rateCar(car, d.dynamics ? d.dynamics->of(*car.def) : nullptr, d.catalogue);
            career.garage.push_back(std::move(car));
            career.profile.raceCars[0] = static_cast<int32_t>(career.garage.size()) - 1;
            host.request("loading", SwitchMode::ReplaceNow);
            return;
        }
        onlineStakes(career, own, rival);
        const auto index = static_cast<size_t>(career.profile.raceCars[0]);
        if (index < career.garage.size()) {
            career.garage.erase(career.garage.begin() + static_cast<std::ptrdiff_t>(index));
        }
        if (d.saving != 0) {
            host.request("autosave", SwitchMode::Push);
            host.delayGridWalkTimeout();
        }
        m_staked = 1;
    }
    host.netCountdown();
}

// 0x004D3850: the windows' leave (the Menu's item kept); the race's automatic gearbox (+0x54) the settings'; the
// views and the list freed.
void OnlineGridWalk::leave(WindowHost& host) {
    GameData* data = host.gameData();
    if (data != nullptr) data->race.gearbox = static_cast<int32_t>(host.settingsValue(save::Settings::kAutomatic));
    for (Slot& slot : m_slots) {
        if (slot.view) slot.view->clear();
        slot.view.reset();
    }
}

// 0x004D4090, the network's delayed call of the pink slip's autosave: +0x760 1 (the window's next frame goes to
// online_main) and everything reset (0x004EA4E0 with 1).
void OnlineGridWalk::timeout() {
    m_left = 1;
    if (net::NetGame* net = net::netGame()) net->session.reset(true);
}

}  // namespace oj::frontend
