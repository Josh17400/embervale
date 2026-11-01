// rpg_test --companion [--seeds A..B] [--soft-share] [--vocations]: M8 Fellowship (VISION_PLAN 9, 13 M8, 15.6.2, 15.26).
// FELLOWS lane (phase A by the lead; the lane tightened and extended it). Every check is a FAIL unless said otherwise.
//   - the tables: every vocation, origin, state, command, hook, deed and skill named; skills unlock at 20 / 50 / 80;
//     approval of any deed stays within -15..15 for every personality; personalities are deterministic, their likes
//     never among their dislikes; a deed against the homeland weighs more
//   - the party block: a full party round-trips byte-identically; damaged blocks are refused or load without a crash
//   - recruiting over 20+ towns and cities: the tavern regulars (40 % of towns one, cities two, villages none), their
//     offer (yes at the inn of an evening, no by day in the street, yes anywhere once befriended), refusals (children,
//     elders, key people: their PartyOffer::why), sellswords at the inns (0..2, hired for a day's wage, paid daily,
//     gone unpaid), every Origin recruited once
//   - per seed, on a real Game: a census recruit (away in the census) with a stake from the real world, following a
//     90-tile walk, into the inn and out again, approval moved by scripted deeds per personality, downed -> bound in
//     time -> 30 %, carried through a door bleeding (the time halved), the save round trip with a party, a downed and
//     a carried member; downed and left -> dead for good with a grave stamped in the home village (solid, found by
//     partyGraveAt, the grave talk, nothing to take: P3), the census death and its grief, Abandoned among the others
//   - (P3 PARTY, namespace p3rules below, first) the owner's companion rules 1-8: every case of ARMOUR_SPAWN_RARITY.md
//     section 9's --companion list, by name, and a seeded fuzz of give / take back / swap / dismiss / leave / rejoin /
//     death (each kit seed lives once; no IF_KIT in the hero's pack or on the ground)
//   - each vocation's ways (party_test_ways: test_party_ways.cpp): a taunt pulls a wolf off the player, the archer
//     shoots from >= 3 tiles, the healer heals, the rogue stabs from behind, the mage's bolts, the hound's bites;
//     Passive takes ~nothing, Defend stays within 3 tiles; the paths (city avenues and gate, a ramp, inn upstairs and
//     down, a cave, fast travel, the carriage, 200 tiles on horseback); waiting at the inn found after a save and load;
//     living at a staged house; determinism; partyStep + partyActor < 0.3 ms per step with 3 members
//   - the damage share: the party takes SHARE_LO..SHARE_HI % of the damage dealt to player + party in bands D 3, 12, 25
//     (a FAIL unless --soft-share); the dealt share is reported. --vocations reports every vocation in every band.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/art/art_party.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

// test_party_ways.cpp
int partyWaysTests(uint64_t seed);
// the P3 PARTY rules (below, in namespace p3rules)
int partyRulesTests(uint64_t seed);
int partyRulesFuzz(uint64_t seed);
int partyRulesMissing();

namespace {

int g_bad = 0;
void fail(const std::string& m) { out("FAIL: companion: %s\n", m.c_str()); g_bad++; }
std::string sd(uint64_t s) { return "seed " + std::to_string(s) + ": "; }

void tables() {
  for (int v = 0; v < (int)party::Vocation::COUNT; v++) if (std::string(party::vocationName((party::Vocation)v)) == "?") fail("a vocation without a name");
  for (int d = 0; d < (int)party::Deed::COUNT; d++) if (!party::deedName((party::Deed)d)[0]) fail("a deed without a name");
  for (int s = 0; s < (int)party::Skill::COUNT; s++) {
    const party::SkillInfo& si = party::skillInfo((party::Skill)s);
    const auto three = party::skillsOf(si.voc);
    bool in = false;
    for (int k = 0; k < 3; k++) if ((int)three[(size_t)k] == s) { in = true; if (si.bond != party::SKILL_BOND[k]) fail(std::string(si.name) + ": unlocks at the wrong bond"); }
    if (!in) fail(std::string(si.name) + ": not among its vocation's three");
    if (!si.what || !si.what[0]) fail(std::string(si.name) + ": no description");
  }
  for (uint32_t seed = 1; seed <= 400; seed++) {
    party::Member m;
    m.p = party::personalityFor((uint8_t)(seed * 37u), (uint8_t)(seed % (uint32_t)life::Job::COUNT), seed);
    const party::Personality again = party::personalityFor((uint8_t)(seed * 37u), (uint8_t)(seed % (uint32_t)life::Job::COUNT), seed);
    if (again.brave != m.p.brave || again.kind != m.p.kind || again.likes != m.p.likes || again.dislikes != m.p.dislikes) fail("personalityFor is not deterministic");
    if (m.p.likes & m.p.dislikes) fail("a personality likes what it dislikes");
    m.site = 0x1234;
    for (int d = 0; d < (int)party::Deed::COUNT; d++) {
      const int v = party::approvalDelta(m, (party::Deed)d, 0x999, 0x777);
      if (v < -15 || v > 15) fail("approval outside -15..15");
      if ((party::deedInfo((party::Deed)d).flags & party::DF_HOME) && (party::Deed)d != party::Deed::JoinedWar) {
        const int far = party::approvalDelta(m, (party::Deed)d, 0x999, 0x777), home = party::approvalDelta(m, (party::Deed)d, 0x1234, 0x777);
        if (std::abs(home) < std::abs(far)) fail(std::string(party::deedName((party::Deed)d)) + ": weighs less at home than abroad");
      }
    }
    if (party::approvalDelta(m, party::Deed::JoinedWar, 0x777, 0x777) > -10) fail("fighting against the homeland is not a betrayal");
    (void)party::vocationFor((uint8_t)(seed % (uint32_t)life::Job::COUNT), (uint8_t)seed, seed);
  }
  party::Member m;
  m.bond = 49;
  if (party::unlockedSkills(m) != 1) fail("bond 49 does not unlock exactly the first skill");
  m.bond = 80;
  if (party::unlockedSkills(m) != 7) fail("bond 80 does not unlock all three skills");
  // approval by personality: the same deed, opposite natures, opposite reactions
  party::Member kind, cruel;
  kind.p.kind = 2; kind.p.lawful = 2; cruel.p.kind = -2; cruel.p.lawful = -2;
  if (party::approvalDelta(kind, party::Deed::Mercy, 0, 0) <= party::approvalDelta(cruel, party::Deed::Mercy, 0, 0)) fail("mercy pleases the cruel as much as the kind");
  if (party::approvalDelta(kind, party::Deed::Stole, 0, 0) >= party::approvalDelta(cruel, party::Deed::Stole, 0, 0)) fail("theft sits better with the lawful than the lawless");
  party::Member brave, timid;
  brave.p.brave = 2; timid.p.brave = -2;
  if (party::approvalDelta(brave, party::Deed::ExploredRuin, 0, 0) <= party::approvalDelta(timid, party::Deed::ExploredRuin, 0, 0)) fail("the dark pleases the timid as much as the brave");
}

void saveBlock() {
  party::Party P;
  for (int k = 0; k < 3; k++) {
    party::Member m;
    m.id = P.nextId++;
    m.name = "MEMBER " + std::to_string(k);
    m.voc = (party::Vocation)k;
    m.approval = (int8_t)(k * 20 - 15); m.bond = (uint8_t)(k * 30);
    m.inv.push_back(makeArrows(5 + k));
    m.eq[party::S_BOW] = 0;
    m.mem.push_back({party::Deed::Mercy, 5, 3, 0xABCull});
    m.quest = "saga1~x"; m.stake.hook = party::Hook::BurnedHome; m.stake.site = 77;
    m.leftDay = 4; m.hiredAt = 0x55; m.mercSlot = 1; m.mountedDay = 9; m.horse = 2;
    if (k == 1) { m.state = party::State::Downed; m.downT = 11.5f; m.flags |= party::MF_CARRIED; }
    P.members.push_back(m);
  }
  party::Fallen f;
  f.id = P.nextId++; f.name = "FALLEN"; f.cause = "BLED OUT"; f.belongings.push_back(makeArrows(3));
  f.rite = 3; f.weapon = 2; f.species = 0xFF; f.wasAt = 0x77; f.graveFlags = party::GF_SITED | party::GF_HOME;
  P.fallen.push_back(f);
  std::vector<uint8_t> a, b;
  P.serialize(a);
  party::Party Q;
  if (!Q.deserialize(a)) { fail("the party block does not load"); return; }
  Q.serialize(b);
  if (a != b || !(P == Q)) fail("the party block does not round-trip byte-identically");
  if (Q.members.size() != 3 || Q.members[1].state != party::State::Downed || Q.members[1].downT != 11.5f || Q.fallen[0].rite != 3)
    fail("the party block lost a downed member's clock or a grave's rite");
  uint64_t h = 0x9A27ull;
  int loaded = 0;
  for (int i = 0; i < 400; i++) {
    std::vector<uint8_t> bad = a;
    h = ew::mix64(h);
    if (i % 3 == 0) bad.resize((size_t)(h % bad.size()));
    else for (int k = 0; k < 1 + (int)(h % 6); k++) bad[(size_t)(ew::mix64(h + k) % bad.size())] ^= (uint8_t)(1 + (h >> (8 + k)) % 255);
    party::Party R;
    if (!R.deserialize(bad)) continue;
    loaded++;
    std::vector<uint8_t> again;
    R.serialize(again);
  }
  out("companion: damaged party blocks: 400 tried, %d loaded without a crash\n", loaded);
  // an older block version is refused
  std::vector<uint8_t> old = a;
  old[0] = 1;
  party::Party O;
  if (O.deserialize(old)) fail("a version-1 party block loaded");
}

int compActor(const Game& g, int mi = 0) {
  for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].comp == mi + 1 && g.actors[i].st != AState::Dead) return (int)i;
  return -1;
}
void steps(Game& g, int n, Input in = Input()) {
  for (int f = 0; f < n; f++) {
    g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
  }
}
// a census person of a settlement who may travel (adult, not a key person, at home); skip: the n-th such
bool candidate(const life::Resident& r) {
  return !(r.flags & (life::RF_DEAD | life::RF_KEY | life::RF_AWAY)) && r.job != life::Job::Child && r.job != life::Job::Elder && r.age >= 18 && r.age <= 62;
}
int recruitAt(Game& g, int si, int skip = 0) {
  const life::Census* c = g.life.census(g.world, si);
  if (!c) return -1;
  int n = 0;
  for (const life::Resident& r : c->res) {
    if (!candidate(r)) continue;
    if (n++ < skip) continue;
    Actor a;
    a.id = -99; a.npc = true; a.human = true; a.site = si; a.resident = r.idx; a.p = g.pl().p + Vec2(20, 0);
    return g.partyRecruit(a, party::Origin::Tavern);
  }
  return -1;
}
int recruitOne(Game& g) { return recruitAt(g, g.world.startSite); }

