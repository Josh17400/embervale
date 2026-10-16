// M10 Beyond the Mist (VISION_PLAN 2.12, 15.4, 15.31): the harbours. WORLD lane.
//
// Every SPF_PORT town and city (region.cpp's coast test: a sea tile on the ring just outside the nominal footprint)
// gets a harbour; a fishing village (the same test, a village) records a rowboat berth by its shore and builds nothing
// (VILLAGES NEVER CHANGE: rpg_test --villages). harbourOf plans the harbour from the elevation alone (waterE: the
// chunks' own sea and deep-water line), so portsIn is cheap (no settlement build) and exact (the chunks stamp what it
// says):
//   - the pier: a straight mole of Ground::Bridge, 3 tiles wide, from a shore tile (the landing) out along the seaward
//     axis, over the coastal shallows to the deep-water line, ending in a T-head 7 wide and 3 deep: the pier head
//     (PortPlan::pierX/Y) is the T's outer middle tile; the berth lies 3 tiles past it, in deep water;
//   - where the shallows run on past the pier's reach (14 tiles), the harbour is DREDGED: a basin round the berth and a
//     channel 5 wide out to the deep sea, marked by buoys;
//   - the moorings: deep berths beside the T's arms and along the mole (the view's moored ships);
//   - the dress (stampHarbour, sea props with their frozen footprints): mooring posts along the mole's edges, the
//     harbourmaster's desk, cargo, fish baskets and an anchor on the T-head, a net rack at the landing, the
//     shipwright's slipway on the shore beside it, buoys offshore; the keepers (Harbourmaster at the desk, Shipwright
//     at the slipway, sailors on the pier and at the door of the port's inn, the sailors' tavern).
// The town (town_dress.cpp) keeps the landing clear and joins it to its streets with a lane (SettlementCtx::harbour).
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include "rpg/world/gen.h"

