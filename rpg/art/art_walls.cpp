// EMBERVALE art: city walls, wall towers and gatehouses (moved out of art_building.cpp in M3), in every CityWall style.
// See rpg/art/art_building.h for the contract.
#include "rpg/art/art_internal.h"
#include "rpg/art/art_heraldry.h"

namespace art {

// ---- city wall -------------------------------------------------------------------------------------
// Walls and towers are height fields over the ground plane, rendered in the oblique view column by column, back to
// front: each ground pixel paints its top at (x, y - z), then the south-facing face below it down to where the next
// pixel's top begins. Each wall tile owns the ground pixels of its cell (plus bevel fills in the empty cells beside
// it and its tower), but renders its whole 3x3 neighbourhood so outlines and faces are decided against the real
// neighbours, then keeps only what it owns: adjacent tiles meet without seams, gaps or double-drawn pixels.
namespace {

constexpr int kBevel = 8;    // legs of the corner bevel / fill triangles, px
constexpr int kStairCut = 13, kStairFill = 3;   // M3: on a 1:1 diagonal: cut and fill legs that make one straight band
// (M3 fixer) 13 px and 11 px above the walk (were 11 and 9): on a north-south run, where no face of the wall shows,
// a tower was barely a bulge on the strip; now a drum that stands out on every run
constexpr int kTowerR = 13;  // wall tower radius, px
constexpr int kTowerZ = WALL_H + 11;

// a column-by-column oblique renderer for height fields. z(gx, gy) > 0 is solid; top() and face() pick colours.
// owner(gx, gy) marks which footprint pixels belong to this sprite (the last painter of a screen pixel owns it).
template <class ZF, class TOP, class FACE, class OWN>
void renderField(Canvas& c, std::vector<uint8_t>& own, int fx0, int fx1, int fy0, int fy1, int ox, int oy, ZF&& zf, TOP&& top,
                 FACE&& face, OWN&& owner) {
  for (int fx = fx0; fx <= fx1; fx++)
    for (int fy = fy0; fy <= fy1; fy++) {
      int z = zf(fx, fy);
      if (z <= 0) continue;
      uint8_t o = owner(fx, fy) ? 1 : 2;
      int x = ox + fx, row = oy + fy - z;
      auto put = [&](int yy, uint32_t col) {
        if (x < 0 || yy < 0 || x >= c.w || yy >= c.h) return;
        c.set(x, yy, col);
        own[(size_t)yy * c.w + x] = o;
      };
      put(row, top(fx, fy, z));
      int zn = std::max(0, zf(fx, fy + 1));
      int rowN = oy + fy + 1 - zn;
      for (int r = row + 1, v = 0; r < rowN; r++, v++) {
        int h = z - 1 - v;   // height of this face pixel above the ground
        if (h < 0) break;
        put(r, face(fx, fy, z, h, v, zn));
      }
    }
}

// keep only this sprite's pixels, plus outline pixels that touch them
Canvas cropOwned(const Canvas& big, const std::vector<uint8_t>& own, int x0, int y0, int w, int h) {
  Canvas lined = big;
  outline(lined, 0.9f);
  Canvas out(w, h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      int bx = x0 + x, by = y0 + y;
      if (bx < 0 || by < 0 || bx >= big.w || by >= big.h) continue;
      size_t i = (size_t)by * big.w + bx;
      if (own[i] == 1) { out.set(x, y, lined.px[i]); continue; }
      if (own[i] == 0 && chA(lined.px[i])) {
        bool touch = false;
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
          int nx = bx + dx[k], ny = by + dy[k];
          if (nx >= 0 && ny >= 0 && nx < big.w && ny < big.h && own[(size_t)ny * big.w + nx] == 1) touch = true;
        }
        if (touch) out.set(x, y, lined.px[i]);
      }
    }
  return out;
}

// the wall's footprint shape from a tile's 8-neighbourhood (coordinates relative to the tile's top-left, any cell of
// the 3x3 block). Cells further out are unknown and treated as empty; they never reach the pixels a tile keeps.
// M3: bits 20..28 of a wall key: which cells of the tile's 3x3 block lie on a 1:1 diagonal staircase (WallShape::stair,
// set by wallKeys from the whole map, so every tile cuts its neighbours' corners exactly as they cut them themselves)
constexpr int kStairShift = 20;
struct WallShape {
  bool c[3][3] = {};   // [cy+1][cx+1]: the cell and its 8 neighbours are wall
  bool st[3][3] = {};  // M3: ... and lie on a staircase
  WallShape() = default;
  explicit WallShape(uint32_t key) {   // a wall tile with these neighbour bits
    static const int bit[3][3] = {{7, 0, 1}, {6, -1, 2}, {5, 4, 3}};
    for (int j = 0; j < 3; j++)
      for (int i = 0; i < 3; i++) {
        c[j][i] = bit[j][i] < 0 ? true : ((key >> bit[j][i]) & 1u) != 0;
        st[j][i] = ((key >> (kStairShift + j * 3 + i)) & 1u) != 0;
      }
  }
  bool cell(int cx, int cy) const {
    if (cx < -1 || cx > 1 || cy < -1 || cy > 1) return false;
    return c[cy + 1][cx + 1];
  }
  int degree(int cx, int cy) const {
    int n = 0;
    for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && cell(cx + ox, cy + oy)) n++;
    return n;
  }
  // M3 (owner carry-over 3): a wall cell on a 1:1 diagonal staircase: its two wall neighbours across from this corner
  // (an L) and the run going on diagonally. Its outer corner is cut much deeper (and the empty cells' inner corners
  // filled much less), so the staircase reads as one straight slanted wall, as thick as a straight run, not a zigzag.
  bool stairCell(int cx, int cy) const {
    if (cx < -1 || cx > 1 || cy < -1 || cy > 1) return false;
    return st[cy + 1][cx + 1] && c[cy + 1][cx + 1];
  }
  // a staircase cell's cut corner is the one away from its L of wall neighbours; a neighbour's L is read from the
  // orthogonals this block can see (a known empty side means the partner is the other one)
  bool stairCut(int cx, int cy, int sx, int sy) const {
    if (!stairCell(cx, cy)) return false;
    auto partner = [&](int ax, int ay, int bx, int by) {   // +1 if cell (a) is the wall partner, -1 if (b)
      const bool aKnown = ax >= -1 && ax <= 1 && ay >= -1 && ay <= 1, bKnown = bx >= -1 && bx <= 1 && by >= -1 && by <= 1;
      if (aKnown && cell(ax, ay)) return 1;
      if (bKnown && cell(bx, by)) return -1;
      if (aKnown) return -1;
      return 1;
    };
    const int xp = partner(cx + 1, cy, cx - 1, cy), yp = partner(cx, cy + 1, cx, cy - 1);
    return sx == -xp && sy == -yp;
  }
  bool in(int gx, int gy) const {
    int cx = (gx >= 0 ? gx / 16 : (gx - 15) / 16), cy = (gy >= 0 ? gy / 16 : (gy - 15) / 16);
    if (cx < -1 || cx > 1 || cy < -1 || cy > 1) return false;
    int lx = gx - cx * 16, ly = gy - cy * 16;
    int kx = lx >= 8, ky = ly >= 8;
    int sx = kx ? 1 : -1, sy = ky ? 1 : -1;
    bool H = cell(cx + sx, cy), V = cell(cx, cy + sy), D = cell(cx + sx, cy + sy);
    int dx = kx ? 15 - lx : lx, dy = ky ? 15 - ly : ly;
    bool tri = dx + dy < kBevel;
    if (cell(cx, cy)) {
      if (!H && !V && !D && stairCut(cx, cy, sx, sy)) return dx + dy >= kStairCut;   // the slanted face of a staircase
      return !(tri && !H && !V && !D && degree(cx, cy) >= 2);   // bevelled outer corner
    }
    if (H && V && (stairCell(cx + sx, cy) || stairCell(cx, cy + sy))) return dx + dy < kStairFill;   // its inside face
    return tri && H && V;                                                         // filled inner corner / diagonal link
  }
  // who draws a footprint pixel: the cell itself, or for fills in an empty cell the wall cell beside it in the same row
  bool mine(int gx, int gy) const {
    if (gy < 0 || gy > 15) return false;
    if (gx >= 0 && gx <= 15) return true;
    if (gx < -16 || gx > 31) return false;
    int lx = gx < 0 ? gx + 16 : gx - 16;
    return gx < 0 ? lx >= 8 : lx < 8;   // the half of the side cell that touches this tile
  }
};

// irregular flagstones for tower floors and walkways: offset slabs with jittered joints, a few lighter and darker
int flagK(int gx, int gy, uint32_t seed) {
  int row = (gy + 64) / 4, yy = (gy + 64) % 4;
  int off = (int)(hash3(row, 0, seed) % 5);
  int w = 4 + (int)(hash3(row, 1, seed) % 3);
  int col = (gx + 64 + off) / w, xx = (gx + 64 + off) % w;
  if (yy == 3 || xx == 0) return 1;
  uint32_t h = hash3(col, row, seed + 9u);
  int k = 2;
  if (h % 4 == 0) k = 3;
  if (yy == 0 && k == 2 && h % 3 == 0) k = 3;
  return k;
}

inline float towerDist(int gx, int gy, int cx, int cy) { return std::hypot(gx + 0.5f - cx, gy + 0.5f - cy); }

