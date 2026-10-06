// EMBERVALE art (M2 Wayfinder): mountains, the wayside places (rpg/world/poi.h vignettes) and the wonders: Peak ..
// DragonBones in art_props.h. Painted procedurally in the shared language of rpg/art: 3/4 top-down view, light from the
// top-left, the hue-shifted ramps of art_internal.h (shadows drift to blue-violet, lights to warm yellow), selective
// outlines, grass and scree anchoring every piece to the ground. The canvas sizes below are the art's own; the tile
// FOOTPRINTS are frozen in art::wildFootprint (the prop stands on the bottom-centre tile of its footprint).
#include <cstdlib>
#include "rpg/art/art_internal.h"

namespace art {

namespace {

struct WildInfo { uint8_t w, h, frames; };
// Peak, StandingStone, CaravanWreck, WatchtowerRuin, FishingShack, TollPost, HerbBed, GraveCairn, Bedroll, ElderTree,
// Colossus, StarShard, DragonBones, GreatPeak
const WildInfo kWild[] = {
    {64, 78, 1}, {18, 36, 1}, {56, 36, 1}, {52, 80, 1}, {52, 50, 1}, {40, 32, 1}, {18, 15, 1},
    {22, 24, 1}, {30, 16, 4}, {112, 128, 1}, {56, 112, 1}, {22, 32, 4}, {120, 62, 1}, {136, 140, 1},
};
constexpr int kWildN = (int)(sizeof(kWild) / sizeof(kWild[0]));
static_assert(kWildN == (int)Prop::COUNT - (int)Prop::Peak, "a size for every M2 prop");
inline const WildInfo& wi(Prop p) { return kWild[std::clamp((int)p - (int)Prop::Peak, 0, kWildN - 1)]; }

// ---- shared bits -----------------------------------------------------------------------------------------------
// grass blades at the foot of a thing (anchors it to the ground)
void tufts(Canvas& c, int x0, int x1, int y, uint32_t seed, float density = 0.5f, const Ramp& R = kLeaf) {
  for (int x = x0; x <= x1; x++) {
    if (hashf(x, y, seed) > density) continue;
    const int h = 1 + (int)(hash3(x, 1, seed) % 3);
    for (int j = 0; j < h; j++) c.set(x + ((j == h - 1 && (x & 1)) ? 1 : 0), y - j, R[j == h - 1 ? 3 : (j == 0 ? 1 : 2)]);
  }
}
// thins the bottom rows out into the ground with an ordered dither (no ink line where a thing grows out of the land)
void feather(Canvas& c, int rows, float edgeOnly = 0.0f) {
  const int b = c.h - 1;
  for (int y = b - rows + 1; y <= b; y++)
    for (int x = 0; x < c.w; x++) {
      if (edgeOnly > 0) {
        const float u = std::fabs((x + 0.5f - c.w * 0.5f) / (c.w * 0.5f));
        if (u < edgeOnly) continue;
      }
      const float fade = (float)(y - (b - rows)) / (float)rows;
      if (bayer(x, y) < fade * 0.9f) c.set(x, y, 0);
    }
}
// an ellipse of soft cast shadow painted INTO the sprite (for parts that shade other parts of the same prop)
void shadeIn(Canvas& c, float cx, float cy, float rx, float ry, float k) {
  for (int y = (int)std::floor(cy - ry); y <= (int)std::ceil(cy + ry); y++)
    for (int x = (int)std::floor(cx - rx); x <= (int)std::ceil(cx + rx); x++) {
      const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
      if (dx * dx + dy * dy > 1 || !solid(c, x, y)) continue;
      c.set(x, y, darken(c.get(x, y), k));
    }
}
struct Blob { float x, y, r; };
// a leaf crown of overlapping clumps, painted back to front, each lit on its top-left with a dark scalloped underside
void crown(Canvas& c, std::vector<Blob> blobs, const Ramp& R, uint32_t seed, float squash = 0.86f, float whole = 0.4f) {
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
        const float edge = 1.0f + 0.08f * std::sin(ang * 7 + ph) + 0.05f * std::sin(ang * 13 + ph2);
        if (d > edge) continue;
        const float nx = dx / edge, ny = dy / edge;
        float l = lightAt(nx * 0.8f, ny * 0.8f);
        l += ((gcx - (x + 0.5f)) / gw + (gcy - (y + 0.5f)) / gh) * whole;   // the whole crown's light
        l += (vnoise(x * 0.5f, y * 0.5f, seed) - 0.5f) * 0.55f;             // clumpy leaves
        int k = lightIndex(l, x, y, 0.12f);
        if (d > edge - 0.12f && ny > 0.2f) k = std::max(0, k - 1);
        c.set(x, y, R[k]);
      }
  }
  for (int y = 0; y < c.h; y++)
    for (int x = 0; x < c.w; x++) {
      const uint32_t p = c.get(x, y);
      if (p != R[3]) continue;
      const uint32_t h = hash3(x, y, seed + 99);
      if (h % 9 == 0) { c.set(x, y, R[4]); if (h % 2) c.set(x + 1, y, R[4]); }
      else if (h % 13 == 0 && c.get(x, y + 1) == R[3]) c.set(x, y + 1, R[2]);
    }
}
// lichen and moss speckles on stone (only where stone already is)
void lichen(Canvas& c, int x0, int y0, int x1, int y1, uint32_t seed, float amount, bool yellow = true) {
  const Ramp Y = ramp5(rgba(92, 96, 52), rgba(140, 140, 62), rgba(186, 178, 84), rgba(214, 206, 118), rgba(236, 230, 160));
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      if (!solid(c, x, y)) continue;
      const float n = vnoise(x / 2.6f, y / 2.6f, seed);
      if (n < 1.0f - amount) continue;
      const bool yel = yellow && vnoise(x / 6.0f, y / 6.0f, seed + 5) > 0.55f;
      const int k = n > 1.0f - amount * 0.45f ? 3 : 2;
      c.set(x, y, yel ? Y[k] : kMoss[k]);
    }
}

