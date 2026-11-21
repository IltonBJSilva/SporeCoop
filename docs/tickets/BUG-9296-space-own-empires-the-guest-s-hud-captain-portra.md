---
id: BUG-9296
title: Space own empires: the guest's HUD captain portrait still shows the host's captain (Brawgle) after its own captain key 7CA81684 is set
status: fixed
kind: bug
stage: Space
reported: 2026-11-21
source: agent smoke 0.7.15.96 lane A
role: guest
versions: 0.7.15.96
fixed_in: unreleased
branch: fix/space-captain
commits: 5fa28cf1, d5100ef6, fa93536d
tests: tools/tests/coop_space_own_panel.py (lab test, not run in the Linux lab yet); agentC6/t9296.py (lane C, by hand)
signature:
docs: docs/re/space.md#space-hud-planet-panel
related: BUG-9279, BUG-9297
switch: space.own_empire_panel
---

## Report
Smoke test of the friends' build 0.7.15.96 on lane A (Windows, Steam exe 3010a413, two instances), 2026-11-21: the
host loaded the Space save Dopov (dev1), hosted, the guest (dev2) joined and waited in the galaxy menu until the host's
ship left the home planet (`coop.space takeoff`), then came into Space. The guest's `coop.space empires`: `own=1 seat=1
money=100000 name='Bob's empire' ... ship=06664CDC captain=7CA81684 archetype=F966D429 (set 1)`, its log "empires: my
archetype F966D429 and its abilities set on my game", `coop.captain` on the guest: `captain=7CA81684/2B978C46/40626200`
(its own creature 'Guleskate'). But the guest's HUD (the captain portrait at the bottom left, beside the planet map)
still showed the host's captain "Brawgle E..." 60 s later, the same portrait as on the host's game
(screenshots r96_space_guest.png and r96_space_guest2.png in the lane A driver folder agentA5). The ship 06664CDC is the host's design: expected, dev2
has no own spaceship design (`mine ufo: 06664CDC '' (0 own)`).

To reproduce: the steps above (the lane A drivers agentA5/r96_space.py, then r96_space2.py), look at the
guest's HUD portrait.

## Root cause
CORRECTION (2026-11-21, fix/space-captain): the report's premise is wrong. The square beside the planet map is not the
captain's portrait: it is the Space HUD's planet panel (window 0x05D646C8), the picture of the active planet's
**citizen species** (SpaceGameUI_PlanetPortraitKey 0x1065F10: cPlanetRecord::GetCitizenSpeciesKey) with the planet
owner empire's name under it (0x044D94D1). On Dopov (the host's home planet) its citizens are the host's species, so
the picture is right; runtime: the picture was not the guest's captain png before or after a rebuild
(`coop.space empires hud`: picture_is_captain=0), with the guest's captain 292846CB set. The captain is shown by the
captain screen (the rank badge), which reads cEmpire +0xC4 whenever it opens. `coop.captain` `species=0679B8C8` is
the world player species (the host's), as designed.
The real defect behind the report: the panel is built only when the active planet changes
(SpaceGameUI_UpdatePlanetPanel 0x106A500); space_empires renames the guest's player empire to "Bob's empire" after
that, so the panel kept the save's name "Brawgle Empire" (the host's) next to the host's species, which read as "the
host's empire / captain". A rebuild (the next planet, or `coop.space empires hud refresh`) showed "Bob's empire".

## Fix
space_empires.cpp: right after the guest's own empire (name, colour, captain) is applied on its game the planet panel
is rebuilt (SpaceGameUI_UpdatePlanetPanel, outside the frame): it shows the guest's own empire name at once. Switch
`space.own_empire_panel` (off: the panel keeps the name it was built with, 0.7.15.96). The picture stays the planet's
citizens (the host's species on the host's planets). Not done: the host's colonies under the host's name on the
guest's game (they are the guest's player empire's there): see docs/re/space.md#space-hud-planet-panel.
Test seams: `coop.space empires hud [refresh]`, `coop.space empires captain <hex>`.

## Verification
Lane C (Windows, Steam exe 3010a413, instances 5/6), Dopov (a full copy of the dev1 profile's Game0), the host founds
the guest's seat with its own creature 292846CB (`coop.space empires found 1 292846CB`), driver agentC6/t9296.py:
- FAIL, `coop.fix space.own_empire_panel off` on the guest: its empire 'Bob's empire' with captain 292846CB, its HUD
  panel 'Brawgle Empire' (agentC6/t9296_off.log, t9296_off_guest.png).
- PASS, switch on: the guest's panel 'Bob's empire' at once (panel=1), the host's panel keeps 'Brawgle Empire'
  (agentC6/t9296_on.log, t9296_on_guest.png, t9296_on_host.png). No crash or fault report.

## History
- 2026-11-21: opened.
- 2026-11-21: CORRECTION of the premise (the planet panel, not the captain); fixed on fix/space-captain (the panel is rebuilt after the own empire is applied).
