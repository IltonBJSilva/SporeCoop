---
id: BUG-9296
title: Space own empires: the guest's HUD captain portrait still shows the host's captain (Brawgle) after its own captain key 7CA81684 is set
status: open
kind: bug
stage: Space
reported: 2026-11-21
source: agent smoke 0.7.15.96 lane A
role: guest
versions: 0.7.15.96
fixed_in:
branch:
commits:
tests:
signature:
docs: docs/re/space.md#space-own-ship-captain
related: BUG-9279
---

## Report
Smoke test of the friends' build 0.7.15.96 on lane A (Windows, Steam exe 3010a413, two instances), 2026-11-21: the
host loaded the Space save Dopov (dev1), hosted, the guest (dev2) joined and waited in the galaxy menu until the host's
ship left the home planet (`coop.space takeoff`), then came into Space. The guest's `coop.space empires`: `own=1 seat=1
money=100000 name='Bob's empire' ... ship=06664CDC captain=7CA81684 archetype=F966D429 (set 1)`, its log "empires: my
archetype F966D429 and its abilities set on my game", `coop.captain` on the guest: `captain=7CA81684/2B978C46/40626200`
(its own creature 'Guleskate'). But the guest's HUD (the captain portrait at the bottom left, beside the planet map)
still showed the host's captain "Brawgle E..." 60 s later, the same portrait as on the host's game
(agentA596_space_guest.png, r96_space_guest2.png in the agents' folder). The ship 06664CDC is the host's design: expected, dev2
has no own spaceship design (`mine ufo: 06664CDC '' (0 own)`).

To reproduce: the steps above (E:\Repositorio\SporeCoop-wtgentA596_space.py then r96_space2.py), look at the
guest's HUD portrait.

## Root cause
Not analysed. Inferred: space_empires.cpp sets cEmpire +0xC4 (cEmpire_SetCaptainKey 0xC31100) on the guest's game,
but the HUD's captain portrait (and probably the captain screen) was built from the save's captain at the Space start
and is not refreshed; `coop.captain` also shows `species=0679B8C8` (the world player species, the host's) on both
games. Not a crash, cosmetic as far as seen.

## Fix
<!-- what changed and why; the switch to turn it off for tests (coop.fix <name>) -->

## Verification
<!-- the test: FAIL without the fix, PASS with it, each with its lab run id (out/lab/YYYYMMDD-HHMMSS);
     or a line `no-fail-run: <reason>` (docs only, regression only, not reproducible) -->

## History
- 2026-11-21: opened.
