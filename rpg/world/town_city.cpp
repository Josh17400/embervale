// M7c the planned city (VISION_PLAN 15.M7c): inside the walls a full street network (avenues from each gate to the
// central plaza and the seat; streets off them laid as real blocks of row houses fronting every street round shared
// back yards and gardens; whole blocks given a role; grid sections tilted where an avenue turns, streets bending along
// a river, lakeshore or hill), every city its own (town_plan.h PlanTraits). CITY lane. The street network is
// town_city_grid.cpp; the M7b plan it grows from is town_urban.cpp. Here: the phase hook (cityAfter), the row houses'
// street faces, the block roles, the ranks and the traits.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/build/blueprint.h"
#include "rpg/world/town_gen.h"
#include "rpg/world/urban.h"

namespace ew {
namespace town {

using art::Building;

namespace {
int D4X(int d) { static const int v[4] = {0, 1, -1, 0}; return v[d]; }
int D4Y(int d) { static const int v[4] = {1, 0, 0, -1}; return v[d]; }
// (debug, EMB_CITY_PPM=<dir>) the planned city's buffer as a picture (one pixel per tile, scaled 3), written just before
// finish: the plan's classes, the mask, the buildings, the walls; the plan reviews of rpg_test's synthetic cities
void dumpPpm(const Gen& g, const char* dir) {
  const int S = 3, W = g.W, H = g.H;
  std::vector<uint8_t> px((size_t)W * H * S * S * 3, 0);
  auto put = [&](int x, int y, int r, int gg, int b) {
    for (int sy = 0; sy < S; sy++)
      for (int sx = 0; sx < S; sx++) {
        const size_t o = ((size_t)(y * S + sy) * W * S + (size_t)(x * S + sx)) * 3;
        px[o] = (uint8_t)r; px[o + 1] = (uint8_t)gg; px[o + 2] = (uint8_t)b;
      }
  };
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const size_t i = g.I(x, y);
      int r = 78, gg = 126, b = 60;
      const Ground gr = g.M.at(x, y);
      if (groundWater(gr)) { r = 52; gg = 92; b = 168; }
      else if (gr == Ground::Bridge) { r = 150; gg = 110; b = 70; }
      else if (gr == Ground::Farmland) { r = 120; gg = 92; b = 50; }
      else if (gr == Ground::Dirt) { r = 132; gg = 104; b = 70; }
      else if (gr == Ground::Road) { r = 150; gg = 146; b = 136; }
      else if (gr == Ground::Plaza) { r = 196; gg = 188; b = 168; }
      switch (g.mask[i]) {
        case K_MAIN: r = 176; gg = 172; b = 164; break;
        case K_LANE: r = 160; gg = 150; b = 130; break;
        case K_SQUARE: r = 226; gg = 214; b = 186; break;
        case K_PARK: r = 70; gg = 160; b = 70; break;
        case K_ROWYARD: r = 110; gg = 150; b = 80; break;
        case K_FIELD: r = 140; gg = 170; b = 70; break;
        default: break;
      }
      if (!g.plan.empty()) {
        const Urb u = (Urb)g.plan[i];
        if (u == Urb::Sidewalk) { r = 214; gg = 204; b = 178; }
        else if (u == Urb::Gutter) { r = 110; gg = 108; b = 104; }
        else if (u == Urb::Street) { r = 150; gg = 150; b = 150; }
      }
      if (g.M.prop[i] && art::isTreeProp((art::Prop)(g.M.prop[i] - 1))) { r = 30; gg = 80; b = 30; }
      if (g.M.wall[i]) { r = 50; gg = 46; b = 50; }
      if (g.M.bldgAt[i] >= 0) {
        const Bldg& bd = g.M.bldgs[(size_t)g.M.bldgAt[i]];
        if (bd.row) { r = 150; gg = 56; b = 40; } else { r = 196; gg = 96; b = 60; }
        if (!townIsHome(bd.type)) { r = 120; gg = 70; b = 150; }
        if (y == bd.r.y + bd.r.h - 1) { r = std::min(255, r + 60); gg = std::min(255, gg + 60); b = std::min(255, b + 40); }
      }
      put(x, y, r, gg, b);
    }
  int homes = 0;
  for (const Bldg& bd : g.M.bldgs) if (townIsHome(bd.type)) homes++;
  char name[512];
  std::snprintf(name, sizeof name, "%s/city_%08x_k%d_h%d.ppm", dir, (unsigned)g.P.seed, (int)g.cp.kind, homes);
  if (FILE* f = std::fopen(name, "wb")) {
    std::fprintf(f, "P6\n%d %d\n255\n", W * S, H * S);
    std::fwrite(px.data(), 1, px.size(), f);
    std::fclose(f);
  }
}
}  // namespace

// after every phase of Gen::step for a planned city (the phase's number as step() counts it)
void Gen::cityAfter(int phase) {
  if (phase == 15) {
    // a fence the city's later works cut short (a footpath through a compound's street side, a house's apron) leaves
    // no lone post standing on its own
    {
      auto fenceAt = [&](int x, int y) { const int q = M.propAt(x, y); return q == (int)art::Prop::FenceH + 1 || q == (int)art::Prop::FenceV + 1; };
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
          if (!fenceAt(x, y)) continue;
          bool nb = false;
          for (int oy = -1; oy <= 1 && !nb; oy++)
            for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && fenceAt(x + ox, y + oy)) { nb = true; break; }
          if (!nb) M.prop[I(x, y)] = 0;
        }
    }
    // (M7c integration) the square's notice board keeps its breathing room: a later step (the seat's forecourt statues,
    // benches, lamps) may have stood something right beside it; then it comes down, and rpg/story/places.cpp stands
    // the board at runtime where there is room round it
    for (int y = 1; y + 1 < H; y++)
      for (int x = 1; x + 1 < W; x++) {
        if (M.propAt(x, y) != (int)art::Prop::NoticeBoard + 1) continue;
        bool crowded = false;
        for (int oy = -1; oy <= 1 && !crowded; oy++)
          for (int ox = -1; ox <= 1; ox++) {
            if (!ox && !oy) continue;
            const size_t j = I(x + ox, y + oy);
            if (M.prop[j] || M.bldgAt[j] >= 0 || (M.wall.size() == M.prop.size() && M.wall[j]) || groundSolid(M.at(x + ox, y + oy))) { crowded = true; break; }
          }
        if (crowded) M.setP(x, y, 0);
      }
    cityFaces();
    cityRoles();
    cityTraits();
    if (const char* d = std::getenv("EMB_CITY_PPM")) dumpPpm(*this, d);
  }
}