// height of a round tower (centre cx, cy) at a pixel, 0 outside: crenellated rim around a stone floor
int towerZ(int gx, int gy, int cx, int cy, int r, int zTop) {
  float d = towerDist(gx, gy, cx, cy);
  if (d > r) return 0;
  if (d > r - 2.2f) {
    float a = std::atan2(gy + 0.5f - cy, gx + 0.5f - cx);
    int seg = (int)std::floor((a + PI) / TAU * 14.0f + 0.25f);
    return zTop + ((seg & 1) ? 1 : 4);
  }
  return zTop;
}

// masonry courses on a vertical face: h = height above the ground, gx = ground x. Returns a ramp index.
int masonryK(int gx, int h, uint32_t var, int base) {
  int row = h / 4, hh = h % 4;
  int off = (row & 1) * 4;
  int bx = gx + off;
  bool mortarH = hh == 3, mortarV = ((bx % 8) + 8) % 8 == 0;
  if (mortarH || mortarV) return base - 1;
  int brick = (((bx >= 0 ? bx : bx - 7) / 8) % 2 + 2) % 2;   // brick parity: identical on both sides of a tile seam
  uint32_t hsh = hash3(brick, row, 41u + var * 7u);
  int k = base;
  if (hsh % 5 == 0) k = base + 1;
  else if (hsh % 7 == 1) k = base - 1;
  if (hh == 2 && k == base) k = base + 1;   // lit upper edge of each course
  return k;
}

}  // namespace

// M3: the wall's culture style (art::CityWall) in the key: Stone (the classic curtain), Palisade (sharpened logs over
// a plank walk), Rampart (an earthen bank, grassed, a stake fence along its crest), Thorn (a living hedge of thorn),
// Adobe (mud brick with pointed merlons and beam ends), WhiteStone (elven: pale dressed stone, a smooth coping with a
// coloured band, slender towers under blue cones). Every style uses the same footprint, joins and tower places.
namespace {
const Ramp kWallAdobe = ramp5(rgba(120, 78, 60), rgba(170, 120, 84), rgba(208, 164, 112), rgba(230, 196, 142), rgba(246, 224, 180));
const Ramp kWallWhite = ramp5(rgba(124, 132, 156), rgba(180, 190, 208), rgba(222, 228, 236), rgba(240, 244, 248), rgba(255, 255, 255));
// (M3 fixer round 2) the jade kingdoms' walls: blue-grey fired brick, a coping and tower caps of green glazed tile, a
// band of red lacquer under the coping, gilded finials (CityWall::Jade)
const Ramp kWallJadeBrick = ramp5(rgba(58, 62, 74), rgba(92, 98, 110), rgba(124, 130, 140), rgba(150, 156, 164), rgba(182, 186, 190));
const Ramp kWallJadeTile = ramp5(rgba(20, 58, 48), rgba(34, 92, 70), rgba(52, 128, 92), rgba(84, 166, 116), rgba(146, 210, 160));
const Ramp kWallLacquer = ramp5(rgba(70, 18, 22), rgba(118, 28, 30), rgba(160, 40, 38), rgba(196, 64, 50), rgba(226, 104, 76));
const Ramp kWallLog = ramp5(rgba(46, 30, 34), rgba(78, 52, 42), rgba(116, 80, 56), rgba(154, 112, 76), rgba(190, 150, 104));
const Ramp kWallEarth = ramp5(rgba(54, 40, 40), rgba(88, 64, 50), rgba(120, 92, 64), rgba(150, 120, 84), rgba(178, 150, 108));
const Ramp kWallGrass = ramp5(rgba(30, 60, 46), rgba(46, 94, 54), rgba(74, 130, 58), rgba(112, 162, 66), rgba(160, 196, 92));
const Ramp kWallThorn = ramp5(rgba(20, 40, 36), rgba(30, 64, 44), rgba(48, 92, 50), rgba(78, 124, 58), rgba(124, 160, 76));
const Ramp kWallTeal = ramp5(rgba(24, 70, 90), rgba(36, 104, 124), rgba(58, 144, 156), rgba(104, 190, 192), rgba(176, 232, 226));
const Ramp kRoofBrownW = ramp5(rgba(54, 34, 38), rgba(88, 56, 46), rgba(124, 82, 58), rgba(158, 112, 74), rgba(192, 148, 100));
// (M3 fixer round 3) the sun temples' lime skin over stone (CityWall::Talud) and the red they paint its bands
const Ramp kWallLime = ramp5(rgba(132, 98, 76), rgba(184, 150, 112), rgba(222, 194, 150), rgba(238, 220, 182), rgba(250, 242, 216));
const Ramp kWallSunRed = ramp5(rgba(80, 22, 24), rgba(128, 36, 30), rgba(168, 52, 38), rgba(200, 80, 52), rgba(228, 120, 80));
const Ramp kWallSlateBlue = ramp5(rgba(34, 40, 76), rgba(50, 64, 112), rgba(74, 96, 154), rgba(108, 134, 190), rgba(158, 182, 222));
}  // namespace

