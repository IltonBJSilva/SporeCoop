---
id: BUG-0870
title: Feature idea: difficulty scaling by party size (stage progress, enemy health and damage, hunger, takeover, raids)
status: fixed
kind: feature
stage: All
reported: 2026-11-10
source: fork PandizeLive/SporeCoop-Lineages feat/lineages-43 lineages36/41-dev, BUG-9100 (founders.h CampaignRules)
role: 
versions: 
fixed_in: unreleased
branch: feat/difficulty-scaling
commits:
tests: tools/tests/coop_difficulty.py (suite difficulty, twin difficulty_off); validate_tests test_difficulty; manual two-instance runs on lane D (Verification)
signature:
docs: docs/re/creature.md#creature-difficulty-scaling
related: BUG-9265 (own progress), BUG-0510 (single-player pace), BUG-9292 (no extra spawns)
---

## Report
With four players the stages go by too fast and enemies are trivial. The fork scales the campaign by the number of
seats, with one slider (0..400 %).

## Root cause
Not a bug. We have `dna_pace` (single-player pace or every gain whole) only. Note: plan-0.7.12 F3 says "Not
proposed: anything that changes the game's own difficulty"; this needs the owner's decision.

## Fix idea
From the fork (PandizeLive/SporeCoop-Lineages lineages36/41-dev, BUG-9100, mods/coop/founders.h CampaignRules, world.cpp MSG_CAMPAIGN_RULES); NOT FINAL: taken from the fork, verify on our code and against our rules before using.
* Rules stored with the campaign (file FST7, 24 bytes; FST1..6 read as the old model) and sent in
  MSG_CAMPAIGN_RULES: seats (2..4), difficulty_percent, per-seat extras: enemy health +15 %, damage +5 %, hunger
  +2.5 % (basis points 250), city takeover +5 %, Space raid +1 raider, progress +60 % per extra seat (model 1).
* `extra_total(per_seat) = round((seats-1) * per_seat * difficulty / 100)`, rounded once over all seats; workload
  (stage progress, Cell food per DNA, rival targets, Civ military interval and fleet cap) = 100 + extra.
* Cell: from 3 seats one extra predator per native predator group, spawned at touching distance of the true
  collision sphere (model radius +0x1244 x scale +0x58) plus a gap, and plant food +25 % then (BUG-9109, 9110).
* Applies to new campaigns only; `coop.rules` prints the values. The fork's Civ / Tribe parts rest on its
  "reserved founders" (separate villages / nations), which we do not have.
* Overlap: plan-0.7.12 F3 (session settings) is the natural home; Rivals 0.8 needs its own balance.

## Fix
Ported by hand from Lineages (PR #196, founders::CampaignRules model 1) as the session setting `all.difficulty` (All
stages, Live, default off; off / 50 / 100 / 150 / 200 / 300 / 400 %; settings_core.h). The rules: difficulty_logic.h
(portable, tests/validate_tests.cpp); the hooks: difficulty.{h,cpp}. The players are the session's players now (not
reserved seats: we have no campaign seats), nothing is saved, nothing new on the wire (the setting goes by name in
MSG_SETTINGS). Where it acts:
* Creature (host, npc.cpp damage_filter / MSG_DAMAGE_REQ): a wild animal's hits on any player x enemy damage %; the
  players' hits on a wild animal / enemy health % (the enemy's health as hits, no bigger max health: nothing to undo
  before a save); every machine: its own avatar's hunger drain x hunger factor (Creature_SetHunger hooked, only the
  call from Creature_HungerUpdate, SporeCoop::CreatureHungerUpdateReturn).
* Civilization (host, h_AddCapture): a player's capture damage / takeover %.
* Space (h_TakeDamage): an NPC's hit on the own ship x enemy damage % (a guest's MSG_UFO_DAMAGE through the same
  hook, the host does not scale what it forwards).
* Stage progress: only with all.own_progress and all.dna_pace off (one total, every gain whole): gains / workload %.
  With own progress or the single-player pace each player already has a solo game's pace: no second scaling.
Not ported (on purpose): extra Cell predators and Space raiders (creature counts stay a single player's, BUG-9292),
Tribe (Lineages' needs its founder villages), Cell enemy health / damage (byte health, its MSG_CELL_COMBAT), Civ buy
price, military interval and fleet cap, rival targets. `coop.fix all.difficulty off`: the game's own values.
`coop.difficulty` prints the values in force and the scaled counts. Owner decision note: plan-0.7.12 F3 did not
propose changing the game's difficulty; it stays off unless the host turns it on.

## Verification
- make test: validate_tests test_difficulty (off, alone, 2 at 100 %, 3 at 50 %, 4 at 400 %, the 400 % cap, progress
  never on top of own progress or the pace, hunger only when the meter falls).
- Lane D (Windows, Steam build, two instances, 2026-11-21), the game's own numbers on both games:
  - Creature (Etheis): off: hits of 3 took 3.00 of a wild animal (host's and guest's hits; the guest's proxy the same),
    bites of 3 took 3.00 of each player's own creature; at 400 %: 1.88 and 3.60. Twin (`coop.fix all.difficulty off`,
    400 %): 3.00 everywhere: FAIL as it should.
  - Hunger: the hook saw the game's drops x 1.0995 (15.17 -> 16.68 host, 14.41 -> 15.85 guest) at 400 %.
  - Civ (Kealeneon): `coop.civ siege` x3: the city's meter 0 -> 3.00 off, 0 -> 2.50 at 400 % (x 1.2), the guest saw it.
  - Space (Dopov): an NPC ship's hit of 100 on the guest's ship: 100 off, 120 at 400 %, the guest's HUD 1,060.
  - Cell (Socle, own progress and pace off): meals of 34 points: +34 off, +10 at 400 % on both games.
  - The settings page: the row under All stages, a click cycles the value, the guest sees it and gets a notice.
- tools/tests/coop_difficulty.py (+ twin) written for the lab; not run in the lab on this PC (no Linux lanes here).
no-fail-run: a feature (default off); the twin run above (fix off) failed as it should on lane D.

## How to test
Lab, 2 instances: host a new campaign with 4 seats, 100 %: an NPC's max HP is 145 % of solo, its hit 115 %, the
Creature hunger drain 107.5 %; 0 % gives the solo values; an old save keeps its numbers.

## History
- 2026-11-10: opened from the fork review for the 0.7.13 plan (feature idea, fork evidence only).
- 2026-11-21: ported on feat/difficulty-scaling (default off), tested on lane D.
