// M8 Fellowship: the companions in the game (rpg/sim/party.h; Game's party hooks and API, game.h). FELLOWS lane.
// This file: the member records and their bodies, the hooks' bodies (partyStep, partyActor, partyFell, partyHit,
// partyKilled, partyAggro, partyQuest, partyRested, partyInteract), the deeds bus and approval, bond by the day, wages,
// downed -> bound -> back up or dead for good (a Fallen record, the census death and its grief), the orders, the paper
// doll, parting. Fighting is party_ai.cpp, moving party_move.cpp, recruiting party_recruit.cpp, graves party_graves.cpp.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_look.h"
#include "rpg/sim/home.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/power_ops.h"
#include "rpg/sim/realm.h"
#include "rpg/world/source.h"

using party::Member;
using party::State;

namespace {
int tileOf(float v) { return (int)std::floor(v / TILE); }
double msSince(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}
}  // namespace

// ================================================================ records and bodies
Member* PartyOps::of(Game& g, const Actor& a) {
  const int mi = (int)a.comp - 1;
  return mi >= 0 && mi < (int)g.party.members.size() ? &g.party.members[(size_t)mi] : nullptr;
}

ew::Gid PartyOps::homeland(const Game& g, const Member& m) {
  const int si = m.site ? g.world.siteHandle(m.site) : -1;
  if (si >= 0 && g.world.sites[(size_t)si].kingdom >= 0) return g.world.kingdoms[(size_t)g.world.sites[(size_t)si].kingdom].id;
  return m.stake.kingdom && m.stake.hook != party::Hook::Exile ? m.stake.kingdom : 0;
}

int PartyOps::actorOf(Game& g, int mi) {
  Member& m = g.party.members[(size_t)mi];
  if (m.actor < 0) return -1;
  const int i = g.findActor(m.actor);
  if (i <= 0 || g.actors[(size_t)i].comp != mi + 1) { m.actor = -1; return -1; }
  return i;
}

void PartyOps::removeMember(Game& g, int mi) {
  const int ai = actorOf(g, mi);
  if (ai > 0) g.actors[(size_t)ai].comp = 0;
  g.party.members.erase(g.party.members.begin() + mi);
  for (Actor& a : g.actors) if (a.comp > mi + 1) a.comp--;
}

void PartyOps::dropActor(Game& g, int mi) {
  const int ai = actorOf(g, mi);
  if (ai > 0) g.actors.erase(g.actors.begin() + ai);
  g.party.members[(size_t)mi].actor = -1;
  g.party.members[(size_t)mi].trailAt = -1;
}

life::Resident* PartyOps::resident(Game& g, const Member& m) {
  if (!m.site || m.res < 0) return nullptr;
  life::Census* c = g.life.findMut(m.site);
  return c && m.res < (int)c->res.size() ? &c->res[(size_t)m.res] : nullptr;
}

void PartyOps::alert(Game& g, int mi, int kind, const std::string& s) {
  const int ai = actorOf(g, mi);
  g.emit(Ev::PartyAlert, ai > 0 ? g.actors[(size_t)ai].p : g.pl().p, mi, (float)kind, s);
  g.events.back().id = g.party.members[(size_t)mi].id;
  if (kind != 4) {   // their own words with it (VOICE: rpg/sim/banter.cpp)
    const std::string cry = party::cryLine(g, g.party.members[(size_t)mi], kind);
    if (!cry.empty()) g.partySay(mi, cry, 3.0f);
  }
}

// a kit of their own people's make (the paper doll shows it in their culture's style)
void PartyOps::kit(Game& g, Member& m, int level) {
  if (m.voc == party::Vocation::Hound) return;
  Rng gr(hash32(m.lookSeed ^ 0x6EA2u) ^ (uint32_t)g.seed);
  const int il = std::clamp(level, 1, gear::MAX_D);
  // (M8b, 15.27.5 "companions use the same rules") each piece's rarity is rolled as a find of their level is
  // (LootSrc::Companion: the NUMBERS lane's gate; Common at low levels), its affixes and power made by makeGear
  gear::DropSource src;
  src.D = il; src.playerLevel = il; src.src = gear::LootSrc::Companion; src.culture = m.culture;
  auto put = [&](ItemKind k, int sub, int slot, Mat mat = Mat::None) {
    Rarity rar = gear::rollRarity(gr, src);
    // (M8b fix) a companion's kit rolls only affixes and powers that work for a companion (gear::CompanionRolls); a
    // Mythic of a kind with no companion-working Mythic power is made a Legendary (never a Mythic with a dead power)
    gear::CompanionRolls only;
    if (rar == Rarity::Mythic && gear::mythicPowersFor(k).empty()) rar = Rarity::Legendary;
    Item it = gear::makeGear(gr, k, sub, il, rar, m.culture, mat);
    m.inv.push_back(it);
    m.eq[(size_t)slot] = (int16_t)(m.inv.size() - 1);
  };
  switch (m.voc) {
    case party::Vocation::Warrior: put(ItemKind::Weapon, (int)WeaponType::Sword, party::S_WEAPON); put(ItemKind::Shield, 0, party::S_SHIELD); break;
    case party::Vocation::Archer: put(ItemKind::Bow, 0, party::S_BOW); put(ItemKind::Weapon, (int)WeaponType::Dagger, party::S_WEAPON); break;
    case party::Vocation::Mage: put(ItemKind::Staff, 0, party::S_STAFF); break;
    case party::Vocation::Healer: put(ItemKind::Weapon, (int)WeaponType::Mace, party::S_WEAPON); break;
    case party::Vocation::Rogue: put(ItemKind::Weapon, (int)WeaponType::Dagger, party::S_WEAPON); break;
    default: break;
  }
  // (fixer M8 r1, review: "two recruits from the same place look almost identical") the kit reads as the trade: a
  // warrior in iron with a helm, an archer or a rogue in leather under a travel cloak, a mage or a healer in cloth (their
  // own dress shows through); the same armour on everyone made a party of clones
  switch (m.voc) {
    case party::Vocation::Warrior: put(ItemKind::Armor, 0, party::S_ARMOR, Mat::Iron); put(ItemKind::Helmet, 0, party::S_HELMET, Mat::Iron); break;
    case party::Vocation::Archer: case party::Vocation::Rogue:
      put(ItemKind::Armor, 0, party::S_ARMOR, Mat::Leather); put(ItemKind::Cloak, 0, party::S_CLOAK); break;
    default: put(ItemKind::Boots, 0, party::S_BOOTS, Mat::Leather); break;   // (cloth folk: their own dress, good boots)
  }
}

