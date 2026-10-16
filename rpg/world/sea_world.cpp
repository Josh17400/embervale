// M10 Beyond the Mist (VISION_PLAN 2.12, 13 M10, 15.4, 15.31): the lands, the Mist, the seas and the ports.
// WORLD lane (rpg/world/source.h is the contract). Pure functions of (seed, x, y) in integer / Q16 maths.
//
// The lands. One LCELL (6144-tile) cell holds one land: Continent, Archipelago or Ocean (phase A's draw, rawDraw). The
// draw is shared by continent() (macro.cpp, which paints the elevation) and every question asked here, so the two never
// disagree. On top of the raw draw:
//   - the HOME RING (the 5 x 5 cells round the origin; the 8 round it carry the per-seed guarantees): a second continent
//     across a strait of open sea 1500-4000 tiles wide from the mainland's coast, the cells either side of the strait
//     left to the sea, and a hidden land a few thousand tiles out on another bearing, clear of everything;
//   - a HIDDEN land is drawn in unwarped space (so its Mist ring is a true ring round its centre) as a lobed blob, and is
//     shrunk until no other land comes within MIST_RING + 260 tiles of its shape bound; continent() also caps every
//     land inside that guard to the sea, so open water sets it apart whatever the noise does;
//   - every land's tiles lie within its shape bound (1.18 r in the warped space continent() draws in; hidden 1.12 r):
//     beyond it the open ocean has no stray specks of land.
// The Mist (mistAt) is a ring MIST_RING wide just outside a hidden land's shape bound, thickening over its outer 60
// tiles and thinning over its inner 60. seaAt reads the chunks' own sea (the block elevation): shallows above the
// deep-water line, deep water below, and the storm sea far from any land.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include "rpg/world/gen.h"

namespace ew {
using namespace gen;

namespace {
const int kDX[8] = {1, 1, 0, -1, -1, -1, 0, 1}, kDY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
constexpr int32_t ORIGIN_R = 3200;
constexpr int32_t WARP_REACH = 1400;     // how far continent()'s two warps (1000 + 380) can carry a warped shape
constexpr int32_t HID_GUARD = MIST_RING + 260;   // past a hidden land's ring start: no other land (MIST_RING + 200 + slack)
constexpr int32_t STORM_OFF = 1500;      // the storm sea: open water this far past every land's shape bound
inline int32_t mistInOf(int32_t R) { return R + R * 15 / 100; }
// the home ring index of a cell (0..7), or -1
inline int ringIndex(int32_t cx, int32_t cy) {
  for (int k = 0; k < 8; k++) if (kDX[k] == cx && kDY[k] == cy) return k;
  return -1;
}
// squared distance from a point to a segment (tiles)
int64_t segDist2(int32_t px, int32_t py, int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  const int64_t vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
  const int64_t vv = vx * vx + vy * vy;
  const int64_t t = wx * vx + wy * vy;
  if (t <= 0 || !vv) return wx * wx + wy * wy;
  if (t >= vv) return dist2(px, py, bx, by);
  const int64_t cross = wx * vy - wy * vx;   // (|cross| / |v| is the distance to the line)
  return cross / vv * cross + (cross % vv) * cross / vv;
}
// a land's real reach (tiles from its centre that its tiles may occupy)
inline int32_t reachOf(const EndlessSource::Impl::LandDraw& D) {
  return D.hidden ? (int32_t)(((int64_t)D.R * EndlessSource::Impl::HID_HI) >> 16)
                  : (int32_t)(((int64_t)D.R * EndlessSource::Impl::SHAPE_HI) >> 16) + WARP_REACH;
}
}  // namespace

const char* seaKindName(SeaKind k) {
  static const char* n[] = {"LAND", "SHALLOWS", "DEEP SEA", "STORM SEA"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)SeaKind::COUNT, "a name for every sea");
  return (int)k < (int)SeaKind::COUNT ? n[(int)k] : "SEA";
}

// ---- the draws
EndlessSource::Impl::LandDraw EndlessSource::Impl::rawDraw(int32_t cx, int32_t cy) {
  LandDraw D;
  D.h = cellSeed(seed, tag("c.cell"), cx, cy);
  if (cx == 0 && cy == 0) { D.kind = 0; D.px = 0; D.py = 0; D.R = ORIGIN_R; return D; }
  const int32_t q = hq(D.h);
  D.kind = (int8_t)(q < Q(0.55) ? 0 : q < Q(0.75) ? 1 : 2);
  D.px = cx * LCELL + (int32_t)((mix64(D.h ^ 1) >> 33) % 2001) - 1000;
  D.py = cy * LCELL + (int32_t)((mix64(D.h ^ 2) >> 33) % 2001) - 1000;
  D.R = 1800 + (int32_t)((mix64(D.h ^ 3) >> 33) % 1201);
  D.hidden = D.kind != 2 && ((mix64(D.h ^ tag("c.hidden")) >> 40) & 3) == 0;
  return D;
}

