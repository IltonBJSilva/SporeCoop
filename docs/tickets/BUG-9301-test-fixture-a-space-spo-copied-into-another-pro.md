---
id: BUG-9301
title: Test fixture: a Space .spo copied into another profile without its galaxy crashes the host's load in cStarRecord::GetPlanetRecord
status: open
kind: test
stage: Space
reported: 2026-11-21
source: lane C test (agentC6)
role: host
versions: 0.7.15.96
fixed_in:
branch: fix/space-captain
commits: fa93536d
tests:
signature: Simulator::cStarRecord::GetPlanetRecord+0x12 (read of 0) < cStarManager::GetPlanetRecord+0x3B
docs: docs/06-testing.md
related: BUG-9296
---

## Report
Lane C, 2026-11-21 09:53: the Space save Dopov.spo (and its .duo* files) was copied from the dev1 test profile into
dev5's Game0, which has another galaxy (its own stars.db, planetRecords.pkp/.pkt). `coop.load Game0 Dopov` at the
galaxy menu crashed the game: `0xC0000005 at Simulator::cStarRecord::GetPlanetRecord+0x12` (read of 0), called from
cStarManager::GetPlanetRecord (crash-20261121-095308-i5.txt in lane C's mods logs).

## Root cause
Observed: a Space planet save names its star and planet records; they live in the profile's Game0 galaxy files, not in
the .spo. A .spo copied alone into a profile whose galaxy lacks that star gives a null star record and the game's own
load faults. Not a coop bug: a test pitfall.

## Fix
None in the code. Test practice (docs/06-testing.md): copy the whole Game0 folder of a profile (the .spo files,
stars.db, planetRecords.pkp/.pkt, the .duo* files), not one .spo. The lane C Dopov fixture is a full copy of dev1's
Game0.

## Verification
no-fail-run: a fixture rule (documentation only).

## History
- 2026-11-21: opened (seen while preparing the BUG-9296 test).
