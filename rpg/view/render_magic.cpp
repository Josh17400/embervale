// M9 "Arcana & Mastery" (VISION_PLAN 15.29.7, 15.30): magic on screen and in the ear. SPELLFX lane.
// The View's entry points for the spells; the look itself lives View-free in magic_fx.h / magic_draw.h /
// magic_surface.h (so the gallery, tools/preview/magic_gallery.cpp, renders the same pixels):
//   magicEvent       Ev::Spell -> the World (effects, particles, decals, sounds, kicks, flashes)
//   magicUpdate      staged script events, synthetic missiles' ends, the missiles' trails, the statuses' sheddings,
//                    the World's ageing, its requests (sounds through engine/audio, the screen kick, flash, lift),
//                    the surfaces' atlas following magic::Field
//   drawMagicGround  under the bodies: the surfaces, decals, sigils, rifts, ground rings
//   drawSpellProj    the sorted pass: a missile's shadow and its light on the ground (its lit body is drawn after the
//                    lighting, so it burns through the night)
//   magicLights      the light map: missiles, wind-ups, impacts, beams, burning and shocked ground, burning bodies;
//                    lightning lifts the whole night for a frame
//   drawMagicFx      after the lighting: missiles, every phase, the summoned forms climbing out, particles, the status
//                    overlays, flames / clouds / arcs over the surfaces, the scene flash
// Budget: pooled particles (900 desktop / 560 phone), effects and decals capped, sheets baked once, at most 48 (24 on a
// phone) surface tiles baked a frame; magicFxStats().ms is the magic's own draw time.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <climits>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include "engine/music_style.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/view/magic_state.h"
#include "rpg/view/prop_traits.h"
#include "rpg/view/view.h"

using mfx::withA;

namespace {
using Clock = std::chrono::steady_clock;
double msSince(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }

// the game's painter (magic_fx.h): Pix rects, blends, the sheets baked into textures the first time they are drawn
struct GamePt {
  Pix& P;
  const std::vector<Tex>& monsters;
  void rect(float x, float y, float w, float h, Color c) { if (c.a > 0.003f) P.rect(x, y, w, h, c); }
  void add(float x, float y, float w, float h, Color c) { if (c.a > 0.003f) P.rectAdd(x, y, w, h, c); }
  void blit(mfx::Sheet& s, int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh, Color tint, int blend, bool flip) {
    if (tint.a <= 0.003f) return;
    if (!s.tex.t) {
      if (s.stream) { s.tex = P.makeStream(s.c.w, s.c.h); s.dirty = true; }
      else s.tex = P.bake(s.c);
    }
    if (s.stream && s.dirty) { P.updateStream(s.tex, s.c.px.data()); s.dirty = false; }
    P.blitEx(s.tex, sx, sy, sw, sh, dx, dy, dw, dh, flip, tint, blend);
  }
  void creature(int mon, int frame, float feetX, float feetY, int rows, float sc, Color tint, int blend, bool flip) {
    if (mon < 0 || mon >= (int)monsters.size() || tint.a <= 0.003f) return;
    const art::Monster m = (art::Monster)mon;
    const int cw = art::monsterCellW(m), ch = art::monsterCellH(m);
    const int r = rows < 0 ? ch : std::min(rows, ch);
    if (r <= 0) return;
    const float x = std::floor(feetX - (float)cw * sc * 0.5f), y = std::floor(feetY - (float)ch * sc);
    P.blitEx(monsters[(size_t)mon], (frame & 7) * cw, 0, cw, r, x, y, (float)cw * sc, (float)r * sc, flip, tint, blend);
  }
};

// (M9 fixer) which surface tiles stand behind a tall prop (a tree, a stall, a standing stone): a non-flat prop at or
// below the tile's row whose sprite covers the tile's flames (the tile and the 12 px they rise). Those tiles draw their
// flames, arcs and clouds in the ground pass, under the y-sorted scene, so the tree in front hides them; the rest draw
// over the bodies as before.
std::vector<uint8_t> surfaceBehindProps(const Game& g) {
  const magic::Field& F = g.arcana.live.field;
  std::vector<uint8_t> hide(F.tiles.size(), 0);
  const Map& m = g.map();
  if (m.prop.empty()) return hide;
  for (size_t i = 0; i < F.tiles.size(); i++) {
    const int tx = F.tiles[i].tx, ty = F.tiles[i].ty;
    const float fx0 = (float)tx * 16, fx1 = fx0 + 16, fy0 = (float)ty * 16 - 12, fy1 = (float)ty * 16 + 16;
    bool behind = false;
    for (int py = ty; py <= ty + 6 && !behind; py++)
      for (int px = tx - 3; px <= tx + 3 && !behind; px++) {
        if (px < 0 || py < 0 || px >= m.w || py >= m.h || (px == tx && py == ty)) continue;
        const int pr = m.prop[(size_t)py * m.w + px];
        if (!pr) continue;
        const art::Prop p = (art::Prop)(pr - 1);
        if (flatProp(p)) continue;
        const int w = art::propW(p), h = art::propH(p);
        if (h < 20) continue;   // low cover (tufts, flowers) is not a wall to hide fire behind
        const float x0 = (float)px * 16 + 8 - (float)w * 0.5f + 2, x1 = x0 + (float)w - 4;
        const float y1 = (float)py * 16 + 16, y0 = y1 - (float)h + 2;
        if (x0 < fx1 && x1 > fx0 && y0 < fy1 && y1 > fy0 && (py > ty || y1 >= fy1)) behind = true;
      }
    hide[i] = behind ? 1 : 0;
  }
  return hide;
}

const Actor* actorById(const Game& g, uint32_t id) {
  if (!id) return nullptr;
  for (const Actor& a : g.actors) if ((uint32_t)a.id == id) return &a;
  return nullptr;
}
mfx::Look lookFor(uint32_t spell) { return mfx::lookOf(mfx::defOf(spell)); }
bool isMagicMissile(const Projectile& pr) {
  return pr.kind == ProjKind::Spell || (pr.spell && (pr.kind == ProjKind::Fireball || pr.kind == ProjKind::IceSpike));
}
// the night's weight for what the magic draws after the lighting (as the beasts' glows: render.cpp drawWeather)
void nightOf(const Game& g, float& dark, float& lightK) {
  const bool dungeon = g.inside && g.subSite >= 0;
  dark = dungeon ? 0.7f : g.inside ? clampf(1.0f - g.daylight(), 0, 1) * 0.6f : clampf(1.0f - g.daylight() * 1.4f, 0, 1);
  lightK = 1.0f - 0.72f * dark;
}
// a body's size for its overlays
void bodyOf(const Actor& a, float& bw, float& bh) {
  if (a.human || a.player || a.npc || a.comp) { bw = 12; bh = 22; return; }
  const float sc = a.scalePct ? (float)a.scalePct / 100.0f : 1.0f;
  bw = std::max(10.0f, (float)art::monsterCellW(a.mon) * 0.62f * sc);
  bh = std::max(10.0f, (float)art::monsterCellH(a.mon) * 0.78f * sc);
}
Vec2 feetOf(const Actor& a) { return a.p + Vec2(0, a.fly ? -20.0f : 0.0f); }

// ==================================================================== (M10 ARCANA lane) the hidden kingdoms' magic
// Each signature reads by its silhouette and colour on the phone: glowing rune circles on the ground (linked when two
// lie close), a spirit mask on the hero's face (fox, owl, stag) with eyes that glow after dark, thorn brambles that
// spring up from the soil and wither, a braided vine bridge laid tile by tile over the water, roots coiling up a snared
// foe, a rippling violet time bubble, the hero's decoy shimmering, song notes rising (the motif's glyph over the head),
// the priests' fire sung into being at dusk, the trials' gates and plates. Sprites are baked once (W.sheet) as pixel
// art lit from the top left with dark outlines; everything eases in and out, nothing pops.
using HFx = View::MagicFxState::HFx;
enum : uint8_t { HX_NOTE = 1, HX_REFRAIN, HX_SONGFIRE, HX_MASKSTRIKE, HX_MASKDON, HX_GATE, HX_COMBO, HX_DECOY, HX_RITE, HX_ALTAR, HX_SNARE };
constexpr uint64_t SK_HIDDEN = 12;   // (the sheets' key kind for this lane's baked sprites)
inline uint32_t px4(int r, int g, int b, int a = 255) { return rgba(r, g, b, a); }
float easeOutBack(float t) { t = mfx::sat(t); const float c1 = 1.70158f, c3 = c1 + 1; return 1 + c3 * (t - 1) * (t - 1) * (t - 1) + c1 * (t - 1) * (t - 1); }
float easeOut(float t) { t = mfx::sat(t); return 1 - (1 - t) * (1 - t); }
Color mulK(Color c, float k) { return Color(c.r * k, c.g * k, c.b * k, c.a); }

// ---- the baked sprites
// a sprite from rows of palette characters ('.' clear); the palette maps a character to an rgba
Canvas fromRows(const char* const* rows, int h, const char* keys, const uint32_t* cols) {
  int w = 0;
  for (int y = 0; y < h; y++) w = std::max(w, (int)std::strlen(rows[y]));
  Canvas c(w, h);
  for (int y = 0; y < h; y++)
    for (int x = 0; rows[y][x]; x++) {
      const char* k = std::strchr(keys, rows[y][x]);
      if (rows[y][x] != '.' && k) c.set(x, y, cols[k - keys]);
    }
  return c;
}
// the three spirit masks, each seen from the front, the side (facing right) and the back: one sheet, 3 x 3 cells of
// 10 x 10 (row: mask, column: view)
Canvas bakeMasks() {
  Canvas out(30, 30);
  static const char* foxF[] = {"K.......K", "KOK...KOK", "KLOKKKOOK", "KLLOOOOOK", "KEEOGOEEK", "KWOOOOOWK", ".KWWRWWK.", "..KWWWK..", "...KKK..."};
  static const char* foxS[] = {"...K.....", "..KOK....", "..KLOKKK.", ".KLLOOOOK", ".KOOOEOOK", ".KWOOOORK", "..KWWWKK.", "...KKK..."};
  static const char* foxB[] = {"K.......K", "KOK...KOK", "KOOK.KOOK", ".KK...KK.", "..SSSSS.."};
  static const char* owlF[] = {"K.K...K.K", "KLKKKKKLK", "KLLLOOOOK", "KCCLOOCCK", "CEECOCEEC", "KCCOYOCCK", ".KOOYOOK.", "..KOOOK..", "...KKK..."};
  static const char* owlS[] = {"...K.K...", "..KLKK...", "..KLLOOK.", ".KCCLOOK.", ".CEECOOK.", ".KCCOOYK.", "..KOOOYK.", "...KOOK..", "....KK..."};
  static const char* owlB[] = {"K.K...K.K", "KOKKKKKOK", ".KOOOOOK.", "..SSSSS.."};
  static const char* stgF[] = {"A.A...A.A", "BA.A.A.AB", ".BAA.AAB.", "..BKKKB..", ".KLVVVVK.", "KVEVVVEVK", "KLVWVWVVK", ".KVVVVVK.", "..KVVVK..", "...KKK..."};
  static const char* stgS[] = {"...A.A...", "...BAA.A.", "....BAAB.", "...KKKB..", "..KLVVVK.", ".KVVVEVVK", ".KLVVVVWK", "..KVVVVK.", "...KVVK..", "....KK..."};
  static const char* stgB[] = {"A.A...A.A", "BA.A.A.AB", ".BAA.AAB.", "..BKKKB..", "...SSS..."};
  const uint32_t fox[] = {px4(70, 26, 12), px4(232, 112, 36), px4(255, 182, 92), px4(250, 244, 232), px4(26, 10, 8), px4(196, 40, 32), px4(255, 214, 120), px4(60, 40, 30)};
  const uint32_t owl[] = {px4(30, 44, 74), px4(182, 206, 228), px4(238, 247, 255), px4(84, 196, 242), px4(14, 18, 32), px4(252, 204, 76), px4(255, 255, 255), px4(50, 44, 40)};
  const uint32_t stg[] = {px4(36, 20, 66), px4(112, 82, 204), px4(174, 154, 252), px4(234, 222, 192), px4(160, 140, 112), px4(12, 8, 26), px4(232, 242, 255), px4(50, 40, 40)};
  struct M { const char* const* r[3]; int h[3]; const char* keys; const uint32_t* cols; };
  const M ms[3] = {{{foxF, foxS, foxB}, {9, 8, 5}, "KOLWERGS", fox}, {{owlF, owlS, owlB}, {9, 9, 4}, "KOLCEYWS", owl}, {{stgF, stgS, stgB}, {10, 10, 5}, "KVLABEWS", stg}};
  for (int m = 0; m < 3; m++)
    for (int v = 0; v < 3; v++) {
      const Canvas c = fromRows(ms[m].r[v], ms[m].h[v], ms[m].keys, ms[m].cols);
      for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) if (c.get(x, y)) out.set(v * 10 + x, m * 10 + y, c.get(x, y));
    }
  return out;
}
// a thorn bush (18 x 24, 4 variants side by side): a rounded mass of dark leaves lit from the top left, thorny canes
// arching out of it with pale hooked thorns, a berry or a white flower, a dark outline round it all
Canvas bakeThorns() {
  const int W = 18, H = 24;
  Canvas out(W * 4, H);
  for (int v = 0; v < 4; v++) {
    Canvas c(W, H);
    uint32_t h = hash32(0x7408Eu + (uint32_t)v * 977u);
    // the canes first (behind the leaves): reddish-brown, arching up and out, drooping at their tips, thorned
    for (int i = 0; i < 4; i++) {
      h = hash32(h + 17u);
      const float x0 = 5.0f + (float)i * 2.6f;
      const float dir = i < 2 ? -1.0f : 1.0f;
      const float A = 3.0f + (float)((h >> 5) % 3u);
      const float Ht = 15.0f + (float)((h >> 8) % 6u);
      int px = -99, py = -99;
      for (int st = 0; st <= 40; st++) {
        const float t = (float)st / 40.0f;
        const float x = x0 + dir * A * std::pow(t, 1.6f);
        float y = 22.0f - Ht * (1.0f - (1.0f - t) * (1.0f - t));
        if (t > 0.8f) y += (t - 0.8f) * 12.0f;
        const int ix = (int)std::lround(x), iy = (int)std::lround(y);
        if (ix == px && iy == py) continue;
        px = ix; py = iy;
        c.set(ix, iy, st % 8 == 0 ? px4(176, 104, 84) : px4(128, 76, 56));
        if (st % 6 == 2 && t > 0.3f) {   // thorns, hooked, pale
          const int sx = (st / 6) % 2 ? ix - 1 : ix + 1;
          c.set(sx, iy, px4(244, 226, 200));
          c.set(sx + (sx < ix ? -1 : 1), iy - 1, px4(206, 180, 150));
        }
      }
    }
    // the bush: overlapping lobes of leaf, each lit on its top left (the nearest lobe's centre decides a pixel's light)
    struct Lobe { float x, y, r; };
    Lobe lobes[6];
    const int nl = 4 + (int)(v % 2);
    for (int i = 0; i < nl; i++) {
      h = hash32(h + 101u);
      lobes[i] = {4.0f + (float)i * (10.0f / (float)(nl - 1)) + (float)((int)(h % 3u) - 1), 15.0f + (float)((h >> 3) % 5u) - (i == 1 || i == nl - 2 ? 3.0f : 0.0f),
                  3.6f + (float)((h >> 7) % 3u) * 0.7f};
    }
    for (int y = 0; y < H - 1; y++)
      for (int x = 0; x < W; x++) {
        int best = -1;
        float bd = 1e9f;
        for (int i = 0; i < nl; i++) {
          const float dx = (float)x - lobes[i].x, dy = ((float)y - lobes[i].y) * 1.15f;
          const float d = std::sqrt(dx * dx + dy * dy) / lobes[i].r;
          if (d < 1.0f && d < bd) { bd = d; best = i; }
        }
        if (best < 0) continue;
        const float dx = ((float)x - lobes[best].x) / lobes[best].r, dy = ((float)y - lobes[best].y) / lobes[best].r;
        const float lit = -(dx + dy) * 0.7f;   // toward the top left
        const uint32_t n = hash2(x, y, 0xB05Bu + (uint32_t)v);
        uint32_t col;
        if (lit > 0.45f) col = (n & 3) ? px4(122, 182, 80) : px4(160, 206, 104);
        else if (lit > 0.0f) col = (n & 3) ? px4(78, 138, 58) : px4(104, 160, 70);
        else if (lit > -0.45f) col = (n & 3) ? px4(50, 100, 44) : px4(66, 120, 52);
        else col = (n & 3) ? px4(32, 70, 34) : px4(44, 86, 40);
        if (bd > 0.82f && lit < 0.2f) col = px4(28, 58, 30);   // the lobe's shaded rim
        c.set(x, y, col);
      }
    // a berry or a flower on the bush
    h = hash32(h + 7u);
    const int bx = 4 + (int)(h % 10u), by = 12 + (int)((h >> 4) % 4u);
    if (v % 2) { c.set(bx, by, px4(160, 120, 180)); c.set(bx + 1, by, px4(44, 14, 48)); c.set(bx, by + 1, px4(52, 18, 56)); c.set(bx + 1, by + 1, px4(36, 10, 40)); }
    else { c.set(bx, by, px4(250, 246, 236)); c.set(bx + 1, by, px4(236, 220, 210)); c.set(bx, by + 1, px4(236, 220, 210)); c.set(bx + 1, by + 1, px4(250, 210, 90)); }
    // the outline
    Canvas o = c;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        if (c.get(x, y)) continue;
        if (c.get(x - 1, y) || c.get(x + 1, y) || c.get(x, y - 1) || c.get(x, y + 1)) o.set(x, y, px4(20, 24, 14));
      }
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (o.get(x, y)) out.set(v * W + x, y, o.get(x, y));
  }
  return out;
}
// a vine bridge's deck tile (16 x 16; horizontal in row 0, vertical in row 1; 2 variants): braided strands over a
// mat of stems, leaves and a few pale flowers at its edges
Canvas bakeVines() {
  Canvas out(32, 32);
  for (int v = 0; v < 2; v++) {
    Canvas c(16, 16);
    for (int x = 0; x < 16; x++) for (int y = 4; y <= 11; y++) c.set(x, y, y == 4 || y == 11 ? px4(36, 44, 18) : px4(66, 74, 30));   // the mat
    for (int s = 0; s < 3; s++)
      for (int x = 0; x < 16; x++) {
        const float ph = (float)x * 0.78f + (float)s * 2.1f + (float)v * 1.3f;
        const int y = 7 + (int)std::lround(std::sin(ph) * 2.4f) + (s == 1 ? 1 : 0);
        c.set(x, y, px4(150, 168, 72)); c.set(x, y + 1, px4(98, 118, 44));
        if (std::cos(ph) > 0.7f) c.set(x, y - 1, px4(196, 210, 120));   // the strand's lit crest
      }
    for (int x = 1; x < 16; x += 4) {   // leaves out at the edges
      const int k = (x / 4 + v) % 3;
      const int y = k == 0 ? 2 : k == 1 ? 12 : 3;
      c.set(x, y, px4(108, 178, 74)); c.set(x + 1, y, px4(58, 122, 48)); c.set(x, y + 1, px4(58, 122, 48)); c.set(x + 1, y + 1, px4(32, 76, 34));
    }
    c.set(5 + v * 6, 3, px4(250, 236, 244)); c.set(6 + v * 6, 3, px4(236, 170, 200));   // a flower
    c.set(11 - v * 7, 12, px4(250, 236, 244));
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++) {
        out.set(v * 16 + x, y, c.get(x, y));
        out.set(v * 16 + y, 16 + x, c.get(x, y));   // (transposed: the vertical deck)
      }
  }
  return out;
}
// roots coiling up (16 x 12, 2 variants)
Canvas bakeRoots() {
  Canvas out(32, 12);
  for (int v = 0; v < 2; v++) {
    Canvas c(16, 12);
    for (int r = 0; r < 4; r++) {
      const float x0 = 2.0f + (float)r * 4.0f + (float)v;
      const float lean = r < 2 ? 1.0f : -1.0f;
      const int ht = 7 + (r * 3 + v * 2) % 5;
      for (int k = 0; k <= ht; k++) {
        const float t = (float)k / (float)ht;
        const int x = (int)std::lround(x0 + lean * t * t * 4.0f), y = 11 - k;
        c.set(x, y, px4(150, 112, 66));
        if (t < 0.6f) c.set(x + 1, y, px4(96, 66, 36));
        if (k == ht) c.set(x + (lean > 0 ? 1 : -1), y + 1, px4(120, 88, 50));
      }
    }
    Canvas o = c;
    for (int y = 0; y < 12; y++)
      for (int x = 0; x < 16; x++)
        if (!c.get(x, y) && (c.get(x - 1, y) || c.get(x + 1, y) || c.get(x, y - 1))) o.set(x, y, px4(36, 22, 12));
    for (int y = 0; y < 12; y++) for (int x = 0; x < 16; x++) if (o.get(x, y)) out.set(v * 16 + x, y, o.get(x, y));
  }
  return out;
}
mfx::Sheet& sheetOf(mfx::World& W, int which) {
  switch (which) {
    case 0: return W.sheet(mfx::sheetKey((mfx::SheetKind)SK_HIDDEN, 1), [] { return bakeMasks(); });
    case 1: return W.sheet(mfx::sheetKey((mfx::SheetKind)SK_HIDDEN, 2), [] { return bakeThorns(); });
    case 2: return W.sheet(mfx::sheetKey((mfx::SheetKind)SK_HIDDEN, 3), [] { return bakeVines(); });
    default: return W.sheet(mfx::sheetKey((mfx::SheetKind)SK_HIDDEN, 4), [] { return bakeRoots(); });
  }
}
// the masks' identity colours (dark, body, glow)
Color maskCol(int m, int i) {
  static const uint32_t c[3][3] = {{px4(140, 40, 16), px4(240, 120, 40), px4(255, 210, 120)},
                                   {px4(40, 90, 150), px4(150, 214, 245), px4(230, 250, 255)},
                                   {px4(70, 40, 160), px4(150, 120, 255), px4(230, 220, 255)}};
  return mfx::cu(c[std::clamp(m, 0, 2)][std::clamp(i, 0, 2)]);
}
// a key's first-seen clock (the view eases things in from it)
float seenAt(View::MagicFxState& S, uint64_t key) {
  for (auto& k : S.seenAt) if (k.first == key) return k.second;
  if (S.seenAt.size() > 256) S.seenAt.erase(S.seenAt.begin());
  S.seenAt.push_back({key, S.clock});
  return S.clock;
}
float semis(uint8_t scale, int degree) {   // a scale degree's semitones above the tonic (the scale's own notes)
  static const int8_t pcs[12][7] = {{0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10},
                                    {0, 2, 4, 5, 7, 9, 10}, {0, 2, 4, 6, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 11}, {0, 1, 4, 5, 7, 8, 10},
                                    {0, 2, 4, 7, 9, 12, 14}, {0, 3, 5, 7, 10, 12, 15}, {0, 2, 3, 7, 8, 12, 14}, {0, 1, 5, 7, 10, 12, 13}};
  return (float)pcs[scale % 12][std::clamp(degree, 0, 6)];
}
void hidSound(View::MagicFxState& S, float delay, uint8_t motif, uint8_t timbre, int root, float vol) {
  if (S.hsounds.size() > 24) return;
  View::MagicFxState::HSound h;
  h.at = S.clock + delay; h.motif = motif; h.timbre = timbre; h.root = root; h.vol = vol;
  S.hsounds.push_back(h);
}
HFx* hidFx(View::MagicFxState& S, uint8_t kind, Vec2 p, Vec2 q, float life, float f, uint32_t id, const mfx::Look& L) {
  if (S.hfx.size() >= 48) S.hfx.erase(S.hfx.begin());
  HFx h;
  h.kind = kind; h.p = p; h.q = q; h.life = life; h.f = f; h.id = id; h.L = L; h.seed = S.w.rnd.u();
  S.hfx.push_back(h);
  return &S.hfx.back();
}
const Actor* heroOf(const Game& g) { return g.actors.empty() ? nullptr : &g.actors[0]; }
const magic::Tradition* attunedSig(const Game& g, magic::Signature s) {
  for (uint32_t tid : g.arcana.book.attuned) if (const magic::Tradition* t = magic::traditionById(tid)) if (t->sig == s) return t;
  return nullptr;
}
}  // namespace

