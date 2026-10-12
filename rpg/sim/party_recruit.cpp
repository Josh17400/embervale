// M8 Fellowship: who joins the party and why (VISION_PLAN 9 recruitment table + the owner's M8 notes). FELLOWS lane.
//   - tavern regulars: one in TAVERN_TOWN_PCT % of towns, TAVERN_CITY in a city, none in a village, chosen from the
//     census (adults, no key people, whose evening plan is the tavern at the inn most nights): willing at the inn of an
//     evening, and anywhere once befriended;
//   - other residents: the player's friends, and people whose job the player did (Game::npcQuestsDone); never key
//     people, children, elders or a guard on watch;
//   - sellswords: 0..MERC_MAX at a town's or a city's inn (generated people, Actor::partyTag), WAGE_MIN..WAGE_MAX a
//     day by the place's danger, paid each day from the purse (unpaid, they go); no personal arc;
//   - the rescued: an M2 missing person led home offers to come (their census resident when bound, else a stranger);
//     saga allies through the VOICE lane's DSL `recruit` (partyRecruitStranger);
//   - the bonded animal: a pup bought from a farmer or herder with dogs or a stablemaster, or a wolf pup at a cleared
//     wolf den; it grows up over PUP_DAYS and travels outside the people capacity;
//   - the stake from the real world (party::stakeFor): the census, the realm, the region's named uniques, the ruins.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/realm.h"
#include "rpg/world/source.h"

using party::Member;
using party::State;

namespace {
constexpr float T = (float)TILE;
int tileOf(float v) { return (int)std::floor(v / T); }
bool evening(float h) { return h >= 17.0f || h < 2.0f; }
// the inn the player stands in (an Inn building's interior), -1 none
int innHere(const Game& g) {
  if (!g.inside || g.subBldg < 0 || g.subBldg >= (int)g.world.over.bldgs.size()) return -1;
  return g.world.over.bldgs[(size_t)g.subBldg].type == art::Building::Inn ? g.subBldg : -1;
}
uint32_t mercSeed(ew::Gid inn, int k) { return hash32((uint32_t)inn ^ (uint32_t)(inn >> 32) ^ (uint32_t)(k * 0x9E3779B9u) ^ 0x3E7Cu); }
bool seedFemale(uint32_t seed) { Rng rr(seed); return rr.f() < 0.45f; }   // (Game::makeLook's first draw: the same person)
}  // namespace

// ================================================================ tavern regulars
std::vector<int> Game::partyRegulars(int si) {
  std::vector<int> out;
  if (si < 0 || si >= (int)world.sites.size() || !world.sites[(size_t)si].settlement()) return out;
  const Site& S = world.sites[(size_t)si];
  for (const auto& e : party.regulars)
    if (e.first == S.id) { for (int16_t r : e.second) out.push_back(r); return out; }
  const life::Census* c = life.census(world, si);
  if (!c) return out;   // (not built yet: asked again later)
  std::vector<int16_t> pick;
  int want = 0;
  if (S.type == SiteType::City) want = party::TAVERN_CITY;
  else if (S.type == SiteType::Town) want = hash32((uint32_t)S.id ^ (uint32_t)(S.id >> 32) ^ 0x7A7Eu) % 100u < (uint32_t)party::TAVERN_TOWN_PCT ? 1 : 0;
  // (the society's evening place: the inn's common room, or a mead hall, a tea house: Census::gathering)
  auto taverns = [&](int16_t b) { return b >= 0 && (b == c->inn || b == c->gathering || b == c->gathering2); };
  if (want > 0 && (c->inn >= 0 || c->gathering >= 0)) {
    // the census's evening plans over a week: the most nights at the inn's tables
    struct Cand { int nights; uint32_t h; int16_t idx; };
    std::vector<Cand> cands;
    for (const life::Resident& r : c->res) {
      if (r.flags & (life::RF_DEAD | life::RF_KEY | life::RF_AWAY)) continue;
      if (r.age < 18 || r.age > 62 || r.job == life::Job::Child || r.job == life::Job::Elder || r.job == life::Job::Guard ||
          r.job == life::Job::Innkeeper || r.job == life::Job::Server || r.job == life::Job::Noble || r.job == life::Job::Priest)
        continue;
      int nights = 0;
      for (int d = 1; d <= 7; d++) {
        const life::Plan pl = life.plan(world, *c, r, d, 20.5f);
        if (pl.act == life::Act::Tavern && taverns(pl.bldg)) nights++;
      }
      if (nights > 0) cands.push_back({nights, hash32((uint32_t)life::npcId(c->site, r.idx) ^ 0xC0A1u), (int16_t)r.idx});
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.nights != b.nights ? a.nights > b.nights : a.h < b.h; });
    for (size_t i = 0; i < cands.size() && (int)pick.size() < want; i++) pick.push_back(cands[i].idx);
  }
  party.regulars.push_back({S.id, pick});
  for (int16_t r : pick) out.push_back(r);
  return out;
}

