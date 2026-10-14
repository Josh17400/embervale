// Skills gallery (M9 "Arcana & Mastery", UI lane): every skill's constellation sky as the skills page bakes it
// (rpg/view/skills_paint.h), in three states of the hero, plus every spell's orb icon and the stealth eye.
//   skills_gallery <outDir>   writes (each zoomed x3 and at 1x as *_1x.png)
//     skies_fresh.png    every skill at level 5 with one perk point (the roots ready, the rest locked), desktop size
//     skies_mid.png      every skill at level 50 with half its reachable perks taken and two points (phone size)
//     skies_full.png     every skill at 100 with its whole tree taken (desktop size)
//     icons.png          every school spell's 16 x 16 orb (1x and x2), the eye open and lidded
//   skills_gallery --check   every tree's stars inside its sky (both sizes) with no two stars closer than 6 px, every
//                            figure stroke inside the grid, every spell icon non-empty; non-zero on failure
#include <cstring>

#include "rpg/view/skills_paint.h"
#include "tools/preview/preview_util.h"

namespace {

void blitC(Canvas& dst, const Canvas& s, int x, int y, int scale = 1) {
  for (int j = 0; j < s.h * scale; j++)
    for (int i = 0; i < s.w * scale; i++) {
      const uint32_t p = s.get(i / scale, j / scale);
      if (!(p >> 24)) continue;
      dst.set(x + i, y + j, p | 0xFF000000u);
    }
}
void fill(Canvas& c, uint32_t col) { for (uint32_t& p : c.px) p = col; }
void label(Canvas& c, int x, int y, const std::string& s, uint32_t col) {
  for (char ch : s) {
    const int gi = pv::font5x7::glyphIndex(ch);
    if (gi >= 0)
      for (int r = 0; r < 7; r++)
        for (int k = 0; k < 5; k++)
          if (pv::font5x7::kFont[gi].rows[r] & (0x10 >> k)) { c.set(x + k + 1, y + r + 1, rgba(0, 0, 0)); c.set(x + k, y + r, col); }
    x += 6;
  }
}

// the hero's skills for a sheet: 0 fresh, 1 mid, 2 full
skills::Skills heroAt(int kind) {
  skills::Skills k;
  for (int i = 0; i < skills::SKILLS; i++) {
    const skills::Skill s = (skills::Skill)i;
    k[s].level = (uint8_t)(kind == 0 ? 5 : kind == 1 ? 50 : 100);
    int n = 0;
    const skills::PerkDef* P = skills::perkTable(s, n);
    if (kind == 2) for (int p = 0; p < n; p++) k[s].perks |= 1u << p;
    if (kind == 1) {   // take every other perk that can be taken, in order
      k.perkPts = 99;
      int took = 0;
      for (int p = 0; p < n; p++) if (skills::perkBlocked(k, s, p).empty() && (took++ % 2 == 0 || p == 0)) k[s].perks |= 1u << p;
      (void)P;
    }
  }
  k.perkPts = kind == 0 ? 1 : kind == 1 ? 2 : 0;
  return k;
}

Canvas sheet(int kind, int sw, int sh) {
  const int cols = 5, pad = 8, lab = 12;
  const int rows = (skills::SKILLS + cols - 1) / cols;
  Canvas c(cols * (sw + pad) + pad, rows * (sh + pad + lab) + pad);
  fill(c, rgba(18, 15, 20));
  const skills::Skills k = heroAt(kind);
  for (int i = 0; i < skills::SKILLS; i++) {
    const skills::Skill s = (skills::Skill)i;
    int n = 0;
    const skills::PerkDef* P = skills::perkTable(s, n);
    std::vector<int> st((size_t)std::max(1, n));
    for (int p = 0; p < n; p++) st[(size_t)p] = skp::perkState(k, s, p);
    const Canvas sky = skp::paintSky(sw, sh, s, P, n, st.data());
    const int x = pad + (i % cols) * (sw + pad), y = pad + (i / cols) * (sh + pad + lab);
    label(c, x, y, skills::skillInfo(s).name, skills::skillInfo(s).color | 0xFF000000u);
    blitC(c, sky, x, y + lab);
    // the panel's frame as the page draws it
    for (int xx = -1; xx <= sw; xx++) { c.set(x + xx, y + lab - 1, rgba(140, 115, 70)); c.set(x + xx, y + lab + sh, rgba(140, 115, 70)); }
    for (int yy = -1; yy <= sh; yy++) { c.set(x - 1, y + lab + yy, rgba(140, 115, 70)); c.set(x + sw, y + lab + yy, rgba(140, 115, 70)); }
  }
  return c;
}

Canvas iconSheet() {
  const auto& all = magic::schoolSpells();
  Canvas c(16 + (int)all.size() * 64, 120);
  fill(c, rgba(18, 15, 20));
  int x = 8;
  for (const magic::SpellDef& d : all) {
    const Canvas ic = skp::paintSpellIcon(d);
    blitC(c, ic, x, 8);
    blitC(c, ic, x, 30, 2);
    std::string nm = d.name;
    if (nm.size() > 10) nm.resize(10);
    label(c, x, 70, nm, rgba(235, 228, 210));
    x += 64;
  }
  blitC(c, skp::paintEye(false, rgba(150, 150, 200)), 8, 90);
  blitC(c, skp::paintEye(true, rgba(150, 150, 200)), 28, 90);
  blitC(c, skp::paintEye(true, rgba(150, 150, 200)), 48, 84, 2);
  blitC(c, skp::paintSpark(), 80, 90);
  return c;
}

int check() {
  int bad = 0;
  for (int i = 0; i < skills::SKILLS; i++) {
    const skills::Skill s = (skills::Skill)i;
    int n = 0;
    const skills::PerkDef* P = skills::perkTable(s, n);
    for (int sz = 0; sz < 2; sz++) {
      const int w = sz ? 188 : 157, h = sz ? 138 : 146;
      const std::vector<skp::PerkVis> v = skp::perkLayout(P, n, w, h);
      for (int a = 0; a < n; a++) {
        if (v[(size_t)a].x < 6 || v[(size_t)a].y < 6 || v[(size_t)a].x > w - 7 || v[(size_t)a].y > h - 7) {
          std::printf("FAIL %s perk %d (%s) outside the sky at %dx%d\n", skills::skillInfo(s).name, a, P[a].name, w, h); bad++;
        }
        for (int b = a + 1; b < n; b++) {
          const int dx = v[(size_t)a].x - v[(size_t)b].x, dy = v[(size_t)a].y - v[(size_t)b].y;
          if (dx * dx + dy * dy < 36) { std::printf("FAIL %s perks %d and %d closer than 6 px\n", skills::skillInfo(s).name, a, b); bad++; }
        }
      }
    }
    for (const auto& st : skp::figureOf(s).strokes)
      for (float f : st) if (f < -9 || f > 9) { std::printf("FAIL %s figure outside the grid\n", skills::skillInfo(s).name); bad++; break; }
    if (skp::figureOf(s).strokes.empty()) { std::printf("FAIL %s has no figure\n", skills::skillInfo(s).name); bad++; }
  }
  for (const magic::SpellDef& d : magic::schoolSpells()) {
    const Canvas ic = skp::paintSpellIcon(d);
    int lit = 0;
    for (uint32_t p : ic.px) if (p >> 24) lit++;
    if (lit < 100) { std::printf("FAIL icon %s nearly empty\n", d.name.c_str()); bad++; }
  }
  std::printf("skills_gallery --check: %s (%d problems)\n", bad ? "FAILED" : "OK", bad);
  return bad ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && !std::strcmp(argv[1], "--check")) return check();
  const std::string out = argc > 1 ? argv[1] : ".";
  struct { const char* name; Canvas c; } sheets[] = {
      {"skies_fresh", sheet(0, 157, 146)}, {"skies_mid", sheet(1, 188, 138)}, {"skies_full", sheet(2, 157, 146)}, {"icons", iconSheet()}};
  for (auto& s : sheets) {
    savePng(s.c, out + "/" + s.name + ".png", kScale);
    savePng(s.c, out + "/" + s.name + "_1x.png", 1);
  }
  return 0;
}
