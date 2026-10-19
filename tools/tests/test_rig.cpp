// rpg_test --rig and --rig-coverage (P2a Clear Sight, VISION_PLAN 15.36 item 11: the lead's frozen harness; a lane
// reports a harness problem instead of editing it). RESTATED at P2b phase A for A3 (VISION_PLAN 15.36 "What P2a keeps,
// adapts and retires"): humanRig, humanPosture2, sleeper2 and rig2Covers are the DOLL's now (rpg/art/doll/doll.h), so
// every gate below measures the doll's atlases; the procedural painters are detached and no longer measured.
//   rpg_test --rig [--gates] [--report]
//   rpg_test --rig-coverage [--seeds A..B] [--gates]
// Always (CI): the contract: metrics (STYLE_P2 3, 8: A3's rows), families, bodyPx identity at rig 1, the atlas index,
// the weapon ids (the greatsword paints as the sword on v1, the herald's horn as before), the ink outer outline on
// every v1 character sheet (Ink / Lit Ink, never #000000, the sheet == outlineInk(raw cell)), the legacy outline hook,
// the holders at rig 1, the key: body and the child move it at rig 1, youth and elder only at rig 2.
// --gates (the doll's: FAIL until the areas land): R2 heights (STYLE_P2 3, the hair's 2 px rise allowed), R4 A3's face
// rows and 3 x 2 hands (from the marks), R5 holes, scratch edge and pivots, R6 atlas sizes and bake time, R7 / R9 strike
// frames distinct by family and by variant, R8 tip paths, R10 painted left frames and the weapon hand, P1 postures, P2
// seats and pillows, P3 youth, child and the humanoid monsters, H2 the holders' ratios at rig 2. --report: every
// measurement, not only the failures. (P2c restates R7-R10 for its families; B1-B12 are rpg_test --doll's.)
// --rig-coverage: R3 as the doll's census of a seed's plain looks (no armour, helm, cloak, shield or culture weapon form)
// that rig2Covers (99 % or more with --gates); rpg_test --doll-coverage is B6's full census.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "rpg/art/art_internal.h"
#include "rpg/art/doll/doll.h"
#include "rpg/sim/game.h"
#include "tools/tests/tests.h"

