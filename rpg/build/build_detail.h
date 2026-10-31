// The builder's private seam (SOLID 1.6): the working state Bx, the culture and kind tags, the small shared
// helpers (inline) and the prototypes that link builder.cpp, build_geom.cpp, build_forms.cpp, build_culture.cpp and
// build_finish.cpp. Include only from rpg/build/build*.cpp; the public face is blueprint.h.
#pragma once
#include "rpg/build/blueprint.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace bld {

inline uint32_t mix32(uint32_t x) {
  x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16;
  return x;
}
constexpr uint32_t RGB(int r, int g, int b) { return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16 | 0xFF000000u; }
inline uint32_t lerpC(uint32_t a, uint32_t b, int t100) {
  if (!a) return b;
  auto ch = [&](int s) { const int x = (int)((a >> s) & 255u), y = (int)((b >> s) & 255u); return (uint32_t)std::clamp(x + (y - x) * t100 / 100, 0, 255) << s; };
  return ch(0) | ch(8) | ch(16) | 0xFF000000u;
}

using art::Building;
using art::DoorShape;
using art::RoofMat;
using art::RoofShape;
using art::WallMat;
using art::WindowShape;

enum Cul : int { CU_NONE, CU_FJORD, CU_HIGHLAND, CU_HEART, CU_IMPERIAL, CU_DUNE, CU_STEPPE, CU_MARSH, CU_JADE, CU_RIVER, CU_SUN, CU_SYLVAN, CU_STAR };
// cult::Seat + 1 (rpg/culture/society.h; the builder does not link the society)
enum SeatK : int { SK_NONE, SK_CASTLE, SK_COURT, SK_HALL, SK_TENT, SK_TEMPLE, SK_GUILD, SK_SPIRE, SK_TREE, SK_STILT };

// the seat of power a culture builds by its own priors (cult::societyOf's), for a palace asked without one
inline int cultureSeat(int cul) {
  switch (cul) {
    case CU_FJORD: return SK_HALL;
    case CU_IMPERIAL: case CU_DUNE: case CU_JADE: return SK_COURT;
    case CU_STEPPE: return SK_TENT;
    case CU_MARSH: return SK_STILT;
    case CU_RIVER: return SK_GUILD;
    case CU_SUN: return SK_TEMPLE;
    case CU_SYLVAN: return SK_TREE;
    case CU_STAR: return SK_SPIRE;
    default: return SK_CASTLE;
  }
}

inline bool flatRoof(RoofShape r) { return r == RoofShape::FlatParapet || r == RoofShape::Dome || r == RoofShape::Onion; }
inline bool pitchedRoof(RoofShape r) { return !flatRoof(r) && r != RoofShape::Stepped; }
inline bool weakWall(WallMat w) { return w == WallMat::Timber || w == WallMat::Plaster || w == WallMat::Log || w == WallMat::Plank || w == WallMat::Wattle || w == WallMat::Felt; }

// ---------------------------------------------------------------- the builder's working state
enum class PK : uint8_t { Home, Hut, Farm, Shop, Inn, Smithy, Temple, Keep, Palace, Tower, Windmill, Watermill, Barracks, Guildhall,
                          Exchange, MeadHall, Bath, TeaHouse, Lodge, Council };
inline PK kindOf(Building b) {
  switch (b) {
    case Building::Hut: return PK::Hut;
    case Building::Farmhouse: case Building::Granary: case Building::Sawmill: return PK::Farm;
    case Building::Shop: case Building::Bakery: case Building::Butcher: case Building::Fishmonger: case Building::Weaver: return PK::Shop;
    case Building::Inn: return PK::Inn;
    case Building::Smithy: case Building::Smelter: return PK::Smithy;
    case Building::Temple: return PK::Temple;
    case Building::Keep: return PK::Keep;
    case Building::Palace: return PK::Palace;
    case Building::Tower: return PK::Tower;
    case Building::Windmill: return PK::Windmill;
    case Building::Watermill: return PK::Watermill;
    case Building::Barracks: return PK::Barracks;
    case Building::Guildhall: return PK::Guildhall;
    case Building::Exchange: return PK::Exchange;
    case Building::MeadHall: return PK::MeadHall;
    case Building::Bathhouse: return PK::Bath;
    case Building::TeaHouse: return PK::TeaHouse;
    case Building::Lodge: return PK::Lodge;
    case Building::CouncilHall: return PK::Council;
    default: return PK::Home;   // houses, stone houses, the tannery
  }
}

