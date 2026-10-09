// M6 Steel: how worn gear looks (rpg/sim/gear_look.h). ARMS lane.
//
// The rules (VISION_PLAN 7.2, 15.11 and the owner's M6 notes):
//   - a piece's MATERIAL decides its metal: leather (band 1, stitched brown), bronze (its own warm gold-brown ramp),
//     iron (band 2, dark grey), steel (band 3, bright blue-grey), a culture alloy (its light / dark keys and sheen);
//   - its MAKER culture decides its silhouette (ArmsStyle: helm, body, shield, blade, polearm, bow, the pauldron /
//     skirt / crest dials, ornament, plume) and Item::form may pick another form (the enum + 1);
//   - heartland pieces (culture 0) of a classic material (none, iron, steel) keep the M5 band look pixel for pixel;
//   - poor pieces (a low item level, common, leather or bronze) lose their ornament: bandits look like bandits;
//   - legendaries glint (HumanLook::glow); nothing is ever added by default: the hero starts in a shirt.
#include "rpg/sim/gear_look.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <initializer_list>
#include "rpg/culture/culture.h"

namespace {
// an armour piece's M5 material band (art::HumanLook): 1 leather, 2 iron, 3 steel, 4 gilded, 5 jade, 6 obsidian, 7 ember
uint8_t bandOf(const Item& it) {
  if (it.name.rfind("LEATHER", 0) == 0 || it.mat == Mat::Leather) return 1;
  return (uint8_t)std::clamp(it.tier + 2, 2, 7);
}
// shield shape by tier: iron round, steel heater, gilded kite, jade heater, obsidian tower, ember kite
uint8_t shieldShapeOf(const Item& it) {
  static const uint8_t s[7] = {1, 2, 3, 2, 4, 3, 4};
  return s[std::clamp((int)it.tier, 0, 6)];
}
// cloak style: mantles are fur-collared and short; the finer cloaks (tier 3+) get gilt trim
uint8_t cloakStyleOf(const Item& it) {
  if (it.name.find("MANTLE") != std::string::npos) return 2;
  return it.tier >= 3 ? 3 : 1;
}
// the culture's alloy a piece is made of (nullptr: none)
const cult::Alloy* alloyOf(const Item& it, const cult::Culture* c) {
  if (!c || it.mat != Mat::Alloy || it.alloy == 0 || it.alloy > c->arms.alloys.size()) return nullptr;
  return &c->arms.alloys[(size_t)it.alloy - 1];
}

// bronze: copper and tin, warm gold-brown (never the gilded band's yellow)
const uint32_t kBronzeLight = rgba(214, 156, 88), kBronzeDark = rgba(104, 58, 34);

// ---- colour helpers (the culture finishes and the exotic alloys)
int chn(uint32_t c, int i) { return (int)((c >> (8 * i)) & 255u); }
uint32_t mixc(uint32_t a, uint32_t b, float t) {
  auto m = [&](int i) { return (uint8_t)std::clamp((int)std::lround(chn(a, i) + (chn(b, i) - chn(a, i)) * t), 0, 255); };
  return rgba(m(0), m(1), m(2));
}
float lumc(uint32_t c) { return (0.299f * chn(c, 0) + 0.587f * chn(c, 1) + 0.114f * chn(c, 2)) / 255.0f; }
void toHsl(uint32_t c, float& h, float& s, float& l) {
  const float r = chn(c, 0) / 255.0f, g = chn(c, 1) / 255.0f, b = chn(c, 2) / 255.0f;
  const float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
  l = (mx + mn) * 0.5f;
  const float d = mx - mn;
  if (d < 1e-4f) { h = 0; s = 0; return; }
  s = l > 0.5f ? d / (2.0f - mx - mn) : d / (mx + mn);
  if (mx == r) h = (g - b) / d + (g < b ? 6.0f : 0.0f);
  else if (mx == g) h = (b - r) / d + 2.0f;
  else h = (r - g) / d + 4.0f;
  h *= 60.0f;
}
uint32_t fromHsl(float h, float s, float l) {
  auto f = [&](float n) {
    const float k = std::fmod(n + h / 30.0f, 12.0f), a = s * std::min(l, 1.0f - l);
    return (uint8_t)std::clamp((int)std::lround(255.0f * (l - a * std::max(-1.0f, std::min(std::min(k - 3.0f, 9.0f - k), 1.0f)))), 0, 255);
  };
  return rgba(f(0), f(8), f(4));
}
// A culture alloy must never read as plain steel (owner 15.11: "travelling far reveals unfamiliar metals"): a grey key
// takes a hue of its own (its own faint cast when it has one, else one of the exotic metals' hues by its name), and
// every alloy carries at least a clear colour.
void exotic(uint32_t light, uint32_t dark, const std::string& name, uint32_t& outL, uint32_t& outD) {
  static const float kHue[8] = {44, 12, 160, 214, 276, 348, 28, 186};   // gold, rose copper, verdigris, cobalt, violet,
                                                                         // crimson, amber, teal
  uint32_t nh = 2166136261u;
  for (char ch : name) nh = (nh ^ (uint8_t)ch) * 16777619u;
  float h, s, l, h2, s2, l2;
  toHsl(light, h, s, l);
  toHsl(dark, h2, s2, l2);
  if (s < 0.12f) h = kHue[nh % 8];
  outL = fromHsl(h, std::max(s, 0.42f), std::clamp(l, 0.30f, 0.80f)) | 0xFF000000u;
  outD = fromHsl(h, std::max(s2, 0.40f), std::clamp(l2, 0.10f, 0.32f)) | 0xFF000000u;
}

// what a piece is made of, as the painter sees it: the band (silhouette and ramp) and, for bronze and alloys, the metal
struct Metal {
  uint8_t band = 0;
  uint32_t light = 0, dark = 0;
  uint8_t sheen = 0;   // cult::Sheen + 1
};
Metal metalOf(const Item& it, const cult::Culture* c) {
  Metal m;
  if (it.mat == Mat::Leather || it.name.rfind("LEATHER", 0) == 0) { m.band = 1; return m; }
  if (it.mat == Mat::Bronze) { m.band = 2; m.light = kBronzeLight; m.dark = kBronzeDark; return m; }
  if (const cult::Alloy* a = alloyOf(it, c)) {
    m.band = 3;
    exotic(a->color, a->color2 ? a->color2 : mixc(a->color, 0, 0.55f), a->name, m.light, m.dark);
    m.sheen = (uint8_t)((int)a->sheen + 1);
    return m;
  }
  // a culture's iron and steel carry its smiths' finish (ArmsStyle::metal, a third of the way): blued, gilt-washed,
  // blackened, pale... the material still reads (iron dark and matte, steel bright), the maker reads beside it
  if (c && (it.mat == Mat::Iron || it.mat == Mat::Steel)) {
    const bool steel = it.mat == Mat::Steel;
    m.band = steel ? 3 : 2;
    if (c->arms.metal) {
      const uint32_t fin = c->arms.metal | 0xFF000000u;
      m.light = mixc(steel ? rgba(208, 214, 226) : rgba(150, 152, 160), fin, steel ? 0.38f : 0.32f) | 0xFF000000u;
      m.dark = mixc(steel ? rgba(78, 84, 100) : rgba(54, 54, 62), mixc(fin, 0, 0.6f), 0.35f) | 0xFF000000u;
      m.sheen = (uint8_t)((int)(steel ? cult::Sheen::Bright : cult::Sheen::Matte) + 1);
    }
    return m;
  }
  m.band = bandOf(it);   // the heartland's (and a classic item's) band: the M5 look
  return m;
}
// a poor piece: a bandit's or a beginner's (no ornament, no crest, small pauldrons)
bool poorOf(const Item& it) {
  return it.ilvl >= 1 && it.ilvl <= 4 && it.rarity == Rarity::Common && (it.mat == Mat::Leather || it.mat == Mat::Bronze);
}
template <class E> uint8_t formOr(uint8_t f, E fallback, int count) { return (uint8_t)(f >= 1 && f <= count ? f : (int)fallback + 1); }

// the forms a culture's piece takes (shared by the doll and the icons)
uint8_t helmFormOf(const Item& it, const cult::ArmsStyle& A) {
  const bool soft = it.mat == Mat::Leather || it.name.rfind("LEATHER", 0) == 0;
  return formOr(it.form, soft ? A.helm[1] : A.helm[0], (int)cult::HelmForm::COUNT);
}
uint8_t bodyFormOf(const Item& it, const cult::ArmsStyle& A) {
  using B = cult::BodyArm;
  const bool soft = it.mat == Mat::Leather || it.name.rfind("LEATHER", 0) == 0;
  B b;
  if (soft) {
    b = (A.body[1] == B::Padded || A.body[1] == B::Leather) ? A.body[1] : ((A.body[0] == B::Padded || A.body[0] == B::Leather) ? A.body[0] : B::Leather);
  } else {
    b = A.body[0];
    if (b == B::Padded) b = B::Brigandine;    // metal under the padding: plates riveted inside the cloth
    else if (b == B::Leather) b = B::Lamellar;
  }
  uint8_t f = formOr(it.form, b, (int)B::COUNT);
  if (it.mat == Mat::Alloy) {   // the culture's master work: a finer cut than its everyday steel
    switch ((B)(f - 1)) {
      case B::Mail: f = (uint8_t)((int)B::Scale + 1); break;
      case B::Padded: case B::Leather: f = (uint8_t)((int)B::Lamellar + 1); break;
      case B::Brigandine: f = (uint8_t)((int)B::Plate + 1); break;
      default: break;
    }
  }
  return f;
}
uint8_t shieldFormOf(const Item& it, const cult::ArmsStyle& A) {
  if (A.shield == cult::ShieldForm::None && !it.form) return 0;
  const uint8_t f = formOr(it.form, A.shield, (int)cult::ShieldForm::COUNT);
  return f == (int)cult::ShieldForm::None + 1 ? 0 : f;
}
uint8_t polearmFormOf(const Item& it, const cult::Culture* c) {
  const cult::Polearm p = c && c->arms.polearm != cult::Polearm::None ? c->arms.polearm : cult::Polearm::Spear;
  const uint8_t f = formOr(it.form, p, (int)cult::Polearm::COUNT);
  return f == (int)cult::Polearm::None + 1 ? 1 : f;
}
uint16_t ornamentOf(const Item& it, const cult::ArmsStyle& A) {
  uint16_t o = A.ornament;
  if (poorOf(it)) o &= (uint16_t)(cult::ARM_FUR_TRIM | cult::ARM_STUDS | cult::ARM_TASSELS);
  if ((it.rarity >= Rarity::Epic || it.mat == Mat::Alloy) && it.mat != Mat::Leather) o |= cult::ARM_ETCHING;
  if (it.mat == Mat::Alloy) o |= cult::ARM_RIVETS;
  if (it.rarity == Rarity::Legendary && it.mat != Mat::Leather) o |= cult::ARM_FILIGREE;
  return o;
}
// a culture shield comes painted in its maker's colours (an alloy one is faced in its metal); 0 = the hero's own arms
void shieldPaint(const Item& it, const cult::ArmsStyle& A, const Metal& m, uint32_t& field, uint32_t& device) {
  field = device = 0;
  if (it.mat == Mat::Alloy && m.light) { field = m.dark; device = m.light; return; }
  if (!A.cloth) return;
  field = A.cloth | 0xFF000000u;
  if (poorOf(it)) { device = A.leather ? (A.leather | 0xFF000000u) : 0; return; }
  // the device: whichever of the plume, the metal finish or gold stands out best on the field
  const uint32_t cands[3] = {A.plume ? (A.plume | 0xFF000000u) : 0u, A.metal ? (A.metal | 0xFF000000u) : 0u, rgba(222, 186, 92)};
  float best = -1;
  for (uint32_t cnd : cands) {
    if (!cnd) continue;
    const float d = std::fabs(lumc(cnd) - lumc(field)) + 0.002f * (float)(std::abs(chn(cnd, 0) - chn(field, 0)) + std::abs(chn(cnd, 1) - chn(field, 1)) +
                                                                   std::abs(chn(cnd, 2) - chn(field, 2))) / 3.0f;
    if (d > best) { best = d; device = cnd; }
  }
}
uint32_t metalTint(const Item& it, const Metal& m) {
  if (m.light) return m.light;
  return it.tint ? it.tint : rgba(200, 205, 215);
}
}  // namespace

