#pragma once
// What every window of the front end asks of it (docs/spec/02-frontend.md): the switch between windows, the message
// window, the interface's sounds, the game's data and texts, the frame's time and the driver's settings. A window keeps
// its own state (the stock's record of it) and is handed the front end through this at each call; a family of windows
// adds what only it asks for in a host of its own (OptionsHost, CareerHost, GarageHost, ...).

#include "Core/Base/Random.h"
#include "Core/Data/GameData.h"
#include "Core/Data/Language.h"
#include "Core/Saves/SaveEntries.h"
#include "Game/Frontend/SharedState.h"
#include "Game/UI/MessageWindow.h"
#include "Game/UI/UiScreen.h"
#include "Game/UI/UiSound.h"
#include "Render/RenderDevice.h"

#include <cstdint>
#include <optional>
#include <string>

namespace oj::scene {
class Engine;
}

namespace oj::frontend {

using ui::MessageType;

// How a window is switched to (the front end's +0xBC; asked for by 0x005A8520, made by 0x005A8580).
enum class SwitchMode : int32_t {
    None = 0,
    Replace = 1,                  // the windows replaced after a frame (it becomes ReplaceNow for that frame)
    ReplaceNow = 2,               // the windows replaced; the windows wait meanwhile
    ReplaceAfterTransitions = 3,  // the top window's elements sent out, the windows replaced when they are out
    ReplaceAlways = 4,            // replaced even when the bottom window's state is registered under the name
    Push = 5,                     // a window over the current one
    Pop = 6,                      // the top window taken off
    Swap = 7,                     // the top window taken off and the new one put over the rest
    Overlay = 8,                  // a window over the windows
};

class WindowHost {
public:
    virtual ~WindowHost() = default;

    // The switch (0x005A8520) to the window of the name; false while another switch waits.
    virtual bool request(const std::string& state, SwitchMode mode) = 0;
    // A window asked for and not yet switched to (the switch's name, +0xB8, not empty).
    virtual bool switchAsked() const = 0;
    // The window under the windows over it (the front end's current, +0x2C): what a window over it writes into the
    // base window's elements (the garage's price and money under its customising windows) goes there.
    virtual ui::Screen& baseScreen() = 0;
    // The message window (0x005A80E0): a note, or a question whose answer (1 yes, 0 no) goes to *answer; small: the
    // smaller font; startNo: the question starting on no.
    virtual void message(MessageType type, const std::wstring& text, bool small, int* answer, bool startNo = false) = 0;
    // 0x005A8330: the message's title.
    virtual void messageTitle(const std::wstring& text) = 0;
    // A message up (the front end's +0x44), and that message asked to close (its +0x48).
    virtual bool messageUp() const = 0;
    virtual void closeMessage() = 0;
    // The number of the message up (the front end's +0x108 while one is up, else 0; 0x005A80E0 counts them 1 ..
    // 9998), and the number the last message opened took (+0x108 itself).
    virtual int32_t messageNumber() const = 0;
    virtual int32_t lastMessageNumber() const = 0;
    // The message's window done with its transitions.
    virtual bool messageTransitionsDone() const = 0;
    // A line of scripts\jettext<language>.txt (0x004FAC80, the save device's and the network's texts), empty before
    // the driver window reads them.
    virtual const std::wstring& jetText(size_t line) const = 0;
    // The crew's name into the game's text lines of the crew (0x00409F30).
    virtual void setCrewNameTexts(const std::wstring& name) = 0;
    // An interface sound (0x005A8860), and one with a volume and the masks of its slot and voice.
    virtual void sound(const char* name) = 0;
    virtual void sound(const char* name, float volume, uint32_t mask) = 0;

    // The game's data (none in the tests' front end), its texts and the language.
    virtual GameData* gameData() = 0;
    virtual const TextTable& gameTexts() const = 0;
    virtual const Language& language() const = 0;
    // The interface's 3D engine (none without a device) and the loading thread's generator (the car views' models).
    virtual scene::Engine* engine() = 0;
    virtual CrtRandom& viewLoading() = 0;
    // The driver's settings (the save system's +0x54).
    virtual std::optional<save::Settings>& driverSettings() = 0;
    // The render device (the windows that draw lines of their own: the stats' graph).
    virtual render::IRenderDevice& device() = 0;
    // The front end's frame time (held to 0.1 s) and the game's ([G+4]+0x1C, not held).
    virtual float frameTime() = 0;
    // The direction held this frame (the input's +0x90: input::kUp .. kRight, else 0).
    virtual uint32_t scrollHeld() const = 0;
    virtual float gameFrameTime() const = 0;

    // A value of the driver's settings (a new driver's without one), and the settings the options change (made a new
    // driver's when none is loaded).
    virtual uint32_t settingsValue(save::Settings::Field field) = 0;
    virtual save::Settings& optionSettings() = 0;
    // The settings' music volume, and the front end's music (0x0046BD80: unless only the target is asked, its volume
    // now and the player's; always its target).
    virtual float settingsMusic() = 0;
    virtual void setMusicVolume(float volume, bool targetOnly) = 0;

    // 0x0046BDD0: the front end's music played (unless it plays) at the settings' volume.
    virtual void startMusic() = 0;
    // The interface's sounds (the race's in a race's pause).
    virtual UiSound& uiSounds() = 0;

    // 0x005A5F20, the windows' enter: a Menu element back to its items, the item the record's leave kept highlighted
    // (none the first time), the tickers started again; 0x004C7F70, the windows' leave: Menu's item kept by the record.
    virtual void baseEnter(ui::Screen& s, const std::string& state) = 0;
    virtual void baseLeave(ui::Screen& s, const std::string& state) = 0;
    // 0x004850E0: a window's help page (CMSAdvice over it) once, unless forced.
    virtual void helpPage(const std::string& window, bool force) = 0;
    // The window over the windows (mode 8, the front end's +0x30): up, and taken away (its leave).
    virtual bool overlayUp() const = 0;
    virtual void closeOverlay() = 0;

    // The stock's globals the windows share.
    virtual SharedState& shared() = 0;
};

}  // namespace oj::frontend
