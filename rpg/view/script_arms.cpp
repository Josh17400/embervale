// M6 Steel, ARMS lane: script commands for the culture arms and armour looks (tools/scripts/m6_arms_*.txt).
//   armskit <archetype|here|none> [mat leather|bronze|iron|steel|alloy] [alloy N] [rarity 0-4] [ilvl N]
//           [spear|sword|greatsword|dagger|axe|mace|bow] [noshield]: take everything off, then wear a full set (body,
//           helmet, shield, gloves, boots, cloak, the weapon; with spear/sword also the culture's bow on the back) made
//           by the nearest culture FAMILY of that archetype (searched over the culture cells around the player, so any
//           archetype is found anywhere; `here` = the culture the player stands in, `none` = heartland pieces).
//           mat alloy without a number wears the culture's best alloy.
//   expect armslook: the player's look carries the worn culture's forms (helm, body, and the weapon's form)
//   expect armsicons: every worn piece made by a culture paints a culture icon (not the classic one)
#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_look.h"
#include "rpg/view/script_api.h"
#include "rpg/world/coords.h"
#include "rpg/world/source.h"

namespace {

std::string low(std::string s) {
  for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return s;
}
int32_t floorDivI(int32_t a, int32_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// the family of the culture cell nearest the player whose archetype is `want` (0 when the world has no source)
uint64_t familyOfArchetype(Game& g, int want) {
  if (!g.world.src) return 0;
  const int32_t gx = g.world.ox + (int32_t)(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)(g.pl().p.y / TILE);
  const int32_t ci = floorDivI(gx, ew::CCELL), cj = floorDivI(gy, ew::CCELL);
  for (int r = 0; r <= 12; r++)
    for (int dj = -r; dj <= r; dj++)
      for (int di = -r; di <= r; di++) {
        if (std::max(std::abs(di), std::abs(dj)) != r) continue;
        const cult::Culture& c = g.world.src->atlas().family(ci + di, cj + dj);
        if ((int)c.archetype == want) return c.id;
      }
  return 0;
}

bool cmdArmsKit(ScriptCtx& c) {
  Game& g = c.game;
  std::string who = "here";
  Mat mat = Mat::Steel;
  int alloy = -1, ilvl = 20, rar = 1, wsub = (int)WeaponType::Spear;
  bool shield = true, bowHand = false;
  for (size_t i = 1; i < c.a.size(); i++) {
    const std::string w = low(c.a[i]);
    if (w == "mat" && i + 1 < c.a.size()) {
      const std::string m = low(c.a[++i]);
      mat = m == "leather" ? Mat::Leather : m == "bronze" ? Mat::Bronze : m == "iron" ? Mat::Iron : m == "alloy" ? Mat::Alloy : Mat::Steel;
    } else if (w == "alloy" && i + 1 < c.a.size()) alloy = std::atoi(c.a[++i].c_str());
    else if (w == "rarity" && i + 1 < c.a.size()) rar = std::clamp(std::atoi(c.a[++i].c_str()), 0, 4);
    else if (w == "ilvl" && i + 1 < c.a.size()) ilvl = std::clamp(std::atoi(c.a[++i].c_str()), 1, 60);
    else if (w == "spear") wsub = (int)WeaponType::Spear;
    else if (w == "sword") wsub = (int)WeaponType::Sword;
    else if (w == "greatsword") wsub = (int)WeaponType::Greatsword;
    else if (w == "dagger") wsub = (int)WeaponType::Dagger;
    else if (w == "axe") wsub = (int)WeaponType::Axe;
    else if (w == "mace") wsub = (int)WeaponType::Mace;
    else if (w == "bow") bowHand = true;
    else if (w == "noshield") shield = false;
    else who = w;
  }
  uint64_t cid = 0;
  if (who == "here") {
    if (g.world.src) {
      const int32_t gx = g.world.ox + (int32_t)(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)(g.pl().p.y / TILE);
      cid = g.world.src->cultureAt(gx, gy);
    }
  } else if (who != "none") {
    int want = -1;
    for (int a = 0; a < (int)cult::Archetype::COUNT; a++)
      if (low(cult::archetypeName((cult::Archetype)a)) == who) want = a;
    if (want < 0) { c.fail("armskit: unknown archetype '" + who + "'"); return true; }
    cid = familyOfArchetype(g, want);
    if (!cid) { c.fail("armskit: no " + who + " culture within reach"); return true; }
  }
  const cult::Culture* cu = cid && g.world.src ? &g.world.src->culture(cid) : nullptr;
  if (mat == Mat::Alloy) {
    const int n = cu ? (int)cu->arms.alloys.size() : 0;
    if (!n) mat = Mat::Steel;
    else alloy = alloy < 1 ? n : std::min(alloy, n);
  }
  // take everything off
  for (ItemKind k : {ItemKind::Weapon, ItemKind::Bow, ItemKind::Staff, ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield, ItemKind::Gloves,
                     ItemKind::Boots, ItemKind::Cloak})
    if (int* s = g.equipSlot(k)) *s = -1;
  Rng r(0xA7A5u ^ (uint32_t)cid ^ (uint32_t)(cid >> 32) ^ (uint32_t)mat * 131u ^ (uint32_t)g.inv.size() * 977u);
  auto give = [&](ItemKind k, int sub, Mat m) {
    Item it = gear::makeGear(r, k, sub, ilvl, (Rarity)rar, cid, m, (uint8_t)(m == Mat::Alloy ? alloy : 0));
    if (k == ItemKind::Cloak && cu && cu->arms.cloth) it.tint = cu->arms.cloth | 0xFF000000u;
    it.name = gear::gearName(it, cu);
    g.inv.push_back(it);
    const int idx = (int)g.inv.size() - 1;
    // a legendary is worn as the look test wants it, past the attunement limit (a test command, never a save)
    if (it.rarity == Rarity::Legendary) { if (int* s = g.equipSlot(k)) *s = idx; }
    else g.useItem(idx);
  };
  for (ItemKind k : {ItemKind::Armor, ItemKind::Helmet, ItemKind::Gloves, ItemKind::Boots}) give(k, 0, mat);
  if (shield && !bowHand) give(ItemKind::Shield, 0, mat);
  give(ItemKind::Bow, 0, Mat::Wood);   // in hand when no weapon follows, else on the back
  if (!bowHand) give(ItemKind::Weapon, wsub, mat == Mat::Leather ? Mat::Iron : mat);
  rar = std::min(rar, 3);
  give(ItemKind::Cloak, 0, Mat::Cloth);   // last and never legendary: its useItem recalculates the look
  return true;
}
EMB_SCRIPT_CMD("armskit", "armskit <archetype|here|none> [mat m] [alloy N] [rarity N] [ilvl N] [spear|sword|...|bow] [noshield]: wear a culture's full arms (M6)", cmdArmsKit);

bool expArmsLook(ScriptCtx& c) {
  const art::HumanLook& L = c.game.pl().look;
  if (!L.helmForm || !L.bodyForm) c.fail("expect armslook: no culture helm / body form on the player's look");
  if (L.weapon == 8 && !L.polearmForm) c.fail("expect armslook: the polearm has no form");
  if (L.weapon == 3 && !L.bowForm) c.fail("expect armslook: the bow has no form");
  if (!L.armorStyle || !L.helmStyle) c.fail("expect armslook: the armour bands are missing");
  return true;
}
EMB_SCRIPT_CMD("expect:armslook", "expect armslook: the player's look carries the worn culture arms (M6)", expArmsLook);

bool expArmsIcons(ScriptCtx& c) {
  Game& g = c.game;
  int n = 0;
  for (ItemKind k : {ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield, ItemKind::Weapon}) {
    const int* s = g.equipSlot(k);
    if (!s || *s < 0 || *s >= (int)g.inv.size()) continue;
    const Item& it = g.inv[(size_t)*s];
    if (!it.culture || !g.world.src) continue;
    const art::IconLook l = gearIcon(it, &g.world.src->culture(it.culture));
    const Canvas a = art::itemIconLook(l), b = art::itemIcon(it.icon, it.tint);
    if (a.px == b.px) c.fail("expect armsicons: " + it.name + " paints the classic icon");
    n++;
  }
  if (!n) c.fail("expect armsicons: no culture piece is worn");
  return true;
}
EMB_SCRIPT_CMD("expect:armsicons", "expect armsicons: worn culture pieces paint culture icons (M6)", expArmsIcons);

}  // namespace