namespace {

using namespace art;

bool g_report = false;
struct Gate {
  const char* id;
  int checks = 0, fails = 0;
  std::string first;
  void ok() { checks++; }
  void bad(const std::string& why) {
    checks++;
    if (!fails) first = why;
    if (g_report || fails < 3) std::printf("  %s: %s\n", id, why.c_str());
    fails++;
  }
};
int g_contractFails = 0;
void cfail(const std::string& s) { std::printf("FAIL: rig contract: %s\n", s.c_str()); g_contractFails++; }
std::string fmt(const char* f, double a = 0, double b = 0, double c = 0, double d = 0) {
  char buf[256];
  std::snprintf(buf, sizeof buf, f, a, b, c, d);
  return buf;
}

float lumaOf(uint32_t c) { return (chR(c) * 0.299f + chG(c) * 0.587f + chB(c) * 0.114f); }

// ---------------------------------------------------------------- the contract (always)
void checkMetrics() {
  const RigMetrics& a = rig(1);
  const RigMetrics& b = rig(2);
  if (a.figH(0, 0, 0) != 22 || a.figH(0, 0, 2) != 23) cfail("rig(1) adult figure 22 (elf 23)");
  if (b.figH(0, 0, 0) != 32 || b.figH(1, 0, 0) != 31 || b.figH(0, 0, 2) != 33 || b.figH(1, 0, 2) != 32) cfail("rig(2) man 32, woman 31, elf +1");
  if (b.figH(0, 2, 0) != 26 || b.figH(1, 1, 0) != 21) cfail("rig(2) youth 26, child 21");
  if (a.ringW != 18 || a.ringH != 7 || b.ringW != 24 || b.ringH != 9) cfail("the ring 18 x 7 (v1), 24 x 9 (v2)");
  if (a.reach[(int)Family::Sword] != 11 || a.fxLift != 8 || a.bathVis != 14 || a.bathUnder != 5) cfail("rig(1) keeps today's swing reach 11 / lift 8 and bathing crop 14 / 5");
  if (b.markerY < b.figH(0, 0, 2) + 3) cfail("rig(2) markers float over the tallest head");
}
void checkFamilies() {
  static const Family want[13] = {Family::Unarmed, Family::Sword, Family::Axe, Family::Bow, Family::Staff, Family::Dagger, Family::Mace,
                                  Family::Greatsword, Family::Spear, Family::Staff, Family::Unarmed, Family::Unarmed, Family::Unarmed};
  for (int w = 0; w < 13; w++) if (familyOfWeapon((uint8_t)w) != want[w]) cfail(fmt("familyOfWeapon(%.0f)", w));
  if (WEAPON_GREATSWORD != 7 || WEAPON_HORN != 12) cfail("weapon ids: greatsword 7, horn 12");
  for (int c = 0; c < CLIP_COUNT; c++) if (std::string(clipName((Clip)c)) == "?") cfail("a clip without a name");
}
void checkScale() {
  const int was = rigVersion();
  setRigVersion(1);
  for (int px = 0; px <= 400; px++) if (bodyPx(px) != px) { cfail(fmt("bodyPx(%.0f) at rig 1", px)); break; }
  if (bodyScale() != 1.0f) cfail("bodyScale() at rig 1");
  setRigVersion(2);
  if (bodyPx(16) != 23 || bodyPx(24) != 35 || std::fabs(bodyScale() - 32.0f / 22.0f) > 1e-6f) cfail("bodyPx at rig 2 (16 -> 23, 24 -> 35)");
  setRigVersion(7);
  if (rigVersion() != 2) cfail("setRigVersion clamps to 2");
  setRigVersion(-3);
  if (rigVersion() != 1) cfail("setRigVersion clamps to 1");
  setRigVersion(was);
}
void checkSheetIndex() {
  RigSheet s;
  s.atlas = Canvas(8, 8);
  FrameRect r; r.w = 2; r.h = 3; r.px = 1; r.py = 2;
  s.add(0, Clip::Walk, 0, r);
  r.x = 2; s.add(0, Clip::Walk, 0, r);
  RigAnchors an; an.tipX = 5; an.tipY = -9;
  r.x = 4; s.add(2, Clip::Strike, 1, r, an);
  if (s.count(0, Clip::Walk) != 2 || s.count(0, Clip::Walk, 2) != 2 || s.count(1, Clip::Walk) != 0) cfail("RigSheet::count");
  if (!s.frame(0, Clip::Walk, 0, 3) || s.frame(0, Clip::Walk, 0, 3)->x != 2) cfail("RigSheet::frame wraps");
  if (s.frame(3, Clip::Idle, 0, 0)) cfail("RigSheet::frame of a missing clip");
  const RigAnchors* pa = s.anchorsOf(2, Clip::Strike, 1, 0);
  if (!pa || pa->tipX != 5 || s.anchors.size() != s.frames.size()) cfail("RigSheet anchors stay aligned with the frames");
  if (s.anchorsOf(0, Clip::Walk, 0, 0)->tipX != kNoAnchor) cfail("a frame without pose data has kNoAnchor");
}
// v1: the greatsword (7) paints as the sword, the herald's horn (12) as the horn it was
void checkWeaponIds() {
  HumanLook a; a.weapon = 1;
  HumanLook b = a; b.weapon = WEAPON_GREATSWORD;
  const Canvas sa = humanSheet(a), sb = humanSheet(b);
  if (sa.px != sb.px) cfail("v1 paints the greatsword (7) exactly as the sword (1)");
  if (a.key() == b.key()) cfail("the greatsword look has its own key");
  HumanLook h; h.outfit = Outfit::Guard; h.weapon = WEAPON_HORN;
  HumanLook n = h; n.weapon = 0;
  if (humanSheet(h).px == humanSheet(n).px) cfail("the herald's horn (12) is painted");
  HumanLook bd; bd.body = 1;
  HumanLook ag; ag.age = 1;
  if (bd.key() == HumanLook().key() || ag.key() == HumanLook().key()) cfail("body and the child move the key");
  HumanLook yo; yo.age = 2;
  if (rigVersion() < 2 && yo.key() != HumanLook().key()) cfail("(P2b) the youth keys as the adult at rig 1");
  if (humanSheet(bd).px != humanSheet(HumanLook()).px) cfail("v1 ignores the body field (no v1 pixel moves)");
}
// the ink outer outline: every silhouette edge Ink (bottom / right) or Lit Ink (top / left), never #000000
int inkEdgeFails(const Canvas& c, int cellW, int cellH, const char* what) {
  int bad = 0;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const uint32_t p = c.get(x, y);
      if (chA(p) < 255) continue;
      const int cx = x % cellW, cy = y % cellH;
      auto clear = [&](int dx, int dy) {
        const int qx = cx + dx, qy = cy + dy;
        if (qx < 0 || qy < 0 || qx >= cellW || qy >= cellH) return false;   // (v1 content touching its cell's edge has no room for an outline there)
        return chA(c.get(x + dx, y + dy)) == 0;
      };
      const bool r = clear(1, 0), d = clear(0, 1), l = clear(-1, 0), u = clear(0, -1);
      if (!(r || d || l || u)) continue;
      const float L = lumaOf(p);
      if ((p & 0x00FFFFFFu) == 0) { if (bad++ < 2) cfail(std::string(what) + fmt(": #000000 on the edge at %.0f,%.0f", x, y)); continue; }
      auto around = [&]() {   // the 3 x 3 round it: alpha / luma (diagnosis)
        std::string s = " [";
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++) {
            const uint32_t q = c.get(x + dx, y + dy);
            s += fmt("%.0f/%.0f ", chA(q), lumaOf(q));
          }
        return s + "]";
      };
      // an edge pixel is Ink (luma 27) or Lit Ink (Ink 30 % toward the shape it outlines: luma 100 or less); at a corner
      // the rule's priority (the shape below, then left, right, above) decides which, so only the bound is checked here
      // (the human sheets are proved == outlineInk(raw) cell by cell below)
      (void)r; (void)d; (void)l; (void)u;
      if (L > 100.5f) { if (bad++ < 2) cfail(std::string(what) + fmt(": a Lit Ink edge pixel of luma %.0f at %.0f,%.0f (100 or less)", L, x, y) + around()); }
    }
  return bad;
}
void checkInkOutline() {
  std::vector<HumanLook> looks;
  for (int o = 0; o < 10; o++)
    for (int v = 0; v < 2; v++) {
      HumanLook L;
      L.outfit = (Outfit)o; L.hair = (Hair)((o + v) % 8); L.weapon = (uint8_t)((o * 3 + v) % 9); L.shield = v; L.cape = o & 1;
      L.skin = v ? rgba(250, 220, 190) : rgba(92, 60, 46);   // the palest and darkest skins
      looks.push_back(L);
    }
  for (const HumanLook& L : looks) {
    const Canvas s = humanSheet(L);
    inkEdgeFails(s, HUMAN_W, HUMAN_H, (fmt("v1 human outfit %.0f weapon %.0f", (int)L.outfit, L.weapon)).c_str());
    // the sheet is exactly the raw cell with the ink outline (nothing else moved)
    for (int row = 0; row < 3; row++)
      for (int f = 0; f < HUMAN_FRAMES; f++) {
        Canvas cell = humanCellRaw(L, row, f);
        outlineInk(cell);
        for (int y = 0; y < HUMAN_H; y++)
          for (int x = 0; x < HUMAN_W; x++)
            if (cell.get(x, y) != s.get(f * HUMAN_W + x, row * HUMAN_H + y)) { cfail(fmt("humanSheet cell %.0f,%.0f != outlineInk(humanCellRaw)", row, f)); y = HUMAN_H; x = HUMAN_W; row = 3; f = HUMAN_FRAMES; }
      }
  }
  for (int m = 0; m < (int)Monster::COUNT; m++) {
    if ((Monster)m == Monster::Wisp) continue;
    const Canvas s = monsterSheet((Monster)m);
    inkEdgeFails(s, monsterCellW((Monster)m), monsterCellH((Monster)m), fmt("monster %.0f", m).c_str());
  }
  for (int k = 0; k < (int)Critter::COUNT; k++) inkEdgeFails(critterSheet((Critter)k, 0), critterCellW((Critter)k), critterCellH((Critter)k), "critter");
  inkEdgeFails(horseSheet(rgba(120, 80, 50), 0, true), HORSE_W, HORSE_H, "horse");
  inkEdgeFails(houndSheet(HoundLook()), HOUND_W, HOUND_H, "hound");
  for (int p = 1; p < (int)Posture::COUNT; p++)
    if ((Posture)p != Posture::Sleep) inkEdgeFails(humanPostureSheet(HumanLook(), (Posture)p), HUMAN_W, HUMAN_H, fmt("posture %.0f", p).c_str());
  // the legacy hook gives back the pre-P2a selective outline (art_hash --legacy-outline, EMB_LEGACY_OUTLINE)
  outlineLegacyForTests() = true;
  const Canvas old = humanSheet(looks[0]);
  outlineLegacyForTests() = false;
  Canvas cell = humanCellRaw(looks[0], 0, 0);
  outline(cell);
  for (int y = 0; y < HUMAN_H; y++)
    for (int x = 0; x < HUMAN_W; x++)
      if (cell.get(x, y) != old.get(x, y)) { cfail("the legacy outline hook paints the pre-P2a outline"); y = HUMAN_H; break; }
}
void checkHoldersRig1() {
  const int was = rigVersion();
  setRigVersion(1);
  if (holderPx(Holder::HouseDoorH) != 16 || holderPx(Holder::HouseDoorW) != 10 || holderPx(Holder::WallOneStorey) != 24) cfail("holders at rig 1: today's door 16 x 10 and wall 24");
  setRigVersion(was);
}

