---
id: BUG-9289
title: Creature: a guest's brain-upgrade cinematic that starts while the host's pause holds its game stays black (no caption); its own progress stops reaching the host until Esc
status: open
kind: bug
stage: Creature
reported: 2026-11-20
source: integration campaign QA lane B (test/integration)
role: guest
versions: 0.7.15.94
fixed_in:
branch:
commits:
tests:
signature: cinematic F6A4B1E4 (brain upgrade)
docs:
related: BUG-1142, BUG-9265
---

## Report
Integration campaign QA (lane B, test/integration = build/amigos 0.7.15.94 + the feature branches, 2026-11-20). In the
Creature stage the host opened History and then the game's Options menu (Esc), which pauses the host's game; the guest's
game waits for it (host busy, BUG-1142: "Player paused the game: yours waits"). While it waited the guest's brain grew
(own progress, `coop.dna gain 3000`). The guest's game started the brain-upgrade cinematic (id F6A4B1E4) and its
screen stayed black: no caption, no check to click, Enter and the caption's check did nothing. `coop.status`:
`cinematic F6A4B1E4 playing`, `busy: cinematic,input_lock,paused`; `coop.cine`: state=2. It stayed so for more than
8 minutes, also after the host resumed ("this game goes on") and after `coop.cine off`. Esc on the guest ended it
(the game's skip) and the guest played on.
While it lasted the guest's own progress never reached the host: own_progress.cpp guest_tick waits while
world::cinematic_playing(), so no report was sent; the host's record of the guest stayed at its values from before the
gain (`player 1 Bob: full=0 pct=0`, record dna=175 bar=60) and the host's evolve button only said "waiting for Bob".

## Reproduce
1. Two games in the host's Creature world, own progress on (the default).
2. Host: Esc (the Options menu) and leave it open.
3. Guest: `coop.dna gain 3000` (its brain grows several levels).
4. Host: close the menu. The guest's screen stays black under the cinematic; `coop.progress` on the host keeps the guest
   at 0 %. Esc on the guest ends it.

## Root cause
Not analysed yet. Observed: the cinematic starts while the mod holds the guest's game for the host's pause (time
paused); its caption never shows. Inferred: the brain cinematic's caption step needs the game time the hold stopped,
and the game's own pause under the caption ("paused" stays in busy) is then never lifted.

## Fix
None yet.

## Verification
no-fail-run: open bug, no fix yet.

## History
- 2026-11-20: opened.
