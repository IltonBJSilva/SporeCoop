// Space feature "empires" (MSG_SPACE kinds 100..109): every player's own empire in the Space stage, the last phase of
// "Separated Lives" (BUG-9279; the rules in space_empires_logic.h, docs/re/space.md#space-own-empires).
//
// Every partner who plays a species of their own (its party seat, party.h: its own creature, its own Civ nation) has
// an empire record of its own, kept by the host next to the planet's save (<planet>.spo.duoempires, never inside the
// game's save):
// * Host: a seat >= 1 with a species of its own and no empire yet gets one when the host's game is in Space (the
//   first Space world after the Civilization launch, or a save of an older version); its money: the host empire's
//   money at that moment. The host answers a guest's K_ASK with K_EMPIRE (have 1: its seat and money; have 0: it plays
//   the host's empire, the shared money as before), takes its K_REPORTs (its money every 2 s) and writes the file with
//   every save of the game. A player who is away keeps their empire and money in the file; they get them back when
//   they join again, also on another evening.
// * Guest: after every Space world it loads from the host it asks; with its own empire its game's money (the player
//   cEmpire +0xD0) is set to its own and shared.cpp stops sharing the Space money for it (own_money()). Its game earns
//   and spends by itself; nothing it does moves the host's money, and the host's purchases never move its money.
// * Diplomacy: a guest with its own empire keeps its own relations with the NPC empires (space_diplomacy.cpp shares
//   nothing for it). It sends its table to the host when it changes (K_DIPLO); the host keeps it by seat
//   (<planet>.spo.duoempdiplo, with every save) and gives it back after every Space world the guest loads
//   (K_DIPLO_SET, right after K_EMPIRE): a rejoin, also on another evening, keeps its own wars and alliances.
// * Its name: on its own game the guest's player empire is called "<player>'s empire" (the name the host's game gives
//   its allied mirror, space_allies.cpp), set back before every save of that game.
// * Rivals mode (space_rivalry.cpp) has budgets of its own: this feature waits while it is on.
// * Its homeworld (BUG-9310, the owner's decision of 2026-11-21): the host picks a planet of a free star system near
//   its home star for every seat with an empire (space_empires_logic.h pick_home_star, the session setting
//   space.guest_home: how many systems away), keeps it next to the planet's save (<planet>.spo.duoemphome) and tells
//   the guest after every K_EMPIRE (K_HOME 106). The guest's game makes it a colony of its own empire (the record-level
//   colony of the kept colonies) with its own species as the system's citizens (cStarRecord +0x74, the key put back
//   before every save of that game). From then on it is one of the guest's colonies: the host's mirror empire shows
//   it (with the guest's species, space_allies.cpp), the host keeps it across a rejoin and on its disk (space_map.cpp).
//   A guest with no species of its own is asked for one first (start_any.cpp, the "Your own start" page).
// `coop.fix space.own_empires off`: nothing is founded or asked, the shared money of 0.7.15.
#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "fixes.h"
#include "settings.h"
#include "spacecoop.h"
#include "founders.h"
#include "identity.h"
#include "modes.h"
#include "own_world.h"
#include "party.h"
#include "player_life.h"
#include "space_empires.h"
#include "space_empires_logic.h"
#include "space_ext.h"
#include "space_launch.h"
#include "space_map.h"
#include "creations.h"
#include "sui.h"
#include "sporekit_game.hpp"
#include "wire/space_empires.h"
#include "world.h"

using skg::cdecl_call;
using skg::thiscall;
using skg::vcall;