struct Bx {
  const Request& r;
  Blueprint& b;
  std::vector<Volume>& v;
  art::ArchStyle& st;
  int W = 0, H = 0, dc = 0, cul = 0, wl = 1, ur = 0, want = 1, budget = 48, seat = 0;
  PK k = PK::Home;
  Building P = Building::House;
  uint32_t s = 0;
  bool grounded = false;    // the assembler laid its own platforms / stilts
  bool lord = false;        // the seat is a lord's (smaller) rather than the ruler's
  Bx(const Request& rq, Blueprint& bp) : r(rq), b(bp), v(bp.vols), st(bp.style) {
    const int wt = std::max(1, rq.wTiles), ht = std::max(1, rq.hTiles);
    W = wt * 16; H = ht * 16; dc = (wt / 2) * 16 + 8;
    cul = st.culture <= CU_STAR ? st.culture : CU_NONE;
    wl = std::min(3, (int)rq.wealth);
    ur = std::min(3, (int)rq.urban);
    P = rq.purpose;
    k = kindOf(P);
    s = mix32(rq.seed * 2654435761u ^ (uint32_t)P * 0x9E3779B1u ^ (uint32_t)cul * 0x85EBCA77u ^ 0xB11D3Bu);
  }
  uint32_t nx() { s = mix32(s + 0x9E3779B9u); return s; }
  int pick(int n) { return n <= 1 ? 0 : (int)(nx() % (uint32_t)n); }
  bool ch(int p256) { return (int)(nx() & 255u) < p256; }
  int rg(int lo, int hi) { return hi <= lo ? lo : lo + pick(hi - lo + 1); }
  int add(const Volume& x) { v.push_back(x); return (int)v.size() - 1; }
  // a volume in the building's style
  Volume vol(VolRole role, int x0, int y0, int x1, int y1, int wallH, int storeys = 1) const {
    Volume x;
    x.role = role;
    x.x0 = (int16_t)x0; x.y0 = (int16_t)y0; x.x1 = (int16_t)x1; x.y1 = (int16_t)y1;
    x.wallH = (int16_t)std::max(1, wallH);
    x.storeys = (uint8_t)std::clamp(storeys, 1, 4);
    x.roof = st.roof; x.roofMat = st.roofMat; x.wall = st.wall; x.pitch = st.pitch; x.eave = (int8_t)st.eave;
    x.window = st.window; x.door = st.door; x.ornament = st.ornament; x.finery = (uint8_t)wl;
    return x;
  }
  // a round volume standing on its front point (cx, y1)
  Volume rnd(VolRole role, int cx, int y1, int rad, int wallH, int storeys = 1) const {
    Volume x = vol(role, cx - rad, y1 - 2 * rad, cx + rad, y1, wallH, storeys);
    x.shape = VolShape::Round;
    return x;
  }
  // the wall height of n storeys (base: one storey's), in the culture's proportions
  int wallFor(int n, int base) const {
    int h = base + 14 * (n - 1);
    if (st.wallH) h = h * (800 + (int)st.wallH * 500 / 255) / 1000;
    const int mn = n <= 1 ? 16 : 22 + 14 * (n - 1);
    return std::max(h, mn);
  }
  // the culture's monumental walls (temples, seats, towers, barracks)
  WallMat monoWall(bool temple = false) const {
    switch (cul) {
      case CU_FJORD: return temple ? WallMat::Plank : WallMat::Stone;
      case CU_HIGHLAND: return WallMat::Rubble;
      case CU_IMPERIAL: case CU_SUN: case CU_STAR: return WallMat::Ashlar;
      case CU_DUNE: return WallMat::Adobe;
      case CU_STEPPE: return temple ? WallMat::Felt : WallMat::Adobe;
      case CU_MARSH: return WallMat::Plank;
      case CU_JADE: return WallMat::Plaster;
      case CU_RIVER: return WallMat::Brick;
      case CU_SYLVAN: return WallMat::Living;
      default: return WallMat::Stone;
    }
  }
  // the culture's fired roof for a monument where the style roofs in straw or sod
  RoofMat monoRoof() const {
    switch (cul) {
      case CU_FJORD: return st.roofMat == RoofMat::Turf ? RoofMat::Shingle : st.roofMat;
      case CU_MARSH: return RoofMat::Thatch;
      case CU_JADE: return st.roofMat == RoofMat::GlazedTile ? st.roofMat : RoofMat::ClayTile;
      case CU_IMPERIAL: return RoofMat::ClayTile;
      case CU_SYLVAN: return st.roofMat == RoofMat::Bark ? RoofMat::Bark : RoofMat::Leaf;
      case CU_STEPPE: return RoofMat::Felt;
      case CU_DUNE: case CU_SUN: return RoofMat::Adobe;
      case CU_STAR: return st.roofMat == RoofMat::Copper ? RoofMat::Copper : RoofMat::Slate;
      default: return (st.roofMat == RoofMat::Thatch || st.roofMat == RoofMat::Turf || st.roofMat == RoofMat::Palm) ? RoofMat::Slate : st.roofMat;
    }
  }
  void monument(Volume& x, bool temple = false) const {
    if (weakWall(x.wall) && !(temple && cul == CU_STEPPE)) x.wall = monoWall(temple);
    if (x.roofMat == RoofMat::Thatch || x.roofMat == RoofMat::Turf || x.roofMat == RoofMat::Palm) x.roofMat = monoRoof();
    if (x.roof == RoofShape::Turf) x.roof = RoofShape::Steep;
  }
  // a glaze / metal for domes and bulbs in the culture's colours
  uint32_t domeTint() const {
    switch (cul) {
      case CU_DUNE: return st.roofMat == RoofMat::GlazedTile && st.roofTint ? st.roofTint : RGB(62, 150, 168);
      case CU_STAR: { static const uint32_t g[3] = {RGB(70, 92, 160), RGB(104, 80, 150), RGB(70, 140, 128)}; return g[(b.req.seed >> 7) % 3]; }
      case CU_RIVER: return RGB(84, 94, 122);
      default: return 0;
    }
  }
  uint32_t limeTint() const { return st.wallTint ? lerpC(st.wallTint, RGB(232, 204, 158), 55) : RGB(232, 204, 158); }
  bool storeyRoom(int n) const { return n <= want; }
};