// ---- Peak ------------------------------------------------------------------------------------------------------
// A mountain seen in the 3/4 view: a jagged skyline, spurs running from the summit down toward the viewer (each lit on
// its west side and shaded on its east side, with a gully between), strata across the faces, a scree apron and grass
// at the foot. Eight shapes (height, lean, a twin summit, the number of spurs) in three lands: grey rock, snow-capped
// (snow on the upper faces, streaming down the gullies) and banded sandstone.
void peak(Canvas& c, int v, int land) {
  const int W = c.w, H = c.h, base = H - 4;
  const uint32_t seed = 7001u + (uint32_t)v * 131u;   // the same shape in every land (a range changes its rock, not its form)
  const Ramp Rg = ramp5(rgba(44, 40, 62), rgba(76, 72, 94), rgba(114, 108, 120), rgba(154, 146, 146), rgba(194, 186, 174));
  const Ramp Rc = ramp5(rgba(36, 38, 66), rgba(62, 68, 100), rgba(96, 104, 134), rgba(138, 146, 170), rgba(184, 192, 210));
  const Ramp Rs = ramp5(rgba(98, 50, 52), rgba(150, 84, 64), rgba(196, 124, 82), rgba(224, 164, 106), rgba(244, 206, 148));
  const Ramp& R = land == 2 ? Rs : (land == 1 ? Rc : Rg);
  // The mountain is a small heightfield on the ground plane (x east, z north/back), seen in the 3/4 view: a ground
  // point (x, z) at height Z shows at screen y = base - z * 0.5 - Z. Every column is drawn front to back with a
  // y-buffer, so near ridges hide the slopes behind them. Radial ridges (spurs) twist down from the summit; light comes
  // from the north-west, high: west slopes lit, east slopes in shade, strata along the contours, snow on the heights.
  struct Cone { float x, z, h, r, twist; int n; };
  // (M2 fixer round 2) the same painter at any canvas: a Peak (64 x 78) or a GreatPeak (the massif at a named summit
  // or a range's crest, about twice as big), its cones scaled to the canvas; the great one is a whole massif
  const float s = W / 64.0f, sv = H / 78.0f;
  const bool great = s > 1.5f;
  const float D = 44.0f * s;                     // ground depth of the footprint (screen: D / 2 px)
  const bool tall = (v & 1) == 0;
  const float hgt = (tall ? 50.0f : 36.0f) * sv * (great ? 1.08f : 1.0f);
  std::vector<Cone> cones;
  const float lean = ((v == 2 || v == 7) ? -5.0f : (v == 4 || v == 5) ? 5.0f : 0.0f) * s;
  const float r0 = (tall ? 25.0f : 26.0f) * s * (great ? 0.80f : 1.0f);
  cones.push_back({W * 0.5f + lean, r0 * 0.8f + 1.0f * s + (great ? 6.0f * s : 0.0f), hgt, r0, (v & 2) ? 0.6f : -0.5f, 4 + (v % 3) + (great ? 2 : 0)});
  if (great) {
    // shoulders and lesser summits either side, foothills in front: one mass of rock, not a cone
    const float side = (v & 4) ? 1.0f : -1.0f;
    cones.push_back({W * 0.5f + side * 19.0f * s, D * 0.52f, hgt * 0.74f, 15.0f * s, 0.4f, 5});
    cones.push_back({W * 0.5f - side * 17.0f * s, D * 0.46f, hgt * 0.60f, 14.0f * s, -0.3f, 4});
    cones.push_back({W * 0.5f + side * 25.0f * s, D * 0.28f, hgt * 0.38f, 12.0f * s, 0.2f, 4});
    cones.push_back({W * 0.5f - side * 24.0f * s, D * 0.24f, hgt * 0.30f, 11.0f * s, -0.2f, 3});
    cones.push_back({W * 0.5f + side * 4.0f * s, D * 0.18f, hgt * 0.26f, 13.0f * s, 0.3f, 4});
  } else {
    if (v == 3 || v == 6) cones.push_back({W * 0.5f + (v == 3 ? 12.0f : -12.0f) * s, D * 0.40f, hgt * 0.8f, 17.0f * s, 0.3f, 4});      // a twin summit
    else if (v != 7) cones.push_back({W * 0.5f + (lean > 0 ? -13.0f : 13.0f) * s, D * 0.34f, hgt * 0.48f, 15.0f * s, -0.4f, 3});     // a shoulder
    if (v == 1 || v == 5) cones.push_back({W * 0.5f - lean * 2, 12.5f * s, hgt * 0.36f, 14.0f * s, 0.2f, 3});                       // a foothill in front
  }
  // every cone stands wholly on the footprint (z >= 0): one reaching past its front edge was cut off flat there
  for (Cone& k : cones) k.z = std::max(k.z, k.r * 0.8f + 1.5f);
  auto Z = [&](float x, float z) -> float {
    float best = 0;
    for (const Cone& k : cones) {
      const float dx = (x - k.x) / k.r, dz = (z - k.z) / (k.r * 0.8f);
      const float r = std::sqrt(dx * dx + dz * dz);
      if (r >= 1) continue;
      const float th = std::atan2(dz, dx) + k.twist * r + (vnoise(x / (7.0f * s), z / (7.0f * s), seed + 3) - 0.5f) * 1.2f;
      const float spur = std::fabs(std::sin(th * k.n * 0.5f));   // 0 on a ridge .. 1 in a valley
      float h = k.h * std::pow(1 - r, 1.35f) * (1.0f - 0.22f * spur * std::min(1.0f, r * 3.0f));
      h += (vnoise(x / (3.0f * s), z / (3.0f * s), seed + 5) - 0.5f) * 2.4f * sv * (1 - r);   // crags
      best = std::max(best, h);
    }
    return best - 1.4f;   // (the skirt ends cleanly: no flat apron round the foot)
  };
  std::vector<int> ybuf(W, base + 1);
  std::vector<float> zbuf(W, -1);
  Canvas zc(W, H);   // per pixel: which ground depth it shows (for the snow and the strata), packed
  for (int x = 0; x < W; x++) {
    for (float z = 0; z <= D; z += great ? 0.4f : 0.25f) {
      const float h = Z(x + 0.5f, z);
      if (h <= 0.0f) continue;
      const int ys = (int)std::floor(base - z * 0.5f - h);
      if (ybuf[x] > base) ybuf[x] = ys + 1;   // the column's nearest point: only its own pixel (not down to the canvas foot)
      if (ys >= ybuf[x]) continue;
      // the normal from the field's slope
      const float e = 1.0f;
      const float gx = Z(x + 0.5f + e, z) - Z(x + 0.5f - e, z), gz = Z(x + 0.5f, z + e) - Z(x + 0.5f, z - e);
      float nx = -gx, ny = 2 * e, nz = -gz;
      const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
      nx /= nl; ny /= nl; nz /= nl;
      // light from the top-left, high and a little toward the viewer (as art_internal's lightAt): west slopes lit,
      // the front in mid tones, east slopes in shade
      const float lx = -0.58f, ly = 0.60f, lz = -0.36f;
      const float lam = nx * lx + ny * ly + nz * lz;
      for (int y = std::max(0, ys); y < ybuf[x]; y++) {
        const float l = lam + (vnoise(x / 2.0f, y / 2.0f, seed + 7) - 0.5f) * 0.10f + (bayer(x, y) - 0.5f) * 0.07f;
        int k = l > 0.80f ? 4 : l > 0.62f ? 3 : l > 0.40f ? 2 : l > 0.14f ? 1 : 0;
        // strata along the contours (bold bands in sandstone)
        const float cont = h + (vnoise(x / 8.0f, z / 8.0f, seed + 9) - 0.5f) * 3.0f;
        const float bandH = (land == 2 ? 4.5f : 6.0f) * sv;
        const float bf = cont / bandH - std::floor(cont / bandH);
        if (land == 2) { if (((int)std::floor(cont / bandH)) & 1) k = std::max(0, k - 1); if (bf > 0.82f && k < 4 && lam > 0.4f) k++; }
        else if (bf < 0.14f && hashf(x / 2, (int)(cont / bandH), seed + 11) < 0.75f && k > 0) k--;
        uint32_t col = R[k];
        // snow on the heights (land 1), thin dusting on a tall grey summit
        const float sn = (land == 1 ? hgt * 0.50f : land == 0 && tall ? hgt * 0.84f : 1e9f) + (vnoise(x / 3.0f, z / 3.0f, seed + 13) - 0.5f) * 7.0f - ny * 4.0f;
        if (h > sn) col = kSnow[std::clamp(k + 1, 1, 4)];
        else if (land == 1 && h > sn - 2.5f && hashf(x, y, seed + 15) < 0.45f) col = kSnow[std::clamp(k, 1, 3)];
        c.set(x, y, col);
        zc.set(x, y, rgba(std::clamp((int)(z * 4), 0, 255), std::clamp((int)(h * 4), 0, 255), 0));
      }
      ybuf[x] = std::max(0, ys);
    }
  }
  // the foot: scree and grass (or snow, or sand) creeping up over the lowest slopes
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!solid(c, x, y)) continue;
      const float h = ((zc.get(x, y) >> 8) & 255) / 4.0f;
      const float hf = hashf(x, y, seed + 17);
      if (h < 4.0f) {   // scree
        if (hf < 0.07f) c.set(x, y, R[3]);
        else if (hf < 0.12f) c.set(x, y, R[1]);
      }
      if (h < 1.0f + vnoise(x / 4.0f, y / 2.0f, seed + 19) * 2.2f && hf < 0.7f) {   // the land creeping up the foot
        if (land == 0 && hf < 0.5f) c.set(x, y, kLeaf[hf < 0.2f ? 3 : 2]);
      }
    }
  if (land == 0) {
    lichen(c, 0, base - 16, W - 1, base, seed + 23, 0.25f, false);
    tufts(c, 2, W - 3, base, seed + 25, 0.30f);
  }
}

// ---- StandingStone -----------------------------------------------------------------------------------------------
// A tall weathered menhir: a tapering slab, its west face lit and its thickness showing in shade on the east, the top
// rounded by weather, cracks, yellow and green lichen, grass round its foot.
void standingStone(Canvas& c) {
  const Ramp R = ramp5(rgba(46, 44, 62), rgba(80, 78, 92), rgba(120, 116, 120), rgba(158, 152, 146), rgba(196, 190, 176));
  const int W = c.w, H = c.h, by = H - 3;
  const float cx = W * 0.5f - 0.5f;
  for (int y = 2; y <= by; y++) {
    const float t = (y - 2.0f) / (by - 2.0f);
    const float hw = 4.2f + t * 2.3f + (vnoise(0, y / 4.0f, 41) - 0.5f) * 1.2f;
    const float lean = (1 - t) * 1.2f;
    // the rounded top
    const float topCut = t < 0.12f ? std::sqrt(std::max(0.0f, 1 - (0.12f - t) / 0.12f * (0.12f - t) / 0.12f)) : 1.0f;
    const float w = hw * topCut;
    for (int x = (int)std::floor(cx + lean - w - 2.2f); x <= (int)std::ceil(cx + lean + w); x++) {
      const float u = (x + 0.5f - (cx + lean)) / std::max(1.0f, w);
      if (u < -1.0f || u > 1.0f + 2.2f / std::max(1.0f, w)) continue;
      int k;
      if (u > 1.0f) k = 1;                                   // the side, in shade
      else {
        float l = lightAt(u * 0.75f, (t < 0.15f ? -0.6f : -0.1f)) + (vnoise(x / 2.0f, y / 3.0f, 43) - 0.5f) * 0.35f;
        k = lightIndex(l, x, y, 0.12f);
        if (u > 0.82f) k = std::min(k, 2);
      }
      c.set(x, y, R[k]);
    }
  }
  // cracks and weathering pits
  for (int y = 8; y < by - 2; y++) if (hashf(0, y, 45) < 0.3f) dotOn(c, (int)cx - 1 + (int)(hash3(1, y / 3, 45) % 3), y, R[0]);
  for (int i = 0; i < 6; i++) dotOn(c, (int)cx - 3 + (int)(hash3(i, 2, 47) % 6), 6 + (int)(hash3(i, 3, 47) % (by - 10)), R[1]);
  lichen(c, 0, by - 14, W - 1, by, 49, 0.38f, true);
  lichen(c, 0, 4, W - 1, 14, 51, 0.22f, true);
  outline(c, 0.95f);
  tufts(c, 1, W - 2, by + 1, 53, 0.55f);
}

