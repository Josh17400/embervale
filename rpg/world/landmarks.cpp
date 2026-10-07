// M2 landmarks (VISION_PLAN 11.5, 2.11): the named features of the land for the world map's labels. WORLD lane.
//
// Every label is decided by a pure rule and emitted by the one region that holds its label point:
//   - ranges: one per converging plate pair (a ridge system), labelled at its highest crest (rangeCrest: the pair's
//     boundary sampled on a 48-tile grid round its bisector, memoised per pair);
//   - peaks: a region's highest point of the relief field at level 6 or more, unless a range is labelled near it;
//   - passes: where a graph road crosses a ridge (the ridge field along the road above 0.30), at the stretch's crest;
//   - lakes of radius 18 or more, labelled at their centre; major rivers (2 tiles wide or more), once per river at
//     the middle of its wide reach (a tributary absorbed by a bigger river is not labelled);
//   - forests, marshes, deserts, hills and seas: the connected areas of one kind on a 48-tile grid over a landmark
//     cell (768 tiles, 3 x 3 regions; memoised), big enough to matter, labelled at the sample nearest the area's middle.
//     (M3c) The area's commonest biome names it: THE ASHEN WASTE, GREYMOOR HEATH, THE SILVERWOOD, THE RED BADLANDS,
//     THE WHISPERING BIRCHES, THE SUNKEN MANGROVES... The open grasslands (steppe, savanna, prairie, heath, chalk downs,
//     alpine meadows, the lake country) are labelled like hills, and a dragon's blight like a waste.
// Names are biome-flavoured and unique within any 3 x 3 block of regions: the region's (or the landmark cell's) slot
// picks a ninth of each name pool, and a region never repeats a name. Ids: makeId(rx, ry, IdKind::Poi, 0x800 | n).
#include <algorithm>
#include <cstdint>
#include <string>
#include "rpg/world/gen.h"

