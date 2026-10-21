# The network (`src/Core/Network`)

How the LAN and online games are put together: the transport, the session, the cars' states, GameSpy's parts and the
windows that use them. What the stock game does is `docs/spec/28-network.md`.

GameSpy's servers are closed, so the online modes find no one; the LAN (link-up) game works. OpenJuice keeps the
stock game's GameSpy host names and protocols as they are (`gamespy.com`, the game's keys): no replacement service.

## The layers

```
the front end's network windows (Game/Frontend/Network)     the race (Game/Race/TrackView/TrackViewNetwork.cpp)
        |                                                             |
   NetSession: machines, players, the host's setup, stages, the race's messages, NetCar
        |
   Transport ("jetNet"): UDP, the LAN search on 8666, the game on 8667, reliable packets, XOR-coded
        |
   (online) Matchmaking over GameSpy: QR2 hosting, the server browser, NAT negotiation
            OnlineUser / GP: login, presence, friends;  statistics (gstats, ghttp);  voice (GameSpy Voice, Speex)
```

- **`Transport`** (`NetTransport.h`): the game's UDP transport. A socket on 8666 for the LAN's search (broadcast) and
  one on 8667 for the game, every peer to every peer; each packet coded by a stream of keys; reliable packets answered
  and sent again.
- **`NetSession`** (`NetSession.h`): the session on the transport: the machines and players, the host's setup of the
  race (place, route, laps, time of day, weather, type, difficulty) and its stages (`SessionStage.h`: the lobby, the
  setup's pages, the cars, loading, loaded, the count, the race). `NetGame` is the transport with its session;
  `startNetGame` makes it as a network window opens, `netGame()` gives it (none outside a network game).
- **`NetCar`** (`NetCar.h`): a race car's state as the session passes it (64 bytes): made of our car, applied to
  another machine's car at once or by steps, carried on by its own velocities between messages.
- **`Matchmaking`** (`Matchmaking.h`): the online game's hosting and searching over GameSpy: a hosted session listed
  by QR2 with the game's keys, the search through the server browser, the join directly or after a NAT negotiation.
  `OnlineSessions.h` is the game's side of it: the profile's online settings and the windows' calls.
- **`GameSpy/`**: the parts of the GameSpy SDK the stock game links, ported as it has them: the shared parts
  (`GameSpy.h`: the game's name and key, the challenge's answer, the available check), QR2 (`GameSpyQr2.h`), the
  server browser (`GameSpyBrowser.h`), NAT negotiation (`GameSpyNatNeg.h`), Presence (`GameSpyPresence.h`, the user
  object `GameSpyUser.h`), the statistics (`GameSpyStats.h`) and HTTP (`GameSpyHttp.h`), Voice (`GameSpyVoice.h`).
- **The game's online objects**: the records and leaderboards (`OnlineRecords.h`, `OnlineStats.h`), the voice
  (`VoiceChat.h`). `NetTcp.h` is the services' non-blocking TCP.

## Each frame

`runGame` runs the network after the phase's frame and the sound (`app.md`), in the stock order: the session's frame
(`netGame()->session.frame`), the online user's (GameSpy Presence), the matchmaking's, the statistics', the voice's.
Each SDK part is polled from its frame ("think"), never from a thread.

In a race the session's hooks are the race's (`TrackView::netHooks`): the cars' states sent and applied, the race's
messages, the watched racer, the notes of a machine lost (`TrackViewNetwork.cpp`). The race's watches of the network
run before and after its front end's windows (`race-frontend.md`).

## The windows

The network's windows are the family `Game/Frontend/Network` (`frontend.md`): the link-up (LAN) windows
(`LinkWindows`), the online windows (sessions, the race's setup, friends, players, statistics, voice). They reach the
network through `NetworkHost` and `OnlineHost` (the waits, keys, hooks, joins), implemented by the front end
(`FrontendNetwork.cpp`, `FrontendOnline.cpp`).

## Testing

`tests/Integration/Network` runs each part on the loopback, with stand-ins of GameSpy's servers where one is needed:

| suite | what |
|---|---|
| `Transport` | the keys, a host found by a search, peers joining, reliable packets |
| `Session` | a host and a guest: names, the setup, the stages |
| `ServerBrowsing`, `Matchmaking`, `NatNegotiation` | QR2 and the browser against a stand-in master, a host found and joined, a negotiation |
| `Presence` | GameSpy Presence against a stand-in connection manager (`tests/Framework/Fakes/FakeGpcm.h`) |
| `Http`, `Leaderboard` | ghttp and the leaderboards against a stand-in server |
| `Tcp` | the services' TCP |

`tests/Unit/Network` checks pieces without sockets (the SDK's shared parts and ids, Presence's messages, the online
records, the voice). The debug build's `--net <lan>,<game>,<to>[,<ip>]` moves the network's ports, so that two copies run
on one machine.
