// doll_gallery (DOLL; P2b phase A skeleton by the lead): the look-and-adjust loop (STYLE_P2 24). The doll's parts, looks
// and clips on grass and dirt with the contact shadow at 1x, 2x and 4x (<board>_1x.png, <board>_2x.png, <board>.png), and
// one part or frame at 12x with a pixel grid and row numbers (A3's look.py contact sheet).
//   doll_gallery <outDir> [--pool FILE] [--who hero|woman] [--look k=v,...] [--palette a3|look] [board...]
//     looks       the look (the A3 hero and woman by default): idle and walk in four facings, the run, on grass and dirt
//     clips       every clip of the look's LOCO, RUN, ACTION and DEATH atlases, frame by frame on one ground line
//     parts       every part of the pool: its base views by facing and key, labelled with its name
//     lineup      a 12-person lineup of three peoples (idle facing down), the same seeds as the owner pack's
//     onion       the walk and the run in each facing: every frame at 30 % over the others (registration at a glance),
//                 then the frames side by side on one ground line with the sole row's span marked
//     death       the falls (both sides) and the lying figures (head west, head east) of every facing
//     sleep       the sleeper in each of the 10 berths over a plain quilt
//     stats       the look's atlas KB per part and bake ms (B10), colours per frame (B8), printed and boarded
//     ramps       the curated skins and hair, the generator's cloth, elves, elders and eyes, on the look's figure
//     part:<name>[:<D|U|R|L>[:<key>]]   one part's view alone at 12x with its pixel grid and row numbers
//     frame:<loco|run|action|death>:<D|U|R|L>:<clip>:<i>[:<variant>]   one composed frame of the look at 12x
//     frame:posture:<D|U|R|L>:<posture>:<i>[:<variant>]   one frame of a posture at 12x (<posture>: a name as in
//                 pose: below, or its art::Posture number; <variant> the posture's variant, e.g. Carry 1 the basket)
//     pose:<posture>[:<variant>]   every frame of a posture, R L D U rows on one ground line beside the look's idle, at 1x
//                 and 6x (doll_pose_<posture>[_v<variant>].png): carry, play, dance, sit, wave, ... (the walking postures:
//                 carry 0-2, play, dance)
//   (P2c SLICE-DOLL)
//     kit         the look in every armour form (padded leather mail scale lamellar brigandine plate leaf) on the man and the
//                 woman, idle in four facings (the fit pairs at a glance)
//     actions     every DRAWN and ACTION clip of the look frame by frame, a row per clip and facing, each frame's ms under it
//     grip:<part>:<D|U|R|L>:<clip>:<i>[:<variant>]   one frame at 12x with its grip (gold), off grip (cyan) and tip (red)
//                 marks, beside the clip's swing onion-skinned at 4x (every frame of windup, strike and follow at 30 %) with
//                 the tip's path drawn through them
//     silhouette  the slice's strike frames (the sword's, the spear's, the bow's release) filled flat, side by side in four
//                 facings, so the families read by shape alone
//   --slice <tag> takes a slice look (tools/doll/slice_looks.txt: hero guard knight archer knightm archerw) with its palette
//   --look sets the look's fields on top of the chosen A3 look: body hair hairVar build people age pattern headwear hood
//   beard beardStyle head nose brow marks neck garb cut outfit weapon cloak jewellery facePaint (numbers) and skin hairColor
//   topColor bottomColor tabardColor patternColor cloakColor headColor eyeColor (#RRGGBB); --palette look colours it by
//   rampsOf (the curated tables and the generator) instead of A3's palettes. (default boards: looks clips parts lineup onion)
//   The drafts before a map exists: node tools/doll/pxc.mjs --root rpg/art/doll/drafts/a3 --bin <scratch>/a3.pool --no-cpp
//   then doll_gallery <dir> --pool <scratch>/a3.pool. `node tools/doll/pxc.mjs --watch --gallery <exe> <dir> [boards]`
//   re-renders on every save.
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
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
using art::RigPart;
using art::RigSheet;

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
  }
  return L;
}

// ---------------------------------------------------------------- the options
int g_who = 0;
bool g_lookPalette = false;
std::string g_lookSpec;

bool applyField(art::HumanLook& L, const std::string& k, const std::string& v) { return art::doll::applyLookField(L, k, v); }   // (P2c: the one list, doll.h)
// (P2c) the slice looks (tools/doll/slice_looks.txt's lines)
struct SliceLook { const char* tag; int who; const char* spec; };
const SliceLook kSlice[] = {
    {"hero", 0, ""},
    {"guard", 0, "outfit=8,armorStyle=2,bodyForm=3,tabardColor=#A8322E,helmet=1,helmStyle=2,helmForm=1,shield=1,shieldStyle=3,shieldForm=2,"
                 "shieldDevice=#E8C25A,weapon=1,bladeForm=1,gloves=1,boots=1,pauldron=1,neck=0,head=1,hair=1,hairVar=1,hairColor=#4A3022,beard=1,"
                 "beardStyle=0,skin=#D9A07A"},
    {"knight", 1, "outfit=5,armorStyle=3,bodyForm=7,pauldron=3,skirt=2,gloves=3,boots=3,helmet=1,helmStyle=3,helmForm=9,crest=3,plumeColor=#B83A32,"
                  "weapon=8,polearmForm=1,hair=5,hairVar=0,hairColor=#2E1E18,skin=#E8B08A"},
    {"archer", 0, "outfit=3,armorStyle=1,bodyForm=2,armsOrnament=16,helmet=1,helmStyle=1,weapon=3,bowForm=1,gloves=1,boots=1,neck=0,topColor=#5E6A3A,"
                  "bottomColor=#4A3A2C,hair=1,hairVar=2,hairColor=#6A4426,beard=1,beardStyle=1,skin=#B98060"},
    {"knightm", 0, "outfit=5,armorStyle=3,bodyForm=7,pauldron=3,skirt=2,gloves=3,boots=3,helmet=1,helmStyle=3,helmForm=9,crest=3,plumeColor=#B83A32,"
                   "weapon=0,neck=0,head=4,beard=1,beardStyle=2"},
    {"archerw", 1, "outfit=3,armorStyle=1,bodyForm=2,armsOrnament=16,helmet=1,helmStyle=1,weapon=0,gloves=1,boots=1,topColor=#5E6A3A,"
                   "bottomColor=#4A3A2C,hair=3,hairVar=0,hairColor=#3A2418"},
};
doll::DollRecipe sliceRecipe(const SliceLook& sl, art::HumanLook* out = nullptr) {
  art::HumanLook L = a3Look(sl.who);
  doll::applyLookSpec(L, sl.spec);
  doll::DollRecipe r = doll::resolve(L);
  r.ramps = sl.spec[0] ? doll::rampsOf(L) : doll::a3Palette(sl.who);
  if (out) *out = L;
  return r;
}
art::HumanLook theLook(int who) {
  art::HumanLook L = a3Look(who);
  size_t a = 0;
  while (a < g_lookSpec.size()) {
    size_t b = g_lookSpec.find(',', a);
    if (b == std::string::npos) b = g_lookSpec.size();
    const std::string kv = g_lookSpec.substr(a, b - a);
    const size_t eq = kv.find('=');
    if (eq != std::string::npos && !applyField(L, kv.substr(0, eq), kv.substr(eq + 1))) std::printf("doll_gallery: --look: unknown field %s\n", kv.c_str());
    a = b + 1;
  }
  return L;
}
doll::DollRecipe recipeOf(int who) {
  doll::DollRecipe r = doll::resolve(theLook(who));
  if (!g_lookPalette) r.ramps = doll::a3Palette(theLook(who).body == 1 ? 1 : 0);
  return r;
}

