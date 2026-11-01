// EMBERVALE art: the doll's post passes (SOLID 3.6, moved verbatim from doll_compose.cpp). Each runs on a composed frame
// inside its dirty box, in the one order of kPostPasses (despeckle, glint, glintCap, glass, starBand, mythicFire, the
// float speck, the Mythic speck); A3's finishing pass (finishPass) runs later, on the trimmed or turned crop.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "rpg/art/doll/doll_compose_internal.h"

namespace art {
namespace doll {
namespace comp {

// the paint's planes (FramePaint writes them, the passes read them; declared in doll_compose_internal.h). Defined here, in
// the passes' own file, so the passes' per-pixel reads stay reads of this file's own thread-locals.
// (P2c forge i2) the pixels the body's armour and pauldrons wrote last (the glint pass reads them)
thread_local std::vector<uint8_t> g_bodyPiece;
thread_local std::vector<uint8_t> g_depthPx;   // (envelope r2) 1 where a held, back or fx item drew the pixel (C19's off-figure)
thread_local std::vector<uint8_t> g_heldPx;   // (ladder i2) the pixels a held weapon wrote this frame

// (P2c forge i2; PLAN C17, STYLE_P2 28.8: "a near-white glint"; the blind panel: "armour fill is 1-px dither noise instead
// of the reference's clean planes") the glints on a styled body's armour and pauldrons: 1 to 3 step-4 clusters from T2. The
// cut tiles' lit rows and the mail and scale units left 4-7 scattered step-4 specks (noise at 1x); to T4 the three nearest
// the top-left light keep their glint, the rest drop to the lit plane (step 3; T5-T6 keep the finish's sparkle, C12b). A figure with none (bronze, the ember, the
// Mythic: their lit planes stop at step 3) takes one on its upper-left lit plane. Styled figures only (forge[] is rig 2's)
// (INTEGRATE silhouettes i1, STYLE_P2 28.2 "no 1 px texture at 1x"; the owner: "a busy checker on torso and limbs") on a
// forge-styled figure, a gear pixel of a grey slot whose step (1-3) sits above or below all of its 3-4 same-slot gear
// neighbours (outlines and seams aside) takes their middle step: one pass, read from a copy. Glints (step 4) are the
// glint pass's, after this
void despecklePass(const DollRecipe& r, FrameOut& o) {
  bool anyStyled = r.shape[0] || r.shape[1] || r.shape[2] || r.shape[3];
  for (int t = 0; t < HumanLook::TIER_SLOTS && !anyStyled; t++)
    for (int a = 0; a < 8; a++) anyStyled |= r.forgeAx[t][a] != 0;
  if (!anyStyled || o.bx1 < o.bx0) return;
  const int x0 = std::max(1, o.bx0), x1 = std::min(CW - 2, o.bx1), y0 = std::max(1, o.by0), y1 = std::min(CH - 2, o.by1);
  static thread_local std::vector<uint8_t> src;
  src = o.code;
  auto gear = [&](size_t k) { return (g_bodyPiece[k] & 2) != 0 && src[k] >= 2; };
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const size_t k = (size_t)y * CW + x;
      if (!gear(k)) continue;
      const uint8_t c = src[k];
      const Slot s = codeSlot(c);
      const int st = codeStep(c);
      if (st == 0 || st == 4 || !greySlot(s)) continue;
      int above = 0, below = 0, n = 0, sum = 0;
      int steps[4];
      for (size_t k2 : {k - 1, k + 1, k - (size_t)CW, k + (size_t)CW}) {
        const uint8_t c2 = src[k2];
        if (!gear(k2) || codeSlot(c2) != s || codeStep(c2) == 0) continue;
        steps[n++] = codeStep(c2);
        sum += codeStep(c2);
        if (st > codeStep(c2)) above++;
        if (st < codeStep(c2)) below++;
      }
      if (n < 3 || (above != n && below != n)) continue;
      std::sort(steps, steps + n);
      o.code[k] = code(s, std::min(3, steps[n / 2]));
      (void)sum;
    }
}
// (P2c envelope, A8) a lone lit pixel on any metal of the figure (a step-3 pixel with no lit 4-neighbour: a broken 1 px dash on
// a plate, a greave, a gauntlet) is noise at 1x: it takes the body (step 2)
void loneLitPass(FrameOut& o) {
  if (o.bx1 < o.bx0) return;
  const int x0 = o.bx0, y0 = o.by0, w = o.bx1 - o.bx0 + 1, h = o.by1 - o.by0 + 1;
  auto at = [&](int x, int y) -> uint8_t& { return o.code[(size_t)(y0 + y) * CW + (x0 + x)]; };
  // (envelope r2 integration, A8) a lit pixel is propped up only by its own depth: a greave's 1 px dash beside the back
  // skirt's lit hem or the spear's blade is still a lone dash on the figure (C19 reads the figure without them)
  auto heldAt = [&](int x, int y) { return !g_depthPx.empty() && g_depthPx[(size_t)(y0 + y) * CW + (x0 + x)] != 0; };
  auto lit = [&](int x, int y, bool held) {
    if (x < 0 || y < 0 || x >= w || y >= h || heldAt(x, y) != held) return false;
    const uint8_t c = at(x, y);
    return c >= 2 && (codeSlot(c) == Slot::Metal || codeSlot(c) == Slot::Metal2) && codeStep(c) >= 3;
  };
  std::vector<int> lone;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const uint8_t c = at(x, y);
      if (c < 2 || codeStep(c) != 3 || (codeSlot(c) != Slot::Metal && codeSlot(c) != Slot::Metal2)) continue;
      const bool hd = heldAt(x, y);
      if (!lit(x - 1, y, hd) && !lit(x + 1, y, hd) && !lit(x, y - 1, hd) && !lit(x, y + 1, hd)) lone.push_back(y * w + x);
    }
  for (int q : lone) at(q % w, q / w) = code(codeSlot(at(q % w, q / w)), 2);
}
void glintPass(const DollRecipe& r, FrameOut& o) {
  bool styled = false, anyStyled = false;
  for (int a = 0; a < 8; a++) styled |= r.forgeAx[HumanLook::TIER_BODY][a] != 0;
  for (int t = 0; t < HumanLook::TIER_SLOTS; t++)
    for (int a = 0; a < 8; a++) anyStyled |= r.forgeAx[t][a] != 0;
  if (!anyStyled || o.bx1 < o.bx0) return;
  // (INTEGRATE forge i2) a body with no styled metal of its own (a soft kit under a styled helm): only the sheen's tell
  const bool bodyStyled = styled && r.pieceTier[HumanLook::TIER_BODY] >= 2;
  const int x0 = o.bx0, y0 = o.by0, w = o.bx1 - o.bx0 + 1, h = o.by1 - o.by0 + 1;
  auto at = [&](int x, int y) -> uint8_t& { return o.code[(size_t)(y0 + y) * CW + (x0 + x)]; };
  // (INTEGRATE forge i2, C17: the Highland plaid, the Marsh mantle and the Sylvan kit carry under 12 px of body metal, so
  // the sheen's tell never drew) `wide`: the sheen and glow fallbacks below may use any metal of the figure (helm, shield)
  bool wide = false;
  auto body = [&](int x, int y) { return wide || (g_bodyPiece[(size_t)(y0 + y) * CW + (x0 + x)] & 1) != 0; };
  auto metalAt = [&](int x, int y, int step) {
    const uint8_t c = at(x, y);
    return c >= 2 && codeSlot(c) == Slot::Metal && codeStep(c) == step && body(x, y);
  };
  std::vector<int> lab((size_t)w * h, -1);
  struct Cl { int key; std::vector<int> px; };
  std::vector<Cl> cl;
  int metal = 0;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      if (metalAt(x, y, 1) || metalAt(x, y, 2) || metalAt(x, y, 3)) metal++;
      if (!metalAt(x, y, 4) || lab[(size_t)y * w + x] >= 0) continue;
      Cl c{1 << 30, {}};
      std::vector<int> st{y * w + x};
      lab[(size_t)y * w + x] = (int)cl.size();
      while (!st.empty()) {
        const int q = st.back(); st.pop_back();
        c.px.push_back(q);
        const int qx = q % w, qy = q / w;
        c.key = std::min(c.key, qy * 2 + qx);
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++) {
            const int nx = qx + dx, ny = qy + dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h || lab[(size_t)ny * w + nx] >= 0 || !metalAt(nx, ny, 4)) continue;
            lab[(size_t)ny * w + nx] = (int)cl.size();
            st.push_back(ny * w + nx);
          }
      }
      cl.push_back(std::move(c));
    }
  // (P2c envelope, A8) a lone lit pixel on any metal of the figure (a step-3 pixel with no lit 4-neighbour: a broken 1 px
  // dash on a plate, a greave, a gauntlet) is noise at 1x: it takes the body (step 2). Highlights are clean planes of 2 x 2
  // or more, glints the step-4 clusters
  {
    loneLitPass(o);
  }
  int metalAll = 0;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) { const uint8_t c = at(x, y); if (c >= 2 && codeSlot(c) == Slot::Metal) metalAll++; }
  if (metalAll < 12) return;
  const bool soft = !bodyStyled || metal + (int)cl.size() < 12;   // (a soft kit: only the sheen's tell, on any metal)
  if (soft) wide = true;
  // (T5-T6 keep their sparkle: the living finish's gold and its glints climb with the tier, C12b; the cap holds to T4)
  // (P2c envelope, A8: "glint noise on heavy plate") three short hard glints a figure at every tier and frame: the T5-T6
  // living finish keeps its sparkle in its own light (the runes, the edge sweep, the motes: render.cpp), never as more step-4
  // specks on the plates
  if (!soft && cl.size() > 3) {
    std::stable_sort(cl.begin(), cl.end(), [](const Cl& a, const Cl& b) { return a.key < b.key; });
    for (size_t i = 3; i < cl.size(); i++)
      for (int q : cl[i].px) at(q % w, q / w) = code(Slot::Metal, 3);
  } else if (cl.empty() && bodyStyled) {
    wide = false;   // (the body's own plates first, then any metal: a soft kit's helm)
    // the brightest step the plates carry (3, else 2: the crackled Mythic and the ember's dark plates), nearest the light
    for (int want = 3; want >= 2; want--) {
      int best = -1, bk = 1 << 30;
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
          if (metalAt(x, y, want) && y * 2 + x < bk) { bk = y * 2 + x; best = y * w + x; }
      if (best >= 0) { at(best % w, best / w) = code(Slot::Metal, 4); break; }
    }
    wide = soft;
  }
  // a glowing alloy's seams from T4 (D28): where the form's drawing marks few seams (N: the lamellar and scale units
  // carry none), its armour's dark seams light instead: a step-1 metal pixel between two lighter metal pixels (above and
  // below, or left and right), the first 10 down the plates
  if ((r.sheen == 6 || r.has("glowseams")) && r.pieceTier[HumanLook::TIER_BODY] >= 4 && r.pieceTier[HumanLook::TIER_BODY] <= 5) {
    int glow = 0;
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) { const uint8_t c = at(x, y); if (c >= 2 && codeSlot(c) == Slot::Glow && codeStep(c) >= 2) glow++; }
    auto lighter = [&](int x, int y) {
      if (x < 0 || y < 0 || x >= w || y >= h) return false;
      const uint8_t c = at(x, y);
      return c >= 2 && codeSlot(c) == Slot::Metal && codeStep(c) >= 2 && body(x, y);
    };
    for (int y = 0; y < h && glow < 10; y++)
      for (int x = 0; x < w && glow < 10; x++)
        if (metalAt(x, y, 1) && ((lighter(x, y - 1) && lighter(x, y + 1)) || (lighter(x - 1, y) && lighter(x + 1, y)))) {
          at(x, y) = code(Slot::Glow, 2);
          glow++;
        }
    // a form with no dark seam inside its plates (the leaf, the smooth Dune plate): any inner step-1 metal pixel (all four
    // neighbours metal), then a seam line cut across the chest's body tone, every other pixel of its widest metal row
    auto metalAny = [&](int x, int y) {
      if (x < 0 || y < 0 || x >= w || y >= h) return false;
      const uint8_t c = at(x, y);
      return c >= 2 && (codeSlot(c) == Slot::Metal || codeSlot(c) == Slot::Glow) && codeStep(c) >= 1 && body(x, y);
    };
    for (int y = 0; y < h && glow < 8; y++)
      for (int x = 0; x < w && glow < 8; x++)
        if (metalAt(x, y, 1) && metalAny(x - 1, y) && metalAny(x + 1, y) && metalAny(x, y - 1) && metalAny(x, y + 1)) {
          at(x, y) = code(Slot::Glow, 2);
          glow++;
        }
    // (INTEGRATE forge i2, C17: the Sylvan kit's body carries too little plate for six) the seam line runs on the widest
    // body row, then on the widest row of any metal (the helm's band, the shield's face)
    for (int pass = 0; pass < 2 && glow < 8; pass++) {
      if (pass) wide = true;
      int by = -1, bn = 0;
      for (int y = 0; y < h; y++) {
        int n = 0;
        for (int x = 0; x < w; x++) n += metalAt(x, y, 2) ? 1 : 0;
        if (n > bn) { bn = n; by = y; }
      }
      for (int x = 0, k = 0; by >= 0 && x < w && glow < 10; x++)
        if (metalAt(x, by, 2) && (k++ & 1) == 0) { at(x, by) = code(Slot::Glow, 2); glow++; }
    }
    // (INTEGRATE silhouettes i1, C17: a tabard or a small plate leaves the seam line short of 8) then every other step 1-2
    // metal pixel down the body, none beside a glow pixel already lit (a seam's dots, never a blob)
    auto glowNear = [&](int x, int y) {
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          if (x + dx < 0 || y + dy < 0 || x + dx >= w || y + dy >= h) continue;
          const uint8_t c = at(x + dx, y + dy);
          if (c >= 2 && codeSlot(c) == Slot::Glow) return true;
        }
      return false;
    };
    const bool wasWide = wide;
    for (int pass = 0; pass < 2 && glow < 8; pass++) {
      wide = wasWide || pass == 1;
      for (int y = 0; y < h && glow < 8; y++)
        for (int x = 0; x < w && glow < 8; x++)
          if ((metalAt(x, y, 1) || metalAt(x, y, 2)) && !glowNear(x, y)) { at(x, y) = code(Slot::Glow, 2); glow++; }
    }
    wide = wasWide;
  }
  // (silhouettes i2, STYLE_P2 28.4 FURNACE "the red is heat in the cracks") the furnace's dark seams glow on every metal piece
  // (helm, plates, shoulders, skirt): a step-1 metal pixel between two lighter metal pixels, above and below or left and
  // right, to 26 glow pixels in all; the seam's ends (one lit neighbour pair only) stay dark so the cracks taper
  if (r.has("furnace")) {
    const bool was = wide;
    wide = true;
    int glow = 0;
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) { const uint8_t c = at(x, y); if (c >= 2 && codeSlot(c) == Slot::Glow && codeStep(c) >= 2) glow++; }
    auto lit = [&](int x, int y) {
      if (x < 0 || y < 0 || x >= w || y >= h) return false;
      const uint8_t c = at(x, y);
      return c >= 2 && codeSlot(c) == Slot::Metal && codeStep(c) >= 2;
    };
    // a vent: a dark pixel under a brighter metal lip, metal (or glow) on all four sides (inside the plates, never the rim)
    auto anyMetal = [&](int x, int y) {
      if (x < 0 || y < 0 || x >= w || y >= h) return false;
      const uint8_t c = at(x, y);
      return c >= 2 && (codeSlot(c) == Slot::Metal || codeSlot(c) == Slot::Glow);
    };
    auto lipAbove = [&](int x, int y) {
      if (!anyMetal(x - 1, y) || !anyMetal(x + 1, y) || !anyMetal(x, y - 1) || !anyMetal(x, y + 1)) return false;
      const uint8_t c = at(x, y), u = at(x, y - 1);
      return codeSlot(u) == Slot::Metal && codeStep(u) > codeStep(c) && codeStep(u) >= 2;
    };
    std::vector<int> seam;
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
        if ((metalAt(x, y, 1) || metalAt(x, y, 0)) && ((lit(x, y - 1) && lit(x, y + 1)) || (lit(x - 1, y) && lit(x + 1, y)) || lipAbove(x, y))) seam.push_back(y * w + x);
    // cracks, never specks: a seam pixel with a seam neighbour (a run of 2 or more) first; lone ones only while under 8
    {
      std::vector<int> runs, lone;
      for (int q : seam) {
        bool nb = false;
        for (int o : {q - 1, q + 1, q - w, q + w}) nb |= std::find(seam.begin(), seam.end(), o) != seam.end() && (o / w == q / w || o % w == q % w);
        (nb ? runs : lone).push_back(q);
      }
      seam = runs;
      for (size_t i = 0; i < lone.size() && (int)seam.size() + glow < 8; i++) seam.push_back(lone[i]);
      std::sort(seam.begin(), seam.end());
    }
    // whole cracks spread down the figure (helm to skirt): the seam lines (4-connected), every k-th line until 26 pixels
    std::vector<std::vector<int>> lines;
    {
      std::vector<char> used(seam.size(), 0);
      for (size_t i = 0; i < seam.size(); i++) {
        if (used[i]) continue;
        std::vector<int> ln{seam[i]};
        used[i] = 1;
        for (size_t k = 0; k < ln.size(); k++)
          for (size_t j = 0; j < seam.size(); j++)
            if (!used[j] && std::abs(seam[j] % w - ln[k] % w) + std::abs(seam[j] / w - ln[k] / w) == 1) { used[j] = 1; ln.push_back(seam[j]); }
        lines.push_back(ln);
      }
    }
    // the body's plates first (the barrel chest's vents, the skirt's joins: 18 pixels), then the helm, shield and limbs
    auto onBody = [&](const std::vector<int>& ln) {
      for (int q : ln) if (g_bodyPiece[(size_t)(y0 + q / w) * CW + (x0 + q % w)] & 1) return true;
      return false;
    };
    for (int grp = 0; grp < 2; grp++) {
      std::vector<const std::vector<int>*> g;
      int total = 0;
      for (const auto& ln : lines) if (onBody(ln) == (grp == 0)) { g.push_back(&ln); total += (int)ln.size(); }
      const int cap = grp == 0 ? std::min(26, glow + 18) : 26;
      const int room = std::max(0, cap - glow);
      if (!room || g.empty()) continue;
      const size_t step = (size_t)std::max(1, (total + room - 1) / room);
      for (size_t i = 0; i < g.size() && glow < cap; i += step)
        for (int q : *g[i]) if (glow < cap) { at(q % w, q / w) = code(Slot::Glow, 2); glow++; }
    }
    // (forge fix i1, the owner: "Emberforged has no hot-metal glow: needs bright molten seams") a crack's inner pixels (a glow
    // pixel with glow on both sides along its line) burn white-hot (the glow's step 3): each crack a hot core in an orange
    // body under a red rim, never a painted stripe; a crack of 2 keeps its orange
    {
      auto g2 = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return false;
        const uint8_t c = at(x, y);
        return c >= 2 && codeSlot(c) == Slot::Glow && codeStep(c) >= 2;
      };
      std::vector<int> core;
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
          if (g2(x, y) && ((g2(x - 1, y) && g2(x + 1, y)) || (g2(x, y - 1) && g2(x, y + 1)))) core.push_back(y * w + x);
      for (int q : core) at(q % w, q / w) = code(Slot::Glow, 3);
    }
    // (silhouettes i3, the blind panel: "a cold iron stove with lit vents, no red-orange gradient") the heat bleeds into the
    // iron: a shadow or body metal pixel under or beside a glowing crack takes the glow's dark step (a red ember edge), the
    // pixel below first (heat rises from the vent's lip), 16 pixels at most, so each crack reads hot-cored, red-rimmed
    {
      std::vector<int> halo;
      auto glow2 = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return false;
        const uint8_t c = at(x, y);
        return c >= 2 && codeSlot(c) == Slot::Glow && codeStep(c) >= 2;
      };
      for (int pass = 0; pass < 2; pass++)
        for (int y = 0; y < h; y++)
          for (int x = 0; x < w; x++) {
            if (!(metalAt(x, y, 1) || metalAt(x, y, 2))) continue;
            const bool hit = pass == 0 ? glow2(x, y - 1) : (glow2(x - 1, y) || glow2(x + 1, y) || glow2(x, y + 1));
            if (hit && std::find(halo.begin(), halo.end(), y * w + x) == halo.end()) halo.push_back(y * w + x);
          }
      for (size_t i = 0; i < halo.size() && i < 16; i++) at(halo[i] % w, halo[i] / w) = code(Slot::Glow, 1);
    }
    wide = was;
  }
  // (forge fix i1, the owner: "Obsidian reads as black cloth or lacquer: needs glassy specular streaks") KNAPPED's glass: up to
  // four diagonal streaks (rising to the right, 45 degrees, the lit facet's step 3, 3-4 px long) across the body's dark facets
  // (step 1-2 metal runs of 3 or more along the diagonal), 7 px apart: the light slides across glass, it does not sit on it
  // the sheen's glint (STYLE_P2 28.7): matte, no step-4 run over 1 px on any metal (a run drops to its first pixel); bright,
  // the top glint a 2 px run (its right or left lit neighbour lifted)
  auto metal4 = [&](int x, int y) { const uint8_t c = at(x, y); return c >= 2 && codeSlot(c) == Slot::Metal && codeStep(c) == 4; };
  if (r.sheen == 1) {
    // (forge i3: right to left, so a run of three keeps its first pixel; left to right split it into two glints)
    for (int y = 0; y < h; y++)
      for (int x = w - 1; x >= 1; x--)
        if (metal4(x, y) && metal4(x - 1, y)) at(x, y) = code(Slot::Metal, 3);
  } else if (r.sheen == 2) {
    bool run = false;
    for (int y = 0; y < h && !run; y++)
      for (int x = 1; x < w && !run; x++) run = metal4(x, y) && metal4(x - 1, y);
    for (int y = 0; y < h && !run; y++)
      for (int x = 0; x < w && !run; x++) {
        if (!metal4(x, y)) continue;
        for (int dx : {1, -1, 2, -2, 3, -3}) {
          const int nx = x + (dx > 0 ? 1 : -1);
          const int want = 4 - std::abs(dx);   // a lit neighbour first, else a body one, else the shadow
          if (nx < 0 || nx >= w || !metalAt(nx, y, want)) continue;
          at(nx, y) = code(Slot::Metal, 4);
          run = true;
          break;
        }
      }
    // no glint with a metal neighbour beside it (a glint on a plate's edge): the top-left glint moves onto the nearest
    // horizontal pair of lit body metal as a 2 px run
    if (!run) {
      int bx = -1, by = -1, bk = 1 << 30;
      // (INTEGRATE forge i2, C17: the Highland and Marsh kits' body metal is mail brick with no lit pair) then any metal's
      for (int pass = 0; pass < 2 && bx < 0; pass++) {
        if (pass) wide = true;
        for (int want = 3; want >= 2 && bx < 0; want--)
          for (int y = 0; y < h; y++)
            for (int x = 0; x + 1 < w; x++)
              if (metalAt(x, y, want) && metalAt(x + 1, y, want) && y * 2 + x < bk) { bk = y * 2 + x; bx = x; by = y; }
      }
      if (bx >= 0) {
        if (!cl.empty() && std::min<size_t>(cl.size(), 3) >= 3)
          for (int q : cl[2].px) if (metal4(q % w, q / w)) at(q % w, q / w) = code(Slot::Metal, 3);
        at(bx, by) = code(Slot::Metal, 4);
        at(bx + 1, by) = code(Slot::Metal, 4);
      }
    }
  }

}

