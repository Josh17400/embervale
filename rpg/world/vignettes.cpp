// M2 Wayfinder: the wayside places (vignettes, rpg/world/poi.h), the wonders, and the start guarantee (VISION_PLAN 2.6,
// M2; PLAN.md task 4). WORLD lane.
//
// Vignettes are planned per region (planVignettes, called from buildRegion after the other sites and the dens): every
// kind has its own way of finding a place (beside a road, on high ground near one, at a forest's edge, on a lake shore,
// where a road bridges a river, a walk out from a village...), each place is checked against the land (one level, dry,
// off the roads) and kept clear of settlements (footprint + 8), sites and dens; then 3-6 kinds are taken per region
// (fewer where it is mostly sea), at most one of each, in an order the region's seed shuffles. Every decision reads only
// pure fields, the settlement lattice and the region's own roads, so the plan is the same whichever region came first.
// Hearts keep 14 tiles inside their region, so places from neighbouring regions never touch.
//
// Names are unique within any 3x3 block of regions by construction: a region's slot (rx mod 3, ry mod 3) picks a ninth
// of each name pool, and a region holds at most one vignette of a kind.
//
// Wonders sit on their own lattice (640 tiles, about one per 2.5 x 2.5 regions where the land suits one), at least 150
// tiles from any settlement, each in a clearing with a dirt track to the nearest road and a landmark label of its name.
// The start plan forces one within 400 tiles of the start when the lattice has none there, and forces enough places
// near the start village that at least 8 kinds of place lie within 120 tiles of its heart and 3 within 60, two of them
// beside the road to the story city.
//
// Layout parameters a stamp needs (which side of the road, where the water is) ride in the top byte of SitePlan::seed,
// so stampVignette works from the plan alone.
#include <algorithm>
#include <cstdint>
#include <string>
#include "rpg/world/gen.h"