// the member's body numbers with their gear (level-synced like the player's)
void PartyOps::body(Game& g, const Member& m, Actor& a) {
  const party::Body b = party::bodyFor(m, g.plLevel);
  const int L = party::levelFor(g.plLevel);
  float armor = b.armor, dmg = b.dmg;
  for (int s : {party::S_ARMOR, party::S_HELMET, party::S_SHIELD, party::S_GLOVES, party::S_BOOTS, party::S_CLOAK}) {
    const int e = m.eq[(size_t)s];
    if (e >= 0 && e < (int)m.inv.size()) armor += gear::usePower(m.inv[(size_t)e], L) * 0.5f;
  }
  // (M8b) their worn affixes and powers, by the player's rules: + health, Ironhide and "+N% ARMOUR"
  const gear::GearStats gs = PowerOps::memberStats(g, m);
  if (gs.has(Unique::Ironhide)) armor *= 1.25f;
  if (gs.get(Affix::ArmourPct) > 0) armor *= 1.0f + 0.01f * (float)gs.get(Affix::ArmourPct);
  const int w = m.voc == party::Vocation::Archer ? m.eq[party::S_BOW] : m.voc == party::Vocation::Mage ? m.eq[party::S_STAFF] : m.eq[party::S_WEAPON];
  const bool shoots = m.voc == party::Vocation::Archer || m.voc == party::Vocation::Mage;
  if (w >= 0 && w < (int)m.inv.size()) dmg = std::max(dmg, gear::usePower(m.inv[(size_t)w], L) * (shoots ? 0.7f : 0.85f));
  const float frac = a.maxHp > 0 ? a.hp / a.maxHp : 1.0f;
  a.maxHp = b.maxHp + (float)std::max(0, gs.get(Affix::Health)); a.dmg = dmg; a.armor = armor; a.speed = b.speed; a.range = b.range; a.aggroR = b.aggroR;
  if (gs.get(Affix::MoveSpeed) > 0 || gs.has(Unique::Windwalker))   // (as the hero's move speed)
    a.speed *= 1.0f + 0.01f * (float)gs.get(Affix::MoveSpeed) + (gs.has(Unique::Windwalker) ? 0.12f : 0.0f);
  a.level = L;
  a.hp = m.hp < 0 ? a.maxHp * std::clamp(frac, 0.0f, 1.0f) : std::min(m.hp, a.maxHp);
}

// a body standing inside something solid (put down on a spot whose tile is open but where a tree trunk, a boulder or a
// fence post overlaps the body's box) cannot walk out: every step from there is refused, and they stood wedged while the
// player fought on. The nearest place round it, out to a tile and a half, where the whole body is clear (else as it is).
static Vec2 unwedged(const Game& g, Vec2 p, float r) {
  if (HomeOps::bodyFree(g, p, r)) return p;
  for (int ring = 1; ring <= 12; ring++) {
    const float d = 2.0f * (float)ring;
    const Vec2 offs[8] = {Vec2(0, -d), Vec2(d, 0), Vec2(0, d), Vec2(-d, 0), Vec2(d, -d), Vec2(d, d), Vec2(-d, d), Vec2(-d, -d)};
    for (const Vec2& o : offs) if (HomeOps::bodyFree(g, p + o, r)) return p + o;
  }
  return p;
}

int PartyOps::spawn(Game& g, int mi, Vec2 at) {
  Member& m = g.party.members[(size_t)mi];
  Actor a;
  a.id = g.nextId_++;
  a.npc = true; a.hostile = false; a.faction = Faction::Player;
  a.human = m.voc != party::Vocation::Hound;
  if (!a.human) a.mon = art::Monster::Wolf;   // (VIEW draws a companion hound from Game::partyHoundLook)
  a.comp = (uint8_t)(mi + 1);
  a.p = at; a.home = at; a.goal = at;
  a.site = -1; a.slot = -1; a.bldg = -1; a.fromMap = false; a.role = Role::Villager; a.name = m.name;
  a.radius = a.human ? 4.5f : 4.0f; a.windup = a.human ? 0.28f : 0.22f;
  at = unwedged(g, at, a.radius);
  a.p = at; a.home = at; a.goal = at;
  a.culture = m.culture;
  fixLook(g, m);
  if (a.human) a.look = g.partyLook(m);
  a.maxHp = 0;
  body(g, m, a);
  if (m.hp < 0) a.hp = a.maxHp;
  a.face = g.pl().face;
  if (m.state == State::Downed) { a.st = AState::Idle; a.posture = art::Posture::Downed; a.hp = 0; }
  g.actors.push_back(a);
  m.actor = a.id;
  m.inPlay = true;
  m.trailAt = -1; m.stuckT = 0; m.lastPos = at;
  return a.id;
}

// ================================================================ death, parting
void PartyOps::dieForGood(Game& g, int mi, const std::string& cause) {
  Member& m = g.party.members[(size_t)mi];
  const int ai = actorOf(g, mi);
  party::Fallen f;
  f.id = m.id; f.name = m.name; f.voc = m.voc; f.origin = m.origin; f.culture = m.culture; f.npc = m.npc; f.site = m.site;
  f.female = m.female;
  // where they fell, on the overworld (inside: the door or the cave mouth the player came in by)
  int ox = 0, oy = 0;
  overworldTile(g, ox, oy);
  if (!g.inside && ai > 0) { ox = tileOf(g.actors[(size_t)ai].p.x); oy = tileOf(g.actors[(size_t)ai].p.y); }
  f.gx = g.world.ox + ox; f.gy = g.world.oy + oy;
  if (g.inside) f.wasAt = g.subSite >= 0 ? g.world.sites[(size_t)g.subSite].id : (g.subBldg >= 0 && g.world.over.bldgs[(size_t)g.subBldg].site >= 0 ? g.world.sites[(size_t)g.world.over.bldgs[(size_t)g.subBldg].site].id : 0);
  else { const int si = g.settlementAt(g.pl().p); f.wasAt = si >= 0 ? g.world.sites[(size_t)si].id : 0; }
  // (fixer M8 r1) a member who died in their census life (at home, left, a raid): they lie in their own place
  life::Resident* res = resident(g, m);
  const bool censusDeath = res && (res->flags & life::RF_DEAD);
  if (censusDeath) {
    const int hs = g.world.siteHandle(m.site);
    if (hs >= 0) { const Site& S = g.world.sites[(size_t)hs]; f.gx = g.world.ox + S.ex; f.gy = g.world.oy + S.ey; f.wasAt = S.id; }
  }
  f.day = g.day; f.cause = cause; f.seed = hash32(m.id * 2654435761u ^ (uint32_t)g.day);
  f.days = m.days; f.kills = m.kills; f.bond = m.bond;
  f.belongings = m.inv;
  if (m.voc == party::Vocation::Hound) { f.species = m.species; f.rite = 1; f.weapon = 0; }
  else {
    f.species = 0xFF;
    f.rite = (uint8_t)(hash32((uint32_t)m.culture ^ (uint32_t)(m.culture >> 32) ^ 0x6A7Eu) % (uint32_t)art::GRAVE_RITES);
    f.weapon = g.partyLook(m).weapon;
  }
  // left to die: the player was far off and not binding the wounds (the others remember it)
  const Vec2 at = ai > 0 ? g.actors[(size_t)ai].p : g.pl().p;
  const bool abandoned = m.state == State::Downed && m.reviveT <= 0 && len(at - g.pl().p) > 10.0f * TILE;
  siteGrave(g, f);
  g.party.fallen.push_back(f);
  if ((int)g.party.fallen.size() > party::MAX_FALLEN) g.party.fallen.erase(g.party.fallen.begin());
  g.party.graveDirty = true;
  if (res) {
    res->flags = (uint16_t)(res->flags & ~(life::RF_COMPANION | life::RF_AWAY));
    if (!censusDeath) g.life.residentDied(m.site, m.res, g.day);   // kin and friends grieve (a census death has already)
  }
  if (ai > 0) {
    Actor& a = g.actors[(size_t)ai];
    a.hp = 0; a.st = AState::Dead; a.stT = 0; a.posture = art::Posture::Downed;   // (fixer M8 r1) they lie where they fell
  }
  if (g.pl().posture == art::Posture::Kneel) g.pl().posture = art::Posture::None;
  g.sfx((int)Sfx::Knell, at, 0.9f);
  alert(g, mi, 4, m.name + " IS DEAD.");
  g.party.stats.deaths++;
  // (fixer M8 r3) a sellsword's inn slot is theirs for good: the same seed would stand the dead man at his table again
  if (m.origin == party::Origin::Mercenary && m.hiredAt) g.marks[markKey(m.hiredAt * 4 + m.mercSlot, (Mk)MK_HIRED)] = MERC_FALLEN;
  removeMember(g, mi);
  if (abandoned) { g.party.stats.abandoned++; g.deed(party::Deed::Abandoned, f.site, 2); }
}

