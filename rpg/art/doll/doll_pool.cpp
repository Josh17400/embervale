// EMBERVALE art: the doll's pool (DOLL; P2b phase A first version by the lead). Reads the bytes tools/doll/pxc.mjs packs
// (rpg/art/doll/doll.h "The pool"): the embedded gen/doll_pool.cpp, or a pool file a tool loads.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "rpg/art/doll/doll_internal.h"

namespace art {
namespace doll {

namespace gen {
extern const unsigned char* const kPoolChunks[];
extern const size_t kPoolChunkSizes[];
extern const size_t kPoolChunkCount;
extern const char kPoolHash[];
}  // namespace gen

// ---------------------------------------------------------------- names (the frozen tables: pxc.mjs mirrors them)
const char* slotName(Slot s) {
  static const char* n[] = {"skin", "hair", "eyes", "face", "ink", "cloth1", "cloth2", "accent", "linen", "cloth3", "leather", "metal",
                            "metal2", "head", "cloak", "pattern", "plume", "field", "device", "glow", "fx"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)SLOT_COUNT, "a name for every slot");
  return (int)s < SLOT_COUNT ? n[(int)s] : "?";
}
char regionChar(Region r) {
  static const char c[] = ".UFCHXTKSBOE";
  static_assert(sizeof(c) - 1 == (size_t)REGION_COUNT, "a character for every region");
  return (int)r < REGION_COUNT ? c[(int)r] : '?';
}
const char* regionName(Region r) {
  static const char* n[] = {"none", "upper", "fore", "cuff", "hand", "flare", "thigh", "knee", "shin", "boot", "sole", "edge"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)REGION_COUNT, "a name for every region");
  return (int)r < REGION_COUNT ? n[(int)r] : "?";
}
const char* layerName(Layer l) {
  static const char* n[] = {"fxBack", "heldBack", "back", "cloakBack", "hairBack", "armFar", "legs", "shoes", "hangBack", "torso", "hangFront",
                            "belt", "neck", "armNear", "head", "face", "beard", "hairFront", "ears", "headwear", "held", "shield", "cloakFront", "fx"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)LAYER_COUNT, "a name for every layer");
  return (int)l < LAYER_COUNT ? n[(int)l] : "?";
}
const char* anchorName(Anchor a) {
  static const char* n[] = {"pivot", "head", "neck", "hip", "shoulderR", "shoulderL", "grip", "gripL"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)ANCHOR_COUNT, "a name for every anchor");
  return (int)a < ANCHOR_COUNT ? n[(int)a] : "?";
}
const char* bodySetName(BodySet b) {
  static const char* n[] = {"man", "woman", "youth", "child"};
  return (int)b < (int)BodySet::COUNT ? n[(int)b] : "?";
}
const char* roleName(Role r) {
  static const char* n[] = {"legs", "arms", "garment", "overlay", "neck", "belt", "sheath", "head", "nose", "brow", "ears", "marks", "expr",
                            "paint", "earring", "beard", "hair", "headwear", "cloak", "held", "fx", "body", "armour", "helm", "shield", "pauldron"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)ROLE_COUNT, "a name for every role");
  return (int)r < ROLE_COUNT ? n[(int)r] : "?";
}

