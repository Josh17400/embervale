// Townsfolk, guards, bandits and wayside people: how they look and what they are called (moved out of game.cpp in M3
// phase A so the PEOPLE lane owns it). makeLook's random draws (rr) are a contract: quests.cpp reproduces the first
// one (personFemale) to know a quest person's sex before the actor exists, so keep the draw order of the shared part;
// culture work (M3) draws from its own hash streams, never from rr (it only reads rr's state on entry, which every
// caller already makes a pure function of the person: site, building and slot, or a quest's person seed).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/game_internal.h"

// ------------------------------------------------------------------ the census look (M3, VISION_PLAN 5.6 "NPCs")
// Dresses a person in a culture: people from its mix, skin and hair from its ranges, the dress cut per sex, headwear,
// the cloth palette and pattern, jewellery and face paint, a name in its phonology. Guards wear their OWNER kingdom's
// arms (so occupation shows) in the kingdom's colours; bandits a poor local version; priests the faith's colours; the
// king and the jarls the culture's regalia. Pure function of its arguments (the galleries and tests call it too).
namespace census {

struct Rank { bool royal = false, capital = false; };

namespace {
struct Stream {   // a private hash stream (never the caller's Rng)
  uint64_t s;
  explicit Stream(uint64_t seed) : s(seed ^ 0xC3A5C85C97CB3127ull) {}
  uint32_t next() {
    s += 0x9E3779B97F4A7C15ull;
    uint64_t z = s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return (uint32_t)((z ^ (z >> 31)) >> 32);
  }
  int pick(int n) { return n <= 1 ? 0 : (int)(next() % (uint32_t)n); }
  bool chance(int p256) { return (int)(next() & 255u) < p256; }
};
uint32_t blend(uint32_t a, uint32_t b, float t) {   // (M4) rgb lerp, opaque
  auto ch = [&](int sh) { return (int)(((a >> sh) & 255) * (1 - t) + ((b >> sh) & 255) * t); };
  return rgba(ch(0), ch(8), ch(16));
}
uint32_t darker(uint32_t c, float k) {
  return rgba((int)((c & 255) * k), (int)(((c >> 8) & 255) * k), (int)(((c >> 16) & 255) * k));
}
// the material band (art::HumanLook: 2 iron, 3 steel, 4 gilded/bronze, 5 jade, 6 obsidian) a culture's metal reads as
uint8_t bandOf(uint32_t metal, int rank) {
  const int r = metal & 255, g = (metal >> 8) & 255, b = (metal >> 16) & 255;
  const bool warm = metal && r > b + 46 && g > b + 14;          // bronze, brass, gold
  const bool green = metal && g > r + 18 && g > b + 6;          // jade-lacquered
  const bool dark = metal && r + g + b < 200;                   // blackened
  if (rank >= 2) return warm ? 4 : (green ? 5 : (dark ? 6 : 3));
  if (rank == 1) return warm ? 4 : (green ? 5 : 3);
  return warm && r > 170 ? 4 : 2;
}
art::Hair hairFor(Stream& h, uint16_t bits, bool female, art::Hair fallback) {
  static const art::Hair fem[] = {art::Hair::Long, art::Hair::Braids, art::Hair::Ponytail, art::Hair::Bun, art::Hair::Curls};
  static const art::Hair male[] = {art::Hair::Short, art::Hair::Bald, art::Hair::Long, art::Hair::Mohawk, art::Hair::Curls,
                                   art::Hair::Ponytail, art::Hair::Braids};
  art::Hair opts[8];
  int n = 0;
  if (female) { for (art::Hair x : fem) if (bits & (1u << (int)x)) opts[n++] = x; }
  else for (art::Hair x : male) if (bits & (1u << (int)x)) opts[n++] = x;
  if (!n) return fallback;
  // the first options are the more common
  const int i = std::min(h.pick(n), h.pick(n));
  return opts[i];
}
}  // namespace

void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner,
           uint64_t seed, const Rank& rank) {
  Stream h(seed);
  const cult::DressStyle& D = C.dress;
  // ---- the people (weights from the culture's mix)
  {
    const int w0 = C.peopleMix[0], w1 = C.peopleMix[1], w2 = C.peopleMix[2], tot = w0 + w1 + w2;
    const int x = tot > 0 ? (int)(h.next() % (uint32_t)tot) : 0;
    L.people = (uint8_t)(x < w0 ? 0 : (x < w0 + w1 ? 1 : 2));
  }
  // ---- skin and hair from the culture's ranges (elves in a human land keep to the paler half)
  {
    int lo = std::clamp((int)D.skinLo, 0, cult::kSkinSteps - 1), hi = std::clamp((int)D.skinHi, lo, cult::kSkinSteps - 1);
    if (L.people == 2 && C.archetype != cult::Archetype::Sylvan && C.archetype != cult::Archetype::Starspire) hi = lo + (hi - lo) / 2;
    L.skin = cult::skinRamp(lo + h.pick(hi - lo + 1)) | 0xFF000000u;
    int nh = 0;
    for (uint32_t hc : D.hairCols) nh += hc != 0;
    if (nh) {
      const int i = std::min(h.pick(nh), h.pick(4));   // the first colours are seen most
      L.hairColor = D.hairCols[std::min(i, nh - 1)] | 0xFF000000u;
    }
    L.hair = hairFor(h, D.hairStyles, female, L.hair);
    L.beard = !female && L.people != 2 && h.chance(D.beardP);
  }
  // ---- cloth: the culture's palette and cut (role colours of the M0/M2 trades stay, so trades stay readable)
  int nc = 0;
  for (uint32_t cc : D.cloth) nc += cc != 0;
  const auto cloth = [&](int i) { return nc ? (D.cloth[(size_t)(i % nc)] | 0xFF000000u) : L.topColor; };
  const bool clothRole = r == Role::Villager || r == Role::Merchant || r == Role::Innkeeper || r == Role::Farmer || r == Role::Child ||
                         r == Role::Fisher || r == Role::Traveller || r == Role::Refugee;
  if (clothRole) {
    if (r == Role::Villager || r == Role::Child || r == Role::Refugee) {
      const int a = h.pick(std::max(1, nc));
      L.topColor = cloth(a);
      L.bottomColor = darker(cloth(a + 1 + h.pick(std::max(1, nc - 1))), 0.72f);
    } else {
      L.bottomColor = darker(cloth(h.pick(std::max(1, nc))), 0.72f);
    }
    L.outfit = art::Outfit::Tunic;
    L.cut = (uint8_t)((int)(female ? D.cutF : D.cutM) + 1);
    if (D.pattern != cult::Pattern::Plain && h.chance(190)) {
      L.pattern = (uint8_t)D.pattern;
      L.patternColor = D.trim ? (D.trim | 0xFF000000u) : 0;
    }
  }
  // headwear (helmets and the legacy hood win in the painter)
  if (!L.hood && h.chance(D.headP)) {
    const int roll = h.pick(10);
    const cult::Headwear w = D.head[roll < 5 ? 0 : (roll < 8 ? 1 : 2)];
    if (w != cult::Headwear::None) {
      L.headwear = (uint8_t)w;
      static const uint32_t furs[4] = {rgba(118, 88, 64), rgba(150, 140, 128), rgba(84, 64, 54), rgba(200, 190, 172)};
      L.headColor = w == cult::Headwear::FurHat ? furs[h.pick(4)] : (w == cult::Headwear::Conical ? 0 : cloth(h.pick(std::max(1, nc)) + 2));
    }
  }
  if (h.chance(D.jewellery)) L.jewellery = (uint8_t)(1 + h.pick(3));
  if (D.facePaint && h.chance(female ? 70 : 130)) L.facePaint = D.facePaint;

  // ---- roles
  const cult::ArmsStyle& A = owner.arms;   // guards serve the owner
  const cult::ArmsStyle& Al = C.arms;      // bandits are local
  // (M6 FOES) the culture's own metal on the best-armed: its finest alloy's light and dark keys and its sheen (a royal
  // guard's or a captain's harness); 0 keeps the band's ramp
  auto applyAlloy = [&](const cult::ArmsStyle& S) {
    if (S.alloys.empty()) return;
    const cult::Alloy& al = S.alloys.back();
    if (!al.color) return;
    L.armourTint = al.color | 0xFF000000u;
    L.armourTint2 = al.color2 ? (al.color2 | 0xFF000000u) : 0;
    L.sheen = (uint8_t)((int)al.sheen + 1);
  };
  // (M6 FOES) the culture's polearm (weapon 8: spear, glaive, halberd) and bow (self, recurve, composite, crossbow,
  // longbow) forms
  auto polearmOf = [](const cult::ArmsStyle& S) -> uint8_t { return S.polearm == cult::Polearm::None ? (uint8_t)((int)cult::Polearm::Spear + 1) : (uint8_t)((int)S.polearm + 1); };
  auto bowOf = [](const cult::ArmsStyle& S) -> uint8_t { return (uint8_t)((int)S.bow + 1); };
  auto applyArms = [&](const cult::ArmsStyle& S, int band, int bodyI, int helmI) {
    L.armorStyle = (uint8_t)band;
    L.helmStyle = (uint8_t)band;
    L.helmet = true;
    L.bodyForm = (uint8_t)((int)S.body[bodyI] + 1);
    L.helmForm = (uint8_t)((int)S.helm[helmI] + 1);
    L.armsOrnament = S.ornament;
    L.pauldron = (uint8_t)(S.pauldron + 1);
    L.skirt = (uint8_t)(S.skirt + 1);
    L.crest = (uint8_t)(S.crest + 1);
    L.headwear = 0; L.cut = 0; L.pattern = 0; L.patternColor = 0;
  };
  switch (r) {
    case Role::Guard: {
      const int rk = rank.royal ? 2 : (rank.capital ? 1 : 0);
      applyArms(A, bandOf(A.metal, rk), 0, 0);
      L.shieldForm = (uint8_t)((int)A.shield + 1);
      L.bladeForm = (uint8_t)((int)A.blade + 1);
      L.weapon = 1;
      // (M6 fixer r3) the culture's own plume colour first (an imperial red crest), else the kingdom's trim
      L.plumeColor = A.plume ? (A.plume | 0xFF000000u) : L.trimColor;
      L.gloves = L.boots = (uint8_t)std::min<int>(L.armorStyle, 3);
      if (rk >= 1) { L.pauldron = (uint8_t)std::min(4, L.pauldron + 1); L.crest = (uint8_t)std::min(4, L.crest + 1); }
      if (rk == 2) L.armsOrnament |= cult::ARM_GILDING;
      if (!L.cloak) {   // the culture's cape in the kingdom's colours
        // (M6 finish fixer, review: "the imperial watch is a flat lemon-yellow robed shape with no legs") a long cloak
        // in a bright livery hid the whole harness and the legs: the watch wear the culture's short cape, the long
        // cloak is the royal guard's
        if (A.cape == 1 || (A.cape == 2 && rk < 2)) L.cape = true;
        else if (A.cape == 2) { L.cloak = 1; L.cloakColor = L.tabardColor; }
      }
      // (fixer M4 r3, review: "both guards pixel-identical") the watch are townsfolk: some keep a beard
      L.beard = !female && L.people != 2 && h.chance(100);
      // (M6) the royal guard wears the realm's own alloy; every watchman's spare bow is the culture's
      if (rk == 2) applyAlloy(A);
      // (M6 fixer, review: "Dune and Jade city watch wear the same generic livery") the rest of the watch wear the
      // culture's signature metal: the most colourful of its alloys (a Dune guard's bright bronze, a Jade guard's
      // glowing jadesilver), so a far city's guards show a strange metal, not the heartland's bright steel
      if (rk < 2) {
        const cult::Alloy* sig = nullptr;
        int best = -1;
        for (const cult::Alloy& al : A.alloys) {
          if (!al.color) continue;
          const int r0 = al.color & 255, g0 = (al.color >> 8) & 255, b0 = (al.color >> 16) & 255;
          const int chroma = std::max(r0, std::max(g0, b0)) - std::min(r0, std::min(g0, b0));
          if (chroma > best) { best = chroma; sig = &al; }
        }
        const uint32_t mt = A.metal;
        const int mr = mt & 255, mg = (mt >> 8) & 255, mb = (mt >> 16) & 255;
        const int mChroma = std::max(mr, std::max(mg, mb)) - std::min(mr, std::min(mg, mb));
        if (!(sig && best >= 40) && mt && mChroma >= 24) {
          // (M6 fixer r3) no colourful alloy: the culture's own metal (an imperial bronze, not the gilded band's lemon)
          auto sc = [](int v, float k) { return std::clamp((int)std::lround(v * k), 0, 255); };
          L.armourTint = rgba(sc(mr, 0.98f), sc(mg, 0.92f), sc(mb, 0.9f));
          L.armourTint2 = rgba(sc(mr, 0.42f), sc(mg, 0.36f), sc(mb, 0.34f));
          L.sheen = (uint8_t)((int)cult::Sheen::Matte + 1);
        }
        if (sig && best >= 40) {
          // (M6 fixer r3, review: "the imperial watch is a flat lemon-yellow robe") the watch's everyday harness: the
          // signature metal weathered toward the culture's own (less saturated, a step darker), so plate breaks and
          // shading read on it; the royal guard alone wears it bright
          const uint32_t base = A.metal ? (A.metal | 0xFF000000u) : rgba(150, 150, 156);
          L.armourTint = darker(blend(sig->color | 0xFF000000u, base, 0.5f), 0.8f);
          L.armourTint2 = darker(sig->color2 ? blend(sig->color2 | 0xFF000000u, base, 0.3f) : L.armourTint, sig->color2 ? 0.85f : 0.5f);
          // a polished or burnished finish is the royal guard's; the watch's harness is worn matte
          const cult::Sheen sh = sig->sheen == cult::Sheen::Bright || sig->sheen == cult::Sheen::Burnished ? cult::Sheen::Matte : sig->sheen;
          L.sheen = (uint8_t)((int)sh + 1);
        }
        // legs that read: the skirt stops above the knee and the boots are leather, so the figure is not one column
        L.skirt = (uint8_t)std::min<int>(L.skirt, 2);
        if (rk == 0) L.boots = 1;
        // (M6 finish fixer, review: "no visible legs or boots: the imperial watch is one column of yellow") dark hose
        // under the harness (a step of the livery toward leather brown), so the legs part from the armour at 1x
        L.bottomColor = darker(blend(L.tabardColor ? L.tabardColor : rgba(90, 80, 70), rgba(70, 54, 44), 0.65f), 0.7f);
      }
      L.bowForm = bowOf(A);
      break;
    }
    // (M4) a kingdom's soldier: the owner culture's arms one step plainer than the town watch (the second body and helm
    // forms of its grammar), its shield, a spear; the tabard in the kingdom's colours (set by makeLook)
    case Role::Soldier: {
      // (M6 fixer r3, review: "the common soldier wears the same grey dome helm in 10 of 12 cultures") the rank seen
      // most wears the culture's SIGNATURE silhouette: its first helm form (a fancy great helm or winged helm stays the
      // captain's: the soldier takes the second), its first body form (plate is a captain's: mail instead), its own
      // metal (bronze, brass, lacquer, green bronze: ArmsStyle::metal when it is not plain grey) and leather or metal
      // boots by how heavy the harness is. The captain keeps the alloy, the cloak, the crest and the plumes.
      const bool fancy = A.helm[0] == cult::HelmForm::GreatHelm || A.helm[0] == cult::HelmForm::Winged;
      applyArms(A, bandOf(A.metal, 0), 0, fancy ? 1 : 0);
      if (L.bodyForm == (uint8_t)((int)cult::BodyArm::Plate + 1)) L.bodyForm = (uint8_t)((int)cult::BodyArm::Mail + 1);
      L.shieldForm = (uint8_t)((int)A.shield + 1);
      L.bladeForm = (uint8_t)((int)A.blade + 1);
      L.plumeColor = A.plume ? (A.plume | 0xFF000000u) : 0;
      {
        const uint32_t m = A.metal;
        const int r0 = m & 255, g0 = (m >> 8) & 255, b0 = (m >> 16) & 255;
        const int chroma = std::max(r0, std::max(g0, b0)) - std::min(r0, std::min(g0, b0));
        const int sum = r0 + g0 + b0;
        if (m && (chroma >= 24 || sum < 300)) {
          // the light key a little above the culture's mid-tone metal, the dark key well below it
          auto sc = [](int v, float k) { return std::clamp((int)std::lround(v * k), 0, 255); };
          L.armourTint = rgba(sc(r0, 1.18f), sc(g0, 1.16f), sc(b0, 1.12f));
          L.armourTint2 = rgba(sc(r0, 0.55f), sc(g0, 0.52f), sc(b0, 0.5f));
          L.sheen = (uint8_t)((int)(sum < 300 ? cult::Sheen::Dark : cult::Sheen::Matte) + 1);   // blackened lacquer : matte bronze
        } else if (!A.alloys.empty() && A.alloys.front().color) {
          // plain grey iron is everyone's: a grey-metal culture's soldiers wear its commonest alloy's finish instead
          const cult::Alloy& al = A.alloys.front();
          L.armourTint = al.color | 0xFF000000u;
          L.armourTint2 = al.color2 ? (al.color2 | 0xFF000000u) : 0;
          L.sheen = (uint8_t)((int)al.sheen + 1);
        }
      }
      // (M6 finish fixer, review must: "Highland, Marsh, River, Sylvan and Starspire soldiers cannot be told apart at
      // 1x") ten helm forms for twelve peoples: the culture's grammar alone let Highland / Marsh / River all draw the
      // kettle and Imperial / Sylvan / Starspire all draw the crest. The common soldier now wears its people's SIGNATURE
      // helm (the strongest silhouette its grammar allows, never another people's), so each is tellable at 16 px:
      // fjord horns, the highland bonnet (no helm: a clansman's blue cap over the quilted coat), the heartland nasal,
      // the imperial crest, the dune aventail, the steppe spired spangenhelm in lamellar, the marsh's boiled-leather
      // kettle, the jade mask, the river's bright kettle in brigandine, the sun temple's plume, the sylvan crest over
      // leaf scales, the starspire wings. Two peoples share a form only where metal and body tell them apart.
      {
        using HF = cult::HelmForm;
        using BA = cult::BodyArm;
        struct Sig { HF helm; BA body; };
        static const Sig kSig[(int)cult::Archetype::COUNT] = {
            {HF::Horned, BA::Mail},             // Fjordfolk
            {HF::Kettle, BA::Padded},           // Highland (bonnet below: no helm)
            {HF::Nasal, BA::Mail},              // Heartland
            {HF::Crested, BA::Scale},           // Imperial
            {HF::ConicalAventail, BA::Mail},    // Dune
            {HF::Spangen, BA::Lamellar},        // Steppe
            {HF::Kettle, BA::Leather},          // Marsh
            {HF::Masked, BA::Lamellar},         // Jade
            {HF::Kettle, BA::Brigandine},       // River
            {HF::Plumed, BA::Padded},           // SunTemple
            {HF::Crested, BA::Leaf},            // Sylvan
            {HF::Winged, BA::Scale},            // Starspire
        };
        const int ar = (int)owner.archetype;
        if (ar >= 0 && ar < (int)cult::Archetype::COUNT) {
          L.helmForm = (uint8_t)((int)kSig[ar].helm + 1);
          L.bodyForm = (uint8_t)((int)kSig[ar].body + 1);
          if (owner.archetype == cult::Archetype::Highland) {
            // the clansman's bonnet: no helm, a cap in the kingdom's colour (darkened) with the culture's plume colour
            L.helmet = false; L.helmStyle = 0; L.helmForm = 0;
            L.headwear = (uint8_t)cult::Headwear::Cap;
            L.headColor = darker(L.tabardColor ? L.tabardColor : rgba(52, 64, 110), 0.55f);
          } else if (owner.archetype == cult::Archetype::Marsh) {
            L.helmStyle = 1;   // boiled leather, not iron
          }
        }
      }
      const cult::BodyArm bf = (cult::BodyArm)std::max(0, (int)L.bodyForm - 1);
      const bool heavy =bf == cult::BodyArm::Mail || bf == cult::BodyArm::Scale || bf == cult::BodyArm::Lamellar || bf == cult::BodyArm::Brigandine;
      L.gloves = L.boots = (uint8_t)std::min<int>(L.armorStyle, heavy ? 2 : 1);
      // (M6 finish fixer) dark hose between the harness and the boots, so the legs part from the armour at 1x
      L.bottomColor = darker(blend(L.tabardColor ? L.tabardColor : rgba(90, 80, 70), rgba(70, 54, 44), 0.65f), 0.7f);
      L.beard = L.beard && h.chance(90);
      {
        // (M6 fixer round 2, review: "the sun-temple soldier's face is a black blob at 1x") under a helm's brow shadow a
        // dark beard on a dark face merges with it into one black shape below the eyes: a soldier of dark skin whose
        // hair is no lighter than it goes clean-shaven, so the mouth and jaw read
        auto lum = [](uint32_t c) { return ((int)(c & 255) * 3 + (int)((c >> 8) & 255) * 6 + (int)((c >> 16) & 255)) / 10; };
        const int ls = lum(L.skin), lh = lum(L.hairColor);
        if (L.beard && ls < 130 && lh <= ls + 20) L.beard = false;
      }
      // (M6) the culture's polearm in hand, and one in five carries its bow slung on the back
      L.polearmForm = polearmOf(A);
      L.bowForm = bowOf(A);
      if (h.chance(52)) L.backItem |= 1;
      break;
    }
    // (M4) a captain: the best of the culture's arms (rank 1), a crest and a cloak in the kingdom's colours, a blade
    case Role::Captain: {
      applyArms(A, bandOf(A.metal, 1), 0, 0);
      L.shieldForm = (uint8_t)((int)A.shield + 1);
      L.bladeForm = (uint8_t)((int)A.blade + 1);
      L.pauldron = (uint8_t)std::min(4, L.pauldron + 1);
      L.crest = (uint8_t)std::min(4, L.crest + 2);
      L.armsOrnament |= cult::ARM_PLUMES;
      L.plumeColor = L.trimColor ? L.trimColor : (A.plume ? (A.plume | 0xFF000000u) : 0);
      L.gloves = L.boots = 3;
      applyAlloy(A);   // (M6) the captain's harness in the realm's finest metal
      L.bowForm = bowOf(A);
      break;
    }
    // (M4) a refugee: the village's own clothes, faded and patched, a headscarf or hood against the road
    case Role::Refugee: {
      L.topColor = blend(L.topColor, rgba(120, 108, 92), 0.45f);
      L.bottomColor = blend(L.bottomColor, rgba(70, 60, 52), 0.4f);
      L.pattern = 0; L.patternColor = 0; L.jewellery = 0;
      if (h.chance(150)) { L.headwear = (uint8_t)(female ? cult::Headwear::Headscarf : cult::Headwear::Hood); L.headColor = darker(L.topColor, 0.8f); }
      break;
    }
    case Role::Bandit: {
      // a poor local version: leather or padding, a leather helm of the local form (or the local hood), local blades
      L.armorStyle = (uint8_t)(h.chance(150) ? 1 : 0);
      if (L.outfit == art::Outfit::Leather || L.armorStyle) L.bodyForm = (uint8_t)((int)Al.body[1] + 1);
      if (L.bodyForm > (int)cult::BodyArm::Leather + 1) L.bodyForm = (uint8_t)((int)cult::BodyArm::Leather + 1);
      if (!L.hood && h.chance(110)) { L.helmStyle = 1; L.helmForm = (uint8_t)((int)Al.helm[1] + 1); L.headwear = 0; }
      if (L.weapon == 1) L.bladeForm = (uint8_t)((int)Al.blade + 1);
      if (h.chance(70) && Al.shield != cult::ShieldForm::None) { L.shieldForm = (uint8_t)((int)cult::ShieldForm::Buckler + 1); }
      L.armsOrnament = (uint16_t)(Al.ornament & (cult::ARM_FUR_TRIM | cult::ARM_STUDS | cult::ARM_TASSELS));
      L.cut = 0; L.pattern = 0;
      // (M6) mismatched pieces taken off the dead: now and then a soldier's iron helm of the land's form over the
      // leathers, odd gloves or boots; the local bow for the archers; never an alloy
      if (!L.hood && h.chance(55)) { L.helmStyle = 2; L.helmForm = (uint8_t)((int)Al.helm[0] + 1); L.headwear = 0; }
      if (h.chance(110)) L.gloves = 1;
      if (h.chance(90)) L.boots = (uint8_t)(h.chance(60) ? 2 : 1);
      L.bowForm = bowOf(Al);
      L.armourTint = 0; L.armourTint2 = 0; L.sheen = 0;
      // (M6 fixer round 2, review: "bandits look the same across cultures: a grey kettle helm, a brown jerkin and a
      // buckler in fjordfolk, heartland, imperial, dune, marsh, jade, river and sylvan alike") each people's outlaws
      // wear their OWN poor kit, read at 1x by the head first: a fur hat, a bonnet, the greenwood hood, a leather
      // crested cap, a turban, a leather spangenhelm, a marsh hood, a straw hat, a bandana, a plumed leather cap, a
      // circlet, a cowl; the coat padded or leather (never better: a bandit is poor), dyed in the land's cloth, and
      // the shield its people's own (or none). A few still wear a soldier's iron helm taken off the dead.
      {
        using HF = cult::HelmForm;
        using BA = cult::BodyArm;
        using SF = cult::ShieldForm;
        using HW = cult::Headwear;
        struct Kit { int8_t helm; HW head; BA body; SF shield; uint32_t headCol; };   // helm -1: the headwear instead
        static const Kit kKit[(int)cult::Archetype::COUNT] = {
            {-1, HW::FurHat, BA::Leather, SF::Round, rgba(112, 80, 56)},                    // Fjordfolk
            {-1, HW::Cap, BA::Padded, SF::None, rgba(40, 52, 96)},                          // Highland
            {-1, HW::Hood, BA::Leather, SF::Buckler, rgba(70, 96, 52)},                     // Heartland
            {(int8_t)HF::Crested, HW::None, BA::Leather, SF::Oval, 0},                      // Imperial
            {-1, HW::Turban, BA::Padded, SF::None, rgba(218, 204, 170)},                    // Dune
            {(int8_t)HF::Spangen, HW::None, BA::Leather, SF::None, 0},                      // Steppe
            {-1, HW::Hood, BA::Padded, SF::None, rgba(86, 84, 58)},                         // Marsh
            {-1, HW::Conical, BA::Padded, SF::None, 0},                                     // Jade
            {-1, HW::Headscarf, BA::Leather, SF::Buckler, rgba(168, 48, 40)},               // River
            {(int8_t)HF::Plumed, HW::None, BA::Padded, SF::Oval, 0},                        // SunTemple
            {-1, HW::Circlet, BA::Padded, SF::Leaf, rgba(196, 170, 96)},                    // Sylvan
            {-1, HW::Veil, BA::Padded, SF::Kite, rgba(70, 64, 104)},                        // Starspire
        };
        const int ar = (int)C.archetype;
        if (ar >= 0 && ar < (int)cult::Archetype::COUNT) {
          const Kit& K = kKit[ar];
          const bool looted = L.helmStyle == 2 && h.chance(80);   // a third of the looted iron helms are kept
          if (!looted) {
            L.hood = false;
            if (K.helm >= 0) { L.helmet = true; L.helmStyle = 1; L.helmForm = (uint8_t)(K.helm + 1); L.headwear = 0; }
            else {
              L.helmet = false; L.helmStyle = 0; L.helmForm = 0;
              if (K.head == HW::Hood) { L.hood = true; L.headwear = 0; }
              else { L.headwear = (uint8_t)K.head; }
              L.headColor = K.headCol ? K.headCol : 0;
            }
          }
          L.bodyForm = (uint8_t)((int)K.body + 1);
          L.armorStyle = (uint8_t)std::max<int>(L.armorStyle, 1);
          L.outfit = art::Outfit::Leather;
          // the coat in the land's own cloth, dyed dark and worn (never the shirt's colour: it read as the same jerkin)
          L.topColor = darker(cloth(1 + h.pick(std::max(1, nc - 1))), 0.78f);
          if (L.hood && K.head == HW::Hood) L.bottomColor = darker(K.headCol, 0.9f);   // the hood is cut from the hose's cloth (the painter's rule)
          L.shieldForm = K.shield == SF::None ? 0 : (uint8_t)((int)K.shield + 1);
          if (K.shield == SF::None) L.shield = false;
        }
      }
      break;
    }
    case Role::Priest: {
      if (C.faith.colour) L.topColor = C.faith.colour | 0xFF000000u;
      if (C.faith.colour2) L.tabardColor = C.faith.colour2 | 0xFF000000u;
      const cult::Cut c0 = female ? D.cutF : D.cutM;
      L.cut = (uint8_t)((c0 == cult::Cut::Gown || c0 == cult::Cut::Kaftan ? (int)c0 : (int)cult::Cut::Robe) + 1);
      L.pattern = (uint8_t)cult::Pattern::BorderTrim;
      L.patternColor = L.tabardColor;
      if (L.headwear == (uint8_t)cult::Headwear::FurHat || L.headwear == (uint8_t)cult::Headwear::Cap) L.headwear = (uint8_t)cult::Headwear::Hood;
      break;
    }
    case Role::Mage: case Role::Herbalist: {
      const cult::Cut c0 = female ? D.cutF : D.cutM;
      L.cut = (uint8_t)((c0 == cult::Cut::Gown || c0 == cult::Cut::Kaftan ? (int)c0 : (int)cult::Cut::Robe) + 1);
      L.pattern = D.pattern != cult::Pattern::Plain ? (uint8_t)cult::Pattern::BorderTrim : 0;
      L.patternColor = L.tabardColor;
      break;
    }
    case Role::Jarl: {
      // the culture's own steel, bareheaded, with its shoulders, skirt and ornament
      L.armorStyle = 3;
      L.bodyForm = (uint8_t)((int)C.arms.body[0] + 1);
      L.armsOrnament = (uint16_t)(C.arms.ornament | cult::ARM_GILDING);
      L.pauldron = (uint8_t)std::min(4, C.arms.pauldron + 2);
      L.skirt = (uint8_t)(C.arms.skirt + 1);
      L.headwear = 0; L.cut = 0; L.pattern = 0;
      if (h.chance(140)) L.jewellery |= 2;
      break;
    }
    case Role::King: {
      // regalia: the culture's finest helm and body form, gilded, in the noble cloak (set by makeLook)
      L.armorStyle = bandOf(C.arms.metal, 2);   // the culture's finest metal; the crown-helm is always gilded
      L.helmStyle = L.armorStyle == 5 ? 5 : 4;
      L.bodyForm = (uint8_t)((int)C.arms.body[0] + 1);
      L.helmForm = (uint8_t)((int)C.arms.helm[0] + 1);
      L.armsOrnament = (uint16_t)(C.arms.ornament | cult::ARM_GILDING | cult::ARM_FILIGREE);
      L.pauldron = (uint8_t)std::min(4, C.arms.pauldron + 2);
      L.skirt = (uint8_t)(C.arms.skirt + 1);
      L.crest = (uint8_t)std::min(4, C.arms.crest + 2);
      L.plumeColor = L.tabardColor;
      L.bladeForm = (uint8_t)((int)C.arms.blade + 1);
      L.headwear = 0; L.cut = 0; L.pattern = 0;
      break;
    }
    case Role::Smith: case Role::Hunter: L.cut = 0; L.pattern = 0; break;
    default: break;
  }
  // ---- the name, in the culture's phonology (titles kept; guards keep theirs: the spawner names them)
  if (r != Role::Guard && r != Role::Soldier && r != Role::Herald) {
    Stream hn(seed ^ 0x6E616D65ull);   // its own stream: every bit of the person seed reaches the name
    std::string nm = cult::personName(C, hn.next(), female);
    if (!nm.empty()) {
      if (r == Role::Jarl) nm = std::string(cult::societyOf(owner).lordTitle) + " " + nm;   // (M3b fixer) the society's lord: NOYAN, EMIR, PROVOST...
      else if (r == Role::Priest) nm = "PRIEST " + nm;
      else if (r == Role::King) nm = std::string(cult::societyOf(owner).rulerTitle) + " " + nm;   // (fix) the society's own title: KHAN, HIGH JARL, DOGE...
      else if (r == Role::Captain) nm = "CAPTAIN " + nm;
      name = nm;
    }
  }
}

}  // namespace census