Canvas wallTile(uint32_t key) {
  WallShape S(key & 255u);
  const bool tower = key & WALL_BIT_TOWER, towerN = key & WALL_BIT_TOWER_N, culvert = (key & WALL_BIT_CULVERT) && !(key & WALL_BIT_S);
  const uint32_t var = (key >> WALL_VAR_SHIFT) & 3u;
  // (M3 fixer) a span over an opening without a gatehouse (WALL_BIT_SPAN): an arch in an east-west run's south face,
  // the walk carried over the road in a north-south run
  const bool span = (key & WALL_BIT_SPAN) != 0;
  const int spos = std::clamp((int)((key >> WALL_SPAN_POS_SHIFT) & 3u), 1, 3);
  const bool vSpan = span && S.cell(0, -1) && S.cell(0, 1) && !(S.cell(-1, 0) && S.cell(1, 0));
  const bool hSpan = span && !vSpan;
  const int style = (int)((key & WALL_STYLE_MASK) >> WALL_STYLE_SHIFT);
  // (M3 fixer round 3) Talud walls take the adobe wall's form (stepped merlons, beam ends) in lime plaster
  const bool talud = style == (int)CityWall::Talud;
  const CityWall cw = talud ? CityWall::Adobe : (style < (int)CityWall::COUNT ? (CityWall)style : CityWall::Stone);
  const bool jade = cw == CityWall::Jade, ws = cw == CityWall::WhiteStone || jade;   // (jade: the white wall's form)
  const Ramp& R = talud ? kWallLime : cw == CityWall::Adobe ? kWallAdobe : (jade ? kWallJadeBrick : (cw == CityWall::WhiteStone ? kWallWhite : (cw == CityWall::Palisade ? kWallLog : kStone)));
  const Ramp& CONE = jade ? kWallJadeTile : kWallSlateBlue;
  const Ramp& BAND = jade ? kWallLacquer : kWallTeal;
  // heights of the style: the walk, the parapet's merlons and gaps, the towers
  const int wallH = cw == CityWall::Rampart ? WALL_H - 4 : (cw == CityWall::Thorn ? WALL_H - 6 : (cw == CityWall::Palisade ? WALL_H - 2 : WALL_H));
  const int tZ = cw == CityWall::Thorn ? kTowerZ - 6 : (cw == CityWall::Rampart ? kTowerZ - 2 : kTowerZ);
  // the 3x3 block plus room for heights: footprint x -16..31, y -16..31
  const int BX = 16, BY = 56, BW = 48, BH = 56 + 32 + 4;
  Canvas big(BW, BH);
  std::vector<uint8_t> own((size_t)BW * BH, 0);
  // the tower's height at a pixel, in the style: crenellated drums, wooden blockhouses with a plank floor, bushy
  // hedge-mounds, adobe drums with rounded merlons, white drums under a tall blue cone
  auto towerH = [&](int gx, int gy, int cy, int& part) -> int {   // part: 0 none, 1 floor / body top, 2 rim, 3 roof
    part = 0;
    if (talud) {   // (M3 fixer round 3) a square bastion under a crest of stepped merlons
      static const int alm[8] = {3, 5, 7, 7, 5, 3, 1, 1};
      const float ax = std::fabs(gx + 0.5f - 8), ay = std::fabs(gy + 0.5f - cy), d = std::max(ax, ay);
      if (d > kTowerR - 2) return 0;
      if (d > kTowerR - 4) { part = 2; const int along = ay > ax ? gx : gy; return tZ + alm[((along % 8) + 8) % 8]; }
      part = 1;
      return tZ;
    }
    const float d = towerDist(gx, gy, 8, cy);
    if (d > kTowerR) return 0;
    switch (cw) {
      case CityWall::WhiteStone: case CityWall::Palisade: case CityWall::Rampart: case CityWall::Jade: {   // a cone (blue slate / shingles) over the drum
        // (M3 fixer round 2: a rampart's towers are timber blockhouses too, like its stakes and gate: grey stone
        // drums on a turf bank mixed three materials on one wall)
        part = 3;
        const float t = 1 - d / kTowerR;
        if (jade) return tZ + 1 + (int)std::lround(11.0f * std::pow(t, 1.7f));   // a flared pagoda cap
        return tZ + 1 + (int)std::lround((cw == CityWall::WhiteStone ? 16.0f : 10.0f) * t);
      }
      case CityWall::Thorn: {   // a great bush: a lumpy dome
        part = 1;
        const float t = d / kTowerR;
        return tZ + (int)std::lround(7.0f * std::sqrt(std::max(0.0f, 1 - t * t)) + 2.0f * hashf(gx / 3, gy / 3, 51u + var));
      }
      case CityWall::Adobe: {
        if (d > kTowerR - 2.2f) {
          part = 2;
          float a = std::atan2(gy + 0.5f - cy, gx + 0.5f - 8.0f);
          int seg = (int)std::floor((a + PI) / TAU * 12.0f + 0.25f);
          float f = (a + PI) / TAU * 12.0f + 0.25f - std::floor((a + PI) / TAU * 12.0f + 0.25f);
          return tZ + ((seg & 1) ? 1 : (f > 0.3f && f < 0.7f ? 5 : 3));   // pointed merlons
        }
        part = 1;
        return tZ;
      }
      default: {
        int z = towerZ(gx, gy, 8, cy, kTowerR, tZ);
        part = d > kTowerR - 2.2f ? 2 : 1;
        return z;
      }
    }
  };
  auto Tw = [&](int gx, int gy, int& z, int& part) {
    if (tower) { z = towerH(gx, gy, 8, part); if (z) return 1; }
    if (towerN) { z = towerH(gx, gy, -8, part); if (z) return 2; }
    return 0;
  };
  // the distance (px, capped) from a footprint pixel to the wall's edge: the rampart's bank and the hedge's crown
  auto edgeDist = [&](int gx, int gy, int cap) {
    for (int r = 1; r <= cap; r++)
      if (!S.in(gx - r, gy) || !S.in(gx + r, gy) || !S.in(gx, gy - r) || !S.in(gx, gy + r)) return r - 1;
    return cap;
  };
  // (M3 fixer) a north-south span is roofed: a gabled roof over the passage between the two jamb towers (ridge north-
  // south, the west slope lit), so the gate reads as a covered gate passage and not as the wall run on unbroken
  auto spanRoof = [&](int gx, int gy) -> int {
    if (!vSpan || gx < 0 || gx > 15 || gy < -16 || gy > 31) return 0;
    const float d = std::fabs(gx + 0.5f - 8.0f);
    return wallH + 3 + (int)std::lround((8.0f - d) * 1.1f);
  };
  auto Z = [&](int gx, int gy) -> int {
    int tz = 0, part = 0;
    if (Tw(gx, gy, tz, part)) return tz;
    if (const int rz = spanRoof(gx, gy)) return rz;
    if (!S.in(gx, gy)) return 0;
    bool eN = !S.in(gx, gy - 1), eS = !S.in(gx, gy + 1), eW = !S.in(gx - 1, gy), eE = !S.in(gx + 1, gy);
    bool e2 = !S.in(gx, gy - 2) || !S.in(gx, gy + 2) || !S.in(gx - 2, gy) || !S.in(gx + 2, gy);
    const int along = (eN || eS || (!eW && !eE && (!S.in(gx, gy - 2) || !S.in(gx, gy + 2)))) ? gx : gy;
    // (M3 fixer) a north-south run shows no face, only its top: its parapets are a pixel broader (3 px) and its merlons
    // stand taller over deeper crenels, so the run reads as a crenellated wall with a walk between, not a paved strip
    const bool nsRun = S.in(gx, gy - 3) && S.in(gx, gy + 3) && (!S.in(gx - 3, gy) || !S.in(gx + 3, gy)) && S.in(gx - 1, gy - 3) == S.in(gx - 1, gy + 3);
    if (nsRun && (cw == CityWall::Stone || cw == CityWall::Adobe || ws)) {
      const int q = ((gy % 8) + 8) % 8;
      if (ws) return wallH + 3;
      if (cw == CityWall::Adobe) return wallH + (q == 0 || q == 7 ? 2 : (q == 1 || q == 6 ? 4 : 6));
      return wallH + (q < 4 ? 6 : 2);
    }
    switch (cw) {
      case CityWall::Rampart: {   // a bank: steep outer slopes up to a crest, a fence of stakes along the crest's middle
        const int e = edgeDist(gx, gy, 6);
        int z = 4 + e * 3;
        if (z > wallH) z = wallH;
        if (e >= 6 && ((along % 3) + 3) % 3 == 0) z = wallH + 4;   // the stakes
        return z;
      }
      case CityWall::Thorn: {   // a hedge: lumpy, rounded at the edges, a few thorny sprigs
        const int e = edgeDist(gx, gy, 4);
        const float n = vnoise(gx / 3.0f, gy / 3.0f, 77u + var);
        return wallH - (4 - e) * 2 + (int)std::lround(n * 4.0f);
      }
      case CityWall::Palisade: {   // logs along the outer edges, pointed; a plank walk behind
        if (eN || eS || eW || eE || e2) {
          const int q = ((along % 3) + 3) % 3;
          return wallH + (q == 1 ? 6 : 4);
        }
        return wallH;
      }
      case CityWall::WhiteStone: case CityWall::Jade: {   // a smooth, rounded coping with no merlons (jade: glazed tiles)
        if (eN || eS || eW || eE) return wallH + 3;
        if (e2) return wallH + 2;
        return wallH;
      }
      case CityWall::Adobe: {   // pointed (stepped) merlons
        if (eN || eS || eW || eE || e2) {
          const int q = ((along % 6) + 6) % 6;
          return wallH + (q == 0 || q == 5 ? 2 : (q == 1 || q == 4 ? 4 : 6));
        }
        return wallH;
      }
      default: {
        // parapet along every exposed edge: a 2px band, merlons 4 on / 4 off along the edge
        if (eN || eS || eW || eE || e2) {
          bool merlon = (((along % 8) + 8) % 8) < 4;
          return WALL_H + (merlon ? 5 : 2);
        }
        return WALL_H;
      }
    }
  };
  auto top = [&](int gx, int gy, int z) -> uint32_t {
    int tz = 0, part = 0;
    const int t = Tw(gx, gy, tz, part);
    int k;
    if (!t && spanRoof(gx, gy)) {   // the gate passage's roof: tiles (shingles, slates, a living thatch) in courses
      const Ramp& RR = ws ? CONE : (cw == CityWall::Thorn ? kWallThorn : (cw == CityWall::Palisade || cw == CityWall::Rampart ? kRoofBrownW : kBrick));
      const float d = gx + 0.5f - 8.0f;
      int kk = d < -0.6f ? 3 : (d > 0.6f ? 1 : 4);   // lit west slope, shaded east slope, the ridge
      if (((gy % 3) + 3) % 3 == 0 && std::fabs(d) > 0.6f) kk = std::max(0, kk - 1);   // the courses
      if (gx == 0 || gx == 15) kk = std::max(0, kk - 1);                            // the eaves
      if (std::fabs(d) > 0.6f && hash3(gx / 2, gy / 3, 47u + var) % 9 == 0) kk = std::min(4, kk + 1);
      return RR[kk];
    }
    if (t) {
      const int cy = t == 1 ? 8 : -8;
      const float d = towerDist(gx, gy, 8, cy);
      if (part == 3) {   // a cone: lit on the north-west, a ridge every few px
        const float dx = gx + 0.5f - 8, dy = gy + 0.5f - cy;
        const float l = (-dx * 0.72f - dy * 0.5f) / std::max(1.0f, d);
        int kk = l > 0.45f ? 4 : (l > 0.0f ? 3 : (l > -0.45f ? 2 : 1));
        const Ramp& C = ws ? CONE : kRoofBrownW;
        const int ring = (int)std::floor(d);
        if ((cw == CityWall::Palisade || cw == CityWall::Rampart) && ring % 3 == 0) kk = std::max(0, kk - 1);
        if (d < 1.2f) return ws ? kGold[4] : kWood[3];
        if (jade && ring % 2 == 0) kk = std::max(0, kk - 1);   // the rings of round tiles
        return C[kk];
      }
      if (cw == CityWall::Thorn) {
        const float n = hashf(gx, gy, 61u + var);
        const float dx = gx + 0.5f - 8, dy = gy + 0.5f - cy;
        int kk = (-dx - dy) / std::max(1.0f, d) > 0.3f ? 3 : 2;
        if (n < 0.15f) kk--;
        if (n > 0.9f) return (hash3(gx, gy, 7u) & 1) ? rgba(224, 200, 214) : rgba(200, 60, 70);   // hips and blossom
        return kWallThorn[std::clamp(kk, 0, 4)];
      }
      if (z > tZ) k = (gx + gy) % 3 == 0 ? 3 : 4;   // merlon caps
      else {
        k = flagK(gx, gy, 61u) + 1;
        if (d > kTowerR - 3.4f) k = std::max(1, k - 1);
        if (std::abs(gx - 9) <= 1 && std::abs(gy - cy - 1) <= 1) return kWoodDark[(gx == 8) ? 3 : 1];   // hatch
      }
      if (cw == CityWall::Palisade || cw == CityWall::Rampart) return kWood[k];
      return R[k];
    }
    switch (cw) {
      case CityWall::Rampart: {
        const int e = edgeDist(gx, gy, 6);
        if (z > wallH) return kWood[((gx + gy) & 1) ? 3 : 2];   // stake tops
        // the bank's slopes: lit facing north-west, shaded facing south-east
        const bool nw = !S.in(gx - e - 1, gy) || !S.in(gx, gy - e - 1);
        int kk = e >= 6 ? 3 : (nw ? 3 : 1);
        if (hash3(gx, gy, 81u + var) % 9 == 0) kk++;
        return kWallGrass[std::clamp(kk, 0, 4)];
      }
      case CityWall::Thorn: {
        const float n = hashf(gx, gy, 63u + var);
        const int e = edgeDist(gx, gy, 3);
        int kk = 2 + (e >= 3 ? 1 : 0);
        if (!S.in(gx - 1, gy - 1)) kk++;
        if (!S.in(gx + 1, gy + 1)) kk--;
        if (n < 0.18f) kk--;
        if (n > 0.95f) return rgba(206, 58, 66);   // haws
        if (n > 0.91f) return kWood[1];            // a thorny stem
        return kWallThorn[std::clamp(kk, 0, 4)];
      }
      case CityWall::Palisade: {
        if (z > wallH) {   // the log ends, cut to points: lit on the west of each point
          const int q = (((S.in(gx, gy - 1) && S.in(gx, gy + 1)) ? gy : gx) % 3 + 3) % 3;
          return kWallLog[q == 0 ? 4 : (q == 1 ? 3 : 1)];
        }
        // the plank walk
        k = ((gy % 4 + 4) % 4 == 3) ? 1 : 2;
        if (hash3(gx / 6, gy / 4, 23u) % 5 == 0) k = 3;
        return kWood[k];
      }
      default: break;
    }
    if (z > wallH) {
      // merlon tops and crenel sills. (M1) Along a north-south run the light from the west catches the west parapet
      // and leaves the east one in shade, so the run reads as a raised wall and not a paved strip
      k = z > wallH + 2 ? 4 : 3;
      const bool wOpen = !S.in(gx - 1, gy) || !S.in(gx - 2, gy) || !S.in(gx - 3, gy), eOpen = !S.in(gx + 1, gy) || !S.in(gx + 2, gy) || !S.in(gx + 3, gy);
      const bool nsEdge = S.in(gx, gy - 3) && S.in(gx, gy + 3);
      if (nsEdge && z <= wallH + 2) k = 1;                 // the crenels between the merlons: deep notches
      else if (nsEdge && eOpen && !wOpen) k = 2;             // the east merlons, in shade
      else if (nsEdge && wOpen && !eOpen) k = 4;             // the west merlons, lit
      if (ws && z == wallH + 3) return (jade ? kWallJadeTile : kWallTeal)[nsEdge && eOpen && !wOpen ? 2 : 3];   // the coloured coping
    } else {
      // walkway flagstones
      int row = ((gy % 4) + 4) % 4, col = (((gx + ((gy >> 2) & 1) * 2) % 5) + 5) % 5;
      k = (row == 3 || col == 0) ? 2 : 3;
      if (hash3((gx + 64) / 5, (gy + 64) / 4, 23u) % 7 == 0 && k == 3) k = 4;
      // (M1) the walkway of a north-south run lies in the east parapet's lee: its east half a shade darker
      if (S.in(gx, gy - 3) && S.in(gx, gy + 3) && (!S.in(gx + 4, gy) || !S.in(gx + 5, gy)) && S.in(gx - 6, gy)) k = std::max(1, k - 1);
    }
    // shade cast by anything taller just up-left
    int zul = Z(gx - 1, gy - 1), zu = Z(gx, gy - 1);
    if (zul > z + 1 || zu > z + 2) k = std::max(0, k - 1);
    if (zul > z + 3 && Z(gx - 2, gy - 2) > z + 3) k = std::max(0, k - 1);
    return R[k];
  };
  auto face = [&](int gx, int gy, int z, int h, int v, int zNext) -> uint32_t {
    int tz = 0, part = 0;
    const int t = Tw(gx, gy, tz, part);
    if (t) {
      // round tower: cylinder light across, its courses (or logs, or leaves), an arrow slit facing out
      float u = (gx + 0.5f - 8) / kTowerR;
      int base = lightIndex(lightAt(std::clamp(u, -0.95f, 0.95f) * 0.95f, 0.15f), gx, h, 0.12f);
      base = std::clamp(base, 1, 3);
      if (talud) {   // flat faces: lit west edge, shaded east edge, a red base course and a painted band under the crest
        base = u < -0.72f ? 3 : (u > 0.72f ? 1 : 2);
        if (h < 3) return kWallSunRed[std::max(0, base - 1)];
        if (h >= tZ - 5 && h <= tZ - 3) return kWallSunRed[(((gx + h) % 4) + 4) % 4 < 2 ? 3 : 1];
        if (v == 0 || h >= tZ - 1) return R[std::min(4, base + 1)];
        return R[std::clamp(base + ((hash3(gx / 3, h / 3, 43u + var) % 9) == 0 ? -1 : 0), 0, 4)];
      }
      if (cw == CityWall::Thorn) {
        const float n = hashf(gx, h, 67u + var);
        if (n > 0.93f) return rgba(206, 58, 66);
        return kWallThorn[std::clamp(base - (h < 4 ? 1 : 0) - (n < 0.2f ? 1 : 0), 0, 4)];
      }
      if (part == 3 && h > tZ) {   // the cone's eave and its underside
        const Ramp& C = ws ? CONE : kRoofBrownW;
        return C[std::max(0, base - 1)];
      }
      if (cw == CityWall::Palisade || cw == CityWall::Rampart) {   // a timber blockhouse: upright logs
        const int q = ((gx % 3) + 3) % 3;
        if (v == 0) return kWallLog[std::min(4, base + 1)];
        if (std::abs(gx - 8) <= 0 && h >= 9 && h <= 14 && zNext == 0) return kInk;
        return kWallLog[std::clamp(base + (q == 0 ? 1 : (q == 2 ? -1 : 0)) - (h < 3 ? 1 : 0), 0, 4)];
      }
      if (v == 0) return R[std::min(4, base + 1)];
      if (tz > tZ && h >= tZ - 1) return R[std::min(4, base + 1)];   // merlon fronts
      if (h == tZ - 2) return R[std::max(0, base - 1)];                  // shadow under the rim
      if (cw == CityWall::Adobe && h == tZ - 5 && (gx % 4 + 4) % 4 == 1) return kWood[1];   // beam ends (toron)
      if (ws && h == tZ - 3) return BAND[base];
      if (std::abs(gx - 8) <= 0 && h >= 9 && h <= 15 && zNext == 0) return kInk;            // arrow slit
      if (gx == 9 && h >= 9 && h <= 15 && zNext == 0) return R[std::max(0, base - 1)];
      if (h < 4) return R[std::max(0, base - 1 - (h == 3 ? -1 : 0))];          // plinth
      if (cw == CityWall::Adobe || ws) return R[std::clamp(base + (hash3(gx / 3, h / 3, 9u + var) % 7 == 0 ? -1 : 0), 0, 4)];
      return R[std::clamp(masonryK(gx, h, var, base), 0, 4)];
    }
    // the slant of the face under this pixel: a 1:1 diagonal running down to the right faces south-west (into the
    // light: a step lighter), one running up to the right faces south-east (a step darker)
    const bool slantSW = S.in(gx + 1, gy + 1) && !S.in(gx - 1, gy + 1), slantSE = S.in(gx - 1, gy + 1) && !S.in(gx + 1, gy + 1);
    const int slant = slantSW ? 1 : (slantSE ? -1 : 0);
    if (zNext > 0) {
      // inner face of the parapet above the walkway: in its own shade
      if (cw == CityWall::Palisade) return kWallLog[v == 0 ? 3 : 1];
      if (cw == CityWall::Rampart || cw == CityWall::Thorn) return (cw == CityWall::Thorn ? kWallThorn : kWallGrass)[v == 0 ? 2 : 1];
      return R[v == 0 ? 3 : 1];
    }
    // (M3 fixer) the arch of a span: a segmental arch across the whole opening (three tiles), a ring of voussoirs, the
    // vault dark under it and the road seen through below (a timber gateway under a lintel in a palisade)
    // (each span tile also paints the halves of its neighbours' cells next to it, as every wall tile does, so the arch
    // is placed by the position in the whole opening; the jambs paint the first and last 8 px as plain wall)
    const float spanOx = (spos - 1) * 16 + gx + 0.5f;
    if (hSpan && spanOx > 0.0f && spanOx < 48.0f && gy >= 0 && gy < 16) {
      const float ox = spanOx, u = (ox - 24.0f) / 16.0f;
      const bool timber = cw == CityWall::Palisade || cw == CityWall::Rampart;
      const int soff = timber ? wallH - 5 : (int)std::lround(10.0f + 7.0f * std::sqrt(std::max(0.0f, 1 - u * u)));
      const bool edge = ox < 9.0f || ox > 39.0f;   // the jambs' own stones stand at the opening's sides
      if (!edge) {
        if (h < soff - 5) return rgba(16, 12, 26, 150);   // the opening: the road beyond, in the vault's shade
        if (h < soff) return mix(rgba(44, 36, 54), rgba(26, 20, 34), (float)(h - (soff - 5)) / 5.0f);   // the vault
        if (timber) {
          if (h < soff + 3) return kWood[h == soff + 2 ? 4 : (h == soff ? 1 : 3)];   // the lintel
        } else if (h < soff + 3) {
          if (h == soff + 2) return R[1];
          return R[(((int)ox / 3) & 1) ? 4 : 3];                                      // voussoirs, lit from the left
        }
        if (!timber && std::fabs(ox - 24.0f) < 1.6f && h >= soff + 3 && h <= soff + 4) return R[4];   // keystone
      }
    }
    switch (cw) {
      case CityWall::Palisade: {   // upright logs, rounded, dark gaps; a band of earth heaped at their foot
        const int q = ((gx % 3) + 3) % 3;
        if (h < 2) return kWallEarth[h == 0 ? 1 : 2];
        int kk = q == 0 ? 3 : (q == 1 ? 2 : 1);
        if (q == 2 && hash3(gx / 3, h / 5, 3u + var) % 3 == 0) kk = 0;
        if (h == wallH - 4 || h == 6) kk = std::max(0, kk - 1);   // the rails lashing them
        return kWallLog[std::clamp(kk + slant, 0, 4)];
      }
      case CityWall::Rampart: {   // the bank's face: grass over earth, the earth showing low down
        if (v == 0) return kWallGrass[3];
        const uint32_t hh = hash3(gx, h, 85u + var);
        if (h < 3 || (h < 6 && hh % 3 == 0)) return kWallEarth[slant > 0 ? 3 : 2];
        return kWallGrass[std::clamp(2 + slant - (h < 7 ? 1 : 0) + (hh % 11 == 0 ? 1 : 0), 0, 4)];
      }
      case CityWall::Thorn: {
        const float n = hashf(gx, h, 69u + var);
        if (n > 0.95f) return rgba(206, 58, 66);
        if (n > 0.9f) return kWood[1];
        return kWallThorn[std::clamp(2 + slant - (h < 4 ? 1 : 0) - (n < 0.25f ? 1 : 0) + (v == 0 ? 1 : 0), 0, 4)];
      }
      default: break;
    }
    // outer face of the wall
    if (culvert && gx >= 0 && gx < 16 && gy >= 0 && gy < 16) {
      // a water gate: a round arch over the river, dark water inside behind an iron grate, a ring of voussoirs
      const float ax = gx + 0.5f - 8.0f, ay = h + 0.5f - 6.0f;
      const float rr = ax * ax + (ay > 0 ? ay * ay : 0.0f);
      const bool opening = std::fabs(ax) < 6.0f && (ay <= 0 || rr < 36.0f);
      if (opening) {
        if (h <= 1) return (gx & 1) ? rgba(92, 132, 168) : rgba(64, 100, 140);    // the river sliding out, lit
        if (gx % 3 == 1 || h == 7) return rgba(46, 44, 52);                        // grate bars
        if (gx % 3 == 2 && h > 2) return rgba(84, 82, 90);                         // lit edge of each bar
        return h > 6 ? rgba(10, 12, 22) : rgba(18, 28, 46);                         // the dark tunnel, water at its foot
      }
      const bool ring = std::fabs(ax) < 7.6f && (ay <= 0 ? std::fabs(ax) >= 6.0f : rr < 57.0f);
      if (ring) {
        int seg = (int)std::floor((std::atan2(std::max(0.0f, ay), ax) / PI) * 7.0f);
        bool joint = (gx + h + seg) % 4 == 0;
        return R[joint ? 1 : (ax < 0 ? 4 : 3)];                                    // voussoirs, lit from the left
      }
    }
    if (v == 0) return R[4];                                  // lit coping edge
    if (h >= wallH + 2) return R[std::clamp(2 + slant, 0, 4)];   // merlon fronts
    if (h == wallH + 1) return R[3];                          // cornice
    if (h == wallH) return R[1];                              // shadow line under the cornice
    if (ws && (h == wallH - 1 || h == wallH - 2)) return BAND[h == wallH - 1 ? 2 : 3];
    if (h < 3) {                                              // foundation course, darker, bigger blocks
      bool joint = ((gx + (h == 1 ? 3 : 0)) % 6 + 6) % 6 == 0;
      return R[joint ? 0 : 1];
    }
    if (h == 3) return R[3];                                  // lit lip of the plinth
    if (cw == CityWall::Adobe) {   // mud brick render: smooth, beam ends in a row, rain-worn
      if (h == wallH - 4 && (gx % 7 + 7) % 7 == 3) return kWood[1];
      if (h == wallH - 5 && (gx % 7 + 7) % 7 == 3) return R[1];
      const float n = vnoise(gx / 4.0f, h / 3.0f, 29u + var);
      return R[std::clamp((n < 0.22f ? 1 : 2) + slant, 0, 4)];
    }
    if (jade) {   // fired brick in stretcher bond, dark joints
      const int row = h / 3, hh = h % 3, bx = gx + (row & 1) * 3;
      int kk = (hh == 0 || ((bx % 6) + 6) % 6 == 0) ? 1 : (hh == 2 ? 3 : 2);
      if (hh != 0 && hash3(bx / 6, row, 37u + var) % 9 == 0) kk = std::max(1, kk - 1);
      return R[std::clamp(kk + slant, 0, 4)];
    }
    if (cw == CityWall::WhiteStone) {   // fine ashlar: long blocks, faint joints
      const int row = h / 5, hh = h % 5, bx = gx + (row & 1) * 6;
      int kk = (hh == 0 || ((bx % 12) + 12) % 12 == 0) ? 1 : (hh == 4 ? 3 : 2);
      return R[std::clamp(kk + slant, 0, 4)];
    }
    int k = masonryK(gx, h, var, 2);
    // grime and moss near the ground, rain streaks from the crenels
    if (h <= 6 && hash3(gx + 40, h, 91u + var) % 4 == 0) k = std::max(0, k - 1);
    if (var >= 2) {   // moss creeping up from the foot in soft patches
      int mh = 3 + (int)(hash3((gx + 40) / 2, 3, 17u + var) % 4) - (int)(hash3((gx + 41) / 3, 4, 19u) % 3);
      if (h <= mh && hash3((gx + 40) / 5, 6, 29u + var) % 3 == 0) return kMoss[h == mh ? 2 : 1];
    }
    if (hash3(gx + 40, 1, 33u + var) % 9 == 0 && h > 8 && h < WALL_H - 1) k = std::max(1, k - 1);
    return R[std::clamp(k + slant, 0, 4)];
  };
  auto owner = [&](int gx, int gy) -> bool {
    int tz = 0, part = 0;
    int t = Tw(gx, gy, tz, part);
    if (t == 1) return true;
    if (t == 2) return false;
    return S.mine(gx, gy);
  };
  renderField(big, own, -16, 31, -16, 31, BX, BY, Z, top, face, owner);
  // the canvas keeps x -WALL_OX..15+WALL_OX and rows from WALL_OY above the tile down to 20 below its top
  Canvas out = cropOwned(big, own, BX - WALL_OX, BY - WALL_OY, WALL_CW, WALL_CH);
  if (hSpan) {   // the outline must not close the arch at the ground line
    for (int gx = -WALL_OX; gx < 16 + WALL_OX; gx++) {
      const float ox = (spos - 1) * 16 + gx + 0.5f;
      if (ox >= 9.0f && ox <= 39.0f) out.set(WALL_OX + gx, WALL_OY + 16, 0);
    }
  }
  if (vSpan) {
    // the vault's mouths in the wall's flanks, either side of the road: dark recesses framed by the jambs' quoins (lit
    // on the west, in shade on the east), deepest right against the wall
    for (int ly = 0; ly < 16; ly++) {
      const int yy = (spos - 1) * 16 + ly;
      const bool quoin = yy < 3 || yy > 44;
      for (int k = 1; k <= 4; k++) {
        const int xw = WALL_OX - k, xe = WALL_OX + 15 + k;
        // the mouth's ends round off like an arch seen from the side: the quoins step out toward the passage's middle
        const int inset = yy < 3 || yy > 44 ? 0 : (yy < 5 || yy > 42 ? 1 : 0);
        if (quoin || k > 4 - inset) {
          if (k == 4) continue;
          out.set(xw, WALL_OY + ly, k == 3 ? kInk : R[(yy == 2 || yy == 45) ? 2 : 4]);
          out.set(xe, WALL_OY + ly, k == 3 ? kInk : R[(yy == 2 || yy == 45) ? 0 : 1]);
        } else {
          const uint32_t dk = k == 1 ? rgba(16, 12, 22) : (k == 2 ? rgba(26, 20, 34) : (k == 3 ? rgba(38, 32, 46) : rgba(56, 48, 62)));
          out.set(xw, WALL_OY + ly, dk);
          out.set(xe, WALL_OY + ly, dk);
        }
      }
    }
  }
  return out;
}


