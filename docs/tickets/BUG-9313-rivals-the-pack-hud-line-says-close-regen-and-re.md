---
id: BUG-9313
title: Rivals: the Pack HUD line says close (regen) and rescue ready in a Rivals Creature stage where the pack aids are off
status: fixed
kind: bug
stage: Creature
reported: 2026-11-21
source: 0.7.15.98 smoke
role: both
versions: 0.7.15.98
fixed_in: unreleased
branch: build/amigos-98
commits: 21a401dd
tests: by hand on lane B, Marona (Creature), driver E:\Repositorio\SporeCoop-wt98	9313.py (not in the repo; no lab test)
signature:
docs:
related: BUG-9306
---

## Report
In the 0.7.15.98 smoke (lane B, Marona, Creature stage set to Rivals) both HUDs showed "Pack: Bob close (regen) | rescue ready"
while the race ran: the regeneration and the pack rescue are co-op aids, off in a Rivals stage (BUG-9306), so the line
promised help the player does not get (screenshots r98_rv_won_host.png / r98_rv_won_guest.png of the release folder).

## Root cause
Observed in the code: teamplay::tick and the rescue read settings::on(kPackHeal / kPackRescue), which settings::get
turns off in a Rivals stage, but teamplay::status() printed "(regen)" by distance alone and "ready" by the cooldown alone.
Only the text was wrong: the heal and the rescue themselves were off.

## Fix
status() shows "(regen)" only when kPackHeal is on and "rescue off" when kPackRescue is off. Switch
`coop.fix team.hud_aids off` (the old text).

## Verification
By hand on lane B (two games, Marona, Creature stage set to Rivals, the guest put next to the host), r98	9313.out:
- `coop.fix team.hud_aids off` on both: FAIL (the bug): "Pack: Bob close (regen) | rescue ready" on the host,
  "Pack: Player close (regen) | rescue ready" on the guest.
- on: PASS: "Pack: Bob close | rescue off" on the host, "Pack: Player close | rescue off" on the guest; back to
  Together: "close (regen) | rescue ready" again. No crash report.
- note: the run's first Together check saw Rivals still set: the host kept the Creature stage = Rivals of the run
  before (the settings are the host's, kept across sessions), a test setup slip, not this bug.

## History
- 2026-11-21: opened.
