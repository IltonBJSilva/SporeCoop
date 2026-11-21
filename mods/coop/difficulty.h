// Difficulty scaling by party size (BUG-0870; the rules: difficulty_logic.h). The host's session setting
// all.difficulty (default off) scales, with the players of the session now (the host counted):
//  * Creature: the hits of wild animals on the players (npc.cpp damage_filter, host) and the players' hits on wild
//    animals (the enemy's effective health), the own creature's hunger (every machine, its own avatar);
//  * Civilization: the capture effort of a city the players attack (civ.cpp h_AddCapture, host);
//  * Space: the hits of NPC ships and turrets on the own ship (squadron.cpp h_TakeDamage, every machine);
//  * stage progress only where the party's gains count whole in one total (difficulty_logic.h).
// No extra creatures are made. `coop.fix all.difficulty off` switches every part off (the game's own values).
// docs/re/creature.md#creature-difficulty-scaling.
#pragma once

#include <string>

#include "difficulty_logic.h"
#include "net/session.h"
#include "sporekit.h"

namespace difficulty {

void init(const SkHost* host, SkMod* mod, net::Session* session);

// the rules in force now (inactive: off, the fix off, no session, alone)
Rules current();
bool active();

// host, Creature: a wild animal hit a player (the avatar or a partner's puppet) / a player hit a wild animal
float on_hit_player(float damage);
float on_hit_enemy(float damage);
// host, Civilization: capture damage of a player's vehicle on a city
float on_capture(float damage);
// Space: a hit of an NPC ship / turret on this game's own ship
float on_space_hit(float damage);
// a gain of the party's shared DNA / food (dna.cpp after the pace); own_progress / dna_pace: the pace rules in force
float on_progress(float gain, bool own_progress, bool dna_pace);
int on_progress_meal(int amount, bool own_progress, bool dna_pace);

std::string status();
int command(void*, int argc, const char* const* argv, SkOut* out); // coop.difficulty

} // namespace difficulty
