// Rivals in every stage (rivals.h, BUG-9306; docs/11-rivals.md#rivals-stages-race).
#include "rivals.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "cellsync.h"
#include "fixes.h"
#include "game.h"
#include "lifecycle.h"
#include "modes.h"
#include "net/protocol.h"
#include "own_progress.h"
#include "settings.h"
#include "space_ext.h"
#include "space_rivalry.h"

namespace rivals {

static const SkHost* H;
static SkMod* M;
static Ctx C;

// `coop.fix core.rivals_stages off`: every stage reads Together (0.7.15.96: the stage ends when every player is done;
// Space keeps its own switch), the twin of the tests
static bool g_fix = true;
COOP_FIX("core.rivals_stages", "BUG-9306", g_fix,
         "rivals: the stage table (rivals.cell .. rivals.space): a Rivals stage is a race (the first done wins, a catch-up timer, then everybody moves on)");

static Race g_race;            // host: the authority; guest: the host's last copy
static double g_race_got = -100; // guest: when it came
static double g_next_step, g_next_send;
static std::vector<uint8_t> g_sent_last;
static std::map<uint8_t, CountFn> g_count;
static std::map<uint8_t, std::function<bool()>> g_end;
static double g_test_timer = -1; // tests: `coop.rivals timer <s>` (the catch-up seconds instead of the setting's)
// host, the race over: its own game's condition met for it (Cell, Creature), the native end shown (Tribe, Civ)
static uint8_t g_filled_stage, g_ended_stage;
static double g_fill_next;
static int g_fill_tries;
static uint32_t g_steps, g_sent, g_got, g_wins, g_overs, g_fills, g_ends, g_space_asks, g_space_follows;
static std::string g_last_event;
// Space: the setting and the rivalry mode follow each other (host)
static int g_sp_want = -1, g_sp_is = -1;
static double g_sp_next;

static bool connected() { return C.sess && C.sess->connected(); }
static bool is_host() { return connected() && C.sess->role() == net::Role::Host; }
static bool is_guest() { return connected() && C.sess->role() == net::Role::Client; }
static size_t players() { return connected() ? C.sess->players().size() : 0; }
static std::string name_of(uint8_t id)
{
    std::string n = C.sess ? C.sess->name_of(id) : "";
    return n.empty() ? "player " + std::to_string(id) : n;
}
static void feed(const std::string& s)
{
    g_last_event = s;
    if (C.feed)
        C.feed(s);
}

static settings::Id row_of(uint8_t s)
{
    switch (s) {
    case kCell: return settings::kRivalsCell;
    case kCreature: return settings::kRivalsCreature;
    case kTribe: return settings::kRivalsTribe;
    case kCiv: return settings::kRivalsCiv;
    case kSpace: return settings::kRivalsSpace;
    }
    return settings::kCount;
}

bool stage_rivals(uint8_t s)
{
    settings::Id id = row_of(s);
    return g_fix && id != settings::kCount && settings::get(id) == 1;
}

bool here() { return players() >= 2 && stage_rivals(stage_of_mode(game::mode_id())); }

bool race_on(uint32_t mode)
{
    uint8_t s = stage_of_mode(mode);
    return s != kNone && s != kSpace && players() >= 2 && stage_rivals(s);
}

static double timer_s()
{
    return g_test_timer > 0 ? g_test_timer : catch_up_seconds(settings::get(settings::kRivalsCatchUp));
}

bool race_over(uint32_t mode, std::string* why)
{
    uint8_t s = stage_of_mode(mode);
    if (why)
        why->clear();
    if (g_race.stage != s || (is_guest() && H->time_seconds() - g_race_got > 6)) {
        if (why)
            *why = "nobody has won the stage yet";
        return false;
    }
    if (g_race.over)
        return true;
    if (why) {
        int left = seconds_left(g_race, H->time_seconds());
        *why = left < 0 ? "nobody has won the stage yet"
                        : name_of(g_race.winner) + " won it, " + clock_text(left) + " left to catch up";
    }
    return false;
}

std::string line()
{
    uint32_t m = game::mode_id();
    if (!race_on(m) || g_race.stage != stage_of_mode(m))
        return "";
    const double now = H->time_seconds();
    std::string s = std::string("Rivals, ") + stage_name(g_race.stage) + ": ";
    if (g_race.over)
        return s + (g_race.why == 1 ? "everybody is done" : "time is up") + ", everybody moves on to the " +
               next_stage_name(g_race.stage) + " stage";
    if (g_race.winner != 0xFF)
        s += name_of(g_race.winner) + " won, " + clock_text(seconds_left(g_race, now)) + " to catch up";
    else
        s += "the first to finish wins";
    std::string ps;
    for (const auto& p : g_race.racers) {
        if (!ps.empty())
            ps += ", ";
        ps += name_of(p.id) + " " + (p.done ? std::string("done") : std::to_string(p.pct) + "%");
    }
    return ps.empty() ? s : s + " (" + ps + ")";
}

// ---- the texts of what happened (every machine, from its own copy of the race)
static void announce(const Race& old, const Race& now_r)
{
    if (now_r.stage == kNone || now_r.stage != stage_of_mode(game::mode_id()))
        return;
    const uint8_t my = C.sess ? C.sess->my_id() : 0;
    const std::string st = stage_name(now_r.stage), next = next_stage_name(now_r.stage);
    const bool same = old.stage == now_r.stage;
    if (now_r.winner != 0xFF && (!same || old.winner == 0xFF)) {
        g_wins++;
        std::string left = clock_text((int)timer_s());
        if (is_guest())
            left = clock_text(std::max(0, seconds_left(now_r, H->time_seconds())));
        feed(now_r.winner == my ? "You win the " + st + " stage! The others have " + left + " to catch up"
                                : name_of(now_r.winner) + " wins the " + st + " stage! " + left + " left to catch up");
    }
    for (size_t i = 1; i < now_r.order.size(); i++)
        if (!same || i >= old.order.size()) {
            uint8_t id = now_r.order[i];
            feed((id == my ? std::string("You finish") : name_of(id) + " finishes") + " the " + st + " stage in place " +
                 std::to_string(i + 1));
        }
    if (now_r.over && (!same || !old.over)) {
        g_overs++;
        std::string how = now_r.why == 1 ? "Everybody finished the " + st + " stage" : "Time is up in the " + st + " stage";
        if (is_host()) {
            const char* act = now_r.stage == kCell       ? "click History's check"
                              : now_r.stage == kCreature ? "use the evolve button"
                              : now_r.stage == kTribe    ? "use the advance button"
                                                         : "use the end window's button";
            feed(how + ": " + act + " to move everybody on to the " + next + " stage (a vote follows)");
        } else
            feed(how + ": " + name_of(0) + " (host) moves everybody on to the " + next + " stage (a vote follows)");
    }
}

// ---- host: every player's condition now
static bool racers_now(uint8_t s, std::vector<Racer>& out)
{
    out.clear();
    if (s == kCell || s == kCreature) {
        std::vector<own_progress::Player> t;
        if (!own_progress::table(t))
            return false;
        const uint32_t m = s == kCell ? own_progress::kCell : own_progress::kCreature;
        for (const auto& p : t) {
            Racer r;
            r.id = p.id;
            r.done = p.reported && p.mode == m && p.full;
            r.pct = (uint8_t)std::clamp(p.reported && p.mode == m ? p.pct : 0, 0, 100);
            out.push_back(r);
        }
        return true;
    }
    auto it = g_count.find(s);
    if (it == g_count.end() || !it->second)
        return false;
    std::vector<uint8_t> ids;
    std::vector<int> won;
    int total = 0, free_left = 0;
    if (!it->second(ids, won, total, free_left) || ids.size() != won.size())
        return false;
    for (uint8_t id : ids) {
        Racer r;
        r.id = id;
        out.push_back(r);
    }
    subjugation_done(out, won, total, free_left);
    return true;
}

static void send_race(double now, bool force)
{
    net::Writer w;
    write_race(w, g_race, now);
    if (!force && w.data == g_sent_last && now < g_next_send)
        return;
    g_sent_last = w.data;
    g_next_send = now + 2;
    C.sess->broadcast_reliable(net::MSG_RIVALS, w);
    g_sent++;
}

// host, the race over: its own game's condition (Cell: the food bar, Creature: the brain) is met for it when it was not:
// the host's game is the one whose native end moves everybody on. The DNA stays what it was (Creature), the bar fills.
static void fill_host(uint8_t s, double now)
{
    if (g_filled_stage == s || now < g_fill_next)
        return;
    g_fill_next = now + 3;
    own_progress::Values v;
    if (!own_progress::local_values(v)) {
        if (++g_fill_tries > 40)
            g_filled_stage = s; // (gave up: no cell or avatar for two minutes)
        return;
    }
    if (own_progress::full(v)) {
        g_filled_stage = s;
        return;
    }
    if (s == kCell) {
        if (cellsync::set_food_total(own_progress::kCellFull)) {
            g_fills++;
            H->log(M, SK_INFO, "rivals: the Cell race is over: the host's food %d -> %d (its game ends the stage)", v.food,
                   own_progress::kCellFull);
            feed("Your cell's bar is filled: the stage ends for everybody");
        }
        return;
    }
    if (s == kCreature) { // a step at a time: each brain upgrade plays its caption
        const float dna = game::dna();
        game::dna_add_raw(150.f);
        game::dna_set_raw(dna);
        g_fills++;
        H->log(M, SK_INFO, "rivals: the Creature race is over: the host's bar +150 (brain %d, DNA kept %.0f)", (int)v.level, dna);
    }
}

static void host_tick(double now)
{
    const uint32_t m = game::mode_id();
    const uint8_t s = stage_of_mode(m);
    if (!race_on(m)) {
        if (g_race.stage != kNone && s != g_race.stage) {
            g_race = Race{};
            send_race(now, true);
        }
        return;
    }
    if (now < g_next_step)
        return;
    g_next_step = now + 1;
    std::vector<Racer> rs;
    if (!racers_now(s, rs))
        return;
    Race old = g_race;
    auto ev = step(g_race, s, rs, now, timer_s());
    g_steps++;
    for (const auto& e : ev)
        H->log(M, SK_INFO, "rivals: %s race: %s %s", stage_name(s),
               e.kind == kWon ? "won by" : e.kind == kPlaced ? "finished by" : "over:",
               e.kind == kOver ? (e.player == 1 ? "everybody done" : "the timer ran out") : name_of(e.player).c_str());
    announce(old, g_race);
    send_race(now, !ev.empty());
    if (g_race.over) {
        if (s == kCell || s == kCreature)
            fill_host(s, now);
        else if (g_ended_stage != s) {
            auto it = g_end.find(s);
            if (it != g_end.end() && it->second && it->second()) {
                g_ended_stage = s;
                g_ends++;
            }
        }
    } else {
        g_filled_stage = g_ended_stage = kNone;
        g_fill_tries = 0;
    }
}

// ---- Space: rivals.space is space_rivalry's mode (the host's switch)
static void space_tick(double now)
{
    if (!g_fix || !is_host() || game::mode_id() != modes::kSpace || !spaceext::in_space() || now < g_sp_next) {
        if (game::mode_id() != modes::kSpace)
            g_sp_want = g_sp_is = -1;
        return;
    }
    g_sp_next = now + 1;
    const int want = settings::get(settings::kRivalsSpace), is = spaceext::rivals() ? 1 : 0;
    auto ask = [&](int v) {
        g_space_asks++;
        H->run_outside_frame([](void* p) {
            const bool on = p != nullptr;
            std::string e = rivalry::set_mode(on);
            H->log(M, SK_INFO, "rivals: the Space stage's setting asks for the %s mode: %s", on ? "rivals" : "allies",
                   e.empty() ? "switched" : e.c_str());
        }, v ? (void*)1 : nullptr);
    };
    if (g_sp_want < 0) { // the first look in this Space stage: the setting decides
        g_sp_want = want;
        g_sp_is = is;
        if (want != is)
            ask(want);
        return;
    }
    if (want != g_sp_want) { // the host changed the setting (the settings page, coop.settings)
        g_sp_want = want;
        if (want != is)
            ask(want);
    } else if (is != g_sp_is && is != want) { // the mode changed otherwise (the Space page's button, a kept session)
        std::string why;
        if (settings::set(settings::kRivalsSpace, is, true, &why)) {
            g_space_follows++;
            g_sp_want = is;
        }
    }
    g_sp_is = is;
}

void tick(double now)
{
    space_tick(now);
    if (!connected())
        return;
    if (is_host())
        host_tick(now);
}

void on_message(uint8_t from, net::Reader& r)
{
    if (!is_guest() || from != 0)
        return;
    Race got;
    const double now = H->time_seconds();
    if (!read_race(r, got, now)) {
        H->log(M, SK_WARN, "rivals: a malformed race from the host dropped");
        return;
    }
    g_got++;
    Race old = g_race;
    g_race = got;
    g_race_got = now;
    announce(old, g_race);
}

void on_session_end()
{
    g_race = Race{};
    g_race_got = -100;
    g_sent_last.clear();
    g_filled_stage = g_ended_stage = kNone;
    g_fill_tries = 0;
    g_sp_want = g_sp_is = -1;
}
LIFECYCLE_HOOK("rivals.session", lifecycle::kSessionEnd, -10, [](const lifecycle::Ctx&) { on_session_end(); }, false);

void set_counter(uint8_t stage, CountFn fn) { g_count[stage] = std::move(fn); }
void set_ender(uint8_t stage, std::function<bool()> fn) { g_end[stage] = std::move(fn); }

void init(const SkHost* host, SkMod* mod, const Ctx& ctx)
{
    H = host;
    M = mod;
    C = ctx;
}

std::string status()
{
    const double now = H->time_seconds();
    std::string s = "rivals: fix=" + std::to_string((int)g_fix) + " role=" + (is_host() ? "host" : is_guest() ? "guest" : "none") +
                    " table";
    for (uint8_t st = kCell; st <= kSpace; st++)
        s += std::string(" ") + stage_name(st) + "=" + (stage_rivals(st) ? "rivals" : "together");
    char b[400];
    snprintf(b, sizeof b, " timer=%.0f here=%d race_on=%d\n", timer_s(), (int)here(), (int)race_on(game::mode_id()));
    s += b;
    snprintf(b, sizeof b,
             "  race: stage=%s winner=%s left=%d over=%d why=%d order=%u racers=%u age=%.1f\n", stage_name(g_race.stage),
             g_race.winner == 0xFF ? "-" : name_of(g_race.winner).c_str(), seconds_left(g_race, now), (int)g_race.over,
             (int)g_race.why, (unsigned)g_race.order.size(), (unsigned)g_race.racers.size(),
             is_guest() ? now - g_race_got : 0.0);
    s += b;
    for (const auto& p : g_race.racers) {
        auto it = std::find(g_race.order.begin(), g_race.order.end(), p.id);
        snprintf(b, sizeof b, "  racer %u %s: done=%d pct=%u place=%d\n", (unsigned)p.id, name_of(p.id).c_str(), (int)p.done,
                 (unsigned)p.pct, it == g_race.order.end() ? 0 : (int)(it - g_race.order.begin() + 1));
        s += b;
    }
    std::string why;
    bool over = race_over(game::mode_id(), &why);
    snprintf(b, sizeof b,
             "  can_end=%d (%s) steps=%u sent=%u got=%u wins=%u overs=%u fills=%u ends=%u space_asks=%u space_follows=%u "
             "space_rivals=%d\n",
             (int)over, why.c_str(), g_steps, g_sent, g_got, g_wins, g_overs, g_fills, g_ends, g_space_asks, g_space_follows,
             (int)spaceext::rivals());
    s += b;
    s += "  line: " + line() + "\n  last: " + g_last_event + "\n";
    return s;
}

int command(int argc, const char* const* argv, std::string& out)
{
    std::string sub = argc > 1 ? argv[1] : "";
    if (sub == "set" && argc > 3) { // set <stage> together|rivals: the host's table (as the settings page)
        std::string st = argv[2];
        uint8_t s = kNone;
        for (uint8_t i = kCell; i <= kSpace; i++) {
            std::string n = stage_name(i);
            for (auto& c : n)
                c = (char)tolower((unsigned char)c);
            if (n == st || n.substr(0, 3) == st)
                s = i;
        }
        std::string v = argv[3], why;
        if (s == kNone || (v != "together" && v != "rivals")) {
            out = "usage: coop.rivals set cell|creature|tribe|civ|space together|rivals";
            return 1;
        }
        if (!settings::set(row_of(s), v == "rivals" ? 1 : 0, false, &why)) {
            out = "refused: " + why;
            return 1;
        }
        out = std::string(stage_name(s)) + ": " + v + "\n";
    } else if (sub == "timer" && argc > 2) { // tests: the catch-up seconds (<= 0: the setting's again)
        g_test_timer = atof(argv[2]);
        out = "catch-up timer " + std::to_string((int)timer_s()) + " s\n";
    } else if (!sub.empty() && sub != "status") {
        out = "coop.rivals [status | set <stage> together|rivals | timer <seconds> (tests)]";
        return 1;
    }
    out += status();
    return 0;
}

} // namespace rivals
