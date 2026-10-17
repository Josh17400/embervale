// M12 "Rise and Fall": a lord's works and people stamped into the window (rpg/sim/lord_ops.h). LORD lane. The M7 home's
// pattern (home::stampWindow, read, not edited): whenever the window's tiles were rewritten (World::placeSerial) or the
// works changed, the props this file put down last time are taken up again (only where they still stand, so a rewritten
// chunk keeps the generator's tiles) and every loaded holding is laid out afresh on the generator's ground:
//   BANNER      Prop::LordBanner on the green by the heart (the view paints it in the owner's arms)
//   HALL        two Prop::Banner flanking the door of the village's largest house (the lord's hall)
//   MARKET      Prop::MarketCross on the green, baskets and sacks at its foot
//   SHRINE      Prop::Shrine at the green's edge with flowers
//   GRANARY     a yard of haystacks, sacks and a barrel by the green
//   MILITIA     a drill yard: two training dummies and a crate (the militiamen stand by it)
//   WATCHTOWER  Prop::Watchtower just outside the ring, by the road that leaves the village
//   PALISADE    a ring of stakes round the village, wobbling with the land (never a box), 4-connected so the runs join,
//               open wherever a road, a bridge or water crosses it, never on a doorstep; trees on the line felled;
//               lone stakes and stubs under 3 taken away. Raised a stretch at a time while it is being built.
//   WALLS       the same ring in stone (Prop::GardenWall) once raised (the palisade's line, the stakes gone)
//   (a work going up) a Prop::Scaffold and a woodpile where it will stand
// Solid as their props say (propSolid), never on a road, bridge, doorstep, building, wall, home plot, cliff or water.
// The steward (Role::Steward, named from Holding::steward in the village's tongue) stands by the banner; the militiamen
// (up to 4 of Holding::militia) by their yard. They are runtime actors (never saved, respawned near the seat).
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstddef>
#include <unordered_set>
#include "rpg/art/art_props.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/world/source.h"

using art::Prop;

namespace {
constexpr int LORD_SLOT = 2100000;   // Actor::slot: + (site hash % 40000) * 16 (+0 the steward, +1.. the militiamen)
constexpr int NEAR = 72;             // tiles: the seat's people stand while the player is this near

struct Rec { int32_t gx, gy; uint8_t was, put, wasSolid; uint16_t work; };
struct Mem {
  const Game* g = nullptr;
  uint32_t serial = 0;
  uint64_t sig = 0;
  bool dirty = true;
  float actorT = 0;
  std::vector<Rec> recs;
};
std::vector<Mem>& mems() { static std::vector<Mem> m; return m; }
Mem& memOf(const Game& g) {
  std::vector<Mem>& m = mems();
  for (Mem& x : m) if (x.g == &g) return x;
  if (m.size() >= 8) m.erase(m.begin());
  m.push_back(Mem());
  m.back().g = &g;
  return m.back();
}

uint32_t hmix(uint64_t a, uint32_t b) { return (uint32_t)ew::mix64(a ^ ((uint64_t)b * 0x9E3779B97F4A7C15ull)); }
int slotBase(ew::Gid site) { return LORD_SLOT + (int)(hmix(site, 0x51u) % 40000u) * 16; }
bool lordSlot(int slot) { return slot >= LORD_SLOT && slot < LORD_SLOT + 40000 * 16; }

// ---------------------------------------------------------------- the layout
constexpr int F_PLAZA = 1, F_TREE = 2, F_FARM = 4, F_TALL = 8, F_WILD = 16;   // F_TALL: a tall sprite (nothing solid where it would overlap)

struct Lay {
  Game& g;
  Map& M;
  Mem& mem;
  const Site& s;
  int si;
  uint64_t key;
  Lay(Game& gg, Mem& m, int site) : g(gg), M(gg.world.over), mem(m), s(gg.world.sites[(size_t)site]), si(site), key(gg.world.sites[(size_t)site].id) {}

