// The rules of every player's own empire in the Space stage (BUG-9279, the last phase of "Separated Lives";
// space_empires.cpp is the game glue, docs/re/space.md#space-own-empires). Portable (no Windows, no game):
// tests/space_empires_tests.cpp checks them natively.
//
// * The host keeps the players' empires by party seat (party.h) next to the planet's save, never inside it:
//   `<planet>.spo.duoempires`, EMP1: u32 'EMP1', u8 n (1..kMaxSeats), n x {u8 flags (1 founded), u64 player identity,
//   u32 species (the seat's own creature key instance), i32 money}. Seat 0 is the host's: the game's player empire,
//   whose money is in the game's own save (its entry stays empty).
// * A seat >= 1 with a species of its own (not the host's) gets an empire of its own; a seat that plays the host's
//   species shares the host's empire and its money as before (shared.cpp), the same rule as the Civilization nations.
// * A new empire starts with the host empire's money at that moment (never more; nothing is taken from the host).
// * The guest's game holds its own money (its player cEmpire +0xD0); it reports it to the host every 2 s. A report
//   counts only when it answers the host's last K_EMPIRE to it (the same serial): a report sent before the guest
//   got its money back after a load is the host save's money, not its own.
#pragma once

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "net/buffer.h"

namespace spaceempires {

constexpr size_t kMaxSeats = 8;   // founders::kMaxSeats
constexpr size_t kPlaySeats = 4;  // founders::kPlaySeats
constexpr int64_t kMoneyMax = 99999999; // the game's cEmpire::AddMoney clamps to 0..99 999 999
constexpr uint32_t kMagic = 0x31504D45u; // EMP1
constexpr size_t kFileMax = 5 + kMaxSeats * 17;

inline int32_t clamp_money(int64_t v) { return v < 0 ? 0 : v > kMoneyMax ? (int32_t)kMoneyMax : (int32_t)v; }
inline bool money_ok(int64_t v) { return v >= 0 && v <= kMoneyMax; }

struct Empire {
    bool founded = false;
    uint64_t player = 0;  // identity.h; 0 = not known
    uint32_t species = 0; // the seat's own creature (key instance)
    int32_t money = 0;
    bool operator==(const Empire&) const = default;
};

struct Roster {
    std::array<Empire, kMaxSeats> e{};
    // one past the last founded entry
    size_t seats() const
    {
        size_t n = 0;
        for (size_t i = 0; i < e.size(); ++i)
            if (e[i].founded)
                n = i + 1;
        return n;
    }
    bool founded(int seat) const { return seat >= 1 && seat < (int)kMaxSeats && e[seat].founded; }
    bool operator==(const Roster&) const = default;
};

// seat 0 is never an empire of the file (the game's own player empire); a founded entry has a species and sane money;
// an identity is in one entry only
inline bool roster_valid(const Roster& r)
{
    if (r.e[0].founded)
        return false;
    for (size_t i = 1; i < r.e.size(); ++i) {
        const Empire& a = r.e[i];
        if (!a.founded) {
            if (a.player || a.species || a.money)
                return false;
            continue;
        }
        if (!a.species || !money_ok(a.money))
            return false;
        for (size_t j = i + 1; j < r.e.size(); ++j)
            if (r.e[j].founded && a.player && r.e[j].player == a.player)
                return false;
    }
    return true;
}

inline std::vector<uint8_t> encode(const Roster& r)
{
    net::Writer w;
    w.u32(kMagic);
    const size_t n = r.seats() ? r.seats() : 1;
    w.u8((uint8_t)n);
    for (size_t i = 0; i < n; ++i) {
        w.u8(r.e[i].founded ? 1 : 0);
        w.u64(r.e[i].player);
        w.u32(r.e[i].species);
        w.u32((uint32_t)r.e[i].money);
    }
    return w.data;
}

// false: not an EMP1 file, cut, longer than it says, or not valid
inline bool decode(const std::vector<uint8_t>& bytes, Roster& out)
{
    if (bytes.size() > kFileMax)
        return false;
    net::Reader r(bytes.data(), bytes.size());
    if (r.u32() != kMagic)
        return false;
    const unsigned n = r.u8();
    if (!r.ok() || n < 1 || n > kMaxSeats || bytes.size() != 5 + n * 17)
        return false;
    Roster v;
    for (unsigned i = 0; i < n; ++i) {
        const uint8_t flags = r.u8();
        if (flags & ~1u)
            return false;
        v.e[i].founded = flags & 1;
        v.e[i].player = r.u64();
        v.e[i].species = r.u32();
        v.e[i].money = (int32_t)r.u32();
    }
    if (!r.ok() || r.left() || !roster_valid(v))
        return false;
    out = v;
    return true;
}

// one party seat as the founding sees it
struct Seat {
    int seat = 0;
    uint32_t species = 0; // the seat's own creature (party.h PartySeat::species), 0 = plays the host's
    uint64_t player = 0;
};

// the next seat whose empire is founded now: the lowest playing seat >= 1 with a species of its own (not the host's)
// and no empire yet; -1: none
inline int next_empire(const Roster& r, const std::vector<Seat>& seats, uint32_t host_species)
{
    int best = -1;
    for (const Seat& s : seats) {
        if (s.seat < 1 || s.seat >= (int)kPlaySeats || !s.species || s.species == host_species || r.e[s.seat].founded)
            continue;
        if (best < 0 || s.seat < best)
            best = s.seat;
    }
    return best;
}

// a new empire's money: the host empire's money now (never more, never below 0)
inline int32_t start_money(int64_t host_money) { return clamp_money(host_money); }

// the host's copy of one seat's money: what the guest's game holds, as it reported it
struct Wallet {
    uint32_t serial = 0; // the host's last K_EMPIRE to the guest (0: none in this session)
    int32_t money = 0;
    // a report of the guest: counted only when it answers the last K_EMPIRE and is a sane amount
    bool report(uint32_t answered, int64_t v)
    {
        if (!serial || answered != serial || !money_ok(v))
            return false;
        money = (int32_t)v;
        return true;
    }
};

// ---- BUG-9310: a partner's own homeworld (the owner's decision of 2026-11-21) ----
// A partner who gets an empire of its own in the host's Space world (also one new to the save) gets a homeworld of
// its own: a colony of its empire on a planet of a free star system near the host's home star, picked by the host and
// kept by seat next to the planet's save: `<planet>.spo.duoemphome`, EMH1: u32 'EMH1', u8 n, n x {u8 seat, u32
// PlanetID}. The guest's game makes the colony (the record-level colony of the kept colonies, space_allies.cpp), so
// the colony is one of the guest's own colonies from then on (the host keeps it across a rejoin and on its disk, its
// mirror empire shows it to the host).
constexpr uint32_t kHomeMagic = 0x31484D45u; // EMH1
constexpr size_t kHomeFileMax = 5 + kMaxSeats * 5;
using Homes = std::array<uint32_t, kMaxSeats>; // by seat: the PlanetID of its homeworld, 0 = none

inline std::vector<uint8_t> encode_homes(const Homes& h)
{
    net::Writer w;
    w.u32(kHomeMagic);
    uint8_t n = 0;
    for (size_t s = 1; s < kMaxSeats; ++s)
        n += h[s] != 0;
    w.u8(n);
    for (size_t s = 1; s < kMaxSeats; ++s)
        if (h[s]) {
            w.u8((uint8_t)s);
            w.u32(h[s]);
        }
    return w.data;
}
// false: not an EMH1 file, cut or longer than it says, a seat out of range or twice, seat 0, a planet id 0, or one
// planet for two seats
inline bool decode_homes(const std::vector<uint8_t>& bytes, Homes& out)
{
    if (bytes.size() > kHomeFileMax)
        return false;
    net::Reader r(bytes.data(), bytes.size());
    if (r.u32() != kHomeMagic)
        return false;
    const unsigned n = r.u8();
    if (!r.ok() || n >= kMaxSeats || bytes.size() != 5 + n * 5)
        return false;
    Homes v{};
    for (unsigned i = 0; i < n; ++i) {
        const unsigned s = r.u8();
        const uint32_t p = r.u32();
        if (!r.ok() || s < 1 || s >= kMaxSeats || v[s] || !p)
            return false;
        for (uint32_t q : v)
            if (q == p)
                return false;
        v[s] = p;
    }
    if (!r.ok() || r.left())
        return false;
    out = v;
    return true;
}

// one star system the host's game can give as a homeworld
struct HomeStar {
    uint32_t star = 0;    // StarID
    float dist = 0;       // galaxy units from the host's home star (neighbour stars are ~0.5-1 apart)
    bool free = false;    // nobody's (no empire, not a home, no NPC or Grox star), a real star (not a core, black hole, disk)
    bool taken = false;   // another seat's homeworld is there
};
// The rule: the stars sorted by their distance from the host's home star (the home star itself left out); the first
// free star from the `rank`-th nearest on ("at least `rank` star systems away": not the host's system, not next door,
// within a short flight). None from there on: the free star nearest to that rank below it. Returns the index into
// `stars` (sorted by the caller or not), -1 = none. rank < 1 counts as 1.
inline int pick_home_star(const std::vector<HomeStar>& stars, int rank)
{
    std::vector<size_t> order(stars.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    for (size_t i = 1; i < order.size(); ++i) // insertion sort: stable, no <algorithm> needed by the tests
        for (size_t j = i; j > 0 && stars[order[j]].dist < stars[order[j - 1]].dist; --j) {
            size_t t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    const size_t from = rank < 1 ? 0 : (size_t)rank - 1;
    auto ok = [&](size_t k) { return stars[order[k]].free && !stars[order[k]].taken && stars[order[k]].dist > 0; };
    for (size_t k = from; k < order.size(); ++k)
        if (ok(k))
            return (int)order[k];
    for (size_t k = from < order.size() ? from : order.size(); k-- > 0;)
        if (ok(k))
            return (int)order[k];
    return -1;
}
// the setting space.guest_home (settings_core.h): 0 off, else the rank of the choice
inline int home_rank(int choice)
{
    static const int r[] = {0, 3, 5, 8, 12};
    return choice >= 0 && choice < 5 ? r[choice] : 5;
}

inline std::string describe(const Roster& r)
{
    std::string out;
    char line[96];
    for (size_t i = 1; i < r.e.size(); ++i) {
        if (!r.e[i].founded)
            continue;
        snprintf(line, sizeof line, "%sseat %zu species %08X money %d", out.empty() ? "" : ", ", i, r.e[i].species,
                 r.e[i].money);
        out += line;
    }
    return out.empty() ? "no own empires" : out;
}

} // namespace spaceempires
