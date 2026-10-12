// M8 Fellowship: the companions' pure rules (rpg/sim/party.h): the tables (vocations, deeds, skills, hooks), approval,
// personality, the fighting numbers, and the party block. FELLOWS lane (phase A: lead, first cut).
#include "rpg/sim/party.h"
#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include "rpg/sim/common.h"
#include "rpg/sim/life.h"

namespace party {

// ---------------------------------------------------------------- names
const char* vocationName(Vocation v) {
  static const char* n[] = {"WARRIOR", "ARCHER", "MAGE", "HEALER", "ROGUE", "HOUND"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Vocation::COUNT, "a name for every vocation");
  return (int)v < (int)Vocation::COUNT ? n[(int)v] : "?";
}
const char* originName(Origin o) {
  static const char* n[] = {"TAVERN REGULAR", "TOWNSFOLK", "MERCENARY", "RESCUED", "SWORN IN A TALE", "BONDED ANIMAL"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Origin::COUNT, "a name for every origin");
  return (int)o < (int)Origin::COUNT ? n[(int)o] : "?";
}
const char* stateName(State s) {
  static const char* n[] = {"WITH YOU", "BLEEDING OUT", "WAITING", "AT HOME", "LEFT YOU", "FALLEN"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)State::COUNT, "a name for every state");
  return (int)s < (int)State::COUNT ? n[(int)s] : "?";
}
const char* cmdName(Cmd c) {
  static const char* n[] = {"FOLLOW", "WAIT HERE", "ATTACK", "DEFEND ME", "USE SKILL", "HOLD BACK", "GO HOME"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Cmd::COUNT, "a name for every command");
  return (int)c < (int)Cmd::COUNT ? n[(int)c] : "?";
}
const char* hookName(Hook h) {
  static const char* n[] = {"NONE", "BURNED HOME", "LOST KIN", "WAR", "REVENGE", "HEIRLOOM", "DEBT", "EXILE", "FAITH", "AMBITION", "LOVE"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Hook::COUNT, "a name for every hook");
  return (int)h < (int)Hook::COUNT ? n[(int)h] : "?";
}
const char* slotName(int s) {
  static const char* n[] = {"WEAPON", "BOW", "STAFF", "ARMOUR", "HELMET", "SHIELD", "RING", "AMULET", "GLOVES", "BOOTS", "CLOAK"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)SLOTS, "a name for every slot");
  return s >= 0 && s < SLOTS ? n[s] : "?";
}
int slotOf(ItemKind k) {
  switch (k) {
    case ItemKind::Weapon: return S_WEAPON;
    case ItemKind::Bow: return S_BOW;
    case ItemKind::Staff: return S_STAFF;
    case ItemKind::Armor: return S_ARMOR;
    case ItemKind::Helmet: return S_HELMET;
    case ItemKind::Shield: return S_SHIELD;
    case ItemKind::Ring: return S_RING;
    case ItemKind::Amulet: return S_AMULET;
    case ItemKind::Gloves: return S_GLOVES;
    case ItemKind::Boots: return S_BOOTS;
    case ItemKind::Cloak: return S_CLOAK;
    default: return -1;
  }
}