Canvas wallPiece(int mask) {
  bool n = mask & 1, e = mask & 2, s = mask & 4, w = mask & 8;
  uint32_t k = (n ? WALL_BIT_N : 0) | (e ? WALL_BIT_E : 0) | (s ? WALL_BIT_S : 0) | (w ? WALL_BIT_W : 0);
  if (n && e) k |= WALL_BIT_NE;
  if (s && e) k |= WALL_BIT_SE;
  if (s && w) k |= WALL_BIT_SW;
  if (n && w) k |= WALL_BIT_NW;
  if ((int)n + (int)e + (int)s + (int)w <= 1) k |= WALL_BIT_TOWER;
  return wallTile(k);
}

// ---- gatehouse ---------------------------------------------------------------------------------------
Canvas gateHouse(uint32_t seed) { return gateHouse(seed, 0, 0, 0); }
// M1: the gatehouse flies its kingdom's colours: the banners on its towers and the arms over the arch carry the field,
// the trim and the charge (field == 0: the old red and gold)
Canvas gateHouse(uint32_t seed, uint32_t field, uint32_t trim, int emblem) { return gateHouse(seed, field, trim, emblem, CityWall::Stone); }
// M3: the gatehouse in a culture's wall style, the same canvas, anchor, towers and passage as the stone one: timber
// towers under shingle cones and a log gate block (palisade, rampart), two great thorn bushes and a living arch
// (thorn), mud-brick towers with pointed merlons and beam ends (adobe), white towers under blue cones and a pointed
// arch (the high elves)
Canvas gateHouse(uint32_t seed, uint32_t field, uint32_t trim, int emblem, CityWall cw) {
  // (M3 fixer round 3) the sun temples' gate: two square pylons stepping up in two tiers under a crest of stepped
  // merlons, lime plaster with red-painted bands and a step-fret frieze, the passage under a corbelled (stepped) arch;
  // the adobe gate's code paths draw the rest (beam ends, banners, lanterns)
  const bool talud = cw == CityWall::Talud;
  if (talud) cw = CityWall::Adobe;
  static const int kAlm[8] = {4, 6, 8, 8, 6, 4, 2, 2};
  const bool kc = field != 0;
  const Ramp BF = kc ? ramp(opaque(field)) : kRed;
  const Ramp BT = kc ? ramp(opaque(trim ? trim : rgba(232, 200, 90))) : kGold;
  // footprint x -16..63 (flank tile, three passage tiles, flank tile), y 0..15; towers centred on the flank tiles
  const bool timber = cw == CityWall::Palisade || cw == CityWall::Rampart, thorn = cw == CityWall::Thorn;
  const bool jade = cw == CityWall::Jade, ws = cw == CityWall::WhiteStone || jade;
  const bool cone = timber || ws;
  const Ramp& R = talud ? kWallLime : cw == CityWall::Adobe ? kWallAdobe : (jade ? kWallJadeBrick : (cw == CityWall::WhiteStone ? kWallWhite : (timber ? kWallLog : (thorn ? kWallThorn : kStone))));
  const Ramp& CONE = jade ? kWallJadeTile : (cw == CityWall::WhiteStone ? kWallSlateBlue : kRoofBrownW);
  const Ramp& BAND = jade ? kWallLacquer : kWallTeal;
  const int BX = GATE_OX, BY = GATE_OY;
  Canvas c(GATE_CW, GATE_CH);
  std::vector<uint8_t> own((size_t)GATE_CW * GATE_CH, 0);
  const int TR = 13, TZ = WALL_H + 16, BZ = WALL_H + 10;   // tower radius / height, gate block height
  const int t0x = -8, t1x = 56, tcy = 8;
  const int ax0 = 3, ax1 = 44, archH = 24;   // the arch opening on the front face (ground x range, height)
  // a tower's height in the style: crenellated (stone, adobe with pointed merlons), under a cone (timber, white
  // stone), or a great rounded bush (thorn)
  constexpr int PY = 12, PT = 9;   // talud pylon half-size and its upper tier's
  auto towerAt = [&](int gx, int gy, int cx) -> int {
    if (talud) {
      const float ax = std::fabs(gx + 0.5f - cx), ay = std::fabs(gy + 0.5f - tcy), d = std::max(ax, ay);
      if (d > PY) return 0;
      if (d > PT) return TZ - 14;   // the lower tier's terrace
      if (d > PT - 2) { const int along = ay > ax ? gx : gy; return TZ + kAlm[((along % 8) + 8) % 8]; }   // the crest
      return TZ;
    }
    const float d = towerDist(gx, gy, cx, tcy);
    if (d > TR) return 0;
    if (jade) return TZ + 1 + (int)std::lround(12.0f * std::pow(1 - d / TR, 1.7f));   // a flared pagoda cap
    if (cone) return TZ + 1 + (int)std::lround((cw == CityWall::WhiteStone ? 17.0f : 11.0f) * (1 - d / TR));
    if (thorn) return TZ - 8 + (int)std::lround(8.0f * std::sqrt(std::max(0.0f, 1 - (d / TR) * (d / TR))) + 2.0f * hashf(gx / 3, gy / 3, 53u));
    if (cw == CityWall::Adobe && d > TR - 2.2f) {
      const float a = (std::atan2(gy + 0.5f - tcy, gx + 0.5f - cx) + PI) / TAU * 14.0f + 0.25f;
      const float f = a - std::floor(a);
      return TZ + (((int)std::floor(a) & 1) ? 1 : (f > 0.3f && f < 0.7f ? 5 : 3));
    }
    return towerZ(gx, gy, cx, tcy, TR, TZ);
  };
  auto Z = [&](int gx, int gy) -> int {
    int z = std::max(towerAt(gx, gy, t0x), towerAt(gx, gy, t1x));
    if (z) return z;
    if (gx < 0 || gx > 47 || gy < 1 || gy > 15) return 0;
    // gate block: walkway with a parapet front and back
    if (gy <= 2 || gy >= 14) {
      if (talud) return BZ + kAlm[((gx % 8) + 8) % 8];
      if (thorn) return BZ - 4 + (int)std::lround(3.0f * vnoise(gx / 3.0f, gy / 2.0f, 55u));
      if (timber) return BZ + (gx % 3 == 1 ? 6 : 4);
      if (ws) return BZ + 3;
      if (cw == CityWall::Adobe) { const int q = ((gx % 6) + 6) % 6; return BZ + (q == 0 || q == 5 ? 2 : (q == 1 || q == 4 ? 4 : 6)); }
      return BZ + (((gx % 8) < 4) ? 5 : 2);
    }
    return thorn ? BZ - 5 : BZ;
  };
  auto which = [&](int gx, int gy) { return towerAt(gx, gy, t0x) ? 0 : (towerAt(gx, gy, t1x) ? 1 : 2); };
  auto archTop = [&](int gx) {   // height of the arch soffit above ground at this x, -1 outside the opening
    if (gx < ax0 || gx > ax1) return -1;
    float u = (gx + 0.5f - (ax0 + ax1 + 1) * 0.5f) / ((ax1 - ax0 + 1) * 0.5f);
    if (timber) return archH - 3;   // a square timber gateway under a lintel
    if (talud) return std::min(archH + 1, archH - 10 + ((int)std::floor((1 - std::fabs(u)) * 24.0f) / 3) * 3);   // corbelled
    if (cw == CityWall::WhiteStone) return (int)std::lround(archH - 9 + 11 * std::pow(std::max(0.0f, 1 - std::fabs(u)), 0.55f));   // pointed
    return (int)std::lround(archH - 8 + 8 * std::sqrt(std::max(0.0f, 1 - u * u)));
  };
  auto top = [&](int gx, int gy, int z) -> uint32_t {
    int w = which(gx, gy);
    if (talud) {
      if (w < 2) {
        const int cx = w == 0 ? t0x : t1x;
        if (z > TZ) return R[4];
        if (z == TZ) return R[(gx - cx + gy) % 5 == 0 ? 2 : 3];
        // the lower terrace: lit on its north and west rims
        return R[(gy < tcy - PT || gx < cx - PT) ? 4 : 3];
      }
      if (z > BZ) return R[4];
      return R[((gy % 4) == 3 || ((gx + (gy / 4) * 2) % 6) == 0) ? 2 : 3];
    }
    if (w < 2) {
      int cx = w == 0 ? t0x : t1x;
      float d = towerDist(gx, gy, cx, tcy);
      if (cone) {   // the cone: lit on the north-west, a gilded / wooden finial at its point
        const float dx = gx + 0.5f - cx, dy = gy + 0.5f - tcy;
        const float l = (-dx * 0.72f - dy * 0.5f) / std::max(1.0f, d);
        int kk = l > 0.45f ? 4 : (l > 0.0f ? 3 : (l > -0.45f ? 2 : 1));
        if (d < 1.2f) return ws ? kGold[4] : kWood[3];
        if (timber && ((int)std::floor(d)) % 3 == 0) kk = std::max(0, kk - 1);
        if (jade && ((int)d) % 2 == 0) kk = std::max(0, kk - 1);   // the rings of round tiles
        return CONE[kk];
      }
      if (thorn) {
        const float n = hashf(gx, gy, 65u);
        if (n > 0.93f) return rgba(206, 58, 66);
        const float dx = gx + 0.5f - cx, dy = gy + 0.5f - tcy;
        return R[std::clamp(((-dx - dy) / std::max(1.0f, d) > 0.3f ? 3 : 2) - (n < 0.18f ? 1 : 0), 0, 4)];
      }
      if (z > TZ) return R[(gx + gy) % 3 == 0 ? 3 : 4];
      int k = flagK(gx, gy, 62u) + 1;
      if (d > TR - 3.4f) k = std::max(1, k - 1);
      int zul = Z(gx - 1, gy - 1);
      if (zul > z + 1) k = std::max(0, k - 1);
      return R[k];
    }
    if (thorn) { const float n = hashf(gx, gy, 66u); if (n > 0.94f) return rgba(206, 58, 66); return R[std::clamp(3 - (n < 0.2f ? 1 : 0) - (gy >= 12 ? 1 : 0), 0, 4)]; }
    if (timber && z > BZ) return kWallLog[gx % 3 == 0 ? 4 : (gx % 3 == 1 ? 3 : 1)];
    if (timber) return kWood[((gy % 4) == 3) ? 1 : 2];
    if (ws && z > BZ) return (jade ? kWallJadeTile : kWallTeal)[3];
    if (z > BZ + 2) return R[4];
    if (z > BZ) return R[3];
    int k = ((gy % 4) == 3 || ((gx + (gy / 4) * 2) % 5) == 0) ? 2 : 3;
    if (Z(gx - 1, gy - 1) > z + 1 || Z(gx, gy - 1) > z + 2) k--;
    return R[k];
  };
  auto face = [&](int gx, int gy, int z, int h, int v, int zNext) -> uint32_t {
    int w = which(gx, gy);
    if (talud) {
      // a step-fret (greca) frieze: a meander of two reds in a band
      auto greca = [&](int u, int hh, int b0) { const int q = ((u % 8) + 8) % 8, r = hh - b0; return (r == 0 || r == 3 || (q == 0 && r < 3) || (q == 4 && r > 0) || (q < 4 && r == 2 && q > 0) || (q > 4 && r == 1)) ? kWallSunRed[3] : kWallSunRed[1]; };
      if (w < 2) {
        const int cx = w == 0 ? t0x : t1x;
        const float ux = gx + 0.5f - cx;
        int base = ux < -PY + 2.5f ? 3 : (ux > PY - 2.5f ? 1 : 2);
        if ((std::fabs(ux) > PT && z == TZ - 14) || h < TZ - 14) {   // the lower tier (talud): the red-painted base course
          if (zNext > 0 && h >= TZ - 14) return R[base];
          if (h < 3) return kWallSunRed[std::max(0, base - 1)];
          if (h == TZ - 15) return R[std::min(4, base + 1)];   // the cornice of the lower tier
          return R[std::clamp(base + ((hash3(gx / 3, h / 3, 31u) % 9) == 0 ? -1 : 0), 0, 4)];
        }
        if (v == 0 || h >= TZ - 1) return R[std::min(4, base + 1)];
        if (h >= TZ - 6 && h <= TZ - 3) return greca(gx, h, TZ - 6);   // the tablero's painted frieze
        if (h == TZ - 7) return R[std::max(0, base - 1)];
        if (zNext == 0 && std::fabs(ux) < 1.0f && h >= TZ - 12 && h <= TZ - 9) return kInk;   // a small dark window
        return R[std::clamp(base + ((hash3(gx / 3, h / 3, 37u) % 11) == 0 ? -1 : 0), 0, 4)];
      }
      if (zNext > 0) return R[v == 0 ? 3 : 1];
      const int at = archTop(gx);
      if (at >= 0 && h < at) return 0;
      if (v == 0 || h >= BZ - 1) return R[4];
      if (h >= BZ - 6 && h <= BZ - 3) return greca(gx, h, BZ - 6);
      if (h == BZ - 7) return R[1];
      if (at >= 0 && h >= at && h < at + 2) return R[h == at ? 1 : 3];   // the corbel steps' soffits and lips
      if (h < 3) return kWallSunRed[2];
      if (h == 3) return R[3];
      return R[std::clamp(2 + ((hash3(gx / 3, h / 3, 41u) % 9) == 0 ? -1 : 0), 0, 4)];
    }
    if (w < 2) {
      int cx = w == 0 ? t0x : t1x;
      float u = (gx + 0.5f - cx) / TR;
      int base = std::clamp(lightIndex(lightAt(std::clamp(u, -0.95f, 0.95f) * 0.95f, 0.15f), gx, h, 0.12f), 1, 3);
      if (thorn) { const float n = hashf(gx, h, 68u); if (n > 0.95f) return rgba(206, 58, 66); return R[std::clamp(base - (h < 4 ? 1 : 0) - (n < 0.2f ? 1 : 0), 0, 4)]; }
      if (cone && h > TZ) return CONE[std::max(0, base - 1)];   // the cone's eave
      if (timber) {
        if (zNext == 0 && std::abs(gx - cx) <= 0 && h >= 12 && h <= 18) return kInk;
        const int q = ((gx % 3) + 3) % 3;
        return kWallLog[std::clamp(base + (q == 0 ? 1 : (q == 2 ? -1 : 0)) - (h < 3 ? 1 : 0) - (h == 10 || h == TZ - 4 ? 1 : 0), 0, 4)];
      }
      if (ws && (h == TZ - 3 || h == TZ - 4)) return BAND[base];
      if (cw == CityWall::Adobe && h == TZ - 6 && (gx % 4 + 4) % 4 == 1) return kWood[1];
      if (v == 0 || (z > TZ && h >= TZ - 1)) return R[std::min(4, base + 1)];
      if (h == TZ - 2) return R[base - 1];
      if (zNext == 0 && std::abs(gx - cx) <= 0 && ((h >= 22 && h <= 28) || (h >= 10 && h <= 15))) return kInk;   // arrow slits
      if (zNext == 0 && gx - cx == 1 && ((h >= 22 && h <= 28) || (h >= 10 && h <= 15))) return R[base - 1];
      if (h < 3) return R[base - 1];
      if (h == 3) return R[std::min(4, base + 1)];
      return R[std::clamp(masonryK(gx, h, seed & 3u, base), 0, 4)];
    }
    if (zNext > 0) return R[v == 0 ? 3 : 1];
    // front face of the gate block with the arch
    int at = archTop(gx);
    if (at >= 0 && h < at) return 0;   // the opening: see through to the passage
    if (thorn) { const float n = hashf(gx, h, 70u); if (n > 0.95f) return rgba(206, 58, 66); if (n > 0.9f) return kWood[1]; return R[std::clamp(2 - (h < 4 ? 1 : 0) - (n < 0.25f ? 1 : 0) + (v == 0 ? 1 : 0), 0, 4)]; }
    if (timber) {
      if (at >= 0 && h >= at && h < at + 3) return kWood[h == at + 2 ? 4 : (h == at ? 1 : 3)];   // the lintel beam
      if (gx == ax0 - 1 || gx == ax1 + 1 || gx == ax0 - 2 || gx == ax1 + 2) return kWood[gx < 24 ? 3 : 1];   // the gate posts
      const int q = ((gx % 3) + 3) % 3;
      if (h < 2) return kWallEarth[2];
      return kWallLog[std::clamp((q == 0 ? 3 : (q == 1 ? 2 : 1)) - (h == BZ - 4 || h == 8 ? 1 : 0), 0, 4)];
    }
    if (ws && (h == BZ - 1 || h == BZ - 2) && v > 0) return BAND[h == BZ - 1 ? 2 : 3];
    if (cw == CityWall::Adobe && h == BZ - 3 && (gx % 7 + 7) % 7 == 3) return kWood[1];
    if (v == 0) return R[4];
    if (h >= BZ + 2) return R[2];
    if (h == BZ + 1) return R[3];
    if (h == BZ) return R[1];
    // voussoirs: a ring of light and dark wedges around the arch
    if (at >= 0 && h >= at && h < at + 3) {
      int wedge = ((gx - ax0) / 3) & 1;
      if (h == at + 2) return R[1];
      return R[wedge ? 4 : 3];
    }
    if (std::abs(gx - 24) <= 1 && at >= 0 && h >= at + 3 && h <= at + 4) return R[4];   // keystone
    if (h < 3) return R[1];
    if (h == 3) return R[3];
    return R[std::clamp(masonryK(gx, h, seed & 3u, 2), 0, 4)];
  };
  auto owner = [&](int, int) { return true; };
  // the field must reach the bottom of the round towers (tcy + TR = 21), or their lower front is never painted
  renderField(c, own, -16 - 8, 63 + 8, -8, tcy + TR + 1, BX, BY, Z, top, face, owner);
  // the passage: dark vault in the upper part of the opening, the raised portcullis teeth, a lit floor beyond
  for (int gx = ax0; gx <= ax1; gx++) {
    int at = archTop(gx);
    int x = BX + gx;
    int frontBase = BY + 15;   // ground row of the front face
    for (int h = at - 1; h >= 0; h--) {
      int y = frontBase - h;
      uint32_t col;
      int depth = at - 1 - h;   // rows below the soffit
      if (depth < 7) col = mix(rgba(30, 24, 40), rgba(44, 36, 54), depth / 7.0f);   // vault in shadow
      else continue;          // lower part: see through to the passage floor (terrain)
      c.set(x, y, col);
    }
    // portcullis teeth hanging just under the soffit (a timber gate: its two leaves standing open against the posts)
    if (talud) continue;   // an open passage: no portcullis
    if (timber || thorn) {
      if (gx - ax0 < 5 || ax1 - gx < 5)
        for (int h = 0; h < at - 1; h++) c.set(x, frontBase - h, kWood[((gx - ax0) % 2 == 0) ? 2 : 3 - (h % 6 == 0 ? 2 : 0)]);
      continue;
    }
    if ((gx - ax0) % 3 == 1) for (int t = 0; t < 4; t++) c.set(x, frontBase - (at - 1) + t, t == 3 ? kIron[3] : kIron[1]);
    c.set(x, frontBase - (at - 1) + 2, kIron[2]);
  }
  // coat of arms over the arch, banners on the towers, lanterns either side of the opening
  {
    int sx = BX + 21, sy = BY + 15 - (archH + 9);
    for (int j = 0; j < 7; j++)
      for (int i = 0; i < 6; i++) {
        if (j >= 5 && (i == 0 || i == 5)) continue;
        if (j == 6 && (i == 1 || i == 4)) continue;
        c.set(sx + i, sy + j, (i == 0 || j == 0) ? BF[3] : (i == 5 || j == 6 ? BF[1] : BF[2]));
      }
    if (kc) {
      for (int j = 0; j < 5; j++)
        for (int i = 0; i < 5; i++)
          if (heraldry::chargeAt(emblem, 5, i, j)) c.set(sx + i, sy + 1 + j, BT[heraldry::chargeShade(emblem, 5, i, j)]);
    } else {
      c.set(sx + 2, sy + 2, kGold[4]); c.set(sx + 3, sy + 2, kGold[3]); c.set(sx + 2, sy + 3, kGold[3]); c.set(sx + 3, sy + 3, kGold[2]);
      c.set(sx + 2, sy + 4, kGold[2]);
    }
  }
  for (int side = 0; side < 2; side++) {
    int cx = side == 0 ? t0x : t1x;
    int bx = BX + cx - 3, by = BY + 15 - (TZ - 6);
    for (int j = 0; j < 16; j++)
      for (int i = 0; i < 6; i++) {
        if (j >= 14 && (i == 2 || i == 3)) continue;
        if (j == 15 && (i == 1 || i == 4)) continue;
        int k = i == 0 ? 3 : (i == 5 ? 1 : 2);
        if (j == 0) k = 1;
        uint32_t col = BF[k];
        if (kc && (i == 0 || i == 5)) col = BT[i == 0 ? 2 : 1];   // the trim down its edges
        if (kc && heraldry::chargeAt(emblem, 5, i, j - 4)) col = BT[std::max(1, heraldry::chargeShade(emblem, 5, i, j - 4) - (i >= 4 ? 1 : 0))];
        c.set(bx + i, by + j, col);
      }
    hline(c, bx - 1, bx + 6, by - 1, kGold[3]);
    if (!kc) {
      c.set(bx + 2, by + 5, kGold[4]); c.set(bx + 3, by + 5, kGold[3]); c.set(bx + 2, by + 6, kGold[3]); c.set(bx + 3, by + 6, kGold[2]);
      c.set(bx + 1, by + 6, kGold[2]); c.set(bx + 4, by + 6, kGold[1]); c.set(bx + 2, by + 7, kGold[2]); c.set(bx + 3, by + 7, kGold[1]);
    }
    // lantern on a bracket by the arch
    int lx = BX + (side == 0 ? ax0 - 3 : ax1 + 3), ly = BY + 15 - 16;
    c.set(lx, ly - 1, kIron[2]); c.set(lx, ly, kGlow[4]); c.set(lx, ly + 1, kGlow[2]); c.set(lx - 1, ly, kIron[1]); c.set(lx + 1, ly, kIron[1]);
  }
  outline(c, 0.9f);
  // the outline must not close the opening at the ground line
  for (int gx = ax0; gx <= ax1; gx++) c.set(BX + gx, BY + 16, 0);
  return c;
}