void play(uint64_t seed) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.godMode = true;
  steps(g, 30);
  const int mi = recruitOne(g);
  if (mi != 0) { fail(sd(seed) + "no census resident of the start village could be recruited"); return; }
  {
    party::Member& m0 = g.party.members[0];
    const life::Census* c = g.life.census(g.world, g.world.startSite);
    if (!c || !(c->res[(size_t)m0.res].flags & life::RF_COMPANION) || !(c->res[(size_t)m0.res].flags & life::RF_AWAY))
      fail("the recruit's census record is not away with the player");
    if (m0.stake.hook == party::Hook::None || m0.stake.site != m0.site || !m0.stake.seed) fail(sd(seed) + "the recruit has no stake in the world");
    const party::Stake again = party::stakeFor(g, g.world.startSite, m0.res);
    if (again.hook != m0.stake.hook || again.kin != m0.stake.kin || again.foe != m0.stake.foe || again.ruin != m0.stake.ruin) fail(sd(seed) + "stakeFor is not deterministic");
    out("companion: %s%s, %s (%s), stake %s (kin %d, foe %d, ruin %d, kingdom %d)\n", sd(seed).c_str(), m0.name.c_str(), party::vocationName(m0.voc),
        life::jobName((life::Job)m0.job), party::hookName(m0.stake.hook), m0.stake.kin != 0, m0.stake.foe != 0, m0.stake.ruin != 0, m0.stake.kingdom != 0);
  }
  steps(g, 10);
  if (compActor(g) < 0) { fail("the companion is not in play after joining"); return; }
  // ---- a walk east: they keep up (CATCH_UP_TILES at worst)
  g.party.stats = party::Stats();
  {
    Input in;
    in.move = Vec2(1, 0);
    steps(g, 90 * 8, in);
  }
  steps(g, 60);
  {
    const int ai = compActor(g);
    const float lag = ai > 0 ? len(g.actors[(size_t)ai].p - g.pl().p) / TILE : 1e9f;
    out("companion: seed %llu: the walk: worst lag %.1f tiles, %d catch-ups, now %.1f tiles behind\n", (unsigned long long)seed, g.party.stats.worstLagTiles,
        g.party.stats.catchUps, lag);
    if (ai < 0 || lag > (float)party::CATCH_UP_TILES) fail(sd(seed) + "the companion fell behind on a walk");
  }
  // ---- into the inn and out again
  int inn = -1;
  const Site& home = g.world.sites[(size_t)g.world.startSite];
  for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount && inn < 0; b++)
    if (g.world.over.bldgs[(size_t)b].type == art::Building::Inn) inn = b;
  if (inn >= 0 && g.debugEnterBuilding(inn, 0)) {
    steps(g, 20);
    const int ai = compActor(g);
    if (ai < 0 || len(g.actors[(size_t)ai].p - g.pl().p) > 6.0f * TILE) fail(sd(seed) + "the companion did not come into the inn");
    g.debugLeave();
    steps(g, 20);
    if (compActor(g) < 0) fail(sd(seed) + "the companion did not come out of the inn");
  }
  // ---- deeds move approval the way the personality says (and the same deed twice in a step is one)
  {
    party::Member& m = g.party.members[0];
    const int before = m.approval;
    const int want = party::approvalDelta(m, party::Deed::Rescued, 0, 0);
    g.deed(party::Deed::Rescued, 0);
    g.deed(party::Deed::Rescued, 0);
    steps(g, 2);
    if (g.party.members[0].approval != std::clamp(before + want, -100, 100)) fail("a deed's approval was not applied as approvalDelta says (once)");
    const int b2 = g.party.members[0].approval;
    g.deed(party::Deed::KilledInnocent, 0, 2);
    steps(g, 2);
    if (g.party.members.empty() || g.party.members[0].approval >= b2) fail("killing an innocent did not cost approval");
    if (!g.party.members.empty()) g.party.members[0].approval = 20;
    // a personality's own: the same deed on a brave and a timid member
    party::Member& mm = g.party.members[0];
    const party::Personality keep = mm.p;
    mm.p = party::Personality(); mm.p.brave = 2; mm.approval = 0;
    g.deed(party::Deed::SlewBeast, 0); steps(g, 2);
    const int braveSaw = g.party.members[0].approval;
    g.party.members[0].p = party::Personality(); g.party.members[0].p.brave = -2; g.party.members[0].approval = 0;
    g.deed(party::Deed::SlewBeast, 0); steps(g, 2);
    const int timidSaw = g.party.members[0].approval;
    if (braveSaw <= timidSaw) fail(sd(seed) + "a beast slain pleased the timid as much as the brave (" + std::to_string(braveSaw) + " vs " + std::to_string(timidSaw) + ")");
    g.party.members[0].p = keep; g.party.members[0].approval = 20;
  }
  if (g.party.members.empty()) return;
  // ---- downed -> bound in time -> back up; the player kneels while binding
  {
    int ai = compActor(g);
    if (ai < 0) { fail("no companion actor before the downing"); return; }
    g.debugFell(g.actors[(size_t)ai].id);
    steps(g, 2);
    if (g.party.members[0].state != party::State::Downed) { fail("at 0 HP the companion is not downed"); return; }
    ai = compActor(g);
    if (ai < 0 || g.actors[(size_t)ai].posture != art::Posture::Downed || g.actors[(size_t)ai].st != AState::Idle) fail(sd(seed) + "the downed body is not in the Downed posture (st Idle)");
    g.pl().p = g.actors[(size_t)ai].p + Vec2(10, 0);
    g.update(SIM_DT, Input());
    Input use; use.interact = true;
    g.update(SIM_DT, use);
    steps(g, 30);
    if (g.pl().posture != art::Posture::Kneel) fail(sd(seed) + "the player does not kneel while binding the wounds");
    steps(g, (int)(party::REVIVE_SECS * 60) + 20);
    ai = compActor(g);
    const party::Member& m = g.party.members[0];
    if (m.state != party::State::With) fail("binding the wounds did not bring the companion back");
    else if (ai < 0 || std::fabs(g.actors[(size_t)ai].hp - g.actors[(size_t)ai].maxHp * party::REVIVE_HP_PCT / 100.0f) > 2.0f)
      fail("the revived companion is not at 30 % health");
    if (g.pl().posture == art::Posture::Kneel) fail(sd(seed) + "the player still kneels after the binding");
  }
  // ---- carried through a door while bleeding out: the clock halved, still downed, and bound inside
  if (inn >= 0) {
    int ai = compActor(g);
    g.debugFell(g.actors[(size_t)ai].id);
    steps(g, 60);
    const float left = g.party.members[0].downT;
    g.debugEnterBuilding(inn, 0);
    steps(g, 2);
    const party::Member& m = g.party.members[0];
    ai = compActor(g);
    if (m.state != party::State::Downed || ai < 0) fail(sd(seed) + "a downed companion was not carried through the door");
    else if (std::fabs(m.downT - left * party::CARRY_FACTOR) > 0.2f || !(m.flags & party::MF_CARRIED)) fail(sd(seed) + "carrying did not halve the bleeding-out time");
    // the save keeps a carried member as they are, byte for byte
    {
      std::vector<uint8_t> a, b;
      g.serialize(a);
      Game h(1);
      if (!h.deserialize(a)) fail(sd(seed) + "a save with a downed, carried companion does not load");
      else { h.serialize(b); if (a != b) fail(sd(seed) + "a save with a downed companion does not round-trip byte-identically"); }
    }
    if (ai > 0) {
      g.pl().p = g.actors[(size_t)ai].p + Vec2(10, 0);
      Input use; use.interact = true;
      g.update(SIM_DT, Input());
      g.update(SIM_DT, use);
      steps(g, (int)(party::REVIVE_SECS * 60) + 20);
      if (g.party.members[0].state != party::State::With) fail(sd(seed) + "a carried companion could not be bound inside");
    }
    g.debugLeave();
    steps(g, 20);
  }
  // ---- the save round-trips with the party
  {
    std::vector<uint8_t> a, b;
    g.serialize(a);
    Game h(1);
    if (!h.deserialize(a)) fail("a save with a companion does not load");
    else {
      h.serialize(b);
      if (a != b) fail("a save with a companion does not round-trip byte-identically");
      if (h.party.members.size() != 1 || h.party.members[0].name != g.party.members[0].name) fail("the companion is not in the loaded game");
      h.mode = Mode::Play;
      steps(h, 10);
      if (compActor(h) < 0) fail("the loaded companion is not in play");
    }
  }
  // ---- a second member to see the first die; downed and left: dead for good
  {
    g.plLevel = std::max(g.plLevel, party::LEADERSHIP_LEVEL);
    const int second = recruitAt(g, g.world.startSite, 1);
    if (second < 0) fail(sd(seed) + "a second census person could not join at level 20");
    const party::Member mm = g.party.members[0];
    int ai = compActor(g);
    g.debugFell(g.actors[(size_t)ai].id);
    g.pl().p += Vec2(TILE * 14.0f, 0);
    if (second >= 0) { const int a2 = compActor(g, second); if (a2 > 0) g.actors[(size_t)a2].p = g.pl().p + Vec2(14, 0); }
    steps(g, (int)(party::DOWNED_SECS * 60) + 30);
    bool gone = true;
    for (const party::Member& o : g.party.members) if (o.id == mm.id) gone = false;
    if (!gone) fail("a companion left bleeding out did not die");
    if (g.party.fallen.size() != 1 || g.party.fallen[0].name != mm.name) { fail("the dead companion has no grave record"); return; }
    const life::Census* c = g.life.census(g.world, g.world.startSite);
    if (!c || !(c->res[(size_t)mm.res].flags & life::RF_DEAD)) fail("the census does not know the companion died");
    int grieving = 0;
    if (c) for (const life::Resident& r : c->res) grieving += (r.flags & life::RF_GRIEVING) != 0;
    if (g.party.stats.abandoned < 1) fail(sd(seed) + "leaving a friend to bleed out is not the Abandoned deed");
    if (second >= 0 && !g.party.members.empty()) {
      bool remembers = false;
      for (const party::Memory& x : g.party.members[0].mem) remembers |= x.deed == party::Deed::Abandoned;
      if (!remembers) fail(sd(seed) + "the other companion does not remember the friend left to die");
    }
    // the grave: stamped in the home village, solid, found, talked to, a keepsake taken
    const party::Fallen& f = g.party.fallen[0];
    if (!(f.graveFlags & party::GF_SITED) || !(f.graveFlags & party::GF_HOME)) fail(sd(seed) + "the grave was not laid in the home village");
    steps(g, 20);
    const int lx = f.gx - g.world.ox, ly = f.gy - g.world.oy;
    if (PartyOps::graveAt(g, lx, ly) != 0) fail(sd(seed) + "partyGraveAt does not find the grave");
    if (!g.world.over.blocked(lx, ly)) fail(sd(seed) + "the grave's tile is not solid");
    const art::GraveLook gl = g.partyGraveLook(0);
    if (!gl.fresh) fail(sd(seed) + "a new grave is not fresh earth");
    out("companion: seed %llu: %s died (%d grieving in %s), grave at %d,%d (rite %d), %s\n", (unsigned long long)seed, mm.name.c_str(), grieving, home.name.c_str(),
        (int)f.gx, (int)f.gy, (int)f.rite, (f.graveFlags & party::GF_HOME) ? "in the home village" : "where they fell");
    g.pl().p = Vec2(lx * TILE + 8.0f, (ly + 1) * TILE + 10.0f);
    g.pl().face = 1; g.pl().aim = Vec2(0, -1);
    g.mode = Mode::Play;
    Input use; use.interact = true;
    g.update(SIM_DT, use);
    if (g.mode != Mode::Dialogue) fail(sd(seed) + "the use button at the grave opened nothing");
    g.mode = Mode::Play;
    // (P3 PARTY, rule 6) the grave holds nothing to take: everything they carried lies where they fell
    const size_t had = g.inv.size();
    if (!f.belongings.empty() || g.partyGraveAct(0, 2) || g.inv.size() != had) fail(sd(seed) + "the grave holds a keepsake to take (P3: they drop everything where they fell)");
    g.partyGraveAct(0, 1);
    if (!g.partyGraveLook(0).flowers) fail(sd(seed) + "the flowers laid do not show on the grave");
    // after a save and a load the grave is stamped again
    std::vector<uint8_t> a;
    g.serialize(a);
    Game h(1);
    if (!h.deserialize(a)) fail(sd(seed) + "a save with a grave does not load");
    else {
      h.mode = Mode::Play;
      steps(h, 20);
      if (!h.world.over.blocked(f.gx - h.world.ox, f.gy - h.world.oy) || PartyOps::graveAt(h, f.gx - h.world.ox, f.gy - h.world.oy) != 0)
        fail(sd(seed) + "the grave is not in the loaded world");
    }
  }
}

