// P2c forge styles (rpg/sim/gear_forge.h; docs/P2C_CONTRACTS.md 3c). FORGE-DOLL.
#include "rpg/sim/gear_forge.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <functional>
#include <initializer_list>
#include <unordered_map>
#include <vector>
#include "rpg/art/doll/doll_forge.h"
#include "rpg/sim/gear_ladder.h"

namespace gear {

namespace F = art::doll::forge;

namespace {
void toAx(const ForgeStyle& s, uint8_t ax[F::AX_COUNT]) {
  ax[0] = s.base; ax[1] = s.shoulder; ax[2] = s.crest; ax[3] = s.cut; ax[4] = s.edge; ax[5] = s.collar; ax[6] = s.limb; ax[7] = s.fit;
}
ForgeStyle fromAx(const uint8_t ax[F::AX_COUNT]) {
  ForgeStyle s;
  s.base = ax[0]; s.shoulder = ax[1]; s.crest = ax[2]; s.cut = ax[3]; s.edge = ax[4]; s.collar = ax[5]; s.limb = ax[6]; s.fit = ax[7];
  return s;
}
int posmod(int a, int m) { return ((a % m) + m) % m; }
uint32_t fnv(uint32_t h, const std::string& s) { for (unsigned char ch : s) { h ^= ch; h *= 16777619u; } return h; }
uint32_t fnv(uint32_t h, uint32_t v) { for (int i = 0; i < 4; i++) { h ^= (v >> (8 * i)) & 255u; h *= 16777619u; } return h; }
uint32_t mix(uint32_t h) { h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; h *= 0x846CA68Bu; h ^= h >> 16; return h; }
}  // namespace

uint64_t packForge(const ForgeStyle& s) { uint8_t ax[F::AX_COUNT]; toAx(s, ax); return F::pack(ax); }
ForgeStyle unpackForge(uint64_t p) { uint8_t ax[F::AX_COUNT]; F::unpack(p, ax); return fromAx(ax); }
int styleDistance(const ForgeStyle& a, const ForgeStyle& b) {
  uint8_t x[F::AX_COUNT], y[F::AX_COUNT]; toAx(a, x); toAx(b, y); return F::distance(x, y);
}
int silhouetteDiffs(const ForgeStyle& a, const ForgeStyle& b) {
  uint8_t x[F::AX_COUNT], y[F::AX_COUNT]; toAx(a, x); toAx(b, y); return F::silhouetteDiffs(x, y);
}
const char* forgeValueName(int axis, int v) { return F::valueName(axis, v); }
int wordHeroBase(int w) {
  static const int k[6] = {F::B_FOUNDRY, F::B_FOUNDRY, F::B_WROUGHT, F::B_ARMOURER, F::B_PARADE, F::B_KNAPPED};
  return w >= 0 && w < 6 ? k[w] : F::B_ARMOURER;
}
std::string describeForge(const ForgeStyle& s) {
  uint8_t ax[F::AX_COUNT];
  toAx(s, ax);
  if (!F::pack(ax)) return "none";
  std::string o = F::valueName(F::AX_BASE, ax[0]);
  o += ":";
  for (int a = 1; a < F::AX_COUNT; a++) { o += " "; o += ax[a] ? F::valueName(a, ax[a]) : "-"; }
  return o;
}

ForgeStyle baseStyle(uint8_t metalKind, cult::BodyArm form) {
  const int fam = F::familyOfBodyArm((int)form);
  int base = 0;
  switch (metalKind) {
    case 0: base = form == cult::BodyArm::Padded ? F::B_HOMESPUN : (form == cult::BodyArm::Leather ? F::B_TANNER : 0); break;
    case 1: base = form == cult::BodyArm::Padded ? F::B_HOMESPUN : F::B_TANNER; break;
    case 2: base = F::B_FOUNDRY; break;
    case 3: base = F::B_WROUGHT; break;
    case 4: case 6: base = F::B_ARMOURER; break;   // (6 jade: legacy, never set; the steel's)
    case 5: base = F::B_PARADE; break;
    case 7: base = F::B_KNAPPED; break;
    case 8: base = F::B_FURNACE; break;
    case 9: base = F::B_STARFALL; break;
    case 11: base = F::B_MYTHIC; break;
    default: break;
  }
  // a metal style on a soft form (a padded coat with plates inside: the cloth's quilt): the metal's own picks, the cut its
  // column for that family
  uint8_t ax[F::AX_COUNT] = {};
  if (!base) return ForgeStyle{};
  F::heroAxes(base, fam, ax);
  return fromAx(ax);
}

int alloyFamilyOf(const cult::Culture& c) {
  using B = cult::BodyArm;
  B b = c.arms.body[0];
  if (b == B::Padded) b = B::Brigandine;
  else if (b == B::Leather) b = B::Lamellar;
  if (b == B::Mail) b = B::Scale;
  else if (b == B::Brigandine) b = B::Plate;
  return F::familyOfBodyArm((int)b);
}

namespace {
// a culture of the lattice: a family (kind 1, the low 32 bits clear) or a kingdom's dialect (kind 2); a test culture
// (Atlas::make's id 0, the ladder kit's 0x1ADD... ids) has no cell
bool latticeId(uint64_t id) { const uint64_t k = id >> 60; return (k == 1 && (id & 0xFFFFFFFFull) == 0) || k == 2; }
}  // namespace

int forgeSchool(const cult::Culture& c) {
  if (latticeId(c.id)) {
    int ci = (int)((c.id >> 46) & 0x3FFF), cj = (int)((c.id >> 32) & 0x3FFF);
    if (ci >= 8192) ci -= 16384;
    if (cj >= 8192) cj -= 16384;
    return posmod(ci, 3) + 3 * posmod(cj, 3);
  }
  return (int)(c.seed % 9u);
}

int forgeParity(const cult::Culture& c) {
  if (latticeId(c.id)) return (int)(((c.id >> 46) + (c.id >> 32)) & 1u);   // (ci + cj) & 1 (14-bit fields: the low bits)
  return (int)((c.seed / 9u) & 1u);
}

int forgeColumn(const cult::Culture& c) {
  if (latticeId(c.id)) return (int)((c.id >> 46) & 1u);   // ci & 1
  return (int)((c.seed / 18u) & 1u);
}

// ---------------------------------------------------------------- the pick rule
namespace {

// STYLE_P2 28.5: each people's menu per axis, in weight order (words; "none" a fittings value 0)
struct Vocab { const char* ax[F::AX_COUNT]; };
const Vocab kVocab[12] = {
    // base(unused) shoulder crest cut edge collar limb fit
    {{"", "fur scale cop slab", "comb spike horns brow", "banded keeled plackart round", "plain dagged serrated rolled", "ruff coif riveted",
      "banded laced scaled", "knot rivets bosses"}},                                                                            // Fjordfolk
    {{"", "slab flap fur cop", "spike finial brow", "splinted keeled square banded", "plain dagged fringed stepped", "stand riveted ruff",
      "studded laced splinted", "studs knot scored"}},                                                                          // Highland
    {{"", "flared plate cop lobed", "knife comb crown finial", "fluted keeled smooth embossed plackart round", "rolled cusped scalloped plain",
      "gorget flared stand", "fluted banded splinted", "rivets bosses inlay"}},                                                 // Heartland
    {{"", "plate cop lobed", "comb brow finial crown", "muscled banded embossed square", "strips scalloped rolled", "gorget torc",
      "banded splinted scaled", "bosses inlay rivets"}},                                                                        // Imperial
    {{"", "scale lobed flared layered", "spike finial crown", "ribbed embossed banded plackart pointed", "fringed scalloped pointed",
      "coif stand torc", "laced scaled", "knot inlay bosses"}},                                                                 // Dune
    {{"", "layered flap scale slab", "spike finial fin", "laced banded staggered square", "stepped fringed dagged spiked", "stand ruff torc",
      "laced banded studded", "studs stars scored"}},                                                                           // Steppe
    {{"", "flap slab leaf layered", "fin spike finial", "splinted smooth banded", "fringed dagged strips plain", "stand coif",
      "laced splinted", "lacing studs scored"}},                                                                                // Marsh
    {{"", "layered winged lobed scale", "fin finial crown spike", "laced banded staggered round", "scalloped fringed rolled", "stand high",
      "laced banded", "knot inlay bosses"}},                                                                                    // Jade
    {{"", "plate slab cop flared", "finial comb brow", "splinted banded smooth keeled", "plain rolled stepped scalloped",
      "gorget stand riveted", "banded studded", "rivets bosses inlay"}},                                                        // River
    {{"", "winged lobed flap shard", "wings crown brow fin", "embossed smooth faceted diamond", "fringed stepped serrated",
      "ruff torc flared", "laced banded", "bosses inlay studs"}},                                                               // SunTemple
    {{"", "leaf lobed flared layered", "fin finial spire crown", "ribbed smooth", "pointed scalloped rolled", "high stand", "ribbed laced",
      "inlay knot none"}},                                                                                                      // Sylvan
    {{"", "crystal winged flared lobed", "spire wings finial crown", "ribbed faceted smooth fluted pointed", "pointed cusped rolled",
      "high gorget", "ribbed fluted", "stars inlay knot"}},                                                                     // Starspire
};
// the hidden peoples' words: their own picks first, at the top weight (STYLE_P2 28.5)
struct Hidden { const char* word; const char* ax[F::AX_COUNT]; };
const Hidden kHidden[6] = {
    {"VEIL", {"", "", "", "round", "fringed", "coif", "", ""}},
    {"WHORL", {"", "lobed", "finial", "fluted", "", "", "", ""}},
    {"TIDE", {"", "layered", "fin", "", "scalloped", "", "", ""}},
    {"PEARL", {"", "cop", "", "smooth", "rolled", "", "", "bosses"}},
    {"SHARD", {"", "shard", "shards", "faceted", "serrated", "", "", ""}},
    {"WRACK", {"", "", "", "crackled", "spiked", "angular", "", ""}},
};
// boosts: "axis:word axis:word ..." lists, each a x1.5 on its values
struct Boost { const char* list; };
// the metal word (0 BRONZE 1 BRASS 2 IRON 3 STEEL 4 SILVER 5 GLASS): STYLE_P2 28.4's characters
const char* const kWordBoost[6] = {
    "cut:muscled cut:smooth cut:embossed cut:round cut:laced shoulder:cop shoulder:lobed edge:rolled edge:strips edge:scalloped collar:torc "
    "collar:rolled crest:comb crest:finial limb:scaled fit:bosses",
    "cut:muscled cut:smooth cut:embossed cut:round cut:laced shoulder:cop shoulder:lobed edge:rolled edge:strips edge:scalloped collar:torc "
    "collar:rolled crest:comb crest:finial limb:scaled fit:bosses",
    "cut:banded cut:splinted cut:square shoulder:slab shoulder:plate edge:plain edge:stepped collar:riveted crest:brow crest:spike "
    "limb:banded limb:splinted fit:rivets fit:studs",
    "cut:fluted cut:keeled cut:pointed cut:staggered shoulder:flared shoulder:layered edge:cusped edge:pointed collar:gorget collar:high "
    "crest:knife crest:comb limb:fluted",
    "cut:smooth cut:embossed cut:round shoulder:lobed shoulder:cop shoulder:winged edge:scalloped edge:rolled collar:flared collar:high "
    "crest:crown crest:finial limb:ribbed fit:inlay",
    "cut:faceted cut:ribbed cut:pointed shoulder:shard shoulder:crystal edge:serrated edge:pointed collar:angular collar:high crest:shards "
    "crest:spire limb:faceted fit:stars",
};
// the sheen's tells (STYLE_P2 28.7; cult::Sheen order)
const char* const kSheenBoost[8] = {
    "fit:scored fit:studs edge:plain edge:stepped",          // matte (never fluted: below)
    "cut:fluted cut:keeled cut:smooth",                      // bright
    "cut:faceted shoulder:shard edge:serrated edge:spiked",  // dark
    "cut:banded limb:banded",                                // banded
    "cut:round cut:pointed cut:laced cut:staggered",         // iridescent
    "cut:vented cut:ribbed cut:banded",                      // glowing
    "cut:smooth cut:ribbed fit:inlay",                       // pale
    "cut:embossed cut:muscled fit:bosses edge:rolled",       // burnished
};
// the rank: everyday plain, warriors' martial, masterwork elaborate
const char* const kRankBoost[3] = {
    "shoulder:plate shoulder:fur shoulder:roll shoulder:flap shoulder:cop shoulder:slab shoulder:scale crest:ridge crest:brow crest:finial "
    "fit:rivets fit:studs",
    "shoulder:flared shoulder:layered shoulder:slab shoulder:vented crest:comb crest:knife crest:spike crest:horns crest:chimney",
    "shoulder:lobed shoulder:winged shoulder:crystal shoulder:leaf shoulder:shard crest:crown crest:spire crest:fin crest:wings crest:shards "
    "fit:inlay fit:knot fit:stars",
};
// the recipe's reagents (contracts 3c): a key word in the reagent's name -> its boosts
struct Reagent { const char* key; const char* list; };
const Reagent kReagent[] = {
    {"ASH", "fit:scored cut:vented shoulder:vented"}, {"CHAR", "fit:scored cut:vented shoulder:vented"},
    {"GLASS", "cut:faceted cut:ribbed shoulder:shard fit:stars"}, {"QUARTZ", "cut:faceted cut:ribbed shoulder:shard fit:stars"},
    {"DUST", "cut:faceted cut:ribbed shoulder:shard fit:stars"}, {"OBSIDIAN", "cut:faceted cut:ribbed shoulder:shard fit:stars"},
    {"SALT", "edge:serrated cut:banded limb:banded"}, {"RIME", "edge:serrated cut:banded limb:banded"},
    {"OIL", "cut:smooth fit:inlay fit:lacing"}, {"RESIN", "cut:smooth fit:inlay fit:lacing"}, {"SAP", "cut:smooth fit:inlay fit:lacing"},
    {"LACQUER", "cut:smooth fit:inlay fit:lacing"}, {"AMBER", "cut:smooth fit:inlay fit:lacing"}, {"COPAL", "cut:smooth fit:inlay fit:lacing"},
    {"BONE", "edge:spiked fit:studs"}, {"HORN", "edge:spiked fit:studs"}, {"STONE", "fit:stars"},
    {"LIME", "cut:muscled cut:embossed"}, {"MARBLE", "cut:muscled cut:embossed"}, {"CLAY", "cut:muscled cut:embossed"},
    {"WATER", "cut:fluted cut:smooth"}, {"SAND", "cut:banded"}, {"MOSS", "cut:ribbed shoulder:leaf"},
};
// the people's ornament bits (cult::ARM_*)
struct Orn { uint16_t bit; const char* list; };
const Orn kOrn[] = {
    {cult::ARM_SCALLOPS, "edge:scalloped"}, {cult::ARM_TASSELS, "edge:fringed"}, {cult::ARM_STUDS, "fit:studs limb:studded"},
    {cult::ARM_RIVETS, "fit:rivets"}, {cult::ARM_FUR_TRIM, "shoulder:fur collar:ruff"}, {cult::ARM_FILIGREE, "cut:embossed fit:inlay"},
    {cult::ARM_GILDING, "cut:embossed fit:inlay"},
};

bool listHas(const char* list, int axis, const char* word) {
  if (!list) return false;
  const char* an = F::axisName(axis);
  const size_t al = std::strlen(an), wl = std::strlen(word);
  for (const char* p = list; *p;) {
    while (*p == ' ') p++;
    const char* e = p;
    while (*e && *e != ' ') e++;
    // "axis:word"
    if ((size_t)(e - p) == al + 1 + wl && !std::strncmp(p, an, al) && p[al] == ':' && !std::strncmp(p + al + 1, word, wl)) return true;
    p = e;
  }
  return false;
}
// the words of a space separated menu line, in order
std::vector<std::string> words(const char* s) {
  std::vector<std::string> v;
  if (!s) return v;
  std::string cur;
  for (const char* p = s;; p++) {
    if (!*p || *p == ' ') { if (!cur.empty()) v.push_back(cur); cur.clear(); if (!*p) break; }
    else cur += *p;
  }
  return v;
}

struct Cand { uint8_t v; int score; };
// the school's classes (0 A, 1 B) of the secondary axes' values: every people's menu (STYLE_P2 28.5) holds both on edge,
// collar and fittings (the Imperial limbs are all B)
int schoolClass(int axis, int v) {
  const char* w = v ? F::valueName(axis, v) : "none";
  static const char* const kA[F::AX_COUNT] = {"", "", "", "",
                                              " plain scalloped stepped spiked cusped ",          // edge
                                              " stand gorget ruff rolled furnace ",               // collar
                                              " laced fluted studded crackled ",                  // limb
                                              " knot rivets studs stars none "};                  // fittings
  const std::string key = std::string(" ") + w + " ";
  return std::strstr(kA[axis], key.c_str()) ? 0 : 1;
}
// the sheen's filter on the cut (28.7): the cuts allowed on the family (empty: any)
bool cutAllowed(int sheen, int cut, int fam) {
  using namespace art::doll::forge;
  switch ((cult::Sheen)sheen) {
    case cult::Sheen::Matte: return cut != C_FLUTED;
    case cult::Sheen::Banded: return cut == C_BANDED || cut == C_SQUARE || cut == C_CHANNEL || cut == C_STUDDED;
    case cult::Sheen::Iridescent:   // a row cut: rows that can alternate two hues
      return fam == F_PLATE ? (cut == C_BANDED || cut == C_VENTED) : fam == F_MAIL ? cut == C_BANDED :
             fam == F_PADDED ? cut == C_BRICK : fam == F_LEATHER ? cut == C_STUDDED : true;   // scale and lamellar rows
    case cult::Sheen::Glowing:      // a seam-rich cut
      return fam == F_PLATE ? (cut == C_VENTED || cut == C_RIBBED || cut == C_BANDED) : cutClass(cut) == 1 || cutClass(cut) == 2;
    default: return true;
  }
}

struct Ctx {
  const cult::Culture* c = nullptr;
  int idx = 0, rank = 0, word = 3, sheen = 1, fam = 0, school = 0;
  uint32_t seed = 0;
  std::vector<std::string> reagents;
  std::vector<Cand> cand[F::AX_COUNT];
};

int boost(const char* list, int axis, const char* w) { return listHas(list, axis, w) ? 384 : 256; }

void buildCandidates(Ctx& X) {
  const cult::Culture& c = *X.c;
  const cult::Alloy& A = c.arms.alloys[(size_t)X.idx];
  const int a1 = std::clamp((int)c.archetype, 0, 11), a2 = std::clamp((int)c.archetype2, 0, 11);
  // the hidden word (the masterwork's name ends with it)
  const Hidden* hid = nullptr;
  for (const Hidden& h : kHidden) {
    const size_t k = std::strlen(h.word);
    if (c.hidden && A.name.size() >= k && A.name.compare(A.name.size() - k, k, h.word) == 0) hid = &h;
  }
  for (int axis = 1; axis < F::AX_COUNT; axis++) {
    std::vector<std::pair<std::string, int>> menu;   // word, weight
    auto add = [&](const std::string& w, int wt) {
      for (auto& m : menu) if (m.first == w) { m.second = std::max(m.second, wt); return; }
      menu.push_back({w, wt});
    };
    if (hid) for (const std::string& w : words(hid->ax[axis])) add(w, 320);
    for (int pass = 0; pass < (c.isolated && a2 != a1 ? 2 : 1); pass++) {
      int wt = 256;
      for (const std::string& w : words(kVocab[pass ? a2 : a1].ax[axis])) { add(w, wt); wt = wt * 13 / 16; }
    }
    std::vector<Cand>& out = X.cand[axis];
    for (const auto& m : menu) {
      int v = m.first == "none" ? 0 : F::valueOf(axis, m.first.c_str());
      if (v < 0) continue;
      if (axis == F::AX_CUT) {
        v = F::cutOnFamily(v, X.fam);
        if (!cutAllowed(X.sheen, v, X.fam)) continue;
      }
      const char* w = v ? F::valueName(axis, v) : "none";
      long s = m.second;
      s = s * boost(kWordBoost[X.word], axis, w) / 256;
      s = s * boost(kSheenBoost[X.sheen & 7], axis, w) / 256;
      s = s * boost(kRankBoost[X.rank], axis, w) / 256;
      for (const std::string& r : X.reagents)
        for (const Reagent& rg : kReagent)
          if (r.find(rg.key) != std::string::npos) s = s * boost(rg.list, axis, w) / 256;
      for (const Orn& o : kOrn)
        if (c.arms.ornament & o.bit) s = s * boost(o.list, axis, w) / 256;
      // the seed's jitter: 12 % per axis and value on the primary axes (shoulder, cut, crest); the school's axes (edge,
      // collar, limb, fittings) keep the people's order, so the school's rotation parts two neighbouring families of one
      // people on every one of them (the seed breaks their exact ties only)
      if (axis == F::AX_SHOULDER || axis == F::AX_CUT || axis == F::AX_CREST) {
        const int j = (int)(mix(X.seed ^ (uint32_t)axis * 0x9E3779B9u ^ (uint32_t)v * 0x85EBCA6Bu) % 63u) - 31;
        s = s * (256 + j) / 256;
      }
      bool dup = false;
      for (Cand& o : out) if (o.v == v) { o.score = std::max(o.score, (int)s); dup = true; }
      if (!dup) out.push_back(Cand{(uint8_t)v, (int)std::max(1L, s)});
    }
    // the sheen's filter left nothing on the menu: the family's own cuts that show the tell
    if (axis == F::AX_CUT && out.empty())
      for (int v = 1; v < F::C_COUNT; v++)
        if (F::cutNative(v, X.fam) && cutAllowed(X.sheen, v, X.fam)) out.push_back(Cand{(uint8_t)v, 64 + (int)(mix(X.seed ^ (uint32_t)v) % 32u)});
    std::stable_sort(out.begin(), out.end(), [&](const Cand& p, const Cand& q) {
      if (p.score != q.score) return p.score > q.score;
      return mix(X.seed ^ p.v * 0x27D4EB2Fu) < mix(X.seed ^ q.v * 0x27D4EB2Fu);
    });
  }
}

}  // namespace

ForgePick alloyPick(const cult::Culture& c, int alloyIndex) {
  ForgePick P;
  if (alloyIndex < 0 || alloyIndex >= (int)c.arms.alloys.size()) { P.ok = false; P.why = "no such alloy"; return P; }
  const cult::Alloy& A = c.arms.alloys[(size_t)alloyIndex];
  Ctx X;
  X.c = &c; X.idx = alloyIndex; X.rank = std::min(alloyIndex, 2);
  // (a two-alloy culture's second is its warriors' steel, never the masterwork: the rank is the index)
  X.word = alloyMetalWord(A, X.rank);
  X.sheen = std::clamp((int)A.sheen, 0, 7);
  X.fam = alloyFamilyOf(c);
  X.school = forgeSchool(c);
  uint32_t h = 2166136261u;
  h = fnv(h, A.name);
  for (const cult::Ingredient& g : A.recipe) {
    h = fnv(h, (uint32_t)(int)g.ore);
    h = fnv(h, (uint32_t)g.parts);
    h = fnv(h, g.reagent);
    if (g.ore < 0) X.reagents.push_back(g.reagent);
  }
  X.seed = h;
  buildCandidates(X);
  // the start: shoulder, cut and crest the best; edge, collar, limb and fittings by the school. The school (FORGE-DOLL i1,
  // contracts 8): each secondary axis' values carry a fixed class (A or B, schoolClass: a 2-colouring in which every
  // people's menu holds both, but the Imperial limbs); the family's cell parity plus the alloy's rank picks the class and the
  // best candidate of that class leads. Every neighbouring cell flips the parity, so two neighbouring families' same-rank
  // alloys part on edge, collar and fittings whatever their peoples (s mod n of the 3 x 3 label could not: a down
  // neighbour kept its pick, and two peoples' different menus collided), and a culture's own alloys alternate the classes
  int idx[F::AX_COUNT] = {}, moved[F::AX_COUNT] = {};
  const int want = (forgeParity(c) + X.rank) & 1;
  for (int a : {(int)F::AX_EDGE, (int)F::AX_COLLAR, (int)F::AX_LIMB, (int)F::AX_FIT}) {
    std::vector<Cand>& cs = X.cand[a];
    if (cs.size() < 2) continue;
    size_t lead = cs.size();
    for (size_t k = 0; k < cs.size() && lead == cs.size(); k++) if (schoolClass(a, cs[k].v) == want) lead = k;
    if (lead == cs.size()) lead = (size_t)want % cs.size();   // (no value of the class: the parity's index)
    std::rotate(cs.begin(), cs.begin() + (long)lead, cs.begin() + (long)lead + 1);   // the lead first, the rest in score order
  }
  // (forge i2; C16c: with the culture's shoulder and crest kept apart, two neighbouring families of one people led with the
  // same shoulder and crest and fell to d 4) the crest follows the school too: the cell's parity leads with the best or the
  // second-best crest, so neighbours part on the helm's loudest read
  // and the shoulder by the cell's column (ci & 1): an orthogonal neighbour flips the parity, a diagonal one the column,
  // so every neighbour parts on the crest or the shoulder
  // (the crest by a class, as the school's axes: finial, comb, wings and ridge A, every other crest B; every people's crest menu
  // holds both. The cell's parity plus the rank picks the class, so orthogonal neighbours lead with crests of two classes)
  auto crestClass = [](int v) { return (v == 10 || v == 2 || v == 13 || v == 1) ? 0 : 1; };
  const int wantCrest = (forgeParity(c) + X.rank) & 1;
  {
    std::vector<Cand>& cs = X.cand[F::AX_CREST];
    for (size_t k = 0; k < cs.size(); k++)
      if (crestClass(cs[k].v) == wantCrest) {
        const uint8_t v = cs[k].v;
        for (size_t j = k; j > 0; j--) cs[j].v = cs[j - 1].v;   // (the values move up, the scores stay in order)
        cs[0].v = v;
        break;
      }
  }
  if (X.cand[F::AX_SHOULDER].size() >= 2 && forgeColumn(c))
    std::swap(X.cand[F::AX_SHOULDER][0].v, X.cand[F::AX_SHOULDER][1].v);
  auto styleNow = [&](uint8_t ax[F::AX_COUNT]) {
    ax[0] = F::B_ALLOY;
    for (int a = 1; a < F::AX_COUNT; a++) ax[a] = X.cand[a].empty() ? 0 : X.cand[a][(size_t)idx[a]].v;
  };
  // the styles to stay apart from: the culture's earlier alloys (7, two silhouette axes), the metal word's hero (5), every
  // hero (4); the heroes on the alloy's own family
  std::vector<ForgeStyle> earlier;
  for (int j = 0; j < alloyIndex; j++) earlier.push_back(alloyPick(c, j).style);   // (round 1's picks: the round-1 bodies)
  const int wordBase = wordHeroBase(X.word);
  struct Target { uint8_t ax[F::AX_COUNT]; int need; bool sil; int kind; };   // kind 0 culture, 1 word hero, 2 hero
  std::vector<Target> targets;
  for (const ForgeStyle& e : earlier) { Target t; toAx(e, t.ax); t.need = 7; t.sil = true; t.kind = 0; targets.push_back(t); }
  { Target t; F::heroAxes(wordBase, X.fam, t.ax); t.need = 5; t.sil = false; t.kind = 1; targets.push_back(t); }
  for (int b = F::B_HOMESPUN; b < F::B_COUNT; b++) {
    if (b == wordBase) continue;
    Target t; F::heroAxes(b, X.fam, t.ax); t.need = 4; t.sil = false; t.kind = 2; targets.push_back(t);
  }
  // the forge i2 rules' strictness: 2 both (the culture's shoulder and crest apart, the word hero's crest apart), 1 the
  // culture's and the word hero's shoulder or crest, 0 i1's (two silhouette axes): a people's menu too short for them (Dune: three crests, PARADE's crown
  // among them) steps down, the bounded search tries each level in turn
  int strict = 2;
  auto failing = [&](const uint8_t ax[F::AX_COUNT]) -> int {
    for (size_t i = 0; i < targets.size(); i++) {
      const Target& t = targets[i];
      // (forge i2; the blind panel: "all 5 Jade alloys are one sprite", "Asiron/Froststeel almost pixel-identical") a
      // culture's alloys part on the two silhouette axes that read at 1x, the shoulder AND the crest (edge and collar are a
      // row or two: the i1 rule's "two of four" let two alloys share both and read as one figure)
      if (F::distance(ax, t.ax) < t.need ||
          (t.sil && (F::silhouetteDiffs(ax, t.ax) < 2 ||
                     (strict >= 1 && (ax[F::AX_SHOULDER] == t.ax[F::AX_SHOULDER] || ax[F::AX_CREST] == t.ax[F::AX_CREST])))))
        return (int)i;
      // (forge i2; C16c drawn: the SILVER masterworks picked PARADE's crown and drew as PARADE, the shoulder's lobes or
      // flares a pixel apart) the metal word's hero is parted on the crest, the helm's loudest read, as well
      if (strict >= 2 && t.kind == 1 && ax[F::AX_CREST] == t.ax[F::AX_CREST]) return (int)i;
      if (strict == 1 && t.kind == 1 && ax[F::AX_CREST] == t.ax[F::AX_CREST] && ax[F::AX_SHOULDER] == t.ax[F::AX_SHOULDER]) return (int)i;
    }
    return -1;
  };
  auto measure = [&](const uint8_t ax[F::AX_COUNT], ForgePick& R) {
    R.minCulture = 99; R.minHero = 99; R.wordHero = 99;
    for (const Target& t : targets) {
      const int d = F::distance(ax, t.ax);
      if (t.kind == 0) R.minCulture = std::min(R.minCulture, d);
      else if (t.kind == 1) R.wordHero = d;
      else R.minHero = std::min(R.minHero, d);
    }
  };
  static const int kOrder[7] = {F::AX_SHOULDER, F::AX_CUT, F::AX_CREST, F::AX_EDGE, F::AX_COLLAR, F::AX_LIMB, F::AX_FIT};
  static const bool kSil[F::AX_COUNT] = {false, true, true, false, true, true, false, false};
  uint8_t ax[F::AX_COUNT];
  styleNow(ax);
  int steps = 0;
  bool ok = failing(ax) < 0;
  // the walk: the colliding axis of highest weight takes its next candidate (shoulder, cut, crest, edge, collar, limb,
  // fittings), at most 64 steps
  while (!ok && steps < 64) {
    const Target& t = targets[(size_t)failing(ax)];
    const bool silOnly = t.sil && F::distance(ax, t.ax) >= t.need;   // the distance holds, the silhouette does not
    // (forge i2) a shared shoulder or crest moves that axis first
    int first = -1;
    if (t.kind == 1 && ax[F::AX_CREST] == t.ax[F::AX_CREST] && moved[F::AX_CREST] < (int)X.cand[F::AX_CREST].size() - 1) first = F::AX_CREST;
    else if (t.sil && ax[F::AX_SHOULDER] == t.ax[F::AX_SHOULDER] && moved[F::AX_SHOULDER] < (int)X.cand[F::AX_SHOULDER].size() - 1) first = F::AX_SHOULDER;
    else if (t.sil && ax[F::AX_CREST] == t.ax[F::AX_CREST] && moved[F::AX_CREST] < (int)X.cand[F::AX_CREST].size() - 1) first = F::AX_CREST;
    int pick = first;
    for (int a : kOrder) {
      if (pick >= 0) break;
      const int n = (int)X.cand[a].size();
      if (ax[a] != t.ax[a] || moved[a] >= n - 1 || (silOnly && !kSil[a])) continue;
      pick = a;
      break;
    }
    if (pick < 0) break;
    idx[pick] = (idx[pick] + 1) % (int)X.cand[pick].size();
    moved[pick]++;
    steps++;
    styleNow(ax);
    ok = failing(ax) < 0;
  }
  if (!ok) {
    // the walk ran out: the bounded search over the best candidates (4 per silhouette axis and cut, 3 for limb and
    // fittings) keeps the highest-scoring style that holds every constraint (deterministic: the scores and the seed)
    int K[F::AX_COUNT];
    for (int a = 1; a < F::AX_COUNT; a++) K[a] = std::min((int)X.cand[a].size(), (a == F::AX_LIMB || a == F::AX_FIT) ? 3 : 4);
    long best = -1;
    int bi[F::AX_COUNT] = {};
    int ii[F::AX_COUNT] = {};
    std::function<void(int)> rec;
    uint8_t tx[F::AX_COUNT];
    tx[0] = F::B_ALLOY;
    rec = [&](int a) {
      if (a == F::AX_COUNT) {
        if (failing(tx) >= 0) return;
        long s = 0;
        for (int b = 1; b < F::AX_COUNT; b++) s += X.cand[b].empty() ? 0 : X.cand[b][(size_t)ii[b]].score;
        if (crestClass(tx[F::AX_CREST]) == wantCrest) s += 1L << 24;   // (the school's crest class first)
        for (int a2 : {(int)F::AX_EDGE, (int)F::AX_COLLAR, (int)F::AX_LIMB, (int)F::AX_FIT})   // (and the school's classes)
          if (tx[a2] && schoolClass(a2, tx[a2]) == want) s += 1L << 20;
        if (s > best) { best = s; for (int b = 1; b < F::AX_COUNT; b++) bi[b] = ii[b]; }
        return;
      }
      if (!K[a]) { tx[a] = 0; ii[a] = 0; rec(a + 1); return; }
      for (int k = 0; k < K[a]; k++) { ii[a] = k; tx[a] = X.cand[a][(size_t)k].v; rec(a + 1); }
    };
    for (strict = 2; strict >= 0 && best < 0; strict--) rec(1);
    strict++;
    steps += 64;
    if (best >= 0) {
      for (int a = 1; a < F::AX_COUNT; a++) idx[a] = bi[a];
      styleNow(ax);
      ok = true;
    }
  }
  P.style = fromAx(ax);
  P.steps = steps;
  P.ok = ok;
  measure(ax, P);
  if (!ok) {
    const int f = failing(ax);
    const Target& t = targets[(size_t)f];
    P.why = t.kind == 0 ? "culture alloy too close" : t.kind == 1 ? "metal word's hero too close" : "a hero style too close";
  }
  return P;
}

// (forge round 2, contracts 3c.1) a culture alloy's style is its details: the shoulder's shape inside the table key's mass,
// the crest on a socketed family, the collar; the cut, edge, limb and fittings retired (0). Before the table is solved (the
// bootstrap build) round 1's picks stand
ForgeStyle alloyStyle(const cult::Culture& c, int alloyIndex) {
  if (alloyIndex < 0 || alloyIndex >= (int)c.arms.alloys.size()) return ForgeStyle{};
  const uint32_t k = tableKey(std::clamp((int)c.archetype, 0, 11), forgeSchool6(c), shapeRank(alloyIndex));
  if (!k) return alloyPick(c, alloyIndex).style;
  const AlloyDetails d = alloyDetails(c, alloyIndex, shapeOfKey(k));
  ForgeStyle s;
  s.base = F::B_ALLOY; s.shoulder = d.shoulder; s.crest = d.crest; s.collar = d.collar;
  return s;
}

ForgeStyle forgeOf(const Item& it, const cult::Culture* maker) {
  const uint8_t mk = metalKindOf(it);
  // the form the piece takes: its own pick, else its maker's (the body's first form; a soft piece the people's soft form)
  cult::BodyArm form = cult::BodyArm::Plate;
  if (it.kind == ItemKind::Armor) {
    using B = cult::BodyArm;
    const bool soft = it.mat == Mat::Leather || it.name.rfind("LEATHER", 0) == 0;
    if (it.form) form = (B)std::min<int>(it.form - 1, 7);
    else if (maker && soft) {   // gear_look bodyFormOf's soft rule
      const cult::ArmsStyle& A = maker->arms;
      form = (A.body[1] == B::Padded || A.body[1] == B::Leather) ? A.body[1] : ((A.body[0] == B::Padded || A.body[0] == B::Leather) ? A.body[0] : B::Leather);
    } else if (maker) form = maker->arms.body[0];
    else if (soft) form = B::Leather;
  } else if (it.mat == Mat::Leather) {
    form = cult::BodyArm::Leather;
  }
  if (mk == 11) return baseStyle(11, form);
  if (it.mat == Mat::Alloy && maker && it.alloy >= 1 && it.alloy <= maker->arms.alloys.size()) {
    ForgeStyle s = alloyStyle(*maker, it.alloy - 1);
    return s;
  }
  if (it.mat == Mat::Cloth && it.kind == ItemKind::Armor) return baseStyle(0, cult::BodyArm::Padded);
  ForgeStyle s = baseStyle(mk, form);
  // (forge i3 integration; the owner: "every alloy or material armor should be different in style") another people's
  // bronze, iron and steel keep the metal word's hero picks but crown the helm with that people's own mark, one crest of
  // its STYLE_P2 28.5 menu that reads facing down (the Fjordfolk's horns, SunTemple's wings; the Heartland keeps its
  // hero's), so the T3 row of peoples reads apart in outline (C12e). A helm with its own crest or plume part keeps that
  // instead (the resolver drops the style's crest).
  if (s.crest && maker && mk >= 2 && mk <= 4) {
    static const char* const kKitCrest[12] = {"horns", "brow", "", "comb", "spike", "spike", "fin", "finial", "finial", "wings", "finial", "spire"};
    const int a = std::clamp((int)maker->archetype, 0, 11);
    const int v = *kKitCrest[a] ? F::valueOf(F::AX_CREST, kKitCrest[a]) : -1;
    if (v > 0) s.crest = (uint8_t)v;
  }
  return s;
}

}  // namespace gear

