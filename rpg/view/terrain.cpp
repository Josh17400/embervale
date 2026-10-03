// Terrain painting: every ground pixel is computed procedurally and baked into 32x32-tile chunk textures.
// Natural terrains get organic, domain-warped borders; built surfaces (roads, floors) stay crisp.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include "rpg/view/view.h"

namespace {
constexpr int CH = 32;   // tiles per chunk side

inline uint32_t C(int r, int g, int b, int a = 255) { return rgba(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), a); }
inline uint32_t lerpc(uint32_t a, uint32_t b, float t) {
  int ar = a & 255, ag = (a >> 8) & 255, ab = (a >> 16) & 255, aa = (a >> 24) & 255;
  int br = b & 255, bg = (b >> 8) & 255, bb = (b >> 16) & 255, ba = (b >> 24) & 255;
  return C((int)(ar + (br - ar) * t), (int)(ag + (bg - ag) * t), (int)(ab + (bb - ab) * t), (int)(aa + (ba - aa) * t));
}
inline uint32_t mul(uint32_t c, float k) {
  return C((int)((c & 255) * k), (int)(((c >> 8) & 255) * k), (int)(((c >> 16) & 255) * k), (int)(c >> 24));
}
inline uint32_t pick3(float n, uint32_t a, uint32_t b, uint32_t c) { return n < 0.40f ? a : (n < 0.62f ? b : c); }
const float kBayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
// smooth large-scale tone field, ordered-dithered at the band edges (classic 16-bit look)
inline float toneField(int px, int py, uint32_t seed) {
  float n = vnoise(px / 13.0f, py / 13.0f, seed) * 0.72f + vnoise(px / 4.5f, py / 4.5f, seed + 1) * 0.28f;
  return n + (kBayer[(py & 3) * 4 + (px & 3)] / 16.0f - 0.5f) * 0.10f;
}
// small grass tufts: a dark root pixel with two lit blade tips, one per 6x6 cell at most
inline int tuft(int px, int py, uint32_t seed, float density) {
  int cx = px / 6, cy = py / 6;
  uint32_t h = hash2(cx, cy, seed);
  if ((h & 1023) / 1024.0f > density) return 0;
  int hx = cx * 6 + 1 + (int)((h >> 10) % 4), hy = cy * 6 + 2 + (int)((h >> 14) % 3);
  if (px == hx && py == hy) return 1;
  if ((px == hx - 1 || px == hx + 1) && py == hy - 1) return 2;
  if (px == hx && py == hy - 2 && (h >> 20) & 1) return 2;
  return 0;
}

bool soft(Ground g) {
  switch (g) {
    case Ground::Grass: case Ground::Meadow: case Ground::ForestFloor: case Ground::Autumn: case Ground::Tundra: case Ground::Snow:
    case Ground::Sand: case Ground::Swamp: case Ground::Dirt: case Ground::Water: case Ground::DeepWater: case Ground::CaveFloor: case Ground::Ice:
      return true;
    default: return false;
  }
}
bool natural(Ground g) { return soft(g) || g == Ground::Rock || g == Ground::CaveWall; }
bool isWall(Ground g) { return g == Ground::Rock || g == Ground::CaveWall || g == Ground::InteriorWall || g == Ground::Void; }
}  // namespace

// Terrace height of a rock tile (0 = not rock). Limited by the distance to open ground, so edges always step down one level at a time.
int View::rockLevel(const Map& m, int tx, int ty) {
  Ground g = m.at(tx, ty);
  if (g != Ground::Rock && g != Ground::CaveWall && !(g == Ground::Void && m.kind != MapKind::Overworld)) return 0;
  if (m.kind != MapKind::Overworld) return 1;
  int want = 1 + (int)(vnoise(tx / 13.0f, ty / 13.0f, 501) * 3.3f);
  if (want <= 1) return 1;
  for (int r = 1; r < want; r++)
    for (int oy = -r; oy <= r; oy++)
      for (int ox = -r; ox <= r; ox++) {
        if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
        Ground n = m.at(tx + ox, ty + oy);
        if (n != Ground::Rock && n != Ground::Void) return r;
      }
  return want;
}