  bool doorstep(int x, int y) const {
    // the tile in front of a door, and the one in front of that (the path out)
    for (int k = 1; k <= 2; k++) {
      const int yy = y - k;
      if (!M.in(x, yy)) continue;
      const int b = M.bldgAt[(size_t)yy * M.w + x];
      if (b >= 0 && b < (int)M.bldgs.size()) {
        const Bldg& B = M.bldgs[(size_t)b];
        if (x == B.doorX() && yy == B.doorY()) return true;
      }
    }
    return false;
  }
  bool free(int x, int y, int fl) const {
    if (!M.in(x, y)) return false;
    const size_t i = (size_t)y * M.w + x;
    const Ground gr = (Ground)M.ground[i];
    if (groundSolid(gr) || gr == Ground::Road || gr == Ground::Bridge || gr == Ground::Swamp || gr == Ground::Ice || gr == Ground::Lava || gr == Ground::Void) return false;
    if (gr == Ground::Plaza && !(fl & F_PLAZA)) return false;
    if (gr == Ground::Farmland && !(fl & F_FARM)) return false;
    if (M.bldgAt[i] >= 0 || M.wall[i]) return false;
    if (!M.height.empty() && (M.height[i] & ((fl & F_WILD) ? Map::HEIGHT_FALL : (Map::HEIGHT_STEP | Map::HEIGHT_FALL)))) return false;
    const int p = M.prop[i];
    if (p) {
      const Prop pp = (Prop)(p - 1);
      const bool cover = !propSolid(pp) && art::isFloraProp(pp);
      // (F_WILD: the ring's line clears the wild in its way too, a boulder or a thicket, so the run has no gap)
      const bool wild = (fl & F_WILD) && (art::isWildSolidProp(pp) || art::isFloraProp(pp));
      if (!cover && !wild && !((fl & F_TREE) && art::isTreeProp(pp))) return false;
    }
    if (doorstep(x, y)) return false;
    if (g.home.plotAt(g.world.ox + x, g.world.oy + y) >= 0) return false;
    return true;
  }
  // a roof south of the tile reaches up over it in the 3/4 view (a house's roof hides what stands 1-3 tiles north of it)
  bool underRoof(int x, int y) const {
    for (int k = 1; k <= 3; k++)
      if (M.in(x, y + k) && M.bldgAt[(size_t)(y + k) * M.w + x] >= 0) return true;
    return false;
  }
  // a block (w x h) free with a margin of `m` tiles holding no building, wall or road (nothing walled off)
  bool block(int x0, int y0, int w, int h, int m, int fl) const {
    for (int y = y0; y < y0 + h; y++)
      for (int x = x0; x < x0 + w; x++) if (!free(x, y, fl) || underRoof(x, y)) return false;
    for (int y = y0 - m; y < y0 + h + m; y++)
      for (int x = x0 - m; x < x0 + w + m; x++) {
        if (!M.in(x, y)) return false;
        const size_t i = (size_t)y * M.w + x;
        if (M.bldgAt[i] >= 0 || M.wall[i]) return false;
        if (x >= x0 && x < x0 + w && y >= y0 && y < y0 + h) continue;
        const int p = M.prop[i];
        if (p && propSolid((Prop)(p - 1)) && !art::isTreeProp((Prop)(p - 1))) return false;   // no prop jammed against it
      }
    // a tall sprite (a standard, a market cross, a tower) rises up to 4 tiles over the ground north of it and is itself
    // hidden by anything tall 1-2 tiles south: nothing solid there (trees behind it are fine: the wood stands back)
    if (fl & F_TALL)
      for (int y = y0 - 4; y <= y0 + h + 1; y++)
        for (int x = x0 - 1; x <= x0 + w; x++) {
          if (!M.in(x, y)) continue;
          if (x >= x0 && x < x0 + w && y >= y0 && y < y0 + h) continue;
          const int p = M.prop[(size_t)y * M.w + x];
          if (!p || !propSolid((Prop)(p - 1))) continue;
          // a crown right behind it tangles with it; one right in front hides its foot (M12 fixer: the standard behind a
          // cherry tree)
          if (!art::isTreeProp((Prop)(p - 1)) || (y >= y0 - 2 && y < y0) || y >= y0 + h) return false;
        }
    // (M12 fixer) a standard keeps its distance from the village's own flags and banners (never two flags of one arms
    // side by side on the green)
    if (avoidFlags > 0)
      for (int y = y0 - avoidFlags; y <= y0 + h + avoidFlags; y++)
        for (int x = x0 - avoidFlags; x <= x0 + w + avoidFlags; x++) {
          if (!M.in(x, y)) continue;
          const int p = M.prop[(size_t)y * M.w + x];
          if (p == (int)Prop::Banner + 1 || p == (int)Prop::LordBanner + 1 || p == (int)Prop::MarketCross + 1) return false;
        }
    return true;
  }
  void put(int x, int y, Prop p, uint16_t work) {
    if (!M.in(x, y)) return;
    const size_t i = (size_t)y * M.w + x;
    mem.recs.push_back({g.world.ox + x, g.world.oy + y, M.prop[i], (uint8_t)((int)p + 1), M.solid[i], work});
    M.prop[i] = (uint8_t)((int)p + 1);
    if (M.bldgAt[i] < 0) M.solid[i] = (uint8_t)((propSolid(p) || M.wall[i]) ? 1 : 0);
  }
  // spots by distance from the heart (rings rmin..rmax), ties broken by a hash so no two villages repeat
  bool spot(int rmin, int rmax, int w, int h, int m, int fl, uint32_t salt, int& ox, int& oy) const {
    struct C { int d; uint32_t k; int x, y; };
    std::vector<C> cs;
    for (int dy = -rmax; dy <= rmax; dy++)
      for (int dx = -rmax; dx <= rmax; dx++) {
        const int d = dx * dx + dy * dy;
        if (d < rmin * rmin || d > rmax * rmax) continue;
        cs.push_back({d, hmix(key ^ ((uint64_t)(uint32_t)(dx + 512) << 20) ^ (uint64_t)(uint32_t)(dy + 512), salt), s.ex + dx, s.ey + dy});
      }
    // nearer first, but in bands of ~2 tiles so the hash spreads the choice round the green
    std::sort(cs.begin(), cs.end(), [](const C& a, const C& b) {
      const int ba = (int)std::sqrt((float)a.d) / 2, bb = (int)std::sqrt((float)b.d) / 2;
      return ba != bb ? ba < bb : a.k < b.k;
    });
    for (const C& c : cs)
      if (block(c.x, c.y, w, h, m, fl)) { ox = c.x; oy = c.y; return true; }
    return false;
  }

