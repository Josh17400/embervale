// EMBERVALE art (M3c Wildlands): the biomes' own flora, AcaciaTree .. Petals in art_props.h. LAND lane.
//
// Every prop is painted in the shared language of rpg/art: the 3/4 top-down view with the light from the top-left, the
// hue-shifted ramps of art_internal.h (shadows drift to blue-violet, lights to warm yellow), volume from lit clumps and
// shaded undersides, a selective outline and no ink line where a thing grows out of the ground. floraVariant(p, v, eco)
// gives each prop real variants (size, lean, crown shape, colour drift) and grows it as its land does: snow on the
// solids in the cold, a drier tint in the hot lands. The trees match the classic trees' scale (oak 40x48, pine 26x46)
// and keep their trunks clear below the crown, so a wood with a free tile between trunks still reads as trees whose
// crowns overlap. CrystalSpire and AshVent glow in 4 frames. Anchor: bottom-centre of the canvas on the prop's tile.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include "rpg/art/art_internal.h"
#include "rpg/world/biomes.h"

namespace art {

namespace {

struct FloraInfo { uint8_t w, h, frames, variants; };
// AcaciaTree .. Petals (art_props.h order)
const FloraInfo kFlora[] = {
    // AcaciaTree, BaobabTree, GiantTree, GnarledTree, BlossomTree, BambooClump, JungleTree
    {46, 44, 1, 6}, {38, 48, 1, 4}, {54, 68, 1, 4}, {38, 46, 1, 6}, {40, 44, 1, 6}, {28, 52, 1, 6}, {48, 60, 1, 6},
    // GiantMushroom, SilverTree, MangroveTree, SwampCypress, PetrifiedTree, LarchTree, JuniperTree
    {38, 46, 1, 6}, {32, 54, 1, 6}, {44, 44, 1, 4}, {34, 52, 1, 4}, {26, 40, 1, 6}, {28, 48, 1, 6}, {32, 30, 1, 6},
    // Hoodoo, CrystalSpire, BasaltColumns, IceSerac, TermiteMound, AshVent, Gorse, Thornbush
    {26, 46, 1, 6}, {28, 42, 4, 4}, {32, 34, 1, 4}, {30, 36, 1, 4}, {20, 32, 1, 4}, {24, 18, 4, 2}, {20, 18, 1, 4}, {20, 18, 1, 4},
    // Heather, Wildflowers, PrairieGrass, CottonGrass, Agave, DryBrush, Saltbush
    {16, 14, 1, 6}, {16, 14, 1, 12}, {16, 20, 1, 8}, {16, 14, 1, 6}, {16, 14, 1, 4}, {16, 12, 1, 4}, {16, 12, 1, 4},
    // Lichen, Wrack, Shells, GlowCaps, Blightweed, SilverFern, JungleFern, Petals
    {16, 10, 1, 12}, {18, 10, 1, 4}, {16, 10, 1, 4}, {16, 12, 1, 4}, {16, 14, 1, 4}, {18, 14, 1, 4}, {20, 16, 1, 4},
    {16, 10, 1, 4},
};
constexpr int kFloraN = (int)(sizeof(kFlora) / sizeof(kFlora[0]));
static_assert(kFloraN == (int)Prop::Petals + 1 - (int)Prop::AcaciaTree, "a size for every M3c flora prop (AcaciaTree .. Petals)");
inline const FloraInfo& fi(Prop p) { return kFlora[std::clamp((int)p - (int)Prop::AcaciaTree, 0, kFloraN - 1)]; }

// ---- how the land grows it
struct Ctx {
  bool cold = false, hot = false, wet = false;
  int eco = -1;
};
Ctx ctxOf(int eco) {
  Ctx k;
  k.eco = eco;
  if (eco >= 0 && eco < (int)Eco::COUNT) {
    const Eco e = (Eco)eco;
    k.cold = ecoHas(e, EF_COLD);
    k.hot = ecoHas(e, EF_HOT);
    k.wet = ecoHas(e, EF_WET);
  }
  return k;
}
// a ramp drifted a little lighter / warmer or darker / cooler by the variant, and dried toward straw in the hot lands
Ramp tweak(const Ramp& R, const Ctx& k, int v, float amount = 0.08f) {
  Ramp o = R;
  const float d = ((int)(hash3(v, 7, 991) % 5) - 2) * amount * 0.5f;
  for (int i = 0; i < 5; i++) {
    uint32_t c = d > 0 ? lighten(R.c[i], d) : (d < 0 ? darken(R.c[i], -d) : R.c[i]);
    if (k.hot) c = mix(c, kThatch[i], 0.22f);
    o.c[i] = opaque(c);
  }
  return o;
}

// ---- building blocks (the language of art_props.cpp's classic trees)
struct Blob { float x, y, r; };
// a leaf crown of overlapping clumps, painted back (top) to front (bottom): each clump lit on its upper-left with a
// shaded underside and a scalloped edge, the whole crown lit from the top-left, clumpy leaf texture, sunlit sparkles
void crown(Canvas& c, std::vector<Blob> blobs, const Ramp& R, uint32_t seed, float squash = 0.88f, float clumpy = 0.55f) {
  std::stable_sort(blobs.begin(), blobs.end(), [](const Blob& a, const Blob& b) { return a.y < b.y; });
  float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
  for (auto& b : blobs) { minX = std::min(minX, b.x - b.r); maxX = std::max(maxX, b.x + b.r); minY = std::min(minY, b.y - b.r); maxY = std::max(maxY, b.y + b.r); }
  const float gcx = (minX + maxX) * 0.5f, gcy = (minY + maxY) * 0.5f, gw = std::max(1.0f, maxX - minX), gh = std::max(1.0f, maxY - minY);
  int bi = 0;
  for (auto& b : blobs) {
    const float ph = hashf(bi, 3, seed) * TAU, ph2 = hashf(bi, 5, seed) * TAU;
    bi++;
    const float ry = b.r * squash;
    for (int y = (int)std::floor(b.y - ry - 2); y <= (int)std::ceil(b.y + ry + 2); y++)
      for (int x = (int)std::floor(b.x - b.r - 2); x <= (int)std::ceil(b.x + b.r + 2); x++) {
        const float dx = (x + 0.5f - b.x) / b.r, dy = (y + 0.5f - b.y) / ry;
        const float d = std::sqrt(dx * dx + dy * dy);
        const float ang = std::atan2(dy, dx);
        const float edge = 1.0f + 0.09f * std::sin(ang * 6 + ph) + 0.05f * std::sin(ang * 11 + ph2);
        if (d > edge) continue;
        const float nx = dx / edge, ny = dy / edge;
        float l = lightAt(nx * 0.82f, ny * 0.82f);
        l += ((gcx - (x + 0.5f)) / gw + (gcy - (y + 0.5f)) / gh) * 0.35f;
        l += (vnoise(x * 0.55f, y * 0.55f, seed) - 0.5f) * clumpy;
        int kk = lightIndex(l, x, y, 0.12f);
        if (d > edge - 0.12f && ny > 0.2f) kk = std::max(0, kk - 1);
        c.set(x, y, R[kk]);
      }
  }
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      if (c.get(x, y) != R[3]) continue;
      const uint32_t h = hash3(x, y, seed + 99);
      if (h % 9 == 0) { c.set(x, y, R[4]); if (h % 2) c.set(x + 1, y, R[4]); }
      else if (h % 13 == 0 && c.get(x, y + 1) == R[3]) c.set(x, y + 1, R[2]);
    }
}
// a trunk: a shaded column with bark streaks, flaring into roots over its last rows (flare: px of spread)
void trunk(Canvas& c, float cx, int baseY, int topY, float w, const Ramp& R, uint32_t seed, float flare = 7.0f, float lean = 0.0f) {
  for (int y = topY; y <= baseY; y++) {
    const float t = (float)(y - topY) / std::max(1, baseY - topY);
    const float hw = w * 0.5f + (t > 0.75f ? (t - 0.75f) * flare : 0.0f);
    const float mx = cx + lean * (1.0f - t);
    const int x0 = (int)std::floor(mx - hw), x1 = (int)std::ceil(mx + hw) - 1;
    for (int x = x0; x <= x1; x++) {
      const float u = (x + 0.5f - mx) / std::max(0.5f, hw);
      int k = lightIndex(lightAt(u * 0.9f, 0) + (hashf(x, y / 3, seed) - 0.5f) * 0.25f, x, y, 0.0f);
      if (hash3(x, y / 2, seed) % 7 == 0) k = std::max(0, k - 1);
      c.set(x, y, R[k]);
    }
  }
}
// snow laid on the upward faces (gentle slopes only, the sides stay bare)
void snowOn(Canvas& c, int depth, uint32_t seed, int maxY = 1 << 20) {
  std::vector<int> top(c.w, -1);
  for (int x = 0; x < c.w; x++)
    for (int y = 0; y < c.h; y++) if (solid(c, x, y)) { top[x] = y; break; }
  for (int x = 0; x < c.w; x++) {
    if (top[x] < 0 || top[x] > maxY) continue;
    const int l = x > 0 && top[x - 1] >= 0 ? top[x - 1] : top[x] + 9, r = x + 1 < c.w && top[x + 1] >= 0 ? top[x + 1] : top[x] + 9;
    const int slope = std::max(std::abs(l - top[x]), std::abs(r - top[x]));
    if (slope > 2) continue;
    const int d = depth + (int)(hash3(x, 0, seed) % 2) - (slope == 2 ? 1 : 0);
    for (int k = 0; k < d && solid(c, x, top[x] + k); k++) c.set(x, top[x] + k, kSnow[k == 0 ? 4 : (k == d - 1 ? 2 : 3)]);
  }
}
// the finish of a solid: a selective outline, and no ink line where it stands on the ground
void finishSolid(Canvas& c, float strength = 0.92f) {
  const Canvas before = c;
  outline(c, strength);
  for (int y = c.h - 3; y < c.h; y++)
    for (int x = 0; x < c.w; x++) if (!solid(before, x, y) && !solid(before, x, y - 1)) c.set(x, y, 0);
}
// the finish of a ground cover: a soft outline (none along its foot), the lowest rows thinned into the ground
void finishCover(Canvas& c, float strength = 0.7f, int rows = 3) {
  const Canvas before = c;
  outline(c, strength);
  for (int y = c.h - 2; y < c.h; y++)
    for (int x = 0; x < c.w; x++) if (!solid(before, x, y)) c.set(x, y, 0);
  for (int y = c.h - rows; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const uint32_t q = c.get(x, y);
      if (!chA(q)) continue;
      if (luma(q) < 0.15f || bayer(x, y) < (y - (c.h - rows)) * (0.9f / rows)) c.set(x, y, 0);
    }
}
// frost on the tips of a ground cover in the cold
void frostTips(Canvas& c, uint32_t seed) {
  const Canvas s = c;
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++)
      if (solid(s, x, y) && !solid(s, x, y - 1) && hashf(x, y, seed) < 0.7f) c.set(x, y, kSnow[3]);
}
// a grass blade from (x, y) rising h px, leaning (its tip lean px aside), in ramp R (k0: its darkest step)
void blade(Canvas& c, float x, int y, int h, float lean, const Ramp& R, int k0 = 1) {
  for (int j = 0; j < h; j++) {
    const float t = (float)j / std::max(1, h);
    const int px2 = (int)std::floor(x + lean * t * t);
    int k = t < 0.3f ? k0 : (t < 0.72f ? k0 + 1 : k0 + 2);
    c.set(px2, y - j, R[std::min(4, k)]);
  }
}

// =====================================================================================================================
// trees
// =====================================================================================================================
const Ramp kBarkW = ramp5(rgba(52, 36, 40), rgba(84, 58, 48), rgba(118, 86, 62), rgba(150, 114, 80), rgba(180, 146, 104));

// a flat umbrella crown on a forked trunk (savanna, scrubland, oasis)
void acacia(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 5101u + (uint32_t)v * 71u;
  const Ramp L = tweak(ramp5(rgba(38, 56, 34), rgba(70, 92, 40), rgba(108, 128, 50), rgba(150, 160, 64), rgba(198, 198, 100)), k, v);
  const float lean = ((int)(v % 5) - 2) * 1.3f;
  const float cx = W * 0.5f - lean * 0.5f;
  const float forkY = H * (0.62f + 0.04f * (v & 1));
  const float crownY = H * (0.30f + 0.03f * (v % 3));
  const float rx = W * (0.36f + 0.04f * (float)(v % 3));
  capsule(c, V(cx, H - 2), V(cx + lean, forkY), 2.6f, 2.0f, kBarkW, 0, 0);
  const int nb = 3 + (v & 1);
  for (int i = 0; i < nb; i++) {
    const float t = (float)i / (nb - 1) - 0.5f;
    const Vec2 a = V(cx + lean, forkY);
    const Vec2 b = V(cx + lean + t * rx * 1.5f + (hashf(i, 1, seed) - 0.5f) * 3.0f, crownY + 3.0f + std::fabs(t) * 2.0f);
    const Vec2 m = (a + b) * 0.5f + V(t * 4.0f, -1.0f);
    capsule(c, a, m, 1.8f, 1.4f, kBarkW, 0, 0);
    capsule(c, m, b, 1.4f, 0.9f, kBarkW, 0, 0);
  }
  std::vector<Blob> bl;
  const int n = 7;
  for (int i = 0; i < n; i++) {
    const float t = (float)i / (n - 1) - 0.5f;
    bl.push_back({cx + lean + t * 2.0f * rx * 0.82f + (hashf(i, 2, seed) - 0.5f) * 2.0f, crownY + 1.5f + (hashf(i, 3, seed) - 0.5f) * 2.0f + std::fabs(t) * 1.5f,
                  5.5f + hashf(i, 4, seed) * 2.0f});
  }
  const int up = 3 + (v % 2);
  for (int i = 0; i < up; i++) {
    const float t = up == 1 ? 0 : (float)i / (up - 1) - 0.5f;
    bl.push_back({cx + lean + t * rx * 0.9f + (hashf(i, 6, seed) - 0.5f) * 2.0f, crownY - 3.0f, 5.0f + hashf(i, 7, seed) * 1.5f});
  }
  crown(c, bl, L, seed, 0.42f, 0.45f);
  if (k.cold) snowOn(c, 2, seed + 3);
  finishSolid(c);
}

