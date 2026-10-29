// EMBERVALE art: the doll's compositor (DOLL). A frame is the recipe's parts placed at their anchors (the body set's
// anchors moved by the clip frame), painted back to front by the layer table, '-' erasing what is below, regions
// recoloured by the look, `when` overlays (or `replace` views) by the recipe's flags, build masks, the elf stretch row,
// derived sways, pattern tiles over the garment's cloth; then the finishing pass (A3's selout: rpg/art/doll/doll.h,
// tools/doll/from_a3.py's reference model), coloured by the recipe's ramps, trimmed to its pivot and packed into the
// look's atlas (P2a's SheetBuilder). A turned frame (lying, a fall's topple: STYLE_P2 13) is composed in the left view
// without the finishing pass, turned a quarter clockwise (head east) or transposed (head west: the turn mirrored), then
// finished again, its pivot under the middle of the body on its back's row.
// Speed (B10): every pass works inside the frame's dirty box, never the whole scratch canvas; the pool's string ids,
// role index and view index are built once on load.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <cstring>
#include "rpg/art/art_holders2.h"
#include "rpg/art/doll/doll_envelope.h"
#include "rpg/art/doll/doll_forge.h"
#include "rpg/art/doll/doll_internal.h"

namespace art {
namespace doll {

namespace {

using namespace pool;

constexpr int CW = 160, CH = 136, CPX = 80, CPY = 104;   // the scratch canvas and its pivot

struct Cut {
  Canvas c; int px = 0, py = 0; RigAnchors an;
  Canvas g; int gx = 0, gy = 0, gn = 0;   // (P2c ladder) the glow plane's crop: its offset in c and its pixel count
};

// ---------------------------------------------------------------- the atlas packer (P2a's SheetBuilder, unchanged)
uint64_t cutHash(const Cut& c) {
  uint64_t h = 1469598103934665603ull ^ ((uint64_t)(uint16_t)c.c.w << 48 | (uint64_t)(uint16_t)c.c.h << 32 | (uint64_t)(uint16_t)c.px << 16 | (uint16_t)c.py);
  for (size_t i = 0; i < c.c.px.size(); i += 3) { h ^= c.c.px[i]; h *= 1099511628211ull; }
  return h;
}
// member by member, never memcmp: RigAnchors has a padding byte after weaponBehind whose value is whatever the stack
// held (MSVC's happened to match, clang's did not), so equal frames failed to share a cut and the atlas grew
bool sameAnchors(const RigAnchors& a, const RigAnchors& b) {
  return a.headX == b.headX && a.headY == b.headY && a.headW == b.headW && a.headH == b.headH && a.faceX == b.faceX &&
         a.faceY == b.faceY && a.handRX == b.handRX && a.handRY == b.handRY && a.handLX == b.handLX && a.handLY == b.handLY &&
         a.hipX == b.hipX && a.hipY == b.hipY && a.footLX == b.footLX && a.footRX == b.footRX && a.gripX == b.gripX &&
         a.gripY == b.gripY && a.tipX == b.tipX && a.tipY == b.tipY && a.weaponBehind == b.weaponBehind && a.topY == b.topY;
}
static_assert(sizeof(RigAnchors) == 20 * sizeof(int16_t), "RigAnchors gained a field: compare it in sameAnchors");
struct AtlasBuilder {
  struct Item { int facing; Clip clip; int variant; int src; };
  std::vector<Cut> cuts;
  std::vector<Item> items;
  std::vector<uint64_t> hashes;
  int add(int facing, Clip c, int variant, Cut&& cut) {
    const uint64_t h = cutHash(cut);
    for (size_t i = 0; i < cuts.size(); i++) {
      if (hashes[i] != h) continue;
      const Cut& o = cuts[i];
      if (o.c.w == cut.c.w && o.c.h == cut.c.h && o.px == cut.px && o.py == cut.py && o.c.px == cut.c.px && sameAnchors(o.an, cut.an)) {
        items.push_back({facing, c, variant, (int)i});
        return (int)i;
      }
    }
    cuts.push_back(std::move(cut));
    hashes.push_back(h);
    items.push_back({facing, c, variant, (int)cuts.size() - 1});
    return (int)cuts.size() - 1;
  }
  void addShared(int facing, Clip c, int variant, int src) { items.push_back({facing, c, variant, src}); }
  RigSheet finish() {
    RigSheet s;
    if (cuts.empty()) return s;
    // shelves, the largest cross size first (P2b fix r1, B10-ARMED-RUN-ATLAS: the atlas's waste is what a shelf leaves
    // beside its shorter cuts and at its end): rows (tallest first) or columns (widest first: the walk and run frames come
    // in a few widths and many heights, so columns waste less), filled next fit (the shelf being filled) or first fit (any
    // shelf with room left), at every shelf length; the smallest atlas wins (the first found on a tie: deterministic)
    const size_t n = cuts.size();
    int area = 0;
    for (const Cut& c : cuts) area += c.c.w * c.c.h;
    const int root = (int)std::sqrt((double)area);
    std::vector<int> order(n), bx(n), by(n), ol(n), oc(n), shelfPos, shelfLen;
    auto pack = [&](int length, bool firstFit, bool cols, bool write) {
      shelfPos.clear(); shelfLen.clear();
      int top = 0;
      for (size_t k = 0; k < n; k++) {
        int s = -1;
        if (firstFit) { for (size_t i = 0; i < shelfLen.size() && s < 0; i++) if (shelfLen[i] + ol[k] <= length) s = (int)i; }
        else if (!shelfLen.empty() && shelfLen.back() + ol[k] <= length) s = (int)shelfLen.size() - 1;
        if (s < 0) { shelfPos.push_back(top); shelfLen.push_back(0); top += oc[k]; s = (int)shelfLen.size() - 1; }
        if (write) {
          const size_t i = (size_t)order[k];
          (cols ? by : bx)[i] = shelfLen[(size_t)s];
          (cols ? bx : by)[i] = shelfPos[(size_t)s];
        }
        shelfLen[(size_t)s] += ol[k];
      }
      return top;
    };
    auto setup = [&](bool cols) {
      for (size_t i = 0; i < n; i++) order[i] = (int)i;
      auto cross = [&](int i) { return cols ? cuts[(size_t)i].c.w : cuts[(size_t)i].c.h; };
      std::sort(order.begin(), order.end(), [&](int a, int b) { return cross(a) != cross(b) ? cross(a) > cross(b) : a < b; });
      int maxL = 0;
      for (size_t k = 0; k < n; k++) {
        const Canvas& c = cuts[(size_t)order[k]].c;
        ol[k] = cols ? c.h : c.w; oc[k] = cols ? c.w : c.h;
        maxL = std::max(maxL, ol[k]);
      }
      return maxL;
    };
    long long bestA = -1;
    int bestL = 0, bestS = 0;
    bool bestFF = false, bestCols = false;
    for (bool cols : {false, true}) {
      const int maxL = setup(cols);
      for (int L = maxL; L <= std::max(maxL, root * 5 / 2); L++)
        for (bool ff : {false, true}) {
          const int S = pack(L, ff, cols, false);
          if (bestA < 0 || (long long)L * S < bestA) { bestA = (long long)L * S; bestL = L; bestS = S; bestFF = ff; bestCols = cols; }
        }
    }
    setup(bestCols);
    pack(bestL, bestFF, bestCols, true);
    int bestW = bestCols ? bestS : bestL, bestH = bestCols ? bestL : bestS;
    // (ladder i2, B10) a skyline packer beside the shelves: each cut (tallest first, or widest first) at the lowest place
    // the skyline leaves for it (then the leftmost), at every width; it fills the gaps a shelf leaves beside its shorter
    // cuts, which the lying and falling frames (wide and short beside tall) waste most. The smaller atlas wins (shelves on
    // a tie: deterministic either way)
    {
      struct Seg { int x, y, w; };
      std::vector<Seg> sky;
      std::vector<int> ord(n), sx(n), sy(n);
      auto skyline = [&](int W, bool write) {
        sky.assign(1, Seg{0, 0, W});
        int top = 0;
        for (size_t k = 0; k < n; k++) {
          const Canvas& c = cuts[(size_t)ord[k]].c;
          int bi = -1, bY = 0, bX = 0;
          for (size_t i = 0; i < sky.size(); i++) {
            const int x = sky[i].x;
            if (x + c.w > W) break;
            int y = 0, rem = c.w;
            for (size_t j = i; rem > 0; j++) { y = std::max(y, sky[j].y); rem -= sky[j].w; }
            if (bi < 0 || y + c.h < bY + c.h || (y == bY && x < bX)) { bi = (int)i; bY = y; bX = x; }
          }
          if (bi < 0) return -1;
          if (write) { sx[(size_t)ord[k]] = bX; sy[(size_t)ord[k]] = bY; }
          top = std::max(top, bY + c.h);
          // the new segment replaces what it covers
          std::vector<Seg> nw;
          nw.reserve(sky.size() + 2);
          for (size_t i = 0; i < (size_t)bi; i++) nw.push_back(sky[i]);
          nw.push_back(Seg{bX, bY + c.h, c.w});
          const int end = bX + c.w;
          for (size_t i = (size_t)bi; i < sky.size(); i++) {
            const int se = sky[i].x + sky[i].w;
            if (se <= end) continue;
            if (sky[i].x < end) nw.push_back(Seg{end, sky[i].y, se - end});
            else nw.push_back(sky[i]);
          }
          // merge equal neighbours
          sky.clear();
          for (const Seg& g : nw) {
            if (!sky.empty() && sky.back().y == g.y) sky.back().w += g.w;
            else sky.push_back(g);
          }
        }
        return top;
      };
      int maxW = 0;
      for (const Cut& c : cuts) maxW = std::max(maxW, c.c.w);
      long long skA = -1;
      int skW = 0, skO = 0;
      // (the bake's budget) only where the shelves waste more than 8 %; widths in steps of 3, then the best one's neighbours
      const bool worth = n >= 4 && (double)area < 0.92 * (double)bestA;
      for (int o = 0; worth && o < 2; o++) {
        for (size_t i = 0; i < n; i++) ord[i] = (int)i;
        std::sort(ord.begin(), ord.end(), [&](int a, int b) {
          const Canvas &A = cuts[(size_t)a].c, &B = cuts[(size_t)b].c;
          if (o == 0) return A.h != B.h ? A.h > B.h : A.w != B.w ? A.w > B.w : a < b;
          return A.w != B.w ? A.w > B.w : A.h != B.h ? A.h > B.h : a < b;
        });
        const int hiW = std::max(maxW, root * 5 / 2);
        long long oA = -1;
        int oW = maxW;
        auto tryW = [&](int W) {
          if (W < maxW || W > hiW) return;
          const int H = skyline(W, false);
          if (H > 0 && (oA < 0 || (long long)W * H < oA)) { oA = (long long)W * H; oW = W; }
        };
        for (int W = maxW; W <= hiW; W += 3) tryW(W);
        const int c = oW;
        for (int W = c - 2; W <= c + 2; W++) if (W != c) tryW(W);
        if (oA > 0 && (skA < 0 || oA < skA)) { skA = oA; skW = oW; skO = o; }
      }
      if (skA > 0 && skA < bestA) {
        for (size_t i = 0; i < n; i++) ord[i] = (int)i;
        std::sort(ord.begin(), ord.end(), [&](int a, int b) {
          const Canvas &A = cuts[(size_t)a].c, &B = cuts[(size_t)b].c;
          if (skO == 0) return A.h != B.h ? A.h > B.h : A.w != B.w ? A.w > B.w : a < b;
          return A.w != B.w ? A.w > B.w : A.h != B.h ? A.h > B.h : a < b;
        });
        bestH = skyline(skW, true);
        bestW = skW;
        for (size_t i = 0; i < n; i++) { bx[i] = sx[i]; by[i] = sy[i]; }
      }
    }
    s.atlas = Canvas(bestW, bestH);
    for (size_t i = 0; i < cuts.size(); i++) {
      const Canvas& c = cuts[i].c;
      for (int y = 0; y < c.h; y++) std::memcpy(&s.atlas.px[(size_t)(by[i] + y) * bestW + bx[i]], &c.px[(size_t)y * c.w], (size_t)c.w * 4);
    }
    // (P2c ladder, contracts 6b) the glow plane: the cuts' glow crops on shelves of their own
    // (ladder i2, C13) each crop cut into 8x8 cells trimmed to their pixels; equal tiles packed once (shelves, tallest first)
    {
      struct Tile { Canvas c; int ox = 0, oy = 0; };          // a placed tile: its pixels and its offset in the frame
      struct Uniq { Canvas c; uint64_t h = 0; int x = 0, y = 0; };
      std::vector<Uniq> uniq;
      std::vector<std::vector<std::pair<int, Tile>>> perCut(n);   // per cut: (unique index, placement)
      bool any = false;
      for (size_t i = 0; i < n; i++) {
        const Cut& c = cuts[i];
        if (!c.gn) continue;
        any = true;
        for (int cy = 0; cy < c.g.h; cy += 8)
          for (int cx = 0; cx < c.g.w; cx += 8) {
            int x0 = 99, y0 = 99, x1 = -1, y1 = -1;
            for (int y = cy; y < std::min(cy + 8, c.g.h); y++)
              for (int x = cx; x < std::min(cx + 8, c.g.w); x++)
                if (c.g.px[(size_t)y * c.g.w + x] >> 24) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
            if (x1 < 0) continue;
            Tile t;
            t.c = Canvas(x1 - x0 + 1, y1 - y0 + 1);
            for (int y = y0; y <= y1; y++)
              for (int x = x0; x <= x1; x++) t.c.px[(size_t)(y - y0) * t.c.w + (x - x0)] = c.g.px[(size_t)y * c.g.w + x];
            t.ox = c.gx + x0; t.oy = c.gy + y0;
            uint64_t h = 1469598103934665603ull ^ ((uint64_t)t.c.w << 8 | (uint64_t)t.c.h);
            for (uint32_t p : t.c.px) { h ^= p; h *= 1099511628211ull; }
            int u = -1;
            for (size_t k = 0; k < uniq.size(); k++)
              if (uniq[k].h == h && uniq[k].c.w == t.c.w && uniq[k].c.h == t.c.h && uniq[k].c.px == t.c.px) { u = (int)k; break; }
            if (u < 0) { u = (int)uniq.size(); Uniq q; q.c = t.c; q.h = h; uniq.push_back(std::move(q)); }
            perCut[i].push_back({u, std::move(t)});
          }
      }
      if (any) {
        int area = 0, wMax = 0;
        for (const Uniq& q : uniq) { area += q.c.w * q.c.h; wMax = std::max(wMax, q.c.w); }
        const int L = std::max(wMax, (int)std::ceil(std::sqrt((double)area) * 1.25));
        std::vector<int> ord(uniq.size());
        for (size_t k = 0; k < ord.size(); k++) ord[k] = (int)k;
        std::stable_sort(ord.begin(), ord.end(), [&](int a, int b) { return uniq[(size_t)a].c.h > uniq[(size_t)b].c.h; });
        int x = 0, y = 0, rowH = 0;
        for (int k : ord) {
          Uniq& q = uniq[(size_t)k];
          if (x + q.c.w > L) { x = 0; y += rowH; rowH = 0; }
          q.x = x; q.y = y;
          x += q.c.w; rowH = std::max(rowH, q.c.h);
        }
        s.glow.atlas = Canvas(L, y + rowH);
        for (const Uniq& q : uniq)
          for (int yy = 0; yy < q.c.h; yy++) std::memcpy(&s.glow.atlas.px[(size_t)(q.y + yy) * L + q.x], &q.c.px[(size_t)yy * q.c.w], (size_t)q.c.w * 4);
        s.glow.rects.reserve(items.size());
        s.glow.count.reserve(items.size());
        s.glow.first.reserve(items.size());
        s.glow.tileCount.reserve(items.size());
        for (const Item& it : items) {
          const Cut& c = cuts[(size_t)it.src];
          FrameRect r;
          if (c.gn) { r.w = (int16_t)c.g.w; r.h = (int16_t)c.g.h; r.px = (int16_t)c.gx; r.py = (int16_t)c.gy; }
          s.glow.rects.push_back(r);
          s.glow.count.push_back((uint16_t)c.gn);
          s.glow.first.push_back((uint32_t)s.glow.tiles.size());
          for (const auto& pt : perCut[(size_t)it.src]) {
            const Uniq& q = uniq[(size_t)pt.first];
            FrameRect t;
            t.x = (int16_t)q.x; t.y = (int16_t)q.y; t.w = (int16_t)q.c.w; t.h = (int16_t)q.c.h;
            t.px = (int16_t)pt.second.ox; t.py = (int16_t)pt.second.oy;
            s.glow.tiles.push_back(t);
          }
          s.glow.tileCount.push_back((uint16_t)perCut[(size_t)it.src].size());
        }
      }
    }
    s.frames.reserve(items.size());
    s.anchors.reserve(items.size());
    for (const Item& it : items) {
      const Cut& c = cuts[(size_t)it.src];
      FrameRect r;
      r.x = (int16_t)bx[(size_t)it.src]; r.y = (int16_t)by[(size_t)it.src];
      r.w = (int16_t)c.c.w; r.h = (int16_t)c.c.h;
      r.px = (int16_t)c.px; r.py = (int16_t)c.py;
      s.add(it.facing, it.clip, it.variant, r, c.an);
    }
    return s;
  }
};

// ---------------------------------------------------------------- one frame
struct Pt { int x = 0, y = 0; bool on = false; };
struct Item {
  const PartRec* part = nullptr;
  const ViewRec* view = nullptr;
  int layer = 0, z = 0, order = 0, sub = 0;
  Anchor anchor = Anchor::Pivot;
  bool garment = false;   // a garment layer item (torso / hangs) whose cloth takes the pattern tile
  int ax = 0, ay = 0;     // the anchor's canvas position
  int tx = 0, ty = 0;     // the pattern tile's origin: the neck's rest point moved by this item's own anchor
  const ViewRec* clip = nullptr;   // (P2c forge) the key fallback: a rest overlay clipped to this key view's opaque pixels
  bool clipTorso = false;          // (forge round 2) a motif's rest view serving a torso key: clipped to the torso armour's view
  bool iris = false;               // (forge round 2) a skirt or motif on an iridescent alloy: its metal in alternating bands
  bool irisRows = false;           // (INTEGRATE silhouettes i1) the body armour's bands: every 2 drawn rows (its seams ignored)
  bool noErase = false;            // (forge round 2, D41) a pauldron lifted over the cloak's front: its erase cells cut nothing
};
// (forge round 2) a part serving `role overlay <prefix>...` (skirt., motif., sleeve.)
bool overlayPrefix(const Pool& P, const PartRec& p, const char* prefix) {
  for (const RoleRec& rr : p.roles)
    if (rr.cat == (uint8_t)Role::Overlay && P.s(rr.value).rfind(prefix, 0) == 0) return true;
  return false;
}
struct Tile { const ViewRec* v = nullptr; Slot on = Slot::Cloth1; };
// (P2c) a region tile (`tags tile` + `on-region <R>`): repeated over those regions' pixels on every part that marks them
// (ladder) fin: a finish tile's FIN_TILE_* bit; fit / stamp: LADDER-FINISH's placements (`tags fit`: the view <prefix><w>x<h>
// that fits the part's P box best, centred on it; `tags stamp`: a view laid once per connected blob of the region, keyed by
// the blob's size); part: the tile's part (its keyed views)
// (P2c forge) forgeSlot: a forge tile's tier slot (its finish-free tiles draw only on that slot's parts; -1 every part);
// forgeMask: a limb tile's regions narrowed to the slots that picked its value
struct RegionTile { const ViewRec* v = nullptr; uint32_t mask = 0; uint8_t fin = 0; bool fit = false, stamp = false; const PartRec* part = nullptr;
                    int forgeSlot = -1; bool forge = false; };
// (wave-2 fix round, D78) the trim band of a view: bandReg = its regions with the widened pixels set to R; bandRow = each R pixel's
// row in the band counted from the outer edge (-1: not trim). Depth = the 4-connected distance from the part's outline (a
// pixel beside a clear cell or the view's edge is 0). A run is widened from its innermost R pixels toward deeper pixels until
// it is W thick; never into G or N, an outline pixel (step 0) or a clear cell
void trimBand(const ViewRec& v, int W, std::vector<uint8_t>& bandReg, std::vector<int8_t>& bandRow) {
  const int n = v.w * v.h;
  bandReg.assign(v.regions.begin(), v.regions.end());
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
  auto isR = [&](int k) { return solid(k) && v.regions[(size_t)k] == (uint8_t)Region::Trim && codeStep(v.cells[(size_t)k]) >= 1; };
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
    const uint8_t g = v.regions[(size_t)k];
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
}
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
// (P2c forge, contracts 3c) a cut tile (`on-slot <slot>`) or an edge stamp (`on-edge hem|rim`) on the armour and pauldron
// parts: their views transformed before the paint (forgeView)
struct ForgeTile { const PartRec* part = nullptr; bool edge = false, rim = false; Slot on = Slot::Metal; };

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

struct FrameOut {
  std::vector<uint8_t> code;
  int bx0 = CW, by0 = CH, bx1 = -1, by1 = -1;   // the dirty box (every pixel written this frame lies inside)
  Pt marks[16];
  int markIds[16];
  int nMarks = 0;
  Pt anchors[ANCHOR_COUNT];
  bool behind = false;
  void clear() {
    if (code.size() != (size_t)CW * CH) code.assign((size_t)CW * CH, 0);
    else if (bx1 >= bx0)
      for (int y = by0; y <= by1; y++) std::memset(&code[(size_t)y * CW + bx0], 0, (size_t)(bx1 - bx0 + 1));
    bx0 = CW; by0 = CH; bx1 = -1; by1 = -1;
  }
};

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
std::string fitPrefix(const Pool& P, const PartRec& p, uint8_t fin, const std::string& motif) {
  std::string first;
  for (uint32_t i = p.firstView; i < p.firstView + p.nViews; i++) {
    const std::string& k = P.s(P.views[i].key);
    size_t e = k.size();
    while (e > 0 && (std::isdigit((unsigned char)k[e - 1]) || k[e - 1] == 'x')) e--;
    if (e == k.size() || e == 0) continue;
    const std::string pre = k.substr(0, e);
    if (fin == FIN_TILE_FILIGREE && pre == motif) return pre;
    if (first.empty() || (fin == FIN_TILE_FILIGREE && pre == "oak")) first = pre;
  }
  return first;
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
bool partHasRoleCat(const PartRec& p, Role r) {
  for (const RoleRec& rr : p.roles) if (rr.cat == (uint8_t)r) return true;
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

thread_local std::vector<Item> g_items;   // the last composed frame's items (frameInfo reads them)
// (P2c) the probe's per-pixel planes (probeFrame sets them for one compose; null otherwise)
thread_local std::vector<int16_t>* g_probeOwner = nullptr;
// (P2c forge i2) the pixels the body's armour and pauldrons wrote last (the glint pass reads them)
thread_local std::vector<uint8_t> g_bodyPiece;
thread_local std::vector<uint8_t> g_depthPx;   // (envelope r2) 1 where a held, back or fx item drew the pixel (C19's off-figure)
thread_local std::vector<uint8_t>* g_probeRegion = nullptr;
thread_local int g_probeGrip = -1, g_probeGripL = -1;
// (P2c SLICE-DOLL i2) a turned frame's screen-space items: a view placed at the pivot (`anchor pivot`: the corpse's spear
// and bow `drop`) is drawn after the turn, level on the ground beside the lying body, not turned with it. 0: every item;
// 1: the body only (the turned pass); 2: the pivot items only (drawn into the turned figure)
thread_local int g_screenMode = 0;

// composes one frame into o (raw: no finishing pass). `facing` is the frame's own (a turned frame passes the left view)
// (ladder i3) the stir: while an idle's breathe frame (frame 1) composes for a look flagged `breeze` (a T5-T6 cloak or a
// plumed helm: doll_resolve.cpp), every part drawing its rest key that has a `breeze` view draws that instead (the cloak's
// hem out a pixel, the crest's tail lifted). The panel saw nothing move on the top tiers; the art was drawn, no clip named it.
thread_local bool g_breeze = false;
// (forge round 2, D41) the falls and the lying figures keep their pauldrons on the arm layer (a fall's arms fly past the
// cloak, and a lifted shoulder closed 1-2 px pockets against the helm: C3a)
thread_local bool g_deathPart = false;
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
void composeFrame(const DollRecipe& r, int facing, const FrameRec& f, FrameOut& o, const std::vector<int>* onlyParts = nullptr,
                  const char* forceKey = nullptr, bool (*keep)(const PartRec&) = nullptr) {
  const Pool& P = get();
  const Ids& I = P.ids;
  o.clear();
  o.nMarks = 0;
  o.behind = f.front == 0;
  Pt base[ANCHOR_COUNT];
  baseAnchors(r, facing, base);
  for (int a = 0; a < ANCHOR_COUNT; a++) {
    Pt p = base[a];
    if (r.elf && a != (int)Anchor::Pivot) p.y -= 1;   // the stretch row: +1 px in the legs
    moveAnchor((Anchor)a, f, p);
    o.anchors[a] = p;
  }
  std::vector<Item>& items = g_items;
  items.clear();
  Tile tile;
  static thread_local std::vector<RegionTile> rtiles;
  rtiles.clear();
  static thread_local std::vector<ForgeTile> ftiles;
  ftiles.clear();
  const PartRec* bandTile = nullptr;   // (wave-2 fix round, D78) the trim tile: `tags tile band` + `on-region R`
  const uint16_t forced = forceKey ? P.strId(forceKey) : (uint16_t)0xFFFF;
  const std::vector<int>& list = onlyParts ? *onlyParts : r.parts;
  for (size_t li = 0; li < list.size(); li++) {
    const int pi = list[li];
    if (pi < 0 || pi >= (int)P.parts.size()) continue;
    const PartRec& p = P.parts[(size_t)pi];
    if (keep && !keep(p)) continue;
    if (partTaggedId(p, I.tile)) {   // a pattern tile: laid over the garments' cloth, never placed itself
      if (partTagged(pi, "band")) { bandTile = &p; continue; }   // (D78) the band mode: widened R runs, laid per piece below
      const ViewRec* tv = viewOf(p, facing, I.rest);
      if (!tv) tv = viewOf(p, 0, I.rest);
      {
        ForgeTile ft;   // (P2c forge) a cut tile or an edge stamp: laid into the armour and pauldron views (forgeView)
        if (forgeTileOf(P, p, ft)) { ftiles.push_back(ft); continue; }
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
        if (rt.forge && !rt.mask) continue;
        if (tv || rt.fit || rt.stamp) rtiles.push_back(rt);
        continue;
      }
      if (tv && !tile.v) { tile.v = tv; tile.on = tileTarget(P, p); }
      continue;
    }
    uint16_t key = I.rest;
    const bool heldPart = I.held != 0xFFFF && p.channel == I.held;
    if (forceKey) key = forced;
    else if (p.channel != I.rest) {
      bool named = false;
      for (const KeyRec& k : f.keys) if (k.channel == p.channel) { key = k.key; named = true; }
      if (key == I.dash) continue;
      // (P2c) `weapon=<deg>,front|back`: the held part's `a<deg>` view (rounded to 22.5); a named `held=<key>` wins
      if (heldPart && !named && f.weapon != -32768) key = I.angle[angleIndex(f.weapon)];
    }
    if (key == 0xFFFF) continue;
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
    if (!v) continue;
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
    if (g_screenMode && (screenItem != (g_screenMode == 2))) continue;
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
  // anchors first (grip-anchored items read the marks of the parts placed before them in every layer)
  int gripFrom = -1, gripLFrom = -1;   // (P2c) the items whose views marked grip and gripL (the gripping arms)
  int handRFrom = -1, handLFrom = -1;  // (P2c i3) the items that marked handR / handL (the grip's fallback: a carry at rest)
  auto place = [&](Item& it) {
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
  };
  for (Item& it : items) if (it.anchor != Anchor::Grip && it.anchor != Anchor::GripL && it.anchor != Anchor::Crown) place(it);
  auto markPt = [&](uint16_t id) { for (int i = 0; i < o.nMarks; i++) if (o.markIds[i] == id && o.marks[i].on) return o.marks[i]; return Pt{}; };
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
  // (P2c ladder i2) a T6 piece in hand carries the edge spark from its grip to its tip (contracts 6b): where the held view
  // marks no tip (the loco carries, the drawn idle), the tip is the view's cell farthest from its grip origin, and the grip
  // the grip anchor; only T6 looks (every other frame's anchors as before)
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
  // paint back to front
  static thread_local std::vector<int> dst0, dst1;
  const uint8_t tileOn = (uint8_t)tile.on;
  const bool twoHand = r.has("twohand");
  // (ladder i2) the pixels a held weapon wrote: a helm's erase cells (the 1-2 rows over its dome, there to clear hair) clear
  // hair, head and face but never the haft or head of a polearm passing behind the helm
  static thread_local std::vector<uint8_t> heldPx;
  heldPx.assign((size_t)CW * CH, 0);
  static thread_local std::vector<uint8_t> shieldPx;   // (wave-2 fix round, D82) the gripL shield's pixels
  shieldPx.assign((size_t)CW * CH, 0);
  // (wave-2 fix round 3, D82; SHIELDS' r2 proposal) the gripL shield's last pixel under each cell (code, region, item): a part
  // that paints over the shield and is then erased by a later part's `-` cell shows the shield again, never a hole through it
  static thread_local std::vector<uint8_t> shieldKeep, shieldKeepRg;
  static thread_local std::vector<int16_t> shieldKeepOw;
  shieldKeep.assign((size_t)CW * CH, 0);
  shieldKeepRg.assign((size_t)CW * CH, 0);
  shieldKeepOw.assign((size_t)CW * CH, -1);
  g_depthPx.assign((size_t)CW * CH, 0);
  // (ladder i2) heldPx marks the pixels a helm's erase may NOT clear: a helm's box clears hair, head, face, beard and ears
  // (LADDER-ARMS's ask); the haft and head of a polearm, a scarf's tail, a collar passing behind the helm stay
  auto isHeld = [&](const PartRec& p) {
    for (const RoleRec& rr : p.roles) {
      const Role r = (Role)rr.cat;
      if (r == Role::Head || r == Role::Nose || r == Role::Brow || r == Role::Ears || r == Role::Marks || r == Role::Expr ||
          r == Role::Paint || r == Role::Earring || r == Role::Beard || r == Role::Hair || r == Role::Headwear || r == Role::Helm ||
          r == Role::Pauldron)
        return false;
    }
    return true;
  };
  // handOnly: (P2c) the gripping arm's H pixels drawn again over the weapon it holds
  // (P2c forge) the hem an edge stamp follows: the armour hang's when the frame draws one, else the torso's
  bool hangArmour = false;
  if (!ftiles.empty())
    for (const Item& it : items)
      if (!it.sub && (it.layer == (int)Layer::HangBack || it.layer == (int)Layer::HangFront) && partHasRoleCat(*it.part, Role::Armour)) hangArmour = true;
  if (g_bodyPiece.size() != (size_t)CW * CH) g_bodyPiece.assign((size_t)CW * CH, 0);
  else std::fill(g_bodyPiece.begin(), g_bodyPiece.end(), (uint8_t)0);
  auto paint = [&](const Item& it, bool handOnly) {
    const ViewRec* vp = it.view;
    // (forge i3) a culture alloy's metal head (DollRecipe::guiseHead) is its armour too: the glints and the sheen's tell may sit on it
    // (INTEGRATE silhouettes i1) bit 2: any gear piece (armour, helm, pauldron, overlay) for the despeckle pass
    const uint8_t bodyPiece = (uint8_t)(((partHasRoleCat(*it.part, Role::Armour) || partHasRoleCat(*it.part, Role::Pauldron) ||
                                          (r.guiseHead && partHasRoleCat(*it.part, Role::Headwear))) ? 1 : 0) |
                                        ((partHasRoleCat(*it.part, Role::Armour) || partHasRoleCat(*it.part, Role::Pauldron) ||
                                          partHasRoleCat(*it.part, Role::Helm) || partHasRoleCat(*it.part, Role::Overlay)) ? 2 : 0) |
                                        ((partHasRoleCat(*it.part, Role::Armour) || P.s(it.part->name).find("armour/motif/") != std::string::npos) ? 4 : 0) |
                                        (partHasRoleCat(*it.part, Role::Helm) ? 16 : 0));   // (envelope fix r2) bit 16: the helm (D62's helm rim)
    // (P2c forge) the cut tiles and edge stamps laid into the body's armour and pauldron views (a `when` overlay keyed on a
    // forge flag is the style's own drawing: no tile over it; edges only on the base views)
    if (!ftiles.empty() && !handOnly && partTierSlot(it.order) == HumanLook::TIER_BODY) {
      const bool arm = partHasRoleCat(*it.part, Role::Armour), pad = partHasRoleCat(*it.part, Role::Pauldron);
      if (arm || pad) {
        const bool forgeOverlay = it.sub && forgeCond(P.s(it.view->when));
        const bool hangItem = it.layer == (int)Layer::HangBack || it.layer == (int)Layer::HangFront;
        const int edgeMode = it.sub ? 0 : pad ? 2 : (arm && (hangArmour ? hangItem : it.layer == (int)Layer::Torso)) ? 1 : 0;
        vp = forgeView(P, *it.view, ftiles, !forgeOverlay, edgeMode, it.view->facing, 0);
      }
    }
    const ViewRec& v = *vp;
    const uint8_t heldMark = isHeld(*it.part) ? 1 : 0;
    // (wave-2 fix round, D82) the gripL shield: its pixels survive every other part's erase cells
    const uint8_t shieldMark = fixOn(FIX_SHIELD) && partHasRoleCat(*it.part, Role::Shield) && it.anchor == Anchor::GripL ? 1 : 0;
    // (wave-2 fix round, D78) the trim band: on a T4-T6 piece every 4-connected run of R pixels widened inward from the part's
    // outline to the tier's width (T4 1 px, T5-T6 2), painted from the trim tile's view for that tier; only adds trim (a run at
    // width keeps every pixel), never over a G or N pixel, an outline pixel or a pixel of another part (the view's own pixels)
    static thread_local std::vector<uint8_t> bandReg;
    static thread_local std::vector<int8_t> bandRow;
    const ViewRec* bandView = nullptr;
    bool bandOn = false;
    {
      const int bts = partTierSlot(it.order);
      const int btier = bts >= 0 ? r.pieceTier[bts] : 0;
      if (bandTile && !handOnly && !v.regions.empty() && btier >= 4 && btier <= 6) {
        bandView = trimBandView(P, *bandTile, btier);
        bandOn = true;
        trimBand(v, btier >= 5 ? 2 : 1, bandReg, bandRow);
      }
    }
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
    // (P2c ladder) LADDER-FINISH's placements on this view: the P box (fit tiles) and the W / G blobs (stamp tiles)
    struct Place { const ViewRec* v = nullptr; int ox = 0, oy = 0; };
    static thread_local std::vector<Place> fitAt;
    static thread_local std::vector<int> blob;
    struct Blob { int x0, y0, x1, y1, n, metal; uint8_t rg; };
    static thread_local std::vector<Blob> blobs;
    fitAt.assign(rtiles.size(), Place{});
    blobs.clear();
    bool anyStamp = false;
    bool forgeFit = false;   // (P2c forge) a fittings tile placed on this slot's P box
    for (const RegionTile& rt : rtiles) if (rt.forgeSlot >= 0 && rt.forgeSlot == ts && rt.fit) forgeFit = true;
    if (hasReg && ts >= 0 && (r.finTiles[ts] || forgeFit)) {
      int px0 = v.w, py0 = v.h, px1 = -1, py1 = -1;
      for (int y = 0; y < v.h; y++)
        for (int x = 0; x < v.w; x++)
          if (v.cells[(size_t)y * v.w + x] >= 2 && v.regions[(size_t)y * v.w + x] == (uint8_t)Region::Panel) {
            px0 = std::min(px0, x); py0 = std::min(py0, y); px1 = std::max(px1, x); py1 = std::max(py1, y);
          }
      for (size_t k = 0; k < rtiles.size(); k++) {
        const RegionTile& rt = rtiles[k];
        if (rt.forgeSlot >= 0 ? rt.forgeSlot != ts : (!rt.fin || !(r.finTiles[ts] & rt.fin))) continue;
        if (rt.stamp) anyStamp = true;
        if (!rt.fit || px1 < 0) continue;
        const int bw = px1 - px0 + 1, bh = py1 - py0 + 1;
        if (const ViewRec* fv = fitView(P, *rt.part, fitPrefix(P, *rt.part, rt.fin, r.motif), bw, bh))
          fitAt[k] = Place{fv, px0 + (bw - fv->w) / 2, py0 + (bh - fv->h) / 2};
      }
      if (anyStamp) {
        blob.assign((size_t)v.w * v.h, -1);
        static thread_local std::vector<int> stack;
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
    // (forge round 2, contracts 3c.1) the iridescent bands: a skirt's or motif's metal alternates Metal and Metal 2 per band
    // between its N seams (every 3 drawn rows where none is marked)
    static thread_local std::vector<uint8_t> irisBand;
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
        static thread_local std::vector<int> metalRow;
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
        const uint8_t rg = c >= 2 && hasReg ? (bandOn ? bandReg[(size_t)y * v.w + x] : v.regions[(size_t)y * v.w + x]) : 0;
        if (handOnly && (c < 2 || rg != (uint8_t)Region::Hand)) continue;
        if (rg && rg < REGION_COUNT && r.region[rg] != 0xFF) c = code((Slot)r.region[rg], codeStep(c));
        // (P2c ladder) R P G N W by the piece's own tier (the outline ring keeps its slot)
        if (rg >= (uint8_t)Region::Trim && rg < REGION_COUNT && ts >= 0 && c >= 2 && codeStep(c) >= 1) {
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
            const int W = r.pieceTier[partTierSlot(it.order)] >= 5 ? 2 : 1;
            int ty = std::min(row, (int)tv.h - 1);
            if ((int)tv.h >= 2 * W && bandFacesDownRight(v, x, y)) ty = std::min(W + row, (int)tv.h - 1);
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
        // (INTEGRATE silhouettes i1) the glints (step 4) stay in the body's Metal: the sheen's spark reads on both bands
        if (it.iris && c >= 2 && codeSlot(c) == Slot::Metal && codeStep(c) >= (it.irisRows ? 0 : 1) && codeStep(c) <= (it.irisRows ? 4 : 3) && irisBand[(size_t)y]) c = code(Slot::Metal2, codeStep(c));   // (silhouettes i2: the body's bands take the plate's rim and glint too, so the second hue reads as a plate, C17 20 %)
        if ((toMetal || brassBody) && c >= 2) {
          const Slot qs = codeSlot(c);
          if (qs == Slot::Head || qs == Slot::Cloth1 || qs == Slot::Cloth2 || qs == Slot::Cloth3 || qs == Slot::Linen || (toMetal && qs == Slot::Leather) ||
              (brassBody && qs == Slot::Accent))
            c = code(Slot::Metal, codeStep(c));
        }
        // (P2c) a region tile over this region's pixels (outlines kept): ink keeps the slot; step 2 keeps the step, 1 / 3 move
        // it one down / up, 0 and 4 set it
        // (ladder) tiles stack: the last-listed finish tile first (glyph over filigree); a clear cell falls through to the next
        if (rg && !rtiles.empty() && codeStep(c) >= 1)
          for (size_t ri = rtiles.size(); ri-- > 0;) {
            const RegionTile& rt = rtiles[ri];
            if (!(rt.mask >> rg & 1)) continue;
            if (rt.fin && !(ts >= 0 && (r.finTiles[ts] & rt.fin))) continue;   // (ladder) a finish tile on its tier's pieces only
            if (rt.forgeSlot >= 0 && ts != rt.forgeSlot) continue;               // (P2c forge) fittings on their slot's pieces
            const ViewRec* tv = nullptr;
            int tx = 0, ty = 0;
            bool placed = false;
            if (rt.fit && fitAt[ri].v) {
              tv = fitAt[ri].v; tx = x - fitAt[ri].ox; ty = y - fitAt[ri].oy;
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
                g_bodyPiece[q] = 0;
                g_depthPx[q] = 1;
                heldPx[q] = 0;
                shieldPx[q] = 1;
                if (g_probeOwner) { (*g_probeOwner)[q] = shieldKeepOw[q]; (*g_probeRegion)[q] = shieldKeepRg[q]; }
                continue;
              }
              o.code[(size_t)Y * CW + X] = 0;
              g_bodyPiece[(size_t)Y * CW + X] = 0;
              g_depthPx[(size_t)Y * CW + X] = 0;
              if (g_probeOwner) { (*g_probeOwner)[(size_t)Y * CW + X] = -1; (*g_probeRegion)[(size_t)Y * CW + X] = 0; }
              continue;
            }
            uint8_t w = c;
            // the pattern tile: a cloth pixel (not its outline) takes the tile's slot where the tile marks it, keeping
            // the cloth's own step (so the garment's shading runs through the pattern); the tile's grid: Item::tx, ty
            if (tiled && (uint8_t)codeSlot(c) == tileOn && codeStep(c) >= 1) {
              const ViewRec& t = *tile.v;
              int tx = (X - it.tx + t.ox) % t.w, ty = (Y - it.ty + t.oy) % t.h;
              if (tx < 0) tx += t.w;
              if (ty < 0) ty += t.h;
              const uint8_t tc = t.cells[(size_t)ty * t.w + tx];
              if (tc >= 2) w = code(codeSlot(tc), codeStep(c));
            }
            o.code[(size_t)Y * CW + X] = w;
            g_bodyPiece[(size_t)Y * CW + X] = (uint8_t)(bodyPiece | (rg == (uint8_t)Region::Trim ? 8 : 0));   // (envelope fix round) bit 8: the drawn trim (R)
            g_depthPx[(size_t)Y * CW + X] = depthMark;
            heldPx[(size_t)Y * CW + X] = heldMark;
            shieldPx[(size_t)Y * CW + X] = shieldMark;
            if (shieldMark) {
              const size_t q = (size_t)Y * CW + X;
              shieldKeep[q] = w;
              shieldKeepRg[q] = rg;
              shieldKeepOw[q] = (int16_t)(&it - items.data());
            }
            if (g_probeOwner) { (*g_probeOwner)[(size_t)Y * CW + X] = (int16_t)(&it - items.data()); (*g_probeRegion)[(size_t)Y * CW + X] = rg; }
            if (X < o.bx0) o.bx0 = X;
            if (X > o.bx1) o.bx1 = X;
            if (Y < o.by0) o.by0 = Y;
            if (Y > o.by1) o.by1 = Y;
          }
        }
      }
    }
  };
  g_probeGrip = gripFrom; g_probeGripL = gripLFrom;
  for (const Item& it : items) {
    paint(it, false);
    // (P2c) the hand over the grip: after a library weapon on the grip, the main hand; on a two-handed weapon the off hand
    // too; after a library weapon on gripL (the bow), the bow hand
    if (!libraryHeld(P, *it.part)) continue;
    if (it.anchor == Anchor::Grip) {
      if (gripFrom >= 0 && &items[(size_t)gripFrom] != &it) paint(items[(size_t)gripFrom], true);
      if (twoHand && gripLFrom >= 0 && &items[(size_t)gripLFrom] != &it) paint(items[(size_t)gripLFrom], true);
    } else if (it.anchor == Anchor::GripL && gripLFrom >= 0 && &items[(size_t)gripLFrom] != &it) {
      paint(items[(size_t)gripLFrom], true);
    }
  }
  despecklePass(r, o);
  glintPass(r, o);
  glintCapPass(r, o);
  glassPass(r, o);
  starBandPass(r, o);
  mythicFirePass(r, o);
  // (forge fix i2, B8 and STYLE_P2 "no 1 px speckle") a round-2 silhouette lays a shoulder mass, a skirt and an over-layer
  // the art drew apart; where two of them meet at a corner a single pixel can be left floating clear of the figure (mr_4's
  // drop shoulders over its tabard, a pixel off the arm's corner on some frames). A figure pixel with no figure neighbour
  // (8-connected) is cleared; a held item's pixels stay
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
  // (silhouettes i2, the blind panel: "Mythic: a fire-sparkle soup of bright 1-2 px specks, the face only just readable";
  // C13's bright-pixel budget) on the Mythic a glow speck (a blob of 1-2 pixels) goes dark: the fire lives
  // in the cracks and the rims, never as speckle
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

// a code buffer (w x h, its pivot at pvx, pvy) trimmed into a Cut coloured by the recipe's ramps
Cut cutOf(const DollRecipe& r, const uint8_t* code, int w, int h, int stride, int pvx, int pvy) {
  int x0 = w, y0 = h, x1 = -1, y1 = -1;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      if (code[(size_t)y * stride + x] >= 2) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
  Cut cut;
  if (x1 < 0) return cut;
  cut.c = Canvas(x1 - x0 + 1, y1 - y0 + 1);
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      const uint8_t c = code[(size_t)y * stride + x];
      if (c >= 2) cut.c.px[(size_t)(y - y0) * cut.c.w + (x - x0)] = r.ramps.c[(int)codeSlot(c)][codeStep(c)] | 0xFF000000u;
    }
  cut.px = pvx - x0; cut.py = pvy - y0;
  cut.an.topY = (int16_t)(y0 - pvy);
  // (P2c ladder) the glow plane's crop: the Glow-slot pixels (after the finishing pass)
  {
    int gx0 = w, gy0 = h, gx1 = -1, gy1 = -1, n = 0;
    for (int y = y0; y <= y1; y++)
      for (int x = x0; x <= x1; x++) {
        const uint8_t c = code[(size_t)y * stride + x];
        if (c >= 2 && codeSlot(c) == Slot::Glow) { gx0 = std::min(gx0, x); gy0 = std::min(gy0, y); gx1 = std::max(gx1, x); gy1 = std::max(gy1, y); n++; }
      }
    if (n) {
      cut.g = Canvas(gx1 - gx0 + 1, gy1 - gy0 + 1);
      for (int y = gy0; y <= gy1; y++)
        for (int x = gx0; x <= gx1; x++) {
          const uint8_t c = code[(size_t)y * stride + x];
          if (c >= 2 && codeSlot(c) == Slot::Glow) cut.g.px[(size_t)(y - gy0) * cut.g.w + (x - gx0)] = r.ramps.c[(int)Slot::Glow][codeStep(c)] | 0xFF000000u;
        }
      cut.gx = gx0 - x0; cut.gy = gy0 - y0; cut.gn = n;
    }
  }
  return cut;
}

// atX, atY (P2c SLICE-DOLL i2): an upright frame's `at`, the figure drawn that far right / down of its pivot (a lunge's
// step into the blow, the recoil's step back): the pivot moves the other way in the cut
Cut toCut(const DollRecipe& r, FrameOut& o, const ComposeOpts& op, int atX = 0, int atY = 0) {
  if (o.bx1 < o.bx0) return Cut();
  const int bw = o.bx1 - o.bx0 + 1, bh = o.by1 - o.by0 + 1;
  uint8_t* at = &o.code[(size_t)o.by0 * CW + o.bx0];
  if (op.finish) finishPass(at, bw, bh, CW);
  Cut cut;
  if (op.trim) cut = cutOf(r, at, bw, bh, CW, CPX - o.bx0 - atX, CPY - o.by0 - atY);
  else cut = cutOf(r, o.code.data(), CW, CH, CW, CPX - atX, CPY - atY);   // (the whole scratch canvas: parity diffs)
  if (cut.c.w <= 0) return cut;
  if (!op.trim) {   // the scratch canvas keeps its size (the box of the opaque pixels is not cut away)
    Canvas full(CW, CH);
    for (int y = 0; y < cut.c.h; y++)
      for (int x = 0; x < cut.c.w; x++) full.px[(size_t)(y + CPY - cut.py) * CW + (x + CPX - cut.px)] = cut.c.px[(size_t)y * cut.c.w + x];
    cut.c = std::move(full);
    cut.gx += CPX - cut.px; cut.gy += CPY - cut.py;   // (the glow crop rides the same move)
    cut.px = CPX; cut.py = CPY;
  }
  RigAnchors& an = cut.an;
  const Pool& P = get();
  const Ids& I = P.ids;
  auto mark = [&](uint16_t id, int16_t& ax, int16_t& ay) {
    if (id == 0xFFFF) return;
    for (int i = 0; i < o.nMarks; i++) if (o.markIds[i] == id && o.marks[i].on) { ax = (int16_t)(o.marks[i].x + atX); ay = (int16_t)(o.marks[i].y + atY); }
  };
  const Pt& hd = o.anchors[(int)Anchor::Head];
  an.headX = (int16_t)(hd.x + atX); an.headY = (int16_t)(hd.y + atY); an.headW = 16; an.headH = 14;
  mark(I.face, an.faceX, an.faceY);
  mark(I.handR, an.handRX, an.handRY);
  mark(I.handL, an.handLX, an.handLY);
  mark(I.grip, an.gripX, an.gripY);
  mark(I.tip, an.tipX, an.tipY);
  const Pt& hp = o.anchors[(int)Anchor::Hip];
  an.hipX = (int16_t)(hp.x + atX); an.hipY = (int16_t)(hp.y + atY);
  mark(I.hip, an.hipX, an.hipY);
  int16_t fy = 0;
  mark(I.footL, an.footLX, fy);
  mark(I.footR, an.footRX, fy);
  an.weaponBehind = o.behind;
  return cut;
}

// the turned figure (STYLE_P2 13): the raw left-view frame in o turned a quarter clockwise (east: head east) or
// transposed (west: the turn mirrored, head west), finished again; the pivot under the middle of the body (halfway from
// the soles to the head box's top) on the figure's lowest row (its back), moved by the frame's `at`
Cut turnedCut(const DollRecipe& r, FrameOut& o, uint8_t turn, int atX, int atY, const ComposeOpts& op, const FrameOut* screen = nullptr) {
  if (o.bx1 < o.bx0) return Cut();
  const int bw = o.bx1 - o.bx0 + 1, bh = o.by1 - o.by0 + 1;
  const int tw = bh, th = bw;
  static thread_local std::vector<uint8_t> t;
  t.assign((size_t)tw * th, 0);
  for (int y = 0; y < bh; y++)
    for (int x = 0; x < bw; x++) {
      const uint8_t c = o.code[(size_t)(o.by0 + y) * CW + o.bx0 + x];
      if (!c) continue;
      const int X = turn == kTurnEast ? bh - 1 - y : y, Y = x;
      t[(size_t)Y * tw + X] = c;
    }
  // the middle of the body in the frame's box, then turned
  const int midY = o.anchors[(int)Anchor::Head].y / 2;
  const int my = CPY + midY - o.by0;   // (the turned x of the body's middle; its column becomes a row)
  const int tmx = turn == kTurnEast ? bh - 1 - my : my;
  int low = -1;
  for (int y = th - 1; y >= 0 && low < 0; y--)
    for (int x = 0; x < tw; x++) if (t[(size_t)y * tw + x] >= 2) { low = y; break; }
  if (low < 0) return Cut();
  int pvx = tmx - atX, pvy = low - atY;
  // (P2c SLICE-DOLL i2) the screen-space items (the corpse's dropped spear / bow): composed alone in the left view, their
  // pivot at CPX, CPY, laid UNDER the turned body at its pivot (the body hides what it lies on)
  if (screen && screen->bx1 >= screen->bx0) {
    const int PAD = 48, Tw = tw + 2 * PAD, Th = th + 2 * PAD;
    static thread_local std::vector<uint8_t> T;
    T.assign((size_t)Tw * Th, 0);
    for (int y = 0; y < th; y++) std::memcpy(&T[(size_t)(y + PAD) * Tw + PAD], &t[(size_t)y * tw], (size_t)tw);
    for (int y = screen->by0; y <= screen->by1; y++)
      for (int x = screen->bx0; x <= screen->bx1; x++) {
        const uint8_t c = screen->code[(size_t)y * CW + x];
        if (c < 2) continue;
        // (head east: the ground item mirrored, so it lies by the near hand toward the feet either way)
        // (ladder i3: the panel saw the spears "a separate line 2-3 px below each corpse") laid 3 px up, under the body:
        // the corpse hides its middle and the ends show either side, so the body lies ON its weapon
        const int X = PAD + pvx + (turn == kTurnEast ? CPX - x : x - CPX), Y = PAD + pvy + y - CPY - 3;
        if (X < 0 || Y < 0 || X >= Tw || Y >= Th) continue;
        uint8_t& d = T[(size_t)Y * Tw + X];
        if (d < 2) d = c;
      }
    if (op.finish) finishPass(T.data(), Tw, Th, Tw);
    return cutOf(r, T.data(), Tw, Th, Tw, pvx + PAD, pvy + PAD);
  }
  if (op.finish) finishPass(t.data(), tw, th, tw);
  Cut cut = cutOf(r, t.data(), tw, th, tw, pvx, pvy);
  return cut;
}

struct FrameJob { int facing; const FrameRec* f; bool lying; uint8_t variant; };
uint8_t turnOf(const FrameRec& f, bool lyingClip, uint8_t variant) {
  if (f.turn) return f.turn;
  if (lyingClip) return variant == 1 ? kTurnEast : kTurnWest;
  return kTurnNone;
}
Cut composeOne(const DollRecipe& r, int facing, const FrameRec& f, uint8_t turn, FrameOut& fo, const ComposeOpts& op) {
  if (turn) {
    static thread_local FrameOut so;
    g_screenMode = 2;
    composeFrame(r, 3, f, so);
    g_screenMode = 1;
    composeFrame(r, 3, f, fo);
    g_screenMode = 0;
    return turnedCut(r, fo, turn, f.atX, f.atY, op, &so);
  }
  composeFrame(r, facing, f, fo);
  return toCut(r, fo, op, f.atX, f.atY);
}

uint8_t partOf(RigPart p) { return poolPart(p); }

}  // namespace

// ---------------------------------------------------------------- the public calls
const uint32_t kGreyRamp[5] = {0xFF202020u, 0xFF555555u, 0xFF8C8C8Cu, 0xFFC8C8C8u, 0xFFF4F4F4u};
bool greySlot(Slot s) {
  switch (s) {
    case Slot::Cloth1: case Slot::Cloth2: case Slot::Cloth3: case Slot::Accent: case Slot::Linen: case Slot::Leather: case Slot::Metal:
    case Slot::Metal2: case Slot::Blade: case Slot::Cloak: case Slot::Pattern: case Slot::Plume: case Slot::Field: case Slot::Device:
    case Slot::Glow: case Slot::Head: return true;   // (forge i2) the headwear is gear: HOMESPUN's hood
    default: return false;
  }
}
DollRamps greyRamps(const DollRamps& r) {
  DollRamps g = r;
  for (int s = 0; s < SLOT_COUNT; s++)
    if (greySlot((Slot)s)) for (int i = 0; i < 5; i++) g.c[s][i] = kGreyRamp[i];
  return g;
}
RigSheet compose(const DollRecipe& rIn, RigPart part, const ComposeOpts* opts) {
  const ComposeOpts op = opts ? *opts : ComposeOpts{};
  struct DeathFlag { DeathFlag(bool v) { g_deathPart = v; } ~DeathFlag() { g_deathPart = false; } } deathFlag(part == RigPart::Death);
  // (P2c forge) the neutral-ramp compose: every material slot in one grey ramp (the shape with the colour removed)
  DollRecipe greyR;
  if (op.grey) { greyR = rIn; greyR.ramps = greyRamps(rIn.ramps); }
  const DollRecipe& r00 = op.grey ? greyR : rIn;
  const Pool& P = get();
  // (forge round 2) noHeld: no held weapon, shield, sheath or back item (the outline masks of C16o and C18)
  DollRecipe bareR;
  if (op.noHeld) {
    bareR = r00;
    bareR.parts.erase(std::remove_if(bareR.parts.begin(), bareR.parts.end(), [&](int pi) {
                        const PartRec& p = P.parts[(size_t)pi];
                        return partHasRoleCat(p, Role::Held) || partHasRoleCat(p, Role::Shield) || partHasRoleCat(p, Role::Sheath) ||
                               partHasRoleCat(p, Role::Fx);
                      }),
                      bareR.parts.end());
  }
  // (wave-2 fix round, D77) noKitCloak: the kit's worn cloak item off a culture alloy whose over-layer used to replace it
  if (op.noKitCloak && r00.kitCloak) {
    if (!op.noHeld) bareR = r00;
    bareR.parts.erase(std::remove_if(bareR.parts.begin(), bareR.parts.end(), [&](int pi) { return partHasRoleCat(P.parts[(size_t)pi], Role::Cloak); }),
                      bareR.parts.end());
    bareR.pauldronTop = bareR.pauldronTopAlloy;
    bareR.kitCloak = false;
  }
  // (fix round 2, D77 restated) an alloy's own CAPE / HALFCAPE at its T3 view: the tier's cloak.t4..t6 flags off
  if (op.noKitCloak && r00.layerCloakT3) {
    if (!op.noHeld) bareR = r00;
    bareR.flags.erase(std::remove_if(bareR.flags.begin(), bareR.flags.end(),
                                     [](const std::string& f) { return f == "cloak.t4" || f == "cloak.t5" || f == "cloak.t6"; }),
                      bareR.flags.end());
    bareR.layerCloakT3 = false;
  }
  const DollRecipe& r0 = op.noHeld || (op.noKitCloak && (r00.kitCloak || r00.layerCloakT3)) ? bareR : r00;
  // (P2b fix B10-CIVIL-STAFF-RUN-ATLAS) the flag `run` while the RUN part composes: a long carried tool (the staff) draws
  // its run carry there (`when run replace`: held close and upright, so the run's frames stay inside B10's 64 KB)
  DollRecipe runR;
  if (part == RigPart::Run) { runR = r0; runR.flags.push_back("run"); }
  const DollRecipe& r = part == RigPart::Run ? runR : r0;
  // (INTEGRATION 2026-10-23) the flag `walk` on the walk clips of every other part (the LOCO walk, the drawn walk): a part
  // may fit the stepping body there (`when walk replace`) without touching the idles, hurts, swings and falls that share its
  // keys (her cloak in the walk's passing frames: B11 W8)
  DollRecipe walkR = r;
  if (part != RigPart::Run) walkR.flags.push_back("walk");
  AtlasBuilder ab;
  const bool breeze = condHolds(r, "breeze");
  std::vector<const ClipRec*> sorted = clipsOf(r.bodyKey, partOf(part), r.family, -1, &r);
  // facing, then clip, then variant: the atlas index order
  std::stable_sort(sorted.begin(), sorted.end(), [](const ClipRec* a, const ClipRec* b) {
    if (a->facing != b->facing) return a->facing < b->facing;
    if (a->clip != b->clip) return a->clip < b->clip;
    return a->variant < b->variant;
  });
  static thread_local FrameOut fo;
  // a lying clip is the same turned figure whatever the facing: composed once per variant, shared by every facing
  // that has no lying clip of its own (the atlas keeps one copy)
  std::vector<int> lyingSrc[RIG_VARIANTS];
  bool lyingHas[4][RIG_VARIANTS] = {};
  for (const ClipRec* c : sorted) if (c->clip == (uint8_t)Clip::Lying && c->variant < RIG_VARIANTS) lyingHas[c->facing & 3][c->variant] = true;
  for (const ClipRec* c : sorted) {
    if (op.onlyFacing >= 0 && c->facing != op.onlyFacing) continue;
    if (c->clip >= CLIP_COUNT) continue;
    const bool lying = c->clip == (uint8_t)Clip::Lying;
    for (uint16_t i = 0; i < c->nFrames; i++) {
      const FrameRec& f = P.frames[c->firstFrame + i];
      // (polish i1) the walk stirs too: its passing frames (1 and 4 of 6, where the hair swings) draw the breeze views, so
      // the T5-T6 cloak's hem and the plume flutter at the step's rhythm (the panel: "cloak/plume flutter in idle and loco")
      g_breeze = breeze && ((c->clip == (uint8_t)Clip::Idle && i == 1) || (c->clip == (uint8_t)Clip::Walk && c->nFrames == 6 && i % 3 == 1));
      Cut cut = composeOne(c->clip == (uint8_t)Clip::Walk ? walkR : r, c->facing, f, turnOf(f, lying, c->variant), fo, op);
      g_breeze = false;
      if (cut.c.w <= 0) continue;
      const int src = ab.add(c->facing, (Clip)c->clip, c->variant, std::move(cut));
      if (lying && c->variant < RIG_VARIANTS) lyingSrc[c->variant].push_back(src);
    }
  }
  for (int v = 0; v < RIG_VARIANTS; v++) {
    if (lyingSrc[v].empty()) continue;
    for (int fc = 0; fc < 4; fc++) {
      if (lyingHas[fc][v] || (op.onlyFacing >= 0 && fc != op.onlyFacing)) continue;
      // (the first facing's frames: a facing of its own already added them above)
      for (int src : lyingSrc[v]) ab.addShared(fc, Clip::Lying, v, src);
      lyingHas[fc][v] = true;
    }
  }
  return ab.finish();
}
RigSheet compose(const HumanLook& look, RigPart part) { return compose(resolve(look), part); }

namespace {
// the clip frame compose draws for (facing, clip, variant, i) of a part, with the recipe compose uses (run / walk flags)
const FrameRec* pickFrame(const DollRecipe& r0, RigPart part, int facing, Clip clip, int variant, int i, DollRecipe& r, uint8_t& turn) {
  const Pool& P = get();
  r = r0;
  if (part == RigPart::Run) r.flags.push_back("run");
  else if (clip == Clip::Walk) r.flags.push_back("walk");
  const ClipRec* pick = nullptr;
  for (int pass = 0; pass < 2 && !pick; pass++)
    for (const ClipRec* c : clipsOf(r.bodyKey, partOf(part), r.family, -1, &r))
      if (c->clip == (uint8_t)clip && c->facing == facing && c->variant == (pass ? 0 : variant)) { pick = c; break; }
  if (!pick && clip == Clip::Lying)   // a lying clip of one facing serves every facing
    for (const ClipRec* c : clipsOf(r.bodyKey, partOf(part), r.family, -1, &r))
      if (c->clip == (uint8_t)clip && c->variant == variant) { pick = c; break; }
  if (!pick || !pick->nFrames) return nullptr;
  const FrameRec& f = P.frames[pick->firstFrame + (uint16_t)(((i % pick->nFrames) + pick->nFrames) % pick->nFrames)];
  turn = turnOf(f, clip == Clip::Lying, pick->variant);
  return &f;
}
}  // namespace

FrameInfo frameInfo(const DollRecipe& r0, RigPart part, int facing, Clip clip, int variant, int i) {
  struct DeathFlag { DeathFlag(bool v) { g_deathPart = v; } ~DeathFlag() { g_deathPart = false; } } deathFlag(part == RigPart::Death);
  FrameInfo out;
  const Pool& P = get();
  DollRecipe r;
  uint8_t turn = 0;
  const FrameRec* fp = pickFrame(r0, part, facing, clip, variant, i, r, turn);
  if (!fp) return out;
  const FrameRec& f = *fp;
  static thread_local FrameOut fo;
  composeFrame(r, turn ? 3 : facing, f, fo);
  out.ok = true;
  out.ms = f.ms; out.turn = turn; out.weapon = f.weapon;
  for (const Item& it : g_items) out.parts.push_back(FrameInfo::P{P.s(it.part->name), layerName((Layer)it.layer), P.s(it.view->key)});
  return out;
}

FrameProbe probeFrame(const DollRecipe& r0, RigPart part, int facing, Clip clip, int variant, int i, bool finish) {
  struct DeathFlag { DeathFlag(bool v) { g_deathPart = v; } ~DeathFlag() { g_deathPart = false; } } deathFlag(part == RigPart::Death);
  FrameProbe out;
  const Pool& P = get();
  DollRecipe r;
  uint8_t turn = 0;
  const FrameRec* fp = pickFrame(r0, part, facing, clip, variant, i, r, turn);
  if (!fp) return out;
  static thread_local FrameOut fo;
  std::vector<int16_t> owner((size_t)CW * CH, -1);
  std::vector<uint8_t> region((size_t)CW * CH, 0);
  g_probeOwner = &owner; g_probeRegion = &region;
  composeFrame(r, turn ? 3 : facing, *fp, fo);
  g_probeOwner = nullptr; g_probeRegion = nullptr;
  out.ok = true;
  out.w = CW; out.h = CH; out.px = CPX - (turn ? 0 : fp->atX); out.py = CPY - (turn ? 0 : fp->atY); out.turn = turn; out.ms = fp->ms; out.weapon = fp->weapon; out.front = fp->front != 0;
  out.code = fo.code;
  if (finish && fo.bx1 >= fo.bx0) finishPass(&out.code[(size_t)fo.by0 * CW + fo.bx0], fo.bx1 - fo.bx0 + 1, fo.by1 - fo.by0 + 1, CW);
  for (size_t k = 0; k < out.code.size(); k++) if (out.code[k] < 2) owner[k] = -1;
  out.owner = std::move(owner);
  out.region = std::move(region);
  for (const Item& it : g_items) {
    FrameProbe::It x;
    x.part = P.s(it.part->name); x.key = P.s(it.view->key);
    x.partIndex = (int)(it.part - P.parts.data());
    x.side = it.part->side; x.layer = (Layer)it.layer; x.anchor = it.anchor; x.ax = it.ax; x.ay = it.ay;
    for (const RoleRec& rr : it.part->roles) {
      x.roles |= 1u << rr.cat;
      // (wave-2 fix round, D81) a library value (`<weapon>.<form>`) names the held part wherever it is listed (the spear's map
      // lists its bare name first: C3d skipped it)
      if (rr.cat == (uint8_t)Role::Held && (x.held.empty() || (x.held.find('.') == std::string::npos && P.s(rr.value).find('.') != std::string::npos)))
        x.held = P.s(rr.value);
    }
    out.items.push_back(x);
  }
  const Pt& hd = fo.anchors[(int)Anchor::Head];
  out.headX = CPX + hd.x; out.headY = CPY + hd.y;
  const Ids& I = P.ids;
  bool handR = false, handL = false;
  int hRX = 0, hRY = 0, hLX = 0, hLY = 0;
  for (int k = 0; k < fo.nMarks; k++) {
    if (!fo.marks[k].on) continue;
    const int X = CPX + fo.marks[k].x, Y = CPY + fo.marks[k].y;
    if (fo.markIds[k] == I.grip) { out.grip = true; out.gripX = X; out.gripY = Y; }
    else if (fo.markIds[k] == I.gripL) { out.gripL = true; out.gripLX = X; out.gripLY = Y; }
    else if (fo.markIds[k] == I.tip) { out.tip = true; out.tipX = X; out.tipY = Y; }
    else if (I.handR != 0xFFFF && fo.markIds[k] == I.handR) { handR = true; hRX = X; hRY = Y; }
    else if (I.handL != 0xFFFF && fo.markIds[k] == I.handL) { handL = true; hLX = X; hLY = Y; }
  }
  // (wave-2 fix round, D81) the compositor's fallback, as placed: an arm that marks no grip carries at its hand mark
  if (!out.grip && handR && g_probeGrip >= 0) { out.grip = true; out.gripX = hRX; out.gripY = hRY; }
  if (!out.gripL && handL && g_probeGripL >= 0) { out.gripL = true; out.gripLX = hLX; out.gripLY = hLY; }
  out.gripFrom = g_probeGrip; out.gripLFrom = g_probeGripL;
  return out;
}

RigSheet composePosture(const DollRecipe& r0, Posture p, uint8_t variant) {
  const Pool& P = get();
  AtlasBuilder ab;
  const ComposeOpts op;
  static thread_local FrameOut fo;
  // (POSES-B) the posture's prop in hand; per facing the variant's own clips, else variant 0's (never both)
  const DollRecipe r = withPosture(r0, p, variant);
  for (const ClipRec* c : postureClips(r.bodyKey, r.family, (int)p, variant)) {
    for (uint16_t i = 0; i < c->nFrames; i++) {
      const FrameRec& f = P.frames[c->firstFrame + i];
      Cut cut = composeOne(r, c->facing, f, f.turn, fo, op);
      if (cut.c.w > 0) ab.add(c->facing, Clip::Idle, 0, std::move(cut));
    }
  }
  return ab.finish();
}
RigSheet composePosture(const HumanLook& look, Posture p, uint8_t variant) { return composePosture(resolve(look), p, variant); }

RigSheet composePart(const DollRecipe& r, const std::string& partName, int facing, const std::string& key) {
  const int pi = findPart(partName);
  AtlasBuilder ab;
  if (pi < 0) return ab.finish();
  FrameRec neutral;
  const std::vector<int> only{pi};
  FrameOut fo;
  composeFrame(r, facing & 3, neutral, fo, &only, key.c_str());
  Cut cut = toCut(r, fo, ComposeOpts{});
  if (cut.c.w > 0) ab.add(facing & 3, Clip::Idle, 0, std::move(cut));
  return ab.finish();
}

Canvas figureStill(const HumanLook& look, int facing) {
  if (!dollCovers(look, RigPart::Loco)) return Canvas();
  ComposeOpts one;
  one.onlyFacing = facing & 3;
  const RigSheet s = compose(resolve(look), RigPart::Loco, &one);
  const FrameRect* f = s.frame(facing, Clip::Idle, 0, 0);
  if (!f) return Canvas();
  Canvas c(f->w, f->h);
  for (int y = 0; y < f->h; y++) for (int x = 0; x < f->w; x++) c.set(x, y, s.atlas.get(f->x + x, f->y + y));
  return c;
}

// ---------------------------------------------------------------- the sleeper (sleeper2)
// The sleeper fitted to a berth (STYLE_P2 15; HOLDERS' berth2 geometry): the body set's Sleep posture clip (POSES-B's
// postures.clip: frame 0 and 1 the breath) composed with hats off; north-south berths take its down view, the face's
// centre on the berth's pillow (headX, headY) and everything below the turned-down sheet (quiltTop) hidden under the
// berth's own blanket; across berths (head west) take its left view transposed like a corpse. The body under the blanket
// shows as a mound DERIVED from the composed body (its silhouette's lit and shaded flanks, alpha only over the quilt),
// so the mound is the drawn body's own shape. No Sleep clip: an empty canvas (the view draws v1).
namespace {
// what a sleeper keeps: no headwear or helm (hats off in bed), nothing in hand, no sheath, shield or effect
bool notHeadwear(const PartRec& p) {
  switch ((Layer)p.layer) {
    case Layer::Headwear: case Layer::Held: case Layer::HeldBack: case Layer::Shield: case Layer::Fx: case Layer::FxBack: return false;
    default: break;
  }
  for (const RoleRec& rr : p.roles)
    if (rr.cat == (uint8_t)Role::Headwear || rr.cat == (uint8_t)Role::Helm || rr.cat == (uint8_t)Role::Held || rr.cat == (uint8_t)Role::Sheath ||
        rr.cat == (uint8_t)Role::Shield || rr.cat == (uint8_t)Role::Fx)
      return false;
  return true;
}
// the frame a sleeper is composed from: the Sleep posture's (POSES-B) when the body set has one for that facing; else the
// doll's own: north-south berths the LOCO idle's front frame, across berths the DEATH lying frame (the side view with the
// lying arms and legs, the pose a corpse is turned from); both with the eyes closed
bool sleeperFrame(const DollRecipe& r, int want, int frame, FrameRec& out) {
  const Pool& P = get();
  const ClipRec* clip = nullptr;
  const std::vector<const ClipRec*> cs = clipsOf(r.bodyKey, kPartPosture, r.family, (int)Posture::Sleep, &r);
  for (const ClipRec* c : cs) if (c->facing == want && c->variant == 0) clip = c;
  if (!clip) for (const ClipRec* c : cs) if (c->facing == want) clip = c;
  if (clip && clip->nFrames) { out = P.frames[clip->firstFrame + (uint16_t)(frame & 1) % clip->nFrames]; return true; }
  const bool across = want == 3;
  for (const ClipRec* c : clipsOf(r.bodyKey, across ? (uint8_t)RigPart::Death : (uint8_t)RigPart::Loco, r.family, -1, &r)) {
    if (c->clip != (uint8_t)(across ? Clip::Lying : Clip::Idle) || !c->nFrames) continue;
    if (!across && c->facing != 0) continue;
    if (clip && c->variant > clip->variant) continue;
    clip = c;
  }
  if (!clip) return false;
  out = P.frames[clip->firstFrame];
  out.turn = 0; out.atX = out.atY = 0; out.bob = 0;
  const uint16_t expr = P.strId("expr"), closed = P.strId("closed");
  if (expr != 0xFFFF && closed != 0xFFFF) {
    bool set = false;
    for (KeyRec& k : out.keys) if (k.channel == expr) { k.key = closed; set = true; }
    if (!set) out.keys.push_back(KeyRec{expr, closed});
  }
  return true;
}
}  // namespace

Canvas sleeper(const HumanLook& look, Berth b, int kit, int frame) {
  const holders::Berth2 g = holders::berth2(b, kit);
  if (g.w <= 0 || g.h <= 0) return Canvas();
  if (!dollCovers(look, RigPart::Loco) || !dollCovers(look, RigPart::Death)) return Canvas();
  const DollRecipe r = resolve(look);
  const Pool& P = get();
  const int want = g.across ? 3 : 0;
  FrameRec f;
  if (!sleeperFrame(r, want, frame, f)) return Canvas();
  static thread_local FrameOut fo;
  composeFrame(r, want, f, fo, nullptr, nullptr, notHeadwear);
  if (fo.bx1 < fo.bx0) return Canvas();
  const Ids& I = P.ids;
  Pt face;
  for (int i = 0; i < fo.nMarks; i++) if (fo.markIds[i] == I.face && fo.marks[i].on) face = fo.marks[i];
  const Pt hd = fo.anchors[(int)Anchor::Head];
  // the face's centre in the scratch canvas (pivot-relative): the face mark's eye row, else the head box's middle
  const int fcx = face.on ? (want == 3 ? face.x + 2 : face.x + 5) : hd.x + 8;
  const int fcy = face.on ? face.y + 2 : hd.y + 9;
  // the layout: (cx, cy) in the berth canvas of each scratch pixel; `under` when it lies under the blanket
  const int bw = fo.bx1 - fo.bx0 + 1, bh = fo.by1 - fo.by0 + 1;
  std::vector<uint8_t> body((size_t)g.w * g.h, 0), cover((size_t)g.w * g.h, 0);
  auto sag = [&](int x) {
    if (b != Berth::Hammock) return 0;
    const float t = std::clamp((x - 2) / (float)std::max(1, g.w - 5), 0.0f, 1.0f);
    return (int)std::lround(std::sin(t * 3.14159f) * 6.0f) - 3;
  };
  for (int y = 0; y < bh; y++)
    for (int x = 0; x < bw; x++) {
      const uint8_t c = fo.code[(size_t)(fo.by0 + y) * CW + fo.bx0 + x];
      if (c < 2) continue;
      const int sx = fo.bx0 + x - CPX, sy = fo.by0 + y - CPY;   // pivot-relative
      int cx, cy, along;
      if (!g.across) { cx = g.headX + (sx - fcx); cy = g.headY + (sy - fcy); along = cy; }
      else { cx = g.headX + (sy - fcy); cy = g.headY + (sx - fcx); cy += sag(cx); along = cx; }   // transposed: head west
      if (cx < 0 || cy < 0 || cx >= g.w || cy >= g.h) continue;
      if (along >= g.quiltTop) cover[(size_t)cy * g.w + cx] = c;
      else body[(size_t)cy * g.w + cx] = c;
    }
  finishPass(body.data(), g.w, g.h, g.w);
  Canvas out(g.w, g.h);
  // the derived mound: the covered body's silhouette, lit on its west / north flank, shaded on the other, the chest's
  // breath a lighter band on frame 1 (alpha over the berth's quilt; Ink at 0 alpha carries the shade, warm white the light)
  const uint32_t lit = rgba(255, 244, 214, 0), dark = rgba(33, 21, 42, 0);
  auto in = [&](int x, int y) { return x >= 0 && y >= 0 && x < g.w && y < g.h && cover[(size_t)y * g.w + x] >= 2; };
  for (int y = 0; y < g.h; y++)
    for (int x = 0; x < g.w; x++) {
      if (!in(x, y)) continue;
      const bool w0 = !in(x - 1, y), e0 = !in(x + 1, y), n0 = !in(x, y - 1), s0 = !in(x, y + 1);
      int a = 0;
      uint32_t col = 0;
      if (g.across ? n0 : w0) { col = lit; a = 70; }
      else if (g.across ? s0 : e0) { col = dark; a = 96; }
      else if (g.across ? (s0 || !in(x, y + 2)) : (e0 || !in(x + 2, y))) { col = dark; a = 52; }
      else if ((frame & 1) && (g.across ? (x - g.quiltTop) < 6 : (y - g.quiltTop) < 6)) { col = lit; a = 40; }
      if (!a) continue;
      out.set(x, y, (col & 0x00FFFFFFu) | (uint32_t)a << 24);
    }
  const DollRamps& R = r.ramps;
  for (int y = 0; y < g.h; y++)
    for (int x = 0; x < g.w; x++) {
      const uint8_t c = body[(size_t)y * g.w + x];
      if (c < 2) continue;
      uint32_t p = R.c[(int)codeSlot(c)][codeStep(c)] | 0xFF000000u;
      if (b == Berth::BunkLow) p = rgba((int)((p & 255) * 0.58f), (int)((p >> 8 & 255) * 0.58f), (int)((p >> 16 & 255) * 0.58f));   // the upper deck's shade
      out.set(x, y, p);
    }
  return out;
}

}  // namespace doll
}  // namespace art
