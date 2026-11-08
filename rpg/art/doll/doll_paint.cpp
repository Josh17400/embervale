// EMBERVALE art: the doll's frame paint (SOLID 3.6, moved from doll_compose.cpp's composeFrame). FramePaint gathers a
// frame's items (the recipe's parts at their views: `replace` and `when` views, the motif and key fallbacks, the tiles),
// sorts them by layer, places their anchors (the crown, then the grip-anchored items, the T6 tip), and paints them back to
// front: the forge cut tiles and edge stamps (stampEdges), the trim band, LADDER-FINISH's fit and stamp placements, the
// iridescent bands, then every pixel recoloured by the recipe's regions, finishes and tiles and written with its erase,
// shield and probe bookkeeping. The order of every step, scan and push is composeFrame's own.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <unordered_map>
#include "rpg/art/art_holders2.h"
#include "rpg/art/doll/doll_compose_internal.h"
#include "rpg/art/doll/doll_envelope.h"
#include "rpg/art/doll/doll_forge.h"

#if defined(_MSC_VER)
#define DOLL_PIXEL_INLINE __forceinline
#else
#define DOLL_PIXEL_INLINE inline __attribute__((always_inline))
#endif

namespace art {
namespace doll {

namespace {

using namespace pool;
using namespace comp;

// (forge round 2) a part serving `role overlay <prefix>...` (skirt., motif., sleeve.)
bool overlayPrefix(const Pool& P, const PartRec& p, const char* prefix) {
  for (const RoleRec& rr : p.roles)
    if (rr.cat == (uint8_t)Role::Overlay && P.s(rr.value).rfind(prefix, 0) == 0) return true;
  return false;
}
// (wave-2 fix round, D78) the trim band of a view: bandReg = its regions with the widened pixels set to R; bandRow = each R pixel's
// row in the band counted from the outer edge (-1: not trim). Depth = the 4-connected distance from the part's outline (a
// pixel beside a clear cell or the view's edge is 0). A run is widened from its innermost R pixels toward deeper pixels until
// it is W thick; never into G or N, an outline pixel (step 0) or a clear cell
// (wave-2 fix round 4, D93) level: the slot's tuned level (DollRecipe::trimLevel); edges: the part's edge kinds (TE_*) the floor
// mode may continue along; t3: a T3 piece (the ceiling mode paints its shortest drawn R runs in its own metal)
// (D93) floorLv: the floor mode's edges laid (1 hem .. 4 the pauldron rim) at edgeW; cutLv: the T3 ceiling mode's run limit
// (1..16); the T4-T6 ceiling narrows W itself (the caller)
}  // namespace

namespace comp {

void trimBand(const ViewRec& v, int W, std::vector<uint8_t>& bandReg, std::vector<int8_t>& bandRow, int floorLv, int edges, int edgeW,
              int cutLv) {
  const int n = v.w * v.h;
  // (D93) a view with no region layer (a pauldron, a cloak's back) reads as all "none": the floor mode may still edge it
  if (v.regions.size() == (size_t)n) bandReg.assign(v.regions.begin(), v.regions.end());
  else bandReg.assign((size_t)n, 0);
  const std::vector<uint8_t>& vreg = bandReg;   // (the drawn regions: bandReg is only widened below, never narrowed before isR reads it)
  bandRow.assign((size_t)n, -1);
  auto solid = [&](int k) { const uint8_t c = v.cells[(size_t)k]; return c >= 2 && c != kErase; };
  static thread_local std::vector<int> dist, q, rOut, rIn;
  dist.assign((size_t)n, -1);
  q.clear();
  for (int y = 0; y < v.h; y++)
    for (int x = 0; x < v.w; x++) {
      const int k = y * v.w + x;
      if (!solid(k)) continue;
      bool edge = x == 0 || y == 0 || x == v.w - 1 || y == v.h - 1;
      if (!edge) edge = !solid(k - 1) || !solid(k + 1) || !solid(k - v.w) || !solid(k + v.w);
      if (edge) { dist[(size_t)k] = 0; q.push_back(k); }
    }
  for (size_t h = 0; h < q.size(); h++) {
    const int k = q[h], x = k % v.w, y = k / v.w;
    const int nb[4] = {x > 0 ? k - 1 : -1, x < v.w - 1 ? k + 1 : -1, y > 0 ? k - v.w : -1, y < v.h - 1 ? k + v.w : -1};
    for (int m : nb) if (m >= 0 && solid(m) && dist[(size_t)m] < 0) { dist[(size_t)m] = dist[(size_t)k] + 1; q.push_back(m); }
  }
  static thread_local std::vector<uint8_t> drawnReg;
  drawnReg = vreg;
  auto isR = [&](int k) { return solid(k) && drawnReg[(size_t)k] == (uint8_t)Region::Trim && codeStep(v.cells[(size_t)k]) >= 1; };
  // rOut: R pixels from the outer edge of the run to here; rIn: from here to the inner edge (q is in BFS order: dist ascending)
  rOut.assign((size_t)n, 0);
  rIn.assign((size_t)n, 0);
  auto nbrs = [&](int k, int out[4]) {
    const int x = k % v.w, y = k / v.w;
    out[0] = x > 0 ? k - 1 : -1; out[1] = x < v.w - 1 ? k + 1 : -1; out[2] = y > 0 ? k - v.w : -1; out[3] = y < v.h - 1 ? k + v.w : -1;
  };
  for (int k : q) {
    if (!isR(k)) continue;
    int o = 1, nb[4];
    nbrs(k, nb);
    for (int m : nb) if (m >= 0 && isR(m) && dist[(size_t)m] == dist[(size_t)k] - 1) o = std::max(o, rOut[(size_t)m] + 1);
    rOut[(size_t)k] = o;
  }
  for (size_t h = q.size(); h-- > 0;) {
    const int k = q[h];
    if (!isR(k)) continue;
    int i = 1, nb[4];
    nbrs(k, nb);
    for (int m : nb) if (m >= 0 && isR(m) && dist[(size_t)m] == dist[(size_t)k] + 1) i = std::max(i, rIn[(size_t)m] + 1);
    rIn[(size_t)k] = i;
  }
  auto canTake = [&](int k) {
    if (!solid(k) || codeStep(v.cells[(size_t)k]) < 1) return false;
    const uint8_t g = drawnReg[(size_t)k];
    return g != (uint8_t)Region::Trim && g != (uint8_t)Region::Gem && g != (uint8_t)Region::Seam;
  };
  for (int k : q) {
    if (!isR(k)) continue;
    bandRow[(size_t)k] = (int8_t)std::min(rOut[(size_t)k] - 1, 7);
  }
  for (int k : q) {
    if (!isR(k) || rIn[(size_t)k] != 1) continue;   // the run's innermost pixels
    const int thick = rOut[(size_t)k];
    if (thick >= W) continue;
    std::vector<int> front{k};
    for (int row = thick; row < W && !front.empty(); row++) {
      std::vector<int> next;
      for (int f : front) {
        int nb[4];
        nbrs(f, nb);
        for (int m : nb) {
          if (m < 0 || dist[(size_t)m] != dist[(size_t)f] + 1 || !canTake(m) || bandReg[(size_t)m] == (uint8_t)Region::Trim) continue;
          bandReg[(size_t)m] = (uint8_t)Region::Trim;
          bandRow[(size_t)m] = (int8_t)row;
          next.push_back(m);
        }
      }
      front.swap(next);
    }
  }
  // (wave-2 fix round 4, D93) the floor mode: the band continues along the piece's own edges in order (floorLv 1 the hem, 2
  // the neck opening, 3 the arm openings, 4 a pauldron's lower rim) at edgeW: from the edge inward past the outline ring
  // (step 0), edgeW cells that can take trim (never a G, N or drawn R cell, a step-0 or clear cell); runs of 2 px or more
  if (floorLv >= 1 && edgeW >= 1 && edges) {
    static thread_local std::vector<uint8_t> add;
    add.assign((size_t)n, 0);
    auto take = [&](int k) { return canTake(k) && bandReg[(size_t)k] != (uint8_t)Region::Trim; };
    auto lay = [&](int x, int y, int dx, int dy) {
      while (x >= 0 && y >= 0 && x < v.w && y < v.h && !solid(y * v.w + x)) { x += dx; y += dy; }
      while (x >= 0 && y >= 0 && x < v.w && y < v.h && solid(y * v.w + x) && codeStep(v.cells[(size_t)y * v.w + x]) == 0) { x += dx; y += dy; }      for (int row = 0; row < edgeW; row++, x += dx, y += dy) {
        if (x < 0 || y < 0 || x >= v.w || y >= v.h) break;
        const int k = y * v.w + x;
        if (!take(k)) break;
        if (!add[(size_t)k] || row < add[(size_t)k] - 1) add[(size_t)k] = (uint8_t)(row + 1);
      }
    };
    int top = v.h, bot = -1;
    for (int k = 0; k < n; k++) if (solid(k)) { top = std::min(top, k / v.w); bot = std::max(bot, k / v.w); }
    const int mid = (top + bot) / 2;
    if ((edges & TE_HEM) && floorLv >= 1)
      for (int x = 0; x < v.w; x++) {
        int yb = -1;
        for (int y = v.h - 1; y >= 0 && yb < 0; y--) if (solid(y * v.w + x)) yb = y;
        if (yb >= mid) lay(x, v.h - 1, 0, -1);
      }
    if ((edges & TE_NECK) && floorLv >= 2) {
      const int cx = v.w / 2, hw = std::max(2, v.w / 6);
      for (int x = std::max(0, cx - hw); x <= std::min(v.w - 1, cx + hw); x++) {
        int yt = -1;
        for (int y = 0; y < v.h && yt < 0; y++) if (solid(y * v.w + x)) yt = y;
        if (yt >= 0 && yt <= mid) lay(x, 0, 0, 1);
      }
    }
    if ((edges & TE_ARM) && floorLv >= 3)
      for (int y = top; y <= mid && y < v.h; y++) { lay(0, y, 1, 0); lay(v.w - 1, y, -1, 0); }
    if ((edges & TE_RIM) && floorLv >= 4)
      for (int x = 0; x < v.w; x++) lay(x, v.h - 1, 0, -1);
    if ((edges & TE_PLATE) && floorLv >= 5) {
      // an internal line cell: solid, a seam (N) or a step-0 cell with solid cells above and below (never the outer ring)
      auto line = [&](int x, int y) {
        if (x < 0 || y < 1 || x >= v.w || y >= v.h - 1) return false;
        const int k = y * v.w + x;
        if (!solid(k)) return false;
        if (drawnReg[(size_t)k] == (uint8_t)Region::Seam) return true;
        return codeStep(v.cells[(size_t)k]) == 0 && solid(k - v.w) && solid(k + v.w);
      };
      static thread_local std::vector<int> ys;
      ys.clear();
      for (int y = 1; y < v.h - 1; y++) {
        int run = 0, best = 0;
        for (int x = 0; x < v.w; x++) { run = line(x, y) && take((y - 1) * v.w + x) ? run + 1 : 0; best = std::max(best, run); }
        if (best >= 2) ys.push_back(y);
      }
      // the lowest (floorLv - 4) lines; a line directly under another counts once (a 2 px seam is one line)
      std::sort(ys.begin(), ys.end(), std::greater<int>());
      int nl = 0, last = -9;
      for (int y : ys) {
        if (y == last - 1) { last = y; continue; }
        if (nl >= floorLv - 4) break;
        nl++;
        last = y;
        for (int x = 0; x < v.w; x++) {
          const int k = (y - 1) * v.w + x;
          if (line(x, y) && take(k) && !add[(size_t)k] && !line(x, y - 1)) add[(size_t)k] = (uint8_t)(kBandRim | 1);
        }
      }
    }
    static thread_local std::vector<int> st;
    static thread_local std::vector<uint8_t> seen;
    seen.assign((size_t)n, 0);
    for (int k0 = 0; k0 < n; k0++) {
      if (!add[(size_t)k0] || seen[(size_t)k0]) continue;
      std::vector<int> comp;
      st.assign(1, k0);
      seen[(size_t)k0] = 1;
      while (!st.empty()) {
        const int k = st.back();
        st.pop_back();
        comp.push_back(k);
        int nb[4];
        nbrs(k, nb);
        for (int m : nb) if (m >= 0 && add[(size_t)m] && !seen[(size_t)m]) { seen[(size_t)m] = 1; st.push_back(m); }
      }
      if (comp.size() < 2) continue;
      for (int k : comp) {
        bandReg[(size_t)k] = (uint8_t)Region::Trim;
        bandRow[(size_t)k] = (int8_t)((add[(size_t)k] & kBandRim) ? kBandRim : (add[(size_t)k] - 1));
      }
    }
  }
  // (D93) the ceiling mode at T3: the shortest drawn R runs (4-connected) paint in the piece's own metal (as at T1-T2): cutLv 1
  // runs of 2 px or less, 2 3, 3 4 ... 9 10, 10 12, 11 14, 12 18, 13 24, 14 32, 15 48, 16 every run
  if (cutLv >= 1) {
    static const int kRun[16] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 18, 24, 32, 48, 1 << 20};
    const int lim = kRun[std::min(15, cutLv - 1)];
    static thread_local std::vector<int> st2;
    static thread_local std::vector<uint8_t> seen2;
    seen2.assign((size_t)n, 0);
    for (int k0 = 0; k0 < n; k0++) {
      if (!isR(k0) || seen2[(size_t)k0]) continue;
      std::vector<int> comp;
      st2.assign(1, k0);
      seen2[(size_t)k0] = 1;
      while (!st2.empty()) {
        const int k = st2.back();
        st2.pop_back();
        comp.push_back(k);
        int nb[4];
        nbrs(k, nb);
        for (int m : nb) if (m >= 0 && isR(m) && !seen2[(size_t)m]) { seen2[(size_t)m] = 1; st2.push_back(m); }
      }
      if ((int)comp.size() > lim) continue;
      for (int k : comp) bandRow[(size_t)k] = -2;   // (-2: R kept as drawn material, its tier's colour not laid)
    }
  }
}

bool partHasRoleCat(const PartRec& p, Role r) {
  for (const RoleRec& rr : p.roles) if (rr.cat == (uint8_t)r) return true;
  return false;
}

}  // namespace comp

namespace {

// the band pixel's outer side faces down or right (its clear neighbour, or the nearest clear cell, is below or to the right)
bool bandFacesDownRight(const ViewRec& v, int x, int y) {
  auto clear = [&](int xx, int yy) { return xx < 0 || yy < 0 || xx >= v.w || yy >= v.h || v.cells[(size_t)yy * v.w + xx] < 2; };
  for (int d = 1; d <= 3; d++) {
    if (clear(x, y - d) || clear(x - d, y)) return false;
    if (clear(x, y + d) || clear(x + d, y)) return true;
  }
  return false;
}
// the trim tile's view for a piece of `tier`: the rest view (facing D) whose condition names that tier (`<slot>.tN` or `tierN`,
// N == tier), else the nearest lower tier's, else the base rest view
const ViewRec* trimBandView(const Pool& P, const PartRec& p, int tier) {
  const uint16_t rest = P.strId("rest");
  const ViewRec* best = nullptr;
  int bestN = -1;
  for (uint32_t vi = p.firstView; vi < p.firstView + p.nViews && vi < P.views.size(); vi++) {
    const ViewRec& w = P.views[vi];
    if (w.facing != 0 || w.key != rest || w.cells.empty()) continue;
    int nT = 0;
    if (w.when) {
      const std::string& c = P.s(w.when);
      for (size_t i = 0; i + 1 < c.size(); i++) {
        if (c[i] == '!' ) { while (i < c.size() && c[i] != ',') i++; continue; }
        int d = -1;
        if (c.compare(i, 2, ".t") == 0 && i + 2 < c.size() && c[i + 2] >= '1' && c[i + 2] <= '6') d = c[i + 2] - '0';
        else if (c.compare(i, 4, "tier") == 0 && i + 4 < c.size() && c[i + 4] >= '1' && c[i + 4] <= '6') d = c[i + 4] - '0';
        if (d > 0) nT = std::max(nT, d);
      }
    }
    if (nT > tier) continue;
    if (nT > bestN) { bestN = nT; best = &w; }
  }
  return best;
}

// the body set's base anchors for a facing (defaults where the clip file names none: the A3 man's)
void baseAnchors(const DollRecipe& r, int facing, Pt out[ANCHOR_COUNT]) {
  const bool woman = r.body == BodySet::Woman;
  const int sR = facing == 0 ? -7 : facing == 1 ? 6 : -1, sL = facing == 0 ? 6 : facing == 1 ? -7 : -1;
  out[(int)Anchor::Pivot] = {0, 0, true};
  out[(int)Anchor::Head] = {-8, woman ? -32 : -33, true};
  out[(int)Anchor::Neck] = {0, -19, true};
  out[(int)Anchor::Hip] = {0, -10, true};
  out[(int)Anchor::ShoulderR] = {sR, -18, true};
  out[(int)Anchor::ShoulderL] = {sL, -18, true};
  out[(int)Anchor::Grip] = {0, 0, false};
  out[(int)Anchor::GripL] = {0, 0, false};
  out[(int)Anchor::Crown] = {0, 0, false};
  if (const BodyRec* b = bodyOf(r.bodyKey))
    for (const AnchorRec& a : b->anchors[facing & 3]) if (a.id < ANCHOR_COUNT) out[a.id] = {a.x, a.y, true};
  // (P2c forge) the crown's default: the head box's top centre (the helm's `mark crown` moves it in composeFrame)
  if (!out[(int)Anchor::Crown].on) out[(int)Anchor::Crown] = {out[(int)Anchor::Head].x + 8, out[(int)Anchor::Head].y, true};
}

void moveAnchor(Anchor a, const FrameRec& f, Pt& p) {
  if (a == Anchor::Pivot) return;
  p.y += f.bob;
  if (a == Anchor::Neck || a == Anchor::Head || a == Anchor::Crown || a == Anchor::ShoulderR || a == Anchor::ShoulderL) p.x += f.lean;
  if (a == Anchor::Hip) { p.x += f.hipX; p.y += f.hipY; }
  if (a == Anchor::Head || a == Anchor::Crown) { p.x += f.headX; p.y += f.headY; }
}

// the destination columns of a view's columns under the recipe's build (broad doubles the marked columns, slim drops
// them; columns move away from / toward the origin's column)
void buildColumns(const DollRecipe& r, const ViewRec& v, std::vector<int>& dst0, std::vector<int>& dst1) {
  dst0.resize(v.w); dst1.assign(v.w, INT32_MIN);
  for (int x = 0; x < v.w; x++) dst0[x] = x;
  if (v.buildRow.empty() || r.build == 0) return;
  const uint8_t mark = r.build == 2 ? 1 : 2;
  for (int x = 0; x < v.w; x++) {
    int shift = 0;
    if (x >= v.ox) { for (int c = v.ox; c < x; c++) if (v.buildRow[c] == mark) shift += mark == 1 ? 1 : -1; }
    else { for (int c = x + 1; c < v.ox; c++) if (v.buildRow[c] == mark) shift += mark == 1 ? -1 : 1; }
    dst0[x] = x + shift;
    if (v.buildRow[x] == mark) {
      if (mark == 2) dst0[x] = INT32_MIN;                               // slim: the column goes
      else dst1[x] = x >= v.ox ? x + shift + 1 : x + shift - 1;          // broad: drawn twice
    }
  }
}

int markSlot(FrameOut& o, uint16_t name) {
  for (int i = 0; i < o.nMarks; i++) if (o.markIds[i] == name) return i;
  if (o.nMarks >= 16) return -1;
  o.markIds[o.nMarks] = name;
  o.marks[o.nMarks] = Pt{};
  return o.nMarks++;
}

bool partIsGarment(const PartRec& p) {
  for (const RoleRec& rr : p.roles) if (rr.cat == (uint8_t)Role::Garment) return true;
  return false;
}
bool partTaggedId(const PartRec& p, uint16_t id) {
  if (id == 0xFFFF) return false;
  for (uint16_t t : p.tags) if (t == id) return true;
  return false;
}
// (P2c) a region tile's regions (the tag on-region:<letters>; 0: a garment tile)
uint32_t regionTileMask(const Pool& P, const PartRec& p) {
  uint32_t m = 0;
  for (uint16_t t : p.tags) {
    const std::string& s = P.s(t);
    if (s.rfind("on-region:", 0) != 0) continue;
    for (size_t i = 10; i < s.size(); i++)
      for (int rg = 1; rg < REGION_COUNT; rg++) if (regionChar((Region)rg) == s[i]) m |= 1u << rg;
  }
  return m;
}
// (P2c ladder) a finish tile (`role overlay finish.<name>`): its DollRecipe::finTiles bit (0: not a finish tile)
uint8_t finishTileBit(const Pool& P, const PartRec& p) {
  for (const RoleRec& rr : p.roles) {
    if (rr.cat != (uint8_t)Role::Overlay) continue;
    const std::string& v = P.s(rr.value);
    if (v == "finish.wear") return FIN_TILE_WEAR;
    if (v == "finish.etch") return FIN_TILE_ETCH;
    if (v == "finish.filigree") return FIN_TILE_FILIGREE;
    if (v == "finish.rune") return FIN_TILE_RUNE;
    if (v == "finish.gem") return FIN_TILE_GEM;
    if (v == "finish.glyph") return FIN_TILE_GLYPH;
    if (v == "finish.mark") return FIN_TILE_MARK;   // (P5) the maker's mark
  }
  return 0;
}
// (P2c ladder) a keyed view of a tile part (facing D): "<prefix><w>x<h>" parsed; the best that fits w x h (largest area)
const ViewRec* fitView(const Pool& P, const PartRec& p, const std::string& prefix, int bw, int bh) {
  const ViewRec* best = nullptr;
  int bestA = 0;
  for (uint32_t i = p.firstView; i < p.firstView + p.nViews; i++) {
    const ViewRec& v = P.views[i];
    if (v.facing != 0 || v.when) continue;
    const std::string& k = P.s(v.key);
    if (k.compare(0, prefix.size(), prefix) != 0) continue;
    int w = 0, h = 0;
    if (std::sscanf(k.c_str() + prefix.size(), "%dx%d", &w, &h) != 2) continue;
    if (w > bw || h > bh || w * h <= bestA) continue;
    best = &v; bestA = w * h;
  }
  return best;
}
// the fit prefix a tile part draws: the filigree's people motif (oak while a people has no drawing), else its own
// (P5) motif2: the look's own motif (DollRecipe::motif), the filigree's next choice when the slot's maker motif has no drawing
// (Highland's plaid before it is drawn keeps its knotwork); the mark draws only its motif (none drawn: "", nothing placed)
std::string fitPrefix(const Pool& P, const PartRec& p, uint8_t fin, const std::string& motif, const std::string& motif2 = std::string()) {
  std::string first;
  bool second = false;
  for (uint32_t i = p.firstView; i < p.firstView + p.nViews; i++) {
    const std::string& k = P.s(P.views[i].key);
    size_t e = k.size();
    while (e > 0 && (std::isdigit((unsigned char)k[e - 1]) || k[e - 1] == 'x')) e--;
    if (e == k.size() || e == 0) continue;
    const std::string pre = k.substr(0, e);
    if ((fin == FIN_TILE_FILIGREE || fin == FIN_TILE_MARK) && pre == motif) return pre;
    if (fin == FIN_TILE_FILIGREE && !motif2.empty() && pre == motif2) second = true;
    if (first.empty() || (fin == FIN_TILE_FILIGREE && pre == "oak")) first = pre;
  }
  if (fin == FIN_TILE_MARK) return std::string();
  return second ? motif2 : first;
}
const ViewRec* keyView(const Pool& P, const PartRec& p, const std::string& key) {
  const uint16_t id = P.strId(key);
  return id == 0xFFFF ? nullptr : viewOf(p, 0, id);
}
// (P2c) a library weapon (`role held <weapon>.<form>`): the gripping hand is drawn again over it
bool libraryHeld(const Pool& P, const PartRec& p) {
  for (const RoleRec& rr : p.roles)
    if (rr.cat == (uint8_t)Role::Held && P.s(rr.value).find('.') != std::string::npos) return true;
  return false;
}
// a tile part's target slot: `tags tile on-<slot>` (default cloth1)
Slot tileTarget(const Pool& P, const PartRec& p) {
  for (uint16_t t : p.tags) {
    const std::string& s = P.s(t);
    if (s.rfind("on-", 0) == 0)
      for (int k = 0; k < SLOT_COUNT; k++) if (s.compare(3, std::string::npos, slotName((Slot)k)) == 0) return (Slot)k;
  }
  return Slot::Cloth1;
}

// (P2c forge, D30) a held WEAPON (`role held <weapon>[.<form>]`: sword axe bow staff dagger mace greatsword spear stick): its
// metal draws in Slot::Blade, the ramp of its own metal (bladeKind); props in a posture's hand keep the armour's metal
bool heldWeapon(const Pool& P, const PartRec& p) {
  static const char* const kW[9] = {"sword", "axe", "bow", "staff", "dagger", "mace", "greatsword", "spear", "stick"};
  for (const RoleRec& rr : p.roles) {
    if (rr.cat != (uint8_t)Role::Held) continue;
    const std::string& v = P.s(rr.value);
    const std::string head = v.substr(0, v.find('.'));
    for (const char* w : kW) if (head == w) return true;
  }
  return false;
}
// (P2c forge) a `when` condition on a forge flag (the key fallback serves only these: the torso overlays keyed `cut-<v>`,
// `forge-<base>`...; every earlier `when` view keeps its own keys)
bool forgeCond(const std::string& c) {
  static const char* const kPre[] = {"cut-", "forge-", "shoulder-", "edge-", "collar-", "limb-", "fit-", "crest-", "boss-", "rim-", "glimb-",
                                     "blimb-", "form-"};
  for (const char* p : kPre) {
    const size_t n = std::strlen(p);
    for (size_t i = 0; i + n <= c.size(); i++)
      if ((i == 0 || c[i - 1] == ',' || c[i - 1] == '!') && c.compare(i, n, p) == 0) return true;
  }
  return false;
}
// a forge tile part: `tags tile` with `on-slot:<slot>` (a cut) or `on-edge:<hem|rim>` (an edge stamp)
bool forgeTileOf(const Pool& P, const PartRec& p, ForgeTile& out) {
  for (uint16_t t : p.tags) {
    const std::string& s = P.s(t);
    if (s.rfind("on-slot:", 0) == 0) {
      out.part = &p; out.edge = false;
      for (int k = 0; k < SLOT_COUNT; k++) if (s.compare(8, std::string::npos, slotName((Slot)k)) == 0) out.on = (Slot)k;
      return true;
    }
    if (s.rfind("on-edge:", 0) == 0) { out.part = &p; out.edge = true; out.rim = s.compare(8, std::string::npos, "rim") == 0; return true; }
  }
  return false;
}
// a forge tile's view for a facing (D serves a facing with none)
const ViewRec* forgeTileView(const Pool& P, const PartRec& p, int facing) {
  const ViewRec* v = viewOf(p, facing, P.ids.rest);
  return v ? v : viewOf(p, 0, P.ids.rest);
}
// the cut tiles and edge stamps laid into one armour or pauldron view (contracts 3c): a cut tile over its slot's lit pixels
// (the outline, R, G and W untouched), its cells moving the step (Ink k: by k - 2) and setting R P G N; an edge stamp along
// the lowest opaque row of each column (cells below add up to 2 rows in the hem's slot, '-' notches). The tiles ride the
// view's origin (the part's anchor). Returns v itself when nothing applies, else a transformed copy (thread-local slot k).
const ViewRec* forgeView(const Pool& P, const ViewRec& v, const std::vector<ForgeTile>& tiles, bool cut, int edgeMode, int facing, int k) {
  static thread_local ViewRec out[4];
  ViewRec* o = nullptr;
  auto copy = [&]() {
    if (o) return;
    o = &out[k & 3];
    *o = v;
    o->cells = v.cells;
    o->regions = v.regions;
  };
  auto posmod = [](int a, int m) { return ((a % m) + m) % m; };
  if (cut)
    for (const ForgeTile& ft : tiles) {
      if (ft.edge) continue;
      const ViewRec* T = forgeTileView(P, *ft.part, facing);
      if (!T || !T->w || !T->h) continue;
      copy();
      const bool treg = !T->regions.empty();
      for (int y = 0; y < o->h; y++)
        for (int x = 0; x < o->w; x++) {
          uint8_t& c = o->cells[(size_t)y * o->w + x];
          if (c < 2 || codeSlot(c) != ft.on || codeStep(c) < 1) continue;
          const uint8_t rg = o->regions.empty() ? 0 : o->regions[(size_t)y * o->w + x];
          if (rg == (uint8_t)Region::Trim || rg == (uint8_t)Region::Gem || rg == (uint8_t)Region::Wear) continue;
          const int tx = posmod(x - o->ox + T->ox, T->w), ty = posmod(y - o->oy + T->oy, T->h);
          const uint8_t tc = T->cells[(size_t)ty * T->w + tx];
          if (tc < 2) continue;
          if (codeSlot(tc) == Slot::Ink) c = code(codeSlot(c), std::clamp(codeStep(c) + codeStep(tc) - 2, 1, 4));
          // (silhouettes i2, the blind panel: "T1 padded: red dots read as a rash or blood") a quilt's ties sit in the
          // cloth's own shade (its seam step), never a second hue on the cream
          else if (codeSlot(tc) == Slot::Accent && codeSlot(c) == Slot::Cloth3) c = code(Slot::Cloth3, std::max(1, codeStep(c) - 2));
          else c = tc;
          const uint8_t tr = treg ? T->regions[(size_t)ty * T->w + tx] : 0;
          if (tr) {
            if (o->regions.empty()) o->regions.assign((size_t)o->w * o->h, 0);
            o->regions[(size_t)y * o->w + x] = tr;
          }
        }
    }
  if (edgeMode)
    for (const ForgeTile& ft : tiles) {
      if (!ft.edge || ft.rim != (edgeMode == 2)) continue;
      const ViewRec* T = forgeTileView(P, *ft.part, facing);
      if (!T || !T->w || !T->h) continue;
      copy();
      // two rows of room below the view
      const int w = o->w, h0 = o->h, h = h0 + 2;
      std::vector<uint8_t> cells((size_t)w * h, 0), regs;
      std::memcpy(cells.data(), o->cells.data(), (size_t)w * h0);
      if (!o->regions.empty()) { regs.assign((size_t)w * h, 0); std::memcpy(regs.data(), o->regions.data(), (size_t)w * h0); }
      std::vector<int> yb((size_t)w, -1);
      for (int x = 0; x < w; x++)
        for (int y = h0 - 1; y >= 0; y--) if (o->cells[(size_t)y * w + x] >= 2) { yb[(size_t)x] = y; break; }
      for (int x = 0; x < w; x++) {
        const int b = yb[(size_t)x];
        if (b < 0) continue;
        // the hem's material: the lowest non-ink pixel of the column within 3 rows of the boundary
        Slot hem = Slot::Ink;
        for (int y = b; y >= std::max(0, b - 3) && hem == Slot::Ink; y--) {
          const uint8_t c = o->cells[(size_t)y * w + x];
          if (c >= 2 && codeSlot(c) != Slot::Ink) hem = codeSlot(c);
        }
        if (hem == Slot::Ink) continue;
        const int tx = posmod(x - o->ox + T->ox, T->w);
        for (int ty = 0; ty < T->h; ty++) {
          const int Y = b + (ty - T->oy);
          if (Y < 0 || Y > b + 2 || Y >= h) continue;
          const uint8_t tc = T->cells[(size_t)ty * T->w + tx];
          if (!tc) continue;
          uint8_t& d = cells[(size_t)Y * w + x];
          if (tc == kErase) { if (Y <= b) d = 0; continue; }
          d = code(codeSlot(tc) == Slot::Ink ? Slot::Ink : hem, codeStep(tc));
          if (!regs.empty() && Y > b) regs[(size_t)Y * w + x] = 0;
        }
      }
      o->h = (uint16_t)h;
      o->cells.swap(cells);
      o->regions.swap(regs);
      if (!o->buildRow.empty()) {}   // (the build row is per column: unchanged)
    }
  return o ? o : &v;
}

// (ladder i2) heldPx marks the pixels a helm's erase may NOT clear: a helm's box clears hair, head, face, beard and ears
// (LADDER-ARMS's ask); the haft and head of a polearm, a scarf's tail, a collar passing behind the helm stay
bool isHeld(const PartRec& p) {
  for (const RoleRec& rr : p.roles) {
    const Role r = (Role)rr.cat;
    if (r == Role::Head || r == Role::Nose || r == Role::Brow || r == Role::Ears || r == Role::Marks || r == Role::Expr ||
        r == Role::Paint || r == Role::Earring || r == Role::Beard || r == Role::Hair || r == Role::Headwear || r == Role::Helm ||
        r == Role::Pauldron)
      return false;
  }
  return true;
}

}  // namespace

namespace comp {

// the frame's anchors: the body set's, the elf's stretch row, moved by the clip frame
void FramePaint::anchors(const DollRecipe& r_, int facing_, const FrameRec& f, FrameOut& o) {
  rp = &r_; facing = facing_; fp = &f; op = &o;
  const DollRecipe& r = r_;
  o.clear();
  o.nMarks = 0;
  o.behind = f.front == 0;
  baseAnchors(r, facing, base);
  for (int a = 0; a < ANCHOR_COUNT; a++) {
    Pt p = base[a];
    if (r.elf && a != (int)Anchor::Pivot) p.y -= 1;   // the stretch row: +1 px in the legs
    moveAnchor((Anchor)a, f, p);
    o.anchors[a] = p;
  }
}

// the items: every listed part at its view for the frame's key (tile parts gathered as tiles, never placed)
void FramePaint::gatherItems(const std::vector<int>* onlyParts, const char* forceKey, bool (*keep)(const PartRec&)) {
  const Pool& P = get();
  const Ids& I = P.ids;
  const DollRecipe& r = *rp;
  std::vector<Item>& items = g_items;
  items.clear();
  tile = Tile{};
  rtiles.clear();
  ftiles.clear();
  bandTile = nullptr;
  const uint16_t forced = forceKey ? P.strId(forceKey) : (uint16_t)0xFFFF;
  const std::vector<int>& list = onlyParts ? *onlyParts : r.parts;
  for (size_t li = 0; li < list.size(); li++) {
    const int pi = list[li];
    if (pi < 0 || pi >= (int)P.parts.size()) continue;
    const PartRec& p = P.parts[(size_t)pi];
    if (keep && !keep(p)) continue;
    if (partTaggedId(p, I.tile)) {   // a pattern tile: laid over the garments' cloth, never placed itself
      gatherTile(pi, p);
      continue;
    }
    gatherPart(pi, p, forceKey, forced);
  }
}

// a tile part: the trim band's, a forge cut or edge stamp, a region tile, else the pattern tile
void FramePaint::gatherTile(int pi, const PartRec& p) {
  const Pool& P = get();
  const Ids& I = P.ids;
  const DollRecipe& r = *rp;
  if (partTagged(pi, "band")) { bandTile = &p; return; }   // (D78) the band mode: widened R runs, laid per piece below
  const ViewRec* tv = viewOf(p, facing, I.rest);
  if (!tv) tv = viewOf(p, 0, I.rest);
  {
    ForgeTile ft;   // (P2c forge) a cut tile or an edge stamp: laid into the armour and pauldron views (forgeView)
    if (forgeTileOf(P, p, ft)) { ftiles.push_back(ft); return; }
  }
  if (uint32_t m = regionTileMask(P, p)) {   // (P2c) a region tile
    RegionTile rt;
    rt.v = tv; rt.mask = m; rt.fin = finishTileBit(P, p); rt.part = &p;
    rt.fit = partTagged(pi, "fit"); rt.stamp = partTagged(pi, "stamp");
    // (P2c forge) the forge tiles: `fit.<v>` on the body's parts only; `limb.<v>` on the regions of the slots that picked
    // it (the body's sleeves and thighs U F T, the gloves' H C X, the boots' B S K)
    for (const RoleRec& rr : p.roles) {
      if (rr.cat != (uint8_t)Role::Overlay) continue;
      const std::string& val = P.s(rr.value);
      if (val.rfind("fit.", 0) == 0) { rt.forge = true; rt.forgeSlot = HumanLook::TIER_BODY; }
      else if (val.rfind("limb.", 0) == 0) {
        rt.forge = true;
        const int lv = forge::valueOf(forge::AX_LIMB, val.c_str() + 5);
        auto rb = [](std::initializer_list<Region> rs) { uint32_t b = 0; for (Region x : rs) b |= 1u << (int)x; return b; };
        uint32_t allow = 0;
        if (lv > 0 && r.forgeAx[HumanLook::TIER_BODY][forge::AX_LIMB] == lv) allow |= rb({Region::Upper, Region::Fore, Region::Thigh});
        if (lv > 0 && r.forgeAx[HumanLook::TIER_GLOVES][forge::AX_LIMB] == lv) allow |= rb({Region::Hand, Region::Cuff, Region::Flare});
        if (lv > 0 && r.forgeAx[HumanLook::TIER_BOOTS][forge::AX_LIMB] == lv) allow |= rb({Region::Boot, Region::Shin, Region::Knee});
        rt.mask = m &= allow;
      }
    }
    if (rt.forge && !rt.mask) return;
    if (tv || rt.fit || rt.stamp) rtiles.push_back(rt);
    return;
  }
  if (tv && !tile.v) { tile.v = tv; tile.on = tileTarget(P, p); }
}

// a placed part: its key, its view (`replace`, the motif fallback), its item, its `when` overlays and key fallbacks
void FramePaint::gatherPart(int pi, const PartRec& p, const char* forceKey, uint16_t forced) {
  const Pool& P = get();
  const Ids& I = P.ids;
  const DollRecipe& r = *rp;
  const FrameRec& f = *fp;
  std::vector<Item>& items = g_items;
  uint16_t key = I.rest;
  const bool heldPart = I.held != 0xFFFF && p.channel == I.held;
  if (forceKey) key = forced;
  else if (p.channel != I.rest) {
    bool named = false;
    for (const KeyRec& k : f.keys) if (k.channel == p.channel) { key = k.key; named = true; }
    if (key == I.dash) return;
    // (P2c) `weapon=<deg>,front|back`: the held part's `a<deg>` view (rounded to 22.5); a named `held=<key>` wins
    if (heldPart && !named && f.weapon != -32768) key = I.angle[angleIndex(f.weapon)];
  }
  if (key == 0xFFFF) return;
  // (FORGE-LIMBS proto r3) the stir also replaces a named `idle` key: the woman's idle clip names hang=idle (her dress
  // hang's own idle view), so her hang-channel parts (cloak, hangs, skirts) never drew `rest` and never stirred (C13)
  const uint16_t kIdleKey = g_breeze ? P.strId("idle") : (uint16_t)0xFFFF;
  if (g_breeze && (key == I.rest || (kIdleKey != 0xFFFF && key == kIdleKey)) && !forceKey) {
    const uint16_t kBreeze = P.strId("breeze");
    if (kBreeze != 0xFFFF && viewOf(p, facing, kBreeze, 0)) key = kBreeze;
  }
  int vf = facing;
  const ViewRec* v = viewOf(p, facing, key, 0);
  if (!v && partTaggedId(p, I.anyfacing)) { v = viewOf(p, 0, key, 0); if (v) vf = 0; }   // (P2c) one D view for every facing
  // a `replace` view whose condition holds is drawn instead of the base
  for (uint32_t wi : p.whenViews) {
    const ViewRec& w = P.views[wi];
    if ((w.flags & 1) && w.facing == vf && w.key == key && condHolds(r, P.s(w.when))) { v = &w; break; }
  }
  // (forge round 2, contracts 3c.1) a chest motif's rest view serves every torso key it has no view for (clipped below to
  // the torso armour's opaque pixels): the motif lives inside the torso and moves with it
  bool motifFallback = false;
  if (!v && key != I.rest && !forceKey && overlayPrefix(P, p, "motif.")) {
    v = viewOf(p, vf, I.rest, 0);
    motifFallback = v != nullptr;
  }
  if (!v) return;
  Item it;
  it.part = &p; it.view = v; it.z = p.z; it.order = pi;
  it.clipTorso = motifFallback;
  // (INTEGRATE forge fix i1, C17: Thorniron's iridescent kit read 19 % second hue, its unbanded collar and crest 60 of the
  // 88 counted metal px) the forge collar and crest overlays band too: the sheen's two hues run over every forged piece
  it.iris = r.irisBands && (overlayPrefix(P, p, "skirt.") || overlayPrefix(P, p, "motif.") || overlayPrefix(P, p, "sleeve.") ||
                            overlayPrefix(P, p, "collar.") || overlayPrefix(P, p, "crest."));
  // (silhouettes i2, C17: the second hue on 11-14 % of the Heartland and Jade kits' metal, the seams too sparse to part the
  // lames) every iridescent piece alternates in 2-row bands: a two-tone figure at 1x
  it.irisRows = it.iris;
  it.layer = v->layer != kNone8 ? v->layer : p.layer;
  if (v->layer == kNone8 && p.side && (it.layer == (int)Layer::ArmNear || it.layer == (int)Layer::ArmFar))
    it.layer = (int)((p.side == 1 && facing == 3) || (p.side == 2 && facing == 2) ? Layer::ArmFar : Layer::ArmNear);
  // (P2c) a weapon placed by angle lies on held (front) or heldBack (back) unless its view names a layer
  if (heldPart && f.weapon != -32768 && v->layer == kNone8) it.layer = (int)(f.front ? Layer::Held : Layer::HeldBack);
  // (forge round 2, D41) shoulders stay on top: on a look with a cloak or the T5 surcoat the near pauldron draws on layer head
  // z -4, over the cloak's front (head z -5) and the surcoat, under the head
  if (r.pauldronTop && !f.turn && !g_deathPart && it.layer == (int)Layer::ArmNear && partHasRoleCat(p, Role::Pauldron)) { it.layer = (int)Layer::Head; it.z = -4; it.noErase = true; }
  it.anchor = (Anchor)(v->anchor != kNone8 ? v->anchor : p.anchor);
  // a turned frame's screen items (drawn level on the ground after the turn) are the views that NAME `anchor pivot`
  // (the drops, the shield's lie); a part anchored at the pivot by default (the legs, the sword) turns with the body
  // (polish i1: the legs' `lie` stood upright out of every corpse, "a boot block sticking up")
  const bool screenItem = v->anchor != kNone8 && (Anchor)v->anchor == Anchor::Pivot;
  if (g_screenMode && (screenItem != (g_screenMode == 2))) return;
  it.garment = (it.layer == (int)Layer::Torso || it.layer == (int)Layer::HangBack || it.layer == (int)Layer::HangFront) && partIsGarment(p);
  // (INTEGRATE silhouettes i1, STYLE_P2 28.7) the iridescent bands run across all the body armour (torso, hang, mail sleeves), so the
  // sheen reads at 1x on a figure whose motif or skirt is small (the owner: "iridescent sheen invisible")
  // (silhouettes i2, C17: the Heartland and Jade kits' second hue at 11-14 %, their metal mostly helm and shoulders) the
  // shoulders and the helm carry the bands too
  if (r.irisBands && !it.iris && (partHasRoleCat(p, Role::Armour) || partHasRoleCat(p, Role::Pauldron) || partHasRoleCat(p, Role::Helm)))
    it.iris = it.irisRows = true;
  items.push_back(it);
  // the optional regions (`when` overlays) of this view whose condition holds
  for (uint32_t wi : p.whenViews) {
    const ViewRec& w = P.views[wi];
    if ((w.flags & 1) || w.facing != vf || w.key != key || !condHolds(r, P.s(w.when))) continue;
    Item ov = it;
    ov.view = &w;
    ov.sub = (int)(wi - p.firstView) + 1;
    // (P5b round 3, DUNE) an overlay that names a layer of its own, other than its view's, draws on that layer (doll.h: "layer /
    // anchor this view's own"): the Dune T3 kulah's turban tail on layer neck, under the near arm, so a fist raised behind the
    // head stays before it. No overlay drawn before this round names a layer other than its base view's: they draw as before
    if (w.layer != kNone8 && w.layer != v->layer) ov.layer = w.layer;
    items.push_back(ov);
  }
  // (P2c forge, contracts 3c) the key fallback: a forge `when` overlay drawn only for key rest serves every key of its
  // facing that has none of its own, laid at that key view's origin and clipped to its opaque pixels
  if (key != I.rest && !forceKey)
    for (uint32_t wi : p.whenViews) {
      const ViewRec& w = P.views[wi];
      if ((w.flags & 1) || w.facing != vf || w.key != I.rest || !forgeCond(P.s(w.when)) || !condHolds(r, P.s(w.when))) continue;
      bool own = false;
      for (uint32_t wj : p.whenViews) {
        const ViewRec& x = P.views[wj];
        if (x.facing == vf && x.key == key && x.when == w.when) { own = true; break; }
      }
      if (own) continue;
      Item ov = it;
      ov.view = &w;
      ov.clip = v;
      ov.sub = (int)(wi - p.firstView) + 1;
      items.push_back(ov);
    }
}

void FramePaint::sortItems() {
  std::vector<Item>& items = g_items;
  // (forge round 2) the motif fallback's clip: the torso armour's view of this frame
  for (Item& it : items) {
    if (!it.clipTorso) continue;
    for (const Item& o : items)
      if (!o.sub && o.layer == (int)Layer::Torso && partHasRoleCat(*o.part, Role::Armour)) { it.clip = o.view; break; }
  }
  std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
    if (a.layer != b.layer) return a.layer < b.layer;
    if (a.z != b.z) return a.z < b.z;
    if (a.order != b.order) return a.order < b.order;
    return a.sub < b.sub;
  });
}

