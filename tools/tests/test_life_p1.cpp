// rpg_test --life-p1 [--seeds A..B] [--city] [--motion] [--hooks] [--report]: P1 "Fresh Roads", the LIFE lane's own
// checks (rpg/sim/life_game.cpp, VISION_PLAN 15.34). Neither part given: both.
//   --motion  the town-motion protocol of `--town-motion` (arrive at the town's heart at 8:00, stand there to 18:00, look
//             every 5 game minutes at everyone outdoors within 24 tiles), with what the frozen harness does not print:
//             who stands still by kind (a resident's job, plan and street task; a generator spawn's role, stall keeper or
//             not), the street tasks begun, and the path searches. Gates (as the harness's): walking >= 15 %, nobody
//             static for 2 game hours (sentries and posted workers in a work pose exempt).
//   --hooks   the stumble-on hooks in the nearest town at 10:00: overheard gossip makes a journal lead naming a real
//             giver (LeadFrom::Gossip, the giver's npcKey, their door) and never the same giver twice in a week; a thief
//             caught (talk to the victim, I'LL CATCH THEM!, reach the thief) pays the errand; a thief ignored leaves an
//             Incident lead and no errand; a broken cart helped (LEND A HAND, stand by it) pays; a brawl stepped into pays;
//             the giver's pin (Game::lifeGiverSpot) knows where a census giver is with nobody in play; an open quest's
//             giver whose plan is indoors is not kept out in the street at night (no plaza pin). (P1b) while a new request
//             or tale with `gossip` lines is on offer in the town (or the start village), an overheard line is one of its
//             lines and its lead names that teller; never the same teller twice a week.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/jobs.h"
#include "rpg/sim/life.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

using art::Posture;

void tick(Game& g, int frames) {
  for (int f = 0; f < frames; f++) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode != Mode::Play && g.mode != Mode::Dialogue) g.mode = Mode::Play;
  }
}
void ticks(Game& g, float secs) { tick(g, (int)(secs * 60.0f)); }

bool workPose(Posture p) {
  switch (p) {
    case Posture::Hammer: case Posture::Hoe: case Posture::Chop: case Posture::Stir: case Posture::Fish: case Posture::Sweep:
    case Posture::Carry: case Posture::Lamp: case Posture::Lute: case Posture::Drum: case Posture::Flute: case Posture::Read: case Posture::Beg:
      return true;
    default: return false;
  }
}
const life::Resident* resOf(Game& g, const Actor& a) {
  if (a.resident < 0 || a.site < 0 || a.site >= (int)g.world.sites.size()) return nullptr;
  life::Census* c = g.life.census(g.world, a.site);
  return c && a.resident < (int)c->res.size() ? &c->res[(size_t)a.resident] : nullptr;
}
// the frozen harness's exemption (test_town_motion.cpp posted())
bool posted(Game& g, const Actor& a) {
  if (a.role == Role::Guard || a.role == Role::Soldier || a.role == Role::Captain || a.role == Role::Herald) return true;
  if (!workPose(a.posture)) return false;
  if (a.stallKeeper) return true;
  const life::Resident* r = resOf(g, a);
  if (!r) return false;
  if (r->work >= 0 && r->act == life::Act::Work) return true;
  switch (r->job) {
    case life::Job::Farmer: case life::Job::Fisher: case life::Job::Miner: case life::Job::Woodcutter: case life::Job::Herder: case life::Job::Labourer:
    case life::Job::Bard: case life::Job::Beggar: case life::Job::Lamplighter:
      return true;
    default: return false;
  }
}
bool g_verbose = false;
std::string kindOf(Game& g, const Actor& a) {
  const life::Resident* r = resOf(g, a);
  if (r) return std::string(life::jobName(r->job)) + " " + life::actName(r->act) + (r->work < 0 ? "@NOWORK" : "") + " " + life::taskName(life::taskOf(g, a.id));
  char b[256];
  snprintf(b, sizeof b, "SPAWN role %d%s%s", (int)a.role, a.stallKeeper ? " stallkeeper" : "", (a.lifeBits & 512u) ? " thief" : "");
  if (g_verbose) snprintf(b + strlen(b), sizeof b - strlen(b), " slot %d bldg %d quest %d party %d lb %u home %.1f,%.1f goal %.1f,%.1f", a.slot, a.bldg, a.quest, (int)a.partyTag, a.lifeBits, a.home.x / TILE, a.home.y / TILE, a.goal.x / TILE, a.goal.y / TILE);
  return b;
}

struct Track { Vec2 anchor; float since = 0; bool allPosted = true, reported = false; std::string why; };

