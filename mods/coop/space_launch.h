// A partner's own spaceship when it joins a game that is already in Space (BUG-9299): space_launch.cpp, part of the
// Space feature "empires" (MSG_SPACE kind 105 K_LAUNCH), docs/re/space.md#space-late-launch.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "space_ext.h"
#include "wire/space_empires.h"

namespace spacelaunch {

// what space_empires.cpp tells the launch on a guest (every frame while the feature is active)
struct Me {
    bool own = false;      // the host gave this game an empire of its own (its K_EMPIRE answered, this Space world)
    bool settled = false;  // that empire carries this player's name, ship and captain on this game (name_apply ran)
    uint32_t serial = 0;   // the K_EMPIRE serial answered
    uint32_t ship = 0;     // the player empire's spacecraft key now (cEmpire +0xB8)
    uint32_t captain = 0;  // its captain key now (cEmpire +0xC4)
};

extern bool g_fix; // space.late_launch
void guest_tick(double now, const Me& me);
void host_message(uint8_t origin, const wire::space_empires::Launch& m);
void on_session_end();
std::vector<spaceext::Row> rows(); // a guest's offer on the Space page; the host's list of partners' launches
std::string status();
// `coop.space empires launch [go | no | offer | open <hex return mode>]`
std::string debug(const std::vector<std::string>& args);

} // namespace spacelaunch

namespace spaceempires {
// guest: the newest own ship design is flown again (a new design was saved): the empire's name, ship and captain are
// set again on the next tick
void ship_changed();
} // namespace spaceempires
