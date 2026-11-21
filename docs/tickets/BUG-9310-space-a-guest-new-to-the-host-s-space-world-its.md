---
id: BUG-9310
title: Space: a guest new to the host's Space world (its party seat has no species) gets no own empire and no late-launch offer
status: fixed
kind: bug
stage: Space
reported: 2026-11-21
source: smoke of build/amigos-97 on lane C
role: guest
versions: 0.7.15.97
fixed_in: unreleased
branch: feat/space-guest-home
commits: 29e9a1c7 1bda97d8
tests: space_guest_home
signature:
docs: docs/re/space.md#space-guest-home
related: BUG-9279, BUG-9293, BUG-9296, BUG-9299, BUG-9312
---

## Report
Smoke of the friends build 0.7.15.97 on lane C (Windows, Steam exe, instances 5/6). The host (dev5) loads Dopov (a
full copy of the dev1 profile's Game0, whose party has seats for lane A's players) and takes off; the guest dev6, whose
identity 47AF3B5C has never played this world, joins late. The host's party gives it a new seat 3 with
species=00000000; `coop.space empires` on the guest stays `guest own=0 seat=-1` with the host's name 'Brawgle Empire'
and captain, and `coop.space empires launch` stays `phase=idle offers=0` (no late-launch offer, BUG-9299) for 90 s.
With `coop.space empires found <gid> 292846CB` on the host (the test seam agentC6 used) the guest gets 'Bob's empire'
and the offer, and the whole late launch passes (r97/late.out).
How to reproduce: E:\Repositorio\SporeCoop-wt
97\late.py without the `found` step (log r97/late_nofound.out).
Question for the owner: should a friend who joins a Space save for the first time get its own empire (with a creature
of its choice, like the "Your own start" page of a new game), or fly with the host's as now?

Owner's answer (2026-11-21): such a guest gets its OWN empire in Space, as if it had evolved, with its own homeworld
near the host's (not the host's system, a few systems away).

## Root cause
Observed (runtime, lane A, dev1's Pradnie: the guest's seat 1 with species 0 and a start-any record naming creature
292846CB): party.cpp takes a guest's species from its shared creature (creations::player_key), and a guest in the host's
Space world shares none until it chooses a creature for that world (creations::choose, recorded per world). Its
start-any record of the planet named a creature, but nothing made the guest play it again; with species 0
space_empires never founds an empire (wants_empire), so no own money, no own captain and no late-launch offer.
`coop.creations choose creature 292846CB` on the guest: species 7CA81684 within a second, empire founded, offer shown.

## Fix
* start_any.cpp: in the host's Space world, a seated guest whose seat has no species is asked with the "Your own start"
  page when it has no record of the planet, and told to play the creature of its record (MSG_START_ANY kind 4 kUse)
  when it has one ("keep" in the record: nothing, it flies the host's empire).
* space_empires.cpp: every seat with an empire gets a homeworld the host picks (space_empires_logic.h pick_home_star:
  the first free star system from the N-th nearest to the host's home star on, N from the session setting
  `space.guest_home`, default 5; a rocky planet without life above creatures, the best T type), kept in
  `<planet>.spo.duoemphome`, sent after every K_EMPIRE (MSG_SPACE kind 106 K_HOME, space family 1.6). The guest's game
  makes it a colony of its own empire (spaceext::colonize_own) with its species as the star's citizens
  (SporeCoop::cStarRecord_SetSpeciesKey, put back before every save of its game). The guest's Space page: "Your
  homeworld" with "Fly there".
* space_allies.cpp: a partner's mirror empire with an empire of its own gets the partner's species (its colonies'
  citizens on the host's game), switch `allies.partner_species`.
* Switch `coop.fix space.guest_home off` (and the setting off): no question, no homeworld: the 0.7.15.97 behaviour.

## Verification
In game, lane A (Steam exe, instances 1/2, dev1 host / dev2 guest, Pradnie restored from a backup before every run),
driver E:\Repositorio\SporeCoop-wt\sgh\t2.py (the lab test tools/tests/coop_space_guest_home.py is written, not run
in the lab: no lab on this PC).
* ON: E:\Repositorio\SporeCoop-wt\sgh\t2_on_PASS.out: RESULT PASS. The guest's own empire (seat species 7CA81684),
  its homeworld 0059901E 3.12 units from the host's home (other runs 2.10, 2.24, 2.57), a colony of its empire on its
  game with citizens 7CA81684 = its captain; on the host the star is "Bob's empire"'s (mirror pol) with citizens
  7CA81684; the HUD planet panel shows the guest's species on both games (picture_is_captain=1; screenshots
  sgh\on_guest_homesystem_pass.png, sgh\on_host_homesystem_pass.png); the late-launch offer; host save, both leave,
  the host loads the save again, the guest joins again: the same homeworld from the file (no new pick), the guest's
  colony again, the host's mirror again with its species. No crash report.
no-fail-run: no lab on this PC; the FAIL / PASS pair is the in-game runs on lane A below (driver sgh\t2.py, the lab
test space_guest_home is its port, not run in the lab yet).
* OFF (`coop.fix space.guest_home off` on both): E:\Repositorio\SporeCoop-wt\sgh\t2_off.out: RESULT FAIL (the guest
  flies the host's empire, no homeworld), as 0.7.15.97.

## History
- 2026-11-21: opened.
- 2026-11-21: fixed on feat/space-guest-home (owner decision: own empire and homeworld).