namespace pool {
namespace {

struct Reader {
  const uint8_t* p;
  size_t n, at = 0;
  bool bad = false;
  bool need(size_t k) { if (at + k > n) { bad = true; return false; } return true; }
  uint8_t u8() { if (!need(1)) return 0; return p[at++]; }
  int8_t i8() { return (int8_t)u8(); }
  uint16_t u16() { if (!need(2)) return 0; const uint16_t v = (uint16_t)(p[at] | p[at + 1] << 8); at += 2; return v; }
  int16_t i16() { return (int16_t)u16(); }
  uint32_t u32() { const uint32_t lo = u16(); const uint32_t hi = u16(); return lo | hi << 16; }
};

// an RLE plane (doll.h "The pool") into w * h cells; false when it runs past the data
bool unrle(const std::vector<uint8_t>& blob, uint32_t at, size_t cells, std::vector<uint8_t>& out) {
  out.assign(cells, 0);
  size_t i = 0;
  while (i < cells) {
    if (at >= blob.size()) return false;
    const uint8_t c = blob[at++];
    if (c < 128) { i += (size_t)c + 1; continue; }
    const size_t k = (size_t)c - 127;
    if (at + k > blob.size()) return false;
    for (size_t j = 0; j < k && i < cells; j++) out[i++] = blob[at + j];
    at += (uint32_t)k;
  }
  return i == cells;
}

bool parse(const uint8_t* data, size_t n, Pool& P, std::string* error) {
  auto fail = [&](const char* why) { if (error) *error = why; return false; };
  Reader r{data, n};
  if (n < 16 || std::memcmp(data, "DOLP", 4) != 0) return fail("not a doll pool (no DOLP header)");
  r.at = 4;
  const uint16_t version = r.u16();
  r.u16();
  const uint32_t size = r.u32();
  r.u32();
  if (version != 1 && version != 2) return fail("pool version is not 1 or 2 (rebuild with tools/doll/pxc.mjs)");
  if (size != n) return fail("pool size does not match its header");
  const uint32_t ns = r.u32();
  P.str.resize(ns);
  for (uint32_t i = 0; i < ns && !r.bad; i++) {
    const uint16_t len = r.u16();
    if (!r.need(len)) break;
    P.str[i].assign((const char*)data + r.at, len);
    r.at += len;
  }
  const uint32_t np = r.u32();
  P.parts.resize(np);
  for (PartRec& p : P.parts) {
    p.name = r.u16(); p.channel = r.u16(); p.layer = r.u8(); p.anchor = r.u8(); p.side = r.u8(); p.z = r.i8(); p.sway = r.u8();
    p.bodyMask = r.u8();
    p.roles.resize(r.u8());
    for (RoleRec& x : p.roles) { x.cat = r.u8(); x.value = r.u16(); }
    p.tags.resize(r.u8());
    for (uint16_t& t : p.tags) t = r.u16();
    p.firstView = r.u32(); p.nViews = r.u16();
    if (r.bad) break;
  }
  const uint32_t nv = r.u32();
  P.views.resize(nv);
  for (ViewRec& v : P.views) {
    v.facing = r.u8(); v.key = r.u16(); v.when = r.u16(); v.layer = r.u8(); v.anchor = r.u8();
    v.ox = r.i16(); v.oy = r.i16(); v.w = r.u16(); v.h = r.u16(); v.stretch = r.i16();
    if (version >= 2) v.flags = r.u8();
    v.pix = r.u32(); v.reg = r.u32(); v.build = r.u32(); v.firstMark = r.u32(); v.nMarks = r.u8();
    if (r.bad) break;
  }
  const uint32_t nm = r.u32();
  P.marks.resize(nm);
  for (MarkRec& m : P.marks) { m.name = r.u16(); m.x = r.i16(); m.y = r.i16(); }
  const uint32_t nb = r.u32();
  P.bodies.resize(nb);
  for (BodyRec& b : P.bodies) {
    b.name = r.u16();
    for (int f = 0; f < 4; f++) {
      b.anchors[f].resize(r.u8());
      for (AnchorRec& a : b.anchors[f]) { a.id = r.u8(); a.x = r.i16(); a.y = r.i16(); }
    }
  }
  const uint32_t nc = r.u32();
  P.clips.resize(nc);
  for (ClipRec& c : P.clips) {
    c.body = r.u16(); c.clip = r.u8(); c.part = r.u8(); c.facing = r.u8(); c.family = r.u8(); c.variant = r.u8(); c.posture = r.u8();
    c.cycle16 = r.u16(); c.firstFrame = r.u32(); c.nFrames = r.u16();
  }
  const uint32_t nf = r.u32();
  P.frames.resize(nf);
  for (FrameRec& f : P.frames) {
    f.keys.resize(r.u8());
    for (KeyRec& k : f.keys) { k.channel = r.u16(); k.key = r.u16(); }
    f.bob = r.i8(); f.lean = r.i8(); f.hipX = r.i8(); f.hipY = r.i8(); f.headX = r.i8(); f.headY = r.i8(); f.sway = r.i8();
    f.ms = r.u16(); f.weapon = r.i16(); f.front = r.u8(); f.plant0 = r.i16(); f.plant1 = r.i16();
    if (version >= 2) { f.turn = r.u8(); f.atX = r.i8(); f.atY = r.i8(); }
    if (r.bad) break;
  }
  const uint32_t nblob = r.u32();
  if (r.bad || !r.need(nblob)) return fail("the pool is truncated");
  std::vector<uint8_t> blob(data + r.at, data + r.at + nblob);
  P.pixelBytes = nblob;
  for (ViewRec& v : P.views) {
    const size_t cells = (size_t)v.w * v.h;
    if (!unrle(blob, v.pix, cells, v.cells)) return fail("a view's pixels run past the data");
    if (v.reg != kNone32 && !unrle(blob, v.reg, cells, v.regions)) return fail("a view's regions run past the data");
    if (v.build != kNone32) {
      if ((size_t)v.build + v.w > blob.size()) return fail("a view's build row runs past the data");
      v.buildRow.assign(blob.begin() + v.build, blob.begin() + v.build + v.w);
    }
  }
  for (const PartRec& p : P.parts)
    if ((size_t)p.firstView + p.nViews > P.views.size() || p.name >= P.str.size()) return fail("a part's views or name are out of range");
  for (const ClipRec& c : P.clips)
    if ((size_t)c.firstFrame + c.nFrames > P.frames.size()) return fail("a clip's frames are out of range");
  // the indexes every bake and every coverage question uses
  P.strIx.reserve(P.str.size() * 2);
  for (size_t i = 0; i < P.str.size(); i++) P.strIx.emplace(P.str[i], (uint16_t)i);
  Ids& I = P.ids;
  I.rest = P.strId("rest"); I.dash = P.strId("-"); I.grip = P.strId("grip"); I.gripL = P.strId("gripL"); I.handR = P.strId("handR");
  I.handL = P.strId("handL"); I.face = P.strId("face"); I.tip = P.strId("tip"); I.hip = P.strId("hip"); I.footL = P.strId("footL");
  I.footR = P.strId("footR"); I.top = P.strId("top"); I.tile = P.strId("tile");
  for (size_t pi = 0; pi < P.parts.size(); pi++) {
    PartRec& p = P.parts[pi];
    for (uint32_t vi = p.firstView; vi < p.firstView + p.nViews; vi++) {
      const ViewRec& v = P.views[vi];
      if (v.when) p.whenViews.push_back(vi);
      else p.baseIx.push_back({(uint32_t)v.facing << 16 | v.key, vi});
    }
    std::sort(p.baseIx.begin(), p.baseIx.end());
    for (const RoleRec& rr : p.roles) {
      std::vector<int>& l = P.byRole[(uint32_t)rr.cat << 16 | rr.value];
      if (l.empty() || l.back() != (int)pi) l.push_back((int)pi);
    }
  }
  P.bytes = n;
  P.ok = true;
  return true;
}

Pool& slot() { static Pool p; return p; }
uint32_t& gen() { static uint32_t g = 1; return g; }
bool& loaded() { static bool b = false; return b; }

void loadEmbedded() {
  std::vector<uint8_t> bytes;
  for (size_t i = 0; i < gen::kPoolChunkCount; i++) bytes.insert(bytes.end(), gen::kPoolChunks[i], gen::kPoolChunks[i] + gen::kPoolChunkSizes[i]);
  Pool p;
  std::string why;
  if (!parse(bytes.data(), bytes.size(), p, &why)) {
    std::fprintf(stderr, "doll: the embedded pool does not parse (%s): rebuild it with node tools/doll/pxc.mjs\n", why.c_str());
    p = Pool();
  }
  p.embedded = true;
  p.hash = gen::kPoolHash;
  slot() = std::move(p);
  loaded() = true;
}

}  // namespace

uint16_t Pool::strId(const std::string& s) const {
  const auto it = strIx.find(s);
  return it == strIx.end() ? (uint16_t)0xFFFF : it->second;
}
uint16_t Pool::strId(const char* s) const { return strId(std::string(s)); }
const std::vector<int>& Pool::partsWithRole(Role r, const std::string& value) const {
  static const std::vector<int> none;
  const uint16_t v = strId(value);
  if (v == 0xFFFF) return none;
  const auto it = byRole.find((uint32_t)r << 16 | v);
  return it == byRole.end() ? none : it->second;
}
const Pool& get() {
  if (!loaded()) loadEmbedded();
  return slot();
}
uint32_t generation() { get(); return gen(); }

const BodyRec* bodyOf(const std::string& bodyKey) {
  const Pool& P = get();
  for (const BodyRec& b : P.bodies) if (P.s(b.name) == bodyKey) return &b;
  const size_t dot = bodyKey.find('.');
  if (dot != std::string::npos) return bodyOf(bodyKey.substr(0, dot));
  return nullptr;
}

std::vector<const ClipRec*> clipsOf(const std::string& bodyKey, uint8_t part, int family, int posture) {
  const Pool& P = get();
  std::vector<const ClipRec*> out;
  // the family's own clips first, then the generic ones, whatever their order in the clip files (a generic clip never
  // doubles a family clip's slot); the modifier set's clips win over the base set's for the same slot
  auto take = [&](const std::string& key) {
    const uint16_t id = P.strId(key);
    if (id == 0xFFFF) return;
    for (int generic = 0; generic < 2; generic++)
      for (const ClipRec& c : P.clips) {
        if (c.body != id || c.part != part) continue;
        if (posture >= 0 ? c.posture != posture : c.posture != kNone8) continue;
        if (generic ? c.family != kNone8 : (c.family == kNone8 || c.family != family)) continue;
        bool dup = false;
        for (const ClipRec* o : out)
          if (o->clip == c.clip && o->facing == c.facing && o->variant == c.variant) dup = true;
        if (!dup) out.push_back(&c);
      }
  };
  take(bodyKey);
  const size_t dot = bodyKey.find('.');
  if (dot != std::string::npos) take(bodyKey.substr(0, dot));
  return out;
}

const ViewRec* viewOf(const PartRec& p, int facing, uint16_t key, uint16_t when) {
  const Pool& P = get();
  if (when == 0) {
    const uint32_t k = (uint32_t)facing << 16 | key;
    const auto it = std::lower_bound(p.baseIx.begin(), p.baseIx.end(), std::make_pair(k, (uint32_t)0));
    return it != p.baseIx.end() && it->first == k ? &P.views[it->second] : nullptr;
  }
  for (uint32_t i : p.whenViews) {
    const ViewRec& v = P.views[i];
    if (v.facing == facing && v.key == key && v.when == when) return &v;
  }
  return nullptr;
}

}  // namespace pool

