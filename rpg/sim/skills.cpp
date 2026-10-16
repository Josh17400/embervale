// M9 "Arcana & Mastery": the skills' data, the gain curve, foe tags, the weapon families' and armour classes' tables,
// the skills block. SKILLS lane (phase A: the lead's first tables; the lane tunes them against --skills).
#include "rpg/sim/skills.h"
#include <algorithm>
#include <cmath>
#include "rpg/sim/foes.h"
#include "rpg/sim/game.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/world.h"

namespace skills {

namespace {
constexpr uint32_t rgb(int r, int g, int b) { return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | 0xFF000000u; }
const SkillInfo kInfo[SKILLS] = {
    {"SWORDS", "LAND BLOWS WITH A SWORD", Group::Combat, rgb(220, 226, 240), 0x5304Du},
    {"AXES", "LAND BLOWS WITH AN AXE", Group::Combat, rgb(232, 150, 110), 0xA7E5u},
    {"MACES", "LAND BLOWS WITH A MACE OR HAMMER", Group::Combat, rgb(200, 190, 170), 0x3ACEu},
    {"SPEARS", "LAND BLOWS WITH A SPEAR OR POLEARM", Group::Combat, rgb(214, 200, 140), 0x5BEAu},
    {"DAGGERS", "LAND BLOWS WITH A DAGGER", Group::Combat, rgb(170, 230, 200), 0xDA66u},
    {"ARCHERY", "HIT FOES WITH ARROWS", Group::Combat, rgb(170, 220, 120), 0xB0F5u},
    {"UNARMED", "FIGHT WITH YOUR FISTS", Group::Combat, rgb(236, 190, 150), 0xF157u},
    {"BLOCK", "TAKE BLOWS ON A SHIELD", Group::Combat, rgb(160, 180, 220), 0xB10Cu},
    {"LIGHT ARMOUR", "TAKE BLOWS IN LIGHT ARMOUR", Group::Armour, rgb(190, 220, 160), 0x1167u},
    {"MEDIUM ARMOUR", "TAKE BLOWS IN MEDIUM ARMOUR", Group::Armour, rgb(200, 180, 130), 0x3ED1u},
    {"HEAVY ARMOUR", "TAKE BLOWS IN HEAVY ARMOUR", Group::Armour, rgb(180, 190, 210), 0x4EA7u},
    {"HERITAGE", "WEAR AND WIELD A PEOPLE'S MAKE", Group::Armour, rgb(240, 200, 120), 0x4E21u},
    {"ATHLETICS", "RUN, ROLL AND TRAVEL ON FOOT", Group::Body, rgb(150, 230, 230), 0xA741u},
    {"SMITHING", "FORGE AND IMPROVE ARMS AND ARMOUR", Group::Craft, rgb(255, 160, 80), 0x5717u},
    {"COOKING", "COOK MEALS", Group::Craft, rgb(250, 210, 120), 0xC00Cu},
    {"FARMING", "TILL, SOW, TEND AND HARVEST", Group::Craft, rgb(170, 210, 100), 0xFA41u},
    {"RIDING", "RIDE A HORSE", Group::Body, rgb(210, 170, 120), 0x41DEu},
    {"SPEECH", "BUY, SELL AND PERSUADE", Group::Social, rgb(250, 220, 170), 0x5EECu},
    {"STEALTH", "SNEAK UNSEEN NEAR FOES", Group::Social, rgb(150, 150, 200), 0x57E1u},
    {"LOCKPICKING", "PICK LOCKS", Group::Social, rgb(200, 200, 160), 0x10C4u},
    {"DESTRUCTION", "HARM FOES WITH SPELLS", Group::Magic, rgb(255, 120, 60), 0xDE57u},
    {"RESTORATION", "HEAL AND WARD WITH SPELLS", Group::Magic, rgb(255, 230, 150), 0x4E57u},
    {"CONJURATION", "SUMMON AND BIND", Group::Magic, rgb(180, 120, 255), 0xC0A1u},
    {"ILLUSION", "FRIGHTEN, CHARM AND BLIND", Group::Magic, rgb(240, 140, 230), 0x1105u},
    {"ALTERATION", "SHAPE STONE, FLESH AND WATER", Group::Magic, rgb(120, 200, 255), 0xA17Eu},
};
}  // namespace

const SkillInfo& skillInfo(Skill s) { return kInfo[std::min<int>((int)s, SKILLS - 1)]; }
const char* groupName(Group g) {
  static const char* n[] = {"COMBAT", "ARMOUR", "BODY", "CRAFT", "SOCIAL", "MAGIC"};
  return (int)g < (int)Group::COUNT ? n[(int)g] : "";
}

// ---- the curve: 0.2 L^1.6 + 2 use points a level (12 220 acts of base 1 from 5 to 100: each level costs more, so the
//      gains slow as the skill climbs). The pace of each skill is skillRate: rpg_test --skills' play-rate bot measures
//      the acts an hour of play gives and asserts 10-20 h to master every skill.
float xpToNext(int level) {
  const int L = std::clamp(level, 1, MAX_LEVEL - 1);
  return 0.2f * std::pow((float)L, 1.6f) + 2.0f;
}
float xpToReach(int from, int to) {
  float s = 0;
  for (int L = std::max(1, from); L < std::min(to, MAX_LEVEL); L++) s += xpToNext(L);
  return s;
}
// (SKILLS lane) Skyrim's rule, scaled to this XP curve: a skill rising to L gives (L - 10) / 3 character XP. The first
// levels of a skill (to 12) give none, so the first hour's pace stays the kills' (--metrics: the first level-up); a
// skill climbing on to mastery gives about 1 350 over the climb, the deep skills of a long game a steady second road.
int charXpForSkillUp(int newLevel) { return std::max(0, (newLevel - 10) / 3); }
float skillRate(Skill s) {
  // calibrated by rpg_test --skills (the play-rate bot's acts per hour of play: tools/tests/test_skills.cpp)
  static const float r[SKILLS] = {
      0.42f, 0.37f, 0.40f, 0.40f, 0.39f, 0.55f, 0.44f, 0.90f,   // swords axes maces spears daggers archery unarmed block
      0.95f, 0.95f, 0.95f,                                      // light, medium, heavy armour (a blow taken each)
      5.00f, 2.74f,                                             // heritage (5 s worn in a fight), athletics (24 tiles)
      18.5f, 27.2f, 19.2f, 2.37f, 24.7f, 9.70f, 1.00f,          // smithing cooking farming riding speech stealth lockpicking
      1.00f, 1.00f, 1.00f, 1.00f, 1.00f};                       // the schools (the MAGIC lane's acts and pace)                       // the schools (the MAGIC lane's acts)
  return r[std::min<int>((int)s, SKILLS - 1)];
}
float levelDamageMul(int level) { return 1.0f + 0.003f * (float)std::max(0, std::min(level, MAX_LEVEL) - START_LEVEL); }
int trainerCap(SiteType where, bool capital) {
  if (capital) return 90;
  return where == SiteType::City ? 80 : where == SiteType::Town ? 60 : 40;
}
int trainPrice(int level) { return 20 + level * level / 6 + level * 2; }

// ---- perks
std::string perkBlocked(const Skills& k, Skill s, int idx) {
  int n = 0;
  const PerkDef* P = perkTable(s, n);
  if (idx < 0 || idx >= n) return "NO SUCH PERK";
  const PerkDef& d = P[idx];
  const Track& t = k[s];
  if (t.has(idx)) return "TAKEN";
  if (s == Skill::Lockpicking) return "NO LOCKS IN THE WORLD YET";   // (M9 fixer) reserved: its tree waits for locks
  if (t.level < d.need) return "NEEDS " + std::string(skillInfo(s).name) + " " + std::to_string((int)d.need);
  if (d.parent >= 0 && !t.has(d.parent)) return std::string("NEEDS ") + P[d.parent].name;
  if (d.parent2 >= 0 && !t.has(d.parent2)) return std::string("NEEDS ") + P[d.parent2].name;
  if (k.perkPts < d.cost) return "NO PERK POINTS";
  return "";
}

// ---- foe tags
const char* foeTagName(FoeTag t) {
  switch (t) {
    case FT_BEAST: return "BEASTS";
    case FT_UNDEAD: return "UNDEAD";
    case FT_SKELETAL: return "SKELETONS";
    case FT_ARMOURED: return "ARMOURED";
    case FT_PEOPLE: return "PEOPLE";
    case FT_CONSTRUCT: return "CONSTRUCTS";
    case FT_FLYING: return "FLIERS";
    case FT_CHARGER: return "CHARGERS";
    case FT_SHIELDED: return "SHIELDED";
    case FT_UNARMOURED: return "UNARMOURED";
    case FT_SPIRIT: return "SPIRITS";
    case FT_HUGE: return "HUGE FOES";
    case FT_DRAGON: return "DRAGONS";
    case FT_UNAWARE: return "THE UNAWARE";
  }
  return "";
}

uint16_t tagsOf(art::Monster mon, bool human, Role role, uint8_t rank, uint16_t foeAffixes) {
  using art::Monster;
  uint16_t t = 0;
  if (human) {
    t |= FT_PEOPLE;
    // the kingdoms' men-at-arms wear mail and carry a shield (their culture's arms: the M3 / M6 soldiers and guards)
    if (role == Role::Soldier || role == Role::Guard || role == Role::Captain || role == Role::Jarl || role == Role::King) t |= FT_ARMOURED | FT_SHIELDED;
    else t |= FT_UNARMOURED;
    if (rank >= (uint8_t)foes::Rank::Elite && role == Role::Bandit) { t |= FT_ARMOURED; t &= (uint16_t)~FT_UNARMOURED; }   // a chief's mail
  } else {
    switch (mon) {
      case Monster::Wolf: case Monster::IceWolf: case Monster::Hyena: case Monster::EmberHound: t |= FT_BEAST | FT_CHARGER | FT_UNARMOURED; break;
      case Monster::Boar: t |= FT_BEAST | FT_CHARGER | FT_UNARMOURED; break;
      case Monster::Bear: case Monster::Troll: case Monster::Yeti: t |= FT_BEAST | FT_HUGE | FT_UNARMOURED; break;
      case Monster::Slime: t |= FT_UNARMOURED; break;
      case Monster::Spider: case Monster::FrostSpider: case Monster::Blightspawn: t |= FT_BEAST | FT_UNARMOURED; break;
      case Monster::Bat: t |= FT_BEAST | FT_FLYING | FT_UNARMOURED; break;
      case Monster::Harpy: t |= FT_BEAST | FT_FLYING | FT_UNARMOURED; break;
      case Monster::Skeleton: t |= FT_UNDEAD | FT_SKELETAL; break;
      case Monster::Draugr: t |= FT_UNDEAD | FT_ARMOURED; break;
      case Monster::Wraith: t |= FT_UNDEAD | FT_SPIRIT; break;
      case Monster::Goblin: t |= FT_BEAST | FT_UNARMOURED; break;   // (M8b's classification: not a person, so a beast)
      case Monster::Mudcrab: case Monster::Scorpion: case Monster::Lurker: t |= FT_BEAST | FT_ARMOURED; break;
      case Monster::Sandworm: t |= FT_BEAST | FT_HUGE | FT_ARMOURED; break;
      case Monster::Dragon: t |= FT_DRAGON | FT_HUGE | FT_FLYING | FT_ARMOURED; break;
      case Monster::Wisp: t |= FT_SPIRIT | FT_FLYING; break;
      case Monster::Golem: t |= FT_CONSTRUCT | FT_ARMOURED | FT_HUGE; break;
      case Monster::SeaSerpent: case Monster::Kraken: t |= FT_BEAST | FT_HUGE; break;   // (M10)
      case Monster::Merrow: t |= FT_BEAST | FT_UNARMOURED; break;                      // (M10) fish-folk (not people: the vs-affixes' rule)
      default: t |= FT_BEAST; break;
    }
  }
  if (foeAffixes & foes::affixBit(foes::Affix::Armoured)) { t |= FT_ARMOURED; t &= (uint16_t)~FT_UNARMOURED; }
  if (foeAffixes & foes::affixBit(foes::Affix::Warded)) t |= FT_SHIELDED;
  if (foeAffixes & foes::affixBit(foes::Affix::Swift)) t |= FT_CHARGER;
  if (rank == (uint8_t)foes::Rank::WorldBoss) t |= FT_HUGE;
  return t;
}
uint16_t tagsOf(const Actor& a) {
  uint16_t t = tagsOf(a.mon, a.human, a.role, a.rank, a.affixes);
  if (a.human) {
    // what this body really wears and carries (the look follows its culture's arms and the gear it was dressed in)
    if (a.look.shield || a.look.shieldStyle) t |= FT_SHIELDED;
    if (a.look.armorStyle >= 2) { t |= FT_ARMOURED; t &= (uint16_t)~FT_UNARMOURED; }
  }
  if (a.hostile && !a.aggro) t |= FT_UNAWARE;
  return t;
}

// ---- weapon families
Skill familyOfType(WeaponType t) {
  switch (t) {
    case WeaponType::Sword: case WeaponType::Greatsword: return Skill::Sword;
    case WeaponType::Axe: return Skill::Axe;
    case WeaponType::Mace: return Skill::Mace;
    case WeaponType::Dagger: return Skill::Dagger;
    case WeaponType::Spear: return Skill::Spear;
    default: return Skill::Sword;
  }
}
Skill familyOf(const Item* w) {
  if (!w) return Skill::Unarmed;
  if (w->kind == ItemKind::Bow) return Skill::Bow;
  if (w->kind != ItemKind::Weapon) return Skill::Unarmed;
  return familyOfType((WeaponType)std::min<int>(w->sub, (int)WeaponType::COUNT - 1));
}
const FamilyTraits& familyTraits(Skill fam) {
  static const FamilyTraits sword{1.0f, 0, 1.0f, 0, "BALANCED. PARRIES A BLOW WHEN TIMED"};
  static const FamilyTraits axe{1.08f, 0, 1.05f, 0, "CLEAVES AND BLEEDS THE UNARMOURED"};
  static const FamilyTraits mace{1.25f, 0, 1.15f, 0, "SLOW, BUT CRUSHES PLATE AND BONE"};
  static const FamilyTraits spear{1.1f, 6, 1.0f, 0, "OUTREACHES ALL AND STOPS A CHARGE"};
  static const FamilyTraits dagger{0.8f, -3, 0.8f, 8, "QUICK. BACKSTABS AND CRITS THE UNAWARE"};
  static const FamilyTraits bow{1.0f, 0, 1.0f, 0, "RANGE. FINE ON BEASTS, POOR ON SHIELDS"};
  static const FamilyTraits fists{0.9f, -4, 0.75f, 0, "ALWAYS AT HAND. STAGGERS, BUT WEAK ON PLATE"};
  switch (fam) {
    case Skill::Sword: return sword;
    case Skill::Axe: return axe;
    case Skill::Mace: return mace;
    case Skill::Spear: return spear;
    case Skill::Dagger: return dagger;
    case Skill::Bow: return bow;
    default: return fists;
  }
}
const std::vector<VsRow>& familyVs(Skill fam) {
  static const std::vector<VsRow> none;
  static const std::vector<VsRow> sword = {{FT_PEOPLE, 10, "A DUELLIST'S BLADE"}, {FT_ARMOURED, -10, "GLANCES OFF PLATE"}};
  static const std::vector<VsRow> axe = {{FT_BEAST, 20, "CLEAVES HIDE"}, {FT_UNARMOURED, 15, "BITES DEEP"}, {FT_ARMOURED, -20, "CATCHES ON PLATE"}, {FT_CONSTRUCT, -15, "CHIPS ON STONE"}};
  static const std::vector<VsRow> mace = {{FT_ARMOURED, 35, "CRUSHES PLATE"}, {FT_SKELETAL, 40, "SHATTERS BONE"}, {FT_CONSTRUCT, 20, "CRACKS STONE"}, {FT_UNARMOURED, -10, "SLOW ON THE NIMBLE"}};
  static const std::vector<VsRow> spear = {{FT_CHARGER, 35, "STOPS A CHARGE"}, {FT_HUGE, 15, "KEEPS THE BIG ONES OFF"}, {FT_FLYING, 15, "REACHES FLIERS"}, {FT_PEOPLE, -15, "CLUMSY IN A BRAWL"}};
  static const std::vector<VsRow> dagger = {{FT_UNAWARE, 60, "BACKSTABS"}, {FT_PEOPLE, 15, "FINDS THE GAPS"}, {FT_ARMOURED, -30, "CANNOT PIERCE PLATE"}, {FT_HUGE, -20, "TOO SHORT FOR GIANTS"}};
  static const std::vector<VsRow> bow = {{FT_BEAST, 20, "A HUNTER'S WEAPON"}, {FT_FLYING, 20, "BRINGS DOWN FLIERS"}, {FT_SHIELDED, -35, "STOPPED BY SHIELDS"}, {FT_SKELETAL, -25, "PASSES THROUGH BONE"}};
  static const std::vector<VsRow> fists = {{FT_UNARMOURED, 10, "BRAWLER"}, {FT_ARMOURED, -40, "BRUISES ON PLATE"}};
  switch (fam) {
    case Skill::Sword: return sword;
    case Skill::Axe: return axe;
    case Skill::Mace: return mace;
    case Skill::Spear: return spear;
    case Skill::Dagger: return dagger;
    case Skill::Bow: return bow;
    case Skill::Unarmed: return fists;
    default: return none;
  }
}
int familyVsPct(Skill fam, uint16_t tags) {
  int s = 0;
  for (const VsRow& r : familyVs(fam)) if (tags & r.tag) s += r.pct;
  return std::clamp(s, -50, 80);
}
std::vector<std::string> familyVsLines(Skill fam) {
  std::vector<std::string> v;
  for (const VsRow& r : familyVs(fam))
    v.push_back((r.pct > 0 ? "+" : "") + std::to_string((int)r.pct) + "% VS " + foeTagName((FoeTag)r.tag) + ": " + r.why);
  return v;
}

// ---- armour classes
const char* weightName(Weight w) {
  static const char* n[] = {"", "LIGHT", "MEDIUM", "HEAVY"};
  return n[(int)w];
}
Weight weightOf(const Item& it) {
  switch (it.kind) {
    case ItemKind::Armor: case ItemKind::Helmet: case ItemKind::Shield: case ItemKind::Gloves: case ItemKind::Boots: break;
    case ItemKind::Cloak: return Weight::Light;
    default: return Weight::None;
  }
  switch (it.mat) {
    case Mat::Leather: case Mat::Cloth: return Weight::Light;
    case Mat::Bronze: case Mat::Bone: case Mat::Wood: return Weight::Medium;
    case Mat::Iron: case Mat::Steel: case Mat::Alloy: return Weight::Heavy;
    default: break;
  }
  // a classic piece (no material): by its tier (0 iron .. : medium), a body piece's power
  return it.power >= 20 ? Weight::Heavy : it.power >= 8 ? Weight::Medium : Weight::Light;
}
Skill armourSkill(Weight w) {
  switch (w) {
    case Weight::Light: return Skill::LightArmour;
    case Weight::Medium: return Skill::MediumArmour;
    case Weight::Heavy: return Skill::HeavyArmour;
    default: return Skill::COUNT;
  }
}
ArmourTrade armourTrade(Weight w, int L) {
  ArmourTrade t;
  // the skill softens the costs and grows the gifts: k runs from 0 at START_LEVEL to 1 at mastery. Light armour at its
  // start level is exactly neutral but for a little noise (the M5 first-hour numbers wear leather from level 3)
  const float k = std::clamp((float)(L - START_LEVEL) / (float)(MAX_LEVEL - START_LEVEL), 0.0f, 1.0f);
  switch (w) {
    case Weight::Light:
      t.speed = 1.0f + 0.04f * k; t.roll = 1.0f + 0.08f * k; t.rollCost = 1.0f - 0.12f * k; t.stRegen = 1.0f + 0.08f * k;
      t.noise = 0.2f - 0.1f * k; t.dr = 0.03f * k;
      break;
    case Weight::Medium:
      t.speed = 0.97f + 0.03f * k; t.roll = 0.96f + 0.04f * k; t.rollCost = 1.1f - 0.1f * k; t.stRegen = 0.96f + 0.04f * k;
      t.noise = 0.45f - 0.1f * k; t.dr = 0.03f + 0.05f * k;
      break;
    case Weight::Heavy:
      t.speed = 0.9f + 0.05f * k; t.roll = 0.84f + 0.08f * k; t.rollCost = 1.3f - 0.15f * k; t.stRegen = 0.85f + 0.08f * k;
      t.noise = 0.85f - 0.2f * k; t.dr = 0.06f + 0.08f * k;
      break;
    default: t.noise = 0.1f; break;
  }
  return t;
}

// ---- the block
void Skills::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(1);   // block version
  w.u8((uint8_t)SKILLS);
  for (const Track& k : t) { w.u8(k.level); w.f32(k.xp); w.u32(k.perks); }
  w.u8((uint8_t)Mat::COUNT);
  for (uint8_t f : matFam) w.u8(f);
  w.u32((uint32_t)cultFam.size());
  for (const auto& kv : cultFam) { w.u64(kv.first); w.u8(kv.second); }
  w.u16(perkPts); w.u16(perksTaken);
  w.u8(trainedLevel); w.u8(trainedCount);
  w.u32((uint32_t)booksRead.size());
  for (uint32_t b : booksRead) w.u32(b);
  w.u16(respecs);
  for (uint32_t u : uses) w.u32(u);
}
bool Skills::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u8() != 1) return false;
  const int n = r.u8();
  if (n > SKILLS) return false;   // a newer build's skills: refused
  *this = Skills();
  for (int i = 0; i < n && !r.bad; i++) {
    Track& k = t[i];
    k.level = (uint8_t)std::clamp<int>(r.u8(), 1, MAX_LEVEL);
    k.xp = r.f32();
    if (!std::isfinite(k.xp) || k.xp < 0) k.xp = 0;
    k.perks = r.u32();
  }
  const int nm = r.u8();
  for (int i = 0; i < nm && !r.bad; i++) { const uint8_t f = r.u8(); if (i < (int)Mat::COUNT) matFam[i] = std::min<uint8_t>(f, 100); }
  const uint32_t nc = r.u32();
  if (nc > 4096) return false;
  for (uint32_t i = 0; i < nc && !r.bad; i++) { const uint64_t id = r.u64(); cultFam[id] = std::min<uint8_t>(r.u8(), 100); }
  perkPts = r.u16(); perksTaken = r.u16();
  trainedLevel = r.u8(); trainedCount = r.u8();
  const uint32_t nb = r.u32();
  if (nb > 4096) return false;
  for (uint32_t i = 0; i < nb && !r.bad; i++) booksRead.push_back(r.u32());
  respecs = r.u16();
  for (int i = 0; i < n && !r.bad; i++) uses[i] = r.u32();
  return !r.bad;
}