struct HouseSpec {
  int base = 26;            // wall height of one storey
  Face face = Face::Windows;
  int porchP = 70;          // chance /256 of a porch (rich: more)
  int porchKind = -1;       // -1 by culture
  bool wings = true, cross = true, dormers = true, jetty = true;
  int ridge = -1;           // -1 by seed, 0 across the view, 1 north-south (the gable to the street)
  bool inset = true;        // a poor house stands smaller on its plot
};

// build_geom.cpp: roof heights and the pieces
int roofHeight(const Volume& x);
int riseOf(const Volume& x);
int roofZAt(const Volume& x, int y);
int porch(Bx& c, int bi, int kind, int halfW, int depth);
void endStacks(Bx& c, int bi, int n);
void stilts(Bx& c, int h);
// build_culture.cpp: homes, culture homes, seats of power
int house(Bx& c, const HouseSpec& sp);
int roundHouse(Bx& c, int n, int base, bool felt, int annexes);
int courtHouse(Bx& c, int base, bool flatRoofs, int backStoreys);
int longHall(Bx& c, int base, int n, int inset);
void cultureHome(Bx& c);
void enclosure(Bx& c, int x0, int y0, int x1, int y1, int h, int th, WallMat mat, RoofShape coping, RoofMat cm, uint32_t feat, int gateHalf);
void seat(Bx& c);
// build_forms.cpp: per-purpose forms
void hut(Bx& c);
void farm(Bx& c);
void shop(Bx& c);
void smithy(Bx& c);
void inn(Bx& c);
void teaHouse(Bx& c);
void greatHall(Bx& c, bool council);
void guildhall(Bx& c);
void exchange(Bx& c);
void bath(Bx& c);
void temple(Bx& c);
void mageTower(Bx& c);
void windmill(Bx& c);
void watermill(Bx& c);
void barracks(Bx& c);
void genericCompound(Bx& c);
void tentForm(Bx& c);
// build_finish.cpp: ground, terrace and finish
void ground(Bx& c);   // (finishing, below)
void finish(Bx& c);

