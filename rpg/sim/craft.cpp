// M6 Steel: crafting data, recipes and knowledge (rpg/sim/craft.h). NUMBERS lane (its forge half). Phase A (lead): the
// data layer for real; the NUMBERS lane tunes counts, skills and values and adds what play needs.
#include "rpg/sim/craft.h"
#include <algorithm>
#include <initializer_list>
#include <cstdio>
#include "rpg/culture/culture.h"
#include "rpg/sim/gear.h"
#include "rpg/world/biomes.h"
#include "rpg/world/source.h"

namespace craft {

const StuffInfo& stuffInfo(Stuff s) {
  using art::Icon;
  using ew::Good;
  static const StuffInfo t[] = {
      {"COPPER ORE", Icon::Ore, rgba(206, 120, 70), 6, Good::Ore},
      {"TIN ORE", Icon::Ore, rgba(196, 200, 206), 6, Good::Ore},
      {"IRON ORE", Icon::Ore, rgba(150, 110, 96), 8, Good::Ore},
      {"COAL", Icon::Ore, rgba(60, 58, 66), 4, Good::Ore},
      {"SILVER ORE", Icon::Ore, rgba(220, 226, 236), 20, Good::Ore},
      {"STRANGE ORE", Icon::Ore, rgba(150, 120, 220), 40, Good::Ore},
      {"COPPER INGOT", Icon::Ingot, rgba(212, 128, 76), 16, Good::Ingot},
      {"TIN INGOT", Icon::Ingot, rgba(200, 204, 210), 16, Good::Ingot},
      {"BRONZE INGOT", Icon::Ingot, rgba(196, 146, 70), 26, Good::Ingot},
      {"IRON INGOT", Icon::Ingot, rgba(150, 152, 160), 24, Good::Ingot},
      {"STEEL INGOT", Icon::Ingot, rgba(200, 208, 222), 48, Good::Ingot},
      {"SILVER INGOT", Icon::Ingot, rgba(226, 230, 240), 60, Good::Ingot},
      {"ALLOY INGOT", Icon::Ingot, rgba(170, 200, 240), 120, Good::Ingot},
      {"RAW HIDE", Icon::Pelt, rgba(176, 130, 86), 8, Good::Hides},
      {"LEATHER", Icon::Pelt, rgba(140, 92, 56), 16, Good::Leather},
      {"TIMBER", Icon::Staff, rgba(150, 110, 70), 4, Good::Planks},
      {"CLOTH", Icon::Scroll, rgba(200, 186, 150), 10, Good::Cloth},
      {"REAGENT", Icon::Gem, rgba(170, 120, 255), 60, Good::Ore},
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Stuff::COUNT, "a StuffInfo for every material");
  return t[std::clamp((int)s, 0, (int)Stuff::COUNT - 1)];
}

Item makeStuff(Stuff s, int count, uint64_t culture, uint8_t alloy, const cult::Culture* c) {
  const StuffInfo& I = stuffInfo(s);
  Item it;
  it.kind = ItemKind::Material;
  it.sub = (uint8_t)s;
  it.count = std::max(1, count);
  it.value = I.value;
  it.icon = I.icon;
  it.tint = I.tint;
  it.name = I.name;
  if (s == Stuff::AlloyIngot || s == Stuff::Reagent) {
    it.culture = culture;
    it.alloy = alloy;
    it.mat = s == Stuff::AlloyIngot ? Mat::Alloy : Mat::None;
    if (c && alloy >= 1 && alloy <= c->arms.alloys.size()) {
      const cult::Alloy& A = c->arms.alloys[(size_t)alloy - 1];
      if (s == Stuff::AlloyIngot) {
        it.name = A.name + " INGOT";
        it.tint = A.color | 0xFF000000u;
        it.value = I.value * (1 + A.tierStep);
      } else {
        for (const cult::Ingredient& g : A.recipe)
          if (g.ore < 0 && !g.reagent.empty()) { it.name = g.reagent; break; }
      }
    }
  } else if (s == Stuff::BronzeIngot) it.mat = Mat::Bronze;
  else if (s == Stuff::IronIngot) it.mat = Mat::Iron;
  else if (s == Stuff::SteelIngot) it.mat = Mat::Steel;
  else if (s == Stuff::Leather) it.mat = Mat::Leather;
  return it;
}

Stuff oreStuff(ew::Ore o) { return (Stuff)std::clamp((int)o, 0, (int)Stuff::RareOre); }
Stuff ingotOf(ew::Ore o) {
  switch (o) {
    case ew::Ore::Copper: return Stuff::CopperIngot;
    case ew::Ore::Tin: return Stuff::TinIngot;
    case ew::Ore::Iron: return Stuff::IronIngot;
    case ew::Ore::Silver: return Stuff::SilverIngot;
    case ew::Ore::Rare: return Stuff::AlloyIngot;
    default: return Stuff::COUNT;   // coal is fuel
  }
}

std::vector<OreSource> oresAt(ew::EndlessSource& src, int32_t gx, int32_t gy) {
  const ew::Geology g = src.geology(gx, gy);
  const uint8_t bias = ecoInfo(src.ecoAt(gx, gy)).ores;
  std::vector<OreSource> v;
  for (int i = 0; i < (int)ew::Ore::COUNT; i++) {
    int r = g.ore[i] + ((bias >> i) & 1 ? 60 : 0);
    if (r < 96) continue;
    v.push_back({(ew::Ore)i, (uint8_t)std::min(255, r)});
  }
  std::stable_sort(v.begin(), v.end(), [](const OreSource& a, const OreSource& b) { return a.richness > b.richness; });
  return v;
}

namespace {
struct Piece { ItemKind kind; uint8_t sub; uint8_t metal; uint8_t extra; Stuff extraStuff; const char* key; };
// metal: ingots (or leather) of the piece's material; extra: timber / leather / cloth beside it
const Piece kMetalPieces[] = {
    {ItemKind::Weapon, (uint8_t)WeaponType::Sword, 2, 1, Stuff::Leather, "sword"},
    {ItemKind::Weapon, (uint8_t)WeaponType::Axe, 2, 1, Stuff::Timber, "axe"},
    {ItemKind::Weapon, (uint8_t)WeaponType::Mace, 3, 1, Stuff::Timber, "mace"},
    {ItemKind::Weapon, (uint8_t)WeaponType::Dagger, 1, 1, Stuff::Leather, "dagger"},
    {ItemKind::Weapon, (uint8_t)WeaponType::Greatsword, 4, 1, Stuff::Leather, "greatsword"},
    {ItemKind::Weapon, (uint8_t)WeaponType::Spear, 1, 2, Stuff::Timber, "spear"},
    {ItemKind::Armor, 0, 5, 1, Stuff::Leather, "body"},
    {ItemKind::Helmet, 0, 2, 1, Stuff::Leather, "helmet"},
    {ItemKind::Shield, 0, 2, 2, Stuff::Timber, "shield"},
    {ItemKind::Gloves, 0, 1, 1, Stuff::Leather, "gloves"},
    {ItemKind::Boots, 0, 1, 1, Stuff::Leather, "boots"},
};
const Piece kLeatherPieces[] = {
    {ItemKind::Armor, 0, 4, 1, Stuff::Cloth, "body"},
    {ItemKind::Helmet, 0, 1, 0, Stuff::COUNT, "helmet"},
    {ItemKind::Gloves, 0, 1, 0, Stuff::COUNT, "gloves"},
    {ItemKind::Boots, 0, 2, 0, Stuff::COUNT, "boots"},
    {ItemKind::Shield, 0, 1, 2, Stuff::Timber, "shield"},
};
bool isWeaponKind(ItemKind k) { return k == ItemKind::Weapon || k == ItemKind::Bow; }
Recipe pieceRecipe(const Piece& p, Mat mat, Stuff metal, const std::string& id, uint8_t skill) {
  Recipe r;
  r.id = id;
  r.at = mat == Mat::Leather ? art::Building::Tanner : art::Building::Smithy;
  r.in.push_back({metal, p.metal});
  if (p.extra && p.extraStuff != Stuff::COUNT) r.in.push_back({p.extraStuff, p.extra});
  r.kind = p.kind;
  r.sub = p.sub;
  r.mat = mat;
  r.skill = skill;
  return r;
}
std::string hex64(uint64_t v) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", (unsigned long long)v);
  return b;
}
}  // namespace

