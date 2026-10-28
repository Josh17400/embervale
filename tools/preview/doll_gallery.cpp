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
//   (P2c LADDER-DOLL)
//     ladder[:<tag,tag,...>|:flavour|:rank|:top]   looks side by side (default the hero's road lt1..lt6), idle in four
//                 facings, on grass and on the darkest day ground, by day and by night: doll_ladder[_<set>]_1x.png and _3x.png
//     finish:<tag>[:<D|U|R|L>]   one living-finish cycle (2.2 s, 12 frames) of a look's drawn idle (else its idle): the glow
//                 plane pulsed additively, the edge spark on the grip-to-tip line, the motes; by day and by night:
//                 doll_finish_<tag>_strip_3x.png and doll_finish_<tag>_<day|night>_fNN.png (3x; owner_pack.py makes the GIFs)
//   (P2c forge) forge[:t5|:woman|:alloys|:metalread|:ladder|:<tag,...>]   each look's forge style (tools/doll/forge_looks.txt),
//                 idle D and R and the walk's passing frame, on grass, dark ground and in greyscale, with its seven picks
//   (forge round 2) alloys1x  silhouette:alloys  twins  world:<w>  shapes:<axis>  shapes:<people>: the silhouette boards
//                 (the board set at true 1x in colour, greyscale and black; the owner's round-1 twins before and now; a world's
//                 6 x 6 window with its nearest pairs; every value of an axis; a people's materials beside its alloys)
//   (alloys i1) alloys      every people's alloys on the T5 plate figure (idle D, R), named, with the family: alloys_8x.png
//     metals      bronze iron steel gilded obsidian emberforged starmetal Mythic on the same figure: metals_8x.png
//   --looks FILE (default tools/doll/ladder_looks.txt) --tag <tag>: a looks-file line (its macros, fields and palette)
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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include "rpg/art/art_holders2.h"
#include "rpg/art/art_fx.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_forge.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/gear_forge.h"
#include "rpg/sim/gear_ladder.h"
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
  // (P2c ladder) the one spec reader: the looks macros (hero kit npc wpn rarity tier) and every field
  std::string bad;
  bool pal = g_lookPalette;
  doll::applyLookSpec(L, g_lookSpec, &pal, nullptr, &bad);
  if (!bad.empty()) std::printf("doll_gallery: --look: unknown field(s) %s\n", bad.c_str());
  return L;
}
// (P2c ladder) a looks file's lines (tag who spec gaits)
std::string g_looksFile = "tools/doll/ladder_looks.txt";
struct FileLook { std::string tag; int who = 0; std::string spec; bool pal = false; };
std::vector<FileLook> readLooksFile() {
  std::vector<FileLook> v;
  FILE* f = std::fopen(g_looksFile.c_str(), "rb");
  if (!f) return v;
  char line[4096];
  while (std::fgets(line, sizeof line, f)) {
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
    char tag[128] = {}, who[16] = {}, spec[3000] = {};
    if (std::sscanf(line, "%127s %15s %2999s", tag, who, spec) < 2) continue;
    FileLook l;
    l.tag = tag; l.who = !std::strcmp(who, "woman") ? 1 : 0; l.spec = std::strcmp(spec, "-") ? spec : "";
    l.pal = l.spec.find("palette=look") != std::string::npos;
    v.push_back(l);
  }
  std::fclose(f);
  return v;
}
bool fileLookRecipe(const std::string& tag, doll::DollRecipe& r, art::HumanLook& L) {
  for (const FileLook& fl : readLooksFile()) {
    if (fl.tag != tag) continue;
    L = a3Look(fl.who);
    bool pal = false;
    doll::applyLookSpec(L, fl.spec, &pal);
    r = doll::resolve(L);
    if (!pal) r.ramps = doll::a3Palette(L.body == 1 ? 1 : 0);
    return true;
  }
  return false;
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
// ---------------------------------------------------------------- (P2c ladder) the ladder and the living finish
const uint32_t kDarkGround = rgba(36, 42, 30);   // the darkest day ground (#242A1E: test_doll_ladder.cpp C14)
void darkGround(Board& b, int y0, int y1) {
  for (int y = y0; y < y1 && y < b.c.h; y++)
    for (int x = 0; x < b.c.w; x++) {
      const uint32_t n = hash2(x / 2, y / 2) % 100;
      b.c.set(x, y, n < 20 ? rgba(30, 36, 26) : n > 90 ? rgba(44, 50, 36) : kDarkGround);
    }
}
// night: the frame darkened (the moonwash's floor) before the living light is added
uint32_t nightOf(uint32_t c) {
  const int r = c & 255, g = c >> 8 & 255, bl = c >> 16 & 255;
  return rgba((int)(r * 0.30f + 4), (int)(g * 0.32f + 6), (int)(bl * 0.40f + 14)) | (c & 0xFF000000u);
}
uint32_t addLight(uint32_t base, uint32_t light, float a) {
  auto ch = [&](int sh) { return std::min(255, (int)((base >> sh & 255) + (light >> sh & 255) * a)); };
  return rgba(ch(0), ch(8), ch(16)) | (base & 0xFF000000u);
}
void finishOnBoard(Board& b, const RigSheet& s, const art::HumanLook& L, size_t fi, const Canvas& f, int bx, int by, float t, float phase,
                   float k, bool strike, art::finish::Particles* ps, uint32_t seed, int fc, Clip clip, int variant, int i);
void boardLadder(const std::string& dir, const std::string& arg) {
  std::string set = arg.size() > 7 ? arg.substr(7) : "";
  std::string list = set;
  if (set.empty()) list = "lt1,lt2,lt3,lt4,lt5,lt6";
  else if (set == "flavour") list = "lt3,jade3,imp3,fjord3";
  else if (set == "rank") list = "nband,guard,nroyal,nking";
  else if (set == "top") list = "lt5,lt6,lt6m,lt6w";
  std::vector<std::string> tags;
  for (size_t a = 0; a <= list.size();) { size_t e = list.find(',', a); if (e == std::string::npos) e = list.size(); if (e > a) tags.push_back(list.substr(a, e - a)); a = e + 1; }
  const int colW = 4 * 34 + 20, rowH = 64;
  Board b((int)tags.size() * colW + 20, 4 * rowH + 30);
  darkGround(b, 18 + rowH * 1, 18 + rowH * 2);
  darkGround(b, 18 + rowH * 3, 18 + rowH * 4);
  b.text(4, 4, "LADDER " + list + ": IDLE D U R L; GRASS AND THE DARKEST DAY GROUND; DAY, THEN NIGHT");
  for (size_t t = 0; t < tags.size(); t++) {
    doll::DollRecipe r;
    art::HumanLook L;
    const int x0 = 14 + (int)t * colW;
    if (!fileLookRecipe(tags[t], r, L)) { b.text(x0, 30, tags[t] + ": NOT IN " + g_looksFile, rgba(232, 96, 60)); continue; }
    const RigSheet s = doll::compose(r, RigPart::Loco);
    b.text(x0, 12 + 8, tags[t] + (L.topTier() ? " T" + std::to_string(L.topTier()) : std::string()));
    for (int band = 0; band < 4; band++) {
      const int g0 = 18 + rowH * band + rowH - 8;
      for (int fc = 0; fc < 4; fc++) {
        int px, py;
        Canvas f = rigpv::frameCanvas(s, fc, Clip::Idle, 0, 0, px, py);
        if (f.w <= 0) continue;
        if (band >= 2) for (uint32_t& c : f.px) if (c >> 24) c = nightOf(c);
        shadow(b, x0 + 16 + fc * 34, g0, std::min(24, std::max(10, f.w - 4)));
        b.put(f, x0 + 16 + fc * 34 - px, g0 - py);
        // (polish i1) the living finish as the game draws it (night: the breath's peak; day: its mean; no particles)
        if (const art::FrameRect* fr = s.frame(fc, Clip::Idle, 0, 0)) {
          const art::finish::Pulse P = art::finish::pulseOf(L.topTier());
          finishOnBoard(b, s, L, (size_t)(fr - s.frames.data()), f, x0 + 16 + fc * 34 - px, g0 - py, band >= 2 ? P.period * 0.5f : P.period * 0.25f,
                        0, band >= 2 ? 1.0f : 0.92f, false, nullptr, 0, fc, Clip::Idle, 0, 0);
        }
      }
    }
  }
  const std::string name = "doll_ladder" + (set.empty() ? std::string() : "_" + set);
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
  savePng(b.c, dir + "/" + name + "_3x.png", 3);
}
void boardFinish(const std::string& dir, const std::string& arg) {
  // finish:<tag>[:<facing>]
  std::string rest = arg.substr(7), tag = rest;
  int facing = 0;
  const size_t c = rest.find(':');
  if (c != std::string::npos) { tag = rest.substr(0, c); const char f = rest[c + 1]; facing = f == 'U' ? 1 : f == 'R' ? 2 : f == 'L' ? 3 : 0; }
  doll::DollRecipe r;
  art::HumanLook L;
  if (!fileLookRecipe(tag, r, L)) { std::printf("doll_gallery: finish: %s not in %s\n", tag.c_str(), g_looksFile.c_str()); return; }
  const bool fights = L.weapon >= 1 && L.weapon <= 8 && !L.civil;
  const RigSheet s = doll::compose(r, fights ? RigPart::Drawn : RigPart::Loco);
  const Clip clip = Clip::Idle;
  const int variant = fights ? 1 : 0;
  const art::finish::Pulse P = art::finish::pulseOf(L.topTier());
  const uint32_t id = 7;
  const float phase = art::finish::actorPhase(id);
  // the spark's sprite (role fx glint) in the look's glow ramp
  int glintPart = -1;
  for (int p = 0; p < doll::partCount(); p++) if (doll::partHasRole(p, doll::Role::Fx, "glint")) { glintPart = p; break; }
  RigSheet star[3];
  for (int k = 0; k < 3 && glintPart >= 0; k++) star[k] = doll::composePart(r, doll::partName(glintPart), facing, k == 0 ? "a1" : k == 1 ? "a2" : "a3");
  const int frames = 12, W = 64, H = 64;
  Board strip(frames * W, 2 * H + 12);
  plain(strip, rgba(86, 140, 62));
  darkGround(strip, H + 6, 2 * H + 12);
  for (int night = 0; night < 2; night++)
    for (int k = 0; k < frames; k++) {
      const float t = 2.2f * k / frames;
      const int n = s.count(facing, clip, variant);
      if (n <= 0) continue;
      const int i = (int)(t / 0.55f) % n;   // the idle's breath frames (0.55 s each)
      int px, py;
      Canvas f = rigpv::frameCanvas(s, facing, clip, variant, i, px, py);
      if (f.w <= 0) continue;
      Canvas big(W, H);
      const int ox = W / 2 - px, oy = H - 8 - py;
      for (int y = 0; y < f.h; y++) for (int x = 0; x < f.w; x++) if (f.get(x, y) >> 24) big.set(ox + x, oy + y, night ? nightOf(f.get(x, y)) : f.get(x, y));
      const art::FrameRect* fr = s.frame(facing, clip, variant, i);
      // (ladder i3) the game's light model (art::finish::frameLights): breath and crawl, the halo, motes or embers
      if (fr && !s.glow.empty()) {
        const size_t fi = (size_t)(fr - s.frames.data());
        std::vector<art::finish::GlowPx> gpx;
        if (fi < s.glow.rects.size() && s.glow.rects[fi].w > 0) s.glow.each(fi, [&](int gx, int gy, uint32_t lc) { gpx.push_back({gx, gy, lc}); });
        std::vector<art::finish::Light> lights;
        // (polish i1) the game's figure model: glow, halo, the T6 aura or the Mythic's flame (the mask), the edge sweep
        const art::finish::Mask mk = art::finish::maskOf(s.atlas, fr->x, fr->y, fr->w, fr->h);
        art::finish::Figure F;
        F.topTier = L.topTier(); F.mythic = L.mythic != 0; F.t = t + 1.0f; F.phase = phase; F.glow = &gpx; F.mask = &mk;
        F.bladeT6 = L.tier[art::HumanLook::TIER_WEAPON] >= 6;
        art::finish::figureLights(F, lights);
        for (const art::finish::Light& q : lights)
          for (int yy = 0; yy < (int)q.h; yy++)
            for (int xx = 0; xx < (int)q.w; xx++) {
              const int X = ox + (int)q.x + xx, Y = oy + (int)q.y + yy;
              if (X < 0 || Y < 0 || X >= W || Y >= H) continue;
              const uint32_t under = (big.get(X, Y) >> 24) ? big.get(X, Y) : (night ? kDarkGround : rgba(86, 140, 62));
              big.set(X, Y, addLight(under, q.c, q.a));
            }
      }
      // (ladder i3) the gleam (T6): the board forces one sweep across the cycle's frames 5-8 so it shows
      if (L.topTier() >= 6 && k >= 5 && k <= 8) {
        const float u = (k - 5 + 0.5f) / 4.0f;
        for (int y = 0; y < f.h; y++)
          for (int c = art::finish::gleamCol(u, f.w, f.h, y & ~1), x = c; x < c + art::finish::kGleamW; x++) {
            if (x < 0 || x >= f.w || !(f.get(x, y) >> 24)) continue;
            const int X = ox + x, Y = oy + y;
            if (X < 0 || Y < 0 || X >= W || Y >= H) continue;
            uint32_t src = f.get(x, y);
            if (L.mythic) src = rgba((int)((src & 255)), (int)((src >> 8 & 255) * 0.62f), (int)((src >> 16 & 255) * 0.3f));
            big.set(X, Y, addLight(big.get(X, Y), src, art::finish::kGleamA));
          }
      }
      // the edge spark on the grip-to-tip line (a T6 piece in hand with a tip mark)
      const art::RigAnchors* an = s.anchorsOf(facing, clip, variant, i);
      if (L.tier[art::HumanLook::TIER_WEAPON] >= 6 && an && an->tipX != art::kNoAnchor && an->gripX != art::kNoAnchor && glintPart >= 0) {
        // (the strip forces one run across the cycle so the board shows it)
        const float u = (float)k / (frames - 1);
        const int fk = art::finish::sparkFrame(u);
        if (fk >= 0 && !star[fk].empty()) {
          int sx, sy;
          const Canvas sc = rigpv::frameCanvas(star[fk], facing, Clip::Idle, 0, 0, sx, sy);
          const int cx = ox + px + an->gripX + (int)std::lround((an->tipX - an->gripX) * u), cy = oy + py + an->gripY + (int)std::lround((an->tipY - an->gripY) * u);
          for (int y = 0; y < sc.h; y++) for (int x = 0; x < sc.w; x++) if (sc.get(x, y) >> 24) big.set(cx - sx + x, cy - sy + y, sc.get(x, y));
        }
      }
      // the frame on the strip and as its own 3x file
      for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (big.get(x, y) >> 24) strip.c.set(k * W + x, night * (H + 6) + y, big.get(x, y));
      Board one(W, H);
      if (night) darkGround(one, 0, H); else plain(one, rgba(86, 140, 62));
      for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (big.get(x, y) >> 24) one.c.set(x, y, big.get(x, y));
      char nm[64];
      std::snprintf(nm, sizeof nm, "/doll_finish_%s_%s_f%02d.png", tag.c_str(), night ? "night" : "day", k);
      savePng(one.c, dir + nm, 3);
    }
  savePng(strip.c, dir + "/doll_finish_" + tag + "_strip_3x.png", 3);
}
// (polish i1) one figure-frame's living finish on a board, as the game draws it (art::finish::figureLights, then the
// gleam): frame f drawn with its pivot at (bx + px, by + py) (bx, by: the frame's top-left on the board). `k` scales the
// light (the game's day 0.85-1.0, night 1.0). Lights fall on the board whatever is under them (the halo, the aura).
void finishOnBoard(Board& b, const RigSheet& s, const art::HumanLook& L, size_t fi, const Canvas& f, int bx, int by, float t, float phase,
                   float k, bool strike, art::finish::Particles* ps, uint32_t seed, int fc, Clip clip, int variant, int i) {
  if (L.topTier() < 5 || fi >= s.frames.size()) return;
  const art::FrameRect& fr = s.frames[fi];
  std::vector<art::finish::GlowPx> gpx;
  if (fi < s.glow.first.size()) s.glow.each(fi, [&](int gx, int gy, uint32_t c) { gpx.push_back({gx, gy, c}); });
  art::finish::Mask mask = art::finish::maskOf(s.atlas, fr.x, fr.y, fr.w, fr.h);
  art::finish::Figure F;
  F.topTier = L.topTier(); F.mythic = L.mythic != 0; F.t = t; F.phase = phase; F.glow = &gpx; F.mask = &mask; F.strike = strike;
  F.bladeT6 = L.tier[art::HumanLook::TIER_WEAPON] >= 6;
  const art::RigAnchors* an = s.anchorsOf(fc, clip, variant, i);
  const bool gripOn = an && an->gripX != art::kNoAnchor, handOn = an && an->handRX != art::kNoAnchor;
  if (an && (gripOn || handOn) && an->tipX != art::kNoAnchor) {
    F.line = true;
    F.gx = (gripOn ? an->gripX : an->handRX + 1.0f) + fr.px; F.gy = (gripOn ? an->gripY : an->handRY + 1.0f) + fr.py;
    F.tx = an->tipX + fr.px; F.ty = an->tipY + fr.py;
  }
  std::vector<art::finish::Light> lights;
  art::finish::figureLights(F, lights);
  auto lay = [&](const art::finish::Light& q, int ox, int oy) {
    for (int yy = 0; yy < (int)q.h; yy++)
      for (int xx = 0; xx < (int)q.w; xx++) {
        const int X = ox + (int)q.x + xx, Y = oy + (int)q.y + yy;
        if (X >= 0 && Y >= 0 && X < b.c.w && Y < b.c.h) b.c.set(X, Y, addLight(b.c.get(X, Y), q.c, q.a * k));
      }
  };
  for (const art::finish::Light& q : lights) lay(q, bx, by);
  if (F.topTier < 6) return;
  if (ps) {
    uint32_t hot = 0;
    int hotL = -1;
    for (const auto& g : gpx) { const int l = (int)(g.c & 255) * 3 + (int)(g.c >> 8 & 255) * 6 + (int)(g.c >> 16 & 255); if (l > hotL) { hotL = l; hot = g.c; } }
    art::finish::particlesStep(*ps, seed, t, F.mythic, (float)bx, (float)by, gpx, F.line && strike, bx + F.tx, by + F.ty);
    std::vector<art::finish::Light> pl;
    art::finish::particleLights(*ps, t, hot, pl);
    for (const art::finish::Light& q : pl) lay(q, 0, 0);
  }
  const float u = art::finish::gleamAt(t, phase);
  if (u >= 0)
    for (int y = 0; y < f.h; y++)
      for (int c = art::finish::gleamCol(u, f.w, f.h, y & ~1), x = c; x < c + art::finish::kGleamW; x++) {
        if (x < 0 || x >= f.w || !(f.get(x, y) >> 24)) continue;
        const int X = bx + x, Y = by + y;
        if (X < 0 || Y < 0 || X >= b.c.w || Y >= b.c.h) continue;
        uint32_t src = f.get(x, y);
        if (L.mythic) src = rgba((int)((src & 255)), (int)((src >> 8 & 255) * 0.62f), (int)((src >> 16 & 255) * 0.3f));
        b.c.set(X, Y, addLight(b.c.get(X, Y), src, art::finish::kGleamA * k));
      }
}
// motion:<tag>[:<facing>[:<night>]]: the figure on grass at game speed, 30 frames a second (each saved 3x), as the game
// plays it: idle 3.3 s (the drawn idle when armed), a walk 3.6 s at 40 px/s, a run 2.4 s at 76 px/s (the RUN part, the
// body's run table), two blows (windup 0.12 s, strike 0.10 s,
// follow 0.20 s, 0.35 s apart), a walk 2.4 s. The living finish is the game's (finishOnBoard). Files motion_<tag>_<F>_f####.png
void boardMotion(const std::string& dir, const std::string& arg) {
  const std::vector<std::string> a = split(arg);   // motion <tag> [facing] [night]
  if (a.size() < 2) { std::printf("doll_gallery: motion:<tag>[:<facing>[:night]]\n"); return; }
  const std::string tag = a[1];
  const int fc = a.size() >= 3 ? facingOf(a[2]) : 2;
  const bool night = a.size() >= 4 && a[3] == "night";
  doll::DollRecipe r;
  art::HumanLook L;
  if (!fileLookRecipe(tag, r, L)) { std::printf("doll_gallery: motion: %s not in %s\n", tag.c_str(), g_looksFile.c_str()); return; }
  const bool fights = L.weapon >= 1 && L.weapon <= 8 && !L.civil;
  const RigSheet loco = doll::compose(r, RigPart::Loco);
  const RigSheet drawn = fights ? doll::compose(r, RigPart::Drawn) : RigSheet();
  const RigSheet act = fights ? doll::compose(r, RigPart::Action) : RigSheet();
  const doll::WalkTable wt = doll::walkTable(L, doll::Gait::Walk, fc);
  const RigSheet run = doll::compose(r, RigPart::Run);
  const doll::WalkTable rt = doll::walkTable(L, doll::Gait::Run, fc);
  const bool side = fc >= 2;
  const int W = side ? 240 : 120, H = side ? 80 : 150;
  const float speed = 40.0f, fps = 30.0f;
  const float tIdle = 3.3f, tWalk1 = 3.6f, tRun = 2.4f, runSpeed = 76.0f, tBlow = 0.42f, tGap = 0.35f, tWalk2 = 2.4f;
  const float total = tIdle + tWalk1 + tRun + 2 * tBlow + tGap + tWalk2;
  const uint32_t id = 7;
  const float phase = art::finish::actorPhase(id);
  art::finish::Particles ps;
  Board ground(W, H);
  if (night) for (uint32_t& c : ground.c.px) c = nightOf(c);
  const int frames = (int)(total * fps);
  const char fl = "DURL"[fc];
  float travel = 0, runTravel = 0;
  for (int n = 0; n < frames; n++) {
    const float t = n / fps;
    const RigSheet* s = &loco;
    Clip clip = Clip::Idle;
    int variant = 0, i = 0;
    bool strike = false, walking = false, running = false;
    if (t < tIdle) {
      if (fights && drawn.count(fc, Clip::Idle, 1) > 0) { s = &drawn; variant = 1; }
      const int cnt = std::max(1, s->count(fc, Clip::Idle, variant));
      i = (int)(t / 0.55f) % cnt;
    } else if (t < tIdle + tWalk1 || t >= total - tWalk2) {
      clip = Clip::Walk; walking = true;
      const int cnt = std::max(1, s->count(fc, Clip::Walk, 0));
      i = wt.frames ? std::min(wt.frameAt(travel), cnt - 1) : (int)(t / 0.13f) % cnt;
    } else if (t < tIdle + tWalk1 + tRun && run.count(fc, Clip::Walk, 0) > 0) {
      s = &run; clip = Clip::Walk; running = true;
      const int cnt = std::max(1, s->count(fc, Clip::Walk, 0));
      i = rt.frames ? std::min(rt.frameAt(runTravel), cnt - 1) : (int)(t / 0.09f) % cnt;
    } else if (t < tIdle + tWalk1 + tRun) {
      // no run frames: hold the drawn idle
      if (fights && drawn.count(fc, Clip::Idle, 1) > 0) { s = &drawn; variant = 1; }
    } else {
      const float u = t - tIdle - tWalk1 - tRun;
      const float v = u < tBlow ? u : u < tBlow + tGap ? -1.0f : u - tBlow - tGap;
      if (v < 0 || !fights) {
        if (fights && drawn.count(fc, Clip::Idle, 1) > 0) { s = &drawn; variant = 1; }
      } else {
        s = &act;
        clip = v < 0.12f ? Clip::Windup : v < 0.22f ? Clip::Strike : Clip::Follow;
        strike = clip == Clip::Strike;
        if (s->count(fc, clip, 0) <= 0) { s = &drawn; clip = Clip::Idle; variant = 1; strike = false; }
      }
    }
    if (walking) travel += speed / fps;
    if (running) { travel += runSpeed / fps; runTravel += runSpeed / fps; }
    const art::FrameRect* fr = s->frame(fc, clip, variant, i);
    Board b = ground;
    int px = 0, py = 0;
    const Canvas f = rigpv::frameCanvas(*s, fc, clip, variant, i, px, py);
    // the walker's place: side views cross the board (left to right, or right to left facing L), D / U walk down / up
    const float d = std::fmod(travel, side ? (float)(W - 60) : (float)(H - 70));
    int cx = W / 2, cy = H - 10;
    if (fc == 2) cx = 30 + (int)d;
    else if (fc == 3) cx = W - 30 - (int)d;
    else if (fc == 0) cy = 50 + (int)d;
    else cy = H - 10 - (int)d;
    if (f.w > 0 && fr) {
      Canvas g = f;
      if (night) for (uint32_t& c : g.px) if (c >> 24) c = nightOf(c);
      shadow(b, cx, cy, std::min(24, std::max(10, f.w - 4)));
      b.put(g, cx - px, cy - py);
      finishOnBoard(b, *s, L, (size_t)(fr - s->frames.data()), f, cx - px, cy - py, t + 1.0f, phase, night ? 1.0f : 0.92f, strike, &ps,
                    id * 2654435761u + 0x5EEDu, fc, clip, variant, i);
    }
    char nm[96];
    std::snprintf(nm, sizeof nm, "/motion_%s_%c%s_f%04d.png", tag.c_str(), fl, night ? "n" : "", n);
    savePng(b.c, dir + nm, 3);
    // the follow crop: 80 x 72 round the figure (the game's near view of one person), 3x
    Canvas crop(80, 72);
    for (int y = 0; y < crop.h; y++)
      for (int x = 0; x < crop.w; x++) {
        const int X = cx - 40 + x, Y = cy - 56 + y;
        crop.set(x, y, X >= 0 && Y >= 0 && X < b.c.w && Y < b.c.h ? b.c.get(X, Y) : rgba(20, 20, 24));
      }
    std::snprintf(nm, sizeof nm, "/motion_%s_%c%s_c%04d.png", tag.c_str(), fl, night ? "n" : "", n);
    savePng(crop, dir + nm, 3);
  }
  std::printf("doll_gallery: motion %s %c: %d frames at %.0f fps\n", tag.c_str(), fl, frames, fps);
}
// lit:<requests file>: still frames with the game's living finish, for the owner boards. Each line of the file:
//   <tag> <D|U|R|L> <loco|action|death|run|drawn> <clip> <variant> <i> <t> <day|night> <strike 0|1> <out base path>
// writes <out>_fig.png (the 1x frame on magenta, 96 x 96, feet at 48,84) and <out>_lit.png (the frame on black with the
// finish added the game's way, particles run for 1 s before t so motes and embers are in the air)
void boardLit(const std::string& dir, const std::string& arg) {
  (void)dir;
  const std::string file = arg.substr(4);
  std::FILE* fp = std::fopen(file.c_str(), "r");
  if (!fp) { std::printf("doll_gallery: lit: cannot open %s\n", file.c_str()); return; }
  char line[1024];
  int done = 0;
  std::string lastTag;
  doll::DollRecipe r;
  art::HumanLook L;
  RigSheet sheets[(int)RigPart::COUNT];
  bool have[(int)RigPart::COUNT] = {};
  while (std::fgets(line, sizeof line, fp)) {
    char tag[128], fac[8], part[16], clip[16], dn[8], out[600];
    int variant = 0, i = 0, strike = 0;
    float t = 0;
    if (line[0] == '#' || std::sscanf(line, "%127s %7s %15s %15s %d %d %f %7s %d %599s", tag, fac, part, clip, &variant, &i, &t, dn, &strike, out) < 10) continue;
    if (lastTag != tag) {
      if (!fileLookRecipe(tag, r, L)) { std::printf("doll_gallery: lit: %s not in %s\n", tag, g_looksFile.c_str()); continue; }
      lastTag = tag;
      for (bool& h : have) h = false;
    }
    const std::string ps = part;
    const RigPart rp = ps == "action" ? RigPart::Action : ps == "death" ? RigPart::Death : ps == "run" ? RigPart::Run : ps == "drawn" ? RigPart::Drawn : RigPart::Loco;
    if (!have[(int)rp]) { sheets[(int)rp] = doll::compose(r, rp); have[(int)rp] = true; }
    const RigSheet& s = sheets[(int)rp];
    static const char* kClips[] = {"idle", "walk", "hurt", "windup", "strike", "follow", "cast", "draw", "release", "block", "roll", "fall", "lying"};
    Clip c = Clip::Idle;
    for (int k = 0; k < (int)Clip::COUNT; k++) if (!std::strcmp(clip, kClips[k])) c = (Clip)k;
    const int fc = facingOf(fac);
    const bool night = !std::strcmp(dn, "night");
    int px = 0, py = 0;
    Canvas f = rigpv::frameCanvas(s, fc, c, variant, i, px, py);
    const art::FrameRect* fr = s.frame(fc, c, variant, i);
    if (f.w <= 0 || !fr) { std::printf("doll_gallery: lit: %s %s %s %s %d %d: no frame\n", tag, fac, part, clip, variant, i); continue; }
    if (night) for (uint32_t& q : f.px) if (q >> 24) q = nightOf(q);
    const int W = 96, H = 96, ox = 48 - px, oy = 84 - py;
    Canvas fig(W, H);
    for (uint32_t& q : fig.px) q = rgba(255, 0, 255);   // the key (savePng writes opaque)
    for (int y = 0; y < f.h; y++) for (int x = 0; x < f.w; x++) if (f.get(x, y) >> 24) { const int X = ox + x, Y = oy + y; if (X >= 0 && Y >= 0 && X < W && Y < H) fig.set(X, Y, f.get(x, y)); }
    Board b(W, H);
    plain(b, rgba(0, 0, 0));
    b.put(f, ox, oy);
    const uint32_t id = 7;
    const float phase = art::finish::actorPhase(id);
    const size_t fi = (size_t)(fr - s.frames.data());
    art::finish::Particles parts;
    Board scratch(W, H);
    for (int n = 30; n >= 1; n--)   // particles: the second before t, the figure standing in this frame
      finishOnBoard(scratch, s, L, fi, f, ox, oy, t - n / 30.0f, phase, 1.0f, strike != 0, &parts, id * 2654435761u + 0x5EEDu, fc, c, variant, i);
    finishOnBoard(b, s, L, fi, f, ox, oy, t, phase, night ? 1.0f : 0.92f, strike != 0, &parts, id * 2654435761u + 0x5EEDu, fc, c, variant, i);
    savePng(fig, std::string(out) + "_fig.png", 1);
    savePng(b.c, std::string(out) + "_lit.png", 1);
    done++;
  }
  std::fclose(fp);
  std::printf("doll_gallery: lit: %d frames\n", done);
}
// film:<requests file>: a figure moving through the world with the game's living finish, frame by frame, for the owner videos.
// Each line: <tag> <D|U|R|L> <part> <clip> <variant> <i> <t> <wx> <wy> <day|night> <strike 0|1> <out base path> (wx, wy: the
// feet in world px, 0..4000 x 0..200). The particles live in world space across the lines of one tag (motes and embers stay
// where they were born, so a walker trails them). Writes <out>_fig.png and <out>_lit.png: the 160 x 112 window round the feet
// (feet at 80,88), the frame alone on magenta and the frame on black with the finish added.
void boardFilm(const std::string& dir, const std::string& arg) {
  (void)dir;
  const std::string file = arg.substr(5);
  std::FILE* fp = std::fopen(file.c_str(), "r");
  if (!fp) { std::printf("doll_gallery: film: cannot open %s\n", file.c_str()); return; }
  char line[1024];
  int done = 0;
  std::string lastTag;
  doll::DollRecipe r;
  art::HumanLook L;
  RigSheet sheets[(int)RigPart::COUNT];
  bool have[(int)RigPart::COUNT] = {};
  art::finish::Particles parts;
  const int BW = 4200, BH = 240, WW = 160, WH = 112;
  Board world(BW, BH);
  while (std::fgets(line, sizeof line, fp)) {
    char tag[128], fac[8], part[16], clip[16], dn[8], out[600];
    int variant = 0, i = 0, strike = 0;
    float t = 0, wx = 0, wy = 0;
    if (line[0] == '#' || std::sscanf(line, "%127s %7s %15s %15s %d %d %f %f %f %7s %d %599s", tag, fac, part, clip, &variant, &i, &t, &wx, &wy, dn, &strike, out) < 12) continue;
    if (lastTag != tag) {
      if (!fileLookRecipe(tag, r, L)) { std::printf("doll_gallery: film: %s not in %s\n", tag, g_looksFile.c_str()); continue; }
      lastTag = tag;
      for (bool& h : have) h = false;
      parts = art::finish::Particles();
    }
    const std::string ps = part;
    const RigPart rp = ps == "action" ? RigPart::Action : ps == "death" ? RigPart::Death : ps == "run" ? RigPart::Run : ps == "drawn" ? RigPart::Drawn : RigPart::Loco;
    if (!have[(int)rp]) { sheets[(int)rp] = doll::compose(r, rp); have[(int)rp] = true; }
    const RigSheet& s = sheets[(int)rp];
    static const char* kClips[] = {"idle", "walk", "hurt", "windup", "strike", "follow", "cast", "draw", "release", "block", "roll", "fall", "lying"};
    Clip c = Clip::Idle;
    for (int k = 0; k < (int)Clip::COUNT; k++) if (!std::strcmp(clip, kClips[k])) c = (Clip)k;
    const int fc = facingOf(fac);
    const bool night = !std::strcmp(dn, "night");
    int px = 0, py = 0;
    Canvas f = rigpv::frameCanvas(s, fc, c, variant, i, px, py);
    const art::FrameRect* fr = s.frame(fc, c, variant, i);
    if (f.w <= 0 || !fr) { std::printf("doll_gallery: film: %s %s %s %s %d %d: no frame\n", tag, fac, part, clip, variant, i); continue; }
    if (night) for (uint32_t& q : f.px) if (q >> 24) q = nightOf(q);
    const int fx = (int)std::floor(wx + 0.5f), fy = (int)std::floor(wy + 0.5f);
    const int x0 = fx - 80, y0 = fy - 88;
    plain(world, rgba(0, 0, 0));
    world.put(f, fx - px, fy - py);
    const uint32_t id = 7;
    finishOnBoard(world, s, L, (size_t)(fr - s.frames.data()), f, fx - px, fy - py, t, art::finish::actorPhase(id), night ? 1.0f : 0.92f,
                  strike != 0, &parts, id * 2654435761u + 0x5EEDu, fc, c, variant, i);
    Canvas lit(WW, WH), fig(WW, WH);
    for (uint32_t& q : fig.px) q = rgba(255, 0, 255);   // the key (savePng writes opaque)
    for (int y = 0; y < WH; y++)
      for (int x = 0; x < WW; x++) {
        const int X = x0 + x, Y = y0 + y;
        lit.set(x, y, X >= 0 && Y >= 0 && X < BW && Y < BH ? (world.c.get(X, Y) | 0xFF000000u) : rgba(0, 0, 0));
        const int u = X - (fx - px), v = Y - (fy - py);
        if (u >= 0 && v >= 0 && u < f.w && v < f.h && (f.get(u, v) >> 24)) fig.set(x, y, f.get(u, v));
      }
    savePng(fig, std::string(out) + "_fig.png", 1);
    savePng(lit, std::string(out) + "_lit.png", 1);
    done++;
  }
  std::fclose(fp);
  std::printf("doll_gallery: film: %d frames\n", done);
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

// ---------------------------------------------------------------- (alloys i1) the metals
// alloys: every people's alloys (a culture with three: the everyday, the warriors' and the masterwork) on the one T5 plate
// figure (the Heartland hero's kit, its metal swapped), idle D and R, labelled with the alloy's in-game name and its family;
// metals: the band metals and Mythic on the same figure. Printed: every alloy of seeds 1..64 with its keys and family.
const char* const kPeopleName[12] = {"FJORDFOLK", "HIGHLAND", "HEARTLAND", "IMPERIAL", "DUNE", "STEPPE", "MARSH", "JADE", "RIVER", "SUNTEMPLE",
                                     "SYLVAN", "STARSPIRE"};
art::HumanLook plateFigure() {
  art::HumanLook L = a3Look(0);
  doll::applyLookSpec(L, "hero=1,kit=heartland:5:7,palette=look");
  // (alloys i2: the blind panel saw "the fixed red tabard, red plume and gold shield cover most of the pixels", so every
  // warm metal ran into the costume) the board's figure wears undyed cloth: a charcoal tabard and cloak, a cream plume and
  // a charcoal shield field, so the metal is what the eye compares (the device is drawn in the metal's own light)
  L.tabardColor = hexc(0x3A3742); L.cloakColor = hexc(0x2A2832); L.plumeColor = hexc(0xE6E0D2);
  L.shieldField = hexc(0x34313C); L.patternColor = 0; L.pattern = 0;
  return L;
}
void metalPair(Board& b, const art::HumanLook& L, int x, int g0) {
  doll::DollRecipe r = doll::resolve(L);
  r.ramps = doll::rampsOf(L);
  const RigSheet s = doll::compose(r, RigPart::Loco);
  for (int k = 0; k < 2; k++) {
    const int fc = k == 0 ? 0 : 2;
    int px, py;
    Canvas f = rigpv::frameCanvas(s, fc, Clip::Idle, 0, 0, px, py);
    if (f.w <= 0) continue;
    shadow(b, x + 16 + k * 34, g0, std::min(24, std::max(10, f.w - 4)));
    b.put(f, x + 16 + k * 34 - px, g0 - py);
  }
  // the metal ramp under the pair (the five steps)
  for (int i = 0; i < 5; i++)
    for (int yy = 0; yy < 3; yy++)
      for (int xx = 0; xx < 6; xx++) b.c.set(x + 10 + i * 6 + xx, g0 + 3 + yy, r.ramps.c[(int)doll::Slot::Metal][i]);
}
void boardAlloys(const std::string& dir) {
  // the census of alloys (seeds 1..64), and the board's culture per people: the first seed with the most alloys
  int pick[12] = {};
  int census[12][3][6] = {};
  for (int a = 0; a < 12; a++) {
    size_t most = 0;
    for (uint32_t sd = 1; sd <= 64; sd++) {
      const cult::Culture c = cult::Atlas::make((cult::Archetype)a, sd);
      const std::vector<uint8_t> fam = gear::alloyFamilies(c);
      for (size_t i = 0; i < c.arms.alloys.size(); i++) {
        const gear::AlloyKeys k = gear::alloyKeys(c, (int)i);
        if (sd <= 4)
          std::printf("alloy %-9s s%-2u %zu %-12s sheen %d light #%06X family %s\n", kPeopleName[a], sd, i + 1, c.arms.alloys[i].name.c_str(), k.sheen,
                      (k.light & 0xFF) << 16 | (k.light & 0xFF00) | (k.light >> 16 & 0xFF), doll::alloyFamilyName(fam[i]));
      }
      if (c.arms.alloys.size() > most) { most = c.arms.alloys.size(); pick[a] = (int)sd; }
      for (size_t i = 0; i < c.arms.alloys.size() && i < 3; i++) {
        const std::string& nm = c.arms.alloys[i].name;
        const char* ws[6] = {"BRONZE", "BRASS", "IRON", "STEEL", "SILVER", "GLASS"};
        for (int w = 0; w < 6; w++)
          if (nm.size() >= std::strlen(ws[w]) && nm.compare(nm.size() - std::strlen(ws[w]), std::string::npos, ws[w]) == 0) { census[a][i][w]++; break; }
      }
    }
    std::printf("census %-9s", kPeopleName[a]);
    for (int i = 0; i < 3; i++) std::printf(" | r%d br%2d bs%2d ir%2d st%2d si%2d gl%2d", i, census[a][i][0], census[a][i][1], census[a][i][2], census[a][i][3], census[a][i][4], census[a][i][5]);
    std::printf("\n");
  }
  {   // which peoples meet in the atlas (gate C15c's cells: worlds 1..6, a 6x6 block, right and down)
    int nb[12][12] = {};
    for (uint64_t w = 1; w <= 6; w++) {
      cult::Atlas at(w);
      for (int ci = 0; ci < 6; ci++)
        for (int cj = 0; cj < 6; cj++)
          for (int dir = 0; dir < 2; dir++) {
            const int x = (int)at.family(ci, cj).archetype, y = (int)at.family(ci + (dir == 0), cj + (dir == 1)).archetype;
            if (x != y) nb[std::min(x, y)][std::max(x, y)]++;
          }
    }
    for (int x = 0; x < 12; x++)
      for (int y = x + 1; y < 12; y++)
        if (nb[x][y]) std::printf("meet %d %d %d\n", x, y, nb[x][y]);
  }
  const art::HumanLook base = plateFigure();
  const int cellW = 2 * 34 + 100, rowH = 74, lab = 78;   // (i3) wide enough for "TURQUOISE STEEL IRIDESCENT"
  Board b(lab + 3 * cellW + 8, 16 + 12 * rowH);
  b.text(4, 4, "ALLOYS ON THE T5 PLATE: NAME, METAL FAMILY, SHEEN, RAMP (IDLE D, R)");
  for (int a = 0; a < 12; a++) {
    const cult::Culture c = cult::Atlas::make((cult::Archetype)a, (uint32_t)pick[a]);
    const std::vector<uint8_t> fam = gear::alloyFamilies(c);
    const int y0 = 16 + a * rowH;
    if (a & 1) darkGround(b, y0, y0 + rowH);
    b.text(4, y0 + 26, kPeopleName[a]);
    b.text(4, y0 + 36, "SEED " + std::to_string(pick[a]));
    for (size_t i = 0; i < c.arms.alloys.size() && i < 3; i++) {
      const gear::AlloyKeys k = gear::alloyKeys(c, (int)i);
      art::HumanLook L = base;
      L.armourTint = k.light; L.armourTint2 = k.dark; L.sheen = k.sheen; L.metalKind = 10; L.bladeKind = 10; L.alloyFam = (uint8_t)(fam[i] + 1);
      { uint32_t rr[5]; doll::alloyRamp(fam[i], k.sheen, rr); L.shieldDevice = rr[3]; }
      const int x = lab + (int)i * cellW;
      b.text(x + 4, y0 + 2, c.arms.alloys[i].name);
      static const char* const kSh[9] = {"-", "MATTE", "BRIGHT", "DARK", "BANDED", "IRIDESCENT", "GLOWING", "PALE", "BURNISHED"};
      b.text(x + 4, y0 + 10, std::string(doll::alloyFamilyName(fam[i])) + " " + kSh[std::min<int>(k.sheen, 8)], rgba(200, 200, 200));
      metalPair(b, L, x, y0 + rowH - 12);
    }
  }
  savePng(b.c, dir + "/alloys_1x.png", 1);
  savePng(b.c, dir + "/alloys_3x.png", 3);
  savePng(b.c, dir + "/alloys_8x.png", 8);
}
void boardMetals(const std::string& dir) {
  struct M { const char* name; int kind; };
  const M ms[8] = {{"BRONZE", 2}, {"IRON", 3}, {"STEEL", 4}, {"GILDED", 5}, {"OBSIDIAN", 7}, {"EMBERFORGED", 8}, {"STARMETAL", 9}, {"MYTHIC", 11}};
  const art::HumanLook base = plateFigure();
  const int cellW = 2 * 34 + 26;
  Board b(4 * cellW + 16, 16 + 2 * 74);
  b.text(4, 4, "METALS: BAND METALS AND MYTHIC, T5 PLATE, IDLE D AND R");
  for (int i = 0; i < 8; i++) {
    art::HumanLook L = base;
    L.armourTint = 0; L.armourTint2 = 0; L.sheen = 0; L.alloyFam = 0;
    L.metalKind = (uint8_t)ms[i].kind; L.bladeKind = (uint8_t)ms[i].kind;
    { uint32_t rr[5]; doll::metalKindRamp(ms[i].kind, rr); L.shieldDevice = rr[3]; }
    if (ms[i].kind == 11) {
      art::HumanLook My = a3Look(0);
      doll::applyLookSpec(My, "hero=1,kit=heartland:6:7,rarity=mythic,palette=look");
      L = My;
    }
    const int x = 8 + (i % 4) * cellW, y0 = 16 + (i / 4) * 74;
    b.text(x + 4, y0 + 2, ms[i].name);
    metalPair(b, L, x, y0 + 62);
  }
  savePng(b.c, dir + "/metals_1x.png", 1);
  savePng(b.c, dir + "/metals_3x.png", 3);
  savePng(b.c, dir + "/metals_8x.png", 8);
}

// (P2c forge, FORGE-DOLL) the forge boards. A sheet of looks side by side, each column labelled with its tag, its style and
// its seven picks (the smith's sheet); idle D, idle R and the walk's passing frame facing R. mode 0: on grass, on the darkest
// day ground and in greyscale (the neutral-ramp compose on a grey ground: the shape with the colour removed); 1 (forgegrey):
// greyscale only; 2 (silhouette): the gear filled black on the body, no weapon. Writes <name>_1x.png, _3x.png and _8x.png
// when 8000 px or less.
//   forge[:t5|:woman|:alloys|:metalread|:ladder|:<tag,...>]   tools/doll/forge_looks.txt (default the ten materials fm_*)
//   forgegrey[:...]   silhouette:forge   alloystyles:<people>:<seed>   families
struct ForgeEntry { std::string label; int who = 0; std::string spec; };   // spec "": a looks-file tag
bool forgeEntryRecipe(const ForgeEntry& e, doll::DollRecipe& r, art::HumanLook& L) {
  if (e.spec.empty()) return fileLookRecipe(e.label, r, L);
  L = a3Look(e.who);
  bool pal = false;
  doll::applyLookSpec(L, e.spec, &pal);
  r = doll::resolve(L);
  if (!pal) r.ramps = doll::a3Palette(L.body == 1 ? 1 : 0);
  return true;
}
std::vector<ForgeEntry> forgeTags(const std::string& set) {
  std::string list = set;
  if (set.empty()) list = "fm_home,fm_tan,fm_bronze,fm_iron,fm_steel,fm_gilt,fm_obsid,fm_ember,fm_star,fm_myth";
  else if (set == "t5") list = "fm_steel,f5_steel,f5_gilt,f5_obsid,f5_ember,f5_star";
  else if (set == "woman") list = "fw_bronze,fw_steel,fw_star,fm_bronze,fm_steel,fm_star";
  else if (set == "alloys") list = "a_hl1,a_hl2,a_hl3,fm_iron,fm_steel,a_jade3,a_star3";
  else if (set == "metalread") list = "mr_1,mr_2,mr_3,mr_4,mr_5,mr_6";
  else if (set == "ladder") list = "lt1,lt2,lt3,lt4,lt5,lt6,lt6m";
  std::vector<ForgeEntry> v;
  for (size_t a = 0; a <= list.size();) {
    size_t e = list.find(',', a);
    if (e == std::string::npos) e = list.size();
    if (e > a) v.push_back(ForgeEntry{list.substr(a, e - a), 0, ""});
    a = e + 1;
  }
  return v;
}
// (envelope fix round, D63: the packager saw the grey boards keep the hair red) greyscale means greyscale: a grey board's
// rows turn every pixel into its luma (Rec. 601) after everything is drawn on them (the figures' hair, skin, eyes and face
// and the labels included), and the check counts any pixel there whose R, G and B differ (main exits 1 on any)
int g_greyBad = 0;
void greyRows(Canvas& c, int y0) {
  for (int y = std::max(0, y0); y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const uint32_t p = c.get(x, y);
      const int l = (int)(0.299 * (p & 255) + 0.587 * (p >> 8 & 255) + 0.114 * (p >> 16 & 255) + 0.5);
      c.set(x, y, (p & 0xFF000000u) | (uint32_t)l | (uint32_t)l << 8 | (uint32_t)l << 16);
    }
}
int greyCheck(const Canvas& c, int y0, const std::string& name) {
  int bad = 0;
  for (int y = std::max(0, y0); y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const uint32_t p = c.get(x, y);
      if ((p & 255) != (p >> 8 & 255) || (p & 255) != (p >> 16 & 255)) bad++;
    }
  if (bad) { std::printf("FAIL: grey board %s: %d px whose R, G and B differ (D63)\n", name.c_str(), bad); g_greyBad++; }
  else std::printf("ok    grey board %s: R = G = B on every pixel\n", name.c_str());
  return bad;
}
void forgeSheet(const std::string& dir, const std::string& name, const std::string& title, const std::vector<ForgeEntry>& es, int mode) {
  if (g_looksFile == "tools/doll/ladder_looks.txt") g_looksFile = "tools/doll/forge_looks.txt";
  namespace F = art::doll::forge;
  const int bands = mode == 0 ? 3 : 1;
  const int colW = 3 * 34 + 14, bandH = 60, top = 14 + 10 * 9;
  Board b((int)es.size() * colW + 12, top + bands * bandH + 8);
  if (mode == 0) darkGround(b, top + bandH, top + 2 * bandH);
  const int greyFrom = mode == 0 ? top + 2 * bandH : (mode == 1 ? top : 1 << 30);
  for (int y = 0; y < b.c.h; y++)
    for (int x = 0; x < b.c.w; x++) {
      const uint32_t p = b.c.get(x, y);
      if (mode == 2 && y >= top) { b.c.set(x, y, rgba(232, 228, 216)); continue; }   // the silhouettes on paper
      if (y < greyFrom) continue;
      const int l = (int)(0.299 * (p & 255) + 0.587 * (p >> 8 & 255) + 0.114 * (p >> 16 & 255));
      b.c.set(x, y, rgba(l, l, l));
    }
  b.text(4, 3, title);
  for (size_t t = 0; t < es.size(); t++) {
    doll::DollRecipe r;
    art::HumanLook L;
    const int x0 = 8 + (int)t * colW;
    if (!forgeEntryRecipe(es[t], r, L)) { b.text(x0, 16, es[t].label + "?", rgba(232, 96, 60)); continue; }
    uint8_t ax[F::AX_COUNT];
    F::unpack(L.forge[art::HumanLook::TIER_BODY], ax);
    std::string up = es[t].label;
    for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
    b.text(x0, 14, up.substr(0, 18));
    std::string base = L.forge[art::HumanLook::TIER_BODY] ? F::valueName(F::AX_BASE, ax[0]) : "no style";
    for (char& ch : base) ch = (char)std::toupper((unsigned char)ch);
    b.text(x0, 23, base + (L.topTier() ? " T" + std::to_string(L.topTier()) : std::string()), rgba(250, 214, 120));
    // (silhouettes i3: the panel read round 1's retired CT/ED/LI/FI columns, blank by design, as "the body never changes")
    // the six silhouette axes as applied (contracts 3c.1), then the shoulder's shape and the crest (the details)
    {
      uint8_t sx[F::SX_COUNT];
      F::unpackShapeAxes(L.forgeShape, sx);
      static const char* const kSx[F::SX_COUNT] = {"HELM", "SKIRT", "MASS", "LAYER", "ARM", "MOTIF"};
      int row = 1;
      for (int a = 0; a < F::SX_COUNT; a++, row++) {
        std::string w = L.forgeShape && sx[a] ? F::shapeName(a, sx[a]) : "-";
        for (char& ch : w) ch = (char)std::toupper((unsigned char)ch);
        b.text(x0, 23 + row * 9, std::string(kSx[a]) + " " + w, rgba(214, 226, 240));
      }
      for (int a : {(int)F::AX_SHOULDER, (int)F::AX_CREST}) {
        const int v = a == F::AX_CREST ? r.forgeAx[art::HumanLook::TIER_HELM][F::AX_CREST] : ax[a];
        std::string w = v ? F::valueName(a, v) : "-";
        for (char& ch : w) ch = (char)std::toupper((unsigned char)ch);
        b.text(x0, 23 + row++ * 9, std::string(a == F::AX_CREST ? "CREST " : "SHAPE ") + w, rgba(170, 182, 196));
      }
    }
    if (mode == 2) {   // the gear filled black (no weapon): the probe's owners
      for (int k = 0; k < 2; k++) {
        const int fc = k == 0 ? 0 : 2;
        const doll::FrameProbe p = doll::probeFrame(r, RigPart::Loco, fc, Clip::Idle, 0, 0, true);
        if (!p.ok) continue;
        const int g0 = top + bandH - 8;
        for (int y = 0; y < p.h; y++)
          for (int x = 0; x < p.w; x++) {
            const size_t q = (size_t)y * p.w + x;
            const int o = p.owner[q];
            if (p.code[q] < 2 || o < 0) continue;
            const auto& it = p.items[(size_t)o];
            if (it.has(doll::Role::Held)) continue;
            const bool gear = it.has(doll::Role::Armour) || it.has(doll::Role::Helm) || it.has(doll::Role::Pauldron) || it.has(doll::Role::Shield) ||
                              it.has(doll::Role::Overlay);
            b.c.set(x0 + 16 + k * 40 + x - p.px, g0 + y - p.py, gear ? rgba(16, 14, 20) : rgba(176, 172, 164));
          }
      }
      continue;
    }
    for (int band = 0; band < bands; band++) {
      doll::ComposeOpts op;
      op.grey = mode == 1 || band == 2;
      const RigSheet s = doll::compose(r, RigPart::Loco, &op);
      const int g0 = top + band * bandH + bandH - 8;
      const struct { int fc; Clip c; int i; } fr[3] = {{0, Clip::Idle, 0}, {2, Clip::Idle, 0}, {2, Clip::Walk, 1}};
      for (int k = 0; k < 3; k++) {
        int px, py;
        Canvas f = rigpv::frameCanvas(s, fr[k].fc, fr[k].c, 0, fr[k].i, px, py);
        if (f.w <= 0) continue;
        shadow(b, x0 + 16 + k * 34, g0, std::min(24, std::max(10, f.w - 4)));
        b.put(f, x0 + 16 + k * 34 - px, g0 - py);
      }
    }
  }
  if (greyFrom < b.c.h) { greyRows(b.c, greyFrom); greyCheck(b.c, greyFrom, name); }
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
  savePng(b.c, dir + "/" + name + "_3x.png", 3);
  if (std::max(b.c.w, b.c.h) * 8 <= 8000) savePng(b.c, dir + "/" + name + "_8x.png", 8);
}
void boardForge(const std::string& dir, const std::string& arg) {
  const size_t c = arg.find(':');
  const std::string kind = arg.substr(0, c), set = c == std::string::npos ? "" : arg.substr(c + 1);
  const std::string sfx = set.empty() ? std::string() : "_" + set;
  if (kind == "forge")
    forgeSheet(dir, "doll_forge" + sfx, "FORGE " + (set.empty() ? std::string("MATERIALS") : set) + ": IDLE D, IDLE R, WALK R; GRASS, DARK GROUND, GREYSCALE", forgeTags(set), 0);
  else if (kind == "forgegrey")
    forgeSheet(dir, "doll_forgegrey" + sfx, "FORGE " + (set.empty() ? std::string("MATERIALS") : set) + " IN GREYSCALE: THE SHAPE WITH THE COLOUR REMOVED", forgeTags(set), 1);
}
// silhouette:forge: the ten materials' gear filled black on the body (no weapon), idle D and R
void boardForgeSilhouette(const std::string& dir) {
  forgeSheet(dir, "doll_silhouette_forge", "SILHOUETTES: THE TEN STYLES' GEAR FILLED BLACK, IDLE D AND R (NO WEAPON)", forgeTags(""), 2);
}
// alloystyles:<people>:<seed>: the culture's alloys on its T4 kit, each beside its metal word's hero style (the same kit)
void boardAlloyStyles(const std::string& dir, const std::string& arg) {
  const size_t a = arg.find(':'), b2 = arg.find(':', a + 1);
  const std::string people = arg.substr(a + 1, b2 == std::string::npos ? std::string::npos : b2 - a - 1);
  const int seed = b2 == std::string::npos ? 1 : std::atoi(arg.c_str() + b2 + 1);
  cult::Archetype ar;
  if (!gear::peopleOf(people, ar)) { std::printf("doll_gallery: alloystyles: unknown people %s\n", people.c_str()); return; }
  const cult::Culture C = cult::Atlas::make(ar, (uint32_t)seed);
  std::vector<ForgeEntry> es;
  for (int k = 0; k < (int)C.arms.alloys.size(); k++) {
    const int w = gear::alloyMetalWord(C.arms.alloys[(size_t)k], std::min(k, 2));
    const char* hero = art::doll::forge::valueName(art::doll::forge::AX_BASE, gear::wordHeroBase(w));
    es.push_back(ForgeEntry{C.arms.alloys[(size_t)k].name, 0, "kit=" + people + ":4:" + std::to_string(seed) + ",alloy=" + std::to_string(k + 1) + ",palette=look"});
    es.push_back(ForgeEntry{std::string("its ") + hero, 0, "kit=" + people + ":4:" + std::to_string(seed) + ",alloy=" + std::to_string(k + 1) + ",forge=" + hero + ",palette=look"});
  }
  std::string up = people;
  for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
  forgeSheet(dir, "doll_alloystyles_" + people + "_" + std::to_string(seed), up + " SEED " + std::to_string(seed) + ": EACH ALLOY ON THE T4 KIT BESIDE ITS METAL WORD'S HERO STYLE", es, 0);
}
// (forge i3) specs:<label>=<spec>;<label>=<spec>...: any looks specs side by side in the forge sheet (design previews)
// ---------------------------------------------------------------- (forge round 2, FORGE-DOLL) the silhouette boards
// STYLE_P2 28.10's board set and kit, contracts 3c.1. Every alloy on its people's kit at its tier (T4; the masterwork T5;
// gear::alloyKitLook), idle D, idle R and the walk's passing frame R at TRUE 1x, written as three sheets (colour on grass
// and dark ground, greyscale, black silhouettes with nothing held: C18's masks) at 1x, 3x and 8x (8x only when 8000 px or
// less). Under each figure its name and key (helm skirt mass layer, 3 letters each).
//   alloys1x           the board set: every people's alloys at seeds 1..3 (a row per people) and the owner's Jade seed 5
//   silhouette:alloys  the same, black silhouettes only (the gear and the body: the C18 masks)
//   twins              the owner's round-1 twin groups, each member before (round 1: the guise, no silhouette) and now
//   world:<w>          world w's 6 x 6 window of families (cells 0..5), each alloy a black silhouette with its name and key;
//                      the 10 nearest pairs (C18's OD) ringed and numbered
//   shapes:<axis>      every value of an axis (helm skirt mass layer arm motif) on one figure (the Heartland T4 kit in an
//                      alloy, the other axes fixed), colour, greyscale and black
//   shapes:<people>    that people's materials (its T1-T3 kits and steel) beside its alloys at seeds 1..3, greyscale and black
namespace F2 = art::doll::forge;
std::string shapeAbbrev(const gear::Shape& s) {
  auto ab = [](int axis, int v) {
    if (!v) return std::string("---");
    std::string w = F2::shapeName(axis, v);
    for (char& ch : w) ch = (char)std::toupper((unsigned char)ch);
    if (w == "NONE") return std::string("-");
    return w.substr(0, 3);
  };
  return ab(F2::SX_HELM, s.helm) + " " + ab(F2::SX_SKIRT, s.skirt) + " " + ab(F2::SX_MASS, s.mass) + " " + ab(F2::SX_LAYER, s.layer);
}
struct ShapeEntry { std::string label, sub; art::HumanLook L; bool pal = true; bool gap = false; };
enum ShapeMode { SM_COLOUR, SM_GREY, SM_BLACK };
const struct { int fc; Clip c; int i; } kTrio[3] = {{0, Clip::Idle, 0}, {2, Clip::Idle, 0}, {2, Clip::Walk, 1}};
const int kTrioPitch = 22;
// a look's three frames (idle D, idle R, walk R) at 1x with their feet on g0, from x
void drawTrio(Board& b, const ShapeEntry& e, int x, int g0, ShapeMode m, int frames = 3) {
  doll::DollRecipe r = doll::resolve(e.L);
  if (!e.pal) r.ramps = doll::a3Palette(e.L.body == 1 ? 1 : 0);
  doll::ComposeOpts op;
  op.grey = m != SM_COLOUR;
  op.noHeld = m == SM_BLACK;
  const RigSheet s = doll::compose(r, RigPart::Loco, &op);
  for (int k = 0; k < frames; k++) {
    int px, py;
    const Canvas f = rigpv::frameCanvas(s, kTrio[k].fc, kTrio[k].c, 0, kTrio[k].i, px, py);
    if (f.w <= 0) continue;
    const int cx = x + 10 + k * kTrioPitch;
    if (m == SM_BLACK) {
      for (int y = 0; y < f.h; y++)
        for (int xx = 0; xx < f.w; xx++)
          if (f.get(xx, y) >> 24) b.c.set(cx - px + xx, g0 - py + y, rgba(18, 16, 22));
      continue;
    }
    shadow(b, cx, g0, std::min(20, std::max(10, f.w - 4)));
    b.put(f, cx - px, g0 - py);
  }
}
void paperFill(Board& b, int y0, int y1, uint32_t col) {
  for (int y = std::max(0, y0); y < y1 && y < b.c.h; y++)
    for (int x = 0; x < b.c.w; x++) b.c.set(x, y, col);
}
void greyFill(Board& b) {
  for (int y = 0; y < b.c.h; y++)
    for (int x = 0; x < b.c.w; x++) {
      const uint32_t p = b.c.get(x, y);
      const int l = (int)(0.299 * (p & 255) + 0.587 * (p >> 8 & 255) + 0.114 * (p >> 16 & 255));
      b.c.set(x, y, rgba(l, l, l));
    }
}
void saveScaled(const Board& b, const std::string& dir, const std::string& name) {
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
  if (std::max(b.c.w, b.c.h) * 3 <= 8000) savePng(b.c, dir + "/" + name + "_3x.png", 3);
  if (std::max(b.c.w, b.c.h) * 8 <= 8000) savePng(b.c, dir + "/" + name + "_8x.png", 8);
}
// rows of entries (a row label each), one sheet per mode
void shapeGrid(const std::string& dir, const std::string& name, const std::string& title, const std::vector<std::string>& rowLabels,
               const std::vector<std::vector<ShapeEntry>>& rows, std::initializer_list<ShapeMode> modes, int frames = 3) {
  size_t cols = 1;
  for (const auto& r : rows) cols = std::max(cols, r.size());
  const int cellW = frames * kTrioPitch + 10, rowH = 64, lab = 64, top = 14;
  for (ShapeMode m : modes) {
    Board b(lab + (int)cols * cellW + 8, top + (int)rows.size() * rowH + 4);
    if (m == SM_BLACK) paperFill(b, 0, b.c.h, rgba(232, 228, 216));
    else {
      for (size_t ri = 0; ri < rows.size(); ri++) if (ri & 1) darkGround(b, top + (int)ri * rowH, top + (int)(ri + 1) * rowH);
      if (m == SM_GREY) greyFill(b);
    }
    const uint32_t ink = m == SM_BLACK ? rgba(40, 36, 44) : rgba(255, 250, 230), dim = m == SM_BLACK ? rgba(96, 90, 104) : rgba(214, 226, 240);
    static const char* const kMode[3] = {"COLOUR", "GREYSCALE", "BLACK SILHOUETTES (NOTHING HELD)"};
    b.text(4, 3, title + " - " + kMode[m], ink);
    for (size_t ri = 0; ri < rows.size(); ri++) {
      const int y0 = top + (int)ri * rowH;
      b.text(4, y0 + 24, rowLabels[ri].substr(0, 10), ink);
      for (size_t ci = 0; ci < rows[ri].size(); ci++) {
        const ShapeEntry& e = rows[ri][ci];
        if (e.gap) continue;
        const int x = lab + (int)ci * cellW;
        b.text(x + 2, y0 + 1, e.label.substr(0, (size_t)(cellW / 6)), m == SM_BLACK ? ink : rgba(250, 214, 120));
        b.text(x + 2, y0 + 9, e.sub.substr(0, (size_t)(cellW / 6)), dim);
        drawTrio(b, e, x, y0 + rowH - 6, m, frames);
      }
    }
    static const char* const kSfx[3] = {"", "_grey", "_black"};
    if (m == SM_GREY) { greyRows(b.c, 0); greyCheck(b.c, 0, name + kSfx[m]); }
    saveScaled(b, dir, name + kSfx[m]);
  }
}
ShapeEntry alloyEntry(const cult::Culture& c, int i, int who = 0) {
  ShapeEntry e;
  e.L = gear::alloyKitLook(a3Look(who), c, i);
  e.label = c.arms.alloys[(size_t)i].name;
  e.sub = shapeAbbrev(gear::alloyShape(c, i));
  return e;
}
// the round-1 body of an alloy (STYLE_P2 28.10's "before"): the guise forced, round 1's picks, no silhouette
ShapeEntry roundOneEntry(const cult::Culture& c, int i) {
  ShapeEntry e = alloyEntry(c, i);
  e.L.forgeShape = 0;
  e.L.forgeGuise = gear::alloyGuise(c, i);
  const uint64_t pk = gear::packForge(gear::alloyPick(c, i).style);
  for (int s : {(int)art::HumanLook::TIER_BODY, (int)art::HumanLook::TIER_HELM, (int)art::HumanLook::TIER_SHIELD, (int)art::HumanLook::TIER_GLOVES,
                (int)art::HumanLook::TIER_BOOTS})
    if (e.L.forge[s]) e.L.forge[s] = pk;
  e.sub = "ROUND 1";
  return e;
}
void boardAlloys1x(const std::string& dir, bool blackOnly) {
  std::vector<std::string> labels;
  std::vector<std::vector<ShapeEntry>> rows;
  for (int p = 0; p < 12; p++) {
    labels.push_back(kPeopleName[p]);
    std::vector<ShapeEntry> row;
    for (uint32_t sd = 1; sd <= 3; sd++) {
      const cult::Culture& c = gear::kitCulture((cult::Archetype)p, sd);
      for (int i = 0; i < (int)c.arms.alloys.size(); i++) row.push_back(alloyEntry(c, i));
    }
    rows.push_back(row);
  }
  {
    labels.push_back("JADE S5");
    std::vector<ShapeEntry> row;
    const cult::Culture& c = gear::kitCulture(cult::Archetype::Jade, 5);
    for (int i = 0; i < (int)c.arms.alloys.size(); i++) row.push_back(alloyEntry(c, i));
    rows.push_back(row);
  }
  if (blackOnly) shapeGrid(dir, "doll_silhouette_alloys", "THE BOARD SET: SEEDS 1..3 AND THE OWNER'S JADE 5, IDLE D, IDLE R, WALK R, TRUE 1X", labels, rows, {SM_BLACK});
  else shapeGrid(dir, "doll_alloys1x", "THE BOARD SET: SEEDS 1..3 AND THE OWNER'S JADE 5, IDLE D, IDLE R, WALK R, TRUE 1X", labels, rows, {SM_COLOUR, SM_GREY, SM_BLACK});
}
// (P2c envelope, contracts 7) envelope:<hero|heroine|guard>[:grey|:black]: a line's tiers side by side on one baseline at 1x
// (saved scaled), the tier and the area ratio (noHeld, walk D frame 0 + idle R, of the line's T1) under each; the looks from
// tools/doll/envelope_looks.txt (or --looks F)
int envelopeArea(const art::HumanLook& L) {
  doll::DollRecipe r = doll::resolve(L);
  doll::ComposeOpts o;
  o.noHeld = true;
  int n = 0;
  for (int fc : {0, 2}) {
    o.onlyFacing = fc;
    const art::RigSheet sh = doll::compose(r, art::RigPart::Loco, &o);
    const art::FrameRect* f = sh.frame(fc, fc == 0 ? Clip::Walk : Clip::Idle, 0, 0);
    if (!f) continue;
    for (int y = 0; y < f->h; y++)
      for (int x = 0; x < f->w; x++) n += (sh.atlas.get(f->x + x, f->y + y) >> 24) != 0;
  }
  return n;
}
// (envelope fix round, D55) a look's helm pixels, idle D + idle R (C12f's head measure): the growth strip prints T2's budget
int envelopeHeadPx(const art::HumanLook& L) {
  const doll::DollRecipe r = doll::resolve(L);
  int n = 0;
  for (int fc : {0, 2}) {
    const doll::FrameProbe p = doll::probeFrame(r, art::RigPart::Loco, fc, Clip::Idle, 0, 0, true);
    for (size_t k = 0; p.ok && k < p.code.size(); k++) {
      if (p.code[k] < 2) continue;
      const int o = p.owner[k];
      if (o >= 0 && (size_t)o < p.items.size() && (p.items[(size_t)o].has(doll::Role::Helm) || p.items[(size_t)o].has(doll::Role::Headwear))) n++;
    }
  }
  return n;
}
void boardEnvelope(const std::string& dir, const std::string& b) {
  std::string line = b.substr(9);
  ShapeMode only = SM_COLOUR;
  bool one = false;
  if (line.size() > 5 && line.compare(line.size() - 5, 5, ":grey") == 0) { line.resize(line.size() - 5); only = SM_GREY; one = true; }
  if (line.size() > 6 && line.compare(line.size() - 6, 6, ":black") == 0) { line.resize(line.size() - 6); only = SM_BLACK; one = true; }
  std::vector<std::string> tags;
  if (line == "hero") tags = {"e1", "e2", "e3", "e4", "e5", "e6", "e6m"};
  else if (line == "heroine") tags = {"e1w", "e2w", "e3w", "e4w", "e5w", "e6w"};
  else if (line == "guard") tags = {"eg0", "eg3", "ecapt", "eking", "e6"};
  else if (line == "adefects") tags = {"a_uha", "a_jaon", "a_enkho", "a_papir", "a_crane", "fm_obsid", "f5_obsid", "fm_myth", "fm_star", "f5_star", "a_thaer", "a_thorn", "a_ilsil"};
  else { std::printf("doll_gallery: envelope:<hero|heroine|guard|adefects>\n"); return; }
  if (g_looksFile == "tools/doll/ladder_looks.txt") g_looksFile = "tools/doll/envelope_looks.txt";
  const std::vector<FileLook> fl = readLooksFile();
  std::vector<ShapeEntry> row;
  int area1 = 0;
  for (const std::string& t : tags)
    for (const FileLook& f : fl) {
      if (f.tag != t) continue;
      ShapeEntry e;
      e.L = a3Look(f.who);
      bool pal = false;
      doll::applyLookSpec(e.L, f.spec, &pal);
      e.pal = pal;
      const int a = envelopeArea(e.L);
      if (!area1) area1 = a;
      char sub[64];
      std::snprintf(sub, sizeof sub, "T%d %.2f", e.L.topTier(), area1 ? (double)a / area1 : 0.0);
      e.label = t;
      e.sub = sub;
      row.push_back(e);
    }
  // (envelope fix round, D55; the delight) the T2 head budget under T2: its helm pixels and T3's / 1.05 (C12f's head step)
  if ((line == "hero" || line == "heroine") && row.size() >= 3) {
    const int h2 = envelopeHeadPx(row[1].L), h3 = envelopeHeadPx(row[2].L);
    char sub[64];
    std::snprintf(sub, sizeof sub, "%s H%d/%d", row[1].label.c_str(), h2, h3 * 100 / 105);   // the label's line has the room
    row[1].label = sub;
  }
  std::string up = line;
  for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
  const std::string title = "THE ENVELOPE: " + up + " (TIER, AREA OF THE FIRST), IDLE D, IDLE R, WALK R, TRUE 1X";
  if (one) shapeGrid(dir, "doll_envelope_" + line, title, {up}, {row}, {only});
  else shapeGrid(dir, "doll_envelope_" + line, title, {up}, {row}, {SM_COLOUR, SM_GREY, SM_BLACK});
}
// (P2c envelope, C16k) heads:owner37: the owner's 37 board alloys, a row per culture, each with its head key (family, variant,
// crest) under it; colour, greyscale and black
void boardHeads(const std::string& dir) {
  std::vector<std::string> labels;
  std::vector<std::vector<ShapeEntry>> rows;
  for (const gear::BoardCulture& bc : gear::forgeBoardSet()) {
    if (!bc.owner) continue;
    const cult::Culture& c = gear::kitCulture((cult::Archetype)bc.people, bc.seed);
    std::vector<ShapeEntry> row;
    for (int i = 0; i < (int)c.arms.alloys.size() && i < 3; i++) {
      ShapeEntry e = alloyEntry(c, i);
      const gear::Shape s = gear::alloyShape(c, i);
      const gear::AlloyDetails d = gear::alloyDetails(c, i, s);
      char sub[64];
      std::snprintf(sub, sizeof sub, "%s V%d %s", F2::shapeName(F2::SX_HELM, s.helm), s.variant, d.crest ? F2::valueName(F2::AX_CREST, d.crest) : "-");
      e.sub = sub;
      for (char& ch : e.sub) ch = (char)std::toupper((unsigned char)ch);
      row.push_back(e);
    }
    labels.push_back(std::string(kPeopleName[bc.people]) + " S" + std::to_string(bc.seed));
    rows.push_back(row);
  }
  shapeGrid(dir, "doll_heads_owner37", "THE OWNER'S 37: A HEAD EACH (FAMILY, VARIANT, CREST), TRUE 1X", labels, rows, {SM_COLOUR, SM_GREY, SM_BLACK});
}
void boardTwins(const std::string& dir) {
  int ng = 0;
  const char* const* groups = gear::forgeTwinGroups(&ng);
  std::vector<std::string> labels;
  std::vector<std::vector<ShapeEntry>> before, now;
  for (int g = 0; g < ng; g++) {
    std::vector<ShapeEntry> rb, rn;
    std::string list = groups[g];
    size_t a = 0;
    while (a < list.size()) {
      size_t e = list.find(' ', a);
      if (e == std::string::npos) e = list.size();
      const std::string nm = list.substr(a, e - a);
      a = e + 1;
      for (const gear::BoardCulture& bc : gear::forgeBoardSet()) {
        if (!bc.owner) continue;
        const cult::Culture& c = gear::kitCulture((cult::Archetype)bc.people, bc.seed);
        for (int i = 0; i < (int)c.arms.alloys.size(); i++)
          if (c.arms.alloys[(size_t)i].name == nm) { rb.push_back(roundOneEntry(c, i)); rb.back().label = nm; rn.push_back(alloyEntry(c, i)); }
      }
    }
    labels.push_back("GROUP " + std::to_string(g + 1));
    before.push_back(rb);
    now.push_back(rn);
  }
  std::vector<std::string> l2;
  std::vector<std::vector<ShapeEntry>> both;
  for (int g = 0; g < ng; g++) {
    l2.push_back(labels[(size_t)g] + " R1");
    both.push_back(before[(size_t)g]);
    l2.push_back(labels[(size_t)g] + " NOW");
    both.push_back(now[(size_t)g]);
  }
  shapeGrid(dir, "doll_twins", "THE OWNER'S ROUND-1 TWINS: EACH GROUP BEFORE (ROUND 1) AND NOW", l2, both, {SM_COLOUR, SM_BLACK});
}
void boardShapesAxis(const std::string& dir, const std::string& axisName) {
  const int axis = F2::shapeAxisOf(axisName.c_str());
  const int n = F2::shapeCount(axis);
  static const char* const kBase[F2::SX_COUNT] = {"cone", "tassets", "round", "none", "close", "plain"};
  std::vector<ShapeEntry> row;
  for (int v = 1; v <= n; v++) {
    if (axis == F2::SX_HELM && v > F2::H_ALLOY_LAST) break;
    ShapeEntry e;
    e.L = a3Look(0);
    std::string spec = "kit=heartland:4:1,alloy=2,palette=look,shape=";
    for (int a = 0; a < F2::SX_COUNT; a++) spec += std::string(a ? "+" : "") + F2::shapeAxisName(a) + ":" + (a == axis ? F2::shapeName(a, v) : kBase[a]);
    doll::applyLookSpec(e.L, spec);
    std::string up = F2::shapeName(axis, v);
    for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
    e.label = up;
    const doll::DollRecipe r = doll::resolve(e.L);
    e.sub = r.shape[axis] == v || (axis == F2::SX_ARM && v <= F2::AR_BARE) ? "DRAWN" : "NOT DRAWN";
    row.push_back(e);
  }
  if (axis == F2::SX_HELM)   // the hero-only families on their materials
    for (const char* m : {"cloth", "leather", "ember", "starmetal"}) {
      ShapeEntry e;
      e.L = a3Look(0);
      doll::applyLookSpec(e.L, std::string("hero=1,kit=heartland:4:7,mat=") + m + ",palette=look");
      e.label = m == std::string("cloth") ? "HOOD" : m == std::string("leather") ? "CAP" : m == std::string("ember") ? "FURNACE" : "CRYSTAL";
      e.sub = "HERO";
      row.push_back(e);
    }
  std::string up = axisName;
  for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
  shapeGrid(dir, "doll_shapes_" + axisName, "THE KIT: EVERY " + up + " ON ONE FIGURE (HEARTLAND T4 IN AN ALLOY)", {up}, {row}, {SM_COLOUR, SM_GREY, SM_BLACK});
}
void boardShapesPeople(const std::string& dir, const std::string& people) {
  cult::Archetype ar;
  if (!gear::peopleOf(people, ar)) return;
  std::vector<ShapeEntry> mats, alloys;
  for (int t = 1; t <= 3; t++) {
    ShapeEntry e;
    e.L = a3Look(0);
    doll::applyLookSpec(e.L, "kit=" + people + ":" + std::to_string(t) + ":1,palette=look");
    static const char* const kT[4] = {"", "T1 KIT", "T2 KIT", "T3 KIT"};
    e.label = kT[t];
    e.sub = gear::describeForge(gear::unpackForge(e.L.forge[art::HumanLook::TIER_BODY])).substr(0, 10);
    mats.push_back(e);
  }
  for (const char* m : {"leather", "bronze", "iron", "steel"}) {
    ShapeEntry e;
    e.L = a3Look(0);
    doll::applyLookSpec(e.L, "kit=" + people + ":4:1,palette=look,mat=" + m + ",forgeKit=0");
    e.L.forgeKit = 0;
    std::string up = m;
    for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
    e.label = up;
    e.sub = "MATERIAL";
    mats.push_back(e);
  }
  for (uint32_t sd = 1; sd <= 3; sd++) {
    const cult::Culture& c = gear::kitCulture(ar, sd);
    for (int i = 0; i < (int)c.arms.alloys.size(); i++) alloys.push_back(alloyEntry(c, i));
  }
  std::string up = people;
  for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
  shapeGrid(dir, "doll_shapes_" + people, up + ": ITS MATERIALS BESIDE ITS ALLOYS (SEEDS 1..3)", {"MATERIALS", "ALLOYS"}, {mats, alloys}, {SM_COLOUR, SM_GREY, SM_BLACK});
}
void boardWorld(const std::string& dir, const std::string& arg) {
  const int w = std::atoi(arg.c_str() + arg.find(':') + 1);
  cult::Atlas at((uint64_t)std::max(1, w));
  struct A { const cult::Culture* c; int idx, ci, cj; doll::Outline o; int x = 0, y = 0; };
  std::vector<A> as;
  for (int cj = 0; cj < 6; cj++)
    for (int ci = 0; ci < 6; ci++) {
      const cult::Culture& c = at.family(ci, cj);
      for (int i = 0; i < (int)c.arms.alloys.size(); i++) {
        A a{&c, i, ci, cj, {}};
        a.o = doll::outlineOf(doll::resolve(gear::alloyKitLook(a3Look(0), c, i)));
        as.push_back(std::move(a));
      }
    }
  // the 10 nearest pairs: body OD under 0.05 and head OD under 0.10 is a twin (C18a); nearest by body / 0.05 + head / 0.10
  struct P { size_t a, b; double s, body, head; };
  std::vector<P> ps;
  for (size_t i = 0; i < as.size(); i++)
    for (size_t j = i + 1; j < as.size(); j++) {
      const doll::OutlineOD d = doll::outlineDistance(as[i].o, as[j].o);
      ps.push_back(P{i, j, d.body / 0.05 + d.head / 0.10, d.body, d.head});
    }
  std::sort(ps.begin(), ps.end(), [](const P& x, const P& y) { return x.s < y.s; });
  const int cellW = 3 * 46 + 6, rowH = 78, top = 14;
  Board b(6 * cellW + 8, top + 6 * rowH + 12 * 8 + 8);
  paperFill(b, 0, b.c.h, rgba(232, 228, 216));
  const uint32_t ink = rgba(40, 36, 44);
  b.text(4, 3, "WORLD " + std::to_string(w) + ": THE 6 X 6 WINDOW (CELLS 0..5), EVERY ALLOY BLACK, NOTHING HELD, TRUE 1X", ink);
  for (A& a : as) {
    const int x0 = 4 + a.ci * cellW + a.idx * 46, y0 = top + a.cj * rowH;
    a.x = x0; a.y = y0;
    if (a.idx == 0) b.text(4 + a.ci * cellW, y0 + 1, std::string(cult::archetypeName(a.c->archetype)).substr(0, 9), rgba(120, 60, 40));
    ShapeEntry e;
    e.L = gear::alloyKitLook(a3Look(0), *a.c, a.idx);
    drawTrio(b, e, x0 - 4, y0 + rowH - 22, SM_BLACK, 2);
    b.text(x0, y0 + rowH - 19, a.c->arms.alloys[(size_t)a.idx].name.substr(0, 7), ink);
    const gear::Shape s = gear::alloyShape(*a.c, a.idx);
    b.text(x0, y0 + rowH - 11, shapeAbbrev(s).substr(0, 7), rgba(96, 90, 104));
  }
  static const uint32_t kRing[10] = {rgba(220, 40, 40), rgba(230, 120, 20), rgba(200, 170, 0), rgba(40, 160, 60), rgba(30, 140, 200),
                                     rgba(90, 70, 210), rgba(200, 50, 170), rgba(120, 80, 40), rgba(0, 150, 150), rgba(140, 140, 140)};
  for (size_t k = 0; k < ps.size() && k < 10; k++) {
    for (size_t q : {ps[k].a, ps[k].b}) {
      const A& a = as[q];
      for (int i = 0; i < 46; i++) { b.c.set(a.x - 3 + i, a.y + 10 + (int)k % 3, kRing[k]); b.c.set(a.x - 3 + i, a.y + rowH - 3 - (int)k % 3, kRing[k]); }
      b.text(a.x + 34, a.y + 12, std::to_string(k + 1), kRing[k]);
    }
    char line[160];
    std::snprintf(line, sizeof line, "%zu %s/%s - %s/%s BODY %.3f HEAD %.3f", k + 1, cult::archetypeName(as[ps[k].a].c->archetype),
                  as[ps[k].a].c->arms.alloys[(size_t)as[ps[k].a].idx].name.c_str(), cult::archetypeName(as[ps[k].b].c->archetype),
                  as[ps[k].b].c->arms.alloys[(size_t)as[ps[k].b].idx].name.c_str(), ps[k].body, ps[k].head);
    b.text(4, top + 6 * rowH + (int)k * 8, line, kRing[k]);
  }
  saveScaled(b, dir, "doll_world_" + std::to_string(w));
}

void boardSpecs(const std::string& dir, const std::string& arg) {
  std::vector<ForgeEntry> es;
  size_t i = arg.find(':') + 1;
  while (i < arg.size()) {
    size_t j = arg.find(';', i);
    if (j == std::string::npos) j = arg.size();
    const std::string e = arg.substr(i, j - i);
    const size_t q = e.find('=');
    if (q != std::string::npos) es.push_back(ForgeEntry{e.substr(0, q), 0, e.substr(q + 1)});
    i = j + 1;
  }
  forgeSheet(dir, "doll_specs", "SPECS", es, 0);
}
// families: the 96 alloy families (8 slots x 12 peoples) as ramps under the plain model and under the people's sheens
void boardFamilies(const std::string& dir) {
  static const char* const kSlot[8] = {"BRONZE", "BRASS", "IRON", "STEEL 1", "SILVER 1", "STEEL 2", "SILVER 2", "GLASS"};
  static const char* const kPeopleShort[12] = {"FJ", "HI", "HE", "IM", "DU", "ST", "MA", "JA", "RI", "SU", "SY", "SS"};
  const int cw = 64, rh = 24;
  Board b(70 + 12 * cw, 30 + 8 * rh * 2);
  for (int y = 0; y < b.c.h; y++) for (int x = 0; x < b.c.w; x++) b.c.set(x, y, rgba(44, 42, 48));
  b.text(4, 3, "THE 96 ALLOY FAMILIES: PLAIN (TOP) AND IN EACH SHEEN ROW 2: MATTE BRIGHT DARK BANDED IRID GLOW PALE BURN");
  for (int p = 0; p < 12; p++) b.text(70 + p * cw + 4, 16, kPeopleShort[p]);
  for (int s = 0; s < 8; s++) {
    b.text(4, 30 + s * rh * 2 + 4, kSlot[s]);
    for (int p = 0; p < 12; p++) {
      const int f = doll::alloyFamilyAt(s, p);
      uint32_t r[5];
      doll::alloyRamp(f, 0, r);
      for (int k = 0; k < 5; k++)
        for (int yy = 0; yy < rh - 6; yy++)
          for (int xx = 0; xx < 11; xx++) b.c.set(70 + p * cw + k * 11 + xx, 30 + s * rh * 2 + yy, r[k] | 0xFF000000u);
      for (int sh = 1; sh <= 8; sh++) {
        doll::alloyRamp(f, (uint8_t)sh, r);
        for (int k = 0; k < 5; k++)
          for (int yy = 0; yy < 3; yy++)
            b.c.set(70 + p * cw + (sh - 1) * 7 + k, 30 + s * rh * 2 + rh - 4 + yy + (k > 4 ? 0 : 0), r[k] | 0xFF000000u);
      }
    }
  }
  savePng(b.c, dir + "/doll_families_1x.png", 1);
  savePng(b.c, dir + "/doll_families_3x.png", 3);
}

int main(int argc, char** argv) {
  gear::installLadderMacros();   // (P2c ladder) the looks macros (hero kit npc wpn rarity tier) in --look and --slice
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
    if (!std::strcmp(argv[i], "--looks") && i + 1 < argc) { g_looksFile = argv[++i]; continue; }
    if (!std::strcmp(argv[i], "--tag") && i + 1 < argc) {   // (P2c ladder) a looks-file line: its spec and palette
      const std::string t = argv[++i];
      bool found = false;
      for (const FileLook& fl : readLooksFile())
        if (fl.tag == t) { g_who = fl.who; g_lookSpec = fl.spec; g_lookPalette = fl.pal; found = true; }
      if (!found) { std::printf("doll_gallery: --tag: %s not in %s\n", t.c_str(), g_looksFile.c_str()); return 1; }
      continue;
    }
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
    else if (b == "ladder" || b.rfind("ladder:", 0) == 0) boardLadder(dir, b);
    else if (b.rfind("finish:", 0) == 0) boardFinish(dir, b);
    else if (b.rfind("motion:", 0) == 0) boardMotion(dir, b);
    else if (b.rfind("lit:", 0) == 0) boardLit(dir, b);
    else if (b.rfind("film:", 0) == 0) boardFilm(dir, b);
    else if (b == "alloys") boardAlloys(dir);
    else if (b == "metals") boardMetals(dir);
    else if (b == "forge" || b.rfind("forge:", 0) == 0 || b == "forgegrey" || b.rfind("forgegrey:", 0) == 0) boardForge(dir, b);
    else if (b == "silhouette:forge") boardForgeSilhouette(dir);
    else if (b.rfind("alloystyles:", 0) == 0) boardAlloyStyles(dir, b);
    else if (b.rfind("specs:", 0) == 0) boardSpecs(dir, b);
    else if (b == "metalread") boardForge(dir, "forge:metalread");
    else if (b == "families") boardFamilies(dir);
    else if (b == "alloys1x") boardAlloys1x(dir, false);
    else if (b == "silhouette:alloys") boardAlloys1x(dir, true);
    else if (b == "twins") boardTwins(dir);
    else if (b.rfind("envelope:", 0) == 0) boardEnvelope(dir, b);
    else if (b == "heads:owner37" || b == "heads") boardHeads(dir);
    else if (b.rfind("world:", 0) == 0) boardWorld(dir, b);
    else if (b.rfind("shapes:", 0) == 0) {
      const std::string w = b.substr(7);
      cult::Archetype ar;
      if (gear::peopleOf(w, ar)) boardShapesPeople(dir, w);
      else if (art::doll::forge::shapeAxisOf(w.c_str()) >= 0) boardShapesAxis(dir, w);
      else std::printf("doll_gallery: shapes: unknown axis or people %s\n", w.c_str());
    }
    else std::printf("doll_gallery: unknown board %s\n", b.c_str());
  }
  return g_greyBad ? 1 : 0;   // (D63) a grey board with a coloured pixel fails the run
}
