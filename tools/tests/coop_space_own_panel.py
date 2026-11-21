#!/usr/bin/env python3
"""The Space HUD's planet panel after a guest's own empire (BUG-9296, space_empires.cpp,
docs/re/space.md#space-hud-planet-panel).

The square beside the planet map is the planet panel (its citizens' picture and its owner empire's name), not the
captain. On the guest's game the player empire becomes "<player>'s empire" (BUG-9279); the panel kept the name it was
built with (the save's) until the next planet. Like coop_space_own_empires.py the host founds the guest's empire with
the test seam `coop.space empires found`.

1. The guest's own empire is applied (its name on its game).
2. The guest's HUD planet panel shows that name at once (the game's own caption 0x044D94D1, `coop.space empires hud`).
3. The host's panel keeps the host's empire name.

  SPORE_LANE=4 python3 tools/tests/coop_space_own_panel.py

SPORE_FIX_OFF=space.own_empire_panel: the guest's panel keeps the save's name (FAIL at check 2).
"""
import re
import time

from coop_space_diplomacy import space_planet
from harness import sp, waiter
from twoinst import Checks, start

FIXTURE = 'space_empire'
wait = waiter(timeout=30, step=1.0, as_bool=True)


def panel(con):
    m = re.search(r"hud name='([^']*)'", sp(con, 'empires hud'))
    return m.group(1) if m else None


def name(con):
    m = re.search(r"name='([^']*)'", sp(con, 'empires'))
    return m.group(1) if m else None


def main():
    host, guest = start(FIXTURE, planet=space_planet())
    ck = Checks()
    time.sleep(4)
    host_panel = panel(host)
    print('  panels before:', host_panel, '/', panel(guest))
    gid = re.search(r'client id=(\d+)', guest.call('coop.status')['out'])
    print('  host:', sp(host, 'empires found', gid.group(1) if gid else 1, '7CA81684'))
    ok = wait(lambda: (name(guest) or '').endswith("'s empire"), 60)
    print('  guest:', sp(guest, 'empires').splitlines()[0])
    ck(ok, "guest: its own empire's name on its game (1)")
    time.sleep(3)
    print('  panels after:', panel(host), '/', panel(guest))
    ck(ok and panel(guest) == name(guest), f"guest: its HUD planet panel shows '{name(guest)}' ({panel(guest)}) (2)")
    ck(panel(host) == host_panel, f'host: its panel keeps {host_panel} ({panel(host)}) (3)')
    return ck.result()


if __name__ == '__main__':
    raise SystemExit(main())
