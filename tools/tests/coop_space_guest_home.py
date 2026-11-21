#!/usr/bin/env python3
"""A partner new to the host's Space world gets its own empire with its own homeworld near the host's (BUG-9310,
space_empires.cpp, start_any.cpp, docs/re/space.md#space-guest-home).

1. The guest has (or chooses) a species of its own: asked by the "Your own start" page when its seat has none (it
   picks its first My Creations creature and four cards), or told to play the creature of its record; the host founds
   its empire (`coop.space empires`: guest own=1).
2. Its homeworld: the host picked a planet of a free star system near its home star (`coop.space empires home`: a
   distance > 0), the guest's game made it a colony of its own empire (mine=1) whose citizens are its own species (the
   star's species key = its captain).
3. The host sees it: the planet's star is the guest's mirror empire's on the host's game, with the guest's species as
   its citizens (the game's own star record, not a counter of the mod).
4. The guest at its homeworld: the game's HUD planet panel shows its species (`coop.space empires hud`:
   picture_is_captain=1).

  SPORE_LANE=4 python3 tools/tests/coop_space_guest_home.py

SPORE_FIX_OFF=space.guest_home: no question, no empire for a guest without a species, no homeworld (FAIL at check 1 or 2).
"""
import re
import time

from coop_space_diplomacy import space_planet
from harness import sp, waiter
from twoinst import Checks, start

FIXTURE = 'space_empire'
wait = waiter(timeout=30, step=1.0, as_bool=True)


def main():
    host, guest = start(FIXTURE, planet=space_planet())
    ck = Checks()
    time.sleep(6)
    creatures = re.findall(r'^creature .* (\w{8})$', guest.call('coop.startany creatures')['out'], re.M)
    if wait(lambda: 'prompt=4' in guest.call('coop.startany')['out'], 20) and creatures:
        print('  guest pick:', guest.call(f'coop.startany pick {creatures[0]} 0 1 2 0')['out'].strip())
    elif 'guest own=1' not in sp(guest, 'empires') and creatures:  # a record without its creature in this world
        print('  guest: no question (a record of this planet?):', guest.call('coop.startany')['out'].splitlines()[2])
    ck(wait(lambda: 'guest own=1' in sp(guest, 'empires'), 90), 'guest: an empire of its own (1)')
    ck(wait(lambda: 'mine=1' in sp(guest, 'empires home'), 60), 'guest: its homeworld is a colony of its own empire (2)')
    gh = sp(guest, 'empires home')
    print('  guest:', gh.strip())
    m = re.search(r'planet=(\w+) star=(\d+) .* citizens=(\w+)', gh)
    cap = re.search(r'captain=(\w+)', sp(guest, 'empires'))
    ck(m and cap and m.group(3) == cap.group(1) and cap.group(1) != '00000000',
       f'guest: its homeworld\'s citizens are its own species ({m.group(3) if m else "-"}) (2)')
    hsp = re.search(r'empire seat=\d+ name=\S+ species=(\w+)', sp(host, 'empires'))
    hsp = hsp.group(1) if hsp else '?'
    ok = wait(lambda: re.search(r'dist=[1-9][\d.]* owner=(?!FFFFFFFF)\w+ citizens=' + hsp, sp(host, 'empires home')), 60)
    print('  host:', sp(host, 'empires home').strip())
    ck(ok, 'host: the guest\'s homeworld star is its mirror empire\'s, a few systems away, with its species (3)')
    if m:
        star = m.group(2)
        ck(re.search(r"owner='[^']*'", sp(host, 'allies star', star)) is not None, 'host: the star has an owner empire (3)')
        print('  guest jump:', sp(guest, 'jumpto', star))
        ck(wait(lambda: 'picture_is_captain' in sp(guest, 'empires hud') and '=1' in sp(guest, 'empires hud').split(
            'picture_is_captain')[1], 60), 'guest: at its homeworld the HUD shows its own species (4)')
    return ck.result()


if __name__ == '__main__':
    raise SystemExit(main())