void PartyOps::leave(Game& g, int mi, bool angry) {
  Member& m = g.party.members[(size_t)mi];
  const int ai = actorOf(g, mi);
  if (ai > 0) {   // the body walks off home, out of sight, before it goes
    Actor& a = g.actors[(size_t)ai];
    a.comp = 0; a.partyTag = PT_WALKOFF; a.target = -1; a.st = AState::Idle; a.posture = art::Posture::None;
    a.lifeT = 0; a.navNext = -1;
    g.party.walkOffs++;
  }
  m.actor = -1;
  m.leftDay = g.day;
  if (life::Resident* r = resident(g, m)) r->flags = (uint16_t)(r->flags & ~life::RF_AWAY);   // home again (RF_COMPANION kept)
  if (angry) {
    m.state = State::Left;
    m.flags |= party::MF_LEFT_ONCE;
  }
}

// (fixer M8 r1) a member back in their census life (at home, left, waiting near their street) who died there (a raid,
// a beast, any hand: lifeKill -> Life::residentDied marks RF_DEAD) is dead for good: a Fallen record and a grave, never
// a body that can be called back to follow the player while their kin grieve
void PartyOps::censusDeaths(Game& g) {
  for (int mi = (int)g.party.members.size() - 1; mi >= 0; mi--) {
    const Member& m = g.party.members[(size_t)mi];
    const life::Resident* r = resident(g, m);
    if (!r || !(r->flags & life::RF_DEAD)) continue;
    dieForGood(g, mi, m.state == State::Home ? "DIED AT HOME" : "DIED FAR FROM YOU");
  }
}

// ================================================================ the deeds bus
void PartyOps::applyDeed(Game& g, const party::Party::Pending& d) {
  int loud = -1, loudDelta = 0;   // the member it moved most (they speak of it)
  for (size_t i = 0; i < g.party.members.size(); i++) {
    Member& m = g.party.members[i];
    if (m.state != State::With && m.state != State::Downed) continue;
    if (m.voc == party::Vocation::Hound) continue;   // the animal judges nobody
    if (m.origin == party::Origin::Mercenary) {
      // a hireling minds only what touches their pay or their neck
      if (d.d != party::Deed::KilledInnocent && d.d != party::Deed::Abandoned && d.d != party::Deed::Cruelty) continue;
    }
    const int delta = std::clamp(party::approvalDelta(m, d.d, d.about, homeland(g, m)) * std::max(1, d.weight), -15, 15);
    if (!delta) continue;
    const int before = m.approval;
    m.approval = (int8_t)std::clamp(before + delta, -100, 100);
    if (std::abs(delta) > std::abs(loudDelta)) { loud = (int)i; loudDelta = delta; }
    if (std::abs(delta) >= 2 || (party::deedInfo(d.d).flags & party::DF_LOUD)) {
      // the same deed about the same thing the same day is one memory (no spam: a pack of bandits is one fight)
      bool merged = false;
      for (party::Memory& x : m.mem)
        if (x.deed == d.d && x.about == d.about && x.day == g.day) { x.delta = (int8_t)std::clamp((int)x.delta + delta, -100, 100); merged = true; break; }
      if (!merged) {
        m.mem.push_back({d.d, (int8_t)delta, g.day, d.about});
        if ((int)m.mem.size() > party::MAX_MEMORIES) m.mem.erase(m.mem.begin());
      }
    }
    if (m.approval < party::APPROVAL_COMPLAIN && before >= party::APPROVAL_COMPLAIN && !(m.flags & party::MF_COMPLAINED)) {
      m.flags |= party::MF_COMPLAINED;
      alert(g, (int)i, 7, m.name + " IS UNHAPPY WITH YOU.");
    }
    if (m.approval >= party::APPROVAL_COMPLAIN) m.flags = (uint16_t)(m.flags & ~party::MF_COMPLAINED);
  }
  g.party.stats.deeds++;
  if (loud >= 0 && std::abs(loudDelta) >= 4) {
    const std::string line = party::deedLine(g, g.party.members[(size_t)loud], d.d, loudDelta);
    if (!line.empty()) g.partySay(loud, line);
  }
  // those who can bear it no longer leave (to their home: recruitable again after amends, VOICE)
  for (int i = (int)g.party.members.size() - 1; i >= 0; i--) {
    Member& m = g.party.members[(size_t)i];
    if (m.state == State::With && m.approval < party::APPROVAL_LEAVE) {
      alert(g, i, 6, m.name + " HAS LEFT YOU.");
      leave(g, i, true);
      // a sellsword simply goes; so does (fixer M8 r2) a stranger with no census home to be found at (a saga's ally,
      // someone rescued, a sworn one: npc 0), whose record could never be made amends with nor parted from
      if (m.origin == party::Origin::Mercenary || m.res < 0) removeMember(g, i);
    }
  }
}

// ================================================================ bond by the day, wages, growing up
void PartyOps::daily(Game& g) {
  for (int mi = (int)g.party.members.size() - 1; mi >= 0; mi--) {
    Member& m = g.party.members[(size_t)mi];
    if (m.lastDay >= g.day) continue;
    const int days = g.day - m.lastDay;
    m.lastDay = g.day;
    // a pup grows up on its own day count, with the player or not
    if ((m.flags & party::MF_PUP) && g.day - m.joinedDay >= party::PUP_DAYS) {
      m.flags = (uint16_t)(m.flags & ~party::MF_PUP);
      const int ai = actorOf(g, mi);
      if (ai > 0) body(g, m, g.actors[(size_t)ai]);
    }
    if ((m.flags & party::MF_WOUNDED) && g.day > m.woundedUntil) m.flags = (uint16_t)(m.flags & ~party::MF_WOUNDED);
    if (m.state != State::With && m.state != State::Downed) continue;
    const uint8_t had = party::unlockedSkills(m);
    const int gain = days * party::BOND_PER_DAY + (m.approval >= party::APPROVAL_QUEST ? days : 0);
    m.bond = (uint8_t)std::clamp((int)m.bond + (m.approval < 0 ? 0 : gain), 0, 100);
    m.days += (uint32_t)days;
    const uint8_t now = party::unlockedSkills(m);
    for (int k = 0; k < 3; k++)
      if ((now & (1u << k)) && !(had & (1u << k)))
        alert(g, mi, 8, m.name + " HAS LEARNED " + party::skillInfo(party::skillsOf(m.voc)[(size_t)k]).name + ".");
    // (fixer M8 r1) a sellsword bleeding out is not paid off mid-dying: the wage waits until they are up (or dead)
    if (m.origin == party::Origin::Mercenary && m.wage > 0 && m.paidUntil < g.day && m.state != State::Downed) {
      const int owed = m.wage * std::max(1, g.day - m.paidUntil);
      if (g.gold >= owed) { g.gold -= owed; m.paidUntil = g.day; g.say("YOU PAY " + m.name + " " + std::to_string(owed) + " GOLD."); }
      else { alert(g, mi, 6, m.name + " WILL NOT FIGHT UNPAID."); g.partyDismiss(mi); }
    }
  }
}