// pieces with default arguments, shared by several files (inline here so the defaults keep one home)
// set a volume's roof
inline void roofTo(Volume& x, RoofShape s, RoofMat m, int pitch, uint32_t tint = 0) { x.roof = s; x.roofMat = m; x.pitch = (uint8_t)pitch; x.roofTint = tint; }
inline void flat(Volume& x, bool crenels = false) {
  x.roof = RoofShape::FlatParapet;
  if (x.roofMat != RoofMat::Adobe && x.roofMat != RoofMat::ClayTile) x.roofMat = RoofMat::Adobe;
  x.pitch = 0;
  if (crenels) x.feat |= VF_CRENELS;
}
// chimneys over a pitched or flat body (chimney => hearth: only with facts.hearth)
inline void chimneys(Bx& c, int bi, int n, int cw = 6) {
  if (!c.r.facts.hearth || n <= 0 || bi < 0) return;
  const Volume bo = c.v[(size_t)bi];
  if (bo.shape != VolShape::Box) return;
  const bool flatTop = flatRoof(bo.roof) || bo.roof == RoofShape::Stepped;
  if (flatTop) n = std::min(n, 1);
  for (int i = 0; i < n; i++) {
    int x, y;
    const int span = bo.x1 - bo.x0;
    if (span < cw + 8) return;
    if (flatTop) { x = c.ch(128) ? bo.x0 + 4 : bo.x1 - cw - 4; y = bo.y0 + 3; }
    else if (bo.ridgeNS) {
      const int mid = (bo.x0 + bo.x1) / 2;
      x = n == 2 ? (i == 0 ? mid - 2 - cw : mid + 2) : (c.ch(128) ? mid + 2 : mid - 2 - cw);
      y = bo.y0 + (bo.y1 - bo.y0) * 3 / 10 + c.pick(6);
    } else {
      const int t = n == 2 ? (i == 0 ? 14 : 80) : 18 + c.pick(64);
      x = bo.x0 + span * t / 100;
      if (std::abs(x + cw / 2 - c.dc) < 5) x += 10;
      y = (bo.y0 + bo.y1) / 2 + 1 + c.pick(3);
    }
    x = std::clamp(x, bo.x0 + 2, bo.x1 - cw - 2);
    // clear of the cupolas, drums, tiers and towers standing on (or before) the roof
    auto clash = [&](int xx) {
      for (const Volume& o : c.v)
        if ((o.role == VolRole::Drum || o.role == VolRole::Tier || o.role == VolRole::Tower || o.role == VolRole::Chimney) && xx < o.x1 + 2 && xx + cw > o.x0 - 2 && y < o.y1 + 2 && y + 4 > o.y0 - 2) return true;
      return false;
    };
    for (int k = 0; k < 4 && clash(x); k++) x = std::clamp(k % 2 ? bo.x0 + 4 + k * 3 : bo.x1 - cw - 4 - k * 3, bo.x0 + 2, bo.x1 - cw - 2);
    if (clash(x)) continue;
    Volume ch = c.vol(VolRole::Chimney, x, y, x + cw, y + 4, 6);
    ch.wall = (bo.wall == WallMat::Adobe) ? WallMat::Adobe : (bo.wall == WallMat::Stone || bo.wall == WallMat::Rubble || bo.wall == WallMat::Ashlar ? WallMat::Stone : WallMat::Brick);
    ch.roof = RoofShape::FlatParapet;
    ch.z0 = bo.z0;
    c.add(ch);
  }
}
// a tower (square or round) at x, its front at y1
inline int towerVol(Bx& c, VolRole role, bool round, int x0, int y1, int w, int wallH, RoofShape roof, RoofMat mat, int pitch, uint32_t tint = 0) {
  Volume t = round ? c.rnd(role, x0 + w / 2, y1, w / 2, wallH) : c.vol(role, x0, y1 - w, x0 + w, y1, wallH);
  roofTo(t, roof, mat, pitch, tint);
  t.ridgeNS = false;
  t.eave = 0;
  if (roof == RoofShape::FlatParapet) t.roofMat = RoofMat::Adobe;
  return c.add(t);
}
// lift every volume standing on the ground onto a platform (plinth, terrace, dais) of height fh
inline void platformUnder(Bx& c, int fh, WallMat mat, bool steps, int frontPad = 3) {
  int x0 = 1 << 20, x1 = -(1 << 20), y0 = 1 << 20, y1 = -(1 << 20);
  // (M7 fix) a porch stands on its own step in front of the plinth: the plinth follows the house's walls (one box to
  // the porch's front left flat slabs of plinth before the facade either side of the porch, in the eaves' shade)
  std::vector<size_t> porches;
  bool walls = false;
  for (const Volume& x : c.v)
    if (x.z0 == 0 && x.role != VolRole::Porch && x.role != VolRole::Chimney && x.role != VolRole::Enclosure && x.role != VolRole::Platform && x.role != VolRole::Tree) walls = true;
  for (size_t i = 0; i < c.v.size(); i++) {   // everything the building stands on its platform, upper tiers and cupolas too
    Volume& x = c.v[i];
    if (x.role == VolRole::Enclosure || x.role == VolRole::Platform || x.role == VolRole::Tree) continue;
    const bool onGround = x.z0 == 0;
    x.z0 = (int16_t)(x.z0 + fh);
    if (x.role == VolRole::Chimney || !onGround) continue;
    if (walls && x.role == VolRole::Porch) { porches.push_back(i); continue; }
    x0 = std::min(x0, (int)x.x0); x1 = std::max(x1, (int)x.x1); y0 = std::min(y0, (int)x.y0); y1 = std::max(y1, (int)x.y1);
  }
  if (x1 <= x0) return;
  auto plat = [&](int px0, int py0, int px1, int py1, bool st) {
    Volume p = c.vol(VolRole::Platform, std::max(-art::BLDG_PAD_X + 1, px0), std::max(-4, py0), std::min(c.W + art::BLDG_PAD_X - 1, px1),
                     std::min(c.H + art::BLDG_PAD_B - 1, py1), fh);
    p.wall = mat;
    p.roof = RoofShape::FlatParapet; p.roofMat = RoofMat::Adobe; p.pitch = 0;
    p.face = Face::Blank;
    if (st) p.feat |= VF_STEPS;
    c.add(p);
  };
  plat(x0 - 2, y0 - 1, x1 + 2, y1 + (porches.empty() ? frontPad : 2), steps && porches.empty());
  bool stepsDone = porches.empty();
  for (size_t i : porches) {   // (a porch reaching no further than the walls stands on the plinth: no step of its own)
    const Volume q = c.v[i];
    if (q.y1 + frontPad <= y1 + 2 || q.x1 <= q.x0) continue;
    plat(q.x0 - 1, y1, q.x1 + 1, q.y1 + frontPad, steps);
    stepsDone = true;
  }
  if (!stepsDone && steps) c.v.back().feat |= VF_STEPS;   // (the plinth keeps the steps then)
  c.grounded = true;
}
// a compound's gate: a gate building over the door column
inline int gateVol(Bx& c, int half, int depth, int wallH, int y1, Face face, RoofShape roof, RoofMat rm, int pitch, WallMat wall, uint32_t tint = 0) {
  Volume g = c.vol(VolRole::Gate, c.dc - half, y1 - depth, c.dc + half, y1, wallH);
  g.face = face; g.wall = wall; g.doorHere = true; g.door = DoorShape::Double;
  roofTo(g, roof, rm, pitch, tint);
  g.ridgeNS = false; g.eave = 0;
  if (roof == RoofShape::FlatParapet) g.roofMat = RoofMat::Adobe;
  return c.add(g);
}

}  // namespace bld
