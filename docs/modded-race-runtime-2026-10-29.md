# Normal frontend and modded races — 29 October 2026

Two complete one-lap races now pass with the installed car packages, from the
ordinary menu through selection/setup, actual driving, results and Continue back
to the performance-group selector. Both races ran in the same production process;
this also exercises unloading one race and loading a different modified car.

| Player car | Preparation | Conditions | Field | Finish |
| --- | --- | --- | --- | --- |
| Volvo V70 2.4 | Factory, 168 bhp; decline auto-mod | Night/wet | 4 cars | 3rd, 67.37 s |
| Peugeot 406 Coupe V6 | Accept auto-mod; selector shows 344 bhp, carbon bonnet, retail vinyl, wheels and body additions | Afternoon/clear | 6 cars | 4th, 57.45 s |

Both loaded `ENGSTROM DISTRICT / Engstrom_R1`, displayed as San Ricardo in the
frontend. The first field included the added Jetta 2.0 FSI, which finished second
at 63.80 s with its correct model name on the results screen. All competitors
finished both events. Night/wet setup correctly offers at most four cars; six
remain available in clear daytime conditions.

The result images display hundredths truncated from the more precise internal
timer: for example the Peugeot screen shows 00:57:44 while the rounded log reads
57.45 s. These are reported as observed rather than silently made identical.
The player-name line is empty because this isolated custom-race run acquired a
controller without creating/loading a named career profile; rival names and
player crew/model labels are present.

## Identity and method

Production `out/linux/juiced` SHA256:
`d44153bb2ad54ded777f1cbb57af59a9c5eee0b93b48658c96f51b60d60f7fec`.
No production source or executable was modified for these tests. The command
starts at `--screen menu`, with installed mods, seed 42 and `--autopilot`. It
does not use `--test` or a direct `--track` shortcut. Autopilot drives the actual
physics; it does not assign finish positions or bypass results handling.

The two preload helpers attach an SDL virtual gamepad and capture the displayed
framebuffer. Input is delivered through the Linux controller backend and the
normal frontend. Button pulses were reduced from 300 ms to 150 ms after the
longer pulses repeated and selected Online instead of Custom Race; this initial
navigation mistake is retained in the log. The Online logon screen was exited
normally without creating an account or contacting a person.

The private Xvfb uses llvmpipe, and audio uses SDL's dummy sink. These runs certify
the observed frontend/race transitions and captures, not Radeon performance,
audible output, manual handling quality, Android rendering or physical controls.
The separate Radeon galleries and district samples retain their own scope.

The support directory was created exclusively for this run. Its runtime log
confirms the owned `production-pad/support/ModSaves` save root. No existing career
was loaded or edited. The assembled mod overlay was temporary. The game and its
owned Xvfb both terminated normally, exit 0; no test game remains running.

## Rejected fixtures and independent checks

Three earlier keyboard-injection runs stopped before a race. Synthetic key-down
events sometimes reached the frontend without their matching key-up. A copied
logging-only diagnostic executable reproduced this; switching to queued events
did not reliably solve it. These failures do not establish a production keyboard
or car-selector defect and are not counted as completed races. Their logs and
separate binary identities remain under `full-race-ui`.

`full-race-ui/verify_run.py` checks each selected player/track, actual Go and
positive driving telemetry, every competitor's finish, player finish, results,
normal return, unmodified binary identity and final exit. It rejects all three
incomplete fixture runs. Additional negative controls remove the player-finish
line, remove the normal-return line, or provide only the boot portion; all are
rejected. A successful process exit by itself cannot pass this checker.

Evidence root: `out/validation/linux-current-20261029/full-race-ui`.
Receipt: `normal-races-receipt.json`; log/process/input trace and 26 captures:
`production-pad/`. Particularly useful images are `v70-preview.png`,
`race-progress.png`, `race-complete.png`, `after-continue.png`,
`peugeot-auto-mod.png`, `peugeot-driving.png`, `peugeot-results.png` and
`peugeot-after-continue.png` in its `captures` directory. The receipt hashes the
log and images. The verifier and helper source are retained beside it.

## Delivery still pending

Artifact hashes were reconfirmed during this follow-up: combined r5 ZIP
`3537ef0d6454b7cdca73d09d4f4d1d92f011cba2d65f2c8bf91df20d280e1373`,
ARM64 v17 APK
`634ba96bf86dc27a962d7dc9f4482028ac4085883709f928f46c536d581b612b`.
ADB currently lists no devices. Neither the final APK nor r5 has been installed
on the RP5. That deployment, save-preservation comparison and physical visual /
driving check remain open. These two Linux races do not close the full project
or every customization/damage combination.
