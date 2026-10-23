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
};
struct Tile { const ViewRec* v = nullptr; Slot on = Slot::Cloth1; };
// (P2c) a region tile (`tags tile` + `on-region <R>`): repeated over those regions' pixels on every part that marks them
// (ladder) fin: a finish tile's FIN_TILE_* bit; fit / stamp: LADDER-FINISH's placements (`tags fit`: the view <prefix><w>x<h>
// that fits the part's P box best, centred on it; `tags stamp`: a view laid once per connected blob of the region, keyed by
// the blob's size); part: the tile's part (its keyed views)
struct RegionTile { const ViewRec* v = nullptr; uint32_t mask = 0; uint8_t fin = 0; bool fit = false, stamp = false; const PartRec* part = nullptr; };

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
  if (const BodyRec* b = bodyOf(r.bodyKey))
    for (const AnchorRec& a : b->anchors[facing & 3]) if (a.id < ANCHOR_COUNT) out[a.id] = {a.x, a.y, true};
}

void moveAnchor(Anchor a, const FrameRec& f, Pt& p) {
  if (a == Anchor::Pivot) return;
  p.y += f.bob;
  if (a == Anchor::Neck || a == Anchor::Head || a == Anchor::ShoulderR || a == Anchor::ShoulderL) p.x += f.lean;
  if (a == Anchor::Hip) { p.x += f.hipX; p.y += f.hipY; }
  if (a == Anchor::Head) { p.x += f.headX; p.y += f.headY; }
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

thread_local std::vector<Item> g_items;   // the last composed frame's items (frameInfo reads them)
// (P2c) the probe's per-pixel planes (probeFrame sets them for one compose; null otherwise)
thread_local std::vector<int16_t>* g_probeOwner = nullptr;
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
  const uint16_t forced = forceKey ? P.strId(forceKey) : (uint16_t)0xFFFF;
  const std::vector<int>& list = onlyParts ? *onlyParts : r.parts;
  for (size_t li = 0; li < list.size(); li++) {
    const int pi = list[li];
    if (pi < 0 || pi >= (int)P.parts.size()) continue;
    const PartRec& p = P.parts[(size_t)pi];
    if (keep && !keep(p)) continue;
    if (partTaggedId(p, I.tile)) {   // a pattern tile: laid over the garments' cloth, never placed itself
      const ViewRec* tv = viewOf(p, facing, I.rest);
      if (!tv) tv = viewOf(p, 0, I.rest);
      if (const uint32_t m = regionTileMask(P, p)) {   // (P2c) a region tile
        RegionTile rt;
        rt.v = tv; rt.mask = m; rt.fin = finishTileBit(P, p); rt.part = &p;
        rt.fit = partTagged(pi, "fit"); rt.stamp = partTagged(pi, "stamp");
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
    if (g_breeze && key == I.rest && !forceKey) {
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
    if (!v) continue;
    Item it;
    it.part = &p; it.view = v; it.z = p.z; it.order = pi;
    it.layer = v->layer != kNone8 ? v->layer : p.layer;
    if (v->layer == kNone8 && p.side && (it.layer == (int)Layer::ArmNear || it.layer == (int)Layer::ArmFar))
      it.layer = (int)((p.side == 1 && facing == 3) || (p.side == 2 && facing == 2) ? Layer::ArmFar : Layer::ArmNear);
    // (P2c) a weapon placed by angle lies on held (front) or heldBack (back) unless its view names a layer
    if (heldPart && f.weapon != -32768 && v->layer == kNone8) it.layer = (int)(f.front ? Layer::Held : Layer::HeldBack);
    it.anchor = (Anchor)(v->anchor != kNone8 ? v->anchor : p.anchor);
    // a turned frame's screen items (drawn level on the ground after the turn) are the views that NAME `anchor pivot`
    // (the drops, the shield's lie); a part anchored at the pivot by default (the legs, the sword) turns with the body
    // (polish i1: the legs' `lie` stood upright out of every corpse, "a boot block sticking up")
    const bool screenItem = v->anchor != kNone8 && (Anchor)v->anchor == Anchor::Pivot;
    if (g_screenMode && (screenItem != (g_screenMode == 2))) continue;
    it.garment = (it.layer == (int)Layer::Torso || it.layer == (int)Layer::HangBack || it.layer == (int)Layer::HangFront) && partIsGarment(p);
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
  for (Item& it : items) if (it.anchor != Anchor::Grip && it.anchor != Anchor::GripL) place(it);
  auto markPt = [&](uint16_t id) { for (int i = 0; i < o.nMarks; i++) if (o.markIds[i] == id && o.marks[i].on) return o.marks[i]; return Pt{}; };
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
  auto paint = [&](const Item& it, bool handOnly) {
    const ViewRec& v = *it.view;
    const uint8_t heldMark = isHeld(*it.part) ? 1 : 0;
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
    const bool toBlade = r.blade && libraryHeld(P, *it.part);
    // (P2c ladder) LADDER-FINISH's placements on this view: the P box (fit tiles) and the W / G blobs (stamp tiles)
    struct Place { const ViewRec* v = nullptr; int ox = 0, oy = 0; };
    static thread_local std::vector<Place> fitAt;
    static thread_local std::vector<int> blob;
    struct Blob { int x0, y0, x1, y1, n, metal; uint8_t rg; };
    static thread_local std::vector<Blob> blobs;
    fitAt.assign(rtiles.size(), Place{});
    blobs.clear();
    bool anyStamp = false;
    if (hasReg && ts >= 0 && r.finTiles[ts]) {
      int px0 = v.w, py0 = v.h, px1 = -1, py1 = -1;
      for (int y = 0; y < v.h; y++)
        for (int x = 0; x < v.w; x++)
          if (v.cells[(size_t)y * v.w + x] >= 2 && v.regions[(size_t)y * v.w + x] == (uint8_t)Region::Panel) {
            px0 = std::min(px0, x); py0 = std::min(py0, y); px1 = std::max(px1, x); py1 = std::max(py1, y);
          }
      for (size_t k = 0; k < rtiles.size(); k++) {
        const RegionTile& rt = rtiles[k];
        if (!rt.fin || !(r.finTiles[ts] & rt.fin)) continue;
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
        const uint8_t rg = c >= 2 && hasReg ? v.regions[(size_t)y * v.w + x] : 0;
        if (handOnly && (c < 2 || rg != (uint8_t)Region::Hand)) continue;
        if (rg && rg < REGION_COUNT && r.region[rg] != 0xFF) c = code((Slot)r.region[rg], codeStep(c));
        // (P2c ladder) R P G N W by the piece's own tier (the outline ring keeps its slot)
        if (rg >= (uint8_t)Region::Trim && rg < REGION_COUNT && ts >= 0 && c >= 2 && codeStep(c) >= 1) {
          const uint8_t fs = r.fin[ts][rg - (uint8_t)Region::Trim];
          if (fs != 0xFF) c = code((Slot)fs, codeStep(c));
        }
        if (toBlade && c >= 2 && codeSlot(c) == Slot::Metal) c = code(Slot::Blade, codeStep(c));
        // (P2c) a region tile over this region's pixels (outlines kept): ink keeps the slot; step 2 keeps the step, 1 / 3 move
        // it one down / up, 0 and 4 set it
        // (ladder) tiles stack: the last-listed finish tile first (glyph over filigree); a clear cell falls through to the next
        if (rg && !rtiles.empty() && codeStep(c) >= 1)
          for (size_t ri = rtiles.size(); ri-- > 0;) {
            const RegionTile& rt = rtiles[ri];
            if (!(rt.mask >> rg & 1)) continue;
            if (rt.fin && !(ts >= 0 && (r.finTiles[ts] & rt.fin))) continue;   // (ladder) a finish tile on its tier's pieces only
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
            if (tc < 2) { if (rt.fin) continue; break; }
            const Slot tsl = codeSlot(tc);
            const int tk = codeStep(tc), ck = codeStep(c);
            int nk;
            if (tsl == Slot::Ink && rt.fin) nk = std::clamp(ck + tk - 2, rt.stamp ? 0 : 1, 4);   // (ladder) ink moves the step by k - 2
            else if (rt.fin && (rt.stamp || tsl == Slot::Glow)) nk = tk;                          // stamps and light write their step
            else nk = tk == 2 ? ck : tk == 1 ? std::max(1, ck - 1) : tk == 3 ? std::min(4, ck + 1) : tk;
            c = code(tsl == Slot::Ink ? codeSlot(c) : tsl, nk);
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
              if (heldPx[(size_t)Y * CW + X] && !heldMark && helmErase) continue;
              o.code[(size_t)Y * CW + X] = 0;
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
            heldPx[(size_t)Y * CW + X] = heldMark;
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
RigSheet compose(const DollRecipe& r0, RigPart part, const ComposeOpts* opts) {
  const ComposeOpts op = opts ? *opts : ComposeOpts{};
  const Pool& P = get();
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
      if (rr.cat == (uint8_t)Role::Held && x.held.empty()) x.held = P.s(rr.value);
    }
    out.items.push_back(x);
  }
  const Pt& hd = fo.anchors[(int)Anchor::Head];
  out.headX = CPX + hd.x; out.headY = CPY + hd.y;
  const Ids& I = P.ids;
  for (int k = 0; k < fo.nMarks; k++) {
    if (!fo.marks[k].on) continue;
    const int X = CPX + fo.marks[k].x, Y = CPY + fo.marks[k].y;
    if (fo.markIds[k] == I.grip) { out.grip = true; out.gripX = X; out.gripY = Y; }
    else if (fo.markIds[k] == I.gripL) { out.gripL = true; out.gripLX = X; out.gripLY = Y; }
    else if (fo.markIds[k] == I.tip) { out.tip = true; out.tipX = X; out.tipY = Y; }
  }
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