// ---- (M10) the hidden signatures' events: true when this lane handled it (the default pipeline skips it)
static bool hidEvent(View& v, Game& g, const Event& e) {
  View::MagicFxState& S = MagicFxOps::st(v);
  mfx::World& W = S.w;
  const magic::Phase ph = (magic::Phase)std::clamp(e.a, 0, (int)magic::Phase::COUNT - 1);
  const std::string& s = e.s;
  if (s.empty()) return false;
  const mfx::Look L = lookFor(e.spell);
  const Actor* hero = heroOf(g);
  const float hearK = hero ? mfx::sat(1.15f - len(e.p - hero->p) / 360.0f) : 1.0f;
  const MusicStyle ms = MusicStyle::unpack(MagicFxOps::musicStyle(v));
  const uint8_t scale = (uint8_t)ms.scale;
  auto noteRoot = [&](float deg) { return 64 + (int)semis(scale, (int)deg); };
  if (ph == magic::Phase::Reaction && s == "NOTE") {
    hidFx(S, HX_NOTE, e.p, e.q, 1.1f, e.f, e.id, L);
    for (int i = 0; i < 4; i++)
      W.spawn(mfx::PK_NOTE, e.p + Vec2(W.rnd.r(-5, 5), W.rnd.r(-3, 2)), Vec2(W.rnd.r(-10, 10), W.rnd.r(-38, -24)), W.rnd.r(0.9f, 1.4f), L.ramp[1], L.ramp[2], L.ramp[4], 1, -6, mfx::PF_ADD);
    hidSound(S, 0, 3, 6, noteRoot(e.f), 0.55f * hearK);   // a plucked note of the culture's scale, a grace note round it
    return true;
  }
  if (ph == magic::Phase::Reaction && s == "REFRAIN") {
    hidFx(S, HX_REFRAIN, e.p, e.q, 1.6f, e.f, e.id, L);
    for (int i = 0; i < 16; i++) {
      const float a = mfx::kTau * (float)i / 16.0f;
      W.spawn(mfx::PK_NOTE, e.p + Vec2(std::cos(a) * 8, std::sin(a) * 4), Vec2(std::cos(a) * 34, std::sin(a) * 16 - 30), W.rnd.r(1.1f, 1.6f), L.ramp[1], L.ramp[3], L.ramp[4], 1, -8, mfx::PF_ADD);
    }
    W.flash(L.c(3), 0.18f);
    W.kick(2.5f);
    // the motif sung again, its three notes one after another, then the choir's chord under them
    if (const magic::SpellDef* d = magic::spellById(e.spell)) {
      const magic::Tradition& t = magic::traditionOfSpell(*d);
      for (int i = 0; i < 3; i++) hidSound(S, 0.02f + 0.17f * (float)i, 3, 6, noteRoot(t.motif[i]), 0.6f * hearK);
    }
    hidSound(S, 0.55f, 0, 5, noteRoot(0), 0.75f * hearK);
    W.sound(Sfx::HolyBloom, 1.1f, 0.55f * hearK);
    return true;
  }
  if (ph == magic::Phase::Reaction && s == "SONGFIRE") {
    hidFx(S, HX_SONGFIRE, e.p, e.q, 2.6f, e.f, e.id, L);
    if (hearK > 0.02f) hidSound(S, 0, 2, 5, noteRoot(e.f) - 12, 0.38f * hearK);   // a low sung phrase
    return true;
  }
  if (ph == magic::Phase::Reaction && (s == "DECOY" || s == "RITE")) {
    hidFx(S, s == "DECOY" ? HX_DECOY : HX_RITE, e.p, e.q, s == "DECOY" ? 0.7f : 2.4f, e.f, e.id, L);
    if (s == "DECOY") { W.sound(Sfx::Shimmer, 1.2f, 0.6f); for (int i = 0; i < 10; i++) W.spawn(mfx::PK_PETAL, e.p + Vec2(W.rnd.r(-8, 8), W.rnd.r(-20, 0)), Vec2(W.rnd.r(-16, 16), W.rnd.r(-20, 4)), W.rnd.r(0.5f, 0.9f), L.ramp[1], L.ramp[2], L.ramp[4], 1, 0, mfx::PF_ADD); }
    else { W.sound(Sfx::HolyBloom, 0.8f, 0.9f); hidSound(S, 0.1f, 0, 5, 60, 0.7f); W.flash(Color(1, 0.9f, 0.7f), 0.3f); W.kick(3); }
    return true;
  }
  if (ph == magic::Phase::Ward && (s == "MASK" || s == "MASK SWAP")) {
    hidFx(S, HX_MASKDON, e.p, e.q, 0.6f, 0, e.id, L);
    for (int i = 0; i < 10; i++) W.spawn(mfx::PK_SMOKE, e.p + Vec2(W.rnd.r(-6, 6), W.rnd.r(-4, 4)), Vec2(W.rnd.r(-18, 18), W.rnd.r(-22, -6)), W.rnd.r(0.4f, 0.7f), px4(60, 50, 70), px4(150, 140, 170), px4(230, 225, 240), 1, 0, 0);
    W.sound(Sfx::WardRing, 1.35f, 0.6f);
    S.maskPop = S.clock;
    return true;
  }
  if (ph == magic::Phase::Release && s == "MASK") {
    for (int i = 0; i < 8; i++) W.spawn(mfx::PK_SMOKE, e.p + Vec2(W.rnd.r(-5, 5), W.rnd.r(-3, 3)), Vec2(W.rnd.r(-12, 12), W.rnd.r(-16, -4)), W.rnd.r(0.4f, 0.6f), px4(60, 50, 70), px4(150, 140, 170), px4(230, 225, 240), 1, 0, 0);
    return true;
  }
  if (ph == magic::Phase::Ward && s == "BUBBLE") {
    W.sound(Sfx::DreamPulse, 0.7f, 0.8f);
    W.sound(Sfx::Shimmer, 0.6f, 0.5f);
    return true;
  }
  if (ph == magic::Phase::Release && s == "BUBBLE") { W.sound(Sfx::Shimmer, 1.4f, 0.4f); return true; }
  if (ph == magic::Phase::Impact && s == "MASKSTRIKE") {
    const int m = std::clamp((int)e.f, 0, 2);
    hidFx(S, HX_MASKSTRIKE, e.p, e.q, 0.42f, (float)m, e.id, L);
    const Color c0 = maskCol(m, 0), c1 = maskCol(m, 1), c2 = maskCol(m, 2);
    const mfx::PartKind pk = m == 0 ? mfx::PK_EMBER : m == 1 ? mfx::PK_SHARD : mfx::PK_SPARK;
    const Vec2 d = norm(e.q - e.p + Vec2(0.001f, 0));
    for (int i = 0; i < 14; i++)
      W.spawn(pk, e.p + Vec2(W.rnd.r(-6, 6), W.rnd.r(-6, 6)), d * W.rnd.r(40, 110) + Vec2(W.rnd.r(-30, 30), W.rnd.r(-40, 10)), W.rnd.r(0.3f, 0.6f), mfx::pk(c0), mfx::pk(c1), mfx::pk(c2), 1, 30, mfx::PF_ADD);
    W.sound(m == 0 ? Sfx::FireBurst : m == 1 ? Sfx::FrostShatter : Sfx::Zap, 1.25f, 0.7f);
    W.kick(2.0f);
    return true;
  }
  if (ph == magic::Phase::Impact && s == "VINES") {
    for (int i = 0; i < 16; i++) W.spawn(mfx::PK_LEAF, e.p + Vec2(W.rnd.r(-10, 10), W.rnd.r(-6, 6)), Vec2(W.rnd.r(-30, 30), W.rnd.r(-40, -10)), W.rnd.r(0.6f, 1.1f), px4(30, 70, 30), px4(80, 150, 60), px4(170, 220, 110), 1, 40, 0);
    W.sound(Sfx::HolyCast, 0.75f, 0.5f);
    W.sound(Sfx::StoneGrind, 1.5f, 0.35f);
    return true;
  }
  if (ph == magic::Phase::Impact && s == "ALTAR") {
    hidFx(S, HX_ALTAR, e.p, e.q, 1.2f, 0, e.id, L);
    for (int i = 0; i < 14; i++) W.spawn(mfx::PK_DROP, e.p + Vec2(W.rnd.r(-6, 6), W.rnd.r(-4, 2)), Vec2(W.rnd.r(-14, 14), W.rnd.r(-46, -20)), W.rnd.r(0.5f, 0.9f), px4(60, 0, 8), px4(170, 20, 34), px4(255, 120, 120), 1, 120, 0);
    W.sound(Sfx::FireCast, 0.55f, 0.6f);
    return true;
  }
  if (ph == magic::Phase::StatusOn && s == "SNARED") {
    hidFx(S, HX_SNARE, e.p, e.q, 0.5f, 0, e.id, L);
    for (int i = 0; i < 8; i++) W.spawn(mfx::PK_ROCK, e.p + Vec2(W.rnd.r(-8, 8), W.rnd.r(-2, 3)), Vec2(W.rnd.r(-20, 20), W.rnd.r(-40, -14)), W.rnd.r(0.3f, 0.6f), px4(50, 36, 22), px4(110, 84, 54), px4(160, 130, 90), 1, 160, 0);
    W.sound(Sfx::StoneGrind, 1.7f, 0.45f);
    return true;
  }
  if (ph == magic::Phase::Combo && s == "GATE") {
    hidFx(S, HX_GATE, e.p, e.q, 1.2f, 0, e.id, L);
    for (int i = 0; i < 24; i++) W.spawn(mfx::PK_GLYPH, e.p + Vec2(W.rnd.r(-28, 28), W.rnd.r(-20, 10)), Vec2(W.rnd.r(-14, 14), W.rnd.r(-46, -16)), W.rnd.r(0.8f, 1.4f), L.ramp[1], L.ramp[2], L.ramp[4], 1, -4, mfx::PF_ADD);
    W.sound(Sfx::WardRing, 0.7f, 0.8f);
    W.sound(Sfx::StoneGrind, 0.6f, 0.6f);
    W.kick(2.0f);
    return true;
  }
  if (ph == magic::Phase::Combo && (s == "STEAM BURST" || s == "THUNDERFIRE" || s == "SHATTERSTORM" || s == "BALEFIRE" || s == "BLACK FROST" || s == "ACID STORM" ||
                                    s == "RUNEQUAKE" || s == "GREAT GLYPH" || s == "DOUBLE GLYPH")) {
    HFx* h = hidFx(S, HX_COMBO, e.p, e.q, 0.8f, e.f, e.id, L);
    if (h) h->seed = (uint32_t)std::hash<std::string>{}(s);
    for (int i = 0; i < 20; i++) {
      const float a = mfx::kTau * (float)i / 20.0f;
      W.spawn(i % 2 ? mfx::PK_GLYPH : mfx::PK_SPARK, e.p + Vec2(std::cos(a) * 6, std::sin(a) * 3), Vec2(std::cos(a) * 70, std::sin(a) * 34 - 20), W.rnd.r(0.5f, 0.9f), L.ramp[1], L.ramp[2], L.ramp[4], 1, 20, mfx::PF_ADD);
    }
    W.sound(Sfx::Sizzle, 0.9f, 0.8f);
    W.sound(Sfx::FireBurst, 1.2f, 0.6f);
    W.flash(L.c(3), 0.12f);
    W.kick(3.0f);
    MagicOps::ev(g, Ev::Text, e.p + Vec2(0, -26), (int)rgba(255, 236, 180), 0, s);
    return true;
  }
  if (ph == magic::Phase::Aftermath && s == "RUNE") {   // a Runecarving rune is carved, not blasted: glyph dust, a grind
    const magic::SpellDef* d = magic::spellById(e.spell);
    if (!d || !d->tradition || magic::traditionOfSpell(*d).sig != magic::Signature::Rune) return false;
    for (int i = 0; i < 10; i++) W.spawn(mfx::PK_GLYPH, e.p + Vec2(W.rnd.r(-12, 12), W.rnd.r(-5, 5)), Vec2(W.rnd.r(-8, 8), W.rnd.r(-26, -10)), W.rnd.r(0.5f, 0.9f), L.ramp[1], L.ramp[3], L.ramp[4], 1, 0, mfx::PF_ADD);
    W.sound(Sfx::StoneGrind, 1.3f, 0.5f);
    W.sound(Sfx::WardRing, 1.6f, 0.3f);
    return true;
  }
  if (ph == magic::Phase::Fizzle && s == "DISCORD") { W.sound(Sfx::SpellFizzle, 0.7f, 0.7f); return false; }
  return false;
}