// ---- CaravanWreck ------------------------------------------------------------------------------------------------
// An overturned covered wagon on its side: the bed's planks and axles toward the viewer, one wheel up in the air (spokes,
// iron tyre), the canvas torn from its hoops and trailing, a crate and a barrel spilled in the grass, a sack split open.
void wheel(Canvas& c, float cx, float cy, float rx, float ry, int spokes, float rot) {
  for (int y = (int)std::floor(cy - ry - 1); y <= (int)std::ceil(cy + ry + 1); y++)
    for (int x = (int)std::floor(cx - rx - 1); x <= (int)std::ceil(cx + rx + 1); x++) {
      const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, d = std::sqrt(dx * dx + dy * dy);
      if (d > 1.0f) continue;
      if (d > 0.80f) { c.set(x, y, d > 0.93f ? kIron[dx + dy < 0 ? 3 : 1] : kWood[dx + dy < 0 ? 3 : 2]); continue; }
      if (d < 0.22f) { c.set(x, y, kWoodDark[d < 0.12f ? 1 : 3]); continue; }
      const float a = std::atan2(dy, dx) + rot;
      if (std::fmod(a + 62.83185f, TAU / spokes) < 0.32f) c.set(x, y, kWood[dx < 0 ? 3 : 2]);
    }
}
void caravanWreck(Canvas& c) {
  const int W = c.w, H = c.h, by = H - 3;
  // the canvas cover torn off its hoops, lying on the ground to the east in heavy folds
  for (int y = by - 12; y <= by; y++)
    for (int x = 30; x < W - 2; x++) {
      const float u = (x - 30.0f) / (W - 32.0f), v = (by - y) / 12.0f;
      const float edge = (1.0f - u * 0.7f) * (0.75f + (vnoise(x / 4.0f, 3, 61) - 0.5f) * 0.5f);
      if (v > edge) continue;
      if (hashf(x / 2, y / 2, 63) < 0.05f) continue;   // a rip
      const float fold = std::sin(x * 0.75f + y * 0.2f);
      c.set(x, y, kCloth[lightIndex(0.35f + fold * 0.35f - v * 0.2f, x, y, 0.1f)]);
    }
  // the wagon box on its side: the floor's planks toward the viewer, the side board along the top
  const int bx0 = 4, bx1 = 34, byt = by - 17, byb = by - 2;
  for (int y = byt; y <= byb; y++)
    for (int x = bx0; x <= bx1; x++) {
      int k = 2;
      if (y <= byt + 2) k = 3;                    // the side board's top edge catches the light
      if (((y - byt) % 4) == 3) k = 1;            // the seams between the floor planks
      if (x == bx0) k = std::min(4, k + 1);
      if (x == bx1 || y == byb) k = 0;
      if (hashf(x / 6, (y - byt) / 4, 65) < 0.2f && k == 2) k = 1;
      c.set(x, y, kWood[k]);
    }
  // iron straps, the axles standing up out of the floor, and a split board
  for (int x : {bx0 + 5, bx1 - 5}) for (int y = byt; y <= byb; y++) c.set(x, y, kIron[y == byt ? 3 : 1]);
  line(c, 18, byt + 5, 23, byb - 2, kWoodDark[0]);
  // the wheels on the upper side, lying flat in the air: seen from above (flattened ellipses) on their stubs
  for (int k = 0; k < 2; k++) {
    const float wx = k == 0 ? bx0 + 6.0f : bx1 - 6.0f, wy = byt - 3.0f;
    vline(c, (int)wx, (int)wy, byt, kIron[1]);
    for (int y = (int)(wy - 4); y <= (int)(wy + 4); y++)
      for (int x = (int)(wx - 8); x <= (int)(wx + 8); x++) {
        const float dx = (x + 0.5f - wx) / 7.5f, dy = (y + 0.5f - wy) / 3.6f, d = std::sqrt(dx * dx + dy * dy);
        if (d > 1.0f) continue;
        if (d > 0.78f) { c.set(x, y, dy < 0 ? kIron[3] : kIron[1]); continue; }
        if (d < 0.2f) { c.set(x, y, kWoodDark[1]); continue; }
        const float a = std::atan2(dy, dx) + k * 0.4f;
        if (std::fmod(a + 62.83185f, TAU / 8) < 0.28f) c.set(x, y, kWood[dy < 0 ? 3 : 2]);
      }
  }
  // spilled goods: a stove-in crate, a barrel rolled away, a split sack of grain
  shadedBox(c, 36, by - 7, 43, by - 1, kWood, 2);
  line(c, 36, by - 7, 43, by - 1, kWoodDark[1]);
  for (int y = by - 5; y <= by; y++)
    for (int x = 45; x <= 52; x++) {
      const float u = (y - (by - 5)) / 5.0f;
      c.set(x, y, (x == 47 || x == 50) ? kIron[1] : kWood[u < 0.3f ? 3 : (u > 0.8f ? 1 : 2)]);
    }
  ellipse(c, 45, by - 2.5f, 1.5, 2.6, kWoodDark[2]);
  ball(c, 12, by + 0.5f, 5.0f, 2.4f, ramp(rgba(200, 176, 128), 0.7f), 0.1f);
  for (int i = 0; i < 9; i++) dot(c, 16 + (int)(hash3(i, 0, 67) % 9), by + (int)(hash3(i, 1, 67) % 2), kThatch[3]);
  outline(c, 0.95f);
  tufts(c, 1, W - 2, by + 1, 69, 0.45f);
}
// ---- WatchtowerRuin ----------------------------------------------------------------------------------------------
// A broken round watchtower: courses of dressed stone round a cylinder (lit west, shaded east), the top broken off in
// a jagged line with a few charred beams of the timber nest still jutting out, an arrow slit, the dark doorway, rubble
// and ivy at its foot.
void watchtower(Canvas& c) {
  const int W = c.w, H = c.h, by = H - 4;
  const float cx = W * 0.5f, r = 15.0f;
  const Ramp S = ramp5(rgba(54, 50, 66), rgba(90, 84, 96), rgba(132, 124, 124), rgba(172, 162, 150), rgba(206, 196, 178));
  // the broken top: a jagged line, higher on the west side
  auto topY = [&](int x) {
    const float u = (x + 0.5f - (cx - r)) / (2 * r);
    return 14.0f + u * 16.0f + (vnoise(x / 2.5f, 0, 71) - 0.5f) * 9.0f + ((x / 3) % 3 == 0 ? 2.0f : 0.0f);
  };
  for (int x = (int)(cx - r); x < (int)(cx + r); x++) {
    const float u = (x + 0.5f - cx) / r;   // -1..1 across the drum
    const float ty = topY(x);
    const float arc = std::sqrt(std::max(0.0f, 1 - u * u)) * 4.0f;   // the courses curve round the drum
    for (int y = (int)ty; y <= by; y++) {
      const float yy = y + arc;
      const int row = (int)std::floor(yy / 5.0f);
      const float within = yy - row * 5.0f;
      const int col = (int)std::floor((std::asin(std::clamp(u, -1.0f, 1.0f)) * 8.0f) + (row & 1) * 0.5f);
      float l = lightAt(u * 0.9f, -0.15f) + (hashf(col, row, 73) - 0.5f) * 0.35f;
      int k = lightIndex(l, x, y, 0.08f);
      if (within < 1.0f) k = std::max(0, k - 1);                                           // the mortar course
      const float cu = (std::asin(std::clamp(u, -1.0f, 1.0f)) * 8.0f) + (row & 1) * 0.5f;
      if (cu - std::floor(cu) < 0.13f) k = std::max(0, k - 1);                            // the joints
      if (y <= ty + 1.5f) k = std::min(4, k + 1);                                          // the broken edge catches light
      c.set(x, y, S[k]);
    }
  }
  // the inside of the broken drum seen over the far wall (dark), above the jagged near edge
  for (int x = (int)(cx - r) + 2; x < (int)(cx + r) - 2; x++) {
    const float u = (x + 0.5f - cx) / r;
    const float back = topY(x) - 3.0f - std::sqrt(std::max(0.0f, 1 - u * u)) * 2.0f;
    for (int y = (int)back; y < (int)topY(x); y++) if (!solid(c, x, y)) c.set(x, y, S[0]);
  }
  // charred beams of the old timber nest jutting out
  thickLine(c, cx - r - 4, 22, cx - 2, 18, 2.0f, kWoodDark[1]);
  thickLine(c, cx + 3, 24, cx + r + 6, 30, 2.0f, kWoodDark[0]);
  thickLine(c, cx - r - 4, 21, cx - 6, 18, 1.0f, kWoodDark[3]);
  for (int i = 0; i < 3; i++) box(c, (int)(cx + r) + 1 + i * 2, 30 + i, (int)(cx + r) + 2 + i * 2, 31 + i, kWoodDark[2]);
  // an arrow slit and the doorway
  for (int y = 40; y < 50; y++) { dot(c, (int)cx - 4, y, kInk); dot(c, (int)cx - 3, y, S[0]); }
  for (int y = by - 14; y <= by; y++)
    for (int x = (int)cx - 4; x <= (int)cx + 3; x++) {
      const float u = (x + 0.5f - (cx - 0.5f)) / 4.0f, v = (y - (by - 14)) / 4.0f;
      if (v < 1 && u * u + (1 - v) * (1 - v) > 1) continue;
      c.set(x, y, y > by - 2 ? kWoodDark[0] : kInk);
    }
  for (int y = by - 14; y <= by; y++) { dotOn(c, (int)cx - 5, y, S[3]); dotOn(c, (int)cx + 4, y, S[1]); }
  // ivy climbing the lit side
  for (int y = by - 30; y <= by; y++)
    for (int x = (int)(cx - r); x < (int)(cx - r) + 12; x++) {
      if (!solid(c, x, y)) continue;
      const float n = vnoise(x / 2.2f, y / 2.2f, 75) + (by - y) / 60.0f;
      if (n > 0.78f) continue;
      if (vnoise(x / 3.5f, y / 3.0f, 77) > 0.58f) c.set(x, y, kLeafDark[n < 0.5f ? 3 : 2]);
    }
  // rubble heaped at its foot
  rock(c, cx + r - 2, by - 1, 6, 4, S, 79, 5);
  rock(c, cx - r + 1, by, 5, 3, S, 81, 4);
  rock(c, cx + r + 6, by + 1, 3.5f, 2.5f, S, 83, 4);
  outline(c, 0.95f);
  tufts(c, 2, W - 3, by + 2, 85, 0.5f);
}