// a swollen bottle trunk, a small crown of stubby branches (savanna)
void baobab(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 5201u + (uint32_t)v * 59u;
  const Ramp B = ramp5(rgba(62, 46, 56), rgba(100, 78, 80), rgba(140, 114, 104), rgba(174, 150, 130), rgba(204, 186, 160));
  const Ramp L = tweak(ramp5(rgba(34, 58, 40), rgba(60, 96, 46), rgba(96, 132, 54), rgba(140, 166, 68), rgba(190, 200, 100)), k, v);
  const float cx = W * 0.5f + ((v & 1) ? 0.5f : -0.5f);
  const int top = (int)(H * (0.30f + 0.04f * (v % 2))), base = H - 2;
  const float girth = 7.5f + (v % 3);
  for (int y = top; y <= base; y++) {
    const float t = (float)(y - top) / (base - top);
    const float hw = 3.0f + (girth - 3.0f) * std::sin(std::min(1.0f, t * 1.15f) * PI * 0.62f) + (t > 0.9f ? (t - 0.9f) * 18.0f : 0.0f);
    for (int x = (int)std::floor(cx - hw); x <= (int)std::ceil(cx + hw) - 1; x++) {
      const float u = (x + 0.5f - cx) / hw;
      int kk = lightIndex(lightAt(u * 0.95f, -0.1f) + (vnoise(x / 1.5f, y / 6.0f, seed) - 0.5f) * 0.35f, x, y, 0.06f);
      // the folds of the bark: vertical ridges, each lit on its left
      const int fold = (int)std::floor((x - cx) * 0.75f + (vnoise(x / 3.0f, y / 10.0f, seed + 1) - 0.5f) * 2.0f + 64.0f);
      if (fold % 3 == 0 && kk > 0) kk--;
      else if (fold % 3 == 1 && kk < 4 && u < 0.3f) kk++;
      if (hash3(x, y, seed + 2) % 23 == 0 && kk > 0) kk--;
      c.set(x, y, B[kk]);
    }
  }
  // stubby branches from the top, each ending in a small leaf clump (or bare in the dry season: variant 3)
  const bool bare = (v & 3) == 3;
  const int nb = 5;
  std::vector<Blob> bl;
  for (int i = 0; i < nb; i++) {
    const float a = -PI * 0.5f + (i - (nb - 1) * 0.5f) * 0.55f + (hashf(i, 1, seed) - 0.5f) * 0.3f;
    const float L0 = 7.0f + hashf(i, 2, seed) * 5.0f;
    const Vec2 p0 = V(cx + std::cos(a) * 2.0f, top + 1.0f), p1 = p0 + V(std::cos(a) * L0, std::sin(a) * L0 * 0.8f);
    capsule(c, p0, p1, 1.9f, 1.0f, B, 0, 0);
    if (!bare) bl.push_back({p1.x, p1.y - 1.0f, 3.6f + hashf(i, 3, seed) * 1.6f});
    else { const Vec2 p2 = p1 + V(std::cos(a + 0.5f) * 3.0f, -2.5f); capsule(c, p1, p2, 0.8f, 0.5f, B, 0, 0); }
  }
  if (!bare) crown(c, bl, L, seed, 0.75f, 0.4f);
  if (k.cold) snowOn(c, 2, seed + 9);
  finishSolid(c);
}

// an old-growth giant: massive buttressed trunk, deep crown (old-growth forest)
void giantTree(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 5301u + (uint32_t)v * 83u;
  // (M3c fixer round 2, review: "old-growth forest does not read as giant; one oak sprite is cloned across the canopy")
  // the four variants are four different trees of the old wood: (0) the great elder, its crown filling the canvas on
  // a trunk wider than any meadow oak; (1) a tall red-barked column of a tree, its narrow crown high over a long bare
  // bole; (2) a broad, lower giant leaning a little; (3) a younger tree of the wood, a size smaller. Their crowns stand
  // at different heights, so the canopy reads in layers, and their trunks show deep vertical furrows.
  const int kind = v % 4;
  static const float kScale[4] = {1.0f, 0.70f, 0.90f, 0.66f}, kTrunk[4] = {14.0f, 10.0f, 12.0f, 8.5f}, kTop[4] = {0.0f, -2.0f, 6.0f, 14.0f};
  const float sc = kScale[kind], top = kTop[kind];
  const Ramp B = kind == 1 ? ramp5(rgba(48, 24, 28), rgba(86, 40, 34), rgba(124, 62, 44), rgba(160, 88, 58), rgba(190, 118, 78))
                           : ramp5(rgba(42, 30, 36), rgba(70, 46, 42), rgba(102, 70, 52), rgba(134, 98, 66), rgba(164, 126, 86));
  const Ramp L = tweak(ramp5(rgba(18, 40, 44), rgba(28, 64, 52), rgba(42, 92, 56), rgba(70, 124, 60), rgba(116, 160, 74)), k, v);
  const float cx = W * 0.5f + (kind == 2 ? 2.0f : (float)((int)(v % 3) - 1));
  trunk(c, cx, H - 2, H / 3 + (kind == 3 ? 10 : 0), kTrunk[kind], B, seed, kind == 0 ? 12.0f : 9.0f);
  // the bark's deep vertical furrows
  for (int y = H / 3; y < H - 3; y++)
    for (int x = (int)(cx - kTrunk[kind] * 0.5f); x <= (int)(cx + kTrunk[kind] * 0.5f); x++) {
      if (!solid(c, x, y)) continue;
      const int fx = ((x + (int)(vnoise(0.5f, y / 9.0f, seed + 9) * 3.0f)) % 3 + 3) % 3;
      if (fx == 0 && hash3(x / 3, y / 5, seed) % 4 != 0) c.set(x, y, B[0]);
    }
  // buttress roots spreading over the ground
  const int roots = kind == 3 ? 2 : 4;
  for (int i = 0; i < roots; i++) {
    const float side = (i & 1) ? 1.0f : -1.0f, reach = (9.0f + (i >> 1) * 4.0f + hashf(i, 1, seed) * 3.0f) * (0.6f + 0.4f * sc);
    capsule(c, V(cx + side * 3.0f, H - 9.0f - (i >> 1) * 2.0f), V(cx + side * reach, H - 2.0f), 2.4f * (0.7f + 0.3f * sc) + (kind == 0 ? 0.6f : 0.0f), 1.0f, B, 0, 0);
  }
  // moss on the trunk's lit side
  for (int y = H / 3; y < H - 4; y++)
    for (int x = (int)cx - 7; x < (int)cx; x++)
      if (solid(c, x, y) && vnoise(x / 2.5f, y / 4.0f, seed + 5) > 0.62f) c.set(x, y, kMoss[(x + y) % 3 == 0 ? 3 : 2]);
  std::vector<Blob> bl;
  const int n = kind == 1 ? 9 : 13;
  const float spreadX = kind == 1 ? W * 0.20f : W * 0.32f * sc, spreadY = kind == 1 ? 15.0f : 13.0f * sc;
  for (int i = 0; i < n; i++) {
    const float a = hashf(i, 2, seed) * TAU, r = std::sqrt(hashf(i, 3, seed));
    bl.push_back({cx + std::cos(a) * r * spreadX + (kind == 2 ? 3.0f : 0.0f), 20.0f + top + std::sin(a) * r * spreadY + (float)(v % 2) * 2.0f,
                  (8.5f + hashf(i, 4, seed) * 3.5f) * (kind == 1 ? 0.82f : sc)});
  }
  bl.push_back({cx, 11.0f + top, 10.0f * (kind == 1 ? 0.85f : sc)});
  if (kind != 1) {
    bl.push_back({cx - W * 0.3f * sc, 30.0f + top, 7.5f * sc});
    bl.push_back({cx + W * 0.3f * sc + (kind == 2 ? 3.0f : 0.0f), 29.0f + top, 7.5f * sc});
  }
  crown(c, bl, L, seed, 0.86f, 0.6f);
  if (k.cold) snowOn(c, 2, seed + 3);
  finishSolid(c);
}

// a twisted near-black tree, a thin dark crown (dark forest, blight edges)
void gnarled(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 5401u + (uint32_t)v * 67u;
  const Ramp D = ramp5(rgba(18, 16, 24), rgba(32, 28, 38), rgba(52, 46, 56), rgba(76, 66, 74), rgba(104, 92, 96));
  const Ramp L = tweak(ramp5(rgba(24, 30, 36), rgba(38, 48, 46), rgba(56, 68, 54), rgba(78, 92, 62), rgba(106, 116, 76)), k, v);
  const float cx = W * 0.5f;
  // the trunk: a sinuous run of capsules, thick at the foot
  Vec2 p = V(cx, H - 2.0f);
  const float sway = ((v & 1) ? 1.0f : -1.0f) * (1.5f + (v % 3));
  for (int i = 0; i < 5; i++) {
    const float t = (i + 1) / 5.0f;
    const Vec2 q = V(cx + std::sin(t * 3.4f + v) * sway, H - 2.0f - t * (H * 0.46f));
    capsule(c, p, q, 3.6f - i * 0.45f, 3.2f - i * 0.45f, D, 0, 0);
    p = q;
  }
  // gnarled branches
  struct Br { Vec2 a; float ang, len, w; int depth; };
  std::vector<Br> st = {{p, -1.57f + (hashf(0, 1, seed) - 0.5f) * 0.4f, 9.0f, 2.6f, 0}};
  std::vector<Vec2> tips;
  while (!st.empty()) {
    const Br b = st.back();
    st.pop_back();
    const Vec2 m = b.a + V(std::cos(b.ang + 0.35f), std::sin(b.ang + 0.35f)) * (b.len * 0.5f);
    const Vec2 e = m + V(std::cos(b.ang - 0.3f), std::sin(b.ang - 0.3f)) * (b.len * 0.55f);
    capsule(c, b.a, m, b.w * 0.5f, b.w * 0.42f, D, 0, 0);
    capsule(c, m, e, b.w * 0.42f, b.w * 0.3f, D, 0, 0);
    if (b.depth < 3) {
      const float s = 0.55f + hashf(b.depth, (int)(b.len * 3), seed) * 0.35f;
      st.push_back({e, b.ang - s, b.len * 0.72f, b.w * 0.7f, b.depth + 1});
      st.push_back({e, b.ang + s, b.len * 0.66f, b.w * 0.66f, b.depth + 1});
      if (b.depth == 0) st.push_back({b.a + (e - b.a) * 0.4f, b.ang + 1.1f * (v & 1 ? 1 : -1), b.len * 0.6f, b.w * 0.6f, 2});
    } else tips.push_back(e);
  }
  // a knot hole and a thin, ragged dark crown (none on the dead ones)
  c.set((int)cx, H - 13, kInk);
  c.set((int)cx + 1, H - 13, D[1]);
  if (v % 3 != 2) {
    std::vector<Blob> bl;
    for (size_t i = 0; i < tips.size(); i += 2) bl.push_back({tips[i].x, tips[i].y + 1.0f, 3.2f + hashf((int)i, 4, seed) * 1.6f});
    crown(c, bl, L, seed, 0.7f, 0.9f);
  }
  if (k.cold) snowOn(c, 1, seed + 7);
  finishSolid(c, 0.95f);
}

// a cherry in full bloom, pink crown (blossom grove)
void blossom(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 5501u + (uint32_t)v * 73u;
  const Ramp B = ramp5(rgba(44, 26, 34), rgba(72, 40, 44), rgba(102, 60, 56), rgba(134, 84, 70), rgba(164, 112, 90));
  Ramp P = ramp5(rgba(128, 58, 92), rgba(186, 96, 134), rgba(228, 146, 176), rgba(246, 192, 212), rgba(255, 232, 240));
  if (v == 3) P = ramp5(rgba(120, 100, 130), rgba(186, 170, 192), rgba(226, 216, 228), rgba(244, 238, 244), rgba(255, 252, 252));   // white
  if (v == 4) P = ramp5(rgba(110, 36, 74), rgba(168, 66, 110), rgba(214, 112, 152), rgba(238, 162, 190), rgba(252, 214, 228));    // deep pink
  P = tweak(P, Ctx{}, v, 0.05f);
  const float cx = W * 0.5f, lean = ((int)(v % 3) - 1) * 1.5f;
  trunk(c, cx, H - 2, (int)(H * 0.5f), 5.0f, B, seed, 6.0f, lean);
  capsule(c, V(cx + lean * 0.5f, H * 0.62f), V(cx - 9.0f, H * 0.40f), 1.4f, 0.9f, B, 0, 0);
  capsule(c, V(cx + lean * 0.5f, H * 0.58f), V(cx + 10.0f, H * 0.42f), 1.4f, 0.9f, B, 0, 0);
  std::vector<Blob> bl;
  const int n = 9 + (v % 3);
  for (int i = 0; i < n; i++) {
    const float a = hashf(i, 2, seed) * TAU, r = std::sqrt(hashf(i, 3, seed));
    bl.push_back({cx + lean + std::cos(a) * r * W * 0.30f, 15.0f + std::sin(a) * r * 8.0f, 6.0f + hashf(i, 4, seed) * 2.5f});
  }
  crown(c, bl, P, seed, 0.84f, 0.5f);
  // petals drifting down and lying at its foot
  for (int i = 0; i < 9; i++) {
    const int x = (int)(hash3(i, 5, seed) % (uint32_t)W), y = H - 1 - (int)(hash3(i, 6, seed) % 3);
    if (!solid(c, x, y)) c.set(x, y, P[3 + (i & 1)]);
  }
  for (int i = 0; i < 4; i++) {
    const int x = (int)(hash3(i, 7, seed) % (uint32_t)W), y = (int)(H * 0.55f + hash3(i, 8, seed) % (uint32_t)(H / 3));
    if (!solid(c, x, y)) c.set(x, y, P[4]);
  }
  if (k.cold) snowOn(c, 2, seed + 3);
  finishSolid(c);
}