namespace ew {
using namespace gen;
using art::Prop;

namespace {
constexpr int PIER_MAX = 14;   // the longest mole (tiles of water it crosses); beyond: dredged
}

bool EndlessSource::Impl::harbourOf(const Node& n, PortPlan& P) {
  auto it = portMemo.find(n.id);
  if (it != portMemo.end()) { if (it->second.first) P = it->second.second; return it->second.first; }
  if (portMemo.size() > 4096) portMemo.clear();
  PortPlan H;
  const bool ok = harbourPlan(n, H, true);
  portMemo[n.id] = {ok, H};
  if (ok) P = H;
  return ok;
}

// the plan itself (no memo; full: with the name, land and kingdom, which the start's port search goes without)
bool EndlessSource::Impl::harbourPlan(const Node& n, PortPlan& P, bool full) {
  // (region.cpp's coast test, exactly: a sea tile on the ring just outside the nominal footprint)
  const int32_t R0 = nominalR(n.type);
  bool coast = false;
  int32_t sx = n.x, sy = n.y;
  for (int k = 0; k < 12 && !coast; k++) {
    const int32_t a = k * (1024 / 12);
    const int32_t px = n.x + icosR(a, R0 + 18), py = n.y + isinR(a, R0 + 18);
    if (waterE(px, py) < ELEV_SEA) { coast = true; sx = px; sy = py; }   // (== tile().sea, without the biome)
  }
  if (!coast) return false;
  PortPlan H;
  H.site = n.id; H.type = n.type; H.x = n.x; H.y = n.y;
  H.harbour = n.type != SiteType::Village;
  if (full) {
    H.land = landAt(n.x, n.y);
    H.kingdom = kingdomAt(n.x, n.y);
    H.name = siteName(n);
  }
  auto sea = [&](int32_t x, int32_t y) { return waterE(x, y) < ELEV_SEA; };
  auto deep = [&](int32_t x, int32_t y) { return waterE(x, y) < ELEV_SEA - Q(0.035); };
  // the seaward axes: toward the coast sample, the dominant component first
  const int32_t vx = sx - n.x, vy = sy - n.y;
  // the seaward axes: toward the coast sample first (the dominant component, then the other), then the rest
  int axes[4][2];
  {
    const int ax = vx >= 0 ? 1 : -1, ay = vy >= 0 ? 1 : -1;
    if (std::abs(vx) >= std::abs(vy)) { axes[0][0] = ax; axes[0][1] = 0; axes[1][0] = 0; axes[1][1] = ay; }
    else { axes[0][0] = 0; axes[0][1] = ay; axes[1][0] = ax; axes[1][1] = 0; }
    axes[2][0] = -axes[1][0]; axes[2][1] = -axes[1][1];
    axes[3][0] = -axes[0][0]; axes[3][1] = -axes[0][1];
  }
  // the landing stays within the footprint (+24): the town's build covers it and joins it to its streets, and every
  // chunk that holds a piece of the harbour's dress also holds the town's buffer (the chunks' 32-tile margin)
  int fw = 0, fh = 0;
  settlementFootprint(n.type, H.harbour ? Archetype::Port : Archetype::Fishing, n.seed, fw, fh);
  const int32_t limX = fw / 2 + 24, limY = fh / 2 + 24;
  int64_t bestScore = INT64_MAX;
  int32_t bRootX = 0, bRootY = 0, bL = 0;
  int bdx = axes[0][0], bdy = axes[0][1];
  const int32_t span = R0 * 3 / 4;
  for (int ai = 0; ai < 4; ai++) {
    const int dx = axes[ai][0], dy = axes[ai][1], px = -dy, py = dx;
    for (int32_t o = -span; o <= span; o += 4) {
      const int32_t bx = n.x + px * o, by = n.y + py * o;
      if (sea(bx, by)) continue;
      // the shore along the axis
      int32_t t = 1;
      const int32_t tMax = R0 + 60;
      while (t <= tMax && !sea(bx + dx * t, by + dy * t)) t++;
      if (t > tMax) continue;
      const int32_t rx = bx + dx * (t - 1), ry = by + dy * (t - 1);   // the landing (land)
      if (std::abs(rx - n.x) > limX || std::abs(ry - n.y) > limY) continue;
      // the shallows along the axis, then the deep within 160 tiles and no land on the way (the channel's way out)
      int32_t L = -1;
      for (int32_t k = 1; k <= 160; k++) {
        const int32_t cx = rx + dx * k, cy = ry + dy * k;
        if (!sea(cx, cy) || (k > 2 && (!sea(cx + px * 2, cy + py * 2) || !sea(cx - px * 2, cy - py * 2)))) break;
        if (deep(cx, cy)) { L = k - 1; break; }
      }
      if (L < 0) continue;
      const int32_t len = std::clamp(L + 2, 6, PIER_MAX);
      // the mole three wide over water (its first two tiles may lean on the shore), the T-head 7 wide, the berth's water
      bool ok = true;
      for (int32_t k = 1; k <= len && ok; k++) {
        const int32_t cx = rx + dx * k, cy = ry + dy * k;
        ok = sea(cx, cy) && (k <= 2 || (sea(cx + px, cy + py) && sea(cx - px, cy - py)));
      }
      for (int32_t k = len - 2; k <= len && ok; k++)
        for (int32_t q = -3; q <= 3 && ok; q++) ok = sea(rx + dx * k + px * q, ry + dy * k + py * q);
      for (int32_t k = len + 1; k <= len + 5 && ok; k++)
        for (int32_t q = -2; q <= 2 && ok; q++) ok = sea(rx + dx * k + px * q, ry + dy * k + py * q);
      if (!ok) continue;
      const int64_t score = (int64_t)std::min<int32_t>(L, 60) * 3 + std::abs(o) + t + ai * 30;
      if (score < bestScore) { bestScore = score; bRootX = rx; bRootY = ry; bL = L; bdx = dx; bdy = dy; }
    }
  }
  if (bestScore == INT64_MAX) {
    // no clean mole anywhere: the coast sample's own shore (a fishing berth; a harbour on a broken coast keeps one too)
    H.rootX = H.pierX = n.x + (sx - n.x) * 9 / 10; H.rootY = H.pierY = n.y + (sy - n.y) * 9 / 10;
    H.berthX = sx; H.berthY = sy;
    H.harbour = false;
    P = H;
    return true;
  }
  const int px = -bdy, py = bdx;
  H.rootX = bRootX; H.rootY = bRootY;
  H.dirX = (int8_t)bdx; H.dirY = (int8_t)bdy;
  if (H.harbour) {
    const int32_t len = std::clamp(bL + 2, 6, PIER_MAX);
    H.pierLen = (uint8_t)len;
    H.dredged = bL + 2 > PIER_MAX;
    H.pierX = bRootX + bdx * len; H.pierY = bRootY + bdy * len;
    H.berthX = H.pierX + bdx * 3; H.berthY = H.pierY + bdy * 3;
    // the other berths: beside the T's arms (seaward of them) and along the mole, where open water lies all round
    // them (the dredged basin about each is then deep)
    auto berth = [&](int32_t x, int32_t y) {
      for (int32_t dy = -2; dy <= 2; dy++)
        for (int32_t dx = -2; dx <= 2; dx++)
          if (!sea(x + dx, y + dy)) return;
      H.moorings.push_back(GTile{x, y});
    };
    berth(H.pierX + bdx + px * 6, H.pierY + bdy + py * 6);
    berth(H.pierX + bdx - px * 6, H.pierY + bdy - py * 6);
    if (len >= 9) {
      berth(bRootX + bdx * (len / 2) + px * 3, bRootY + bdy * (len / 2) + py * 3);
      berth(bRootX + bdx * (len / 2) - px * 3, bRootY + bdy * (len / 2) - py * 3);
    }
  } else {
    // a fishing village: the landing and the shallow water before it (a rowboat's berth; no jetty is built)
    H.pierX = bRootX; H.pierY = bRootY;
    H.berthX = bRootX + bdx; H.berthY = bRootY + bdy;
  }
  P = H;
  return true;
}

// is a global tile of the harbour plan dredged deep water (the basin round the berth and the moorings, the channel out)
bool EndlessSource::Impl::harbourDeep(const PortPlan& P, int32_t x, int32_t y) {
  if (!P.harbour || !P.pierLen) return false;
  const int px = -P.dirY, py = P.dirX;
  // the basin: round the berth, and a little round each mooring
  if (dist2(x, y, P.berthX, P.berthY) <= 4 * 4 + 2) return true;
  for (const GTile& m : P.moorings) if (dist2(x, y, m.x, m.y) <= 2 * 2 + 1) return true;
  // the channel: 5 wide from the berth along the axis, out to the natural deep (dredgeLen)
  const int32_t rx = x - P.berthX, ry = y - P.berthY;
  const int32_t along = rx * P.dirX + ry * P.dirY, across = rx * px + ry * py;
  return along >= 0 && along <= harbourDredgeLen(P) && std::abs(across) <= 2;
}

int32_t EndlessSource::Impl::harbourDredgeLen(const PortPlan& P) {
  if (!P.dredged) return 0;
  auto it = dredgeMemo.find(P.site);
  if (it != dredgeMemo.end()) return it->second;
  int32_t L = 0;
  const int px = -P.dirY, py = P.dirX;
  for (int32_t k = 1; k <= 160; k++) {
    const int32_t cx = P.berthX + P.dirX * k, cy = P.berthY + P.dirY * k;
    bool all = true;
    for (int q = -2; q <= 2 && all; q++) all = waterE(cx + px * q, cy + py * q) < ELEV_SEA - Q(0.035);
    L = k;
    if (all || waterE(cx, cy) >= ELEV_SEA) break;
  }
  if (dredgeMemo.size() > 4096) dredgeMemo.clear();
  dredgeMemo[P.site] = L;
  return L;
}

// the harbour's ground, props and keepers on one chunk (after the settlement buffers): pure in the plan and the town's
// build, so every chunk agrees on multi-tile pieces
void EndlessSource::Impl::stampHarbour(const PortPlan& P, const SettlementOut* T, Stamp& S, std::vector<SpawnPlan>* spawns) {
  if (!P.harbour || !P.pierLen) return;
  const int dx = P.dirX, dy = P.dirY, px = -dy, py = dx;
  const int32_t len = P.pierLen;
  auto at = [&](int32_t k, int32_t o, int32_t& x, int32_t& y) { x = P.rootX + dx * k + px * o; y = P.rootY + dy * k + py * o; };
  auto wet = [&](Ground g) { return g == Ground::Water || g == Ground::DeepWater; };
  // the town's own pieces at a global tile (nothing of the harbour is laid over a building, a wall or a prop)
  auto townFree = [&](int32_t x, int32_t y) {
    if (!T) return true;
    const int32_t bx = x - T->gx, by = y - T->gy;
    if (!T->buf.in(bx, by)) return true;
    const size_t i = (size_t)by * T->buf.w + bx;
    if (!T->buf.bldgAt.empty() && T->buf.bldgAt[i] >= 0) return false;
    if (!T->buf.wall.empty() && T->buf.wall[i]) return false;
    if (!T->buf.prop.empty() && T->buf.prop[i]) return false;
    return true;
  };
  auto landHere = [&](int32_t x, int32_t y) { return waterE(x, y) >= ELEV_SEA; };
  if (spawns == nullptr) {
    // ---- ground: dredging first (water only, the sea's own: not a river), then the mole and its T-head
    const int32_t x0 = S.x0, y0 = S.y0;
    for (int32_t y = y0; y < y0 + CHUNK; y++)
      for (int32_t x = x0; x < x0 + CHUNK; x++) {
        if (S.at(x, y) != Ground::Water || S.c.biome[S.idx(x, y)] != (uint8_t)Biome::Ocean) continue;
        if (harbourDeep(P, x, y)) S.g(x, y, Ground::DeepWater);
      }
    for (int32_t k = 1; k <= len; k++)
      for (int32_t o = -3; o <= 3; o++) {
        if (std::abs(o) > 1 && k < len - 2) continue;   // the mole 3 wide, the T-head 7
        int32_t x, y;
        at(k, o, x, y);
        if (!S.in(x, y)) continue;
        if (wet(S.at(x, y))) { S.g(x, y, Ground::Bridge); S.clear(x, y); }
        S.reserve(x, y);
      }
    // the landing: the shore tiles at the mole's root, trodden and clear (T: the chunk lies within the town's margin)
    for (int32_t k = -2; k <= 0 && T; k++)
      for (int32_t o = -1; o <= 1; o++) {
        int32_t x, y;
        at(k, o, x, y);
        if (!S.in(x, y) || wet(S.at(x, y)) || !townFree(x, y)) continue;
        if (S.at(x, y) == Ground::Grass || S.at(x, y) == Ground::Meadow || S.at(x, y) == Ground::ForestFloor || S.at(x, y) == Ground::Rock)
          S.g(x, y, Ground::Dirt);
        S.clear(x, y);
        S.reserve(x, y);
      }
    // ---- the dress
    auto prop = [&](int32_t x, int32_t y, Prop p) { if (S.in(x, y)) { S.p(x, y, p); S.reserve(x, y); } };
    for (int32_t k = 2; k <= len - 3; k += 3) {   // mooring posts along the mole's edges, staggered
      int32_t x, y;
      at(k, (k / 3) % 2 ? 1 : -1, x, y);
      prop(x, y, Prop::MooringPost);
    }
    {
      int32_t x, y;
      at(len - 1, -2, x, y); prop(x, y, Prop::HarbourDesk);
      at(len, 3, x, y); prop(x, y, Prop::MooringPost);
      at(len, -3, x, y); prop(x, y, Prop::MooringPost);
      at(len - 2, 3, x, y); prop(x, y, Prop::CargoPile);
      at(len - 1, 3, x, y); prop(x, y, Prop::FishBaskets);
      at(len - 2, -3, x, y); prop(x, y, Prop::Anchor);
    }
    // a net rack at the landing (on land, clear of the town's pieces)
    for (int s : {2, -2, 3, -3}) {
      if (!T) break;
      int32_t x, y;
      at(-1, s, x, y);
      if (landHere(x, y) && townFree(x, y)) { if (S.in(x, y) && !wet(S.at(x, y))) prop(x, y, Prop::NetRack); break; }
    }
    // buoys: either side of the berth, and down the dredged channel every 10 tiles
    {
      int32_t bx = P.berthX + dx * 3, by = P.berthY + dy * 3;
      for (int s : {-5, 5}) {
        const int32_t x = bx + px * s, y = by + py * s;
        if (S.in(x, y) && wet(S.at(x, y))) prop(x, y, Prop::Buoy);
      }
      const int32_t dl = harbourDredgeLen(P);
      for (int32_t k = 10; k <= dl; k += 10)
        for (int s : {-3, 3}) {
          const int32_t x = P.berthX + dx * k + px * s, y = P.berthY + dy * k + py * s;
          if (S.in(x, y) && wet(S.at(x, y))) prop(x, y, Prop::Buoy);
        }
    }
  }
  // ---- the shipwright's slipway on the shore beside the landing (3x2, the prop on the bottom row's middle tile)
  int32_t slX = 0, slY = 0;
  bool slip = false;
  for (int32_t o = 6; o <= 12 && !slip && T; o++)
    for (int s : {1, -1}) {
      const int32_t ox = P.rootX + px * o * s, oy = P.rootY + py * o * s;
      // the shore along the axis from the landing's row
      int32_t t = -6;
      while (t <= 6 && landHere(ox + dx * t, oy + dy * t)) t++;
      if (t <= -6 || t > 6) continue;
      const int32_t bx = ox + dx * (t - 1), by = oy + dy * (t - 1);   // the last land tile before the water
      bool ok = true;
      for (int32_t yy = by - 1; yy <= by && ok; yy++)
        for (int32_t xx = bx - 1; xx <= bx + 1 && ok; xx++) ok = landHere(xx, yy) && townFree(xx, yy);
      if (!ok) continue;
      slX = bx; slY = by; slip = true;
      break;
    }
  if (spawns == nullptr && slip) {
    for (int32_t yy = slY - 1; yy <= slY; yy++)
      for (int32_t xx = slX - 1; xx <= slX + 1; xx++) {
        if (!S.in(xx, yy)) continue;
        S.p(xx, yy, xx == slX && yy == slY ? Prop::Slipway : Prop::Filler);
        if (S.at(xx, yy) != Ground::Sand && S.at(xx, yy) != Ground::Dirt) S.g(xx, yy, Ground::Sand);
        S.reserve(xx, yy);
      }
  }
  if (spawns == nullptr) return;
  // ---- the keepers (with the site's records, on every chunk the town touches)
  auto keeper = [&](int slot, int32_t x, int32_t y, Role role) {
    SpawnPlan s;
    s.siteId = P.site;
    s.sp.x = x; s.sp.y = y;
    s.sp.npc = true; s.sp.role = role;
    s.sp.slot = 0x7100 + slot;
    spawns->push_back(s);
  };
  int32_t x, y;
  at(len - 1, -1, x, y); keeper(0, x, y, Role::Harbourmaster);
  if (slip) keeper(1, slX + 2, slY, Role::Shipwright);
  at(len / 2, 0, x, y); keeper(2, x, y, Role::Sailor);
  at(len - 1, 2, x, y); keeper(3, x, y, Role::Sailor);
  // the sailors' tavern: a sailor at the door of the port's inn
  if (T)
    for (const Bldg& b : T->buf.bldgs)
      if (b.type == art::Building::Inn) {
        keeper(4, T->gx + b.r.x + b.r.w / 2, T->gy + b.r.y + b.r.h + 1, Role::Sailor);
        break;
      }
}

}  // namespace ew
