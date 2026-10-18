// rigposes_gallery (P2a POSES; skeleton by the phase A lead, handed to POSES): what town folk do with their bodies on v2
// (STYLE_P2 13, 15, 16). Every board at 1x and 4x.
//   rigposes_gallery <outDir>   rigposes_postures.png    the 31 postures (their frames) in four facings, the village man
//                               rigposes_women.png       the same on the village woman (a Dress: the seated skirts)
//                               rigposes_bards.png       the bard's instruments by culture (Lute, Flute, Drum variants)
//                                                        and the kneel's two variants
//                               rigposes_seats.png       sitters on the chair, stool, bench, throne, cushion and the
//                                                        ground in every facing the seat takes (the seat's v1 / v2 art
//                                                        under them, the seat line in gold)
//                               rigposes_ride.png        the Ride posture on the rig-2 horse in four facings
//                               rigposes_sleepers.png    the v2 sleeper in each of the 10 berths, frame 0 and 1
//                               rigposes_looks.png       beards, headwear, culture cuts and rags in seven postures
//                               rigposes_kin.png         a youth and a child in the postures kids take (sitting, tag,
//                                                        eating, carrying) and the adults beside them
//                               rigposes_humanoids.png   skeleton, draugr and goblin on v2 (every monster frame), beside
//                                                        the 32 px hero
//   rigposes_gallery <outDir> check                      per posture: frames per facing, atlas KB and bake ms; the
//                                                        sitters' hip rows against seatFit2's seat lines
#include <chrono>
#include <cstring>
#include "tools/preview/rig_preview.h"
#include "rpg/art/rig/rig_internal.h"