// ------------------------------------------------------------------------------------------------ the street faces
// A city house whose side looks onto a street (a corner house at a terrace's end, a house along a north-south street:
// the street on that side within two tiles over half its depth, no neighbour sharing that wall) gets ROW_SIDE_W /
// ROW_SIDE_E; one whose back looks onto a street (a lane right behind it over half its width) ROW_BACK. The builder
// dresses those faces (ART lane); the door stays in the front wall. Inside the walls only; never a village.
void Gen::cityFaces() {
  if (!planned || village) return;
  for (Bldg& b : M.bldgs) {
    if (!townIsHome(b.type) && b.type != Building::Shop && b.type != Building::Inn) continue;
    if (walled && !ins(b.r.cx(), b.r.y + b.r.h - 1)) continue;
    auto streetCol = [&](int x) {
      int n = 0;
      for (int y = b.r.y; y < b.r.y + b.r.h; y++) if (isStreet(x, y)) n++;
      return n;
    };
    const int need = (b.r.h + 1) / 2;
    uint8_t row = b.row;
    if (!(row & bld::ROW_W) && (streetCol(b.r.x - 1) >= need || (streetCol(b.r.x - 2) >= need && !bldgAt(b.r.x - 1, b.r.y + b.r.h - 1)))) row |= bld::ROW_SIDE_W;
    if (!(row & bld::ROW_E) && (streetCol(b.r.x + b.r.w) >= need || (streetCol(b.r.x + b.r.w + 1) >= need && !bldgAt(b.r.x + b.r.w, b.r.y + b.r.h - 1)))) row |= bld::ROW_SIDE_E;
    int back = 0;
    for (int x = b.r.x; x < b.r.x + b.r.w; x++) if (isStreet(x, b.r.y - 1) || isStreet(x, b.r.y - 2)) back++;
    if (back * 2 >= b.r.w) row |= bld::ROW_BACK;
    b.row = row;
  }
}

