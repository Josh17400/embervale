// M5 "Hearth and Hall": the census (VISION_PLAN 10.1, 15.12). CITIZENS lane.
// Deterministic from the site record, its buildings (which arrive whole: Site::bldgFirst / bldgCount), its overworld
// spawns (which travel with the buildings: World::siteSpawns) and its culture; integer hashing only; never depends on
// what else is loaded, on load order or on the clock. Built in slices (buildCensusStep) so a capital of 700+ people
// never costs one update step more than its budget; buildCensus runs the same steps at once.
//   0. the buildings are sorted into homes (capacity by type, storeys, the culture's family size) and workplaces;
//   1. every workplace gets its keepers (an inn: the innkeeper and 1-2 servers; a smithy: the smith and an apprentice; a
//      temple: a priest and an acolyte; the seat: the lord, a steward and servants...), who live at work when it is a
//      dwelling (inn, temple, seat, lodge) and otherwise head a household (spouse, children) in the nearest free home;
//   2. the remaining homes fill with households (a couple, children by the age pyramid, an elder, a grown child) whose
//      working adults follow the settlement's specialisation and the society's classes (cult::societyOf);
//   3. towns and cities add a bard (at the gathering place), a lamplighter and beggars (more where crime is tolerated);
//   4. friends: workmates and neighbours within a short walk (Tie);
//   5. the spawn map: each generator spawn of the site is ONE resident (keyed building slots 0, the overworld spawns by
//      role, each building's other interior slots from its own household), never two spawns the same person;
//   6. the stores start with a few days of what the settlement makes (15.11) and food for its people.
#include <algorithm>
#include <chrono>
#include <cmath>
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/life.h"