void startLevels(Skills& k, Background bg) {
  k = Skills();
  auto up = [&](Skill s, int lv) { k[s].level = (uint8_t)std::max<int>(k[s].level, lv); };
  switch (bg) {
    case Background::Blacksmith: up(Skill::Smithing, 15); up(Skill::Mace, 10); break;
    case Background::Hunter: up(Skill::Bow, 15); up(Skill::Stealth, 10); break;
    case Background::Novice: up(Skill::Restoration, 15); up(Skill::Speech, 10); break;
    case Background::Urchin: up(Skill::Lockpicking, 15); up(Skill::Dagger, 10); up(Skill::Stealth, 10); break;
    case Background::Farmhand: up(Skill::Farming, 15); up(Skill::Athletics, 10); break;
    case Background::Noble: up(Skill::Speech, 15); up(Skill::Sword, 10); up(Skill::Riding, 10); break;
    case Background::Sailor: up(Skill::Athletics, 15); up(Skill::Unarmed, 10); break;
    case Background::Marked: up(Skill::Destruction, 15); up(Skill::Illusion, 10); break;
    default: break;
  }
}

void defaultsForLevel(Skills& k, int plLevel, Background bg, const Item* weapon, const Item* bodyArmour) {
  startLevels(k, bg);
  const int L = std::clamp(plLevel, 1, 80);
  // what a hero of that level has plausibly used (15.29.5): the wielded weapon's family most, the worn armour's class
  // next, Athletics, Heritage and Speech from the miles, the fights and the markets. The steps follow the play-rate model
  // of --skills (a level-20 hero of about 12 h of play: the main weapon near 62, the armour near 46, athletics 39)
  auto up = [&](Skill s, int lv) { if ((int)s < SKILLS) k[s].level = (uint8_t)std::clamp<int>(std::max<int>(k[s].level, lv), 1, MAX_LEVEL); };
  auto curve = [&](int top, int per10) { return std::min(top, START_LEVEL + per10 * (L - 1) / 10); };
  up(familyOf(weapon), curve(85, 30));
  if (bodyArmour) { const Skill a = armourSkill(weightOf(*bodyArmour)); if (a != Skill::COUNT) up(a, curve(75, 22)); }
  up(Skill::Athletics, curve(70, 18));
  up(Skill::Heritage, curve(50, 10));
  up(Skill::Speech, curve(45, 8));
  k.perkPts = (uint16_t)std::max(0, L - 1);
}

Item makeSkillBook(Skill s, uint32_t seed, int ilvl) {
  Item it;
  it.kind = ItemKind::Book;
  it.sub = BOOK_SKILL;
  it.form = (uint8_t)s;
  it.seed = seed;
  it.ilvl = (uint8_t)std::clamp(ilvl, 1, 60);
  it.icon = art::Icon::Book;
  it.tint = skillInfo(s).color;
  it.value = 60 + 6 * it.ilvl;
  it.name = std::string("A BOOK OF ") + skillInfo(s).name;
  return it;
}
bool isSkillBook(const Item& it) { return it.kind == ItemKind::Book && it.sub == BOOK_SKILL && it.form < SKILLS; }

}  // namespace skills
