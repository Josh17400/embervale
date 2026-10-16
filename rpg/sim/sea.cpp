// M10 "Beyond the Mist": the sea's tables, the wind, the saved block (rpg/sim/sea.h). SEA lane.
#include "rpg/sim/sea.h"

#include <algorithm>
#include <cmath>

namespace sea {

const HullInfo& hullInfo(Hull h) {
  // name, speed, windGain, turn, price, hull, waters, crew, cargo, minLevel, sailorLevel, radius
  static const HullInfo t[] = {
      {"ROWBOAT", 5.0f, 0.0f, 2.6f, 300, 60, W_SHALLOW, 0, 0, 1, 1, 7.0f},
      {"SLOOP", 8.0f, 4.0f, 1.6f, 4000, 160, W_SHALLOW | W_DEEP, 2, 1, 8, 4, 11.0f},
      {"GALLEON", 7.0f, 4.0f, 0.9f, 20000, 420, W_DEEP | W_STORM, 4, 4, 18, 12, 18.0f},
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Hull::COUNT, "a row for every hull");
  return t[(int)h < (int)Hull::COUNT ? (int)h : 0];
}
const char* hullName(Hull h) { return hullInfo(h).name; }

const std::vector<Species>& species() {
  // phase A: a starter list (the SEA lane adds the rare and legendary fish, the Mist's own, the seasons)
  static const std::vector<Species> s = {
      // name, waters, climate, seasons, hours, rarity, value, fight, minCm, maxCm, color
      {"PERCH", FW_LAKE | FW_RIVER, FC_COLD | FC_TEMPERATE, 0xF, FH_DAWN | FH_DAY | FH_DUSK, 0, 6, 0.25f, 15, 35, 0x6E9A4AFFu},
      {"BROWN TROUT", FW_RIVER | FW_LAKE, FC_COLD | FC_TEMPERATE, 0x7, FH_DAWN | FH_DUSK, 1, 14, 0.45f, 25, 60, 0x9A7A4AFFu},
      {"PIKE", FW_LAKE | FW_RIVER, FC_COLD | FC_TEMPERATE, 0xF, FH_DAY | FH_DUSK, 1, 18, 0.6f, 40, 110, 0x5A7A3AFFu},
      {"CARP", FW_LAKE, FC_TEMPERATE | FC_WARM, 0x7, FH_DAY | FH_NIGHT, 0, 8, 0.4f, 30, 80, 0xB08A3AFFu},
      {"HERRING", FW_COAST, FC_COLD | FC_TEMPERATE, 0xF, FH_DAWN | FH_DAY, 0, 5, 0.2f, 20, 35, 0x9AB0C0FFu},
      {"MACKEREL", FW_COAST | FW_DEEP, FC_TEMPERATE | FC_WARM, 0x7, FH_DAY, 0, 7, 0.35f, 25, 45, 0x3A7A8AFFu},
      {"COD", FW_DEEP | FW_COAST, FC_COLD | FC_TEMPERATE, 0xF, FH_DAWN | FH_DAY | FH_DUSK, 1, 16, 0.5f, 40, 120, 0x8A8A6AFFu},
      {"SWORDFISH", FW_DEEP, FC_WARM | FC_TEMPERATE, 0x6, FH_DAY, 2, 60, 0.85f, 120, 300, 0x4A5A8AFFu},
      {"MIST EEL", FW_MIST, FC_COLD | FC_TEMPERATE | FC_WARM, 0xF, FH_DUSK | FH_NIGHT, 2, 80, 0.7f, 60, 180, 0x8AC0C0FFu},
      // ---- SEA lane (append only: catches are saved by index)
      {"SALMON", FW_RIVER | FW_COAST, FC_COLD | FC_TEMPERATE, 0x6, FH_DAWN | FH_DUSK, 1, 22, 0.6f, 50, 110, 0xC07A6AFFu},
      {"CATFISH", FW_LAKE | FW_RIVER, FC_TEMPERATE | FC_WARM, 0xF, FH_DUSK | FH_NIGHT, 0, 9, 0.5f, 30, 120, 0x6A5A4AFFu},
      {"GRAYLING", FW_RIVER, FC_COLD, 0xF, FH_DAWN | FH_DAY, 0, 8, 0.3f, 20, 45, 0x8A9AAAFFu},
      {"GOLDEN CARP", FW_LAKE, FC_TEMPERATE | FC_WARM, 0x3, FH_DAWN | FH_DAY, 2, 70, 0.55f, 40, 90, 0xE0B040FFu},
      {"STURGEON", FW_RIVER | FW_LAKE, FC_COLD | FC_TEMPERATE, 0xF, FH_NIGHT | FH_DAWN, 2, 90, 0.8f, 100, 260, 0x5A6A6AFFu},
      {"SEA BREAM", FW_COAST, FC_TEMPERATE | FC_WARM, 0xF, FH_DAY | FH_DUSK, 0, 9, 0.35f, 25, 50, 0xB0B8C8FFu},
      {"ICEFISH", FW_COAST | FW_DEEP, FC_COLD, 0x9, FH_DAWN | FH_DAY | FH_NIGHT, 1, 20, 0.4f, 25, 55, 0xD8E8F0FFu},
      {"PUFFERFISH", FW_COAST, FC_WARM, 0xF, FH_DAY, 1, 24, 0.3f, 15, 40, 0xC8B060FFu},
      {"RED SNAPPER", FW_COAST | FW_DEEP, FC_WARM | FC_TEMPERATE, 0x7, FH_DAWN | FH_DAY, 1, 26, 0.55f, 35, 90, 0xD05A4AFFu},
      {"HALIBUT", FW_DEEP, FC_COLD | FC_TEMPERATE, 0xF, FH_DAY | FH_DUSK, 2, 75, 0.75f, 80, 250, 0x7A6A5AFFu},
      {"TUNA", FW_DEEP, FC_TEMPERATE | FC_WARM, 0x6, FH_DAWN | FH_DAY, 1, 40, 0.8f, 80, 220, 0x3A4A7AFFu},
      {"GLASS PERCH", FW_MIST, FC_COLD | FC_TEMPERATE | FC_WARM, 0xF, FH_DAWN | FH_DAY | FH_DUSK | FH_NIGHT, 1, 45, 0.4f, 20, 50, 0xC8E0E8FFu},
      // the legendaries: one per water, each with a name and a tale (fishStory); caught once
      {"OLD WHISKERS", FW_LAKE, FC_COLD | FC_TEMPERATE | FC_WARM, 0xF, FH_NIGHT, 3, 600, 0.95f, 180, 260, 0x4A4030FFu},
      {"THE SILVER KING", FW_RIVER, FC_COLD | FC_TEMPERATE | FC_WARM, 0x6, FH_DAWN, 3, 650, 0.95f, 120, 160, 0xE8E8F0FFu},
      {"GRANDMOTHER CRAB", FW_COAST, FC_COLD | FC_TEMPERATE | FC_WARM, 0xF, FH_DUSK | FH_NIGHT, 3, 550, 0.9f, 90, 130, 0xC04A2AFFu},
      {"THE BLUE TYRANT", FW_DEEP, FC_COLD | FC_TEMPERATE | FC_WARM, 0xF, FH_DAY, 3, 900, 1.0f, 300, 420, 0x2A4AB0FFu},
      {"THE PALE LANTERN", FW_MIST, FC_COLD | FC_TEMPERATE | FC_WARM, 0xF, FH_NIGHT | FH_DUSK, 3, 1200, 0.9f, 150, 220, 0xF0F8E0FFu},
      // (rare fish of the coasts: a warm one and a cold one)
      {"OPAH", FW_COAST | FW_DEEP, FC_TEMPERATE | FC_WARM, 0xF, FH_DAY | FH_DUSK, 2, 85, 0.7f, 60, 150, 0xC85A6AFFu},
      {"WOLF EEL", FW_COAST, FC_COLD | FC_TEMPERATE, 0xF, FH_DUSK | FH_NIGHT, 2, 70, 0.75f, 80, 200, 0x6A6A5AFFu},
  };
  return s;
}

const char* fishStory(int sp) {
  const std::vector<Species>& S = species();
  if (sp < 0 || sp >= (int)S.size() || S[(size_t)sp].rarity < 3) return "";
  const std::string n = S[(size_t)sp].name;
  if (n == "OLD WHISKERS") return "A CATFISH AS OLD AS THE LAKE. THEY SAY IT ONCE PULLED A FERRYMAN UNDER.";
  if (n == "THE SILVER KING") return "THE SALMON THAT CLIMBS EVERY FALL. NO NET HAS EVER HELD IT.";
  if (n == "GRANDMOTHER CRAB") return "THE FISHWIVES LEAVE HER A SHARE OF EVERY CATCH. SHE TAKES FINGERS OTHERWISE.";
  if (n == "THE BLUE TYRANT") return "A MARLIN THE LENGTH OF A SLOOP. IT HAS SNAPPED A HUNDRED LINES.";
  if (n == "THE PALE LANTERN") return "A FISH THAT GLOWS IN THE MIST. SAILORS SAY IT LEADS LOST SHIPS HOME.";
  return "";
}

uint8_t hourBits(float h) {
  if (h >= 5.0f && h < 8.0f) return FH_DAWN;
  if (h >= 8.0f && h < 18.0f) return FH_DAY;
  if (h >= 18.0f && h < 21.0f) return FH_DUSK;
  return FH_NIGHT;
}
uint8_t climateOf(float t) { return t < 0.36f ? FC_COLD : t > 0.64f ? FC_WARM : FC_TEMPERATE; }

int legendaryOf(uint8_t water) {
  const std::vector<Species>& S = species();
  for (int i = 0; i < (int)S.size(); i++) if (S[(size_t)i].rarity == 3 && (S[(size_t)i].waters & water)) return i;
  return -1;
}

int pickSpecies(uint8_t water, uint8_t climate, int season, float hour, float roll, const std::vector<Catch>& caught) {
  const std::vector<Species>& S = species();
  const uint8_t hb = hourBits(hour);
  static const float kWeight[4] = {100.0f, 34.0f, 7.0f, 1.0f};
  float total = 0;
  float w[64] = {};
  const int n = std::min((int)S.size(), 64);
  for (int i = 0; i < n; i++) {
    const Species& s = S[(size_t)i];
    if (!(s.waters & water) || !(s.climate & climate) || !(s.seasons & (1 << (season & 3)))) continue;
    float k = kWeight[std::min<int>(s.rarity, 3)];
    if (!(s.hours & hb)) { if (s.rarity >= 2) continue; k *= 0.2f; }   // (the common ones still bite now and then off their hour)
    if (s.rarity == 3) {   // one of a kind: caught once, never again
      bool had = false;
      for (const Catch& c : caught) if (c.species == i && c.count) had = true;
      if (had) continue;
    }
    w[i] = k;
    total += k;
  }
  if (total <= 0) return -1;
  float r = std::clamp(roll, 0.0f, 0.99999f) * total;
  for (int i = 0; i < n; i++) {
    if (w[i] <= 0) continue;
    if (r < w[i]) return i;
    r -= w[i];
  }
  for (int i = n - 1; i >= 0; i--) if (w[i] > 0) return i;
  return -1;
}

// ---- the shipwright's book
const std::vector<UpgradeInfo>& upgrades() {
  static const std::vector<UpgradeInfo> u = {
      // bit, name, price, hulls (bit per Hull: 1 rowboat, 2 sloop, 4 galleon)
      {UP_PLATING, "IRON PLATING", 900, 7},
      {UP_SAILS, "NEW SAILS", 700, 6},
      {UP_HOLD, "A DEEPER HOLD", 600, 6},
      {UP_KEEL, "A STORM KEEL", 2400, 2},
      {UP_LANTERN, "A BOW LANTERN", 150, 7},
      {UP_FIGUREHEAD, "A FIGUREHEAD", 500, 6},
  };
  return u;
}
float maxHp(const Boat& b) { return hullInfo(b.hull).hull * ((b.upgrades & UP_PLATING) ? 1.5f : 1.0f); }
float boatSpeed(const Boat& b) { return hullInfo(b.hull).speed * ((b.upgrades & UP_SAILS) ? 1.15f : 1.0f); }
bool sailsStorm(const Boat& b) { return (hullInfo(b.hull).waters & W_STORM) || (b.hull == Hull::Sloop && (b.upgrades & UP_KEEL)); }
int hullPrice(Hull h, bool sailor) { const int p = hullInfo(h).price; return sailor ? p * 85 / 100 : p; }
int raisePrice(Hull h, bool sailor) { return hullPrice(h, sailor) * 40 / 100; }
int repairPrice(const Boat& b) { return std::max(5, (int)std::lround((maxHp(b) - b.hp) * 2.0f)); }
int mapPrice(int sites) { return std::clamp(200 + sites * 40, 200, 1500); }

int Sea::ownedAt(ew::Gid dock) const {
  for (int i = 0; i < (int)boats.size(); i++)
    if (!boats[(size_t)i].ferry && !boats[(size_t)i].sunk && boats[(size_t)i].dock == dock && i != aboard) return i;
  return -1;
}
int Sea::owned(Hull h) const {
  for (int i = 0; i < (int)boats.size(); i++)
    if (!boats[(size_t)i].ferry && !boats[(size_t)i].sunk && boats[(size_t)i].hull == h) return i;
  return -1;
}

int fareOf(int tiles) { return std::clamp(20 + tiles / 25, 20, 400); }

Sea::Wind windAt(uint64_t seed, float gx, float gy, double hours) {
  // a slow field: the prevailing wind turns over days, gusts over an hour, and varies over a few thousand tiles
  const uint32_t s = (uint32_t)(seed ^ (seed >> 32)) ^ 0x57494E44u;
  const float days = (float)(hours / 24.0);
  const float base = fbm(days * 0.35f, 0.5f, s, 3) * 6.2831853f * 1.6f;
  const float local = (vnoise(gx / 2400.0f, gy / 2400.0f, s + 7) - 0.5f) * 1.4f;
  const float gust = vnoise((float)hours * 0.8f, gx / 900.0f, s + 13);
  Sea::Wind w;
  w.dir = std::fmod(base + local, 6.2831853f);
  if (w.dir < 0) w.dir += 6.2831853f;
  w.speed = std::clamp(0.25f + 0.75f * gust, 0.0f, 1.0f);
  return w;
}

float windFactor(Hull h, float heading, const Sea::Wind& w) {
  const HullInfo& H = hullInfo(h);
  if (H.windGain <= 0) return 1.0f;   // oars
  // (SEA lane) the points of sail: within 35 degrees of the wind's eye a sail only luffs (in irons: a crawl, so the
  // helmsman tacks), close-hauled (35..65) it draws more and more, a beam reach is the hull's speed, and running before
  // the wind adds windGain. A light air narrows it all toward the hull's own way.
  const float c = std::cos(heading - w.dir);   // 1 astern (the wind behind), -1 head-on
  const float off = std::acos(std::clamp(-c, -1.0f, 1.0f)) * 57.29578f;   // degrees off the wind's eye (0 head-on)
  float f;
  if (off < 35.0f) f = 0.12f + 0.10f * off / 35.0f;
  else if (off < 65.0f) { const float k = (off - 35.0f) / 30.0f; f = 0.22f + 0.78f * k * (2.0f - k); }
  else f = 1.0f;
  const float astern = std::max(0.0f, c) * w.speed * H.windGain / H.speed;
  // a light wind: less to fear head-on, less to gain astern
  f = 1.0f + (f - 1.0f) * (0.45f + 0.55f * w.speed);
  return std::max(0.12f, f + astern);
}

const LandRecord* Sea::land(uint32_t id) const {
  auto it = std::lower_bound(lands.begin(), lands.end(), id, [](const LandRecord& r, uint32_t v) { return r.land < v; });
  return it != lands.end() && it->land == id ? &*it : nullptr;
}
LandRecord& Sea::landMut(uint32_t id) {
  auto it = std::lower_bound(lands.begin(), lands.end(), id, [](const LandRecord& r, uint32_t v) { return r.land < v; });
  if (it != lands.end() && it->land == id) return *it;
  LandRecord r;
  r.land = id;
  return *lands.insert(it, r);
}
bool Sea::hasChart(uint32_t landId) const { const LandRecord* r = land(landId); return r && (r->flags & LF_CHART); }
bool Sea::knowsPort(ew::Gid site) const { return std::binary_search(portsKnown.begin(), portsKnown.end(), site); }

// ---- the block (Game's sea block, SAVE_VER 21): its own version byte first
// Layout (block v1): ver u8; boats u32 n x {id u32, hull u8, name str, culture u64, dock u64, gx i32, gy i32,
// heading f32, hp f32, upgrades u16, sunk u8}; aboard i32; portsKnown u32 n x u64; lands u32 n x {land u32, chain u8,
// flags u8, contactDay i32}; catches u32 n x {species u16, count u16, bestCm u16}; nextBoatId u32; fishCaught u32;
// voyages u16.
// (block v2, SEA lane) each boat adds flags u8 (bit 0 ferry) after sunk; after voyages: voyage on u8, and when on {from
// u64, to u64, t f32, dur f32, gold i32, event u8, toX i32, toY i32, eventAt f32, phase u8, hours f32, course f32}.
// A v1 block still loads (no ferries, no voyage).
static constexpr uint8_t SEA_BLOCK_VER = 2;
void Sea::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(SEA_BLOCK_VER);
  w.u32((uint32_t)boats.size());
  for (const Boat& b : boats) {
    w.u32(b.id); w.u8((uint8_t)b.hull); w.str(b.name); w.u64(b.culture); w.u64(b.dock); w.i32(b.gx); w.i32(b.gy);
    w.f32(b.heading); w.f32(b.hp); w.u16(b.upgrades); w.u8(b.sunk ? 1 : 0);
    w.u8(b.ferry ? 1 : 0);
  }
  w.i32(aboard);
  w.u32((uint32_t)portsKnown.size());
  for (ew::Gid p : portsKnown) w.u64(p);
  w.u32((uint32_t)lands.size());
  for (const LandRecord& r : lands) { w.u32(r.land); w.u8((uint8_t)r.chain); w.u8(r.flags); w.i32(r.contactDay); }
  w.u32((uint32_t)catches.size());
  for (const Catch& c : catches) { w.u16(c.species); w.u16(c.count); w.u16(c.bestCm); }
  w.u32(nextBoatId); w.u32(fishCaught); w.u16(voyages);
  const Voyage& v = live.voyage;
  w.u8(v.on ? 1 : 0);
  if (v.on) {
    w.u64(v.from); w.u64(v.to); w.f32(v.t); w.f32(v.dur); w.i32(v.gold); w.u8(v.event);
    w.i32(v.toX); w.i32(v.toY); w.f32(v.eventAt); w.u8(v.phase); w.f32(v.hours); w.f32(v.course);
  }
}

