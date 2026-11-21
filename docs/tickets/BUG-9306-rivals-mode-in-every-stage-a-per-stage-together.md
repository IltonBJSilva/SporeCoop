---
id: BUG-9306
title: Rivals mode in every stage: a per-stage Together / Rivals choice, the race's stage end (first wins, catch-up timer, all move on)
status: open
kind: feature
stage: All
reported: 2026-11-21
source: owner
role: 
versions: 0.7.15.96
fixed_in:
branch: feat/rivals-stages
commits:
tests: tests/validate_tests.cpp test_rivals; by hand on lane A with agentRV/race_cell*.py, race_cr1.py (switch off), race_cr2.py, space3.py (the batch folder, not in the repo)
signature:
docs: docs/11-rivals.md#rivals-stages-race, docs/re/cell.md#cell-rivals-race, docs/re/creature.md#creature-rivals-race
related: BUG-0794, BUG-9265, BUG-9268, BUG-9274, BUG-9308
---

## Report
The owner's wish (2026-11-18, again 2026-11-21): a Rivals option in every stage (docs/11-rivals.md; the idea ticket
BUG-0794). Only Space had one (space_rivalry.cpp, F9 > Space > "Switch to rivals"). The "Together" behaviour (own DNA
and bar, own nest / village / nation / money / cards / empire, nobody advances alone) stays the default.

## Root cause
Not a bug: a feature.

## Fix
* Step 1: the stage table in the session settings (rivals.cell .. rivals.space, rivals.catch_up; group "Rivals" of the
  co-op window's settings page; Live; kept with the host's world; the guests see it). Space's rivals mode follows the
  setting and the setting follows the Space page's button. The co-op aids (settings marked `aid`) read off in a Rivals
  stage. Wider value buttons on the settings page ("Together" fits).
* Step 2: the race (rivals.cpp, rules rivals_logic.h, MSG_RIVALS 111 / session 1.8): Cell and Creature end when the
  first player's own bar is full and the catch-up timer ran out (or everybody is done); the host's own bar is filled
  for its game's native end; the existing vote moves everybody on. Notices, the HUD status block and the lobby line show
  the race; `coop.rivals`.
* Switch: `coop.fix core.rivals_stages off` (every stage reads Together, 0.7.15.96).

## Verification
By hand on Windows lane A (no Linux lab on this PC), two instances:
* Cell (Talviorus): the host's settings page "Cell stage: Rivals" (the guest's page shows it greyed); the guest's bar
  full first: both games "Bob wins the Cell stage! 0:45 left to catch up", the timer in the status block; after 45 s
  "Time is up", the host's food 36 -> 1000, its History check started the vote, both games entered Creature. PASS.
* Creature (Marona): `coop.fix core.rivals_stages off` on both: the guest's brain 4, after 60 s no race, the host's bar
  not filled, both still in Creature (race_cr1_off.log: the Together behaviour, the FAIL twin of the race). Switch on:
  Bob wins at once, the host plays on during the 45 s, then its brain 0 -> 4 (DNA kept 0), its evolve button started
  the vote, the guest's I'm ready opened the host's stage-end editor (race_cr2_on.log). PASS.
* Space (Dopov): the settings page switched both games to the rivals mode, the Space page's "Switch to allies" set the
  setting back to Together on both games, `coop.rivals set space ...` both ways (space_on2.log, space3_on.log). PASS.

## History
- 2026-11-21: opened.
- 2026-11-21: steps 1 and 2 (table, Cell / Creature race, Space wired) on feat/rivals-stages, tested on lane A.
