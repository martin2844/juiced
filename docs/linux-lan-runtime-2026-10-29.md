# Two complete Linux frontends over loopback — 29 October 2026

The ordinary Multiplayer entry now has an observed complete two-process race:
discovery, joining, lobby, host setup synchronized to the guest, car readiness,
loading/countdown, driving, finishing, return to lobby and guest departure.
No production networking or game logic was changed for this verification.

Both processes run the installed mod composition and the production executable
SHA256 `d44153bb2ad54ded777f1cbb57af59a9c5eee0b93b48658c96f51b60d60f7fec`.
They boot at the normal menu, without `--test` or `--track`, and use `--autopilot`
to drive actual physics. Each has its own support/save directory, command file,
virtual SDL gamepad and owned Xvfb display. Rendering is llvmpipe and audio is
dummy; there is no hardware/FPS or audible-output claim.

Host uses `--net 31666,31667,31668,127.0.0.1`; guest uses
`--net 31668,31669,31666,127.0.0.1`. These existing diagnostic options direct
discovery and game traffic to this machine. The host appears in the guest's
search as `(1/6)`, and joining fills two lobby slots. No named profile was created
or loaded, so both names are empty. The independent process labels identify
which player was controlled; this does not certify named-profile UI.

The host selected Group G, Angel North Central / `Angel_North_R1`, one lap,
afternoon/clear. The guest followed setup without receiving local selection
inputs. Both selected the default factory Punto and marked ready. Both loaded
the same requested scene, waited for players, counted down and reached Go.
The game's grid log labels every non-local car “AI”; the network grid source
actually assigns the other peer's car `CarKind::Remote` and its state hooks
apply the received network data. The label alone is not evidence of an AI rival.

The guest finished first (its local log: 65.84 s), host second (66.02 s). Both
returned automatically to `link_lobby`, showing the same ordered scores:
10 points / 1 race for the winner and 6 points / 1 race for the other player.
The remote prediction logs report different local crossing times; identical
cross-peer displayed lap times were not verified. The capture named
`network-results.png` was taken after the return and shows the scored lobby,
not a captured timing-results table. These limits are retained explicitly.

Guest Back -> “Leave game session?” -> Yes returns to `link_search`. The host
remains in the lobby with one slot, 6 points and 1 race; the departed slot is
cleared. Both games were then quit normally. Both game processes and both owned
Xvfb servers exited 0 and no longer exist. Existing user saves were not loaded.

Evidence: `out/validation/linux-current-20261029/full-race-ui/lan-{host,guest}`,
including process identities, full runtime logs, pad actions and PNG/BMP captures.
`lan-runtime-receipt.json` binds the logs/images and observed scores. Its checker
`verify_lan_run.py` requires the ordered frontend/race/return sequence, two
finishers, expected local positions, binary identities and terminal processes.
Six altered-log controls (no Go, no local finish, no normal return for each peer)
are rejected. These complement the six socket/session checks now included in
Linux's nine passing CTest suites.

This is one same-machine LAN-path race with two players and factory cars.
Physical network latency, packet loss, six-player races, different installed
mod rosters, spectators, named careers, voice and Internet services remain
unverified. It does not close the broader project or the pending RP5 deployment.
