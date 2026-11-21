---
id: BUG-9311
title: Civ own nations: a guest's game that left the session founds a capital for seat 1 in its copy of the host's world
status: open
kind: bug
stage: Civ
reported: 2026-11-21
source: lane B test (BUG-9304, task 2)
role: guest
versions: 0.7.15.96
fixed_in:
branch: fix/civ-citizens
commits:
tests:
signature:
docs: docs/re/civ.md#civ-guest-military-capture
related: BUG-9274, BUG-9276, BUG-9304
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
A Civ session with the guest's own nation; on the guest `coop.leave` while in Civ (do not go to the menu at once): the
guest's log shows "civ nations: founding seat 1's capital" within seconds.

## Root cause
Observed: civ_nations.cpp `authority()` is `in_civ() && !guest()`; after the session ends the guest's game is not a
guest any more, so found_task treats it as the world's owner (the code lets the host found while it plays alone:
"the nations live in the host's game also while it plays alone"). The roster read for that world was the guest's own
copy without a .duonations, and party seat 1 had its species, so seat 1 got a capital (inferred from the log lines). Not fixed here: telling a received
copy of the host's world from a world of one's own needs a rule of its own (own_world answers only during a session).

## Fix
None yet.

## Verification
no-fail-run: open, not fixed.

## History
- 2026-11-21: opened.
