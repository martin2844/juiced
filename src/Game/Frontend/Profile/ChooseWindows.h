#pragma once
// The windows that choose a new driver's crew logo (cmschooselogo) and phone (cmsmobilechoose).

#include "Game/Frontend/Profile/ProfileHost.h"
#include "Game/UI/UiScreen.h"

#include <cstdint>

namespace oj::frontend {

class ChooseLogo {
public:
    // 0x004C3B70
    void enter(ProfileHost& host, ui::Screen& s);
    // 0x004C3BD0
    void update(ProfileHost& host, ui::Screen& s, uint32_t event);
};

class ChoosePhone {
public:
    // 0x004C3DD0
    void update(ProfileHost& host, ui::Screen& s, uint32_t event);

private:
    int m_phone = 0;  // the phone chosen for a new career
};

}  // namespace oj::frontend
