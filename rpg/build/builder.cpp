// The BUILDER generator (VISION_PLAN 15.14, M3b "Builders & Societies"): the construction grammar. See blueprint.h.
//
// design() turns culture x purpose x wealth x urban x seed into a Blueprint: the resolved form, the massing (a body,
// then wings, towers, porches, annexes, tiers, drums with domes, chimneys, a compound's enclosure, gate and tents),
// storeys, a roof kind and materials per volume, doors, windows, ornament, signage, a yard and the interior's shape.
// Every building is ASSEMBLED, never copied: the seed draws the wing side, the porch, the ridge's direction, dormers,
// annexes, tiers and the storeys the form allows, so neighbours are never clones.
//
// The culture is ArchStyle::culture (cult::Archetype + 1). Each culture has its own vocabulary (longhouses and stave
// halls in the fjords, rubble cottages and tower houses in the highlands, timber frames and the only true castles in the
// heartland, courtyard domus and domes in the empire, flat terraces, iwans and minarets in the dunes, yurts and tent
// courts on the steppe, stilt houses in the marsh, halls on platforms under sweeping roofs on the jade terraces, tall
// gabled merchant houses on the river, stepped platforms under the sun, living halls in the sylvan woods and white
// spires in the starspire cities); the purpose adds its function (a shop front, a forge bay, a belfry, a gate).
//
// Deterministic integer maths only (the blueprint is generation: it decides interiors and footprints).
#include "rpg/build/blueprint.h"
#include <algorithm>
#include <cstdint>

