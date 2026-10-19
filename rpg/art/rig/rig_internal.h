// EMBERVALE art internals: the v2 rig's painters (rpg/art/rig/*.cpp), shared by RIG-BODY, RIG-ARMS and POSES. Not part
// of the public API (that is rpg/art/art_rig.h). RIG-BODY owns this header (the skeleton and its anchors land here
// with the style slice); the other rig areas ADD their declarations, never rename or change one.
//
// How a v2 frame is made (STYLE_P2 3-10):
//   1. a Pose (pivot-relative joint positions, the facing, the expression, the weapon's grip and direction) is
//      solved against the look's Body (heights and widths by body, build, people and age);
//   2. paintFigure paints the parts back to front on a Fig (a scratch canvas with a layer id per pixel): legs, the
//      skirt, the torso, the arms, the head (face, ears, hair), the hands, the weapon, each in its own ramps with the
//      light from the top-left; a part laid over another of a similar value gets a 1 px selective inner contour
//      (STYLE_P2 8); the skin under the hair gets its one-row contact shadow;
//   3. the ink outer outline goes round the whole silhouette (outlineInk: Ink below / right, Lit Ink above / left);
//   4. the frame is trimmed to its opaque box with its pivot (the point between the feet on the sole row: F) and its
//      pose data (RigAnchors, pivot-relative), and packed into the look's atlas (SheetBuilder).
// Rows in poses are pivot-relative: y = 0 is F, the sole's OUTLINE row; painted pixels end at y = -1.
#pragma once
#include <cstdint>
#include <vector>
#include "rpg/art/art_internal.h"
#include "rpg/art/art_rig.h"