uint32_t View::groundPixel(const Map& m, int px, int py) {
  int tx = px >> 4, ty = py >> 4;
  Ground real = m.at(tx, ty);
  if (real == Ground::Void && m.kind == MapKind::Overworld) real = Ground::DeepWater;
  // organic borders: look the terrain up at a warped position
  Ground g = real;
  int sx = px, sy = py;   // sample position (warped for natural terrain)
  if (natural(real)) {
    float amp = (real == Ground::Rock || real == Ground::CaveWall) ? 5.0f : 7.0f;
    float wx = px + (vnoise(px / 7.0f, py / 7.0f, 11) - 0.5f) * amp + (hashf(px, py, 3) - 0.5f) * 1.5f;
    float wy = py + (vnoise(px / 7.0f, py / 7.0f, 23) - 0.5f) * amp + (hashf(px, py, 5) - 0.5f) * 1.5f;
    Ground w = m.at((int)std::floor(wx / 16), (int)std::floor(wy / 16));
    if (w == Ground::Void && m.kind == MapKind::Overworld) w = Ground::DeepWater;
    if (natural(w) && !(isWall(w) != isWall(real) && m.kind != MapKind::Overworld)) { g = w; sx = (int)std::floor(wx); sy = (int)std::floor(wy); }
  } else if (real == Ground::Road || real == Ground::Farmland) {
    // roads fray a little at their edges into the surrounding soft ground
    float wx = px + (vnoise(px / 5.0f, py / 5.0f, 31) - 0.5f) * 4.0f, wy = py + (vnoise(px / 5.0f, py / 5.0f, 37) - 0.5f) * 4.0f;
    Ground w = m.at((int)std::floor(wx / 16), (int)std::floor(wy / 16));
    if (soft(w) && !groundWater(w)) g = w;
  }
  float n = toneField(px, py, 101);
  float h = hashf(px, py, 13);
  int lx = px & 15, ly = py & 15;
  uint32_t c = 0;
  switch (g) {
    case Ground::Grass:
    case Ground::Meadow: {
      bool meadow = g == Ground::Meadow;
      uint32_t a = meadow ? C(104, 168, 72) : C(78, 144, 62), b = meadow ? C(120, 182, 80) : C(92, 160, 68), cc = meadow ? C(136, 192, 88) : C(106, 172, 76);
      c = pick3(n, a, b, cc);
      float big = vnoise(px / 40.0f, py / 40.0f, 141);
      c = mul(c, 0.94f + big * 0.12f);
      int tf = tuft(px, py, 401, meadow ? 0.55f : 0.45f);
      if (tf == 1) c = mul(c, 0.74f);
      else if (tf == 2) c = mul(c, 1.16f);
      if (meadow && h > 0.992f) { static const uint32_t fl[] = {C(250, 250, 240), C(250, 220, 90), C(240, 130, 170), C(160, 150, 250)}; c = fl[hash2(px, py, 9) & 3]; }
      break;
    }
    case Ground::ForestFloor:
      c = pick3(n, C(50, 96, 50), C(58, 108, 54), C(66, 118, 58));
      c = mul(c, 0.9f + vnoise(px / 30.0f, py / 30.0f, 151) * 0.18f);
      { int tf = tuft(px, py, 411, 0.5f); if (tf == 1) c = mul(c, 0.72f); else if (tf == 2) c = mul(c, 1.14f); }
      if (h < 0.012f) c = C(110, 84, 50);
      break;
    case Ground::Autumn:
      c = pick3(n, C(152, 98, 50), C(168, 112, 56), C(184, 128, 64));
      if (h < 0.02f) c = C(200, 70, 40);
      else if (h < 0.04f) c = C(226, 176, 76);
      { int tf = tuft(px, py, 421, 0.35f); if (tf == 1) c = mul(c, 0.75f); else if (tf == 2) c = C(214, 150, 60); }
      break;
    case Ground::Tundra:
      c = pick3(n, C(100, 120, 92), C(112, 132, 100), C(124, 142, 108));
      if (h < 0.01f) c = C(150, 150, 146);
      { int tf = tuft(px, py, 431, 0.4f); if (tf == 1) c = mul(c, 0.78f); else if (tf == 2) c = mul(c, 1.12f); }
      break;
    case Ground::Snow:
      c = pick3(n, C(214, 224, 238), C(226, 234, 244), C(238, 243, 250));
      c = mul(c, 0.95f + vnoise(px / 26.0f, py / 26.0f, 161) * 0.06f);
      if (h > 0.996f) c = C(255, 255, 255);
      break;
    case Ground::Sand: {
      float rip = std::sin(px * 0.35f + py * 0.12f + vnoise(px / 16.0f, py / 16.0f, 171) * 8);
      c = pick3(n, C(214, 190, 134), C(224, 202, 146), C(232, 212, 158));
      if (rip > 0.8f) c = mul(c, 0.93f);
      if (h < 0.008f) c = C(180, 156, 112);
      break;
    }
    case Ground::Swamp:
      c = pick3(n, C(66, 82, 52), C(74, 92, 56), C(84, 100, 60));
      if (vnoise(px / 6.0f, py / 6.0f, 181) > 0.74f) c = C(58, 74, 66);
      if (h < 0.05f) c = C(100, 120, 60);
      break;
    case Ground::Dirt:
      c = pick3(n, C(132, 100, 66), C(144, 112, 74), C(156, 124, 82));
      if (h < 0.015f) c = C(172, 152, 122);
      else if (h < 0.03f) c = mul(c, 0.8f);
      break;
    case Ground::Farmland: {
      bool ridge = (py % 4) < 2;
      c = ridge ? C(128, 94, 60) : C(98, 70, 46);
      if (ridge && (hash2(px / 3, py / 4, 191) & 3) == 0) c = C(96, 150, 60);
      if (ridge && h < 0.08f) c = C(148, 112, 74);
      break;
    }
    case Ground::Road: {
      // worn cobbles: irregular stones with soft mortar, packed earth showing through toward the edges
      int row = py / 5;
      int shift = (row & 1) * 3;
      int col = (px + shift) / 6;
      bool mortar = ((px + shift) % 6 == 0) || (py % 5 == 0);
      uint32_t stone = lerpc(C(164, 152, 130), C(184, 172, 148), hashf(col, row, 201));
      uint32_t earth = lerpc(C(150, 124, 90), C(162, 136, 98), n);
      c = mortar ? lerpc(stone, C(130, 116, 96), 0.55f) : stone;
      if (!mortar && ((px + shift) % 6 == 1 || py % 5 == 1)) c = mul(c, 1.05f);
      // how close to the road's edge is this pixel?
      bool edgeL = m.at(tx - 1, ty) != Ground::Road && m.at(tx - 1, ty) != Ground::Plaza && m.at(tx - 1, ty) != Ground::Bridge;
      bool edgeR = m.at(tx + 1, ty) != Ground::Road && m.at(tx + 1, ty) != Ground::Plaza && m.at(tx + 1, ty) != Ground::Bridge;
      bool edgeU = m.at(tx, ty - 1) != Ground::Road && m.at(tx, ty - 1) != Ground::Plaza && m.at(tx, ty - 1) != Ground::Bridge;
      bool edgeD = m.at(tx, ty + 1) != Ground::Road && m.at(tx, ty + 1) != Ground::Plaza && m.at(tx, ty + 1) != Ground::Bridge;
      float e = 0;
      if (edgeL) e = std::max(e, 1.0f - lx / 6.0f);
      if (edgeR) e = std::max(e, 1.0f - (15 - lx) / 6.0f);
      if (edgeU) e = std::max(e, 1.0f - ly / 6.0f);
      if (edgeD) e = std::max(e, 1.0f - (15 - ly) / 6.0f);
      float wear = vnoise(px / 7.0f, py / 7.0f, 211) * 0.8f + e * 0.75f;
      if (wear > 0.72f) c = earth;
      else if (wear > 0.64f && mortar) c = earth;
      break;
    }
    case Ground::Plaza: {
      int row = py / 8, shift = (row & 1) * 4, col = (px + shift) / 8;
      bool line = ((px + shift) % 8 == 0) || (py % 8 == 0);
      c = line ? C(112, 110, 112) : lerpc(C(160, 158, 156), C(184, 180, 174), hashf(col, row, 221));
      if (!line && h < 0.04f) c = mul(c, 0.88f);
      break;
    }
    case Ground::StoneFloor: {
      int row = py / 8, shift = (row & 1) * 4, col = (px + shift) / 8;
      bool line = ((px + shift) % 8 == 0) || (py % 8 == 0);
      c = line ? C(58, 56, 62) : lerpc(C(96, 94, 104), C(116, 112, 122), hashf(col, row, 231));
      if (!line && h < 0.03f) c = C(80, 78, 86);
      break;
    }
    case Ground::WoodFloor: {
      int row = py / 4;
      int seamOff = (int)(hash2(row, 0, 241) % 24);
      bool seam = (py % 4 == 0) || ((px + seamOff) % 24 == 0);
      c = seam ? C(76, 50, 32) : lerpc(C(138, 94, 58), C(158, 110, 66), hashf((px + seamOff) / 24, row, 251));
      if (!seam && h < 0.06f) c = mul(c, 0.86f);   // grain
      break;
    }
    case Ground::Bridge: {
      bool vertical = groundWater(m.at(tx - 1, ty)) || groundWater(m.at(tx + 1, ty));
      int a = vertical ? px : py, b2 = vertical ? py : px;
      (void)b2;
      bool gap = a % 4 == 0;
      c = gap ? C(70, 46, 30) : lerpc(C(140, 96, 56), C(160, 112, 66), hashf(a / 4, 0, 261));
      int edge = vertical ? lx : ly;
      if (edge < 1 || edge > 14) c = C(90, 60, 36);
      break;
    }
    case Ground::Rock:
    case Ground::CaveWall: {
      // terraced cliffs: each rock tile has a height level; a tile whose southern neighbour is lower shows a cliff face
      bool cave = g == Ground::CaveWall;
      int gx = sx >> 4, gy = sy >> 4, sl = sx & 15, slx = sy & 15;
      (void)sl;
      int L = rockLevel(m, gx, gy), Lb = rockLevel(m, gx, gy + 1), La = rockLevel(m, gx, gy - 1);
      int faceH = cave ? 11 : 11;
      bool face = Lb < L && slx >= 16 - faceH;
      bool cold = !cave && (m.biomeAt(gx, gy) == Biome::Snow || m.biomeAt(gx, gy + 2) == Biome::Snow || m.biomeAt(gx, gy - 2) == Biome::Snow);
      if (face) {
        float k = (slx - (16 - faceH)) / (float)faceH;
        uint32_t base = cave ? C(78, 64, 58) : lerpc(C(98, 86, 82), C(112, 100, 92), (L - 1) / 3.0f);
        c = mul(base, 1.05f - k * 0.38f);
        int col = sx / 3 + (int)(hash2(sx / 3, gy, 281) % 2);
        if ((hash2(col, gy, 283) & 3) == 0) c = mul(c, 0.84f);          // vertical fissures
        if ((sx % 3) == 0) c = mul(c, 0.93f);
        if (slx == 16 - faceH) c = mul(c, 1.25f);                       // lit lip at the top of the face
        if (slx >= 14) c = mul(c, 0.72f);                                // contact shadow at the base
        if (cold && slx <= 16 - faceH + 1) c = C(236, 240, 248);         // snow overhang
      } else {
        // calm plateau surface, lighter with height
        uint32_t lo = cave ? C(44, 37, 35) : C(126, 119, 109), hi = cave ? C(54, 46, 42) : C(134, 127, 116);
        float lvl = (L - 1) / 3.0f;
        c = n < 0.5f ? lo : hi;
        c = mul(c, 0.94f + lvl * 0.16f + (vnoise(sx / 30.0f, sy / 30.0f, 291) - 0.5f) * 0.05f);
        int tf = tuft(sx, sy, 297, 0.25f);
        if (tf == 1) c = mul(c, 0.78f); else if (tf == 2) c = mul(c, 1.12f);      // pebbles
        if (!cave && L == 1 && m.kind == MapKind::Overworld && vnoise(sx / 9.0f, sy / 9.0f, 299) > 0.74f) c = lerpc(c, C(104, 128, 80), 0.35f);   // moss on the low ledges
        if (cold && (L >= 2 || vnoise(sx / 12.0f, sy / 12.0f, 301) > 0.45f)) {
          c = n < 0.5f ? C(228, 234, 244) : C(240, 244, 250);
          if (tf == 1) c = C(206, 214, 230);
        }
        if (La < L && slx < 2) c = mul(c, 1.18f);    // rim where the terrace above ends
        if (cave) c = mul(c, 0.75f + vnoise(sx / 10.0f, sy / 10.0f, 311) * 0.2f);
      }
      break;
    }
    case Ground::CaveFloor:
      c = pick3(n, C(70, 62, 58), C(78, 70, 64), C(86, 76, 70));
      if (h < 0.015f) c = C(104, 94, 86);
      else if (h < 0.03f) c = mul(c, 0.8f);
      break;
    case Ground::InteriorWall: {
      Ground below = m.at(tx, ty + 1);
      bool stone = below == Ground::StoneFloor || m.at(tx, ty + 2) == Ground::StoneFloor;
      if (below != Ground::InteriorWall && below != Ground::Void && ty > 0) {
        if (stone) {
          int row = py / 5, shift = (row & 1) * 5;
          bool line = ((px + shift) % 10 == 0) || (py % 5 == 0);
          c = line ? C(60, 58, 64) : lerpc(C(118, 114, 120), C(134, 128, 134), hashf((px + shift) / 10, row, 321));
        } else {
          bool seam = px % 6 == 0;
          c = seam ? C(70, 44, 28) : lerpc(C(128, 84, 50), C(146, 98, 58), hashf(px / 6, 0, 331));
          if (ly % 8 == 7) c = mul(c, 0.8f);
        }
        if (ly < 2) c = C(60, 40, 30);
        if (ly == 15) c = mul(c, 0.55f);
      } else {
        c = C(34, 26, 24);
        if (h < 0.1f) c = C(42, 32, 28);
      }
      break;
    }
    case Ground::Water:
    case Ground::DeepWater: {
      // semi-transparent tint over the animated water layer drawn underneath
      bool deep = g == Ground::DeepWater;
      float shore = 0;
      for (int k = 0; k < 4; k++) {
        static const int dx[4] = {3, -3, 0, 0}, dy[4] = {0, 0, 3, -3};
        Ground o = m.at((px + dx[k]) >> 4, (py + dy[k]) >> 4);
        if (o != Ground::Void && !groundWater(o) && o != Ground::Bridge) shore = 1;
      }
      c = deep ? C(28, 58, 118, 190) : C(48, 112, 168, 130);
      if (m.kind != MapKind::Overworld) c = C(30, 60, 90, 170);
      if (shore > 0 && !deep) c = C(150, 200, 220, 150);
      if (shore > 0 && hashf(px / 2, py / 2, 341) < 0.5f) c = C(220, 240, 250, 210);
      break;
    }
    case Ground::Ice:
      c = pick3(n, C(170, 210, 236), C(186, 222, 242), C(200, 232, 248));
      if (h < 0.02f) c = C(255, 255, 255);
      break;
    case Ground::Lava:
      c = pick3(n, C(220, 80, 20), C(250, 130, 30), C(255, 190, 60));
      break;
    default:
      c = C(0, 0, 0);
      break;
  }
  // ledge shading: soft ground sitting above water / lower ground gets a dark lip
  if (soft(g) && !groundWater(g) && g != Ground::CaveFloor) {
    Ground b1 = m.at(px >> 4, (py + 2) >> 4);
    if (groundWater(b1) && ((py + 2) >> 4) != (py >> 4)) c = mul(c, 0.7f);
  }
  return c;
}