void Game::makeLook(Actor& a, Role r, Rng& rr) {
  const uint64_t entry = rr.s;   // the person's seed (read, never drawn): the culture stream starts from it
  art::HumanLook& L = a.look;
  static const uint32_t skins[] = {rgba(244, 204, 168), rgba(232, 180, 140), rgba(200, 146, 104), rgba(150, 100, 70), rgba(110, 72, 50)};
  static const uint32_t hairs[] = {rgba(40, 30, 24), rgba(90, 56, 30), rgba(150, 96, 50), rgba(210, 170, 90), rgba(180, 70, 40), rgba(200, 200, 200), rgba(240, 220, 150)};
  static const uint32_t cloth[] = {rgba(70, 110, 150), rgba(150, 60, 50), rgba(80, 120, 70), rgba(140, 110, 60), rgba(110, 80, 130), rgba(160, 140, 110), rgba(60, 70, 90), rgba(170, 120, 60)};
  bool female = rr.f() < 0.45f;
  L.skin = skins[rr.irange(5)];
  L.hairColor = hairs[rr.irange(7)];
  L.topColor = cloth[rr.irange(8)];
  L.bottomColor = cloth[rr.irange(8)];
  L.bottomColor = rgba((int)((L.bottomColor & 255) * 0.7f), (int)(((L.bottomColor >> 8) & 255) * 0.7f), (int)(((L.bottomColor >> 16) & 255) * 0.7f));
  L.hair = female ? (rr.f() < 0.5f ? art::Hair::Long : (rr.f() < 0.5f ? art::Hair::Braids : art::Hair::Ponytail))
                  : (rr.f() < 0.15f ? art::Hair::Bald : (rr.f() < 0.15f ? art::Hair::Mohawk : art::Hair::Short));
  L.beard = !female && rr.f() < 0.45f;
  L.outfit = female && rr.f() < 0.6f ? art::Outfit::Dress : art::Outfit::Tunic;
  L.weapon = 0;
  a.name = makePersonName(rr, female);
  switch (r) {
    case Role::Guard: L.outfit = art::Outfit::Guard; L.helmet = true; L.shield = true; L.weapon = 1; L.tabardColor = rgba(150, 40, 40); L.beard = false; break;
    case Role::Jarl: L.outfit = art::Outfit::Plate; L.cape = true; L.tabardColor = rgba(130, 30, 40); a.name = "JARL " + a.name; break;
    case Role::Priest: L.outfit = art::Outfit::Robe; L.topColor = rgba(232, 234, 244); L.tabardColor = rgba(150, 34, 48); a.name = "PRIEST " + a.name; break;   // white + crimson: never skin-toned
    case Role::Mage: L.outfit = art::Outfit::Robe; L.topColor = rgba(70, 60, 140); L.tabardColor = rgba(200, 180, 80); L.hood = rr.f() < 0.5f; L.weapon = 4; break;
    case Role::Smith: L.outfit = art::Outfit::Leather; L.weapon = 6; L.beard = !female; break;
    case Role::Innkeeper: L.outfit = female ? art::Outfit::Dress : art::Outfit::Tunic; L.topColor = rgba(170, 140, 100); break;
    case Role::Merchant: L.topColor = rgba(150, 70, 110); break;
    case Role::Farmer: L.topColor = rgba(140, 120, 70); L.weapon = 0; break;
    case Role::Bandit:
      L.outfit = rr.f() < 0.6f ? art::Outfit::Leather : art::Outfit::Rags; L.hood = rr.f() < 0.5f; L.tabardColor = rgba(80, 60, 50);
      L.weapon = rr.f() < 0.5f ? 1 : 2;
      break;
    case Role::King:
      // gilded plate under a long noble cloak in the kingdom's colours (below), a winged gilded helm, a sword
      L.outfit = art::Outfit::Plate; L.armorStyle = 4; L.helmStyle = 4; L.helmet = true; L.cloak = 3; L.beard = !female;
      L.weapon = 1; L.weaponColor = rgba(236, 206, 120); L.trimColor = rgba(236, 196, 92); L.amulet = true; L.ring = true;
      L.tabardColor = rgba(130, 30, 40); L.cloakColor = rgba(130, 30, 40);
      a.name = "KING " + a.name;
      break;
    // M2 wayside people (rpg/sim/wayside.cpp): colours only, after the shared draws, so every other look is unchanged
    case Role::Hunter:
      L.outfit = art::Outfit::Leather; L.topColor = rgba(96, 112, 64); L.bottomColor = rgba(84, 64, 44); L.weapon = 3;
      L.hood = (hash32((uint32_t)a.slot * 977u + 13u) & 1) != 0; L.tabardColor = rgba(120, 90, 54);
      break;
    case Role::Fisher: L.outfit = art::Outfit::Tunic; L.topColor = rgba(74, 104, 132); L.bottomColor = rgba(70, 64, 54); break;
    case Role::Herbalist: L.outfit = art::Outfit::Robe; L.topColor = rgba(92, 128, 76); L.tabardColor = rgba(200, 180, 110); L.hood = true; break;
    case Role::Traveller: L.cape = true; L.tabardColor = rgba(110, 84, 60); L.topColor = rgba(140, 120, 90); L.hood = true; break;
    // M4 Banners (colours only, after the shared draws): the kingdoms' men-at-arms, the war's homeless, the realm's voice
    case Role::Soldier: L.outfit = art::Outfit::Guard; L.helmet = true; L.shield = true; L.weapon = 8; L.tabardColor = rgba(150, 40, 40); a.name = "SOLDIER"; break;
    case Role::Captain:
      L.outfit = art::Outfit::Plate; L.helmet = true; L.shield = true; L.weapon = 1; L.shieldStyle = 2; L.cloak = 1; L.beard = !female && L.beard;
      L.tabardColor = rgba(150, 40, 40); L.cloakColor = rgba(150, 40, 40);
      break;
    case Role::Refugee:
      L.outfit = female && L.outfit == art::Outfit::Dress ? art::Outfit::Dress : art::Outfit::Rags;
      L.weapon = (hash32((uint32_t)a.slot * 2654435761u ^ 0x5EF1u) % 3 == 0) ? 9 : 0;   // some lean on a stick
      break;
    case Role::Herald:
      // a tabard in the kingdom's colours over a padded coat (no mail, no helm), a horn at the hip
      L.outfit = art::Outfit::Guard; L.helmet = false; L.shield = false; L.weapon = 7; L.beard = false;
      L.tabardColor = rgba(150, 40, 40); L.cape = true;
      a.name = "HERALD";
      break;
    default: break;
  }
  // M1 kingdom identity (VISION_PLAN 15.8): guards wear their kingdom's colours; the king's cloak and the royal guard
  // (the palace and its barracks) carry the royal banner. (Colours only: the look's random draws above are unchanged.)
  const int home = a.site >= 0 ? a.site : (a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].site : -1);
  census::Rank rank;
  // (M4) the men of a kingdom (soldiers, captains, heralds) wear the arms of the kingdom they serve (Actor::realm),
  // wherever they stand: a besieger's camp, a road patrol, a capital's square
  const bool kingsMan = r == Role::Soldier || r == Role::Captain || r == Role::Herald;
  int servedK = -1;
  if (kingsMan && a.realm) { auto it = world.kingdomById.find(a.realm); if (it != world.kingdomById.end()) servedK = it->second; }
  if (kingsMan && servedK < 0 && home >= 0 && home < (int)world.sites.size()) servedK = world.sites[(size_t)home].kingdom;
  if (kingsMan && servedK >= 0) {
    const Kingdom& K = world.kingdoms[(size_t)servedK];
    L.tabardColor = K.color;
    L.trimColor = K.color2;
    if (r == Role::Captain) { L.cloakColor = K.color; L.shieldStyle = 2; }
    if (r == Role::Herald) { L.headwear = (uint8_t)cult::Headwear::Cap; L.headColor = K.color2 ? (K.color2 | 0xFF000000u) : K.color; }
    const cult::Culture* KC = world.cultureOfKingdom(servedK);
    const cult::Culture* LC = world.cultureOf(home);
    const cult::Culture* PC = KC ? KC : LC;   // soldiers are the kingdom's own people
    if (PC) {
      const uint64_t s = entry ^ ((uint64_t)(uint32_t)(a.slot + 1) << 24) ^ a.realm ^ PC->id;
      census::dress(L, a.name, r, female, *PC, KC ? *KC : *PC, s, rank);
      if (r == Role::Herald) { L.tabardColor = K.color; L.headwear = (uint8_t)cult::Headwear::Cap; L.headColor = K.color2 ? (K.color2 | 0xFF000000u) : K.color; L.cut = 0; L.pattern = 0; }
    }
    if (r == Role::Soldier) a.name = "SOLDIER";
    if (r == Role::Herald) a.name = "HERALD";
    if (KC || PC) a.culture = KC ? KC->id : PC->id;   // (M6) whose arms they wear: their loot is that culture's make
    return;
  }
  if (const Kingdom* K = world.kingdomOf(home)) {
    const art::Building in = a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].type : art::Building::House;
    const bool royal = in == art::Building::Palace || in == art::Building::Barracks;
    if (r == Role::Guard) {
      // (fixer M4 r3, review: "the occupier's guard is grey mail with a speck of yellow") the kingdom's stronger colour
      L.tabardColor = K->tabard();
      if (royal) {   // the royal guard: steel plate and great-helm, a heater shield with the royal field and trim
        L.outfit = art::Outfit::Plate; L.armorStyle = 3; L.helmStyle = 3; L.shieldStyle = 2; L.trimColor = K->color2;
        L.cloak = 3; L.cloakColor = K->color;
        a.name = "ROYAL GUARD";
        rank.royal = true;
      } else if (world.sites[(size_t)home].capital) {
        L.shieldStyle = 2; L.trimColor = K->color2;   // a capital's watch carries the royal shield
        rank.capital = true;
      }
    }
    if (r == Role::King) { L.cloakColor = K->color; L.tabardColor = K->color; }
    if (r == Role::Jarl) L.tabardColor = K->color;
  }
  // M3: the culture they live in (their settlement's), and the culture of whoever rules it (guards wear the owner's arms)
  const cult::Culture* C = world.cultureOf(home);
  const int kh = home >= 0 && home < (int)world.sites.size() ? world.sites[(size_t)home].kingdom : -1;
  const cult::Culture* owner = world.cultureOfKingdom(kh);
  if (C) {
    const uint64_t s = entry ^ ((uint64_t)(uint32_t)(a.site + 1) << 40) ^ ((uint64_t)(uint32_t)(a.bldg + 1) << 20) ^ (uint64_t)(uint32_t)a.slot ^ C->id;
    census::dress(L, a.name, r, female, *C, owner ? *owner : *C, s, rank);
    // (M6) whose arms they wear: a guard its owner's, a bandit the land's own (their loot is that culture's make)
    if (r == Role::Guard || r == Role::King || r == Role::Jarl) a.culture = owner ? owner->id : C->id;
    else if (r == Role::Bandit) a.culture = C->id;
  }
}
