// rpg/core/fingerprint.h (P5): the peoples' fingerprints on armour, one row per maker people (docs/FIX_BATCH_P5.md 3,
// STYLE_P2 30). Data only (MARKS tunes the values within FIX_BATCH_P5 3.5); the doll's resolver and the icon painter read
// it, keyed by a piece's maker people (cult::Archetype + 1; 0 none), so the worn piece and its icon agree by construction.
// Core: every layer may include it; it includes only <cstdint>.
//
// The dye is the thread and binding colour on a piece's rims, hems, laces and seams; dye2 the second colour of a two-tone
// band (0: a solid band). The band runs along canvas x + y: `run` px of the dye, then dye2 up to `period`. The motif keys
// the mark tile (`finish.mark`, T1-T4: stitched, then engraved) and the filigree (`finish.filigree`, T5-T6: gold).
// Separation (checked by `python tools/doll/p5_marks.py --table` and rpg_test --fingerprint F1): primaries >= dE 25 apart,
// two-tone signatures >= 25, every primary >= 20 from each step 1-3 of the leather, bronze, iron, steel, gilded and brass
// ramps, every dye2 >= 20 from its own dye.
#pragma once
#include <cstdint>

namespace fp {
constexpr int kPeoples = 13;   // 0 none (culture 0: no fingerprint), 1..12 cult::Archetype + 1

// (P5b, docs/STYLE_P5B.md 3-5 and 9) a people's SIGNATURE on its armour: read only under FIX_SIG (doll_envelope.h; with
// FIX_FP), by the doll's resolver (doll_resolve.cpp sigLook) and the icon painter (art_items.cpp), keyed by each worn piece's
// own maker (the helm's people picks the head, the body's the axes, the shield's the form, the cloak's the colour). The
// words are the doll's own (doll_forge.h shapeList and valueName, the role vocabulary of doll.h): "" keeps what the piece
// draws today. A people with no signature (sig nullptr) draws P5's look, pixel for pixel. The style slice (STYLE_P5B 9)
// fills Jade, Imperial and Fjordfolk; the other nine follow at production.
struct SigTier {
  const char* head;      // the head the helm piece draws: a helm role value ("fam.kabuto" "form9" "form4" ...) or
                         // "hw:<headwear>" (a soft head: the guise path, its cloth in the people's pair); "" the piece's
                         // own head and crest (the people shows by its crest colour and body: a tier the slice leaves open)
  uint8_t variant;       // the helm family's variant (doll_forge.h shapeVariant; 0 the base drawing)
  const char* crest;     // the helm's crest (valueName AX_CREST: "horns" "comb" "knife" ...); "" none
  const char* skirt;     // shapeList skirt: none tassets strips hauberk split bell
  const char* mass;      // shapeList mass: none round broad tall drop mantle
  const char* shoulder;  // the pauldron drawing's family (`role pauldron <family>.<size>`): layered slab fur flap ...; "" none
  const char* layer;     // shapeList layer: none tabard sash halfcape cape scarf; "" the ladder's own (the T3 people's cape)
  const char* arm;       // shapeList arm: close bare wide heavy
  bool ownGrowth = false;   // (fix r1) the people's head grows itself at this tier (its own ADDED views: the Fjordfolk jarl's
                            // gilt horns), so the family's legend growth part (`<family>_legend`: the horned helm's T5 comb)
                            // steps aside
  bool giltGrowth = false;  // (fix r1) the family's legend growth at this tier in the gilt (the Jade T5 kuwagata: the legend
                            // part's metal steps 1-4 drawn in Device, the T5 device's gold; no pool bytes)
  bool coif = false;        // (P5b area C) the people's hood worn under the helm at this tier (headwear/cloth/hood in Sig::hood:
                            // the Starspire knight keeps the acolyte's indigo hood under his crown; no pool bytes)
};
struct Sig {
  uint32_t dom, acc;     // the dominant and the accent (0xRRGGBB): the cloth (heads, the layer, cape and cloak, linings,
                         // crests, lacing, the shield's field); never a lit metal or leather plane (STYLE_P5B 2.3)
  const char* shield;    // the people's shield form (cult::ShieldForm by name): round kite heater tower crescent oval buckler
  uint32_t crest;        // the crest's and the plume's colour (0: the dominant)
  bool laces;            // the people laces its plates (Jade): the body's dye seams in full, the dominant alone
  uint32_t trim;         // a T1 metal head's fittings metal (Metal 2 by the look's trim: Imperial's legion brass); 0 the look's
  SigTier t[7];          // by the piece's tier, [1]..[6]; [0] unused
  bool gilt = false;     // (fix r1) the legend's gilt: a signed T5 helm's and shield's fittings (their Metal 2: the brow band,
                         // the cheek guards, the horns, the rim; drawn in Device, the T5 device's gold) over a culture alloy's own
                         // polished trim, never the body's Metal 2 (an alloy's banded rows): Imperial crimson and gold, the
                         // Fjordfolk jarl's gilt horns and rim; false keeps the alloy's
  uint32_t hood = 0;     // (P5b area C) a soft head's cloth (the Head slot of an `hw:` head and a T1 cap; 0xRRGGBB): 0 the accent.
                         // Starspire's hood and cowl and Sun Temple's band and cap are their dominant (the crest's colour
                         // the other of the pair: the star-white edge, the yellow feathers)
  bool cloth = true;     // (P5b area A) the pair on the people's cloth: the shield's field and device, the cloak, the dye band (the
                         // pair's hue held); false keeps P5's (the Heartland: its hero's road, D54-D55, stays as the owner knows it)
  const char* shieldAlt = nullptr;   // (fix r1) the people's second shield form (STYLE_P5B 5): a piece made in it keeps it (the
                                     // Dune's crescent beside the dhal); nullptr: every piece the people's `shield`
};
// (fix r1) the form a signed shield piece draws (ShieldForm + 1: round 1 kite 2 heater 3 tower 4 crescent 5 oval 6 buckler 7
// leaf 8), from the piece's own `own`: the people's form, or the piece's own where it is the people's second (Sig::shieldAlt)
inline int sigShieldForm(const Sig* s, int own) {
  static const char* const kForm[8] = {"round", "kite", "heater", "tower", "crescent", "oval", "buckler", "leaf"};
  auto same = [](const char* a, const char* b) { while (*a && *a == *b) { a++; b++; } return *a == *b; };
  auto idx = [&](const char* w) { for (int i = 0; i < 8; i++) if (w && same(w, kForm[i])) return i + 1; return 0; };
  if (!s) return own;
  if (s->shieldAlt && own == idx(s->shieldAlt)) return own;
  const int f = idx(s->shield);
  return f ? f : own;
}
// clang-format off
// Jade (flared, banded): the jingasa, then the kabuto, then gilt kuwagata; sode, the kusazuri's bell, wide sleeves, the
// vermilion obi; lacquer black and vermilion (STYLE_P5B 5 Jade; FLUX jade_full_*, jade_pixel_1)
inline constexpr Sig kSigJade{0xE8641C, 0x1C1A1E, "oval", 0, true, 0, {
    {},
    {"hw:conical", 0, "",      "",        "none",   "",        "sash",     "close"},
    {"hw:conical", 0, "",      "",        "drop",   "layered", "sash",     "wide"},   // (fix r1) the great kasa (p5b_sig_fr1.py: the jingasa's part)
    {"fam.kabuto", 0, "",      "bell",    "drop",   "layered", "sash",     "wide"},
    {"fam.kabuto", 0, "fin",   "bell",    "drop",   "layered", "sash",     "wide"},
    {"fam.kabuto", 1, "horns", "bell",    "drop",   "layered", "tabard",   "wide", false, true},
    {"fam.kabuto", 0, "crown", "bell",    "drop",   "layered", "tabard",   "wide"}}};
// Imperial (rectangular, banded, the crest's arc): the bronze knob cap, then the crested helm with a transverse crimson crest,
// the Attic crest at the top; banded shoulders, pteruges, the focale, the paludamentum, the scutum; crimson and gold
inline constexpr Sig kSigImperial{0x7C082A, 0xE2B840, "tower", 0xB4243C, false, 0xCB9034, {
    {},
    {"form1",      0, "",      "",        "none",   "",        "scarf",    "close"},
    {"form1",      0, "",      "",        "broad",  "layered", "scarf",    "close"},   // (P5b A) the Montefortino, its tuft (STYLE_P5B 4)
    {"form9",      0, "",      "strips",  "broad",  "layered", "halfcape", "close"},
    {"fam.plumed", 1, "",      "strips",  "broad",  "layered", "halfcape", "close"},
    {"fam.crested",0, "",      "strips",  "broad",  "layered", "cape",     "close"},
    {"fam.crested",0, "fin",   "strips",  "broad",  "layered", "cape",     "close"}}, true};
// Fjordfolk (round, heavy-shouldered): the leather cap with a fur brow, the spangenhelm with nasal and aventail, gilt horns
// at the saga tier; the fur mantle, the split mail skirt, bare forearms, the woad cape; the biggest round shield
inline constexpr Sig kSigFjordfolk{0x4878C8, 0xE8E2CE, "round", 0x8C9098, false, 0, {
    {},
    {"form4",      0, "",      "",        "none",   "",        "none",     "bare"},
    {"form4",      0, "",      "",        "mantle", "roll",    "none",     "bare"},
    {"form4",      0, "",      "hauberk", "mantle", "roll",    "",         "bare"},
    {"form4",      0, "",      "hauberk", "mantle", "pelt",    "cape",     "bare"},   // (P5b A) the spectacle helm (p5b_sig_A.py)
    {"fam.horned", 0, "",      "hauberk", "mantle", "pelt",    "cape",     "bare", true},
    {"fam.horned", 0, "",      "hauberk", "mantle", "pelt",    "cape",     "bare"}}, true};
// (P5b area A) Highland (asymmetric, long quilted lines, a small round targe): the bonnet and its toorie, the plaid hood, the
// kettle under its fur band and feather; the plaid over the left shoulder from T1, a plaid half cape from T4, the kilted
// strips, bare arms, broad shoulders from T4; tartan green and saffron (STYLE_P5B 5 Highland; FLUX highland_full_*)
inline constexpr Sig kSigHighland{0x185030, 0xD89A28, "buckler", 0, false, 0, {
    {},
    {"hw:cap",     0, "",      "",        "none",   "",        "sash",     "bare"},
    {"hw:cap",     0, "",      "",        "drop",   "flap",    "sash",     "bare"},   // the bonnet and its badge (the hood: over T2's head budget)
    {"form2",      0, "",      "strips",  "none",   "",        "sash",     "bare"},
    {"form2",      0, "",      "strips",  "broad",  "fur",     "sash",     "bare"},   // (the fur trim: the T4 figure under the waiting T5's area)
    {},   // T5-T6 (null): P5's look until the owner's budget ruling (STYLE_P5B 6, decision 5)
    {}}};
// (P5b area A) Heartland (the hero's road; square, heraldic): unchanged from T2 to T6 (D54, D55, D83: soft cap, nasal with
// knife comb, open crested, gilt onion, crystal), its body the hero's shapes at every tier (skirt null: unsigned); P5b adds a
// Heartland-made T1 helm drawn as the chapel (the levy's brimmed kettle). The people's pair stays off its cloaks, shield fields
// and dye band (cloth false): the hero's road wears them (the menus doll T1-T6, the opening), so decision 3 there is the
// owner's to make (STYLE_P5B 5 Heartland; 11: never touch the hero's road beyond its T1 chapel)
inline constexpr Sig kSigHeartland{0xC02030, 0xEAE6D8, nullptr, 0, false, 0, {
    {},
    {"form2",      0, "",      nullptr,   nullptr,  nullptr,   nullptr,    nullptr},   // the chapel (p5b_sig_A.py)
    {}, {}, {}, {}, {}}, false, 0, false};   // (integration) gilt, hood, cloth
// (P5b production, area B: Dune, Steppe, Marsh; their heads ADDED by tools/doll/p5b_sig_B.py, their icons rpg/art/sig_icons_B.inc;
// T1-T4 drawn, T5-T6 the people's axes on their own heads until the budget ruling. T1 keeps its skirt unset: a split or strip
// skirt stands outside T1's bare outline, C12f)
// Dune (vertical spike, flowing kaftan, a small round shield): the low turban with its teal wraps and tail, the turban over the
// spiked cap, the needle-spiked kulah with its mail aventail and a teal turban band; lobed shoulders, the split kaftan from T2,
// wide sleeves, the sash, the scarf, the half cape at T3 (the T3 cape stood past the legs behind the split)
inline constexpr Sig kSigDune{0x18849E, 0xECE2C8, "round", 0, false, 0, {   // (fix r1) the crescent its second form
    {},
    {"hw:turban",  0, "",      "",        "none",   "",        "sash",     "close"},
    {"hw:turban",  0, "",      "hauberk", "round",  "lobed",   "scarf",    "wide"},
    {"fam.veil",   0, "",      "bell",    "tall",   "",        "scarf",    "wide"},
    {"fam.veil",   2, "spike", "split",   "tall",   "",        "halfcape", "wide"},
    {"fam.onion",  0, "spike", "bell",    "tall",   "",        "halfcape", "bare"},
    {"fam.onion",  0, "spike", "bell",    "tall",   "",        "halfcape", "bare"}}, false, 0, true, "crescent"};
// Steppe (a tall point, horizontal lamellar rows, a fur roll at the neck): the fur-flapped felt hat, the peaked felt cap and its
// rust knot, the pointed helm under the horsetail on lamellar cheek and neck guards; the fur mantle at T2, lamellar flaps and
// the strip skirt at T3, the split riding coat at T4, the rust scarf from T3; laces: the coat's seams in full rust
// (fix r1) the rust reddened within its family to madder rust #9C3400 (STYLE_P5B 3: "MARKS reddens it first if the real ramps
// disagree"): #9C4426 sat dE 5.8 from leather 5's step 2 and read as leather (F1 wants 20); #9C3400 is 20.3 from every material
// step and 25.3 from the Heartland red, its luma 77 against the sky's 143
inline constexpr Sig kSigSteppe{0x9C3400, 0x3CACD6, "round", 0, true, 0, {
    {},
    {"hw:furhat",  0, "",      "",        "none",   "",        "none",     "close"},
    {"hw:furhat",  0, "",      "strips",  "mantle", "fur",     "scarf",    "close"},
    {"fam.plumed", 0, "",      "strips",  "drop",   "flap",    "scarf",    "close"},
    {"fam.plumed", 2, "",      "split",   "drop",   "scale",   "halfcape", "wide"},
    {"fam.plumed", 4, "finial","split",   "drop",   "layered", "halfcape", "heavy"},
    {"fam.plumed", 4, "finial","bell",    "mantle", "pelt",    "halfcape", "heavy"}}};
// Marsh (ragged, drooping, a long oval shield): the bog cap fringed in moss (the hood measured 40 % people colour, S2), the
// rawhide kettle with a bone crest and two feathers, the tasselled reed war hat, the wide war hat; hide flaps, the fringed
// strip skirt from T2, bare arms, the reed half cape from T3
inline constexpr Sig kSigMarsh{0x707E28, 0xD6CCA8, "oval", 0, false, 0, {
    {},
    {"hw:headscarf", 0, "",    "",        "none",   "",        "none",     "bare"},
    {"form1",      0, "",      "strips",  "none",   "",        "none",     "bare"},
    {"form2",      0, "",      "strips",  "broad",  "",        "scarf",    "wide"},
    {"fam.warhat", 0, "",      "strips",  "drop",   "flap",    "halfcape", "bare"},
    {"fam.warhat", 0, "",      "strips",  "drop",   "flap",    "halfcape", "bare"},
    {"fam.warhat", 0, "",      "strips",  "drop",   "flap",    "halfcape", "bare"}}};
// ---- (P5b area C, STYLE_P5B 4-5) River, Sun Temple, Sylvan and Starspire at T1-T4; T5-T6 wear their families' existing legend
// heads (no pool bytes; the T5-T6 legend overlays wait for the owner's budget decision, STYLE_P5B 6 option B). The people heads'
// added views: tools/doll/p5b_sig_C.py
// River (the guild river-city, decision 1; rectangular, heavy-sleeved): the chaperon on its roll, the leather chapel, then the
// morion growing (one family from T3, the owner's default; the sallet the alloys' secondary) under a lapis plume at T4; slab
// shoulders, tassets, the livery tabard, puffed sleeves, the heater; deep lapis and linen white (STYLE_P5B 3's white and lapis
// livery with the lapis dominant, tuned within its family to #12335E: F1's rules hold, a near-white dominant never clears the
// light steel and iron steps by 20, and the plan's lapis sat 18 from the Fjordfolk woad; the roll and the plume in it, the
// chaperon's cloth the linen)
inline constexpr Sig kSigRiver{0x12335E, 0xE8E4D6, "heater", 0, false, 0, {
    {},
    {"hw:headscarf", 0, "",      "",        "none",   "",        "none",     "close"},
    {"fam.kettle", 0, "",      "tassets", "broad",  "slab",    "tabard",   "heavy"},
    {"fam.morion", 0, "",      "tassets", "broad",  "slab",    "tabard",   "heavy"},
    {"fam.morion", 0, "",      "tassets", "broad",  "plate",   "tabard",   "wide", false, false, true},
    {"fam.morion", 0, "",      "tassets", "broad",  "slab",    "tabard",   "heavy"},
    {"fam.morion", 1, "",      "tassets", "broad",  "slab",    "tabard",   "heavy"}}, false, 0xE8E4D6};
// Sun Temple (radiating fans, round chimalli, animal heads): the turquoise headband and its yellow feather, the quilted cap
// and its fan, the jaguar's head (the masked family's place), the jaguar under a feather crest; the feather mantle, the
// feathered bell, bare arms, the feather cape; turquoise and feather yellow (the feathers and the pelt in the yellow)
inline constexpr Sig kSigSunTemple{0x1CA086, 0xF0C030, "round", 0xF0C030, false, 0, {
    {},
    {"hw:circlet", 0, "",      "",        "none",   "",        "none",     "bare"},
    {"hw:cap",     0, "",      "",        "mantle", "feather", "none",     "bare"},
    {"fam.mask",   0, "",      "bell",    "mantle", "feather", "cape",     "bare"},
    {"fam.mask",   0, "",      "bell",    "mantle", "feather", "cape",     "bare"},
    {"fam.mask",   0, "",      "bell",    "mantle", "feather", "cape",     "bare", false, false, true},
    {"fam.crown",  1, "",      "bell",    "tall",   "winged",  "cape",     "bare", false, false, true}}, false, 0x1CA086};
// Sylvan (curved, swept back, slim: the lightest silhouette at every tier): the leaf circlet, the leaf crown, the winged helm
// whose wings grow (low and swept back at T3, the leaf fin at T4); leaf shoulders, the long split leaf skirt, close sleeves,
// the leaf cloak; leaf green and forest
inline constexpr Sig kSigSylvan{0xA0C850, 0x2E5A2A, "leaf", 0, false, 0, {
    {},
    {"hw:circlet", 0, "",      "",        "none",   "",        "none",     "close"},
    {"hw:circlet", 0, "",      "split",   "drop",   "leaf",    "none",     "close"},
    {"fam.winged", 0, "",      "strips",  "drop",   "leaf",    "",         "close"},
    {"fam.winged", 3, "",      "split",   "drop",   "leaf",    "cape",     "close"},
    {"fam.winged", 0, "",      "split",   "drop",   "leaf",    "cape",     "close"},
    {"fam.winged", 2, "",      "strips",  "drop",   "leaf",    "",         "close"}}};
// Starspire (sharp, vertical, symmetric; gothic-arched, fluted and celestial, never spiky-evil): the deep pointed hood, the
// stiff cowl, then the star-pointed crown worn over the same indigo hood (SigTier::coif) and growing; tall flared shoulders,
// the split hem then tassets, the indigo cape, the kite; indigo and star white (the hood's star-white edge)
inline constexpr Sig kSigStarspire{0x5A3CB4, 0xDCE4F8, "kite", 0xDCE4F8, false, 0, {
    {},
    {"hw:hood",    0, "",      "",        "none",   "",        "none",     "close"},
    {"hw:hood",    0, "",      "tassets", "tall",   "flared",  "none",     "close"},
    {"fam.crown",  1, "",      "tassets", "tall",   "flared",  "cape",     "close", false, false, true},
    {"fam.crown",  0, "",      "tassets", "tall",   "flared",  "cape",     "close", false, false, true},
    {"fam.crown",  3, "",      "bell",    "tall",   "crystal", "",         "close", false, false, true},
    {"fam.crown",  2, "",      "bell",    "tall",   "crystal", "",         "close", false, false, true}}, false, 0x5A3CB4};
// ---- (end of area C)
// clang-format on

struct Row {
  const char* name;     // "fjordfolk" ... (the doll's people-<name>); "none"
  const char* motif;    // the mark's and the filigree's key prefix: "knot" "plaid" "oak" "laurel" "arab" "horse" ...
  uint32_t dye, dye2;   // 0xRRGGBB; dye2 0: a solid band
  uint8_t run, period;  // the band along canvas x + y: run px of dye, then dye2 up to period
  const Sig* sig = nullptr;   // (P5b) the people's signature (FIX_SIG); nullptr: P5's look
};
// clang-format off
inline constexpr Row kRows[kPeoples] = {
    {"none",      "oak",    0x000000, 0x000000, 0, 0},   // culture 0: the classic pieces, no fingerprint (oak: the classic filigree)
    {"fjordfolk", "knot",   0x3C6EBE, 0xE8E2CE, 3, 4, &kSigFjordfolk},   // woad blue, bleached wool; knotwork
    {"highland",  "plaid",  0x1E683A, 0xAC2430, 2, 4, &kSigHighland},   // tartan green, red (a check); the tartan sett (new, D74's hem-plaid sett)
    {"heartland", "oak",    0xC42828, 0xEAE6D8, 4, 5, &kSigHeartland},   // heraldic red, white; oak leaves
    {"imperial",  "laurel", 0x641E6E, 0xE2B840, 2, 3, &kSigImperial},   // purple, gold thread; laurel
    {"dune",      "arab",   0x243484, 0xECA61E, 2, 4, &kSigDune},   // indigo, saffron; arabesque
    {"steppe",    "horse",  0x3CACD6, 0xC43028, 2, 3, &kSigSteppe},   // sky blue, horse-hair red; running horses
    {"marsh",     "reed",   0x687E24, 0x465078, 4, 5, &kSigMarsh},   // moss, faded indigo; reeds
    {"jade",      "clouds", 0xF06E1E, 0x1C1A1E, 2, 3, &kSigJade},   // vermilion, black lacquer; cloud scrolls
    {"river",     "wave",   0x12868C, 0xECEAE2, 2, 4, &kSigRiver},   // river teal, white; waves
    {"suntemple", "ray",    0xB41E64, 0x249688, 2, 4, &kSigSunTemple},   // magenta, teal; sun rays
    {"sylvan",    "vein",   0xA0C850, 0xE8B040, 4, 5, &kSigSylvan},   // leaf green, amber; leaf veins
    {"starspire", "star",   0x9678F0, 0xDCE4F8, 3, 4, &kSigStarspire},   // violet, star white; four-point stars
};
// clang-format on
inline const Row& row(int people) { return kRows[people > 0 && people < kPeoples ? people : 0]; }
// (P5b) the people's signature row for a piece of tier t (1..6), or nullptr (no signature, or no tier)
inline const Sig* sig(int people) { return row(people).sig; }
inline const SigTier* sigTier(int people, int tier) {
  const Sig* s = sig(people);
  return s && tier >= 1 && tier <= 6 ? &s->t[tier] : nullptr;
}
}  // namespace fp