// ---- FishingShack ------------------------------------------------------------------------------------------------
// A plank shack on low stilts: walls of weathered vertical boards, a shingled roof with a deep eave overhang and real
// volume (lit west slope, shaded east), a door and a shuttered window, a net hung to dry on two poles, a barrel and a
// row of fish on a line.
void fishingShack(Canvas& c) {
  const int W = c.w, H = c.h, by = H - 3;
  const int wx0 = 9, wx1 = W - 14, wy0 = 22, wy1 = by - 6;
  // stilts and their shadow
  for (int x : {wx0 + 1, wx0 + 10, wx1 - 9, wx1 - 1}) { vline(c, x, wy1, by, kWoodDark[1]); vline(c, x + 1, wy1, by, kWoodDark[0]); }
  for (int x = wx0; x <= wx1; x++) c.set(x, wy1 + 1, kWoodDark[0]);
  // the walls: the front (south) face of vertical boards and the east gable end in shade
  for (int y = wy0; y <= wy1; y++)
    for (int x = wx0; x <= wx1 + 5; x++) {
      const bool side = x > wx1;
      if (side && y < wy0 + (x - wx1)) continue;
      const int board = side ? 0 : (x - wx0) / 3;
      int k = side ? 1 : ((x - wx0) % 3 == 0 ? 1 : 2);
      if (!side && hashf(board, 0, 91) < 0.3f) k = std::max(1, k - 1);
      if (!side && hashf(board, y / 5, 93) < 0.08f) k = 3;
      if (y == wy1) k = 0;
      c.set(x, y, kWood[k]);
    }
  // the door and a shuttered window
  box(c, wx0 + 13, wy1 - 12, wx0 + 19, wy1 - 1, kWoodDark[1]);
  vline(c, wx0 + 13, wy1 - 12, wy1 - 1, kWoodDark[3]);
  dot(c, wx0 + 18, wy1 - 6, kIron[3]);
  box(c, wx0 + 3, wy1 - 11, wx0 + 8, wy1 - 6, kWoodDark[0]);
  box(c, wx0 + 2, wy1 - 12, wx0 + 3, wy1 - 5, kWood[3]);
  // the roof: a gable with its ridge running east-west, shingles in rows, the eave overhanging the front wall
  const int ry0 = 6, ry1 = wy0 + 3;
  for (int y = ry0; y <= ry1; y++)
    for (int x = wx0 - 4; x <= wx1 + 4; x++) {
      const float t = (y - ry0) / (float)(ry1 - ry0);
      const int inset = (int)((1 - t) * 3);
      if (x < wx0 - 4 + inset || x > wx1 + 4 - inset) continue;
      const int row = (y - ry0) / 3;
      const bool edge = (y - ry0) % 3 == 0;
      const int sh = (x + (row & 1) * 2) / 4;
      int k = 2 + (t < 0.35f ? 1 : 0);
      if (edge) k = 1;
      if (hashf(sh, row, 95) < 0.2f) k = std::max(1, k - 1);
      if (x <= wx0 - 3 + inset) k = std::min(4, k + 1);
      if (y == ry1) k = 0;
      c.set(x, y, kThatch[k]);
    }
  hline(c, wx0 - 3, wx1 + 3, ry0, kThatch[4]);
  // the eave's shadow on the wall
  for (int x = wx0; x <= wx1; x++) { c.set(x, wy0 + 4, darken(c.get(x, wy0 + 4), 0.5f)); c.set(x, wy0 + 5, darken(c.get(x, wy0 + 5), 0.3f)); }
  // the gable end above the side wall, in shade
  for (int y = ry1 - 8; y <= ry1; y++)
    for (int x = wx1 + 5; x <= wx1 + 5 + (ry1 - y) / 3; x++) c.set(x, y, kWood[1]);
  // the net on its poles (west)
  vline(c, 2, 16, by, kWoodDark[2]);
  vline(c, 7, 14, by - 2, kWoodDark[2]);
  for (int y = 16; y <= 30; y++)
    for (int x = 2; x <= 8; x++) {
      const float sag = std::sin((x - 2) / 6.0f * 3.14159f) * 2.0f;
      if (y < 16 + sag) continue;
      if (((x + y) % 3 == 0) || ((x - y + 30) % 3 == 0)) c.set(x, y, rgba(170, 160, 130));
    }
  // a line of fish under the eave, and a barrel
  for (int i = 0; i < 4; i++) {
    const int fx = wx1 - 12 + i * 3;
    vline(c, fx, wy0 + 6, wy0 + 9, kIron[3]);
    dot(c, fx, wy0 + 10, kIron[2]);
  }
  for (int y = by - 7; y <= by; y++)
    for (int x = W - 9; x <= W - 3; x++) {
      const float u = (x - (W - 9)) / 6.0f;
      c.set(x, y, (y == by - 5 || y == by - 2) ? kIron[1] : kWood[u < 0.3f ? 3 : (u > 0.7f ? 1 : 2)]);
    }
  ellipse(c, W - 6, by - 7, 3.2f, 1.2f, kWoodDark[1]);
  outline(c, 0.95f);
  tufts(c, 1, W - 2, by + 1, 97, 0.4f);
}