// ================================================================ sellswords at an inn
int PartyOps::mercsAt(const Game& g, int bi) {
  if (bi < 0 || bi >= (int)g.world.over.bldgs.size()) return 0;
  const Bldg& b = g.world.over.bldgs[(size_t)bi];
  if (b.type != art::Building::Inn || b.site < 0 || b.site >= (int)g.world.sites.size()) return 0;
  const SiteType t = g.world.sites[(size_t)b.site].type;
  if (t != SiteType::Town && t != SiteType::City) return 0;
  return (int)(hash32((uint32_t)b.id ^ (uint32_t)(b.id >> 32) ^ 0x5E11u) % (uint32_t)(party::MERC_MAX + 1));
}

int PartyOps::mercWage(const Game& g, int si) {
  const int lvl = si >= 0 && si < (int)g.world.sites.size() ? g.world.sites[(size_t)si].level : std::max(1, g.plLevel);
  const bool city = si >= 0 && si < (int)g.world.sites.size() && g.world.sites[(size_t)si].type == SiteType::City;
  return std::clamp(party::WAGE_MIN + 4 * lvl + (city ? 12 : 0), party::WAGE_MIN, party::WAGE_MAX);
}

party::Vocation PartyOps::mercVoc(uint32_t seed) {
  const uint32_t h = hash32(seed ^ 0x7E0Cu) % 20u;
  return h < 8 ? party::Vocation::Warrior : h < 14 ? party::Vocation::Archer : h < 18 ? party::Vocation::Rogue : h < 19 ? party::Vocation::Mage : party::Vocation::Healer;
}

void PartyOps::mapChanged(Game& g) {
  const int bi = innHere(g);
  if (bi < 0 || g.subFloor != 0) return;
  const int n = mercsAt(g, bi);
  if (!n) return;
  const Bldg& b = g.world.over.bldgs[(size_t)bi];
  const Map& M = g.sub;
  std::vector<int> spots;
  for (int y = 2; y < M.h - 2; y++)
    for (int x = 1; x < M.w - 1; x++) {
      if (M.blocked(x, y) || y >= M.exitY - 1) continue;
      if (!g.bodyFree(Vec2(x * T + 8.0f, y * T + 10.0f), 6.0f, false)) continue;
      spots.push_back(y * M.w + x);
    }
  std::sort(spots.begin(), spots.end(), [&](int a, int c) { return hash32((uint32_t)a ^ (uint32_t)b.id) < hash32((uint32_t)c ^ (uint32_t)b.id); });
  for (int k = 0; k < n; k++) {
    // hired (and not let go): not here; one hired lately and let go comes back after a week
    bool away = false;
    for (const Member& m : g.party.members) if (m.origin == party::Origin::Mercenary && m.hiredAt == b.id && m.mercSlot == k) away = true;
    auto it = g.marks.find(markKey(b.id * 4 + (uint64_t)k, (Mk)MK_HIRED));
    if (it != g.marks.end() && (it->second == MERC_FALLEN || g.day - it->second < 7)) away = true;   // (fixer M8 r3) the dead stay dead
    if (away) continue;
    int at = -1;
    for (int s : spots) {
      const Vec2 c((s % M.w) * T + 8.0f, (s / M.w) * T + 10.0f);
      bool crowd = false;
      for (size_t i = 1; i < g.actors.size() && !crowd; i++) if (len2(g.actors[i].p - c) < (2.0f * T) * (2.0f * T)) crowd = true;
      if (!crowd) { at = s; break; }
    }
    if (at < 0) return;
    const uint32_t seed = mercSeed(b.id, k);
    Actor a;
    a.id = g.nextId_++;
    a.npc = true; a.human = true; a.hostile = false; a.faction = Faction::Town;
    a.role = Role::Traveller; a.site = b.site; a.bldg = bi; a.slot = -1; a.resident = -1;
    a.p = Vec2((at % M.w) * T + 8.0f, (at / M.w) * T + 10.0f); a.home = a.p; a.goal = a.p;
    Rng rr(seed);
    g.makeLook(a, Role::Traveller, rr);
    const uint64_t cul = g.world.sites[(size_t)b.site].culture;
    if (cul && g.world.src) { const std::string nm = cult::personName(g.world.src->culture(cul), seed, seedFemale(seed)); if (!nm.empty()) a.name = nm; }
    a.look.outfit = art::Outfit::Leather;
    a.look.hood = false;
    const party::Vocation v = mercVoc(seed);
    a.look.weapon = v == party::Vocation::Archer ? 3 : v == party::Vocation::Mage ? 4 : v == party::Vocation::Rogue ? 2 : 1;
    a.look.shield = v == party::Vocation::Warrior;
    a.culture = cul;
    a.partyTag = (uint8_t)(party::PT_MERC0 + k);
    a.face = 0;
    a.speed = 40; a.maxHp = a.hp = 60;
    g.actors.push_back(a);
  }
}