namespace ew {
using namespace gen;
using art::Monster;
using art::Prop;

namespace {
constexpr int32_t VIG_MARGIN = 14;      // vignette hearts keep this far inside their region
constexpr int32_t WCELL = 640, WM = 110; // the wonder lattice and its margin (wonders >= 220 apart)
constexpr int DX4[4] = {1, 0, -1, 0}, DY4[4] = {0, 1, 0, -1};   // 0 east, 1 south, 2 west, 3 north

// the largest footprint half-sizes a settlement class may have (settlementFootprint plus archetype extras)
void nodeHalf(SiteType t, int32_t& hw, int32_t& hh) {
  if (t == SiteType::City) { hw = 100; hh = 80; }
  else if (t == SiteType::Town) { hw = 56; hh = 44; }
  else { hw = 33; hh = 25; }
}

int64_t segD2(int32_t x, int32_t y, int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  const int64_t vx = bx - ax, vy = by - ay, wx = x - ax, wy = y - ay;
  const int64_t L2 = vx * vx + vy * vy, t = wx * vx + wy * vy;
  if (L2 == 0 || t <= 0) return wx * wx + wy * wy;
  if (t >= L2) return dist2(x, y, bx, by);
  const int64_t cr = wx * vy - wy * vx;
  return cr * cr / L2;
}

// the nearest of the 4 directions to a vector
int dir4(int32_t vx, int32_t vy) {
  if (std::abs(vx) >= std::abs(vy)) return vx >= 0 ? 0 : 2;
  return vy >= 0 ? 1 : 3;
}

// ---- name pools. Person and word pools hold 9 x k entries: a region's slot picks entries slot, slot + 9, ...
const char* kFolk[] = {"BRAN", "ODA", "HALVARD", "MERRIC", "SIGNY", "TOVE", "ANSELM", "GRETA", "WULF",
                       "EDDA", "RORIK", "MAREN", "OSWIN", "HILDE", "TAMSIN", "BJORN", "ISOLDE", "CORM",
                       "ELSPETH", "GARRICK", "INGA", "AUDUN", "RHOSYN", "FENN", "OLWEN", "DAGNY", "HUGO",
                       "YRSA", "THORVALD", "ASTA", "GODRIC", "LISBET", "EINAR", "MABYN", "KETIL", "SOLVEIG",
                       "ALDRIC", "BERIT", "CEDRIC", "DORTE", "EGIL", "FRIDA", "GUNNAR", "HELKA", "IVOR",
                       "JORUNN", "KAI", "LIV", "MAGNUS", "NESSA", "ORM", "PERNILLE", "QUILL", "RAGNA",
                       "SVEN", "THYRA", "ULF", "VIGDIS", "WILLEM", "AGNES", "BALDER", "CATRIN", "DUNSTAN",
                       "EIRA", "FALK", "GISLA", "HAKON", "IDA", "JARVIS", "KARIN", "LEIF", "MOIRA",
                       "NJAL", "OTTILIE", "PIERS", "RUNA", "SEBALD", "TILDE", "UNNA", "VALDIS", "WYNN",
                       "ARNE", "BRYNJA", "COLM", "DAVINA", "ERLAND", "FENNA", "GORM", "HEDDA", "ISAK",
                       "JONAS", "KELDA", "LORCAN", "MILDA", "NIELS", "ORLA", "PADRAIG", "RIKKE", "STEIN",
                       "TORVI", "URSA", "VERN", "WENDA", "ALVAR", "BODIL", "CASPAR", "DEIRDRE", "EBBE"};
const char* kStoneAdj[] = {"WHISPERING", "SLEEPING", "WEEPING", "SINGING", "DANCING", "SILENT", "GREY", "HOLLOW", "MOSSY",
                           "NINE MAIDENS'", "WATCHING", "BROKEN", "SUNKEN", "OLD", "FROSTED", "WIND", "MOONLIT", "HUNGRY",
                           "PILGRIMS'", "KINGS'", "WIDOWS'", "DREAMING", "RINGING", "HOODED", "LANTERN", "SHEPHERDS'", "GIANTS'"};
const char* kTollHead[] = {"TROLLBRIDGE", "THE TROLL'S CROSSING", "TOLLGATE", "THE OLD TOLL", "GRIMBRIDGE",
                           "THE HUNGRY BRIDGE", "TROLLFORD", "THE TOLLKEEPER'S SPAN", "UNDERBRIDGE"};
const char* kRiverA[] = {"SILVER", "BLACK", "GREY", "WHITE", "RED", "COLD", "SWIFT", "STILL", "DEEP",
                         "WILLOW", "ALDER", "OTTER", "SALMON", "STONE", "MOSS", "AMBER", "IRON", "HOLLY",
                         "RAVEN", "MIST", "THORN", "ELDER", "HARE", "FERN", "ASH", "HAZEL", "WOLF"};
const char* kRiverB[] = {"RUN", "WATER", "FLOW", "BURN", "BECK", "WASH", "RUSH", "BROOK", "WEND"};
const char* kWonderPlace[] = {"ELDHOLM", "VARRAK", "MORNVALE", "ISENGARTH", "THULE", "AVERNOS", "KESSRA", "DUNMARROW", "SKALD",
                              "OSTMERE", "HALLOW", "BRYNDOR", "CALDERA", "VESPER", "ORMSKIRK", "TARNHOLD", "ILLYR", "GAUNT",
                              // (the start plan's own wonder draws from these: no lattice wonder shares its name)
                              "ARDENHOLM", "SILVERDALE", "WYNMOOR", "HEARTHVALE", "GREYMARCH", "OAKENSHAW", "BRIGHTWATER", "STAGHOLT", "KINGSREACH"};

std::string folkName(int slot, uint32_t sd) { return kFolk[(size_t)slot + 9 * (sd % 12u)]; }
}  // namespace

// ============================================================== names
std::string EndlessSource::Impl::riverName(uint32_t river) {
  // the river's spring cell picks the first word's ninth: rivers rising near each other never share a name
  const int slot = (int)floorMod(riverCellX(river), 3) + 3 * (int)floorMod(riverCellY(river), 3);
  const uint64_t h = mix64(seed ^ tag("river.name") ^ river);
  return std::string(kRiverA[(size_t)slot + 9 * (h % 3)]) + kRiverB[(h >> 8) % 9];
}

// (instance: the first or second place of its kind in its region; the names of the two always differ)
std::string EndlessSource::Impl::vignetteTitle(VignetteKind k, int32_t x, int32_t y, uint32_t sd, uint32_t river, int instance) {
  const int slot = (int)floorMod(regionOf(x), 3) + 3 * (int)floorMod(regionOf(y), 3);
  const uint32_t v = (uint32_t)(mix64(sd ^ 0x7177u) >> 20);
  // one person per kind and instance in a region (kinds 0-8, second places 9-17, twelve to a slot): never two the same
  const uint32_t rb = (uint32_t)(mix64(seed ^ tag("vig.names") ^ key2(regionOf(x), regionOf(y))) >> 24) + (uint32_t)instance;
  const std::string who = folkName(slot, rb + (uint32_t)k + 9u * (uint32_t)instance);
  switch (k) {
    case VignetteKind::Caravan: {
      static const char* t[] = {"'S LAST WAGON", "'S WRECKED WAGON", "'S CARAVAN"};
      return who + t[(v >> 4) % 3];
    }
    case VignetteKind::HunterCamp: {
      const uint32_t q = (v >> 4) % 3;
      return q == 0 ? "OLD " + who + "'S CAMP" : q == 1 ? who + "'S HUNTING CAMP" : who + "'S LODGE";
    }
    case VignetteKind::StandingStones: return std::string("THE ") + kStoneAdj[(size_t)slot + 9 * (rb % 3)] + " STONES";
    case VignetteKind::Watchtower: {
      const uint32_t q = (v >> 4) % 3;
      return q == 0 ? who + "'S WATCH" : q == 1 ? who + "'S TOWER" : "THE WATCH OF " + who;
    }
    case VignetteKind::FishingHut: {
      static const char* t[] = {"'S LANDING", "'S JETTY", "'S FISHING HUT"};
      return who + t[(v >> 4) % 3];
    }
    case VignetteKind::TollBridge: return std::string(kTollHead[slot]) + " ON THE " + riverName(river);
    case VignetteKind::HerbGarden: {
      static const char* t[] = {"'S HERB GARDEN", "'S GARDEN", "'S PHYSIC GARDEN"};
      return who + t[(v >> 4) % 3];
    }
    case VignetteKind::LoneGrave: return (v >> 4) & 1 ? "THE GRAVE OF " + who : who + "'S CAIRN";
    case VignetteKind::Wayrest: {
      static const char* t[] = {"'S REST", "'S FIRE", "'S WAYREST"};
      return who + t[(v >> 4) % 3];
    }
    default: return vignetteName(k);
  }
}

std::string EndlessSource::Impl::wonderTitle(WonderKind k, int32_t x, int32_t y, uint32_t sd, bool forcedOne) {
  // wonders stand at least 220 tiles apart: the lattice cell's slot keeps neighbours' places apart
  const int slot = (int)floorMod(floorDiv(x, WCELL), 3) + 3 * (int)floorMod(floorDiv(y, WCELL), 3);
  const uint32_t v = (uint32_t)(mix64(sd ^ 0x30D3u) >> 24);
  const std::string place = kWonderPlace[(size_t)slot + 9 * (forcedOne ? 2 : (v & 1))];
  switch (k) {
    case WonderKind::ElderTree: return (v >> 2) & 1 ? "THE ELDER OAK OF " + place : "THE GREAT TREE OF " + place;
    case WonderKind::Colossus: return (v >> 2) & 1 ? "THE COLOSSUS OF " + place : "THE SUNKEN KING OF " + place;
    case WonderKind::Starfall: return (v >> 2) & 1 ? "THE STARFALL OF " + place : "THE FALLEN STAR OF " + place;
    case WonderKind::DragonBones: return (v >> 2) & 1 ? "THE DRAGON BONES OF " + place : "THE WYRM'S REST OF " + place;
    default: return wonderName(k);
  }
}

// ============================================================== helpers
// within pad tiles of a settlement's (largest possible) footprint: the distance from the rectangle, round its corners
bool EndlessSource::Impl::settleZone(const std::vector<Node>& nodes, int32_t x, int32_t y, int32_t pad) {
  for (const Node& n : nodes) {
    int32_t hw, hh;
    nodeHalf(n.type, hw, hh);
    const int64_t ox = std::max(0, std::abs(x - n.x) - hw), oy = std::max(0, std::abs(y - n.y) - hh);
    if (ox * ox + oy * oy < (int64_t)pad * pad) return true;
  }
  return false;
}

namespace {
// a rectangle of land on one relief level: dry, no rock, no beach, no marsh water, no river or lake (sampled every 2
// tiles, edges included)
struct LandCheck {
  EndlessSource::Impl& I;
  bool operator()(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int lv, bool swampOk = false) const {
    for (int32_t y = y0;; y = std::min(y + 2, y1)) {
      for (int32_t x = x0;; x = std::min(x + 2, x1)) {
        const TileF f = I.tile(x, y);
        if (f.sea || f.rock || f.biome == Biome::Beach || f.biome == Biome::Mountain) return false;
        if (!swampOk && f.biome == Biome::Swamp) return false;
        if (I.natLevel(x, y) != lv) return false;
        if (I.riverWidthCell(x, y)) return false;
        if (x == x1) break;
      }
      if (y == y1) break;
    }
    return true;
  }
};
}  // namespace

// ============================================================== one vignette's geometry and land
// (x, y) is the heart; dir the layout direction (away from the road; toward the water); sd the place's seed. Only the
// land is judged here (dry, level, off rivers and lakes); the kind's setting (a road, a forest edge, high ground) is the
// caller's to find. (relaxed: kept for the start guarantee's fallback calls; the land test is the same either way.)
bool EndlessSource::Impl::vignetteAt(VignetteKind k, int32_t x, int32_t y, int dir, uint32_t sd, SitePlan& p, bool relaxed) {
  (void)relaxed;
  p = SitePlan();
  p.type = SiteType::Vignette;
  p.kind = (uint8_t)k;
  p.ex = x; p.ey = y;
  dir &= 3;
  int hw = 4, hh = 3;
  switch (k) {
    case VignetteKind::Caravan: hw = 4; hh = 3; break;
    case VignetteKind::HunterCamp: hw = 4; hh = 3; break;
    case VignetteKind::StandingStones: hw = 5; hh = 4; break;
    case VignetteKind::Watchtower: hw = 4; hh = 3; break;
    case VignetteKind::HerbGarden: hw = 5; hh = 4; break;
    case VignetteKind::LoneGrave: hw = 2; hh = 2; break;
    case VignetteKind::Wayrest: hw = 3; hh = 2; break;
    case VignetteKind::TollBridge: hw = 0; hh = 0; break;
    case VignetteKind::FishingHut: hw = 3; hh = 3; break;
    default: break;
  }
  p.w = 2 * hw + 1; p.h = 2 * hh + 1;
  p.gx = x - hw; p.gy = y - hh;
  const int lv = natLevel(x, y);
  LandCheck land{*this};
  if (k == VignetteKind::FishingHut) {
    // the shack's box (3 x 2 above the heart) and a path to the shore are land; the jetty (3-5 tiles) is water
    const int L = 3 + (int)((sd >> 3) % 3u);
    const int dS = dir == 3 ? 3 : 2;                            // heart -> shore tile
    const int32_t sx = x + DX4[dir] * dS, sy = y + DY4[dir] * dS;
    if (!land(x - 2, y - 2, x + 2, y + 1, lv)) return false;
    for (int s = 0; s <= dS; s++)
      if (natLevel(x + DX4[dir] * s, y + DY4[dir] * s) != lv || waterAtPlan(x + DX4[dir] * s, y + DY4[dir] * s)) return false;
    for (int s = 1; s <= L; s++)
      if (!waterAtPlan(sx + DX4[dir] * s, sy + DY4[dir] * s)) return false;
    // the jetty's far end may not reach the other bank (a wide river): one more tile of water beyond it
    if (!waterAtPlan(sx + DX4[dir] * (L + 1), sy + DY4[dir] * (L + 1))) return false;
    const int32_t ex2 = sx + DX4[dir] * L, ey2 = sy + DY4[dir] * L;
    p.gx = std::min(x - 2, ex2); p.gy = std::min(y - 2, ey2);
    p.w = std::max(x + 2, ex2) - p.gx + 1; p.h = std::max(y + 1, ey2) - p.gy + 1;
    p.seed = (sd & 0xFFFFFFu) | ((uint32_t)dir << 24) | ((uint32_t)L << 28);
    return true;
  }
  if (k == VignetteKind::TollBridge) {
    // the post on the bank beside the road (dir: from the road to the post), checked by the planner; only its tile and
    // the troll's must be dry land
    if (tile(x, y).sea || riverWidthCell(x, y) >= 8) return false;
    p.seed = (sd & 0xFFFFFFu) | ((uint32_t)dir << 24);
    return true;
  }
  // (a watchtower crowns a knoll: only the tower's own ground must be level; the flats level the rest round it)
  if (k == VignetteKind::Watchtower) { if (!land(x - 2, y - 2, x + 2, y + 1, lv)) return false; }
  else if (!land(x - hw - 1, y - hh - 1, x + hw + 1, y + hh + 1, lv, k == VignetteKind::LoneGrave)) return false;
  if (lakeNear(x, y, std::max(hw, hh) + 4)) return false;
  p.seed = (sd & 0xFFFFFFu) | ((uint32_t)dir << 24);
  return true;
}

// water at a tile, judged from the plans only (rivers by their pieces, lakes by their shores; no settlement test, so
// the region planner may ask)
bool EndlessSource::Impl::waterAtPlan(int32_t x, int32_t y) {
  std::shared_ptr<const RegionHydro> H = hydro(regionOf(x), regionOf(y));
  for (const Lake& L : H->lakes) {
    const int32_t rr = lakeRadius(L, x, y);
    if (dist2(x, y, L.x, L.y) < (int64_t)rr * rr) return true;
  }
  const int i = (x - regionOf(x) * REGION) >> 3, j = (y - regionOf(y) * REGION) >> 3;
  if (!H->riverW[j * HN + i]) return false;
  for (const RiverSeg& s : H->segs) {
    const int64_t r = (s.w + 1) / 2;
    if (segD2(x, y, s.x0, s.y0, s.x1, s.y1) <= r * r) return true;
  }
  return tile(x, y).sea;
}

// ============================================================== the region planner
void EndlessSource::Impl::planVignettes(int32_t rx, int32_t ry, RegionData& D, const std::vector<Node>& nodes,
                                        const std::vector<std::shared_ptr<const Edge>>& near) {
  RegionPlan& R = D.plan;
  const int32_t x0 = rx * REGION, y0 = ry * REGION, x1 = x0 + REGION, y1 = y0 + REGION;
  const uint64_t rh = cellSeed(seed, tag("vig.region"), rx, ry);
  // how much of the region is land (sparse samples): a coast region holds fewer places
  int landN = 0;
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < 4; i++)
      if (!tile(x0 + 32 + i * 64, y0 + 32 + j * 64).sea) landN++;
  if (landN < 2) return;
  int want = 3 + (int)(rh % 4u);
  if (landN < 12) want = std::max(1, want * landN / 12);
  // kinds the start plan already put in this region are not planned again (one of a kind per region: unique names)
  bool used[(int)VignetteKind::COUNT] = {};
  for (const SitePlan& s : R.sites) if (s.type == SiteType::Vignette && s.kind < (uint8_t)VignetteKind::COUNT) used[s.kind] = true;
  auto inner = [&](int32_t x, int32_t y) { return x >= x0 + VIG_MARGIN && y >= y0 + VIG_MARGIN && x < x1 - VIG_MARGIN && y < y1 - VIG_MARGIN; };
  // road distance (to the graph roads near the region)
  auto roadD2 = [&](int32_t x, int32_t y, int32_t lim) {
    int64_t best = (int64_t)lim * lim + 1;
    for (auto& e : near) {
      if (x < e->x0 - lim || x > e->x1 + lim || y < e->y0 - lim || y > e->y1 + lim) continue;
      for (size_t n = 0; n + 1 < e->pts.size(); n++) {
        const GTile &a = e->pts[n], &b = e->pts[n + 1];
        if (std::max(a.x, b.x) + lim < x || std::min(a.x, b.x) - lim > x || std::max(a.y, b.y) + lim < y || std::min(a.y, b.y) - lim > y) continue;
        best = std::min(best, segD2(x, y, a.x, a.y, b.x, b.y));
      }
    }
    return best;
  };
  // clear of everything already planned: sites (by footprint), dens, other vignettes, settlements, forced sites
  std::vector<SitePlan> mine;
  auto clear = [&](const SitePlan& p, int32_t roadPad) {
    if (!inner(p.ex, p.ey)) return false;
    const int32_t hw = p.w / 2 + 1, hh = p.h / 2 + 1;
    if (settleZone(nodes, p.ex, p.ey, 8 + std::max(hw, hh))) return false;
    for (const SitePlan& s : R.sites) {
      if (isSettlement(s.type)) continue;
      if (p.gx - 10 < s.gx + s.w && s.gx - 10 < p.gx + p.w && p.gy - 10 < s.gy + s.h && s.gy - 10 < p.gy + p.h) return false;
      if (dist2(p.ex, p.ey, s.ex, s.ey) < 30ll * 30ll) return false;
    }
    for (const SitePlan& f : forcedSites) if (dist2(p.ex, p.ey, f.ex, f.ey) < 34ll * 34ll) return false;
    for (const DenPlan& d : R.dens) if (dist2(p.ex, p.ey, d.x, d.y) < 22ll * 22ll) return false;
    for (const SitePlan& s : mine) if (dist2(p.ex, p.ey, s.ex, s.ey) < 44ll * 44ll) return false;
    if (nearWonder(p.ex, p.ey, 30)) return false;
    // off the roads: no road through the footprint (plus a margin); road-side places come within roadPad
    for (int32_t yy = p.gy; yy < p.gy + p.h; yy += 2)
      for (int32_t xx = p.gx; xx < p.gx + p.w; xx += 2)
        if (roadD2(xx, yy, roadPad + 1) <= (int64_t)roadPad * roadPad) return false;
    if (roadD2(p.ex, p.ey, roadPad + 1) <= (int64_t)roadPad * roadPad) return false;
    return true;
  };
  // (a cheap first look, all of it implied by clear(): the land test is the expensive part, so it waits)
  auto quick = [&](int32_t x, int32_t y) {
    if (!inner(x, y) || settleZone(nodes, x, y, 10)) return false;
    for (const SitePlan& s : mine) if (dist2(x, y, s.ex, s.ey) < 40ll * 40ll) return false;
    for (const SitePlan& s : R.sites) if (!isSettlement(s.type) && dist2(x, y, s.ex, s.ey) < 26ll * 26ll) return false;
    return true;
  };
  struct Cand { int32_t x, y; int dir; uint32_t sd; uint32_t score; uint32_t river; };
  std::vector<Cand> cands[(int)VignetteKind::COUNT];
  auto hscore = [&](int32_t x, int32_t y, uint32_t k) { return (uint32_t)(tileHash(rh ^ (k * 0x9E37u), x, y) >> 40); };
  // ---- along the roads: wayrests halfway, caravans far from towns, watchtowers on high ground near the road
  for (auto& e : near) {
    if (e->pts.size() < 2) continue;
    if (e->x1 < x0 || e->x0 >= x1 || e->y1 < y0 || e->y0 >= y1) continue;
    int64_t total = 0;
    std::vector<int32_t> arc(e->pts.size(), 0);
    for (size_t n = 1; n < e->pts.size(); n++) { total += idist(e->pts[n - 1].x, e->pts[n - 1].y, e->pts[n].x, e->pts[n].y); arc[n] = (int32_t)total; }
    if (total < 40) continue;
    int32_t next = 12;
    for (size_t n = 1; n + 1 < e->pts.size(); n++) {
      if (arc[n] < next) continue;
      next = arc[n] + 18;
      const GTile& g = e->pts[n];
      if (!inner(g.x, g.y)) continue;
      const int32_t tx = e->pts[n + 1].x - e->pts[n - 1].x, ty = e->pts[n + 1].y - e->pts[n - 1].y;
      const int along = dir4(tx, ty);
      const uint32_t hs = (uint32_t)(tileHash(rh ^ 0x51DEu, g.x, g.y) >> 33);
      const int side = (hs & 1) ? (along + 1) & 3 : (along + 3) & 3;      // a normal to the road
      // the footprint's near edge two tiles off the road's line (three off a highway)
      const bool across = side == 0 || side == 2;
      const int offW = (across ? 3 : 2) + 2 + (e->cls == 0 ? 1 : 0), offC = (across ? 4 : 3) + 2 + (e->cls == 0 ? 1 : 0);
      const int32_t frac = (int32_t)((int64_t)arc[n] * 100 / total);
      if (frac >= 35 && frac <= 65)
        cands[(int)VignetteKind::Wayrest].push_back({g.x + DX4[side] * offW, g.y + DY4[side] * offW, side, hs, (uint32_t)std::abs(frac - 50) << 20 | (hs & 0xFFFFF), 0});
      if (!settleZone(nodes, g.x, g.y, 60 + 6))
        cands[(int)VignetteKind::Caravan].push_back({g.x + DX4[side] * offC, g.y + DY4[side] * offC, side, hs, hscore(g.x, g.y, 1), 0});
      // high ground within 12 tiles: a knoll at least a level above the road
      const int rl = natLevel(g.x, g.y);
      for (int k = 0; k < 8; k++) {
        const int32_t a = (int32_t)((hs >> 4) & 127) + k * 128;
        for (int32_t r : {8, 11}) {
          const int32_t qx = g.x + icosR(a, r), qy = g.y + isinR(a, r);
          const int ql = natLevel(qx, qy);
          if (ql < rl + 1) continue;
          bool top = true;
          for (int m = 0; m < 4 && top; m++) if (natLevel(qx + DX4[m] * 5, qy + DY4[m] * 5) > ql) top = false;
          if (top) { cands[(int)VignetteKind::Watchtower].push_back({qx, qy, dir4(qx - g.x, qy - g.y), hs ^ (uint32_t)k, hscore(qx, qy, 3), 0}); break; }
        }
      }
    }
  }
  // ---- toll bridges: where a road crosses a river at least 2 wide, outside the settlements
  {
    std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
    for (auto& e : near) {
      if (e->x1 < x0 || e->x0 >= x1 || e->y1 < y0 || e->y0 >= y1) continue;
      for (size_t n = 0; n + 1 < e->pts.size(); n++) {
        const GTile &a = e->pts[n], &b = e->pts[n + 1];
        if (!inner(a.x, a.y)) continue;
        for (const RiverSeg& s : H->segs) {
          if (s.w < 1) continue;   // (streams too: the road bridges them where the troll keeps his toll, chunkgen.cpp)
          if (std::max(a.x, b.x) < std::min(s.x0, s.x1) || std::min(a.x, b.x) > std::max(s.x0, s.x1) ||
              std::max(a.y, b.y) < std::min(s.y0, s.y1) || std::min(a.y, b.y) > std::max(s.y0, s.y1)) continue;
          // proper intersection (integer orientation tests)
          auto orient = [](int64_t ax, int64_t ay, int64_t bx, int64_t by, int64_t cx, int64_t cy) {
            const int64_t v = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            return (v > 0) - (v < 0);
          };
          const int o1 = orient(a.x, a.y, b.x, b.y, s.x0, s.y0), o2 = orient(a.x, a.y, b.x, b.y, s.x1, s.y1);
          const int o3 = orient(s.x0, s.y0, s.x1, s.y1, a.x, a.y), o4 = orient(s.x0, s.y0, s.x1, s.y1, b.x, b.y);
          if (o1 == o2 || o3 == o4) continue;
          // (M2 fixer round 2) the bridge head as the chunk draws the road: walk the road's own 4-connected raster
          // from a toward b; the head is the last dry tile before the water, the crossing the run of water after it.
          // along: the way the raster steps onto the water. (The old head, found walking back from b, often lay far
          // from the river, and the post and troll stood off the road and in the stream.) chunkgen.cpp lays the deck
          // straight from the head along that way and joins it to the road on the far bank.
          int32_t hx = 0, hy = 0, px0 = a.x, py0 = a.y;
          int along = -1, state = 0, wet = 0, prevStep = -1;
          bool ok = true, haveHead = false;
          auto visit = [&](int32_t qx, int32_t qy) {
            if (state == 2 || !ok) return;
            const int step = (qx == px0 && qy == py0) ? -1 : dir4(qx - px0, qy - py0);
            const bool w = waterAtPlan(qx, qy);
            if (state == 0) {
              if (!w) { hx = qx; hy = qy; haveHead = true; prevStep = step; }
              else if (haveHead) { state = 1; along = step; wet = 1; (void)prevStep; }
            } else if (state == 1) {
              if (w) wet++;
              else state = 2;
            }
            px0 = qx; py0 = qy;
          };
          // the road's raster on from a, over this piece and the next few (a piece often ends in mid-stream)
          for (size_t m2 = n; m2 + 1 < e->pts.size() && m2 < n + 6 && state != 2 && ok; m2++) {
            const GTile &c0 = e->pts[m2], &c1 = e->pts[m2 + 1];
            bool first = m2 == n;
            walk4(c0.x, c0.y, c1.x, c1.y, [&](int32_t qx, int32_t qy) {
              if (!first && qx == c0.x && qy == c0.y) return;   // (the joint was the last piece's end)
              visit(qx, qy);
            });
          }
          if (!ok || state != 2 || along < 0 || wet > 6) continue;
          // (a footbridge near a toll is not laid: baseRect asks the region plans, chunkgen.cpp)
          const uint32_t hs = (uint32_t)(tileHash(rh ^ 0x7011u, hx, hy) >> 33);
          // beside the road at the head: past the road's width (a highway is two tiles, on its +x / +y side), on
          // whichever side has dry bank for the post and the troll beside it (the hash picks when both do)
          int side = -1, off = 2;
          for (int t = 0; t < 2 && side < 0; t++) {
            const int sd2 = ((hs & 1) != 0) == (t == 0) ? (along + 1) & 3 : (along + 3) & 3;
            const int of2 = (e->cls == 0 && (DX4[sd2] > 0 || DY4[sd2] > 0)) ? 3 : 2;
            bool dry = true;
            for (int k = of2 - 1; k <= of2 && dry; k++)
              for (int st = 0; st <= 1 && dry; st++)   // (and the tile on toward the river: the bank, not a spit)
                if (waterAtPlan(hx + DX4[sd2] * k + DX4[along] * st, hy + DY4[sd2] * k + DY4[along] * st)) dry = false;
            if (dry) { side = sd2; off = of2; }
          }
          if (side < 0) continue;
          const int32_t px = hx + DX4[side] * off, py = hy + DY4[side] * off;
          cands[(int)VignetteKind::TollBridge].push_back({px, py, side | (along << 2), hs, (s.w >= 2 ? 0u : 0x800000u) | hscore(px, py, 5), s.river});   // (a wide river first)
        }
      }
    }
  }
  // ---- fishing huts on lake shores (lakes clear of the settlements) and wide river banks
  {
    std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
    for (const Lake& L : H->lakes) {
      if (!inner(L.x, L.y) || settleZone(nodes, L.x, L.y, 2 * L.r + 16)) continue;
      const uint32_t hs = (uint32_t)(mix64(rh ^ ((uint64_t)L.seed << 3)) >> 33);
      for (int k = 0; k < 4; k++) {
        const int d = (int)((hs + (uint32_t)k) & 3);   // the direction from the lake to the shore point
        int32_t sx = L.x, sy = L.y;
        bool found = false;
        for (int32_t t = L.r / 2; t < 2 * L.r + 4; t++) {
          const int32_t qx = L.x + DX4[d] * t, qy = L.y + DY4[d] * t;
          const int32_t rr = lakeRadius(L, qx, qy);
          if (dist2(qx, qy, L.x, L.y) >= (int64_t)rr * rr) { sx = qx; sy = qy; found = true; break; }
        }
        if (!found) continue;
        const int toWater = (d + 2) & 3;
        const int dS = toWater == 3 ? 3 : 2;
        const int32_t hx = sx - DX4[toWater] * dS, hy = sy - DY4[toWater] * dS;
        cands[(int)VignetteKind::FishingHut].push_back({hx, hy, toWater, hs ^ (uint32_t)(k * 77), hscore(hx, hy, 4), 0});
      }
    }
    for (const RiverSeg& s : H->segs) {
      if (s.w < 3) continue;
      const int32_t mx = (s.x0 + s.x1) / 2, my = (s.y0 + s.y1) / 2;
      if (!inner(mx, my)) continue;
      const int along = dir4(s.x1 - s.x0, s.y1 - s.y0);
      const uint32_t hs = (uint32_t)(tileHash(rh ^ 0xF15Eu, mx, my) >> 33);
      const int fromWater = (hs & 1) ? (along + 1) & 3 : (along + 3) & 3;
      const int toWater = (fromWater + 2) & 3;
      // the bank: walk out of the water
      int32_t sx = mx, sy = my;
      for (int t = 0; t < 6 && waterAtPlan(sx, sy); t++) { sx += DX4[fromWater]; sy += DY4[fromWater]; }
      const int dS = toWater == 3 ? 3 : 2;
      const int32_t hx = sx - DX4[toWater] * dS, hy = sy - DY4[toWater] * dS;
      cands[(int)VignetteKind::FishingHut].push_back({hx, hy, toWater, hs, hscore(hx, hy, 4) | 0x80000000u, 0});   // lakes first
    }
  }
  // ---- the lattice of the open land: hunter's camps at forest edges, standing stones on open ground, lone graves
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < 4; i++) {
      const uint64_t ch = mix64(rh ^ (uint64_t)(j * 4 + i + 1) * 0x9E3779B97F4A7C15ull);
      const int32_t px = x0 + i * 64 + 14 + (int32_t)((ch >> 8) % 36), py = y0 + j * 64 + 14 + (int32_t)((ch >> 24) % 36);
      const TileF f = tile(px, py);
      if (f.sea || f.rock) continue;
      const bool wood = f.biome == Biome::Forest || f.biome == Biome::Taiga || f.biome == Biome::Autumn;
      if (wood) {
        int open = 0;
        for (int k = 0; k < 8; k++) {
          const Biome b = tile(px + icosR(k * 128, 13), py + isinR(k * 128, 13)).biome;
          if (b == Biome::Plains || b == Biome::Snow || b == Biome::Desert) open++;
        }
        if (open) cands[(int)VignetteKind::HunterCamp].push_back({px, py, (int)(ch & 3), (uint32_t)(ch >> 32), hscore(px, py, 2), 0});
      } else if (f.biome == Biome::Plains || f.biome == Biome::Snow) {
        // the open side of a forest's edge: the wood within 12 tiles
        int woods = 0;
        for (int k = 0; k < 8; k++) {
          const Biome b = tile(px + icosR(k * 128, 12), py + isinR(k * 128, 12)).biome;
          if (b == Biome::Forest || b == Biome::Taiga || b == Biome::Autumn) woods++;
        }
        if (woods >= 2) cands[(int)VignetteKind::HunterCamp].push_back({px, py, (int)(ch & 3), (uint32_t)(ch >> 32), hscore(px, py, 2) | 0x800000u, 0});
      }
      // (M3c) the menhirs stand on open, wind-swept land (heath and moor, the chalk downs, the steppe and tundra, the
      // standing-stone plains above all), not in a lush meadow or the blight; hunters camp on the savanna and steppe too
      const bool openLand = ecoHas(f.eco, EF_OPEN) && !ecoHas(f.eco, EF_HOSTILE) && f.eco != Eco::FlowerMeadow && f.eco != Eco::LakeDistrict;
      const bool stoneLand = f.eco == Eco::StonePlains || f.eco == Eco::Heath || f.eco == Eco::ChalkDowns || f.eco == Eco::Steppe || f.eco == Eco::Tundra ||
                             f.eco == Eco::AlpineMeadow;
      if (!wood && openLand && (ecoInfo(f.eco).trades & TR_HUNT) && (ch & 2))
        cands[(int)VignetteKind::HunterCamp].push_back({px, py, (int)(ch & 3), (uint32_t)(ch >> 32), hscore(px, py, 2) | 0x800000u, 0});
      if ((f.biome == Biome::Plains || (f.biome == Biome::Snow && (ch & 1))) && openLand && (stoneLand || (ch & 4)))
        cands[(int)VignetteKind::StandingStones].push_back({px, py, 0, (uint32_t)(ch >> 32), hscore(px, py, 6), 0});
      if (f.biome != Biome::Beach && f.biome != Biome::Mountain)
        cands[(int)VignetteKind::LoneGrave].push_back({px + 7, py + 5, 0, (uint32_t)(ch >> 30), hscore(px, py, 7), 0});
    }
  // ---- herb gardens: a walk of 50-90 tiles out from a village or town
  for (const Node& n : nodes) {
    if (n.type == SiteType::City) continue;
    const uint64_t nh = mix64(rh ^ n.id);
    for (int k = 0; k < 6; k++) {
      const int32_t a = (int32_t)((nh >> 8) & 1023) + k * 171;
      const int32_t r = 52 + (int32_t)((nh >> (20 + k * 4)) % 36u);
      const int32_t px = n.x + icosR(a, r), py = n.y + isinR(a, r);
      if (!inner(px, py)) continue;
      cands[(int)VignetteKind::HerbGarden].push_back({px, py, (int)((nh >> 50) & 3), (uint32_t)(nh >> 32) ^ (uint32_t)k, hscore(px, py, 8), 0});
    }
  }
  // ---- pick: kinds in the region's order, the best fitting place of each, until the region has its share
  // (the kinds the land rarely offers come first when it does: a road over a wide river, a lake shore, a knoll by a
  // road; the rest in an order the region's seed shuffles)
  int order[(int)VignetteKind::COUNT];
  const int rare[3] = {(int)VignetteKind::TollBridge, (int)VignetteKind::FishingHut, (int)VignetteKind::Watchtower};
  int no = 0;
  for (int k : rare) order[no++] = k;
  for (int k = 0; k < (int)VignetteKind::COUNT; k++) if (k != rare[0] && k != rare[1] && k != rare[2]) order[no++] = k;
  for (int k = (int)VignetteKind::COUNT - 1; k > 3; k--) std::swap(order[k], order[3 + (int)(mix64(rh ^ (uint64_t)k * 31) % (uint64_t)(k - 2))]);
  int placed = 0;
  for (int oi = 0; oi < (int)VignetteKind::COUNT && placed < want; oi++) {
    const VignetteKind k = (VignetteKind)order[oi];
    if (used[(int)k]) continue;
    std::vector<Cand>& cs = cands[(int)k];
    std::stable_sort(cs.begin(), cs.end(), [](const Cand& a, const Cand& b) { return a.score < b.score; });
    int tries = 0;
    for (const Cand& c : cs) {
      if (++tries > 16) break;
      SitePlan p;
      const bool roadside = k == VignetteKind::Caravan || k == VignetteKind::Wayrest || k == VignetteKind::TollBridge;
      // (a road-side place steps further out from a winding road until the road clears its footprint)
      bool ok = false;
      for (int extra = 0; extra <= (roadside && k != VignetteKind::TollBridge ? 4 : 0) && !ok; extra++) {
        const int32_t qx = c.x + DX4[c.dir & 3] * extra, qy = c.y + DY4[c.dir & 3] * extra;
        ok = quick(qx, qy) && vignetteAt(k, qx, qy, c.dir & 3, c.sd, p, false) && clear(p, roadside ? 1 : k == VignetteKind::Watchtower ? 3 : 6);
      }
      if (!ok) continue;
      if (k == VignetteKind::TollBridge) p.seed = (p.seed & 0xFFFFFFu) | ((uint32_t)c.dir << 24);   // side and along
      p.id = makeId(rx, ry, IdKind::Site, 0x200u + (uint32_t)k);
      p.name = vignetteTitle(k, p.ex, p.ey, p.seed, c.river, 0);
      mine.push_back(p);
      placed++;
      break;
    }
  }
  // the wild with no road, river or wood offers few kinds: a second ring of stones or lone grave (its name differs)
  for (int k : {(int)VignetteKind::LoneGrave, (int)VignetteKind::StandingStones, (int)VignetteKind::HunterCamp}) {
    if (placed >= std::min(want, 3)) break;
    bool have = false;
    for (const SitePlan& q : mine) if (q.kind == (uint8_t)k) have = true;
    if (!have) continue;
    int tries = 0;
    for (const Cand& c : cands[k]) {
      if (++tries > 16) break;
      SitePlan p;
      if (!quick(c.x, c.y) || !vignetteAt((VignetteKind)k, c.x, c.y, c.dir & 3, c.sd ^ 0x2222u, p, false) || !clear(p, 6)) continue;
      p.id = makeId(rx, ry, IdKind::Site, 0x210u + (uint32_t)k);
      p.name = vignetteTitle((VignetteKind)k, p.ex, p.ey, p.seed, 0, 1);
      mine.push_back(p);
      placed++;
      break;
    }
  }
  for (const SitePlan& p : mine) {
    R.sites.push_back(p);
    R.sites.back().level = danger(p.ex, p.ey);
    R.sites.back().kingdom = kingdomAt(p.ex, p.ey);
    R.sites.back().culture = cultureAt(p.ex, p.ey);   // M3
    // (the land under a wayside place was checked level, footprint and a tile round it: no terraces needed; a watchtower
    // crowns a knoll, which its flat zone levels round the tower)
    D.flatLevel.push_back({p.kind == (uint8_t)VignetteKind::Watchtower ? natLevel(p.ex, p.ey) : -1, 0});
  }
}

