// M10 script commands (SEA lane; phase A by the lead): stage the sea for screenshots and checks (VISION_PLAN 2.12,
// 15.4, 15.31). Steering aboard uses the ordinary movement (`walk east 6`, held keys): the boat follows the stick.
//   boat give <rowboat|sloop|galleon> [radius]   a boat of ours on the nearest water it can sail within radius tiles
//                                  (default 12); fails when there is none
//   boat board [i]                 step aboard boat i (default: the last one given), wherever it lies
//   boat leave                     step ashore (fails with no landing within 2 tiles)
//   sea port [n]                   teleport to the n-th nearest port (default 0) within 6000 tiles: the shore by its
//                                  berth (ew::EndlessSource::portsIn)
//   sea mistedge [in]              teleport to open water just outside the nearest hidden land's Mist (in: just inside
//                                  it), a sloop of ours there, aboard, facing the land; fails when no hidden land with
//                                  open water at its Mist lies within 30000 tiles
//   sea chart [here]               hold the sea-chart of the nearest hidden land (here: the one whose Mist the hero is in)
//   sea face <deg>                 the boat aboard's heading (0 east, 90 south)
//   sea helm <deg|off|land>        hold a course at full sail while the stick is idle (land: toward the nearest hidden
//                                  land's heart; the Mist's turn-back lets go of it)
//   expect sea aboard <0|1>        aboard a boat or not
//   expect sea mist [min]          the Mist's density here is at least min (default 0.05)
//   expect sea turned <0|1>        the Mist is turning the boat back
//   expect sea boats <n>           at least n boats owned
//   expect sea speed <min>         the boat aboard makes at least min tiles a second
// SEA lane (phase B):
//   boat moor                      aboard near a port's berth: glide into it and step ashore (fails: no berth near)
//   boat keel                      the boat aboard gets a storm keel (scripts: the open ocean)
//   sea storm [power]              a weather front comes in on the boat (power 0.2..1, default 0.9)
//   sea fish                       cast a line facing the water (fails: no water in reach)
//   sea shore                      teleport to a land tile beside the shallows, facing the water
//   sea tale                       the old sailor's tale of the nearest hidden land (the chart chain's first step)
//   sea wreck | cave               teleport to the chain's wreck / sea cave of the nearest hidden land, a galleon of ours
//                                  there, aboard
//   sea land                       teleport onto the nearest hidden land's heart (first contact)
//   sea ferry                      at a harbour: passage to the nearest other harbour (learned and paid for here)
//   sea spawn <serpent|kraken|merrow|pirates> [boss]   the sea's creatures round the boat (boss: a world boss)
//   sea sink                       the hull aboard is breached
//   expect sea fish <n>            at least n fish caught
//   expect sea chain <0..4>        the nearest hidden land's chain stands at least at this step
//   expect sea landed <0|1>        the nearest hidden land has been set foot on
//   expect sea storm <0|1>         a storm is over the sea
//   expect sea voyage <0|1>        a ferry passage is under way
//   expect sea line <phase>        the fishing line's phase (none casting waiting bite reeling landed lost)
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/sea.h"
#include "rpg/sim/sea_ops.h"
#include "rpg/sim/foes.h"
#include "rpg/view/script_api.h"
#include "rpg/world/source.h"

