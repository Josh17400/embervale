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
#include <cmath>
#include <cstring>
#include "rpg/art/art_holders2.h"
#include "rpg/art/doll/doll_internal.h"

namespace art {
namespace doll {

namespace {

using namespace pool;

constexpr int CW = 160, CH = 136, CPX = 80, CPY = 104;   // the scratch canvas and its pivot

struct Cut { Canvas c; int px = 0, py = 0; RigAnchors an; };

// ---------------------------------------------------------------- the atlas packer (P2a's SheetBuilder, unchanged)
uint64_t cutHash(const Cut& c) {
  uint64_t h = 1469598103934665603ull ^ ((uint64_t)(uint16_t)c.c.w << 48 | (uint64_t)(uint16_t)c.c.h << 32 | (uint64_t)(uint16_t)c.px << 16 | (uint16_t)c.py);
  for (size_t i = 0; i < c.c.px.size(); i += 3) { h ^= c.c.px[i]; h *= 1099511628211ull; }
  return h;
}
bool sameAnchors(const RigAnchors& a, const RigAnchors& b) { return std::memcmp(&a, &b, sizeof(RigAnchors)) == 0; }
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
    const int bestW = bestCols ? bestS : bestL, bestH = bestCols ? bestL : bestS;
    s.atlas = Canvas(bestW, bestH);
    for (size_t i = 0; i < cuts.size(); i++) {
      const Canvas& c = cuts[i].c;
      for (int y = 0; y < c.h; y++) std::memcpy(&s.atlas.px[(size_t)(by[i] + y) * bestW + bx[i]], &c.px[(size_t)y * c.w], (size_t)c.w * 4);
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
struct RegionTile { const ViewRec* v = nullptr; uint16_t mask = 0; };

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
uint16_t regionTileMask(const Pool& P, const PartRec& p) {
  uint16_t m = 0;
  for (uint16_t t : p.tags) {
    const std::string& s = P.s(t);
    if (s.rfind("on-region:", 0) != 0) continue;
    for (size_t i = 10; i < s.size(); i++)
      for (int rg = 1; rg < REGION_COUNT; rg++) if (regionChar((Region)rg) == s[i]) m |= (uint16_t)(1u << rg);
  }
  return m;
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
      if (const uint16_t m = regionTileMask(P, p)) { if (tv) rtiles.push_back(RegionTile{tv, m}); continue; }   // (P2c) a region tile
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
    if (g_screenMode && ((it.anchor == Anchor::Pivot) != (g_screenMode == 2))) continue;
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
  // paint back to front
  static thread_local std::vector<int> dst0, dst1;
  const uint8_t tileOn = (uint8_t)tile.on;
  const bool twoHand = r.has("twohand");
  // handOnly: (P2c) the gripping arm's H pixels drawn again over the weapon it holds
  auto paint = [&](const Item& it, bool handOnly) {
    const ViewRec& v = *it.view;
    const int sway = it.part->sway && f.sway ? f.sway : 0, swayFrom = (int)v.h - std::min<int>(it.part->sway, v.h);
    buildColumns(r, v, dst0, dst1);
    const bool stretch = r.elf && v.stretch >= 0;
    const bool hasReg = !v.regions.empty();
    const bool tiled = tile.v && it.garment;
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
        // (P2c) a region tile over this region's pixels (outlines kept): ink keeps the slot; step 2 keeps the step, 1 / 3 move
        // it one down / up, 0 and 4 set it
        if (rg && !rtiles.empty() && codeStep(c) >= 1)
          for (const RegionTile& rt : rtiles) {
            if (!(rt.mask >> rg & 1)) continue;
            const ViewRec& t = *rt.v;
            int tx = (it.ax + x - v.ox - it.tx + t.ox) % t.w, ty = (it.ay + y - v.oy - it.ty + t.oy) % t.h;
            if (tx < 0) tx += t.w;
            if (ty < 0) ty += t.h;
            const uint8_t tc = t.cells[(size_t)ty * t.w + tx];
            if (tc < 2) break;
            const Slot ts = codeSlot(tc);
            const int tk = codeStep(tc), ck = codeStep(c);
            const int nk = tk == 2 ? ck : tk == 1 ? std::max(1, ck - 1) : tk == 3 ? std::min(4, ck + 1) : tk;
            c = code(ts == Slot::Ink ? codeSlot(c) : ts, nk);
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
        const int X = PAD + pvx + (turn == kTurnEast ? CPX - x : x - CPX), Y = PAD + pvy + y - CPY;
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
      Cut cut = composeOne(c->clip == (uint8_t)Clip::Walk ? walkR : r, c->facing, f, turnOf(f, lying, c->variant), fo, op);
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