// ---------------------------------------------------------------- recruiting over many towns and cities
void recruiting(uint64_t A, uint64_t B) {
  int towns = 0, townsWith = 0, cities = 0, citiesTwo = 0, villages = 0, villageRegs = 0, inns = 0, mercs = 0;
  int offeredAtInn = 0, refusedByDay = 0, offeredFriend = 0, regularsTried = 0;
  int refusedChild = 0, refusedOld = 0, refusedKey = 0;
  bool hired = false, paid = false, leftUnpaid = false;
  bool origins[(int)party::Origin::COUNT] = {};
  for (uint64_t seed = A; seed <= B && (towns + cities < 20 || !hired); seed++) {
    Game g(seed);
    g.newEndlessGame(seed);
    g.mode = Mode::Play;
    g.noWildSpawns = true;
    g.godMode = true;
    steps(g, 10);
    const ew::GTile st = g.world.src->start().spawn;
    std::vector<ew::SettlementNode> nodes = g.world.src->settlementsIn(st.x - 900, st.y - 900, st.x + 900, st.y + 900);
    std::sort(nodes.begin(), nodes.end(), [&](const ew::SettlementNode& a, const ew::SettlementNode& b) {
      return std::abs(a.x - st.x) + std::abs(a.y - st.y) < std::abs(b.x - st.x) + std::abs(b.y - st.y);
    });
    int visited = 0;
    for (const ew::SettlementNode& n : nodes) {
      if (visited >= 9) break;
      if (n.type == SiteType::Village && villages >= 6) continue;
      g.teleportGlobal(n.x, n.y + 2);
      steps(g, 30);
      const int si = g.world.siteHandle(n.id);
      if (si < 0) continue;
      const life::Census* c = g.life.census(g.world, si);
      if (!c) continue;
      visited++;
      const std::vector<int> regs = g.partyRegulars(si);
      const Site& S = g.world.sites[(size_t)si];
      if (S.type == SiteType::Village) { villages++; villageRegs += (int)regs.size(); }
      else if (S.type == SiteType::Town) { towns++; townsWith += !regs.empty(); if (regs.size() > 1) fail(sd(seed) + S.name + ": a town with two tavern regulars"); }
      else if (S.type == SiteType::City) { cities++; citiesTwo += regs.size() == 2; }
      // the refusals, in the census's own people
      for (const life::Resident& r : c->res) {
        Actor a; a.id = -5; a.npc = true; a.human = true; a.site = si; a.resident = r.idx;
        if (r.flags & life::RF_DEAD) continue;
        const Game::PartyOffer o = g.partyOffer(a);
        if (r.job == life::Job::Child && o.why == "CHILD") refusedChild++;
        if ((r.job == life::Job::Elder || r.age > 62) && !(r.flags & life::RF_KEY) && o.why == "OLD") refusedOld++;
        if ((r.flags & life::RF_KEY) && o.why == "KEY") refusedKey++;
        if (o.ok && !(r.flags & life::RF_BEFRIENDED) && std::find(regs.begin(), regs.end(), (int)r.idx) == regs.end())
          fail(sd(seed) + S.name + ": " + r.name + " would join with no reason to");
      }
      // a regular: no by day in the street, yes at the inn of an evening, yes anywhere once befriended
      if (!regs.empty() && c->inn >= 0) {
        regularsTried++;
        const int ri = regs[0];
        Actor a; a.id = -6; a.npc = true; a.human = true; a.site = si; a.resident = ri;
        g.hour = 11.0f;
        if (!g.partyOffer(a).ok) refusedByDay++;
        if (g.debugEnterBuilding(S.bldgFirst + c->inn, 0)) {
          steps(g, 3);
          g.hour = 20.5f;
          const Game::PartyOffer o = g.partyOffer(a);
          if (o.ok && o.origin == party::Origin::Tavern) offeredAtInn++;
          else fail(sd(seed) + S.name + ": a tavern regular at the inn of an evening says " + o.why);
          // the sellswords there
          const int k = PartyOps::mercsAt(g, S.bldgFirst + c->inn);
          inns++;
          int here = 0;
          for (const Actor& x : g.actors) here += x.partyTag >= party::PT_MERC0 && x.partyTag < party::PT_MERC0 + party::MERC_MAX;
          mercs += here;
          if (here != k) fail(sd(seed) + S.name + ": the inn should keep " + std::to_string(k) + " sellswords, " + std::to_string(here) + " are there");
          if (here > 0 && !hired) {
            for (size_t i = 1; i < g.actors.size(); i++) {
              Actor& x = g.actors[i];
              if (x.partyTag < party::PT_MERC0 || x.partyTag >= party::PT_MERC0 + party::MERC_MAX) continue;
              g.gold = 1000;
              const Game::PartyOffer o = g.partyOffer(x);
              if (!o.ok || o.wage < party::WAGE_MIN || o.wage > party::WAGE_MAX) { fail(sd(seed) + "a sellsword's offer: " + o.why); break; }
              const int mi = g.partyRecruit(x, o.origin);
              if (mi < 0 || g.party.members[(size_t)mi].origin != party::Origin::Mercenary || g.gold != 1000 - o.wage) { fail(sd(seed) + "a sellsword could not be hired"); break; }
              hired = true;
              origins[(int)party::Origin::Mercenary] = true;
              // a day's wage, then gone when it cannot be paid
              g.day++;
              steps(g, 2);
              paid = g.gold == 1000 - 2 * o.wage;
              g.gold = 0;
              g.day++;
              steps(g, 2);
              leftUnpaid = g.party.members.empty();
              break;
            }
          }
          g.debugLeave();
          steps(g, 5);
        }
        g.life.befriend(c->site, ri);
        g.hour = 11.0f;
        const Game::PartyOffer o = g.partyOffer(a);
        if (o.ok) offeredFriend++;
      }
    }
  }
  out("companion: recruiting: %d towns (%d with a tavern regular, %d %%), %d cities (%d with two), %d villages (%d regulars); "
      "%d inns, %d sellswords; regulars: %d of %d at the inn of an evening, %d refused by day, %d befriended willing anywhere; "
      "refused: %d children, %d elders, %d key people\n",
      towns, townsWith, towns ? 100 * townsWith / towns : 0, cities, citiesTwo, villages, villageRegs, inns, mercs, offeredAtInn, regularsTried, refusedByDay,
      offeredFriend, refusedChild, refusedOld, refusedKey);
  if (towns + cities < 20) fail("fewer than 20 towns and cities visited (" + std::to_string(towns + cities) + ")");
  if (villageRegs) fail("a village has a tavern regular");
  if (towns >= 10 && (townsWith * 100 < towns * 15 || townsWith * 100 > towns * 70)) fail("the towns with a regular are far from 40 %");
  if (cities && citiesTwo * 2 < cities) fail("most cities do not have two tavern regulars");
  if (regularsTried && offeredAtInn < regularsTried) fail("a tavern regular refused at the inn of an evening");
  if (regularsTried && refusedByDay < regularsTried) fail("a tavern regular offered by day in the street without being a friend");
  if (regularsTried && offeredFriend < regularsTried) fail("a befriended regular would not come");
  if (!refusedChild || !refusedOld) fail("children and elders were not refused (CHILD / OLD)");
  if (!hired) fail("no sellsword was hired over the inns visited");
  else { if (!paid) fail("a sellsword's daily wage was not paid from the purse"); if (!leftUnpaid) fail("an unpaid sellsword did not leave"); }
  // every origin
  {
    Game g(A);
    g.newEndlessGame(A);
    g.mode = Mode::Play;
    g.noWildSpawns = true;
    g.godMode = true;
    g.plLevel = party::LEADERSHIP_LEVEL;
    steps(g, 10);
    auto take = [&](int mi) { if (mi >= 0) { origins[(int)g.party.members[(size_t)mi].origin] = true; g.partyDismiss(mi); } };
    const life::Census* c = g.life.census(g.world, g.world.startSite);
    if (c) {
      int fi = -1;
      for (const life::Resident& r : c->res) if (candidate(r)) { fi = r.idx; break; }
      if (fi >= 0) {
        g.life.befriend(c->site, fi);
        Actor a; a.id = -7; a.npc = true; a.human = true; a.site = g.world.startSite; a.resident = fi; a.p = g.pl().p + Vec2(16, 0);
        const Game::PartyOffer o = g.partyOffer(a);
        if (!o.ok || o.origin != party::Origin::Resident) fail("a befriended resident is not offered as a Resident (" + o.why + ")");
        take(g.partyRecruit(a, o.origin));
      }
      take(recruitAt(g, g.world.startSite, 3));   // (a tavern regular's path: the Tavern origin)
    }
    take(g.partyRecruitStranger("ASKA", true, g.world.sites[(size_t)g.world.startSite].culture, 77, party::Origin::Saga, g.pl().p + Vec2(16, 0)));
    // a missing person led home
    {
      Quest q;
      q.id = 9001; q.type = QType::Missing; q.state = QState::Complete; q.flags = QF_FOUND | QF_ESCORTED; q.subject = "INGRID"; q.giverSite = g.world.startSite;
      g.quests.push_back(q);
      Actor a; a.id = g.pl().id + 5000; a.npc = true; a.human = true; a.quest = q.id; a.name = "INGRID"; a.p = g.pl().p + Vec2(16, 0);
      g.actors.push_back(a);
      const Game::PartyOffer o = g.partyOffer(g.actors.back());
      if (!o.ok || o.origin != party::Origin::Rescued) fail("a missing person led home does not offer to come (" + o.why + ")");
      else take(g.partyRecruit(g.actors.back(), o.origin));
    }
    std::string why;
    const int pup = g.partyAdoptAnimal(0, 1234, g.pl().p + Vec2(12, 0), why);
    if (pup >= 0) {
      if (!(g.party.members[(size_t)pup].flags & party::MF_PUP)) fail("the pup is not a pup");
      // it travels outside the people capacity
      const int p1 = recruitAt(g, g.world.startSite, 4), p2 = recruitAt(g, g.world.startSite, 5);
      if (p1 < 0 || p2 < 0) fail("the animal took a person's place in the party (capacity)");
      if (g.partyAdoptAnimal(1, 99, g.pl().p, why) >= 0 || why != "ANIMAL") fail("a second animal was taken in");
      // grows up over PUP_DAYS
      const int ai = (int)g.party.members.size() > pup ? compActor(g, pup) : -1;
      const float pupHp = ai > 0 ? g.actors[(size_t)ai].maxHp : 0;
      g.day += party::PUP_DAYS;
      steps(g, 3);
      int hi = -1;
      for (size_t i = 0; i < g.party.members.size(); i++) if (g.party.members[i].voc == party::Vocation::Hound) hi = (int)i;
      if (hi < 0 || (g.party.members[(size_t)hi].flags & party::MF_PUP)) fail("the pup did not grow up in 14 days");
      else { const int a2 = compActor(g, hi); if (a2 > 0 && g.actors[(size_t)a2].maxHp <= pupHp) fail("the grown hound is no stronger than the pup"); }
      if (hi >= 0) take(hi);
    } else fail("a pup could not be taken in: " + why);
    for (int o = 0; o < (int)party::Origin::COUNT; o++) if (!origins[o]) fail(std::string("no member of origin ") + party::originName((party::Origin)o) + " was recruited");
  }
}