// ================================================================ downed: bleeding out, binding the wounds
void PartyOps::revive(Game& g, int mi, Actor& a, bool fromAfar) {
  Member& m = g.party.members[(size_t)mi];
  // (fixer M8 r2) one downed while waiting or at home gets back up to that life, not into the party
  m.state = (m.flags & party::MF_DOWN_WAIT) ? State::Waiting : (m.flags & party::MF_DOWN_HOME) ? State::Home : State::With;
  m.reviveT = 0; m.downT = 0;
  m.flags = (uint16_t)(m.flags & ~(party::MF_CARRIED | party::MF_DOWN_WAIT | party::MF_DOWN_HOME));
  a.st = AState::Idle; a.stT = 0; a.posture = art::Posture::None;
  a.hp = a.maxHp * (float)party::REVIVE_HP_PCT / 100.0f;
  m.hp = a.hp;
  m.flags |= party::MF_WOUNDED;
  m.woundedUntil = g.day + 1;
  m.revives++;
  m.lowCryT = 8.0f;
  g.party.stats.revives++;
  g.sfx((int)Sfx::Revive, a.p);
  g.emit(Ev::Heal, a.p);
  (void)fromAfar;
  alert(g, mi, 3, m.name + " IS BACK ON THEIR FEET.");
}

void PartyOps::downedStep(Game& g, float dt) {
  Actor& p = g.pl();
  bool binding = false;
  for (int mi = 0; mi < (int)g.party.members.size(); mi++) {
    Member& m = g.party.members[(size_t)mi];
    if (m.state != State::Downed) continue;
    const int ai = actorOf(g, mi);
    if (ai < 0) continue;   // (keepUp brings them in first)
    Actor& a = g.actors[(size_t)ai];
    m.downT -= dt;
    if (m.reviveT > 0) {
      const bool near = len2(a.p - p.p) < (1.8f * TILE) * (1.8f * TILE);
      const bool still = p.st == AState::Idle || p.st == AState::Hurt;
      if (near && still && g.time - g.lastHurtT > 0.05f && p.st != AState::Dead) {
        m.reviveT += dt;
        binding = true;
        p.face = faceOf(a.p - p.p);
        if (m.reviveT >= party::REVIVE_SECS) { revive(g, mi, a, false); continue; }
      } else m.reviveT = 0;   // stepped away or struck: the binding stops
    }
    if (m.downT <= 0) { dieForGood(g, mi, "BLED OUT"); mi--; continue; }
  }
  // the player kneels while binding (art::Posture::Kneel; VIEW draws the player's posture like anyone's)
  if (binding) { if (p.st == AState::Idle) p.posture = art::Posture::Kneel; }
  else if (p.posture == art::Posture::Kneel) p.posture = art::Posture::None;
}

// ================================================================ a companion's body each step
bool PartyOps::actorStep(Game& g, Actor& a, float dt) {
  Member* mp = of(g, a);
  if (!mp) { a.comp = 0; return false; }
  const auto t0 = std::chrono::steady_clock::now();
  Member& m = *mp;
  const int mi = indexOf(g, m);
  m.actor = a.id;
  m.inPlay = true;
  const Actor& p = g.pl();
  for (float& c : m.skillCd) if (c > 0) c -= dt;
  if (m.tauntT > 0) m.tauntT -= dt;
  if (m.potionCd > 0) m.potionCd -= dt;
  if (m.eagleT > 0) m.eagleT -= dt;
  if (m.guardT > 0) m.guardT -= dt;
  if (m.state == State::Downed) {
    a.st = AState::Idle; a.posture = art::Posture::Downed; a.knock = Vec2(); a.hp = 0; a.target = -1;
    g.party.actorMs += msSince(t0);
    return true;
  }
  if (a.posture == art::Posture::Downed || (a.posture == art::Posture::Ride && !g.homeRiding())) a.posture = art::Posture::None;
  if (m.state == State::Waiting) {
    a.st = AState::Idle;
    if (len2(p.p - a.p) < (6.0f * TILE) * (6.0f * TILE)) a.face = faceOf(p.p - a.p);
    g.party.actorMs += msSince(t0);
    return true;
  }
  if (m.state == State::Home) { homeLife(g, a, m, dt); g.party.actorMs += msSince(t0); return true; }
  if (m.state != State::With) { g.party.actorMs += msSince(t0); return true; }
  if (a.posture != art::Posture::Ride) a.posture = art::Posture::None;
  a.useX = a.useY = -1;
  // (wedged in a trunk or a post by a placement that only asked for an open tile: out to the nearest clear place)
  if (!HomeOps::bodyFree(g, a.p, a.radius)) { a.p = unwedged(g, a.p, a.radius); a.navNext = -1; }
  // Defend: braced at the player's side, a blow's shove dies out at once (it carried them off the leash while the
  // player moved on)
  if (m.stance == party::Cmd::Defend) a.knock = a.knock * 0.5f;
  m.hp = a.hp >= a.maxHp ? -1.0f : a.hp;
  // Last Stand: fighting on at 1 HP until the time runs out, then down
  if (m.lastStandT > 0) {
    m.lastStandT -= dt;
    a.hp = std::max(a.hp, 1.0f);
    if (m.lastStandT <= 0) { a.hp = 0; g.partyFell(a, -1); g.party.actorMs += msSince(t0); return true; }
  }
  // the low-health cry (danger feedback, 15.6.2)
  if (m.lowCryT > 0) m.lowCryT -= dt;
  if (a.hp < a.maxHp * (float)party::LOW_HP_PCT / 100.0f && m.lowCryT <= 0) {
    m.lowCryT = 20.0f;
    alert(g, mi, 1, m.name + " IS BADLY HURT.");
  }
  potion(g, a, m);
  // (fixer M8 r1) Defend: a swing or a shot being drawn while the player has run on is let go, back to their side
  // (the blow already landed in Strike: only its follow-through is cut short)
  if (m.stance == party::Cmd::Defend && (a.st == AState::Windup || a.st == AState::Strike || a.st == AState::Recover) &&
      len2(a.p - p.p) > (2.4f * TILE) * (2.4f * TILE)) { a.st = AState::Idle; a.stT = 0; }
  // the attack cycle (windup, strike, recover)
  if (a.st == AState::Windup) {
    if (a.stT >= a.windup) { a.st = AState::Strike; a.stT = 0; strike(g, a, m); }
    g.party.actorMs += msSince(t0);
    return true;
  }
  if (a.st == AState::Strike) { if (a.stT > 0.18f) { a.st = AState::Recover; a.stT = 0; } g.party.actorMs += msSince(t0); return true; }
  if (a.st == AState::Recover) { if (a.stT > 0.2f) { a.st = AState::Idle; a.stT = 0; } g.party.actorMs += msSince(t0); return true; }
  if (a.st == AState::Hurt) { if (a.stT > 0.22f) { a.st = AState::Idle; a.stT = 0; } g.party.actorMs += msSince(t0); return true; }
  heal(g, a, m, mi);
  autoSkills(g, a, m, mi);
  if (!fight(g, a, m, mi, dt)) follow(g, a, m, mi, dt);
  g.party.actorMs += msSince(t0);
  return true;
}

