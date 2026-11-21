// Difficulty scaling by party size (BUG-0870): see difficulty.h and difficulty_logic.h.
#include "difficulty.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "fixes.h"
#include "game.h"
#include "settings.h"
#include "sporekit_game.hpp"

namespace difficulty {

static const SkHost* H;
static SkMod* M;
static net::Session* S;
static bool g_fix = true;
COOP_FIX("all.difficulty", "BUG-0870", g_fix,
         "all stages: the difficulty scaling by party size (the session setting all.difficulty) applies; off: the game's own values");
static int g_test_players = 0; // tests (coop.difficulty players <n>): the party size, memory only (0: the session's)

struct Count {
    uint32_t n = 0;
    double in = 0, out = 0;
    void add(float a, float b)
    {
        n++;
        in += a;
        out += b;
    }
};
static Count g_to_players, g_to_enemies, g_capture, g_space, g_progress, g_meal, g_hunger;

Rules current()
{
    Rules r;
    if (!g_fix || !S || !S->connected())
        return r;
    r.percent = choice_percent(settings::get(settings::kDifficulty));
    r.players = g_test_players > 0 ? (unsigned)g_test_players : (unsigned)S->players().size();
    return r;
}

bool active() { return current().active(); }

float on_hit_player(float damage)
{
    const Rules r = current();
    if (!r.active() || !(damage > 0) || !std::isfinite(damage))
        return damage;
    float out = r.damage_to_players(damage);
    g_to_players.add(damage, out);
    return out;
}

float on_hit_enemy(float damage)
{
    const Rules r = current();
    if (!r.active() || !(damage > 0) || !std::isfinite(damage))
        return damage;
    float out = r.damage_to_enemies(damage);
    g_to_enemies.add(damage, out);
    return out;
}

float on_capture(float damage)
{
    const Rules r = current();
    if (!r.active() || !(damage > 0) || !std::isfinite(damage))
        return damage;
    float out = r.capture_damage(damage);
    g_capture.add(damage, out);
    return out;
}

float on_space_hit(float damage)
{
    const Rules r = current();
    if (!r.active() || !(damage > 0) || !std::isfinite(damage))
        return damage;
    float out = r.damage_to_players(damage);
    g_space.add(damage, out);
    return out;
}

float on_progress(float gain, bool own_progress, bool dna_pace)
{
    const Rules r = current();
    if (!r.active())
        return gain;
    float out = r.progress_gain(gain, own_progress, dna_pace);
    if (out != gain)
        g_progress.add(gain, out);
    return out;
}

// Cell food points are whole numbers: the remainder waits for the next meal
static float g_meal_rest;
int on_progress_meal(int amount, bool own_progress, bool dna_pace)
{
    const Rules r = current();
    if (!r.active() || amount <= 0)
        return amount;
    float f = r.progress_gain((float)amount, own_progress, dna_pace);
    if (f == (float)amount)
        return amount;
    g_meal_rest += f;
    int out = (int)std::floor(g_meal_rest + 1e-4f);
    g_meal_rest = std::max(0.f, g_meal_rest - (float)out);
    g_meal.add((float)amount, (float)out);
    return out;
}

// ---- hunger (Creature, every machine: its own avatar) ----
// The creature's hunger update (Creature_HungerUpdate 0xC0D5A0) writes the falling meter through Creature_SetHunger
// 0xC0BB30 from 0xC0D66C (return 0xC0D671, SporeCoop::CreatureHungerUpdateReturn); only that write of the own avatar
// is scaled, a meal or the game's other writes are not (Lineages BUG-9100).
using SetHunger_t = void(SK_THISCALL*)(void*, float);
static SetHunger_t o_SetHunger;
static uintptr_t a_HungerUpdateReturn;
static void SK_THISCALL h_SetHunger(void* c, float value)
{
    if (a_HungerUpdateReturn && (uintptr_t)__builtin_return_address(0) == a_HungerUpdateReturn && c &&
        c == game::avatar() && game::in_creature_stage()) {
        const Rules r = current();
        const float previous = skg::field<float>(c, game::kHungerOff);
        if (r.active() && std::isfinite(value) && std::isfinite(previous)) {
            const float v = r.hunger_after(previous, value);
            if (v != value) {
                g_hunger.add(previous - value, previous - v);
                value = v;
            }
        }
    }
    o_SetHunger(c, value);
}

void init(const SkHost* host, SkMod* mod, net::Session* session)
{
    H = host;
    M = mod;
    S = session;
    a_HungerUpdateReturn = H->sym("SporeCoop::CreatureHungerUpdateReturn");
    if (uintptr_t a = H->sym("SporeCoop::Creature_SetHunger"); a && a_HungerUpdateReturn)
        H->hook_create(M, (void*)a, (void*)h_SetHunger, (void**)&o_SetHunger);
}

static std::string line(const char* what, const Count& c)
{
    char b[160];
    snprintf(b, sizeof b, "%s n=%u in=%.2f out=%.2f\n", what, c.n, c.in, c.out);
    return b;
}

std::string status()
{
    const Rules r = current();
    char b[400];
    snprintf(b, sizeof b,
             "difficulty %s fix=%d setting=%d percent=%u players=%u%s enemy_hp=%u%% enemy_dmg=%u%% hunger=%.4f takeover=%u%% "
             "progress=%u%% (shared total, own_progress and dna_pace off only) hunger_hook=%s\n",
             r.active() ? "on" : "off", g_fix ? 1 : 0, settings::get(settings::kDifficulty), r.percent, r.players,
             g_test_players > 0 ? " (test)" : "", r.enemy_health_percent(), r.enemy_damage_percent(), r.hunger_factor(),
             r.takeover_percent(), r.workload_percent(), o_SetHunger ? "on" : "off");
    std::string s = b;
    if (void* av = game::in_creature_stage() ? game::avatar() : nullptr) { // the game's own numbers (tests)
        snprintf(b, sizeof b, "avatar hp=%.3f max=%.3f hunger=%.3f\n", game::health(av), game::max_health(av), game::hunger(av));
        s += b;
    }
    s += line("to_players", g_to_players);
    s += line("to_enemies", g_to_enemies);
    s += line("capture", g_capture);
    s += line("space", g_space);
    s += line("progress", g_progress);
    s += line("meal", g_meal);
    s += line("hunger_drain", g_hunger);
    return s;
}

int command(void*, int argc, const char* const* argv, SkOut* out)
{
    if (argc > 2 && !strcmp(argv[1], "players")) // tests: the party size, memory only (0: the session's)
        g_test_players = std::max(0, std::min(16, atoi(argv[2])));
    else if (argc > 1 && !strcmp(argv[1], "reset"))
        g_to_players = g_to_enemies = g_capture = g_space = g_progress = g_meal = g_hunger = Count{};
    H->out_printf(out, "%s", status().c_str());
    return 0;
}

} // namespace difficulty
