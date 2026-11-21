---
id: BUG-9299
title: Space: a partner who joins when the host is already in Space (or who had no own Civ launch) gets no offer to design its own spaceship
status: open
kind: feature
stage: Space
reported: 2026-11-21
source: owner (separate lives batch)
role: guest
versions: 0.7.15.96
fixed_in:
branch: feat/space-late-launch
commits: 73d242a7
tests: tools/tests/coop_space_late_launch.py (suite space_late_launch, not run: no Linux lab here); by hand on lane A with the batch driver agentLL/run.py (not in the repo)
signature:
docs: docs/re/space.md#space-late-launch, docs/06-testing.md#testing-space-late-launch
related: BUG-9295, BUG-9279, BUG-9305, BUG-9296
---

## Report
The owner's "separate lives" plan: every partner has its own Space empire, ship and captain. BUG-9295 lets a guest with
its own nation design its own spaceship at the Civ -> Space vote. A partner who joins when the host is already in Space
(or who had no launch of its own) got no offer: its own empire (space_empires.cpp, BUG-9279) flew the newest ship design
of its library, or the save's (the host's) ship when it had none (lane A, Dopov: "no own ship designs", the guest's
empire ship 06664CDC = the host's).

## Root cause
Missing feature. Lineages (PR #196) has a launch offer (MSG_SPACE_LAUNCH Offer / Request / Continue / Ready / Cancel)
that sends the partner the host's retained Civilization checkpoint (an archive the host keeps when its Civ becomes
Space-ready) so the partner replays the Civ -> Space launch. Not ported: it needs the checkpoint archive and the
founders roster of the Civ save, and a save played into Space before the archive existed (Dopov, Pradnie) has none.
Observed (runtime, lane A): the game's spaceship editor 0x96B24187 opens from Space with a plain EditorRequest and returns
to the same Space world with the design in the player's library (docs/re/space.md#space-late-launch).

## Fix
Written new (space_launch.cpp, part of the Space feature "empires"): a guest in the host's Space world with an empire of
its own and no choice recorded for the planet (its worlds.ini [late_launch]) is offered its own spaceship (a line, the
co-op window's Space page rows Design / Not now, `coop.space empires launch go|no`). Design opens the game's spaceship
editor from Space past the vote; its check mark saves the design and returns to Space; space_empires then flies it
(Empire_SetSpacecraftKey) with the player's species as captain, and the host's game gets the design as the partner's ship
(creations share, the fleet mirror's model). MSG_SPACE kind 105 K_LAUNCH (guest -> host) tells the host each step
(a line for everybody, its record in `coop.space empires launch`); space family wire 1.5. civ_launch.cpp records "civ"
for the planet after BUG-9295's own launch so no second offer comes. Switch: `coop.fix space.late_launch off` (no offer:
0.7.15.96).

## Verification
By hand on Windows lane A (two instances of the Steam exe 3010a413, the GOG 2024 code), driver `agentLL/run.py` (the
batch folder, not in the repo): dev1's Dopov Space save, the host in Space and off its home planet, then the guest
(dev2, seat 1 with its own species 7CA81684) joins late: a real join, no test seam. Not a lab run (no out/lab id): no
Linux lab on this PC; the lab test coop_space_late_launch.py is written for it.
* PASS (switch on, 2026-11-21 10:58-11:05, run_on3.log; also run_on.log 10:12): the offer (the guest's line, the host's
  record state 1); `launch go` opens the spaceship editor from Space; the red cross returns to Space with the offer
  kept; the Space page's Design button opens it again and closes the co-op window; the design 'Bob9276' saved, back
  in Space the guest's empire flies 292B8FF3 (its new design) with captain 7CA81684; on the HOST's game "player 1 ufo:
  7C85E35F/476A98C7/40656200 'Bob9276'", its launch record "state=3 ship=292B8FF3 captain=7CA81684", and the guest's
  mirror ship in the host's fleet has model 7C85E35F (run_on_host_meet.png: the white design labelled Bob next to the
  host's ship over Dopov). The "Not now" button (t11.py) records "no" and the host gets state 4.
* FAIL (`coop.fix space.late_launch off` on the guest, 10:24-10:30, run_off.log): no offer in 60 s (offers=0), no
  editor; the guest's empire flies the newest design of its library from before.
* One ON run (run_on2.log, 10:26) failed when the guest's editor took 36 s to load and the host dropped it: BUG-9305,
  fixed in the same commit; run_on3 is after that fix.
No crash report in any run (the 10:28 report of instance 2 is the 36 s hang of run_on2).

## History
- 2026-11-21: opened.
- 2026-11-21: written new on feat/space-late-launch (73d242a7); in-game ON PASS / OFF FAIL on lane A.
