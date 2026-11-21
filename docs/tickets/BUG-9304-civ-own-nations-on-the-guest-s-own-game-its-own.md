---
id: BUG-9304
title: Civ own nations: on the guest's own game its own capital's citizens are the host's species
status: fixed
kind: bug
stage: Civ
reported: 2026-11-21
source: QA campaign (lane B)
role: guest
versions: 0.7.15.96
fixed_in:
branch: fix/civ-citizens
commits:
tests: by hand on lane B, E:\Repositorio\SporeCoop-wt\civcit\t9304.py and t9304_2.py (not in the repo; the QA campaign drivers on the Creature save Mirey)
signature:
docs: docs/re/civ.md#civ-own-nations, docs/re/tribe.md#tribe-outfit-own-species
related: BUG-9274, BUG-9294, BUG-9262, BUG-1156
switch: tribe.outfit_own_species
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
Found at runtime (lane B, 2026-11-21, fix/civ-citizens). CORRECTION (2026-11-21, fix/civ-citizens): the first
inference ("the key does not resolve, SetSpeciesProfile falls back to the player species", marked superseded below) is
wrong: on the guest's game the
nation's key resolved (`coop.civ nations species`: nation 000029B3 key 7CB0FA33 profile 285207E0 known=1, and the 19
citizens of its capital had exactly that profile 285207E0), but that profile object held the HOST's species: its own key
+0x504 was 292B9714 (the host's) and its model +0x510 7D68D6FC (the guest's local key of the host's creation)
(`mem.read 0x28520CE4`, `coop.creations speciesof 7CB0FA33`: "key 292B9714"). The same object was the guest's own
Creature avatar's profile earlier in the process.

The chain (each step seen in the guest's log or memory):
1. Creature: BUG-9262 (creature.own_player_species) makes the guest game's player species (cSpeciesManager +0xA4) its
   own creature K, so its nest and family are its own.
2. The host moves on to Tribe; when the guest's game makes the stage change itself (`stage: Creature -> Tribe` on the
   guest, after its tribal-entry caption was confirmed), the game's new player tribe takes the player species, i.e.
   the species profile of K (cTribe +0x18CC).
3. 0.1 s later the outfit sync of BUG-1156 (designs.cpp outfit_sync / apply_outfit) sees that the player tribe's
   profile key (K) is not the host's tribe species and reloads that profile in place with the host's species
   (SporeCoop::SpeciesProfile_Load + +0x504 = the host's key): `designs: the tribe wears the host's outfit 292B9714
   (here 7D68D6FC)`. The profile object of K now IS the host's species.
4. cSpeciesManager keeps one profile per key for the whole process, so every later lookup of K on the guest's game
   gives the host's species: its own Tribe village, and the Civ capital founded with species K (nation +0x268 = K):
   the city's citizens are built from that profile and carry its key 292B9714. A rejoin or a world reload does not help
   (the cache lives with the process); a fresh guest process loading the same Civ save was right (seen 3 times), which
   is why the earlier rejoin checks still showed it and a cold load never did.
The QA run's log has the same line (10:15:48 "the tribe wears the host's outfit 292B8132 (here 7D8BE144)" 0.1 s after the
guest's own "stage: Creature -> Tribe"). When the guest's game does not change stage itself (it waits and loads the
host's Tribe world instead), the player tribe is the host's species and nothing goes wrong (one OFF run, not reproduced).

Ruled out at runtime before (QA campaign, still true): the timing of the species key (still wrong after a full leave +
rejoin) and the guest's nation being the player's on its game (still wrong with civ.nations_native off and a rejoin).
~ superseded (2026-11-21): the first inference, "GetSpeciesProfile returns null for the guest's key on its own game and
SetSpeciesProfile's null fallback gives the player species", see the CORRECTION above: the key resolved, its profile
was the host's species.

## Fix
designs.cpp: the outfit is never loaded into the species profile of a player's own creature (the profile object of a
session player's creature key, `creations::player_species`, or a profile whose key +0x504 is such a key); outfit_sync
returns "aligned" for it, apply_outfit checks again. The host's tribe (after the world reload) still gets the host's
outfit as before. Switch `coop.fix tribe.outfit_own_species off` (on by default): the 0.7.15.96 behaviour.
`coop.designs` shows `own_kept=` (the outfits not loaded for that reason). Test helpers on this branch: `coop.civ nations
species` (each nation's and city's species key, the profile the game gives for it, and the citizens' profiles) and
`coop.civ nations rekey <seat>` (host, tests: a seat's nation takes its player's creature now).

## Verification
Lane B, two fresh games each run (the profile cache is per process), the Creature save Mirey (dev3, brain full; the
guest plays its own creature 7CCDE692), driver E:\Repositorio\SporeCoop-wt\civcit\t9304.py (OFF) / t9304_2.py (ON):
evolve to Tribe through the game (vote, editors, the guest's own Tribe entry), Tribe -> Civ, the guest's capital.
* OFF (`coop.fix tribe.outfit_own_species off` on the guest): FAIL. In Tribe `coop.creations speciesof 7CCDE692` on the
  guest: "key 292AFAF6" (the host's); in Civ the guest's capital 00002AF9 (species 7CCDE692 on both games): guest
  `citizens city_pol=00002AF9 species=292AFAF6 count=19`. Output civcit\t_off.out. The first reproduction (Phivenova
  campaign replay, the same build without the switch): civcit\off4_g.png (the guest: the host's red creature in its
  own capital) / off4_h.png (the host: the guest's yellow creature at the same spot).
* ON (the default): PASS. In Tribe the same speciesof gave "key 7CCDE692" (the outfit not loaded into it: "the player
  tribe's profile is the species of a player's own creature 7CCDE692 ... (BUG-9304)", then after the world reload "the
  tribe wears the host's outfit 292AFAF6 (here 7DD5C761)" on the host's tribe as before); in Civ the guest's capital
  00002BC2: host `citizens city_pol=00002BC2 species=7CCDE692 count=19`, guest the same 7CCDE692 (the guest's first
  world after the founding could not be written, "disk full" on C:, so its city had blank keys and the player species
  until a leave and rejoin; the profile stayed its own: the OFF bug survives a rejoin, the ON result after it is the
  fix's). Output civcit\t_on.out.

## History
- 2026-11-21: opened.
- 2026-11-21: root cause found and fixed on fix/civ-citizens (tribe.outfit_own_species).
