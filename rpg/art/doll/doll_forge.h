// EMBERVALE art: forge styles' shared tables (P2c forge slice, FORGE-DOLL; docs/P2C_CONTRACTS.md 3c, docs/STYLE_P2.md 28).
// Header only, so the simulation (rpg/sim/gear_forge.cpp: the pick rule) and the doll (doll_resolve.cpp: flags, needs, the
// cut on the form's family) read ONE copy. A forge style is seven picks (shoulder crest cut edge collar limb fittings) and
// a base (0 generated: an alloy; 1-10 the materials' hero styles), packed a byte per axis into HumanLook::forge[slot]:
// byte 0 base, 1 shoulder, 2 crest, 3 cut, 4 edge, 5 collar, 6 limb, 7 fittings; an axis value is a 1-based index into its
// list (0: the form's own drawing); a packed 0 is no style (today's pixels). The lists are APPEND ONLY (flags and maps key
// on the words).
#pragma once
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace art {
namespace doll {
namespace forge {

enum Axis : int { AX_BASE, AX_SHOULDER, AX_CREST, AX_CUT, AX_EDGE, AX_COLLAR, AX_LIMB, AX_FIT, AX_COUNT };
enum Base : int { B_ALLOY, B_HOMESPUN, B_TANNER, B_FOUNDRY, B_WROUGHT, B_ARMOURER, B_PARADE, B_KNAPPED, B_FURNACE, B_STARFALL,
                  B_MYTHIC, B_COUNT };
// the armour families a cut reads on (STYLE_P2 28.3): plate (plate, brigandine, leaf), mail, scale, lamellar, padded, leather
enum Family : int { F_PLATE, F_MAIL, F_SCALE, F_LAMELLAR, F_PADDED, F_LEATHER, F_COUNT };

inline const char* const* axisList(int axis, int* n) {
  static const char* const kBase[] = {"alloy", "homespun", "tanner", "foundry", "wrought", "armourer", "parade", "knapped", "furnace",
                                      "starfall", "mythic"};
  static const char* const kShoulder[] = {"plate", "fur", "scale", "roll", "flap", "cop", "slab", "flared", "lobed", "shard", "vented",
                                          "crystal", "flame", "layered", "winged", "leaf",
                                          "pelt", "feather"};   // (forge round 2: the MANTLE's pelt and feathers, STYLE_P2 28.3)
  static const char* const kCrest[] = {"ridge", "comb", "knife", "brow", "crown", "shards", "chimney", "spire", "fin", "finial", "spike",
                                       "horns", "wings", "flame"};
  static const char* const kCut[] = {"smooth", "fluted", "keeled", "banded", "faceted", "muscled", "ribbed", "vented", "embossed",
                                     "splinted", "crackled", "plain", "plackart", "round", "pointed", "square", "laced", "staggered",
                                     "channel", "diamond", "brick", "lacedfront", "stitched", "studded", "tooled"};
  static const char* const kEdge[] = {"plain", "rolled", "scalloped", "dagged", "strips", "serrated", "spiked", "cusped", "pointed",
                                      "stepped", "fringed", "flame"};
  static const char* const kCollar[] = {"rolled", "stand", "torc", "riveted", "gorget", "flared", "angular", "furnace", "high", "ember",
                                        "ruff", "coif"};
  static const char* const kLimb[] = {"plain", "banded", "splinted", "scaled", "laced", "studded", "ribbed", "fluted", "faceted",
                                      "crackled"};
  static const char* const kFit[] = {"rivets", "studs", "bosses", "stars", "scored", "knot", "inlay", "lacing"};
#define EMB_FORGE_N(a) (int)(sizeof(a) / sizeof(a[0]))
  switch (axis) {
    case AX_BASE: *n = EMB_FORGE_N(kBase); return kBase;
    case AX_SHOULDER: *n = EMB_FORGE_N(kShoulder); return kShoulder;
    case AX_CREST: *n = EMB_FORGE_N(kCrest); return kCrest;
    case AX_CUT: *n = EMB_FORGE_N(kCut); return kCut;
    case AX_EDGE: *n = EMB_FORGE_N(kEdge); return kEdge;
    case AX_COLLAR: *n = EMB_FORGE_N(kCollar); return kCollar;
    case AX_LIMB: *n = EMB_FORGE_N(kLimb); return kLimb;
    case AX_FIT: *n = EMB_FORGE_N(kFit); return kFit;
    default: *n = 0; return nullptr;
  }
#undef EMB_FORGE_N
}
inline int axisCount(int axis) { int n = 0; axisList(axis, &n); return n; }
// an axis value's word: the base by its index (0 alloy), every other axis 1-based ("" for 0 or out of range)
inline const char* valueName(int axis, int v) {
  int n = 0;
  const char* const* l = axisList(axis, &n);
  if (!l) return "";
  if (axis == AX_BASE) return v >= 0 && v < n ? l[v] : "";
  return v >= 1 && v <= n ? l[v - 1] : "";
}
// a word's value on an axis (-1: none; the base 0-based, the rest 1-based)
inline int valueOf(int axis, const char* w) {
  int n = 0;
  const char* const* l = axisList(axis, &n);
  for (int i = 0; l && i < n; i++)
    if (!std::strcmp(l[i], w)) return axis == AX_BASE ? i : i + 1;
  return -1;
}
inline const char* axisName(int axis) {
  static const char* const k[AX_COUNT] = {"base", "shoulder", "crest", "cut", "edge", "collar", "limb", "fit"};
  return axis >= 0 && axis < AX_COUNT ? k[axis] : "";
}
inline int axisOfName(const char* w) {
  for (int a = 0; a < AX_COUNT; a++) if (!std::strcmp(axisName(a), w)) return a;
  if (!std::strcmp(w, "fittings")) return AX_FIT;
  return -1;
}

// packed: a byte per axis (0 base .. 7 fittings)
inline uint64_t pack(const uint8_t ax[AX_COUNT]) {
  uint64_t p = 0;
  for (int a = 0; a < AX_COUNT; a++) p |= (uint64_t)ax[a] << (8 * a);
  return p;
}
inline void unpack(uint64_t p, uint8_t ax[AX_COUNT]) { for (int a = 0; a < AX_COUNT; a++) ax[a] = (uint8_t)(p >> (8 * a)); }

// the cut values (1-based, kCut order)
enum Cut : int { C_NONE, C_SMOOTH, C_FLUTED, C_KEELED, C_BANDED, C_FACETED, C_MUSCLED, C_RIBBED, C_VENTED, C_EMBOSSED, C_SPLINTED,
                 C_CRACKLED, C_PLAIN, C_PLACKART, C_ROUND, C_POINTED, C_SQUARE, C_LACED, C_STAGGERED, C_CHANNEL, C_DIAMOND, C_BRICK,
                 C_LACEDFRONT, C_STITCHED, C_STUDDED, C_TOOLED, C_COUNT };
// the family a cut value belongs to (plate's 11, mail's plain and plackart, scale's 3, lamellar's laced and staggered, padded's
// 3, leather's 4; banded is plate's, mail's and lamellar's: native to all three)
inline bool cutNative(int cut, int fam) {
  switch (fam) {
    case F_PLATE: return cut >= C_SMOOTH && cut <= C_CRACKLED;
    case F_MAIL: return cut == C_PLAIN || cut == C_PLACKART || cut == C_BANDED;
    case F_SCALE: return cut == C_ROUND || cut == C_POINTED || cut == C_SQUARE;
    case F_LAMELLAR: return cut == C_LACED || cut == C_STAGGERED || cut == C_BANDED;
    case F_PADDED: return cut == C_CHANNEL || cut == C_DIAMOND || cut == C_BRICK;
    case F_LEATHER: return cut >= C_LACEDFRONT && cut <= C_TOOLED;
    default: return false;
  }
}
// a cut's character (how a generated alloy's cut reads on another family): 0 round, 1 vertical, 2 banded, 3 angular
inline int cutClass(int cut) {
  switch (cut) {
    case C_FLUTED: case C_RIBBED: case C_SPLINTED: case C_CHANNEL: case C_LACEDFRONT: case C_PLACKART: return 1;
    case C_BANDED: case C_VENTED: case C_SQUARE: case C_STUDDED: return 2;
    case C_FACETED: case C_KEELED: case C_POINTED: case C_STAGGERED: case C_DIAMOND: case C_TOOLED: return 3;
    default: return 0;
  }
}
// a generated cut on a family: itself where native, else that family's value of the same character
inline int cutOnFamily(int cut, int fam) {
  if (!cut || cutNative(cut, fam)) return cut;
  static const uint8_t kByClass[F_COUNT][4] = {
      {C_SMOOTH, C_FLUTED, C_BANDED, C_FACETED},          // plate
      {C_PLAIN, C_PLACKART, C_BANDED, C_PLACKART},        // mail (a plate cut over mail is the plackart)
      {C_ROUND, C_POINTED, C_SQUARE, C_POINTED},          // scale
      {C_LACED, C_STAGGERED, C_BANDED, C_STAGGERED},      // lamellar
      {C_BRICK, C_CHANNEL, C_CHANNEL, C_DIAMOND},         // padded
      {C_STITCHED, C_LACEDFRONT, C_STUDDED, C_TOOLED}};   // leather
  // mail: a plate cut other than smooth reads as the plackart (a breastplate over the mail in that cut)
  if (fam == F_MAIL && cut >= C_SMOOTH && cut <= C_CRACKLED && cut != C_SMOOTH && cut != C_BANDED) return C_PLACKART;
  return kByClass[fam][cutClass(cut)];
}
// the armour family of a doll armour word (doll_resolve.cpp armourOf: padded leather mail scale lamellar brigandine plate leaf)
inline int familyOfArmour(const char* a) {
  if (!a) return F_PLATE;
  if (!std::strcmp(a, "mail")) return F_MAIL;
  if (!std::strcmp(a, "scale")) return F_SCALE;
  if (!std::strcmp(a, "lamellar")) return F_LAMELLAR;
  if (!std::strcmp(a, "padded")) return F_PADDED;
  if (!std::strcmp(a, "leather")) return F_LEATHER;
  return F_PLATE;   // plate brigandine leaf
}
// cult::BodyArm (Padded Leather Mail Scale Lamellar Brigandine Plate Leaf) -> family
inline int familyOfBodyArm(int bodyArm) {
  static const uint8_t k[8] = {F_PADDED, F_LEATHER, F_MAIL, F_SCALE, F_LAMELLAR, F_PLATE, F_PLATE, F_PLATE};
  return bodyArm >= 0 && bodyArm < 8 ? k[bodyArm] : F_PLATE;
}

// The materials' hero styles (STYLE_P2 28.4's table): shoulder crest edge collar limb fittings, and the cut per family
// (plate mail scale lamellar padded leather). Index: Base (1..10; 0 unused).
struct Hero { uint8_t shoulder, crest, edge, collar, limb, fit; uint8_t cut[F_COUNT]; };
inline const Hero& hero(int base) {
  enum { S_PLATE = 1, S_FUR, S_SCALE, S_ROLL, S_FLAP, S_COP, S_SLAB, S_FLARED, S_LOBED, S_SHARD, S_VENTED, S_CRYSTAL, S_FLAME,
         S_LAYERED, S_WINGED, S_LEAF };
  enum { K_RIDGE = 1, K_COMB, K_KNIFE, K_BROW, K_CROWN, K_SHARDS, K_CHIMNEY, K_SPIRE, K_FIN, K_FINIAL, K_SPIKE, K_HORNS, K_WINGS,
         K_FLAME };
  enum { E_PLAIN = 1, E_ROLLED, E_SCALLOPED, E_DAGGED, E_STRIPS, E_SERRATED, E_SPIKED, E_CUSPED, E_POINTED, E_STEPPED, E_FRINGED,
         E_FLAME };
  enum { L_ROLLED = 1, L_STAND, L_TORC, L_RIVETED, L_GORGET, L_FLARED, L_ANGULAR, L_FURNACE, L_HIGH, L_EMBER, L_RUFF, L_COIF };
  enum { M_PLAIN = 1, M_BANDED, M_SPLINTED, M_SCALED, M_LACED, M_STUDDED, M_RIBBED, M_FLUTED, M_FACETED, M_CRACKLED };
  enum { T_RIVETS = 1, T_STUDS, T_BOSSES, T_STARS, T_SCORED, T_KNOT, T_INLAY, T_LACING };
  static const Hero k[B_COUNT] = {
      {0, 0, 0, 0, 0, 0, {0, 0, 0, 0, 0, 0}},
      // HOMESPUN: the quilt on every family it meets (padded); a metal form never takes it
      {S_ROLL, 0, E_ROLLED, L_ROLLED, M_PLAIN, T_LACING, {C_SMOOTH, C_PLAIN, C_ROUND, C_LACED, C_CHANNEL, C_STITCHED}},
      // TANNER'S
      {S_FLAP, K_RIDGE, E_DAGGED, L_STAND, M_LACED, T_STUDS, {C_SMOOTH, C_PLAIN, C_ROUND, C_LACED, C_CHANNEL, C_LACEDFRONT}},
      // FOUNDRY
      {S_COP, K_COMB, E_STRIPS, L_TORC, M_SCALED, T_BOSSES, {C_MUSCLED, C_PLACKART, C_ROUND, C_BANDED, C_BRICK, C_STITCHED}},
      // WROUGHT
      {S_SLAB, K_BROW, E_PLAIN, L_RIVETED, M_SPLINTED, T_RIVETS, {C_BANDED, C_BANDED, C_SQUARE, C_BANDED, C_CHANNEL, C_STUDDED}},
      // ARMOURER'S
      {S_FLARED, K_KNIFE, E_CUSPED, L_GORGET, M_FLUTED, 0, {C_FLUTED, C_PLACKART, C_POINTED, C_STAGGERED, C_CHANNEL, C_TOOLED}},
      // PARADE
      {S_LOBED, K_CROWN, E_SCALLOPED, L_FLARED, M_STUDDED, T_INLAY, {C_EMBOSSED, C_PLACKART, C_ROUND, C_LACED, C_DIAMOND, C_TOOLED}},
      // KNAPPED
      {S_SHARD, K_SHARDS, E_SERRATED, L_ANGULAR, M_FACETED, 0, {C_FACETED, C_PLACKART, C_POINTED, C_STAGGERED, C_DIAMOND, C_TOOLED}},
      // FURNACE
      {S_VENTED, K_CHIMNEY, E_STEPPED, L_FURNACE, M_BANDED, T_SCORED, {C_VENTED, C_PLACKART, C_SQUARE, C_BANDED, C_CHANNEL, C_STUDDED}},
      // STARFALL
      {S_CRYSTAL, K_SPIRE, E_POINTED, L_HIGH, M_RIBBED, T_STARS, {C_RIBBED, C_PLACKART, C_POINTED, C_STAGGERED, C_CHANNEL, C_LACEDFRONT}},
      // MYTHIC
      {S_FLAME, K_FLAME, E_FLAME, L_EMBER, M_CRACKLED, 0, {C_CRACKLED, C_PLACKART, C_ROUND, C_LACED, C_BRICK, C_STITCHED}}};
  return k[base >= 0 && base < B_COUNT ? base : 0];
}
// a hero style on a family, as packed axes
inline void heroAxes(int base, int fam, uint8_t ax[AX_COUNT]) {
  const Hero& h = hero(base);
  ax[AX_BASE] = (uint8_t)base;
  ax[AX_SHOULDER] = h.shoulder; ax[AX_CREST] = h.crest; ax[AX_CUT] = h.cut[fam >= 0 && fam < F_COUNT ? fam : 0];
  ax[AX_EDGE] = h.edge; ax[AX_COLLAR] = h.collar; ax[AX_LIMB] = h.limb; ax[AX_FIT] = h.fit;
}
// the cut a packed style draws on a family: a hero style's own column, a generated one by its character
inline int cutFor(const uint8_t ax[AX_COUNT], int fam) {
  if (ax[AX_BASE] >= B_HOMESPUN && ax[AX_BASE] < B_COUNT) return hero(ax[AX_BASE]).cut[fam];
  return cutOnFamily(ax[AX_CUT], fam);
}
// how different two styles are (contracts 3c): shoulder 3, cut 3, crest 2, edge 2, collar 1, limb 1, fittings 1 (of 13)
inline int distance(const uint8_t a[AX_COUNT], const uint8_t b[AX_COUNT]) {
  static const int w[AX_COUNT] = {0, 3, 2, 3, 2, 1, 1, 1};
  int d = 0;
  for (int i = 1; i < AX_COUNT; i++) if (a[i] != b[i]) d += w[i];
  return d;
}
// the silhouette axes (shoulder, crest, edge, collar) that differ
inline int silhouetteDiffs(const uint8_t a[AX_COUNT], const uint8_t b[AX_COUNT]) {
  return (a[AX_SHOULDER] != b[AX_SHOULDER]) + (a[AX_CREST] != b[AX_CREST]) + (a[AX_EDGE] != b[AX_EDGE]) + (a[AX_COLLAR] != b[AX_COLLAR]);
}

// (forge i3; the blind panel: "about 20 of the 37 alloys wear the same kettle helm on the same torso and legs, told apart
// only by colour"; "give each culture its own helm and body silhouette: torso, skirt, pauldron size, cape, closed vs open
// face") a culture alloy's GUISE: the silhouette its armour is made in, beyond the seven picks: the helm (HelmForm + 1 of a
// drawn helm: 1 nasal 2 kettle 3 great helm 4 spangen 8 masked 9 crested), the skirt (2 short 3 mid 4 knee), the cloak
// (0 none 2 fur mantle 3 long noble cloak) and the shoulders' size (2 small 3 medium 4 large). gear::alloyGuise picks it per
// culture (the culture's alloys never share a helm); HumanLook::forgeGuise carries it packed (0 none: today's pixels);
// doll_resolve applies it at rig 2 (styleKitOf the helm, ladderLook the dials; the tier caps still apply).
struct Guise { uint8_t helm = 0, skirt = 0, cloak = 0, pauldron = 0; };
// the guise's metal head shapes drawn from the headwear library in the armour's own metal ramp: 11 the mail coif (the hood's
// shape), 12 the turban helm, 13 the jingasa (the conical war hat), 14 the aventail (the veil's shape); the cult::Headwear
// index each borrows (0: a helm, not a headwear)
inline int guiseHeadwear(int helm) { return helm == 11 ? 1 : helm == 12 ? 3 : helm == 13 ? 7 : helm == 14 ? 5 : 0; }
inline uint16_t packGuise(const Guise& g) {
  return (uint16_t)((g.helm & 15u) | (g.skirt & 7u) << 4 | (g.cloak & 3u) << 7 | (g.pauldron & 7u) << 9 | 1u << 15);
}
inline Guise unpackGuise(uint16_t p) {
  Guise g;
  if (!(p >> 15)) return g;
  g.helm = (uint8_t)(p & 15u); g.skirt = (uint8_t)(p >> 4 & 7u); g.cloak = (uint8_t)(p >> 7 & 3u); g.pauldron = (uint8_t)(p >> 9 & 7u);
  return g;
}

// ======================================================================================== (forge round 2) the silhouette
// docs/P2C_CONTRACTS.md 3c.1, docs/STYLE_P2.md 28.3 and 28.5. Six axes read at 1x with the colour off, drawn once and
// composed: the helm family (the helm piece), the skirt, the shoulder mass, the over-layer, the arm bulk and the chest motif
// (the body piece). Every list is 1-based (0: unset) and APPEND ONLY (flags `helmfam-<v>` `skirt-<v>`... and the parts' role
// values key on the words). HumanLook::forgeShape packs them (helm bits 0-4, skirt 5-7, mass 8-10, layer 11-13, arm 14-15
// stored as arm - 1 (0: close, so an unset arm reads as the close arm), motif 16-19, bit 31 set); compare packed values,
// never memcmp a struct.
enum ShapeAxis : int { SX_HELM, SX_SKIRT, SX_MASS, SX_LAYER, SX_ARM, SX_MOTIF, SX_COUNT };
enum HelmFam : int { H_UNSET, H_CONE, H_KETTLE, H_WARHAT, H_GREAT, H_MASK, H_CRESTED, H_ONION, H_VEIL, H_HORNED, H_WINGED, H_SALLET,
                     H_SNOUT, H_KABUTO, H_MORION, H_PLUMED, H_CROWN, H_HOOD, H_CAP, H_FURNACE, H_CRYSTAL, H_LADDER, H_COUNT };
constexpr int H_ALLOY_LAST = H_CROWN;   // cone..crown: the alloys' families; hood cap furnace crystal: hero-only; ladder: T5-T6
enum SkirtV : int { SK_UNSET, SK_NONE, SK_TASSETS, SK_STRIPS, SK_HAUBERK, SK_SPLIT, SK_BELL, SK_COUNT };
enum MassV : int { MS_UNSET, MS_NONE, MS_ROUND, MS_BROAD, MS_TALL, MS_DROP, MS_MANTLE, MS_COUNT };
enum LayerV : int { LY_UNSET, LY_NONE, LY_TABARD, LY_SASH, LY_HALFCAPE, LY_CAPE, LY_SCARF, LY_COUNT };
enum ArmV : int { AR_UNSET, AR_CLOSE, AR_BARE, AR_WIDE, AR_HEAVY, AR_COUNT };
enum MotifV : int { MO_UNSET, MO_PLAIN, MO_DISC, MO_STRAPS, MO_BANDS, MO_CHEVRON, MO_KEEL, MO_BIB, MO_RAYS, MO_BEAST, MO_CRESCENT, MO_COUNT };

inline const char* const* shapeList(int axis, int* n) {
  static const char* const kHelm[] = {"cone", "kettle", "warhat", "great", "mask", "crested", "onion", "veil", "horned", "winged", "sallet",
                                      "snout", "kabuto", "morion", "plumed", "crown", "hood", "cap", "furnace", "crystal", "ladder"};
  static const char* const kSkirt[] = {"none", "tassets", "strips", "hauberk", "split", "bell"};
  static const char* const kMass[] = {"none", "round", "broad", "tall", "drop", "mantle"};
  static const char* const kLayer[] = {"none", "tabard", "sash", "halfcape", "cape", "scarf"};
  static const char* const kArm[] = {"close", "bare", "wide", "heavy"};
  static const char* const kMotif[] = {"plain", "disc", "straps", "bands", "chevron", "keel", "bib", "rays", "beast", "crescent"};
#define EMB_SHAPE_N(a) (int)(sizeof(a) / sizeof(a[0]))
  switch (axis) {
    case SX_HELM: *n = EMB_SHAPE_N(kHelm); return kHelm;
    case SX_SKIRT: *n = EMB_SHAPE_N(kSkirt); return kSkirt;
    case SX_MASS: *n = EMB_SHAPE_N(kMass); return kMass;
    case SX_LAYER: *n = EMB_SHAPE_N(kLayer); return kLayer;
    case SX_ARM: *n = EMB_SHAPE_N(kArm); return kArm;
    case SX_MOTIF: *n = EMB_SHAPE_N(kMotif); return kMotif;
    default: *n = 0; return nullptr;
  }
#undef EMB_SHAPE_N
}
inline int shapeCount(int axis) { int n = 0; shapeList(axis, &n); return n; }
inline const char* shapeName(int axis, int v) {   // "" for 0 or out of range
  int n = 0;
  const char* const* l = shapeList(axis, &n);
  return l && v >= 1 && v <= n ? l[v - 1] : "";
}
inline int shapeValueOf(int axis, const char* w) {   // -1: no such word
  int n = 0;
  const char* const* l = shapeList(axis, &n);
  for (int i = 0; l && i < n; i++) if (!std::strcmp(l[i], w)) return i + 1;
  return -1;
}
inline const char* shapeAxisName(int axis) {
  static const char* const k[SX_COUNT] = {"helm", "skirt", "mass", "layer", "arm", "motif"};
  return axis >= 0 && axis < SX_COUNT ? k[axis] : "";
}
inline int shapeAxisOf(const char* w) {
  for (int a = 0; a < SX_COUNT; a++) if (!std::strcmp(shapeAxisName(a), w)) return a;
  return -1;
}
inline uint32_t packShapeAxes(const uint8_t v[SX_COUNT]) {
  const uint32_t arm = v[SX_ARM] >= 1 ? (uint32_t)(v[SX_ARM] - 1) & 3u : 0u;
  return (v[SX_HELM] & 31u) | (v[SX_SKIRT] & 7u) << 5 | (v[SX_MASS] & 7u) << 8 | (v[SX_LAYER] & 7u) << 11 | arm << 14 |
         (v[SX_MOTIF] & 15u) << 16 | 1u << 31;
}
inline void unpackShapeAxes(uint32_t p, uint8_t v[SX_COUNT]) {
  for (int a = 0; a < SX_COUNT; a++) v[a] = 0;
  if (!(p >> 31)) return;
  v[SX_HELM] = (uint8_t)(p & 31u); v[SX_SKIRT] = (uint8_t)(p >> 5 & 7u); v[SX_MASS] = (uint8_t)(p >> 8 & 7u);
  v[SX_LAYER] = (uint8_t)(p >> 11 & 7u); v[SX_ARM] = (uint8_t)((p >> 14 & 3u) + 1); v[SX_MOTIF] = (uint8_t)(p >> 16 & 15u);
}
// a closed family (STYLE_P2 28.2: a dark visor band 2 px or more across the eye row, a breath slot or grille below it)
// (P2c envelope, contracts 3d, D51) a helm family's VARIANT (0 = the family's base drawing, 1..7): HumanLook::forgeShape bits
// 20-22 (bit 31 still marks a set value). The head key is (family, variant, crest). The fix round's `when helmfam-<v>,<mass>`
// views are re-keyed `when helmfam-<v>,variant-<n>` with their pixels unchanged; the numbering, in each map's order:
//   warhat 1 the great kasa, 2 the gat | great 1 the barrel, 2 the sugarloaf | crested 1 the burgonet | crown 1 the sun crown |
//   plumed 1 the centurion, 2 the fountain, 3 the mohawk, 4 the fall | snout 1 the hounskull on a mail bell, 2 the tall
//   hounskull, 3 the rondel bascinet | veil 1 the spire, 2 the turban | winged 1 the eagle, 2 the fin, 3 the messenger |
//   kabuto 1 the crescent kabuto | onion 1 the kulah (tools/doll/rekey_variants.mjs re-keys the maps, crest placements too)
// and new drawings the owner's board asks for (FORGE-HEAD): horned 1, crown 2-3, morion 1.
inline uint32_t shapeVariant(uint32_t p) { return p >> 31 ? (p >> 20) & 7u : 0u; }
inline uint32_t withShapeVariant(uint32_t p, int v) { return p ? (p & ~(7u << 20)) | ((uint32_t)(v & 7) << 20) : p; }
inline uint32_t headKeyOf(int helm, int variant, int crest) { return (uint32_t)(helm & 31) | (uint32_t)(variant & 7) << 5 | (uint32_t)(crest & 255) << 8; }
// the variants a family's key may name (drawn or asked of FORGE-HEAD; `--doll-coverage --scope envelope` lists the undrawn)
inline int helmVariantCount(int h) {
  switch (h) {
    case H_WARHAT: case H_GREAT: case H_VEIL: return 2;
    case H_CRESTED: case H_HORNED: case H_MORION: case H_KABUTO: case H_ONION: return 1;
    case H_CROWN: case H_SNOUT: case H_WINGED: return 3;
    case H_PLUMED: return 4;
    default: return 0;
  }
}
// the variant the fix round's mass keys drew (the re-key's translation, and every look that is not a culture alloy's helm:
// the hero styles' plumed and winged helms keep the views their mass picked, so no approved pixel moves). alloyBody: the
// `alloyshape` flag (most of the fix round's views asked for it)
inline int legacyHelmVariant(int h, int mass, bool alloyBody) {
  auto in = [&](std::initializer_list<int> ms) { for (int m : ms) if (m == mass) return true; return false; };
  switch (h) {
    case H_PLUMED:
      if (mass == MS_ROUND) return 1;
      if (mass == MS_DROP) return 4;
      if (alloyBody && in({MS_NONE, MS_MANTLE})) return 2;
      if (alloyBody && in({MS_BROAD, MS_TALL})) return 3;
      return 0;
    case H_WINGED:
      if (mass == MS_TALL) return 2;
      if (alloyBody && in({MS_NONE, MS_MANTLE})) return 1;
      if (alloyBody && in({MS_ROUND, MS_DROP})) return 3;
      return 0;
    default: break;
  }
  if (!alloyBody) return 0;
  switch (h) {
    case H_WARHAT: return in({MS_BROAD, MS_TALL}) ? 1 : in({MS_NONE, MS_MANTLE}) ? 2 : 0;
    case H_GREAT: return in({MS_NONE, MS_ROUND, MS_MANTLE}) ? 1 : in({MS_BROAD, MS_TALL, MS_DROP}) ? 2 : 0;
    case H_CRESTED: return in({MS_NONE, MS_MANTLE}) ? 1 : 0;
    case H_CROWN: return in({MS_NONE, MS_MANTLE, MS_TALL}) ? 1 : 0;
    case H_SNOUT: return in({MS_NONE, MS_MANTLE}) ? 1 : in({MS_TALL, MS_DROP}) ? 2 : in({MS_BROAD, MS_ROUND}) ? 3 : 0;
    case H_VEIL: return in({MS_DROP, MS_TALL}) ? 1 : in({MS_NONE, MS_ROUND, MS_BROAD, MS_MANTLE}) ? 2 : 0;
    case H_KABUTO: return in({MS_BROAD, MS_DROP}) ? 1 : 0;
    case H_ONION: return in({MS_NONE, MS_ROUND, MS_BROAD, MS_MANTLE}) ? 1 : 0;
    default: return 0;
  }
}

inline bool helmClosed(int h) { return h == H_GREAT || h == H_MASK || h == H_SNOUT || h == H_FURNACE || h == H_CRYSTAL; }
// a family whose map carries a crest or plume socket (the details' crest sits there; contracts 3c.1)
inline bool helmSocket(int h) {
  return h == H_CONE || h == H_KETTLE || h == H_GREAT || h == H_CRESTED || h == H_ONION || h == H_SALLET || h == H_SNOUT ||
         h == H_KABUTO || h == H_MORION || h == H_PLUMED || h == H_CROWN || h == H_WARHAT;
}

// ---------------------------------------------------------------- kShapeVocab: STYLE_P2 28.5 exactly
// Per people (cult::Archetype order) and axis, the menu in typical-first order (the first the heaviest, each next 13/16 of
// the one before); '*' marks the people's signature (a value no other people wears, or wears only by its own hand)
struct ShapeVocab { const char* ax[SX_COUNT]; };
inline const ShapeVocab& shapeVocab(int people) {
  static const ShapeVocab k[12] = {
      {{"*horned cone veil kettle mask crested", "hauberk split none", "mantle round drop broad", "cape halfcape scarf none", "bare heavy close",
        "bands disc bib keel"}},                                                                                                // Fjordfolk
      {{"kettle veil cone snout morion sallet", "*strips hauberk none", "broad mantle round none", "*sash halfcape cape none", "bare close heavy",
        "straps bands disc plain"}},                                                                                            // Highland
      {{"*great sallet snout crown cone kettle", "tassets hauberk none", "tall broad round", "*tabard cape halfcape none", "heavy close",
        "keel disc rays plain"}},                                                                                               // Heartland
      {{"*crested mask winged crown plumed kettle", "*strips tassets none", "broad drop round tall", "*halfcape cape sash none",
        "close bare heavy", "bands disc rays keel"}},                                                                           // Imperial
      {{"*onion veil mask cone plumed winged", "*split hauberk bell none", "round drop tall none", "sash *scarf halfcape none", "*wide bare heavy",
        "disc rays bands chevron"}},                                                                                            // Dune
      {{"*plumed cone veil mask onion", "split bell strips none", "*drop mantle round broad", "scarf halfcape sash none", "close wide heavy",
        "bib disc straps chevron"}},                                                                                            // Steppe
      {{"warhat kettle cone mask crested plumed", "strips split bell none", "drop none round broad", "halfcape scarf sash none", "bare close wide",
        "bands chevron bib plain"}},                                                                                            // Marsh
      {{"*kabuto warhat mask crested winged crown", "*bell split tassets none", "*drop tall round none", "tabard sash cape none",
        "*wide heavy close", "bib disc bands chevron"}},                                                                        // Jade
      {{"sallet *morion kettle cone great snout", "tassets hauberk split none", "broad round tall none", "tabard sash halfcape none",
        "heavy close wide", "keel bands disc straps"}},                                                                         // River
      {{"plumed *mask winged crown crested", "*bell strips split none", "tall round mantle none", "*cape halfcape sash none", "bare heavy close",
        "*rays disc chevron bands"}},                                                                                           // SunTemple
      {{"winged crested horned mask crown plumed", "split bell strips none", "drop tall round none", "cape halfcape scarf none", "close bare wide",
        "chevron keel rays plain"}},                                                                                            // Sylvan
      {{"winged *crown crested plumed mask sallet", "split bell tassets none", "*tall round drop none", "cape halfcape scarf none",
        "close wide heavy", "rays disc chevron keel"}},                                                                         // Starspire
  };
  return k[people >= 0 && people < 12 ? people : 2];
}
// a people's menu on an axis as values (1-based) with weights (the first 256, each next x13/16) and signature marks;
// returns the count
inline int shapeMenu(int people, int axis, uint8_t* vals, int* weights, bool* star, int cap) {
  const char* s = shapeVocab(people).ax[axis];
  int n = 0, w = 256;
  char word[24];
  for (const char* p = s; *p && n < cap;) {
    while (*p == ' ') p++;
    if (!*p) break;
    bool st = false;
    if (*p == '*') { st = true; p++; }
    int k = 0;
    while (*p && *p != ' ' && k < 23) word[k++] = *p++;
    word[k] = 0;
    const int v = shapeValueOf(axis, word);
    if (v > 0) { vals[n] = (uint8_t)v; if (weights) weights[n] = w; if (star) star[n] = st; n++; w = w * 13 / 16; }
  }
  return n;
}
inline bool inShapeMenu(int people, int axis, int v) {
  uint8_t vals[8];
  const int n = shapeMenu(people, axis, vals, nullptr, nullptr, 8);
  for (int i = 0; i < n; i++) if (vals[i] == v) return true;
  return false;
}
// the rank flavour (STYLE_P2 28.5: weights, not walls): the everyday plain, the warriors' martial, the masterwork elaborate;
// a x1.5 on the listed values ("axis:word"). Only the CAPE is held to masterworks (PLAN D38: a wall, not a weight)
inline const char* shapeRankFlavour(int rank) {
  static const char* const k[3] = {
      "layer:none layer:tabard mass:round mass:none skirt:tassets skirt:strips skirt:hauberk",
      "mass:broad mass:drop mass:tall layer:sash layer:halfcape skirt:split skirt:bell",
      "mass:mantle mass:tall layer:cape layer:halfcape skirt:bell skirt:split"};
  return k[rank >= 0 && rank < 3 ? rank : 0];
}
// the shoulder's shape inside its mass (the details; contracts 3c.1 kMassShapes): the round-1 shoulder values (AX_SHOULDER,
// 1-based) each mass may draw, typical first
inline int massShapes(int mass, uint8_t out[5]) {
  // shoulder values: 1 plate 2 fur 3 scale 4 roll 5 flap 6 cop 7 slab 8 flared 9 lobed 10 shard 11 vented 12 crystal 13 flame
  // 14 layered 15 winged 16 leaf 17 pelt 18 feather
  static const uint8_t k[MS_COUNT][5] = {{0}, {0}, {6, 9, 4, 0, 0}, {7, 11, 1, 0, 0}, {15, 8, 12, 10, 13}, {14, 3, 5, 16, 0}, {2, 17, 18, 0, 0}};
  int n = 0;
  if (mass > 0 && mass < MS_COUNT) for (int i = 0; i < 5 && k[mass][i]; i++) out[n++] = k[mass][i];
  return n;
}
// the mass a round-1 shoulder value belongs to (MS_NONE for 0)
inline int massOfShoulder(int sh) {
  for (int m = MS_ROUND; m < MS_COUNT; m++) {
    uint8_t s[5];
    const int n = massShapes(m, s);
    for (int i = 0; i < n; i++) if (s[i] == sh) return m;
  }
  return MS_NONE;
}
// a body form's own hang as a skirt value (cult::BodyArm: Padded Leather Mail Scale Lamellar Brigandine Plate Leaf): the
// materials' skirt where the style keeps the form's hang
inline int skirtOfBodyArm(int bodyArm) {
  static const uint8_t k[8] = {SK_HAUBERK, SK_STRIPS, SK_HAUBERK, SK_HAUBERK, SK_HAUBERK, SK_TASSETS, SK_TASSETS, SK_TASSETS};
  return bodyArm >= 0 && bodyArm < 8 ? k[bodyArm] : SK_TASSETS;
}
// a people's helm form (cult::HelmForm: Nasal Kettle GreatHelm Spangen Horned Plumed ConicalAventail Masked Crested Winged) as
// a family
inline int helmFamOfForm(int helmForm) {
  static const uint8_t k[10] = {H_CONE, H_KETTLE, H_GREAT, H_CONE, H_HORNED, H_PLUMED, H_VEIL, H_MASK, H_CRESTED, H_WINGED};
  return helmForm >= 0 && helmForm < 10 ? k[helmForm] : H_CONE;
}

// ---------------------------------------------------------------- design estimates (g1's first solve; contracts 3c.1)
// The outline difference two values make at 1x, in pixels of the figure's opaque mask (idle D, idle R and the walk's passing
// frame R averaged), from PLAN's constants ("silhouettes: what each axis is worth"). The measured solve (rpg_test
// --forge-solve on the delivered pool) replaces them.
inline int estAxisPx(int axis, int a, int b) {
  if (a == b) return 0;
  if (a > b) { const int t = a; a = b; b = t; }
  switch (axis) {
    case SX_SKIRT: {   // none tassets strips hauberk split bell
      static const uint8_t k[7][7] = {{0}, {0, 0, 22, 26, 36, 46, 50}, {0, 0, 0, 12, 22, 34, 36}, {0, 0, 0, 0, 18, 28, 30},
                                      {0, 0, 0, 0, 0, 16, 22}, {0, 0, 0, 0, 0, 0, 20}, {0}};
      return k[a][b];
    }
    case SX_MASS: {    // none round broad tall drop mantle
      static const uint8_t k[7][7] = {{0}, {0, 0, 10, 22, 18, 16, 24}, {0, 0, 0, 12, 12, 12, 16}, {0, 0, 0, 0, 14, 16, 14},
                                      {0, 0, 0, 0, 0, 18, 16}, {0, 0, 0, 0, 0, 0, 18}, {0}};
      return k[a][b];
    }
    case SX_LAYER: {   // none tabard sash halfcape cape scarf (a tabard and a sash lie inside the outline: a few hem pixels)
      static const uint8_t k[7][7] = {{0}, {0, 0, 4, 2, 18, 36, 8}, {0, 0, 0, 4, 18, 34, 10}, {0, 0, 0, 0, 18, 34, 8},
                                      {0, 0, 0, 0, 0, 22, 18}, {0, 0, 0, 0, 0, 0, 32}, {0}};
      return k[a][b];
    }
    case SX_ARM: {     // close bare wide heavy
      static const uint8_t k[5][5] = {{0}, {0, 0, 1, 8, 6}, {0, 0, 0, 8, 6}, {0, 0, 0, 0, 8}, {0}};
      return k[a][b];
    }
    default: return 0;
  }
}
// a family's outline features (px): rise over the head box, flare past it each side, brim, face cover, tail behind
inline void helmFeatures(int h, int f[5]) {
  static const int8_t k[H_COUNT][5] = {
      {0, 0, 0, 0, 0}, {3, 0, 0, 2, 0}, {1, 0, 2, 0, 0}, {2, 0, 4, 0, 0}, {1, 0, 0, 10, 0}, {1, 0, 0, 10, 1}, {3, 0, 0, 0, 1},
      {5, 0, 0, 0, 0}, {3, 1, 0, 6, 4}, {3, 3, 0, 0, 0}, {3, 2, 0, 0, 0}, {1, 0, 0, 4, 3}, {2, 0, 0, 8, 2}, {1, 3, 0, 0, 1},
      {4, 0, 2, 0, 3}, {5, 0, 0, 0, 1}, {3, 1, 0, 0, 0}, {1, 1, 0, 3, 2}, {1, 0, 0, 0, 0}, {2, 1, 0, 10, 0}, {6, 1, 0, 10, 0},
      {2, 0, 0, 2, 0}};
  for (int i = 0; i < 5; i++) f[i] = k[h >= 0 && h < H_COUNT ? h : 0][i];
}
inline double estHeadOD(int a, int b) {
  if (a == b) return 0.0;
  int fa[5], fb[5];
  helmFeatures(a, fa); helmFeatures(b, fb);
  static const int w[5] = {6, 8, 6, 3, 4};
  int px = 8;   // two families always differ in their dome's own line
  for (int i = 0; i < 5; i++) px += w[i] * (fa[i] > fb[i] ? fa[i] - fb[i] : fb[i] - fa[i]);
  return px / (110.0 + px * 0.5);
}
inline double estBodyOD(const uint8_t a[SX_COUNT], const uint8_t b[SX_COUNT]) {
  int px = 0;
  for (int ax : {(int)SX_SKIRT, (int)SX_MASS, (int)SX_LAYER, (int)SX_ARM}) px += estAxisPx(ax, a[ax], b[ax]);
  return px / (260.0 + px * 0.5);
}

}  // namespace forge
}  // namespace doll
}  // namespace art