// ---------------------------------------------------------------- the damage share in three bands
struct ShareOut { int pct = 0; int dealtPct = 0; double tot = 0; int onMember = 0, frames = 0, downs = 0; int voc = 0; };
ShareOut shareRun(uint64_t seed, int D, int voc) {
  ShareOut r;
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.debugKit();
  g.plLevel = D;
  steps(g, 20);
  if (recruitOne(g) != 0) { fail("share: no recruit"); return r; }
  r.voc = (int)g.party.members[0].voc;
  if (voc >= 0) {
    party::Member& m = g.party.members[0];
    m.voc = (party::Vocation)voc;
    m.inv.clear();
    m.eq.fill(-1);
    PartyOps::kit(g, m, D);
    const int ai = compActor(g);
    if (ai > 0) { g.actors[(size_t)ai].look = g.partyLook(m); PartyOps::body(g, m, g.actors[(size_t)ai]); g.actors[(size_t)ai].hp = g.actors[(size_t)ai].maxHp; }
  }
  steps(g, 20);
  // out in the open country, away from townsfolk (whom wolves hunt too) and the watch (who fight them)
  {
    const int32_t sx = g.world.ox + (int32_t)(g.pl().p.x / TILE), sy = g.world.oy + (int32_t)(g.pl().p.y / TILE);
    static const int off[][2] = {{90, 0}, {-90, 0}, {0, 90}, {0, -90}, {70, 70}, {-70, 70}, {70, -70}, {-70, -70}, {130, 0}, {-130, 0}};
    for (const auto& o : off) {
      g.teleportGlobal(sx + o[0], sy + o[1]);
      steps(g, 30);
      bool folk = g.settlementAt(g.pl().p) >= 0;
      for (const Actor& a : g.actors) if (a.npc && !a.comp && !a.hostile && len2(a.p - g.pl().p) < (25.0f * TILE) * (25.0f * TILE)) folk = true;
      if (!folk) break;
    }
  }
  g.party.stats = party::Stats();
  // (M7c merge) 32 waves, not 16: a run's share is a mean over the fights, and 16 left it a few points either way on
  // the lie of the land (a wolf snagged in a birch wood, an archer kiting out of reach): --seeds 1..20 put 2 of 60
  // runs at 22-24 % with the game unchanged, and any tuning only moved the misses to other seeds. The bounds stand.
  for (int wave = 0; wave < 32; wave++) {
    // between waves: a downed companion is bound and everyone is whole again (a rest)
    if (!g.party.members.empty()) {
      party::Member& m0 = g.party.members[0];
      const int ai = compActor(g);
      if (m0.state == party::State::Downed) { m0.state = party::State::With; m0.downT = 0; m0.reviveT = 0; }
      if (ai > 0) { g.actors[(size_t)ai].hp = g.actors[(size_t)ai].maxHp; g.actors[(size_t)ai].st = AState::Idle; g.actors[(size_t)ai].posture = art::Posture::None; g.actors[(size_t)ai].p = g.pl().p + Vec2(-14, 6); }
      m0.lastStandT = 0;
    }
    g.pl().hp = g.pl().maxHp;
    // (only the party and the wolves: a passer-by on the road would draw the pack too)
    for (size_t i = 1; i < g.actors.size();) {
      const Actor& o = g.actors[i];
      if ((o.npc && !o.comp && !o.hostile) || o.hostile) g.actors.erase(g.actors.begin() + (std::ptrdiff_t)i); else i++;   // (and the last wave's)
    }
    // three wolves on open ground the player can walk to straight (never across water or a wall)
    {
      int put = 0;
      for (int k = 0; k < 16 && put < 3; k++) {
        const float an = 0.4f * (float)(k + wave) + (float)(k % 2) * 3.1f;
        const Vec2 at = g.pl().p + Vec2(std::cos(an), std::sin(an) * 0.8f) * (64.0f + 6.0f * (float)put);
        bool open = true;
        for (float t = 0; t <= 1.0f && open; t += 0.1f) {
          const Vec2 q = g.pl().p + (at - g.pl().p) * t;
          if (g.world.over.blocked((int)std::floor(q.x / TILE), (int)std::floor(q.y / TILE))) open = false;
        }
        if (!open) continue;
        g.debugSpawnAt(art::Monster::Wolf, at, D);
        put++;
      }
    }
    for (int f = 0; f < 60 * 35; f++) {
      Input in;
      int tgt = -1;
      float bd = 1e9f;
      for (size_t i = 1; i < g.actors.size(); i++) {
        const Actor& e = g.actors[i];
        if (!e.hostile || e.st == AState::Dead) continue;
        const float d2 = len2(e.p - g.pl().p);
        if (d2 < bd) { bd = d2; tgt = (int)i; }
      }
      if (tgt < 0) break;
      const Vec2 d = g.actors[(size_t)tgt].p - g.pl().p;
      in.move = bd > 18.0f * 18.0f ? norm(d) : Vec2();
      in.attack = bd < 26.0f * 26.0f && f % 12 == 0;
      g.pl().aim = norm(d);
      g.pl().hp = std::max(g.pl().hp, g.pl().maxHp * 0.5f);   // (the hero does not die: only the shares count)
      g.update(SIM_DT, in);
      g.events.clear();
      if (g.mode != Mode::Play) g.mode = Mode::Play;
      if (g.pl().st == AState::Dead) { g.pl().st = AState::Idle; g.pl().stT = 0; g.pl().hp = g.pl().maxHp * 0.5f; }   // (never dead: the shares)
      r.frames++;
      {
        const int ca = compActor(g);
        if (ca > 0) for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].hostile && g.actors[i].st != AState::Dead && g.actors[i].target == g.actors[(size_t)ca].id) { r.onMember++; break; }
      }
      if (!g.party.members.empty() && g.party.members[0].state == party::State::Downed) {   // down until the wave is over (never bleeding out)
        if (g.party.members[0].downT > party::DOWNED_SECS - 0.05f) r.downs++;
        g.party.members[0].downT = party::DOWNED_SECS - 0.1f;
      }
    }
  }
  r.tot = g.party.stats.dmgPlayer + g.party.stats.dmgParty;
  r.pct = r.tot > 0 ? (int)std::lround(100.0 * g.party.stats.dmgParty / r.tot) : 0;
  const double dealt = g.party.stats.dealtPlayer + g.party.stats.dealtParty;
  r.dealtPct = dealt > 0 ? (int)std::lround(100.0 * g.party.stats.dealtParty / dealt) : 0;
  return r;
}

void share(uint64_t A, uint64_t B, bool hard, bool allVoc) {
  for (uint64_t seed = A; seed <= B; seed++)
    for (int D : {3, 12, 25}) {
      const ShareOut r = shareRun(seed, D, -1);
      out("companion: seed %llu band D%d (%s): the party took %d %% of %.0f damage, dealt %d %% (a foe on them %d %% of the time, downed %d)\n", (unsigned long long)seed, D, party::vocationName((party::Vocation)r.voc), r.pct, r.tot, r.dealtPct, r.frames ? 100 * r.onMember / r.frames : 0, r.downs);
      if (hard && (r.pct < party::SHARE_LO_PCT || r.pct > party::SHARE_HI_PCT))
        fail(sd(seed) + "band D" + std::to_string(D) + ": the party's damage share " + std::to_string(r.pct) + " % is outside " + std::to_string(party::SHARE_LO_PCT) +
             ".." + std::to_string(party::SHARE_HI_PCT) + " %");
    }
  if (allVoc)
    for (int v = 0; v < (int)party::Vocation::COUNT; v++)
      for (int D : {3, 12, 25}) {
        const ShareOut r = shareRun(A, D, v);
        out("companion: vocations: %s band D%d: took %d %%, dealt %d %%, a foe on them %d %% of the time, downed %d\n", party::vocationName((party::Vocation)v), D, r.pct, r.dealtPct, r.frames ? 100 * r.onMember / r.frames : 0, r.downs);
      }
}