// ---------------------------------------------------------------- deeds and approval
const DeedInfo& deedInfo(Deed d) {
  //                    name                    brave kind pious lawful base flags
  static const DeedInfo t[] = {
      {"HELPED SOMEONE",            0,  1,  0,  1,  2, 0},
      {"LET SOMEONE DOWN",          0, -1,  0,  0, -2, 0},
      {"BROUGHT SOMEONE HOME",      1,  2,  0,  0,  4, DF_LOUD},
      {"HELPED REFUGEES",           0,  2,  1,  0,  3, DF_HOME},
      {"GAVE TO THE NEEDY",         0,  2,  1,  0,  2, 0},
      {"MADE A FRIEND",             0,  1,  0,  0,  1, 0},
      {"SHOWED MERCY",             -1,  3,  1,  0,  2, DF_LOUD},
      {"CRUELTY",                   0, -3, -1, -1, -4, DF_LOUD},
      {"KILLED AN INNOCENT",        0, -3, -2, -3, -8, DF_HOME | DF_LOUD},
      {"KILLED A GUARD",            0, -1,  0, -3, -4, DF_HOME | DF_LOUD},
      {"STOLE",                     0, -1, -1, -3, -2, 0},
      {"PUT DOWN BANDITS",          2,  1,  0,  2,  3, 0},
      {"KILLED A BEAST",            2,  0,  0,  0,  1, 0},
      {"SLEW A LEGEND",             3,  1,  0,  0,  4, DF_LOUD},
      {"TOOK A SIDE IN A WAR",      2, -1,  0,  0,  0, DF_HOME | DF_LOUD},
      {"DEFENDED A TOWN",           2,  2,  0,  1,  4, DF_HOME | DF_LOUD},
      {"WENT INTO THE DARK",        2,  0, -1,  0,  1, 0},
      {"DRANK AT THE INN",         -1,  1, -1,  0,  1, 0},
      {"PRAYED",                    0,  0,  3,  0,  0, 0},
      {"PAID A BRIBE",              0,  0,  0, -2, -1, 0},
      {"LIED",                      0, -1,  0, -1, -2, 0},
      {"BROKE AN OATH",             0, -1, -1, -3, -4, DF_LOUD},
      {"ROBBED THE DEAD",           0, -1, -3, -1, -4, DF_LOUD},
      {"LEFT A FRIEND TO DIE",      1, -2,  0,  0, -6, DF_LOUD},
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Deed::COUNT, "a row for every deed");
  return t[(int)d < (int)Deed::COUNT ? (int)d : 0];
}
const char* deedName(Deed d) { return deedInfo(d).name; }

int approvalDelta(const Member& m, Deed d, ew::Gid about, ew::Gid homeland) {
  const DeedInfo& I = deedInfo(d);
  int v = I.base + I.brave * m.p.brave + I.kind * m.p.kind + I.pious * m.p.pious + I.lawful * m.p.lawful;
  if (m.p.likes & deedBit(d)) v += 4;
  if (m.p.dislikes & deedBit(d)) v -= 4;
  const bool home = about && ((homeland && about == homeland) || about == m.site);
  if (d == Deed::JoinedWar && about) {
    if (homeland && about == homeland) v = -12 - std::max(0, (int)m.p.lawful);         // fought against their own people
    else if (m.stake.kingdom && about == m.stake.kingdom &&
             (m.stake.hook == Hook::War || m.stake.hook == Hook::BurnedHome || m.stake.hook == Hook::Exile)) v = std::max(v, 8);   // their old enemy
  } else if ((I.flags & DF_HOME) && home) {
    v *= 2;
  }
  return std::clamp(v, -15, 15);
}

// ---------------------------------------------------------------- skills
const SkillInfo& skillInfo(Skill s) {
  static const SkillInfo t[] = {
      {"TAUNT", Vocation::Warrior, 20, 14, "DRAWS THE FOES NEARBY ONTO THEMSELVES."},
      {"SHIELD WALL", Vocation::Warrior, 50, 30, "HALVES THE BLOWS THAT LAND ON YOU BOTH FOR SIX SECONDS."},
      {"LAST STAND", Vocation::Warrior, 80, 120, "FIGHTS ON FOR FIVE SECONDS WHEN THEY SHOULD FALL."},
      {"PINNING SHOT", Vocation::Archer, 20, 10, "AN ARROW THAT SLOWS ITS MARK."},
      {"VOLLEY", Vocation::Archer, 50, 25, "ARROWS RAIN ON EVERY FOE NEAR THE MARK."},
      {"EAGLE EYE", Vocation::Archer, 80, 40, "LONG SHOTS THAT FIND THE GAPS IN ARMOUR."},
      {"FIREBOLT", Vocation::Mage, 20, 6, "A BOLT OF THEIR PEOPLE'S FIRE."},
      {"WARD", Vocation::Mage, 50, 30, "A SHIELD OF LIGHT AROUND YOU."},
      {"STORM", Vocation::Mage, 80, 45, "A BURST THAT STRIKES EVERY FOE AROUND THEM."},
      {"MEND", Vocation::Healer, 20, 12, "HEALS WHOEVER IS HURT WORST."},
      {"PURIFY", Vocation::Healer, 50, 25, "PUTS OUT BURNS AND LIFTS FROST AND POISON."},
      {"SECOND WIND", Vocation::Healer, 80, 90, "RAISES A FALLEN FRIEND FROM ACROSS THE FIELD."},
      {"FLANK", Vocation::Rogue, 20, 8, "SLIPS BEHIND THE FOE FOR A CRUEL BLOW."},
      {"SMOKE", Vocation::Rogue, 50, 30, "A CLOUD THAT MAKES THE FOES LOSE YOU."},
      {"ASSASSINATE", Vocation::Rogue, 80, 40, "A FINISHING BLOW ON THE BADLY HURT."},
      {"GUARD", Vocation::Hound, 20, 20, "STANDS OVER THE FALLEN AND HOLDS THE FOES OFF."},
      {"PIN DOWN", Vocation::Hound, 50, 18, "BRINGS A FOE DOWN AND HOLDS IT THERE."},
      {"FETCH", Vocation::Hound, 80, 10, "BRINGS BACK WHAT WAS DROPPED."},
  };
  static_assert(sizeof(t) / sizeof(t[0]) == (size_t)Skill::COUNT, "a row for every skill");
  return t[(int)s < (int)Skill::COUNT ? (int)s : 0];
}
std::array<Skill, 3> skillsOf(Vocation v) {
  const int b = std::clamp((int)v, 0, (int)Vocation::COUNT - 1) * 3;
  return {(Skill)b, (Skill)(b + 1), (Skill)(b + 2)};
}
uint8_t unlockedSkills(const Member& m) {
  uint8_t bits = 0;
  for (int k = 0; k < 3; k++) if (m.bond >= SKILL_BOND[k]) bits |= (uint8_t)(1u << k);
  return bits;
}

