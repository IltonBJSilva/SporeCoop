---
id: BUG-9304
title: Civ own nations: on the guest's own game its own capital's citizens are the host's species
status: open
kind: bug
stage: Civ
reported: 2026-11-21
source: QA campaign (lane B)
role: guest
versions: 0.7.15.96
fixed_in:
branch: fix/qa-campaign
commits:
tests: by hand on lane B, E:\Repositorio\SporeCoop-wt\qa\cit.py (not in the repo)
signature:
docs: docs/re/civ.md#civ-own-nations
related: BUG-9274, BUG-9294
---

## Report
QA campaign of the friends build 0.7.15.96 (build/amigos-97) on lane B: one new two-player game from Cell to Civ
(planet Calvanin; host creature 'Yurgh' 292B8132, purple; guest creature 'Glorbo' 7C8BEFA2, blue). In Civ the guest's
own capital (nation 0000262E, species key 7C8BEFA2 on both games) was founded 4 s after the host closed the Civ intro.
The host's game shows that capital's 19 citizens as the guest's blue species; the guest's OWN game shows the same
city's citizens as the host's purple species (screenshots qa/civ_cit.png: left the host, right the guest, the same
spot of the guest's capital). So the guest sees its own city peopled by its partner's creatures.

`coop.civ nations citizens` with each camera at the guest's capital:
* host: `citizens city_pol=0000262E species=7C8BEFA2 count=19`
* guest: `citizens city_pol=0000262E species=292B8132 count=19`; its `coop.civ nations`: `own nation 0000262E
  species=7C8BEFA2 cities=1`.

## Reproduce
1. A new game, the guest picks its own cell, play on to Civ (or any Civ save where the guest's nation was founded with
   the guest's own species).
2. The guest's camera at its own capital (BUG-9294 takes it there): `coop.civ nations citizens` on the guest lists the
   capital's citizens with the host's species; a close look shows the host's creature walking in the guest's city.

## Root cause
Not found yet. Ruled out at runtime:
* timing of the species key: the guest's game read the nation's +0x268 as 0 in the first seconds (the key comes with
  the world share 4.5 s later), but after the world came, after a full leave + rejoin, the citizens were still the
  host's species;
* the guest's nation being "the player's" on its game: with `coop.fix civ.nations_native off` on the guest and a
  rejoin, still the host's species.
Inferred (not checked at runtime): GetSpeciesProfile (0x4DF580, docs/re/creations.md 2.1) returns null when the key's
resource is not reachable through the game's resource manager, and SetSpeciesProfile (0xC22450) takes a null species as
the player species (docs/re/creature.md, the PS table: "a null species is PS", "citizen fallback"). On the host the
guest's creature 7C8BEFA2 is archived with the planet (creations::key_known before the founding); on the guest's own
game that key (the species of its Creature avatar; its library has 'Glorbo' as 292B8139) may not resolve in Civ, so its
citizens fall back to the save's player species, the host's. Next step: on the guest, GetSpeciesProfile(7C8BEFA2) and
KeyExists on that key in Civ; if null, make the key reachable (the guest's own creation shared into the world's
database, as the host does) before the capital's citizens are made. docs/re/civ.md#civ-own-nations says the Boheus run had the guest species on
both games, so it worked once; what differs (the guest's creature made by the new-game landfall, BUG-9280) is open.

## Fix
None yet.

## Verification
no-fail-run: open bug, no fix yet.

## History
- 2026-11-21: opened.
