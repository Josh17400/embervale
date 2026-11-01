// M4 "Banners": the war made visible and the kingdoms' guards (rpg/sim/war.h). WARDS lane.
// The Game hooks the core loop calls (warStep / warTalk / warChoose / warKill / warPropKingdom / warHostile) and what
// they drive (VISION_PLAN 4.4 tier A "present", 4.5, 4.7, 10.4):
//   - the overlays (war_overlay.cpp) re-applied whenever the loaded settlements' realm states or the window change;
//   - the old garrison of a conquered place stood down (the next guards streamed in wear the new owner's colours);
//   - the siege camp's soldiers and captain, the defenders' captain and the men on the walls, the refugees of a camp
//     and the ones walking the road from the burned place;
//   - road patrols of 2-4 soldiers on a kingdom's roads near the player (hostile during a war to a player whose
//     reputation with them is <= -25), fighting the beasts and bandits they meet;
//   - the siege quests: "BREAK THE SIEGE" (the defenders' captain: slay 12 besiegers and their captain) and "JOIN THE
//     ASSAULT" (the besiegers' captain: slay the defenders at the walls until the gate falls); each kill is 0.5
//     strength to the player's side (realm::Realm::contribute) and reputation; the realm resolves the siege on its day.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "engine/sound_ids.h"
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/service_ops.h"
#include "rpg/sim/war.h"
#include "rpg/world/source.h"
#include "rpg/sim/war_ops.h"

namespace {
double nowMs() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
// runtime people's slots (Actor::slot; stable per siege / camp, so a quest giver keeps one identity across loads)
constexpr int SLOT_CAPTAIN = 20000;    // + (siege & 1023) * 4 + 1 the besiegers' captain, + 2 the defenders'
constexpr int SLOT_SOLDIER = 24200;    // camp soldiers, patrols, refugees (+ a running count)
constexpr int DLG_ASSAULT = DLG_WAR + 1, DLG_BREAK = DLG_WAR + 2, DLG_WARNEWS = DLG_WAR + 3;
constexpr int BREAK_NEED = 12;
uint64_t campKey(const WarCamp& c) {
  if (c.kind == CampKind::Siege) return (uint64_t)c.siege;
  if (c.kind == CampKind::Checkpoint) return warTileKey(c.gx, c.gy) ^ 0xC4EC000000000000ull;
  return (uint64_t)c.site ^ 0x5EF0000000000000ull;
}
inline Vec2 tileC(int x, int y) { return Vec2(x * TILE + 8.0f, y * TILE + 10.0f); }

const realm::Siege* siegeById(const Game& g, uint32_t id) {
  for (const realm::Siege& s : g.realm.sieges()) if (s.id == id) return &s;
  return nullptr;
}
std::string kname(const Game& g, ew::Gid k) {
  if (const realm::KingdomState* K = g.realm.kingdom(k)) return K->name;
  auto it = g.world.kingdomById.find(k);
  return it == g.world.kingdomById.end() ? std::string("THE ENEMY") : g.world.kingdoms[(size_t)it->second].name;
}
// a soldier's greeting in his people's idiom (VISION_PLAN 4.5: "soldiers greet in their culture")
std::string soldierGreeting(const Game& g, const Actor& a) {
  const std::string K = kname(g, a.realm);
  int arch = 2;
  auto it = g.world.kingdomById.find(a.realm);
  if (it != g.world.kingdomById.end())
    if (const cult::Culture* C = g.world.cultureOfKingdom(it->second)) arch = (int)C->archetype;
  static const char* lines[(int)cult::Archetype::COUNT][2] = {
      {"HAIL, WANDERER. THE ROADS OF %K ARE WATCHED BY STEEL AND RAVEN.", "KEEP YOUR AXE SHEATHED AND YOUR WORD TRUE, AND %K WILL KEEP YOU."},
      {"WELL MET ON THE HIGH ROAD. %K'S CLANS GUARD THESE PASSES.", "THE GLENS ARE QUIET WHILE %K'S SPEARS ARE ON THE ROAD."},
      {"GOOD DAY. YOU'RE ON %K'S ROAD, UNDER %K'S PEACE.", "MOVE ALONG, FRIEND. THE KING'S MEN OF %K HAVE A ROUND TO WALK."},
      {"HALT AND BE KNOWN. THIS ROAD IS HELD BY THE LEGIONS OF %K.", "ORDER ON THE ROADS, ORDER IN THE TOWNS. THAT IS THE WAY OF %K."},
      {"PEACE UPON YOU, TRAVELLER. THE SANDS ARE WIDE, BUT %K'S RIDERS WIDER.", "WATER AND SHADE TO YOU. %K KEEPS THE WELLS SAFE."},
      {"HAI! A RIDER ON FOOT? THE STEPPE IS %K'S, AND ITS ROADS WITH IT.", "THE KHAN'S HORSE TAIL FLIES OVER %K. RIDE WELL, STRANGER."},
      {"MIND THE BOARDS AND THE BOG, STRANGER. %K WATCHES THE CAUSEWAYS.", "THE MIST HIDES MUCH. %K'S WARDENS SEE THROUGH IT."},
      {"HONOURED TRAVELLER, %K'S WARDENS BID YOU A HARMONIOUS ROAD.", "THE MANDATE OF %K RUNS TO THE LAST MILESTONE."},
      {"THE RIVER GIVES AND THE RIVER TAKES. %K KEEPS ITS BANKS.", "TOLLS ARE PAID AT THE FORD. %K'S PEACE IS FREE."},
      {"THE SUN SEES YOU, TRAVELLER, AND SO DOES %K.", "WALK IN THE LIGHT. %K'S SPEARS WALK BESIDE YOU."},
      {"THE TREES TOLD US YOU WERE COMING. %K'S WARDENS WALK SOFTLY HERE.", "LEAVE THE GROVES AS YOU FOUND THEM, AND %K WILL REMEMBER YOU KINDLY."},
      {"THE STARS FORETOLD A STRANGER. %K'S SENTINELS BID YOU WELCOME.", "YOU WALK UNDER %K'S SPIRES NOW. MIND YOUR STEP."},
  };
  const char* l = lines[std::clamp(arch, 0, (int)cult::Archetype::COUNT - 1)][hash32((uint32_t)a.id * 7u + (uint32_t)g.day) & 1];
  std::string out;
  for (const char* p = l; *p; p++) {
    if (p[0] == '%' && p[1] == 'K') { out += K; p++; }
    else out += *p;
  }
  return out;
}
}  // namespace

// ---------------------------------------------------------------- shared helpers (war.h)
bool warFoes(const Game& g, const Actor& a, const Actor& b) {
  if (factionsHostile(a.faction, b.faction)) return true;
  if (WarOps::hostile(g, a, b)) return true;
  if (a.player) return b.hostile;
  if (b.player) return a.hostile;
  return false;
}

bool warBarred(const Game& g, int bi) {
  if (bi < 0 || bi >= (int)g.world.over.bldgs.size()) return false;
  if (g.world.over.bldgs[(size_t)bi].charred == 2) return true;
  return std::find(g.war.barred.begin(), g.war.barred.end(), bi) != g.war.barred.end();
}

bool warFrontier(const Game& g, int si) {
  if (si < 0 || si >= (int)g.world.sites.size()) return false;
  const Site& s = g.world.sites[(size_t)si];
  if (!s.settlement()) return false;
  if (s.kingdom < 0) return true;
  const realm::SettlementState* st = g.realm.settlement(s.id);
  return st && (st->flags & realm::SS_FRONTIER) && !st->owner;
}

bool warEmpty(const Game& g, int si) {
  if (si < 0 || si >= (int)g.world.sites.size()) return false;
  const realm::SettlementState* st = g.realm.settlement(g.world.sites[(size_t)si].id);
  return st && (st->flags & (realm::SS_ABANDONED | realm::SS_RUINED));
}

