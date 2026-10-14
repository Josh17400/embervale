// rpg_test --skills: the M9 learn-by-use skill system (VISION_PLAN 15.29.1, 15.29.2, 15.29.4, 15.29.5). SKILLS lane.
//   - the tables: every skill named and worded; every perk tree well formed for the constellation UI (needs within
//     1..100, parents earlier, no two stars on one spot), 6-10 perks for the weapons and armour, 3-6 for the others
//   - the curve rises (gains slow), and a play-rate bot measures the use points an hour of play gives each skill:
//     combat in a real arena (blows landed, blows taken, time worn in a fight), travel on foot, sneaking near the unaware,
//     and the crafts and trade at a modelled pace (their acts timed by the real hooks); every skill masters in 10-20 h
//   - foe tags for the whole roster; the weapon families' vs rows; an A/B time-to-kill table (each family against the
//     M6 roster at equal item level) with the owner's wins and losses asserted
//   - the armour classes' trade-offs measured in play: walking speed, the roll's reach and cost, stamina coming back,
//     how near a sneaking hero gets to an unaware wolf, the damage a blow does
//   - gains only from their own act (a sword blow, an arrow, a blow taken, a walk, a trade, a forge, a meal, a field)
//   - every perk applied (a probe for each: its number or its behaviour, off against on) and saved
//   - trainers (census jobs, the cap per level), books (read once; loot rates by source), companions teaching, respec
//   - the skills block, a save's round trip, old saves' defaults by level
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <set>
#include <string>
#include <vector>
#include "rpg/sim/craft.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/party.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/skills.h"
#include "tools/tests/tests.h"

