// M5 "Hearth and Hall" script commands for the townsfolk (TOWNSFOLK lane; rpg/sim/life_game.cpp). `embervale
// --script-help` lists them.
//   folk [all]                      print what the townsfolk's actors are doing (counts by posture, walkers, path stats)
//                                   and, nearest first, the residents in play: name, job, plan, posture, tile
//                                   (all: also the residents of the nearest settlement who are not out, with their plan)
//   folkcap <n>                     tests: resident actors a settlement may put on the street at once (0: the default)
//   entergathering [2]              enter the nearest settlement's gathering place (its census's: an inn's common room, a
//                                   mead hall, a tea house, a bathhouse); 2: its second one
//   gotojob <job> [n]               stand two tiles south of the n-th nearest resident actor of that job (farmer, smith,
//                                   child, beggar, lamplighter, bard...) out in the street
//   gotocritter [n]                 stand beside the n-th nearest village animal
//   leave                           step straight out of the building (or site) the player is in
//   expect folk <residents|walking|seated|sleeping|working|critters|stuck> <N> [max]
//   expect folk posture <posture> <N>   at least N people in that posture (sit, sitdrink, hammer, hoe, sleep, lute...)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

namespace {

std::string lowerS(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c + 32); return s; }

const char* postureName(art::Posture p) {
  static const char* n[] = {"none", "sit", "siteat", "sitdrink", "eat", "drink", "cheer", "hammer", "hoe", "sweep", "chop", "stir", "carry",
                            "fish", "sleep", "wave", "play", "dance", "lute", "drum", "flute", "pray", "beg", "lamp", "read", "sitfloor", "sitflooreat", "sitfloordrink",
                            "ride"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)art::Posture::COUNT, "a name for every posture");
  return (int)p < (int)art::Posture::COUNT ? n[(int)p] : "?";
}
int postureOf(const std::string& w) {
  for (int k = 0; k < (int)art::Posture::COUNT; k++) if (w == postureName((art::Posture)k)) return k;
  return -1;
}

int nearestSettlement(Game& g) {
  if (g.inside && g.subBldg >= 0) return g.world.over.bldgs[(size_t)g.subBldg].site;
  int best = -1;
  float bd = 1e30f;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (int si : g.world.nearSites) {
    if (si < 0 || si >= (int)g.world.sites.size() || !g.world.sites[(size_t)si].settlement()) continue;
    const Site& s = g.world.sites[(size_t)si];
    const float dx = s.ex - px, dy = s.ey - py, d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = si; }
  }
  return best;
}