// ---------------------------------------------------------------- who they are
Vocation vocationFor(uint8_t job, uint8_t traits, uint32_t seed) {
  using life::Job;
  const uint32_t h = hash32(seed ^ 0x70CA7104u);
  switch ((Job)job) {
    case Job::Guard: case Job::Noble: case Job::Smith: case Job::Miner: case Job::Woodcutter: case Job::Labourer: return Vocation::Warrior;
    case Job::Hunter: return Vocation::Archer;
    case Job::Priest: return Vocation::Healer;
    case Job::Scholar: return Vocation::Mage;
    case Job::Beggar: case Job::Server: case Job::Bard: case Job::Merchant: case Job::Lamplighter: return h % 3 ? Vocation::Rogue : Vocation::Archer;
    case Job::Farmer: case Job::Herder: return (traits & life::TR_BRAVE) || h % 2 ? Vocation::Warrior : Vocation::Archer;
    case Job::Fisher: return h % 2 ? Vocation::Archer : Vocation::Rogue;
    case Job::Baker: case Job::Tailor: case Job::Servant: case Job::Innkeeper: return h % 3 == 0 ? Vocation::Healer : (h % 3 == 1 ? Vocation::Rogue : Vocation::Warrior);
    case Job::Stablehand: return h % 2 ? Vocation::Warrior : Vocation::Archer;
    default: break;
  }
  static const Vocation any[] = {Vocation::Warrior, Vocation::Archer, Vocation::Rogue, Vocation::Healer, Vocation::Mage};
  return any[h % 5u];
}

Personality personalityFor(uint8_t traits, uint8_t job, uint32_t seed) {
  using life::Job;
  Personality p;
  uint32_t h = hash32(seed ^ 0x9E3779B9u);
  auto roll = [&](int lo, int hi) { h = hash32(h + 0x6D2B79F5u); return lo + (int)(h % (uint32_t)(hi - lo + 1)); };
  // each axis: the traits first, then a little of the person's own (sequenced draws: brave, kind, pious, lawful)
  int brave = roll(-1, 1), kind = roll(-1, 1), pious = roll(-1, 1), lawful = roll(-1, 1);
  if (traits & life::TR_BRAVE) brave += 2;
  if (traits & life::TR_SOCIABLE) kind += 1;
  if (traits & life::TR_GRUMPY) kind -= 1;
  if (traits & life::TR_DEVOUT) pious += 2;
  if (traits & life::TR_THRIFTY) lawful += 1;
  if (traits & life::TR_LAZY) brave -= 1;
  const Job j = (Job)job;
  if (j == Job::Guard || j == Job::Noble || j == Job::Priest || j == Job::Scholar) lawful += 1;
  if (j == Job::Beggar || j == Job::Bard) lawful -= 1;
  if (j == Job::Priest) pious += 1;
  if (j == Job::Hunter || j == Job::Guard) brave += 1;
  p.brave = (int8_t)std::clamp(brave, -2, 2);
  p.kind = (int8_t)std::clamp(kind, -2, 2);
  p.pious = (int8_t)std::clamp(pious, -2, 2);
  p.lawful = (int8_t)std::clamp(lawful, -2, 2);
  // two likes and one or two dislikes, never against the grain of the axes (a kind soul does not like cruelty)
  const Member probe = [&] { Member m; m.p = p; return m; }();
  std::vector<int> pos, neg;
  for (int d = 0; d < (int)Deed::COUNT; d++) {
    const int v = approvalDelta(probe, (Deed)d, 0, 0);
    if (v >= 1) pos.push_back(d);
    if (v <= 0) neg.push_back(d);
  }
  for (int k = 0; k < 2 && !pos.empty(); k++) {
    const int i = roll(0, (int)pos.size() - 1);
    p.likes |= 1u << (unsigned)pos[(size_t)i];
    pos.erase(pos.begin() + i);
  }
  const int nd = roll(1, 2);
  for (int k = 0; k < nd && !neg.empty(); k++) {
    const int i = roll(0, (int)neg.size() - 1);
    p.dislikes |= 1u << (unsigned)neg[(size_t)i];
    neg.erase(neg.begin() + i);
  }
  return p;
}

