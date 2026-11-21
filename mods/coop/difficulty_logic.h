// Difficulty scaling by party size (BUG-0870, ported from Lineages BUG-9100 founders::CampaignRules; the session setting
// all.difficulty, default off). The rules only: portable (no game calls), checked by tests/validate_tests.cpp.
//
// Every extra player (the players in the session over one) adds a per-player extra, multiplied by the setting's
// percent and rounded once over all players (Lineages model 1): enemy health +15 %, enemy damage +5 %, hunger +2.5 %,
// city takeover effort +5 %, stage progress +60 %. Two players at 100 %: enemies 115 % health, 105 % damage.
// Off (the default): 100 % everywhere, the game's own values.
//
// Differences from Lineages (docs/re/creature.md#creature-difficulty-scaling):
//  * the players are the players of the session now (Lineages: the campaign's reserved seats; its BUG-9199 moved Space
//    to the party present for the same reason). Nothing is saved: the setting is the host's, the values follow the
//    party as players come and go.
//  * no extra spawns (Lineages: an extra Cell predator per group from three seats, one Space raider per seat): the
//    creature counts stay a single player's (BUG-9292: the lag came from more cells).
//  * enemy health is the players' damage to an enemy divided by the health percent (the same number of hits as a
//    health bar that much longer), not a bigger max health: nothing to convert before a save, nothing new on the wire.
//  * stage progress applies only where the party's gains enter one total whole (all.own_progress off and
//    all.dna_pace off): own progress and the single-player pace already give each player a solo game's pace, so a
//    second scaling there would count the party twice.
#pragma once

#include <cstdint>

namespace difficulty {

// the setting's choices (settings_core.h names "off|50 %|100 %|150 %|200 %|300 %|400 %")
constexpr int kChoices = 7;
inline unsigned choice_percent(int choice)
{
    static const unsigned p[kChoices] = {0, 50, 100, 150, 200, 300, 400};
    return choice >= 0 && choice < kChoices ? p[choice] : 0;
}
constexpr unsigned kMaxPercent = 400;

// per extra player at 100 % (Lineages' defaults, coop.ini [campaign] there)
constexpr unsigned kHealthPerPlayer = 15, kDamagePerPlayer = 5, kTakeoverPerPlayer = 5, kProgressPerPlayer = 60;
constexpr unsigned kHungerBasisPerPlayer = 250; // 100 basis points = 1 %

struct Rules {
    unsigned players = 1; // in the session now, the host counted (1..)
    unsigned percent = 0; // the setting (0 = off)

    bool active() const { return percent > 0 && players > 1; }
    // the whole extra of the players over one, rounded to the nearest unit
    uint32_t extra_total(uint32_t per_player) const
    {
        const uint64_t over = players > 1 ? players - 1u : 0u;
        const unsigned pct = percent > kMaxPercent ? kMaxPercent : percent;
        return uint32_t((over * per_player * pct + 50u) / 100u);
    }
    uint32_t enemy_health_percent() const { return 100u + extra_total(kHealthPerPlayer); }
    uint32_t enemy_damage_percent() const { return 100u + extra_total(kDamagePerPlayer); }
    uint32_t takeover_percent() const { return 100u + extra_total(kTakeoverPerPlayer); }
    uint32_t workload_percent() const { return 100u + extra_total(kProgressPerPlayer); }
    float hunger_factor() const { return 1.f + extra_total(kHungerBasisPerPlayer) / 10000.f; }

    // the multipliers the hooks apply
    float damage_to_players(float d) const { return d > 0 ? d * enemy_damage_percent() / 100.f : d; }
    float damage_to_enemies(float d) const { return d > 0 ? d * 100.f / enemy_health_percent() : d; }
    float capture_damage(float d) const { return d > 0 ? d * 100.f / takeover_percent() : d; }
    // stage progress: only where the gains of the party count whole in one total (see the header)
    float progress_gain(float g, bool own_progress, bool dna_pace) const
    {
        return g > 0 && !own_progress && !dna_pace ? g * 100.f / workload_percent() : g;
    }
    // hunger: a falling food meter falls this much faster (rising: a meal, untouched)
    float hunger_after(float previous, float next) const
    {
        return next < previous && previous > 0 ? previous + (next - previous) * hunger_factor() : next;
    }
};

} // namespace difficulty