void View::bakeChunk(const Map& m, int cx, int cy, Canvas& c) {
  c = Canvas(CH * 16, CH * 16);
  bakeRows(m, cx, cy, c, 0, c.h);
  bakeFinish(m, cx, cy, c);
}

void View::bakeRows(const Map& m, int cx, int cy, Canvas& c, int r0, int r1) {
  int x0 = cx * CH * 16, y0 = cy * CH * 16;
  for (int y = r0; y < r1; y++)
    for (int x = 0; x < c.w; x++) c.px[(size_t)y * c.w + x] = groundPixel(m, x0 + x, y0 + y);
}

void View::bakeFinish(const Map& m, int cx, int cy, Canvas& c) {
  // soft ambient occlusion along the base of walls and cliffs
  for (int ty = 0; ty < CH; ty++)
    for (int tx = 0; tx < CH; tx++) {
      int wx = cx * CH + tx, wy = cy * CH + ty;
      Ground g = m.at(wx, wy);
      if (isWall(g) || groundWater(g)) continue;
      bool wallAbove = isWall(m.at(wx, wy - 1));
      if (!wallAbove) continue;
      for (int y = 0; y < 5; y++)
        for (int x = 0; x < 16; x++) {
          uint32_t& p = c.px[(size_t)(ty * 16 + y) * c.w + tx * 16 + x];
          p = mul(p, 0.62f + y * 0.075f);
        }
    }
}