// ---- TollPost ----------------------------------------------------------------------------------------------------
// A road toll post: a stout post with a striped boom pivoting on it (red and white, its counterweight a stone in a
// sling), an iron-bound toll box on a stand, a crude painted sign.
void tollPost(Canvas& c) {
  const int W = c.w, H = c.h, by = H - 3;
  const int px = W / 2;
  // the boom, reaching east from the pivot
  for (int x = px - 6; x < W - 1; x++) {
    const int band = (x - px + 60) / 4;
    const uint32_t col = (band & 1) ? rgba(206, 60, 52) : rgba(236, 228, 210);
    c.set(x, 11, (band & 1) ? rgba(232, 96, 76) : kWhite);
    c.set(x, 12, col);
    c.set(x, 13, darken(col, 0.45f));
  }
  // the counterweight on the short end
  ball(c, px - 7, 15, 2.5f, 2.5f, kStone, 0.1f);
  vline(c, px - 7, 12, 13, kWoodDark[1]);
  // the post
  for (int y = 9; y <= by; y++) { c.set(px - 1, y, kWood[3]); c.set(px, y, kWood[2]); c.set(px + 1, y, kWood[1]); }
  dot(c, px, 12, kIron[3]);
  // the toll box on its stand
  vline(c, px - 5, by - 6, by, kWoodDark[1]);
  shadedBox(c, px - 9, by - 12, px - 2, by - 7, kWoodDark, 3);
  hline(c, px - 9, px - 2, by - 10, kIron[2]);
  hline(c, px - 7, px - 4, by - 12, kInk);   // the slot
  // the sign: a rough board on the post with daubed letters
  shadedBox(c, px + 2, 17, px + 13, 23, kWood, 3);
  for (int x = px + 4; x <= px + 11; x += 2) { dot(c, x, 19, rgba(120, 30, 30)); dot(c, x, 21, rgba(120, 30, 30)); if (x % 4 == 0) dot(c, x + 1, 20, rgba(120, 30, 30)); }
  outline(c, 0.95f);
  tufts(c, px - 10, px + 6, by + 1, 101, 0.5f);
}

// ---- HerbBed -----------------------------------------------------------------------------------------------------
// A low timber-edged bed with three rows of different herbs: lavender, silver sage and marigold, earth between them.
void herbBed(Canvas& c) {
  const int W = c.w, H = c.h, by = H - 2;
  // the frame: the near board facing the viewer, the far one as a thin line
  for (int x = 1; x < W - 1; x++) { c.set(x, by, kWood[1]); c.set(x, by - 1, kWood[2]); c.set(x, by - 2, kWood[3]); c.set(x, 5, kWood[2]); }
  for (int y = 5; y <= by; y++) { c.set(1, y, kWood[3]); c.set(W - 2, y, kWood[1]); }
  for (int y = 6; y <= by - 3; y++) for (int x = 2; x < W - 2; x++) c.set(x, y, (x + y) % 3 ? rgba(92, 64, 48) : rgba(110, 78, 56));
  const Ramp Lav = ramp5(rgba(54, 40, 84), rgba(88, 62, 128), rgba(128, 96, 176), rgba(168, 140, 214), rgba(214, 196, 240));
  const Ramp Sage = ramp5(rgba(46, 66, 62), rgba(78, 104, 90), rgba(118, 142, 118), rgba(160, 180, 150), rgba(204, 214, 188));
  const Ramp Mar = ramp5(rgba(120, 50, 30), rgba(190, 92, 36), rgba(236, 146, 48), rgba(250, 196, 84), rgba(255, 236, 150));
  for (int r = 0; r < 3; r++) {
    const int y = 6 + r * 3;
    for (int x = 3; x < W - 3; x += 3) {
      const float jx = x + (hashf(x, r, 103) - 0.5f) * 1.2f;
      ball(c, jx + 0.5f, y + 1.0f, 1.7f, 1.6f, r == 0 ? Lav : (r == 1 ? Sage : kLeaf), 0.1f);
      if (r == 0) { dot(c, (int)jx, y - 1, Lav[4]); dot(c, (int)jx + 1, y - 2, Lav[3]); }
      if (r == 2) dot(c, (int)jx, y, Mar[3 + (x & 1)]);
    }
  }
  outline(c, 0.8f);
}

// ---- GraveCairn --------------------------------------------------------------------------------------------------
// A lone grave. (M2 fixer round 3, review: "at 1x it is hard to tell from the grey boulders around it") It reads as a
// grave at a glance: a pale upright headstone with a rounded top and a carved cross at the north end, a long low mound
// of turned earth before it ringed with fieldstones (the cairn), wild flowers laid at its foot; lit from the top-left,
// the mound's south-east flank and the stone's east edge in shade.
void graveCairn(Canvas& c) {
  const int W = c.w, H = c.h, by = H - 2;
  const Ramp St = ramp5(rgba(70, 68, 80), rgba(116, 112, 118), rgba(162, 158, 156), rgba(200, 196, 186), rgba(230, 226, 214));
  const Ramp Fs = ramp5(rgba(54, 50, 64), rgba(88, 84, 94), rgba(126, 120, 120), rgba(160, 152, 144), rgba(194, 186, 172));
  const Ramp Ea = ramp5(rgba(56, 38, 30), rgba(84, 58, 40), rgba(112, 80, 52), rgba(140, 104, 66), rgba(168, 130, 86));
  const int cx = W / 2;
  // the headstone: a slab with a rounded top, its south face toward us, a lit top edge and west side, the east in shade
  const int sx0 = cx - 5, sx1 = cx + 4, sTop = 1, sFoot = 12;
  for (int y = sTop; y <= sFoot; y++)
    for (int x = sx0; x <= sx1; x++) {
      const float dx = x + 0.5f - (sx0 + sx1 + 1) * 0.5f, r = (sx1 - sx0 + 1) * 0.5f;
      if (y < sTop + 4) {   // the rounded top
        const float dy = (sTop + 4) - (y + 0.5f);
        if (dx * dx + dy * dy * 1.9f > r * r) continue;
      }
      int k = 3;
      if (x == sx0 || (y < sTop + 4 && dx < 0 && (dx - 1) * (dx - 1) + ((sTop + 4) - (y + 0.5f)) * ((sTop + 4) - (y + 0.5f)) * 1.9f > r * r)) k = 4;
      if (x >= sx1 - 1) k = x == sx1 ? 1 : 2;
      if (y >= sFoot - 1) k = std::min(k, 2);
      c.set(x, y, St[k]);
    }
  // the carved cross on its face
  for (int y = sTop + 3; y <= sTop + 9; y++) c.set(cx - 1, y, St[1]);
  hline(c, cx - 3, cx + 1, sTop + 5, St[1]);
  for (int y = sTop + 3; y <= sTop + 9; y++) c.set(cx, y, St[4]);   // the cut's lit far edge
  // its foot sunk in the earth
  hline(c, sx0, sx1, sFoot, St[0]);
  // the mound of turned earth before it, long and low, lit on its north-west, grass creeping over its edge
  for (int y = sFoot - 1; y <= by; y++)
    for (int x = 2; x < W - 2; x++) {
      const float u = (x + 0.5f - cx) / 7.0f, v = (y + 0.5f - (sFoot + by) * 0.5f) / ((by - sFoot) * 0.5f + 1.0f);
      const float d = u * u + v * v;
      if (d > 1.0f || (y < sFoot + 1 && std::fabs(u) < 0.8f && y <= sFoot)) continue;
      const float lit = -u * 0.5f - v * 0.6f + (1.0f - d) * 0.5f;
      int k = lit > 0.55f ? 4 : lit > 0.25f ? 3 : lit > -0.05f ? 2 : lit > -0.4f ? 1 : 0;
      if (hashf(x, y, 131) < 0.12f) k = std::max(0, k - 1);
      if (d > 0.72f && hashf(x, y, 133) < 0.45f) { c.set(x, y, kLeaf[y > by - 2 ? 1 : 2]); continue; }
      c.set(x, y, Ea[k]);
    }
  // the fieldstones ringing it (the cairn)
  struct Fst { float x, y, rx, ry; };
  const float fb = static_cast<float>(by);
  const Fst fs[] = {{3.5f, fb - 5, 2.2f, 1.7f}, {W - 4.0f, fb - 6, 2.0f, 1.6f}, {4.5f, fb - 1, 2.4f, 1.6f}, {W - 5.0f, fb - 1.5f, 2.4f, 1.7f},
                    {cx - 3.0f, fb + 0.2f, 2.0f, 1.4f}, {cx + 3.5f, fb + 0.4f, 2.1f, 1.4f}};
  for (int i = 0; i < 6; i++) rock(c, fs[i].x, fs[i].y, fs[i].rx, fs[i].ry, Fs, 141 + i, 4);
  // wild flowers laid at the foot of the stone
  const uint32_t fl[3] = {rgba(236, 74, 70), rgba(250, 214, 92), rgba(244, 240, 228)};
  for (int i = 0; i < 6; i++) {
    const int x = cx - 3 + (int)(hash3(i, 3, 151) % 7), y = sFoot + 1 + (int)(hash3(i, 4, 151) % 2);
    c.set(x, y + 1, kLeaf[2]);
    c.set(x, y, fl[i % 3]);
  }
  outline(c, 0.95f);
  tufts(c, 1, W - 2, by + 1, 123, 0.5f);
}