namespace spaceempires {

enum Kind : uint8_t {
    K_ASK = 100,       // guest -> host {to 0, serial 0}: my Space world opened, which empire is mine?
    K_EMPIRE = 101,    // host -> guest {to, serial, have, seat, money}: your own empire and its money (have 0: the host's)
    K_REPORT = 102,    // guest -> host {to 0, serial answered, money}: my own empire's money now
    K_DIPLO = 103,     // guest -> host {to 0, serial answered, part}: my own empire's diplomacy table (one part)
    K_DIPLO_SET = 104, // host -> guest {to, serial of its K_EMPIRE, part}: your table as I kept it (count 0: none)
    K_LAUNCH = 105,    // guest -> host {to 0, serial answered, state, ship, captain}: my own spaceship (space_launch.cpp)
    K_HOME = 106,      // host -> guest {to, serial of its K_EMPIRE, planet}: your own empire's homeworld (BUG-9310)
};

static const SkHost* H;
static SkMod* M;
static uintptr_t a_PlayerEmpire, a_StrAssign, a_Color, a_SetShip, a_SetCaptain, a_SpaceGame, a_SetArchetype, a_NounMgr;
static uintptr_t a_StarMgr, a_RequirePlanets, a_GetPlanets, a_StarSpecies; // BUG-9310

static bool g_fix = true;
COOP_FIX("space.own_empires", "BUG-9279", g_fix,
         "space: a partner who plays a species of their own has their own empire and money (kept with the planet)");
// own ship and captain: on its game a partner's empire flies the partner's newest own ship design (or the one
// `coop.space empires ship <hex>` chose) and has the partner's own species as its captain, set again after every Space
// world it loads from the host (the host's save holds the host's), put back before a save
static bool g_ship_fix = true;
COOP_FIX("space.own_ship", "BUG-9279", g_ship_fix,
         "space: a partner's own empire flies the partner's own ship design and has the partner's species as captain");
// the archetype's abilities: a partner's own Space card (feat/own-cards writes it into its cPlayer +0x110C) gets the
// tools of that archetype on its game (the save's are the host's): the game's own archetype setter, as the archetype
// change mission does (SpaceGame_SetArchetype(spacegame, false, false): the card stays, its tools replace the old ones)
static bool g_archetype_fix = true;
COOP_FIX("space.own_archetype", "BUG-9279", g_archetype_fix,
         "space: a partner's own Space archetype card brings that archetype's abilities on the partner's game");
static uint32_t g_arch_applied, g_arch_epoch, g_arch_sets;
static bool g_arch_queued;
static uint32_t space_card()
{
    void* nm = a_NounMgr ? cdecl_call<void*>(a_NounMgr) : nullptr;
    void* pl = nm && !IsBadReadPtr((uint8_t*)nm + 0x74, 4) ? *(void**)((uint8_t*)nm + 0x74) : nullptr;
    return pl && !IsBadReadPtr((uint8_t*)pl + 0x110C, 4) ? *(uint32_t*)((uint8_t*)pl + 0x110C) : 0;
}
static void archetype_task(void*)
{
    g_arch_queued = false;
    const uint32_t card = space_card();
    void* sg = a_SpaceGame ? cdecl_call<void*>(a_SpaceGame) : nullptr;
    if (!card || !sg || !a_SetArchetype || !spaceext::in_space())
        return;
    thiscall<void>(a_SetArchetype, sg, 0, 0); // from cPlayer +0x110C, no badge / achievement
    g_arch_applied = card;
    g_arch_epoch = spaceext::world_epoch();
    g_arch_sets++;
    H->log(M, SK_INFO, "empires: my archetype %08X and its abilities set on my game", card);
}
static bool g_name_fix = true;
COOP_FIX("space.own_empire_name", "BUG-9279", g_name_fix,
         "space: a partner's own empire carries the partner's name and colour on the partner's game");

constexpr uintptr_t kColorId = 0x10, kColorRgb = 0x30; // cEmpire colour id, cached RGB floats (docs/re/space.md 15.1)
constexpr uintptr_t kName = 0x3C;
constexpr uintptr_t kShipKey = 0xB8, kCaptainKey = 0xC4; // cEmpire spacecraft / captain ResourceKey  // cEmpire name, eastl::string16 {begin, end, cap} (docs/re/space.md 13.1)
constexpr uintptr_t kMoney = 0xD0; // cEmpire money (int; docs/re/space.md 13.1)
constexpr double kAskEvery = 3.0, kReportEvery = 2.0, kFoundEvery = 1.0, kDiploEvery = 5.0, kDiploResend = 60.0;
const wchar_t* const kSuffix = L".duoempires";
const wchar_t* const kDiploSuffix = L".duoempdiplo";
const wchar_t* const kHomeSuffix = L".duoemphome"; // BUG-9310
constexpr uint32_t kDiploMagic = 0x31444D45u; // EMD1
constexpr size_t kDiploMax = wire::space_empires::kPartMax * wire::space_empires::kPartsMax;
constexpr size_t kDiploFileMax = 5 + kMaxSeats * (5 + kDiploMax);

namespace ew = wire::space_empires;

static void* player_empire()
{
    void* e = a_PlayerEmpire ? cdecl_call<void*>(a_PlayerEmpire) : nullptr;
    return e && !IsBadReadPtr(e, kMoney + 4) ? e : nullptr;
}
static int money_now()
{
    void* e = player_empire();
    return e ? *(int*)((uint8_t*)e + kMoney) : -1;
}
static bool set_money(int32_t v)
{
    void* e = player_empire();
    if (!e)
        return false;
    *(int*)((uint8_t*)e + kMoney) = clamp_money(v);
    return true;
}
static bool active() { return g_fix && spaceext::connected() && !spaceext::rivals(); }

// BUG-9310 (the owner's decision of 2026-11-21): every partner with an empire of its own gets its own homeworld, and a
// partner new to the host's Space world with no species of its own is asked for one (start_any.cpp). Off: such a
// partner flies the host's empire (0.7.15.97); a partner with a species keeps its empire and money, without a home.
static bool g_home_fix = true;
COOP_FIX("space.guest_home", "BUG-9310", g_home_fix,
         "space: a partner new to a Space world chooses its species and its own empire gets its own homeworld near the host's");
static int home_rank_now() { return home_rank(settings::get(settings::kSpaceGuestHome)); }
static bool homes_on() { return g_fix && g_home_fix && home_rank_now() > 0 && !spaceext::rivals(); }
bool home_asks() { return homes_on() && spaceext::connected(); }

static std::u16string name_of_empire(void* e)
{
    std::u16string s;
    if (!e)
        return s;
    const char16_t* b = *(const char16_t**)((uint8_t*)e + kName);
    const char16_t* en = *(const char16_t**)((uint8_t*)e + kName + 4);
    if (!b || en < b || en - b > 200 || IsBadReadPtr(b, (size_t)(en - b) * 2 + 2))
        return s;
    return std::u16string(b, en);
}
static void set_name(void* e, const std::u16string& v)
{
    if (e && a_StrAssign)
        thiscall<void>(a_StrAssign, (uint8_t*)e + kName, v.data(), v.data() + v.size());
}
static std::u16string utf16(const std::string& s)
{
    std::u16string o;
    for (size_t i = 0; i < s.size();) {
        uint32_t c = (uint8_t)s[i];
        size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        if (n == 2 && i + 1 < s.size())
            c = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
        else if (n == 3 && i + 2 < s.size())
            c = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
        else if (n != 1)
            c = '?';
        o += (char16_t)(c > 0xFFFF ? '?' : c);
        i += n;
    }
    return o;
}
static std::string utf8(const std::u16string& s)
{
    std::string o;
    for (char16_t c : s) {
        if (c < 0x80)
            o += (char)c;
        else if (c < 0x800) {
            o += (char)(0xC0 | (c >> 6));
            o += (char)(0x80 | (c & 0x3F));
        } else {
            o += (char)(0xE0 | (c >> 12));
            o += (char)(0x80 | ((c >> 6) & 0x3F));
            o += (char)(0x80 | (c & 0x3F));
        }
    }
    return o;
}

static void send_parts(uint8_t kind, const std::vector<uint8_t>& bytes, uint8_t to, uint32_t serial, uint16_t version)
{
    for (auto& p : ew::parts_of(bytes, to, serial, version)) {
        net::Writer w;
        p.write(w);
        spaceext::send(kind, w);
    }
}

// ---------------- the host ----------------
static std::string g_planet;    // the planet whose roster g_roster is
static Roster g_roster;
static bool g_read;
static std::array<Wallet, kMaxSeats> g_wallet{}; // by seat: the guest's money as it reported it (this session)
static std::array<std::vector<uint8_t>, kMaxSeats> g_diplo{}; // by seat: its own diplomacy table (ew::table_bytes)
static std::array<ew::Joiner, kMaxSeats> g_diplo_in{};
static bool g_diplo_dirty;
static std::map<uint8_t, int> g_told;            // player -> the seat its last K_EMPIRE named (-1: the host's empire)
static uint32_t g_serial;
static uint16_t g_diplo_version;
static double g_next_found;
static std::string g_last_err;
static Homes g_homes{};          // BUG-9310: by seat, its homeworld's PlanetID
static bool g_home_queued;
static double g_next_home;
static uint32_t g_home_picks, g_home_sent, g_home_none;
static std::string g_home_why = "-"; // why the last pick found none
// a player who left: its id goes to the next newcomer
PLAYER_STATE("space_empires", [](uint8_t id) { g_told.erase(id); }, [](uint8_t id) -> size_t { return g_told.count(id); });
static struct {
    uint32_t founded, asks, answers, reports, reports_bad, writes, write_fail, diplo_in, diplo_bad, diplo_out;
} g_n;

// EMD1: u32 magic, u8 n, n x {u8 seat, u32 len, len bytes (ew::table_bytes)}
static std::vector<uint8_t> diplo_encode()
{
    net::Writer w;
    w.u32(kDiploMagic);
    uint8_t n = 0;
    for (size_t s = 1; s < kMaxSeats; s++)
        n += !g_diplo[s].empty();
    w.u8(n);
    for (size_t s = 1; s < kMaxSeats; s++) {
        if (g_diplo[s].empty())
            continue;
        w.u8((uint8_t)s);
        w.u32((uint32_t)g_diplo[s].size());
        for (uint8_t b : g_diplo[s])
            w.u8(b);
    }
    return w.data;
}
static bool diplo_decode(const std::vector<uint8_t>& bytes)
{
    net::Reader r(bytes.data(), bytes.size());
    if (r.u32() != kDiploMagic)
        return false;
    std::array<std::vector<uint8_t>, kMaxSeats> v{};
    const unsigned n = r.u8();
    for (unsigned i = 0; i < n && r.ok(); i++) {
        const unsigned s = r.u8();
        const uint32_t len = r.u32();
        if (!r.ok() || s < 1 || s >= kMaxSeats || len > kDiploMax || len > r.left())
            return false;
        v[s].resize(len);
        for (uint32_t j = 0; j < len; j++)
            v[s][j] = r.u8();
        std::vector<wire::space_diplomacy::SnapRec> t;
        if (!ew::table_read(v[s], t))
            return false;
    }
    if (!r.ok() || r.left())
        return false;
    g_diplo = std::move(v);
    return true;
}

static void roster_load()
{
    const std::string planet = world::current_game_key();
    if (planet == g_planet && g_read)
        return;
    g_planet = planet;
    g_roster = {};
    g_wallet = {};
    g_diplo = {};
    g_diplo_in = {};
    g_diplo_dirty = false;
    g_told.clear();
    g_read = true;
    g_homes = {};
    std::vector<uint8_t> bytes;
    if (!planet.empty() && founders::read_side(planet, kSuffix, bytes, kFileMax)) {
        if (decode(bytes, g_roster))
            H->log(M, SK_INFO, "empires: this planet's own empires: %s", describe(g_roster).c_str());
        else
            H->log(M, SK_WARN, "empires: the planet's empire file is not valid: ignored");
    }
    bytes.clear();
    if (!planet.empty() && founders::read_side(planet, kDiploSuffix, bytes, kDiploFileMax) && !diplo_decode(bytes))
        H->log(M, SK_WARN, "empires: the planet's empire diplomacy file is not valid: ignored");
    bytes.clear();
    if (!planet.empty() && founders::read_side(planet, kHomeSuffix, bytes, kHomeFileMax)) {
        if (decode_homes(bytes, g_homes))
            H->log(M, SK_INFO, "empires: this planet's partner homeworlds read");
        else
            H->log(M, SK_WARN, "empires: the planet's homeworld file is not valid: ignored");
    }
}

static bool roster_write(const char* why)
{
    if (g_planet.empty() || !g_roster.seats())
        return false;
    if (!founders::write_side(g_planet, kSuffix, encode(g_roster))) {
        g_n.write_fail++;
        g_last_err = "could not write the empire file";
        H->log(M, SK_WARN, "empires: could not write the empire file (%s)", why);
        return false;
    }
    bool any = g_diplo_dirty;
    for (auto& d : g_diplo)
        any = any || !d.empty();
    if (any) {
        if (founders::write_side(g_planet, kDiploSuffix, diplo_encode()))
            g_diplo_dirty = false;
        else
            g_n.write_fail++;
    }
    bool homes = false;
    for (uint32_t h : g_homes)
        homes = homes || h;
    if (homes && !founders::write_side(g_planet, kHomeSuffix, encode_homes(g_homes)))
        g_n.write_fail++;
    g_n.writes++;
    H->log(M, SK_INFO, "empires: written (%s): %s", why, describe(g_roster).c_str());
    return true;
}

// the host's own seat: usually 0, but a planet played by other founders before has the host on a later seat (lane C's
// Kealeneon: the host on seat 2, seat 0 a founder of another PC)
static int host_seat()
{
    const int s = party::seat_of_player(spaceext::my_id());
    return s < 0 ? 0 : s;
}
static uint32_t host_species()
{
    const auto& seats = party::roster().seats;
    const int h = host_seat();
    return h < (int)seats.size() ? seats[h].species : 0;
}

// the seat of a seated player wants an empire of its own (a species of its own, not the host's)
static bool wants_empire(int seat)
{
    const auto& seats = party::roster().seats;
    if (seat < 1 || seat >= (int)kPlaySeats || seat >= (int)seats.size() || seat == host_seat())
        return false;
    return seats[seat].species && seats[seat].species != host_species();
}

static void send_empire(uint8_t player, int seat)
{
    ew::Empire m;
    m.to = player;
    m.serial = ++g_serial;
    m.have = g_roster.founded(seat);
    m.seat = m.have ? (uint8_t)seat : 0;
    m.money = m.have ? g_roster.e[seat].money : 0;
    m.species = m.have ? g_roster.e[seat].species : 0;
    if (m.have)
        g_wallet[seat].serial = m.serial;
    net::Writer w;
    m.write(w);
    spaceext::send(K_EMPIRE, w);
    g_told[player] = m.have ? seat : -1;
    g_n.answers++;
    if (m.have) { // its relations as it left them (count 0: none kept, the host save's are its start)
        send_parts(K_DIPLO_SET, g_diplo[seat], player, m.serial, ++g_diplo_version);
        g_n.diplo_out++;
    }
    if (m.have && homes_on() && g_homes[seat]) { // BUG-9310: its homeworld
        ew::Home h;
        h.to = player;
        h.serial = m.serial;
        h.planet = g_homes[seat];
        net::Writer hw;
        h.write(hw);
        spaceext::send(K_HOME, hw);
        g_home_sent++;
    }
    H->log(M, SK_INFO, "empires: player %u (%s) told: %s", player, spaceext::name_of(player).c_str(),
           m.have ? ("its own empire, seat " + std::to_string(seat) + ", money " + std::to_string(m.money) +
                     ", diplomacy " + std::to_string(g_diplo[seat].size()) + " bytes")
                        .c_str()
                  : "the host's empire");
}

static void host_found()
{
    const auto& seats = party::roster().seats;
    std::vector<Seat> want;
    for (size_t s = 1; s < seats.size(); ++s)
        if ((int)s != host_seat()) // the host's own empire is the game's
            want.push_back({(int)s, seats[s].species, seats[s].player});
    const int seat = next_empire(g_roster, want, host_species());
    if (seat < 1)
        return;
    const int host_money = money_now();
    if (host_money < 0)
        return; // no player empire yet
    Empire& e = g_roster.e[seat];
    e.founded = true;
    e.player = seats[seat].player;
    e.species = seats[seat].species;
    e.money = start_money(host_money);
    if (!roster_valid(g_roster)) {
        e = {};
        g_last_err = "the empire of seat " + std::to_string(seat) + " would make the roster invalid";
        return;
    }
    g_n.founded++;
    roster_write("founded");
    const uint8_t p = party::player_of_seat(seat);
    std::string who = !seats[seat].name.empty() ? seats[seat].name : "seat " + std::to_string(seat);
    H->log(M, SK_INFO, "empires: %s's own empire founded (seat %d, species %08X, money %d)", who.c_str(), seat, e.species,
           e.money);
    spaceext::notice(who + spaceext::tr("'s own empire is founded: its own money, allied with yours",
                                        ": основана своя империя, свои деньги, союзник"));
    if (p != party::kNobody && g_told.count(p))
        send_empire(p, seat); // it plays now: its money is its own from now on
}

// ---------------- BUG-9310: the homeworlds (host) ----------------
static bool rd(const void* p, size_t n) { return p && !IsBadReadPtr(p, n); }
template <typename T>
static T& F(void* p, uintptr_t off) { return *(T*)((uint8_t*)p + off); }
static std::vector<void*> ptrs(void* v, size_t max)
{
    std::vector<void*> out;
    if (!rd(v, 8))
        return out;
    void** b = F<void**>(v, 0);
    void** e = F<void**>(v, 4);
    if (!b || e <= b || (size_t)(e - b) > max || !rd(b, (e - b) * 4))
        return out;
    out.assign(b, e);
    return out;
}
static void* star_mgr() { return a_StarMgr ? cdecl_call<void*>(a_StarMgr) : nullptr; }
// a star's planet records (generated for a star never visited: cStarManager::RequirePlanetsForStar, the same
// planets on every game: spacecoop.cpp's seeded generation, BUG-0185)
static std::vector<void*> star_planets(void* st)
{
    if (!F<uint8_t>(st, 0xAC) && a_RequirePlanets && star_mgr())
        thiscall<void>(a_RequirePlanets, star_mgr(), st, 0);
    void* v = a_GetPlanets ? thiscall<void*>(a_GetPlanets, st) : (uint8_t*)st + 0x84;
    return ptrs(v, 64);
}
// the planet of a star that becomes a homeworld: no life above creatures on it (tech <= 1), a rocky planet (type +0x28
// 2..5, T0..T3: not a gas giant or an asteroid belt), the best terraforming type first; 0 = none
static uint32_t home_planet_of(void* st)
{
    uint32_t best = 0;
    int best_type = -1;
    for (void* r : star_planets(st)) {
        if (!rd(r, 0x1A4) || F<uint32_t>(r, 0x194) > 1)
            continue;
        const int type = F<int>(r, 0x28);
        if (type < 2 || type > 5 || type <= best_type)
            continue;
        best = F<uint32_t>(r, 0x184);
        best_type = type;
    }
    return best;
}
static std::string str16(void* s16)
{
    if (!rd(s16, 8))
        return "";
    const char16_t* b = F<const char16_t*>(s16, 0);
    const char16_t* e = F<const char16_t*>(s16, 4);
    if (!b || e <= b || e - b > 200 || !rd(b, (e - b) * 2))
        return "";
    return utf8(std::u16string(b, e));
}
static std::string planet_label(uint32_t planet)
{
    void* st = spacecoop::star_record(planet & 0xFFFFFFu);
    if (!rd(st, 0xB0))
        return "?";
    std::string name;
    for (void* r : ptrs((uint8_t*)st + 0x84, 64))
        if (rd(r, 0x1A4) && F<uint32_t>(r, 0x184) == planet)
            name = str16((uint8_t*)r + 0x18);
    return (name.empty() ? std::string("a planet") : name) + " (" + str16((uint8_t*)st + 0x60) + ")";
}

// pick a homeworld for `seat` (outside the frame: it may generate a star's planets). The rule: space_empires_logic.h
// pick_home_star over the 64 stars nearest the host's home star (the setting's rank); a star whose planets offer no
// homeworld is left out and the pick runs again.
static uint32_t pick_home(int seat, std::string& why)
{
    void* sm = star_mgr();
    void* me = player_empire();
    void* home = me ? spacecoop::star_record(F<uint32_t>(me, 0xB0)) : nullptr;
    if (!sm || !rd(home, 0xB0)) {
        why = "no home star";
        return 0;
    }
    const float* hp = &F<float>(home, 0x3C);
    std::vector<std::pair<float, void*>> cands; // the nearest 64
    uint8_t* b = F<uint8_t*>(sm, 0xC8);
    uint8_t* e = F<uint8_t*>(sm, 0xCC);
    for (uint8_t* sec = b; sec && sec + 20 <= e && rd(sec, 20); sec += 20) { // the sector table (docs/re/space.md 9.1)
        void** arr = *(void***)sec;
        void** end = *(void***)(sec + 4);
        for (void** it = arr; arr && it < end && it - arr < 4096 && rd(it, 4); it++) {
            void* st = *it;
            if (!rd(st, 0xB0) || st == home)
                continue;
            const float* p = &F<float>(st, 0x3C);
            const float d = std::sqrt((p[0] - hp[0]) * (p[0] - hp[0]) + (p[1] - hp[1]) * (p[1] - hp[1]) +
                                      (p[2] - hp[2]) * (p[2] - hp[2]));
            if (cands.size() < 64)
                cands.push_back({d, st});
            else {
                size_t fz = 0;
                for (size_t k = 1; k < cands.size(); k++)
                    if (cands[k].first > cands[fz].first)
                        fz = k;
                if (d < cands[fz].first)
                    cands[fz] = {d, st};
            }
        }
    }
    std::vector<HomeStar> c;
    for (auto& [d, st] : cands) {
        HomeStar h;
        h.star = F<uint32_t>(st, 0x70);
        h.dist = d;
        const uint32_t type = F<uint32_t>(st, 0x48); // StarType: 4.. real stars and binaries (1 core, 2 black hole, 3 disk)
        h.free = F<uint32_t>(st, 0x54) == 0xFFFFFFFFu && !(F<uint32_t>(st, 0x5C) & 0xF) && type >= 4 && type <= 12;
        for (size_t s2 = 1; s2 < kMaxSeats; s2++)
            h.taken = h.taken || ((int)s2 != seat && g_homes[s2] && (g_homes[s2] & 0xFFFFFFu) == (h.star & 0xFFFFFFu));
        c.push_back(h);
    }
    for (int tries = 0; tries < 12; tries++) {
        const int i = pick_home_star(c, home_rank_now());
        if (i < 0)
            break;
        if (uint32_t planet = home_planet_of(cands[i].second)) {
            H->log(M, SK_INFO,
                   "empires: seat %d's homeworld: planet %08X of star %u, %.2f units from home (rule: the first free "
                   "star from the %d-th nearest on)",
                   seat, planet, c[i].star, c[i].dist, home_rank_now());
            return planet;
        }
        c[i].free = false; // no planet for a homeworld there
    }
    why = "no free star system with a rocky planet near the home star";
    return 0;
}

static void home_task(void*)
{
    g_home_queued = false;
    if (!homes_on() || !spaceext::is_host() || !spaceext::in_space() || g_planet != world::current_game_key())
        return;
    const auto& seats = party::roster().seats;
    for (size_t seat = 1; seat < kMaxSeats; seat++) {
        if (!g_roster.e[seat].founded || g_homes[seat])
            continue;
        std::string why;
        const uint32_t planet = pick_home((int)seat, why);
        if (!planet) {
            g_home_none++;
            g_home_why = why;
            return; // tried again in a while
        }
        g_homes[seat] = planet;
        g_home_picks++;
        roster_write("homeworld");
        const std::string who =
            seat < seats.size() && !seats[seat].name.empty() ? seats[seat].name : "seat " + std::to_string(seat);
        spaceext::notice(who + spaceext::tr("'s own homeworld: ", ": своя родная планета: ") + planet_label(planet));
        const uint8_t p = party::player_of_seat((int)seat);
        if (p != party::kNobody && g_told.count(p) && g_told[p] == (int)seat)
            send_empire(p, (int)seat); // its K_HOME comes with it
    }
}

static void host_tick(double now)
{
    roster_load();
    if (!spaceext::in_space() || now < g_next_found)
        return;
    g_next_found = now + kFoundEvery;
    host_found();
    if (homes_on() && !g_home_queued && now >= g_next_home) {
        bool want = false;
        for (size_t seat = 1; seat < kMaxSeats; seat++)
            want = want || (g_roster.e[seat].founded && !g_homes[seat]);
        if (want) {
            g_next_home = now + 5.0;
            g_home_queued = true;
            H->run_outside_frame(home_task, nullptr);
        }
    }
}

uint32_t partner_species(uint8_t player)
{
    if (!homes_on() || !spaceext::is_host())
        return 0;
    auto it = g_told.find(player);
    return it != g_told.end() && it->second >= 1 && g_roster.founded(it->second) ? g_roster.e[it->second].species : 0;
}

// the save hook (any stage, before the game writes): the players' money goes into the empire file with the save
static void host_save()
{
    if (!g_fix || !spaceext::is_host() || !g_read || g_planet != world::current_game_key())
        return;
    if (g_roster.seats())
        roster_write("the game saves");
}

static void host_message(uint8_t kind, uint8_t origin, net::Reader& r)
{
    if (kind == K_ASK) {
        ew::Ask a;
        if (!a.read(r) || a.to != 0)
            return;
        g_n.asks++;
        if (!spaceext::in_space())
            return; // it asks again
        roster_load();
        const int seat = party::seat_of_player(origin);
        if (seat < 0)
            return; // not seated yet: it asks again
        if (wants_empire(seat) && !g_roster.founded(seat))
            return; // founded within a second: it asks again
        send_empire(origin, seat);
        return;
    }
    if (kind == K_REPORT) {
        ew::Report m;
        if (!m.read(r) || m.to != 0)
            return;
        const int seat = party::seat_of_player(origin);
        if (!g_roster.founded(seat) || !g_wallet[seat].report(m.serial, m.money)) {
            g_n.reports_bad++;
            return;
        }
        g_n.reports++;
        g_roster.e[seat].money = g_wallet[seat].money;
        return;
    }
    if (kind == K_DIPLO) {
        ew::Part p;
        const int seat = party::seat_of_player(origin);
        if (!p.read(r) || p.to != 0 || !g_roster.founded(seat) || !g_wallet[seat].serial ||
            p.serial != g_wallet[seat].serial) {
            g_n.diplo_bad++;
            return; // a table of a world the host has not answered yet: not its own
        }
        std::vector<uint8_t> bytes;
        if (!g_diplo_in[seat].add(p, bytes))
            return;
        std::vector<wire::space_diplomacy::SnapRec> t;
        if (!bytes.empty() && !ew::table_read(bytes, t)) {
            g_n.diplo_bad++;
            return;
        }
        if (bytes != g_diplo[seat]) {
            g_diplo[seat] = std::move(bytes);
            g_diplo_dirty = true;
        }
        g_n.diplo_in++;
        H->log(M, SK_INFO, "empires: seat %d's own diplomacy: %u empires", seat, (unsigned)t.size());
        return;
    }
    if (kind == K_LAUNCH) { // BUG-9299
        ew::Launch m;
        if (m.read(r) && m.to == 0)
            spacelaunch::host_message(origin, m);
    }
}

bool own_money_player(uint8_t player)
{
    if (!active() || !spaceext::is_host())
        return false;
    auto it = g_told.find(player);
    return it != g_told.end() && it->second >= 1;
}

// ---------------- the guest ----------------
static bool g_own;          // the host said: my own empire (sticky for the session)
static int g_seat;
static uint32_t g_answer;   // the serial of the host's last K_EMPIRE
static uint32_t g_answered; // the Space world epoch the last K_EMPIRE was applied in (0: none)
static double g_next_ask, g_next_report, g_next_diplo, g_diplo_sent_at;
static bool g_told_player;
static uint32_t g_g_asks, g_g_sets, g_g_reports, g_g_diplo_sets, g_g_diplo_sent;
static bool g_diplo_ready;           // the host's K_DIPLO_SET of g_answer is applied: my table is my own from now
static ew::Joiner g_diplo_join;
static std::vector<uint8_t> g_diplo_last; // the table I sent last
static uint16_t g_g_version;
// the name and colour: the empire object I renamed, its game name and colour, the epoch I did it in
static void* g_renamed;
static std::u16string g_orig_name;
static uint32_t g_orig_color;
static float g_orig_rgb[3];
// the ship and captain: the game's keys before ours (put back before a save), ours, the species the host told
struct Key3 {
    uint32_t i = 0, t = 0, g = 0;
    bool operator==(const Key3&) const = default;
};
static Key3 g_orig_ship, g_orig_captain, g_my_ship, g_my_captain;
static bool g_ship_set, g_captain_set;
static uint32_t g_species;
static uint32_t g_ship_choice; // coop.space empires ship <hex>
static Key3& key_at(void* e, uintptr_t off) { return *(Key3*)((uint8_t*)e + off); }
static double g_next_ship_try;
// the ship: the chosen or the newest own ship design of this player's library (Empire_SetSpacecraftKey also gives the
// empire's other ships that model); false: none known yet
static bool apply_ship(void* e)
{
    Key3 ship{};
    for (auto& o : creations::options(creations::kUfo))
        if (!g_ship_choice || o.key.instance == g_ship_choice) {
            ship = {o.key.instance, o.key.type, o.key.group};
            break;
        }
    if (ship.i && !ship.t) // an option names the instance only: the ship type and group of every ship design
        ship.t = 0x476A98C7u, ship.g = 0x40656200u;
    static uint32_t s_told;
    // (not creations::key_known: a design of EditorSaves.package is no ResourceManager record under the ship key until
    // the game makes its model; a key the game cannot draw gets the default ship, gamefixes game.ufo_model)
    if (!ship.i || !a_SetShip) {
        if (ship.i && s_told != ship.i) {
            s_told = ship.i;
            H->log(M, SK_INFO, "empires: no ship setter for %08X/%08X/%08X", ship.i, ship.t, ship.g);
        }
        return false;
    }
    g_orig_ship = key_at(e, kShipKey);
    g_my_ship = ship;
    if (!(g_orig_ship == ship))
        thiscall<void>(a_SetShip, e, &ship);
    g_ship_set = true;
    H->log(M, SK_INFO, "empires: my ship %08X (the game's %08X)", ship.i, g_orig_ship.i);
    return true;
}
static void ship_task(void*)
{
    void* e = player_empire();
    if (e && g_renamed == e && !g_ship_set && g_own && g_ship_fix)
        apply_ship(e);
}
static uint32_t g_renamed_epoch;

bool own_money() { return active() && !spaceext::is_host() && g_own; }
int own_seat() { return own_money() ? g_seat : -1; }

static std::u16string own_name() { return utf16(spaceext::name_of(spaceext::my_id()) + "'s empire"); }

static void name_undo()
{
    if (!g_renamed)
        return;
    if (g_renamed == player_empire() && g_renamed_epoch == spaceext::world_epoch()) {
        if (g_ship_set && a_SetShip && key_at(g_renamed, kShipKey) == g_my_ship)
            thiscall<void>(a_SetShip, g_renamed, &g_orig_ship);
        if (g_captain_set && a_SetCaptain && key_at(g_renamed, kCaptainKey) == g_my_captain)
            thiscall<void>(a_SetCaptain, g_renamed, &g_orig_captain);
        g_ship_set = g_captain_set = false;
        set_name(g_renamed, g_orig_name);
        *(uint32_t*)((uint8_t*)g_renamed + kColorId) = g_orig_color;
        memcpy((uint8_t*)g_renamed + kColorRgb, g_orig_rgb, sizeof g_orig_rgb);
    }
    g_renamed = nullptr;
}
// the Space HUD's planet panel (window 0x05D646C8: the active planet's owner, its name 0x044D94D1 and its species
// picture 0x036C0957, docs/re/space.md#space-hud-planet-panel)
constexpr uint32_t kPanelName = 0x044D94D1u, kPanelPicture = 0x036C0957u, kPngType = 0x2F7D0004u;
// 1: the panel's picture is the png of creature `inst` (group 0x40626200), 0: another one, -1: not known
static int hud_picture_is(uint32_t inst)
{
    static uintptr_t get_image = H->sym("UTFWin::Image::GetImage"), image_vt = H->sym("UTFWin::ImageDrawable::vftable");
    sui::Win root = sui::main_window();
    sui::Win w = root ? sui::find(root, kPanelPicture) : nullptr;
    uint8_t* d = w ? (uint8_t*)sui::drawable(w) : nullptr;
    if (!inst || !d || IsBadReadPtr(d, 0x28) || !image_vt || *(uint32_t*)d != (uint32_t)image_vt || !get_image)
        return -1;
    void* img = *(void**)(d + 0x24);
    void* want = nullptr;
    uint32_t k[3] = {inst, kPngType, 0x40626200u};
    if (!cdecl_call<bool>(get_image, k, &want, false, -1, -1) || !want)
        return -1;
    const bool same = want == img;
    vcall<void>(want, 1); // the reference GetImage gave
    return same ? 1 : 0;
}
static std::string hud_panel()
{
    sui::Win root = sui::main_window();
    sui::Win n = root ? sui::find(root, kPanelName) : nullptr;
    void* e = player_empire();
    const uint32_t cap = e ? key_at(e, kCaptainKey).i : 0;
    char b[200];
    snprintf(b, sizeof b, "name='%s' picture_is_captain(%08X)=%d", n ? sui::caption(n).c_str() : "?", cap, hud_picture_is(cap));
    return b;
}
// BUG-9296: the planet panel is built when the active planet changes (SpaceGameUI_UpdatePlanetPanel); a rename of the
// player empire afterwards left it showing the game's old name (the host's empire's) until the next planet. On: the
// panel is built again right after this player's own empire is applied on its game. Off: as 0.7.15.96.
static bool g_panel_fix = true;
COOP_FIX("space.own_empire_panel", "BUG-9296", g_panel_fix,
         "space: the Space HUD's planet panel shows a partner's own empire name at once (it kept the host's empire name)");
static uint32_t g_panel_sets;
static void hud_refresh() // outside the frame
{
    static uintptr_t refresh = H->sym("SporeCoop::SpaceGameUI_UpdatePlanetPanel"), ui_get = H->sym("SporeCoop::SpaceGameUI_Get");
    if (!g_panel_fix || !refresh || !ui_get || !spaceext::in_space())
        return;
    void* ui = cdecl_call<void*>(ui_get);
    if (!ui || IsBadReadPtr(ui, 0x228) || !*(void**)((uint8_t*)ui + 0x224)) // its layout (the HUD is built)
        return;
    thiscall<void>(refresh, ui);
    g_panel_sets++;
}
static void apply_own(void* e, uint32_t species, bool ship);
static void name_apply()
{
    if (!g_name_fix || g_renamed || !g_own || g_answered != spaceext::world_epoch())
        return;
    void* e = player_empire();
    if (!e)
        return;
    apply_own(e, g_species, true);
}
// the name, colour, captain (and ship) of this player's own empire on the empire object `e` of its game; put back by
// name_undo (before a save, at the session's end)
static void apply_own(void* e, uint32_t species, bool ship)
{
    g_orig_name = name_of_empire(e);
    g_ship_set = g_captain_set = false;
    if (g_ship_fix) {
        if (ship)
            apply_ship(e); // (the library's list may come later: guest_tick tries again)
        // the captain: this player's own species (a creature of its own library)
        const Key3 cap{species, 0x2B978C46u, 0x40626200u};
        if (species && a_SetCaptain && creations::key_known({cap.i, cap.t, cap.g})) {
            g_orig_captain = key_at(e, kCaptainKey);
            g_my_captain = cap;
            if (!(g_orig_captain == cap))
                thiscall<void>(a_SetCaptain, e, &cap);
            g_captain_set = true;
        }
        H->log(M, SK_INFO, "empires: my captain %08X (the game's %08X)", g_captain_set ? cap.i : 0u, g_orig_captain.i);
    }
    g_orig_color = *(uint32_t*)((uint8_t*)e + kColorId);
    memcpy(g_orig_rgb, (uint8_t*)e + kColorRgb, sizeof g_orig_rgb);
    set_name(e, own_name());
    // its colour: the game's palette colour nearest to this player's co-op colour (the host's game draws its allied
    // mirror empire in that colour too, space_allies.cpp)
    if (a_Color) {
        const uint32_t argb = spacemap::player_color(spaceext::my_id());
        uint32_t best = 0;
        float bd = 1e9f;
        for (uint32_t i = 0; i < 12; i++) {
            const float* c = cdecl_call<float*>(a_Color, 0x53DBCF3u + i);
            if (!c || IsBadReadPtr(c, 12))
                continue;
            float d = 0;
            for (int k = 0; k < 3; k++) {
                const float want = ((argb >> (16 - 8 * k)) & 0xFF) / 255.f;
                d += (c[k] - want) * (c[k] - want);
            }
            if (d < bd) {
                bd = d;
                best = 0x53DBCF3u + i;
            }
        }
        const float* rgb = best ? cdecl_call<float*>(a_Color, best) : nullptr;
        if (rgb && !IsBadReadPtr(rgb, 12)) {
            *(uint32_t*)((uint8_t*)e + kColorId) = best;
            memcpy((uint8_t*)e + kColorRgb, rgb, 12);
        }
    }
    g_renamed = e;
    g_renamed_epoch = spaceext::world_epoch();
    hud_refresh(); // BUG-9296
    H->log(M, SK_INFO, "empires: my empire is called '%s' here (the game's name '%s')", utf8(own_name()).c_str(),
           utf8(g_orig_name).c_str());
}
static bool g_name_queued;
static void name_task(void*)
{
    g_name_queued = false;
    name_apply();
}

// BUG-9297: a guest's own launch from Civilization (civ_launch.cpp, BUG-9295) starts its own game in Space on its home
// planet. That game is its copy of the host's Civ save, so its player empire had the host's captain and name until the
// host's world loaded (only a world received from the host was given the guest's own). On: civ_launch gives the
// guest's own nation species here and the guest's local start gets its own captain, name and colour (not the ship: it
// flies the ship it has just designed; not the money: the host tells it with K_EMPIRE in the host's world), put back
// before a save like the rest. Off: the host's captain and name until the host's world loads (0.7.15.96).
static bool g_local_fix = true;
COOP_FIX("space.own_captain_local", "BUG-9297", g_local_fix,
         "space: a partner's own launch from Civ starts in Space with the partner's own captain and empire name, not "
         "the host's");
static uint32_t g_local_species, g_local_epoch, g_local_sets;
static const char* g_local_why = "-"; // why the last local_start did not apply (status)
static bool g_local_queued;
static void local_task(void*)
{
    g_local_queued = false;
    void* e = player_empire();
    if (!e || !g_local_species || !spaceext::in_space() || !spaceext::world_epoch()) {
        g_local_why = !e ? "no player empire" : "not in a Space world";
        return;
    }
    if (g_renamed && g_renamed_epoch == spaceext::world_epoch()) {
        g_local_why = "already my own empire here";
        g_local_epoch = spaceext::world_epoch();
        return;
    }
    g_renamed = nullptr;
    apply_own(e, g_local_species, false);
    g_local_epoch = spaceext::world_epoch();
    g_local_sets++;
    g_local_why = "applied";
    H->log(M, SK_INFO, "empires: my own launch's Space start has my captain %08X and my empire's name", g_local_species);
}
bool local_start(uint32_t species)
{
    if (g_local_fix && g_local_epoch && g_local_epoch == spaceext::world_epoch() && g_renamed)
        return true; // done in this world
    g_local_why = !g_local_fix ? "off" : !g_name_fix ? "name off" : !species ? "no species" : !active() ? "not active"
                  : spaceext::is_host() ? "host" : !spaceext::in_space() ? "not in Space" : "queued";
    if (!g_local_fix || !g_name_fix || !species || !active() || spaceext::is_host() || !spaceext::in_space())
        return false;
    g_local_species = species;
    if (!g_local_queued) {
        g_local_queued = true;
        H->run_outside_frame(local_task, nullptr);
    }
    return false;
}

// ---------------- BUG-9310: my homeworld (guest) ----------------
// K_HOME names it; in every Space world of the host's that answered my K_ASK it is made a colony of my empire (the
// record-level colony of the kept colonies: spaceext::colonize_own, "" also when it is mine already) and its star
// system gets my species as its citizens: cStarRecord +0x74 {instance, type, group} (Simulator::cPlanetRecord::
// GetCitizenSpeciesKey reads the star's key: the HUD's planet panel picture, the cities' citizens), set with the game's
// own setter (SporeCoop::cStarRecord_SetSpeciesKey) and put back before every save of my game.
static uint32_t g_home_planet;       // from the host (0: none)
static uint32_t g_home_epoch;        // the Space world it was made mine in (0: not yet)
static bool g_home_apply_queued, g_home_told;
static double g_next_home_try;
static std::string g_home_last = "-"; // the last try's answer
static uint32_t g_home_made, g_home_waits, g_home_fails, g_home_keys;
static void* g_home_star;            // the star record whose species key I set (pointer compared only)
static uint32_t g_home_star_epoch;
static uint32_t g_home_key_orig[3], g_home_key_mine[3];
static uint32_t my_species()
{
    if (g_species && creations::key_known({g_species, 0x2B978C46u, 0x40626200u}))
        return g_species;
    return creations::mine(creations::kCreature).key.instance; // the creature this game plays (its own key)
}
static void home_undo()
{
    if (g_home_star && g_home_star_epoch == spaceext::world_epoch() && a_StarSpecies && rd(g_home_star, 0x80) &&
        !memcmp((uint8_t*)g_home_star + 0x74, g_home_key_mine, 12))
        thiscall<void>(a_StarSpecies, g_home_star, g_home_key_orig);
    g_home_star = nullptr;
    g_home_epoch = 0; // made again (and the key set again) in the next tick
}
static void home_apply_task(void*)
{
    g_home_apply_queued = false;
    const uint32_t e = spaceext::world_epoch();
    if (!g_home_planet || !g_own || g_answered != e || !spaceext::in_space() || own_world::space_other())
        return;
    std::string why = spaceext::colonize_own(g_home_planet);
    g_home_last = why.empty() ? "made" : why;
    if (why == "wait") { // my ship is at that planet now
        g_home_waits++;
        g_next_home_try = H->time_seconds() + 3;
        return;
    }
    if (!why.empty()) {
        g_home_fails++;
        g_next_home_try = H->time_seconds() + 15;
        H->log(M, SK_WARN, "empires: my homeworld %08X can not be mine: %s", g_home_planet, why.c_str());
        return;
    }
    // its citizens: my species
    void* st = spacecoop::star_record(g_home_planet & 0xFFFFFFu);
    const uint32_t sp = my_species();
    if (st && rd(st, 0x80) && sp && a_StarSpecies) {
        const uint32_t key[3] = {sp, 0x2B978C46u, 0x40626200u};
        if (memcmp((uint8_t*)st + 0x74, key, 12)) {
            memcpy(g_home_key_orig, (uint8_t*)st + 0x74, 12);
            memcpy(g_home_key_mine, key, 12);
            thiscall<void>(a_StarSpecies, st, key);
            g_home_star = st;
            g_home_star_epoch = e;
            g_home_keys++;
        }
    }
    g_home_epoch = e;
    g_home_made++;
    H->log(M, SK_INFO, "empires: my homeworld %08X is my colony in this world, citizens %08X", g_home_planet, sp);
    if (!g_home_told) {
        g_home_told = true;
        spaceext::notice(spaceext::tr("Your own homeworld: ", "Ваша родная планета: ") + planet_label(g_home_planet));
    }
}

// the save hook on the guest: the game's own name goes into its save
static void guest_save()
{
    if (!spaceext::is_host()) {
        name_undo();
        home_undo();
    }
}

static void guest_tick(double now)
{
    if (own_world::space_other() || !spaceext::in_space() || !modes::current_world())
        return; // a Space save of the guest's own, or not known yet
    const uint32_t e = spaceext::world_epoch();
    if (!e)
        return;
    if (g_renamed && g_renamed_epoch != e)
        g_renamed = nullptr; // another world: that empire object is gone
    if (g_answered != e) {
        if (now < g_next_ask)
            return;
        g_next_ask = now + kAskEvery;
        ew::Ask a;
        net::Writer w;
        a.write(w);
        spaceext::send(K_ASK, w);
        g_g_asks++;
        return;
    }
    if (!g_own)
        return;
    if (g_home_planet && homes_on() && g_home_epoch != e && !g_home_apply_queued && now >= g_next_home_try) { // BUG-9310
        g_home_apply_queued = true;
        g_next_home_try = now + 3.0;
        H->run_outside_frame(home_apply_task, nullptr);
    }
    if (!g_renamed && g_name_fix && !g_name_queued && player_empire()) {
        g_name_queued = true;
        H->run_outside_frame(name_task, nullptr); // after a save, or the first time in this world
    }
    if (g_archetype_fix && g_renamed && !g_arch_queued && a_SetArchetype) {
        const uint32_t card = space_card();
        if (card && (card != g_arch_applied || g_arch_epoch != e)) {
            g_arch_queued = true;
            H->run_outside_frame(archetype_task, nullptr);
        }
    }
    if (g_ship_fix && g_renamed && !g_ship_set && now >= g_next_ship_try) {
        g_next_ship_try = now + 3.0;
        H->run_outside_frame(ship_task, nullptr);
    }
    if (g_diplo_ready && now >= g_next_diplo) {
        g_next_diplo = now + kDiploEvery;
        auto bytes = ew::table_bytes(diplo::own_table());
        if (bytes.size() <= kDiploMax && (bytes != g_diplo_last || now - g_diplo_sent_at > kDiploResend)) {
            send_parts(K_DIPLO, bytes, 0, g_answer, ++g_g_version);
            g_diplo_last = std::move(bytes);
            g_diplo_sent_at = now;
            g_g_diplo_sent++;
        }
    }
    if (now < g_next_report)
        return;
    g_next_report = now + kReportEvery;
    const int m = money_now();
    if (m < 0)
        return;
    ew::Report rep;
    rep.serial = g_answer;
    rep.money = m;
    net::Writer w;
    rep.write(w);
    spaceext::send(K_REPORT, w);
    g_g_reports++;
}

static void guest_message(uint8_t kind, net::Reader& r)
{
    if (kind == K_DIPLO_SET) {
        ew::Part p;
        if (!p.read(r) || p.to != spaceext::my_id() || p.serial != g_answer || !g_own)
            return;
        std::vector<uint8_t> bytes;
        if (!g_diplo_join.add(p, bytes))
            return;
        std::vector<wire::space_diplomacy::SnapRec> t;
        if (!bytes.empty()) {
            if (!ew::table_read(bytes, t) || !diplo::own_apply(t))
                return; // not in Space now: the next K_EMPIRE brings it again
        }
        g_diplo_ready = true;
        g_diplo_last = bytes;
        g_diplo_sent_at = H ? H->time_seconds() : 0;
        g_next_diplo = g_diplo_sent_at + kDiploEvery;
        g_g_diplo_sets++;
        H->log(M, SK_INFO, "empires: my own diplomacy from the host: %u empires", (unsigned)t.size());
        return;
    }
    if (kind == K_HOME) { // BUG-9310: my homeworld, after the K_EMPIRE it names
        ew::Home h;
        if (!h.read(r) || h.to != spaceext::my_id() || h.serial != g_answer || !g_own)
            return;
        if (h.planet != g_home_planet) {
            g_home_planet = h.planet;
            g_home_epoch = 0;
            g_next_home_try = 0;
        }
        H->log(M, SK_INFO, "empires: my homeworld is planet %08X", h.planet);
        return;
    }
    if (kind != K_EMPIRE)
        return;
    ew::Empire m;
    if (!m.read(r) || m.to != spaceext::my_id())
        return;
    if (own_world::space_other() || !spaceext::in_space())
        return; // it asks again in the host's Space world
    if (m.have && !set_money(m.money))
        return; // no player empire now: it asks again
    g_own = m.have;
    g_seat = m.have ? m.seat : -1;
    g_species = m.have ? m.species : 0;
    g_answer = m.serial;
    g_answered = spaceext::world_epoch();
    g_next_report = 0;
    g_diplo_ready = false; // until its K_DIPLO_SET
    g_g_sets++;
    H->log(M, SK_INFO, "empires: %s", m.have ? ("my own empire, seat " + std::to_string(m.seat) + ", money " +
                                                std::to_string(m.money)).c_str()
                                             : "I play the host's empire (shared money)");
    if (m.have && !g_told_player) {
        g_told_player = true;
        spaceext::notice(spaceext::tr("Your own empire: your own money ($", "Ваша империя: свои деньги ($") +
                         std::to_string(m.money) + spaceext::tr("), your own diplomacy", "), своя дипломатия"));
    }
}

// ---------------- wiring ----------------
// BUG-9299: a new own ship design: the name, ship and captain are set again (name_undo puts the game's back first)
void ship_changed()
{
    H->run_outside_frame([](void*) { name_undo(); }, nullptr); // (the setters change models: outside the frame)
}

static void tick(double now)
{
    if (!active())
        return;
    if (spaceext::is_host()) {
        host_tick(now);
        return;
    }
    // BUG-9299: the own launch from Space also runs in the editor it opens (guest_tick is Space only)
    void* e = player_empire();
    spacelaunch::Me me;
    me.own = g_own && g_answered && g_answered == spaceext::world_epoch() && !own_world::space_other();
    me.settled = me.own && g_renamed && g_renamed == e;
    me.serial = g_answer;
    me.ship = e ? key_at(e, kShipKey).i : 0;
    me.captain = e ? key_at(e, kCaptainKey).i : 0;
    spacelaunch::guest_tick(now, me);
    guest_tick(now);
}

static void on_message(uint8_t kind, uint8_t origin, net::Reader& r)
{
    if (!active())
        return;
    if (spaceext::is_host())
        host_message(kind, origin, r);
    else
        guest_message(kind, r);
}

// lifecycle-exempt: a Space feature (space_ext.h), run by spaceext::session_end_all from the spacecoop.session hook
static void on_session_end()
{
    name_undo();
    home_undo(); // BUG-9310
    g_home_planet = 0;
    g_home_told = false;
    g_home_last = "-";
    g_wallet = {};
    g_diplo_in = {};
    g_told.clear();
    g_own = false;
    g_seat = -1;
    g_answer = 0;
    g_answered = 0;
    g_next_ask = g_next_report = g_next_diplo = g_diplo_sent_at = 0;
    g_told_player = false;
    g_diplo_ready = false;
    g_diplo_join = {};
    g_diplo_last.clear();
    g_arch_applied = g_arch_epoch = 0;
    g_local_species = g_local_epoch = 0;
    spacelaunch::on_session_end();
}

static std::string status()
{
    char b[640];
    if (spaceext::is_host() || !spaceext::connected())
        snprintf(b, sizeof b,
                 "empires: fix=%d host roster=[%s] founded=%u asks=%u answers=%u reports=%u bad=%u writes=%u fail=%u"
                 " diplo in=%u bad=%u out=%u%s%s",
                 (int)g_fix, describe(g_roster).c_str(), g_n.founded, g_n.asks, g_n.answers, g_n.reports, g_n.reports_bad,
                 g_n.writes, g_n.write_fail, g_n.diplo_in, g_n.diplo_bad, g_n.diplo_out, g_last_err.empty() ? "" : " err=",
                 g_last_err.c_str());
    else
        snprintf(b, sizeof b,
                 "empires: fix=%d guest own=%d seat=%d money=%d name='%s' color=%08X serial=%u answered_epoch=%u epoch=%u asks=%u"
                 " sets=%u reports=%u diplo ready=%d sets=%u sent=%u ship=%08X captain=%08X archetype=%08X (set %u) local=%u (%s) panel=%u",
                 (int)g_fix, (int)g_own, g_seat, money_now(), utf8(name_of_empire(player_empire())).c_str(),
                 player_empire() ? *(uint32_t*)((uint8_t*)player_empire() + kColorId) : 0u, g_answer,
                 g_answered, spaceext::world_epoch(), g_g_asks, g_g_sets, g_g_reports, (int)g_diplo_ready, g_g_diplo_sets,
                 g_g_diplo_sent, player_empire() ? key_at(player_empire(), kShipKey).i : 0u,
                 player_empire() ? key_at(player_empire(), kCaptainKey).i : 0u, g_arch_applied, g_arch_sets, g_local_sets, g_local_why, g_panel_sets);
    char hb[320];
    if (spaceext::is_host() || !spaceext::connected()) {
        std::string homes;
        for (size_t s = 1; s < kMaxSeats; s++)
            if (g_homes[s]) {
                char t[48];
                snprintf(t, sizeof t, " seat%zu=%08X", s, g_homes[s]);
                homes += t;
            }
        snprintf(hb, sizeof hb, "home: fix=%d on=%d rank=%d homes=[%s ] picks=%u none=%u (%s) sent=%u", (int)g_home_fix,
                 (int)homes_on(), home_rank_now(), homes.c_str(), g_home_picks, g_home_none, g_home_why.c_str(), g_home_sent);
    } else {
        void* st = g_home_planet ? spacecoop::star_record(g_home_planet & 0xFFFFFFu) : nullptr;
        snprintf(hb, sizeof hb, "home: fix=%d on=%d planet=%08X mine_epoch=%u made=%u waits=%u fails=%u last=%s keys=%u citizens=%08X",
                 (int)g_home_fix, (int)homes_on(), g_home_planet, g_home_epoch, g_home_made, g_home_waits, g_home_fails,
                 g_home_last.c_str(), g_home_keys, rd(st, 0x80) ? F<uint32_t>(st, 0x74) : 0u);
    }
    return std::string(b) + "\n" + hb + "\n" + spacelaunch::status();
}

static std::vector<spaceext::Row> rows()
{
    std::vector<spaceext::Row> out;
    if (!active())
        return out;
    out = spacelaunch::rows(); // BUG-9299: the own spaceship's offer (guest), the partners' launches (host)
    if (!spaceext::is_host()) {
        if (g_own)
            out.push_back({spaceext::tr("Your own empire: your own money and diplomacy",
                                        "Ваша империя: свои деньги и дипломатия"),
                           "", nullptr, 0});
        if (g_own && g_home_planet && homes_on()) { // BUG-9310
            const uint32_t star = g_home_planet & 0xFFFFFFu;
            out.push_back({spaceext::tr("Your homeworld: ", "Ваша родная планета: ") + planet_label(g_home_planet),
                           spaceext::tr("Fly there", "Лететь"),
                           [star] {
                               std::string why = spacecoop::travel(star);
                               if (!why.empty())
                                   spaceext::notice(why);
                           },
                           0});
        }
        return out;
    }
    const auto& seats = party::roster().seats;
    for (size_t s = 1; s < kMaxSeats; ++s) {
        if (!g_roster.e[s].founded)
            continue;
        std::string who = s < seats.size() && !seats[s].name.empty() ? seats[s].name : "seat " + std::to_string(s);
        const bool on = party::player_of_seat((int)s) != party::kNobody;
        out.push_back({who + spaceext::tr("'s own empire: $", ": своя империя: $") + std::to_string(g_roster.e[s].money) +
                           (on ? "" : spaceext::tr(" (away)", " (нет в игре)")),
                       "", nullptr, 0});
        if (g_homes[s] && homes_on()) // BUG-9310
            out.push_back({"  " + std::string(spaceext::tr("homeworld: ", "родная планета: ")) + planet_label(g_homes[s]),
                           "", nullptr, 0});
    }
    return out;
}

static std::string debug(const std::vector<std::string>& a)
{
    // coop.space empires [money <n> | write | found <player> <species> | launch ...]: tests
    if (!a.empty() && a[0] == "launch") // BUG-9299: the own spaceship's offer
        return spacelaunch::debug(std::vector<std::string>(a.begin() + 1, a.end()));
    if (a.size() >= 2 && a[0] == "money") {
        const int v = atoi(a[1].c_str());
        return set_money(v) ? "empires: my money set to " + std::to_string(v) : "empires: no player empire";
    }
    if (a.size() >= 2 && a[0] == "ship" && !spaceext::is_host()) { // the ship design my empire flies (0: the newest)
        g_ship_choice = (uint32_t)strtoul(a[1].c_str(), nullptr, 16);
        name_undo();
        return "empires: my ship choice " + a[1] + " (set again in a moment)";
    }
    if (a.size() >= 2 && a[0] == "captain" && !spaceext::is_host()) { // tests: my captain's species (a creature of my library)
        g_species = (uint32_t)strtoul(a[1].c_str(), nullptr, 16);
        name_undo();
        return "empires: my captain species " + a[1] + " (set again in a moment)";
    }
    if (!a.empty() && a[0] == "hud") { // tests: the Space HUD's planet panel (its owner's name and species picture)
        static uintptr_t refresh = H->sym("SporeCoop::SpaceGameUI_UpdatePlanetPanel"), ui_get = H->sym("SporeCoop::SpaceGameUI_Get");
        void* ui = ui_get ? cdecl_call<void*>(ui_get) : nullptr;
        if (!ui || !refresh || !spaceext::in_space())
            return "empires: no Space HUD";
        if (a.size() >= 2 && a[1] == "refresh")
            thiscall<void>(refresh, ui);
        return "empires: hud " + hud_panel() + "\n";
    }
    if (!a.empty() && a[0] == "ships") {
        std::string s;
        for (auto& o : creations::options(creations::kUfo)) {
            char b[96];
            snprintf(b, sizeof b, "ship %08X '%s'\n", o.key.instance, o.name.c_str());
            s += b;
        }
        return s.empty() ? "no own ship designs\n" : s;
    }
    if (!a.empty() && a[0] == "write")
        return roster_write("test") ? "empires: written" : "empires: not written";
    // BUG-9310: home [clear <seat>]: the homeworlds; clear (host) forgets a seat's (picked again in a moment)
    if (!a.empty() && a[0] == "home") {
        if (a.size() >= 3 && a[1] == "clear" && spaceext::is_host()) {
            const int seat = atoi(a[2].c_str());
            if (seat < 1 || seat >= (int)kMaxSeats)
                return "empires: no such seat";
            g_homes[seat] = 0;
            roster_write("home cleared");
            return "empires: seat " + a[2] + "'s homeworld forgotten";
        }
        std::string o;
        void* me = player_empire();
        if (spaceext::is_host() || !spaceext::connected()) {
            for (size_t s = 1; s < kMaxSeats; s++)
                if (g_homes[s]) {
                    void* st = spacecoop::star_record(g_homes[s] & 0xFFFFFFu);
                    void* hs = me ? spacecoop::star_record(F<uint32_t>(me, 0xB0)) : nullptr;
                    float d = -1;
                    if (rd(st, 0xB0) && rd(hs, 0xB0)) {
                        const float* p = &F<float>(st, 0x3C);
                        const float* q = &F<float>(hs, 0x3C);
                        d = std::sqrt((p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2]));
                    }
                    char t[256];
                    snprintf(t, sizeof t, "home seat=%zu planet=%08X star=%u '%s' dist=%.2f owner=%08X citizens=%08X\n", s,
                             g_homes[s], g_homes[s] & 0xFFFFFFu, planet_label(g_homes[s]).c_str(), d,
                             rd(st, 0xB0) ? F<uint32_t>(st, 0x54) : 0u, rd(st, 0x80) ? F<uint32_t>(st, 0x74) : 0u);
                    o += t;
                }
        } else if (g_home_planet) {
            void* st = spacecoop::star_record(g_home_planet & 0xFFFFFFu);
            char t[256];
            snprintf(t, sizeof t, "home planet=%08X star=%u '%s' owner=%08X mine=%d citizens=%08X\n", g_home_planet,
                     g_home_planet & 0xFFFFFFu, planet_label(g_home_planet).c_str(), rd(st, 0xB0) ? F<uint32_t>(st, 0x54) : 0u,
                     (int)(rd(st, 0xB0) && me && F<uint32_t>(st, 0x54) == F<uint32_t>(me, 0x84)),
                     rd(st, 0x80) ? F<uint32_t>(st, 0x74) : 0u);
            o += t;
        }
        if (me && rd(me, 0xB8))
            o += "my_home_star=" + std::to_string(F<uint32_t>(me, 0xB0)) + "\n"; // the game's player empire's home
        return o;
    }
    // tests (a lab fixture without a guest on a species of its own): found <player id> <species hex>: that player's
    // seat gets an empire as if it played that species, then the player is told again
    if (a.size() >= 3 && a[0] == "found" && spaceext::is_host()) {
        const uint8_t p = (uint8_t)atoi(a[1].c_str());
        const int seat = party::seat_of_player(p);
        const int host_money = money_now();
        if (seat < 1 || seat >= (int)kPlaySeats || seat == host_seat() || g_roster.founded(seat) || host_money < 0)
            return "empires: no seat to found for player " + a[1];
        Empire& e = g_roster.e[seat];
        e = {true, party::roster().seats[seat].player, (uint32_t)strtoul(a[2].c_str(), nullptr, 16), start_money(host_money)};
        if (!roster_valid(g_roster)) {
            e = {};
            return "empires: not valid";
        }
        g_n.founded++;
        roster_write("test found");
        send_empire(p, seat);
        return "empires: seat " + std::to_string(seat) + " founded";
    }
    std::string s = status() + "\n";
    if (spaceext::is_host() || !spaceext::connected()) {
        const auto& seats = party::roster().seats;
        for (size_t i = 1; i < kMaxSeats; ++i) {
            if (!g_roster.e[i].founded)
                continue;
            std::vector<wire::space_diplomacy::SnapRec> t;
            if (!g_diplo[i].empty())
                ew::table_read(g_diplo[i], t);
            char b[240];
            snprintf(b, sizeof b, "empire seat=%zu name=%s species=%08X money=%d online=%d serial=%u diplo=%u\n", i,
                     i < seats.size() ? seats[i].name.c_str() : "?", g_roster.e[i].species, g_roster.e[i].money,
                     (int)(party::player_of_seat((int)i) != party::kNobody), g_wallet[i].serial, (unsigned)t.size());
            s += b;
        }
    }
    return s;
}

