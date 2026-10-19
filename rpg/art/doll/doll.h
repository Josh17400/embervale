// EMBERVALE art: the DOLL contract (P2b phase A, lead; VISION_PLAN 15.36 "The production method: the doll"; the
// artists' rulebook is docs/STYLE_P2.md section 24). FROZEN after phase A: areas ADD names, enum values (at the end, before
// COUNT) and functions; never rename, reorder or change a meaning. A harness problem goes in the area's report.
//
// A person at rig 2 is composed at bake time from hand-drawn part maps in layers (a paper doll), recoloured by ramps.
// Nothing here draws a limb, a face or a fold: every pixel comes from a .pxm map an artist placed.
//
//   maps (.pxm, rpg/art/doll/maps/**)  --tools/doll/pxc.mjs-->  rpg/art/doll/gen/doll_pool.cpp (the pool, embedded)
//   clips (.clip, rpg/art/doll/clips/)  ----------^                         |
//   HumanLook --resolve()--> DollRecipe (body set, parts, ramps, regions, flags) --compose()--> RigSheet (art_rig.h)
//
// Who owns what (VISION_PLAN 15.36 P2b areas): this header and the harness (tools/tests/test_doll.cpp) are the lead's;
// the .cpp files next to it, gen/ and tools/doll/* are DOLL's; the maps and clips are the artist areas' folders.
//   doll_pool.cpp      the pool: loading (embedded, or a file for tools), parsing, the part / view / clip lookups
//   doll_compose.cpp   the compositor: placement, layers, erase, regions, `when` overlays, build masks, stretch rows,
//                      sways, marks -> RigAnchors, the finishing pass (A3's selout), the atlas packer (P2a's SheetBuilder)
//   doll_resolve.cpp   the resolver (HumanLook -> DollRecipe), coverage (dollCovers / dollMissing), the role vocabulary
//   doll_ramps.cpp     the ramps: rampA3 (the generator calibrated on A3), the curated skin, hair, eye, linen, leather and
//                      metal tables, and a look's DollRamps
//   doll_fit.cpp       seatFit2 / bedFit2 (moved from the procedural rig with their numbers unchanged), the sleeper canvas
//
// ======================================================================================== The .pxm format (version 1)
// One part per file, UTF-8 text, under rpg/art/doll/maps/. A line starting with '|' is a grid row (one character per
// pixel; a second '|' starts a comment); a line starting with "# " (or a lone "#", or "//") is a comment; every other
// non-blank line is a directive. Directives before the first `view` describe the part:
//   part <name>                  the part's id (default: its path under maps/ without ".pxm", e.g. "hair/adult/tousled")
//   use <legend> [...]           characters from maps/legend/<legend>.legend (later ones override earlier ones)
//   legend <char> <slot> <step>  a local legend entry (overrides the used legends): slot by name (Slot), step 0-4
//   body <set> [...]             the body sets it serves: man woman youth child adult (= man woman); default from the
//                                path (".../man/...", ".../woman/...", "youth/", "child/"; else adult)
//   role <category> <value>      what look value it serves (Role, by name; value: the resolver's vocabulary, below);
//                                repeatable. EVERY part of the recipe's roles is drawn: a hair style may be several files
//                                (back, front, tuft, hang), each with `role hair <style>`
//   layer <layer>                its layer (Layer, by name; default by its first role)
//   anchor <anchor>              the anchor its origin lands on (Anchor, by name; default by layer)
//   keys <channel>               the clip channel that picks its key per frame (default by layer: legs / shoes -> legs,
//                                arms -> armR / armL by side, hangs -> hang, cloaks -> cloak, held -> held, face -> expr,
//                                fx -> fx, everything else -> rest = rigid, drawn with key "rest" on every frame)
//   side R|L                     an arm's side (the body's right or left): near or far by the facing
//   z <n>                        order inside its layer (default 0; then the part's name)
//   sway <rows>                  a hang: the bottom <rows> rows move by the frame's sway (x) (derived sways)
//   tags <word> [...]            free words: `draft` (pxc --strict spares it), `overlay` (no orphan check), `mirror-ok`,
//                                (DOLL, P2b) `tile` (a pattern tile: its view (D serves every facing) is repeated over the
//                                garment parts' cloth1 pixels, `on-<slot>` another slot; each tile cell naming a slot recolours
//                                the cloth pixel under it to that slot KEEPING the cloth's step (so the shading runs through),
//                                outlines untouched; the tile rides the garment's anchor; '.' keeps the cloth)
// Then one block per view:
//   view <D|U|R|L> [key <k>] [origin <x> <y>] [layer <l>] [anchor <a>] [when <cond> [replace]] [stretch <row>]
//     key      the key (default "rest"); legs and arms per pose key (idle, walkA..., run1-6, action and posture keys),
//              hangs per sway key (rest, wide, flare, sit, floor, kneel, ride), hair hangs per lag key, tufts per bob key
//     origin   where the anchor lands, in the grid's own coordinates (may lie outside the grid); required on base views
//     layer / anchor   this view's own (the scabbard behind the legs facing right, in front facing left)
//     when     an OPTIONAL REGION: this grid overlays the base view of the same facing and key only when the look's
//              recipe holds <cond>: a flag, "!flag", or flags joined by ',' (all must hold). Origin defaults to the base's
//     replace  (DOLL, P2b) with `when`: drawn INSTEAD of the base view when the condition holds (a capped hair style
//              under a hat, a kilt's bare knee): the first replacing view whose condition holds wins
//     stretch  the leg drawing's stretch row: doubled for elves (rows on the far side of the origin move out by 1)
//   mark <name> <x> <y>          a named point in this view's grid (face, handR, handL, grip, tip, footL, footR, hip, top):
//                                the compositor turns marks into RigAnchors, and `grip` / `gripL` into anchors for held
//                                items and shields
//   |<row>                       the grid: '.' (or ' ') keeps what is below, '-' ERASES what is below, every other
//                                character goes through the legend to (slot, step)
//   regions                      followed by '|' rows the grid's size: a region character per pixel ('.' none):
//                                U upper arm, F forearm, C cuff, H hand, X gauntlet flare; T thigh, K knee, S shin, B boot,
//                                O sole; E garment edge. The look maps a region to a slot (a short sleeve: F -> skin),
//                                keeping each pixel's drawn step; unmapped regions draw as drawn
//   build                        followed by ONE '|' row the grid's width: '+' a column broad doubles, 'x' a column slim
//                                removes ('.' else). Columns move away from (broad) or toward (slim) the origin's column
// Rules the compiler enforces (pxc.mjs; errors fail the build's --check): every character in the legend, origins on base
// views, one view per (facing, key, when), a `when` view has its base, a left view is never the plain mirror of its
// right view (FX excepted). Craft warnings (errors with --strict, `draft` parts spared): orphan pixels (all eight
// neighbours clear), the top-left light (each material's left half of its own pixels at least as light by step as its
// right half), a key missing a facing, a part no role uses.
//
// Legends (maps/legend/*.legend): `legend <char> <slot> <step>` lines. person.legend keeps A3's hero characters
// (K ink, # lash, @ eye white, M mouth, L lip, Y blush, E iris, G glint; a-e hair, q-u skin, 5-9 cloth1, 1-4 cloth2,
// m-p accent, f-j leather, v-z metal, N-P metal2 1-3, C B A the arc's edge, body and core); woman.legend adds the
// woman's (v-y linen, A-D cloth3, R S accent 1 and 3).
//
// ======================================================================================== The clip format (.clip)
// rpg/art/doll/clips/*.clip, line based ('#' comments):
//   body <man|woman|youth|child>[.<modifier>]   the body set of the lines below ("man.elder": the elder's anchors and
//                                clips; a lookup falls back to the base set for whatever the modifier set lacks)
//   anchors <D|U|R|L> <name>=<x>,<y> ...         the body set's anchors, pivot-relative (x right, y down; the pivot is
//                                the point between the feet on the sole row F; the man's head box top-left -8,-33)
//   clip <name> <D|U|R|L> [part <loco|action|death|run|posture>] [family <f>] [variant <n>] [posture <p>] [cycle <px>]
//                                name: art::clipName (idle walk hurt windup strike follow cast draw release block roll
//                                fall lying) or `run` (= walk in the RUN part). part defaults by name (idle/walk/hurt
//                                loco; fall/lying death; posture when `posture` is given; else action). family: the
//                                ACTION family (art::familyName) the clip is for (default: any). cycle: the walk / run
//                                cycle in px when the frames give no plant
//   f <channel>=<key> ... [bob=<n>] [lean=<n>] [hip=<dx>,<dy>] [head=<dx>,<dy>] [sway=<n>] [ms=<n>]
//     [weapon=<deg>,<front|back>] [plant=<x0>:<x1>] [turn=<west|east>] [at=<dx>,<dy>]
//                                one frame. <channel>=<key> picks the key of the parts on that channel; "-" draws none of
//                                them; a channel the frame does not name draws its parts' "rest" view (if any). Channels
//                                are free words; the standard ones: legs armR armL hang hair tuft neck belt sheath cloak
//                                held torso expr fx. Moves (art px): bob moves every anchor but the pivot (y); lean moves
//                                the neck, head and shoulders (x); hip= and head= add to those anchors. sway moves a hang's
//                                bottom rows (x). ms: the frame's time where the clip is timed (swings, idles).
//                                plant: the planted foot's x (pivot-relative, forward +) at the frame's start and end
//                                (walkTable derives each frame's travel from it)
//                                (DOLL, pool v2) turn: the frame is composed in the LEFT view (whatever the clip's facing),
//                                then laid down (STYLE_P2 13): east = a quarter turn clockwise (head east), west = that turn
//                                mirrored (head west, the top-left light kept), and finished again; its pivot sits under the
//                                middle of the body (halfway from the soles to the head box's top) on the figure's lowest row
//                                (its back). at: the turned figure's offset from that rest (the topple: 2 px up, 2 px toward
//                                where the body stood). A `lying` clip's frames turn by its variant when they name no turn
//                                (0 west, 1 east), and a lying clip of one facing serves every facing that has none
//
// ======================================================================================== The pool
// pxc.mjs packs everything into one byte pool (little-endian; doll_pool.cpp reads it): "DOLP", u16 version (2; 1 is
// read too: v2 adds a view's flags byte after its stretch row and a frame's turn and at after its plant), u16 flags,
// u32 size, u32 0; strings (u32 n; u16 len + UTF-8 each); parts; views; marks; body sets; clips; frames; the pixel data
// (per view an RLE plane: a control byte < 128 = that + 1 clear cells, >= 128 = that - 127 literal bytes; a literal is
// 1 erase or 2 + slot * 5 + step; region planes the same with region ids; build rows raw). The record layouts are written
// out in pxc.mjs ("the pool") and read in doll_pool.cpp in the same order. The game embeds gen/doll_pool.cpp (no file is
// read at run time: the web build has no assets); tools may load a pool file instead (loadPool: rpg_test --pool,
// doll_gallery --pool), e.g. the A3 drafts: node tools/doll/pxc.mjs --root rpg/art/doll/drafts/a3 --bin <f> --no-cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/art/art_rig.h"