// ------------------------------------------------------------------------------------------------ the block roles
// Every block of the city (the land the streets, walls and water cut off, inside the walls) takes a role from what
// stands in it: the seat's precinct or the palace grounds, the temple close, the market, the park, the barracks, the
// harbour (a block on the water with its quays), walled family compounds, an ordinary block of homes; the orchards,
// kitchen gardens and craft yards cityRoleBlocks kept keep theirs. And the ranks the plan knows: avenues Main, block
// streets and their walks Street, lanes and alleys Lane, squares and closes Square, the park's and the yards' paths
// Footpath (rankLayer fills the rest).
void Gen::cityRoles() {
  const size_t n = (size_t)W * H;
  if (role.size() != n || rank.size() != n) return;
  // --- the ranks
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const size_t i = I(x, y);
      if (rank[i]) continue;
      const uint8_t k = mask[i];
      if (k == K_SQUARE) { rank[i] = (uint8_t)Rank::Square; continue; }
      if (k == K_YARD && (M.at(x, y) == Ground::Road || M.at(x, y) == Ground::Dirt || M.at(x, y) == Ground::Plaza || M.at(x, y) == Ground::Bridge)) {
        rank[i] = (uint8_t)Rank::Footpath;
        continue;
      }
      if (k != K_MAIN && k != K_LANE) {
        if (k == K_PARK && !plan.empty() && (Urb)plan[i] == Urb::ParkPath) rank[i] = (uint8_t)Rank::Footpath;
        continue;
      }
      const Urb u = plan.empty() ? Urb::None : (Urb)plan[i];
      Rank r = k == K_MAIN ? Rank::Main : Rank::Lane;
      switch (u) {
        case Urb::Avenue: case Urb::Gutter: r = Rank::Main; break;
        case Urb::Street: r = Rank::Street; break;
        case Urb::Lane: case Urb::Alley: r = Rank::Lane; break;
        case Urb::Square: case Urb::Close: r = Rank::Square; break;
        case Urb::Sidewalk: {
          // a walk is its carriageway's: the nearest carriage tile's class within two
          r = Rank::Main;
          int bestD = 99;
          for (int oy = -2; oy <= 2; oy++)
            for (int ox = -2; ox <= 2; ox++) {
              if (!in(x + ox, y + oy)) continue;
              const Urb v = (Urb)plan[I(x + ox, y + oy)];
              const int d = std::max(std::abs(ox), std::abs(oy));
              if (d >= bestD) continue;
              if (v == Urb::Avenue || v == Urb::Gutter) { bestD = d; r = Rank::Main; }
              else if (v == Urb::Street) { bestD = d; r = Rank::Street; }
              else if (v == Urb::Lane || v == Urb::Alley) { bestD = d; r = Rank::Lane; }
            }
          break;
        }
        default: break;
      }
      rank[i] = (uint8_t)r;
    }
  // --- the blocks
  std::vector<int> lab(n, -1);
  std::vector<int> st, tiles;
  auto land = [&](int x, int y) {
    if (!in(x, y)) return false;
    const size_t i = I(x, y);
    return !isStreet(x, y) && !M.wall[i] && !water[i] && (!walled || inside[i]);
  };
  int id = 0;
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      if (!land(x, y) || lab[I(x, y)] >= 0) continue;
      tiles.clear();
      st.clear();
      st.push_back((int)I(x, y));
      lab[I(x, y)] = id;
      bool seat = false, temple = false, market = false, parkB = false, barracks = false, harbour = false, comp = false, homesB = false;
      int kept = 0;
      while (!st.empty()) {
        const int c = st.back();
        st.pop_back();
        tiles.push_back(c);
        const int qx = c % W, qy = c / W;
        if (inCompound(qx, qy)) seat = true;
        for (const Precinct& pc : precincts)
          if (pc.plot.contains(qx, qy)) { if (pc.role == 3) temple = true; else seat = true; }
        if (mktZone.w > 0 && mktZone.contains(qx, qy)) market = true;
        if (mask[(size_t)c] == K_PARK) parkB = true;
        if (mask[(size_t)c] == K_COMPOUND && compound.w == 0) comp = true;
        if (mask[(size_t)c] == K_YARD && M.at(qx, qy) == Ground::Plaza)
          for (int d = 0; d < 4; d++) if (in(qx + D4X(d), qy + D4Y(d)) && water[I(qx + D4X(d), qy + D4Y(d))]) harbour = true;
        const int bi = M.bldgAt[(size_t)c];
        if (bi >= 0) {
          if (bi == barracksIdx) barracks = true;
          else if (townIsHome(M.bldgs[(size_t)bi].type)) homesB = true;
          if (bi == seatIdx || bi == palaceIdx || bi == keepIdx) seat = true;
          if (M.bldgs[(size_t)bi].type == Building::Temple && (M.bldgs[(size_t)bi].civic & bld::CIVIC_SACRED)) temple = true;
        }
        if (role[(size_t)c]) kept++;
        for (int d = 0; d < 4; d++) {
          const int nx = qx + D4X(d), ny = qy + D4Y(d);
          if (!land(nx, ny) || lab[I(nx, ny)] >= 0) continue;
          lab[I(nx, ny)] = id;
          st.push_back((int)I(nx, ny));
        }
      }
      id++;
      // a block that reaches the buffer's edge is the open country (no role); one cityRoleBlocks gave a role keeps it
      bool edge = false;
      for (int c : tiles) {
        const int qx = c % W, qy = c / W;
        if (qx <= 1 || qy <= 1 || qx >= W - 2 || qy >= H - 2) { edge = true; break; }
      }
      if (edge || kept * 2 >= (int)tiles.size()) continue;
      BlockRole r = BlockRole::None;
      if (seat) r = compound.w > 0 ? BlockRole::Seat : BlockRole::Seat;
      else if (temple) r = BlockRole::Temple;
      else if (market) r = BlockRole::Market;
      else if (parkB) r = BlockRole::Park;
      else if (barracks) r = BlockRole::Barracks;
      else if (harbour) r = BlockRole::Harbour;
      else if (comp) r = BlockRole::Compound;
      else if (homesB) r = BlockRole::Homes;
      if (r == BlockRole::None) continue;
      for (int c : tiles) if (!role[(size_t)c]) role[(size_t)c] = (uint8_t)r;
    }
  // the squares of the precincts and the market's paving are their blocks' too
  for (const Precinct& pc : precincts) setRoleRect(pc.sq, pc.role == 3 ? BlockRole::Temple : BlockRole::Seat);
  if (mktZone.w > 0)
    for (int y = mktZone.y; y < mktZone.y + mktZone.h; y++)
      for (int x = mktZone.x; x < mktZone.x + mktZone.w; x++) if (get(x, y) == K_SQUARE) setRole(x, y, BlockRole::Market);
  if (park.w > 0) setRoleRect(park, BlockRole::Park);
}