namespace ew {
using namespace gen;

namespace {
constexpr int32_t ACELL = 768, AS = 48, AN = ACELL / AS;   // landmark cells: 3 x 3 regions, sampled every 48 tiles

// word pools (27 = 9 slots x 3)
const char* kRangeA[] = {"GREY", "IRON", "STORM", "FROST", "RAVEN", "THUNDER", "BROKEN", "SILVER", "WOLF",
                         "ASH", "DRAGON", "GIANT", "HOLLOW", "WHITE", "BLACK", "CLOUD", "KING", "WINTER",
                         "SUNDERED", "HOWLING", "ANCIENT", "STONE", "EAGLE", "SKY", "OLD", "BITTER", "GLASS"};
const char* kRangeB[] = {"TEETH", "SPINE", "PEAKS", "MOUNTAINS", "FANGS", "CROWNS", "RIDGE", "HORNS", "SPIRES"};
const char* kWord[] = {"WOLF", "RAVEN", "STAG", "HAWK", "BEAR", "OWL", "ELK", "FOX", "BOAR",
                       "THORN", "ASH", "OAK", "ELDER", "ROWAN", "YEW", "BIRCH", "HOLLY", "ALDER",
                       "SUN", "MOON", "STAR", "STORM", "MIST", "FROST", "EMBER", "DAWN", "DUSK"};
const char* kPeakA[] = {"STORM", "EAGLE", "IRON", "GREY", "WHITE", "RAVEN", "FROST", "THUNDER", "CLOUD",
                        "WOLF", "HAWK", "STAR", "SUN", "MOON", "DRAGON", "GIANT", "KING", "QUEEN",
                        "HOLLOW", "BROKEN", "SILVER", "GOLD", "ASH", "EMBER", "WINTER", "BLACK", "WIND"};
const char* kLakeA[] = {"MIRROR", "SILVER", "STILL", "DEEP", "BLUE", "GLASS", "COLD", "MOON", "SWAN",
                        "REED", "HERON", "CRANE", "GREY", "WHITE", "BLACK", "LONG", "BRIGHT", "SHADOW",
                        "LILY", "OTTER", "PIKE", "SALMON", "DREAM", "WHISPER", "SKY", "DUSK", "MAIDEN"};
const char* kForestA[] = {"WHISPERING", "OLD", "DEEP", "GREEN", "SHADOW", "TANGLED", "ANCIENT", "WILD", "MOSSY",
                          "SILENT", "ELDER", "DARK", "MISTY", "HOLLOW", "THORNY", "WEEPING", "KING'S", "HUNTER'S",
                          "WITCH", "STAG", "WOLF", "RAVEN", "BRIAR", "FERN", "OWL", "DUSK", "GLOAM"};
const char* kMarshA[] = {"BLACK", "SUNKEN", "DROWNED", "GREY", "STINKING", "SALLOW", "REED", "MIST", "BOG",
                         "WILLOW", "HAG'S", "LOST", "MURK", "WEEPING", "SLOW", "GLOOM", "FROG", "EEL",
                         "WIDOW'S", "CROW", "SEDGE", "MOSS", "RUSH", "LANTERN", "DEAD", "COLD", "SOUR"};
const char* kDesertA[] = {"RED", "BURNING", "GOLDEN", "SHIMMERING", "SCORCHED", "ENDLESS", "BONE", "COPPER", "SUN",
                          "AMBER", "DRY", "SILENT", "THIRSTING", "WHITE", "DUST", "GLASS", "SALT", "EMBER",
                          "HOWLING", "SAFFRON", "BARREN", "SHIFTING", "OCHRE", "BRASS", "DUNE", "MIRAGE", "ASH"};
const char* kHillA[] = {"GREEN", "ROLLING", "WINDY", "SHEPHERD'S", "BARROW", "CHALK", "HEATHER", "GORSE", "SKYLARK",
                        "BRACKEN", "BLUE", "LONG", "STONE", "CAIRN", "HARE", "PIPER'S", "BELL", "THISTLE",
                        "MEADOW", "SUMMER", "LARK", "WOLD", "FOLD", "OLD", "FAIRY", "HOLLOW", "BRAMBLE"};
const char* kSeaA[] = {"GREY", "SHIVERING", "SUNSET", "WHALE", "STORM", "SILVER", "SAPPHIRE", "SERPENT", "MISTY",
                       "NORTHERN", "AMBER", "BROKEN", "SINGING", "DROWNED", "WINTER", "PEARL", "DRAGON", "QUIET",
                       "IRON", "SALT", "WANDERING", "STARLIT", "CORAL", "KRAKEN", "GULL", "GLASS", "WIND"};

int slotOf(int32_t a, int32_t b) { return (int)floorMod(a, 3) + 3 * (int)floorMod(b, 3); }
}  // namespace

// the biome areas of a landmark cell: (kind, label point, size in samples), in a fixed order
struct EndlessSource::Impl::AreaLabels {
  struct L { LandmarkKind kind; int32_t x, y, size; uint8_t flavour; };
  std::vector<L> labels;
};

std::shared_ptr<EndlessSource::Impl::AreaLabels> EndlessSource::Impl::areaLabels(int32_t ci, int32_t cj) {
  const uint64_t key = key2(ci, cj);
  auto it = areaMemo.find(key);
  if (it != areaMemo.end()) return it->second;
  auto A = std::make_shared<AreaLabels>();
  // class per sample: 0 none, 1 forest, 2 marsh, 3 desert (and waste), 4 hills (and open grassland), 5 sea; flavour: the
  // sample's eco (M3c: the area's commonest names it)
  std::vector<uint8_t> cls((size_t)AN * AN, 0), fl((size_t)AN * AN, 0);
  const int32_t x0 = ci * ACELL, y0 = cj * ACELL;
  for (int j = 0; j < AN; j++)
    for (int i = 0; i < AN; i++) {
      const int32_t x = x0 + i * AS + AS / 2, y = y0 + j * AS + AS / 2;
      const Coarse c = coarse(x, y);
      uint8_t k = 0;
      if (c.e < ELEV_SEA - Q(0.02)) k = 5;
      else if (c.e >= ELEV_SEA + Q(0.01)) {
        const Biome b = c.rock > Q(0.5) ? Biome::Mountain : classify(c.e, c.t, c.m, x, y, false);
        const int lv = levelOf(c.e);
        const Eco e = ecoFar(b, c, x, y);
        fl[(size_t)j * AN + i] = (uint8_t)e;
        const bool grass = e == Eco::Steppe || e == Eco::Savanna || e == Eco::Prairie || e == Eco::Heath || e == Eco::ChalkDowns ||
                           e == Eco::AlpineMeadow || e == Eco::LakeDistrict || e == Eco::StonePlains;
        if (e == Eco::Blight) k = 3;
        else if (b == Biome::Forest || b == Biome::Autumn || b == Biome::Taiga) k = 1;
        else if (b == Biome::Swamp) k = 2;
        else if (b == Biome::Desert) k = 3;
        else if (b == Biome::Plains && (lv == 2 || lv == 3 || grass)) k = 4;
      }
      cls[(size_t)j * AN + i] = k;
    }
  static const int minSize[6] = {0, 16, 5, 12, 10, 31};   // (in 48-tile samples)
  std::vector<uint8_t> seen((size_t)AN * AN, 0);
  std::vector<int> q;
  for (int s0 = 0; s0 < AN * AN; s0++) {
    if (!cls[(size_t)s0] || seen[(size_t)s0]) continue;
    const uint8_t k = cls[(size_t)s0];
    q.assign(1, s0);
    seen[(size_t)s0] = 1;
    int64_t sx = 0, sy = 0;
    int ecoN[(int)Eco::COUNT] = {};
    for (size_t h = 0; h < q.size(); h++) {
      const int c = q[h], i = c % AN, j = c / AN;
      sx += i; sy += j;
      ecoN[fl[(size_t)c] % (int)Eco::COUNT]++;
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int d = 0; d < 4; d++) {
        const int ni = i + dx[d], nj = j + dy[d];
        if (ni < 0 || nj < 0 || ni >= AN || nj >= AN) continue;
        const int n = nj * AN + ni;
        if (seen[(size_t)n] || cls[(size_t)n] != k) continue;
        seen[(size_t)n] = 1;
        q.push_back(n);
      }
    }
    if ((int)q.size() < minSize[k]) continue;
    // the label point: the sample of the area nearest its centroid (doubled to stay in integers)
    const int64_t mx2 = 2 * sx, my2 = 2 * sy, n = (int64_t)q.size();
    int best = q[0];
    int64_t bd = INT64_MAX;
    for (int c : q) {
      const int64_t dx = 2 * (c % AN) * n - mx2, dy = 2 * (c / AN) * n - my2, d = dx * dx + dy * dy;
      if (d < bd) { bd = d; best = c; }
    }
    uint8_t flav = 0;
    {
      int bestN = -1;
      for (int b = 0; b < (int)Eco::COUNT; b++) if (ecoN[b] > bestN) { bestN = ecoN[b]; flav = (uint8_t)b; }
    }
    static const LandmarkKind kinds[6] = {LandmarkKind::Forest, LandmarkKind::Forest, LandmarkKind::Marsh, LandmarkKind::Desert, LandmarkKind::Hills, LandmarkKind::Sea};
    A->labels.push_back({kinds[k], x0 + (best % AN) * AS + AS / 2, y0 + (best / AN) * AS + AS / 2, (int32_t)q.size() * AS, flav});
  }
  if (areaMemo.size() > 512) areaMemo.clear();
  areaMemo.emplace(key, A);
  return A;
}