bool Sea::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  const uint8_t ver = r.u8();
  if (ver < 1 || ver > SEA_BLOCK_VER || r.bad) return false;
  Sea S;
  auto fits = [&](uint32_t n, size_t minBytes) { return !r.bad && (uint64_t)n * minBytes <= (uint64_t)(in.size() - std::min(in.size(), r.p)); };
  const uint32_t nb = r.u32();
  if (!fits(nb, 40)) return false;
  for (uint32_t i = 0; i < nb && !r.bad; i++) {
    Boat b;
    b.id = r.u32();
    const uint8_t h = r.u8();
    if (h >= (uint8_t)Hull::COUNT) return false;
    b.hull = (Hull)h;
    b.name = r.str(); b.culture = r.u64(); b.dock = r.u64(); b.gx = r.i32(); b.gy = r.i32();
    b.heading = r.f32(); b.hp = r.f32(); b.upgrades = r.u16(); b.sunk = r.u8() != 0;
    if (ver >= 2) b.ferry = (r.u8() & 1) != 0;
    if (!std::isfinite(b.heading)) b.heading = 0;
    if (!std::isfinite(b.hp) || b.hp < 0) b.hp = 0;
    S.boats.push_back(std::move(b));
  }
  S.aboard = r.i32();
  if (S.aboard < -1 || S.aboard >= (int)S.boats.size()) S.aboard = -1;
  const uint32_t np = r.u32();
  if (!fits(np, 8)) return false;
  for (uint32_t i = 0; i < np && !r.bad; i++) S.portsKnown.push_back(r.u64());
  std::sort(S.portsKnown.begin(), S.portsKnown.end());
  const uint32_t nl = r.u32();
  if (!fits(nl, 10)) return false;
  for (uint32_t i = 0; i < nl && !r.bad; i++) {
    LandRecord L;
    L.land = r.u32();
    const uint8_t c = r.u8();
    L.chain = c < (uint8_t)Chain::COUNT ? (Chain)c : Chain::None;
    L.flags = r.u8(); L.contactDay = r.i32();
    S.lands.push_back(L);
  }
  std::sort(S.lands.begin(), S.lands.end(), [](const LandRecord& a, const LandRecord& b) { return a.land < b.land; });
  const uint32_t nc = r.u32();
  if (!fits(nc, 6)) return false;
  for (uint32_t i = 0; i < nc && !r.bad; i++) { Catch c; c.species = r.u16(); c.count = r.u16(); c.bestCm = r.u16(); S.catches.push_back(c); }
  S.nextBoatId = r.u32(); S.fishCaught = r.u32(); S.voyages = r.u16();
  if (ver >= 2 && r.u8()) {
    Voyage& v = S.live.voyage;
    v.on = true;
    v.from = r.u64(); v.to = r.u64(); v.t = r.f32(); v.dur = r.f32(); v.gold = r.i32(); v.event = r.u8();
    v.toX = r.i32(); v.toY = r.i32(); v.eventAt = r.f32(); v.phase = r.u8(); v.hours = r.f32(); v.course = r.f32();
    if (!std::isfinite(v.t) || !std::isfinite(v.dur) || !std::isfinite(v.eventAt) || !std::isfinite(v.hours) || !std::isfinite(v.course) || v.phase > 3)
      v = Voyage();
  }
  if (r.bad) return false;
  *this = std::move(S);
  return true;
}

}  // namespace sea
