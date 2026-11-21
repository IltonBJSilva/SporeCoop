// Rivals in every stage (BUG-9306, docs/11-rivals.md 11.2 and 11.5; the rules and the wire: rivals_logic.h).
//
// * The stage table: the session settings rivals.cell .. rivals.space (Together, the default, or Rivals) and
//   rivals.catch_up, set by the host in the co-op window's settings page (F9 > Settings > Rivals) or with
//   `coop.settings set rivals.<stage> rivals`, kept with the host's world, shown to the guests (MSG_SETTINGS).
// * The race of a Rivals stage (host): every second the host checks every player's condition (Cell, Creature: the own
//   bar of own_progress; Tribe: NPC tribes allied with the player's village; Civilization: NPC cities the player's
//   nation took). The first done wins the stage, the catch-up timer starts; the stage may end when the timer runs out
//   or everybody is done (`over`). Everybody sees it (MSG_RIVALS, notices, the status block, the co-op window).
// * The stage's end (own_progress.cpp, vote.cpp, tribe, civ): Together waits for every player; Rivals waits for the
//   race to be over. Then the host's game ends the stage natively (its own condition is met for it when it was not:
//   the host's game is the one that changes stage) and the existing vote moves everybody on.
// * Space: rivals.space is the Space rivals mode of space_rivalry.cpp (the host's switch; the F9 Space page's button
//   and the setting follow each other).
// * `coop.fix core.rivals_stages off`: every stage reads Together (0.7.15.96), the twin of the tests.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "net/buffer.h"
#include "net/session.h"
#include "rivals_logic.h"
#include "sporekit.h"

namespace rivals {

struct Ctx {
    net::Session* sess = nullptr;
    std::function<void(const std::string&)> feed; // this player's notification
};

void init(const SkHost* host, SkMod* mod, const Ctx& ctx);
void tick(double now);
void on_message(uint8_t from, net::Reader& r);
void on_session_end();

// the table: stage `s` is a Rivals stage (the setting; false with the switch off)
bool stage_rivals(uint8_t s);
// the stage this game plays now is a Rivals stage in a session of two or more players (co-op aids read off)
bool here();
// the race of game mode `mode` runs in this session (a Rivals stage, two or more players)
bool race_on(uint32_t mode);
// the race of `mode` is over: the stage may end (false while it runs or before a winner); `why` says what is left
bool race_over(uint32_t mode, std::string* why = nullptr);
// the race's line for the HUD and the co-op window ("" when no race runs)
std::string line();

// Tribe / Civilization (host): the counts of the stage's condition, filled by tribe_villages.cpp / civ_nations.cpp.
// won[i]: what player ids[i] has; total: the NPC tribes / cities of the condition; free_left: not taken by anybody
using CountFn = std::function<bool(std::vector<uint8_t>& ids, std::vector<int>& won, int& total, int& free_left)>;
void set_counter(uint8_t stage, CountFn fn);
// Tribe / Civilization (host): the native end of the stage once the race is over (set by tribe / civ)
void set_ender(uint8_t stage, std::function<bool()> fn);

std::string status();
int command(int argc, const char* const* argv, std::string& out); // coop.rivals

} // namespace rivals