// (P2c envelope, A8: "glint noise on heavy plate") after the glint pass, on every frame of a styled figure (a soft one too):
// the figure's step-4 metal in three glints at most, then no lone lit pixel
void glintCapPass(const DollRecipe& r, FrameOut& o) {
  bool anyStyled = false;
  for (int t = 0; t < HumanLook::TIER_SLOTS; t++)
    for (int a = 0; a < 8; a++) anyStyled |= r.forgeAx[t][a] != 0;
  // (envelope r2 integration, A8 "glint noise on heavy plate") the plain heavy plate (T5-T6, no forge style: the hero line's
  // own e5 / e6) is cleaned of lone lit dashes too; its drawn art keeps everything else
  if (!anyStyled && r.pieceTier[HumanLook::TIER_BODY] >= 5) { loneLitPass(o); return; }
  if (!anyStyled || o.bx1 < o.bx0) return;
  const int x0 = o.bx0, y0 = o.by0, w = o.bx1 - o.bx0 + 1, h = o.by1 - o.by0 + 1;
  auto at = [&](int x, int y) -> uint8_t& { return o.code[(size_t)(y0 + y) * CW + (x0 + x)]; };
  struct Cl { int key; std::vector<int> px; };
  // (P2c envelope, A8: "3 a figure at most, in every facing and frame") the figure's step-4 metal (Metal and Metal 2: the
  // plates, the helm, the trim, the greaves) in three glints at most: the largest (the bright sheen's 2 px run) and then those
  // nearest the top-left light keep step 4, the rest take the lit plane (step 3). Every frame, a soft one too (a back view
  // under the long cloak shows little body plate, and its helm, trim and greaves still glint)
  {
    auto m4 = [&](int x, int y) {
      if (x < 0 || y < 0 || x >= w || y >= h) return false;
      const uint8_t c = at(x, y);
      return c >= 2 && (codeSlot(c) == Slot::Metal || codeSlot(c) == Slot::Metal2) && codeStep(c) == 4;
    };
    std::vector<int> seen((size_t)w * h, 0);
    std::vector<Cl> all;
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        if (!m4(x, y) || seen[(size_t)y * w + x]) continue;
        Cl c{1 << 30, {}};
        std::vector<int> st{y * w + x};
        seen[(size_t)y * w + x] = 1;
        while (!st.empty()) {
          const int q = st.back(); st.pop_back();
          c.px.push_back(q);
          c.key = std::min(c.key, (q / w) * 2 + q % w);
          for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
              const int nx = q % w + dx, ny = q / w + dy;
              if (!m4(nx, ny) || seen[(size_t)ny * w + nx]) continue;
              seen[(size_t)ny * w + nx] = 1;
              st.push_back(ny * w + nx);
            }
        }
        // the body armour's own glints first (C17: 1-3 on the armour), then the helm's, the trim's and the greaves'
        // (the plates' own Metal before the gold trim's Metal 2: C17 counts the armour's Metal glints)
        bool onBody = false, metal1 = false;
        for (int q : c.px) {
          onBody |= (g_bodyPiece[(size_t)(y0 + q / w) * CW + (x0 + q % w)] & 1) != 0;
          metal1 |= codeSlot(at(q % w, q / w)) == Slot::Metal;
        }
        if (!onBody) c.key += 1 << 20;
        if (!metal1) c.key += 1 << 19;
        all.push_back(std::move(c));
      }
    if (all.size() > 3) {
      // which three stay: the body armour's own first (C17: 1-3 on the armour), a horizontal 2 px run first among them (the
      // bright sheen's tell), then the larger (the T5-T6 sparkle keeps its pixels: C12b); then the helm's, the trim's, the greaves
      auto run2 = [&](const Cl& c) {
        for (int q : c.px) for (int q2 : c.px) if (q2 == q + 1 && q / w == q2 / w) return true;
        return false;
      };
      std::stable_sort(all.begin(), all.end(), [&](const Cl& a, const Cl& b) {
        const bool ra = run2(a), rb = run2(b), ba = a.key < (1 << 20), bb = b.key < (1 << 20), ma = (a.key & (1 << 19)) == 0, mb = (b.key & (1 << 19)) == 0;
        if (ba != bb) return ba;
        if (ma != mb) return ma;
        if (ra != rb) return ra;
        if (a.px.size() != b.px.size()) return a.px.size() > b.px.size();
        return a.key < b.key;
      });
      std::vector<char> keep(all.size(), 0);
      for (size_t i = 0; i < all.size() && i < 3; i++) keep[i] = 1;
      // the bright sheen's 2 px run stays wherever it was laid (a plate with no lit pair lays it on the helm or the shield)
      if (!run2(all[0]) && !run2(all[1]) && !run2(all[2]))
        for (size_t i = 3; i < all.size(); i++) if (run2(all[i])) { keep[2] = 0; keep[i] = 1; break; }
      for (size_t i = 0; i < all.size(); i++)
        if (!keep[i]) for (int q : all[i].px) at(q % w, q / w) = code(codeSlot(at(q % w, q / w)), 3);
    }
  }
  // (A8) and again after the glints moved: a glint taken down to a lone step-3 pixel takes the body too
  // a lone lit pixel on any metal of the figure (a step-3 pixel with no lit 4-neighbour: a broken 1 px
  // dash on a plate, a greave, a gauntlet) is noise at 1x: it takes the body (step 2). Highlights are clean planes of 2 x 2
  // or more, glints the step-4 clusters
  {
    loneLitPass(o);
  }
}
// (P2c envelope r2, FORGE-MATERIAL prototype; PLAN A4/A7, D53) THE GLASS PASS: obsidian and every GLASS alloy render as glass,
// not lit iron. Per metal piece (a 4-connected run of Metal steps 1-4 inside its step-0 outline): the light stays only on
// the piece's TOP-LEFT edge (a lit band 1 px deep: the facet's hard edge), the inside of the plane goes to the dark glass body
// (step 2), the LOWER-RIGHT inside edge of each piece of 12 px or more takes the cold rim (step 3: light through the glass),
// and the two or three largest pieces (body armour first) take ONE hard specular each: a 2 px step-4 bar on the lit edge.
// Every older step-4 pixel on the glass goes, so the glints are exactly these bars (2-6 px, 2-3 a figure, one off the helm).
void glassPass(const DollRecipe& r, FrameOut& o) {
  if (!r.has("glass") || o.bx1 < o.bx0) return;
  // (envelope r3, FORGE-MATERIAL; A7) a GLASS alloy's Metal 2 (its second hue: Thaerglass's shoulders) is glass too; obsidian's
  // Metal 2 is the gold trim and stays metal
  // (envelope fix round, FORGE-MATERIAL; D62) a GLASS alloy's Metal 2 is now its river SILVER (the rims, seams and brow band
  // set round the panes): it stays metal, as obsidian's gold trim does; the leather of its harness still grows in the glass
  const bool alloyGlass = !r.has("metal-obsidian");
  const bool glass2 = false;
  const int x0 = o.bx0, y0 = o.by0, w = o.bx1 - o.bx0 + 1, h = o.by1 - o.by0 + 1;
  auto at = [&](int x, int y) -> uint8_t& { return o.code[(size_t)(y0 + y) * CW + (x0 + x)]; };
  // (D59) W7's face box + 1 px (the head anchor's 16 x 14 box: x 2-13, y 4-13): no glass light there in any facing, so a
  // mask's lower face is one dark plane and no lit bar or specular crosses the face
  const int fhx = CPX + o.anchors[(int)Anchor::Head].x, fhy = CPY + o.anchors[(int)Anchor::Head].y;
  auto face = [&](int x, int y) { const int X = x0 + x, Y = y0 + y; return X >= fhx + 2 && X <= fhx + 13 && Y >= fhy + 4 && Y <= fhy + 13; };
  auto gslot = [&](uint8_t c) { return c >= 2 && codeStep(c) >= 1 && (codeSlot(c) == Slot::Metal || (glass2 && codeSlot(c) == Slot::Metal2)); };
  auto glass = [&](int x, int y) { return x >= 0 && y >= 0 && x < w && y < h && gslot(at(x, y)); };
  // the planes: a piece is a run of steps 2-4 (the drawn step-1 shadow lines part the lames and facets, as the outline does)
  auto plane = [&](int x, int y) { return glass(x, y) && codeStep(at(x, y)) >= 2; };
  // (envelope r3, A7) a GLASS alloy's body is glass all through: the leather of its armour and shoulder pieces (Thaerglass's
  // sylvan cut is a leather harness) is grown in the glass (Metal, its step kept); obsidian keeps its leather straps
  if (alloyGlass)
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        uint8_t& c = at(x, y);
        if (c >= 2 && codeSlot(c) == Slot::Leather && codeStep(c) >= 1 && (g_bodyPiece[(size_t)(y0 + y) * CW + (x0 + x)] & 1)) c = code(Slot::Metal, codeStep(c));
      }
  std::vector<int> lab((size_t)w * h, -1);
  struct Pc { std::vector<int> px; int x0, y0, x1, y1; bool body; Slot slot; };
  std::vector<Pc> pcs;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      if (!plane(x, y) || lab[(size_t)y * w + x] >= 0) continue;
      const Slot sl = codeSlot(at(x, y));
      Pc pc{{}, x, y, x, y, false, sl};
      std::vector<int> st{y * w + x};
      lab[(size_t)y * w + x] = (int)pcs.size();
      while (!st.empty()) {
        const int q = st.back(); st.pop_back();
        pc.px.push_back(q);
        const int qx = q % w, qy = q / w;
        pc.x0 = std::min(pc.x0, qx); pc.x1 = std::max(pc.x1, qx); pc.y0 = std::min(pc.y0, qy); pc.y1 = std::max(pc.y1, qy);
        pc.body |= (g_bodyPiece[(size_t)(y0 + qy) * CW + (x0 + qx)] & 1) != 0;
        const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& e : d) {
          const int nx = qx + e[0], ny = qy + e[1];
          if (plane(nx, ny) && codeSlot(at(nx, ny)) == sl && lab[(size_t)ny * w + nx] < 0) { lab[(size_t)ny * w + nx] = (int)pcs.size(); st.push_back(ny * w + nx); }
        }
      }
      pcs.push_back(std::move(pc));
    }
  auto in = [&](int x, int y, int id) { return x >= 0 && y >= 0 && x < w && y < h && lab[(size_t)y * w + x] == id; };
  auto put = [&](int x, int y, int s) { at(x, y) = code(codeSlot(at(x, y)), s); };
  for (int id = 0; id < (int)pcs.size(); id++) {
    const Pc& pc = pcs[(size_t)id];
    const bool big = pc.px.size() >= 4 && pc.y1 - pc.y0 >= 1;
    const int mx = (pc.x0 + pc.x1 + 1) / 2, my = (pc.y0 + pc.y1 + 1) / 2;
    const int litX = pc.x0 + (pc.x1 - pc.x0) * 3 / 5;                    // the edge light fades out past three fifths of the width
    std::vector<uint8_t> nst(pc.px.size()), rim(pc.px.size(), 0);
    for (size_t i = 0; i < pc.px.size(); i++) {
      const int x = pc.px[i] % w, y = pc.px[i] / w;
      const int st = codeStep(at(x, y));
      const bool top = !in(x, y - 1, id), lft = !in(x - 1, y, id), rgt = !in(x + 1, y, id), bot = !in(x, y + 1, id);
      // black glass is dark: the light is a LINE on the top edge from the lit corner (and down the left edge of a big plane's
      // upper half), never the whole facet
      const bool lit = pc.y1 - pc.y0 >= 2 && ((top && x <= litX) || (lft && y <= my && pc.px.size() >= 20));   // a thin lame stays dark glass
      int s = st == 3 ? (lit ? 3 : 2) : st;                              // the inside dark glass (glints kept)
      const bool deep = pc.px.size() >= 20, rgt2 = rgt || (deep && !in(x + 2, y, id)), bot2 = bot || (deep && !in(x, y + 2, id));
      if (big && st < 4 && !top && ((rgt2 && y >= my) || (bot2 && x >= mx))) { s = 3; rim[i] = 1; }   // 2 px deep on a big plane   // the cold rim (D53): the edge light through the glass
      nst[i] = (uint8_t)s;
    }
    // (envelope r3, A4/A7) THE REFLECTION: a 2 px diagonal band (rising to the right) across each plane of 14 px or more, two
    // fifths of the way in from the lit corner: light lying ACROSS the glass, the tell that it is glass and not lacquer or cloth
    if (pc.px.size() >= 20 && pc.x1 - pc.x0 >= 4 && pc.y1 - pc.y0 >= 4) {
      const int span = (pc.x1 - pc.x0) + (pc.y1 - pc.y0);
      const int c0 = pc.x0 + pc.y0 + std::max(2, span * 2 / 5);
      for (size_t i = 0; i < pc.px.size(); i++) {
        const int x = pc.px[i] % w, y = pc.px[i] / w, c = x + y;
        if ((c == c0 || c == c0 + 1) && nst[i] <= 2) { nst[i] = 3; rim[i] = 2; }
      }
    }
    // no pinched pixel: a dark pixel whose piece neighbours are all lit joins the light (the band beside the lit edge)
    {
      std::vector<int> idx((size_t)w * h, -1);
      for (size_t i = 0; i < pc.px.size(); i++) idx[(size_t)pc.px[i]] = (int)i;
      std::vector<size_t> up;
      for (size_t i = 0; i < pc.px.size(); i++) {
        if (nst[i] > 2) continue;
        const int x = pc.px[i] % w, y = pc.px[i] / w;
        int n = 0, lit = 0;
        const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& e : d)
          if (in(x + e[0], y + e[1], id)) { n++; lit += nst[(size_t)idx[(size_t)((y + e[1]) * w + x + e[0])]] >= 3; }
        if (n >= 2 && lit == n) up.push_back(i);
      }
      for (size_t i : up) nst[i] = 3;
    }
    // the glass stays DARK: at most two fifths of a piece lit (steps 3-4); the rim and the glints stay, the edge light nearest
    // the lit corner stays, the farthest goes first (a pale glass's facet read as white plaster when it was all light)
    if (pc.px.size() >= 6) {
      std::vector<std::pair<int, size_t>> lit;
      int n4 = 0;
      for (size_t i = 0; i < pc.px.size(); i++) {
        if (nst[i] == 4) n4++;
        else if (nst[i] == 3 && !rim[i]) lit.push_back({(pc.px[i] % w - pc.x0) + (pc.px[i] / w - pc.y0), i});
      }
      int nr = 0;
      for (uint8_t q : rim) nr += q;
      const int cap = (int)pc.px.size() * 9 / 20 - n4 - nr;
      std::stable_sort(lit.begin(), lit.end());
      for (size_t k = (size_t)std::max(0, cap); k < lit.size(); k++) nst[lit[k].second] = 2;
    }
    for (size_t i = 0; i < pc.px.size(); i++) put(pc.px[i] % w, pc.px[i] / w, nst[i]);
  }
  // the cold rim on the WHOLE body piece too (the step-1 shadow lines included: the r2 rule): its lower-right inside edge
  {
    std::vector<int> lab2((size_t)w * h, -1);
    int nid = 0;
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        if (!glass(x, y) || lab2[(size_t)y * w + x] >= 0) continue;
        const Slot sl = codeSlot(at(x, y));
        std::vector<int> px, st{y * w + x};
        int ax0 = x, ay0 = y, ax1 = x, ay1 = y;
        lab2[(size_t)y * w + x] = nid;
        while (!st.empty()) {
          const int q = st.back(); st.pop_back(); px.push_back(q);
          const int qx = q % w, qy = q / w;
          ax0 = std::min(ax0, qx); ax1 = std::max(ax1, qx); ay0 = std::min(ay0, qy); ay1 = std::max(ay1, qy);
          const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
          for (const auto& e : d) {
            const int nx = qx + e[0], ny = qy + e[1];
            if (glass(nx, ny) && codeSlot(at(nx, ny)) == sl && lab2[(size_t)ny * w + nx] < 0) { lab2[(size_t)ny * w + nx] = nid; st.push_back(ny * w + nx); }
          }
        }
        if (px.size() >= 8 && ax1 - ax0 >= 2 && ay1 - ay0 >= 2) {
          const int mx = (ax0 + ax1 + 1) / 2, my = (ay0 + ay1 + 1) / 2;
          auto in2 = [&](int xx, int yy) { return xx >= 0 && yy >= 0 && xx < w && yy < h && lab2[(size_t)yy * w + xx] == nid; };
          std::vector<int> set;
          for (int q : px) {
            const int qx = q % w, qy = q / w;
            const bool tl = !in2(qx - 1, qy) || !in2(qx, qy - 1), br = !in2(qx + 1, qy) || !in2(qx, qy + 1);
            if (br && !tl && codeStep(at(qx, qy)) < 3 && (qx >= mx || qy >= my) && (g_bodyPiece[(size_t)(y0 + qy) * CW + (x0 + qx)] & 1)) set.push_back(q);
          }
          for (int q : set) put(q % w, q / w, 3);
        }
        nid++;
      }
  }
  // (D59) the face box goes dark: every glass pixel there at step 3-4 (the edge light, the reflection band, an older glint)
  // takes the dark glass body (step 2)
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      if (face(x, y) && at(x, y) >= 2 && codeStep(at(x, y)) >= 3 && (glass(x, y) || codeSlot(at(x, y)) == Slot::Metal2)) put(x, y, 2);
  // the trim's own step-4 points (the filigree's lit scroll ends) take its lit plane: on glass the speculars are the bars only
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const uint8_t c = at(x, y);
      if (!alloyGlass && c >= 2 && codeSlot(c) == Slot::Metal2 && codeStep(c) == 4) at(x, y) = code(Slot::Metal2, 3);   // (a GLASS alloy's silver keeps its glint)
    }
  // the speculars: the glint pass's glints stay where they are (C17: 1-3 on the armour); a 1 px one grows into a 2 px bar
  // along its piece (a matte sheen keeps its single point, C17); a cluster over 6 px keeps its first 6; with fewer than two,
  // the largest pieces (body armour first) take a 2 px bar at the first lit-edge pair nearest the top-left light
  const bool matte = r.sheen == 1;
  auto m4 = [&](int x, int y) { return glass(x, y) && codeStep(at(x, y)) == 4; };
  int groups = 0;
  {
    std::vector<char> seen((size_t)w * h, 0);
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        if (!m4(x, y) || seen[(size_t)y * w + x]) continue;
        std::vector<int> g, st{y * w + x};
        seen[(size_t)y * w + x] = 1;
        while (!st.empty()) {
          const int q = st.back(); st.pop_back(); g.push_back(q);
          for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
              const int nx = q % w + dx, ny = q / w + dy;
              if (m4(nx, ny) && !seen[(size_t)ny * w + nx]) { seen[(size_t)ny * w + nx] = 1; st.push_back(ny * w + nx); }
            }
        }
        groups++;
        // (envelope fix r2, DOLL; D62 "short diagonal bars or diamonds") on a GLASS alloy a cluster keeps 3 px at most, laid
        // as the diagonal: its 2 px nearest the top-left and a tail one row down and left (r4's 6 px straight bar across
        // a_thaer's chest facing R read as a mouth under the seam's glow dots: the face the judge saw); obsidian keeps 6
        if (alloyGlass && g.size() > 3) {
          std::sort(g.begin(), g.end(), [&](int a, int b) { return (a % w + a / w) != (b % w + b / w) ? (a % w + a / w) < (b % w + b / w) : a < b; });
          for (size_t i = 2; i < g.size(); i++) put(g[i] % w, g[i] / w, 3);
          const int tx = g[0] % w - 1, ty = g[0] / w + 1;
          if (in(tx, ty, lab[(size_t)g[0]]) && !face(tx, ty)) put(tx, ty, 4);
          g.resize(2);
        }
        for (size_t i = 6; i < g.size(); i++) put(g[i] % w, g[i] / w, 3);
        if (g.size() != 1 || matte) continue;
        const int gx = g[0] % w, gy = g[0] / w, id = lab[(size_t)g[0]];
        bool grown = false;
        for (int dx : {1, -1})
          if (!grown && in(gx + dx, gy, id) && codeStep(at(gx + dx, gy)) >= 2) { put(gx + dx, gy, 4); grown = true; }
        for (int dy : {1, -1})
          if (!grown && in(gx, gy + dy, id) && codeStep(at(gx, gy + dy)) >= 2) { put(gx, gy + dy, 4); grown = true; }
        if (!grown) { put(gx, gy, 3); groups--; }
      }
  }
  std::vector<int> order;
  for (int id = 0; id < (int)pcs.size(); id++) if (pcs[(size_t)id].px.size() >= 10) order.push_back(id);
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    if (pcs[(size_t)a].body != pcs[(size_t)b].body) return pcs[(size_t)a].body;
    return pcs[(size_t)a].px.size() > pcs[(size_t)b].px.size();
  });
  for (int id : order) {
    if (groups >= 3) break;
    bool has4 = false;
    for (int q : pcs[(size_t)id].px) has4 |= codeStep(at(q % w, q / w)) == 4;
    if (has4) continue;
    int bx = -1, by = -1, bk = 1 << 30;
    for (int q : pcs[(size_t)id].px) {
      const int x = q % w, y = q / w;
      if (!in(x + 1, y, id) || codeStep(at(x, y)) < 2 || codeStep(at(x, y)) > 3 || codeStep(at(x + 1, y)) < 2 || codeStep(at(x + 1, y)) > 3) continue;
      if (face(x, y) || face(x + 1, y) || face(x - 1, y + 1)) continue;   // (D59) never a specular on the face
      if (in(x, y - 1, id) && in(x + 1, y - 1, id)) continue;          // on the top edge
      const int k = (y - pcs[(size_t)id].y0) * 2 + (x - pcs[(size_t)id].x0) + (x == pcs[(size_t)id].x0 ? 3 : 0);
      if (k < bk) { bk = k; bx = x; by = y; }
    }
    if (bx < 0) continue;
    put(bx, by, 4);
    if (!matte) {
      put(bx + 1, by, 4);
      // a big plane's specular is a hard diagonal (the light lying across the facet): its tail one row down and left
      if (pcs[(size_t)id].body && pcs[(size_t)id].px.size() >= 30 && in(bx - 1, by + 1, id) && codeStep(at(bx - 1, by + 1)) <= 2) put(bx - 1, by + 1, 4);
    }
    groups++;
  }
  // (wave 2 int2, C17 "1-3 on the armour") the glints already laid on the helm, the greaves and the silver fill the three
  // before the loop above reaches a body piece, and a glass body made only of narrow facets (the shard and crystal shoulders'
  // spikes under a tabard: no plane of 10 px with a lit top-edge pair) never qualifies for a bar: then the body piece with
  // the most lit pixels takes ONE specular at its lit pixel nearest the top-left light (a 2 px pair when the facet allows
  // and the sheen is not matte), and the figure's farthest glint off the body goes to the lit plane so three stay at most
  {
    auto bodyAt = [&](int x, int y) { return (g_bodyPiece[(size_t)(y0 + y) * CW + (x0 + x)] & 1) != 0; };
    bool body4 = false;
    for (int y = 0; y < h && !body4; y++)
      for (int x = 0; x < w && !body4; x++) body4 = m4(x, y) && bodyAt(x, y);
    int best = -1, bestN = 0;
    for (int id = 0; !body4 && id < (int)pcs.size(); id++) {
      if (!pcs[(size_t)id].body) continue;
      int n = 0;
      for (int q : pcs[(size_t)id].px) n += codeStep(at(q % w, q / w)) >= 2 && !face(q % w, q / w);
      if (n > bestN) { bestN = n; best = id; }
    }
    if (best >= 0 && bestN >= 2) {
      int bx = -1, by = -1, bk = 1 << 30;
      for (int q : pcs[(size_t)best].px) {
        const int x = q % w, y = q / w;
        if (codeStep(at(x, y)) < 2 || face(x, y)) continue;
        const int k = (y - pcs[(size_t)best].y0) * 2 + (x - pcs[(size_t)best].x0);
        if (k < bk) { bk = k; bx = x; by = y; }
      }
      if (bx >= 0) {
        put(bx, by, 4);
        if (!matte) {
          if (in(bx + 1, by, best) && codeStep(at(bx + 1, by)) >= 2 && !face(bx + 1, by)) put(bx + 1, by, 4);
          else if (in(bx, by + 1, best) && codeStep(at(bx, by + 1)) >= 2 && !face(bx, by + 1)) put(bx, by + 1, 4);
        }
        // the cap: every step-4 cluster of the figure's metal (Metal and Metal 2, 8-connected); over three, the off-body
        // clusters farthest from the top-left light take step 3
        auto any4 = [&](int x, int y) {
          if (x < 0 || y < 0 || x >= w || y >= h) return false;
          const uint8_t c = at(x, y);
          return c >= 2 && (codeSlot(c) == Slot::Metal || codeSlot(c) == Slot::Metal2) && codeStep(c) == 4;
        };
        struct G4 { int key; bool body; std::vector<int> px; };
        std::vector<G4> gs;
        std::vector<char> seen((size_t)w * h, 0);
        for (int y = 0; y < h; y++)
          for (int x = 0; x < w; x++) {
            if (!any4(x, y) || seen[(size_t)y * w + x]) continue;
            G4 g{1 << 30, false, {}};
            std::vector<int> st{y * w + x};
            seen[(size_t)y * w + x] = 1;
            while (!st.empty()) {
              const int q = st.back(); st.pop_back(); g.px.push_back(q);
              g.key = std::min(g.key, (q / w) * 2 + q % w);
              g.body |= bodyAt(q % w, q / w);
              for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                  const int nx = q % w + dx, ny = q / w + dy;
                  if (any4(nx, ny) && !seen[(size_t)ny * w + nx]) { seen[(size_t)ny * w + nx] = 1; st.push_back(ny * w + nx); }
                }
            }
            gs.push_back(std::move(g));
          }
        std::stable_sort(gs.begin(), gs.end(), [](const G4& a, const G4& b) { return a.body != b.body ? !a.body : a.key > b.key; });
        for (size_t i = 0; i < gs.size() && gs.size() - i > 3 && !gs[i].body; i++)
          for (int q : gs[i].px) at(q % w, q / w) = code(codeSlot(at(q % w, q / w)), 3);
      }
    }
  }
  // (envelope fix round, FORGE-MATERIAL; D62 "no pale patch") on a GLASS alloy no connected step 3-4 region of its metal
  // (Metal and the silver) reaches 12 px: the light is a small top-left facet, a rim line and 2-6 px glints, never a pale
  // plane (the enamel and jade tell). A region of 12 or more keeps its rim pixels (step 3 within 2 px of the outline at the
  // lower right) and its glints longest and gives back the pixels farthest from its top-left corner first; a rim alone that
  // still runs 12 or more is cut in its middle
  if (alloyGlass) {
    auto anyM = [&](int x, int y) { if (x < 0 || y < 0 || x >= w || y >= h) return false; const uint8_t c = at(x, y); return c >= 2 && codeStep(c) >= 1 && (codeSlot(c) == Slot::Metal || codeSlot(c) == Slot::Metal2); };
    auto litM = [&](int x, int y) { return anyM(x, y) && codeStep(at(x, y)) >= 3; };
    auto rimLike = [&](int x, int y) {
      if (codeStep(at(x, y)) != 3) return false;
      for (int d = 1; d <= 2; d++)
        for (int e = 0; e < 3; e++) {
          const int nx = x + (e != 1 ? d : 0), ny = y + (e != 0 ? d : 0);
          if (nx >= w || ny >= h || at(nx, ny) < 2 || codeStep(at(nx, ny)) == 0) return true;
        }
      return false;
    };
    for (int pass = 0; pass < 32; pass++) {
      bool any = false;
      std::vector<char> seen((size_t)w * h, 0);
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
          if (seen[(size_t)y * w + x] || !litM(x, y)) continue;
          std::vector<int> g, st{y * w + x};
          seen[(size_t)y * w + x] = 1;
          int rx0 = x, ry0 = y;
          while (!st.empty()) {
            const int q = st.back(); st.pop_back(); g.push_back(q);
            rx0 = std::min(rx0, q % w); ry0 = std::min(ry0, q / w);
            const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& e : d) {
              const int nx = q % w + e[0], ny = q / w + e[1];
              if (litM(nx, ny) && !seen[(size_t)ny * w + nx]) { seen[(size_t)ny * w + nx] = 1; st.push_back(ny * w + nx); }
            }
          }
          if (g.size() < 12) continue;
          any = true;
          // the order to give back: plain lit first (farthest from the top-left first), then rims, never the glints unless alone
          std::vector<std::pair<int, int>> order;
          for (int q : g) {
            const int qx = q % w, qy = q / w, st4 = codeStep(at(qx, qy)) == 4;
            const int cls = st4 ? 2 : rimLike(qx, qy) ? 1 : 0;
            order.push_back({cls * 1000 - ((qx - rx0) + (qy - ry0)), q});
          }
          std::sort(order.begin(), order.end());
          const size_t drop = g.size() - 11;
          if (order[drop - 1].first < 1000) { for (size_t i = 0; i < drop && order[i].first < 1000; i++) put(order[i].second % w, order[i].second / w, 2); }
          else put(g[g.size() / 2] % w, g[g.size() / 2] / w, 2);   // a rim alone: cut it
        }
      if (!any) break;
    }
  }
  // (envelope fix round, FORGE-MATERIAL; D59 / D62 "the rim as one run of 3 px or more") every convex body piece (a
  // 4-connected run of glass, steps 1-4, of 12 px or more on the armour or a shoulder) carries its light-through-the-glass as
  // ONE run: where its longest rim run is under 3 px, the longest chain of its lower-right inside edge (a pixel whose right or
  // lower neighbour is the outline, a drawn line or the ground, past the piece's middle) takes step 3 for up to 5 px, its
  // up-left neighbour kept dark glass (steps 1-2: the core the rim reads against). Never in the face box.
  // (envelope fix r2, DOLL; D62 "on the helm, the torso and each shoulder") the helm is the third part: r4's helm rim came
  // only from the drawn map (a_thaer's dome drawn at step 1 with a step-2 right edge: the glass tell 0 px on the helm), so the
  // run is now laid on the helm too, down the dome's right side below its upper third (the
  // wings above are their own blades; the dome narrows to the brow band, so its lower half alone held 2 px)
  for (int part = 0; part < 3; part++) {   // the torso (armour + chest motif), the shoulders and the helm apart: each its own run
    std::vector<int> lab3((size_t)w * h, -1);
    int nid = 0;
    auto gl = [&](int x, int y) {
      if (!glass(x, y)) return false;
      const uint8_t b = g_bodyPiece[(size_t)(y0 + y) * CW + (x0 + x)];
      return part == 0 ? (b & 4) != 0 : part == 1 ? ((b & 1) && !(b & 4) && !(b & 16)) : (b & 16) != 0;
    };
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        if (!gl(x, y) || lab3[(size_t)y * w + x] >= 0) continue;
        std::vector<int> px, st{y * w + x};
        lab3[(size_t)y * w + x] = nid;
        int ax0 = x, ax1 = x, ay0 = y, ay1 = y;
        while (!st.empty()) {
          const int q = st.back(); st.pop_back(); px.push_back(q);
          const int qx = q % w, qy = q / w;
          ax0 = std::min(ax0, qx); ax1 = std::max(ax1, qx); ay0 = std::min(ay0, qy); ay1 = std::max(ay1, qy);
          const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
          for (const auto& e : d) {
            const int nx = qx + e[0], ny = qy + e[1];
            if (gl(nx, ny) && lab3[(size_t)ny * w + nx] < 0) { lab3[(size_t)ny * w + nx] = nid; st.push_back(ny * w + nx); }
          }
        }
        const int id3 = nid++;
        if ((int)px.size() < (part == 0 ? 6 : 12)) continue;
        const int mx = (ax0 + ax1 + 1) / 2, my = (ay0 + ay1 + 1) / 2;
        auto in3 = [&](int xx, int yy) { return xx >= 0 && yy >= 0 && xx < w && yy < h && lab3[(size_t)yy * w + xx] == id3; };
        auto edgeBR = [&](int xx, int yy) {   // within 2 px right, down or diagonally down-right: the outline, a line or the ground
          for (int d = 1; d <= 2; d++)
            for (int e = 0; e < 3; e++) {
              const int nx = xx + (e != 1 ? d : 0), ny = yy + (e != 0 ? d : 0);
              if (nx >= w || ny >= h || at(nx, ny) < 2 || codeStep(at(nx, ny)) == 0) return true;
            }
          return false;
        };
        auto core = [&](int xx, int yy) { return (in3(xx - 1, yy) && codeStep(at(xx - 1, yy)) <= 2) || (in3(xx - 1, yy - 1) && codeStep(at(xx - 1, yy - 1)) <= 2); };
        auto cand = [&](int xx, int yy) { return in3(xx, yy) && !face(xx, yy) && codeStep(at(xx, yy)) <= 3 && edgeBR(xx, yy) && (part == 2 ? (xx >= mx && yy >= ay0 + (ay1 - ay0) / 3) : (xx >= mx || yy >= my)) && (in3(xx - 1, yy) || in3(xx, yy - 1)); };
        // the longest chain of candidates, and the longest run already lit: a 4-connected run (envelope fix r2: r4 counted
        // every lit candidate of a chain, so a staircase of diagonal step-3 pixels on a_thaer's dome counted 4 and the run was
        // skipped, then the lone-pixel rule below took each of them back: the helm's glass tell 0 px)
        std::vector<int> chain;
        std::vector<char> seen((size_t)w * h, 0);
        for (int q : px) {
          const int qx = q % w, qy = q / w;
          if (seen[(size_t)q] || !cand(qx, qy)) continue;
          std::vector<int> c, s2{q};
          seen[(size_t)q] = 1;
          while (!s2.empty()) {
            const int z = s2.back(); s2.pop_back(); c.push_back(z);
            const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& e : d) {
              const int nx = z % w + e[0], ny = z / w + e[1];
              if (nx >= 0 && ny >= 0 && nx < w && ny < h && !seen[(size_t)ny * w + nx] && cand(nx, ny)) { seen[(size_t)ny * w + nx] = 1; s2.push_back(ny * w + nx); }
            }
          }
          if (c.size() > chain.size()) chain = c;
        }
        auto litc = [&](int xx, int yy) { return cand(xx, yy) && codeStep(at(xx, yy)) == 3 && core(xx, yy); };
        int best = 0;
        std::fill(seen.begin(), seen.end(), (char)0);
        for (int q : px) {
          if (seen[(size_t)q] || !litc(q % w, q / w)) continue;
          int n = 0;
          std::vector<int> s2{q};
          seen[(size_t)q] = 1;
          while (!s2.empty()) {
            const int z = s2.back(); s2.pop_back(); n++;
            const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& e : d) {
              const int nx = z % w + e[0], ny = z / w + e[1];
              if (nx >= 0 && ny >= 0 && nx < w && ny < h && !seen[(size_t)ny * w + nx] && litc(nx, ny)) { seen[(size_t)ny * w + nx] = 1; s2.push_back(ny * w + nx); }
            }
          }
          best = std::max(best, n);
        }
        if (best >= 3 || chain.size() < 3) continue;
        // from the chain's pixel nearest the lower-right corner, 5 px at most grown along the chain (one 4-connected run)
        {
          const int far = *std::max_element(chain.begin(), chain.end(), [&](int a, int b) { return (a % w + a / w) < (b % w + b / w); });
          std::vector<int> run{far}, fr{far};
          std::vector<char> inC((size_t)w * h, 0), took((size_t)w * h, 0);
          for (int z : chain) inC[(size_t)z] = 1;
          took[(size_t)far] = 1;
          while (!fr.empty() && run.size() < 5) {
            std::vector<int> nx;
            for (int z : fr) {
              const int d[4][2] = {{0, -1}, {-1, 0}, {0, 1}, {1, 0}};
              for (const auto& e : d) {
                const int xx = z % w + e[0], yy = z / w + e[1];
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                const int q = yy * w + xx;
                if (inC[(size_t)q] && !took[(size_t)q] && run.size() < 5) { took[(size_t)q] = 1; run.push_back(q); nx.push_back(q); }
              }
            }
            fr = nx;
          }
          chain = run;
        }
        for (int z : chain) {
          const int zx = z % w, zy = z / w;
          put(zx, zy, 3);
          if (in3(zx - 1, zy) && codeStep(at(zx - 1, zy)) == 3 && std::find(chain.begin(), chain.end(), z - 1) == chain.end()) put(zx - 1, zy, 2);
        }
      }
  }
  // (envelope fix round, FORGE-MATERIAL; D62 "no pale patch") on a GLASS alloy no connected step 3-4 region of its metal
  // (Metal and the silver) reaches 12 px: the light is a small top-left facet, a rim line and 2-6 px glints, never a pale
  // plane (the enamel and jade tell). A region of 12 or more keeps its rim pixels (step 3 within 2 px of the outline at the
  // lower right) and its glints longest and gives back the pixels farthest from its top-left corner first; a rim alone that
  // still runs 12 or more is cut
  if (alloyGlass) {
    auto anyM = [&](int x, int y) { if (x < 0 || y < 0 || x >= w || y >= h) return false; const uint8_t c = at(x, y); return c >= 2 && codeStep(c) >= 1 && (codeSlot(c) == Slot::Metal || codeSlot(c) == Slot::Metal2); };
    auto litM = [&](int x, int y) { return anyM(x, y) && codeStep(at(x, y)) >= 3; };
    auto rimLike = [&](int x, int y) {
      if (codeStep(at(x, y)) != 3) return false;
      for (int d = 1; d <= 2; d++)
        for (int e = 0; e < 3; e++) {
          const int nx = x + (e != 1 ? d : 0), ny = y + (e != 0 ? d : 0);
          if (nx >= w || ny >= h || at(nx, ny) < 2 || codeStep(at(nx, ny)) == 0) return true;
        }
      return false;
    };
    for (int pass = 0; pass < 32; pass++) {
      bool any = false;
      std::vector<char> seen((size_t)w * h, 0);
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
          if (seen[(size_t)y * w + x] || !litM(x, y)) continue;
          std::vector<int> g, st{y * w + x};
          seen[(size_t)y * w + x] = 1;
          int rx0 = x, ry0 = y;
          while (!st.empty()) {
            const int q = st.back(); st.pop_back(); g.push_back(q);
            rx0 = std::min(rx0, q % w); ry0 = std::min(ry0, q / w);
            const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& e : d) {
              const int nx = q % w + e[0], ny = q / w + e[1];
              if (litM(nx, ny) && !seen[(size_t)ny * w + nx]) { seen[(size_t)ny * w + nx] = 1; st.push_back(ny * w + nx); }
            }
          }
          if (g.size() < 12) continue;
          any = true;
          std::vector<std::pair<int, int>> order;   // plain lit first (farthest from the top-left first), then rims, glints last
          for (int q : g) {
            const int qx = q % w, qy = q / w, st4 = codeStep(at(qx, qy)) == 4;
            const int cls = st4 ? 2 : rimLike(qx, qy) ? 1 : 0;
            order.push_back({cls * 1000 - ((qx - rx0) + (qy - ry0)), q});
          }
          std::sort(order.begin(), order.end());
          const size_t drop = g.size() - 11;
          if (order[drop - 1].first < 1000) { for (size_t i = 0; i < drop; i++) put(order[i].second % w, order[i].second / w, 2); }
          else { std::sort(g.begin(), g.end()); put(g[g.size() / 2] % w, g[g.size() / 2] / w, 2); }   // a rim alone: cut it
        }
      if (!any) break;
    }
  }
  // no lone lit pixel (a step-3 pixel with no lit 4-neighbour takes the body)
  std::vector<int> lone;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      if (!glass(x, y) || codeStep(at(x, y)) != 3) continue;
      int n = 0;
      for (int k = 0; k < 4; k++) {
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        if (glass(x + dx[k], y + dy[k]) && codeStep(at(x + dx[k], y + dy[k])) >= 3) n++;
      }
      if (!n) lone.push_back(y * w + x);
    }
  for (int q : lone) put(q % w, q / w, 2);
  // (envelope fix r2, DOLL; D62 "the glowing seams") on a GLASS alloy a seam is a line of light: a glass pixel between two
  // seam pixels (Glow, left and right or above and below) burns with them, so a dotted seam (r4: 1 px glow dots 2 px apart
  // across a_thaer's chest facing R, read as two eyes) reads as one seam. Never in the face box
  if (alloyGlass) {
    auto gw = [&](int x, int y) { return x >= 0 && y >= 0 && x < w && y < h && at(x, y) >= 2 && codeSlot(at(x, y)) == Slot::Glow; };
    std::vector<std::pair<int, uint8_t>> fill;
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        if (!glass(x, y) || face(x, y)) continue;
        if (gw(x - 1, y) && gw(x + 1, y)) fill.push_back({y * w + x, at(x - 1, y)});
        else if (gw(x, y - 1) && gw(x, y + 1)) fill.push_back({y * w + x, at(x, y - 1)});
      }
    for (const auto& f : fill) at(f.first % w, f.first / w) = code(Slot::Glow, std::min(2, codeStep(f.second)));
  }
}
// (envelope fix round, FORGE-MATERIAL; D61) STARMETAL's bands as SEEN: the torso (the armour and its chest motif, bit 4 of the
// body-piece mask) re-banded on the composed frame, 2 metal rows of violet (Metal) then 2 of cyan (Metal 2) counted down the
// rows that show torso metal (a belt or a collar over the chest skips its rows, so the bands stay 2 rows tall where the plate
// shows: the per-part bands of r3 fell under the collar and read as one hue, C19 A6 1 band). Steps kept; the glint too.
// (envelope fix round, FORGE-MATERIAL; D60 "fire ... in 2-4 px cracks (Glow)", "dark metal, bright fire") the MYTHIC's
// burning seams: the drawn shadow lines INSIDE its body armour and shoulders (Metal step 1 with no outline or background
// 4-neighbour: the recesses between the plates) burn, Glow step 2 with a step-3 hot point every third pixel of a run, where
// they run 3 px or more (4-connected); shorter recess lines and every line above the torso's top row + 1 stay black (W7's
// face box and the helm are never lit). So the black plate is parted by fire, its lit planes polished silver over it.
void mythicFirePass(const DollRecipe& r, FrameOut& o) {
  if (!(r.has("mythic") || r.has("metal-mythic")) || o.bx1 < o.bx0) return;
  const int x0 = o.bx0, y0 = o.by0, w = o.bx1 - o.bx0 + 1, h = o.by1 - o.by0 + 1;
  auto at = [&](int x, int y) -> uint8_t& { return o.code[(size_t)(y0 + y) * CW + (x0 + x)]; };
  auto bp = [&](int x, int y) { return g_bodyPiece[(size_t)(y0 + y) * CW + (x0 + x)]; };
  int top = h;
  for (int y = 0; y < h && top == h; y++)
    for (int x = 0; x < w; x++) if (at(x, y) >= 2 && (bp(x, y) & 4)) { top = y; break; }
  auto seam = [&](int x, int y) {
    if (x < 1 || y < 1 || x >= w - 1 || y >= h - 1 || y <= top + 1) return false;
    const uint8_t c = at(x, y);
    if (c < 2 || codeSlot(c) != Slot::Metal || codeStep(c) != 1 || !(bp(x, y) & 1)) return false;
    const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& e : d) if (at(x + e[0], y + e[1]) < 2) return false;   // inside the figure (a drawn step-0 line beside it is the plate's edge)
    return true;
  };
  std::vector<char> seen((size_t)w * h, 0);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      if (seen[(size_t)y * w + x] || !seam(x, y)) continue;
      std::vector<int> px, st{y * w + x};
      seen[(size_t)y * w + x] = 1;
      while (!st.empty()) {
        const int q = st.back(); st.pop_back(); px.push_back(q);
        const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& e : d) {
          const int nx = q % w + e[0], ny = q / w + e[1];
          if (seam(nx, ny) && !seen[(size_t)ny * w + nx]) { seen[(size_t)ny * w + nx] = 1; st.push_back(ny * w + nx); }
        }
      }
      if (px.size() < 3) continue;
      std::sort(px.begin(), px.end());
      for (size_t i = 0; i < px.size(); i++) at(px[i] % w, px[i] / w) = code(Slot::Glow, i % 3 == 1 ? 3 : 2);
    }
  // (envelope fix r2, DOLL; D60 "polished black steel whose lit planes are bright ... a 2 x 2 plane or larger on every piece,
  // against near-black recesses") the black body (Metal step 2, luma under 170 since r2: r4's #C6CAD6 body was silver, so the
  // A5 selftest's pressed-dark Mythic still counted as lit) is polished: each plane of the body armour, the shoulders and the
  // helm (a 4-connected run of Metal steps 2-3 of 4 px or more) takes its lit step on the 70 % of it nearest its top-left
  // corner (the light's side), as one clean diagonal-edged plane; the recesses (step 1), the fire and the glints are kept,
  // and W7's face box + 1 px is never lit
  {
    const int fhx = CPX + o.anchors[(int)Anchor::Head].x - x0, fhy = CPY + o.anchors[(int)Anchor::Head].y - y0;
    auto face = [&](int x, int y) { return x >= fhx + 2 && x <= fhx + 13 && y >= fhy + 4 && y <= fhy + 13; };
    auto pl = [&](int x, int y) {
      if (x < 0 || y < 0 || x >= w || y >= h || face(x, y)) return false;
      const uint8_t c = at(x, y);
      return c >= 2 && codeSlot(c) == Slot::Metal && (codeStep(c) == 2 || codeStep(c) == 3) && (bp(x, y) & 17);
    };
    // the darkness only in the recesses: a step-1 AREA (every pixel of a 2 x 2 block of Metal step 1 on the armour, the
    // shoulders or the helm) is a plate's shadow side, not a recess line: it joins the plate (step 2) before the polish
    {
      auto d1 = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h || face(x, y)) return false;
        const uint8_t c = at(x, y);
        return c >= 2 && codeSlot(c) == Slot::Metal && codeStep(c) == 1 && (bp(x, y) & 17);
      };
      std::vector<int> area;
      for (int y = 0; y + 1 < h; y++)
        for (int x = 0; x + 1 < w; x++)
          if (d1(x, y) && d1(x + 1, y) && d1(x, y + 1) && d1(x + 1, y + 1))
            for (int q : {y * w + x, y * w + x + 1, (y + 1) * w + x, (y + 1) * w + x + 1}) area.push_back(q);
      for (int q : area) at(q % w, q / w) = code(Slot::Metal, 2);
    }
    std::fill(seen.begin(), seen.end(), (char)0);
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) {
        if (seen[(size_t)y * w + x] || !pl(x, y)) continue;
        std::vector<int> px, st{y * w + x};
        seen[(size_t)y * w + x] = 1;
        int ax = x, ay = y;
        while (!st.empty()) {
          const int q = st.back(); st.pop_back(); px.push_back(q);
          ax = std::min(ax, q % w); ay = std::min(ay, q / w);
          const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
          for (const auto& e : d) {
            const int nx = q % w + e[0], ny = q / w + e[1];
            if (pl(nx, ny) && !seen[(size_t)ny * w + nx]) { seen[(size_t)ny * w + nx] = 1; st.push_back(ny * w + nx); }
          }
        }
        if (px.size() < 4) continue;
        std::sort(px.begin(), px.end(), [&](int a, int b) {
          const int da = (a % w - ax) + (a / w - ay), db = (b % w - ax) + (b / w - ay);
          return da != db ? da < db : a < b;
        });
        const size_t lit = std::max<size_t>(4, px.size() * 70 / 100);
        for (size_t i = 0; i < px.size(); i++) at(px[i] % w, px[i] / w) = code(Slot::Metal, i < lit ? 3 : 2);
      }
  }
}
void starBandPass(const DollRecipe& r, FrameOut& o) {
  if (!r.irisBands || !r.has("metal-starmetal") || o.bx1 < o.bx0) return;
  // the torso-metal rows (top to bottom): each row's plain metal pixels and its drawn trim (bit 8: kept in its own slot)
  struct Row { int y, plain, trim1, trim2; };
  std::vector<Row> rows;
  for (int y = o.by0; y <= o.by1; y++) {
    Row rw{y, 0, 0, 0};
    for (int x = o.bx0; x <= o.bx1; x++) {
      const uint8_t c = o.code[(size_t)y * CW + x];
      const uint8_t b = g_bodyPiece[(size_t)y * CW + x];
      if (c < 2 || !(b & 4) || codeStep(c) < 1 || (codeSlot(c) != Slot::Metal && codeSlot(c) != Slot::Metal2)) continue;
      if (b & 8) (codeSlot(c) == Slot::Metal ? rw.trim1 : rw.trim2)++;
      else rw.plain++;
    }
    if (rw.plain + rw.trim1 + rw.trim2) rows.push_back(rw);
  }
  // the phase (0-3) whose 2-row bands, with the trim where it is drawn, give the most alternating runs (C19 A6's count)
  auto runs = [&](int ph) {
    int n = 0, last = 0, len = 0, cur = 0;
    for (size_t i = 0; i <= rows.size(); i++) {
      int lab = 0;
      if (i < rows.size()) {
        const bool two = (((int)i + ph) / 2) & 1;
        const int m1 = rows[i].trim1 + (two ? 0 : rows[i].plain), m2 = rows[i].trim2 + (two ? rows[i].plain : 0);
        lab = m1 > m2 ? 1 : m2 > m1 ? 2 : 0;
      }
      if (i < rows.size() && lab == cur) { len++; continue; }
      if (cur && len >= 2 && cur != last) { n++; last = cur; }
      cur = lab; len = 1;
    }
    return n;
  };
  int best = 0, bestN = -1;
  for (int ph = 0; ph < 4; ph++) { const int n = runs(ph); if (n > bestN) { bestN = n; best = ph; } }
  for (size_t i = 0; i < rows.size(); i++) {
    const Slot to = ((((int)i + best) / 2) & 1) ? Slot::Metal2 : Slot::Metal;
    const int y = rows[i].y;
    for (int x = o.bx0; x <= o.bx1; x++) {
      uint8_t& c = o.code[(size_t)y * CW + x];
      const uint8_t b = g_bodyPiece[(size_t)y * CW + x];
      if (c < 2 || (b & 12) != 4 || codeStep(c) < 1 || codeStep(c) == 4 || (codeSlot(c) != Slot::Metal && codeSlot(c) != Slot::Metal2)) continue;   // the glints stay in the body's Metal (C17)
      c = code(to, codeStep(c));
    }
  }
}
// (forge fix i2, B8 and STYLE_P2 "no 1 px speckle") a round-2 silhouette lays a shoulder mass, a skirt and an over-layer
// the art drew apart; where two of them meet at a corner a single pixel can be left floating clear of the figure (mr_4's
// drop shoulders over its tabard, a pixel off the arm's corner on some frames). A figure pixel with no figure neighbour
// (8-connected) is cleared; a held item's pixels stay
void floatSpeckPass(const DollRecipe& r, FrameOut& o) {
  const std::vector<uint8_t>& heldPx = g_heldPx;
  if (r.has("alloyshape") && o.bx1 >= o.bx0) {
    auto at = [&](int x, int y) { return x >= 0 && y >= 0 && x < CW && y < CH && o.code[(size_t)y * CW + x] != 0; };
    for (int y = o.by0; y <= o.by1; y++)
      for (int x = o.bx0; x <= o.bx1; x++) {
        const size_t q = (size_t)y * CW + x;
        if (!o.code[q] || heldPx[q]) continue;
        bool nb = false;
        for (int dy = -1; dy <= 1 && !nb; dy++)
          for (int dx = -1; dx <= 1 && !nb; dx++) nb = (dx || dy) && at(x + dx, y + dy);
        if (!nb) o.code[q] = 0;
      }
  }
}
// (silhouettes i2, the blind panel: "Mythic: a fire-sparkle soup of bright 1-2 px specks, the face only just readable";
// C13's bright-pixel budget) on the Mythic a glow speck (a blob of 1-2 pixels) goes dark: the fire lives
// in the cracks and the rims, never as speckle
void mythicSpeckPass(const DollRecipe& r, FrameOut& o) {
  const std::vector<uint8_t>& heldPx = g_heldPx;
  if (r.has("mythic") && o.bx1 >= o.bx0) {
    static thread_local std::vector<uint8_t> src;
    src = o.code;
    auto glowAt = [&](int x, int y) {
      if (x < 0 || y < 0 || x >= CW || y >= CH) return false;
      const uint8_t c = src[(size_t)y * CW + x];
      return c >= 2 && codeSlot(c) == Slot::Glow;
    };
    // a glow blob of 1 or 2 pixels (8-connected) is a speck: it goes dark (the cracks and rims are longer)
    static thread_local std::vector<uint8_t> seen;
    seen.assign((size_t)CW * CH, 0);
    std::vector<int> blob, st;
    for (int y = o.by0; y <= o.by1; y++)
      for (int x = o.bx0; x <= o.bx1; x++) {
        if (!glowAt(x, y) || seen[(size_t)y * CW + x]) continue;
        blob.clear(); st.assign(1, y * CW + x);
        seen[(size_t)y * CW + x] = 1;
        while (!st.empty() && blob.size() < 3) {
          const int q = st.back(); st.pop_back();
          blob.push_back(q);
          for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
              const int nx = q % CW + dx, ny = q / CW + dy;
              if ((dx || dy) && glowAt(nx, ny) && !seen[(size_t)ny * CW + nx]) { seen[(size_t)ny * CW + nx] = 1; st.push_back(ny * CW + nx); }
            }
        }
        if (blob.size() + st.size() > 2) continue;
        bool held = false;
        for (int q : blob) held |= heldPx[(size_t)q] != 0;
        if (!held) for (int q : blob) o.code[(size_t)q] = code(Slot::Metal, 1);
      }
    // (envelope r2 integration, C19 A5 "lone glow px") a glow pixel that touches the rest of the fire only at a corner is a
    // speck at 1x too (a diagonal stair of single pixels): the fire runs in 4-connected cracks and rims, the stair goes dark
    // the shield's sunburst rays (diagonal pairs of single glow pixels) thicken into bent 3 px rays instead: the corner
    // pixel between a pair, if it is drawn plate (not outline), takes the fire, so the ray stays and is 4-connected
    {
      src = o.code;
      for (int y = o.by0; y <= o.by1; y++)
        for (int x = o.bx0; x <= o.bx1; x++) {
          const size_t q = (size_t)y * CW + x;
          if (!glowAt(x, y) || g_depthPx[q] != 1) continue;
          auto glowNow = [&](int gx, int gy) { const uint8_t c = o.code[(size_t)gy * CW + gx]; return c >= 2 && codeSlot(c) == Slot::Glow; };
          bool joined = false;
          for (int dy : {1, -1})
            for (int dx : {1, -1}) {
              if (joined || !glowAt(x + dx, y + dy)) continue;
              if (glowNow(x - 1, y) || glowNow(x + 1, y) || glowNow(x, y - 1) || glowNow(x, y + 1)) { joined = true; continue; }
              const int cx[2] = {x, x + dx}, cy[2] = {y + dy, y};   // the two corner pixels between the pair
              for (int k = 0; k < 2 && !joined; k++) {
                const size_t c = (size_t)cy[k] * CW + cx[k];
                const uint8_t cc = o.code[c];
                if (cc >= 2 && codeStep(cc) != 0 && g_depthPx[c] == 1 && codeSlot(cc) != Slot::Glow) { o.code[c] = src[q]; joined = true; }
              }
            }
        }
    }
    {
      src = o.code;
      std::vector<int> stair;
      for (int y = o.by0; y <= o.by1; y++)
        for (int x = o.bx0; x <= o.bx1; x++) {
          const size_t q = (size_t)y * CW + x;
          if (!glowAt(x, y) || g_depthPx[q] == 2) continue;   // (the shield too; a held blade keeps its own fire, C17 blades)
          if (!glowAt(x - 1, y) && !glowAt(x + 1, y) && !glowAt(x, y - 1) && !glowAt(x, y + 1)) stair.push_back((int)q);
        }
      for (int q : stair) o.code[(size_t)q] = code(Slot::Metal, 1);
    }
    // (silhouettes i3, the blind panel: "the crackle and flame finish is 1 px glitter noise all over the body, yellow-orange
    // speckles floating off the crown") a lone metal or glow pixel whose four neighbours all share one other code takes that
    // code (the plates read as planes; the cracks, 2 px and longer, stay), and a figure pixel with no figure neighbour (a
    // floating spark) is cleared
    src = o.code;
    auto codeAt = [&](int x, int y) -> uint8_t { return x < 0 || y < 0 || x >= CW || y >= CH ? 0 : src[(size_t)y * CW + x]; };
    for (int y = o.by0; y <= o.by1; y++)
      for (int x = o.bx0; x <= o.bx1; x++) {
        const size_t q = (size_t)y * CW + x;
        const uint8_t c = src[q];
        if (c < 2 || heldPx[q]) continue;
        const Slot s = codeSlot(c);
        int nb = 0;
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++) nb += (dx || dy) && codeAt(x + dx, y + dy) >= 2;
        if (!nb && (s == Slot::Glow || s == Slot::Metal || s == Slot::Metal2)) { o.code[q] = 0; continue; }
        if (s != Slot::Metal && s != Slot::Metal2 && s != Slot::Glow) continue;
        if (codeStep(c) == 0) continue;
        const uint8_t u = codeAt(x, y - 1), d = codeAt(x, y + 1), l = codeAt(x - 1, y), rr = codeAt(x + 1, y);
        if (u == d && d == l && l == rr && u != c && u >= 2 && codeStep(u) != 0) {
          const Slot us = codeSlot(u);
          if (us == Slot::Metal || us == Slot::Metal2 || us == Slot::Glow) o.code[q] = u;
        }
      }
  }
}
namespace {
constexpr PostPass kPostPasses[] = {despecklePass, glintPass, glintCapPass, glassPass, starBandPass, mythicFirePass, floatSpeckPass, mythicSpeckPass};
}  // namespace
void postPasses(const DollRecipe& r, FrameOut& o) {
  for (PostPass pass : kPostPasses) pass(r, o);
}

