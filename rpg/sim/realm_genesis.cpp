// M4 "Banners": genesis and the level of detail (VISION_PLAN 4.2, 4.4; rpg/sim/realm.h). REALM lane.
// A kingdom is instantiated when its seat lies within `activeRadius` (3) kingdom cells of the player: its settlements
// come from the cheap lattice (EndlessSource::settlementsIn over the 3 x 3 kingdom cells round its seat: no region
// plans), its population, wealth and military from their sizes and its culture's values, its government, titles and
// inheritance from its society, a ruler with traits, a granary and a harvest day. At most `maxActive` (48) realms are
// active, nearest first; the others are dormant and catch up with min(daysAway / 7, 50) weekly ticks at 7x rates when
// they come back. Kingdoms that never came near are latent (never built).
// Instantiation is time-sliced (focusBudgetMs per call: the web has no threads): reading one kingdom cell's
// settlements costs 1-4 ms the first time, so focus does a few cells per step, nearest kingdom first.
// Genesis is a pure function of the seed and the generator's plans (any order of instantiation gives the same realms).
#include <algorithm>
#include <cmath>
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/realm.h"
#include "rpg/world/source.h"

namespace realm {
namespace {

uint64_t cellKey(int32_t cx, int32_t cy) { return ((uint64_t)(uint32_t)cx << 32) | (uint32_t)cy; }
int cheb(int32_t ax, int32_t ay, int32_t bx, int32_t by) { return std::max(std::abs(ax - bx), std::abs(ay - by)); }

}  // namespace

// one quarter of a kingdom cell at a time (a whole cell's first read costs 2-8 ms; a quarter keeps a step near budget).
// scanned_ holds the quarters done (bits 0-3); true once the cell is complete.
bool Realm::scanCell(ew::EndlessSource& src, int32_t cx, int32_t cy) {
  uint8_t& done = scanned_[cellKey(cx, cy)];
  if (done == 15) return true;
  int q = 0;
  while (done & (1 << q)) q++;
  done = (uint8_t)(done | (1 << q));
  stats.scans++;
  const int32_t h = ew::KCELL / 2;
  const int32_t x0 = cx * ew::KCELL - h + (q & 1) * h, y0 = cy * ew::KCELL - h + (q >> 1) * h;
  for (const ew::SettlementNode& n : src.settlementsIn(x0, y0, x0 + h, y0 + h))
    noteSite(n.id, n.kingdom, (uint8_t)n.type, n.x, n.y);
  return done == 15;
}

void Realm::recount(KingdomState& k) {
  float pop = 0, wsum = 0, ysum = 0;
  Gid best = 0;
  int bestRank = -1;
  for (Gid g : k.settlements) {
    const SettlementState* s = settlement(g);
    if (!s) continue;
    if (s->flags & (SS_ABANDONED | SS_RUINED)) continue;
    pop += detail::sitePop(s->type) * s->popPct / 100.0f;
    const float w = s->type == (uint8_t)SiteType::City ? 3.0f : s->type == (uint8_t)SiteType::Town ? 1.5f : 1.0f;
    wsum += w;
    ysum += w * s->yield / 100.0f;
    const int rank = (2 - std::min<int>(2, s->type)) * 4 + (g == k.capital ? 1 : 0);
    if (rank > bestRank) { bestRank = rank; best = g; }
  }
  k.pop = pop;
  // a cell's own land (the countryside round its seats) evens the yield out: 0.85 .. 1.25
  k.yieldRatio = wsum > 0 ? std::clamp(0.2f + 0.8f * ysum / wsum, 0.8f, 1.3f) : 1.0f;
  if (best && (k.capital == 0 || std::find(k.settlements.begin(), k.settlements.end(), k.capital) == k.settlements.end())) k.capital = best;
  // the capital first (the contract)
  auto it = std::find(k.settlements.begin(), k.settlements.end(), k.capital);
  if (it != k.settlements.end() && it != k.settlements.begin()) std::rotate(k.settlements.begin(), it, it + 1);
}

void Realm::instantiate(ew::EndlessSource& src, Gid id) {
  if (kingdom(id)) return;
  const ew::KingdomPlan* kp = src.kingdom(id);
  if (!kp) return;
  KingdomState k;
  k.id = id;
  k.capital = kp->capital;
  k.culture = kp->culture;
  k.name = kp->name;
  k.color = kp->color; k.color2 = kp->color2; k.emblem = kp->emblem;
  k.tier = Tier::Dormant;
  const uint64_t h0 = ew::mix64(id ^ seed_);
  k.foundedDay = -(Realm::YEAR * (40 + (int)(h0 % 161)));
  const cult::Culture& C = src.culture(kp->culture);
  const cult::Society S = cult::societyOf(C);
  k.gov = (uint8_t)S.government;
  k.inherit = (uint8_t)S.inheritance;
  for (int v = 0; v < V_COUNT; v++) k.values[v] = C.values[v];
  const uint64_t h = ew::mix64(id ^ seed_ ^ 0x52554C45ull);
  k.ruler.female = S.inheritance == cult::Inheritance::Matrilineal ? (h & 7) != 0 : (h & 3) == 0;
  k.ruler.name = cult::personName(C, (uint32_t)(h >> 8), k.ruler.female);
  k.ruler.title = S.rulerTitle ? S.rulerTitle : "KING";
  k.ruler.house = "HOUSE " + cult::personName(C, (uint32_t)(h >> 24) ^ 0x484F5553u, false);
  k.ruler.age = (uint8_t)(24 + (h >> 20) % 40);
  k.ruler.traits = detail::traitsFor(k.values, ew::mix64(h));
  k.ruler.dynasty = ew::mix64(h);
  for (const SettlementState& s : settlements_)
    if (s.owner == id) k.settlements.push_back(s.site);
  std::sort(k.settlements.begin(), k.settlements.end());
  recount(k);
  const float merc = k.values[V_MERCANTILE] / 255.0f, mart = k.values[V_MARTIAL] / 255.0f;
  k.wealth = k.pop * (0.6f + merc);
  k.military = std::max(1.0f, k.pop * (0.6f + 1.2f * mart));
  k.stability = std::clamp(0.58f + 0.15f * k.values[V_HONOUR] / 255.0f + (float)((h >> 40) % 10) / 100.0f, 0.3f, 0.9f);
  k.food = 0.45f + (float)((h0 >> 20) % 50) / 100.0f;
  k.harvestDoy = (uint16_t)(165 + (h0 >> 32) % 60);
  const int year = day_ / Realm::YEAR, doy = day_ % Realm::YEAR;
  k.harvestYear = (int16_t)(doy >= k.harvestDoy ? year : year - 1);   // this year's harvest is in when past its day
  k.lastTick = (uint16_t)std::clamp(day_, 0, 65535);
  kingdoms_.push_back(std::move(k));
  rebuildIndex();
  linkRelations(src, id);
  histQueue_.push_back(id);
  stats.instantiated = (int)kingdoms_.size();
}

// relations with the realms already instantiated nearby (VISION_PLAN 4.2: Contact with genesis neighbours, Unknown
// beyond). Every value is a pure function of the pair, so the order of instantiation does not matter.
void Realm::linkRelations(ew::EndlessSource& src, Gid id) {
  const KingdomState* K = kingdom(id);
  if (!K) return;
  const int32_t kx = ew::EndlessSource::kcellOf(capX(*K)), ky = ew::EndlessSource::kcellOf(capY(*K));
  const cult::Culture& CA = src.culture(K->culture);
  std::vector<Gid> others;
  for (const KingdomState& o : kingdoms_)
    if (o.id != id && !o.fallen) others.push_back(o.id);
  for (Gid oid : others) {
    const KingdomState* O = kingdom(oid);
    const KingdomState* A = kingdom(id);
    if (!O || !A) continue;
    const int d = cheb(kx, ky, ew::EndlessSource::kcellOf(capX(*O)), ew::EndlessSource::kcellOf(capY(*O)));
    if (d > 2 || relation(id, oid)) continue;
    const cult::Culture& CB = src.culture(O->culture);
    const Gid lo = std::min(id, oid), hi = std::max(id, oid);
    const uint64_t h = ew::mix64(lo * 31 + hi ^ seed_ ^ 0x52454C41ull);
    // shared border: settlement pairs within 700 tiles
    int border = 0;
    for (Gid ga : A->settlements) {
      const SettlementState* sa = settlement(ga);
      if (!sa) continue;
      for (Gid gb : O->settlements) {
        const SettlementState* sb = settlement(gb);
        if (!sb) continue;
        const int64_t dx = sa->gx - sb->gx, dy = sa->gy - sb->gy;
        if (dx * dx + dy * dy < 700ll * 700) border++;
      }
    }
    const bool sameFaith = CA.faith.kind == CB.faith.kind && !CA.faith.names.empty() && !CB.faith.names.empty() &&
                           CA.faith.names[0] == CB.faith.names[0];
    int base = (int)std::lround(-40.0f * cult::distance(CA, CB)) + (sameFaith ? 15 : 0) - 3 * std::min(border, 4) +
               (int)(h % 21) - 6;
    const float merc = (A->values[V_MERCANTILE] + O->values[V_MERCANTILE]) / 510.0f;
    const float ratioA = A->yieldRatio, ratioB = O->yieldRatio;
    const float iso = (A->values[V_ISOLATIONIST] + O->values[V_ISOLATIONIST]) / 510.0f;
    Relation* r = relMut(id, oid, true);
    r->border = (uint8_t)std::min(border, 255);
    if (d <= 1 || border > 0) {
      r->state = Rel::Contact;
      if ((float)((h >> 16) % 1000) / 1000.0f < 0.25f + 0.5f * merc - 0.3f * iso) { r->state = Rel::Trade; base += 10; }
      // a food deal where one side's harvests feed the other (15.6.3: what a failed harvest will break)
      const bool aSells = ratioA > 1.02f && ratioB < 1.0f, bSells = ratioB > 1.02f && ratioA < 1.0f;
      if ((aSells || bSells) && (h >> 32) % 100 < 70) {
        r->foodDeal = true;
        r->seller = aSells ? id : oid;
        r->state = Rel::Trade;
      }
    } else {
      r->state = Rel::Unknown;
    }
    r->base = (int8_t)std::clamp(base, -80, 60);
    r->opinion = r->base;
    r->sinceDay = (uint16_t)std::clamp(day_, 0, 65535);
    r->stepDay = r->sinceDay;
  }
}

void Realm::setTiers() {
  struct C { double d; int ix; };
  std::vector<C> cand;
  const int32_t px = ew::EndlessSource::kcellOf(focusGx_), py = ew::EndlessSource::kcellOf(focusGy_);
  for (size_t i = 0; i < kingdoms_.size(); i++) {
    const KingdomState& k = kingdoms_[i];
    if (k.fallen) continue;
    const int32_t sx = capX(k), sy = capY(k);
    if (cheb(px, py, ew::EndlessSource::kcellOf(sx), ew::EndlessSource::kcellOf(sy)) > activeRadius) continue;
    const double dx = sx - focusGx_, dy = sy - focusGy_;
    cand.push_back({dx * dx + dy * dy, (int)i});
  }
  std::sort(cand.begin(), cand.end(), [&](const C& a, const C& b) {
    return a.d != b.d ? a.d < b.d : kingdoms_[(size_t)a.ix].id < kingdoms_[(size_t)b.ix].id;
  });
  std::vector<uint8_t> want(kingdoms_.size(), 0);
  for (size_t i = 0; i < cand.size() && (int)i < maxActive; i++) want[(size_t)cand[i].ix] = 1;
  stats.active = stats.dormant = 0;
  for (size_t i = 0; i < kingdoms_.size(); i++) {
    KingdomState& k = kingdoms_[i];
    if (want[i]) {
      if (k.tier != Tier::Active) {
        // back from dormancy: catch up (VISION_PLAN 4.4)
        const int away = day_ - (int)k.lastTick;
        const int weeks = std::min(std::max(0, away) / 7, 50);
        if (weeks > 0) catchUp(k, weeks);
        k.lastTick = (uint16_t)std::clamp(day_, 0, 65535);
        k.tier = Tier::Active;
      }
      stats.active++;
    } else {
      if (k.tier == Tier::Active) k.lastTick = (uint16_t)std::clamp(day_, 0, 65535);
      k.tier = k.fallen ? Tier::Latent : Tier::Dormant;
      if (!k.fallen) stats.dormant++;
    }
  }
}

void Realm::focus(ew::EndlessSource& src, int32_t gx, int32_t gy, int day) { focusImpl(src, gx, gy, day, focusBudgetMs); }
void Realm::focusNow(ew::EndlessSource& src, int32_t gx, int32_t gy, int day) { focusImpl(src, gx, gy, day, 1e18); }

void Realm::focusImpl(ew::EndlessSource& src, int32_t gx, int32_t gy, int day, double budgetMs) {
  src_ = &src;
  if (day_ == 0) day_ = std::max(0, day);
  const int32_t kx = ew::EndlessSource::kcellOf(gx), ky = ew::EndlessSource::kcellOf(gy);
  const bool moved = kx != focusKx_ || ky != focusKy_;
  if (!moved && !refocus_ && probe_.empty() && pending_.empty() && histQueue_.empty()) return;
  const double t0 = detail::nowMs();
  focusGx_ = gx; focusGy_ = gy;
  if (moved || refocus_) {
    focusKx_ = kx; focusKy_ = ky;
    refocus_ = false;
    // the kingdom cells in range, nearest first: probed (is there a kingdom? built yet?) a unit of work at a time
    struct P { int64_t d; int32_t cx, cy; };
    std::vector<P> cells;
    for (int32_t cy = ky - activeRadius; cy <= ky + activeRadius; cy++)
      for (int32_t cx = kx - activeRadius; cx <= kx + activeRadius; cx++) {
        const int64_t dx = (int64_t)cx * ew::KCELL - gx, dy = (int64_t)cy * ew::KCELL - gy;
        cells.push_back({dx * dx + dy * dy, cx, cy});
      }
    std::sort(cells.begin(), cells.end(), [](const P& a, const P& b) { return a.d != b.d ? a.d < b.d : (a.cy != b.cy ? a.cy < b.cy : a.cx < b.cx); });
    probe_.clear();
    for (const P& p : cells) probe_.push_back(((uint64_t)(uint32_t)p.cx << 32) | (uint32_t)p.cy);
    pending_.clear();
    setTiers();
    // the history memo after a load (warmed a kingdom per unit of work below)
    for (const KingdomState& k : kingdoms_) if (!k.rebel && !hist_.count(k.id)) histQueue_.push_back(k.id);
  }
  // probe the cells (nearest first), then instantiate nearest first, until the budget is spent (at least one unit of
  // work per call)
  bool worked = false;
  while (!probe_.empty() && !(worked && detail::nowMs() - t0 >= budgetMs)) {
    const uint64_t c = probe_.front();
    probe_.erase(probe_.begin());
    const double u0 = detail::nowMs();
    const Gid id = src.kingdomOfCell((int32_t)(uint32_t)(c >> 32), (int32_t)(uint32_t)c);
    if (id && !kingdom(id) && std::find(pending_.begin(), pending_.end(), id) == pending_.end()) pending_.push_back(id);
    stats.worstProbeMs = std::max(stats.worstProbeMs, detail::nowMs() - u0);
    worked = true;
  }
  if (!probe_.empty()) { stats.lastFocusMs = detail::nowMs() - t0; stats.worstFocusMs = std::max(stats.worstFocusMs, stats.lastFocusMs); return; }
  while (!pending_.empty()) {
    const Gid id = pending_.front();
    const int32_t sx = ew::idRx(id), sy = ew::idRy(id);
    bool ready = true;
    for (int32_t cy = sy - 1; cy <= sy + 1 && ready; cy++)
      for (int32_t cx = sx - 1; cx <= sx + 1; cx++) {
        for (;;) {
          auto it = scanned_.find(cellKey(cx, cy));
          if (it != scanned_.end() && it->second == 15) break;
          if (worked && detail::nowMs() - t0 >= budgetMs) { ready = false; break; }
          const double u0 = detail::nowMs();
          scanCell(src, cx, cy);
          stats.worstScanMs = std::max(stats.worstScanMs, detail::nowMs() - u0);
          worked = true;
        }
        if (!ready) break;
      }
    if (!ready) break;
    if (worked && detail::nowMs() - t0 >= budgetMs) break;
    if (planned_ != id) {
      // its plan and culture first, a unit of their own (the generator builds the kingdom's dialect: the dearest part)
      const double p0 = detail::nowMs();
      if (const ew::KingdomPlan* kp = src.kingdom(id)) (void)src.culture(kp->culture);
      planned_ = id;
      stats.worstPlanMs = std::max(stats.worstPlanMs, detail::nowMs() - p0);
      worked = true;
      if (detail::nowMs() - t0 >= budgetMs) break;
    }
    const double u0 = detail::nowMs();
    instantiate(src, id);
    pending_.erase(pending_.begin());
    setTiers();
    stats.worstInstMs = std::max(stats.worstInstMs, detail::nowMs() - u0);
    worked = true;
    if (detail::nowMs() - t0 >= budgetMs) break;
  }
  // the history pre-roll, one kingdom per unit of work (chronicle() and history() warm on demand anyway)
  while (!histQueue_.empty() && !(worked && detail::nowMs() - t0 >= budgetMs)) {
    const Gid id = histQueue_.front();
    histQueue_.erase(histQueue_.begin());
    if (hist_.count(id)) continue;
    const double u0 = detail::nowMs();
    warmHistory(src, id);
    stats.worstHistMs = std::max(stats.worstHistMs, detail::nowMs() - u0);
    worked = true;
  }
  stats.lastFocusMs = detail::nowMs() - t0;
  stats.worstFocusMs = std::max(stats.worstFocusMs, stats.lastFocusMs);
}

}  // namespace realm