// ================================================================ the hooks
void Game::partyStep(float dt) {
  const int mk = mapKey();
  if (mk != party.mapSeen) { party.mapSeen = mk; party.mapChanges++; PartyOps::mapChanged(*this); }
  if (party.members.empty() && party.pending.empty() && party.fallen.empty() && party.walkOffs == 0) { party.actorMs = 0; return; }
  const auto t0 = std::chrono::steady_clock::now();
  party.banterT += dt;
  if (party.shieldWallT > 0) party.shieldWallT -= dt;
  if (party.smokeT > 0) party.smokeT -= dt;
  { const float keep = std::max(0.0f, 1.0f - dt / 30.0f); party.recentPl *= keep; party.recentParty *= keep; }
  if (!party.pending.empty()) {
    const std::vector<party::Party::Pending> todo = party.pending;   // (applying may raise more: Abandoned)
    party.pending.clear();
    for (const party::Party::Pending& d : todo) PartyOps::applyDeed(*this, d);
  }
  PartyOps::daily(*this);
  PartyOps::trailStep(*this);
  PartyOps::keepUp(*this, dt);
  PartyOps::downedStep(*this, dt);
  party.slowT -= dt;
  if (party.slowT <= 0) {
    party.slowT = 0.25f;
    PartyOps::censusDeaths(*this);
    PartyOps::placesStep(*this);
    PartyOps::gravesStep(*this);
  }
  // bodies walking home out of sight (left, dismissed): gone once off the screen or out of the door
  if (party.walkOffs > 0) {
    int left = 0;
    for (size_t i = 1; i < actors.size();) {
      Actor& a = actors[i];
      if (a.partyTag != PartyOps::PT_WALKOFF) { i++; continue; }
      const Vec2 d = a.p - pl().p;
      const bool gone = std::fabs(d.x) > (sleepHalfW + 1.0f) * TILE || std::fabs(d.y) > (sleepHalfH + 1.0f) * TILE || (inside && a.lifeT > 3.0f) || a.lifeT > 40.0f;
      if (gone) { actors.erase(actors.begin() + (std::ptrdiff_t)i); continue; }
      left++;
      i++;
    }
    party.walkOffs = left;
  }
  const double mine = msSince(t0);
  const auto tb = std::chrono::steady_clock::now();
  party::banterStep(*this, dt);   // VOICE (rpg/sim/banter.cpp)
  party.stats.banterMsWorst = std::max(party.stats.banterMsWorst, msSince(tb));
  const double ms = mine + party.actorMs;   // (the FELLOWS lane's own cost: banter is timed apart)
  party.actorMs = 0;
  if (!party.members.empty()) {
    party.stats.stepMsSum += ms;
    party.stats.stepMsWorst = std::max(party.stats.stepMsWorst, ms);
    party.stats.stepN++;
  }
}

bool Game::partyActor(Actor& a, float dt) { return PartyOps::actorStep(*this, a, dt); }

bool Game::partyFell(Actor& a, int killer) {
  Member* m = PartyOps::of(*this, a);
  if (!m) return false;
  (void)killer;
  const int mi = PartyOps::indexOf(*this, *m);
  if (m->state == State::Downed) {   // struck while down: the bleeding quickens; a heavy blow ends it
    // (fixer M8 r2) only a fresh blow (partyHit sets lastBlow just before it lands) counts, once: a burn, a stale
    // heavy blow or any other non-blow kill() on the downed body no longer drains the window frame by frame
    a.hp = 0; a.burnT = 0; a.slowT = 0;
    if (m->lastBlow <= 0 && !m->lastBlowHeavy) return true;
    const bool heavy = m->lastBlowHeavy || m->lastBlow >= party::HEAVY_BLOW_PCT * std::max(1.0f, a.maxHp);
    m->lastBlow = 0; m->lastBlowHeavy = false;
    if (heavy) {
      PartyOps::dieForGood(*this, mi, "STRUCK DOWN WHERE THEY LAY");
      return true;
    }
    m->downT -= 5.0f;
    return true;
  }
  // Last Stand (the warrior's third bond skill): they fight on for five seconds more
  if (m->lastStandT > 0) { a.hp = 1; return true; }
  if (m->voc == party::Vocation::Warrior && (party::unlockedSkills(*m) & 4) && m->skillCd[2] <= 0 && m->state == State::With) {
    m->skillCd[2] = party::skillInfo(party::Skill::LastStand).cooldown;
    m->lastStandT = 5.0f;
    a.hp = 1;
    party.stats.lastStands++;
    emit(Ev::Text, a.p + Vec2(0, -30), (int)rgba(255, 200, 120), 0, "LAST STAND");
    sfx((int)Sfx::Roar, a.p, 1.4f, 0.6f);
    return true;
  }
  // (fixer M8 r2) one told to wait, or living at home, is downed where they are and gets back up to that life (not
  // into the party past its capacity): MF_DOWN_WAIT / MF_DOWN_HOME remember it (PartyOps::revive)
  uint16_t from = 0;
  if (m->state == State::Waiting) from = party::MF_DOWN_WAIT;
  else if (m->state == State::Home) from = party::MF_DOWN_HOME;
  m->state = State::Downed;
  m->downT = party::DOWNED_SECS;
  m->reviveT = 0;
  m->downs++;
  m->target = -1;
  m->tauntT = 0;
  m->lastBlow = 0; m->lastBlowHeavy = false;   // (fixer M8 r2) the blow that downed them is spent
  m->flags = (uint16_t)((m->flags & ~(party::MF_CARRIED | party::MF_DOWN_WAIT | party::MF_DOWN_HOME)) | from);
  a.hp = 0; a.st = AState::Idle; a.stT = 0; a.posture = art::Posture::Downed; a.knock = Vec2();
  a.burnT = 0; a.slowT = 0;   // (fixer M8 r2) a burning companion stops burning when they fall (else they bled out in 0.15 s)
  party.stats.downs++;
  PartyOps::alert(*this, mi, 2, m->name + " IS DOWN! GET TO THEM!");
  sfx((int)Sfx::Hurt, a.p, 0.8f);
  return true;
}

float Game::partyHit(Actor& victim, float dmg, int attacker) {
  if (party.members.empty() || dmg <= 0) return dmg;
  const int atkI = attacker >= 0 ? findActor(attacker) : -1;
  if (attacker == pl().id && victim.hostile) { party.playerTarget = victim.id; party.playerTargetT = time; }
  if (victim.player || victim.comp) {
    if (party.shieldWallT > 0) dmg *= 0.5f;   // Shield Wall
    if (victim.player && party.wardHp > 0) {   // the mage's Ward soaks first
      const float soak = std::min(party.wardHp, dmg);
      party.wardHp -= soak;
      dmg -= soak;
      if (soak > 0) emit(Ev::Sparkle, victim.p + Vec2(0, -8));
    }
  }
  if (victim.comp) {
    if (Member* m = PartyOps::of(*this, victim)) {
      m->lastBlow = dmg;
      m->lastBlowHeavy = atkI > 0 && actors[(size_t)atkI].heavy;
      if (m->stance == party::Cmd::Defend && m->voc == party::Vocation::Warrior) dmg *= 0.85f;   // braced behind the shield
    }
  }
  if (victim.player) { party.stats.dmgPlayer += dmg; party.recentPl += dmg; }
  else if (victim.comp && victim.posture != art::Posture::Downed) { party.stats.dmgParty += dmg; party.recentParty += dmg; }   // (a blow on the downed costs time, not health)
  if (attacker >= 0 && victim.hostile) {
    if (attacker == pl().id) party.stats.dealtPlayer += dmg;
    else if (party.indexOfActor(attacker) >= 0) party.stats.dealtParty += dmg;
  }
  return dmg;
}