int motion(uint64_t A, uint64_t B, bool city, bool report) {
  int bad = 0, statics = 0, n = 0;
  double share = 0, arrival = 0, worst = 0, avgMs = 0;
  std::map<std::string, int> kinds;
  int tasks[8] = {}, legFails[8] = {}, paths = 0, fails = 0;
  for (uint64_t seed = A; seed <= B; seed++) {
    g_curSeed = seed;
    for (int pass = 0; pass < (city ? 2 : 1); pass++) {
      Game g(seed);
      g.newEndlessGame(seed);
      g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
      tick(g, 10);
      const Site& h = g.world.sites[(size_t)g.world.startSite];
      const int si = g.world.findSiteNear(g.world.ox + h.ex, g.world.oy + h.ey, pass == 0 ? SiteType::Town : SiteType::City, 8);
      if (si < 0) continue;
      const ew::Gid sid = g.world.sites[(size_t)si].id;
      { const Site s = g.world.sites[(size_t)si]; g.teleportGlobal(g.world.ox + s.ex, g.world.oy + s.ey + 1); }
      g.hour = 8.0f;
      tick(g, 2);
      const float dayH = (float)g.day * 24.0f;
      std::map<int, Track> tr;
      double sh = 0, ar = 0;
      int samples = 0, arrN = 0, st = 0;
      const int s0 = g.lifeFolkStats().steps;
      const double m0 = g.lifeFolkStats().sumMs;
      const int p0 = g.lifeFolkStats().pathRequests, f0 = g.lifeFolkStats().pathFails;
      float next = 8.0f;
      double w = 0;
      while (true) {
        const float now = (float)g.day * 24.0f + g.hour - dayH;
        if (now >= 18.0f) break;
        if (now + 1e-4f >= next) {
          next = now + 5.0f / 60.0f;
          const int here = g.world.siteHandle(sid);
          if (here < 0 || g.inside) break;
          const Site& S = g.world.sites[(size_t)here];
          const Vec2 heart((S.ex + 0.5f) * TILE, (S.ey + 0.5f) * TILE);
          int present = 0, walking = 0;
          std::map<int, bool> seen;
          for (size_t k = 1; k < g.actors.size(); k++) {
            const Actor& a = g.actors[k];
            if (!a.npc || !a.human || a.critter || a.comp || a.st == AState::Dead || a.hostile) continue;
            if (len(a.p - heart) > 24.0f * TILE) continue;
            present++;
            if (a.st == AState::Walk) walking++;
            seen[a.id] = true;
            const bool ok = posted(g, a);
            auto it = tr.find(a.id);
            if (it == tr.end() || len(a.p - it->second.anchor) > 1.5f * TILE) { Track t; t.anchor = a.p; t.since = now; t.allPosted = ok; tr[a.id] = t; continue; }
            Track& t = it->second;
            if (t.allPosted && !ok && report) {
              char wb[256];
              snprintf(wb, sizeof wb, "first unposted at %.2f: posture %d st %d %s", now, (int)a.posture, (int)a.st, life::debugFolk(g, a.id).c_str());
              t.why = wb;
            }
            t.allPosted = t.allPosted && ok;
            if (!t.reported && now - t.since >= 2.0f && !t.allPosted) {
              t.reported = true;
              st++;
              g_verbose = report;
              const std::string kd = kindOf(g, a);
              g_verbose = false;
              kinds[kd]++;
              if (report) {
                out("  life-p1 motion seed %llu %s: %s static %.2f-%.2f (%s, posture %d) at %d,%d\n", (unsigned long long)seed, S.name.c_str(), a.name.c_str(),
                    t.since, now, kd.c_str(), (int)a.posture, (int)std::floor(a.p.x / TILE), (int)std::floor(a.p.y / TILE));
                out("      %s st %d\n", life::debugFolk(g, a.id).c_str(), (int)a.st);
                if (!t.why.empty()) out("      %s\n", t.why.c_str());
                // the ground round them: . open, # blocked, B a building, P a prop, @ them
                const int ax = (int)std::floor(a.p.x / TILE), ay = (int)std::floor((a.p.y - 2) / TILE);
                for (int y = ay - 4; y <= ay + 4; y++) {
                  std::string row = "      ";
                  for (int x = ax - 6; x <= ax + 6; x++) {
                    const Map& m = g.world.over;
                    char ch = '?';
                    if (m.in(x, y)) ch = (x == ax && y == ay) ? '@' : m.bldgAt[(size_t)y * m.w + x] >= 0 ? 'B' : m.propAt(x, y) && m.blocked(x, y) ? 'P' : m.blocked(x, y) ? '#' : '.';
                    row += ch;
                  }
                  out("%s\n", row.c_str());
                }
              }
            }
          }
          for (auto it = tr.begin(); it != tr.end();) it = seen.count(it->first) ? std::next(it) : tr.erase(it);
          if (present) {
            sh += (double)walking / present;
            samples++;
            if (now - 8.0f < 0.25f + 1e-3f) { ar += (double)walking / present; arrN++; }
          }
        }
        tick(g, 30);
        w = std::max(w, g.lifeFolkStats().lastMs);
      }
      const int steps = g.lifeFolkStats().steps - s0;
      const double avg = steps ? (g.lifeFolkStats().sumMs - m0) / steps : 0;
      const double wsh = samples ? 100.0 * sh / samples : 0, arsh = arrN ? 100.0 * ar / arrN : 0;
      const life::StreetStats& ss = life::streetStats(g);
      for (int k = 0; k < 8; k++) { tasks[k] += ss.tasks[k]; legFails[k] += ss.legFails[k]; }
      paths += g.lifeFolkStats().pathRequests - p0;
      fails += g.lifeFolkStats().pathFails - f0;
      out("  life-p1 motion seed %llu %s %s: walking %.1f %% (arrival %.1f %%), static %d, life step avg %.3f ms worst %.2f ms, paths %d, incidents %d (leads %d), overheard leads %d of %d lines\n",
          (unsigned long long)seed, pass ? "city" : "town", g.world.sites[(size_t)g.world.siteHandle(sid)].name.c_str(), wsh, arsh, st, avg, w,
          g.lifeFolkStats().pathRequests - p0, ss.incidents, ss.incidentLeads, ss.gossipLeads, ss.gossipLines);
      share += wsh; arrival += arsh; statics += st; worst = std::max(worst, w); avgMs += avg;
      n++;
    }
  }
  printf("life-p1 motion: %d places: walking %.1f %% (arrival %.1f %%), static %d, life step avg %.3f ms worst %.2f ms, paths %d (failed %d)\n", n,
         n ? share / n : 0.0, n ? arrival / n : 0.0, statics, n ? avgMs / n : 0.0, worst, paths, fails);
  printf("life-p1 motion: tasks begun:");
  for (int k = 0; k < 7; k++) printf(" %s %d", life::taskName(k), tasks[k]);
  printf("\nlife-p1 motion: routes not found, by task:");
  for (int k = 0; k < 7; k++) printf(" %s %d", life::taskName(k), legFails[k]);
  printf("\n");
  for (auto& kv : kinds) printf("  static: %4d %s\n", kv.second, kv.first.c_str());
  if (n && share / n < 15.0) { printf("FAIL: life-p1 motion walking %.1f %% (want >= 15)\n", share / n); bad++; }
  if (statics) { printf("FAIL: life-p1 motion %d static for 2 game hours (want 0)\n", statics); bad++; }
  return bad;
}