void FramePaint::place(Item& it) {
  const Pool& P = get();
  const Ids& I = P.ids;
  FrameOut& o = *op;
  std::vector<Item>& items = g_items;
  const Pt& a = o.anchors[(int)it.anchor];
  it.ax = CPX + a.x; it.ay = CPY + a.y;
  // (torso and hangs share one tile grid at rest, so a stripe runs on from the bodice into the skirt; each item's grid
  // rides its own anchor's motion, so the pattern never slides over the cloth)
  it.tx = it.ax - base[(int)it.anchor].x + base[(int)Anchor::Neck].x; it.ty = it.ay - base[(int)it.anchor].y + base[(int)Anchor::Neck].y;
  for (uint8_t m = 0; m < it.view->nMarks; m++) {
    const MarkRec& mk = P.marks[it.view->firstMark + m];
    const int s = markSlot(o, mk.name);
    if (s >= 0) o.marks[s] = Pt{it.ax + mk.x - it.view->ox - CPX, it.ay + mk.y - it.view->oy - CPY, true};
    if (mk.name == I.grip && I.grip != 0xFFFF) gripFrom = (int)(&it - items.data());
    if (mk.name == I.gripL && I.gripL != 0xFFFF) gripLFrom = (int)(&it - items.data());
    if (mk.name == I.handR && I.handR != 0xFFFF) handRFrom = (int)(&it - items.data());
    if (mk.name == I.handL && I.handL != 0xFFFF) handLFrom = (int)(&it - items.data());
  }
}