int companionCmd(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  bool hard = true, allVoc = false, shareOnly = false;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc && !parseSeedRange(argv[++i], a, b)) { printf("bad --seeds\n"); return 2; }
    if (!std::strcmp(argv[i], "--soft-share")) hard = false;
    if (!std::strcmp(argv[i], "--strict-share")) hard = true;
    if (!std::strcmp(argv[i], "--vocations")) allVoc = true;
    if (!std::strcmp(argv[i], "--share-only")) shareOnly = true;
  }
  g_bad = 0;
  auto t0 = std::chrono::steady_clock::now();
  auto lap = [&](const char* what) {
    const auto t1 = std::chrono::steady_clock::now();
    out("companion: (%s: %.1f s)\n", what, std::chrono::duration<double>(t1 - t0).count());
    t0 = t1;
  };
  tables();
  saveBlock();
  // (P3 PARTY) the owner's companion rules first (the gift at dismissal first), then the seeded fuzz over every seed
  if (!shareOnly) {
    for (uint64_t s = a; s <= b; s++) { g_curSeed = s; g_bad += partyRulesTests(s); }
    g_bad += partyRulesMissing();
    lap("rules");
    for (uint64_t s = a; s <= b; s++) { g_curSeed = s; g_bad += partyRulesFuzz(s); }
    lap("rules fuzz");
  }
  if (!shareOnly) {
    recruiting(a, std::max(b, a + 5));
    lap("recruiting");
    for (uint64_t s = a; s <= b; s++) { g_curSeed = s; play(s); lap("play"); g_bad += partyWaysTests(s); lap("ways"); }
  }
  share(a, b, hard, allVoc);
  lap("share");
  printf("companion: %d failure(s)\n", g_bad);
  return g_bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--companion", "M8 Fellowship: tables, the party block, recruiting over 20 towns, origins, following, doors, deeds, downed / carried / dead for good, graves, vocations, stances, paths, waiting, the house, the damage share in 3 bands (hard) [--seeds A..B] [--soft-share] [--vocations]", companionCmd);

// ================================================================ P3 PARTY: the owner's companion rules, every case by name, and the fuzz
// rpg_test --companion, P3 PARTY part (FIX_BATCH_P3 2.2 / 8.2): the owner's companion rules 1-8 (ARMOUR_SPAWN_RARITY.md
// section 6) on real games, every case of its section 9 --companion list by name (the gift at dismissal first), and a
// seeded fuzz. Every check is a FAIL unless said otherwise.
//   - a gift handed back at dismissal (the live bug: partyDismiss -> removeMember took it), TAKE BACK refused on a kit
//     piece, a worse or equal gift declined (both packs unchanged), a better gift worn with the kit stowed, TAKE BACK of a
//     gift re-wearing the kit, a gift-for-gift swap returning the first gift, an unpaid and an angry sellsword returning
//     exactly the gifts, a death dropping exactly what they carried as ground loot within a tile (no IF_KIT, nothing into
//     the pack, an empty grave), the kit's item level at most the hero's + 4 on every recruit path, the hire mark on
//     every living parting of a sellsword (his table empty 7 days, standing on the 7th, MERC_FALLEN after his death),
//     leaving and coming back with the hero 10 levels higher (a census friend asked back at home; a sellsword dismissed,
//     unpaid or angry rehired after his week), an angry census companion and an angry sellsword's apology, the parted
//     roll through a save and a load, a version-4 party block, damaged blocks, a parted friend's census death, the 33rd
//     parting, an old save's companion with no kit
//   - the fuzz (seeds A..B): give, take back, swap, dismiss, leave, rejoin, death in a seeded order; after every step no
//     IF_KIT in the hero's pack or on the ground, and each kit seed lives once (pack, members, parted roll, ground), never
//     made again
namespace p3rules {

using party::Member;

int g_rb = 0;
std::set<std::string> g_named;   // the cases checked (printed the first time each runs)
void bad(uint64_t seed, const std::string& m) { out("FAIL: companion: rules: seed %llu: %s\n", (unsigned long long)seed, m.c_str()); g_rb++; }
void named(const char* c) { if (g_named.insert(c).second) out("companion: rules: %s\n", c); }

const char* const kCases[] = {
    "a gift handed back at dismissal (the live bug)",
    "TAKE BACK refused on a kit piece",
    "a worse or equal gift declined, both packs unchanged",
    "a better gift worn, the kit stowed",
    "TAKE BACK of a gift re-wears the kit piece",
    "a gift-for-gift swap returns the first gift",
    "an unpaid sellsword returns exactly the gifts",
    "an angry sellsword returns exactly the gifts",
    "a death drops exactly what they carried as ground loot within a tile, no IF_KIT, nothing into the pack, an empty grave",
    "the kit's item level at most the hero's level + 4 on every recruit path",
    "the hire mark set to the day on every living parting of a sellsword, his table empty 7 days and standing on the 7th",
    "MERC_FALLEN after a sellsword's death: his table never stands again",
    "a census companion dismissed and asked back at home with the hero 10 levels higher comes back the same",
    "a sellsword dismissed and rehired after his week with the hero 10 levels higher comes back the same",
    "a sellsword unpaid and rehired after his week comes back the same",
    "a sellsword angry, ANGRY at his table until his apology, then rehired the same",
    "an angry census companion's record unchanged by the leave and the apology on a later day, approval at least -30",
    "the parted roll through a save and a load",
    "a version-4 party block loads with an empty parted roll",
    "damaged party blocks with a parted roll refused or loaded without a crash",
    "a parted census companion's death removes their record, no pile and no grave",
    "the 33rd parting forgets the friend parted longest ago",
    "an old save's companion who parts with no kit piece gets their kit at the return",
};

void steps(Game& g, int n) {
  for (int f = 0; f < n; f++) {
    g.update(SIM_DT, Input());
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
  }
}
void setup(Game& g, uint64_t seed) {
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.godMode = true;
  g.plLevel = party::LEADERSHIP_LEVEL;   // (room for two people; the kit's cap reads it)
  g.gold = 100000;
  steps(g, 10);
}
// the start village's adults who are no key people, in census order (the dead and the away keep their place, so the n-th
// is always the same person)
bool adult(const life::Resident& r) {
  return !(r.flags & life::RF_KEY) && r.job != life::Job::Child && r.job != life::Job::Elder && r.age >= 18 && r.age <= 62;
}
// the n-th of them, befriended (a reason to come), as a body at the player's side (resident -1: none, or dead)
Actor censusBody(Game& g, int skip) {
  Actor a;
  a.id = -99; a.npc = true; a.human = true; a.site = -1; a.resident = -1;
  const life::Census* c = g.life.census(g.world, g.world.startSite);
  if (!c) return a;
  int n = 0;
  for (const life::Resident& r : c->res) {
    if (!adult(r)) continue;
    if (n++ < skip) continue;
    if (r.flags & life::RF_DEAD) return a;
    a.site = g.world.startSite; a.resident = r.idx; a.p = g.pl().p + Vec2(18, 0);
    g.life.befriend(c->site, r.idx);
    return a;
  }
  return a;
}
int recruitCensus(Game& g, int skip) {
  Actor a = censusBody(g, skip);
  if (a.resident < 0) return -1;
  return g.partyRecruit(a, party::Origin::Resident);
}
std::vector<uint32_t> gearSeeds(const std::vector<Item>& v) {
  std::vector<uint32_t> s;
  for (const Item& it : v) if (party::slotOf(it.kind) >= 0) s.push_back(it.seed);
  std::sort(s.begin(), s.end());
  return s;
}
std::map<int, uint32_t> kitBySlot(const Member& m) {
  std::map<int, uint32_t> k;
  for (int s = 0; s < party::SLOTS; s++) {
    const int e = m.eq[(size_t)s];
    if (e >= 0 && e < (int)m.inv.size() && party::isKit(m.inv[(size_t)e])) k[s] = m.inv[(size_t)e].seed;
  }
  return k;
}
std::vector<uint32_t> kitSeeds(const Member& m) {
  std::vector<uint32_t> s;
  for (const Item& it : m.inv) if (party::isKit(it)) s.push_back(it.seed);
  std::sort(s.begin(), s.end());
  return s;
}
int countSeed(const std::vector<Item>& v, uint32_t seed) { int n = 0; for (const Item& it : v) n += it.seed == seed && party::slotOf(it.kind) >= 0; return n; }
bool packHasKit(const Game& g) { for (const Item& it : g.inv) if (party::isKit(it)) return true; return false; }
int slotWorn(const Member& m) {
  for (int s : {party::S_WEAPON, party::S_ARMOR, party::S_BOW, party::S_STAFF, party::S_BOOTS})
    if (m.eq[(size_t)s] >= 0 && m.eq[(size_t)s] < (int)m.inv.size()) return s;
  return -1;
}
uint32_t g_giftSalt = 1;
// a piece for slot s of the kind they wear there: better (verdict > 0 at their level than `than`) or plainly worse
Item makeFor(const Game& g, const Member& m, int s, int want, const Item* than) {
  const int e = m.eq[(size_t)s];
  const Item& w = m.inv[(size_t)e];
  const int L = party::levelFor(g.plLevel);
  Rng r(hash32(m.id * 7919u ^ g_giftSalt++ ^ (uint32_t)g.seed));
  if (want < 0) return gear::makeGear(r, w.kind, w.sub, 1, Rarity::Common, 0, w.mat);
  const Item& ref = than ? *than : w;
  for (int k = 0; k < 40; k++) {
    const Rarity rr = (Rarity)std::min(5, 2 + k / 8);
    Item it = gear::makeGear(r, w.kind, w.sub, std::min(gear::MAX_D, L + 4), rr, 0, Mat::None);
    if (party::giftVerdict(it, &ref, L) > 0) return it;
  }
  return gear::makeGear(r, w.kind, w.sub, gear::MAX_D, Rarity::Mythic, 0, Mat::None);
}
int toPack(Game& g, const Item& it) {
  g.inv.push_back(it);
  for (int i = (int)g.inv.size() - 1; i >= 0; i--) if (g.inv[(size_t)i].seed == it.seed && g.inv[(size_t)i].kind == it.kind) return i;
  return -1;
}
int memberById(const Game& g, uint32_t id) {
  for (size_t i = 0; i < g.party.members.size(); i++) if (g.party.members[i].id == id) return (int)i;
  return -1;
}
// the record a return must restore (rule 8): id, bond, approval, memories, flags, days, kills, VOICE fields, kit by slot
std::string sameWhy(const Member& a, const Member& b, bool approvalToo = true) {
  if (a.id != b.id) return "id";
  if (a.bond != b.bond) return "bond " + std::to_string(a.bond) + " -> " + std::to_string(b.bond);
  if (party::unlockedSkills(a) != party::unlockedSkills(b)) return "skills";
  if (approvalToo && a.approval != b.approval) return "approval";
  if (a.mem.size() != b.mem.size()) return "memories";
  for (size_t i = 0; i < a.mem.size(); i++) if (a.mem[i].deed != b.mem[i].deed || a.mem[i].day != b.mem[i].day || a.mem[i].delta != b.mem[i].delta) return "a memory";
  if ((a.flags & (party::MF_QUEST_DONE | party::MF_QUEST_OFFERED | party::MF_PUP)) != (b.flags & (party::MF_QUEST_DONE | party::MF_QUEST_OFFERED | party::MF_PUP))) return "flags";
  if (a.days != b.days || a.kills != b.kills) return "days / kills";
  if (a.quest != b.quest || a.questState != b.questState) return "the VOICE fields";
  if (a.horse != b.horse || a.hasLook != b.hasLook || a.lookSeed != b.lookSeed) return "horse / look";
  if (kitBySlot(a) != kitBySlot(b)) return "the kit by slot";
  for (const Item& it : b.inv) if (party::isKit(it) && party::slotOf(it.kind) < 0) return "a kit flag on a supply";
  return std::string();
}

// ---- the sellswords: a town in the loaded window whose inn keeps one
struct Inn { int bi = -1; ew::Gid id = 0; int slot = -1; };
Inn findInn(Game& g) {
  Inn I;
  for (size_t si = 0; si < g.world.sites.size() && I.bi < 0; si++) {
    const Site& S = g.world.sites[si];
    if (S.type != SiteType::Town && S.type != SiteType::City) continue;
    for (int b = S.bldgFirst; b < S.bldgFirst + S.bldgCount && b < (int)g.world.over.bldgs.size(); b++)
      if (PartyOps::mercsAt(g, b) > 0) { I.bi = b; I.id = g.world.over.bldgs[(size_t)b].id; I.slot = 0; break; }
  }
  return I;
}
int mercActor(Game& g, int slot) {
  for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].partyTag == party::PT_MERC0 + slot) return (int)i;
  return -1;
}
// into the inn (a fresh map: its sellswords stand, or not); the merc actor of the slot (-1 none)
int enterInn(Game& g, const Inn& I) {
  if (g.inside) g.debugLeave();
  steps(g, 2);
  if (!g.debugEnterBuilding(I.bi, 0)) return -1;
  steps(g, 3);
  return mercActor(g, I.slot);
}
int hire(Game& g, const Inn& I) {
  const int ai = enterInn(g, I);
  if (ai < 0) return -1;
  return g.partyRecruit(g.actors[(size_t)ai], party::Origin::Mercenary);
}
uint64_t hireKey(const Inn& I) { return markKey(I.id * 4 + (uint64_t)I.slot, (Mk)PartyOps::MK_HIRED); }
int32_t hireMark(const Game& g, const Inn& I) { auto it = g.marks.find(hireKey(I)); return it == g.marks.end() ? -999 : it->second; }
bool chooseOpt(Game& g, const std::string& label) {
  for (size_t o = 0; o < g.dlg.opts.size(); o++) if (g.dlg.opts[o].label == label) { g.dialogueChoose((int)o); return true; }
  return false;
}