namespace life {

namespace {

uint32_t h32(uint64_t a, uint64_t b) { return (uint32_t)(ew::mix64(a * 0x9E3779B97F4A7C15ull ^ ew::mix64(b + 0x51D7ull)) >> 16); }
// how many people a home holds (VISION_PLAN 10.1: a hut 1-2, a house 2-4, a townhouse 3-5, a farmhouse 3-6), with the
// culture's family size (pious, inward-looking peoples keep big households; merchant and scholar towns small ones)
int homeCapacity(const Bldg& b, uint32_t h, int famBias) {
  using art::Building;
  int cap = 0;
  switch (b.type) {
    case Building::Hut: return 1 + (int)(h % 2u);
    case Building::Farmhouse: cap = 3 + (int)(h % 4u); break;
    case Building::House: case Building::StoneHouse:
      if (b.storeys >= 2 || b.urban >= 1) cap = 3 + (int)(h % 3u);
      else cap = 2 + (int)(h % 3u);
      break;
    default: return 0;
  }
  if (famBias > 0 && ((h >> 8) % 3u) == 0) cap++;
  if (famBias < 0 && ((h >> 8) % 3u) == 0) cap--;
  return std::clamp(cap, 1, 6);
}

// the keepers a workplace needs (first = the master, who is the building's interior slot 0)
struct Keepers { Job master = Job::None, hand = Job::None; int hands = 0; bool livesIn = false; };
Keepers keepersOf(const Bldg& b, uint32_t h) {
  using art::Building;
  Keepers k;
  if (bldgIsSeat(b)) { k.master = Job::Noble; k.hand = Job::Servant; k.hands = bldgIsRoyalSeat(b) ? 3 : 2; k.livesIn = true; return k; }
  switch (b.type) {
    case Building::Inn: k.master = Job::Innkeeper; k.hand = Job::Server; k.hands = 1 + (int)(h % 2u); k.livesIn = true; break;
    case Building::MeadHall: case Building::TeaHouse: case Building::Bathhouse:
      k.master = Job::Innkeeper; k.hand = Job::Server; k.hands = 1; k.livesIn = true; break;
    case Building::Smithy: k.master = Job::Smith; k.hand = Job::Smith; k.hands = 1; break;
    case Building::Smelter: k.master = Job::Smith; k.hand = Job::Miner; k.hands = 1; break;
    case Building::Shop: case Building::Exchange: k.master = Job::Merchant; break;
    case Building::Bakery: k.master = Job::Baker; k.hand = Job::Baker; k.hands = (int)(h % 2u); break;
    case Building::Butcher: k.master = Job::Merchant; break;
    case Building::Fishmonger: k.master = Job::Merchant; k.hand = Job::Fisher; k.hands = 1; break;
    case Building::Weaver: case Building::Tanner: k.master = Job::Tailor; break;
    case Building::Sawmill: k.master = Job::Woodcutter; k.hand = Job::Woodcutter; k.hands = 1; break;
    case Building::Windmill: case Building::Watermill: case Building::Granary: k.master = Job::Farmer; break;
    case Building::Temple: k.master = Job::Priest; k.hand = Job::Priest; k.hands = 1; k.livesIn = true; break;
    case Building::Tower: k.master = Job::Scholar; k.livesIn = true; break;
    case Building::Guildhall: case Building::CouncilHall: k.master = Job::Elder; break;
    case Building::Barracks: case Building::Lodge: k.master = Job::Guard; k.hand = Job::Guard; k.hands = 2; k.livesIn = true; break;
    default: break;
  }
  return k;
}

// the job mix of ordinary working adults (VISION_PLAN 10.1 by archetype; 15.14 society classes; the culture's values)
struct JobMix {
  uint16_t w[(int)Job::COUNT] = {};
  int total = 0;
  void add(Job j, int n) { if (n > 0) { w[(int)j] = (uint16_t)(w[(int)j] + n); total += n; } }
  Job pick(uint32_t h) const {
    if (total <= 0) return Job::Labourer;
    int r = (int)(h % (uint32_t)total);
    for (int j = 0; j < (int)Job::COUNT; j++) { r -= w[j]; if (r < 0) return (Job)j; }
    return Job::Labourer;
  }
};
JobMix jobMix(const Site& s, int tier, const cult::Society* so, const uint8_t* v) {
  Job spec = Job::Farmer;
  switch ((ew::Specialty)s.special) {
    case ew::Specialty::Fishing: spec = Job::Fisher; break;
    case ew::Specialty::Mining: spec = Job::Miner; break;
    case ew::Specialty::Lumber: spec = Job::Woodcutter; break;
    case ew::Specialty::Herding: spec = Job::Herder; break;
    default: spec = Job::Farmer; break;
  }
  const uint16_t cls = so ? so->classes : 0;
  auto val = [&](int i) { return v ? (int)v[i] : 100; };   // martial, mercantile, pious, scholarly, seafaring, expansionist, isolationist, honour
  JobMix m;
  if (tier == 0) {
    m.add(spec, 60);
    m.add(Job::Farmer, 12 + ((cls & cult::CLS_FARMERS) ? 4 : 0));
    m.add(Job::Hunter, 6 + val(0) / 64 + ((cls & cult::CLS_WARRIORS) ? 2 : 0));
    m.add(Job::Labourer, 5 + ((cls & cult::CLS_SERFS) ? 4 : 0));
    m.add(Job::Herder, (cls & cult::CLS_HERDERS) ? 6 : 1);
    m.add(Job::Fisher, val(4) / 64);
    m.add(Job::Woodcutter, 3);
    return m;
  }
  m.add(spec, 25);
  m.add(Job::Labourer, 13 + ((cls & cult::CLS_SERFS) ? 6 : 0));
  m.add(Job::Merchant, 7 + ((cls & cult::CLS_MERCHANTS) ? 6 : 0) + val(1) / 40);
  m.add(Job::Tailor, 5 + ((cls & cult::CLS_CRAFTSFOLK) ? 3 : 0));
  m.add(Job::Smith, 2 + ((cls & cult::CLS_CRAFTSFOLK) ? 2 : 0));
  m.add(Job::Baker, 3);
  m.add(Job::Stablehand, 4);
  m.add(Job::Farmer, 9 + ((cls & cult::CLS_FARMERS) ? 4 : 0));
  m.add(Job::Scholar, 2 + ((cls & cult::CLS_SCHOLARS) ? 4 : 0) + val(3) / 50);
  m.add(Job::Hunter, 3 + ((cls & cult::CLS_WARRIORS) ? 3 : 0));
  m.add(Job::Herder, (cls & cult::CLS_HERDERS) ? 5 : 1);
  m.add(Job::Priest, 1 + ((cls & cult::CLS_PRIESTS) ? 2 : 0) + val(2) / 90);
  m.add(Job::Fisher, val(4) / 40);
  if (tier >= 2 && (cls & cult::CLS_NOBLES)) m.add(Job::Servant, 4);
  return m;
}

// traits from the culture's values (each an independent roll on its own hash byte)
uint8_t traitsFor(uint32_t h, const uint8_t* v) {
  auto val = [&](int i) { return v ? (int)v[i] : 100; };
  auto roll = [&](int k, int pct) { return (int)(h32(h, (uint64_t)k * 131u) % 100u) < pct; };
  uint8_t t = 0;
  if (roll(1, 15 + val(1) / 10 + (255 - val(6)) / 20)) t |= TR_SOCIABLE;
  if (roll(2, 8 + val(2) / 4)) t |= TR_DEVOUT;
  if (roll(3, 6 + val(1) / 10)) t |= TR_THRIFTY;
  if (roll(4, 8)) t |= TR_LAZY;
  if (roll(5, 10)) t |= TR_GLUTTON;
  if (roll(6, 6 + (val(0) + val(7)) / 12)) t |= TR_BRAVE;
  if (roll(7, 8 + val(3) / 20)) t |= TR_NIGHTOWL;
  if (roll(8, 5 + val(6) / 12)) t |= TR_GRUMPY;
  return t;
}

std::string nameFor(const cult::Culture* cu, uint32_t seed, bool female) {
  if (cu) return cult::personName(*cu, seed, female);
  Rng r(seed);
  return makePersonName(r, female);
}

// what a job's work earns an hour, and the purse it feels comfortable with (the Money need: coin against this)
int purseOf(Job j) {
  switch (j) {
    case Job::Noble: return 160;
    case Job::Merchant: case Job::Innkeeper: case Job::Scholar: return 60;
    case Job::Smith: case Job::Guard: case Job::Baker: case Job::Tailor: return 45;
    case Job::Child: return 10;
    case Job::Elder: return 20;
    case Job::Beggar: return 12;
    default: return 30;
  }
}

// candidate classes of the spawn map (by the spawn's role)
enum Cls { C_CHILD, C_ADULT, C_FARM, C_FISH, C_HUNT, C_SMITH, C_TRADE, C_PRIEST, C_MAGE, C_INN, C_COUNT };
int clsOfRole(Role r) {
  switch (r) {
    case Role::Child: return C_CHILD;
    case Role::Farmer: return C_FARM;
    case Role::Fisher: return C_FISH;
    case Role::Hunter: return C_HUNT;
    case Role::Smith: return C_SMITH;
    case Role::Merchant: return C_TRADE;
    case Role::Priest: return C_PRIEST;
    case Role::Mage: return C_MAGE;
    case Role::Innkeeper: return C_INN;
    default: return C_ADULT;
  }
}
bool inCls(const Resident& r, int cls) {
  if (cls == C_CHILD) return r.job == Job::Child;
  if (r.job == Job::Child) return false;
  if (cls == C_ADULT) return r.job != Job::Guard && r.job != Job::Noble;
  const Role jr = jobRole(r.job);
  switch (cls) {
    case C_FARM: return jr == Role::Farmer;
    case C_FISH: return jr == Role::Fisher;
    case C_HUNT: return jr == Role::Hunter;
    case C_SMITH: return jr == Role::Smith;
    case C_TRADE: return jr == Role::Merchant;
    case C_PRIEST: return jr == Role::Priest;
    case C_MAGE: return jr == Role::Mage;
    case C_INN: return r.job == Job::Server || r.job == Job::Innkeeper;
    default: return false;
  }
}
bool guardish(Role r) { return r == Role::Guard || r == Role::King || r == Role::Soldier || r == Role::Captain || r == Role::Herald; }

int64_t spawnKey(int bldgOff, int slot) { return ((int64_t)(bldgOff + 1) << 32) | (int64_t)(uint32_t)slot; }

// the rank of an interior's slot among the building's own people (ground slots 1..15, then 15 per upper floor)
int innerRank(int slot) {
  if (slot <= 0) return -1;
  const int f = slot / 16, k = slot % 16;
  if (f == 0) return slot - 1;
  if (k == 0) return -1;
  return 15 + (f - 1) * 15 + (k - 1);
}

uint8_t festivalKindOf(const cult::Culture* cu, const Site& s) {
  if (!cu) return FEST_HARVEST;
  const uint16_t d = cu->faith.domains;   // sun, moon, sea, war, harvest, death, craft, wisdom, storm, forest, hearth, stars
  if (cu->faith.kind == 2) return FEST_ANCESTORS;
  if (cu->customs.drink == 1) return FEST_MEAD;
  if (cu->customs.drink == 3 || (d & (2u | 2048u))) return FEST_LANTERNS;
  if ((d & 4u) || (ew::Specialty)s.special == ew::Specialty::Fishing) return FEST_SEA;
  if (d & 16u || (ew::Specialty)s.special == ew::Specialty::Farming) return FEST_HARVEST;
  if (cu->faith.kind == 3 || (d & 512u)) return FEST_SPRING;
  if ((d & 1u) || cu->customs.drink == 4) return FEST_FIRES;
  return FEST_GOD;
}

}  // namespace

const char* festivalName(uint8_t k) {
  static const char* n[] = {"HARVEST FAIR", "HOLY FEAST", "LANTERN NIGHT", "ANCESTORS' DAY", "SPRING DANCE", "BLESSING OF THE BOATS",
                            "MIDSUMMER FIRES", "MEAD FEAST"};
  static_assert(sizeof(n) / sizeof(n[0]) == FEST_COUNT, "a name for every festival");
  return k < FEST_COUNT ? n[k] : "";
}

// ---------------------------------------------------------------- the builder
bool buildCensusStep(const World& w, Life::Build& b, int budget) {
  // the slice is counted in steps of work (a workplace, a home, a resident, a spawn), never on the clock: how far a
  // census gets in a frame, and so the hour it joins the sim, must not depend on the machine (determinism)
  // (a household, phase 2, costs about twelve times the other steps)
  int ticks = 0;
  auto outOfTime = [&](int phase) { return budget > 0 && (ticks += phase == 2 ? 12 : 1) >= budget; };
  Census& c = b.c;
  const int si = b.site;
  if (si < 0 || si >= (int)w.sites.size()) { c = Census(); return true; }
  const Site& s = w.sites[(size_t)si];
  const cult::Culture* cu = w.cultureOf(si);
  const uint8_t* vals = cu ? cu->values : nullptr;
  cult::Society so;
  if (cu) so = cult::societyOf(*cu);
  const int famBias = !vals ? 0 : (vals[2] + vals[6] > 330 ? 1 : (vals[1] + vals[3] > 330 ? -1 : 0));

  auto add = [&](Job j, int home, int work, uint16_t hh, uint32_t h) -> int {
    Resident r;
    r.idx = (uint16_t)c.res.size();
    r.job = j;
    r.female = (h & 1u) != 0;
    const uint32_t a = h >> 3, a2 = h32(h, 0xA9Eull);
    if (j == Job::Child) r.age = (uint8_t)(2 + a % 14u);
    else if (j == Job::Elder) r.age = (uint8_t)(60 + a % 14u + a2 % 13u);
    else r.age = (uint8_t)(18 + a % 22u + a2 % 21u);   // a triangle: most adults in their thirties
    r.traits = traitsFor(h32(h, 0x7A175ull), vals);
    r.jitter = (int8_t)((int)((h >> 20) % 13u) - 6);
    r.shift = j == Job::Guard ? (uint8_t)((h >> 25) & 1u) : 0;
    r.lookSeed = h32(c.seed, (uint64_t)r.idx * 7919u + 0x100Cull);
    r.home = (int16_t)home;
    r.work = (int16_t)work;
    r.household = hh;
    const int purse = purseOf(j);
    r.coin = (uint16_t)(purse / 2 + (int)((h >> 12) % (uint32_t)std::max(1, purse / 2)));
    r.need = {{(uint8_t)(55 + (h >> 4) % 30u), (uint8_t)(60 + (h >> 7) % 30u), (uint8_t)(45 + (h >> 10) % 40u),
               (uint8_t)(45 + (h >> 13) % 40u), (uint8_t)std::min(100, r.coin * 100 / std::max(1, purse))}};
    r.name = nameFor(cu, h32(c.seed, (uint64_t)r.idx * 104729u + 0x4A4Eull), r.female);
    c.res.push_back(r);
    return (int)r.idx;
  };
  // the nearest home with room for n more (by footprint centre; ties by offset): -1 none
  auto homeNear = [&](int cx, int cy, int n) -> int {
    int best = -1, bd = 1 << 30;
    for (size_t i = 0; i < b.homes.size(); i++) {
      if (b.homes[i].cap - b.homes[i].used < n) continue;
      const int d = std::abs(b.homes[i].cx - cx) + std::abs(b.homes[i].cy - cy);
      if (d < bd) { bd = d; best = (int)i; }
    }
    return best;
  };
  auto homeIx = [&](int off) -> int { for (size_t i = 0; i < b.homes.size(); i++) if (b.homes[i].off == off) return (int)i; return -1; };
  // a child or two for a household, by the age pyramid (most young couples have them; parents past 50 rarely)
  auto children = [&](int head, int home, uint16_t hh, uint32_t h, int room) {
    int made = 0;
    const int pa = c.res[(size_t)head].age;
    for (int k = 0; k < room; k++) {
      const uint32_t hk = h32(h, (uint64_t)k + 0xC41Dull);
      if (pa > 55 || (int)(hk % 100u) >= (pa < 45 ? 70 - k * 15 : 25)) break;
      const int ci = add(Job::Child, home, -1, hh, hk);
      Resident& ch = c.res[(size_t)ci];
      ch.age = (uint8_t)std::min<int>(ch.age, std::max(1, pa - 17));
      ch.parent = (int16_t)head;
      made++;
    }
    return made;
  };

  for (;;) {
    const int phase = b.phase;
    switch (b.phase) {
      case 0: {   // ---- set up and sort the buildings
        c = Census();
        b.homes.clear(); b.works.clear(); b.household = 0; b.pos = 0;
        c.site = s.id;
        c.handle = si;
        c.seed = h32(s.id, 0xCE5505ull);
        c.tier = s.type == SiteType::Village ? 0 : s.type == SiteType::Town ? 1 : s.capital ? 3 : 2;
        if (!s.settlement() || s.bldgCount <= 0) return true;
        cult::Gathering gat = cult::Gathering::Tavern, gat2 = cult::Gathering::Tavern;
        if (cu) { gat = so.gathering; gat2 = so.gathering2; }
        c.gatherKind = (uint8_t)gat;
        c.festivalKind = festivalKindOf(cu, s);
        const art::Building gatB = cult::gatheringPurpose(gat), gat2B = cult::gatheringPurpose(gat2);
        for (int off = 0; off < s.bldgCount; off++) {
          const int bi = s.bldgFirst + off;
          if (bi < 0 || bi >= (int)w.over.bldgs.size()) break;
          const Bldg& B = w.over.bldgs[(size_t)bi];
          if (B.site != si) continue;
          const uint32_t h = h32(B.id ? B.id : B.seed, 0x4011Eull);
          if (const int cap = homeCapacity(B, h, famBias); cap > 0) {
            Life::Build::Home hm;
            hm.off = off; hm.cap = cap; hm.cx = B.r.cx(); hm.cy = B.r.cy();
            b.homes.push_back(hm);
          } else if (keepersOf(B, h).master != Job::None) b.works.push_back(off);
          using art::Building;
          if (B.type == Building::Inn && c.inn < 0) c.inn = (int16_t)off;
          if (B.type == Building::Temple && c.temple < 0) c.temple = (int16_t)off;
          if (bldgIsSeat(B) && c.seat < 0) c.seat = (int16_t)off;
          if ((B.type == Building::Exchange || B.type == Building::Shop) && c.market < 0) c.market = (int16_t)off;
          if (B.type == Building::Bakery && c.bakery < 0) c.bakery = (int16_t)off;
          if ((B.type == Building::Windmill || B.type == Building::Watermill) && c.mill < 0) c.mill = (int16_t)off;
          if (B.type == Building::Smithy && c.smithy < 0) c.smithy = (int16_t)off;
          if (gatB != Building::COUNT && B.type == gatB && c.gathering < 0 && !bldgIsSeat(B)) c.gathering = (int16_t)off;
          if (gat2 != gat && gat2B != Building::COUNT && B.type == gat2B && c.gathering2 < 0 && off != c.gathering && !bldgIsSeat(B))
            c.gathering2 = (int16_t)off;
        }
        if (c.gathering < 0) c.gathering = c.inn;   // the inn's common room serves (a grove / plaza gathering: -1 = the plaza)
        if (gatB == art::Building::COUNT && gat != cult::Gathering::Tavern) c.gathering = -1;
        // (M5 fixer) where the evening crowd keeps another hall (a mead hall, a tea house, the baths), the inn is the
        // second gathering place: some of the evening is spent there, so a traveller walking into the inn finds company
        if (c.gathering2 < 0 && c.inn >= 0 && c.inn != c.gathering) c.gathering2 = c.inn;
        b.phase = 1; b.pos = 0;
        break;
      }
      case 1: {   // ---- the workplaces' keepers
        if (b.pos >= b.works.size()) { b.phase = 2; b.pos = 0; break; }
        const int off = b.works[b.pos++];
        const Bldg& B = w.over.bldgs[(size_t)(s.bldgFirst + off)];
        const uint32_t h = h32(B.id ? B.id : B.seed, 0x4011Eull);
        const Keepers k = keepersOf(B, h);
        for (int n = 0; n <= k.hands; n++) {
          const Job j = n == 0 ? k.master : k.hand;
          if (j == Job::None) break;
          const uint32_t hr = h32(h, (uint64_t)n + 0x77ull);
          int home = -1;
          const uint16_t hh = b.household++;
          int hi = -1;
          if (k.livesIn) home = off;
          else if ((hi = homeNear(B.r.cx(), B.r.cy(), 1)) >= 0) { home = b.homes[(size_t)hi].off; b.homes[(size_t)hi].used++; }
          const int ri = add(j, home, off, hh, hr);
          if (n == 0) { c.res[(size_t)ri].keyBldg = (int16_t)off; c.res[(size_t)ri].keySlot = 0; c.res[(size_t)ri].flags |= RF_KEY; }
          // a master who lives out keeps a household: a spouse and children join while the home has room
          if (!k.livesIn && hi >= 0 && n == 0) {
            Life::Build::Home& hm = b.homes[(size_t)hi];
            if (hm.used < hm.cap && (hr >> 5) % 4u != 0) {
              const JobMix mix = jobMix(s, c.tier, cu ? &so : nullptr, vals);
              const int sp = add(mix.pick(hr >> 3), home, -1, hh, h32(hr, 0x5F05Eull));
              c.res[(size_t)sp].spouse = (int16_t)ri; c.res[(size_t)ri].spouse = (int16_t)sp;
              c.res[(size_t)sp].female = !c.res[(size_t)ri].female;
              c.res[(size_t)sp].name = nameFor(cu, h32(c.seed, (uint64_t)sp * 104729u + 0x4A4Eull), c.res[(size_t)sp].female);
              c.res[(size_t)sp].age = (uint8_t)std::clamp((int)c.res[(size_t)ri].age + (int)((hr >> 9) % 9u) - 4, 18, 70);
              hm.used++;
              hm.used += children(ri, home, hh, hr, hm.cap - hm.used);
            }
          }
        }
        break;
      }
      case 2: {   // ---- households in the homes still empty or partly filled
        if (b.pos >= b.homes.size()) { b.phase = 3; b.pos = 0; break; }
        Life::Build::Home& hm = b.homes[b.pos++];
        const uint32_t h = h32(c.seed, (uint64_t)hm.off * 2654435761u + 0x4040ull);
        int room = hm.cap - hm.used;
        if (room <= 0) break;
        const JobMix mix = jobMix(s, c.tier, cu ? &so : nullptr, vals);
        const uint16_t hh = b.household++;
        const int head = add(mix.pick(h32(h, 1)), hm.off, -1, hh, h32(h, 1));
        room--;
        // the home's interior slot 0 is its head (when the generator puts someone there)
        c.res[(size_t)head].keyBldg = (int16_t)hm.off; c.res[(size_t)head].keySlot = 0; c.res[(size_t)head].flags |= RF_KEY;
        const uint32_t hs = h32(h, 2);
        if (room > 0 && hs % 100u < 78) {   // a spouse
          const int sp = add(mix.pick(h32(hs, 3)), hm.off, -1, hh, hs);
          Resident& S = c.res[(size_t)sp];
          S.female = !c.res[(size_t)head].female;
          S.name = nameFor(cu, h32(c.seed, (uint64_t)sp * 104729u + 0x4A4Eull), S.female);
          S.age = (uint8_t)std::clamp((int)c.res[(size_t)head].age + (int)((hs >> 9) % 9u) - 4, 18, 75);
          S.spouse = (int16_t)head; c.res[(size_t)head].spouse = (int16_t)sp;
          room--;
        }
        if (room > 0 && h32(h, 4) % 100u < 18) {   // an elder (a parent of the couple)
          const int e = add(Job::Elder, hm.off, -1, hh, h32(h, 5));
          c.res[(size_t)e].age = (uint8_t)std::max<int>(c.res[(size_t)e].age, c.res[(size_t)head].age + 20);
          room--;
        }
        if (room > 0) room -= children(head, hm.off, hh, h32(h, 6), room);
        if (room > 0 && c.res[(size_t)head].age >= 42 && h32(h, 7) % 100u < 40) {   // a grown son or daughter who works
          const int g = add(mix.pick(h32(h, 8)), hm.off, -1, hh, h32(h, 9));
          c.res[(size_t)g].age = (uint8_t)std::clamp((int)c.res[(size_t)head].age - 20 - (int)(h32(h, 10) % 6u), 16, 40);
          c.res[(size_t)g].parent = (int16_t)head;
          room--;
        }
        hm.used = hm.cap - std::max(0, room);
        break;
      }
      case 3: {   // ---- town life: a bard, a lamplighter, beggars; who pays for whose meals
        if (c.tier >= 1) {
          const uint32_t h = h32(c.seed, 0xBA4Dull);
          add(Job::Bard, c.gathering >= 0 ? c.gathering : -1, c.gathering, b.household++, h);
          int lh = -1;
          const int hi = homeNear(s.ex, s.ey, 1);
          if (hi >= 0) { lh = b.homes[(size_t)hi].off; b.homes[(size_t)hi].used++; }
          add(Job::Lamplighter, lh, -1, b.household++, h32(h, 1));
          int beggars = c.tier >= 3 ? 2 + (int)((h >> 8) % 2u) : c.tier >= 2 ? 1 + (int)((h >> 8) % 2u) : (int)((h >> 8) % 2u);
          if (cu && so.crimeTolerance > 150) beggars++;
          for (int k = 0; k < beggars; k++) {
            const int ri = add(Job::Beggar, -1, -1, b.household++, h32(h, 2 + (uint64_t)k));
            c.res[(size_t)ri].flags |= RF_HOMELESS;
            c.res[(size_t)ri].coin = 0;
            c.res[(size_t)ri].need[(int)Need::Money] = 0;
          }
        }
        // payers: each household's first earner pays for its children, elders and itself
        for (Resident& r : c.res) {
          if (r.job != Job::Child && r.job != Job::Elder) { r.payer = (int16_t)r.idx; continue; }
          r.payer = -1;
        }
        for (Resident& r : c.res) {
          if (r.payer >= 0) continue;
          for (const Resident& o : c.res)
            if (o.household == r.household && o.payer == (int16_t)o.idx) { r.payer = (int16_t)o.idx; break; }
        }
        b.hx.resize(c.res.size()); b.hy.resize(c.res.size());
        for (size_t i = 0; i < c.res.size(); i++) {
          const int off = c.res[i].home;
          if (off >= 0 && s.bldgFirst + off < (int)w.over.bldgs.size()) {
            const Bldg& B = w.over.bldgs[(size_t)(s.bldgFirst + off)];
            b.hx[i] = (int16_t)B.r.cx(); b.hy[i] = (int16_t)B.r.cy();
          } else { b.hx[i] = (int16_t)s.ex; b.hy[i] = (int16_t)s.ey; }
        }
        b.phase = 4; b.pos = 0;
        break;
      }
      case 4: {   // ---- friends: workmates and neighbours within a short walk (deterministic pairs by index)
        if (b.pos >= c.res.size()) { b.phase = 5; b.pos = 0; c.baseTies = (uint16_t)std::min<size_t>(c.ties.size(), 65535); break; }
        const size_t i = b.pos++;
        const Resident& a = c.res[i];
        if (a.job == Job::Child) break;
        for (size_t j = i + 1; j < c.res.size() && j < i + 14; j++) {
          const Resident& o = c.res[j];
          if (o.job == Job::Child || a.household == o.household) continue;
          const bool mates = a.work >= 0 && a.work == o.work;
          const bool near = a.home >= 0 && o.home >= 0 && std::abs(b.hx[i] - b.hx[j]) + std::abs(b.hy[i] - b.hy[j]) <= 12;
          if (!mates && !near) continue;
          const uint32_t h = h32(c.seed, (uint64_t)i * 65537u + j);
          if (h % 3u == 0) continue;
          if (c.ties.size() >= 65000) break;
          c.ties.push_back(Tie{(uint16_t)i, (uint16_t)j, (uint8_t)(35 + h % 50u)});
        }
        break;
      }
      case 5: {   // ---- the spawn map: keyed slots and the overworld spawns
        if (b.pos == 0) {
          b.anchored.assign(c.res.size(), 0);
          c.spawnKeys.clear(); c.spawnRes.clear();
          for (const Resident& r : c.res)
            if (r.keyBldg >= 0 && r.keySlot >= 0) {
              c.spawnKeys.push_back(spawnKey(r.keyBldg, r.keySlot));
              c.spawnRes.push_back((int16_t)r.idx);
              b.anchored[r.idx] = 1;
            }
          b.order.resize(c.res.size());
          for (size_t i = 0; i < c.res.size(); i++) b.order[i] = (uint16_t)i;
          const uint32_t sd = c.seed;
          std::sort(b.order.begin(), b.order.end(), [sd](uint16_t x, uint16_t y) {
            const uint32_t hx = h32(sd, (uint64_t)x + 0x5A0Bull), hy = h32(sd, (uint64_t)y + 0x5A0Bull);
            return hx != hy ? hx < hy : x < y;
          });
          b.cursor.assign(C_COUNT, 0);
          b.owSpawns.clear();
          auto it = w.siteSpawns.find(si);
          if (it != w.siteSpawns.end())
            for (int idx : it->second) {
              if (idx < 0 || idx >= (int)w.over.spawns.size()) continue;
              const Spawn& sp = w.over.spawns[(size_t)idx];
              if (sp.site != si || !sp.npc || sp.bandit || guardish(sp.role)) continue;
              b.owSpawns.push_back(idx);
            }
          std::sort(b.owSpawns.begin(), b.owSpawns.end(), [&](int x, int y) { return w.over.spawns[(size_t)x].slot < w.over.spawns[(size_t)y].slot; });
          b.owSpawns.erase(std::unique(b.owSpawns.begin(), b.owSpawns.end(),
                                       [&](int x, int y) { return w.over.spawns[(size_t)x].slot == w.over.spawns[(size_t)y].slot; }),
                           b.owSpawns.end());
        }
        if (b.pos >= b.owSpawns.size()) { b.phase = 6; b.pos = 0; break; }
        const Spawn& sp = w.over.spawns[(size_t)b.owSpawns[b.pos++]];
        int cls = clsOfRole(sp.role);
        int pick = -1;
        for (int tries = 0; tries < 2 && pick < 0; tries++) {
          size_t& cur = b.cursor[(size_t)cls];
          while (cur < b.order.size()) {
            const int ri = b.order[cur];
            if (!b.anchored[(size_t)ri] && inCls(c.res[(size_t)ri], cls) && !((c.res[(size_t)ri].flags & RF_HOMELESS) && cls != C_ADULT)) { pick = ri; break; }
            cur++;
          }
          if (cls == C_CHILD) break;   // a child spawn is a child or nobody
          cls = C_ADULT;
        }
        if (pick >= 0) {
          b.anchored[(size_t)pick] = 1;
          c.spawnKeys.push_back(spawnKey(-1, sp.slot));
          c.spawnRes.push_back((int16_t)pick);
        }
        break;
      }
      case 6: {   // ---- each building's own people (its interior's other slots); the stores; done
        // sort the keyed / overworld map by key
        {
          std::vector<std::pair<int64_t, int16_t>> kv(c.spawnKeys.size());
          for (size_t i = 0; i < kv.size(); i++) kv[i] = {c.spawnKeys[i], c.spawnRes[i]};
          std::sort(kv.begin(), kv.end());
          for (size_t i = 0; i < kv.size(); i++) { c.spawnKeys[i] = kv[i].first; c.spawnRes[i] = kv[i].second; }
        }
        const int nb = std::max(0, s.bldgCount);
        c.innerStart.assign((size_t)nb + 1, 0);
        for (const Resident& r : c.res)
          if (!b.anchored[r.idx] && r.home >= 0 && r.home < nb) c.innerStart[(size_t)r.home + 1]++;
        for (int k = 0; k < nb; k++) c.innerStart[(size_t)k + 1] = (uint16_t)(c.innerStart[(size_t)k + 1] + c.innerStart[(size_t)k]);
        c.inner.assign(c.innerStart[(size_t)nb], 0);
        {
          std::vector<uint16_t> fill(c.innerStart.begin(), c.innerStart.end() - 1);
          for (const Resident& r : c.res)
            if (!b.anchored[r.idx] && r.home >= 0 && r.home < nb) c.inner[fill[(size_t)r.home]++] = r.idx;
        }
        // the settlement's stores start a few days' worth of what it makes (15.11) and food for its people
        const int n = (int)c.res.size();
        for (int g = 0; g < (int)ew::Good::COUNT; g++) {
          const bool made = (s.produces >> g) & 1u;
          c.stock[(size_t)g] = (uint16_t)(made ? 20 + n / 2 : 4);
          c.price[(size_t)g] = 100;
        }
        // food for two days (bread and produce where nothing edible is made here)
        const int meals = n * 5;
        const ew::Good foods[] = {ew::Good::Bread, ew::Good::Produce, ew::Good::Fish, ew::Good::Meat, ew::Good::Grain};
        int madeFoods = 0;
        for (ew::Good f : foods) if ((s.produces >> (int)f) & 1u) madeFoods++;
        for (ew::Good f : foods) {
          const bool made = (s.produces >> (int)f) & 1u;
          if (madeFoods ? made : (f == ew::Good::Bread || f == ew::Good::Produce))
            c.stock[(size_t)f] = (uint16_t)std::min(60000, (int)c.stock[(size_t)f] + meals / std::max(1, madeFoods ? madeFoods : 2));
        }
        for (int g = 0; g < (int)ew::Good::COUNT; g++) c.use[(size_t)g] = (uint16_t)std::max(2, n / 10);
        tieIndex(c);
        b.anchored.clear(); b.anchored.shrink_to_fit();
        b.order.clear(); b.order.shrink_to_fit();
        b.hx.clear(); b.hy.clear();
        b.phase = 7;
        return true;
      }
      default: return true;
    }
    if (outOfTime(phase)) return false;
  }
}

void buildCensus(const World& w, int si, Census& out) {
  Life::Build b;
  b.site = si;
  buildCensusStep(w, b, 0);
  out = std::move(b.c);
}

void tieIndex(Census& c) {
  c.tiesOf.assign(c.res.size(), std::vector<uint32_t>());
  for (size_t t = 0; t < c.ties.size(); t++) {
    const Tie& T = c.ties[t];
    if (T.a < c.res.size()) c.tiesOf[T.a].push_back((uint32_t)t);
    if (T.b != PLAYER_TIE && T.b < c.res.size()) c.tiesOf[T.b].push_back((uint32_t)t);
  }
}

Tie* findTie(Census& c, int a, int b) {
  if (a < 0 || a >= (int)c.tiesOf.size()) return nullptr;
  for (uint32_t t : c.tiesOf[(size_t)a]) {
    if (t >= c.ties.size()) continue;
    Tie& T = c.ties[t];
    if ((T.a == a && T.b == b) || (T.b == a && T.a == b)) return &T;
  }
  return nullptr;
}

int spawnResident(const Census& c, const Spawn& sp, int bldgOff) {
  if (!sp.npc || sp.bandit || c.res.empty()) return -1;
  const int64_t key = spawnKey(bldgOff, sp.slot);
  const auto it = std::lower_bound(c.spawnKeys.begin(), c.spawnKeys.end(), key);
  if (it != c.spawnKeys.end() && *it == key) return c.spawnRes[(size_t)(it - c.spawnKeys.begin())];
  if (bldgOff < 0 || sp.slot == 0 || bldgOff + 1 >= (int)c.innerStart.size()) return -1;
  // an interior's other people: the building's own household by rank (role permitting)
  const int rank = innerRank(sp.slot);
  const int lo = c.innerStart[(size_t)bldgOff], hi = c.innerStart[(size_t)bldgOff + 1];
  if (rank < 0 || lo + rank >= hi) return -1;
  const Resident& r = c.res[c.inner[(size_t)(lo + rank)]];
  const bool guardSpawn = guardish(sp.role);
  if (guardSpawn != (r.job == Job::Guard)) return -1;
  if (sp.role == Role::Child && r.job != Job::Child) return -1;
  return r.idx;
}

}  // namespace life