// a sellsword for hire keeps their spot at the inn (leaning on the wall, watching the door); a body walking home goes
bool PartyOps::folk(Game& g, Actor& a, float dt) {
  if (a.partyTag == PT_WALKOFF) {
    a.lifeT += dt;
    const Actor& p = g.pl();
    Vec2 away = norm(a.p - p.p + Vec2(0.0001f, 0.0f));
    if (g.inside) away = norm(Vec2(g.sub.exitX * T + 8.0f, (g.sub.exitY + 1) * T) - a.p + Vec2(0.0001f, 0.0f));   // out of the door
    const Vec2 goal = a.p + away * (3.0f * T);
    if (!g.navStep(a, goal, a.speed * 0.9f, dt)) g.moveActor(a, away * (a.speed * 0.9f * dt));
    a.st = AState::Walk;
    a.posture = art::Posture::None;
    return true;
  }
  if (a.partyTag >= party::PT_MERC0 && a.partyTag < party::PT_MERC0 + party::MERC_MAX) {
    const bool talking = g.mode == Mode::Dialogue && g.dlg.actor == a.id;
    a.st = AState::Idle;
    if (talking || len2(g.pl().p - a.p) < (4.0f * T) * (4.0f * T)) a.face = faceOf(g.pl().p - a.p);
    else a.face = 0;
    if (len2(a.home - a.p) > 4.0f) g.moveActor(a, norm(a.home - a.p) * std::min(a.speed * 0.5f * dt, len(a.home - a.p)));
    return true;
  }
  return false;
}

// ================================================================ who would come, and on what terms
Game::PartyOffer Game::partyOffer(const Actor& a) const {
  PartyOffer o;
  if (a.comp || a.hostile || !a.npc) { o.why = "NONE"; return o; }
  const int cap = party::capacity(plLevel);
  // a sellsword for hire
  if (a.partyTag >= party::PT_MERC0 && a.partyTag < party::PT_MERC0 + party::MERC_MAX) {
    const int si = a.site;
    o.origin = party::Origin::Mercenary;
    o.voc = PartyOps::mercVoc(mercSeed(a.bldg >= 0 && a.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)a.bldg].id : 0, a.partyTag - party::PT_MERC0));
    o.wage = PartyOps::mercWage(*this, si);
    if (party.people() >= cap) { o.why = "FULL"; return o; }
    if ((int)party.members.size() >= party::MAX_RECORDS) { o.why = "RECORDS"; return o; }   // (fixer M8 r1) waiting ones count
    if (gold < o.wage) { o.why = "BROKE"; return o; }
    o.ok = true;
    return o;
  }
  if (!a.human) { o.why = "NONE"; return o; }
  // a missing person the player led home
  if (a.quest > 0 && a.resident < 0) {
    const Quest* q = questById(a.quest);
    if (q && q->type == QType::Missing && (q->flags & QF_ESCORTED)) {
      if (party.people() >= cap) { o.why = "FULL"; return o; }
      if ((int)party.members.size() >= party::MAX_RECORDS) { o.why = "RECORDS"; return o; }
      o.ok = true; o.origin = party::Origin::Rescued;
      o.voc = party::vocationFor(0, 0, hash32((uint32_t)q->id * 2654435761u ^ 0x5EC0u));
      return o;
    }
  }
  if (a.resident < 0 || a.site < 0 || a.site >= (int)world.sites.size()) { o.why = "NONE"; return o; }
  const life::Census* c = life.find(world.sites[(size_t)a.site].id);
  if (!c || a.resident >= (int)c->res.size()) { o.why = "NONE"; return o; }
  const life::Resident& r = c->res[(size_t)a.resident];
  const ew::Gid npc = life::npcId(c->site, r.idx);
  for (const party::Member& m : party.members)
    if (m.npc == npc) { o.why = m.state == State::Left ? "ANGRY" : "MEMBER"; return o; }
  if (r.flags & life::RF_DEAD) { o.why = "DEAD"; return o; }
  if (r.flags & life::RF_KEY) { o.why = "KEY"; return o; }
  if (r.job == life::Job::Child || r.age < 16) { o.why = "CHILD"; return o; }
  if (r.job == life::Job::Elder || r.age > 62) { o.why = "OLD"; return o; }
  const life::Plan now = life.plan(world, *c, r, day, hour);
  if ((r.job == life::Job::Guard && (now.act == life::Act::Patrol || now.act == life::Act::Work)) ||
      ((r.job == life::Job::Innkeeper || r.job == life::Job::Server) && now.act == life::Act::Work)) { o.why = "DUTY"; return o; }
  if (party.people() >= cap) { o.why = "FULL"; return o; }
  const bool befriended = (r.flags & life::RF_BEFRIENDED) != 0;
  // (fixer M8 r1, review: "the 8-record cap is hidden") those waiting, at home or gone off count toward MAX_RECORDS:
  // the offer says so up front instead of a yes that backs out
  const bool records = (int)party.members.size() >= party::MAX_RECORDS;
  const std::vector<int> regs = const_cast<Game*>(this)->partyRegulars(a.site);
  const bool regular = std::find(regs.begin(), regs.end(), (int)r.idx) != regs.end();
  // at the inn (or the society's evening hall) of their own town
  bool atInn = false;
  if (inside && subBldg >= 0 && subBldg < (int)world.over.bldgs.size() && world.over.bldgs[(size_t)subBldg].site == a.site) {
    const int off = subBldg - world.sites[(size_t)a.site].bldgFirst;
    atInn = off >= 0 && (off == c->inn || off == c->gathering || off == c->gathering2);
  }
  auto qd = npcQuestsDone.find(npcKey(a));
  const bool helped = qd != npcQuestsDone.end() && qd->second > 0;
  o.voc = party::vocationFor((uint8_t)r.job, r.traits, r.lookSeed);
  if (regular && (befriended || (atInn && evening(hour)))) { o.origin = party::Origin::Tavern; if (records) o.why = "RECORDS"; else o.ok = true; return o; }
  if (befriended || helped) { o.origin = party::Origin::Resident; if (records) o.why = "RECORDS"; else o.ok = true; return o; }
  o.why = "UNWILLING";
  return o;
}

