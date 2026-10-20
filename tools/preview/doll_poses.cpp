// doll_poses (POSES-B, P2b): the look-and-adjust loop for the postures on the doll (STYLE_P2 13, 15, 16; VISION_PLAN
// 15.36 P2b POSES-B). Every board at 1x, 2x and 4x (judge at 1x and zoomed).
//   doll_poses <outDir> [--palette look] [--pool FILE] [board...]   (--pool: node tools/doll/pxc.mjs --bin FILE --no-cpp)
//     poses       the 31 postures and Row in four facings, every frame: the A3 hero, the A3 woman (her dress), the woman
//                 in shirt and trousers (her legs), each on its own board (poses_hero, poses_woman, poses_womant)
//     seats       sitters on the chair, stool, bench, throne, cushion and the ground (seatFit2), the seat line in gold
//     ride        Ride on HOLDERS' rig-2 horse (saddleSeat) in four facings, the hip mark on the saddle point
//     row         Row's four strokes in four facings over a thwart line
//     sleep       the sleeper in the 10 berths, both breaths
//     variants    the bard's instruments by culture, the kneel's two, the carried loads
//     folk        census people of four peoples (men, women, an elder) in eight postures
//     check       per posture: frames per facing against postureInfo, atlas KB, bake ms, what the doll lacks (coverage);
//                 the sitters' hips against the seat lines (P2 restated); the rider's hips on the saddle
//     zoom:<posture>:<D|U|R|L>[:<frame>[:<who 0 hero|1 woman|2 womant>]]   one posture frame at 12x with its grid
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include "rpg/art/art_holders2.h"
#include "rpg/art/doll/doll.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/world.h"
#include "tools/preview/rig_preview.h"

namespace census {
struct Rank { bool royal = false, capital = false; };
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner, uint64_t seed,
           const Rank& rank);
}  // namespace census

namespace {
namespace doll = art::doll;
using art::Clip;
using art::Posture;
using art::RigSheet;

bool g_lookPalette = false;

uint32_t hexc(uint32_t rgb) { return 0xFF000000u | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF); }
art::HumanLook a3Look(int who) {
  art::HumanLook L;
  if (who == 0) {
    L.hair = art::Hair::Short; L.hairVar = 2; L.neck = 1; L.weapon = 1;
    L.skin = hexc(0xE9A077); L.hairColor = hexc(0xAA4A2C); L.topColor = hexc(0x28728C); L.bottomColor = hexc(0x4F4B6B); L.tabardColor = hexc(0xC83E38);
    L.eyeColor = hexc(0x3A78B0);
  } else {
    L.body = 1; L.hair = art::Hair::Braids; L.outfit = art::Outfit::Dress;
    L.skin = hexc(0xEEAA86); L.hairColor = hexc(0xC79346); L.topColor = hexc(0x922F60); L.tabardColor = hexc(0x28728C);
    L.eyeColor = hexc(0x3F8A5A);
    if (who == 2) { L.outfit = art::Outfit::Tunic; L.bottomColor = hexc(0x4F4B6B); }
  }
  return L;
}
doll::DollRecipe recipeOf(int who) {
  doll::DollRecipe r = doll::resolve(a3Look(who));
  if (!g_lookPalette) r.ramps = doll::a3Palette(who ? 1 : 0);
  return r;
}

const char* postureName(Posture p) {
  static const char* n[] = {"NONE", "SIT", "SITEAT", "SITDRINK", "EAT", "DRINK", "CHEER", "HAMMER", "HOE", "SWEEP", "CHOP", "STIR", "CARRY", "FISH",
                            "SLEEP", "WAVE", "PLAY", "DANCE", "LUTE", "DRUM", "FLUTE", "PRAY", "BEG", "LAMP", "READ", "SITFLOOR", "SITFLEAT",
                            "SITFLDRINK", "RIDE", "DOWNED", "KNEEL", "ROW"};
  return (int)p < (int)(sizeof(n) / sizeof(n[0])) ? n[(int)p] : "?";
}
int postureOf(const std::string& s) {
  for (int p = 0; p < (int)Posture::COUNT; p++) {
    std::string n = postureName((Posture)p);
    std::string t = s;
    for (char& c : t) c = (char)std::toupper((unsigned char)c);
    if (n == t) return p;
  }
  return std::atoi(s.c_str());
}