namespace {
// ---- the hidden signatures' ageing and requests (magicUpdate)
void hidUpdate(View& v, Game& g, float dt) {
  View::MagicFxState& S = MagicFxOps::st(v);
  mfx::World& W = S.w;
  for (size_t i = 0; i < S.hfx.size();) {
    HFx& h = S.hfx[i];
    h.t += dt;
    if (h.kind == HX_SONGFIRE) {   // embers drawn in to the flame, then the flame breathes out notes as it rises
      const float u = h.t / h.life;
      if (u < 0.35f && W.rnd.f() < dt * 40.0f) {
        const float a = W.rnd.r(0, mfx::kTau), r = W.rnd.r(16, 26);
        const Vec2 at = h.q + Vec2(std::cos(a) * r, std::sin(a) * r * 0.6f);
        W.spawn(mfx::PK_EMBER, at, (h.q - at) * 2.6f, 0.38f, h.L.ramp[1], h.L.ramp[2], h.L.ramp[4], 1, 0, mfx::PF_ADD);
      } else if (u >= 0.35f && u < 0.9f && W.rnd.f() < dt * 9.0f) {
        W.spawn(mfx::PK_NOTE, h.q + Vec2(W.rnd.r(-4, 4), -6), Vec2(W.rnd.r(-10, 10), W.rnd.r(-30, -18)), W.rnd.r(0.9f, 1.3f), h.L.ramp[1], h.L.ramp[3], h.L.ramp[4], 1, -4, mfx::PF_ADD);
      }
    }
    if (h.t >= h.life) { S.hfx.erase(S.hfx.begin() + (std::ptrdiff_t)i); continue; }
    i++;
  }
  // the notes due now
  for (size_t i = 0; i < S.hsounds.size();) {
    if (S.hsounds[i].at > S.clock) { i++; continue; }
    const View::MagicFxState::HSound h = S.hsounds[i];
    S.hsounds.erase(S.hsounds.begin() + (std::ptrdiff_t)i);
    if (h.vol <= 0.01f) continue;
    const MusicStyle ms = MusicStyle::unpack(MagicFxOps::musicStyle(v));
    MagicFxOps::audio(v).playMotif(h.motif, (uint8_t)ms.scale, h.timbre, h.root, h.vol);
  }
  // withering thorns and vines shed leaves; a worn mask breathes spirit wisps
  const magic::Live& L = g.arcana.live;
  for (const magic::Growth& w : L.growths) {
    if (w.life > 1.6f || W.rnd.f() > dt * 6.0f) continue;
    const Vec2 c((float)w.tx * TILE + 8, (float)w.ty * TILE + (w.kind == magic::GROW_THORN ? 4.0f : 8.0f));
    W.spawn(mfx::PK_LEAF, c + Vec2(W.rnd.r(-6, 6), W.rnd.r(-6, 2)), Vec2(W.rnd.r(-12, 12), W.rnd.r(-10, 4)), W.rnd.r(0.8f, 1.3f), px4(60, 44, 20), px4(130, 104, 50), px4(190, 160, 90), 1, 26, 0);
  }
  const Actor* hero = heroOf(g);
  if (hero && L.mask >= 0 && W.rnd.f() < dt * 5.0f) {
    const Color c1 = maskCol(L.mask, 1), c2 = maskCol(L.mask, 2);
    W.spawn(mfx::PK_MOTE, hero->p + Vec2(W.rnd.r(-5, 5), -18 + W.rnd.r(-2, 2)), Vec2(W.rnd.r(-6, 6), W.rnd.r(-16, -8)), W.rnd.r(0.6f, 1.0f), mfx::pk(maskCol(L.mask, 0)), mfx::pk(c1), mfx::pk(c2), 1, -2, mfx::PF_ADD);
  }
  if (S.maskWorn != L.mask) { S.maskWorn = L.mask; S.maskPop = S.clock; }
  // bubbles: slow motes drifting round inside
  for (const magic::Bubble& b : L.bubbles)
    if (W.rnd.f() < dt * 10.0f) {
      const float a = W.rnd.r(0, mfx::kTau), r = W.rnd.r(0.2f, 0.9f) * b.r;
      W.spawn(mfx::PK_MOTE, b.p + Vec2(std::cos(a) * r, std::sin(a) * r * 0.72f), Vec2(std::sin(a) * 6, -std::cos(a) * 3 - 4), W.rnd.r(1.0f, 1.6f), px4(70, 40, 120), px4(170, 140, 255), px4(240, 230, 255), 1, 0, mfx::PF_ADD);
    }
}

// a filled ellipse in horizontal runs
template <class Pt> void fillEllipse(Pt& P, float cx, float cy, float rx, float ry, Color c, bool add) {
  if (rx < 1 || ry < 1) return;
  for (int y = (int)std::floor(-ry); y <= (int)std::ceil(ry); y++) {
    const float k = 1.0f - ((float)y * (float)y) / (ry * ry);
    if (k <= 0) continue;
    const float hw = std::floor(rx * std::sqrt(k));
    if (add) P.add(std::floor(cx - hw), std::floor(cy) + (float)y, hw * 2 + 1, 1, c);
    else P.rect(std::floor(cx - hw), std::floor(cy) + (float)y, hw * 2 + 1, 1, c);
  }
}
// an unbroken ellipse outline (every pixel round it: no dotted gaps at a large radius), wobbling
template <class Pt> void ellipseLine(Pt& P, float cx, float cy, float rx, float ry, Color c, bool add, float wob = 0, float ph = 0) {
  if (rx < 1) return;
  const int n = std::max(24, (int)(rx * 7.0f));
  int lx = INT32_MIN, ly = INT32_MIN;
  for (int i = 0; i < n; i++) {
    const float a = mfx::kTau * (float)i / (float)n;
    const float w = 1.0f + wob * std::sin(a * 5.0f + ph);
    const int x = (int)std::floor(cx + std::cos(a) * rx * w), y = (int)std::floor(cy + std::sin(a) * ry * w);
    if (x == lx && y == ly) continue;
    lx = x; ly = y;
    if (add) P.add((float)x, (float)y, 1, 1, c); else P.rect((float)x, (float)y, 1, 1, c);
  }
}
// a thorn tile's bramble: it springs up out of the soil (cropped from the ground up, a little overshoot) and withers
// at the end (browns, sinks); two clumps a tile for a thick hedge
template <class Pt> void drawThorn(Pt& P, mfx::World& W, const mfx::Ctx& C, const magic::Growth& w, float k, Color tint) {
  mfx::Sheet& sh = sheetOf(W, 1);
  const float grow = easeOutBack(std::min(1.0f, (w.age - 0.05f * (float)w.order) / 0.45f));
  const float wither = mfx::sat((1.6f - w.life) / 1.6f);
  const float vis = std::max(0.0f, grow * (1.0f - easeOut(wither) * 0.9f));
  if (vis <= 0.02f) return;
  const Vec2 base = mfx::scr(C, Vec2((float)w.tx * TILE, (float)w.ty * TILE + 16));
  const Color wt = mfx::lerpC(Color(1, 1, 1, 1), Color(0.78f, 0.58f, 0.38f, 1), wither);
  for (int c = 0; c < 2; c++) {
    const int var = (int)((w.seed >> (c * 3)) % 4u);
    const float h = std::floor(std::min(24.0f, 24.0f * vis * (c ? 0.86f : 1.0f)));
    if (h < 1) continue;
    const float x = base.x - 2 + (float)c * 6 + (float)((w.seed >> (8 + c)) & 1);
    const float yb = base.y - (float)c * 2;
    P.blit(sh, var * 18, 24 - (int)h, 18, (int)h, x, yb - h, 18, h, Color(tint.r * wt.r * k, tint.g * wt.g * k, tint.b * wt.b * k, tint.a * (1.0f - wither * wither)), 0, ((w.seed >> c) & 1) != 0);
  }
}
// the thorn tiles in drawing order (north first: a clump nearer the viewer overlaps the one behind it)
std::vector<magic::Growth> sortedThorns(const magic::Live& L) {
  std::vector<magic::Growth> v;
  for (const magic::Growth& w : L.growths) if (w.kind == magic::GROW_THORN) v.push_back(w);
  std::stable_sort(v.begin(), v.end(), [](const magic::Growth& a, const magic::Growth& b) { return a.ty < b.ty || (a.ty == b.ty && a.tx < b.tx); });
  return v;
}
bool growthAt(const magic::Live& L, int tx, int ty, uint8_t kind) {
  for (const magic::Growth& w : L.growths) if (w.tx == tx && w.ty == ty && w.kind == kind) return true;
  return false;
}
// the trial the hero stands in (its layout), or nullptr
const TrialLayout* trialHere(const Game& g) {
  if (!g.inside || g.subSite < 0 || g.subSite >= (int)g.world.sites.size()) return nullptr;
  const TrialLayout* T = trialLayoutOf(g.world.sites[(size_t)g.subSite].id);
  return T && !T->plates.empty() ? T : nullptr;
}
Color trialCol(const TrialLayout& T, int i) {
  if (T.spec.ramp[1]) return mfx::cu(T.spec.ramp[std::clamp(i, 0, 2)]);
  static const uint32_t d[3] = {px4(60, 40, 100), px4(170, 140, 255), px4(240, 230, 255)};
  return mfx::cu(d[std::clamp(i, 0, 2)]);
}
// a note glyph (a filled head and a stem) at (x, y), lit
template <class Pt> void noteGlyph(Pt& P, float x, float y, Color c, bool add) {
  auto r = [&](float a, float b, float w, float h) { if (add) P.add(std::floor(x + a), std::floor(y + b), w, h, c); else P.rect(std::floor(x + a), std::floor(y + b), w, h, c); };
  r(0, 1, 3, 2); r(1, 0, 2, 1); r(1, 3, 1, 0); r(2, -5, 1, 6); r(3, -5, 2, 1); r(4, -4, 1, 1);
}
// the Songweaving motif's glyph: three notes on a staff at their degrees; done: notes matched so far glow
template <class Pt> void motifGlyph(Pt& P, float cx, float cy, const uint8_t* motif, int done, Color dim, Color lit, float t) {
  for (int l = 0; l < 3; l++) P.rect(std::floor(cx - 13), std::floor(cy - 2 + (float)l * 3), 26, 1, withA(dim, dim.a * 0.45f));
  for (int i = 0; i < 3; i++) {
    const float x = cx - 10 + (float)i * 8, y = cy + 5 - (float)motif[i] * 1.4f;
    const bool on = i < done, next = i == done;
    Color c = on ? lit : dim;
    if (next) c = withA(lit, (0.35f + 0.35f * std::sin(t * 7.0f)) * lit.a);
    noteGlyph(P, x, y, c, on || next);
  }
}
}  // namespace