// ================================================================ joining
namespace {
// a member who is no census person (a sellsword, someone rescued, a saga's ally, the animal)
Member stranger(Game& g, const std::string& name, bool female, uint64_t culture, uint32_t lookSeed, party::Origin origin, party::Vocation voc) {
  Member m;
  m.id = g.party.nextId++;
  m.origin = origin;
  m.voc = voc;
  m.state = State::With;
  m.name = name; m.female = female; m.culture = culture; m.lookSeed = lookSeed;
  m.age = (uint8_t)(20 + hash32(lookSeed ^ 0xA6Eu) % 25u);
  m.p = party::personalityFor(0, 0, lookSeed);
  m.stake.seed = hash32(lookSeed ^ 0x57A4E000u);
  m.stake.hook = origin == party::Origin::Mercenary ? party::Hook::Debt : origin == party::Origin::Rescued ? party::Hook::LostKin : party::Hook::None;
  const int si = g.settlementAt(g.inside ? Vec2() : g.pl().p);
  int site = si;
  if (g.inside && g.subBldg >= 0) site = g.world.over.bldgs[(size_t)g.subBldg].site;
  if (site >= 0 && site < (int)g.world.sites.size()) {
    m.site = g.world.sites[(size_t)site].id;
    if (g.world.sites[(size_t)site].kingdom >= 0) m.stake.kingdom = g.world.kingdoms[(size_t)g.world.sites[(size_t)site].kingdom].id;
  }
  m.stake.site = m.site;
  m.approval = (int8_t)(origin == party::Origin::Rescued ? 30 : origin == party::Origin::Saga ? 25 : 10);
  m.joinedDay = g.day; m.lastDay = g.day;
  return m;
}
}  // namespace