// the ground: grass on the left half, dirt on the right, the contact shadow under each figure
void dirt(Board& b, int y0, int y1) {
  for (int y = y0; y < y1 && y < b.c.h; y++)
    for (int x = b.c.w / 2; x < b.c.w; x++) {
      const uint32_t n = hash2(x / 2, y / 2) % 100;
      b.c.set(x, y, n < 20 ? rgba(110, 86, 60) : n > 90 ? rgba(140, 112, 80) : rgba(123, 95, 66));
    }
}
void floorBoards(Board& b) {
  for (int y = 0; y < b.c.h; y++)
    for (int x = 0; x < b.c.w; x++) {
      const bool seam = (y % 8) == 0 || ((x + (y / 8) * 13) % 32) == 0;
      b.c.set(x, y, seam ? rgba(96, 66, 46) : (((x + y) & 7) == 0 ? rgba(134, 96, 64) : rgba(126, 90, 60)));
    }
}
void shadow(Board& b, int cx, int cy, int w) {
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -w / 2; dx <= w / 2; dx++) {
      const float e = (dx * dx) / (float)(w * w / 4) + (dy * dy) / 6.25f;
      if (e <= 1.0f) b.c.set(cx + dx, cy + dy, art::shade(b.c.get(cx + dx, cy + dy), 0.72f));
    }
}
void save3(const Board& b, const std::string& dir, const std::string& name) {
  rigpv::saveBoth(b, dir, name);
  savePng(b.c, dir + "/" + name + "_2x.png", 2);
}
// a posture frame with its pivot at (x, y)
void putFrame(Board& b, const RigSheet& s, int facing, int i, int x, int y, bool shade = true) {
  int px, py;
  const Canvas f = rigpv::frameCanvas(s, facing, Clip::Idle, 0, i, px, py);
  if (f.w <= 0) return;
  if (shade) shadow(b, x, y, std::min(24, std::max(10, f.w - 4)));
  b.put(f, x - px, y - py);
}

// ---------------------------------------------------------------- boards
void boardPoses(const std::string& dir, int who, const char* name) {
  const doll::DollRecipe r = recipeOf(who);
  const int rows = (int)Posture::COUNT - 2;
  Board b(1180, rows * 46 + 40);
  dirt(b, 0, b.c.h);
  int y = 46;
  for (int p = 1; p < (int)Posture::COUNT; p++) {
    if ((Posture)p == Posture::Sleep) continue;
    const RigSheet s = doll::composePosture(r, (Posture)p, 0);
    b.text(4, y - 30, postureName((Posture)p));
    int x = 100;
    bool any = false;
    for (int facing = 0; facing < 4; facing++) {
      const int n = s.count(facing, Clip::Idle, 0);
      for (int i = 0; i < n; i++) { putFrame(b, s, facing, i, x, y); x += 30; any = true; }
      x += 14;
    }
    if (!any) {
      const std::vector<doll::Missing> m = doll::dollMissingPosture(a3Look(who), (Posture)p, 0);
      b.text(x, y - 12, m.empty() ? "NOT PAINTED" : "NOT PAINTED: " + m[0].what(), rgba(232, 96, 60));
    }
    y += 46;
  }
  save3(b, dir, name);
}

