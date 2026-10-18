// P2a L6 (restated 2026-10-18, fix round 1, L6-NIGHT-BRIGHT-GROUND): an UNLIT townsperson at night. The hero carries a
// torch (render.cpp: a 100 px light at his feet) and the night lift is fullest round him, so the hero's own shot pair
// measures the best case. These commands stage a villager on the ground the hero stood on (gotoground), step the hero
// `tiles` (8: 128 px, past the torch's 100 px and the lift's 96 px easing) east, and shoot the villager's pair: the world
// with and without the villager's body, the hero hidden in both and the contact shadow kept (p2_measure.mjs contrast).
// Test only (added beside the frozen phase A harness; nothing here renames or changes a phase A command).
//   npcstage pick            remember the nearest townsperson's look (run it in a village by day; else a plain civilian)
//   npcstage [tiles]         stage the villager where the hero stands, facing down, and step the hero `tiles` east
//   npcpair <with> <without> two device shots of the same moment: the staged villager with and without its body
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/view/rise_view.h"
#include "rpg/view/screen.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

namespace {
struct NightNpc {
  bool havePick = false;
  art::HumanLook pick;
  int id = -1;          // the staged actor's id
  Vec2 spot;            // where it stands
} s_npc;

int actorIndex(const Game& g, int id) {   // (Game::findActor is private)
  if (id < 0) return -1;
  for (size_t k = 0; k < g.actors.size(); k++) if (g.actors[k].id == id) return (int)k;
  return -1;
}

bool cmdNpcStage(ScriptCtx& c) {
  Game& g = c.game;
  if (c.arg(1) == "pick") {
    int best = -1;
    float bd = 1e30f;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& a = g.actors[k];
      if (!a.npc || !a.human || a.st == AState::Dead || a.role == Role::Guard || a.role == Role::Child ||
          a.role == Role::King || a.hostile)
        continue;
      const float d = len2(a.p - g.pl().p);
      if (d < bd) { bd = d; best = (int)k; }
    }
    if (best < 0) { c.fail("npcstage pick: no townsperson near"); return true; }
    s_npc.pick = g.actors[(size_t)best].look;
    s_npc.havePick = true;
    std::printf("script: npcstage pick %s\n", g.actors[(size_t)best].name.c_str());
    return true;
  }
  if (g.inside) { c.fail("npcstage: outdoors only"); return true; }
  const int tiles = c.arg(1).empty() ? 8 : std::max(5, std::atoi(c.arg(1).c_str()));
  const Vec2 spot = g.pl().p;
  int k = actorIndex(g, s_npc.id);
  if (k < 0) {
    const int id = SkillOps::spawnPerson(g, Role::Bandit, spot, 1);
    k = actorIndex(g, id);
    if (k < 0) { c.fail("npcstage: could not spawn the villager"); return true; }
    s_npc.id = id;
  }
  Actor& a = g.actors[(size_t)k];
  // a civilian: no weapon, armour or helm, not hostile, standing still
  a.hostile = false; a.aggro = false; a.npc = true; a.faction = Faction::Town; a.role = Role::Villager;
  a.name = "VILLAGER"; a.target = -1;
  if (s_npc.havePick) a.look = s_npc.pick;
  else { a.look.weapon = 0; a.look.shield = false; a.look.armorStyle = 0; a.look.helmet = false; }
  a.speed = 0; a.home = a.goal = spot;
  s_npc.spot = spot;
  // the hero steps away east (his torch and the lift's full strength go with him) and the camera follows
  g.pl().p = spot + Vec2((float)(tiles * TILE), 0);
  g.pl().aim = Vec2(0, 1);
  g.pl().face = 0;
  c.view.snap(g);
  std::printf("script: npcstage villager at %.0f,%.0f, the hero %d tiles east\n", spot.x, spot.y, tiles);
  return true;
}
EMB_SCRIPT_CMD("npcstage", "npcstage pick | npcstage [tiles]: stage an unlit villager where the hero stands and step the hero tiles (8) east (P2a L6 night, test only)", cmdNpcStage);

bool cmdNpcPair(ScriptCtx& c) {
  Game& g = c.game;
  if (c.arg(1).empty() || c.arg(2).empty()) { c.fail("npcpair: <with.png> <without.png>"); return true; }
  const int k = actorIndex(g, s_npc.id);
  if (k < 0) { c.fail("npcpair: no staged villager (npcstage first)"); return true; }
  {   // pinned on its spot, idle, facing down (its AI may have nudged it since the stage)
    Actor& a = g.actors[(size_t)k];
    a.p = s_npc.spot; a.vel = Vec2(0, 0); a.knock = Vec2(0, 0); a.st = AState::Idle; a.stT = 0; a.face = 0; a.aim = Vec2(0, 1);
  }
  const bool wasHidden = c.view.p2a.hidePlayer;
  const screen::Settings wasSet = screen::get();
  { screen::Settings s = wasSet; s.ring = false; screen::set(s); }   // no ring on either shot
  Pix& pix = RiseViewOps::pix(c.view);
  c.view.p2a.hidePlayer = true;   // the hero's body hidden in both shots
  for (int pass = 0; pass < 2; pass++) {
    // the second shot: the villager's body drawn the way the hidden hero's is (not at all), its contact shadow kept
    g.actors[(size_t)k].player = pass == 1;
    c.view.draw(g, false);
    g.actors[(size_t)k].player = false;
    if (!pix.screenshot(c.arg(1 + pass).c_str())) c.fail("npcpair: could not write " + c.arg(1 + pass));
  }
  c.view.p2a.hidePlayer = wasHidden;
  screen::set(wasSet);
  return true;
}
EMB_SCRIPT_CMD("npcpair", "npcpair <with.png> <without.png>: the staged villager with and without its body, the hero hidden in both (P2a L6 night, test only)", cmdNpcPair);
}  // namespace