// ---------------------------------------------------------------- the stumble-on hooks
struct Ctx {
  Game& g;
  int bad = 0;
  uint64_t seed;
  void fail(const std::string& s) { out("FAIL: life-p1 hooks seed %llu: %s\n", (unsigned long long)seed, s.c_str()); bad++; }
};
Actor* byId(Game& g, int id) {
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].id == id) return &g.actors[k];
  return nullptr;
}
const Quest* questById(Game& g, int id) { for (const Quest& q : g.quests) if (q.id == id) return &q; return nullptr; }
int leadsFrom(Game& g, jobs::LeadFrom f) { int n = 0; for (const jobs::Lead& l : g.jobBook.leads) n += l.from == f; return n; }
void arrive(Game& g, int si, float hour) {
  if (g.inside) g.debugLeave();
  const Site s = g.world.sites[(size_t)si];
  g.teleportGlobal(g.world.ox + s.ex, g.world.oy + s.ey + 1);
  g.hour = hour;
  tick(g, 2);
  ticks(g, 4.0f);
}
void closeTalk(Game& g) { if (g.mode == Mode::Dialogue) g.closeDialogue(); g.mode = Mode::Play; }

int hooks(uint64_t seed, bool report) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
  tick(g, 10);
  Ctx C{g, 0, seed};
  const Site& h = g.world.sites[(size_t)g.world.startSite];
  int si = g.world.findSiteNear(g.world.ox + h.ex, g.world.oy + h.ey, SiteType::Town, 8);
  if (si < 0) { out("WARN: life-p1 hooks seed %llu: no town near the start\n", (unsigned long long)seed); return 0; }
  const ew::Gid sid = g.world.sites[(size_t)si].id;
  arrive(g, si, 10.0f);
  si = g.world.siteHandle(sid);
  const std::string town = g.world.sites[(size_t)si].name;
  // ---- overheard gossip: a lead naming a real giver, never the same giver twice a week
  {
    const uint32_t id = life::forceGossip(g);
    if (!id) C.fail("no overheard lead in " + town + " (nobody in the street has work to name?)");
    else {
      const jobs::Lead* L = g.jobBook.lead(id);
      if (!L || L->from != jobs::LeadFrom::Gossip || !L->who || !L->hasPos || L->site != sid || L->text.empty())
        C.fail("the overheard lead is not a Gossip lead naming a giver with a place");
      else {
        bool real = false;
        for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.npcKey(g.actors[k]) == L->who) real = true;
        if (!real) C.fail("the overheard lead's giver is nobody in play");
        if (report) out("  life-p1 hooks seed %llu: overheard lead '%s' (qtype %d)\n", (unsigned long long)seed, L->text.c_str(), (int)L->qtype);
        const uint64_t who = L->who;
        const uint32_t id2 = life::forceGossip(g);
        const jobs::Lead* L2 = id2 ? g.jobBook.lead(id2) : nullptr;
        if (L2 && L2->who == who) C.fail("the same giver overheard twice in one week");
      }
    }
  }
  // ---- a thief caught
  {
    const int gold0 = g.gold;
    if (!life::forceIncident(g, life::INC_THIEF)) C.fail("no thief incident could be staged in " + town);
    else {
      const life::IncidentView v = life::incidentNow(g);
      if (!life::debugTalk(g, v.victim)) C.fail("could not talk to the thief's victim");
      else if (!life::debugChoose(g, "I'LL CATCH")) C.fail("the victim offers no I'LL CATCH THEM!");
      closeTalk(g);
      const life::IncidentView v2 = life::incidentNow(g);
      if (!v2.quest || !v2.engaged) C.fail("taking it on opened no errand");
      const Quest* q = v2.quest ? questById(g, v2.quest) : nullptr;
      if (q && (q->type != QType::Errand || q->stage != (int)jobs::ErrandKind::Thief || !q->hasPos || q->subject.empty())) C.fail("the thief errand is not an Errand / Thief with a place and a line");
      Actor* t = byId(g, v2.other);
      if (!t) C.fail("no thief in play");
      else {
        ticks(g, 1.0f);   // (they run)
        if ((t = byId(g, v2.other))) { g.pl().p = t->p + Vec2(4.0f, 0.0f); tick(g, 3); }
        q = v2.quest ? questById(g, v2.quest) : nullptr;
        if (!q || q->state != QState::Done || (q->flags & QF_FAILED)) C.fail("catching the thief did not end the errand in success");
        if (g.gold <= gold0) C.fail("catching the thief paid nothing");
        if (life::incidentNow(g).kind) C.fail("the thief incident is still going after the catch");
      }
    }
    ticks(g, 6.0f);
  }
  // ---- a thief ignored: a lead, no errand
  {
    arrive(g, g.world.siteHandle(sid), 11.0f);
    const int leads0 = leadsFrom(g, jobs::LeadFrom::Incident);
    const size_t quests0 = g.quests.size();
    if (life::forceIncident(g, life::INC_THIEF)) {
      uint64_t vkey = 0;
      if (Actor* v = byId(g, life::incidentNow(g).victim)) vkey = g.npcKey(*v);
      // (the player stays put at the heart: the thief runs for the edge on his own side, away from them)
      for (int s = 0; s < 100 && life::incidentNow(g).kind; s++) ticks(g, 1.0f);
      if (life::incidentNow(g).kind) C.fail("an ignored thief never got away (100 s)");
      // (a new Incident lead, or the open lead naming the robbed already: addLead never doubles one person's lead)
      bool named = false;
      for (const jobs::Lead& l : g.jobBook.leads) named |= vkey && l.who == vkey && l.state == jobs::LS_OPEN;
      if (leadsFrom(g, jobs::LeadFrom::Incident) <= leads0 && !named) {
        C.fail("an ignored thief left no Incident lead");
        if (report)
          for (const jobs::Lead& l : g.jobBook.leads) out("    lead %u from %d state %d who %llx qtype %d: %s\n", l.id, (int)l.from, (int)l.state, (unsigned long long)l.who, (int)l.qtype, l.text.c_str());
        if (report) out("    victim key %llx, street stats: incident leads %d\n", (unsigned long long)vkey, life::streetStats(g).incidentLeads);
      }
      if (g.quests.size() != quests0) C.fail("an ignored thief opened a quest");
    } else C.fail("no second thief incident could be staged");
  }
  // ---- a broken cart helped
  {
    bool staged = false;
    for (int tries = 0; tries < 3 && !staged; tries++) {
      arrive(g, g.world.siteHandle(sid), 12.0f + tries);
      staged = life::forceIncident(g, life::INC_CART);
    }
    if (!staged) out("WARN: life-p1 hooks seed %llu: no cart near %s's heart to break down\n", (unsigned long long)seed, town.c_str());
    else {
      const int gold0 = g.gold;
      ticks(g, 6.0f);   // (the owner walks to the cart)
      life::IncidentView v = life::incidentNow(g);
      if (!life::debugTalk(g, v.victim) || !life::debugChoose(g, "LEND A HAND")) C.fail("the cart's owner offers no LEND A HAND");
      closeTalk(g);
      v = life::incidentNow(g);
      if (!v.quest) C.fail("lending a hand opened no errand");
      else {
        g.pl().p = Vec2((v.gx - g.world.ox) * TILE + 8.0f, (v.gy - g.world.oy + 1) * TILE + 10.0f);
        ticks(g, 7.5f);
        const Quest* q = questById(g, v.quest);
        if (!q || q->state != QState::Done || (q->flags & QF_FAILED)) C.fail("holding the cart did not end the errand in success");
        if (g.gold <= gold0) C.fail("helping with the cart paid nothing");
      }
    }
  }
  // ---- a brawl stepped into
  {
    arrive(g, g.world.siteHandle(sid), 15.0f);
    if (!life::forceIncident(g, life::INC_BRAWL)) C.fail("no brawl could be staged in " + town);
    else {
      const int gold0 = g.gold;
      ticks(g, 2.0f);
      const life::IncidentView v = life::incidentNow(g);
      Actor* a = byId(g, v.victim);
      Actor* b = byId(g, v.other);
      if (!a || !b) C.fail("the brawlers are not in play");
      else {
        g.pl().p = (a->p + b->p) * 0.5f;
        tick(g, 3);
        if (life::incidentNow(g).kind) C.fail("stepping between the brawlers did not end the brawl");
        if (g.gold <= gold0) C.fail("breaking up the brawl paid nothing");
      }
    }
  }
  // ---- the giver's pin and no plaza pin
  {
    arrive(g, g.world.siteHandle(sid), 10.0f);
    si = g.world.siteHandle(sid);
    life::Census* c = g.life.census(g.world, si);
    int giverId = -1;
    Quest q;
    for (size_t k = 1; k < g.actors.size() && giverId < 0; k++) {
      const Actor a = g.actors[k];
      if (a.resident < 0 || a.site != si || !g.offersWork(a)) continue;
      bool ok = false;
      q = g.offerFor(a, QType::COUNT, ok);
      if (ok) giverId = a.id;
    }
    if (giverId < 0 || !c) out("WARN: life-p1 hooks seed %llu: nobody in %s's street offers work\n", (unsigned long long)seed, town.c_str());
    else {
      g.debugAccept(q);
      const Quest& acc = g.quests.back();
      int tx = 0, ty = 0;
      if (!g.lifeGiverSpot(acc, tx, ty)) C.fail("lifeGiverSpot does not know where a census giver is");
      // night: their plan indoors, they are not out in the street (no plaza pin)
      const int ri = acc.giverSlot - 3000;
      g.hour = 23.5f;
      tick(g, 3);
      ticks(g, 2.0f);
      si = g.world.siteHandle(sid);
      c = g.life.census(g.world, si);
      if (c && ri >= 0 && ri < (int)c->res.size()) {
        const life::Plan p = g.life.plan(g.world, *c, c->res[(size_t)ri], g.day, g.hour);
        bool out = false;
        for (size_t k = 1; k < g.actors.size(); k++) if (isGiver(acc, g.actors[k]) && g.actors[k].st != AState::Dead) out = true;
        if (p.bldg >= 0 && out && !g.inside) C.fail("an open quest's giver whose plan is indoors is still kept out in the street at 23:30");
        if (!g.lifeGiverSpot(acc, tx, ty)) C.fail("lifeGiverSpot does not know where the giver is at night");
        else if (p.bldg >= 0) {
          const int b = c->bldgHandle(g.world.sites[(size_t)si], p.bldg);
          if (b >= 0 && (tx != g.world.over.bldgs[(size_t)b].doorX() || ty != g.world.over.bldgs[(size_t)b].doorY())) C.fail("lifeGiverSpot at night is not the door of the building their plan has them in");
        }
      }
    }
  }
  // ---- (P1b, REQUESTS lane) overheard gossip of the new items: while a request or a tale with `gossip` lines is on offer
  // in the settlement (story::tellerOffer), a forced overheard line is one of its lines and the lead names that teller
  // (LeadFrom::Gossip, qtype Story, their npcKey, a place); a second one that week never names the same teller. The town,
  // then the start village, week after week (16 at most) until something with gossip is on offer
  {
    const story::dsl::Library& L = story::dsl::library();
    const ew::Gid places[2] = {sid, g.world.sites[(size_t)g.world.startSite].id};
    bool seen = false;
    int offered = 0;
    for (int pass = 0; pass < 2 && !seen; pass++)
      for (int w = 0; w < 16 && !seen; w++) {
        const int at = g.world.siteHandle(places[pass]);
        if (at < 0) break;
        arrive(g, at, 10.0f);
        const int here = g.world.siteHandle(places[pass]);
        std::map<uint64_t, int> tellers;   // npcKey -> the library script they offer
        std::vector<Actor> folk;
        for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.actors[k].human && g.actors[k].site == here) folk.push_back(g.actors[k]);
        for (const Actor& a : folk) {
          const story::Offer of = story::tellerOffer(g, a);
          if (of.lib >= 0 && (size_t)of.lib < L.scripts.size() && !L.scripts[(size_t)of.lib].family.empty() && !L.scripts[(size_t)of.lib].gossip.empty())
            tellers[g.npcKey(a)] = of.lib;
        }
        if (!tellers.empty()) {
          offered++;
          const uint32_t id = life::forceGossip(g);
          const jobs::Lead* Ld = id ? g.jobBook.lead(id) : nullptr;
          if (Ld && Ld->qtype == (uint8_t)QType::Story) {
            auto t = tellers.find(Ld->who);
            if (Ld->from != jobs::LeadFrom::Gossip || !Ld->hasPos || t == tellers.end()) C.fail("a P1b overheard lead does not name a teller of an item with gossip");
            else {
              const story::dsl::Script& s = L.scripts[(size_t)t->second];
              bool line = false;
              for (const std::string& gl : s.gossip) line |= Ld->text.find("\"" + gl + "\"") != std::string::npos;
              if (!line) C.fail("the P1b overheard lead does not carry one of " + s.id + "'s gossip lines: " + Ld->text);
              if (report) out("  life-p1 hooks seed %llu: P1b overheard (%s, week %d): %s\n", (unsigned long long)seed, s.id.c_str(), w, Ld->text.c_str());
              const uint64_t who = Ld->who;
              const uint32_t id2 = life::forceGossip(g);
              const jobs::Lead* L2 = id2 ? g.jobBook.lead(id2) : nullptr;
              if (L2 && L2->qtype == (uint8_t)QType::Story && L2->who == who) C.fail("the same P1b teller overheard twice in one week");
              seen = true;
            }
          }
        }
        g.day += 7;
        tick(g, 3);
      }
    if (!seen) C.fail("no P1b item with gossip was overheard in 16 weeks of " + town + " and the start village (" + std::to_string(offered) + " weeks with one on offer)");
  }
  if (report) {
    const life::StreetStats& s = life::streetStats(g);
    out("  life-p1 hooks seed %llu: incidents %d, errands opened %d won %d lost %d, incident leads %d, gossip lines %d leads %d\n", (unsigned long long)seed,
        s.incidents, s.errandsOpened, s.errandsWon, s.errandsLost, s.incidentLeads, s.gossipLines, s.gossipLeads);
  }
  return C.bad;
}

