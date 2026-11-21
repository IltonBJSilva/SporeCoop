#!/usr/bin/env python3
# suite: none (a manual driver: it needs a game it starts itself, at boot; Windows lanes)
"""The Space test commands outside Space (BUG-9300).

`coop.space takeoff` / `coop.space land` sent while a game boots or sits in the galaxy menu called GetCurrentContext
without a simulator and crashed the game. Started here: the game, `takeoff` a few seconds after the start, then
`takeoff` and `land` at the galaxy menu: each answers "not in Space" and the game still answers.

  coop_space_cmd_guard.py --exe E:\\SporeLaneC\\SporebinEP1\\SporeApp.exe --instance 5 --profile dev5
The game is left running (end it with the lane's own tools).
"""
import argparse
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
from sporectl import Console  # noqa: E402

fails = []


def c(n, line):
    try:
        r = Console(n, 30).call(line)
        return (r.get('out') or r.get('error') or str(r)).strip()
    except Exception as e:  # a console that does not answer: the check that uses it fails
        return f'ERR {e}'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', required=True)
    ap.add_argument('--instance', type=int, default=5)
    ap.add_argument('--profile', default='dev5')
    a = ap.parse_args()
    n = a.instance
    subprocess.Popen([a.exe, f'-sporekit-instance:{n}', f'-sporekit-profile:{a.profile}'],
                         cwd=os.path.dirname(a.exe))
    time.sleep(6)
    boot = c(n, 'coop.space takeoff')
    t = time.time() + 240
    while time.time() < t and 'GalaxyGameEntry' not in c(n, 'game.mode'):
        time.sleep(3)
    menu = [c(n, 'coop.space takeoff'), c(n, 'coop.space land')]
    time.sleep(3)
    alive = 'GalaxyGameEntry' in c(n, 'game.mode')
    print('boot:', boot, '| menu:', menu, '| alive:', alive)
    for what, ok in (('takeoff while booting', 'not in Space' in boot or boot.startswith('ERR')),
                     ('takeoff / land at the menu answer "not in Space"', all('not in Space' in r for r in menu)),
                     ('the game lives', alive)):
        print(('  ok   ' if ok else '  FAIL ') + what)
        if not ok:
            fails.append(what)
    print('RESULT', 'FAIL' if fails else 'PASS', fails)
    return 1 if fails else 0


if __name__ == '__main__':
    raise SystemExit(main())
