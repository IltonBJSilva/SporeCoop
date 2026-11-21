---
id: BUG-9316
title: Creature: a guest that chose its own creature mid-session crashed in Herd_NestPosition (null nest) in the cinematic after its end-of-stage editor
status: open
kind: crash
stage: Creature
reported: 2026-11-21
source: 0.7.15.98 smoke
role: guest
versions: 0.7.15.98
fixed_in:
branch: build/amigos-98
commits:
tests:
signature: dc04aee5:0x016DAE60 read 0x160 (Herd_NestPosition, eax=ecx=0) < CineScript_Resume (stack scan), guest, mode Creature right after Editor -> Creature at the stage end
docs:
related: BUG-0018, BUG-9309, BUG-9304
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
Not analysed yet. Inferred: the chosen creature's avatar (a swap in the middle of the Creature stage, in a galaxy where
this guest never had a nest of its own) has a herd without a nest, and the game's evolve cinematic reads that nest
(BUG-0018 had the same address for a destroyed creature's herd after a migration). Next: the dump, the guest avatar's
herd (+nest) after `coop.creations choose creature` (coop.nest home said nest d=4 m just before), and whether it also
happens without the mid-session choose (a guest that started the world with its own creature).

## Fix
<!-- what changed and why; the switch to turn it off for tests (coop.fix <name>) -->

## Verification
<!-- the test: FAIL without the fix, PASS with it, each with its lab run id (out/lab/YYYYMMDD-HHMMSS);
     or a line `no-fail-run: <reason>` (docs only, regression only, not reproducible) -->

## History
- 2026-11-21: opened.