// The strait's width as continent() would draw it with only the mainland and a continent 2 at (c2x, c2y) radius R2
// (the cells either side of the strait are left to the sea): the shores found along rays fanning +-45 degrees about
// the line between them, on the continent field (C >= 0.5: within ~150 tiles of the real coast, the hills aside).
int32_t EndlessSource::Impl::homeGap(int32_t c2x, int32_t c2y, int32_t R2) {
  const uint64_t w1 = mix64(seed ^ tag("c.warp")), w2 = mix64(seed ^ tag("c.warp2"));
  const uint64_t d0 = mix64(seed ^ tag("c.d0")), d9 = mix64(seed ^ tag("c.d9")), d1 = mix64(seed ^ tag("c.d1")), d2 = mix64(seed ^ tag("c.d2"));
  auto C = [&](int32_t x, int32_t y) {
    int32_t wx = x, wy = y;
    warpQ(wx, wy, 11, 1000, w1);
    warpQ(wx, wy, 9, 380, w2);
    const int32_t ro = (int32_t)(((int64_t)idist(wx, wy, 0, 0) << 16) / ORIGIN_R);
    const int32_t r2 = (int32_t)(((int64_t)idist(wx, wy, c2x, c2y) << 16) / R2);
    int32_t v = std::max(Q_ONE - sstep(Q(0.75), Q(1.15), ro), Q_ONE - sstep(Q(0.75), Q(1.15), r2));
    const int32_t nearCap = std::max(Q_ONE - sstep(SHAPE_LO, SHAPE_HI, ro), Q_ONE - sstep(SHAPE_LO, SHAPE_HI, r2));
    v += qm(centered(vnoiseQ(x, y, 10, d0), Q(1.6)), Q(0.30)) + qm(centered(vnoiseQ(x, y, 9, d9), Q(1.6)), Q(0.13)) +
         qm(centered(fbmQ(x, y, 8, 3, d1), Q(2.0)), Q(0.08)) + qm(centered(fbmQ(x, y, 6, 2, d2), Q(2.0)), Q(0.04));
    return std::min(v, Q(0.18) + 2 * nearCap);
  };
  // the bearing of continent 2 from the origin (1024 a turn; integer search)
  int32_t a0 = 0;
  {
    int64_t best = INT64_MIN;
    for (int32_t k = 0; k < 1024; k += 2) {
      const int64_t dot = (int64_t)icosR(k, 4096) * c2x + (int64_t)isinR(k, 4096) * c2y;
      if (dot > best) { best = dot; a0 = k; }
    }
  }
  // each shore: the first run of open sea (C < 0.5 for 240 tiles) out along the ray from a land's heart
  auto shores = [&](int32_t ox, int32_t oy, int32_t aMid, int32_t r0, int32_t r1, std::vector<std::pair<int32_t, int32_t>>& out) {
    for (int32_t a = aMid - 128; a <= aMid + 128; a += 12) {
      int32_t seaRun = 0, lastLand = -1;
      for (int32_t r = r0; r <= r1; r += 24) {
        const int32_t x = ox + icosR(a, r), y = oy + isinR(a, r);
        if (C(x, y) >= Q(0.5)) { lastLand = r; seaRun = 0; }
        else if ((seaRun += 24) >= 240) break;
      }
      if (lastLand >= 0) out.push_back({ox + icosR(a, lastLand), oy + isinR(a, lastLand)});
    }
  };
  std::vector<std::pair<int32_t, int32_t>> sO, s2;
  shores(0, 0, a0, ORIGIN_R / 2, ORIGIN_R * 2, sO);
  shores(c2x, c2y, (a0 + 512) & 1023, R2 / 2, R2 * 2, s2);
  int64_t best = INT64_MAX;
  for (const auto& p : sO)
    for (const auto& q : s2) best = std::min(best, dist2(p.first, p.second, q.first, q.second));
  return best == INT64_MAX ? 0 : (int32_t)isqrt((uint64_t)best);
}