// the highest crest of the ranges round a point: the converging boundary of the plate pair holding (x, y), sampled
// round its bisector (a 48-tile grid, 2400 tiles along, +-192 across); memoised per pair
EndlessSource::Impl::RangeCrest EndlessSource::Impl::rangeCrest(int32_t px, int32_t py) {
  int32_t ridge = 0, rift = 0;
  uint64_t pair = 0;
  plates(px, py, ridge, rift, &pair);
  auto it = crestMemo.find(pair);
  if (it != crestMemo.end()) return it->second;
  RangeCrest rc;
  rc.key = (uint32_t)(pair >> 20);
  // the two plate points of the pair: the nearest two to the (unwarped) point, searched round it
  const int32_t PC = 1536;
  const int32_t cx0 = floorDiv(px, PC), cy0 = floorDiv(py, PC);
  int32_t ax = 0, ay = 0, bx = 0, by = 0;
  bool found = false;
  for (int32_t cy = cy0 - 2; cy <= cy0 + 2 && !found; cy++)
    for (int32_t cx = cx0 - 2; cx <= cx0 + 2 && !found; cx++)
      for (int32_t dy = cy - 1; dy <= cy + 1 && !found; dy++)
        for (int32_t dx = cx - 1; dx <= cx + 1 && !found; dx++) {
          if (dx == cx && dy == cy) continue;
          const uint64_t ka = key2(cx, cy), kb = key2(dx, dy);
          const uint64_t pk = ka < kb ? mix64(ka) ^ kb : mix64(kb) ^ ka;
          if (pk != pair) continue;
          const PlatePt A = platePoint(cx, cy), B = platePoint(dx, dy);
          ax = A.x; ay = A.y; bx = B.x; by = B.y;
          found = true;
        }
  if (found) {
    const int32_t mx = (ax + bx) / 2, my = (ay + by) / 2;
    const int32_t L = std::max(1, idist(ax, ay, bx, by));
    // along: perpendicular to AB; across: along AB
    const int64_t ux = -(int64_t)(by - ay), uy = (int64_t)(bx - ax);
    int32_t bestE = INT32_MIN;
    for (int32_t s = -1200; s <= 1200; s += 48)
      for (int32_t t = -192; t <= 192; t += 48) {
        const int32_t x = mx + (int32_t)((ux * s + (int64_t)(bx - ax) * t) / L), y = my + (int32_t)((uy * s + (int64_t)(by - ay) * t) / L);
        int32_t rg = 0, rf = 0;
        uint64_t pk = 0;
        plates(x, y, rg, rf, &pk);
        if (pk != pair || rg < Q(0.30)) continue;
        const int32_t e = elevation(x, y, nullptr, nullptr, 2);
        if (e < ELEV_SEA + Q(0.02)) continue;
        if (e > bestE) { bestE = e; rc.x = x; rc.y = y; rc.e = e; rc.ok = true; }
      }
  }
  if (crestMemo.size() > 4096) crestMemo.clear();
  crestMemo.emplace(pair, rc);
  return rc;
}

