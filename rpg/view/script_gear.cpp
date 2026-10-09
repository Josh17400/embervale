// M6 Steel script commands (lead, phase A): stage gear, materials and elite foes for screenshots and checks. Lanes add
// their own commands in their own script_*.cpp files.
//   gear6 <kind> [ilvl N] [rarity 0-4] [here|none|<archetype>] [mat leather|bronze|iron|steel|alloy] [alloy N] [form N]
//        [equip]: a piece of gear in the pack (kind: sword axe mace dagger greatsword spear bow staff body helmet shield
//        gloves boots cloak ring amulet); `here` = the culture where the player stands, an archetype name = the nearest
//        settlement culture of that archetype within the loaded window's sites, none = heartland (default here)
//   kit6 [here|none|<archetype>] [mat ...] [rarity N] [ilvl N]: equip a full set (body, helmet, shield, gloves, boots,
//        cloak, a spear or with `sword` a sword) in a culture's style: the paper doll / world look check
//   stuff <material words> <n>: a stack of crafting material ("stuff iron ore 5", "stuff bronze ingot 3")
//   elite <monster> [n] [rank elite|champion|named] [affix words...]: spawn n (1) monsters 60 px east with a rank, affixes
//        and the foes::variantFor look set (the FOES lane's own spawn logic may add more)
//   expect gearlook: the player's look shows its worn culture forms (helmForm / bodyForm set when a culture piece is worn)
#include <algorithm>
#include <initializer_list>
#include <cctype>
#include <cstdlib>
#include <string>
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/story/dsl.h"
#include "rpg/view/script_api.h"
#include "rpg/world/source.h"

namespace {

std::string low(std::string s) {
  for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return s;
}

bool kindOf(const std::string& w, ItemKind& k, int& sub) {
  struct K { const char* n; ItemKind k; int sub; };
  static const K t[] = {{"sword", ItemKind::Weapon, 0}, {"axe", ItemKind::Weapon, 1}, {"mace", ItemKind::Weapon, 2}, {"dagger", ItemKind::Weapon, 3},
                        {"greatsword", ItemKind::Weapon, 4}, {"spear", ItemKind::Weapon, 5}, {"bow", ItemKind::Bow, 0},
                        {"staff", ItemKind::Staff, 0}, {"body", ItemKind::Armor, 0}, {"armor", ItemKind::Armor, 0},
                        {"helmet", ItemKind::Helmet, 0}, {"shield", ItemKind::Shield, 0}, {"gloves", ItemKind::Gloves, 0},
                        {"boots", ItemKind::Boots, 0}, {"cloak", ItemKind::Cloak, 0}, {"ring", ItemKind::Ring, 0},
                        {"amulet", ItemKind::Amulet, 0}};
  for (const K& x : t) if (w == x.n) { k = x.k; sub = x.sub; return true; }
  return false;
}

Mat matOf(const std::string& w) {
  if (w == "leather") return Mat::Leather;
  if (w == "bronze") return Mat::Bronze;
  if (w == "iron") return Mat::Iron;
  if (w == "steel") return Mat::Steel;
  if (w == "alloy") return Mat::Alloy;
  return Mat::None;
}

// the culture a script names: here, none, or an archetype (the nearest loaded settlement of it)
uint64_t cultureOfWord(Game& g, const std::string& w) {
  if (w == "none" || !g.world.src) return 0;
  const int32_t gx = g.world.ox + (int32_t)(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)(g.pl().p.y / TILE);
  if (w.empty() || w == "here") return g.world.src->cultureAt(gx, gy);
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++) {
    if (low(cult::archetypeName((cult::Archetype)a)) != w) continue;
    for (size_t si = 0; si < g.world.sites.size(); si++) {
      const cult::Culture* c = g.world.cultureOf((int)si);
      if (c && (int)c->archetype == a) return c->id;
    }
  }
  return 0;
}

struct GearArgs { int ilvl = 8; Rarity rar = Rarity::Common; std::string cult = "here"; Mat mat = Mat::None; int alloy = 0, form = 0; bool equip = false, sword = false; };
GearArgs parseArgs(const ScriptCtx& c, size_t from) {
  GearArgs A;
  for (size_t i = from; i < c.a.size(); i++) {
    const std::string w = low(c.a[i]);
    if (w == "ilvl" && i + 1 < c.a.size()) A.ilvl = std::atoi(c.a[++i].c_str());
    else if (w == "rarity" && i + 1 < c.a.size()) A.rar = (Rarity)std::clamp(std::atoi(c.a[++i].c_str()), 0, 4);
    else if (w == "mat" && i + 1 < c.a.size()) A.mat = matOf(low(c.a[++i]));
    else if (w == "alloy" && i + 1 < c.a.size()) A.alloy = std::atoi(c.a[++i].c_str());
    else if (w == "form" && i + 1 < c.a.size()) A.form = std::atoi(c.a[++i].c_str());
    else if (w == "equip") A.equip = true;
    else if (w == "sword") A.sword = true;
    else A.cult = w;
  }
  if (A.mat == Mat::Alloy && A.alloy == 0) A.alloy = 1;
  return A;
}

