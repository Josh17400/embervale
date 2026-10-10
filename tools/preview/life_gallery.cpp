// Life gallery (M5 "Hearth and Hall", ART lane): the townsfolk's postures, seated and sleeping figures on the real
// furniture, the village animals, the speech bubbles and the festival dressing (rpg/art/art_life.h).
//   life_gallery <outDir>   writes (each at 1x as *_1x.png and zoomed)
//     postures_<group>.png   every posture x 3 facings x its frames, for a row of looks (plain, dress, robe, rags, guard,
//                            the 12 culture villagers, an elf, a half-breed, a child, an elder)
//     bards.png              the bard's instruments by culture style (lute, oud, fiddle, harp; flute, pipes, shawm, horn,
//                            bells, voice; frame drum, bodhran, barrel drum, hand drums, gong, clappers, jingle ring)
//     seats.png              sitters composited on every seat kind (chair, stool, bench, throne, cushion, the floor) in
//                            every facing it takes, with a table where one belongs
//     beds.png               sleepers in every berth (classic and the culture beds, bunks, hammocks, mats, the bedroll,
//                            the ground) and the generic Sleep cell over the long bed by bedFit
//     critters.png           every animal x its coats x 3 facings x 8 frames
//     bubbles.png, festival.png
//   life_gallery --check     verifies every cell (non-empty, inside its cell with room for its outline, outlined), the
//                            seated figures against seatFit (the seat under the sitter, the feet on the floor), the
//                            sleepers inside their berths, the critters standing on their row; non-zero on failure
#include <cctype>
#include <initializer_list>

#include "rpg/culture/culture.h"
#include "rpg/sim/world.h"
#include "tools/preview/preview_util.h"

namespace census {
struct Rank { bool royal = false, capital = false; };
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner,
           uint64_t seed, const Rank& rank);
}  // namespace census

namespace {

using art::HumanLook;
using art::Posture;

const char* const kPostureName[] = {"NONE", "SIT", "SITEAT", "SITDRNK", "EAT", "DRINK", "CHEER", "HAMMER", "HOE", "SWEEP",
                                    "CHOP", "STIR", "CARRY", "FISH", "SLEEP", "WAVE", "PLAY", "DANCE", "LUTE", "DRUM",
                                    "FLUTE", "PRAY", "BEG", "LAMP", "READ", "SITFLR", "FLREAT", "FLRDRNK", "RIDE"};
static_assert(sizeof(kPostureName) / sizeof(kPostureName[0]) == (size_t)Posture::COUNT, "posture names");

Canvas cellOf(const Canvas& sheet, int col, int row, int w, int h) {
  Canvas c(w, h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) c.set(x, y, sheet.get(col * w + x, row * h + y));
  return c;
}
void blitOver(Canvas& dst, const Canvas& src, int x0, int y0, bool flip = false) {
  for (int y = 0; y < src.h; y++)
    for (int x = 0; x < src.w; x++) {
      const uint32_t p = src.get(flip ? src.w - 1 - x : x, y);
      const int a = (int)(p >> 24);
      if (!a) continue;
      if (a == 255) { dst.set(x0 + x, y0 + y, p); continue; }
      const uint32_t d = dst.get(x0 + x, y0 + y);
      dst.set(x0 + x, y0 + y, art::mix(d | 0xFF000000u, p | 0xFF000000u, a / 255.0f));
    }
}
// a dark wooden floor board pattern to show interior furniture on
void floorPatch(Board& b, int x0, int y0, int w, int h) {
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++) {
      const int plank = (y - y0) / 4;
      uint32_t col = ((x + plank * 7) % 23 == 0 || (y - y0) % 4 == 3) ? rgba(82, 60, 54) : rgba(114, 86, 68);
      if (hash2(x / 3, plank) % 5 == 0 && (y - y0) % 4 != 3) col = rgba(124, 96, 74);
      b.c.set(x, y, col);
    }
}
void contactShadow(Board& b, int cx, int cy, int rx, int ry) {
  for (int y = cy - ry; y <= cy + ry; y++)
    for (int x = cx - rx; x <= cx + rx; x++) {
      const float d = ((x - cx) * (x - cx)) / (float)(rx * rx) + ((y - cy) * (y - cy)) / (float)(ry * ry);
      if (d <= 1.0f) b.c.set(x, y, art::shade(b.c.get(x, y), 0.74f));
    }
}