namespace gear {

uint16_t alloyGuise(const cult::Culture& c, int alloyIndex) {
  const int n = (int)c.arms.alloys.size();
  if (alloyIndex < 0 || alloyIndex >= n) return 0;
  // the heads each people makes (Guise::helm codes: 1 nasal 2 kettle 3 great helm 4 spangen 8 masked 9 crested; 11 mail coif
  // 12 turban helm 13 jingasa 14 aventail), its most typical first (STYLE_P2 28.5's vocabulary at the head)
  // (the mail hoods, coif and aventail, late in most menus: in i3's first cut they took a third of the alloys)
  static const uint8_t kVocab[12][6] = {
      {3, 8, 1, 9, 11, 4},     // Fjordfolk: the great helm, the mask, the nasal, the crested helm, the coif
      {3, 11, 9, 1, 8, 2},     // Highland: the great helm, the coif, the crested helm (its own kettle hat is the people's)
      {1, 9, 2, 11, 3, 8},     // Heartland: the nasal, the crested helm, the kettle, the coif (the great helm late: the T4 hero's face reads, C12)
      {9, 3, 1, 8, 14, 2},     // Imperial: the crested helm, the great helm, the nasal, the mask
      {12, 4, 8, 14, 9, 3},    // Dune: the turban helm, the spangenhelm, the mask, the aventail
      {4, 12, 8, 2, 11, 3},    // Steppe: the spangenhelm, the turban helm, the mask
      {13, 2, 8, 12, 11, 9},   // Marsh: the jingasa, the kettle hat, the mask, the turban helm
      {13, 9, 4, 3, 14, 12},   // Jade: the jingasa, the crested helm, the spangenhelm, the great helm (its mask is the people's)
      {13, 12, 2, 9, 11, 3},   // River: the jingasa, the turban helm, the kettle hat
      {12, 9, 3, 8, 13, 14},   // SunTemple: the turban helm, the crested helm, the great helm, the mask
      {14, 9, 1, 13, 3, 8},    // Sylvan: the aventail, the crested helm, the nasal, the jingasa
      {4, 3, 8, 12, 14, 9}};   // Starspire: the spangenhelm, the great helm, the mask, the turban helm   // Starspire   // Starspire
  // the drawn helm each crest sits best on (crest 1..14: ridge comb knife brow crown shards chimney spire fin finial spike
  // horns wings flame)
  static const uint8_t kCrestHelm[15] = {0, 1, 9, 1, 2, 9, 8, 3, 4, 8, 2, 4, 3, 9, 3};
  static const uint8_t kAll[10] = {1, 2, 3, 4, 8, 9, 11, 12, 13, 14};
  const int ar = std::min<int>((int)c.archetype, 11);
  // the family's own order: its first alloy's name (a family's dialects copy its alloys, so they share it) rotates the menu
  const uint32_t fam = mix(fnv(2166136261u, c.arms.alloys[0].name));
  uint8_t used[8] = {};
  int nu = 0;
  int own = 0;
  // the people's own helm (as the doll draws it: an undrawn form's nearest, doll_resolve drawnForm) is what its universal
  // materials wear (bronze, iron, steel keep the people's forms, C16c): no alloy takes it, and the everyday alloy (the
  // BRONZE or BRASS beside FOUNDRY in the people's form) not even its shape
  {
    static const uint8_t kNext[11] = {0, 0, 1, 9, 1, 4, 9, 4, 4, 1, 4};
    int f = (int)c.arms.helm[0] + 1;
    for (int guard = 0; guard < 4 && f > 0 && !(f == 1 || f == 2 || f == 3 || f == 4 || f == 8 || f == 9); guard++) f = f <= 10 ? kNext[f] : 0;
    own = f;
  }
  F::Guise g;
  for (int i = 0; i <= alloyIndex; i++) {
    const ForgeStyle s = alloyPick(c, i).style;   // (round 1's picks: the guise is round 1's body)
    // (the Heartland's universal materials wear their hero styles' own helms, doll_resolve styleKitOf's kHelm: there the
    // alloy only keeps off its metal word's hero helm, by the code; the T4 hero's everyday alloy keeps an open face, C12)
    static const uint8_t kHeroHelm[F::B_COUNT] = {0, 0, 0, 9, 2, 1, 12, 8, 3, 4, 0};
    const bool heart = c.archetype == cult::Archetype::Heartland;
    const int ownI = heart ? kHeroHelm[wordHeroBase(alloyMetalWord(c.arms.alloys[(size_t)i], std::min(i, 2)))] : own;
    uint8_t cand[16];
    int nc = 0;
    auto push = [&](int v) {
      for (int k = 0; k < nc; k++) if (cand[k] == v) return;
      if (nc < 16) cand[nc++] = (uint8_t)v;
    };
    const int rot = (int)(fam % 4u);
    if (i == 0) push(kVocab[ar][(fam >> 4) % 2u]);   // the everyday alloy in one of the people's two most typical heads
    // the crest's own helm where the people makes it (in its first four)
    for (int k = 0; k < 4; k++) if (kVocab[ar][k] == kCrestHelm[s.crest < 15 ? s.crest : 0]) push(kVocab[ar][k]);
    for (int k = 0; k < 4; k++) push(kVocab[ar][(k + rot + 2 * i) % 4]);
    push(kVocab[ar][4]); push(kVocab[ar][5]);
    for (int k = 0; k < 10; k++) push(kAll[(k + (int)(fam >> 8) % 10) % 10]);
    // a culture's alloys never share a head; the open domed helms (nasal, kettle, spangen, crested) read as one shape at 1x,
    // so the first pass wants a head of another shape than every earlier alloy's, the second just another helm
    // (the coif and the aventail, both a mail hood, are one shape too)
    auto shape = [](int h) { return (h == 1 || h == 2 || h == 4 || h == 9) ? 1 : h == 14 ? 11 : h; };
    int pick = cand[0];
    bool found = false;
    for (int pass = 0; pass < 2 && !found; pass++)
      for (int k = 0; k < nc && !found; k++) {
        bool taken = false;
        for (int u = 0; u < nu; u++) if (pass == 0 ? shape(used[u]) == shape(cand[k]) : used[u] == cand[k]) taken = true;
        if (cand[k] == ownI || (pass == 0 && i == 0 && !heart && ownI && shape(ownI) == shape(cand[k]))) taken = true;
        if (!taken) { pick = cand[k]; found = true; }
      }
    if (nu < 8) used[nu++] = (uint8_t)pick;
    if (i < alloyIndex) continue;
    g.helm = (uint8_t)pick;
    // the skirt by the hem's edge: the long hems (strips, dagged, scalloped, fringed, flame) knee length; the plain, rolled,
    // spiked and stepped short; the rest mid
    switch (s.edge) {
      case 3: case 4: case 5: case 11: case 12: g.skirt = 4; break;
      case 1: case 2: case 7: case 10: g.skirt = 2; break;
      default: g.skirt = 3; break;
    }
    const int rank = std::min(i, 2);
    g.pauldron = (uint8_t)(rank == 0 ? 2 : rank == 1 ? 3 : 4);
    // the masterwork cloaked: a fur mantle over the ruff, rolled and coif collars, else the long noble cloak
    if (rank == 2) g.cloak = (uint8_t)((s.collar == 1 || s.collar == 11 || s.collar == 12) ? 2 : 3);
  }
  return F::packGuise(g);
}

}  // namespace gear