art::HumanLook appearanceLook(const Appearance& app, const cult::Culture* home) {
  art::HumanLook L;
  L.skin = app.skin;
  L.hairColor = app.hairColor;
  L.hair = (art::Hair)(app.hair < (uint8_t)art::Hair::COUNT ? app.hair : 1);
  L.beard = app.beard;
  L.eyeColor = app.eyeColor;
  L.build = app.build;
  L.topColor = app.topColor;
  L.bottomColor = app.bottomColor;
  L.tabardColor = rgba(170, 50, 40);   // the hero's heraldry: the painted field of every shield
  if (!app.heraldry.empty()) L.tabardColor = app.heraldry.field | 0xFF000000u;   // M3: personal arms chosen in the creator
  L.people = (uint8_t)std::min<int>(app.people, 2);   // M3: human, half-breed or elf (ears, proportions)
  L.outfit = art::Outfit::Tunic;
  if (home) {
    const cult::Cut c = app.female ? home->dress.cutF : home->dress.cutM;
    // a poncho or a robe still reads as "the shirt on their back"; every cut is cloth (armour hides it)
    L.cut = (uint8_t)((int)c + 1);
    if (home->dress.pattern != cult::Pattern::Plain) {
      L.pattern = (uint8_t)home->dress.pattern;
      L.patternColor = home->dress.trim ? (home->dress.trim | 0xFF000000u) : 0;
    }
  }
  return L;
}