namespace bld {

namespace {
uint64_t mix(uint64_t k, uint64_t v) {
  k ^= v + 0x9E3779B97F4A7C15ull + (k << 6) + (k >> 2);
  k *= 0xBF58476D1CE4E5B9ull;
  return k ^ (k >> 31);
}
uint32_t mix32(uint32_t x) {
  x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16;
  return x;
}
constexpr uint32_t RGB(int r, int g, int b) { return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16 | 0xFF000000u; }
uint32_t lerpC(uint32_t a, uint32_t b, int t100) {
  if (!a) return b;
  auto ch = [&](int s) { const int x = (int)((a >> s) & 255u), y = (int)((b >> s) & 255u); return (uint32_t)std::clamp(x + (y - x) * t100 / 100, 0, 255) << s; };
  return ch(0) | ch(8) | ch(16) | 0xFF000000u;
}
}  // namespace

const char* formName(Form f) {
  static const char* n[] = {"auto", "rect", "round", "L", "courtyard", "compound", "tower", "long", "stepped", "tent"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Form::COUNT, "a name for every form");
  return (int)f < (int)Form::COUNT ? n[(int)f] : "?";
}

uint64_t Request::key() const {
  uint64_t k = 0x6275696C64657233ull;   // "builder3": bump the constant when design() changes its output for a request
  k = mix(k, (uint64_t)purpose | (uint64_t)wTiles << 8 | (uint64_t)hTiles << 16 | (uint64_t)wealth << 24 | (uint64_t)urban << 32 |
             (uint64_t)form << 40 | (uint64_t)civic << 48 | (uint64_t)seat << 56);
  k = mix(k, style.key());
  k = mix(k, (uint64_t)seed);
  k = mix(k, (uint64_t)facts.storeys | (uint64_t)(facts.hearth ? 1 : 0) << 8 | (uint64_t)facts.emblem << 16 | (uint64_t)(uint32_t)facts.variant << 32);
  k = mix(k, (uint64_t)facts.banner << 32 | facts.banner2);
  if (facts.charred) k = mix(k, 0xC4A2ull << 8 | (uint64_t)(uint32_t)facts.charred);   // (M4) 0 keeps every key as before
  return k;
}

Request simpleRequest(art::Building purpose, int wTiles, int hTiles, const art::ArchStyle& style, uint32_t seed,
                      const art::BuildingFacts& facts) {
  Request r;
  r.purpose = purpose;
  r.wTiles = wTiles;
  r.hTiles = hTiles;
  r.style = style;
  r.seed = seed;
  r.facts = facts;
  return r;
}

namespace {

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
int cultureSeat(int cul) {
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

// the painter's roof slopes (x100) by pitch: 0..4 the culture's roofs, 5..8 spires and needles
const int kSlope[9] = {45, 62, 80, 105, 140, 190, 230, 280, 340};
bool flatRoof(RoofShape r) { return r == RoofShape::FlatParapet || r == RoofShape::Dome || r == RoofShape::Onion; }
bool pitchedRoof(RoofShape r) { return !flatRoof(r) && r != RoofShape::Stepped; }
bool weakWall(WallMat w) { return w == WallMat::Timber || w == WallMat::Plaster || w == WallMat::Log || w == WallMat::Plank || w == WallMat::Wattle || w == WallMat::Felt; }

// MIRRORS the painter (art_building.cpp volumeMass): eave overhangs at the sides / back and at the front
void overhang(const Volume& x, int& ov, int& ovF) {
  ov = 3; ovF = 2;
  switch (x.roof) {
    case RoofShape::Turf: ov = 2; break;
    case RoofShape::Pagoda: ov = 5; ovF = 4; break;
    case RoofShape::Sweep: ov = 4; ovF = 3; break;
    case RoofShape::Mansard: ov = 2; ovF = 2; break;
    case RoofShape::Stepped: ov = 1; ovF = 1; break;
    case RoofShape::Tent: case RoofShape::Spire: ov = 1; ovF = 1; break;
    case RoofShape::FlatParapet: case RoofShape::Dome: case RoofShape::Onion: ov = 0; ovF = 0; break;
    default: break;
  }
  if ((x.roofMat == RoofMat::Thatch || x.roofMat == RoofMat::Palm) && pitchedRoof(x.roof) && x.roof != RoofShape::Tent && x.roof != RoofShape::Spire) { ov = 4; ovF = 3; }
  if (ov > 0 || ovF > 0) { ov += x.eave; ovF += x.eave; }
  if (x.role == VolRole::Chimney || x.role == VolRole::Platform || x.role == VolRole::Enclosure || x.role == VolRole::Tree) { ov = 0; ovF = 0; }
}
int slopeOf(const Volume& x) {
  int s = kSlope[std::clamp((int)x.pitch, 0, 8)];
  if (x.roof == RoofShape::Steep) s = std::max(s, 140);
  if (x.roof == RoofShape::Turf) s = std::clamp(s, 80, 105);
  return s;
}
// MIRRORS the painter's roof heights (px)
int roofHeight(const Volume& x) {
  if (x.role == VolRole::Platform || x.role == VolRole::Enclosure || x.role == VolRole::Chimney || x.role == VolRole::Tree) return 0;
  int ov, ovF;
  overhang(x, ov, ovF);
  const int w = x.x1 - x.x0, d = x.y1 - x.y0;
  if (x.shape != VolShape::Box) {
    const int r = std::min(w, d) / 2;
    switch (x.roof) {
      case RoofShape::Dome: return r * 60 / 100;
      case RoofShape::Onion: return (r * 10 + 15) * 17 / 100;
      case RoofShape::FlatParapet: return 0;
      case RoofShape::Stepped: return r * 60 / 100;
      default:
        if (x.roofMat == RoofMat::Felt && x.roof == RoofShape::Conical) return (r + ov) * (52 + 8 * std::min(3, (int)x.pitch)) / 100;
        if (x.roof == RoofShape::Tent) return (r + ov) * 95 / 100;
        if (x.roof == RoofShape::Sweep) return (r + ov) * 115 / 100;   // (fixer: a slender swept cone)
        return (r + ov) * std::max(130, slopeOf(x) * 14 / 10) / 100;
    }
  }
  const int span = x.ridgeNS ? (w + 2 * ov) : (d + ov + ovF);
  switch (x.roof) {
    case RoofShape::FlatParapet: return 0;
    case RoofShape::Dome: return std::max(0, std::min(w, d) / 2 - 7) * 80 / 100;
    case RoofShape::Onion: return std::max(0, std::min(w, d) / 2 - 6) * 160 / 100;
    case RoofShape::Stepped: return span * std::max(70, slopeOf(x)) / 200;
    case RoofShape::Spire: return std::min(w + 2 * ov, d + ov + ovF) * slopeOf(x) / 200;
    case RoofShape::Tent: return span * 60 / 200;
    default: return span * slopeOf(x) / 200;
  }
}
// px the volume rises above the footprint's top edge (z - y at its highest point), roughly as the painter measures it
int riseOf(const Volume& x) {
  const int zt = x.z0 + x.wallH, rh = roofHeight(x);
  int ov, ovF;
  overhang(x, ov, ovF);
  if (x.role == VolRole::Chimney) return zt + 1 - x.y0;
  if (x.role == VolRole::Tree) return zt + 40 - (x.y0 + x.y1) / 2;   // the crown
  if (x.shape != VolShape::Box) {
    const int r = std::min(x.x1 - x.x0, x.y1 - x.y0) / 2, cy = x.y1 - r;
    if (x.roof == RoofShape::FlatParapet) return zt + 5 - (cy - r);
    return std::max(zt - 1 + rh - cy, zt + 2 - (cy - r));
  }
  switch (x.roof) {
    case RoofShape::FlatParapet: case RoofShape::Stepped: return std::max(zt + 6 - x.y0, zt + rh - (x.y0 + x.y1) / 2);
    case RoofShape::Dome: return std::max(zt + 6 - x.y0, zt + 3 + rh - (x.y0 + x.y1) / 2);
    case RoofShape::Onion: return std::max(zt + 6 - x.y0, zt + 6 + rh - (x.y0 + x.y1) / 2);
    default: break;
  }
  // the upswept eave corners of a pagoda or a sweep roof stand above the back eave
  const int lift = x.roof == RoofShape::Pagoda ? 6 : (x.roof == RoofShape::Sweep ? 8 : 0);
  if (x.ridgeNS) return std::max(zt - 1 + rh - (x.y1 - 8), zt + lift - (x.y0 - ov));   // the painter hips the back of a long ridge
  return std::max(zt - 1 + rh - (x.y0 - ov + x.y1 + ovF) / 2, zt + lift - (x.y0 - ov));
}

// the height of a box volume's pitched roof at depth y on its ridge line's column (an upper stage set on the roof
// stands with its front foot there)
int roofZAt(const Volume& x, int y) {
  int ov, ovF;
  overhang(x, ov, ovF);
  const int rh = roofHeight(x), zt = x.z0 + x.wallH - 1;
  if (!pitchedRoof(x.roof)) return zt + 1;
  if (x.ridgeNS) return zt + rh;
  const int ry0 = x.y0 - ov, ry1 = x.y1 + ovF, half = std::max(1, (ry1 - ry0) / 2);
  const int d = std::min(y - ry0, ry1 - y);
  return zt + rh * std::clamp(d, 0, half) / half;
}

// ---------------------------------------------------------------- the builder's working state
enum class PK : uint8_t { Home, Hut, Farm, Shop, Inn, Smithy, Temple, Keep, Palace, Tower, Windmill, Watermill, Barracks, Guildhall,
                          Exchange, MeadHall, Bath, TeaHouse, Lodge, Council };
PK kindOf(Building b) {
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

// ---------------------------------------------------------------- pieces
// set a volume's roof
void roofTo(Volume& x, RoofShape s, RoofMat m, int pitch, uint32_t tint = 0) { x.roof = s; x.roofMat = m; x.pitch = (uint8_t)pitch; x.roofTint = tint; }
void flat(Volume& x, bool crenels = false) {
  x.roof = RoofShape::FlatParapet;
  if (x.roofMat != RoofMat::Adobe && x.roofMat != RoofMat::ClayTile) x.roofMat = RoofMat::Adobe;
  x.pitch = 0;
  if (crenels) x.feat |= VF_CRENELS;
}
// a door porch before the body's front (the body's front is set back by `back` px): a gabled porch on two posts, a
// columned portico under a pediment, a veranda of posts the width of the house, or an arcade; it carries the door
int porch(Bx& c, int bi, int kind, int halfW, int depth) {
  const Volume& bo = c.v[(size_t)bi];
  const int y1 = std::min(c.H + art::BLDG_PAD_B - 1, bo.y1 + depth);
  int x0 = c.dc - halfW, x1 = c.dc + halfW;
  if (kind == 2) { x0 = bo.x0 + 2; x1 = bo.x1 - 2; }
  Volume p = c.vol(VolRole::Porch, x0, bo.y1 - 1, x1, y1, std::max(14, std::min((int)bo.wallH - 4, 20)));
  p.z0 = bo.z0;
  p.window = bo.window; p.door = bo.door; p.wall = bo.wall; p.wallTint = bo.wallTint; p.trimTint = bo.trimTint;
  p.ornament = bo.ornament;
  p.feat = bo.feat & (VF_STILTS);
  p.roofMat = bo.roofMat; p.roofTint = bo.roofTint;
  switch (kind) {
    case 0: p.face = Face::Colonnade; p.roof = pitchedRoof(bo.roof) && bo.roof != RoofShape::Turf ? RoofShape::Gable : RoofShape::Hip; p.ridgeNS = true; p.pitch = (uint8_t)std::max(2, (int)bo.pitch); break;
    case 1: p.face = Face::Colonnade; p.roof = RoofShape::Gable; p.ridgeNS = true; p.pitch = 0; p.wallH = (int16_t)std::max(18, (int)bo.wallH - 2);
            if (p.roofMat == RoofMat::Thatch || p.roofMat == RoofMat::Palm || p.roofMat == RoofMat::Adobe) p.roofMat = RoofMat::ClayTile; break;
    case 2: p.face = Face::Veranda; p.roof = RoofShape::Hip; p.pitch = 1; p.wallH = (int16_t)std::max(14, std::min((int)bo.wallH - 6, 18)); break;
    default: p.face = Face::Arcade; p.roof = flatRoof(bo.roof) ? RoofShape::FlatParapet : RoofShape::Hip; p.pitch = 1; break;
  }
  if (p.roof == RoofShape::FlatParapet) { p.roofMat = RoofMat::Adobe; p.wallH = (int16_t)(p.wallH + 4); }
  if (p.roof == RoofShape::Turf) p.roof = RoofShape::Gable;
  c.v[(size_t)bi].doorHere = false;
  p.doorHere = true;
  return c.add(p);
}
// a lower side wing beside the body (east or west), set back a little
int sideWing(Bx& c, int bi, bool east, int ww, int setBack, int wallH) {
  const Volume bo = c.v[(size_t)bi];
  Volume w = c.vol(VolRole::Wing, east ? bo.x1 : bo.x0 - ww, bo.y0 + setBack, east ? bo.x1 + ww : bo.x0, bo.y1 - (setBack > 8 ? 2 : 0), wallH);
  w.z0 = bo.z0;
  w.wall = bo.wall; w.wallTint = bo.wallTint; w.roofMat = bo.roofMat; w.roofTint = bo.roofTint; w.window = bo.window;
  w.feat = bo.feat & VF_STILTS;
  w.roof = bo.roof == RoofShape::Gable || bo.roof == RoofShape::Steep ? RoofShape::Hip : bo.roof;
  if (bo.shape != VolShape::Box) { w.roof = bo.roof; }
  w.ridgeNS = false;
  return c.add(w);
}
// chimneys over a pitched or flat body (chimney => hearth: only with facts.hearth)
void chimneys(Bx& c, int bi, int n, int cw = 6) {
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
// a gable-end stack at each end of a side-gabled cottage (highland, heartland)
void endStacks(Bx& c, int bi, int n) {
  if (!c.r.facts.hearth || n <= 0) return;
  const Volume bo = c.v[(size_t)bi];
  for (int i = 0; i < n; i++) {
    const bool east = n == 1 ? c.ch(128) : i == 1;
    const int x = east ? bo.x1 - 9 : bo.x0 + 3;
    Volume ch = c.vol(VolRole::Chimney, x, (bo.y0 + bo.y1) / 2 - 1, x + 6, (bo.y0 + bo.y1) / 2 + 3, 6);
    ch.wall = bo.wall == WallMat::Rubble || bo.wall == WallMat::Stone ? WallMat::Stone : WallMat::Brick;
    ch.roof = RoofShape::FlatParapet;
    ch.z0 = bo.z0;
    c.add(ch);
  }
}
// a tower (square or round) at x, its front at y1
int towerVol(Bx& c, VolRole role, bool round, int x0, int y1, int w, int wallH, RoofShape roof, RoofMat mat, int pitch, uint32_t tint = 0) {
  Volume t = round ? c.rnd(role, x0 + w / 2, y1, w / 2, wallH) : c.vol(role, x0, y1 - w, x0 + w, y1, wallH);
  roofTo(t, roof, mat, pitch, tint);
  t.ridgeNS = false;
  t.eave = 0;
  if (roof == RoofShape::FlatParapet) t.roofMat = RoofMat::Adobe;
  return c.add(t);
}
// lift every volume standing on the ground onto a platform (plinth, terrace, dais) of height fh
void platformUnder(Bx& c, int fh, WallMat mat, bool steps, int frontPad = 3) {
  int x0 = 1 << 20, x1 = -(1 << 20), y0 = 1 << 20, y1 = -(1 << 20);
  for (Volume& x : c.v) {   // everything the building stands on its platform, upper tiers and cupolas too
    if (x.role == VolRole::Enclosure || x.role == VolRole::Platform || x.role == VolRole::Tree) continue;
    const bool onGround = x.z0 == 0;
    x.z0 = (int16_t)(x.z0 + fh);
    if (x.role == VolRole::Chimney || !onGround) continue;
    x0 = std::min(x0, (int)x.x0); x1 = std::max(x1, (int)x.x1); y0 = std::min(y0, (int)x.y0); y1 = std::max(y1, (int)x.y1);
  }
  if (x1 <= x0) return;
  Volume p = c.vol(VolRole::Platform, std::max(-art::BLDG_PAD_X + 1, x0 - 2), std::max(-4, y0 - 1), std::min(c.W + art::BLDG_PAD_X - 1, x1 + 2),
                   std::min(c.H + art::BLDG_PAD_B - 1, y1 + frontPad), fh);
  p.wall = mat;
  p.roof = RoofShape::FlatParapet; p.roofMat = RoofMat::Adobe; p.pitch = 0;
  p.face = Face::Blank;
  if (steps) p.feat |= VF_STEPS;
  c.add(p);
  c.grounded = true;
}
void stilts(Bx& c, int h) {
  for (Volume& x : c.v) {
    if (x.role == VolRole::Enclosure || x.role == VolRole::Platform || x.role == VolRole::Tree) continue;
    x.z0 = (int16_t)(x.z0 + h);
    if (x.role != VolRole::Chimney) x.feat |= VF_STILTS;
  }
  c.grounded = true;
}

// ---------------------------------------------------------------- homes: the generic rectangular house
struct HouseSpec {
  int base = 26;            // wall height of one storey
  Face face = Face::Windows;
  int porchP = 70;          // chance /256 of a porch (rich: more)
  int porchKind = -1;       // -1 by culture
  bool wings = true, cross = true, dormers = true, jetty = true;
  int ridge = -1;           // -1 by seed, 0 across the view, 1 north-south (the gable to the street)
  bool inset = true;        // a poor house stands smaller on its plot
};
int house(Bx& c, const HouseSpec& sp) {
  const int n = c.want;
  int x0 = 0, x1 = c.W, y0 = 0, y1 = c.H;
  // a poor house stands smaller than its plot (a yard round it); a narrow plot keeps its width
  if (sp.inset && c.wl == 0 && c.W >= 48) {
    const int room = std::max(0, std::min(c.dc - 10 - 2, c.W - c.dc - 10 - 2));
    const int a = std::min(room, c.rg(2, 7)), bb = std::min(room, c.rg(2, 7));
    x0 += a; x1 -= bb;
    if (c.H >= 32) y0 += c.rg(2, 6);
  } else if (c.wl == 1 && c.H >= 48 && c.ch(90)) y0 += c.rg(2, 4);
  // a wing: a lower side wing (set back), or an L (the wing forward, the body set back)
  int wing = 0, lWing = 0, ww = 0;
  if (sp.wings && c.W >= 64 && c.ch(c.wl >= 2 ? 150 : 80)) {
    ww = c.W >= 80 ? c.rg(22, 28) : c.rg(17, 20);
    wing = c.ch(128) ? 1 : -1;
    if (wing == 1 && x1 - ww < c.dc + 10) wing = 0;
    if (wing == -1 && x0 + ww > c.dc - 10) wing = 0;
    if (wing && c.H >= 48 && c.ch(110)) lWing = wing;
  }
  int mx0 = x0, mx1 = x1;
  if (wing == 1) mx1 = x1 - ww;
  if (wing == -1) mx0 = x0 + ww;
  int my1 = y1;
  if (lWing) my1 = y1 - c.rg(8, 12);
  // a porch: the body stands back to make room
  const bool pitched = pitchedRoof(c.st.roof);
  int pk = sp.porchKind;
  if (pk < 0) pk = (c.st.ornament & art::ORN_PORCH_COLUMNS) ? 1 : 0;
  const bool hasPorch = !lWing && my1 - y0 >= 32 && c.ch(sp.porchP + (c.wl >= 2 ? 50 : 0) - (c.wl == 0 ? 50 : 0));
  if (hasPorch) my1 -= pk == 2 ? 7 : 8;
  Volume bo = c.vol(VolRole::Body, mx0, y0, mx1, my1, c.wallFor(n, sp.base) + c.rg(-2, 2), n);
  bo.face = sp.face;
  bo.doorHere = true;
  if (pitched && c.ch(70)) bo.pitch = (uint8_t)std::clamp((int)bo.pitch + (c.ch(128) ? 1 : -1), 1, 4);
  if (flatRoof(bo.roof)) bo.wallH = (int16_t)(bo.wallH + 4);
  if (pitched) {
    const bool narrow = (mx1 - mx0) <= (my1 - y0) + 24;
    bo.ridgeNS = sp.ridge >= 0 ? sp.ridge == 1 : (narrow && c.pick(3) != 0);
    if (bo.roof == RoofShape::Turf && c.cul == CU_NONE) bo.ridgeNS = false;
  }
  if (n >= 2 && sp.jetty && (bo.wall == WallMat::Timber || bo.wall == WallMat::Plaster) && pitched && c.ch(128)) bo.feat |= VF_JETTY;
  if (sp.dormers && pitched && !bo.ridgeNS && my1 - y0 >= 44 && !c.st.snow &&
      (bo.roof == RoofShape::Gable || bo.roof == RoofShape::Hip || bo.roof == RoofShape::Steep || bo.roof == RoofShape::Mansard))
    bo.dormers = (uint8_t)c.pick(3);
  const int bi = c.add(bo);
  // the wing
  if (wing) {
    Volume w = c.vol(VolRole::Wing, wing == 1 ? mx1 : x0, lWing ? y0 + 4 : y0 + c.rg(4, 8), wing == 1 ? x1 : mx0, y1,
                     lWing ? c.v[(size_t)bi].wallH : std::max(16, c.wallFor(1, sp.base) - 4 - (c.k == PK::Inn ? 6 : 0)));
    w.wall = c.v[(size_t)bi].wall;
    if (lWing) {   // the L's forward wing turns its gable to the street
      w.storeys = (uint8_t)n;
      w.ridgeNS = pitched;
      if (pitched && w.roof == RoofShape::Hip && c.ch(128)) w.roof = RoofShape::Gable;
      if (c.v[(size_t)bi].feat & VF_JETTY) w.feat |= VF_JETTY;
    } else {
      w.ridgeNS = false;
      if (w.roof == RoofShape::Gable || w.roof == RoofShape::Steep) w.roof = RoofShape::Hip;
      if (flatRoof(w.roof)) { w.roof = RoofShape::FlatParapet; w.wallH = (int16_t)(w.wallH + 4); }
    }
    c.add(w);
  }
  // a cross gable over a bay of a wide side-gabled body
  Volume& B = c.v[(size_t)bi];
  if (sp.cross && pitched && !B.ridgeNS && B.x1 - B.x0 >= 72 && B.roof != RoofShape::Turf && c.ch(110)) {
    const int cw = c.rg(22, 28);
    const bool left = c.ch(128);
    int cx0 = left ? B.x0 + 6 : B.x1 - 6 - cw;
    if (std::abs(cx0 + cw / 2 - c.dc) < 8) cx0 = left ? cx0 + 16 : cx0 - 16;
    if (cx0 >= B.x0 + 2 && cx0 + cw <= B.x1 - 2) {
      Volume g = c.vol(VolRole::Wing, cx0, (B.y0 + B.y1) / 2, cx0 + cw, B.y1, B.wallH, n);
      g.roof = RoofShape::Gable; g.ridgeNS = true; g.wall = B.wall; g.feat = B.feat & VF_JETTY;
      g.gable = (uint8_t)(c.cul == CU_RIVER ? 1 + c.pick(2) : 0);
      B.dormers = 0;
      c.add(g);
    }
  }
  if (hasPorch) porch(c, bi, pk, pk == 1 ? 13 : 10, pk == 2 ? 8 : 9);
  // two storeys: a balcony over the door or a hood over it
  Volume& B2 = c.v[(size_t)bi];
  if (n >= 2 && !hasPorch && !c.st.awnings && B2.shape == VolShape::Box) {
    const int r = c.pick(5);
    if (r < 2 && c.dc - 12 >= B2.x0 + 2 && c.dc + 12 <= B2.x1 - 2) B2.feat |= VF_BALCONYDOOR;
    else if (r < 4) B2.feat |= VF_HOOD;
  } else if (n == 1 && !hasPorch && pitched && c.ch(60) && c.wl >= 1) c.v[(size_t)bi].feat |= VF_HOOD;
  return bi;
}

// a round house: yurt, rondavel, elven pod; annexes are smaller round ones
int roundHouse(Bx& c, int n, int base, bool felt, int annexes) {
  // (fix) a felt yurt is never stacked: one storey, a low lattice wall under a wide felt roof; a richer or bigger
  // household shows in more yurts round it, never in height
  if (felt) { n = 1; base = std::min(base, 17); }
  const int rad = std::max(10, std::min((c.W - (annexes ? 20 : 0)) / 2, c.H / 2 + 3) - 1 - (c.wl == 0 ? c.rg(1, 3) : c.rg(0, 2)));
  // the drum stands where the plot leaves it room: off centre when it can (the door stays on the door column)
  int cx = c.dc;
  const int room = std::max(0, std::min(rad - 8, (c.W - 2 * rad) / 2 - 1));
  if (room > 0) cx = std::clamp(c.dc + (c.ch(128) ? 1 : -1) * c.rg(0, room), rad + 1, c.W - rad - 1);
  if (std::abs(cx - c.dc) > rad - 8) cx = c.dc;
  Volume bo = c.rnd(VolRole::Body, cx, c.H - (c.H >= 40 ? c.pick(3) : 0), rad, felt ? 15 + c.rg(0, 2) : c.wallFor(n, base) + c.rg(-2, 2), n);
  bo.doorHere = true;
  if (felt) {
    bo.wall = WallMat::Felt; bo.roofMat = RoofMat::Felt; bo.roof = RoofShape::Conical; bo.feat |= VF_CROWN; bo.pitch = (uint8_t)c.rg(1, 2); bo.window = WindowShape::Round;
    bo.door = DoorShape::Flap;
    bo.eave = 0;
    // a felt cover sooted dark at the crown, or bleached, or the household's dyed band
    const int tone = c.pick(4);
    if (tone == 1) bo.roofTint = lerpC(c.st.roofTint ? c.st.roofTint : RGB(212, 198, 172), RGB(120, 104, 92), 30);
    else if (tone == 2) bo.roofTint = lerpC(c.st.roofTint ? c.st.roofTint : RGB(212, 198, 172), RGB(250, 246, 236), 40);
    else if (tone == 3 && c.st.accentTint) bo.roofTint = lerpC(c.st.roofTint ? c.st.roofTint : RGB(212, 198, 172), c.st.accentTint, 18);
  }
  const int bi = c.add(bo);
  for (int a = 0; a < annexes; a++) {
    const bool east = a == 0 ? (cx < c.W / 2 ? true : (cx > c.W / 2 ? false : c.ch(128))) : !(cx < c.W / 2);
    const int ar = std::max(7, std::min(rad * 2 / 3 - c.pick(3), (east ? c.W - (cx + rad) : cx - rad) + 6));
    const int ax = east ? std::min(c.W + art::BLDG_PAD_X - ar - 1, cx + rad + ar - 6) : std::max(-art::BLDG_PAD_X + ar + 1, cx - rad - ar + 6);
    Volume an = c.rnd(VolRole::Annex, ax, c.H - c.rg(2, 6), ar, felt ? std::max(12, bo.wallH - c.rg(1, 3)) : std::max(14, bo.wallH - c.rg(4, 8)));
    an.wall = bo.wall; an.roof = bo.roof; an.roofMat = bo.roofMat; an.feat = bo.feat & (VF_CROWN); an.pitch = bo.pitch; an.eave = bo.eave;
    an.window = bo.window; an.door = bo.door; an.roofTint = bo.roofTint;
    c.add(an);
  }
  return bi;
}

// a nomad household's camp round its ger: a felt store tent, a wooden entrance vestibule, a cart shed (by seed)
void campExtras(Bx& c, int bi) {
  const Volume B = c.v[(size_t)bi];
  const int r = std::min(B.x1 - B.x0, B.y1 - B.y0) / 2, cx = (B.x0 + B.x1) / 2;
  const int leftRoom = cx - r, rightRoom = c.W - (cx + r);
  const bool east = rightRoom >= leftRoom;
  const int roomPx = std::max(leftRoom, rightRoom) + art::BLDG_PAD_X - 2;
  switch (c.pick(c.wl >= 1 ? 4 : 3)) {
    case 1:   // a store tent of felt on its ridge pole, beside and behind the ger
      if (roomPx >= 12) {
        const int tw = std::min(roomPx, 14 + c.pick(5)), x0 = east ? cx + r - 4 : cx - r + 4 - tw;
        Volume t = c.vol(VolRole::Tent, x0, std::max(-4, B.y0 + 2), x0 + tw, std::max(B.y0 + 12, B.y1 - 8 - c.pick(4)), 7);
        roofTo(t, RoofShape::Tent, RoofMat::Felt, 2, B.roofTint); t.wall = WallMat::Felt; t.ridgeNS = true; t.feat |= VF_ROPES; t.face = Face::Blank;
        c.add(t);
      }
      break;
    case 2: {   // a wooden vestibule before the door, its own little felt roof (the door is in it)
      Volume v = c.vol(VolRole::Porch, c.dc - 7, B.y1 - 4, c.dc + 7, std::min(c.H + 3, B.y1 + 4), 13);
      roofTo(v, RoofShape::Gable, RoofMat::Felt, 1, B.roofTint); v.ridgeNS = true; v.wall = WallMat::Plank; v.face = Face::Windows; v.door = DoorShape::Double;
      v.doorHere = true; c.v[(size_t)bi].doorHere = false;
      c.add(v);
      break;
    }
    case 3:   // a cart shed of planks under felt
      if (roomPx >= 14) {
        const int sw = std::min(roomPx, 16 + c.pick(4)), x0 = east ? cx + r - 3 : cx - r + 3 - sw;
        Volume sh = c.vol(VolRole::Annex, x0, B.y1 - 16 - c.pick(4), x0 + sw, B.y1 - 2, 10);
        roofTo(sh, RoofShape::Hip, RoofMat::Felt, 1, B.roofTint); sh.wall = WallMat::Plank; sh.face = Face::Colonnade;
        c.add(sh);
      }
      break;
    default: break;
  }
}

// a courtyard house: a front range with the door, a back hall, side ranges, the court between (visible from above)
int courtHouse(Bx& c, int base, bool flatRoofs, int backStoreys) {
  const int n = c.want;
  const int fd = std::max(14, c.H / 4), bd = std::max(16, c.H * 3 / 10), sw = std::max(12, c.W / 5);
  // the back hall is the body (the main rooms, the storeys); the front range carries the door
  Volume bo = c.vol(VolRole::Body, 0, 0, c.W, bd, c.wallFor(std::max(n, backStoreys), base), std::max(n, backStoreys));
  if (flatRoofs) flat(bo);
  else bo.ridgeNS = false;
  if (flatRoofs) bo.wallH = (int16_t)(bo.wallH + 4);
  bo.face = Face::Windows;
  // (fix) the hall's door into the court shows only where the court can be seen over the front range (low flat roofs,
  // a deep court); behind a tiled front range it stood on that roof, an upper door opening onto nothing
  const int courtD = c.H - fd - bd;
  if (flatRoofs || courtD >= 40) bo.feat |= VF_DOOR;
  const int bi = c.add(bo);
  for (int sd = 0; sd < 2; sd++) {
    Volume sr = c.vol(VolRole::Wing, sd ? c.W - sw : 0, bd, sd ? c.W : sw, c.H - fd, c.wallFor(1, base - 4));
    if (flatRoofs) { flat(sr); sr.wallH = (int16_t)(sr.wallH + 4); } else { sr.ridgeNS = true; if (sr.roof == RoofShape::Gable || sr.roof == RoofShape::Steep) sr.roof = RoofShape::Hip; }
    sr.face = Face::Blank;
    c.add(sr);
  }
  Volume fr = c.vol(VolRole::Wing, 0, c.H - fd, c.W, c.H, c.wallFor(1, base - 6));
  if (flatRoofs) { flat(fr); fr.wallH = (int16_t)(fr.wallH + 4); } else fr.ridgeNS = false;
  fr.doorHere = true;
  fr.face = Face::Windows;
  c.add(fr);
  Volume court = c.vol(VolRole::Platform, sw, bd, c.W - sw, c.H - fd, 1);
  court.face = Face::Blank;
  court.wall = (c.cul == CU_JADE || c.cul == CU_IMPERIAL || c.cul == CU_STAR) ? WallMat::Ashlar : (c.cul == CU_DUNE ? WallMat::Adobe : WallMat::Stone);
  court.roof = RoofShape::FlatParapet;
  if (c.wl >= 3 && c.ch(128)) court.feat |= VF_GARDEN;
  c.add(court);
  return bi;
}

// a long hall: the ridge along the length (door in the long side), annexes and porches by seed
int longHall(Bx& c, int base, int n, int inset) {
  Volume bo = c.vol(VolRole::Body, inset, std::max(0, c.H / 8 - 2 + c.pick(3)), c.W - inset, c.H - (c.ch(110) ? 6 : 0), c.wallFor(n, base), n);
  bo.ridgeNS = false;
  bo.doorHere = true;
  if (bo.roof == RoofShape::Hip && c.ch(128)) bo.roof = RoofShape::Gable;
  return c.add(bo);
}

// ---------------------------------------------------------------- culture homes
void cultureHome(Bx& c) {
  HouseSpec sp;
  const int n = c.want;
  switch (c.cul) {
    case CU_FJORD: {
      // longhouses under turf or shingle; a loft house (jettied upper floor) when two storeys; carved porches for the rich
      sp.ridge = n >= 2 ? (c.ch(128) ? 1 : 0) : 0;
      sp.wings = c.W >= 80; sp.cross = false; sp.dormers = false; sp.porchKind = 0; sp.porchP = 60;
      sp.base = 22;
      const int bi = house(c, sp);
      Volume& B = c.v[(size_t)bi];
      if (n >= 2 && (B.wall == WallMat::Log || B.wall == WallMat::Plank)) B.feat |= VF_JETTY;
      if (c.wl >= 2) B.feat |= VF_CARVED;
      // a lean-to outshot along one end of the longhouse (byre, store)
      if (c.W >= 64 && c.ch(110) && B.x1 - B.x0 >= 56) {
        const bool east = c.ch(128);
        Volume lt = c.vol(VolRole::Annex, east ? B.x1 - 16 : B.x0, B.y0 - 2 < 0 ? 0 : B.y0, east ? B.x1 : B.x0 + 16, B.y0 + 10, std::max(12, B.wallH - 8));
        lt.roof = RoofShape::Hip; lt.ridgeNS = false; lt.pitch = 1;
        if (lt.roofMat == RoofMat::Turf) lt.roof = RoofShape::Turf;
        if ((east && c.dc < lt.x0 - 10) || (!east && c.dc > lt.x1 + 10)) c.add(lt);
      }
      chimneys(c, bi, c.st.chimneys);
      break;
    }
    case CU_HIGHLAND: {
      if (c.wl >= 3 && n >= 2 && c.W >= 48) {
        // a tower house: a tall narrow block (the hall over the vaulted store), a round stair turret at a front corner,
        // a lower range beside it
        const int tw = std::max(34, std::min(c.W * 55 / 100, 52));
        const bool east = c.ch(128);
        int tx0 = std::clamp(c.dc - tw / 2 + (east ? 6 : -6), 0, c.W - tw);
        Volume bo = c.vol(VolRole::Body, tx0, 0, tx0 + tw, c.H - 2, c.wallFor(n, 28) + 6, n);
        bo.roof = RoofShape::Gable; bo.ridgeNS = true; bo.gable = 1; bo.pitch = (uint8_t)std::max(3, (int)c.st.pitch);
        bo.roofMat = bo.roofMat == RoofMat::Thatch ? RoofMat::Slate : bo.roofMat; bo.wall = bo.wall == WallMat::Wattle ? WallMat::Rubble : bo.wall;
        bo.doorHere = true;
        const int bi = c.add(bo);
        const int rx = east ? tx0 + tw - 3 : tx0 + 3;
        Volume t = c.rnd(VolRole::Tower, rx, c.H - 1, 6, bo.wallH + 8);
        roofTo(t, RoofShape::Conical, RoofMat::Slate, 4); t.wall = bo.wall; t.face = Face::Slits; t.eave = 0;
        c.add(t);
        if (c.W - tw >= 20) {
          Volume r = c.vol(VolRole::Wing, east ? 0 : tx0 + tw, 6, east ? tx0 : c.W, c.H, c.wallFor(1, 22));
          r.ridgeNS = false; r.roof = RoofShape::Gable; r.roofMat = c.st.roofMat; r.wall = bo.wall;
          if ((east && r.x1 - r.x0 >= 16) || (!east && r.x1 - r.x0 >= 16)) c.add(r);
        }
        endStacks(c, bi, 1);
        return;
      }
      sp.base = c.wl == 0 ? 20 : 24;
      sp.ridge = 0;   // side-gabled cottages with a stack at each gable
      sp.cross = c.wl >= 2; sp.dormers = c.wl >= 2; sp.porchKind = 0; sp.porchP = 40;
      const int bi = house(c, sp);
      Volume& B = c.v[(size_t)bi];
      if (c.wl == 0 && B.roofMat == RoofMat::Thatch) { B.roof = RoofShape::Hip; B.pitch = 3; }   // the blackhouse's rounded hip
      endStacks(c, bi, c.wl == 0 ? 1 : (c.ch(150) ? 2 : 1));
      break;
    }
    case CU_IMPERIAL: {
      if (c.wl >= 2 && c.W >= 80 && c.H >= 48 && n == 1) {   // a domus round its atrium
        const int bi = courtHouse(c, 24, false, 1);
        Volume& B = c.v[(size_t)bi];
        roofTo(B, RoofShape::Hip, RoofMat::ClayTile, 1, B.roofTint);
        for (Volume& x : c.v) if (x.roofMat == RoofMat::Thatch) x.roofMat = RoofMat::ClayTile;
        if (c.wl >= 3) { c.v.back().feat |= VF_GARDEN; }
        chimneys(c, bi, 1);
        return;
      }
      sp.base = 26; sp.porchKind = 1; sp.porchP = 90; sp.dormers = false; sp.jetty = false;
      sp.ridge = c.ch(170) ? 0 : 1;
      const int bi = house(c, sp);
      for (Volume& x : c.v) {
        if (x.roof == RoofShape::Gable || x.roof == RoofShape::Steep) x.roof = c.ch(128) ? RoofShape::Hip : RoofShape::Gable;
        if (x.pitch > 2) x.pitch = 2;
      }
      chimneys(c, bi, std::min(1, (int)c.st.chimneys));
      break;
    }
    case CU_DUNE: {
      if (c.wl >= 2 && c.W >= 80 && c.H >= 48) {   // a courtyard house; the rich add a dome over the main room
        const int bi = courtHouse(c, 24, true, n);
        if (c.wl >= 3 || c.ch(100)) {
          Volume& B = c.v[(size_t)bi];
          const int dw = std::min(B.x1 - B.x0, 40);
          const int dx = c.ch(128) ? B.x0 + 4 : B.x1 - 4 - dw;
          Volume d = c.vol(VolRole::Drum, dx, 0, dx + dw, std::min((int)B.y1, dw), 4);
          d.z0 = (int16_t)(B.z0 + B.wallH - 4);
          roofTo(d, RoofShape::Dome, c.st.roofMat == RoofMat::GlazedTile ? RoofMat::GlazedTile : RoofMat::Adobe, 0, c.st.roofMat == RoofMat::GlazedTile ? c.domeTint() : 0);
          d.face = Face::Blank;
          c.add(d);
        }
        if (c.st.ornament & art::ORN_WINDCATCHER) {
          const Volume B = c.v[(size_t)bi];
          const int wx = c.ch(128) ? B.x0 + 4 : B.x1 - 14;
          Volume wc = c.vol(VolRole::Tower, wx, B.y0 + 3, wx + 10, B.y0 + 11, 15);
          wc.z0 = (int16_t)(B.z0 + B.wallH); flat(wc); wc.face = Face::Vents;
          c.add(wc);
        }
        chimneys(c, bi, 1);
        return;
      }
      sp.base = 24; sp.porchKind = 3; sp.porchP = c.wl >= 2 ? 80 : 20; sp.wings = c.W >= 64;
      const int bi = house(c, sp);
      Volume& B = c.v[(size_t)bi];
      if (B.roof == RoofShape::Dome && c.ch(140)) { B.roof = RoofShape::FlatParapet; }
      if (B.roof == RoofShape::Dome || B.roof == RoofShape::Onion) {   // a lime-washed dome, or the rich's turquoise glaze
        if (c.wl >= 2) { B.roofMat = RoofMat::GlazedTile; B.roofTint = c.domeTint(); }
        else { B.roofMat = RoofMat::Adobe; B.roofTint = lerpC(B.wallTint ? B.wallTint : RGB(222, 186, 132), RGB(246, 238, 220), 55); }
      }
      if (flatRoof(B.roof) && B.x1 - B.x0 >= 36 && c.ch(c.wl >= 1 ? 170 : 90)) {   // the stair head room on the roof
        const Volume Bc = B;
        const int rw = std::min(Bc.x1 - Bc.x0 - 8, c.rg(16, 24));
        const int rx = c.ch(128) ? Bc.x0 + 3 : Bc.x1 - 3 - rw;
        Volume rr = c.vol(VolRole::Annex, rx, Bc.y0 + 3, rx + rw, Bc.y0 + 3 + std::min(16, (Bc.y1 - Bc.y0) * 4 / 10), 11);
        rr.z0 = (int16_t)(Bc.z0 + Bc.wallH); flat(rr);
        c.add(rr);
      }
      if ((c.st.ornament & art::ORN_WINDCATCHER) && flatRoof(c.v[(size_t)bi].roof) && c.v[(size_t)bi].x1 - c.v[(size_t)bi].x0 >= 40) {
        const Volume Bc = c.v[(size_t)bi];
        bool left = c.ch(128);
        for (const Volume& x : c.v) if (x.role == VolRole::Annex && x.z0 > 0) left = x.x0 > (Bc.x0 + Bc.x1) / 2;
        const int wx = left ? Bc.x0 + 4 : Bc.x1 - 14;
        Volume wc = c.vol(VolRole::Tower, wx, Bc.y0 + 5, wx + 10, Bc.y0 + 13, 15);
        wc.z0 = (int16_t)(Bc.z0 + Bc.wallH); flat(wc); wc.face = Face::Vents;
        c.add(wc);
      }
      chimneys(c, bi, 1);
      break;
    }
    case CU_STEPPE: {
      // (fix) one storey always: a household that asked for more (a bigger, richer one) pitches a second yurt beside
      const int ann = c.W >= 64 ? (c.wl >= 2 || n >= 2 ? (c.W >= 80 ? 2 : 1) : (c.ch(120) ? 1 : 0)) : (n >= 2 && c.W >= 48 ? 1 : 0);
      const int bi = roundHouse(c, 1, 20, true, ann);
      campExtras(c, bi);
      break;
    }
    case CU_MARSH: {
      sp.base = 22; sp.porchKind = 2; sp.porchP = 110; sp.cross = false; sp.dormers = false; sp.jetty = false;
      sp.ridge = c.ch(150) ? 0 : 1;
      const int bi = house(c, sp);
      for (Volume& x : c.v) if (pitchedRoof(x.roof) && x.role != VolRole::Porch) { x.roof = c.ch(170) ? RoofShape::Steep : RoofShape::Hip; x.pitch = 4; }
      if (c.wl >= 2) c.v[(size_t)bi].feat |= VF_CARVED;
      chimneys(c, bi, c.wl >= 2 ? 1 : 0);
      stilts(c, 6 + c.pick(3));   // the marsh folk build every house on stilts, over the wet ground or the water
      break;
    }
    case CU_JADE: {
      if (c.wl >= 3 && c.W >= 80 && c.H >= 48 && n == 1) {   // a siheyuan: halls round a court, the gate in the front range
        const int bi = courtHouse(c, 24, false, 1);
        for (Volume& x : c.v) if (x.role != VolRole::Platform) { roofTo(x, RoofShape::Pagoda, x.roofMat == RoofMat::Thatch ? RoofMat::ClayTile : x.roofMat, 2, x.roofTint); x.ridgeNS = x.role == VolRole::Wing && x.y1 < c.H - 4 && x.x1 - x.x0 < 30; }
        c.v.back().feat |= VF_GARDEN;
        chimneys(c, bi, 1);
        return;
      }
      sp.base = 24; sp.porchKind = c.wl >= 2 ? 2 : 0; sp.porchP = 80; sp.dormers = false; sp.cross = false; sp.jetty = false;
      sp.ridge = 0;
      const int bi = house(c, sp);
      Volume& B = c.v[(size_t)bi];
      if (B.roofMat == RoofMat::Thatch || B.roofMat == RoofMat::Palm) { B.roof = RoofShape::Gable; B.pitch = (uint8_t)std::max(3, (int)B.pitch); }
      else if (c.wl >= 1) { B.roof = c.ch(170) ? RoofShape::Pagoda : RoofShape::Hip; B.pitch = 2; }
      if (n >= 2 && c.wl >= 1 && B.roof == RoofShape::Pagoda && c.v.size() == 1) {   // a double eave: a skirt roof round the ground floor
        Volume sk = c.vol(VolRole::Porch, B.x0, B.y1 - 1, B.x1, std::min(c.H + 3, B.y1 + 6), 16);
        roofTo(sk, RoofShape::Pagoda, B.roofMat, 1, B.roofTint); sk.face = Face::Veranda; sk.doorHere = true; B.y1 = (int16_t)(B.y1 - 6); sk.y0 = (int16_t)(B.y1 - 1);
        B.doorHere = false;
        c.add(sk);
      }
      chimneys(c, bi, c.wl >= 1 ? 1 : 0);
      if (c.st.foundation == art::Foundation::Platform || c.st.foundation == art::Foundation::Terrace || c.wl >= 2) platformUnder(c, c.wl >= 2 ? 5 : 3, WallMat::Ashlar, true);
      break;
    }
    case CU_RIVER: {
      // tall narrow merchant houses, the gable to the street (crow-stepped, bell or plain), a hoist dormer
      sp.base = 26; sp.ridge = 1; sp.cross = c.W >= 80; sp.porchP = 20; sp.wings = c.W >= 80;
      const int bi = house(c, sp);
      Volume& B = c.v[(size_t)bi];
      if (pitchedRoof(B.roof) && B.roof != RoofShape::Mansard) {
        B.ridgeNS = true;
        B.roof = RoofShape::Gable;
        B.pitch = (uint8_t)std::max(3, (int)B.pitch);
        B.gable = (uint8_t)(c.wl == 0 ? 0 : (c.pick(4) < 3 ? 1 + c.pick(2) : 3));
      }
      // a double house: two gables side by side on a wide plot
      if (B.x1 - B.x0 >= 72 && B.ridgeNS && c.v.size() == 1 && c.ch(170)) {
        const int mid = (B.x0 + B.x1) / 2 + (c.dc < (B.x0 + B.x1) / 2 ? 4 : -4);
        Volume g = B;
        g.role = VolRole::Wing; g.doorHere = false;
        if (c.dc < mid) { g.x0 = (int16_t)mid; B.x1 = (int16_t)mid; } else { g.x1 = (int16_t)mid; B.x0 = (int16_t)mid; }
        g.wallH = (int16_t)(g.wallH + (c.ch(128) ? 4 : -4));
        g.gable = (uint8_t)(1 + c.pick(3));
        g.wallTint = lerpC(B.wallTint, RGB(150, 70, 60), 20);
        c.add(g);
      }
      chimneys(c, bi, 1);
      break;
    }
    case CU_SUN: {
      if (c.wl == 0 && n == 1) {   // a palm-thatched hut of wattle
        sp.base = 20; sp.ridge = 0; sp.cross = false; sp.dormers = false; sp.porchKind = 2; sp.porchP = 60;
        c.st.roof = RoofShape::Steep; c.st.roofMat = RoofMat::Palm; c.st.wall = WallMat::Wattle;
        const int bi = house(c, sp);
        chimneys(c, bi, 0);
        break;
      }
      // a lime-plastered flat-roofed block; most carry a set-back upper room so the house steps up like a little temple
      c.st.roof = RoofShape::FlatParapet; c.st.roofMat = RoofMat::Adobe;
      sp.base = 24; sp.porchKind = 3; sp.porchP = c.wl >= 2 ? 70 : 20; sp.wings = c.W >= 64;
      const int bi = house(c, sp);
      const Volume B = c.v[(size_t)bi];
      if (B.x1 - B.x0 >= 36 && !(c.v.size() > 1 && c.v.back().role == VolRole::Porch) && c.ch(60) && c.H >= 32) {
        // a walled forecourt: the house stands back behind a low parapet wall with a gap at the door
        if (c.v[(size_t)bi].y1 >= c.H - 2 && c.v[(size_t)bi].y1 - c.v[(size_t)bi].y0 >= 30) {
          c.v[(size_t)bi].y1 = (int16_t)(c.v[(size_t)bi].y1 - 9);
          c.v[(size_t)bi].doorHere = false;
          const Volume Bm = c.v[(size_t)bi];   // (a copy: the list grows below)
          for (int sd = 0; sd < 2; sd++) {
            Volume e = c.vol(VolRole::Enclosure, sd ? c.dc + 7 : Bm.x0, c.H - 3, sd ? Bm.x1 : c.dc - 7, c.H, 9);
            e.wall = WallMat::Adobe; e.wallTint = Bm.wallTint; e.roof = RoofShape::FlatParapet; e.roofMat = RoofMat::Adobe; e.face = Face::Blank;
            if (e.x1 - e.x0 >= 4) c.add(e);
          }
          for (int sd = 0; sd < 2; sd++) {
            Volume e = c.vol(VolRole::Enclosure, sd ? Bm.x1 - 3 : Bm.x0, Bm.y1, sd ? Bm.x1 : Bm.x0 + 3, c.H - 3, 9);
            e.wall = WallMat::Adobe; e.wallTint = Bm.wallTint; e.roof = RoofShape::FlatParapet; e.roofMat = RoofMat::Adobe; e.face = Face::Blank;
            c.add(e);
          }
          Volume d = c.vol(VolRole::Porch, c.dc - 7, c.H - 6, c.dc + 7, c.H, 12);
          d.wall = WallMat::Adobe; d.wallTint = Bm.wallTint; flat(d, true); d.face = Face::Gate; d.doorHere = true;
          c.add(d);
        }
      }
      if (B.x1 - B.x0 >= 36 && c.ch(170)) {
        const int bw = B.x1 - B.x0, bd = B.y1 - B.y0;
        const int kind = c.pick(3);
        const int rw = std::max(20, kind == 0 ? bw * 62 / 100 : bw * (38 + c.pick(3) * 6) / 100);
        const int rx = kind == 0 ? B.x0 + (bw - rw) / 2 : (c.ch(128) ? B.x0 + 2 : B.x1 - 2 - rw);
        Volume rr = c.vol(VolRole::Tier, rx, B.y0 + 2, rx + rw, B.y0 + 2 + std::max(12, bd * (45 + c.pick(2) * 10) / 100), 14 + c.pick(3) * 2);
        rr.z0 = (int16_t)(B.z0 + B.wallH); flat(rr);
        c.add(rr);
      }
      if (c.wl >= 3) platformUnder(c, 4, WallMat::Ashlar, true);
      break;
    }
    case CU_SYLVAN: {
      if (c.ch(c.wl >= 2 ? 110 : 170) || c.W < 56) {   // a living pod (round) under a leaf cone or sweep
        const int bi = roundHouse(c, n, 22, false, c.W >= 72 && c.wl >= 1 ? 1 : 0);
        for (Volume& x : c.v) {
          x.wall = WallMat::Living;
          if (x.roofMat != RoofMat::Bark && x.roofMat != RoofMat::Leaf) x.roofMat = RoofMat::Leaf;
          x.roof = (x.role == VolRole::Body || c.ch(170)) ? RoofShape::Sweep : RoofShape::Conical;
          x.window = WindowShape::Round; x.door = DoorShape::Round;
        }
        c.v[(size_t)bi].ornament |= art::ORN_VINES;
        // the pod's growth: a living veranda before the door, a second pod raised on its roots, or nothing
        const Volume B = c.v[(size_t)bi];
        const int g = c.pick(3);
        if (g == 1 && B.y1 - B.y0 >= 28) {
          Volume ve = c.vol(VolRole::Porch, c.dc - 9, B.y1 - 5, c.dc + 9, std::min(c.H + 3, B.y1 + 4), 13);
          roofTo(ve, RoofShape::Hip, B.roofMat, 1, B.roofTint); ve.wall = WallMat::Living; ve.face = Face::Veranda; ve.doorHere = true;
          c.v[(size_t)bi].doorHere = false;
          c.add(ve);
        } else if (g == 2 && c.W >= 48) {
          const bool east = (B.x0 + B.x1) / 2 < c.W / 2;
          const int pr = std::max(7, (B.x1 - B.x0) / 4 + c.pick(3));
          Volume up = c.rnd(VolRole::Wing, east ? std::min(c.W + 4 - pr, B.x1 - 2) : std::max(pr - 4, (int)B.x0 + 2), B.y0 + 2 * pr + 2, pr, 12);
          up.z0 = (int16_t)(8 + c.pick(4)); up.feat |= VF_STILTS; up.wall = WallMat::Living; roofTo(up, RoofShape::Conical, B.roofMat, 4, B.roofTint);
          up.window = WindowShape::Round;
          c.add(up);
        }
        return;
      }
      sp.base = 24; sp.ridge = 0; sp.cross = false; sp.dormers = false; sp.porchKind = 2; sp.porchP = 70;
      c.st.roof = RoofShape::Sweep;
      if (c.st.roofMat != RoofMat::Bark) c.st.roofMat = RoofMat::Leaf;
      const int bi = house(c, sp);
      if (c.W >= 64 && c.ch(140)) {   // a pod grown at one end of the hall
        const Volume B = c.v[(size_t)bi];
        const bool east = c.dc < (B.x0 + B.x1) / 2;
        const int rad = std::min(13, c.H / 2 - 2);
        Volume pod = c.rnd(VolRole::Annex, east ? B.x1 - rad + 4 : B.x0 + rad - 4, c.H - 2, rad, B.wallH + 4);
        roofTo(pod, RoofShape::Conical, B.roofMat, 4); pod.wall = WallMat::Living; pod.window = WindowShape::Round;
        c.add(pod);
      }
      break;
    }
    case CU_STAR: {
      // white ashlar under blue slate: the bulb on its drum, a steep hip with a needle turret, a flat terrace with a
      // slender corner tower, or the bulb over a taller block beside a lower wing
      sp.base = 26; sp.ridge = 0; sp.cross = false; sp.dormers = false; sp.porchKind = 1; sp.porchP = 50; sp.wings = false;
      const uint32_t glaze = c.domeTint();
      int kind = c.pick(4);
      if (c.wl == 0) kind = 1;
      if (kind == 1) { c.st.roof = RoofShape::Hip; c.st.roofMat = RoofMat::Slate; c.st.roofTint = glaze; c.st.pitch = 4; }
      else { c.st.roof = kind == 0 ? RoofShape::Onion : RoofShape::FlatParapet; c.st.roofMat = RoofMat::Slate; c.st.roofTint = glaze; }
      const int bi = house(c, sp);
      Volume& B = c.v[(size_t)bi];
      const bool east = c.ch(128);
      if (kind == 1 || kind == 2) {
        const int extra = kind == 2 ? 16 : 14;
        // (fix) under a hip the turret is an engaged corner tower standing on the ground, half outside the corner and
        // proud of the facade, so its drum runs from the plinth past the eave (inside the corner the roof's overhang
        // cut it off and it floated on the slope); on a flat terrace it rises from the terrace corner
        const bool hipT = kind == 1;
        const int tr = hipT ? 6 : 7;
        const int tcx = hipT ? (east ? std::min((int)B.x1 - 1, c.W + art::BLDG_PAD_X - tr - 1) : std::max((int)B.x0 + 1, -art::BLDG_PAD_X + tr + 1)) : (east ? B.x1 - 7 : B.x0 + 7);
        const int ty1 = hipT ? std::min(c.H + art::BLDG_PAD_B, (int)B.y1 + 5) : (int)B.y1;
        Volume t = c.rnd(VolRole::Tower, tcx, ty1, tr, B.wallH + extra);
        roofTo(t, kind == 2 ? RoofShape::Spire : RoofShape::Onion, RoofMat::Slate, kind == 2 ? 7 : 4, glaze);
        t.wall = B.wall;
        if (kind == 1) B.wallH = (int16_t)std::max(16, B.wallH - 4);
        c.add(t);
      } else if (kind == 3 && B.x1 - B.x0 >= 44) {
        const int split = B.x0 + (B.x1 - B.x0) * 55 / 100;
        Volume wg = B;
        wg.role = VolRole::Wing; wg.doorHere = false; wg.roof = RoofShape::FlatParapet; wg.wallH = (int16_t)std::max(16, B.wallH - 10);
        wg.y0 = (int16_t)(B.y0 + 6); wg.storeys = 1; wg.dormers = 0; wg.feat = 0;
        if (east) { wg.x0 = (int16_t)split; B.x1 = (int16_t)split; } else { wg.x1 = (int16_t)(B.x1 - (split - B.x0)); B.x0 = (int16_t)wg.x1; }
        B.roof = RoofShape::Onion;
        if (c.dc < B.x0 + 6 || c.dc > B.x1 - 6) { B.doorHere = false; wg.doorHere = true; for (Volume& x : c.v) if (x.role == VolRole::Porch) x.doorHere = false; }
        c.add(wg);
        bool any = false;
        for (const Volume& x : c.v) if (x.doorHere) any = true;
        if (!any) c.v.back().doorHere = true;
      }
      chimneys(c, bi, c.st.chimneys ? 1 : 0, 5);
      break;
    }
    case CU_HEART: default: {
      // timber frame and plaster under thatch, shingle or tile; jetties, cross gables, dormers; the rich build manors
      // with cross wings either side of the hall
      if (c.wl >= 3 && c.W >= 80 && c.H >= 48 && n <= 2) {
        Volume bo = c.vol(VolRole::Body, 14, 0, c.W - 14, c.H - 10, c.wallFor(n, 26), n);
        bo.ridgeNS = false; bo.doorHere = false;
        if (!pitchedRoof(bo.roof)) bo.roof = RoofShape::Hip;
        const int bi = c.add(bo);
        for (int sd = 0; sd < 2; sd++) {
          Volume w = c.vol(VolRole::Wing, sd ? c.W - 22 : 0, 2, sd ? c.W : 22, c.H, c.v[(size_t)bi].wallH, n);
          w.ridgeNS = true; w.roof = RoofShape::Gable; w.wall = c.v[(size_t)bi].wall; w.pitch = (uint8_t)std::max(2, (int)w.pitch);
          if (n >= 2 && (w.wall == WallMat::Timber || w.wall == WallMat::Plaster)) w.feat |= VF_JETTY;
          c.add(w);
        }
        porch(c, bi, c.ch(128) ? 0 : 1, 11, 9);
        chimneys(c, bi, 2);
        return;
      }
      sp.base = c.wl == 0 ? 22 : 26;
      const int bi = house(c, sp);
      chimneys(c, bi, c.st.chimneys);
      break;
    }
  }
}

// ---------------------------------------------------------------- huts, barns, workshops, shops
void hut(Bx& c) {
  if (c.cul == CU_STEPPE) { roundHouse(c, 1, 18, true, 0); return; }
  if (c.cul == CU_SYLVAN) { const int bi = roundHouse(c, 1, 18, false, 0); Volume& B = c.v[(size_t)bi]; B.wall = WallMat::Living; B.roof = RoofShape::Conical; B.roofMat = RoofMat::Bark; return; }
  HouseSpec sp;
  sp.base = 18; sp.wings = false; sp.cross = false; sp.dormers = false; sp.porchP = 0; sp.jetty = false;
  WallMat& wm = c.st.wall;
  if (wm == WallMat::Stone || wm == WallMat::Brick || wm == WallMat::Ashlar || wm == WallMat::Plaster) wm = c.cul == CU_HIGHLAND ? WallMat::Rubble : (c.cul == CU_NONE ? WallMat::Log : WallMat::Wattle);
  RoofMat& rm = c.st.roofMat;
  if (c.cul == CU_IMPERIAL || c.cul == CU_RIVER) rm = RoofMat::ClayTile;
  else if (rm == RoofMat::Slate || rm == RoofMat::ClayTile || rm == RoofMat::Copper || rm == RoofMat::GlazedTile) rm = (c.cul == CU_SUN || c.cul == CU_MARSH) ? RoofMat::Palm : RoofMat::Thatch;
  RoofShape& rs = c.st.roof;
  if (rs == RoofShape::Dome || rs == RoofShape::Onion || rs == RoofShape::Mansard || rs == RoofShape::Stepped || rs == RoofShape::Pagoda)
    rs = rm == RoofMat::Adobe ? RoofShape::FlatParapet : RoofShape::Gable;
  if (rs == RoofShape::Gable && (rm == RoofMat::Thatch || rm == RoofMat::Palm)) c.st.pitch = (uint8_t)std::max(3, (int)c.st.pitch);
  sp.ridge = c.W < 52 ? 1 : 0;
  const int bi = house(c, sp);
  if (c.cul == CU_MARSH) stilts(c, 6);
  if (c.r.facts.hearth && (c.st.smoke || (c.r.seed & 4))) chimneys(c, bi, 1, 5);
}

void farm(Bx& c) {
  // the barn in the local building culture: red board-and-batten under a gambrel in the farmlands, mud brick in the
  // desert, logs in the north, the culture's own walls elsewhere; the granary raised on staddles, the sawmill an open shed
  const bool sawmill = c.P == Building::Sawmill, granary = c.P == Building::Granary;
  if (c.cul == CU_STEPPE && !sawmill) {   // a felt store tent and a smaller one
    roundHouse(c, 1, 18, true, c.W >= 64 ? 1 : 0);
    return;
  }
  Volume bo = c.vol(VolRole::Body, 0, c.wl == 0 ? 3 : 0, c.W, c.H, c.wallFor(1, 28), c.want);
  bo.doorHere = true;
  bo.face = sawmill ? Face::Colonnade : Face::BarnDoor;
  const bool classicBarn = c.cul == CU_NONE || c.cul == CU_HEART || c.cul == CU_HIGHLAND || c.cul == CU_RIVER;
  if (bo.wall == WallMat::Adobe) { /* mud brick */ }
  else if (bo.wall == WallMat::Log) { /* logs */ }
  else if (classicBarn) { bo.wall = WallMat::Plank; bo.feat |= VF_BARNBOARDS; bo.wallTint = RGB(156, 52, 46); }
  if (pitchedRoof(bo.roof)) {
    bo.ridgeNS = classicBarn || c.ch(128);
    bo.roof = RoofShape::Gable;
    if (classicBarn) bo.feat |= VF_GAMBREL;
    bo.pitch = (uint8_t)std::max(2, (int)bo.pitch);
  }
  if (sawmill) { bo.roof = pitchedRoof(bo.roof) ? RoofShape::Gable : RoofShape::FlatParapet; bo.feat &= ~(uint32_t)VF_GAMBREL; bo.ridgeNS = false; bo.wallH = (int16_t)std::max(18, bo.wallH - 6); }
  const int bi = c.add(bo);
  if (granary && c.cul != CU_DUNE && c.cul != CU_SUN) stilts(c, 4);
  // a lean-to along one side (a cart shed) on wide plots
  if (c.W >= 80 && c.ch(140)) {
    const bool east = c.ch(128);
    const Volume B = c.v[(size_t)bi];
    if ((east && c.dc < B.x1 - 30) || (!east && c.dc > B.x0 + 30)) {
      Volume& Bm = c.v[(size_t)bi];
      if (east) Bm.x1 = (int16_t)(Bm.x1 - 20); else Bm.x0 = (int16_t)(Bm.x0 + 20);
      const Volume B2 = c.v[(size_t)bi];
      Volume lt = c.vol(VolRole::Annex, east ? B2.x1 : B2.x0 - 20, B2.y0 + 12, east ? B2.x1 + 20 : B2.x0, B2.y1, 13);
      lt.roof = pitchedRoof(B2.roof) ? RoofShape::Hip : RoofShape::FlatParapet; lt.pitch = 0; lt.ridgeNS = false; lt.wall = B2.wall; lt.feat = B2.feat & VF_BARNBOARDS;
      lt.wallTint = B2.wallTint; lt.face = Face::Colonnade;
      c.add(lt);
    }
  }
}

void shop(Bx& c) {
  if (c.cul == CU_STEPPE && c.want == 1) {   // a trader's yurt with an awning before the door
    roundHouse(c, 1, 20, true, c.W >= 64 ? 1 : 0);
    return;
  }
  HouseSpec sp;
  sp.base = 30; sp.face = Face::Shopfront; sp.porchP = 0; sp.cross = c.W >= 80; sp.wings = c.W >= 72;
  if (c.cul == CU_RIVER) sp.ridge = 1;
  if (c.cul == CU_STEPPE) {   // a two-storey round trading hall
    const int bi = roundHouse(c, c.want, 22, true, 0);
    c.v[(size_t)bi].face = Face::Shopfront;
    c.v[(size_t)bi].wall = WallMat::Plank;
    return;
  }
  if (c.cul == CU_SUN) { c.st.roof = RoofShape::FlatParapet; c.st.roofMat = RoofMat::Adobe; }
  if (c.cul == CU_SYLVAN) { c.st.roof = RoofShape::Sweep; if (c.st.roofMat != RoofMat::Bark) c.st.roofMat = RoofMat::Leaf; }
  const int bi = house(c, sp);
  if (c.cul == CU_RIVER && pitchedRoof(c.v[(size_t)bi].roof)) { Volume& B = c.v[(size_t)bi]; B.ridgeNS = true; B.roof = RoofShape::Gable; B.gable = (uint8_t)(1 + c.pick(3)); B.pitch = (uint8_t)std::max(3, (int)B.pitch); }
  if (c.cul == CU_MARSH) stilts(c, 6);
  if (c.cul == CU_JADE && c.wl >= 2) platformUnder(c, 3, WallMat::Ashlar, true);
  chimneys(c, bi, (c.P == Building::Bakery) ? 1 : (c.st.chimneys ? 1 : 0), c.P == Building::Bakery ? 7 : 6);
}

void smithy(Bx& c) {
  // the forge: a sturdy body and an open forge bay under a lean-to on one side, the forge's stack at the junction
  const bool east = c.ch(160);
  const int bw = std::min(28, c.W / 2 - 4);
  int mx0 = 0, mx1 = c.W;
  if (east) mx1 = c.W - bw; else mx0 = bw;
  if (east && c.dc + 10 > mx1) { mx1 = c.W; }
  if (!east && c.dc - 10 < mx0) { mx0 = 0; }
  const bool bay = mx1 - mx0 < c.W;
  Volume bo = c.vol(VolRole::Body, mx0, c.wl == 0 ? 3 : 0, mx1, c.H, c.wallFor(c.want, 26), c.want);
  bo.doorHere = true;
  if (bo.wall == WallMat::Timber || bo.wall == WallMat::Plaster || bo.wall == WallMat::Felt || bo.wall == WallMat::Wattle)
    bo.wall = c.cul == CU_HIGHLAND ? WallMat::Rubble : (c.cul == CU_DUNE || c.cul == CU_STEPPE || c.cul == CU_SUN ? WallMat::Adobe : (c.cul == CU_RIVER ? WallMat::Brick : (c.cul == CU_SYLVAN ? WallMat::Living : WallMat::Stone)));
  if (bo.roofMat == RoofMat::Felt) { bo.roof = RoofShape::FlatParapet; bo.roofMat = RoofMat::Adobe; }
  if (flatRoof(bo.roof)) { bo.roof = RoofShape::FlatParapet; bo.wallH = (int16_t)(bo.wallH + 4); }
  bo.ridgeNS = pitchedRoof(bo.roof) && (mx1 - mx0) <= c.H + 16 && c.ch(128);
  const int bi = c.add(bo);
  if (bay) {
    Volume fb = c.vol(VolRole::Annex, east ? mx1 : 0, 10, east ? c.W : mx0, c.H, std::max(16, bo.wallH - 6));
    fb.face = Face::ForgeBay;
    fb.roof = pitchedRoof(bo.roof) ? RoofShape::Gable : RoofShape::FlatParapet;
    fb.ridgeNS = false; fb.pitch = 1;
    fb.wall = bo.wall == WallMat::Adobe ? WallMat::Adobe : WallMat::Timber;
    if (fb.roof == RoofShape::FlatParapet) fb.roofMat = RoofMat::Adobe;
    c.add(fb);
    if (c.r.facts.hearth) {   // the forge stack over the bay's junction
      const int x = east ? mx1 + 2 : mx0 - 10;
      Volume ch = c.vol(VolRole::Chimney, x, 6, x + 8, 10, 9);
      ch.wall = bo.wall == WallMat::Adobe ? WallMat::Adobe : WallMat::Stone;
      ch.roof = RoofShape::FlatParapet;
      c.add(ch);
    }
  } else chimneys(c, bi, 1, 8);
  if (c.P == Building::Smelter && c.r.facts.hearth) {   // the smelter's tall furnace stack
    const Volume B = c.v[(size_t)bi];
    const int x = c.ch(128) ? B.x0 + 4 : B.x1 - 13;
    Volume ch = c.vol(VolRole::Chimney, x, B.y0 + 4, x + 9, B.y0 + 10, 14);
    ch.wall = WallMat::Stone; ch.roof = RoofShape::FlatParapet;
    c.add(ch);
  }
}

// ---------------------------------------------------------------- inns, tea houses, gathering halls
void inn(Bx& c) {
  const int n = c.want;
  switch (c.cul) {
    case CU_DUNE:
      if (c.W >= 80 && c.H >= 48) {   // a caravanserai: ranges round a court, an iwan gate
        const int bi = courtHouse(c, 22, true, n);
        (void)bi;
        for (Volume& x : c.v) if (x.doorHere) { x.face = Face::Iwan; x.wallH = (int16_t)(x.wallH + 6); }
        return;
      }
      break;
    case CU_STEPPE: {   // (fix) a great guest yurt, its sleeping yurts round it (one storey: wealth is in the cluster)
      (void)n;
      roundHouse(c, 1, 17, true, c.W >= 80 ? 2 : (c.W >= 48 ? 1 : 0));
      c.v[0].ornament |= art::ORN_LANTERNS;
      return;
    }
    case CU_SYLVAN: {
      const int bi = roundHouse(c, n, 22, false, c.W >= 64 ? 1 : 0);
      for (Volume& x : c.v) { x.wall = WallMat::Living; x.roof = RoofShape::Sweep; if (x.roofMat != RoofMat::Bark) x.roofMat = RoofMat::Leaf; x.window = WindowShape::Round; }
      c.v[(size_t)bi].ornament |= art::ORN_VINES | art::ORN_LANTERNS;
      return;
    }
    default: break;
  }
  HouseSpec sp;
  sp.base = 28; sp.porchP = 60; sp.wings = c.W >= 64;
  if (c.cul == CU_FJORD || c.cul == CU_MARSH) { sp.ridge = 0; sp.cross = false; sp.dormers = c.cul == CU_FJORD; }
  if (c.cul == CU_RIVER) sp.ridge = 1;
  if (c.cul == CU_IMPERIAL) sp.porchKind = 1;
  if (c.cul == CU_JADE) { sp.porchKind = 2; sp.porchP = 160; sp.ridge = 0; sp.cross = false; sp.dormers = false; }
  if (c.cul == CU_SUN) { c.st.roof = RoofShape::FlatParapet; c.st.roofMat = RoofMat::Adobe; sp.porchKind = 3; }
  const int bi = house(c, sp);
  Volume& B = c.v[(size_t)bi];
  B.feat |= VF_LAMPS;
  if (c.cul == CU_FJORD && (B.wall == WallMat::Log || B.wall == WallMat::Plank)) B.feat |= VF_JETTY | VF_CARVED;
  if (c.cul == CU_JADE && pitchedRoof(B.roof)) { B.roof = RoofShape::Pagoda; B.pitch = 2; if (B.roofMat == RoofMat::Thatch) B.roofMat = RoofMat::ClayTile; B.feat |= VF_LANTERNS; }
  if (c.cul == CU_RIVER && pitchedRoof(B.roof)) { B.ridgeNS = true; B.roof = RoofShape::Gable; B.gable = (uint8_t)(1 + c.pick(3)); B.pitch = (uint8_t)std::max(3, (int)B.pitch); }
  chimneys(c, bi, std::max(1, (int)c.st.chimneys));
  if (c.cul == CU_MARSH) stilts(c, 6);
  if (c.cul == CU_JADE) platformUnder(c, 3, WallMat::Ashlar, true);
}

void teaHouse(Bx& c) {
  const int n = c.want;
  if (c.cul == CU_STEPPE) { roundHouse(c, n, 22, true, c.W >= 64 ? 1 : 0); c.v[0].ornament |= art::ORN_LANTERNS; return; }
  // a pavilion of two storeys under sweeping eaves, a veranda all round the ground floor, lanterns
  Volume bo = c.vol(VolRole::Body, 6, 2, c.W - 6, c.H - 8, c.wallFor(n, 24), n);
  if (c.cul == CU_JADE || c.cul == CU_IMPERIAL || c.cul == CU_RIVER) roofTo(bo, RoofShape::Pagoda, bo.roofMat == RoofMat::Thatch ? RoofMat::ClayTile : bo.roofMat, 2, bo.roofTint);
  // (fixer r2) the starspire's own: a white ashlar pavilion under a blue-slate hip with an onion lantern on its ridge
  // (its city of white stone and onion domes, not a jade pagoda)
  if (c.cul == CU_STAR) { bo.wall = WallMat::Ashlar; roofTo(bo, RoofShape::Hip, RoofMat::Slate, 3, c.domeTint()); bo.window = WindowShape::Arched; }
  if (flatRoof(bo.roof)) { bo.roof = c.cul == CU_DUNE ? RoofShape::Dome : RoofShape::Hip; bo.roofMat = c.cul == CU_DUNE ? RoofMat::GlazedTile : RoofMat::ClayTile; bo.roofTint = c.domeTint(); }
  bo.ridgeNS = false;
  bo.feat |= VF_LANTERNS;
  bo.ornament |= art::ORN_LANTERNS;
  const int bi = c.add(bo);
  Volume ver = c.vol(VolRole::Porch, 0, c.H - 12, c.W, c.H + 2, 15);
  roofTo(ver, bo.roof == RoofShape::Pagoda ? RoofShape::Pagoda : RoofShape::Hip, bo.roofMat == RoofMat::GlazedTile && bo.roof == RoofShape::Dome ? RoofMat::ClayTile : bo.roofMat, 1, bo.roofTint);
  ver.face = Face::Veranda; ver.doorHere = true; ver.ridgeNS = false;
  c.add(ver);
  if (c.cul == CU_STAR && bo.x1 - bo.x0 >= 40) {   // the onion lantern on the ridge
    const Volume B = c.v[(size_t)bi];
    Volume cu = c.rnd(VolRole::Drum, (B.x0 + B.x1) / 2, (B.y0 + B.y1) / 2 + 5, 5, 7);
    cu.z0 = (int16_t)(roofZAt(B, cu.y1) - 3);
    roofTo(cu, RoofShape::Onion, RoofMat::Slate, 0, c.domeTint()); cu.wall = WallMat::Ashlar; cu.face = Face::Vents;
    c.add(cu);
  }
  chimneys(c, bi, 1);
  if (c.cul == CU_JADE) platformUnder(c, 3, WallMat::Ashlar, true);
  if (c.cul == CU_MARSH) stilts(c, 6);
}

// a great hall (mead hall, lodge, moot hall): long, a carved porch, side annexes; the steppe's is a feasting tent
void greatHall(Bx& c, bool council) {
  const int n = c.want;
  if (c.cul == CU_STEPPE) {
    if (council || n >= 2) { roundHouse(c, n, 24, true, c.W >= 80 ? 2 : (c.W >= 48 ? 1 : 0)); c.v[0].ornament |= art::ORN_PRAYER_FLAGS; return; }
    // the feasting tent: a great ridge tent of canvas, guy ropes, a smaller tent either side
    Volume bo = c.vol(VolRole::Body, 6, 4, c.W - 6, c.H - 2, c.wallFor(1, 20), 1);
    roofTo(bo, RoofShape::Tent, RoofMat::Felt, 2, c.st.roofTint);
    bo.wall = WallMat::Felt; bo.feat |= VF_ROPES; bo.doorHere = true; bo.door = DoorShape::Flap; bo.ridgeNS = false;
    c.add(bo);
    if (c.W >= 80) for (int sd = 0; sd < 2; sd++) {
      Volume t = c.rnd(VolRole::Tent, sd ? c.W - 2 : 2, c.H - 4, 9, 12);
      roofTo(t, RoofShape::Tent, RoofMat::Felt, 2, c.st.accentTint ? lerpC(c.st.roofTint, c.st.accentTint, 30) : c.st.roofTint); t.wall = WallMat::Felt; t.feat |= VF_ROPES;
      c.add(t);
    }
    return;
  }
  if (c.cul == CU_SYLVAN || (council && (c.cul == CU_DUNE || c.cul == CU_IMPERIAL))) {
    if (c.cul == CU_SYLVAN) {   // a round hall of living wood round a sacred tree
      const int bi = roundHouse(c, n, 26, false, c.W >= 80 ? 1 : 0);
      for (Volume& x : c.v) { x.wall = WallMat::Living; x.roof = RoofShape::Sweep; if (x.roofMat != RoofMat::Bark) x.roofMat = RoofMat::Leaf; x.window = WindowShape::Round; x.door = DoorShape::Round; }
      c.v[(size_t)bi].ornament |= art::ORN_VINES;
      return;
    }
    // a domed council chamber with a portico
    Volume bo = c.vol(VolRole::Body, 4, 0, c.W - 4, c.H - 10, c.wallFor(n, 28), n);
    bo.wall = c.monoWall(); roofTo(bo, RoofShape::Dome, c.cul == CU_DUNE ? RoofMat::GlazedTile : RoofMat::Copper, 0, c.domeTint());
    const int bi = c.add(bo);
    porch(c, bi, c.cul == CU_IMPERIAL ? 1 : 3, 16, 11);
    return;
  }
  if (c.cul == CU_STAR) {   // a white hall under a steep blue hip, a needle spire over the door
    Volume bo = c.vol(VolRole::Body, 2, 4, c.W - 2, c.H, c.wallFor(n, 28), n);
    bo.wall = WallMat::Ashlar; roofTo(bo, RoofShape::Hip, RoofMat::Slate, 3, c.domeTint()); bo.doorHere = true;
    const int bi = c.add(bo);
    Volume t = c.vol(VolRole::Tower, c.dc - 8, c.H - 18, c.dc + 8, c.H - 2, c.v[(size_t)bi].wallH + 12);
    roofTo(t, RoofShape::Spire, RoofMat::Slate, 7, c.domeTint()); t.wall = WallMat::Ashlar; t.face = Face::Vents;
    c.add(t);
    return;
  }
  if (c.cul == CU_SUN) {   // a flat-roofed hall on a platform behind a crest of stepped merlons
    Volume bo = c.vol(VolRole::Body, 4, 2, c.W - 4, c.H - 2, c.wallFor(n, 26) + 4, n);
    bo.wall = WallMat::Adobe; bo.wallTint = c.limeTint(); flat(bo, true); bo.doorHere = true; bo.ornament |= art::ORN_PAINTED_BANDS;
    c.add(bo);
    platformUnder(c, 5, WallMat::Ashlar, true);
    return;
  }
  const int bi = longHall(c, c.cul == CU_FJORD ? 24 : 26, n, c.wl >= 2 ? 0 : 3);
  Volume& B = c.v[(size_t)bi];
  if (c.cul == CU_MARSH) { B.roof = RoofShape::Steep; B.pitch = 4; B.feat |= VF_CARVED; }
  if (c.cul == CU_JADE) { roofTo(B, RoofShape::Pagoda, B.roofMat == RoofMat::Thatch ? RoofMat::ClayTile : B.roofMat, 2, B.roofTint); }
  if (c.cul == CU_FJORD) { B.feat |= VF_CARVED; B.ornament |= art::ORN_DRAGON_HEADS | art::ORN_CARVED_RIDGE; if (B.roof == RoofShape::Gable) B.roof = c.st.roof == RoofShape::Turf ? RoofShape::Turf : RoofShape::Steep; }
  if (council) B.feat |= VF_BANNERS;
  // a carved porch over the door, side annexes on wide plots
  if (B.y1 >= c.H - 2 && B.y1 - B.y0 >= 36) { B.y1 = (int16_t)(B.y1 - 8); porch(c, bi, c.cul == CU_IMPERIAL || c.cul == CU_RIVER ? 1 : 0, 10, 9); }
  if (c.W >= 96 && c.ch(170)) {
    const Volume Bc = c.v[(size_t)bi];
    const bool east = c.ch(128);
    if ((east && c.dc < Bc.x1 - 30) || (!east && c.dc > Bc.x0 + 30)) {
      Volume an = c.vol(VolRole::Annex, east ? Bc.x1 - 22 : Bc.x0, Bc.y1 - 2, east ? Bc.x1 : Bc.x0 + 22, std::min(c.H, Bc.y1 + 8), std::max(14, Bc.wallH - 8));
      an.roof = pitchedRoof(Bc.roof) ? RoofShape::Hip : RoofShape::FlatParapet; an.pitch = 1; an.wall = Bc.wall;
      if (an.roofMat == RoofMat::Turf) an.roof = RoofShape::Turf;
      c.add(an);
    }
  }
  chimneys(c, bi, council ? 1 : 2);
  if (c.cul == CU_MARSH) stilts(c, 7);
  if (c.cul == CU_JADE) platformUnder(c, 5, WallMat::Ashlar, true);
}

void guildhall(Bx& c) {
  const int n = c.want;
  // a hall of the guilds: a wide hall of two storeys under a tall roof, a belfry tower (river: brick under an onion),
  // the guild's banner
  Volume bo = c.vol(VolRole::Body, 2, 2, c.W - 2, c.W >= 64 ? c.H - 6 : c.H, c.wallFor(n, 28), n);
  c.monument(bo);
  if (c.cul == CU_HEART || c.cul == CU_NONE) { if (bo.wall == WallMat::Stone && c.ch(128)) bo.wall = WallMat::Timber; }
  if (c.cul == CU_STEPPE) { roundHouse(c, n, 22, true, c.W >= 48 ? 1 : 0); c.v[0].feat |= VF_BANNERS; return; }
  if (c.cul == CU_SUN || c.cul == CU_DUNE) flat(bo, c.cul == CU_SUN);
  bo.ridgeNS = c.cul == CU_RIVER || (pitchedRoof(bo.roof) && c.ch(90));
  if (c.cul == CU_RIVER && pitchedRoof(bo.roof)) { bo.roof = RoofShape::Gable; bo.gable = 1; bo.pitch = 4; }
  if (flatRoof(bo.roof)) bo.wallH = (int16_t)(bo.wallH + 4);
  bo.doorHere = true; bo.feat |= VF_BANNERS;
  if (c.cul == CU_HEART && (bo.wall == WallMat::Timber || bo.wall == WallMat::Plaster)) bo.feat |= VF_JETTY;
  const int bi = c.add(bo);
  // the belfry: a square tower at a front corner, standing proud of the hall's front, its cap in the culture's idiom
  bool east = c.ch(128);
  const int tw = 16;
  int tx = east ? c.W - 2 - tw : 2;
  if (c.dc + 8 > tx && c.dc - 8 < tx + tw) { east = !east; tx = east ? c.W - 2 - tw : 2; }
  RoofShape cap = RoofShape::Spire;
  RoofMat cm = RoofMat::Slate;
  int pitch = 6;
  uint32_t tint = 0;
  switch (c.cul) {
    case CU_RIVER: cap = RoofShape::Onion; tint = RGB(84, 110, 100); cm = RoofMat::Copper; pitch = 2; break;
    case CU_DUNE: cap = RoofShape::Onion; cm = RoofMat::GlazedTile; tint = c.domeTint(); break;
    case CU_JADE: cap = RoofShape::Pagoda; cm = c.v[(size_t)bi].roofMat; pitch = 3; break;
    case CU_SUN: cap = RoofShape::FlatParapet; break;
    case CU_IMPERIAL: cap = RoofShape::Dome; cm = RoofMat::Copper; break;
    case CU_SYLVAN: cap = RoofShape::Sweep; cm = RoofMat::Leaf; pitch = 4; break;
    case CU_STAR: cap = RoofShape::Spire; tint = c.domeTint(); pitch = 7; break;
    case CU_FJORD: cap = RoofShape::Spire; cm = RoofMat::Shingle; pitch = 6; break;
    case CU_MARSH: cap = RoofShape::Spire; cm = RoofMat::Thatch; pitch = 5; break;
    default: break;
  }
  if (c.W >= 64 && c.cul == CU_DUNE) {   // the dunes: a minaret for the call to the exchange, a dome over the hall
    Volume mi = c.rnd(VolRole::Tower, east ? c.W - 8 : 8, c.H, 6, c.v[(size_t)bi].wallH + 30);
    roofTo(mi, RoofShape::Onion, RoofMat::GlazedTile, 0, c.domeTint()); mi.wall = WallMat::Adobe; mi.feat |= VF_BALCONY; mi.face = Face::Blank;
    c.add(mi);
    const Volume B = c.v[(size_t)bi];
    const int dw = std::min(B.x1 - B.x0 - 8, 40);
    Volume dm = c.vol(VolRole::Drum, (B.x0 + B.x1) / 2 - dw / 2, B.y0, (B.x0 + B.x1) / 2 + dw / 2, B.y0 + std::min(dw, B.y1 - B.y0), 5);
    dm.z0 = (int16_t)(B.z0 + B.wallH - 4); roofTo(dm, RoofShape::Dome, RoofMat::GlazedTile, 0, c.domeTint()); dm.face = Face::Blank; dm.wall = WallMat::Adobe;
    c.add(dm);
  } else if (c.W >= 64) {
    // tall enough to clear the roof it stands before (the hall's ridge stands further back, so higher in the view)
    const int t = towerVol(c, VolRole::Tower, false, tx, c.H, tw, c.v[(size_t)bi].wallH + roofHeight(c.v[(size_t)bi]) * 3 / 4 + 10, cap, cm, pitch, tint);
    c.v[(size_t)t].wall = c.v[(size_t)bi].wall; c.v[(size_t)t].face = Face::Vents; c.v[(size_t)t].feat |= VF_BELL;
    if (cap == RoofShape::FlatParapet) c.v[(size_t)t].feat |= VF_CRENELS;
  }
  chimneys(c, bi, 1);
  if (c.cul == CU_JADE) platformUnder(c, 4, WallMat::Ashlar, true);
  if (c.cul == CU_MARSH) stilts(c, 6);
}

void exchange(Bx& c) {
  const int n = c.want;
  // the merchants' exchange: an open arcade on the ground floor, the counting rooms above, a cupola or a lantern
  if (c.cul == CU_STEPPE) { roundHouse(c, n, 22, true, c.W >= 64 ? 1 : 0); c.v[0].face = Face::Shopfront; return; }
  // the arcade is the body's own ground floor (an open loggia of arches), the counting rooms over it
  Volume bo = c.vol(VolRole::Body, 2, 2, c.W - 2, c.H, c.wallFor(std::max(2, n), 28) - (n >= 2 ? 0 : 6), n);
  c.monument(bo);
  if (c.cul == CU_SUN || c.cul == CU_DUNE) flat(bo, c.cul == CU_SUN);
  if (flatRoof(bo.roof)) bo.wallH = (int16_t)(bo.wallH + 4);
  bo.ridgeNS = false;
  bo.face = Face::Arcade; bo.doorHere = true;
  if (pitchedRoof(bo.roof) && bo.roof != RoofShape::Pagoda && bo.roof != RoofShape::Sweep) bo.roof = c.cul == CU_RIVER || c.cul == CU_HEART ? RoofShape::Mansard : RoofShape::Hip;
  if (bo.wall == WallMat::Brick) bo.trimTint = RGB(222, 210, 186);
  const int bi = c.add(bo);
  // a cupola / lantern on the ridge
  const Volume B = c.v[(size_t)bi];
  if (B.x1 - B.x0 >= 56) {
    Volume cu = c.rnd(VolRole::Drum, (B.x0 + B.x1) / 2, (B.y0 + B.y1) / 2 + 6, 6, 8);
    cu.z0 = (int16_t)(roofZAt(B, cu.y1) - 3);
    roofTo(cu, c.cul == CU_RIVER || c.cul == CU_DUNE || c.cul == CU_STAR ? RoofShape::Onion : RoofShape::Dome, c.cul == CU_DUNE ? RoofMat::GlazedTile : RoofMat::Copper, 0, c.domeTint());
    cu.wall = B.wall == WallMat::Adobe ? WallMat::Adobe : WallMat::Timber; cu.face = Face::Vents;
    if (flatRoof(B.roof)) cu.z0 = (int16_t)(B.z0 + B.wallH - 2);
    c.add(cu);
  }
  chimneys(c, bi, 1);
  if (c.cul == CU_JADE) platformUnder(c, 4, WallMat::Ashlar, true);
}

void bath(Bx& c) {
  const int n = c.want;
  switch (c.cul) {
    case CU_IMPERIAL: case CU_DUNE: case CU_STAR: case CU_SUN: {
      // the thermae / hammam: a hall under a great dome on its drum, lower domed or vaulted halls either side, the
      // domes studded with glass oculi, steam from the vents
      Volume bo = c.vol(VolRole::Body, c.W / 4, 0, c.W - c.W / 4, c.H - 4, c.wallFor(n, 26) + 4, n);
      bo.wall = c.cul == CU_DUNE ? WallMat::Adobe : (c.cul == CU_SUN ? WallMat::Adobe : WallMat::Ashlar);
      if (c.cul == CU_SUN) bo.wallTint = c.limeTint();
      roofTo(bo, RoofShape::Dome, c.cul == CU_IMPERIAL ? RoofMat::Copper : (c.cul == CU_DUNE ? RoofMat::Adobe : RoofMat::Slate), 0, c.cul == CU_STAR ? c.domeTint() : 0);
      bo.feat |= VF_OCULI | VF_STEAM; bo.doorHere = true;
      const int bi = c.add(bo);
      for (int sd = 0; sd < 2; sd++) {
        const Volume B = c.v[(size_t)bi];
        Volume w = c.vol(VolRole::Wing, sd ? B.x1 : 0, 6, sd ? c.W : B.x0, c.H - 2, std::max(18, B.wallH - 8));
        w.wall = B.wall; w.wallTint = B.wallTint;
        if (w.x1 - w.x0 >= 22) { roofTo(w, RoofShape::Dome, B.roofMat, 0, B.roofTint); w.feat |= VF_OCULI; }
        else flat(w);
        c.add(w);
      }
      if (c.cul == CU_IMPERIAL) { Volume& B = c.v[(size_t)bi]; B.y1 = (int16_t)(B.y1 - 6); porch(c, bi, 1, 12, 9); }
      chimneys(c, bi, 1);
      return;
    }
    case CU_STEPPE: roundHouse(c, n, 20, true, 1); c.v[0].feat |= VF_STEAM; return;
    default: {
      // a bathhouse hall (jade, river, heartland...): a long body, steam vents on its ridge, a veranda
      HouseSpec sp;
      sp.base = 26; sp.ridge = 0; sp.porchKind = c.cul == CU_JADE ? 2 : 0; sp.porchP = 140; sp.dormers = false; sp.cross = false;
      const int bi = house(c, sp);
      Volume& B = c.v[(size_t)bi];
      B.feat |= VF_STEAM;
      if (c.cul == CU_JADE && pitchedRoof(B.roof)) { B.roof = RoofShape::Pagoda; B.pitch = 2; if (B.roofMat == RoofMat::Thatch) B.roofMat = RoofMat::ClayTile; }
      chimneys(c, bi, 1);
      if (c.cul == CU_JADE) platformUnder(c, 3, WallMat::Ashlar, true);
      if (c.cul == CU_MARSH) stilts(c, 6);
      return;
    }
  }
}

// ---------------------------------------------------------------- temples
void temple(Bx& c) {
  const int n = c.want;
  auto nave = [&](int spireTint) {
    // a nave (side-gabled on wide temples so the steeple stands clear) and a tower over the door with a spire
    Volume bo = c.vol(VolRole::Body, 4, 0, c.W - 4, c.H - 6, c.wallFor(n, 30), n);
    c.monument(bo, true);
    bo.roof = RoofShape::Gable; bo.ridgeNS = c.W < 80; bo.pitch = (uint8_t)(bo.ridgeNS ? 2 : 3);
    bo.face = Face::Sacred;
    if (bo.window == WindowShape::Square || bo.window == WindowShape::Slit) bo.window = WindowShape::Arched;
    const int bi = c.add(bo);
    const int tw = c.cul == CU_STAR ? 16 : (c.cul == CU_HIGHLAND ? 18 : 22);
    // the tower stands before the nave: tall enough to clear the nave's roof (its ridge, further back, stands higher in
    // the view)
    const Volume N = c.v[(size_t)bi];
    const int clear = std::max(44, N.wallH + roofHeight(N) * 3 / 4 + 8);
    Volume t = c.vol(VolRole::Tower, c.dc - tw / 2, c.H - 24, c.dc + tw / 2, c.H, std::max(58, clear));
    t.wall = N.wall; t.face = Face::Sacred; t.doorHere = true; t.feat |= VF_BELL;
    t.window = N.window;
    switch (c.cul) {
      case CU_RIVER: roofTo(t, RoofShape::Onion, RoofMat::Copper, 2, RGB(84, 110, 100)); t.wallH = (int16_t)std::max(50, clear + 4); break;
      case CU_HIGHLAND: roofTo(t, RoofShape::Hip, RoofMat::Slate, 5); t.wallH = (int16_t)std::max(44, clear); break;
      case CU_STAR: roofTo(t, RoofShape::Spire, RoofMat::Slate, 8, spireTint ? c.domeTint() : 0); break;
      default: roofTo(t, RoofShape::Spire, c.v[(size_t)bi].roofMat == RoofMat::Shingle ? RoofMat::Shingle : RoofMat::Slate, 7); break;
    }
    c.add(t);
  };
  switch (c.cul) {
    case CU_IMPERIAL: {   // a rotunda: an ashlar drum under a great copper dome, a columned portico before it
      Volume h = c.rnd(VolRole::Body, c.W / 2, c.H - 8, std::min(c.W / 2 - 2, (c.H - 8) / 2 + 6), c.wallFor(n, 30), n);
      h.wall = WallMat::Ashlar; roofTo(h, RoofShape::Dome, RoofMat::Copper, 0, c.st.roofTint && c.st.roofMat == RoofMat::Copper ? c.st.roofTint : 0);
      h.face = Face::Sacred; h.window = WindowShape::Arched;
      const int bi = c.add(h);
      porch(c, bi, 1, std::min(22, c.W / 3), 10);
      return;
    }
    case CU_DUNE: {   // a domed prayer hall, an iwan portal, a slender minaret at one front corner
      Volume h = c.vol(VolRole::Body, 0, 0, c.W, c.H - 4, c.wallFor(n, 30) + 4, n);
      h.wall = WallMat::Adobe; roofTo(h, RoofShape::Dome, RoofMat::GlazedTile, 0, c.domeTint()); h.face = Face::Sacred;
      const int bi = c.add(h);
      Volume iw = c.vol(VolRole::Porch, c.dc - 12, c.H - 10, c.dc + 12, c.H, c.v[(size_t)bi].wallH + 6);
      iw.wall = WallMat::Adobe; iw.face = Face::Iwan; flat(iw); iw.doorHere = true; c.v[(size_t)bi].doorHere = false;
      c.add(iw);
      const bool east = c.ch(128);
      Volume mi = c.rnd(VolRole::Tower, east ? c.W - 7 : 7, c.H - 1, 6, 64);
      roofTo(mi, RoofShape::Onion, RoofMat::GlazedTile, 0, c.domeTint()); mi.wall = WallMat::Adobe; mi.feat |= VF_BALCONY; mi.face = Face::Blank;
      c.add(mi);
      return;
    }
    case CU_STEPPE: {   // the great temple yurt: a wide felt drum under a low cone, its crown ring, prayer flags
      roundHouse(c, n, 24, true, c.W >= 80 ? 2 : (c.W >= 64 ? 1 : 0));
      c.v[0].ornament |= art::ORN_PRAYER_FLAGS;
      c.v[0].face = Face::Windows;
      return;
    }
    case CU_JADE: {   // a pagoda: two or three tiers of glazed roofs with upswept corners, each tier set in, on a platform
      const int tiers = c.W >= 80 ? 3 : 2;
      int x0 = 2, x1 = c.W - 2, y0 = 4, y1 = c.H;
      Volume bo = c.vol(VolRole::Body, x0, y0, x1, y1, 24, n);
      roofTo(bo, RoofShape::Pagoda, c.monoRoof(), 1, c.st.roofMat == RoofMat::GlazedTile ? c.st.roofTint : 0);
      bo.wall = WallMat::Plaster; bo.doorHere = true; bo.face = Face::Windows; bo.window = WindowShape::Lattice;
      c.add(bo);
      int zb = 24 + roofHeight(c.v[0]) * 55 / 100;
      for (int k = 1; k < tiers; k++) {
        x0 += 12; x1 -= 12; y0 += 6; y1 -= 10;
        if (x1 - x0 < 20 || y1 - y0 < 10) break;
        Volume t = c.vol(VolRole::Tier, x0, y0, x1, y1, 15);
        t.z0 = (int16_t)zb;
        roofTo(t, RoofShape::Pagoda, c.v[0].roofMat, 1, c.v[0].roofTint);
        t.wall = WallMat::Plaster; t.window = WindowShape::Lattice;
        c.add(t);
        zb += 15 + roofHeight(t) * 45 / 100;
      }
      platformUnder(c, 6, WallMat::Ashlar, true, 4);
      return;
    }
    case CU_SUN: {   // a stepped pyramid: a doorway into its foot, the great stair up its front, a shrine on top
      Volume py = c.vol(VolRole::Body, 0, 0, c.W, c.H, 16, n);
      py.wall = WallMat::Adobe; py.wallTint = c.limeTint();
      roofTo(py, RoofShape::Stepped, RoofMat::Adobe, 3, lerpC(c.limeTint(), RGB(186, 150, 104), 55));
      py.feat |= VF_STAIR; py.doorHere = true; py.face = Face::Blank;   // (fix) the foot is the first riser, not a house front
      const int bi = c.add(py);
      const int sw = std::max(18, c.W * 34 / 100), sd = std::max(10, c.H * 3 / 10);
      Volume sh = c.vol(VolRole::Tier, c.W / 2 - sw / 2, c.H / 2 - sd / 2 - 2, c.W / 2 + sw / 2, c.H / 2 + sd / 2 - 2, 12);
      sh.z0 = (int16_t)(16 + roofHeight(c.v[(size_t)bi]));
      flat(sh, true); sh.wall = WallMat::Adobe; sh.wallTint = c.limeTint(); sh.feat |= VF_DOOR; sh.ornament |= art::ORN_PAINTED_BANDS;
      c.add(sh);
      return;
    }
    case CU_SYLVAN: {   // a tree shrine: a round hall of living wood under a leaf sweep, the sacred tree rising through it
      Volume tr = c.rnd(VolRole::Tree, c.W / 2, c.H / 2 + 6, 7, 40);
      tr.wall = WallMat::Living;
      Volume h = c.rnd(VolRole::Body, c.W / 2, c.H, std::min(c.W / 2 - 2, c.H / 2 + 4), c.wallFor(n, 26), n);
      h.wall = WallMat::Living; roofTo(h, RoofShape::Sweep, RoofMat::Leaf, 2, c.st.roofTint); h.doorHere = true; h.door = DoorShape::Round; h.window = WindowShape::Round;
      c.add(h);
      c.add(tr);
      return;
    }
    case CU_MARSH: {   // a stilt shrine: a tall steep thatch over a small hall on a raised deck, carved ridge ends
      Volume bo = c.vol(VolRole::Body, 6, 4, c.W - 6, c.H - 6, c.wallFor(n, 22), n);
      roofTo(bo, RoofShape::Steep, RoofMat::Thatch, 4); bo.ridgeNS = true; bo.feat |= VF_CARVED; bo.wall = WallMat::Plank; bo.face = Face::Sacred;
      bo.ornament |= art::ORN_CARVED_RIDGE | art::ORN_LANTERNS;
      const int bi = c.add(bo);
      porch(c, bi, 2, 0, 6);
      stilts(c, 8);
      return;
    }
    case CU_FJORD: {   // a stave church: a low arcaded gallery round the foot, the nave's steep shingle roof, an upper
                       // stage on its ridge, a slender spire over the crossing, dragon heads on every ridge end
      // (fixer round 3) the tiers read as tiers: the nave's stave wall stands a band above the gallery's roof, its roof
      // a little less steep, the upper stage on the ridge itself (its walls rise clear of the nave's roof all round),
      // the spire on the upper stage's roof. Under snow the roofs go white, and the wall bands between them are what
      // still show the tiered stave church (a pile of white roofs read as one featureless block)
      Volume bo = c.vol(VolRole::Body, 10, 6, c.W - 10, c.H - 10, c.wallFor(n, 34), n);
      roofTo(bo, RoofShape::Gable, RoofMat::Shingle, 3, c.st.roofMat == RoofMat::Shingle ? c.st.roofTint : 0); bo.wall = WallMat::Plank;
      bo.ornament |= art::ORN_DRAGON_HEADS; bo.face = Face::Sacred; bo.window = WindowShape::Round; bo.ridgeNS = false;
      const int bi = c.add(bo);
      const Volume B = c.v[(size_t)bi];
      Volume ga = c.vol(VolRole::Porch, 4, c.H - 12, c.W - 4, c.H, 17);   // (fixer r2) tall enough that its arches show under its eave
      roofTo(ga, RoofShape::Hip, RoofMat::Shingle, 1, B.roofTint); ga.wall = WallMat::Plank; ga.face = Face::Arcade; ga.doorHere = true;
      ga.ornament = art::ORN_CARVED_RIDGE;
      c.add(ga);
      const int my = (B.y0 + B.y1) / 2;
      Volume t1 = c.vol(VolRole::Tier, B.x0 + 12, my - 7, B.x1 - 12, my + 7, 12);
      int spZ = roofZAt(B, my + 6) + 2;
      if (t1.x1 - t1.x0 >= 20 && t1.y1 - t1.y0 >= 8) {
        t1.z0 = (int16_t)(roofZAt(B, t1.y1) - 1);
        roofTo(t1, RoofShape::Steep, RoofMat::Shingle, 3, B.roofTint); t1.wall = WallMat::Plank; t1.ornament = art::ORN_DRAGON_HEADS; t1.window = WindowShape::Round;
        c.add(t1);
        spZ = roofZAt(t1, my + 5) + 1;
      }
      Volume sp = c.vol(VolRole::Tower, c.dc - 6, my - 6, c.dc + 6, my + 6, 12);
      sp.z0 = (int16_t)spZ; roofTo(sp, RoofShape::Spire, RoofMat::Shingle, 7, B.roofTint); sp.wall = WallMat::Plank; sp.face = Face::Vents;
      c.add(sp);
      return;
    }
    case CU_STAR: nave(1); return;
    default: nave(0); return;
  }
}

// ---------------------------------------------------------------- the mage tower
void mageTower(Bx& c) {
  const int n = std::max(2, c.want);
  const int rad = std::min(20, std::min(c.W, c.H) / 2 - 1);
  bool round = true;
  RoofShape cap = RoofShape::Conical;
  RoofMat cm = RoofMat::Slate;
  int pitch = 4;
  uint32_t tint = RGB(122, 92, 170);
  WallMat wall = c.monoWall();
  switch (c.cul) {
    case CU_DUNE: cap = RoofShape::Onion; cm = RoofMat::GlazedTile; tint = c.domeTint(); break;
    case CU_RIVER: cap = RoofShape::Onion; cm = RoofMat::Copper; tint = RGB(84, 110, 100); break;
    case CU_STAR: cap = RoofShape::Spire; tint = c.domeTint(); pitch = 7; break;
    case CU_JADE: round = false; cap = RoofShape::Pagoda; cm = c.monoRoof(); tint = c.st.roofMat == RoofMat::GlazedTile ? c.st.roofTint : 0; pitch = 2; break;
    case CU_SYLVAN: cap = RoofShape::Sweep; cm = RoofMat::Leaf; tint = c.st.roofTint; pitch = 4; break;
    case CU_IMPERIAL: cap = RoofShape::Dome; cm = RoofMat::Copper; tint = 0; break;
    case CU_FJORD: round = false; cap = RoofShape::Spire; cm = RoofMat::Shingle; tint = 0; pitch = 6; wall = WallMat::Log; break;
    case CU_MARSH: cap = RoofShape::Spire; cm = RoofMat::Thatch; tint = 0; pitch = 5; wall = WallMat::Plank; break;
    case CU_SUN: round = false; cap = RoofShape::FlatParapet; break;
    case CU_STEPPE: cap = RoofShape::Dome; cm = RoofMat::GlazedTile; tint = RGB(58, 150, 160); wall = WallMat::Brick; break;   // (fix) a fired-brick tower, its glazed dome
    case CU_HIGHLAND: cap = RoofShape::FlatParapet; break;
    default: break;
  }
  Volume t = round ? c.rnd(VolRole::Body, c.W / 2, c.H - 1, rad, 62 + c.pick(10), n) : c.vol(VolRole::Body, c.W / 2 - rad + 2, c.H - 2 * rad + 4, c.W / 2 + rad - 2, c.H, 60 + c.pick(8), n);
  roofTo(t, cap, cm, pitch, tint);
  t.wall = wall; t.face = Face::Arcane; t.doorHere = true; t.eave = 0;
  if (cap == RoofShape::FlatParapet) { t.feat |= VF_CRENELS; t.roofMat = RoofMat::Adobe; }
  if (c.cul == CU_SUN) { t.wallTint = c.limeTint(); t.wall = WallMat::Adobe; }
  if (c.cul == CU_JADE) t.wall = WallMat::Plaster;
  c.add(t);
  // low annexes either side on a wide plot
  if (c.W - 2 * rad > 20) {
    Volume a = c.vol(VolRole::Annex, 0, c.H - 26, c.W, c.H - 4, 20);
    a.wall = wall; a.roof = flatRoof(c.st.roof) ? RoofShape::FlatParapet : RoofShape::Hip; a.face = Face::Windows;
    if (a.roof == RoofShape::FlatParapet) a.roofMat = RoofMat::Adobe;
    c.add(a);
  }
}

// ---------------------------------------------------------------- mills
void windmill(Bx& c) {
  // a tower mill: a round tower of whitewashed rubble (or the region's stone) under a boarded or thatched cap; the sails
  // turn on the cap's front (the painter's machinery)
  const int rad = std::min(std::min(c.W, c.H) / 2 - 4, 17);
  const int cx = std::clamp(c.dc, rad + 3, c.W - rad - 3);
  Volume t = c.rnd(VolRole::Body, cx, c.H - 1, rad, 54 + c.pick(6), std::max(2, c.want));
  const bool white = c.st.wall != WallMat::Stone && c.st.wall != WallMat::Brick && c.st.wall != WallMat::Adobe && c.st.wall != WallMat::Rubble && c.st.wall != WallMat::Ashlar;
  t.wall = white ? WallMat::Plaster : c.st.wall;
  t.wallTint = white ? RGB(228, 212, 180) : 0;
  const bool thatch = c.st.roofMat == RoofMat::Thatch || c.st.roofMat == RoofMat::Turf || c.st.roofMat == RoofMat::Palm;
  roofTo(t, RoofShape::Conical, thatch ? RoofMat::Thatch : RoofMat::Shingle, 2);
  t.eave = 0; t.face = Face::Blank; t.doorHere = true;
  c.add(t);
}
void watermill(Bx& c) {
  HouseSpec sp;
  sp.base = 28; sp.porchP = 0; sp.wings = false; sp.cross = false; sp.dormers = false;
  if (weakWall(c.st.wall)) c.st.wall = c.monoWall();
  if (c.st.roofMat == RoofMat::Felt) { c.st.roof = RoofShape::Gable; c.st.roofMat = RoofMat::Shingle; }
  if (c.cul == CU_STEPPE) { c.st.wall = WallMat::Adobe; c.st.roof = RoofShape::FlatParapet; c.st.roofMat = RoofMat::Adobe; }
  const int bi = house(c, sp);
  c.v[(size_t)bi].face = Face::Windows;
}

// ---------------------------------------------------------------- barracks
void barracks(Bx& c) {
  const int n = c.want;
  switch (c.cul) {
    case CU_FJORD: case CU_MARSH: case CU_SYLVAN: case CU_STEPPE: greatHall(c, false); c.v[0].feat |= VF_BANNERS; return;
    case CU_IMPERIAL: {
      // (M3b fixer) a castrum's barrack block: one long range under a hipped tile roof with a pedimented portico on its
      // door and a square stair tower at one end under its own hip. (The court of ranges with corner hips on a 7x4 plot
      // left a court one row deep: it read as facades stacked on facades, windows cut by the corner roofs.)
      Volume bo = c.vol(VolRole::Body, 0, 0, c.W, c.H, c.wallFor(n, 26), n);
      c.monument(bo);
      roofTo(bo, RoofShape::Hip, RoofMat::ClayTile, 2, bo.roofTint);
      bo.doorHere = true; bo.feat |= VF_BANNERS;
      bo.ridgeNS = false;
      const int bi = c.add(bo);
      if (c.W >= 64) {
        const bool east = (c.r.seed >> 5) & 1;
        Volume& B = c.v[(size_t)bi];
        if (east) B.x1 = (int16_t)(c.W - 20); else B.x0 = 20;
        Volume t = c.vol(VolRole::Tower, east ? c.W - 22 : 0, 2, east ? c.W : 22, c.H, c.wallFor(n, 26) + 14);
        t.wall = B.wall; t.wallTint = B.wallTint; t.trimTint = B.trimTint; t.window = B.window; t.face = Face::Slits;
        roofTo(t, RoofShape::Hip, RoofMat::ClayTile, 2, B.roofTint);
        c.add(t);
      }
      {
        Volume& B = c.v[(size_t)bi];
        if (B.y1 - B.y0 >= 36) { B.y1 = (int16_t)(B.y1 - 6); porch(c, bi, 1, 12, 8); }
      }
      chimneys(c, bi, 1);
      return;
    }
    default: break;
  }
  // the garrison's hall in the culture's monumental material, its watchtower at one end
  Volume bo = c.vol(VolRole::Body, 0, 0, c.W, c.H, c.wallFor(n, 26), n);
  c.monument(bo);
  if (c.cul == CU_DUNE || c.cul == CU_SUN) flat(bo, true);
  if (flatRoof(bo.roof)) bo.wallH = (int16_t)(bo.wallH + 4);
  if (c.cul == CU_SUN) { bo.wallTint = c.limeTint(); bo.wall = WallMat::Adobe; }
  bo.doorHere = true; bo.feat |= VF_BANNERS;
  bo.ridgeNS = false;
  if (c.cul == CU_JADE && pitchedRoof(bo.roof)) roofTo(bo, RoofShape::Pagoda, c.monoRoof(), 2, bo.roofTint);
  const int bi = c.add(bo);
  if (c.W >= 80) {
    const bool east = (c.r.seed >> 5) & 1;
    Volume& B = c.v[(size_t)bi];
    if (east) B.x1 = (int16_t)std::min((int)B.x1, c.W - 26); else B.x0 = (int16_t)std::max((int)B.x0, 26);
    Volume t = c.vol(VolRole::Tower, east ? c.W - 26 : 2, 2, east ? c.W - 2 : 26, c.H - 2, 58);
    t.wall = B.wall; t.wallTint = B.wallTint; t.face = Face::Slits;
    switch (c.cul) {
      case CU_JADE: roofTo(t, RoofShape::Pagoda, B.roofMat, 2, B.roofTint); t.wallH = 44; break;
      case CU_RIVER: roofTo(t, RoofShape::Gable, B.roofMat, 4, B.roofTint); t.ridgeNS = true; t.gable = 1; t.wallH = 44; break;
      case CU_STAR: roofTo(t, RoofShape::Spire, RoofMat::Slate, 6, c.domeTint()); t.wallH = 44; break;
      default: flat(t, true); break;
    }
    c.add(t);
  }
  chimneys(c, bi, 1);
  if (c.cul == CU_SUN) platformUnder(c, 4, WallMat::Ashlar, true);
  if (c.cul == CU_JADE) platformUnder(c, 4, WallMat::Ashlar, true);
}

// ---------------------------------------------------------------- seats of power
void ground(Bx& c);   // (finishing, below)

// the culture's monumental idiom: the walls, roofs, tower caps and enclosure a people builds its great houses in (so
// a seat the society asks of any culture, a guildhall in the dunes or a temple complex on the river, speaks its idiom)
struct Idiom {
  WallMat wall = WallMat::Stone;
  uint32_t wallTint = 0;
  RoofShape roof = RoofShape::Hip;
  RoofMat roofMat = RoofMat::Slate;
  int pitch = 3;
  uint32_t roofTint = 0;
  bool flat = false, crenels = false;
  RoofShape cap = RoofShape::Spire;
  RoofMat capMat = RoofMat::Slate;
  int capPitch = 6;
  uint32_t capTint = 0;
  WallMat encl = WallMat::Stone;
  RoofShape coping = RoofShape::FlatParapet;
  RoofMat copingMat = RoofMat::Adobe;
  uint32_t enclFeat = VF_CRENELS;
  uint32_t enclTint = 0;
  WindowShape window = WindowShape::Arched;
};
Idiom idiomOf(const Bx& c) {
  Idiom d;
  const art::ArchStyle& st = c.st;
  switch (c.cul) {
    case CU_FJORD:
      d.wall = st.wall == WallMat::Plank ? WallMat::Plank : WallMat::Log; d.roof = st.roofMat == RoofMat::Turf ? RoofShape::Turf : RoofShape::Steep;
      d.roofMat = st.roofMat == RoofMat::Turf ? RoofMat::Turf : RoofMat::Shingle; d.pitch = 3; d.roofTint = st.roofMat == RoofMat::Shingle ? st.roofTint : 0;
      d.cap = RoofShape::Spire; d.capMat = RoofMat::Shingle; d.capPitch = 6;
      d.encl = WallMat::Log; d.enclFeat = VF_POINTS; d.window = WindowShape::Slit;
      break;
    case CU_HIGHLAND:
      d.wall = WallMat::Rubble; d.roof = RoofShape::Gable; d.roofMat = RoofMat::Slate; d.pitch = 4;
      d.cap = RoofShape::Conical; d.capMat = RoofMat::Slate; d.capPitch = 4; d.encl = WallMat::Rubble; d.window = WindowShape::Square;
      break;
    case CU_IMPERIAL:
      d.wall = WallMat::Ashlar; d.wallTint = st.wall == WallMat::Ashlar ? st.wallTint : 0; d.roof = RoofShape::Hip; d.roofMat = RoofMat::ClayTile; d.pitch = 1;
      d.roofTint = st.roofMat == RoofMat::ClayTile ? st.roofTint : 0; d.cap = RoofShape::Dome; d.capMat = RoofMat::Copper; d.capPitch = 0;
      d.encl = WallMat::Ashlar; d.coping = RoofShape::Gable; d.copingMat = RoofMat::ClayTile; d.enclFeat = 0; d.window = WindowShape::Tall;
      break;
    case CU_DUNE:
      d.wall = WallMat::Adobe; d.wallTint = st.wall == WallMat::Adobe ? st.wallTint : 0; d.roof = RoofShape::FlatParapet; d.roofMat = RoofMat::Adobe; d.flat = true; d.crenels = true;
      d.cap = RoofShape::Onion; d.capMat = RoofMat::GlazedTile; d.capPitch = 0; d.capTint = c.domeTint(); d.encl = WallMat::Adobe; d.enclTint = d.wallTint;
      d.window = WindowShape::Screen;
      break;
    case CU_STEPPE:
      d.wall = WallMat::Felt; d.roof = RoofShape::Conical; d.roofMat = RoofMat::Felt; d.pitch = 1; d.roofTint = st.roofTint;
      d.cap = RoofShape::Conical; d.capMat = RoofMat::Felt; d.capPitch = 1; d.encl = WallMat::Felt; d.enclFeat = VF_PENNANTS; d.window = WindowShape::Round;
      break;
    case CU_MARSH:
      d.wall = WallMat::Plank; d.roof = RoofShape::Steep; d.roofMat = RoofMat::Thatch; d.pitch = 4;
      d.cap = RoofShape::Spire; d.capMat = RoofMat::Thatch; d.capPitch = 5; d.encl = WallMat::Wattle; d.enclFeat = VF_POINTS; d.window = WindowShape::Square;
      break;
    case CU_JADE:
      d.wall = WallMat::Plaster; d.wallTint = RGB(176, 52, 44); d.roof = RoofShape::Pagoda; d.roofMat = st.roofMat == RoofMat::GlazedTile ? RoofMat::GlazedTile : RoofMat::ClayTile;
      d.pitch = 2; d.roofTint = st.roofMat == RoofMat::GlazedTile ? st.roofTint : RGB(212, 168, 60);
      d.cap = RoofShape::Pagoda; d.capMat = d.roofMat; d.capPitch = 2; d.capTint = d.roofTint;
      d.encl = WallMat::Plaster; d.enclTint = RGB(170, 50, 42); d.coping = RoofShape::Gable; d.copingMat = d.roofMat; d.enclFeat = 0; d.window = WindowShape::Lattice;
      break;
    case CU_RIVER:
      d.wall = WallMat::Brick; d.roof = RoofShape::Gable; d.roofMat = RoofMat::Slate; d.pitch = 4;
      d.cap = RoofShape::Onion; d.capMat = RoofMat::Copper; d.capPitch = 0; d.capTint = RGB(84, 110, 100);
      d.encl = WallMat::Brick; d.enclFeat = 0; d.window = WindowShape::Tall;
      break;
    case CU_SUN:
      d.wall = WallMat::Adobe; d.wallTint = c.limeTint(); d.roof = RoofShape::FlatParapet; d.roofMat = RoofMat::Adobe; d.flat = true; d.crenels = true;
      d.cap = RoofShape::FlatParapet; d.capMat = RoofMat::Adobe; d.encl = WallMat::Adobe; d.enclTint = c.limeTint(); d.window = WindowShape::Square;
      break;
    case CU_SYLVAN:
      d.wall = WallMat::Living; d.roof = RoofShape::Sweep; d.roofMat = st.roofMat == RoofMat::Bark ? RoofMat::Bark : RoofMat::Leaf; d.pitch = 2; d.roofTint = st.roofTint;
      d.cap = RoofShape::Sweep; d.capMat = d.roofMat; d.capPitch = 3; d.encl = WallMat::Living; d.enclFeat = 0; d.window = WindowShape::Round;
      break;
    case CU_STAR:
      d.wall = WallMat::Ashlar; d.roof = RoofShape::Hip; d.roofMat = RoofMat::Slate; d.pitch = 3; d.roofTint = c.domeTint();
      d.cap = RoofShape::Spire; d.capMat = RoofMat::Slate; d.capPitch = 7; d.capTint = d.roofTint; d.encl = WallMat::Ashlar; d.enclFeat = 0;
      d.window = WindowShape::Pointed;
      break;
    default:
      d.wall = weakWall(st.wall) ? WallMat::Stone : st.wall; d.roof = pitchedRoof(st.roof) && st.roof != RoofShape::Turf ? (st.roof == RoofShape::Gable ? RoofShape::Gable : RoofShape::Hip) : RoofShape::Hip;
      d.roofMat = c.monoRoof(); d.pitch = 3; d.roofTint = d.roofMat == st.roofMat ? st.roofTint : 0;
      if (flatRoof(st.roof)) { d.roof = RoofShape::FlatParapet; d.roofMat = RoofMat::Adobe; d.flat = true; d.crenels = true; d.cap = RoofShape::Dome; d.capMat = RoofMat::Copper; }
      break;
  }
  return d;
}
void dress(Volume& x, const Idiom& d, bool roof = true) {
  x.wall = d.wall; x.wallTint = d.wallTint; x.window = d.window;
  if (!roof) return;
  x.roof = d.roof; x.roofMat = d.roofMat; x.pitch = (uint8_t)d.pitch; x.roofTint = d.roofTint;
  if (d.flat) { x.roof = RoofShape::FlatParapet; x.roofMat = RoofMat::Adobe; x.wallH = (int16_t)(x.wallH + 4); if (d.crenels) x.feat |= VF_CRENELS; }
}
void capOf(Volume& x, const Idiom& d) {
  roofTo(x, d.cap, d.capMat, d.capPitch, d.capTint);
  if (d.cap == RoofShape::FlatParapet) { x.roofMat = RoofMat::Adobe; x.feat |= VF_CRENELS; }
  x.eave = 0;
}

// build a piece of a compound in its own small footprint with the culture's own assembler (a temple, a guildhall, an
// exchange), then set it into the compound at (x0, y0) px: its entrance becomes a painted door (the compound's door is
// its gate). Returns the index of its first volume (its body).
template <class F>
int subBuild(Bx& c, Building purpose, int x0, int y0, int wt, int ht, F&& fn) {
  Request rq = c.r;
  rq.purpose = purpose; rq.wTiles = std::max(2, wt); rq.hTiles = std::max(2, ht);
  rq.form = Form::Auto; rq.civic = 0; rq.seat = 0;
  rq.facts.storeys = c.want;
  rq.seed = mix32(c.r.seed ^ (uint32_t)purpose * 0x2545F491u ^ (uint32_t)(x0 * 977 + y0));
  Blueprint tb;
  tb.req = rq; tb.style = c.st; tb.facts = rq.facts;
  Bx s(rq, tb);
  s.want = c.want; s.budget = c.budget + y0; s.wl = c.wl; s.lord = c.lord; s.cul = c.cul; s.ur = c.ur;
  fn(s);
  ground(s);
  const int first = (int)c.v.size();
  for (Volume x : s.v) {
    x.x0 = (int16_t)(x.x0 + x0); x.x1 = (int16_t)(x.x1 + x0); x.y0 = (int16_t)(x.y0 + y0); x.y1 = (int16_t)(x.y1 + y0);
    if (x.doorHere) { x.doorHere = false; x.feat |= VF_DOOR; }
    c.v.push_back(x);
  }
  return first;
}

// the enclosure of a compound: a front wall either side of the gate, side walls, a back wall
void enclosure(Bx& c, int x0, int y0, int x1, int y1, int h, int th, WallMat mat, RoofShape coping, RoofMat cm, uint32_t feat, int gateHalf) {
  auto seg = [&](int a0, int b0, int a1, int b1) {
    if (a1 - a0 < 2 || b1 - b0 < 2) return;
    Volume e = c.vol(VolRole::Enclosure, a0, b0, a1, b1, h);
    e.wall = mat; e.face = Face::Blank; e.feat = feat; e.roof = coping; e.roofMat = cm; e.pitch = 1; e.eave = 0;
    e.ridgeNS = (a1 - a0) < (b1 - b0);
    if (mat == WallMat::Felt) e.face = Face::Lattice;
    c.add(e);
  };
  seg(x0, y0, x1, y0 + th);                          // the back wall
  seg(x0, y0 + th, x0 + th, y1 - th);                // west
  seg(x1 - th, y0 + th, x1, y1 - th);                // east
  seg(x0, y1 - th, c.dc - gateHalf, y1);             // the front, west of the gate
  seg(c.dc + gateHalf, y1 - th, x1, y1);             // east of the gate
}
// a compound's gate: a gate building over the door column
int gateVol(Bx& c, int half, int depth, int wallH, int y1, Face face, RoofShape roof, RoofMat rm, int pitch, WallMat wall, uint32_t tint = 0) {
  Volume g = c.vol(VolRole::Gate, c.dc - half, y1 - depth, c.dc + half, y1, wallH);
  g.face = face; g.wall = wall; g.doorHere = true; g.door = DoorShape::Double;
  roofTo(g, roof, rm, pitch, tint);
  g.ridgeNS = false; g.eave = 0;
  if (roof == RoofShape::FlatParapet) g.roofMat = RoofMat::Adobe;
  return c.add(g);
}

// the castle: the heartland's walled palace (the great hall between wings, an entrance tower, drum towers), the keep of
// a lord (a curtain, a donjon, drum towers); the highlands' tower castle
void seatCastle(Bx& c) {
  const int W = c.W, H = c.H;
  const bool high = c.cul == CU_HIGHLAND;
  const WallMat stone = c.cul == CU_HIGHLAND ? WallMat::Rubble : (c.st.wall == WallMat::Adobe ? WallMat::Adobe : WallMat::Stone);
  const bool flatSt = flatRoof(c.st.roof);
  const RoofMat rm = c.monoRoof() == RoofMat::Shingle && c.cul != CU_FJORD ? RoofMat::Slate : c.monoRoof();
  if (c.lord) {
    if (high) {   // a tower house within its barmkin wall
      const int tw = std::min(W - 40, 64);
      Volume bo = c.vol(VolRole::Body, W / 2 - tw / 2, 4, W / 2 + tw / 2, H - 10, c.wallFor(c.want, 30) + 14, c.want);
      bo.wall = stone; roofTo(bo, RoofShape::Gable, RoofMat::Slate, 4); bo.ridgeNS = false; bo.face = Face::Slits; bo.feat |= VF_FLAG | VF_DOOR;
      bo.gable = 1;
      const int bi = c.add(bo);
      for (int sd = 0; sd < 2; sd++) {   // corbelled corner turrets
        const Volume B = c.v[(size_t)bi];
        Volume t = c.rnd(VolRole::Tower, sd ? B.x1 - 2 : B.x0 + 2, B.y1 - 2, 6, B.wallH + 6);
        roofTo(t, RoofShape::Conical, RoofMat::Slate, 5); t.wall = stone; t.face = Face::Slits; t.eave = 0;
        c.add(t);
      }
      enclosure(c, 0, 0, W, H, 14, 5, stone, RoofShape::FlatParapet, RoofMat::Adobe, VF_CRENELS, 10);
      gateVol(c, 10, 8, 22, H, Face::Gate, RoofShape::FlatParapet, RoofMat::Adobe, 0, stone);
      chimneys(c, bi, 1);
      return;
    }
    // the heartland keep: a curtain, a donjon at the back with its own roof, drum towers proud of the corners
    Volume m = c.vol(VolRole::Body, 0, 0, W, H, 36, c.want);
    m.wall = stone; flat(m, true); m.face = Face::Slits; m.feat |= VF_BANNERS; m.doorHere = true;
    const int bi = c.add(m);
    const int dw = std::min(W - 40, 64), dd = std::min(H - 20, 34);
    Volume dj = c.vol(VolRole::Tower, W / 2 - dw / 2, 4, W / 2 + dw / 2, 4 + dd, 50);
    dj.wall = stone; dj.feat |= VF_FLAG; dj.face = Face::Slits;
    if (flatSt) flat(dj, true); else roofTo(dj, RoofShape::Hip, rm, 3);
    c.add(dj);
    for (int k = 0; k < 4; k++) {
      const bool front = k < 2;
      const int r = front ? 11 : 10, cx = (k & 1) ? W - (front ? 5 : 7) : (front ? 5 : 7), cy = front ? H - 7 : 9;
      Volume t = c.rnd(VolRole::Tower, cx, cy + r, r, front ? 50 : 58);
      t.wall = stone; t.face = Face::Slits; t.eave = 0;
      if (flatSt) flat(t, true); else roofTo(t, RoofShape::Conical, RoofMat::Slate, 3);
      c.add(t);
    }
    chimneys(c, 1, 1);
    (void)bi;
    return;
  }
  if (high) {   // the highland tower castle: a great tower house over a hall range, a curtain with drum towers
    const int tw = std::min(W / 3, 80);
    Volume bo = c.vol(VolRole::Body, W / 4, 10, W - W / 4, H - 30, c.wallFor(c.want, 28), c.want);
    bo.wall = stone; roofTo(bo, RoofShape::Gable, RoofMat::Slate, 3); bo.ridgeNS = false; bo.face = Face::Windows; bo.feat |= VF_BANNERS | VF_DOOR;
    const int bi = c.add(bo);
    Volume tw0 = c.vol(VolRole::Tower, W / 2 - tw / 2, 0, W / 2 + tw / 2, 40, 76);
    tw0.wall = stone; roofTo(tw0, RoofShape::Gable, RoofMat::Slate, 4); tw0.gable = 1; tw0.ridgeNS = false; tw0.face = Face::Slits; tw0.feat |= VF_FLAG;
    c.add(tw0);
    enclosure(c, 0, 4, W, H, 22, 7, stone, RoofShape::FlatParapet, RoofMat::Adobe, VF_CRENELS, 14);
    for (int k = 0; k < 2; k++) {
      Volume t = c.rnd(VolRole::Tower, k ? W - 8 : 8, H, 10, 40);
      t.wall = stone; roofTo(t, RoofShape::Conical, RoofMat::Slate, 4); t.face = Face::Slits; t.eave = 0; t.feat |= VF_FLAG;
      c.add(t);
    }
    gateVol(c, 14, 12, 34, H, Face::Gate, RoofShape::FlatParapet, RoofMat::Adobe, 0, stone);
    c.v.back().feat |= VF_CRENELS | VF_BANNERS;
    chimneys(c, bi, 2);
    return;
  }
  // the heartland palace: the great hall between wings, the entrance tower with the great door, drum towers
  const int wingW = W * 27 / 100;
  Volume hall = c.vol(VolRole::Body, wingW - 6, 6, W - wingW + 6, H - 12, 46, c.want);
  hall.wall = stone; hall.face = Face::Windows; hall.window = WindowShape::Tall;
  if (flatSt) roofTo(hall, RoofShape::Dome, RoofMat::Copper, 0);
  else roofTo(hall, c.st.roof == RoofShape::Gable || c.st.roof == RoofShape::Steep ? RoofShape::Gable : RoofShape::Hip, rm, 3, rm == RoofMat::Slate ? 0 : c.st.roofTint);
  c.add(hall);
  for (int s = 0; s < 2; s++) {
    Volume wg = c.vol(VolRole::Wing, s == 0 ? 12 : W - wingW - 2, 16, s == 0 ? wingW + 2 : W - 12, H - 4, 36, c.want);
    wg.wall = stone; wg.feat |= VF_BANNERS;
    if (flatSt) flat(wg, true); else { roofTo(wg, RoofShape::Hip, rm, 2); wg.ridgeNS = true; }
    c.add(wg);
  }
  Volume et = c.vol(VolRole::Tower, c.dc - 22, H - 28, c.dc + 22, H, 54);
  et.wall = stone; et.doorHere = true; et.feat |= VF_BANNERS | VF_FLAG | VF_LAMPS; et.face = Face::Windows;
  if (flatSt) flat(et, true); else roofTo(et, RoofShape::Hip, rm, 6);
  c.add(et);
  for (int k = 0; k < 4; k++) {
    const bool front = k < 2;
    const int r = front ? 12 : 11, cx = (k & 1) ? W - (front ? 6 : 9) : (front ? 6 : 9), cy = front ? H - 7 : 11;
    Volume t = c.rnd(VolRole::Tower, cx, cy + r, r, front ? 56 : 66);
    t.wall = stone; t.face = Face::Slits; t.feat |= VF_FLAG; t.eave = 0;
    if (flatSt) roofTo(t, RoofShape::Dome, RoofMat::Copper, 0); else roofTo(t, RoofShape::Conical, RoofMat::Slate, 3);
    c.add(t);
  }
  chimneys(c, 0, 2);
}

// a court palace in the imperial, dune or jade idiom
void seatCourt(Bx& c) {
  const int W = c.W, H = c.H;
  const int n = c.want;
  if (c.cul == CU_JADE) {
    // a walled precinct: a red wall under a tiled coping, a gate pavilion, the throne hall on a three-step platform under
    // a double-eaved sweeping roof, side halls facing the court
    const int hw = c.lord ? W * 6 / 10 : W / 2, hd = c.lord ? H / 2 : H * 42 / 100;
    const int hy0 = c.lord ? 8 : 10;
    Volume hall = c.vol(VolRole::Body, W / 2 - hw / 2, hy0, W / 2 + hw / 2, hy0 + hd, c.wallFor(n, 24), n);
    hall.wall = WallMat::Plaster; hall.wallTint = RGB(176, 52, 44);
    roofTo(hall, RoofShape::Pagoda, c.st.roofMat == RoofMat::GlazedTile ? RoofMat::GlazedTile : RoofMat::ClayTile, 2,
           c.st.roofMat == RoofMat::GlazedTile ? c.st.roofTint : RGB(212, 168, 60));
    hall.ornament |= art::ORN_DRAGON_HEADS | art::ORN_GILDING; hall.window = WindowShape::Lattice; hall.feat |= VF_DOOR | VF_LANTERNS;
    const int bi = c.add(hall);
    const Volume B = c.v[(size_t)bi];
    Volume skirt = c.vol(VolRole::Porch, B.x0 - 4, B.y1 - 2, B.x1 + 4, B.y1 + 8, 15);
    roofTo(skirt, RoofShape::Pagoda, B.roofMat, 1, B.roofTint); skirt.face = Face::Colonnade; skirt.wall = WallMat::Plaster; skirt.wallTint = B.wallTint;
    skirt.z0 = 0;
    c.add(skirt);
    // the platform under the hall (three steps), the side halls
    Volume pl = c.vol(VolRole::Platform, B.x0 - 8, B.y0 - 3, B.x1 + 8, B.y1 + 12, 8);
    pl.wall = WallMat::Ashlar; pl.face = Face::Blank; pl.feat |= VF_STEPS;
    for (Volume& x : c.v) if (x.role == VolRole::Body || x.role == VolRole::Porch) x.z0 = (int16_t)(x.z0 + 8);
    c.add(pl);
    if (!c.lord || W >= 128) for (int sd = 0; sd < 2; sd++) {
      const int sx0 = sd ? W - 10 - W / 6 : 10, sx1 = sd ? W - 10 : 10 + W / 6;
      Volume sh = c.vol(VolRole::Wing, sx0, H / 3, sx1, H - 22, c.wallFor(1, 22));
      sh.wall = WallMat::Plaster; sh.wallTint = RGB(176, 52, 44); roofTo(sh, RoofShape::Pagoda, B.roofMat, 2, B.roofTint); sh.ridgeNS = true; sh.window = WindowShape::Lattice;
      c.add(sh);
    }
    Volume court = c.vol(VolRole::Platform, 6, B.y1 + 4, W - 6, H - 6, 1);
    court.wall = WallMat::Ashlar; court.face = Face::Blank;
    c.add(court);
    enclosure(c, 0, 0, W, H, 18, 5, WallMat::Plaster, RoofShape::Gable, B.roofMat, 0, 16);
    for (Volume& x : c.v) if (x.role == VolRole::Enclosure) { x.wallTint = RGB(170, 50, 42); x.roofTint = B.roofTint; }
    gateVol(c, 16, 14, 24, H + 2, Face::Gate, RoofShape::Pagoda, B.roofMat, 2, WallMat::Plaster, B.roofTint);
    c.v.back().wallTint = RGB(176, 52, 44); c.v.back().ornament |= art::ORN_GILDING; c.v.back().feat |= VF_LANTERNS;
    return;
  }
  if (c.cul == CU_DUNE) {
    // a walled palace round a garden court: the domed audience hall at the back, flat-roofed ranges either side, two
    // minarets, the iwan gate
    const int hw = c.lord ? W / 2 : W * 42 / 100;
    Volume hall = c.vol(VolRole::Body, W / 2 - hw / 2, 6, W / 2 + hw / 2, c.lord ? H - 18 : H / 2 + 6, c.wallFor(n, 26) + 4, n);
    hall.wall = WallMat::Adobe; roofTo(hall, RoofShape::Onion, RoofMat::GlazedTile, 0, c.domeTint()); hall.feat |= VF_DOOR; hall.ornament |= art::ORN_PAINTED_BANDS;
    hall.window = WindowShape::Screen;
    const int bi = c.add(hall);
    const Volume B = c.v[(size_t)bi];
    for (int sd = 0; sd < 2; sd++) {
      Volume r = c.vol(VolRole::Wing, sd ? B.x1 : 8, 10, sd ? W - 8 : B.x0, c.lord ? H - 14 : H / 2 + 14, std::max(22, B.wallH - 8));
      r.wall = WallMat::Adobe; flat(r, true); r.wallH = (int16_t)(r.wallH + 4); r.window = WindowShape::Screen;
      c.add(r);
      Volume mi = c.rnd(VolRole::Tower, sd ? B.x1 + 2 : B.x0 - 2, B.y1 - 2, 5, B.wallH + 34);
      roofTo(mi, RoofShape::Onion, RoofMat::GlazedTile, 0, c.domeTint()); mi.wall = WallMat::Adobe; mi.feat |= VF_BALCONY; mi.face = Face::Blank;
      c.add(mi);
    }
    Volume court = c.vol(VolRole::Platform, 8, B.y1, W - 8, H - 6, 1);
    court.wall = WallMat::Adobe; court.face = Face::Blank; court.feat |= VF_GARDEN;
    if (H - 6 - B.y1 >= 10) c.add(court);
    enclosure(c, 0, 0, W, H, 20, 6, WallMat::Adobe, RoofShape::FlatParapet, RoofMat::Adobe, VF_CRENELS, 15);
    gateVol(c, 15, 12, 40, H, Face::Iwan, RoofShape::FlatParapet, RoofMat::Adobe, 0, WallMat::Adobe);
    c.v.back().feat |= VF_CRENELS | VF_BANNERS; c.v.back().ornament |= art::ORN_PAINTED_BANDS;
    return;
  }
  // the imperial palace: a colonnaded front range with a central portico under a pediment, the audience hall at the
  // back under a great copper dome on its drum, ranges round the court, square corner pavilions
  const int hw = c.lord ? W / 2 : W * 46 / 100;
  Volume hall = c.vol(VolRole::Body, W / 2 - hw / 2, 4, W / 2 + hw / 2, c.lord ? H * 6 / 10 : H / 2, c.wallFor(n, 28), n);
  hall.wall = WallMat::Ashlar; roofTo(hall, RoofShape::Hip, RoofMat::ClayTile, 1, c.st.roofMat == RoofMat::ClayTile ? c.st.roofTint : 0); hall.feat |= VF_DOOR;
  hall.window = WindowShape::Tall;
  const int bi = c.add(hall);
  const Volume B = c.v[(size_t)bi];
  Volume drum = c.rnd(VolRole::Drum, (B.x0 + B.x1) / 2, (B.y0 + B.y1) / 2 + std::min(22, hw / 4), std::min(22, hw / 4), 10);
  drum.z0 = (int16_t)(B.z0 + B.wallH); roofTo(drum, RoofShape::Dome, RoofMat::Copper, 0); drum.wall = WallMat::Ashlar; drum.face = Face::Windows; drum.window = WindowShape::Arched;
  c.add(drum);
  Volume fr = c.vol(VolRole::Wing, 10, H - 24, W - 10, H - 6, c.wallFor(1, 24));
  fr.wall = WallMat::Ashlar; roofTo(fr, RoofShape::Hip, RoofMat::ClayTile, 1, B.roofTint); fr.face = Face::Colonnade; fr.feat |= VF_BANNERS;
  c.add(fr);
  for (int sd = 0; sd < 2; sd++) {
    Volume r = c.vol(VolRole::Wing, sd ? W - 10 - W / 7 : 10, 12, sd ? W - 10 : 10 + W / 7, H - 24, c.wallFor(1, 24));
    r.wall = WallMat::Ashlar; roofTo(r, RoofShape::Hip, RoofMat::ClayTile, 1, B.roofTint); r.ridgeNS = true; r.face = Face::Colonnade;
    c.add(r);
    Volume pv = c.vol(VolRole::Tower, sd ? W - 22 : 0, H - 26, sd ? W : 22, H - 2, c.wallFor(1, 24) + 14);
    pv.wall = WallMat::Ashlar; roofTo(pv, RoofShape::Hip, RoofMat::ClayTile, 2, B.roofTint);
    c.add(pv);
  }
  Volume court = c.vol(VolRole::Platform, 10 + W / 7, B.y1, W - 10 - W / 7, H - 24, 1);
  court.wall = WallMat::Ashlar; court.face = Face::Blank; court.feat |= VF_GARDEN;
  if (H - 24 - B.y1 >= 8) c.add(court);
  Volume po = c.vol(VolRole::Porch, c.dc - 24, H - 30, c.dc + 24, H + 2, c.wallFor(1, 24) + 6);
  po.wall = WallMat::Ashlar; roofTo(po, RoofShape::Gable, RoofMat::ClayTile, 0, B.roofTint); po.ridgeNS = true; po.face = Face::Colonnade; po.doorHere = true;
  po.ornament |= art::ORN_GILDING | art::ORN_PORCH_COLUMNS;
  c.add(po);
  platformUnder(c, 4, WallMat::Ashlar, true);
}

// the jarl's (a thane's) great hall in its stockade: the long hall under turf or shingle with carved ridge ends, a
// carved porch, side halls, the stockade with its gate tower and watch platforms
void seatHall(Bx& c) {
  const int W = c.W, H = c.H, n = c.want;
  const Idiom d = idiomOf(c);
  const bool north = c.cul == CU_FJORD || c.cul == CU_HIGHLAND || c.cul == CU_HEART || c.cul == CU_NONE;
  // the hall at the back of its court, the side halls forward of its ends (a U round the court), the stockade in front
  const int hy0 = c.lord ? 6 : 10, hy1 = c.lord ? H - 14 : std::min(H - 30, hy0 + std::max(40, H * 42 / 100));
  Volume hall = c.vol(VolRole::Body, c.lord ? 10 : W / 6, hy0, c.lord ? W - 10 : W - W / 6, hy1, c.wallFor(n, 26), n);
  if (c.cul == CU_FJORD) {
    hall.wall = d.wall;
    roofTo(hall, c.st.roofMat == RoofMat::Turf ? RoofShape::Turf : RoofShape::Steep, c.st.roofMat == RoofMat::Turf ? RoofMat::Turf : RoofMat::Shingle, 3, c.st.roofTint);
    hall.ornament |= art::ORN_DRAGON_HEADS | art::ORN_CARVED_RIDGE | art::ORN_PAINTED_BANDS;
  } else {   // a thane's moot hall: timber framing (the culture's walls) under a steep roof of its own
    dress(hall, d);
    if (c.cul == CU_HEART || c.cul == CU_NONE) { hall.wall = WallMat::Timber; roofTo(hall, RoofShape::Steep, RoofMat::Shingle, 4, 0); }
    hall.ornament |= art::ORN_CARVED_RIDGE | art::ORN_FINIALS;
  }
  hall.ridgeNS = false;
  hall.feat |= VF_CARVED | VF_BANNERS | VF_DOOR;
  const int bi = c.add(hall);
  const Volume B = c.v[(size_t)bi];
  // a carved porch before the hall's door, side halls / a byre
  Volume po = c.vol(VolRole::Porch, c.dc - 10, B.y1 - 1, c.dc + 10, B.y1 + 8, 16);
  po.z0 = B.z0; roofTo(po, flatRoof(B.roof) ? RoofShape::FlatParapet : RoofShape::Steep, flatRoof(B.roof) ? RoofMat::Adobe : (B.roofMat == RoofMat::Turf ? RoofMat::Shingle : B.roofMat), 4, B.roofMat == RoofMat::Turf ? 0 : B.roofTint);
  po.ridgeNS = true; po.face = Face::Colonnade; po.wall = B.wall; po.feat |= VF_CARVED; po.ornament = art::ORN_CARVED_RIDGE;
  c.add(po);
  if (!c.lord) {
    for (int sd = 0; sd < 2; sd++) {
      Volume sh = c.vol(VolRole::Wing, sd ? W - W / 6 + 2 : 10, B.y0 + 10, sd ? W - 10 : W / 6 - 2, H - 16, c.wallFor(1, 20));
      sh.wall = B.wall; roofTo(sh, B.roof, B.roofMat, 3, B.roofTint); sh.ridgeNS = true; sh.feat |= VF_CARVED;
      if (sh.x1 - sh.x0 >= 16) c.add(sh);
    }
    Volume court = c.vol(VolRole::Platform, W / 6, hy1 + 1, W - W / 6, H - 6, 1);
    court.wall = WallMat::Adobe; court.face = Face::Blank;
    if (court.y1 - court.y0 >= 6) c.add(court);
  }
  // the stockade (the culture's own wall elsewhere), its gate tower and watch platforms at the front corners
  const WallMat ew = north ? WallMat::Log : d.encl;
  enclosure(c, 0, 0, W, H, 16, 4, ew, north ? RoofShape::FlatParapet : d.coping, north ? RoofMat::Adobe : d.copingMat, north ? VF_POINTS : d.enclFeat, 12);
  if (!north) for (Volume& x : c.v) if (x.role == VolRole::Enclosure) x.wallTint = d.enclTint;
  gateVol(c, 12, 8, 26, H, Face::Gate, north ? RoofShape::Gable : d.cap, north ? RoofMat::Shingle : d.capMat, 3, north ? WallMat::Log : d.wall, north ? 0 : d.capTint);
  c.v.back().ridgeNS = false;
  c.v.back().feat |= VF_CARVED;
  for (int sd = 0; sd < 2; sd++) {
    const int t = towerVol(c, VolRole::Tower, false, sd ? W - 14 : 0, H, 14, 30, north ? RoofShape::Hip : d.cap, north ? RoofMat::Shingle : d.capMat, 3, north ? 0 : d.capTint);
    c.v[(size_t)t].wall = north ? WallMat::Log : d.wall;
    c.v[(size_t)t].face = Face::Slits;
  }
  chimneys(c, bi, 2);
}

// the khan's great tent in a ring of court tents and banners, a felt-and-lattice fence round it (no stone)
void seatTent(Bx& c) {
  const int W = c.W, H = c.H, n = c.want;
  const int rad = std::min(c.lord ? H / 2 - 4 : H / 2 - 8, W / 5);
  Volume gt = c.rnd(VolRole::Body, W / 2, H - (c.lord ? 6 : 14), rad, c.wallFor(n, 22), n);
  gt.wall = n >= 2 ? WallMat::Plank : WallMat::Felt; roofTo(gt, RoofShape::Conical, RoofMat::Felt, 1, c.st.roofTint);
  gt.feat |= VF_CROWN | VF_DOOR | VF_BANNERS; gt.ornament |= art::ORN_GILDING | art::ORN_PAINTED_BANDS | art::ORN_FINIALS; gt.window = WindowShape::Round;
  gt.door = DoorShape::Flap;
  if (!c.lord) gt.wallTint = lerpC(c.st.wallTint, RGB(236, 226, 200), 50);
  c.add(gt);
  // (M3b round 3) the court tents stand apart round the great tent, never crammed against it or each other (six
  // yurts edge to edge overlapped and clipped in a box): one or two each side, set in the corners of the court between
  // the great tent and the fence, the space between them clear; felt yurts and the culture's cloth tents alternating
  const int ring = c.lord ? 2 : 4;
  const int side = std::max(0, (W - 2 * rad) / 2 - 14);           // the room beside the great tent, inside the fence
  const int tr = std::clamp(std::min(side / 2, (H - 27) / 4), 7, 15);
  for (int k = 0; k < ring; k++) {
    const bool east = k & 1;
    const bool front = k >= 2;
    const int cx = east ? W - 11 - tr : 11 + tr;
    const int fy = front ? H - 11 : 11 + 2 * tr;                  // the tent's front (its circle's south edge)
    Volume t = c.rnd(VolRole::Tent, cx, fy, tr, 12 + (front ? 2 : 0));
    const bool cloth = (k + (int)(c.r.seed & 1)) % 2 == 0;
    t.wall = WallMat::Felt;
    roofTo(t, cloth ? RoofShape::Tent : RoofShape::Conical, RoofMat::Felt, 1, cloth && c.st.accentTint ? lerpC(c.st.accentTint, RGB(236, 226, 200), 35) : c.st.roofTint);
    if (!cloth) t.feat |= VF_CROWN; else t.feat |= VF_ROPES;
    if (std::abs(cx - W / 2) >= rad + tr + 3) c.add(t);           // (never into the great tent's felt)
  }
  // the court ground, the lattice fence, the gate of painted posts, horse-tail standards
  Volume court = c.vol(VolRole::Platform, 6, 6, W - 6, H - 6, 1);
  court.wall = WallMat::Adobe; court.face = Face::Blank;
  c.add(court);
  enclosure(c, 0, 0, W, H, 10, 3, WallMat::Felt, RoofShape::FlatParapet, RoofMat::Felt, VF_PENNANTS, 12);
  gateVol(c, 12, 6, 22, H, Face::Gate, RoofShape::Tent, RoofMat::Felt, 2, WallMat::Plank, c.st.accentTint ? c.st.accentTint : RGB(176, 50, 46));
  c.v.back().feat |= VF_PENNANTS;
  c.grounded = true;
}

// the sun theocracy's temple complex: a stepped pyramid with a shrine on top in a walled precinct, the priests' ranges
void seatPyramid(Bx& c) {
  const int W = c.W, H = c.H;
  const uint32_t lime = c.limeTint();
  const int pw = c.lord ? W * 6 / 10 : W * 46 / 100, pd = c.lord ? H - 14 : H * 7 / 10;
  Volume py = c.vol(VolRole::Body, W / 2 - pw / 2, 4, W / 2 + pw / 2, 4 + pd, 16, c.want);
  py.wall = WallMat::Adobe; py.wallTint = lime;
  roofTo(py, RoofShape::Stepped, RoofMat::Adobe, 3, lerpC(lime, RGB(186, 150, 104), 55));
  py.feat |= VF_STAIR | VF_DOOR; py.ornament |= art::ORN_PAINTED_BANDS; py.face = Face::Blank;
  const int bi = c.add(py);
  const Volume B = c.v[(size_t)bi];
  const int sw = std::max(18, pw * 34 / 100), sd = std::max(10, pd * 3 / 10);
  Volume sh = c.vol(VolRole::Tier, W / 2 - sw / 2, (B.y0 + B.y1) / 2 - sd / 2 - 2, W / 2 + sw / 2, (B.y0 + B.y1) / 2 + sd / 2 - 2, 12);
  sh.z0 = (int16_t)(16 + std::min(roofHeight(B), c.budget - 40));
  flat(sh, true); sh.wall = WallMat::Adobe; sh.wallTint = lime; sh.feat |= VF_DOOR | VF_FLAG; sh.ornament |= art::ORN_PAINTED_BANDS;
  c.add(sh);
  if (!c.lord) for (int sd2 = 0; sd2 < 2; sd2++) {   // the priests' ranges either side, flat behind stepped merlons
    Volume r = c.vol(VolRole::Wing, sd2 ? B.x1 + 6 : 8, 14, sd2 ? W - 8 : B.x0 - 6, H - 20, c.wallFor(1, 24) + 4);
    r.wall = WallMat::Adobe; r.wallTint = lime; flat(r, true); r.ornament |= art::ORN_PAINTED_BANDS;
    if (r.x1 - r.x0 >= 16) c.add(r);
  }
  Volume court = c.vol(VolRole::Platform, 5, 5, W - 5, H - 5, 1);
  court.wall = WallMat::Ashlar; court.face = Face::Blank;
  c.add(court);
  enclosure(c, 0, 0, W, H, 14, 5, WallMat::Adobe, RoofShape::FlatParapet, RoofMat::Adobe, VF_CRENELS, 14);
  for (Volume& x : c.v) if (x.role == VolRole::Enclosure) x.wallTint = lime;
  gateVol(c, 14, 10, 26, H, Face::Gate, RoofShape::FlatParapet, RoofMat::Adobe, 0, WallMat::Adobe);
  c.v.back().wallTint = lime; c.v.back().feat |= VF_CRENELS | VF_BANNERS; c.v.back().ornament |= art::ORN_PAINTED_BANDS;
}

// a theocracy's temple complex in any other people's idiom: the culture's own high temple (a stave church, a domed
// prayer hall with its minaret, a rotunda, a pagoda, a nave and spire) raised in a walled precinct, the priests'
// ranges either side of the court, the precinct gate
void templeComplex(Bx& c) {
  const int W = c.W, H = c.H;
  const Idiom d = idiomOf(c);
  const int wt = W / 16, ht = H / 16;
  const int tw = std::max(4, c.lord ? wt * 6 / 10 : wt * 45 / 100), th = std::max(3, ht - (c.lord ? 1 : 3));
  const int tx = (W - tw * 16) / 2;
  subBuild(c, Building::Temple, tx, 4, tw, th, [](Bx& s) { temple(s); });
  for (Volume& x : c.v) x.feat |= (x.role == VolRole::Body) ? VF_BANNERS : 0u;
  c.v[0].ornament |= art::ORN_GILDING;
  const int ty1 = 4 + th * 16;
  if (!c.lord) for (int sd = 0; sd < 2; sd++) {   // the priests' ranges
    Volume r = c.vol(VolRole::Wing, sd ? tx + tw * 16 + 6 : 8, 14, sd ? W - 8 : tx - 6, H - 20, c.wallFor(1, 22));
    dress(r, d);
    r.ridgeNS = !d.flat && r.x1 - r.x0 < r.y1 - r.y0;
    if (r.x1 - r.x0 >= 16) c.add(r);
  }
  Volume court = c.vol(VolRole::Platform, 5, std::min(ty1, H - 12), W - 5, H - 5, 1);
  court.wall = d.wall == WallMat::Adobe ? WallMat::Adobe : WallMat::Ashlar; court.face = Face::Blank;
  if (court.y1 - court.y0 >= 4) c.add(court);
  enclosure(c, 0, 0, W, H, 14, 5, d.encl, d.coping, d.copingMat, d.enclFeat, 14);
  for (Volume& x : c.v) if (x.role == VolRole::Enclosure) x.wallTint = d.enclTint;
  const int g = gateVol(c, 14, 10, 26, H, d.cap == RoofShape::FlatParapet || d.flat ? Face::Gate : Face::Gate, d.flat ? RoofShape::FlatParapet : d.roof, d.flat ? RoofMat::Adobe : d.roofMat, 2,
                        d.wall, d.roofTint);
  c.v[(size_t)g].wallTint = d.wallTint;
  c.v[(size_t)g].feat |= VF_BANNERS;
  if (d.flat) c.v[(size_t)g].feat |= VF_CRENELS;
  if (c.cul == CU_DUNE) c.v[(size_t)g].face = Face::Iwan;
}
void seatTemple(Bx& c) {
  if (c.cul == CU_SUN || c.cul == CU_NONE) seatPyramid(c);
  else templeComplex(c);
}

// the merchant republic's guildhall and exchange facing one square: the culture's guildhall (its belfry rising as the
// city's tallest tower) beside the culture's arcaded exchange, a loggia over the door between them
void seatGuild(Bx& c) {
  const int W = c.W, H = c.H;
  const int wt = W / 16, ht = H / 16;
  if (wt < 8 || ht < 3) { guildhall(c); c.v[0].feat |= VF_BANNERS; for (Volume& x : c.v) if (x.role == VolRole::Tower) x.feat |= VF_FLAG; return; }
  const bool east = c.ch(128);
  const int gwt = std::max(4, wt * 55 / 100), ewt = std::max(3, wt - gwt);
  const int gx = east ? W - gwt * 16 : 0, ex = east ? 0 : gwt * 16;
  subBuild(c, Building::Guildhall, gx, 0, gwt, ht, [](Bx& s) { guildhall(s); });
  for (Volume& x : c.v) if (x.role == VolRole::Tower) { x.wallH = (int16_t)(x.wallH + (c.lord ? 8 : 22)); x.feat |= VF_FLAG; }
  c.v[0].feat |= VF_BANNERS;
  c.v[0].ornament |= art::ORN_GILDING;
  const int ehT = std::max(2, ht - (c.lord ? 0 : 1));
  subBuild(c, Building::Exchange, ex, (ht - ehT) * 16, ewt, ehT, [](Bx& s) { exchange(s); });
}

// a court palace in an idiom of its own (a river prince's palazzo, a monarchy elsewhere): the hall of state at the back
// of a court, ranges either side, the front range with the gate tower, the culture's tower caps on the corners
void courtGeneric(Bx& c) {
  const int W = c.W, H = c.H, n = c.want;
  const Idiom d = idiomOf(c);
  const int fd = c.lord ? 16 : 22, bd = c.lord ? H / 2 : H * 45 / 100, sw = c.lord ? 22 : W / 6;
  Volume hall = c.vol(VolRole::Body, 6, 2, W - 6, bd, c.wallFor(n, 28), n);
  dress(hall, d);
  hall.ridgeNS = false; hall.feat |= VF_BANNERS | VF_DOOR; hall.ornament |= art::ORN_GILDING | art::ORN_FINIALS;
  if (c.cul == CU_RIVER) { hall.window = WindowShape::Tall; hall.roof = RoofShape::Hip; hall.pitch = 2; }
  c.add(hall);
  for (int sd = 0; sd < 2; sd++) {
    Volume r = c.vol(VolRole::Wing, sd ? W - sw : 0, bd, sd ? W : sw, H - fd, c.wallFor(1, 24));
    dress(r, d);
    r.ridgeNS = !d.flat;
    if (r.roof == RoofShape::Gable || r.roof == RoofShape::Steep) r.roof = RoofShape::Hip;
    c.add(r);
  }
  Volume fr = c.vol(VolRole::Wing, 0, H - fd, W, H, c.wallFor(std::min(2, n), 24));
  dress(fr, d);
  fr.ridgeNS = false; fr.storeys = (uint8_t)std::min(2, n);
  if (c.cul == CU_RIVER) fr.face = Face::Arcade;
  c.add(fr);
  Volume court = c.vol(VolRole::Platform, sw, bd, W - sw, H - fd, 1);
  court.wall = WallMat::Ashlar; court.face = Face::Blank; court.feat |= VF_GARDEN;
  if (court.y1 - court.y0 >= 6) c.add(court);
  // the gate tower in the front range, its cap in the culture's idiom
  const int gw = 26;
  Volume g = c.vol(VolRole::Gate, c.dc - gw / 2, H - fd - 6, c.dc + gw / 2, H + 2, fr.wallH + 18);
  dress(g, d, false);
  capOf(g, d);
  g.face = Face::Gate; g.doorHere = true; g.feat |= VF_BANNERS | VF_FLAG;
  if (g.roof == RoofShape::Pagoda || g.roof == RoofShape::Sweep || g.roof == RoofShape::Spire || g.roof == RoofShape::Conical) g.pitch = (uint8_t)std::max(2, (int)g.pitch);
  c.add(g);
  if (!c.lord) for (int sd = 0; sd < 2; sd++) {   // corner towers
    const int t = towerVol(c, VolRole::Tower, c.cul != CU_JADE && c.cul != CU_SUN, sd ? W - 18 : 0, H, 18, fr.wallH + 20, d.cap, d.capMat, d.capPitch, d.capTint);
    c.v[(size_t)t].wall = d.wall; c.v[(size_t)t].wallTint = d.wallTint; c.v[(size_t)t].face = Face::Windows; c.v[(size_t)t].window = d.window;
    if (d.cap == RoofShape::FlatParapet) c.v[(size_t)t].feat |= VF_CRENELS;
  }
  chimneys(c, 0, 2);
}

// the starspire council: a white hall of the high council, a great white spire rising over it, slender turrets
void seatSpire(Bx& c) {
  const int W = c.W, H = c.H, n = c.want;
  const uint32_t glaze = c.domeTint();
  Volume hall = c.vol(VolRole::Body, c.lord ? 6 : W / 8, c.lord ? 8 : 14, c.lord ? W - 6 : W - W / 8, H - 8, c.wallFor(n, 28), n);
  hall.wall = WallMat::Ashlar; roofTo(hall, RoofShape::Hip, RoofMat::Slate, 3, glaze); hall.window = WindowShape::Pointed;
  hall.ornament |= art::ORN_GILDING | art::ORN_FINIALS; hall.feat |= VF_BANNERS;
  const int bi = c.add(hall);
  const Volume B = c.v[(size_t)bi];
  const int sr = c.lord ? 11 : 15;
  Volume sp = c.rnd(VolRole::Tower, W / 2, B.y0 + 2 * sr - 2, sr, c.lord ? 64 : 84);
  sp.wall = WallMat::Ashlar; roofTo(sp, RoofShape::Spire, RoofMat::Slate, 8, glaze); sp.face = Face::Windows; sp.window = WindowShape::Pointed; sp.feat |= VF_BALCONY | VF_FLAG;
  c.add(sp);
  for (int sd = 0; sd < 2; sd++) {
    Volume t = c.rnd(VolRole::Tower, sd ? B.x1 - 2 : B.x0 + 2, B.y1 + 2, 7, B.wallH + 18);
    t.wall = WallMat::Ashlar; roofTo(t, RoofShape::Onion, RoofMat::Slate, 0, glaze); t.face = Face::Windows; t.window = WindowShape::Pointed;
    c.add(t);
  }
  porch(c, bi, 3, 16, 8);
  for (Volume& x : c.v) if (x.role == VolRole::Porch) { x.wall = WallMat::Ashlar; x.window = WindowShape::Pointed; }
  platformUnder(c, 5, WallMat::Ashlar, true);
}

// the sylvan tree palace: halls grown round a colossal living tree, pods in its roots, a raised walk
void seatTree(Bx& c) {
  const int W = c.W, H = c.H, n = c.want;
  const int tr = c.lord ? 10 : 16;
  Volume hall = c.rnd(VolRole::Body, c.dc, H - 2, std::min(c.lord ? H / 2 - 2 : H / 4 + 2, W / 6), c.wallFor(n, 24), n);
  const uint32_t leafTint = lerpC(c.st.roofTint ? c.st.roofTint : RGB(112, 162, 66), RGB(196, 178, 84), 35);   // a golden leaf, lighter than the crown
  hall.wall = WallMat::Living; roofTo(hall, RoofShape::Sweep, RoofMat::Leaf, 2, leafTint);
  hall.doorHere = true; hall.door = DoorShape::Round; hall.window = WindowShape::Round; hall.ornament |= art::ORN_VINES | art::ORN_LANTERNS | art::ORN_GILDING;
  hall.feat |= VF_BANNERS;
  const int bi = c.add(hall);
  Volume tree = c.rnd(VolRole::Tree, W / 2, (c.lord ? H / 2 : H / 2 - 4) + tr, tr, c.lord ? 44 : 56);
  tree.wall = WallMat::Living;
  c.add(tree);
  const int pods = c.lord ? 2 : 4;
  for (int k = 0; k < pods; k++) {
    const bool east = k & 1;
    const int row = k / 2;
    const int pr = row == 0 ? std::min(16, H / 4) : 11;
    const int cx = east ? W / 2 + std::max(tr + pr + 4, W / 4 + row * 10) + 6 : W / 2 - std::max(tr + pr + 4, W / 4 + row * 10) - 6;
    Volume pod = c.rnd(VolRole::Wing, std::clamp(cx, pr + 2, W - pr - 2), row == 0 ? H - 6 : H / 2 + 4, pr, c.wallFor(1, 20));
    pod.wall = WallMat::Living; roofTo(pod, k % 3 == 2 ? RoofShape::Conical : RoofShape::Sweep, k % 2 ? RoofMat::Bark : RoofMat::Leaf, 3, k % 2 ? 0 : leafTint); pod.window = WindowShape::Round;
    pod.ornament |= art::ORN_VINES;
    c.add(pod);
  }
  if (!c.lord) {   // a pod grown high in the tree, on a ring platform round the trunk
    Volume ring = c.rnd(VolRole::Platform, W / 2, H / 2 - 4 + tr + 6, tr + 6, 4);
    ring.z0 = 30; ring.wall = WallMat::Living; ring.feat |= VF_STILTS;
    c.add(ring);
    Volume hp = c.rnd(VolRole::Wing, W / 2 + tr - 2, H / 2 - 4 + tr + 8, 9, 14);
    hp.z0 = 34; hp.wall = WallMat::Living; roofTo(hp, RoofShape::Conical, c.v[(size_t)bi].roofMat, 4, c.st.roofTint); hp.window = WindowShape::Round;
    c.add(hp);
  }
  c.grounded = true;
}

// the marsh elders' great stilt longhouse over the water, side halls, a boardwalk
void seatStilt(Bx& c) {
  const int W = c.W, H = c.H, n = c.want;
  const int hy0 = c.lord ? 6 : 12, hy1 = c.lord ? H - 18 : std::min(H - 26, hy0 + std::max(36, H * 40 / 100));
  Volume hall = c.vol(VolRole::Body, c.lord ? 6 : W / 6, hy0, c.lord ? W - 6 : W - W / 6, hy1, c.wallFor(n, 24), n);
  hall.wall = WallMat::Plank; roofTo(hall, RoofShape::Steep, RoofMat::Thatch, 4); hall.ridgeNS = false;
  hall.ornament |= art::ORN_CARVED_RIDGE | art::ORN_LANTERNS | art::ORN_PAINTED_BANDS; hall.feat |= VF_CARVED | VF_BANNERS | VF_DOOR;
  const int bi = c.add(hall);
  const Volume B = c.v[(size_t)bi];
  Volume po = c.vol(VolRole::Porch, B.x0 + 4, B.y1 - 1, B.x1 - 4, B.y1 + 8, 15);
  roofTo(po, RoofShape::Hip, RoofMat::Thatch, 2); po.face = Face::Veranda; po.wall = WallMat::Plank;
  c.add(po);
  if (!c.lord) for (int sd = 0; sd < 2; sd++) {   // the elders' side halls, gable-on to the water, and their boardwalks
    Volume sh = c.vol(VolRole::Wing, sd ? W - W / 6 + 6 : 6, B.y0 + 6, sd ? W - 6 : W / 6 - 6, H - 12, c.wallFor(1, 20));
    sh.wall = WallMat::Plank; roofTo(sh, RoofShape::Steep, RoofMat::Thatch, 4); sh.ridgeNS = true; sh.feat |= VF_CARVED;
    sh.ornament |= art::ORN_CARVED_RIDGE;
    if (sh.x1 - sh.x0 >= 16) c.add(sh);
    Volume walk = c.vol(VolRole::Platform, sd ? B.x1 - 2 : W / 6 - 6, B.y1 - 6, sd ? W - W / 6 + 6 : B.x0 + 2, B.y1 + 2, 2);
    walk.wall = WallMat::Plank; walk.face = Face::Blank; walk.roof = RoofShape::FlatParapet; walk.roofMat = RoofMat::Adobe;
    if (walk.x1 - walk.x0 >= 4) c.add(walk);
  }
  stilts(c, 11);
  // the boardwalk from the veranda to the door column's landing on the bottom row, on its own posts
  const Volume Bh = c.v[(size_t)bi];
  Volume bw = c.vol(VolRole::Platform, c.dc - 7, Bh.y1 + 6, c.dc + 7, H, 2);
  bw.z0 = (int16_t)(Bh.z0 - 2); bw.wall = WallMat::Plank; bw.face = Face::Blank; bw.feat |= VF_STILTS; bw.doorHere = true; bw.roof = RoofShape::FlatParapet; bw.roofMat = RoofMat::Adobe;
  if (bw.y1 - bw.y0 >= 4) c.add(bw);
}

void seat(Bx& c) {
  // a castle only where the culture really has one (the heartland, the highlands); a court palace in its own idiom
  // outside the imperial, dune and jade courts
  int sk = c.seat;
  if (sk == SK_CASTLE && c.cul != CU_HEART && c.cul != CU_HIGHLAND && c.cul != CU_NONE) sk = cultureSeat(c.cul);
  if (sk == SK_COURT && c.cul != CU_IMPERIAL && c.cul != CU_DUNE && c.cul != CU_JADE) { courtGeneric(c); return; }
  switch (sk) {
    case SK_COURT: seatCourt(c); return;
    case SK_HALL: seatHall(c); return;
    case SK_TENT: seatTent(c); return;
    case SK_TEMPLE: seatTemple(c); return;
    case SK_GUILD: seatGuild(c); return;
    case SK_SPIRE: seatSpire(c); return;
    case SK_TREE: seatTree(c); return;
    case SK_STILT: seatStilt(c); return;
    default: seatCastle(c); return;
  }
}

// ---------------------------------------------------------------- forms asked by the settlement
// a compound (several buildings in an enclosure) of an ordinary purpose: its main building, a smaller one, the wall
void genericCompound(Bx& c) {
  const int W = c.W, H = c.H;
  const bool tents = c.cul == CU_STEPPE;
  if (tents) {
    const int rad = std::max(9, std::min(H / 2 - 6, W / 4));
    Volume bo = c.rnd(VolRole::Body, c.W / 2, H - 8, rad, c.wallFor(std::min(c.want, 2), 20), std::min(c.want, 2));
    bo.wall = c.want >= 2 ? WallMat::Plank : WallMat::Felt; roofTo(bo, RoofShape::Conical, RoofMat::Felt, 1, c.st.roofTint); bo.feat |= VF_CROWN | VF_DOOR; bo.door = DoorShape::Flap;
    c.add(bo);
    for (int sd = 0; sd < 2; sd++) {
      Volume t = c.rnd(VolRole::Tent, sd ? W - 12 : 12, H - 8, 8, 12);
      t.wall = WallMat::Felt; roofTo(t, RoofShape::Conical, RoofMat::Felt, 1, c.st.roofTint); t.feat |= VF_CROWN;
      if (W >= 64) c.add(t);
    }
    enclosure(c, 0, 0, W, H, 9, 3, WallMat::Felt, RoofShape::FlatParapet, RoofMat::Felt, 0, 8);
    gateVol(c, 8, 4, 18, H, Face::Gate, RoofShape::Tent, RoofMat::Felt, 2, WallMat::Plank, c.st.accentTint);
    return;
  }
  Volume bo = c.vol(VolRole::Body, 6, 4, W - 6, H * 6 / 10, c.wallFor(c.want, 24), c.want);
  if (flatRoof(bo.roof)) bo.wallH = (int16_t)(bo.wallH + 4);
  bo.feat |= VF_DOOR; bo.ridgeNS = false;
  c.add(bo);
  const WallMat ew = c.cul == CU_FJORD ? WallMat::Log : (c.cul == CU_DUNE || c.cul == CU_SUN ? WallMat::Adobe : (weakWall(c.st.wall) ? WallMat::Stone : c.st.wall));
  enclosure(c, 0, 0, W, H, 12, 4, ew, c.cul == CU_JADE ? RoofShape::Gable : RoofShape::FlatParapet, c.cul == CU_JADE ? RoofMat::ClayTile : RoofMat::Adobe,
            c.cul == CU_FJORD ? VF_POINTS : 0, 9);
  gateVol(c, 9, 6, 20, H, Face::Gate, c.cul == CU_JADE ? RoofShape::Pagoda : (flatRoof(c.st.roof) ? RoofShape::FlatParapet : RoofShape::Gable), c.st.roofMat == RoofMat::Felt ? RoofMat::Shingle : c.st.roofMat, 2, ew);
}

void tentForm(Bx& c) {
  // a tent: round felt (the steppe's ger) or a ridge tent of canvas, guy ropes
  if (c.cul == CU_STEPPE || c.W <= c.H + 16) {
    const int bi = roundHouse(c, 1, 18, true, 0);
    if (c.cul != CU_STEPPE) { c.v[(size_t)bi].roof = RoofShape::Tent; c.v[(size_t)bi].feat &= ~(uint32_t)VF_CROWN; c.v[(size_t)bi].feat |= VF_ROPES; }
    return;
  }
  Volume bo = c.vol(VolRole::Body, 4, 4, c.W - 4, c.H - 2, c.wallFor(1, 16), 1);
  roofTo(bo, RoofShape::Tent, RoofMat::Felt, 2, c.st.accentTint ? lerpC(c.st.accentTint, RGB(236, 226, 200), 45) : 0);
  bo.wall = WallMat::Felt; bo.feat |= VF_ROPES; bo.doorHere = true; bo.door = DoorShape::Flap; bo.ridgeNS = false;
  c.add(bo);
}

// ---------------------------------------------------------------- finishing
void ground(Bx& c) {
  if (c.grounded) return;
  for (const Volume& x : c.v) if (x.role == VolRole::Platform || (x.feat & VF_STILTS)) return;
  const art::Foundation f = c.st.foundation;
  if ((c.st.stilts || f == art::Foundation::Stilts) && c.k != PK::Temple && c.k != PK::Keep && c.k != PK::Palace && c.k != PK::Tower && c.k != PK::Windmill) {
    if (c.cul == CU_DUNE || c.cul == CU_STEPPE) return;
    stilts(c, 7);
    return;
  }
  if (f == art::Foundation::None || f == art::Foundation::Stilts || c.k == PK::Tower || c.k == PK::Windmill) return;
  const bool mon = c.k == PK::Keep || c.k == PK::Temple || c.k == PK::Palace;
  const int fh = f == art::Foundation::Platform ? (mon ? 8 : 5) : (f == art::Foundation::Terrace ? 4 : 3);
  const WallMat fm = (c.cul == CU_JADE || c.cul == CU_SUN || c.cul == CU_STAR || c.cul == CU_IMPERIAL) ? WallMat::Ashlar : (c.cul == CU_HIGHLAND ? WallMat::Rubble : WallMat::Stone);
  platformUnder(c, fh, fm, true, f == art::Foundation::Terrace ? 4 : (f == art::Foundation::Platform ? 3 : 2));
}

// keep every volume under the rise budget: spires and towers lose pitch, then wall height (never below their storeys)
void fitAll(Bx& c) {
  for (Volume& x : c.v) {
    if (x.role == VolRole::Chimney) continue;
    const int extra = (x.feat & VF_FLAG) ? 12 : ((x.roof == RoofShape::Spire || x.roof == RoofShape::Onion || x.roof == RoofShape::Conical) ? 6 : 2);
    const int lim = c.budget - extra;
    const int minWall = x.role == VolRole::Body ? std::max(16, (x.storeys <= 1 ? 16 : 22 + 14 * (x.storeys - 1)) - 4) : std::max(10, x.wallH - 24);
    for (int it = 0; it < 12 && riseOf(x) > lim; it++) {
      const int minPitch = x.roof == RoofShape::Spire ? 5 : 1;
      if (pitchedRoof(x.roof) && x.pitch > minPitch && (x.roof == RoofShape::Spire || x.role == VolRole::Tower || it % 2 == 0)) { x.pitch--; continue; }
      if (x.wallH > minWall) { x.wallH = (int16_t)std::max(minWall, (int)x.wallH - std::max(2, riseOf(x) - lim)); continue; }
      if (x.z0 > 0 && (x.role == VolRole::Tier || x.role == VolRole::Drum || x.role == VolRole::Wing || x.role == VolRole::Annex)) { x.z0 = (int16_t)std::max(0, x.z0 - 2); continue; }
      break;
    }
  }
}

void finish(Bx& c) {
  Blueprint& b = c.b;
  if (c.v.empty() || c.v[0].role != VolRole::Body) {   // (never: every assembler starts with its body)
    Volume bo = c.vol(VolRole::Body, 0, 0, c.W, c.H, c.wallFor(c.want, 26), c.want);
    c.v.insert(c.v.begin(), bo);
  }
  // exactly one door volume, spanning the door column, its front on the footprint's bottom row
  int doors = 0;
  for (Volume& x : c.v) {
    if (!x.doorHere) continue;
    const bool ok = c.dc >= x.x0 && c.dc < x.x1 && x.y1 >= c.H - 16 && doors == 0;
    if (ok) doors++; else x.doorHere = false;
  }
  if (!doors) {
    int best = -1;
    for (int i = 0; i < (int)c.v.size(); i++) {
      const Volume& x = c.v[(size_t)i];
      if (x.role == VolRole::Chimney || x.role == VolRole::Enclosure || x.role == VolRole::Tree || x.role == VolRole::Platform) continue;
      if (c.dc >= x.x0 && c.dc < x.x1 && x.y1 >= c.H - 16 && (best < 0 || x.y1 > c.v[(size_t)best].y1)) best = i;
    }
    if (best >= 0) c.v[(size_t)best].doorHere = true;
    else {   // a doorway volume over the door column
      Volume d = c.vol(VolRole::Porch, c.dc - 8, c.H - 8, c.dc + 8, c.H, std::min(18, (int)c.v[0].wallH));
      d.face = Face::Gate; d.doorHere = true; d.roof = flatRoof(c.v[0].roof) ? RoofShape::FlatParapet : RoofShape::Gable; d.ridgeNS = true;
      if (d.roof == RoofShape::FlatParapet) d.roofMat = RoofMat::Adobe;
      c.add(d);
    }
  }
  // (owner) an open entrance (colonnade, arcade, veranda, iwan) is walked into between its pillars: wide enough at the
  // door column for a person to pass its end pillars (openPillars puts them at the volume's ends)
  for (Volume& x : c.v) {
    if (!x.doorHere || !openFaceKind(x.face) || x.shape != VolShape::Box) continue;
    const int half = x.face == Face::Iwan ? 11 : 10;
    x.x0 = (int16_t)std::min((int)x.x0, c.dc - half);
    x.x1 = (int16_t)std::max((int)x.x1, c.dc + half);
  }
  // the volumes inside the sprite's pads
  for (Volume& x : c.v) {
    x.x0 = (int16_t)std::max(-art::BLDG_PAD_X, (int)x.x0);
    x.x1 = (int16_t)std::min(c.W + art::BLDG_PAD_X, (int)x.x1);
    x.y1 = (int16_t)std::min(c.H + art::BLDG_PAD_B, (int)x.y1);
    if (x.x1 <= x.x0) x.x1 = (int16_t)(x.x0 + 1);
    if (x.y1 <= x.y0) x.y0 = (int16_t)(x.y1 - 1);
    x.storeys = (uint8_t)std::clamp((int)x.storeys, 1, 4);
  }
  ground(c);
  fitAll(c);
  const Volume& bo = c.v[0];
  b.facts.storeys = bo.storeys;
  // the form, from what was built
  if (c.r.form != Form::Auto) b.form = c.r.form;
  else {
    bool encl = false, court = false;
    int wings = 0;
    for (const Volume& x : c.v) { if (x.role == VolRole::Enclosure) encl = true; if (x.role == VolRole::Platform && x.wallH <= 1) court = true; if (x.role == VolRole::Wing && x.storeys >= 1) wings++; }
    if (encl) b.form = Form::Compound;
    else if (court) b.form = Form::Courtyard;
    else if (bo.roof == RoofShape::Stepped) b.form = Form::Stepped;
    else if (bo.roof == RoofShape::Tent) b.form = Form::Tent;
    else if (bo.shape != VolShape::Box) b.form = (bo.storeys >= 3 && bo.wallH >= 50) ? Form::Tower : Form::Round;
    else if (bo.storeys >= 3 || (bo.wallH >= 50 && bo.x1 - bo.x0 <= 48)) b.form = Form::Tower;
    else {
      bool L = false;
      for (const Volume& x : c.v) if (x.role == VolRole::Wing && x.ridgeNS && x.y1 >= bo.y1 + 6 && x.storeys == bo.storeys) L = true;
      if (L) b.form = Form::L;
      else if (!bo.ridgeNS && bo.x1 - bo.x0 >= 2 * (bo.y1 - bo.y0) + 16) b.form = Form::Long;
      else b.form = Form::Rect;
    }
  }
  if (b.form == Form::Tent || b.form == Form::Auto) { if (b.form == Form::Auto) b.form = Form::Rect; }
  // the interior: the floor plan of the body
  InteriorShape& in = b.interior;
  in.floors = bo.storeys;
  in.furniture = c.st.furniture;
  in.culture = c.st.culture;
  switch (b.form) {
    case Form::Round: case Form::Tent: in.plan = bo.shape != VolShape::Box ? Floorplan::Round : Floorplan::Rect; break;
    case Form::Courtyard: {
      in.plan = Floorplan::Courtyard;
      for (const Volume& x : c.v)
        if (x.role == VolRole::Platform && x.wallH <= 1) {
          in.courtW = (uint8_t)std::clamp((x.x1 - x.x0) * 16 / std::max(1, c.W), 2, 12);
          in.courtH = (uint8_t)std::clamp((x.y1 - x.y0) * 16 / std::max(1, c.H), 2, 12);
        }
      if (!in.courtW) { in.courtW = 6; in.courtH = 6; }
      break;
    }
    case Form::L: {
      in.plan = Floorplan::L;
      bool westWing = false;
      for (const Volume& x : c.v) if (x.role == VolRole::Wing && x.ridgeNS && x.y1 >= bo.y1 + 6) westWing = x.x0 < bo.x0;
      in.lCorner = westWing ? 3 : 2;   // the yard is the front corner on the other side
      break;
    }
    case Form::Long: in.plan = Floorplan::Long; break;
    case Form::Stepped: in.plan = Floorplan::Cross; break;
    case Form::Compound: in.plan = bo.shape != VolShape::Box ? Floorplan::Round : (bo.x1 - bo.x0 >= 2 * (bo.y1 - bo.y0) + 16 ? Floorplan::Long : Floorplan::Rect); break;
    case Form::Tower: in.plan = bo.shape != VolShape::Box ? Floorplan::Round : Floorplan::Rect; break;
    default: in.plan = bo.shape != VolShape::Box ? Floorplan::Round : Floorplan::Rect; break;
  }
  if (bo.shape != VolShape::Box) in.plan = Floorplan::Round;   // (a round body is always a round interior)
  // the sign
  Signage& sg = b.sign;
  auto sk = [&](Sign k, uint8_t icon) { sg.kind = k; sg.icon = icon; };
  const bool lanternPeople = c.cul == CU_JADE || c.cul == CU_MARSH;
  switch (c.P) {
    case Building::Inn: sk(lanternPeople ? Sign::Lantern : (c.cul == CU_STEPPE || c.cul == CU_DUNE ? Sign::Banner : (c.cul == CU_FJORD ? Sign::Carved : Sign::Hanging)), ICON_MUG); break;
    case Building::Smithy: sk(c.cul == CU_STEPPE ? Sign::Totem : Sign::Hanging, ICON_ANVIL); break;
    case Building::Smelter: sk(Sign::Hanging, ICON_INGOT); break;
    case Building::Shop: sk(Sign::Awning, ICON_PURSE); break;
    case Building::Bakery: sk(Sign::Awning, ICON_LOAF); break;
    case Building::Butcher: sk(Sign::Awning, ICON_HAM); break;
    case Building::Fishmonger: sk(Sign::Awning, ICON_FISH); break;
    case Building::Weaver: sk(Sign::Awning, ICON_YARN); break;
    case Building::Tanner: sk(Sign::Hanging, ICON_HIDE); break;
    case Building::Granary: sk(Sign::Hanging, ICON_SHEAF); break;
    case Building::Sawmill: sk(Sign::Hanging, ICON_SAW); break;
    case Building::Watermill: case Building::Windmill: sk(Sign::Hanging, ICON_SACK); break;
    case Building::Guildhall: sk(Sign::Banner, ICON_KEY); break;
    case Building::Exchange: sk(Sign::Board, ICON_SCALES); break;
    case Building::MeadHall: sk(c.cul == CU_FJORD || c.cul == CU_HIGHLAND ? Sign::Carved : Sign::Banner, ICON_HORN); break;
    case Building::Bathhouse: sk(lanternPeople ? Sign::Lantern : Sign::Banner, ICON_STEAM); break;
    case Building::TeaHouse: sk(Sign::Lantern, ICON_CUP); break;
    case Building::Lodge: sk(c.cul == CU_STEPPE ? Sign::Totem : Sign::Banner, ICON_SHIELD); break;
    case Building::CouncilHall: sk(Sign::Carved, c.cul == CU_STAR || c.cul == CU_SYLVAN ? ICON_STAR : ICON_CIRCLE); break;
    default: break;
  }
  // the yard
  if (c.k == PK::Home && c.wl == 0 && c.cul != CU_STEPPE) b.yard = c.cul == CU_HIGHLAND ? Yard::Wall : Yard::Fence;
  else if (c.k == PK::Farm) b.yard = Yard::Fence;
  for (const Volume& x : c.v) if (x.role == VolRole::Enclosure) b.yard = (x.feat & VF_POINTS) ? Yard::Stockade : Yard::Wall;
  // how tall the picture is (ground shadows)
  int hmax = 0;
  for (const Volume& x : c.v) hmax = std::max(hmax, x.z0 + x.wallH + roofHeight(x) + 6 + c.H - x.y1);
  b.heightPx = hmax;
}

}  // namespace

Blueprint design(const Request& r) {
  Blueprint b;
  b.req = r;
  b.style = r.style;
  b.facts = r.facts;
  Bx c(r, b);
  b.culture = (uint8_t)c.cul;
  const int asked = r.facts.storeys > 0 ? r.facts.storeys : art::defaultStoreys(r.purpose);
  c.want = std::clamp(asked, 1, 4);
  c.budget = art::riseBudgetTiles(r.purpose, asked) * 16;
  // the seat of power: asked by the society, or the culture's own for a palace / a lord's keep
  c.seat = r.seat;
  if (!c.seat && (c.k == PK::Palace || c.k == PK::Keep)) c.seat = cultureSeat(c.cul);
  if (c.seat && !(c.k == PK::Palace || c.k == PK::Keep || (r.civic & CIVIC_SEAT))) c.seat = 0;
  c.lord = c.k != PK::Palace;
  b.seat = (uint8_t)c.seat;
  // the form asked by the settlement (it may only change what the form can hold: tents 1 storey, towers 2+)
  const Form asks = r.form;
  if (asks == Form::Tent) c.want = 1;
  if (asks == Form::Tower) c.want = std::max(2, c.want);
  const bool seatKind = c.seat && (c.k == PK::Palace || c.k == PK::Keep || (r.civic & CIVIC_SEAT));
  if (seatKind && (c.k == PK::Palace || c.k == PK::Keep)) seat(c);
  else if (asks == Form::Compound && c.k != PK::Palace && c.k != PK::Keep && !c.seat) genericCompound(c);
  else if (asks == Form::Tent && c.k != PK::Palace) tentForm(c);
  else if (asks == Form::Courtyard && c.k != PK::Palace && c.k != PK::Keep && c.W >= 48 && c.H >= 48) {
    const int bi = courtHouse(c, 24, flatRoof(c.st.roof) || c.cul == CU_DUNE || c.cul == CU_SUN, c.want);
    chimneys(c, bi, 1);
  } else if (asks == Form::Round && c.k != PK::Palace && c.k != PK::Keep && c.k != PK::Temple && c.seat == 0) {
    const int bi = roundHouse(c, c.want, 22, c.cul == CU_STEPPE, 0);
    if (c.cul == CU_SYLVAN) { c.v[(size_t)bi].wall = WallMat::Living; c.v[(size_t)bi].roof = RoofShape::Sweep; }
  } else if (asks == Form::Long && (c.k == PK::Home || c.k == PK::MeadHall || c.k == PK::Lodge || c.k == PK::Inn || c.k == PK::Barracks)) {
    const int bi = longHall(c, 26, c.want, 0);
    chimneys(c, bi, 2);
  } else if (asks == Form::Stepped && c.seat == 0 && c.k != PK::Palace) {
    c.cul = CU_SUN;
    temple(c);
  } else if (c.seat && (c.k == PK::Palace || c.k == PK::Keep || (r.civic & CIVIC_SEAT))) {
    // the seat of power in the culture's idiom (a lord's seat smaller); a lord's seat asked as a mead hall, council
    // hall, temple or guildhall keeps its purpose's hall, grander
    if (c.k == PK::Palace || c.k == PK::Keep) seat(c);
    else {
      switch (c.k) {
        case PK::MeadHall: case PK::Lodge: greatHall(c, false); break;
        case PK::Council: greatHall(c, true); break;
        case PK::Temple: temple(c); break;
        case PK::Guildhall: guildhall(c); break;
        default: cultureHome(c); break;
      }
      for (Volume& x : c.v) if (x.role == VolRole::Body) x.feat |= VF_BANNERS;
      c.v[0].ornament |= art::ORN_GILDING;
    }
  } else {
    switch (c.k) {
      case PK::Home: cultureHome(c); break;
      case PK::Hut: hut(c); break;
      case PK::Farm: farm(c); break;
      case PK::Shop: shop(c); break;
      case PK::Inn: inn(c); break;
      case PK::Smithy: smithy(c); break;
      case PK::Temple: temple(c); break;
      case PK::Keep: case PK::Palace: seat(c); break;
      case PK::Tower: mageTower(c); break;
      case PK::Windmill: windmill(c); break;
      case PK::Watermill: watermill(c); break;
      case PK::Barracks: barracks(c); break;
      case PK::Guildhall: guildhall(c); break;
      case PK::Exchange: exchange(c); break;
      case PK::MeadHall: greatHall(c, false); break;
      case PK::Bath: bath(c); break;
      case PK::TeaHouse: teaHouse(c); break;
      case PK::Lodge: greatHall(c, false); if (!c.v.empty()) c.v[0].feat |= VF_BANNERS; break;
      case PK::Council: greatHall(c, true); break;
    }
  }
  if (asks == Form::Tent) for (Volume& x : c.v) if (x.role == VolRole::Body) x.storeys = 1;
  // (fix) small homes had too few massing choices on a 3x2 or 3x3 plot, so neighbours came out as clones: each
  // household dresses its front its own way, in its people's manner (shutters, a flower box, a vine, lanterns, painted
  // bands, a carved ridge...; arched or screened windows in the south), from its own seed (no draw from the shared
  // stream, so nothing else about the town changes). A yurt household varies by its camp instead.
  if (c.k == PK::Home && !c.seat && !c.v.empty() && c.cul != CU_STEPPE) {
    const uint32_t h = mix32(r.seed * 0x2C1B3C6Du ^ 0x5EED0F7u);
    Volume& B = c.v[0];
    uint16_t k[3] = {art::ORN_SHUTTERS, art::ORN_FLOWERBOX, art::ORN_VINES};
    switch (c.cul) {
      case CU_DUNE: k[0] = art::ORN_PAINTED_BANDS; break;
      case CU_SUN: k[0] = art::ORN_PAINTED_BANDS; break;
      case CU_JADE: k[0] = art::ORN_LANTERNS; k[2] = art::ORN_CARVED_RIDGE; break;
      case CU_MARSH: case CU_SYLVAN: k[0] = art::ORN_LANTERNS; break;
      case CU_FJORD: k[2] = art::ORN_CARVED_RIDGE; break;
      case CU_RIVER: k[2] = art::ORN_PAINTED_BANDS; break;
      case CU_STAR: k[0] = art::ORN_FINIALS; break;
      default: break;
    }
    for (int i = 0; i < 3; i++) if ((h >> i) & 1) B.ornament ^= k[i];
    // (M3b round 3) and its massing too: two River houses side by side differed only in their awning's colour. The
    // chimney stands at either end, a pitched roof may turn its gable to the street (in its people's gable outline),
    // and a roof with its ridge across may carry a dormer
    if (B.shape == VolShape::Box && (B.roof == art::RoofShape::Gable || B.roof == art::RoofShape::Hip)) {
      if ((h >> 4) & 1)
        for (Volume& x : c.v)
          if (x.role == VolRole::Chimney) {
            const int16_t nx0 = (int16_t)(B.x0 + B.x1 - x.x1), nx1 = (int16_t)(B.x0 + B.x1 - x.x0);
            x.x0 = nx0; x.x1 = nx1;
          }
      const bool gableLand = c.cul == CU_RIVER || c.cul == CU_HEART || c.cul == CU_HIGHLAND || c.cul == CU_FJORD;
      if (((h >> 5) & 3) == 0 && gableLand && B.x1 - B.x0 <= 64 && !B.ridgeNS) {
        B.ridgeNS = true;
        B.roof = art::RoofShape::Gable;
        B.dormers = 0;
        if (c.cul == CU_RIVER) B.gable = (uint8_t)(1 + (h >> 7) % 3u);
      } else if (!B.ridgeNS && ((h >> 9) & 3) == 0 && B.x1 - B.x0 >= 48) {
        B.dormers = B.dormers ? 0 : 1;
      }
    }
    if (((h >> 3) & 1) && (c.cul == CU_IMPERIAL || c.cul == CU_DUNE))
      B.window = c.cul == CU_IMPERIAL ? (B.window == WindowShape::Arched ? WindowShape::Square : WindowShape::Arched)
                                      : (B.window == WindowShape::Screen ? WindowShape::Arched : WindowShape::Screen);
  }
  finish(c);
  b.key = r.key() ^ ((uint64_t)b.form << 59);
  return b;
}

const char* validate(const Blueprint& b) {
  if (b.vols.empty()) return "no volumes";
  if (b.vols[0].role != VolRole::Body) return "the first volume is not the body";
  if (b.form == Form::Auto || b.form >= Form::COUNT) return "unresolved form";
  int doors = 0;
  const int W = b.req.wTiles * 16, H = b.req.hTiles * 16;
  const int dc = (b.req.wTiles / 2) * 16 + 8;   // the door column's centre px
  for (const Volume& v : b.vols) {
    if (v.x1 <= v.x0 || v.y1 <= v.y0) return "an empty volume";
    if (v.x0 < -art::BLDG_PAD_X || v.x1 > W + art::BLDG_PAD_X || v.y1 > H + art::BLDG_PAD_B) return "a volume outside the sprite's pads";
    if (v.storeys < 1 || v.storeys > 4) return "storeys out of 1..4";
    if (v.role == VolRole::Chimney && !b.req.facts.hearth) return "a chimney without a hearth";
    if (v.doorHere) {
      doors++;
      if (dc < v.x0 || dc >= v.x1) return "the door volume does not span the door column";
      if (v.y1 < H - 16) return "the door volume's front is not on the footprint's bottom row";
    }
  }
  if (doors != 1) return "not exactly one door volume";
  if (b.interior.floors != b.vols[0].storeys) return "interior floors differ from the body's storeys";
  if (b.facts.storeys != b.vols[0].storeys) return "facts.storeys differ from the body's storeys";
  if (b.interior.plan >= Floorplan::COUNT) return "bad interior plan";
  if (b.vols[0].shape != VolShape::Box && b.interior.plan != Floorplan::Round) return "a round body without a round interior";
  if (b.req.form == Form::Tent && b.vols[0].storeys != 1) return "a tent of more than one storey";
  // (owner) an open front is entered between its pillars: the door column must be a walk-in bay
  const OpenFront of = openFront(b);
  if (of.open() && !(of.gaps & (1u << (b.req.wTiles / 2)))) return "an open front without a walk-in bay at the door column";
  return "";
}

// ---------------------------------------------------------------- open fronts
bool openFaceKind(Face f) { return f == Face::Colonnade || f == Face::Arcade || f == Face::Veranda || f == Face::Iwan; }

void openPillars(const Volume& v, int doorX, std::vector<Pillar>& out) {
  out.clear();
  if (!openFaceKind(v.face)) return;
  const bool round = v.shape != VolShape::Box;
  int X0 = v.x0, X1 = v.x1;
  if (round) {   // the face of a round volume: the inscribed disc's width (as the painter's facade)
    const int d = std::min(v.x1 - v.x0, v.y1 - v.y0);
    X0 = (v.x0 + v.x1 - d) / 2; X1 = X0 + d;
  }
  const int w = X1 - X0;
  if (w <= 0) return;
  if (v.face == Face::Iwan) {   // the recess's two jambs (art_building.cpp iwanFace: a frame up to 26 px, 3 px jambs)
    const int du = (doorX >= X0 + 4 && doorX < X1 - 4) ? doorX - X0 : w / 2;
    const int fw = std::min(w - 4, 26), u0 = du - fw / 2, u1 = u0 + fw;
    out.push_back(Pillar{(int16_t)(X0 + u0), (int16_t)(X0 + u0 + 3)});
    out.push_back(Pillar{(int16_t)(X0 + u1 - 3), (int16_t)(X0 + u1)});
    return;
  }
  const int pw = v.face == Face::Veranda ? 2 : 3;
  out.push_back(Pillar{(int16_t)X0, (int16_t)(X0 + pw)});
  // a pillar on each tile boundary b (centred: [b - 1, b - 1 + pw)) at least 8 px clear of the end pillars
  for (int b = ((X0 + pw + 8) / 16) * 16; b < X1; b += 16) {
    const int p0 = b - 1, p1 = p0 + pw;
    if (p0 - (X0 + pw) < 8 || (X1 - pw) - p1 < 8) continue;
    out.push_back(Pillar{(int16_t)p0, (int16_t)p1});
  }
  out.push_back(Pillar{(int16_t)(X1 - pw), (int16_t)X1});
}

OpenFront openFront(const Blueprint& b) {
  OpenFront o;
  const int wt = std::max(1, (int)b.req.wTiles), H = std::max(1, (int)b.req.hTiles) * 16, dcol = wt / 2, doorX = dcol * 16 + 8;
  const int n = (int)b.vols.size();
  auto raisedV = [](const Volume& v) { return ((v.feat & VF_STILTS) && v.z0 > 0) || v.z0 > 6; };
  auto onFront = [&](const Volume& v) { return v.y1 >= H - 16; };
  auto occluder = [](const Volume& v) {
    return v.role != VolRole::Chimney && v.role != VolRole::Tree && v.role != VolRole::Platform && !openFaceKind(v.face);
  };
  // the open volume a person meets first at front-row column t (its centre), or -1 (a closed wall stands in front, or
  // nothing open there)
  auto frontAt = [&](int t) {
    const int cx = t * 16 + 8;
    int best = -1;
    for (int i = 0; i < n; i++) {
      const Volume& v = b.vols[(size_t)i];
      if (v.role == VolRole::Platform || v.role == VolRole::Chimney || v.role == VolRole::Tree) continue;   // (steps and plinths lead up)
      if (v.shape != VolShape::Box || cx < v.x0 || cx >= v.x1) continue;
      if (best < 0 || v.y1 > b.vols[(size_t)best].y1) best = i;
    }
    if (best < 0) return -1;
    const Volume& v = b.vols[(size_t)best];
    if (!openFaceKind(v.face) || occluder(v) || !onFront(v)) return -1;
    return best;
  };
  // the entrance: the door volume when it is open, else an open volume standing before the door column (a portico
  // before a hall): either way no door is painted
  const int dv = b.doorVol();
  if (dv >= 0 && dv < n && b.vols[(size_t)dv].doorHere && openFaceKind(b.vols[(size_t)dv].face)) o.vol = dv;
  else if (const int f = frontAt(dcol); f >= 0) o.vol = f;
  if (o.vol >= 0) {
    const Volume& v = b.vols[(size_t)o.vol];
    o.face = v.face;
    o.raised = raisedV(v);
    if (o.raised) {   // the ladder or the steps at the door column only
      openPillars(v, doorX, o.pillars);
      o.gaps = 1u << std::min(31, dcol);
      o.bays = 1;
      return o;
    }
  }
  // every front-row column whose frontmost volume is an open face on the ground: walked in between its pillars
  std::vector<std::vector<Pillar>> pil((size_t)n);
  std::vector<char> made((size_t)n, 0);
  std::vector<char> used((size_t)n, 0);
  for (int t = 0; t < std::min(32, wt); t++) {
    const int f = frontAt(t);
    if (f < 0 || raisedV(b.vols[(size_t)f])) continue;
    if (!made[(size_t)f]) { openPillars(b.vols[(size_t)f], doorX, pil[(size_t)f]); made[(size_t)f] = 1; }
    const std::vector<Pillar>& P = pil[(size_t)f];
    if (P.size() < 2) continue;
    // the bays of this volume within the tile
    bool entry = false;
    for (size_t i = 0; i + 1 < P.size(); i++) {
      const int a = std::max((int)P[i].x1, t * 16), z = std::min((int)P[i + 1].x0, t * 16 + 16);
      if (z - a >= OPEN_MIN_BAY) entry = true;
    }
    if (!entry) continue;
    o.gaps |= 1u << t;
    used[(size_t)f] = 1;
    uint16_t m = 0;
    for (int px = 0; px < 16; px++) {
      const int x = t * 16 + px;
      bool solid = x < P.front().x0 || x >= P.back().x1;
      for (const Pillar& q : P) if (x >= q.x0 && x < q.x1) solid = true;
      if (solid) m |= (uint16_t)(1u << px);
    }
    o.solid[(size_t)t] = m;
  }
  for (int i = 0; i < n; i++)
    if (used[(size_t)i]) {
      for (const Pillar& q : pil[(size_t)i]) o.pillars.push_back(q);
      for (size_t k = 0; k + 1 < pil[(size_t)i].size(); k++) if (pil[(size_t)i][k + 1].x0 - pil[(size_t)i][k].x1 >= OPEN_MIN_BAY) o.bays++;
    }
  std::sort(o.pillars.begin(), o.pillars.end(), [](const Pillar& a, const Pillar& c) { return a.x0 < c.x0; });
  return o;
}

}  // namespace bld