// ---- Bedroll -----------------------------------------------------------------------------------------------------
// A traveller's bedroll by a ring of fire stones: the blanket in a striped wool, a rolled pillow, a pack; the embers
// glow and breathe (4 frames), a thread of smoke.
void bedroll(Canvas& c, int frame) {
  const int H = c.h, by = H - 2;
  const Ramp Bl = ramp5(rgba(70, 30, 40), rgba(118, 44, 46), rgba(168, 68, 54), rgba(204, 106, 72), rgba(234, 160, 112));
  for (int y = by - 7; y <= by; y++)
    for (int x = 1; x <= 16; x++) {
      const float u = (x - 1) / 15.0f, v = (y - (by - 7)) / 7.0f;
      if ((x == 1 || x == 16) && (y == by - 7 || y == by)) continue;
      int k = v < 0.3f ? 3 : (v > 0.8f ? 1 : 2);
      if (x == 1) k = std::min(4, k + 1);
      uint32_t col = Bl[k];
      if ((x + 1) % 5 == 0) col = darken(col, 0.35f);                    // the stripes
      if (u > 0.65f && v < 0.55f) col = kCloth[k];                         // the blanket turned back
      c.set(x, y, col);
    }
  ball(c, 3.5f, by - 4.0f, 2.6f, 3.0f, kCloth, 0.1f);                     // the rolled pillow
  shadedBox(c, 1, by - 11, 6, by - 8, kLeather, 2);                     // the pack behind it
  // the fire ring and its embers
  const float fx = 23.5f, fy = by - 3.0f;
  for (int i = 0; i < 7; i++) {
    const float a = i / 7.0f * TAU;
    ball(c, fx + std::cos(a) * 4.2f, fy + std::sin(a) * 2.6f, 1.6f, 1.2f, kStone, 0.1f);
  }
  const float glow = 0.5f + 0.5f * std::sin(frame / 4.0f * TAU);
  for (int y = (int)fy - 1; y <= (int)fy + 1; y++)
    for (int x = (int)fx - 2; x <= (int)fx + 2; x++) {
      const float h = hashf(x, y, 131 + (uint32_t)frame);
      const int k = h < 0.25f + glow * 0.25f ? 3 + (h < 0.12f ? 1 : 0) : (h < 0.7f ? 1 : 0);
      c.set(x, y, k >= 1 ? kFire[k] : kWoodDark[0]);
    }
  thickLine(c, fx - 2, fy, fx + 2, fy - 1, 1.0f, kWoodDark[1]);   // a charred stick
  for (int k = 0; k < 3; k++) c.set((int)fx + ((k + frame) % 2), (int)fy - 3 - k * 2 - (frame & 1), withA(kStone[3], 120 - k * 30));
  outline(c, 0.85f);
}

// ---- ElderTree ---------------------------------------------------------------------------------------------------
// A colossal ancient tree: a trunk as broad as a house, its bark in deep twisting ridges, great buttress roots spreading
// over the ground with their own shadows, a hollow, and a crown like a green hill: dozens of clumps, lit on the top-left,
// deep shadow under its eaves, the light catching single leaves; moss on the north of the trunk.
void elderTree(Canvas& c) {
  const int W = c.w, H = c.h, by = H - 4;
  const float cx = W * 0.5f;
  const Ramp Bk = ramp5(rgba(40, 28, 40), rgba(66, 44, 46), rgba(96, 68, 56), rgba(130, 98, 72), rgba(164, 132, 96));
  // the roots, spreading over the ground (painted first: the trunk stands on them)
  const float roots[][3] = {{-1.0f, 34, 9}, {-0.55f, 22, 6}, {0.0f, 12, 5}, {0.5f, 24, 6}, {1.0f, 36, 9}, {-0.8f, 40, 7}, {0.8f, 30, 7}};
  for (const auto& rt : roots) {
    const float dir = rt[0], len = rt[1], drop = rt[2];
    const Vec2 a(cx + dir * 10.0f, (float)by - 12), b(cx + dir * (10.0f + len), (float)by - 2 + drop * 0.2f - (std::fabs(dir) < 0.3f ? -3 : 0));
    capsule(c, a, b, 5.5f, 1.5f, Bk, 0, 0.08f);
  }
  // the trunk: a flared column with twisting bark ridges
  const int ty0 = 52, ty1 = by - 8;
  for (int y = ty0; y <= ty1; y++) {
    const float t = (y - ty0) / (float)(ty1 - ty0);
    const float hw = 12.0f + t * t * 9.0f + (vnoise(0, y / 6.0f, 141) - 0.5f) * 2.0f;
    for (int x = (int)(cx - hw); x <= (int)(cx + hw); x++) {
      const float u = (x + 0.5f - cx) / hw;
      const float ridge = std::sin((x + 0.5f - cx) * 0.9f + y * 0.18f + vnoise(x / 4.0f, y / 8.0f, 143) * 3.0f);
      float l = lightAt(u * 0.9f, 0) + ridge * 0.22f + (vnoise(x / 2.0f, y / 2.0f, 145) - 0.5f) * 0.2f;
      int k = lightIndex(l, x, y, 0.1f);
      if (ridge < -0.75f) k = std::max(0, k - 1);
      c.set(x, y, Bk[k]);
    }
  }
  // the hollow
  for (int y = by - 30; y <= by - 16; y++)
    for (int x = (int)cx - 6; x <= (int)cx + 3; x++) {
      const float u = (x + 1.5f - cx) / 4.5f, v = (y - (by - 23.0f)) / 7.0f;
      if (u * u + v * v > 1) continue;
      c.set(x, y, u * u + v * v > 0.7f ? Bk[0] : kInk);
    }
  // moss on the trunk's north and west
  for (int y = ty0; y <= ty1; y++)
    for (int x = 0; x < W; x++) {
      if (!solid(c, x, y) || x > cx) continue;
      if (vnoise(x / 3.0f, y / 3.0f, 147) > 0.68f) c.set(x, y, kMoss[(x + y) % 3 == 0 ? 3 : 2]);
    }
  // the crown: a broad dome of clumps
  std::vector<Blob> b;
  const int rings = 4;
  for (int r = 0; r < rings; r++) {
    const float ry = 18.0f + r * 12.0f, span = 26.0f + r * 9.0f;
    const int n = 4 + r * 2;
    for (int i = 0; i < n; i++) {
      const float f = n == 1 ? 0.5f : i / (float)(n - 1);
      const float x = cx + (f - 0.5f) * 2 * span + (hashf(i, r, 149) - 0.5f) * 6;
      const float y = ry + std::fabs(f - 0.5f) * 10.0f + (hashf(i, r, 151) - 0.5f) * 4;
      b.push_back({x, y, 12.0f + hashf(i, r, 153) * 5.0f - r * 0.6f});
    }
  }
  b.push_back({cx, 14, 13});
  b.push_back({cx - 14, 20, 11});
  b.push_back({cx + 15, 21, 11});
  // a canvas of its own: the crown's shadow then falls on the trunk below
  Canvas cr(W, H);
  crown(cr, b, kLeafDark, 155, 0.8f, 0.55f);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) if (solid(cr, x, y)) c.set(x, y, cr.get(x, y));
  // the deep shade under the crown on the trunk
  for (int x = 0; x < W; x++) {
    int last = -1;
    for (int y = 0; y < H; y++) if (solid(cr, x, y)) last = y;
    if (last < 0) continue;
    for (int y = last + 1; y <= last + 9 && y < H; y++)
      if (solid(c, x, y)) c.set(x, y, darken(c.get(x, y), 0.55f - (y - last) * 0.05f));
  }
  // hanging moss strands under the eaves
  for (int x = 12; x < W - 12; x += 5) {
    int last = -1;
    for (int y = 0; y < H; y++) if (solid(cr, x, y)) last = y;
    if (last < 0 || hashf(x, 0, 157) < 0.5f) continue;
    const int len = 3 + (int)(hash3(x, 1, 157) % 6);
    for (int k = 1; k <= len; k++) c.set(x, last + k, kMoss[k < len / 2 ? 2 : 1]);
  }
  outline(c, 0.9f);
  tufts(c, 2, W - 3, by + 2, 159, 0.55f);
  tufts(c, 8, W - 9, by - 1, 161, 0.18f);
}

