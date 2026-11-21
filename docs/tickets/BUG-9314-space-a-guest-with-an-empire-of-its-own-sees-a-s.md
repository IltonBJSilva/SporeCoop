---
id: BUG-9314
title: Space: a guest with an empire of its own sees a ship as a white box (its HUD ship portrait and the ship in its home system)
status: open
kind: bug
stage: Space
reported: 2026-11-21
source: in-game test of feat/space-guest-home, lane A
role: guest
versions: 0.7.15.97
fixed_in:
branch:
commits:
tests:
signature:
docs: docs/re/space.md#space-late-launch
related: BUG-9299, BUG-9310
---

## Report
In-game test of BUG-9310 (lane A, Steam exe, dev1's Pradnie, the guest Bob with an empire of its own): the guest's
screenshot at its homeworld (E:\Repositorio\SporeCoop-wt\sgh\on_guest_homesystem.png) shows a ship as a plain white
rounded box: the HUD's own-ship portrait (bottom right) and a ship in the scene (behind the "Empire 1" reward dialog).
Reported to the orchestrator as "the guest sees the host's ship as a white box".

## Root cause
Not confirmed. What is known (runtime, 2026-11-21, `coop.creations` / `coop.space empires` / `coop.fleet list` on both
games during the BUG-9312 runs):
* The white portrait is the guest's OWN ship: the guest's empire flies 292B8FF3 'Bob9276' (`mine ufo: 292B8FF3
  'Bob9276' (3 own)`, `ship #-1 ... (me) model=292B8FF3`; launch phase=offered, "my own empire flies a ship I did not
  design here"). The host's game shows the guest's mirror ship (7CA73C1D 'Bob9276' there) with the same white shape
  (E:\Repositorio\SporeCoop-wt\sgh\on_host_homesystem.png, near the comm portrait). Inferred: 'Bob9276' is a design an
  earlier test made by pressing the spaceship editor's check mark on an empty design (BUG-9299 runs on dev2): the
  editor's bare white hull, rendered correctly. Then it is not a bug.
* The host's ship: the host has no own UFO design in its library (`mine ufo: 43C67520 '' (0 own)`); its creation share
  gives the guest `player 0 ufo: 43C67520/476A98C7/40656200 ''` (no name). If the guest's game does not have that
  design's files, its model of the host's ship could be a placeholder too; the guest's fleet listed no mirror of the
  host's ship while the screenshot was taken (`mirrors=0`), so this was not seen.
To settle it: in the guest's game open its ship 'Bob9276' in the Sporepedia / the ship editor (a bare hull = not a bug);
fly both ships into one system and screenshot the host's ship on the guest's game.

## Fix
<!-- not fixed (not trivial / possibly not a bug) -->

## Verification
<!-- none yet -->

## History
- 2026-11-21: opened.
