// A partner's own spaceship when it joins a game that is already in Space (BUG-9299, docs/re/space.md#space-late-launch).
//
// A guest with its own nation designs its own spaceship at the Civ -> Space vote (civ_launch.cpp, BUG-9295). A partner
// who joins when the host is already in Space, or who had no launch of its own, got the host's ship: its own empire
// (space_empires.cpp: its money, its species as captain) flew the newest ship design of its library, or the save's (the
// host's) when it had none. Lineages (PR #196) answers this with a retained Civilization checkpoint the host sends;
// written new here: the game's own spaceship editor (0x96B24187, the editor of the end of Civ) opens from Space.
// * Guest: in the host's Space world with an empire of its own (space_empires K_EMPIRE have 1, its name, ship and captain
//   set) and no launch decided for this planet (worlds.ini [late_launch] <world crc>): an offer ("Design your own
//   spaceship": a line, the co-op window's Space page with Design / Not now, `coop.space empires launch go|no`). Design
//   opens the spaceship editor (an EditorRequest with the Space mode as its return mode, past the vote: one player's own
//   design); its Done saves the design into the player's own library and returns to Space. The newest own ship design
//   is the one space_empires gives the player's empire (Empire_SetSpacecraftKey), so the ship flies at once on this
//   game and on the others (the squadron's mirror of this player has its model); the captain is the player's species.
//   The planet's note keeps "done <ship>" or "no"; the editor closed without a new design keeps the offer.
// * Host: the guest tells each step (K_LAUNCH): a line for everybody ("Bob designs its own spaceship", "Bob flies its
//   own spaceship"), the state by player in `coop.space empires launch` and on the Space page.
// `coop.fix space.late_launch off`: no offer (the partner flies what its empire had: 0.7.15.96).
#include "space_launch.h"

#include <windows.h>

#include <cstdio>
#include <map>

#include "call_guard.h"
#include "civ_launch.h"
#include "coop_ui.h"
#include "creations.h"
#include "designs.h"
#include "fixes.h"
#include "game.h"
#include "modes.h"
#include "own_world.h"
#include "sporekit_game.hpp"
#include "world.h"

namespace coop_core {
extern bool g_vote_bypass; // vote.cpp: the next Submit is not a vote (a player's own editor)
void host_busy_guest_long_wait(uint8_t player, bool on); // host_busy.cpp, BUG-9305
void host_busy_flush_now();
}