void boardSeats(const std::string& dir) {
  struct S { art::Prop seat; const char* n; };
  const S seats[] = {{art::Prop::Chair, "CHAIR"}, {art::Prop::Stool, "STOOL"}, {art::Prop::Bench, "BENCH"}, {art::Prop::Throne, "THRONE"},
                     {art::Prop::Cushion, "CUSHION"}, {art::Prop::COUNT, "GROUND"}};
  Board b(1100, 6 * 64 + 40);
  floorBoards(b);
  int y0 = 60;
  for (const S& st : seats) {
    b.text(4, y0 - 40, st.n);
    int x0 = 90;
    for (int who = 0; who < 3; who++) {
      const doll::DollRecipe r = recipeOf(who);
      for (int facing = 0; facing < 4; facing++) {
        const art::SeatFit f = art::seatFit2(st.seat, -1, facing);
        const int tx = x0, ty = y0;   // the seat tile's bottom-centre
        if (st.seat != art::Prop::COUNT) {
          const Canvas sp = art::propSprite(st.seat);
          b.put(sp, tx - sp.w / 2, ty - sp.h);
        }
        if (f.ok) {
          const RigSheet s = doll::composePosture(r, f.sit, 0);
          const int ax = facing == 3 ? -f.ax : f.ax;
          putFrame(b, s, facing, 0, tx + ax, ty + f.ay, false);
          for (int k = -11; k <= -9; k++) b.c.set(tx + k, ty - 16 + f.seatY, rgba(242, 193, 78));   // the seat line
        } else b.text(tx - 4, ty - 10, "-", rgba(232, 96, 60));
        x0 += 30;
      }
      x0 += 24;
    }
    y0 += 64;
  }
  save3(b, dir, "poses_seats");
}

void boardRide(const std::string& dir) {
  Board b(760, 190);
  dirt(b, 0, b.c.h);
  const int cw = art::horseCellW(), ch = art::horseCellH();
  int x = 16;
  for (int who = 0; who < 3; who += 2) {
    const RigSheet s = doll::composePosture(recipeOf(who), Posture::Ride, 0);
    for (int facing = 0; facing < 4; facing++) {
      const int row = facing == 0 ? 0 : facing == 1 ? 1 : 2;
      Canvas horse = art::horseCellRig(rgba(140, 92, 56), 3, true, row, 0);
      if (facing == 3) {   // the sheet faces right: the view flips it for left
        Canvas m(horse.w, horse.h);
        for (int yy = 0; yy < horse.h; yy++) for (int xx = 0; xx < horse.w; xx++) m.set(horse.w - 1 - xx, yy, horse.get(xx, yy));
        horse = m;
      }
      const int baseY = 160;
      shadow(b, x + cw / 2, baseY, 30);
      b.put(horse, x, baseY - ch + 2);
      int dx = 0, dy = 0;
      art::saddleSeat(row, 0, dx, dy);
      if (facing == 3) dx = -dx;
      const int seatX = x + cw / 2 + dx, seatY = baseY + dy;
      int px, py;
      const Canvas f = rigpv::frameCanvas(s, facing, Clip::Idle, 0, 0, px, py);
      const art::RigAnchors* an = s.anchorsOf(facing, Clip::Idle, 0, 0);
      if (f.w > 0 && an) b.put(f, seatX - (px + an->hipX), seatY - (py + an->hipY));
      else b.text(x, baseY - 40, "NO RIDE", rgba(232, 96, 60));
      if (row == 0) b.put(art::horseCellRig(rgba(140, 92, 56), 3, true, 3, 0), x, baseY - ch + 2);   // the head and chest over the rider
      else if (row == 1) {
        const int sr = std::clamp(ch - 2 + dy, 0, ch);
        Canvas rump(horse.w, horse.h);
        for (int yy = sr + 1; yy < horse.h; yy++) for (int xx = 0; xx < horse.w; xx++) rump.set(xx, yy, horse.get(xx, yy));
        b.put(rump, x, baseY - ch + 2);
      }
      b.c.set(seatX, seatY, rgba(242, 193, 78));
      x += cw + 6;
    }
    x += 12;
  }
  b.text(4, 4, "RIDE ON THE RIG-2 HORSE (SADDLESEAT): DOWN UP RIGHT LEFT - HERO, WOMAN IN TROUSERS");
  save3(b, dir, "poses_ride");
}