// --req-probe (P1b REQUESTS lane, diagnostics): per seed, the nearest harbour (a port town or city) and what its notice
// board raises week by week, and whether the start's realm sees a war in the first twelve weeks (the herald's request)
int reqProbe(uint64_t A, uint64_t B) {
  for (uint64_t seed = A; seed <= B; seed++) {
    g_curSeed = seed;
    Game g(seed);
    g.newEndlessGame(seed);
    g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
    tick(g, 10);
    const Site st = g.world.sites[(size_t)g.world.startSite];
    const int32_t sx = g.world.ox + st.ex, sy = g.world.oy + st.ey;
    ew::Gid best = 0;
    int64_t bd = INT64_MAX;
    for (int32_t ry = ew::regionOf(sy) - 8; ry <= ew::regionOf(sy) + 8; ry++)
      for (int32_t rx = ew::regionOf(sx) - 8; rx <= ew::regionOf(sx) + 8; rx++) {
        const ew::RegionPlan R = g.world.regionPlan(rx, ry);
        for (const ew::SitePlan& p : R.sites) {
          if ((p.type != SiteType::Town && p.type != SiteType::City) || p.archetype != ew::Archetype::Port || (p.flags & ew::SPF_HIDDEN)) continue;
          const int64_t d = (int64_t)(p.ex - sx) * (p.ex - sx) + (int64_t)(p.ey - sy) * (p.ey - sy);
          if (d < bd) { bd = d; best = p.id; }
        }
      }
    std::string near;
    for (int32_t ry = ew::regionOf(sy) - 4; ry <= ew::regionOf(sy) + 4; ry++)
      for (int32_t rx = ew::regionOf(sx) - 4; rx <= ew::regionOf(sx) + 4; rx++) {
        const ew::RegionPlan R = g.world.regionPlan(rx, ry);
        for (const ew::SitePlan& p : R.sites) {
          if ((p.type != SiteType::Town && p.type != SiteType::City) || (p.flags & ew::SPF_HIDDEN)) continue;
          const double d = std::sqrt((double)((int64_t)(p.ex - sx) * (p.ex - sx) + (int64_t)(p.ey - sy) * (p.ey - sy)));
          near += " " + std::string(p.type == SiteType::City ? "C" : "T") + std::to_string((int)p.archetype) + (story::nearCoast(g, p.ex, p.ey) ? "c" : "") + "@" + std::to_string((int)d);
        }
      }
    out("req-probe seed %llu: towns/cities within 4 regions (type, archetype, c=coast @tiles):%s\n", (unsigned long long)seed, near.c_str());
    int wars = 0;
    std::string board;
    if (best) {
      const int h = g.world.ensureSite(best);
      if (h >= 0) g.world.sites[(size_t)h].discovered = true;
      if (h >= 0 && g.fastTravel(h)) {
        tick(g, 8);
        {
          const int here = g.world.siteHandle(best);
          for (const char* rid : {"rq_barrels", "rq_yellowflag", "rq_articles"}) {
            const int li = story::dsl::library().find(rid);
            if (li < 0 || here < 0) continue;
            const story::dsl::Script& s = story::dsl::library().scripts[(size_t)li];
            std::string why, cwhy;
            const bool gates = story::offerGates(g, s, here, &why);
            std::vector<story::Binding> cast;
            const bool cast_ok = story::castScript(g, s, -1, here, story::offerCastKey(g, s, nullptr, here), cast, cwhy);
            out("req-probe seed %llu: %s at the harbour: gates %s (%s), cast %s (%s), precheck %d\n", (unsigned long long)seed, rid, gates ? "ok" : "no", why.c_str(),
                cast_ok ? "ok" : "no", cwhy.c_str(), (int)story::offerPrecheck(g, s, here));
          }
        }
        for (int w = 0; w < 12; w++) {
          const int here = g.world.siteHandle(best);
          if (here < 0) break;
          const int r = story::requestOfferForPlace(g, (int)story::dsl::HookKind::Board, here);
          const story::Offer o = story::placeOffer(g, (int)story::dsl::HookKind::Board, here);
          board += " w" + std::to_string(w) + ":" + (r >= 0 ? story::dsl::library().scripts[(size_t)r].id : std::string("-")) + "/" + (o.any() ? o.id() : std::string("-"));
          for (const realm::War& wr : g.realm.wars()) if (!wr.endDay) { wars++; break; }
          g.day += 7;
          tick(g, 3);
        }
      }
    }
    out("req-probe seed %llu: nearest harbour %s at %.0f tiles (%s 4 regions); weeks at war %d;%s\n", (unsigned long long)seed,
        best ? "found" : "none", best ? std::sqrt((double)bd) : 0.0, best && std::sqrt((double)bd) <= 4.0 * (1 << ew::REGION_SHIFT) ? "within" : "beyond", wars, board.c_str());
  }
  return 0;
}

