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

5. After the host's saves its game still has the alliance with the guest's empire (BUG-9312: the game's flags, `coop.space
   diplomacy rel <pol>`).

6. (BUG-9315) The homeworld stays a colony in the guest's game (its planet record: tech 5, not only the star) and the
   host's allies mirror has that planet (`coop.space allies list`: `  planet <id>`, it skips nothing of the guest's but
   the host's own homeworld, which the guest's game shares).

SPORE_FIX_OFF=space.guest_home_keep: the game turns the homeworld made in the tick its star's planets were first
generated back into a plain planet; the guest's roster and the host's mirror miss it (FAIL at check 6; lab lane A
2026-11-21: in every run whose host store did not hold that colony already).
SPORE_FIX_OFF=space.guest_home: no question, no empire for a guest without a species, no homeworld (FAIL at check 1 or 2).
SPORE_FIX_OFF=diplomacy.mirror_ally: the game's attitude pass breaks the alliance made again after a save (check 5; seen
in a reloaded world, not every first session: docs/re/space.md#space-mirror-ally-break).
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
    if m:  # 6 (BUG-9315)
        planet, star = m.group(1), m.group(2)
        time.sleep(15)
        rec = lambda: re.search(r'^  planet ' + planet + r" '[^']*' tech=(\d+)", sp(guest, 'allies star', star), re.M)
        ck(wait(lambda: rec() and rec().group(1) == '5', 30), f'guest: its homeworld {planet} is still a colony (tech 5) (6)')
        ck(wait(lambda: re.search(r'^  planet ' + planet, sp(host, 'allies list'), re.M), 60),
           f"host: the allies mirror has the guest's homeworld {planet} (6)")
        home0 = re.search(r' home \d+/(\w{8})', sp(host, 'map roster 0'))
        skipped = re.findall(r'^  skipped (\w{8}):', sp(host, 'allies list'), re.M)
        print('  host: mirror skips', skipped, 'host home', home0.group(1) if home0 else '-')
        ck(all(home0 and p == home0.group(1) for p in skipped),
           "host: the mirror skips none of the guest's planets but the host's own homeworld (6)")
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
    # 5 (BUG-9312): the host's saves undo the mirror and make it again; the game's attitude pass broke the alliance made
    # again ("Your alliance with the Bob's empire has been severed."). The game's own flags of the pair stay allied.
    # The pass checks the empires near the player: the host flies into a star system of the mirror first.
    al = sp(host, 'allies list')
    own = re.search(r'mirror .* pol=(8\w+) applied=1', al)
    mst = re.search(r'^  star (\d+)', al, re.M)
    if own and mst:
        rel = lambda: host.call(f'coop.space diplomacy rel {own.group(1)}')['out']
        print('  host jump:', sp(host, 'jumpto', mst.group(1)))
        time.sleep(30)
        ck('flags=2' in rel(), 'host: in a star system of the guest\'s empire the game keeps the alliance (5)')
        for _ in range(2):
            print('  host save:', host.call('coop.save')['out'].strip())
            time.sleep(20)
            print('  host:', rel().strip())
        ck('flags=2' in rel(), 'host: after its saves the game keeps the alliance with the guest\'s empire (5)')
    else:
        ck(False, 'host: the guest\'s empire is mirrored (allies list) (5)')
    return ck.result()


if __name__ == '__main__':
    raise SystemExit(main())
