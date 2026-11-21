---
id: BUG-9302
title: Cell: custom cells (the players My Creations cells) as native species of their diet, without giants
status: fixed
kind: feature
stage: Cell
reported: 2026-11-21
source: Lineages PR #196 (Custom Cells), the separate-lives batch
role: host
versions: 
fixed_in: unreleased
branch: feat/custom-cells
commits: 14d351a9
tests: tools/tests/coop_custom_cells.py (suite custom_cells, twin custom_cells_off); manual two-instance runs on lane D
signature:
docs: docs/re/cell.md#cell-custom-cells
related: BUG-9275, BUG-9285, BUG-9292
---

## Report
The separate-lives batch ports Lineages' "Custom Cells" (PR #196): the cells the players made swim in the Cell stage as
native species (about 1 in 6 spawns) and follow their mouth's diet. Our port (BUG-9275) had them, BUG-9285 took them
out again (only the players' current cells now) because they appeared as giants; BUG-9292 asked for no more cells.

## Root cause
Not a bug: a feature. The giants of BUG-9285: a My Creations cell went onto a spawn of ANY native species, the species
of the levels above the avatar too, which the game draws bigger. Observed (runtime): PopulateMarker's level argument is
the level (+0x358) of the cell it makes; the live pool holds levels from the avatar's up.

## Fix
The session setting `cell.custom_cells` (Cell, Live, on as in Lineages): with cell.lineage_cells and
cell.lineage_current on, the pool also holds up to 24 My Creations cells (the host's and the ones every guest sends;
owner -1), and they take about one in six spawns of another native species with the same diet, only spawns of the
avatar's food level or below (h_Populate keeps the marker's level; cellsync_lineage.cpp choose_model). The partners'
current cells keep their BUG-9285 rule (player species only). Nothing is spawned: only the model of a spawn the game
makes anyway. Switches: `coop.fix cell.custom_cells off` (no custom cells), `coop.fix cell.custom_level off` (the twin:
every level, Lineages' giants). coop_lineage_current.py switches the custom cells off (it counts only current cells).

## Verification
Lane D (Windows, Steam build, two instances, Socle at level 6, `coop.lineage share 100`), driver
E:\Repositorio\SporeCoop-wt\laneD\t_cc.py: on (three runs): every live My Creations cell on the host of level 5 or 6
(12 models), the guest's game drew all 12; off: no new swap; the twin (custom_level off): cells of level 7: FAIL as it
should. Pool counts on vs off the same within noise (avg 595-712 vs 540-700). Screenshots of both games.
tools/tests/coop_custom_cells.py (+ twin) written for the lab, not run there (no Linux lanes on this PC).
no-fail-run: a feature; the twin above failed as it should on lane D.

## History
- 2026-11-21: opened.