Canvas gatePiece() { return gateHouse(0); }

// M3: a wall tile on a 1:1 diagonal staircase (the whole map known): an outer corner with the L of wall across from it
// and the run going on diagonally (WallShape::stairCut, kStairShift)
template <class AT>
bool stairFull(AT&& at, int x, int y) {
  if (!at(x, y)) return false;
  for (int sy = -1; sy <= 1; sy += 2)
    for (int sx = -1; sx <= 1; sx += 2)
      if (at(x - sx, y) && at(x, y - sy) && !at(x + sx, y) && !at(x, y + sy) && !at(x + sx, y + sy) && (at(x + sx, y - sy) || at(x - sx, y + sy)))
        return true;
  return false;
}

int wallShadeAt(const uint8_t* wall, int W, int H, int px, int py) {
  auto at = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H && wall[(size_t)y * W + x] != 0; };
  // the wall's ground shape at a pixel: build the 3x3 neighbourhood of the pixel's cell
  auto shapeAt = [&](int gx, int gy) {
    int cx = gx >= 0 ? gx / 16 : (gx - 15) / 16, cy = gy >= 0 ? gy / 16 : (gy - 15) / 16;
    WallShape S;
    bool any = false;
    for (int j = -1; j <= 1; j++)
      for (int i = -1; i <= 1; i++) { S.c[j + 1][i + 1] = at(cx + i, cy + j); S.st[j + 1][i + 1] = stairFull(at, cx + i, cy + j); any = any || S.c[j + 1][i + 1]; }
    return any && S.in(gx - cx * 16, gy - cy * 16);
  };
  if (shapeAt(px, py)) return 0;
  // contact shade right at the foot of the wall face, then the cast shadow, which falls down-right
  if (shapeAt(px, py - 1) || shapeAt(px - 1, py - 1)) return 2;
  // (M1) the walls stand WALL_H px tall: the shadow reaches about half that down-right, so a north-south run casts a
  // band of shade along its east foot (it read as a flat road before, with a 6 px sliver)
  static const int sx[11] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, sy[11] = {1, 1, 2, 3, 3, 4, 4, 5, 5, 6, 6};
  for (int k = 0; k < 11; k++)
    if (shapeAt(px - sx[k], py - sy[k])) return 1;
  return 0;
}

