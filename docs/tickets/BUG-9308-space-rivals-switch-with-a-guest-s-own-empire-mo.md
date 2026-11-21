---
id: BUG-9308
title: Space rivals switch with a guest's own empire money: the split halves the host's money and the guest's share is lost, the merge gives nothing back
status: open
kind: bug
stage: Space
reported: 2026-11-21
source: found on the way (feat/rivals-stages, lane A)
role: host
versions: 0.7.15.96
fixed_in:
branch: feat/rivals-stages
commits:
tests: by hand on lane A with agentRV/space3.py (the batch folder, not in the repo); space3_on.log PASS, space3_off.log FAIL
signature: coop.space rivalry: after "mode rivals" and "mode allies" the sum of the players' money is lower than before
docs: docs/re/space.md#space-rivals-own-money
related: BUG-9306, BUG-9279, BUG-1029
---

## Report
Found while wiring the stage table's `rivals.space` to the Space rivals mode (BUG-9306), lane A, Dopov, a guest with its
own empire (space_empires.cpp, BUG-9279): the switch to rivals took half of the host's money (100000 -> 50000) and the
guest's game ended with 0 or half of its own money; the switch back to allies put the host's money up by the guest's
last report but the guest kept the cut. 50000 of 200000 were lost each round. The same with `coop.space rivalry mode
rivals` (the old console path), so not caused by the table.

## Root cause
Observed: space_rivalry.cpp's split counts every player of the roster (total / players for each, the host keeps its
share) and each guest's game adds `share - total` to its money; a guest with its own empire money holds its own budget,
not the shared total, so the delta took money it never had. The merge then credited the guest's report to the host
(and the guest's own empire was not refilled). spaceempires::own_money / own_money_player read false while the rivals
mode is on (their `active()`), so they could not tell such a guest at the merge either.

## Fix
`coop.fix space.rivals_own_money` (on): a guest with its own empire money (space_empires.cpp own_empire /
own_empire_player, which answer in both modes) is outside the split and the merge, asks the host for no budget (K_ASK)
and ignores K_SET in the rivals mode: its own empire's money is its budget in both modes. Off: 0.7.15.96.

## Verification
By hand on Windows lane A (no Linux lab on this PC), driver agentRV/space3.py (Dopov, the guest joined in Space with its
own empire; the host switches with the settings page's "Space stage: Rivals" and back with the Space page's "Switch to
allies"):
* PASS (switch on, space3_on.log): host 100000 / guest 100000 before, after rivals and after allies.
* FAIL (`coop.fix space.rivals_own_money off` on both games, space3_off.log): after rivals host 50000 / guest 50000,
  after allies host 100000 / guest 50000.

## History
- 2026-11-21: opened.
- 2026-11-21: fixed on feat/rivals-stages, ON PASS / OFF FAIL by hand on lane A.