// ---------------------------------------------------------------- numbers
int capacity(int playerLevel) { return playerLevel >= LEADERSHIP_LEVEL ? PARTY_PEOPLE_LEADER : PARTY_PEOPLE; }
int levelFor(int playerLevel) { return std::max(1, playerLevel - 1); }

Body bodyFor(const Member& m, int playerLevel) {
  //                        maxHp  dmg  armor speed range aggroR
  static const Body base[] = {{125, 12, 9, 50, 15, 110},    // Warrior
                              {90, 10, 5, 52, 120, 150},    // Archer (the range is the bow's)
                              {80, 13, 4, 48, 100, 140},    // Mage
                              {95, 7, 6, 48, 15, 120},      // Healer
                              {95, 13, 5, 56, 14, 120},     // Rogue
                              {95, 10, 5, 62, 14, 130}};    // Hound
  static_assert(sizeof(base) / sizeof(base[0]) == (size_t)Vocation::COUNT, "a body for every vocation");
  Body b = base[std::clamp((int)m.voc, 0, (int)Vocation::COUNT - 1)];
  const int L = levelFor(playerLevel);
  b.maxHp *= 1.0f + 0.10f * (float)(L - 1);
  b.dmg *= 1.0f + 0.09f * (float)(L - 1);
  b.armor += 1.2f * (float)L;
  if (m.voc == Vocation::Hound && (m.flags & MF_PUP)) { b.maxHp *= 0.6f; b.dmg *= 0.5f; }
  if (m.flags & MF_WOUNDED) b.maxHp *= 0.75f;
  return b;
}

// ---------------------------------------------------------------- the party
Member* Party::byId(uint32_t id) {
  for (Member& m : members) if (m.id == id) return &m;
  return nullptr;
}
const Member* Party::byId(uint32_t id) const {
  for (const Member& m : members) if (m.id == id) return &m;
  return nullptr;
}
int Party::indexOfActor(int actorId) const {
  if (actorId < 0) return -1;
  for (size_t i = 0; i < members.size(); i++) if (members[i].actor == actorId) return (int)i;
  return -1;
}
int Party::with() const {
  int n = 0;
  for (const Member& m : members) n += m.state == State::With || downedWith(m);
  return n;
}
int Party::people() const {
  int n = 0;
  for (const Member& m : members) n += (m.state == State::With || downedWith(m)) && m.voc != Vocation::Hound;
  return n;
}
bool Party::animal() const {
  for (const Member& m : members) if ((m.state == State::With || downedWith(m)) && m.voc == Vocation::Hound) return true;
  return false;
}
void Party::clear() { *this = Party(); }

