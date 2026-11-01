// EMBERVALE art: the prop families (SOLID 5.1 ART-TABLES). Private to rpg/art: only art_props.cpp and art_holders2.cpp
// include it.
//
// - kPropFamilies: one ordered row per prop family painted outside art_props.cpp (its range test, its canvas size and
//   frame count, its painter, whether it outlines itself). propW / propH / propFrames walk it in this order, then fall
//   back to art_props.cpp's kPropInfo; paintProp's default branch walks it, then the economy and M0b painters. The
//   ranges are disjoint (each family is a run of the append-only Prop enum), so the walk order only matters for speed.
//   A new family: one row here (and its painter's file).
// - the small prop kit both painters share (tabletop things, table tops, shop goods, grass tufts): bodies here, in an
//   unnamed namespace, as art_internal.h does, so each painter keeps its own copy;
// - h2: the rig-2 holder painters (art_holders2.cpp) and the bed geometry art_props.cpp's berth2 reads.
#pragma once
#include "rpg/art/art_internal.h"
#include "rpg/art/art_city.h"
#include "rpg/art/art_relief.h"
#include "rpg/art/art_rise.h"
#include "rpg/art/art_sea.h"

namespace art {

void paintEconomyProp(Canvas& c, Prop p, int frame);   // M1 economy: rpg/art/art_market.cpp (Sacks .. WaterWheel)
// M2 Wayfinder: rpg/art/art_wild.cpp (Peak .. DragonBones): painters and canvas sizes
void paintWildProp(Canvas& c, Prop p, int frame);
void paintCultureFurniture(Canvas& c, Prop p, int frame);   // M3: rpg/art/art_culture_props.cpp
int wildPropW(Prop p);
int wildPropH(Prop p);
int wildPropFrames(Prop p);
// M3b interiors: rpg/art/art_culture_furniture.cpp (FirePitL .. FoldScreen): painters and canvas sizes
void paintInteriorM3b(Canvas& c, Prop p, int frame);
int m3bPropW(Prop p);
int m3bPropH(Prop p);
int m3bPropFrames(Prop p);
inline bool m3bP(Prop p) { return (int)p >= (int)Prop::FirePitL && (int)p <= (int)Prop::FoldScreen; }
// M3c Wildlands: rpg/art/art_flora.cpp (AcaciaTree .. Petals): painters and canvas sizes
void paintFloraProp(Canvas& c, Prop p, int frame);
int floraPropW(Prop p);
int floraPropH(Prop p);
int floraPropFrames(Prop p);
// M4 Banners: rpg/art/art_war.cpp (WarTent .. Barricade) and rpg/art/art_lore.cpp (NoticeBoard .. LostJournal)
void paintWarProp(Canvas& c, Prop p, int frame);
int warPropW(Prop p);
int warPropH(Prop p);
int warPropFrames(Prop p);
void paintLoreProp(Canvas& c, Prop p, int frame);
int lorePropW(Prop p);
int lorePropH(Prop p);
int lorePropFrames(Prop p);

// one prop family: a range test, its canvas size and frames, its painter; selfOutlined: its painter outlines the cell
// itself, so propSprite does not (M2 wild props and M3c flora)
struct PropFamily {
  bool (*has)(Prop);
  int (*w)(Prop);
  int (*h)(Prop);
  int (*frames)(Prop);
  void (*paint)(Canvas&, Prop, int);
  bool selfOutlined;
};
// in the order propW / propH / propFrames have always tested them
inline constexpr PropFamily kPropFamilies[] = {
  {isRiseProp, risePropW, risePropH, risePropFrames, paintRiseProp, false},              // M12 (art_rise.cpp)
  {isReliefProp, reliefPropW, reliefPropH, reliefPropFrames, paintReliefProp, false},    // M11 (art_relief.cpp)
  {isSeaProp, seaPropW, seaPropH, seaPropFrames, paintSeaProp, false},                   // M10 (art_sea.cpp)
  {isCityProp, cityPropW, cityPropH, cityPropFrames, paintCityProp, false},              // M7b (art_city.cpp)
  {isHomeProp, homePropW, homePropH, homePropFrames, paintHomeProp, false},              // M7 (art_home.cpp)
  {isWarProp, warPropW, warPropH, warPropFrames, paintWarProp, false},                   // M4 (art_war.cpp)
  {isLoreProp, lorePropW, lorePropH, lorePropFrames, paintLoreProp, false},              // M4 (art_lore.cpp)
  {isWildProp, wildPropW, wildPropH, wildPropFrames, paintWildProp, true},               // M2 (art_wild.cpp)
  {isWildlandsFlora, floraPropW, floraPropH, floraPropFrames, paintFloraProp, true},     // M3c (art_flora.cpp)
  {m3bP, m3bPropW, m3bPropH, m3bPropFrames, paintInteriorM3b, false},                    // M3b (art_culture_furniture.cpp)
};
// the family that paints p, or nullptr (art_props.cpp's own props)
inline const PropFamily* propFamily(Prop p) {
  for (const PropFamily& f : kPropFamilies) if (f.has(p)) return &f;
  return nullptr;
}

namespace {
// the palette entries the kit and the holders share with the rest of art_props.cpp
const Ramp kBlueCloth = ramp5(rgba(30, 36, 76), rgba(44, 62, 118), rgba(64, 96, 160), rgba(104, 140, 194), rgba(160, 192, 226));
const Ramp kClay = ramp5(rgba(84, 40, 40), rgba(132, 66, 48), rgba(178, 98, 62), rgba(210, 138, 86), rgba(234, 178, 120));
const Ramp kGlassG = ramp5(rgba(26, 54, 50), rgba(40, 90, 70), rgba(66, 134, 92), rgba(124, 186, 136), rgba(206, 238, 206));
const Ramp kPewter = ramp5(rgba(52, 52, 68), rgba(88, 90, 106), rgba(130, 132, 144), rgba(170, 172, 180), rgba(214, 216, 220));
const Ramp kSoot = ramp5(rgba(30, 28, 38), rgba(52, 50, 60), rgba(80, 76, 84), rgba(110, 104, 108), rgba(144, 136, 134));
const uint32_t kShadowCol = rgba(34, 20, 46);

// little clump of grass blades at the base of things (anchors props visually)
void grassTuft(Canvas& c, int x, int y, uint32_t seed, int n = 4) {
  for (int i = 0; i < n; i++) {
    int bx = x + i * 2 - n + (int)(hash3(i, 0, seed) % 2);
    int h = 2 + (int)(hash3(i, 1, seed) % 3);
    for (int j = 0; j < h; j++) c.set(bx + (j == h - 1 && (i & 1) ? 1 : 0), y - j, kLeaf[j == h - 1 ? 3 : 2]);
  }
}

// small tabletop things (a few pixels each); (x, y) is the item's base on the table surface
void plateAt(Canvas& c, int x, int y, uint32_t food) {
  hline(c, x - 2, x + 2, y, kBone[2]); hline(c, x - 2, x + 2, y - 1, kBone[4]); hline(c, x - 1, x + 1, y - 2, kBone[3]);
  c.set(x - 1, y - 1, food); c.set(x, y - 1, food); c.set(x, y - 2, mix(food, kWhite, 0.4f));
}
void mugAt(Canvas& c, int x, int y, bool foam) {
  vline(c, x, y - 3, y, kPewter[3]); vline(c, x + 1, y - 3, y, kPewter[2]); vline(c, x + 2, y - 3, y, kPewter[1]);
  c.set(x + 3, y - 2, kPewter[1]);
  if (foam) { hline(c, x, x + 2, y - 4, kWhite); c.set(x + 1, y - 3, kCloth[4]); }
  else hline(c, x, x + 2, y - 3, kPewter[0]);
}
void candleAt(Canvas& c, int x, int y, int h) {
  vline(c, x, y - h + 1, y, kCloth[4]); vline(c, x + 1, y - h + 1, y, kCloth[2]);
  c.set(x, y - h, kFire[3]); c.set(x, y - h - 1, kFire[4]); c.set(x + 1, y - h, kFire[2]);
  c.set(x, y - h - 2, withA(kGlow[4], 140));
}
void breadAt(Canvas& c, int x, int y) {
  hline(c, x - 2, x + 2, y, kThatch[1]); hline(c, x - 2, x + 2, y - 1, kThatch[2]); hline(c, x - 1, x + 1, y - 2, kThatch[3]);
  c.set(x - 1, y - 1, kThatch[4]); c.set(x + 1, y - 2, kThatch[2]);
}
void jugAt(Canvas& c, int x, int y) {
  ball(c, x + 0.5f, y - 2.0f, 2.2f, 2.4f, kClay);
  vline(c, x, y - 5, y - 4, kClay[2]); c.set(x + 1, y - 5, kClay[1]); c.set(x, y - 6, kClay[3]);
  c.set(x + 3, y - 3, kClay[1]);
}

// table tops for 1-tile tables and the joining segments (left/right ends optional)
void tableTop(Canvas& c, int x0, int x1, int top, int depth, bool leftEnd, bool rightEnd, const Ramp& R) {
  for (int y = top; y < top + depth; y++)
    for (int x = x0; x <= x1; x++) {
      int k = 3;
      if ((y - top) % 3 == 2) k = 2;                       // board seams run along the table
      if (y == top) k = 3;
      if (y == top + depth - 1) k = 4;                     // lit front lip
      if (leftEnd && x == x0) k = std::min(4, k + 1);
      if (rightEnd && x == x1) k = std::max(1, k - 1);
      c.set(x, y, R[k]);
    }
  for (int y = top + depth; y < top + depth + 2; y++)       // apron
    for (int x = x0; x <= x1; x++) {
      int k = y == top + depth ? 1 : 2;
      if (leftEnd && x == x0) k = 2;
      if (rightEnd && x == x1) k = 0;
      c.set(x, y, R[k]);
    }
}

// M0b fix round 2: a shop's counter carries the trade's things, not the inn's tankards: an open ledger and quill at
// the left end, the shop bell at the right, and along the middle brass scales, bolts of cloth, a tray of coins or a
// folded cloth (goods 0..3)
void shopCounterGoods(Canvas& c, int top, int part, int goods) {
  if (part == 0) {   // the ledger, open, and an inkpot with its quill
    for (int y = top; y <= top + 3; y++)
      for (int x = 3; x <= 11; x++) {
        int k = x == 7 ? 1 : (y == top ? 4 : (x < 7 ? 3 : 2));
        c.set(x, y, kBone[k]);
      }
    for (int x : {4, 5, 6, 8, 9, 10}) c.set(x, top + 1, kBone[1]);   // lines of writing
    for (int x : {4, 5, 8, 9, 10}) c.set(x, top + 2, kBone[1]);
    hline(c, 3, 11, top + 4, kLeather[1]);
    c.set(13, top + 2, kWoodDark[0]); c.set(14, top + 2, kWoodDark[0]); c.set(13, top + 1, kWoodDark[1]); c.set(14, top + 1, kPewter[3]);
    c.set(14, top - 1, kBone[4]); c.set(14, top, kBone[3]); c.set(15, top - 2, kBone[4]);
    return;
  }
  if (part == 2) {
    ball(c, 8.0f, top + 1.5f, 3.0f, 2.2f, kBrass);   // the shop bell
    c.set(7, top - 1, kBrass[4]); vline(c, 8, top - 2, top - 1, kBrass[2]);
    return;
  }
  switch (goods & 3) {
    case 0:   // brass scales: a post, the beam, two pans on chains
      vline(c, 8, top - 7, top + 2, kBrass[2]); c.set(7, top - 6, kBrass[4]); c.set(7, top - 5, kBrass[3]);
      hline(c, 7, 9, top + 3, kBrass[1]); hline(c, 6, 10, top + 3, kBrass[1]);
      hline(c, 3, 13, top - 7, kBrass[3]); c.set(3, top - 7, kBrass[4]); c.set(13, top - 7, kBrass[1]);
      for (int sx : {3, 13}) {
        vline(c, sx, top - 6, top - 2, kBrass[1]);
        hline(c, sx - 2, sx + 2, top - 1, kBrass[3]); hline(c, sx - 1, sx + 1, top, kBrass[1]);
        c.set(sx - 2, top - 1, kBrass[4]);
      }
      c.set(12, top - 2, kGold[4]); c.set(13, top - 2, kGold[3]); c.set(14, top - 2, kGold[2]);   // a weight on one pan
      break;
    case 1:   // bolts of cloth lying on the counter, ends towards us
      for (int b = 0; b < 2; b++) {
        const Ramp& R = b == 0 ? kRed : kBlueCloth;
        int x0 = 2 + b * 6;
        for (int y = top - 2; y <= top + 2; y++)
          for (int x = x0; x <= x0 + 4; x++) {
            int k = y == top - 2 ? 4 : (y == top + 2 ? 1 : (x == x0 ? 3 : (x == x0 + 4 ? 1 : 2)));
            c.set(x, y, R[k]);
          }
        c.set(x0 + 2, top, R[0]); c.set(x0 + 1, top - 1, R[4]);
      }
      break;
    case 2:   // a tray of coins and a small strongbox
      for (int x = 2; x <= 8; x++) { c.set(x, top + 2, kWoodDark[1]); c.set(x, top + 1, kWoodDark[3]); }
      for (int i = 0; i < 4; i++) { c.set(3 + i * 1 + (i & 1), top + (i & 1), kGold[3 + (i & 1)]); c.set(3 + i + (i & 1), top + 1 - (i & 1), kGold[2]); }
      for (int y = top - 3; y <= top + 2; y++)
        for (int x = 10; x <= 14; x++) c.set(x, y, y == top - 3 ? kWood[4] : (x == 14 ? kWood[1] : (y == top - 1 ? kIron[2] : kWood[2])));
      c.set(12, top, kGold[4]);
      break;
    default:   // a folded cloth and a jar of buttons
      for (int y = top; y <= top + 2; y++)
        for (int x = 3; x <= 10; x++) c.set(x, y, kCloth[y == top ? 4 : (y == top + 2 ? 1 : 3)]);
      jugAt(c, 12, top + 2);
      break;
  }
}
}  // namespace

// (P2a HOLDERS) the rig-2 holder painters: art_holders2.cpp
namespace h2 {
constexpr int kBedHb0 = 3, kBedHb1 = 13;           // the classic bed's headboard rows (40 tall)
constexpr int kLongHb0 = 9, kLongHb1 = 19;         // the long bed's (48 tall: it starts a tile lower)
constexpr int kBunkH = 60, kBunkUp0 = 3, kBunkUp1 = 30;   // the upper mattress rows (pillow from kBunkUp0)
void chair(Canvas& c);
void stool(Canvas& c);
void bench(Canvas& c);
void throne(Canvas& c);
void tableSmall(Canvas& c, int kind);
void tableSeg(Canvas& c, int part);
void table(Canvas& c);
void counterSeg(Canvas& c, int part, int goods);
void counter(Canvas& c);
void bed(Canvas& c);
Canvas longBed(int variant);
void bunkBed(Canvas& c);
void fence(Canvas& c, bool vertical);
void ladder(Canvas& c);
void gravestone(Canvas& c);
}  // namespace h2

}  // namespace art
