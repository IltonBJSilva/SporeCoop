// Rivals in every stage (BUG-9306, docs/11-rivals.md 11.2 and 11.5): the stage table (Together / Rivals per stage, in
// the session settings: settings_core.h rivals.*) and the race of a Rivals stage: the first player to meet the stage's
// condition wins it, a catch-up timer runs for the others, the stage ends when the timer runs out or every player is
// done; then everybody moves on together (the existing vote). "Together" (the default) keeps the 0.7.15.9x rule: the
// stage ends when every player is done.
//
// The rules and the wire format, portable (no game calls) so that `make test` checks them (tests/validate_tests.cpp).
// The game side is rivals.cpp.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "net/buffer.h"

namespace rivals {

// the stages of the table (0: none, a menu, an editor)
enum Stage : uint8_t { kNone = 0, kCell = 1, kCreature = 2, kTribe = 3, kCiv = 4, kSpace = 5 };

// the game's mode ids (modes.h is not portable)
inline Stage stage_of_mode(uint32_t m)
{
    switch (m) {
    case 0x1654C00: return kCell;
    case 0x1654C01: return kCreature;
    case 0x1654C02: return kTribe;
    case 0x1654C04: return kCiv;
    case 0x1654C05: return kSpace;
    }
    return kNone;
}

inline const char* stage_name(uint8_t s)
{
    static const char* n[] = {"-", "Cell", "Creature", "Tribe", "Civilization", "Space"};
    return s <= kSpace ? n[s] : "?";
}

inline const char* next_stage_name(uint8_t s)
{
    static const char* n[] = {"-", "Creature", "Tribe", "Civilization", "Space", "-"};
    return s <= kSpace ? n[s] : "?";
}

// the catch-up timer choices of rivals.catch_up (settings_core.h: "1 min|2 min|3 min|5 min|10 min")
inline int catch_up_seconds(int choice)
{
    static const int s[] = {60, 120, 180, 300, 600};
    return choice >= 0 && choice < 5 ? s[choice] : 300;
}

// one player in the race now (the host fills it every second)
struct Racer {
    uint8_t id = 0;
    bool done = false; // meets the stage's condition now (Cell: food 1000, Creature: brain 4, Tribe / Civ: > half)
    uint8_t pct = 0;   // progress towards it in percent (the HUD line)
    bool operator==(const Racer&) const = default;
};

// the race of one stage (the host's copy is the authority; a guest keeps the last one the host sent)
struct Race {
    uint8_t stage = kNone;
    uint8_t winner = 0xFF;             // the first player done (0xFF: nobody yet)
    std::vector<uint8_t> order;        // the players in the order they got done (the winner first)
    double won_at = -1, ends_at = -1;  // host clock
    bool over = false;                 // the timer ran out or everybody is done: the stage may end
    uint8_t why = 0;                   // over: 1 everybody done, 2 the timer ran out
    std::vector<Racer> racers;         // the last step's players
    bool operator==(const Race&) const = default;
};

enum EventKind : uint8_t { kWon = 1, kPlaced = 2, kOver = 3 };
struct Event {
    EventKind kind;
    uint8_t player = 0; // kWon / kPlaced: who; kOver: the reason (1 everybody done, 2 the timer)
    uint8_t place = 0;  // kPlaced: 2nd, 3rd, ...
};

// One step of the race of `stage` at `now` with the players as they are; a new stage starts a new race. Fewer than two
// players: no race (nothing changes, nobody wins alone). Returns what happened in this step.
inline std::vector<Event> step(Race& r, uint8_t stage, const std::vector<Racer>& players, double now, double timer_s)
{
    std::vector<Event> ev;
    if (stage != r.stage) {
        r = Race{};
        r.stage = stage;
    }
    r.racers = players;
    if (stage == kNone || r.over || players.size() < 2)
        return ev;
    for (const auto& p : players) {
        if (!p.done || std::find(r.order.begin(), r.order.end(), p.id) != r.order.end())
            continue;
        r.order.push_back(p.id);
        if (r.winner == 0xFF) {
            r.winner = p.id;
            r.won_at = now;
            r.ends_at = now + timer_s;
            ev.push_back({kWon, p.id, 1});
        } else
            ev.push_back({kPlaced, p.id, (uint8_t)r.order.size()});
    }
    if (r.winner == 0xFF)
        return ev;
    bool all = true;
    for (const auto& p : players)
        all = all && std::find(r.order.begin(), r.order.end(), p.id) != r.order.end();
    if (all || now >= r.ends_at) {
        r.over = true;
        r.why = all ? 1 : 2;
        ev.push_back({kOver, r.why, 0});
    }
    return ev;
}

// the seconds the catch-up timer still runs (-1: no winner yet; 0: over)
inline int seconds_left(const Race& r, double now)
{
    if (r.winner == 0xFF)
        return -1;
    if (r.over)
        return 0;
    return std::max(0, (int)(r.ends_at - now + 0.999));
}

inline std::string clock_text(int s)
{
    if (s < 0)
        s = 0;
    char b[16];
    snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60);
    return b;
}