const EndlessSource::Impl::HomePlan& EndlessSource::Impl::homePlan() {
  if (home.ready) return home;
  HomePlan& H = home;
  const uint64_t h = mix64(seed ^ tag("c.home"));
  H.k2 = (int)(h % 8);
  H.kH = (H.k2 + 3 + (int)((h >> 8) % 3)) % 8;
  auto unit = [](int k, int32_t& ux, int32_t& uy) {   // Q16 unit vector of a ring direction
    const int32_t d = (kDX[k] && kDY[k]) ? 46341 : 65536;
    ux = kDX[k] * d; uy = kDY[k] * d;
  };
  int32_t ux, uy;
  // continent 2: across a strait of open sea from the mainland's coast. The nominal coasts (0.95 r) are only a first
  // guess: the warps and the coast detail move a real coast by up to ~1500 tiles, so the strait is measured on the
  // continent field itself (homeGap) and continent 2 moved out or in until it is 1900..3400 tiles wide
  unit(H.k2, ux, uy);
  H.R2 = 2200 + (int32_t)((h >> 16) % 700);
  const int32_t gap = 2300 + (int32_t)((h >> 24) % 600);
  const int32_t coastO = ORIGIN_R * 95 / 100, coast2 = H.R2 * 95 / 100;
  int32_t D2 = coastO + gap + coast2;
  const int32_t perp = (int32_t)((h >> 32) % 1001) - 500;
  for (int it = 0; it < 5; it++) {
    H.c2x = qm(ux, D2) - qm(uy, perp);
    H.c2y = qm(uy, D2) + qm(ux, perp);
    const int32_t g = homeGap(H.c2x, H.c2y, H.R2);
    if (g >= 2100 && g <= 3400) break;
    D2 += g < 2100 ? 2600 - g : -(g - 2700);
  }
  H.p0x = qm(ux, coastO); H.p0y = qm(uy, coastO);
  H.p1x = H.c2x - qm(ux, coast2); H.p1y = H.c2y - qm(uy, coast2);
  // the hidden land: its guard (Mist ring + open water) clear of the mainland's warped reach
  unit(H.kH, ux, uy);
  H.RH = 1300 + (int32_t)((h >> 40) % 500);
  const int32_t DH = (int32_t)(((int64_t)ORIGIN_R * SHAPE_HI) >> 16) + WARP_REACH + mistInOf(H.RH) + HID_GUARD + 60 +
                     (int32_t)((h >> 48) % 500);
  const int32_t perpH = (int32_t)((h >> 56) % 401) - 200;
  H.hx = qm(ux, DH) - qm(uy, perpH);
  H.hy = qm(uy, DH) + qm(ux, perpH);
  H.ready = true;
  return H;
}

// The start's harbour (planned once, on first need, after the home plan): the mainland's nearest shore on the real
// elevation (the firth itself left out while it is planned: firthCut returns 0 meanwhile, whoever asks), away from
// the strait and the hidden land's bearing, which keep their open sea; a firth is cut in from that shore to ~2050
// tiles from the origin and the start's port stands on its bank. Deterministic: nothing it reads is cached with the firth.
const EndlessSource::Impl::HomePlan& EndlessSource::Impl::firthPlan() {
  HomePlan& H = home;
  homePlan();
  if (H.firthReady || H.firthBusy) return H;
  H.firthBusy = true;
  const uint64_t h = mix64(seed ^ tag("c.firth"));
  {
    auto ringDist = [](int a, int b) { const int d = std::abs(a - b) & 1023; return std::min(d, 1024 - d); };
    int32_t a2 = 0, aH = 0;
    {
      int64_t b2 = INT64_MIN, bH = INT64_MIN;
      for (int32_t k = 0; k < 1024; k += 4) {
        const int64_t c = icosR(k, 4096), s = isinR(k, 4096);
        if (c * H.c2x + s * H.c2y > b2) { b2 = c * H.c2x + s * H.c2y; a2 = k; }
        if (c * H.hx + s * H.hy > bH) { bH = c * H.hx + s * H.hy; aH = k; }
      }
    }
    // the bearings with open sea beyond them, nearest shore first
    std::vector<std::pair<int32_t, int32_t>> cands;   // (shore distance, bearing)
    for (int32_t a = 0; a < 1024; a += 16) {
      if (ringDist(a, a2) < 128 || ringDist(a, aH) < 128) continue;
      // the open sea's shore on this bearing: in from 5200 tiles (open water there), the first land met (an inland
      // basin or a lagoon nearer the start is no harbour: its water never reaches the other lands)
      if (elevation(icosR(a, 5200), isinR(a, 5200), nullptr, nullptr) >= ELEV_SEA) continue;
      int32_t coast = -1;
      for (int32_t r = 5200; r >= 1200; r -= 24)
        if (elevation(icosR(a, r), isinR(a, r), nullptr, nullptr) >= ELEV_SEA) { coast = r + 24; break; }
      if (coast > 0) cands.push_back({coast, a});
    }
    std::sort(cands.begin(), cands.end());
    if (cands.empty()) cands.push_back({3000, (int32_t)((h >> 4) % 1024)});
    // the firth (always: a sheltered sea loch with deep water down its middle, where a harbour's mole always finds its
    // way out): its tip ~2050 tiles out (no nearer than 1750: the start's own country is never cut), its mouth past the
    // shore; its middle line wanders (lateral offsets up to 160). It keeps 560 tiles from every kingdom's seat (kcell
    // chooses the seats on the land without it too), so no realm moves for it.
    auto lay = [&](int32_t bestD, int32_t bestA) {
      const int32_t r0 = std::max<int32_t>(1750, std::min<int32_t>(2050, bestD - 500)), r1 = bestD + 260;
      const int32_t ca = icosR(bestA, 4096), sa = isinR(bestA, 4096);
      for (int k = 0; k < 5; k++) {
        const int32_t r = r0 + (r1 - r0) * k / 4;
        const int32_t lat = k == 0 || k == 4 ? 0 : (int32_t)((mix64(h ^ (uint64_t)(0xF1A7u + k)) >> 40) % 321) - 160;
        H.fx[k] = (int32_t)(((int64_t)ca * r - (int64_t)sa * lat) / 4096);
        H.fy[k] = (int32_t)(((int64_t)sa * r + (int64_t)ca * lat) / 4096);
      }
      H.fBox[0] = H.fBox[2] = H.fx[0]; H.fBox[1] = H.fBox[3] = H.fy[0];
      for (int k = 1; k < 5; k++) {
        H.fBox[0] = std::min(H.fBox[0], H.fx[k]); H.fBox[2] = std::max(H.fBox[2], H.fx[k]);
        H.fBox[1] = std::min(H.fBox[1], H.fy[k]); H.fBox[3] = std::max(H.fBox[3], H.fy[k]);
      }
      H.fBox[0] -= 420; H.fBox[1] -= 420; H.fBox[2] += 420; H.fBox[3] += 420;
      // the port's bank: a quarter of the way from the tip, beside the water
      const int32_t mx = (H.fx[0] * 3 + H.fx[1]) / 4, my = (H.fy[0] * 3 + H.fy[1]) / 4;
      H.portTx = mx - (int32_t)((int64_t)sa * 160 / 4096); H.portTy = my + (int32_t)((int64_t)ca * 160 / 4096);
    };
    auto clearOfSeats = [&]() {
      for (int32_t ky = EndlessSource::kcellOf(H.fBox[1] - 600); ky <= EndlessSource::kcellOf(H.fBox[3] + 600); ky++)
        for (int32_t kx = EndlessSource::kcellOf(H.fBox[0] - 600); kx <= EndlessSource::kcellOf(H.fBox[2] + 600); kx++) {
          const KCell kc = kcell(kx, ky);
          if (!kc.capital) continue;
          for (int k = 0; k < 4; k++)
            if (segDist2(kc.x, kc.y, H.fx[k], H.fy[k], H.fx[k + 1], H.fy[k + 1]) < 560ll * 560ll) return false;
        }
      return true;
    };
    H.firth = true;
    bool laid = false;
    for (const auto& c : cands) {
      lay(c.first, c.second);
      if (clearOfSeats()) { laid = true; break; }
    }
    if (!laid) lay(cands[0].first, cands[0].second);
  }
  H.firthBusy = false;
  H.firthReady = true;
  return H;
}