// ---------------------------------------------------------------- drawing helpers
// the ground: half grass, half dirt, the contact shadow under each figure (the renderer's ellipse, STYLE_P2 7)
void dirt(Board& b, int y0, int y1) {
  for (int y = y0; y < y1 && y < b.c.h; y++)
    for (int x = b.c.w / 2; x < b.c.w; x++) {
      const uint32_t n = hash2(x / 2, y / 2) % 100;
      b.c.set(x, y, n < 20 ? rgba(110, 86, 60) : n > 90 ? rgba(140, 112, 80) : rgba(123, 95, 66));
    }
}
void plain(Board& b, uint32_t c) { for (uint32_t& p : b.c.px) p = c; }
void shadow(Board& b, int cx, int cy, int w) {
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -w / 2; dx <= w / 2; dx++) {
      const float e = (dx * dx) / (float)(w * w / 4) + (dy * dy) / 6.25f;
      if (e <= 1.0f) b.c.set(cx + dx, cy + dy, art::shade(b.c.get(cx + dx, cy + dy), 0.72f));
    }
}
void putFrame(Board& b, const RigSheet& s, int facing, Clip c, int v, int i, int x, int ground, bool shade = true) {
  int px, py;
  const Canvas f = rigpv::frameCanvas(s, facing, c, v, i, px, py);
  if (f.w <= 0) return;
  if (shade) shadow(b, x, ground, std::min(24, std::max(10, f.w - 4)));
  b.put(f, x - px, ground - py);
}
void save3(const Board& b, const std::string& dir, const std::string& name) {
  rigpv::saveBoth(b, dir, name);
  savePng(b.c, dir + "/" + name + "_2x.png", 2);
}
Canvas zoom12(const Canvas& f, int pivX, int pivY) {
  const int k = 12, lm = 30;
  Canvas z(f.w * k + lm, f.h * k + 2);
  for (size_t i = 0; i < z.px.size(); i++) z.px[i] = rgba(40, 36, 46);
  for (int y = 0; y < f.h; y++)
    for (int x = 0; x < f.w; x++) {
      const uint32_t p = f.get(x, y);
      for (int dy = 0; dy < k; dy++)
        for (int dx = 0; dx < k; dx++) {
          const bool grid = dx == 0 || dy == 0;
          uint32_t c = (p >> 24) ? (p | 0xFF000000u) : ((x + y) & 1 ? rgba(58, 54, 66) : rgba(50, 46, 58));
          if (grid) c = art::shade(c, 0.8f);
          z.set(lm + x * k + dx, y * k + dy, c);
        }
    }
  if (pivX >= 0) for (int dx = 0; dx < k; dx++) z.set(lm + pivX * k + dx, pivY * k + k - 1, rgba(242, 193, 78));
  return z;
}
void rowNumbers(Board& b, int x, int y0, int h, int pivY) {
  for (int y = 0; y < h; y++)
    if ((pivY - y) % 2 == 0) b.text(x, y0 + y * 12 + 2, "F" + (pivY - y ? "-" + std::to_string(pivY - y) : std::string()), rgba(200, 196, 180));
}

