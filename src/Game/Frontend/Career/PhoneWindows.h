#pragma once
// The career's phone: its calls, menu, contacts and choice.

#include "Core/Career/Career.h"
#include "Game/Frontend/Career/CareerHost.h"
#include "Game/UI/UiScreen.h"

#include <array>
#include <cstdint>
#include <string>

namespace oj::frontend {

// The phone's windows (CMSMobilePhone, CMSMobilePhoneMenu, CMSMobilePhoneContacts,
// CMSMobileChoose) and the phone's frame in the career's windows, with the globals they share.
class MobilePhone {
public:
    // 0x004B60F0
    void frame(CareerHost& host, ui::Screen& screen, const ui::Screen*& shown);
    // 0x004D17A0
    void enter(CareerHost& host, ui::Screen& s);
    // 0x004D1C10
    void update(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004D1AD0
    void leave(CareerHost& host);
    // 0x004D2460
    void enterMenu(CareerHost& host, ui::Screen& s);
    // 0x004D27C0
    void updateMenu(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004D2CB0
    void enterContacts(CareerHost& host, ui::Screen& s);
    // 0x004D2D90
    void updateContacts(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004C3DB0
    void enterChoose();
    // 0x004C3DD0
    void updateChoose(CareerHost& host, ui::Screen& s, uint32_t event);

private:
    std::wstring lineText(CareerHost& host, int32_t text);
    // 0x004B65E0: the call's sample played once (below 0x21, not -1, with a speech object: its line stopped,
    // 0x0040B600, then its +0x14 with the sample's crew and the sample); its text, Mr.
    std::wstring callText(CareerHost& host, PhoneMessage& call);
    // 0x004D23B0: PIC_PICTURE by the caller: a crew's leader (kind 0, crew 0 .. 8) its leader's picture, a crewmate (1)
    // ui_crewmember_<nn>, Mr.
    void showCaller(CareerHost& host, ui::Screen& s, PhoneCaller kind, int32_t crew);
    // The phone model's picture on Mobile: 1 ui_moto_02, 2 ui_sharp_01, 3 ui_sharp_02, 4 ui_sony_02, 5 ui_sony, else
    // ui_moto.
    void showModel(CareerHost& host, ui::Screen& s);
    // 0x004D1F10: RingIn each 2 s (its handle kept, [0x0075EF74]); caller "NEW MESSAGE" (336) for a text message (type
    // 10), else the caller's name.
    void ringing(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004D2140: caller "RINGING" (1638) over 0.2 of each second, else empty; RingOut each 2 s.
    void calling(CareerHost& host, ui::Screen& s, uint32_t event);
    // 0x004B6B20 by the call's type, with the answer (the item's place): 0 (a crewmate asks to join): +0xFA0 0; yes: it
    // joins (0x004B80D0), sample 0x25, line 316; no: sample 0x26, line 317. 1 (the player asks a crewmate): yes: it
    // joins, sample 0x22, line 320; no: the phone idles. 2 (host challenge): yes: the next free day (0x004A6A10, +0x14
    // and +0x10), the host rights race (0x004A2DC0), sample 2, line 322, the event gone to (+0x0C); no: the crew's
    // respect to at most its last step less 100 (and not below 0) noted for its message (+0x18 .. +0x24), sample 3,
    // line 323. 5 (pink slip race): yes: the next free day, the pink slip race (0x004A2F80), sample 6, line 325, gone
    // to; no: the crew's respect less 25 noted, sample 5, line 326. 8 (the player calls a crew's leader; with the
    // advice to race for pinks the first answer is the second, with the advice to take a challenge the second the
    // third; without cars the second idles the phone): first: the challenge texts (the advised challenge's kind, else
    // each of the three with " (c)" when done) as the answers with END CALL, sample 0x15, line 332, the crew shown
    // (+0xF9C); second: a crew past its second step with fewer than 3 pink slip races and fewer than 55 cars: the next
    // free day, the pink slip race, sample 8, line 328, gone to; else sample 7, line 329; third: the phone idles. 9
    // (the challenges offered): END CALL (the second with the advice, else the fourth) idles; else the next free day,
    // +0xF9C 0, and with a garage car of the challenge's model (0x004A8C80) its event (0x004A3130), sample 9, line 333,
    // gone to; else sample 10, line 334. 10 .. 15 idle the phone; 16 .. 19 set the crew's respect to a step less 20
    // (100, 300, 1000, 600; stock problem: 18 goes on into 19), its gains (0x004BBD80) and idle the phone.
    void answerCall(CareerHost& host, int32_t answer);
    void stopRing(CareerHost& host);

    // The record of CMSMobileChoose (0x00721DC4): +0x0C the model highlighted, +0x10 the answer to buying.
    struct Choose {
        int32_t item = -1;
        int32_t answer = -1;
    };
    // The phone's globals: the challenges' texts of a call (0x00769A28 ..), the ring's time ([0x0075EF78]), the
    // wait after a new window ([0x0079E820], [0x0079E824]), RINGING's blink ([0x0079E860]) and the model shown
    // ([0x00725618]).
    std::array<std::wstring, 3> m_challenges;
    float m_ring = 0.0f;
    uint32_t m_sound = 0;  // [0x0075EF74]: the handle of the ring or the dial
    float m_wait = 0.0f;
    float m_blink = 0.0f;
    int32_t m_shownModel = 0;
    Choose m_choose;
};

}  // namespace oj::frontend