static bool init(const SkHost* h, SkMod* m)
{
    H = h;
    M = m;
    a_PlayerEmpire = H->sym("Simulator::GetPlayerEmpire");
    a_StrAssign = H->sym("wstring_copy");
    a_Color = H->sym("Simulator::GetCachedColorFromId");
    a_SetShip = H->sym("SporeCoop::Empire_SetSpacecraftKey");
    a_SetCaptain = H->sym("SporeCoop::cEmpire_SetCaptainKey");
    a_SpaceGame = H->sym("Simulator::cSimulatorSpaceGame::Get");
    a_SetArchetype = H->sym("SporeCoop::SpaceGame_SetArchetype");
    a_NounMgr = H->sym("Simulator::cGameNounManager::Get");
    a_StarMgr = H->sym("Simulator::cStarManager::Get"); // BUG-9310
    a_RequirePlanets = H->sym("Simulator::cStarManager::RequirePlanetsForStar");
    a_GetPlanets = H->sym("Simulator::cStarRecord::GetPlanetRecords");
    a_StarSpecies = H->sym("SporeCoop::cStarRecord_SetSpeciesKey");
    spaceext::on_save(host_save);
    spaceext::on_save(guest_save);
    return a_PlayerEmpire != 0;
}

static bool reg = spaceext::add({.name = "empires",
                                 .kind_lo = 100,
                                 .kind_hi = 109,
                                 .init = init,
                                 .tick = tick,
                                 .on_message = on_message,
                                 .on_session_end = on_session_end,
                                 .rows = rows,
                                 .status = status,
                                 .debug = debug});

} // namespace spaceempires
