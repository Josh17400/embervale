// EMBERVALE art: the doll's internals (DOLL owns this header; rpg/art/doll/doll.h is the public contract). The pool as
// parsed from pxc.mjs's bytes, shared by doll_pool.cpp, doll_compose.cpp and doll_resolve.cpp.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "rpg/art/doll/doll.h"

namespace art {
namespace doll {
namespace pool {

constexpr uint8_t kNone8 = 255;
constexpr uint32_t kNone32 = 0xFFFFFFFFu;
constexpr int16_t kNoPlant = -32768;

struct RoleRec { uint8_t cat = 0; uint16_t value = 0; };
struct PartRec {
  uint16_t name = 0, channel = 0;
  uint8_t layer = 0, anchor = 0, side = 0;   // side: 0 none, 1 R, 2 L
  int8_t z = 0;
  uint8_t sway = 0, bodyMask = 0;
  std::vector<RoleRec> roles;
  std::vector<uint16_t> tags;
  uint32_t firstView = 0;
  uint16_t nViews = 0;
  // (built on load) the base views sorted by (facing, key) for viewOf; the optional-region (`when`) views
  std::vector<std::pair<uint32_t, uint32_t>> baseIx;   // (facing << 16 | key, view index)
  std::vector<uint32_t> whenViews;
};
struct ViewRec {
  uint8_t facing = 0;
  uint16_t key = 0, when = 0;                // when 0: a base view
  uint8_t layer = kNone8, anchor = kNone8;   // kNone8: the part's
  int16_t ox = 0, oy = 0;
  uint16_t w = 0, h = 0;
  int16_t stretch = -1;
  uint8_t flags = 0;                         // (pool v2) bit 0: `replace` (a `when` view drawn instead of its base)
  uint32_t pix = 0, reg = kNone32, build = kNone32;
  uint32_t firstMark = 0;
  uint8_t nMarks = 0;
  std::vector<uint8_t> cells, regions;       // decoded on load (w * h; regions empty when the view has none)
  std::vector<uint8_t> buildRow;             // w: 0, 1 broad doubles the column, 2 slim removes it (empty: none)
};
struct MarkRec { uint16_t name = 0; int16_t x = 0, y = 0; };
struct AnchorRec { uint8_t id = 0; int16_t x = 0, y = 0; };
struct BodyRec { uint16_t name = 0; std::vector<AnchorRec> anchors[4]; };
struct KeyRec { uint16_t channel = 0, key = 0; };
struct FrameRec {
  std::vector<KeyRec> keys;
  int8_t bob = 0, lean = 0, hipX = 0, hipY = 0, headX = 0, headY = 0, sway = 0;
  uint16_t ms = 0;
  int16_t weapon = -32768;
  uint8_t front = 1;
  int16_t plant0 = kNoPlant, plant1 = kNoPlant;   // x 16
  uint8_t turn = 0;                                // (pool v2) 0 upright, 1 lying head west, 2 lying head east
  int8_t atX = 0, atY = 0;                         // (pool v2) the turned figure's offset from its rest (art px)
};
struct ClipRec {
  uint16_t body = 0;
  uint8_t clip = 0, part = 0, facing = 0, family = kNone8, variant = 0, posture = kNone8;
  uint16_t cycle16 = 0;
  uint32_t firstFrame = 0;
  uint16_t nFrames = 0;
  uint16_t when = 0;   // (pool v3, P2c) the clip's condition (`clip ... when <flags>`): a string id, 0 none
};
// RigPart values in the pool (art::RigPart, plus the posture clips); (P2c) art::RigPart::Drawn (4 in the enum) is 5 in the pool
constexpr uint8_t kPartPosture = 4, kPartDrawn = 5;
inline uint8_t poolPart(RigPart p) { return p == RigPart::Drawn ? kPartDrawn : (uint8_t)p; }
// frame turns (pool v2): the frame composed in the left view, then laid down (STYLE_P2 13)
constexpr uint8_t kTurnNone = 0, kTurnWest = 1, kTurnEast = 2;

// the string ids every bake asks for (0xFFFF: not in the pool), resolved once on load
struct Ids { uint16_t rest = 0xFFFF, dash = 0xFFFF, grip = 0xFFFF, gripL = 0xFFFF, handR = 0xFFFF, handL = 0xFFFF, face = 0xFFFF,
             tip = 0xFFFF, hip = 0xFFFF, footL = 0xFFFF, footR = 0xFFFF, top = 0xFFFF, tile = 0xFFFF;
             // (P2c) the held channel, the anyfacing tag, the library's 16 angle keys (a000 a022 ... a337)
             uint16_t held = 0xFFFF, anyfacing = 0xFFFF, angle[16] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
                                                                      0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}; };
// (P2c) the library's view key for a weapon angle (degrees counter-clockwise from screen-right, rounded to 22.5): "a000".."a337"
inline int angleIndex(int deg) { int k = (int)((deg % 360 + 360) % 360 / 22.5 + 0.5); return k % 16; }
inline void angleKey(int index, char out[5]) {
  const int d = (int)(index * 22.5);
  out[0] = 'a'; out[1] = (char)('0' + d / 100); out[2] = (char)('0' + d / 10 % 10); out[3] = (char)('0' + d % 10); out[4] = 0;
}

struct Pool {
  bool ok = false;
  bool embedded = true;
  size_t bytes = 0, pixelBytes = 0;
  std::string hash;
  std::vector<std::string> str;
  std::vector<PartRec> parts;
  std::vector<ViewRec> views;
  std::vector<MarkRec> marks;
  std::vector<BodyRec> bodies;
  std::vector<ClipRec> clips;
  std::vector<FrameRec> frames;
  // (built on load) the string index, the common ids, and the parts serving each (role, value)
  std::unordered_map<std::string, uint16_t> strIx;
  Ids ids;
  std::unordered_map<uint32_t, std::vector<int>> byRole;   // role << 16 | value string id -> parts (sorted)
  uint16_t strId(const char* s) const;   // 0xFFFF: not in the pool
  uint16_t strId(const std::string& s) const;
  const std::vector<int>& partsWithRole(Role r, const std::string& value) const;
  const std::string& s(uint16_t id) const { static const std::string e; return id < str.size() ? str[id] : e; }
};
const Pool& get();
uint32_t generation();

// the body record for a body key ("man.elder" falls back to "man"); nullptr: none
const BodyRec* bodyOf(const std::string& bodyKey);
// the clips of a body key for a RigPart / posture (the modifier set's clips first, the base set's for the rest).
// (P2c) clip conditions: with a recipe, a conditioned clip whose condition holds wins its slot (name, facing, variant)
// over the unconditioned one of the same tier; without one, conditioned clips are skipped
std::vector<const ClipRec*> clipsOf(const std::string& bodyKey, uint8_t part, int family, int posture, const DollRecipe* r = nullptr);
// a `when` condition ("flag", "!flag", joined by ',') against the recipe's flags
bool condHolds(const DollRecipe& r, const std::string& cond);
// a part's view for (facing, key, when id 0 = base); nullptr: none
const ViewRec* viewOf(const PartRec& p, int facing, uint16_t key, uint16_t when = 0);

// (POSES-B, P2b) a posture's clips for a variant: per facing the variant's own clips, else variant 0's (never both)
std::vector<const ClipRec*> postureClips(const std::string& bodyKey, int family, int posture, uint8_t variant);

}  // namespace pool

// (POSES-B, P2b) what a posture puts in the hand (STYLE_P2 15): the `held` value of the prop maps (maps/held/props/**,
// PROPS: `role held <value>`, `keys held`, anchored on the grip; the posture clip's `held=<key>` names the gripping arm's
// key). nullptr: nothing in hand. doll_resolve.cpp holds the table.
const char* postureProp(Posture p, uint8_t variant);
// the recipe with the posture's prop added (its need and the parts serving it)
DollRecipe withPosture(const DollRecipe& r, Posture p, uint8_t variant);
}  // namespace doll
}  // namespace art
