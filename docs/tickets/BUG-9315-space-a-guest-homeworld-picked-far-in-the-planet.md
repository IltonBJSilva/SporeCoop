---
id: BUG-9315
title: Space: a guest homeworld made a colony when its star's planets were first generated turns back into a plain planet in the guest's game, so the host's allies mirror never shows it
status: fixed
kind: bug
stage: Space
reported: 2026-11-21
source: in-game test of feat/space-guest-home, lane A
role: host
versions: 0.7.15.97, 0.7.15.98
fixed_in: unreleased
branch: fix/guest-home-mirror
commits: 9d965336
tests: tools/tests/coop_space_guest_home.py check 6 (twin SPORE_FIX_OFF=space.guest_home_keep); lane A driver E:\Repositorio\SporeCoop-wt\gm\t5.py (gm\t5_off.out FAIL, gm\t5_on.out PASS)
signature:
docs: docs/re/space.md#space-guest-home-lost
related: BUG-9310, BUG-9312
---

## Report
In-game test of BUG-9312 (lane A, Steam exe, dev1's Pradnie restored from the same backup as the passing runs, the guest
Bob new to the save): the host picked the guest's homeworld 0559A013 (star 5873683 'Satis-3', `dist=3.39`) instead of
one of the 0059xxxx planets of the earlier runs. The guest's game made it its colony (`coop.space empires home` on the
guest: owner=800024B3 mine=1 citizens=7CA81684), but on the host `coop.space empires home` kept `owner=FFFFFFFF
citizens=00000000` for 120 s and after a save, a reload and a rejoin; `coop.space allies list` showed the mirror with
6 stars / 6 planets, all 0059xxxx (the older colonies of the guest's roster), not 0559A013 nor its star.
How to reproduce: E:\Repositorio\SporeCoop-wt\sgh\t3.py setup (not every run: the pick differs per run).

## Root cause
(2026-11-21, fix/guest-home-mirror) On the GUEST, not the host: `home_apply_task` (space_empires.cpp) makes the
homeworld a record-level colony with `spaceext::colonize_own`, whose `planet_record` generates the never visited star's
planets (`RequirePlanetsForStar`, log "planets of star 0059A013 generated (seeded ...)") in the same tick. Moments later
the planet record was a plain planet again: tech 1, the same record (the same name), while the star stayed the guest's
(+0x54 = its pol, star tech 5), so `coop.space empires home` still said mine=1. The mod made it once per world
(`g_home_epoch`) and never looked again. The guest's roster (space_map.cpp read_local) lists only tech-5 planets, so the
homeworld was not in it and the host's mirror never wanted it (wanted=7 = the 6 kept colonies + the host's Pradnie).
In the passing runs the host's kept-colonies store already held 0559A013: the guest's K_KEPT restore had generated and
colonized that star about a second earlier, and the colony stayed. What turns the record back is not known (~; the
generation log said pending=0); made again later it stayed tech 5 (25 s, and the whole test).
The "one skipped planet" in every run is a different thing and correct: `skipped 00599019: a colony or civilization
here already` is the host's own homeworld Pradnie, which the guest's game (the host's save, the player empire renamed)
lists as its home (`home shared` in the host's copy of the roster); the mirror must not take it.

Earlier notes (before the cause was found):
* The failing run is sgh	3_off_v1.out: the host's mirror had `wanted=7` (the guest's roster: its 6 older colonies +
  0559A013) but `stars=6 planets=6`: one wanted planet was not applied. `apply()` (space_allies.cpp) records why in
  `m.skipped` and `coop.space allies list` prints it as `  skipped <planet>: <why>`, but the driver cut the list at
  500 characters, so the reason was not seen. Candidates from `blocked()`: "not in this galaxy" (planet_record:
  index 5 of the host's star, the high byte of 0559A013), "a home star" (star +0x5C & 3), "a star system of another
  empire here", "you are on this planet now".
* The later runs (t3_off.out, t3_on_PASS.out) also picked 0559A013 and their mirror DID show star 5873683, but again
  `wanted=8` with `stars=7 planets=7`: one wanted planet is skipped there too (not known which).
* Why the pick differs per run: the host's kept-colonies store `%APPDATA%\SporeKit-dev1\Spore\Games-sporecoop-store  game0\pradnie.spo.txt` (BUG-1149, `r space.colonies`) is NOT restored by the drivers (only Games\Game0 is). Each run's
  homeworld becomes a kept colony of the guest there, the host's mirror owns that star in the next run (+0x54 not -1),
  so the pick moves on to the next free star. Now it holds 7 planets: 00599013 00599021 00599014 0059901E 02599012
  0059900D 0559A013 (copy: sghak\dev1_store\pradnie.spo.txt.after9312). The file ends with a checksum line
  (`end <hash>`): edit it only through the mod.
* Next steps: run sgh	4.py (prints the whole allies list with the skipped lines and `allies star <n>` of the guest's
  homeworld); to get the v1 state, restore a store without 0559A013 (or add a host test command forcing the pick);
  then fix the skip behind a COOP_FIX switch; and make the drivers restore the store file too.

## Fix
space_empires.cpp: every 5 s the guest checks that its homeworld's planet record (the star's own list, nothing
generated) is tech 5 and its star is its empire's; when not, it logs "my homeworld ... is not my colony any more in this
world: made again (BUG-9315)" and makes it again (`coop.space empires`: `keep=1 checks=N lost=N`). space_allies.cpp
colonize: a star already in the empire's star list (+0x88) is not added a second time (AddStarOwnership; each entry is a
star of the roster). Switch `coop.fix space.guest_home_keep off`: made once per world (0.7.15.98).

## Verification
Lane A (Steam exe, dev1 Pradnie from sgh\bak, the host's store gm\bak\store\six.txt = the 6 kept colonies without
0559A013, as before the first failing run), driver gm\t5.py, which also restores the host's store file:
* before the fix (the 0.7.15.97 build): gm\t5_diag.out with the full store (0559A013 kept) passed check 1 only
  because the store had the colony; gm\t5_six1.out with six.txt FAILED (the guest's record of 0559A013 tech=1, the
  host's mirror stars=6, wanted=7).
* OFF (`coop.fix space.guest_home_keep off` on both), run 20261121-212312 (lane A driver, not the lab): gm\t5_off.out FAIL check 1 (mirror wanted=7 planets=6, the host's
  star owner FFFFFFFF), keep=0 lost=0.
* ON, run 20261121-212516 (lane A driver, not the lab): gm\t5_on.out PASS (wanted=8 planets=7 incl. 0559A013, the host's `empires home` owner=80002C3C citizens=7CA81684;
  the guest's record tech=5 with its civ; keep=1 checks=2 lost=1, one "made again" at s 10.3, no repeat). Screenshot
  gm\b9315_on_host_in_guest_home_system.png (the host flown into Satis-3).
* Manual on the failing game before the fix: `coop.space empire colony 0559A013` on the guest made it tech 5 again; 25 s
  later the guest's roster had it and the host's mirror showed it.
* make test (SKIP_SANITIZERS=1): pass.

## History
- 2026-11-21: opened.
- 2026-11-21: root cause found and fixed on fix/guest-home-mirror (from build/amigos-98): 9d965336.