int Game::partyRecruit(Actor& who, party::Origin origin) {
  if ((int)party.members.size() >= party::MAX_RECORDS) return -1;
  if (!who.npc || who.comp || who.hostile) return -1;
  // a sellsword: hired for the first day's wage, here and now
  if (who.partyTag >= party::PT_MERC0 && who.partyTag < party::PT_MERC0 + party::MERC_MAX) {
    const PartyOffer o = partyOffer(who);
    if (!o.ok) return -1;
    const int k = who.partyTag - party::PT_MERC0;
    const ew::Gid inn = who.bldg >= 0 && who.bldg < (int)world.over.bldgs.size() ? world.over.bldgs[(size_t)who.bldg].id : 0;
    const uint32_t seed = mercSeed(inn, k);
    Member m = stranger(*this, who.name, seedFemale(seed), who.culture, seed, party::Origin::Mercenary, PartyOps::mercVoc(seed));
    m.wage = o.wage;
    m.paidUntil = day;
    m.hiredAt = inn; m.mercSlot = (uint8_t)k;
    gold -= o.wage;
    PartyOps::kit(*this, m, std::max(1, world.sites[(size_t)std::max(0, who.site)].level));
    marks[markKey(inn * 4 + (uint64_t)k, (Mk)PartyOps::MK_HIRED)] = day;
    const Vec2 at = who.p;
    const int oldId = who.id;
    party.members.push_back(std::move(m));
    const int mi = (int)party.members.size() - 1;
    const int ai = findActor(oldId);
    if (ai > 0) actors.erase(actors.begin() + ai);
    const int nid = PartyOps::spawn(*this, mi, at);
    if (dlg.actor == oldId) dlg.actor = nid;
    PartyOps::alert(*this, mi, 5, party.members[(size_t)mi].name + " JOINS YOU FOR " + std::to_string(o.wage) + " GOLD A DAY.");
    sfx((int)Sfx::Coin, pl().p, 1.0f, 0.7f);
    return mi;
  }
  if (!who.human) return -1;
  // someone rescued who is no census person (a missing person's quest body)
  if (who.resident < 0 && who.quest > 0) {
    const Quest* q = questById(who.quest);
    if (!q || q->type != QType::Missing || party.people() >= party::capacity(plLevel)) return -1;
    // (fixer M8 r2) a raid's captive (Census::takenIdx) is a census person: they join as themselves, with their home,
    // kin and history, and their census record goes away with the player (not one at home and one in the party)
    if (q->giverId && q->giverSite >= 0 && q->giverSite < (int)world.sites.size() && world.sites[(size_t)q->giverSite].id == q->giverId) {
      if (life::Census* c = life.census(world, q->giverSite)) {
        int ix = -1;
        if (c->takenIdx >= 0 && c->takenIdx < (int)c->res.size() && c->res[(size_t)c->takenIdx].name == q->subject) ix = c->takenIdx;
        for (size_t k = 0; k < c->res.size() && ix < 0; k++)
          if (c->res[k].name == q->subject && !(c->res[k].flags & life::RF_DEAD) && c->res[k].job != life::Job::Child) ix = (int)k;
        if (ix >= 0) {
          if (c->takenIdx == ix) { c->takenIdx = -1; c->takenQuest = 0; }   // (the raid's thread is closed: quests.cpp)
          Actor tmp;
          tmp.id = who.id; tmp.npc = true; tmp.human = true; tmp.site = q->giverSite; tmp.resident = ix; tmp.p = who.p; tmp.name = q->subject;
          who.quest = 0;
          if (c->res[(size_t)ix].actor >= 0) {   // their street body at home (if one stood up there) goes too
            const int hb = findActor(c->res[(size_t)ix].actor);
            if (hb > 0 && actors[(size_t)hb].id != tmp.id) actors.erase(actors.begin() + hb);
            c->res[(size_t)ix].actor = -1;
          }
          return partyRecruit(tmp, party::Origin::Rescued);
        }
      }
    }
    const uint32_t seed = hash32((uint32_t)q->id * 2654435761u ^ 0x5EC0u);
    const uint64_t cul = q->giverSite >= 0 && q->giverSite < (int)world.sites.size() ? world.sites[(size_t)q->giverSite].culture : 0;
    Member m = stranger(*this, q->subject.empty() ? who.name : q->subject, seedFemale(seed), cul, seed, party::Origin::Rescued, party::vocationFor(0, 0, seed));
    if (q->giverSite >= 0 && q->giverSite < (int)world.sites.size()) { m.site = world.sites[(size_t)q->giverSite].id; m.stake.site = q->targetId ? q->targetId : m.site; }
    m.stake.hook = party::Hook::LostKin;
    m.stake.day = day;
    PartyOps::kit(*this, m, std::max(1, plLevel - 1));
    const Vec2 at = who.p;
    const int oldId = who.id;
    who.quest = 0;
    party.members.push_back(std::move(m));
    const int mi = (int)party.members.size() - 1;
    const int ai = findActor(oldId);
    if (ai > 0) actors.erase(actors.begin() + ai);
    const int nid = PartyOps::spawn(*this, mi, at);
    if (dlg.actor == oldId) dlg.actor = nid;
    PartyOps::alert(*this, mi, 5, party.members[(size_t)mi].name + " JOINS YOU.");
    return mi;
  }
  if (who.resident < 0 || who.site < 0 || who.site >= (int)world.sites.size()) return -1;
  life::Census* c = life.census(world, who.site);
  if (!c || who.resident >= (int)c->res.size()) return -1;
  life::Resident& r = c->res[(size_t)who.resident];
  if (r.flags & life::RF_DEAD) return -1;
  const ew::Gid npc = life::npcId(c->site, r.idx);
  for (const party::Member& o : party.members) if (o.npc == npc) return -1;   // (already one of the party: partyCommand)
  if (party.people() >= party::capacity(plLevel)) return -1;
  const Site& S = world.sites[(size_t)who.site];
  Member m;
  m.id = party.nextId++;
  m.origin = origin;
  m.voc = party::vocationFor((uint8_t)r.job, r.traits, r.lookSeed);
  m.state = State::With;
  m.npc = npc;
  m.site = c->site; m.res = (int16_t)r.idx;
  m.name = r.name; m.female = r.female; m.age = r.age; m.job = (uint8_t)r.job;
  m.culture = S.culture; m.lookSeed = r.lookSeed;
  m.p = party::personalityFor(r.traits, (uint8_t)r.job, r.lookSeed);
  m.stake = party::stakeFor(*this, who.site, who.resident);
  const bool friendOf = (r.flags & life::RF_BEFRIENDED) != 0;
  m.approval = (int8_t)(friendOf ? 25 : origin == party::Origin::Rescued ? 30 : 10);
  m.bond = (uint8_t)(friendOf ? 10 : 0);
  m.joinedDay = day; m.lastDay = day;
  PartyOps::kit(*this, m, S.level);
  r.flags |= life::RF_AWAY | life::RF_COMPANION;
  const Vec2 at = who.p;
  const int oldId = who.id;
  r.actor = -1;
  party.members.push_back(std::move(m));
  const int mi = (int)party.members.size() - 1;
  const int ai = findActor(oldId);
  if (ai > 0) actors.erase(actors.begin() + ai);   // (the street body goes; the companion takes its place)
  const int nid = PartyOps::spawn(*this, mi, at);
  if (dlg.actor == oldId) dlg.actor = nid;
  PartyOps::alert(*this, mi, 5, party.members[(size_t)mi].name + " JOINS YOU.");
  sfx((int)Sfx::QuestStart, pl().p, 1.1f, 0.6f);
  return mi;
}