// ---------------------------------------------------------------- the gates (rig 2)
struct Fr { Canvas c; int px = 0, py = 0; const RigAnchors* an = nullptr; bool ok = false; };
Fr cut(const RigSheet& s, int facing, Clip clip, int variant, int i) {
  Fr f;
  const FrameRect* r = s.frame(facing, clip, variant, i);
  if (!r || r->w <= 0 || r->h <= 0 || s.atlas.w <= 0) return f;
  f.c = Canvas(r->w, r->h);
  for (int y = 0; y < r->h; y++) for (int x = 0; x < r->w; x++) f.c.set(x, y, s.atlas.get(r->x + x, r->y + y));
  f.px = r->px; f.py = r->py;
  f.an = s.anchorsOf(facing, clip, variant, i);
  f.ok = true;
  return f;
}
// opaque rows / columns of a frame
bool box(const Canvas& c, int& x0, int& y0, int& x1, int& y1) {
  x0 = y0 = 1 << 20; x1 = y1 = -1;
  for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) if (chA(c.get(x, y))) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
  return x1 >= 0;
}
uint32_t at(const Fr& f, int ax, int ay) { return f.c.get(f.px + ax, f.py + ay); }   // pivot-relative
// the share of the union of two frames' opaque pixels where they differ (aligned at their pivots)
float differ(const Fr& a, const Fr& b) {
  int un = 0, dif = 0;
  const int x0 = -std::max(a.px, b.px), x1 = std::max(a.c.w - a.px, b.c.w - b.px);
  const int y0 = -std::max(a.py, b.py), y1 = std::max(a.c.h - a.py, b.c.h - b.py);
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) {
      const bool oa = chA(at(a, x, y)) != 0, ob = chA(at(b, x, y)) != 0;
      if (oa || ob) un++;
      if (oa != ob || (oa && at(a, x, y) != at(b, x, y))) dif++;
    }
  return un ? (float)dif / un : 0.0f;
}
Fr mirror(const Fr& f) {
  Fr m = f;
  for (int y = 0; y < f.c.h; y++) for (int x = 0; x < f.c.w; x++) m.c.set(f.c.w - 1 - x, y, f.c.get(x, y));
  m.px = f.c.w - 1 - f.px;
  return m;
}
HumanLook bodyLook(int body, int build, int people, int age) {
  HumanLook L;
  L.body = (uint8_t)body; L.build = (uint8_t)build; L.people = (uint8_t)people; L.age = (uint8_t)age;
  L.skin = rgba(236, 188, 146);           // S3
  L.hairColor = rgba(120, 70, 36);        // Chestnut
  return L;
}
HumanLook armed(Family f) {
  HumanLook L = bodyLook(0, 0, 0, 0);
  static const uint8_t w[FAMILY_COUNT] = {0, 5, 1, WEAPON_GREATSWORD, 2, 6, 8, 4, 3};
  L.weapon = w[(int)f];
  return L;
}

