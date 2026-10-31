// Builder culture homes and seats of power (SOLID 1.6, cut from builder.cpp): the generic house, round, court and
// long-hall assemblers, cultureHome (every people's own house), the monumental idiom and every seat* (castle, court,
// hall, tent, pyramid, temple complex, guild, spire, tree, stilt) with seat() choosing among them.
#include "rpg/build/build_detail.h"

namespace bld {

// ---------------------------------------------------------------- homes: the generic rectangular house
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
  // (M7 fix) the player's own house keeps one body (its porch, cross gable, dormers and chimneys give it depth): a side
  // wing's joint with the body (its roof run into the body's, the plinth's step, the eave over the facade) did not
  // meet the owner's bar on the houses the player builds and looks at every day
  if (sp.wings && !c.r.home && c.W >= 64 && c.ch(c.wl >= 2 ? 150 : 80)) {
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
  // (M7 fix) not on the player's house: a jettied cross gable's shaded underside read as a dark slab under the facade
  if (n >= 2 && sp.jetty && !c.r.home && (bo.wall == WallMat::Timber || bo.wall == WallMat::Plaster) && pitched && c.ch(128)) bo.feat |= VF_JETTY;
  if (sp.dormers && pitched && !bo.ridgeNS && my1 - y0 >= 44 && !c.st.snow &&
      (bo.roof == RoofShape::Gable || bo.roof == RoofShape::Hip || bo.roof == RoofShape::Steep || bo.roof == RoofShape::Mansard))
    bo.dormers = (uint8_t)c.pick(3);
  const int bi = c.add(bo);
  // the wing
  if (wing) {
    // (M7 fix) a side wing's front is flush with the body's (a porch set the body back: the wing stood out past the
    // facade, its roof spilling over the body's windows and its foundation leaving a notch at the joint)
    Volume w = c.vol(VolRole::Wing, wing == 1 ? mx1 : x0, lWing ? y0 + 4 : y0 + c.rg(4, 8), wing == 1 ? x1 : mx0, lWing ? y1 : my1,
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
  // (M7 fixer r2, review: "the hall's cross gable meets the main roof badly": its small gable sat under the main eave,
  // its ridge a light sliver up the main slope with no valley) not on the player's house: its dormers give the roof
  // its depth instead
  if (sp.cross && !c.r.home && pitched && !B.ridgeNS && B.x1 - B.x0 >= 72 && B.roof != RoofShape::Turf && c.ch(110)) {
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
  // (draws sequenced: the right argument's first, as MSVC/GCC evaluate them; clang goes left to right)
  const int drumH = felt ? 15 + c.rg(0, 2) : c.wallFor(n, base) + c.rg(-2, 2);
  const int drumY = c.H - (c.H >= 40 ? c.pick(3) : 0);
  Volume bo = c.rnd(VolRole::Body, cx, drumY, rad, drumH, n);
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
    const int annH = felt ? std::max(12, bo.wallH - c.rg(1, 3)) : std::max(14, bo.wallH - c.rg(4, 8));   // (sequenced: right argument first)
    const int annY = c.H - c.rg(2, 6);
    Volume an = c.rnd(VolRole::Annex, ax, annY, ar, annH);
    an.wall = bo.wall; an.roof = bo.roof; an.roofMat = bo.roofMat; an.feat = bo.feat & (VF_CROWN); an.pitch = bo.pitch; an.eave = bo.eave;
    an.window = bo.window; an.door = bo.door; an.roofTint = bo.roofTint;
    c.add(an);
  }
  return bi;
}

namespace {
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
}  // namespace

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
  // (draws sequenced: the right argument's first, as MSVC/GCC evaluate them; clang goes left to right)
  const int hallY1 = c.H - (c.ch(110) ? 6 : 0);
  const int hallY0 = std::max(0, c.H / 8 - 2 + c.pick(3));
  Volume bo = c.vol(VolRole::Body, inset, hallY0, c.W - inset, hallY1, c.wallFor(n, base), n);
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
      if (c.r.home && n >= 2) {
        // (M7) the player's two-storey shell, as the steppe builds its lords' winter seats: a ger of timber, two storeys
        // under a wide felt crown (the round body the society's courts use), with a felt ger pitched beside it for the
        // household's summer (one storey: a yurt is never stacked)
        const int bi = roundHouse(c, n, 22, false, c.W >= 96 ? 2 : 1);
        for (Volume& x : c.v) {
          const bool body = &x == &c.v[(size_t)bi];
          x.wall = body ? WallMat::Plank : WallMat::Felt;
          roofTo(x, RoofShape::Conical, RoofMat::Felt, body ? 1 : 2, c.st.roofTint);
          x.feat |= VF_CROWN;
          x.eave = 0;
          x.window = WindowShape::Round;
          x.door = body ? DoorShape::Double : DoorShape::Flap;
          if (!body) { x.storeys = 1; x.wallH = (int16_t)std::min<int>(x.wallH, 16); }
        }
        c.v[(size_t)bi].ornament |= art::ORN_PAINTED_BANDS;
        break;
      }
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
      // (M7 fix r3, review: "jade player-house fronts are a thin strip under a huge roof, with no door you can see") the
      // player's own house shows its street face as the town's homes do: the full wall, its windows and the moon door,
      // never a veranda or a skirt-roofed gallery across the whole front (the open bays hid the door and the wall)
      sp.base = 24; sp.porchKind = c.wl >= 2 && !c.r.home ? 2 : 0; sp.porchP = c.r.home ? 0 : 80; sp.dormers = false; sp.cross = false; sp.jetty = false;
      sp.ridge = 0;
      const int bi = house(c, sp);
      Volume& B = c.v[(size_t)bi];
      if (B.roofMat == RoofMat::Thatch || B.roofMat == RoofMat::Palm) {
        B.roof = RoofShape::Gable; B.pitch = (uint8_t)std::max(3, (int)B.pitch);
        if (c.r.home) { B.pitch = 2; B.wallH = (int16_t)(B.wallH + 4); }   // (M7 fix r3) the player's thatched hut: its wall and door clear of the eave
      }
      else if (c.wl >= 1) { B.roof = c.ch(170) ? RoofShape::Pagoda : RoofShape::Hip; B.pitch = 2; }
      if (n >= 2 && c.wl >= 1 && B.roof == RoofShape::Pagoda && c.v.size() == 1 && !c.r.home) {   // a double eave: a skirt roof round the ground floor
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
        // (draws sequenced: the right argument's first, as MSVC/GCC evaluate them; clang goes left to right)
        const int tierH = 14 + c.pick(3) * 2;
        const int tierY1 = B.y0 + 2 + std::max(12, bd * (45 + c.pick(2) * 10) / 100);
        Volume rr = c.vol(VolRole::Tier, rx, B.y0 + 2, rx + rw, tierY1, tierH);
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

// ---------------------------------------------------------------- seats of power
namespace {

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
}  // namespace

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

namespace {
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
}  // namespace

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

}  // namespace bld