// ---------------------------------------------------------------- looks
struct Look { std::string name; HumanLook L; };
HumanLook plainLook() {
  HumanLook L;
  L.skin = rgba(226, 176, 132); L.hairColor = rgba(96, 60, 34); L.topColor = rgba(70, 110, 160); L.bottomColor = rgba(84, 66, 50);
  return L;
}
std::vector<Look> looks() {
  std::vector<Look> v;
  { HumanLook L = plainLook(); v.push_back({"TUNIC", L}); }
  { HumanLook L = plainLook(); L.outfit = art::Outfit::Dress; L.hair = art::Hair::Long; L.topColor = rgba(150, 60, 70); L.hairColor = rgba(170, 110, 50); v.push_back({"DRESS", L}); }
  { HumanLook L = plainLook(); L.outfit = art::Outfit::Robe; L.topColor = rgba(232, 234, 244); L.tabardColor = rgba(150, 34, 48); L.hair = art::Hair::Bald; L.beard = true; L.hairColor = rgba(200, 200, 196); v.push_back({"PRIEST", L}); }
  { HumanLook L = plainLook(); L.outfit = art::Outfit::Rags; L.hair = art::Hair::Curls; L.skin = rgba(150, 100, 70); v.push_back({"RAGS", L}); }
  { HumanLook L = plainLook(); L.outfit = art::Outfit::Guard; L.helmet = true; L.tabardColor = rgba(40, 70, 150); v.push_back({"GUARD", L}); }
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++) {
    const cult::Culture C = cult::Atlas::make((cult::Archetype)a, 1234u + (uint32_t)a * 7919u);
    HumanLook L = plainLook();
    const bool female = (a & 1) != 0;
    L.hair = female ? art::Hair::Long : art::Hair::Short;
    std::string name;
    census::Rank rank;
    census::dress(L, name, Role::Villager, female, C, C, 77u + (uint64_t)a * 131, rank);
    std::string nm = cult::archetypeName((cult::Archetype)a);
    v.push_back({nm.substr(0, 7), L});
  }
  { HumanLook L = plainLook(); L.people = 2; L.hair = art::Hair::Long; L.topColor = rgba(80, 130, 90); L.cut = 8; L.patternColor = rgba(220, 200, 120); v.push_back({"ELF", L}); }
  { HumanLook L = plainLook(); L.people = 1; L.cut = 7; L.topColor = rgba(170, 90, 50); L.pattern = 1; L.patternColor = rgba(230, 200, 100); v.push_back({"HALF/PON", L}); }
  { HumanLook L = plainLook(); L.hair = art::Hair::Ponytail; L.topColor = rgba(200, 160, 60); v.push_back({"CHILD", L}); }
  { HumanLook L = plainLook(); L.hair = art::Hair::Short; L.hairColor = rgba(210, 210, 206); L.beard = true; L.topColor = rgba(110, 90, 70); L.cloak = 1; L.cloakColor = rgba(70, 90, 60); v.push_back({"ELDER", L}); }
  { HumanLook L = plainLook(); L.cut = 6; L.topColor = rgba(60, 110, 70); L.headwear = 2; L.headColor = rgba(90, 70, 60); v.push_back({"KILT/CAP", L}); }
  return v;
}

// ---------------------------------------------------------------- the posture sheets
void postureSheets(const std::string& dir) {
  const std::vector<Look> lk = looks();
  // groups of postures per image so each reads at 4x
  const std::vector<std::vector<Posture>> groups = {
      {Posture::Sit, Posture::SitEat, Posture::SitDrink, Posture::SitFloor, Posture::SitFloorEat, Posture::SitFloorDrink, Posture::Beg},
      {Posture::Eat, Posture::Drink, Posture::Cheer, Posture::Wave, Posture::Pray, Posture::Read, Posture::Play, Posture::Dance},
      {Posture::Hammer, Posture::Hoe, Posture::Sweep, Posture::Chop, Posture::Stir, Posture::Carry, Posture::Fish, Posture::Lamp},
      {Posture::Lute, Posture::Drum, Posture::Flute, Posture::Sleep},
  };
  const char* gname[] = {"seated", "social", "work", "music"};
  for (size_t gi = 0; gi < groups.size(); gi++) {
    const auto& G = groups[gi];
    // columns: per posture, 3 facings x its frames; rows: looks
    int colsW = 0;
    std::vector<int> nf;
    for (Posture p : G) { nf.push_back(art::postureInfo(p).frames); colsW += 3 * nf.back() * 16 + 10; }
    Board b(80 + colsW, 14 + (int)lk.size() * 27);
    int x = 80;
    for (size_t i = 0; i < G.size(); i++) { b.text(x, 2, kPostureName[(int)G[i]]); x += 3 * nf[i] * 16 + 10; }
    for (size_t li = 0; li < lk.size(); li++) {
      const int y = 12 + (int)li * 27;
      b.text(2, y + 9, lk[li].name.substr(0, 8));
      x = 80;
      for (size_t i = 0; i < G.size(); i++) {
        const Canvas sh = art::humanPostureSheet(lk[li].L, G[i]);
        for (int row = 0; row < 3; row++)
          for (int f = 0; f < nf[i]; f++) b.put(cellOf(sh, f, row, 16, 24), x + (row * nf[i] + f) * 16, y);
        x += 3 * nf[i] * 16 + 10;
      }
    }
    savePng(b.c, dir + "/postures_" + gname[gi] + ".png", 3);
    savePng(b.c, dir + "/postures_" + gname[gi] + "_1x.png", 1);
  }
}