  // the ring: an ellipse round the village a little beyond its area, wobbling with low harmonics (organic, never a box),
  // traced 4-connected (a corner tile on every diagonal step) so a run of stakes joins without gaps
  std::vector<std::pair<int, int>> ring() const {
    const float cx = s.r.x + s.r.w * 0.5f, cy = s.r.y + s.r.h * 0.5f;
    const float rx = s.r.w * 0.5f + 2.5f, ry = s.r.h * 0.5f + 2.5f;
    const uint32_t h = hmix(key, 0x7121u);
    const float p1 = (h & 1023) / 163.0f, p2 = ((h >> 10) & 1023) / 163.0f, p3 = ((h >> 20) & 1023) / 163.0f;
    std::vector<std::pair<int, int>> out;
    int px = INT32_MIN, py = INT32_MIN;
    const float step = 0.5f / std::max(rx, ry);
    for (float a = 0; a < 6.2832f + step; a += step) {
      const float k = 1.0f + 0.07f * std::sin(2 * a + p1) + 0.05f * std::sin(3 * a + p2) + 0.03f * std::sin(5 * a + p3);
      const int x = (int)std::lround(cx + std::cos(a) * rx * k), y = (int)std::lround(cy + std::sin(a) * ry * k);
      if (x == px && y == py) continue;
      if (px != INT32_MIN && x != px && y != py) out.push_back({x, py});
      out.push_back({x, y});
      px = x; py = y;
    }
    // the loop closes on its first tile: drop the repeats
    std::unordered_set<uint64_t> seen;
    std::vector<std::pair<int, int>> uniq;
    for (auto& t : out) {
      const uint64_t k2 = ((uint64_t)(uint32_t)t.first << 32) | (uint32_t)t.second;
      if (seen.insert(k2).second) uniq.push_back(t);
    }
    return uniq;
  }
  int avoidFlags = 0;       // block(): tiles round a spot that hold no flag, banner or market cross
  bool dirtPaths = false;   // trodden dirt is the village's footpaths here (not the land's own ground): kept open
  bool roadAt(int x, int y) const {
    const Ground gr = M.at(x, y);
    return gr == Ground::Road || gr == Ground::Bridge || gr == Ground::Plaza || (dirtPaths && gr == Ground::Dirt);
  }
};

// lays out one holding's works (h: the holding; day: today, for a work going up)
void layHolding(Game& g, Mem& mem, int si, const lord::Holding& h) {
  Lay L(g, mem, si);
  const Site& s = L.s;
  const uint16_t W = h.works, B = h.building;
  int bx = s.ex, by = s.ey;
  auto scaffoldAt = [&](int x, int y, uint16_t work) {
    L.put(x, y, Prop::Scaffold, work);
    if (L.free(x + 1, y + 1, F_PLAZA)) L.put(x + 1, y + 1, Prop::Woodpile, work);
  };
  // ---- the hall: banners either side of the largest house's door
  if ((W | B) & lord::LW_HALL) {
    int best = -1, area = 0;
    for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount && b < (int)L.M.bldgs.size(); b++) {
      const Bldg& Bd = L.M.bldgs[(size_t)b];
      if (Bd.site != si) continue;
      const int a = Bd.r.w * Bd.r.h;
      if (a > area) { area = a; best = b; }
    }
    if (best >= 0) {
      const Bldg& Bd = L.M.bldgs[(size_t)best];
      const int dy = Bd.doorY() + 1;
      for (int side = -1; side <= 1; side += 2) {
        const int dx = Bd.doorX() + side;
        if (!L.free(dx, dy, F_PLAZA)) continue;
        L.put(dx, dy, (W & lord::LW_HALL) ? Prop::Banner : Prop::Scaffold, lord::LW_HALL);
        if (!(W & lord::LW_HALL)) break;   // one scaffold while it goes up
      }
    }
  }
  // ---- the banner on the green
  if ((W | B) & lord::LW_BANNER) {
    int x, y;
    L.avoidFlags = 5;
    bool found = L.spot(0, 9, 1, 1, 1, F_PLAZA | F_TALL, 1, x, y);
    L.avoidFlags = 3;
    if (!found) found = L.spot(0, 9, 1, 1, 1, F_PLAZA | F_TALL, 1, x, y);
    L.avoidFlags = 0;
    if (!found) found = L.spot(0, 8, 1, 1, 1, F_PLAZA | F_TALL, 1, x, y);
    if (found) {
      if (W & lord::LW_BANNER) L.put(x, y, Prop::LordBanner, lord::LW_BANNER);
      else L.put(x, y, Prop::Scaffold, lord::LW_BANNER);
      bx = x; by = y;
    }
  }
  // ---- the market: a cross with baskets at its foot; a village that has its cross already gets trestle tables of
  //      goods by it instead (never a second cross)
  if ((W | B) & lord::LW_MARKET) {
    bool hasCross = false;
    for (int y = s.r.y; y < s.r.y + s.r.h && !hasCross; y++)
      for (int x = s.r.x; x < s.r.x + s.r.w && !hasCross; x++) hasCross = L.M.propAt(x, y) == (int)Prop::MarketCross + 1;
    int x, y;
    if (hasCross) {
      int placed = 0;
      for (int k = 0; k < 2; k++) {
        if (!L.spot(2 + 3 * k, 12, 2, 1, 1, F_PLAZA, 2 + (uint32_t)k, x, y)) break;
        if (!(W & lord::LW_MARKET)) { scaffoldAt(x, y, lord::LW_MARKET); placed++; break; }
        L.put(x, y, Prop::MarketTable, lord::LW_MARKET);
        L.put(x + 1, y, Prop::Filler, lord::LW_MARKET);
        if (L.free(x - 1, y, F_PLAZA) && L.free(x - 1, y + 1, F_PLAZA | F_FARM)) L.put(x - 1, y, (hmix(L.key, 11 + (uint32_t)k) & 1) ? Prop::Baskets : Prop::Crate, lord::LW_MARKET);
        placed++;
      }
      (void)placed;
    } else if (L.spot(2, 11, 1, 1, 1, F_PLAZA | F_TALL, 2, x, y)) {
      if (W & lord::LW_MARKET) {
        L.put(x, y, Prop::MarketCross, lord::LW_MARKET);
        const int side = (hmix(L.key, 3) & 1) ? 1 : -1;
        if (L.free(x + side, y + 1, F_PLAZA) && L.free(x + side, y + 2, F_PLAZA | F_FARM)) L.put(x + side, y + 1, Prop::Baskets, lord::LW_MARKET);
        if (L.free(x - side, y + 1, F_PLAZA) && L.free(x - side, y + 2, F_PLAZA | F_FARM)) L.put(x - side, y + 1, Prop::Sacks, lord::LW_MARKET);
      } else {
        scaffoldAt(x, y, lord::LW_MARKET);
      }
    }
  }
  // ---- the shrine at the green's edge, flowers before it
  if ((W | B) & lord::LW_SHRINE) {
    int x, y;
    if (L.spot(6, 14, 1, 1, 1, F_TALL, 4, x, y)) {
      if (W & lord::LW_SHRINE) {
        L.put(x, y, Prop::Shrine, lord::LW_SHRINE);
        static const Prop fl[3] = {Prop::Flowers1, Prop::Flowers2, Prop::Flowers3};
        const uint32_t k = hmix(L.key, 5);
        if (L.free(x - 1, y + 1, 0) && !L.M.prop[(size_t)(y + 1) * L.M.w + x - 1]) L.put(x - 1, y + 1, fl[k % 3], lord::LW_SHRINE);
        if (L.free(x + 1, y + 1, 0) && !L.M.prop[(size_t)(y + 1) * L.M.w + x + 1]) L.put(x + 1, y + 1, fl[(k >> 4) % 3], lord::LW_SHRINE);
      } else {
        scaffoldAt(x, y, lord::LW_SHRINE);
      }
    }
  }
  // ---- the granary yard
  if ((W | B) & lord::LW_GRANARY) {
    int x, y;
    if (L.spot(5, 15, 3, 2, 1, 0, 6, x, y)) {
      if (W & lord::LW_GRANARY) {
        const bool flip = hmix(L.key, 7) & 1;
        L.put(flip ? x + 2 : x, y, Prop::Haystack, lord::LW_GRANARY);
        L.put(flip ? x : x + 2, y + 1, Prop::Haystack, lord::LW_GRANARY);
        L.put(x + 1, y, Prop::Sacks, lord::LW_GRANARY);
        L.put(flip ? x + 2 : x, y + 1, Prop::Barrel, lord::LW_GRANARY);
      } else {
        scaffoldAt(x, y, lord::LW_GRANARY);
      }
    }
  }
  // ---- the militia's drill yard
  if ((W | B) & lord::LW_MILITIA) {
    int x, y;
    if (L.spot(6, 16, 3, 2, 1, 0, 8, x, y)) {
      if (W & lord::LW_MILITIA) {
        L.put(x, y, Prop::TrainingDummy, lord::LW_MILITIA);
        L.put(x + 2, y + 1, Prop::TrainingDummy, lord::LW_MILITIA);
        L.put((hmix(L.key, 9) & 1) ? x + 2 : x, (hmix(L.key, 9) & 1) ? y : y + 1, Prop::Crate, lord::LW_MILITIA);
      } else {
        scaffoldAt(x, y, lord::LW_MILITIA);
      }
    }
  }
  // ---- the ring: the palisade (or the stone walls) and the watchtower by the road out
  const std::vector<std::pair<int, int>> ring = L.ring();
  {
    // the footpaths: a village's lanes of trodden dirt cross the ring like its roads (unless dirt is the land itself)
    int dirt = 0;
    for (const auto& t : ring) dirt += L.M.at(t.first, t.second) == Ground::Dirt;
    L.dirtPaths = dirt * 100 < (int)ring.size() * 35;
  }
  const bool walls = (W & lord::LW_WALLS) != 0, stakes = (W & lord::LW_PALISADE) != 0;
  const bool ringGoing = !walls && ((B & lord::LW_PALISADE) || (B & lord::LW_WALLS));
  if ((W | B) & lord::LW_WATCHTOWER) {
    // by the first road that leaves the village (in ring order from the south), a step to its side and outward
    const float cx = s.r.x + s.r.w * 0.5f, cy = s.r.y + s.r.h * 0.5f;
    int tx = INT32_MIN, ty = 0;
    std::vector<size_t> order(ring.size());
    for (size_t i = 0; i < ring.size(); i++) order[i] = i;
    // start from the ring tile due south of the centre (the view's front): a tower there reads best
    size_t south = 0;
    float bs = -1e9f;
    for (size_t i = 0; i < ring.size(); i++) if (ring[i].second - cy - std::fabs(ring[i].first - cx) > bs) { bs = ring[i].second - cy - std::fabs(ring[i].first - cx); south = i; }
    std::rotate(order.begin(), order.begin() + (std::ptrdiff_t)south, order.end());
    // (M12 fixer) the tower stands clear of the ring's line (its footprint and a tile round it hold no ring tile), so
    // the palisade or the wall runs whole past it instead of being stamped over
    std::unordered_set<uint64_t> ringSet;
    for (const auto& t : ring) ringSet.insert(((uint64_t)(uint32_t)t.first << 32) | (uint32_t)t.second);
    auto nearRing = [&](int x, int y) {
      for (int yy = y - 2; yy <= y + 1; yy++)
        for (int xx = x - 2; xx <= x + 2; xx++)
          if (ringSet.count(((uint64_t)(uint32_t)xx << 32) | (uint32_t)yy)) return true;
      return false;
    };
    auto tryAt = [&](int x, int y) {
      if (tx != INT32_MIN || nearRing(x, y)) return;
      if (L.block(x - 1, y - 1, 3, 2, 0, F_TALL)) { tx = x; ty = y; }
    };
    for (size_t k = 0; k < order.size() && tx == INT32_MIN; k++) {
      const auto& t = ring[order[k]];
      if (!L.roadAt(t.first, t.second)) continue;
      const float ox = t.first - cx, oy = t.second - cy, ol = std::max(1.0f, std::sqrt(ox * ox + oy * oy));
      for (int side = 2; side <= 4 && tx == INT32_MIN; side++)
        for (int sg = -1; sg <= 1 && tx == INT32_MIN; sg += 2)
          for (float out : {-3.0f, 3.5f}) {
            // along the ring (perpendicular to the outward direction), inside the ring by the gate first, else outside
            const int x = t.first + (int)std::lround(-oy / ol * side * sg + ox / ol * out);
            const int y = t.second + (int)std::lround(ox / ol * side * sg + oy / ol * out);
            tryAt(x, y);
          }
    }
    for (size_t k = 0; k < order.size() && tx == INT32_MIN; k += 3) {
      const auto& t = ring[order[k]];
      const float ox = t.first - cx, oy = t.second - cy, ol = std::max(1.0f, std::sqrt(ox * ox + oy * oy));
      tryAt(t.first - (int)std::lround(ox / ol * 3), t.second - (int)std::lround(oy / ol * 3));
      tryAt(t.first + (int)std::lround(ox / ol * 3.5f), t.second + (int)std::lround(oy / ol * 3.5f));
    }
    if (tx != INT32_MIN) {
      if (W & lord::LW_WATCHTOWER) L.put(tx, ty, Prop::Watchtower, lord::LW_WATCHTOWER);
      else scaffoldAt(tx, ty, lord::LW_WATCHTOWER);
    }
  }
  if (stakes || walls || ringGoing) {
    size_t n = ring.size();
    // a work going up: the stakes stand a stretch at a time from the south, the rest still to come
    if (ringGoing) {
      const lord::WorkInfo* wi = lord::workInfo(B);
      const int days = wi ? wi->days : 6;
      const int left = std::max(0, (int)h.buildDone - g.day);
      const float done = std::clamp(1.0f - (float)left / (float)std::max(1, days), 0.0f, 1.0f);
      n = stakes && (B & lord::LW_WALLS) ? ring.size() : (size_t)(ring.size() * std::max(0.08f, done));
    }
    const float cy = s.r.y + s.r.h * 0.5f;
    size_t start = 0;
    float bs = -1e9f;
    for (size_t i = 0; i < ring.size(); i++) if (ring[i].second - cy > bs) { bs = ring[i].second - cy; start = i; }
    // candidate stakes: free ground (trees on the line felled), never a road, plaza, bridge or doorstep
    std::vector<std::pair<int, int>> want;
    for (size_t k = 0; k < n && k < ring.size(); k++) {
      const auto& t = ring[(start + k) % ring.size()];
      if (L.free(t.first, t.second, F_TREE | F_FARM | F_WILD) && !L.roadAt(t.first, t.second)) want.push_back(t);
    }
    // lone stakes and stubs under 3 go: every run joins its neighbours or is not built at all
    std::unordered_set<uint64_t> set;
    auto K = [](int x, int y) { return ((uint64_t)(uint32_t)x << 32) | (uint32_t)y; };
    for (auto& t : want) set.insert(K(t.first, t.second));
    std::unordered_set<uint64_t> keep;
    for (auto& t : want) {
      if (keep.count(K(t.first, t.second))) continue;
      std::vector<std::pair<int, int>> comp{t}, stack{t};
      std::unordered_set<uint64_t> vis{K(t.first, t.second)};
      while (!stack.empty()) {
        const auto c = stack.back();
        stack.pop_back();
        static const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& d : d4) {
          const int nx = c.first + d[0], ny = c.second + d[1];
          const uint64_t nk = K(nx, ny);
          if (set.count(nk) && vis.insert(nk).second) { comp.push_back({nx, ny}); stack.push_back({nx, ny}); }
        }
      }
      if (comp.size() >= 3) for (auto& c : comp) keep.insert(K(c.first, c.second));
      else for (auto& c : comp) keep.insert(K(c.first, c.second) ^ 0x8000000000000000ull);   // seen, dropped
    }
    const Prop pr = walls ? Prop::GardenWall : Prop::Palisade;
    const uint16_t work = walls ? lord::LW_WALLS : lord::LW_PALISADE;
    for (auto& t : want) if (keep.count(K(t.first, t.second))) L.put(t.first, t.second, pr, work);
    // stone going up over the stakes: a scaffold at the south of the line
    if (stakes && (B & lord::LW_WALLS) && !want.empty()) {
      for (auto& t : want)
        if (keep.count(K(t.first, t.second))) {
          const int x = t.first, y = t.second + 1;
          if (L.free(x, y, 0) && L.free(x, y + 1, 0)) { L.put(x, y, Prop::Scaffold, lord::LW_WALLS); break; }
        }
    }
  }
  (void)bx; (void)by;
}

