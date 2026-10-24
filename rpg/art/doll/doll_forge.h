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
                                          "crystal", "flame", "layered", "winged", "leaf"};
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

}  // namespace forge
}  // namespace doll
}  // namespace art