// A3's selout over a w x h code buffer: an outline pixel (step 0 of a lit slot) whose outward edge faces the top-left
// takes step 1 (outward normal from the clear neighbours, orthogonal 1.0, diagonal 0.7; lit when its dot with
// (-0.707, -0.707) exceeds 0.35)
void finishPass(uint8_t* code, int w, int h, int stride) {
  static thread_local std::vector<uint8_t> src;
  src.resize((size_t)(w + 2) * (h + 2));
  std::fill(src.begin(), src.end(), (uint8_t)0);
  for (int y = 0; y < h; y++) std::memcpy(&src[(size_t)(y + 1) * (w + 2) + 1], &code[(size_t)y * stride], (size_t)w);
  const int sw = w + 2;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const uint8_t c = src[(size_t)(y + 1) * sw + x + 1];
      if (c < 2 || codeStep(c) != 0 || !slotLit(codeSlot(c))) continue;
      float nx = 0, ny = 0;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          if (!dx && !dy) continue;
          if (src[(size_t)(y + 1 + dy) * sw + x + 1 + dx] < 2) {
            const float k = (dx == 0 || dy == 0) ? 1.0f : 0.7f;
            nx += dx * k; ny += dy * k;
          }
        }
      const float n = std::sqrt(nx * nx + ny * ny);
      if (n < 1e-6f) continue;
      if ((nx * -0.7071f + ny * -0.7071f) / n > 0.35f) code[(size_t)y * stride + x] = (uint8_t)(c + 1);
    }
}

}  // namespace comp
}  // namespace doll
}  // namespace art
