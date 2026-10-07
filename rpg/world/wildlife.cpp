// M3c Wildlands: the biomes' wildlife (rpg/world/wildlife.h). LIFE lane (the den table: frozen, see the header).
#include "rpg/world/wildlife.h"

namespace ew {

using art::Monster;

namespace {

// three species with cumulative odds in 1/100: a below pa, b below pb, else c
struct Trio { Monster a; uint8_t pa; Monster b; uint8_t pb; Monster c; };
constexpr Monster None = Monster::COUNT;

const Trio& denTrio(Eco e) {
  static const Trio T[] = {
      {None, 0, None, 0, None},                                           // Ocean
      {None, 0, None, 0, None},                                           // SandBeach
      {None, 0, None, 0, None},                                           // Shingle
      {None, 0, None, 0, None},                                           // CoralCoast
      {None, 0, None, 0, None},                                           // SeaCliffs
      {Monster::Wolf, 50, Monster::Goblin, 85, Monster::Skeleton},        // Meadow (the classic plains)
      {Monster::Wolf, 50, Monster::Goblin, 85, Monster::Skeleton},        // FlowerMeadow
      {Monster::Wolf, 55, Monster::Goblin, 85, Monster::Boar},            // Prairie
      {Monster::Wolf, 50, Monster::Hyena, 80, Monster::Goblin},           // Steppe
      {Monster::Hyena, 60, Monster::Goblin, 85, Monster::Scorpion},       // Savanna
      {Monster::Wolf, 50, Monster::Skeleton, 75, Monster::Goblin},        // Heath
      {Monster::Wolf, 50, Monster::Goblin, 85, Monster::Skeleton},        // ChalkDowns
      {Monster::Wolf, 50, Monster::Troll, 75, Monster::Bear},             // AlpineMeadow
      {Monster::Wraith, 40, Monster::Skeleton, 80, Monster::Wolf},        // StonePlains
      {Monster::Wolf, 45, Monster::Spider, 75, Monster::Bear},            // MixedForest (the classic forest)
      {Monster::Wolf, 55, Monster::Spider, 75, Monster::Bear},            // BirchWood
      {Monster::Bear, 40, Monster::Spider, 75, Monster::Troll},           // GiantForest
      {Monster::Spider, 40, Monster::Blightspawn, 65, Monster::Wraith},   // DarkForest
      {Monster::Wolf, 50, Monster::Goblin, 80, Monster::Boar},            // BlossomGrove
      {Monster::Spider, 40, Monster::Goblin, 70, Monster::Boar},          // BambooForest
      {Monster::Lurker, 30, Monster::Spider, 70, Monster::Goblin},        // Jungle
      {Monster::Wisp, 45, Monster::Spider, 75, Monster::Slime},           // MushroomForest
      {Monster::Wisp, 45, Monster::Wolf, 75, Monster::Spider},            // Silverwood
      {Monster::Goblin, 40, Monster::Spider, 70, Monster::Bear},          // AutumnWood (the classic autumn)
      {Monster::Wolf, 50, Monster::Bear, 80, Monster::Troll},             // Taiga (the classic taiga)
      {Monster::Wolf, 40, Monster::Slime, 70, Monster::Troll},            // TaigaBog
      {Monster::IceWolf, 55, Monster::FrostSpider, 80, Monster::Troll},   // SnowField (the classic snow)
      {Monster::IceWolf, 60, Monster::Troll, 85, Monster::FrostSpider},   // Tundra
      {Monster::Yeti, 50, Monster::IceWolf, 80, Monster::FrostSpider},    // Glacier
      {Monster::IceWolf, 60, Monster::Yeti, 80, Monster::FrostSpider},    // FrozenLakes
      {Monster::Spider, 60, Monster::Skeleton, 100, Monster::Skeleton},   // ReedMarsh (the classic swamp)
      {Monster::Skeleton, 40, Monster::Wisp, 70, Monster::Slime},         // PeatBog
      {Monster::Lurker, 50, Monster::Mudcrab, 80, Monster::Spider},       // Mangrove
      {Monster::Lurker, 40, Monster::Spider, 75, Monster::Slime},         // FloodedForest
      {Monster::Wolf, 50, Monster::Goblin, 85, Monster::Lurker},          // LakeDistrict
      {Monster::Scorpion, 40, Monster::Sandworm, 70, Monster::Goblin},    // Dunes
      {Monster::Scorpion, 40, Monster::Goblin, 75, Monster::Skeleton},    // StonyDesert
      {Monster::Goblin, 45, Monster::Scorpion, 75, Monster::Skeleton},    // Badlands
      {Monster::Scorpion, 50, Monster::Skeleton, 100, Monster::Skeleton}, // SaltFlats
      {Monster::Hyena, 40, Monster::Scorpion, 70, Monster::Goblin},       // Scrubland
      {Monster::Goblin, 50, Monster::Scorpion, 80, Monster::Lurker},      // Oasis
      {None, 0, None, 0, None},                                           // Mountain
      {Monster::EmberHound, 60, Monster::Skeleton, 85, Monster::Troll},   // AshFields
      {Monster::Wisp, 50, Monster::FrostSpider, 80, Monster::Slime},      // CrystalBarrens
      {Monster::Scorpion, 40, Monster::Skeleton, 75, Monster::Wraith},    // PetrifiedForest
      {Monster::Blightspawn, 60, Monster::Skeleton, 85, Monster::Wraith}, // Blight
  };
  static_assert(sizeof(T) / sizeof(T[0]) == (size_t)Eco::COUNT, "a den trio for every eco");
  return T[(int)e < (int)Eco::COUNT ? (int)e : (int)Eco::Meadow];
}

Monster pick(const Trio& t, int p100) { return p100 < t.pa ? t.a : p100 < t.pb ? t.b : t.c; }

}  // namespace

bool denOf(Eco e, int32_t q, Monster& mon, uint8_t& pack) {
  const Trio& t = denTrio(e);
  if (t.a == None) return false;
  const int p100 = (int)(((int64_t)(q & 0xFFFF) * 100) >> 16);
  mon = pick(t, p100);
  switch (mon) {
    case Monster::Wolf: case Monster::IceWolf: case Monster::Hyena: pack = (uint8_t)(3 + (q & 1)); break;
    case Monster::Goblin: pack = (uint8_t)(3 + ((q >> 1) & 1)); break;
    case Monster::Skeleton: case Monster::Blightspawn: pack = 3; break;
    case Monster::Spider: case Monster::FrostSpider: case Monster::Scorpion: case Monster::EmberHound: case Monster::Wisp: pack = 2; break;
    default: pack = 1; break;
  }
  return true;
}

// The lone roamers (Game's wildlife spawner, game.cpp): by day the biome's own beasts (the den trio, with a lighter
// creature a quarter of the time); by night the land's night things come out first: wisps over magic lands and the
// bogs, the blight's spawn, wraiths over the marshes, the restless dead on the open plains and the dry lands, hyenas
// on the savanna, ice wolves on the snow, ember hounds over the ash.
bool roamerOf(Eco e, bool night, float roll, Monster& mon) {
  const Biome fam = ecoFamily(e);
  if (fam == Biome::Ocean || fam == Biome::Mountain) return false;
  if (fam == Biome::Beach) {
    if (e == Eco::CoralCoast && roll < 0.3f) { mon = Monster::Lurker; return true; }
    if (e == Eco::SeaCliffs) { if (night && roll < 0.3f) { mon = Monster::Skeleton; return true; } mon = Monster::Bat; return roll < 0.5f; }
    mon = Monster::Mudcrab;
    return true;
  }
  if (night) {
    switch (e) {
      case Eco::MushroomForest: case Eco::Silverwood: case Eco::CrystalBarrens: case Eco::StonePlains:
        if (roll < 0.40f) { mon = Monster::Wisp; return true; }
        break;
      case Eco::PeatBog: case Eco::TaigaBog: case Eco::FloodedForest:
        if (roll < 0.25f) { mon = Monster::Wisp; return true; }
        if (roll < 0.45f) { mon = e == Eco::TaigaBog ? Monster::Wolf : Monster::Wraith; return true; }
        break;
      case Eco::Blight: case Eco::DarkForest:
        if (roll < 0.45f) { mon = Monster::Blightspawn; return true; }
        if (roll < 0.60f) { mon = Monster::Wraith; return true; }
        break;
      case Eco::Savanna: case Eco::Scrubland: case Eco::Steppe:
        if (roll < 0.45f) { mon = Monster::Hyena; return true; }
        break;
      case Eco::AshFields:
        if (roll < 0.55f) { mon = Monster::EmberHound; return true; }
        break;
      case Eco::SnowField: case Eco::Tundra: case Eco::FrozenLakes: case Eco::Glacier:
        if (roll < 0.35f) { mon = Monster::IceWolf; return true; }
        break;
      case Eco::Jungle: case Eco::Mangrove:
        if (roll < 0.25f) { mon = Monster::Spider; return true; }
        break;
      default:
        if (fam == Biome::Swamp && roll < 0.30f) { mon = Monster::Wraith; return true; }
        if ((fam == Biome::Plains || fam == Biome::Desert) && roll < 0.30f) { mon = Monster::Skeleton; return true; }
        if ((fam == Biome::Forest || fam == Biome::Autumn) && roll < 0.15f) { mon = Monster::Bat; return true; }
        break;
    }
  }
  const Trio& t = denTrio(e);
  if (t.a == None) return false;
  // a quarter of the time a lighter creature of the land (slimes on the plains, boars in the woods, crabs in the
  // marsh, scorpions in the desert...)
  if (roll >= 0.75f) {
    switch (e) {
      case Eco::Savanna: case Eco::Steppe: case Eco::Scrubland: mon = Monster::Hyena; return true;
      case Eco::Dunes: mon = Monster::Sandworm; return true;
      case Eco::MushroomForest: case Eco::CrystalBarrens: mon = Monster::Slime; return true;
      case Eco::Glacier: case Eco::FrozenLakes: mon = Monster::IceWolf; return true;
      case Eco::Jungle: case Eco::Mangrove: case Eco::FloodedForest: mon = Monster::Lurker; return roll < 0.85f;
      case Eco::AshFields: mon = Monster::EmberHound; return true;
      case Eco::Blight: mon = Monster::Blightspawn; return true;
      default: break;
    }
    switch (fam) {
      case Biome::Plains:
        // (M3c fixer) the farmland plains keep their boars (the classic plains had them, and the hunt quests of the
        // farming villages name them)
        if (e == Eco::Meadow || e == Eco::FlowerMeadow || e == Eco::ChalkDowns || e == Eco::Heath || e == Eco::Prairie || e == Eco::StonePlains) {
          mon = roll < 0.875f ? Monster::Boar : Monster::Slime;
          return true;
        }
        mon = e == Eco::AlpineMeadow ? Monster::Wolf : Monster::Slime;
        return true;
      case Biome::Forest: case Biome::Autumn: mon = Monster::Boar; return true;
      case Biome::Swamp: mon = Monster::Mudcrab; return true;
      case Biome::Desert: mon = Monster::Scorpion; return true;
      case Biome::Taiga: mon = Monster::Wolf; return true;
      case Biome::Snow: mon = Monster::IceWolf; return true;
      default: break;
    }
  }
  mon = pick(t, (int)(roll * 133.0f) % 100);
  // the big brutes are rarer as lone roamers by day (they keep to their dens)
  if ((mon == Monster::Troll || mon == Monster::Yeti) && roll > 0.5f) mon = t.a == mon ? t.b : t.a;
  return true;
}

namespace {

// (M3c fixer) can a hunter actually meet monster m in biome e? Its dens (denOf, every q), or a lone roamer (roamerOf,
// day and night, a fine sweep of the roll) that the spawner does not hold back near home (the big brutes only come
// from dens there, game.cpp updateSpawning).
struct SpawnTable {
  uint8_t can[(int)Eco::COUNT][(int)Monster::COUNT] = {};
  SpawnTable() {
    for (int ei = 0; ei < (int)Eco::COUNT; ei++) {
      const Eco e = (Eco)ei;
      Monster m;
      uint8_t pack;
      for (int32_t q = 0; q < 65536; q += 97)
        if (denOf(e, q, m, pack)) can[ei][(int)m] = 1;
      for (int night = 0; night < 2; night++)
        for (int k = 0; k < 4000; k++) {
          if (!roamerOf(e, night != 0, (float)k / 4000.0f, m)) continue;
          if (m == Monster::Troll || m == Monster::Bear || m == Monster::Yeti || m == Monster::Lurker) continue;
          can[ei][(int)m] = 1;
        }
    }
  }
};
const SpawnTable& spawnTable() { static const SpawnTable T; return T; }

bool huntable(Eco e, Monster m) {
  if (m == Monster::Skeleton || m == Monster::Wraith || m == Monster::Goblin || m == Monster::Slime || m == Monster::Wisp ||
      m == Monster::Bat || m == Monster::COUNT)
    return false;
  if (m == Monster::Blightspawn && e != Eco::Blight) return false;
  return true;
}

}  // namespace

bool canMeet(Eco e, Monster m) {
  if ((int)e < 0 || (int)e >= (int)Eco::COUNT || (int)m < 0 || (int)m >= (int)Monster::COUNT) return false;
  return spawnTable().can[(int)e][(int)m] != 0;
}

// The hunted are beasts, not the dead, goblin bands or slimes: those slots fall to a beast of the land. (M3c fixer)
// Every target is one the land actually spawns (canMeet): a slot whose beast never comes out here takes the family's
// fallback if that one does, else the next beast that does live here (dens first, then roamers). A land where no
// beast lives at all (bare rock, open sea) keeps the old generic trio; the quest marker then finds the nearest den.
void huntTargets(Eco e, Monster out[3]) {
  const Biome fam = ecoFamily(e);
  Monster fb1 = Monster::Wolf, fb2 = Monster::Boar;
  switch (fam) {
    case Biome::Desert: fb1 = e == Eco::Scrubland ? Monster::Hyena : Monster::Scorpion; fb2 = e == Eco::Dunes ? Monster::Sandworm : Monster::Scorpion; break;
    case Biome::Snow: fb1 = Monster::IceWolf; fb2 = e == Eco::Glacier || e == Eco::FrozenLakes ? Monster::Yeti : Monster::IceWolf; break;
    case Biome::Taiga: fb1 = Monster::Wolf; fb2 = Monster::Bear; break;
    case Biome::Swamp: fb1 = Monster::Lurker; fb2 = Monster::Mudcrab; break;
    case Biome::Forest: case Biome::Autumn: fb1 = Monster::Wolf; fb2 = e == Eco::Jungle ? Monster::Lurker : Monster::Boar; break;
    case Biome::Plains:
      if (e == Eco::Savanna || e == Eco::Steppe) { fb1 = Monster::Hyena; fb2 = e == Eco::Steppe ? Monster::Wolf : Monster::Hyena; }
      if (e == Eco::LakeDistrict) fb2 = Monster::Lurker;
      break;
    default: break;
  }
  if (e == Eco::AshFields) { fb1 = fb2 = Monster::EmberHound; }
  if (e == Eco::Blight) { fb1 = Monster::Blightspawn; fb2 = Monster::Wolf; }
  auto ok = [&](Monster m) { return huntable(e, m) && canMeet(e, m); };
  // the beasts that live here, in a fixed order: the den trio, the fallbacks, then every other species
  Monster cand[(int)Monster::COUNT + 5];
  int nc = 0;
  auto add = [&](Monster m) {
    if (!ok(m)) return;
    for (int i = 0; i < nc; i++) if (cand[i] == m) return;
    cand[nc++] = m;
  };
  const Trio& t = denTrio(e);
  if (t.a != None) { add(t.a); add(t.b); add(t.c); }
  add(fb1); add(fb2);
  for (int i = 0; i < (int)Monster::COUNT; i++) add((Monster)i);
  if (nc == 0 && canMeet(e, Monster::Bat)) cand[nc++] = Monster::Bat;   // the cliffs: only the bat colonies live there
  if (nc == 1 && cand[0] == Monster::Bat) { out[0] = out[1] = out[2] = Monster::Bat; return; }
  if (nc == 0) { out[0] = Monster::Wolf; out[1] = Monster::Boar; out[2] = Monster::Spider; return; }
  if (t.a == None) { for (int i = 0; i < 3; i++) out[i] = cand[i % nc]; return; }
  out[0] = t.a; out[1] = t.b; out[2] = t.c;
  int next = 0;
  for (int i = 0; i < 3; i++) {
    if (ok(out[i])) continue;
    const Monster fb = i == 0 ? fb1 : fb2;
    if (ok(fb)) { out[i] = fb; continue; }
    out[i] = cand[next++ % nc];
  }
}

// A cave's / camp's theme beast where the land has its own (frost caves, scorpion pits, ember dens, croc grottoes...)
bool caveTheme(Eco e, int32_t q, Monster& mon) {
  switch (e) {
    case Eco::Glacier: mon = (q & 1) ? Monster::Yeti : Monster::FrostSpider; return true;
    case Eco::Tundra: case Eco::SnowField: case Eco::FrozenLakes: mon = (q & 1) ? Monster::FrostSpider : Monster::IceWolf; return true;
    case Eco::Dunes: case Eco::SaltFlats: mon = (q & 1) ? Monster::Sandworm : Monster::Scorpion; return true;
    case Eco::StonyDesert: case Eco::Badlands: case Eco::PetrifiedForest: mon = Monster::Scorpion; return true;
    case Eco::Savanna: case Eco::Scrubland: mon = Monster::Hyena; return true;
    case Eco::AshFields: mon = Monster::EmberHound; return true;
    case Eco::Jungle: case Eco::Mangrove: case Eco::FloodedForest: mon = (q & 1) ? Monster::Spider : Monster::Lurker; return true;
    case Eco::MushroomForest: case Eco::CrystalBarrens: case Eco::Silverwood: mon = Monster::Wisp; return true;
    case Eco::PeatBog: mon = (q & 1) ? Monster::Wisp : Monster::Skeleton; return true;
    case Eco::Blight: case Eco::DarkForest: mon = Monster::Blightspawn; return true;
    default: return false;
  }
}

}  // namespace ew
