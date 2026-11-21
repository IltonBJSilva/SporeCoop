---
id: BUG-9307
title: unit-symbols: triage_symbols_check fails when sporekit_mod_main+0x40 is an instruction with DWARF line 0
status: duplicate
kind: tooling
stage: Tools
reported: 2026-11-21
source: make test on feat/difficulty-scaling and feat/custom-cells (base build/amigos-97)
role: 
versions: 
fixed_in:
branch: fix/unit-symbols
commits:
tests:
signature:
docs:
related: BUG-9303
---

## Report
`SKIP_SANITIZERS=1 make test` on feat/difficulty-scaling and on feat/custom-cells (both from build/amigos-97; the second
does not touch coop.cpp) fails in unit-symbols: `triage_symbols_check: 4 failure(s)`; the frame line reads
`symbol coop.dll+0xB3040 sporekit_mod_main+0x40 mods/coop/coop.cpp` with no `:<line>`, so the regex
(`coop\.cpp:\d+`) does not match. Earlier batch logs (fix/civ-own-camera, test/integration) had 0 failures.

How to reproduce: build build/amigos-97 (or either branch), `python3 tools/tests/triage_symbols_check.py build
$LLVM_MINGW`; `llvm-addr2line -f -i -e build/coop.dll <_sporekit_mod_main + 0x40>` prints `coop.cpp:0`.

## Root cause
Observed (llvm-objdump -d -l): sporekit_mod_main+0x40 (0x100B3040 in this build) is `movl 0x104d3424, %edx`, the
reload after `call game::init`, which the compiler gives DWARF line 0; crash_triage prints the file without a line for
it. The check assumes +0x40 always has a line (BUG-1123 / BUG-1128 chose +0x40 for a build of their time): it depends
on the code layout at the start of sporekit_mod_main, not on the change under test.

## Fix
Duplicate of BUG-9303 (the same failure, reported on feat/space-late-launch): fixed there on fix/unit-symbols; the
idea of this ticket (the first address at or after +0x40 whose line is not 0) is what was done. No change here.

## Verification
no-fail-run: duplicate; BUG-9303's verification (the old test fails 4 checks, the fixed one passes) covers it.

## History
- 2026-11-21: opened.
- 2026-11-21 (fix/unit-symbols): duplicate of BUG-9303; its report and root cause merged into BUG-9303.