// the firth: a sea loch cut into the mainland along its middle line, narrow at its tip, widening to the mouth, its
// banks wandering (value noise on the width) so it reads as a drowned glen, never a canal
int32_t EndlessSource::Impl::firthCut(int32_t x, int32_t y) {
  if (noFirth) return 0;
  const HomePlan& H = firthPlan();
  if (!H.firthReady) return 0;   // (being planned: the land without it)
  if (!H.firth || x < H.fBox[0] || y < H.fBox[1] || x > H.fBox[2] || y > H.fBox[3]) return 0;
  int64_t best = INT64_MAX;
  int seg = 0;
  for (int k = 0; k < 4; k++) {
    const int64_t d = segDist2(x, y, H.fx[k], H.fy[k], H.fx[k + 1], H.fy[k + 1]);
    if (d < best) { best = d; seg = k; }
  }
  const int32_t d = (int32_t)isqrt((uint64_t)best);
  const int32_t w0 = 120 + seg * 55;   // the half-width where the cut fades out (tip 120 .. mouth 285)
  const int32_t w = w0 + (int32_t)((int64_t)(vnoiseQ(x, y, 6, mix64(seed ^ tag("firth.w"))) - 32768) * w0 / 2 / 32768);
  if (d >= w) return 0;
  // (deepest along the middle line, fading over the outer 55 % of the half-width; the tip is rounded, segDist2 measures
  // to the tip point past its end)
  return (int32_t)((int64_t)(Q_ONE - sstep(Q(0.45), Q_ONE, (int32_t)((int64_t)d * 65536 / std::max(1, w)))) * 17 / 10);   // (up to 1.7: the
                                                                                                                  //  land's field tops ~1.55)
}

