#!/usr/bin/env python3
"""A partner who joins a game already in Space designs its own spaceship (BUG-9299, space_launch.cpp,
docs/re/space.md#space-late-launch).

The lab's Space fixtures have no guest on a species of its own, so the host founds the guest's empire with the test seam
`coop.space empires found` (as coop_space_own_empires.py does).

1. The guest, with its own empire in the host's Space world, gets the offer (`coop.space empires launch`: phase offered;
   the host's record: state 1).
2. Design: the game's spaceship editor opens on the guest (the Editor mode) and returns to Space with the editor's red
   cross: no new design, the offer stays.
3. Design again while the guest's game stands 40 s right after it (`kit.test.hang`, as a slow first editor load on a
   busy PC, BUG-9305): the host keeps the guest in its session, the editor opens. Three parts and a name, the check
   mark: back in Space the guest's empire flies the new design (cEmpire +0xB8 = the newest own ship design, a key it did
   not have before) with its own captain (the species the host founded).
4. The host: the partner's ship is the new design in its game (its creations registry `player <id> ufo` names the
   design, the guest's mirror ship in its fleet has that model) and its launch record says done with the guest's ship
   and captain.

  SPORE_LANE=4 python3 tools/tests/coop_space_late_launch.py

SPORE_FIX_OFF=space.late_launch: no offer (FAIL at check 1).
SPORE_FIX_OFF=space.launch_long_wait: the host drops the guest during its 40 s stand (FAIL at check 3).
"""
import re
import time

from coop_space_diplomacy import space_planet
from harness import sp, waiter
from twoinst import Checks, start

FIXTURE = 'space_empire'
wait = waiter(timeout=30, step=1.0, as_bool=True)
SPECIES = '7CA81684'


def launch(con):
    return sp(con, 'empires launch')


def ship_and_captain(con):
    m = re.search(r'ship=(\w+) captain=(\w+)', sp(con, 'empires'))
    return (m.group(1), m.group(2)) if m else (None, None)


def ships(con):
    return re.findall(r'^ship (\w+) ', sp(con, 'empires ships'), re.M)


def build(guest, name):
    """three hull parts from the palette onto the editor's ring, the name, the check mark (1024x768)"""
    for (x, y), (tx, ty) in (((55, 115), (690, 380)), ((135, 115), (690, 430)), ((215, 115), (690, 330))):
        guest.call(f'drag {x} {y} {tx} {ty}')
        time.sleep(2)
    guest.call('click 510 743')
    time.sleep(1)
    guest.call(f'text {name}')
    time.sleep(1)
    guest.call('key RETURN')
    time.sleep(1)
    guest.call('click 1000 745')


def editor(con):
    return 'Edit' in con.call('game.mode')['out']


def space(con):
    return 'Space' in con.call('game.mode')['out']


def main():
    host, guest = start(FIXTURE, planet=space_planet())
    ck = Checks()
    time.sleep(4)
    gid = re.search(r'client id=(\d+)', guest.call('coop.status')['out'])
    gid = gid.group(1) if gid else '1'
    print('  host:', sp(host, 'empires found', gid, SPECIES))
    ck(wait(lambda: 'guest own=1' in sp(guest, 'empires'), 60), 'guest: its own empire')
    print('  guest:', sp(guest, 'empires launch offer'))  # a lab profile may keep an older choice of this planet
    ok = wait(lambda: 'phase=offered' in launch(guest), 60)
    ck(ok, 'guest: offered to design its own spaceship (1)')
    ck(wait(lambda: re.search(rf'launch player={gid} .*state=1', launch(host)), 20), 'host: told of the offer (1)')
    if not ok:
        print('  guest:', launch(guest).splitlines()[0])
        return ck.result()
    before = set(ships(guest))
    print('  guest:', sp(guest, 'empires launch go'))
    ck(wait(lambda: editor(guest), 40), 'guest: its spaceship editor opens from Space (2)')
    time.sleep(8)
    guest.call('click 968 748')  # the editor's red cross: no design
    ck(wait(lambda: space(guest) and 'phase=offered' in launch(guest) and 'cancelled=1' in launch(guest), 60),
       'guest: back in Space without a design, the offer stays (2)')
    print('  guest:', sp(guest, 'empires launch go'))
    guest.call('kit.test.hang 40000')  # BUG-9305: the guest's game stands 40 s before its editor loads
    time.sleep(45)
    ck(re.search(r'^\s+\[\d+\] \S+ .*rtt=', host.call('coop.status')['out'], re.M) is not None,
       "host: the guest is still in its session after the guest's 40 s stand (3)")
    ck(wait(lambda: editor(guest), 60), 'guest: its spaceship editor opens again (3)')
    time.sleep(8)
    name = 'Lab%04d' % (int(time.time()) % 10000)
    build(guest, name)
    ck(wait(lambda: space(guest) and 'done=1' in launch(guest), 120), f"guest: its design '{name}' saved, back in Space (3)")
    new = [s for s in ships(guest) if s not in before]
    ck(len(new) == 1, f'guest: one new own ship design ({new}) (3)')
    ck(wait(lambda: new and ship_and_captain(guest) == (new[0], SPECIES), 30),
       f'guest: its empire flies its new design with its own captain ({ship_and_captain(guest)}) (3)')
    ck(wait(lambda: re.search(rf"player {gid} ufo: \w+/\w+/\w+ '{name}'", host.call('coop.creations')['out']), 60),
       f"host: the guest's ship is its design '{name}' in the host's game (4)")
    ck(wait(lambda: re.search(rf'launch player={gid} .*state=3 ship={new[0] if new else "-"} captain={SPECIES}',
                              launch(host)), 30), "host: the guest's launch is done with its ship and captain (4)")
    m = re.search(rf"player {gid} ufo: (\w+)/", host.call('coop.creations')['out'])
    if m:
        print('  host fleet:', [l for l in host.call('coop.fleet list')['out'].splitlines() if 'puppet' in l])
    return ck.result()


if __name__ == '__main__':
    raise SystemExit(main())