namespace {
void writeLook(BinW& w, const art::HumanLook& L) {
  for (uint32_t c : {L.skin, L.hairColor, L.topColor, L.bottomColor, L.tabardColor, L.weaponColor, L.eyeColor, L.cloakColor, L.trimColor,
                     L.headColor, L.patternColor, L.plumeColor, L.armourTint, L.armourTint2, L.glowColor, L.shieldField, L.shieldDevice})
    w.u32(c);
  for (uint8_t b : {(uint8_t)L.hair, (uint8_t)L.outfit, (uint8_t)L.beard, (uint8_t)L.helmet, (uint8_t)L.hood, (uint8_t)L.cape, (uint8_t)L.shield,
                    L.weapon, L.build, L.helmStyle, L.armorStyle, L.gloves, L.boots, L.cloak, L.shieldStyle, L.backItem, (uint8_t)L.amulet,
                    (uint8_t)L.ring, L.people, L.cut, L.headwear, L.pattern, L.facePaint, L.jewellery, L.helmForm, L.bodyForm, L.shieldForm,
                    L.bladeForm, L.pauldron, L.skirt, L.crest, L.polearmForm, L.bowForm, L.sheen, L.glow})
    w.u8(b);
  w.u16(L.armsOrnament);
}
bool readLook(BinR& r, art::HumanLook& L) {
  for (uint32_t* c : {&L.skin, &L.hairColor, &L.topColor, &L.bottomColor, &L.tabardColor, &L.weaponColor, &L.eyeColor, &L.cloakColor, &L.trimColor,
                      &L.headColor, &L.patternColor, &L.plumeColor, &L.armourTint, &L.armourTint2, &L.glowColor, &L.shieldField, &L.shieldDevice})
    *c = r.u32();
  const uint8_t hair = r.u8(), outfit = r.u8();
  if (hair >= (uint8_t)art::Hair::COUNT || outfit >= (uint8_t)art::Outfit::COUNT) return false;
  L.hair = (art::Hair)hair; L.outfit = (art::Outfit)outfit;
  for (bool* b : {&L.beard, &L.helmet, &L.hood, &L.cape, &L.shield}) *b = r.u8() != 0;
  for (uint8_t* b : {&L.weapon, &L.build, &L.helmStyle, &L.armorStyle, &L.gloves, &L.boots, &L.cloak, &L.shieldStyle, &L.backItem}) *b = r.u8();
  L.amulet = r.u8() != 0; L.ring = r.u8() != 0;
  for (uint8_t* b : {&L.people, &L.cut, &L.headwear, &L.pattern, &L.facePaint, &L.jewellery, &L.helmForm, &L.bodyForm, &L.shieldForm,
                     &L.bladeForm, &L.pauldron, &L.skirt, &L.crest, &L.polearmForm, &L.bowForm, &L.sheen, &L.glow})
    *b = r.u8();
  L.armsOrnament = r.u16();
  return !r.bad;
}
void writeMember(BinW& w, const Member& m) {
  w.u32(m.id); w.u8((uint8_t)m.origin); w.u8((uint8_t)m.voc); w.u8((uint8_t)m.state); w.u8((uint8_t)m.stance);
  w.u64(m.npc); w.u64(m.site); w.u16((uint16_t)m.res);
  w.str(m.name); w.u8(m.female ? 1 : 0); w.u8(m.age); w.u8(m.job); w.u64(m.culture); w.u8(m.people); w.u32(m.lookSeed); w.u8(m.species);
  w.u8((uint8_t)m.p.brave); w.u8((uint8_t)m.p.kind); w.u8((uint8_t)m.p.pious); w.u8((uint8_t)m.p.lawful); w.u32(m.p.likes); w.u32(m.p.dislikes);
  w.u8((uint8_t)m.stake.hook); w.u64(m.stake.site); w.u64(m.stake.kin); w.u64(m.stake.foe); w.u64(m.stake.kingdom); w.u64(m.stake.ruin);
  w.i32(m.stake.day); w.u32(m.stake.seed);
  w.u8((uint8_t)m.approval); w.u8(m.bond); w.u16(m.flags);
  w.f32(m.hp); w.f32(m.downT); w.i32(m.woundedUntil);
  w.u16((uint16_t)m.inv.size());
  for (const Item& it : m.inv) writeItem(w, it);
  for (int16_t e : m.eq) w.u16((uint16_t)e);
  w.u8(m.horse); w.i32(m.wage); w.i32(m.paidUntil);
  w.i32(m.gx); w.i32(m.gy); w.u64(m.mapId); w.u8(m.mapKind); w.u8(m.floor); w.u32(m.homePlot);
  w.i32(m.joinedDay); w.i32(m.lastDay); w.u32(m.days); w.u16(m.kills); w.u16(m.downs); w.u16(m.revives);
  w.u8((uint8_t)m.mem.size());
  for (const Memory& x : m.mem) { w.u8((uint8_t)x.deed); w.u8((uint8_t)x.delta); w.i32(x.day); w.u64(x.about); }
  w.str(m.quest); w.u32(m.questInst); w.u8(m.questState); w.u32(m.talkBits); w.i32(m.lastTalkDay); w.u32(m.lastLine);
  w.i32(m.leftDay); w.u64(m.hiredAt); w.u8(m.mercSlot); w.i32(m.mountedDay);   // (v2)
  w.u8(m.hasLook ? 1 : 0);   // (v3)
  if (m.hasLook) writeLook(w, m.look);
}
bool readMember(BinR& r, Member& m) {
  m = Member();
  m.id = r.u32();
  const uint8_t o = r.u8(), v = r.u8(), s = r.u8(), c = r.u8();
  if (o >= (uint8_t)Origin::COUNT || v >= (uint8_t)Vocation::COUNT || s >= (uint8_t)State::COUNT || c >= (uint8_t)Cmd::COUNT) return false;
  m.origin = (Origin)o; m.voc = (Vocation)v; m.state = (State)s; m.stance = (Cmd)c;
  m.npc = r.u64(); m.site = r.u64(); m.res = (int16_t)r.u16();
  m.name = r.str(); m.female = r.u8() != 0; m.age = r.u8(); m.job = r.u8(); m.culture = r.u64(); m.people = r.u8(); m.lookSeed = r.u32(); m.species = r.u8();
  m.p.brave = (int8_t)r.u8(); m.p.kind = (int8_t)r.u8(); m.p.pious = (int8_t)r.u8(); m.p.lawful = (int8_t)r.u8(); m.p.likes = r.u32(); m.p.dislikes = r.u32();
  const uint8_t hk = r.u8();
  if (hk >= (uint8_t)Hook::COUNT || m.job >= (uint8_t)life::Job::COUNT) return false;
  m.stake.hook = (Hook)hk; m.stake.site = r.u64(); m.stake.kin = r.u64(); m.stake.foe = r.u64(); m.stake.kingdom = r.u64(); m.stake.ruin = r.u64();
  m.stake.day = r.i32(); m.stake.seed = r.u32();
  m.approval = (int8_t)r.u8(); m.bond = r.u8(); m.flags = r.u16();
  if (m.approval < -100 || m.approval > 100 || m.bond > 100) return false;
  m.hp = r.f32(); m.downT = r.f32(); m.woundedUntil = r.i32();
  if (!std::isfinite(m.hp) || !std::isfinite(m.downT)) return false;
  const int ni = r.u16();
  if (ni > 200) return false;
  for (int i = 0; i < ni && !r.bad; i++) m.inv.push_back(readItem(r));
  for (int16_t& e : m.eq) { e = (int16_t)r.u16(); if (e < -1 || e >= (int)m.inv.size()) e = -1; }
  m.horse = r.u8(); m.wage = r.i32(); m.paidUntil = r.i32();
  m.gx = r.i32(); m.gy = r.i32(); m.mapId = r.u64(); m.mapKind = r.u8(); m.floor = r.u8(); m.homePlot = r.u32();
  m.joinedDay = r.i32(); m.lastDay = r.i32(); m.days = r.u32(); m.kills = r.u16(); m.downs = r.u16(); m.revives = r.u16();
  const int nm = r.u8();
  if (nm > MAX_MEMORIES) return false;
  for (int i = 0; i < nm && !r.bad; i++) {
    Memory x;
    const uint8_t d = r.u8();
    if (d >= (uint8_t)Deed::COUNT) return false;
    x.deed = (Deed)d; x.delta = (int8_t)r.u8(); x.day = r.i32(); x.about = r.u64();
    m.mem.push_back(x);
  }
  m.quest = r.str(); m.questInst = r.u32(); m.questState = r.u8(); m.talkBits = r.u32(); m.lastTalkDay = r.i32(); m.lastLine = r.u32();
  m.leftDay = r.i32(); m.hiredAt = r.u64(); m.mercSlot = r.u8(); m.mountedDay = r.i32();   // (v2)
  m.hasLook = r.u8() != 0;   // (v3)
  if (m.hasLook && !readLook(r, m.look)) return false;
  if (m.mercSlot >= MERC_MAX || m.downT < 0 || m.downT > DOWNED_SECS + 1 || m.hp > 1e6f) return false;
  return !r.bad;
}
void writeFallen(BinW& w, const Fallen& f) {
  w.u32(f.id); w.str(f.name); w.u8((uint8_t)f.voc); w.u8((uint8_t)f.origin); w.u64(f.culture); w.u64(f.npc); w.u64(f.site);
  w.i32(f.gx); w.i32(f.gy); w.i32(f.day); w.str(f.cause); w.u32(f.seed); w.u32(f.days); w.u16(f.kills); w.u8(f.bond); w.u8(f.graveFlags);
  w.u16((uint16_t)f.belongings.size());
  for (const Item& it : f.belongings) writeItem(w, it);
  w.u8(f.rite); w.u8(f.weapon); w.u8(f.species); w.u64(f.wasAt);   // (v2)
  w.u8(f.female ? 1 : 0);   // (v4)
}
bool readFallen(BinR& r, Fallen& f) {
  f = Fallen();
  f.id = r.u32(); f.name = r.str();
  const uint8_t v = r.u8(), o = r.u8();
  if (v >= (uint8_t)Vocation::COUNT || o >= (uint8_t)Origin::COUNT) return false;
  f.voc = (Vocation)v; f.origin = (Origin)o;
  f.culture = r.u64(); f.npc = r.u64(); f.site = r.u64();
  f.gx = r.i32(); f.gy = r.i32(); f.day = r.i32(); f.cause = r.str(); f.seed = r.u32(); f.days = r.u32(); f.kills = r.u16(); f.bond = r.u8(); f.graveFlags = r.u8();
  const int ni = r.u16();
  if (ni > 200) return false;
  for (int i = 0; i < ni && !r.bad; i++) f.belongings.push_back(readItem(r));
  f.rite = r.u8(); f.weapon = r.u8(); f.species = r.u8(); f.wasAt = r.u64();   // (v2)
  f.female = r.u8() != 0;   // (v4)
  if (f.rite >= 5 || (f.species != 0xFF && f.species > 1)) return false;
  return !r.bad;
}
}  // namespace