// ---- (M10) the ground pass: rune circles and their links, the trials' plates and gates, thorns behind the hero, the
//      vine decks, the bubbles' floors
static void hidGround(View& v, Game& g, GamePt& P, const mfx::Ctx& C) {
  View::MagicFxState& S = MagicFxOps::st(v);
  mfx::World& W = S.w;
  const magic::Live& L = g.arcana.live;
  const Actor* hero = heroOf(g);
  // ---- a Verdant trial's chasms: black water sunk below the floor, the far side's cut face, the near lip lit, slow
  //      ripples (drawn here: a ruin's floor art has no water of its own)
  if (const TrialLayout* T = trialHere(g)) {
    const Map& m = g.map();
    for (const IRect& ch : T->chasms)
      for (int y = ch.y; y < ch.y + ch.h; y++)
        for (int x = ch.x; x < ch.x + ch.w; x++) {
          const Ground gr = m.at(x, y);
          if (!groundWater(gr) && gr != Ground::Bridge) continue;
          const Vec2 s = mfx::scr(C, Vec2((float)x * TILE, (float)y * TILE));
          if (!mfx::onScr(C, s, 20)) continue;
          P.rect(s.x, s.y, 16, 16, Color(0.04f, 0.07f, 0.11f, 1));
          if (y == ch.y) {   // the far side: a cut stone face down into the water, lit from above
            P.rect(s.x, s.y, 16, 6, Color(0.24f, 0.22f, 0.26f, 1));
            P.rect(s.x, s.y, 16, 1, Color(0.42f, 0.4f, 0.44f, 1));
            for (int k = 0; k < 16; k += 5) P.rect(s.x + (float)((k + x * 3) % 16), s.y + 2, 1, 4, Color(0.14f, 0.13f, 0.16f, 1));
            P.rect(s.x, s.y + 6, 16, 2, Color(0.02f, 0.03f, 0.05f, 1));
          }
          if (y == ch.y + ch.h - 1) P.rect(s.x, s.y + 15, 16, 1, Color(0.5f, 0.48f, 0.52f, 1));   // the near lip
          for (int k = 0; k < 2; k++) {   // ripples drifting
            const float ph = std::fmod(S.clock * 0.6f + (float)((x * 7 + y * 3 + k * 5) % 11) / 11.0f, 1.0f);
            const float rx = s.x + 2 + std::fmod((float)(x * 13 + k * 7) + S.clock * 3.0f, 12.0f);
            P.rect(std::floor(rx), s.y + 8 + (float)k * 4, 3, 1, Color(0.3f, 0.45f, 0.6f, 0.35f * std::sin(ph * 3.14159f)));
          }
        }
  }
  // ---- the bubbles' floors: a tinted disc with a slow inner ripple (under the bodies inside it)
  for (const magic::Bubble& b : L.bubbles) {
    const float in = easeOutBack(std::min(1.0f, b.age / 0.3f)), out = mfx::sat(b.life / 0.3f);
    const float r = b.r * in * (0.85f + 0.15f * out);
    const Vec2 s = mfx::scr(C, b.p);
    if (!mfx::onScr(C, s, r + 8)) continue;
    fillEllipse(P, s.x, s.y, r, r * 0.72f, Color(0.30f, 0.20f, 0.62f, 0.30f * out), false);
    fillEllipse(P, s.x, s.y, r * 0.92f, r * 0.66f, Color(0.46f, 0.34f, 0.86f, 0.10f * out), true);
    for (int k = 0; k < 3; k++) {   // the slowed time's ripples, drifting outward
      const float ph = std::fmod(S.clock * 0.3f + (float)k / 3.0f, 1.0f);
      ellipseLine(P, s.x, s.y, r * ph, r * 0.72f * ph, Color(0.78f, 0.7f, 1.0f, 0.4f * (1 - ph) * out), true, 0.02f, S.clock * 2.0f + (float)k);
    }
  }
  // ---- the vine bridges: a braided deck laid out tile by tile from the near bank, a shadow on the water under it
  for (const magic::Growth& w : L.growths) {
    if (w.kind != magic::GROW_VINE) continue;
    // (it runs the way its tiles run: a run along y is a vertical deck, else a horizontal one)
    const bool horiz = !(growthAt(L, w.tx, w.ty - 1, magic::GROW_VINE) || growthAt(L, w.tx, w.ty + 1, magic::GROW_VINE));
    const float grow = easeOut(mfx::sat((w.age - 0.16f * (float)w.order) / 0.32f));
    const float wither = mfx::sat((1.6f - w.life) / 1.6f);
    if (grow <= 0.01f) continue;
    const Vec2 s = mfx::scr(C, Vec2((float)w.tx * TILE, (float)w.ty * TILE));
    if (!mfx::onScr(C, s, 24)) continue;
    mfx::Sheet& sh = sheetOf(W, 2);
    const int var = (int)(w.seed & 1u);
    const float a = 1.0f - wither * wither;
    const Color tint = mfx::lerpC(Color(1, 1, 1, a), Color(0.72f, 0.55f, 0.34f, a), wither);
    P.rect(s.x + 2, s.y + (horiz ? 12 : 2), horiz ? 16 * grow : 14, horiz ? 4 : 16 * grow, Color(0.02f, 0.05f, 0.08f, 0.35f * a));   // its shadow on the water
    if (horiz) {   // (grows from the side the bridge began: order 0's neighbour)
      const bool fromLeft = !growthAt(L, w.tx - 1, w.ty, magic::GROW_VINE) || w.order == 0 ? true : false;
      const int cw = std::max(1, (int)std::floor(16 * grow));
      P.blit(sh, var * 16 + (fromLeft ? 0 : 16 - cw), 0, cw, 16, s.x + (fromLeft ? 0.0f : 16.0f - (float)cw), s.y, (float)cw, 16, tint, 0, false);
    } else {
      const int chh = std::max(1, (int)std::floor(16 * grow));
      P.blit(sh, var * 16, 16 + 16 - chh, 16, chh, s.x, s.y + 16.0f - (float)chh, 16, (float)chh, tint, 0, false);
    }
  }
  // ---- the runes: a circle of glyphs turning slowly on the ground, its heart pulsing; two within reach linked by a
  //      line of light
  int ri = 0;
  for (const magic::Zone& z : L.zones) {
    if (z.kind != magic::ZoneKind::Rune || z.life <= 0) continue;
    const Vec2 s = mfx::scr(C, z.p);
    if (!mfx::onScr(C, s, 30)) continue;
    const mfx::Look Lk = lookFor(z.spell);
    const float t0 = seenAt(S, ((uint64_t)z.seed << 20) ^ (uint64_t)(int)z.p.x * 131 ^ (uint64_t)(int)z.p.y);
    const float in = easeOutBack(std::min(1.0f, (S.clock - t0) / 0.35f));
    const float pulse = 0.75f + 0.25f * std::sin(S.clock * 3.2f + (float)ri);
    const float rx = 15.0f * in, ry = 7.5f * in;
    fillEllipse(P, s.x, s.y, rx + 2, ry + 1, Color(0.05f, 0.03f, 0.08f, 0.45f * in), false);   // the carved, scorched disc
    fillEllipse(P, s.x, s.y, rx, ry, withA(Lk.c(1), 0.22f * in * pulse), true);
    ellipseLine(P, s.x, s.y + 1, rx, ry, Lk.c(1, 0.9f * in), false);   // the groove's shadow
    ellipseLine(P, s.x, s.y, rx, ry, Lk.c(3, 0.95f * in), true);
    ellipseLine(P, s.x, s.y, rx * 0.58f, ry * 0.58f, Lk.c(3, 0.6f * in * pulse), true);
    mfx::Sheet& rs = W.runes(Lk.glyph ? Lk.glyph : 0x5EEDu);
    for (int k = 0; k < 6; k++) {
      const float a = S.clock * 0.6f + mfx::kTau * (float)k / 6.0f;
      const float gx = s.x + std::cos(a) * rx * 1.05f - 4, gy = s.y + std::sin(a) * ry * 1.05f - 4;
      P.blit(rs, (k % 8) * mfx::RUNE, 0, mfx::RUNE, mfx::RUNE, std::floor(gx), std::floor(gy), (float)mfx::RUNE, (float)mfx::RUNE, Lk.c(4, 0.9f * in), 1, false);
    }
    P.blit(rs, (ri % 8) * mfx::RUNE, 0, mfx::RUNE, mfx::RUNE, s.x - 4, s.y - 5, (float)mfx::RUNE, (float)mfx::RUNE, Lk.c(4, in * pulse), 1, false);
    mfx::gglow(P, W, s.x, s.y, 22 * in, Lk.c(2, 0.45f * pulse * in));
    // the link to a partner within reach (the hero's own runes)
    for (const magic::Zone& o : L.zones) {
      if (&o == &z || o.kind != magic::ZoneKind::Rune || o.life <= 0 || o.owner != z.owner) continue;
      if (o.p.x < z.p.x || (o.p.x == z.p.x && o.p.y <= z.p.y)) continue;
      if (len2(o.p - z.p) > magic::RUNE_LINK * magic::RUNE_LINK) continue;
      const Vec2 s2 = mfx::scr(C, o.p);
      const mfx::Look L2 = lookFor(o.spell);
      const int n = std::max(2, (int)(len(s2 - s) / 3.0f));
      for (int k = 1; k < n; k++) {
        const float u = (float)k / (float)n;
        const float wv = std::sin(u * 3.14159f) * 2.0f * std::sin(S.clock * 4.0f + u * 9.0f);
        const Color c = mfx::lerpC(Lk.c(3), L2.c(3), u);
        mfx::dot(P, s.x + (s2.x - s.x) * u, s.y + (s2.y - s.y) * u + wv, withA(c, (0.35f + 0.4f * std::sin(S.clock * 6.0f - u * 10.0f)) * in), true);
      }
    }
    ri++;
  }
  // ---- the thorns behind the hero (their roots north of the hero's feet): drawn under the bodies, back to front
  for (const magic::Growth& w : sortedThorns(L))
    if (!hero || (float)(w.ty + 1) * TILE <= hero->p.y + 1) {
      const Vec2 s = mfx::scr(C, Vec2((float)w.tx * TILE + 8, (float)w.ty * TILE + 15));
      if (!mfx::onScr(C, s, 30)) continue;
      fillEllipse(P, s.x + 2, s.y, 9, 3, Color(0, 0, 0, 0.28f * mfx::sat(w.age / 0.4f) * mfx::sat(w.life)), false);   // its cast shadow
      drawThorn(P, W, C, w, 1.0f, Color(1, 1, 1, 1));
    }
  // ---- the trial's plates and gates
  if (const TrialLayout* T = trialHere(g)) {
    const magic::TrialRun& R = L.trial;
    const Color c0 = trialCol(*T, 0), c1 = trialCol(*T, 1), c2 = trialCol(*T, 2);
    for (size_t i = 0; i < T->plates.size(); i++) {
      const TrialPlate& p = T->plates[i];
      const Vec2 s = mfx::scr(C, Vec2((float)p.x * TILE + 8, (float)p.y * TILE + 10));
      if (!mfx::onScr(C, s, 30)) continue;
      const bool down = (R.platesDown >> i) & 1ull;
      const float pulse = 0.6f + 0.4f * std::sin(S.clock * 2.4f + (float)i * 1.3f);
      switch (p.kind) {
        case TP_NOTE: {   // a round singing stone set in the floor, its note carved in it
          fillEllipse(P, s.x, s.y + 1, 7, 4, Color(0.12f, 0.11f, 0.14f, 0.9f), false);
          fillEllipse(P, s.x, s.y, 6, 3, Color(0.46f, 0.44f, 0.5f, 1), false);
          P.rect(s.x - 4, s.y - 2, 4, 1, Color(0.66f, 0.64f, 0.7f, 1));
          noteGlyph(P, s.x - 2, s.y - 1, down ? withA(c2, 1) : Color(0.2f, 0.18f, 0.24f, 1), down);
          if (down) mfx::gglow(P, W, s.x, s.y, 14, withA(c1, 0.6f));
          break;
        }
        case TP_RUNE: break;   // (its rune circle is the zone's, above)
        case TP_MASK: {   // a stone plinth with its mask floating over it (gone while the hero wears it)
          P.rect(s.x - 5, s.y - 6, 10, 3, Color(0.62f, 0.6f, 0.58f, 1));
          P.rect(s.x - 5, s.y - 3, 10, 5, Color(0.38f, 0.36f, 0.36f, 1));
          P.rect(s.x - 5, s.y - 6, 1, 8, Color(0.74f, 0.72f, 0.7f, 1));
          P.rect(s.x + 4, s.y - 6, 1, 8, Color(0.24f, 0.22f, 0.24f, 1));
          fillEllipse(P, s.x + 1, s.y + 3, 7, 2, Color(0, 0, 0, 0.3f), false);
          if (L.mask != p.arg) {
            const float bob = std::floor(std::sin(S.clock * 2.0f + (float)i) * 1.5f);
            P.blit(sheetOf(W, 0), 0, p.arg * 10, 10, 10, s.x - 5, s.y - 18 + bob, 10, 10, Color(1, 1, 1, 1), 0, false);
            mfx::glow(P, W, s.x, s.y - 13 + bob, 9, withA(maskCol(p.arg, 1), 0.35f * pulse), true);
          }
          break;
        }
        case TP_POD: {   // a seed pod on the bank: a swelling green bulb
          const float sw = 1.0f + 0.12f * std::sin(S.clock * 3.0f + (float)i);
          fillEllipse(P, s.x + 1, s.y + 2, 6, 2, Color(0, 0, 0, 0.3f), false);
          fillEllipse(P, s.x, s.y - 2, 4 * sw, 4 * sw, Color(0.16f, 0.32f, 0.12f, 1), false);
          fillEllipse(P, s.x, s.y - 2, 3 * sw, 3 * sw, Color(0.38f, 0.62f, 0.26f, 1), false);
          P.rect(s.x - 2, s.y - 4, 2, 1, Color(0.7f, 0.86f, 0.46f, 1));
          P.rect(s.x, s.y - 8, 1, 3, Color(0.3f, 0.5f, 0.2f, 1));
          mfx::gglow(P, W, s.x, s.y - 1, 10, Color(0.5f, 0.9f, 0.4f, 0.25f * pulse));
          break;
        }
        case TP_ALTAR: {   // a carved basin before the altar: empty, then brimming red once paid
          const bool paid = (R.paid >> i) & 1ull;
          fillEllipse(P, s.x, s.y, 7, 3, Color(0.2f, 0.18f, 0.18f, 1), false);
          fillEllipse(P, s.x, s.y, 5, 2, paid ? Color(0.62f, 0.06f, 0.1f, 1) : Color(0.1f, 0.08f, 0.08f, 1), false);
          if (paid) { P.rect(s.x - 3, s.y - 1, 2, 1, Color(1, 0.45f, 0.45f, 1)); mfx::gglow(P, W, s.x, s.y, 12, Color(1, 0.2f, 0.2f, 0.4f * pulse)); }
          break;
        }
        case TP_MIRROR: {   // a still violet pool that shows another you
          fillEllipse(P, s.x, s.y, 7, 3, Color(0.18f, 0.12f, 0.26f, 1), false);
          fillEllipse(P, s.x, s.y, 6, 2, Color(0.56f, 0.44f, 0.86f, 0.9f), false);
          const float sx = std::fmod(S.clock * 9.0f, 14.0f) - 7;
          P.add(s.x + sx, s.y - 1, 2, 1, Color(0.9f, 0.85f, 1, 0.8f));
          break;
        }
        case TP_PRESS: {   // a square stone that sinks when weighed down
          const float dy = down ? 1.0f : 0.0f;
          P.rect(s.x - 6, s.y - 4 + dy, 12, 8, Color(0.42f, 0.4f, 0.44f, 1));
          P.rect(s.x - 6, s.y - 4 + dy, 12, 1, Color(0.62f, 0.6f, 0.64f, 1));
          P.rect(s.x - 6, s.y - 4 + dy, 1, 8, Color(0.56f, 0.54f, 0.58f, 1));
          P.rect(s.x - 6, s.y + 4, 12, 1, Color(0.12f, 0.1f, 0.14f, 1));
          if (down) mfx::gglow(P, W, s.x, s.y, 12, withA(c1, 0.5f));
          break;
        }
        case TP_SANCTUM: {   // the tradition's sigil turning on the floor before the altar
          mfx::Sheet& sg = W.sigil(T->spec.tradition ^ 0x51611u);
          const int fr = (int)(S.clock * 6.0f) % mfx::SIG_FR;
          P.blit(sg, fr * mfx::SIG_W, 0, mfx::SIG_W, mfx::SIG_H, s.x - 24, s.y - 12, 48, 24, withA(R.passed ? c2 : c1, 0.75f), 1, false);
          mfx::gglow(P, W, s.x, s.y, 22, withA(c1, 0.3f * pulse));
          break;
        }
        default: break;
      }
    }
    // the gates: a veil across the way in the tradition's light (its pattern the signature's), dissolving when opened
    for (size_t gi = 0; gi < T->gates.size(); gi++) {
      const TrialGate& G = T->gates[gi];
      const bool open = (R.gatesOpen >> gi) & 1u;
      const uint64_t key = 0x6A7E000000ull ^ ((uint64_t)T->spec.tradition << 8) ^ gi;
      float o = 0;
      if (open) o = mfx::sat((S.clock - seenAt(S, key)) / 0.9f);
      if (open && o >= 1) continue;
      const float a = 1.0f - easeOut(o);
      const Vec2 s = mfx::scr(C, Vec2((float)G.x * TILE, (float)G.y * TILE));
      const float w = (float)G.w * TILE, top = s.y - 10, bot = s.y + (float)G.h * TILE;
      if (!mfx::onScr(C, Vec2(s.x + w * 0.5f, top), 60)) continue;
      for (int x = 0; x < (int)w; x++) {   // the veil: columns shimmering in and out
        const float sh = 0.55f + 0.25f * std::sin((float)x * 0.45f + S.clock * 2.2f) + 0.2f * std::sin((float)x * 0.13f - S.clock * 1.3f);
        P.rect(s.x + (float)x, top + 2, 1, bot - top - 2, withA(c0, 0.55f * a * sh));
        P.add(s.x + (float)x, top + 2, 1, bot - top - 2, withA(c1, 0.22f * a * sh));
      }
      P.rect(s.x, top, w, 2, withA(c2, 0.7f * a));
      P.rect(s.x, bot - 2, w, 2, withA(c0, 0.8f * a));
      const magic::Signature sig = (magic::Signature)T->spec.sig;
      const float cx = s.x + w * 0.5f, cy = (top + bot) * 0.5f - o * 14.0f;
      if (sig == magic::Signature::Song) {
        const int done = R.room == G.room ? R.prog : 0;
        motifGlyph(P, cx, cy, T->spec.motif, done, withA(c2, 0.8f * a), withA(Color(1, 0.92f, 0.6f, 1), a), S.clock);
      } else if (sig == magic::Signature::Mask && G.mask >= 0) {
        const float pulse = 0.7f + 0.3f * std::sin(S.clock * 2.5f);
        P.blit(sheetOf(W, 0), 0, G.mask * 10, 10, 10, cx - 10, cy - 10, 20, 20, Color(1, 1, 1, a), 0, false);
        mfx::glow(P, W, cx, cy, 16, withA(maskCol(G.mask, 1), 0.4f * a * pulse), true);
      } else if (sig == magic::Signature::Rune) {
        mfx::Sheet& rs = W.runes(T->spec.tradition ^ 0x60E5u);
        for (int k = 0; k < 6; k++) {
          const float ph = 0.5f + 0.5f * std::sin(S.clock * 3.0f + (float)k * 1.1f);
          P.blit(rs, k * mfx::RUNE, 0, mfx::RUNE, mfx::RUNE, s.x + 4 + (float)(k % 3) * 20, top + 6 + (float)(k / 3) * 14, (float)mfx::RUNE, (float)mfx::RUNE, withA(c2, a * ph), 1, false);
        }
      } else if (sig == magic::Signature::Blood) {
        uint32_t h = (uint32_t)gi * 977u + 13u;
        for (int vein = 0; vein < 5; vein++) {   // veins branching down the membrane, the blood's pulse running along them
          float x = s.x + 6 + (float)vein * 13, y = top + 3;
          for (int k = 0; k < 18; k++) {
            h = hash32(h);
            x += (float)((int)(h % 3u) - 1);
            y += 2;
            const float beat = 0.5f + 0.5f * std::sin(S.clock * 5.0f - (float)k * 0.6f);
            P.rect(std::floor(x), std::floor(y), 1, 2, Color(0.75f + 0.25f * beat, 0.08f, 0.12f, 0.85f * a));
          }
        }
      } else if (sig == magic::Signature::Dream) {
        for (int k = 0; k < 3; k++) {   // a mirror's light sliding across it
          const float x = std::fmod(S.clock * 22.0f + (float)k * 23.0f, w + 20.0f) - 10.0f;
          for (int j = 0; j < (int)(bot - top - 4); j++) P.add(s.x + x + (float)j * 0.3f, top + 2 + (float)j, 2, 1, Color(0.9f, 0.86f, 1, 0.35f * a));
        }
      }
    }
  }
}

