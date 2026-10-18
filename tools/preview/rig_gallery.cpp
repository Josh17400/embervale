// rig_gallery (P2a RIG-BODY; skeleton by the phase A lead, handed to RIG-BODY): the v2 body, faces, hands, hair, cloth,
// locomotion and the fall (STYLE_P2 2-5, 7, 10, 13, 14, 19). Every board at 1x and 4x.
//   rig_gallery <outDir>    rig_slice.png      the style slice: the hero as a man and as a woman (Idle, Walk, Hurt, the
//                                              sword's three swings, Fall, Lying) and the village woman (Idle, Walk),
//                                              four facings, beside today's v1 villager
//                           rig_frames.png     every slice frame on flat grass and road, on a fixed grid (a study sheet:
//                                              cell 40 x 52, the pivot at the cell's (20, 46))
//                           rig_bodies.png     man / woman x slim / average / broad x human / half-breed / elf x adult /
//                                              youth / child, down idle
//                           rig_fall.png       the hero man (rows 1-4) and woman (5-8) per facing (down, up, right,
//                                              left): the fall to the west (recoil, buckle, topple, lying head west),
//                                              then to the east, at 6x on the study grid (cell 40 x 52, pivot (20, 46))
//                           (the base, also alone with `base`:) rig_hair (the 8 styles on both bodies, the beard),
//                           rig_skins (S1-S8, eye and hair colours), rig_cloth (Tunic, Dress, Robe, the hood, Rags,
//                           the 8 culture cuts), rig_patterns (the 6 patterns), rig_headwear (the legacy hood and the
//                           8 kinds over short and long hair), rig_marks (face paint, jewellery, the peoples' ears),
//                           rig_carry / rig_carryfall (every weapon at rest, and its fall and lying), rig_kin /
//                           rig_kinfall (youth, child, the builds; their fall): idle in four facings, then a walk
//   rig_gallery <outDir> check                 the slice's frame numbers (heights, widths, R8-R10, the sword's tip
//                                              separations, the topple side per variant, atlas KB)
//   rig_gallery <outDir> dump <who 0-2> <part 0-2> <facing> <clip> <variant> <i>
//                                              prints one frame as text (a letter per colour, its legend and luma)
//   rig_gallery <outDir> hash                  the slice's atlases hashed, whole and per clip (a regression check)
//   rig_gallery <outDir> sizes [weapon]        R6 per look: LOCO / DEATH KB, packing use and bake ms (the R6 looks,
//                                              weapons, headwear, hair, cuts); with a weapon id its frames' sizes
//   rig_gallery <outDir> prof                  R6's bake measure (test_rig.cpp's own loop), three runs
//   rig_gallery <outDir> fields <A> <B>        R3's census of seeds A..B: what each uncovered plain look carries
// The judge reads these boards.
#include <chrono>
#include <cstring>
#include <map>
#include "tools/preview/rig_preview.h"
#include "rpg/art/rig/rig_internal.h"
#include "rpg/sim/anim.h"
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/skill_ops.h"