// ======================================================================== (forge round 2) the silhouette (contracts 3c.1)
namespace gear {

namespace {
#include "rpg/sim/gear_forge_shapes.inc"   // kShapeTable[12][36][3], kShapePoolHash (rpg_test --forge-solve; never hand-edited)

void toSx(const Shape& s, uint8_t v[F::SX_COUNT]) {
  v[F::SX_HELM] = s.helm; v[F::SX_SKIRT] = s.skirt; v[F::SX_MASS] = s.mass; v[F::SX_LAYER] = s.layer; v[F::SX_ARM] = s.arm; v[F::SX_MOTIF] = s.motif;
}
Shape fromSx(const uint8_t v[F::SX_COUNT]) {
  Shape s;
  s.helm = v[F::SX_HELM]; s.skirt = v[F::SX_SKIRT]; s.mass = v[F::SX_MASS]; s.layer = v[F::SX_LAYER]; s.arm = v[F::SX_ARM]; s.motif = v[F::SX_MOTIF];
  return s;
}
bool latticeCell(const cult::Culture& c, int& ci, int& cj) {
  const uint64_t id = c.id, k = id >> 60;
  if (!((k == 1 && (id & 0xFFFFFFFFull) == 0) || k == 2)) return false;
  ci = (int)((id >> 46) & 0x3FFF); cj = (int)((id >> 32) & 0x3FFF);
  if (ci >= 8192) ci -= 16384;
  if (cj >= 8192) cj -= 16384;
  return true;
}
int peopleRow(const cult::Culture& c) { return std::clamp((int)c.archetype, 0, 11); }
}  // namespace

uint32_t packShape(const Shape& s) {
  uint8_t v[F::SX_COUNT];
  toSx(s, v);
  bool any = false;
  for (uint8_t x : v) any |= x != 0;
  return any ? F::packShapeAxes(v) : 0u;
}
Shape unpackShape(uint32_t p) { uint8_t v[F::SX_COUNT]; F::unpackShapeAxes(p, v); return fromSx(v); }
uint32_t shapeKey(const Shape& s) { return (uint32_t)s.helm | (uint32_t)s.skirt << 8 | (uint32_t)s.mass << 16 | (uint32_t)s.layer << 24; }
Shape shapeOfKey(uint32_t k) {
  Shape s;
  s.helm = (uint8_t)(k & 255u); s.skirt = (uint8_t)(k >> 8 & 255u); s.mass = (uint8_t)(k >> 16 & 255u); s.layer = (uint8_t)(k >> 24 & 255u);
  return s;
}
int keyAxesApart(const Shape& a, const Shape& b) { return (a.helm != b.helm) + (a.skirt != b.skirt) + (a.mass != b.mass) + (a.layer != b.layer); }
int shapeRank(int alloyIndex) { return std::clamp(alloyIndex, 0, 2); }

int forgeSchool6(const cult::Culture& c) {
  int ci = 0, cj = 0;
  if (latticeCell(c, ci, cj)) return posmod(ci, 6) + 6 * posmod(cj, 6);
  const int s = (int)c.seed - 1;
  return posmod(s * 12 + peopleRow(c) + 6 * (s >= 0 ? s / 3 : 0), 36);
}

std::string describeShape(const Shape& s) {
  if (!packShape(s)) return "none";
  auto nm = [](int a, int v) { return v ? std::string(F::shapeName(a, v)) : std::string("-"); };
  return nm(F::SX_HELM, s.helm) + " " + nm(F::SX_SKIRT, s.skirt) + " " + nm(F::SX_MASS, s.mass) + " " + nm(F::SX_LAYER, s.layer) + " | " +
         nm(F::SX_ARM, s.arm) + " " + nm(F::SX_MOTIF, s.motif);
}

Shape heroShape(int base, const cult::Culture* people) {
  static const uint8_t k[F::B_COUNT][F::SX_COUNT] = {
      {0, 0, 0, 0, 0, 0},
      {F::H_HOOD, F::SK_HAUBERK, F::MS_ROUND, F::LY_NONE, F::AR_CLOSE, 0},      // HOMESPUN
      {F::H_CAP, F::SK_STRIPS, F::MS_DROP, F::LY_NONE, F::AR_CLOSE, 0},         // TANNER'S
      {F::H_CRESTED, F::SK_STRIPS, F::MS_ROUND, F::LY_NONE, F::AR_CLOSE, 0},    // FOUNDRY
      {F::H_KETTLE, F::SK_TASSETS, F::MS_BROAD, F::LY_NONE, F::AR_CLOSE, 0},    // WROUGHT
      {F::H_CONE, F::SK_TASSETS, F::MS_TALL, F::LY_NONE, F::AR_CLOSE, 0},       // ARMOURER'S
      {F::H_ONION, F::SK_TASSETS, F::MS_ROUND, F::LY_NONE, F::AR_CLOSE, 0},     // PARADE
      {F::H_MASK, F::SK_TASSETS, F::MS_TALL, F::LY_NONE, F::AR_CLOSE, 0},       // KNAPPED
      {F::H_FURNACE, F::SK_TASSETS, F::MS_BROAD, F::LY_NONE, F::AR_HEAVY, 0},   // FURNACE
      {F::H_CRYSTAL, F::SK_SPLIT, F::MS_TALL, F::LY_NONE, F::AR_CLOSE, 0},      // STARFALL
      {F::H_LADDER, F::SK_TASSETS, F::MS_TALL, F::LY_CAPE, F::AR_CLOSE, 0}};    // MYTHIC
  if (base < 1 || base >= F::B_COUNT) return Shape{};
  Shape s = fromSx(k[base]);
  if (people && people->archetype != cult::Archetype::Heartland) {
    const int hf = (int)people->arms.helm[0];
    using H = cult::HelmForm;
    const bool signature = hf == (int)H::Horned || hf == (int)H::ConicalAventail || hf == (int)H::Masked || hf == (int)H::Winged;
    if (base >= F::B_TANNER && base <= F::B_ARMOURER) {
      s.helm = (uint8_t)F::helmFamOfForm(hf);
      using B = cult::BodyArm;
      const cult::ArmsStyle& A = people->arms;
      B form = A.body[0];
      if (base == F::B_TANNER) form = (A.body[1] == B::Padded || A.body[1] == B::Leather) ? A.body[1] : ((A.body[0] == B::Padded || A.body[0] == B::Leather) ? A.body[0] : B::Leather);
      s.skirt = (uint8_t)F::skirtOfBodyArm((int)form);
    } else if (signature && base != F::B_MYTHIC) {
      s.helm = (uint8_t)F::helmFamOfForm(hf);
    }
  }
  return s;
}

namespace {
const uint32_t (*g_shapeOverride)[36][3] = nullptr;   // (INTEGRATE silhouettes i1) the solver's candidate table, test only
}
void setShapeTableOverride(const uint32_t (*t)[36][3]) { g_shapeOverride = t; }
uint32_t tableKey(int people, int school, int rank) {
  if (people < 0 || people >= 12 || school < 0 || school >= 36 || rank < 0 || rank > 2) return 0;
  if (g_shapeOverride) return g_shapeOverride[people][school][rank];
  return kShapeTable[people][school][rank];
}
const uint32_t (*shapeTable())[36][3] { return g_shapeOverride ? g_shapeOverride : kShapeTable; }
uint64_t shapeTablePoolHash() { return kShapePoolHash; }

// ---------------------------------------------------------------- the details (contracts 3c.1)
namespace {
// STYLE_P2 28.5's root words: roots, then the shoulder shapes, crests, motifs, arm and collars they ask for
struct RootDetail { const char* roots; const char* shoulder; const char* crest; const char* motif; const char* arm; const char* collar; };
const RootDetail kRootDetails[] = {
    {"RIME FROST", "crystal scale", "spike", "bands", "heavy", "high"},
    {"STORM WIND SKY", "flared", "wings", "chevron", "", ""},
    {"WOLF LION JAGUAR", "pelt fur", "", "disc", "", "ruff"},
    {"PEAT CRAG OAK LOCK", "slab vented", "brow finial", "bands keel", "heavy", "riveted"},
    {"THORN BRIAR ARROW", "shard flared", "spike", "chevron", "", "angular"},
    {"STAG", "layered leaf", "horns", "chevron", "", ""},
    {"CROWN AUREL SOL SUN DAWN", "lobed", "crown fin", "rays disc", "", "flared"},
    {"LEGION GUILD COIN", "plate slab", "comb", "bands straps disc", "", ""},
    {"EAGLE FALCON QUETZ HERON CRANE", "winged flared feather", "wings fin", "keel rays", "", ""},
    {"SAND DUNE MIST REED EEL", "lobed flap", "finial", "bands plain", "wide", "coif stand"},
    {"DRAGON", "shard scale", "horns", "bib", "", "angular"},
    {"JADE PEARL AMBER MOON", "cop lobed", "finial", "disc", "", "high"},
    {"TIDE LEAF", "lobed leaf", "fin", "chevron", "", ""},
    {"HORSE", "layered", "", "disc", "", ""},
    {"STAR SPIRE", "crystal", "spire crown", "rays keel", "", ""},
};
// the hidden peoples' words (the masterwork): motif, shoulder, crest, collar
struct HiddenDetail { const char* word; const char* motif; const char* shoulder; const char* crest; const char* collar; };
const HiddenDetail kHiddenDetails[6] = {
    {"VEIL", "bib", "", "", "coif"}, {"WHORL", "rays", "lobed", "finial", ""}, {"TIDE", "chevron", "layered", "fin", ""},
    {"PEARL", "disc", "cop", "finial", ""}, {"SHARD", "chevron", "shard", "shards", ""}, {"WRACK", "keel", "", "spike", "angular"}};
bool hasWord(const char* list, const char* w) {
  for (const std::string& x : words(list)) if (x == w) return true;
  return false;
}
const char* const kMetalWords[6] = {"BRONZE", "BRASS", "IRON", "STEEL", "SILVER", "GLASS"};
}  // namespace

std::string alloyRoot(const std::string& name) {
  for (const char* mw : kMetalWords) {
    const size_t k = std::strlen(mw);
    if (name.size() > k && name.compare(name.size() - k, k, mw) == 0) {
      const std::string pre = name.substr(0, name.size() - k);
      for (const RootDetail& r : kRootDetails) if (hasWord(r.roots, pre.c_str())) return pre;
      return "";
    }
  }
  return "";
}

AlloyDetails alloyDetails(const cult::Culture& c, int alloyIndex, const Shape& key) {
  AlloyDetails D;
  if (alloyIndex < 0 || alloyIndex >= (int)c.arms.alloys.size()) return D;
  const cult::Alloy& A = c.arms.alloys[(size_t)alloyIndex];
  const int rank = shapeRank(alloyIndex);
  const int word = alloyMetalWord(A, rank);
  const int sheen = std::clamp((int)A.sheen, 0, 7);
  const int a1 = peopleRow(c), a2 = std::clamp((int)c.archetype2, 0, 11);
  const bool iso = c.isolated && a2 != a1;
  uint32_t seed = fnv(2166136261u, A.name);
  std::vector<std::string> reagents;
  for (const cult::Ingredient& g : A.recipe) if (g.ore < 0) reagents.push_back(g.reagent);
  const std::string root = alloyRoot(A.name);
  const RootDetail* rd = nullptr;
  for (const RootDetail& r : kRootDetails) if (!root.empty() && hasWord(r.roots, root.c_str())) rd = &r;
  if (rd) D.root = root;
  const HiddenDetail* hd = nullptr;
  if (c.hidden && rank == 2)
    for (const HiddenDetail& h : kHiddenDetails) {
      const size_t k = std::strlen(h.word);
      if (A.name.size() >= k && A.name.compare(A.name.size() - k, k, h.word) == 0) hd = &h;
    }
  // the culture's earlier alloys (the walk prefers another motif and another arm for each)
  std::vector<AlloyDetails> earlier;
  for (int j = 0; j < alloyIndex; j++) earlier.push_back(alloyDetails(c, j, shapeOfKey(tableKey(a1, forgeSchool6(c), shapeRank(j)))));
  auto jitter = [&](int axis, int v) { return (int)(mix(seed ^ (uint32_t)axis * 0x9E3779B9u ^ (uint32_t)v * 0x85EBCA6Bu) % 17u); };
  // a menu axis of the silhouette (arm, motif): the people's menu (both for an isolated family), the root's and the hidden
  // word's asks first, the sheen's filter on the motif
  auto pickShapeAxis = [&](int axis, const char* rootAsk, const char* hidAsk) -> uint8_t {
    struct C { uint8_t v; long s; };
    std::vector<C> cs;
    for (int pass = 0; pass < (iso ? 2 : 1); pass++) {
      uint8_t vals[8]; int wts[8];
      const int n = F::shapeMenu(pass ? a2 : a1, axis, vals, wts, nullptr, 8);
      for (int i = 0; i < n; i++) {
        bool dup = false;
        for (C& x : cs) if (x.v == vals[i]) { dup = true; x.s = std::max(x.s, (long)wts[i] * (pass ? 13 : 16) / 16); }
        if (!dup) cs.push_back(C{vals[i], (long)wts[i] * (pass ? 13 : 16) / 16});
      }
    }
    if (axis == F::SX_MOTIF) {   // the sheen's filter (STYLE_P2 28.7): banded BANDS; iridescent BIB BANDS CHEVRON; glowing seam-rich
      const char* filt = sheen == (int)cult::Sheen::Banded ? "bands" : sheen == (int)cult::Sheen::Iridescent ? "bib bands chevron" :
                         sheen == (int)cult::Sheen::Glowing ? "bands chevron keel rays" : nullptr;
      if (filt) {
        std::vector<C> kept;
        for (const C& x : cs) if (hasWord(filt, F::shapeName(axis, x.v))) kept.push_back(x);
        if (kept.empty()) kept.push_back(C{(uint8_t)F::shapeValueOf(axis, words(filt)[0].c_str()), 200});
        cs = kept;
      }
    }
    for (C& x : cs) {
      const char* w = F::shapeName(axis, x.v);
      if (hidAsk && *hidAsk && hasWord(hidAsk, w)) x.s += 4000;
      if (rootAsk && *rootAsk && hasWord(rootAsk, w)) x.s += 2000;
      for (const AlloyDetails& e : earlier) if ((axis == F::SX_MOTIF ? e.motif : e.arm) == x.v) x.s -= 300;
      x.s += jitter(axis + 16, x.v);
    }
    std::stable_sort(cs.begin(), cs.end(), [](const C& p, const C& q) { return p.s > q.s; });
    return cs.empty() ? (uint8_t)1 : cs[0].v;
  };
  D.arm = pickShapeAxis(F::SX_ARM, rd ? rd->arm : "", "");
  D.motif = pickShapeAxis(F::SX_MOTIF, rd ? rd->motif : "", hd ? hd->motif : "");
  // a ForgeStyle axis (shoulder, crest, collar): its candidates scored by round 1's boosts, the root's and hidden word's asks
  auto scoreStyle = [&](int axis, const std::vector<uint8_t>& cands, const char* rootAsk, const char* hidAsk) -> uint8_t {
    long best = -1;
    uint8_t pick = 0;
    for (size_t i = 0; i < cands.size(); i++) {
      const uint8_t v = cands[i];
      const char* w = F::valueName(axis, v);
      long s = 256 - (long)i * 24;
      s = s * boost(kWordBoost[word], axis, w) / 256;
      s = s * boost(kSheenBoost[sheen], axis, w) / 256;
      s = s * boost(kRankBoost[rank], axis, w) / 256;
      for (const std::string& r : reagents)
        for (const Reagent& rg : kReagent) if (r.find(rg.key) != std::string::npos) s = s * boost(rg.list, axis, w) / 256;
      for (const Orn& o : kOrn) if (c.arms.ornament & o.bit) s = s * boost(o.list, axis, w) / 256;
      if (hidAsk && *hidAsk && hasWord(hidAsk, w)) s += 4000;
      if (rootAsk && *rootAsk && hasWord(rootAsk, w)) s += 2000;
      s += jitter(axis, v);
      if (s > best) { best = s; pick = v; }
    }
    return pick;
  };
  {
    uint8_t ms[5];
    const int n = F::massShapes(key.mass, ms);
    std::vector<uint8_t> cands(ms, ms + n);
    // the MANTLE in the people's own hide (STYLE_P2 28.5): the SunTemple's feathers and pelt first
    if (key.mass == F::MS_MANTLE && a1 == (int)cult::Archetype::SunTemple) std::reverse(cands.begin(), cands.end());
    D.shoulder = n ? scoreStyle(F::AX_SHOULDER, cands, rd ? rd->shoulder : "", hd ? hd->shoulder : "") : 0;
  }
  if (F::helmSocket(key.helm)) {
    std::vector<uint8_t> cands;
    // the crests a family's socket takes (the CRESTED family's comb is the crest; the kabuto a crescent, a disc or horns)
    static const char* const kFamCrest[F::H_COUNT] = {"", "spike finial knife ridge horns wings", "brow finial spike", "", "comb fin horns wings crown",
                                                      "", "comb knife fin ridge", "finial spike", "", "", "", "comb ridge", "comb spike",
                                                      "horns fin crown", "comb knife", "spike finial", "crown", "", "", "", "", ""};
    const char* fam = kFamCrest[key.helm < F::H_COUNT ? key.helm : 0];
    auto add = [&](const std::string& w) {
      const int v = F::valueOf(F::AX_CREST, w.c_str());
      if (v > 0 && hasWord(fam, w.c_str()) && std::find(cands.begin(), cands.end(), (uint8_t)v) == cands.end()) cands.push_back((uint8_t)v);
    };
    if (hd) for (const std::string& w : words(hd->crest)) add(w);
    if (rd) for (const std::string& w : words(rd->crest)) add(w);
    for (const std::string& w : words(kVocab[a1].ax[F::AX_CREST])) add(w);
    if (iso) for (const std::string& w : words(kVocab[a2].ax[F::AX_CREST])) add(w);
    for (const std::string& w : words(fam)) add(w);   // (the family's own when the people's menu has none of them)
    D.crest = cands.empty() ? 0 : scoreStyle(F::AX_CREST, cands, rd ? rd->crest : "", hd ? hd->crest : "");
    // (silhouettes i2, the blind panel: "spike-top cones: Froststeel, Thorniron, Thornsteel...") a culture's second alloy with
    // the same root never repeats its crest: the next crest its family's socket takes (the root still sets its shoulder,
    // motif, arm and collar)
    for (int guard = 0; guard < 8 && D.crest; guard++) {
      bool dup = false;
      for (const AlloyDetails& e : earlier) dup |= e.crest == D.crest;
      if (!dup) break;
      cands.erase(std::remove(cands.begin(), cands.end(), D.crest), cands.end());
      if (cands.empty()) { D.crest = 0; break; }
      D.crest = scoreStyle(F::AX_CREST, cands, "", hd ? hd->crest : "");
    }
  }
  {
    std::vector<uint8_t> cands;
    auto add = [&](const std::string& w) {
      const int v = F::valueOf(F::AX_COLLAR, w.c_str());
      if (v > 0 && std::find(cands.begin(), cands.end(), (uint8_t)v) == cands.end()) cands.push_back((uint8_t)v);
    };
    if (hd) for (const std::string& w : words(hd->collar)) add(w);
    if (rd) for (const std::string& w : words(rd->collar)) add(w);
    for (const std::string& w : words(kVocab[a1].ax[F::AX_COLLAR])) add(w);
    if (iso) for (const std::string& w : words(kVocab[a2].ax[F::AX_COLLAR])) add(w);
    D.collar = scoreStyle(F::AX_COLLAR, cands, rd ? rd->collar : "", hd ? hd->collar : "");
  }
  return D;
}

Shape alloyShape(const cult::Culture& c, int alloyIndex) {
  if (alloyIndex < 0 || alloyIndex >= (int)c.arms.alloys.size()) return Shape{};
  const uint32_t k = tableKey(peopleRow(c), forgeSchool6(c), shapeRank(alloyIndex));
  if (!k) return Shape{};
  Shape s = shapeOfKey(k);
  const AlloyDetails d = alloyDetails(c, alloyIndex, s);
  s.arm = d.arm; s.motif = d.motif;
  return s;
}

Shape shapeOf(const Item& it, const cult::Culture* maker) {
  const ForgeStyle st = forgeOf(it, maker);
  if (!packForge(st)) return Shape{};
  if (st.base == F::B_ALLOY) {
    if (it.mat == Mat::Alloy && maker && it.alloy >= 1 && it.alloy <= maker->arms.alloys.size()) return alloyShape(*maker, it.alloy - 1);
    return Shape{};
  }
  return heroShape(st.base, maker);
}

// ---------------------------------------------------------------- the board set (contracts 3c.1, STYLE_P2 28.10)
std::vector<BoardCulture> forgeBoardSet() {
  std::vector<BoardCulture> v;
  for (uint32_t sd = 1; sd <= 3; sd++)
    for (int p = 0; p < 12; p++) {
      const bool owner = sd == (p == 6 || p == 7 ? 2u : 1u);   // the owner's board: seed 1, the Marsh and Jade seed 2
      v.push_back(BoardCulture{p, sd, owner});
    }
  v.push_back(BoardCulture{7, 5, true});   // the owner's Jade seed 5 (JAONBRASS, DRAGONSTEEL, UNSHISILVER)
  return v;
}
const char* const* forgeTwinGroups(int* n) {
  static const char* const k[] = {"KISTEEL JAONBRASS DRAGONSTEEL UHABRASS GREBRASS STAGBRONZE", "BRIARSTEEL GRESSTEEL THAERGLASS",
                                  "SUNBRONZE BAKOSTEEL SLAURIRON JAGUARIRON MIKASTEEL",
                                  "ASIRON PASABRONZE THORNIRON UNSHISILVER ANIBRASS CRANESILVER LAVISILVER",
                                  "FROSTSTEEL SANDSTEEL PAPIRON GWESTEEL ENKHOBRASS", "ROLSTEEL IKOSILVER"};
  *n = (int)(sizeof(k) / sizeof(k[0]));
  return k;
}

// ---------------------------------------------------------------- the solver
std::vector<std::pair<int, Shape>> materialKeys(int people) {
  std::vector<std::pair<int, Shape>> out;
  for (uint32_t sd = 1; sd <= 8; sd++) {
    const cult::Culture c = cult::Atlas::make((cult::Archetype)std::clamp(people, 0, 11), sd);
    for (int b = F::B_HOMESPUN; b < F::B_COUNT; b++) {
      const Shape s = heroShape(b, &c);
      bool dup = false;
      for (const auto& o : out) if (o.first == b && shapeKey(o.second) == shapeKey(s)) dup = true;
      if (!dup) out.push_back({b, s});
    }
  }
  return out;
}

ShapeModel estimateModel() {
  ShapeModel m;
  auto ax = [](const Shape& s, uint8_t v[F::SX_COUNT]) { toSx(s, v); v[F::SX_ARM] = 0; v[F::SX_MOTIF] = 0; };
  m.body = [ax](int, const Shape& a, const Shape& b) { uint8_t x[F::SX_COUNT], y[F::SX_COUNT]; ax(a, x); ax(b, y); return F::estBodyOD(x, y); };
  m.bodyX = [ax](int pa, const Shape& a, int pb, const Shape& b) {
    uint8_t x[F::SX_COUNT], y[F::SX_COUNT]; ax(a, x); ax(b, y);
    return F::estBodyOD(x, y) + (pa != pb ? 0.02 : 0.0);   // two peoples' kits differ a little in their own forms
  };
  m.head = [](int a, int b) { return F::estHeadOD(a, b); };
  m.material = [ax](int people, const Shape& a, int base) {
    const cult::Culture c = cult::Atlas::make((cult::Archetype)std::clamp(people, 0, 11), 1);
    uint8_t x[F::SX_COUNT], y[F::SX_COUNT]; ax(a, x); ax(heroShape(base, &c), y);
    return F::estBodyOD(x, y) + 0.02;   // a hero style's own drawing (its cut and hang) adds to its outline
  };
  return m;
}

namespace {
struct KeyCand { uint32_t key; Shape s; double score; };
// the board set's cells: the kit cultures of seeds 1..3 and the owner's Jade seed 5 (contracts 3c.1, forge round 2)
std::vector<std::pair<int, int>> boardCells() {
  std::vector<std::pair<int, int>> v;
  for (int sd = 1; sd <= 3; sd++)
    for (int p = 0; p < 12; p++) { cult::Culture c; c.archetype = (cult::Archetype)p; c.seed = (uint32_t)sd; c.id = 0; v.push_back({p, forgeSchool6(c)}); }
  { cult::Culture c; c.archetype = cult::Archetype::Jade; c.seed = 5; c.id = 0; v.push_back({7, forgeSchool6(c)}); }
  return v;
}
bool flavourHas(int rank, int axis, int v) {
  const std::string w = std::string(F::shapeAxisName(axis)) + ":" + F::shapeName(axis, v);
  for (const std::string& x : words(F::shapeRankFlavour(rank))) if (x == w) return true;
  return false;
}
}  // namespace

std::vector<std::string> checkShapeTable(const uint32_t (*t)[36][3], const ShapeModel& m) {
  std::vector<std::string> out;
  char b[256];
  std::vector<std::vector<std::pair<int, Shape>>> mats(12);
  for (int p = 0; p < 12; p++) mats[(size_t)p] = materialKeys(p);
  std::unordered_map<uint32_t, int> owner;
  for (int p = 0; p < 12; p++)
    for (int s = 0; s < 36; s++) {
      const uint32_t* k = t[p][s];
      for (int r = 0; r < 3; r++) {
        const Shape x = shapeOfKey(k[r]);
        const char* pn = cult::archetypeName((cult::Archetype)p);
        if (!k[r]) { std::snprintf(b, sizeof b, "rule 1: %s school %d rank %d: no key", pn, s, r); out.push_back(b); continue; }
        const bool menu = F::inShapeMenu(p, F::SX_HELM, x.helm) && F::inShapeMenu(p, F::SX_SKIRT, x.skirt) && F::inShapeMenu(p, F::SX_MASS, x.mass) &&
                          F::inShapeMenu(p, F::SX_LAYER, x.layer) && (x.layer != F::LY_CAPE || r == 2);
        if (!menu) { std::snprintf(b, sizeof b, "rule 1: %s school %d rank %d: %s outside the menu", pn, s, r, describeShape(x).c_str()); out.push_back(b); }
        auto it = owner.find(k[r]);
        if (it != owner.end() && it->second != s) {
          std::snprintf(b, sizeof b, "rule 3: %s school %d rank %d: %s also in school %d", pn, s, r, describeShape(x).c_str(), it->second); out.push_back(b);
        } else owner[k[r]] = s;
        for (const auto& mk : mats[(size_t)p]) {
          const double od = m.material(p, x, mk.first);
          if (keyAxesApart(x, mk.second) < 2 || od < 0.06) {
            std::snprintf(b, sizeof b, "rule 4: %s school %d rank %d: %s vs %s (%s): %d axes, body OD %.3f", pn, s, r, describeShape(x).c_str(),
                          F::valueName(F::AX_BASE, mk.first), describeShape(mk.second).c_str(), keyAxesApart(x, mk.second), od);
            out.push_back(b);
            break;
          }
        }
      }
      for (int i = 0; i < 3; i++)
        for (int j = i + 1; j < 3; j++) {
          const Shape x = shapeOfKey(k[i]), y = shapeOfKey(k[j]);
          const int body = (x.skirt != y.skirt) + (x.mass != y.mass) + (x.layer != y.layer);
          const double od = m.body(p, x, y);
          if (x.helm == y.helm || body < 2 || od < 0.08) {
            std::snprintf(b, sizeof b, "rule 2: %s school %d ranks %d %d: %s vs %s (helm %s, %d body axes, body OD %.3f)", cult::archetypeName((cult::Archetype)p), s, i,
                          j, describeShape(x).c_str(), describeShape(y).c_str(), x.helm == y.helm ? "shared" : "apart", body, od);
            out.push_back(b);
          }
        }
    }
  return out;
}

namespace {
// (silhouettes i3) helm families that read alike at true 1x (the blind panel's groups)
bool helmAlike(int a, int b) {
  if (a == b) return true;
  auto in = [&](std::initializer_list<int> g) {
    bool x = false, y = false;
    for (int v : g) { x |= v == a; y |= v == b; }
    return x && y;
  };
  return in({F::H_WARHAT, F::H_KABUTO, F::H_KETTLE}) || in({F::H_HORNED, F::H_WINGED}) || in({F::H_CONE, F::H_VEIL, F::H_ONION}) ||
         in({F::H_GREAT, F::H_SNOUT, F::H_MASK});
}
}  // namespace
ShapeSolve solveShapes(const ShapeModel& m) {
  ShapeSolve S;
  // the candidates per people and rank: inside its menus (rule 1), two axes and a body OD 0.06 off each material (rule 4)
  std::vector<KeyCand> cand[12][3];
  for (int p = 0; p < 12; p++) {
    const auto mats = materialKeys(p);
    uint8_t hv[8], sv[8], mv[8], lv[8];
    int hw[8], sw[8], mw[8], lw[8];
    bool hs[8], ss[8], msx[8], ls[8];
    const int nh = F::shapeMenu(p, F::SX_HELM, hv, hw, hs, 8), ns = F::shapeMenu(p, F::SX_SKIRT, sv, sw, ss, 8);
    const int nm = F::shapeMenu(p, F::SX_MASS, mv, mw, msx, 8), nl = F::shapeMenu(p, F::SX_LAYER, lv, lw, ls, 8);
    for (int a = 0; a < nh; a++)
      for (int b = 0; b < ns; b++)
        for (int c = 0; c < nm; c++)
          for (int d = 0; d < nl; d++) {
            Shape s;
            s.helm = hv[a]; s.skirt = sv[b]; s.mass = mv[c]; s.layer = lv[d];
            bool ok = true;
            for (const auto& mk : mats) if (keyAxesApart(s, mk.second) < 2 || m.material(p, s, mk.first) < 0.06) { ok = false; break; }
            if (!ok) continue;
            for (int r = 0; r < 3; r++) {
              if (s.layer == F::LY_CAPE && r != 2) continue;
              double sc = std::log((double)hw[a]) + std::log((double)sw[b]) + std::log((double)mw[c]) + std::log((double)lw[d]);
              int fl = flavourHas(r, F::SX_SKIRT, s.skirt) + flavourHas(r, F::SX_MASS, s.mass) + flavourHas(r, F::SX_LAYER, s.layer);
              sc += 0.4 * fl;
              if (r == 2) sc += 0.6 * ((hs[a] ? 1 : 0) + (ss[b] ? 1 : 0) + (msx[c] ? 1 : 0) + (ls[d] ? 1 : 0));   // the signature on most masterworks
              // (silhouettes i2, the blind panel: "a crown on a mid-tier hauberk muddles the ladder: a crown should mean high
              // rank") the CROWN helm leans to the masterwork
              if (s.helm == F::H_CROWN && r < 2) sc -= 1.5;
              cand[p][r].push_back(KeyCand{shapeKey(s), s, sc});
            }
          }
    for (int r = 0; r < 3; r++) std::stable_sort(cand[p][r].begin(), cand[p][r].end(), [](const KeyCand& x, const KeyCand& y) { return x.score > y.score; });
  }
  // the processing order: the board set's cells first (C18a holds them hard), then every school's cells
  const std::vector<std::pair<int, int>> board = boardCells();
  std::vector<std::pair<int, int>> order = board;
  // then the peoples with the fewest candidates first (the Heartland's three skirts and masses), every school in turn
  std::vector<int> byTight(12);
  for (int p = 0; p < 12; p++) byTight[(size_t)p] = p;
  std::stable_sort(byTight.begin(), byTight.end(), [&](int a, int b) { return cand[a][0].size() < cand[b][0].size(); });
  for (int p : byTight)
    for (int s = 0; s < 36; s++)
      if (std::find(order.begin(), order.end(), std::make_pair(p, s)) == order.end()) order.push_back({p, s});
  auto isBoard = [&](int p, int s) { return std::find(board.begin(), board.end(), std::make_pair(p, s)) != board.end(); };
  // (silhouettes i2) the owner's round-1 board (37 alloys side by side): its cells, for the helm families' spread
  std::vector<std::pair<int, int>> ownerCells;
  for (const BoardCulture& bc : forgeBoardSet())
    if (bc.owner) { cult::Culture c; c.archetype = (cult::Archetype)bc.people; c.seed = bc.seed; c.id = 0; ownerCells.push_back({bc.people, forgeSchool6(c)}); }
  auto isOwner = [&](int p, int s) { return std::find(ownerCells.begin(), ownerCells.end(), std::make_pair(p, s)) != ownerCells.end(); };
  // C18c's windows (soft): worlds 1..6, the 6 x 6 windows at cells 0..5 and 3..8: each window's (people, school) cells and
  // its alloy count; no body outline on more than 4 % and no helm family on more than 20 % of a window's alloys
  struct Win { std::vector<std::pair<int, int>> cells; int alloys = 0; };
  std::vector<Win> wins;
  std::vector<int> winsOf[12][36];
  for (uint64_t w = 1; w <= 6; w++) {
    cult::Atlas at(w);
    for (int o : {0, 3}) {
      Win W;
      for (int cj = o; cj < o + 6; cj++)
        for (int ci = o; ci < o + 6; ci++) {
          const cult::Culture& c = at.family(ci, cj);
          W.cells.push_back({peopleRow(c), forgeSchool6(c)});
          W.alloys += (int)std::min<size_t>(c.arms.alloys.size(), 3);
        }
      for (const auto& c : W.cells) winsOf[c.first][c.second].push_back((int)wins.size());
      wins.push_back(W);
    }
  }
  // the state: each key's school (a key owned by one school: rule 3) with a count of its uses
  std::unordered_map<uint32_t, std::pair<int, int>> owner;
  bool set[12][36] = {};
  int helmCount[12][F::H_COUNT] = {}, bodyCount[12][512] = {}, valueUse[12][F::SX_COUNT][16] = {};
  auto bodyIx = [](const Shape& s) { return (s.skirt & 7) | (s.mass & 7) << 3 | (s.layer & 7) << 6; };
  auto apply = [&](int p, int s, int sign) {
    for (int r = 0; r < 3; r++) {
      const uint32_t k = S.table[p][s][r];
      if (!k) continue;
      const Shape x = shapeOfKey(k);
      auto& o = owner[k];
      if (sign > 0) { o.first = s; o.second++; } else if (--o.second <= 0) owner.erase(k);
      helmCount[p][x.helm] += sign;
      bodyCount[p][bodyIx(x)] += sign;
      valueUse[p][F::SX_HELM][x.helm] += sign; valueUse[p][F::SX_SKIRT][x.skirt] += sign;
      valueUse[p][F::SX_MASS][x.mass] += sign; valueUse[p][F::SX_LAYER][x.layer] += sign;
    }
  };
  // the twin test (rule 5): body OD under 0.05 and head OD under 0.10
  // (INTEGRATE silhouettes i1) a pair the real figures measured as twins (rpg_test --forge-solve's C18a passes) is one
  auto known = [&](int pa, const Shape& a, int pb, const Shape& b) { return m.knownTwin && m.knownTwin(pa, a, pb, b); };
  auto twin = [&](int pa, const Shape& a, int pb, const Shape& b) {
    return known(pa, a, pb, b) || (m.head(a.helm, b.helm) < 0.10 && m.bodyX(pa, a, pb, b) < 0.05);
  };
  auto adjacent = [](int s, int t) {
    const int x = s % 6, y = s / 6, u = t % 6, v = t / 6;
    const int dx = std::min(posmod(x - u, 6), posmod(u - x, 6)), dy = std::min(posmod(y - v, 6), posmod(v - y, 6));
    return dx + dy == 1 || (dx == 1 && dy == 1);
  };
  struct Near { int q; Shape s; double w; bool board; int t = 0, rr = 0; bool owner = false; };
  std::vector<Near> nearKeys;
  auto gatherNear = [&](int p, int s, bool boardCell) {
    nearKeys.clear();
    (void)p;
    for (int q = 0; q < 12; q++)
      for (int t = 0; t < 36; t++) {
        if (!set[q][t] || (t == s && q == p)) continue;
        const bool bt = boardCell && isBoard(q, t), at = t != s && adjacent(s, t);
        // (C18a over the windows: every two cultures of a window meet there; a twin between far cells is allowed twice a world)
        bool wt = false;
        if (!bt && !at && t != s)
          for (int wi : winsOf[p][s]) {
            for (const auto& cell : wins[(size_t)wi].cells) if (cell.first == q && cell.second == t) { wt = true; break; }
            if (wt) break;
          }
        if (!bt && !at && !wt) continue;
        for (int rr = 0; rr < 3; rr++)
          if (const uint32_t k = S.table[q][t][rr]) nearKeys.push_back(Near{q, shapeOfKey(k), bt ? 80.0 : at ? 10.0 : 6.0, bt, t, rr, bt && isOwner(q, t)});
      }
  };
  auto keyPenalty = [&](int p, int s, int r, const KeyCand& c, bool boardCell) -> double {
    double pen = 0;
    // spread: a helm family on more than 25 % of a people's 108 keys, a body outline on more than 4 % (4 of 108)
    if (helmCount[p][c.s.helm] >= 20) pen += 0.5 * (helmCount[p][c.s.helm] - 19);
    if (bodyCount[p][bodyIx(c.s)] >= 3) pen += 1.5 * (bodyCount[p][bodyIx(c.s)] - 2);
    for (int a : {(int)F::SX_HELM, (int)F::SX_SKIRT, (int)F::SX_MASS, (int)F::SX_LAYER}) {
      const int v = a == F::SX_HELM ? c.s.helm : a == F::SX_SKIRT ? c.s.skirt : a == F::SX_MASS ? c.s.mass : c.s.layer;
      if (valueUse[p][a][v] == 0) pen -= 0.3;   // every menu value used
    }
    // twins against the keys already placed: the board set's, then the adjacent schools'
    int boardBody = 0;
    for (const Near& n : nearKeys) {
      // the board set's pairs on their real figures where the model measures them (C18a composes the real alloys)
      const bool tw = known(p, c.s, n.q, n.s) ||
                      (n.board && m.boardBody && boardCell
                           ? m.head(c.s.helm, n.s.helm) < 0.10 && m.boardBody(p, s, r, c.s, n.q, n.t, n.rr, n.s) < 0.05
                           : twin(p, c.s, n.q, n.s));
      if (tw) pen += known(p, c.s, n.q, n.s) ? std::max(n.w, 40.0) : n.w;
      if (n.board && n.s.skirt == c.s.skirt && n.s.mass == c.s.mass && n.s.layer == c.s.layer) boardBody++;
    }
    // C18c over the board set: no body outline on more than 4 % of its alloys (3 of about 100)
    if (boardBody >= 3) pen += 8.0 * (boardBody - 2);
    // (silhouettes i2, the blind panel on the owner's 37: "five winged helms, four veils, three war hats") on the owner's board
    // a helm family serves two alloys at most where the menus allow (16 families for 37), and two that share one stand
    // apart on all three body axes; over the whole board set a family serves 8 of about 108 at most
    // (silhouettes i3, the blind panel: "Papiron, Kisteel, Unshisilver and Pasabronze the same wide hat; horned and winged
    // the same two prongs; Gressteel = Slauriron even at 8x") families that read alike at 1x count as one: a wide brim
    // (warhat, kabuto, kettle), two prongs (horned, winged), a pointed dome (cone, veil, onion), a closed bucket (great,
    // snout, mask), and any two the pool measures under head OD 0.16; two alike on the owner's board stand apart on all
    // three body axes
    if (boardCell) {
      double ownHelm = 0;
      int boardHelm = 0;
      for (const Near& n : nearKeys) {
        if (!n.board) continue;
        const bool same = n.s.helm == c.s.helm, like = same || helmAlike(n.s.helm, c.s.helm) || m.head(n.s.helm, c.s.helm) < 0.16;
        if (!like) continue;
        if (same) boardHelm++;
        if (n.owner && isOwner(p, s)) {
          ownHelm += same ? 1.0 : 0.6;
          const int apart = (n.s.skirt != c.s.skirt) + (n.s.mass != c.s.mass) + (n.s.layer != c.s.layer);
          if (apart < 3) pen += same ? 14.0 : 7.0;
          if (apart < 2) pen += same ? 20.0 : 10.0;
        }
      }
      if (ownHelm > 0.0) pen += 2.5 * ownHelm * ownHelm;
      if (boardHelm >= 8) pen += 1.0 * (boardHelm - 7);
    }
    // and over the worlds' windows
    for (int wi : winsOf[p][s]) {
      const Win& W = wins[(size_t)wi];
      int body = 0, helm = 0;
      for (const auto& cell : W.cells) {
        if ((cell.first == p && cell.second == s) || !set[cell.first][cell.second]) continue;
        for (int rr = 0; rr < 3; rr++) {
          const uint32_t k = S.table[cell.first][cell.second][rr];
          if (!k) continue;
          const Shape x = shapeOfKey(k);
          if (x.skirt == c.s.skirt && x.mass == c.s.mass && x.layer == c.s.layer) body++;
          if (x.helm == c.s.helm) helm++;
        }
      }
      if (body + 1 > (int)(0.04 * W.alloys)) pen += 4.0 * (body + 1 - (int)(0.04 * W.alloys));
      if (helm + 1 > (int)(0.20 * W.alloys) - 1) pen += 1.0 * (helm + 2 - (int)(0.20 * W.alloys));
    }
    (void)r;
    return pen;
  };
  auto solveCell = [&](int p, int s) -> bool {
    const bool bc = isBoard(p, s);
    gatherNear(p, s, bc);
    std::vector<KeyCand> top[3];
    for (int r = 0; r < 3; r++) {
      for (const KeyCand& c : cand[p][r]) {
        auto it = owner.find(c.key);
        if (it != owner.end() && it->second.first != s) continue;   // rule 3: owned by another school
        bool boardEqual = false;   // C16u: no two board-set alloys share a key (a same-school culture of another people too)
        for (const Near& n : nearKeys) if (n.board && shapeKey(n.s) == c.key) boardEqual = true;
        if (boardEqual) continue;
        KeyCand x = c;
        x.score -= keyPenalty(p, s, r, c, bc);
        top[r].push_back(x);
      }
      std::stable_sort(top[r].begin(), top[r].end(), [](const KeyCand& x, const KeyCand& y) { return x.score > y.score; });
    }
    for (size_t N : {(size_t)40, (size_t)160, (size_t)100000}) {
      double best = -1e18;
      uint32_t bk[3] = {};
      const size_t n0 = std::min(N, top[0].size()), n1 = std::min(N, top[1].size()), n2 = std::min(N, top[2].size());
      for (size_t i = 0; i < n0; i++)
        for (size_t j = 0; j < n1; j++) {
          const KeyCand &x = top[0][i], &y = top[1][j];
          if (x.s.helm == y.s.helm || x.key == y.key || known(p, x.s, p, y.s)) continue;
          if ((x.s.skirt != y.s.skirt) + (x.s.mass != y.s.mass) + (x.s.layer != y.s.layer) < 2 || m.body(p, x.s, y.s) < 0.08) continue;
          if (x.score + y.score + (n2 ? top[2][0].score : 0) <= best) break;
          for (size_t k = 0; k < n2; k++) {
            const KeyCand& z = top[2][k];
            const double sc = x.score + y.score + z.score;
            if (sc <= best) break;
            if (z.s.helm == x.s.helm || z.s.helm == y.s.helm || z.key == x.key || z.key == y.key || known(p, x.s, p, z.s) || known(p, y.s, p, z.s)) continue;
            if ((x.s.skirt != z.s.skirt) + (x.s.mass != z.s.mass) + (x.s.layer != z.s.layer) < 2 || m.body(p, x.s, z.s) < 0.08) continue;
            if ((y.s.skirt != z.s.skirt) + (y.s.mass != z.s.mass) + (y.s.layer != z.s.layer) < 2 || m.body(p, y.s, z.s) < 0.08) continue;
            best = sc; bk[0] = x.key; bk[1] = y.key; bk[2] = z.key;
          }
        }
      if (bk[0]) {
        for (int r = 0; r < 3; r++) S.table[p][s][r] = bk[r];
        return true;
      }
    }
    return false;
  };
  if (m.start)   // (silhouettes i2) a warm start: every cell holds the given table's keys, its counts applied
    for (int p = 0; p < 12; p++)
      for (int s = 0; s < 36; s++)
        if (m.start[p][s][0]) { for (int r = 0; r < 3; r++) S.table[p][s][r] = m.start[p][s][r]; set[p][s] = true; apply(p, s, +1); }
  // (silhouettes i2) the hero's ladder kit (the Heartland seed 7 culture: lt4, lt5, the guards, the royals and the king judged
  // by C3, B11 and C12 on their shipped helms and hair) keeps its shipped keys: a re-solve never moves the judged ladder's art
  const int pinP = (int)cult::Archetype::Heartland, pinS = forgeSchool6(kitCulture(cult::Archetype::Heartland, 7));
  {
    const uint32_t (*shipped)[36][3] = kShapeTable;
    if (shipped[pinP][pinS][0]) {
      if (set[pinP][pinS]) apply(pinP, pinS, -1);
      for (int r = 0; r < 3; r++) S.table[pinP][pinS][r] = shipped[pinP][pinS][r];
      set[pinP][pinS] = true;
      apply(pinP, pinS, +1);
    }
  }
  for (int pass = 0; pass < 5; pass++)
    for (const auto& c : order) {
      const int p = c.first, s = c.second;
      if (p == pinP && s == pinS && set[p][s]) continue;
      if (set[p][s]) { apply(p, s, -1); set[p][s] = false; }
      const uint32_t keep[3] = {S.table[p][s][0], S.table[p][s][1], S.table[p][s][2]};
      if (solveCell(p, s)) { set[p][s] = true; apply(p, s, +1); }
      else if (keep[0]) { for (int r = 0; r < 3; r++) S.table[p][s][r] = keep[r]; set[p][s] = true; apply(p, s, +1); }
      else if (pass == 4) {
        char b[160];
        std::snprintf(b, sizeof b, "hard: %s school %d: no key triple holds rules 1-4", cult::archetypeName((cult::Archetype)p), s);
        S.log.push_back(b);
        S.hardFails++;
      }
    }
  // the soft summary: twins (board set, adjacent schools, all)
  for (int p = 0; p < 12; p++)
    for (int s = 0; s < 36; s++)
      for (int q = 0; q < 12; q++)
        for (int t = 0; t < 36; t++) {
          if (t < s || (t == s && q <= p)) continue;
          if (t == s && !(isBoard(p, s) && isBoard(q, t))) continue;
          for (int r = 0; r < 3; r++)
            for (int rr = 0; rr < 3; rr++) {
              const uint32_t a = S.table[p][s][r], b = S.table[q][t][rr];
              if (!a || !b || !twin(p, shapeOfKey(a), q, shapeOfKey(b))) continue;
              S.twins++;
              if (isBoard(p, s) && isBoard(q, t)) S.twinsBoard++;
              if (adjacent(s, t)) S.twinsAdjacent++;
            }
        }
  const std::vector<std::string> bad = checkShapeTable(S.table, m);
  for (const std::string& x : bad) S.log.push_back(x);
  S.hardFails = (int)bad.size();
  char b[200];
  std::snprintf(b, sizeof b, "solve: %d hard failures; twins %d (board set %d, adjacent schools %d)", S.hardFails, S.twins, S.twinsBoard, S.twinsAdjacent);
  S.log.push_back(b);
  return S;
}

std::string shapeTableInc(const ShapeSolve& s, uint64_t poolHash, const std::string& how) {
  std::string o;
  char b[96];
  o += "// GENERATED by rpg_test --forge-solve (docs/P2C_CONTRACTS.md 3c.1): the shape table, kShapeTable[people][school][rank]\n";
  o += "// = helm | skirt << 8 | mass << 16 | layer << 24 (art::doll::forge SX_* values). NEVER hand-edit: re-solve.\n";
  o += "// " + how + "\n";
  std::snprintf(b, sizeof b, "static const uint64_t kShapePoolHash = 0x%016llXull;\n", (unsigned long long)poolHash);
  o += b;
  o += "static const uint32_t kShapeTable[12][36][3] = {\n";
  for (int p = 0; p < 12; p++) {
    o += "    {   // " + std::string(cult::archetypeName((cult::Archetype)p)) + "\n";
    for (int sc = 0; sc < 36; sc++) {
      std::snprintf(b, sizeof b, "%s{0x%08Xu, 0x%08Xu, 0x%08Xu}%s", sc % 4 == 0 ? "        " : " ", s.table[p][sc][0], s.table[p][sc][1], s.table[p][sc][2],
                    sc == 35 ? "" : ",");
      o += b;
      if (sc % 4 == 3 || sc == 35) o += "\n";
    }
    o += p == 11 ? "    }\n" : "    },\n";
  }
  o += "};\n";
  return o;
}

}  // namespace gear