int Game::partyRecruitStranger(const std::string& name, bool female, uint64_t culture, uint32_t lookSeed, party::Origin origin, Vec2 at) {
  if ((int)party.members.size() >= party::MAX_RECORDS) return -1;
  if (origin == party::Origin::Animal) {
    std::string why;
    return partyAdoptAnimal((uint8_t)(lookSeed & 1u), lookSeed, at, why);
  }
  if (party.people() >= party::capacity(plLevel)) return -1;
  Member m = stranger(*this, name, female, culture, lookSeed, origin, party::vocationFor(0, 0, lookSeed));
  const int si = settlementAt(pl().p);
  const int lvl = si >= 0 ? world.sites[(size_t)si].level : std::max(1, plLevel);
  if (origin == party::Origin::Mercenary) {
    m.wage = PartyOps::mercWage(*this, si);
    m.paidUntil = day;
  }
  PartyOps::kit(*this, m, lvl);
  party.members.push_back(std::move(m));
  const int mi = (int)party.members.size() - 1;
  PartyOps::spawn(*this, mi, at);
  PartyOps::alert(*this, mi, 5, party.members[(size_t)mi].name + " JOINS YOU.");
  return mi;
}

int Game::partyAdoptAnimal(uint8_t species, uint32_t seed, Vec2 at, std::string& why) {
  for (const party::Member& o : party.members) if (o.voc == party::Vocation::Hound) { why = "ANIMAL"; return -1; }
  if ((int)party.members.size() >= party::MAX_RECORDS) { why = "FULL"; return -1; }
  static const char* const hound[] = {"BRAN", "FLINT", "ASH", "TALLY", "RUSK", "MOSS", "WREN", "STORM"};
  static const char* const wolf[] = {"GREYMANE", "SHADE", "FROST", "EMBER", "SKOLL", "HATI", "RIME", "DUSK"};
  Member m;
  m.id = party.nextId++;
  m.origin = party::Origin::Animal;
  m.voc = party::Vocation::Hound;
  m.state = State::With;
  m.species = species > 1 ? 1 : species;
  m.name = (m.species ? wolf : hound)[hash32(seed ^ 0xD06u) % 8u];
  m.female = (hash32(seed ^ 0xF3u) & 1u) != 0;
  m.lookSeed = seed;
  m.age = 0;
  m.flags |= party::MF_PUP;
  m.approval = 50; m.bond = 10;
  const int si = settlementAt(pl().p);
  if (si >= 0) m.site = world.sites[(size_t)si].id;
  m.joinedDay = day; m.lastDay = day;
  party.animalDay = day;
  party.members.push_back(std::move(m));
  const int mi = (int)party.members.size() - 1;
  PartyOps::spawn(*this, mi, at);
  PartyOps::alert(*this, mi, 5, party.members[(size_t)mi].name + " IS YOURS NOW.");
  sfx((int)Sfx::Howl, at, 1.6f, 0.5f);
  return mi;
}