// --keeper: one stall keeper's track over an hour (diagnostics)
int keeperTrack(uint64_t seed) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
  tick(g, 10);
  const Site& h = g.world.sites[(size_t)g.world.startSite];
  const int si = g.world.findSiteNear(g.world.ox + h.ex, g.world.oy + h.ey, SiteType::Town, 8);
  if (si < 0) return 0;
  arrive(g, si, 9.0f);
  int id = -1;
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].stallKeeper) { id = g.actors[k].id; break; }
  if (id < 0) { printf("no keeper\n"); return 0; }
  for (int s = 0; s < 120; s++) {
    Actor* a = byId(g, id);
    if (!a) { printf("keeper gone\n"); break; }
    if (s % 4 == 0 || a->st == AState::Walk)
      printf("  keeper %s t %.2f p %.2f,%.2f home %.2f,%.2f st %d posture %d asleep %d speed %.1f\n", a->name.c_str(), g.hour, a->p.x / TILE, a->p.y / TILE,
             a->home.x / TILE, a->home.y / TILE, (int)a->st, (int)a->posture, (int)a->asleep, a->speed);
    tick(g, a->st == AState::Walk ? 5 : 30);
  }
  return 0;
}

// --track NAME SEED [--city] [--from H]: the motion protocol's day in one place, one person's state printed from H on
// for 20 game minutes (diagnostics)
int trackActor(const char* name, uint64_t seed, bool city, float from) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play; g.godMode = true; g.noWildSpawns = true;
  tick(g, 10);
  const Site& h = g.world.sites[(size_t)g.world.startSite];
  const int si = g.world.findSiteNear(g.world.ox + h.ex, g.world.oy + h.ey, city ? SiteType::City : SiteType::Town, 8);
  if (si < 0) return 0;
  { const Site s = g.world.sites[(size_t)si]; g.teleportGlobal(g.world.ox + s.ex, g.world.oy + s.ey + 1); }
  g.hour = 8.0f;
  tick(g, 2);
  const int day0 = g.day;
  while ((g.day - day0) * 24.0f + g.hour < from + 0.34f) {
    const float now = (g.day - day0) * 24.0f + g.hour;
    if (now >= from)
      for (const Actor& a : g.actors)
        if (a.name == name)
          printf("  %.3f %s p %.2f,%.2f goal %.2f,%.2f home %.2f,%.2f st %d posture %d asleep %d think %.2f %s\n", now, name, a.p.x / TILE, a.p.y / TILE,
                 a.goal.x / TILE, a.goal.y / TILE, a.home.x / TILE, a.home.y / TILE, (int)a.st, (int)a.posture, (int)a.asleep, a.thinkT,
                 life::debugFolk(g, a.id).c_str());
    if (now >= from)
      for (const Actor& a : g.actors)
        if (a.name == name)
          for (const Actor& o : g.actors)
            if (o.id != a.id && len(o.p - a.p) < 1.5f * TILE)
              printf("      near %s p %.2f,%.2f st %d resident %d stall %d posture %d %s\n", o.name.c_str(), o.p.x / TILE, o.p.y / TILE, (int)o.st, o.resident,
                     (int)o.stallKeeper, (int)o.posture, life::debugFolk(g, o.id).c_str());
    tick(g, now >= from ? 10 : 30);
  }
  return 0;
}