const std::vector<Recipe>& baseRecipes() {
  static const std::vector<Recipe> R = [] {
    std::vector<Recipe> v;
    auto mat = [&](const char* id, art::Building at, std::vector<Need> in, Stuff out, int n, int skill) {
      Recipe r;
      r.id = id; r.at = at; r.in = std::move(in); r.out = out; r.outCount = (uint8_t)n; r.skill = (uint8_t)skill;
      v.push_back(r);
    };
    // smelting (the smelter: ew::Recipe Ore -> Ingot) and alloying
    mat("smelt.copper", art::Building::Smelter, {{Stuff::CopperOre, 2}}, Stuff::CopperIngot, 1, 0);
    mat("smelt.tin", art::Building::Smelter, {{Stuff::TinOre, 2}}, Stuff::TinIngot, 1, 0);
    mat("smelt.bronze", art::Building::Smelter, {{Stuff::CopperIngot, 3}, {Stuff::TinIngot, 1}}, Stuff::BronzeIngot, 3, 0);
    mat("smelt.iron", art::Building::Smelter, {{Stuff::IronOre, 2}, {Stuff::Coal, 1}}, Stuff::IronIngot, 1, 8);
    mat("smelt.steel", art::Building::Smelter, {{Stuff::IronIngot, 2}, {Stuff::Coal, 2}}, Stuff::SteelIngot, 1, 25);
    mat("smelt.silver", art::Building::Smelter, {{Stuff::SilverOre, 2}}, Stuff::SilverIngot, 1, 15);
    // the tanner (ew::Recipe Hides -> Leather)
    mat("tan.leather", art::Building::Tanner, {{Stuff::Hide, 2}}, Stuff::Leather, 1, 0);
    // gear in the universal tiers
    for (const Piece& p : kLeatherPieces) v.push_back(pieceRecipe(p, Mat::Leather, Stuff::Leather, std::string("forge.leather.") + p.key, 0));
    struct T { Mat m; Stuff s; const char* k; int skill; };
    for (const T& t : {T{Mat::Bronze, Stuff::BronzeIngot, "bronze", 0}, T{Mat::Iron, Stuff::IronIngot, "iron", 12}, T{Mat::Steel, Stuff::SteelIngot, "steel", 30}})
      for (const Piece& p : kMetalPieces) v.push_back(pieceRecipe(p, t.m, t.s, std::string("forge.") + t.k + "." + p.key, (uint8_t)t.skill));
    // the bowyer (at the sawmill) and the weaver
    Recipe bow;
    bow.id = "craft.bow"; bow.at = art::Building::Sawmill; bow.in = {{Stuff::Timber, 3}, {Stuff::Cloth, 1}}; bow.kind = ItemKind::Bow;
    bow.mat = Mat::Wood; bow.skill = 5;
    v.push_back(bow);
    Recipe cloak;
    cloak.id = "craft.cloak"; cloak.at = art::Building::Weaver; cloak.in = {{Stuff::Cloth, 3}}; cloak.kind = ItemKind::Cloak;
    cloak.mat = Mat::Cloth; cloak.skill = 0;
    v.push_back(cloak);
    return v;
  }();
  return R;
}