// ---- Colossus ----------------------------------------------------------------------------------------------------
// The weathered statue of a forgotten king, sunk to the knees in the land: a crowned bearded head, a cloak falling
// from broad shoulders, both hands on the pommel of a greatsword standing point-down before him. Stone lit from the
// top-left; long cracks, a broken crown point, moss on every upward surface, grass and rubble at the foot.
void colossus(Canvas& c) {
  const int W = c.w, H = c.h, by = H - 4;
  const float cx = W * 0.5f;
  const Ramp S = ramp5(rgba(56, 50, 62), rgba(92, 84, 90), rgba(134, 124, 118), rgba(172, 162, 146), rgba(208, 198, 176));
  auto put = [&](int x, int y, float nx, float ny, float extra = 0.0f) {
    float l = lightAt(nx, ny) + extra + (vnoise(x / 3.0f, y / 3.0f, 171) - 0.5f) * 0.25f;
    c.set(x, y, S[lightIndex(l, x, y, 0.1f)]);
  };
  // the cloaked body: a broad trapezoid with folds
  const int sy0 = 40, sy1 = by - 6;
  for (int y = sy0; y <= sy1; y++) {
    const float t = (y - sy0) / (float)(sy1 - sy0);
    const float hw = 18.0f + t * 4.0f;
    for (int x = (int)(cx - hw); x <= (int)(cx + hw); x++) {
      const float u = (x + 0.5f - cx) / hw;
      const float fold = std::sin(u * 11.0f + t * 2.0f) * 0.25f;
      put(x, y, u * 0.85f, t < 0.08f ? -0.7f : -0.05f, fold - 0.22f);
    }
  }
  // the shoulders' rounded tops
  ball(c, cx - 13, sy0 + 2, 7, 5, S, 0.1f);
  ball(c, cx + 13, sy0 + 2, 7, 5, S, 0.1f, -1);
  // the arms folded down to the hands on the pommel
  capsule(c, V(cx - 15, sy0 + 3), V(cx - 4, sy0 + 26), 4.5f, 3.5f, S, 0, 0.08f);
  capsule(c, V(cx + 15, sy0 + 3), V(cx + 4, sy0 + 26), 4.5f, 3.5f, S, -1, 0.08f);
  // the greatsword: pommel and grip between the hands, the guard, the blade down into the earth
  for (int y = sy0 + 29; y <= by - 2; y++) { c.set((int)cx - 1, y, S[4]); c.set((int)cx, y, S[3]); c.set((int)cx + 1, y, S[1]); }
  thickLine(c, cx - 8, sy0 + 30, cx + 8, sy0 + 30, 2.2f, S[3]);
  hline(c, (int)cx - 8, (int)cx + 8, sy0 + 31, S[1]);
  ball(c, cx, sy0 + 21, 3.0f, 3.0f, S, 0.1f);
  ball(c, cx - 3, sy0 + 26, 3.5f, 3.0f, S, 0.1f);   // the hands
  ball(c, cx + 3, sy0 + 26, 3.5f, 3.0f, S, 0.1f, -1);
  // the head: beard, face, the crown
  ball(c, cx, 29, 8.5f, 9.5f, S, 0.1f);
  for (int y = 30; y <= 42; y++)   // the beard, in combed strands
    for (int x = (int)cx - 6; x <= (int)cx + 6; x++) {
      const float u = (x + 0.5f - cx) / 6.5f, v = (y - 30) / 12.0f;
      if (std::fabs(u) > 1.0f - v * 0.65f) continue;
      put(x, y, u * 0.6f, 0.2f, ((x & 1) ? 0.12f : -0.08f));
    }
  hline(c, (int)cx - 4, (int)cx - 2, 26, S[0]); hline(c, (int)cx + 2, (int)cx + 4, 26, S[0]);   // the eyes, deep set
  vline(c, (int)cx, 27, 30, S[3]); dot(c, (int)cx + 1, 30, S[1]);                            // the nose
  hline(c, (int)cx - 3, (int)cx + 3, 33, S[1]);                                              // the mouth under the moustache
  // the crown: a band with five points, one broken off
  for (int x = (int)cx - 8; x <= (int)cx + 8; x++) { c.set(x, 20, S[3]); c.set(x, 21, S[2]); c.set(x, 22, S[1]); }
  for (int i = 0; i < 5; i++) {
    const int px = (int)cx - 8 + i * 4;
    const int h = i == 3 ? 2 : 5;
    for (int k = 1; k <= h; k++) { c.set(px, 20 - k, S[3]); if (k < h) c.set(px + 1, 20 - k, S[1]); }
  }
  // cracks running down the stone, weathering pits
  auto crack = [&](float x0, float y0, int n, uint32_t s) {
    float x = x0, y = y0;
    for (int i = 0; i < n; i++) {
      if (solid(c, (int)x, (int)y)) { c.set((int)x, (int)y, S[0]); if (solid(c, (int)x + 1, (int)y)) c.set((int)x + 1, (int)y, S[3]); }
      x += (hashf(i, 0, s) - 0.5f) * 2.0f; y += 1.0f;
    }
  };
  crack(cx - 9, 46, 24, 173);
  crack(cx + 6, 22, 16, 175);
  crack(cx + 12, 60, 30, 177);
  for (int i = 0; i < 30; i++) dotOn(c, (int)(cx - 20 + hash3(i, 1, 179) % 40), 20 + (int)(hash3(i, 2, 179) % 80), S[1]);
  // moss on the upward surfaces: crown, shoulders, the guard, the folds
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!solid(c, x, y) || solid(c, x, y - 1)) continue;
      if (vnoise(x / 2.5f, y / 2.5f, 181) > 0.4f) { c.set(x, y, kMoss[3]); if (solid(c, x, y + 1) && hashf(x, y, 183) < 0.6f) c.set(x, y + 1, kMoss[2]); }
    }
  lichen(c, 0, by - 30, W - 1, by, 185, 0.3f, true);
  lichen(c, 0, 36, W - 1, 60, 187, 0.18f, false);
  // the earth he is sunk in: a mound with rubble
  for (int y = by - 7; y <= by; y++)
    for (int x = (int)cx - 26; x <= (int)cx + 26; x++) {
      const float u = (x + 0.5f - cx) / 26.0f, v = (by - y) / 7.0f;
      if (v > (1 - u * u) * (0.8f + vnoise(x / 4.0f, 0, 189) * 0.4f)) continue;
      c.set(x, y, kLeaf[lightIndex(0.5f - u * 0.5f - v * 0.2f + (hashf(x, y, 191) - 0.5f) * 0.4f, x, y, 0.1f)]);
    }
  rock(c, cx - 20, by - 1, 4, 3, S, 193, 4);
  rock(c, cx + 18, by, 5, 3, S, 195, 4);
  outline(c, 0.9f);
  tufts(c, 2, W - 3, by + 2, 197, 0.6f);
  tufts(c, (int)cx - 22, (int)cx + 22, by - 5, 199, 0.25f);
}

// ---- StarShard ---------------------------------------------------------------------------------------------------
// A jagged shard of a fallen star, half buried: facets of pale blue and violet crystal, a white-hot core seam, a halo
// that breathes over 4 frames, motes of light rising, the scorched earth at its foot.
void starShard(Canvas& c, int frame) {
  const int W = c.w, H = c.h, by = H - 3;
  const float cx = W * 0.5f;
  const float pulse = 0.5f + 0.5f * std::sin(frame / 4.0f * TAU);
  // the halo (translucent, dithered, breathing)
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const float dx = (x + 0.5f - cx) / (9.0f + pulse * 1.5f), dy = (y + 0.5f - (by - 11.0f)) / (14.0f + pulse * 2.0f);
      const float d = dx * dx + dy * dy;
      if (d > 1) continue;
      if (bayer(x, y) * 1.3f < (1 - d)) c.set(x, y, withA(kCrystal[3], (int)((1 - d) * (50 + pulse * 50))));
    }
  // scorched, cracked earth at its foot
  for (int x = (int)cx - 8; x <= (int)cx + 8; x++) {
    c.set(x, by + 1, rgba(58, 44, 46));
    if (std::abs(x - (int)cx) < 6) c.set(x, by, rgba(74, 54, 52));
  }
  // a cluster of crystals: each one a long prism, its west facet lit, east facet in shade, a bright ridge between
  struct Cr { float x, lean, h, w; };
  const Cr crs[] = {{cx - 5, -0.35f, 9, 2.2f}, {cx + 5, 0.4f, 11, 2.4f}, {cx, 0.08f, 24, 3.6f}, {cx - 2, -0.15f, 14, 2.6f}, {cx + 2.5f, 0.2f, 7, 2.0f}};
  for (const Cr& k : crs) {
    for (int y = 0; y <= (int)k.h; y++) {
      const float t = y / k.h;                       // 0 at the foot .. 1 at the tip
      const float mx = k.x + k.lean * y;
      const float hw = k.w * (t < 0.75f ? 1.0f : (1 - t) / 0.25f);
      const int yy = by - y;
      for (int x = (int)std::floor(mx - hw); x <= (int)std::ceil(mx + hw); x++) {
        const float u = (x + 0.5f - mx) / std::max(0.5f, hw);
        if (std::fabs(u) > 1.05f) continue;
        int idx = u < -0.15f ? 3 : (u < 0.2f ? 4 : 1);
        if (t > 0.85f) idx = 4;
        if (std::fabs(u) > 0.85f) idx = std::max(0, idx - 1);
        c.set(x, yy, kCrystal[idx]);
      }
    }
  }
  // the white-hot heart of the great crystal, flickering
  for (int y = by - 18; y < by - 2; y++) if ((y + frame) % 3 != 0) c.set((int)std::lround(cx + 0.08f * (by - y)), y, kWhite);
  // motes rising
  for (int i = 0; i < 5; i++) {
    const int mx = (int)cx - 8 + (int)(hash3(i, 0, 211) % 17);
    const int my = by - 8 - (int)((hash3(i, 1, 211) % 12 + frame * 3 + i * 5) % 22);
    if (my > 0) c.set(mx, my, i == (frame & 3) ? kWhite : kCrystal[4]);
  }
  // outline the crystals only (not the halo)
  Canvas src = c;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (chA(src.get(x, y)) == 255) continue;
      static const int nx[4] = {0, -1, 1, 0}, ny[4] = {1, 0, 0, -1};
      for (int k = 0; k < 4; k++) {
        const uint32_t n = src.get(x + nx[k], y + ny[k]);
        if (chA(n) == 255 && n != rgba(58, 44, 46) && n != rgba(74, 54, 52)) { c.set(x, y, kCrystal[0]); break; }
      }
    }
}

