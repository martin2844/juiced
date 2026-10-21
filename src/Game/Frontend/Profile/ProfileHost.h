#pragma once
// What the windows of a new driver (keyboard, cmschooselogo, cmsmobilechoose) ask of the front end besides what every
// window does.

#include "Core/Saves/Profile.h"
#include "Game/Frontend/WindowHost.h"

#include <optional>
#include <string>

namespace oj::frontend {

struct KeyboardSetup;

class ProfileHost : public virtual WindowHost {
public:
    // The name filter (scripts\profanities.txt and nonprofanities.txt) refusing a name.
    virtual bool nameRefused(const std::wstring& name) const = 0;
    // The new driver the keyboard made (none before).
    virtual std::optional<save::Profile>& profile() = 0;
    // The name window of a new driver (NEW DRIVER NAME and the name to start from) set up before keyboard: DONE makes
    // the driver (0x004675E0) and goes on to the crew logo, BACK returns to the driver list.
    virtual void setUpDriverName(const std::wstring& text) = 0;
    // What the window that opens keyboard sets up (its state object, 0x00720C88).
    virtual KeyboardSetup& keyboardSetup() = 0;
};

}  // namespace oj::frontend