// the steward's / militia's look-up
int findLordActor(const Game& g, int slot) {
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (!a.fromMap && a.slot == slot && a.st != AState::Dead) return (int)i;
  }
  return -1;
}
// the tile the steward keeps to: beside the banner (or the heart)
bool stewardTile(const Game& g, const Mem& mem, const lord::Holding& h, int si, int& tx, int& ty) {
  const Site& s = g.world.sites[(size_t)si];
  tx = s.ex; ty = s.ey + 1;
  for (const Rec& r : mem.recs)
    if (r.put == (uint8_t)((int)Prop::LordBanner + 1) || (r.work == lord::LW_BANNER && r.put == (uint8_t)((int)Prop::Scaffold + 1))) {
      const int x = r.gx - g.world.ox, y = r.gy - g.world.oy;
      if (std::abs(x - s.ex) < 12 && std::abs(y - s.ey) < 12) { tx = x + 1; ty = y + 1; }
    }
  (void)h;
  return true;
}
bool yardTile(const Game& g, const Mem& mem, int si, int& tx, int& ty) {
  const Site& s = g.world.sites[(size_t)si];
  for (const Rec& r : mem.recs)
    if (r.work == lord::LW_MILITIA && r.put == (uint8_t)((int)Prop::TrainingDummy + 1)) {
      const int x = r.gx - g.world.ox, y = r.gy - g.world.oy;
      if (std::abs(x - s.ex) < 20 && std::abs(y - s.ey) < 20) { tx = x + 1; ty = y + 1; return true; }
    }
  tx = s.ex - 2; ty = s.ey + 2;
  return false;
}