void Game::partyKilled(const Actor& v, int killer) {
  if (party.members.empty() || v.comp) return;
  const int ci = party.indexOfActor(killer);
  if (ci >= 0) party.members[(size_t)ci].kills++;
  if (killer != pl().id && ci < 0) return;
  using party::Deed;
  const ew::Gid siteId = v.site >= 0 && v.site < (int)world.sites.size() ? world.sites[(size_t)v.site].id : 0;
  const bool byPlayer = killer == pl().id;
  // (fixer M8 r2) a man of a siege's armies, at that siege (its town or the camp 14-30 tiles out): a soldier there is
  // in battle, and so is the watch of a town the player besieges with the attackers. Killing them is the war the player
  // chose (Deed::JoinedWar weighed that once), not the crime of KILLED A GUARD over and over.
  auto inBattle = [&]() {
    if (!v.realm) return false;
    for (const realm::Siege& s : realm.sieges()) {
      if (s.over || (s.attacker != v.realm && s.defender != v.realm)) continue;
      if (v.role == Role::Guard && !(s.playerSide == 1 && s.defender == v.realm)) continue;
      const int si = world.siteHandle(s.site);
      if (si < 0) continue;
      const Site& S = world.sites[(size_t)si];
      if (std::hypot(S.ex - v.p.x / TILE, S.ey - v.p.y / TILE) < std::max(S.r.w, S.r.h) * 0.5f + 46) return true;
    }
    return false;
  };
  if (v.npc && v.human && !v.hostile) {
    if (!byPlayer) return;   // (a companion never strikes townsfolk unless the war makes them foes)
    if (v.role == Role::Guard || v.role == Role::Soldier || v.role == Role::Captain) { if (!inBattle()) deed(Deed::KilledGuard, v.realm); }
    else deed(Deed::KilledInnocent, siteId, 2);
  } else if (v.faction == Faction::Bandit) deed(Deed::ClearedBandits, siteId);
  else if (v.unique || v.mon == art::Monster::Dragon) deed(Deed::SlewMonster, v.unique);
  else if (v.hostile && v.human && v.realm && (v.role == Role::Soldier || v.role == Role::Captain || v.role == Role::Guard)) {
    // (fixer M8 r2) a kingdom's man-at-arms who came at the player: in a war (a besieger, the side the player fights)
    // that is battle, not a crime; the side taken already weighed once (Deed::JoinedWar, war_game.cpp). Only a watch
    // turned on the player with no war behind it (a caught thief's pursuers) is a guard killed.
    bool warring = false;
    for (const realm::War& w : realm.wars()) if (!w.endDay && (w.attacker == v.realm || w.defender == v.realm)) warring = true;
    if (!warring && v.role == Role::Guard && !inBattle()) deed(Deed::KilledGuard, v.realm);
  } else if (v.hostile && (v.rank >= 2 || v.boss || settlementAt(v.p) >= 0)) deed(Deed::SlewBeast, siteId);
}

float Game::partyAggro(const Actor& hostile, const Actor& member) const {
  const int mi = (int)member.comp - 1;
  if (mi < 0 || mi >= (int)party.members.size()) return 1.0f;
  const Member& m = party.members[(size_t)mi];
  if (m.state == State::Downed) return 12.0f;   // the downed: finished off only when nothing else is near
  if (m.state != State::With) return 1.6f;
  if (party.smokeT > 0) return 0.0f;
  if (m.tauntT > 0) return 0.22f;   // Taunt
  if (m.stance == party::Cmd::Passive) return 6.0f;   // holding back: only what has nothing else near
  // a hurt companion has fallen back behind the player (party_ai.cpp): the foes go for the one in front
  float hurt = member.maxHp > 0 && member.hp < member.maxHp * 0.35f && m.lastStandT <= 0 ? 1.8f : 1.0f;
  // the pack leans toward whoever has been spared lately (Party::recentPl / recentParty): the party drawn less the more
  // of the last blows it took, more when the player took nearly all of them (no lean before a few blows have landed)
  // (M8b merge, --companion --seeds 1..20) a firmer lean (x14 in 0.4..2.0, was x8 in 0.5..1.8): the old one settled
  // ~2.5 points under the middle with a +-4 spread, so a kiting archer at D25 (seed 17: 24 %) fell out of the band
  // whenever the fights' lengths moved (the M8b damage numbers moved them; the mean over 60 runs did not move)
  const float seen = party.recentPl + party.recentParty;
  if (seen > 1.0f) {
    const float share = party.recentParty / seen, mid = (float)(party::SHARE_LO_PCT + party::SHARE_HI_PCT) * 0.005f;
    hurt *= std::clamp(1.0f + (share - mid) * 14.0f, 0.4f, 2.0f);
  }
  switch (m.voc) {
    case party::Vocation::Warrior: return hurt * (m.stance == party::Cmd::Defend ? 0.42f : 0.47f);   // in front, the shield up
    case party::Vocation::Hound: return hurt * 0.61f;
    case party::Vocation::Rogue: return hurt * 0.5f;
    case party::Vocation::Healer: return hurt * 0.65f;
    case party::Vocation::Archer: return hurt * 0.27f;   // soft, alone at the edge of the fight: the pack's favourite
    default: return hurt * 0.58f;   // the mage
  }
}

void Game::partyQuest(const Quest& q, bool success) {
  if (party.members.empty()) return;
  using party::Deed;
  if (!success) { deed(Deed::QuestFailed, q.giverId); return; }
  switch (q.type) {
    case QType::Missing: deed(Deed::Rescued, q.giverId); break;
    case QType::Protect: case QType::War: deed(Deed::DefendedTown, q.giverId); break;
    case QType::Bounty: case QType::NamedBandit: deed(Deed::ClearedBandits, q.giverId); break;
    case QType::Supply: deed(Deed::Charity, q.giverId); break;
    default: deed(Deed::QuestDone, q.giverId); break;
  }
}

void Game::partyRested(int hours) {
  for (size_t mi = 0; mi < party.members.size(); mi++) {
    Member& m = party.members[mi];
    if (m.state != State::With && m.state != State::Home && m.state != State::Waiting) continue;
    m.hp = -1;
    if (hours >= 6 && (m.flags & party::MF_WOUNDED)) m.flags = (uint16_t)(m.flags & ~party::MF_WOUNDED);
    const int ai = PartyOps::actorOf(*this, (int)mi);
    if (ai > 0) { PartyOps::body(*this, m, actors[(size_t)ai]); actors[(size_t)ai].hp = actors[(size_t)ai].maxHp; }
  }
}