// ---------------------------------------------------------------- boards
void boardLooks(const std::string& dir) {
  Board b(900, 300);
  dirt(b, 0, 300);
  for (int who = 0; who < 2; who++) {
    const doll::DollRecipe r = recipeOf(who);
    const RigSheet s = doll::compose(r, RigPart::Loco), run = doll::compose(r, RigPart::Run);
    const int g0 = 80 + who * 130;
    b.text(4, g0 - 66, std::string(who ? "WOMAN" : "HERO") + (g_lookPalette ? " (LOOK PALETTE)" : " (A3 PALETTE)") + (g_lookSpec.empty() ? "" : " " + g_lookSpec));
    if (s.empty()) {
      b.text(4, g0 - 20, "NOT COVERED: NO CLIPS FOR THIS BODY SET", rgba(232, 96, 60));
      for (const doll::Missing& m : doll::dollMissing(theLook(who), RigPart::Loco)) { b.text(4, g0 - 8, m.what(), rgba(232, 96, 60)); break; }
      continue;
    }
    int x = 24;
    for (int f = 0; f < 4; f++) { putFrame(b, s, f, Clip::Idle, 0, 0, x, g0); x += 32; }
    x += 16;
    for (int f = 0; f < 4; f++) {
      for (int i = 0; i < s.count(f, Clip::Walk); i++) { putFrame(b, s, f, Clip::Walk, 0, i, x, g0); x += 24; }
      x += 8;
    }
    if (!run.empty()) {
      b.text(x, g0 - 56, "RUN");
      for (int f = 2; f < 4; f++)
        for (int i = 0; i < run.count(f, Clip::Walk); i++) { putFrame(b, run, f, Clip::Walk, 0, i, x, g0); x += 26; }
    }
  }
  save3(b, dir, "doll_looks");
}
void boardClips(const std::string& dir) {
  const doll::DollRecipe r = recipeOf(g_who);
  Board b(1400, 560);
  int y = 70;
  for (RigPart p : {RigPart::Loco, RigPart::Run, RigPart::Drawn, RigPart::Action, RigPart::Death}) {
    const RigSheet s = doll::compose(r, p);
    for (int c = 0; c < art::CLIP_COUNT; c++)
      for (int v = 0; v < art::RIG_VARIANTS; v++) {
        bool any = false;
        for (int f = 0; f < 4; f++) any = any || s.count(f, (Clip)c, v) > 0;
        if (!any || (v && s.frame(0, (Clip)c, v, 0) == s.frame(0, (Clip)c, 0, 0))) continue;
        rigpv::clipRow(b, s, (Clip)c, v, 70, y, 30, std::string(p == RigPart::Run ? "RUN " : "") + art::clipName((Clip)c) + (v ? " v" + std::to_string(v) : ""));
        y += 48;
        if (y > b.c.h - 10) break;
      }
  }
  if (y == 70) b.text(10, 40, "NO CLIPS IN THE POOL", rgba(232, 96, 60));
  save3(b, dir, "doll_clips");
}
void boardParts(const std::string& dir) {
  const int n = doll::partCount();
  Board b(1200, std::max(120, 20 + n * 56));
  int y = 50;
  static const char* const keys[] = {"rest", "idle", "walkA", "walkA2", "walkB", "walkC", "walkC2", "walkD", "run1", "run2", "run3", "run4",
                                     "run5", "run6", "hurt", "fwd", "back", "flow", "B", "N", "F", "A", "C", "L", "R", "up", "down",
                                     "swing", "wide", "flare", "lie"};
  for (int p = 0; p < n; p++) {
    b.text(4, y - 40, doll::partName(p));
    doll::DollRecipe r = recipeOf(doll::partServes(p, doll::BodySet::Woman) && !doll::partServes(p, doll::BodySet::Man) ? 1 : 0);
    int x = 40;
    for (int f = 0; f < 4; f++)
      for (const char* key : keys) {
        const RigSheet s = doll::composePart(r, doll::partName(p), f, key);
        if (s.empty()) continue;
        int px, py;
        const Canvas c = rigpv::frameCanvas(s, f, Clip::Idle, 0, 0, px, py);
        if (c.w <= 0) continue;
        b.put(c, x - px, y - py);
        x += std::max(16, c.w + 4);
        if (x > b.c.w - 40) break;
      }
    y += 56;
  }
  if (!n) b.text(10, 40, "THE POOL HAS NO PARTS YET", rgba(232, 96, 60));
  save3(b, dir, "doll_parts");
}
void boardLineup(const std::string& dir) {
  Board b(480, 130);
  dirt(b, 0, 130);
  static const cult::Archetype arch[3] = {cult::Archetype::Fjordfolk, cult::Archetype::Dune, cult::Archetype::Jade};
  int x = 20;
  for (int k = 0; k < 12; k++) {   // four villagers of each people, women and men by turns (census::dress)
    const cult::Culture C = cult::Atlas::make(arch[k / 4], 4242u + (uint32_t)arch[k / 4] * 97u);
    art::HumanLook L;
    std::string name;
    census::dress(L, name, Role::Villager, (k & 1) != 0, C, C, 0x11E0Bull * (uint64_t)(k + 1) ^ (uint64_t)arch[k / 4] << 32, census::Rank{});
    const RigSheet s = doll::compose(L, RigPart::Loco);
    if (s.empty()) b.text(x, 60, "-", rgba(232, 96, 60));
    putFrame(b, s, 0, Clip::Idle, 0, 0, x + 12, 90);
    // a look the doll does not cover yet draws v1 in the game: marked, with what it misses first
    if (!doll::dollCovers(L, RigPart::Loco)) {
      b.text(x + 9, 96, "X", rgba(232, 96, 60));
      const std::vector<doll::Missing> m = doll::dollMissing(L, RigPart::Loco);
      if (!m.empty()) std::printf("  lineup %d: not covered: %s\n", k + 1, m[0].what().c_str());
    }
    x += 36;
  }
  b.text(4, 4, "LINEUP (12, THREE PEOPLES) - IDLE DOWN");
  save3(b, dir, "doll_lineup");
}
// the walk and run registration board: per facing an onion skin (every frame at 30 %) and the strip of frames on one
// ground line, the lowest row's span under each frame (the planted foot) in Ember Gold
// Then, for the side views, the same onion skin with every frame moved on by the walk table's travel (walkTable: where
// each frame starts in the cycle), as the game places it: the planted foot must stay on one spot (a foot that slides
// smears sideways); the ground line and the pivot column ruled in; the crown's rows over each onion (the bob).
void onionAt(Board& b, const RigSheet& s, int f, int cx, int g0, const doll::WalkTable* travel) {
  const int n = s.count(f, Clip::Walk);
  for (int i = 0; i < n; i++) {
    int px, py;
    const Canvas c = rigpv::frameCanvas(s, f, Clip::Walk, 0, i, px, py);
    const int dx = travel && travel->frames ? (int)std::lround(travel->start[std::min(i, travel->frames - 1)]) * (f == 3 ? -1 : 1) : 0;
    for (int y = 0; y < c.h; y++)
      for (int x = 0; x < c.w; x++) {
        const uint32_t p = c.get(x, y);
        const int X = cx + dx - px + x, Y = g0 - py + y;
        if (p >> 24) b.c.set(X, Y, art::mix(b.c.get(X, Y), p | 0xFF000000u, 0.3f));
      }
  }
}
void boardOnion(const std::string& dir) {
  const doll::DollRecipe r = recipeOf(g_who);
  const art::HumanLook look = theLook(g_who);
  Board b(900, 250);
  int row = 0;
  const uint32_t panel = rgba(196, 190, 176), rule = rgba(170, 160, 146), ink = rgba(40, 36, 46);
  for (RigPart part : {RigPart::Loco, RigPart::Run}) {
    const RigSheet s = doll::compose(r, part);
    const int g0 = 80 + row * 110;
    b.text(4, g0 - 70, part == RigPart::Run ? "RUN: ONION SKIN (30 %, CROWN ROWS), MOVED ON BY THE TABLE; FRAMES" : "WALK: ONION SKIN (30 %, CROWN ROWS), MOVED ON BY THE TABLE; FRAMES");
    for (int y = g0 - 50; y < g0 + 16; y++)   // a neutral panel under the onion skins (30 % reads on it, not on grass)
      for (int x = 2; x < 330; x++) b.c.set(x, y, panel);
    for (int x = 2; x < 330; x++) b.c.set(x, g0 + 1, rule);   // the ground line under the sole row
    for (int f = 0; f < 4; f++) {
      const int cx = 24 + f * 44;
      for (int y = g0 - 48; y <= g0; y++) if (y % 2 == 0) b.c.set(cx, y, rule);   // the pivot column, dotted
      onionAt(b, s, f, cx, g0, nullptr);
      b.c.set(cx, g0 + 2, rgba(200, 60, 40));
      b.text(cx - 3, g0 + 5, std::string(1, "DURL"[f]), ink);
      // the crown's rows over the frames (the bob)
      int top0 = 99, top1 = -99;
      for (int i = 0; i < s.count(f, Clip::Walk); i++) {
        const art::FrameRect* fr = s.frame(f, Clip::Walk, 0, i);
        const art::RigAnchors* an = s.anchorsOf(f, Clip::Walk, 0, i);
        if (!fr) continue;
        if (an) { top0 = std::min<int>(top0, -an->topY); top1 = std::max<int>(top1, -an->topY); }
      }
      if (top1 >= 0) {
        const std::string t = top0 == top1 ? std::to_string(top0) : std::to_string(top0) + "-" + std::to_string(top1);
        b.text(cx - (int)t.size() * 3, g0 - 48, t, ink);
      }
    }
    // the side views moved on by the table: the planted foot stays put
    for (int f = 2; f < 4; f++) {
      const doll::WalkTable t = doll::walkTable(look, part == RigPart::Run ? doll::Gait::Run : doll::Gait::Walk, f);
      const int cx = f == 2 ? 196 : 300;
      onionAt(b, s, f, cx, g0, &t);
      b.text(cx - 12, g0 + 5, std::string(f == 2 ? "R+" : "L-") + std::to_string((int)std::lround(t.cycle)), ink);
    }
    int x = 350;
    for (int f = 2; f < 4; f++) {
      const int n = s.count(f, Clip::Walk);
      for (int i = 0; i < n; i++) {
        int px, py;
        const Canvas c = rigpv::frameCanvas(s, f, Clip::Walk, 0, i, px, py);
        if (c.w <= 0) continue;
        b.put(c, x - px, g0 - py);
        for (int xx = 0; xx < c.w; xx++) if (c.get(xx, c.h - 1) >> 24) b.c.set(x - px + xx, g0 + 2, rgba(242, 193, 78));
        b.c.set(x, g0 + 4, rgba(255, 255, 255));
        x += 28;
      }
      x += 14;
    }
    if (s.empty()) b.text(210, g0 - 20, "NO CLIPS", rgba(232, 96, 60));
    row++;
  }
  save3(b, dir, "doll_onion");
}
void boardDeath(const std::string& dir) {
  const doll::DollRecipe r = recipeOf(g_who);
  const RigSheet s = doll::compose(r, RigPart::Death);
  Board b(900, 260);
  dirt(b, 0, 260);
  if (s.empty()) b.text(10, 40, "NO DEATH CLIPS FOR THIS BODY SET", rgba(232, 96, 60));
  for (int v = 0; v < 2; v++) {
    const int g0 = 80 + v * 110;
    b.text(4, g0 - 66, v ? "FALL EAST, THEN LYING HEAD EAST" : "FALL WEST, THEN LYING HEAD WEST");
    int x = 30;
    for (int f = 0; f < 4; f++) {
      for (int i = 0; i < s.count(f, Clip::Fall, v); i++) { putFrame(b, s, f, Clip::Fall, v, i, x, g0); x += 40; }
      x += 12;
    }
    int px, py;
    const Canvas ly = rigpv::frameCanvas(s, 0, Clip::Lying, v, 0, px, py);
    if (ly.w > 0) {
      shadow(b, x + 20, g0, ly.w);
      b.put(ly, x + 20 - px, g0 - py);
      b.text(x, g0 + 8, std::to_string(ly.w) + "X" + std::to_string(ly.h));
    }
  }
  save3(b, dir, "doll_death");
}
void boardSleep(const std::string& dir) {
  const art::HumanLook L = theLook(g_who);
  Board b(560, 120);
  plain(b, rgba(60, 50, 44));
  int x = 8;
  for (int k = 0; k < (int)art::Berth::COUNT; k++) {
    const art::holders::Berth2 g = art::holders::berth2((art::Berth)k, -1);
    Canvas quilt(g.w, g.h);
    for (int y = 0; y < g.h; y++)
      for (int xx = 0; xx < g.w; xx++) {
        const bool cover = g.across ? (xx >= g.quiltTop && xx <= g.quiltEnd) : (y >= g.quiltTop && y <= g.quiltEnd);
        quilt.set(xx, y, cover ? rgba(150, 70, 60) : rgba(214, 206, 186));
      }
    b.put(quilt, x, 20);
    const Canvas c = doll::sleeper(L, (art::Berth)k, -1, 0);
    if (c.w > 0) b.put(c, x, 20); else b.text(x, 30, "V1", rgba(232, 96, 60));
    x += std::max(g.w, 24) + 8;
  }
  b.text(4, 4, "SLEEPERS IN THE 10 BERTHS (OVER A PLAIN QUILT)");
  save3(b, dir, "doll_sleep");
}
void boardStats(const std::string& dir) {
  const doll::DollRecipe r = recipeOf(g_who);
  Board b(520, 120);
  plain(b, rgba(40, 36, 46));
  int y = 6;
  auto line = [&](const std::string& s) { std::printf("  %s\n", s.c_str()); b.text(6, y, s); y += 10; };
  static const char* const nm[4] = {"LOCO", "ACTION", "DEATH", "RUN"};
  static const int budget[4] = {128, 512, 48, 64};
  for (int p = 0; p < 4; p++) {
    const auto t0 = std::chrono::steady_clock::now();
    const RigSheet s = doll::compose(r, (RigPart)p);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    int maxCols = 0;
    long long area = 0;
    std::set<std::pair<int, int>> seenRect;
    for (const art::FrameRect& f : s.frames) if (seenRect.insert({f.x, f.y}).second) area += (long long)f.w * f.h;
    for (size_t i = 0; i < s.frames.size(); i++) {
      const art::FrameRect& f = s.frames[i];
      std::set<uint32_t> cols;
      for (int yy = 0; yy < f.h; yy++) for (int xx = 0; xx < f.w; xx++) { const uint32_t px = s.atlas.get(f.x + xx, f.y + yy); if (px >> 24) cols.insert(px); }
      maxCols = std::max(maxCols, (int)cols.size());
    }
    char buf[160];
    std::snprintf(buf, sizeof buf, "%-6s %3zu FRAMES %6.1f KB (BUDGET %d) PACKED %2.0f%%  BAKE %.3f MS  COLOURS %d (48)", nm[p], s.frames.size(), s.bytes() / 1024.0,
                  budget[p], s.bytes() ? 400.0 * area / s.bytes() : 0.0, ms, maxCols);
    line(buf);
  }
  doll::ComposeOpts one;
  one.onlyFacing = 2;
  std::vector<double> t;
  for (int i = 0; i < 40; i++) {
    const auto t0 = std::chrono::steady_clock::now();
    const RigSheet s = doll::compose(r, RigPart::Loco, &one);
    t.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    (void)s;
  }
  std::sort(t.begin(), t.end());
  char buf[160];
  std::snprintf(buf, sizeof buf, "ONE FACING'S LOCO: P50 %.3f P95 %.3f MS (GATE 0.385)", t[t.size() / 2], t[t.size() * 95 / 100]);
  line(buf);
  const doll::PoolStats ps = doll::poolStats();
  std::snprintf(buf, sizeof buf, "POOL %d PARTS %d VIEWS %d CLIPS %.3f MB (1.0)", ps.parts, ps.views, ps.clips, ps.bytes / 1048576.0);
  line(buf);
  save3(b, dir, "doll_stats");
}
// the curated tables and the generator on the look: the hero in each of the culture's 12 skins and the creator's 8,
// in the creator's 10 hair colours and the cultures' darkest, the woman in the creator's 12 shirts; elves and elders;
// with each ramp's five steps as a swatch under the figure
void boardRamps(const std::string& dir) {
  static const uint32_t skins[20] = {rgba(250, 222, 200), rgba(244, 204, 168), rgba(236, 188, 146), rgba(232, 180, 140), rgba(218, 164, 120),
                                     rgba(200, 146, 104), rgba(180, 124, 86),  rgba(160, 108, 74),  rgba(150, 100, 70),  rgba(128, 84, 58),
                                     rgba(110, 72, 50),   rgba(88, 58, 42),    rgba(250, 220, 190), rgba(242, 204, 166), rgba(236, 188, 146),
                                     rgba(214, 160, 116), rgba(190, 134, 94),  rgba(160, 104, 70),  rgba(124, 80, 56),   rgba(92, 60, 46)};
  static const uint32_t hairs[14] = {rgba(36, 30, 34),  rgba(70, 44, 30),   rgba(120, 70, 36),   rgba(150, 64, 36),  rgba(200, 104, 44),
                                     rgba(220, 180, 100), rgba(232, 220, 182), rgba(150, 148, 150), rgba(228, 226, 218), rgba(120, 50, 110),
                                     rgba(24, 20, 20),  rgba(30, 24, 22),   rgba(60, 42, 30),    rgba(196, 160, 100)};
  static const uint32_t shirts[12] = {rgba(70, 110, 150), rgba(150, 50, 44), rgba(60, 110, 64), rgba(196, 170, 120), rgba(110, 70, 140), rgba(200, 120, 40),
                                      rgba(80, 80, 88),   rgba(226, 220, 200), rgba(40, 60, 100), rgba(130, 40, 70),  rgba(70, 130, 130), rgba(140, 100, 60)};
  Board b(760, 330);
  dirt(b, 0, 330);
  auto swatch = [&](int x, int y, const uint32_t* c) { for (int i = 0; i < 5; i++) for (int dy = 0; dy < 3; dy++) for (int dx = 0; dx < 3; dx++) b.c.set(x + i * 3 + dx, y + dy, c[i] | 0xFF000000u); };
  auto figure = [&](const art::HumanLook& L, int x, int g, art::doll::Slot slot) {
    doll::DollRecipe r = doll::resolve(L);
    const RigSheet s = doll::compose(r, RigPart::Loco);
    putFrame(b, s, 0, Clip::Idle, 0, 0, x, g);
    swatch(x - 7, g + 4, r.ramps.c[(int)slot]);
  };
  b.text(4, 4, "SKINS: CULTURE 12, CREATOR 8 (CURATED FROM A3'S FOUR)");
  for (int i = 0; i < 20; i++) { art::HumanLook L = a3Look(0); L.skin = skins[i]; figure(L, 14 + i * 36, 52, doll::Slot::Skin); }
  b.text(4, 74, "HAIR: CREATOR 10, CULTURE DARKS; ELF AND ELDER LAST");
  for (int i = 0; i < 14; i++) { art::HumanLook L = a3Look(0); L.hairColor = hairs[i]; figure(L, 14 + i * 36, 122, doll::Slot::Hair); }
  { art::HumanLook L = a3Look(0); L.people = 2; figure(L, 14 + 14 * 36, 122, doll::Slot::Skin); }
  { art::HumanLook L = a3Look(0); L.age = 3; figure(L, 14 + 15 * 36, 122, doll::Slot::Hair); }
  { art::HumanLook L = a3Look(0); L.age = 3; L.hairColor = rgba(220, 180, 100); figure(L, 14 + 16 * 36, 122, doll::Slot::Hair); }
  b.text(4, 144, "CLOTH: THE CREATOR'S 12 SHIRTS (THE GENERATOR)");
  for (int i = 0; i < 12; i++) { art::HumanLook L = a3Look(1); L.topColor = shirts[i]; figure(L, 14 + i * 36, 192, doll::Slot::Cloth1); }
  b.text(4, 214, "CLOTH: THE CREATOR'S 12 SHIRTS ON THE HERO");
  for (int i = 0; i < 12; i++) { art::HumanLook L = a3Look(0); L.topColor = shirts[i]; figure(L, 14 + i * 36, 262, doll::Slot::Cloth1); }
  b.text(4, 284, "EYES: AUTO BY SKIN AND HAIR (0), THE CREATOR'S 6");
  static const uint32_t eyes[7] = {0, rgba(96, 58, 32), rgba(60, 112, 196), rgba(56, 140, 80), rgba(120, 132, 146), rgba(206, 146, 40), rgba(136, 80, 176)};
  for (int i = 0; i < 7; i++) {
    art::HumanLook L = a3Look(0); L.eyeColor = eyes[i];
    const doll::DollRecipe r = doll::resolve(L);
    for (int k = 2; k < 4; k++) for (int dy = 0; dy < 6; dy++) for (int dx = 0; dx < 6; dx++) b.c.set(14 + i * 36 + (k - 2) * 7 + dx, 300 + dy, r.ramps.c[(int)doll::Slot::Eyes][k] | 0xFF000000u);
  }
  save3(b, dir, "doll_ramps");
}
int facingOf(const std::string& s) { return s == "U" ? 1 : s == "R" ? 2 : s == "L" ? 3 : 0; }
std::vector<std::string> split(const std::string& s);
// art::Posture by name (lower case, as postures.clip names them) or number; -1: unknown
const char* const kPostureNames[] = {"none", "sit", "siteat", "sitdrink", "eat", "drink", "cheer", "hammer", "hoe", "sweep", "chop", "stir",
                                     "carry", "fish", "sleep", "wave", "play", "dance", "lute", "drum", "flute", "pray", "beg", "lamp",
                                     "read", "sitfloor", "sitflooreat", "sitfloordrink", "ride", "downed", "kneel"};
