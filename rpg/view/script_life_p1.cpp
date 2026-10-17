// P1 "Fresh Roads" script commands for the LIFE lane's stumble-on hooks (rpg/sim/life_game.cpp, life.h). `embervale
// --script-help` lists them.
//   incident thief|cart|brawl       stage a street incident near the player now (the hour's own roll aside)
//   gotoincident [n]                stand n tiles (default 3) south of where the incident is (the thief, the cart, the brawl)
//   incidenttalk                    talk to the incident's person (their plea, the offer to step in)
//   incidenthelp [open]             take it on (I'LL CATCH THEM! / LEND A HAND / BREAK IT UP); open: keep their answer up
//   catchthief                      walk up behind the running thief (the catch: the errand pays on the spot)
//   overhear                        the nearest resident lets slip a need of someone in town (a journal LEAD); prints it
//   leads                           print the journal's leads
//   expect incident none|thief|cart|brawl [engaged]
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/sim/jobs.h"
#include "rpg/sim/life.h"
#include "rpg/view/script_api.h"

namespace {

int kindOf(const std::string& w) {
  if (w == "thief") return life::INC_THIEF;
  if (w == "cart") return life::INC_CART;
  if (w == "brawl") return life::INC_BRAWL;
  if (w == "none") return life::INC_NONE;
  return -1;
}
const char* kindName(int k) { return k == life::INC_THIEF ? "thief" : k == life::INC_CART ? "cart" : k == life::INC_BRAWL ? "brawl" : "none"; }
Actor* byId(Game& g, int id) {
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].id == id) return &g.actors[k];
  return nullptr;
}

bool cmdIncident(ScriptCtx& c) {
  const int k = kindOf(c.arg(1));
  if (k <= 0) { c.fail("incident thief|cart|brawl"); return true; }
  if (!life::forceIncident(c.game, k)) { c.fail(std::string("incident ") + c.arg(1) + ": nobody suitable near (or no cart, no way out of town)"); return true; }
  const life::IncidentView v = life::incidentNow(c.game);
  std::printf("incident: %s at %d,%d (victim %d, other %d)\n", kindName(v.kind), v.gx, v.gy, v.victim, v.other);
  return true;
}
EMB_SCRIPT_CMD("incident", "incident thief|cart|brawl: stage a street incident near the player now (P1 LIFE)", cmdIncident);

bool cmdGotoIncident(ScriptCtx& c) {
  Game& g = c.game;
  const life::IncidentView v = life::incidentNow(g);
  if (!v.kind) { c.fail("gotoincident: no incident going on"); return true; }
  const int n = c.arg(1).empty() ? 3 : std::atoi(c.arg(1).c_str());
  g.pl().p = Vec2((float)(v.gx - g.world.ox) * TILE + 8.0f, (float)(v.gy - g.world.oy + n) * TILE + 10.0f);
  return true;
}
EMB_SCRIPT_CMD("gotoincident", "gotoincident [n]: stand n tiles south of the street incident (P1 LIFE)", cmdGotoIncident);

bool cmdIncidentTalk(ScriptCtx& c) {
  Game& g = c.game;
  const life::IncidentView v = life::incidentNow(g);
  if (!v.kind) { c.fail("incidenttalk: no incident going on"); return true; }
  if (!life::debugTalk(g, v.victim)) { c.fail("incidenttalk: could not talk to them"); return true; }
  std::printf("incidenttalk: %s\n", g.dlg.text.c_str());
  return true;
}
EMB_SCRIPT_CMD("incidenttalk", "incidenttalk: talk to the incident's person (the plea and the offer to step in; P1 LIFE)", cmdIncidentTalk);

// (P1 integration fixer) the errands won when a thief was taken on: the run ends in a catch the moment the thief comes
// within reach of the hero, so a cutpurse whose way out passes the hero is caught before `catchthief` walks up (the
// crowd, and so the victim and the route, differ run to run with frame timing): that catch counts, an escape does not
int g_wonAtThiefHelp = -1;

bool cmdIncidentHelp(ScriptCtx& c) {
  Game& g = c.game;
  const life::IncidentView v = life::incidentNow(g);
  if (!v.kind) { c.fail("incidenthelp: no incident going on"); return true; }
  if (!(g.mode == Mode::Dialogue && g.dlg.actor == v.victim) && !life::debugTalk(g, v.victim)) { c.fail("incidenthelp: could not talk to them"); return true; }
  const char* opt = v.kind == life::INC_THIEF ? "I'LL CATCH" : v.kind == life::INC_CART ? "LEND A HAND" : "BREAK IT UP";
  if (!life::debugChoose(g, opt)) { c.fail(std::string("incidenthelp: no ") + opt + " option"); return true; }
  std::printf("incidenthelp: %s (errand %d)\n", g.dlg.text.c_str(), life::incidentNow(g).quest);
  if (v.kind == life::INC_THIEF) g_wonAtThiefHelp = life::streetStats(g).errandsWon;
  if (c.arg(1) != "open") g.closeDialogue();   // (GO.: off after the thief / to the cart; "open" keeps the answer up for a shot)
  return true;
}
EMB_SCRIPT_CMD("incidenthelp", "incidenthelp: talk to the incident's person and take it on (P1 LIFE)", cmdIncidentHelp);