namespace art {
namespace doll {

// ---------------------------------------------------------------- the frozen tables (pxc.mjs mirrors every list)
enum class Slot : uint8_t {
  Skin, Hair, Eyes, Face, Ink, Cloth1, Cloth2, Accent, Linen, Cloth3, Leather, Metal, Metal2, Head, Cloak, Pattern, Plume,
  Field, Device, Glow, Fx, COUNT
};
// Face's steps: 0 lash, 1 eye white, 2 mouth, 3 lip tint, 4 blush. Eyes: 2 iris, 3 glint. Ink: 0. Fx: 0 edge, 1 body,
// 2 core (A3's arc). Every other slot is a 5-step material ramp: 0 outline, 1 shadow, 2 base, 3 light, 4 highlight.
constexpr int SLOT_COUNT = (int)Slot::COUNT;
const char* slotName(Slot s);
// the finishing pass lights step 0 -> 1 on every slot but Ink, Face, Eyes and Fx (STYLE_P2 8)
inline bool slotLit(Slot s) { return s != Slot::Ink && s != Slot::Face && s != Slot::Eyes && s != Slot::Fx; }

enum class Region : uint8_t { None, Upper, Fore, Cuff, Hand, Flare, Thigh, Knee, Shin, Boot, Sole, Edge, COUNT };
constexpr int REGION_COUNT = (int)Region::COUNT;
char regionChar(Region r);   // 'U' 'F' 'C' 'H' 'X' 'T' 'K' 'S' 'B' 'O' 'E' ('.' for None)
const char* regionName(Region r);

// the layer table (STYLE_P2 24), back to front; a view may name its own
enum class Layer : uint8_t {
  FxBack, HeldBack, Back, CloakBack, HairBack, ArmFar, Legs, Shoes, HangBack, Torso, HangFront, Belt, Neck, ArmNear, Head,
  Face, Beard, HairFront, Ears, Headwear, Held, Shield, CloakFront, Fx, COUNT
};
constexpr int LAYER_COUNT = (int)Layer::COUNT;
const char* layerName(Layer l);

enum class Anchor : uint8_t { Pivot, Head, Neck, Hip, ShoulderR, ShoulderL, Grip, GripL, COUNT };
constexpr int ANCHOR_COUNT = (int)Anchor::COUNT;
const char* anchorName(Anchor a);

// body sets: the adult man and woman, the youth (26 px, the adult head box) and the child (21 px); elders are the adult
// sets with the "elder" modifier (the stoop: their own anchors in the clips)
enum class BodySet : uint8_t { Man, Woman, Youth, Child, COUNT };
const char* bodySetName(BodySet b);   // "man" "woman" "youth" "child"
BodySet bodySetOf(const HumanLook& L);

// what a part serves (`role <category> <value>`)
enum class Role : uint8_t {
  Legs, Arms, Garment, Overlay, Neck, Belt, Sheath, Head, Nose, Brow, Ears, Marks, Expr, Paint, Earring, Beard, Hair,
  Headwear, Cloak, Held, Fx, Body, Armour, Helm, Shield, Pauldron, COUNT
};
constexpr int ROLE_COUNT = (int)Role::COUNT;
const char* roleName(Role r);

enum class Gait : uint8_t { Walk, Run };

// pixel codes in a composed frame and in the pool: 0 clear, 1 erase (maps only), 2 + slot * 5 + step
constexpr uint8_t kClear = 0, kErase = 1;
constexpr uint8_t code(Slot s, int step) { return (uint8_t)(2 + (int)s * 5 + step); }
constexpr Slot codeSlot(uint8_t c) { return (Slot)((c - 2) / 5); }
constexpr int codeStep(uint8_t c) { return (c - 2) % 5; }

// ---------------------------------------------------------------- the role vocabulary (the resolver's values)
// The value names a look asks for. Areas ADD values (and their rules in doll_resolve.cpp); artists use them in `role`.
//   legs      std (the leg library: regions say trousers, bare knees, boots), tips (shoe tips under a long skirt)
//   arms      std (the arm library: regions say the sleeve)
//   garment   tunic culturetunic dress robe kaftan wrap coat kilt poncho gown rags apron smock jerkin
//   overlay   apron tabard sash baldric; (the pattern tiles, `tags tile`) stripes checks dots vines
//                                                neck     scarf kerchief collar torc amulet
//   belt      keys trophies pouches               sheath   sword dagger axe mace (P2b: sword)
//   head      man / woman: headTypeName(); child: round oval pointed elfin
//   nose      noseName() (child: button straight)   brow  browName() (child: soft thin)   ears  human half elf
//   marks     freckles scar beauty weathered lined gaunt (adults only)
//   expr      blink hurt fierce closed talk      paint  stripes dots mask sun    earring  earrings
//   beard     beardName()  hair  hairStyleName() (adults), childHairName() (children)
//   headwear  hood cap turban furhat veil circlet conical headscarf legacyhood cloakhood
//   cloak     travel fur noble cape              held  sword axe bow staff dagger mace greatsword spear stick ledger scroll
//                                                     horn (civilian items at rest; P2c the weapons by angle)
//   fx        arc                                (P2c: armour helm shield pauldron)
// The flags `when` tests (doll_resolve.cpp flagsOf): the body set's name (man woman youth child), elder, slim, broad, half,
// elf, amulet, ring, armed, pattern and the pattern's name (stripes checks border dots embroidery vines), capped (any hat,
// hood or helm), the headwear's name (hood cap turban furhat veil circlet conical headscarf), hood (the legacy hood), beard,
// kid (child or youth), glint (a legendary glow), earrings, and each mark (freckles scar beauty weathered lined gaunt)
// The hair styles (HumanLook::hair family x HumanLook::hairVar, STYLE_P2 21): 24 for adults (man, woman, youth)
constexpr int HAIR_STYLES = 24, CHILD_HAIR_STYLES = 10;
int hairStyleOf(Hair family, uint8_t hairVar);   // 0..23 (an out-of-range variant clamps to the family's last)
const char* hairStyleName(int style);            // bald shaved cropped short tousled spiky curly receding mohawk topknot bob
                                                 // shag longloose longwavy ponytail highponytail braid twinbraids bun locs
                                                 // crownbraid topbun pigtails bigcurls
int hairVariants(Hair family);                   // the styles in a family (Bald 2, Short 5, Long 4, Ponytail 3, Mohawk 2,
                                                 // Braids 4, Bun 2, Curls 2)
const char* childHairName(int style);            // short tousled cropped curly bob long pigtails braid bun ponytail
const char* headTypeName(int body, uint8_t head);  // man: oval square round long craggy heavy fine sharp longelf;
                                                   // woman: oval heart round long square full fine sharp longelf
const char* noseName(int body, uint8_t nose);      // man: button straight long hooked broad; woman: button straight upturned aquiline
const char* browName(int body, uint8_t brow);      // man: straight heavy arched thin; woman: arched straight thin full
const char* beardName(uint8_t beardStyle);         // full stubble boxed long braided goatee moustache vandyke chops handlebar
const char* neckName(uint8_t neck);                // "" scarf kerchief collar

// ---------------------------------------------------------------- the recipe
struct DollRamps { uint32_t c[SLOT_COUNT][5] = {}; };   // every slot's 5 steps (rgba, opaque)
struct Need { Role role = Role::Legs; std::string value; };
struct DollRecipe {
  BodySet body = BodySet::Man;
  std::string bodyKey = "man";       // the clip set: "man", "woman.elder"... (falls back to the base set)
  bool elf = false;                  // stretch rows double (+1 px in the legs)
  int build = 0;                     // 0 average, 1 slim, 2 broad (build masks)
  uint8_t family = 0;                // art::Family of what is in hand (picks the ACTION clips)
  std::vector<Need> needs;           // every field value the look asks for, in a fixed order
  std::vector<int> parts;            // the pool's parts serving those needs (all of them, deduped, sorted)
  std::vector<std::string> flags;    // the conditions `when` views test (ornaments, dials, "elder", "broad", "elf"...)
  uint8_t region[REGION_COUNT];      // the slot each region draws in (0xFF: as drawn)
  DollRamps ramps;
  DollRecipe() { for (uint8_t& r : region) r = 0xFF; }
  bool has(const char* flag) const;
};
DollRecipe resolve(const HumanLook& look);

// ---------------------------------------------------------------- coverage (the census reads these)
struct Missing {
  Role role = Role::Legs;
  std::string value;     // the look's value ("tousled")
  std::string part;      // the part that lacks a view ("" when no part serves the value at all)
  BodySet body = BodySet::Man;
  int facing = -1;       // -1: every facing
  std::string key;       // the frame's key the part lacks ("" when no part at all)
  std::string what() const;   // "hair=tousled (man): no part" / "legs=std (woman) legs/adult/std R key walkB: no view"
};
// what the look needs that no authored part covers, for a RigPart's clips (Loco, Run, Action, Death) or for every part
// the look uses (Loco, Death, Run if it may run, Action if it fights): empty = covered
std::vector<Missing> dollMissing(const HumanLook& look, RigPart part);
std::vector<Missing> dollMissing(const HumanLook& look);
// the part composes with nothing missing (cached by look.key(); cleared when the pool changes)
bool dollCovers(const HumanLook& look, RigPart part);
// a posture at rig 2 composes with nothing missing
bool dollCoversPosture(const HumanLook& look, Posture p);
// parts no look of the census reaches are DEAD (B6 wants 0): the harness marks what it reached here
void markReached(const DollRecipe& r);
void clearReached();
std::vector<std::string> deadParts();   // the pool's parts no markReached recipe used

// ---------------------------------------------------------------- composing
struct ComposeOpts {
  bool finish = true;       // the finishing pass (A3's selout); off: the raw composite (tests)
  bool trim = true;         // frames trimmed to their opaque box (off: the scratch canvas, for parity diffs)
  int onlyFacing = -1;      // compose one facing (-1: all four)
};
// A RigSheet as art_rig.h defines it (frames by facing x clip x variant, a pivot per frame, RigAnchors from the marks;
// RigAnchors::topY the frame's top). Loco: Idle, Walk, Hurt; Run: Walk (the run cycle); Action: the family's clips;
// Death: Fall and Lying. Empty when the pool has no clip for the body set.
RigSheet compose(const HumanLook& look, RigPart part);
RigSheet compose(const DollRecipe& r, RigPart part, const ComposeOpts* opts = nullptr);
RigSheet composePosture(const HumanLook& look, Posture p, uint8_t variant);
RigSheet composePosture(const DollRecipe& r, Posture p, uint8_t variant);
// one part's view alone at its anchor in the recipe's body (the `dollpart` hook, doll_gallery): frames of the part's
// key on the clip's frame 0 for that facing. Empty when the part has no such view.
RigSheet composePart(const DollRecipe& r, const std::string& partName, int facing, const std::string& key);
// the statue figure (humanFigureStill2): the Loco idle frame 0 of a facing, trimmed (empty canvas: not covered)
Canvas figureStill(const HumanLook& look, int facing);
// the sleeper fitted to a berth (sleeper2): DOLL makes it (the side head turned for west-east berths); empty canvas: not
// covered (the view draws v1)
Canvas sleeper(const HumanLook& look, Berth b, int kit, int frame);

// ---------------------------------------------------------------- the walk
struct WalkTable {
  int frames = 0;          // 0: no clip (the caller keeps its own table)
  float cycle = 0;         // px of travel per cycle (man 19, woman 20; the run about 36)
  float start[16] = {};    // the travel (px into the cycle) at which each frame starts
  int frameAt(float px) const;   // the frame for a distance walked (wrapped into the cycle)
};
// the walk (or run) frame table of a body set, from its clip's plant anchors (facing 2: right; the side views decide
// the stride); when the pool has no clip: frames 0 and the stride defaults (man 19, woman 20, youth 16, child 13; run 36)
WalkTable walkTable(BodySet set, Gait gait, int facing = 2);
// (DOLL, P2b) the same for a look's own clip set: the elder's ("man.elder") walk clip first, the base set's for the rest
WalkTable walkTable(const HumanLook& look, Gait gait, int facing = 2);
constexpr float kGaitSwitch = 54.0f;   // px/s: walk below, run above (VISION_PLAN 15.36 "Constants for the doll")

// ---------------------------------------------------------------- ramps (STYLE_P2 6)
enum class Material : uint8_t { Cloth, Leather, Metal, Skin, Hair };
// A ramp from a base colour (step 2): step 1 darker toward violet, step 0 deeper (luma 70 or less), step 3 lighter toward
// yellow, step 4 lighter, warmer and a little desaturated; metals wider with a white-hot step 4. Gate: within delta E 6
// (CIE76) per step of A3's 14 hand ramps given their bases (rpg_test --doll).
void rampA3(uint32_t base, Material m, uint32_t out[5]);
// the generator alone (rampA3 without the curated tables): what the calibration gate measures
void rampGenerate(uint32_t base, Material m, uint32_t out[5]);
struct A3Ramp { const char* name; Material material; uint32_t c[5]; int steps; };   // steps: 4 or 5 drawn in A3
const std::vector<A3Ramp>& a3Ramps();   // A3's 14 hand ramps (STYLE_P2 6), the calibration set
// A3's own palettes as DollRamps (the parity looks): 0 the hero, 1 the woman
DollRamps a3Palette(int who);
// the look's ramps (curated skin and hair, eyes, the generator for cloth): doll_ramps.cpp
DollRamps rampsOf(const HumanLook& look);
float deltaE76(uint32_t a, uint32_t b);

// ---------------------------------------------------------------- the pool
// the embedded pool (gen/doll_pool.cpp) is used unless a tool loads another; every cache keyed on the pool is cleared
// on a change (poolGeneration() moves)
bool loadPool(const uint8_t* data, size_t n, std::string* error = nullptr);
bool loadPoolFile(const char* path, std::string* error = nullptr);
void useEmbeddedPool();
uint32_t poolGeneration();
struct PoolStats {
  int parts = 0, views = 0, clips = 0, frames = 0, bodySets = 0;
  size_t bytes = 0, pixelBytes = 0;
  long long opaquePx = 0, cells = 0;
  std::string hash;        // pxc.mjs's hash of the pool ("" for a loaded file)
  bool embedded = true;
};
PoolStats poolStats();
// the pool's parts (for the gallery, the census and the dead-part report)
int partCount();
const char* partName(int part);
int findPart(const std::string& name);   // -1: none
bool partHasRole(int part, Role role, const std::string& value);
bool partServes(int part, BodySet b);
bool partTagged(int part, const char* tag);

}  // namespace doll
}  // namespace art
