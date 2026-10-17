// M12 "Rise and Fall": the lordship block's data, tables and persistence (rpg/sim/lordship.h). LORD lane (phase A by
// the lead; phase B: the rules of taxes and loyalty, charters by prosperity, the block v2 with the hall's new state).
#include "rpg/sim/lordship.h"
#include <algorithm>
#include <cmath>
#include "rpg/sim/common.h"
#include "rpg/sim/world.h"

namespace lord {
namespace {
// v1: phase A's layout (tests/fixtures/save_v22.bin); v2: phase B appends the hall's state and each holding's flags
constexpr uint8_t LORD_BLOCK_VER = 2;
const WorkInfo kWorks[] = {
    {LW_BANNER, "THE LORD'S BANNER", 20, 1},  {LW_PALISADE, "A PALISADE", 300, 6}, {LW_HALL, "A LORD'S HALL", 600, 10},
    {LW_GRANARY, "A GRANARY", 250, 5},        {LW_MILITIA, "A MILITIA", 200, 4},   {LW_MARKET, "A MARKET CROSS", 350, 6},
    {LW_WATCHTOWER, "A WATCHTOWER", 280, 5},  {LW_WALLS, "STONE WALLS", 1500, 20}, {LW_SHRINE, "A SHRINE", 220, 4},
};
}  // namespace

const WorkInfo* workInfo(uint16_t bit) {
  for (const WorkInfo& w : kWorks) if (w.bit == bit) return &w;
  return nullptr;
}
int charterPrice(uint8_t siteType) { return siteType == (uint8_t)SiteType::Village ? 2500 : 0; }

// ---------------------------------------------------------------- phase B rules
const char* taxWord(uint8_t taxPct) { return taxPct <= TAX_LOW ? "LOW" : taxPct >= TAX_HEAVY ? "HEAVY" : "FAIR"; }

int charterPriceFor(uint8_t siteType, int prosperity, int popPct) {
  const int base = charterPrice(siteType);
  if (!base) return 0;
  // a thriving, full village costs more (prosperity 50 and a full census: the phase A 2500)
  const int p = std::clamp(prosperity, 0, 100), pop = std::clamp(popPct, 10, 150);
  return std::clamp(base * (60 + p) / 110 * (50 + pop) / 150 / 10 * 10, 1500, 3500);
}

int taxPerDay(const Holding& h, int popPct, int prosperity) {
  const int pros = std::min(100, prosperity + ((h.works & LW_MARKET) ? 15 : 0));
  // a village of a hundred souls at a fair tenth: about 3 gold a day (about 200 in 60 days)
  int g = (int)std::lround(popPct * (0.5f + pros / 100.0f) * h.taxPct / 300.0f);
  if (h.flags & HF_UNREST) g /= 2;   // unrest: half the taxes never reach the hall
  return std::max(0, g);
}

int loyaltyTarget(const Holding& h) {
  // low tax: love; heavy: grumbling toward unrest (5% -> 80, 10% -> 65, 20% -> 35)
  int t = 80 - 3 * ((int)h.taxPct - TAX_LOW);
  if (h.works & LW_SHRINE) t += 6;
  if (h.works & LW_MARKET) t += 4;
  if (h.works & LW_BANNER) t += 2;
  if (h.works & (LW_PALISADE | LW_WALLS)) t += 3;   // a lord who keeps them safe
  if (h.works & LW_HALL) t += 3;
  return std::clamp(t, 5, 95);
}

uint16_t workNeeds(uint16_t bit) {
  if (bit == LW_WALLS) return LW_PALISADE | LW_WATCHTOWER;
  if (bit == LW_HALL) return LW_BANNER;
  return 0;
}

Holding* Lordship::holding(Gid site) {
  for (Holding& h : holdings) if (h.site == site) return &h;
  return nullptr;
}
const Holding* Lordship::holding(Gid site) const {
  for (const Holding& h : holdings) if (h.site == site) return &h;
  return nullptr;
}

// Layout (LORD_BLOCK_VER 1): ver u8, treasury i32, liege u64, tributeDue i32, taxSinceTribute i32, tributeDay u16, missed u8, realm u64,
// realmName str, independenceDay u16, nextTask u32; holdings (u32 n: site u64, how u8, since u16, tax u8, loyalty u8,
// works u16, building u16, buildDone u16, militia u8, lastTaxDay i32, steward u64).
// v2 appends after each holding: flags u8, raidDay u16, task u32, taskDay u16; and after the holdings: withhold u8,
// crowned u8, weekTax i32, weekDay u16, wrathDay u16, wrath u8, siege u32, march u32, peaceOffer u8, peaceDay u16.
void Lordship::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(LORD_BLOCK_VER);
  w.i32(treasury); w.u64(liege); w.i32(tributeDue); w.i32(taxSinceTribute); w.u16(tributeDay); w.u8(missed); w.u64(realm); w.str(realmName);
  w.u16(independenceDay); w.u32(nextTask);
  w.u32((uint32_t)holdings.size());
  for (const Holding& h : holdings) {
    w.u64(h.site); w.u8((uint8_t)h.how); w.u16(h.sinceDay); w.u8(h.taxPct); w.u8(h.loyalty); w.u16(h.works);
    w.u16(h.building); w.u16(h.buildDone); w.u8(h.militia); w.i32(h.lastTaxDay); w.u64(h.steward);
    w.u8(h.flags); w.u16(h.raidDay); w.u32(h.task); w.u16(h.taskDay);
  }
  w.u8(withhold); w.u8(crowned); w.i32(weekTax); w.u16(weekDay); w.u16(wrathDay); w.u8(wrath); w.u32(siege); w.u32(march);
  w.u8(peaceOffer); w.u16(peaceDay);
}

bool Lordship::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  const uint8_t ver = r.u8();
  if (r.bad || ver < 1 || ver > LORD_BLOCK_VER) return false;
  Lordship L;
  L.treasury = r.i32(); L.liege = r.u64(); L.tributeDue = r.i32(); L.taxSinceTribute = r.i32(); L.tributeDay = r.u16(); L.missed = r.u8(); L.realm = r.u64();
  L.realmName = r.str(); L.independenceDay = r.u16(); L.nextTask = r.u32();
  const uint32_t n = r.u32();
  if (r.bad || (uint64_t)n * 32 > (uint64_t)(in.size() - std::min(in.size(), r.p))) return false;
  for (uint32_t i = 0; i < n && !r.bad; i++) {
    Holding h;
    h.site = r.u64(); h.how = (How)std::min<uint8_t>(r.u8(), (uint8_t)How::COUNT - 1); h.sinceDay = r.u16();
    h.taxPct = std::min<uint8_t>(r.u8(), 30); h.loyalty = std::min<uint8_t>(r.u8(), 100); h.works = r.u16();
    h.building = r.u16(); h.buildDone = r.u16(); h.militia = r.u8(); h.lastTaxDay = r.i32(); h.steward = r.u64();
    if (ver >= 2) { h.flags = r.u8(); h.raidDay = r.u16(); h.task = r.u32(); h.taskDay = r.u16(); }
    L.holdings.push_back(h);
  }
  if (ver >= 2) {
    L.withhold = r.u8(); L.crowned = r.u8(); L.weekTax = r.i32(); L.weekDay = r.u16(); L.wrathDay = r.u16(); L.wrath = r.u8();
    L.siege = r.u32(); L.march = r.u32(); L.peaceOffer = r.u8(); L.peaceDay = r.u16();
  }
  if (r.bad || r.p != in.size()) return false;
  *this = std::move(L);
  return true;
}

}  // namespace lord