void gateHeights(Gate& g) {   // R2 (and P3's youth and child)
  for (int body = 0; body < 2; body++)
    for (int build = 0; build < 3; build++)
      for (int people = 0; people < 3; people++)
        for (int age = 0; age < 3; age++) {
          const HumanLook L = bodyLook(body, build, people, age);
          const RigSheet s = humanRig(L, RigPart::Loco);
          for (int facing = 0; facing < RIG_FACINGS; facing++) {
            const Fr f = cut(s, facing, Clip::Idle, 0, 0);
            const std::string who = fmt("body %.0f build %.0f people %.0f age %.0f", body, build, people, age) + fmt(" facing %.0f", facing);
            if (!f.ok) { g.bad(who + ": no LOCO idle frame"); continue; }
            int x0, y0, x1, y1;
            box(f.c, x0, y0, x1, y1);
            const int h = y1 - y0 + 1, want = rig(2).figH(L);
            if (h < want || h > want + 2) g.bad(who + fmt(": %.0f px tall (%.0f; the hair may rise 2 more)", h, want)); else g.ok();
            if (y1 != f.py) g.bad(who + fmt(": the sole row %.0f is not the pivot row %.0f", y1, f.py));
          }
        }
}
void gateFaces(Gate& g, Gate& hands) {   // R4, restated for A3 (STYLE_P2 4 and 5): the face's rows from its mark
  const HumanLook L = bodyLook(0, 0, 0, 0);
  const RigSheet s = humanRig(L, RigPart::Loco);
  uint32_t sk[5];
  doll::rampA3(L.skin, doll::Material::Skin, sk);
  const float skinL = lumaOf(sk[2]);
  auto face = [&](const Fr& f, const std::string& who) {
    if (!f.ok || !f.an || f.an->faceX == kNoAnchor) { g.bad(who + ": no face mark"); return; }
    const int fx = f.an->faceX, fy = f.an->faceY;   // the brows' row, the 10 px face's column 0
    int bad = 0;
    for (int col : {1, 2, 7, 8}) if (skinL - lumaOf(at(f, fx + col, fy + 1)) < 60) bad |= 1;      // the lashes
    if (lumaOf(at(f, fx + 1, fy + 2)) < skinL && lumaOf(at(f, fx + 8, fy + 2)) < skinL) bad |= 2;  // eye whites (A3)
    if (!(lumaOf(at(f, fx + 5, fy + 3)) < skinL - 8 || lumaOf(at(f, fx + 4, fy + 3)) < skinL - 8)) bad |= 8;
    if (!(lumaOf(at(f, fx + 5, fy + 4)) < skinL - 20 || lumaOf(at(f, fx + 4, fy + 4)) < skinL - 20)) bad |= 16;
    if (bad) g.bad(who + ": face missing" + std::string(bad & 1 ? " lashes" : "") + (bad & 2 ? " whites" : "") + (bad & 8 ? " nose" : "") +
                   (bad & 16 ? " mouth" : ""));
    else g.ok();
  };
  for (int i = 0; i < std::max(1, s.count(0, Clip::Idle)) && i < 2; i++) face(cut(s, 0, Clip::Idle, 0, i), fmt("down idle %.0f", i));   // (frame 2 is the blink)
  for (int i = 0; i < std::max(1, s.count(0, Clip::Walk)); i++) face(cut(s, 0, Clip::Walk, 0, i), fmt("down walk %.0f", i));
  for (int facing = 0; facing < 2; facing++)
    for (Clip c : {Clip::Idle, Clip::Walk})
      for (int i = 0; i < s.count(facing, c); i++) {
        const Fr f = cut(s, facing, c, 0, i);
        const std::string who = fmt("facing %.0f ", facing) + clipName(c) + fmt(" %.0f", i);
        if (!f.an || f.an->handRX == kNoAnchor || f.an->handLX == kNoAnchor) { hands.bad(who + ": no hand marks"); continue; }
        int skinPx = 0;
        for (int hnd = 0; hnd < 2; hnd++)
          for (int dy = 0; dy < 2; dy++)
            for (int dx = 0; dx < 3; dx++) {   // A3's 3 x 2 hand
              const uint32_t p = hnd ? at(f, f.an->handLX + dx, f.an->handLY + dy) : at(f, f.an->handRX + dx, f.an->handRY + dy);
              skinPx += chA(p) && std::fabs(lumaOf(p) - skinL) < 90;
            }
        if (skinPx < 8) hands.bad(who + fmt(": %.0f of 12 hand pixels in skin", skinPx)); else hands.ok();
      }
  if (!s.count(0, Clip::Idle)) hands.bad("no LOCO sheet");
}
int holes(const Canvas& c) {
  std::vector<uint8_t> seen((size_t)c.w * c.h, 0);
  std::vector<int> q;
  for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++)
    if ((x == 0 || y == 0 || x == c.w - 1 || y == c.h - 1) && !chA(c.get(x, y))) { seen[(size_t)y * c.w + x] = 1; q.push_back(y * c.w + x); }
  while (!q.empty()) {
    const int i = q.back(); q.pop_back();
    const int x = i % c.w, y = i / c.w;
    const int nx[4] = {x - 1, x + 1, x, x}, ny[4] = {y, y, y - 1, y + 1};
    for (int k = 0; k < 4; k++) {
      if (nx[k] < 0 || ny[k] < 0 || nx[k] >= c.w || ny[k] >= c.h) continue;
      const size_t j = (size_t)ny[k] * c.w + nx[k];
      if (!seen[j] && !chA(c.get(nx[k], ny[k]))) { seen[j] = 1; q.push_back((int)j); }
    }
  }
  int n = 0;
  for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) n += !chA(c.get(x, y)) && !seen[(size_t)y * c.w + x];
  return n;
}
// R5's one exemption (P2a fix 1), for the bow's Draw frames only: the see-through gap between the drawn string and the
// stave is the art (the ground shows through it; rig_actions.cpp inkFinish keepOpen), closed where the bow arm crosses
// it. An enclosed region of at most 40 px is exempt when every one of its pixels lies between the string and the stave:
// looking along the 8 compass lines, the bowstring (rig_arms.cpp paintBow's string colour) within 12 px one way and the
// stave (kWood 2..4) within 12 px another way at least 90 degrees apart (the narrow wedge at a tip sees them at a right
// angle). Returns the enclosed px left after the exemption.
int holesBowGapExempt(const Canvas& c) {
  if (!holes(c)) return 0;
  const int W = c.w, H = c.h;
  auto rgb = [](uint32_t p) { return p & 0x00FFFFFFu; };
  const uint32_t str = rgb(rgba(232, 224, 200));
  auto isStr = [&](int x, int y) { return chA(c.get(x, y)) && rgb(c.get(x, y)) == str; };
  auto isWood = [&](int x, int y) {
    if (!chA(c.get(x, y))) return false;
    const uint32_t p = rgb(c.get(x, y));
    return p == rgb(kWood[2]) || p == rgb(kWood[3]) || p == rgb(kWood[4]);
  };
  // 1 string, 2 stave, 3 both: what lies within 12 px from (x, y) along (dx, dy)
  auto ray = [&](int x, int y, int dx, int dy) {
    int seen = 0;
    for (int k = 1; k <= 12; k++) {
      const int qx = x + dx * k, qy = y + dy * k;
      if (qx < 0 || qy < 0 || qx >= W || qy >= H) break;
      seen |= (isStr(qx, qy) ? 1 : 0) | (isWood(qx, qy) ? 2 : 0);
    }
    return seen;
  };
  auto between = [&](int x, int y) {
    static const int dirs[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    int seen8[8];
    for (int d = 0; d < 8; d++) seen8[d] = ray(x, y, dirs[d][0], dirs[d][1]);
    for (int a = 0; a < 8; a++)
      for (int b = 0; b < 8; b++)
        if ((seen8[a] & 1) && (seen8[b] & 2) && dirs[a][0] * dirs[b][0] + dirs[a][1] * dirs[b][1] <= 0) return true;
    return false;
  };
  std::vector<uint8_t> seen((size_t)W * H, 0);
  std::vector<int> q;
  auto flood = [&](int start, std::vector<int>* region) {
    q.assign(1, start);
    seen[(size_t)start] = 1;
    while (!q.empty()) {
      const int i = q.back(); q.pop_back();
      if (region) region->push_back(i);
      const int x = i % W, y = i / W;
      const int nx[4] = {x - 1, x + 1, x, x}, ny[4] = {y, y, y - 1, y + 1};
      for (int k = 0; k < 4; k++) {
        if (nx[k] < 0 || ny[k] < 0 || nx[k] >= W || ny[k] >= H) continue;
        const size_t j = (size_t)ny[k] * W + nx[k];
        if (!seen[j] && !chA(c.get(nx[k], ny[k]))) { seen[j] = 1; q.push_back((int)j); }
      }
    }
  };
  // the outside air first, as holes() floods it; what is left is enclosed, region by region
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
    if ((x == 0 || y == 0 || x == W - 1 || y == H - 1) && !chA(c.get(x, y)) && !seen[(size_t)y * W + x]) flood(y * W + x, nullptr);
  int left = 0;
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
    if (chA(c.get(x, y)) || seen[(size_t)y * W + x]) continue;
    std::vector<int> region;
    flood(y * W + x, &region);
    bool gap = region.size() <= 40;
    for (size_t r = 0; r < region.size() && gap; r++) gap = between(region[r] % W, region[r] / W);
    if (!gap) left += (int)region.size();
  }
  return left;
}
void gateFrames(Gate& g) {   // R5
  for (int fam = 0; fam < FAMILY_COUNT; fam++) {
    const HumanLook L = armed((Family)fam);
    for (RigPart part : {RigPart::Loco, RigPart::Action, RigPart::Death}) {
      if (part != RigPart::Action && fam) continue;
      const RigSheet s = humanRig(L, part);
      if (s.empty()) { g.bad(std::string(familyName((Family)fam)) + fmt(": part %.0f has no frames", (int)part)); continue; }
      const int maxW = part == RigPart::Action ? 78 : 46, maxH = part == RigPart::Action ? 70 : 46;
      for (int facing = 0; facing < RIG_FACINGS; facing++)
        for (int c = 0; c < CLIP_COUNT; c++)
          for (int v = 0; v < RIG_VARIANTS; v++) {
            if (v && s.count(facing, (Clip)c, v) == s.count(facing, (Clip)c, 0) && s.frame(facing, (Clip)c, v, 0) == s.frame(facing, (Clip)c, 0, 0)) continue;
            for (int i = 0; i < s.count(facing, (Clip)c, v); i++) {
              const Fr f = cut(s, facing, (Clip)c, v, i);
              const std::string who = std::string(familyName((Family)fam)) + " " + clipName((Clip)c) + fmt(" f%.0f v%.0f #%.0f", facing, v, i);
              const int h = (Family)fam == Family::Bow && (Clip)c == Clip::Draw ? holesBowGapExempt(f.c) : holes(f.c);
              if (h) g.bad(who + fmt(": %.0f transparent px enclosed", h));
              else if (f.c.w > maxW || f.c.h > maxH) g.bad(who + fmt(": %.0f x %.0f touches the scratch canvas's edge", f.c.w, f.c.h));
              else g.ok();
              if (part == RigPart::Loco && ((Clip)c == Clip::Idle || (Clip)c == Clip::Walk)) {
                int x0, y0, x1, y1;
                box(f.c, x0, y0, x1, y1);
                if (y1 != f.py) g.bad(who + fmt(": the feet on row %.0f, the pivot row %.0f", y1, f.py));
              }
            }
          }
    }
  }
}
void gateSizes(Gate& g) {   // R6
  std::vector<HumanLook> looks;
  for (int i = 0; i < 20; i++) {
    HumanLook L = bodyLook(i & 1, i % 3, (i / 3) % 3, 0);
    L.hair = (Hair)(i % 8); L.topColor = rgba(40 + i * 9, 90, 160 - i * 4); L.weapon = (uint8_t)(i % 9);
    looks.push_back(L);
  }
  std::vector<double> v1ms, locoMs;
  for (const HumanLook& L : looks) {
    auto t0 = std::chrono::steady_clock::now();
    const Canvas s1 = humanSheet(L);
    v1ms.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    t0 = std::chrono::steady_clock::now();
    const RigSheet lo = humanRig(L, RigPart::Loco);
    locoMs.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / RIG_FACINGS);
    const RigSheet ac = humanRig(L, RigPart::Action), de = humanRig(L, RigPart::Death);
    if (lo.empty() || de.empty() || (L.weapon && ac.empty())) { g.bad("a look without its LOCO / ACTION / DEATH atlas"); continue; }
    if (lo.bytes() > 128 * 1024) g.bad(fmt("LOCO %.0f KB (128)", lo.bytes() / 1024.0)); else g.ok();
    if (de.bytes() > 48 * 1024) g.bad(fmt("DEATH %.0f KB (48)", de.bytes() / 1024.0)); else g.ok();
    if (ac.bytes() > 512 * 1024) g.bad(fmt("ACTION %.0f KB (512)", ac.bytes() / 1024.0)); else g.ok();
    (void)s1;
  }
  for (int p = 1; p < (int)Posture::COUNT; p++) {
    if ((Posture)p == Posture::Sleep) continue;
    const RigSheet ps = humanPosture2(looks[0], (Posture)p, 0);
    if (ps.empty()) { g.bad(fmt("posture %.0f has no atlas", p)); continue; }
    if (ps.bytes() > 64 * 1024) g.bad(fmt("posture %.0f atlas %.0f KB (64)", p, ps.bytes() / 1024.0)); else g.ok();
  }
  std::sort(v1ms.begin(), v1ms.end());
  std::sort(locoMs.begin(), locoMs.end());
  const double p95 = v1ms[(size_t)(v1ms.size() * 95 / 100)], loco = locoMs[locoMs.size() / 2];
  std::printf("  R6 bake: v1 sheet p50 %.2f p95 %.2f ms; one facing's LOCO p50 %.2f ms\n", v1ms[v1ms.size() / 2], p95, loco);
  if (loco > p95) g.bad(fmt("one facing's LOCO bakes in %.2f ms (the v1 sheet's p95 %.2f)", loco, p95)); else g.ok();
}
void gateFamilies(Gate& r7, Gate& r9) {   // R7, R9
  std::vector<RigSheet> act;
  for (int f = 0; f < 8; f++) act.push_back(humanRig(armed((Family)f), RigPart::Action));
  for (int facing = 0; facing < RIG_FACINGS; facing++) {
    for (int a = 0; a < 8; a++)
      for (int b = a + 1; b < 8; b++)
        for (int i = 0; i < 2; i++) {
          const Fr fa = cut(act[(size_t)a], facing, Clip::Strike, 0, i), fb = cut(act[(size_t)b], facing, Clip::Strike, 0, i);
          const std::string who = std::string(familyName((Family)a)) + " / " + familyName((Family)b) + fmt(" facing %.0f strike %.0f", facing, i);
          if (!fa.ok || !fb.ok) { r7.bad(who + ": no strike frame"); continue; }
          const float d = differ(fa, fb);
          if (d < 0.25f) r7.bad(who + fmt(": differ in %.0f %% of their union (25)", d * 100)); else r7.ok();
        }
    for (int f = 0; f < 8; f++)
      for (int va = 0; va < 3; va++)
        for (int vb = va + 1; vb < 3; vb++) {
          const Fr fa = cut(act[(size_t)f], facing, Clip::Strike, va, 0), fb = cut(act[(size_t)f], facing, Clip::Strike, vb, 0);
          const std::string who = std::string(familyName((Family)f)) + fmt(" facing %.0f variants %.0f / %.0f", facing, va, vb);
          if (!fa.ok || !fb.ok || act[(size_t)f].frame(facing, Clip::Strike, va, 0) == act[(size_t)f].frame(facing, Clip::Strike, vb, 0)) { r9.bad(who + ": not painted apart"); continue; }
          const float d = differ(fa, fb);
          if (d < 0.25f) r9.bad(who + fmt(": differ in %.0f %% (25)", d * 100)); else r9.ok();
        }
  }
}
// R8: the tip's path over Windup -> Strike -> Follow (pose data), side-on (facing right)
void gateTips(Gate& g) {
  auto path = [&](const RigSheet& s, int variant, std::vector<Vec2>& pts, std::vector<Vec2>& grips) {
    pts.clear(); grips.clear();
    for (Clip c : {Clip::Windup, Clip::Strike, Clip::Follow})
      for (int i = 0; i < s.count(2, c, variant); i++) {
        const RigAnchors* an = s.anchorsOf(2, c, variant, i);
        if (an && an->tipX != kNoAnchor) { pts.push_back(Vec2(an->tipX, an->tipY)); grips.push_back(Vec2(an->gripX, an->gripY)); }
      }
  };
  auto sweep = [&](const std::vector<Vec2>& p, const std::vector<Vec2>& gr) {   // degrees the tip turns about the grip
    float tot = 0;
    for (size_t i = 1; i < p.size(); i++) {
      const float a0 = std::atan2(p[i - 1].y - gr[i - 1].y, p[i - 1].x - gr[i - 1].x), a1 = std::atan2(p[i].y - gr[i].y, p[i].x - gr[i].x);
      float d = a1 - a0;
      while (d > 3.14159265f) d -= 6.2831853f;
      while (d < -3.14159265f) d += 6.2831853f;
      tot += d;
    }
    return std::fabs(tot) * 57.29578f;
  };
  for (int f = 0; f < FAMILY_COUNT; f++) {
    const Family fam = (Family)f;
    const RigSheet s = humanRig(armed(fam), RigPart::Action);
    std::vector<Vec2> p, gr;
    const std::string who = familyName(fam);
    if (fam == Family::Bow) {
      const RigAnchors* a0 = s.anchorsOf(2, Clip::Draw, 0, 0);
      const RigAnchors* a2 = s.anchorsOf(2, Clip::Draw, 0, 2);
      if (!a0 || !a2 || a0->handRX == kNoAnchor || a2->handRX == kNoAnchor) { g.bad(who + ": no draw pose data"); continue; }
      const float d = std::fabs((float)(a2->handRX - a0->handRX));
      if (d < 6) g.bad(who + fmt(": the draw hand travels %.0f px back (6)", d)); else g.ok();
      continue;
    }
    path(s, 0, p, gr);
    if (p.size() < 3) { g.bad(who + ": no tip path in the pose data"); continue; }
    const Vec2 a = p.front(), b = p[p.size() / 2];
    float minY = 1e9f, maxY = -1e9f, maxX = 0;
    for (const Vec2& q : p) { minY = std::min(minY, q.y); maxY = std::max(maxY, q.y); maxX = std::max(maxX, std::fabs(q.x)); }
    const float along = std::fabs(b.x - a.x), side = std::fabs(b.y - a.y), turn = sweep(p, gr);
    bool ok = true;
    std::string m;
    switch (fam) {
      case Family::Unarmed: ok = along >= 7; m = fmt("the fist travels %.0f px (7)", along); break;
      case Family::Dagger: ok = along >= 8 && side <= 2; m = fmt("the tip travels %.0f px along, %.0f sideways (8, 2)", along, side); break;
      case Family::Sword: ok = turn >= 120; m = fmt("the arc spans %.0f deg (120)", turn); break;
      case Family::Greatsword: {
        std::vector<Vec2> p2, g2;
        path(s, 1, p2, g2);
        const float t2 = p2.size() > 2 ? sweep(p2, g2) : 0;
        ok = maxY - minY >= 24 && turn >= 100 && t2 >= 160;
        m = fmt("hit 1 falls %.0f px through %.0f deg (24, 100); hit 2 sweeps %.0f deg (160)", maxY - minY, turn, t2);
        break;
      }
      case Family::Axe: ok = maxY - minY >= 20 && turn >= 60 && p.back().y > -12; m = fmt("the head falls %.0f px through %.0f deg, ends at %.0f (20, 60, low)", maxY - minY, turn, p.back().y); break;
      case Family::Mace: ok = maxY - minY >= 24 && maxY >= -2; m = fmt("the head falls %.0f px, lands %.0f px over the ground (24, within 2)", maxY - minY, -maxY); break;
      case Family::Spear: ok = along >= 12 && side <= 2 && maxX >= 30; m = fmt("the tip travels %.0f straight, %.0f sideways, reaches %.0f (12, 2, 30)", along, side, maxX); break;
      case Family::Staff: ok = turn >= 90; m = fmt("the arc spans %.0f deg (90)", turn); break;
      default: break;
    }
    if (ok) g.ok(); else g.bad(who + ": " + m);
  }
}
void gateLeft(Gate& g) {   // R10
  for (int f = 0; f < 8; f++) {
    const RigSheet act = humanRig(armed((Family)f), RigPart::Action);
    const RigSheet lo = humanRig(armed((Family)f), RigPart::Loco);
    for (const RigSheet* s : {&lo, &act})
      for (int c = 0; c < CLIP_COUNT; c++)
        for (int i = 0; i < s->count(2, (Clip)c); i++) {
          const Fr r = cut(*s, 2, (Clip)c, 0, i), l = cut(*s, 3, (Clip)c, 0, i);
          const std::string who = std::string(familyName((Family)f)) + " " + clipName((Clip)c) + fmt(" %.0f", i);
          if (!l.ok) { g.bad(who + ": no left frame"); continue; }
          if (differ(l, mirror(r)) < 0.05f) g.bad(who + ": the left frame is the right one mirrored"); else g.ok();
        }
    for (int facing = 0; facing < RIG_FACINGS; facing++)
      for (Clip c : {Clip::Windup, Clip::Strike, Clip::Follow})
        for (int i = 0; i < act.count(facing, c); i++) {
          const RigAnchors* an = act.anchorsOf(facing, c, 0, i);
          if (!f) continue;   // (unarmed: no weapon)
          const std::string who = std::string(familyName((Family)f)) + fmt(" facing %.0f ", facing) + clipName(c);
          if (!an || an->gripX == kNoAnchor || an->handRX == kNoAnchor) { g.bad(who + ": no grip / hand pose data"); continue; }
          if (std::abs(an->gripX - an->handRX) > 2 || std::abs(an->gripY - an->handRY) > 2) g.bad(who + ": the weapon is not in the right hand"); else g.ok();
        }
    if (act.empty()) g.bad(std::string(familyName((Family)f)) + ": no ACTION atlas");
  }
}
void gatePostures(Gate& g) {   // P1
  const HumanLook L = bodyLook(0, 0, 0, 0);
  for (int p = 1; p < (int)Posture::COUNT; p++) {
    if ((Posture)p == Posture::Sleep) continue;
    const PostureInfo pi = postureInfo((Posture)p);
    const int variants = (Posture)p == Posture::Kneel ? 2 : 1;
    for (int v = 0; v < variants; v++) {
      const RigSheet s = humanPosture2(L, (Posture)p, (uint8_t)v);
      const int facings = (pi.oneRow || pi.lying) ? 1 : RIG_FACINGS;
      for (int facing = 0; facing < facings; facing++) {
        const int n = s.count(facing, Clip::Idle, 0);
        if (n != pi.frames) g.bad(fmt("posture %.0f variant %.0f facing %.0f: %.0f frames", p, v, facing, n) + fmt(" (%.0f)", pi.frames)); else g.ok();
      }
    }
  }
}
void gateSeats(Gate& g) {   // P2
  const HumanLook L = bodyLook(0, 0, 0, 0);
  for (Prop seat : {Prop::Chair, Prop::Stool, Prop::Bench, Prop::Throne, Prop::COUNT})
    for (int facing = 0; facing < RIG_FACINGS; facing++) {
      const SeatFit f = seatFit2(seat, -1, facing);
      if (!f.ok) continue;
      const RigSheet s = humanPosture2(L, f.sit, 0);
      const RigAnchors* an = s.anchorsOf(std::min(facing, 3), Clip::Idle, 0, 0);
      const std::string who = fmt("seat %.0f facing %.0f", (int)seat, facing);
      if (!an || an->hipY == kNoAnchor) { g.bad(who + ": no hip pose data"); continue; }
      const int hipRow = 16 + f.ay + an->hipY;   // the hips' row inside the seat's tile
      if (hipRow != f.seatY) g.bad(who + fmt(": the hips %.0f px off the seat", hipRow - f.seatY)); else g.ok();
    }
  for (int b = 0; b < (int)Berth::COUNT; b++) {
    const BedFit bf = bedFit2((Berth)b, -1);
    const Canvas c = sleeper2(L, (Berth)b, -1, 0);
    if (c.w <= 0) { g.bad(fmt("berth %.0f: no v2 sleeper", b)); continue; }
    if (!chA(c.get(bf.headX, bf.headY))) g.bad(fmt("berth %.0f: no head on the pillow at %.0f,%.0f", b, bf.headX, bf.headY)); else g.ok();
  }
}
void gateKin(Gate& g) {   // P3
  for (int age : {1, 2}) {
    const RigSheet s = humanRig(bodyLook(0, 0, 0, age), RigPart::Loco);
    const Fr f = cut(s, 0, Clip::Idle, 0, 0);
    int x0, y0, x1, y1;
    if (!f.ok || !box(f.c, x0, y0, x1, y1)) { g.bad(fmt("age %.0f: no LOCO", age)); continue; }
    const int want = age == 1 ? 21 : 26;
    if (y1 - y0 + 1 != want) g.bad(fmt("age %.0f: %.0f px tall (%.0f)", age, y1 - y0 + 1, want)); else g.ok();
  }
  for (int sp = 1; sp <= 3; sp++) {
    const int w = humanoidCellW2(sp), h = humanoidCellH2(sp);
    const Canvas c = humanoidCell2(sp, 0);
    int x0, y0, x1, y1;
    if (w <= 0 || h <= 0 || c.w <= 0 || !box(c, x0, y0, x1, y1)) { g.bad(fmt("humanoid %.0f: not on v2", sp)); continue; }
    const int fh = y1 - y0 + 1;
    const bool ok = sp == 3 ? (fh >= 22 && fh <= 26) : std::abs(fh - 32) <= 1;
    if (!ok) g.bad(fmt("humanoid %.0f: %.0f px tall (skeleton / draugr 32, goblin 0.7-0.8 of it)", sp, fh)); else g.ok();
  }
}
void gateHolders(Gate& g) {   // H2
  struct R { Holder h; float lo, hi; };
  static const R rs[] = {
      {Holder::HouseDoorH, 0.70f, 0.76f}, {Holder::SmallDoorH, 0.55f, 0.62f}, {Holder::TempleDoorH, 0.88f, 0.94f}, {Holder::GrandDoorH, 0.97f, 1.03f},
      {Holder::PalaceDoorH, 1.16f, 1.22f}, {Holder::WallOneStorey, 1.00f, 1.12f}, {Holder::ChairSeat, 0.31f, 0.37f}, {Holder::StoolSeat, 0.31f, 0.37f},
      {Holder::BenchSeat, 0.31f, 0.37f}, {Holder::TableTop, 0.44f, 0.50f}, {Holder::CounterTop, 0.44f, 0.50f}, {Holder::HorseWithers, 0.80f, 0.90f},
      {Holder::RowboatGunwale, 0.35f, 0.45f}, {Holder::DeckRail, 0.45f, 0.55f}, {Holder::GateClear, 1.25f, 9.0f}, {Holder::ArchClear, 1.25f, 9.0f},
      {Holder::CityGateClear, 1.25f, 9.0f}, {Holder::Fence, 0.45f, 0.60f}, {Holder::Hedge, 0.45f, 0.60f}, {Holder::GardenWall, 0.45f, 0.60f},
      {Holder::CartWheel, 0.45f, 0.55f}, {Holder::HandcartWheel, 0.45f, 0.55f}};
  const int was = rigVersion();
  setRigVersion(2);
  for (const R& r : rs) {
    const int px = holderPx(r.h);
    const float k = px / 32.0f;
    if (px <= 0) g.bad(std::string(holderName(r.h)) + ": not measured");
    else if (k < r.lo || k > r.hi) g.bad(std::string(holderName(r.h)) + fmt(": %.0f px = %.2f of the figure (%.2f-%.2f)", px, k, r.lo, r.hi));
    else g.ok();
  }
  if (holderPx(Holder::LadderRungStep) != 6) g.bad(fmt("ladder rungs every %.0f px (6)", holderPx(Holder::LadderRungStep))); else g.ok();
  setRigVersion(was);
}

