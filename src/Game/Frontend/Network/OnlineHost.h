#pragma once
// What the GameSpy windows ask of the front end: the network game's (NetworkHost) and the name window's setup they
// fill for an account's field (ProfileHost's keyboard).

#include "Game/Frontend/Network/NetworkHost.h"
#include "Game/Frontend/Profile/ProfileHost.h"

namespace oj::frontend {

class OnlineVoice;

class OnlineHost : public virtual NetworkHost, public virtual ProfileHost {
public:
    // online_voice's window, whose purpose the windows opening it set.
    virtual OnlineVoice& onlineVoice() = 0;
};

}  // namespace oj::frontend