Pt FramePaint::markPt(uint16_t id) const {
  const FrameOut& o = *op;
  for (int i = 0; i < o.nMarks; i++) if (o.markIds[i] == id && o.marks[i].on) return o.marks[i];
  return Pt{};
}

// anchors first (grip-anchored items read the marks of the parts placed before them in every layer)
void FramePaint::placeAnchors() {
  const Pool& P = get();
  const Ids& I = P.ids;
  FrameOut& o = *op;
  std::vector<Item>& items = g_items;
  gripFrom = -1; gripLFrom = -1;
  handRFrom = -1; handLFrom = -1;
  for (Item& it : items) if (it.anchor != Anchor::Grip && it.anchor != Anchor::GripL && it.anchor != Anchor::Crown) place(it);
  // (P2c forge) the crown: the helm view's `mark crown`, else the head box's top centre (baseAnchors, moved with the head)
  if (I.crown != 0xFFFF) { const Pt cr = markPt(I.crown); if (cr.on) o.anchors[(int)Anchor::Crown] = cr; }
  for (Item& it : items) if (it.anchor == Anchor::Crown) place(it);
  // (i3) a grip that falls back to the hand mark takes that arm as the gripping arm too, so the fist is drawn over a
  // weapon carried at rest (the spear's upright carry facing down / up) instead of the haft running past the hand
  Pt g = markPt(I.grip); if (!g.on) { g = markPt(I.handR); if (g.on && gripFrom < 0) gripFrom = handRFrom; }
  Pt gl = markPt(I.gripL); if (!gl.on) { gl = markPt(I.handL); if (gl.on && gripLFrom < 0) gripLFrom = handLFrom; }
  o.anchors[(int)Anchor::Grip] = g.on ? g : Pt{0, -10, true};
  o.anchors[(int)Anchor::GripL] = gl.on ? gl : Pt{0, -10, true};
  for (Item& it : items) if (it.anchor == Anchor::Grip || it.anchor == Anchor::GripL) place(it);
  tipMark();
}