namespace {
uint64_t chunkKey(uint64_t mapId, int cx, int cy) { return mapId * 1000003ull ^ ((uint64_t)(uint32_t)cx << 20) ^ (uint32_t)cy; }
}  // namespace

void View::workerLoop() {
  for (;;) {
    BakeJob job;
    {
      std::unique_lock<std::mutex> lk(mu_);
      cv_.wait(lk, [&] { return quit_ || !jobs_.empty(); });
      if (quit_) return;
      job = jobs_.front();
      jobs_.pop_front();
    }
    BakeDone d;
    d.mapId = job.mapId; d.cx = job.cx; d.cy = job.cy;
    bakeChunk(*job.map, job.cx, job.cy, d.c);
    std::lock_guard<std::mutex> lk(mu_);
    done_.push_back(std::move(d));
  }
}

void View::clearChunks() {
  for (auto& ch : chunks_) pix_->destroy(ch.tex);
  chunks_.clear();
  std::lock_guard<std::mutex> lk(mu_);
  jobs_.clear();
  pending_.clear();
}

void View::prefetch(const Map& m, uint64_t mapId, Vec2 cam) {
#ifndef __EMSCRIPTEN__
  if (!worker_.joinable()) worker_ = std::thread([this] { workerLoop(); });
#endif
  if (!snap_ || snapId_ != mapId) { snap_ = std::make_shared<Map>(m); snapId_ = mapId; }
  int c0x = (int)std::floor((cam.x - 256) / 512), c0y = (int)std::floor((cam.y - 256) / 512);
  int c1x = (int)std::floor((cam.x + Pix::W + 256) / 512), c1y = (int)std::floor((cam.y + Pix::H + 256) / 512);
  std::lock_guard<std::mutex> lk(mu_);
  // keep only jobs for the current map
  for (size_t i = 0; i < jobs_.size();) {
    if (jobs_[i].mapId == mapId) { i++; continue; }
    // forget it was pending too, or coming back to that map would wait forever and bake inline (a hitch)
    uint64_t k = chunkKey(jobs_[i].mapId, jobs_[i].cx, jobs_[i].cy);
    pending_.erase(std::remove(pending_.begin(), pending_.end(), k), pending_.end());
    jobs_.erase(jobs_.begin() + i);
  }
  // finished bakes nobody will collect (another map, or a chunk already baked inline) are ~1 MB each: drop them
  for (size_t i = 0; i < done_.size();) {
    bool stale = done_[i].mapId != mapId;
    for (auto& ch : chunks_) if (ch.cx == done_[i].cx && ch.cy == done_[i].cy && ch.mapId == done_[i].mapId) stale = true;
    if (stale) {
      uint64_t k = chunkKey(done_[i].mapId, done_[i].cx, done_[i].cy);
      pending_.erase(std::remove(pending_.begin(), pending_.end(), k), pending_.end());
      done_.erase(done_.begin() + i);
    } else i++;
  }
  for (int cy = c0y; cy <= c1y; cy++)
    for (int cx = c0x; cx <= c1x; cx++) {
      if (cx < 0 || cy < 0 || cx * 32 >= m.w || cy * 32 >= m.h) continue;
      bool have = false;
      for (auto& ch : chunks_) if (ch.cx == cx && ch.cy == cy && ch.mapId == mapId) { have = true; break; }
      if (have) continue;
      uint64_t k = chunkKey(mapId, cx, cy);
      if (std::find(pending_.begin(), pending_.end(), k) != pending_.end()) continue;
      pending_.push_back(k);
      jobs_.push_back({snap_, mapId, cx, cy});
    }
  cv_.notify_one();
}

