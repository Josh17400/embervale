// M6 Steel script commands of the NUMBERS lane (numbers and the forge). See rpg/view/script_api.h for how they register.
//   forgeat smith|smelter|tanner|weaver   open the forge screen of the nearest worker of that station (as WORK THE ...)
//   forgepick <recipe id>                 select that recipe on the forge screen (its tab and row)
//   forgemake <recipe id> [n]             make it n times (1) at the open bench; fails when it cannot be made
//   plevel N                              the hero's level (attunement, level sync checks); recalculated at once
//   packfront <words>|LEGENDARY|SYNCED    move the first pack item whose name holds the words (or the first legendary, the
//                                         first synced piece) to the top: the pack shows its details; equipment follows it
//   gomining                              travel to the nearest mining settlement (village, town or city)
//   shopat merchant|smith|...             open the shop of the nearest person of that role in play
//   expect crafted <words>                a crafted item (IF_CRAFTED) whose name holds the words is in the pack
//   expect stocks <words>                 the open shop's shelf has an item whose name holds the words
//   expect purse                          the open shop shows a merchant's purse (7.5 rule 8)
//   expect potioncd                       the shared potion cooldown is running
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/craft.h"
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/view/script_api.h"
#include "rpg/world/coords.h"
#include "rpg/world/source.h"

namespace {

std::string up(std::string s) {
  for (char& ch : s) if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
  return s;
}

int recipeIndex(const Game& g, const std::string& id) {
  for (size_t i = 0; i < g.bench.recipes.size(); i++) if (g.bench.recipes[i].id == id) return (int)i;
  return -1;
}

bool cmdForgeAt(ScriptCtx& c) {
  Game& g = c.game;
  const std::string w = c.arg(1);
  art::Building at = art::Building::Smithy;
  if (w == "smelter") at = art::Building::Smelter;
  else if (w == "tanner" || w == "tannery") at = art::Building::Tanner;
  else if (w == "weaver" || w == "loom") at = art::Building::Weaver;
  else if (w == "sawmill" || w == "bowyer") at = art::Building::Sawmill;
  int best = -1;
  float bd = 1e30f;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (!a.npc || !a.human || a.st == AState::Dead) continue;
    const bool smith = a.role == Role::Smith;
    const art::Building b = a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size() ? g.world.over.bldgs[(size_t)a.bldg].type : art::Building::House;
    if (!(smith && at == art::Building::Smithy) && b != at) continue;
    const float d = len2(a.p - g.pl().p);
    if (d < bd) { bd = d; best = a.id; }
  }
  if (best < 0 || !craft::openBench(g, best, at)) c.fail("forgeat: nobody to lend a " + w + " here");
  return true;
}
EMB_SCRIPT_CMD("forgeat", "forgeat smith|smelter|tanner|weaver: open the forge screen of the nearest such worker (M6)", cmdForgeAt);

bool cmdForgePick(ScriptCtx& c) {
  const int i = recipeIndex(c.game, c.arg(1));
  if (i < 0) { c.fail("forgepick: the bench has no recipe '" + c.arg(1) + "'"); return true; }
  c.game.bench.focus = i;
  return true;
}
EMB_SCRIPT_CMD("forgepick", "forgepick <recipe id>: select that recipe on the forge screen (M6)", cmdForgePick);

bool cmdForgeMake(ScriptCtx& c) {
  Game& g = c.game;
  const int n = c.a.size() > 2 ? std::max(1, std::atoi(c.arg(2).c_str())) : 1;
  for (int k = 0; k < n; k++) {
    const int i = recipeIndex(g, c.arg(1));
    std::string why;
    if (i < 0) { c.fail("forgemake: the bench has no recipe '" + c.arg(1) + "'"); return true; }
    if (!craft::benchMake(g, i, &why)) { c.fail("forgemake " + c.arg(1) + ": " + why); return true; }
  }
  return true;
}
EMB_SCRIPT_CMD("forgemake", "forgemake <recipe id> [n]: make it at the open bench (M6)", cmdForgeMake);

bool cmdPLevel(ScriptCtx& c) {
  Game& g = c.game;
  g.plLevel = std::clamp(std::atoi(c.arg(1).c_str()), 1, gear::LEVEL_CAP);
  // re-derive the numbers (recalcPlayer runs on every equip toggle): take a piece off and on again
  for (int* e : {&g.eqArmor, &g.eqWeapon, &g.eqHelmet})
    if (*e >= 0) { const int i = *e; g.useItem(i); g.useItem(i); break; }
  return true;
}
EMB_SCRIPT_CMD("plevel", "plevel N: the hero's level (M6 attunement / level sync checks)", cmdPLevel);