bool Game::partyBuyHorse(int mi, int breed, std::string& why) {
  if (mi < 0 || mi >= (int)party.members.size()) { why = "THERE IS NOBODY TO RIDE IT"; return false; }
  party::Member& m = party.members[(size_t)mi];
  if (m.voc == party::Vocation::Hound) { why = "A HOUND RUNS ON ITS OWN FOUR LEGS"; return false; }
  if (m.horse) { why = m.name + " HAS A HORSE ALREADY"; return false; }
  if (breed < 0 || breed >= (int)home::Breed::COUNT) { why = "NO SUCH HORSE"; return false; }
  const int price = home::breedInfo((home::Breed)breed).price * party::HORSE_FOR_COMPANION_PCT / 100;
  if (gold < price) { why = "YOU CANNOT AFFORD HER"; return false; }
  gold -= price;
  m.horse = (uint8_t)(breed + 1);
  m.flags |= party::MF_MOUNTED;
  m.mountedDay = day;
  sfx((int)Sfx::Coin, pl().p);
  return true;
}

// a cleared wolf den: the pup left behind (the bonded wolf)
bool PartyOps::denInteract(Game& g) {
  if (g.world.dens.empty()) return false;
  for (const Member& o : g.party.members) if (o.voc == party::Vocation::Hound) return false;
  const Actor& p = g.pl();
  const int tx = tileOf(p.p.x), ty = tileOf(p.p.y);
  for (size_t i = 0; i < g.world.dens.size(); i++) {
    const Den& d = g.world.dens[i];
    if (std::abs(d.x - tx) > 2 || std::abs(d.y - ty) > 2) continue;
    if (d.mon != art::Monster::Wolf && d.mon != art::Monster::IceWolf) continue;
    if (g.denClearedDay((int)i) < 0 || g.marks.count(markKey(d.id, (Mk)MK_PUP))) continue;
    std::string why;
    const int mi = g.partyAdoptAnimal(1, hash32((uint32_t)d.id ^ (uint32_t)(d.id >> 32) ^ 0x9A9Au), g.freeSpot(d.x, d.y + 1), why);
    // (fixer M8 r1) the den is marked only once its pup is taken: a full party leaves it there for later
    if (mi >= 0) {
      g.marks[markKey(d.id, (Mk)MK_PUP)] = g.day;
      g.say("A WOLF PUP CREEPS OUT OF THE DEN AND SNIFFS YOUR HAND. " + g.party.members[(size_t)mi].name + " FOLLOWS YOU NOW.");
      return true;
    }
    if (why == "FULL") { g.say("A WOLF PUP WATCHES YOU FROM THE DEN. YOU HAVE TOO MANY FRIENDS TO LOOK AFTER ALREADY."); return true; }
    return false;
  }
  return false;
}

