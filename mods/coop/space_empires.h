// Every player's own empire in the Space stage (BUG-9279): space_empires.cpp, the rules in space_empires_logic.h,
// docs/re/space.md#space-own-empires.
#pragma once

#include <cstdint>

namespace spaceempires {

// guest: the host gave this game an empire of its own in this session: its Space money is its own (shared.cpp leaves
// the Space money alone, contracts pay it its own reward)
bool own_money();
// guest: its own empire's party seat, -1: none
int own_seat();
// host: player `player` has an empire of its own: its Space money is not the host's (shared.cpp drops its changes)
bool own_money_player(uint8_t player);
// guest, BUG-9297: its own launch from Civ (civ_launch.cpp) started its game in Space: its player empire gets its own
// captain (`species`, its nation's), name and colour. Call until it answers true (applied in this Space world).
bool local_start(uint32_t species);
// BUG-9310: partners get their own homeworld in Space now (switch space.guest_home and the session setting of the same
// name): start_any asks a guest seated in the host's Space world with no species of its own for one
bool home_asks();
// host, BUG-9310: the species (creature key instance, the host's key) of player `player`'s own empire, 0 = it has none
// or the homeworlds are off (space_allies.cpp gives the partner's mirror empire that species: its colonies' citizens)
uint32_t partner_species(uint8_t player);

} // namespace spaceempires

#include <vector>

#include "wire/space_diplomacy.h"

namespace diplo { // space_diplomacy.cpp
// BUG-9279: my relations with the NPC empires (the records with events, a state or a contact), as a K_SNAP has them
std::vector<wire::space_diplomacy::SnapRec> own_table();
// make my relations these (outside the frame); false: not in Space
bool own_apply(const std::vector<wire::space_diplomacy::SnapRec>& recs);
} // namespace diplo
