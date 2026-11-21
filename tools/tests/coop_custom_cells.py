#!/usr/bin/env python3
"""BUG-9302 (Lineages' Custom Cells): the players' My Creations cells swim as native species of their diet, but only on
spawns of the player's level or below (never a giant), with the session setting cell.custom_cells (default off in the friends build; the test turns it on).

What each game shows is read from the game: `coop.lineage` lists the live creature cells of the pool by the model key the
game draws them with (cell +0xFC) and, for each cell of a player model, its level (+0x358) and what it chases.
  1. the host's candidates are its own My Creations cells and the ones the guest sent (owner=-1), and the guest's
     current cell (owner=1, the player species' spawns, BUG-9285);
  2. with `coop.lineage share 100` and both players swimming about (90 s) the host's pool holds My Creations cells; EVERY
     live cell of a My Creations model is of the host's own food level or below (no bigger species: no giant); the
     guest's game draws them from its own keys of those models;
  3. cell.custom_cells off: no new custom swap (the host's `custom swapped=` stays);
  4. no faults.
--old (`coop.fix cell.custom_level off` on the host, the twin `custom_cells_off`): the cells go onto every level as in
Lineages: step 2 fails (cells of the levels above, the giants of BUG-9285).

  SPORE_LANE=N python3 tools/tests/coop_custom_cells.py [--old]
"""
import re
import sys
import time

from twoinst import Checks, shot, start
from coop_lineage_cells import out, roam, wait


def lin(con):
    s = out(con, 'coop.lineage')
    d = {'cands': {}, 'pcells': [], 'remap': {}, 'lof': -1, 'custom': -1}
    for ln in s.splitlines():
        m = re.match(r'cell (\w{8}) sig=\w+ (\S\S) .* from=(\S+) peers=\d+ owner=(-?\d+)', ln)
        if m:
            d['cands'][m.group(1)] = (m.group(2), m.group(3), int(m.group(4)))
        m = re.match(r'pcell \d+ model=(\w+) diet=\S+ .* level=(-?\d+)', ln)
        if m:
            d['pcells'].append((m.group(1), int(m.group(2))))
        m = re.match(r'remap (\w+) -> (\w+)', ln)
        if m:
            d['remap'][m.group(1)] = m.group(2)
        m = re.search(r'own \w{8} food_level=(-?\d+)', ln)
        if m:
            d['lof'] = int(m.group(1))
        m = re.search(r'custom swapped=(\d+)', ln)
        if m:
            d['custom'] = int(m.group(1))
    return d


def main():
    old = '--old' in sys.argv
    host, guest = start('cell', own_progress=True)
    ck = Checks()
    try:
        if old:
            host.call('coop.fix cell.custom_level off')
        host.call('coop.settings force cell.custom_cells on')
        host.call('coop.lineage share 100')
        ck(wait(lambda: any(v[2] < 0 for v in lin(host)['cands'].values()), 120, 3),
           '1: the host has My Creations cells among its candidates')
        time.sleep(20)  # the guest's cells and their records (8 s before a first use)
        d = lin(host)
        custom = {k for k, v in d['cands'].items() if v[2] < 0}
        print(f"  candidates: custom {sorted(custom)}, current {sorted(k for k, v in d['cands'].items() if v[2] >= 0)}",
              flush=True)
        seen, levels, gseen = set(), set(), set()
        t0, k = time.time(), 0
        while time.time() - t0 < 90:
            d = lin(host)
            for m, lv in d['pcells']:
                if m in custom:
                    seen.add(m)
                    levels.add((m, lv, d['lof']))
            g = lin(guest)
            inv = {v: h for h, v in g['remap'].items()}
            gseen |= {inv.get(m, m) for m, _ in g['pcells']}
            roam(host, guest, k)
            k += 1
            time.sleep(6)
        print(f'  My Creations cells live on the host (model, level, host level): {sorted(levels)}', flush=True)
        ck(seen, f'2: the host\'s pool holds My Creations cells ({sorted(seen)})')
        ck(levels and all(lv <= hl for _, lv, hl in levels),
           '2: every My Creations cell is of the player\'s level or below (no giant)')
        ck(gseen & custom, f'2: the guest\'s game draws them ({sorted(gseen & custom)})')
        shot(guest, 'custom_cells_guest')
        shot(host, 'custom_cells_host')
        host.call('coop.settings force cell.custom_cells off')
        time.sleep(3)
        before = lin(host)['custom']
        for i in range(6):
            roam(host, guest, k + i)
            time.sleep(6)
        after = lin(host)['custom']
        ck(after == before, f'3: cell.custom_cells off: no new custom swap ({before} -> {after})')
    finally:
        for cmd in ('coop.fix cell.custom_level on', 'coop.settings force cell.custom_cells on', 'coop.lineage share 17'):
            try:
                host.call(cmd)
            except OSError:
                pass
    ck.faults(host=host, guest=guest)
    return ck.result()


if __name__ == '__main__':
    raise SystemExit(main())