bool cmdFolk(ScriptCtx& c) {
  Game& g = c.game;
  const Game::LifeFolkStats& s = g.lifeFolkStats();
  int post[(int)art::Posture::COUNT] = {};
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc) post[(int)g.actors[k].posture]++;
  std::string pl;
  for (int k = 1; k < (int)art::Posture::COUNT; k++) if (post[k]) pl += std::string(" ") + postureName((art::Posture)k) + "=" + std::to_string(post[k]);
  std::printf("folk: hour %.2f residents %d walking %d seated %d working %d sleeping %d animals %d stuck %d | spawned %d put away %d | paths %d (hits %d, failed %d) | ms last %.3f worst %.3f avg %.4f\n",
              g.hour, s.residents, s.walking, s.seated, s.working, s.sleeping, s.critters, s.stuck, s.spawned, s.despawned, s.pathRequests, s.pathCacheHits,
              s.pathFails, s.lastMs, s.worstMs, s.steps ? s.sumMs / s.steps : 0.0);
  std::printf("  postures:%s\n", pl.c_str());
  if (c.arg(1) == "near") {   // every person within 12 tiles of the hero: who spawned them (debugging the street)
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& a = g.actors[k];
      if (!a.npc || len2(a.p - g.pl().p) > (12.0f * 16.0f) * (12.0f * 16.0f)) continue;
      std::printf("  near %-18s role %d site %d bldg %d slot %d resident %d quest %d posture %d at %d,%d\n", a.name.c_str(), (int)a.role, a.site, a.bldg, a.slot,
                  a.resident, a.quest, (int)a.posture, (int)(a.p.x / 16), (int)(a.p.y / 16));
    }
  }
  const int si = nearestSettlement(g);
  life::Census* cs = si >= 0 ? g.life.census(g.world, si) : nullptr;
  std::vector<std::pair<float, int>> near;
  for (size_t k = 1; k < g.actors.size(); k++)
    if (g.actors[k].npc) near.push_back({len2(g.actors[k].p - g.pl().p), (int)k});
  std::sort(near.begin(), near.end());
  for (size_t i = 0; i < near.size() && i < 24; i++) {
    const Actor& a = g.actors[(size_t)near[i].second];
    std::string plan = "-";
    std::string job = a.critter ? "animal" : "-";
    if (cs && a.resident >= 0 && a.resident < (int)cs->res.size() && (a.site == si || g.inside)) {
      const life::Resident& r = cs->res[(size_t)a.resident];
      const life::Plan p = g.life.plan(g.world, *cs, r, g.day, g.hour);
      plan = std::string(life::actName(p.act)) + "@" + life::placeName(p.place) + (p.bldg >= 0 ? "#" + std::to_string(p.bldg) : "");
      job = life::jobName(r.job);
    }
    std::printf("  %-18s %-11s %-22s %-9s st %d at %d,%d%s\n", a.name.c_str(), job.c_str(), plan.c_str(), postureName(a.posture), (int)a.st,
                (int)std::floor(a.p.x / TILE), (int)std::floor((a.p.y - 2) / TILE), a.useX >= 0 ? " (uses furniture)" : "");
  }
  if (lowerS(c.arg(1)) == "all" && cs) {
    for (const life::Resident& r : cs->res) {
      const life::Plan p = g.life.plan(g.world, *cs, r, g.day, g.hour);
      std::printf("  res %3d %-18s %-11s %-11s@%-10s bldg %3d home %3d work %3d actor %d flags 0x%x\n", r.idx, r.name.c_str(), life::jobName(r.job),
                  life::actName(p.act), life::placeName(p.place), p.bldg, r.home, r.work, r.actor, r.flags);
    }
  }
  return true;
}
EMB_SCRIPT_CMD("folk", "folk [all]: print what the townsfolk's actors are doing (M5 TOWNSFOLK)", cmdFolk);

bool cmdFolkCap(ScriptCtx& c) {
  c.game.lifeFolkCap = std::max(0, std::atoi(c.arg(1).c_str()));
  return true;
}
EMB_SCRIPT_CMD("folkcap", "folkcap <n>: tests: resident actors a settlement may put out at once (0: default)", cmdFolkCap);

bool cmdEnterGathering(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) g.debugLeave();
  const int si = nearestSettlement(g);
  life::Census* cs = si >= 0 ? g.life.census(g.world, si) : nullptr;
  if (!cs) { c.fail("entergathering: no settlement here"); return true; }
  const int off = c.arg(1) == "2" ? cs->gathering2 : cs->gathering;
  const Site& st = g.world.sites[(size_t)si];
  if (off < 0 || off >= st.bldgCount) { c.fail("entergathering: " + st.name + " gathers in the open"); return true; }
  if (!g.debugEnterBuilding(st.bldgFirst + off, 0)) { c.fail("entergathering: could not enter"); return true; }
  g.mode = Mode::Play;
  c.view.snap(g);
  {
    const Bldg& B = g.world.over.bldgs[(size_t)(st.bldgFirst + off)];
    std::printf("script: entergathering -> %s of %s (seed %08x, %dx%d, interior %dx%d)\n", bldgTypeName(B.type), st.name.c_str(), (unsigned)B.seed,
                B.r.w, B.r.h, g.sub.w, g.sub.h);
  }
  return true;
}
EMB_SCRIPT_CMD("entergathering", "entergathering [2]: enter the nearest settlement's gathering place (M5)", cmdEnterGathering);

bool cmdLeave(ScriptCtx& c) {
  c.game.debugLeave();
  c.game.mode = Mode::Play;
  c.view.snap(c.game);
  return true;
}
EMB_SCRIPT_CMD("leave", "leave: step straight out of the building or site the player is in (M5)", cmdLeave);