void actorsStep(Game& g, Mem& mem) {
  std::vector<int> want;
  const lord::Lordship& L = g.lordship;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  if (!g.inside) {
    for (const lord::Holding& h : L.holdings) {
      const int si = g.world.siteHandle(h.site);
      if (si < 0) continue;
      const Site& s = g.world.sites[(size_t)si];
      if (std::fabs(s.ex - px) > NEAR || std::fabs(s.ey - py) > NEAR || !g.world.over.in(s.ex, s.ey)) continue;
      const int base = slotBase(h.site);
      const cult::Culture* C = g.world.cultureOf(si);
      // the steward
      want.push_back(base);
      if (findLordActor(g, base) < 0) {
        int tx, ty;
        stewardTile(g, mem, h, si, tx, ty);
        Actor a;
        a.id = LordOps::nextId(g);
        a.npc = true; a.human = true; a.hostile = false; a.faction = Faction::Town;
        a.p = LordOps::freeSpot(g, tx, ty); a.home = a.p; a.goal = a.p;
        a.site = -1; a.slot = base; a.fromMap = false;
        a.maxHp = a.hp = 50; a.radius = 5; a.speed = 30;
        a.role = Role::Steward;
        const bool female = ((h.steward >> 40) & 3) == 0;
        a.name = C ? cult::personName(*C, (uint32_t)h.steward, female) : std::string("THE STEWARD");
        Rng r((uint32_t)(h.steward >> 8) | 1u);
        LordOps::makeLook(g, a, Role::Steward, r);
        g.actors.push_back(a);
      }
      // the militiamen by their yard
      const int nm = (h.works & lord::LW_MILITIA) ? std::min<int>(4, h.militia) : 0;
      for (int k = 0; k < nm; k++) {
        const int slot = base + 1 + k;
        want.push_back(slot);
        if (findLordActor(g, slot) >= 0) continue;
        int tx, ty;
        yardTile(g, mem, si, tx, ty);
        Actor a;
        a.id = LordOps::nextId(g);
        a.npc = true; a.human = true; a.hostile = false; a.faction = Faction::Town;
        a.p = LordOps::freeSpot(g, tx + (k % 2 ? 1 : -1) * (1 + k / 2), ty + (k % 2)); a.home = a.p; a.goal = a.p;
        a.site = -1; a.slot = slot; a.fromMap = false;
        a.maxHp = a.hp = 60; a.radius = 5; a.speed = 42;
        a.role = Role::Villager;
        a.militia = true;
        const uint32_t seed = hmix(h.site, 0x3100u + (uint32_t)k);
        a.name = C ? cult::personName(*C, seed, false) : std::string("MILITIAMAN");
        Rng r(seed | 1u);
        LordOps::makeLook(g, a, Role::Villager, r);
        a.dmg = 8;   // the lord's levy: drilled, armed from the hall (Game::updateFolk arms a militiaman when he fights)
        g.actors.push_back(a);
      }
    }
  }
  for (size_t k = g.actors.size(); k-- > 1;) {
    const Actor& a = g.actors[k];
    if (a.fromMap || !lordSlot(a.slot)) continue;
    if (std::find(want.begin(), want.end(), a.slot) == want.end()) g.actors.erase(g.actors.begin() + (std::ptrdiff_t)k);
  }
}