void boardRow(const std::string& dir) {
  Board b(900, 150);
  int x = 30;
  for (int who = 0; who < 3; who += 2) {
    const RigSheet s = doll::composePosture(recipeOf(who), Posture::Row, 0);
    for (int facing = 0; facing < 4; facing++) {
      for (int i = 0; i < s.count(facing, Clip::Idle, 0); i++) {
        const art::RigAnchors* an = s.anchorsOf(facing, Clip::Idle, 0, i);
        const int seatY = 110;
        int px, py;
        const Canvas f = rigpv::frameCanvas(s, facing, Clip::Idle, 0, i, px, py);
        if (f.w > 0 && an) b.put(f, x - px, seatY - (py + an->hipY));
        for (int k = -10; k <= 10; k++) b.c.set(x + k, seatY + 1, rgba(120, 80, 50));   // the thwart
        x += 26;
      }
      x += 10;
    }
    x += 20;
  }
  b.text(4, 4, "ROW: REACH CATCH PULL RECOVER, FOUR FACINGS (HIPS ON THE THWART)");
  save3(b, dir, "poses_row");
}

void boardSleep(const std::string& dir) {
  Board b(1100, 380);
  floorBoards(b);
  for (int who = 0; who < 2; who++) {
    int x = 10;
    const art::HumanLook L = a3Look(who ? 1 : 0);
    for (int k = 0; k < (int)art::Berth::COUNT; k++) {
      const art::BedFit bf = art::bedFit2((art::Berth)k, -1);
      const Canvas berth = art::berthSprite((art::Berth)k, -1, 0);
      for (int f = 0; f < 2; f++) {
        const int baseY = 160 + who * 180;
        const Canvas c = doll::sleeper(L, (art::Berth)k, -1, f);
        const int w = std::max<int>(std::max<int>(berth.w, c.w), bf.w);
        if (berth.w > 0) b.put(berth, x + (w - berth.w) / 2, baseY - berth.h);
        if (c.w > 0) b.put(c, x + (w - c.w) / 2, baseY - c.h);
        else b.text(x, baseY - 20, "-", rgba(232, 96, 60));
        x += std::max(30, w + 8);
      }
      x += 8;
    }
  }
  b.text(4, 4, "SLEEPERS IN THE 10 BERTHS (BREATH 0, 1): BED LONGBED BUNKLOW BUNKHIGH HAMMOCK LONGHAMMOCK MAT LONGMAT BEDROLL GROUND");
  save3(b, dir, "poses_sleep");
}

void boardVariants(const std::string& dir) {
  struct V { Posture p; uint8_t v; const char* n; };
  const V vs[] = {
      {Posture::Lute, (uint8_t)LeadInst::Lute, "LUTE"},       {Posture::Lute, (uint8_t)LeadInst::Oud, "OUD"},
      {Posture::Lute, (uint8_t)LeadInst::Fiddle, "FIDDLE"},   {Posture::Lute, (uint8_t)LeadInst::Harp, "HARP"},
      {Posture::Flute, (uint8_t)LeadInst::Flute, "FLUTE"},    {Posture::Flute, (uint8_t)LeadInst::Pipes, "PIPES"},
      {Posture::Flute, (uint8_t)LeadInst::Reed, "REED"},      {Posture::Flute, (uint8_t)LeadInst::Horn, "HORN"},
      {Posture::Flute, (uint8_t)LeadInst::Bells, "BELLS"},    {Posture::Flute, (uint8_t)LeadInst::Voice, "VOICE"},
      {Posture::Drum, (uint8_t)PercKind::Frame, "FRAME"},     {Posture::Drum, (uint8_t)PercKind::Bodhran, "BODHRAN"},
      {Posture::Drum, (uint8_t)PercKind::Taiko, "TAIKO"},     {Posture::Drum, (uint8_t)PercKind::Hand, "HAND"},
      {Posture::Drum, (uint8_t)PercKind::Gong, "GONG"},       {Posture::Drum, (uint8_t)PercKind::Wood, "WOOD"},
      {Posture::Drum, (uint8_t)PercKind::Bells, "JINGLE"},    {Posture::Carry, 1, "CARRY BASKET"},
      {Posture::Carry, 2, "CARRY CRATE"},                     {Posture::Kneel, 0, "KNEEL BIND"},
      {Posture::Kneel, 1, "KNEEL MOURN"},
  };
  const int n = (int)(sizeof(vs) / sizeof(vs[0]));
  Board b(1100, n * 46 + 30);
  dirt(b, 0, b.c.h);
  int y = 46;
  for (const V& v : vs) {
    b.text(4, y - 30, v.n);
    int x = 120;
    for (int who = 0; who < 3; who += 2) {
      const RigSheet s = doll::composePosture(recipeOf(who), v.p, v.v);
      for (int facing = 0; facing < 4; facing++) {
        for (int i = 0; i < s.count(facing, Clip::Idle, 0); i++) { putFrame(b, s, facing, i, x, y); x += 28; }
        x += 8;
      }
      x += 20;
    }
    y += 46;
  }
  save3(b, dir, "poses_variants");
}