Tex View::chunkTex(const Map& m, uint64_t mapId, int cx, int cy) {
  for (auto& ch : chunks_)
    if (ch.cx == cx && ch.cy == cy && ch.mapId == mapId) { ch.used = t_; return ch.tex; }
  auto store = [&](Canvas& c) -> Tex {
    Chunk* slot = nullptr;
    if (chunks_.size() < 24) { chunks_.push_back(Chunk()); slot = &chunks_.back(); }
    else {
      slot = &chunks_[0];
      for (auto& ch : chunks_) if (ch.used < slot->used) slot = &ch;
      pix_->destroy(slot->tex);
    }
    slot->tex = pix_->bake(c);
    slot->cx = cx; slot->cy = cy; slot->mapId = mapId; slot->used = t_;
    return slot->tex;
  };
  // finished in the background?
  {
    std::unique_lock<std::mutex> lk(mu_);
    for (size_t i = 0; i < done_.size(); i++)
      if (done_[i].mapId == mapId && done_[i].cx == cx && done_[i].cy == cy) {
        BakeDone d = std::move(done_[i]);
        done_.erase(done_.begin() + i);
        uint64_t k = chunkKey(mapId, cx, cy);
        pending_.erase(std::remove(pending_.begin(), pending_.end(), k), pending_.end());
        lk.unlock();
        return store(d.c);
      }
  }
  // needed right now (teleport / first frame): bake synchronously
  Canvas c;
  auto t0 = std::chrono::steady_clock::now();
  bakeChunk(m, cx, cy, c);
  if (getenv("EMB_TIMING")) printf("chunk %d,%d baked inline in %.1f ms\n", cx, cy, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
  {
    std::lock_guard<std::mutex> lk(mu_);
    uint64_t k = chunkKey(mapId, cx, cy);
    for (size_t i = 0; i < jobs_.size();) if (jobs_[i].mapId == mapId && jobs_[i].cx == cx && jobs_[i].cy == cy) jobs_.erase(jobs_.begin() + i); else i++;
    pending_.erase(std::remove(pending_.begin(), pending_.end(), k), pending_.end());
  }
  return store(c);
}

void View::shutdown() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    quit_ = true;
  }
  cv_.notify_all();
  if (worker_.joinable()) worker_.join();
}

