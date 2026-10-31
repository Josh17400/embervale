// Builder finishing (SOLID 1.6, cut from builder.cpp): ground() lays the foundations, terrace() and streetFaces()
// make a row house one unit of a frontage, fitAll() keeps the rise budget, and finish() completes every blueprint.
#include "rpg/build/build_detail.h"

namespace bld {

// ---------------------------------------------------------------- finishing
void ground(Bx& c) {
  if (c.grounded) return;
  for (const Volume& x : c.v) if (x.role == VolRole::Platform || (x.feat & VF_STILTS)) return;
  art::Foundation f = c.st.foundation;
  // (M7 fixer r2, review: "player houses stop straight on the grass") the player's own house is built properly: on a
  // stone footing even where the culture's cottages often go without (a felt yurt and a stilt house keep their own)
  if (c.r.home && f == art::Foundation::None && !c.st.stilts && c.cul != CU_STEPPE) f = art::Foundation::Plinth;
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
namespace {

// (M7b CITYART) a row house in a continuous frontage (Request::row, VISION_PLAN 15.23): the body runs to the footprint's
// edge on each shared side (bld::ROW_W / ROW_E) and carries the party bit there (VF_PARTY_*: the painter stops its roof
// flush on the shared wall, unhipped, and draws nothing past that edge), its foundation runs on to the same edge, and
// whatever stood out on a shared side (a side wing, an annex, a porch, a stair turret) is not built: the neighbour's
// house stands there. Hipped and swept roofs with the ridge along the street become gables of the same material (their
// ridge and eaves run on along the terrace); a jettied upper floor keeps its jetty only on a free-standing house (the
// ground floor's inset left a notch at every joint). Each house of a terrace keeps its own facade colour (a limewash
// or a paint of its people's by its seed), and in a rich quarter window boxes and shutters. ROW_YARD: a low wall along
// the back of the plot (the yard behind is walled by the city's garden walls). No draw from the builder's stream: a
// pure function of the request, so every other choice (and every free-standing building) is unchanged.
// (M7c ART, VISION_PLAN 15.M7c) a row house's other street faces and its depth, after terrace() made it one unit of a
// row (Request::row ROW_SIDE_W / ROW_SIDE_E / ROW_BACK; no draw from the builder's stream, a pure function of the
// request). The painter dresses a street side as an elevation from Blueprint::req.row (its street-end quoins, a window
// in the end bay of every storey, the roof hipped down to the side street); here the builder adds what has volume:
//  - a deep plot (a body deeper than 52 px: blocks of row houses round shared yards) is a double pile: the front range
//    under its ridge along the street and a lower rear range behind it under its own parallel roof, so a deep terrace
//    never reads as one great roof slab, and the rear range's roof is what the street behind sees;
//  - a corner house (a street side) in a city of pitched roofs, two storeys or more, carries a corbelled oriel at its
//    street corner from the first floor up: a round turret under a cone for the well-off, a box bay under a little hip
//    roof for the rest (the flat-roofed peoples a closed box balcony under a flat top), turning the corner as a
//    commercial tileset's corner piece does.
void streetFaces(Bx& c, bool pw, bool pe, bool sideW, bool sideE, bool back) {
  if (c.v.empty() || c.v[0].shape != VolShape::Box) return;
  Volume& B = c.v[0];
  const bool pitched = pitchedRoof(B.roof);
  const int n = std::clamp((int)B.storeys, 1, 4);
  // the double pile
  const int depth = B.y1 - B.y0;
  if (pitched && depth > 52) {
    const int front = std::clamp(depth / 2 + 6, 34, 46);
    Volume R = B;
    R.role = VolRole::Wing;
    R.doorHere = false;
    R.y1 = (int16_t)(B.y1 - front + 2);
    R.storeys = (uint8_t)std::max(1, n - 1);
    R.wallH = (int16_t)std::max(16, (int)B.wallH - 12);
    R.face = Face::Windows;
    R.dormers = 0;
    R.feat &= (uint32_t)(VF_PARTY_W | VF_PARTY_E);
    R.ornament &= (uint16_t)~(art::ORN_FLOWERBOX);
    B.y0 = (int16_t)(B.y1 - front);
    if (B.dormers && (B.y1 - B.y0) < 44) B.dormers = 0;
    // its stacks: on the front range's ridge or the rear range's, wherever they stood
    for (Volume& x : c.v)
      if (x.role == VolRole::Chimney && x.y0 < B.y0) {
        const int d = x.y1 - x.y0;
        if (x.y1 > R.y1 - 2) { x.y0 = (int16_t)(B.y0 + 2); x.y1 = (int16_t)(x.y0 + d); }
      }
    c.add(R);
  }
  // the corner turret: one corner house in three of the well-off (two in three of the rich), so a side street lined
  // by the ends of a block's rows is not a parade of turrets; a flat-roofed people's corner house a closed balcony
  if ((sideW || sideE) && n >= 2 && c.W >= 32 && c.r.purpose != Building::Inn && c.wl >= 2) {
    const bool east = sideE;   // (a house with two street sides turns its east corner)
    const int floor1 = std::min((int)B.wallH - 10, 24);
    const int up = (int)B.wallH - floor1;
    const uint32_t h = mix32(c.r.seed * 0x2C1B3C6Du ^ 0x0E1E7u);
    const bool want = (h % 3u) < (c.wl >= 3 ? 2u : 1u);
    // (M7c fixer, review: "terrace and house roof ends draw as a melted, rounded bulge overhanging the wall"; "a dark
    // cone wedged into its roof slope at the corner") a corbelled turret this small sank into the main roof: its cone
    // met the eave and the slope and drew as a blob with a spike, not a turret. The pitched-roof corner house turns
    // its corner with its dressed street side (quoins, the end bay's windows, the roof hipped to the side street)
    constexpr bool kCornerTurret = false;
    if (kCornerTurret && want && pitched && up >= 12 && (c.cul == CU_HEART || c.cul == CU_HIGHLAND || c.cul == CU_RIVER || c.cul == CU_STAR || c.cul == CU_NONE || c.cul == CU_FJORD)) {
      const int rad = 6;
      const int cx = east ? c.W - rad + 2 : rad - 2;
      Volume t = c.rnd(VolRole::Tower, cx, c.H + 1, rad, up + 6, 1);
      t.z0 = (int16_t)floor1;
      t.roof = RoofShape::Conical;
      t.pitch = 4;
      t.roofMat = B.roofMat == RoofMat::Thatch ? RoofMat::Shingle : B.roofMat;
      t.wall = B.wall;
      t.wallTint = B.wallTint;
      t.face = Face::Windows;
      t.feat = 0;
      t.ornament = B.ornament & (uint16_t)~(art::ORN_FLOWERBOX | art::ORN_VINES);
      c.add(t);
    } else if (want && !pitched && up >= 10 && (c.cul == CU_DUNE || c.cul == CU_SUN || c.cul == CU_IMPERIAL)) {
      const int bw = 14;   // a closed timber balcony (a mashrabiya) on the street corner, under a flat top
      Volume o = c.vol(VolRole::Annex, east ? c.W - bw + 1 : -1, c.H - 8, east ? c.W + 1 : bw - 1, c.H + 2, std::min(up - 2, 14), 1);
      o.z0 = (int16_t)floor1;
      o.roof = RoofShape::FlatParapet; o.roofMat = RoofMat::Adobe; o.pitch = 0;
      o.wall = WallMat::Plank;
      o.face = Face::Windows;
      o.window = WindowShape::Lattice;
      c.add(o);
    }
  }
  (void)pw; (void)pe; (void)back;
}

void terrace(Bx& c) {
  const uint8_t row = c.r.row;
  if (!row || c.v.empty() || c.v[0].shape != VolShape::Box) return;
  // (M7c ART) a side that looks onto a street is never a party wall (ROW_SIDE_* wins over a stray ROW_W / ROW_E)
  const bool pw = (row & ROW_W) != 0 && !(row & ROW_SIDE_W), pe = (row & ROW_E) != 0 && !(row & ROW_SIDE_E);
  const bool sideW = (row & ROW_SIDE_W) != 0, sideE = (row & ROW_SIDE_E) != 0, back = (row & ROW_BACK) != 0;
  const int bx0 = c.v[0].x0, bx1 = c.v[0].x1, by1 = c.v[0].y1;
  // what stands out on a shared side goes (never the door's volume, the body, a chimney or the foundation)
  std::vector<Volume> keep;
  keep.reserve(c.v.size());
  for (size_t i = 0; i < c.v.size(); i++) {
    const Volume& x = c.v[i];
    const bool side = (pw && x.x1 <= bx0 + 2) || (pe && x.x0 >= bx1 - 2) || (pw && x.x0 < 0) || (pe && x.x1 > c.W);
    const bool removable = i > 0 && x.role != VolRole::Chimney && x.role != VolRole::Platform && x.role != VolRole::Body;
    if ((side && removable && !x.doorHere) || (x.role == VolRole::Porch && i > 0)) continue;   // (a row's door opens on the walk)
    keep.push_back(x);
  }
  c.v.swap(keep);
  // (fixer M7b r3, review: "the eave jumps 1-2 px at every party wall, the courses don't line up, hipped units alternate
  // with ridged ones, two-tone halves") a terrace is one roof: only the body (and its chimneys, foundation and yard
  // wall) is built, to a measure that is a function of what the units of one terrace share (the people's style, the
  // depth, the storeys; never the house's own seed): the same wall height, depth, pitch, roof form and covering, so the
  // eave line, the ridge and the tile courses run straight on from house to house
  {
    std::vector<Volume> one;
    one.reserve(c.v.size());
    for (size_t i = 0; i < c.v.size(); i++) {
      const Volume& x = c.v[i];
      if (i == 0 || x.role == VolRole::Chimney || x.role == VolRole::Platform || x.role == VolRole::Enclosure) one.push_back(x);
    }
    c.v.swap(one);
    Volume& B0 = c.v[0];
    B0.doorHere = true;
    B0.x0 = 0;
    B0.x1 = (int16_t)c.W;
    // (M7c ART) a back on a street (ROW_BACK): the back wall stands on the plot's back line, along the street's walk
    B0.y0 = (int16_t)(back ? 0 : (c.H >= 48 ? 8 : 2));
    B0.y1 = (int16_t)c.H;
    const int n = std::clamp((int)B0.storeys, 1, 4);
    // ((M7c fixer, review: "a front wall about 1 tile tall, windows right under the eave") a one-storey town house of a
    // terrace stands a few pixels taller than a cottage: a band of wall over its windows under the eave)
    B0.wallH = (int16_t)c.wallFor(n, (c.wl >= 2 ? 26 : 24) + (n == 1 ? 5 : 0));
    B0.eave = 0;
    B0.gable = 0;
    B0.feat &= ~(uint32_t)(VF_JETTY | VF_STILTS);
    // (M7c ART, city_gallery --check "hole at joint" on the marsh rows) a raised people's units come down onto their
    // plinth like every row (VF_STILTS is gone, so stilts() left the body floating 6 or 7 px over the ground with no
    // posts under it: ground showed through every party wall's foot)
    if (B0.z0 > 0 && B0.z0 <= 8) {
      const int dz = -(int)B0.z0;
      for (Volume& x : c.v)
        if (x.role != VolRole::Platform && x.role != VolRole::Enclosure) x.z0 = (int16_t)std::max(0, (int)x.z0 + dz);
    }
    if (pitchedRoof(B0.roof)) {
      B0.roof = RoofShape::Gable;
      B0.ridgeNS = false;
      B0.pitch = (uint8_t)std::clamp((int)c.st.pitch, 1, 4);
      B0.roofMat = c.st.roofMat;
      B0.roofTint = 0;
    } else {
      B0.roof = RoofShape::FlatParapet;
      B0.pitch = 0;
    }
    if (B0.dormers && (B0.y1 - B0.y0) < 44) B0.dormers = 0;
    // (M7c fixer, review: "row-house terraces look squat and flat": a 1-tile front under a 3-tile slab of uniform roof,
    // no dormers) a one-storey house of a pitched terrace with the depth for it keeps an attic under its roof: one
    // dormer on the front plane for two houses in three (a pure function of the request), so each house of the row
    // shows on the roof and the long slab is broken house by house
    if (pitchedRoof(B0.roof) && n == 1 && (B0.y1 - B0.y0) >= 40 && c.W >= 40 && !c.st.snow && !back) {
      const uint32_t dh = mix32(c.r.seed * 0x85EBCA6Bu ^ 0xD0E7u);
      if (dh % 3u != 0) B0.dormers = 1;
    }
    // its chimneys stand in the body's depth (a stack left where a wing stood floated behind the roof)
    for (Volume& x : c.v)
      if (x.role == VolRole::Chimney && (x.y0 < B0.y0 || x.y1 > B0.y1)) {
        const int d = x.y1 - x.y0;
        x.y0 = (int16_t)std::max((int)B0.y0, std::min((int)x.y0, (int)B0.y1 - d));
        x.y1 = (int16_t)(x.y0 + d);
      }
    // the foundation under the whole front: one plinth, as deep as the body
    for (Volume& x : c.v)
      if (x.role == VolRole::Platform) {
        x.x0 = (int16_t)std::min((int)x.x0, (int)B0.x0); x.x1 = (int16_t)std::max((int)x.x1, (int)B0.x1);
        x.y0 = (int16_t)std::min((int)x.y0, (int)B0.y0);
      }
  }
  const uint32_t h = mix32(c.r.seed * 0x9E3779B1u ^ 0x7E44ACEu);
  const uint32_t look = c.r.facts.variant ? (uint32_t)(c.r.facts.variant - 1) : (h >> 8);   // the house's front in its row
  for (Volume& x : c.v) {
    const bool body = &x == &c.v[0] || (x.role == VolRole::Body);
    if (x.role == VolRole::Chimney) {   // inside the plot (a stack on the party wall stands just within it)
      const int w = x.x1 - x.x0;
      if (pw && x.x0 < 0) { x.x0 = 0; x.x1 = (int16_t)w; }
      if (pe && x.x1 > c.W) { x.x1 = (int16_t)c.W; x.x0 = (int16_t)(c.W - w); }
      continue;
    }
    if (x.role == VolRole::Platform) {   // the foundation runs on along the terrace
      if (pw && x.x0 <= bx0 + 1) x.x0 = 0;
      if (pe && x.x1 >= bx1 - 1) x.x1 = (int16_t)c.W;
      if (pw) x.feat |= VF_PARTY_W;
      if (pe) x.feat |= VF_PARTY_E;
      continue;
    }
    // a row's front is a closed facade on the walk (a door, windows, a shop's front): a veranda or a colonnade along a
    // terrace would open bays the rooms behind it (one doorway between two party walls) cannot answer
    if (openFaceKind(x.face)) x.face = Face::Windows;
    if (!body && !(x.y1 >= by1 - 2 && ((pw && x.x0 <= bx0) || (pe && x.x1 >= bx1)))) continue;
    if (pw && x.x0 <= bx0 + 1) { x.x0 = 0; x.feat |= VF_PARTY_W; }
    if (pe && x.x1 >= bx1 - 1) { x.x1 = (int16_t)c.W; x.feat |= VF_PARTY_E; }
    x.feat &= ~VF_JETTY;
    // a city row never thatches (the fire would run the length of the street): shingles or tiles of the people's own
    if (x.roofMat == RoofMat::Thatch || x.roofMat == RoofMat::Palm) x.roofMat = (c.cul == CU_DUNE || c.cul == CU_SUN || c.cul == CU_IMPERIAL || c.cul == CU_RIVER) ? RoofMat::ClayTile : RoofMat::Shingle;
    if ((!x.ridgeNS && x.roof == RoofShape::Hip) || x.roof == RoofShape::Pagoda || x.roof == RoofShape::Sweep) x.roof = RoofShape::Gable;   // (a swept eave's horns would rise through the neighbour's roof)
  }
  Volume& B = c.v[0];
  // the frontage runs flush along the street: the body's front on the plot's front edge (a porch set it back: the
  // neighbours' fronts then stood forward of it, a notch of open ground at each joint), and the door is in it
  if (B.y1 < c.H) {
    const int d = c.H - B.y1;
    for (Volume& x : c.v)
      if (x.role == VolRole::Chimney && x.y1 <= B.y1 + 1 && x.y0 >= B.y0 - 1) { x.y0 = (int16_t)(x.y0 + d); x.y1 = (int16_t)(x.y1 + d); }
    B.y1 = (int16_t)c.H;
    for (Volume& x : c.v)   // (and its plinth with it: the wall never stands past its foundation)
      if (x.role == VolRole::Platform && x.y1 < B.y1 + 2) x.y1 = (int16_t)std::min(c.H + art::BLDG_PAD_B - 1, B.y1 + 2);
  }
  bool door = false;
  for (const Volume& x : c.v) if (x.doorHere) door = true;
  if (!door) B.doorHere = true;
  // its own colour in the row: the people's limewashes and paints on plaster, timber and adobe (stone and brick keep
  // their own), a darker or lighter firing of the brick
  static const uint32_t wash[12][4] = {
      {RGB(222, 210, 180), RGB(196, 120, 90), RGB(206, 196, 150), RGB(180, 190, 196)},   // fjordfolk: ochre, red, cream, grey
      {RGB(226, 220, 204), RGB(214, 196, 160), RGB(200, 204, 196), RGB(230, 210, 190)},   // highland
      {RGB(236, 224, 196), RGB(226, 196, 150), RGB(214, 178, 170), RGB(196, 206, 214)},   // heartland: cream, ochre, pink, pale blue
      {RGB(232, 214, 186), RGB(220, 170, 130), RGB(226, 200, 160), RGB(214, 150, 120)},   // imperial: the warm washes
      {RGB(240, 232, 214), RGB(226, 200, 156), RGB(232, 216, 186), RGB(210, 176, 130)},   // dune: white, sand
      {RGB(214, 196, 166), RGB(200, 176, 140), RGB(224, 210, 180), RGB(190, 166, 130)},   // steppe
      {RGB(200, 184, 150), RGB(180, 170, 130), RGB(214, 200, 166), RGB(170, 150, 120)},   // marsh
      // ((M7c fixer, review: "jade terraces read as one repeated module": four pale greys no phone could tell apart)
      {RGB(234, 230, 220), RGB(224, 204, 166), RGB(198, 214, 198), RGB(222, 192, 182)},   // jade: lime white, ochre, celadon, rose
      {RGB(232, 220, 196), RGB(226, 200, 170), RGB(210, 214, 206), RGB(220, 190, 150)},   // river
      {RGB(240, 230, 210), RGB(226, 196, 150), RGB(232, 210, 176), RGB(214, 180, 140)},   // sun temples
      {RGB(220, 226, 206), RGB(206, 216, 196), RGB(230, 232, 214), RGB(196, 206, 186)},   // sylvan
      {RGB(236, 238, 242), RGB(222, 226, 236), RGB(232, 232, 226), RGB(214, 220, 232)}};  // starspire
  if (B.wall == WallMat::Plaster || B.wall == WallMat::Timber || B.wall == WallMat::Adobe || B.wall == WallMat::Wattle) {
    const int ci = c.cul >= 1 && c.cul <= 12 ? c.cul - 1 : 2;
    const uint32_t col = wash[ci][(look / 32u + (h >> 3)) % 4u];   // (M7c integration: and its own seed, two rows' fronts never alike)
    for (Volume& x : c.v) if ((x.feat & (VF_PARTY_W | VF_PARTY_E)) && x.role != VolRole::Platform && x.wall == B.wall) x.wallTint = col;
  } else if (B.wallTint) {
    // (M7c integration, rpg_test --builder: two stone row houses a street apart were clones, their fronts' 32 paints
    // matching) stone, brick and the rest keep their own colour but each house its own quarrying or firing of it, a
    // shade lighter or darker by its place in the row (Bldg::variant), so 128 fronts, not 32
    static const int pct[4] = {100, 93, 106, 88};
    const int k = pct[(look / 32u + (h >> 11)) % 4u];
    auto sc = [&](uint32_t v, int sh) { return (uint32_t)std::min(255, (int)((v >> sh) & 255u) * k / 100) << sh; };
    for (Volume& x : c.v)
      if ((x.feat & (VF_PARTY_W | VF_PARTY_E)) && x.role != VolRole::Platform && x.wall == B.wall && x.wallTint) {
        const uint32_t t = x.wallTint;
        x.wallTint = (t & 0xFF000000u) | sc(t, 16) | sc(t, 8) | sc(t, 0);
      }
  }
  if (c.wl >= 3) B.ornament |= (uint16_t)(art::ORN_FLOWERBOX | art::ORN_SHUTTERS);   // a rich quarter's window boxes and shutters
  else if (c.wl <= 0) B.ornament &= (uint16_t)~(art::ORN_FLOWERBOX | art::ORN_VINES);
  // (fixer M7b r3) one roof, but each house its own front: its door and window frames painted (the people's own trim
  // or one of three paints), shutters or none, a creeper or none, by its place in the terrace (Bldg::variant, which the
  // generator numbers along the row so no two houses near each other share a front)
  {
    static const uint32_t trims[4] = {0, RGB(72, 60, 48), RGB(118, 44, 38), RGB(54, 72, 90)};
    const uint32_t tr = trims[look % 4u];
    B.trimTint = tr;
    auto setOrn = [&](uint16_t bit, bool on) { B.ornament = (uint16_t)(on ? (B.ornament | bit) : (B.ornament & ~bit)); };
    if (c.wl < 3) setOrn((uint16_t)art::ORN_SHUTTERS, ((look / 4u) & 1u) != 0);
    if (c.wl > 0) setOrn((uint16_t)art::ORN_VINES, ((look / 8u) & 1u) != 0);
    if (c.wl > 0 && c.wl < 3) setOrn((uint16_t)art::ORN_FLOWERBOX, ((look / 16u) & 1u) != 0);
  }
  // ((M7c fixer, review: "five identical red moon doors at even spacing ... evenly spaced identical chimneys") each
  // house of a jade row its own door and windows among its people's (the moon door, a double leaf, an arched door; the
  // lattice, round and screen windows), and in every row each house's stack a little along its ridge and its own height,
  // by its place in the row (no draw: a pure function of the request)
  if (c.cul == CU_JADE) {
    static const art::DoorShape doors[3] = {art::DoorShape::Moon, art::DoorShape::Double, art::DoorShape::Arched};
    static const art::WindowShape wins[3] = {art::WindowShape::Lattice, art::WindowShape::Round, art::WindowShape::Screen};
    for (Volume& x : c.v) {
      if (x.role == VolRole::Platform || x.role == VolRole::Enclosure || x.role == VolRole::Chimney) continue;
      if (x.doorHere) x.door = doors[(look / 3u) % 3u];
      if (x.wall == B.wall) x.window = wins[(look / 5u + (look / 3u)) % 3u];
    }
  }
  {
    const int dx = ((int)((look >> 2) % 3u) - 1) * 5, dz = (int)((look >> 4) % 3u) * 2;
    for (Volume& x : c.v) {
      if (x.role != VolRole::Chimney) continue;
      const int w = x.x1 - x.x0;
      const int nx0 = std::clamp((int)x.x0 + dx, (int)B.x0 + 3, std::max((int)B.x0 + 3, (int)B.x1 - 3 - w));
      x.x0 = (int16_t)nx0; x.x1 = (int16_t)(nx0 + w);
      x.wallH = (int16_t)(x.wallH + dz);
    }
  }
  if ((row & ROW_YARD) && !back) {   // the yard's low back wall along the back of the plot, the house set a little forward of it
    const int back = std::min(8, std::max(0, (int)B.y1 - (int)B.y0 - 32));
    if (back >= 4) {
      for (Volume& x : c.v) if (x.role == VolRole::Body && x.y0 < back) x.y0 = (int16_t)back;
      Volume wv = c.vol(VolRole::Enclosure, pw ? 0 : 2, 0, pe ? c.W : c.W - 2, 3, 9);
      wv.wall = B.wall == WallMat::Timber || B.wall == WallMat::Plaster ? WallMat::Stone : B.wall;
      wv.roof = RoofShape::FlatParapet; wv.roofMat = RoofMat::Adobe; wv.pitch = 0; wv.face = Face::Blank;
      if (pw) wv.feat |= VF_PARTY_W;
      if (pe) wv.feat |= VF_PARTY_E;
      c.add(wv);
    }
  }
  streetFaces(c, pw, pe, sideW, sideE, back);
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
}  // namespace

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
  terrace(c);
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

}  // namespace bld