// a clump of tall jointed stems with leaf sprays (bamboo forest)
void bamboo(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 5601u + (uint32_t)v * 61u;
  const Ramp S = tweak(ramp5(rgba(40, 72, 40), rgba(70, 116, 50), rgba(112, 160, 62), rgba(158, 196, 84), rgba(208, 228, 128)), k, v, 0.06f);
  const Ramp Lf = tweak(ramp5(rgba(30, 62, 40), rgba(50, 102, 50), rgba(84, 144, 58), rgba(130, 184, 74), rgba(184, 218, 110)), k, v, 0.06f);
  const int n = 4 + (v % 3);
  struct Stem { float x, lean; int h; bool back; };
  std::vector<Stem> stems;
  for (int i = 0; i < n; i++) {
    const float x = W * 0.5f + (i - (n - 1) * 0.5f) * 3.4f + (hashf(i, 1, seed) - 0.5f) * 2.0f;
    const float mid = 1.0f - std::fabs((i + 0.5f) / n - 0.5f) * 1.2f;   // the tallest stems in the clump's heart
    stems.push_back({x, (x - W * 0.5f) * 0.22f + (hashf(i, 2, seed) - 0.5f) * 2.5f, (int)(H * (0.40f + 0.38f * mid + 0.18f * hashf(i, 3, seed))), (i & 1) == 0});
  }
  // the back stems first (in shade), the front ones over them
  for (int pass = 0; pass < 2; pass++)
    for (const Stem& s : stems) {
      if (s.back != (pass == 0)) continue;
      const int bias = s.back ? -1 : 0;
      const int node0 = (int)(hash3((int)(s.x * 10), 4, seed) % 6);
      for (int j = 0; j < s.h; j++) {
        const float t = (float)j / s.h;
        const int x = (int)std::floor(s.x + s.lean * t), y = H - 2 - j;
        const bool node = ((j + node0) % 7) == 0;
        c.set(x, y, S[std::clamp((node ? 4 : 3) + bias, 0, 4)]);
        c.set(x + 1, y, S[std::clamp((node ? 2 : 1) + bias, 0, 4)]);
        if (node) c.set(x - 1, y, S[std::clamp(2 + bias, 0, 4)]);
        if (node && j > 8 && hash3(j, (int)s.x, seed) % 2 == 0) {   // a leaf spray from the node
          const int dir = (hash3(j, 9, seed) & 1) ? 1 : -1;
          for (int q = 1; q <= 4; q++) { c.set(x + dir * q, y + q / 2, Lf[std::clamp(3 - q / 2 + bias, 0, 4)]); if (q > 1) c.set(x + dir * q, y + q / 2 + 1, Lf[std::clamp(1 + bias, 0, 4)]); }
        }
      }
      // the spray of leaves at the top: long narrow leaves drooping out each way, the left ones in the light
      const int tx = (int)std::floor(s.x + s.lean), ty = H - 2 - s.h;
      for (int q = 0; q < 8; q++) {
        const float a = -PI * 0.5f + (q - 3.5f) * 0.42f + (hashf(q, (int)s.x, seed) - 0.5f) * 0.3f;
        const float L = 5.0f + hashf(q, 2, seed + (uint32_t)s.h) * 3.0f;
        Vec2 p0 = V(tx + 0.5f, ty + 3.0f);
        for (int j = 1; j <= (int)L; j++) {
          const float t = j / L;
          const Vec2 p1 = V(tx + 0.5f + std::cos(a) * L * t * 1.2f, ty + 3.0f + std::sin(a) * L * t * 0.7f + t * t * 4.0f);
          line(c, (int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, Lf[std::clamp((std::cos(a) < 0 ? 3 : 2) + (t > 0.7f ? -1 : 0) + bias, 0, 4)]);
          if (j > 1 && j < L - 1) c.set((int)p1.x, (int)p1.y + 1, Lf[std::clamp(1 + bias, 0, 4)]);
          p0 = p1;
        }
      }
    }
  if (k.cold) snowOn(c, 1, seed + 5);
  finishSolid(c, 0.85f);
}

// a tall broadleaf with buttress roots and hanging vines (jungle)
void jungleTree(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 5701u + (uint32_t)v * 79u;
  const Ramp B = ramp5(rgba(56, 48, 46), rgba(94, 82, 70), rgba(134, 120, 96), rgba(168, 156, 124), rgba(196, 186, 152));
  const Ramp L = tweak(ramp5(rgba(16, 48, 40), rgba(26, 84, 50), rgba(42, 122, 56), rgba(76, 160, 64), rgba(132, 200, 86)), k, v);
  const Ramp Vn = ramp5(rgba(18, 44, 34), rgba(30, 72, 42), rgba(48, 104, 48), rgba(80, 140, 58), rgba(120, 176, 74));
  const float cx = W * 0.5f + ((int)(v % 3) - 1);
  trunk(c, cx, H - 2, (int)(H * 0.42f), 5.0f, B, seed, 8.0f);
  for (int i = 0; i < 4; i++) {   // buttress fins
    const float side = (i & 1) ? 1.0f : -1.0f, reach = 6.0f + (i >> 1) * 3.0f;
    capsule(c, V(cx + side * 1.5f, H - 12.0f + (i >> 1) * 3.0f), V(cx + side * reach, H - 2.0f), 1.6f, 0.8f, B, 0, 0);
  }
  // two layers of crown: a broad lower tier in shade, a lit upper one
  std::vector<Blob> lo, hi;
  for (int i = 0; i < 6; i++) {
    const float t = i / 5.0f - 0.5f;
    lo.push_back({cx + t * W * 0.78f, 26.0f + std::fabs(t) * 4.0f + hashf(i, 1, seed) * 2.0f, 6.5f + hashf(i, 2, seed) * 2.0f});
  }
  for (int i = 0; i < 5; i++) {
    const float t = i / 4.0f - 0.5f;
    hi.push_back({cx + t * W * 0.5f + (hashf(i, 3, seed) - 0.5f) * 3.0f, 13.0f + std::fabs(t) * 6.0f + (v % 2) * 2.0f, 7.0f + hashf(i, 4, seed) * 2.5f});
  }
  hi.push_back({cx, 9.0f, 8.0f});
  crown(c, lo, L, seed + 1, 0.72f, 0.6f);
  crown(c, hi, L, seed, 0.8f, 0.55f);
  // vines hanging from the crown's underside
  for (int i = 0; i < 6; i++) {
    const int x = (int)(cx + (hashf(i, 5, seed) - 0.5f) * W * 0.7f);
    int y0 = -1;
    for (int y = H - 1; y >= 0; y--) if (solid(c, x, y) && y < H * 0.6f) { y0 = y; break; }
    if (y0 < 0) continue;
    const int len = 5 + (int)(hash3(i, 6, seed) % 10);
    for (int j = 1; j <= len; j++) { c.set(x + (j / 4) % 2, y0 + j, Vn[j % 4 == 1 ? 3 : 1]); if (j % 3 == 0) c.set(x + 1 + (j / 4) % 2, y0 + j, Vn[2]); }
  }
  if (k.cold) snowOn(c, 2, seed + 3);
  finishSolid(c);
}

// a mushroom the size of a tree, glowing gills (mushroom forest)
void giantMushroom(Canvas& c, int v, const Ctx& k) {
  (void)k;
  const int W = c.w, H = c.h;
  const uint32_t seed = 5801u + (uint32_t)v * 89u;
  static const Ramp caps[3] = {
      ramp5(rgba(54, 30, 74), rgba(92, 50, 118), rgba(138, 80, 164), rgba(182, 124, 200), rgba(222, 178, 232)),   // violet
      ramp5(rgba(22, 56, 74), rgba(34, 96, 112), rgba(54, 140, 148), rgba(98, 184, 178), rgba(170, 226, 214)),    // teal
      ramp5(rgba(84, 24, 52), rgba(140, 40, 70), rgba(190, 66, 90), rgba(224, 112, 116), rgba(248, 172, 160))};   // rose
  const Ramp& Cp = caps[v % 3];
  const Ramp St = ramp5(rgba(100, 90, 110), rgba(156, 146, 156), rgba(204, 196, 192), rgba(230, 224, 214), rgba(250, 246, 236));
  const float cx = W * 0.5f + ((v & 1) ? 1.0f : -1.0f), lean = ((int)(v % 3) - 1) * 2.0f;
  const float capY = H * (0.30f + 0.04f * (v % 2)), rx = W * (0.40f + 0.04f * ((v >> 1) % 2)), ry = rx * 0.52f;
  // the stem: a pale column swelling at the foot, a ring under the cap
  capsule(c, V(cx, H - 3.0f), V(cx + lean, capY + 3.0f), 4.6f, 3.2f, St, 0, 0.05f);
  ellipse(c, cx, H - 3.0f, 5.5f, 2.0f, St[1]);
  for (int x = (int)(cx + lean * 0.4f - 4); x <= (int)(cx + lean * 0.4f + 4); x++) { c.set(x, (int)(capY + ry + 4), St[x < cx ? 4 : 2]); c.set(x, (int)(capY + ry + 5), St[1]); }
  // the gills: a dark band under the cap's near rim, flecked with glowing spores
  for (int x = (int)(cx + lean - rx * 0.92f); x <= (int)(cx + lean + rx * 0.92f); x++) {
    const float u = (x + 0.5f - (cx + lean)) / rx;
    const int y0 = (int)(capY + ry * 0.55f + (1 - u * u) * ry * 0.35f);
    for (int y = y0; y < y0 + 3; y++) c.set(x, y, (x + y) % 3 == 0 ? rgba(150, 236, 226) : Cp[0]);
  }
  // the cap: a shaded dome with pale spots
  ball(c, cx + lean, capY, rx, ry, Cp, 0.10f);
  for (int i = 0; i < 7; i++) {
    const float a = hashf(i, 1, seed) * TAU, r = 0.25f + 0.6f * hashf(i, 2, seed);
    const float sx = cx + lean + std::cos(a) * rx * r, sy = capY + std::sin(a) * ry * r * 0.8f - 1.0f;
    const float sr = 1.0f + hashf(i, 3, seed) * 1.2f;
    ellipse(c, sx, sy, sr, sr * 0.7f, St[3]);
    c.set((int)std::floor(sx - sr * 0.4f), (int)std::floor(sy - 0.4f), St[4]);
  }
  // tiny caps round the foot
  for (int i = 0; i < 2; i++) {
    const float x = cx + (i ? 7.0f : -8.0f), y = H - 3.0f;
    vline(c, (int)x, (int)y - 2, (int)y, St[2]);
    ball(c, x, y - 3.0f, 2.2f, 1.4f, Cp, 0.0f);
  }
  if (k.cold) snowOn(c, 2, seed + 7, (int)capY + 2);
  finishSolid(c);
}

// a slender pale tree with silver-green leaves (silverwood)
void silverTree(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 5901u + (uint32_t)v * 97u;
  const Ramp B = ramp5(rgba(96, 104, 118), rgba(150, 158, 168), rgba(200, 206, 210), rgba(228, 232, 232), rgba(250, 252, 250));
  const Ramp L = tweak(ramp5(rgba(38, 62, 70), rgba(64, 98, 98), rgba(100, 138, 128), rgba(144, 178, 160), rgba(196, 222, 204)), k, v, 0.05f);
  // (M3c fixer round 3, review: "one pale tree sprite repeated evenly with almost no size or shape variation") the six
  // variants are three builds (a tall slender one, a middling one, a low broad one) each in two leans: the crown's top,
  // its width and the trunk's height all follow the build
  const int build = (v / 2) % 3;
  const float cx = W * 0.5f, lean = ((v & 1) ? 1.0f : -1.0f) * (0.6f + 0.6f * (float)(v % 3));
  const float wmul = build == 0 ? 0.78f : build == 1 ? 1.0f : 1.22f;
  trunk(c, cx, H - 2, (int)(H * (build == 2 ? 0.26f : 0.34f)), 3.6f, B, seed, 5.0f, lean);
  for (int y = (int)(H * 0.4f); y < H - 3; y++) if (hash3(0, y, seed) % 6 == 0) c.set((int)(cx + lean * (1 - (y - H * 0.34f) / (H * 0.66f))) + 1, y, B[0]);
  capsule(c, V(cx + lean * 0.6f, H * 0.52f), V(cx - 6.0f, H * 0.34f), 1.0f, 0.6f, B, 0, 0);
  capsule(c, V(cx + lean * 0.6f, H * 0.47f), V(cx + 6.0f, H * 0.30f), 1.0f, 0.6f, B, 0, 0);
  std::vector<Blob> bl;
  const float top = build == 0 ? 3.0f : build == 1 ? 8.0f : 15.0f;
  const float span = build == 0 ? 0.46f : build == 1 ? 0.38f : 0.30f;
  for (int i = 0; i < 9; i++) {
    const float t = (float)i / 8.0f;
    bl.push_back({cx + lean + (hashf(i, 1, seed) - 0.5f) * W * (0.25f + 0.25f * std::sin(t * PI)) * wmul, top + t * H * span,
                  (4.2f + std::sin(t * PI) * 2.6f) * (0.85f + 0.15f * wmul)});
  }
  crown(c, bl, L, seed, 0.9f, 0.5f);
  // trailing leaf strands and a few silver glints
  for (int i = 0; i < 5; i++) {
    const int x = (int)(cx + (hashf(i, 2, seed) - 0.5f) * W * 0.6f);
    int y0 = -1;
    for (int y = (int)(H * 0.7f); y >= 0; y--) if (solid(c, x, y)) { y0 = y; break; }
    if (y0 < 0) continue;
    for (int j = 1; j <= 3 + (int)(hash3(i, 3, seed) % 4); j++) c.set(x, y0 + j, L[j & 1 ? 2 : 3]);
  }
  for (int i = 0; i < 6; i++) { const int x = (int)(hash3(i, 4, seed) % (uint32_t)W), y = (int)(hash3(i, 5, seed) % (uint32_t)(H / 2)); if (solid(c, x, y)) c.set(x, y, rgba(255, 255, 250)); }
  if (k.cold) snowOn(c, 2, seed + 3);
  finishSolid(c);
}

// a crown on arching stilt roots (mangrove coast)
void mangrove(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 6001u + (uint32_t)v * 53u;
  const Ramp B = ramp5(rgba(48, 38, 40), rgba(80, 64, 56), rgba(114, 94, 74), rgba(146, 124, 96), rgba(176, 156, 122));
  const Ramp L = tweak(ramp5(rgba(24, 54, 46), rgba(38, 88, 58), rgba(60, 122, 64), rgba(98, 158, 76), rgba(152, 194, 102)), k, v);
  const float cx = W * 0.5f;
  const float hub = H - 14.0f - (v % 2) * 2.0f;
  capsule(c, V(cx, hub), V(cx + ((v & 1) ? 1.0f : -1.0f), H * 0.42f), 2.2f, 1.8f, B, 0, 0);
  // the stilt roots: arches from the trunk down into the water, the far ones in shade
  const int nr = 6;
  for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < nr; i++) {
      const bool far = (i & 1) == 0;
      if (far != (pass == 0)) continue;
      const float t = (float)i / (nr - 1) - 0.5f;
      const Vec2 a = V(cx + t * 3.0f, hub + (hashf(i, 1, seed) - 0.5f) * 3.0f);
      const Vec2 e = V(cx + t * W * 0.78f, H - 2.0f - (far ? 1.0f : 0.0f));
      const Vec2 m = V((a.x + e.x) * 0.5f + t * 4.0f, std::min(a.y, e.y) - 3.0f);
      capsule(c, a, m, 1.3f, 1.1f, B, far ? -1 : 0, 0);
      capsule(c, m, e, 1.1f, 0.9f, B, far ? -1 : 0, 0);
    }
  std::vector<Blob> bl;
  for (int i = 0; i < 9; i++) {
    const float a = hashf(i, 2, seed) * TAU, r = std::sqrt(hashf(i, 3, seed));
    bl.push_back({cx + std::cos(a) * r * W * 0.28f, 14.0f + std::sin(a) * r * 6.0f, 6.0f + hashf(i, 4, seed) * 2.5f});
  }
  crown(c, bl, L, seed, 0.78f, 0.55f);
  // the water lapping round the roots: a few ripples where they go in
  for (int x = 2; x < W - 2; x++) if (solid(c, x, H - 2) && !solid(c, x, H - 1) && hash3(x, 9, seed) % 2) c.set(x, H - 1, withA(kWater[4], 140));
  if (k.cold) snowOn(c, 2, seed + 3);
  finishSolid(c);
}