bool Game::partyInteract() {
  if (!inside && PartyOps::denInteract(*this)) return true;   // a cleared wolf den's pup
  if (party.members.empty() && party.fallen.empty()) return false;
  const Actor& p = pl();
  // a downed companion first (the danger), then a grave
  for (size_t mi = 0; mi < party.members.size(); mi++) {
    Member& m = party.members[mi];
    if (m.state != State::Downed) continue;
    const int ai = PartyOps::actorOf(*this, (int)mi);
    if (ai <= 0 || len2(actors[(size_t)ai].p - p.p) > (1.8f * TILE) * (1.8f * TILE)) continue;
    if (m.reviveT <= 0) { m.reviveT = 0.001f; say("YOU BIND " + m.name + "'S WOUNDS..."); }
    return true;
  }
  if (!inside) {
    const int fi = PartyOps::graveNear(*this, p.p + faceVec(p.face) * 6.0f, 1.4f * TILE);
    if (fi >= 0) { partyGraveTalk(fi); return true; }
  }
  return false;
}

bool Game::partyFolk(Actor& a, float dt) { return PartyOps::folk(*this, a, dt); }

// ================================================================ the API
void Game::deed(party::Deed d, ew::Gid about, int weight) {
  if (party.members.empty()) return;
  // (no spam) the same deed about the same thing already queued this step is one deed
  for (const party::Party::Pending& p : party.pending) if (p.d == d && p.about == about) return;
  party.pending.push_back({d, about, weight});
}

bool Game::partyGraveAct(int fi, int act) {
  if (fi < 0 || fi >= (int)party.fallen.size()) return false;
  party::Fallen& f = party.fallen[(size_t)fi];
  switch (act) {
    case 1: if (f.graveFlags & party::GF_FLOWERS) return false; f.graveFlags |= party::GF_FLOWERS | party::GF_VISITED; return true;
    case 2:
      if ((f.graveFlags & party::GF_KEEPSAKE) || f.belongings.empty()) return false;
      addItem(f.belongings.front());
      f.belongings.erase(f.belongings.begin());
      f.graveFlags |= party::GF_KEEPSAKE | party::GF_VISITED;
      return true;
    case 3: f.graveFlags |= party::GF_VISITED; return true;
    default: return false;
  }
}

bool Game::partyCanCall(int mi) const {
  if (mi < 0 || mi >= (int)party.members.size()) return false;
  const Member& m = party.members[(size_t)mi];
  if (m.state == State::With) return true;
  if (m.state != State::Waiting && m.state != State::Home) return false;
  const float reach = (float)party::CATCH_UP_TILES * TILE;
  auto near = [&](int actorId) {
    const int i = actorId >= 0 ? findActor(actorId) : -1;
    return i > 0 && actors[(size_t)i].st != AState::Dead && len2(actors[(size_t)i].p - pl().p) < reach * reach;
  };
  if (m.actor >= 0 && near(m.actor)) return true;
  if (m.state == State::Waiting) {   // where they wait is here (their body comes into play on the next slow step)
    if (m.mapKind == 0 && !inside) {
      const Vec2 at((float)(m.gx - world.ox) * TILE + 8.0f, (float)(m.gy - world.oy) * TILE + 10.0f);
      if (len2(at - pl().p) < reach * reach) return true;
    } else if (m.mapKind == 2 && inside && subBldg >= 0 && subBldg < (int)world.over.bldgs.size()) {
      if (world.over.bldgs[(size_t)subBldg].id == m.mapId && subFloor == m.floor) return true;
    } else if (m.mapKind == 1 && inside && subSite >= 0 && subSite < (int)world.sites.size()) {
      if (world.sites[(size_t)subSite].id == m.mapId) return true;
    }
  }
  if (m.site && m.res >= 0)
    if (const life::Census* c = life.find(m.site))
      if (m.res < (int)c->res.size() && near(c->res[(size_t)m.res].actor)) return true;
  return false;
}

bool Game::partyCommand(int mi, party::Cmd c) {
  if (mi < 0 || mi >= (int)party.members.size()) return false;
  Member& m = party.members[(size_t)mi];
  using party::Cmd;
  if (m.state == State::Downed || m.state == State::Dead) return false;
  switch (c) {
    case Cmd::Follow: case Cmd::Attack: case Cmd::Defend: case Cmd::Passive:
      if (m.state != State::With) {
        if (m.state == State::Left) return false;   // (amends first: VOICE sets the state back to Home, then Follow)
        if (const life::Resident* rd = PartyOps::resident(*this, m)) if (rd->flags & life::RF_DEAD) return false;   // (fixer M8 r1)
        if (!partyCanCall(mi)) return false;   // (fixer M8 r1) never popped to the player's side from another place
        if (m.voc != party::Vocation::Hound && party.people() >= party::capacity(plLevel)) return false;
        if (m.voc == party::Vocation::Hound && party.animal()) return false;
        m.state = State::With;
        m.homePlot = 0;
        m.lastDay = std::max(m.lastDay, day);
        if (life::Resident* r = PartyOps::resident(*this, m)) {
          r->flags |= life::RF_AWAY | life::RF_COMPANION;
          if (r->actor >= 0) {   // their street body becomes the companion again (keepUp spawns it beside the player)
            const int ri = findActor(r->actor);
            if (ri > 0 && !actors[(size_t)ri].comp) {
              const Vec2 at = actors[(size_t)ri].p;
              actors.erase(actors.begin() + ri);
              r->actor = -1;
              if (PartyOps::actorOf(*this, mi) < 0 && len2(at - pl().p) < ((float)party::CATCH_UP_TILES * TILE) * ((float)party::CATCH_UP_TILES * TILE)) PartyOps::spawn(*this, mi, at);
            }
            r->actor = -1;
          }
        }
      }
      m.stance = c;
      return true;
    case Cmd::Wait: {
      if (m.state != State::With) return false;
      m.state = State::Waiting;
      const int ai = PartyOps::actorOf(*this, mi);
      const Vec2 at = ai > 0 ? actors[(size_t)ai].p : pl().p;
      m.mapKind = inside ? (subBldg >= 0 ? 2 : 1) : 0;
      m.mapId = !inside ? 0 : subBldg >= 0 ? world.over.bldgs[(size_t)subBldg].id : (subSite >= 0 ? world.sites[(size_t)subSite].id : 0);
      m.floor = (uint8_t)subFloor;
      m.gx = (inside ? 0 : world.ox) + tileOf(at.x);
      m.gy = (inside ? 0 : world.oy) + tileOf(at.y);
      if (ai > 0) { actors[(size_t)ai].st = AState::Idle; actors[(size_t)ai].posture = art::Posture::None; }
      return true;
    }
    case Cmd::Home: {
      if (m.state == State::Left) return false;   // (fixer M8 r2) amends first, like Follow (no way round the leave rule)
      // the player's own built house first (owner: companions can live there), else their census home
      int plot = -1;
      for (size_t i = 0; i < home.plots.size() && plot < 0; i++)
        if (home.plots[i].hw && (home.plots[i].kind == home::PlotKind::House || home.plots[i].state == home::PlotState::Built)) plot = (int)i;
      life::Resident* r = PartyOps::resident(*this, m);
      if (plot < 0 && !r) {   // nowhere to call home: they wait where they stand
        if (m.state != State::With) return false;
        return partyCommand(mi, Cmd::Wait);
      }
      m.state = State::Home;
      m.homePlot = plot >= 0 ? (uint32_t)home.plots[(size_t)plot].id : 0;
      m.target = -1;
      if (plot >= 0 && PartyOps::homePlotOf(*this, m) != home::plotOfInterior(*this)) PartyOps::leave(*this, mi, false);   // the body walks off
      else if (plot < 0) PartyOps::leave(*this, mi, false);
      m.leftDay = -1;
      if (r) {
        if (plot >= 0) r->flags |= life::RF_AWAY | life::RF_COMPANION;   // they live with the player now
        else r->flags = (uint16_t)(r->flags & ~life::RF_AWAY);           // their own life resumes at home (RF_COMPANION kept)
      }
      return true;
    }
    case Cmd::Skill: {
      if (m.state != State::With) return false;
      const int ai = PartyOps::actorOf(*this, mi);
      if (ai < 0) return false;
      const uint8_t un = party::unlockedSkills(m);
      for (int k = 2; k >= 0; k--)
        if ((un & (1u << k)) && m.skillCd[k] <= 0 && PartyOps::useSkill(*this, actors[(size_t)ai], m, k, true)) return true;
      return false;
    }
    default: return false;
  }
}

