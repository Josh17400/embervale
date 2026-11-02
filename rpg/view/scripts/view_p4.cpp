// P4 VIEW test commands (docs/FIX_BATCH_P4.md 4, 7 6a/6b/6f, 9.3). Every command is also a Witness call.
//   p4dungeon ruin|cave [above N]   enter the nearest ruin (or cave) of the loaded window whose level is at least the
//                                   hero's + N (default 0), through Game::debugEnterSite (WITNESS's state test uses it)
//   expect dlggift <text>|<pat*>    the open dialogue panel's gift strip holds that gift ("OLD BLADE" or "+ OLD BLADE";
//                                   * wildcards at either end)
//   expect notoast <text>|<pat*>    no toast on show (or waiting) reads that (a gift said in the panel is not repeated)
//   expect skull <N>[,<M>...]       the danger skull beside the place's name (dangerSkull: -1 none, 0 green .. 3 purple)
//   expect banner <text>|<pat*>     the centre banner is up and its heading reads that (or "<heading> / <sub>" does)
//   expect menuhint <text>|<pat*>   the open menu page's key hint, as drawn at its foot (keyboard)
//   expect pickuplift [px]          a fresh drop (younger than 0.3 s) was drawn at least px (default 3) above its spot
#include <cmath>
#include <cstdlib>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/view/hud_layout.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/view/view_probe.h"

namespace {
// "*a*" substring, "a*" prefix, "*a" suffix, else exact
bool wild(const std::string& have, std::string want) {
  const bool head = !want.empty() && want.front() == '*';
  if (head) want.erase(0, 1);
  const bool tail = !want.empty() && want.back() == '*';
  if (tail) want.pop_back();
  if (head && tail) return have.find(want) != std::string::npos;
  if (tail) return have.compare(0, want.size(), want) == 0;
  if (head) return have.size() >= want.size() && have.compare(have.size() - want.size(), want.size(), want) == 0;
  return have == want;
}

bool cmdDungeon(ScriptCtx& c) {
  Game& g = c.game;
  const std::string kind = c.arg(1);
  if (kind != "ruin" && kind != "cave") { c.fail("p4dungeon ruin|cave [above N]"); return true; }
  const SiteType want = kind == "ruin" ? SiteType::Ruin : SiteType::Cave;
  const int above = c.arg(2) == "above" ? std::atoi(c.arg(3).c_str()) : 0;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  int best = -1;
  float bd = 1e30f;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (s.type != want || s.level < g.plLevel + above) continue;
    const float d = (s.ex - px) * (s.ex - px) + (s.ey - py) * (s.ey - py);
    if (d < bd) { bd = d; best = (int)i; }
  }
  if (best < 0) { c.fail("p4dungeon: no " + kind + " of level " + std::to_string(g.plLevel + above) + "+ in the loaded world"); return true; }
  if (!g.debugEnterSite(best)) c.fail("p4dungeon: cannot enter " + g.world.sites[(size_t)best].name);
  return true;
}
EMB_SCRIPT_CMD("p4dungeon", "p4dungeon ruin|cave [above N]: into the nearest ruin or cave at least N levels above the hero (P4 6a)", cmdDungeon);

bool expDlgGift(ScriptCtx& c) {
  const std::string want = c.rest(2);
  if (c.game.mode != Mode::Dialogue) { c.fail("expect dlggift " + want + ": no dialogue open"); return true; }
  std::string seen;
  for (const Toast* t : FxState::dlgGifts(c.view)) {
    const std::string name = t->s.rfind("+ ", 0) == 0 ? t->s.substr(2) : t->s;
    if (wild(t->s, want) || wild(name, want)) return true;
    seen += (seen.empty() ? "" : " | ") + t->s;
  }
  c.fail("expect dlggift " + want + ": the gift strip holds [" + seen + "]");
  return true;
}
EMB_SCRIPT_CMD("expect:dlggift", "expect dlggift <text>: the dialogue panel's gift strip holds that gift (P4 3)", expDlgGift);

bool expNoToast(ScriptCtx& c) {
  const std::string want = c.rest(2);
  for (const Toast& t : c.view.toastsShown())
    if (wild(t.s, want)) { c.fail("expect notoast " + want + ": the toast '" + t.s + "' is " + (t.dlg ? "a gift still held for the panel" : "in the column")); return true; }
  return true;
}
EMB_SCRIPT_CMD("expect:notoast", "expect notoast <text>: no toast reads that (P4 3: a gift said in the panel is not repeated)", expNoToast);

bool expSkull(ScriptCtx& c) {
  const int have = dangerSkull(c.game);
  std::string list = c.arg(2);
  for (size_t at = 0; at <= list.size();) {
    const size_t comma = list.find(',', at);
    const std::string one = list.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!one.empty() && std::atoi(one.c_str()) == have) return true;
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  c.fail("expect skull " + list + ": the skull is " + std::to_string(have) + " (dungeon level " + std::to_string(dungeonLevel(c.game)) +
         ", hero level " + std::to_string(c.game.plLevel) + ")");
  return true;
}
EMB_SCRIPT_CMD("expect:skull", "expect skull <N>[,<M>]: the danger skull beside the place's name (-1 none, 0..3) (P4 6a)", expSkull);

bool expBanner(ScriptCtx& c) {
  const FxState& f = FxState::of(c.view);
  const std::string want = c.rest(2);
  if (f.bannerT_ > 0 && (wild(f.banner_, want) || wild(f.banner_ + " / " + f.bannerSub_, want))) return true;
  c.fail("expect banner " + want + ": the banner reads '" + f.banner_ + " / " + f.bannerSub_ + "'" + (f.bannerT_ > 0 ? "" : " (not up)"));
  return true;
}
EMB_SCRIPT_CMD("expect:banner", "expect banner <text>: the centre banner is up and reads that (P4 6a)", expBanner);

bool expMenuHint(ScriptCtx& c) {
  if (c.game.mode != Mode::Menu) { c.fail("expect menuhint: no menu open"); return true; }
  const int tab = ViewProbe::menuTab(c.view);
  const std::string have = menuHint(tab, c.view.touchUI);
  if (!wild(have, c.rest(2))) c.fail("expect menuhint " + c.rest(2) + ": the " + menuTabName(tab) + " page's hint reads '" + have + "'");
  return true;
}
EMB_SCRIPT_CMD("expect:menuhint", "expect menuhint <text>: the open menu page's key hint (P4 6b)", expMenuHint);

bool expPickupLift(ScriptCtx& c) {
  const float need = c.arg(2).empty() ? 3.0f : (float)std::atof(c.arg(2).c_str());
  float age = -1;
  const float lift = pickupLiftDrawn(age);
  if (age < 0 || age >= kTossTime || lift < need)
    c.fail("expect pickuplift: the last tossed drop drawn was " + std::to_string(lift) + " px up at " + std::to_string(age) + " s");
  return true;
}
EMB_SCRIPT_CMD("expect:pickuplift", "expect pickuplift [px]: a fresh drop was drawn tossed above its spot (P4 6f)", expPickupLift);
}  // namespace