int warGuardsWanted(const Game& g, int si) {
  if (si < 0 || si >= (int)g.world.sites.size()) return 0;
  const Site& s = g.world.sites[(size_t)si];
  if (!s.settlement() || warFrontier(g, si) || warEmpty(g, si)) return 0;
  const realm::SettlementState* st = g.realm.settlement(s.id);
  const ew::Gid owner = g.world.kingdoms[(size_t)s.kingdom].id;
  int garrison = st ? st->garrison : (s.type == SiteType::City ? 60 : s.type == SiteType::Town ? 30 : 10);
  // the owner's martial strength: military per thousand people against a middling realm (0.75 .. 1.3)
  float mf = 1.0f;
  if (const realm::KingdomState* K = g.realm.kingdom(owner)) mf = std::clamp(0.75f + 0.25f * K->military / std::max(0.5f, K->pop), 0.75f, 1.3f);
  const float gm = garrison * mf;
  int n;
  switch (s.type) {
    case SiteType::City: n = std::clamp((int)std::lround(7 + (gm - 50) / 8.0f), 7, 12); break;
    case SiteType::Town: n = std::clamp((int)std::lround(3 + (gm - 22) / 10.0f), 3, 5); break;
    default: n = gm >= 12 ? 3 : 2; break;
  }
  if (s.capital) n += 2;
  // a besieged place calls every man to the walls
  if (st && (st->flags & realm::SS_BESIEGED)) n += s.type == SiteType::Village ? 2 : 4;
  // a freshly taken place holds a thinner, foreign garrison
  if (st && (st->flags & realm::SS_OCCUPIED) && s.type != SiteType::Village) n = std::max(2, n - 1);
  return n;
}

void warArmMilitia(const Game& g, Actor& a, uint64_t key) {
  const bool frontier = warFrontier(g, a.site);
  const uint32_t h = hash32((uint32_t)key * 2246822519u ^ (uint32_t)g.seed) % 100u;
  bool brave = a.role == Role::Smith || a.role == Role::Farmer || (a.role == Role::Villager && h < (frontier ? 70u : 30u));
  if (frontier && (a.role == Role::Innkeeper || a.role == Role::Merchant || a.role == Role::Hunter || a.role == Role::Fisher) && h < 60u) brave = true;
  if (!brave) { a.militia = false; return; }
  a.militia = true;
  a.dmg = 7;
  if (frontier && a.site >= 0 && a.site < (int)g.world.sites.size()) a.dmg = 9.0f * (1.0f + 0.13f * (std::max(1, g.world.sites[(size_t)a.site].level) - 1));
}

bool warSpawnAllowed(const Game& g, int si, int tx, int ty, int slot) {
  if (si < 0 || si >= (int)g.world.sites.size()) return true;
  const Site& s = g.world.sites[(size_t)si];
  const realm::SettlementState* st = g.realm.settlement(s.id);
  if (!st) return true;
  if ((st->popPct < 100 || st->damage) && warKeyPerson(g, si, slot)) return true;
  // the realm's population loss (a burning, a conquest): that share of the people is gone
  if (st->popPct < 100 && (int)(hash32((uint32_t)slot * 2654435761u ^ (uint32_t)s.id) % 100u) >= st->popPct) return false;
  if (st->damage == 0) return true;
  // the people of a burned-out home are gone with it (VISION_PLAN 4.5: "their residents are absent")
  const Map& M = g.world.over;
  for (int i = 0; i < s.bldgCount; i++) {
    const int bi = s.bldgFirst + i;
    if (bi < 0 || bi >= (int)M.bldgs.size()) break;
    const Bldg& b = M.bldgs[(size_t)bi];
    if (b.site != si || b.charred != 2) continue;
    if (tx >= b.r.x - 1 && tx <= b.r.x + b.r.w && ty >= b.r.y - 1 && ty <= b.r.y + b.r.h + 1) return false;
  }
  return true;
}

bool warGatesOpen(const Game& g, ew::Gid site) {
  if (std::find(g.war.gatesOpen.begin(), g.war.gatesOpen.end(), site) != g.war.gatesOpen.end()) return true;
  const realm::Siege* sg = g.realm.siegeAt(site);
  if (!sg) return false;
  for (const Quest& q : g.quests) {
    if (q.type != QType::War || q.targetId != site || q.stage != (int)sg->id || (q.flags & QF_FAILED)) continue;
    if (!(q.flags & QF_WAR_ATTACK)) return true;           // the defenders let their champion in
    if (q.state != QState::Active) return true;            // the assault broke the gate
  }
  // (M12 SERVICE) the army's orders: a relief sent in by the defenders, or an assault that broke the gate
  for (const service::Order& o : g.service.orders) {
    if (o.siege != sg->id || o.state == service::OS_FAILED) continue;
    if (o.kind == service::OrderKind::BreakSiege) return true;
    if (o.kind == service::OrderKind::Assault && o.state == service::OS_DONE) return true;
  }
  return false;
}

bool warGatePass(const Game& g, int tx, int ty) {
  return !g.war.gateTiles.empty() && g.war.gateTiles.count(warTileKey(g.world.ox + tx, g.world.oy + ty)) != 0;
}

bool warCaptainDead(const Game& g, uint32_t siege) {
  for (const WarQuestRun& r : g.war.runs) if (r.siege == siege && r.captainDead) return true;
  for (const Quest& q : g.quests) if (q.type == QType::War && q.stage == (int)siege && (q.flags & QF_WAR_CAPDEAD)) return true;
  // (M12 SERVICE) a campaign order's kill: BREAK THE SIEGE's captain, or the captain an ASSASSINATE order was sent for
  for (const service::Order& o : g.service.orders)
    if (o.siege == siege && (o.flags & service::OF_CAPDEAD) && (o.kind == service::OrderKind::BreakSiege || o.kind == service::OrderKind::Assassinate)) return true;
  return false;
}

// a street person who must stay in play whatever the war did to the place: the giver (or turn-in) of an open quest, or
// an actor a running story is bound to (fixer M4 r1: a burned home or a lost share of the people never takes a quest's
// giver with it; fixer M4 r3: nor a smaller watch, nor an emptied place, a guard's bounty included)
bool warKeyPerson(const Game& g, int si, int slot) {
  for (const Quest& q : g.quests)
    if (q.state != QState::Done && q.giverSite == si && q.giverBldg == -1 && q.giverSlot == slot) return true;
  const uint64_t key = g.npcKeyOf(si, -1, slot);
  for (const story::Instance& in : g.story.running()) {
    if (in.done || in.failed) continue;
    for (const story::Binding& b : in.cast) if (b.id == key) return true;
  }
  return false;
}

// ---------------------------------------------------------------- Game hooks
bool WarOps::hostile(const Game& g, const Actor& a, const Actor& b) {
  return a.realm && b.realm && a.realm != b.realm && g.realm.atWar(a.realm, b.realm);
}

ew::Gid Game::warPropKingdom(int tx, int ty) const {
  auto it = war.propKingdom.find(warTileKey(world.ox + tx, world.oy + ty));
  return it == war.propKingdom.end() ? 0 : it->second;
}

