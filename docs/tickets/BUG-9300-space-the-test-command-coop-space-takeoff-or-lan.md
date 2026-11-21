---
id: BUG-9300
title: Space: the test command coop.space takeoff (or land) sent before the Space game exists crashes the game in GetCurrentContext
status: fixed
kind: crash
stage: Space
reported: 2026-11-21
source: lane C test (agentC6)
role: host
versions: 0.7.15.96
fixed_in: unreleased
branch: fix/space-captain
commits: 6d26b051, a76260fb
tests: tools/tests/coop_space_cmd_guard.py (manual driver, Windows lane; agentC6/repo_guard.log PASS)
signature: Simulator::GetCurrentContext+0x5 (data 0x00000010), module call coop.dll spacecoop::debug
docs:
related: BUG-9296
---

## Report
Lane C (Steam exe 3010a413, coop 0.7.15.96 + fix/space-captain), 2026-11-21 10:00: a test driver of an earlier run
(the agent's own, still running) sent `coop.space takeoff` to instance 5 while that game was still booting (5 s after
the start, before the galaxy menu). The game crashed: `exception 0xC0000005 at Simulator::GetCurrentContext+0x5 (data
0x00000010)`, the module call coop.dll+0x2E1BD0 = spacecoop::debug (the `takeoff` branch of mods/coop/spacecoop.cpp),
crash report crash-20261121-100020-i5.txt in lane C's mods logs.
Reproduce: start a game and send `coop.space takeoff` (or `coop.space land`) before any Space game exists.

## Root cause
Observed: the `land` / `takeoff` test commands only checked that the cAppModeSpace pointer is set (it is set at boot)
and then called GetCurrentContext, which reads `[[0x16DDA8C]+0x10]` (the simulator): null before a game exists.
A test command only (players never send it); drivers of the lab and of agents send it while they wait for Space.

## Fix
Both commands answer "not in Space" unless the Space stage runs (spacecoop's in_space()). No switch (a test command's
own guard).

## Verification
no-fail-run: a test command's guard (the crash itself is the report's: crash-20261121-100020-i5.txt). Checked in the
game (lane C, 2026-11-21 11:05, agentC6/t9300.py, t9300.log): `coop.space takeoff` 6 s after the start and
`takeoff` / `land` at the galaxy menu answer "not in Space", the game lives, no crash report.

## History
- 2026-11-21: opened and fixed on fix/space-captain.
