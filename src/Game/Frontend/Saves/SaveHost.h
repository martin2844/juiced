#pragma once
// What the save device's windows (driver, saveprofile, autosave, loadhighscores, loadreplay, bootupcheck) ask of the
// front end besides what every window does.

#include "Core/Saves/SaveDevice.h"
#include "Game/Frontend/Profile/ProfileHost.h"

#include <optional>
#include <string>

namespace oj::frontend {

class SaveHost : public virtual ProfileHost {
public:
    // The front end's +0x110: busy with the device (no network message window while set, 0x005A7F90).
    virtual void setSaveBusy(int32_t busy) = 0;
    // scripts\jettext<language>.txt, read by the driver window's first enter.
    virtual TextTable& jetTexts() = 0;
    // The game's side of the device (what a driver's and the records' files hold).
    virtual save::SaveGameSide& saveSide() = 0;
    // A driver loaded or saved: the music at the settings' music volume, the effects' and the speech's volumes.
    virtual void settingsVolumes() = 0;
    // The network's delayed call ([0x0075ED3C] .. [0x0075ED44]) autosave runs: made when its time is out, else its time
    // run down by the frame time; and cancelled by autosave's leave.
    virtual void netCallFrame() = 0;
    virtual void cancelNetCall() = 0;
};

}  // namespace oj::frontend