std::vector<Recipe> cultureRecipes(const cult::Culture& c) {
  std::vector<Recipe> v;
  if (!c.id) return v;   // no culture (a gallery's stand-in): nothing of its own
  const std::string ck = hex64(c.id);
  // its patterns in the universal metals (needs its armour / weapon pattern secret: Recipe::culture set, alloy 0)
  struct T { Mat m; Stuff s; const char* k; int skill; };
  for (const T& t : {T{Mat::Bronze, Stuff::BronzeIngot, "bronze", 15}, T{Mat::Iron, Stuff::IronIngot, "iron", 30}, T{Mat::Steel, Stuff::SteelIngot, "steel", 50}})
    for (const Piece& p : kMetalPieces) {
      Recipe r = pieceRecipe(p, t.m, t.s, "forge." + ck + "." + t.k + "." + p.key, (uint8_t)t.skill);
      r.culture = c.id;
      v.push_back(r);
    }
  // its alloys: smelting each, then every piece in it
  for (size_t i = 0; i < c.arms.alloys.size(); i++) {
    const cult::Alloy& A = c.arms.alloys[i];
    const uint8_t ai = (uint8_t)(i + 1);
    Recipe s;
    s.id = "smelt." + ck + "." + std::to_string(ai);
    s.at = art::Building::Smelter;
    for (const cult::Ingredient& g : A.recipe) {
      if (g.ore >= 0 && g.ore < (int)ew::Ore::COUNT) s.in.push_back({oreStuff((ew::Ore)g.ore), (uint8_t)std::max(1, (int)g.parts)});
      else s.in.push_back({Stuff::Reagent, (uint8_t)std::max(1, (int)g.parts), c.id, ai});
    }
    s.in.push_back({Stuff::Coal, 2});
    s.out = Stuff::AlloyIngot; s.outCount = 1;
    s.culture = c.id; s.alloy = ai;
    s.skill = (uint8_t)std::min(100, 40 + 15 * (int)A.tierStep);
    v.push_back(s);
    for (const Piece& p : kMetalPieces) {
      Recipe r = pieceRecipe(p, Mat::Alloy, Stuff::AlloyIngot, "forge." + ck + ".alloy" + std::to_string(ai) + "." + p.key,
                             (uint8_t)std::min(100, 50 + 12 * (int)A.tierStep));
      r.in[0].culture = c.id; r.in[0].alloy = ai;
      r.culture = c.id; r.alloy = ai;
      v.push_back(r);
    }
  }
  return v;
}