void bards(const std::string& dir) {
  struct V { Posture p; uint8_t v; const char* n; };
  const V vs[] = {{Posture::Lute, (uint8_t)LeadInst::Lute, "LUTE"}, {Posture::Lute, (uint8_t)LeadInst::Oud, "OUD"},
                  {Posture::Lute, (uint8_t)LeadInst::Fiddle, "FIDDLE"}, {Posture::Lute, (uint8_t)LeadInst::Harp, "HARP"},
                  {Posture::Flute, (uint8_t)LeadInst::Flute, "FLUTE"}, {Posture::Flute, (uint8_t)LeadInst::Pipes, "PIPES"},
                  {Posture::Flute, (uint8_t)LeadInst::Reed, "SHAWM"}, {Posture::Flute, (uint8_t)LeadInst::Horn, "HORN"},
                  {Posture::Flute, (uint8_t)LeadInst::Bells, "BELLS"}, {Posture::Flute, (uint8_t)LeadInst::Voice, "VOICE"},
                  {Posture::Drum, (uint8_t)PercKind::Frame, "FRAME"}, {Posture::Drum, (uint8_t)PercKind::Bodhran, "BODHRAN"},
                  {Posture::Drum, (uint8_t)PercKind::Taiko, "TAIKO"}, {Posture::Drum, (uint8_t)PercKind::Tabla, "TABLA"},
                  {Posture::Drum, (uint8_t)PercKind::Gong, "GONG"}, {Posture::Drum, (uint8_t)PercKind::Wood, "CLAPPER"},
                  {Posture::Drum, (uint8_t)PercKind::Bells, "JINGLE"}};
  const int n = (int)(sizeof(vs) / sizeof(vs[0]));
  Board b(70 + 2 * (6 * 16 + 8), 12 + n * 27);
  std::vector<Look> lk = looks();
  for (int i = 0; i < n; i++) {
    const int y = 10 + i * 27;
    b.text(2, y + 9, vs[i].n);
    for (int k = 0; k < 2; k++) {
      const HumanLook& L = lk[k == 0 ? 0 : 6 + (i % 10)].L;
      const Canvas sh = art::humanPostureSheet(L, vs[i].p, vs[i].v);
      for (int row = 0; row < 3; row++)
        for (int f = 0; f < 2; f++) b.put(cellOf(sh, f, row, 16, 24), 70 + k * (6 * 16 + 8) + (row * 2 + f) * 16, y);
    }
  }
  savePng(b.c, dir + "/bards.png", 4);
  savePng(b.c, dir + "/bards_1x.png", 1);
}