void Party::serialize(std::vector<uint8_t>& out) const {
  out.clear();
  BinW w(out);
  w.u8(VERSION);
  w.u32(nextId); w.i32(animalDay);
  // each record length-prefixed (a lane that grows a record later reads older ones by their own length)
  auto rec = [&](auto&& fn) {
    std::vector<uint8_t> b;
    BinW bw(b);
    fn(bw);
    w.u32((uint32_t)b.size());
    for (uint8_t c : b) w.u8(c);
  };
  w.u8((uint8_t)std::min<size_t>(members.size(), MAX_RECORDS));
  for (size_t i = 0; i < members.size() && i < (size_t)MAX_RECORDS; i++) rec([&](BinW& bw) { writeMember(bw, members[i]); });
  w.u16((uint16_t)std::min<size_t>(fallen.size(), MAX_FALLEN));
  for (size_t i = 0; i < fallen.size() && i < (size_t)MAX_FALLEN; i++) rec([&](BinW& bw) { writeFallen(bw, fallen[i]); });
}

bool Party::deserialize(const std::vector<uint8_t>& in) {
  BinR r(in);
  if (r.u8() != VERSION || r.bad) return false;
  Party P;
  P.nextId = r.u32(); P.animalDay = r.i32();
  auto rec = [&](auto&& fn) {
    const uint32_t n = r.u32();
    if (r.bad || n > in.size() - std::min(in.size(), r.p)) return false;
    std::vector<uint8_t> b(in.begin() + (std::ptrdiff_t)r.p, in.begin() + (std::ptrdiff_t)(r.p + n));
    r.p += n;
    BinR br(b);
    return fn(br) && br.p == b.size();
  };
  const int nm = r.u8();
  if (r.bad || nm > MAX_RECORDS) return false;
  for (int i = 0; i < nm; i++) {
    Member m;
    if (!rec([&](BinR& br) { return readMember(br, m); })) return false;
    if (m.state == State::Dead) continue;   // (a Dead member's record is the Fallen one)
    if (m.id == 0 || m.id >= P.nextId) return false;
    P.members.push_back(std::move(m));
  }
  const int nf = r.u16();
  if (r.bad || nf > MAX_FALLEN) return false;
  for (int i = 0; i < nf; i++) {
    Fallen f;
    if (!rec([&](BinR& br) { return readFallen(br, f); })) return false;
    P.fallen.push_back(std::move(f));
  }
  if (r.bad || r.p != in.size()) return false;
  *this = std::move(P);
  return true;
}

bool Party::operator==(const Party& o) const {
  std::vector<uint8_t> a, b;
  serialize(a);
  o.serialize(b);
  return a == b;
}

}  // namespace party
