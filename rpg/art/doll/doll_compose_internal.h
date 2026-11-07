// EMBERVALE art: the doll compositor's private seam (SOLID 3.6). Included only by doll_compose.cpp (the atlas, the frame
// driver, frameInfo / probeFrame / trimShares and the side channel's owner), doll_paint.cpp (FramePaint: the frame's items,
// anchors and back-to-front paint) and doll_passes.cpp (the post passes and A3's finishing pass). Nothing else includes it.
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include "rpg/art/doll/doll_internal.h"

namespace art {
namespace doll {
namespace comp {

using namespace pool;

constexpr int CW = 160, CH = 136, CPX = 80, CPY = 104;   // the scratch canvas and its pivot

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
struct Tile { const ViewRec* v = nullptr; Slot on = Slot::Cloth1; };
// (P2c) a region tile (`tags tile` + `on-region <R>`): repeated over those regions' pixels on every part that marks them
// (ladder) fin: a finish tile's FIN_TILE_* bit; fit / stamp: LADDER-FINISH's placements (`tags fit`: the view <prefix><w>x<h>
// that fits the part's P box best, centred on it; `tags stamp`: a view laid once per connected blob of the region, keyed by
// the blob's size); part: the tile's part (its keyed views)
// (P2c forge) forgeSlot: a forge tile's tier slot (its finish-free tiles draw only on that slot's parts; -1 every part);
// forgeMask: a limb tile's regions narrowed to the slots that picked its value
struct RegionTile { const ViewRec* v = nullptr; uint32_t mask = 0; uint8_t fin = 0; bool fit = false, stamp = false; const PartRec* part = nullptr;
                    int forgeSlot = -1; bool forge = false; };
// (P2c forge, contracts 3c) a cut tile (`on-slot <slot>`) or an edge stamp (`on-edge hem|rim`) on the armour and pauldron
// parts: their views transformed before the paint (forgeView)
struct ForgeTile { const PartRec* part = nullptr; bool edge = false, rim = false; Slot on = Slot::Metal; };

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
// (D93) the trim band's edge kinds (trimBand's `edges`: the floor mode's walk)
enum TrimEdge : int { TE_HEM = 1, TE_NECK = 2, TE_ARM = 4, TE_RIM = 8, TE_PLATE = 16 };

// (fix round 3, D93's walk past its four edges) floorLv 5.. the plate rims: the lower rim of the piece's own lames, the lowest
// (floorLv - 4) internal lines first (a step-0 line inside the piece, solid above and below, or a seam N row); 1 px, laid in the
// tile's last (shaded, drawn-step) row: what shows when the hem is drawn R and the neck and arm openings sit under the arms
constexpr int kBandRim = 0x40;   // bandRow's flag: a plate rim (the tile's last row)

// (wave-2 fix round, D78; doll_paint.cpp) the trim band of a view (the comment at its definition); trimSelftests reads it too
void trimBand(const ViewRec& v, int W, std::vector<uint8_t>& bandReg, std::vector<int8_t>& bandRow, int floorLv = 0, int edges = 0, int edgeW = 0,
              int cutLv = 0);
bool partHasRoleCat(const PartRec& p, Role r);   // doll_paint.cpp

// ---------------------------------------------------------------- the side channel (defined in doll_compose.cpp)
extern thread_local std::vector<Item> g_items;   // the last composed frame's items (frameInfo reads them)
// (P2c) the probe's per-pixel planes (probeFrame sets them for one compose; null otherwise)
extern thread_local std::vector<int16_t>* g_probeOwner;
// (P2c forge i2) the pixels the body's armour and pauldrons wrote last (the glint pass reads them)
extern thread_local std::vector<uint8_t> g_bodyPiece;
extern thread_local std::vector<uint8_t> g_depthPx;   // (envelope r2) 1 where a held, back or fx item drew the pixel (C19's off-figure)
extern thread_local std::vector<uint8_t>* g_probeRegion;
extern thread_local int g_probeGrip, g_probeGripL;
// (P2c SLICE-DOLL i2) a turned frame's screen-space items: a view placed at the pivot (`anchor pivot`: the corpse's spear
// and bow `drop`) is drawn after the turn, level on the ground beside the lying body, not turned with it. 0: every item;
// 1: the body only (the turned pass); 2: the pivot items only (drawn into the turned figure)
extern thread_local int g_screenMode;

// composes one frame into o (raw: no finishing pass). `facing` is the frame's own (a turned frame passes the left view)
// (ladder i3) the stir: while an idle's breathe frame (frame 1) composes for a look flagged `breeze` (a T5-T6 cloak or a
// plumed helm: doll_resolve.cpp), every part drawing its rest key that has a `breeze` view draws that instead (the cloak's
// hem out a pixel, the crest's tail lifted). The panel saw nothing move on the top tiers; the art was drawn, no clip named it.
extern thread_local bool g_breeze;
// (forge round 2, D41) the falls and the lying figures keep their pauldrons on the arm layer (a fall's arms fly past the
// cloak, and a lifted shoulder closed 1-2 px pockets against the helm: C3a)
extern thread_local bool g_deathPart;
// (ladder i2) the pixels a held weapon wrote this frame (FramePaint writes them; a helm's erase and the speck passes spare them)
extern thread_local std::vector<uint8_t> g_heldPx;
// (P5) the code a fingerprinted pixel had before its dye or mark (0: the fingerprint left it): the post passes read the
// figure as it was before P5 (composeFrame swaps them in and the fingerprint back after), so no glint, speck or seam pass
// moves a pixel the fingerprint did not paint. Sized and written only for a recipe with DollRecipe::fpSlots
extern thread_local std::vector<uint8_t> g_fpUnder;
// (P5 fix r1) the cuff owner per pixel: the tier slot + 1 of the glove or boot whose cuff-region pixel was painted there last
// (0: any other part); the visible cuff pass (fpVisibleCuffs) reads it, so a cuff a sleeve or a hem covers shows below it
extern thread_local std::vector<uint8_t> g_fpCuffOwn;
void fpVisibleCuffs(const DollRecipe& r, FrameOut& o);
void fpDyeLeads(const DollRecipe& r, FrameOut& o);   // (P5 fix r2) a band showing its dye2 alone (dye < 2 px) turns its phase

// ---------------------------------------------------------------- the passes (doll_passes.cpp)
using PostPass = void (*)(const DollRecipe&, FrameOut&);
void despecklePass(const DollRecipe& r, FrameOut& o);
void glintPass(const DollRecipe& r, FrameOut& o);
void glintCapPass(const DollRecipe& r, FrameOut& o);
void glassPass(const DollRecipe& r, FrameOut& o);
void starBandPass(const DollRecipe& r, FrameOut& o);
void mythicFirePass(const DollRecipe& r, FrameOut& o);
void floatSpeckPass(const DollRecipe& r, FrameOut& o);
void mythicSpeckPass(const DollRecipe& r, FrameOut& o);
// every post pass of a composed frame, in its one order (kPostPasses in doll_passes.cpp)
void postPasses(const DollRecipe& r, FrameOut& o);
// A3's selout over a w x h code buffer (the finishing pass: after the turn, on the trimmed crop)
void finishPass(uint8_t* code, int w, int h, int stride);

// ---------------------------------------------------------------- FramePaint (doll_paint.cpp)
// One frame's paint: the recipe's parts gathered into items at their views, the anchors placed (grip-anchored items read
// the marks of the parts before them), then every item painted back to front into a FrameOut. composeFrame keeps one per
// thread (its vectors are the frame's scratch, reused frame to frame as the old thread_local locals were).
struct FramePaint {
  struct Place { const ViewRec* v = nullptr; int ox = 0, oy = 0; };   // LADDER-FINISH's fit placement on a view
  struct Blob { int x0, y0, x1, y1, n, metal; uint8_t rg; };          // a W / G blob (stamp tiles)
  struct ItemPaint {   // one item's paint state, derived once before its pixels
    const Item* it = nullptr;
    const ViewRec* v = nullptr;
    bool handOnly = false;
    uint8_t bodyPiece = 0, heldMark = 0, shieldMark = 0, depthMark = 0;
    bool helmErase = false;
    int sway = 0, swayFrom = 0;
    bool stretch = false, hasReg = false, tiled = false;
    int ts = -1;
    bool toBlade = false, toMetal = false, brassBody = false;
    bool toLeather = false;   // (P5b) a signed leather body's styled skirt and sleeves: their metal in the leather
    bool helmLeather = false;   // (P5b fix r2) a signed T2 leather helm: its Metal in the leather, its Metal 2 fittings kept
    bool toGilt = false;      // (P5b fix r1) a gilt people's signed T5 helm or shield: its fittings (Metal 2) in the gilt (Device)
    bool toGiltGrowth = false;   // (P5b fix r1) a signed helm's legend growth part (the Jade kuwagata): its metal 1-4 in the gilt
    const ViewRec* bandView = nullptr;
    bool bandOn = false;
    // (P5) the fingerprint on this piece (DollRecipe::fpSlots): R / N in the dye band, N thinned, a cuff or hem row (fpEdge)
    bool fp = false, fpR = false, fpN = false, fpThin = false, fpOuter = false, fpStitch = false, fpSparse = false, fpHemRow = false, fpEdge = false;
    bool fpBoss = false;   // (fix r1) a T1-T4 shield view with no P (the buckler: rim and boss): its boss (G) takes the dye band
    bool fpLining = false; // (fix r1) a T3-T5 body, helm or shield view with R but no N or D: the lining along the rim's inner edge
  };