// ================================================================ the cases
void giftsCases(Game& g, uint64_t seed) {
  // a census friend with a better gift, sent home: the gift comes back once, their kit and record wait in the roll
  int mi = recruitCensus(g, 0);
  if (mi < 0) { bad(seed, "no census person of the start village would join"); return; }
  {
    Member& m = g.party.members[(size_t)mi];
    const int s = slotWorn(m);
    const std::map<int, uint32_t> kit = kitBySlot(m);
    if (s < 0 || kit.empty()) { bad(seed, "a new companion wears no kit"); return; }
    const Item gift = makeFor(g, m, s, 1, nullptr);
    const int ii = toPack(g, gift);
    if (!g.partyEquip(mi, ii)) bad(seed, "a better gift was not taken");
    const std::vector<uint32_t> pack = gearSeeds(g.inv);
    const uint32_t id = m.id;
    const ew::Gid npc = m.npc;
    g.partyDismiss(mi);
    std::vector<uint32_t> want = pack;
    want.push_back(gift.seed);
    std::sort(want.begin(), want.end());
    if (gearSeeds(g.inv) != want || countSeed(g.inv, gift.seed) != 1) bad(seed, "the gift did not come back exactly once at the dismissal");
    if (packHasKit(g)) bad(seed, "a kit piece reached the hero's pack at the dismissal");
    const int pi = g.party.partedOf(npc, 0, 0);
    if (pi < 0 || g.party.parted[(size_t)pi].m.id != id || kitBySlot(g.party.parted[(size_t)pi].m) != kit) bad(seed, "the record (and the kit as it left) is not in the parted roll");
    named(kCases[0]);
  }
  // take back, declines, a better gift, take back of a gift, the swap
  mi = recruitCensus(g, 1);
  if (mi < 0) { bad(seed, "a second census person would not join"); return; }
  Member& m = g.party.members[(size_t)mi];
  const int s = slotWorn(m);
  const uint32_t kitSeed = m.inv[(size_t)m.eq[(size_t)s]].seed;
  {
    const std::vector<uint32_t> pack = gearSeeds(g.inv), mine = gearSeeds(m.inv);
    if (g.partyUnequip(mi, s) || gearSeeds(g.inv) != pack || gearSeeds(m.inv) != mine) bad(seed, "TAKE BACK moved a kit piece");
    else named(kCases[1]);
  }
  {
    Item same = m.inv[(size_t)m.eq[(size_t)s]];
    same.flags = 0; same.seed ^= 0x5A5A5A5Au;
    const Item worse = makeFor(g, m, s, -1, nullptr);
    const int a = toPack(g, same);
    const std::vector<uint32_t> pack = gearSeeds(g.inv), mine = gearSeeds(m.inv);
    const bool took1 = g.partyEquip(mi, a);
    const int b = toPack(g, worse);
    const std::vector<uint32_t> pack2 = gearSeeds(g.inv);
    const bool took2 = g.partyEquip(mi, b);
    if (took1 || took2 || gearSeeds(m.inv) != mine || gearSeeds(g.inv) != pack2 || pack2.size() != pack.size() + 1) bad(seed, "a worse or equal gift was taken, or a pack changed");
    else named(kCases[2]);
  }
  const Item g1 = makeFor(g, m, s, 1, nullptr);
  {
    if (!g.partyEquip(mi, toPack(g, g1))) bad(seed, "a better gift was declined");
    const int e = m.eq[(size_t)s];
    bool stowed = false;
    for (size_t i = 0; i < m.inv.size(); i++) stowed |= m.inv[i].seed == kitSeed && party::isKit(m.inv[i]) && (int)i != e;
    if (e < 0 || m.inv[(size_t)e].seed != g1.seed || !stowed) bad(seed, "a better gift is not worn with the kit piece stowed");
    else named(kCases[3]);
  }
  {
    if (!g.partyUnequip(mi, s) || countSeed(g.inv, g1.seed) != 1 || m.eq[(size_t)s] < 0 || m.inv[(size_t)m.eq[(size_t)s]].seed != kitSeed)
      bad(seed, "TAKE BACK of a gift did not return it and re-wear the kit piece");
    else named(kCases[4]);
  }
  {
    int ii = -1;
    for (size_t i = 0; i < g.inv.size(); i++) if (g.inv[i].seed == g1.seed) ii = (int)i;
    g.partyEquip(mi, ii);
    const Item g2 = makeFor(g, m, s, 1, &g1);
    const bool ok = g.partyEquip(mi, toPack(g, g2));
    if (!ok || countSeed(g.inv, g1.seed) != 1 || countSeed(m.inv, g1.seed) != 0 || m.inv[(size_t)m.eq[(size_t)s]].seed != g2.seed || countSeed(m.inv, kitSeed) != 1)
      bad(seed, "a gift-for-gift swap did not return the first gift (or moved the kit)");
    else named(kCases[5]);
  }
  // a death: everything they carry on the ground where they fell
  {
    g.inv.push_back(makePotion(PotionType::Health, 1));
    for (size_t i = 0; i < g.inv.size(); i++) if (g.inv[i].kind == ItemKind::Potion) { g.partyGive(mi, (int)i); break; }
    const int ai = PartyOps::actorOf(g, mi);
    if (ai < 0) { bad(seed, "the companion has no body in play"); return; }
    const Vec2 at = g.actors[(size_t)ai].p;
    g.pl().p = at + Vec2(120, 0);   // (out of the magnet's reach)
    std::vector<std::pair<uint32_t, int>> carried;
    for (const Item& it : m.inv) carried.push_back({it.seed, it.count});
    std::sort(carried.begin(), carried.end());
    const size_t had = g.pickups.size(), fallen = g.party.fallen.size();
    const std::vector<uint32_t> pack = gearSeeds(g.inv);
    const size_t packN = g.inv.size();
    PartyOps::dieForGood(g, mi, "TESTED");
    std::vector<std::pair<uint32_t, int>> ground;
    bool far = false, kit = false;
    for (size_t k = had; k < g.pickups.size(); k++) {
      ground.push_back({g.pickups[k].item.seed, g.pickups[k].item.count});
      far |= len(g.pickups[k].p - at) > (float)TILE;
      kit |= party::isKit(g.pickups[k].item);
    }
    std::sort(ground.begin(), ground.end());
    const bool grave = g.party.fallen.size() == fallen + 1 && g.party.fallen.back().belongings.empty();
    if (ground != carried || far || kit || gearSeeds(g.inv) != pack || g.inv.size() != packN || !grave)
      bad(seed, "a death did not lay exactly what they carried within a tile (" + std::to_string(ground.size()) + " of " + std::to_string(carried.size()) +
                    (far ? ", too far" : "") + (kit ? ", IF_KIT on the ground" : "") + (grave ? "" : ", the grave holds something") + ")");
    else named(kCases[8]);
    g.pickups.resize(had);
  }
}

void kitCapCase(Game& g, uint64_t seed) {
  const int was = g.plLevel;
  g.plLevel = 2;
  bool ok = true;
  auto capped = [&](const Member& m) { for (const Item& it : m.inv) if (party::isKit(it) && it.ilvl > g.plLevel + 4) return false; return party::holdsKit(m); };
  const int mi = recruitCensus(g, 2);
  if (mi >= 0) { ok &= capped(g.party.members[(size_t)mi]); g.partyDismiss(mi); } else ok = false;
  for (party::Origin o : {party::Origin::Saga, party::Origin::Rescued, party::Origin::Mercenary}) {
    const int k = g.partyRecruitStranger("CAPTEST", false, g.world.sites[(size_t)g.world.startSite].culture, 4242u + (uint32_t)o, o, g.pl().p + Vec2(14, 0));
    if (k < 0) { ok = false; continue; }
    ok &= capped(g.party.members[(size_t)k]);
    PartyOps::removeMember(g, k);
  }
  Member x;
  x.voc = party::Vocation::Warrior; x.lookSeed = 99;
  PartyOps::kit(g, x, 45);
  ok &= capped(x);
  g.plLevel = was;
  if (!ok) bad(seed, "a kit piece above the hero's level + 4 on a recruit path");
  else named(kCases[9]);
}