// ---- MSG_RIVALS (111, session family), host -> guests: u8 kind 1 (the race), u8 stage, u8 flags (1 over, 2 the
// timer ran out), u8 winner (0xFF none), u16 seconds left (0xFFFF: no winner yet), u8 n, n x {u8 player, u8 flags (1
// done), u8 pct, u8 place (0: not done)}
constexpr uint8_t kKindRace = 1;

inline void write_race(net::Writer& w, const Race& r, double now)
{
    w.u8(kKindRace);
    w.u8(r.stage);
    w.u8((uint8_t)((r.over ? 1 : 0) | (r.why == 2 ? 2 : 0)));
    w.u8(r.winner);
    int s = seconds_left(r, now);
    w.u16(s < 0 ? 0xFFFF : (uint16_t)std::min(s, 0xFFFE));
    size_t n = std::min<size_t>(r.racers.size(), 16);
    w.u8((uint8_t)n);
    for (size_t i = 0; i < n; i++) {
        const Racer& p = r.racers[i];
        auto it = std::find(r.order.begin(), r.order.end(), p.id);
        w.u8(p.id);
        w.u8(p.done ? 1 : 0);
        w.u8(std::min<uint8_t>(p.pct, 100));
        w.u8(it == r.order.end() ? 0 : (uint8_t)(it - r.order.begin() + 1));
    }
}

// a guest's copy: the host's race at the guest's `now` (ends_at from the seconds left); false when cut short or wrong
inline bool read_race(net::Reader& rd, Race& out, double now)
{
    if (rd.u8() != kKindRace)
        return false;
    Race r;
    r.stage = rd.u8();
    uint8_t f = rd.u8();
    r.winner = rd.u8();
    uint16_t s = rd.u16();
    unsigned n = rd.u8();
    if (!rd.ok() || r.stage > kSpace || n > 16)
        return false;
    r.over = (f & 1) != 0;
    r.why = r.over ? ((f & 2) ? 2 : 1) : 0;
    if (s != 0xFFFF) {
        r.won_at = now;
        r.ends_at = now + s;
    }
    std::vector<std::pair<uint8_t, uint8_t>> placed; // (place, id)
    for (unsigned i = 0; i < n; i++) {
        Racer p;
        p.id = rd.u8();
        p.done = (rd.u8() & 1) != 0;
        p.pct = std::min<uint8_t>(rd.u8(), 100);
        uint8_t place = rd.u8();
        if (place)
            placed.push_back({place, p.id});
        r.racers.push_back(p);
    }
    if (!rd.ok() || rd.left())
        return false;
    std::sort(placed.begin(), placed.end());
    for (auto& [pl, id] : placed)
        r.order.push_back(id);
    if ((r.winner == 0xFF) != r.order.empty())
        return false;
    out = std::move(r);
    return true;
}

// ---- Tribe / Civilization (docs/11-rivals.md 11.3.3, 11.3.4): the first to subjugate more than half of the NPC
// tribes (allied or conquered) / to own more than half of the NPC cities wins. `won`: what the player has; `free`: the
// NPC tribes / cities nobody has yet; `total`: every NPC tribe / city of the planet at the stage's start (or now).
// When no free NPC is left before anybody has more than half, the one with the most is done (ties: all of them).
inline bool more_than_half(int won, int total) { return total > 0 && won * 2 > total; }

inline void subjugation_done(std::vector<Racer>& ps, const std::vector<int>& won, int total, int free_left)
{
    int best = 0;
    for (int w : won)
        best = std::max(best, w);
    for (size_t i = 0; i < ps.size() && i < won.size(); i++) {
        ps[i].done = more_than_half(won[i], total) || (free_left <= 0 && best > 0 && won[i] == best);
        int need = total / 2 + 1;
        ps[i].pct = (uint8_t)std::clamp(total > 0 ? won[i] * 100 / std::max(1, need) : 0, 0, 100);
    }
}

} // namespace rivals