void giveGear(Game& g, ItemKind k, int sub, const GearArgs& A) {
  Rng r(0x6EA4u ^ (uint32_t)g.inv.size() * 977u ^ (uint32_t)k * 131u);
  const uint64_t cid = cultureOfWord(g, A.cult);
  // (NUMBERS, phase B) made with the culture in hand: its forms, alloys and words
  const cult::Culture* cc = cid && g.world.src ? &g.world.src->culture(cid) : nullptr;
  Item it = gear::makeGearC(r, k, sub, A.ilvl, A.rar, cc, A.mat, (uint8_t)A.alloy);
  if (A.form) { it.form = (uint8_t)A.form; it.name = gear::gearName(it, cc); }
  g.inv.push_back(it);
  if (A.equip) {
    // equip the new piece in place of whatever holds that slot (useItem swaps it; a legendary refused by attunement
    // leaves the old piece worn, so the look and the stats never go stale)
    g.useItem((int)g.inv.size() - 1);
  }
}

bool cmdGear(ScriptCtx& c) {
  ItemKind k;
  int sub = 0;
  if (!kindOf(low(c.arg(1)), k, sub)) { c.fail("gear6: unknown kind '" + c.arg(1) + "'"); return true; }
  giveGear(c.game, k, sub, parseArgs(c, 2));
  return true;
}
EMB_SCRIPT_CMD("gear6", "gear6 <kind> [ilvl N] [rarity 0-4] [here|none|<archetype>] [mat m] [alloy N] [form N] [equip]: a piece of gear (M6)", cmdGear);

bool cmdKit6(ScriptCtx& c) {
  GearArgs A = parseArgs(c, 1);
  A.equip = true;
  for (ItemKind k : {ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield, ItemKind::Gloves, ItemKind::Boots, ItemKind::Cloak}) giveGear(c.game, k, 0, A);
  giveGear(c.game, ItemKind::Weapon, A.sword ? (int)WeaponType::Sword : (int)WeaponType::Spear, A);
  return true;
}
EMB_SCRIPT_CMD("kit6", "kit6 [here|none|<archetype>] [mat m] [rarity N] [ilvl N] [sword]: equip a full culture set (M6)", cmdKit6);

bool cmdStuff(ScriptCtx& c) {
  if (c.a.size() < 3) { c.fail("stuff <material words> <n>"); return true; }
  const int n = std::max(1, std::atoi(c.a.back().c_str()));
  std::string want;
  for (size_t i = 1; i + 1 < c.a.size(); i++) { if (i > 1) want += ' '; want += c.a[i]; }
  want = low(want);
  for (int s = 0; s < (int)craft::Stuff::COUNT; s++)
    if (low(craft::stuffInfo((craft::Stuff)s).name) == want) { c.game.inv.push_back(craft::makeStuff((craft::Stuff)s, n)); return true; }
  c.fail("stuff: unknown material '" + want + "'");
  return true;
}
EMB_SCRIPT_CMD("stuff", "stuff <material words> <n>: a stack of crafting material, e.g. stuff iron ore 5 (M6)", cmdStuff);

bool cmdElite(ScriptCtx& c) {
  const int m = story::dsl::monsterWord(c.arg(1));
  if (m < 0 || m >= (int)art::Monster::COUNT) { c.fail("elite: unknown monster '" + c.arg(1) + "'"); return true; }
  int n = 1;
  foes::Rank rank = foes::Rank::Elite;
  uint16_t aff = 0;
  for (size_t i = 2; i < c.a.size(); i++) {
    const std::string w = low(c.a[i]);
    if (!w.empty() && std::isdigit((unsigned char)w[0])) { n = std::clamp(std::atoi(w.c_str()), 1, 12); continue; }
    if (w == "elite") { rank = foes::Rank::Elite; continue; }
    if (w == "champion") { rank = foes::Rank::Champion; continue; }
    if (w == "named") { rank = foes::Rank::Named; continue; }
    for (int f = 0; f < (int)foes::Affix::COUNT; f++)
      if (low(foes::affixInfo((foes::Affix)f).name) == w) aff |= foes::affixBit((foes::Affix)f);
  }
  Game& g = c.game;
  for (int k = 0; k < n; k++) {
    const int id = g.debugSpawnAt((art::Monster)m, g.pl().p + Vec2(60.0f + 18.0f * k, -10.0f + 14.0f * (k % 3)), std::max(1, g.plLevel));
    for (Actor& a : g.actors)
      if (a.id == id) {
        a.rank = (uint8_t)rank;
        a.affixes |= aff;
        const foes::Variant v = foes::variantFor(a.mon, 0, a.affixes, rank, (uint32_t)id);
        a.overlays |= v.overlays;
        if (v.tint) a.bodyTint = v.tint;
        if (v.scalePct) a.scalePct = v.scalePct;
      }
  }
  return true;
}
EMB_SCRIPT_CMD("elite", "elite <monster> [n] [elite|champion|named] [affix words]: spawn ranked monsters with affixes and their look (M6)", cmdElite);

bool expGearLook(ScriptCtx& c) {
  const art::HumanLook& L = c.game.pl().look;
  if (!L.helmForm && !L.bodyForm && !L.shieldForm) c.fail("expect gearlook: no culture forms on the player's look");
  return true;
}
EMB_SCRIPT_CMD("expect:gearlook", "expect gearlook: the player's look carries the worn culture forms (M6)", expGearLook);

}  // namespace
