---
id: BUG-9303
title: make test unit-symbols: triage_symbols_check names sporekit_mod_main+0x40 without a coop.cpp line on build/amigos-97
status: fixed
kind: test
stage: Tools
reported: 2026-11-21
source: make test (feat/space-late-launch; also feat/difficulty-scaling and feat/custom-cells: BUG-9307)
role: 
versions: 0.7.15.96
fixed_in: unreleased
branch: fix/unit-symbols
commits: 6f502d1c
tests: unit-symbols
signature:
docs: docs/06-testing.md#testing-unit-symbols-line0
related: BUG-9299, BUG-9307, BUG-1123, BUG-1128
---

## Report
`SKIP_SANITIZERS=1 make test` in WSL fails at unit-symbols (tools/tests/triage_symbols_check.py, 4 failures): the
planted fault at sporekit_mod_main+0x40 is named `sporekit_mod_main+0x40  mods/coop/coop.cpp` with no line number,
and the check wants `mods/coop/coop.cpp:<line>` (or an inlined function at a coop.cpp line). Seen on
feat/space-late-launch and on its base build/amigos-97 (015efaae) with the feature's changes stashed (coop.dll+0xB3010);
fix/civ-own-camera's build passes. So it is the code layout of build/amigos-97's coop.dll, not one change.

The same failure was reported on feat/difficulty-scaling and on feat/custom-cells (both from build/amigos-97; the second
does not touch coop.cpp) as BUG-9307, a duplicate of this ticket: the frame line reads
`symbol coop.dll+0xB3040 sporekit_mod_main+0x40 mods/coop/coop.cpp` with no `:<line>`, so the regex
(`coop\.cpp:\d+`) does not match. Earlier batch logs (fix/civ-own-camera, test/integration) had 0 failures.

How to reproduce: build build/amigos-97 (`make all`), then
`python3 tools/tests/triage_symbols_check.py build $LLVM_MINGW`; `llvm-addr2line -f -i -e build/coop.dll
<_sporekit_mod_main + 0x40>` prints `coop.cpp:0`.

Not upstream: origin/main (f03ff689, 0.7.15.2) and upstream/main (cf4e3260, 0.7.15.5) built in WSL pass the check
unchanged; there +0x40 lies in `coop_core::init_config()` inlined into sporekit_mod_main (coop.cpp:2381 / 2382).

## Root cause
Observed (llvm-objdump -d -l, llvm-symbolizer --inlines over +0x38..+0x50 of build/amigos-97's coop.dll, 2026-11-21):

    +0x32  call game::init                      coop.cpp:3033
    +0x37  add  esp, 8
    +0x3A  mov  ecx, [H]                        line 0
    +0x40  mov  edx, [M]                        line 0   <- the planted fault
    +0x46  test al, al                          coop.cpp:3033
    +0x4A  push 1 ... (init_config inlined)     coop.cpp:2498 (inlined into sporekit_mod_main, coop.cpp:3037)

The reloads of the globals H and M after `call game::init` (the call may change them) carry DWARF line 0; crash_triage
then prints the function and file and no line, which is right (line 0 means "no source line"). The test's address was
fixed at +0x40 (BUG-1123 / BUG-1128 chose it for a build of their time, where +0x40 was inside the inlined
init_config); whether that byte has a line depends on the code layout at the start of sporekit_mod_main, not on the
change under test. Nothing is wrong in crash_triage or in coop.dll.

## Fix
tools/tests/triage_symbols_check.py (fix/unit-symbols, 6f502d1c): `planted_offset()` asks llvm-symbolizer for
sporekit_mod_main+0x40..+0x100 and plants the fault at the first offset whose innermost frame has a coop.cpp line
(this build: +0x46; a build like upstream's: +0x40 as before). The checks still demand `mods/coop/coop.cpp:<line>` (or
the inlined form) at that offset. New check: when +0x40 (or a later byte before the planted one) has line 0, a fault
there is named `sporekit_mod_main+0x40  mods/coop/coop.cpp` with no `:0`. No switch: a test-only change.

## Verification
- Without the fix (the committed test on build/amigos-97 015efaae, WSL ~/wt/fix-unit-symbols, 2026-11-21):
  `triage_symbols_check: 4 failure(s)` (the two --symbols checks, the downloaded symbols, the cache run).
- With the fix: `triage_symbols_check: 0 failure(s)`, the planted fault at sporekit_mod_main+0x46, the line-0 check
  ok.
- The fixed test still catches a broken tool (crash_triage.py changed in the tree, then restored): a triage that
  drops the line number: 4 failures; a triage that prints `coop.cpp:0`: 1 failure (the new line-0 check).
- no-fail-run: test-only change (no lab run; the unit test is the check itself).

## History
- 2026-11-21: opened.
- 2026-11-21: BUG-9307 (feat/difficulty-scaling) closed as its duplicate; text merged here.
- 2026-11-21 (fix/unit-symbols): fixed in the test (6f502d1c); upstream main passes unchanged.
