// Unit tests: what the game window's procedure does with a WM_SYSCOMMAND (Game/Platform/Platform.h systemCommand).

#include "Framework/Test.h"

#include "Game/Platform/Platform.h"

#include <windows.h>

namespace {

using oj::SystemCommand;
using oj::systemCommand;
using oj::test::check;

}  // namespace

// The key menu: Alt+Enter minimizes, any other key's menu is dropped (the system menu never opens from Alt).
OJ_UNIT_TEST(SystemCommand, KeyMenu) {
    check(systemCommand(SC_KEYMENU, VK_RETURN) == SystemCommand::kMinimize, "Alt+Enter: minimized");
    check(systemCommand(SC_KEYMENU, 0) == SystemCommand::kDrop, "Alt alone: dropped, no menu");
    check(systemCommand(SC_KEYMENU, ' ') == SystemCommand::kDrop &&
              systemCommand(SC_KEYMENU, 'f') == SystemCommand::kDrop,
          "Alt and a key: dropped");
    check(systemCommand(SC_KEYMENU | 3, 0) == SystemCommand::kDrop, "the command's low four bits passed over");
}

// The close ends the game; the screen saver, the hot key and the monitor's power are dropped; the rest the system's.
OJ_UNIT_TEST(SystemCommand, Others) {
    check(systemCommand(SC_CLOSE, 0) == SystemCommand::kClose, "close: the game ends");
    check(systemCommand(SC_SCREENSAVE, 0) == SystemCommand::kDrop &&
              systemCommand(SC_HOTKEY, 0) == SystemCommand::kDrop &&
              systemCommand(SC_MONITORPOWER, 2) == SystemCommand::kDrop,
          "screen saver, hot key, monitor power: dropped");
    check(systemCommand(SC_MOUSEMENU, 0) == SystemCommand::kDefault &&
              systemCommand(SC_MOVE, 0) == SystemCommand::kDefault &&
              systemCommand(SC_MINIMIZE, 0) == SystemCommand::kDefault,
          "the mouse's menu, move, minimize: the system's");
}
