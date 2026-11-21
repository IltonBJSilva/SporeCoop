---
id: BUG-9303
title: make test unit-symbols: triage_symbols_check names sporekit_mod_main+0x40 without a coop.cpp line on build/amigos-97
status: open
kind: test
stage: Tools
reported: 2026-11-21
source: make test (feat/space-late-launch)
role: 
versions: 0.7.15.96
fixed_in:
branch:
commits:
tests:
signature:
docs:
related: BUG-9299
---

## Report
`SKIP_SANITIZERS=1 make test` in WSL fails at unit-symbols (tools/tests/triage_symbols_check.py, 4 failures): the
planted fault at sporekit_mod_main+0x40 is named `sporekit_mod_main+0x40  mods/coop/coop.cpp` with no line number,
and the check wants `mods/coop/coop.cpp:<line>` (or an inlined function at a coop.cpp line). Seen on
feat/space-late-launch and on its base build/amigos-97 (015efaae) with the feature's changes stashed (coop.dll+0xB3010);
fix/civ-own-camera's build passes. So it is the code layout of build/amigos-97's coop.dll, not one change.

How to reproduce: in ~/wt/int97 (or any build/amigos-97 build) `make all`, then
`python3 tools/tests/triage_symbols_check.py build <llvm-mingw dir>`.

## Root cause
Inferred: the address sporekit_mod_main+0x40 has no line-table row in this build (an instruction the compiler put
under no source line), so llvm-symbolizer prints the function and file only. The test's address is fixed (+0x40), not
taken from the line table. Not analysed further (not part of feat/space-late-launch).

## Fix
<!-- what changed and why; the switch to turn it off for tests (coop.fix <name>) -->

## Verification
<!-- the test: FAIL without the fix, PASS with it, each with its lab run id (out/lab/YYYYMMDD-HHMMSS);
     or a line `no-fail-run: <reason>` (docs only, regression only, not reproducible) -->

## History
- 2026-11-21: opened.
