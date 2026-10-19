// rigdraft (P2b phase A, lead; VISION_PLAN 15.36 "Adapted" and item 3): exports the REJECTED procedural rig once as
// drafts for the doll, so its numbers and rasterizers are not lost when the P2c flip deletes rpg/art/rig:
//   poses    P2a's pose numbers per frame (the anchors its painters solved: head, hands, grip, tip, hip, the weapon's
//            depth) for LOCO, every ACTION family x variant, DEATH and the 31 postures: number drafts for the clip tables
//   weapons  every base weapon (art ids 1-9) rasterized at the 16 directions (22.5 degree steps) by rig_arms.cpp, as
//            .pxm drafts (the grip at the origin, the tip marked); P2c's WEAPONS cleans them by hand
//   fx       the 9 family effects (art_fx.cpp) frame by frame as .pxm drafts; SHIELDS-FX re-cuts them to A3's arc
// Colours are matched to the nearest A3 hero colour of maps/legend/person.legend (a draft is blocking-in: every committed
// map is hand-placed and hand-checked, STYLE_P2 24).
//   rigdraft <outDir> [poses] [weapons] [fx]        (default: all three)
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include "rpg/art/doll/doll.h"
#include "rpg/art/rig/rig_internal.h"
#include "tools/preview/preview_util.h"

namespace {
using art::Clip;
using art::RigPart;
using art::RigSheet;

// A3's hero colours and their person.legend characters (docs/ref/style_a3/pal.py)
struct Ch { char c; uint32_t rgb; };
const Ch kPal[] = {{'K', 0x21152A}, {'a', 0x3A1622}, {'b', 0x742E2C}, {'c', 0xAA4A2C}, {'d', 0xD9733A}, {'e', 0xF5B060}, {'q', 0x5E2A36},
                   {'r', 0xB8644F}, {'s', 0xE9A077}, {'t', 0xF8C597}, {'u', 0xFFE6C2}, {'5', 0x162540}, {'6', 0x1F4A68}, {'7', 0x28728C},
                   {'8', 0x3E9FA7}, {'9', 0x8BD8C6}, {'m', 0x44122C}, {'n', 0x87232F}, {'o', 0xC83E38}, {'p', 0xF47650}, {'f', 0x2C1621},
                   {'g', 0x5B3129}, {'h', 0x8D5536}, {'i', 0xBF8249}, {'j', 0xE7B672}, {'1', 0x1C1732}, {'2', 0x33304E}, {'3', 0x4F4B6B},
                   {'4', 0x777493}, {'v', 0x201C31}, {'w', 0x525C7A}, {'x', 0x95A2BC}, {'y', 0xD5DEEA}, {'z', 0xFFFFFF}, {'N', 0x704020},
                   {'O', 0xCB9034}, {'P', 0xF8DA80}, {'C', 0x7FA8D8}, {'B', 0xC9E6F4}, {'A', 0xFFFDF4}};
// the colours a draft may use (weapons: steel, brass, leather and the ink; effects: the arc's three tones, steel, brass,
// leather), so a light blade never turns into trouser blue
const char* g_allowed = nullptr;
char nearest(uint32_t px) {
  const int r = px & 255, g = px >> 8 & 255, b = px >> 16 & 255;
  int best = 1 << 30;
  char out = '.';
  for (const Ch& c : kPal) {
    if (g_allowed && !std::strchr(g_allowed, c.c)) continue;
    const int cr = c.rgb >> 16 & 255, cg = c.rgb >> 8 & 255, cb = c.rgb & 255;
    const int d = (r - cr) * (r - cr) * 3 + (g - cg) * (g - cg) * 4 + (b - cb) * (b - cb) * 2;
    if (d < best) { best = d; out = c.c; }
  }
  return out;
}
std::string grid(const Canvas& c, int x0, int y0, int x1, int y1) {
  std::string s;
  for (int y = y0; y <= y1; y++) {
    s += '|';
    for (int x = x0; x <= x1; x++) { const uint32_t p = c.get(x, y); s += (p >> 24) ? nearest(p) : '.'; }
    s += '\n';
  }
  return s;
}
bool bounds(const Canvas& c, int& x0, int& y0, int& x1, int& y1) {
  x0 = y0 = 1 << 20; x1 = y1 = -1;
  for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) if (c.get(x, y) >> 24) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
  return x1 >= 0;
}
std::string pt(const char* n, int16_t x, int16_t y) {
  if (x == art::kNoAnchor || y == art::kNoAnchor) return "";
  return std::string(" ") + n + "=" + std::to_string(x) + "," + std::to_string(y);
}
void dumpSheet(std::ofstream& o, const RigSheet& s, const std::string& what) {
  for (int f = 0; f < 4; f++)
    for (int c = 0; c < art::CLIP_COUNT; c++)
      for (int v = 0; v < art::RIG_VARIANTS; v++) {
        if (v && s.frame(f, (Clip)c, v, 0) == s.frame(f, (Clip)c, 0, 0)) continue;
        for (int i = 0; i < s.count(f, (Clip)c, v); i++) {
          const art::FrameRect* r = s.frame(f, (Clip)c, v, i);
          const art::RigAnchors* an = s.anchorsOf(f, (Clip)c, v, i);
          o << what << " facing=" << "DURL"[f] << " clip=" << art::clipName((Clip)c) << " variant=" << v << " frame=" << i << " w=" << r->w << " h=" << r->h
            << " pivot=" << r->px << "," << r->py;
          if (an) o << pt("head", an->headX, an->headY) << pt("face", an->faceX, an->faceY) << pt("handR", an->handRX, an->handRY) << pt("handL", an->handLX, an->handLY)
                    << pt("hip", an->hipX, an->hipY) << pt("grip", an->gripX, an->gripY) << pt("tip", an->tipX, an->tipY) << " behind=" << (an->weaponBehind ? 1 : 0);
          o << "\n";
        }
      }
}
art::HumanLook lookFor(int weapon) {
  art::HumanLook L;
  L.weapon = (uint8_t)weapon;
  L.skin = rgba(236, 188, 146); L.hairColor = rgba(120, 70, 36);
  return L;
}