// ============================================================== wonders
bool EndlessSource::Impl::wonderSpot(int32_t x, int32_t y, uint32_t sd, SitePlan& p) {
  const TileF f = tile(x, y);
  if (f.sea || f.rock) return false;
  WonderKind k;
  const uint32_t c = (sd >> 5) & 1;
  switch (f.biome) {
    case Biome::Forest: case Biome::Autumn: k = WonderKind::ElderTree; break;
    case Biome::Taiga: k = c ? WonderKind::DragonBones : WonderKind::ElderTree; break;
    case Biome::Plains: k = (f.m < Q(0.44) && c) ? WonderKind::Starfall : WonderKind::Colossus; break;
    case Biome::Desert: case Biome::Snow: k = c ? WonderKind::Starfall : WonderKind::DragonBones; break;
    default: return false;
  }
  // (M3c) the wondrous lands keep their own wonders: a dragon's bones in the blight and the ash, a fallen star in the
  // crystal barrens, an elder tree in the old-growth and the silverwood
  if (f.eco == Eco::Blight || f.eco == Eco::AshFields || f.eco == Eco::PetrifiedForest) k = WonderKind::DragonBones;
  else if (f.eco == Eco::CrystalBarrens || f.eco == Eco::StonePlains) k = WonderKind::Starfall;
  else if (f.eco == Eco::GiantForest || f.eco == Eco::Silverwood) k = WonderKind::ElderTree;
  const int lv = natLevel(x, y);
  // a level clearing (radius 8) with no water near
  for (int r : {4, 8})
    for (int a = 0; a < 8; a++) {
      const int32_t qx = x + icosR(a * 128, r), qy = y + isinR(a * 128, r);
      const TileF g = tile(qx, qy);
      if (g.sea || g.rock || g.biome == Biome::Beach || g.biome == Biome::Swamp || natLevel(qx, qy) != lv || riverWidthCell(qx, qy)) return false;
    }
  if (riverWidthCell(x, y) || lakeNear(x, y, 14)) return false;
  p = SitePlan();
  p.type = SiteType::Wonder;
  p.kind = (uint8_t)k;
  p.ex = x; p.ey = y;
  switch (k) {
    case WonderKind::ElderTree: p.w = 15; p.h = 11; break;
    case WonderKind::Colossus: p.w = 13; p.h = 11; break;
    case WonderKind::Starfall: p.w = 15; p.h = 13; break;
    default: p.w = 15; p.h = 9; break;
  }
  p.gx = x - p.w / 2; p.gy = y - p.h / 2;
  p.seed = sd & 0xFFFFFFu;
  p.name = wonderTitle(k, x, y, p.seed);
  return true;
}