void wallKeys(const uint8_t* wall, int W, int H, const std::pair<int, int>* gates, int nGates, std::vector<uint32_t>& keys) {
  keys.assign((size_t)W * H, 0);
  auto at = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H && wall[(size_t)y * W + x] != 0; };
  std::vector<uint8_t> flank((size_t)W * H, 0), tower((size_t)W * H, 0);
  std::vector<std::pair<int, int>> marks;   // towers and gate centres placed so far (spacing)
  for (int i = 0; i < nGates; i++) {
    int gx = gates[i].first, gy = gates[i].second;
    // 1 = hidden under the gatehouse tower; 2 = still drawn: a flank joined to the ring only diagonally keeps its
    // wall piece (and the diagonal link it paints), or a sliver of ground shows between the link and the tower
    if (at(gx - 1, gy)) flank[(size_t)gy * W + gx - 1] = at(gx - 2, gy) ? 1 : 2;
    if (at(gx + 3, gy)) flank[(size_t)gy * W + gx + 3] = at(gx + 4, gy) ? 1 : 2;
    marks.push_back({gx + 1, gy});
  }
  const int nGateMarks = (int)marks.size();
  auto degree = [&](int x, int y) {
    int n = 0;
    for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && at(x + ox, y + oy)) n++;
    return n;
  };
  auto clear = [&](int x, int y, int towerGap, int gateGap) {
    for (int i = 0; i < (int)marks.size(); i++) {
      int d = std::max(std::abs(marks[i].first - x), std::abs(marks[i].second - y));
      if (d < (i < nGateMarks ? gateGap : towerGap)) return false;
    }
    return true;
  };
  auto place = [&](int x, int y) { tower[(size_t)y * W + x] = 1; marks.push_back({x, y}); };
  // 1. every wall end (and lone pier) gets a tower. An end is a tile whose wall neighbours all lie within one 90-degree
  //    arc of its 8-neighbourhood (three consecutive cells): the last tile of a run that steps diagonally into the
  //    opening has two or three such neighbours, not one, and without a tower it ends in a bare wedge or stub.
  static const int cdx[8] = {0, 1, 1, 1, 0, -1, -1, -1}, cdy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  auto isEnd = [&](int x, int y) {
    int mask = 0;
    for (int b = 0; b < 8; b++) if (at(x + cdx[b], y + cdy[b])) mask |= 1 << b;
    if (!mask) return true;
    // arcs centred on an orthogonal neighbour only: {W, S} is a corner of the ring, not an end
    for (int c = 0; c < 8; c += 2) {
      int arc = (1 << c) | (1 << ((c + 1) & 7)) | (1 << ((c + 7) & 7));
      if ((mask & ~arc) == 0) return true;
    }
    return false;
  };
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      if (at(x, y) && !flank[(size_t)y * W + x] && isEnd(x, y)) place(x, y);
  // two end towers on touching tiles would merge into one blob: keep the one further into the run
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!tower[(size_t)y * W + x]) continue;
      for (int b = 0; b < 4; b++) {   // E, SE, S, SW neighbours (each pair is visited once)
        static const int ndx2[4] = {1, 1, 0, -1}, ndy2[4] = {0, 1, 1, 1};
        int nx = x + ndx2[b], ny = y + ndy2[b];
        if (nx < 0 || ny < 0 || nx >= W || ny >= H || !tower[(size_t)ny * W + nx]) continue;
        if (degree(nx, ny) >= degree(x, y)) tower[(size_t)y * W + x] = 0; else tower[(size_t)ny * W + nx] = 0;
      }
    }
  // 2. strong corners: an L whose two arms run straight for 3+ tiles
  struct Cand { int score, x, y; };
  std::vector<Cand> cands;
  auto run = [&](int x, int y, int dx, int dy) {
    int n = 0;
    for (int k = 1; k <= 6; k++) {
      int px = x + dx * k, py = y + dy * k;
      if (!at(px, py)) break;
      bool straight = dx ? (!at(px, py - 1) && !at(px, py + 1)) : (!at(px - 1, py) && !at(px + 1, py));
      if (!straight) break;
      n++;
    }
    return n;
  };
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!at(x, y) || flank[(size_t)y * W + x] || tower[(size_t)y * W + x]) continue;
      bool e = at(x + 1, y), w = at(x - 1, y), n = at(x, y - 1), s = at(x, y + 1);
      if ((e == w) || (n == s)) continue;
      int a = run(x, y, e ? 1 : -1, 0), b = run(x, y, 0, s ? 1 : -1);
      if (a >= 3 && b >= 3) cands.push_back({a + b, x, y});
    }
  std::stable_sort(cands.begin(), cands.end(), [](const Cand& p, const Cand& q) { return p.score > q.score; });
  for (const Cand& c : cands)
    if (clear(c.x, c.y, 6, 4)) place(c.x, c.y);
  // 3. long runs: a tower wherever nothing stands within 9 tiles
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!at(x, y) || flank[(size_t)y * W + x] || tower[(size_t)y * W + x]) continue;
      bool e = at(x + 1, y), w = at(x - 1, y), n = at(x, y - 1), s = at(x, y + 1);
      bool straight = (e && w && !n && !s) || (n && s && !e && !w);
      if (straight && degree(x, y) == 2 && clear(x, y, 10, 5)) place(x, y);
    }
  // keys
  static const int ndx[8] = {0, 1, 1, 1, 0, -1, -1, -1}, ndy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      if (!at(x, y) || flank[(size_t)y * W + x] == 1) continue;
      uint32_t k = 0;
      for (int b = 0; b < 8; b++) if (at(x + ndx[b], y + ndy[b])) k |= 1u << b;
      if (tower[(size_t)y * W + x]) k |= WALL_BIT_TOWER;
      if (y > 0 && tower[(size_t)(y - 1) * W + x]) k |= WALL_BIT_TOWER_N;
      k |= (hash3(x, y, 777u) & 3u) << WALL_VAR_SHIFT;
      for (int j = -1; j <= 1; j++)   // M3: which cells of the 3x3 block lie on a staircase
        for (int i = -1; i <= 1; i++)
          if (stairFull(at, x + i, y + j)) k |= 1u << (kStairShift + (j + 1) * 3 + (i + 1));
      keys[(size_t)y * W + x] = k;
    }
}

}  // namespace art
