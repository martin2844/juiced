# Linux test-matrix audit — 2026-10-31

The native test matrix now compiles nine previously omitted portable integration
sources. CTest increases from10 to17 suites. The final ordinary production test
runner reports **407 passing checks and1 failure** against the original resource
inventory. The remaining failure is the same119 rejected original D8M files;
it has not been excluded, allowed or relabelled as a pass. Unit tests report
**388 passing checks,0 failures**.

This pass changes the Linux test list and two network test fixtures. It changes
no production engine code, car assets, physics, save formats or Android code.
The habitual launcher still executes the same development game ELF. The user is
using the RP5: no adb/device access, installation, export or screenshot occurred.

## Executed checks and source inclusion

Evidence: `out/validation/linux-current-20261029/suite-coverage`.
`verify_coverage.py` checks the complete case reports against their log totals,
source selection, exact original EXE, failures and preserved user data.

| Measurement | Before | Accepted current run |
| --- | --- | --- |
| CTest suites |10/10 |17/17 |
| Executed unit cases / assertions |116 /380pass |119 /388pass |
| Executed integration cases / assertions |114 /377pass,1fail |133 /407pass,1fail |
| Selected cases executing zero checks |0 |0 |
| Integration source files compiled |47of60 |56of60 |
| Unit source files compiled |57of61 |57of61 |

The additional unit cases are registered inside the newly compiled integration
sources: two ExeTables cases and one test-server packet-identity case. A source
file count is not a line/function/whole-game coverage percentage.

The nine additions are ExeTables, OnlineLogin, HTTP, Leaderboards, Matchmaking,
NAT negotiation, Peerchat, Presence and ServerBrowsing. Seven network suites
are individually registered with CTest, using local stand-in services, a shared
loopback-port resource lock and60-second timeouts. OnlineLogin and the original
EXE comparison run with retail resources in the full integration invocation.
The latter actually executes one check comparing the built-in game tables
against the original executable; it is not a missing-EXE skip.

The case-audit executable replaces only the runner object, adding begin/end
records and assertion deltas around the same selected functions. It links the
current compiled test objects and game libraries. The ordinary production test
runner independently returns the same407/1 result. The recorder is confined to
`out/validation`; production TestMain and the game ELF remain untouched. Five
negative controls reject missing-end, empty, zero-check, mismatched and duplicate
case reports.

The four omitted integration sources are Windows DirectSound, DirectInput,
DxDiag/system checks and Windows7 API checks. The four omitted unit sources are
CPU-info, DirectX checking, voice capture and system commands. The native port
explicitly provides an unavailable microphone/voice-capture backend; no native
voice-chat support is claimed by compiling unrelated audio tests.

## Presence fixture: receiver completion

Enabling Presence revealed a test ordering error. The original fixture stopped
when the sender's transfer-complete callback fired. The connection processes
peer output before transfer generation/callbacks, so that callback can precede
the next flush and receiver poll. The retained receiver transcript then had the
request but no begin/data/end frames. The fixture incorrectly failed its later
wire-content assertions and never reached its outgoing-peer subcase.

The corrected fixture pumps until both the completion callback and the expected
receiver final frame are present. It retains the existing request, normalized
file name, begin, exact `12345` bytes, length, MD5 and callback assertions.
Before:8pass/1fail; after:9pass/0fail, including the outgoing peer.
An independent Python frame parser verifies all346 retained TCP bytes: six
frames including four transfer frames. Seven negatives reject the original
premature transcript, wrong MD5, wrong payload, missing end, missing begin,
truncated terminator and wrong data length. No production protocol fix or live
file-transfer support is claimed.

## Matchmaking fixture: interleaved keepalive

One ordinary full run initially reported406/2. That failure is retained as
`production-integration-final.log`. Phase-only diagnostics reproduced it once
in six full runs; packet diagnostics reproduced it on isolated trial51 after
51 passing trials. Earlier isolated passing retries did not close the issue.

The failed packet transcript proves the actual master received packet types
**3,8,1,3**, all with the same QR2 instance key. The test consumed the valid
five-byte type8 keepalive as though it were the type1 challenge response. Its
next read then consumed the displaced challenge response as though it were the
NAT-message acknowledgement. Search/direct join still succeeded, but the
listing/NAT assertions failed. This was an assumption in the stand-in server
about datagram ordering, not evidence of failed game matchmaking.

