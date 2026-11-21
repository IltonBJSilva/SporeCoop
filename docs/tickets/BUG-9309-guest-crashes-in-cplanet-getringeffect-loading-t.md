---
id: BUG-9309
title: Guest crashes in cPlanet::GetRingEffect loading the host's world after its session closed while it was in its own creature editor
status: fixed
kind: crash
stage: Menu
reported: 2026-11-21
source: lane B test (BUG-9304 campaign replay)
role: guest
versions: 0.7.15.96
fixed_in: unreleased
branch: fix/guest-leave
commits: bc7b7952
tests: editor_quit (coop_editor_quit.py, not run in the lab); manual lane C driver t9309.py
signature: 0xC0000005 at 0x01340A20 Simulator::cPlanet::GetRingEffect+0x90 (data 0x13C) < cSpaceGfx::InitializeInternal+0x9A5 < SporeCoop::GalaxyMenu_SimReset
docs: docs/re/space.md#space-editor-quit-menu, docs/archive/known-issues-2026.md#issue-guest-ringeffect-after-editor
related: BUG-0310, BUG-0284
switch: world.no_editor_quit
---

## Report
Lane B, 0.7.15.96 + fix/qa-campaign, the QA campaign drivers replayed on the Cell save Phivenova (dev3 / dev4). In
Creature the guest was in its own creature editor (the vote's editor, "own editor opened 66.3 s after the vote") when
the campaign driver's save step saved, made the guest leave (`coop.leave`) and took the host to the galaxy menu. The
guest's game quit its editor to the galaxy menu ("own editor result: ... cancelled 1"); 30 s later it joined again,
received the host's world and, 0.3 s into `LoadGameByName`, crashed:

`exception 0xC0000005 at 0x01340A20 Simulator::cPlanet::GetRingEffect+0x90 (data 0x0000013C)`, called from
`Simulator::cSpaceGfx::InitializeInternal+0x9A5 < SporeCoop::GalaxyMenu_SimReset+0x2D02`. The line before the load:
`world: the galaxy menu still holds the old world's player empire 800021CA: cleared it` (BUG-0284's reset). Crash
report kept as E:\Repositorio\SporeCoop-wt\civcit\crash-20261121-120351-i4.txt (lane B, not in the repo).

## Reproduce
Lane C (Windows, Steam build), E:\Repositorio\SporeCoop-wt\guestleave\t9309.py: Creature save Boheus (the Dopov galaxy
copy, dev5 host / dev6 guest), the guest's own editor (`coop.vote editor`), `coop.leave` on the guest, `coop.menu` on the
host, `coop.menu` on the guest while still in its editor, the host loads Boheus again and hosts, the guest joins: the
guest's load faults at 0xC70A20 every time with the quit allowed (2 of 2). Earlier steps of the one lane B run: Steps of the one run: a two-player Creature game, the guest in its own editor after a vote, the guest
leaves the session from the editor, the host goes to the galaxy menu, the guest's game leaves the editor to the menu,
the host loads the world again and hosts, the guest joins: the guest's load faults.

## Root cause
Observed (objdump of the Steam text dump, lane C runtime):
* The fault is in 0xC70A20 `cPlanet_GetPlanetType` (the function after GetRingEffect; thiscall, `[this+0x13C]`, no
  null check of this), called with a null planet by 0x10361D0, a cSpaceGfx set-up step that switches on
  GetCurrentContext: for context 0 (on a planet) it reads `Simulator_GetActivePlanet()`'s type. The guest's galaxy menu
  held context 0 and no active planet (`coop.status`: "menu context 0"; the host's menu: -1).
* The game's mode change 0xB336E0 runs the menu's reset `GalaxyMenu_SimReset` 0xB32C30 (player record clear, then
  `SetCurrentContext(-1)` 0x1021850, the stars read) only when the mode left is a simulator mode; Editor 0xDBDBA1 ->
  Galaxy menu skips it. BUG-0284's clear at our load nulled the player record and the active planet, not the context.
* The game never goes Editor -> Galaxy menu by itself: its Options menu in an in-stage editor greys out "Exit to Main
  Menu" and "Quit Spore". The quit came from `coop.menu` (the campaign driver: "if the guest is not in the menu,
  coop.menu"), i.e. coop's `post_quit_to_menu` posted the quit message 0x4715068 in the editor (lane B log: "leaving
  the stage 0x00DBDBA1 (quit to the galaxy menu posted)"). via_menu_tick posts it only in a stage, so no player path
  is known; the player's own leave while in an editor is safe (the game stays in the editor).
* Repairing the menu instead was tried and is not enough: the context reset to -1 alone, then the whole
  GalaxyMenu_SimReset called in that menu, both moved the fault to the next Creature load's HUD (0xC2FF9A, the global
  0x169E3E8 null; "SpeciesGoal_Refresh+0x3EA"): the creature stage's teardown is skipped too.

## Fix
`world::post_quit_to_menu` refuses in the editor mode ("leave the editor first"), as the game's own menu does
(switch `world.no_editor_quit`; off: posted in the editor as before). `coop.status` prints "editor quits refused N
(menu context C)". The campaign drivers must leave the editor first (red cross or check), then ask for the menu.

## Verification
Lane C, two instances (dev5 host, dev6 guest), driver t9309.py, logs in E:\Repositorio\SporeCoop-wt\guestleave:
* FAIL (switch off, `coop.fix world.no_editor_quit off` on the guest): t9309_off.log (14:46): the guest's quit from its
  editor went to the menu with context 0, the load faulted at 0xC90A20 (runtime base 0x420000) cPlanet_GetPlanetType
  data 0x13C, crash-20261121-144626-i6.txt; the same with the build before the fix (14:20, crash-20261121-142025-i6.txt).
* PASS (switch on): t9309_on.log (14:5x): "leave the editor first", the red cross back to Creature, the menu with
  context -1, the guest joined and plays its own creature (on_guest_joined.png), the host sees "Bob [Creature]"
  (on_host_joined.png), no crash report; second PASS t9309_on2.log.
* Lab test editor_quit (coop_editor_quit.py) added, not run (no Linux lane here).

## History
- 2026-11-21: opened (seen once on lane B while replaying the campaign for BUG-9304).
- 2026-11-21: reproduced on lane C (2 of 2 with the quit allowed), root cause, fixed on fix/guest-leave.
