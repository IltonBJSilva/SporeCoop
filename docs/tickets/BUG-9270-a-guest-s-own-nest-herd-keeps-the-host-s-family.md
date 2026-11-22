---
id: BUG-9270
title: A guest's own nest herd keeps the host's family copies in its member list after the guest joins it
status: fixed
kind: bug
stage: Creature
reported: 2026-11-20
source: Lineages port test (lane A)
role: 
versions: 
fixed_in: unreleased
branch: fix/evolve-nest
commits: be72cff6
tests: E:/Repositorio/SporeCoop-wt/evnest/t9316.py (two games, lane B)
signature:
docs: docs/re/creature.md#creature-herd-member-list
related: BUG-9262, BUG-9316
---

## Report
Lane A 2026-11-20: on the guest's game, its own nest herd (`coop.nest herd <home>`) lists 9 members: the guest, its 4
family copies and 4 copies of the host's family (species 065CBE70, standing at the host's nest 60 m away).

## Root cause
Observed: own_family_tick moves those 4 copies to their species' nest herd (`#18 (another species) leaves our own nest
herd ...`, Creature_SetHerd) and `coop.npc list` then shows them in that herd (herd=281D29A8), yet the guest's own herd's
member list (+0x40) still names them. Inferred: CreatureGame_JoinNestHerd (the claim) carries the avatar's old herd
members into the new herd's list, and Creature_SetHerd removes a creature only from the herd its +0x1674 names.
Effect not seen: the game's family logic reads +0x40 (mates, greetings); no wrong behaviour was observed in the test.

## Fix
0xC6B5E0 (read from the running game, 2026-11-21) only lets go of the herd's mom / egg layer: Creature_SetHerd never
touches a member list. The copies came into the guest's avatar herd while it played the host's species; own_family_tick
moved them out with SetHerd only. Effect found: BUG-9316 (a destroyed stale member crashed the fire cinematic). Fixed
with it: `creature.herd_list_stale` (the move takes it out of the old list with HerdList_Remove 0xACC940; the avatar
herd's list is purged of members of other herds every frame).

## Verification
BUG-9316's run: OFF 4 STALE members in the guest's own nest herd (evnest/t_off.out), ON 0 (evnest/t_on.out).

## History
- 2026-11-20: opened (found in the Lineages port test, lane A).
- 2026-11-21: cause read in the asm, fixed with BUG-9316 on fix/evolve-nest.