// --flee NAME SEED: wolves in the nearest kingdom's town at noon (the town-defence check's set-up, roughly), one person's
// flight printed (diagnostics)
int fleeTrack(const char* name, uint64_t seed) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  const Site home = g.world.sites[(size_t)g.world.startSite];
  int member = -1;
  float bd = 1e30f;
  for (int i = 0; i < (int)g.world.sites.size(); i++) {
    const Site& s = g.world.sites[(size_t)i];
    if ((s.type != SiteType::Town && s.type != SiteType::City) || s.kingdom < 0) continue;
    const float d = std::hypot((float)(s.ex - home.ex), (float)(s.ey - home.ey));
    if (d < bd) { bd = d; member = i; }
  }
  if (member < 0) return 0;
  g.godMode = true; g.noWildSpawns = true; g.hour = 12;
  { const Site s = g.world.sites[(size_t)member]; g.teleportGlobal(g.world.ox + s.ex, g.world.oy + s.ey + 2); }
  tick(g, 3);
  const Site st = g.world.sites[(size_t)member];
  const Vec2 mid((st.r.x + st.r.w * 0.5f) * TILE, (st.r.y + st.r.h * 0.5f) * TILE);
  int near = -1;
  float nd = 1e30f;
  for (size_t k = 1; k < g.actors.size(); k++)
    if (g.actors[k].npc && g.actors[k].site == member && g.actors[k].role != Role::Guard && len2(g.actors[k].p - mid) < nd) { nd = len2(g.actors[k].p - mid); near = (int)k; }
  if (near < 0) return 0;
  const Vec2 at = g.actors[(size_t)near].p;
  for (int k = 0; k < 3; k++) g.debugSpawnAt(art::Monster::Wolf, at + Vec2((k - 1) * 10.0f, -4.0f), std::max(1, g.world.zoneLevel(st.ex, st.ey)));
  printf("flee: %s wolves at %.0f,%.0f\n", st.name.c_str(), at.x / TILE, at.y / TILE);
  for (int f = 0; f < 30 * 60; f++) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
    if (f % 30 == 0)
      for (const Actor& a : g.actors)
        if (a.name == name) {
          const life::Resident* r = resOf(g, a);
          printf("  %.1f s %s p %.1f,%.1f home %.1f,%.1f st %d flee %.1f homeBldg %d militia %d hp %.0f %s %s\n", f / 60.0f, name, a.p.x / TILE, a.p.y / TILE, a.home.x / TILE,
                 a.home.y / TILE, (int)a.st, a.fleeT, a.homeBldg, (int)a.militia, a.hp, r ? life::jobName(r->job) : "SPAWN", life::debugFolk(g, a.id).c_str());
        }
  }
  return 0;
}