// ---------------------------------------------------------------- seats
// a sitter on a seat drawn the way the view does it: the seat at its anchor (bottom-centre on the tile's bottom-centre),
// the figure's cell at (p.x - 8, p.y - 22) with p = tile bottom-centre + (ax, ay)
struct SeatShot { art::Prop seat; int facing; };
void drawSeated(Canvas& c, int tx, int ty, art::Prop seat, int facing, const HumanLook& L, Posture p, int frame, bool table) {
  const art::SeatFit f = art::seatFit(seat, -1, facing);
  const Posture sp = art::seatedPosture(seat, p);
  const int bx = tx + 8, bottom = ty + 16;
  // a table north (sitter facing up: the table is above it) or south (facing down)
  auto drawTable = [&](int ttx, int tty) { const Canvas t = art::propSprite(art::Prop::TableSmall); blitOver(c, t, ttx + 8 - t.w / 2, tty + 16 - t.h); };
  if (table && facing == 1) drawTable(tx, ty - 16);
  if (seat != art::Prop::COUNT) {
    const Canvas s = art::propSprite(seat);
    const int fw = art::propW(seat);
    Canvas one(fw, s.h);
    for (int y = 0; y < s.h; y++) for (int x = 0; x < fw; x++) one.set(x, y, s.get(x, y));
    if (f.front) blitOver(c, one, bx - fw / 2, bottom - one.h);
  }
  if (f.ok) {
    const Canvas sh = art::humanPostureSheet(L, sp);
    const int row = facing == 3 ? 2 : facing;
    const Canvas cell = cellOf(sh, frame % std::max(1, (int)art::postureInfo(sp).frames), row, 16, 24);
    const int px = bx + f.ax, py = bottom + f.ay;
    blitOver(c, cell, px - 8, py - 22 + art::postureInfo(sp).dy, facing == 3);
  }
  if (table && facing == 0) drawTable(tx, ty + 16);
}
void seats(const std::string& dir) {
  const std::vector<art::Prop> kinds = {art::Prop::Chair, art::Prop::Stool, art::Prop::Bench, art::Prop::Throne, art::Prop::Cushion, art::Prop::COUNT};
  const char* kn[] = {"CHAIR", "STOOL", "BENCH", "THRONE", "CUSHION", "FLOOR"};
  std::vector<Look> lk = looks();
  const int pick[] = {0, 1, 2, 9, 12, 18};
  const Posture ps[] = {Posture::Sit, Posture::SitEat, Posture::SitDrink};
  Board b(60 + 6 * 4 * 26, 16 + (int)kinds.size() * 3 * 44);
  for (size_t k = 0; k < kinds.size(); k++)
    for (int pi = 0; pi < 3; pi++) {
      const int y0 = 10 + ((int)k * 3 + pi) * 44;
      b.text(2, y0 + 16, kn[k]);
      b.text(2, y0 + 25, kPostureName[(int)art::seatedPosture(kinds[k], ps[pi])]);
      floorPatch(b, 58, y0, 6 * 4 * 26, 44);
      for (int li = 0; li < 6; li++)
        for (int fc = 0; fc < 4; fc++) {
          const int tx = 60 + (li * 4 + fc) * 26 + 4, ty = y0 + 20;
          contactShadow(b, tx + 8, ty + 14, 6, 2);
          drawSeated(b.c, tx, ty, kinds[k], fc, lk[pick[li]].L, ps[pi], 1, kinds[k] != art::Prop::Throne && kinds[k] != art::Prop::COUNT && (fc == 0 || fc == 1));
        }
    }
  savePng(b.c, dir + "/seats.png", 3);
  savePng(b.c, dir + "/seats_1x.png", 1);
}

// ---------------------------------------------------------------- beds
struct BerthShot { art::Berth b; int kit; const char* n; };
const BerthShot kBerths[] = {
    {art::Berth::Bed, -1, "BED"}, {art::Berth::LongBed, -1, "LONGBED"}, {art::Berth::LongBed, 7, "JADE"}, {art::Berth::LongBed, 4, "DUNE"},
    {art::Berth::LongBed, 10, "SYLVAN"}, {art::Berth::LongBed, 11, "STAR"}, {art::Berth::Bed, 5, "STEPPE"}, {art::Berth::Bed, 6, "MARSH"},
    {art::Berth::BunkLow, -1, "BUNK LO"}, {art::Berth::BunkHigh, -1, "BUNK HI"}, {art::Berth::LongHammock, -1, "HAMMOCK2"},
    {art::Berth::LongMat, -1, "MAT2"}, {art::Berth::Hammock, -1, "HAMMOCK"}, {art::Berth::Mat, -1, "MAT"},
    {art::Berth::Bedroll, -1, "BEDROLL"}, {art::Berth::Ground, -1, "GROUND"}};
void beds(const std::string& dir) {
  std::vector<Look> lk = looks();
  const int n = (int)(sizeof(kBerths) / sizeof(kBerths[0]));
  const int pick[] = {0, 1, 3, 9, 14, 18};
  Board b(70 + 7 * 34, 12 + n * 54);
  for (int i = 0; i < n; i++) {
    const int y0 = 8 + i * 54;
    b.text(2, y0 + 20, kBerths[i].n);
    floorPatch(b, 66, y0, 7 * 34, 52);
    for (int li = 0; li < 7; li++) {
      const int x0 = 70 + li * 34;
      const art::BedFit fit = art::bedFit(kBerths[i].b, kBerths[i].kit);
      const Canvas bed = art::berthSprite(kBerths[i].b, kBerths[i].kit, li);
      const int bx = x0 + 14 - bed.w / 2, by = y0 + 50 - bed.h;
      blitOver(b.c, bed, bx, by);
      if (li < 6) blitOver(b.c, art::sleeperSprite(lk[pick[li]].L, kBerths[i].b, kBerths[i].kit, li & 1), bx, by);
      else if (!fit.across && kBerths[i].b != art::Berth::BunkLow) {   // the generic Sleep cell placed by bedFit's ax / ay
        const Canvas sh = art::humanPostureSheet(lk[0].L, Posture::Sleep);
        const int px = x0 + 14 + fit.ax, py = y0 + 50 + fit.ay;   // the anchor tile's bottom-centre is the canvas's
        blitOver(b.c, cellOf(sh, 0, 0, 16, 24), px - 8, py - 22);
      }
    }
  }
  savePng(b.c, dir + "/beds.png", 3);
  savePng(b.c, dir + "/beds_1x.png", 1);
}