bool Game::partyEquip(int mi, int ii) {
  if (mi < 0 || mi >= (int)party.members.size() || ii < 0 || ii >= (int)inv.size()) return false;
  Member& m = party.members[(size_t)mi];
  const int slot = party::slotOf(inv[(size_t)ii].kind);
  if (slot < 0 || m.voc == party::Vocation::Hound) return false;
  Item it = inv[(size_t)ii];
  it.count = 1;
  // off the player first (the pack's indices close up)
  for (int* s : {&eqWeapon, &eqBow, &eqStaff, &eqArmor, &eqHelmet, &eqShield, &eqRing, &eqAmulet, &eqGloves, &eqBoots, &eqCloak}) {
    if (*s == ii) *s = -1;
    else if (*s > ii) (*s)--;
  }
  inv.erase(inv.begin() + ii);
  if (m.eq[(size_t)slot] >= 0) partyUnequip(mi, slot);
  m.inv.push_back(it);
  m.eq[(size_t)slot] = (int16_t)(m.inv.size() - 1);
  recalcPlayer();
  const int ai = PartyOps::actorOf(*this, mi);
  if (ai > 0) { actors[(size_t)ai].look = partyLook(m); PartyOps::body(*this, m, actors[(size_t)ai]); }
  return true;
}

bool Game::partyUnequip(int mi, int slot) {
  if (mi < 0 || mi >= (int)party.members.size() || slot < 0 || slot >= party::SLOTS) return false;
  Member& m = party.members[(size_t)mi];
  const int e = m.eq[(size_t)slot];
  if (e < 0 || e >= (int)m.inv.size()) return false;
  const Item it = m.inv[(size_t)e];
  m.inv.erase(m.inv.begin() + e);
  for (int16_t& x : m.eq) { if (x == e) x = -1; else if (x > e) x--; }
  addItem(it, false);
  const int ai = PartyOps::actorOf(*this, mi);
  if (ai > 0) { actors[(size_t)ai].look = partyLook(m); PartyOps::body(*this, m, actors[(size_t)ai]); }
  return true;
}

bool Game::partyGive(int mi, int ii) {
  if (mi < 0 || mi >= (int)party.members.size() || ii < 0 || ii >= (int)inv.size()) return false;
  const ItemKind k = inv[(size_t)ii].kind;
  if (k != ItemKind::Potion && k != ItemKind::Food) return false;
  Member& m = party.members[(size_t)mi];
  Item one = inv[(size_t)ii];
  one.count = 1;
  bool stacked = false;
  for (Item& x : m.inv) if (x.kind == one.kind && x.name == one.name) { x.count++; stacked = true; break; }
  if (!stacked) {
    m.inv.push_back(one);
  }
  if (--inv[(size_t)ii].count <= 0) {
    inv.erase(inv.begin() + ii);
    for (int* s : {&eqWeapon, &eqBow, &eqStaff, &eqArmor, &eqHelmet, &eqShield, &eqRing, &eqAmulet, &eqGloves, &eqBoots, &eqCloak})
      if (*s > ii) (*s)--;
  }
  return true;
}

void Game::partyDismiss(int mi) {
  if (mi < 0 || mi >= (int)party.members.size()) return;
  Member& m = party.members[(size_t)mi];
  if (m.state == State::Downed) return;   // (fixer M8 r1) nobody walks off while bleeding out
  if (life::Resident* r = PartyOps::resident(*this, m)) r->flags = (uint16_t)(r->flags & ~(life::RF_AWAY | life::RF_COMPANION));
  if (m.origin == party::Origin::Mercenary && m.hiredAt) marks.erase(markKey(m.hiredAt * 4 + m.mercSlot, (Mk)PartyOps::MK_HIRED));   // back to their inn
  PartyOps::leave(*this, mi, false);
  PartyOps::removeMember(*this, mi);
}

void Game::partySay(int mi, const std::string& line, float secs) {
  if (mi < 0 || mi >= (int)party.members.size() || line.empty()) return;
  const int ai = PartyOps::actorOf(*this, mi);
  emit(Ev::Banter, ai > 0 ? actors[(size_t)ai].p : pl().p, mi, secs, line);
  events.back().id = party.members[(size_t)mi].id;
  party.lastBanter = line;
  party.lastBanterBy = mi;
  party.banterT = 0;
  party.stats.banter++;
  talkMem.said(speech::lineHash(line), day);
  party.members[(size_t)mi].lastLine = speech::lineHash(line);
}

// (fixer M8 r1) the street look a member's gear is worn over: their census dress (false when their census is not
// loaded: the caller keeps waiting for it), else the generated one from their own seed
bool PartyOps::baseLook(Game& g, const Member& m, art::HumanLook& out) {
  Actor a;
  a.role = Role::Villager;
  bool dressed = false;
  if (m.site && m.res >= 0) dressed = g.dressAsResident(a, m.site, m.res);
  if (!dressed) {
    Rng rr(m.lookSeed);
    g.makeLook(a, Role::Villager, rr);
  }
  out = a.look;
  return dressed || !(m.site && m.res >= 0);
}

// fixed once (saved): the look never changes when their census is put away or the site handles move
void PartyOps::fixLook(Game& g, Member& m) {
  if (m.hasLook || m.voc == party::Vocation::Hound) return;
  art::HumanLook L;
  if (baseLook(g, m, L)) { m.look = L; m.hasLook = true; }
}

art::HumanLook Game::partyLook(const Member& m) const {
  art::HumanLook L;
  if (m.hasLook) L = m.look;
  else PartyOps::baseLook(const_cast<Game&>(*this), m, L);
  L.civil = 0;   // (P2b fix TRADES-DRAWN-ON-V1) a companion fights: a recruited smith's or hunter's tool is a weapon again
  auto at = [&](int s) -> const Item* { const int e = m.eq[(size_t)s]; return e >= 0 && e < (int)m.inv.size() ? &m.inv[(size_t)e] : nullptr; };
  WornGear wg;
  wg.weapon = at(party::S_WEAPON); wg.bow = at(party::S_BOW); wg.staff = at(party::S_STAFF); wg.armor = at(party::S_ARMOR);
  wg.helmet = at(party::S_HELMET); wg.shield = at(party::S_SHIELD); wg.ring = at(party::S_RING); wg.amulet = at(party::S_AMULET);
  wg.gloves = at(party::S_GLOVES); wg.boots = at(party::S_BOOTS); wg.cloak = at(party::S_CLOAK);
  wearGear(L, wg, [this](uint64_t id) -> const cult::Culture* { return id && world.src ? &world.src->culture(id) : nullptr; });
  if (m.voc == party::Vocation::Archer && !wg.weapon) L.weapon = 3;   // the bow in hand
  return L;
}