EndlessSource::Impl::LandDraw EndlessSource::Impl::preDraw(int32_t cx, int32_t cy) {
  LandDraw D = rawDraw(cx, cy);
  if ((cx == 0 && cy == 0) || std::abs(cx) > 2 || std::abs(cy) > 2) return D;
  const HomePlan& H = homePlan();
  const int k = ringIndex(cx, cy);
  if (k >= 0 && k == H.k2) { D.kind = 0; D.px = H.c2x; D.py = H.c2y; D.R = H.R2; D.hidden = false; return D; }
  if (k >= 0 && k == H.kH) { D.kind = 0; D.px = H.hx; D.py = H.hy; D.R = H.RH; D.hidden = true; return D; }
  if (k >= 0 && (k == (H.k2 + 1) % 8 || k == (H.k2 + 7) % 8)) { D.kind = 2; D.hidden = false; return D; }   // the strait's flanks
  if (k >= 0) D.hidden = false;   // (the mainland's neighbours: a hidden land there could not keep its Mist clear of it)
  if (D.kind == 2) return D;
  const int32_t reach = (int32_t)(((int64_t)D.R * SHAPE_HI) >> 16);
  // the home hidden land's guard stays open sea
  const int32_t guardH = mistInOf(H.RH) + HID_GUARD;
  if (idist(D.px, D.py, H.hx, H.hy) < reach + WARP_REACH / 2 + guardH) { D.kind = 2; D.hidden = false; return D; }
  // the strait stays open sea, and no land bridges the mainland and continent 2
  const int64_t sd = segDist2(D.px, D.py, H.p0x, H.p0y, H.p1x, H.p1y);
  const int64_t clear = (int64_t)(reach + 500) * (reach + 500);
  const int32_t reach2 = (int32_t)(((int64_t)H.R2 * SHAPE_HI) >> 16), reachO = (int32_t)(((int64_t)ORIGIN_R * SHAPE_HI) >> 16);
  if (sd < clear || (idist(D.px, D.py, H.c2x, H.c2y) < reach + reach2 + 200 && idist(D.px, D.py, 0, 0) < reach + reachO + 200)) {
    D.kind = 2; D.hidden = false;
  }
  return D;
}

const EndlessSource::Impl::LandDraw& EndlessSource::Impl::landDraw(int32_t cx, int32_t cy) {
  DrawSlot& S = drawSlots[(uint32_t)(cx * 31 + cy * 17 + 4096 * 33) & 1023u];
  if (S.used && S.cx == cx && S.cy == cy) return S.d;
  LandDraw D = preDraw(cx, cy);
  const bool homeHidden = std::abs(cx) <= 1 && std::abs(cy) <= 1 && ringIndex(cx, cy) == homePlan().kH;
  if (D.kind != 2 && D.hidden && !homeHidden) {
    // set apart: shrink until every neighbour's reach (taken as a warped land's, the larger) stays out of the guard
    int32_t room = INT32_MAX;
    for (int dj = -1; dj <= 1; dj++)
      for (int di = -1; di <= 1; di++) {
        if (!di && !dj) continue;
        LandDraw N = preDraw(cx + di, cy + dj);
        if (N.kind == 2) continue;
        N.hidden = false;
        room = std::min(room, idist(D.px, D.py, N.px, N.py) - reachOf(N) - HID_GUARD);
      }
    const int32_t R = room == INT32_MAX ? D.R : std::min(D.R, room * 100 / 115);
    if (R < 900) D.hidden = false;
    else D.R = R;
  }
  S.used = true; S.cx = cx; S.cy = cy; S.d = D;
  return S.d;
}

LandPlan EndlessSource::Impl::landCell(int32_t cx, int32_t cy, bool* ok) {
  const uint64_t key = key2(cx, cy);
  auto it = landCellMemo.find(key);
  if (it != landCellMemo.end()) { if (ok) *ok = it->second.first; return it->second.second; }
  const LandDraw D = landDraw(cx, cy);
  LandPlan L;
  const bool live = D.kind != 2;
  L.kind = D.kind == 1 ? LandKind::Archipelago : LandKind::Continent;
  L.cellX = cx; L.cellY = cy;
  L.cx = D.px; L.cy = D.py; L.r = D.R;
  L.origin = cx == 0 && cy == 0;
  L.id = live ? landIdOf(cx, cy) : 0;
  L.hidden = live && D.hidden;
  if (L.hidden) {
    L.mistIn = mistInOf(L.r);
    L.mistOut = L.mistIn + MIST_RING;
  }
  if (live) {
    // names: a hidden land in its own people's tongue ("VARRAKH"); the rest as the sailors of the nearest shore call
    // them ("THE SUNDERED ISLES", "THE ASKVAR REACH")
    const uint64_t n = mix64(D.h ^ tag("c.name"));
    if (L.origin) L.name = "THE MAINLAND";
    else if (L.hidden) {
      const cult::Culture& C = atlas().get(hiddenFamily(L.id));
      L.name = cult::placeName(C, (uint32_t)(n >> 7));
      if (L.kind == LandKind::Archipelago) L.name += " ISLES";
    } else {
      static const char* kB[] = {"SUNDERED", "GREY", "FARTHEST", "SILENT", "DROWNED", "OLD", "WESTERN", "PALE", "AMBER", "STORMWRACK", "GULL", "SALT"};
      static const char* kC[] = {"REACH", "SHORES", "LANDS", "MARCHES", "COAST", "STRAND"};
      if (L.kind == LandKind::Archipelago) L.name = std::string("THE ") + kB[(n >> 8) % 12] + " ISLES";
      else if ((n >> 30) & 1) {
        const cult::Culture& C = atlas().get(cultureAt(L.cx, L.cy));
        std::string w = cult::landmarkName(C, (uint32_t)(n >> 11), (int)LandmarkKind::Sea);
        if (w.size() > 9) w.resize(9);
        L.name = "THE " + w + " " + kC[(n >> 16) % 6];
      } else L.name = std::string("THE ") + kB[(n >> 8) % 12] + " " + kC[(n >> 16) % 6];
    }
  }
  if (landCellMemo.size() > 4096) landCellMemo.clear();
  landCellMemo[key] = {live, L};
  if (ok) *ok = live;
  return L;
}

