// M8b Treasure script commands (lead, phase A; the VIEW lane owns this file after phase A and may add commands): stage
// loot for screenshots and checks (VISION_PLAN 15.27, 15.28). `plevel N` (script_numbers.cpp) sets the hero's level,
// `gear6 <kind> ... rarity 0-5` (script_gear.cpp) puts one exact piece in the pack.
//   lootdrop <kind> [rarity N|common..mythic] [ilvl N] [n K]: K (1) pieces lying on the ground round the player (the
//        loot beam / sparkle and the pickup notice check); kind as gear6 (sword ... amulet)
//   lootroll <n> [level L] [D d] [rank r] [src body|chest|hoard|boss|shop|quest|saga|finale|campaign|forge]: n drops
//        rolled by gear::rollDropC for that source at that player level and danger, laid on the ground round the player
//        (the saturation look: what a level-L hero really finds)
//   expect lootvalid: every equippable item in the pack and on the ground passes gear::lootValid (15.27.2)
//   expect lootpickups <n>: at least n item pickups lie on the ground
// (M8b VIEW lane)
//   lootladder <kind> [ilvl N] [dy D] [from R] [to R]: one piece of each rarity (Common .. Mythic) in a row west to east,
//        D (-60) px north of the player (the beams rise away from him), 24 px apart: the ladder side by side
//   lootstock <kind> [rarity N|name] [ilvl N]: a piece in the open shop's stock (the shop's compare check)
//   lootsel menu <i|last> | shop <buy|sell> <i|last> | doll <slot 0-10> <cand>: the cursor on a row (View::lootSelect)
//   expect lootcard [rarity N|name] [compare] [verdict up|down|even] [affixes N] [lost N] [power] [worn]: what the last
//        item card drew (View::lastCard)
//   expect lootbeams <rarity> <n>: at least n pickups of that rarity were drawn with their ground presence last frame
//   expect lootchimes <n>: the LootChime has played at least n times; expect lootembers: a Mythic wearer's embers rose
//   expect lootframes <n>: at least n rarity frames were drawn last frame
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

