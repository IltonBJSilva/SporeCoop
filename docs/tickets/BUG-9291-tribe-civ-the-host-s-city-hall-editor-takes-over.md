---
id: BUG-9291
title: Tribe -> Civ: the host's City Hall editor takes over 30 s to open on a loaded PC and every guest's session times out
status: open
kind: bug
stage: Tribe, Civ
reported: 2026-11-20
source: integration campaign QA lane B (test/integration)
role: host, guest
versions: 0.7.15.94
fixed_in:
branch:
commits:
tests:
signature: hang: App::cJob::Wait < cTextureManager < Palettes::PalettePageUI::Load
docs:
related: BUG-0655, BUG-0576
---

## Report
Integration campaign QA (lane B, test/integration, planet Epina, 2026-11-20 22:06): the Tribe -> Civ vote ended
(22:06:31, "the editor does not wait for the game's own input_lock,paused"); the host's game then drew no frame for
more than 30 s while it opened the City Hall editor. The SporeKit watchdog wrote a hang report at 22:07:01 (main thread
in App::cJob::Wait under cTextureManager, from Palettes::PalettePageUI::Load: the editor's palette images). The guest's
session ended at the same moment: "connection to host lost (timeout)", last heard 29.6 s before. The host's game came
back by itself, finished both editors and reached Civ alone; the guest stayed in its Tribe world, offline, until it
joined again (the rejoin then brought it into Civ with its own nation, BUG-9274). The same step on planet Homan an hour
before kept the guest (no stall that long). Lane A's games ran on the same PC at the time.

## Reproduce
Not on demand: two games in Tribe, the host's goal full (`coop.tribe advance`), the advance button, the stage card,
the guest's I'm ready; on a loaded PC the host's City Hall editor can take more than 30 s to open.

## Root cause
Observed: the host's main thread waits for the game's own texture jobs (report-20261120-220701-i3); during that time
the host sends nothing, and the guest's session ends after 30 s without a packet (net/session.cpp, BUG-0655 notes the
same rule). Inferred: any host stall over 30 s in a load or an editor's opening drops every guest.

## Fix
None yet. Ideas: the host tells its guests before a known long wait (the editors at a stage's end, the Tribe -> Civ
editor) so the guests' timeout is longer for it, or a send from a thread of its own while the main thread waits.

## Verification
no-fail-run: open bug, no fix yet.

## History
- 2026-11-20: opened.