namespace {

art::HumanLook villager() {
  art::HumanLook L;
  L.skin = rgba(214, 160, 116);         // S4
  L.hairColor = rgba(70, 44, 30);       // Umber
  L.hair = art::Hair::Short;
  L.topColor = rgba(120, 140, 80);
  L.bottomColor = rgba(92, 70, 52);
  return L;
}

const char* postureName(art::Posture p) {
  static const char* n[] = {"NONE", "SIT", "SITEAT", "SITDRINK", "EAT", "DRINK", "CHEER", "HAMMER", "HOE", "SWEEP", "CHOP", "STIR", "CARRY", "FISH",
                            "SLEEP", "WAVE", "PLAY", "DANCE", "LUTE", "DRUM", "FLUTE", "PRAY", "BEG", "LAMP", "READ", "SITFLOOR", "SITFLEAT",
                            "SITFLDRINK", "RIDE", "DOWNED", "KNEEL"};
  return (int)p < (int)(sizeof(n) / sizeof(n[0])) ? n[(int)p] : "?";
}

// a board of postures for one look: one row per posture, its frames in the four facings
void postureBoard(const art::HumanLook& L, const std::string& dir, const std::string& name) {
  const int rows = (int)art::Posture::COUNT - 2;
  Board b(1500, rows * 50 + 30);
  int y = 46;
  for (int p = 1; p < (int)art::Posture::COUNT; p++) {
    if ((art::Posture)p == art::Posture::Sleep) continue;
    const art::RigSheet s = art::humanPosture2(L, (art::Posture)p, 0);
    rigpv::clipRow(b, s, art::Clip::Idle, 0, 90, y, 36, postureName((art::Posture)p));
    y += 50;
  }
  rigpv::saveBoth(b, dir, name);
}

void bardBoard(const art::HumanLook& L, const std::string& dir) {
  struct V { art::Posture p; uint8_t v; const char* n; };
  const V vs[] = {
      {art::Posture::Lute, (uint8_t)LeadInst::Lute, "LUTE"},     {art::Posture::Lute, (uint8_t)LeadInst::Oud, "OUD"},
      {art::Posture::Lute, (uint8_t)LeadInst::Fiddle, "FIDDLE"}, {art::Posture::Lute, (uint8_t)LeadInst::Harp, "HARP"},
      {art::Posture::Flute, (uint8_t)LeadInst::Flute, "FLUTE"},  {art::Posture::Flute, (uint8_t)LeadInst::Pipes, "PIPES"},
      {art::Posture::Flute, (uint8_t)LeadInst::Reed, "REED"},    {art::Posture::Flute, (uint8_t)LeadInst::Horn, "HORN"},
      {art::Posture::Flute, (uint8_t)LeadInst::Brass, "BRASS"},  {art::Posture::Flute, (uint8_t)LeadInst::Bells, "BELLS"},
      {art::Posture::Flute, (uint8_t)LeadInst::Voice, "VOICE"},  {art::Posture::Drum, (uint8_t)PercKind::Frame, "FRAME"},
      {art::Posture::Drum, (uint8_t)PercKind::Bodhran, "BODHRAN"}, {art::Posture::Drum, (uint8_t)PercKind::Taiko, "TAIKO"},
      {art::Posture::Drum, (uint8_t)PercKind::Hand, "HAND"},     {art::Posture::Drum, (uint8_t)PercKind::Gong, "GONG"},
      {art::Posture::Drum, (uint8_t)PercKind::Wood, "WOOD"},     {art::Posture::Drum, (uint8_t)PercKind::Bells, "JINGLE"},
      {art::Posture::Carry, 1, "CARRY BASKET"},                 {art::Posture::Carry, 2, "CARRY CRATE"},
      {art::Posture::Kneel, 0, "KNEEL BIND"},                    {art::Posture::Kneel, 1, "KNEEL MOURN"},
  };
  const int n = (int)(sizeof(vs) / sizeof(vs[0]));
  Board b(1000, n * 50 + 30);
  int y = 46;
  for (const V& v : vs) {
    const art::RigSheet s = art::humanPosture2(L, v.p, v.v);
    rigpv::clipRow(b, s, art::Clip::Idle, 0, 110, y, 36, v.n);
    y += 50;
  }
  rigpv::saveBoth(b, dir, "rigposes_bards");
}

// a posture frame stood with its pivot at (x, y)
void putFrame(Board& b, const art::RigSheet& s, int facing, int i, int x, int y) {
  int px, py;
  const Canvas f = rigpv::frameCanvas(s, facing, art::Clip::Idle, 0, i, px, py);
  if (f.w > 0) b.put(f, x - px, y - py);
}

// sitters on their seats: the seat sprite on its tile (bottom-centre), the sitter's pivot at seatFit2's spot
void seatBoard(const art::HumanLook& man, const art::HumanLook& woman, const std::string& dir) {
  struct S { art::Prop seat; const char* n; };
  const S seats[] = {{art::Prop::Chair, "CHAIR"}, {art::Prop::Stool, "STOOL"}, {art::Prop::Bench, "BENCH"}, {art::Prop::Throne, "THRONE"},
                     {art::Prop::Cushion, "CUSHION"}, {art::Prop::COUNT, "GROUND"}};
  Board b(1100, 6 * 70 + 40);
  // a wooden floor under the seats
  for (int y = 0; y < b.c.h; y++)
    for (int x = 0; x < b.c.w; x++) {
      const bool seam = (y % 8) == 0 || ((x + (y / 8) * 13) % 32) == 0;
      b.c.set(x, y, seam ? rgba(96, 66, 46) : (((x + y) & 7) == 0 ? rgba(134, 96, 64) : rgba(126, 90, 60)));
    }
  int y0 = 60;
  for (const S& st : seats) {
    b.text(4, y0 - 40, st.n);
    int x0 = 90;
    for (int who = 0; who < 2; who++) {
      const art::HumanLook& L = who ? woman : man;
      for (int facing = 0; facing < 4; facing++) {
        const art::SeatFit f = art::seatFit2(st.seat, -1, facing);
        const int tx = x0, ty = y0;   // the seat tile's bottom-centre
        if (st.seat != art::Prop::COUNT) {
          const Canvas sp = art::propSprite(st.seat);
          b.put(sp, tx - sp.w / 2, ty - sp.h);
        }
        if (f.ok) {
          const art::RigSheet s = art::humanPosture2(L, f.sit, 0);
          const int ax = facing == 3 ? -f.ax : f.ax;
          putFrame(b, s, facing, 0, tx + ax, ty + f.ay);
          for (int k = -9; k <= -7; k++) b.c.set(tx + k, ty - 16 + f.seatY, rgba(242, 193, 78));   // the seat line
        } else b.text(tx - 4, ty - 10, "-", rgba(232, 96, 60));
        x0 += 56;
      }
      x0 += 30;
    }
    y0 += 70;
  }
  rigpv::saveBoth(b, dir, "rigposes_seats");
}

// the Ride posture on HOLDERS' rig-2 horse: the rider's hip anchor on saddleSeat, the horse's head row over the rider
// facing down and its rump over the legs facing up (home_view.cpp's layering)
void rideBoard(const art::HumanLook& man, const art::HumanLook& woman, const std::string& dir) {
  Board b(700, 170);
  const int cw = art::horseCellW(), ch = art::horseCellH();
  int x = 20;
  for (int who = 0; who < 2; who++) {
    const art::RigSheet s = art::humanPosture2(who ? woman : man, art::Posture::Ride, 0);
    for (int facing = 0; facing < 4; facing++) {
      const int row = facing == 0 ? 0 : facing == 1 ? 1 : 2;
      Canvas horse = art::horseCell(rgba(140, 92, 56), 3, true, row, 0);
      if (facing == 3) {   // the sheet faces right: the view flips it for left
        Canvas m(horse.w, horse.h);
        for (int yy = 0; yy < horse.h; yy++) for (int xx = 0; xx < horse.w; xx++) m.set(horse.w - 1 - xx, yy, horse.get(xx, yy));
        horse = m;
      }
      const int baseY = 140;
      b.put(horse, x, baseY - ch + 2);
      int dx = 0, dy = 0;
      art::saddleSeat(row, 0, dx, dy);
      if (facing == 3) dx = -dx;
      const int seatX = x + cw / 2 + dx, seatY = baseY + dy;
      int px, py;
      const Canvas f = rigpv::frameCanvas(s, facing, art::Clip::Idle, 0, 0, px, py);
      const art::RigAnchors* an = s.anchorsOf(facing, art::Clip::Idle, 0, 0);
      if (f.w > 0 && an) b.put(f, seatX - (px + an->hipX), seatY - (py + an->hipY));
      if (row == 0) {   // the head and chest over the rider
        Canvas head = art::horseCell(rgba(140, 92, 56), 3, true, 3, 0);
        b.put(head, x, baseY - ch + 2);
      } else if (row == 1) {
        const int sr = std::clamp(ch - 2 + dy, 0, ch);
        Canvas rump(horse.w, horse.h);
        for (int yy = sr + 1; yy < horse.h; yy++) for (int xx = 0; xx < horse.w; xx++) rump.set(xx, yy, horse.get(xx, yy));
        b.put(rump, x, baseY - ch + 2);
      }
      b.c.set(seatX, seatY, rgba(242, 193, 78));
      x += cw + 8;
    }
    x += 10;
  }
  b.text(4, 4, "RIDE ON THE RIG-2 HORSE (SADDLESEAT): DOWN UP RIGHT LEFT");
  rigpv::saveBoth(b, dir, "rigposes_ride");
}

void sleeperBoard(const art::HumanLook& L, const std::string& dir) {
  Board b(1100, 230);
  int x = 10;
  for (int k = 0; k < (int)art::Berth::COUNT; k++) {
    const art::BedFit bf = art::bedFit2((art::Berth)k, -1);
    const Canvas berth = art::berthSprite((art::Berth)k, -1, 0);
    for (int f = 0; f < 2; f++) {
      const int baseY = 150;
      const Canvas c = art::sleeper2(L, (art::Berth)k, -1, f);
      const int w = std::max<int>(std::max<int>(berth.w, c.w), bf.w);
      if (berth.w > 0) b.put(berth, x + (w - berth.w) / 2, baseY - berth.h);
      if (c.w > 0) b.put(c, x + (w - c.w) / 2, baseY - c.h);
      else b.text(x, baseY - 20, "-", rgba(232, 96, 60));
      x += std::max(30, w + 8);
    }
    x += 10;
  }
  b.text(4, 4, "SLEEPERS IN THE 10 BERTHS (FRAME 0, 1): BED LONGBED BUNKLOW BUNKHIGH HAMMOCK LONGHAMMOCK MAT LONGMAT BEDROLL GROUND");
  rigpv::saveBoth(b, dir, "rigposes_sleepers");
}

// RIG-BODY's civilian base in the postures: beards, headwear, culture cuts (robe, kaftan, kilt, poncho, gown), rags
void looksBoard(const std::string& dir) {
  struct LK { const char* n; uint8_t cut, headwear; bool beard, rags, woman; art::Outfit outfit; };
  const LK lks[] = {{"BEARD CAP", 0, 2, true, false, false, art::Outfit::Tunic}, {"ROBE HOOD", 2, 1, false, false, false, art::Outfit::Robe},
                    {"KAFTAN TURBAN", 3, 3, true, false, false, art::Outfit::Tunic}, {"KILT", 6, 0, true, false, false, art::Outfit::Tunic},
                    {"PONCHO HAT", 7, 5, false, false, false, art::Outfit::Tunic}, {"GOWN VEIL", 8, 6, false, false, true, art::Outfit::Dress},
                    {"RAGS", 0, 0, true, true, false, art::Outfit::Rags}};
  const art::Posture ps[] = {art::Posture::Sit, art::Posture::SitDrink, art::Posture::Hammer, art::Posture::SitFloor, art::Posture::Kneel,
                             art::Posture::Beg, art::Posture::Wave};
  const int n = (int)(sizeof(lks) / sizeof(lks[0]));
  Board b(1700, n * 50 + 30);
  int y = 46;
  for (const LK& k : lks) {
    art::HumanLook L = villager();
    L.cut = k.cut; L.headwear = k.headwear; L.beard = k.beard; L.outfit = k.outfit;
    L.topColor = rgba(140, 90, 60); L.headColor = rgba(200, 180, 120); L.patternColor = rgba(220, 200, 140); L.pattern = 1;
    if (k.woman) { L.body = 1; L.hair = art::Hair::Long; }
    int x = 120;
    b.text(4, y - 30, k.n);
    for (art::Posture p : ps) {
      const art::RigSheet s = art::humanPosture2(L, p, 0);
      for (int facing = 0; facing < 4; facing++) {
        putFrame(b, s, facing, 0, x + 14, y);
        x += 26;
      }
      x += 12;
    }
    y += 50;
  }
  rigpv::saveBoth(b, dir, "rigposes_looks");
}

void kinBoard(const std::string& dir) {
  Board b(2300, 5 * 56 + 40);
  const art::Posture ps[] = {art::Posture::Sit, art::Posture::Play, art::Posture::Eat, art::Posture::Carry, art::Posture::SitFloor};
  int y = 50;
  for (art::Posture p : ps) {
    int x = 90;
    b.text(4, y - 30, postureName(p));
    for (int age : {0, 2, 1}) {
      art::HumanLook L = villager();
      L.age = (uint8_t)age;
      const art::RigSheet s = art::humanPosture2(L, p, 0);
      x = rigpv::clipRow(b, s, art::Clip::Idle, 0, x, y, 30, "") + 20;
    }
    y += 56;
  }
  rigpv::saveBoth(b, dir, "rigposes_kin");
}

void humanoidBoard(const std::string& dir) {
  Board b(1100, 230);
  int y = 30;
  const art::RigSheet hero = art::humanRig(rigpv::sliceHero(false), art::RigPart::Loco);
  for (int sp = 1; sp <= 3; sp++) {
    int x = 70;
    b.text(4, y + 10, sp == 1 ? "SKEL" : sp == 2 ? "DRAUGR" : "GOBLIN");
    const int cw = art::humanoidCellW2(sp), ch = art::humanoidCellH2(sp);
    {
      int px, py;
      const Canvas h = rigpv::frameCanvas(hero, 2, art::Clip::Idle, 0, 0, px, py);
      if (h.w > 0) b.put(h, x, y + std::max(ch, 40) - 2 - py);
      x += 30;
    }
    for (int f = 0; f < art::MONSTER_FRAMES; f++) {
      Canvas c = art::humanoidCell2(sp, f);
      if (c.w > 0) { art::outlineInk(c); b.put(c, x, y + std::max(ch, 40) - ch); }
      else b.text(x, y + 10, "-", rgba(232, 96, 60));
      x += std::max(30, cw + 4);
    }
    y += std::max(40, ch) + 20;
  }
  rigpv::saveBoth(b, dir, "rigposes_humanoids");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::printf("usage: rigposes_gallery <outDir> [check]\n"); return 2; }
  const std::string dir = argv[1];
  art::setRigVersion(2);
  const art::HumanLook man = villager();
  if (argc > 2 && !std::strcmp(argv[2], "check")) {
    double worstMs = 0, totalKB = 0;
    size_t worstB = 0;
    for (int p = 1; p < (int)art::Posture::COUNT; p++) {
      const auto t0 = std::chrono::steady_clock::now();
      const art::RigSheet s = art::humanPosture2(man, (art::Posture)p, 0);
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      worstMs = std::max(worstMs, ms); worstB = std::max(worstB, s.bytes()); totalKB += s.bytes() / 1024.0;
      std::printf("%-11s frames %d/%d/%d/%d  %5.1f KB  %.2f ms\n", postureName((art::Posture)p), s.count(0, art::Clip::Idle), s.count(1, art::Clip::Idle),
                  s.count(2, art::Clip::Idle), s.count(3, art::Clip::Idle), s.bytes() / 1024.0, ms);
    }
    std::printf("postures: worst atlas %.1f KB (R6 64), worst bake %.2f ms, all %.0f KB\n", worstB / 1024.0, worstMs, totalKB);
    for (art::Prop seat : {art::Prop::Chair, art::Prop::Stool, art::Prop::Bench, art::Prop::Throne, art::Prop::Cushion, art::Prop::COUNT})
      for (int facing = 0; facing < 4; facing++) {
        const art::SeatFit f = art::seatFit2(seat, -1, facing);
        if (!f.ok) continue;
        const art::RigSheet s = art::humanPosture2(man, f.sit, 0);
        const art::RigAnchors* an = s.anchorsOf(facing, art::Clip::Idle, 0, 0);
        std::printf("seat %d facing %d: seat line row %d, hips row %d\n", (int)seat, facing, f.seatY, an ? 16 + f.ay + an->hipY : -99);
      }
    return 0;
  }
  const art::HumanLook woman = rigpv::sliceVillager();
  postureBoard(man, dir, "rigposes_postures");
  postureBoard(woman, dir, "rigposes_women");
  bardBoard(man, dir);
  seatBoard(man, woman, dir);
  rideBoard(man, woman, dir);
  sleeperBoard(man, dir);
  looksBoard(dir);
  kinBoard(dir);
  humanoidBoard(dir);
  return 0;
}