// a buttressed cypress with knees and hanging moss (flooded forest, bogs)
void swampCypress(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 6101u + (uint32_t)v * 71u;
  const Ramp B = ramp5(rgba(46, 34, 38), rgba(78, 58, 50), rgba(112, 86, 66), rgba(144, 116, 86), rgba(172, 146, 110));
  const Ramp L = tweak(ramp5(rgba(30, 46, 36), rgba(54, 76, 46), rgba(84, 108, 56), rgba(122, 142, 70), rgba(170, 180, 100)), k, v);
  const Ramp Ms = ramp5(rgba(70, 78, 70), rgba(104, 112, 98), rgba(140, 146, 126), rgba(174, 178, 154), rgba(204, 206, 182));
  const float cx = W * 0.5f;
  trunk(c, cx, H - 2, (int)(H * 0.30f), 4.0f, B, seed, 12.0f);
  for (int i = 0; i < 4; i++) {   // the buttressed foot spreading into the mire
    const float side = (i & 1) ? 1.0f : -1.0f, reach = 5.0f + (i >> 1) * 3.0f;
    capsule(c, V(cx + side * 1.5f, H - 10.0f + (i >> 1) * 3.0f), V(cx + side * reach, H - 2.0f), 1.8f, 0.9f, B, 0, 0);
  }
  for (int i = 0; i < 3; i++) {   // knees standing out of the mire
    const float x = cx + (i == 0 ? -10.0f : i == 1 ? 9.0f : 13.0f) + (hashf(i, 1, seed) - 0.5f) * 2.0f;
    capsule(c, V(x, H - 2.0f), V(x + 0.3f, H - 5.0f - (float)(hash3(i, 2, seed) % 3)), 1.3f, 0.7f, B, 0, 0);
  }
  // the crown: soft feathery tiers narrowing upward
  std::vector<Blob> bl;
  for (int i = 0; i < 4; i++) {
    const float y = 8.0f + i * 6.5f, w = 4.0f + i * 2.4f;
    bl.push_back({cx - w * 0.6f, y, 3.6f + i * 0.7f});
    bl.push_back({cx + w * 0.6f, y + 0.5f, 3.6f + i * 0.7f});
    bl.push_back({cx, y - 1.0f, 3.8f + i * 0.5f});
  }
  crown(c, bl, L, seed, 0.7f, 0.85f);
  // grey moss hanging from the boughs
  for (int i = 0; i < 7; i++) {
    const int x = (int)(cx + (hashf(i, 3, seed) - 0.5f) * W * 0.6f);
    int y0 = -1;
    for (int y = (int)(H * 0.66f); y >= 0; y--) if (solid(c, x, y)) { y0 = y; break; }
    if (y0 < 0) continue;
    for (int j = 1; j <= 3 + (int)(hash3(i, 4, seed) % 6); j++) c.set(x + (j > 3 ? 1 : 0) * ((i & 1) ? 1 : -1), y0 + j, Ms[j == 1 ? 3 : (j & 1) + 1]);
  }
  if (k.cold) snowOn(c, 2, seed + 3);
  finishSolid(c);
}

// a stone trunk, standing or broken, banded red and grey (petrified forest)
void petrified(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 6201u + (uint32_t)v * 43u;
  static const Ramp bands[3] = {
      ramp5(rgba(74, 40, 44), rgba(120, 70, 62), rgba(162, 108, 86), rgba(196, 150, 120), rgba(226, 194, 164)),
      ramp5(rgba(62, 58, 70), rgba(98, 94, 104), rgba(136, 132, 138), rgba(172, 168, 168), rgba(206, 202, 196)),
      ramp5(rgba(92, 60, 40), rgba(146, 102, 60), rgba(192, 146, 86), rgba(220, 186, 124), rgba(242, 220, 170))};
  const float cx = W * 0.5f + ((int)(v % 3) - 1);
  const bool broken = (v % 2) == 1;
  const int top = (int)(H * (broken ? 0.30f + 0.12f * hashf(v, 1, seed) : 0.10f));
  const float w = 8.0f + (v % 3);
  for (int y = top; y < H - 1; y++) {
    const float t = (float)(y - top) / (H - 1 - top);
    const float hw = w * 0.5f + (t > 0.8f ? (t - 0.8f) * 12.0f : 0.0f);
    const int band = (y + (int)(hash3(y / 4, 0, seed) % 2)) / 4;
    const Ramp& R = bands[hash3(band, 1, seed) % 3];
    for (int x = (int)std::floor(cx - hw); x <= (int)std::ceil(cx + hw) - 1; x++) {
      const float u = (x + 0.5f - cx) / hw;
      int kk = lightIndex(lightAt(u * 0.92f, 0), x, y, 0.05f);
      if (hash3(x, y, seed) % 11 == 0 && kk > 0) kk--;
      c.set(x, y, R[kk]);
    }
    if (hash3(y, 2, seed) % 9 == 0) for (int x = (int)(cx - hw); x <= (int)(cx + hw); x++) if (solid(c, x, y)) c.set(x, y, darken(c.get(x, y), 0.4f));   // a crack round it
  }
  if (broken) {   // the jagged break, the stone's rings showing on the top
    for (int x = (int)(cx - w * 0.5f); x <= (int)(cx + w * 0.5f); x++) {
      const int j = (int)(hash3(x, 3, seed) % 4);
      for (int y = top; y < top + j; y++) c.set(x, y, 0);
      c.set(x, top + j, bands[2][3]);
    }
    // the fallen piece beside it
    for (int x = 0; x < 9; x++)
      for (int y = 0; y < 4; y++) {
        const int px2 = (int)cx + ((v & 2) ? 2 : -11) + x, py2 = H - 5 + y;
        if (solid(c, px2, py2)) continue;
        c.set(px2, py2, bands[(x / 3) % 3][y == 0 ? 3 : (y == 3 ? 1 : 2)]);
      }
  } else {   // a stub of a branch and a weathered top
    capsule(c, V(cx + 1.0f, H * 0.36f), V(cx + 6.0f, H * 0.26f), 1.4f, 1.0f, bands[1], 0, 0);
    ellipse(c, cx, (float)top, w * 0.5f, 1.4f, bands[0][3]);
  }
  if (k.cold) snowOn(c, 2, seed + 5);
  finishSolid(c);
}

// conifer tiers (the larch's): a jagged-edged stack of tiers, its needles parted to show the light
void conifer(Canvas& c, const Ramp& P, uint32_t seed, int tiers, bool sparse, bool snow) {
  const int W = c.w, H = c.h;
  const float cx = W * 0.5f;
  trunk(c, cx, H - 2, (int)(H * 0.30f), 3.0f, kBarkW, seed, 5.0f);
  const float top = 2, bottom = H - 8.0f;
  for (int t = 0; t < tiers; t++) {
    const float f = (float)t / (tiers - 1);
    const float ty0 = top + (bottom - top) * f * 0.80f, ty1 = ty0 + (bottom - top) * (0.28f + 0.05f * f);
    const float hw = 2.5f + f * (W * 0.5f - 3.5f);
    for (int y = (int)ty0; y <= (int)ty1; y++) {
      const float u = (y - ty0) / std::max(1.0f, ty1 - ty0);
      const float half = hw * (0.15f + 0.85f * u);
      for (int x = (int)(cx - half - 1); x <= (int)(cx + half + 1); x++) {
        const float rel = (x + 0.5f - cx) / std::max(1.0f, half);
        if (std::fabs(rel) > 1.0f) continue;
        const float jag = (float)(hash3(x, t, seed) % 3);
        if (y > ty1 - jag - std::fabs(rel) * 1.5f) continue;
        if (sparse && hash3(x, y, seed + 7) % 5 == 0 && std::fabs(rel) > 0.3f) continue;   // the larch's thin needles
        const float l = lightAt(rel * 0.8f, (u - 0.5f) * 0.9f) + (hashf(x, y, seed) - 0.5f) * 0.3f;
        int k = lightIndex(l, x, y, 0.15f);
        if (((x + y * 2) % 5) == 0 && k > 0) k--;
        if (y > ty1 - jag - std::fabs(rel) * 1.5f - 1.5f) k = std::max(0, k - 1);
        if (snow && u < (t == 0 ? 0.12f : 0.26f) + 0.25f * hashf(x / 2, t, seed) - std::fabs(rel) * 0.15f) { c.set(x, y, kSnow[std::clamp(k + 1, 1, 4)]); continue; }
        c.set(x, y, P[k]);
      }
    }
  }
}

// a golden-green larch (taiga bog, alpine meadows); variants 4, 5 in their autumn gold
void larch(Canvas& c, int v, const Ctx& k) {
  const uint32_t seed = 6301u + (uint32_t)v * 37u;
  Ramp P = ramp5(rgba(58, 68, 36), rgba(100, 114, 44), rgba(148, 156, 58), rgba(192, 190, 82), rgba(230, 220, 128));
  if (v >= 4) P = ramp5(rgba(96, 58, 34), rgba(160, 104, 40), rgba(208, 152, 52), rgba(236, 196, 82), rgba(252, 232, 146));
  P = tweak(P, Ctx{}, v, 0.06f);
  conifer(c, P, seed, 4 + (v % 2), true, k.cold);
  finishSolid(c);
}

// a low wind-bent juniper (heath, chalk downs, sea cliffs, steppe)
void juniper(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 6401u + (uint32_t)v * 29u;
  const Ramp L = tweak(ramp5(rgba(22, 44, 50), rgba(36, 70, 64), rgba(58, 100, 78), rgba(92, 132, 96), rgba(142, 172, 128)), k, v);
  const float wind = (v & 1) ? 1.0f : -1.0f;   // the way the sea wind bent it
  const float cx = W * 0.5f - wind * 2.0f;
  capsule(c, V(cx, H - 2.0f), V(cx + wind * 3.0f, H - 9.0f), 2.2f, 1.6f, kBarkW, 0, 0);
  capsule(c, V(cx + wind * 3.0f, H - 9.0f), V(cx + wind * 8.0f, H - 13.0f), 1.6f, 1.0f, kBarkW, 0, 0);
  std::vector<Blob> bl;
  const int n = 6 + (v % 3);
  for (int i = 0; i < n; i++) {
    const float t = (float)i / (n - 1);
    bl.push_back({cx + wind * (t * 11.0f - 2.0f) + (hashf(i, 1, seed) - 0.5f) * 3.0f, H - 13.0f - (1 - std::fabs(t - 0.4f)) * 5.0f + hashf(i, 2, seed) * 3.0f,
                  4.0f + hashf(i, 3, seed) * 2.0f});
  }
  crown(c, bl, L, seed, 0.7f, 0.8f);
  for (int i = 0; i < 6; i++) {   // blue berries
    const int x = (int)(hash3(i, 4, seed) % (uint32_t)W), y = (int)(hash3(i, 5, seed) % (uint32_t)H);
    if (solid(c, x, y) && solid(c, x, y + 1)) { c.set(x, y, rgba(96, 110, 170)); c.set(x, y - 1, rgba(170, 186, 226)); }
  }
  if (k.cold) snowOn(c, 2, seed + 5);
  finishSolid(c);
}