  // the frame (anchors() sets them)
  const DollRecipe* rp = nullptr;
  int facing = 0;
  const FrameRec* fp = nullptr;
  FrameOut* op = nullptr;
  Pt base[ANCHOR_COUNT];
  // the gathered tiles
  Tile tile;
  std::vector<RegionTile> rtiles;
  std::vector<ForgeTile> ftiles;
  const PartRec* bandTile = nullptr;   // (wave-2 fix round, D78) the trim tile: `tags tile band` + `on-region R`
  // the gripping arms (placeAnchors)
  int gripFrom = -1, gripLFrom = -1;   // (P2c) the items whose views marked grip and gripL (the gripping arms)
  int handRFrom = -1, handLFrom = -1;  // (P2c i3) the items that marked handR / handL (the grip's fallback: a carry at rest)
  // back to front
  const Pool* pool = nullptr;                 // the frame's pool and planes (paintBackToFront looks them up once)
  std::vector<Item>* itemsV = nullptr;
  std::vector<uint8_t>* heldPxV = nullptr, *bodyPxV = nullptr, *depthPxV = nullptr;
  std::vector<int16_t>* probeOwnerV = nullptr;
  std::vector<uint8_t>* probeRegionV = nullptr;
  std::vector<int> dst0, dst1;
  uint8_t tileOn = 0;
  bool twoHand = false, hangArmour = false;
  std::vector<uint8_t> shieldPx;   // (wave-2 fix round, D82) the gripL shield's pixels
  std::vector<uint8_t> shieldKeep, shieldKeepRg;
  std::vector<int16_t> shieldKeepOw;
  // one item's scratch
  std::vector<uint8_t> bandReg;
  std::vector<int8_t> bandRow;
  std::vector<Place> fitAt;
  std::vector<Place> fitAt0;   // (P5) the pre-P5 fits (finishPlacements; empty: the same as fitAt)
  std::vector<int> blob, stack;
  std::vector<Blob> blobs;
  std::vector<uint8_t> irisBand;
  std::vector<int> metalRow;
  std::vector<int16_t> fpRow;   // (P5) per view column: the cuff's top row or the hem's bottom row (-1 none)
  uint8_t fpPre = 0;
  bool fpSeam = false;          // (P5) the pixel is a dye seam (Region::DyeSeam: read as no region, painted as N by the fingerprint)            // (P5) recolour's code before the fingerprint touched it (0: untouched); stamp files it in g_fpUnder