// ---- (M10) the light map: runes, plates, gates, bubbles, a worn mask's eyes, the song-fire
static int hidLights(View& v, Game& g, const std::function<void(Vec2, float, Color, float)>& light) {
  View::MagicFxState& S = MagicFxOps::st(v);
  const magic::Live& L = g.arcana.live;
  int n = 0;
  for (const magic::Zone& z : L.zones)
    if (z.kind == magic::ZoneKind::Rune && z.life > 0) { light(z.p, 30, lookFor(z.spell).c(2), 0.55f + 0.15f * std::sin(S.clock * 3.2f)); n++; }
  for (const magic::Bubble& b : L.bubbles) { light(b.p, b.r * 1.1f, Color(0.6f, 0.5f, 1.0f), 0.45f * mfx::sat(b.life / 0.3f)); n++; }
  const Actor* hero = heroOf(g);
  if (hero && L.mask >= 0) { light(hero->p + Vec2(0, -16), 26, maskCol(L.mask, 1), 0.45f); n++; }
  for (const HFx& h : S.hfx) {
    if (h.kind == HX_SONGFIRE) { const float u = h.t / h.life; light(h.q, 70, h.L.c(2), 0.9f * mfx::sat(u * 4.0f) * mfx::sat((1 - u) * 3.0f)); n++; }
    if (h.kind == HX_REFRAIN || h.kind == HX_RITE) { light(h.p, 90, h.L.c(3), 1.0f - h.t / h.life); n++; }
    if (h.kind == HX_MASKSTRIKE) { light(h.p, 50, maskCol((int)h.f, 1), 1.0f - h.t / h.life); n++; }
  }
  if (const TrialLayout* T = trialHere(g)) {
    for (size_t i = 0; i < T->plates.size(); i++) {
      const TrialPlate& p = T->plates[i];
      const Vec2 c((float)p.x * TILE + 8, (float)p.y * TILE + 10);
      if (p.kind == TP_SANCTUM) { light(c, 60, trialCol(*T, 1), 0.7f); n++; }
      else if (p.kind == TP_MASK || p.kind == TP_POD || ((L.trial.platesDown >> i) & 1ull)) { light(c, 26, trialCol(*T, 1), 0.4f); n++; }
    }
    for (size_t gi = 0; gi < T->gates.size(); gi++) {
      if ((L.trial.gatesOpen >> gi) & 1u) continue;
      const TrialGate& G = T->gates[gi];
      light(Vec2((float)G.x * TILE + (float)G.w * 8.0f, (float)(G.y + G.h) * TILE), 58, trialCol(*T, 1), 0.75f);
      n++;
    }
  }
  return n;
}