// =====================================================================================================================
// rocks and shrubs
// =====================================================================================================================
// a banded sandstone pillar with a cap rock (badlands)
void hoodoo(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 6501u + (uint32_t)v * 41u;
  static const Ramp bands[3] = {
      ramp5(rgba(96, 46, 40), rgba(150, 76, 52), rgba(194, 114, 72), rgba(222, 156, 104), rgba(242, 204, 150)),
      ramp5(rgba(110, 66, 50), rgba(166, 108, 72), rgba(208, 152, 104), rgba(232, 192, 140), rgba(248, 226, 184)),
      ramp5(rgba(86, 40, 40), rgba(134, 64, 50), rgba(176, 94, 64), rgba(206, 132, 90), rgba(232, 178, 132))};
  const Ramp Cap = ramp5(rgba(60, 46, 50), rgba(94, 76, 72), rgba(130, 108, 96), rgba(164, 142, 122), rgba(196, 178, 152));
  const float cx = W * 0.5f + ((int)(v % 3) - 1) * 0.5f;
  const int capTop = (int)(H * (0.04f + 0.14f * hashf(v, 1, seed))), capBot = capTop + 4 + (v % 2);
  const float lean = ((int)(v % 5) - 2) * 0.9f;
  for (int y = capBot; y < H - 1; y++) {
    const float t = (float)(y - capBot) / (H - 1 - capBot);
    // knobbly segments where the soft beds weathered back between harder ones, swelling to a broad scree foot
    const float knob = vnoise(0, y / 4.5f, seed) - 0.5f, knob2 = vnoise(3.0f, y / 2.0f, seed + 9) - 0.5f;
    const float hw = 3.6f + knob * 3.2f + knob2 * 0.8f + t * t * 6.0f;
    const float mx = cx + lean * (1 - t) + (vnoise(1.0f, y / 7.0f, seed + 4) - 0.5f) * 1.6f;
    const int band = (y + (int)(vnoise(0, y / 5.0f, seed + 1) * 3)) / 3;
    const Ramp& R = bands[hash3(band, 2, seed) % 3];
    for (int x = (int)std::floor(mx - hw); x <= (int)std::ceil(mx + hw) - 1; x++) {
      const float u = (x + 0.5f - mx) / std::max(1.0f, hw);
      int kk = lightIndex(lightAt(u * 0.92f, -0.05f) + (hashf(x, y / 2, seed) - 0.5f) * 0.2f, x, y, 0.05f);
      if ((x + band) % 4 == 0 && kk > 0 && hashf(x, band, seed + 3) < 0.4f) kk--;   // rain flutes
      c.set(x, y, R[kk]);
    }
  }
  // the cap rock: a harder slab, wider than the neck, its top lit
  // the cap rock: an irregular harder slab, a little wider than the neck below it, its top lit
  const float capW = 4.5f + (v % 3), capX = cx + lean + (hashf(v, 2, seed) - 0.5f) * 2.0f;
  for (int y = capTop; y <= capBot; y++)
    for (int x = (int)std::floor(capX - capW - 1); x <= (int)std::ceil(capX + capW + 1); x++) {
      const float u = (x + 0.5f - capX) / capW;
      const float rim = 1.0f + (hashf(x, 3, seed) - 0.5f) * 0.4f - (y == capTop ? 0.25f : 0.0f) - (y == capBot ? 0.1f : 0.0f);
      if (std::fabs(u) > rim) continue;
      int kk = y <= capTop + 1 ? (u < 0.2f ? 4 : 3) : lightIndex(lightAt(u * 0.9f, 0.2f), x, y, 0.05f);
      if (y == capBot) kk = std::max(0, kk - 2);
      c.set(x, y, Cap[kk]);
    }
  // rubble at its foot
  for (int i = 0; i < 4; i++) {
    const float x = cx + (i < 2 ? -1.0f : 1.0f) * (6.0f + hashf(i, 4, seed) * 4.0f), y = H - 2.5f;
    ball(c, x, y, 1.6f + hashf(i, 5, seed), 1.2f, bands[i % 3], 0.0f);
  }
  if (k.cold) snowOn(c, 2, seed + 7);
  finishSolid(c);
}

// a cluster of tall glowing crystals (crystal barrens; a light source); frame f: the glow's pulse
void crystalSpire(Canvas& c, int v, int f, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 6601u + (uint32_t)v * 47u;
  static const Ramp cols[2] = {
      ramp5(rgba(54, 38, 110), rgba(96, 70, 176), rgba(146, 116, 226), rgba(196, 172, 246), rgba(244, 236, 255)),
      ramp5(rgba(30, 60, 112), rgba(48, 110, 176), rgba(84, 168, 222), rgba(150, 220, 244), rgba(230, 252, 255))};
  const Ramp& R = cols[v & 1];
  const float pulse = 0.5f + 0.5f * std::sin(f * (TAU / 4.0f));
  struct Cr { float x, ang, len, w; };
  std::vector<Cr> cr;
  const int n = 4 + (v % 2);
  for (int i = 0; i < n; i++) {
    const float t = (float)i / (n - 1) - 0.5f;
    cr.push_back({W * 0.5f + t * 12.0f, -PI * 0.5f + t * 0.9f + (hashf(i, 1, seed) - 0.5f) * 0.25f, (i == n / 2 ? H * 0.82f : H * (0.38f + 0.3f * hashf(i, 2, seed))), 3.0f + hashf(i, 3, seed) * 1.6f});
  }
  // the shorter (back) crystals first
  std::sort(cr.begin(), cr.end(), [](const Cr& a, const Cr& b) { return a.len < b.len; });
  for (const Cr& q : cr) {
    const Vec2 base = V(q.x, H - 3.0f), dir = V(std::cos(q.ang), std::sin(q.ang)), nrm = V(-dir.y, dir.x);
    const Vec2 tip = base + dir * q.len, sh = base + dir * (q.len - q.w * 1.6f);
    // two faces: the left one turned to the light, the right one in shade, a lit ridge between them
    std::vector<Vec2> lf = {base - nrm * q.w, sh - nrm * q.w, tip, sh, base};
    std::vector<Vec2> rf = {base, sh, tip, sh + nrm * q.w, base + nrm * q.w};
    const bool leftLit = nrm.x < 0;   // (nrm points right of the crystal's axis)
    poly(c, lf, R[leftLit ? 3 : 1]);
    poly(c, rf, R[leftLit ? 1 : 3]);
    line(c, (int)base.x, (int)base.y, (int)sh.x, (int)sh.y, R[4]);
    line(c, (int)sh.x, (int)sh.y, (int)tip.x, (int)tip.y, R[4]);
    // the inner glow, pulsing up the core
    const int gl = (int)(q.len * (0.35f + 0.35f * pulse));
    for (int j = 2; j < gl; j++) {
      const Vec2 p = base + dir * (float)j - nrm * (q.w * 0.35f);
      if (hash3(j, (int)q.x, seed) % 3) c.set((int)p.x, (int)p.y, mix(R[2], kWhite, 0.25f + 0.45f * pulse));
    }
  }
  // the rock it grows from
  ball(c, W * 0.5f - 4.0f, H - 2.5f, 4.0f, 2.0f, kStone, 0.0f);
  ball(c, W * 0.5f + 5.0f, H - 2.0f, 3.0f, 1.6f, kStone, 0.0f);
  // a glint or two that moves with the frame
  for (int i = 0; i < 3; i++) {
    const int x = (int)(hash3(i + f * 3, 4, seed) % (uint32_t)W), y = (int)(hash3(i + f * 3, 5, seed) % (uint32_t)H);
    if (solid(c, x, y)) { c.set(x, y, rgba(255, 255, 255)); c.set(x + 1, y, R[4]); c.set(x, y - 1, R[4]); }
  }
  if (k.cold) snowOn(c, 1, seed + 5);
  finishSolid(c, 0.85f);
}

// a knot of hexagonal basalt columns (ash fields, sea cliffs)
void basalt(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 6701u + (uint32_t)v * 31u;
  const Ramp R = ramp5(rgba(22, 22, 30), rgba(40, 40, 50), rgba(62, 62, 72), rgba(92, 92, 100), rgba(128, 128, 132));
  struct Col { float x, y, r; int h; };
  std::vector<Col> cols;
  // a stepped knot: the tall columns at the back, shorter ones crowding in front and to the sides
  const int n = 6 + (v % 2);
  for (int i = 0; i < n; i++) {
    const float gx = (float)(i % 3) - 1.0f, gy = (float)(i / 3);
    const float x = W * 0.5f + gx * 7.0f + (gy == 1 ? 3.5f : 0.0f) + (hashf(i, 1, seed) - 0.5f) * 2.0f;
    const float y = H - 9.0f + gy * 4.0f + (hashf(i, 2, seed) - 0.5f) * 1.5f;
    cols.push_back({x, y, 3.3f + hashf(i, 3, seed) * 0.6f, (int)(H * (gy < 1 ? 0.45f + 0.35f * hashf(i, 4, seed) : 0.15f + 0.25f * hashf(i, 4, seed)))});
  }
  std::sort(cols.begin(), cols.end(), [](const Col& a, const Col& b) { return a.y < b.y; });   // the back ones first
  for (const Col& q : cols) {
    const float ry = q.r * 0.55f;
    const int ytop = (int)(q.y - q.h);
    // the body: the left faces turned to the light, the right ones in shade, a seam at each hexagon edge
    for (int y = ytop; y <= (int)q.y; y++)
      for (int x = (int)std::floor(q.x - q.r); x <= (int)std::ceil(q.x + q.r) - 1; x++) {
        const float u = (x + 0.5f - q.x) / q.r;
        int kk = u < -0.5f ? 3 : u < 0.0f ? 2 : u < 0.5f ? 1 : 0;
        if (std::fabs(std::fabs(u) - 0.5f) < 0.12f || std::fabs(u) < 0.06f) kk = std::max(0, kk - 1);
        if (hash3(x, y / 3, seed) % 9 == 0) kk = std::max(0, kk - 1);
        c.set(x, y, R[kk]);
      }
    // the hexagonal top, lit, its near edge catching the light
    std::vector<Vec2> hex;
    for (int j = 0; j < 6; j++) { const float a = j * (TAU / 6.0f); hex.push_back(V(q.x + std::cos(a) * q.r, ytop + std::sin(a) * ry)); }
    poly(c, hex, R[3]);
    for (int j = 3; j < 6; j++) { const float a = j * (TAU / 6.0f); c.set((int)(q.x + std::cos(a) * q.r * 0.8f), (int)(ytop + std::sin(a) * ry * 0.8f), R[4]); }
  }
  if (k.cold) snowOn(c, 2, seed + 7);
  finishSolid(c);
}

// a blue-white ice block, its lit face cracked (glacier)
void iceSerac(Canvas& c, int v, const Ctx& k) {
  (void)k;
  const int W = c.w, H = c.h;
  const uint32_t seed = 6801u + (uint32_t)v * 23u;
  const Ramp R = ramp5(rgba(62, 96, 156), rgba(104, 150, 204), rgba(156, 198, 232), rgba(206, 232, 248), rgba(246, 252, 255));
  // an angular block: a tall rock of few, flat facets
  rock(c, W * 0.5f, H * 0.52f, W * (0.36f + 0.04f * (v % 2)), H * (0.46f - 0.04f * (v % 2)), R, seed, 5);
  // cracks down the lit face, blue depths in them
  for (int i = 0; i < 3; i++) {
    int x = (int)(W * 0.3f + hashf(i, 1, seed) * W * 0.3f), y = (int)(H * 0.2f + hashf(i, 2, seed) * H * 0.2f);
    for (int j = 0; j < 8 + (int)(hash3(i, 3, seed) % 6); j++) {
      if (solid(c, x, y)) { c.set(x, y, R[0]); if (solid(c, x - 1, y)) c.set(x - 1, y, R[4]); }
      y++;
      if (hash3(i, j, seed) % 3 == 0) x += (hash3(j, i, seed) & 1) ? 1 : -1;
    }
  }
  snowOn(c, 2, seed + 4);
  for (int i = 0; i < 4; i++) { const int x = (int)(hash3(i, 5, seed) % (uint32_t)W), y = (int)(hash3(i, 6, seed) % (uint32_t)H); if (solid(c, x, y)) c.set(x, y, rgba(255, 255, 255)); }
  finishSolid(c);
}

// a tall ridged earth mound (savanna)
void termiteMound(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 6901u + (uint32_t)v * 19u;
  const Ramp R = tweak(ramp5(rgba(86, 52, 40), rgba(130, 84, 56), rgba(170, 118, 76), rgba(204, 154, 104), rgba(230, 192, 140)), Ctx{}, v, 0.06f);
  auto spire = [&](float cx, int top, float bw) {
    for (int y = top; y < H - 1; y++) {
      const float t = (float)(y - top) / (H - 1 - top);
      const float hw = bw * (0.25f + 0.75f * std::sqrt(t)) + (vnoise(0, y / 2.5f, seed + (int)cx) - 0.5f) * 1.6f;
      for (int x = (int)std::floor(cx - hw); x <= (int)std::ceil(cx + hw) - 1; x++) {
        const float u = (x + 0.5f - cx) / std::max(0.8f, hw);
        int kk = lightIndex(lightAt(u * 0.9f, -0.15f) + (vnoise(x / 1.3f, y / 5.0f, seed) - 0.5f) * 0.45f, x, y, 0.05f);   // ridges down its sides
        c.set(x, y, R[kk]);
      }
    }
  };
  const float cx = W * 0.5f;
  if (v % 2) spire(cx + 4.5f, (int)(H * 0.42f), 3.6f);
  spire(cx - 0.5f, (int)(H * (0.04f + 0.1f * (v % 3))), 5.4f + (v % 2));
  if (v >= 2) spire(cx - 5.0f, (int)(H * 0.52f), 3.2f);
  // the lumps where the termites built on, and the scree of crumbs round the foot
  for (int i = 0; i < 5; i++) {
    const float x = cx + (hashf(i, 1, seed) - 0.5f) * 8.0f, y = H * 0.35f + hashf(i, 2, seed) * H * 0.5f;
    if (solid(c, (int)x, (int)y)) ball(c, x, y, 1.6f, 1.3f, R, 0.0f);
  }
  for (int i = 0; i < 4; i++) ball(c, cx + (i < 2 ? -1.0f : 1.0f) * (6.0f + hashf(i, 3, seed) * 2.0f), H - 2.5f, 1.3f, 1.0f, R, 0.0f);
  if (k.cold) snowOn(c, 1, seed + 5);
  finishSolid(c);
}

// a smoking fissure ringed with sulphur (ash fields; a light source); frame f: the glow's pulse
void ashVent(Canvas& c, int v, int f, const Ctx& k) {
  (void)k;
  const int W = c.w, H = c.h;
  const uint32_t seed = 7001u + (uint32_t)v * 17u;
  const Ramp Rk = ramp5(rgba(20, 18, 22), rgba(36, 32, 36), rgba(56, 50, 52), rgba(80, 72, 70), rgba(110, 100, 92));
  const Ramp Su = ramp5(rgba(110, 96, 30), rgba(170, 150, 40), rgba(220, 200, 70), rgba(244, 230, 120), rgba(255, 250, 190));
  const float pulse = 0.5f + 0.5f * std::sin(f * (TAU / 4.0f) + v);
  const float cx = W * 0.5f, cy = H * 0.62f;
  // the ring of black rock lumps round the vent, the back ones first
  for (int i = 0; i < 9; i++) {
    const float a = PI + i * (TAU / 9.0f) + hashf(i, 1, seed) * 0.3f;
    const float x = cx + std::cos(a) * 8.0f, y = cy + std::sin(a) * 4.0f;
    if (std::sin(a) < 0) ball(c, x, y, 3.0f + hashf(i, 2, seed), 2.2f, Rk, 0.05f);
  }
  // the vent: sulphur crust, the dark throat, the glow deep in it
  ellipse(c, cx, cy, 6.5f, 3.2f, Su[2]);
  ellipse(c, cx + 0.5f, cy + 0.3f, 4.8f, 2.3f, Rk[0]);
  const uint32_t g0 = mix(rgba(160, 50, 30), rgba(255, 150, 50), pulse), g1 = mix(rgba(230, 110, 40), rgba(255, 230, 140), pulse);
  ellipse(c, cx + 1.0f, cy + 0.6f, 3.0f, 1.3f, g0);
  ellipse(c, cx + 1.0f, cy + 0.6f, 1.6f, 0.7f, g1);
  for (int x = (int)(cx - 6); x <= (int)(cx + 6); x++) if (hash3(x, 3, seed) % 3 == 0) c.set(x, (int)(cy - 3), Su[3]);   // the crust's lit rim
  for (int i = 0; i < 9; i++) {
    const float a = i * (TAU / 9.0f) + hashf(i, 1, seed) * 0.3f;
    const float x = cx + std::cos(a) * 8.0f, y = cy + std::sin(a) * 4.0f;
    if (std::sin(a) >= 0) ball(c, x, y, 3.0f + hashf(i, 2, seed), 2.2f, Rk, 0.05f);
  }
  // embers rising out of it, a different few each frame
  for (int i = 0; i < 2; i++) {
    const int x = (int)cx + (int)(hash3(i, f, seed) % 5) - 2, y = (int)cy - 3 - (int)(hash3(i, f + 9, seed) % 6);
    c.set(x, y, g1);
  }
  finishSolid(c, 0.85f);
}