void WarOps::step(Game& gm, float dt) {
  if (!gm.world.endless || !gm.world.src || gm.world.over.ground.empty()) return;
  const double t0 = nowMs();
  if (gm.war.barredSayT > 0) gm.war.barredSayT -= dt;
  // ---- 1. the overlays follow the realm and the window
  gm.war.sigT -= dt;
  if (gm.war.sigT <= 0 || gm.war.dirty || gm.war.appliedShifts != gm.world.windowShifts) {
    gm.war.sigT = 0.5f;
    uint64_t sig = 0x9E37ull ^ (uint64_t)gm.world.windowShifts * 0x100000001B3ull;
    for (const realm::War& w : gm.realm.wars()) if (!w.endDay) sig = ew::mix64(sig ^ ((uint64_t)w.id * 0xBF58476D1CE4E5B9ull));   // (checkpoints)
    for (int si : gm.world.nearSites) {
      if (si < 0 || si >= (int)gm.world.sites.size()) continue;
      const Site& s = gm.world.sites[(size_t)si];
      if (!s.settlement()) continue;
      const realm::SettlementState* st = gm.realm.settlement(s.id);
      if (!st) continue;
      if (!st->damage && !(st->flags & (realm::SS_GARRISON | realm::SS_REFUGEES | realm::SS_ABANDONED | realm::SS_RUINED | realm::SS_BESIEGED))) continue;
      const realm::Siege* sg = gm.realm.siegeAt(s.id);
      const bool open = warGatesOpen(gm, s.id);
      sig = ew::mix64(sig ^ s.id ^ ((uint64_t)st->flags << 8) ^ ((uint64_t)st->damage << 24) ^ ((uint64_t)st->owner * 31) ^ ((uint64_t)st->garrisonOf * 7) ^
                      ((uint64_t)(sg ? sg->id : 0) << 40) ^ ((uint64_t)s.bldgCount << 50) ^ (open ? 0xABCDull : 0) ^ ((uint64_t)st->refugeesFrom * 3));
    }
    if (sig != gm.war.appliedSig || gm.war.dirty || gm.war.appliedShifts != gm.world.windowShifts) {
      warApplyOverlays(gm);
      gm.war.appliedSig = sig;
      gm.war.appliedShifts = gm.world.windowShifts;
      gm.war.appliedSerial = gm.realm.eventSerial();
      gm.war.dirty = false;
    }
  }
  if (gm.inside) {   // the camps and patrols wait outside
    const double ms = nowMs() - t0;
    gm.war.steps++; gm.war.stepMs += ms; gm.war.worstStepMs = std::max(gm.war.worstStepMs, ms);
    return;
  }
  // a copy, not a reference: spawnMan pushes onto actors below and can reallocate it
  struct { Vec2 p; } const P{gm.pl().p};
  const float ptx = P.p.x / TILE, pty = P.p.y / TILE;
  auto findId = [&](int id) { return gm.findActor(id); };
  // the player's side in the sieges (active War quests)
  struct Side { uint32_t siege; ew::Gid foe; bool assault; ew::Gid site; };
  std::vector<Side> sides;
  for (const Quest& q : gm.quests)
    if (q.type == QType::War && q.state == QState::Active)
      if (const realm::Siege* sg = siegeById(gm, (uint32_t)q.stage))
        if (!sg->over) sides.push_back({sg->id, (q.flags & QF_WAR_ATTACK) ? sg->defender : sg->attacker, (q.flags & QF_WAR_ATTACK) != 0, sg->site});
  // (M12 SERVICE) an open campaign order at a siege puts the player on its side the same way
  if (gm.service.enlisted())
    for (const service::Order& o : gm.service.orders)
      if (o.state == service::OS_OPEN && o.siege && (o.kind == service::OrderKind::BreakSiege || o.kind == service::OrderKind::Assault))
        if (const realm::Siege* sg = siegeById(gm, o.siege))
          if (!sg->over) sides.push_back({sg->id, o.kind == service::OrderKind::Assault ? sg->defender : sg->attacker, o.kind == service::OrderKind::Assault, sg->site});
  auto spawnMan = [&](Role r, ew::Gid kingdom, Vec2 at, int slot, int level, int site) {
    Actor a;
    a.id = gm.nextId_++;
    a.human = true; a.npc = true; a.role = r;
    a.p = at; a.home = at; a.goal = at;
    a.faction = r == Role::Refugee ? Faction::Town : Faction::Army;
    a.realm = r == Role::Refugee ? 0 : kingdom;
    a.site = site; a.slot = slot; a.fromMap = false; a.bldg = -1;
    Rng rr(hash32((uint32_t)slot * 2654435761u ^ (uint32_t)kingdom ^ (uint32_t)(kingdom >> 32)) ^ (uint32_t)gm.seed);
    gm.makeLook(a, r, rr);
    a.maxHp = 140; a.dmg = 13; a.speed = 50; a.range = 16; a.radius = 4.5f; a.aggroR = 120;
    if (r == Role::Captain) { a.maxHp = 320; a.dmg = 20; a.speed = 52; a.radius = 5.0f; }
    if (r == Role::Refugee) { a.maxHp = 50; a.dmg = 0; a.speed = 40; }
    const int L = std::max(1, level);
    a.maxHp *= 1.0f + 0.10f * (L - 1);
    a.dmg *= 1.0f + 0.13f * (L - 1);
    a.armor = r == Role::Refugee ? 0 : L * 1.5f;
    a.hp = a.maxHp;
    a.level = 10;
    a.special = 0.5f;
    a.xp = r == Role::Captain ? 60 : (r == Role::Soldier ? 22 : 0);
    gm.actors.push_back(a);
    return a.id;
  };
  // ---- 2. a conquered place: its old garrison stands down (the next watch streamed in wears the new owner's colours)
  for (int si : gm.activeSites_) {
    if (si < 0 || si >= (int)gm.world.sites.size() || !gm.world.sites[(size_t)si].settlement()) continue;
    const Site& s = gm.world.sites[(size_t)si];
    const ew::Gid owner = s.kingdom >= 0 ? gm.world.kingdoms[(size_t)s.kingdom].id : 0;
    auto it = gm.war.guardOwner.find(si);
    if (it != gm.war.guardOwner.end() && it->second == owner) continue;
    gm.war.guardOwner[si] = owner;
    for (size_t k = 1; k < gm.actors.size();) {
      const Actor& a = gm.actors[k];
      if (a.npc && a.fromMap && a.site == si && a.role == Role::Guard && a.realm != owner && !(gm.mode == Mode::Dialogue && gm.dlg.actor == a.id))
        gm.actors.erase(gm.actors.begin() + (std::ptrdiff_t)k);
      else k++;
    }
    for (size_t k = 0; k < gm.sheltered_.size();)
      if (gm.sheltered_[k].site == si && gm.sheltered_[k].role == Role::Guard && gm.sheltered_[k].realm != owner) gm.sheltered_.erase(gm.sheltered_.begin() + (std::ptrdiff_t)k);
      else k++;
    // the townsfolk take up arms (or put them down) by the new state of things: a place left with no kingdom's watch
    // arms most of its people (the same rule as Game::spawnHuman's, keyed by the same identity)
    auto rearm = [&](Actor& a) {
      if (!a.npc || !a.fromMap || a.site != si || a.role == Role::Guard || !a.human) return;
      const uint64_t key = ((uint64_t)(a.site + 1) << 24) ^ ((uint64_t)(a.bldg + 1) << 12) ^ (uint64_t)a.slot;
      warArmMilitia(gm, a, key);
    };
    for (size_t k = 1; k < gm.actors.size(); k++) rearm(gm.actors[k]);
    for (Actor& a : gm.sheltered_) rearm(a);
  }
  // ---- 3. the camps' people (checked twice a second)
  gm.war.campT -= dt;
  if (gm.war.campT <= 0) {
    gm.war.campT = 0.5f;
    for (WarCamp& c : gm.war.camps) {
      const int cx = c.gx - gm.world.ox, cy = c.gy - gm.world.oy;
      const float d = std::hypot(cx + 0.5f - ptx, cy + 0.5f - pty);
      const uint64_t key = campKey(c);
      std::vector<int>& ids = gm.war.campPeople[key];
      // drop the dead and the gone
      ids.erase(std::remove_if(ids.begin(), ids.end(), [&](int id) { int i = findId(id); return i < 0 || gm.actors[(size_t)i].st == AState::Dead; }), ids.end());
      if (d > 70) {   // far: put them away (they come back when the player does)
        for (int id : ids) { int i = findId(id); if (i > 0 && !(gm.mode == Mode::Dialogue && gm.dlg.actor == id)) gm.actors.erase(gm.actors.begin() + i); }
        ids.clear();
        continue;
      }
      if (d > 52 && ids.empty()) continue;
      const int si = gm.world.siteHandle(c.site);
      const int level = std::max(1, gm.world.zoneLevel(cx, cy));
      if (c.kind == CampKind::Siege) {
        const realm::Siege* sg = siegeById(gm, c.siege);
        if (!sg || sg->over) continue;
        // the captain at the command tent (one identity per siege: the quest's giver)
        const int capSlot = SLOT_CAPTAIN + (int)(c.siege & 1023) * 4 + 1;
        bool capHere = false;
        for (int id : ids) { int i = findId(id); if (i > 0 && gm.actors[(size_t)i].slot == capSlot) capHere = true; }
        const bool capDead = warCaptainDead(gm, c.siege);
        if (!capHere && !capDead) ids.push_back(spawnMan(Role::Captain, c.kingdom, tileC(c.cmdX - gm.world.ox, c.cmdY - gm.world.oy), capSlot, level + 1, si));
        // the soldiers: 8 in the field while the siege lasts; the fallen are replaced from the tents now and then
        int men = 0;
        for (int id : ids) { int i = findId(id); if (i > 0 && gm.actors[(size_t)i].role == Role::Soldier) men++; }
        float& reinforce = gm.war.reinforceT[key];
        if (reinforce > 0) reinforce -= 0.5f;
        const int want = std::clamp(3 + (int)std::lround(sg->atk), 6, 8);
        const bool first = men == 0 && ids.size() <= 1;
        while (men < want && (first || reinforce <= 0) && !c.posts.empty()) {
          // (fixer M4 r1, review: "one soldier at each tent door, the same pose five times") the first men are spread
          // over the camp's posts from the back (the fire, the palisade's gaps, the catapult) with a step of jitter each,
          // not one per tent door
          const size_t np = c.posts.size();
          const auto& pt = first ? c.posts[(np - 1 - ((size_t)men * 2) % np)] : (c.tents.empty() ? c.posts[0] : c.tents[(size_t)(gm.war.nextSlot % (int)c.tents.size())]);
          const int slot = SLOT_SOLDIER + (gm.war.nextSlot++ % 4000);
          const uint32_t jh = hash32((uint32_t)slot * 2246822519u ^ c.siege);
          const int jx = first ? (int)(jh % 3) - 1 : 0, jy = first ? (int)((jh >> 4) % 3) - 1 : 1;
          ids.push_back(spawnMan(Role::Soldier, c.kingdom, gm.freeSpot(pt.first - gm.world.ox + jx, pt.second - gm.world.oy + jy), slot, level, -1));
          men++;
          if (!first) { reinforce = 7.0f; break; }
        }
        // their orders: hold the camp (a post now and then), or storm the gate beside a player who joined the assault
        bool storm = false;
        for (const Side& sd : sides) if (sd.siege == c.siege && sd.assault) storm = true;
        const Site* S = si >= 0 ? &gm.world.sites[(size_t)si] : nullptr;
        int gateX = S ? S->ex : cx, gateY = S ? S->ey : cy;
        if (S) {   // the opening of the walls nearest the camp
          int bd = 1 << 30;
          for (const IRect& r : gm.world.wallGaps) {
            if (r.x + r.w < S->r.x - 2 || r.y + r.h < S->r.y - 2 || r.x > S->r.x + S->r.w + 2 || r.y > S->r.y + S->r.h + 2) continue;
            const int gx = r.x + r.w / 2, gy = r.y + r.h / 2, dd = std::abs(gx - cx) + std::abs(gy - cy);
            if (dd < bd) { bd = dd; gateX = gx; gateY = gy; }
          }
        }
        const bool near = std::hypot(gateX - ptx, gateY - pty) < 34;
        for (int id : ids) {
          const int i = findId(id);
          if (i <= 0) continue;
          Actor& a = gm.actors[(size_t)i];
          if (a.target >= 0) continue;
          if (a.role == Role::Captain) { a.goal = tileC(c.cmdX - gm.world.ox, c.cmdY - gm.world.oy); a.special = 0.45f; continue; }
          if (storm && near) {
            a.goal = tileC(gateX + (int)(a.id % 3) - 1, gateY + ((gateY > cy) ? -1 : 1) * (1 + (int)(a.id % 2)));
            a.special = 0.85f;
          } else if (len2(a.goal - a.p) < 8.0f * 8.0f && gm.rng_.f() < 0.12f) {
            const auto& pt = c.posts[(size_t)(gm.rng_.next() % c.posts.size())];
            a.goal = tileC(pt.first - gm.world.ox, pt.second - gm.world.oy);
            a.special = 0.42f;
          }
        }
        // the defenders' captain inside (BREAK THE SIEGE), by the gate or at the heart of a village
        if (S && sg->defender) {
          const uint64_t dkey = key ^ 0xDEF0000000000000ull;
          std::vector<int>& dids = gm.war.campPeople[dkey];
          const int dSlot = SLOT_CAPTAIN + (int)(c.siege & 1023) * 4 + 2;
          // (fixer M4 r2) a fallen defenders' captain is not back the next half second: a new one takes his place only
          // after a few minutes (the assault could otherwise farm him at the gate)
          float& dWait = gm.war.reinforceT[dkey];
          if (dWait > 0) dWait -= 0.5f;
          dids.erase(std::remove_if(dids.begin(), dids.end(), [&](int id) {
                       int i = findId(id);
                       if (i >= 0 && gm.actors[(size_t)i].st == AState::Dead) { dWait = 240.0f; return true; }
                       return i < 0;
                     }), dids.end());
          if (dids.empty() && dWait <= 0 && std::hypot(S->ex - ptx, S->ey - pty) < 46) {
            // inside the walls, two steps in from the gate nearest the camp (a village: by its heart)
            int dx = S->ex, dy = S->ey + 2;
            if (gateX != S->ex || gateY != S->ey) { dx = gateX; dy = gateY + (gateY > S->ey ? -3 : 3); }
            dids.push_back(spawnMan(Role::Captain, sg->defender, gm.freeSpot(dx, dy), dSlot, level + 1, si));
            const int i = findId(dids.back());
            if (i > 0) gm.actors[(size_t)i].special = 0.4f;
          }
        }
      } else if (c.kind == CampKind::Checkpoint) {
        // two of the land's soldiers on watch by the barricades
        int men = 0;
        for (int id : ids) if (findId(id) > 0) men++;
        for (int k = men; k < 2 && !c.posts.empty(); k++) {
          const auto& pt = c.posts[(size_t)k % c.posts.size()];
          const int slot = SLOT_SOLDIER + (gm.war.nextSlot++ % 4000);
          ids.push_back(spawnMan(Role::Soldier, c.kingdom, gm.freeSpot(pt.first - gm.world.ox, pt.second - gm.world.oy), slot, level, -1));
          const int i = findId(ids.back());
          if (i > 0) gm.actors[(size_t)i].special = 0.35f;
        }
      } else if (c.kind == CampKind::Refugee) {
        // refugees round the fire, and two on the road between the camp and the burned place
        int sitting = 0, walking = 0;
        for (int id : ids) { int i = findId(id); if (i > 0) (gm.actors[(size_t)i].slot & 1 ? walking : sitting)++; }
        const int wantSit = 4 + (int)(c.site % 3);
        for (int k = sitting; k < wantSit && !c.posts.empty(); k++) {
          const auto& pt = c.posts[(size_t)k % c.posts.size()];
          const int slot = (SLOT_SOLDIER + (gm.war.nextSlot++ % 4000)) & ~1;
          ids.push_back(spawnMan(Role::Refugee, 0, gm.freeSpot(pt.first - gm.world.ox, pt.second - gm.world.oy), slot, level, -1));
        }
        for (int k = walking; k < 2; k++) {
          const int slot = (SLOT_SOLDIER + (gm.war.nextSlot++ % 4000)) | 1;
          const float t = k == 0 ? 0.35f : 0.75f;
          const int wx = (int)std::lround(cx + (c.walkX - gm.world.ox - cx) * t), wy = (int)std::lround(cy + (c.walkY - gm.world.oy - cy) * t);
          ids.push_back(spawnMan(Role::Refugee, 0, gm.freeSpot(wx, wy), slot, level, -1));
          const int i = findId(ids.back());
          if (i > 0) gm.actors[(size_t)i].goal = tileC(k == 0 ? c.walkX - gm.world.ox : cx, k == 0 ? c.walkY - gm.world.oy : cy);
        }
        for (int id : ids) {
          const int i = findId(id);
          if (i <= 0) continue;
          Actor& a = gm.actors[(size_t)i];
          if (a.slot & 1) {   // a walker: to the far end of the road and back to the fire
            a.special = 0.38f;
            if (len2(a.goal - a.p) < 10.0f * 10.0f) {
              const Vec2 far = tileC(c.walkX - gm.world.ox, c.walkY - gm.world.oy), fire = tileC(cx, cy + 2);
              a.goal = len2(far - a.p) < len2(fire - a.p) ? fire : far;
            }
          } else if (len2(a.goal - a.p) < 6.0f * 6.0f && gm.rng_.f() < 0.05f) {
            const auto& pt = c.posts[(size_t)(gm.rng_.next() % c.posts.size())];
            a.goal = tileC(pt.first - gm.world.ox, pt.second - gm.world.oy);
            a.special = 0.3f;
          }
        }
      }
    }
    // the men on a besieged town's walls: its runtime watch posts stand inside the gates
    for (int si : gm.activeSites_) {
      if (si < 0 || si >= (int)gm.world.sites.size()) continue;
      const Site& S = gm.world.sites[(size_t)si];
      const realm::SettlementState* st = gm.realm.settlement(S.id);
      if (!st || !(st->flags & realm::SS_BESIEGED)) continue;
      std::vector<std::pair<int, int>> inner;
      for (const IRect& r : gm.world.wallGaps) {
        if (r.x + r.w < S.r.x - 2 || r.y + r.h < S.r.y - 2 || r.x > S.r.x + S.r.w + 2 || r.y > S.r.y + S.r.h + 2) continue;
        const int gx = r.x + r.w / 2, gy = r.y + r.h / 2;
        const int ix = gx + (r.w <= r.h ? (gx < S.ex ? 2 : -2) : 0), iy = gy + (r.w > r.h ? (gy < S.ey ? 2 : -2) : 0);
        inner.push_back({ix, iy});
      }
      if (inner.empty()) continue;
      int k = 0;
      for (Actor& a : gm.actors) {
        if (!a.npc || a.site != si || a.role != Role::Guard || a.slot < 3900 || a.target >= 0) continue;
        const auto& g = inner[(size_t)(k++) % inner.size()];
        const Vec2 post = tileC(g.first + (k % 2), g.second);
        if (len2(a.home - post) > 4.0f) { a.home = post; a.goal = post; }
      }
    }
    // the player's side: the men of the kingdom they fight are hostile to them; so is a warring realm's patrol when the
    // player's name is mud there (reputation <= -25: VISION_PLAN 4.5)
    for (size_t k = 1; k < gm.actors.size(); k++) {
      Actor& a = gm.actors[k];
      if (!a.npc || !a.human || !a.realm || (a.role != Role::Soldier && a.role != Role::Captain && a.role != Role::Guard)) continue;
      bool h = false;
      for (const Side& sd : sides) {
        if (a.realm != sd.foe) continue;
        if (a.role == Role::Guard) { if (sd.assault && gm.world.sites.size() > (size_t)std::max(0, a.site) && a.site >= 0 && gm.world.sites[(size_t)a.site].id == sd.site) h = true; }
        else h = true;
      }
      // (M12 SERVICE) a sworn soldier is an enemy combatant to the men of every realm at war with his banner, and a
      // deserter is hunted by his old comrades for a while
      if (!h && a.role != Role::Guard && gm.service.enlisted() && a.realm != gm.service.sworn && gm.realm.atWar(a.realm, gm.service.sworn)) h = true;
      if (!h && a.role != Role::Guard && gm.service.huntedBy && a.realm == gm.service.huntedBy && gm.day < (int)gm.service.huntedUntil) h = true;
      if (!h && a.role != Role::Guard && gm.realm.rep(a.realm) <= -25) {
        bool warring = false;
        for (const realm::War& w : gm.realm.wars()) if (!w.endDay && (w.attacker == a.realm || w.defender == a.realm)) warring = true;
        h = warring;
      }
      if (h != a.hostile) {
        a.hostile = h;
        a.target = -1; a.thinkT = 0;
        if (h) gm.emit(Ev::Text, a.p + Vec2(0, -24), (int)rgba(255, 110, 90), 0, "!");
      }
    }
  }
  // ---- 3b. (fixer M4 r1, review: "besiegers pile onto the hero in one unreadable clump") the soldiers keep their
  // distance from each other and from the hero: they ring him at arm's length instead of standing on the same tiles
  {
    std::vector<size_t> men;
    for (size_t k = 1; k < gm.actors.size(); k++) {
      const Actor& a = gm.actors[k];
      if (a.npc && a.human && a.st != AState::Dead && (a.role == Role::Soldier || a.role == Role::Captain) && len2(a.p - P.p) < (14.0f * TILE) * (14.0f * TILE))
        men.push_back(k);
    }
    if (!men.empty()) {
      const float keep = 19.0f, keepP = 18.0f;   // (melee reaches 16 + the body + 2: a ring at 18 still strikes)
      std::vector<Vec2> push(men.size(), Vec2(0, 0));
      for (size_t i = 0; i < men.size(); i++) {
        const Actor& a = gm.actors[men[i]];
        for (size_t j = i + 1; j < men.size(); j++) {
          const Actor& b = gm.actors[men[j]];
          Vec2 d = a.p - b.p;
          float l = len(d);
          if (l >= keep) continue;
          if (l < 0.01f) { d = Vec2((float)((a.id % 3) - 1) + 0.5f, (float)((b.id % 3) - 1)); l = len(d); }
          const Vec2 u = d * (1.0f / l);
          const float o = (keep - l) * 0.5f;
          push[i] += u * o; push[j] -= u * o;
        }
        Vec2 d = a.p - P.p;
        float l = len(d);
        if (l < keepP) {
          if (l < 0.01f) { d = Vec2(1, 0); l = 1; }
          push[i] += d * ((keepP - l) / l);
        }
      }
      for (size_t i = 0; i < men.size(); i++) {
        Vec2 v = push[i];
        const float l = len(v);
        if (l < 0.05f) continue;
        const float mx = 60.0f * dt;   // a shuffle, never a jump
        if (l > mx) v = v * (mx / l);
        gm.moveActor(gm.actors[men[i]], v);
      }
    }
  }
  // ---- 4. road patrols (VISION_PLAN 4.5)
  gm.war.patrolT -= dt;
  for (size_t pi = 0; pi < gm.war.patrols.size();) {
    WarPatrol& pt = gm.war.patrols[pi];
    pt.age += dt;
    pt.ids.erase(std::remove_if(pt.ids.begin(), pt.ids.end(), [&](int id) { int i = findId(id); return i < 0 || gm.actors[(size_t)i].st == AState::Dead; }), pt.ids.end());
    bool far = true;
    for (int id : pt.ids) { const int i = findId(id); if (i > 0 && len2(gm.actors[(size_t)i].p - P.p) < (56.0f * TILE) * (56.0f * TILE)) far = false; }
    const bool done = pt.at >= (int)pt.path.size();
    bool offscreen = true;
    for (int id : pt.ids) { const int i = findId(id); if (i > 0 && len2(gm.actors[(size_t)i].p - P.p) < (30.0f * TILE) * (30.0f * TILE)) offscreen = false; }
    if (pt.ids.empty() || far || (done && offscreen) || (pt.age > 400 && offscreen)) {
      for (int id : pt.ids) { const int i = findId(id); if (i > 0 && !(gm.mode == Mode::Dialogue && gm.dlg.actor == id)) gm.actors.erase(gm.actors.begin() + i); }
      gm.war.patrols.erase(gm.war.patrols.begin() + (std::ptrdiff_t)pi);
      continue;
    }
    static const bool dbgP = std::getenv("EMB_WAR_DEBUG") != nullptr;
    if (dbgP && (int)(pt.age * 60) % 300 == 0) {
      const int li0 = findId(pt.ids[0]);
      if (li0 > 0)
        std::printf("war: patrol of %zu: leader %.0f,%.0f (player %.0f,%.0f), road %d/%zu, target %d, state %d\n", pt.ids.size(),
                    gm.actors[(size_t)li0].p.x / TILE, gm.actors[(size_t)li0].p.y / TILE, P.p.x / TILE, P.p.y / TILE, pt.at, pt.path.size(),
                    gm.actors[(size_t)li0].target, (int)gm.actors[(size_t)li0].st);
    }
    // the leader walks the road; the others keep a stride or two behind him, side by side
    const int li = findId(pt.ids[0]);
    if (li > 0) {
      Actor& L = gm.actors[(size_t)li];
      if (L.target < 0 && !done) {
        // how far along the road he is: the nearest of the next few road tiles (he cuts the corners)
        int best = pt.at;
        float bd = 1e30f;
        for (int k = pt.at; k < std::min((int)pt.path.size(), pt.at + 8); k++) {
          const float d = len2(tileC(pt.path[(size_t)k].first - gm.world.ox, pt.path[(size_t)k].second - gm.world.oy) - L.p);
          if (d < bd) { bd = d; best = k; }
        }
        if (bd < 12.0f * 12.0f) pt.at = std::min((int)pt.path.size(), best + 1);
        // aim a few tiles ahead along the road (smooth corners, fewer path plans)
        const int ahead = std::min((int)pt.path.size() - 1, pt.at + 2);
        L.goal = tileC(pt.path[(size_t)ahead].first - gm.world.ox, pt.path[(size_t)ahead].second - gm.world.oy);
        L.special = 0.5f;
      }
      const Vec2 dir = norm(L.goal - L.p + Vec2(0.001f, 0.0f));
      const Vec2 side(-dir.y, dir.x);
      for (size_t k = 1; k < pt.ids.size(); k++) {
        const int i = findId(pt.ids[k]);
        if (i <= 0) continue;
        Actor& a = gm.actors[(size_t)i];
        if (a.target >= 0) continue;
        const float back = 14.0f * (float)((k + 1) / 2), lat = (k % 2 ? 7.0f : -7.0f);
        a.goal = L.p - dir * back + side * lat;
        a.special = len2(a.goal - a.p) > 30.0f * 30.0f ? 0.75f : 0.55f;
      }
    }
    pi++;
  }
  if (gm.war.patrolT <= 0) {
    gm.war.patrolT = 10.0f;
    const int ptxI = (int)std::floor(ptx), ptyI = (int)std::floor(pty);
    const bool inTown = gm.settlementAt(P.p) >= 0;
    if (gm.war.patrols.size() < 2 && !inTown && (gm.rng_.f() < 0.6f || gm.war.patrolsSpawned == 0)) {
      Map& M = gm.world.over;
      // a road tile in the ring 24-36 tiles out (off screen), out of town: every one round the ring, then one by chance
      int sx = -1, sy = -1;
      {
        std::vector<int> ring;
        for (int r = 24; r <= 36; r += 3)
          for (int k = 0, n = (int)(6.2831853f * r); k < n; k++) {
            const float an = 6.2831853f * k / n;
            const int x = ptxI + (int)std::lround(std::cos(an) * r), y = ptyI + (int)std::lround(std::sin(an) * r);
            if (x < 6 || y < 6 || x >= World::WIN - 6 || y >= World::WIN - 6) continue;
            const Ground gr = M.at(x, y);
            if ((gr != Ground::Road && gr != Ground::Bridge) || M.blocked(x, y) || gm.world.siteAt(x, y, 2) >= 0) continue;
            ring.push_back(y * World::WIN + x);
          }
        if (!ring.empty()) { const int pick = ring[(size_t)(gm.rng_.next() % ring.size())]; sx = pick % World::WIN; sy = pick / World::WIN; }
      }
      ew::Gid K = 0;
      if (sx >= 0) {
        const int32_t gx = gm.world.ox + sx, gy = gm.world.oy + sy;
        K = gm.realm.landOwner(gm.world.src->kingdomAt(gx, gy), gx, gy);
        if (K && !gm.world.kingdomById.count(K)) K = 0;
      }
      static const bool dbg = std::getenv("EMB_WAR_DEBUG") != nullptr;
      if (dbg) std::printf("war: patrol check: road %d,%d (player %d,%d), land %llu\n", sx, sy, ptxI, ptyI, (unsigned long long)K);
      if (K) {
        // the road from here: a breadth-first walk along road and bridge tiles; the patrol walks to the far end
        const int W = World::WIN;
        std::vector<int> prev((size_t)W * W, -2);
        std::vector<int> q;
        q.reserve(4096);
        q.push_back(sy * W + sx);
        prev[(size_t)(sy * W + sx)] = -1;
        int far = sy * W + sx, farD = 0, farPass = -1, farPassD = 0;
        std::vector<int> dist;
        std::vector<uint8_t> passes;   // the road from the start to here goes by the player (within 6 tiles)
        dist.reserve(4096);
        passes.reserve(4096);
        dist.push_back(0);
        passes.push_back(std::abs(sx - ptxI) + std::abs(sy - ptyI) <= 6 ? 1 : 0);
        static const int ddx[4] = {1, -1, 0, 0}, ddy[4] = {0, 0, 1, -1};
        for (size_t h = 0; h < q.size() && q.size() < 6000; h++) {
          const int cx = q[h] % W, cy = q[h] / W;
          for (int k = 0; k < 4; k++) {
            const int nx = cx + ddx[k], ny = cy + ddy[k];
            if (nx < 4 || ny < 4 || nx >= W - 4 || ny >= W - 4) continue;
            const size_t ni = (size_t)(ny * W + nx);
            if (prev[ni] != -2) continue;
            const Ground gr = M.at(nx, ny);
            if ((gr != Ground::Road && gr != Ground::Bridge && gr != Ground::Plaza) || M.blocked(nx, ny)) continue;
            prev[ni] = q[h];
            q.push_back((int)ni);
            dist.push_back(dist[h] + 1);
            passes.push_back((uint8_t)(passes[h] || std::abs(nx - ptxI) + std::abs(ny - ptyI) <= 6));
            // the far end of a road that goes by the player (they walk past the player), else the farthest end
            if (dist.back() <= 140 && dist.back() > farD) { farD = dist.back(); far = (int)ni; }
            if (dist.back() <= 140 && passes.back() && dist.back() > farPassD) { farPassD = dist.back(); farPass = (int)ni; }
          }
        }
        if (dbg) std::printf("war: patrol road reach %zu tiles, far end %d\n", q.size(), farD);
        if (farPass >= 0 && farPassD >= 20) { far = farPass; farD = farPassD; }
        if (farD >= 20) {
          std::vector<std::pair<int32_t, int32_t>> path;
          for (int c2 = far; c2 >= 0; c2 = prev[(size_t)c2]) path.push_back({gm.world.ox + c2 % W, gm.world.oy + c2 / W});
          std::reverse(path.begin(), path.end());
          WarPatrol pt;
          pt.kingdom = K;
          pt.path = std::move(path);
          const int n = 2 + (int)(gm.rng_.next() % 3);
          const int level = std::max(1, gm.world.zoneLevel(sx, sy));
          for (int k = 0; k < n; k++) {
            const auto& t = pt.path[(size_t)std::min((int)pt.path.size() - 1, k)];
            const int slot = SLOT_SOLDIER + (gm.war.nextSlot++ % 4000);
            pt.ids.push_back(spawnMan(Role::Soldier, K, gm.freeSpot(t.first - gm.world.ox, t.second - gm.world.oy), slot, level, -1));
          }
          std::reverse(pt.ids.begin(), pt.ids.end());   // the one spawned furthest along the road leads
          pt.at = std::min((int)pt.path.size() - 1, n);
          gm.war.patrols.push_back(std::move(pt));
          gm.war.patrolsSpawned++;
        }
      }
    }
  }
  // ---- 5. the siege quests: progress is counted in warKill; here the realm's verdict (the siege's resolve day)
  for (Quest& q : gm.quests) {
    if (q.type != QType::War || q.state == QState::Done) continue;
    const realm::Siege* sg = siegeById(gm, (uint32_t)q.stage);
    if (sg && !sg->over) continue;
    const bool assault = (q.flags & QF_WAR_ATTACK) != 0;
    if (!sg && q.state == QState::Active) {
      // (fixer M4 r2) the siege ended and was put away while the player was not about (a long absence: the realm caught
      // up months in one step): the quest ends with it, neither won nor paid, never left open forever
      if (gm.realm.kingdoms().empty()) continue;   // (the realm not built yet)
      const std::string site = q.target >= 0 && q.target < (int)gm.world.sites.size() ? gm.world.sites[(size_t)q.target].name : std::string("THE TOWN");
      q.state = QState::Done;
      q.flags |= QF_FAILED;
      q.desc = "THE SIEGE OF " + site + " ENDED WHILE YOU WERE AWAY.";
      gm.emit(Ev::QuestUpdate, P.p, q.id, 1, "QUEST ENDED: " + q.title);
      gm.say("THE SIEGE OF " + site + " ENDED WHILE YOU WERE AWAY.");
      continue;
    }
    const bool won = sg && assault == sg->attackerWon;
    if (sg && q.state == QState::Active) {
      const std::string site = q.target >= 0 && q.target < (int)gm.world.sites.size() ? gm.world.sites[(size_t)q.target].name : std::string("THE TOWN");
      char buf[200];
      std::snprintf(buf, sizeof buf, "%s %.1f AGAINST %.1f", won ? (assault ? "IT FELL:" : "IT HELD:") : "LOST:", assault ? sg->atk : sg->def, assault ? sg->def : sg->atk);
      if (won) {
        q.state = QState::Complete;
        q.desc = std::string(assault ? "THE ASSAULT TOOK " : "THE SIEGE OF ") + site + (assault ? "." : " IS BROKEN.") + " YOUR STEEL COUNTED: " + buf;
        gm.emit(Ev::QuestUpdate, P.p, q.id, 2, std::string(assault ? site + " HAS FALLEN" : "THE SIEGE OF " + site + " IS BROKEN") + ": RETURN TO " + q.giverName);
        gm.say(assault ? site + " HAS FALLEN!" : "THE SIEGE OF " + site + " IS BROKEN!");
      } else {
        q.state = QState::Done;
        q.flags |= QF_FAILED;
        q.desc = std::string("THE SIEGE ENDED AGAINST YOUR SIDE. ") + buf;
        gm.emit(Ev::QuestUpdate, P.p, q.id, 1, "QUEST FAILED: " + q.title);
        gm.say(assault ? site + " HELD AGAINST THE ASSAULT." : site + " HAS FALLEN TO THE BESIEGERS.");
      }
    }
    // (fixer M4 r1) a won siege quest whose giver is not here to pay: the camp breaks up when the siege ends and its
    // captain is never seen again, so his rider brings the reward (the captain still standing near pays in person)
    if (q.state == QState::Complete) {
      bool giverNear = false;
      for (size_t k = 1; k < gm.actors.size(); k++) {
        const Actor& a = gm.actors[k];
        if (a.npc && a.slot == q.giverSlot && a.role == Role::Captain && a.st != AState::Dead && len2(a.p - P.p) < (40.0f * TILE) * (40.0f * TILE)) giverNear = true;
      }
      if (!giverNear) {
        gm.say("A RIDER FROM " + q.giverName + " FINDS YOU WITH YOUR PAY: " + std::to_string(q.gold) + " GOLD.");
        gm.completeQuest(q);
      }
    }
  }
  const double ms = nowMs() - t0;
  gm.war.steps++;
  gm.war.stepMs += ms;
  gm.war.worstStepMs = std::max(gm.war.worstStepMs, ms);
}

