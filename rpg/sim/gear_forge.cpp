// P2c forge styles (rpg/sim/gear_forge.h; docs/P2C_CONTRACTS.md 3c). FORGE-DOLL.
#include "rpg/sim/gear_forge.h"
#include <algorithm>
#include <cstring>
#include <functional>
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
  for (int j = 0; j < alloyIndex; j++) earlier.push_back(alloyStyle(c, j));
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

ForgeStyle alloyStyle(const cult::Culture& c, int alloyIndex) { return alloyPick(c, alloyIndex).style; }

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
    const ForgeStyle s = alloyStyle(c, i);
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