// ---------------------------------------------------------------- critters, bubbles, festival
const char* const kCritName[] = {"DOG", "CAT", "HEN", "ROOSTER", "GOAT", "PIG", "DUCK", "COW", "SHEEP", "HORSE"};
static_assert(sizeof(kCritName) / sizeof(kCritName[0]) == (size_t)art::Critter::COUNT, "critter names");
int coatsOf(art::Critter c) {
  switch (c) {
    case art::Critter::Dog: case art::Critter::Cat: return 6;
    case art::Critter::Chicken: return 5;
    case art::Critter::Goat: case art::Critter::Pig: case art::Critter::Cow: case art::Critter::Horse: return 4;
    case art::Critter::Sheep: return 6;   // (v 4, 5: shorn, by sheepVariant)
    default: return 3;
  }
}
// the variant shown for coat index v (sheep 4 and 5: shorn coats, variant bit 7)
uint32_t critterVariant(art::Critter c, int v) { return c == art::Critter::Sheep && v >= 4 ? (uint32_t)(128 + v - 4) : (uint32_t)v; }
void critters(const std::string& dir) {
  int H = 10;
  for (int k = 0; k < (int)art::Critter::COUNT; k++) H += coatsOf((art::Critter)k) * (art::critterCellH((art::Critter)k) * 3 + 4) + 10;
  int cwMax = 22;
  for (int k = 0; k < (int)art::Critter::COUNT; k++) cwMax = std::max(cwMax, art::critterCellW((art::Critter)k));
  Board b(70 + 8 * cwMax, H);
  int y = 6;
  for (int k = 0; k < (int)art::Critter::COUNT; k++) {
    const art::Critter cr = (art::Critter)k;
    const int cw = art::critterCellW(cr), ch = art::critterCellH(cr);
    b.text(2, y + 4, kCritName[k]);
    for (int v = 0; v < coatsOf(cr); v++) {
      const Canvas sh = art::critterSheet(cr, critterVariant(cr, v));
      for (int row = 0; row < 3; row++)
        for (int f = 0; f < art::CRITTER_FRAMES; f++) b.put(cellOf(sh, f, row, cw, ch), 70 + f * cwMax, y + row * ch);
      y += ch * 3 + 4;
    }
    y += 10;
  }
  savePng(b.c, dir + "/critters.png", 3);
  savePng(b.c, dir + "/critters_1x.png", 1);
  // the animals among people at 1x and 2x: scale check
  Board s(380, 60);
  std::vector<Look> lk = looks();
  int x = 6;
  for (int k = 0; k < (int)art::Critter::COUNT; k++) {
    const art::Critter cr = (art::Critter)k;
    const int cw = art::critterCellW(cr), ch = art::critterCellH(cr);
    const Canvas sh = art::critterSheet(cr, (uint32_t)k);
    contactShadow(s, x + cw / 2, 40, cw / 3, 1);
    s.put(cellOf(sh, 0, 2, cw, ch), x, 42 - ch);
    x += cw + 2;
    if (k == 1 || k == 4) { const Canvas hs = art::humanSheet(lk[(size_t)k].L); contactShadow(s, x + 8, 40, 5, 1); s.put(cellOf(hs, 0, 0, 16, 24), x, 42 - 24 + 2 - 2); x += 18; }
  }
  savePng(s.c, dir + "/critters_scale.png", 4);
}
void bubbles(const std::string& dir) {
  Board b(16 + (int)art::Bubble::COUNT * 16, 60);
  std::vector<Look> lk = looks();
  for (int i = 1; i < (int)art::Bubble::COUNT; i++) {
    const int x = 8 + (i - 1) * 16;
    b.put(art::bubbleSprite((art::Bubble)i), x + 2, 4);
    const Canvas hs = art::humanSheet(lk[(size_t)i % lk.size()].L);
    b.put(cellOf(hs, 0, 0, 16, 24), x, 30);
    b.put(art::bubbleSprite((art::Bubble)i), x + 3, 30 - 9);
  }
  savePng(b.c, dir + "/bubbles.png", 5);
  savePng(b.c, dir + "/bubbles_1x.png", 1);
}
void festival(const std::string& dir) {
  Board b(260, 120);
  const uint32_t cols[][2] = {{rgba(190, 50, 50), rgba(240, 200, 90)}, {rgba(50, 90, 170), rgba(236, 230, 210)}, {rgba(60, 130, 80), rgba(220, 170, 60)}};
  for (int i = 0; i < 3; i++) b.put(art::festivalBunting(70 + i * 20, cols[i][0], cols[i][1], (uint32_t)i), 4 + i * 82, 6);
  for (int i = 0; i < 6; i++) b.put(art::festivalLantern(i % 2 ? rgba(220, 70, 50) : rgba(236, 180, 70), (uint32_t)i), 8 + i * 14, 40);
  // a lamppost lit at dusk (darkened board), the flame frames
  for (int y = 60; y < 120; y++) for (int x = 0; x < 260; x++) b.c.set(x, y, art::shade(b.c.get(x, y), 0.45f));
  const Canvas lp = art::propSprite(art::Prop::Lamppost);
  const Canvas fl = art::lampFlame();
  int hx, hy;
  art::lampHead(hx, hy);
  for (int f = 0; f < art::LAMP_FLAME_FRAMES; f++) {
    const int x0 = 10 + f * 24, y0 = 70;
    blitOver(b.c, lp, x0, y0);
    blitOver(b.c, cellOf(fl, f, 0, art::LAMP_FLAME_W, art::LAMP_FLAME_H), x0 + hx - art::LAMP_FLAME_W / 2, y0 + hy - art::LAMP_FLAME_H + 1);
  }
  savePng(b.c, dir + "/festival.png", 4);
}

