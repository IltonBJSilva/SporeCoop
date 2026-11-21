---
id: BUG-9305
title: Space: a guest's own spaceship editor that takes 36 s to load drops the guest (the host hears nothing for 30 s)
status: open
kind: bug
stage: Space
reported: 2026-11-21
source: found on the way (feat/space-late-launch, lane A)
role: guest
versions: 0.7.15.96
fixed_in:
branch: feat/space-late-launch
commits: 73d242a7
tests: tools/tests/coop_space_late_launch.py step 3 (kit.test.hang 40000 after Design; not run: no Linux lab here); by hand on lane A with agentLL/hang.py (not in the repo)
signature: guest: WATCHDOG (no frames) in App::cJob::Wait under Palettes::StandardItemUI after "my spaceship editor requested"; host: "player N left: timed out"
docs: docs/re/space.md#space-late-launch
related: BUG-9299, BUG-9291, BUG-9290
---

## Report
Found on the way (feat/space-late-launch, lane A, with three other lanes' games running on the same PC): the guest
pressed Design (BUG-9299), the game's spaceship editor took 36 s to load (the sporekit watchdog wrote a hang report at
30 s: the main thread in the editor palette's texture jobs, `App::cJob::Wait` under `Palettes::StandardItemUI`), and
the host ended the guest's session at 30 s ("player 1 left: timed out (nothing heard for the timeout)"); the guest's
screen then showed "disconnected: connection to host lost (timeout)". The first runs of the same editor loaded in 3 s.

## Root cause
Observed: a guest's game sends nothing while its main thread stands, and the host's net::Session drops a peer after
`timeout_ms` (30 s) without packets. BUG-9291 made the guests wait longer for a HOST that tells it is about to load an
editor; nothing did the same the other way round, for a guest's own editor (the Space launch's spaceship editor here;
the Creature own editor has the same exposure, not seen).

## Fix
The guest tells the host before it asks for the editor (K_LAUNCH state 2, flushed at once with net::Session::flush_now,
the request a second later); the host then waits up to busy_model::kLongWaitMs (180 s) for its peers' packets
(host_busy.cpp host_busy_guest_long_wait) until that guest tells another state (offered again, done, not now) or
10 min passed or it left. The Design's open timeout counts frames in Space since the request (a long stand has no
frames). `coop.status` shows `guest_long=<n> told=<n>` on the host's "host busy" line. Switch (on the host):
`coop.fix space.launch_long_wait off` (the session's 30 s).

## Verification
By hand on Windows lane A, driver `agentLL/hang.py` (the batch folder, not in the repo): the BUG-9299 setup (Dopov, a
late join), then the guest's Design followed at once by `kit.test.hang 40000` (its main thread stands 40 s before the
editor request, as the slow load did). Not a lab run (no Linux lab on this PC).
* PASS (switch on, 10:46-10:50, hang_on.log): the host's "host busy" line shows `timeout=180000 ms guest_long=1` during
  the stand; after 50 s the guest is still in the host's session, its editor opens, and after its red cross the host's
  timeout is back to 30000 ms (guest_long=0).
* FAIL (`coop.fix space.launch_long_wait off` on the host, 10:50-10:53, hang_off.log): the host's timeout stays 30000 ms;
  "player 1 'Bob' left: timed out (no packets)" at 10:50:22.
* The original report: run_on2.log (10:26, before the fix): the real 36 s editor load, the host dropped the guest.

## History
- 2026-11-21: opened.
- 2026-11-21: fixed on feat/space-late-launch (73d242a7), ON PASS / OFF FAIL by hand on lane A.