// the virtual culture cell of a hidden land's people (cult::familyId of the cell holding its centre, shifted far off
// the map: culture_map.cpp's climate callback maps it back and marks it hidden). Mainland families never see it.
uint64_t EndlessSource::Impl::hiddenFamily(uint32_t landId) {
  const LandDraw& D = landDraw(landCellX(landId), landCellY(landId));
  return cult::familyId(floorDiv(D.px, CCELL) + HIDDEN_CCELL_OFF, floorDiv(D.py, CCELL));
}

uint32_t EndlessSource::Impl::hiddenShapeAt(int32_t x, int32_t y) {
  const int32_t cx0 = floorDiv(x + LCELL / 2, LCELL), cy0 = floorDiv(y + LCELL / 2, LCELL);
  for (int32_t cy = cy0 - 1; cy <= cy0 + 1; cy++)
    for (int32_t cx = cx0 - 1; cx <= cx0 + 1; cx++) {
      const LandDraw& D = landDraw(cx, cy);
      if (D.kind == 2 || !D.hidden) continue;
      const int64_t lim = ((int64_t)D.R * HID_HI) >> 16;
      if (dist2(x, y, D.px, D.py) <= lim * lim) return landIdOf(cx, cy);
    }
  return 0;
}

bool EndlessSource::Impl::landTile(int32_t x, int32_t y) {
  const int32_t e = elevation(x, y, nullptr, nullptr) + qm(centered(vnoiseQ(x, y, 3, mix64(seed ^ tag("t.d1"))), Q(1.0)), Q(0.010)) +
                    qm(centered(vnoiseQ(x, y, 1, mix64(seed ^ tag("t.d2"))), Q(1.0)), Q(0.004));
  return e >= ELEV_SEA;
}

uint32_t EndlessSource::Impl::landAt(int32_t x, int32_t y) {
  if (const uint32_t h = hiddenShapeAt(x, y)) return landTile(x, y) ? h : 0;
  // the warped space continent() draws the other lands in
  int32_t wx = x, wy = y;
  warpQ(wx, wy, 11, 1000, mix64(seed ^ tag("c.warp")));
  warpQ(wx, wy, 9, 380, mix64(seed ^ tag("c.warp2")));
  const int32_t cx0 = floorDiv(wx + LCELL / 2, LCELL), cy0 = floorDiv(wy + LCELL / 2, LCELL);
  uint32_t best = 0;
  int64_t bestR = 0;
  for (int32_t cy = cy0 - 1; cy <= cy0 + 1; cy++)
    for (int32_t cx = cx0 - 1; cx <= cx0 + 1; cx++) {
      const LandDraw& D = landDraw(cx, cy);
      if (D.kind == 2 || D.hidden) continue;
      const int64_t r = ((int64_t)idist(wx, wy, D.px, D.py) << 16) / std::max<int32_t>(1, D.R);
      if (r > SHAPE_HI) continue;
      if (!best || r < bestR) { best = landIdOf(cx, cy); bestR = r; }
    }
  if (!best) return 0;
  return landTile(x, y) ? best : 0;
}

uint32_t EndlessSource::Impl::hiddenNear(int32_t x, int32_t y, int32_t pad) {
  const int32_t cx0 = floorDiv(x + LCELL / 2, LCELL), cy0 = floorDiv(y + LCELL / 2, LCELL);
  for (int32_t cy = cy0 - 1; cy <= cy0 + 1; cy++)
    for (int32_t cx = cx0 - 1; cx <= cx0 + 1; cx++) {
      const LandDraw& D = landDraw(cx, cy);
      if (D.kind == 2 || !D.hidden) continue;
      const int64_t out = mistInOf(D.R) + MIST_RING + pad;
      if (dist2(x, y, D.px, D.py) <= out * out) return landIdOf(cx, cy);
    }
  return 0;
}