uint64_t secretKey(uint64_t culture) { return cult::familyOf(culture); }

bool Knowledge::knows(uint64_t culture, uint8_t alloy, SecretKind k) const { return progress(culture, alloy, k) >= 100; }
int Knowledge::progress(uint64_t culture, uint8_t alloy, SecretKind k) const {
  const uint64_t key = secretKey(culture);
  for (const Secret& s : secrets)
    if (s.culture == key && s.alloy == alloy && s.kind == k) return s.progress;
  return 0;
}
bool Knowledge::advance(uint64_t culture, uint8_t alloy, SecretKind k, int amount, uint8_t how) {
  const uint64_t key = secretKey(culture);
  for (Secret& s : secrets)
    if (s.culture == key && s.alloy == alloy && s.kind == k) {
      const bool was = s.progress >= 100;
      s.progress = (uint8_t)std::clamp((int)s.progress + amount, 0, 100);
      s.how |= how;
      return !was && s.progress >= 100;
    }
  Secret s;
  s.culture = key; s.alloy = alloy; s.kind = k; s.progress = (uint8_t)std::clamp(amount, 0, 100); s.how = how;
  secrets.push_back(s);
  return s.progress >= 100;
}

// craft block v1: version u8 (1), skill u16, secrets u32 n x (culture u64, alloy u8, kind u8, progress u8, how u8),
// trust u32 n x (npc key u64, trust u8)
// craft block v2 (M6 fixer r4) appends the live caps a load must not reset: purse u32 n x (key u64, period i32, gold i32),
// trustBuy u32 n x (key u64, period i32, gained i32), commissionAt u32 n x (key u64, period i32). v1 still loads.
void Knowledge::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(2);
  w.u16(skill);
  w.u32((uint32_t)secrets.size());
  for (const Secret& s : secrets) { w.u64(s.culture); w.u8(s.alloy); w.u8((uint8_t)s.kind); w.u8(s.progress); w.u8(s.how); }
  w.u32((uint32_t)trust.size());
  for (const auto& kv : trust) { w.u64(kv.first); w.u8(kv.second); }
  w.u32((uint32_t)live.purse.size());
  for (const auto& kv : live.purse) { w.u64(kv.first); w.u32((uint32_t)kv.second.first); w.u32((uint32_t)kv.second.second); }
  w.u32((uint32_t)live.trustBuy.size());
  for (const auto& kv : live.trustBuy) { w.u64(kv.first); w.u32((uint32_t)kv.second.first); w.u32((uint32_t)kv.second.second); }
  w.u32((uint32_t)live.commissionAt.size());
  for (const auto& kv : live.commissionAt) { w.u64(kv.first); w.u32((uint32_t)kv.second); }
}
bool Knowledge::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  const uint8_t ver = r.u8();
  if ((ver != 1 && ver != 2) || r.bad) return false;
  Knowledge k;
  k.skill = std::min<uint16_t>(r.u16(), 1000);
  uint32_t n = r.u32();
  if (r.bad || n > 100000) return false;
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    Secret s;
    s.culture = r.u64(); s.alloy = r.u8(); s.kind = (SecretKind)r.u8(); s.progress = std::min<uint8_t>(r.u8(), 100); s.how = r.u8();
    if ((int)s.kind >= (int)SecretKind::COUNT) continue;
    k.secrets.push_back(s);
  }
  n = r.u32();
  if (r.bad || n > 100000) return false;
  for (uint32_t i = 0; i < n && !r.bad; i++) { const uint64_t key = r.u64(); k.trust[key] = std::min<uint8_t>(r.u8(), 100); }
  if (r.bad) return false;
  // the timers are never saved (a load starts them ready); the purses and trust caps are, from v2
  k.live = gear::Live();
  if (ver >= 2) {
    n = r.u32();
    if (r.bad || n > 100000) return false;
    for (uint32_t i = 0; i < n && !r.bad; i++) { const uint64_t key = r.u64(); const int a = (int)r.u32(); const int b = (int)r.u32(); k.live.purse[key] = {a, b}; }
    n = r.u32();
    if (r.bad || n > 100000) return false;
    for (uint32_t i = 0; i < n && !r.bad; i++) { const uint64_t key = r.u64(); const int a = (int)r.u32(); const int b = (int)r.u32(); k.live.trustBuy[key] = {a, b}; }
    n = r.u32();
    if (r.bad || n > 100000) return false;
    for (uint32_t i = 0; i < n && !r.bad; i++) { const uint64_t key = r.u64(); k.live.commissionAt[key] = (int)r.u32(); }
    if (r.bad) return false;
  }
  *this = k;
  return true;
}