void boardFolk(const std::string& dir) {
  static const cult::Archetype arch[4] = {cult::Archetype::Fjordfolk, cult::Archetype::Dune, cult::Archetype::Jade, cult::Archetype::Highland};
  const Posture ps[] = {Posture::Sit, Posture::SitDrink, Posture::Hammer, Posture::Sweep, Posture::SitFloor, Posture::Kneel, Posture::Wave, Posture::Pray};
  Board b(1400, 8 * 50 + 40);
  dirt(b, 0, b.c.h);
  int y = 50;
  for (int k = 0; k < 8; k++) {
    const cult::Culture C = cult::Atlas::make(arch[k / 2], 4242u + (uint32_t)arch[k / 2] * 97u);
    art::HumanLook L;
    std::string name;
    census::dress(L, name, k == 7 ? Role::Priest : Role::Villager, (k & 1) != 0, C, C, 0x5EA7ull * (uint64_t)(k + 3) ^ (uint64_t)arch[k / 2] << 32,
                  census::Rank{});
    if (k == 7) L.age = 3;
    b.text(4, y - 34, name.substr(0, 12));
    int x = 110;
    for (Posture p : ps) {
      const RigSheet s = doll::composePosture(L, p, 0);
      for (int facing = 0; facing < 4; facing++) { putFrame(b, s, facing, 0, x, y); x += 26; }
      x += 12;
    }
    y += 50;
  }
  save3(b, dir, "poses_folk");
}

int check() {
  int bad = 0;
  for (int who = 0; who < 3; who++) {
    const doll::DollRecipe r = recipeOf(who);
    double worstMs = 0;
    size_t worstB = 0;
    for (int p = 1; p < (int)Posture::COUNT; p++) {
      const art::PostureInfo pi = art::postureInfo((Posture)p);
      const auto t0 = std::chrono::steady_clock::now();
      const RigSheet s = doll::composePosture(r, (Posture)p, 0);
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      worstMs = std::max(worstMs, ms); worstB = std::max(worstB, s.bytes());
      const int facings = (Posture)p == Posture::Sleep ? 0 : (pi.oneRow || pi.lying) ? 1 : 4;
      std::string fr;
      bool ok = true;
      for (int f = 0; f < 4; f++) {
        const int n = s.count(f, Clip::Idle, 0);
        fr += std::to_string(n) + (f < 3 ? "/" : "");
        if (f < facings && n != pi.frames) ok = false;
      }
      const std::vector<doll::Missing> m = doll::dollMissingPosture(a3Look(who), (Posture)p, 0);
      std::printf("%s %-6s %-10s frames %s (%d)  %5.1f KB  %.2f ms%s%s\n", ok ? "ok  " : "BAD ", who == 0 ? "hero" : who == 1 ? "woman" : "womant",
                  postureName((Posture)p), fr.c_str(), pi.frames, s.bytes() / 1024.0, ms, m.empty() ? "" : "  lacks: ", m.empty() ? "" : m[0].what().c_str());
      if (!ok && facings) bad++;
    }
    std::printf("postures (%s): worst atlas %.1f KB (64), worst bake %.2f ms\n", who == 0 ? "hero" : who == 1 ? "woman" : "womant", worstB / 1024.0, worstMs);
    for (art::Prop seat : {art::Prop::Chair, art::Prop::Stool, art::Prop::Bench, art::Prop::Throne, art::Prop::Cushion, art::Prop::COUNT})
      for (int facing = 0; facing < 4; facing++) {
        const art::SeatFit f = art::seatFit2(seat, -1, facing);
        if (!f.ok) continue;
        const RigSheet s = doll::composePosture(r, f.sit, 0);
        const art::RigAnchors* an = s.anchorsOf(facing, Clip::Idle, 0, 0);
        const int hip = an ? 16 + f.ay + an->hipY : -99;
        const bool ok = hip == f.seatY;
        if (!ok) bad++;
        std::printf("%s seat %d facing %d: seat line row %d, hips row %d\n", ok ? "ok  " : "BAD ", (int)seat, facing, f.seatY, hip);
      }
  }
  std::printf("doll_poses check: %d bad\n", bad);
  return bad ? 1 : 0;
}