void wearGear(art::HumanLook& L, const WornGear& g, const CultureLookup& cultureOf) {
  auto maker = [&](const Item* it) -> const cult::Culture* { return it && it->culture && cultureOf ? cultureOf(it->culture) : nullptr; };
  auto metal = [&](const Item* it) { return metalOf(*it, maker(it)); };
  if (g.armor) L.armorStyle = metal(g.armor).band;
  if (g.helmet) L.helmStyle = metal(g.helmet).band;
  if (g.gloves) L.gloves = metal(g.gloves).band;
  if (g.boots) L.boots = metal(g.boots).band;
  if (g.cloak) {
    L.cloak = cloakStyleOf(*g.cloak);
    L.cloakColor = g.cloak->tint ? g.cloak->tint : rgba(112, 82, 58);
  }
  if (g.shield) {
    L.shieldStyle = shieldShapeOf(*g.shield);
    L.trimColor = g.shield->tint;
    const Metal m = metal(g.shield);
    if (m.light) L.trimColor = m.light;
  }
  L.amulet = g.amulet != nullptr;
  L.ring = g.ring != nullptr;
  // in hand: the melee weapon, else the staff, else the bow; whatever else is equipped rides on the back
  auto tintOf = [&](const Item* it) { return metalTint(*it, metal(it)); };
  if (g.weapon) {
    static const uint8_t wmap[] = {1, 2, 6, 5, 1, 8};   // sword, axe, mace (hammer head), dagger, greatsword, spear (M6)
    static_assert(sizeof(wmap) / sizeof(wmap[0]) == (size_t)WeaponType::COUNT, "a hand look for every weapon type");
    L.weapon = wmap[g.weapon->sub % (int)WeaponType::COUNT];
    L.weaponColor = tintOf(g.weapon);
  } else if (g.staff) {
    L.weapon = 4;
    L.weaponColor = g.staff->tint ? g.staff->tint : rgba(200, 205, 215);
  } else if (g.bow) {
    L.weapon = 3;
    L.weaponColor = g.bow->tint ? g.bow->tint : rgba(200, 205, 215);
  }
  if (g.bow && L.weapon != 3) L.backItem |= 1;
  if (g.staff && L.weapon != 4) L.backItem |= 2;

  // ---- the maker cultures' silhouettes. Heartland pieces (no culture) set nothing here, so the M5 look is unchanged.
  if (const cult::Culture* c = maker(g.helmet)) {
    const cult::ArmsStyle& A = c->arms;
    L.helmForm = helmFormOf(*g.helmet, A);
    const bool poor = poorOf(*g.helmet);
    L.crest = (uint8_t)(poor ? 1 : (g.helmet->mat == Mat::Alloy ? std::min(A.crest + 1, 3) : A.crest) + 1);
    if (A.plume && !poor) L.plumeColor = A.plume | 0xFF000000u;
    if (!L.armsOrnament) L.armsOrnament = ornamentOf(*g.helmet, A);
  }
  if (const cult::Culture* c = maker(g.armor)) {
    const cult::ArmsStyle& A = c->arms;
    const bool poor = poorOf(*g.armor);
    L.bodyForm = bodyFormOf(*g.armor, A);
    L.pauldron = (uint8_t)(poor ? std::min<int>(A.pauldron, 1) + 1 : (g.armor->mat == Mat::Alloy ? std::min(A.pauldron + 1, 3) : A.pauldron) + 1);
    L.skirt = (uint8_t)(poor ? std::min<int>(A.skirt, 1) + 1 : A.skirt + 1);
    L.armsOrnament = ornamentOf(*g.armor, A);
    if (!L.plumeColor && A.plume && !poor) L.plumeColor = A.plume | 0xFF000000u;
    // (M6 fixer round 2, review: "a worn padded gambeson does not look like its icon: the shirt's colour, quilted to
    // a checker") a padded coat or a brigandine is cloth: the maker's cloth, as its icon shows it (gearIcon's accent),
    // never the shirt under it
    const cult::BodyArm bf = (cult::BodyArm)(L.bodyForm - 1);
    if (A.cloth && (bf == cult::BodyArm::Padded || bf == cult::BodyArm::Brigandine)) L.topColor = A.cloth | 0xFF000000u;
  }
  if (const cult::Culture* c = maker(g.shield)) {
    L.shieldForm = shieldFormOf(*g.shield, c->arms);
    shieldPaint(*g.shield, c->arms, metal(g.shield), L.shieldField, L.shieldDevice);
  }
  if (g.weapon) {
    const cult::Culture* c = maker(g.weapon);
    if (L.weapon == 8) L.polearmForm = polearmFormOf(*g.weapon, c);   // every M6 spear wears a proper head
    else if (c && L.weapon == 1) L.bladeForm = formOr(g.weapon->form, c->arms.blade, (int)cult::Blade::COUNT);
  }
  if (const cult::Culture* c = maker(g.bow)) L.bowForm = formOr(g.bow->form, c->arms.bow, (int)cult::BowKind::COUNT);

  // the metal: the body armour's (else, with no body armour, the helmet's, the gloves', the boots') tints every metal
  // piece; a steel or iron body keeps the band ramps
  const Item* src = g.armor ? g.armor : (g.helmet ? g.helmet : (g.gloves ? g.gloves : g.boots));
  if (src) {
    const Metal m = metal(src);
    if (m.light) {
      L.armourTint = m.light;
      L.armourTint2 = m.dark;
      L.sheen = m.sheen;
    }
  }
  // legendaries glint, in their alloy's light (else a warm gold)
  const Item* glowing[5] = {g.weapon, g.armor, g.helmet, g.shield, g.cloak};
  for (int i = 0; i < 5; i++)
    if (glowing[i] && glowing[i]->rarity == Rarity::Legendary) {
      L.glow |= (uint8_t)(1u << i);
      if (!L.glowColor) {
        const Metal m = metal(glowing[i]);
        L.glowColor = m.sheen ? m.light : rgba(255, 190, 80);
      }
    }
}

