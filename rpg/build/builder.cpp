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
// SOLID 1.6 split: build_detail.h (Bx, shared helpers), build_geom.cpp (roof and volume helpers), build_forms.cpp
// (per-purpose forms), build_culture.cpp (culture homes, seats), build_finish.cpp (ground, terrace, finish).
#include "rpg/build/blueprint.h"
#include "rpg/build/build_detail.h"
#include <algorithm>
#include <cstdint>

namespace bld {

namespace {
uint64_t mix(uint64_t k, uint64_t v) {
  k ^= v + 0x9E3779B97F4A7C15ull + (k << 6) + (k >> 2);
  k *= 0xBF58476D1CE4E5B9ull;
  return k ^ (k >> 31);
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
  if (home) k = mix(k, 0x4803ull << 8 | (uint64_t)home);   // (M7) 0 keeps every key as before
  if (row) k = mix(k, 0x7B0Aull << 8 | (uint64_t)row);     // (M7b) 0 keeps every key as before
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
  } else if (asks == Form::Long && c.cul == CU_STEPPE && r.home && c.k == PK::Home) {
    // (M7) the player's longhouse in the steppe manner: a long feast tent of felt on its ridge poles and guy ropes (a
    // box under a yurt's cone was no building at all), a small ger pitched against its east end
    Volume bo = c.vol(VolRole::Body, 2, std::max(2, c.H / 8), c.W - (c.W >= 112 ? 22 : 4), c.H - 1, c.wallFor(1, 18), 1);
    // (M7 fix) its felt pitched over a ridge pole down its length and hipped at the ends (a tent cone over so long a
    // box read as one flat beige plane: no ridge, no lit and shaded ends)
    roofTo(bo, RoofShape::Hip, RoofMat::Felt, 3, c.st.roofTint);
    bo.wall = WallMat::Felt; bo.feat |= VF_ROPES; bo.doorHere = true; bo.door = DoorShape::Flap; bo.ridgeNS = false; bo.face = Face::Windows;
    bo.ornament |= art::ORN_PAINTED_BANDS;
    const int bi = c.add(bo);
    if (c.W >= 112) {
      Volume g = c.rnd(VolRole::Annex, c.W - 12, c.H - 2, 11, 13);
      g.wall = WallMat::Felt; roofTo(g, RoofShape::Conical, RoofMat::Felt, 2, c.st.roofTint); g.feat |= VF_CROWN; g.eave = 0;
      g.window = WindowShape::Round; g.door = DoorShape::Flap;
      c.add(g);
    }
    (void)bi;
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