namespace {
bool matches(const Item& it, const Need& n) {
  if (it.kind != ItemKind::Material || it.sub != (uint8_t)n.stuff || it.count <= 0) return false;
  if (n.stuff == Stuff::AlloyIngot || n.stuff == Stuff::Reagent) return it.culture == n.culture && it.alloy == n.alloy;
  return true;
}
int have(const std::vector<Item>& inv, const Need& n) {
  int c = 0;
  for (const Item& it : inv) if (matches(it, n)) c += it.count;
  return c;
}
SecretKind secretFor(const Recipe& r) {
  if (!r.gear()) return SecretKind::AlloyRecipe;
  return isWeaponKind(r.kind) ? SecretKind::WeaponPattern : SecretKind::ArmourPattern;
}
}  // namespace

bool canMake(const Recipe& r, const std::vector<Item>& inv, const Knowledge& k, std::string* why) {
  if (k.skillLevel() < r.skill) { if (why) *why = "NEEDS SMITHING " + std::to_string(r.skill); return false; }
  if (r.culture) {
    // a culture's recipe: its pattern secret (gear) and, for an alloy, the alloy's recipe secret
    if (r.gear() && !k.knows(r.culture, 0, secretFor(r))) { if (why) *why = "AN UNKNOWN PATTERN"; return false; }
    if (r.alloy && !k.knows(r.culture, r.alloy, SecretKind::AlloyRecipe)) { if (why) *why = "AN UNKNOWN ALLOY"; return false; }
  }
  for (const Need& n : r.in)
    if (have(inv, n) < n.count) { if (why) *why = std::string("NEEDS ") + std::to_string(n.count) + " " + stuffInfo(n.stuff).name; return false; }
  return true;
}

