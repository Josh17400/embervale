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
//     partyGraveAt, the grave talk, a keepsake), the census death and its grief, Abandoned among the others
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
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

// test_party_ways.cpp
int partyWaysTests(uint64_t seed);

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
    if (g.partyGraveAt(lx, ly) != 0) fail(sd(seed) + "partyGraveAt does not find the grave");
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
    const size_t had = g.inv.size();
    const bool kept = g.partyGraveAct(0, 2);
    if (!kept || g.inv.size() < had) fail(sd(seed) + "no keepsake could be taken from the grave");
    if (g.partyGraveAct(0, 2)) fail(sd(seed) + "a second keepsake was taken");
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
      if (!h.world.over.blocked(f.gx - h.world.ox, f.gy - h.world.oy) || h.partyGraveAt(f.gx - h.world.ox, f.gy - h.world.oy) != 0)
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