void WarOps::talk(Game& g, Actor& a) {
  if (!a.npc || !a.human) return;
  auto findSiege = [&](ew::Gid k, bool attacker) -> const realm::Siege* {
    for (const realm::Siege& s : g.realm.sieges())
      if (!s.over && (attacker ? s.attacker == k : s.defender == k)) {
        // the siege this man belongs to: the one whose site he stands by (a camp 14-30 tiles out counts)
        const int si = g.world.siteHandle(s.site);
        if (si < 0) continue;
        const Site& S = g.world.sites[(size_t)si];
        if (std::hypot(S.ex - a.p.x / TILE, S.ey - a.p.y / TILE) < std::max(S.r.w, S.r.h) * 0.5f + 46) return &s;
      }
    return nullptr;
  };
  auto activeOn = [&](uint32_t siege) -> Quest* {
    for (Quest& q : g.quests) if (q.type == QType::War && q.stage == (int)siege && q.state != QState::Done) return &q;
    return nullptr;
  };
  if (a.role == Role::Captain && a.realm) {
    // (M12 SERVICE) a town's captain of the watch (service_orders.cpp) is never a besieger; with no siege at his walls
    // his talk is the army's (ServiceOps::talk: enlisting, reports)
    const bool watch = ServiceOps::isServiceCaptain(a);
    const bool att = !watch && a.slot >= SLOT_CAPTAIN && a.slot < SLOT_SOLDIER && (a.slot - SLOT_CAPTAIN) % 4 == 1;
    const realm::Siege* sg = findSiege(a.realm, att);
    if (!sg && watch) return;
    if (!sg) { speech::propose(g.dlg, speech::P_NEWS, speech::Topic::War, "THE WAR IS A SLOW BUSINESS, FRIEND. WE WAIT FOR ORDERS."); return; }
    const int si = g.world.siteHandle(sg->site);
    const std::string site = si >= 0 ? g.world.sites[(size_t)si].name : std::string("THE TOWN");
    const std::string foe = kname(g, att ? sg->defender : sg->attacker);
    Quest* q = activeOn(sg->id);
    if (q && q->state == QState::Active) {
      // (M7b) what is still to do, in the captain's words (q.desc stays the journal's)
      if (((q->flags & QF_WAR_ATTACK) != 0) == att)
        speech::propose(g.dlg, speech::P_QUEST, speech::Topic::QuestActive,
                        att ? "YOU'RE WITH US, THEN. MAKE FOR THE GATE OF " + site + " WHEN YOU'RE READY. MY MEN WILL FOLLOW YOU IN."
                            : "YOU'RE WITH US, THEN. THEIR CAMP LIES OUTSIDE THE WALLS. KILL THEIR CAPTAIN AND THE SIEGE BREAKS.");
      else speech::propose(g.dlg, speech::P_URGENT, speech::Topic::War, "YOU STAND WITH " + foe + "? THEN WE HAVE NOTHING TO SAY.");
      return;
    }
    if (q) return;   // a reward waits (the built-in COLLECT option is already there)
    char odds[64];
    std::snprintf(odds, sizeof odds, "%.0f MEN AGAINST %.0f", (att ? sg->atk : sg->def) * 25, (att ? sg->def : sg->atk) * 25);
    if (att) {
      speech::propose(g.dlg, speech::P_URGENT, speech::Topic::War,
                      "WE HAVE " + site + " IN A RING OF STEEL, " + std::string(odds) + ". " + foe + " WILL OPEN ITS GATES OR WATCH THEM BURN. A SWORD LIKE YOURS WOULD BE PAID WELL.");
      g.dlg.opts.push_back({"JOIN THE ASSAULT", DLG_ASSAULT, (int)sg->id});
    } else {
      const bool walled = si >= 0 && g.world.sites[(size_t)si].type != SiteType::Village;
      speech::propose(g.dlg, speech::P_URGENT, speech::Topic::War,
                      foe + (walled ? " HAS CAMPED AT OUR WALLS, " : " HAS CAMPED OUTSIDE OUR VILLAGE, ") + std::string(odds) + ". BREAK THEIR CAMP, KILL THEIR CAPTAIN, AND " +
                          site + " WILL NOT FORGET YOU.");
      g.dlg.opts.push_back({"BREAK THE SIEGE", DLG_BREAK, (int)sg->id});
    }
    return;
  }
  if (a.role == Role::Soldier) { speech::propose(g.dlg, speech::P_SMALLTALK, speech::Topic::War, soldierGreeting(g, a)); return; }
  if (a.role == Role::Refugee) {
    const char* lines[3] = {"WE LOST EVERYTHING. THE ROOF CAME DOWN BEFORE WE COULD SAVE THE GRAIN.",
                            "THEY CAME AT NIGHT WITH TORCHES. WE RAN WITH WHAT WE COULD CARRY.",
                            "THE PEOPLE HERE ARE KIND, BUT THERE ISN'T BREAD ENOUGH FOR ALL OF US."};
    std::string from;
    for (const WarCamp& c : g.war.camps)
      if (c.kind == CampKind::Refugee && std::hypot(c.gx - g.world.ox - a.p.x / TILE, c.gy - g.world.oy - a.p.y / TILE) < 40) {
        const int fh = g.world.siteHandle(c.other);
        if (fh >= 0) from = g.world.sites[(size_t)fh].name;
      }
    speech::propose(g.dlg, speech::P_NEED, speech::Topic::Need, (from.empty() ? std::string() : "WE COME FROM " + from + ". ") + lines[hash32((uint32_t)a.id) % 3]);
    return;
  }
  // a guard of a realm at war has news of the front (VISION_PLAN 4.6: guards talk war news)
  if (a.role == Role::Guard && a.realm) {
    for (const realm::War& w : g.realm.wars()) {
      if (w.endDay || (w.attacker != a.realm && w.defender != a.realm)) continue;
      const ew::Gid foe = w.attacker == a.realm ? w.defender : w.attacker;
      if (hash32((uint32_t)a.id ^ (uint32_t)g.day * 977u) % 10 < 6)
        speech::propose(g.dlg, speech::P_NEWS, speech::Topic::War,
                        "WE'RE AT WAR WITH " + kname(g, foe) + (w.name.empty() ? std::string(". ") : ". THEY'RE CALLING IT " + w.name + ". ") +
                            "KEEP YOUR EYES OPEN ON THE ROADS, AND YOUR BLADE CLOSE.");
      break;
    }
  }
}

