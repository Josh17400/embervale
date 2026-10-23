// EMBERVALE art API: the Clear Sight rig contract (P2a phase A, VISION_PLAN 15.36 item 4; art direction docs/STYLE_P2.md).
// Part of rpg/art.h (include that). FROZEN after phase A: lanes may ADD names, never rename them or change their meaning.
//
// Two human rigs live side by side through P2a and P2b:
//   rig 1  today's 16 x 24 sheets (art_human.cpp), the shipped default through P2a; frozen (no new v1 art)
//   rig 2  the 32 px v2 rig (rpg/art/rig/*.cpp: RIG-BODY, RIG-ARMS, POSES), behind `rig 2` (EMB_RIG=2, web ?rig=2,
//          script `rig 2`, Witness reset param "rig": 2) until P2b flips every human at once and deletes v1
// The switch, the body scale and the metrics are header-only (inline) so rpg/sim (anim.cpp, game.cpp) reads them
// without linking rpg_art. The painters (humanRig, humanPosture2, sleeper2...) live in rpg_art.
//
// Which file implements what (P2b phase A: the art-direction change, VISION_PLAN 15.36): the rejected procedural painters
// (rpg/art/rig/*.cpp) are DETACHED: nothing calls them at rig 2 (their entry points are renamed legacy*, kept compiled only
// for tools/preview/rigdraft.cpp until the P2c flip deletes them). Rig 2 draws the DOLL (rpg/art/doll/doll.h):
//   art_rig.cpp            (lead)      the dispatch to the doll (humanRig, humanPosture2, sleeper2, humanFigureStill2,
//                                      humanoidCell2, rig2Covers), the RigSheet helpers, holderName
//   doll/*.cpp             (DOLL)      the pool, the compositor, the resolver and coverage, the ramps, seatFit2 / bedFit2
//   art_fx.cpp             (P2c)       the 9 family effects (art::Fx::SwingArc..Twang: their sizes in kFxInfo)
//   art_props.cpp          (HOLDERS)   holderPx (with the holders' painters: art_building, art_home, art_sea...)
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include "engine/pix.h"
#include "rpg/art/art_human.h"
#include "rpg/art/art_life.h"

