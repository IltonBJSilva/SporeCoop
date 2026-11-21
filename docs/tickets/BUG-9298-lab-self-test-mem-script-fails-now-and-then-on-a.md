---
id: BUG-9298
title: lab self-test mem_script fails now and then on a loaded WSL (the 400 MB script measured at 163 MB, the 25 s limit)
status: fixed
kind: test
stage: Tools
reported: 2026-11-21
source: make test on WSL, fix/civ-own-camera, 2026-11-21
role: 
versions: 
fixed_in: unreleased
branch: fix/unit-symbols
commits: 0f6e216b
tests: unit-lab
signature:
docs: docs/06-testing.md#testing-unit-symbols-line0
related: BUG-0916
---

## Report
`SKIP_SANITIZERS=1 make -k -j8 HOSTCXX=clang++ test` in WSL (Ubuntu-24.04, Windows 11 host with four Spore games of
other lanes running, 2026-11-21 09:2x) failed once in unit-lab: `FAIL mem_script: a script of 400 MB over a 150 MB
limit is killed and fails (FAIL lab: the test script used 163 MB of memory (limit 150 MB, SPORE_LAB_SCRIPT_RSS_MB): k)`.
The run right after passed with no change (log agentB/test-fix-civ-own-camera.log of lane B).

## Root cause
Opened as: not analysed. tools/tests/lab_selftest.py mem_script needs the kill and the whole check within 25 s
(`time.time() - t0 < 25`); a slow allocation on a loaded machine (163 MB seen, not 400) may pass that limit, or the kill
came later than the check expects. The same class as the known "zombie leader" WSL flake of unit-tools.

Observed (fix/unit-symbols, 2026-11-21): the failed check line held the "test script used" line, so the kill worked;
what failed was the 25 s bound. That bound covered the killed run, the small run after it and run_one's checks after
each (AFTER sleep, lane and report scans). With 24 busy loops and 4 GB held beside it in WSL, the killed run alone took
12-19 s and both runs 25+ s: the old check failed 2 of 3 runs. The kill itself came at 158-217 MB (the sampler's 0.3 s
step while the 400 MB bytearray is filled), as the report's 163 MB.

## Fix
tools/tests/lab_selftest.py (0f6e216b): the big script sleeps 120 s and the bound is its own run's time, under 60 s (a
kill that did not happen still fails it: the lab with kill_group removed took 131.7 s, FAIL); the killed script's
processes are polled up to 10 s before "no process left" is judged; the three conditions (killed and failed, the time,
no process left) are separate checks, so a failure names its cause. What the case checks is unchanged.

## Verification
- Under load (24 busy loops + 4 GB held, `lab_selftest.py --case mem_cases`, WSL, 2026-11-21): the old check FAIL in
  2 of 3 runs; the new check ok in 3 of 3 (the killed run 18.8 s, 12.0 s, 12.6 s).
- A lab that never kills (kill_group in MemSampler.sample replaced by `pass`, restored after): the new check FAIL
  (`the kill ends its run long before the script's own 120 s (131.7 s)`).
- no-fail-run: test-only change (no lab run; the self-test is the check itself).

## History
- 2026-11-21: opened.
- 2026-11-21 (fix/unit-symbols): reproduced under load, the bound fixed (0f6e216b).
