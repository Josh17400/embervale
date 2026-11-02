// P3 VIEW test commands (docs/FIX_BATCH_P3.md 3 B7 B8 B11 B12, 8.3). Every command is also a Witness call.
//   expect zoomscale <D>          the world scale in use (screen::worldScale) and the world layer as drawn are both D
//   expect toast <text>|<prefix*> a toast on show reads exactly <text> (or starts with <prefix> when it ends in *)
//   p3grab                        the hero steps onto the nearest piece lying on the ground (the game's own pickup takes it)
//   p3kit <member> <party slot>   the piece they wear there is their own kit (IF_KIT; stages the PARTY rule's flag)
//   p3parted <member> [daysago] [angry]   a copy of that member's record on the parted roll (Party::parted), parted
//                                 daysago days back (the PARTED page's staging; the member stays)
//   p3page parted|remembered|party   the PARTY page's page (after `partyview page`)
//   p3say <member> <words...>     the member says a line (Game::partySay: Ev::Banter, Party::lastBanter)
//   expect cardmaker <text>       the last item card's maker row (gear::makerLine); * at either end: a prefix / suffix /
//                                 substring match
//   p3boss                        the nearest living hostile becomes a dungeon boss (foes::Rank::Boss): the Witness
//                                 state's `boss` / `rank` fields for a staged boss
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include "rpg/sim/foes.h"
#include "rpg/sim/game.h"
#include "rpg/sim/party.h"
#include "rpg/view/view_probe.h"
#include "rpg/view/screen.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

struct P3ViewScriptOps {
  static const Pix::Layer& worldLayer(const View& v) { return v.pix_->worldLayer(); }
  static int& partyPage(View& v) { return v.party_.partyPage_; }
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

// "*a*" substring, "a*" prefix, "*a" suffix, else exact
bool wildMatch(const std::string& have, std::string want) {
  const bool head = !want.empty() && want.front() == '*';
  if (head) want.erase(0, 1);
  const bool tail = !want.empty() && want.back() == '*';
  if (tail) want.pop_back();
  if (head && tail) return have.find(want) != std::string::npos;
  if (tail) return have.compare(0, want.size(), want) == 0;
  if (head) return have.size() >= want.size() && have.compare(have.size() - want.size(), want.size(), want) == 0;
  return have == want;
}

bool expLootMaker(ScriptCtx& c) {
  const View::CardInfo ci = ViewProbe::lastCard(c.view);
  if (ci.rarity < 0) { c.fail("expect cardmaker: no item card drawn"); return true; }
  if (!wildMatch(ci.maker, c.rest(2))) c.fail("expect cardmaker " + c.rest(2) + ": the maker row reads '" + ci.maker + "' on " + ci.name);
  return true;
}
EMB_SCRIPT_CMD("expect:cardmaker", "expect cardmaker <text>: the last item card's maker row as drawn (gear::makerLine; * wildcards) (P3 VIEW; expect lootmaker is LOOT's pack-piece check)", expLootMaker);

party::Member* memberArg(ScriptCtx& c, size_t i) {
  const int k = std::atoi(c.arg(i).c_str());
  if (k < 0 || k >= (int)c.game.party.members.size()) { c.fail(c.arg(0) + ": no member " + c.arg(i)); return nullptr; }
  return &c.game.party.members[(size_t)k];
}

bool cmdKit(ScriptCtx& c) {
  party::Member* m = memberArg(c, 1);
  if (!m) return true;
  const int slot = std::atoi(c.arg(2).c_str());
  const int e = slot >= 0 && slot < party::SLOTS ? m->eq[(size_t)slot] : -1;
  if (e < 0 || e >= (int)m->inv.size()) { c.fail("p3kit: nothing worn in slot " + c.arg(2)); return true; }
  m->inv[(size_t)e].flags |= party::IF_KIT;
  return true;
}
EMB_SCRIPT_CMD("p3kit", "p3kit <member> <party slot>: the piece worn there is their own kit (IF_KIT) (P3)", cmdKit);

bool cmdParted(ScriptCtx& c) {
  party::Member* m = memberArg(c, 1);
  if (!m) return true;
  party::Parted pt;
  pt.m = party::savedForm(*m);
  pt.day = c.game.day - (c.a.size() > 2 ? std::atoi(c.arg(2).c_str()) : 0);
  pt.angry = c.a.size() > 3 && c.arg(3) == "angry";
  party::Party::syncParted(pt);
  c.game.party.parted.push_back(pt);
  if ((int)c.game.party.parted.size() > party::MAX_PARTED) c.game.party.parted.erase(c.game.party.parted.begin());
  return true;
}
EMB_SCRIPT_CMD("p3parted", "p3parted <member> [daysago] [angry]: a copy of their record on the parted roll (P3)", cmdParted);

bool cmdPage(ScriptCtx& c) {
  const std::string w = c.arg(1);
  int& pg = Ops::partyPage(c.view);
  if (w == "parted") pg = 2;
  else if (w == "remembered") pg = 1;
  else if (w == "party") pg = 0;
  else c.fail("p3page parted|remembered|party");
  return true;
}
EMB_SCRIPT_CMD("p3page", "p3page parted|remembered|party: the PARTY page's page (P3)", cmdPage);

bool cmdSay(ScriptCtx& c) {
  const int k = std::atoi(c.arg(1).c_str());
  if (k < 0 || k >= (int)c.game.party.members.size()) { c.fail("p3say: no member " + c.arg(1)); return true; }
  c.game.partySay(k, c.rest(2), 5.0f);
  return true;
}
EMB_SCRIPT_CMD("p3say", "p3say <member> <words>: the member says a line (P3)", cmdSay);
}  // namespace
