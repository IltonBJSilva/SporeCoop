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
Not looked into. Inferred candidates: the guest's roster (space_map.cpp kept_roster) or the host's allies sync
(sync_from_rosters / colonize) skips this planet (a far star never generated on the host? the planet id's high byte
05 vs 00), or the host's pick itself is a star the host's game has no record of yet.

## Fix
<!-- not fixed -->

## Verification
<!-- none yet -->

## History
- 2026-11-21: opened.