int rigCmd(int argc, char** argv) {
  bool gates = false;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--report")) g_report = true;
  }
  g_contractFails = 0;
  setRigVersion(1);
  checkMetrics();
  checkFamilies();
  checkScale();
  checkSheetIndex();
  checkWeaponIds();
  checkInkOutline();
  checkHoldersRig1();
  std::printf("rig contract: %d failure(s)\n", g_contractFails);
  int gateFails = 0;
  if (gates) {
    setRigVersion(2);
    Gate r2{"R2 heights"}, r4{"R4 faces"}, r4h{"R4 hands"}, r5{"R5 frames"}, r6{"R6 sizes"}, r7{"R7 families"}, r8{"R8 tips"}, r9{"R9 variants"},
        r10{"R10 left / weapon hand"}, p1{"P1 postures"}, p2{"P2 seats / pillows"}, p3{"P3 kin / humanoids"}, h2{"H2 holders"};
    gateHeights(r2);
    gateFaces(r4, r4h);
    gateFrames(r5);
    gateSizes(r6);
    gateFamilies(r7, r9);
    gateTips(r8);
    gateLeft(r10);
    gatePostures(p1);
    gateSeats(p2);
    gateKin(p3);
    gateHolders(h2);
    setRigVersion(1);
    for (Gate* g : {&r2, &r4, &r4h, &r5, &r6, &r7, &r8, &r9, &r10, &p1, &p2, &p3, &h2}) {
      std::printf("%s %-24s %4d checks, %4d failed%s%s\n", g->fails ? "FAIL:" : "ok   ", g->id, g->checks, g->fails, g->fails ? "; first: " : "", g->first.c_str());
      gateFails += g->fails ? 1 : 0;
    }
  }
  const int bad = g_contractFails + gateFails;
  std::printf(bad ? "rig: %d failure(s)\n" : "rig: 0 failures\n", bad);
  return bad ? 1 : 0;
}
RPG_TEST_CMD("--rig", "P2a rig contract, restated for A3 at P2b [--gates: the doll: R2 R4-R10 P1-P3 H2] [--report]", rigCmd);