// ================================================================ the stake from the real world
party::Stake party::stakeFor(Game& g, int si, int res) {
  Stake s;
  if (si < 0 || si >= (int)g.world.sites.size()) return s;
  const Site& S = g.world.sites[(size_t)si];
  life::Census* c = g.life.census(g.world, si);
  if (!c || res < 0 || res >= (int)c->res.size()) return s;
  const life::Resident& r = c->res[(size_t)res];
  const uint32_t h = hash32(r.lookSeed ^ 0x57A4E000u);
  s.seed = h;
  s.site = c->site;
  s.day = -(int32_t)(30 + h % 2000u);
  const ew::Gid myK = S.homeKingdom >= 0 && S.homeKingdom < (int)g.world.kingdoms.size() ? g.world.kingdoms[(size_t)S.homeKingdom].id
                      : S.kingdom >= 0 && S.kingdom < (int)g.world.kingdoms.size() ? g.world.kingdoms[(size_t)S.kingdom].id : 0;
  const ew::Gid ownK = S.kingdom >= 0 && S.kingdom < (int)g.world.kingdoms.size() ? g.world.kingdoms[(size_t)S.kingdom].id : 0;
  s.kingdom = myK;
  // ---- the home burned: the realm's flags, charred buildings, a siege, a raid that broke in; a refugee's own home
  const realm::SettlementState* st = g.realm.settlement(S.id);
  int charred = 0;
  for (int b = S.bldgFirst; b < S.bldgFirst + S.bldgCount && b < (int)g.world.over.bldgs.size(); b++) charred += g.world.over.bldgs[(size_t)b].charred > 0;
  const bool burned = (st && ((st->flags & (realm::SS_BURNED | realm::SS_RUINED | realm::SS_REBUILDING)) || st->damage >= 25)) || charred >= 2;
  const bool besieged = g.realm.siegeAt(S.id) != nullptr;
  const bool refugee = (r.flags & life::RF_REFUGEE) || r.origin == life::ORIGIN_REFUGEE;
  // ---- kin lost: a household member dead or away (not travelling with the player), or carried off by raiders
  ew::Gid kin = 0;
  int kinDay = 0;
  for (const life::Resident& o : c->res) {
    if (o.household != r.household || o.idx == r.idx) continue;
    if ((o.flags & life::RF_DEAD) || ((o.flags & life::RF_AWAY) && !(o.flags & life::RF_COMPANION)) || c->takenIdx == (int)o.idx) {
      kin = life::npcId(c->site, o.idx);
      kinDay = o.griefDay ? (int)o.griefDay : (c->raidDay >= 0 ? c->raidDay : 0);
      break;
    }
  }
  // ---- a war of their homeland: going on, or one they came back from
  ew::Gid enemy = 0;
  int warDay = 0;
  bool warNow = false;
  for (const realm::War& w : g.realm.wars()) {
    if (w.attacker != myK && w.defender != myK && w.attacker != ownK && w.defender != ownK) continue;
    const bool on = w.endDay == 0;
    if (enemy && !(on && !warNow)) continue;
    enemy = (w.attacker == myK || w.attacker == ownK) ? w.defender : w.attacker;
    warDay = w.startDay;
    warNow = on;
  }
  // ---- a named beast of their region (the nearest within 300 tiles of home)
  const int32_t gx = g.world.ox + S.ex, gy = g.world.oy + S.ey;
  if (g.world.src) {
    const int32_t rx = gx >= 0 ? gx / ew::REGION : -((-gx + ew::REGION - 1) / ew::REGION);
    const int32_t ry = gy >= 0 ? gy / ew::REGION : -((-gy + ew::REGION - 1) / ew::REGION);
    int64_t bd = 300ll * 300ll;
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++)
        for (const foes::NamedUnique& u : foes::namedInRegion(*g.world.src, rx + dx, ry + dy)) {
          const int64_t ddx = u.gx - gx, ddy = u.gy - gy, d = ddx * ddx + ddy * ddy;
          if (d < bd) { bd = d; s.foe = u.id; }
        }
  }
  // ---- a ruin near home (the family heirloom lost there)
  {
    int64_t bd = 140ll * 140ll;
    for (const Site& o : g.world.sites) {
      if (o.type != SiteType::Ruin) continue;
      const int64_t ddx = o.ex - S.ex, ddy = o.ey - S.ey, d = ddx * ddx + ddy * ddy;
      if (d < bd) { bd = d; s.ruin = o.id; }
    }
  }
  const Personality p = personalityFor(r.traits, (uint8_t)r.job, r.lookSeed);
  // the hook: the heaviest thing in their life first
  if (burned || refugee) {
    s.hook = Hook::BurnedHome;
    if (refugee && st && st->refugeesFrom) s.site = st->refugeesFrom;
    if (st && st->burnDay) s.day = st->burnDay;
  } else if (kin) {
    s.hook = Hook::LostKin;
    s.kin = kin;
    if (kinDay) s.day = kinDay;
  } else if (enemy && (warNow || p.brave >= 0 || r.job == life::Job::Guard)) {
    s.hook = Hook::War;   // a veteran (brave) or a deserter (not): VOICE reads Personality::brave
    s.day = warDay;
  } else if (besieged || c->raidDay >= 0) {
    s.hook = Hook::BurnedHome;
    if (c->raidDay >= 0) s.day = c->raidDay;
  } else if (s.foe && h % 3u != 0) {
    s.hook = Hook::Revenge;
  } else if (s.ruin && h % 2u == 0) {
    s.hook = Hook::Heirloom;
  } else {
    using life::Job;
    if (r.job == Job::Priest || (r.traits & life::TR_DEVOUT)) s.hook = Hook::Faith;
    else if (r.job == Job::Merchant || r.job == Job::Beggar || (r.traits & life::TR_THRIFTY)) s.hook = Hook::Debt;
    else if (r.job == Job::Noble || r.job == Job::Bard || (r.traits & life::TR_BRAVE)) s.hook = Hook::Ambition;
    else if (h % 7u == 0 && ownK && myK && ownK != myK) s.hook = Hook::Exile;   // their people's land is held by another crown
    else if (r.spouse < 0 && (r.traits & life::TR_SOCIABLE)) s.hook = Hook::Love;
    else { static const Hook pool[] = {Hook::Ambition, Hook::Faith, Hook::Love, Hook::Debt}; s.hook = pool[(h >> 4) % 4u]; }
  }
  if (enemy) s.kingdom = enemy;   // the kingdom it is about: the enemy of their war
  if (s.hook == Hook::Exile) s.kingdom = ownK;
  return s;
}
