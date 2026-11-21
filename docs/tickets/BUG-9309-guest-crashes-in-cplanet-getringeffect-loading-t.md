---
id: BUG-9309
title: Guest crashes in cPlanet::GetRingEffect loading the host's world after its session closed while it was in its own creature editor
status: open
kind: crash
stage: Menu
reported: 2026-11-21
source: lane B test (BUG-9304 campaign replay)
role: guest
versions: 0.7.15.96
fixed_in:
branch: fix/civ-citizens
commits:
tests:
signature: 0xC0000005 at 0x01340A20 Simulator::cPlanet::GetRingEffect+0x90 (data 0x13C) < cSpaceGfx::InitializeInternal+0x9A5 < SporeCoop::GalaxyMenu_SimReset
docs:
related: BUG-0310, BUG-0284
---

## Report
Lane B, 0.7.15.96 + fix/qa-campaign, the QA campaign drivers replayed on the Cell save Phivenova (dev3 / dev4). In
Creature the guest was in its own creature editor (the vote's editor, "own editor opened 66.3 s after the vote") when
the campaign driver's save step saved, made the guest leave (`coop.leave`) and took the host to the galaxy menu. The
guest's game quit its editor to the galaxy menu ("own editor result: ... cancelled 1"); 30 s later it joined again,
received the host's world and, 0.3 s into `LoadGameByName`, crashed:

`exception 0xC0000005 at 0x01340A20 Simulator::cPlanet::GetRingEffect+0x90 (data 0x0000013C)`, called from
`Simulator::cSpaceGfx::InitializeInternal+0x9A5 < SporeCoop::GalaxyMenu_SimReset+0x2D02`. The line before the load:
`world: the galaxy menu still holds the old world's player empire 800021CA: cleared it` (BUG-0284's reset). Crash
report kept as E:\Repositorio\SporeCoop-wt\civcit\crash-20261121-120351-i4.txt (lane B, not in the repo).

## Reproduce
Not tried again. Steps of the one run: a two-player Creature game, the guest in its own editor after a vote, the guest
leaves the session from the editor, the host goes to the galaxy menu, the guest's game leaves the editor to the menu,
the host loads the world again and hosts, the guest joins: the guest's load faults.

## Root cause
Not analysed. BUG-0310 saw the same GetRingEffect+0x90 fault in the load after a stage was left with a fault; here
no fault came before it, but the stage left was an editor (Editor -> Galaxy menu, "Creature -> none").

## Fix
None yet.

## Verification
no-fail-run: open, not analysed.

## History
- 2026-11-21: opened (seen once on lane B while replaying the campaign for BUG-9304).