bool WarOps::choose(Game& g, const DlgOpt& o) {
  if (o.action != DLG_ASSAULT && o.action != DLG_BREAK && o.action != DLG_WARNEWS) return false;
  if (o.action == DLG_WARNEWS) { g.mode = Mode::Play; return true; }
  const realm::Siege* sg = siegeById(g, (uint32_t)o.arg);
  const int ai = g.findActor(g.dlg.actor);
  if (!sg || sg->over || ai < 0) { g.dlg.text = "IT'S OVER. TOO LATE FOR THAT NOW."; g.dlg.opts = {{"FAREWELL.", A_BYE, 0}}; return true; }
  const Actor& cap = g.actors[(size_t)ai];
  const bool assault = o.action == DLG_ASSAULT;
  const int si = g.world.siteHandle(sg->site);
  const std::string site = si >= 0 ? g.world.sites[(size_t)si].name : std::string("THE TOWN");
  const int level = si >= 0 ? std::max(1, g.world.sites[(size_t)si].level) : g.plLevel;
  Quest q;
  q.type = QType::War;
  q.title = assault ? "THE ASSAULT ON " + site : "BREAK THE SIEGE OF " + site;
  q.need = assault ? (si >= 0 && g.world.sites[(size_t)si].type == SiteType::Village ? 6 : 8) : BREAK_NEED;
  q.have = 0;
  q.desc = assault ? "SLAY " + std::to_string(q.need) + " OF " + kname(g, sg->defender) + "'S DEFENDERS AT THE WALLS AND THE GATE WILL FALL."
                   : "SLAY " + std::to_string(q.need) + " OF " + kname(g, sg->attacker) + "'S BESIEGERS AND THEIR CAPTAIN.";
  q.giverName = cap.name;
  q.giverSite = cap.site; q.giverBldg = -1; q.giverSlot = cap.slot;
  q.giverId = si >= 0 ? g.world.sites[(size_t)si].id : 0;
  q.target = si;
  q.targetId = sg->site;
  q.stage = (int)sg->id;
  // rewards scale with the danger: the place's level against the player's (VISION_PLAN 4.7)
  const bool danger = level > g.plLevel + 2;
  q.gold = (int)((80 + level * 14) * (danger ? 1.5f : 1.0f));
  q.xp = 90 + level * 18;
  if (danger) q.flags |= QF_DANGER;
  if (assault) q.flags |= QF_WAR_ATTACK;
  if (const WarCamp* c = warCampOf(g, sg->id)) { q.hasPos = true; q.tgx = assault ? g.world.ox + (si >= 0 ? g.world.sites[(size_t)si].ex : 0) : c->gx; q.tgy = assault ? g.world.oy + (si >= 0 ? g.world.sites[(size_t)si].ey : 0) : c->gy; }
  g.acceptQuest(q);
  g.trackedQuest = g.quests.back().id;   // (fixer M4 r1) the war the player just joined leads the HUD and the compass
  g.deed(party::Deed::JoinedWar, assault ? sg->defender : sg->attacker);   // M8: the side fought against
  g.war.runs.push_back(WarQuestRun{g.quests.back().id, false, sg->id});
  // the defenders let their champion in by the nearest gate; the besiegers storm with the player (warStep)
  if (!assault && si >= 0 && std::find(g.war.gatesOpen.begin(), g.war.gatesOpen.end(), sg->site) == g.war.gatesOpen.end()) {
    g.war.gatesOpen.push_back(sg->site);
    g.war.dirty = true;
  }
  g.dlg.text = assault ? "GOOD. WHEN YOU'RE READY, MAKE FOR THE GATE. MY MEN WILL FOLLOW YOU IN." : "THE GODS SENT YOU. THEIR CAMP LIES OUTSIDE THE WALLS. THE GATE IS OPEN FOR YOU.";
  g.dlg.opts = {{"FAREWELL.", A_BYE, 0}};
  return true;
}

