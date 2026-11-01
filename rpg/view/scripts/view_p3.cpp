// P3 VIEW test commands (docs/FIX_BATCH_P3.md 3 B7 B8 B11 B12, 8.3). Every command is also a Witness call.
//   expect zoomscale <D>          the world scale in use (screen::worldScale) and the world layer as drawn are both D
//   expect toast <text>|<prefix*> a toast on show reads exactly <text> (or starts with <prefix> when it ends in *)
//   p3grab                        the hero steps onto the nearest piece lying on the ground (the game's own pickup takes it)
//   p3boss                        the nearest living hostile becomes a dungeon boss (foes::Rank::Boss): the Witness
//                                 state's `boss` / `rank` fields for a staged boss
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include "rpg/sim/foes.h"
#include "rpg/sim/game.h"
#include "rpg/view/screen.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

struct P3ViewScriptOps {
  static const Pix::Layer& worldLayer(const View& v) { return v.pix_->worldLayer(); }
};

namespace {
using Ops = P3ViewScriptOps;

bool expZoomScale(ScriptCtx& c) {
  const int want = std::atoi(c.arg(2).c_str());
  const int have = screen::worldScale();
  const float drawn = Ops::worldLayer(c.view).scale;
  if (have != want || std::fabs(drawn - (float)want) > 1e-3f)
    c.fail("expect zoomscale " + c.arg(2) + ": the zoom asks for " + std::to_string(have) + ", the world layer is drawn at " + std::to_string(drawn));
  return true;
}
EMB_SCRIPT_CMD("expect:zoomscale", "expect zoomscale <D>: the world scale in use and the world layer as drawn (P3 B8)", expZoomScale);

bool expToast(ScriptCtx& c) {
  std::string want = c.rest(2);
  const bool prefix = !want.empty() && want.back() == '*';
  if (prefix) want.pop_back();
  std::string seen;
  for (const Toast& t : c.view.toastsShown()) {
    if (prefix ? t.s.compare(0, want.size(), want) == 0 : t.s == want) return true;
    seen += (seen.empty() ? "" : " | ") + t.s;
  }
  c.fail("expect toast " + c.rest(2) + ": the toasts are [" + seen + "]");
  return true;
}
EMB_SCRIPT_CMD("expect:toast", "expect toast <text>|<prefix*>: a toast on show reads that (P3 B11)", expToast);

bool cmdBoss(ScriptCtx& c) {
  Game& g = c.game;
  const Vec2 p = g.pl().p;
  Actor* best = nullptr;
  float bd = 1e30f;
  for (Actor& a : g.actors) {
    if (!a.hostile || a.st == AState::Dead || a.id == g.pl().id) continue;
    const float d = dot(a.p - p, a.p - p);
    if (d < bd) { bd = d; best = &a; }
  }
  if (!best) { c.fail("p3boss: no living hostile near"); return true; }
  best->boss = true;
  best->rank = (uint8_t)foes::Rank::Boss;
  return true;
}
EMB_SCRIPT_CMD("p3boss", "p3boss: the nearest living hostile becomes a dungeon boss (rank 4; P3 B12's Witness state)", cmdBoss);

bool cmdGrab(ScriptCtx& c) {
  Game& g = c.game;
  const Vec2 p = g.pl().p;
  const Pickup* best = nullptr;
  float bd = 1e30f;
  for (const Pickup& k : g.pickups) {
    const float d = dot(k.p - p, k.p - p);
    if (d < bd) { bd = d; best = &k; }
  }
  if (!best) { c.fail("p3grab: nothing lies on the ground"); return true; }
  g.pl().p = best->p;
  return true;
}
EMB_SCRIPT_CMD("p3grab", "p3grab: the hero steps onto the nearest piece on the ground; the game's pickup takes it (P3 B11)", cmdGrab);
}  // namespace