The corrected stand-in consumes only exact five-byte type8 messages carrying
this instance key, with a bounded loop. Challenge, acknowledgement, hosted
address, search contents, session membership, NAT negotiation and leave checks
are retained. Wrong-instance, wrong-type, truncated, oversized and missing-key
packets are not silently discarded. Unsafe indexing on malformed/absent initial
packets is guarded.

Every run now injects a real valid keepalive before the challenge response.
An out-only negative executable restores the original one-read receiver while
keeping that deterministic interleaving: **2pass/1fail**. The current production
test runner on the same fixture gives **3pass/0fail**. Four unit checks verify
the filter identity and malformed-packet boundaries. Both the ordinary full
runner and the independent case recorder subsequently give407/1, with no
matchmaking failure. Production QR2/transport/matchmaking code is unchanged.

## Audio evidence and limits

The ledger's outstanding-audio row was stale: SoundListener.CameraTurned,
VoiceManager's two cases and CutScene.LastShotPausesTheSounds already execute
and pass all9 checks in the complete integration run. They validate listener
rotation, voice-slot/fade arbitration and paused scene sound logic using the
native SDL/dummy-output test host. They do not certify an audible mix, long-run
spatial sound, microphone input, physical Android audio or live online play.

## Clean build and unchanged runtime

Clean Nix r13 runs all17 CTest suites successfully and matches907 critical
current source inputs. Exactly three critical inputs differ from r11: the Linux
test list, PresenceTests and MatchmakingTests. Output:
`/nix/store/xp2zdsab74hagrgqy6aaqkz5ji6z2s8l-openjuice-1.02.5-linux-mods1`.

Its actual game ELF is byte-identical to r11:
`f141d956b7cd5fca85f797199be54bb7cb89edf0f93e829891a0c197da2e473c`.
All306 dependency store paths and installed notices match. The only changed
installed file is the wrapper, identical after normalizing its own store prefix.
The new wrapper passes an isolated `--help` invocation. Prior r11 complete-race
evidence is reused by this executable/dependency identity, not described as a
new r13 race. The existing r11 runtime archive remains the current game code;
there is no reason to export another identical307-path runtime closure.

Exact actual compiled source companion:
`out/releases/OpenJuice-20261031-r13-nix-source.tar.xz`,5777932bytes, SHA256
`af93e1e1820568fe12378eedd022fdefb20be500cde82e7ecdea0708cbda1257`.
All1343 files are independently restored and hash-checked; the restored NAR and
derivation match the actual build input. This is an exact-source restoration,
not a second independent compiler run. Existing r9 LGPL media source remains
applicable because the media dependency is unchanged. The intermediate r12
build/source receipts are retained as history; r13 adds the final matchmaking
fixture correction. No new Android version is needed for these Linux tests.

The current development ELF remains
`2a03fb550556047faf828794c624ea25aaa885f5d60fdb2efbc438daa50ce084`.
The local signed APK25 remains
`572ad6463c5e875c17ae78ef411e71ed6b0a5af1d6feea89e8389052753bf0d7`.
Neither is rebuilt or replaced for these test-only changes. RP5 remains
APK19/car packr5. Linux remains car bundle r8 / Wave2revision7.

All six personal Linux profile/creative files and every installed Wave2 file
match their prior hashes. The controlled synthetic save tree used by the full
tests is also byte-identical before/after. No public upload, push or commit was
made. An incremental checkpoint binds the prior split-main-menu checkpoint,
the three current source updates, documentation and replayable evidence.
After verification,24 explicitly owned diagnostic executables/object files
were removed from the audit directory (98256792 logical bytes). Their hashes,
sources, build helpers, logs and receipts remain; production binaries, Nix
source/store paths, user data and installed mods were not removed.

This closes omitted portable-suite execution and these two fixture defects.
It does not close the strict119-D8M inventory failure, remaining career/route/
rendering breadth, physical LAN/network conditions, Android runtime/controls/
car review, source-art limits or final distribution/publication review. The
broader verification goal remains active.