// ---- DragonBones -------------------------------------------------------------------------------------------------
// The bones of an ancient dragon bleaching in the open, in perspective: the great horned skull on the west resting on
// its jaw, the spine lying along the ground as a chain of vertebrae, the ribs arching up over it like the beams of a
// sunken hall (the far half of each arch behind in shade, the near half in front, some broken), a wing bone and the
// tail's last vertebrae in the grass.
void dragonBones(Canvas& c) {
  const int W = c.w, H = c.h, by = H - 4;
  const Ramp B = kBone;
  auto spineY = [&](float x) { return by - 6.0f - std::sin((x - 26) / (W - 30.0f) * 3.14159f) * 3.0f; };
  // one rib arch: a curve in the north-south plane from the spine (the far foot, up the screen) over the top to the
  // near foot (down the screen), swept a little back toward the tail
  auto arch = [&](float x, float h, bool nearHalf, bool broken) {
    const float sy = spineY(x);
    const float half = 13.0f;   // ground half-width of the cage (tiles of depth: screen half = half / 2)
    Vec2 prev(0, 0);
    bool first = true;
    const int n = 16;
    for (int i = 0; i <= n; i++) {
      const float th = 3.14159f * i / n;          // 0 far foot .. pi near foot
      if (nearHalf != (th > 1.5708f)) { first = true; continue; }
      if (broken && nearHalf && th > 2.4f) break;
      const float zz = std::cos(th) * half;        // + far, - near
      const float hh = std::sin(th) * h;
      const Vec2 p(x + std::sin(th) * 5.0f + th * 1.2f, sy - zz * 0.5f - hh);
      if (!first) capsule(c, prev, p, 1.4f, 1.2f, B, nearHalf ? 0 : -1, 0.05f);
      prev = p;
      first = false;
    }
  };
  // the far halves of the ribs, then the spine, then the near halves
  for (int i = 0; i < 6; i++) arch(36.0f + i * 12.0f, 25.0f - std::fabs(i - 2.0f) * 3.0f, false, false);
  for (float x = 26; x < W - 6; x += 3.0f) {
    const float y = spineY(x);
    const float r = 2.6f - (x - 26) / (W - 30.0f) * 1.3f;
    ball(c, x, y, r + 0.6f, r, B, 0.08f);
    if (((int)(x / 3.0f)) % 2 == 0 && x < W - 24) thickLine(c, x, y - r, x + 1.0f, y - r - 3.0f, 1.0f, B[3]);
  }
  for (int i = 0; i < 4; i++) ball(c, W - 7.0f + i * 1.5f, by - 1.0f + i * 0.4f, 1.3f, 1.0f, B, 0.08f);
  for (int i = 0; i < 6; i++) arch(36.0f + i * 12.0f, 25.0f - std::fabs(i - 2.0f) * 3.0f, true, i == 4);
  // a wing bone lying across the ground in front
  capsule(c, V(62, by + 1), V(92, by + 2.5f), 1.8f, 1.1f, B, -1, 0.06f);
  capsule(c, V(92, by + 2.5f), V(103, by - 1), 1.1f, 0.7f, B, -1, 0.06f);
  // the skull: cranium, a long snout with teeth, the eye socket, two swept horns, the jaw on the ground
  capsule(c, V(22, by - 4), V(4, by - 2), 3.2f, 2.0f, B, -1, 0.06f);             // the jaw
  ball(c, 20, by - 13, 10.0f, 8.0f, B, 0.08f);                                   // the cranium
  capsule(c, V(15, by - 10), V(2, by - 7), 6.0f, 3.6f, B, 0, 0.06f);             // the snout
  for (int x = 3; x < 17; x += 2) { dot(c, x, by - 4, kWhite); dot(c, x + 1, by - 5, B[3]); }   // the teeth
  ellipse(c, 18, by - 15, 3.4f, 2.8f, B[0]);                                     // the eye socket
  ellipse(c, 17.5f, by - 15.5f, 2.2f, 1.7f, kInk);
  ellipse(c, 5, by - 10, 1.3f, 1.0f, B[0]);                                      // the nostril
  capsule(c, V(24, by - 19), V(37, by - 30), 2.7f, 0.8f, B, 0, 0.06f);           // the horns
  capsule(c, V(27, by - 16), V(41, by - 22), 2.2f, 0.7f, B, -1, 0.06f);
  // cracks, weathering, moss in the hollows, grass growing through
  for (int i = 0; i < 40; i++) { const int x = (int)(hash3(i, 1, 221) % W), y = (int)(hash3(i, 2, 221) % H); if (solid(c, x, y) && luma(c.get(x, y)) > 0.6f) c.set(x, y, B[1]); }
  lichen(c, 0, by - 10, W - 1, by + 2, 223, 0.12f, false);
  outline(c, 0.9f);
  tufts(c, 0, W - 1, by + 3, 225, 0.45f);
  tufts(c, 4, W - 5, by - 1, 227, 0.14f);
}
}  // namespace

int wildPropW(Prop p) { return wi(p).w; }
int wildPropH(Prop p) { return wi(p).h; }
int wildPropFrames(Prop p) { return wi(p).frames; }

void paintWildProp(Canvas& c, Prop p, int frame) {
  switch (p) {
    case Prop::Peak: { Canvas k = peakVariant(0, 0); blit(c, k, 0, 0); break; }
    case Prop::StandingStone: standingStone(c); break;
    case Prop::CaravanWreck: caravanWreck(c); break;
    case Prop::WatchtowerRuin: watchtower(c); break;
    case Prop::FishingShack: fishingShack(c); break;
    case Prop::TollPost: tollPost(c); break;
    case Prop::HerbBed: herbBed(c); break;
    case Prop::GraveCairn: graveCairn(c); break;
    case Prop::Bedroll: bedroll(c, frame); break;
    case Prop::ElderTree: elderTree(c); break;
    case Prop::Colossus: colossus(c); break;
    case Prop::StarShard: starShard(c, frame); break;
    case Prop::DragonBones: dragonBones(c); break;
    case Prop::GreatPeak: { Canvas k = peakVariant(8, 0); blit(c, k, 0, 0); break; }
    default: break;
  }
}

Canvas peakVariant(int v, int land) {
  // v 0..7 a Peak; v 8..15 a GreatPeak (the massif at a named summit or a range's crest) in shape v & 7
  const Prop p = v >= 8 ? Prop::GreatPeak : Prop::Peak;
  Canvas c(wildPropW(p), wildPropH(p));
  peak(c, v & 7, std::clamp(land, 0, 2));
  const Canvas before = c;
  outline(c, 0.92f);
  // no ink line where the mountain meets the land: its lowest rows keep their own pixels only
  for (int y = c.h - 8; y < c.h; y++)
    for (int x = 0; x < c.w; x++) if (!solid(before, x, y) && !solid(before, x, y - 1)) c.set(x, y, 0);
  feather(c, 2);
  return c;
}

}  // namespace art
