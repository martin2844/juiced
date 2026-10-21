#pragma once
// The friends' and players' windows of GameSpy (docs/spec/28-network.md), each in its own files, and what they
// share.

#include "Core/Network/NetSession.h"
#include "Game/Frontend/Network/OnlineHost.h"
#include "Game/UI/UiScreen.h"
#include "Game/Frontend/Network/OnlinePlayers.h"
#include "Game/Frontend/Network/OnlinePlayerOptions.h"
#include "Game/Frontend/Network/OnlineFriends.h"
#include "Game/Frontend/Network/OnlineFriendOptions.h"
#include "Game/Frontend/Network/OnlineVoice.h"

#include <cstdint>
#include <optional>

namespace oj::frontend {

// The note of a game invitation or a friend request over the windows (0x004E1860): the front end's and the
// race's.
void drawPresenceIcon(render::IRenderDevice& device, ui::Screen& loader, const std::string& topWindow,
                      bool race);
// The online keys of the network game's windows (online_players and online_friends over online_bg): true when
// they took the event.
bool netOnlineKeys(WindowHost& host, uint32_t event);
// The options' target: the player and the user (referenced) the player's and friend's options are for.
void onlineOptionsTarget(WindowHost& host, int32_t player, int32_t user);

}  // namespace oj::frontend