int lifeP1(int argc, char** argv) {
  for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--keeper")) return keeperTrack(1);
  for (int i = 1; i + 2 < argc; i++) if (!strcmp(argv[i], "--flee")) return fleeTrack(argv[i + 1], (uint64_t)atoll(argv[i + 2]));
  for (int i = 1; i + 2 < argc; i++)
    if (!strcmp(argv[i], "--track")) {
      bool city = false;
      float from = 8.0f;
      for (int k = 1; k < argc; k++) {
        if (!strcmp(argv[k], "--city")) city = true;
        if (!strcmp(argv[k], "--from") && k + 1 < argc) from = (float)atof(argv[k + 1]);
      }
      return trackActor(argv[i + 1], (uint64_t)atoll(argv[i + 2]), city, from);
    }
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "--req-probe")) {
      uint64_t a = 1, b = 3;
      for (int k = 1; k < argc; k++) if (!strcmp(argv[k], "--seeds") && k + 1 < argc) parseSeedRange(argv[k + 1], a, b);
      return reqProbe(a, b);
    }
  uint64_t A = 1, B = 3;
  bool city = false, report = false, mot = false, hk = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) {
      if (!parseSeedRange(argv[++i], A, B)) { printf("bad --seeds range '%s'\n", argv[i]); return 2; }
    } else if (!strcmp(argv[i], "--city")) city = true;
    else if (!strcmp(argv[i], "--report")) report = true;
    else if (!strcmp(argv[i], "--motion")) mot = true;
    else if (!strcmp(argv[i], "--hooks")) hk = true;
  }
  if (!mot && !hk) mot = hk = true;
  const auto t0 = std::chrono::steady_clock::now();
  int bad = 0;
  if (hk) {
    int hb = 0;
    for (uint64_t s = A; s <= B; s++) { g_curSeed = s; hb += hooks(s, report); }
    printf("life-p1 hooks: seeds %llu..%llu, %d failures\n", (unsigned long long)A, (unsigned long long)B, hb);
    bad += hb;
  }
  if (mot) bad += motion(A, B, city, report);
  printf("life-p1: %d failures (%.0f s)\n", bad, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--life-p1", "P1 LIFE lane: town motion by kind, gossip leads, street incidents, the giver's pin [--seeds A..B --city --motion --hooks --report]",
             lifeP1);