// a thorny gorse bush, yellow flowers (heath, chalk downs)
void gorse(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 7101u + (uint32_t)v * 13u;
  const Ramp L = tweak(ramp5(rgba(24, 46, 34), rgba(42, 74, 40), rgba(66, 104, 46), rgba(100, 136, 58), rgba(146, 170, 80)), k, v);
  const float cx = W * 0.5f, by = H - 2.0f;
  std::vector<Blob> bl = {{cx - 4, by - 5, 4.5f}, {cx + 4, by - 5, 4.5f}, {cx, by - 8, 5.0f}, {cx - 1, by - 3, 5.0f}};
  if (v & 1) bl.push_back({cx + 6, by - 3, 3.2f});
  crown(c, bl, L, seed, 0.85f, 0.9f);
  // spines poking out of the silhouette
  const Canvas s = c;
  for (int y = 1; y < H - 2; y++)
    for (int x = 1; x < W - 1; x++)
      if (!solid(s, x, y) && (solid(s, x + 1, y) || solid(s, x, y + 1)) && hash3(x, y, seed) % 3 == 0) c.set(x, y, L[1]);
  // yellow flowers, most on the lit side
  const Ramp Y = ramp5(rgba(150, 100, 30), rgba(210, 160, 40), rgba(244, 206, 60), rgba(252, 232, 110), rgba(255, 250, 190));
  for (int i = 0; i < 14; i++) {
    const int x = (int)(hash3(i, 1, seed) % (uint32_t)W), y = (int)(hash3(i, 2, seed) % (uint32_t)H);
    if (!solid(s, x, y) || (x > cx + 3 && i % 2)) continue;
    c.set(x, y, Y[x < cx ? 3 : 2]);
    if (i % 3 == 0) c.set(x + 1, y, Y[1]);
  }
  if (k.cold) frostTips(c, seed + 3);
  finishSolid(c, 0.85f);
}

// a grey thorn scrub (scrubland, steppe, badlands)
void thornbush(Canvas& c, int v, const Ctx& k) {
  const int W = c.w, H = c.h;
  const uint32_t seed = 7201u + (uint32_t)v * 11u;
  const Ramp B = ramp5(rgba(54, 46, 48), rgba(86, 76, 72), rgba(118, 106, 96), rgba(150, 138, 122), rgba(182, 172, 152));
  const Ramp L = tweak(ramp5(rgba(56, 62, 50), rgba(84, 92, 66), rgba(114, 122, 84), rgba(146, 152, 106), rgba(184, 186, 140)), k, v);
  const Vec2 base = V(W * 0.5f, H - 2.0f);
  // a tangle of branches fanning up and out, the back ones in shade
  for (int i = 0; i < 9; i++) {
    const float a = -PI * 0.5f + (i - 4) * 0.30f + (hashf(i, 1, seed) - 0.5f) * 0.25f;
    const float L0 = 7.0f + hashf(i, 2, seed) * 4.0f;
    const Vec2 e = base + V(std::cos(a) * L0 * 1.15f, std::sin(a) * L0);
    capsule(c, base, e, 0.9f, 0.5f, B, (i & 1) ? -1 : 0, 0);
    const Vec2 m = base + (e - base) * 0.6f;
    const Vec2 tw = m + V(std::cos(a + 0.8f) * 3.0f, std::sin(a + 0.8f) * 3.0f);
    line(c, (int)m.x, (int)m.y, (int)tw.x, (int)tw.y, B[2]);
    // thorns and a few tiny grey-green leaves
    for (int j = 0; j < 3; j++) {
      const Vec2 q = base + (e - base) * (0.35f + 0.25f * j);
      c.set((int)q.x + ((i + j) & 1 ? 1 : -1), (int)q.y, B[1]);
      if (hash3(i, j, seed) % 2) { c.set((int)q.x, (int)q.y - 1, L[3]); c.set((int)q.x + 1, (int)q.y - 1, L[2]); }
    }
  }
  if (k.cold) frostTips(c, seed + 3);
  finishSolid(c, 0.8f);
}

// =====================================================================================================================
// ground covers (walked over)
// =====================================================================================================================
// small rounded cushions (heather, saltbush): blobs lit on their upper-left
void cushions(Canvas& c, const Ramp& R, uint32_t seed, int n, float rmin, float rmax) {
  std::vector<Blob> bl;
  for (int i = 0; i < n; i++)
    bl.push_back({2.5f + hashf(i, 1, seed) * (c.w - 5.0f), c.h - 2.5f - hashf(i, 2, seed) * 3.0f - rmax * 0.5f, rmin + hashf(i, 3, seed) * (rmax - rmin)});
  crown(c, bl, R, seed, 0.8f, 0.7f);
}

// (M4, owner carry-over: the heath's cover read as flat lilac loaves on stalks) a heather tussock: a low, ragged mound
// of wiry twigs, a skyline of fine spikes rather than a smooth dome, lit on its top-left and dark under its bottom-right;
// purple-brown with the bloom in flecks on the lit crown, old brown growth at the flanks, the twigs fraying into the
// ground at its foot (dithered, no hard edge). Variants differ in size, lean, how much is in bloom and how brown it is.
void heather(Canvas& c, int v, const Ctx& k) {
  const uint32_t seed = 7301u + (uint32_t)v * 7u;
  const int W = c.w, H = c.h;
  const float bloom = 0.35f + 0.13f * (float)(v % 4);   // how much of it is in flower
  const float brown = (v % 3) * 0.12f;                  // the older, browner plants
  const Ramp P = tweak(ramp5(rgba(40, 28, 38), rgba(62, 42, 54), rgba(86, 60, 74), rgba(112, 80, 96), rgba(140, 104, 122)), k, v, 0.05f);
  const Ramp B = ramp5(rgba(46, 36, 30), rgba(70, 54, 40), rgba(96, 76, 52), rgba(124, 100, 68), rgba(150, 126, 88));
  const Ramp F = ramp5(rgba(110, 64, 108), rgba(140, 86, 136), rgba(168, 110, 160), rgba(194, 140, 186), rgba(220, 176, 214));
  // the mound's height at each column: a broad hump (one or two lobes) with a ragged twiggy skyline
  const float cx = W * 0.5f + ((int)(hash3(v, 1, seed) % 3) - 1) * 1.0f;
  const float hw = W * (0.36f + 0.04f * (v % 3));
  const bool twin = (v % 2) == 1;
  std::vector<int> top((size_t)W, H);
  for (int x = 0; x < W; x++) {
    const float u = (x + 0.5f - cx) / hw;
    float hgt = 1.0f - u * u;
    if (twin) hgt = std::max(hgt * 0.8f, 0.85f - (u + 0.45f) * (u + 0.45f) * 2.2f);
    if (hgt <= 0) continue;
    const float base = std::sqrt(hgt) * (H - 4) * 0.92f;
    const int jag = (int)(hash3(x, 3, seed) % 3);   // spikes of the twigs on the skyline
    top[(size_t)x] = (H - 2) - (int)base + (x % 2 ? jag : jag / 2);
  }
  for (int x = 0; x < W; x++) {
    const int t = top[(size_t)x];
    if (t >= H - 1) continue;
    for (int y = std::max(0, t); y < H - 1; y++) {
      // the surface normal from the skyline's slope and the depth into the mound
      const int tl = x > 0 ? top[(size_t)x - 1] : t + 2, tr = x + 1 < W ? top[(size_t)x + 1] : t + 2;
      const float sx = (float)(tr - tl) * 0.18f;            // the slope across: + faces left (the light)
      const float d = (float)(y - t) / std::max(1, H - 1 - t);   // 0 the crown .. 1 the foot
      float l = 0.55f + sx * 0.8f - d * 0.95f + (cx - x) / W * 0.6f;
      l += (hashf(x, y, seed) - 0.5f) * 0.35f;              // the twigs' fine broken texture
      int ki = lightIndex(l, x, y, 0.18f);
      if ((x + y * 3 + (int)(hash3(x, y / 2, seed) % 3)) % 4 == 0) ki = std::max(0, ki - 1);   // dark twig lines
      uint32_t col = P[ki];
      if (hashf(x, y, seed + 5) < brown + d * 0.35f) col = B[std::min(4, ki + 1)];   // old brown growth low down
      if (d < 0.45f && ki >= 2 && hashf(x, y, seed + 9) < bloom * (1.0f - d)) col = F[std::min(4, ki + (hashf(x, y, seed + 11) < 0.3f ? 1 : 0))];
      c.set(x, y, col);
    }
    // a twig or flower spike standing above the skyline here and there
    if (t > 1 && hash3(x, 7, seed) % 4 == 0) { c.set(x, t - 1, hashf(x, 0, seed + 13) < bloom ? F[3] : P[2]); if (hash3(x, 8, seed) % 2) c.set(x, t - 2, P[1]); }
  }
  // the foot frays into the ground: the lowest rows thinned by an ordered dither, darker toward the right (its shadow)
  for (int y = H - 4; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!chA(c.get(x, y))) continue;
      if (bayer(x, y) < (y - (H - 4)) * 0.28f) { c.set(x, y, 0); continue; }
      if (x > cx) c.set(x, y, darken(c.get(x, y), 0.18f));
    }
  if (k.cold) frostTips(c, seed + 3);
  // a soft outline on the top and the left only (the light's side keeps its crisp twiggy skyline)
  const Canvas s = c;
  for (int y = 1; y < H - 3; y++)
    for (int x = 1; x < W - 1; x++)
      if (!solid(s, x, y) && solid(s, x + 1, y + 1) && solid(s, x, y + 1) && hash3(x, y, seed + 17) % 2 == 0) c.set(x, y, rgba(40, 26, 36, 150));
}

// (M4, owner carry-over: the alpine meadow's flora repeated: every clump was the same confetti of six colours) a
// wildflower patch is mostly ONE kind in one colour, as they grow: daisy heads, tall spikes (lupin, gentian), a low
// cushion starred with tiny blooms (moss campion), or a loose mixed clump in two colours. The kind is v % 4, its
// colour v / 4 from the land's own flowers (the high meadows: gentian blue, edelweiss white, alpine pink, globeflower
// yellow; the lowland meadows: white, yellow, poppy red, cornflower blue, pink, orange). Lit from the top-left.
void wildflowers(Canvas& c, int v, const Ctx& k) {
  const uint32_t seed = 7401u + (uint32_t)v * 7u;
  const int W = c.w, H = c.h;
  const bool alpine = k.eco == (int)Eco::AlpineMeadow || k.cold;
  static const uint32_t lowland[6] = {rgba(250, 248, 236), rgba(250, 214, 70), rgba(228, 70, 60), rgba(96, 120, 236), rgba(236, 130, 190), rgba(250, 160, 60)};
  static const uint32_t high[4] = {rgba(70, 96, 226), rgba(246, 246, 236), rgba(228, 118, 176), rgba(246, 210, 64)};
  const int kind = v % 4, ci = v / 4;
  const uint32_t main = alpine ? high[(ci + v / 2) % 4] : lowland[(ci * 2 + v) % 6];
  const uint32_t second = alpine ? high[(ci + 2) % 4] : lowland[(ci * 2 + v + 3) % 6];
  const Ramp pr = ramp(main, 0.8f), sr = ramp(second, 0.8f);
  auto head = [&](int x, int y, const Ramp& R, bool big) {   // a bloom: lit petals up-left, shade down-right, an eye
    c.set(x, y - 1, R[4]); c.set(x - 1, y, R[3]); c.set(x + 1, y, R[2]); c.set(x, y + 1, R[1]);
    if (big) { c.set(x - 1, y - 1, R[3]); c.set(x + 1, y + 1, R[1]); }
    c.set(x, y, R[0] == R[4] ? rgba(250, 226, 110) : (main == rgba(250, 214, 70) || main == rgba(246, 210, 64) ? rgba(200, 120, 40) : rgba(250, 226, 110)));
  };
  switch (kind) {
    case 0: {   // daisies: a loose spray of heads on short stems
      for (int i = 0; i < 6; i++) blade(c, 2.0f + i * 2.3f, H - 2, 2 + (int)(hash3(i, 0, seed) % 3), (hashf(i, 1, seed) - 0.5f) * 2.0f, kLeaf, 1);
      const int n = 4 + (int)(hash3(v, 5, seed) % 3);
      for (int i = 0; i < n; i++) {
        const int x = 2 + (int)(hash3(i, 2, seed) % (uint32_t)(W - 4)), y = H - 5 - (int)(hash3(i, 3, seed) % 5u);
        vline(c, x, y + 1, H - 2, kLeaf[1 + (i & 1)]);
        head(x, y, pr, i == 0);
      }
      break;
    }
    case 1: {   // spikes: two or three tall racemes of small florets, darker toward their foot
      for (int i = 0; i < 5; i++) blade(c, 3.0f + i * 2.4f, H - 2, 3 + (int)(hash3(i, 0, seed) % 3), (hashf(i, 1, seed) - 0.5f) * 2.5f, kLeaf, 1);
      const int n = 2 + (int)(hash3(v, 6, seed) % 2);
      for (int i = 0; i < n; i++) {
        const int x = 3 + i * 4 + (int)(hash3(i, 7, seed) % 3u), top = 1 + (int)(hash3(i, 8, seed) % 3u);
        vline(c, x, top + 2, H - 2, kLeaf[1]);
        for (int y = top; y < H - 5; y++) {
          const int kk = std::clamp(4 - (y - top) * 4 / std::max(1, H - 6 - top), 1, 4);
          c.set(x, y, pr[kk]);
          if ((y + i) % 2 == 0) { c.set(x - 1, y, pr[std::max(1, kk - 1)]); }
          else c.set(x + 1, y, pr[std::max(0, kk - 2)]);
        }
      }
      break;
    }
    case 2: {   // a cushion: a low dome of fine leaves starred with tiny blooms
      ball(c, W * 0.5f, H - 4.0f, W * 0.36f, 3.2f, kMoss, 0.12f);
      for (int i = 0; i < 11; i++) {
        const int x = 3 + (int)(hash3(i, 2, seed) % (uint32_t)(W - 6)), y = H - 7 + (int)(hash3(i, 3, seed) % 4u);
        if (!solid(c, x, y)) continue;
        c.set(x, y, x < W / 2 ? pr[4] : pr[3]);
        if (hash3(i, 9, seed) % 3 == 0) c.set(x + 1, y, pr[2]);
      }
      break;
    }
    default: {   // a mixed clump in two colours, taller grass among it
      for (int i = 0; i < 7; i++) blade(c, 1.5f + i * 2.0f, H - 2, 3 + (int)(hash3(i, 0, seed) % 4), (hashf(i, 1, seed) - 0.5f) * 3.0f, kLeaf, 1);
      const int n = 5 + (v % 3);
      for (int i = 0; i < n; i++) {
        const int x = 2 + (int)(hash3(i, 2, seed) % (uint32_t)(W - 4)), y = 3 + (int)(hash3(i, 3, seed) % (uint32_t)(H - 7));
        vline(c, x, y + 1, H - 2, kLeaf[1 + (i & 1)]);
        head(x, y, (i % 3) ? pr : sr, false);
      }
      break;
    }
  }
  if (k.cold) frostTips(c, seed + 3);
  finishCover(c);
}