namespace {

// a look's part; the sword's swings come from the slice painter until rig_actions.cpp forwards to it (RIG-ARMS)
art::RigSheet sheetOf(const art::HumanLook& L, art::RigPart part) {
  art::RigSheet s = art::humanRig(L, part);
  if (s.empty() && part == art::RigPart::Action) s = art::rig2::sliceSwordAction(L);
  return s;
}

void dumpFrame(const art::RigSheet& s, int facing, art::Clip c, int variant, int i) {
  int px, py;
  const Canvas f = rigpv::frameCanvas(s, facing, c, variant, i, px, py);
  if (f.w <= 0) { std::printf("no frame\n"); return; }
  std::map<uint32_t, char> pal;
  const char* letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@#$%&*+=?";
  std::printf("frame %d x %d, pivot %d,%d (rows: y relative to the pivot)\n", f.w, f.h, px, py);
  for (int y = 0; y < f.h; y++) {
    std::printf("%4d ", y - py);
    for (int x = 0; x < f.w; x++) {
      const uint32_t p = f.get(x, y);
      if (!(p >> 24)) { std::putchar(x == px ? ':' : '.'); continue; }
      auto it = pal.find(p);
      if (it == pal.end()) it = pal.emplace(p, letters[pal.size() % 72]).first;
      std::putchar(it->second);
    }
    std::putchar('\n');
  }
  for (auto& kv : pal) {
    const uint32_t p = kv.first;
    const int r = p & 255, g = (p >> 8) & 255, b = (p >> 16) & 255;
    std::printf("  %c #%02X%02X%02X luma %.0f\n", kv.second, r, g, b, r * 0.299 + g * 0.587 + b * 0.114);
  }
  const art::RigAnchors* an = s.anchorsOf(facing, c, variant, i);
  if (an) std::printf("anchors: head %d,%d face %d,%d handR %d,%d handL %d,%d hip %d,%d grip %d,%d tip %d,%d behind %d\n", an->headX, an->headY, an->faceX,
                      an->faceY, an->handRX, an->handRY, an->handLX, an->handLY, an->hipX, an->hipY, an->gripX, an->gripY, an->tipX, an->tipY,
                      (int)an->weaponBehind);
}

// a flat study board: frames on a fixed grid over flat grass (even rows) and road (odd rows)
struct Study {
  Canvas c;
  int cols, rows;
  static constexpr int CW = 40, CH = 52;
  Study(int cols_, int rows_) : c(cols_ * CW, rows_ * CH), cols(cols_), rows(rows_) {
    for (int y = 0; y < c.h; y++)
      for (int x = 0; x < c.w; x++) c.set(x, y, ((y / CH) & 1) ? rgba(176, 146, 104) : rgba(86, 140, 62));
  }
  void put(const art::RigSheet& s, int facing, art::Clip clip, int variant, int i, int col, int row) {
    int px, py;
    const Canvas f = rigpv::frameCanvas(s, facing, clip, variant, i, px, py);
    if (f.w <= 0) return;
    const int ox = col * CW + 20 - px, oy = row * CH + 46 - py;
    for (int y = 0; y < f.h; y++)
      for (int x = 0; x < f.w; x++) {
        const uint32_t p = f.get(x, y);
        if (p >> 24) c.set(ox + x, oy + y, p);
      }
  }
};

// a labelled sheet of looks: one row per look, its frames stood on a common ground line by their pivots (cells 36 x 54,
// the pivot at (18, 48)), alternate rows on grass and road, the label in the left column (RIG-BODY's base boards)
struct LookSheet {
  static constexpr int LW = 112, CW = 36, CH = 54, PX = 18, PY = 48;
  Board b;
  int cols;
  LookSheet(int cols_, int rows) : b(LW + cols_ * CW, rows * CH + 4), cols(cols_) {
    for (int y = 0; y < b.c.h; y++)
      if ((y / CH) & 1)
        for (int x = LW; x < b.c.w; x++) b.c.set(x, y, rgba(176, 146, 104));
  }
  void label(int row, const std::string& s) { b.text(2, row * CH + PY - 16, s); }
  void put(const art::RigSheet& s, int facing, art::Clip clip, int variant, int i, int col, int row) {
    int px, py;
    const Canvas f = rigpv::frameCanvas(s, facing, clip, variant, i, px, py);
    if (f.w <= 0) return;
    b.put(f, LW + col * CW + PX - px, row * CH + PY - py);
  }
  // the four facings' idle (down, up, right, left), then the walk's six frames of `walkFacing`
  void idleAndWalk(const art::HumanLook& L, int row, int walkFacing) {
    const art::RigSheet s = art::humanRig(L, art::RigPart::Loco);
    for (int fc = 0; fc < 4; fc++) put(s, fc, art::Clip::Idle, 0, 0, fc, row);
    for (int i = 0; i < 6 && 4 + i < cols; i++) put(s, walkFacing, art::Clip::Walk, 0, i, 4 + i, row);
  }
};

art::HumanLook plainLook(bool woman) {
  art::HumanLook L = rigpv::sliceHero(woman);
  L.weapon = 0;
  return L;
}

// the RIG-BODY base boards (STYLE_P2 2-7, 13-14): hair, skins, garments, headwear, marks, kids and walks
void baseBoards(const std::string& dir) {
  using art::HumanLook;
  static const char* hairName[8] = {"BALD", "SHORT", "LONG", "PONYTAIL", "MOHAWK", "BRAIDS", "BUN", "CURLS"};
  static const uint32_t hairCol[8] = {rgba(36, 30, 34), rgba(120, 70, 36), rgba(220, 180, 100), rgba(150, 64, 36), rgba(200, 104, 44),
                                      rgba(70, 44, 30), rgba(232, 220, 182), rgba(36, 30, 34)};
  {   // hair: the 8 styles on the man (idle x 4, a side walk) and on the woman, then the beard
    LookSheet sh(10, 18);
    for (int k = 0; k < 8; k++)
      for (int w = 0; w < 2; w++) {
        HumanLook L = plainLook(w == 1);
        L.hair = (art::Hair)k; L.hairColor = hairCol[k];
        const int row = k * 2 + w;
        sh.label(row, std::string(hairName[k]) + (w ? " (W)" : " (M)"));
        sh.idleAndWalk(L, row, 2);
      }
    for (int k = 0; k < 2; k++) {
      HumanLook L = plainLook(false);
      L.beard = true; L.hair = k ? art::Hair::Bald : art::Hair::Short; L.hairColor = k ? rgba(150, 148, 150) : rgba(70, 44, 30);
      sh.label(16 + k, k ? "BEARD, BALD, ASH" : "BEARD, SHORT");
      sh.idleAndWalk(L, 16 + k, 3);
    }
    rigpv::saveBoth(sh.b, dir, "rig_hair");
  }
  {   // skins S1-S8 on both bodies, with the eight eye colours and hair colours varied
    static const uint32_t skins[8] = {rgba(250, 220, 190), rgba(242, 204, 166), rgba(236, 188, 146), rgba(214, 160, 116),
                                      rgba(190, 134, 94), rgba(160, 104, 70), rgba(124, 80, 56), rgba(92, 60, 46)};
    static const uint32_t eyes[6] = {rgba(96, 58, 32), rgba(60, 112, 196), rgba(56, 140, 80), rgba(120, 132, 146), rgba(206, 146, 40), rgba(136, 80, 176)};
    static const uint32_t hairs[10] = {rgba(36, 30, 34), rgba(70, 44, 30), rgba(120, 70, 36), rgba(150, 64, 36), rgba(200, 104, 44),
                                       rgba(220, 180, 100), rgba(232, 220, 182), rgba(150, 148, 150), rgba(228, 226, 218), rgba(120, 50, 110)};
    LookSheet sh(10, 8);
    for (int k = 0; k < 8; k++) {
      for (int w = 0; w < 2; w++) {
        HumanLook L = plainLook(w == 1);
        L.skin = skins[k]; L.eyeColor = eyes[(k + w) % 6]; L.hairColor = hairs[(k * 3 + w) % 10];
        L.hair = w ? art::Hair::Long : art::Hair::Short;
        const art::RigSheet s = art::humanRig(L, art::RigPart::Loco);
        for (int fc = 0; fc < 4; fc++) sh.put(s, fc, art::Clip::Idle, 0, 0, w * 5 + fc, k);
      }
      char t[16]; std::snprintf(t, sizeof t, "S%d", k + 1);
      sh.label(k, t);
    }
    rigpv::saveBoth(sh.b, dir, "rig_skins");
  }
  {   // garments: the outfits and the 8 culture cuts (woman and man), idle x 4 and a side walk
    struct G { const char* name; art::Outfit o; int cut; int body; uint32_t top, pat; int pattern; bool hood; };
    const G gs[] = {
        {"TUNIC", art::Outfit::Tunic, 0, 0, rgba(60, 100, 160), 0, 0, false},
        {"DRESS", art::Outfit::Dress, 0, 1, rgba(120, 60, 110), 0, 0, false},
        {"ROBE", art::Outfit::Robe, 0, 0, rgba(70, 70, 120), 0, 0, false},
        {"ROBE, HOOD", art::Outfit::Robe, 0, 1, rgba(110, 40, 40), 0, 0, true},
        {"RAGS", art::Outfit::Rags, 0, 0, 0, 0, 0, false},
        {"CUT TUNIC", art::Outfit::Tunic, 1, 0, rgba(70, 120, 80), rgba(220, 190, 110), 3, false},
        {"CUT ROBE", art::Outfit::Robe, 2, 0, rgba(60, 80, 140), rgba(200, 160, 70), 0, false},
        {"CUT KAFTAN", art::Outfit::Tunic, 3, 0, rgba(150, 90, 40), rgba(230, 200, 120), 5, false},
        {"CUT WRAP", art::Outfit::Dress, 4, 1, rgba(200, 180, 140), rgba(160, 60, 50), 3, false},
        {"CUT COAT", art::Outfit::Tunic, 5, 0, rgba(90, 60, 50), 0, 0, false},
        {"CUT KILT", art::Outfit::Tunic, 6, 0, rgba(60, 110, 70), rgba(170, 50, 50), 2, false},
        {"CUT PONCHO", art::Outfit::Tunic, 7, 0, rgba(170, 110, 60), rgba(70, 40, 90), 0, false},
        {"CUT GOWN", art::Outfit::Dress, 8, 1, rgba(80, 140, 110), rgba(230, 210, 140), 6, false},
        {"KAFTAN (W)", art::Outfit::Dress, 3, 1, rgba(40, 90, 120), rgba(220, 170, 80), 1, false},
        {"COAT (W)", art::Outfit::Tunic, 5, 1, rgba(120, 40, 50), 0, 4, false},
        {"KILT (W)", art::Outfit::Tunic, 6, 1, rgba(50, 70, 120), rgba(200, 180, 90), 5, false},
    };
    const int n = (int)(sizeof gs / sizeof gs[0]);
    LookSheet sh(10, n);
    for (int k = 0; k < n; k++) {
      HumanLook L = plainLook(gs[k].body == 1);
      L.outfit = gs[k].o; L.cut = (uint8_t)gs[k].cut; L.pattern = (uint8_t)gs[k].pattern; L.hood = gs[k].hood;
      if (gs[k].top) L.topColor = gs[k].top;
      L.patternColor = gs[k].pat;
      L.tabardColor = rgba(200, 170, 80);
      sh.label(k, gs[k].name);
      sh.idleAndWalk(L, k, 2);
    }
    rigpv::saveBoth(sh.b, dir, "rig_cloth");
  }
  {   // patterns 0-6 on a dress and a tunic
    static const char* pn[7] = {"PLAIN", "STRIPES", "CHECKS", "BORDER", "DOTS", "EMBROIDERY", "VINES"};
    LookSheet sh(10, 7);
    for (int p = 0; p < 7; p++) {
      for (int w = 0; w < 2; w++) {
        HumanLook L = plainLook(w == 1);
        L.outfit = w ? art::Outfit::Dress : art::Outfit::Tunic; L.cut = w ? 8 : 1;
        L.pattern = (uint8_t)p; L.patternColor = rgba(230, 200, 120); L.topColor = w ? rgba(150, 70, 60) : rgba(60, 90, 140);
        const art::RigSheet s = art::humanRig(L, art::RigPart::Loco);
        for (int fc = 0; fc < 4; fc++) sh.put(s, fc, art::Clip::Idle, 0, 0, w * 5 + fc, p);
      }
      sh.label(p, pn[p]);
    }
    rigpv::saveBoth(sh.b, dir, "rig_patterns");
  }
  {   // headwear: the legacy hood and the 8 culture kinds, over short hair (man) and long hair (woman)
    static const char* hn[9] = {"HOOD (LEGACY)", "HOOD", "CAP", "TURBAN", "FUR HAT", "VEIL", "CIRCLET", "CONICAL", "HEADSCARF"};
    LookSheet sh(10, 18);
    for (int k = 0; k < 9; k++)
      for (int w = 0; w < 2; w++) {
        HumanLook L = plainLook(w == 1);
        L.hair = w ? art::Hair::Long : art::Hair::Short;
        if (k == 0) L.hood = true; else L.headwear = (uint8_t)k;
        L.headColor = k == 4 ? rgba(130, 96, 70) : (w ? rgba(170, 60, 70) : rgba(200, 190, 150));
        const int row = k * 2 + w;
        sh.label(row, std::string(hn[k]) + (w ? " (W)" : " (M)"));
        sh.idleAndWalk(L, row, 3);
      }
    rigpv::saveBoth(sh.b, dir, "rig_headwear");
  }
  {   // marks: face paint 1-4, jewellery 1-3, the peoples' ears (half-breed, elf) and an elf woman
    struct M { const char* name; int paint, jewel, people, body; };
    const M ms[] = {{"STRIPES", 1, 0, 0, 0}, {"DOTS", 2, 0, 0, 1}, {"MASK BAND", 3, 0, 0, 0}, {"SUN MARK", 4, 0, 0, 1},
                    {"EARRINGS", 0, 1, 0, 1}, {"TORC", 0, 2, 0, 0}, {"BOTH", 0, 3, 0, 1},
                    {"HALF-BREED M", 0, 0, 1, 0}, {"HALF-BREED W", 0, 0, 1, 1}, {"ELF M", 0, 0, 2, 0}, {"ELF W", 0, 0, 2, 1}, {"ELF W, SUN, EARRING", 4, 1, 2, 1}};
    const int n = (int)(sizeof ms / sizeof ms[0]);
    LookSheet sh(10, n);
    for (int k = 0; k < n; k++) {
      HumanLook L = plainLook(ms[k].body == 1);
      L.facePaint = (uint8_t)ms[k].paint; L.jewellery = (uint8_t)ms[k].jewel; L.people = (uint8_t)ms[k].people;
      if (ms[k].people == 2) { L.hairColor = rgba(232, 220, 182); L.hair = ms[k].body ? art::Hair::Long : art::Hair::Ponytail; }
      sh.label(k, ms[k].name);
      sh.idleAndWalk(L, k, 2);
    }
    rigpv::saveBoth(sh.b, dir, "rig_marks");
  }
  {   // what a fighter carries in LOCO and how it falls: every weapon at rest (RIG-ARMS's armsRest; the sword the slice's,
    // the bow braced in the off hand), idle x 4 and a side walk; then the fall and lying with it
    static const char* wn[10] = {"UNARMED", "SWORD", "AXE", "BOW", "STAFF", "DAGGER", "MACE", "GREATSWORD", "SPEAR", "WALKING STICK"};
    LookSheet sh(10, 10), fs(10, 10);
    for (int w = 0; w < 10; w++) {
      HumanLook L = plainLook(w & 1);
      L.weapon = (uint8_t)w;
      sh.label(w, wn[w]);
      sh.idleAndWalk(L, w, 2);
      const art::RigSheet d = art::humanRig(L, art::RigPart::Death);
      fs.label(w, wn[w]);
      for (int fc = 0; fc < 2; fc++)
        for (int i = 0; i < 3; i++) fs.put(d, fc == 0 ? 0 : 2, art::Clip::Fall, fc, i, fc * 4 + i, w);
      fs.put(d, 0, art::Clip::Lying, 0, 0, 3, w);
      fs.put(d, 0, art::Clip::Lying, 1, 0, 8, w);
    }
    rigpv::saveBoth(sh.b, dir, "rig_carry");
    rigpv::saveBoth(fs.b, dir, "rig_carryfall");
  }
  {   // kids and builds: youth and child (boy, girl), slim / broad, idle x 4 and a side walk; then their fall and lying
    struct K { const char* name; int body, age, build, people; art::Hair hair; art::Outfit o; int cut; };
    const K ks[] = {{"YOUTH (M)", 0, 2, 0, 0, art::Hair::Short, art::Outfit::Tunic, 0}, {"YOUTH (W)", 1, 2, 0, 0, art::Hair::Ponytail, art::Outfit::Dress, 0},
                    {"CHILD (M)", 0, 1, 0, 0, art::Hair::Curls, art::Outfit::Tunic, 0}, {"CHILD (W)", 1, 1, 0, 0, art::Hair::Braids, art::Outfit::Dress, 0},
                    {"CHILD, KILT", 0, 1, 0, 0, art::Hair::Short, art::Outfit::Tunic, 6}, {"SLIM (M)", 0, 0, 1, 0, art::Hair::Short, art::Outfit::Tunic, 0},
                    {"BROAD (M)", 0, 0, 2, 0, art::Hair::Short, art::Outfit::Tunic, 0}, {"SLIM (W)", 1, 0, 1, 0, art::Hair::Long, art::Outfit::Dress, 0},
                    {"BROAD (W)", 1, 0, 2, 0, art::Hair::Bun, art::Outfit::Tunic, 0}};
    const int n = (int)(sizeof ks / sizeof ks[0]);
    LookSheet sh(10, n);
    for (int k = 0; k < n; k++) {
      HumanLook L = plainLook(ks[k].body == 1);
      L.age = (uint8_t)ks[k].age; L.build = (uint8_t)ks[k].build; L.hair = ks[k].hair; L.outfit = ks[k].o; L.cut = (uint8_t)ks[k].cut;
      L.patternColor = rgba(170, 50, 50);
      sh.label(k, ks[k].name);
      sh.idleAndWalk(L, k, 2);
    }
    rigpv::saveBoth(sh.b, dir, "rig_kin");
    LookSheet fs(10, n);
    for (int k = 0; k < n; k++) {
      HumanLook L = plainLook(ks[k].body == 1);
      L.age = (uint8_t)ks[k].age; L.build = (uint8_t)ks[k].build; L.hair = ks[k].hair; L.outfit = ks[k].o; L.cut = (uint8_t)ks[k].cut;
      const art::RigSheet d = art::humanRig(L, art::RigPart::Death);
      const art::RigSheet lo = art::humanRig(L, art::RigPart::Loco);
      fs.label(k, ks[k].name);
      fs.put(lo, 0, art::Clip::Hurt, 0, 0, 0, k);
      fs.put(lo, 2, art::Clip::Hurt, 0, 0, 1, k);
      for (int i = 0; i < 3; i++) fs.put(d, 0, art::Clip::Fall, 0, i, 2 + i, k);
      fs.put(d, 0, art::Clip::Lying, 0, 0, 6, k);
      for (int i = 0; i < 2; i++) fs.put(d, 2, art::Clip::Fall, 1, 1 + i, 7 + i, k);
      fs.put(d, 0, art::Clip::Lying, 1, 0, 9, k);
    }
    rigpv::saveBoth(fs.b, dir, "rig_kinfall");
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::printf("usage: rig_gallery <outDir> [dump who part facing clip variant i]\n"); return 2; }
  const std::string dir = argv[1];
  art::setRigVersion(2);
  const art::HumanLook who[3] = {rigpv::sliceHero(false), rigpv::sliceHero(true), rigpv::sliceVillager()};
  if (argc >= 3 && !std::strcmp(argv[2], "check")) {   // the slice's frame-level numbers (the harness's own measures)
    // the Steel Arc (STYLE_P2 6, 12): per frame its box and both named colours, in the 32 px cell and the finisher's 38
    for (int size : {32, 38}) {
      std::printf("steel arc %d x %d:", size, size);
      for (int f = 0; f < 4; f++) {
        Canvas a(size, size);
        art::rig2::steelArc(a, f);
        int x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1;
        bool core = false, edge = false;
        for (int y = 0; y < a.h; y++)
          for (int x = 0; x < a.w; x++) {
            const uint32_t p = a.get(x, y);
            if (!(p >> 24)) continue;
            x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y);
            if ((p & 0xFFFFFF) == (rgba(244, 241, 230) & 0xFFFFFF)) core = true;
            if ((p & 0xFFFFFF) == (rgba(184, 196, 216) & 0xFFFFFF)) edge = true;
          }
        std::printf("  f%d box %dx%d core %d edge %d", f, x1 - x0 + 1, y1 - y0 + 1, (int)core, (int)edge);
      }
      std::printf("\n");
    }
    auto cutF = [](const art::RigSheet& s, int fc, art::Clip c, int v, int i, Canvas& out, int& px, int& py) {
      out = rigpv::frameCanvas(s, fc, c, v, i, px, py);
      return out.w > 0;
    };
    auto opaqueBox = [](const Canvas& c, int& x0, int& y0, int& x1, int& y1) {
      x0 = y0 = 1 << 20; x1 = y1 = -1;
      for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) if (c.get(x, y) >> 24) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
    };
    auto differ = [](const Canvas& a, int apx, int apy, const Canvas& b, int bpx, int bpy) {   // test_rig.cpp's measure
      int un = 0, dif = 0;
      const int x0 = -std::max(apx, bpx), x1 = std::max(a.w - apx, b.w - bpx), y0 = -std::max(apy, bpy), y1 = std::max(a.h - apy, b.h - bpy);
      for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
          const uint32_t pa = a.get(apx + x, apy + y), pb = b.get(bpx + x, bpy + y);
          const bool oa = (pa >> 24) != 0, ob = (pb >> 24) != 0;
          if (oa || ob) un++;
          if (oa != ob || (oa && pa != pb)) dif++;
        }
      return un ? (float)dif / un : 0.0f;
    };
    const char* fn[4] = {"down", "up", "right", "left"};
    for (int k = 0; k < 3; k++) {
      const art::RigSheet lo = art::humanRig(who[k], art::RigPart::Loco), de = art::humanRig(who[k], art::RigPart::Death);
      std::printf("look %d (%s): LOCO %d KB, DEATH %d KB\n", k, k == 0 ? "hero man" : k == 1 ? "hero woman" : "villager", (int)(lo.bytes() / 1024), (int)(de.bytes() / 1024));
      for (int fc = 0; fc < 4; fc++) {
        Canvas c; int px, py, x0, y0, x1, y1;
        cutF(lo, fc, art::Clip::Idle, 0, 0, c, px, py);
        opaqueBox(c, x0, y0, x1, y1);
        // the body's height: the tallest column that is not the weapon (the crown's outline to the sole's)
        std::printf("  %-5s idle: frame %dx%d, opaque rows %d..%d (%d px), walk %d frames, idle %d, hurt %d\n", fn[fc], c.w, c.h, y0 - py, y1 - py, y1 - y0 + 1,
                    lo.count(fc, art::Clip::Walk), lo.count(fc, art::Clip::Idle), lo.count(fc, art::Clip::Hurt));
      }
      // the shoulder width in the down idle: the widest opaque run on the shoulder rows (incl. the outline)
      {
        art::HumanLook L = who[k]; L.weapon = 0;
        const art::RigSheet u = art::humanRig(L, art::RigPart::Loco);
        Canvas c; int px, py;
        cutF(u, 0, art::Clip::Idle, 0, 0, c, px, py);
        const art::rig2::Body B = art::rig2::bodyOf(L);
        int best = 0;
        for (int y = py + B.shoulder; y <= py + B.shoulder + 1; y++) {
          int l = -1, r = -1;
          for (int x = 0; x < c.w; x++) if (c.get(x, y) >> 24) { if (l < 0) l = x; r = x; }
          best = std::max(best, r - l + 1);
        }
        const art::RigAnchors* an = u.anchorsOf(0, art::Clip::Idle, 0, 0);
        std::printf("  unarmed down idle: shoulder line %d px wide (incl. outline); hands at %d,%d / %d,%d; feet %d / %d; hip row %d\n", best,
                    an ? an->handRX : 0, an ? an->handRY : 0, an ? an->handLX : 0, an ? an->handLY : 0, an ? an->footRX : 0, an ? an->footLX : 0, an ? an->hipY : 0);
      }
      if (k < 2) {
        for (int v = 0; v < 2; v++) {
          Canvas c; int px, py, x0, y0, x1, y1;
          cutF(de, 0, art::Clip::Lying, v, 0, c, px, py);
          opaqueBox(c, x0, y0, x1, y1);
          std::printf("  lying (head %s): %d x %d px\n", v ? "east" : "west", x1 - x0 + 1, y1 - y0 + 1);
        }
        // the topple leans to the side the body then lies on (the judge's SLICE-FALL-SIDE): variant 0's opaque mass
        // left of the pivot (west), variant 1's right (east), in every facing; the buckle leans the same way
        {
          int bad = 0;
          std::string line;
          for (int fc = 0; fc < 4; fc++)
            for (int v = 0; v < 2; v++)
              for (int i = 1; i <= 2; i++) {
                Canvas c; int px, py;
                if (!cutF(de, fc, art::Clip::Fall, v, i, c, px, py)) { bad++; continue; }
                long sx = 0, n = 0;
                for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) if (c.get(x, y) >> 24) { sx += x - px; n++; }
                const float m = n ? (float)sx / n : 0.0f;
                if (i == 2) { char t[48]; std::snprintf(t, sizeof t, " %s/%s %+.1f", fn[fc], v ? "E" : "W", m); line += t; }
                if ((v == 0 && m >= 0) || (v == 1 && m <= 0)) { bad++; std::printf("  FALL %s variant %d frame %d: mass %+.1f px from the pivot (wrong side)\n", fn[fc], v, i, m); }
              }
          std::printf("  FALL sides (topple mass from the pivot):%s; %d wrong\n", line.c_str(), bad);
        }
        // the sword: R8 (the arc about the grip, side-on), R9 (variants differ), R10 (left painted, weapon in the right hand)
        art::HumanLook L = who[k];
        const art::RigSheet ac = art::rig2::sliceSwordAction(L);
        std::printf("  sword ACTION %d KB, %d frames\n", (int)(ac.bytes() / 1024), (int)ac.frames.size());
        for (int v = 0; v < 3; v++) {
          float tot = 0;
          float pa = 0;
          bool first = true;
          for (art::Clip cl : {art::Clip::Windup, art::Clip::Strike, art::Clip::Follow})
            for (int i = 0; i < ac.count(2, cl, v); i++) {
              const art::RigAnchors* an = ac.anchorsOf(2, cl, v, i);
              const float a = std::atan2((float)(an->tipY - an->gripY), (float)(an->tipX - an->gripX));
              if (!first) { float d = a - pa; while (d > 3.14159265f) d -= 6.2831853f; while (d < -3.14159265f) d += 6.2831853f; tot += d; }
              first = false; pa = a;
            }
          std::printf("  R8 hit %d: the tip turns %.0f deg about the grip (side-on)\n", v + 1, std::fabs(tot) * 57.29578f);
        }
        // the slice judge's swing measures (round 1): per facing, the tip angle about the grip; every pair of variants
        // 45 degrees or more apart on windup frame 0 and on at least one strike frame; side-on, hits 1 and 2 strike with
        // the tip within 4 px of the shoulder row; hit 3 runs from 80 degrees or more down to -45 or less
        {
          auto ang = [&](int fc, art::Clip cl, int v, int i) {
            const art::RigAnchors* an = ac.anchorsOf(fc, cl, v, i);
            if (!an || an->tipX == art::kNoAnchor) return 999.0f;
            return std::atan2((float)-(an->tipY - an->gripY), (float)(an->tipX - an->gripX)) * 57.29578f;
          };
          auto sep = [](float a, float b) { float d = std::fabs(a - b); while (d > 360) d -= 360; return d > 180 ? 360 - d : d; };
          const art::rig2::Body B = art::rig2::bodyOf(L);
          int fails = 0;
          for (int fc = 0; fc < 4; fc++) {
            for (int va = 0; va < 3; va++)
              for (int vb = va + 1; vb < 3; vb++) {
                const float w = sep(ang(fc, art::Clip::Windup, va, 0), ang(fc, art::Clip::Windup, vb, 0));
                const float s = std::max(sep(ang(fc, art::Clip::Strike, va, 0), ang(fc, art::Clip::Strike, vb, 0)),
                                         sep(ang(fc, art::Clip::Strike, va, 1), ang(fc, art::Clip::Strike, vb, 1)));
                if (w < 45 || s < 45) { fails++; std::printf("  TIPS %-5s hits %d/%d: windup0 %.0f, best strike %.0f deg apart (45)\n", fn[fc], va + 1, vb + 1, w, s); }
              }
            if (fc >= 2) {
              for (int v = 0; v < 2; v++)
                for (int i = 0; i < 2; i++) {
                  const art::RigAnchors* an = ac.anchorsOf(fc, art::Clip::Strike, v, i);
                  if (!an || std::abs(an->tipY - B.shoulder) > 4) { fails++; std::printf("  TIPS %-5s hit %d strike %d: tip row %d, shoulder %d (4)\n", fn[fc], v + 1, i, an ? an->tipY : 0, B.shoulder); }
                }
              float hi = -999, lo = 999;
              for (art::Clip cl : {art::Clip::Windup, art::Clip::Strike})
                for (int i = 0; i < 2; i++) {
                  float a = ang(fc, cl, 2, i);
                  if (fc == 3) a = 180 - a;   // facing left: measured from pointing forward (left)
                  while (a > 180) a -= 360;
                  while (a <= -180) a += 360;
                  hi = std::max(hi, a); lo = std::min(lo, a);
                }
              if (hi < 80 || lo > -45) { fails++; std::printf("  TIPS %-5s hit 3: %.0f .. %.0f (80 .. -45)\n", fn[fc], hi, lo); }
            }
          }
          std::printf("  TIPS: %d failures (windup 0 and strike separations per facing, side tip rows, hit 3's drop)\n", fails);
        }
        for (int fc = 0; fc < 4; fc++) {
          float mn = 1;
          for (int va = 0; va < 3; va++)
            for (int vb = va + 1; vb < 3; vb++)
              for (int si = 0; si < 2; si++) {   // R9 on BOTH strike frames
                Canvas a, b; int apx, apy, bpx, bpy;
                cutF(ac, fc, art::Clip::Strike, va, si, a, apx, apy);
                cutF(ac, fc, art::Clip::Strike, vb, si, b, bpx, bpy);
                mn = std::min(mn, differ(a, apx, apy, b, bpx, bpy));
              }
          int bad = 0, n = 0;
          for (art::Clip cl : {art::Clip::Windup, art::Clip::Strike, art::Clip::Follow})
            for (int v = 0; v < 3; v++)
              for (int i = 0; i < ac.count(fc, cl, v); i++) {
                const art::RigAnchors* an = ac.anchorsOf(fc, cl, v, i);
                n++;
                if (!an || std::abs(an->gripX - an->handRX) > 2 || std::abs(an->gripY - an->handRY) > 2) bad++;
              }
          std::printf("  R9 %-5s: strike variants differ by %.0f %% at least on both strike frames (25); weapon in the right hand %d / %d frames\n", fn[fc], mn * 100, n - bad, n);
        }
        // R10: left frames are not the right ones mirrored (LOCO and ACTION)
        int same = 0, tot = 0;
        for (const art::RigSheet* s : {&lo, &ac})
          for (int c = 0; c < art::CLIP_COUNT; c++)
            for (int v = 0; v < 3; v++)
              for (int i = 0; i < s->count(2, (art::Clip)c, v); i++) {
                Canvas r, l; int rpx, rpy, lpx, lpy;
                if (!cutF(*s, 2, (art::Clip)c, v, i, r, rpx, rpy) || !cutF(*s, 3, (art::Clip)c, v, i, l, lpx, lpy)) continue;
                Canvas m(r.w, r.h);
                for (int y = 0; y < r.h; y++) for (int x = 0; x < r.w; x++) m.set(r.w - 1 - x, y, r.get(x, y));
                tot++;
                if (differ(l, lpx, lpy, m, r.w - 1 - rpx, rpy) < 0.05f) same++;
              }
        std::printf("  R10: %d of %d left frames are a mirrored right frame (0)\n", same, tot);
      }
    }
    return 0;
  }
  if (argc >= 3 && !std::strcmp(argv[2], "bench")) {   // bake time: LOCO per facing (R6), DEATH, the slice's swings
    {
      const art::HumanLook L = who[0];
      const art::rig2::Body B = art::rig2::bodyOf(L);
      art::rig2::Kit K;
      art::rig2::makeKit(L, K);
      const art::rig2::Pose P = art::rig2::restPose(B, 0, true);
      const int N = 2000;
      auto t0 = std::chrono::steady_clock::now();
      for (int i = 0; i < N; i++) { art::RigAnchors an; const Canvas c = art::rig2::paintRaw(L, B, K, P, 48, 48, 44, an); (void)c; }
      auto t1 = std::chrono::steady_clock::now();
      for (int i = 0; i < N; i++) { const art::rig2::Cut c = art::rig2::paintFrame(L, B, K, P, 48, 48, 44); (void)c; }
      auto t2 = std::chrono::steady_clock::now();
      for (int i = 0; i < N / 20; i++) { art::rig2::Kit k2; art::rig2::makeKit(L, k2); }
      auto t3 = std::chrono::steady_clock::now();
      // the packer alone: 40 distinct frames of a typical size
      auto t4 = std::chrono::steady_clock::now();
      for (int r = 0; r < 50; r++) {
        art::rig2::SheetBuilder sb;
        for (int i = 0; i < 40; i++) {
          art::rig2::Pose Q = P;
          Q.hLy = (int8_t)(Q.hLy - (i % 3));
          Q.fLy = (int8_t)(i % 2);
          Q.headX = (int8_t)((i / 6) % 2);
          Q.dy = (int8_t)((i / 12) % 2);
          sb.add(i % 4, art::Clip::Walk, 0, art::rig2::paintFrame(L, B, K, Q, 48, 48, 44));
        }
        const art::RigSheet sh = sb.finish();
        (void)sh;
      }
      auto t5 = std::chrono::steady_clock::now();
      std::printf("40 frames + pack: %.3f ms\n", std::chrono::duration<double, std::milli>(t5 - t4).count() / 50);
      std::printf("one frame: paint %.2f us, paint + outline + trim %.2f us; makeKit %.2f us\n", std::chrono::duration<double, std::micro>(t1 - t0).count() / N,
                  std::chrono::duration<double, std::micro>(t2 - t1).count() / N, std::chrono::duration<double, std::micro>(t3 - t2).count() / (N / 20));
    }
    for (int k = 0; k < 3; k++) {
      double lo = 0, de = 0, ac = 0, v1 = 0;
      const int N = 60;
      for (int i = 0; i < N; i++) {
        art::HumanLook L = who[k];
        L.topColor = rgba(40 + i, 90, 160);   // a fresh look each time (no cache anywhere)
        auto t0 = std::chrono::steady_clock::now();
        const art::RigSheet a = art::humanRig(L, art::RigPart::Loco);
        auto t1 = std::chrono::steady_clock::now();
        const art::RigSheet b = art::humanRig(L, art::RigPart::Death);
        auto t2 = std::chrono::steady_clock::now();
        const art::RigSheet c = art::rig2::sliceSwordAction(L);
        auto t3 = std::chrono::steady_clock::now();
        art::setRigVersion(1);
        const Canvas d = art::humanSheet(L);
        art::setRigVersion(2);
        auto t4 = std::chrono::steady_clock::now();
        lo += std::chrono::duration<double, std::milli>(t1 - t0).count();
        de += std::chrono::duration<double, std::milli>(t2 - t1).count();
        ac += std::chrono::duration<double, std::milli>(t3 - t2).count();
        v1 += std::chrono::duration<double, std::milli>(t4 - t3).count();
        if (i == 0) std::printf("look %d: LOCO %d KB (%dx%d, %d frames), DEATH %d KB, ACTION %d KB\n", k, (int)(a.bytes() / 1024), a.atlas.w, a.atlas.h,
                                (int)a.frames.size(), (int)(b.bytes() / 1024), (int)(c.bytes() / 1024));
      }
      std::printf("look %d: LOCO %.3f ms (%.3f per facing), DEATH %.3f, ACTION %.3f, v1 sheet %.3f ms (means of %d)\n", k, lo / N, lo / N / 4, de / N, ac / N, v1 / N, N);
    }
    return 0;
  }
  if (argc >= 4 && !std::strcmp(argv[2], "heroframe")) {   // the in-game hero of a seed with the scripts' `skill kit sword none`:
    // its look and its down idle frame at 1x (p2_measure.mjs subset's FRAME.png for the slice shots)
    const uint64_t seed = (uint64_t)std::atoll(argv[3]);
    Game g(seed);
    g.newEndlessGame(seed);
    g.mode = Mode::Play;
    for (int f = 0; f < 30; f++) { g.update(SIM_DT, Input()); g.events.clear(); g.mode = Mode::Play; }
    Rng r(0x5C1Du + (uint32_t)skills::Skill::Sword);   // script_skills.cpp `skill kit sword none`, verbatim
    g.eqWeapon = g.eqBow = g.eqArmor = g.eqHelmet = g.eqShield = g.eqGloves = g.eqBoots = -1;
    g.inv.push_back(gear::makeGear(r, ItemKind::Weapon, (int)WeaponType::Sword, std::max(1, g.plLevel), Rarity::Common, 0, Mat::Iron));
    g.eqWeapon = (int)g.inv.size() - 1;
    SkillOps::recalc(g);
    for (int f = 0; f < 5; f++) { g.update(SIM_DT, Input()); g.events.clear(); g.mode = Mode::Play; }
    for (int body = 0; body < 2; body++) {
      // the woman exactly as the slice scripts make her: `body woman` (Appearance::female) and `hair braids chestnut`
      // (rpg/view/script_slice.cpp), the look rebuilt by the sim
      if (body) { g.app.female = true; g.app.hair = (uint8_t)art::Hair::Braids; g.app.hairColor = rgba(120, 70, 36); AnimOps::refreshHero(g); }
      art::HumanLook L = g.pl().look;
      std::printf("hero (%s): top %08x bottom %08x weapon %d %08x hair %d covered %d\n", body ? "woman" : "man", L.topColor, L.bottomColor, L.weapon, L.weaponColor,
                  (int)L.hair, (int)art::rig2Covers(L));
      const art::RigSheet s = art::humanRig(L, art::RigPart::Loco);
      int px, py;
      const Canvas f = rigpv::frameCanvas(s, 0, art::Clip::Idle, 0, 0, px, py);
      savePng(f, dir + (body ? "/hero_woman_down_idle.png" : "/hero_down_idle.png"), 1);
    }
    return 0;
  }
  if (argc >= 4 && !std::strcmp(argv[2], "census")) {   // the looks round the start of a seed: what rig 2 covers
    const uint64_t seed = (uint64_t)std::atoll(argv[3]);
    Game g(seed);
    g.newEndlessGame(seed);
    g.mode = Mode::Play;
    for (int f = 0; f < 120; f++) { g.update(SIM_DT, Input()); g.events.clear(); g.mode = Mode::Play; }
    const Vec2 p0 = g.pl().p;
    int n = 0, cov = 0;
    for (const Actor& x : g.actors) {
      if (!x.human) continue;
      const float d = std::sqrt(len2(x.p - p0)) / 16.0f;
      if (d > 40) continue;
      const art::HumanLook& L = x.look;
      const bool c = art::rig2Covers(L);
      n++; cov += c;
      std::printf("%5.1f t  role %2d body %d age %d ppl %d bld %d  outfit %d cut %d pat %d hair %d hw %d jw %d fp %d beard %d wpn %d helm %d/%d hood %d cape %d cloak %d shld %d arm %d  %s %s (%.0f,%.0f)\n", d,
                  (int)x.role, L.body, L.age, L.people, L.build, (int)L.outfit, L.cut, L.pattern, (int)L.hair, L.headwear, L.jewellery, L.facePaint,
                  (int)L.beard, L.weapon, (int)L.helmet, L.helmStyle, (int)L.hood, (int)L.cape, L.cloak, (int)L.shield, L.armorStyle, c ? "COVERED" : "", x.name.c_str(), (x.p.x - p0.x) / 16.0f, (x.p.y - p0.y) / 16.0f);
    }
    std::printf("%d of %d humans within 40 tiles covered\n", cov, n);
    return 0;
  }
  if (argc >= 3 && !std::strcmp(argv[2], "base")) { baseBoards(dir); return 0; }   // only RIG-BODY's base boards
  if (argc >= 3 && !std::strcmp(argv[2], "prof")) {   // R6's bake measure: test_rig.cpp gateSizes's own loop, verbatim
    // (v1 sheet, LOCO timed, then ACTION and DEATH), its p50 / p95, three runs
    for (int run = 0; run < 3; run++) {
      std::vector<double> v1ms, locoMs;
      for (int i = 0; i < 20; i++) {
        art::HumanLook L;
        L.body = (uint8_t)(i & 1); L.build = (uint8_t)(i % 3); L.people = (uint8_t)((i / 3) % 3);
        L.skin = rgba(236, 188, 146); L.hairColor = rgba(120, 70, 36);
        L.hair = (art::Hair)(i % 8); L.topColor = rgba(40 + i * 9, 90, 160 - i * 4); L.weapon = (uint8_t)(i % 9);
        auto t0 = std::chrono::steady_clock::now();
        art::setRigVersion(1);
        const Canvas s1 = art::humanSheet(L);
        art::setRigVersion(2);
        v1ms.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
        t0 = std::chrono::steady_clock::now();
        const art::RigSheet lo = art::humanRig(L, art::RigPart::Loco);
        locoMs.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 4);
        const art::RigSheet ac = art::humanRig(L, art::RigPart::Action), de = art::humanRig(L, art::RigPart::Death);
        (void)s1;
      }
      std::sort(v1ms.begin(), v1ms.end());
      std::sort(locoMs.begin(), locoMs.end());
      std::printf("gate loop: v1 p50 %.3f p95 %.3f; one facing's LOCO p50 %.3f ms\n", v1ms[10], v1ms[19], locoMs[10]);
    }
    return 0;
  }
  if (argc >= 3 && !std::strcmp(argv[2], "sizes")) {   // R6 per look: LOCO / DEATH KB and the packing's waste, by what
    // the look carries or wears (the widest base looks: weapons, headwear, hair, cuts)
    auto report = [](const char* what, const art::HumanLook& L) {
      auto t0 = std::chrono::steady_clock::now();
      for (int k = 0; k < 20; k++) { const art::RigSheet x = art::humanRig(L, art::RigPart::Loco); (void)x; }
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 20 / 4;
      std::printf("[%.3f ms/facing] ", ms);
      const art::RigSheet lo = art::humanRig(L, art::RigPart::Loco), de = art::humanRig(L, art::RigPart::Death);
      auto used = [](const art::RigSheet& s) {
        long a = 0;
        std::vector<int> seen;
        for (const art::FrameRect& r : s.frames) {
          const int key = r.x * 65536 + r.y;
          if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
          seen.push_back(key);
          a += (long)r.w * r.h;
        }
        return a;
      };
      std::printf("%-22s LOCO %3d KB (%3.0f %% used)  DEATH %3d KB (%3.0f %% used)\n", what, (int)(lo.bytes() / 1024),
                  lo.bytes() ? 400.0 * used(lo) / lo.bytes() : 0.0, (int)(de.bytes() / 1024), de.bytes() ? 400.0 * used(de) / de.bytes() : 0.0);
    };
    static const char* wn[10] = {"none", "sword", "axe", "bow", "staff", "dagger", "mace", "greatsword", "spear", "stick"};
    for (int i = 0; i < 20; i++) {   // rpg_test --rig R6's twenty looks (test_rig.cpp gateSizes), verbatim
      art::HumanLook L;
      L.body = (uint8_t)(i & 1); L.build = (uint8_t)(i % 3); L.people = (uint8_t)((i / 3) % 3); L.age = 0;
      L.skin = rgba(236, 188, 146); L.hairColor = rgba(120, 70, 36);
      L.hair = (art::Hair)(i % 8); L.topColor = rgba(40 + i * 9, 90, 160 - i * 4); L.weapon = (uint8_t)(i % 9);
      char t[64]; std::snprintf(t, sizeof t, "R6 #%d b%d bl%d p%d h%d %s", i, i & 1, i % 3, (i / 3) % 3, i % 8, wn[i % 9]);
      report(t, L);
    }
    for (int w = 0; w < 10; w++) { art::HumanLook L = plainLook(false); L.weapon = (uint8_t)w; report((std::string("weapon ") + wn[w]).c_str(), L); }
    for (int h = 1; h <= 8; h++) { art::HumanLook L = plainLook(true); L.hair = art::Hair::Long; L.headwear = (uint8_t)h; report((std::string("headwear ") + std::to_string(h)).c_str(), L); }
    for (int h = 0; h < 8; h++) { art::HumanLook L = plainLook(true); L.hair = (art::Hair)h; report((std::string("hair ") + std::to_string(h)).c_str(), L); }
    for (int c = 1; c <= 8; c++) { art::HumanLook L = plainLook(true); L.cut = (uint8_t)c; L.outfit = art::Outfit::Dress; report((std::string("cut ") + std::to_string(c)).c_str(), L); }
    { art::HumanLook L = plainLook(false); L.build = 2; L.hair = art::Hair::Curls; L.headwear = 7; L.outfit = art::Outfit::Robe; report("broad, cone, robe", L); }
    if (argc >= 4) {   // the frames of one weapon's DEATH and LOCO atlases (w x h, shared frames once)
      art::HumanLook L = plainLook(false);
      L.weapon = (uint8_t)std::atoi(argv[3]);
      for (int part : {0, 2}) {
        const art::RigSheet s = art::humanRig(L, (art::RigPart)part);
        std::printf("%s:", part ? "DEATH" : "LOCO");
        for (int fc = 0; fc < 4; fc++)
          for (int c = 0; c < art::CLIP_COUNT; c++)
            for (int v = 0; v < 2; v++)
              for (int i = 0; i < s.count(fc, (art::Clip)c, v); i++) {
                if (v && s.frame(fc, (art::Clip)c, v, 0) == s.frame(fc, (art::Clip)c, 0, 0)) continue;
                const art::FrameRect* r = s.frame(fc, (art::Clip)c, v, i);
                std::printf(" %s%d.%d.%d=%dx%d", art::clipName((art::Clip)c), fc, v, i, r->w, r->h);
              }
        std::printf("\n");
      }
    }
    return 0;
  }
  if (argc >= 3 && !std::strcmp(argv[2], "hash")) {   // the locked slice's atlases, hashed (pixels, rects, pivots, anchors):
    // every RIG-BODY change after the slice gate must leave these lines unchanged
    auto fnv = [](uint64_t h, const void* p, size_t n) {
      const uint8_t* b = (const uint8_t*)p;
      for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; }
      return h;
    };
    // the pose data field by field (RigAnchors ends in a bool: its padding byte is whatever the stack held)
    auto anHash = [&](uint64_t h, const art::RigAnchors& a) {
      const int16_t v[18] = {a.headX, a.headY, a.headW, a.headH, a.faceX, a.faceY, a.handRX, a.handRY, a.handLX, a.handLY,
                             a.hipX, a.hipY, a.footLX, a.footRX, a.gripX, a.gripY, a.tipX, a.tipY};
      h = fnv(h, v, sizeof v);
      const uint8_t b = a.weaponBehind ? 1 : 0;
      return fnv(h, &b, 1);
    };
    auto sheetHash = [&](const art::RigSheet& s) {
      uint64_t h = 1469598103934665603ull;
      h = fnv(h, &s.atlas.w, sizeof s.atlas.w);
      h = fnv(h, &s.atlas.h, sizeof s.atlas.h);
      if (!s.atlas.px.empty()) h = fnv(h, s.atlas.px.data(), s.atlas.px.size() * 4);
      for (const art::FrameRect& r : s.frames) h = fnv(h, &r, sizeof r);
      for (const art::RigAnchors& a : s.anchors) h = anHash(h, a);
      for (const auto& sp : s.spans) { h = fnv(h, &sp.first, sizeof sp.first); h = fnv(h, &sp.count, sizeof sp.count); }
      return h;
    };
    const char* nm[5] = {"hero man", "hero woman", "villager", "man unarmed", "woman unarmed"};
    for (int k = 0; k < 5; k++) {
      art::HumanLook L = k < 3 ? who[k] : rigpv::sliceHero(k == 4);
      if (k >= 3) L.weapon = 0;
      std::printf("%-14s LOCO %016llx  DEATH %016llx  ACTION %016llx\n", nm[k], (unsigned long long)sheetHash(art::humanRig(L, art::RigPart::Loco)),
                  (unsigned long long)sheetHash(art::humanRig(L, art::RigPart::Death)), (unsigned long long)sheetHash(sheetOf(L, art::RigPart::Action)));
    }
    // per clip, by frame content (pixels, size, pivot, anchors), so a change to one clip shows alone
    for (int k = 0; k < 5; k++) {
      art::HumanLook L = k < 3 ? who[k] : rigpv::sliceHero(k == 4);
      if (k >= 3) L.weapon = 0;
      std::printf("%-14s", nm[k]);
      for (int part = 0; part < 3; part++) {
        const art::RigSheet s = sheetOf(L, (art::RigPart)part);
        for (int c = 0; c < art::CLIP_COUNT; c++) {
          uint64_t h = 1469598103934665603ull;
          int n = 0;
          for (int fc = 0; fc < 4; fc++)
            for (int v = 0; v < 3; v++)
              for (int i = 0; i < s.count(fc, (art::Clip)c, v); i++) {
                int px, py;
                const Canvas f = rigpv::frameCanvas(s, fc, (art::Clip)c, v, i, px, py);
                h = fnv(h, &f.w, 4); h = fnv(h, &f.h, 4); h = fnv(h, &px, 4); h = fnv(h, &py, 4);
                if (!f.px.empty()) h = fnv(h, f.px.data(), f.px.size() * 4);
                const art::RigAnchors* an = s.anchorsOf(fc, (art::Clip)c, v, i);
                if (an) h = anHash(h, *an);
                n++;
              }
          if (n) std::printf(" %s:%04llx", art::clipName((art::Clip)c), (unsigned long long)(h & 0xFFFF));
        }
      }
      std::printf("\n");
    }
    return 0;
  }
  if (argc >= 5 && !std::strcmp(argv[2], "fields")) {   // R3's census (rpg_test --rig-coverage's plain looks): what each
    // uncovered look carries, field by field, so the painters know what is left (seeds A..B)
    const int a = std::atoi(argv[3]), bEnd = std::atoi(argv[4]);
    std::map<std::string, int> tally;
    int plain = 0, cov = 0;
    for (int s = a; s <= bEnd; s++) {
      Game g((uint64_t)s);
      g.newEndlessGame((uint64_t)s);
      g.mode = Mode::Play;
      for (int f = 0; f < 60; f++) { g.update(SIM_DT, Input()); g.events.clear(); g.mode = Mode::Play; }
      for (const Actor& x : g.actors) {
        if (!x.human) continue;
        const art::HumanLook& L = x.look;
        const bool armour = L.armorStyle || L.outfit == art::Outfit::Leather || L.outfit == art::Outfit::Chain || L.outfit == art::Outfit::Plate ||
                            L.outfit == art::Outfit::Elven || L.outfit == art::Outfit::Ebony || L.outfit == art::Outfit::Guard;
        if (armour || L.helmet || L.helmStyle || L.cloak || L.cape || L.shield || L.shieldStyle || L.bladeForm || L.polearmForm || L.bowForm) continue;
        plain++;
        if (art::rig2Covers(L)) { cov++; continue; }
        char t[64];
        auto add = [&](const char* k, int v) { if (v) { std::snprintf(t, sizeof t, "%s %d", k, v); tally[t]++; } };
        add("age", L.age); add("people", L.people); add("build", L.build); add("outfit", (int)L.outfit + 100); add("cut", L.cut); add("pattern", L.pattern);
        add("hair", (int)L.hair + 100); add("headwear", L.headwear); add("hood", L.hood); add("facePaint", L.facePaint); add("jewellery", L.jewellery);
        add("beard", L.beard); add("weapon", L.weapon); add("gloves", L.gloves); add("boots", L.boots); add("backItem", L.backItem); add("amulet", L.amulet);
        add("ring", L.ring); add("glow", L.glow); add("armsOrnament", L.armsOrnament); add("armourTint", L.armourTint ? 1 : 0); add("helmForm", L.helmForm);
        add("bodyForm", L.bodyForm); add("shieldForm", L.shieldForm); add("pauldron", L.pauldron); add("skirt", L.skirt); add("crest", L.crest);
        add("sheen", L.sheen); add("body", L.body + 10);
      }
    }
    std::printf("%d of %d plain looks covered (%.1f %%); fields of the uncovered (outfit / hair: value + 100; body + 10):\n", cov, plain, plain ? 100.0 * cov / plain : 0.0);
    for (auto& kv : tally) std::printf("  %-16s %5d\n", kv.first.c_str(), kv.second);
    return 0;
  }
  if (argc >= 9 && !std::strcmp(argv[2], "dump")) {
    art::HumanLook L = who[std::atoi(argv[3]) % 3];
    if (std::atoi(argv[3]) >= 3) { L = rigpv::sliceHero(std::atoi(argv[3]) == 4); L.weapon = 0; }   // 3 / 4: the unarmed man / woman
    if (argc >= 10) L.hair = (art::Hair)std::atoi(argv[9]);   // [hair style]
    if (argc >= 11) L.people = (uint8_t)std::atoi(argv[10]);  // [people]
    const art::RigSheet s = sheetOf(L, (art::RigPart)std::atoi(argv[4]));
    std::printf("atlas %d x %d (%d KB), %d frames\n", s.atlas.w, s.atlas.h, (int)(s.bytes() / 1024), (int)s.frames.size());
    dumpFrame(s, std::atoi(argv[5]), (art::Clip)std::atoi(argv[6]), std::atoi(argv[7]), std::atoi(argv[8]));
    return 0;
  }
  {
    Board b(1500, 1420);
    const char* names[3] = {"HERO (MAN)", "HERO (WOMAN)", "VILLAGER (DRESS, S6, WHEAT)"};
    int y = 60;
    for (int k = 0; k < 3; k++) {
      const art::RigSheet loco = art::humanRig(who[k], art::RigPart::Loco);
      const art::RigSheet death = art::humanRig(who[k], art::RigPart::Death);
      b.text(4, y - 50, names[k]);
      int x = rigpv::clipRow(b, loco, art::Clip::Idle, 0, 140, y, 40, "IDLE / HURT");
      x = rigpv::clipRow(b, loco, art::Clip::Hurt, 0, x + 20, y, 40, "");
      y += 56;
      rigpv::clipRow(b, loco, art::Clip::Walk, 0, 140, y, 40, "WALK");
      y += 56;
      if (k < 2) {
        // the sword's three swings (Windup 2, Strike 2, Follow 2 per facing): the slice's painter
        const art::RigSheet act = sheetOf(who[k], art::RigPart::Action);
        static const char* hit[3] = {"HIT 1 FOREHAND", "HIT 2 BACKHAND", "HIT 3 OVERHEAD"};
        for (int v = 0; v < 3; v++) {
          b.text(4, y - 30, hit[v]);
          int xx = 140;
          for (int facing = 0; facing < art::RIG_FACINGS; facing++) {
            for (art::Clip c : {art::Clip::Windup, art::Clip::Strike, art::Clip::Follow})
              for (int i = 0; i < act.count(facing, c, v); i++) {
                int px, py;
                const Canvas f = rigpv::frameCanvas(act, facing, c, v, i, px, py);
                if (f.w <= 0) continue;
                b.put(f, xx + 22 - px, y - py);
                b.c.set(xx + 22, y + 1, rgba(242, 193, 78));
                xx += 44;
              }
            xx += 14;
          }
          y += 66;
        }
        x = rigpv::clipRow(b, death, art::Clip::Fall, 0, 140, y, 48, "FALL / LYING");
        x = rigpv::clipRow(b, death, art::Clip::Lying, 0, x + 20, y, 56, "");
        y += 56;
        b.text(4, y - 30, "LYING W / E");
        int xx = 140;
        for (int v = 0; v < 2; v++) {
          int px, py;
          const Canvas f = rigpv::frameCanvas(death, 0, art::Clip::Lying, v, 0, px, py);
          if (f.w > 0) { b.put(f, xx + 20 - px, y - py); b.c.set(xx + 20, y + 1, rgba(242, 193, 78)); }
          xx += 60;
        }
        y += 40;
      }
      y += 40;
    }
    // the upgrade: today's v1 villager at the same scale
    art::setRigVersion(1);
    const Canvas v1 = art::humanSheet(rigpv::sliceVillager());
    Canvas cell(art::HUMAN_W, art::HUMAN_H);
    for (int yy = 0; yy < art::HUMAN_H; yy++) for (int xx = 0; xx < art::HUMAN_W; xx++) cell.set(xx, yy, v1.get(xx, yy));
    b.text(1340, 20, "V1 (TODAY)");
    b.put(cell, 1380, 40);
    art::setRigVersion(2);
    rigpv::saveBoth(b, dir, "rig_slice");
  }
  {
    // the study sheet: per look, a row per clip, the four facings side by side
    struct RowSpec { int who; art::RigPart part; art::Clip clip; int variant; };
    std::vector<RowSpec> spec;
    for (int k = 0; k < 3; k++) {
      spec.push_back({k, art::RigPart::Loco, art::Clip::Idle, 0});
      spec.push_back({k, art::RigPart::Loco, art::Clip::Walk, 0});
      if (k < 2) {
        for (int v = 0; v < 3; v++) spec.push_back({k, art::RigPart::Action, art::Clip::Windup, v});
        spec.push_back({k, art::RigPart::Death, art::Clip::Fall, 0});
      }
    }
    Study st(32, (int)spec.size());
    std::map<int, art::RigSheet> sheets;
    for (size_t r = 0; r < spec.size(); r++) {
      const RowSpec& rs = spec[r];
      const int key = rs.who * 4 + (int)rs.part;
      if (!sheets.count(key)) sheets[key] = sheetOf(who[rs.who], rs.part);
      const art::RigSheet& s = sheets[key];
      int col = 0;
      for (int facing = 0; facing < 4; facing++) {
        std::vector<std::pair<art::Clip, int>> clips;
        if (rs.clip == art::Clip::Idle) clips = {{art::Clip::Idle, 0}, {art::Clip::Hurt, 0}};
        else if (rs.clip == art::Clip::Windup) clips = {{art::Clip::Windup, rs.variant}, {art::Clip::Strike, rs.variant}, {art::Clip::Follow, rs.variant}};
        else if (rs.clip == art::Clip::Fall) clips = {{art::Clip::Fall, 0}, {art::Clip::Lying, 0}};
        else clips = {{rs.clip, 0}};
        for (auto& cv : clips)
          for (int i = 0; i < s.count(facing, cv.first, cv.second); i++) {
            if (col < st.cols) st.put(s, facing, cv.first, cv.second, i, col, (int)r);
            col++;
          }
      }
    }
    savePng(st.c, dir + "/rig_frames_1x.png", 1);
    savePng(st.c, dir + "/rig_frames.png", 4);
  }
  {
    // the Steel Arc board (STYLE_P2 12): per facing (rows down, up, right, left) the man's two strike frames of each
    // hit with the arc's frames 1 and 2 over them, then the arc's 4 frames alone. The arc sits, turns, sweeps and layers
    // as render_anim.cpp drawSwingFx draws it in game (slice fix r2): its circle's centre on the body (facing down at the
    // crown, a bowl across the chest; facing up at F-16, an arch 3 px over the crown, drawn BEHIND the body; side-on at
    // the shoulder row, in front of the face; left mirrored); hit 1 (facing left: hit 2) sweeps the other way; hit 3
    // facing down / up stands upright beside the weapon arm (the overhead cut seen end-on).
    const int CW = 56, CH = 64, PX = 28, PY = 56;
    Canvas c(CW * 10, CH * 4);
    for (int y = 0; y < c.h; y++)
      for (int x = 0; x < c.w; x++) c.set(x, y, ((x / CW + y / CH) & 1) ? rgba(176, 146, 104) : rgba(86, 140, 62));
    Canvas arc[4];
    for (int f = 0; f < 4; f++) { arc[f] = Canvas(32, 32); art::rig2::steelArc(arc[f], f); }
    // render_anim.cpp rvArcCells + the blit's flip: back mirrors the source top to bottom, rot 1 / 3 turns it a quarter
    // clockwise / counter-clockwise, flip mirrors it left to right (exact pixel moves, never resampled)
    auto turned = [&](const Canvas& s, int rot, bool back, bool flip) {
      Canvas o(32, 32);
      for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
          const uint32_t p = s.get(x, back ? 31 - y : y);
          if (!p) continue;
          int nx = rot == 1 ? 31 - y : rot == 3 ? y : x, ny = rot == 1 ? x : rot == 3 ? 31 - x : y;
          if (flip) nx = 31 - nx;
          o.set(nx, ny, p);
        }
      return o;
    };
    auto blend = [&](int x, int y, uint32_t p) {
      const int a = (int)(p >> 24);
      if (!a || x < 0 || y < 0 || x >= c.w || y >= c.h) return;
      c.set(x, y, a >= 255 ? p : art::opaque(art::mix(c.get(x, y), art::opaque(p), a / 255.0f)));
    };
    auto putArc = [&](int f, int fc, int v, int ox, int oy) {   // (ox, oy): the pivot; v: the hit (0..2)
      const bool upright = v == 2 && fc <= 1;
      const bool back = upright ? fc == 1 : v == (fc == 3 ? 1 : 0), flip = upright ? fc == 0 : fc == 3;
      const int rot = upright ? 0 : fc == 0 ? 1 : fc == 1 ? 3 : 0;
      static const int kArcX[4] = {0, 0, -1, 1}, kArcY[4] = {-32, -16, -17, -17};
      static const int kDirX[4] = {0, 0, 1, -1}, kDirY[4] = {1, -1, 0, 0};
      const int ax = upright ? (fc == 0 ? 6 : -11) : kArcX[fc], ay = upright ? (fc == 0 ? -18 : -26) : kArcY[fc];
      const int dx = upright ? (fc == 0 ? -1 : 1) : kDirX[fc], dy = upright ? 0 : kDirY[fc];
      const Canvas t = turned(arc[f], rot, back, flip);
      const int cx = ax + dx * 17, cy = ay + dy * 17;   // the cell's centre (half the cell plus the 1 px beyond it)
      for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) blend(ox + cx - 16 + x, oy + cy - 16 + y, t.get(x, y));
    };
    const art::RigSheet ac = sheetOf(who[0], art::RigPart::Action);
    for (int fc = 0; fc < 4; fc++) {
      int col = 0;
      for (int v = 0; v < 3; v++)
        for (int i = 0; i < 2; i++, col++) {
          int px, py;
          const Canvas f = rigpv::frameCanvas(ac, fc, art::Clip::Strike, v, i, px, py);
          const int ox = col * CW + PX, oy = fc * CH + PY;
          const bool behind = fc == 1 && v != 2;   // render_anim.cpp rvSwingFxBehind
          if (behind) putArc(1 + i, fc, v, ox, oy);
          for (int y = 0; y < f.h; y++) for (int x = 0; x < f.w; x++) blend(ox - px + x, oy - py + y, f.get(x, y));
          if (!behind) putArc(1 + i, fc, v, ox, oy);
        }
      for (int f = 0; f < 4; f++, col++) putArc(f, fc, 1, col * CW + PX, fc * CH + PY);   // hit 2's sweep, alone
    }
    savePng(c, dir + "/rig_arc_1x.png", 1);
    savePng(c, dir + "/rig_arc.png", 4);
  }
  {
    // the fall board: per look and facing, the fall to the west (variant 0) and to the east (variant 1): recoil,
    // buckle, topple, then the lying body it ends in (the head on the fall side, STYLE_P2 13), on grass and road
    Study st(8, 8);
    for (int k = 0; k < 2; k++) {
      const art::RigSheet de = art::humanRig(who[k], art::RigPart::Death);
      for (int fc = 0; fc < 4; fc++) {
        const int row = k * 4 + fc;
        for (int v = 0; v < 2; v++) {
          for (int i = 0; i < 3; i++) st.put(de, fc, art::Clip::Fall, v, i, v * 4 + i, row);
          st.put(de, fc, art::Clip::Lying, v, 0, v * 4 + 3, row);
        }
      }
    }
    savePng(st.c, dir + "/rig_fall_1x.png", 1);
    savePng(st.c, dir + "/rig_fall.png", 6);
  }
  {
    Board b(1200, 560);
    int x = 10, y = 50;
    for (int body = 0; body < 2; body++)
      for (int people = 0; people < 3; people++)
        for (int build = 0; build < 3; build++) {
          for (int age : {0, 2, 1}) {
            art::HumanLook L = rigpv::sliceHero(body == 1);
            L.weapon = 0; L.people = (uint8_t)people; L.build = (uint8_t)(build == 0 ? 1 : build == 1 ? 0 : 2); L.age = (uint8_t)age;
            const art::RigSheet s = art::humanRig(L, art::RigPart::Loco);
            int px, py;
            const Canvas f = rigpv::frameCanvas(s, 0, art::Clip::Idle, 0, 0, px, py);
            if (f.w > 0) b.put(f, x + 12 - px, y - py); else b.text(x, y - 8, "-", rgba(232, 96, 60));
            x += 26;
          }
          x += 8;
          if (x > 1120) { x = 10; y += 60; }
        }
    b.text(4, 4, "BODIES: MAN / WOMAN x HUMAN / HALF / ELF x SLIM / AVERAGE / BROAD x ADULT / YOUTH / CHILD");
    rigpv::saveBoth(b, dir, "rig_bodies");
  }
  return 0;
}