art::IconLook gearIcon(const Item& it, const cult::Culture* maker) {
  art::IconLook l;
  l.icon = it.icon;
  l.tint = it.tint;
  const bool m6 = maker || it.mat == Mat::Bronze || it.mat == Mat::Alloy || it.rarity >= Rarity::Epic;
  if (!m6 || it.kind == ItemKind::Material) return l;   // the classic icon (crafting stuff keeps its own)
  l.rarity = (uint8_t)it.rarity;
  l.mat = (uint8_t)it.mat;
  const Metal m = metalOf(it, maker);
  if (m.light && it.kind != ItemKind::Cloak && it.kind != ItemKind::Ring && it.kind != ItemKind::Amulet) {
    l.tint = m.light;
    l.tint2 = m.dark;
    l.sheen = m.sheen;
  }
  if (maker) {
    const cult::ArmsStyle& A = maker->arms;
    l.ornament = ornamentOf(it, A);
    l.accent = A.cloth ? (A.cloth | 0xFF000000u) : 0;
    switch (it.kind) {
      case ItemKind::Helmet: l.form = helmFormOf(it, A); if (A.plume && !poorOf(it)) l.accent = A.plume | 0xFF000000u; break;
      case ItemKind::Armor: l.form = bodyFormOf(it, A); break;
      case ItemKind::Shield:
        l.form = shieldFormOf(it, A);
        if (it.mat == Mat::Alloy && m.dark) l.accent = m.dark;   // faced in its metal, as on the doll (shieldPaint)
        break;
      case ItemKind::Bow: l.form = formOr(it.form, A.bow, (int)cult::BowKind::COUNT); break;
      case ItemKind::Weapon:
        if (it.sub == (uint8_t)WeaponType::Spear) l.form = polearmFormOf(it, maker);
        else if (it.sub == (uint8_t)WeaponType::Sword || it.sub == (uint8_t)WeaponType::Greatsword || it.sub == (uint8_t)WeaponType::Dagger)
          l.form = formOr(it.form, A.blade, (int)cult::Blade::COUNT);
        break;
      default: break;
    }
  } else if (it.kind == ItemKind::Weapon && it.sub == (uint8_t)WeaponType::Spear) {
    l.form = polearmFormOf(it, nullptr);
  }
  return l;
}
