#pragma once
// The online windows' globals and the command line's online switches the main menu takes.

#include "Core/Network/NetTransport.h"

#include <cstdint>
#include <optional>
#include <string>

namespace oj::frontend {

// The online switches of the command line (the main function 0x00418300), taken by the main menu's enter: -host
// ([0x0075ED50]), -connect a.b.c.d:port ([0x0075ED54], the address [0x0075ED58]), -name ([0x0075ED78]), -email
// ([0x0075EDB8]) and -ppass ([0x0075EE20]).
struct OnlineCommandLine {
    bool host = false;
    bool connect = false;
    std::string address;   // 31 characters at most
    std::wstring name;     // 30
    std::wstring email;    // 50
    std::wstring password;  // 30
};

// What online_main chose for online_mode to go on to: a quick match, a search, a session to create.
enum class OnlineChoice : int32_t { Quick = 0, Search = 1, Create = 2 };

// The online windows' globals (they outlive the front end's objects).
struct OnlineForms {
    std::wstring nick;      // [0x00728C00]: the edit buffers (30, 50, 30 and 30 characters)
    std::wstring email;     // [0x00728B98]
    std::wstring password;  // [0x00728B54]
    std::wstring verify;    // [0x00728B14]
    std::string createFocus;   // [0x0075EE74]: the button online_create_gs's enter highlights
    std::string connectFocus;  // [0x0075EE88]: online_connect_gs's
    int32_t commandLine = 0;     // [0x0075EE84]: the command line's account in the buffers: online_connect_gs
                                 // logs in at once
    int32_t accountChanged = 0;  // [0x0075EE98]: online_main's enter saves the driver (autosave)
    int32_t host = 0;            // [0x0075EE9C]: -host: online_main goes on to online_mode (create)
    int32_t connect = 0;         // [0x0075EEA0]: -connect: online_main joins connectAddress
    std::optional<net::Address> connectAddress;  // [0x00715D28]
    std::optional<net::Address> invitation;      // [0x00715D24]: a session online_main joins
    OnlineChoice mode = OnlineChoice::Quick;     // [0x00721858]: online_mode's choice
    int32_t details = 0;         // [0x0075EEA4]: the create windows as the details of a friend's session
    int32_t gridWalked = 0;      // [0x0075EE6C]: the online career's grid walk entered (link_lobby clears it)
    std::string background;      // [0x0072182C]: the window online_bg opens
    int32_t friendsFromMain = 0; // [0x0075EE70]: the friends opened from online_main (their way back is to it)
    // The friends' and players' windows: the options' local player and user ([0x00715D2C], [0x00715D30]), the
    // voice window's friend, our address, user and voice mail ([0x00715D34] .. [0x00715D40]) and its way back
    // ([0x0075EEFC]); the name highlighted in online_friends ([0x0075EEB0]); the matchmaking and the network game
    // the friends' windows made ([0x0075EEA8], [0x0075EEF0]); a friend's session being joined ([0x0075EEF4]); a
    // closedown asked ([0x0075EEF8]).
    int32_t optionsPlayer = -1;
    int32_t optionsUser = -1;
    int32_t voiceFriend = -1;
    std::optional<net::Address> voiceAddress;
    int32_t voiceUser = -1;
    int32_t voiceMail = -1;
    std::string voiceBack;
    std::string friendName;
    int32_t friendsMadeMatchmaking = 0;
    int32_t friendsMadeNetGame = 0;
    int32_t friendJoining = 0;
    int32_t closedownAsked = 0;
};

}  // namespace oj::frontend