bool cmdPackFront(ScriptCtx& c) {
  Game& g = c.game;
  const std::string want = up(c.rest(1));
  int at = -1;
  // (the newest match: a script gives the piece it wants shown last)
  for (int i = (int)g.inv.size() - 1; i >= 0 && at < 0; i--) {
    const Item& it = g.inv[(size_t)i];
    // two keywords: LEGENDARY (the first legendary), SYNCED (the first piece working below its item level)
    if (want == "LEGENDARY" ? it.rarity == Rarity::Legendary
        : want == "SYNCED" ? (itemEquippable(it.kind) && it.ilvl > 0 && gear::effLevel(it.ilvl, g.plLevel) < it.ilvl)
        : it.name.find(want) != std::string::npos) at = i;
  }
  if (at < 0) { c.fail("packfront: no '" + want + "' in the pack"); return true; }
  if (at == 0) return true;
  const Item it = g.inv[(size_t)at];
  g.inv.erase(g.inv.begin() + at);
  g.inv.insert(g.inv.begin(), it);
  for (int* e : {&g.eqWeapon, &g.eqBow, &g.eqStaff, &g.eqArmor, &g.eqHelmet, &g.eqShield, &g.eqRing, &g.eqAmulet, &g.eqGloves, &g.eqBoots, &g.eqCloak}) {
    if (*e == at) *e = 0;
    else if (*e >= 0 && *e < at) (*e)++;
  }
  return true;
}
EMB_SCRIPT_CMD("packfront", "packfront <words>: move that pack item to the top (its details show first) (M6)", cmdPackFront);

bool cmdGoMining(ScriptCtx& c) {
  Game& g = c.game;
  const Site& h = g.world.sites[(size_t)g.world.startSite];
  const int32_t gx = g.world.ox + h.ex, gy = g.world.oy + h.ey;
  // the region plans ring by ring: a settlement that lives from mining (its specialty or its archetype)
  ew::Gid best = 0;
  int64_t bd = INT64_MAX;
  const int32_t rx0 = ew::regionOf(gx), ry0 = ew::regionOf(gy);
  for (int r = 0; r <= 16 && !best; r++)
    for (int32_t ry = ry0 - r; ry <= ry0 + r; ry++)
      for (int32_t rx = rx0 - r; rx <= rx0 + r; rx++) {
        if (std::max(std::abs(rx - rx0), std::abs(ry - ry0)) != r) continue;
        const ew::RegionPlan R = g.world.regionPlan(rx, ry);
        for (const ew::SitePlan& p : R.sites) {
          if (p.type != SiteType::Village && p.type != SiteType::Town && p.type != SiteType::City) continue;
          if (p.special != ew::Specialty::Mining && p.archetype != ew::Archetype::Mining) continue;
          const int64_t dx = p.ex - gx, dy = p.ey - gy, d = dx * dx + dy * dy;
          if (d < bd) { bd = d; best = p.id; }
        }
      }
  const int si = best ? g.world.ensureSite(best) : -1;
  if (si < 0) { c.fail("gomining: no mining settlement within 16 regions"); return true; }
  g.world.sites[(size_t)si].discovered = true;
  g.fastTravel(si);
  g.sleepFade = 0;
  return true;
}
EMB_SCRIPT_CMD("gomining", "gomining: travel to the nearest mining settlement (M6 shops sell its ores)", cmdGoMining);

bool cmdShopAt(ScriptCtx& c) {
  Game& g = c.game;
  const std::string w = c.arg(1);
  Role want = Role::Merchant;
  if (w == "smith") want = Role::Smith;
  else if (w == "mage") want = Role::Mage;
  else if (w == "priest") want = Role::Priest;
  else if (w == "hunter") want = Role::Hunter;
  int best = -1;
  float bd = 1e30f;
  for (size_t k = 1; k < g.actors.size(); k++) {
    const Actor& a = g.actors[k];
    if (!a.npc || a.role != want || a.st == AState::Dead) continue;
    const float d = len2(a.p - g.pl().p);
    if (d < bd) { bd = d; best = a.id; }
  }
  if (best < 0 || !craft::openShopOf(g, best)) c.fail("shopat: no " + w + " in play");
  return true;
}
EMB_SCRIPT_CMD("shopat", "shopat merchant|smith|mage|priest|hunter: open the shop of the nearest one in play (M6)", cmdShopAt);

bool expCrafted(ScriptCtx& c) {
  const std::string want = up(c.rest(2));
  for (const Item& it : c.game.inv) if ((it.flags & IF_CRAFTED) && it.name.find(want) != std::string::npos) return true;
  c.fail("expect crafted: no crafted '" + want + "' in the pack");
  return true;
}
EMB_SCRIPT_CMD("expect:crafted", "expect crafted <words>: a crafted item with those words is in the pack (M6)", expCrafted);

bool expStocks(ScriptCtx& c) {
  const std::string want = up(c.rest(2));
  for (const Item& it : c.game.shop.stock) if (it.name.find(want) != std::string::npos) return true;
  c.fail("expect stocks: the shelf has no '" + want + "'");
  return true;
}
EMB_SCRIPT_CMD("expect:stocks", "expect stocks <words>: the open shop sells an item with those words (M6)", expStocks);

bool expPurse(ScriptCtx& c) {
  if (c.game.craft.live.purse.find(c.game.shop.key) == c.game.craft.live.purse.end()) c.fail("expect purse: the shop has no merchant's purse");
  return true;
}
EMB_SCRIPT_CMD("expect:purse", "expect purse: the open shop has a merchant's purse (M6, 7.5 rule 8)", expPurse);

bool expPotionCd(ScriptCtx& c) {
  if (c.game.craft.live.potionCd <= 0) c.fail("expect potioncd: the potion cooldown is not running");
  return true;
}
EMB_SCRIPT_CMD("expect:potioncd", "expect potioncd: the shared potion cooldown is running (M6)", expPotionCd);

}  // namespace