namespace spacelaunch {

namespace ew = wire::space_empires;

bool g_fix = true;
COOP_FIX("space.late_launch", "BUG-9299", g_fix,
         "space: a partner in the host's Space world who has its own empire is offered the spaceship editor to design "
         "its own ship (a partner who joined after the Civ launch)");

// BUG-9305: the spaceship editor's first load stood a guest's main thread 36 s on a busy PC (lane A with three other
// lanes running): the host heard nothing for 30 s and ended the guest's session. The guest now tells the host (K_LAUNCH
// state 2, sent and flushed) a second before it asks for the editor, and the host waits up to 180 s for its packets until
// the guest tells another state (host_busy.cpp). Off (on the host): the session's 30 s.
static bool g_long_fix = true;
COOP_FIX("space.launch_long_wait", "BUG-9305", g_long_fix,
         "space: the host waits longer for a guest whose own spaceship editor loads (it can stand that game 30 s and more)");

constexpr const char* kNote = "late_launch"; // worlds.ini section: "done <ship hex>" | "no"
constexpr uint32_t kKind = 105;              // K_LAUNCH (space_empires.cpp enum Kind)
constexpr double kSettle = 8.0;              // the empire settled this long before the offer
constexpr double kOpenWait = 30.0;           // the editor did not open after Design: the offer again (and
constexpr uint32_t kOpenFrames = 300;        // this many frames in Space since the request: a long stand is no frames)
constexpr double kBackWait = 20.0;           // back in Space: the new design shows in the library within this

enum class Phase : uint8_t { Idle, Offered, Opening, Editor, Back };
static Phase g_phase = Phase::Idle;
static uint32_t g_epoch;       // the Space world the offer belongs to
static uint32_t g_world;       // its planet (modes::current_world)
static double g_since, g_settled_at;
static uint32_t g_before;      // the newest own ship design before the editor
static uint32_t g_ret_mode;    // tests: the request's return mode (0: Space)
static double g_submit_at;     // Opening: the editor is asked for at this time (0: asked)
static uint32_t g_open_frames; // Opening: frames in Space since the request
static Me g_me;
static uint32_t g_report;      // a new design: K_LAUNCH done is sent when the empire flies it (0: none due)
static double g_report_at;
static uint32_t g_offers, g_opens, g_editors, g_done, g_declined, g_cancelled, g_open_fail, g_sent;
static std::string g_last = "-";

struct Seen {
    uint8_t state = 0;
    uint32_t ship = 0, captain = 0;
    std::string name;
};
static std::map<uint8_t, Seen> g_host; // host: by player

static const SkHost* H() { return spaceext::host(); }
static SkMod* M() { return spaceext::mod(); }

static const char* phase_name(Phase p)
{
    switch (p) {
    case Phase::Offered: return "offered";
    case Phase::Opening: return "opening";
    case Phase::Editor: return "editor";
    case Phase::Back: return "back";
    default: return "idle";
    }
}

static uint32_t newest_ship()
{
    auto o = creations::options(creations::kUfo);
    return o.empty() ? 0 : o.front().key.instance;
}

static void send_state(uint8_t state, uint32_t ship = 0, uint32_t captain = 0)
{
    ew::Launch m;
    m.to = 0;
    m.serial = g_me.serial;
    m.state = state;
    m.ship = ship;
    m.captain = captain;
    net::Writer w;
    m.write(w);
    spaceext::send((uint8_t)kKind, w);
    g_sent++;
}

static void note(const std::string& v)
{
    if (g_world)
        creations::world_note_set(kNote, g_world, v);
}

static void set_phase(Phase p, const char* why)
{
    if (p == g_phase)
        return;
    H()->log(M(), SK_INFO, "launch: %s -> %s (%s)", phase_name(g_phase), phase_name(p), why);
    g_phase = p;
    g_since = H()->time_seconds();
    g_last = why;
}

// outside the frame: the game's spaceship editor for a new design, its return mode Space (the game's own request at the
// end of Civ returns to Civ, docs/re/civ.md#civ-launch-pause), past the vote (a player's own design)
static void open_task(void*)
{
    if (g_phase != Phase::Opening || game::mode_id() != modes::kSpace)
        return;
    const SkHost* h = H();
    uintptr_t alloc = h->sym("GeneralAllocator::new_"), ctor = h->sym("EditorRequest_ctor"),
              submit = h->sym("Editors::EditorRequest::Submit");
    void* req = alloc && ctor && submit ? skg::cdecl_call<void*>(alloc, (size_t)0x9C, "EditorRequest", 0, 0, 0, 0) : nullptr;
    if (!req) {
        g_open_fail++;
        set_phase(Phase::Offered, "no editor request here");
        return;
    }
    skg::thiscall<void*>(ctor, req);
    uint8_t* b = (uint8_t*)req;
    H()->log(M(), SK_INFO,
             "launch: the request as built: +0x1C %08X +0x34..+0x3D %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X "
             "+0x90 %08X",
             *(uint32_t*)(b + 0x1C), b[0x34], b[0x35], b[0x36], b[0x37], b[0x38], b[0x39], b[0x3A], b[0x3B], b[0x3C], b[0x3D],
             *(uint32_t*)(b + 0x90));
    *(uint32_t*)(b + 0xC) = designs::kShipEditor;
    *(uint32_t*)(b + 0x10) = *(uint32_t*)(b + 0x14) = *(uint32_t*)(b + 0x18) = 0; // a new design
    *(uint32_t*)(b + 0x1C) = g_ret_mode ? g_ret_mode : modes::kSpace;
    g_before = newest_ship();
    bool r;
    {
        callguard::Hold bypass(coop_core::g_vote_bypass);
        r = skg::cdecl_call<bool>(submit, req);
    }
    g_opens++;
    H()->log(M(), SK_INFO, "launch: my spaceship editor requested (return mode %08X, newest ship before %08X) -> %d",
             *(uint32_t*)(b + 0x1C), g_before, (int)r);
    if (!r) {
        g_open_fail++;
        set_phase(Phase::Offered, "the editor request was refused");
    }
}

static bool may_offer(const Me& me)
{
    if (!g_fix || !me.own || !me.settled || own_world::space_other() || !spaceext::in_space())
        return false;
    if (civ::launch::running())
        return false; // BUG-9295's own launch from Civ is under way: its ship is its own
    const uint32_t w = modes::current_world();
    return w && creations::world_note(kNote, w).empty();
}

void guest_tick(double now, const Me& me)
{
    g_me = me;
    if (g_report && ((me.settled && me.ship == g_report) || now - g_report_at > 15.0)) {
        H()->log(M(), SK_INFO, "launch: my empire flies %08X (my design %08X), captain %08X: the host is told", me.ship,
                 g_report, me.captain);
        send_state(ew::kLaunchDone, me.ship, me.captain);
        g_report = 0;
    }
    const uint32_t mode = game::mode_id(), epoch = spaceext::world_epoch();
    if (g_phase != Phase::Idle && (!g_fix || epoch != g_epoch || mode == modes::kMenu || mode == modes::kLoad)) {
        set_phase(Phase::Idle, !g_fix ? "switched off" : "another world");
        return;
    }
    switch (g_phase) {
    case Phase::Idle:
        if (!may_offer(me)) {
            g_settled_at = 0;
            return;
        }
        if (!g_settled_at)
            g_settled_at = now;
        if (now - g_settled_at < kSettle || world::cinematic_playing())
            return;
        g_epoch = epoch;
        g_world = modes::current_world();
        g_offers++;
        set_phase(Phase::Offered, "my own empire flies a ship I did not design here");
        spaceext::notice(spaceext::tr("Design your own spaceship: co-op window (F9) > Space > Design, or keep this one: Not now",
                                      "Создайте свой корабль: окно кооператива (F9) > Космос > Создать, или оставьте этот: Не сейчас"));
        send_state(ew::kLaunchOffered);
        return;
    case Phase::Offered:
        return;
    case Phase::Opening:
        if (g_submit_at && now >= g_submit_at) { // BUG-9305: a second after the host was told
            g_submit_at = 0;
            g_open_frames = 0;
            g_since = now;
            H()->run_outside_frame(open_task, nullptr);
        } else if (!g_submit_at && mode == modes::kSpace)
            g_open_frames++;
        if (mode == modes::kEditor || mode == modes::kEdit) {
            g_editors++;
            set_phase(Phase::Editor, "my spaceship editor is open");
        } else if (!g_submit_at && now - g_since > kOpenWait && g_open_frames > kOpenFrames) {
            g_open_fail++;
            set_phase(Phase::Offered, "the editor did not open");
            send_state(ew::kLaunchOffered); // (the host's long wait ends)
        }
        return;
    case Phase::Editor:
        if (mode == modes::kSpace)
            set_phase(Phase::Back, "back in Space");
        return;
    case Phase::Back: {
        const uint32_t ship = newest_ship();
        if (ship && ship != g_before) {
            g_done++;
            char v[24];
            snprintf(v, sizeof v, "done %08X", ship);
            note(v);
            spaceempires::ship_changed();
            set_phase(Phase::Idle, "a new ship design");
            g_settled_at = 0;
            char b[96];
            snprintf(b, sizeof b, "launch: my own spaceship %08X (before %08X)", ship, g_before);
            H()->log(M(), SK_INFO, "%s", b);
            spaceext::notice(spaceext::tr("Your own spaceship is ready: your empire flies it now",
                                          "Ваш корабль готов: ваша империя летает на нём"));
            g_report = ship;
            g_report_at = now;
            return;
        }
        if (now - g_since > kBackWait) {
            g_cancelled++;
            set_phase(Phase::Offered, "no new ship design (the editor was closed)");
            send_state(ew::kLaunchOffered); // (the host's long wait ends)
            spaceext::notice(spaceext::tr("No new spaceship saved: Design (F9 > Space) to try again",
                                          "Новый корабль не сохранён: Создать (F9 > Космос), чтобы попробовать снова"));
        }
        return;
    }
    }
}

// ---------------- the player's choice (outside the frame: a row's button, a command) ----------------
static std::string go()
{
    if (g_phase != Phase::Offered)
        return "launch: no offer now (" + std::string(phase_name(g_phase)) + ")";
    if (game::mode_id() != modes::kSpace || world::cinematic_playing())
        return "launch: not now (not in Space or a cinematic plays)";
    set_phase(Phase::Opening, "Design");
    coop_ui::open(false); // the co-op window stayed over the editor (its lobby page)
    send_state(ew::kLaunchEditor); // BUG-9305: the host waits longer while the editor loads
    coop_core::host_busy_flush_now();
    g_submit_at = H()->time_seconds() + 1.0;
    return "launch: my spaceship editor opens";
}
static std::string no()
{
    if (g_phase != Phase::Offered)
        return "launch: no offer now (" + std::string(phase_name(g_phase)) + ")";
    g_declined++;
    note("no");
    set_phase(Phase::Idle, "Not now");
    send_state(ew::kLaunchDeclined, g_me.ship, g_me.captain);
    return "launch: I keep the ship my empire has";
}

void host_message(uint8_t origin, const ew::Launch& m)
{
    Seen& s = g_host[origin];
    const bool news = s.state != m.state || s.ship != m.ship;
    s.state = m.state;
    s.ship = m.ship;
    s.captain = m.captain;
    s.name = spaceext::name_of(origin);
    coop_core::host_busy_guest_long_wait(origin, g_long_fix && m.state == ew::kLaunchEditor); // BUG-9305
    H()->log(M(), SK_INFO, "launch: player %u (%s): state %u ship %08X captain %08X", origin, s.name.c_str(), m.state,
             m.ship, m.captain);
    if (!news)
        return;
    if (m.state == ew::kLaunchEditor)
        spaceext::notice(s.name + spaceext::tr(" designs their own spaceship", " создаёт свой корабль"));
    else if (m.state == ew::kLaunchDone)
        spaceext::notice(s.name + spaceext::tr(" flies their own spaceship now", " теперь летает на своём корабле"));
}

// lifecycle-exempt: a Space feature's part (space_ext.h), run by space_empires' on_session_end
void on_session_end()
{
    g_submit_at = 0;
    if (g_phase != Phase::Idle)
        set_phase(Phase::Idle, "the session ended");
    g_host.clear();
    g_report = 0;
    g_settled_at = 0;
    g_me = {};
}

std::vector<spaceext::Row> rows()
{
    std::vector<spaceext::Row> out;
    if (!g_fix)
        return out;
    if (spaceext::is_host()) {
        for (auto& [id, s] : g_host)
            if (s.state == ew::kLaunchDone || s.state == ew::kLaunchEditor)
                out.push_back({s.name + (s.state == ew::kLaunchDone ? spaceext::tr(" flies their own spaceship", " летает на своём корабле")
                                                                    : spaceext::tr(" designs their own spaceship", " создаёт свой корабль")),
                               "", nullptr, 0});
        return out;
    }
    if (g_phase == Phase::Offered) {
        out.push_back({spaceext::tr("Design your own spaceship (your empire flies it)", "Создайте свой корабль (ваша империя летает на нём)"),
                       spaceext::tr("Design", "Создать"), [] { go(); }, 0xFFFFD75Fu});
        out.push_back({spaceext::tr("Keep the ship your empire has", "Оставить корабль вашей империи"),
                       spaceext::tr("Not now", "Не сейчас"), [] { no(); }, 0});
    } else if (g_phase == Phase::Opening || g_phase == Phase::Editor || g_phase == Phase::Back)
        out.push_back({spaceext::tr("Your spaceship editor opens", "Редактор корабля открывается"), "", nullptr, 0});
    return out;
}

std::string status()
{
    char b[400];
    snprintf(b, sizeof b,
             "launch: fix=%d phase=%s world=%08X note='%s' before=%08X offers=%u opens=%u editors=%u done=%u declined=%u "
             "cancelled=%u open_fail=%u sent=%u last=%s",
             (int)g_fix, phase_name(g_phase), modes::current_world(),
             modes::current_world() ? creations::world_note(kNote, modes::current_world()).c_str() : "", g_before, g_offers,
             g_opens, g_editors, g_done, g_declined, g_cancelled, g_open_fail, g_sent, g_last.c_str());
    std::string s = b;
    for (auto& [id, x] : g_host) {
        snprintf(b, sizeof b, "\nlaunch player=%u name=%s state=%u ship=%08X captain=%08X", id, x.name.c_str(), x.state,
                 x.ship, x.captain);
        s += b;
    }
    return s;
}

std::string debug(const std::vector<std::string>& a)
{
    if (!a.empty() && a[0] == "go")
        return go();
    if (!a.empty() && a[0] == "no")
        return no();
    if (!a.empty() && a[0] == "offer" && !spaceext::is_host()) { // tests: the planet's note forgotten, offered again
        if (modes::current_world())
            creations::world_note_set(kNote, modes::current_world(), "");
        if (g_phase == Phase::Offered)
            set_phase(Phase::Idle, "offer again");
        g_settled_at = 0;
        return "launch: this planet's choice forgotten (offered again in a moment)";
    }
    if (a.size() >= 2 && a[0] == "ret") { // tests: the editor request's return mode (0: Space)
        g_ret_mode = (uint32_t)strtoul(a[1].c_str(), nullptr, 16);
        return "launch: return mode " + a[1];
    }
    return status() + "\n";
}

} // namespace spacelaunch