void poses(const std::string& dir) {
  std::ofstream o(dir + "/rig_poses.txt");
  o << "# P2a's pose numbers (tools/preview/rigdraft.cpp from the detached procedural rig): pivot-relative anchors per frame\n";
  static const int fam[art::FAMILY_COUNT] = {0, 5, 1, art::WEAPON_GREATSWORD, 2, 6, 8, 4, 3};
  for (int body = 0; body < 2; body++) {
    art::HumanLook L = lookFor(1);
    L.body = (uint8_t)body;
    dumpSheet(o, art::legacyRig(L, RigPart::Loco), std::string("loco body=") + (body ? "woman" : "man"));
    dumpSheet(o, art::legacyRig(L, RigPart::Death), std::string("death body=") + (body ? "woman" : "man"));
  }
  for (int f = 0; f < art::FAMILY_COUNT; f++) dumpSheet(o, art::legacyRig(lookFor(fam[f]), RigPart::Action), std::string("action family=") + art::familyName((art::Family)f));
  for (int p = 1; p < (int)art::Posture::COUNT; p++) {
    if ((art::Posture)p == art::Posture::Sleep || (art::Posture)p == art::Posture::Row) continue;
    dumpSheet(o, art::legacyPosture2(lookFor(0), (art::Posture)p, 0), "posture=" + std::to_string(p));
  }
  std::printf("wrote %s/rig_poses.txt\n", dir.c_str());
}

void weapons(const std::string& dir) {
  static const char* names[10] = {"", "sword", "axe", "bow", "staff", "dagger", "mace", "greatsword", "spear", "stick"};
  art::rig2::Kit K;
  art::rig2::makeKit(lookFor(1), K);
  g_allowed = "vwxyzNOPfghijK";
  for (int w = 1; w <= 9; w++) {
    std::ofstream o(dir + "/weapon_" + names[w] + ".pxm");
    o << "# a draft of the weapon " << names[w] << " (art id " << w << ") at 16 directions, from P2a's rasterizer (tools/preview/rigdraft.cpp)\n";
    o << "part held/weapons/" << names[w] << "\ntags draft p2a\nuse person\nbody adult\nrole held " << names[w] << "\nlayer held\nanchor grip\nkeys held\n";
    for (int d = 0; d < 16; d++) {
      art::rig2::Fig fig(80, 80, 40, 40);
      int tx = 0, ty = 0;
      art::rig2::paintArms(fig, K, w, 0.5f, 0.5f, d, 0, 0, tx, ty);
      Canvas c(fig.W, fig.H);
      c.px = fig.px;
      int x0, y0, x1, y1;
      if (!bounds(c, x0, y0, x1, y1)) continue;
      o << "\nview R key a" << d << " origin " << (fig.ox - x0) << " " << (fig.oy - y0) << "\n";
      o << "mark tip " << (tx + fig.ox - x0) << " " << (ty + fig.oy - y0) << "\n";
      o << grid(c, x0, y0, x1, y1);
    }
  }
  std::printf("wrote %s/weapon_*.pxm\n", dir.c_str());
}

void effects(const std::string& dir) {
  g_allowed = "CBAvwxyzNOPfghij";
  static const char* names[] = {"swingarc", "heavyarc", "choparc", "smashring", "stabglint", "thruststreak", "bluntswish", "punchstar", "twang"};
  for (int k = 0; k < 9; k++) {
    const art::Fx f = (art::Fx)((int)art::Fx::SwingArc + k);
    const Canvas s = art::fxSprite(f);
    const int fw = art::fxW(f), fh = art::fxH(f), n = art::fxFrames(f);
    std::ofstream o(dir + "/fx_" + names[k] + ".pxm");
    o << "# a draft of the effect " << names[k] << " (" << fw << " x " << fh << ", " << n << " frames) from art_fx.cpp (tools/preview/rigdraft.cpp)\n";
    o << "part fx/" << names[k] << "\ntags draft p2a\nuse person\nbody adult\nrole fx " << names[k] << "\nlayer fx\nanchor pivot\nkeys fx\n";
    for (int i = 0; i < n; i++) {
      Canvas c(fw, fh);
      for (int y = 0; y < fh; y++) for (int x = 0; x < fw; x++) c.set(x, y, s.get(i * fw + x, y));
      o << "\nview R key f" << i << " origin " << fw / 2 << " " << fh / 2 << "\n" << grid(c, 0, 0, fw - 1, fh - 1);
    }
  }
  std::printf("wrote %s/fx_*.pxm\n", dir.c_str());
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::printf("rigdraft <outDir> [poses] [weapons] [fx]\n"); return 1; }
  const std::string dir = argv[1];
  bool p = argc == 2, w = argc == 2, f = argc == 2;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "poses")) p = true;
    else if (!std::strcmp(argv[i], "weapons")) w = true;
    else if (!std::strcmp(argv[i], "fx")) f = true;
  }
  art::setRigVersion(2);
  if (p) poses(dir);
  if (w) weapons(dir);
  if (f) effects(dir);
  return 0;
}