void zoom(const std::string& dir, const std::string& spec) {
  // zoom:<posture>:<facing>[:<frame>[:<who>]]
  std::vector<std::string> t;
  size_t a = 0;
  while (a <= spec.size()) {
    size_t b = spec.find(':', a);
    if (b == std::string::npos) b = spec.size();
    t.push_back(spec.substr(a, b - a));
    a = b + 1;
  }
  if (t.size() < 3) return;
  const int p = postureOf(t[1]);
  const int facing = t[2].empty() ? 0 : (int)(std::strchr("DURL", t[2][0]) ? std::strchr("DURL", t[2][0]) - "DURL" : 0);
  const int fi = t.size() > 3 ? std::atoi(t[3].c_str()) : 0;
  const int who = t.size() > 4 ? std::atoi(t[4].c_str()) : 0;
  const RigSheet s = doll::composePosture(recipeOf(who), (Posture)p, 0);
  int px, py;
  const Canvas f = rigpv::frameCanvas(s, facing, Clip::Idle, 0, fi, px, py);
  if (f.w <= 0) { std::printf("zoom: no frame\n"); return; }
  const int k = 12;
  Canvas z(f.w * k, f.h * k);
  for (int y = 0; y < f.h; y++)
    for (int x = 0; x < f.w; x++) {
      const uint32_t c0 = f.get(x, y);
      for (int dy = 0; dy < k; dy++)
        for (int dx = 0; dx < k; dx++) {
          uint32_t c = (c0 >> 24) ? (c0 | 0xFF000000u) : ((x + y) & 1 ? rgba(58, 54, 66) : rgba(50, 46, 58));
          if (dx == 0 || dy == 0) c = art::shade(c, 0.8f);
          if (x == px && y == py && dy == k - 1) c = rgba(242, 193, 78);
          z.set(x * k + dx, y * k + dy, c);
        }
    }
  savePng(z, dir + "/zoom_" + postureName((Posture)p) + "_" + "DURL"[facing & 3] + std::to_string(fi) + "_" + std::to_string(who) + ".png", 1);
}

// strip:<posture>[:<variant>]: the posture's frames in four facings for the hero, the woman and the woman in trousers, on a
// half grass, half dirt ground with the contact shadow, written at 1x and 6x (poses_strip_<P>[_v]_1x.png / .png)
void strip(const std::string& dir, const std::string& spec) {
  const size_t c1 = spec.find(':', 6);
  const std::string pn = spec.substr(6, c1 == std::string::npos ? std::string::npos : c1 - 6);
  const int p = postureOf(pn);
  const int v = c1 == std::string::npos ? 0 : std::atoi(spec.c_str() + c1 + 1);
  Board b(420, 3 * 44 + 8);
  dirt(b, 0, b.c.h);
  for (int who = 0; who < 3; who++) {
    const RigSheet s = doll::composePosture(recipeOf(who), (Posture)p, (uint8_t)v);
    int x = 20;
    const int y = 40 + who * 44;
    for (int facing = 0; facing < 4; facing++) {
      for (int i = 0; i < s.count(facing, Clip::Idle, 0); i++) { putFrame(b, s, facing, i, x, y); x += 22; }
      x += 8;
    }
  }
  const std::string nm = dir + "/poses_strip_" + postureName((Posture)p) + (v ? "_" + std::to_string(v) : std::string());
  savePng(b.c, nm + "_1x.png", 1);
  savePng(b.c, nm + ".png", 6);
}