void mercCases(Game& g, uint64_t seed) {
  const Inn I = findInn(g);
  if (I.bi < 0) { out("companion: rules: seed %llu: no inn with a sellsword in the window (the sellsword cases run on another seed)\n", (unsigned long long)seed); return; }
  // dismissed: the mark, the week, the same man back
  int mi = hire(g, I);
  if (mi < 0) { bad(seed, "the sellsword could not be hired"); return; }
  Member before = g.party.members[(size_t)mi];
  const Item gift = makeFor(g, before, slotWorn(before), 1, nullptr);
  g.partyEquip(mi, toPack(g, gift));
  before = g.party.members[(size_t)mi];
  const int lvl0 = party::levelFor(g.plLevel);
  g.partyDismiss(mi);
  bool markOk = hireMark(g, I) == g.day;
  const int day0 = g.day;
  bool emptyWeek = true;
  for (int d = 1; d < 7; d++) { g.day = day0 + d; emptyWeek &= enterInn(g, I) < 0; }
  g.day = day0 + 7;
  g.plLevel += 10;
  int ai = enterInn(g, I);
  const bool stands = ai > 0 && g.actors[(size_t)ai].name == before.name;
  if (!markOk || !emptyWeek || !stands) bad(seed, std::string("the dismissed sellsword's week") + (markOk ? "" : " (mark)") + (emptyWeek ? "" : " (stood early)") + (stands ? "" : " (not standing on the 7th as himself)"));
  else named(kCases[10]);
  mi = ai > 0 ? g.partyRecruit(g.actors[(size_t)ai], party::Origin::Mercenary) : -1;
  auto backSame = [&](int k, const Member& was, int lvl, bool approvalToo) -> std::string {
    if (k < 0) return "not rehired";
    const Member& now = g.party.members[(size_t)k];
    const std::string w = sameWhy(was, now, approvalToo);
    if (!w.empty()) return w;
    if (party::levelFor(g.plLevel) < lvl) return "level below the one he left at";
    if (countSeed(g.inv, gift.seed) != 1) return "the gift is not in the pack exactly once";
    return std::string();
  };
  {
    Member was = before;
    // (his gift came back at the parting: the record compared without it, and the kit worn again where it held)
    for (int e = (int)was.inv.size() - 1; e >= 0; e--) if (!party::isKit(was.inv[(size_t)e]) && party::slotOf(was.inv[(size_t)e].kind) >= 0) { was.inv.erase(was.inv.begin() + e); for (int16_t& x : was.eq) { if (x == e) x = -1; else if (x > e) x--; } }
    for (int s = 0; s < party::SLOTS; s++) PartyOps::wearKit(was, s);
    const std::string w = backSame(mi, was, lvl0, true);
    if (!w.empty()) bad(seed, "the rehired sellsword is not the same man: " + w); else named(kCases[13]);
    before = was;
  }
  if (mi < 0) return;
  // unpaid: the gift back, the mark, rehired after the week the same
  {
    const Item g2 = makeFor(g, g.party.members[(size_t)mi], slotWorn(g.party.members[(size_t)mi]), 1, nullptr);
    g.partyEquip(mi, toPack(g, g2));
    const std::vector<uint32_t> pack = gearSeeds(g.inv);
    const Member was = g.party.members[(size_t)mi];
    const long goldWas = g.gold;
    g.gold = 0;
    g.day++;
    steps(g, 3);
    std::vector<uint32_t> want = pack; want.push_back(g2.seed); std::sort(want.begin(), want.end());
    const bool gone = memberById(g, was.id) < 0;
    if (!gone || gearSeeds(g.inv) != want || hireMark(g, I) != g.day) bad(seed, "an unpaid sellsword did not return exactly the gifts (or set his mark)");
    else named(kCases[6]);
    g.gold = goldWas;
    const int pu = g.party.partedOf(0, I.id, I.slot);
    if (pu >= 0) before = g.party.parted[(size_t)pu].m;   // (as he left: a day's bond earned with the hero, the gift handed back)
    g.day += 7;
    ai = enterInn(g, I);
    mi = ai > 0 ? g.partyRecruit(g.actors[(size_t)ai], party::Origin::Mercenary) : -1;
    const std::string w = mi >= 0 ? sameWhy(before, g.party.members[(size_t)mi], true) : std::string("not rehired");
    if (!w.empty()) bad(seed, "the unpaid sellsword rehired is not the same man: " + w); else named(kCases[14]);
  }
  if (mi < 0) return;
  // angry: the gift back, ANGRY at his table until his apology (refused the first day), then his wage
  {
    const Item g3 = makeFor(g, g.party.members[(size_t)mi], slotWorn(g.party.members[(size_t)mi]), 1, nullptr);
    g.partyEquip(mi, toPack(g, g3));
    const std::vector<uint32_t> pack = gearSeeds(g.inv);
    g.party.members[(size_t)mi].approval = -100;
    const uint32_t id = g.party.members[(size_t)mi].id;
    g.deed(party::Deed::KilledInnocent, 0, 3);
    steps(g, 2);
    std::vector<uint32_t> want = pack; want.push_back(g3.seed); std::sort(want.begin(), want.end());
    const int pi = g.party.partedOf(0, I.id, I.slot);
    if (memberById(g, id) >= 0 || gearSeeds(g.inv) != want || pi < 0 || !g.party.parted[(size_t)pi].angry || hireMark(g, I) != g.day)
      bad(seed, "an angry sellsword did not return exactly the gifts (or keep his record, angry, or set his mark)");
    else named(kCases[7]);
    g.day += 7;
    ai = enterInn(g, I);
    bool angryOk = ai > 0 && g.partyOffer(g.actors[(size_t)ai]).why == "ANGRY" && g.partyRecruit(g.actors[(size_t)ai], party::Origin::Mercenary) < 0;
    // the apology at his table: refused today, taken on a later day
    if (ai > 0) { HomeOps::talkTo(g, g.actors[(size_t)ai]); angryOk &= chooseOpt(g, "I WAS WRONG. I'M SORRY."); g.mode = Mode::Play; }
    angryOk &= g.party.partedOf(0, I.id, I.slot) >= 0 && g.party.parted[(size_t)g.party.partedOf(0, I.id, I.slot)].angry;
    g.day++;
    ai = enterInn(g, I);
    int back = -1;
    if (ai > 0) {
      HomeOps::talkTo(g, g.actors[(size_t)ai]);
      angryOk &= chooseOpt(g, "I WAS WRONG. I'M SORRY.") && chooseOpt(g, "I WAS WRONG, AND I'LL DO BETTER.") && chooseOpt(g, "HERE'S YOUR WAGE.");
      g.mode = Mode::Play;
      back = memberById(g, id);
    }
    if (!angryOk || back < 0 || g.party.members[(size_t)back].approval < party::APPROVAL_COMPLAIN + 10) bad(seed, "the angry sellsword's ANGRY and apology at his table");
    else named(kCases[15]);
    if (back >= 0) {
      // his death: the slot never stands again
      PartyOps::dieForGood(g, back, "TESTED");
      g.pickups.clear();
      g.day += 30;
      if (hireMark(g, I) != PartyOps::MERC_FALLEN || enterInn(g, I) >= 0) bad(seed, "a dead sellsword's table stands again");
      else named(kCases[11]);
    }
  }
  if (g.inside) g.debugLeave();
  steps(g, 2);
}

void returnCases(Game& g, uint64_t seed) {
  // a census friend sent home and asked back at home, the hero 10 levels higher
  int mi = recruitCensus(g, 3);
  if (mi < 0) { bad(seed, "no census person for the return case"); return; }
  {
    Member& m = g.party.members[(size_t)mi];
    m.bond = 33; m.approval = 21; m.days = 9; m.kills = 4;
    m.mem.push_back({party::Deed::Mercy, 3, g.day, 0x77});
    const Item gift = makeFor(g, m, slotWorn(m), 1, nullptr);
    g.partyEquip(mi, toPack(g, gift));
    const int lvl = party::levelFor(g.plLevel);
    const Member was0 = g.party.members[(size_t)mi];
    g.partyDismiss(mi);
    const Member was = g.party.parted.back().m;   // (as it left: the gift handed back)
    g.plLevel += 10;
    g.day += 3;
    steps(g, 2);
    const Actor a = censusBody(g, 3);
    Actor body = a;
    const Game::PartyOffer o = g.partyOffer(body);
    const int k = o.ok ? g.partyRecruit(body, o.origin) : -1;
    std::string w = k < 0 ? "not back (" + o.why + ")" : sameWhy(was, g.party.members[(size_t)k]);
    if (w.empty() && was.id != was0.id) w = "id";
    if (w.empty() && party::levelFor(g.plLevel) < lvl) w = "level";
    if (w.empty() && countSeed(g.inv, gift.seed) != 1) w = "the gift is not in the pack exactly once";
    if (w.empty()) { const uint8_t b = g.party.members[(size_t)k].bond; g.day++; steps(g, 2); g.day--; if (g.party.members[(size_t)k].bond > b + 2) w = "bond grew for the days away"; }
    if (!w.empty()) bad(seed, "a census friend asked back is not the same: " + w);
    else named(kCases[12]);
    if (k >= 0) g.partyDismiss(k);
  }
  // an angry census companion: stays a member, Left; the apology on a later day lifts approval, nothing else changes
  mi = recruitCensus(g, 4);
  if (mi >= 0) {
    g.party.members[(size_t)mi].approval = -100;
    const Member was = g.party.members[(size_t)mi];
    g.deed(party::Deed::KilledInnocent, 0, 3);
    steps(g, 2);
    const int k = memberById(g, was.id);
    std::string why;
    Member left;
    if (k < 0 || g.party.members[(size_t)k].state != party::State::Left) why = "not a member, Left";
    else {
      left = g.party.members[(size_t)k];
      Member trimmed = left;   // (the deed that drove them off is a memory of the deed, not of the leave)
      if (trimmed.mem.size() == was.mem.size() + 1) trimmed.mem.pop_back();
      const std::string w = sameWhy(was, trimmed, false);
      if (!w.empty()) why = "the leave changed " + w;
    }
    if (k >= 0 && why.empty()) {
      Actor body;
      body.id = g.pl().id + 777777; body.npc = true; body.human = true; body.site = g.world.startSite; body.resident = was.res; body.p = g.pl().p + Vec2(16, 0);
      g.actors.push_back(body);
      HomeOps::talkTo(g, g.actors.back());
      if (!chooseOpt(g, "I WAS WRONG. I'M SORRY.")) why = "no apology offered (" + g.dlg.text + ")";
      g.mode = Mode::Play;
      g.day++;
      HomeOps::talkTo(g, g.actors.back());
      if (why.empty() && !(chooseOpt(g, "I WAS WRONG. I'M SORRY.") && chooseOpt(g, "I WAS WRONG, AND I'LL DO BETTER."))) why = "the apology on a later day (" + g.dlg.text + ")";
      g.mode = Mode::Play;
      const int k2 = memberById(g, was.id);
      if (why.empty() && (k2 < 0 || g.party.members[(size_t)k2].approval < party::APPROVAL_COMPLAIN + 10)) why = "approval not lifted to -30";
      if (why.empty()) { const std::string w = sameWhy(left, g.party.members[(size_t)k2], false); if (!w.empty()) why = "the apology changed " + w; }
      for (size_t q = 1; q < g.actors.size(); q++) if (g.actors[q].id == body.id) { g.actors.erase(g.actors.begin() + (std::ptrdiff_t)q); break; }
      if (k2 >= 0) g.partyDismiss(k2);
    }
    if (!why.empty()) bad(seed, "an angry census companion: " + why);
    else named(kCases[16]);
  }
}