bool make(const Recipe& r, std::vector<Item>& inv, Knowledge& k, Rng& rng, int ilvl, const cult::Culture* c, Item& out) {
  if (!canMake(r, inv, k, nullptr)) return false;
  // consume (stacks that reach 0 stay with count 0: the caller removes them and fixes its equipment indices)
  for (const Need& n : r.in) {
    int left = n.count;
    for (Item& it : inv) {
      if (left <= 0) break;
      if (!matches(it, n)) continue;
      const int take = std::min(left, it.count);
      it.count -= take;
      left -= take;
    }
  }
  if (!r.gear()) {
    out = makeStuff(r.out, r.outCount, r.culture, r.alloy, c);
  } else {
    // rarity from the smith's skill above the recipe's: a master's work is often fine
    const int over = std::max(0, k.skillLevel() - r.skill);
    const float q = rng.f();
    Rarity rar = Rarity::Common;
    if (q < 0.02f + over * 0.002f) rar = Rarity::Rare;
    else if (q < 0.15f + over * 0.006f) rar = Rarity::Uncommon;
    // (NUMBERS) the item level is the place's danger, capped by what the material can reach (bronze band 1, iron 2...)
    const int cap = gear::bandHi(gear::matBandCap(r.mat, c, r.alloy));
    out = gear::makeGearC(rng, r.kind, r.sub, std::clamp(ilvl, 1, cap), rar, c && c->id == r.culture ? c : nullptr, r.mat, r.alloy);
    if (r.culture) out.culture = r.culture;
    out.flags |= IF_CRAFTED;
  }
  // practice makes the smith (tenths of a skill point; harder work teaches more: a point a piece at first, then
  // more for the hard pieces; NUMBERS tuning: smithing 30 (steel) after about forty iron-and-bronze makes)
  k.skill = (uint16_t)std::min(1000, (int)k.skill + 8 + (r.gear() ? 4 : 0) + r.skill / 5);
  return true;
}

// ===================================================================================================== NUMBERS lane (phase B)
bool isStation(art::Building b) {
  return b == art::Building::Smithy || b == art::Building::Smelter || b == art::Building::Tanner || b == art::Building::Weaver ||
         b == art::Building::Sawmill;
}
const char* stationVerb(art::Building b) {
  switch (b) {
    case art::Building::Smelter: return "WORK THE SMELTER";
    case art::Building::Tanner: return "WORK THE TANNERY";
    case art::Building::Weaver: return "WORK THE LOOM";
    case art::Building::Sawmill: return "WORK THE BOWYER'S BENCH";
    default: return "WORK THE FORGE";
  }
}
const char* stationName(art::Building b) {
  switch (b) {
    case art::Building::Smelter: return "SMELTER";
    case art::Building::Tanner: return "TANNERY";
    case art::Building::Weaver: return "LOOM";
    case art::Building::Sawmill: return "BOWYER'S BENCH";
    default: return "FORGE";
  }
}

SecretKind secretOf(const Recipe& r) { return secretFor(r); }

std::string recipeName(const Recipe& r, const cult::Culture* c) {
  const cult::Culture* rc = c && c->id == r.culture ? c : nullptr;
  if (!r.gear()) {
    if (r.out == Stuff::AlloyIngot && rc && r.alloy >= 1 && r.alloy <= rc->arms.alloys.size())
      return rc->arms.alloys[(size_t)r.alloy - 1].name + " INGOT" + (r.outCount > 1 ? " x" + std::to_string(r.outCount) : std::string());
    return std::string(stuffInfo(r.out).name) + (r.outCount > 1 ? " x" + std::to_string(r.outCount) : std::string());
  }
  Item t;
  t.kind = r.kind; t.sub = r.sub; t.mat = r.mat; t.alloy = r.alloy; t.ilvl = 1;
  if (rc) {
    // the culture's pattern: its form for the piece
    Rng tr(1);
    Item f = gear::makeGearC(tr, r.kind, r.sub, 1, Rarity::Common, rc, r.mat == Mat::None ? Mat::Bronze : r.mat, r.alloy);
    t.form = f.form;
  }
  std::string w = r.mat == Mat::Wood || r.mat == Mat::Cloth ? std::string() : gear::matWord(t, rc);
  std::string n = (w.empty() ? std::string() : w + " ") + gear::pieceNoun(t);
  if (r.kind == ItemKind::Bow) n = "HUNTING BOW";
  if (r.kind == ItemKind::Cloak) n = "TRAVEL CLOAK";
  if (rc && r.alloy == 0) n = rc->adjective + " " + n;
  return n;
}