  // the frame, in order
  void anchors(const DollRecipe& r, int facing, const FrameRec& f, FrameOut& o);
  void gatherItems(const std::vector<int>* onlyParts, const char* forceKey, bool (*keep)(const PartRec&));
  void sortItems();
  void placeAnchors();
  void paintBackToFront();

 private:
  void gatherTile(int pi, const PartRec& p);
  void gatherPart(int pi, const PartRec& p, const char* forceKey, uint16_t forced);
  void place(Item& it);
  Pt markPt(uint16_t id) const;
  void tipMark();
  void paintItem(const Item& it, bool handOnly);
  const ViewRec* stampEdges(const Item& it, bool handOnly);
  void trimBandOf(const Item& it, const ViewRec& v, bool handOnly, ItemPaint& ip);
  void finishPlacements(const ViewRec& v, int ts, bool hasReg);
  void irisBands(const Item& it, const ViewRec& v, bool hasReg);
  void fingerprintOf(const Item& it, const ViewRec& v, ItemPaint& ip);
  uint8_t fpColour(const ItemPaint& ip, int x, int y, uint8_t c, uint8_t rg) const;
  // (P5 fix r2, B10) the per-pixel path in two instantiations: Fp (a frame with a fingerprint: DollRecipe::fpSlots) files the
  // pre-P5 code under each pixel; without it the loop is the pre-P5 one (no fingerprint branch, no under plane)
  void paintPixels(const ItemPaint& ip);
  template <bool Fp> void paintPixelsT(const ItemPaint& ip);
  template <bool Fp> uint8_t recolour(const ItemPaint& ip, int x, int y, uint8_t c, uint8_t rg);
  uint8_t regionTiles(const ItemPaint& ip, int x, int y, uint8_t c, uint8_t rg, uint8_t tiles, bool pre5 = false);
  template <bool Fp> void stamp(const ItemPaint& ip, int x, uint8_t c, uint8_t rg, int sx, const int ys[2]);
};

}  // namespace comp
}  // namespace doll
}  // namespace art
