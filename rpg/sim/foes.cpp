// M6 Steel: foes data and generators (rpg/sim/foes.h). FOES lane.
// Everything here is a pure function of the world seed and coordinates (integer hashes; no Game, no Rng streams), so a
// den, a named unique's lair or a world boss's route is the same whoever asks first. The generators memoise per world
// seed and region / kingdom cell (a small bounded cache behind a mutex: the main thread asks, but tests may run worlds
// side by side).
#include "rpg/sim/foes.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <mutex>
#include <tuple>
#include "rpg/culture/culture.h"
#include "rpg/world/biomes.h"
#include "rpg/world/source.h"
#include "rpg/world/wildlife.h"

namespace foes {

using art::Monster;

namespace {

uint64_t mixh(uint64_t a, uint64_t b) { return ew::mix64(a * 0x9E3779B97F4A7C15ull ^ ew::mix64(b + 0x632BE59BD9B4E019ull)); }
uint64_t cellKey(int32_t x, int32_t y) { return ((uint64_t)(uint32_t)x << 32) | (uint32_t)y; }
int64_t d2(int32_t ax, int32_t ay, int32_t bx, int32_t by) { const int64_t dx = ax - bx, dy = ay - by; return dx * dx + dy * dy; }

bool flyer(Monster m) { return m == Monster::Bat || m == Monster::Wisp || m == Monster::Wraith || m == Monster::Harpy; }

std::string upperS(std::string s) {
  for (char& c : s) if (c >= 'a' && c <= 'z') c = (char)(c - 32);
  return s;
}

// ---- variant looks by the land (7.6 "an Ashen Wolf, a Crystal Troll"): tint (the body's light key), overlays, prefix
struct EcoLook { uint32_t tint; uint16_t overlays; const char* prefix; };
EcoLook ecoLook(int eco) {
  
  if (eco < 0 || eco >= (int)Eco::COUNT) return {0, 0, ""};
  switch ((Eco)eco) {
    case Eco::AshFields: return {rgba(78, 72, 70), art::MO_EMBER, "ASHEN"};
    case Eco::CrystalBarrens: return {rgba(150, 172, 232), art::MO_CRYSTAL, "CRYSTAL"};
    case Eco::MixedForest: case Eco::BirchWood: case Eco::GiantForest: case Eco::BlossomGrove: case Eco::AutumnWood:
    case Eco::Taiga:
      return {rgba(96, 124, 62), art::MO_MOSS, "MOSSBACK"};
    case Eco::Jungle: case Eco::BambooForest: return {rgba(70, 120, 58), art::MO_SPIKES | art::MO_MOSS, "THORNBACK"};
    case Eco::SnowField: case Eco::Tundra: case Eco::Glacier: case Eco::FrozenLakes:
      return {rgba(206, 224, 242), art::MO_RIME, "RIME"};
    case Eco::Dunes: case Eco::StonyDesert: case Eco::SaltFlats: case Eco::Oasis: case Eco::Scrubland:
      return {rgba(208, 172, 112), art::MO_SPIKES, "DUNE"};
    case Eco::Badlands: return {rgba(176, 98, 62), art::MO_HORNS, "REDROCK"};
    case Eco::Blight: case Eco::DarkForest: return {rgba(108, 82, 118), art::MO_BONEMASK, "BLIGHTED"};
    case Eco::ReedMarsh: case Eco::PeatBog: case Eco::Mangrove: case Eco::FloodedForest: case Eco::TaigaBog:
      return {rgba(84, 98, 60), art::MO_MOSS, "MIRE"};
    case Eco::Mountain: case Eco::AlpineMeadow: case Eco::StonePlains:
      return {rgba(132, 130, 126), art::MO_PLATES, "GRANITE"};
    case Eco::PetrifiedForest: return {rgba(146, 120, 96), art::MO_PLATES, "PETRIFIED"};
    case Eco::MushroomForest: case Eco::Silverwood: return {rgba(122, 104, 168), art::MO_EYES, "GLOAM"};
    case Eco::Savanna: case Eco::Steppe: case Eco::Prairie: return {rgba(176, 134, 82), art::MO_HORNS, "TAWNY"};
    case Eco::SandBeach: case Eco::Shingle: case Eco::CoralCoast: case Eco::SeaCliffs:
      return {rgba(92, 124, 134), art::MO_PLATES, "BRINE"};
    default: return {0, 0, ""};
  }
}

// ---- names (named uniques and world bosses speak the land's tongue: cult::personName plus an epithet)
const char* const kEpithet[] = {"UNBOWED", "PALE", "HUNGRY", "RED", "GREY", "OLD", "BLIND", "SILENT", "CRUEL", "PATIENT",
                                "SCARRED", "BLACK", "RESTLESS", "WIDOWMAKER", "LAME", "TWICE-BURIED", "MOONEYED", "GRIM"};
const char* const kOldWord[] = {"OLD", "MOTHER", "FATHER", "BLACK", "GREY", "ONE-EYED", "BIG", "LAME"};
const char* const kBeastWord[] = {"NINEFANGS", "IRONHIDE", "GUTRIPPER", "MAWBREAKER", "BONEGNAWER", "REDTOOTH", "SKULLCRUSHER",
                                  "HALFTAIL", "STONEBACK", "COLDEYE", "WIDOWMAW", "BLOODTUSK", "NIGHTCLAW", "ASHJAW"};
const char* const kDeed[] = {"IT TOOK THE MILLER'S BOY", "IT HAS KILLED THREE HUNTERS SINCE THE THAW", "IT DRAGS OFF SHEEP BY THE DOZEN",
                             "NOBODY WHO WENT AFTER IT CAME HOME", "IT TORE THE DOOR OFF THE SHEPHERD'S HUT", "IT KILLS FOR SPORT, NOT HUNGER",
                             "THE DOGS WON'T GO NEAR THAT COUNTRY", "IT TOOK A WHOLE CARAVAN, OXEN AND ALL", "THEY SAY ARROWS BREAK ON IT",
                             "IT CAME FOR THE CHARCOAL BURNERS LAST WINTER"};

std::string cultureName(ew::EndlessSource& src, int32_t gx, int32_t gy, uint32_t h, bool female, uint64_t& cultureId) {
  cultureId = src.cultureAt(gx, gy);
  std::string n;
  if (cultureId) n = cult::personName(src.culture(cultureId), h, female);
  if (n.empty()) {
    static const char* const fb[] = {"SKARN", "VELKA", "MORGRA", "ODDRUN", "HASKA", "TURVAL", "GRIMA", "ULFAR"};
    n = fb[h % 8];
  }
  return upperS(n);
}

// the species a named unique of this land is: the land's den beast (wildlife.h), never one of the small fry
Monster namedSpecies(Eco e, uint64_t h) {
  for (int k = 0; k < 4; k++) {
    Monster m;
    uint8_t pack = 0;
    if (!ew::denOf(e, (int32_t)((h >> (k * 16)) & 0xFFFF), m, pack)) break;
    if (m == Monster::Slime || m == Monster::Bat || m == Monster::Mudcrab || m == Monster::Wisp || m == Monster::Goblin) continue;
    return m;
  }
  const Biome f = ecoFamily(e);
  if (f == Biome::Snow) return Monster::Yeti;
  if (f == Biome::Desert) return Monster::Scorpion;
  if (f == Biome::Swamp) return Monster::Lurker;
  return (h & 1) ? Monster::Bear : Monster::Wolf;
}

bool landOk(const ew::MacroSample& m) {
  if (m.water) return false;
  const Biome f = ecoFamily(m.eco);
  return f != Biome::Ocean && f != Biome::Beach && f != Biome::Mountain;
}

std::mutex g_memoMu;
std::map<std::tuple<uint64_t, int32_t, int32_t>, std::vector<NamedUnique>> g_named;
std::map<std::tuple<uint64_t, int32_t, int32_t>, std::pair<bool, WorldBoss>> g_bosses;

}  // namespace

const char* rankName(Rank r) {
  static const char* n[] = {"", "ELITE", "CHAMPION", "", "BOSS", "WORLD BOSS"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Rank::COUNT, "a name for every rank");
  return (int)r < (int)Rank::COUNT ? n[(int)r] : "";
}

const AffixInfo& affixInfo(Affix a) {
  static const AffixInfo t[] = {
      {"SWIFT", rgba(120, 230, 255), "+30% SPEED"},
      {"ARMOURED", rgba(190, 190, 200), "+60% ARMOUR, -20% SPEED"},
      {"VAMPIRIC", rgba(200, 30, 60), "HEALS 30% OF THE DAMAGE IT DEALS"},
      {"FRENZIED", rgba(255, 90, 40), "+40% ATTACK SPEED BELOW HALF HEALTH"},
      {"SPLITTING", rgba(150, 230, 90), "BECOMES TWO SMALL ONES ON DEATH"},
      {"BURNING", rgba(255, 140, 30), "LEAVES A FIRE TRAIL"},
      {"FROSTBOUND", rgba(150, 200, 255), "SLOWS ON HIT"},
      {"WARDED", rgba(255, 230, 120), "SHIELDS ALLIES WITHIN 5 TILES"},
      {"SUMMONER", rgba(170, 110, 255), "CALLS 2 MINIONS EVERY 12 S"},
      {"BLINKING", rgba(210, 120, 255), "TELEPORTS BEHIND YOU EVERY 6 S"},
      {"REGENERATING", rgba(90, 220, 120), "2% HEALTH A SECOND UNLESS BURNING"},
      {"VOLATILE", rgba(255, 200, 60), "EXPLODES 1 S AFTER DEATH"},
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Affix::COUNT, "an AffixInfo for every affix");
  return t[std::clamp((int)a, 0, (int)Affix::COUNT - 1)];
}

uint32_t auraColor(uint16_t affixes) {
  for (int i = 0; i < (int)Affix::COUNT; i++)
    if (affixes & affixBit((Affix)i)) return affixInfo((Affix)i).aura;
  return 0;
}

Variant variantFor(art::Monster mon, int eco, uint16_t affixes, Rank rank, uint32_t seed) {
  Variant v;
  // the land's look (elites and up wear it; an ordinary pack member keeps the species' own colours)
  if (rank != Rank::Normal && rank != Rank::Boss) {
    const EcoLook e = ecoLook(eco);
    // the undead and the golem keep their own colours under the land's marks (a mossy skeleton, a crystal golem)
    const bool keepBody = mon == Monster::Skeleton || mon == Monster::Draugr || mon == Monster::Wraith || mon == Monster::Wisp;
    if (!keepBody) v.tint = e.tint;
    v.overlays |= e.overlays;
    v.prefix = e.prefix;
    // the land's own creatures need no land word ("RIME ICE WOLF", "GRANITE STONE GOLEM" say it twice)
    const std::string p = e.prefix;
    if ((p == "RIME" && (mon == Monster::IceWolf || mon == Monster::FrostSpider || mon == Monster::Yeti)) ||
        (p == "ASHEN" && mon == Monster::EmberHound) || (p == "GLOAM" && mon == Monster::Wisp) ||
        (p == "MIRE" && mon == Monster::Lurker) || (p == "GRANITE" && mon == Monster::Golem) || (p == "DUNE" && mon == Monster::Sandworm))
      v.prefix.clear();
  }
  if (affixes & affixBit(Affix::Frostbound)) v.overlays |= art::MO_RIME;
  if (affixes & affixBit(Affix::Burning)) v.overlays |= art::MO_EMBER;
  if (affixes & affixBit(Affix::Armoured)) v.overlays |= art::MO_PLATES;
  if (affixes & affixBit(Affix::Vampiric)) v.overlays |= art::MO_EYES;
  if (rank >= Rank::Elite && rank != Rank::Boss) v.overlays |= art::MO_EYES;
  if (rank == Rank::Champion) v.overlays |= art::MO_HORNS;
  if (rank == Rank::Named) v.overlays |= (seed & 1) ? art::MO_HORNS : art::MO_SPIKES;
  if (rank == Rank::WorldBoss) v.overlays |= art::MO_HORNS | art::MO_SPIKES;
  if (rank == Rank::Elite) v.scalePct = 110;
  else if (rank == Rank::Champion) v.scalePct = 115;
  else if (rank == Rank::Named) v.scalePct = 125;
  else if (rank == Rank::WorldBoss) v.scalePct = mon == Monster::Dragon ? 100 : 160;
  // (M6 fixer round 2) the draw scale is always 1 (one pixel grid), so a world-boss lurker is the GREAT form: a bigger
  // painting in its own cell, standing high (art::MO_GREAT)
  if (rank == Rank::WorldBoss && mon == Monster::Lurker) v.overlays |= art::MO_GREAT;
  // only what the species' body can carry (no horns on a wisp, no plates on a harpy)
  v.overlays = art::overlaysFit(mon, v.overlays);
  return v;
}

bool affixAllowed(Affix f, art::Monster mon, bool human) {
  if (mon == Monster::Dragon && !human) return false;
  if (human) return f != Affix::Splitting && f != Affix::Summoner && f != Affix::Blinking && f != Affix::Volatile;
  if (f == Affix::Burning && (mon == Monster::Lurker)) return false;   // (it lives in the water)
  if (f == Affix::Armoured && flyer(mon)) return false;                // (plates and flight)
  return true;
}

uint16_t pickAffixes(uint64_t h, int n, art::Monster mon, bool human) {
  uint16_t out = 0;
  int got = 0;
  for (int k = 0; k < 40 && got < n; k++) {
    h = ew::mix64(h + 0x9E3779B97F4A7C15ull);
    const Affix f = (Affix)(h % (uint64_t)Affix::COUNT);
    if (!affixAllowed(f, mon, human) || (out & affixBit(f))) continue;
    // a regenerating foe that also burns would heal under its own fire: one or the other
    if ((f == Affix::Regenerating && (out & affixBit(Affix::Burning))) || (f == Affix::Burning && (out & affixBit(Affix::Regenerating)))) continue;
    out |= affixBit(f);
    got++;
  }
  return out;
}

FoeRoll rollFoe(uint64_t h, int D, art::Monster mon, bool human) {
  FoeRoll r;
  if (D < ELITE_MIN_D || (mon == Monster::Dragon && !human)) return r;
  h = ew::mix64(h ^ 0xE117Eull);
  if ((int)(h % 10000u) >= ELITE_PER_10K) return r;
  r.rank = Rank::Elite;
  r.affixes = pickAffixes(h >> 16, 1 + (int)((h >> 14) & 1), mon, human);
  return r;
}

bool championPack(uint64_t packKey, int D, art::Monster mon, bool human, uint16_t& affixes, int& packId) {
  affixes = 0;
  packId = 0;
  if (D < CHAMPION_MIN_D || human || mon == Monster::Dragon) return false;
  const uint64_t h = ew::mix64(packKey ^ 0xC4A3910Bull);
  if ((int)(h % 10000u) >= CHAMPION_PER_10K) return false;
  affixes = pickAffixes(h >> 20, 3, mon, human);
  packId = (int)((h >> 33) & 0x3FFFFFFFu) | 1;
  return true;
}

float rankHpMul(Rank r) {
  switch (r) {
    case Rank::Elite: return 2.2f;
    case Rank::Champion: return 3.5f;
    case Rank::Named: return 6.0f;
    case Rank::WorldBoss: return 25.0f;
    default: return 1.0f;
  }
}
float rankDmgMul(Rank r) {
  switch (r) {
    case Rank::Elite: case Rank::Champion: return 1.3f;
    case Rank::Named: return 1.5f;
    case Rank::WorldBoss: return 1.6f;
    default: return 1.0f;
  }
}
float rankXpMul(Rank r) {
  switch (r) {
    case Rank::Elite: return 2.5f;
    case Rank::Champion: return 3.5f;
    case Rank::Named: return 8.0f;
    case Rank::WorldBoss: return 20.0f;
    default: return 1.0f;
  }
}

std::string foeName(const std::string& base, uint16_t affixes, const std::string& prefix) {
  std::string n;
  for (int i = 0; i < (int)Affix::COUNT; i++)
    if (affixes & affixBit((Affix)i)) { n += affixInfo((Affix)i).name; n += ' '; }
  if (!prefix.empty()) { n += prefix; n += ' '; }
  return n + base;
}

art::Monster minionOf(art::Monster m) {
  switch (m) {
    case Monster::Skeleton: case Monster::Draugr: case Monster::Wraith: return Monster::Skeleton;
    case Monster::Spider: case Monster::FrostSpider: case Monster::Scorpion: return Monster::Spider;
    case Monster::Troll: case Monster::Bear: case Monster::Yeti: case Monster::Golem: return m == Monster::Yeti ? Monster::IceWolf : Monster::Goblin;
    case Monster::Wisp: case Monster::Bat: case Monster::Harpy: return Monster::Bat;
    case Monster::Blightspawn: case Monster::Slime: return Monster::Slime;
    case Monster::Lurker: case Monster::Mudcrab: return Monster::Mudcrab;
    case Monster::Sandworm: return Monster::Scorpion;
    case Monster::Dragon: return Monster::EmberHound;
    default: return m;   // wolves call wolves, goblins goblins, hyenas hyenas
  }
}

const char* worldBossKind(art::Monster m) {
  switch (m) {
    case Monster::Yeti: return "FROST GIANT";
    case Monster::Lurker: return "WYRM";
    case Monster::Wraith: return "LICH";
    case Monster::Dragon: return "DRAGON";
    default: return "BEHEMOTH";
  }
}

std::vector<NamedUnique> namedInRegion(ew::EndlessSource& src, int32_t rx, int32_t ry) {
  const auto key = std::make_tuple(src.seed(), rx, ry);
  {
    std::lock_guard<std::mutex> lk(g_memoMu);
    auto it = g_named.find(key);
    if (it != g_named.end()) return it->second;
  }
  std::vector<NamedUnique> out;
  const int32_t x0 = rx * ew::REGION, y0 = ry * ew::REGION;
  const uint64_t base = mixh(src.seed() ^ 0x4E414D4544ull, cellKey(rx, ry));
  const int want = 1 + (int)((base >> 7) & 1);
  const ew::GTile spawn = src.start().spawn;
  const std::vector<ew::SettlementNode> nodes = src.settlementsIn(x0 - 80, y0 - 80, x0 + ew::REGION + 80, y0 + ew::REGION + 80);
  for (int n = 0; n < want; n++) {
    for (int t = 0; t < 48; t++) {
      const uint64_t h = mixh(base, (uint64_t)(n * 97 + t + 1));
      const int32_t gx = x0 + 20 + (int32_t)(h % (uint64_t)(ew::REGION - 40));
      const int32_t gy = y0 + 20 + (int32_t)((h >> 24) % (uint64_t)(ew::REGION - 40));
      if (d2(gx, gy, spawn.x, spawn.y) < 150ll * 150ll) continue;   // the start's own fields stay a beginner's
      bool ok = true;
      for (const ew::SettlementNode& s : nodes) if (d2(gx, gy, s.x, s.y) < 60ll * 60ll) { ok = false; break; }
      for (const NamedUnique& o : out) if (ok && d2(gx, gy, o.gx, o.gy) < 50ll * 50ll) ok = false;
      if (!ok) continue;
      // (M6 fixer r4) the cheap far sample first: a full macro() on a sea or mountain try built a cold region's caches
      if (!landOk(src.macroFar(gx, gy))) continue;
      const ew::MacroSample m = src.macro(gx, gy);
      if (!landOk(m)) continue;
      NamedUnique u;
      u.id = ew::makeId(rx, ry, ew::IdKind::Poi, 0x800u + (uint32_t)n);
      u.mon = namedSpecies(m.eco, h >> 8);
      u.gx = gx; u.gy = gy;
      u.D = std::max(1, src.danger(gx, gy));
      uint64_t cid = 0;
      const bool female = ((h >> 41) & 3) == 0;
      const std::string pn = cultureName(src, gx, gy, (uint32_t)(h >> 32), female, cid);
      switch ((h >> 44) % 4) {
        case 0: u.title = std::string("THE ") + kEpithet[(h >> 48) % (sizeof(kEpithet) / sizeof(kEpithet[0]))]; u.name = pn + " " + u.title; break;
        case 1: u.name = std::string(kOldWord[(h >> 48) % (sizeof(kOldWord) / sizeof(kOldWord[0]))]) + " " + pn; break;
        default: u.name = pn + " " + kBeastWord[(h >> 48) % (sizeof(kBeastWord) / sizeof(kBeastWord[0]))]; break;
      }
      u.affixes = pickAffixes(h ^ 0xA77Full, 2 + (int)((h >> 52) & 1), u.mon, false);
      u.look = variantFor(u.mon, (int)m.eco, u.affixes, Rank::Named, (uint32_t)h);
      u.rumour = "THEY CALL IT " + u.name + ". " + kDeed[(h >> 56) % (sizeof(kDeed) / sizeof(kDeed[0]))] + ".";
      out.push_back(u);
      break;
    }
  }
  std::lock_guard<std::mutex> lk(g_memoMu);
  if (g_named.size() > 4096) g_named.clear();
  g_named[key] = out;
  return out;
}

// (M6 fixer r4, review: "foes step still hitches 13-17 ms on a plain overland walk") a cold kingdom cell's world boss is
// made in two slices on two ticks (bossWarmStep): the lair search (macro samples) first, then the rest (its culture's
// name, which may generate the culture: ~10 ms). The lair is memoised on its own; worldBossOf's result is the same
// whichever way it is reached.
struct Lair { bool ok = false; int32_t x = 0, y = 0; Eco eco = Eco::Ocean; };
std::map<std::tuple<uint64_t, int32_t, int32_t>, Lair> g_lairs;   // guarded by g_memoMu
Lair lairOf(ew::EndlessSource& src, int32_t kx, int32_t ky) {
  const auto key = std::make_tuple(src.seed(), kx, ky);
  {
    std::lock_guard<std::mutex> lk(g_memoMu);
    auto it = g_lairs.find(key);
    if (it != g_lairs.end()) return it->second;
  }
  Lair L;
  const int32_t cx = kx * ew::KCELL, cy = ky * ew::KCELL;
  const uint64_t base = mixh(src.seed() ^ 0x574F524C44ull, cellKey(kx, ky));
  const ew::GTile spawn = src.start().spawn;
  const std::vector<ew::SettlementNode> nodes = src.settlementsIn(cx - ew::KCELL / 2, cy - ew::KCELL / 2, cx + ew::KCELL / 2, cy + ew::KCELL / 2);
  for (int t = 0; t < 96 && !L.ok; t++) {
    const uint64_t h = mixh(base, (uint64_t)t + 1);
    const int32_t gx = cx - 420 + (int32_t)(h % 840u), gy = cy - 420 + (int32_t)((h >> 24) % 840u);
    if (d2(gx, gy, spawn.x, spawn.y) < 360ll * 360ll) continue;
    bool clear = true;
    // far from every settlement (a little closer is allowed on later tries: a crowded kingdom still has its beast)
    const int64_t keep = t < 64 ? 90 : 60;
    for (const ew::SettlementNode& s : nodes) if (d2(gx, gy, s.x, s.y) < keep * keep) { clear = false; break; }
    if (!clear) continue;
    if (!landOk(src.macroFar(gx, gy))) continue;   // the cheap far sample first (see namedInRegion)
    const ew::MacroSample lm = src.macro(gx, gy);
    if (!landOk(lm)) continue;
    L.x = gx; L.y = gy; L.eco = lm.eco;
    L.ok = true;
  }
  std::lock_guard<std::mutex> lk(g_memoMu);
  if (g_lairs.size() > 2048) g_lairs.clear();
  g_lairs[key] = L;
  return L;
}

WorldBoss worldBossOf(ew::EndlessSource& src, int32_t kx, int32_t ky, bool& ok) {
  const auto key = std::make_tuple(src.seed(), kx, ky);
  {
    std::lock_guard<std::mutex> lk(g_memoMu);
    auto it = g_bosses.find(key);
    if (it != g_bosses.end()) { ok = it->second.first; return it->second.second; }
  }
  WorldBoss b;
  ok = false;
  const int32_t cx = kx * ew::KCELL, cy = ky * ew::KCELL;
  const uint64_t base = mixh(src.seed() ^ 0x574F524C44ull, cellKey(kx, ky));
  const ew::GTile spawn = src.start().spawn;
  const bool startCell = ew::EndlessSource::kcellOf(spawn.x) == kx && ew::EndlessSource::kcellOf(spawn.y) == ky;
  const std::vector<ew::SettlementNode> nodes = src.settlementsIn(cx - ew::KCELL / 2, cy - ew::KCELL / 2, cx + ew::KCELL / 2, cy + ew::KCELL / 2);
  const Lair L = lairOf(src, kx, ky);
  ok = L.ok;
  b.lairX = L.x; b.lairY = L.y;
  if (ok) {
    const Eco e = L.eco;
    const Biome f = ecoFamily(e);
    Monster mon = Monster::Troll;
    if (f == Biome::Snow || e == Eco::Taiga) mon = Monster::Yeti;
    else if (f == Biome::Swamp || e == Eco::LakeDistrict || e == Eco::Jungle) mon = Monster::Lurker;
    else if (e == Eco::Blight || e == Eco::DarkForest || e == Eco::StonePlains || e == Eco::PetrifiedForest || e == Eco::Heath) mon = Monster::Wraith;
    else if (e == Eco::AshFields || e == Eco::Badlands || e == Eco::Dunes || e == Eco::StonyDesert || e == Eco::AlpineMeadow)
      mon = startCell ? Monster::Golem : Monster::Dragon;   // (Ashfang already rules the start's skies)
    else if (e == Eco::CrystalBarrens || (base >> 9) % 3 == 0) mon = Monster::Golem;
    b.mon = mon;
    b.kind = worldBossKind(mon);
    b.id = ew::makeId(kx, ky, ew::IdKind::Poi, 0xF00);
    b.D = std::max(1, src.danger(b.lairX, b.lairY)) + 5;
    b.range = 220 + (int32_t)((base >> 13) % 100u);
    static const char* const ep[5][4] = {
        {"WHITE", "RIMEBORN", "COLD KING", "AVALANCHE"},       // frost giant
        {"DEEP", "DROWNER", "COILED", "BLACKWATER"},           // wyrm
        {"UNDYING", "HOLLOW KING", "GRAVE-LORD", "PALE"},      // lich
        {"RED", "CINDERWING", "SKYBURNER", "GOLDHOARD"},       // dragon
        {"MOUNTAIN", "EARTHSHAKER", "STONEFIST", "GREAT"},      // behemoth
    };
    const int kk = mon == Monster::Yeti ? 0 : mon == Monster::Lurker ? 1 : mon == Monster::Wraith ? 2 : mon == Monster::Dragon ? 3 : 4;
    const std::string pn = cultureName(src, b.lairX, b.lairY, (uint32_t)(base >> 20), ((base >> 3) & 3) == 0, b.culture);
    b.title = std::string("THE ") + ep[kk][(base >> 40) % 4];
    b.name = pn + " " + b.title;
    b.look = variantFor(mon, (int)e, 0, Rank::WorldBoss, (uint32_t)base);
    switch (mon) {
      case Monster::Yeti: b.look.tint = rgba(222, 234, 246); b.look.overlays |= art::MO_RIME; b.look.prefix = ""; break;
      case Monster::Wraith: b.look.tint = 0; b.look.overlays |= art::MO_BONEMASK | art::MO_EYES; b.look.prefix = ""; break;
      // (M6 fixer round 2, review must: "the WYRM reads as a small flat lizard, green on green under the jungle canopy")
      // the great form (MO_GREAT, standing high) in a blackwater slate hide that parts from any green ground, a pale
      // bone crest down its back, horns and burning eyes; no plates or moss (both sank it into the undergrowth)
      case Monster::Lurker:
        b.look.tint = rgba(74, 86, 104);
        b.look.overlays = art::MO_GREAT | art::MO_HORNS | art::MO_SPIKES | art::MO_EYES;
        b.look.prefix = "";
        break;
      case Monster::Dragon: b.look.overlays = art::MO_HORNS | art::MO_EMBER; b.look.tint = 0; b.look.prefix = ""; break;
      default: b.look.overlays |= art::MO_PLATES; b.look.prefix = ""; break;
    }
    // its route: the lair, then up to five haunts round it on land, visited a day each
    b.routeX[0] = b.lairX; b.routeY[0] = b.lairY;
    b.routeN = 1;
    for (int k = 0; k < 24 && b.routeN < WorldBoss::ROUTE; k++) {
      const uint64_t h = mixh(base ^ 0x407Eull, (uint64_t)k);
      // (integers only: the route is part of the world and must agree on every compiler)
      static const int kCos[16] = {1000, 924, 707, 383, 0, -383, -707, -924, -1000, -924, -707, -383, 0, 383, 707, 924};
      const int dir = (int)(h % 16u);
      const int64_t r = (int64_t)b.range * (300 + (int64_t)((h >> 16) % 600u)) / 1000;
      const int32_t x = b.lairX + (int32_t)(r * kCos[dir] / 1000), y = b.lairY + (int32_t)(r * kCos[(dir + 12) % 16] / 1000);
      if (d2(x, y, spawn.x, spawn.y) < 250ll * 250ll) continue;   // it never roams into the start's own country
      // (M6 fixer r4) judged on the far sample alone (a full macro() per haunt built a cold region's caches each, 10+ ms
      // a boss on the walk); spawning still checks the real ground under it (foeStep: blocked, roads, towns)
      if (!landOk(src.macroFar(x, y))) continue;
      bool nearTown = false;
      for (const ew::SettlementNode& s : nodes) if (d2(x, y, s.x, s.y) < 30ll * 30ll) { nearTown = true; break; }
      if (nearTown) continue;
      b.routeX[b.routeN] = x; b.routeY[b.routeN] = y;
      b.routeN++;
    }
  }
  std::lock_guard<std::mutex> lk(g_memoMu);
  if (g_bosses.size() > 2048) g_bosses.clear();
  g_bosses[key] = {ok, b};
  return b;
}

void worldBossAt(const WorldBoss& b, int day, float hour, int32_t& gx, int32_t& gy) {
  const int n = std::max(1, b.routeN);
  if (b.routeN <= 1) { gx = b.lairX; gy = b.lairY; return; }
  const int seg = ((day % n) + n) % n, nxt = (seg + 1) % n;
  // it lies up in the morning and the evening, and travels between its haunts through the middle of the day
  const float t = std::clamp((hour - 9.0f) / 9.0f, 0.0f, 1.0f);
  gx = b.routeX[seg] + (int32_t)std::lround((float)(b.routeX[nxt] - b.routeX[seg]) * t);
  gy = b.routeY[seg] + (int32_t)std::lround((float)(b.routeY[nxt] - b.routeY[seg]) * t);
}

Phase bossPhaseFor(art::Monster mon, int phaseIndex, uint32_t seed) {
  // the second phase never repeats the first; the bodies that can't summon (a lone golem) shield instead
  static const Phase p[] = {Phase::SummonAdds, Phase::Enrage, Phase::Hazard, Phase::Shield};
  const uint32_t a = (seed + 7u) % 4u;
  uint32_t i = phaseIndex <= 1 ? a : (a + 1u + (seed >> 8) % 3u) % 4u;
  Phase ph = p[i];
  if (ph == Phase::SummonAdds && (mon == Monster::Golem || mon == Monster::Lurker)) ph = Phase::Shield;
  return ph;
}

bool namedReady(ew::EndlessSource& src, int32_t rx, int32_t ry) {
  std::lock_guard<std::mutex> lk(g_memoMu);
  return g_named.count(std::make_tuple(src.seed(), rx, ry)) != 0;
}

bool bossReady(ew::EndlessSource& src, int32_t kx, int32_t ky) {
  std::lock_guard<std::mutex> lk(g_memoMu);
  return g_bosses.count(std::make_tuple(src.seed(), kx, ky)) != 0;
}

bool bossWarmStep(ew::EndlessSource& src, int32_t kx, int32_t ky) {
  if (bossReady(src, kx, ky)) return true;
  bool lairKnown;
  {
    std::lock_guard<std::mutex> lk(g_memoMu);
    lairKnown = g_lairs.count(std::make_tuple(src.seed(), kx, ky)) != 0;
  }
  if (!lairKnown) { (void)lairOf(src, kx, ky); return false; }
  bool ok = false;
  (void)worldBossOf(src, kx, ky, ok);
  return true;
}

void clearMemo() {
  std::lock_guard<std::mutex> lk(g_memoMu);
  g_named.clear();
  g_bosses.clear();
  g_lairs.clear();
}

std::string beastNameNear(ew::EndlessSource& src, int mon, int32_t gx, int32_t gy, int rank) {
  const int32_t kx = ew::EndlessSource::kcellOf(gx), ky = ew::EndlessSource::kcellOf(gy);
  std::string best;
  int64_t bd = -1;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      if (rank == (int)Rank::Named) continue;
      bool ok = false;
      const WorldBoss b = worldBossOf(src, kx + dx, ky + dy, ok);
      if (!ok || (mon >= 0 && (int)b.mon != mon)) continue;
      // its own kingdom cell is its country; a neighbour's beast only when the place lies in its reach
      const int64_t d = d2(gx, gy, b.lairX, b.lairY);
      const int64_t r = 2 * (int64_t)b.range + 100;
      if (((dx || dy) && d > r * r) || (bd >= 0 && d >= bd)) continue;
      bd = d;
      best = b.name;
    }
  if (!best.empty() || rank == (int)Rank::WorldBoss) return best;
  const int32_t rx = gx >= 0 ? gx / ew::REGION : -((-gx + ew::REGION - 1) / ew::REGION), ry = gy >= 0 ? gy / ew::REGION : -((-gy + ew::REGION - 1) / ew::REGION);
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++)
      for (const NamedUnique& u : namedInRegion(src, rx + dx, ry + dy)) {
        if (mon >= 0 && (int)u.mon != mon) continue;
        const int64_t d = d2(gx, gy, u.gx, u.gy);
        if (d > 400ll * 400ll || (bd >= 0 && d >= bd)) continue;
        bd = d;
        best = u.name;
      }
  return best;
}

}  // namespace foes