// (P2c ladder i2) a T6 piece in hand carries the edge spark from its grip to its tip (contracts 6b): where the held view
// marks no tip (the loco carries, the drawn idle), the tip is the view's cell farthest from its grip origin, and the grip
// the grip anchor; only T6 looks (every other frame's anchors as before)
void FramePaint::tipMark() {
  const Pool& P = get();
  const Ids& I = P.ids;
  const DollRecipe& r = *rp;
  FrameOut& o = *op;
  std::vector<Item>& items = g_items;
  if (r.pieceTier[3] >= 6 && I.tip != 0xFFFF && I.grip != 0xFFFF && !markPt(I.tip).on)
    for (const Item& it : items) {
      if (it.anchor != Anchor::Grip) continue;
      bool held = false;
      for (const RoleRec& rr : it.part->roles) if (rr.cat == (uint8_t)Role::Held) held = true;
      if (!held) continue;
      const ViewRec& v = *it.view;
      int bx = -1, by = -1, bd = -1;
      for (int y = 0; y < v.h; y++)
        for (int x = 0; x < v.w; x++)
          if (v.cells[(size_t)y * v.w + x] >= 2) {
            const int d = (x - v.ox) * (x - v.ox) + (y - v.oy) * (y - v.oy);
            if (d > bd) { bd = d; bx = x; by = y; }
          }
      if (bd < 16) break;   // a stub (a dagger's knob, a fist's edge): no run
      const int st = markSlot(o, I.tip);
      if (st >= 0) o.marks[st] = Pt{it.ax + bx - v.ox - CPX, it.ay + by - v.oy - CPY, true};
      if (!markPt(I.grip).on) { const int sg = markSlot(o, I.grip); if (sg >= 0) o.marks[sg] = Pt{it.ax - CPX, it.ay - CPY, true}; }
      break;
    }
}

// paint back to front
void FramePaint::paintBackToFront() {
  const Pool& P = get();
  const DollRecipe& r = *rp;
  std::vector<Item>& items = g_items;
  std::vector<uint8_t>& heldPx = g_heldPx;
  // the frame's planes, looked up once (stamp writes them for every pixel)
  pool = &P; itemsV = &items; heldPxV = &heldPx; bodyPxV = &g_bodyPiece; depthPxV = &g_depthPx;
  probeOwnerV = g_probeOwner; probeRegionV = g_probeRegion;
  tileOn = (uint8_t)tile.on;
  twoHand = r.has("twohand");
  // (ladder i2) the pixels a held weapon wrote: a helm's erase cells (the 1-2 rows over its dome, there to clear hair) clear
  // hair, head and face but never the haft or head of a polearm passing behind the helm
  heldPx.assign((size_t)CW * CH, 0);
  shieldPx.assign((size_t)CW * CH, 0);
  // (P5b2 fix r3, judge item 2: C3b "hair over the helm's rim", 2,176 frames, the m9 looks' forced hair under the signed T4-T6
  // heads) a people's signed head is drawn over every hair style the census gives, and the drawn heads' erase cells clear only
  // the hair they were drawn over: a long, loose or mohawk style stood above the rim at the back. The rule, not the cells: on a
  // signed helm's look no hair pixel stays above the shell's top in a column the shell covers (hairUnderRim, after the paint)
  hhOn = fixOn(FIX_SIG) && !r.flags.empty() && std::any_of(r.flags.begin(), r.flags.end(), [](const std::string& f) { return f.rfind("sighelm-", 0) == 0; });
  if (hhOn) hhPx.assign((size_t)CW * CH, 0);
  // (P5b2 fix r3, judge item 2: C3a, 1,953 one-pixel holes in the signed heads, shoulders and skirts) the signed pieces are
  // drawn over every body, hair and stance the census gives; where two of them meet a cell or two of the canvas showed through,
  // a pinhole no 32 px figure reads as a shape. closeGearPockets, after the paint: a pocket the flood from the canvas edge never
  // reaches, walled by gear on every side (1-2 px; up to 8 where a head or shoulder piece walls it), takes its darkest neighbour (the line between the two pieces)
  pocketsOn = fixOn(FIX_SIG) && std::any_of(r.flags.begin(), r.flags.end(), [](const std::string& f) { return f.rfind("sig", 0) == 0 && f.find('-') != std::string::npos; });
  // (never on the approved slice's own kits at T1 and T3, Fjordfolk, Imperial and Jade bodies: their frames are frozen as the
  // owner signed them, STYLE_P5B 9. At T5 the slice kits' pinholes are real defects, ruled R1 in STYLE_P5B 10.1: the
  // Fjordfolk pelt beside the scale coat, the Imperial woman's skirt strips, the Jade man's bell skirt and sode close here too,
  // one pixel each, and slice_same is re-recorded for exactly the frames 10.1 lists)
  const bool sigLook = pocketsOn;   // (P5b3 GRIPS) a signed look under FIX_SIG, the slice kits included (the pad over the grip)
  if (pocketsOn)
    for (const char* sp : {"sigbody-fjordfolk", "sigbody-imperial", "sigbody-jade"})
      if (r.has(sp))
        for (int t : {1, 3})
          if (r.has(("armour.t" + std::to_string(t)).c_str()) && !r.has(("armour.t" + std::to_string(t + 1)).c_str())) pocketsOn = false;
  // (wave-2 fix round 3, D82; SHIELDS' r2 proposal) the gripL shield's last pixel under each cell (code, region, item): a part
  // that paints over the shield and is then erased by a later part's `-` cell shows the shield again, never a hole through it
  shieldKeep.assign((size_t)CW * CH, 0);
  shieldKeepRg.assign((size_t)CW * CH, 0);
  shieldKeepOw.assign((size_t)CW * CH, -1);
  g_depthPx.assign((size_t)CW * CH, 0);
  // handOnly: (P2c) the gripping arm's H pixels drawn again over the weapon it holds
  // (P2c forge) the hem an edge stamp follows: the armour hang's when the frame draws one, else the torso's
  hangArmour = false;
  if (!ftiles.empty())
    for (const Item& it : items)
      if (!it.sub && (it.layer == (int)Layer::HangBack || it.layer == (int)Layer::HangFront) && partHasRoleCat(*it.part, Role::Armour)) hangArmour = true;
  if (g_bodyPiece.size() != (size_t)CW * CH) g_bodyPiece.assign((size_t)CW * CH, 0);
  else std::fill(g_bodyPiece.begin(), g_bodyPiece.end(), (uint8_t)0);
  g_probeGrip = gripFrom; g_probeGripL = gripLFrom;
  // (P5b3 GRIPS) the grip mark's pixel (the arm's views placed it in gatherItems), for the pauldron over the grip facing U below
  int gripQ = -1;
  if (gripFrom >= 0 && P.ids.grip != 0xFFFF)
    for (int i = 0; i < op->nMarks; i++)
      if (op->markIds[i] == P.ids.grip && op->marks[i].on) {
        const int X = CPX + op->marks[i].x, Y = CPY + op->marks[i].y;
        if (X >= 0 && Y >= 0 && X < CW && Y < CH) gripQ = Y * CW + X;
      }
  for (const Item& it : items) {
    const uint8_t gripWas = gripQ >= 0 ? op->code[(size_t)gripQ] : 0;
    paintItem(it, false);
    // (P5b2 fix r3, judge item 2: C3d "no hand pixels on the gripping arm", 1,984 frames) a fist that holds a drawn weapon is
    // before the body. Two of the signed kits' pieces are drawn after the arm and went over it: D41's near pauldron, lifted to
    // layer head on a cloaked look (the Imperial T3/T5 kit facing D: the drawn stance, the follow-through), and the wide sleeve
    // and the scarf facing D (the Dune T3 kit: the greatsword's windup brings the right fist to the left shoulder). After such a
    // piece the gripping arm's hand (H) pixels are drawn again, as after the weapon it holds. Only with a weapon placed by angle
    // (the drawn stance and the swings, never the carry at rest, whose hand hangs under the lifted shoulder as the approved slice
    // draws it), never facing U (the fists before the chest, the back toward us), never a far arm; signed looks (FIX_SIG)
    // (P5b3 GRIPS, judge r4 item 2: C3d "the grip outside the fist", 64 frames, the Imperial T5 kit's axe and dagger strike
    // and follow facing U) facing U too, but only where the same side's pauldron (at its own layer, not lifted) was drawn over
    // the grip pixel itself: the strike raises the fist to the shoulder, and the layered sode drawn after the arm hid all of it
    // but a 1 px sliver (the dagger), or hid it in
    // the weaponless frame while the drawn one showed it only through the weapon's own hand pass (the axe). A fist the pad
    // leaves the grip of (the drawn idle and walk) keeps the pad over it, as before; the cloth case stays facing D; signed
    // looks only (an unsigned look's frames stay as p4f1 drew them: S5.rig2)
    const bool padOverGripU = sigLook && facing == 1 && gripQ >= 0 && op->code[(size_t)gripQ] != gripWas;
    if (fixOn(FIX_SIG) && fp && fp->weapon != -32768 && (facing != 1 || padOverGripU)) {
      const bool liftedPad = (it.noErase && it.layer == (int)Layer::Head && partHasRoleCat(*it.part, Role::Pauldron)) ||
                             (padOverGripU && partHasRoleCat(*it.part, Role::Pauldron));
      const std::string& nm = P.s(it.part->name);
      const bool cloth = facing == 0 && (nm.find("sleeve") != std::string::npos || partHasRoleCat(*it.part, Role::Neck));
      if (liftedPad || cloth) {
        auto fist = [&](int from) {
          if (from < 0 || &items[(size_t)from] == &it) return;
          const int side = items[(size_t)from].part->side;
          const bool far = (side == 1 && facing == 3) || (side == 2 && facing == 2);
          if (far || (liftedPad && side != it.part->side)) return;
          paintItem(items[(size_t)from], true);
        };
        fist(gripFrom);
        if (twoHand) fist(gripLFrom);
      }
    }
    // (P2c) the hand over the grip: after a library weapon on the grip, the main hand; on a two-handed weapon the off hand
    // too; after a library weapon on gripL (the bow), the bow hand
    if (!libraryHeld(P, *it.part)) continue;
    if (it.anchor == Anchor::Grip) {
      if (gripFrom >= 0 && &items[(size_t)gripFrom] != &it) paintItem(items[(size_t)gripFrom], true);
      if (twoHand && gripLFrom >= 0 && &items[(size_t)gripLFrom] != &it) paintItem(items[(size_t)gripLFrom], true);
    } else if (it.anchor == Anchor::GripL && gripLFrom >= 0 && &items[(size_t)gripLFrom] != &it) {
      paintItem(items[(size_t)gripLFrom], true);
    }
  }
  if (hhOn) hairUnderRim();
  if (pocketsOn) closeGearPockets();
}