// census:<archetype index>:<role index>: the script's `lineup` people (the same seeds: rpg/view/script_p2b.cpp) in nine
// postures, every facing, and what each one's postures lack (printed)
void censusBoard(const std::string& dir, const std::string& spec) {
  int arch = 0, role = 0;
  std::sscanf(spec.c_str(), "census:%d:%d", &arch, &role);
  const cult::Culture C = cult::Atlas::make((cult::Archetype)arch, 4242u + (uint32_t)arch * 97u);
  const Posture ps[] = {Posture::Sit, Posture::SitFloor, Posture::Drink, Posture::Hammer, Posture::Wave, Posture::Pray, Posture::Kneel, Posture::Carry, Posture::Ride};
  Board b(1500, 8 * 46 + 20);
  dirt(b, 0, b.c.h);
  for (int k = 0; k < 8; k++) {
    art::HumanLook L;
    std::string name;
    census::dress(L, name, (Role)role, (k & 1) != 0, C, C, 0x11E0Bull * (uint64_t)(k + 1) ^ (uint64_t)arch << 32 ^ (uint64_t)role << 40, census::Rank{});
    L.age = 0;
    const int y = 40 + k * 46;
    int x = 16;
    for (Posture p : ps) {
      const RigSheet s = doll::composePosture(L, p, 0);
      for (int facing = 0; facing < 4; facing++) { putFrame(b, s, facing, 0, x, y); x += 24; }
      x += 10;
      const std::vector<doll::Missing> m = doll::dollMissingPosture(L, p, 0);
      int shown = 0;
      for (const doll::Missing& mi : m)
        if (mi.role != doll::Role::Held && !mi.part.empty() && shown++ < 3) std::printf("person %d (%s) %s: %s\n", k, name.c_str(), postureName(p), mi.what().c_str());
    }
  }
  save3(b, dir, "poses_census");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::printf("usage: doll_poses <outDir> [--palette look] [poses seats ride row sleep variants folk check zoom:...]\n"); return 2; }
  const std::string dir = argv[1];
  art::setRigVersion(2);
  std::vector<std::string> boards;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--palette") && i + 1 < argc) { g_lookPalette = !std::strcmp(argv[++i], "look"); continue; }
    if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) {   // a pool file (pxc.mjs --bin FILE --no-cpp): no rebuild per map edit
      std::string err;
      if (!doll::loadPoolFile(argv[++i], &err)) { std::printf("doll_poses: --pool: %s\n", err.c_str()); return 1; }
      continue;
    }
    boards.push_back(argv[i]);
  }
  if (boards.empty()) boards = {"poses", "seats", "ride", "row", "sleep", "variants", "folk"};
  int rc = 0;
  for (const std::string& b : boards) {
    if (b == "poses") { boardPoses(dir, 0, "poses_hero"); boardPoses(dir, 1, "poses_woman"); boardPoses(dir, 2, "poses_womant"); }
    else if (b == "seats") boardSeats(dir);
    else if (b == "ride") boardRide(dir);
    else if (b == "row") boardRow(dir);
    else if (b == "sleep") boardSleep(dir);
    else if (b == "variants") boardVariants(dir);
    else if (b == "folk") boardFolk(dir);
    else if (b == "check") rc |= check();
    else if (b.rfind("zoom:", 0) == 0) zoom(dir, b);
    else if (b.rfind("strip:", 0) == 0) strip(dir, b);
    else if (b.rfind("census:", 0) == 0) censusBoard(dir, b);
    else std::printf("doll_poses: unknown board %s\n", b.c_str());
  }
  return rc;
}