namespace {

using skills::Skill;
using skills::Weight;
using namespace skills::perk;

int s_bad = 0;
#define SK_FAIL(...) do { out("FAIL: skills: " __VA_ARGS__); s_bad++; } while (0)

const char* nm(Skill s) { return skills::skillInfo(s).name; }

void skRun(Game& g, float secs) {
  const int n = (int)std::lround(secs / SIM_DT);
  for (int f = 0; f < n; f++) { g.update(SIM_DT, Input()); g.events.clear(); if (g.mode == Mode::LevelUp) g.mode = Mode::Play; }
}

// ---------------------------------------------------------------- a quiet field and a kit
bool fieldSpot(Game& g, int& ax, int& ay) {
  // dry, mostly open country well away from every site (no townsfolk come by); freshGame clears what is left
  const Map& m = g.world.over;
  const Site& home = g.world.sites[g.world.startSite];
  int best = -1;
  float bd = 1e30f;
  for (int y = 14; y < m.h - 14; y += 2)
    for (int x = 18; x < m.w - 18; x += 2) {
      const Biome b = m.biomeAt(x, y);
      if (b == Biome::Ocean || b == Biome::Beach || b == Biome::Mountain || b == Biome::Swamp) continue;
      if (g.world.siteAt(x, y, 30) >= 0) continue;
      int wet = 0;
      for (int oy = -10; oy <= 10; oy += 2)
        for (int ox = -14; ox <= 14; ox += 2) {
          const Ground gr = m.at(x + ox, y + oy);
          if (gr == Ground::Water || gr == Ground::DeepWater || gr == Ground::Rock || gr == Ground::Lava || m.biomeAt(x + ox, y + oy) == Biome::Ocean) wet++;
        }
      if (wet) continue;
      const float d = std::hypot((float)(x - home.r.cx()), (float)(y - home.r.cy()));
      if (d < bd) { bd = d; best = y * m.w + x; }
    }
  if (best < 0) return false;
  ax = best % m.w; ay = best / m.w;
  return true;
}
Vec2 g_field;
uint64_t g_cult = 0;   // the start settlement's people (a real culture for the Heritage probes)
bool g_haveField = false;
// one step of play; the field follows the endless window when it shifts (Ev::WindowShift's offset)
void tick(Game& g, const Input& in) {
  g.pickups.clear();   // (nothing dropped is picked up: a bandit's blade would arm a bare-handed hero mid-measure)
  g.update(SIM_DT, in);
  for (const Event& e : g.events) if (e.type == Ev::WindowShift) g_field += e.p;
}
// a fresh game standing in an open field (nobody near), daylight, play mode
void freshGame(Game& g, uint64_t seed) {
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.hour = 12;
  int ax = 0, ay = 0;
  g_haveField = fieldSpot(g, ax, ay);
  if (g_haveField) g.pl().p = Vec2(ax * TILE + 8.0f, ay * TILE + 8.0f);
  if (!g_haveField) out("FAIL: skills: no open field for the measures (seed %llu)\n",(unsigned long long)seed);
  // let the endless window settle round the new place (it may recentre a few times): until a second passes quietly
  for (int quiet = 0, f = 0; quiet < 60 && f < 60 * 20; f++) {
    g.update(SIM_DT, Input());
    bool shifted = false;
    for (const Event& e : g.events) shifted |= e.type == Ev::WindowShift;
    g.events.clear();
    g.mode = Mode::Play;
    g.actors.resize(1);
    quiet = shifted ? 0 : quiet + 1;
  }
  g.actors.resize(1);
  g_field = g.pl().p;
  // a clean stage: open grass, level, no props, walls or buildings 14 tiles round (the measures need the room)
  Map& m = g.world.over;
  const int cx = (int)std::floor(g_field.x / TILE), cy = (int)std::floor(g_field.y / TILE);
  for (int y = cy - 10; y <= cy + 10; y++)
    for (int x = cx - 14; x <= cx + 14; x++) {
      if (!m.in(x, y)) continue;
      const size_t i = (size_t)y * m.w + x;
      m.setG(x, y, Ground::Grass);
      m.setP(x, y, 0);
      if (i < m.wall.size()) m.wall[i] = 0;
      if (i < m.bldgAt.size()) m.bldgAt[i] = -1;
      if (i < m.solid.size()) m.solid[i] = 0;
      if (i < m.height.size()) m.height[i] = (uint8_t)(m.height[(size_t)cy * m.w + cx] & ~(Map::HEIGHT_CLIFF | Map::HEIGHT_RAMP));
    }
  g_cult = g.world.sites[(size_t)g.world.startSite].culture;
}
WeaponType typeOf(Skill f) {
  switch (f) {
    case Skill::Axe: return WeaponType::Axe;
    case Skill::Mace: return WeaponType::Mace;
    case Skill::Spear: return WeaponType::Spear;
    case Skill::Dagger: return WeaponType::Dagger;
    default: return WeaponType::Sword;
  }
}
Mat matOf(Weight w) { return w == Weight::Light ? Mat::Leather : w == Weight::Medium ? Mat::Bronze : Mat::Iron; }
// the hero at level L with a weapon of the family (Bow: a bow and arrows, no blade; Unarmed: bare hands), a full set of
// one armour class (None: a shirt), a shield or not; every skill at its start level and no perks unless kept
void kit(Game& g, Skill fam, Weight w, bool shield, int L, uint64_t culture = 0, bool keepSkills = false) {
  Rng r(77u + (uint32_t)fam * 13u + (uint32_t)w);
  g.inv.clear();
  g.eqWeapon = g.eqBow = g.eqStaff = g.eqArmor = g.eqHelmet = g.eqShield = g.eqRing = g.eqAmulet = g.eqGloves = g.eqBoots = g.eqCloak = -1;
  g.plLevel = L;
  if (fam == Skill::Bow) {
    g.inv.push_back(gear::makeGear(r, ItemKind::Bow, 0, L, Rarity::Common, culture, Mat::Wood)); g.eqBow = (int)g.inv.size() - 1;
    g.inv.push_back(makeArrows(999));
  } else if (fam != Skill::Unarmed) {
    g.inv.push_back(gear::makeGear(r, ItemKind::Weapon, (int)typeOf(fam), L, Rarity::Common, culture, Mat::Iron)); g.eqWeapon = (int)g.inv.size() - 1;
  }
  if (w != Weight::None) {
    const ItemKind ks[] = {ItemKind::Armor, ItemKind::Helmet, ItemKind::Gloves, ItemKind::Boots};
    int* slots[] = {&g.eqArmor, &g.eqHelmet, &g.eqGloves, &g.eqBoots};
    for (int k = 0; k < 4; k++) { g.inv.push_back(gear::makeGear(r, ks[k], 0, L, Rarity::Common, culture, matOf(w))); *slots[k] = (int)g.inv.size() - 1; }
  }
  if (shield) { g.inv.push_back(gear::makeGear(r, ItemKind::Shield, 0, L, Rarity::Common, culture, Mat::Wood)); g.eqShield = (int)g.inv.size() - 1; }
  if (!keepSkills) { skills::Skills fresh; g.skills = fresh; g.skills.live.smithInit = true; }
  g.godMode = false;
  g.mode = Mode::Play;
  g.slowMo = 0; g.hitStop = 0;
  SkillOps::recalc(g);
  g.pl().hp = g.pl().maxHp; g.stamina = g.maxSt;
  g.pl().st = AState::Idle; g.pl().stT = 0; g.pl().iframes = 0; g.pl().knock = Vec2();
}
Actor& actorById(Game& g, int id) { return g.actors[(size_t)SkillOps::findActor(g, id)]; }

// ---------------------------------------------------------------- tables
void checkTables() {
  for (int i = 0; i < skills::SKILLS; i++) {
    const Skill s = (Skill)i;
    const skills::SkillInfo& inf = skills::skillInfo(s);
    if (!inf.name || !*inf.name || !inf.line || !*inf.line) SK_FAIL("skill %d has no name or line\n", i);
    int n = 0;
    const skills::PerkDef* P = skills::perkTable(s, n);
    if (!P || n < 1 || n > skills::MAX_PERKS) SK_FAIL("%s: a tree of %d perks\n", inf.name, n);
    // the owner's sizes: weapons and armour 6-10 perks, the others 3-6 (Lockpicking reserved: no locks yet)
    if (!skills::isMagic(s) && s != Skill::Lockpicking) {
      const bool big = s <= Skill::HeavyArmour;
      if (big && (n < 6 || n > 10)) SK_FAIL("%s has %d perks (6-10 wanted)\n", inf.name, n);
      if (!big && (n < 3 || n > 6)) SK_FAIL("%s has %d perks (3-6 wanted)\n", inf.name, n);
    }
    std::set<int> spots;
    for (int k = 0; k < n; k++) {
      const skills::PerkDef& d = P[k];
      if (!d.name || !*d.name || !d.line || !*d.line) SK_FAIL("%s perk %d has no name or line\n", inf.name, k);
      if (d.need < 1 || d.need > skills::MAX_LEVEL) SK_FAIL("%s perk %s needs level %d\n", inf.name, d.name, d.need);
      if (d.parent >= k || d.parent2 >= k || d.parent < -1 || d.parent2 < -1) SK_FAIL("%s perk %s hangs from a later perk\n", inf.name, d.name);
      if (d.parent >= 0 && P[d.parent].need > d.need) SK_FAIL("%s perk %s needs less than its parent\n", inf.name, d.name);
      if (d.parent2 >= 0 && P[d.parent2].need > d.need) SK_FAIL("%s perk %s needs less than its second parent\n", inf.name, d.name);
      if (d.x < -8 || d.x > 8 || d.y < -8 || d.y > 8) SK_FAIL("%s perk %s off the grid\n", inf.name, d.name);
      if (!spots.insert(d.x * 100 + d.y).second) SK_FAIL("%s perk %s shares a star's spot\n", inf.name, d.name);
      if (d.cost < 1) SK_FAIL("%s perk %s costs nothing\n", inf.name, d.name);
    }
  }
}

void checkCurve(bool table) {
  float prev = 0;
  for (int L = 1; L < skills::MAX_LEVEL; L++) {
    const float x = skills::xpToNext(L);
    if (!(x > prev)) SK_FAIL("the curve does not rise at level %d (%.2f after %.2f)\n", L, x, prev);
    prev = x;
  }
  const float master = skills::xpToReach(skills::START_LEVEL, skills::MAX_LEVEL);
  if (!(master > 0) || !std::isfinite(master)) SK_FAIL("no finite way to mastery (%.1f)\n", master);
  if (table) out("skills: %.0f use points from %d to %d (level 25 %.0f, 50 %.0f, 75 %.0f)\n", master, skills::START_LEVEL, skills::MAX_LEVEL,
                 skills::xpToReach(skills::START_LEVEL, 25), skills::xpToReach(skills::START_LEVEL, 50), skills::xpToReach(skills::START_LEVEL, 75));
  if (skills::charXpForSkillUp(30) <= 0 || skills::charXpForSkillUp(90) <= skills::charXpForSkillUp(30)) SK_FAIL("skill-ups give no rising character XP\n");
}

// ---------------------------------------------------------------- the play-rate bot (15.29.5: ~10-20 h to master)
// The fight: the family's weapon on foes of the roster one after another (a wolf, a skeleton, a bandit), swinging (or
// loosing) as fast as the weapon allows; the hero's own skills rise as they would. Returns use points a second.
float swingRate(Game& g, Skill fam, float secs);
// The blows a fight brings: a wolf and a bandit at the hero, who stands and takes them (health topped up): the worn
// class, the shield and the time worn in a fight all learn. Returns seconds simulated.
float blowsRun(Game& g, float secs) {
  const int a = SkillOps::spawnMonster(g, art::Monster::Wolf, g_field + Vec2(16, 0), 5);
  const int b = SkillOps::spawnPerson(g, Role::Bandit, g_field + Vec2(-16, 0), 5);
  for (int id : {a, b}) actorById(g, id).aggro = true;
  const int n = (int)std::lround(secs / SIM_DT);
  for (int f = 0; f < n; f++) {
    g.pl().p = g_field;
    tick(g, Input());
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
    g.pl().hp = g.pl().maxHp;
    for (int id : {a, b}) { const int k = SkillOps::findActor(g, id); if (k >= 0) { Actor& e = g.actors[(size_t)k]; e.hp = e.maxHp; if (len2(e.p - g_field) > 40 * 40) e.p = g_field + Vec2(id == a ? 16.0f : -16.0f, 0); } }
  }
  return secs;
}

// the share of an hour of ordinary play spent at each kind of act (the model; the acts' points are measured)
constexpr float SHARE_FIGHT = 0.25f;     // a quarter of play is fighting (with the main weapon, in the worn armour)
constexpr float SHARE_FOOT = 0.45f;      // travelling and exploring on foot
constexpr float SHARE_RIDE = 0.15f;      // on horseback, for a rider
constexpr float SHARE_SNEAK = 0.10f;     // creeping up on the unaware, for a sneak
constexpr float ENGAGE = 0.5f;           // of a fight's time, the share spent striking (the rest closing in, dodging, waiting)

void checkMastery(uint64_t seed, bool table) {
  const float need = skills::xpToReach(skills::START_LEVEL, skills::MAX_LEVEL);
  float perHour[skills::SKILLS] = {};   // use points an hour of play gives (at each skill's own pace)
  Game g(seed);
  freshGame(g, seed);
  // ---- combat: each family's strikes; the blows a fight brings (the armour classes, Block, Heritage)
  for (Skill f : {Skill::Sword, Skill::Axe, Skill::Mace, Skill::Spear, Skill::Dagger, Skill::Bow, Skill::Unarmed})
    perHour[(int)f] = swingRate(g, f, 40.0f) * 3600.0f * SHARE_FIGHT * ENGAGE;
  for (Weight w : {Weight::Light, Weight::Medium, Weight::Heavy}) {
    g.actors.resize(1);
    kit(g, Skill::Sword, w, w == Weight::Light, 5);
    for (float& x : g.skills.live.points) x = 0;
    const float s = blowsRun(g, 60.0f);
    const Skill a = skills::armourSkill(w);
    perHour[(int)a] = g.skills.live.points[(int)a] / s * 3600.0f * SHARE_FIGHT;
    if (w == Weight::Light) perHour[(int)Skill::Block] = g.skills.live.points[(int)Skill::Block] / s * 3600.0f * SHARE_FIGHT;
    if (w == Weight::Medium) perHour[(int)Skill::Heritage] = g.skills.live.points[(int)Skill::Heritage] / s * 3600.0f * SHARE_FIGHT;
  }
  // ---- on foot: a minute's walk across the field (Athletics); a rider's minute from the horse's pace (Riding)
  {
    g.actors.resize(1);
    kit(g, Skill::Sword, Weight::None, false, 5);
    for (float& x : g.skills.live.points) x = 0;
    Input in;
    const int n = (int)std::lround(60.0f / SIM_DT);
    for (int f = 0; f < n; f++) {
      const float a = (float)(f / 240) * 1.7f;
      in.move = Vec2(std::cos(a), std::sin(a));
      if (len2(g.pl().p - g_field) > 90.0f * 90.0f) in.move = norm(g_field - g.pl().p);
      tick(g, in);
      g.events.clear();
      g.mode = Mode::Play;
    }
    perHour[(int)Skill::Athletics] = g.skills.live.points[(int)Skill::Athletics] * 60.0f * SHARE_FOOT;
    const float walkPx = g.pl().speed;   // px a second on foot
    const float ridePx = walkPx * 2.2f;  // (a heartland horse: about 2.2 times the walk, home::breedInfo)
    perHour[(int)Skill::Riding] = ridePx * 3600.0f / (16.0f * TILE) * skills::skillRate(Skill::Riding) * SHARE_RIDE;
  }
  // ---- sneaking near three unaware wolves (Stealth)
  {
    g.actors.resize(1);
    kit(g, Skill::Dagger, Weight::Light, false, 5);
    g.pl().p = g_field;
    for (float& x : g.skills.live.points) x = 0;
    std::vector<int> ids;
    for (int k = 0; k < 3; k++) {
      ids.push_back(SkillOps::spawnMonster(g, art::Monster::Wolf, g_field + Vec2(80.0f + 20.0f * (float)k, 0), 5));
      actorById(g, ids.back()).aggroR = 1.0f;   // (dozing: the measure is the time spent near the unaware)
    }
    g.godMode = true;
    g.skills.live.sneaking = true;
    const int n = (int)std::lround(60.0f / SIM_DT);
    for (int f = 0; f < n; f++) {
      for (int id : ids) { const int k = SkillOps::findActor(g, id); if (k >= 0) { g.actors[(size_t)k].aggro = false; g.actors[(size_t)k].p = g_field + Vec2(80.0f + 20.0f * (float)(id % 3), 0); } }
      Input in;
      in.move = Vec2((f / 120) % 2 ? 0.5f : -0.5f, 0);
      tick(g, in);
      g.events.clear();
    }
    g.skills.live.sneaking = false;
    g.godMode = false;
    perHour[(int)Skill::Stealth] = g.skills.live.points[(int)Skill::Stealth] * 60.0f * SHARE_SNEAK;
  }
  // ---- the crafts and the trade: each act's points from the real hook, at a modelled pace an hour
  auto ptsOf = [&](Skill s, const std::function<void()>& act) {
    for (float& x : g.skills.live.points) x = 0;
    act();
    return g.skills.live.points[(int)s];
  };
  {
    Rng r5(5);
    Item sword = gear::makeGear(r5, ItemKind::Weapon, 0, 5, Rarity::Common, 0, Mat::Iron);
    Item ingot = craft::makeStuff(craft::Stuff::BronzeIngot, 1);
    const float piece = ptsOf(Skill::Smithing, [&] { Item it = sword; SkillOps::forged(g, it); });
    const float smelt = ptsOf(Skill::Smithing, [&] { Item it = ingot; SkillOps::forged(g, it); });
    perHour[(int)Skill::Smithing] = 12.0f * piece + 20.0f * smelt;   // a smith's hour: 12 pieces and 20 smelts
    const float meal = ptsOf(Skill::Cooking, [&] { SkillOps::cooked(g, makeFood(0)); });
    perHour[(int)Skill::Cooking] = 30.0f * meal;                      // a cook's hour: 30 meals (stores and a kitchen)
    const float sow = ptsOf(Skill::Farming, [&] { SkillOps::farmed(g, 1); });
    const float reap = ptsOf(Skill::Farming, [&] { SkillOps::farmed(g, 3); });
    perHour[(int)Skill::Farming] = 45.0f * sow + 20.0f * reap;        // a farmer's hour: 45 tilled, sown, watered; 20 harvests
    const float sale = ptsOf(Skill::Speech, [&] { SkillOps::traded(g, 60); });
    perHour[(int)Skill::Speech] = 30.0f * sale;                       // a trader's hour: 30 deals of about 60 gold
  }
  // ---- the verdict: 10-20 h for every skill but the reserved and the schools (the MAGIC lane's)
  if (table) out("skills: mastery, 5 to 100, at a normal play rate (hours):");
  for (int i = 0; i < skills::SKILLS; i++) {
    const Skill s = (Skill)i;
    if (skills::isMagic(s) || s == Skill::Lockpicking) continue;
    const float h = perHour[i] > 0 ? need / perHour[i] : 1e9f;
    if (table) out(" %s %.1f", nm(s), h);
    if (!(h >= 10.0f && h <= 20.0f)) SK_FAIL("%s masters in %.1f h (10-20 wanted; %.0f points an hour)\n", nm(s), h, perHour[i]);
  }
  if (table) out("\n");
}

// ---------------------------------------------------------------- tags and families
void checkTags() {
  for (int m = 0; m < (int)art::Monster::COUNT; m++) {
    const uint16_t t = skills::tagsOf((art::Monster)m, false, Role::Villager, 0, 0);
    if (!t) SK_FAIL("monster %d has no foe tags\n", m);
    if (t & skills::FT_PEOPLE) SK_FAIL("monster %d is tagged as people\n", m);
  }
  if (!(skills::tagsOf(art::Monster::Wolf, true, Role::Bandit, 0, 0) & skills::FT_PEOPLE)) SK_FAIL("a bandit is not people\n");
  if (!(skills::tagsOf(art::Monster::Wolf, true, Role::Soldier, 0, 0) & skills::FT_ARMOURED)) SK_FAIL("a soldier is not armoured\n");
  if (!(skills::tagsOf(art::Monster::Bear, false, Role::Villager, 0, foes::affixBit(foes::Affix::Armoured)) & skills::FT_ARMOURED)) SK_FAIL("the Armoured affix does not armour\n");
  if (!(skills::tagsOf(art::Monster::Bear, false, Role::Villager, 0, foes::affixBit(foes::Affix::Swift)) & skills::FT_CHARGER)) SK_FAIL("the Swift affix does not make a charger\n");
  if (!(skills::tagsOf(art::Monster::Bear, false, Role::Villager, 0, foes::affixBit(foes::Affix::Warded)) & skills::FT_SHIELDED)) SK_FAIL("the Warded affix does not shield\n");
  for (int b = 0; b < skills::FOE_TAGS; b++) if (!*skills::foeTagName((skills::FoeTag)(1u << b))) SK_FAIL("foe tag %d has no name\n", b);
}

void checkFamilies(bool table) {
  for (Skill f : {Skill::Sword, Skill::Axe, Skill::Mace, Skill::Spear, Skill::Dagger, Skill::Bow, Skill::Unarmed}) {
    int up = 0, down = 0;
    for (const skills::VsRow& r : skills::familyVs(f)) (r.pct > 0 ? up : down)++;
    if (!up || !down) SK_FAIL("%s wins %d and loses %d against foe tags (it must do both)\n", nm(f), up, down);
    if (skills::familyVsLines(f).empty() || !*skills::familyTraits(f).line) SK_FAIL("%s has no words for the UI\n", nm(f));
    if (table) {
      out("skills: %-8s", nm(f));
      for (const std::string& l : skills::familyVsLines(f)) out("  [%s]", l.c_str());
      out("\n");
    }
  }
  if (skills::familyVsPct(Skill::Mace, skills::tagsOf(art::Monster::Skeleton, false, Role::Villager, 0, 0)) <= 0) SK_FAIL("maces do not crush skeletons\n");
  if (skills::familyVsPct(Skill::Spear, skills::tagsOf(art::Monster::Wolf, false, Role::Villager, 0, 0)) <= 0) SK_FAIL("spears do not stop wolves\n");
  if (skills::familyVsPct(Skill::Bow, skills::tagsOf(art::Monster::Wolf, true, Role::Soldier, 0, 0)) >= 0) SK_FAIL("bows are not weak against shields\n");
  if (skills::familyTraits(Skill::Mace).swingTime <= 1.0f) SK_FAIL("maces are not slow\n");
  const skills::ArmourTrade lt = skills::armourTrade(Weight::Light, 20), hv = skills::armourTrade(Weight::Heavy, 20);
  if (!(hv.dr > lt.dr && hv.speed < lt.speed && hv.noise > lt.noise && hv.rollCost > lt.rollCost)) SK_FAIL("the armour classes do not trade off\n");
}

// ---------------------------------------------------------------- the A/B time-to-kill table
struct Foe { const char* name; art::Monster mon; Role role; bool person; };
const Foe kFoes[] = {
    {"WOLF", art::Monster::Wolf, Role::Villager, false},       // beast, charger, unarmoured
    {"BEAR", art::Monster::Bear, Role::Villager, false},       // huge beast
    {"SKELETON", art::Monster::Skeleton, Role::Villager, false},
    {"DRAUGR", art::Monster::Draugr, Role::Villager, false},   // armoured undead
    {"GOLEM", art::Monster::Golem, Role::Villager, false},     // construct, armoured, huge
    {"BANDIT", art::Monster::Wolf, Role::Bandit, true},        // people, unarmoured
    {"SOLDIER", art::Monster::Wolf, Role::Soldier, true},      // people, mail and a shield
};
constexpr int NFOES = (int)(sizeof(kFoes) / sizeof(kFoes[0]));
const Skill kFams[] = {Skill::Sword, Skill::Axe, Skill::Mace, Skill::Spear, Skill::Dagger, Skill::Bow, Skill::Unarmed};
constexpr int NFAMS = 7;

// seconds to kill one pinned, passive foe of level L with the family's weapon of item level L (skills at their start)
float duel(Game& g, Skill fam, const Foe& fo, int L) {
  g.actors.resize(1);
  g.projs.clear();
  kit(g, fam, Weight::None, false, L);
  g.godMode = true;
  const Vec2 off(fam == Skill::Bow ? 80.0f : 13.0f, 0);
  const Vec2 at = g_field + off;
  const int id = fo.person ? SkillOps::spawnPerson(g, fo.role, at, L) : SkillOps::spawnMonster(g, fo.mon, at, L);
  {
    Actor& e = actorById(g, id);
    e.maxHp *= 4.0f; e.hp = e.maxHp;   // (four bodies' worth: a finer measure of the pace)
    e.aggro = true;
  }
  const int n = (int)std::lround(90.0f / SIM_DT);
  for (int f = 0; f < n; f++) {
    const int k = SkillOps::findActor(g, id);
    if (k < 0 || g.actors[(size_t)k].st == AState::Dead) return (float)f * SIM_DT;
    Actor& e = g.actors[(size_t)k];
    e.p = g_field + off; e.knock = Vec2(); e.atkCd = 9.0f; e.aggro = true;
    g.skills[fam].level = skills::START_LEVEL; g.skills[fam].xp = 0;   // (the table is at the start level: no gains mid-duel)
    if (e.st == AState::Windup || e.st == AState::Strike) { e.st = AState::Idle; e.stT = 0; }
    g.pl().p = g_field; g.pl().aim = Vec2(1, 0);
    Input in;
    if (fam == Skill::Bow) in.bow = true; else in.attack = true;
    tick(g, in);
    g.events.clear();
    if (g.mode != Mode::Play) g.mode = Mode::Play;
  }
  return 90.0f;
}

float swingRate(Game& g, Skill fam, float secs) {
  const Foe* mix[] = {&kFoes[0], &kFoes[2], &kFoes[5]};
  g.actors.resize(1);
  g.projs.clear();
  kit(g, fam, Weight::None, false, 5);
  g.godMode = true;
  for (float& x : g.skills.live.points) x = 0;
  float t = 0;
  int k = 0;
  while (t < secs) {
    const Foe& fo = *mix[k++ % 3];
    const Vec2 off(fam == Skill::Bow ? 80.0f : 13.0f, 0);
    const Vec2 at = g_field + off;
    const int id = fo.person ? SkillOps::spawnPerson(g, fo.role, at, 5) : SkillOps::spawnMonster(g, fo.mon, at, 5);
    actorById(g, id).aggro = true;
    for (int f = 0; f < 60 * 30 && t < secs; f++) {
      const int i = SkillOps::findActor(g, id);
      if (i < 0 || g.actors[(size_t)i].st == AState::Dead) break;
      Actor& e = g.actors[(size_t)i];
      e.p = g_field + off; e.knock = Vec2(); e.atkCd = 9.0f; e.aggro = true;
      if (e.st == AState::Windup || e.st == AState::Strike) { e.st = AState::Idle; e.stT = 0; }
      g.pl().p = g_field; g.pl().aim = Vec2(1, 0);
      Input in;
      if (fam == Skill::Bow) in.bow = true; else in.attack = true;
      tick(g, in);
      g.events.clear();
      if (g.mode != Mode::Play) g.mode = Mode::Play;
      t += SIM_DT;
    }
    g.actors.resize(1);
  }
  g.godMode = false;
  return g.skills.live.points[(int)fam] / std::max(1.0f, t);
}

void checkTTK(uint64_t seed, bool table) {
  Game g(seed);
  freshGame(g, seed);
  constexpr int L = 10;
  float t[NFAMS][NFOES] = {};
  for (int fi = 0; fi < NFAMS; fi++)
    for (int ei = 0; ei < NFOES; ei++) { for (int k = 0; k < 4; k++) t[fi][ei] += 0.25f * duel(g, kFams[fi], kFoes[ei], L); }
  if (table) {
    out("skills: time to kill (s), item level %d, skills at %d (each foe x4 health, pinned, 4 duels each):\n  %-8s", L, skills::START_LEVEL, "");
    for (const Foe& fo : kFoes) out(" %9s", fo.name);
    out("\n");
    for (int fi = 0; fi < NFAMS; fi++) {
      out("  %-8s", nm(kFams[fi]));
      for (int ei = 0; ei < NFOES; ei++) out(" %9.1f", t[fi][ei]);
      out("\n");
    }
  }
  auto T = [&](Skill f, const char* foe) {
    int fi = 0, ei = 0;
    for (int k = 0; k < NFAMS; k++) if (kFams[k] == f) fi = k;
    for (int k = 0; k < NFOES; k++) if (!std::strcmp(kFoes[k].name, foe)) ei = k;
    return t[fi][ei];
  };
  // how much better (> 1) or worse (< 1) a family fares against a foe than against its own average, the sword's
  // pace taken out: the edge the vs rows give
  auto edge = [&](Skill f, const char* foe) {
    double lg = 0;
    for (const Foe& fo : kFoes) lg += std::log(T(Skill::Sword, fo.name) / std::max(0.1f, T(f, fo.name)));
    return (T(Skill::Sword, foe) / std::max(0.1f, T(f, foe))) / std::exp(lg / NFOES);
  };
  auto win = [&](Skill f, const char* foe, bool wins) {
    const bool ok = wins ? T(f, foe) < T(Skill::Sword, foe) : T(f, foe) > T(Skill::Sword, foe);
    if (!ok) SK_FAIL("%s should %s the sword against a %s (%.1f s against %.1f s)\n", nm(f), wins ? "beat" : "lose to", foe, T(f, foe), T(Skill::Sword, foe));
  };
  // maces crush the armoured and the bones, but are slow on the nimble
  win(Skill::Mace, "SKELETON", true); win(Skill::Mace, "DRAUGR", true); win(Skill::Mace, "SOLDIER", true); win(Skill::Mace, "GOLEM", true);
  win(Skill::Mace, "WOLF", false);
  // axes cleave beasts and the unarmoured, catch on plate
  win(Skill::Axe, "WOLF", true); win(Skill::Axe, "BEAR", true); win(Skill::Axe, "BANDIT", true);
  win(Skill::Axe, "GOLEM", false); win(Skill::Axe, "SOLDIER", false);
  // spears stop the chargers and keep the big ones off; clumsy in a brawl with people
  win(Skill::Spear, "WOLF", true); win(Skill::Spear, "BEAR", true); win(Skill::Spear, "BANDIT", false);
  // daggers find the gaps in people, never in plate
  win(Skill::Dagger, "SOLDIER", false); win(Skill::Dagger, "GOLEM", false);
  if (edge(Skill::Dagger, "BANDIT") <= edge(Skill::Dagger, "SOLDIER")) SK_FAIL("daggers do not favour the unarmoured\n");
  // bows suit beasts, are weak against shields and bones
  if (edge(Skill::Bow, "WOLF") <= edge(Skill::Bow, "SOLDIER")) SK_FAIL("bows do no better on wolves than on shielded soldiers (%.2f / %.2f)\n", edge(Skill::Bow, "WOLF"), edge(Skill::Bow, "SOLDIER"));
  if (edge(Skill::Bow, "WOLF") <= edge(Skill::Bow, "SKELETON")) SK_FAIL("bows do no better on wolves than on skeletons\n");
  // fists: a brawler's, hopeless on plate
  if (edge(Skill::Unarmed, "BANDIT") <= edge(Skill::Unarmed, "SOLDIER")) SK_FAIL("fists do no better on bandits than on soldiers\n");
  // swords are balanced: against every foe within a quarter of the median blade
  for (const Foe& fo : kFoes) {
    std::vector<float> v;
    for (Skill f : {Skill::Sword, Skill::Axe, Skill::Mace, Skill::Spear, Skill::Dagger}) v.push_back(T(f, fo.name));
    std::sort(v.begin(), v.end());
    const float med = v[2];
    if (T(Skill::Sword, fo.name) > med * 1.25f || T(Skill::Sword, fo.name) < med * 0.75f) SK_FAIL("the sword is not balanced against a %s (%.1f s, median %.1f s)\n", fo.name, T(Skill::Sword, fo.name), med);
  }
  // daggers backstab and crit the unaware: the first blow on an unaware bandit against an aware one
  {
    g.actors.resize(1);
    kit(g, Skill::Dagger, Weight::None, false, L);
    const int id = SkillOps::spawnPerson(g, Role::Bandit, g_field + Vec2(10, 0), L);
    double aware = 0, unaware = 0;
    for (int k = 0; k < 8; k++)
      for (int u = 0; u < 2; u++) {
        Actor& e = actorById(g, id);
        e.hp = e.maxHp = 1e5f; e.aggro = u == 1 ? false : true; e.p = g_field + Vec2(10, 0);
        g.pl().p = g_field; g.pl().aim = Vec2(1, 0);
        const float h0 = e.hp;
        SkillOps::swing(g);
        (u ? unaware : aware) += h0 - actorById(g, id).hp;
      }
    if (table) out("skills: a dagger's first blow, unaware %.0f against aware %.0f (x%.2f)\n", unaware / 8, aware / 8, unaware / std::max(1.0, aware));
    if (unaware < aware * 2.0) SK_FAIL("a dagger on the unaware is not a backstab (x%.2f)\n", unaware / std::max(1.0, aware));
  }
}

// ---------------------------------------------------------------- armour trade-offs in play
struct TradeRow { float walk = 0, roll = 0, rollCost = 0, regen = 0, notice = 0, taken = 0; };
TradeRow measureTrade(Game& g, Weight w, int level) {
  TradeRow r;
  g.actors.resize(1);
  kit(g, Skill::Sword, w, false, 10);
  const Skill a = skills::armourSkill(w);
  if (a != Skill::COUNT) SkillOps::setLevel(g, a, level);
  // a two-second walk
  g.pl().p = g_field;
  Input in;
  in.move = Vec2(1, 0);
  for (int f = 0; f < 120; f++) { tick(g, in); g.events.clear(); g.mode = Mode::Play; }
  r.walk = len(g.pl().p - g_field);
  // one roll from standing: its reach and its cost
  g.pl().p = g_field; g.pl().st = AState::Idle; g.stamina = g.maxSt; g.pl().aim = Vec2(1, 0);
  Input rl;
  rl.roll = true;
  const float s0 = g.stamina;
  tick(g, rl);
  r.rollCost = s0 - g.stamina;
  for (int f = 0; f < 30; f++) { tick(g, Input()); g.events.clear(); }
  r.roll = len(g.pl().p - g_field);
  // stamina coming back over a second
  g.stamina = 0;
  g.pl().st = AState::Idle;
  for (int f = 0; f < 60; f++) { tick(g, Input()); g.events.clear(); }
  r.regen = g.stamina;
  // sneaking up on an unaware wolf: how near before it notices
  g.pl().p = g_field;
  g.skills.live.sneaking = true;
  SkillOps::recalc(g);
  const Vec2 wolfAt = g_field + Vec2(150, 0);
  const int id = SkillOps::spawnMonster(g, art::Monster::Wolf, wolfAt, 10);
  r.notice = 0;
  for (int f = 0; f < 60 * 12; f++) {
    Actor& e = actorById(g, id);
    e.p = wolfAt;
    if (e.aggro) { r.notice = len(wolfAt - g.pl().p); break; }
    Input mv;
    mv.move = Vec2(1, 0);
    tick(g, mv);
    g.events.clear();
    g.mode = Mode::Play;
  }
  g.skills.live.sneaking = false;
  SkillOps::recalc(g);
  // a wolf's bite through damage()
  {
    Actor& e = actorById(g, id);
    e.p = g.pl().p + Vec2(10, 0);
    g.pl().iframes = 0; g.pl().st = AState::Idle; g.pl().hp = g.pl().maxHp;
    const float h0 = g.pl().hp;
    SkillOps::damage(g, g.pl(), 30.0f, e.p, e.id);
    r.taken = h0 - g.pl().hp;
  }
  return r;
}
void checkTrades(uint64_t seed, bool table) {
  Game g(seed);
  freshGame(g, seed);
  TradeRow row[4][2];
  const Weight ws[] = {Weight::None, Weight::Light, Weight::Medium, Weight::Heavy};
  for (int i = 0; i < 4; i++) for (int k = 0; k < 2; k++) row[i][k] = measureTrade(g, ws[i], k ? 100 : skills::START_LEVEL);
  if (table) {
    out("skills: armour in play (skill 5 / 100): walk px in 2 s, roll px, roll stamina, stamina back in 1 s, a sneaking hero noticed at px, a 30 blow taken\n");
    const char* names[] = {"NONE", "LIGHT", "MEDIUM", "HEAVY"};
    for (int i = 0; i < 4; i++)
      out("  %-7s walk %5.1f/%5.1f  roll %5.1f/%5.1f  cost %4.1f/%4.1f  regen %4.1f/%4.1f  noticed %5.1f/%5.1f  taken %4.1f/%4.1f\n", names[i],
          row[i][0].walk, row[i][1].walk, row[i][0].roll, row[i][1].roll, row[i][0].rollCost, row[i][1].rollCost, row[i][0].regen, row[i][1].regen,
          row[i][0].notice, row[i][1].notice, row[i][0].taken, row[i][1].taken);
  }
  const TradeRow &L = row[1][0], &M = row[2][0], &H = row[3][0];
  if (!(H.walk < M.walk && M.walk < L.walk)) SK_FAIL("heavier armour does not walk slower (%.1f %.1f %.1f)\n", L.walk, M.walk, H.walk);
  if (!(H.roll < L.roll)) SK_FAIL("heavy armour does not shorten the roll\n");
  if (!(H.rollCost > M.rollCost && M.rollCost > L.rollCost - 0.01f)) SK_FAIL("heavier armour does not cost more to roll (%.1f %.1f %.1f)\n", L.rollCost, M.rollCost, H.rollCost);
  if (!(H.regen < L.regen)) SK_FAIL("heavy armour does not slow the stamina\n");
  if (!(H.notice > L.notice && L.notice > 0)) SK_FAIL("heavy armour is not louder to a sneaking hero (%.1f against %.1f)\n", H.notice, L.notice);
  if (!(H.taken < M.taken && M.taken < L.taken)) SK_FAIL("heavier armour does not stop more (%.1f %.1f %.1f)\n", L.taken, M.taken, H.taken);
  for (int i = 1; i < 4; i++)
    if (!(row[i][1].walk >= row[i][0].walk && row[i][1].taken <= row[i][0].taken && row[i][1].rollCost <= row[i][0].rollCost + 0.01f && row[i][1].notice <= row[i][0].notice + 0.5f))
      SK_FAIL("armour skill %d does not soften its class's costs\n", i);
}

// ---------------------------------------------------------------- the block
void checkBlock() {
  skills::Skills a;
  skills::startLevels(a, Background::Hunter);
  a[Skill::Mace].level = 42; a[Skill::Mace].xp = 3.5f; a[Skill::Mace].perks = 0x5u;
  a.matFam[(int)Mat::Steel] = 33; a.cultFam[0xABCDEFull] = 71; a.perkPts = 3; a.booksRead = {7u, 9u}; a.respecs = 1; a.uses[3] = 99;
  a.trainedLevel = 4; a.trainedCount = 2;
  std::vector<uint8_t> blk;
  a.serialize(blk);
  skills::Skills b;
  if (!b.deserialize(blk)) { SK_FAIL("the skills block does not read back\n"); return; }
  std::vector<uint8_t> blk2;
  b.serialize(blk2);
  if (blk != blk2 || b[Skill::Mace].level != 42 || b.level(Skill::Bow) <= skills::START_LEVEL || b.cultFam[0xABCDEFull] != 71 || b.trainedCount != 2)
    SK_FAIL("the skills block round trip changed it\n");
  std::vector<uint8_t> bad = blk;
  bad[0] = 99;
  if (b.deserialize(bad)) SK_FAIL("a skills block of an unknown version was read\n");
  // (15.29.5) an old save's defaults by its level: higher levels, more skill; the weapon's family leads
  Rng r(3);
  const Item sw = gear::makeGear(r, ItemKind::Weapon, (int)WeaponType::Mace, 5, Rarity::Common, 0, Mat::Iron);
  const Item ar = gear::makeGear(r, ItemKind::Armor, 0, 5, Rarity::Common, 0, Mat::Iron);
  skills::Skills d1, d20;
  skills::defaultsForLevel(d1, 1, Background::None, &sw, &ar);
  skills::defaultsForLevel(d20, 20, Background::None, &sw, &ar);
  if (d1.level(Skill::Mace) != skills::START_LEVEL || d20.level(Skill::Mace) < 45 || d20.level(Skill::HeavyArmour) < 35 || d20.perkPts != 19)
    SK_FAIL("an old level-20 save's defaults: mace %d, heavy %d, points %d\n", d20.level(Skill::Mace), d20.level(Skill::HeavyArmour), d20.perkPts);
}

// ---------------------------------------------------------------- gains only from their own act
void checkGains(uint64_t seed) {
  Game g(seed);
  freshGame(g, seed);
  kit(g, Skill::Sword, Weight::Medium, true, 5);
  auto only = [&](const char* what, const skills::Skills& before, std::initializer_list<Skill> want) {
    for (int i = 0; i < skills::SKILLS; i++) {
      const uint32_t d = g.skills.uses[i] - before.uses[i];
      bool w = false;
      for (Skill s : want) w |= (int)s == i;
      if (w && !d) SK_FAIL("%s did not train %s\n", what, nm((Skill)i));
      if (!w && d) SK_FAIL("%s trained %s\n", what, nm((Skill)i));
    }
  };
  const int id = SkillOps::spawnMonster(g, art::Monster::Wolf, g.pl().p + Vec2(10, 0), 3);
  actorById(g, id).hp = actorById(g, id).maxHp = 1e5f;
  actorById(g, id).aggro = true;
  g.pl().aim = Vec2(1, 0);
  skills::Skills b = g.skills;
  for (int i = 0; i < 6; i++) SkillOps::swing(g);
  only("a sword blow", b, {Skill::Sword});
  b = g.skills;
  SkillOps::taken(g, id, 5.0f);
  only("a blow taken in medium armour with a shield", b, {Skill::MediumArmour, Skill::Block});
  b = g.skills;
  SkillOps::traded(g, 80);
  only("a sale", b, {Skill::Speech});
  b = g.skills;
  { Rng r(1); Item it = gear::makeGear(r, ItemKind::Weapon, 0, 3, Rarity::Common, 0, Mat::Iron); SkillOps::forged(g, it); }
  only("a forged piece", b, {Skill::Smithing});
  b = g.skills;
  SkillOps::cooked(g, makeFood(0));
  only("a meal", b, {Skill::Cooking});
  b = g.skills;
  SkillOps::farmed(g, 3);
  only("a harvest", b, {Skill::Farming});
  // an arrow: Archery only
  g.actors.resize(1);
  kit(g, Skill::Bow, Weight::None, false, 5);
  const int w2 = SkillOps::spawnMonster(g, art::Monster::Wolf, g.pl().p + Vec2(40, 0), 3);
  actorById(g, w2).hp = actorById(g, w2).maxHp = 1e5f;
  actorById(g, w2).aggro = true;
  b = g.skills;
  for (int f = 0; f < 40; f++) {
    actorById(g, w2).p = g.pl().p + Vec2(40, 0);
    if (f == 0) { g.pl().aim = Vec2(1, 0); SkillOps::shootArrow(g); }
    SkillOps::projectiles(g, SIM_DT);
  }
  only("an arrow", b, {Skill::Bow});
  // a walk: Athletics only
  g.actors.resize(1);
  kit(g, Skill::Sword, Weight::None, false, 5);
  b = g.skills;
  Input in;
  for (int f = 0; f < 60 * 8; f++) {
    in.move = (f / 120) % 2 ? Vec2(-1, 0) : Vec2(1, 0);
    tick(g, in);
    g.events.clear();
    g.mode = Mode::Play;
  }
  only("a walk", b, {Skill::Athletics});
  // a real roll in heavy armour with CHARGE is a shoulder charge (the input path, not the hook alone)
  {
    g.actors.resize(1);
    kit(g, Skill::Sword, Weight::Heavy, false, 5);
    g.skills[Skill::HeavyArmour].level = 60;
    g.skills[Skill::HeavyArmour].perks = (1u << HV_JUGGERNAUT) | (1u << HV_FITTED) | (1u << HV_CHARGE);
    SkillOps::recalc(g);
    g.pl().p = g_field;
    Input r;
    r.roll = true;
    r.move = Vec2(1, 0);
    tick(g, r);
    g.events.clear();
    if (g.pl().st != AState::Roll || g.skills.live.chargeT <= 0) SK_FAIL("a roll in heavy armour with CHARGE did not charge (state %d, charge %.2f)\n", (int)g.pl().st, g.skills.live.chargeT);
  }
  // character XP from a skill's gains
  const int xp0 = g.plXp, lv0 = g.plLevel;
  for (int i = 0; i < 400; i++) g.skillUse(Skill::Axe, 1.0f);
  if (g.plXp == xp0 && g.plLevel == lv0) SK_FAIL("skill-ups gave no character XP\n");
}

// ---------------------------------------------------------------- every perk applied, and saved
struct Probe {
  Skill s;
  int idx;
  std::vector<int> base;   // perks of the tree taken in both runs (a capstone's parents)
  int dir;                 // +1: the measure rises with the perk, -1: it falls
  std::function<double(Game&)> f;
};
// a dummy foe (not in the world): for strikeMul probes
Actor dummy(art::Monster m, bool human = false, Role role = Role::Villager) {
  Actor a;
  a.id = 99999; a.hostile = true; a.aggro = true; a.mon = m; a.human = human; a.role = role;
  a.maxHp = a.hp = 100; a.armor = 30; a.level = 10;
  if (human && role == Role::Soldier) { a.look.shield = true; a.look.armorStyle = 2; }
  return a;
}
// a real foe in the world beside the hero (for struck / takenMul probes)
Actor& foeHere(Game& g, art::Monster m, Vec2 off = Vec2(14, 0)) {
  const int id = SkillOps::spawnMonster(g, m, g.pl().p + off, 10);
  Actor& e = actorById(g, id);
  e.hp = e.maxHp = 1e4f; e.aggro = true; e.st = AState::Idle; e.stT = 0;
  return e;
}
bool hurt(const Actor& a) { return a.st == AState::Hurt; }

std::vector<Probe> probes() {
  std::vector<Probe> v;
  auto sm = [](Skill fam, Actor a, bool power = false) { return [=](Game& g) { return (double)SkillOps::strikeMul(g, a, fam, power); }; };
  auto struckState = [](Skill fam, art::Monster m, bool crit, bool power, std::function<double(Game&, Actor&)> after, bool human = false) {
    return [=](Game& g) {
      Actor& e = human ? actorById(g, SkillOps::spawnPerson(g, Role::Bandit, g.pl().p + Vec2(14, 0), 10)) : foeHere(g, m);
      e.hp = e.maxHp = 1e4f;
      const int id = e.id;
      SkillOps::preStrike(g, e, fam);
      SkillOps::struck(g, actorById(g, id), fam, 20.0f, crit, power, false);
      return after(g, actorById(g, id));
    };
  };
  auto taken = [](int atkDist, float dmg, std::function<void(Game&)> pre = nullptr) {
    return [=](Game& g) {
      Actor& e = foeHere(g, art::Monster::Wolf, Vec2((float)atkDist, 0));
      if (pre) pre(g);
      return (double)SkillOps::takenMul(g, e.id, dmg);
    };
  };
  auto stat = [](Affix a) { return [=](Game& g) { SkillOps::recalc(g); return (double)g.craft.live.stats.get(a); }; };
  Actor wolf = dummy(art::Monster::Wolf), bear = dummy(art::Monster::Bear), soldier = dummy(art::Monster::Wolf, true, Role::Soldier);
  Actor bandit = dummy(art::Monster::Wolf, true, Role::Bandit), draugr = dummy(art::Monster::Draugr);
  Actor weak = wolf; weak.hp = 20;
  Actor sleepy = wolf; sleepy.aggro = false;
  // ---- swords
  v.push_back({Skill::Sword, SW_BLADESMAN, {}, +1, sm(Skill::Sword, wolf)});
  v.push_back({Skill::Sword, SW_PARRY, {}, -1, taken(14, 50, [](Game& g) { g.pl().st = AState::Windup; g.pl().aim = Vec2(1, 0); })});
  v.push_back({Skill::Sword, SW_KEEN, {}, +1, stat(Affix::CritChance)});
  v.push_back({Skill::Sword, SW_RIPOSTE, {SW_PARRY}, +1, [](Game& g) {
    Actor& e = foeHere(g, art::Monster::Wolf); g.pl().st = AState::Windup; g.pl().aim = Vec2(1, 0);
    SkillOps::takenMul(g, e.id, 50); return (double)g.skills.live.riposteT; }});
  v.push_back({Skill::Sword, SW_WHIRLWIND, {}, -1, [](Game& g) { float r = 24, c = -0.1f; SkillOps::swingShape(g, true, r, c); return (double)c; }});
  v.push_back({Skill::Sword, SW_FLURRY, {}, -1, [](Game& g) { return (double)SkillOps::swingCost(g); }});
  v.push_back({Skill::Sword, SW_MASTER, {}, +1, struckState(Skill::Sword, art::Monster::Wolf, true, false, [](Game&, Actor& e) { return (double)hurt(e); })});
  // ---- axes
  v.push_back({Skill::Axe, AX_AXEMAN, {}, +1, sm(Skill::Axe, wolf)});
  v.push_back({Skill::Axe, AX_BLEED, {}, +1, struckState(Skill::Axe, art::Monster::Wolf, false, false, [](Game&, Actor& e) { return (double)e.bleedT; })});
  v.push_back({Skill::Axe, AX_CLEAVE, {}, +1, [](Game& g) { float r = 24, c = -0.1f; SkillOps::swingShape(g, true, r, c); return (double)r - c; }});
  v.push_back({Skill::Axe, AX_HEADSMAN, {}, +1, sm(Skill::Axe, weak)});
  v.push_back({Skill::Axe, AX_DEEP, {AX_BLEED}, +1, struckState(Skill::Axe, art::Monster::Wolf, false, false, [](Game&, Actor& e) { return (double)(e.bleedT * e.bleedDps); })});
  v.push_back({Skill::Axe, AX_HEW, {}, +1, struckState(Skill::Axe, art::Monster::Wolf, false, true, [](Game&, Actor& e) { return (double)hurt(e); })});
  v.push_back({Skill::Axe, AX_BLOODLUST, {}, +1, [](Game& g) {
    Actor& e = foeHere(g, art::Monster::Wolf); e.bleedT = 3; e.bleedDps = 2; g.stamina = 0;
    SkillOps::struck(g, e, Skill::Axe, 20.0f, false, false, true); return (double)g.stamina; }});
  // ---- maces
  v.push_back({Skill::Mace, MA_MACEMAN, {}, +1, sm(Skill::Mace, wolf)});
  v.push_back({Skill::Mace, MA_CRUSH, {}, +1, struckState(Skill::Mace, art::Monster::Draugr, false, true, [](Game&, Actor& e) { return (double)hurt(e); })});
  v.push_back({Skill::Mace, MA_BONEBREAKER, {}, +1, sm(Skill::Mace, draugr)});
  v.push_back({Skill::Mace, MA_SKULL, {}, +1, [](Game& g) { Actor& e = foeHere(g, art::Monster::Skeleton); SkillOps::preStrike(g, e, Skill::Mace); return (double)e.reassembled; }});
  v.push_back({Skill::Mace, MA_HAMMERFALL, {MA_CRUSH}, -1, struckState(Skill::Mace, art::Monster::Wolf, false, true, [](Game&, Actor& e) { return (double)e.stT; })});
  v.push_back({Skill::Mace, MA_BREAKER, {}, +1, struckState(Skill::Mace, art::Monster::Draugr, false, false, [](Game& g, Actor& e) { return (double)SkillOps::strikeMul(g, e, Skill::Mace, false); })});
  v.push_back({Skill::Mace, MA_SHOCKWAVE, {}, +1, [](Game& g) {
    Actor& o = foeHere(g, art::Monster::Wolf, Vec2(20, 12)); const int oid = o.id;
    Actor& e = foeHere(g, art::Monster::Wolf); SkillOps::struck(g, e, Skill::Mace, 20, false, true, false); return (double)hurt(actorById(g, oid)); }});
  // ---- spears
  v.push_back({Skill::Spear, SP_SPEARMAN, {}, +1, sm(Skill::Spear, wolf)});
  v.push_back({Skill::Spear, SP_SETCHARGE, {}, +1, [](Game& g) {
    Actor& e = foeHere(g, art::Monster::Wolf); e.lunge = true; e.st = AState::Windup; const int id = e.id;
    SkillOps::preStrike(g, e, Skill::Spear); actorById(g, id).st = AState::Windup;
    SkillOps::struck(g, actorById(g, id), Skill::Spear, 20, false, false, false); return (double)hurt(actorById(g, id)); }});
  v.push_back({Skill::Spear, SP_REACH, {}, +1, [](Game& g) { return (double)SkillOps::reachAdd(g); }});
  v.push_back({Skill::Spear, SP_IMPALE, {}, +1, [](Game& g) { float r = 24, c = -0.1f; SkillOps::swingShape(g, true, r, c); return (double)r; }});
  v.push_back({Skill::Spear, SP_BAY, {}, +1, [](Game& g) { Actor& e = foeHere(g, art::Monster::Wolf); e.knock = Vec2(60, 0);
    SkillOps::struck(g, e, Skill::Spear, 20, false, false, false); return (double)len(e.knock); }});
  v.push_back({Skill::Spear, SP_GIANT, {}, +1, sm(Skill::Spear, bear)});
  v.push_back({Skill::Spear, SP_PHALANX, {}, +1, [](Game& g) { Actor& e = foeHere(g, art::Monster::Wolf); SkillOps::takenMul(g, e.id, 20); return (double)hurt(e); }});
  // ---- daggers
  v.push_back({Skill::Dagger, DA_HAND, {}, +1, sm(Skill::Dagger, wolf)});
  v.push_back({Skill::Dagger, DA_BACKSTAB, {}, +1, sm(Skill::Dagger, sleepy)});
  v.push_back({Skill::Dagger, DA_QUICK, {}, -1, [](Game& g) { return (double)SkillOps::swingTime(g); }});
  v.push_back({Skill::Dagger, DA_SILENT, {}, -1, [](Game& g) {
    Actor& o = foeHere(g, art::Monster::Wolf, Vec2(40, 0)); o.aggro = false; const int oid = o.id;
    Actor& e = foeHere(g, art::Monster::Wolf); e.aggro = false; const int id = e.id;
    SkillOps::preStrike(g, e, Skill::Dagger); SkillOps::struck(g, actorById(g, id), Skill::Dagger, 20, false, false, true); return (double)actorById(g, oid).aggro; }});
  v.push_back({Skill::Dagger, DA_ASSASSIN, {DA_BACKSTAB}, +1, sm(Skill::Dagger, sleepy)});
  v.push_back({Skill::Dagger, DA_LACERATE, {}, +1, struckState(Skill::Dagger, art::Monster::Wolf, true, false, [](Game&, Actor& e) { return (double)e.bleedT; }, true)});
  v.push_back({Skill::Dagger, DA_SHADOW, {}, +1, [](Game& g) { g.skills.live.sneaking = true; Actor& e = foeHere(g, art::Monster::Wolf);
    SkillOps::struck(g, e, Skill::Dagger, 20, false, false, true); g.skills.live.sneaking = false; return (double)g.skills.live.unseenT; }});
  // ---- bows
  auto shot = [](int n, std::function<double(Game&)> f) {
    return [=](Game& g) { for (int i = 0; i < n; i++) { g.pl().shootCd = 0; g.pl().st = AState::Idle; g.pl().aim = Vec2(1, 0); SkillOps::shootArrow(g); } return f(g); };
  };
  v.push_back({Skill::Bow, BO_OVERDRAW, {}, +1, sm(Skill::Bow, wolf)});
  v.push_back({Skill::Bow, BO_EAGLE, {}, +1, shot(1, [](Game& g) { return g.projs.empty() ? 0.0 : (double)len(g.projs.back().v); })});
  v.push_back({Skill::Bow, BO_STEADY, {}, +1, shot(1, [](Game& g) { return (double)g.slowMo; })});
  v.push_back({Skill::Bow, BO_MARK, {}, +1, struckState(Skill::Bow, art::Monster::Wolf, false, false, [](Game& g, Actor& e) { return (double)SkillOps::strikeMul(g, e, Skill::Bow, false); })});
  v.push_back({Skill::Bow, BO_QUICK, {}, -1, [](Game& g) { return (double)SkillOps::drawMul(g); }});
  v.push_back({Skill::Bow, BO_POWER, {}, +1, struckState(Skill::Bow, art::Monster::Wolf, false, false, [](Game&, Actor& e) { return (double)hurt(e); })});
  v.push_back({Skill::Bow, BO_RANGER, {}, +1, sm(Skill::Bow, soldier)});
  v.push_back({Skill::Bow, BO_TWIN, {}, +1, shot(3, [](Game& g) { return (double)g.projs.size(); })});
  // ---- fists
  v.push_back({Skill::Unarmed, UN_FISTS, {}, +1, sm(Skill::Unarmed, wolf)});
  v.push_back({Skill::Unarmed, UN_BRAWLER, {}, +1, struckState(Skill::Unarmed, art::Monster::Wolf, false, true, [](Game&, Actor& e) { return (double)hurt(e); })});
  v.push_back({Skill::Unarmed, UN_HAYMAKER, {}, +1, sm(Skill::Unarmed, bandit)});
  v.push_back({Skill::Unarmed, UN_IRONSKIN, {}, -1, taken(14, 50)});
  v.push_back({Skill::Unarmed, UN_FLURRY, {}, -1, [](Game& g) { return (double)SkillOps::swingCost(g); }});
  v.push_back({Skill::Unarmed, UN_STUN, {UN_BRAWLER}, -1, struckState(Skill::Unarmed, art::Monster::Wolf, false, true, [](Game&, Actor& e) { return (double)e.stT; })});
  // ---- block (a shield carried)
  v.push_back({Skill::Block, BL_WALL, {}, -1, taken(14, 50)});
  v.push_back({Skill::Block, BL_BASH, {}, +1, [](Game& g) { SkillOps::rolled(g); return (double)g.skills.live.chargeT; }});
  v.push_back({Skill::Block, BL_DEFLECT, {}, -1, taken(90, 50)});
  v.push_back({Skill::Block, BL_STEADFAST, {}, +1, [](Game& g) { return (double)SkillOps::unstaggerable(g, 20); }});
  v.push_back({Skill::Block, BL_REFLEX, {}, -1, taken(14, 400)});
  v.push_back({Skill::Block, BL_BULWARK, {}, -1, taken(14, 50)});
  // ---- the armour classes (a full set of the class worn)
  auto regen = [](Game& g) { return (double)SkillOps::staminaRegenMul(g); };
  auto rollc = [](Game& g) { return (double)SkillOps::rollCostMul(g); };
  auto speed = [](Game& g) { return (double)SkillOps::speedMul(g); };
  v.push_back({Skill::LightArmour, LI_AGILE, {}, -1, taken(14, 50)});
  v.push_back({Skill::LightArmour, LI_WIND, {}, +1, regen});
  v.push_back({Skill::LightArmour, LI_DEFT, {}, -1, rollc});
  v.push_back({Skill::LightArmour, LI_UNHINDERED, {}, +1, speed});
  v.push_back({Skill::LightArmour, LI_FIT, {}, +1, stat(Affix::ArmourPct)});
  v.push_back({Skill::LightArmour, LI_EVASION, {}, +1, [](Game& g) { g.stamina = 0; SkillOps::dodged(g); return (double)g.stamina; }});
  v.push_back({Skill::LightArmour, LI_MATCHLESS, {}, +1, [](Game& g) { SkillOps::dodged(g); return (double)g.skills.live.unseenT; }});
  v.push_back({Skill::MediumArmour, ME_BALANCED, {}, -1, taken(14, 50)});
  v.push_back({Skill::MediumArmour, ME_STEADY, {}, +1, [](Game& g) { return (double)SkillOps::unstaggerable(g, 9); }});
  v.push_back({Skill::MediumArmour, ME_WELLBAL, {}, -1, rollc});
  v.push_back({Skill::MediumArmour, ME_WIND, {}, +1, stat(Affix::Stamina)});
  v.push_back({Skill::MediumArmour, ME_FIT, {}, +1, stat(Affix::ArmourPct)});
  v.push_back({Skill::MediumArmour, ME_RALLY, {}, +1, [](Game& g) { Actor& e = foeHere(g, art::Monster::Wolf); g.pl().hp = g.pl().maxHp * 0.2f; g.stamina = 0;
    SkillOps::taken(g, e.id, 5); return (double)g.stamina; }});
  v.push_back({Skill::MediumArmour, ME_VETERAN, {}, -1, taken(14, 50)});
  v.push_back({Skill::HeavyArmour, HV_JUGGERNAUT, {}, -1, taken(14, 50)});
  v.push_back({Skill::HeavyArmour, HV_FITTED, {}, +1, speed});
  v.push_back({Skill::HeavyArmour, HV_CHARGE, {}, +1, [](Game& g) { SkillOps::rolled(g); return (double)g.skills.live.chargeT; }});
  v.push_back({Skill::HeavyArmour, HV_CONDITION, {}, +1, regen});
  v.push_back({Skill::HeavyArmour, HV_FIT, {}, +1, stat(Affix::ArmourPct)});
  v.push_back({Skill::HeavyArmour, HV_IMMOVABLE, {}, -1, [](Game& g) { return (double)SkillOps::knockMul(g); }});
  v.push_back({Skill::HeavyArmour, HV_UNSTOPPABLE, {HV_CHARGE}, -1, [](Game& g) {
    Actor& e = foeHere(g, art::Monster::Wolf, Vec2(6, 0)); const int id = e.id; SkillOps::rolled(g);
    g.skills.live.preId = -1; tick(g, Input()); g.events.clear();
    return (double)actorById(g, id).hp; }});
  v.push_back({Skill::HeavyArmour, HV_REFLECT, {}, -1, [](Game& g) { Actor& e = foeHere(g, art::Monster::Wolf); double s = 0;
    for (int i = 0; i < 10; i++) s += SkillOps::takenMul(g, e.id, 20); return s; }});
  // ---- heritage (every piece of one people's make, steel in hand)
  v.push_back({Skill::Heritage, HE_KINSHIP, {}, +1, stat(Affix::ArmourPct)});
  v.push_back({Skill::Heritage, HE_METALS, {}, +1, [](Game& g) { g.inv[(size_t)g.eqWeapon].mat = Mat::Steel; g.skills.matFam[(int)Mat::Steel] = 100;
    return (double)SkillOps::strikeMul(g, dummy(art::Monster::Wolf), Skill::Sword, false); }});
  v.push_back({Skill::Heritage, HE_BLADEMAIL, {}, +1, sm(Skill::Sword, wolf)});
  v.push_back({Skill::Heritage, HE_BRONZE, {}, +1, [](Game& g) { g.inv[(size_t)g.eqArmor].mat = Mat::Bronze; SkillOps::recalc(g); return (double)g.craft.live.stats.get(Affix::ArmourPct); }});
  v.push_back({Skill::Heritage, HE_OWN, {}, -1, [](Game& g) { Actor a = dummy(art::Monster::Wolf, true, Role::Soldier); a.culture = g_cult; return (double)SkillOps::detectMul(g, a); }});
  v.push_back({Skill::Heritage, HE_HOMELAND, {}, +1, [](Game& g) { const uint64_t h0 = g.app.homeland; g.app.homeland = g_cult; SkillOps::recalc(g); const double v = g.craft.live.stats.get(Affix::Health); g.app.homeland = h0; SkillOps::recalc(g); return v; }});
  // ---- athletics
  v.push_back({Skill::Athletics, AT_STRIDE, {}, +1, speed});
  v.push_back({Skill::Athletics, AT_SECONDWIND, {}, +1, regen});
  v.push_back({Skill::Athletics, AT_LIGHTFEET, {}, +1, [](Game& g) { return (double)SkillOps::rollMul(g); }});
  v.push_back({Skill::Athletics, AT_ENDURANCE, {}, +1, stat(Affix::Stamina)});
  v.push_back({Skill::Athletics, AT_TRAVELLER, {}, +1, [](Game& g) { g.skills.live.combatT = 99; return (double)SkillOps::speedMul(g); }});
  // ---- smithing
  auto forge = [](Mat m, uint64_t cult) { return [=](Game& g) { Rng r(9); Item it = gear::makeGear(r, ItemKind::Weapon, 0, 8, Rarity::Common, cult, m); SkillOps::forged(g, it); return (double)it.power; }; };
  v.push_back({Skill::Smithing, SM_STEEL, {}, +1, forge(Mat::Iron, 0)});
  v.push_back({Skill::Smithing, SM_TEMPER, {}, +1, forge(Mat::Bronze, 0)});
  v.push_back({Skill::Smithing, SM_THRIFTY, {}, +1, [](Game& g) { double n = 0; for (int i = 0; i < 12; i++) { g.time += 1.0f; Item it = craft::makeStuff(craft::Stuff::BronzeIngot, 1); SkillOps::forged(g, it); n += it.count; } return n; }});
  v.push_back({Skill::Smithing, SM_STUDY, {}, +1, [](Game& g) { return (double)SkillOps::studyMul(g); }});
  v.push_back({Skill::Smithing, SM_MASTERWORK, {}, +1, forge(Mat::Bronze, g_cult)});
  // ---- cooking, farming, riding
  auto meal = [] { Item m = makeFood(0); m.flags |= IF_CRAFTED; return m; };
  v.push_back({Skill::Cooking, CO_HEARTY, {}, +1, [=](Game& g) { return (double)SkillOps::fedMul(g, meal()); }});
  v.push_back({Skill::Cooking, CO_SECOND, {}, +1, [](Game& g) { double n = 0; for (int i = 0; i < 30; i++) { g.time += 0.7f; n += SkillOps::secondHelping(g); } return n; }});
  v.push_back({Skill::Cooking, CO_FIELD, {}, +1, [=](Game& g) { return (double)SkillOps::mealHeal(g, meal()); }});
  v.push_back({Skill::Farming, FA_GREEN, {}, +1, [](Game& g) { return (double)SkillOps::harvestBonus(g); }});
  v.push_back({Skill::Farming, FA_RAIN, {}, +1, [](Game& g) { return (double)SkillOps::rainMaker(g); }});
  v.push_back({Skill::Farming, FA_HUSBANDRY, {}, +1, [](Game& g) { return (double)SkillOps::produceMul(g); }});
  v.push_back({Skill::Riding, RI_HORSEMAN, {}, +1, [](Game& g) { return (double)SkillOps::horseMul(g); }});
  v.push_back({Skill::Riding, RI_SURESEAT, {}, +1, [](Game& g) { return (double)SkillOps::keepSaddle(g, 5); }});
  v.push_back({Skill::Riding, RI_STABLE, {}, -1, [](Game& g) { return (double)SkillOps::stableFee(g, 100); }});
  // ---- speech (a merchant of the start town)
  auto merchant = [](Game& g) -> Actor& {
    Actor a; a.id = 88000 + (int)g.actors.size(); a.npc = true; a.human = true; a.role = Role::Merchant; a.site = g.world.startSite; a.slot = 4321; a.name = "MERCHANT";
    g.actors.push_back(a); g.dlg.actor = a.id; g.dlg.opts.clear(); return g.actors.back(); };
  auto optCount = [=](Game& g) { Actor& a = merchant(g); SkillOps::talk(g, a); double n = 0; for (const DlgOpt& o : g.dlg.opts) n += o.action == DLG_SKILLS + 3; return n; };
  v.push_back({Skill::Speech, SC_HAGGLER, {}, -1, [](Game& g) { return (double)SkillOps::buyMul(g, Role::Merchant); }});
  v.push_back({Skill::Speech, SC_INTIMIDATE, {}, +1, optCount});
  v.push_back({Skill::Speech, SC_PERSUADE, {}, +1, optCount});
  v.push_back({Skill::Speech, SC_TRAINERPET, {}, -1, [](Game& g) { return (double)SkillOps::trainCost(g, Skill::Sword); }});
  v.push_back({Skill::Speech, SC_SILVER, {SC_PERSUADE}, +1, [=](Game& g) {
    double won = 0;
    for (int d = 0; d < 12; d++) {
      g.day = 10 + d; Actor& a = merchant(g); (void)a;
      DlgOpt o; o.action = DLG_SKILLS + 4;
      SkillOps::choose(g, o);
      won += g.dlg.text.find("SINCE IT'S YOU") != std::string::npos;
    }
    return won; }});
  // ---- stealth (sneaking)
  v.push_back({Skill::Stealth, ST_SOFTSTEP, {}, -1, [](Game& g) { g.skills.live.sneaking = true; const double d = SkillOps::detectMul(g, dummy(art::Monster::Wolf)); g.skills.live.sneaking = false; return d; }});
  v.push_back({Skill::Stealth, ST_SHADOW, {}, -1, [](Game& g) { g.skills.live.sneaking = true; const double d = SkillOps::detectMul(g, dummy(art::Monster::Wolf)); g.skills.live.sneaking = false; return d; }});
  v.push_back({Skill::Stealth, ST_SNEAKATTACK, {}, +1, [=](Game& g) { g.skills.live.sneaking = true; const double d = SkillOps::strikeMul(g, sleepy, Skill::Sword, false); g.skills.live.sneaking = false; return d; }});
  v.push_back({Skill::Stealth, ST_VANISH, {}, +1, [](Game& g) { g.skills.live.sneaking = true; const bool b = SkillOps::losesTrail(g, dummy(art::Monster::Wolf), 9.0f * TILE); g.skills.live.sneaking = false; return (double)b; }});
  return v;
}

// the kit a perk's probe is measured in
void probeKit(Game& g, Skill s) {
  switch (s) {
    case Skill::Axe: case Skill::Mace: case Skill::Dagger: case Skill::Bow: case Skill::Unarmed: kit(g, s, Weight::None, false, 10); break;
    case Skill::Spear: kit(g, s, Weight::None, true, 10); break;
    case Skill::Block: kit(g, Skill::Sword, Weight::None, true, 10); break;
    case Skill::LightArmour: case Skill::Stealth: kit(g, Skill::Sword, Weight::Light, false, 10); break;
    case Skill::MediumArmour: kit(g, Skill::Sword, Weight::Medium, false, 10); break;
    case Skill::HeavyArmour: kit(g, Skill::Sword, Weight::Heavy, false, 10); break;
    case Skill::Heritage: kit(g, Skill::Sword, Weight::Medium, false, 10, g_cult); break;
    default: kit(g, Skill::Sword, Weight::None, false, 10); break;
  }
  for (int i = 0; i < skills::SKILLS; i++) g.skills.t[i].level = 50;
  g.skills.live.smithInit = true;
  g.gold = 100000;
}

void checkPerks(uint64_t seed, bool table) {
  Game g(seed);
  freshGame(g, seed);
  const std::vector<Probe> ps = probes();
  std::set<int> covered;
  int ok = 0;
  for (const Probe& p : ps) {
    double val[2] = {0, 0};
    for (int on = 0; on < 2; on++) {
      g.actors.resize(1);
      g.projs.clear();
      g.slowMo = 0;
      g.pl().p = g_field;
      probeKit(g, p.s);
      for (int b : p.base) g.skills[p.s].perks |= 1u << b;
      if (on) g.skills[p.s].perks |= 1u << p.idx;
      SkillOps::recalc(g);
      g.pl().hp = g.pl().maxHp;
      val[on] = p.f(g);
      g.events.clear();
    }
    int n = 0;
    const skills::PerkDef* P = skills::perkTable(p.s, n);
    const bool works = p.dir > 0 ? val[1] > val[0] + 1e-6 : val[1] < val[0] - 1e-6;
    if (!works) SK_FAIL("perk %s %s does nothing (%.3f without, %.3f with)\n", nm(p.s), P[p.idx].name, val[0], val[1]);
    else ok++;
    covered.insert((int)p.s * 64 + p.idx);
  }
  // every perk of every non-magic tree has its probe (Lockpicking: reserved, no locks in the world yet)
  int total = 0;
  for (int i = 0; i < skills::SKILLS; i++) {
    const Skill s = (Skill)i;
    if (skills::isMagic(s) || s == Skill::Lockpicking) continue;
    int n = 0;
    const skills::PerkDef* P = skills::perkTable(s, n);
    for (int k = 0; k < n; k++) { total++; if (!covered.count(i * 64 + k)) SK_FAIL("perk %s %s has no probe\n", nm(s), P[k].name); }
  }
  if (table) out("skills: %d of %d perks change play (probe off/on)\n", ok, total);
  // ---- taken by the rules, kept by a save: every tree, every perk
  g.actors.resize(1);
  kit(g, Skill::Sword, Weight::None, false, 30);
  for (int i = 0; i < skills::SKILLS; i++) g.skills.t[i].level = 100;
  g.skills.perkPts = 2000;
  for (int i = 0; i < skills::SKILLS; i++) {
    if ((Skill)i == Skill::Lockpicking) continue;   // reserved: no locks yet, its perk is refused
    int n = 0;
    skills::perkTable((Skill)i, n);
    for (int k = 0; k < n; k++) { std::string why; if (!SkillOps::takePerk(g, (Skill)i, k, &why)) SK_FAIL("perk %d of %s cannot be taken at 100: %s\n", k, nm((Skill)i), why.c_str()); }
  }
  std::vector<uint8_t> buf;
  g.serialize(buf);
  Game h(seed);
  if (!h.deserialize(buf)) { SK_FAIL("a save with every perk does not load\n"); return; }
  for (int i = 0; i < skills::SKILLS; i++)
    if (h.skills.t[i].perks != g.skills.t[i].perks || h.skills.t[i].level != g.skills.t[i].level) SK_FAIL("%s's perks or level changed in a save\n", nm((Skill)i));
  // perks need their level and parents, a point each; respec is costly and rises
  Game k(seed);
  freshGame(k, seed);
  kit(k, Skill::Sword, Weight::None, false, 5);
  k.skills.perkPts = 1;
  k.skills[Skill::Sword].level = 10;
  std::string why;
  if (!SkillOps::takePerk(k, Skill::Sword, 0, &why)) SK_FAIL("the first sword perk could not be taken: %s\n", why.c_str());
  if (SkillOps::takePerk(k, Skill::Sword, 0, &why)) SK_FAIL("a perk was taken twice\n");
  if (SkillOps::takePerk(k, Skill::Sword, SW_WHIRLWIND, &why)) SK_FAIL("a perk was taken past its level and parents\n");
  k.gold = 100;
  if (SkillOps::respec(k, Skill::Sword, &why)) SK_FAIL("a respec was had for 100 gold\n");
  k.gold = 10000;
  const int c1 = SkillOps::respecCost(k, Skill::Sword);
  if (!SkillOps::respec(k, Skill::Sword, &why) || k.skills.perkPts != 1 || k.skills[Skill::Sword].perks) SK_FAIL("the respec did not return the perk: %s\n", why.c_str());
  SkillOps::takePerk(k, Skill::Sword, 0, &why);
  if (SkillOps::respecCost(k, Skill::Sword) <= c1) SK_FAIL("a second respec costs no more than the first\n");
}

// ---------------------------------------------------------------- trainers, books, companions
void checkTeaching(uint64_t seed, bool table) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  skRun(g, 0.5f);
  // a trainer of the start settlement (census jobs: a smith, a guard, a hunter, an innkeeper...)
  int trainer = -1, priest = -1;
  skills::Skill sk[3];
  int cap = 0, n = 0;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    int c2 = 0;
    skills::Skill s2[3];
    if (trainer < 0 && SkillOps::teaches(g, g.actors[i], s2, c2) > 0) { trainer = a.id; n = SkillOps::teaches(g, g.actors[i], sk, cap); }
    if (priest < 0 && a.npc && a.role == Role::Priest) priest = a.id;
  }
  if (trainer < 0) { SK_FAIL("no trainer in the start settlement\n"); return; }
  if (table) out("skills: a trainer (%s) teaches %s%s%s up to %d\n", actorById(g, trainer).name.c_str(), nm(sk[0]), n > 1 ? " / " : "", n > 1 ? nm(sk[1]) : "", cap);
  g.gold = 100000;
  g.plLevel = 3;
  const int lv0 = g.skills.level(sk[0]);
  int taught = 0;
  g.dlg.actor = trainer;
  for (int i = 0; i < 8; i++) {
    DlgOpt o; o.action = DLG_SKILLS + 100 + (int)sk[0];
    SkillOps::choose(g, o);
    taught = g.skills.level(sk[0]) - lv0;
  }
  if (taught != std::min(skills::TRAIN_PER_LEVEL, cap - lv0)) SK_FAIL("a trainer taught %d levels at one character level (cap %d)\n", taught, skills::TRAIN_PER_LEVEL);
  g.plLevel = 4;
  { DlgOpt o; o.action = DLG_SKILLS + 100 + (int)sk[0]; SkillOps::choose(g, o); }
  if (g.skills.level(sk[0]) != lv0 + taught + 1) SK_FAIL("a new character level did not open more lessons\n");
  // the talk: the trainer's option on the first page
  {
    Actor& a = actorById(g, trainer);
    g.dlg.opts.clear();
    SkillOps::talk(g, a);
    bool any = false;
    for (const DlgOpt& o : g.dlg.opts) any |= o.action == DLG_SKILLS + 1;
    if (!any) SK_FAIL("the trainer offers no lessons\n");
  }
  // respec at a priest (any skill)
  if (priest >= 0) {
    g.skills.perkPts = 1;
    SkillOps::takePerk(g, Skill::Athletics, 0);
    g.dlg.actor = priest;
    DlgOpt o; o.action = DLG_SKILLS + 2;
    SkillOps::choose(g, o);
    bool any = false;
    for (const DlgOpt& op : g.dlg.opts) any |= op.action == DLG_SKILLS + 200 + (int)Skill::Athletics;
    if (!any) SK_FAIL("the priest offers no unlearning\n");
    DlgOpt u; u.action = DLG_SKILLS + 200 + (int)Skill::Athletics;
    SkillOps::choose(g, u);
    if (g.skills[Skill::Athletics].perks) SK_FAIL("the priest's unlearning did not return the perk\n");
  }
  // a book: read once, a level
  {
    Item b = skills::makeSkillBook(Skill::Spear, 4242u, 5);
    g.inv.push_back(b);
    const int lv = g.skills.level(Skill::Spear);
    g.useItem((int)g.inv.size() - 1);
    g.inv.push_back(b);
    g.useItem((int)g.inv.size() - 1);
    if (g.skills.level(Skill::Spear) != lv + 1) SK_FAIL("a book taught %d levels (1 wanted, once)\n", g.skills.level(Skill::Spear) - lv);
  }
  // books among the loot: rare on bodies, likelier in chests, likeliest in a hoard
  {
    int got[3] = {};
    const gear::LootSrc src[3] = {gear::LootSrc::Body, gear::LootSrc::Chest, gear::LootSrc::Hoard};
    for (int k = 0; k < 3; k++)
      for (int i = 0; i < 2000; i++) {
        const size_t before = g.pickups.size();
        SkillOps::loot(g, Vec2((float)(i % 50) * 16.0f, (float)(i / 50) * 16.0f), 5, src[k]);
        got[k] += (int)(g.pickups.size() - before);
        g.pickups.resize(before);
      }
    if (table) out("skills: books in 2000 drops: body %d, chest %d, hoard %d\n", got[0], got[1], got[2]);
    if (!(got[0] < got[1] && got[1] < got[2] && got[0] > 0 && got[0] < 40)) SK_FAIL("skill books drop %d / %d / %d (body < chest < hoard, bodies rare)\n", got[0], got[1], got[2]);
    Item b = skills::makeSkillBook(Skill::Sword, 1u, 5);
    std::string why;
    if (!gear::lootValid(b, &why)) SK_FAIL("a skill book is not valid loot: %s\n", why.c_str());
  }
  // a scholar's shelf holds a book
  {
    Actor a; a.id = 77777; a.npc = true; a.human = true; a.role = Role::Mage; a.site = g.world.startSite; a.slot = 999;
    std::vector<Item> st;
    SkillOps::shopStock(g, a, st);
    bool book = false;
    for (const Item& it : st) book |= skills::isSkillBook(it);
    if (!book) SK_FAIL("a scholar sells no skill book\n");
  }
  // a companion teaches their vocation, free, by their bond
  {
    party::Member m;
    m.id = 1; m.voc = party::Vocation::Warrior; m.name = "TESTA"; m.bond = 50;
    g.party.members.push_back(m);
    const int mi = (int)g.party.members.size() - 1;
    g.plLevel = 9;
    const int lv = g.skills.level(Skill::Block), gold = g.gold;
    DlgOpt o; o.action = DLG_SKILLS + 300 + (int)Skill::Block; o.arg = mi;
    SkillOps::choose(g, o);
    if (g.skills.level(Skill::Block) != lv + 1 || g.gold != gold) SK_FAIL("a companion did not teach Block for free\n");
  }
}

int skillsCmd(int argc, char** argv) {
  uint64_t seed = 3;
  bool table = true;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 10);
    if (!std::strcmp(argv[i], "--quiet")) table = false;
  }
  s_bad = 0;
  checkTables();
  checkCurve(table);
  checkTags();
  checkFamilies(table);
  checkBlock();
  checkGains(seed);
  checkTrades(seed, table);
  checkTTK(seed, table);
  checkPerks(seed, table);
  checkTeaching(seed, table);
  checkMastery(seed, table);
  out("skills: %s (%d failures)\n", s_bad ? "FAIL" : "OK", s_bad);
  return s_bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--skills", "M9: learn-by-use skills: tables, trees, the curve and mastery hours (play-rate bot), foe tags, the weapon-vs-foe TTK table, armour trade-offs in play, gains from the right act only, every perk probed and saved, trainers / books / companions, the block [--seed N] [--quiet]", skillsCmd);