void prairieGrass(Canvas& c, int v, const Ctx& k) {
  const uint32_t seed = 7501u + (uint32_t)v * 7u;
  const Ramp R = tweak(ramp5(rgba(96, 78, 42), rgba(148, 120, 54), rgba(194, 166, 74), rgba(224, 202, 106), rgba(246, 230, 152)), k, v, 0.06f);
  const float wind = 2.5f + (v % 3);   // they all lean the way the wind blows (east)
  const int n = 8 + (v % 3);
  for (int i = 0; i < n; i++) {
    const float x = 1.0f + i * (c.w - 3.0f) / n + (hashf(i, 1, seed) - 0.5f);
    const float mid = 1.0f - std::fabs((i + 0.5f) / n - 0.5f) * 1.1f;
    const int h = std::max(5, (int)((c.h - 4) * (0.55f + 0.45f * mid) + (hashf(i, 2, seed) - 0.5f) * 4.0f));
    blade(c, x, c.h - 2, h, wind + (hashf(i, 3, seed) - 0.5f) * 2.0f, R, (i % 3 == 0) ? 0 : 1);
    if (i % 2 == 0) {   // a seed head
      const int tx = (int)std::floor(x + wind), ty = c.h - 2 - h;
      c.set(tx, ty, R[4]); c.set(tx, ty + 1, R[3]); c.set(tx + 1, ty + 1, R[2]);
    }
  }
  if (k.cold) frostTips(c, seed + 3);
  finishCover(c);
}

void cottonGrass(Canvas& c, int v, const Ctx& k) {
  const uint32_t seed = 7601u + (uint32_t)v * 7u;
  const Ramp R = tweak(ramp5(rgba(50, 64, 46), rgba(80, 98, 58), rgba(112, 130, 72), rgba(148, 160, 94), rgba(188, 194, 128)), k, v, 0.06f);
  const int n = 5 + (v % 3);
  for (int i = 0; i < n; i++) {
    // in a loose tussock: the stems fan out from the middle, of all heights
    const float x = c.w * 0.5f + (hashf(i, 2, seed) - 0.5f) * (c.w - 5.0f);
    const int h = 4 + (int)(hash3(i, 0, seed) % 8);
    const float lean = (x - c.w * 0.5f) * 0.35f + (hashf(i, 1, seed) - 0.5f) * 2.5f;
    blade(c, x, c.h - 2, h, lean, R, 1);
    if (hash3(i, 3, seed) % 3 != 0) {   // the cotton tuft: a soft white ball, lit top-left
      const float tx = x + lean, ty = (float)(c.h - 2 - h);
      ellipse(c, tx, ty, 1.7f, 1.5f, kSnow[2]);
      c.set((int)std::floor(tx - 0.5f), (int)std::floor(ty - 0.5f), kSnow[4]);
      c.set((int)std::floor(tx + 0.8f), (int)std::floor(ty + 0.8f), kSnow[1]);
    }
  }
  finishCover(c, 0.6f);
}

void agave(Canvas& c, int v, const Ctx& k) {
  (void)k;
  const uint32_t seed = 7701u + (uint32_t)v * 7u;
  const Ramp R = tweak(ramp5(rgba(34, 58, 64), rgba(56, 94, 92), rgba(88, 134, 120), rgba(128, 172, 150), rgba(182, 210, 186)), Ctx{}, v, 0.06f);
  const Vec2 base = V(c.w * 0.5f, c.h - 3.0f);
  // thick pointed leaves radiating from the heart: the back ones first, the near ones over them, lit from the top-left
  const int n = 9;
  int order[9];
  for (int i = 0; i < n; i++) order[i] = i;
  std::sort(order, order + n, [&](int a, int b) { return std::sin(PI + a * (PI / (n - 1))) < std::sin(PI + b * (PI / (n - 1))); });
  for (int oi = 0; oi < n; oi++) {
    const int i = order[oi];
    const float a = PI + 0.15f + i * ((PI - 0.3f) / (n - 1)) + (hashf(i, 1, seed) - 0.5f) * 0.2f;
    const float L = 6.0f + (1.0f - std::fabs(i - (n - 1) * 0.5f) / n) * 3.5f + hashf(i, 2, seed) * 1.5f;
    const Vec2 e = base + V(std::cos(a) * L * 1.1f, std::sin(a) * L * 0.9f);
    capsule(c, base, e, 1.6f, 0.3f, R, std::cos(a) < -0.3f ? 1 : (std::cos(a) > 0.3f ? -1 : 0), 0.0f);
    c.set((int)e.x, (int)e.y, rgba(190, 120, 90));   // the red spine at its tip
  }
  if (v == 3) {   // a flower stalk
    vline(c, (int)base.x, 0, (int)base.y - 2, R[2]);
    c.set((int)base.x, 0, rgba(250, 210, 80)); c.set((int)base.x + 1, 1, rgba(230, 170, 60));
  }
  finishCover(c, 0.75f, 2);
}

void dryBrush(Canvas& c, int v, const Ctx& k) {
  (void)k;
  const uint32_t seed = 7801u + (uint32_t)v * 7u;
  const Ramp R = ramp5(rgba(84, 64, 44), rgba(126, 100, 64), rgba(166, 136, 88), rgba(200, 172, 118), rgba(228, 206, 156));
  const float cx = c.w * 0.5f, cy = c.h * 0.52f, rx = 6.5f + (v % 2), ry = 4.4f;
  // a loose ball of dry twigs (a tumbleweed caught on the ground): crossing strokes, lit on top
  for (int i = 0; i < 16; i++) {
    const float a = hashf(i, 1, seed) * TAU, b = a + 1.6f + hashf(i, 2, seed) * 1.6f;
    const Vec2 p0 = V(cx + std::cos(a) * rx, cy + std::sin(a) * ry), p1 = V(cx + std::cos(b) * rx, cy + std::sin(b) * ry);
    const int kk = (p0.y + p1.y) * 0.5f < cy ? 3 : ((p0.y + p1.y) * 0.5f > cy + 1 ? 1 : 2);
    line(c, (int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, R[kk]);
  }
  for (int x = (int)(cx - rx + 1); x <= (int)(cx + rx - 1); x++) if (hash3(x, 4, seed) % 2) c.set(x, (int)(cy + ry), R[0]);
  finishCover(c, 0.6f, 2);
}

void saltbush(Canvas& c, int v, const Ctx& k) {
  const uint32_t seed = 7901u + (uint32_t)v * 7u;
  const Ramp R = tweak(ramp5(rgba(60, 72, 84), rgba(96, 112, 120), rgba(136, 152, 156), rgba(176, 190, 190), rgba(214, 224, 222)), k, v, 0.05f);
  cushions(c, R, seed, 4 + (v % 2), 3.2f, 4.6f);
  finishCover(c, 0.75f, 2);
}

void lichen(Canvas& c, int v, const Ctx& k) {
  (void)k;
  const uint32_t seed = 8001u + (uint32_t)v * 7u;
  // (M3c fixer round 2, review: "tundra lichen rock is one sprite repeated dozens of times ... orange and olive blocks
  // look like camouflage") six different things of the tundra floor: a low stone, a pair of small stones, a flat slab,
  // a crowberry cushion with no stone at all, a split boulder top and a scatter of pebbles. Each wears ONE lichen
  // (sulphur yellow, rust orange or sage) in a few round crusts on its lit top, not a patchwork over all of it.
  static const uint32_t lc[3][2] = {{rgba(196, 188, 90), rgba(156, 150, 70)}, {rgba(206, 128, 70), rgba(162, 96, 56)}, {rgba(160, 172, 116), rgba(124, 136, 90)}};
  const uint32_t* L = lc[(v / 2 + v) % 3];
  const int kind = v % 6;
  const bool flip = (v / 6) & 1;
  const float mid = c.w * 0.5f;
  if (kind == 0) rock(c, mid + 1.5f, c.h * 0.56f, c.w * 0.36f, c.h * 0.44f, kStone, seed, 5);
  else if (kind == 1) { rock(c, mid - 3.0f, c.h * 0.60f, c.w * 0.24f, c.h * 0.36f, kStone, seed, 5); rock(c, mid + 4.0f, c.h * 0.70f, c.w * 0.16f, c.h * 0.26f, kStone, seed + 1, 4); }
  else if (kind == 2) rock(c, mid, c.h * 0.66f, c.w * 0.44f, c.h * 0.30f, kStone, seed, 4);
  else if (kind == 4) { rock(c, mid - 2.0f, c.h * 0.58f, c.w * 0.26f, c.h * 0.42f, kStone, seed, 5); rock(c, mid + 3.0f, c.h * 0.58f, c.w * 0.22f, c.h * 0.40f, kStone, seed + 3, 5); }
  else if (kind == 5) for (int i = 0; i < 4; i++) rock(c, 3.0f + hashf(i, 5, seed) * (c.w - 6.0f), c.h - 2.5f - hashf(i, 6, seed) * 2.0f, 2.0f + hashf(i, 7, seed) * 1.2f, 1.8f, kStone, seed + i, 3);
  if (kind != 3) {
    // the lichen crusts: a few round patches on the stone's upper part, darker at their rims
    for (int i = 0; i < 3; i++) {
      const float px = 3.0f + hashf(i, 8, seed) * (c.w - 6.0f), py = 1.5f + hashf(i, 9, seed) * (c.h * 0.45f), r = 1.2f + hashf(i, 10, seed) * 1.4f;
      for (int y = (int)(py - r - 1); y <= (int)(py + r + 1); y++)
        for (int x = (int)(px - r - 1); x <= (int)(px + r + 1); x++) {
          const float d = std::hypot(x + 0.5f - px, (y + 0.5f - py) * 1.3f);
          if (d < r && solid(c, x, y)) c.set(x, y, d < r - 0.8f ? L[0] : L[1]);
        }
    }
  }
  // crowberry and moss cushions round its foot (the whole of the cushion variant)
  const int cushions = kind == 3 ? 5 : 3;
  for (int i = 0; i < cushions; i++) {
    const float x = 2.0f + hashf(i, 1, seed) * (c.w - 4.0f), y = kind == 3 ? c.h - 2.5f - hashf(i, 2, seed) * 3.0f : c.h - 2.5f;
    if (!solid(c, (int)x, (int)y)) ball(c, x, y, kind == 3 ? 2.8f : 2.0f, kind == 3 ? 2.0f : 1.4f, kMoss, 0.0f);
  }
  if (kind == 3)
    for (int i = 0; i < 4; i++) c.set(3 + (int)(hash3(i, 11, seed) % (uint32_t)(c.w - 6)), c.h - 4 - (int)(hash3(i, 12, seed) % 3u), rgba(40, 34, 56));   // crowberries
  if (flip) {   // mirrored half the time, so no two read alike
    Canvas t = c;
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) c.set(x, y, t.get(c.w - 1 - x, y));
  }
  finishCover(c, 0.8f, 2);
}

void wrack(Canvas& c, int v, const Ctx& k) {
  (void)k;
  const uint32_t seed = 8101u + (uint32_t)v * 7u;
  const Ramp Kp = ramp5(rgba(30, 34, 24), rgba(52, 58, 34), rgba(78, 84, 46), rgba(110, 110, 62), rgba(146, 140, 90));
  const Ramp Dw = ramp5(rgba(92, 84, 80), rgba(136, 126, 114), rgba(176, 166, 150), rgba(206, 198, 182), rgba(232, 226, 212));
  // a bleached stick of driftwood and strands of kelp tangled along the tideline
  if (v != 2) capsule(c, V(1.5f, c.h - 4.0f), V(c.w - 2.5f, c.h - 6.5f - (v & 1) * 1.5f), 1.6f, 1.2f, Dw, 0, 0);
  for (int i = 0; i < 7; i++) {
    Vec2 p = V(1.0f + hashf(i, 1, seed) * (c.w - 6.0f), c.h - 2.0f - hashf(i, 2, seed) * 4.0f);
    for (int j = 0; j < 7; j++) {
      const Vec2 q = p + V(1.0f + hashf(i, j, seed) * 1.2f, (hashf(j, i, seed) - 0.5f) * 2.0f);
      line(c, (int)p.x, (int)p.y, (int)q.x, (int)q.y, Kp[1 + (j % 3)]);
      line(c, (int)p.x, (int)p.y + 1, (int)q.x, (int)q.y + 1, Kp[j % 2]);
      p = q;
    }
  }
  for (int i = 0; i < 3; i++) c.set((int)(hash3(i, 3, seed) % (uint32_t)c.w), c.h - 3, rgba(120, 104, 54));   // bladders
  finishCover(c, 0.7f, 2);
}

