---
id: BUG-9316
title: Creature: a guest that chose its own creature mid-session crashed in Herd_NestPosition (null nest) in the cinematic after its end-of-stage editor
status: fixed
kind: crash
stage: Creature
reported: 2026-11-21
source: 0.7.15.98 smoke
role: guest
versions: 0.7.15.98
fixed_in: unreleased
branch: fix/evolve-nest
commits: be72cff6
tests: E:/Repositorio/SporeCoop-wt/evnest/t9316.py (two games, lane B)
signature: dc04aee5:0x016DAE60 read 0x160 (Herd_NestPosition, eax=ecx=0) < CineScript_Resume (stack scan), guest, mode Creature right after Editor -> Creature at the stage end
docs: docs/re/creature.md#creature-herd-member-list
related: BUG-0018, BUG-9270, BUG-9262, BUG-9309, BUG-9304
---

## Report
0.7.15.98 smoke on lane B (two games, the Creature save Marona of the dev1 galaxy copy; the guest profile is new to that
galaxy, so it first played the host's species 065CBE70). The guest then chose its own creature with
`coop.creations choose creature 292B971E` (it became 7CB0FA33 in this world, swaps=2). Both brains were filled
(`coop.dna gain`), the host's evolve button started the vote, both opened their end-of-Creature editors. 1.5 s after the
guest left its editor (stage: Editor -> Creature, its "own editor result" logged, the host's UFO and puppet shown) the
guest's game crashed: 0xC0000005 at SporeCoop::Herd_NestPosition, read of 0x160 (a null herd / nest), the stack scan
in cinematic code (CineScript_Resume, cCinematicManager_SetupStart / SetActor / HandleMessage): the evolve cinematic
asked for its actor's herd's nest. The host was unaffected. Seen once; driver civcit\t9304m2.py (r98 folder log
t9304m2_on.out, crash files in r98\crash2: crash-20261121-150729-i4.txt / .dmp / report zip).

## Root cause
A herd's member list (+0x40..+0x44) is a vector of intrusive references kept by the game's own adds and removes
(cCreatureAnimal::Create, CreatureGame_JoinNestHerd, the destroy 0xC083D0 -> `HerdList_Remove` 0xACC940 on the herd its
+0x1674 names). `Creature_SetHerd` 0xC04700 only changes +0x1674 (and the old herd's mom / egg layer); it never touches a
list (asm read from the running game: docs/re/creature.md#creature-herd-member-list).
While the guest plays the host's species, the copies of the host's family are made in its avatar's herd. After it chose
its own creature and claimed its own nest, own_family_tick (BUG-9262) moved those copies to their species' nest herd
with SetHerd only: they stayed in the guest's own nest herd's list (its avatar herd), marked STALE by `coop.nest herd`
(4 of 9 members in the OFF run). The copies were destroyed later (the editor round trip) from no list, and the stale
entries' references kept them alive with +0x1674 = 0. Back from the end-of-Creature editor the fire cinematic 2677CD86
casts every member of the avatar herd's list as an actor placed relative to its herd's nest: Herd_NestPosition(null),
read of 0x160. In the OFF run's crash ebx = 36B8C778, one of the four STALE members listed before the vote.
Not needed: the mid-session choice itself; any guest whose avatar's herd list got a copy that the mod moved away.

## Fix
`game::move_to_herd` (game.cpp): the mod's herd moves (own_family_tick, both directions) take the creature out of its old
herd's list with the game's `HerdList_Remove` (its reference released as the game does) before SetHerd; and every frame
of the Creature stage `game::herd_list_purge` drops from the avatar herd's list each member whose +0x1674 is another
herd or none (logged "left the member list (BUG-9316)"). `coop.nest herd` marks STALE members.
Switch: `coop.fix creature.herd_list_stale off` (SetHerd only, no purge: 0.7.15.98).

## Verification
Driver E:/Repositorio/SporeCoop-wt/evnest/t9316.py (lane B, Marona of the dev1 galaxy copy, dev4's worlds.ini reset so
the guest is new to the world: it plays the host's species 065CBE70, chooses its own creature 292B971E (7CB0FA33 here),
claims its own nest, both brains full, the vote, both end-of-Creature editors, back to Creature):
* OFF (`creature.herd_list_stale off` on both): guest avatar herd 9 members, 4 STALE (herd 364A8700); the guest crashed
  2 s after its editor: crash-20261121-213045-i4 (0xC0000005 Herd_NestPosition read 0x160, cinematic 2677CD86 playing,
  ebx = the STALE member 36B8C778): evnest/t_off.out, evnest/crash_off/. RESULT FAIL.
* ON: the same four copies left the nest herd (log "leaves our own nest herd"), 0 STALE, no crash report, the fire
  cinematic 2677CD86 played on both games (screenshots evnest/on_cine{1,2}_{host,guest}.png: the letterboxed cinematic at
  each player's own nest), both back in Creature: evnest/t_on.out RESULT PASS.
* Also ON with a guest that played its own creature from the join (evnest/t_on_ownfromstart.out): no crash, 2677CD86 on
  both.
No lab run id: lane B is a Windows lane outside tools/lab.py.

## History
- 2026-11-21: opened.
- 2026-11-21: root cause (stale herd list entries, BUG-9270), fixed on fix/evolve-nest (creature.herd_list_stale), OFF crash / ON pass on lane B.