bool EndlessSource::Impl::wonderOf(int32_t i, int32_t j, SitePlan& out) {
  const uint64_t key = key2(i, j);
  auto it = wonderMemo.find(key);
  if (it != wonderMemo.end()) { out = it->second.second; return it->second.first; }
  if (wonderMemo.size() > 20000) wonderMemo.clear();
  bool ok = false;
  SitePlan p;
  const uint64_t h = cellSeed(seed, tag("wonder.cell"), i, j);
  if (hq(h) < Q(0.88)) {
    std::vector<Node> nodes;
    const int32_t cx0 = i * WCELL, cy0 = j * WCELL;
    nodesIn(cx0 - 300, cy0 - 300, cx0 + WCELL + 300, cy0 + WCELL + 300, nodes);
    for (int n = 0; n < 12 && !ok; n++) {
      const uint64_t hn = mix64(h ^ (0x100u + (uint64_t)n));
      const int32_t x = cx0 + WM + (int32_t)((hn & 0xFFFFFFFFull) % (uint64_t)(WCELL - 2 * WM));
      const int32_t y = cy0 + WM + (int32_t)((hn >> 32) % (uint64_t)(WCELL - 2 * WM));
      if (settleZone(nodes, x, y, 150)) continue;
      bool clearF = true;
      for (const SitePlan& f : forcedSites) {
        if (f.type == SiteType::Wonder) { if (dist2(f.ex, f.ey, x, y) < 300ll * 300ll) clearF = false; }
        else if (dist2(f.ex, f.ey, x, y) < 90ll * 90ll) clearF = false;
      }
      if (!clearF) continue;
      if (!wonderSpot(x, y, (uint32_t)(hn >> 16), p)) continue;
      p.id = makeId(regionOf(x), regionOf(y), IdKind::Site, 0x2F0u + (uint32_t)(floorMod(i, 2) + 2 * floorMod(j, 2)));
      ok = true;
    }
  }
  wonderMemo.emplace(key, std::make_pair(ok, p));
  out = p;
  return ok;
}

