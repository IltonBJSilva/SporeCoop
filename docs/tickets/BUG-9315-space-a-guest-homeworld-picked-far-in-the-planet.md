---
id: BUG-9315
title: Space: a guest homeworld picked far in the planet ids (0559A013) is the guest's colony on its game but the host's allies mirror never shows it
status: open
kind: bug
stage: Space
reported: 2026-11-21
source: in-game test of feat/space-guest-home, lane A
role: host
versions: 0.7.15.97
fixed_in:
branch: feat/space-guest-home
commits:
tests:
signature:
docs: docs/re/space.md#space-mirror-ally-break
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
Not found yet (stopped at the owner's pause, 2026-11-21). What is known:
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
<!-- not fixed -->

## Verification
<!-- none yet -->

## History
- 2026-11-21: opened.
