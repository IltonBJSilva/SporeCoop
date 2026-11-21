#!/usr/bin/env python3
"""BUG-0870: difficulty scaling by party size (the session setting all.difficulty, default off), Creature.

What each game shows is read from the game: an animal's health on the host's game and on the guest's proxy
(`coop.npc list` hp=), each player's own creature's health (`coop.difficulty` avatar hp=, game::health of the own avatar).
With two players at 400 % an enemy has 160 % health (a player's hit takes 1 / 1.6 of it) and hits 120 % harder.
  1. the guest plays with the host's value (`coop.difficulty` on the guest: enemy_hp=160%);
  2. the host's hit of 3 on a wild animal takes 1.875 of its health on the host, the guest's proxy shows the same;
  3. the guest's hit of 3 (MSG_DAMAGE_REQ, applied by the host) takes 1.875 on the host's game, the guest's proxy too;
  4. an animal's bite of 3 (`coop.npc npchit`, the game's TakeDamage with the animal as attacker) takes 3.6 of the
     guest's own creature on the guest's game and 3.6 of the host's creature;
  5. no faults.
The partners' healing (creature.pack_heal) is off during the test: it refills a bite within a second.
--old (`coop.fix all.difficulty off` on both, the twin `difficulty_off`): the game's own values: every step fails.

  SPORE_LANE=N python3 tools/tests/coop_difficulty.py [--old]
"""
import re
import sys
import time

from twoinst import Checks, shot, start

D = 3.0


def out(con, cmd):
    r = con.call(cmd)
    return r.get('out') or r.get('error') or ''


def npc_hp(con, nid):
    m = re.search(rf'^\s+#{nid} .*?\bhp=([0-9.]+)', out(con, 'coop.npc list'), re.M)
    return float(m.group(1)) if m else None


def wild_npcs(host):
    """ids of the host's live wild animals (not the host's family or pack) with health for two hits"""
    ids = []
    for ln in out(host, 'coop.npc list').splitlines():
        m = re.match(r'\s+#(\d+) \S+ species=\S+ hp=([0-9.]+) ', ln)
        if m and float(m.group(2)) >= 4 and ' DEAD' not in ln and ' family' not in ln and ' pack' not in ln:
            ids.append(m.group(1))
    return ids


def av_hp(con):
    m = re.search(r'avatar hp=([0-9.]+) max=', out(con, 'coop.difficulty'))
    return float(m.group(1)) if m else None


def bite(host, con, npc, player):
    """npc bites `player`; the first reading of that player's own creature that moved (they regenerate)"""
    b = av_hp(con)
    out(host, f'coop.npc npchit {npc} {player} {D}')
    t, a = time.time(), b
    while time.time() - t < 3:
        a = av_hp(con)
        if a is not None and b is not None and a < b - 0.05:
            break
        time.sleep(0.05)
    return (b - a) if a is not None and b is not None else 0.0


def main():
    old = '--old' in sys.argv
    host, guest = start('creature')
    ck = Checks()
    try:
        if old:
            for con in (host, guest):
                con.call('coop.fix all.difficulty off')
        out(host, 'coop.settings set creature.pack_heal off')
        print('  ' + out(host, 'coop.settings set all.difficulty 6'), flush=True)
        time.sleep(2)
        g = out(guest, 'coop.difficulty')
        print('  guest: ' + g.splitlines()[0] if g else '  guest: -', flush=True)
        ck('enemy_hp=160%' in g, '1: the guest plays with the host\'s value (enemy health 160 %)')
        ids = wild_npcs(host)
        print(f'  wild animals: {ids}', flush=True)
        ck(len(ids) >= 3, 'three wild animals near the players')
        if len(ids) >= 3:
            want = D / 1.6
            for who, con, nid in (('host', host, ids[0]), ('guest', guest, ids[1])):
                b = npc_hp(host, nid)
                out(con, f'coop.npc hit {nid} {D}')
                time.sleep(1.5)
                a, ag = npc_hp(host, nid), npc_hp(guest, nid)
                print(f'  {who} hits #{nid}: host {b} -> {a}, the guest\'s proxy {ag}', flush=True)
                ck(b is not None and a is not None and abs((b - a) - want) < 0.05,
                   f'{"2" if who == "host" else "3"}: the {who}\'s hit of {D} took {want:.3f} of the animal\'s health on the host\'s game')
                ck(ag is not None and a is not None and abs(ag - a) < 0.05, f'the guest\'s game shows the same health ({ag})')
            for who, con, p in (('guest', guest, 1), ('host', host, 0)):
                drops = []
                for _ in range(3):
                    drops.append(bite(host, con, ids[2], p))
                    time.sleep(7)
                print(f'  bites on the {who}: {["%.3f" % x for x in drops]}', flush=True)
                ck(abs(max(drops) - D * 1.2) < 0.2, f'4: a bite of {D} took {D * 1.2:.2f} of the {who}\'s own creature on its own game')
        shot(guest, 'difficulty_guest')
    finally:
        for cmd in ('coop.settings set all.difficulty 0', 'coop.settings set creature.pack_heal on'):
            try:
                host.call(cmd)
            except OSError:
                pass
        for con in (host, guest):
            try:
                con.call('coop.fix all.difficulty on')
            except OSError:
                pass
    ck.faults(host=host, guest=guest)
    return ck.result()


if __name__ == '__main__':
    raise SystemExit(main())