// ================================================================ --check
int g_fail = 0, g_cells = 0;
void fail(const std::string& what) { if (g_fail < 400 || std::getenv("LIFE_ALL")) std::printf("FAIL %s\n", what.c_str()); g_fail++; }
// a raw (pre-outline) cell: non-empty and inside the cell with a pixel's room for its outline on every side but the bottom
bool rawInside(const Canvas& c, int& ox0, int& oy0, int& ox1, int& oy1) {
  ox0 = c.w; oy0 = c.h; ox1 = -1; oy1 = -1;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++)
      if (c.get(x, y) >> 24) { ox0 = std::min(ox0, x); oy0 = std::min(oy0, y); ox1 = std::max(ox1, x); oy1 = std::max(oy1, y); }
  return ox1 >= 0;
}
// the final cell: every pixel of the raw figure that borders the outside is wrapped by the outline
bool outlined(const Canvas& raw, const Canvas& fin) {
  for (int y = 0; y < raw.h; y++)
    for (int x = 0; x < raw.w; x++) {
      if (!(raw.get(x, y) >> 24)) continue;
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        const int nx = x + dx[k], ny = y + dy[k];
        if (nx < 0 || ny < 0 || nx >= raw.w || ny >= raw.h) continue;
        if (!(fin.get(nx, ny) >> 24)) return false;
      }
    }
  return true;
}
void checkPostures() {
  const std::vector<Look> lk = looks();
  for (size_t li = 0; li < lk.size(); li++)
    for (int p = 1; p < (int)Posture::COUNT; p++) {
      const Posture P = (Posture)p;
      if (P == Posture::Sleep) continue;
      const art::PostureInfo pi = art::postureInfo(P);
      const Canvas sh = art::humanPostureSheet(lk[li].L, P);
      std::vector<uint8_t> vars = {0};
      if (P == Posture::Lute) vars = {(uint8_t)LeadInst::Lute, (uint8_t)LeadInst::Oud, (uint8_t)LeadInst::Fiddle, (uint8_t)LeadInst::Harp};
      if (P == Posture::Flute) vars = {(uint8_t)LeadInst::Flute, (uint8_t)LeadInst::Pipes, (uint8_t)LeadInst::Reed, (uint8_t)LeadInst::Horn, (uint8_t)LeadInst::Bells, (uint8_t)LeadInst::Voice};
      if (P == Posture::Drum) vars = {(uint8_t)PercKind::Frame, (uint8_t)PercKind::Bodhran, (uint8_t)PercKind::Taiko, (uint8_t)PercKind::Tabla, (uint8_t)PercKind::Gong, (uint8_t)PercKind::Wood, (uint8_t)PercKind::Bells};
      for (uint8_t v : vars) {
        const Canvas shv = art::humanPostureSheet(lk[li].L, P, v);
        for (int row = 0; row < 3; row++)
          for (int f = 0; f < pi.frames; f++) {
            g_cells++;
            const Canvas raw = art::humanPostureCellRaw(lk[li].L, P, v, row, f);
            const Canvas fin = cellOf(shv, f, row, 16, 24);
            const std::string id = lk[li].name + " " + kPostureName[p] + " v" + std::to_string(v) + " row " + std::to_string(row) + " f" + std::to_string(f);
            int x0, y0, x1, y1;
            if (!rawInside(raw, x0, y0, x1, y1)) { fail(id + ": empty"); continue; }
            if (x0 < 1 || x1 > 14 || y0 < 1) fail(id + ": leaves the cell (" + std::to_string(x0) + "," + std::to_string(y0) + ")-(" + std::to_string(x1) + "," + std::to_string(y1) + ")");
            if (!outlined(raw, fin)) fail(id + ": not outlined");
            // the feet stay on the ground row (standing figures) / the seat (seated): nothing below row 23
            if (y1 > 23) fail(id + ": below the cell");
          }
        (void)sh;
      }
    }
}
void checkSeats() {
  const std::vector<Look> lk = looks();
  const std::vector<art::Prop> kinds = {art::Prop::Chair, art::Prop::Stool, art::Prop::Bench, art::Prop::Throne, art::Prop::Cushion, art::Prop::COUNT};
  for (art::Prop s : kinds)
    for (int fc = 0; fc < 4; fc++) {
      const art::SeatFit f = art::seatFit(s, -1, fc);
      if (!f.ok) continue;
      const Posture sp = art::seatedPosture(s, Posture::Sit);
      if (sp != f.sit) fail(std::string("seat ") + std::to_string((int)s) + ": seatedPosture disagrees with seatFit.sit");
      const bool floor = sp == Posture::SitFloor;
      // where the cell's seat row lands in the seat's tile
      const int cellTop = 16 + f.ay - 22 + art::postureInfo(sp).dy;   // tile-relative (0 = the tile's top)
      const int seatRow = cellTop + (floor ? art::SEAT_ROW_FLOOR : art::SEAT_ROW);
      if (std::abs(seatRow - (f.seatY + (floor ? 3 : 3))) > 3) fail("seat " + std::to_string((int)s) + " facing " + std::to_string(fc) + ": seat row " + std::to_string(seatRow) + " vs seatY " + std::to_string(f.seatY));
      // the seat is under the sitter: at the sitter's seat line the seat sprite is solid under its body's middle
      if (s != art::Prop::COUNT) {
        const Canvas sp2 = art::propSprite(s);
        const int fw = art::propW(s), top = 16 - sp2.h;   // the sprite's top row in tile coordinates
        bool under = false;
        for (int dy = -2; dy <= 2 && !under; dy++)
          for (int dx = -2; dx <= 2 && !under; dx++) {
            const int sx = fw / 2 + dx + (fc >= 2 ? (fc == 2 ? -1 : 1) : 0), sy = f.seatY + dy - top;
            if (sp2.get(sx, sy) >> 24) under = true;
          }
        if (!under) fail("seat " + std::to_string((int)s) + " facing " + std::to_string(fc) + ": no seat under the sitter");
      }
      // the feet (the cell's lowest pixel) stay on the seat's own tile
      for (size_t li = 0; li < lk.size(); li++) {
        const Canvas raw = art::humanPostureCellRaw(lk[li].L, sp, 0, fc == 3 ? 2 : fc, 0);
        int x0, y0, x1, y1;
        rawInside(raw, x0, y0, x1, y1);
        if (cellTop + y1 > 15) fail("seat " + std::to_string((int)s) + " facing " + std::to_string(fc) + " " + lk[li].name + ": the feet leave the tile");
      }
    }
}
void checkBeds() {
  const std::vector<Look> lk = looks();
  for (const BerthShot& bs : kBerths) {
    if (bs.b == art::Berth::Ground) continue;
    const Canvas bed = art::berthSprite(bs.b, bs.kit, 0);
    const art::BedFit fit = art::bedFit(bs.b, bs.kit);
    if (bed.w != fit.w || bed.h != fit.h) fail(std::string(bs.n) + ": bedFit size " + std::to_string(fit.w) + "x" + std::to_string(fit.h) + " vs the berth " + std::to_string(bed.w) + "x" + std::to_string(bed.h));
    for (size_t li = 0; li < lk.size(); li += 3)
      for (int fr = 0; fr < 2; fr++) {
        const Canvas s = art::sleeperSprite(lk[li].L, bs.b, bs.kit, fr);
        g_cells++;
        // the berth's footprint: per row, from its leftmost to its rightmost pixel (an arched headboard's corners and a
        // hammock's sag between its posts count as the berth), a pixel of slack for the outline
        std::vector<int> rx0(bed.h, 1 << 20), rx1(bed.h, -1);
        for (int y = 0; y < bed.h; y++)
          for (int x = 0; x < bed.w; x++)
            if (bed.get(x, y) >> 24) { rx0[y] = std::min(rx0[y], x); rx1[y] = std::max(rx1[y], x); }
        int n = 0, out = 0, fx = -1, fy = -1;
        for (int y = 0; y < s.h; y++)
          for (int x = 0; x < s.w; x++) {
            if (!(s.get(x, y) >> 24)) continue;
            n++;
            bool on = false;
            for (int dy = -2; dy <= 2 && !on; dy++) {
              const int yy = y + dy;
              if (yy >= 0 && yy < bed.h && x >= rx0[yy] - 1 && x <= rx1[yy] + 1) on = true;
            }
            if (!on) { if (!out) { fx = x; fy = y; } out++; }
          }
        if (!n) fail(std::string(bs.n) + ": empty sleeper");
        if (out > 0) fail(std::string(bs.n) + " " + lk[li].name + ": " + std::to_string(out) + " px of the sleeper off the berth (first at " + std::to_string(fx) + "," + std::to_string(fy) + ")");
        if (fit.headX < 2 || fit.headX >= fit.w - 2 || fit.headY < 1 || fit.headY >= fit.h) fail(std::string(bs.n) + ": head outside");
      }
  }
}
void checkCritters() {
  for (int k = 0; k < (int)art::Critter::COUNT; k++) {
    const art::Critter cr = (art::Critter)k;
    const int cw = art::critterCellW(cr), ch = art::critterCellH(cr);
    for (int v = 0; v < coatsOf(cr); v++) {
      const Canvas sh = art::critterSheet(cr, critterVariant(cr, v));
      if (sh.w != cw * art::CRITTER_FRAMES || sh.h != ch * 3) fail(std::string(kCritName[k]) + ": sheet size");
      for (int row = 0; row < 3; row++)
        for (int f = 0; f < art::CRITTER_FRAMES; f++) {
          g_cells++;
          const Canvas c = cellOf(sh, f, row, cw, ch);
          int x0, y0, x1, y1;
          const std::string id = std::string(kCritName[k]) + " v" + std::to_string(v) + " row " + std::to_string(row) + " f" + std::to_string(f);
          if (!rawInside(c, x0, y0, x1, y1)) { fail(id + ": empty"); continue; }
          if (x0 < 0 || x1 > cw - 1 || y0 < 0) fail(id + ": leaves the cell");
          // a pixel on the cell's edge must be the outline (darker than the body pixel inside it), never the body cut off
          {
            auto lum = [](uint32_t p) { return (int)(p & 255) * 3 + (int)((p >> 8) & 255) * 6 + (int)((p >> 16) & 255); };
            int cut = 0;
            for (int y = 0; y < ch; y++)
              for (int x = 0; x < cw; x++) {
                if (!(x == 0 || x == cw - 1 || y == 0) || !(c.get(x, y) >> 24)) continue;
                const int ix = x == 0 ? 1 : (x == cw - 1 ? cw - 2 : x), iy = y == 0 ? 1 : y;
                const uint32_t in = c.get(ix, iy);
                if ((in >> 24) && lum(c.get(x, y)) >= lum(in)) cut++;
                if (!(in >> 24)) cut++;
              }
            if (cut) fail(id + ": the body reaches the cell edge (" + std::to_string(cut) + " px)");
          }
          if (y1 < ch - 2 || y1 > ch - 1) fail(id + ": not standing on its row (lowest " + std::to_string(y1) + ")");
        }
    }
  }
}
void checkBubbles() {
  for (int i = 1; i < (int)art::Bubble::COUNT; i++) {
    g_cells++;
    const Canvas b = art::bubbleSprite((art::Bubble)i);
    int glyph = 0;
    for (int y = 1; y <= 6; y++) for (int x = 1; x <= 9; x++) { const uint32_t p = b.get(x, y); if ((p >> 24) && p != rgba(250, 246, 232) && p != rgba(214, 206, 214) && p != rgba(70, 58, 86)) glyph++; }
    if (b.w != 11 || b.h != 10) fail("bubble size");
    if (glyph < 3) fail("bubble " + std::to_string(i) + ": no glyph");
  }
  if (art::bubbleSprite(art::Bubble::None).px != Canvas(11, 10).px) fail("bubble None not empty");
}
void checkDeterminism() {
  const std::vector<Look> lk = looks();
  for (int p = 1; p < (int)Posture::COUNT; p++)
    if (art::humanPostureSheet(lk[3].L, (Posture)p).px != art::humanPostureSheet(lk[3].L, (Posture)p).px) fail("posture not deterministic");
  if (art::critterSheet(art::Critter::Dog, 3).px != art::critterSheet(art::Critter::Dog, 3).px) fail("critter not deterministic");
  if (art::humanPostureSheet(lk[0].L, Posture::Lute).px != art::humanPostureSheet(lk[0].L, Posture::Lute, 0).px) fail("variant 0 is not the default");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && std::string(argv[1]) == "--check") {
    checkPostures();
    checkSeats();
    checkBeds();
    checkCritters();
    checkBubbles();
    checkDeterminism();
    std::printf("life_gallery --check: %d cells, %d failures\n", g_cells, g_fail);
    return g_fail ? 1 : 0;
  }
  const std::string dir = argc >= 2 ? argv[1] : ".";
  postureSheets(dir);
  bards(dir);
  seats(dir);
  beds(dir);
  critters(dir);
  bubbles(dir);
  festival(dir);
  return 0;
}