void WarOps::kill(Game& g, const Actor& victim, int killer) {
  if (!victim.npc || !victim.human) return;
  const bool byPlayer = killer == g.pl().id;
  if (!byPlayer || !victim.realm) return;
  for (Quest& q : g.quests) {
    if (q.type != QType::War || q.state != QState::Active) continue;
    const realm::Siege* sg = siegeById(g, (uint32_t)q.stage);
    if (!sg || sg->over) continue;
    const bool assault = (q.flags & QF_WAR_ATTACK) != 0;
    const ew::Gid foe = assault ? sg->defender : sg->attacker, friendK = assault ? sg->attacker : sg->defender;
    if (victim.realm != foe) continue;
    if (assault && victim.role != Role::Guard && victim.role != Role::Captain && victim.role != Role::Soldier) continue;
    WarQuestRun* run0 = nullptr;
    for (WarQuestRun& r : g.war.runs) if (r.quest == q.id) run0 = &r;
    if (assault && victim.role == Role::Captain && run0 && run0->defCaptainCounted) continue;   // (fixer M4 r2) once per assault
    // each enemy of the siege slain: 0.5 strength to the player's side, reputation with both (VISION_PLAN 4.7)
    g.realm.contribute(sg->id, assault ? 1 : 2, 0.5f);
    g.realm.addRep(friendK, 1);
    g.realm.addRep(foe, -2);
    WarQuestRun* run = nullptr;
    for (WarQuestRun& r : g.war.runs) if (r.quest == q.id) run = &r;
    if (!run) { g.war.runs.push_back(WarQuestRun{q.id, false, sg->id}); run = &g.war.runs.back(); }
    if (victim.role == Role::Captain && !assault) { run->captainDead = true; q.flags |= QF_WAR_CAPDEAD; }
    if (victim.role == Role::Captain && assault) run->defCaptainCounted = true;
    if (q.flags & QF_WAR_CAPDEAD) run->captainDead = true;
    // (fixer M4 r3) every non-captain kill counts, before or after the captain falls (the captain is the bonus, not a tally)
    if (assault || victim.role != Role::Captain) q.have = std::min(q.need, q.have + 1);
    const bool done = q.have >= q.need && (assault || run->captainDead);
    char line[80];
    std::snprintf(line, sizeof line, "%d/%d%s", q.have, q.need, (!assault && run->captainDead) ? " + CAPTAIN" : "");
    g.emit(Ev::Text, victim.p + Vec2(0, -26), (int)rgba(255, 220, 120), 0, line);
    const std::string site = q.target >= 0 && q.target < (int)g.world.sites.size() ? g.world.sites[(size_t)q.target].name : std::string("THE TOWN");
    q.desc = std::string(assault ? "DEFENDERS SLAIN " : "BESIEGERS SLAIN ") + std::to_string(q.have) + "/" + std::to_string(q.need) +
             (assault ? "" : (run->captainDead ? ", THEIR CAPTAIN DEAD" : ", THEIR CAPTAIN STILL STANDS")) + ".";
    if (done) {
      q.state = QState::Complete;
      g.realm.addRep(friendK, 8);
      g.realm.addRep(foe, -8);
      if (assault) {   // the gate falls: the barricades come down
        if (std::find(g.war.gatesOpen.begin(), g.war.gatesOpen.end(), sg->site) == g.war.gatesOpen.end()) g.war.gatesOpen.push_back(sg->site);
        g.war.dirty = true;
      }
      const std::string m = assault ? "THE GATE OF " + site + " IS OPEN! RETURN TO " + q.giverName : "THE CAMP IS BROKEN! RETURN TO " + q.giverName;
      g.emit(Ev::QuestUpdate, victim.p, q.id, 2, m);
      g.say(m);
      g.sfx((int)Sfx::QuestStart, victim.p, 1.2f);
    }
  }
}