// R3: the census's plain looks drawn on v2
int coverageCmd(int argc, char** argv) {
  bool gates = false;
  uint64_t a = 1, b = 3;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
  }
  int plain = 0, covered = 0, all = 0;
  for (uint64_t s = a; s <= b; s++) {
    Game g(s);
    g.newEndlessGame(s);
    g.mode = Mode::Play;
    for (int f = 0; f < 60; f++) { g.update(SIM_DT, Input()); g.events.clear(); g.mode = Mode::Play; }
    for (const Actor& x : g.actors) {
      if (!x.human) continue;
      all++;
      const HumanLook& L = x.look;
      const bool armour = L.armorStyle || L.outfit == Outfit::Leather || L.outfit == Outfit::Chain || L.outfit == Outfit::Plate ||
                          L.outfit == Outfit::Elven || L.outfit == Outfit::Ebony || L.outfit == Outfit::Guard;
      if (armour || L.helmet || L.helmStyle || L.cloak || L.cape || L.shield || L.shieldStyle || L.bladeForm || L.polearmForm || L.bowForm) continue;
      plain++;
      covered += rig2Covers(L) ? 1 : 0;
    }
  }
  const double share = plain ? 100.0 * covered / plain : 0.0;
  std::printf("R3 rig coverage: %d of %d plain looks drawn on v2 (%.1f %%), %d humans in the census of seeds %llu..%llu\n", covered, plain, share, all,
              (unsigned long long)a, (unsigned long long)b);
  if (gates && share < 99.0) { std::printf("FAIL: R3 %.1f %% of the plain looks on v2 (99 or more)\n", share); return 1; }
  return 0;
}
RPG_TEST_CMD("--rig-coverage", "P2a rig coverage of a census's plain looks [--seeds A..B] [--gates: 99 % or more]", coverageCmd);

}  // namespace