// ---- (M10) after the lighting: thorns in front of the hero, roots, the mask on the hero's face, the decoys'
//      shimmer, the bubbles' domes, the custom bursts, the signature's pips over the hero's head
static void hidFxDraw(View& v, Game& g, GamePt& P, const mfx::Ctx& C, Pix& pix) {
  View::MagicFxState& S = MagicFxOps::st(v);
  mfx::World& W = S.w;
  const magic::Live& L = g.arcana.live;
  const Actor* hero = heroOf(g);
  const float k = C.lightK;
  // thorns in front of the hero (their roots south of the hero's feet): over the bodies behind them
  for (const magic::Growth& w : sortedThorns(L))
    if (hero && (float)(w.ty + 1) * TILE > hero->p.y + 1) {
      const Vec2 s = mfx::scr(C, Vec2((float)w.tx * TILE + 8, (float)w.ty * TILE + 15));
      if (!mfx::onScr(C, s, 30)) continue;
      fillEllipse(P, s.x + 2, s.y, 9, 3, Color(0, 0, 0, 0.28f * k * mfx::sat(w.age / 0.4f) * mfx::sat(w.life)), false);
      drawThorn(P, W, C, w, k, Color(1, 1, 1, 1));
    }
  // roots coiling up a snared body
  for (const auto& sn : L.snared) {
    const Actor* a = actorById(g, (uint32_t)sn.first);
    if (!a) continue;
    const Vec2 s = mfx::scr(C, feetOf(*a));
    if (!mfx::onScr(C, s, 30)) continue;
    const float up = easeOutBack(mfx::sat((magic::SNARE_SECS * 1.6f - sn.second) / 0.3f + 0.2f)), down = mfx::sat(sn.second / 0.4f);
    const float h = std::floor(12.0f * std::min(up, down));
    if (h < 1) continue;
    mfx::Sheet& rs = sheetOf(W, 3);
    P.blit(rs, (a->id & 1) * 16, 12 - (int)h, 16, (int)h, s.x - 8, s.y + 2 - h, 16, h, Color(k, k, k, 1), 0, false);
  }
  // the bubbles' domes: a rippling rim, a highlight arc at the top left, the clock-hand sweeping round
  for (const magic::Bubble& b : L.bubbles) {
    const float in = easeOutBack(std::min(1.0f, b.age / 0.3f)), out = mfx::sat(b.life / 0.3f);
    const float r = b.r * in * (0.85f + 0.15f * out);
    const Vec2 s = mfx::scr(C, b.p);
    if (!mfx::onScr(C, s, r + 8)) continue;
    ellipseLine(P, s.x, s.y, r + 1, r * 0.72f + 1, Color(0.5f, 0.36f, 0.95f, 0.45f * out), true, 0.02f, S.clock * 4.0f);   // the rim's soft edge
    ellipseLine(P, s.x, s.y, r, r * 0.72f, Color(0.86f, 0.8f, 1.0f, 0.9f * out), true, 0.02f, S.clock * 4.0f);
    ellipseLine(P, s.x, s.y - r * 0.2f, r * 0.97f, r * 0.62f, Color(0.66f, 0.56f, 1.0f, 0.22f * out), true, 0.04f, -S.clock * 3.0f);   // the dome's far side
    for (int i = 0; i < 10; i++) {   // the highlight: a short bright arc at the dome's top left
      const float a = 3.6f + (float)i * 0.07f;
      mfx::dot(P, s.x + std::cos(a) * r * 0.82f, s.y - r * 0.3f + std::sin(a) * r * 0.5f, Color(1, 1, 1, 0.55f * out), true);
    }
    const float ang = -S.clock * 0.6f;   // the slowed clock's hand
    for (int i = 2; i < (int)(r * 0.5f); i++) mfx::dot(P, s.x + std::cos(ang) * (float)i, s.y + std::sin(ang) * (float)i * 0.72f, Color(0.85f, 0.8f, 1, 0.4f * out), true);
  }
  // the decoys: the hero's own look, shimmering with violet scanlines and slipping now and then
  for (const Actor& a : g.actors) {
    if (a.magicTag != magic::MT_DECOY || a.st == AState::Dead) continue;
    const Vec2 s = mfx::scr(C, a.p);
    if (!mfx::onScr(C, s, 30)) continue;
    const float fade = mfx::sat(a.magicT / 0.6f);
    const float glitch = std::sin(S.clock * 23.0f + (float)a.id) > 0.93f ? 2.0f : 0.0f;
    for (int y = 0; y < 24; y += 2) P.add(s.x - 6 + glitch * ((y / 2) % 2 ? 1 : -1), s.y - 24 + (float)y, 12, 1, Color(0.65f, 0.45f, 1.0f, (0.26f + 0.12f * std::sin(S.clock * 9 + (float)y)) * fade));
    P.add(s.x - 7, s.y - 24, 1, 22, Color(0.8f, 0.6f, 1.0f, 0.35f * fade));   // a violet edge light: not quite solid
    P.add(s.x + 6, s.y - 24, 1, 22, Color(0.8f, 0.6f, 1.0f, 0.35f * fade));
    mfx::gglow(P, W, s.x, s.y, 12, Color(0.6f, 0.45f, 1.0f, 0.45f * fade));
  }
  // the spirit mask on the hero's face (front, side or back view by the facing), with a pop as it is put on
  if (hero && L.mask >= 0 && hero->st != AState::Dead) {
    const int m = L.mask;
    const int view = hero->face == 0 ? 0 : hero->face == 1 ? 2 : 1;
    const bool flip = hero->face == 3;
    const float pop = 1.0f + 0.35f * (1.0f - easeOut(mfx::sat((S.clock - S.maskPop) / 0.25f)));
    const Vec2 s = mfx::scr(C, hero->p);
    const float bob = hero->st == AState::Walk ? std::floor(std::fabs(std::sin(hero->animT * 9.0f * 1.5708f)) * 1.0f) : 0.0f;
    const int ch = m == 2 ? 10 : 9;
    const float wpx = 10 * pop, hpx = 10 * pop;
    const float fx = s.x - wpx * 0.5f + (view == 1 ? (flip ? -1.0f : 1.0f) : 0.0f) + 0.5f;
    // (over the face: the mask's eye row on the hero's eyes; seen from behind, its ears and strap ride the crown)
    const float fy = s.y - (view == 2 ? 24.0f : 18.0f) - (m == 2 ? 3.0f : 0.0f) - (hpx - 10) * 0.5f - bob;
    P.blit(sheetOf(W, 0), view * 10, m * 10, 10, ch, std::floor(fx), std::floor(fy), std::floor(wpx), std::floor(hpx * (float)ch / 10.0f), Color(k, k, k, 1), 0, flip);
    if (view == 0 && C.dark > 0.25f) {   // after dark the spirit's eyes glow
      const Color e = maskCol(m, 2);
      const float ey = fy + (m == 2 ? 5.0f : 4.0f) * pop;
      P.add(std::floor(fx + 2 * pop), std::floor(ey), 2, 1, withA(e, 0.9f * C.dark));
      P.add(std::floor(fx + 6 * pop), std::floor(ey), 2, 1, withA(e, 0.9f * C.dark));
      mfx::glow(P, W, fx + wpx * 0.5f, ey, 6, withA(maskCol(m, 1), 0.35f * C.dark), true);
    }
  }
  // the custom bursts
  for (const HFx& h : S.hfx) {
    const float u = mfx::sat(h.t / h.life);
    const Vec2 s = mfx::scr(C, h.p);
    switch (h.kind) {
      case HX_NOTE: {   // the note sung: a ring rings out, the note glyph pops up and drifts
        mfx::ring(P, s.x, s.y, 4 + 16 * easeOut(u), 2 + 8 * easeOut(u), h.L.c(3, 0.8f * (1 - u)), true, 2);
        const Vec2 q = mfx::scr(C, h.q);
        const float pop = easeOutBack(mfx::sat(u * 4.0f));
        noteGlyph(P, q.x - 2, q.y - 10 * u - 2 * pop, h.L.c(4, (1 - u * u)), true);
        break;
      }
      case HX_REFRAIN: {   // the motif's glyph flares over the singer, rings of gold widen
        for (int r = 0; r < 3; r++) {
          const float uu = mfx::sat(u * 1.5f - (float)r * 0.15f);
          if (uu <= 0 || uu >= 1) continue;
          mfx::ring(P, s.x, s.y, 8 + 46 * easeOut(uu), 4 + 23 * easeOut(uu), h.L.c(3 - (r & 1), 0.85f * (1 - uu)), true, 2);
        }
        if (const magic::SpellDef* d = magic::spellById(h.L.spell)) {
          const magic::Tradition& t = magic::traditionOfSpell(*d);
          motifGlyph(P, s.x, s.y - 30 - 8 * u, t.motif, 3, h.L.c(2, 1 - u), Color(1, 0.94f, 0.66f, 1 - u * u), S.clock);
        }
        mfx::glow(P, W, s.x, s.y - 8, 30 * (1 - u), h.L.c(3, 0.6f * (1 - u)));
        break;
      }
      case HX_SONGFIRE: {   // a flame sung into being over the priest: it gathers, burns, and rises away as notes
        const Vec2 q = mfx::scr(C, h.q);
        const float grow = easeOutBack(mfx::sat((u - 0.25f) / 0.2f)), fade = mfx::sat((1 - u) / 0.3f);
        const float a = grow * fade;
        if (a > 0.02f) {
          mfx::Sheet& rs = W.rampSheet(h.L);
          const int fr = (int)(S.clock * 12.0f + (float)h.seed) % mfx::FLAME_FR;
          const float rise = -10.0f * mfx::sat((u - 0.6f) / 0.4f);
          mfx::flame(P, rs, 1, fr, q.x, q.y + 6 + rise, a, 1);
          mfx::flame(P, rs, 0, (fr + 3) % mfx::FLAME_FR, q.x - 4, q.y + 7 + rise, a * 0.7f, 1);
          mfx::flame(P, rs, 0, (fr + 5) % mfx::FLAME_FR, q.x + 4, q.y + 7 + rise, a * 0.7f, 1);
          mfx::glow(P, W, q.x, q.y + rise, 18, h.L.c(2, 0.55f * a), true);
        }
        mfx::glow(P, W, s.x, s.y, 7, h.L.c(3, 0.6f * mfx::sat(u * 6.0f) * fade), true);   // the priest's hands
        break;
      }
      case HX_MASKSTRIKE: {   // the spirit's slash: a crescent sweep in the mask's colours, the animal's head flashing
        const Vec2 q = mfx::scr(C, h.q);
        const Vec2 d = norm(q - s + Vec2(0.001f, 0));
        const Vec2 nrm(-d.y, d.x);
        const int m = (int)h.f;
        const float sweep = easeOut(mfx::sat(u * 2.2f)), fade = 1 - u;
        for (int i = 0; i < 26; i++) {
          const float a = -1.2f + 2.4f * (float)i / 25.0f;
          if ((float)i / 25.0f > sweep) break;
          const float r = 20.0f + 4.0f * std::cos(a * 2.0f);
          const Vec2 p = s + d * (std::cos(a) * r - 6) + nrm * (std::sin(a) * r);
          P.add(std::floor(p.x), std::floor(p.y), 2, 2, withA(maskCol(m, 1), 0.85f * fade));
          P.add(std::floor(p.x), std::floor(p.y), 1, 1, withA(maskCol(m, 2), fade));
        }
        P.blit(sheetOf(W, 0), 0, m * 10, 10, m == 2 ? 10 : 9, q.x - 10, q.y - 12, 20, 20, withA(maskCol(m, 2), 0.7f * (1 - mfx::sat(u * 1.6f))), 1, d.x < 0);
        break;
      }
      case HX_MASKDON: {
        mfx::ring(P, s.x, s.y + 4, 4 + 14 * easeOut(u), 2 + 7 * easeOut(u), Color(0.9f, 0.86f, 1, 0.6f * (1 - u)), true, 1);
        break;
      }
      case HX_DECOY: {
        const float r = 18 * easeOut(u);
        mfx::ring(P, s.x, s.y, r, r * 0.5f, Color(0.75f, 0.6f, 1, 0.8f * (1 - u)), true, 2);
        P.add(s.x - 1, s.y - 26 * (1 - u), 2, 26 * (1 - u), Color(0.8f, 0.7f, 1, 0.5f * (1 - u)));
        break;
      }
      case HX_RITE: {   // the threshold crossed: a pillar of the tradition's light, its sigil rising
        const float a = mfx::sat(u * 5.0f) * (1 - u);
        mfx::beam(P, s.x, s.y - 120, s.y, 26 * (1 - 0.4f * u), h.L.c(3, 0.7f * a), true);
        mfx::Sheet& sg = W.sigil(h.L.glyph ^ 0x717Eu);
        P.blit(sg, ((int)(S.clock * 8) % mfx::SIG_FR) * mfx::SIG_W, 0, mfx::SIG_W, mfx::SIG_H, s.x - 32, s.y - 16 - 40 * u, 64, 32, h.L.c(4, a), 1, false);
        break;
      }
      case HX_GATE: {
        const Vec2 q = mfx::scr(C, h.q);
        mfx::gglow(P, W, s.x, s.y + 8, (q.x - s.x) * 1.4f * (1 - u), h.L.c(3, 0.7f * (1 - u)));
        break;
      }
      case HX_COMBO: {   // two runes' pair: a two-coloured spiral flung out, a ring
        for (int i = 0; i < 24; i++) {
          const float a = (float)i * 0.52f + u * 6.0f;
          const float r = 6.0f + 34.0f * easeOut(u) * ((float)i / 24.0f);
          const Color c = i % 2 ? h.L.c(3) : h.L.c(1);
          P.add(std::floor(s.x + std::cos(a) * r), std::floor(s.y + std::sin(a) * r * 0.55f), 2, 1, withA(c, 0.9f * (1 - u)));
        }
        mfx::ring(P, s.x, s.y, 10 + 40 * easeOut(u), 5 + 20 * easeOut(u), h.L.c(4, 0.8f * (1 - u)), true, 2);
        break;
      }
      case HX_ALTAR: {
        mfx::ring(P, s.x, s.y + 12, 6 + 18 * easeOut(u), 3 + 8 * easeOut(u), Color(0.9f, 0.1f, 0.15f, 0.7f * (1 - u)), true, 2);
        break;
      }
      case HX_SNARE: {
        mfx::ring(P, s.x, s.y, 4 + 10 * easeOut(u), 2 + 4 * easeOut(u), Color(0.6f, 0.46f, 0.26f, 0.7f * (1 - u)), false, 2);
        break;
      }
      default: break;
    }
  }
  // ---- the signature's state over the hero's head (only when it has something to say)
  if (!hero || hero->st == AState::Dead || g.mode != Mode::Play) return;
  const Vec2 hs = mfx::scr(C, hero->p);
  float hy = hs.y - 36;
  if (const magic::Tradition* t = attunedSig(g, magic::Signature::Song)) {
    const int prog = (int)g.arcana.book.res[(int)magic::Resource::Notes];
    if (prog > 0 || g.time - L.noteT < 4.0f) {
      motifGlyph(P, hs.x, hy, t->motif, prog, Color(0.75f, 0.68f, 0.5f, 0.75f), Color(1, 0.88f, 0.45f, 1), S.clock);
      hy -= 12;
    }
  }
  const bool charge = attunedSig(g, magic::Signature::Rune) || attunedSig(g, magic::Signature::Mask) || L.mask >= 0;
  if (charge) {
    const int ch = (int)std::floor(g.arcana.book.res[(int)magic::Resource::Charge] + 0.001f);
    const Color on = L.mask >= 0 ? maskCol(L.mask, 1) : Color(0.75f, 0.6f, 1.0f, 1);
    for (int i = 0; i < 5; i++) {   // five charge diamonds
      const float x = hs.x - 14 + (float)i * 6, y = hy;
      P.rect(x + 1, y, 1, 1, Color(0.08f, 0.06f, 0.1f, 0.8f)); P.rect(x, y + 1, 3, 1, Color(0.08f, 0.06f, 0.1f, 0.8f)); P.rect(x + 1, y + 2, 1, 1, Color(0.08f, 0.06f, 0.1f, 0.8f));
      if (i < ch) { P.add(x + 1, y, 1, 1, on); P.add(x, y + 1, 3, 1, on); P.add(x + 1, y + 2, 1, 1, withA(on, 0.6f)); }
    }
    hy -= 6;
  }
  if (attunedSig(g, magic::Signature::Blood)) {
    const float mul = MagicOps::bloodMul(g);
    if (mul > 1.05f) {
      const float beat = 0.6f + 0.4f * std::sin(S.clock * (4.0f + 6.0f * (mul - 1.0f)));
      P.rect(hs.x - 12, hy - 1, 2, 1, Color(0.9f, 0.1f, 0.14f, beat)); P.rect(hs.x - 13, hy, 4, 2, Color(0.8f, 0.06f, 0.1f, beat)); P.rect(hs.x - 12, hy + 2, 2, 1, Color(0.5f, 0.02f, 0.06f, beat));
      char buf[16];
      std::snprintf(buf, sizeof buf, "X%.1f", mul);
      pix.textS(hs.x - 8, hy - 2, buf, 1, Color(1, 0.55f, 0.5f, 0.95f));
      hy -= 9;
    }
  }
  if (attunedSig(g, magic::Signature::Dream)) {   // the dream's hold: an hourglass, its ring the longest bubble or decoy left
    float left = 0, full = magic::DECOY_SECS;
    for (const magic::Bubble& b : L.bubbles) { if (b.life > left) { left = b.life; full = magic::DREAM_SLOW_SECS; } }
    for (const Actor& a : g.actors) if (a.magicTag == magic::MT_DECOY && a.st != AState::Dead && a.magicT > left) { left = a.magicT; full = std::max(a.magicT, magic::DECOY_SECS); }
    if (left > 0) {
      const float f = mfx::sat(left / std::max(0.1f, full));
      for (int i = 0; i < 16; i++) if ((float)i / 16.0f < f) { const float a = -1.5708f + mfx::kTau * (float)i / 16.0f; mfx::dot(P, hs.x + std::cos(a) * 4, hy + 1 + std::sin(a) * 4, Color(0.75f, 0.62f, 1.0f, 0.9f), true); }
      P.rect(hs.x - 1, hy - 2, 3, 1, Color(0.9f, 0.85f, 1, 1)); P.rect(hs.x, hy - 1, 1, 3, Color(0.7f, 0.55f, 1, 1)); P.rect(hs.x - 1, hy + 2, 3, 1, Color(0.9f, 0.85f, 1, 1));
      hy -= 11;
    }
  }
  if (!L.growths.empty() && attunedSig(g, magic::Signature::Verdant)) {   // the shaping's time left: a leaf and its ring
    float left = 0;
    for (const magic::Growth& w : L.growths) left = std::max(left, w.life);
    const float f = mfx::sat(left / magic::VERDANT_SECS);
    for (int i = 0; i < 16; i++) if ((float)i / 16.0f < f) { const float a = -1.5708f + mfx::kTau * (float)i / 16.0f; mfx::dot(P, hs.x + std::cos(a) * 4, hy + 1 + std::sin(a) * 4, Color(0.6f, 0.9f, 0.4f, 0.9f), true); }
    P.rect(hs.x - 1, hy, 3, 2, Color(0.3f, 0.6f, 0.24f, 1));
  }
}

