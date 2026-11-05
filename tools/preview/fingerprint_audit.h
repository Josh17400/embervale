// tools/preview/fingerprint_audit.h (P5 GATES, docs/FIX_BATCH_P5.md 2.1, 5.5, 7): the peoples' fingerprint measure, shared by
// the gallery (tools/preview/fingerprint_gallery.cpp: the sheets) and the gate (tools/tests/test_fingerprint.cpp: rpg_test
// --fingerprint). Productised from the planner's scratch audit (p5_audit.cpp):
//  - the columns: every cult::BodyArm, HelmForm and ShieldForm with the materials and tiers the shops make, the people's own
//    form at every material, gloves, boots and cloaks (enumerated from the enums' COUNT, so a new form joins by itself);
//  - the makers: none (culture 0) and the 12 archetypes, each gear::kitCulture(people, seed);
//  - worn: the piece through wearGear over A3's man and woman in their shirts, idle frame 0, facings D and R, composed by
//    art::doll::probeFrame; the piece's own pixels by the probe's owner and region;
//  - icons: gearIcon -> art::itemIconLook at rig 2 (16 px);
//  - per pair of makers and view: xor (silhouette px), grey (shared px whose luma differs by 12+), distinct (px dE 10+ from
//    every colour of the other's palette); twin = all three within max(min, area / 40) in every view, colour-only = shape
//    and greyscale twins whose palettes differ.
// The fingerprint's contracts (rpg/core/fingerprint.h, HumanLook::makers, the dye slots, FIX_FP) are read when present, so
// one source measures every stage of the batch (before wave 1: no table, no dye slot, FIX_FP a no-op).
// Header-only (inline): two executables include it; nothing here draws text or writes files.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "rpg/art.h"
#include "rpg/art/art_items.h"
#include "rpg/art/art_rig.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_envelope.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_ladder.h"
#include "rpg/sim/gear_look.h"
#if __has_include("rpg/core/fingerprint.h")
#include "rpg/core/fingerprint.h"
#define FPA_HAVE_TABLE 1
#else
#define FPA_HAVE_TABLE 0
#endif