// Without threads (the web build) the prefetch queue is baked a few rows at a time on the main thread,
// within a per-frame time budget, so walking into new terrain never stalls a frame.
void View::pumpBake(double budgetMs) {
#ifdef __EMSCRIPTEN__
  auto t0 = std::chrono::steady_clock::now();
  for (;;) {
    if (!incrOn_) {
      std::lock_guard<std::mutex> lk(mu_);
      if (jobs_.empty()) return;
      incrJob_ = jobs_.front();
      jobs_.pop_front();
      incrCanvas_ = Canvas(CH * 16, CH * 16);
      incrRow_ = 0;
      incrOn_ = true;
    }
    int r1 = std::min(incrCanvas_.h, incrRow_ + 16);
    bakeRows(*incrJob_.map, incrJob_.cx, incrJob_.cy, incrCanvas_, incrRow_, r1);
    incrRow_ = r1;
    if (incrRow_ >= incrCanvas_.h) {
      bakeFinish(*incrJob_.map, incrJob_.cx, incrJob_.cy, incrCanvas_);
      BakeDone d;
      d.mapId = incrJob_.mapId; d.cx = incrJob_.cx; d.cy = incrJob_.cy; d.c = std::move(incrCanvas_);
      std::lock_guard<std::mutex> lk(mu_);
      done_.push_back(std::move(d));
      incrOn_ = false;
      incrJob_.map.reset();
    }
    if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() > budgetMs) return;
  }
#else
  (void)budgetMs;
#endif
}
