#!/usr/bin/env python3
# suite: none (a manual driver for Windows lanes with 1024x768 windows; the lab has no Civ -> Space guest launch fixture)
"""A guest's own Civ -> Space launch starts with its own captain and empire name (BUG-9297, after BUG-9295).

Both games in a Civ session where the guest has its own nation (the Kealeneon fixture of the batch: the host loaded
the planet and hosts, the guest joined and chose its own creature). The driver takes every other city for the team
(`coop.civ nations capture`), the host's History check starts the spaceship vote, the guest votes; the guest's own
History check opens its own ship editor, it makes a ship and launches its own game into Space. Then, while the host
is still in its own spaceship editor (the host's K_EMPIRE cannot reach the guest yet), the guest's game is checked:
  * its player empire's captain is its own species (`coop.civ launch` species=), its name "<player>'s empire"
    (`coop.space empires`), and the HUD planet panel shows that name (`coop.space empires hud`);
  * `--off` (coop.fix space.own_captain_local off on the guest): the save's (the host's) captain and name: FAILs.

  coop_civ_launch_captain.py --host 5 --guest 6 [--off]
Screenshots next to this file (civ_launch_captain_<on|off>_*.png).
"""
import argparse
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from sporectl import Console  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
fails = []
SEAT = re.compile(r'seat (\d+): owner=(-?\d+) nation=([0-9A-F]{8}) species=([0-9A-F]{8}) cities=(\d+) money=(-?\d+)')


def c(n, line):
    try:
        r = Console(n, 30).call(line)
        return (r.get('out') or r.get('error') or str(r)).strip()
    except Exception as e:  # a console that does not answer: the check that uses it fails
        return f'ERR {e}'


def wait(f, secs, step=2.0):
    t = time.time() + secs
    while time.time() < t:
        try:
            if f():
                return True
        except Exception:
            pass
        time.sleep(step)
    return False


def ck(ok, what):
    print(('  ok   ' if ok else '  FAIL ') + what, flush=True)
    if not ok:
        fails.append(what)
    return ok


def click(n, x, y):
    c(n, f'move {x} {y}')
    time.sleep(0.5)
    c(n, f'click {x} {y}')
    time.sleep(0.6)


def shot(n, name):
    subprocess.run([sys.executable, os.path.join(HERE, '..', 'sporectl.py'), '-i', str(n), 'shot',
                    os.path.join(HERE, name + '.png')], capture_output=True)


def mode(n):
    return c(n, 'game.mode')


def build_ship(n, name):
    """three parts from the palette, a name, Done (the spaceship editor at 1024x768)"""
    for (x, y), (tx, ty) in (((55, 115), (690, 380)), ((135, 115), (690, 430)), ((215, 115), (690, 330))):
        c(n, f'drag {x} {y} {tx} {ty}')
        time.sleep(2)
    click(n, 510, 743)
    time.sleep(1)
    c(n, f'text {name}')
    time.sleep(1)
    c(n, 'key RETURN')
    time.sleep(1)
    click(n, 1000, 745)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--host', type=int, default=5)
    ap.add_argument('--guest', type=int, default=6)
    ap.add_argument('--off', action='store_true')
    a = ap.parse_args()
    H, G, tag = a.host, a.guest, 'off' if a.off else 'on'
    if a.off:
        print(c(G, 'coop.fix space.own_captain_local off'))
    seats = {int(m[0]): m[2] for m in SEAT.findall(c(H, 'coop.civ nations'))}
    ck(len(seats) >= 2, f'host: two player nations ({seats})')
    k = 0
    for i, p in re.findall(r'^city (\d+) \S+ pol=([0-9A-F]{8})', c(H, 'coop.civ'), re.M):
        if p in seats.values():
            continue
        c(H, f'coop.civ nations capture {i} {3 if k == 0 else 0}')
        k += 1
        time.sleep(3)
    ck(wait(lambda: 'the planet is won' in c(H, 'coop.civ nations'), 60), 'the planet is won')
    time.sleep(5)
    click(H, 975, 751)  # History, its check
    time.sleep(4)
    click(H, 997, 745)
    ck(wait(lambda: c(H, 'coop.vote').startswith('vote active'), 120, 4), 'the vote')
    time.sleep(3)
    click(G, 270, 583)
    ck(wait(lambda: 'Editor' in mode(H), 90, 1), 'vote ended, host in its spaceship editor')
    ck(wait(lambda: 'phase=civ' in c(G, 'coop.civ launch'), 60), 'guest: its game shows the planet won')
    time.sleep(3)
    click(G, 975, 751)
    time.sleep(4)
    click(G, 997, 745)
    if not ck(wait(lambda: 'Editor' in mode(G), 40), 'guest: its own spaceship editor'):
        return 1
    time.sleep(8)
    build_ship(G, 'Bob%04d' % (int(time.time()) % 10000))
    ck(wait(lambda: 'Space' in mode(G), 120, 3), 'guest: its own game launches into Space')
    time.sleep(30)
    ck('Edit' in mode(H), 'host: still in its spaceship editor')
    launch = c(G, 'coop.civ launch')
    emp = c(G, 'coop.space empires')
    hud = c(G, 'coop.space empires hud')
    print(launch, emp, hud, sep='\n')
    m = re.search(r'species=([0-9A-F]{8})', launch)
    sp = m.group(1) if m and m.group(1) != '00000000' else None
    ck(sp is not None and f'captain={sp}' in emp, f"guest: its own start's captain is its own species {sp}")
    ck("name='Bob's empire'" in emp, "guest: its own start's empire carries its name")
    ck("name='Bob's empire'" in hud, "guest: the HUD planet panel shows its empire's name")
    shot(G, f'civ_launch_captain_{tag}_guest')
    build_ship(H, 'Hostship')  # the host goes on (the rest is BUG-9295's driver)
    print('RESULT', 'FAIL' if fails else 'PASS', fails)
    return 1 if fails else 0


if __name__ == '__main__':
    raise SystemExit(main())