bool cmdGotoJob(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) { c.fail("gotojob: outdoors only"); return true; }
  const std::string want = lowerS(c.arg(1));
  const int nth = std::max(0, std::atoi(c.arg(2).c_str()));
  std::vector<std::pair<float, int>> cand;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (a.resident < 0 || a.site < 0) continue;
    life::Census* cs = g.life.census(g.world, a.site);
    if (!cs || a.resident >= (int)cs->res.size()) continue;
    if (lowerS(life::jobName(cs->res[(size_t)a.resident].job)) != want) continue;
    cand.push_back({len2(a.p - g.pl().p), a.id});
  }
  std::sort(cand.begin(), cand.end());
  if ((int)cand.size() <= nth) { c.fail("gotojob: no " + want + " out (" + std::to_string(cand.size()) + ")"); return true; }
  for (const Actor& a : g.actors)
    if (a.id == cand[(size_t)nth].second) {
      const std::string name = a.name;   // (the teleport may move the actors)
      const int32_t gx = g.world.ox + (int)std::floor(a.p.x / TILE), gy = g.world.oy + (int)std::floor((a.p.y - 2) / TILE) + 2;
      g.teleportGlobal(gx, gy);
      c.view.snap(g);
      std::printf("script: gotojob -> %s\n", name.c_str());
      break;
    }
  return true;
}
EMB_SCRIPT_CMD("gotojob", "gotojob <job> [n]: stand two tiles south of the n-th nearest resident of that job out in the street (M5)", cmdGotoJob);

bool cmdGotoCritter(ScriptCtx& c) {
  Game& g = c.game;
  const int nth = std::max(0, std::atoi(c.arg(1).c_str()));
  std::vector<std::pair<float, int>> cand;
  for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].critter) cand.push_back({len2(g.actors[k].p - g.pl().p), (int)k});
  std::sort(cand.begin(), cand.end());
  if ((int)cand.size() <= nth) { c.fail("gotocritter: no animal out"); return true; }
  const Actor& a = g.actors[(size_t)cand[(size_t)nth].second];
  const std::string name = a.name;
  const int32_t gx = g.world.ox + (int)std::floor(a.p.x / TILE) + 1, gy = g.world.oy + (int)std::floor((a.p.y - 2) / TILE) + 1;
  g.teleportGlobal(gx, gy);
  c.view.snap(g);
  std::printf("script: gotocritter -> %s\n", name.c_str());
  return true;
}
EMB_SCRIPT_CMD("gotocritter", "gotocritter [n]: stand beside the n-th nearest village animal (M5)", cmdGotoCritter);

bool expFolk(ScriptCtx& c) {
  Game& g = c.game;
  const Game::LifeFolkStats& s = g.lifeFolkStats();
  const std::string what = lowerS(c.arg(2));
  if (what == "posture") {
    const int p = postureOf(lowerS(c.arg(3)));
    if (p < 0) { c.fail("expect folk posture <sit|sitdrink|hammer|hoe|sleep|lute|...> <N>"); return true; }
    int n = 0;
    for (size_t k = 1; k < g.actors.size(); k++) n += g.actors[k].npc && (int)g.actors[k].posture == p;
    const int want = std::atoi(c.arg(4).c_str());
    if (n < want) c.fail("expect folk posture " + c.arg(3) + ": " + std::to_string(n) + " < " + std::to_string(want));
    return true;
  }
  int have = -1;
  if (what == "residents") have = s.residents;
  else if (what == "walking") have = s.walking;
  else if (what == "seated") have = s.seated;
  else if (what == "sleeping") have = s.sleeping;
  else if (what == "working") have = s.working;
  else if (what == "critters" || what == "animals") have = s.critters;
  else if (what == "stuck") have = s.stuck;
  if (have < 0) { c.fail("expect folk <residents|walking|seated|sleeping|working|critters|stuck> <N> [max]"); return true; }
  const int lo = std::atoi(c.arg(3).c_str());
  const int hi = c.arg(4).empty() ? 1 << 30 : std::atoi(c.arg(4).c_str());
  if (have < lo || have > hi) c.fail("expect folk " + what + ": " + std::to_string(have) + " not in [" + std::to_string(lo) + ", " + (c.arg(4).empty() ? std::string("...") : c.arg(4)) + "]");
  return true;
}
EMB_SCRIPT_CMD("expect:folk", "expect folk <residents|walking|seated|sleeping|working|critters|stuck> <N> [max] | posture <p> <N> (M5)", expFolk);

}  // namespace