namespace art {
namespace rig2 {

// a 5-step ramp with external linkage (art_internal.h's Ramp lives in an unnamed namespace, one per file)
struct Ramp5 {
  uint32_t c[5] = {};
  uint32_t operator[](int i) const { return c[i < 0 ? 0 : (i > 4 ? 4 : i)]; }
};
inline Ramp5 r5(const Ramp& r) { Ramp5 o; for (int i = 0; i < 5; i++) o.c[i] = r[i]; return o; }

// the parts of humanRig (art_rig.cpp dispatches); an empty RigSheet means "not painted yet"
RigSheet rigLoco(const HumanLook& look);     // rig_body.cpp   (RIG-BODY): Idle 3, Walk 6, Hurt 1 in four facings
RigSheet rigAction(const HumanLook& look);   // rig_actions.cpp (RIG-ARMS): the look's family x 3 variants x
                                             //   (Windup 2, Strike 2, Follow 2), Cast 2, the bow's Draw 3 + Release 1;
                                             //   until it paints the sword family it should return
                                             //   sliceSwordAction(look) for Family::Sword (the style slice's swings)
RigSheet rigDeath(const HumanLook& look);    // rig_death.cpp  (RIG-BODY): Fall 3, Lying 2 (variant 0 head west, 1 east)
                                             //   Fall is painted per facing AND side: variant 0 buckles and topples
                                             //   WEST (it ends in Lying variant 0), variant 1 EAST (Lying variant 1),
                                             //   in all four facings (animPick's side: the knock, else the facing)

// ---------------------------------------------------------------- scratch canvases (STYLE_P2 3)
constexpr int LOCO_W = 48, LOCO_H = 48, LOCO_FEET = 44;   // locomotion, postures, the fall and lying (feet at row 44)
constexpr int ACT_W = 80, ACT_H = 72, ACT_FEET = 60;      // actions (swings, casts, the bow)

enum Facing : uint8_t { FDown = 0, FUp = 1, FRight = 2, FLeft = 3 };
enum Expr : uint8_t { ExNeutral, ExBlink, ExHurt, ExEffort, ExClosed };

// ---------------------------------------------------------------- the body base (STYLE_P2 3, 14)
// bodyOf gives every body the skeleton paints: man / woman x slim / average / broad x human / half-breed / elf, and the
// youth (age 2: 26 px, torso 6, legs 7) and child (age 1: 21 px, an 11 px head, torso 5, legs 5) proportions of
// STYLE_P2 3, so LOCO and DEATH paint kids natively at their own sizes (R2). POSES (rig_kin.cpp) owns what kids DO beyond
// that (their postures, hopping at tag); a kid's posture reads the same Body.
struct Body {
  int woman = 0;          // 0 man, 1 woman
  int build = 0;          // HumanLook::build (0 average, 1 slim, 2 broad), after the people's slimming
  int people = 0, age = 0;
  int legs = 11;          // rows from the hip row to F (man 11, woman 10; the elf +1; youth 7, child 5)
  int hip = -11;          // the hip row (pivot-relative)
  int shoulder = -18;     // the shoulder row
  int neck = -19;         // the neck / collar row
  int headTop = -30;      // the top painted row of the head at rest (the crown's outline is one above)
  int shoulderHalf = 7;   // the shoulder line incl. the arms is x = -shoulderHalf .. shoulderHalf - 1 - trimR (front views)
  int torsoHalf = 5;      // the torso at the chest: x = -torsoHalf .. torsoHalf - 1 - trimR
  int waistHalf = 4;
  int hipHalf = 5;
  int stance = 2;         // front views: the feet's inner columns are x = -stance .. stance - 1 apart (a gap of 2 * stance)
  int armSwing = 3;       // walk: the hands swing this far (man 3, woman 2)
  // (RIG-BODY, after the slice: every default below is the slice's adult, so the slice's frames never move)
  int trimR = 0;          // 1: an odd width (a slim build): the right edge of the shoulders, chest, waist and hips 1 px in
  int headRows = 12;      // the head box's rows incl. the crown outline (a child 11: the forehead row is dropped, so the
                          //   face and hair maps paint every map row but row kHeadSkipRow, one px rounder)
  int armU = 4, armF = 4; // the upper arm and the forearm (youth 3 / 3, child 3 / 2)
  int chestDepth = 0;     // side views: the chest 1 px deeper (broad)
  int stride = 7;         // side views: the walk's widest foot offset from the hips (adult 7; youth 5, child 4)
  int ear = 0;            // 0 a round ear (human), 1 half-breed (a 1 px tip up), 2 elf (2-3 px swept up and back,
                          //   breaking the head's outline)
  int chinW = 6;          // the chin row's width (men 6; women, elves and children 4)
  int legW = 3;           // the legs' width (a child's 2)
  float hipJoint = 2.5f;  // front / back views: the hip joints' centres at x = -hipJoint and +hipJoint (a child's 2)
  int shoeW = 4;          // the shoe's widest row (a child's 3)
};
constexpr int kHeadSkipRow = 3;   // the face / hair map row a child's 11 px head leaves out (the forehead)
Body bodyOf(const HumanLook& look);

// ---------------------------------------------------------------- the resolved colours of a look (STYLE_P2 6, 7)
struct Kit {
  Ramp5 skin, hair, top, sleeve, leg, boot, belt, trim, pat, metal, guard;
  uint32_t lash = 0, iris = 0, brow = 0, mouth = 0, rose = 0, blush = 0;
  int hair_ = 1;          // Hair
  int outfit = 0;         // Outfit (Tunic, Dress, Robe, Rags as painted)
  int cut = 0;            // the garment cut (cult::Cut + 1; 0 the outfit's own)
  int pattern = 0;        // cult::Pattern (0 plain)
  bool skirt = false;     // a long skirt to the ankles (Dress, Robe, the long cuts)
  bool beard = false;
  int weapon = 0;         // HumanLook::weapon
  // (RIG-BODY, after the slice: the civilian base; every default leaves the slice's looks as they were)
  Ramp5 head;             // headwear cloth (headColor, else topColor)
  Ramp5 hoodR;            // the legacy hood's cloth (a robe's or a culture garment's own cloth, else the trousers' shade)
  Ramp5 fur, straw, gold;
  int headwear = 0;       // cult::Headwear painted (0 none; the legacy hood wins over it)
  bool hood = false;      // the legacy hood (HumanLook::hood)
  int facePaint = 0;      // 1 stripes, 2 dots, 3 mask band, 4 sun mark
  int jewellery = 0;      // bits: 1 earrings, 2 torc
  int hang = 0;           // what hangs below the hips: HangNone (the tunic's own hem), HangShort (the culture tunic's
                          //   skirt, 2 rows), HangKnee (coat, kilt, poncho fringe), HangLong (to the ankles: skirt is set)
  bool rags = false;      // torn hems, patches, bare arms and feet
  bool openFront = false; // the kaftan's and the coat's front opening (the inner garment shows down the middle)
};
enum Hang : uint8_t { HangNone, HangShort, HangKnee, HangLong };
void makeKit(const HumanLook& look, Kit& k);
bool longSleeved(const Kit& k);   // rig_cloth.cpp: the forearm in the sleeve's cloth (robes, gowns, kaftans, coats)

// ---------------------------------------------------------------- a pose (one frame of the skeleton)
// Every position is pivot-relative (x right, y down; y = 0 the sole's outline row). Hands are the 2 x 2 fist's
// top-left; feet are the shoe's ankle column (front views: the shoe's inner column) and a lift (rows off the ground).
struct Pose {
  uint8_t facing = FDown;
  uint8_t expr = ExNeutral;
  int8_t dy = 0;            // everything above the knees moves down this much (walk bob +1, a crouch +2)
  int8_t chest = 0;         // the shoulders, chest, arms and head rise (-1: the breath)
  int8_t lean = 0;          // the shoulders and head shift this far in x against the hips (a lean, a slump)
  int8_t headX = 0, headY = 0;   // the head against the neck (snapped back, bowed)
  int8_t hipX = 0;          // the hips (and the whole upper body) shift in x (weight on one leg)
  int8_t hRx = 0, hRy = 0, hLx = 0, hLy = 0;   // the hands (right = the weapon hand)
  uint8_t hRfront = 1, hLfront = 1;            // side views: the arm is drawn in front of the torso (else behind)
  int8_t fRx = 0, fRy = 0, fLx = 0, fLy = 0;   // the feet: column, lift
  int8_t kR = 0, kL = 0;    // side views: the knees pushed this many px further forward (front views: unused)
  int8_t wDir = -1;         // the weapon's direction (0..15 clockwise from pointing right; -1: none drawn)
  uint8_t wBehind = 0;      // the weapon is drawn behind the body
  int8_t wShort = 0;        // the weapon foreshortened by this many px
  uint8_t twist = 0;        // side views: the torso turned a quarter toward the viewer (swings)
  uint8_t elbowOut = 0;     // the arms' elbows bend outward / up (raised arms) instead of down
  // (RIG-BODY, after the slice) secondary motion: what hangs lags one walk frame behind the body (STYLE_P2 10, depth)
  int8_t hairSwing = 0;     // a braid's or a tail's hanging part shifts this far in x (side views: + = behind the body)
  uint8_t hemLag = 0;       // a hanging hem follows hemRx / hemLx (the last frame's feet) instead of the feet
  int8_t hemRx = 0, hemLx = 0;
  uint8_t sheen = 0;        // the hair's sheen slides 1 px back along its band (the head dipping on a walk's down frame)
  uint8_t hatOff = 0;       // a hat that falls off (the cap, the conical hat) is not painted (the lying body)
  uint8_t bowRest = 0;      // the bow (art id 3) carried braced in the off hand (its fist hL*), aimed along wDir (rig_arms.cpp paintBow)
};

// ---------------------------------------------------------------- the figure canvas
struct Fig {
  int W = 0, H = 0, ox = 0, oy = 0;    // canvas size and the pivot's canvas position
  std::vector<uint32_t> px;            // RGBA
  std::vector<uint8_t> lay;            // the layer each pixel was painted by (0: empty)
  uint8_t cur = 0;                     // the layer being painted
  uint8_t legLay[2] = {0, 0};          // (RIG-BODY) the two legs' layers (a long skirt covers every leg pixel above its hem)
  int lx0 = 0, ly0 = 0, lx1 = -1, ly1 = -1;   // the current layer's bounds (canvas)
  int gx0 = 1 << 20, gy0 = 1 << 20, gx1 = -1, gy1 = -1;   // everything painted (canvas)
  Fig(int w, int h, int pivX, int pivY) : W(w), H(h), ox(pivX), oy(pivY), px((size_t)w * h, 0), lay((size_t)w * h, 0) {}
  bool in(int cx, int cy) const { return cx >= 0 && cy >= 0 && cx < W && cy < H; }
  void set(int x, int y, uint32_t c) {   // pivot-relative
    const int cx = x + ox, cy = y + oy;
    if ((unsigned)cx >= (unsigned)W || (unsigned)cy >= (unsigned)H) return;   // (one compare per axis: R6's bake time)
    const size_t i = (size_t)cy * W + cx;
    px[i] = c;
    lay[i] = cur;
    lx0 = cx < lx0 ? cx : lx0;
    ly0 = cy < ly0 ? cy : ly0;
    lx1 = cx > lx1 ? cx : lx1;
    ly1 = cy > ly1 ? cy : ly1;
  }
  uint32_t get(int x, int y) const { const int cx = x + ox, cy = y + oy; return in(cx, cy) ? px[(size_t)cy * W + cx] : 0u; }
  uint8_t layerAt(int x, int y) const { const int cx = x + ox, cy = y + oy; return in(cx, cy) ? lay[(size_t)cy * W + cx] : (uint8_t)0; }
  bool solid(int x, int y) const { return (get(x, y) >> 24) != 0; }
  void begin() { mergeBounds(); cur++; lx0 = W; ly0 = H; lx1 = -1; ly1 = -1; }
  void mergeBounds() {   // the layer just painted joins everything painted (call before reading gx0..gy1)
    if (lx1 < lx0) return;
    if (lx0 < gx0) gx0 = lx0;
    if (ly0 < gy0) gy0 = ly0;
    if (lx1 > gx1) gx1 = lx1;
    if (ly1 > gy1) gy1 = ly1;
  }
  // the inner contour (STYLE_P2 8): older pixels next to the current layer, of a similar value, darken toward the
  // current layer's outline colour (the `layered` rule). Call after painting a layer.
  void contour(float strength = 0.75f, float similar = 0.16f, uint8_t skipLayer = 0);   // skipLayer: never contour that layer (a hand's own arm)
};

// ---------------------------------------------------------------- painters (each file owns its part)
// rig_core.cpp: limbs, the frame (paint + outline + trim), the atlas
void limb(Fig& f, float ax, float ay, float bx, float by, int w, const Ramp5& r, int litBias = 0);
struct Cut { Canvas c; int px = 0, py = 0; RigAnchors an; };
Cut paintFrame(const HumanLook& look, const Body& B, const Kit& K, const Pose& P, int scratchW, int scratchH, int feet);
// the same in two steps: the raw figure (no outline, the whole scratch canvas; the pivot at (W / 2, feet)) and its anchors,
// then the ink outline and the trim (the death atlas turns a raw figure onto the ground in between)
Canvas paintRaw(const HumanLook& look, const Body& B, const Kit& K, const Pose& P, int scratchW, int scratchH, int feet, RigAnchors& an);
Cut finishFrame(Canvas raw, int pivX, int pivY, const RigAnchors& an);
struct SheetBuilder {
  struct Item { int facing; Clip clip; int variant; int src; };   // src: index into cuts (shared frames share a rect)
  std::vector<Cut> cuts;
  std::vector<Item> items;
  std::vector<uint64_t> hashes;   // (RIG-BODY, R6) one per cut: add compares the pixels only where the hashes match
  int add(int facing, Clip c, int variant, Cut&& cut);              // returns the cut's index
  void addShared(int facing, Clip c, int variant, int src) { items.push_back({facing, c, variant, src}); }
  RigSheet finish();
};
// the weapon in hand (the slice's sword; RIG-ARMS generalises the base weapons in rig_arms.cpp). grip = the pixel
// under the fist's centre; returns the tip (pivot-relative) through tipX / tipY
void paintWeapon(Fig& f, const Kit& K, int weapon, float gx, float gy, int dir, int shorten, int& tipX, int& tipY);

// rig_face.cpp: the head (face, ears, expressions) and the hands
struct HeadAt { int x = 0, y = 0; };   // the head box's top-left (pivot-relative): the face's column 0 is x + 1
void paintFace(Fig& f, const Body& B, const Kit& K, const Pose& P, HeadAt h, RigAnchors& an);
void paintHand(Fig& f, const Kit& K, int x, int y, bool open = false);
struct Cut;
Cut blinkOf(const Cut& neutral, const Body& B, const Kit& K, int facing);   // the idle blink from the neutral frame

// rig_hair.cpp: the hair over (front) and behind (back) the head and body
void paintHair(Fig& f, const Body& B, const Kit& K, const Pose& P, HeadAt h, bool backLayer);
void hairContactShadow(Fig& f, const Kit& K, HeadAt h, uint8_t hairLayer);
// (RIG-BODY, after the slice) the beard (men only, STYLE_P2 4: the chin and jaw rows and the mouth corners, 1 px of the
// mouth left), painted after the face and before the hair
void paintBeard(Fig& f, const Body& B, const Kit& K, const Pose& P, HeadAt h);
// (RIG-BODY) face paint and earrings over the painted face (rig_face.cpp), after the hair and before the headwear
void paintFaceMarks(Fig& f, const Body& B, const Kit& K, const Pose& P, HeadAt h);
// (RIG-BODY) the left facing's row of a right-facing map (hair, beard, headwear): every code lands mirrored, then each
// stretch of one material (hair 0-4, a ribbon t T, gold y g G h, skin a-e, fur f u F v) is put back in its left-to-right
// order, so it is lit from the top-left again, while a lone feature (a jewel j, a shadow l, stubble s) keeps its mirrored
// place (a ribbon tied at the back of the head stays at the back). out gets cols chars ('.' where nothing).
inline int mapMaterial(char ch) {
  if (ch >= '0' && ch <= '4') return 1;
  if (ch == 't' || ch == 'T') return 2;
  if (ch == 'y' || ch == 'g' || ch == 'G' || ch == 'h') return 3;
  if (ch >= 'a' && ch <= 'e') return 4;
  if (ch == 'f' || ch == 'u' || ch == 'F' || ch == 'v') return 5;
  return 0;   // a lone feature
}
inline void mirrorRelit(const char* row, int n, int cols, char* out) {
  for (int i = 0; i < cols; i++) out[i] = '.';
  for (int i = 0; i < n && i < cols; i++) out[cols - 1 - i] = row[i];
  int i = 0;
  while (i < cols) {
    const int m = mapMaterial(out[i]);
    if (!m) { i++; continue; }
    int j = i;
    while (j < cols && mapMaterial(out[j]) == m) j++;
    for (int a = i, b = j - 1; a < b; a++, b--) { const char t = out[a]; out[a] = out[b]; out[b] = t; }
    i = j;
  }
}
// (RIG-BODY) a face / hair map row -> its row on the painted head (a child's 11 px head skips kHeadSkipRow); -1 = skipped
inline int headRow(const Body& B, int r) { return B.headRows >= 12 ? r : (r == kHeadSkipRow ? -1 : (r > kHeadSkipRow ? r - 1 : r)); }

// rig_cloth.cpp: the torso (garment), the sleeves' colour, the trousers, the skirt, patterns
struct TorsoAt { int x = 0, y = 0; };  // the torso's centre column and the shoulder row
void paintTorso(Fig& f, const Body& B, const Kit& K, const Pose& P, TorsoAt t);
void paintSkirt(Fig& f, const Body& B, const Kit& K, const Pose& P, TorsoAt t, int footRx, int footLx);
const Ramp5& sleeveRamp(const Kit& K);
const Ramp5& legRamp(const Kit& K);
uint32_t patternAt(const Kit& K, int x, int y, uint32_t base, int idx);   // the garment's pattern over its base colour
// (RIG-BODY, after the slice) the civilian base:
//   paintLegWear   after the legs, before the body: the kilt's bare knees, the rags' torn shins and bare feet (the legs'
//                  layers given, so only leg pixels change)
//   paintHeadwear  the 8 culture headwear kinds and the legacy hood (STYLE_P2 2, 7: cloth lit top-left, brims seen from
//                  the high 3/4 camera); backLayer: what falls behind the body (a hood's or veil's drape, a scarf's tails)
//   headwearHides  the hair under it is not painted, only long lengths below the ears (the circlet, the cap and the
//                  conical hat leave the hair)
//   paintTorc      jewellery bit 2: a gold torc at the collar
void paintLegWear(Fig& f, const Body& B, const Kit& K, const Pose& P, uint8_t legLayerA, uint8_t legLayerB);
void paintHeadwear(Fig& f, const Body& B, const Kit& K, const Pose& P, HeadAt h, bool backLayer);
bool headwearHides(const Kit& K);
void paintTorc(Fig& f, const Body& B, const Kit& K, const Pose& P, TorsoAt t);

// rig_body.cpp: the standing pose at rest of a facing (the hands at the hips; with a sword, the sword at rest)
Pose restPose(const Body& B, int facing, bool sword);
// rig_body.cpp: the slice's sword swings until RIG-ARMS's action library lands (rig_actions.cpp calls it for the
// sword family): 3 variants x (Windup 2, Strike 2, Follow 2) x 4 facings
RigSheet sliceSwordAction(const HumanLook& look);
// rig_body.cpp: the slice's Steel Arc (STYLE_P2 6, 12: the sword and dagger crescent, 32 x 32, 4 frames) painted into
// one 32 x 32 cell, facing right like every turning effect (the renderer turns it for down / up and mirrors it for
// left). The arc's centre is the cell's left-middle (the body, the FX placed at the family's reach in front of it), the
// crescent sweeping clockwise top to bottom: frame 0 the leading edge, 1 the growing blade with a glint at its head,
// 2 the full crescent, 3 the fading tail; core #F4F1E6 on the cutting rim, edge #B8C4D8 inside, on every frame.
// art_fx.cpp (RIG-ARMS) paints Fx::SwingArc with it (kFxInfo {32, 32, 4}). The arc scales with the cell's height: a
// 38 x 38 cell paints the finisher's 1.2x natively (no runtime scaling of a sprite, STYLE_P2 20).
void steelArc(Canvas& cell, int frame);

// ================================================================ (POSES, P2a wave 2) added declarations
// rig_kin.cpp: what kids do with their bodies in the postures (bodyOf gives their proportions): a child's legs dangle
// from a chair-height seat, a child hops at tag (Play), a youth's tools are shorter
int kinSeatLegs(const Body& B, int seatH);   // the rows a seated kid's shins reach below the knee (an adult: to the floor)
int kinHop(const Body& B, int frame);         // Play: the rows a kid's whole body rises on frame (0: on the ground)

// rig_poses.cpp: the posture painter. rig_core's depth order and part painters (face, hair, torso, skirt, hands, the ink
// outline and trim) with a posture's own legs, arms that may pass behind the body in front / back views, and props in
// hand painted in their own ramps between the layers (the hand drawn over its grip, STYLE_P2 15).
enum LegMode : uint8_t {
  LegStand,   // rig_core's standing legs (the Pose's feet and lifts)
  LegChair,   // seated at chair height: the hips on the seat (seatH px over the pivot), thighs forward, shins to the floor
  LegFloor,   // cross-legged: the hips 2 px over the pivot (the floor or cushion), the crossed shins before them
  LegKneel,   // on one knee: the hips 5 px over the pivot, the near / screen-left knee up, the other on the ground
  LegRide,    // astride: the legs down either side of a horse's barrel (side views: the far leg hidden by the horse)
  LegNone     // no legs (a sleeper under its blanket, a bather)
};
enum PropLayer : uint8_t {
  PropBack,    // before everything (behind the body: a back view's things held in front of the chest)
  PropBody,    // after the legs and the torso, before the arms
  PropGrip,    // what a hand grips: after the far arm and the head, before the near arm (side) / the hands (front, back)
  PropHand,    // after every arm, before the hands
  PropTop,     // over everything (the flame on a pole, a raised mug's foam)
  PropHead,    // right after the head (headwear-like extras: a goblin's ears, a draugr's horns)
  PROP_LAYERS
};
struct PoseX {
  Pose p;
  uint8_t legs = LegStand;
  int8_t seatH = 11;            // LegChair: the seat line over the pivot (the hips' row is -seatH)
  bool armRBack = false, armLBack = false;   // front / back views: the arm (and its hand) is hidden behind the body
  bool openR = false, openL = false;         // an open hand (2 x 3)
  bool hideHandR = false, hideHandL = false; // the hand is inside a prop (a sack's mouth, a drum)
  bool noHair = false;          // the hair is not painted (a skull, a helm painted by a hook)
  void (*prop)(Fig& f, const Body& B, const Kit& K, const PoseX& X, int layer, const void* ctx) = nullptr;
  const void* ctx = nullptr;    // the prop painter's data (the posture, its frame and variant)
  int8_t fr = 0;                // the posture's frame (for the prop painter)
  uint8_t var = 0;              // the posture's variant
};
// the raw figure (no outline; the pivot at (W / 2, feet)) and its pose data; finishPose adds the ink outer outline (rig_core's
// rule: Ink below / right, Lit Ink above / left), closes pinholes and trims to the opaque box with the pivot kept
Canvas paintPoseRaw(const HumanLook& look, const Body& B, const Kit& K, const PoseX& X, int W, int H, int feet, RigAnchors& an);
Cut finishPose(const Canvas& raw, int pivX, int pivY, const RigAnchors& an);
Cut paintPose(const HumanLook& look, const Body& B, const Kit& K, const PoseX& X, int W, int H, int feet);
// a straight 1 px stick (hafts, poles, rods) in ramp r on clean pixel steps, lit on its upper third; and the props
void propStick(Fig& f, float x0, float y0, float x1, float y1, const Ramp5& r, int k = 2);

// ================================================================ (RIG-ARMS, P2a wave 2) added declarations
// rig_arms.cpp: the base weapons at 16 angles (STYLE_P2 11), on paintWeapon's grip convention (gx, gy = the fist's
// centre - 0.5: the weapon's step 0 is the fist's top-left pixel). paintArms paints every drawn art id (armsDrawn: 1
// sword (= paintWeapon, to the pixel), 2 axe, 4 staff, 5 dagger, 6 mace, 7 greatsword, 8 spear, 9 walking stick);
// edge = the side the axe's bit faces (+1 the clockwise side of dir, -1 the other); tip = the blade's point, the axe's
// bit, the mace's head, the spear's point, the staff's gem, the stick's crook. armsTwoHandGap: px down the grip of the
// off hand when it holds the weapon too (0: one hand). paintBow: the bow in the off hand (its fist's top-left), the
// string to the string hand while drawn (draw 0 braced, 1 half, 2 full, 3 loosed), the arrow arrowLen px along aim
// (drawn before the stave when arrowFirst); returns the arrow's head (or the bow's front) through tipX / tipY.
bool armsDrawn(int weapon);
int armsTwoHandGap(int weapon);
// how a fighter carries the weapon standing and walking (hRx / hRy / wDir / wBehind / hRfront of p, whose facing is set):
// the sword as rig_body.cpp restWeapon; every other drawn art id its own (for LOCO, Cast and the bow family's casts)
void armsRest(const Body& B, int weapon, Pose& p);
void paintArms(Fig& f, const Kit& K, int weapon, float gx, float gy, int dir, int shorten, int edge, int& tipX, int& tipY);
void paintBow(Fig& f, const Kit& K, int bowX, int bowY, int aim, int half, int stringX, int stringY, int draw, int arrowLen, bool arrowFirst, int& tipX,
              int& tipY);

}  // namespace rig2

// (P2b phase A, lead) the procedural rig DETACHED: its public entry points renamed, so art::humanPosture2, sleeper2,
// seatFit2, bedFit2, rig2Covers, humanFigureStill2 and humanoidCell2 are the doll's (art_rig.cpp, rpg/art/doll). Nothing in
// the game calls these; tools/preview/rigdraft.cpp exports their pose numbers and pixels once as drafts. The P2c flip
// deletes rpg/art/rig with v1.
bool legacyRig2Covers(const HumanLook& L);
Canvas legacyFigureStill2(const HumanLook& look, int facing);
RigSheet legacyPosture2(const HumanLook& look, Posture p, uint8_t variant);
SeatFit legacySeatFit2(Prop seat, int kit, int facing);
BedFit legacyBedFit2(Berth b, int kit);
Canvas legacySleeper2(const HumanLook& look, Berth b, int kit, int frame);
int legacyHumanoidCellW2(int species);
int legacyHumanoidCellH2(int species);
Canvas legacyHumanoidCell2(int species, int frame);
inline RigSheet legacyRig(const HumanLook& look, RigPart part) {
  switch (part) {
    case RigPart::Loco: return rig2::rigLoco(look);
    case RigPart::Action: return rig2::rigAction(look);
    case RigPart::Death: return rig2::rigDeath(look);
    default: return RigSheet();
  }
}
}  // namespace art