uint64_t worksSig(const Game& g) {
  uint64_t h = 0x1EAFull ^ g.lordship.realm;
  for (const lord::Holding& x : g.lordship.holdings) {
    h = ew::mix64(h ^ x.site);
    h = ew::mix64(h ^ ((uint64_t)x.works << 16) ^ ((uint64_t)x.building << 32) ^ x.militia);
    // a ring going up grows each day
    if (x.building) h = ew::mix64(h ^ (uint64_t)std::max(0, (int)x.buildDone - g.day));
  }
  return h;
}
}  // namespace

void LordOps::restamp(Game& g) { memOf(g).dirty = true; }
void LordOps::forgetStamps(Game& g) {
  Mem& m = memOf(g);
  m.recs.clear();
  m.dirty = true;
}

void LordOps::stampStep(Game& g) {
  Mem& mem = memOf(g);
  Map& M = g.world.over;
  if (M.ground.empty() || M.solid.size() != M.ground.size() || M.prop.size() != M.ground.size()) return;
  const uint64_t sig = worksSig(g);
  if (mem.dirty || mem.serial != g.world.placeSerial || mem.sig != sig) {
    // take up what was put down (only where it still stands: a rewritten chunk has the generator's tiles again)
    for (const Rec& r : mem.recs) {
      const int x = r.gx - g.world.ox, y = r.gy - g.world.oy;
      if (!M.in(x, y)) continue;
      const size_t i = (size_t)y * M.w + x;
      if (M.prop[i] != r.put) continue;
      M.prop[i] = r.was;
      if (M.bldgAt[i] < 0) M.solid[i] = (uint8_t)(((r.was && propSolid((Prop)(r.was - 1))) || M.wall[i]) ? 1 : 0);
    }
    mem.recs.clear();
    for (const lord::Holding& h : g.lordship.holdings) {
      if (!(h.works | h.building)) continue;
      const int si = g.world.siteHandle(h.site);
      if (si < 0) continue;
      const Site& s = g.world.sites[(size_t)si];
      if (s.r.x + s.r.w + 12 < 0 || s.r.y + s.r.h + 12 < 0 || s.r.x - 12 >= M.w || s.r.y - 12 >= M.h) continue;
      layHolding(g, mem, si, h);
    }
    mem.serial = g.world.placeSerial;
    mem.sig = sig;
    mem.dirty = false;
    mem.actorT = 0;
  }
  if ((mem.actorT -= 1.0f / 60.0f) <= 0) {
    mem.actorT = 0.25f;
    actorsStep(g, mem);
  }
}

std::vector<LordOps::WorkTile> LordOps::stamped(const Game& g) {
  std::vector<WorkTile> out;
  for (const Mem& m : mems())
    if (m.g == &g)
      for (const Rec& r : m.recs) {
        const int x = r.gx - g.world.ox, y = r.gy - g.world.oy;
        if (g.world.over.in(x, y) && g.world.over.prop[(size_t)y * g.world.over.w + x] == r.put) out.push_back({r.gx, r.gy, r.put, r.work});
      }
  return out;
}

int LordOps::stewardActor(const Game& g, const lord::Holding& h) { return findLordActor(g, slotBase(h.site)); }
