---
id: BUG-9312
title: Space: every save of the host (autosave too) shows "Your alliance with the <partner>s empire has been severed" (the allies mirror undone before the save)
status: open
kind: bug
stage: Space
reported: 2026-11-21
source: in-game test of feat/space-guest-home, lane A
role: host
versions: 0.7.15.97
fixed_in:
branch:
commits:
tests:
signature:
docs:
related: BUG-9310
---

## Report
In-game test of BUG-9310 (lane A, Steam exe, dev1's Pradnie in Space, the guest's new colony on its homeworld): right
after the host's autosave "a colony founded" (sporekit.log 13:52:55 `autosave: the host's game saves (a colony
founded)`), the host's HUD showed the event line "Your alliance with the Bob's empire has been severed."
(E:\Repositorio\SporeCoop-wt\sgh\on_host_homesystem.png). Nothing was severed for the players: the mirror is applied
again after the save and the alliance with it.
How to reproduce: a guest with a colony of its own the host mirrors (space_allies.cpp); the host saves (pause menu,
`coop.save`, or the autosave); look at the host's event log.

## Root cause
Inferred (not checked in the disassembly): space_allies.cpp's h_Save undoes every mirror before the game writes
(unapply -> set_relation(m, false) -> the cEmpire ally-remove calls for both sides), and the game posts its "alliance
severed" UI message for a player-side removal (docs/re/space.md 15.3: DeclareAlliance / its removal post UI messages
when the player is one side). The re-apply after the save adds the alliance back silently. A save-time undo should
clear the flags and vectors without the game's message (as rel_flags does), or the message should be suppressed.

## Fix
<!-- what changed and why; the switch to turn it off for tests (coop.fix <name>) -->

## Verification
<!-- the test: FAIL without the fix, PASS with it, each with its lab run id (out/lab/YYYYMMDD-HHMMSS);
     or a line `no-fail-run: <reason>` (docs only, regression only, not reproducible) -->

## History
- 2026-11-21: opened.