int postureOf(const std::string& s) {
  if (!s.empty() && std::isdigit((unsigned char)s[0])) return std::atoi(s.c_str());
  std::string l = s;
  for (char& c : l) c = (char)std::tolower((unsigned char)c);
  for (int i = 0; i < (int)(sizeof kPostureNames / sizeof kPostureNames[0]); i++) if (l == kPostureNames[i]) return i;
  return -1;
}
// pose:<posture>[:<variant>]: every frame of the posture in R, L, D and U, beside the look's idle, on one ground line
void boardPose(const std::string& dir, const std::string& arg) {
  const std::vector<std::string> w = split(arg);
  const int pz = w.size() > 1 ? postureOf(w[1]) : -1;
  if (pz <= 0) { std::printf("doll_gallery: %s: pose:<posture name or number>[:<variant>]\n", arg.c_str()); return; }
  const int variant = w.size() > 2 ? std::atoi(w[2].c_str()) : 0;
  const doll::DollRecipe r = recipeOf(g_who);
  const RigSheet ps = doll::composePosture(r, (art::Posture)pz, (uint8_t)variant), idle = doll::compose(r, RigPart::Loco);
  int n = 0;
  for (int f = 0; f < 4; f++) n = std::max(n, ps.count(f, Clip::Idle, 0));
  if (!n) { std::printf("doll_gallery: %s: the look has no such posture\n", arg.c_str()); return; }
  const int cw = 36, rh = 50, x0 = 56;
  Board b(x0 + (n + 1) * cw + 8, 24 + 4 * rh);
  plain(b, rgba(60, 104, 70));
  b.text(4, 4, arg + " (" + std::to_string(n) + " frames; idle first)");
  const int order[4] = {2, 3, 0, 1};
  for (int k = 0; k < 4; k++) {
    const int fc = order[k], ground = 24 + k * rh + rh - 6;
    b.text(4, ground - 20, std::string(1, "DURL"[fc]));
    for (int x = 0; x < b.c.w; x++) b.c.set(x, ground + 1, rgba(46, 82, 54));
    putFrame(b, idle, fc, Clip::Idle, 0, 0, x0 - cw / 2 + cw / 2, ground);
    for (int i = 0; i < ps.count(fc, Clip::Idle, 0); i++) putFrame(b, ps, fc, Clip::Idle, 0, i, x0 + (i + 1) * cw, ground);
  }
  std::string name = "doll_pose_" + std::string(pz < (int)(sizeof kPostureNames / sizeof kPostureNames[0]) ? kPostureNames[pz] : w[1].c_str()) +
                     (variant ? "_v" + std::to_string(variant) : std::string());
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
  savePng(b.c, dir + "/" + name + ".png", 6);
}
std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> v;
  size_t a = 0;
  for (size_t b2; (b2 = s.find(':', a)) != std::string::npos; a = b2 + 1) v.push_back(s.substr(a, b2 - a));
  v.push_back(s.substr(a));
  return v;
}
void boardZoom(const std::string& dir, const std::string& arg) {
  const std::vector<std::string> w = split(arg);
  RigSheet s;
  int facing = 0, idx = 0, variant = 0;
  Clip clip = Clip::Idle;
  if (w[0] == "part" && w.size() >= 2) {
    facing = w.size() > 2 ? facingOf(w[2]) : 0;
    const int p = doll::findPart(w[1]);
    const doll::DollRecipe r = recipeOf(p >= 0 && doll::partServes(p, doll::BodySet::Woman) && !doll::partServes(p, doll::BodySet::Man) ? 1 : 0);
    s = doll::composePart(r, w[1], facing, w.size() > 3 ? w[3] : "rest");
  } else if (w[0] == "frame" && w.size() >= 5) {
    // frame:<part>:<facing>:<clip>:<i>[:<variant>] (the old frame:<who>:... form is read too)
    size_t o = 1;
    int who = g_who;
    if (w[1] == "hero" || w[1] == "woman") { who = w[1] == "woman" ? 1 : 0; o = 2; }
    if (w.size() < o + 4) { std::printf("doll_gallery: %s: frame:<part>:<facing>:<clip>:<i>[:<variant>]\n", arg.c_str()); return; }
    const RigPart part = w[o] == "run" ? RigPart::Run : w[o] == "action" ? RigPart::Action : w[o] == "drawn" ? RigPart::Drawn : w[o] == "death" ? RigPart::Death : RigPart::Loco;
    // (OUTER) frame:posture:<facing>:<posture>:<i>[:<variant>]: one frame of a posture (the cloaks' sit, floor, kneel and
    // ride drapes; the walking postures' Carry 0-2, Play and Dance). <posture> a name or its art::Posture number; the
    // variant is the posture's (composePosture files every variant's frames as Idle variant 0)
    facing = facingOf(w[o + 1]);
    idx = std::atoi(w[o + 3].c_str());
    if (w[o] == "posture") {
      const int pz = postureOf(w[o + 2]);
      if (pz <= 0) { std::printf("doll_gallery: %s: unknown posture %s\n", arg.c_str(), w[o + 2].c_str()); return; }
      s = doll::composePosture(recipeOf(who), (art::Posture)pz, (uint8_t)(w.size() > o + 4 ? std::atoi(w[o + 4].c_str()) : 0));
    } else {
      s = doll::compose(recipeOf(who), part);
      for (int c = 0; c < art::CLIP_COUNT; c++) if (w[o + 2] == art::clipName((Clip)c)) clip = (Clip)c;
      if (w.size() > o + 4) variant = std::atoi(w[o + 4].c_str());
    }
  }
  int px, py;
  const Canvas f = rigpv::frameCanvas(s, facing, clip, variant, idx, px, py);
  if (f.w <= 0) { std::printf("doll_gallery: %s: nothing to show\n", arg.c_str()); return; }
  const Canvas z = zoom12(f, px, py);
  Board b(z.w + 4, z.h + 24);
  plain(b, rgba(40, 36, 46));
  b.put(z, 2, 20);
  rowNumbers(b, 2, 20, f.h, py);
  b.text(4, 4, arg);
  std::string name = "doll_zoom_" + arg;
  for (char& ch : name) if (ch == ':' || ch == '/' || ch == '\\') ch = '_';
  savePng(b.c, dir + "/" + name + ".png", 1);
}