uint32_t EndlessSource::Impl::hiddenLandAt(int32_t x, int32_t y) {
  const int32_t cx0 = floorDiv(x + LCELL / 2, LCELL), cy0 = floorDiv(y + LCELL / 2, LCELL);
  for (int32_t cy = cy0 - 1; cy <= cy0 + 1; cy++)
    for (int32_t cx = cx0 - 1; cx <= cx0 + 1; cx++) {
      const LandDraw& D = landDraw(cx, cy);
      if (D.kind == 2 || !D.hidden) continue;
      const int64_t out = mistInOf(D.R) + MIST_RING;
      if (dist2(x, y, D.px, D.py) <= out * out) return landIdOf(cx, cy);
    }
  return 0;
}

// the Mist: thickening over the outer 60 tiles of the ring (a boat sails into it from the open sea), thick through
// the ring, thinning over its inner 60 tiles to the clear waters round the hidden land
int32_t EndlessSource::Impl::mistAt(int32_t x, int32_t y) {
  const int32_t cx0 = floorDiv(x + LCELL / 2, LCELL), cy0 = floorDiv(y + LCELL / 2, LCELL);
  int32_t best = 0;
  for (int32_t cy = cy0 - 1; cy <= cy0 + 1; cy++)
    for (int32_t cx = cx0 - 1; cx <= cx0 + 1; cx++) {
      const LandDraw& D = landDraw(cx, cy);
      if (D.kind == 2 || !D.hidden) continue;
      const int32_t mIn = mistInOf(D.R), mOut = mIn + MIST_RING;
      if (std::abs(x - D.px) >= mOut || std::abs(y - D.py) >= mOut) continue;
      const int32_t d = idist(x, y, D.px, D.py);
      if (d <= mIn || d >= mOut) continue;
      const int32_t in = d - mIn, out = mOut - d;
      const int32_t v = std::min({Q_ONE, (int32_t)((int64_t)in * Q_ONE / 60), (int32_t)((int64_t)out * Q_ONE / 60)});
      best = std::max(best, v);
    }
  return best;
}

SeaKind EndlessSource::Impl::seaAt(int32_t x, int32_t y) {
  const int32_t e = waterE(x, y);   // (the chunks' own sea: tile().sea is e < ELEV_SEA)
  if (e >= ELEV_SEA) return waterAt(x, y) ? SeaKind::Shallow : SeaKind::Land;   // lakes and rivers
  if (e >= ELEV_SEA - Q(0.035)) return SeaKind::Shallow;                        // (macro.cpp groundFor: Ground::Water)
  // the storm sea: open ocean far past every land's shape bound (hidden lands in real space, the rest warped)
  int32_t wx = x, wy = y;
  warpQ(wx, wy, 11, 1000, mix64(seed ^ tag("c.warp")));
  warpQ(wx, wy, 9, 380, mix64(seed ^ tag("c.warp2")));
  for (int pass = 0; pass < 2; pass++) {
    const int32_t qx = pass ? x : wx, qy = pass ? y : wy;
    const int32_t cx0 = floorDiv(qx + LCELL / 2, LCELL), cy0 = floorDiv(qy + LCELL / 2, LCELL);
    for (int32_t cy = cy0 - 1; cy <= cy0 + 1; cy++)
      for (int32_t cx = cx0 - 1; cx <= cx0 + 1; cx++) {
        const LandDraw& D = landDraw(cx, cy);
        if (D.kind == 2 || D.hidden != (pass == 1)) continue;
        const int64_t lim = (D.hidden ? (((int64_t)D.R * HID_HI) >> 16) + MIST_RING : (((int64_t)D.R * SHAPE_HI) >> 16)) + STORM_OFF;
        if (dist2(qx, qy, D.px, D.py) <= lim * lim) return SeaKind::Deep;
      }
  }
  return SeaKind::Storm;
}