void shells(Canvas& c, int v, const Ctx& k) {
  (void)k;
  const uint32_t seed = 8201u + (uint32_t)v * 7u;
  static const Ramp sh[3] = {
      ramp5(rgba(150, 120, 112), rgba(196, 166, 152), rgba(232, 208, 192), rgba(246, 232, 220), rgba(255, 248, 240)),
      ramp5(rgba(150, 80, 80), rgba(206, 120, 116), rgba(236, 160, 150), rgba(248, 198, 186), rgba(255, 230, 220)),
      ramp5(rgba(110, 96, 120), rgba(160, 146, 168), rgba(204, 192, 208), rgba(230, 222, 232), rgba(250, 246, 250))};
  const int n = 4 + (v % 3);
  for (int i = 0; i < n; i++) {
    const float x = 3.0f + hashf(i, 1, seed) * (c.w - 6.0f), y = c.h - 2.5f - hashf(i, 2, seed) * 4.5f;
    const Ramp& R = sh[hash3(i, 3, seed) % 3];
    if (i % 2) {   // a scallop: a fan with ribs
      for (int a = 0; a < 6; a++) {
        const float an = PI + 0.35f + a * 0.48f;
        line(c, (int)x, (int)y, (int)(x + std::cos(an) * 3.2f), (int)(y + std::sin(an) * 2.4f), R[a < 2 ? 4 : (a < 4 ? 3 : 2)]);
      }
      c.set((int)x, (int)y, R[1]);
    } else {   // a whelk: a little spiral cone
      ball(c, x, y, 2.3f, 1.6f, R, 0.0f);
      c.set((int)x, (int)y, R[0]);
      c.set((int)x + 2, (int)y, R[2]);
    }
  }
  if (v == 1) { capsule(c, V(4.0f, c.h - 3.0f), V(7.0f, c.h - 6.0f), 0.8f, 0.5f, sh[1], 0, 0); capsule(c, V(7.0f, c.h - 6.0f), V(9.0f, c.h - 5.0f), 0.6f, 0.4f, sh[1], 0, 0); }   // a coral twig
  finishCover(c, 0.6f, 1);
}

void glowCaps(Canvas& c, int v, const Ctx& k) {
  (void)k;
  const uint32_t seed = 8301u + (uint32_t)v * 7u;
  const Ramp Cp = ramp5(rgba(26, 66, 86), rgba(38, 116, 136), rgba(66, 178, 188), rgba(130, 228, 220), rgba(214, 255, 246));
  const Ramp St = ramp5(rgba(90, 90, 104), rgba(140, 140, 150), rgba(190, 190, 192), rgba(220, 220, 216), rgba(244, 244, 236));
  // a fairy ring of small glowing caps, the far ones smaller
  const int n = 5 + (v % 2);
  std::vector<std::pair<float, float>> pos;
  for (int i = 0; i < n; i++) {
    const float a = i * (TAU / n) + hashf(i, 1, seed) * 0.5f;
    pos.push_back({c.w * 0.5f + std::cos(a) * 5.5f, c.h - 5.0f + std::sin(a) * 3.0f});
  }
  std::sort(pos.begin(), pos.end(), [](const std::pair<float, float>& a, const std::pair<float, float>& b) { return a.second < b.second; });
  for (auto& p : pos) {
    const float r = 1.8f + (p.second - (c.h - 8.0f)) * 0.25f;
    vline(c, (int)p.first, (int)(p.second - r), (int)p.second + 1, St[2]);
    ball(c, p.first, p.second - r - 0.5f, r + 0.6f, r * 0.7f, Cp, 0.0f);
    c.set((int)p.first, (int)(p.second - r), Cp[4]);
  }
  finishCover(c, 0.6f, 1);
}

void blightweed(Canvas& c, int v, const Ctx& k) {
  (void)k;
  const uint32_t seed = 8401u + (uint32_t)v * 7u;
  const Ramp R = ramp5(rgba(14, 10, 18), rgba(32, 22, 36), rgba(56, 38, 58), rgba(84, 58, 82), rgba(116, 84, 110));
  // black thorny tendrils curling up out of the dead ground, their tips a sickly purple
  for (int i = 0; i < 5; i++) {
    Vec2 p = V(3.0f + i * (c.w - 6.0f) / 4.0f, c.h - 2.0f);
    float a = -PI * 0.5f + (hashf(i, 1, seed) - 0.5f) * 0.5f;
    const float curl = (i & 1 ? 1.0f : -1.0f) * (0.07f + hashf(i, 2, seed) * 0.08f);
    const int len = 9 + (int)(hash3(i, 3, seed) % 4);
    for (int j = 0; j < len; j++) {
      const Vec2 q = p + V(std::cos(a), std::sin(a));
      c.set((int)q.x, (int)q.y, R[j < len - 2 ? 2 + (j & 1) : 4]);
      if (j < len / 2) c.set((int)q.x + 1, (int)q.y, R[1]);   // thicker at the root
      if (j % 3 == 1) c.set((int)q.x + (curl > 0 ? -1 : 1), (int)q.y, R[1]);   // a thorn
      p = q;
      a += curl;
    }
    c.set((int)p.x, (int)p.y, rgba(160, 70, 130));
  }
  finishCover(c, 0.8f);
}

// arching fronds with leaflets (the classic fern's way), in ramp R, n fronds over a spread
void fronds(Canvas& c, const Ramp& R, uint32_t seed, int n, float L0, float spread) {
  const Vec2 base = V(c.w * 0.5f, c.h - 2.0f);
  for (int oi = 0; oi < n; oi++) {
    const int f = (oi % 2) ? n - 1 - oi / 2 : oi / 2;   // the outer fronds first, the middle ones on top
    const float a = PI + (PI - spread) * 0.5f + f * spread / (n - 1);
    const float L = L0 + (f == n / 2 ? 1.5f : 0.0f) + hashf(f, 0, seed);
    const int bias = std::abs(f - n / 2) <= 1 ? 1 : (f == 0 || f == n - 1 ? -1 : 0);
    Vec2 prev = base;
    for (int s = 1; s <= 10; s++) {
      const float t = s / 10.0f;
      const Vec2 p = base + V(std::cos(a) * L * t, std::sin(a) * L * t + t * t * 5.0f);
      line(c, (int)prev.x, (int)prev.y, (int)p.x, (int)p.y, R[std::clamp(1 + bias, 0, 4)]);
      if (s >= 2 && s <= 9 && (s & 1) == 0) {
        const float pa = a + PI * 0.5f, ll = 2.2f * (1.0f - t * 0.5f);
        c.set((int)(p.x + std::cos(pa) * ll), (int)(p.y + std::sin(pa) * ll), R[std::clamp(3 + bias, 0, 4)]);
        c.set((int)(p.x - std::cos(pa) * ll), (int)(p.y - std::sin(pa) * ll), R[std::clamp(2 + bias, 0, 4)]);
      }
      prev = p;
    }
  }
}

void silverFern(Canvas& c, int v, const Ctx& k) {
  const uint32_t seed = 8501u + (uint32_t)v * 7u;
  const Ramp R = tweak(ramp5(rgba(56, 80, 86), rgba(90, 120, 120), rgba(130, 160, 154), rgba(172, 198, 190), rgba(214, 232, 224)), k, v, 0.05f);
  fronds(c, R, seed, 7, 7.5f + (v % 2), PI - 0.5f);
  for (int i = 0; i < 3; i++) { const int x = (int)(hash3(i, 1, seed) % (uint32_t)c.w), y = (int)(hash3(i, 2, seed) % (uint32_t)c.h); if (solid(c, x, y)) c.set(x, y, rgba(255, 255, 250)); }
  finishCover(c, 0.7f);
}

void jungleFern(Canvas& c, int v, const Ctx& k) {
  const uint32_t seed = 8601u + (uint32_t)v * 7u;
  const Ramp R = tweak(ramp5(rgba(14, 46, 36), rgba(24, 80, 46), rgba(38, 116, 52), rgba(68, 152, 60), rgba(120, 192, 80)), k, v, 0.05f);
  // big broad leaves: each a pointed ellipse with a lit upper half and a pale midrib, the back ones first
  const Vec2 base = V(c.w * 0.5f, c.h - 2.0f);
  const int n = 5;
  for (int oi = 0; oi < n; oi++) {
    const int i = (oi % 2) ? n - 1 - oi / 2 : oi / 2;
    const float a = PI + 0.35f + i * ((PI - 0.7f) / (n - 1)) + (hashf(i, 1, seed) - 0.5f) * 0.2f;
    const float L = 8.0f + hashf(i, 2, seed) * 2.0f + (i == n / 2 ? 1.5f : 0.0f);
    const Vec2 dir = V(std::cos(a), std::sin(a) * 0.8f), nrm = V(-dir.y, dir.x);
    for (float t = 0.15f; t <= 1.0f; t += 0.04f) {
      const float wdt = std::sin(t * PI) * 2.6f;
      const Vec2 m = base + dir * (L * t) + V(0, t * t * 3.0f);
      for (float s = -wdt; s <= wdt; s += 0.5f) {
        const Vec2 q = m + nrm * s;
        const bool up = (nrm * s).y < 0;
        c.set((int)std::floor(q.x), (int)std::floor(q.y), R[std::clamp((up ? 3 : 1) + (std::fabs(s) > wdt - 0.6f ? -1 : 0), 0, 4)]);
      }
      c.set((int)std::floor(m.x), (int)std::floor(m.y), R[4]);
    }
  }
  if (k.cold) frostTips(c, seed + 3);
  finishCover(c, 0.75f);
}

void petals(Canvas& c, int v, const Ctx& k) {
  (void)k;
  const uint32_t seed = 8701u + (uint32_t)v * 7u;
  const Ramp P = v == 3 ? ramp5(rgba(150, 140, 156), rgba(206, 196, 210), rgba(236, 230, 238), rgba(248, 244, 248), rgba(255, 255, 255))
                        : ramp5(rgba(150, 80, 110), rgba(204, 120, 150), rgba(236, 168, 190), rgba(248, 204, 218), rgba(255, 234, 240));
  // fallen petals scattered on the ground, drifted into a loose heap: each a 2-px petal, lit on its upper-left
  const int n = 20 + (v % 3) * 4;
  for (int i = 0; i < n; i++) {
    const float a = hashf(i, 1, seed) * TAU, r = std::sqrt(hashf(i, 2, seed));
    const int x = (int)(c.w * 0.5f + std::cos(a) * r * 7.5f), y = (int)(c.h * 0.55f + std::sin(a) * r * 4.0f);
    c.set(x, y, P[3 + (i & 1)]);
    c.set(x + 1, y, P[2]);
    if (i % 3 == 0) c.set(x, y + 1, P[1]);
  }
  finishCover(c, 0.45f, 1);
}

// ---- one cell of a prop in variant v, frame f, as eco grows it
void paintCell(Canvas& c, Prop p, int v, int f, int eco) {
  const Ctx k = ctxOf(eco);
  switch (p) {
    case Prop::AcaciaTree: acacia(c, v, k); break;
    case Prop::BaobabTree: baobab(c, v, k); break;
    case Prop::GiantTree: giantTree(c, v, k); break;
    case Prop::GnarledTree: gnarled(c, v, k); break;
    case Prop::BlossomTree: blossom(c, v, k); break;
    case Prop::BambooClump: bamboo(c, v, k); break;
    case Prop::JungleTree: jungleTree(c, v, k); break;
    case Prop::GiantMushroom: giantMushroom(c, v, k); break;
    case Prop::SilverTree: silverTree(c, v, k); break;
    case Prop::MangroveTree: mangrove(c, v, k); break;
    case Prop::SwampCypress: swampCypress(c, v, k); break;
    case Prop::PetrifiedTree: petrified(c, v, k); break;
    case Prop::LarchTree: larch(c, v, k); break;
    case Prop::JuniperTree: juniper(c, v, k); break;
    case Prop::Hoodoo: hoodoo(c, v, k); break;
    case Prop::CrystalSpire: crystalSpire(c, v, f, k); break;
    case Prop::BasaltColumns: basalt(c, v, k); break;
    case Prop::IceSerac: iceSerac(c, v, k); break;
    case Prop::TermiteMound: termiteMound(c, v, k); break;
    case Prop::AshVent: ashVent(c, v, f, k); break;
    case Prop::Gorse: gorse(c, v, k); break;
    case Prop::Thornbush: thornbush(c, v, k); break;
    case Prop::Heather: heather(c, v, k); break;
    case Prop::Wildflowers: wildflowers(c, v, k); break;
    case Prop::PrairieGrass: prairieGrass(c, v, k); break;
    case Prop::CottonGrass: cottonGrass(c, v, k); break;
    case Prop::Agave: agave(c, v, k); break;
    case Prop::DryBrush: dryBrush(c, v, k); break;
    case Prop::Saltbush: saltbush(c, v, k); break;
    case Prop::Lichen: lichen(c, v, k); break;
    case Prop::Wrack: wrack(c, v, k); break;
    case Prop::Shells: shells(c, v, k); break;
    case Prop::GlowCaps: glowCaps(c, v, k); break;
    case Prop::Blightweed: blightweed(c, v, k); break;
    case Prop::SilverFern: silverFern(c, v, k); break;
    case Prop::JungleFern: jungleFern(c, v, k); break;
    case Prop::Petals: petals(c, v, k); break;
    default: break;
  }
}

}  // namespace

int floraPropW(Prop p) { return fi(p).w; }
int floraPropH(Prop p) { return fi(p).h; }
int floraPropFrames(Prop p) { return fi(p).frames; }
int floraVariants(Prop p) { return isWildlandsFlora(p) ? fi(p).variants : 1; }

void paintFloraProp(Canvas& c, Prop p, int frame) { paintCell(c, p, 0, frame, -1); }

Canvas floraVariant(Prop p, int v, int eco) {
  const int n = std::max(1, propFrames(p));
  const int w = floraPropW(p), h = floraPropH(p);
  Canvas sheet(w * n, h);
  const int vv = v % std::max(1, floraVariants(p));
  for (int f = 0; f < n; f++) {
    Canvas cell(w, h);
    paintCell(cell, p, vv, f, eco);
    place(sheet, cell, f, 0);
  }
  return sheet;
}

}  // namespace art