void rollCases(Game& g, uint64_t seed) {
  // through a save and a load
  {
    std::vector<uint8_t> a, b;
    g.serialize(a);
    Game h(1);
    if (g.party.parted.empty() || !h.deserialize(a)) bad(seed, "the save with a parted roll did not load");
    else {
      std::vector<uint8_t> pa, pb;
      g.party.serialize(pa); h.party.serialize(pb);
      h.serialize(b);
      if (pa != pb || a != b || h.party.parted.size() != g.party.parted.size()) bad(seed, "the parted roll did not round-trip");
      else named(kCases[17]);
    }
  }
  // a version-4 block (save_v23): the same bytes without the roll's count
  {
    party::Party P = g.party;
    P.parted.clear();
    std::vector<uint8_t> v5;
    P.serialize(v5);
    std::vector<uint8_t> v4(v5.begin(), v5.end() - 1);
    v4[0] = 4;
    party::Party Q;
    Q.parted.resize(3);
    if (!Q.deserialize(v4) || !Q.parted.empty() || Q.members.size() != P.members.size()) bad(seed, "a version-4 party block did not load with an empty roll");
    else named(kCases[18]);
    std::vector<uint8_t> v3 = v4;
    v3[0] = 3;
    if (party::Party().deserialize(v3)) bad(seed, "a version-3 party block loaded");
  }
  // damaged blocks
  {
    std::vector<uint8_t> a;
    g.party.serialize(a);
    uint64_t h = 0x9A28ull ^ seed;
    int loaded = 0;
    for (int i = 0; i < 200; i++) {
      std::vector<uint8_t> d = a;
      h = ew::mix64(h);
      if (i % 3 == 0) d.resize((size_t)(h % d.size()));
      else for (int k = 0; k < 1 + (int)(h % 6); k++) d[(size_t)(ew::mix64(h + k) % d.size())] ^= (uint8_t)(1 + (h >> (8 + k)) % 255);
      party::Party R;
      if (R.deserialize(d)) { loaded++; std::vector<uint8_t> again; R.serialize(again); }
    }
    named(kCases[19]);
    (void)loaded;
  }
  // a parted census companion's death in their own life
  {
    const int mi = recruitCensus(g, 5);
    if (mi >= 0) {
      const Member m = g.party.members[(size_t)mi];
      g.partyDismiss(mi);
      const size_t had = g.party.parted.size(), piles = g.pickups.size(), graves = g.party.fallen.size();
      life::Census* c = g.life.findMut(m.site);
      if (c && m.res >= 0 && m.res < (int)c->res.size()) c->res[(size_t)m.res].flags |= life::RF_DEAD;
      steps(g, 30);
      if (g.party.partedOf(m.npc, 0, 0) >= 0 || g.party.parted.size() != had - 1 || g.pickups.size() != piles || g.party.fallen.size() != graves)
        bad(seed, "a parted census companion's death did not remove their record (or left a pile or a grave)");
      else named(kCases[20]);
    }
  }
  // the 33rd parting
  {
    std::vector<party::Parted> keep = g.party.parted;
    g.party.parted.clear();
    for (int i = 0; i < party::MAX_PARTED; i++) {
      party::Parted p;
      p.m.id = g.party.nextId++; p.m.name = "OLD FRIEND " + std::to_string(i); p.m.npc = 0xF00D0000ull + (uint64_t)i; p.m.res = 0; p.day = i;
      party::Party::syncParted(p);
      g.party.parted.push_back(p);
    }
    const uint32_t oldest = g.party.parted.front().m.id;
    const int mi = recruitCensus(g, 6);
    if (mi >= 0) {
      const uint32_t id = g.party.members[(size_t)mi].id;
      g.partyDismiss(mi);
      bool gone = true;
      for (const party::Parted& p : g.party.parted) gone &= p.m.id != oldest;
      if ((int)g.party.parted.size() != party::MAX_PARTED || !gone || g.party.parted.back().m.id != id) bad(seed, "the 33rd parting did not forget the oldest");
      else named(kCases[21]);
    }
    g.party.parted = keep;
  }
  // an old save's companion (no IF_KIT on their gear): everything comes back as gifts once; their kit is made at the return
  {
    const int mi = recruitCensus(g, 7);
    if (mi >= 0) {
      Member& m = g.party.members[(size_t)mi];
      for (Item& it : m.inv) it.flags = (uint8_t)(it.flags & ~party::IF_KIT);
      const size_t gearN = gearSeeds(m.inv).size(), packN = gearSeeds(g.inv).size();
      g.partyDismiss(mi);
      const bool allBack = gearSeeds(g.inv).size() == packN + gearN;
      const Actor body = censusBody(g, 7);
      Actor b2 = body;
      const int k = g.partyRecruit(b2, party::Origin::Resident);
      bool ok = allBack && k >= 0 && party::holdsKit(g.party.members[(size_t)k]);
      if (k >= 0) for (const Item& it : g.party.members[(size_t)k].inv) ok &= !party::isKit(it) || countSeed(g.inv, it.seed) == 0;
      if (!ok) bad(seed, "an old save's companion: the gear did not all come back, or no (new) kit at the return");
      else named(kCases[22]);
      if (k >= 0) g.partyDismiss(k);
    }
  }
}

// ================================================================ the fuzz
struct Fuzz {
  std::set<uint32_t> kit;       // every kit seed seen with IF_KIT
  std::set<uint32_t> retired;   // kit seeds that lost IF_KIT (a death's pile, an old save's gift): never IF_KIT again
  std::string why;
  void see(const std::vector<Item>& v, std::map<uint32_t, int>& n, bool mayKit) {
    for (const Item& it : v) {
      if (party::slotOf(it.kind) < 0) continue;
      if (kit.count(it.seed)) n[it.seed]++;
      if (party::isKit(it)) {
        if (!mayKit && why.empty()) why = "an IF_KIT item in the hero's pack or on the ground";
        if (retired.count(it.seed) && why.empty()) why = "a kit was made again (a retired kit seed is IF_KIT again)";
        if (!kit.count(it.seed)) { kit.insert(it.seed); n[it.seed]++; }
      } else if (kit.count(it.seed)) retired.insert(it.seed);
    }
  }
  bool check(const Game& g) {
    std::map<uint32_t, int> n;
    for (const Member& m : g.party.members) see(m.inv, n, true);
    for (const party::Parted& p : g.party.parted) see(p.m.inv, n, true);
    see(g.inv, n, false);
    std::vector<Item> ground;
    for (const Pickup& k : g.pickups) if (k.gold == 0) ground.push_back(k.item);
    see(ground, n, false);
    for (const auto& e : n) if (e.second > 1 && why.empty()) why = "a kit seed lives twice";
    return why.empty();
  }
};

}  // namespace p3rules

using namespace p3rules;

int partyRulesTests(uint64_t seed) {
  g_rb = 0;
  Game g(seed);
  setup(g, seed);
  giftsCases(g, seed);
  kitCapCase(g, seed);
  returnCases(g, seed);
  mercCases(g, seed);
  rollCases(g, seed);
  return g_rb;
}

int partyRulesMissing() {
  int n = 0;
  for (const char* c : kCases)
    if (!g_named.count(c)) { out("FAIL: companion: rules: never checked over these seeds: %s\n", c); n++; }
  out("companion: rules: %zu of %zu cases checked\n", g_named.size(), sizeof(kCases) / sizeof(kCases[0]));
  return n;
}

int partyRulesFuzz(uint64_t seed) {
  g_rb = 0;
  Game g(seed);
  setup(g, seed);
  Rng r(hash32((uint32_t)seed * 0x9E3779B1u ^ 0xF022u));
  Fuzz F;
  int ops[9] = {0};
  const life::Census* c = g.life.census(g.world, g.world.startSite);
  const int folk = 8;
  for (int step = 0; step < 160 && F.why.empty(); step++) {
    const int op = r.irange(9);
    const int nm = (int)g.party.members.size();
    const int mi = nm ? r.irange(nm) : -1;
    const bool with = mi >= 0 && g.party.members[(size_t)mi].state == party::State::With;
    switch (op) {
      case 0: case 1: {   // someone joins: new, or back from the roll
        if (!c) break;
        Actor a = censusBody(g, r.irange(folk));
        if (a.resident >= 0 && g.partyRecruit(a, party::Origin::Resident) >= 0) ops[0]++;
        break;
      }
      case 2: case 3: {   // a gift, better or worse (a swap when a gift is worn)
        if (!with || g.party.members[(size_t)mi].voc == party::Vocation::Hound) break;
        const Member& m = g.party.members[(size_t)mi];
        const int s = slotWorn(m);
        if (s < 0) break;
        const Item it = makeFor(g, m, s, op == 2 ? 1 : -1, nullptr);
        g.partyEquip(mi, toPack(g, it));
        ops[1]++;
        break;
      }
      case 4: {   // take back
        if (mi < 0) break;
        g.partyUnequip(mi, r.irange(party::SLOTS));
        ops[2]++;
        break;
      }
      case 5: {   // dismissed
        if (!with) break;
        g.partyDismiss(mi);
        ops[3]++;
        break;
      }
      case 6: {   // angry (a census friend stays Left; brought round at once so the fuzz goes on)
        if (!with) break;
        g.party.members[(size_t)mi].approval = -100;
        g.deed(party::Deed::KilledInnocent, 0, 3);
        p3rules::steps(g, 1);
        for (Member& m : g.party.members) if (m.state == party::State::Left) { m.state = party::State::Home; m.approval = -30; }
        ops[4]++;
        break;
      }
      case 7: {   // a death where they stand (the hero takes up some of it: the magnet)
        if (!with || PartyOps::actorOf(g, mi) < 0) break;
        const int ai = PartyOps::actorOf(g, mi);
        g.pl().p = g.actors[(size_t)ai].p + Vec2(r.irange(2) ? 10.0f : 90.0f, 0);
        PartyOps::dieForGood(g, mi, "FUZZ");
        p3rules::steps(g, 40);
        ops[5]++;
        break;
      }
      default: {   // time, levels, the ground's 240 s
        g.plLevel = std::min(60, g.plLevel + r.irange(3));
        if (r.irange(4) == 0) g.day++;
        p3rules::steps(g, 3);
        break;
      }
    }
    p3rules::steps(g, 1);
    if (!F.check(g)) bad(seed, "fuzz step " + std::to_string(step) + " (op " + std::to_string(op) + "): " + F.why);
  }
  // the roll through a save and a load, and the invariant on the loaded game
  {
    std::vector<uint8_t> a;
    g.serialize(a);
    Game h(1);
    if (!h.deserialize(a)) bad(seed, "fuzz: the save did not load");
    else if (!F.check(h)) bad(seed, "fuzz: after a load: " + F.why);
  }
  out("companion: rules fuzz: seed %llu: %d joins, %d gifts, %d take-backs, %d dismissals, %d angry, %d deaths; %zu kit seeds, %zu parted%s\n",
      (unsigned long long)seed, ops[0], ops[1], ops[2], ops[3], ops[4], ops[5], F.kit.size(), g.party.parted.size(), g_rb ? "" : ": each kit seed lives once, no IF_KIT in the pack or on the ground");
  return g_rb;
}
