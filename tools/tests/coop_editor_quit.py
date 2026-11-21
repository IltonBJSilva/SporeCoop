#!/usr/bin/env python3
"""No quit to the galaxy menu from a guest's in-stage editor (BUG-9309).

Creature fixture. The game greys out "Exit to Main Menu" in an in-stage editor; a quit message posted there anyway
(coop.menu: the campaign driver of lane B did it after the guest left the session in its own editor) took the game
Editor -> Galaxy menu without the menu's own reset (GalaxyMenu_SimReset): the menu kept the stage's context 0 and the
next load of the host's world faulted (0xC70A20, "cPlanet::GetRingEffect+0x90"). The full replay (leave, rejoin,
the fault) was a manual Windows lane driver (docs/re/space.md#space-editor-quit-menu); this test checks the rule:

  1. The host evolves (`coop.vote evolve`), the guest takes "Evolve now": its own editor opens.
  2. `coop.menu` on the guest is refused: its game stays in the editor.
  3. The red cross leaves the editor; the guest plays its creature again; no fault on either game.

  coop_editor_quit.py                                           (PASS)
  SPORE_FIX_OFF=world.no_editor_quit coop_editor_quit.py        (step 2 FAILS: the guest's game goes to the menu)
"""
import time

from harness import mode, waiter
from coop_ui import Ui
from twoinst import Checks, shot, start

DONE = (1416, 1062)    # the creature editors' green check (1440x1080 test window)
CANCEL = (1383, 1062)  # the red cross

wait = waiter(step=1.0)


def in_editor(con):
    return 'Editor' in mode(con)


def back(con, m0, secs=60):
    return wait(lambda: mode(con) == m0 and con.call('coop.avatar')['ok'], secs)


def main():
    host, guest = start()
    ck = Checks()
    gu = Ui(guest, 'guest')
    time.sleep(3)
    m0 = mode(host)

    # 1. the guest's own editor
    print(' ', host.call('coop.vote evolve')['out'].strip().splitlines()[0], flush=True)
    st = gu.wait(lambda s: s.get('open') == '1' and s['ready']['vis'], 20, 'the offer')
    if st:
        gu.click('ready')
    ck(wait(lambda: in_editor(guest), 90), f"the guest's own editor opens ({mode(guest).strip()})")
    time.sleep(8)

    # 2. no quit to the galaxy menu from the editor
    r = guest.call('coop.menu')
    print('  guest coop.menu:', (r.get('out') or r.get('error') or '').strip(), flush=True)
    time.sleep(8)
    ck(in_editor(guest), f"the guest's game stays in its editor ({mode(guest).strip()})")
    shot(guest, 'editor_quit.png')

    # 3. the editor left with its red cross
    if in_editor(guest):
        guest.call('click %d %d' % CANCEL)
        if not back(guest, m0, 20):
            guest.call('click %d %d' % DONE)
    ck(back(guest, m0), f"the guest plays its creature again ({mode(guest).strip()})")
    if in_editor(host):
        host.call('click %d %d' % DONE)
    back(host, m0)
    ck.faults(host=host, guest=guest)
    return ck.result()


if __name__ == '__main__':
    raise SystemExit(main())