// ------------------------------------------------------------------------------------------------ the traits
void Gen::cityTraits() {
  PlanTraits& T = O.traits;
  T.planKind = cp.kind;
  T.gates = (uint8_t)std::min<size_t>(255, gatePlan.size());
  int av = 0;
  for (size_t k = 0; k < gatePlan.size() && av < 6; k++) {
    const GatePlan& g = gatePlan[k];
    const float a = datan2((float)(g.y - cy), (float)(g.x - cx));
    T.avenueDeg[av++] = (int16_t)std::floor(a * (180.0f / D_PI) + 0.5f);
  }
  T.avenues = (uint8_t)av;
  T.blockW = (uint8_t)std::clamp(cp.SX, 0, 255);
  T.blockH = (uint8_t)std::clamp(cp.P, 0, 255);
  int secs = 0;
  for (const CitySec& s : csecs)
    if (s.area >= 200) {
      if (secs < 4) T.gridDeg[secs] = s.deg;
      secs++;
    }
  T.gridSections = (uint8_t)std::min(255, secs);
  // the water inside the walls, and its kind
  int wet = 0, edgeWet = 0;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!water[I(x, y)]) continue;
      if (!walled || inside[I(x, y)]) wet++;
      if (x == 0 || y == 0 || x == W - 1 || y == H - 1) edgeWet++;
    }
  if (stilt) T.water = WaterKind::Marsh;
  else if (!wet) T.water = WaterKind::None;
  else if (P.archetype == Archetype::Port || P.archetype == Archetype::Fishing) T.water = WaterKind::Coast;
  else if (edgeWet > 0) T.water = WaterKind::River;
  else T.water = WaterKind::Lake;
  // the bridges: pieces of bridge deck inside the walls
  {
    std::vector<uint8_t> seen((size_t)W * H, 0);
    int nb = 0;
    for (int y = 1; y < H - 1; y++)
      for (int x = 1; x < W - 1; x++) {
        if (seen[I(x, y)] || M.at(x, y) != Ground::Bridge || (walled && !inside[I(x, y)])) continue;
        std::vector<int> q{(int)I(x, y)};
        seen[I(x, y)] = 1;
        for (size_t h = 0; h < q.size(); h++) {
          const int qx = q[h] % W, qy = q[h] / W;
          for (int d = 0; d < 4; d++) {
            const int nx = qx + D4X(d), ny = qy + D4Y(d);
            if (in(nx, ny) && !seen[I(nx, ny)] && M.at(nx, ny) == Ground::Bridge) { seen[I(nx, ny)] = 1; q.push_back((int)I(nx, ny)); }
          }
        }
        if (q.size() >= 3) nb++;
      }
    T.bridges = (uint8_t)std::min(255, nb);
  }
  // the squares on the way from the market to the seat, the main square's paving
  int pl = 0;
  for (const Square& s : squares) if (s.role == 0 || s.role == 1 || s.role == 4) pl++;
  T.plazas = (uint8_t)std::min(255, pl);
  {
    int tilesSq = 0;
    std::vector<uint8_t> seen((size_t)W * H, 0);
    std::vector<int> q;
    if (get(cx, cy) == K_SQUARE) { q.push_back((int)I(cx, cy)); seen[I(cx, cy)] = 1; }
    for (size_t h = 0; h < q.size(); h++) {
      tilesSq++;
      const int qx = q[h] % W, qy = q[h] / W;
      for (int d = 0; d < 4; d++) {
        const int nx = qx + D4X(d), ny = qy + D4Y(d);
        if (in(nx, ny) && !seen[I(nx, ny)] && get(nx, ny) == K_SQUARE) { seen[I(nx, ny)] = 1; q.push_back((int)I(nx, ny)); }
      }
    }
    T.plazaTiles = (int16_t)std::min(32000, tilesSq);
  }
  // the seat's place: the palace grounds' gate or the seat precinct's square, in tenths of the radii from the heart
  for (const Precinct& pc : precincts)
    if (pc.role == 1) {
      const int sx = pc.sq.x + pc.sq.w / 2, sy = pc.sq.y;
      T.seatDX = (int8_t)std::clamp((int)std::floor((sx - cx) * 10.0f / std::max(1.0f, rx) + 0.5f), -100, 100);
      T.seatDY = (int8_t)std::clamp((int)std::floor((sy - cy) * 10.0f / std::max(1.0f, ry) + 0.5f), -100, 100);
      break;
    }
  // the blocks per role (a block: a 4-connected piece of one role)
  {
    for (int k = 0; k < (int)BlockRole::COUNT; k++) if (k != (int)BlockRole::Faubourg && k != (int)BlockRole::Farm) T.blocks[k] = 0;
    std::vector<uint8_t> seen((size_t)W * H, 0);
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        const uint8_t r = role[I(x, y)];
        if (!r || seen[I(x, y)] || r == (uint8_t)BlockRole::Faubourg || r == (uint8_t)BlockRole::Farm) continue;
        std::vector<int> q{(int)I(x, y)};
        seen[I(x, y)] = 1;
        for (size_t h = 0; h < q.size(); h++) {
          const int qx = q[h] % W, qy = q[h] / W;
          for (int d = 0; d < 4; d++) {
            const int nx = qx + D4X(d), ny = qy + D4Y(d);
            if (in(nx, ny) && !seen[I(nx, ny)] && role[I(nx, ny)] == r) { seen[I(nx, ny)] = 1; q.push_back((int)I(nx, ny)); }
          }
        }
        if (q.size() >= 8 && T.blocks[r] < 255) T.blocks[r]++;
      }
  }
}

}  // namespace town
}  // namespace ew