// ---------------------------------------------------------------- (P2c SLICE-DOLL) kit, actions, grip, silhouette
void boardKit(const std::string& dir) {
  static const char* kForms[8] = {"padded", "leather", "mail", "scale", "lamellar", "brigandine", "plate", "leaf"};
  Board b(8 * 150 + 40, 2 * 120 + 40);
  dirt(b, 0, b.c.h);
  b.text(4, 4, "KIT: EVERY ARMOUR FORM ON THE MAN AND THE WOMAN, IDLE D U R L" + (g_lookSpec.empty() ? std::string() : " (" + g_lookSpec + ")"));
  for (int who = 0; who < 2; who++)
    for (int f = 0; f < 8; f++) {
      art::HumanLook L = theLook(who);
      L.bodyForm = (uint8_t)(f + 1);
      if (L.outfit == art::Outfit::Dress) L.outfit = art::Outfit::Tunic;
      if (f == 6 && !L.pauldron) L.pauldron = 3;
      doll::DollRecipe r = doll::resolve(L);
      if (!g_lookPalette) r.ramps = doll::a3Palette(L.body == 1 ? 1 : 0);
      const RigSheet s = doll::compose(r, RigPart::Loco);
      const int x0 = 20 + f * 150, g0 = 90 + who * 120;
      b.text(x0, g0 - 70, std::string(kForms[f]) + (who ? " W" : " M"));
      if (s.empty()) {
        for (const doll::Missing& m : doll::dollMissing(L, RigPart::Loco)) { b.text(x0, g0 - 58, m.value + ": NO PART", rgba(232, 96, 60)); break; }
        continue;
      }
      for (int fc = 0; fc < 4; fc++) putFrame(b, s, fc, Clip::Idle, 0, 0, x0 + 16 + fc * 32, g0);
    }
  save3(b, dir, "doll_kit");
}
void boardActions(const std::string& dir) {
  const doll::DollRecipe r = recipeOf(g_who);
  struct Row { RigPart part; Clip clip; int variant; };
  std::vector<Row> rows;
  for (RigPart p : {RigPart::Drawn, RigPart::Action}) {
    const RigSheet s = doll::compose(r, p);
    for (int c = 0; c < art::CLIP_COUNT; c++)
      for (int v = 0; v < art::RIG_VARIANTS; v++) {
        bool any = false;
        for (int f = 0; f < 4; f++) {
          const RigSheet::Span& sp = s.spans[RigSheet::slot(f, (Clip)c, v)];
          any = any || sp.count > 0;
        }
        if (any) rows.push_back({p, (Clip)c, v});
      }
  }
  Board b(1600, std::max(120, (int)rows.size() * 4 * 56 + 60));
  dirt(b, 0, b.c.h);
  b.text(4, 4, "ACTIONS: EVERY DRAWN AND ACTION CLIP, FRAME BY FRAME, D U R L (MS UNDER EACH FRAME)" + (g_lookSpec.empty() ? std::string() : " " + g_lookSpec));
  if (rows.empty()) b.text(4, 30, "NO DRAWN OR ACTION CLIPS FOR THIS LOOK", rgba(232, 96, 60));
  std::map<int, RigSheet> cache;
  int y = 70;
  for (const Row& row : rows) {
    auto it = cache.find((int)row.part);
    if (it == cache.end()) it = cache.emplace((int)row.part, doll::compose(r, row.part)).first;
    const RigSheet& s = it->second;
    for (int fc = 0; fc < 4; fc++) {
      const RigSheet::Span& sp = s.spans[RigSheet::slot(fc, row.clip, row.variant)];
      b.text(4, y - 40, std::string(row.part == RigPart::Drawn ? "DRAWN " : "ACTION ") + art::clipName(row.clip) + " v" + std::to_string(row.variant) + " " + "DURL"[fc]);
      int x = 150;
      for (int i = 0; i < sp.count; i++) {
        putFrame(b, s, fc, row.clip, row.variant, i, x, y);
        const doll::FrameInfo fi = doll::frameInfo(r, row.part, fc, row.clip, row.variant, i);
        b.text(x - 8, y + 6, std::to_string(fi.ms) + "MS", rgba(240, 236, 220));
        x += 56;
      }
      y += 56;
    }
  }
  save3(b, dir, "doll_actions");
}
// the marks of a probe, pivot-relative
struct Marks { bool g = false, gl = false, t = false; int gx = 0, gy = 0, glx = 0, gly = 0, tx = 0, ty = 0; };
Marks marksOf(const doll::FrameProbe& p) {
  Marks m;
  if (!p.ok) return m;
  m.g = p.grip; m.gx = p.gripX - p.px; m.gy = p.gripY - p.py;
  m.gl = p.gripL; m.glx = p.gripLX - p.px; m.gly = p.gripLY - p.py;
  m.t = p.tip; m.tx = p.tipX - p.px; m.ty = p.tipY - p.py;
  return m;
}
void boardGrip(const std::string& dir, const std::string& arg) {
  const std::vector<std::string> w = split(arg);
  if (w.size() < 5) { std::printf("doll_gallery: grip:<part>:<D|U|R|L>:<clip>:<i>[:<variant>]\n"); return; }
  const RigPart part = w[1] == "run" ? RigPart::Run : w[1] == "action" ? RigPart::Action : w[1] == "drawn" ? RigPart::Drawn : w[1] == "death" ? RigPart::Death : RigPart::Loco;
  const int facing = facingOf(w[2]);
  Clip clip = Clip::Idle;
  for (int c = 0; c < art::CLIP_COUNT; c++) if (w[3] == art::clipName((Clip)c)) clip = (Clip)c;
  const int idx = std::atoi(w[4].c_str()), variant = w.size() > 5 ? std::atoi(w[5].c_str()) : 0;
  const doll::DollRecipe r = recipeOf(g_who);
  const RigSheet s = doll::compose(r, part);
  int px, py;
  const Canvas f = rigpv::frameCanvas(s, facing, clip, variant, idx, px, py);
  if (f.w <= 0) { std::printf("doll_gallery: %s: nothing to show\n", arg.c_str()); return; }
  Canvas z = zoom12(f, px, py);
  const Marks m = marksOf(doll::probeFrame(r, part, facing, clip, variant, idx));
  auto mark = [&](bool on, int mx, int my, uint32_t col) {
    if (!on) return;
    const int X = 30 + (mx + px) * 12, Y = (my + py) * 12;
    for (int d = 0; d < 12; d++) {
      z.set(X + d, Y, col); z.set(X + d, Y + 11, col); z.set(X, Y + d, col); z.set(X + 11, Y + d, col);
      z.set(X + d, Y + 1, col); z.set(X + d, Y + 10, col); z.set(X + 1, Y + d, col); z.set(X + 10, Y + d, col);
    }
  };
  mark(m.g, m.gx, m.gy, rgba(242, 193, 78));
  mark(m.gl, m.glx, m.gly, rgba(90, 220, 230));
  mark(m.t, m.tx, m.ty, rgba(232, 70, 60));
  // the swing onion-skinned at 4x: every frame of windup, strike and follow at 30 % over the board, the tip path drawn
  const int k = 4, ow = 72, oh = 72;
  Canvas on(ow * k, oh * k);
  for (uint32_t& q : on.px) q = rgba(40, 36, 46);
  std::vector<std::pair<int, int>> tips;
  const int cx = ow / 2, cy = oh - 12;
  const RigSheet as = doll::compose(r, RigPart::Action);
  int nFrames = 0;
  for (Clip c : {Clip::Windup, Clip::Strike, Clip::Follow})
    for (int i = 0; i < as.count(facing, c, variant); i++) {
      int fx, fy;
      const Canvas fr = rigpv::frameCanvas(as, facing, c, variant, i, fx, fy);
      if (fr.w <= 0) continue;
      nFrames++;
      for (int y = 0; y < fr.h; y++)
        for (int x = 0; x < fr.w; x++) {
          const uint32_t q = fr.get(x, y);
          if (!(q >> 24)) continue;
          const int X = cx - fx + x, Y = cy - fy + y;
          if (X < 0 || Y < 0 || X >= ow || Y >= oh) continue;
          for (int dy = 0; dy < k; dy++)
            for (int dx = 0; dx < k; dx++) {
              const uint32_t o = on.get(X * k + dx, Y * k + dy);
              auto mix = [&](int sh) { return (int)(((o >> sh) & 255) * 0.7 + ((q >> sh) & 255) * 0.3); };
              on.set(X * k + dx, Y * k + dy, rgba(mix(0), mix(8), mix(16)));
            }
        }
      const Marks mm = marksOf(doll::probeFrame(r, RigPart::Action, facing, c, variant, i));
      if (mm.t) tips.push_back({cx + mm.tx, cy + mm.ty});
    }
  for (size_t i = 0; i + 1 < tips.size(); i++) {   // the tip's path: a line through the tip marks, a dot on each
    const int x0 = tips[i].first * k + 2, y0 = tips[i].second * k + 2, x1 = tips[i + 1].first * k + 2, y1 = tips[i + 1].second * k + 2;
    const int n = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    for (int j = 0; j <= n; j++) on.set(x0 + (x1 - x0) * j / std::max(1, n), y0 + (y1 - y0) * j / std::max(1, n), rgba(232, 70, 60));
  }
  for (const auto& t : tips) for (int dy = 0; dy < k; dy++) for (int dx = 0; dx < k; dx++) on.set(t.first * k + dx, t.second * k + dy, rgba(255, 220, 120));
  Board b(z.w + on.w + 12, std::max(z.h, on.h) + 40);
  plain(b, rgba(40, 36, 46));
  b.put(z, 2, 30);
  rowNumbers(b, 2, 30, f.h, py);
  b.put(on, z.w + 8, 30);
  b.text(4, 4, arg + "  GRIP (GOLD) OFF GRIP (CYAN) TIP (RED)" + (m.g ? "" : "  NO GRIP MARK"));
  b.text(z.w + 8, 16, "THE SWING ONION-SKINNED (" + std::to_string(nFrames) + " FRAMES), THE TIP PATH" + (tips.empty() ? ": NO TIP MARKS" : ""));
  std::string name = "doll_grip_" + arg.substr(5);
  for (char& ch : name) if (ch == ':' || ch == '/' || ch == '\\') ch = '_';
  savePng(b.c, dir + "/" + name + ".png", 1);
}
void boardSilhouette(const std::string& dir) {
  struct Fam { const char* tag; Clip clip; const char* label; };
  const Fam fams[] = {{"hero", Clip::Strike, "SWORD (A3)"}, {"guard", Clip::Strike, "SWORD + SHIELD"}, {"knight", Clip::Strike, "SPEAR"}, {"archer", Clip::Release, "BOW"}};
  const int cell = 70;
  Board b(4 * 4 * cell + 60, 2 * cell + 80);
  plain(b, rgba(226, 220, 204));
  b.text(4, 4, "SILHOUETTE: EVERY STRIKE FRAME FILLED FLAT (THE BOW: ITS RELEASE), D U R L PER FAMILY", rgba(30, 22, 42));
  int x0 = 20;
  for (const Fam& fm : fams) {
    const SliceLook* sl = nullptr;
    for (const SliceLook& q : kSlice) if (!std::strcmp(q.tag, fm.tag)) sl = &q;
    const doll::DollRecipe r = sliceRecipe(*sl);
    const RigSheet s = doll::compose(r, RigPart::Action);
    b.text(x0, 24, fm.label, rgba(30, 22, 42));
    for (int fc = 0; fc < 4; fc++) {
      for (int row = 0; row < 2; row++) {   // row 0: filled flat; row 1: the frame itself
        int px, py;
        const Canvas f = rigpv::frameCanvas(s, fc, fm.clip, 0, 0, px, py);
        const int gx = x0 + fc * cell + cell / 2, gy = 40 + cell + row * cell - 6;
        if (f.w <= 0) { b.text(x0 + fc * cell, gy - 20, "-", rgba(160, 40, 40)); continue; }
        Canvas flat = f;
        if (!row) for (uint32_t& q : flat.px) if (q >> 24) q = rgba(30, 22, 42);
        b.put(flat, gx - px, gy - py);
      }
    }
    x0 += 4 * cell + 10;
  }
  save3(b, dir, "doll_silhouette");
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("doll_gallery <outDir> [--pool FILE] [--who hero|woman] [--look k=v,...] [--palette a3|look] [looks clips parts lineup onion death sleep stats "
                "part:<name>[:<facing>[:<key>]] frame:<part>:<facing>:<clip>:<i>[:<variant>]]\n");
    return 1;
  }
  const std::string dir = argv[1];
  std::vector<std::string> boards;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--pool") && i + 1 < argc) {
      std::string err;
      if (!doll::loadPoolFile(argv[++i], &err)) { std::printf("doll_gallery: --pool: %s\n", err.c_str()); return 1; }
      continue;
    }
    if (!std::strcmp(argv[i], "--who") && i + 1 < argc) { g_who = !std::strcmp(argv[++i], "woman") ? 1 : 0; continue; }
    if (!std::strcmp(argv[i], "--look") && i + 1 < argc) { g_lookSpec = argv[++i]; continue; }
    if (!std::strcmp(argv[i], "--palette") && i + 1 < argc) { g_lookPalette = !std::strcmp(argv[++i], "look"); continue; }
    if (!std::strcmp(argv[i], "--slice") && i + 1 < argc) {   // (P2c) a slice look: its fields and its palette
      const char* t = argv[++i];
      bool found = false;
      for (const SliceLook& sl : kSlice)
        if (!std::strcmp(sl.tag, t)) { g_who = sl.who; g_lookSpec = sl.spec; g_lookPalette = sl.spec[0] != 0; found = true; }
      if (!found) { std::printf("doll_gallery: --slice: unknown tag %s (hero guard knight archer knightm archerw)\n", t); return 1; }
      continue;
    }
    boards.push_back(argv[i]);
  }
  if (boards.empty()) boards = {"looks", "clips", "parts", "lineup", "onion"};
  art::setRigVersion(2);
  const doll::PoolStats ps = doll::poolStats();
  std::printf("doll_gallery: pool %d parts, %d clips (%s)\n", ps.parts, ps.clips, ps.embedded ? "embedded" : "loaded");
  for (const std::string& b : boards) {
    if (b == "looks") boardLooks(dir);
    else if (b == "clips") boardClips(dir);
    else if (b == "parts") boardParts(dir);
    else if (b == "lineup") boardLineup(dir);
    else if (b == "onion") boardOnion(dir);
    else if (b == "death") boardDeath(dir);
    else if (b == "sleep") boardSleep(dir);
    else if (b == "stats") boardStats(dir);
    else if (b == "ramps") boardRamps(dir);
    else if (b.rfind("part:", 0) == 0 || b.rfind("frame:", 0) == 0) boardZoom(dir, b);
    else if (b.rfind("pose:", 0) == 0) boardPose(dir, b);
    else if (b == "kit") boardKit(dir);
    else if (b == "actions") boardActions(dir);
    else if (b == "silhouette") boardSilhouette(dir);
    else if (b.rfind("grip:", 0) == 0) boardGrip(dir, b);
    else std::printf("doll_gallery: unknown board %s\n", b.c_str());
  }
  return 0;
}