bool EndlessSource::Impl::nearWonder(int32_t x, int32_t y, int32_t r) {
  for (const SitePlan& f : forcedSites)
    if (f.type == SiteType::Wonder && dist2(f.ex, f.ey, x, y) < (int64_t)(r + f.w / 2) * (r + f.w / 2)) return true;
  for (int32_t j = floorDiv(y - r - 16, WCELL); j <= floorDiv(y + r + 16, WCELL); j++)
    for (int32_t i = floorDiv(x - r - 16, WCELL); i <= floorDiv(x + r + 16, WCELL); i++) {
      SitePlan w;
      if (wonderOf(i, j, w) && dist2(w.ex, w.ey, x, y) < (int64_t)(r + w.w / 2) * (r + w.w / 2)) return true;
    }
  return false;
}

// ============================================================== the start guarantee (VISION_PLAN 2.6, M2)
void EndlessSource::Impl::forceStartPois() {
  const Node& home = forced[0];
  const Node& city = forced[1];
  const int32_t hx = home.x, hy = home.y;
  std::vector<Node> nodes;
  nodesIn(hx - 600, hy - 600, hx + 600, hy + 600, nodes);
  std::vector<std::shared_ptr<const Edge>> roads;
  graphEdges(hx - 260, hy - 260, hx + 260, hy + 260, roads);
  std::shared_ptr<const Edge> story = edge(home, city);
  auto roadD = [&](int32_t x, int32_t y) {
    int64_t best = INT64_MAX;
    for (auto& e : roads)
      for (size_t n = 0; n + 1 < e->pts.size(); n++) best = std::min(best, segD2(x, y, e->pts[n].x, e->pts[n].y, e->pts[n + 1].x, e->pts[n + 1].y));
    for (size_t n = 0; n + 1 < story->pts.size(); n++)
      best = std::min(best, segD2(x, y, story->pts[n].x, story->pts[n].y, story->pts[n + 1].x, story->pts[n + 1].y));
    return best;
  };
  const uint32_t baseLocal = LOCAL_FORCED_VIG;
  int nForced = 0;
  auto clearOf = [&](const SitePlan& p, int32_t roadPad, int32_t spacing) {
    if (settleZone(nodes, p.ex, p.ey, 8 + std::max(p.w, p.h) / 2 + 1)) return false;
    for (const SitePlan& f : forcedSites) if (dist2(f.ex, f.ey, p.ex, p.ey) < (int64_t)spacing * spacing) return false;
    for (const DenPlan& d : forcedDens) if (dist2(d.x, d.y, p.ex, p.ey) < 26ll * 26ll) return false;
    for (int32_t yy = p.gy; yy < p.gy + p.h; yy += 2)
      for (int32_t xx = p.gx; xx < p.gx + p.w; xx += 2)
        if (roadD(xx, yy) <= (int64_t)roadPad * roadPad) return false;
    return roadD(p.ex, p.ey) > (int64_t)roadPad * roadPad;
  };
  auto addForced = [&](SitePlan p, VignetteKind k, uint32_t river) {
    p.id = makeId(regionOf(p.ex), regionOf(p.ey), IdKind::Site, baseLocal + (uint32_t)nForced++);
    // (a second forced place of a kind in one region is its second instance: another person's name)
    int inst = 0;
    for (const SitePlan& f : forcedSites)
      if (f.type == SiteType::Vignette && f.kind == (uint8_t)k && regionOf(f.ex) == regionOf(p.ex) && regionOf(f.ey) == regionOf(p.ey)) inst++;
    p.name = vignetteTitle(k, p.ex, p.ey, p.seed, river, std::min(inst, 1));
    forcedSites.push_back(p);
  };
  auto haveKind = [&](VignetteKind k) {
    for (const SitePlan& f : forcedSites) if (f.type == SiteType::Vignette && f.kind == (uint8_t)k) return true;
    return false;
  };
  // 1. beside the road to the story city: a wayfarers' rest 45-62 tiles out, an overturned caravan 88-118 out
  if (story->pts.size() >= 2) {
    const bool startIsA = story->a == home.id;
    const size_t np = story->pts.size();
    for (int which = 0; which < 4; which++) {
      // (a second try further out when the first stretch has no room: still within the road's first 300 tiles)
      const VignetteKind k = (which & 1) == 0 ? VignetteKind::Wayrest : VignetteKind::Caravan;
      if (which >= 2 && haveKind(k)) continue;
      const int32_t dmin = which == 0 ? 45 : which == 1 ? 88 : which == 2 ? 62 : 118, dmax = which == 0 ? 62 : which == 1 ? 118 : 200;
      bool done = false;
      for (size_t q = 1; q + 1 < np && !done; q++) {
        const GTile& g = story->pts[startIsA ? q : np - 1 - q];
        const GTile& gp = story->pts[startIsA ? q - 1 : np - q];
        const GTile& gn = story->pts[startIsA ? q + 1 : np - 2 - q];
        const int32_t d = idist(g.x, g.y, hx, hy);
        if (d < dmin) continue;
        if (d > dmax) break;
        const int along = dir4(gn.x - gp.x, gn.y - gp.y);
        for (int s = 0; s < 2 && !done; s++) {
          const int side = ((s + (int)(mix64(seed ^ tag("start.vig") ^ (uint64_t)which) & 1)) & 1) ? (along + 1) & 3 : (along + 3) & 3;
          const bool across = side == 0 || side == 2;
          const int off = (k == VignetteKind::Wayrest ? (across ? 3 : 2) : (across ? 4 : 3)) + 2 + (story->cls == 0 ? 1 : 0);
          SitePlan p;
          const uint32_t sd = (uint32_t)(mix64(seed ^ tag("start.vig.sd") ^ (uint64_t)(which * 131 + (int)q)) >> 16);
          bool ok = false;
          int why = 0;
          for (int extra = 0; extra <= 4 && !ok; extra++) {
            const bool v = vignetteAt(k, g.x + DX4[side] * (off + extra), g.y + DY4[side] * (off + extra), side, sd, p, false);
            ok = v && clearOf(p, 1, 26);
            if (!v) why |= 1; else if (!ok) why |= 2;
          }
          (void)why;
          if (!ok) continue;
          // the caravan stays 60 tiles clear of every settlement's footprint
          if (k == VignetteKind::Caravan && settleZone(nodes, p.ex, p.ey, 60)) continue;
          addForced(p, k, 0);
          done = true;
        }
      }
    }
    // a short road to the story city leaves no stretch 60 tiles clear of both towns for a caravan: another place then
    // stands within sight of the road (a lone grave, a watchtower, a hunter's camp, standing stones, a herb garden),
    // 7-18 tiles off it
    int byRoad = 0;
    for (const SitePlan& f : forcedSites)
      if (f.type == SiteType::Vignette && (f.kind == (uint8_t)VignetteKind::Wayrest || f.kind == (uint8_t)VignetteKind::Caravan)) byRoad++;
    // (M2 fixer round 2: a second, wider look when the first finds no room: the road's first 260 tiles, 5-21 off it,
    // closer spacing; seeds 27, 33, 39 and 52 had only one place by the story road)
    for (int wide = 0; wide < 2 && byRoad < 2; wide++)
      for (VignetteKind k : {VignetteKind::LoneGrave, VignetteKind::Watchtower, VignetteKind::HunterCamp, VignetteKind::StandingStones, VignetteKind::HerbGarden}) {
        if (byRoad >= 2) break;
        if (haveKind(k)) continue;
        bool done = false;
        for (size_t q = 1; q + 1 < np && !done; q += wide ? 1 : 2) {
          const GTile& g = story->pts[startIsA ? q : np - 1 - q];
          const GTile& gp = story->pts[startIsA ? q - 1 : np - q];
          const GTile& gn = story->pts[startIsA ? q + 1 : np - 2 - q];
          const int32_t d = idist(g.x, g.y, hx, hy);
          if (d < (wide ? 24 : 40)) continue;
          if (d > (wide ? 260 : 200)) break;
          const int along = dir4(gn.x - gp.x, gn.y - gp.y);
          for (int s = 0; s < 2 && !done; s++)
            for (int32_t off = wide ? 5 : 7; off <= (wide ? 21 : 18) && !done; off += wide ? 2 : 4) {
              const int side = s ? (along + 1) & 3 : (along + 3) & 3;
              SitePlan p;
              const uint32_t sd = (uint32_t)(mix64(seed ^ tag("start.vig.road2") ^ (uint64_t)((int)k * 7919 + (int)q * 31 + off + wide * 0x5151)) >> 16);
              if (!vignetteAt(k, g.x + DX4[side] * off, g.y + DY4[side] * off, side, sd, p, true) || !clearOf(p, wide ? 2 : 3, wide ? 16 : 20)) continue;
              addForced(p, k, 0);
              byRoad++;
              done = true;
            }
        }
      }
  }
  // 2. within 60 of the heart: a lone grave, a herb garden (and standing stones if the land is open); then within 120
  //    standing stones, a hunter's camp, a watchtower near a road
  struct Want { VignetteKind k; int32_t d0, d1; };
  const Want wants[] = {{VignetteKind::LoneGrave, 44, 60}, {VignetteKind::HerbGarden, 50, 60}, {VignetteKind::StandingStones, 52, 60},
                        {VignetteKind::StandingStones, 66, 116}, {VignetteKind::HunterCamp, 66, 116}, {VignetteKind::Watchtower, 66, 116},
                        {VignetteKind::LoneGrave, 66, 116}, {VignetteKind::HerbGarden, 60, 88}};
  auto countKinds = [&](int32_t r, bool withLattice) {
    std::vector<int> ks;
    for (const SitePlan& f : forcedSites)
      if (dist2(f.ex, f.ey, hx, hy) <= (int64_t)r * r) {
        const int key = poiKindKey(f.type, f.kind);
        if (std::find(ks.begin(), ks.end(), key) == ks.end()) ks.push_back(key);
      }
    for (const DenPlan& d : forcedDens)
      if (dist2(d.x, d.y, hx, hy) <= (int64_t)r * r && std::find(ks.begin(), ks.end(), 99) == ks.end()) ks.push_back(99);
    (void)withLattice;
    return (int)ks.size();
  };
  for (int pass = 0; pass < 2; pass++)
    for (const Want& w : wants) {
      const bool inner60 = w.d1 <= 60;
      if (!inner60 && countKinds(120, false) >= 9 && pass == 0) continue;
      if (inner60 && countKinds(60, false) >= 3) continue;
      if (haveKind(w.k)) continue;
      const int32_t a0 = (int32_t)(hq(mix64(seed ^ tag("start.vig.a") ^ (uint64_t)w.k)) & 1023);
      bool done = false;
      for (int32_t d = w.d0; d <= w.d1 && !done; d += 4)
        for (int n = 0; n < 24 && !done; n++) {
          const int32_t a = a0 + n * 1024 / 24;
          const int32_t px = hx + icosR(a, d), py = hy + isinR(a, d);
          const TileF f = tile(px, py);
          if (pass == 0) {
            if (w.k == VignetteKind::StandingStones && f.biome != Biome::Plains && f.biome != Biome::Snow) continue;
            if (w.k == VignetteKind::HunterCamp) {
              if (f.biome != Biome::Forest && f.biome != Biome::Taiga && f.biome != Biome::Autumn) continue;
              int open = 0;
              for (int k = 0; k < 8; k++) {
                const Biome b = tile(px + icosR(k * 128, 13), py + isinR(k * 128, 13)).biome;
                if (b == Biome::Plains || b == Biome::Snow || b == Biome::Desert) open++;
              }
              if (!open) continue;
            }
            if (w.k == VignetteKind::Watchtower) {
              if (roadD(px, py) > 12ll * 12ll) continue;
              bool high = false;
              for (int m = 0; m < 4; m++) if (natLevel(px + DX4[m] * 9, py + DY4[m] * 9) < natLevel(px, py)) high = true;
              if (!high) continue;
            }
          }
          SitePlan p;
          const uint32_t sd = (uint32_t)(mix64(seed ^ tag("start.vig.sd2") ^ (uint64_t)((int)w.k * 977 + d * 31 + n)) >> 16);
          if (!vignetteAt(w.k, px, py, (int)(sd & 3), sd, p, pass > 0)) continue;
          if (!clearOf(p, 5, 24)) continue;
          addForced(p, w.k, 0);
          done = true;
        }
    }
  // (M2 fixer round 2) the 60-tile ring still short of three kinds (seeds 26, 40: no room at 44-60 tiles): any small
  // place that fits, from 20 tiles out, with closer spacing
  if (countKinds(60, false) < 3)
    for (VignetteKind k : {VignetteKind::LoneGrave, VignetteKind::HerbGarden, VignetteKind::StandingStones, VignetteKind::HunterCamp}) {
      if (countKinds(60, false) >= 3) break;
      bool have60 = false;
      for (const SitePlan& f : forcedSites)
        if (f.type == SiteType::Vignette && f.kind == (uint8_t)k && dist2(f.ex, f.ey, hx, hy) <= 60ll * 60ll) have60 = true;
      if (have60) continue;
      const int32_t a0 = (int32_t)(hq(mix64(seed ^ tag("start.vig.a60") ^ (uint64_t)k)) & 1023);
      bool done = false;
      for (int32_t d = 20; d <= 58 && !done; d += 3)
        for (int n = 0; n < 32 && !done; n++) {
          const int32_t a = a0 + n * 1024 / 32;
          const int32_t px = hx + icosR(a, d), py = hy + isinR(a, d);
          SitePlan p;
          const uint32_t sd = (uint32_t)(mix64(seed ^ tag("start.vig.sd60") ^ (uint64_t)((int)k * 977 + d * 31 + n)) >> 16);
          if (!vignetteAt(k, px, py, (int)(sd & 3), sd, p, true)) continue;
          if (!clearOf(p, 3, 16)) continue;
          addForced(p, k, 0);
          done = true;
        }
    }
  // 3. a wonder within 400 tiles: the lattice's, or one forced in
  bool wonder = false;
  for (int32_t j = floorDiv(hy - 400, WCELL); j <= floorDiv(hy + 400, WCELL) && !wonder; j++)
    for (int32_t i = floorDiv(hx - 400, WCELL); i <= floorDiv(hx + 400, WCELL) && !wonder; i++) {
      SitePlan w;
      if (wonderOf(i, j, w) && dist2(w.ex, w.ey, hx, hy) <= 390ll * 390ll) wonder = true;
    }
  if (!wonder) {
    const int32_t a0 = (int32_t)(hq(mix64(seed ^ tag("start.wonder"))) & 1023);
    for (int32_t d = 200; d <= 380 && !wonder; d += 20)
      for (int n = 0; n < 32 && !wonder; n++) {
        const int32_t a = a0 + n * 1024 / 32;
        const int32_t px = hx + icosR(a, d), py = hy + isinR(a, d);
        if (settleZone(nodes, px, py, 150)) continue;
        bool clr = true;
        for (const SitePlan& f : forcedSites) if (dist2(f.ex, f.ey, px, py) < 90ll * 90ll) clr = false;
        if (!clr) continue;
        SitePlan p;
        if (!wonderSpot(px, py, (uint32_t)(mix64(seed ^ tag("start.wonder.sd") ^ (uint64_t)(d * 64 + n)) >> 16), p)) continue;
        p.id = makeId(regionOf(px), regionOf(py), IdKind::Site, LOCAL_FORCED_WONDER);
        p.name = wonderTitle((WonderKind)p.kind, px, py, p.seed, true);
        forcedSites.push_back(p);
        wonder = true;
      }
  }
  wonderMemo.clear();   // the lattice answers above saw no forced wonder; drop them
}