namespace fpa {
namespace dl = art::doll;

// ================================================================ the contracts, read when present
// FIX_FP (FIX_BATCH_P5 4.3): 4096; before wave 1 the bit is outside FIX_ALL and clearing it changes nothing
template <class E> constexpr uint32_t fixFpBit() {
  if constexpr (requires { E::FIX_FP; }) return (uint32_t)E::FIX_FP;
  else return 4096u;
}
inline uint32_t fixFp() { return fixFpBit<dl::FixRule>(); }
// the first dye slot (Dye, Dye2, DyeT0..7, Dye2T0..7, appended before COUNT); COUNT while the slots are not in
template <class S> constexpr int firstDyeSlot() {
  if constexpr (requires { S::Dye; }) return (int)S::Dye;
  else return (int)S::COUNT;
}
inline bool dyeSlot(dl::Slot s) { return (int)s >= firstDyeSlot<dl::Slot>() && (int)s < (int)dl::Slot::COUNT; }
inline bool dyeCode(uint8_t c) { return c >= 2 && dyeSlot(dl::codeSlot(c)); }
inline bool haveDyeSlots() { return firstDyeSlot<dl::Slot>() < (int)dl::Slot::COUNT; }
// HumanLook::makers (4.2): -1 while the field is not in
template <class L> int makerAt(const L& l, int slot) {
  if constexpr (requires { l.makerAt(slot); }) return l.makerAt(slot);
  else return -1;
}
template <class L> bool setMaker(L& l, int slot, int people) {
  if constexpr (requires { l.setMaker(slot, people); }) { l.setMaker(slot, people); return true; }
  else return false;
}
constexpr bool haveTable() { return FPA_HAVE_TABLE != 0; }
struct Dye { uint32_t dye = 0, dye2 = 0; int run = 0, period = 0; const char* name = "none"; const char* motif = ""; };
inline Dye dyeOf(int people) {
#if FPA_HAVE_TABLE
  const fp::Row& r = fp::row(people);
  // (P5b) a signed people's pair is its dominant and accent (STYLE_P5B 3, 8.1), drawn under FIX_SIG
  if (r.sig && dl::fixOn(dl::FIX_SIG) && dl::fixOn(dl::FIX_FP)) return Dye{r.sig->dom, r.sig->acc, r.run, r.period, r.name, r.motif};
  return Dye{r.dye, r.dye2, r.run, r.period, r.name, r.motif};
#else
  (void)people;
  return Dye{};
#endif
}
// FIX_FP on (the game) or off (the pre-P5 look: `--before`, F6, F7), restored on scope exit
struct FpScope {
  uint32_t saved;
  explicit FpScope(bool on) : saved(dl::fixRules()) { dl::fixRules() = on ? saved | fixFp() : saved & ~fixFp(); }
  ~FpScope() { dl::fixRules() = saved; }
  FpScope(const FpScope&) = delete;
  FpScope& operator=(const FpScope&) = delete;
};

// ================================================================ makers and columns
constexpr int NM = 13;   // makers: 0 none, 1..12 cult::Archetype + 1
inline const char* makerName(int m) {
  static const char* const k[NM] = {"none", "fjordfolk", "highland", "heartland", "imperial", "dune", "steppe", "marsh", "jade", "river",
                                    "suntemple", "sylvan", "starspire"};
  return m >= 0 && m < NM ? k[m] : "?";
}
inline const char* makerShort(int m) {
  static const char* const k[NM] = {"none", "fjord", "highl", "heart", "imper", "dune", "steppe", "marsh", "jade", "river", "sunt", "sylvan", "stars"};
  return m >= 0 && m < NM ? k[m] : "?";
}
inline const cult::Culture* maker(int m, uint32_t seed) { return m <= 0 ? nullptr : &gear::kitCulture((cult::Archetype)(m - 1), seed); }

enum Grp { G_BODY, G_HELM, G_SHIELD, G_GLOVES, G_BOOTS, G_CLOAK, G_COUNT };
inline const char* grpName(int g) {
  static const char* const k[G_COUNT] = {"body", "helm", "shield", "gloves", "boots", "cloak"};
  return g >= 0 && g < G_COUNT ? k[g] : "?";
}
// a material at the tier the shops make it (item level and rarity as icons_gallery's recipe)
struct Mt { const char* tag; Mat mat; int ilvl; Rarity rar; int alloy; int tier; };
inline const Mt kLeaT1{"leather", Mat::Leather, 3, Rarity::Common, 0, 1}, kLeaT2{"leather", Mat::Leather, 6, Rarity::Rare, 0, 2},
    kBroT1{"bronze", Mat::Bronze, 4, Rarity::Common, 0, 1}, kIroT2{"iron", Mat::Iron, 10, Rarity::Common, 0, 2},
    kSteT3{"steel", Mat::Steel, 20, Rarity::Common, 0, 3}, kAllT4{"alloy", Mat::Alloy, 28, Rarity::Common, 1, 4},
    kAllT5{"master", Mat::Alloy, 47, Rarity::Rare, 9, 5}, kClothT1{"cloak", Mat::Cloth, 3, Rarity::Common, 0, 1},
    kClothT3{"cloak", Mat::Cloth, 20, Rarity::Common, 0, 3}, kClothT5{"cloak", Mat::Cloth, 47, Rarity::Rare, 0, 5};
struct Col {
  std::string tag;   // "body mail steel T3", "helm own bronze T1" ...
  Grp g;
  ItemKind kind;
  Mt mt;
  int form;   // 0 the people's own form; else the enum value + 1 (Item::form)
};

inline std::string lower(const char* s) { std::string o(s); for (char& c : o) c = (char)std::tolower((unsigned char)c); return o; }
inline const char* bodyArmName(int f) {
  static const char* const k[] = {"padded", "leather", "mail", "scale", "lamellar", "brigandine", "plate", "leaf"};
  return f >= 0 && f < (int)(sizeof(k) / sizeof(k[0])) ? k[f] : "form";
}
inline const char* helmFormName(int f) {
  static const char* const k[] = {"nasal", "kettle", "greathelm", "spangen", "horned", "plumed", "conical", "masked", "crested", "winged"};
  return f >= 0 && f < (int)(sizeof(k) / sizeof(k[0])) ? k[f] : "form";
}
inline const char* shieldFormName(int f) {
  static const char* const k[] = {"round", "kite", "heater", "tower", "crescent", "oval", "buckler", "leaf", "none"};
  return f >= 0 && f < (int)(sizeof(k) / sizeof(k[0])) ? k[f] : "form";
}
inline std::string colTag(Grp g, const char* form, const Mt& m) { return std::string(grpName(g)) + " " + form + " " + m.tag + " T" + std::to_string(m.tier); }

// every column (FIX_BATCH_P5 2.1, widened to every form): the people's own form at each material, then each form forced
inline std::vector<Col> columns() {
  using B = cult::BodyArm;
  std::vector<Col> v;
  for (const Mt* m : {&kLeaT1, &kLeaT2, &kBroT1, &kIroT2, &kSteT3, &kAllT4, &kAllT5}) v.push_back({colTag(G_BODY, "own", *m), G_BODY, ItemKind::Armor, *m, 0});
  for (int f = 0; f < (int)B::COUNT; f++) {
    const bool soft = (B)f == B::Padded || (B)f == B::Leather;
    if (soft) {
      for (const Mt* m : {&kLeaT1, &kLeaT2}) v.push_back({colTag(G_BODY, bodyArmName(f), *m), G_BODY, ItemKind::Armor, *m, f + 1});
      continue;
    }
    for (const Mt* m : {&kBroT1, &kIroT2, &kSteT3}) v.push_back({colTag(G_BODY, bodyArmName(f), *m), G_BODY, ItemKind::Armor, *m, f + 1});
    // an alloy keeps a forced form unless its finer cut replaces it (gear_look.cpp bodyFormOf: mail -> scale, brigandine -> plate)
    if ((B)f != B::Mail && (B)f != B::Brigandine) v.push_back({colTag(G_BODY, bodyArmName(f), kAllT4), G_BODY, ItemKind::Armor, kAllT4, f + 1});
  }
  for (const Mt* m : {&kLeaT2, &kBroT1, &kIroT2, &kSteT3, &kAllT4}) v.push_back({colTag(G_HELM, "own", *m), G_HELM, ItemKind::Helmet, *m, 0});
  for (int f = 0; f < (int)cult::HelmForm::COUNT; f++)
    for (const Mt* m : {&kBroT1, &kIroT2, &kSteT3}) v.push_back({colTag(G_HELM, helmFormName(f), *m), G_HELM, ItemKind::Helmet, *m, f + 1});
  for (const Mt* m : {&kLeaT2, &kBroT1, &kIroT2, &kSteT3, &kAllT4}) v.push_back({colTag(G_SHIELD, "own", *m), G_SHIELD, ItemKind::Shield, *m, 0});
  for (int f = 0; f < (int)cult::ShieldForm::COUNT; f++) {
    if ((cult::ShieldForm)f == cult::ShieldForm::None) continue;
    for (const Mt* m : {&kLeaT2, &kBroT1, &kIroT2, &kSteT3}) v.push_back({colTag(G_SHIELD, shieldFormName(f), *m), G_SHIELD, ItemKind::Shield, *m, f + 1});
  }
  for (const Mt* m : {&kLeaT1, &kLeaT2, &kIroT2, &kSteT3, &kAllT4}) v.push_back({colTag(G_GLOVES, "own", *m), G_GLOVES, ItemKind::Gloves, *m, 0});
  for (const Mt* m : {&kLeaT1, &kLeaT2, &kIroT2, &kSteT3, &kAllT4}) v.push_back({colTag(G_BOOTS, "own", *m), G_BOOTS, ItemKind::Boots, *m, 0});
  for (const Mt* m : {&kClothT1, &kClothT3, &kClothT5}) v.push_back({colTag(G_CLOAK, "own", *m), G_CLOAK, ItemKind::Cloak, *m, 0});
  return v;
}

// the piece as the shops make it (gear::makeGearC, icons_gallery's recipe); false: this maker makes no such piece (culture 0
// has no alloy; an alloy the maker cannot forge)
inline bool makeItem(uint32_t seed, int m, const Col& c, Item& out) {
  const cult::Culture* C = maker(m, seed);
  Rng r(0x1C0u + (uint32_t)m * 131u + (uint32_t)c.mt.ilvl * 7u);
  uint8_t al = 0;
  if (c.mt.mat == Mat::Alloy) {
    if (!C || C->arms.alloys.empty()) return false;
    al = (uint8_t)std::min<int>(c.mt.alloy, (int)C->arms.alloys.size());
  }
  Item it = gear::makeGearC(r, c.kind, 0, c.mt.ilvl, c.mt.rar, C, c.mt.mat, al);
  if (c.mt.mat == Mat::Alloy && it.mat != Mat::Alloy) return false;
  if (c.form) { it.form = (uint8_t)c.form; it.name = gear::gearName(it, C); }
  if (c.kind == ItemKind::Cloak && !it.tint && C) it.tint = (C->heraldry.field ? C->heraldry.field : (C->arms.cloth ? C->arms.cloth : 0)) | 0xFF000000u;
  out = it;
  return true;
}
inline int itemTier(uint32_t seed, int m, const Item& it) { return gear::lookTier(it, maker(m, seed)).tier; }

// ================================================================ colour maths (CIE76, the audit's)
struct Lab { double L, a, b; };
inline double linC(double c) { c /= 255.0; return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
inline Lab toLab(uint32_t v) {   // v: engine rgba (r in the low byte)
  const double r = linC(v & 255), g = linC(v >> 8 & 255), b = linC(v >> 16 & 255);
  double X = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047, Y = 0.2126 * r + 0.7152 * g + 0.0722 * b, Z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883;
  auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116; };
  X = f(X); Y = f(Y); Z = f(Z);
  return Lab{116 * Y - 16, 500 * (X - Y), 200 * (Y - Z)};
}
inline double dE(uint32_t a, uint32_t b) {
  static std::map<uint64_t, double> memo;
  const uint64_t k = (uint64_t)(a & 0xFFFFFF) << 24 | (b & 0xFFFFFF);
  auto it = memo.find(k);
  if (it != memo.end()) return it->second;
  const Lab x = toLab(a), y = toLab(b);
  const double d = std::sqrt((x.L - y.L) * (x.L - y.L) + (x.a - y.a) * (x.a - y.a) + (x.b - y.b) * (x.b - y.b));
  memo[k] = d;
  return d;
}
inline uint32_t hexc(uint32_t v) { return rgba((int)(v >> 16 & 255), (int)(v >> 8 & 255), (int)(v & 255)); }   // 0xRRGGBB -> rgba
inline int luma(uint32_t v) { return (int)std::lround(0.299 * (v & 255) + 0.587 * (v >> 8 & 255) + 0.114 * (v >> 16 & 255)); }
inline uint32_t greyOf(uint32_t v) { const int l = luma(v); return rgba(l, l, l) | (v & 0xFF000000u); }
inline double labHue(const Lab& l) { double h = std::atan2(l.b, l.a) * 180.0 / 3.14159265358979; return h < 0 ? h + 360 : h; }
inline double labChroma(const Lab& l) { return std::sqrt(l.a * l.a + l.b * l.b); }
// a pixel of the people's dye family (F5): within 15 degrees of the row's dye (or dye2) hue, chroma >= 20
inline bool inDyeFamily(uint32_t px, const Dye& d) {
  const Lab p = toLab(px);
  if (labChroma(p) < 20) return false;
  for (uint32_t base : {d.dye, d.dye2}) {
    if (!base) continue;
    const Lab b = toLab(hexc(base));
    if (labChroma(b) < 20) continue;
    double dh = std::fabs(labHue(p) - labHue(b));
    if (dh > 180) dh = 360 - dh;
    if (dh <= 15) return true;
  }
  return false;
}
// HSL mean hue and saturation of a pixel set (the audit's leather measure, FIX_BATCH_P5 3.5)
struct HueSat { double hue = 0, sat = 0; };
template <class It> HueSat meanHueSat(It begin, It end) {
  double s[3] = {0, 0, 0};
  int n = 0;
  for (It i = begin; i != end; ++i) { const uint32_t v = i->second; for (int c = 0; c < 3; c++) s[c] += (v >> (8 * c) & 255); n++; }
  if (!n) return {};
  const double r = s[0] / n / 255, g = s[1] / n / 255, b = s[2] / n / 255, mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn, l = (mx + mn) / 2;
  HueSat o;
  o.sat = d < 1e-6 ? 0 : d / (1 - std::fabs(2 * l - 1));
  if (d > 1e-6) o.hue = mx == r ? 60 * std::fmod((g - b) / d + 6, 6.0) : mx == g ? 60 * ((b - r) / d + 2) : 60 * ((r - g) / d + 4);
  return o;
}

// ================================================================ pair measures
// a piece image: colours at offsets from an origin (the pivot for the doll; 0,0 for icons)
struct Img { std::map<std::pair<int, int>, uint32_t> px; };
struct Cmp { int xorPx = 0, greyPx = 0, distinctPx = 0; int wx = 0, wy = 0; double wd = -1; };   // w*: the pixel they differ most at
inline Cmp compare(const Img& A, const Img& B) {
  Cmp c;
  std::map<uint32_t, int> pa, pb;
  for (auto& [k, v] : A.px) pa[v]++;
  for (auto& [k, v] : B.px) pb[v]++;
  auto worst = [&](std::pair<int, int> k, double d) { if (d > c.wd) { c.wd = d; c.wx = k.first; c.wy = k.second; } };
  for (auto& [k, v] : A.px) {
    auto it = B.px.find(k);
    if (it == B.px.end()) { c.xorPx++; worst(k, 999); continue; }
    if (std::abs(luma(v) - luma(it->second)) >= 12) c.greyPx++;
    if (v != it->second) worst(k, dE(v, it->second));
  }
  for (auto& [k, v] : B.px) if (!A.px.count(k)) { c.xorPx++; worst(k, 999); }
  auto nearest = [&](uint32_t col, const std::map<uint32_t, int>& P) { double m = 1e9; for (auto& [d, n] : P) m = std::min(m, dE(col, d)); return m; };
  for (auto& [col, n] : pa) if (nearest(col, pb) >= 10) c.distinctPx += n;
  for (auto& [col, n] : pb) if (nearest(col, pa) >= 10) c.distinctPx += n;
  return c;
}
enum Verdict { V_DISTINCT = 0, V_COLOUR = 1, V_TWIN = 2 };
inline const char* verdictName(Verdict v) { return v == V_TWIN ? "TWIN" : v == V_COLOUR ? "colour-only" : "distinct"; }
inline int tolOf(int area, int minTol) { return std::max(minTol, area / 40); }
inline Verdict verdictOf(const std::vector<Cmp>& views, int area, int minTol) {
  bool shapeTwin = true, palTwin = true;
  const int tol = tolOf(area, minTol);
  for (const Cmp& c : views) {
    if (c.xorPx > tol || c.greyPx > tol) shapeTwin = false;
    if (c.distinctPx > tol) palTwin = false;
  }
  if (shapeTwin && palTwin) return V_TWIN;
  if (shapeTwin) return V_COLOUR;
  return V_DISTINCT;
}
struct PairNote { int a, b; Verdict v; Cmp worst; int view; };   // worst: the view whose measures are largest
struct ColResult {
  int twin = 0, colour = 0, distinct = 0, pairs = 0;
  std::vector<Verdict> v;           // NM x NM
  std::vector<PairNote> notes;      // every twin and colour-only pair
};
// views[m][i]: maker m's view i (the same views for every maker); have[m]: the maker makes the piece
inline ColResult pairsOf(const std::vector<std::vector<Img>>& views, const std::vector<int>& area, const std::vector<bool>& have, int minTol) {
  ColResult cr;
  cr.v.assign(NM * NM, V_DISTINCT);
  for (int a = 0; a < NM; a++)
    for (int b = a + 1; b < NM; b++) {
      if (!have[(size_t)a] || !have[(size_t)b] || views[(size_t)a].size() != views[(size_t)b].size()) continue;
      std::vector<Cmp> cs;
      for (size_t i = 0; i < views[(size_t)a].size(); i++) cs.push_back(compare(views[(size_t)a][i], views[(size_t)b][i]));
      const Verdict v = verdictOf(cs, std::max(area[(size_t)a], area[(size_t)b]), minTol);
      cr.v[(size_t)(a * NM + b)] = cr.v[(size_t)(b * NM + a)] = v;
      cr.pairs++;
      if (v == V_DISTINCT) { cr.distinct++; continue; }
      (v == V_TWIN ? cr.twin : cr.colour)++;
      PairNote n{a, b, v, cs[0], 0};
      for (size_t i = 1; i < cs.size(); i++)
        if (cs[i].xorPx + cs[i].greyPx + cs[i].distinctPx > n.worst.xorPx + n.worst.greyPx + n.worst.distinctPx) { n.worst = cs[i]; n.view = (int)i; }
      cr.notes.push_back(n);
    }
  return cr;
}
inline Verdict worstOf(const ColResult& cr, int m) {
  Verdict w = V_DISTINCT;
  for (int o = 0; o < NM; o++) if (o != m) w = std::max(w, cr.v[(size_t)(m * NM + o)]);
  return w;
}
inline std::string noteText(const PairNote& n, const char* const* viewNames) {
  char b[200];
  std::snprintf(b, sizeof b, "%s %s=%s (%s: xor %d grey %d distinct %d, most at %d,%d)", verdictName(n.v), makerShort(n.a), makerShort(n.b),
                viewNames ? viewNames[n.view] : "icon", n.worst.xorPx, n.worst.greyPx, n.worst.distinctPx, n.worst.wx, n.worst.wy);
  return b;
}

// ================================================================ the doll
inline art::HumanLook shirtLook(int body) {   // A3's man or woman in their shirts (the hero path)
  Appearance app;
  app.female = body == 1;
  if (body == 1) app.hair = 2;
  return appearanceLook(app, nullptr);
}
inline bool armourSlotCode(uint8_t code) {
  using S = dl::Slot;
  const S s = code >= 2 ? dl::codeSlot(code) : S::Skin;
  return s == S::Metal || s == S::Metal2 || s == S::Leather || s == S::Accent || s == S::Linen || s == S::Cloth3 || s == S::Plume || s == S::Glow || dyeSlot(s);
}
// the piece's own pixels by the probe's owner and region (the audit's isolation)
inline bool inPiece(Grp g, const dl::FrameProbe::It& it, uint8_t region, uint8_t code) {
  using R = dl::Role;
  using RG = dl::Region;
  auto has = [&](const char* s) { return it.part.find(s) != std::string::npos; };
  switch (g) {
    case G_BODY:
      if (it.has(R::Armour) || it.has(R::Pauldron)) return true;
      if (it.has(R::Overlay) && (has("tabard") || has("sash"))) return true;
      if (it.has(R::Neck) && has("cowl")) return true;
      return it.has(R::Arms) && (region == (uint8_t)RG::Upper || region == (uint8_t)RG::Fore || region == (uint8_t)RG::Cuff) && armourSlotCode(code);
    case G_HELM: return it.has(R::Helm) || (it.has(R::Headwear) && has("helm"));
    case G_SHIELD: return it.has(R::Shield);
    case G_GLOVES: return it.has(R::Arms) && (region == (uint8_t)RG::Hand || region == (uint8_t)RG::Flare);
    case G_BOOTS:
      return it.has(R::Legs) && (region == (uint8_t)RG::Boot || region == (uint8_t)RG::Sole ||
                                 ((region == (uint8_t)RG::Shin || region == (uint8_t)RG::Knee) && armourSlotCode(code)));
    case G_CLOAK: return it.has(R::Cloak);
    default: return false;
  }
}
constexpr int kWhole = -1, kAllArmour = -2;   // figure(): the whole figure; every armour piece it wears
inline bool inArmour(int grp, const dl::FrameProbe::It& it, uint8_t region, uint8_t code) {
  if (grp != kAllArmour) return inPiece((Grp)grp, it, region, code);
  for (int g = 0; g < G_COUNT; g++) if (inPiece((Grp)g, it, region, code)) return true;
  return false;
}
struct Fig {
  Canvas full;
  int px = 0, py = 0;
  Img piece;            // the piece's pixels (offsets from the pivot); kWhole: the whole figure, kAllArmour: its armour
  int area = 0, dyed = 0;
  int dyeFamily = 0;    // dye-slot pixels of the piece within the people's dye family (F5)
};
inline uint32_t codeColour(const dl::DollRecipe& r, uint8_t c) { return r.ramps.c[(int)dl::codeSlot(c)][dl::codeStep(c)] | 0xFF000000u; }
inline Fig figure(const art::HumanLook& L, int facing, int grp, const Dye* dye = nullptr, art::Clip clip = art::Clip::Idle, int frame = 0) {
  Fig f;
  const dl::DollRecipe r = dl::resolve(L);
  const dl::FrameProbe p = dl::probeFrame(r, art::RigPart::Loco, facing, clip, 0, frame, true);
  if (!p.ok) return f;
  f.full = Canvas(p.w, p.h);
  f.px = p.px; f.py = p.py;
  for (int y = 0; y < p.h; y++)
    for (int x = 0; x < p.w; x++) {
      const int k = y * p.w + x;
      const uint8_t c = p.code[(size_t)k];
      if (c < 2) continue;
      const uint32_t col = codeColour(r, c);
      f.full.set(x, y, col);
      const int o = p.owner[(size_t)k];
      const bool mine = grp == kWhole || (o >= 0 && o < (int)p.items.size() && inArmour(grp, p.items[(size_t)o], p.region[(size_t)k], c));
      if (!mine) continue;
      f.piece.px[{x - p.px, y - p.py}] = col;
      if (dyeCode(c)) { f.dyed++; if (dye && inDyeFamily(col, *dye)) f.dyeFamily++; }
    }
  f.area = (int)f.piece.px.size();
  return f;
}
inline void wearOne(art::HumanLook& L, const Item& it, uint32_t seed) {
  WornGear w;
  switch (it.kind) {
    case ItemKind::Armor: w.armor = &it; break;
    case ItemKind::Helmet: w.helmet = &it; break;
    case ItemKind::Shield: w.shield = &it; break;
    case ItemKind::Gloves: w.gloves = &it; break;
    case ItemKind::Boots: w.boots = &it; break;
    case ItemKind::Cloak: w.cloak = &it; break;
    default: break;
  }
  wearGear(L, w, [seed](uint64_t id) -> const cult::Culture* {
    for (int m = 1; m < NM; m++) if (maker(m, seed)->id == id) return maker(m, seed);
    return nullptr;
  });
}
constexpr int kWornViews = 4;   // man D, man R, woman D, woman R
inline const char* const* wornViewNames() { static const char* const k[kWornViews] = {"man D", "man R", "woman D", "woman R"}; return k; }

// one column measured: every maker's worn views and icon (on: the game's look; FIX_FP off for the "before" icon share)
struct Cell {
  bool have = false;
  Item item;
  int tier = 0;
  art::HumanLook look[2];          // man, woman
  Fig fig[kWornViews];
  int area = 0, maxShare = 0;      // the piece's largest view; its dye share (% of the piece, the largest of the views)
  Canvas icon, iconBefore;
  int iconArea = 0, iconChanged = 0, iconFamily = 0;   // opaque px; px FIX_FP changed; changed px in the dye family
};
struct ColumnRun {
  Col col;
  int tier = 0;                    // the column's look tier (maker 1's piece)
  Cell cell[NM];
  ColResult worn, icons;
};
inline ColumnRun runColumn(uint32_t seed, const Col& c, bool fpOn = true) {
  ColumnRun cr;
  cr.col = c;
  FpScope scope(fpOn);
  std::vector<std::vector<Img>> wv(NM), iv(NM);
  std::vector<int> wa(NM, 0), ia(NM, 0);
  std::vector<bool> have(NM, false);
  for (int m = 0; m < NM; m++) {
    Cell& e = cr.cell[m];
    if (!makeItem(seed, m, c, e.item)) continue;
    e.have = have[(size_t)m] = true;
    e.tier = itemTier(seed, m, e.item);
    if (m == 1 || !cr.tier) cr.tier = e.tier;
    const Dye d = dyeOf(m);
    for (int body = 0; body < 2; body++) {
      e.look[body] = shirtLook(body);
      wearOne(e.look[body], e.item, seed);
      for (int fi = 0; fi < 2; fi++) {
        Fig& f = e.fig[body * 2 + fi];
        f = figure(e.look[body], fi == 0 ? 0 : 2, c.g, &d);
        e.area = std::max(e.area, f.area);
        if (f.area) e.maxShare = std::max(e.maxShare, 100 * f.dyed / f.area);
        wv[(size_t)m].push_back(f.piece);
      }
    }
    wa[(size_t)m] = e.area;
    const art::IconLook il = gearIcon(e.item, maker(m, seed));
    e.icon = art::itemIconLook(il);
    { FpScope off(false); e.iconBefore = art::itemIconLook(il); }
    Img im;
    for (int y = 0; y < e.icon.h; y++)
      for (int x = 0; x < e.icon.w; x++) {
        const uint32_t v = e.icon.get(x, y);
        if (!(v >> 24)) continue;
        im.px[{x, y}] = v;
        if (v != e.iconBefore.get(x, y)) { e.iconChanged++; if (inDyeFamily(v, d)) e.iconFamily++; }
      }
    e.iconArea = ia[(size_t)m] = (int)im.px.size();
    iv[(size_t)m].push_back(im);
  }
  cr.worn = pairsOf(wv, wa, have, 2);
  cr.icons = pairsOf(iv, ia, have, 3);
  return cr;
}

// ================================================================ a piece cropped from its wearer (the swatches)
// the man wearing the piece (facing D; grp: the piece's own pixels, kWhole the figure) cropped to its box; empty: the maker
// makes no such piece
inline Canvas pieceCrop(uint32_t seed, int m, const Col& c, int grp) {
  Item it;
  if (!makeItem(seed, m, c, it)) return Canvas();
  art::HumanLook L = shirtLook(0);
  wearOne(L, it, seed);
  const Fig f = figure(L, 0, grp);
  int x0 = 1 << 20, y0 = 1 << 20, x1 = -(1 << 20), y1 = -(1 << 20);
  for (const auto& [k, v] : f.piece.px) { x0 = std::min(x0, k.first); y0 = std::min(y0, k.second); x1 = std::max(x1, k.first); y1 = std::max(y1, k.second); }
  if (x1 < x0) return Canvas();
  Canvas o(x1 - x0 + 1, y1 - y0 + 1);
  for (const auto& [k, v] : f.piece.px) o.set(k.first - x0, k.second - y0, v);
  return o;
}

// ================================================================ looks files (tools/doll/fp_looks.txt)
// `tag who people spec`: who man|woman; people the expected maker (a people name, or - for maker-less looks); spec a looks
// spec whose `@` is replaced by the seed
struct FpLook { std::string tag, who, people, spec; };
inline std::vector<FpLook> readFpLooks(const std::string& path) {
  std::vector<FpLook> v;
  FILE* f = std::fopen(path.c_str(), "r");
  if (!f) return v;
  char line[1024];
  while (std::fgets(line, sizeof line, f)) {
    std::string s(line);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    if (s.empty() || s[0] == '#') continue;
    char a[128], b[32], c[64], d[768];
    if (std::sscanf(s.c_str(), "%127s %31s %63s %767s", a, b, c, d) == 4) v.push_back({a, b, c, d});
  }
  std::fclose(f);
  return v;
}
inline int peopleIndex(const std::string& name) {
  for (int m = 1; m < NM; m++) if (name == makerName(m)) return m;
  return 0;
}
inline art::HumanLook lookOf(const FpLook& l, uint32_t seed, std::string* bad = nullptr) {
  gear::installLadderMacros();
  std::string spec = l.spec;
  for (size_t i; (i = spec.find('@')) != std::string::npos;) spec.replace(i, 1, std::to_string(seed));
  art::HumanLook L;
  L.body = (uint8_t)(l.who == "woman" ? 1 : 0);
  std::string b;
  gear::applyLadderSpec(L, spec, nullptr, nullptr, &b);
  if (bad) *bad = b;
  return L;
}
// the armour slots a look wears (F8): body, helm, shield, gloves, boots, cloak with a tier
inline std::vector<int> armourSlotsWorn(const art::HumanLook& L) {
  using H = art::HumanLook;
  std::vector<int> v;
  for (int s : {(int)H::TIER_BODY, (int)H::TIER_HELM, (int)H::TIER_SHIELD, (int)H::TIER_GLOVES, (int)H::TIER_BOOTS, (int)H::TIER_CLOAK})
    if (L.tier[s] > 0) v.push_back(s);
  return v;
}
inline const char* tierSlotName(int s) {
  static const char* const k[] = {"body", "helm", "shield", "weapon", "gloves", "boots", "cloak", "back"};
  return s >= 0 && s < 8 ? k[s] : "?";
}

}  // namespace fpa