namespace {

std::string low(std::string s) {
  for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return s;
}

bool kindWord(const std::string& w, ItemKind& k, int& sub) {
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

int rarityWord(const std::string& w) {
  for (int r = 0; r < RARITIES; r++) if (w == low(rarityName((Rarity)r))) return r;
  return std::clamp(std::atoi(w.c_str()), 0, RARITIES - 1);
}

gear::LootSrc srcWord(const std::string& w) {
  if (w == "chest") return gear::LootSrc::Chest;
  if (w == "hoard") return gear::LootSrc::Hoard;
  if (w == "explore") return gear::LootSrc::Explore;
  if (w == "boss") return gear::LootSrc::Boss;
  if (w == "worldboss") return gear::LootSrc::WorldBoss;
  if (w == "shop") return gear::LootSrc::Shop;
  if (w == "merchant") return gear::LootSrc::Merchant;
  if (w == "quest") return gear::LootSrc::Quest;
  if (w == "saga") return gear::LootSrc::Saga;
  if (w == "finale") return gear::LootSrc::SagaFinale;
  if (w == "campaign") return gear::LootSrc::CampaignFinale;
  if (w == "forge") return gear::LootSrc::Forge;
  if (w == "companion") return gear::LootSrc::Companion;
  return gear::LootSrc::Body;
}

// a spot on rings round the player (k of n; twelve to a ring), outside the 40-px pickup pull so the pieces stay lying
// on the ground for the screenshot (walk into them to pick them up)
Vec2 ringSpot(const Game& g, int k, int n) {
  const int per = std::min(12, std::max(1, n)), ring = k / 12;
  const float a = 6.2831853f * (float)(k % 12) / (float)per - 1.5707963f + 0.26f * (float)ring;
  const float rad = 54.0f + 20.0f * (float)ring;
  return g.pl().p + Vec2(std::cos(a) * rad, std::sin(a) * rad * 0.8f + 4.0f);
}

bool cmdLootDrop(ScriptCtx& c) {
  Game& g = c.game;
  ItemKind k;
  int sub = 0;
  if (!kindWord(low(c.arg(1)), k, sub)) { c.fail("lootdrop: unknown kind '" + c.arg(1) + "'"); return true; }
  int rar = 0, ilvl = std::max(1, g.plLevel), n = 1;
  for (size_t i = 2; i + 1 < c.a.size(); i++) {
    const std::string w = low(c.a[i]);
    if (w == "rarity") rar = rarityWord(low(c.a[++i]));
    else if (w == "ilvl") ilvl = std::clamp(std::atoi(c.a[++i].c_str()), 1, gear::MAX_D);
    else if (w == "n") n = std::clamp(std::atoi(c.a[++i].c_str()), 1, 24);
  }
  Rng r(0x10A7u ^ (uint32_t)g.pickups.size() * 977u ^ (uint32_t)k * 131u ^ (uint32_t)rar * 7u);
  for (int j = 0; j < n; j++) {
    Pickup p;
    p.item = gear::makeGearC(r, k, sub, ilvl, (Rarity)rar, nullptr);
    p.p = ringSpot(g, j, n);
    g.pickups.push_back(p);
  }
  return true;
}
EMB_SCRIPT_CMD("lootdrop", "lootdrop <kind> [rarity N|name] [ilvl N] [n K]: pieces on the ground round the player (M8b)", cmdLootDrop);

bool cmdLootRoll(ScriptCtx& c) {
  Game& g = c.game;
  const int n = std::clamp(std::atoi(c.arg(1).c_str()), 1, 48);
  gear::DropSource s;
  s.playerLevel = std::max(1, g.plLevel);
  s.D = s.playerLevel;
  for (size_t i = 2; i + 1 < c.a.size(); i++) {
    const std::string w = low(c.a[i]);
    if (w == "level") s.playerLevel = std::clamp(std::atoi(c.a[++i].c_str()), 1, gear::LEVEL_CAP);
    else if (w == "d") s.D = std::clamp(std::atoi(c.a[++i].c_str()), 1, gear::MAX_D);
    else if (w == "rank") s.rank = (uint8_t)std::clamp(std::atoi(c.a[++i].c_str()), 0, 5);
    else if (w == "src") s.src = srcWord(low(c.a[++i]));
  }
  Rng r(0x2077u ^ (uint32_t)g.pickups.size() * 31u ^ (uint32_t)s.playerLevel * 977u ^ (uint32_t)s.D * 131u);
  int laid = 0;
  for (int tries = 0; laid < n && tries < n * 12; tries++) {
    const Item it = gear::rollDropC(r, s, nullptr);
    if (!itemEquippable(it.kind)) continue;   // the gear only: potions and goods would hide the spread
    Pickup p;
    p.item = it;
    p.p = ringSpot(g, laid, n);
    g.pickups.push_back(p);
    laid++;
  }
  return true;
}
EMB_SCRIPT_CMD("lootroll", "lootroll <n> [level L] [D d] [rank r] [src s]: n rolled gear drops on the ground (M8b)", cmdLootRoll);

bool expLootValid(ScriptCtx& c) {
  const Game& g = c.game;
  std::string why;
  for (const Item& it : g.inv) if (!gear::lootValid(it, &why)) { c.fail("lootvalid (pack): " + why); return true; }
  for (const Pickup& p : g.pickups) if (!p.gold && !gear::lootValid(p.item, &why)) { c.fail("lootvalid (ground): " + why); return true; }
  return true;
}
EMB_SCRIPT_CMD("expect:lootvalid", "expect lootvalid: every equippable item in the pack and on the ground passes gear::lootValid (M8b)", expLootValid);

bool expLootPickups(ScriptCtx& c) {
  const int want = std::atoi(c.arg(2).c_str());
  int n = 0;
  for (const Pickup& p : c.game.pickups) n += p.gold == 0;
  if (n < want) c.fail("lootpickups: " + std::to_string(n) + " on the ground (" + std::to_string(want) + " wanted)");
  return true;
}
EMB_SCRIPT_CMD("expect:lootpickups", "expect lootpickups <n>: at least n item pickups on the ground (M8b)", expLootPickups);

bool cmdLootLadder(ScriptCtx& c) {
  Game& g = c.game;
  ItemKind k;
  int sub = 0;
  if (!kindWord(low(c.arg(1)), k, sub)) { c.fail("lootladder: unknown kind '" + c.arg(1) + "'"); return true; }
  int ilvl = std::max(1, g.plLevel), lo = 0, hi = RARITIES - 1;
  float dy = -60;
  for (size_t i = 2; i + 1 < c.a.size(); i++) {
    const std::string w = low(c.a[i]);
    if (w == "ilvl") ilvl = std::clamp(std::atoi(c.a[++i].c_str()), 1, gear::MAX_D);
    else if (w == "dy") dy = (float)std::atof(c.a[++i].c_str());
    else if (w == "from") lo = rarityWord(low(c.a[++i]));
    else if (w == "to") hi = rarityWord(low(c.a[++i]));
  }
  const int n = std::max(1, hi - lo + 1);
  for (int r = lo; r <= hi; r++) {
    Rng rr(0x1ADDu ^ (uint32_t)r * 7919u ^ (uint32_t)k * 131u ^ (uint32_t)ilvl * 31u);
    Pickup p;
    p.item = gear::makeGearC(rr, k, sub, ilvl, (Rarity)r, nullptr);
    p.p = g.pl().p + Vec2(((float)(r - lo) - (float)(n - 1) * 0.5f) * 24.0f, dy);
    // (M8b fix) never under a prop or in a wall: step north / south to the nearest open spot (the piece's foot and
    // the icon above it both clear), so a crystal or a stall never hides a rung of the ladder
    for (int s = 0; s < 12; s++) {
      const float oy = (float)((s + 1) / 2) * 8.0f * ((s & 1) ? -1.0f : 1.0f);
      const Vec2 q = p.p + Vec2(0, s ? oy : 0.0f);
      auto clear = [&](float x, float y) {
        const int tx = (int)std::floor(x / TILE), ty = (int)std::floor(y / TILE);
        return g.map().in(tx, ty) && !g.map().blocked(tx, ty) && g.map().propAt(tx, ty) == 0;
      };
      if (clear(q.x, q.y) && clear(q.x, q.y - 10) && clear(q.x - 6, q.y - 4) && clear(q.x + 6, q.y - 4)) { p.p = q; break; }
    }
    g.pickups.push_back(p);
  }
  return true;
}
EMB_SCRIPT_CMD("lootladder", "lootladder <kind> [ilvl N] [dy D] [from R] [to R]: one piece of each rarity in a row north of the player (M8b)", cmdLootLadder);

bool cmdLootStock(ScriptCtx& c) {
  Game& g = c.game;
  if (g.mode != Mode::Shop) { c.fail("lootstock: no shop open"); return true; }
  ItemKind k;
  int sub = 0;
  if (!kindWord(low(c.arg(1)), k, sub)) { c.fail("lootstock: unknown kind '" + c.arg(1) + "'"); return true; }
  int rar = 0, ilvl = std::max(1, g.plLevel);
  for (size_t i = 2; i + 1 < c.a.size(); i++) {
    const std::string w = low(c.a[i]);
    if (w == "rarity") rar = rarityWord(low(c.a[++i]));
    else if (w == "ilvl") ilvl = std::clamp(std::atoi(c.a[++i].c_str()), 1, gear::MAX_D);
  }
  Rng r(0x5700u ^ (uint32_t)g.shop.stock.size() * 977u ^ (uint32_t)k * 131u ^ (uint32_t)rar * 7u);
  Item it = gear::makeGearC(r, k, sub, ilvl, (Rarity)rar, nullptr);
  it.flags |= IF_SHOP;
  g.shop.stock.insert(g.shop.stock.begin(), it);
  return true;
}
EMB_SCRIPT_CMD("lootstock", "lootstock <kind> [rarity N|name] [ilvl N]: a piece at the head of the open shop's stock (M8b)", cmdLootStock);

bool cmdLootSel(ScriptCtx& c) {
  std::string err;
  if (!c.view.lootSelect(c.game, c.a, err)) c.fail(err);
  return true;
}
EMB_SCRIPT_CMD("lootsel", "lootsel menu <i|last> | shop <buy|sell> <i|last> | doll <slot> <cand>: the cursor on a row (M8b)", cmdLootSel);

bool expLootCard(ScriptCtx& c) {
  const View::CardInfo ci = c.view.lastCard();
  if (ci.rarity < 0) { c.fail("lootcard: no item card drawn"); return true; }
  for (size_t i = 2; i < c.a.size(); i++) {
    const std::string w = low(c.a[i]);
    if (w == "rarity" && i + 1 < c.a.size()) {
      const int want = rarityWord(low(c.a[++i]));
      if (ci.rarity != want) c.fail("lootcard: rarity " + std::to_string(ci.rarity) + " (" + std::to_string(want) + " wanted) on " + ci.name);
    } else if (w == "compare") {
      if (!ci.compare) c.fail("lootcard: no compare on " + ci.name);
    } else if (w == "worn") {
      if (!ci.worn) c.fail("lootcard: not shown as worn: " + ci.name);
    } else if (w == "power") {
      if (!ci.power) c.fail("lootcard: no power line on " + ci.name);
    } else if (w == "verdict" && i + 1 < c.a.size()) {
      const std::string v = low(c.a[++i]);
      const int want = v == "up" ? 1 : v == "down" ? -1 : 0;
      if (ci.verdict != want) c.fail("lootcard: verdict " + std::to_string(ci.verdict) + " (" + v + " wanted) on " + ci.name);
    } else if (w == "affixes" && i + 1 < c.a.size()) {
      const int want = std::atoi(c.a[++i].c_str());
      if (ci.affixLines < want) c.fail("lootcard: " + std::to_string(ci.affixLines) + " affix lines (" + std::to_string(want) + " wanted) on " + ci.name);
    } else if (w == "lost" && i + 1 < c.a.size()) {
      const int want = std::atoi(c.a[++i].c_str());
      if (ci.lostLines < want) c.fail("lootcard: " + std::to_string(ci.lostLines) + " lost lines (" + std::to_string(want) + " wanted) on " + ci.name);
    }
  }
  return true;
}
EMB_SCRIPT_CMD("expect:lootcard", "expect lootcard [rarity N] [compare] [verdict up|down|even] [affixes N] [lost N] [power] [worn] (M8b)", expLootCard);

bool expLootBeams(ScriptCtx& c) {
  const int r = rarityWord(low(c.arg(2))), want = std::max(1, std::atoi(c.arg(3).c_str()));
  const int got = c.view.lootStats().beams[std::clamp(r, 0, 5)];
  if (got < want) c.fail("lootbeams: " + std::to_string(got) + " " + rarityName((Rarity)r) + " drawn (" + std::to_string(want) + " wanted)");
  return true;
}
EMB_SCRIPT_CMD("expect:lootbeams", "expect lootbeams <rarity> <n>: pickups of that rarity drawn with their beam last frame (M8b)", expLootBeams);

bool expLootChimes(ScriptCtx& c) {
  const int want = std::atoi(c.arg(2).c_str()), got = c.view.lootStats().chimes;
  if (got < want) c.fail("lootchimes: " + std::to_string(got) + " (" + std::to_string(want) + " wanted)");
  return true;
}
EMB_SCRIPT_CMD("expect:lootchimes", "expect lootchimes <n>: the LootChime played at least n times (M8b)", expLootChimes);

bool expLootEmbers(ScriptCtx& c) {
  if (c.view.lootStats().embers <= 0) c.fail("lootembers: no Mythic wearer's embers rose");
  return true;
}
EMB_SCRIPT_CMD("expect:lootembers", "expect lootembers: a Mythic wearer's embers rose (M8b)", expLootEmbers);

bool expLootFrames(ScriptCtx& c) {
  const int want = std::atoi(c.arg(2).c_str()), got = c.view.lootStats().frames;
  if (got < want) c.fail("lootframes: " + std::to_string(got) + " rarity frames last frame (" + std::to_string(want) + " wanted)");
  return true;
}
EMB_SCRIPT_CMD("expect:lootframes", "expect lootframes <n>: at least n rarity frames drawn last frame (M8b)", expLootFrames);

}  // namespace