// ---------------------------------------------------------------- the public pool calls
bool loadPool(const uint8_t* data, size_t n, std::string* error) {
  pool::Pool p;
  if (!pool::parse(data, n, p, error)) return false;
  p.embedded = false;
  pool::slot() = std::move(p);
  pool::loaded() = true;
  pool::gen()++;
  return true;
}
bool loadPoolFile(const char* path, std::string* error) {
  FILE* f = std::fopen(path, "rb");
  if (!f) { if (error) *error = std::string("cannot open ") + path; return false; }
  std::vector<uint8_t> b;
  uint8_t buf[65536];
  size_t k;
  while ((k = std::fread(buf, 1, sizeof buf, f)) > 0) b.insert(b.end(), buf, buf + k);
  std::fclose(f);
  return loadPool(b.data(), b.size(), error);
}
void useEmbeddedPool() {
  pool::loadEmbedded();
  pool::gen()++;
}
uint32_t poolGeneration() { return pool::generation(); }

PoolStats poolStats() {
  const pool::Pool& P = pool::get();
  PoolStats s;
  s.parts = (int)P.parts.size(); s.views = (int)P.views.size(); s.clips = (int)P.clips.size(); s.frames = (int)P.frames.size();
  s.bodySets = (int)P.bodies.size(); s.bytes = P.bytes; s.pixelBytes = P.pixelBytes; s.hash = P.hash; s.embedded = P.embedded;
  for (const pool::ViewRec& v : P.views) {
    s.cells += (long long)v.w * v.h;
    for (size_t i = 0; i < (size_t)v.w * v.h && i < v.cells.size(); i++) s.opaquePx += v.cells[i] >= 2;
  }
  return s;
}
int partCount() { return (int)pool::get().parts.size(); }
const char* partName(int part) {
  const pool::Pool& P = pool::get();
  return part >= 0 && part < (int)P.parts.size() ? P.s(P.parts[(size_t)part].name).c_str() : "";
}
int findPart(const std::string& name) {
  const pool::Pool& P = pool::get();
  const uint16_t id = P.strId(name);
  if (id == 0xFFFF) return -1;
  for (size_t i = 0; i < P.parts.size(); i++) if (P.parts[i].name == id) return (int)i;
  return -1;
}
bool partHasRole(int part, Role role, const std::string& value) {
  const pool::Pool& P = pool::get();
  if (part < 0 || part >= (int)P.parts.size()) return false;
  const uint16_t v = P.strId(value);
  if (v == 0xFFFF) return false;
  for (const pool::RoleRec& r : P.parts[(size_t)part].roles) if (r.cat == (uint8_t)role && r.value == v) return true;
  return false;
}
bool partServes(int part, BodySet b) {
  const pool::Pool& P = pool::get();
  return part >= 0 && part < (int)P.parts.size() && (P.parts[(size_t)part].bodyMask >> (int)b & 1);
}
bool partTagged(int part, const char* tag) {
  const pool::Pool& P = pool::get();
  if (part < 0 || part >= (int)P.parts.size()) return false;
  for (uint16_t t : P.parts[(size_t)part].tags) if (P.s(t) == tag) return true;
  return false;
}

}  // namespace doll
}  // namespace art
