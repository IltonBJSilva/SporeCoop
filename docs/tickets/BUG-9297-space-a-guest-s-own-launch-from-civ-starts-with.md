---
id: BUG-9297
title: Space: a guest's own launch from Civ starts with the host's captain and money until the host's world loads
status: fixed
kind: bug
stage: Space
reported: 2026-11-21
source: lane C test (BUG-9295)
role: guest
versions: 0.7.15.2
fixed_in: unreleased
branch: fix/space-captain
commits: 5fa28cf1, d5100ef6, fa93536d
tests: tools/tests/coop_civ_launch_captain.py (manual driver, Windows lane; the lab has no Civ -> Space guest launch fixture), agentC6/t9297.py (lane C)
signature:
docs: docs/re/civ.md#civ-guest-launch-captain
related: BUG-9295, BUG-9296
switch: space.own_captain_local
---

## Report
Seen in the BUG-9295 test (lane C, Kealeneon): after the guest's own launch (its own ship editor, its own game into
Space) the guest's planet-side Space start is its local copy of the host's Civ save: the HUD shows the host's captain
(the host's species, "Keigh Emp...") and the save's money (88,888 on the fixture; the seat's own money is the same number there, so the money part is not confirmed yet). Only when the guest leaves its home
planet and the host's world loads does space_empires set the guest's own captain (7CA81684), money and empire name.
Reproduce: agentC4/t1.py (batch folder), screenshot after "guest: its own game launches into Space".

## Root cause
Observed (lane C, 2026-11-21): the guest's own Space start is its copy of the host's Civ save: player empire 'Keigh
Empire', captain 292ACCC6 (the host's), the HUD planet panel 'Keigh Empire'. space_empires applies the seat's own
empire only on a K_EMPIRE answer, which the host sends once its own game is in Space (it designs its ship after the
guest); that answer does reach the guest's local start (the same save name, not own_world::space_other), so the window
is the guest's start while the host is still in its editor (the ticket's "until the host's world loads" was that
window plus the hold). Two details found on the way: on the guest's game its adopted nation's species (cCivilization
+0x268) is 0, and the Space start's lift-off cinematic stays on until the player clicks through the texts (a gate on
cinematic_playing never fired).

## Fix
civ_launch.cpp keeps the guest's species at the end of the spaceship vote (civ::nations::own_species, else its own
creature creations::mine(kCreature): the seat's species); in its Space phase it calls space_empires::local_start every
second until applied: the player empire gets that captain (cEmpire_SetCaptainKey), "<player>'s empire", the player's
colour, and the HUD planet panel is rebuilt (BUG-9296); all put back before a save (name_undo). Switch
`space.own_captain_local` (off: the host's captain and name until the host's K_EMPIRE, 0.7.15.96). The money stays the
save's until K_EMPIRE (the host tells the seat's money; 88,888 both in the fixture).

## Verification
Lane C (Windows, Steam exe 3010a413, instances 5/6), the Kealeneon fixture (agentC2/fixture_keal), driver
agentC6/t9297.py: the team victory, the vote, the guest's own ship editor and launch; 30 s later, with the host still
in its spaceship editor, the guest's `coop.space empires` / `coop.space empires hud`:
- FAIL, `coop.fix space.own_captain_local off` on the guest: 'Keigh Empire', captain 292ACCC6, panel 'Keigh Empire'
  (agentC6/t9297_off.log, t9297_off_guest.png); after the host reached Space the K_EMPIRE path set its own.
- PASS, switch on: 'Bob's empire', captain 7CFDDDC1 (its own species, `coop.civ launch` species=7CFDDDC1
  own_start=1), panel 'Bob's empire' (agentC6/t9297_on.log); after the start texts the HUD shows "Bob's emp..."
  (agentC6/t9297_on_guest_hud.png). Earlier runs t9297_v1_* / t9297_v2_on.log: the species 0 and the cinematic gate.
The repo's manual driver tools/tests/coop_civ_launch_captain.py (the same steps; the session prepared by
agentC6/prep_keal.py): PASS on (species 292846CB, its own creature's key; agentC6/repo_on.log), FAIL with `--off`
('Keigh Empire', captain 292ACCC6; agentC6/repo_off.log).
No crash or fault report in either game.

## History
- 2026-11-21: opened.
- 2026-11-21: fixed on fix/space-captain (the own launch's Space start gets the guest's captain and name at once).