bool cmdCatchThief(ScriptCtx& c) {
  Game& g = c.game;
  const life::IncidentView v = life::incidentNow(g);
  if (v.kind != life::INC_THIEF) {
    if (v.kind == life::INC_NONE && g_wonAtThiefHelp >= 0 && life::streetStats(g).errandsWon > g_wonAtThiefHelp) {
      std::printf("catchthief: already caught (the thief ran within reach of the hero; the errand paid)\n");
      g_wonAtThiefHelp = -1;
      return true;
    }
    c.fail(g_wonAtThiefHelp >= 0 ? "catchthief: the thief got away before the catch" : "catchthief: no thief running");
    return true;
  }
  Actor* t = byId(g, v.other);
  if (!t) { c.fail("catchthief: the thief is gone"); return true; }
  // up behind them, a step at a time (returns false until the catch: script time waits)
  const Vec2 d = t->p - g.pl().p;
  const float l = len(d);
  if (l > 14.0f) { g.pl().p += d * (std::min(l - 10.0f, 3.0f) / l); g.pl().face = std::fabs(d.x) > std::fabs(d.y) ? (d.x > 0 ? 2 : 3) : (d.y > 0 ? 0 : 1); return false; }
  g.pl().p = t->p - d * (10.0f / std::max(l, 1.0f));
  return true;
}
EMB_SCRIPT_CMD("catchthief", "catchthief: walk up behind the running thief (P1 LIFE)", cmdCatchThief);

bool cmdOverhear(ScriptCtx& c) {
  const uint32_t id = life::forceGossip(c.game);
  if (!id) { c.fail("overhear: nobody in town has work to name"); return true; }
  const jobs::Lead* L = c.game.jobBook.lead(id);
  std::printf("overhear: lead %u: %s\n", id, L ? L->text.c_str() : "?");
  return true;
}
EMB_SCRIPT_CMD("overhear", "overhear: the nearest resident lets slip a need of someone in town (a journal LEAD; P1 LIFE)", cmdOverhear);

// (P1 fixer r2) the townsfolk standing closer than px (default 12) to another near the player: one heap of two bodies
bool cmdCrowd(ScriptCtx& c) {
  Game& g = c.game;
  const float px = c.arg(1).empty() ? 12.0f : (float)std::atof(c.arg(1).c_str());
  int n = 0;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (!a.npc || a.st == AState::Dead || len(a.p - g.pl().p) > 16.0f * TILE) continue;
    for (size_t j = i + 1; j < g.actors.size(); j++) {
      const Actor& b = g.actors[j];
      if (!b.npc || b.st == AState::Dead || len(b.p - a.p) >= px) continue;
      n++;
      std::printf("  crowd %.1f px: %d %s [%s] / %d %s [%s]\n", len(b.p - a.p), a.id, a.name.c_str(), life::debugFolk(g, a.id).c_str(), b.id, b.name.c_str(),
                  life::debugFolk(g, b.id).c_str());
    }
  }
  std::printf("crowd: %d pair(s) closer than %.0f px\n", n, px);
  return true;
}
EMB_SCRIPT_CMD("crowd", "crowd [px]: print the townsfolk near the player standing closer than px (12) to another (P1)", cmdCrowd);

// (P1 fixer r2) who someone is, by name (role, resident, site, slot, map spawn, position, their street runtime)
bool cmdWho(ScriptCtx& c) {
  Game& g = c.game;
  int n = 0;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (a.name != c.arg(1)) continue;
    n++;
    std::printf("who %s: id %d role %d resident %d site %d bldg %d slot %d fromMap %d lifeBits %u st %d posture %d at %.1f,%.1f home %.1f,%.1f [%s]\n", a.name.c_str(), a.id,
                (int)a.role, a.resident, a.site, a.bldg, a.slot, (int)a.fromMap, (unsigned)a.lifeBits, (int)a.st, (int)a.posture, a.p.x / TILE, a.p.y / TILE,
                a.home.x / TILE, a.home.y / TILE, life::debugFolk(g, a.id).c_str());
  }
  if (!n) c.fail("who: nobody called " + c.arg(1));
  return true;
}
EMB_SCRIPT_CMD("who", "who <NAME>: print who that is (role, resident, slot, spawn, runtime; P1 debug)", cmdWho);

bool cmdLeads(ScriptCtx& c) {
  for (const jobs::Lead& L : c.game.jobBook.leads)
    std::printf("  lead %u %s state %d day %d qtype %d who %llx pos %d %d,%d: %s\n", L.id, jobs::leadFromName(L.from), (int)L.state, L.day, (int)L.qtype,
                (unsigned long long)L.who, (int)L.hasPos, L.gx, L.gy, L.text.c_str());
  return true;
}
EMB_SCRIPT_CMD("leads", "leads: print the journal's leads (P1)", cmdLeads);

bool expIncident(ScriptCtx& c) {
  const int want = kindOf(c.arg(2));
  if (want < 0) { c.fail("expect incident none|thief|cart|brawl [engaged]"); return true; }
  const life::IncidentView v = life::incidentNow(c.game);
  if (v.kind != want) c.fail(std::string("expect incident: ") + kindName(v.kind) + ", not " + c.arg(2));
  else if (c.arg(3) == "engaged" && !v.engaged) c.fail("expect incident: not taken on yet");
  return true;
}
EMB_SCRIPT_CMD("expect:incident", "expect incident none|thief|cart|brawl [engaged]: the street incident going on (P1 LIFE)", expIncident);

}  // namespace
