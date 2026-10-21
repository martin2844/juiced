#pragma once
// The stock's globals that more than one window reads or writes. The front end keeps one (static, as the stock's
// globals outlive its front ends) and hands it to the windows through WindowHost::shared.

#include "Game/Frontend/Network/OnlineForms.h"

#include <cstdint>
#include <string>

namespace oj::frontend {

struct SharedState {
    std::string tutorialReturn;          // [0x0072158C]: the window vid_tutorial goes back to
    int32_t autosaveAsked = 0;           // [0x0075ED38]: the driver asked for the autosave (read by autosave)
    OnlineCommandLine onlineCommandLine;  // the command line's online switches the main menu's enter takes
    OnlineForms onlineForms;             // the online windows' globals
};

}  // namespace oj::frontend