// ============================================================== stamps
namespace {
// a prop with its frozen footprint (art::wildFootprint): on the bottom-centre tile, Filler on the rest
void putWild(Stamp& S, int32_t x, int32_t y, Prop p) {
  int w = 1, h = 1;
  art::wildFootprint(p, w, h);
  for (int32_t yy = y - h + 1; yy <= y; yy++)
    for (int32_t xx = x - w / 2; xx <= x + w / 2; xx++) S.p(xx, yy, Prop::Filler);
  S.p(x, y, p);
}
bool wetG(Ground g) { return g == Ground::Water || g == Ground::DeepWater; }
// trodden earth where grass or leaf litter grows (snow and sand keep their own look: a dirt patch on them reads as a
// hole in the land)
void dirt(Stamp& S, int32_t x, int32_t y) {
  const Ground g = S.at(x, y);
  if (g == Ground::Grass || g == Ground::Meadow || g == Ground::ForestFloor || g == Ground::Autumn || g == Ground::Tundra || g == Ground::Swamp ||
      g == Ground::Rock || g == Ground::Farmland)
    S.g(x, y, Ground::Dirt);
}
}  // namespace

void EndlessSource::Impl::stampVignette(const SitePlan& p, Stamp& S) {
  const int32_t x = p.ex, y = p.ey;
  const VignetteKind k = (VignetteKind)p.kind;
  const int dir = (int)((p.seed >> 24) & 3);
  const uint32_t sd = p.seed & 0xFFFFFFu;
  Rng r(sd ^ 0x5617u ^ ((uint32_t)p.kind << 20));
  // the place is cleared of the wild (trees, rocks) and of hard ground; water stays water
  for (int32_t yy = p.gy; yy < p.gy + p.h; yy++)
    for (int32_t xx = p.gx; xx < p.gx + p.w; xx++) {
      if (!S.in(xx, yy)) continue;
      S.clear(xx, yy);
      if (S.at(xx, yy) == Ground::Rock) S.g(xx, yy, Ground::Dirt);
    }
  auto spawnAt = [&](int slot, int32_t sx, int32_t sy, bool npc, Role role, bool bandit, Monster mon, bool boss) {
    SpawnPlan s;
    s.siteId = p.id;
    s.sp.x = sx; s.sp.y = sy;
    s.sp.npc = npc; s.sp.role = role; s.sp.bandit = bandit; s.sp.mon = mon; s.sp.boss = boss;
    s.sp.slot = slot;
    S.c.spawns.push_back(s);
  };
  // a ragged patch of trodden earth (radii in tiles, the edge noisy)
  // (the edge wanders with a smooth noise, then a little per-tile fray, so the patch is never a box or a diamond)
  auto patch = [&](int32_t cx, int32_t cy, int32_t rxx, int32_t ryy) {
    for (int32_t yy = cy - ryy - 2; yy <= cy + ryy + 2; yy++)
      for (int32_t xx = cx - rxx - 2; xx <= cx + rxx + 2; xx++) {
        const int32_t dx = (xx - cx) * 100 / std::max(1, rxx), dy = (yy - cy) * 100 / std::max(1, ryy);
        const int32_t wob = (vnoiseQ(xx * 2, yy * 2, 2, mix64(sd ^ 0xD178u)) - 32768) * 45 / 32768;   // -45..45
        const int32_t j = (int32_t)(tileHash(sd ^ 0xD177u, xx, yy) % 16);
        const int32_t r = 92 + wob + j;
        if (dx * dx + dy * dy < r * r) dirt(S, xx, yy);
      }
  };
  switch (k) {
    case VignetteKind::Caravan: {
      // the wagon on its side beside the road, its load spilled toward the road; the ambushers lie in wait further out
      patch(x, y, 3, 2);
      putWild(S, x, y, Prop::CaravanWreck);
      const int tx = -DX4[dir], ty = -DY4[dir];                 // toward the road
      const int px = -ty, py = tx;                              // along the road
      S.p(x + tx + px * 2, y + ty + py * 2 + (ty == 0 ? 1 : 0), Prop::Crate);
      S.p(x + px * -2 + tx, y + py * -2 + ty + (ty == 0 ? 1 : 0), Prop::Barrel);
      S.p(x - 2 + (r.irange(2)), y + 2, Prop::Crate);
      S.p(x + 2, y + 1 + r.irange(2), Prop::Chest);
      if (r.irange(2)) S.p(x - 3, y - 1, Prop::Barrel);
      S.p(x + 1 - 2 * r.irange(2), y - 2, Prop::Bones);
      for (int k2 = 0; k2 < 3; k2++) {
        const int out = 3 + r.irange(4);                          // 6-10 tiles off the road
        const int sidew = (k2 - 1) * 3 + r.irange(2);
        spawnAt(1 + k2, x + DX4[dir] * out + px * sidew, y + DY4[dir] * out + py * sidew, false, Role::Bandit, true, Monster::Wolf, false);
      }
      break;
    }
    case VignetteKind::HunterCamp: {
      patch(x, y, 3, 2);
      S.p(x, y, Prop::Campfire);
      S.p(x - 2, y - 2, Prop::Tent);
      S.p(x + 2, y - 2, Prop::HideRack);
      S.p(x + 3, y + 1, Prop::Woodpile);
      S.p(x - 2, y + 1, Prop::Log);
      if (r.irange(2)) S.p(x + 1, y - 2, Prop::Barrel);
      S.p(x - 3, y - 1, Prop::Bones);
      spawnAt(0, x + 1, y + 1, true, Role::Hunter, false, Monster::Wolf, false);
      break;
    }
    case VignetteKind::StandingStones: {
      // a ring of 5-7 menhirs about a heart stone, the grass inside kept short and starred with flowers
      for (int32_t yy = y - 3; yy <= y + 3; yy++)
        for (int32_t xx = x - 4; xx <= x + 4; xx++) {
          const int32_t dx = xx - x, dy = yy - y;
          if (dx * dx * 9 + dy * dy * 16 > 144) continue;
          const Ground g = S.at(xx, yy);
          if (g == Ground::Grass || g == Ground::Tundra || g == Ground::ForestFloor) S.g(xx, yy, Ground::Meadow);
        }
      putWild(S, x, y, Prop::StandingStone);
      const int n = 5 + r.irange(3);
      const int32_t a0 = r.irange(1024);
      for (int k2 = 0; k2 < n; k2++) {
        const int32_t a = a0 + k2 * 1024 / n + r.irange(40) - 20;
        const int32_t sx = x + (icosR(a, 4 * 256) + (icosR(a, 4 * 256) >= 0 ? 128 : -128)) / 256;
        const int32_t sy = y + (isinR(a, 3 * 256) + (isinR(a, 3 * 256) >= 0 ? 128 : -128)) / 256;
        if (sx == x && sy == y) continue;
        putWild(S, sx, sy, Prop::StandingStone);
      }
      for (int k2 = 0; k2 < 3; k2++) {
        const int32_t fx = x + r.irange(5) - 2, fy = y + r.irange(3) - 1;
        if (S.in(fx, fy) && !S.c.prop[S.idx(fx, fy)]) S.p(fx, fy, k2 == 0 ? Prop::Flowers1 : k2 == 1 ? Prop::Flowers2 : Prop::Flowers3);
      }
      break;
    }
    case VignetteKind::Watchtower: {
      patch(x, y + 1, 3, 2);
      putWild(S, x, y, Prop::WatchtowerRuin);
      // fallen masonry round its foot, the chest in the lee of the wall
      static const int rub[6][2] = {{-3, -1}, {-3, 0}, {3, -2}, {3, 0}, {-2, 2}, {2, -3}};
      for (int k2 = 0; k2 < 6; k2++)
        if (r.irange(3)) S.p(x + rub[k2][0], y + rub[k2][1], k2 < 4 ? Prop::RuinWall : (r.irange(2) ? Prop::Rock : Prop::MossRock));
      S.p(x + 2, y + 1, Prop::Chest);
      S.p(x - 1, y + 2, Prop::Bones);
      spawnAt(1, x - 2, y + 1, false, Role::Bandit, true, Monster::Wolf, false);
      spawnAt(2, x + 2, y - 1 + (int)(sd & 1) * 3, false, Role::Bandit, true, Monster::Wolf, false);
      break;
    }
    case VignetteKind::FishingHut: {
      const int L = (int)((p.seed >> 28) & 7);
      const int dS = dir == 3 ? 3 : 2;
      const int32_t sx = x + DX4[dir] * dS, sy = y + DY4[dir] * dS;
      // the path from the shack to the water, the jetty's planks out over it
      for (int s = 0; s <= dS; s++) dirt(S, x + DX4[dir] * s, y + DY4[dir] * s);
      for (int s = 1; s <= L; s++) {
        const int32_t jx = sx + DX4[dir] * s, jy = sy + DY4[dir] * s;
        if (wetG(S.at(jx, jy))) { S.g(jx, jy, Ground::Bridge); S.clear(jx, jy); }
      }
      putWild(S, x, y, Prop::FishingShack);
      // the drying rack and the catch beside the shack, on the side away from the path
      const int sideX = (sd & 1) ? 1 : -1;
      if (dir == 1 || dir == 3) { S.p(x + 3 * sideX, y, Prop::DryingRack); S.p(x - 2 * sideX, y + (dir == 1 ? 1 : 0), Prop::Barrel); }
      else { S.p(x, y + 1 + 0 * sideX, Prop::DryingRack); S.p(x - DX4[dir] * 2, y - 1, Prop::Barrel); }
      spawnAt(0, x + DX4[dir] * (dS - 1) + (dir == 1 || dir == 3 ? 1 : 0), y + DY4[dir] * (dS - 1) + (dir == 0 || dir == 2 ? 1 : 0), true, Role::Fisher,
              false, Monster::Wolf, false);
      break;
    }
    case VignetteKind::TollBridge: {
      // the post on the bank beside the road at the bridge head; the troll waits beside the road at the head
      dirt(S, x, y);
      S.p(x, y, Prop::TollPost);
      // (M2 fixer round 2) at the bridge head between his post and the road, so the post, the troll and the bridge
      // read as one crossing (the planner puts the post two tiles off the road's edge at the head)
      const int32_t tx = x - DX4[dir], ty = y - DY4[dir];
      spawnAt(0, tx, ty, false, Role::Villager, false, Monster::Troll, true);
      // (M2 fixer round 3, review: "the hero at the approach is almost hidden under a tree crown") the bridge head is
      // a clearing: no tree, bush or boulder grows within three tiles of the post or the troll's spot
      for (int32_t yy = -3; yy <= 3; yy++)
        for (int32_t xx = -3; xx <= 3; xx++) {
          if (xx * xx + yy * yy > 10) continue;
          for (int k = 0; k < 2; k++) {
            const int32_t qx = (k ? tx : x) + xx, qy = (k ? ty : y) + yy;
            if (wetG(S.at(qx, qy))) continue;
            S.reserve(qx, qy);
            if (!(qx == x && qy == y)) S.clear(qx, qy);
          }
        }
      break;
    }
    case VignetteKind::HerbGarden: {
      // a fenced plot with a gap in its south side, beds of herbs in rows either side of the middle path
      const int32_t fx0 = x - 4, fx1 = x + 4, fy0 = y - 3, fy1 = y + 3;
      for (int32_t yy = fy0; yy <= fy1; yy++)
        for (int32_t xx = fx0; xx <= fx1; xx++) {
          if (yy > fy0 && yy < fy1 && xx > fx0 && xx < fx1) { dirt(S, xx, yy); continue; }
          if (yy == fy1 && xx == x) { dirt(S, xx, yy); continue; }   // the gate
          S.p(xx, yy, (yy == fy0 || yy == fy1) ? Prop::FenceH : Prop::FenceV);
        }
      int beds = 0;
      for (int32_t yy : {y - 2, y, y + 2})
        for (int32_t xx = x - 3; xx <= x + 3; xx++) {
          if (xx == x) continue;
          if (yy == y + 2 && std::abs(xx - x) <= 1) continue;          // room by the gate
          if (beds >= 6 && (tileHash(sd ^ 0xBEDu, xx, yy) & 3) == 0) continue;
          S.p(xx, yy, Prop::HerbBed);
          beds++;
        }
      S.p(x + 5, y + 2, Prop::Baskets);
      if (r.irange(2)) S.p(x - 5, y + 2, Prop::Barrel);
      spawnAt(0, x, y + 1, true, Role::Herbalist, false, Monster::Wolf, false);
      break;
    }
    case VignetteKind::LoneGrave: {
      // (M2 fixer round 3) a little clearing round it, so no boulder of the field beside it reads as another grave
      for (int32_t yy = -3; yy <= 2; yy++)
        for (int32_t xx = -3; xx <= 3; xx++)
          if (xx * xx + yy * yy <= 10 && !wetG(S.at(x + xx, y + yy))) { S.reserve(x + xx, y + yy); S.clear(x + xx, y + yy); }
      putWild(S, x, y, Prop::GraveCairn);
      if (sd & 1) S.p(x + 1 + (int)((sd >> 1) & 1), y - 1, Prop::DeadTree);
      else { S.p(x - 1, y + 1, Prop::Flowers2); S.p(x + 1, y + 1, Prop::Flowers3); }
      if ((sd >> 2) & 1) S.p(x - 1, y, Prop::Flowers1);
      break;
    }
    case VignetteKind::Wayrest: {
      patch(x, y, 2, 1);
      S.p(x, y, Prop::Campfire);
      S.p(x - 2, y, Prop::Bedroll);
      S.p(x + 1, y - 1, Prop::Log);
      S.p(x + 2, y + 1, Prop::Log);
      if (sd & 1) S.p(x - 2, y - 1, Prop::Barrel);
      spawnAt(0, x + 1, y + 1, true, Role::Traveller, false, Monster::Wolf, false);
      break;
    }
    default: break;
  }
}