namespace art {

// ---------------------------------------------------------------- weapon art ids (HumanLook::weapon)
// 0 none, 1 sword, 2 axe, 3 bow, 4 staff, 5 dagger, 6 mace / hammer / pick, 7 GREATSWORD (P2a; v1 paints it as the
// sword), 8 spear / polearm, 9 walking stick, 10 ledger and 11 scroll case (art_rise.h), 12 the herald's horn (it was
// id 7 before P2a; v1 paints it exactly as before)
constexpr uint8_t WEAPON_GREATSWORD = 7;
constexpr uint8_t WEAPON_HORN = 12;

// ---------------------------------------------------------------- the switch
namespace rigdetail {
inline int& version() { static int v = 1; return v; }
}  // namespace rigdetail
inline int rigVersion() { return rigdetail::version(); }                       // 1 or 2
inline void setRigVersion(int v) { rigdetail::version() = v >= 2 ? 2 : 1; }   // callers flush their people caches
// what a body uses (doors, walls, seats, beds, mounts, boats, carts, ladders, gates) scales with the rig: 1 at rig 1,
// k = 32 / 22 at rig 2. Footprints in tiles never change.
inline float bodyScale() { return rigVersion() >= 2 ? 32.0f / 22.0f : 1.0f; }
inline int bodyPx(int px) { return rigVersion() >= 2 ? (int)std::lround(px * (32.0 / 22.0)) : px; }   // exactly px at rig 1

// ---------------------------------------------------------------- clips and families (STYLE_P2 sections 10 and 12)
enum class Clip : uint8_t { Idle, Walk, Hurt, Windup, Strike, Follow, Cast, Draw, Release, Block, Roll, Fall, Lying, COUNT };
enum class Family : uint8_t { Unarmed, Dagger, Sword, Greatsword, Axe, Mace, Spear, Staff, Bow, COUNT };
constexpr int CLIP_COUNT = (int)Clip::COUNT, FAMILY_COUNT = (int)Family::COUNT;
constexpr int RIG_FACINGS = 4;    // 0 down, 1 up, 2 right, 3 left (Actor::face); v2 paints all four, never mirrored
constexpr int RIG_VARIANTS = 3;   // hit 1 forehand, hit 2 backhand, hit 3 finisher (Actor::combo)
// the swing family of what is in hand (the walking stick swings as a staff; a ledger, scroll or horn is no weapon)
inline Family familyOfWeapon(uint8_t w) {
  switch (w) {
    case 1: return Family::Sword;
    case 2: return Family::Axe;
    case 3: return Family::Bow;
    case 4: case 9: return Family::Staff;
    case 5: return Family::Dagger;
    case 6: return Family::Mace;
    case 7: return Family::Greatsword;
    case 8: return Family::Spear;
    default: return Family::Unarmed;
  }
}
inline Family familyOf(const HumanLook& look) { return familyOfWeapon(look.weapon); }
inline const char* clipName(Clip c) {
  static const char* n[] = {"idle", "walk", "hurt", "windup", "strike", "follow", "cast", "draw", "release", "block", "roll", "fall", "lying"};
  return (int)c < CLIP_COUNT ? n[(int)c] : "?";
}
inline const char* familyName(Family f) {
  static const char* n[] = {"unarmed", "dagger", "sword", "greatsword", "axe", "mace", "spear", "staff", "bow"};
  return (int)f < FAMILY_COUNT ? n[(int)f] : "?";
}

// ---------------------------------------------------------------- metrics (heights in art px; F = the sole's outline row)
// Everything the view places relative to a body reads these, never a literal: markers, bubbles, plates, bars, the ring,
// the contact shadow, the slash FX reach, the bathing and swimming crops. rig(1) is today's numbers, rig(2) STYLE_P2 3.
struct RigMetrics {
  int version = 1;
  // figure height incl. the outline, by body (0 man, 1 woman) x age (0 adult, 1 child, 2 youth) x people (0 human,
  // 1 half-breed, 2 elf)
  uint8_t fig[2][3][3] = {};
  int headTop = 21;     // rows above F of the crown's outline row (adult man)
  int eyeRow = 15;      // rows above F of the eyes' lower pixel
  int handRow = 9;      // rows above F of the hands at idle
  int markerY = 30;     // px above the feet where an over-head marker / telegraph starts
  int bubbleY = 34;     // px above the feet of a speech or thought bubble's bottom
  int plateY = 28;      // px above the feet of an elite's name plate
  int barY = 26;        // px above the feet of a damaged foe's health bar
  int ringW = 18, ringH = 7;        // the hero's ring (STYLE_P2 8)
  int shadowW = 12, shadowH = 5;    // the contact shadow at the feet
  int reach[FAMILY_COUNT] = {};     // px from the body centre to the swing FX centre, per family
  int fxLift = 8;                   // px above the feet of the swing FX centre
  int bathVis = 14, bathUnder = 5;  // the bathing / swimming crop: rows shown in the air, rows tinted under the water
  int figH(int body, int age, int people) const {
    // (P2b) age 3, the elder: the adult body with the head stack a pixel lower at rig 2 (STYLE_P2 3, 14); v1's adult at rig 1
    if (age == 3) return figH(body, 0, people) - (version >= 2 ? 1 : 0);
    return fig[body < 0 ? 0 : body > 1 ? 1 : body][age < 0 ? 0 : age > 2 ? 2 : age][people < 0 ? 0 : people > 2 ? 2 : people];
  }
  int figH(const HumanLook& L) const { return figH(L.body, L.age, L.people); }
};
inline const RigMetrics& rig(int v) {
  static const RigMetrics r1 = [] {
    RigMetrics m;
    m.version = 1;
    for (int b = 0; b < 2; b++)
      for (int p = 0; p < 3; p++) {
        m.fig[b][0][p] = (uint8_t)(p == 2 ? 23 : 22);   // v1: one figure for both bodies; an elf a pixel taller
        m.fig[b][1][p] = (uint8_t)(p == 2 ? 19 : 18);   // childBody: rows dropped from the adult
        m.fig[b][2][p] = m.fig[b][0][p];                // v1 has no youth
      }
    for (int f = 0; f < FAMILY_COUNT; f++) m.reach[f] = 11;   // one arc for all, at aim * 11
    return m;
  }();
  static const RigMetrics r2 = [] {
    RigMetrics m;
    m.version = 2;
    for (int p = 0; p < 3; p++) {
      const int elf = p == 2 ? 1 : 0;
      m.fig[0][0][p] = (uint8_t)(32 + elf); m.fig[1][0][p] = (uint8_t)(31 + elf);   // adult man 32, woman 31; elf +1
      m.fig[0][1][p] = m.fig[1][1][p] = 21;                                       // child
      m.fig[0][2][p] = m.fig[1][2][p] = (uint8_t)(26 + elf);                      // youth (KIN-YOUTH: elf +1, as the adult's)
    }
    m.headTop = 31; m.eyeRow = 22; m.handRow = 10;   // (P2b) A3's rows: crown F-31 (woman F-30), eyes F-23..F-22, hands F-11..F-10
    m.markerY = 36; m.bubbleY = 42; m.plateY = 38; m.barY = 36;   // 4 px of air over the hair (STYLE_P2 17)
    m.ringW = 24; m.ringH = 9;
    m.shadowW = 16; m.shadowH = 6;
    static const int reach2[FAMILY_COUNT] = {9, 12, 16, 22, 16, 16, 30, 22, 12};
    for (int f = 0; f < FAMILY_COUNT; f++) m.reach[f] = reach2[f];
    m.fxLift = 12;
    m.bathVis = 20; m.bathUnder = 7;
    return m;
  }();
  return v >= 2 ? r2 : r1;
}
inline const RigMetrics& rig() { return rig(rigVersion()); }

// ---------------------------------------------------------------- atlases (STYLE_P2 10)
// A frame in an atlas: its rect and its pivot (px, py: the point between the feet on the sole row, relative to the
// rect's top-left). The renderer places the pivot on the actor's position and never flips a v2 people frame.
struct FrameRect { int16_t x = 0, y = 0, w = 0, h = 0, px = 0, py = 0; };
// The pose data of a frame (the skeleton's anchors), in px relative to the frame's pivot (x right, y down; the sole row
// is y = 0, the crown about -31). kNoAnchor: not in this frame. The painters fill it (RIG-BODY: head, hands, hips, feet;
// RIG-ARMS: the weapon's grip and tip); rpg_test --rig measures faces, hands, tip paths and the weapon hand from it, and
// P2b's gear layers paint on it.
constexpr int16_t kNoAnchor = -32768;
struct RigAnchors {
  int16_t headX = kNoAnchor, headY = kNoAnchor, headW = 0, headH = 0;   // the head box's top-left and size
  int16_t faceX = kNoAnchor, faceY = kNoAnchor;   // the face's top-left (front views: column 0 of the 10 px face, row of the brows)
  int16_t handRX = kNoAnchor, handRY = kNoAnchor;  // the right hand (the weapon hand) 2x2 fist's top-left
  int16_t handLX = kNoAnchor, handLY = kNoAnchor;  // the left hand
  int16_t hipX = kNoAnchor, hipY = kNoAnchor;      // the seat point (between the hips, on the seat line)
  int16_t footLX = kNoAnchor, footRX = kNoAnchor;  // the feet's columns on the sole row (contact frames)
  int16_t gripX = kNoAnchor, gripY = kNoAnchor;    // where the weapon is held (under the fist)
  int16_t tipX = kNoAnchor, tipY = kNoAnchor;      // the weapon's tip (blade point, axe / mace head, spear point, staff end)
  bool weaponBehind = false;                        // the weapon is drawn behind the body this frame (STYLE_P2 11)
  int16_t topY = kNoAnchor;                         // (P2b) the frame's top row (its highest opaque pixel: hair, a hat, a
                                                    // crest): markers and plates float over it (STYLE_P2 17)
};
// LOCO all humans; ACTION fighters; DEATH when someone falls; (P2b) RUN movers above the gait switch (54 px/s: clip Walk
// holds the run cycle)
enum class RigPart : uint8_t { Loco, Action, Death, Run, Drawn, COUNT };   // (P2c) Drawn: the drawn idle, walk (variant 1) and run (variant 2)
// A look's atlas for one part: the trimmed frames packed into one canvas, indexed by facing x clip x variant.
struct RigSheet {
  struct Span { int16_t first = -1; uint8_t count = 0; };
  Canvas atlas;
  std::vector<FrameRect> frames;
  std::vector<RigAnchors> anchors;   // one per frame when the painter fills them (else empty)
  std::vector<Span> spans = std::vector<Span>((size_t)RIG_FACINGS * CLIP_COUNT * RIG_VARIANTS);
  static size_t slot(int facing, Clip c, int variant) {
    facing = facing < 0 ? 0 : facing > 3 ? 3 : facing;
    variant = variant < 0 ? 0 : variant >= RIG_VARIANTS ? RIG_VARIANTS - 1 : variant;
    return ((size_t)facing * CLIP_COUNT + (size_t)c) * RIG_VARIANTS + (size_t)variant;
  }
  // frames of (facing, clip, variant); a clip painted without variants answers for every variant through variant 0
  int count(int facing, Clip c, int variant = 0) const;
  // frame i (wrapped into the clip's count), or nullptr when the clip has no frames in this sheet
  const FrameRect* frame(int facing, Clip c, int variant, int i) const;
  // the packer's helper: the frame's rect must already be inside atlas; frames of one span are added consecutively
  void add(int facing, Clip c, int variant, const FrameRect& r);
  void add(int facing, Clip c, int variant, const FrameRect& r, const RigAnchors& an);   // with its pose data
  // the pose data of frame i of (facing, clip, variant) (nullptr: no frame or no anchors painted)
  const RigAnchors* anchorsOf(int facing, Clip c, int variant, int i) const;
  bool empty() const { return frames.empty(); }
  size_t bytes() const { return (size_t)atlas.w * atlas.h * 4; }
};

// the v2 atlas of a look's part (empty until the painters fill it: art_rig.cpp dispatches to rigLoco / rigAction /
// rigDeath). Cached by the view under (look.key(), part): View::rigTex.
RigSheet humanRig(const HumanLook& look, RigPart part);
// a posture on v2 (the 31 postures of art_life.h, their variants and frame counts, four facings: clips Idle / Walk
// hold the posture's frames as variant 0)
RigSheet humanPosture2(const HumanLook& look, Posture p, uint8_t variant);
// the v2 sleeper fitted to a berth (bedFit2's canvas; drawn after the berth at its own anchor), frame 0 / 1 breathing
Canvas sleeper2(const HumanLook& look, Berth b, int kit, int frame);
// true when every field of the look is painted on v2 (P2b makes it real per field; P2a's stub: false). The view draws
// v2 for a look only where this holds, so `rig 2` never shows a half-painted person.
bool rig2Covers(const HumanLook& look);
// the statue / prop figure (humanFigureStill) and the humanoid monsters' cells (humanoidCell) on v2: humanFigureStill
// and the monster sheets route here when rigVersion() >= 2
Canvas humanFigureStill2(const HumanLook& look, int facing);
Canvas humanoidCell2(int species, int frame);   // species: 1 skeleton, 2 draugr, 3 goblin (art_internal.h Species)
int humanoidCellW2(int species);
int humanoidCellH2(int species);

// ---------------------------------------------------------------- body-holders (STYLE_P2 1 and 16)
// The fit of a v2 sitter / sleeper (POSES and HOLDERS agree the numbers; the v1 seatFit / bedFit stay byte-identical).
// Phase A stubs return the v1 fits.
SeatFit seatFit2(Prop seat, int kit, int facing);   // rig_poses.cpp (POSES)
BedFit bedFit2(Berth b, int kit);                   // rig_sleep.cpp (POSES)
// What a body uses, measured from the painters (HOLDERS fill it from the numbers they paint with; the values are px at
// the CURRENT rig, so rpg_test --rig --gates reads H2's ratios at rig 2 and proves rig 1 unchanged). Phase A: today's
// numbers at rig 1 (and bodyPx of them at rig 2 where the painter already scales: the doors and walls).
enum class Holder : uint8_t {
  HouseDoorH, HouseDoorW, SmallDoorH, TempleDoorH, GrandDoorH, PalaceDoorH,   // door openings (height 0.70-0.76 of the figure)
  WallOneStorey,                    // a one-storey wall (1.00-1.12)
  ChairSeat, StoolSeat, BenchSeat, ThroneSeat,   // the seat surface above the floor (0.14-0.20 at rig 2: A3's knee)
  TableTop, CounterTop,             // (0.44-0.50)
  HorseWithers,                     // the ridden horse's withers above the ground (0.80-0.90)
  RowboatGunwale, DeckRail,         // (0.35-0.45, 0.45-0.55)
  LadderRungStep, StairStep,        // rungs every 6 px
  GateClear, ArchClear, CityGateClear,   // clear height of a passage (1.25 or more)
  Fence, Hedge, GardenWall,         // (0.45-0.60)
  CartWheel, HandcartWheel,         // wheel diameter (0.45-0.55)
  GraveMarker,                      // (x 1.2 at most of today's)
  COUNT
};
int holderPx(Holder h);   // art_props.cpp (HOLDERS); 0 = not measured yet
const char* holderName(Holder h);

// ---------------------------------------------------------------- the ink outer outline (STYLE_P2 6 and 8)
constexpr uint32_t kRigInk = 0xFF2A161Eu;   // Ink #1E162A (rgba(30, 22, 42)): never #000000
// (tests only: `art_hash --legacy-outline`) the character sheets painted with the pre-P2a selective outline, so the
// hashes prove the ink outline is the only pixel change of phase A. Never set in the game.
inline bool& outlineLegacyForTests() { static bool b = false; return b; }

}  // namespace art
