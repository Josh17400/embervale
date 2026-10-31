// Builder per-purpose forms (SOLID 1.6, cut from builder.cpp): huts, farms, shops, smithies, inns, tea houses,
// great halls, guildhalls, exchanges, baths, temples, the mage tower, mills and barracks, then the forms the
// settlement asks for (a generic compound, a tent).
#include "rpg/build/build_detail.h"

namespace bld {

// ---------------------------------------------------------------- huts, barns, workshops, shops
void hut(Bx& c) {
  if (c.cul == CU_STEPPE) { roundHouse(c, 1, 18, true, 0); return; }
  if (c.cul == CU_SYLVAN) { const int bi = roundHouse(c, 1, 18, false, 0); Volume& B = c.v[(size_t)bi]; B.wall = WallMat::Living; B.roof = RoofShape::Conical; B.roofMat = RoofMat::Bark; return; }
  HouseSpec sp;
  // (M7 fix r3, review: "the jade hut is a thin strip under its thatch") the player's own hut stands a little taller in
  // the wall under a less steep thatch, so its front wall and door read below the eave
  sp.base = c.r.home ? 22 : 18; sp.wings = false; sp.cross = false; sp.dormers = false; sp.porchP = 0; sp.jetty = false;
  WallMat& wm = c.st.wall;
  if (wm == WallMat::Stone || wm == WallMat::Brick || wm == WallMat::Ashlar || wm == WallMat::Plaster) wm = c.cul == CU_HIGHLAND ? WallMat::Rubble : (c.cul == CU_NONE ? WallMat::Log : WallMat::Wattle);
  RoofMat& rm = c.st.roofMat;
  if (c.cul == CU_IMPERIAL || c.cul == CU_RIVER) rm = RoofMat::ClayTile;
  else if (rm == RoofMat::Slate || rm == RoofMat::ClayTile || rm == RoofMat::Copper || rm == RoofMat::GlazedTile)
    rm = (c.cul == CU_SUN || c.cul == CU_MARSH) ? RoofMat::Palm : (c.cul == CU_DUNE ? RoofMat::Adobe : RoofMat::Thatch);   // (M7: a dune hut is mud brick)
  if (c.cul == CU_DUNE && rm == RoofMat::Adobe) { c.st.wall = WallMat::Adobe; c.st.roof = RoofShape::FlatParapet; }
  RoofShape& rs = c.st.roof;
  if (rs == RoofShape::Dome || rs == RoofShape::Onion || rs == RoofShape::Mansard || rs == RoofShape::Stepped || rs == RoofShape::Pagoda)
    rs = rm == RoofMat::Adobe ? RoofShape::FlatParapet : RoofShape::Gable;
  if (rs == RoofShape::Gable && (rm == RoofMat::Thatch || rm == RoofMat::Palm)) c.st.pitch = (uint8_t)(c.r.home ? 2 : std::max(3, (int)c.st.pitch));
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
    case CU_STEPPE: {
      // (M5 TOWNSFOLK, owner rule: an inn lets its beds upstairs only) the steppe road's guest house: a round timber
      // drum of two storeys (the common room round the stove below, the felt-hung sleeping gallery above) under a wide
      // felt cone with its crown ring, felt yurts round it for the household (a felt yurt itself is never stacked)
      const int bi = roundHouse(c, std::max(2, n), 22, false, c.W >= 80 ? 2 : (c.W >= 48 ? 1 : 0));
      for (Volume& x : c.v) {
        x.roof = RoofShape::Conical; x.roofMat = RoofMat::Felt; x.window = WindowShape::Round;
        if (x.role == VolRole::Annex) { x.wall = WallMat::Felt; x.door = DoorShape::Flap; x.eave = 0; x.feat |= VF_CROWN; }
      }
      c.v[(size_t)bi].feat |= VF_CROWN;
      c.v[(size_t)bi].ornament |= art::ORN_LANTERNS;
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

}  // namespace bld
