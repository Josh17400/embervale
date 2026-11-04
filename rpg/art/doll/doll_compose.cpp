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
#include <unordered_map>
#include <map>
#include <cmath>
#include <cstring>
#include "rpg/art/art_holders2.h"
#include "rpg/art/doll/doll_envelope.h"
#include "rpg/art/doll/doll_forge.h"
#include "rpg/art/doll/doll_internal.h"
#include "rpg/art/doll/doll_compose_internal.h"

namespace art {
namespace doll {

namespace {

using namespace pool;
using namespace comp;

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

}  // namespace

namespace comp {

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
// (forge round 2, D41) the falls and the lying figures keep their pauldrons on the arm layer (a fall's arms fly past the
// cloak, and a lifted shoulder closed 1-2 px pockets against the helm: C3a)
thread_local bool g_deathPart = false;
// (g_bodyPiece, g_depthPx and g_heldPx, the paint's planes the passes read, are defined beside the passes: doll_passes.cpp)
thread_local std::vector<uint8_t> g_fpUnder;   // (P5)
thread_local std::vector<uint8_t> g_fpCuffOwn;   // (P5 fix r1)

// (P5 fix r1, F2: the steel gloves and boots) a glove's or boot's cuff is the top row of its worn regions in the VIEW, but a
// sleeve or a tunic's hem painted later covers that row on the figure, so the cuff never shows. Per canvas column, the cuff
// piece's topmost visible pixel (the row right under the cover) takes the band where the column shows none; a gauntlet (a
// Flare in its cuff mask: a 3 px steel hand whose top row is glint and outline) its top two visible rows. The drawn step is
// kept (1-3; an outline and a glint keep theirs); the code under is filed in g_fpUnder (the passes read the pre-P5 figure)
void fpVisibleCuffs(const DollRecipe& r, FrameOut& o) {
  if (!(r.fpCuff & r.fpSlots) || o.bx1 < o.bx0) return;
  for (int ts = 0; ts < 8; ts++) {
    if (!((r.fpCuff & r.fpSlots) >> ts & 1)) continue;
    const uint8_t own = (uint8_t)(ts + 1);
    const int period = std::max<int>(1, r.dyePeriod[ts]);
    const int rows = (r.fpWide >> ts & 1) ? 2 : 1;
    for (int x = o.bx0; x <= o.bx1; x++) {
      int top = -1;
      for (int y = o.by0; y <= o.by1 && top < 0; y++)
        if (g_fpCuffOwn[(size_t)y * CW + x] == own && o.code[(size_t)y * CW + x] >= 2) top = y;
      if (top < 0) continue;
      bool dyed = false;
      for (int y = top; y < top + rows && y <= o.by1; y++) {
        const Slot cs = o.code[(size_t)y * CW + x] >= 2 ? codeSlot(o.code[(size_t)y * CW + x]) : Slot::Skin;
        if (cs == dyeSlotOf(ts, false) || cs == dyeSlotOf(ts, true)) dyed = true;
      }
      if (dyed && rows == 1) continue;
      for (int y = top; y < top + rows && y <= o.by1; y++) {
        const size_t q = (size_t)y * CW + x;
        const uint8_t c = o.code[q];
        if (g_fpCuffOwn[q] != own || c < 2) continue;
        const int st = codeStep(c);
        const Slot cs = codeSlot(c);
        if (st < 1 || st > 3) continue;
        if (!(cs == Slot::Leather || cs == Slot::Metal || cs == Slot::Metal2 || cs == Slot::Cloth1 || cs == Slot::Cloth2 || cs == Slot::Cloth3 ||
              cs == Slot::Linen || cs == Slot::Accent)) continue;
        int ph = (x + y) % period;
        if (ph < 0) ph += period;
        const bool second = ph >= r.dyeRun[ts];
        o.code[q] = code(dyeSlotOf(ts, second), r.fpFlat ? (second ? 2 : std::min(st, 2)) : st);
        if (!g_fpUnder[q]) g_fpUnder[q] = c;
      }
    }
  }
}

}  // namespace comp

namespace {

// composes one frame into o (raw: no finishing pass). `facing` is the frame's own (a turned frame passes the left view):
// FramePaint gathers, places and paints the items back to front, then the post passes run in their one order
void composeFrame(const DollRecipe& r, int facing, const FrameRec& f, FrameOut& o, const std::vector<int>* onlyParts = nullptr,
                  const char* forceKey = nullptr, bool (*keep)(const PartRec&) = nullptr) {
  static thread_local FramePaint paint;
  const bool fp = r.fpSlots != 0;
  if (fp) { g_fpUnder.assign((size_t)CW * CH, 0); g_fpCuffOwn.assign((size_t)CW * CH, 0); }
  paint.anchors(r, facing, f, o);
  paint.gatherItems(onlyParts, forceKey, keep);
  paint.sortItems();
  paint.placeAnchors();
  paint.paintBackToFront();
  if (!fp || o.bx1 < o.bx0) { postPasses(r, o); return; }
  fpVisibleCuffs(r, o);   // (P5 fix r1) a cuff the sleeve or the hem covers shows right under it
  // (P5) the passes run on the figure as it was before the fingerprint; each fingerprinted pixel a pass left alone takes its
  // dye or mark back (a pass that rewrote it, a glint or a speck, keeps the pass's pixel)
  static thread_local std::vector<uint8_t> keepFp;
  keepFp.assign((size_t)CW * CH, 0);
  for (int y = o.by0; y <= o.by1; y++)
    for (int x = o.bx0; x <= o.bx1; x++) {
      const size_t q = (size_t)y * CW + x;
      if (g_fpUnder[q] && o.code[q] >= 2) { keepFp[q] = o.code[q]; o.code[q] = g_fpUnder[q]; }
    }
  postPasses(r, o);
  static thread_local std::vector<uint8_t> preFin;   // (fix r1) the pre-P5 figure's finished pixel under each fingerprinted one
  preFin.assign((size_t)CW * CH, 0);
  for (int y = o.by0; y <= o.by1; y++)
    for (int x = o.bx0; x <= o.bx1; x++) {
      const size_t q = (size_t)y * CW + x;
      if (!keepFp[q]) continue;
      preFin[q] = o.code[q];
      if (o.code[q] == g_fpUnder[q]) { o.code[q] = keepFp[q]; continue; }
      // (fix r1) a pass that re-shaded a dyed pixel in its own material (the same slot, a step 1-3: a contact shadow, a lit
      // edge) keeps the dye at the pass's step; a glint, a speck or an outline the pass wrote keeps the pass's pixel
      const uint8_t c = o.code[q];
      const Slot ks = codeSlot(keepFp[q]);
      if (c >= 2 && isDyeSlot(ks) && codeSlot(c) == codeSlot(g_fpUnder[q]) && codeStep(c) >= 1 && codeStep(c) <= 3) {
        const bool second = ks >= Slot::Dye2T0;
        o.code[q] = code(ks, r.fpFlat ? (second ? 2 : std::min(codeStep(c), 2)) : codeStep(c));
      }
    }
  // (fix r1, the judge's R1 read: "the dye lands as speckle, not bands"; STYLE_P2 27.3, FIX_BATCH_P5 3.4: "shapes, never a lone
  // dot") a fingerprinted pixel the figure shows changed with no changed 4-neighbour (a thinned lace, a stitch on a diagonal
  // edge, a mark clipped to one cell, a lit run's last unlit pixel) goes back to the pre-P5 pixel: every dye and mark pixel
  // the figure keeps touches another
  auto changed = [&](int x, int y) {
    if (x < o.bx0 || x > o.bx1 || y < o.by0 || y > o.by1) return false;
    const size_t q = (size_t)y * CW + x;
    return keepFp[q] && o.code[q] != preFin[q];
  };
  // (fix r1, C19 A8: "a_thorn: 2 lone 1 px lit pixels" by a cuff) a lit metal pixel (step 3) the fingerprint left must keep a
  // lit neighbour if it had one: where its lit neighbours were all dyed, they go back (until none is left alone)
  auto litAt = [&](int x, int y, bool pre) {
    if (x < o.bx0 || x > o.bx1 || y < o.by0 || y > o.by1) return false;
    const size_t q = (size_t)y * CW + x;
    const uint8_t c = pre && keepFp[q] ? preFin[q] : o.code[q];
    const Slot s = c >= 2 ? codeSlot(c) : Slot::Skin;
    return c >= 2 && codeStep(c) >= 3 && (s == Slot::Metal || s == Slot::Metal2 || s == Slot::Blade);
  };
  const int nb4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  for (bool again = true; again;) {
    again = false;
    for (int y = o.by0; y <= o.by1; y++)
      for (int x = o.bx0; x <= o.bx1; x++) {
        const size_t q = (size_t)y * CW + x;
        if (changed(x, y) || !litAt(x, y, false) || codeStep(o.code[q]) != 3) continue;
        bool litNow = false, litBefore = false;
        for (const auto& d : nb4) { litNow = litNow || litAt(x + d[0], y + d[1], false); litBefore = litBefore || litAt(x + d[0], y + d[1], true); }
        if (litNow || !litBefore) continue;
        for (const auto& d : nb4) {
          const int nx = x + d[0], ny = y + d[1];
          if (!changed(nx, ny) || !litAt(nx, ny, true)) continue;
          const size_t n = (size_t)ny * CW + nx;
          o.code[n] = preFin[n];
          keepFp[n] = 0;
          again = true;
        }
      }
  }
  for (int y = o.by0; y <= o.by1; y++)
    for (int x = o.bx0; x <= o.bx1; x++) {
      const size_t q = (size_t)y * CW + x;
      if (!changed(x, y) || changed(x - 1, y) || changed(x + 1, y) || changed(x, y - 1) || changed(x, y + 1)) continue;
      o.code[q] = preFin[q];
      keepFp[q] = 0;
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
    default: return isDyeSlot(s);   // (P5) the dye band greys as a material
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

// (wave-2 fix round 4, D93) C12b's measure on the idle frame 0 of a facing: per tier slot the gear pixels (armour, helm,
// shield, pauldron, held, sheath, cloak, overlay parts) and those of them in the trim region drawn in Metal 2, after the
// finishing pass (tools/tests/test_doll_ladder.cpp richness reads the same)
TrimShare trimShares(const DollRecipe& r0, int facing) {
  TrimShare m;
  const Pool& P = get();
  DollRecipe r;
  uint8_t turn = 0;
  const FrameRec* fp = pickFrame(r0, RigPart::Loco, facing, Clip::Idle, 0, 0, r, turn);
  if (!fp) return m;
  static thread_local FrameOut fo;
  static thread_local std::vector<int16_t> owner;
  static thread_local std::vector<uint8_t> region;
  owner.assign((size_t)CW * CH, -1);
  region.assign((size_t)CW * CH, 0);
  std::vector<int16_t>* so = g_probeOwner;
  std::vector<uint8_t>* sr = g_probeRegion;
  g_probeOwner = &owner; g_probeRegion = &region;
  composeFrame(r, turn ? 3 : facing, *fp, fo);
  g_probeOwner = so; g_probeRegion = sr;
  std::vector<uint8_t> code = fo.code;
  if (fo.bx1 >= fo.bx0) finishPass(&code[(size_t)fo.by0 * CW + fo.bx0], fo.bx1 - fo.bx0 + 1, fo.by1 - fo.by0 + 1, CW);
  for (size_t k = 0; k < code.size(); k++) {
    if (code[k] < 2 || owner[k] < 0 || (size_t)owner[k] >= g_items.size()) continue;
    const Item& it = g_items[(size_t)owner[k]];
    bool gear = false;
    for (const RoleRec& rr : it.part->roles) {
      const Role ro = (Role)rr.cat;
      gear = gear || ro == Role::Armour || ro == Role::Helm || ro == Role::Shield || ro == Role::Pauldron || ro == Role::Held || ro == Role::Sheath ||
             ro == Role::Cloak || ro == Role::Overlay;
    }
    const int ts = partTierSlot((int)(it.part - P.parts.data()));
    if (!gear || ts < 0 || ts >= 8) continue;
    m.px[ts]++;
    if (region[k] == (uint8_t)Region::Trim && codeSlot(code[k]) == Slot::Metal2) m.trim[ts]++;
    else if (region[k] == (uint8_t)Region::Trim) m.glow[ts]++;
    else if (region[k] == (uint8_t)Region::Seam) m.seam[ts]++;
    else if (region[k] == (uint8_t)Region::Gem) m.gem[ts]++;
    else if (codeStep(code[k]) == 0) m.outline[ts]++;
  }
  // (fix round 3) EMB_TRIM_PARTS=<slot>: the slot's visible gear pixels and Metal 2 trim per part (who owns an over-ceiling share)
  if (const char* ps = std::getenv("EMB_TRIM_PARTS")) {
    const int want = std::atoi(ps);
    std::map<std::string, std::pair<int, int>> per;
    for (size_t k = 0; k < code.size(); k++) {
      if (code[k] < 2 || owner[k] < 0 || (size_t)owner[k] >= g_items.size()) continue;
      const Item& it = g_items[(size_t)owner[k]];
      if (partTierSlot((int)(it.part - P.parts.data())) != want) continue;
      auto& e = per[P.s(it.part->name)];
      e.first++;
      if (region[k] == (uint8_t)Region::Trim && codeSlot(code[k]) == Slot::Metal2) e.second++;
    }
    std::printf("    trim parts facing %d slot %d:", facing, want);
    for (const auto& kv : per) std::printf(" %s %d/%d", kv.first.c_str(), kv.second.second, kv.second.first);
    std::printf("\n");
  }
  if (const char* mp = std::getenv("EMB_TRIM_MAP")) {
    const int want = std::atoi(mp);
    std::printf("    trim map facing %d slot %d:\n", facing, want);
    for (int y = fo.by0; y <= fo.by1; y++) {
      std::string line = "      ";
      for (int x = fo.bx0; x <= fo.bx1; x++) {
        const size_t k = (size_t)y * CW + x;
        char ch = code[k] >= 2 ? ',' : ' ';
        if (code[k] >= 2 && owner[k] >= 0 && (size_t)owner[k] < g_items.size() && partTierSlot((int)(g_items[(size_t)owner[k]].part - P.parts.data())) == want)
          ch = region[k] == (uint8_t)Region::Trim ? (codeSlot(code[k]) == Slot::Metal2 ? 'R' : 'r') : codeStep(code[k]) == 0 ? 'o' : (char)('a' + std::min(25, (int)region[k]));
        line += ch;
      }
      std::printf("%s\n", line.c_str());
    }
  }
  return m;
}
// the tiers' trim bands (C12b, STYLE_P2 27.2; D78: the bands stand): T3 3-14 %, T4 6-20, T5-T6 10-24; a helm's and a
// shield's ceiling 36 %, the cloak's 30 %
// (D93) the levels depend on what is drawn where (parts, flags, regions, tiers), never on the colours: looks that differ only
// in their ramps share them (a census of one outfit in many dyes measures once). Every compose input but the ramps is hashed
static uint64_t trimKey(const DollRecipe& r) {
  uint64_t h = 0xCBF29CE484222325ull;
  auto mix = [&](const void* p, size_t n) { const uint8_t* b = (const uint8_t*)p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001B3ull; } };
  auto mixS = [&](const std::string& t) { mix(t.data(), t.size()); const uint8_t z = 0; mix(&z, 1); };
  mix(&r.body, sizeof r.body); mixS(r.bodyKey); mix(&r.elf, 1); mix(&r.build, sizeof r.build); mix(&r.family, 1);
  for (int p : r.parts) mix(&p, sizeof p);
  for (const std::string& f : r.flags) mixS(f);
  mix(r.region, sizeof r.region); mix(r.fin, sizeof r.fin); mix(r.finTiles, sizeof r.finTiles); mix(r.pieceTier, sizeof r.pieceTier); mix(&r.rare, 1);
  mix(&r.blade, 1); mix(&r.guiseHead, 1); mixS(r.motif); mix(r.forgeAx, sizeof r.forgeAx); mix(&r.armourFam, sizeof r.armourFam); mix(&r.sheen, 1);
  mix(r.shape, sizeof r.shape); mix(&r.helmVariant, 1); mix(&r.pauldronTop, 1); mix(&r.kitCloak, 1); mix(&r.pauldronTopAlloy, 1);
  mix(&r.layerCloakT3, 1); mix(&r.irisBands, 1); mix(&r.dropHang, 1);
  const uint32_t fx = fixRules();
  mix(&fx, sizeof fx);
  return h;
}
static void tuneTrimUncached(DollRecipe& r);
void tuneTrim(DollRecipe& r) {
  for (int g = 0; g < 2; g++)
    for (int s = 0; s < 8; s++) { r.trimLevel[g][s] = 0; r.trimCeil[g][s] = 0; }
  if (!fixOn(FIX_TRIM4)) return;
  bool any = false;
  for (int s : {0, 1, 2, 4, 5, 6}) any = any || r.pieceTier[s] >= 3;
  if (!any) return;
  struct L8 { int8_t f[2][8], c[2][8]; };
  static thread_local std::unordered_map<uint64_t, L8> cache;
  const uint64_t key = trimKey(r);
  auto it = cache.find(key);
  if (it != cache.end() && !std::getenv("EMB_TRIM_DEBUG")) {
    std::memcpy(r.trimLevel, it->second.f, sizeof r.trimLevel);
    std::memcpy(r.trimCeil, it->second.c, sizeof r.trimCeil);
    return;
  }
  tuneTrimUncached(r);
  if (cache.size() > 20000) cache.clear();
  L8 v;
  std::memcpy(v.f, r.trimLevel, sizeof r.trimLevel);
  std::memcpy(v.c, r.trimCeil, sizeof r.trimCeil);
  cache[key] = v;
}
static void tuneTrimUncached(DollRecipe& r) {
  static const double kLo[7] = {0, 0, 0, 0.03, 0.06, 0.10, 0.10}, kHi[7] = {0, 0, 0.03, 0.14, 0.20, 0.24, 0.24};
  static const int kSlots[6] = {0, 1, 2, 4, 5, 6};
  int tt[8] = {};
  bool any = false;
  for (int s : kSlots) {
    if (r.pieceTier[s] < 3) continue;
    tt[s] = std::min(6, r.pieceTier[s] + ((r.rare >> s & 1) ? 1 : 0));
    any = true;
  }
  if (!any) return;
  auto lo = [&](int s) { return kLo[tt[s]]; };
  auto hi = [&](int s) { double h = kHi[tt[s]]; if (s == 1 || s == 2) h = std::max(h, 0.36); if (s == 6) h = std::max(h, 0.30); return h; };
  // (fix round 2) per facing pair g (0: the idle D, 1: the idle R, the frames C12b reads) and slot: the share as C12b reads it;
  // a facing showing under 20 px of the piece is not read, nor one where the cloak shows no trim (drawn in its own colours there)
  struct Sh { double v = 0; bool seen = false; };
  auto measure = [&](Sh out[2][8]) {
    for (int g = 0; g < 2; g++) {
      const TrimShare m = trimShares(r, g ? 2 : 0);
      for (int s = 0; s < 8; s++) {
        out[g][s] = Sh{};
        if (m.px[s] < 20 || (s == 6 && !m.trim[s])) continue;
        out[g][s].v = (double)m.trim[s] / m.px[s];
        out[g][s].seen = true;
      }
    }
  };
  const bool dbg = std::getenv("EMB_TRIM_DEBUG") != nullptr;
  Sh cur[2][8];
  measure(cur);
  // each (facing pair, slot) outside its band walks its own list of (edges, ceiling) levels, all of them a step at a time (their
  // pixels are their own: a slot's pieces, a facing's views); the first level at its target (the floor + 2 points under, the
  // ceiling - 2 over) is kept, else the first inside the band, else the one nearest it (never one farther than D78's band alone)
  struct Cand { int8_t f, c; };
  // the floor walks the edges in order, each first at 1 px (c 1), then at the tier's width
  // (fix round 3) then on past the four edges into the plate rims, the lowest lame first (floor 5 .. 10: 1 .. 6 lines)
  static const Cand kFloor[] = {{1, 1}, {1, 0}, {2, 1}, {2, 0}, {3, 1}, {3, 0}, {4, 1}, {4, 0}, {5, 1}, {5, 0}, {6, 1}, {6, 0},
                                {7, 1}, {7, 0}, {8, 1}, {8, 0}, {9, 1}, {9, 0}, {10, 1}, {10, 0}};
  constexpr int kNFloor = (int)(sizeof kFloor / sizeof kFloor[0]);
  // the ceiling at T4-T6: the band's added rows 2 px to 1, then off; then on into the edges at 1 px where narrowing the band
  // sinks the piece under its floor
  static const Cand kCeil[] = {{0, 1}, {0, 2}, {1, 1}, {2, 1}, {1, 2}, {2, 2}, {3, 1}, {3, 2}, {4, 1}, {4, 2}};
  static const Cand kCeil3[] = {{0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}, {0, 7}, {0, 8}, {0, 9}, {0, 10}, {0, 11}, {0, 12}, {0, 13}, {0, 14}, {0, 15}, {0, 16}};
  const Cand* list[2][8] = {};
  int nList[2][8] = {}, kind[2][8] = {};   // kind 1 floor, -1 ceiling
  double bestV[2][8] = {};
  Cand best[2][8] = {};
  int bestRank[2][8] = {};
  auto viol = [&](int s, const Sh& x) { return x.seen ? std::max(0.0, lo(s) - x.v) + std::max(0.0, x.v - hi(s)) : 0.0; };
  bool anyKind = false;
  for (int g = 0; g < 2; g++)
    for (int s : kSlots) {
      bestRank[g][s] = 9;
      if (!tt[s] || !cur[g][s].seen) continue;
      if (dbg) std::printf("    tuneTrim %c slot %d T%d level 0: %.1f %% (band %.0f-%.0f)\n", "DR"[g], s, tt[s], cur[g][s].v * 100, lo(s) * 100, hi(s) * 100);
      const bool under = cur[g][s].v < lo(s) - 1e-9, over = cur[g][s].v > hi(s) + 1e-9;
      if (!under && !over) continue;
      bestV[g][s] = viol(s, cur[g][s]);
      bestRank[g][s] = 2;
      if (tt[s] >= 4 && under) { kind[g][s] = 1; list[g][s] = kFloor; nList[g][s] = kNFloor; }
      else if (over) { kind[g][s] = -1; list[g][s] = tt[s] >= 4 ? kCeil : kCeil3; nList[g][s] = tt[s] >= 4 ? 10 : 16; }
      anyKind = anyKind || kind[g][s] != 0;
    }
  if (!anyKind) return;
  bool done[2][8] = {};
  for (int step = 0; step < kNFloor; step++) {
    bool act = false;
    for (int g = 0; g < 2; g++)
      for (int s : kSlots) {
        if (!kind[g][s] || done[g][s]) continue;
        // (fix round 3) the plate rims (floor 5 on) only where the four edges never reach the band: a slot they bring inside
        // keeps their level (the signed-off lines' tuned pieces as fix round 2 laid them)
        if (step >= nList[g][s] || (kind[g][s] == 1 && step == 8 && bestRank[g][s] <= 1)) {
          done[g][s] = true; r.trimLevel[g][s] = best[g][s].f; r.trimCeil[g][s] = best[g][s].c; continue;
        }
        r.trimLevel[g][s] = list[g][s][step].f;
        r.trimCeil[g][s] = list[g][s][step].c;
        act = true;
      }
    if (!act) break;
    Sh nw[2][8];
    measure(nw);
    for (int g = 0; g < 2; g++)
      for (int s : kSlots) {
        if (!kind[g][s] || done[g][s]) continue;
        const double v = viol(s, nw[g][s]);
        const bool in = v < 1e-9;
        const bool atTarget = in && nw[g][s].seen && (kind[g][s] == 1 ? nw[g][s].v >= lo(s) + 0.02 - 1e-9 : nw[g][s].v <= hi(s) - 0.02 + 1e-9);
        const int rank = atTarget ? 0 : in ? 1 : 2;
        if (dbg)
          std::printf("    tuneTrim step %d %c slot %d T%d floor %d cut %d: %.1f %%%s\n", step, "DR"[g], s, tt[s], r.trimLevel[g][s], r.trimCeil[g][s], nw[g][s].v * 100,
                      rank == 0 ? " (target)" : rank == 1 ? " (in band)" : "");
        if (rank < bestRank[g][s] || (rank == 2 && bestRank[g][s] == 2 && v < bestV[g][s] - 1e-9)) { bestRank[g][s] = rank; bestV[g][s] = v; best[g][s] = list[g][s][step]; }
        if (rank == 0) done[g][s] = true;
      }
  }
  for (int g = 0; g < 2; g++)
    for (int s : kSlots) {
      if (!kind[g][s]) continue;
      r.trimLevel[g][s] = best[g][s].f;
      r.trimCeil[g][s] = best[g][s].c;
    }
  // (fix round 2) EMB_TRIM_STUCK: a (facing, slot) the rule leaves outside its band, with what its visible pixels are made of
  if (std::getenv("EMB_TRIM_STUCK"))
    for (int g = 0; g < 2; g++) {
      bool stuck = false;
      for (int s : kSlots) stuck = stuck || (kind[g][s] && bestRank[g][s] == 2);
      if (!stuck) continue;
      const TrimShare m = trimShares(r, g ? 2 : 0);
      for (int s : kSlots) {
        if (!kind[g][s] || bestRank[g][s] != 2) continue;
        std::printf("    TRIMSTUCK %s %c slot %d T%d %s: level 0 %.1f %% -> %d of %d (%.1f %%) at floor %d cut %d; visible seam %d gem %d outline %d glow %d plain %d\n",
                    r.bodyKey.c_str(), "DR"[g], s, tt[s], kind[g][s] > 0 ? "under" : "over", cur[g][s].v * 100, m.trim[s], m.px[s], m.px[s] ? 100.0 * m.trim[s] / m.px[s] : 0.0,
                    best[g][s].f, best[g][s].c, m.seam[s], m.gem[s], m.outline[s], m.glow[s], m.px[s] - m.trim[s] - m.seam[s] - m.gem[s] - m.outline[s] - m.glow[s]);
      }
    }
}

// (fix round 2, C14) the materials touching on the idle D and R and the lit top-left edge, as C14 reads them (the same probe)
void contactRead(const DollRecipe& r, ContactRead& out) {
  std::memset(out.touch, 0, sizeof out.touch);
  for (int g = 0; g < 2; g++) {
    out.edge[g].clear();
    const FrameProbe f = probeFrame(r, RigPart::Loco, g ? 2 : 0, Clip::Idle, 0, 0, true);
    if (!f.ok) continue;
    auto at = [&](int x, int y) -> uint8_t { return x < 0 || y < 0 || x >= f.w || y >= f.h ? 0 : f.code[(size_t)y * f.w + x]; };
    std::vector<int> rowMin((size_t)f.h, -1), colMin((size_t)f.w, -1);
    for (int y = 0; y < f.h; y++)
      for (int x = 0; x < f.w; x++) {
        const uint8_t a = at(x, y);
        if (a < 2 || codeStep(a) == 0) continue;
        if (codeSlot(a) != Slot::Ink) {
          if (rowMin[(size_t)y] < 0) rowMin[(size_t)y] = x;
          if (colMin[(size_t)x] < 0) colMin[(size_t)x] = y;
        }
        for (int d = 0; d < 2; d++) {
          const uint8_t b = at(x + (d == 0), y + (d == 1));
          if (b < 2 || codeStep(b) == 0 || codeSlot(a) == codeSlot(b)) continue;
          uint16_t& c = out.touch[(int)codeSlot(a)][(int)codeSlot(b)];
          if (c < 0xFFFF) c++;
        }
      }
    std::vector<uint8_t> seen((size_t)f.w * f.h, 0);
    auto add = [&](int x, int y) { const size_t k = (size_t)y * f.w + x; if (!seen[k]) { seen[k] = 1; out.edge[g].push_back(f.code[k]); } };
    for (int y = 0; y < f.h; y++) if (rowMin[(size_t)y] >= 0) add(rowMin[(size_t)y], y);
    for (int x = 0; x < f.w; x++) if (colMin[(size_t)x] >= 0) add(x, colMin[(size_t)x]);
  }
}
void partContacts(DollRecipe& r, const HumanLook& L) {
  // C14 reads the kits (a tiered look: the ladder, the forge, the envelope); an untiered townsfolk look keeps rampsOf's parting
  // and costs no composing here. The read is composed once per drawing (cached by what is drawn, never the colours)
  if (L.topTier() < 1) return;
  static thread_local std::unordered_map<uint64_t, ContactRead> cache;
  const uint64_t key = trimKey(r) ^ 0x7C14C14C14C14C14ull;
  auto it = cache.find(key);
  if (it == cache.end()) {
    ContactRead v;
    contactRead(r, v);
    if (cache.size() > 20000) cache.clear();
    it = cache.emplace(key, std::move(v)).first;
  }
  // (C14's own yardsticks) a dark sheen's, KNAPPED's, FURNACE's and GLASS's lit edge is their own step 1: no lift
  const bool body = L.forge[HumanLook::TIER_BODY] != 0;
  const bool edgeOwn = body && (L.sheen == 3 || L.metalKind == 7 || L.metalKind == 8 || r.has("glass"));
  partByContact(r.ramps, L, it->second, edgeOwn, std::getenv("EMB_C14_DEBUG") != nullptr);
}

// (P5) the recipe's drawing with its fingerprint (trimKey and every fp field: what the fingerprint paints, never the colours)
static uint64_t fpKey(const DollRecipe& r, uint64_t salt) {
  uint64_t key = trimKey(r) ^ salt;
  auto mix = [&](const void* p, size_t n) { const uint8_t* b = (const uint8_t*)p; for (size_t i = 0; i < n; i++) { key ^= b[i]; key *= 0x100000001B3ull; } };
  mix(&r.fpSlots, 1); mix(&r.fpR, 1); mix(&r.fpN, 1); mix(&r.fpCuff, 1); mix(&r.fpHem, 1); mix(&r.fpThin, 1); mix(&r.fpOuter, 1); mix(&r.fpStitch, 1); mix(&r.fpSparse, 1); mix(&r.fpFlat, 1); mix(&r.fpMark, 1); mix(&r.fpLeather, 1); mix(&r.fpWide, 1);
  mix(r.cuffMask, sizeof r.cuffMask); mix(r.dyeRun, sizeof r.dyeRun); mix(r.dyePeriod, sizeof r.dyePeriod);
  for (const std::string& m : r.slotMotif) { mix(m.data(), m.size()); const uint8_t z = 0; mix(&z, 1); }
  return key;
}
// (P5, FIX_BATCH_P5 3.5) the dye's share of each fingerprinted piece (its dye pixels over its pixels, the largest of the four idle
// views) held under its cap: body 15 % at T1-T2 and 20 % at T3-T5, helm 25, shield 30, gloves 40, boots 20, cloak 15. A slot
// over its cap thins, a step at a time: N on every other canvas row, then R on its outer row only (the binding), then R
// stitched (a period of the band, a period as drawn; a cloak's hem too), then sparser (two as drawn), then no N.
// Chosen once per drawing (cached) on the composed idles, so a new piece never needs a hand-tuned rule
// (the mean colour of the body and helm pieces on the idle D: FIX_BATCH_P5 3.5's leather read)
struct FpMean { double c[2][3] = {}; int n[2] = {}; };
void meanHueSat(const FpMean& m, int s, double& h, double& sat) {
  h = sat = 0;
  if (!m.n[s]) return;
  const double r = m.c[s][0] / m.n[s], g = m.c[s][1] / m.n[s], b = m.c[s][2] / m.n[s];
  const double mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
  sat = mx > 0 ? d / mx : 0;
  if (d <= 0) return;
  h = mx == r ? 60.0 * std::fmod((g - b) / d, 6.0) : mx == g ? 60.0 * ((b - r) / d + 2.0) : 60.0 * ((r - g) / d + 4.0);
  if (h < 0) h += 360.0;
}
// (fix r1) the tier slot whose PIECE a probed pixel is, as rpg_test --fingerprint isolates it (FIX_BATCH_P5 2.1: the body's
// armour, pauldrons, tabard, sash, cowl and the sleeves' armour pixels; the helm; the shield; the gloves' Hand and Flare; the
// boots' Boot, Sole and armoured Shin and Knee; the cloak), so the shares tuneFp holds are the shares the gate measures. -1: none
int fpPieceSlot(const FrameProbe::It& it, uint8_t region, uint8_t code) {
  using H = HumanLook;
  const Slot s = code >= 2 ? codeSlot(code) : Slot::Skin;
  const bool armourCode = s == Slot::Metal || s == Slot::Metal2 || s == Slot::Leather || s == Slot::Accent || s == Slot::Linen ||
                          s == Slot::Cloth3 || s == Slot::Plume || s == Slot::Glow || isDyeSlot(s);
  auto has = [&](const char* k) { return it.part.find(k) != std::string::npos; };
  const Region g = (Region)region;
  if (it.has(Role::Armour) || it.has(Role::Pauldron) || (it.has(Role::Overlay) && (has("tabard") || has("sash"))) || (it.has(Role::Neck) && has("cowl")))
    return H::TIER_BODY;
  if (it.has(Role::Helm) || (it.has(Role::Headwear) && has("helm"))) return H::TIER_HELM;
  if (it.has(Role::Shield)) return H::TIER_SHIELD;
  if (it.has(Role::Cloak)) return H::TIER_CLOAK;
  if (it.has(Role::Arms)) {
    if (g == Region::Hand || g == Region::Flare) return H::TIER_GLOVES;
    if ((g == Region::Upper || g == Region::Fore || g == Region::Cuff) && armourCode) return H::TIER_BODY;
  }
  if (it.has(Role::Legs) && (g == Region::Boot || g == Region::Sole || ((g == Region::Shin || g == Region::Knee) && armourCode))) return H::TIER_BOOTS;
  return -1;
}
void fpShares(const DollRecipe& r, int dye[8], int px[8], FpMean* mean = nullptr, int facings = 4, int* colours = nullptr) {
  for (int s = 0; s < 8; s++) dye[s] = px[s] = 0;
  if (colours) *colours = 0;
  for (int f = 0; f < facings; f++) {
    const FrameProbe p = probeFrame(r, RigPart::Loco, f, Clip::Idle, 0, 0, true);
    if (!p.ok) continue;
    if (colours) {   // (B8's read: the frame's distinct colours)
      std::vector<uint32_t> cs;
      for (uint8_t c : p.code) if (c >= 2) cs.push_back(r.ramps.c[(int)codeSlot(c)][codeStep(c)]);
      std::sort(cs.begin(), cs.end());
      *colours = std::max(*colours, (int)(std::unique(cs.begin(), cs.end()) - cs.begin()));
    }
    int d[8] = {}, n[8] = {};
    for (size_t k = 0; k < p.code.size(); k++) {
      if (p.code[k] < 2 || p.owner[k] < 0 || (size_t)p.owner[k] >= p.items.size()) continue;
      const int ts = fpPieceSlot(p.items[(size_t)p.owner[k]], p.region[k], p.code[k]);
      if (ts < 0) continue;
      n[ts]++;
      if (isDyeSlot(codeSlot(p.code[k]))) d[ts]++;
      if (mean && f == 0 && ts <= HumanLook::TIER_HELM) {
        const uint32_t c = r.ramps.c[(int)codeSlot(p.code[k])][codeStep(p.code[k])];
        mean->c[ts][0] += c & 255u; mean->c[ts][1] += c >> 8 & 255u; mean->c[ts][2] += c >> 16 & 255u;
        mean->n[ts]++;
      }
    }
    for (int s = 0; s < 8; s++)
      if (n[s] && (long)d[s] * std::max(1, px[s]) > (long)dye[s] * n[s]) { dye[s] = d[s]; px[s] = n[s]; }
  }
}
int fpCap(const DollRecipe& r, int s) {
  static const int kCap[8] = {15, 25, 30, 100, 40, 20, 15, 100};
  return s == HumanLook::TIER_BODY && r.pieceTier[s] >= 3 ? 20 : kCap[s];
}
void tuneFp(DollRecipe& r) {
  if (!r.fpSlots) return;
  struct T { uint8_t thin, outer, stitch, sparse, n, wide; bool flat; };
  static thread_local std::unordered_map<uint64_t, T> cache;
  const uint64_t key = fpKey(r, 0x7F9A7F9A7F9A7F9Aull);
  auto it = cache.find(key);
  if (it != cache.end()) { r.fpThin = it->second.thin; r.fpOuter = it->second.outer; r.fpStitch = it->second.stitch; r.fpSparse = it->second.sparse; r.fpN = it->second.n; r.fpWide = it->second.wide; r.fpFlat = it->second.flat; return; }
  // the pre-P5 read of the body and the helm (a leather piece keeps its hue in 15-45 and its saturation within 0.10)
  FpMean pre;
  {
    DollRecipe r0 = r;
    r0.fpSlots = 0;
    int d0[8], p0[8];
    fpShares(r0, d0, p0, &pre, 1);
  }
  // (B8: 48 colours a frame) a figure the full band would carry past 44 on its idles draws the dye in two steps, the dye2 in one
  {
    int d1[8], p1[8], cols = 0;
    fpShares(r, d1, p1, nullptr, 4, &cols);
    if (cols > 44) r.fpFlat = true;   // (the walk frames carry a few colours more than the idles)
  }
  for (int round = 0; round < 6; round++) {
    int dye[8], px[8];
    FpMean now;
    fpShares(r, dye, px, &now);
    bool moved = false;
    for (int s = 0; s < 8; s++) {
      const uint8_t b = (uint8_t)(1u << s);
      bool over = px[s] && dye[s] * 100 > fpCap(r, s) * px[s];
      if (s <= HumanLook::TIER_HELM && (r.fpSlots & b) && (r.fpLeather & b)) {
        double h0, s0, h1, s1;
        meanHueSat(pre, s, h0, s0);
        meanHueSat(now, s, h1, s1);
        if (pre.n[s] && h0 >= 15 && h0 <= 45 && s0 >= 0.2 && (h1 < std::min(15.0, h0 - 2) || h1 > std::max(45.0, h0 + 2) || s1 < s0 - 0.09)) over = true;
      }
      if (!(r.fpSlots & b) || !over) continue;
      if (r.fpWide & b) r.fpWide &= (uint8_t)~b;   // (fix r1) a gauntlet's cuff back to one row
      else if ((r.fpN & b) && !(r.fpThin & b)) r.fpThin |= b;
      else if ((r.fpR & b) && !(r.fpOuter & b)) r.fpOuter |= b;
      else if (((r.fpR | r.fpHem) & b) && !(r.fpStitch & b)) r.fpStitch |= b;
      else if (((r.fpR | r.fpHem) & b) && !(r.fpSparse & b)) r.fpSparse |= b;
      else if (r.fpN & b) r.fpN &= (uint8_t)~b;
      else continue;
      moved = true;
    }
    if (!moved) break;
  }
  if (cache.size() > 20000) cache.clear();
  cache[key] = T{r.fpThin, r.fpOuter, r.fpStitch, r.fpSparse, r.fpN, r.fpWide, r.fpFlat};
}

void fpBudget(DollRecipe& r) {
  // B8's own read: every LOCO, RUN and DRAWN frame of the composed sheets, its distinct colours (the dye ramps' colours left out of the palette)
  auto count = [&](std::vector<uint32_t>* palette) {
    std::vector<uint32_t> dye;
    for (int s = (int)Slot::DyeT0; s < SLOT_COUNT; s++) for (int q = 0; q < 5; q++) dye.push_back(r.ramps.c[s][q] | 0xFF000000u);
    std::sort(dye.begin(), dye.end());
    int most = 0;
    std::vector<uint32_t> pal, cs;
    for (RigPart part : {RigPart::Loco, RigPart::Run, RigPart::Drawn}) {
    const RigSheet sh = compose(r, part);
    for (const FrameRect& fr : sh.frames) {
      cs.clear();
      for (int y = 0; y < fr.h; y++)
        for (int x = 0; x < fr.w; x++) {
          const uint32_t p = sh.atlas.get(fr.x + x, fr.y + y);
          if (!(p >> 24)) continue;
          cs.push_back(p);
          if (palette && !std::binary_search(dye.begin(), dye.end(), p | 0xFF000000u)) pal.push_back(p | 0xFF000000u);
        }
      std::sort(cs.begin(), cs.end());
      most = std::max(most, (int)(std::unique(cs.begin(), cs.end()) - cs.begin()));
    }
    }
    if (palette) { std::sort(pal.begin(), pal.end()); pal.erase(std::unique(pal.begin(), pal.end()), pal.end()); *palette = pal; }
    return most;
  };
  // (cached by the drawing and its colours: the dye slots' ramps as the budget left them)
  struct Dyes { uint32_t c[SLOT_COUNT - (int)Slot::DyeT0][5]; };
  static thread_local std::unordered_map<uint64_t, Dyes> cache;
  uint64_t key = fpKey(r, 0xB8B8B8B8B8B8B8B8ull);
  for (int s = 0; s < SLOT_COUNT; s++)
    for (int q = 0; q < 5; q++) { key ^= r.ramps.c[s][q]; key *= 0x100000001B3ull; }
  auto it = cache.find(key);
  if (it == cache.end()) {
    std::vector<uint32_t> pal;
    const int c0 = count(&pal);
    if (c0 > 48) {
      snapDyes(r.ramps, pal, false);
      if (count(nullptr) > 48) snapDyes(r.ramps, pal, true);
    }
    Dyes d;
    for (int s = (int)Slot::DyeT0; s < SLOT_COUNT; s++) for (int q = 0; q < 5; q++) d.c[s - (int)Slot::DyeT0][q] = r.ramps.c[s][q];
    if (cache.size() > 20000) cache.clear();
    it = cache.emplace(key, d).first;
  }
  for (int s = (int)Slot::DyeT0; s < SLOT_COUNT; s++) for (int q = 0; q < 5; q++) r.ramps.c[s][q] = it->second.c[s - (int)Slot::DyeT0][q];
}

void partDyeContacts(DollRecipe& r) {
  static thread_local std::unordered_map<uint64_t, ContactRead> cache;
  const uint64_t key = fpKey(r, 0xD7E0D7E0D7E0D7E0ull);
  auto it = cache.find(key);
  if (it == cache.end()) {
    ContactRead v;
    contactRead(r, v);
    if (cache.size() > 20000) cache.clear();
    it = cache.emplace(key, std::move(v)).first;
  }
  partDyeByContact(r.ramps, it->second);
}

// (D93) the band's selftests on synthetic views: a T6 plate with no R lands in 10-24 % by the floor mode; a T3 piece with every
// edge marked R lands under 14 % by the ceiling mode; a gem and a seam are never touched. Returns the number missed
int trimSelftests(std::vector<std::string>* missed) {
  int miss = 0;
  auto fail = [&](const char* w) { miss++; if (missed) missed->push_back(w); };
  // a 14 x 18 torso: outline ring (step 0), body steps 1-3 in Metal
  auto plate = [](ViewRec& v) {
    v.w = 14; v.h = 18;
    v.cells.assign(14 * 18, 0);
    v.regions.assign(14 * 18, 0);
    for (int y = 0; y < 18; y++)
      for (int x = 0; x < 14; x++) {
        const bool neck = y < 2 && x >= 5 && x <= 8;
        if (neck) continue;
        const bool ring = x == 0 || y == 0 || x == 13 || y == 17 || (y == 2 && x >= 5 && x <= 8) || ((x == 4 || x == 9) && y < 2);
        v.cells[(size_t)y * 14 + x] = code(Slot::Metal, ring ? 0 : 1 + (x + y) % 3);
      }
  };
  auto share = [](const ViewRec& v, const std::vector<uint8_t>& reg, const std::vector<int8_t>& row) {
    int n = 0, t = 0;
    for (size_t k = 0; k < v.cells.size(); k++) {
      if (v.cells[k] < 2) continue;
      n++;
      if (reg[k] == (uint8_t)Region::Trim && row[k] != -2 && codeStep(v.cells[k]) >= 1) t++;
    }
    return n ? (double)t / n : 0.0;
  };
  std::vector<uint8_t> reg;
  std::vector<int8_t> row;
  {
    ViewRec v;
    plate(v);
    double got = -1;
    for (int lv = 1; lv <= 4; lv++) {   // the floor's edges in order at T6's width
      trimBand(v, 2, reg, row, lv, TE_HEM | TE_NECK | TE_ARM, 2, 0);
      const double sh = share(v, reg, row);
      if (sh > 0.24) break;
      got = sh;
      if (sh >= 0.12) break;
    }
    if (!(got >= 0.10 && got <= 0.24)) fail("a T6 plate with no R did not land in 10-24 %");
    // a gem and a seam on the hem are never touched
    ViewRec g = v;
    for (int x = 3; x <= 6; x++) g.regions[(size_t)16 * 14 + x] = (uint8_t)Region::Gem;
    for (int x = 7; x <= 10; x++) g.regions[(size_t)16 * 14 + x] = (uint8_t)Region::Seam;
    trimBand(g, 2, reg, row, 4, TE_HEM | TE_NECK | TE_ARM, 2, 0);
    bool touched = false;
    for (int x = 3; x <= 10; x++) touched |= reg[(size_t)16 * 14 + x] == (uint8_t)Region::Trim;
    if (touched) fail("the floor mode laid trim over a gem or a seam");
  }
  {
    ViewRec v;
    plate(v);   // every edge marked R, in runs (the drawn rim broken at the corners and the neck)
    for (int y = 1; y < 17; y++)
      for (int x = 1; x < 13; x++) {
        const int k = y * 14 + x;
        if (v.cells[(size_t)k] < 2 || codeStep(v.cells[(size_t)k]) == 0) continue;
        if (x == 1 || x == 12 || y == 16 || y == 3) v.regions[(size_t)k] = (uint8_t)Region::Trim;
      }
    double got = 1;
    for (int lv = 1; lv <= 16; lv++) {   // the ceiling's runs at T3, shortest first
      trimBand(v, 0, reg, row, 0, 0, 0, lv);
      got = share(v, reg, row);
      if (got <= 0.12) break;
    }
    if (got >= 0.14) fail("a T3 piece with every edge marked R did not land under 14 %");
  }
  {
    // (fix round 3) the plate rims: a lamellar body (internal step-0 lines at rows 6, 10 and 14, a 2-row seam at 10-11) with
    // its hem drawn R: floor 5 rims the lowest lame only (row 13), floor 6 the next (row 9, over the 2-row seam: one line), a
    // gem on row 13 stays, a lone pixel is never laid, nothing on a line or the outer ring
    ViewRec v;
    plate(v);
    for (int x = 1; x < 13; x++) {
      v.cells[(size_t)6 * 14 + x] = code(Slot::Metal, 0);
      v.cells[(size_t)14 * 14 + x] = code(Slot::Metal, 0);
      v.regions[(size_t)10 * 14 + x] = (uint8_t)Region::Seam;
      v.regions[(size_t)11 * 14 + x] = (uint8_t)Region::Seam;
      v.regions[(size_t)16 * 14 + x] = (uint8_t)Region::Trim;
    }
    v.regions[(size_t)13 * 14 + 3] = (uint8_t)Region::Gem;
    v.regions[(size_t)13 * 14 + 4] = (uint8_t)Region::Gem;
    v.regions[(size_t)13 * 14 + 11] = (uint8_t)Region::Gem;   // (leaves x 12 alone: a 1 px run)
    auto rimRows = [&](int lv) {
      trimBand(v, 2, reg, row, lv, TE_HEM | TE_PLATE, 2, 0);
      std::vector<int> n(18, 0);
      for (int y = 0; y < 18; y++)
        for (int x = 0; x < 14; x++) if (row[(size_t)y * 14 + x] >= 0 && (row[(size_t)y * 14 + x] & kBandRim)) n[(size_t)y]++;
      return n;
    };
    const std::vector<int> a = rimRows(4), b = rimRows(5), c = rimRows(6);
    int sa = 0;
    for (int y = 0; y < 18; y++) sa += a[(size_t)y];
    if (sa) fail("floor 4 laid a plate rim");
    if (b[13] != 8 || b[9] || c[13] != 8 || c[9] != 12 || c[5]) fail("the plate rims did not take the lowest lames in order");
    trimBand(v, 2, reg, row, 6, TE_HEM | TE_PLATE, 2, 0);
    for (int x : {3, 4, 11, 12}) if (reg[(size_t)13 * 14 + x] == (uint8_t)Region::Trim) fail("a plate rim over a gem or a lone pixel");
    for (int x = 0; x < 14; x++)
      if (reg[(size_t)14 * 14 + x] == (uint8_t)Region::Trim || reg[(size_t)10 * 14 + x] == (uint8_t)Region::Trim) fail("a plate rim on its line");
  }
  return miss;
}

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