void EndlessSource::Impl::planLandmarks(int32_t rx, int32_t ry, RegionData& D) {
  RegionPlan& R = D.plan;
  const int32_t x0 = rx * REGION, y0 = ry * REGION, x1 = x0 + REGION, y1 = y0 + REGION;
  auto inR = [&](int32_t x, int32_t y) { return x >= x0 && y >= y0 && x < x1 && y < y1; };
  const int slot = slotOf(rx, ry);
  const uint64_t rh = cellSeed(seed, tag("landmark.region"), rx, ry);
  auto add = [&](LandmarkKind k, int32_t x, int32_t y, int32_t size, std::string name) {
    for (const LandmarkPlan& o : R.landmarks)
      if (o.name == name) name = std::string(slot & 1 ? "OLD " : "GREAT ") + name;   // (a second one of a name in a region)
    LandmarkPlan l;
    l.id = makeId(rx, ry, IdKind::Poi, 0x800u | (uint32_t)R.landmarks.size());
    l.kind = k; l.x = x; l.y = y; l.size = size; l.name = name;
    R.landmarks.push_back(l);
  };
  auto pickSlot = [&](const char* const* pool, uint64_t h) { return std::string(pool[(size_t)slot + 9 * (h % 3)]); };
  // ---- ranges: the crests of the converging pairs round the region (sampled at its corners and middle)
  std::vector<uint32_t> rangesDone;
  int32_t crestX = INT32_MIN, crestY = 0;
  for (int k = 0; k < 9; k++) {
    const int32_t sx = x0 - 384 + (k % 3) * 512, sy = y0 - 384 + (k / 3) * 512;
    const RangeCrest c = rangeCrest(sx, sy);
    if (!c.ok || !inR(c.x, c.y) || std::find(rangesDone.begin(), rangesDone.end(), c.key) != rangesDone.end()) continue;
    rangesDone.push_back(c.key);
    const uint64_t h = mix64(rh ^ c.key);
    const TileF f = tile(c.x, c.y);
    std::string nm;
    if (f.biome == Biome::Snow || f.t < Q(0.30)) nm = "THE " + pickSlot(kRangeA, h) + " " + kRangeB[(h >> 8) % 9];
    else if (f.biome == Biome::Desert) nm = "THE " + pickSlot(kDesertA, h) + " " + kRangeB[(h >> 8) % 9];
    else nm = "THE " + pickSlot(kRangeA, h) + " " + kRangeB[(h >> 8) % 9];
    add(LandmarkKind::Range, c.x, c.y, 1200, nm);
    crestX = c.x; crestY = c.y;
  }
  // ---- the region's highest peak (level 6 or more), when no range is labelled near it
  {
    int32_t be = INT32_MIN, bx = 0, by = 0;
    for (int32_t y = y0 + 8; y < y1; y += 16)
      for (int32_t x = x0 + 8; x < x1; x += 16) {
        const int32_t e = reliefE(x, y);
        if (e > be) { be = e; bx = x; by = y; }
      }
    if (levelOf(be) >= 6 && !tile(bx, by).sea && (crestX == INT32_MIN || dist2(bx, by, crestX, crestY) > 160ll * 160ll)) {
      const uint64_t h = mix64(rh ^ 0x9EA4u);
      const std::string w = pickSlot(kPeakA, h);
      const uint32_t form = (uint32_t)((h >> 9) % 3);
      add(LandmarkKind::Peak, bx, by, 60, form == 0 ? "MOUNT " + w : form == 1 ? w + " PEAK" : w + "HORN");
    }
  }
  // ---- passes: where a road crosses a ridge (a road crossing the same range twice close by is named once)
  std::vector<GTile> passes;
  for (auto& e : D.edges) {
    int32_t bestR = 0, bx = 0, by = 0;
    bool inRun = false;
    auto flush = [&]() {
      bool near = false;
      for (const GTile& q : passes) if (dist2(q.x, q.y, bx, by) < 96ll * 96ll) near = true;
      if (inRun && bestR > Q(0.30) && inR(bx, by) && !near) {
        passes.push_back(GTile{bx, by});
        const uint64_t h = mix64(rh ^ e->key ^ (uint64_t)(uint32_t)bx);
        const std::string w = std::string(kWord[(size_t)slot + 9 * ((h + R.landmarks.size()) % 3)]);
        const uint32_t form = (uint32_t)((h >> 11) % 3);
        add(LandmarkKind::Pass, bx, by, 40, form == 0 ? w + " PASS" : form == 1 ? w + "GATE PASS" : "THE " + w + " PASS");
      }
      inRun = false; bestR = 0;
    };
    int32_t step = 0;
    for (const GTile& g : e->pts) {
      if (++step % 3) continue;
      if (g.x < x0 - 64 || g.y < y0 - 64 || g.x >= x1 + 64 || g.y >= y1 + 64) { flush(); continue; }
      int32_t rg = 0, rf = 0;
      plates(g.x, g.y, rg, rf);
      const int32_t land = rg;   // (the plate ridge; the road only crosses it on land)
      if (land > Q(0.22)) {
        if (!inRun) { inRun = true; bestR = 0; }
        if (land > bestR) { bestR = land; bx = g.x; by = g.y; }
      } else flush();
    }
    flush();
  }
  // ---- lakes and major rivers
  {
    std::shared_ptr<const RegionHydro> H = hydro(rx, ry);
    int nl = 0;
    for (const Lake& L : H->lakes) {
      if (L.r < 18 || !inR(L.x, L.y)) continue;
      const uint64_t h = mix64(rh ^ ((uint64_t)L.seed << 5));
      const std::string a = std::string(kLakeA[(size_t)slot + 9 * ((h + (uint64_t)nl++) % 3)]);
      const uint32_t form = (uint32_t)((h >> 13) % 3);
      add(LandmarkKind::Lake, L.x, L.y, L.r * 2, form == 0 ? a + "MERE" : form == 1 ? "LAKE " + a : a + " WATER");
    }
    std::vector<uint32_t> riversSeen;
    for (const RiverSeg& s : H->segs) {
      if (s.w < 2 || !s.river || std::find(riversSeen.begin(), riversSeen.end(), s.river) != riversSeen.end()) continue;
      riversSeen.push_back(s.river);
      std::shared_ptr<const Trace> T = trace(riverCellX(s.river), riverCellY(s.river));
      int first = -1, last = -1;
      for (int n = 0; n < (int)T->segs.size(); n++)
        if (T->segs[(size_t)n].w >= 2) { if (first < 0) first = n; last = n; }
      if (first < 0 || last - first < 12) continue;   // (a short reach is no major river)
      const RiverSeg& m = T->segs[(size_t)((first + last) / 2)];
      const int32_t lx = (m.x0 + m.x1) / 2, ly = (m.y0 + m.y1) / 2;
      if (!inR(lx, ly)) continue;
      // labelled only where this river is the one the index keeps (a tributary absorbed by a bigger river is not)
      bool own = false;
      for (const RiverSeg& o : H->segs) if (o.x0 == m.x0 && o.y0 == m.y0 && o.x1 == m.x1 && o.y1 == m.y1 && o.river == s.river) own = true;
      if (!own) continue;
      add(LandmarkKind::River, lx, ly, (last - first + 1) * 24, "THE " + riverName(s.river));
    }
  }
  // ---- forests, marshes, deserts, hills, seas: the landmark cell's areas whose label point lies in this region
  {
    const int32_t ci = floorDiv(x0, ACELL), cj = floorDiv(y0, ACELL);
    std::shared_ptr<AreaLabels> A = areaLabels(ci, cj);
    const int cslot = slotOf(ci, cj);
    // (the k-th area of a kind in the cell takes word k % 3 of the cell's ninth and form k / 3: never two alike)
    int seen[(int)LandmarkKind::COUNT] = {};
    const uint64_t ch = mix64(seed ^ tag("landmark.area") ^ key2(ci, cj));
    for (const AreaLabels::L& a : A->labels) {
      const int kn = seen[(int)a.kind]++;
      if (!inR(a.x, a.y)) continue;
      auto w = [&](const char* const* pool) { return std::string(pool[(size_t)cslot + 9 * ((ch + (uint64_t)kn) % 3)]); };
      std::string nm;
      const uint32_t form = (uint32_t)((((ch >> 8) % 3) + (uint64_t)(kn / 3)) % 3);
      const Eco fe = (Eco)a.flavour;
      auto two = [&](const std::string& p, const std::string& q) { return form == 1 ? q : p; };
      switch (a.kind) {
        case LandmarkKind::Forest:
          // (M3c) by the wood's commonest biome
          switch (fe) {
            case Eco::Taiga: nm = two("THE " + w(kForestA) + " PINES", "THE " + w(kForestA) + " TAIGA"); break;
            case Eco::TaigaBog: nm = two("THE " + w(kForestA) + " LARCHES", "THE " + w(kMarshA) + " MUSKEG"); break;
            case Eco::AutumnWood: nm = two("THE " + w(kForestA) + " WEALD", "THE " + w(kForestA) + " WOODS"); break;
            case Eco::BirchWood: nm = two("THE " + w(kForestA) + " BIRCHES", w(kForestA) + " BIRCHWOOD"); break;
            case Eco::GiantForest: nm = two("THE " + w(kForestA) + " GIANTS", "THE ELDERWOOD OF " + w(kWord)); break;
            case Eco::DarkForest: nm = two("THE " + w(kForestA) + " MIRKWOOD", "THE DARKWOOD OF " + w(kWord)); break;
            case Eco::BlossomGrove: nm = two("THE " + w(kWord) + " BLOSSOM VALE", "THE PETAL GROVES"); break;
            case Eco::BambooForest: nm = two("THE " + w(kForestA) + " BAMBOO", "THE " + w(kWord) + " CANEBRAKE"); break;
            case Eco::Jungle: nm = two("THE " + w(kForestA) + " JUNGLE", "THE " + w(kWord) + " TANGLE"); break;
            case Eco::MushroomForest: nm = two("THE " + w(kForestA) + " TOADSTOOLS", "THE GLOWCAP GROVE"); break;
            case Eco::Silverwood: nm = two("THE SILVERWOOD", "THE " + w(kWord) + " SILVERWOOD"); break;
            default: nm = form == 0 ? "THE " + w(kForestA) + " WOOD" : form == 1 ? "THE " + w(kForestA) + " FOREST" : w(kForestA) + " FOREST"; break;
          }
          break;
        case LandmarkKind::Marsh:
          switch (fe) {
            case Eco::PeatBog: nm = two("THE " + w(kMarshA) + " BOG", w(kMarshA) + " MOSS"); break;
            case Eco::Mangrove: nm = two("THE " + w(kMarshA) + " MANGROVES", "THE " + w(kMarshA) + " ROOTS"); break;
            case Eco::FloodedForest: nm = two("THE " + w(kMarshA) + " FLOODWOOD", "THE DROWNED WOOD OF " + w(kWord)); break;
            default: nm = form == 0 ? "THE " + w(kMarshA) + " FEN" : form == 1 ? w(kMarshA) + " MIRE" : "THE " + w(kMarshA) + " MARSHES"; break;
          }
          break;
        case LandmarkKind::Desert:
          switch (fe) {
            case Eco::AshFields: nm = two("THE ASHEN WASTE", "THE " + w(kDesertA) + " CINDERS"); break;
            case Eco::CrystalBarrens: nm = two("THE " + w(kDesertA) + " SHARDLANDS", "THE CRYSTAL BARRENS"); break;
            case Eco::PetrifiedForest: nm = two("THE " + w(kDesertA) + " STONEWOOD", "THE PETRIFIED GROVES"); break;
            case Eco::Badlands: nm = two("THE " + w(kDesertA) + " BADLANDS", "THE " + w(kDesertA) + " MESAS"); break;
            case Eco::SaltFlats: nm = two("THE " + w(kDesertA) + " SALT PAN", "THE WHITE FLATS"); break;
            case Eco::StonyDesert: nm = two("THE " + w(kDesertA) + " STONEFIELD", "THE " + w(kDesertA) + " HAMADA"); break;
            case Eco::Scrubland: nm = two("THE " + w(kDesertA) + " SCRUB", "THE " + w(kDesertA) + " THORNLANDS"); break;
            case Eco::Blight: nm = two("THE " + w(kMarshA) + " BLIGHT", "THE BLIGHTED REACH"); break;
            default: nm = form == 0 ? "THE " + w(kDesertA) + " WASTE" : form == 1 ? "THE " + w(kDesertA) + " SANDS" : "THE " + w(kDesertA) + " DUNES"; break;
          }
          break;
        case LandmarkKind::Hills:
          switch (fe) {
            case Eco::Heath: {
              const std::string hw = w(kHillA);   // GREYMOOR HEATH (a word with an apostrophe keeps its own: THE PIPER'S MOOR)
              nm = form != 1 && hw.find('\'') == std::string::npos ? hw + "MOOR HEATH" : "THE " + hw + " MOOR";
              break;
            }
            case Eco::ChalkDowns: nm = two("THE " + w(kHillA) + " DOWNS", "THE WHITE DOWNS"); break;
            case Eco::AlpineMeadow: nm = two("THE " + w(kHillA) + " HIGH PASTURES", "THE " + w(kHillA) + " ALPS"); break;
            case Eco::Steppe: nm = two("THE " + w(kDesertA) + " STEPPE", "THE " + w(kWord) + " STEPPE"); break;
            case Eco::Savanna: nm = two("THE " + w(kDesertA) + " SAVANNA", "THE " + w(kWord) + " PLAINS"); break;
            case Eco::Prairie: nm = two("THE " + w(kHillA) + " GRASSLANDS", "THE " + w(kWord) + " PRAIRIE"); break;
            case Eco::LakeDistrict: nm = two("THE " + w(kLakeA) + " MERES", "THE " + w(kLakeA) + " TARNS"); break;
            case Eco::StonePlains: nm = two("THE PLAIN OF STONES", "THE " + w(kHillA) + " MENHIRS"); break;
            default: nm = form == 0 ? "THE " + w(kHillA) + " HILLS" : form == 1 ? "THE " + w(kHillA) + " DOWNS" : "THE " + w(kHillA) + " FELLS"; break;
          }
          break;
        default: nm = "THE " + w(kSeaA) + " SEA"; break;
      }
      add(a.kind, a.x, a.y, a.size, nm);
    }
  }
}

}  // namespace ew