namespace {
int g_lastBoat = -1;

sea::Hull hullWord(const std::string& w0, bool& ok) {
  std::string w;
  for (char ch : w0) w += (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
  ok = true;
  if (w == "rowboat" || w == "row") return sea::Hull::Rowboat;
  if (w == "sloop") return sea::Hull::Sloop;
  if (w == "galleon") return sea::Hull::Galleon;
  ok = false;
  return sea::Hull::Rowboat;
}

bool cmdBoat(ScriptCtx& c) {
  Game& g = c.game;
  const std::string op = c.arg(1);
  if (op == "give") {
    bool ok = false;
    const sea::Hull h = hullWord(c.arg(2), ok);
    if (!ok) { c.fail("boat give: unknown hull '" + c.arg(2) + "'"); return true; }
    const int r = c.arg(3).empty() ? 12 : std::atoi(c.arg(3).c_str());
    g_lastBoat = SeaOps::giveBoat(g, h, r);
    if (g_lastBoat < 0) c.fail("boat give: no water for a " + std::string(sea::hullName(h)) + " within " + std::to_string(r) + " tiles");
    return true;
  }
  if (op == "board") {
    const int i = c.arg(2).empty() ? g_lastBoat : std::atoi(c.arg(2).c_str());
    if (!SeaOps::board(g, i)) c.fail("boat board: could not board boat " + std::to_string(i));
    return true;
  }
  if (op == "leave") {
    if (!SeaOps::disembark(g)) c.fail("boat leave: no landing within 2 tiles");
    return true;
  }
  if (op == "moor") {
    if (!SeaOps::moorStart(g)) c.fail("boat moor: no berth within reach");
    return true;
  }
  if (op == "keel") {
    if (!g.sea.atSea()) { c.fail("boat keel: not aboard"); return true; }
    g.sea.boats[(size_t)g.sea.aboard].upgrades |= sea::UP_KEEL;
    return true;
  }
  c.fail("boat: give|board|leave|moor|keel");
  return true;
}
EMB_SCRIPT_CMD("boat", "boat give <rowboat|sloop|galleon> [radius] | board [i] | leave (M10)", cmdBoat);

bool cmdSea(ScriptCtx& c) {
  Game& g = c.game;
  const std::string op = c.arg(1);
  if (!g.world.src || !g.world.endless) { c.fail("sea: not on the endless world"); return true; }
  ew::EndlessSource& src = *g.world.src;
  const int32_t hx = SeaOps::heroGx(g), hy = SeaOps::heroGy(g);
  if (op == "port") {
    const int n = c.arg(2).empty() ? 0 : std::atoi(c.arg(2).c_str());
    std::vector<ew::PortPlan> ports = src.portsIn(hx - 6000, hy - 6000, hx + 6000, hy + 6000);
    std::sort(ports.begin(), ports.end(), [&](const ew::PortPlan& a, const ew::PortPlan& b) {
      const double da = std::hypot((double)a.x - hx, (double)a.y - hy), db = std::hypot((double)b.x - hx, (double)b.y - hy);
      return da < db || (da == db && a.site < b.site);
    });
    if (n < 0 || n >= (int)ports.size()) { c.fail("sea port: no port " + std::to_string(n) + " within 6000 tiles"); return true; }
    const ew::PortPlan& P = ports[(size_t)n];
    // the shore between the heart and the berth: the last land tile walking from the heart toward the berth
    int32_t tx = P.x, ty = P.y;
    const double L = std::max(1.0, std::hypot((double)P.berthX - P.x, (double)P.berthY - P.y));
    for (int s = 0; s <= (int)L; s++) {
      const int32_t x = P.x + (int32_t)std::lround((P.berthX - P.x) * s / L), y = P.y + (int32_t)std::lround((P.berthY - P.y) * s / L);
      if (src.seaAt(x, y) != ew::SeaKind::Land) break;
      tx = x; ty = y;
    }
    g.teleportGlobal(tx, ty);
    return true;
  }
  if (op == "mistedge") {
    // spots on open water outside (or just inside) a hidden land's Mist with open deep water all the way in to the
    // ring's inner edge; the first is teleported to, and the line is retried each frame until the window's chunks have
    // streamed in and a sloop fits there (a few seconds at most per spot, then the next)
    struct Spot { int32_t x, y, lx, ly; };
    static std::vector<Spot> spots;
    static size_t cur = 0;
    static float since = 0;
    if (c.waited == 0) {
      spots.clear(); cur = 0; since = 0;
      const bool inside = c.arg(2) == "in";
      std::vector<ew::LandPlan> lands = src.landsIn(hx - 30000, hy - 30000, hx + 30000, hy + 30000);
      std::sort(lands.begin(), lands.end(), [&](const ew::LandPlan& a, const ew::LandPlan& b) {
        return std::hypot((double)a.cx - hx, (double)a.cy - hy) < std::hypot((double)b.cx - hx, (double)b.cy - hy);
      });
      for (const ew::LandPlan& L : lands) {
        if (!L.hidden || spots.size() >= 6) continue;
        for (int k = 0; k < 24 && spots.size() < 6; k++) {
          const double a = std::atan2((double)hy - L.cy, (double)hx - L.cx) + k * 0.2617994 * ((k & 1) ? 1 : -1) * ((k + 1) / 2);
          bool open = true;
          for (int32_t r = L.mistOut + 30; r >= L.mistIn + 40 && open; r -= 20) {
            const ew::SeaKind sk = src.seaAt(L.cx + (int32_t)std::lround(std::cos(a) * r), L.cy + (int32_t)std::lround(std::sin(a) * r));
            open = sk == ew::SeaKind::Deep || sk == ew::SeaKind::Storm;
          }
          if (!open) continue;
          const int32_t r = inside ? L.mistIn + 150 : L.mistOut + 24;
          spots.push_back({L.cx + (int32_t)std::lround(std::cos(a) * r), L.cy + (int32_t)std::lround(std::sin(a) * r), L.cx, L.cy});
          k = 24;   // (one spot a land)
        }
      }
      if (spots.empty()) { c.fail("sea mistedge: no hidden land with open water at its Mist within 30000 tiles"); return true; }
      g.teleportGlobal(spots[0].x, spots[0].y);
      return false;
    }
    if (cur >= spots.size()) { c.fail("sea mistedge: no sloop fitted at any spot"); return true; }
    const Spot& S = spots[cur];
    const ew::GTile at{S.x, S.y};
    g_lastBoat = SeaOps::giveBoat(g, sea::Hull::Sloop, 30, "", &at);
    if (g_lastBoat >= 0) g.sea.boats[(size_t)g_lastBoat].upgrades |= sea::UP_KEEL;   // (the open ocean round the Mist)
    if (g_lastBoat >= 0 && SeaOps::board(g, g_lastBoat)) {
      g.sea.live.heading = (float)std::atan2((double)S.ly - S.y, (double)S.lx - S.x);
      g.sea.live.autopilot = -100;
      g.sea.boats[(size_t)g_lastBoat].heading = g.sea.live.heading;
      return true;
    }
    if (g_lastBoat >= 0) g.sea.boats.pop_back();
    if (c.waited - since > 4.0f && ++cur < spots.size()) { since = c.waited; g.teleportGlobal(spots[cur].x, spots[cur].y); }
    return false;
  }
  if (op == "chart") {
    uint32_t land = 0;
    if (c.arg(2) == "here") land = g.sea.live.mistLand ? g.sea.live.mistLand : src.hiddenLandAt(hx, hy);
    else {
      double best = 1e18;
      for (const ew::LandPlan& L : src.landsIn(hx - 30000, hy - 30000, hx + 30000, hy + 30000))
        if (L.hidden) { const double d = std::hypot((double)L.cx - hx, (double)L.cy - hy); if (d < best) { best = d; land = L.id; } }
    }
    if (!land) { c.fail("sea chart: no hidden land"); return true; }
    sea::LandRecord& R = g.sea.landMut(land);
    R.chain = sea::Chain::Chart;
    R.flags |= sea::LF_CHART | sea::LF_RUMOURED;
    return true;
  }
  if (op == "face") {
    if (!g.sea.atSea()) { c.fail("sea face: not aboard"); return true; }
    g.sea.live.heading = (float)(std::atof(c.arg(2).c_str()) * 3.14159265 / 180.0);
    return true;
  }
  if (op == "helm") {
    if (!g.sea.atSea()) { c.fail("sea helm: not aboard"); return true; }
    const std::string w = c.arg(2);
    if (w == "off") { g.sea.live.autopilot = -100; return true; }
    if (w == "out") {   // out of the nearest port: from its pier toward its berth and on
      const Vec2 d = g.sea.live.nearBerth - g.sea.live.nearPier;
      if (!g.sea.live.nearPort || len(d) < 1.0f) { c.fail("sea helm out: no port near"); return true; }
      g.sea.live.autopilot = std::atan2(d.y, d.x);
      return true;
    }
    if (w == "land") {
      double best = 1e18;
      for (const ew::LandPlan& L : src.landsIn(hx - 30000, hy - 30000, hx + 30000, hy + 30000))
        if (L.hidden) {
          const double d = std::hypot((double)L.cx - hx, (double)L.cy - hy);
          if (d < best) { best = d; g.sea.live.autopilot = (float)std::atan2((double)L.cy - hy, (double)L.cx - hx); }
        }
      if (best >= 1e18) c.fail("sea helm land: no hidden land");
      return true;
    }
    g.sea.live.autopilot = (float)(std::atof(w.c_str()) * 3.14159265 / 180.0);
    return true;
  }
  // ---- SEA lane (phase B)
  auto nearestHidden = [&]() -> uint32_t {
    uint32_t land = 0;
    double best = 1e18;
    for (const ew::LandPlan& L : src.landsIn(hx - 30000, hy - 30000, hx + 30000, hy + 30000))
      if (L.hidden) { const double d = std::hypot((double)L.cx - hx, (double)L.cy - hy); if (d < best) { best = d; land = L.id; } }
    return land;
  };
  if (op == "storm") {
    const float pw = c.arg(2).empty() ? 0.9f : (float)std::atof(c.arg(2).c_str());
    const Vec2 wv(std::cos(g.sea.live.wind.dir), std::sin(g.sea.live.wind.dir));
    SeaOps::beginStorm(g, g.pl().p - wv * (6.0f * TILE), pw, 120.0f);
    return true;
  }
  if (op == "fish") {
    g.sea.live.autoReel = c.arg(2) == "auto";
    g.sea.live.fishCd = 0;
    if (!SeaOps::castLine(g)) c.fail("sea fish: no water in reach of a line");
    return true;
  }
  if (op == "waitline") {
    static const char* kPh[] = {"none", "casting", "waiting", "bite", "reeling", "landed", "lost"};
    return c.arg(2) == kPh[(int)g.sea.live.line.phase];
  }
  if (op == "isle") {
    // the coast of the nearest archipelago (or any other land), a sloop of ours offshore, aboard, facing the land
    std::vector<ew::LandPlan> lands = src.landsIn(hx - 20000, hy - 20000, hx + 20000, hy + 20000);
    std::sort(lands.begin(), lands.end(), [&](const ew::LandPlan& a, const ew::LandPlan& b) {
      const bool ia = a.kind == ew::LandKind::Archipelago, ib = b.kind == ew::LandKind::Archipelago;
      if (ia != ib) return ia;
      return std::hypot((double)a.cx - hx, (double)a.cy - hy) < std::hypot((double)b.cx - hx, (double)b.cy - hy);
    });
    static int32_t ix = 0, iy = 0, lx = 0, ly = 0;
    if (c.waited == 0) {
      bool found = false;
      for (const ew::LandPlan& L : lands) {
        if (L.origin || L.hidden || found) continue;
        for (int k = 0; k < 16 && !found; k++) {
          const double a = k * 0.3927;
          int32_t last = -1;
          for (int32_t r = 0; r < L.r * 2; r += 8) {
            const int32_t x = L.cx + (int32_t)std::lround(std::cos(a) * r), y = L.cy + (int32_t)std::lround(std::sin(a) * r);
            const ew::SeaKind sk = src.seaAt(x, y);
            if (sk == ew::SeaKind::Land) last = r;
            else if (last >= 0 && r - last >= 14 && (sk == ew::SeaKind::Deep || sk == ew::SeaKind::Shallow)) {   // (the coast in view)
              ix = x; iy = y; lx = L.cx + (int32_t)std::lround(std::cos(a) * last); ly = L.cy + (int32_t)std::lround(std::sin(a) * last);
              found = true;
              break;
            }
          }
        }
      }
      if (!found) { c.fail("sea isle: no other land's coast within 20000 tiles"); return true; }
      if (g.sea.atSea()) { g.sea.aboard = -1; g.pl().swim = 0; g.pl().radius = 4.5f; }
      g.teleportGlobal(ix, iy);
      return false;
    }
    const ew::GTile at{ix, iy};
    g_lastBoat = SeaOps::giveBoat(g, sea::Hull::Sloop, 20, "", &at);
    if (g_lastBoat >= 0) g.sea.boats[(size_t)g_lastBoat].upgrades |= sea::UP_KEEL;
    if (g_lastBoat >= 0 && SeaOps::board(g, g_lastBoat)) {
      g.sea.live.heading = (float)std::atan2((double)ly - iy, (double)lx - ix);
      g.sea.boats[(size_t)g_lastBoat].heading = g.sea.live.heading;
      return true;
    }
    if (g_lastBoat >= 0) g.sea.boats.pop_back();
    if (c.waited > 6.0f) c.fail("sea isle: no water for a sloop off that coast");
    return c.waited > 6.0f;
  }
  if (op == "shore") {
    // a land tile beside three tiles of shallow water, nearest the hero; the hero faces the water
    const Map& m = g.world.over;
    const int cx = (int)std::floor(g.pl().p.x / TILE), cy = (int)std::floor(g.pl().p.y / TILE);
    static const int D[4][2] = {{0, 1}, {1, 0}, {-1, 0}, {0, -1}};
    for (int r = 1; r < 120; r++)
      for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
          if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
          const int tx = cx + dx, ty = cy + dy;
          if (!m.in(tx, ty) || m.blocked(tx, ty) || groundWater(m.at(tx, ty)) || m.at(tx, ty) == Ground::Void) continue;
          for (const auto& d : D) {
            bool ok = true;
            for (int k = 1; k <= 3 && ok; k++) ok = m.in(tx + d[0] * k, ty + d[1] * k) && m.at(tx + d[0] * k, ty + d[1] * k) == Ground::Water;
            if (!ok) continue;
            g.teleportGlobal(g.world.ox + tx, g.world.oy + ty);
            g.pl().aim = Vec2((float)d[0], (float)d[1]);
            g.pl().face = d[1] > 0 ? 0 : d[1] < 0 ? 1 : d[0] > 0 ? 2 : 3;
            return true;
          }
        }
    c.fail("sea shore: no shallows near");
    return true;
  }
  if (op == "tale") {
    const uint32_t land = nearestHidden();
    if (!land) { c.fail("sea tale: no hidden land"); return true; }
    SeaOps::tellOfLand(g, land, "AN OLD SAILOR");
    return true;
  }
  if (op == "wreck" || op == "cave") {
    const uint32_t land = nearestHidden();
    ew::SitePlan wreck, cave;
    if (!land || !SeaOps::chainPlaces(g, land, wreck, cave)) { c.fail("sea " + op + ": no chain places"); return true; }
    const ew::SitePlan& P = op == "wreck" ? wreck : cave;
    if (c.waited == 0) { if (g.sea.atSea()) SeaOps::disembark(g); g.sea.aboard = -1; g.pl().swim = 0; g.pl().radius = 4.5f; g.teleportGlobal(P.ex, P.ey); return false; }
    const ew::GTile at{P.ex, P.ey};
    g_lastBoat = SeaOps::giveBoat(g, sea::Hull::Galleon, 8, "", &at);
    if (g_lastBoat >= 0 && SeaOps::board(g, g_lastBoat)) return true;
    if (g_lastBoat >= 0) g.sea.boats.pop_back();
    // (a beached wreck or a cave mouth under a crag often has no galleon's water within 8 tiles: then the hero stands
    // at the place on foot, which is what a look at it needs)
    return c.waited > 6.0f;
  }
  if (op == "land") {
    const uint32_t land = nearestHidden();
    if (!land) { c.fail("sea land: no hidden land"); return true; }
    const ew::LandPlan L = src.land(land);
    if (g.sea.atSea()) { g.sea.aboard = -1; g.pl().swim = 0; g.pl().radius = 4.5f; }
    g.teleportGlobal(L.cx, L.cy);
    g.realm.focusNow(src, L.cx, L.cy, g.day);
    return true;
  }
  if (op == "ferry") {
    if (!g.sea.live.nearPort || !g.sea.live.nearHarbour) { c.fail("sea ferry: not at a harbour"); return true; }
    ew::PortPlan T;
    double best = 1e18;
    bool have = false;
    for (const ew::PortPlan& p : src.portsIn(hx - 6000, hy - 6000, hx + 6000, hy + 6000)) {
      if (p.site == g.sea.live.nearPort || !p.harbour || src.land(p.land).hidden) continue;
      const double d = std::hypot((double)p.x - hx, (double)p.y - hy);
      if (d < best) { best = d; T = p; have = true; }
    }
    if (!have) { c.fail("sea ferry: no other harbour within 6000 tiles"); return true; }
    if (!SeaOps::beginVoyage(g, T.site, T.pierX, T.pierY, sea::fareOf((int)best), (int)best)) c.fail("sea ferry: no ferry could put out");
    return true;
  }
  if (op == "spawn") {
    const std::string w = c.arg(2);
    const int D = std::max(g.plLevel, src.danger(hx, hy));
    if (w == "pirates") { SeaOps::spawnPirates(g, g.pl().p + Vec2(std::cos(g.sea.live.heading + 1.3f), std::sin(g.sea.live.heading + 1.3f)) * (14.0f * TILE), D); return true; }
    const art::Monster m = w == "kraken" ? art::Monster::Kraken : w == "merrow" ? art::Monster::Merrow : art::Monster::SeaSerpent;
    const uint8_t kind = w == "kraken" ? 2 : w == "merrow" ? 4 : 1;
    const int n = w == "merrow" ? 3 : 1;
    for (int k = 0; k < n; k++) {
      Vec2 at;
      bool found = false;
      for (int t = 0; t < 40 && !found; t++) {
        const float an = (float)t * 0.7f + (float)k, r = (m == art::Monster::Merrow ? 5.0f : 8.0f) * TILE;
        at = g.pl().p + Vec2(std::cos(an) * r, std::sin(an) * r);
        const int tx = (int)std::floor(at.x / TILE), ty = (int)std::floor(at.y / TILE);
        found = g.world.over.in(tx, ty) && groundWater(g.world.over.at(tx, ty));
      }
      if (!found) { c.fail("sea spawn: no water near"); return true; }
      const int id = SeaOps::spawnSwimmer(g, m, at, D, kind);
      const int i = SeaOps::findActor(g, id);
      if (i >= 0 && c.arg(3) == "boss") {
        Actor& a = g.actors[(size_t)i];
        a.rank = (uint8_t)foes::Rank::WorldBoss;
        a.maxHp *= foes::rankHpMul(foes::Rank::WorldBoss) * foes::SEA_BOSS_HP_SHARE; a.hp = a.maxHp;
        a.dmg *= foes::rankDmgMul(foes::Rank::WorldBoss);
        a.scalePct = 150; a.radius += 2.0f;
        a.name = m == art::Monster::Kraken ? "THE DEEP MOTHER" : "VASHRA THE COILED DEEP";
      }
    }
    return true;
  }
  if (op == "sink") {
    if (!g.sea.atSea()) { c.fail("sea sink: not aboard"); return true; }
    SeaOps::hurtBoat(g, 1e6f, g.pl().p);
    return true;
  }
  c.fail("sea: port|mistedge|chart|face|helm|storm|fish|shore|tale|wreck|cave|land|ferry|spawn|sink");
  return true;
}
EMB_SCRIPT_CMD("sea", "sea port [n] | mistedge [in] | chart [here] | face <deg> (M10)", cmdSea);

bool expSea(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = c.arg(2);
  const std::string v = c.arg(3);
  if (what == "aboard") { if ((g.sea.atSea() ? 1 : 0) != std::atoi(v.c_str())) c.fail("expect sea aboard " + v + ": aboard is " + std::to_string(g.sea.atSea() ? 1 : 0)); return true; }
  if (what == "mist") {
    const float m = v.empty() ? 0.05f : (float)std::atof(v.c_str());
    if (g.sea.live.mist < m) c.fail("expect sea mist " + std::to_string(m) + ": it is " + std::to_string(g.sea.live.mist));
    return true;
  }
  if (what == "turned") { if ((g.sea.live.turningBack ? 1 : 0) != std::atoi(v.c_str())) c.fail("expect sea turned " + v); return true; }
  if (what == "boats") { if ((int)g.sea.boats.size() < std::atoi(v.c_str())) c.fail("expect sea boats " + v + ": " + std::to_string(g.sea.boats.size())); return true; }
  if (what == "speed") {
    if (g.sea.live.speed < (float)std::atof(v.c_str())) c.fail("expect sea speed " + v + ": " + std::to_string(g.sea.live.speed));
    return true;
  }
  // ---- SEA lane (phase B)
  if (what == "fish") { if ((int)g.sea.fishCaught < std::atoi(v.c_str())) c.fail("expect sea fish " + v + ": " + std::to_string(g.sea.fishCaught)); return true; }
  if (what == "storm") { if ((g.sea.live.storm.on ? 1 : 0) != std::atoi(v.c_str())) c.fail("expect sea storm " + v); return true; }
  if (what == "voyage") { if ((g.sea.live.voyage.on ? 1 : 0) != std::atoi(v.c_str())) c.fail("expect sea voyage " + v); return true; }
  if (what == "line") {
    static const char* kPh[] = {"none", "casting", "waiting", "bite", "reeling", "landed", "lost"};
    const int ph = (int)g.sea.live.line.phase;
    if (v != kPh[ph]) c.fail("expect sea line " + v + ": it is " + kPh[ph]);
    return true;
  }
  if (what == "chain" || what == "landed") {
    if (!g.world.src) { c.fail("expect sea " + what + ": no world"); return true; }
    const int32_t hx = SeaOps::heroGx(g), hy = SeaOps::heroGy(g);
    uint32_t land = 0;
    double best = 1e18;
    for (const ew::LandPlan& L : g.world.src->landsIn(hx - 30000, hy - 30000, hx + 30000, hy + 30000))
      if (L.hidden) { const double d = std::hypot((double)L.cx - hx, (double)L.cy - hy); if (d < best) { best = d; land = L.id; } }
    const sea::LandRecord* r = g.sea.land(land);
    if (what == "chain") { if (!r || (int)r->chain < std::atoi(v.c_str())) c.fail("expect sea chain " + v + ": " + std::to_string(r ? (int)r->chain : 0)); }
    else if (((r && (r->flags & sea::LF_LANDED)) ? 1 : 0) != std::atoi(v.c_str())) c.fail("expect sea landed " + v);
    return true;
  }
  c.fail("expect sea aboard|mist|turned|boats|speed|fish|storm|voyage|line|chain|landed");
  return true;
}
EMB_SCRIPT_CMD("expect:sea", "expect sea aboard <0|1> | mist [min] | turned <0|1> | boats <n> | speed <min> (M10)", expSea);
}  // namespace
