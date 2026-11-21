---
id: BUG-9310
title: Space: a guest new to the host's Space world (its party seat has no species) gets no own empire and no late-launch offer
status: open
kind: bug
stage: Space
reported: 2026-11-21
source: smoke of build/amigos-97 on lane C
role: guest
versions: 0.7.15.97
fixed_in:
branch:
commits:
tests:
signature:
docs:
related:
---

## Report
Smoke of the friends build 0.7.15.97 on lane C (Windows, Steam exe, instances 5/6). The host (dev5) loads Dopov (a
full copy of the dev1 profile's Game0, whose party has seats for lane A's players) and takes off; the guest dev6, whose
identity 47AF3B5C has never played this world, joins late. The host's party gives it a new seat 3 with
species=00000000; `coop.space empires` on the guest stays `guest own=0 seat=-1` with the host's name 'Brawgle Empire'
and captain, and `coop.space empires launch` stays `phase=idle offers=0` (no late-launch offer, BUG-9299) for 90 s.
With `coop.space empires found <gid> 292846CB` on the host (the test seam agentC6 used) the guest gets 'Bob's empire'
and the offer, and the whole late launch passes (r97/late.out).
How to reproduce: E:\Repositorio\SporeCoop-wt97\late.py without the `found` step (log r97/late_nofound.out).
Question for the owner: should a friend who joins a Space save for the first time get its own empire (with a creature
of its choice, like the "Your own start" page of a new game), or fly with the host's as now?

## Root cause
<!-- observed (runtime, logs, disassembly) vs inferred -->

## Fix
<!-- what changed and why; the switch to turn it off for tests (coop.fix <name>) -->

## Verification
<!-- the test: FAIL without the fix, PASS with it, each with its lab run id (out/lab/YYYYMMDD-HHMMSS);
     or a line `no-fail-run: <reason>` (docs only, regression only, not reproducible) -->

## History
- 2026-11-21: opened.