void EndlessSource::Impl::stampWonder(const SitePlan& p, Stamp& S) {
  const int32_t x = p.ex, y = p.ey;
  const WonderKind k = (WonderKind)p.kind;
  const uint32_t sd = p.seed & 0xFFFFFFu;
  // the clearing: an oval with a ragged edge, cleared of the wild
  const int32_t rxx = p.w / 2, ryy = p.h / 2;
  auto inOval = [&](int32_t xx, int32_t yy, int32_t shrink) {
    const int32_t dx = (xx - x) * 100 / std::max(1, rxx - shrink), dy = (yy - y) * 100 / std::max(1, ryy - shrink);
    const int32_t j = (int32_t)(tileHash(sd ^ 0x0A1Bu, xx, yy) % 22);
    return dx * dx + dy * dy < (88 + j) * (88 + j);
  };
  for (int32_t yy = p.gy; yy < p.gy + p.h; yy++)
    for (int32_t xx = p.gx; xx < p.gx + p.w; xx++) {
      if (!inOval(xx, yy, 0)) continue;
      S.clear(xx, yy);
      if (S.at(xx, yy) == Ground::Rock) S.g(xx, yy, Ground::Dirt);
    }
  auto sprinkle = [&](uint32_t salt, int n, std::initializer_list<Prop> l) {
    for (int k2 = 0; k2 < n; k2++) {
      const uint64_t h = mix64(sd ^ salt ^ (uint64_t)k2 * 0x9E37u);
      const int32_t xx = p.gx + 1 + (int32_t)(h % (uint64_t)std::max(1, p.w - 2)), yy = p.gy + 1 + (int32_t)((h >> 20) % (uint64_t)std::max(1, p.h - 2));
      if (!inOval(xx, yy, 1) || (std::abs(xx - x) <= 3 && yy >= y - 3 && yy <= y + 1)) continue;
      if (S.in(xx, yy) && !S.c.prop[S.idx(xx, yy)] && !groundSolid(S.at(xx, yy))) S.p(xx, yy, *(l.begin() + (size_t)((h >> 40) % l.size())));
    }
  };
  switch (k) {
    case WonderKind::ElderTree: {
      // the glade: mossy forest floor and meadow under the great crown, ferns and mushrooms round its roots
      for (int32_t yy = p.gy; yy < p.gy + p.h; yy++)
        for (int32_t xx = p.gx; xx < p.gx + p.w; xx++)
          if (inOval(xx, yy, 2)) { const Ground g = S.at(xx, yy); if (g != Ground::Void && !wetG(g)) S.g(xx, yy, (tileHash(sd, xx, yy) & 3) ? Ground::Meadow : Ground::ForestFloor); }
      putWild(S, x, y, Prop::ElderTree);
      sprinkle(0x11u, 10, {Prop::Fern, Prop::Mushrooms, Prop::Flowers1, Prop::Flowers3, Prop::MossRock});
      break;
    }
    case WonderKind::Colossus: {
      // broken paving round the statue's foot, toppled masonry about it
      for (int32_t yy = y - 3; yy <= y + 3; yy++)
        for (int32_t xx = x - 4; xx <= x + 4; xx++) {
          const uint32_t h = (uint32_t)(tileHash(sd ^ 0xC010u, xx, yy) & 255);
          const int32_t dd = std::abs(xx - x) + std::abs(yy - y);
          if (dd <= 4 && (int32_t)h < 200 - dd * 30) S.g(xx, yy, Ground::StoneFloor);
          else if (dd <= 5 && h < 90) S.g(xx, yy, Ground::Dirt);
        }
      putWild(S, x, y, Prop::Colossus);
      sprinkle(0x22u, 6, {Prop::RuinColumn, Prop::Rock, Prop::MossRock, Prop::Boulder});
      break;
    }
    case WonderKind::Starfall: {
      // the crater: a bowl of scorched earth (an oval 6 x 5, its edge wandering), the thrown-up rim a broken ring of
      // boulders and rocks with gaps, the heart shard in the middle and a few more strewn across the floor, never two
      // side by side
      for (int32_t yy = y - 6; yy <= y + 6; yy++)
        for (int32_t xx = x - 7; xx <= x + 7; xx++) {
          const int32_t dx = xx - x, dy = yy - y;
          const int32_t q = (dx * dx * 100) / 36 + (dy * dy * 100) / 25;   // 100 on the oval
          const int32_t wob = (vnoiseQ(xx * 3, yy * 3, 2, mix64(sd ^ 0x5F0u)) - 32768) * 22 / 32768;
          if (q < 70 + wob) dirt(S, xx, yy);
          else if (q < 125 + wob && q >= 85 + wob) {
            const uint32_t h = (uint32_t)(tileHash(sd ^ 0x5F2u, xx, yy) % 7);
            if (h < 3 && S.in(xx, yy) && !S.c.prop[S.idx(xx, yy)] && !groundSolid(S.at(xx, yy)) && !wetG(S.at(xx, yy)))
              S.p(xx, yy, h == 0 ? Prop::Boulder : h == 1 ? Prop::Rock : Prop::MossRock);
            else if (h == 3) dirt(S, xx, yy);
          }
        }
      putWild(S, x, y, Prop::StarShard);
      Rng r(sd ^ 0x5A4Du);
      const int n = 3 + r.irange(3);
      std::vector<GTile> put{GTile{x, y}};
      for (int k2 = 0; k2 < n * 3 && (int)put.size() <= n; k2++) {
        const int32_t a = r.irange(1024), rr = 2 + r.irange(3);
        const int32_t sx = x + icosR(a, rr), sy = y + isinR(a, rr) * 4 / 5;
        bool near = false;
        for (const GTile& t : put) if (std::abs(t.x - sx) <= 1 && std::abs(t.y - sy) <= 1) near = true;
        if (near) continue;
        putWild(S, sx, sy, Prop::StarShard);
        put.push_back(GTile{sx, sy});
      }
      break;
    }
    case WonderKind::DragonBones: {
      // (M2 fixer round 2) a ragged oval of trampled earth under the bones (not tile-sized blotches), and none on snow:
      // the bones lie in the snow
      const Biome hb = S.in(x, y) ? (Biome)S.c.biome[S.idx(x, y)] : Biome::Plains;
      const bool snowy = S.at(x, y) == Ground::Snow || S.at(x, y) == Ground::Ice || S.at(x, y) == Ground::Tundra || hb == Biome::Snow ||
                         hb == Biome::Taiga || hb == Biome::Mountain;
      if (!snowy)
        for (int32_t yy = y - 3; yy <= y + 2; yy++)
          for (int32_t xx = x - 7; xx <= x + 7; xx++) {
            const int32_t dx = xx - x, dy = yy - y;
            const int32_t q = (dx * dx * 100) / 36 + (dy * dy * 100) / 6;   // 100 on the oval
            const int32_t wob = (vnoiseQ(xx * 3, yy * 3, 2, mix64(sd ^ 0xD4Cu)) - 32768) * 30 / 32768;
            if (q < 75 + wob) dirt(S, xx, yy);
          }
      putWild(S, x, y, Prop::DragonBones);
      sprinkle(0x33u, 7, {Prop::Bones, Prop::SkullPile, Prop::Bones, Prop::Rock});
      break;
    }
    default: break;
  }
}

}  // namespace ew