// ------------------------------------------------------------------ the Ops
View::MagicFxState& MagicFxOps::st(View& v) {
  if (!v.magicFx_) {
    v.magicFx_ = std::make_shared<View::MagicFxState>();
    v.magicFx_->w.caps = mfx::capsFor(v.touchUI);   // (the phone: touch controls on)
  }
  return *v.magicFx_;
}
Pix& MagicFxOps::pix(View& v) { return *v.pix_; }
Audio& MagicFxOps::audio(View& v) { return *v.audio_; }
uint64_t MagicFxOps::musicStyle(View& v) { return v.musicStyle_; }
void MagicFxOps::stage(View& v, float delay, const Event& e) {
  View::MagicFxState& S = st(v);
  View::MagicFxState::Staged s;
  s.at = S.clock + std::max(0.0f, delay); s.e = e;
  S.staged.push_back(s);
}
void MagicFxOps::stageMissile(View& v, float delay, const Projectile& pr) {
  View::MagicFxState& S = st(v);
  View::MagicFxState::Staged s;
  s.at = S.clock + std::max(0.0f, delay); s.missile = true; s.pr = pr;
  S.staged.push_back(s);
}
void MagicFxOps::resetPeaks(View& v) {
  View::MagicFxState& S = st(v);
  S.peak = View::MagicFxStats(); S.msWorst = 0; S.msSum = 0; S.msFrames = 0; S.updWorst = 0;
}
bool mfxScorched(View& v, float x, float y) {
  const mfx::World& W = MagicFxOps::st(v).w;
  for (const mfx::Decal& d : W.decals) {
    if (d.kind != mfx::D_CRATER || d.life - d.t < 1.5f) continue;   // (its last fading seconds: the ground grows back)
    const float rx = (float)mfx::decalW(d.kind) * d.size * 0.5f * 0.62f, ry = (float)mfx::decalH(d.kind) * d.size * 0.5f * 0.62f;
    const float dx = (x - d.p.x) / rx, dy = (y - d.p.y) / ry;
    if (dx * dx + dy * dy < 1.0f) return true;
  }
  return false;
}

bool mfxClaimExplode(View& v, Vec2 p) {
  View::MagicFxState& S = MagicFxOps::st(v);
  for (const mfx::Fx& f : S.w.fx)
    if (f.kind == (uint8_t)magic::Phase::Impact && f.t < 0.05f && len2(f.p - p) < 14 * 14) return true;
  return false;
}
void mfxNoteSfx(View& v, int sfx) {
  View::MagicFxState& S = MagicFxOps::st(v);
  switch ((Sfx)sfx) {
    case Sfx::Fireball: S.classicSfx[0] = true; break;
    case Sfx::Frost: S.classicSfx[1] = true; break;
    case Sfx::Heal: S.classicSfx[2] = true; break;
    case Sfx::Explode: S.classicSfx[3] = true; break;
    default: break;
  }
}

View::MagicFxStats View::magicFxStats() const { return magicFx_ ? magicFx_->last : MagicFxStats(); }

// ------------------------------------------------------------------ events
void View::magicEvent(Game& g, const Event& e) {
  if (hidEvent(*this, g, e)) return;   // (M10) the hidden signatures' own moments
  View::MagicFxState& S = MagicFxOps::st(*this);
  const mfx::Look L = lookFor(e.spell);
  const magic::Phase ph = (magic::Phase)std::clamp(e.a, 0, (int)magic::Phase::COUNT - 1);
  const size_t before = S.w.fx.size();
  S.w.event(ph, L, e.p, e.q, e.f, e.id, e.s);
  if (ph == magic::Phase::Summon)   // the form that climbs out: the summoned actor's own, else a spirit wolf
    for (size_t i = before; i < S.w.fx.size(); i++) {
      mfx::Fx& f = S.w.fx[i];
      if (f.kind != (uint8_t)magic::Phase::Summon) continue;
      const Actor* a = actorById(g, e.id);
      f.sub = (uint8_t)(a && !a->human ? (int)a->mon : (int)art::Monster::Wolf);
    }
}

