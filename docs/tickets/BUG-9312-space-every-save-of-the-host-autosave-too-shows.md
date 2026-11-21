---
id: BUG-9312
title: Space: every save of the host (autosave too) shows "Your alliance with the <partner>s empire has been severed" (the allies mirror undone before the save)
status: open
kind: bug
stage: Space
reported: 2026-11-21
source: in-game test of feat/space-guest-home, lane A
role: host
versions: 0.7.15.97
fixed_in:
branch: feat/space-guest-home
commits: 7eef1f1b
tests: E:\Repositorio\SporeCoop-wt\sgh\t3.py on lane A (not in the repo); tools/tests/coop_space_guest_home.py check 5 (lab, not run: no Linux lab here)
signature: BreakAlliance 0xD01E70 called from 0x101341C (Empire_AttitudeCheck) with the allies mirror's pol, rel 0.00
docs: docs/re/space.md#space-mirror-ally-break
related: BUG-9310, BUG-9315
---

## Report
In-game test of BUG-9310 (lane A, Steam exe, dev1's Pradnie in Space, the guest's new colony on its homeworld): right
after the host's autosave "a colony founded" (sporekit.log 13:52:55 `autosave: the host's game saves (a colony
founded)`), the host's HUD showed the event line "Your alliance with the Bob's empire has been severed."
(E:\Repositorio\SporeCoop-wt\sgh\on_host_homesystem.png).
How to reproduce: a guest with an empire of its own the host mirrors (space_allies.cpp); the host flies into a star
system of that empire (the tests flew to the guest's homeworld before saving); the alliance breaks at once, and again
after each save (the mirror's re-apply).

## Root cause
The first guess (the save-time undo posts the message) was wrong: `cEmpire_RemoveAlly` 0xC32990 is a plain vector
erase. Observed (objdump of the unpacked Steam exe + a diagnostic hook, lane A):
* The message 0x4445D44 is posted only by `BreakAlliance` 0xD01E70. Its caller was 0x101341C in the game's attitude check
  0x1013370 (`SporeCoop::Empire_AttitudeCheck`): an allied empire whose relation (CalculateRelationship) is below a
  tuning gets its alliance broken. Log: `BreakAlliance from 0x101341C a=<mirror pol> b=<player pol> rel=0.00 flags=2`.
* The allies mirror's relation is 0.00 (the mod sets the alliance flags and vectors, no relation events), below the
  tuning. The check runs (every frame) for the empires near the player: with the host in a star system of the mirror
  the alliance broke at once; each save's re-apply made it again and the next check broke it again (the "every save").
  With the host elsewhere the check never ran for the mirror (0 breaks in 3 saves), which is why not every save showed
  it. A broken mirror stayed unallied (the game's flags 0) until the next re-apply.

## Fix
space_diplomacy.cpp, switch `diplomacy.mirror_ally` (`coop.fix diplomacy.mirror_ally off`: the game's calls run, as
0.7.15.97): a hook on Empire_AttitudeCheck returns at once for an allies mirror (spaceext::allies_mirror_pol), and the
BreakAlliance hook drops a break between the player's empire and a mirror from any other caller (logged, rate limited).
The players' alliance is the mod's (allies set_relation; spaceext::relation in rivals mode). The save-time undo is
unchanged: during the save the game's flags of the pair are 0 (seen at 2-4 s after `coop.save`), so the save holds no
alliance with the mirror. New `coop.space diplomacy rel <pol>`: the game's flags and relation of (pol, my empire) and
the counts.

## Verification
Lane A, E:\Repositorio\SporeCoop-wt\sgh\t3.py (dev1 Pradnie host, dev2 joins, both restored from sgh\bak; the host
jumps into a star of the guest's mirror empire, then saves twice; checks the game's flags of the pair):
* OFF (`coop.fix diplomacy.mirror_ally off`): FAIL, sgh\t3_off.out: in the mirror's system flags 0 (breaks=1) before
  any save, after each save flags 0 again (breaks 2, 3), log 3 x "the game breaks the alliance"; screenshot
  sgh\b9312_off_host_after_save.png: the red warning icon of an event at the left, the neutral face.
* ON: PASS, sgh\t3_on_PASS.out: flags 2 in the mirror's system and after both saves, breaks=0, attitude_skipped=1466;
  screenshot sgh\b9312_on_host_after_save.png: no warning icon. No crash report in either run.
* A first version that dropped only the BreakAlliance call passed too but the check ran every frame (1254 dropped
  breaks in about a minute): the attitude check hook replaced it as the main guard.

## History
- 2026-11-21: opened.
- 2026-11-21: root cause (the game's attitude check, not the save's undo), fixed on feat/space-guest-home.
