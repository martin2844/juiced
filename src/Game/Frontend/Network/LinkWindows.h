#pragma once
// The network game's LAN windows (docs/spec/28-network.md, "The windows").

#include "Core/Network/NetTransport.h"
#include "Core/Race/Racer.h"
#include "Game/Cars/CarSound.h"
#include "Game/Cars/CarView.h"
#include "Game/Frontend/CustomRace/CustomRaceHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

namespace oj::frontend {

// The window of the record of link_search (0x0072170C).
class LinkSearch {
public:
    // 0x0046E800
    void enter(NetworkHost& host, ui::Screen& s);
    // 0x0046E880
    void update(NetworkHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x0046E3E0: GAMELIST again: "NEW GAME" (its address none), then each session found, "<name> (<players>/<most>)"
    // with its host's address; the first item highlighted, or the session highlighted before when it is still there (by
    // its address).
    void fillList(NetworkHost& host, ui::Screen& s);

    float m_timer = 0.0f;      // +0x0C: the list's, again each second
    bool m_joining = false;    // +0x10
    std::vector<net::Address> m_listed;  // the hosts' addresses of GAMELIST's sessions (its items' +0x24)
};

// The window of the record of link_lobby (0x00721724): +0x0C (read by nothing), the online pink slip's
// end shown (+0x14) and its autosave asked (+0x10: the session left at the next frame).
class LinkLobby {
public:
    // 0x0046EBB0
    void enter(WindowHost& host, ui::Screen& s);
    // 0x0046FB20
    void update(NetworkHost& host, ui::Screen& s, uint32_t event);

private:
    // 0x0046EED0: each slot's name multiline and "-", its icons (playericon, CPU, voice) hidden, its points, races and
    // rating "-".
    void clearSlots(ui::Screen& s);
    // 0x0046F250, in the lobby only (the stage 0): the players by their points, most first (a bubble sort starting
    // again after each swap); the slots cleared (0x0046EED0); each slot: playericon shown for the host's machine's
    // (ours when we host) whose index on its machine is 0 (Stock problem: the index read is the unsorted player's of
    // the slot's place), CPU for a second player of a machine; with the voice ([0x0075F044]) the user of the slot's
    // place's player (unsorted as playericon's) referenced (vt+0x10; Stock problem: never let go, one reference more a
    // frame) and with one, voice<n>: ours, with the user object's first local player's devices opened, shown with the
    // icon on while it sends (vt+0x20) else off; another's the user's icon (mute, on, off, or hidden); the name, the
    // points and the races; with the statistics, the player's user's record asked for (0x0046F9CA): rating<n> its rank
    // ("-" for 0).
    void showSlots(ui::Screen& s);

    int32_t m_unused = 0;    // +0x0C
    int32_t m_leaving = 0;   // +0x10
    int32_t m_settled = 0;   // +0x14
};

// multi (the record 0x00721680): split screen or the network; no state of its own.
class Multi {
public:
    // 0x0046CAA0
    void enter(CustomRaceHost& host, ui::Screen& s);
    // 0x0046CB10
    void update(CustomRaceHost& host, ui::Screen& s, uint32_t event);
};

// The window of the record of link_selcar (0x00768090, vtable 0x006F2BEC): each slot's car view (+0x0C),
// car (+0x24), config seen (+0x48C, 0x74 bytes) and list of cars shown (+0x7D4, 0x00489B80, its
// current car).
class LinkSelCar {
public:
    // 0x0046FF10
    void enter(CustomRaceHost& host, ui::Screen& s);
    // 0x00471160
    void update(CustomRaceHost& host, ui::Screen& s, uint32_t event);
    // 0x004709B0
    void leave(WindowHost& host);

private:
    void freeViews();

    struct Slot {
        std::unique_ptr<scene::CarView> view;
        Car car;
        std::array<uint8_t, 0x74> seen{};
        std::deque<Car> list;
        uint32_t current = 0;
    };
    std::array<Slot, 6> m_slots;
};

}  // namespace oj::frontend
