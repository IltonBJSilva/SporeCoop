---
id: BUG-9311
title: Civ own nations: a guest's game that left the session founds a capital for seat 1 in its copy of the host's world
status: fixed
kind: bug
stage: Civ
reported: 2026-11-21
source: lane B test (BUG-9304, task 2)
role: guest
versions: 0.7.15.96
fixed_in: unreleased
branch: fix/guest-leave
commits: d89f98d1
tests: manual lane C driver t9311.py; unit tests/win_tests.cpp (slotfs::received_slot)
signature:
docs: docs/re/civ.md#civ-received-world-no-founding, docs/re/civ.md#civ-guest-military-capture, docs/archive/known-issues-2026.md#issue-guest-alone-founds-capital
related: BUG-9274, BUG-9276, BUG-9304
switch: civ.nations_received_world
---

## Report
Lane B, 0.7.15.96 + fix/civ-citizens, the Creature save Mirey played on to Civ (dev3 host, dev4 guest; the guest's own
nation 00002BC2 founded by the host). The test driver made the guest leave (`coop.leave`) to rejoin. In the 4 s between
the session's end and the guest's quit to the galaxy menu, the GUEST's game (no session any more, still in its local copy
of the host's Civ world) ran the host-side founding:

    12:55:02.247 store: world 'game26/mirey.spo' opened: 0 players, 0 records (no file)
    12:55:02.258 civ nations: founding seat 1's capital (seat 1)
    12:55:06.274 civ nations: seat 1 has their own nation 00002BC1 (seat 1, species 7CCDE692, 0 city key(s)), capital at
                 (-206 -452 -97), 747 m from the host's
    12:55:07.348 save: ... (the world saved while leaving the stage)

So the guest's game founded a city (SpawnNextCity on its own copy), wrote a roster for that copy and saved it into its
received slot (dev4 Game26). After the rejoin the host's world replaced it (no harm seen in the session); a guest that
plays on alone in its copy of the host's Civ world would keep a second nation of its own there.

## Reproduce
Lane C, E:\Repositorio\SporeCoop-wt\guestleave\t9311.py: Kealeneon (dev5 host, dev6 guest Bob), the host founds the
guest's nation (seat 3 there), the guest plays the reloaded world with it, then `coop.leave` on the guest (it stays in
Civ). Lane B's copy (Mirey, played Tribe -> Civ) had the Tribe founders (.duofounders) and no Civ roster in the received
slot, so its game wanted a capital for the guest's seat; the Kealeneon copy comes with the host's .duonations, so the
driver renames it to .duofounders before the leave (side files only). Then the guest's game alone: "no free site:
seat 3's nation takes over NPC nation 000021D0", the city's species key set, the roster written into its slot.
Lane B's steps: A Civ session with the guest's own nation; on the guest `coop.leave` while in Civ (do not go to the menu at once): the
guest's log shows "civ nations: founding seat 1's capital" within seconds.

## Root cause
Observed: civ_nations.cpp `authority()` is `in_civ() && !guest()`; after the session ends the guest's game is not a
guest any more, so found_task treats it as the world's owner (the code lets the host found while it plays alone:
"the nations live in the host's game also while it plays alone"). The roster read for that world was the guest's own
copy without a .duonations, and party seat 1 had its species, so seat 1 got a capital (inferred from the log lines). The rule that tells them apart
now: a received slot carries the marker sporecoop.txt (`host_slot`, `host`, `planet`: slotfs.cpp writes it into every
received world, the tests' own copies remove it).
Also seen: the party roster is loaded only on a host and is empty on the guest's game after the leave; the wanted
seats came from the Tribe founders of the copy (founders::read Stage::Tribe), not from the party.

## Fix
civ_nations `authority()` is false on a game that does not host and plays a received slot
(`world::playing_received_world()`, slotfs::received_slot on the save database's folder, read at most once a second):
no founding, adoption, alliance raise or team campaign in a copy of another player's world. Hosting such a copy keeps
the host's work. Switch `civ.nations_received_world` (off: as before). `coop.civ nations` prints "received world
<0|1> (held N, fix F)".

## Verification
Lane C, two instances (dev5 host, dev6 guest), driver t9311.py, logs in E:\Repositorio\SporeCoop-wt\guestleave:
* FAIL (switch off on the guest): t9311_off.log (15:20): after the leave the guest's game logged "seat 3's nation takes
  over NPC nation 000021D0", set the city's species key and wrote kealeneon.spo.duonations into its received slot.
* PASS (switch on): t9311_on.log and t9311_on2.log: no founding line, no roster written, "received world 1" with held > 0,
  the game stays in Civ, no crash report; the host's seats unchanged.
* Unit: tests/win_tests.cpp test_slot_match checks slotfs::received_slot (a received slot names its host, an own slot
  and "" are not received; make test-win, not run here).

## History
- 2026-11-21: opened.
- 2026-11-21: reproduced on lane C (with the copy's roster as lane B's), fixed on fix/guest-leave.
