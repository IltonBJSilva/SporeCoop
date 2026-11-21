#!/usr/bin/env python3
"""BUG-9306 (Rivals per stage, docs/11-rivals.md#rivals-stages-race): a Rivals stage is a race. The host sets the
stage to Rivals (`coop.rivals set <stage> rivals`, the settings page's row), the guest fills its own bar first: both
games read "Bob won" (the race the host sends, MSG_RIVALS), the host's game stays in the stage while the catch-up timer
runs (shortened to 40 s with `coop.rivals timer`); when it runs out the race is over, the host's own bar is full (its
game's condition met for its native end: the food bar / the brain read from the game), and the host's end button
starts the vote; the guest's I'm ready moves both games on.

--part cell (fixture cell): the food bar (cCellSerializableData +0x1C) of each game; both reach Creature.
--part creature (fixture creature): the brain level; the host's game opens its stage-end editor.

The twin: SPORE_FIX_OFF=core.rivals_stages (every stage reads Together): no winner, the host's bar is not filled, the
host's game stays in the stage after the timer: FAIL. On lane A by hand (Windows, no lab): agentRV/race_cell*.py,
race_cr1.py (the twin), race_cr2.py.
"""
import argparse
import time

from coop_campaign import HISTORY_BTN, HISTORY_OK
from coop_own_progress import captions, click, mode, out, prog, ready, vote_open, wait
from twoinst import Checks, shot, start

TIMER = 40


def race(con):
    return out(con, 'coop.rivals')


def end_button(host, stage):
    """the host's stage end the player's way: History (Cell: the HUD button under the portrait) and its check"""
    if stage == 'cell':
        from coop_own_progress import cell_history_check
        cell_history_check(host)
    else:
        click(host, *HISTORY_BTN, 3)
        click(host, *HISTORY_OK, 2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--part', choices=('cell', 'creature'), default='cell')
    a = ap.parse_args()
    host, guest = start(a.part, own_progress=True)
    ck = Checks()
    stage = 'Cell' if a.part == 'cell' else 'Creature'
    host.call(f'coop.rivals set {a.part} rivals')
    host.call(f'coop.rivals timer {TIMER}')
    ck(wait(lambda: f'{stage}=rivals' in race(guest), 15), f'the guest sees the host\'s table: {stage} = Rivals')
    ph = prog(host)
    # the guest's own bar fills first
    if a.part == 'cell':
        guest.call(f'coop.pool meal {1000 - prog(guest)["food"]}')
    else:
        guest.call('coop.dna gain 3000')
    ck(wait(lambda: prog(guest)['full'] == 1, 90, 2), "the guest's own bar is full")
    captions(guest)
    ck(wait(lambda: 'winner=Bob' in race(host), 30), 'the host\'s game: Bob won the race')
    ck(wait(lambda: 'winner=Bob' in race(guest), 10), 'the guest\'s game: it won the race')
    shot(host, f'rivals_{a.part}_won_host.png')
    time.sleep(TIMER / 2)
    ck(stage in mode(host) and prog(host)['full'] == 0, 'during the timer the host plays on in the stage, its bar its own')
    ck(wait(lambda: 'over=1' in race(host), TIMER + 30, 2), 'the race is over after the catch-up timer')
    t = time.time()
    while time.time() - t < 150 and prog(host)['full'] != 1:
        captions(host, 10)
        time.sleep(2)
    p = prog(host)
    ck(p['full'] == 1, f"the host's bar is full for its native end (food {p['food']}, brain {p['level']}; "
                       f"before {ph['food']}, {ph['level']})")
    captions(host, 30)
    t = time.time()
    while time.time() - t < 120 and not vote_open(host) and stage in mode(host):
        captions(host, 10)
        end_button(host, a.part)
        time.sleep(6)
    ck(vote_open(host), 'the host\'s end button starts the vote')
    ready(guest)
    if a.part == 'cell':
        ck(wait(lambda: 'Creature' in mode(host) or 'Editor' in mode(host), 180), 'the host\'s game leaves Cell')
        ck(wait(lambda: 'Creature' in mode(guest) or 'Editor' in mode(guest), 240), 'the guest\'s game follows')
    else:
        ck(wait(lambda: 'Editor' in mode(host) or 'Tribe' in mode(host), 120), 'the host\'s game opens its editor')
    ck.faults(host=host, guest=guest)
    return ck.result()


if __name__ == '__main__':
    raise SystemExit(main())