void FramePaint::closeGearPockets() {
  FrameOut& o = *op;
  const int x0 = std::max(0, o.bx0 - 1), x1 = std::min(CW - 1, o.bx1 + 1), y0 = std::max(0, o.by0 - 1), y1 = std::min(CH - 1, o.by1 + 1);
  if (x1 < x0 || y1 < y0) return;
  const int W = x1 - x0 + 1, H = y1 - y0 + 1;
  auto at = [&](int x, int y) { return (size_t)(y0 + y) * CW + (x0 + x); };
  auto clear = [&](int x, int y) { return o.code[at(x, y)] < 2; };
  std::vector<uint8_t> seen((size_t)W * H, 0);
  std::vector<int> st;
  for (int x = 0; x < W; x++) for (int y : {0, H - 1}) if (clear(x, y) && !seen[(size_t)y * W + x]) { seen[(size_t)y * W + x] = 1; st.push_back(y * W + x); }
  for (int y = 0; y < H; y++) for (int x : {0, W - 1}) if (clear(x, y) && !seen[(size_t)y * W + x]) { seen[(size_t)y * W + x] = 1; st.push_back(y * W + x); }
  while (!st.empty()) {
    const int k = st.back(); st.pop_back();
    const int x = k % W, y = k / W;
    const int nb[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
    for (auto& n : nb) {
      if (n[0] < 0 || n[1] < 0 || n[0] >= W || n[1] >= H) continue;
      const int q = n[1] * W + n[0];
      if (seen[(size_t)q] || !clear(n[0], n[1])) continue;
      seen[(size_t)q] = 1; st.push_back(q);
    }
  }
  auto gear = [&](size_t q) { return ((*bodyPxV)[q] & 2) || shieldPx[q]; };
  for (int k0 = 0; k0 < W * H; k0++) {
    if (seen[(size_t)k0] || !clear(k0 % W, k0 / W)) continue;
    std::vector<int> pocket{k0};
    seen[(size_t)k0] = 1;
    bool walled = true, helm = false;
    for (size_t j = 0; j < pocket.size(); j++) {
      const int x = pocket[j] % W, y = pocket[j] / W;
      const int nb[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
      for (auto& n : nb) {
        const int q = n[1] * W + n[0];
        if (clear(n[0], n[1])) { if (!seen[(size_t)q]) { seen[(size_t)q] = 1; pocket.push_back(q); } }
        else if (!gear(at(n[0], n[1]))) walled = false;
        else {
          const uint8_t b = (*bodyPxV)[at(n[0], n[1])];
          if ((b & 16) || ((b & 3) == 3 && !(b & 4))) helm = true;   // a helm's or a pauldron's pixel
        }
      }
    }
    // (up to 8 px where a head or shoulder piece walls it, a pinhole of 1-2 px among the body's plates and skirts; a larger
    // pocket among them is that drawing's negative space and stays)
    if (!walled || pocket.size() > (helm ? 8u : 2u)) continue;
    // fill from the outside in: each cell takes its darkest drawn neighbour (cells filled earlier count)
    for (int pass = 0; pass < 8; pass++) {
      bool left = false;
      for (int k : pocket) {
        const int x = k % W, y = k / W;
        const size_t q = at(x, y);
        if (o.code[q] >= 2) continue;
        size_t best = (size_t)-1;
        for (const auto& n : {std::pair<int, int>{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}}) {
          const size_t m = at(n.first, n.second);
          if (o.code[m] < 2) continue;
          if (best == (size_t)-1 || codeStep(o.code[m]) < codeStep(o.code[best])) best = m;
        }
        if (best == (size_t)-1) { left = true; continue; }
        o.code[q] = o.code[best];
        if (!g_fpUnder.empty() && g_fpUnder.size() == o.code.size()) g_fpUnder[q] = g_fpUnder[best];
        (*bodyPxV)[q] = (*bodyPxV)[best]; (*depthPxV)[q] = (*depthPxV)[best]; (*heldPxV)[q] = (*heldPxV)[best]; shieldPx[q] = shieldPx[best];
        if (hhOn) hhPx[q] = hhPx[best];
        if (probeOwnerV) { (*probeOwnerV)[q] = (*probeOwnerV)[best]; (*probeRegionV)[q] = (*probeRegionV)[best]; }
      }
      if (!left) break;
    }
  }
}

// (P5b2 fix r3) the hair under the helm: in each column the helm's shell (not its crest or plume) covers, a hair pixel left above
// the shell's top is cleared (C3b's rim, P2c envelope: the shell over the head box + 1 column; a turned frame keeps its own)
void FramePaint::hairUnderRim() {
  FrameOut& o = *op;
  if (fp && fp->turn) return;
  const int hx0 = CPX + base[(int)Anchor::Head].x - 3, hx1 = CPX + base[(int)Anchor::Head].x + 18;   // (the box + 1, and the bob of a walk or run: 2 more)
  for (int X = std::max(0, hx0); X <= std::min(CW - 1, hx1); X++) {
    int top = -1;
    for (int Y = 0; Y < CH; Y++) if (hhPx[(size_t)Y * CW + X] == 2 && o.code[(size_t)Y * CW + X] >= 2) { top = Y; break; }
    if (top < 0) continue;
    for (int Y = 0; Y < top; Y++) {
      const size_t q = (size_t)Y * CW + X;
      if (hhPx[q] != 1 || o.code[q] < 2) continue;
      o.code[q] = 0;
      if (!g_fpUnder.empty() && g_fpUnder.size() == o.code.size()) g_fpUnder[q] = 0;
      (*bodyPxV)[q] = 0; (*depthPxV)[q] = 0; hhPx[q] = 0;
      if (probeOwnerV) { (*probeOwnerV)[q] = -1; (*probeRegionV)[q] = 0; }
    }
  }
}

// one item: its derived state, then its pixels
void FramePaint::paintItem(const Item& it, bool handOnly) {
  const Pool& P = get();
  const DollRecipe& r = *rp;
  const FrameRec& f = *fp;
  ItemPaint ip;
  ip.it = &it;
  ip.handOnly = handOnly;
  // (forge i3) a culture alloy's metal head (DollRecipe::guiseHead) is its armour too: the glints and the sheen's tell may sit on it
  // (INTEGRATE silhouettes i1) bit 2: any gear piece (armour, helm, pauldron, overlay) for the despeckle pass
  const uint8_t bodyPiece = (uint8_t)(((partHasRoleCat(*it.part, Role::Armour) || partHasRoleCat(*it.part, Role::Pauldron) ||
                                        (r.guiseHead && partHasRoleCat(*it.part, Role::Headwear))) ? 1 : 0) |
                                      ((partHasRoleCat(*it.part, Role::Armour) || partHasRoleCat(*it.part, Role::Pauldron) ||
                                        partHasRoleCat(*it.part, Role::Helm) || partHasRoleCat(*it.part, Role::Overlay)) ? 2 : 0) |
                                      ((partHasRoleCat(*it.part, Role::Armour) || P.s(it.part->name).find("armour/motif/") != std::string::npos) ? 4 : 0) |
                                      (partHasRoleCat(*it.part, Role::Helm) ? 16 : 0));   // (envelope fix r2) bit 16: the helm (D62's helm rim)
  const ViewRec* vp = stampEdges(it, handOnly);
  const ViewRec& v = *vp;
  const uint8_t heldMark = isHeld(*it.part) ? 1 : 0;
  // (wave-2 fix round, D82) the gripL shield: its pixels survive every other part's erase cells
  const uint8_t shieldMark = fixOn(FIX_SHIELD) && partHasRoleCat(*it.part, Role::Shield) && it.anchor == Anchor::GripL ? 1 : 0;
  trimBandOf(it, v, handOnly, ip);
  const uint8_t depthMark = partHasRoleCat(*it.part, Role::Held) ? 2 : (partHasRoleCat(*it.part, Role::Shield) || partHasRoleCat(*it.part, Role::Sheath) ||
                             it.layer == (int)Layer::Back || it.layer == (int)Layer::HeldBack || it.layer == (int)Layer::FxBack ||
                             it.layer == (int)Layer::Fx) ? 1 : 0;   // (2: a held weapon)
  // only a helm's erase spares held gear (its box clears hair round the dome); every other part's erase cuts as drawn
  bool helmErase = false;
  for (const RoleRec& rr : it.part->roles) if (rr.cat == (uint8_t)Role::Helm || rr.cat == (uint8_t)Role::Headwear) helmErase = true;
  const int sway = it.part->sway && f.sway ? f.sway : 0, swayFrom = (int)v.h - std::min<int>(it.part->sway, v.h);
  buildColumns(r, v, dst0, dst1);
  const bool stretch = r.elf && v.stretch >= 0;
  const bool hasReg = !v.regions.empty();
  const bool tiled = tile.v && it.garment;
  // (P2c ladder) the piece's tier slot (its finish regions and tiles) and the held weapon's own metal (Slot::Blade)
  const int ts = partTierSlot(it.order);
  // (P2c forge, D30) every held weapon's metal in Slot::Blade (its ramp is bladeKind's; equal to Metal when they match)
  const bool toBlade = heldWeapon(P, *it.part) || (r.blade && libraryHeld(P, *it.part));
  // (forge i3) a culture alloy's metal head (a coif, turban helm, jingasa or aventail cut from the headwear library): its
  // cloth in the armour's metal
  bool toMetal = false;
  if (r.guiseHead)
    for (const RoleRec& rr : it.part->roles) if (rr.cat == (uint8_t)Role::Headwear) toMetal = true;
  // (envelope fix round, D58) a brass or bronze everyday alloy (resolver flag `brassbody`): the torso armour and the chest
  // motif (armour/motif/) in the alloy's brass: their cloth and trim (Accent) cells; leather straps keep their leather (forge fix i1)
  bool brassBody = false;
  if (r.has("brassbody")) {
    for (const RoleRec& rr : it.part->roles) if (rr.cat == (uint8_t)Role::Armour) brassBody = true;
    if (overlayPrefix(P, *it.part, "motif.")) brassBody = true;
  }
  // (P5b) a signed leather body (`sigleather`): the styled skirt's and sleeves' metal draws in the leather (a T1 kusazuri,
  // pteruges and the split hem are leather; their steps and outline kept)
  bool toLeather = r.has("sigleather") && (overlayPrefix(P, *it.part, "skirt.") || overlayPrefix(P, *it.part, "sleeve."));
  // (P5b2 fix r2, judge item 4: C12b's step-4 metal fell T1 -> T2 on the Imperial line, the T1 cap's brass glints gone into the
  // T2 leather) the leather helm keeps its brass fittings: its Metal 2 (cheek-guard hinges, a knob, rivets) stays Metal 2
  bool helmLeather = false;
  if (r.has("sighelmleather"))
    for (const RoleRec& rr : it.part->roles) if (rr.cat == (uint8_t)Role::Helm) helmLeather = true;
  // (P5b fix r1, STYLE_P5B 2.4) a gilt people's signed T5 helm (`sighelmgilt`) or shield (`sigshieldgilt`): the piece's fittings
  // (Metal 2: the trim band, the brow band, the cheek guards, a rim, the horns' gilt) in the gilt, the piece's own steel and the
  // body's Metal 2 bands kept
  bool toGilt = false, toGiltGrowth = false;
  for (const RoleRec& rr : it.part->roles) {
    if (rr.cat == (uint8_t)Role::Helm && r.has("sighelmgilt")) toGilt = true;
    if (rr.cat == (uint8_t)Role::Shield && r.has("sigshieldgilt")) toGilt = true;
    // (the legend growth in the gilt: `sighelmgrowthgilt`, the helm family's `headwear/helm/<family>_legend` part, its metal
    // steps 1-4 in Device; its outline, Metal 2 and the trim band as drawn: the pixels the kuwagata re-cut views drew)
    if (rr.cat == (uint8_t)Role::Helm && r.has("sighelmgrowthgilt")) {
      const std::string& nm = P.s(it.part->name);
      toGiltGrowth = nm.size() > 7 && nm.compare(nm.size() - 7, 7, "_legend") == 0;
    }
  }
  finishPlacements(v, ts, hasReg);
  irisBands(it, v, hasReg);
  ip.ts = ts;
  fingerprintOf(it, v, ip);
  ip.v = vp;
  ip.bodyPiece = bodyPiece; ip.heldMark = heldMark; ip.shieldMark = shieldMark; ip.depthMark = depthMark;
  ip.helmErase = helmErase;
  ip.hh = 0;
  if (hhOn) {
    if (partHasRoleCat(*it.part, Role::Hair)) ip.hh = 1;
    else if (partHasRoleCat(*it.part, Role::Helm)) {
      const std::string& nm = P.s(it.part->name);
      ip.hh = (nm.find("helm/crest/") != std::string::npos || nm.find("_crest") != std::string::npos || nm.find("_plume") != std::string::npos) ? 3 : 2;
    }
  }
  ip.sway = sway; ip.swayFrom = swayFrom;
  ip.stretch = stretch; ip.hasReg = hasReg; ip.tiled = tiled;
  ip.ts = ts;
  ip.toBlade = toBlade; ip.toMetal = toMetal; ip.brassBody = brassBody; ip.toLeather = toLeather; ip.helmLeather = helmLeather; ip.toGilt = toGilt; ip.toGiltGrowth = toGiltGrowth;
  paintPixels(ip);
}

// (P2c forge) the cut tiles and edge stamps laid into the body's armour and pauldron views (a `when` overlay keyed on a
// forge flag is the style's own drawing: no tile over it; edges only on the base views)
const ViewRec* FramePaint::stampEdges(const Item& it, bool handOnly) {
  const Pool& P = get();
  const ViewRec* vp = it.view;
  if (!ftiles.empty() && !handOnly && partTierSlot(it.order) == HumanLook::TIER_BODY) {
    const bool arm = partHasRoleCat(*it.part, Role::Armour), pad = partHasRoleCat(*it.part, Role::Pauldron);
    if (arm || pad) {
      const bool forgeOverlay = it.sub && forgeCond(P.s(it.view->when));
      const bool hangItem = it.layer == (int)Layer::HangBack || it.layer == (int)Layer::HangFront;
      const int edgeMode = it.sub ? 0 : pad ? 2 : (arm && (hangArmour ? hangItem : it.layer == (int)Layer::Torso)) ? 1 : 0;
      vp = forgeView(P, *it.view, ftiles, !forgeOverlay, edgeMode, it.view->facing, 0);
    }
  }
  return vp;
}

// (wave-2 fix round, D78) the trim band: on a T4-T6 piece every 4-connected run of R pixels widened inward from the part's
// outline to the tier's width (T4 1 px, T5-T6 2), painted from the trim tile's view for that tier; only adds trim (a run at
// width keeps every pixel), never over a G or N pixel, an outline pixel or a pixel of another part (the view's own pixels)
void FramePaint::trimBandOf(const Item& it, const ViewRec& v, bool handOnly, ItemPaint& ip) {
  const Pool& P = get();
  const DollRecipe& r = *rp;
  const ViewRec* bandView = nullptr;
  bool bandOn = false;
  int bandW = 0;   // (D93) the band's width on this piece after its slot's level (0: no rows added)
  {
    const int bts = partTierSlot(it.order);
    const int btier = bts >= 0 ? r.pieceTier[bts] : 0;
    // (wave-2 fix round 4, D93) the slot's tuned level: the floor mode continues the band along the piece's edges, the
    // ceiling mode narrows the band's own rows (T4-T6) or paints the shortest drawn R runs in the piece's metal (T3); never
    // the weapon in hand
    const bool tun = bts >= 0 && bts != 3 && bts != 7 && fixOn(FIX_TRIM4);
    const int tg = facing >= 2 ? 1 : 0;   // (fix round 2) the facing pair's level
    const int lvl = tun ? r.trimLevel[tg][bts] : 0, cut = tun ? r.trimCeil[tg][bts] : 0;
    int edges = 0;
    if (lvl >= 1) {
      bool armour = false, pauld = false, cloak = false, skirt = false, helm = false;
      for (const RoleRec& rr : it.part->roles) {
        const Role ro = (Role)rr.cat;
        if (ro == Role::Armour) armour = true;
        if (ro == Role::Pauldron) pauld = true;
        if (ro == Role::Cloak) cloak = true;
        if (ro == Role::Helm || ro == Role::Headwear) helm = true;
        if (ro == Role::Overlay) {
          const std::string& ov = P.s(rr.value);
          if (ov.rfind("skirt.", 0) == 0 || ov == "tabard" || ov == "sash" || ov.rfind("mail", 0) == 0) skirt = true;
        }
      }
      const bool hang = it.layer == (int)Layer::HangBack || it.layer == (int)Layer::HangFront;
      if (helm) edges = 0;
      else if (pauld) edges = TE_RIM | TE_PLATE;
      else if (armour && it.layer == (int)Layer::Torso) edges = TE_HEM | TE_NECK | TE_ARM | TE_PLATE;
      else if (cloak && !armour) edges = TE_HEM;
      else if (armour || hang || skirt) edges = TE_HEM | TE_PLATE;
    }
    const bool t3cut = btier == 3 && cut >= 1 && !v.regions.empty() && !handOnly;
    // (a view with no regions takes the band only for the floor mode's edges: D78's band has no marked run to widen there)
    if ((bandTile && !handOnly && (!v.regions.empty() || (lvl >= 1 && edges)) && btier >= 4 && btier <= 6) || t3cut) {
      bandView = btier >= 4 && bandTile ? trimBandView(P, *bandTile, btier) : nullptr;
      bandOn = true;
      const int tierW = btier >= 4 ? (btier >= 5 ? 2 : 1) : 0;
      // the ceiling mode at T4-T6: cut 1 the band's added rows 2 px to 1, cut 2 off; the floor's edges at the tier's
      // width, 1 px once the ceiling narrows the band
      bandW = btier >= 4 ? (cut >= 2 ? 0 : cut == 1 ? std::min(tierW, 1) : tierW) : 0;
      const int edgeW = btier >= 4 ? (cut >= 1 ? 1 : tierW) : 0;
      trimBand(v, bandW, bandReg, bandRow, lvl, edges, edgeW, btier == 3 ? cut : 0);
    }
    static const bool kBandDbg = std::getenv("EMB_BAND_DEBUG") != nullptr;   // (fix round 3) each tuned piece's band, printed
    if (tun && (lvl || cut) && kBandDbg) {
      int nR = 0, nAdd = 0, nPx = 0;
      if (bandOn)
        for (size_t k = 0; k < bandRow.size(); k++) { if (bandRow[k] >= 0) nR++; if (bandRow[k] >= 0 && (v.regions.size() != bandRow.size() || v.regions[k] != (uint8_t)Region::Trim)) nAdd++; }
      for (uint8_t c : v.cells) if (c >= 2 && c != kErase) nPx++;
      std::printf("      band f%d slot %d T%d %s layer %d lvl %d cut %d edges %d tile %d regions %d on %d: px %d R %d added %d\n", facing, bts, btier,
                  P.s(it.part->name).c_str(), it.layer, lvl, cut, edges, bandTile ? 1 : 0, v.regions.empty() ? 0 : 1, bandOn ? 1 : 0, nPx, nR, nAdd);
    }

  }
  ip.bandView = bandView;
  ip.bandOn = bandOn;
}

// (P2c ladder) LADDER-FINISH's placements on this view: the P box (fit tiles) and the W / G blobs (stamp tiles)
void FramePaint::finishPlacements(const ViewRec& v, int ts, bool hasReg) {
  const Pool& P = get();
  const DollRecipe& r = *rp;
  fitAt.assign(rtiles.size(), Place{});
  fitAt0.clear();   // (P5) the pre-P5 fits where the slot's maker motif moved one (empty: none moved)
  blobs.clear();
  bool anyStamp = false;
  bool forgeFit = false;   // (P2c forge) a fittings tile placed on this slot's P box
  for (const RegionTile& rt : rtiles) if (rt.forgeSlot >= 0 && rt.forgeSlot == ts && rt.fit) forgeFit = true;
  if (hasReg && ts >= 0 && (r.tilesAt(ts) || forgeFit)) {
    int px0 = v.w, py0 = v.h, px1 = -1, py1 = -1;
    for (int y = 0; y < v.h; y++)
      for (int x = 0; x < v.w; x++)
        if (v.cells[(size_t)y * v.w + x] >= 2 && v.regions[(size_t)y * v.w + x] == (uint8_t)Region::Panel) {
          px0 = std::min(px0, x); py0 = std::min(py0, y); px1 = std::max(px1, x); py1 = std::max(py1, y);
        }
    for (size_t k = 0; k < rtiles.size(); k++) {
      const RegionTile& rt = rtiles[k];
      if (rt.forgeSlot >= 0 ? rt.forgeSlot != ts : (!rt.fin || !((r.tilesAt(ts) | r.finTiles[ts]) & rt.fin))) continue;   // (P5: the etch a mark replaced is placed for the under plane)
      if (rt.stamp) anyStamp = true;
      if (!rt.fit || px1 < 0) continue;
      const int bw = px1 - px0 + 1, bh = py1 - py0 + 1;
      // (P5) the slot's maker motif (the filigree and the mark); a slot with no maker keeps the look's motif
      const std::string pre = r.fpOn(ts) ? fitPrefix(P, *rt.part, rt.fin, r.motifAt(ts), r.motif) : fitPrefix(P, *rt.part, rt.fin, r.motif);
      if (pre.empty()) continue;
      const ViewRec* fv = fitView(P, *rt.part, pre, bw, bh);
      // (P5) the people's filigree where it fits this panel as well as the look's own drawing does; a panel its motif has no
      // fit as large for (the T5 set-stone fits of the shoulders) keeps the look's (FIX_BATCH_P5 3.3: never a lesser finish)
      if (rt.fin == FIN_TILE_FILIGREE && r.fpOn(ts) && pre != r.motif) {
        const ViewRec* own = fitView(P, *rt.part, fitPrefix(P, *rt.part, rt.fin, r.motif), bw, bh);
        if (own && (!fv || own->w * own->h > fv->w * fv->h)) fv = own;
      }
      if (fv) fitAt[k] = Place{fv, px0 + (bw - fv->w) / 2, py0 + (bh - fv->h) / 2};
      // (P5, F6) the fit the pre-P5 figure drew here (the look's motif), so recolour files the pre-P5 code under a moved
      // filigree pixel and the post passes read the figure as it was (no pass moves a pixel outside the panel)
      if (r.fpOn(ts) && rt.fin == FIN_TILE_FILIGREE && pre != r.motif) {
        const std::string pre0 = fitPrefix(P, *rt.part, rt.fin, r.motif);
        const ViewRec* f0 = pre0.empty() ? nullptr : fitView(P, *rt.part, pre0, bw, bh);
        if (f0 != fv) {
          if (fitAt0.empty()) fitAt0 = fitAt;
          fitAt0[k] = f0 ? Place{f0, px0 + (bw - f0->w) / 2, py0 + (bh - f0->h) / 2} : Place{};
        }
      }
    }
    if (!fitAt0.empty())   // (the later fits as placed: only the moved filigree differs)
      for (size_t k = 0; k < rtiles.size(); k++)
        if (!(r.fpOn(ts) && rtiles[k].fin == FIN_TILE_FILIGREE)) fitAt0[k] = fitAt[k];
    if (anyStamp) {
      blob.assign((size_t)v.w * v.h, -1);
      for (int y = 0; y < v.h; y++)
        for (int x = 0; x < v.w; x++) {
          const size_t k = (size_t)y * v.w + x;
          const uint8_t g0 = v.regions[k];
          if (blob[k] >= 0 || v.cells[k] < 2 || (g0 != (uint8_t)Region::Wear && g0 != (uint8_t)Region::Gem)) continue;
          Blob b{x, y, x, y, 0, 0, g0};
          const int id = (int)blobs.size();
          stack.clear();
          stack.push_back((int)k);
          blob[k] = id;
          while (!stack.empty()) {
            const int q = stack.back();
            stack.pop_back();
            const int qx = q % v.w, qy = q / v.w;
            b.x0 = std::min(b.x0, qx); b.y0 = std::min(b.y0, qy); b.x1 = std::max(b.x1, qx); b.y1 = std::max(b.y1, qy); b.n++;
            const uint8_t qc = v.cells[(size_t)q];
            const Slot qs = codeSlot(qc);
            if (qs == Slot::Metal || qs == Slot::Metal2 || qs == Slot::Blade) b.metal++;
            const int nb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& d : nb) {
              const int nx = qx + d[0], ny = qy + d[1];
              if (nx < 0 || ny < 0 || nx >= v.w || ny >= v.h) continue;
              const size_t nk = (size_t)ny * v.w + nx;
              if (blob[nk] < 0 && v.cells[nk] >= 2 && v.regions[nk] == g0) { blob[nk] = id; stack.push_back((int)nk); }
            }
          }
          blobs.push_back(b);
        }
    }
  }
}

// (forge round 2, contracts 3c.1) the iridescent bands: a skirt's or motif's metal alternates Metal and Metal 2 per band
// between its N seams (every 3 drawn rows where none is marked)
void FramePaint::irisBands(const Item& it, const ViewRec& v, bool hasReg) {
  if (it.iris) {
    irisBand.assign(v.h, 0);
    bool seams = false;
    if (hasReg && !it.irisRows)
      for (size_t k = 0; k < v.regions.size() && !seams; k++) seams = v.regions[k] == (uint8_t)Region::Seam && v.cells[k] >= 2;
    uint8_t b = 0;
    int top = -1;
    // (forge fix i1) 3-row bands on a piece 9 rows tall or more (a two-tone plate at 1x), 2-row on a small one (a shoulder,
    // a helm band) so it still carries both hues
    int rowsDrawn = 0;
    for (int y = 0; y < v.h; y++)
      for (int x = 0; x < v.w; x++) if (v.cells[(size_t)y * v.w + x] >= 2) { rowsDrawn++; break; }
    const int period = rowsDrawn >= 9 ? 3 : 2;
    for (int y = 0; y < v.h; y++) {
      bool any = false, seamRow = false;
      for (int x = 0; x < v.w; x++) {
        const size_t k = (size_t)y * v.w + x;
        if (v.cells[k] >= 2) any = true;
        if (hasReg && v.regions[k] == (uint8_t)Region::Seam && v.cells[k] >= 2) seamRow = true;
      }
      if (any && top < 0) top = y;
      irisBand[(size_t)y] = b;
      if (seams ? seamRow : (top >= 0 && (y - top) % period == period - 1)) b ^= 1;
    }
    // (INTEGRATE forge fix i1) the bands' phase balances the piece's own metal: a jerkin whose metal is its collar and belt
    // plates (Thorniron's T4 torso) took the body hue on both and the second hue on 19 % of it, so the bands' boundaries
    // shift (0 to 2 periods) to the phase that leaves each hue nearest half the piece's metal (a two-tone piece, not a
    // one-hue piece with a stripe); the drawn phase stays unless another is clearly better (a tenth of the metal)
    if (!seams && top >= 0) {
      metalRow.assign(v.h, 0);
      int all = 0;
      for (int y = 0; y < v.h; y++)
        for (int x = 0; x < v.w; x++) {
          const uint8_t c = v.cells[(size_t)y * v.w + x];
          if (c >= 2 && codeSlot(c) == Slot::Metal) { metalRow[(size_t)y]++; all++; }
        }
      auto off = [&](int o) {
        int on = 0;
        for (int y = top; y < v.h; y++) on += (((y - top + o) / period) & 1) ? metalRow[(size_t)y] : 0;
        return std::abs(2 * on - all);
      };
      int best = 0, bestD = off(0);
      for (int o = 1; o < 2 * period; o++) { const int d = off(o); if (d < bestD) { bestD = d; best = o; } }
      if (all && best && off(0) - bestD > all / 10)
        for (int y = top; y < v.h; y++) irisBand[(size_t)y] = (uint8_t)(((y - top + best) / period) & 1);
    }
  }
}

// (P5, FIX_BATCH_P5 3.3) the piece's fingerprint: R and N by the recipe's bits; the cuff (gloves, boots: the top row of the
// worn regions, per column) and the hem (a cloak part with no R: its lowest lit pixel, per column) as a row per view column
void FramePaint::fingerprintOf(const Item& it, const ViewRec& v, ItemPaint& ip) {
  const DollRecipe& r = *rp;
  const int ts = ip.ts;
  ip.fp = ip.fpR = ip.fpN = ip.fpThin = ip.fpOuter = ip.fpStitch = ip.fpSparse = ip.fpHemRow = ip.fpEdge = ip.fpBoss = ip.fpLining = false;
  if (!r.fpOn(ts)) return;
  ip.fp = true;
  // (fix r1, F2: the T3 buckler, 61 colour-only pairs of 78) a shield view with no panel (P) is rim and boss: at T1-T4, while
  // the boss is no gem yet, the boss (G) takes the dye band, so the people's colours sit where a larger shield wears its mark
  if (ts == HumanLook::TIER_SHIELD && r.pieceTier[ts] <= 4 && !v.regions.empty()) {
    bool panel = false, gem = false;
    for (size_t k = 0; k < v.regions.size() && !panel; k++) {
      if (v.cells[k] < 2) continue;
      panel = v.regions[k] == (uint8_t)Region::Panel;
      gem = gem || v.regions[k] == (uint8_t)Region::Gem;
    }
    ip.fpBoss = gem && !panel;
  }
  // (fix r1, F5: a T3-T5 metal piece whose view draws its brass rim (R) but no seam (N, D) showed nothing of its maker on the
  // doll while its icon wore the dye) the lining: the piece's own pixels along the rim's inner edge take the dye band as N
  // would (thinned as N), the rim itself stays the ladder's brass (C12's trim)
  if ((ts == HumanLook::TIER_BODY || ts == HumanLook::TIER_HELM || ts == HumanLook::TIER_SHIELD) && r.pieceTier[ts] >= 3 && r.pieceTier[ts] <= 5 &&
      (r.fpN >> ts & 1) && !v.regions.empty()) {
    bool trim = false, seam = false;
    for (size_t k = 0; k < v.regions.size() && !seam; k++) {
      if (v.cells[k] < 2) continue;
      trim = trim || v.regions[k] == (uint8_t)Region::Trim;
      seam = v.regions[k] == (uint8_t)Region::Seam || v.regions[k] == (uint8_t)Region::DyeSeam;
    }
    ip.fpLining = trim && !seam;
  }
  ip.fpR = r.fpR >> ts & 1;
  ip.fpN = r.fpN >> ts & 1;
  ip.fpThin = r.fpThin >> ts & 1;
  ip.fpOuter = r.fpOuter >> ts & 1;
  ip.fpStitch = r.fpStitch >> ts & 1;
  ip.fpSparse = r.fpSparse >> ts & 1;
  const bool cuff = (r.fpCuff >> ts & 1) && !v.regions.empty();
  // (fix r1, fp-landing) the procedural hem on a cloak VIEW with no R (the halfcape's, the noble's and the travel cloak's base
  // views draw no border: before, only a part with no R in any view took the hem, so those views carried nothing)
  bool viewTrim = false;
  for (size_t k = 0; k < v.regions.size() && !viewTrim; k++) viewTrim = v.regions[k] == (uint8_t)Region::Trim && v.cells[k] >= 2;
  // (fix r1, F5) a body, helm or shield view with no landing region at all (no R, N, D or P: the T4 alloy plates, scales and
  // lamellae, an iron round shield's studded face) takes the same procedural hem: its lowest pixel per column
  bool bare = (ts == HumanLook::TIER_BODY || ts == HumanLook::TIER_HELM || ts == HumanLook::TIER_SHIELD) && r.pieceTier[ts] <= 5 && (r.fpN >> ts & 1);
  for (size_t k = 0; k < v.regions.size() && bare; k++)
    if (v.cells[k] >= 2) {
      const uint8_t g = v.regions[k];
      bare = !(g == (uint8_t)Region::Trim || g == (uint8_t)Region::Seam || g == (uint8_t)Region::DyeSeam || g == (uint8_t)Region::Panel);
    }
  const bool hem = !cuff && (((r.fpHem >> ts & 1) && partHasRoleCat(*it.part, Role::Cloak) && !viewTrim) || bare);
  if (!cuff && !hem) return;
  fpRow.assign(v.w, -1);
  for (int x = 0; x < v.w; x++)
    for (int y0 = 0; y0 < v.h; y0++) {
      const int y = hem ? v.h - 1 - y0 : y0;
      const size_t k = (size_t)y * v.w + x;
      const uint8_t c = v.cells[k];
      if (c < 2) continue;
      if (cuff) { if (r.cuffMask[ts] >> v.regions[k] & 1) { fpRow[(size_t)x] = (int16_t)y; break; } }
      else if (codeStep(c) >= 1) { fpRow[(size_t)x] = (int16_t)y; break; }
    }
  ip.fpEdge = true;
  ip.fpHemRow = hem;
}
// (P5) a pixel in the piece's dye band: the drawn step kept (1-3: the outline keeps its slot, no dye on a glint), the band
// along canvas x + y (run px of the dye, then the dye2 to the period); N thinned to every other canvas row where heavy
DOLL_PIXEL_INLINE uint8_t FramePaint::fpColour(const ItemPaint& ip, int x, int y, uint8_t c, uint8_t rg) const {
  const ViewRec& v0 = *ip.v;
  // (fix r1, F5: the T3 mail, scale and plate bodies draw their seams as interior lines) a seam pixel drawn as a line inside
  // the piece (step 0, every 4-neighbour opaque in the view: never the silhouette's ring) takes the dye at its shadow step
  auto interior = [&]() {
    for (const auto& d : {std::pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
      const int qx = x + d.first, qy = y + d.second;
      if (qx < 0 || qy < 0 || qx >= v0.w || qy >= v0.h || v0.cells[(size_t)qy * v0.w + qx] < 2) return false;
    }
    return true;
  };
  const bool seamLine = codeStep(c) == 0 && ip.fpN && (rg == (uint8_t)Region::Seam || fpSeam) && interior();
  const int st = seamLine ? 1 : codeStep(c);
  if (st < 1 || st > 3) return c;
  const Slot cs = codeSlot(c);
  if (cs == Slot::Glow || cs == Slot::Ink || cs == Slot::Skin || cs == Slot::Fx) return c;
  // (a metal plate's lit run stays metal in R and N: a dyed lit pixel would leave its neighbour a lone lit dash, C19 A8)
  const bool litMetal = st == 3 && (cs == Slot::Metal || cs == Slot::Metal2 || cs == Slot::Blade);
  const Item& it = *ip.it;
  const ViewRec& v = *ip.v;
  const int px = it.ax + x - v.ox, py = it.ay + y - v.oy;
  bool on = false, edge = false;
  // (fix r1) a T1-T2 piece's thread binding (R) takes its lit metal too: a bronze coat's collar is lit along its whole run, so
  // the binding showed only on its outline (the despeckle then cleared the rest); the glints (step 4) stay metal
  const bool thread = rg == (uint8_t)Region::Trim && ip.fpR && rp->pieceTier[ip.ts] <= 2;
  if (litMetal && !(ip.fpBoss && rg == (uint8_t)Region::Gem) && !thread) on = false;
  else if (rg == (uint8_t)Region::Trim) {
    on = ip.fpR;
    if (on && ip.fpOuter) {   // (the binding only: an R pixel on the piece's outer row)
      auto out = [&](int qx, int qy) { if (qx < 0 || qy < 0 || qx >= v.w || qy >= v.h) return true; const uint8_t q = v.cells[(size_t)qy * v.w + qx]; return q < 2 || codeStep(q) == 0; };
      on = out(x - 1, y) || out(x + 1, y) || out(x, y - 1) || out(x, y + 1);
    }
  }
  else if (rg == (uint8_t)Region::Seam || fpSeam) on = ip.fpN && !(ip.fpThin && (py >> 1 & 1));   // (fix r1) thinned in two-row blocks: a vertical lace keeps runs, never dots
  else if (rg == (uint8_t)Region::Gem && ip.fpBoss) on = true;   // (fix r1) the buckler's boss, lit plane and all (a glint stays)
  else if (!rg && ip.fpLining) {   // (fix r1) the lining: an unregioned pixel of the piece touching its drawn rim (R)
    auto isR = [&](int qx, int qy) {
      if (qx < 0 || qy < 0 || qx >= v.w || qy >= v.h) return false;
      const size_t q = (size_t)qy * v.w + qx;
      return v.cells[q] >= 2 && v.regions[q] == (uint8_t)Region::Trim;
    };
    on = (isR(x - 1, y) || isR(x + 1, y) || isR(x, y - 1) || isR(x, y + 1)) && !(ip.fpThin && (py >> 1 & 1));
  }
  // (the cuff and the hem dye a garment's own material only: leather, metal, cloth, the cloak; never a pixel a probe's region
  // marker recoded, the motion gates' Plume boot, Field knee, Device sole)
  if (!on && (!litMetal || !ip.fpHemRow) && !(ip.fpHemRow && rg == (uint8_t)Region::Trim) && ip.fpEdge && fpRow[(size_t)x] == y)   // (fix r1: a hem never takes the trim band's brass: C12)
    on = edge = cs == Slot::Leather || cs == Slot::Metal || cs == Slot::Metal2 || cs == Slot::Cloth1 || cs == Slot::Cloth2 ||
                cs == Slot::Cloth3 || cs == Slot::Linen || cs == Slot::Accent || cs == Slot::Cloak || cs == Slot::Pattern || cs == Slot::Head;
  if (!on) return c;
  const DollRecipe& r = *rp;
  const int ts = ip.ts, period = std::max<int>(1, r.dyePeriod[ts]);
  if (ip.fpStitch && (edge ? ip.fpHemRow : rg == (uint8_t)Region::Trim)) {   // (one period of the band, then one or two as drawn)
    const int p2 = std::max(2, period), cyc = (ip.fpSparse ? 3 : 2) * p2;
    int s2 = (px + py) % cyc;
    if (s2 < 0) s2 += cyc;
    if (s2 >= p2) return c;
  }
  int ph = (px + py) % period;
  if (ph < 0) ph += period;
  // (fix r1, the judge's R1 read: "an Imperial T3 lamellar skirt with a purple and gold checkerboard dither") from T3 the
  // lacing and seams (N, D, the lining, a seam line) take the dye alone: a lamellar's laces sit a plate apart, so the band's
  // two colours alternated lace by lace into a checker; the two-tone band stays on the rims, hems and cuffs
  const bool seam = !edge && rg != (uint8_t)Region::Trim && rg != (uint8_t)Region::Gem;
  const bool second = ph >= r.dyeRun[ts] && !(seam && r.pieceTier[ts] >= 3);
  return code(dyeSlotOf(ts, second), r.fpFlat ? (second ? 2 : std::min(st, 2)) : st);
}

void FramePaint::paintPixels(const ItemPaint& ip) {
  if (rp->fpSlots) paintPixelsT<true>(ip);
  else paintPixelsT<false>(ip);
}
template <bool Fp>
void FramePaint::paintPixelsT(const ItemPaint& ipIn) {
  const ItemPaint ip = ipIn;   // (a local copy: the per-pixel pieces read it from registers, not through the caller's frame)
  const Item& it = *ip.it;
  const ViewRec& v = *ip.v;
  const bool handOnly = ip.handOnly, stretch = ip.stretch, hasReg = ip.hasReg, bandOn = ip.bandOn;
  const int sway = ip.sway, swayFrom = ip.swayFrom;
  for (int y = 0; y < v.h; y++) {
    int ys[2] = {y, INT32_MIN};
    if (stretch) {
      if (v.stretch < v.oy) { if (y < v.stretch) ys[0] = y - 1; else if (y == v.stretch) { ys[0] = y - 1; ys[1] = y; } }
      else { if (y > v.stretch) ys[0] = y + 1; else if (y == v.stretch) ys[1] = y + 1; }
    }
    const int sx = y >= swayFrom ? sway : 0;
    const uint8_t* row = &v.cells[(size_t)y * v.w];
    for (int x = 0; x < v.w; x++) {
      uint8_t c = row[x];
      if (!c) continue;
      if (it.clip) {   // (P2c forge) the key fallback: only over the key view's own opaque pixels
        const int cx = x - v.ox + it.clip->ox, cy = y - v.oy + it.clip->oy;
        if (cx < 0 || cy < 0 || cx >= it.clip->w || cy >= it.clip->h || it.clip->cells[(size_t)cy * it.clip->w + cx] < 2) continue;
      }
      uint8_t rg = c >= 2 && (hasReg || bandOn) ? (bandOn ? bandReg[(size_t)y * v.w + x] : v.regions[(size_t)y * v.w + x]) : 0;
      if constexpr (Fp) fpSeam = rg == (uint8_t)Region::DyeSeam;   // (P5) a dye seam reads as no region to every rule but the fingerprint's
      if (rg == (uint8_t)Region::DyeSeam) rg = 0;
      if (handOnly && (c < 2 || rg != (uint8_t)Region::Hand)) continue;
      c = recolour<Fp>(ip, x, y, c, rg);
      stamp<Fp>(ip, x, c, rg, sx, ys);
    }
  }
}

// a pixel's code: the recipe's region slot, the tier's finish, the trim band's tile, the held weapon's metal, the
// iridescent bands, the culture alloy's metal and the region tiles
template <bool Fp>
DOLL_PIXEL_INLINE uint8_t FramePaint::recolour(const ItemPaint& ip, int x, int y, uint8_t c, uint8_t rg) {
  const Pool& P = *pool;
  const DollRecipe& r = *rp;
  const Item& it = *ip.it;
  const ViewRec& v = *ip.v;
  const int ts = ip.ts;
  const bool bandOn = ip.bandOn, toBlade = ip.toBlade, toMetal = ip.toMetal, brassBody = ip.brassBody;
  const ViewRec* bandView = ip.bandView;
  if constexpr (Fp) fpPre = 0;
  // (P5) a part's dye cells draw in its tier slot's pair (as a held part's metal in Blade); no tier slot: the Accent default
  if (c >= code(Slot::Dye, 0) && c < code(Slot::DyeT0, 0) && ts >= 0) c = code(dyeSlotOf(ts, codeSlot(c) == Slot::Dye2), codeStep(c));
  if (rg && rg < REGION_COUNT && r.region[rg] != 0xFF) c = code((Slot)r.region[rg], codeStep(c));
  // (P2c ladder) R P G N W by the piece's own tier (the outline ring keeps its slot)
  if (rg >= (uint8_t)Region::Trim && rg < REGION_COUNT && ts >= 0 && c >= 2 && codeStep(c) >= 1 &&
      !(bandOn && rg == (uint8_t)Region::Trim && bandRow[(size_t)y * v.w + x] == -2)) {   // (D93: a T3 run kept as drawn)
    const uint8_t fs = r.fin[ts][rg - (uint8_t)Region::Trim];
    if (fs != 0xFF) c = code((Slot)fs, codeStep(c));
  }
  // (D78) the band's cells: a row per depth from the outer edge (row 0 the outer row); a tile with twice the band's rows
  // gives the bottom- and right-facing edges their own rows (the lower half), so the top-left can be the lit one
  if (bandOn && bandView && c >= 2 && codeStep(c) >= 1 && rg == (uint8_t)Region::Trim) {
    const int k = y * v.w + x;
    const int row = bandRow[(size_t)k];
    if (row >= 0) {
      const ViewRec& tv = *bandView;
      const int W = r.pieceTier[partTierSlot(it.order)] >= 5 ? 2 : 1;   // (the tile's rows by the tier's width, whatever D93 narrowed)
      int ty = std::min(row, (int)tv.h - 1);
      if (row & kBandRim) ty = (int)tv.h - 1;   // (a plate rim: the shaded, drawn-step row)
      else if ((int)tv.h >= 2 * W && bandFacesDownRight(v, x, y)) ty = std::min(W + row, (int)tv.h - 1);
      int tx = ((it.ax + x - v.ox - it.tx) + (it.ay + y - v.oy - it.ty)) % (int)tv.w;
      if (tx < 0) tx += tv.w;
      const uint8_t tc = tv.cells[(size_t)ty * tv.w + tx];
      if (tc >= 2) {
        const Slot tsl = codeSlot(tc);
        const int tk = codeStep(tc), ck = codeStep(c);
        const int nk = tsl == Slot::Ink ? std::clamp(ck + tk - 2, 1, 4) : tsl == Slot::Glow ? tk : (tk == 2 ? ck : tk == 1 ? std::max(1, ck - 1) : tk == 3 ? std::min(4, ck + 1) : tk);
        c = code(tsl == Slot::Ink ? codeSlot(c) : tsl, nk);
      }
    }
  }
  if (toBlade && c >= 2 && codeSlot(c) == Slot::Metal) c = code(Slot::Blade, codeStep(c));
  if (ip.toLeather && c >= 2 && (codeSlot(c) == Slot::Metal || codeSlot(c) == Slot::Metal2)) c = code(Slot::Leather, codeStep(c));
  if (ip.helmLeather && c >= 2 && codeSlot(c) == Slot::Metal) c = code(Slot::Leather, codeStep(c));
  if (ip.toGilt && c >= 2 && codeSlot(c) == Slot::Metal2) c = code(Slot::Device, codeStep(c));   // (P5b fix r1) the legend's gilt
  if (ip.toGiltGrowth && c >= 2 && codeSlot(c) == Slot::Metal && codeStep(c) >= 1) c = code(Slot::Device, codeStep(c));
  // (INTEGRATE silhouettes i1) the glints (step 4) stay in the body's Metal: the sheen's spark reads on both bands
  if (it.iris && c >= 2 && codeSlot(c) == Slot::Metal && codeStep(c) >= (it.irisRows ? 0 : 1) && codeStep(c) <= (it.irisRows ? 4 : 3) && irisBand[(size_t)y]) c = code(Slot::Metal2, codeStep(c));   // (silhouettes i2: the body's bands take the plate's rim and glint too, so the second hue reads as a plate, C17 20 %)
  if ((toMetal || brassBody) && c >= 2) {
    const Slot qs = codeSlot(c);
    if (qs == Slot::Head || qs == Slot::Cloth1 || qs == Slot::Cloth2 || qs == Slot::Cloth3 || qs == Slot::Linen || (toMetal && qs == Slot::Leather) ||
        (brassBody && qs == Slot::Accent))
      c = code(Slot::Metal, codeStep(c));
  }
  // the region tiles (the finish tiles by the piece's tiles: the mark where the fingerprint lays it); (P5) where the mark
  // changed the pixel, the code under it is the pre-P5 one (its etch at T4), so the passes read the pre-P5 figure
  if constexpr (!Fp) return regionTiles(ip, x, y, c, rg, ts >= 0 ? r.finTiles[ts] : 0);   // (no fingerprint: tilesAt is finTiles)
  const uint8_t tiles = ts >= 0 ? r.tilesAt(ts) : 0;
  const uint8_t c1 = regionTiles(ip, x, y, c, rg, tiles);
  if (ip.fp && ts >= 0 && (tiles != r.finTiles[ts] || !fitAt0.empty())) {
    const uint8_t c0 = regionTiles(ip, x, y, c, rg, r.finTiles[ts], true);
    if (c0 != c1) fpPre = c0;
  }
  c = c1;
  // (P5) the maker's dye band on R, N, the cuff and the hem, last: the code under it (fpPre) is the finished pre-P5 pixel
  if (ip.fp && c >= 2) {
    const uint8_t c0 = c;
    c = fpColour(ip, x, y, c, rg);
    if (c != c0 && !fpPre) fpPre = c0;
  }
  return c;
}

// (P2c) the region tiles over a pixel (`tiles`: the piece's finish tiles in play); recolour's last tile step
DOLL_PIXEL_INLINE uint8_t FramePaint::regionTiles(const ItemPaint& ip, int x, int y, uint8_t c, uint8_t rg, uint8_t tiles, bool pre5) {
  const Pool& P = *pool;
  const DollRecipe& r = *rp;
  const Item& it = *ip.it;
  const ViewRec& v = *ip.v;
  const int ts = ip.ts;
  // (P2c) a region tile over this region's pixels (outlines kept): ink keeps the slot; step 2 keeps the step, 1 / 3 move
  // it one down / up, 0 and 4 set it
  // (ladder) tiles stack: the last-listed finish tile first (glyph over filigree); a clear cell falls through to the next
  if (rg && !rtiles.empty() && codeStep(c) >= 1)
    for (size_t ri = rtiles.size(); ri-- > 0;) {
      const RegionTile& rt = rtiles[ri];
      if (!(rt.mask >> rg & 1)) continue;
      if (rt.fin && !(ts >= 0 && (tiles & rt.fin))) continue;   // (ladder) a finish tile on its tier's pieces only
      if (rt.forgeSlot >= 0 && ts != rt.forgeSlot) continue;               // (P2c forge) fittings on their slot's pieces
      const ViewRec* tv = nullptr;
      int tx = 0, ty = 0;
      bool placed = false;
      const std::vector<Place>& fits = pre5 && !fitAt0.empty() ? fitAt0 : fitAt;   // (P5) the pre-P5 fits for the under plane
      if (rt.fit && fits[ri].v) {
        tv = fits[ri].v; tx = x - fits[ri].ox; ty = y - fits[ri].oy;
        placed = true;
        if (tx < 0 || ty < 0 || tx >= tv->w || ty >= tv->h) continue;
      } else if (rt.stamp && !blobs.empty() && blob[(size_t)y * v.w + x] >= 0) {
        const Blob& b = blobs[(size_t)blob[(size_t)y * v.w + x]];
        const int bw = b.x1 - b.x0 + 1, bh = b.y1 - b.y0 + 1;
        char key[24];
        const bool wearSet = keyView(P, *rt.part, "cloth2x2") != nullptr;
        if (bh == 1 && (tv = keyView(P, *rt.part, "hem")) != nullptr) { tx = (x - b.x0) % tv->w; ty = 0; }
        else {
          if (wearSet) std::snprintf(key, sizeof key, "%s%dx%d", b.metal * 2 > b.n ? "metal" : "cloth", std::min(bw, 3), std::min(bh, 3));
          else std::snprintf(key, sizeof key, "%dx%d", std::min(bw, 3), std::min(bh, 3));
          tv = keyView(P, *rt.part, key);
          tx = x - b.x0; ty = y - b.y0;
        }
        if (tv) {
          placed = true;
          if (tx < 0 || ty < 0 || tx >= tv->w || ty >= tv->h) continue;
        }
      }
      if (!placed) {   // the periodic `rest` tile (on the part's grid)
        if (!rt.v) continue;
        tv = rt.v;
        tx = (it.ax + x - v.ox - it.tx + tv->ox) % tv->w; ty = (it.ay + y - v.oy - it.ty + tv->oy) % tv->h;
        if (tx < 0) tx += tv->w;
        if (ty < 0) ty += tv->h;
      }
      const uint8_t tc = tv->cells[(size_t)ty * tv->w + tx];
      if (tc < 2) { if (rt.fin || rt.forge) continue; break; }
      const Slot tsl = codeSlot(tc);
      const int tk = codeStep(tc), ck = codeStep(c);
      // (P5) the mark's thread cells: the slot's dye on leather and cloth at T1-T2 (the drawn step), Ink relief one step down
      // on metal and at T3-T4 (the engraving)
      if (rt.fin == FIN_TILE_MARK && ck >= 4 && (codeSlot(c) == Slot::Metal || codeSlot(c) == Slot::Metal2 || codeSlot(c) == Slot::Blade))
        break;   // (fix r1, C17) a metal glint stays the metal's light: no mark cell cuts or dyes it (a lit cloth plane still takes it)
      if (rt.fin == FIN_TILE_MARK && (tsl == Slot::Dye || tsl == Slot::Dye2)) {
        const Slot cs = codeSlot(c);
        // (fix r1, F5: a T3-T4 shield's painted field and a cloth helm showed nothing of their maker) the cut is the metal's:
        // on a painted field, cloth or leather the mark stays thread (stitched, appliqued) at every tier it is drawn
        const bool cut = cs == Slot::Metal || cs == Slot::Metal2 || cs == Slot::Blade;
        c = cut ? code(cs, std::max(1, ck - 1)) : code(dyeSlotOf(ts, tsl == Slot::Dye2), r.fpFlat ? (tsl == Slot::Dye2 ? 2 : std::clamp(ck, 1, 2)) : std::clamp(ck, 1, 3));
        break;
      }
      int nk;
      if (tsl == Slot::Ink && (rt.fin || rt.forge)) nk = std::clamp(ck + tk - 2, rt.stamp ? 0 : 1, 4);   // (ladder) ink moves the step by k - 2
      else if (rt.fin && (rt.stamp || tsl == Slot::Glow)) nk = tk;                          // stamps and light write their step
      else nk = tk == 2 ? ck : tk == 1 ? std::max(1, ck - 1) : tk == 3 ? std::min(4, ck + 1) : tk;
      // (silhouettes i2, the blind panel: "T1 padded: red 1-2 px dots read as a rash or blood") the wear stamps darn and
      // fray in the garment's own colour (its step), never in a second hue
      const bool toneOnTone = rt.fin == FIN_TILE_WEAR && tsl != Slot::Glow;
      c = code(tsl == Slot::Ink || toneOnTone ? codeSlot(c) : tsl, toneOnTone ? std::clamp(nk, 1, 3) : nk);
      break;
    }
  return c;
}

// a pixel written at its build columns and stretch rows: the erase cells (the shield and held gear spared), the pattern
// tile, the body-piece, depth, held and shield planes, the probe's owner and region, the dirty box
template <bool Fp>
DOLL_PIXEL_INLINE void FramePaint::stamp(const ItemPaint& ip, int x, uint8_t c, uint8_t rg, int sx, const int ys[2]) {
  FrameOut& o = *op;
  std::vector<Item>& items = *itemsV;
  std::vector<uint8_t>& heldPx = *heldPxV;
  std::vector<uint8_t>& bodyPx = *bodyPxV;
  std::vector<uint8_t>& depthPx = *depthPxV;
  std::vector<int16_t>* const probeOwner = probeOwnerV;
  std::vector<uint8_t>* const probeRegion = probeRegionV;
  const Item& it = *ip.it;
  const ViewRec& v = *ip.v;
  const uint8_t heldMark = ip.heldMark, shieldMark = ip.shieldMark, bodyPiece = ip.bodyPiece, depthMark = ip.depthMark;
  const bool helmErase = ip.helmErase, tiled = ip.tiled;
  constexpr bool fpFrame = Fp;   // (P5) g_fpUnder is sized for this frame (rp->fpSlots)
  for (int yi = 0; yi < 2; yi++) {
    if (ys[yi] == INT32_MIN) continue;
    for (int xi = 0; xi < 2; xi++) {
      const int dx = xi ? dst1[x] : dst0[x];
      if (dx == INT32_MIN) continue;
      const int X = it.ax + dx - v.ox + sx, Y = it.ay + ys[yi] - v.oy;
      if (X < 0 || Y < 0 || X >= CW || Y >= CH) continue;
      if (c == kErase) {
        if (it.noErase) continue;
        if (heldPx[(size_t)Y * CW + X] && !heldMark && helmErase) continue;
        if (shieldPx[(size_t)Y * CW + X] && !shieldMark) continue;   // (D82) the gripL shield under another part's erase
        if (fixOn(FIX_SHIELD) && shieldKeep[(size_t)Y * CW + X] && !shieldMark) {   // (D82) the shield under an erased overpaint
          const size_t q = (size_t)Y * CW + X;
          o.code[q] = shieldKeep[q];
          if constexpr (fpFrame) { g_fpUnder[q] = 0; g_fpCuffOwn[q] = 0; }
          bodyPx[q] = 0;
          depthPx[q] = 1;
          heldPx[q] = 0;
          shieldPx[q] = 1;
          if (hhOn) hhPx[q] = 0;
          if (probeOwner) { (*probeOwner)[q] = shieldKeepOw[q]; (*probeRegion)[q] = shieldKeepRg[q]; }
          continue;
        }
        o.code[(size_t)Y * CW + X] = 0;
        if constexpr (fpFrame) { g_fpUnder[(size_t)Y * CW + X] = 0; g_fpCuffOwn[(size_t)Y * CW + X] = 0; }
        bodyPx[(size_t)Y * CW + X] = 0;
        depthPx[(size_t)Y * CW + X] = 0;
        if (hhOn) hhPx[(size_t)Y * CW + X] = 0;
        if (probeOwner) { (*probeOwner)[(size_t)Y * CW + X] = -1; (*probeRegion)[(size_t)Y * CW + X] = 0; }
        continue;
      }
      uint8_t w = c, u = Fp ? fpPre : 0;
      // the pattern tile: a cloth pixel (not its outline) takes the tile's slot where the tile marks it, keeping
      // the cloth's own step (so the garment's shading runs through the pattern); the tile's grid: Item::tx, ty
      // (P5: the code under a fingerprinted pixel the same, so the passes see the pre-P5 figure)
      for (int pass = 0; pass < (Fp && fpPre ? 2 : 1); pass++) {
        uint8_t& z = pass ? u : w;
        if (tiled && (uint8_t)codeSlot(z) == tileOn && codeStep(z) >= 1) {
          const ViewRec& t = *tile.v;
          int tx = (X - it.tx + t.ox) % t.w, ty = (Y - it.ty + t.oy) % t.h;
          if (tx < 0) tx += t.w;
          if (ty < 0) ty += t.h;
          const uint8_t tc = t.cells[(size_t)ty * t.w + tx];
          if (tc >= 2) z = code(codeSlot(tc), codeStep(z));
        }
      }
      o.code[(size_t)Y * CW + X] = w;
      if constexpr (fpFrame) {
        g_fpUnder[(size_t)Y * CW + X] = u;
        // (fix r1) the cuff's owner plane: a glove's or boot's worn-region pixel (cuffMask) as last painted (0: another part's)
        g_fpCuffOwn[(size_t)Y * CW + X] = ip.fpEdge && !ip.fpHemRow && ip.ts >= 0 && (rp->cuffMask[ip.ts] >> rg & 1) ? (uint8_t)(ip.ts + 1) : 0;
      }
      bodyPx[(size_t)Y * CW + X] = (uint8_t)(bodyPiece | (rg == (uint8_t)Region::Trim ? 8 : 0));   // (envelope fix round) bit 8: the drawn trim (R)
      depthPx[(size_t)Y * CW + X] = depthMark;
      heldPx[(size_t)Y * CW + X] = heldMark;
      shieldPx[(size_t)Y * CW + X] = shieldMark;
      if (hhOn) hhPx[(size_t)Y * CW + X] = ip.hh == 2 && codeSlot(w) == Slot::Plume ? 3 : ip.hh;
      if (shieldMark) {
        const size_t q = (size_t)Y * CW + X;
        shieldKeep[q] = w;
        shieldKeepRg[q] = rg;
        shieldKeepOw[q] = (int16_t)(&it - items.data());
      }
      if (probeOwner) { (*probeOwner)[(size_t)Y * CW + X] = (int16_t)(&it - items.data()); (*probeRegion)[(size_t)Y * CW + X] = rg; }
      if (X < o.bx0) o.bx0 = X;
      if (X > o.bx1) o.bx1 = X;
      if (Y < o.by0) o.by0 = Y;
      if (Y > o.by1) o.by1 = Y;
    }
  }
}

}  // namespace comp
}  // namespace doll
}  // namespace art