std::vector<Recipe> benchRecipes(art::Building at, const Knowledge& k, const cult::Culture* c, const std::vector<Item>& inv) {
  std::vector<Recipe> v;
  for (const Recipe& r : baseRecipes()) if (r.at == at) v.push_back(r);
  if (c && c->id) {
    // the station culture's own: the patterns (alloy 0) once their secret is begun, its alloys once theirs is
    for (const Recipe& r : cultureRecipes(*c)) {
      if (r.at != at) continue;
      const SecretKind sk = secretFor(r);
      bool show = r.gear() ? k.progress(r.culture, 0, sk) > 0 : false;
      if (r.alloy) show = k.progress(r.culture, r.alloy, SecretKind::AlloyRecipe) > 0 && (!r.gear() || k.progress(r.culture, 0, sk) > 0);
      if (show) v.push_back(r);
    }
  }
  // makeable first, then by skill (a stable order the screen keeps)
  std::stable_sort(v.begin(), v.end(), [&](const Recipe& a, const Recipe& b) {
    const bool ma = canMake(a, inv, k, nullptr), mb = canMake(b, inv, k, nullptr);
    if (ma != mb) return ma;
    return a.skill < b.skill;
  });
  return v;
}

bool howAllowed(const cult::Culture& c, uint8_t alloy, SecretKind kind, uint8_t how) {
  if (how & HOW_QUEST) return true;
  if (kind != SecretKind::AlloyRecipe || alloy == 0) return true;   // patterns: any honest (or dishonest) way
  if (alloy > c.arms.alloys.size()) return false;
  return (c.arms.alloys[(size_t)alloy - 1].learn & how & 15) != 0;
}
bool learnSecret(Knowledge& k, const cult::Culture& c, uint8_t alloy, SecretKind kind, int amount, uint8_t how) {
  if (!c.id || !howAllowed(c, alloy, kind, how)) return false;
  return k.advance(c.id, alloy, kind, amount, how);
}
std::string secretName(const cult::Culture& c, uint8_t alloy, SecretKind kind) {
  if (kind == SecretKind::AlloyRecipe && alloy >= 1 && alloy <= c.arms.alloys.size()) return "THE SECRET OF " + c.arms.alloys[(size_t)alloy - 1].name;
  return "THE " + c.adjective + (kind == SecretKind::WeaponPattern ? " WEAPON PATTERN" : " ARMOUR PATTERN");
}

Item makeLore(const cult::Culture& c, uint8_t alloy, SecretKind kind, uint32_t seed) {
  Item it;
  it.kind = ItemKind::Misc;
  it.sub = MISC_LORE;
  it.culture = c.id;
  it.alloy = alloy;
  it.form = (uint8_t)kind;
  it.seed = seed;
  it.rarity = Rarity::Rare;
  it.icon = kind == SecretKind::AlloyRecipe ? art::Icon::Scroll : art::Icon::Ingot;
  it.tint = alloy >= 1 && alloy <= c.arms.alloys.size() ? (c.arms.alloys[(size_t)alloy - 1].color | 0xFF000000u) : rgba(170, 150, 110);
  if (kind == SecretKind::AlloyRecipe && alloy >= 1 && alloy <= c.arms.alloys.size())
    it.name = "ALLOY NOTES: " + c.arms.alloys[(size_t)alloy - 1].name;
  else it.name = c.adjective + (kind == SecretKind::WeaponPattern ? " BLADE MOULD" : " ARMOUR MOULD");
  it.value = 60;
  return it;
}

}  // namespace craft