void View::magicUpdate(Game& g, float dt) {
  View::MagicFxState& S = MagicFxOps::st(*this);
  mfx::World& W = S.w;
  const auto tu = Clock::now();
  S.clock += dt;
  // staged script events and missiles that are due
  for (size_t i = 0; i < S.staged.size();) {
    if (S.staged[i].at > S.clock) { i++; continue; }
    const View::MagicFxState::Staged st = S.staged[i];
    S.staged.erase(S.staged.begin() + (std::ptrdiff_t)i);
    if (st.missile) g.projs.push_back(st.pr);
    else magicEvent(g, st.e);
  }
  // synthetic missiles: the sim has no def for them, so the view gives them their impact and aftermath when they end
  for (View::MagicFxState::Flying& f : S.flying) f.seen = false;
  for (const Projectile& pr : g.projs) {
    if (pr.kind != ProjKind::Spell || !mfx::isSynth(pr.spell)) continue;
    View::MagicFxState::Flying* best = nullptr;
    float bd = 24 * 24;
    for (View::MagicFxState::Flying& f : S.flying) {
      if (f.seen || f.spell != pr.spell) continue;
      const float d = len2(f.p + f.v * dt - pr.p);
      if (d < bd) { bd = d; best = &f; }
    }
    if (!best) { S.flying.push_back(View::MagicFxState::Flying{pr.spell, pr.p, pr.v, pr.mag, true}); continue; }
    best->p = pr.p; best->v = pr.v; best->mag = pr.mag; best->seen = true;
  }
  for (size_t i = 0; i < S.flying.size();) {
    const View::MagicFxState::Flying f = S.flying[i];
    if (f.seen) { i++; continue; }
    S.flying.erase(S.flying.begin() + (std::ptrdiff_t)i);
    const mfx::Look L = lookFor(f.spell);
    const Vec2 end = f.p + f.v * dt;
    W.missileEnd(L, end, f.v, f.mag);
    if (L.leaves != magic::Surface::None) {
      const int tx = (int)std::floor(end.x / TILE), ty = (int)std::floor((end.y + 8) / TILE);
      const int r = L.size >= 3 ? 2 : L.size >= 2 || f.mag >= 1.5f ? 1 : 0;
      for (int j = -r; j <= r; j++)
        for (int k = -r; k <= r; k++)
          if (k * k + j * j <= r * r + r / 2) MagicOps::laySurface(g, tx + k, ty + j, L.leaves, 8.0f + 2.0f * (float)L.size, (uint8_t)std::min(3, 1 + (int)L.size / 2), g.pl().id);
    }
  }
  // the missiles' sheddings (embers, shards, sparks, motes)
  for (const Projectile& pr : g.projs)
    if (isMagicMissile(pr)) W.trail(lookFor(pr.spell), pr.p, pr.v, pr.mag, dt, (uint32_t)(pr.from.x * 7 + pr.from.y * 13));
  // the statuses' sheddings
  if (g.mode != Mode::Title)
    for (const Actor& a : g.actors) {
      if (!a.status || a.st == AState::Dead) continue;
      if (len2(a.p - g.pl().p) > 420.0f * 420.0f) continue;
      float bw, bh;
      bodyOf(a, bw, bh);
      mfx::statusTick(W, feetOf(a), a.status, bw, bh, dt);
    }
  W.update(dt, [&](uint32_t id, Vec2& feet) {
    const Actor* a = actorById(g, id);
    if (!a) return false;
    feet = feetOf(*a);
    return true;
  });
  hidUpdate(*this, g, dt);   // (M10) the hidden signatures' bursts, notes due, leaves shed, wisps
  // the requests
  {
    const float hear = 1.0f;
    for (const mfx::SoundReq& r : W.sounds) {
      const int si = (int)r.s;
      if (!r.motif && S.clock - S.sfxT[si] < 0.05f) continue;   // a burst of events: one voice
      if ((S.classicSfx[0] && r.s == Sfx::FireCast) || (S.classicSfx[1] && r.s == Sfx::FrostCast) ||
          (S.classicSfx[2] && (r.s == Sfx::HolyCast || r.s == Sfx::HolyBloom)) || (S.classicSfx[3] && r.s == Sfx::FireBurst))
        continue;   // the classic spell already sounds there
      S.sfxT[si] = S.clock;
      if (r.motif) {
        const MusicStyle ms = MusicStyle::unpack(MagicFxOps::musicStyle(*this));
        static const int roots[7] = {62, 74, 57, 76, 67, 69, 72};
        audio_->playMotif(r.motifId, (uint8_t)ms.scale, r.timbre, roots[r.timbre % 7], r.vol * hear);
      } else audio_->play(r.s, r.pitch, r.vol * hear);
    }
    W.sounds.clear();
    for (bool& b : S.classicSfx) b = false;
    if (W.shake > 0) { shake_ = std::max(shake_, W.shake); W.shake = 0; }
    if (W.flashA > S.flashA) { S.flashA = W.flashA; S.flashC = W.flashC; }
    if (W.lift > S.liftA) { S.liftA = W.lift; S.liftC = W.liftC; }
    W.flashA = 0; W.lift = 0;
  }
  S.surf.sync(g.arcana.live.field, g.world.ox, g.world.oy, touchUI ? 16 : 32);
  S.updMs = msSince(tu);
  S.updWorst = std::max(S.updWorst, S.updMs);
}

void View::magicShift(Vec2 d) {
  if (!magicFx_) return;
  magicFx_->w.shift(d);
  for (auto& s : magicFx_->staged) { s.e.p += d; s.e.q += d; s.pr.p += d; s.pr.from += d; }
  for (auto& f : magicFx_->flying) f.p += d;
}

// ------------------------------------------------------------------ drawing
void View::drawMagicGround(Game& g, Vec2 cam) {
  View::MagicFxState& S = MagicFxOps::st(*this);
  // a new frame's count begins (the ground pass is the frame's first magic draw)
  S.last = S.cur;
  S.peak.effects = std::max(S.peak.effects, S.cur.effects);
  S.peak.particles = std::max(S.peak.particles, S.cur.particles);
  S.peak.lights = std::max(S.peak.lights, S.cur.lights);
  S.peak.decals = std::max(S.peak.decals, S.cur.decals);
  S.peak.surfaceTiles = std::max(S.peak.surfaceTiles, S.cur.surfaceTiles);
  S.peak.missiles = std::max(S.peak.missiles, S.cur.missiles);
  S.peak.statusOverlays = std::max(S.peak.statusOverlays, S.cur.statusOverlays);
  S.peak.ms = std::max(S.peak.ms, S.cur.ms);
  if (S.cur.effects || S.cur.particles || S.cur.surfaceTiles || S.cur.missiles || S.cur.statusOverlays) {
    S.msWorst = std::max(S.msWorst, S.cur.ms);
    S.msSum += S.cur.ms;
    S.msFrames++;
  }
  S.cur = MagicFxStats();
  const auto t0 = Clock::now();
  GamePt P{*pix_, monsters_};
  mfx::Ctx C;
  C.cam = cam; C.W = (float)Pix::W; C.H = (float)Pix::H; C.t = t_;
  nightOf(g, C.dark, C.lightK);
  S.cur.surfaceTiles = S.surf.draw(P, C, g.arcana.live.field);
  {   // (M9 fixer) the flames behind a tree or a stall go down here, under the scene that hides them
    const std::vector<uint8_t> hide = surfaceBehindProps(g);
    S.surf.drawOver(P, S.w, C, g.arcana.live.field, hide.data(), true);
  }
  mfx::drawGround(P, S.w, C);
  hidGround(*this, g, P, C);   // (M10) runes, vines, thorns behind the hero, bubbles' floors, the trials' plates and gates
  S.cur.decals = (int)S.w.decals.size();
  S.cur.ms += msSince(t0);
}

void View::drawSpellProj(Game& g, const Projectile& pr, int idx, Vec2 cam) {
  (void)g; (void)idx;
  View::MagicFxState& S = MagicFxOps::st(*this);
  const auto t0 = Clock::now();
  GamePt P{*pix_, monsters_};
  mfx::Ctx C;
  C.cam = cam; C.W = (float)Pix::W; C.H = (float)Pix::H; C.t = t_;
  mfx::drawMissileShadow(P, S.w, C, lookFor(pr.spell), pr.p, pr.mag);
  S.cur.missiles++;
  S.cur.ms += msSince(t0);
}

void View::magicLights(Game& g, const std::function<void(Vec2, float, Color, float)>& light) {
  View::MagicFxState& S = MagicFxOps::st(*this);
  const auto t0 = Clock::now();
  int n = 0;
  if (S.liftA > 0.01f) {   // lightning: the whole night lit for a frame
    pix_->rectAdd(0, 0, (float)lightMap_.w, (float)lightMap_.h, S.liftC.withA(S.liftA));
    S.liftA *= 0.35f;
    if (S.liftA < 0.05f) S.liftA = 0;
    n++;
  }
  for (const Projectile& pr : g.projs) {
    if (!isMagicMissile(pr)) continue;
    const mfx::Look L = lookFor(pr.spell);
    const float sc = mfx::baseScale(L) * std::max(0.6f, pr.mag);
    light(pr.p, 56.0f * sc, L.c(2), 0.95f);
    light(pr.p - norm(pr.v) * 14.0f, 34.0f * sc, L.c(1), 0.6f);   // the lit trail behind it
    n += 2;
  }
  n += mfx::lightsOf(S.w, [&](Vec2 p, float r, Color c, float k) { light(p, r, c, k); });
  n += hidLights(*this, g, light);   // (M10)
  n += S.surf.lights(g.arcana.live.field, t_, [&](Vec2 p, float r, Color c, float k) { light(p, r, c, k); });
  for (const Actor& a : g.actors) {
    if (!a.status || a.st == AState::Dead) continue;
    if (a.hasStatus(magic::Status::Burning)) { light(a.p + Vec2(0, -10), 44, Color(1.0f, 0.55f, 0.22f), 0.75f); n++; }
    if (a.hasStatus(magic::Status::Shocked)) { light(a.p + Vec2(0, -10), 30, Color(0.55f, 0.6f, 1.0f), 0.4f + 0.3f * std::sin(t_ * 31 + (float)a.id)); n++; }
  }
  S.cur.lights += n;
  S.cur.ms += msSince(t0);
}

void View::drawMagicFx(Game& g, Vec2 cam) {
  View::MagicFxState& S = MagicFxOps::st(*this);
  const auto t0 = Clock::now();
  GamePt P{*pix_, monsters_};
  mfx::Ctx C;
  C.cam = cam; C.W = (float)Pix::W; C.H = (float)Pix::H; C.t = t_;
  nightOf(g, C.dark, C.lightK);
  mfx::World& W = S.w;
  // the surfaces' flames, clouds, arcs and night glow
  {
    const std::vector<uint8_t> hide = surfaceBehindProps(g);
    S.surf.drawOver(P, W, C, g.arcana.live.field, hide.data(), false);
  }
  // the summoned forms climbing out of their rifts: the creature's sheet rising through the rift line, in the
  // school's light, then fading into the real summon (or into motes, a phase-A summon with no actor)
  for (const mfx::Fx& f : W.fx)
    if (f.kind == (uint8_t)magic::Phase::Summon && f.delay <= 0)
      mfx::drawSummonForm(P, W, C, f, (int)f.sub, art::monsterCellH((art::Monster)f.sub), (int)(t_ * 8) % 4);
  // the missiles' lit bodies (after the lighting: they burn through the night)
  for (const Projectile& pr : g.projs) {
    if (!isMagicMissile(pr)) continue;
    mfx::drawMissile(P, W, C, lookFor(pr.spell), pr.p, pr.v, pr.mag, pr.life, (uint32_t)(pr.from.x * 7 + pr.from.y * 13), len(pr.p - pr.from));
  }
  for (const mfx::Fx& f : W.fx) if (f.delay <= 0) mfx::drawPhaseFx(P, W, C, f);
  mfx::drawParts(P, W, C);
  hidFxDraw(*this, g, P, C, *pix_);   // (M10) thorns in front, roots, the mask, decoys, domes, bursts, the signature's pips
  // the status overlays on every body that carries one
  int overlays = 0;
  for (const Actor& a : g.actors) {
    if (!a.status || a.st == AState::Dead || a.indoors != g.pl().indoors) continue;
    const Vec2 s = mfx::scr(C, feetOf(a));
    if (!mfx::onScr(C, s, 40)) continue;
    float bw, bh;
    bodyOf(a, bw, bh);
    overlays += mfx::drawStatus(P, W, C, s, bw, bh, a.status, (uint32_t)a.id * 0x2545F491u);
  }
  // the scene flash (lightning, a meteor's landing): one frame full, the next a fading echo
  if (S.flashA > 0.01f) {
    P.add(0, 0, C.W, C.H, S.flashC.withA(S.flashA));
    S.flashA *= 0.3f;
    if (S.flashA < 0.04f) S.flashA = 0;
  }
  S.cur.effects = (int)W.fx.size();
  S.cur.particles = (int)W.parts.size();
  S.cur.statusOverlays = overlays;
  S.cur.ms += msSince(t0);
}