// ---- streaming at sea: a chunk of the open sea, deep water through and through. Every coarse sample its tiles (and
//      baseRect's 2-tile border, through tile()'s 5-tile warp) read lies below the deep-water line by more than the
//      coast detail can lift it, and nothing of the 3 x 3 regions round it reaches it (sites, dens, roads, harbours
//      and their channels). Pure: it reads the blocks and the region plans the full pipeline reads.
bool EndlessSource::Impl::oceanChunk(int32_t cx, int32_t cy) {
  const int32_t x0 = cx * CHUNK, y0 = cy * CHUNK;
  const int32_t lim = ELEV_SEA - Q(0.035) - Q(0.016);
  const int32_t gx0 = floorDiv(x0 - 8, CG), gx1 = floorDiv(x0 + CHUNK + 8, CG) + 1;
  const int32_t gy0 = floorDiv(y0 - 8, CG), gy1 = floorDiv(y0 + CHUNK + 8, CG) + 1;
  for (int32_t gy = gy0; gy <= gy1; gy++)
    for (int32_t gx = gx0; gx <= gx1; gx++) {
      const int32_t bx = floorDiv(gx, BLK), by = floorDiv(gy, BLK);
      const Block& B = *block(bx, by);
      if (B.at(gx - bx * BLK, gy - by * BLK).e >= lim) return false;
    }
  const int32_t rx0 = regionOf(x0), ry0 = regionOf(y0);
  for (int k = 0; k < 9; k++) {
    const std::shared_ptr<const RegionData> RD = regionData(rx0 - 1 + k % 3, ry0 - 1 + k / 3);
    for (const SitePlan& p : RD->plan.sites) {
      const int m = isSettlement(p.type) ? 40 : 8;
      if (!(p.gx - m >= x0 + CHUNK || p.gy - m >= y0 + CHUNK || p.gx + p.w + m <= x0 || p.gy + p.h + m <= y0)) return false;
      if (p.flags & SPF_PORT) {
        PortPlan PP;
        if (harbourOf(Node{p.id, p.type, p.ex, p.ey, p.seed, p.flags}, PP) && PP.harbour) {
          const int32_t reach = PP.pierLen + 14 + harbourDredgeLen(PP);
          if (!(PP.rootX + reach < x0 - 4 || PP.rootX - reach >= x0 + CHUNK + 4 || PP.rootY + reach < y0 - 4 || PP.rootY - reach >= y0 + CHUNK + 4))
            return false;
        }
      }
    }
    for (const DenPlan& d : RD->plan.dens)
      if (!(d.x + 6 < x0 || d.y + 6 < y0 || d.x - 6 >= x0 + CHUNK || d.y - 6 >= y0 + CHUNK)) return false;
    for (const RoadPlan& rp : RD->plan.roads)
      for (const GTile& g : rp.pts)
        if (g.x >= x0 - 4 && g.y >= y0 - 4 && g.x < x0 + CHUNK + 4 && g.y < y0 + CHUNK + 4) return false;
  }
  return true;
}

// ---- the public doors
uint32_t EndlessSource::landAt(int32_t gx, int32_t gy) { d_->makeStart(); return d_->landAt(gx, gy); }
LandPlan EndlessSource::land(uint32_t id) {
  d_->makeStart();
  if (!id) return LandPlan();
  bool ok = false;
  const LandPlan L = d_->landCell(landCellX(id), landCellY(id), &ok);
  return ok && L.id == id ? L : LandPlan();
}
std::vector<LandPlan> EndlessSource::landsIn(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
  d_->makeStart();
  std::vector<LandPlan> out;
  const int32_t pad = 3200 * 118 / 100 + WARP_REACH + MIST_RING + LCELL / 2;
  for (int32_t cy = floorDiv(y0 - pad, LCELL); cy <= floorDiv(y1 + pad, LCELL); cy++)
    for (int32_t cx = floorDiv(x0 - pad, LCELL); cx <= floorDiv(x1 + pad, LCELL); cx++) {
      bool ok = false;
      const LandPlan L = d_->landCell(cx, cy, &ok);
      if (!ok) continue;
      const int32_t reach = L.hidden ? L.mistOut : L.r * 118 / 100 + WARP_REACH;
      if (L.cx + reach < x0 || L.cx - reach >= x1 || L.cy + reach < y0 || L.cy - reach >= y1) continue;
      out.push_back(L);
    }
  std::sort(out.begin(), out.end(), [](const LandPlan& a, const LandPlan& b) { return a.id < b.id; });
  return out;
}
uint32_t EndlessSource::hiddenLandAt(int32_t gx, int32_t gy) { d_->makeStart(); return d_->hiddenLandAt(gx, gy); }
int32_t EndlessSource::mistAt(int32_t gx, int32_t gy) { d_->makeStart(); return d_->mistAt(gx, gy); }
SeaKind EndlessSource::seaAt(int32_t gx, int32_t gy) { d_->makeStart(); return d_->seaAt(gx, gy); }

void EndlessSource::oceanFastPath(bool on) { d_->oceanFast = on; }
bool EndlessSource::chartPlaces(uint32_t landId, SitePlan& wreck, SitePlan& cave) { d_->makeStart(); return d_->chartPlaces(landId, wreck, cave); }
bool EndlessSource::trialOf(Gid kingdom, int which, SitePlan& out) { d_->makeStart(); return d_->trialOf(kingdom, which, out); }

std::vector<PortPlan> EndlessSource::portsIn(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
  d_->makeStart();
  std::vector<Node> nodes;
  d_->nodesIn(x0, y0, x1, y1, nodes);
  std::vector<PortPlan> out;
  for (const Node& n : nodes) {
    if (n.x < x0 || n.y < y0 || n.x >= x1 || n.y >= y1) continue;
    PortPlan P;
    if (!d_->harbourOf(n, P)) continue;
    out.push_back(std::move(P));
  }
  std::sort(out.begin(), out.end(), [](const PortPlan& a, const PortPlan& b) { return a.site < b.site; });
  out.erase(std::unique(out.begin(), out.end(), [](const PortPlan& a, const PortPlan& b) { return a.site == b.site; }), out.end());
  return out;
}

}  // namespace ew
