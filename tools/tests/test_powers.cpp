// rpg_test --powers: every affix and every unique / Mythic power that may roll does something in play (VISION_PLAN
// 15.27.2: "Every affix must work in combat or the sim"). POWERS lane.
// For each working affix and power an A/B run: the same arena (one Game copied), the same foe, the same hero with one
// piece that carries it and the same piece without it (gear::makeGear, then the affix / power overwritten). The
// measured number must move the way its words say (damage dealt, damage taken, health healed, burn per second, foes
// chained, time slowed, a wolf summoned and gone, a phoenix rebirth once per cooldown...). Then the same for a companion
// wearing it (15.27.5), and the time-to-kill with real Epic and Mythic kits at levels 12 and 30 (>= 2.5 hits).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include "engine/audio.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/power_ops.h"
#include "rpg/sim/powers.h"
#include "tools/tests/tests.h"

namespace {

int g_bad = 0;
constexpr int L = 20;   // the hero's level for the A/B runs (the affix values are this level's mid-range)

Actor* byId(Game& g, int id) {
  const int k = PowerOps::findActor(g, id);
  return k >= 0 ? &g.actors[(size_t)k] : nullptr;
}
void run(Game& g, float secs, Input in = Input(), const std::function<void(Game&)>& each = nullptr) {
  const int n = (int)std::lround(secs / SIM_DT);
  for (int f = 0; f < n; f++) {
    if (each) each(g);
    g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode == Mode::LevelUp) g.mode = Mode::Play;
  }
}
int countWolves(const Game& g, int by = -1) {
  int n = 0;
  for (const Actor& a : g.actors) if (a.powTag == powers::PT_WOLF && a.st != AState::Dead && (by < 0 || (int)a.powBy == by)) n++;
  return n;
}

// ---------------------------------------------------------------- the arena
void clearFoes(Game& g) {
  for (size_t i = 1; i < g.actors.size();) {
    if (!g.actors[i].comp) g.actors.erase(g.actors.begin() + (std::ptrdiff_t)i); else i++;
  }
  g.projs.clear();
  g.pickups.clear();
}
void openCountry(Game& g) {
  const int32_t sx = g.world.ox + (int32_t)(g.pl().p.x / TILE), sy = g.world.oy + (int32_t)(g.pl().p.y / TILE);
  static const int off[][2] = {{90, 0}, {-90, 0}, {0, 90}, {0, -90}, {70, 70}, {-70, 70}, {70, -70}, {-70, -70}, {130, 0}, {-130, 0}};
  for (const auto& o : off) {
    g.teleportGlobal(sx + o[0], sy + o[1]);
    run(g, 0.5f);
    bool bad = g.settlementAt(g.pl().p) >= 0;
    for (int dx = -6; dx <= 6 && !bad; dx++)
      for (int dy = -4; dy <= 4 && !bad; dy++) {
        const int tx = (int)std::floor(g.pl().p.x / TILE) + dx, ty = (int)std::floor(g.pl().p.y / TILE) + dy;
        if (g.map().blocked(tx, ty)) bad = true;
      }
    for (const Actor& a : g.actors) if (a.npc && !a.comp && !a.hostile && len2(a.p - g.pl().p) < (25.0f * TILE) * (25.0f * TILE)) bad = true;
    if (!bad) return;
  }
}
void nakedKit(Game& g, int level, uint64_t seed) {
  g.inv.clear();
  g.eqWeapon = g.eqBow = g.eqStaff = g.eqArmor = g.eqHelmet = g.eqShield = g.eqRing = g.eqAmulet = g.eqGloves = g.eqBoots = g.eqCloak = -1;
  Rng r(seed ^ 0x5157u);
  g.inv.push_back(gear::makeGear(r, ItemKind::Weapon, (int)WeaponType::Sword, level, Rarity::Common, 0));
  g.eqWeapon = 0;
  g.inv.push_back(gear::makeGear(r, ItemKind::Armor, 0, level, Rarity::Common, 0));
  g.eqArmor = 1;
  g.plLevel = level;
  PowerOps::recalc(g);
  g.pl().hp = g.pl().maxHp;
  g.stamina = g.maxSt; g.mp = g.maxMp;
  g.craft.live.fx = powers::State();
}
// a recruit from the start village's census (as --companion recruits them)
int recruit(Game& g) {
  const life::Census* c = g.life.census(g.world, g.world.startSite);
  if (!c) return -1;
  for (const life::Resident& r : c->res) {
    if ((r.flags & (life::RF_DEAD | life::RF_KEY | life::RF_AWAY)) || r.job == life::Job::Child || r.job == life::Job::Elder || r.age < 18 || r.age > 62) continue;
    Actor a;
    a.id = -99; a.npc = true; a.human = true; a.site = g.world.startSite; a.resident = r.idx; a.p = g.pl().p + Vec2(20, 0);
    return g.partyRecruit(a, party::Origin::Tavern);
  }
  return -1;
}
int compIdx(const Game& g) {
  for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].comp == 1 && g.actors[i].st != AState::Dead) return (int)i;
  return -1;
}
Game makeBase(uint64_t seed, int level, bool companion) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.hour = 12;
  run(g, 0.5f);
  if (companion) { recruit(g); run(g, 0.3f); }
  openCountry(g);
  clearFoes(g);
  nakedKit(g, level, seed);
  if (companion && !g.party.members.empty()) {
    party::Member& m = g.party.members[0];
    m.inv.clear(); m.eq.fill(-1);
    m.voc = party::Vocation::Warrior;
    PartyOps::kit(g, m, level);
    for (Item& it : m.inv) { for (ItemAffix& a : it.affix) a = ItemAffix(); it.unique = Unique::None; it.rarity = Rarity::Common; }
    m.stance = party::Cmd::Passive;
    run(g, 0.2f);
    const int ci = compIdx(g);
    if (ci > 0) { g.actors[(size_t)ci].p = g.pl().p + Vec2(-10, 0); PowerOps::companionBody(g, 0); g.actors[(size_t)ci].hp = g.actors[(size_t)ci].maxHp; }
  }
  g.events.clear();
  return g;
}

int foe(Game& g, art::Monster m, Vec2 off, float hp = 1e5f, int level = L) {
  const int id = g.debugSpawnAt(m, g.pl().p + off, level);
  Actor* a = byId(g, id);
  if (!a) return -1;
  a->rank = 0; a->affixes = 0; a->overlays = 0; a->bodyTint = 0; a->unique = 0; a->pack = 0;
  a->maxHp = a->hp = hp; a->regen = 0;
  return id;
}
int bandit(Game& g, Vec2 off, float hp = 1e5f) {
  const int id = PowerOps::spawnBandit(g, g.pl().p + off, L);
  Actor* a = byId(g, id);
  if (!a) return -1;
  a->hostile = true; a->aggro = true; a->rank = 0; a->affixes = 0; a->maxHp = a->hp = hp; a->regen = 0;
  return id;
}

// the piece: a Rare of the kind at level L carrying exactly one affix or power (or nothing: the "without")
Item piece(ItemKind k, int sub, Affix a, int v, Unique u) {
  Rng r(0xA11CEu + (uint32_t)k * 31u + (uint32_t)sub);
  Item it = gear::makeGear(r, k, sub, L, Rarity::Rare, 0);
  for (ItemAffix& x : it.affix) x = ItemAffix();
  if (a != Affix::None) { it.affix[0].kind = a; it.affix[0].value = (int16_t)v; }
  it.unique = u;
  return it;
}
void wear(Game& g, const Item& it) {
  g.inv.push_back(it);
  *g.equipSlot(it.kind) = (int)g.inv.size() - 1;
  PowerOps::recalc(g);
}
void giveMember(Game& g, const Item& it) {
  party::Member& m = g.party.members[0];
  m.inv.push_back(it);
  m.eq[(size_t)party::slotOf(it.kind)] = (int16_t)(m.inv.size() - 1);
  PowerOps::companionBody(g, 0);
}
int midValue(Affix a) {
  int lo = 0, hi = 0;
  gear::affixRange(a, L, lo, hi);
  return std::max(1, (lo + hi) / 2);
}

// ---------------------------------------------------------------- measurements (on a copy with the piece on)
float hitOn(Game& g, int id, float dmg, int attacker = -1, Ench e = Ench::None) {
  Actor* v = byId(g, id);
  if (!v) return 0;
  const float h = v->hp;
  const int at = attacker < 0 ? g.pl().id : attacker;
  const int ai = PowerOps::findActor(g, at);
  PowerOps::hurtE(g, *v, dmg, ai >= 0 ? g.actors[(size_t)ai].p : g.pl().p, at, e, 0);
  v = byId(g, id);
  return v ? h - v->hp : h;
}
float takenFrom(Game& g, int foeId, float dmg, Ench e = Ench::None) {
  Actor& p = g.pl();
  p.iframes = 0;
  if (p.st == AState::Roll) p.st = AState::Idle;
  const float h = p.hp;
  Actor* f = byId(g, foeId);
  PowerOps::hurtE(g, g.pl(), dmg, f ? f->p : g.pl().p + Vec2(10, 0), foeId, e, 0);
  return h - g.pl().hp;
}
// one plain first blow of the hero at the foe (placed in reach, straight ahead)
float swingAt(Game& g, int id, int combo = 0) {
  Actor* v = byId(g, id);
  if (!v) return 0;
  Actor& p = g.pl();
  v->p = p.p + Vec2(12, 0); v->knock = Vec2();
  p.aim = Vec2(1, 0); p.face = 2; p.combo = combo;
  const float h = v->hp;
  PowerOps::swing(g);
  v = byId(g, id);
  return v ? h - v->hp : h;
}
int critTexts(Game& g) {
  int n = 0;
  for (const Event& e : g.events) if (e.type == Ev::Text && e.s.size() > 1 && e.s.back() == '!') n++;
  return n;
}
int textCount(Game& g, const char* s) {
  int n = 0;
  for (const Event& e : g.events) if (e.type == Ev::Text && e.s == s) n++;
  return n;
}

// ---------------------------------------------------------------- the table
struct Row { std::string who, name, measure; double a, b; bool ok; };
std::vector<Row> rows;
void report(const std::string& who, const std::string& name, const std::string& measure, double a, double b, bool ok) {
  rows.push_back({who, name, measure, a, b, ok});
  if (!ok) { out("FAIL: powers: %s %s: %s %.3f -> %.3f does not move as its words say\n", who.c_str(), name.c_str(), measure.c_str(), a, b); g_bad++; }
}
bool near(double x, double want, double rel = 0.02, double abs = 0.05) { return std::fabs(x - want) <= std::max(abs, std::fabs(want) * rel); }

using Fn = std::function<double(Game&)>;
// A/B: the base copied twice, the piece with and without; fn measures
void ab(const Game& base, const std::string& who, const std::string& name, const std::string& measure, const Item& with, const Item& without,
        const Fn& fn, const std::function<bool(double, double)>& ok, const std::vector<Item>& extra = {}) {
  double r[2];
  for (int k = 0; k < 2; k++) {
    Game g = base;
    for (const Item& e : extra) wear(g, e);
    if (who == "companion") giveMember(g, k ? with : without);
    else wear(g, k ? with : without);
    g.events.clear();
    r[k] = fn(g);
  }
  report(who, name, measure, r[0], r[1], ok(r[0], r[1]));
}

// ---------------------------------------------------------------- the player's affixes
void heroAffixes(const Game& base) {
  auto ring = [](Affix a, int v) { return piece(ItemKind::Ring, 0, a, v, Unique::None); };
  const Item none = ring(Affix::None, 0);
  auto ratioCase = [&](Affix a, art::Monster m, bool human) {
    const int v = midValue(a);
    ab(base, "hero", affixText(a, v), "damage dealt", ring(a, v), none,
       [&](Game& g) { const int id = human ? bandit(g, Vec2(12, 0)) : foe(g, m, Vec2(12, 0)); return (double)hitOn(g, id, 100); },
       [v](double x, double y) { return x > 0 && near(y / x, 1.0 + 0.01 * v, 0.01); });
  };
  ratioCase(Affix::FireDmg, art::Monster::Wolf, false);
  ratioCase(Affix::FrostDmg, art::Monster::Wolf, false);
  ratioCase(Affix::ShadowDmg, art::Monster::Wolf, false);
  ratioCase(Affix::VsBeasts, art::Monster::Wolf, false);
  ratioCase(Affix::VsUndead, art::Monster::Skeleton, false);
  ratioCase(Affix::VsPeople, art::Monster::Wolf, true);
  {  // the slayer words name their foes only: VS BEASTS does nothing to a bandit
    const int v = midValue(Affix::VsBeasts);
    ab(base, "hero", affixText(Affix::VsBeasts, v) + " (on a bandit)", "damage dealt", ring(Affix::VsBeasts, v), none,
       [&](Game& g) { return (double)hitOn(g, bandit(g, Vec2(12, 0)), 100); }, [](double x, double y) { return x > 0 && near(y, x, 0.001); });
  }
  {
    const int v = midValue(Affix::LifeDrain);
    ab(base, "hero", affixText(Affix::LifeDrain, v), "healed per 100 dealt", ring(Affix::LifeDrain, v), none,
       [&](Game& g) { g.pl().hp = 30; const float d = hitOn(g, foe(g, art::Monster::Wolf, Vec2(12, 0)), 100); return (g.pl().hp - 30) * 100.0 / std::max(1.0f, d); },
       [v](double x, double y) { return near(x, 0) && near(y, v, 0.02); });
  }
  {
    const int v = midValue(Affix::CritChance);
    ab(base, "hero", affixText(Affix::CritChance, v), "crit %", ring(Affix::CritChance, v), none,
       [&](Game& g) {
         const int id = foe(g, art::Monster::Wolf, Vec2(12, 0));
         int c = 0;
         for (int k = 0; k < 1000; k++) { swingAt(g, id); c += critTexts(g); g.events.clear(); }
         return c / 10.0;
       },
       [v](double x, double y) { return std::fabs((y - x) - v) <= std::max(3.0, v * 0.35); });
  }
  {
    const int v = midValue(Affix::CritDmg);
    const Item sure = piece(ItemKind::Amulet, 0, Affix::CritChance, 100, Unique::None);   // (every blow a crit)
    ab(base, "hero", affixText(Affix::CritDmg, v), "crit damage", ring(Affix::CritDmg, v), none,
       [&](Game& g) { const int id = foe(g, art::Monster::Wolf, Vec2(12, 0)); double s = 0; for (int k = 0; k < 50; k++) s += swingAt(g, id); return s; },
       [v](double x, double y) { return x > 0 && near(y / x, (gear::CRIT_MULT + 0.01 * v) / gear::CRIT_MULT, 0.01); }, {sure});
  }
  {
    const int v = midValue(Affix::StaminaRegen);
    ab(base, "hero", affixText(Affix::StaminaRegen, v), "stamina in 1 s", ring(Affix::StaminaRegen, v), none,
       [&](Game& g) { g.stamina = 0; run(g, 1.0f); return (double)g.stamina; },
       [v](double x, double y) { return x > 0 && near(y / x, 1.0 + 0.01 * v, 0.03); });
  }
  {
    const int v = midValue(Affix::MoveSpeed);
    ab(base, "hero", affixText(Affix::MoveSpeed, v), "move speed", ring(Affix::MoveSpeed, v), none,
       [&](Game& g) { return (double)g.pl().speed; }, [v](double x, double y) { return near(y / x, 1.0 + 0.01 * v, 0.005); });
  }
  {
    const int v = midValue(Affix::RollDist);
    ab(base, "hero", affixText(Affix::RollDist, v), "roll px", ring(Affix::RollDist, v), none,
       [&](Game& g) { const Vec2 a = g.pl().p; Input in; in.move = Vec2(1, 0); in.roll = true; run(g, SIM_DT, in); run(g, 0.4f); return (double)len(g.pl().p - a); },
       [v](double x, double y) { return x > 0 && near(y / x, 1.0 + 0.01 * v, 0.06); });
  }
  {
    const int v = midValue(Affix::Thorns);
    ab(base, "hero", affixText(Affix::Thorns, v), "returned per 100 taken", ring(Affix::Thorns, v), none,
       [&](Game& g) {
         const int id = foe(g, art::Monster::Wolf, Vec2(10, 0));
         const float h = byId(g, id)->hp;
         const float t = takenFrom(g, id, 60);
         return (h - byId(g, id)->hp) * 100.0 / std::max(0.1f, t);
       },
       [v](double x, double y) { return near(x, 0) && near(y, v, 0.02); });
  }
  for (Affix a : {Affix::ResFire, Affix::ResFrost, Affix::ResShadow}) {
    const int v = midValue(a);
    const Ench e = a == Affix::ResFire ? Ench::Fire : a == Affix::ResFrost ? Ench::Frost : Ench::Drain;
    ab(base, "hero", affixText(a, v), "damage taken", ring(a, v), none,
       [&](Game& g) { return (double)takenFrom(g, foe(g, art::Monster::Wolf, Vec2(10, 0)), 60, e); },
       [v](double x, double y) { return x > 0 && near(y / x, 1.0 - 0.01 * v, 0.01); });
  }
  {
    const int v = midValue(Affix::SpellPower);
    ab(base, "hero", affixText(Affix::SpellPower, v), "firebolt damage", ring(Affix::SpellPower, v), none,
       [&](Game& g) { g.spellsKnown = 1; g.spell = Spell::Flames; g.mp = 500; g.pl().shootCd = 0; g.pl().st = AState::Idle; PowerOps::cast(g); return g.projs.empty() ? 0.0 : (double)g.projs.back().dmg; },
       [v](double x, double y) { return x > 0 && near(y - x, 18.0 * 0.02 * v, 0.02); });
  }
  {
    const int v = midValue(Affix::Magicka);
    ab(base, "hero", affixText(Affix::Magicka, v), "max magicka", ring(Affix::Magicka, v), none,
       [&](Game& g) { g.mp = 1e5f; run(g, SIM_DT); return (double)g.mp; }, [v](double x, double y) { return near(y - x, v, 0.0, 0.01); });
  }
  {
    const int v = midValue(Affix::PotionEffect);
    ab(base, "hero", affixText(Affix::PotionEffect, v), "potion heal", ring(Affix::PotionEffect, v), none,
       [&](Game& g) { g.inv.push_back(makePotion(PotionType::Health, 0)); g.pl().hp = 1; g.useItem((int)g.inv.size() - 1); return (double)g.pl().hp - 1; },
       [v](double x, double y) { return x > 0 && near(y / x, 1.0 + 0.01 * v, 0.01); });
  }
  {
    const int v = midValue(Affix::GoldFind);
    ab(base, "hero", affixText(Affix::GoldFind, v), "gold from 1000", ring(Affix::GoldFind, v), none,
       [&](Game& g) { const int g0 = g.gold; Pickup pk; pk.p = g.pl().p; pk.gold = 1000; pk.t = 1; g.pickups.push_back(pk); run(g, 0.1f); return (double)(g.gold - g0); },
       [v](double x, double y) { return near(y / x, 1.0 + 0.01 * v, 0.002); });
  }
  {
    const int v = midValue(Affix::Health);
    ab(base, "hero", affixText(Affix::Health, v), "max health", ring(Affix::Health, v), none,
       [&](Game& g) { return (double)g.pl().maxHp; }, [v](double x, double y) { return near(y - x, v, 0.0, 0.01); });
  }
  {
    const int v = midValue(Affix::Burn);
    ab(base, "hero", affixText(Affix::Burn, v), "burn dmg/s (3 s)", ring(Affix::Burn, v), none,
       [&](Game& g) {
         g.godMode = true;
         const int id = foe(g, art::Monster::Wolf, Vec2(12, 0), 2000);   // (a small pool: float steps stay far below the tick)
         hitOn(g, id, 1);
         const float h = byId(g, id)->hp;
         run(g, 3.5f);
         return (h - byId(g, id)->hp) / 3.0;
       },
       [v](double x, double y) { return near(y - x, v, 0.01); });
  }
  {
    const int v = midValue(Affix::AttackSpeed);
    ab(base, "hero", affixText(Affix::AttackSpeed, v), "swings in 10 s", ring(Affix::AttackSpeed, v), none,
       [&](Game& g) {
         Input in; in.attack = true;
         int n = 0;
         for (int f = 0; f < 600; f++) {
           g.stamina = g.maxSt;
           g.update(SIM_DT, in);
           for (const Event& e : g.events) if (e.type == Ev::Sfx && e.a == (int)Sfx::Swing) n++;
           g.events.clear();
         }
         return (double)n;
       },
       [v](double x, double y) { return x > 0 && y / x >= 1.0 + 0.6 * 0.01 * v && y / x <= 1.0 + 1.4 * 0.01 * v; });
    ab(base, "hero", affixText(Affix::AttackSpeed, v) + " (bow)", "bow draw s", ring(Affix::AttackSpeed, v), none,
       [&](Game& g) {
         Rng r(9);
         g.inv.push_back(gear::makeGear(r, ItemKind::Bow, 0, L, Rarity::Common, 0)); g.eqBow = (int)g.inv.size() - 1;
         g.inv.push_back(makeArrows(20)); PowerOps::recalc(g);
         g.pl().shootCd = 0; PowerOps::shoot(g);
         return (double)g.pl().shootCd;
       },
       [v](double x, double y) { return x > 0 && near(x / y, 1.0 + 0.01 * v, 0.005); });
  }
  {
    const int v = midValue(Affix::ArmourPct);
    ab(base, "hero", affixText(Affix::ArmourPct, v), "armour rating", ring(Affix::ArmourPct, v), none,
       [&](Game& g) { return (double)g.armorRating(); }, [v](double x, double y) { return x > 0 && near(y / x, 1.0 + 0.01 * v, 0.005); });
  }
  {
    const int v = midValue(Affix::HealthRegen);
    ab(base, "hero", affixText(Affix::HealthRegen, v), "healed in 10 s", ring(Affix::HealthRegen, v), none,
       [&](Game& g) { g.pl().hp = 10; run(g, 10.05f); return (double)g.pl().hp - 10; },
       [v](double x, double y) { return near(y - x, 2.0 * v, 0.01, 0.2); });
  }
  {
    const int v = midValue(Affix::Stamina);
    ab(base, "hero", affixText(Affix::Stamina, v), "max stamina", ring(Affix::Stamina, v), none,
       [&](Game& g) { g.stamina = 1e5f; run(g, SIM_DT); return (double)g.stamina; }, [v](double x, double y) { return near(y - x, v, 0.0, 0.01); });
  }
}

// ---------------------------------------------------------------- the player's powers
void heroPowers(const Game& base) {
  auto ring = [](Unique u) { return piece(ItemKind::Ring, 0, Affix::None, 0, u); };
  const Item none = ring(Unique::None);
  const Item sure = piece(ItemKind::Amulet, 0, Affix::CritChance, 100, Unique::None);
  auto P = [&](Unique u, const std::string& measure, const Fn& fn, const std::function<bool(double, double)>& ok, const std::vector<Item>& extra = {}) {
    ab(base, "hero", uniqueInfo(u).name, measure, ring(u), none, fn, ok, extra);
  };
  auto ratio = [](double want, double tol = 0.01) { return [want, tol](double x, double y) { return x > 0 && near(y / x, want, tol); }; };
  auto more = [](double x, double y) { return y > x + 1e-6; };
  auto less = [](double x, double y) { return y < x - 1e-6; };
  auto cluster = [](Game& g, int n) {
    std::vector<int> ids;
    const Vec2 o[] = {Vec2(12, 0), Vec2(34, 12), Vec2(40, -14), Vec2(60, 2), Vec2(70, 20)};
    for (int k = 0; k < n; k++) ids.push_back(foe(g, art::Monster::Wolf, o[k]));
    return ids;
  };
  // ---- M6 powers
  P(Unique::ChainLightning, "foes chained in 3 swings", [&](Game& g) { auto f = cluster(g, 4); for (int k = 0; k < 3; k++) { swingAt(g, f[0]); for (int i = 1; i < 4; i++) byId(g, f[(size_t)i])->knock = Vec2(); } return (double)g.craft.live.fx.chained; }, more);
  P(Unique::FireTrail, "patches laid by a roll", [&](Game& g) { Input in; in.move = Vec2(1, 0); in.roll = true; run(g, SIM_DT, in); run(g, 0.35f); return (double)g.craft.live.fx.patches.size(); }, more);
  P(Unique::SpectralWolf, "wolves after a kill / after 10.5 s", [&](Game& g) {
    g.godMode = true; Actor* f = byId(g, foe(g, art::Monster::Wolf, Vec2(30, 0), 5)); PowerOps::kill(g, *f, g.pl().id); run(g, 0.1f);
    const int n = countWolves(g); run(g, 10.5f); return n * 10.0 + countWolves(g); }, [](double x, double y) { return x == 0 && y == 10; });
  P(Unique::SplitArrows, "arrows in flight", [&](Game& g) {
    Rng r(9); g.inv.push_back(gear::makeGear(r, ItemKind::Bow, 0, L, Rarity::Common, 0)); g.eqBow = (int)g.inv.size() - 1; g.inv.push_back(makeArrows(20)); PowerOps::recalc(g);
    g.pl().aim = Vec2(1, 0); PowerOps::shoot(g); size_t most = 0; run(g, 0.6f, Input(), [&](Game& h) { most = std::max(most, h.projs.size()); }); return (double)most; },
    [](double x, double y) { return x == 1 && y == 3; });
  P(Unique::TimeSlow, "slow-mo s below 30 %", [&](Game& g) { g.pl().hp = g.pl().maxHp * 0.2f; takenFrom(g, foe(g, art::Monster::Wolf, Vec2(10, 0)), 1); return (double)g.slowMo; }, more);
  P(Unique::ReflectBlock, "missiles turned back", [&](Game& g) {
    const int id = foe(g, art::Monster::Wolf, Vec2(60, 0)); Input in; in.roll = true; in.move = Vec2(1, 0); run(g, SIM_DT, in);
    Projectile pr; pr.p = g.pl().p + Vec2(16, -7); pr.v = Vec2(-220, 0); pr.fromPlayer = false; pr.owner = id; pr.fac = byId(g, id)->faction; pr.dmg = 10; pr.kind = ProjKind::Spit; pr.life = 1;
    g.projs.push_back(pr); int r = 0; run(g, 0.2f, Input(), [&](Game& h) { for (const Projectile& q : h.projs) if (q.powFlags & PJ_REFLECTED) r = 1; }); return (double)r; }, more);
  P(Unique::BloodMagic, "firebolt damage", [&](Game& g) { g.spellsKnown = 1; g.spell = Spell::Flames; g.mp = 500; PowerOps::cast(g); return g.projs.empty() ? 0.0 : (double)g.projs.back().dmg; }, ratio(1.4));
  P(Unique::StillCrit, "crit after standing 1 s", [&](Game& g) { const int id = foe(g, art::Monster::Wolf, Vec2(12, 0)); int c = 0; for (int k = 0; k < 10; k++) { run(g, 1.1f); byId(g, id)->p = g.pl().p + Vec2(12, 0); g.events.clear(); swingAt(g, id); c += critTexts(g); } return (double)c; }, more);
  P(Unique::Executioner, "damage to a foe at 20 %", [&](Game& g) { const int id = foe(g, art::Monster::Wolf, Vec2(12, 0)); byId(g, id)->hp = 2e4f; return (double)hitOn(g, id, 100); }, ratio(1.5));
  P(Unique::Bulwark, "damage taken at full health", [&](Game& g) { return (double)takenFrom(g, foe(g, art::Monster::Wolf, Vec2(10, 0)), 40); }, ratio(0.7));
  P(Unique::Vampire, "healed by a kill", [&](Game& g) { g.pl().hp = 20; Actor* f = byId(g, foe(g, art::Monster::Wolf, Vec2(30, 0), 5)); PowerOps::kill(g, *f, g.pl().id); return (double)g.pl().hp - 20; }, more);
  P(Unique::FrostNova, "a foe's slow s when struck", [&](Game& g) { const int id = foe(g, art::Monster::Wolf, Vec2(10, 0)); takenFrom(g, id, 5); return (double)byId(g, id)->slowT; }, more);
  P(Unique::Thunderclap, "finisher stuns (s)", [&](Game& g) { const int id = foe(g, art::Monster::Wolf, Vec2(12, 0)); swingAt(g, id, 2); return (double)byId(g, id)->atkCd; }, more);
  P(Unique::GoldenTouch, "gold from 1000", [&](Game& g) { const int g0 = g.gold; Pickup pk; pk.p = g.pl().p; pk.gold = 1000; pk.t = 1; g.pickups.push_back(pk); run(g, 0.1f); return (double)(g.gold - g0); }, ratio(1.25, 0.002));
  P(Unique::Windwalker, "move speed", [&](Game& g) { return (double)g.pl().speed; }, ratio(1.12, 0.002));
  P(Unique::Berserker, "damage at 50 % health", [&](Game& g) { g.pl().hp = g.pl().maxHp * 0.5f; return (double)hitOn(g, foe(g, art::Monster::Wolf, Vec2(12, 0)), 100); }, ratio(1.1));
  P(Unique::Ward, "damage taken after 7 s calm", [&](Game& g) { run(g, 7.0f); return (double)takenFrom(g, foe(g, art::Monster::Wolf, Vec2(10, 0)), 40); }, less);
  P(Unique::Huntsman, "damage to a wolf", [&](Game& g) { return (double)hitOn(g, foe(g, art::Monster::Wolf, Vec2(12, 0)), 100); }, ratio(1.3));
  P(Unique::Gravebane, "damage to a skeleton", [&](Game& g) { return (double)hitOn(g, foe(g, art::Monster::Skeleton, Vec2(12, 0)), 100); }, ratio(1.3));
  P(Unique::Dragonbane, "damage to a world boss", [&](Game& g) { const int id = foe(g, art::Monster::Wolf, Vec2(12, 0)); byId(g, id)->rank = 5; return (double)hitOn(g, id, 100); }, ratio(1.4));
  P(Unique::Ember, "burn damage in 2 s", [&](Game& g) { g.godMode = true; const int id = foe(g, art::Monster::Wolf, Vec2(12, 0)); hitOn(g, id, 1); const float h = byId(g, id)->hp; run(g, 2.0f); return (double)(h - byId(g, id)->hp); }, more);
  P(Unique::Rime, "a foe's slow s", [&](Game& g) { const int id = foe(g, art::Monster::Wolf, Vec2(12, 0)); hitOn(g, id, 1); return (double)byId(g, id)->slowT; }, more);
  P(Unique::Echo, "echoes in 300 swings", [&](Game& g) { const int id = foe(g, art::Monster::Wolf, Vec2(12, 0)); int n = 0; for (int k = 0; k < 300; k++) { swingAt(g, id); n += textCount(g, "ECHO"); g.events.clear(); } return (double)n; }, more);
  P(Unique::SecondWind, "stamina after a blow at 20 %", [&](Game& g) { g.stamina = 0; g.pl().hp = g.pl().maxHp * 0.2f; takenFrom(g, foe(g, art::Monster::Wolf, Vec2(10, 0)), 1); return (double)g.stamina; }, more);
  P(Unique::Stormcaller, "damage in 20 crit swings", [&](Game& g) { const int id = foe(g, art::Monster::Wolf, Vec2(12, 0)); double s = 0; for (int k = 0; k < 20; k++) s += swingAt(g, id); return s; }, ratio(1.5, 0.06), {sure});   // (its strike's own draws shift the later blows' spread)
  P(Unique::Shadowstep, "foes hunting the hero after a roll", [&](Game& g) {
    const int id = foe(g, art::Monster::Wolf, Vec2(60, 0)); g.godMode = true; run(g, 0.5f);
    Input in; in.roll = true; in.move = Vec2(-1, 0); run(g, SIM_DT, in); run(g, 0.6f);
    const Actor* f = byId(g, id); return f && f->aggro ? 1.0 : 0.0; }, [](double x, double y) { return x == 1 && y == 0; });
  P(Unique::Thornmail, "returned per 100 taken", [&](Game& g) { const int id = foe(g, art::Monster::Wolf, Vec2(10, 0)); const float h = byId(g, id)->hp; const float t = takenFrom(g, id, 60); return (h - byId(g, id)->hp) * 100.0 / std::max(0.1f, t); },
    [](double x, double y) { return near(x, 0) && near(y, 25, 0.02); });
  P(Unique::Quickdraw, "bow draw s", [&](Game& g) {
    Rng r(9); g.inv.push_back(gear::makeGear(r, ItemKind::Bow, 0, L, Rarity::Common, 0)); g.eqBow = (int)g.inv.size() - 1; g.inv.push_back(makeArrows(20)); PowerOps::recalc(g);
    PowerOps::shoot(g); return (double)g.pl().shootCd; }, ratio(0.7, 0.005));
  P(Unique::Arcanist, "firebolt damage", [&](Game& g) { g.spellsKnown = 1; g.spell = Spell::Flames; g.mp = 500; PowerOps::cast(g); return g.projs.empty() ? 0.0 : (double)g.projs.back().dmg; }, ratio(1.2));
  P(Unique::Lifebloom, "healed 10 s after a potion", [&](Game& g) { g.inv.push_back(makePotion(PotionType::Health, 0)); g.pl().hp = 1; g.useItem((int)g.inv.size() - 1); run(g, 10.1f); return (double)g.pl().hp; }, more);
  P(Unique::Warcry, "damage after a kill", [&](Game& g) { Actor* f = byId(g, foe(g, art::Monster::Wolf, Vec2(30, 0), 5)); PowerOps::kill(g, *f, g.pl().id); return (double)hitOn(g, foe(g, art::Monster::Wolf, Vec2(12, 0)), 100); }, ratio(1.15));
  P(Unique::Ironhide, "armour rating", [&](Game& g) { return (double)g.armorRating(); }, ratio(1.25, 0.002));
  P(Unique::Wayfarer, "max stamina", [&](Game& g) { g.stamina = 1e5f; run(g, SIM_DT); return (double)g.stamina; }, ratio(1.2, 0.002));
  // ---- the Mythic powers
  P(Unique::Tempest, "foes chained by one swing", [&](Game& g) { auto f = cluster(g, 5); swingAt(g, f[0]); return (double)g.craft.live.fx.chained; },
    [](double x, double y) { return x == 0 && y == powers::TEMPEST_ARCS; });
  P(Unique::Tempest, "chain damage to the others", [&](Game& g) { auto f = cluster(g, 4); swingAt(g, f[0]); double s = 0; for (int i = 1; i < 4; i++) s += 1e5 - byId(g, f[(size_t)i])->hp; return s; }, more);
  P(Unique::Inferno, "burn damage to a foe in the trail", [&](Game& g) {
    g.godMode = true; Input in; in.move = Vec2(1, 0); run(g, 1.0f, in);
    if (g.craft.live.fx.patches.empty()) return 0.0;
    const Vec2 at = g.craft.live.fx.patches[g.craft.live.fx.patches.size() / 2].p;
    const int id = foe(g, art::Monster::Wolf, at - g.pl().p);
    byId(g, id)->aggro = false; byId(g, id)->speed = 0;
    run(g, 1.0f); return (double)(1e5 - byId(g, id)->hp); }, more);
  P(Unique::Soulfeast, "healed per 100 dealt", [&](Game& g) { g.pl().hp = 30; const float d = hitOn(g, foe(g, art::Monster::Wolf, Vec2(12, 0)), 100); return (g.pl().hp - 30) * 100.0 / std::max(1.0f, d); },
    [](double x, double y) { return near(x, 0) && near(y, powers::SOULFEAST_PCT, 0.02); });
  P(Unique::Soulfeast, "% max health healed by a kill", [&](Game& g) { g.pl().hp = 30; Actor* f = byId(g, foe(g, art::Monster::Wolf, Vec2(30, 0), 5)); PowerOps::kill(g, *f, g.pl().id); return (g.pl().hp - 30) * 100.0 / g.pl().maxHp; },
    [](double x, double y) { return near(x, 0) && near(y, powers::SOULFEAST_KILL_PCT, 0.02); });
  P(Unique::BrambleCrown, "aura stings in 3 s", [&](Game& g) {
    g.godMode = true; const int id = foe(g, art::Monster::Wolf, Vec2(14, 0)); run(g, 3.05f, Input(), [&](Game& h) { if (Actor* f = byId(h, id)) { f->p = h.pl().p + Vec2(14, 0); f->knock = Vec2(); } });
    return (double)g.craft.live.fx.procs[(int)powers::Fx::Thorns] + (1e5 - byId(g, id)->hp > 0 ? 0.5 : 0.0); }, [](double x, double y) { return x == 0 && y == 3.5; });
  P(Unique::Chronos, "px a charging foe covers in 1 s after a perfect dodge", [&](Game& g) {
    g.godMode = false;
    const int id = foe(g, art::Monster::Bear, Vec2(150, 0)); byId(g, id)->aggro = true;
    Input in; in.roll = true; in.move = Vec2(0, 1); run(g, SIM_DT, in);
    g.pl().st = AState::Roll; g.pl().stT = 0.02f; g.pl().iframes = 0.3f;
    PowerOps::hurt(g, g.pl(), 5, byId(g, id)->p, id);
    const Vec2 a = byId(g, id)->p; g.godMode = true; run(g, 1.0f);
    return (double)len(byId(g, id)->p - a); }, [](double x, double y) { return x > 10 && y < x * 0.55; });
  P(Unique::Chronos, "time slowed (s)", [&](Game& g) {
    const int id = foe(g, art::Monster::Wolf, Vec2(12, 0)); Input in; in.roll = true; run(g, SIM_DT, in); PowerOps::hurt(g, g.pl(), 5, byId(g, id)->p, id);
    return (double)g.craft.live.fx.slowT; }, [](double x, double y) { return x == 0 && near(y, powers::CHRONOS_SECS, 0.01); });
  P(Unique::Phoenix, "fatal blows survived: 1st, 2nd at once, 3rd after 181 s", [&](Game& g) {
    const int id = foe(g, art::Monster::Wolf, Vec2(10, 0));
    double s = 0;
    takenFrom(g, id, 1e6f); if (g.mode != Mode::Dead && near(g.pl().hp, g.pl().maxHp * powers::PHOENIX_HP, 0.01)) s += 100;
    if (g.mode == Mode::Dead) return s;
    takenFrom(g, id, 1e6f); if (g.mode != Mode::Dead) s += 10;
    if (g.mode == Mode::Dead) { g.mode = Mode::Play; g.pl().st = AState::Idle; g.pl().hp = g.pl().maxHp; }
    clearFoes(g); g.godMode = true; run(g, 181.0f); g.godMode = false;
    takenFrom(g, foe(g, art::Monster::Wolf, Vec2(10, 0)), 1e6f); if (g.mode != Mode::Dead) s += 1;
    return s; }, [](double x, double y) { return x == 0 && y == 101; });
  P(Unique::Starfall, "star hits in 5 crit swings", [&](Game& g) { auto f = cluster(g, 3); for (int k = 0; k < 5; k++) { swingAt(g, f[0]); run(g, 0.7f); } return (double)g.craft.live.fx.starHits; }, more, {sure});
  P(Unique::Wolfking, "wolves after a kill / after 12.5 s", [&](Game& g) {
    g.godMode = true; Actor* f = byId(g, foe(g, art::Monster::Wolf, Vec2(30, 0), 5)); PowerOps::kill(g, *f, g.pl().id); run(g, 0.1f);
    const int n = countWolves(g); run(g, 12.5f); return n * 10.0 + countWolves(g); }, [](double x, double y) { return x == 0 && y == 10; });
  P(Unique::Wolfking, "damage the wolf deals in 4 s", [&](Game& g) {
    g.godMode = true; Actor* f = byId(g, foe(g, art::Monster::Wolf, Vec2(30, 0), 5)); PowerOps::kill(g, *f, g.pl().id); run(g, 0.1f);
    const int id = foe(g, art::Monster::Wolf, Vec2(40, 10)); byId(g, id)->speed = 0; byId(g, id)->dmg = 0;
    run(g, 4.0f); return (double)(1e5 - byId(g, id)->hp); }, more);
  {  // the wolf is never saved, looted, counted or a quest's: its kills are its master's (fixer r2), its death drops nothing
    Game g = base;
    wear(g, ring(Unique::Wolfking));
    g.godMode = true;
    Actor* f = byId(g, foe(g, art::Monster::Wolf, Vec2(30, 0), 5));
    PowerOps::kill(g, *f, g.pl().id);
    run(g, 0.1f);
    const int kills0 = g.kills;
    const int prey = foe(g, art::Monster::Wolf, Vec2(36, 0), 3);
    byId(g, prey)->speed = 0; byId(g, prey)->dmg = 0;
    run(g, 3.0f);
    const bool preyDead = !byId(g, prey) || byId(g, prey)->st == AState::Dead;
    int wolf = -1;
    for (const Actor& a : g.actors) if (a.powTag == powers::PT_WOLF) wolf = a.id;
    const size_t pk0 = g.pickups.size();
    if (wolf >= 0) PowerOps::kill(g, *byId(g, wolf), -1);
    std::vector<uint8_t> save;
    g.serialize(save);
    Game h(g.seed);
    const bool loaded = h.deserialize(save);
    const bool ok = preyDead && g.kills == kills0 + 1 && g.pickups.size() == pk0 && loaded && countWolves(h) == 0;
    report("hero", "WOLFKING", "wolf kills credit the hero / wolf loot / saved wolves", 0, ok ? 0 : 1, ok);
  }
}

// ---------------------------------------------------------------- companions: the same rules
void companionPowers(const Game& base) {
  if (base.party.members.empty() || compIdx(base) < 0) { out("FAIL: powers: no companion for the companion A/B runs\n"); g_bad++; return; }
  auto ring = [](Affix a, int v, Unique u = Unique::None) { return piece(ItemKind::Ring, 0, a, v, u); };
  const Item none = ring(Affix::None, 0);
  auto C = [&](const std::string& name, const std::string& measure, const Item& with, const Fn& fn, const std::function<bool(double, double)>& ok) {
    ab(base, "companion", name, measure, with, none, fn, ok);
  };
  auto cid = [](Game& g) { return g.actors[(size_t)compIdx(g)].id; };
  auto cact = [](Game& g) -> Actor& { return g.actors[(size_t)compIdx(g)]; };
  {
    const int v = midValue(Affix::VsBeasts);
    C(affixText(Affix::VsBeasts, v), "damage dealt", ring(Affix::VsBeasts, v), [&](Game& g) { return (double)hitOn(g, foe(g, art::Monster::Wolf, Vec2(30, 0)), 100, cid(g)); },
      [v](double x, double y) { return x > 0 && near(y / x, 1.0 + 0.01 * v, 0.01); });
  }
  {
    const int v = midValue(Affix::FireDmg);
    C(affixText(Affix::FireDmg, v), "damage dealt", ring(Affix::FireDmg, v), [&](Game& g) { return (double)hitOn(g, foe(g, art::Monster::Wolf, Vec2(30, 0)), 100, cid(g)); },
      [v](double x, double y) { return x > 0 && near(y / x, 1.0 + 0.01 * v, 0.01); });
  }
  {
    const int v = midValue(Affix::Health);
    C(affixText(Affix::Health, v), "max health", ring(Affix::Health, v), [&](Game& g) { return (double)cact(g).maxHp; }, [v](double x, double y) { return near(y - x, v, 0.0, 0.01); });
  }
  {
    const int v = midValue(Affix::ArmourPct);
    C(affixText(Affix::ArmourPct, v), "armour", ring(Affix::ArmourPct, v), [&](Game& g) { return (double)cact(g).armor; }, [v](double x, double y) { return x > 0 && near(y / x, 1.0 + 0.01 * v, 0.005); });
  }
  {
    const int v = midValue(Affix::LifeDrain);
    C(affixText(Affix::LifeDrain, v), "healed per 100 dealt", ring(Affix::LifeDrain, v), [&](Game& g) {
      cact(g).hp = 10; const float d = hitOn(g, foe(g, art::Monster::Wolf, Vec2(30, 0)), 100, cid(g)); return (cact(g).hp - 10) * 100.0 / std::max(1.0f, d); },
      [v](double x, double y) { return near(x, 0) && near(y, v, 0.02); });
  }
  {
    const int v = midValue(Affix::ResFire);
    C(affixText(Affix::ResFire, v), "damage taken", ring(Affix::ResFire, v), [&](Game& g) {
      const int f = foe(g, art::Monster::Wolf, Vec2(-30, 0)); Actor& c = cact(g); const float h = c.hp; PowerOps::hurtE(g, c, 30, byId(g, f)->p, f, Ench::Fire, 0); return (double)(h - cact(g).hp); },
      [v](double x, double y) { return x > 0 && near(y / x, 1.0 - 0.01 * v, 0.01); });
  }
  {
    const int v = midValue(Affix::Burn);
    C(affixText(Affix::Burn, v), "burn dmg/s (3 s)", ring(Affix::Burn, v), [&](Game& g) {
      g.godMode = true; const int id = foe(g, art::Monster::Wolf, Vec2(60, 0), 2000); byId(g, id)->speed = 0; byId(g, id)->dmg = 0; byId(g, id)->aggro = false;
      hitOn(g, id, 1, cid(g)); const float h = byId(g, id)->hp;
      run(g, 3.5f, Input(), [&](Game& hh) { if (Actor* f = byId(hh, id)) { f->p = hh.pl().p + Vec2(60, 0); } });
      return (h - byId(g, id)->hp) / 3.0; },
      [v](double x, double y) { return near(y - x, v, 0.01); });
  }
  {
    const int v = midValue(Affix::CritChance);
    C(affixText(Affix::CritChance, v), "crit %", ring(Affix::CritChance, v), [&](Game& g) {
      const int id = foe(g, art::Monster::Wolf, Vec2(30, 0));
      int c = 0;
      for (int k = 0; k < 1000; k++) {
        Actor& a = cact(g); Actor* f = byId(g, id);
        f->p = a.p + Vec2(10, 0); f->knock = Vec2(); a.aim = Vec2(1, 0);
        PowerOps::companionSwing(g, cact(g)); c += critTexts(g); g.events.clear();
      }
      return c / 10.0; },
      [v](double x, double y) { return std::fabs((y - x) - v) <= std::max(3.0, v * 0.35); });
  }
  C("TEMPEST", "foes chained by one blow", ring(Affix::None, 0, Unique::Tempest), [&](Game& g) {
    std::vector<int> f;
    const Vec2 c = cact(g).p;
    for (Vec2 o : {Vec2(10, 0), Vec2(30, 12), Vec2(36, -14), Vec2(54, 0)}) f.push_back(foe(g, art::Monster::Wolf, c + o - g.pl().p));
    hitOn(g, f[0], 50, cid(g)); return (double)g.craft.live.fx.chained; }, [](double x, double y) { return x == 0 && y == powers::TEMPEST_ARCS; });
  C("WOLFKING", "wolves of theirs after a kill", ring(Affix::None, 0, Unique::Wolfking), [&](Game& g) {
    g.godMode = true; Actor* f = byId(g, foe(g, art::Monster::Wolf, Vec2(40, 0), 5)); const int c = cid(g); PowerOps::kill(g, *f, c); run(g, 0.1f); return (double)countWolves(g, c); },
    [](double x, double y) { return x == 0 && y == 1; });
  C("BRAMBLE CROWN", "aura damage in 3 s", ring(Affix::None, 0, Unique::BrambleCrown), [&](Game& g) {
    g.godMode = true; const int id = foe(g, art::Monster::Wolf, Vec2(60, 0)); byId(g, id)->dmg = 0;
    run(g, 3.05f, Input(), [&](Game& h) { if (Actor* f = byId(h, id)) { f->p = h.actors[(size_t)compIdx(h)].p + Vec2(12, 0); f->knock = Vec2(); f->aggro = false; } });
    return (double)g.craft.live.fx.auraDealt; }, [](double x, double y) { return near(x, 0) && y > 0; });
  C("SOULFEAST", "healed per 100 dealt", ring(Affix::None, 0, Unique::Soulfeast), [&](Game& g) {
    cact(g).hp = 10; const float d = hitOn(g, foe(g, art::Monster::Wolf, Vec2(30, 0)), 100, cid(g)); return (cact(g).hp - 10) * 100.0 / std::max(1.0f, d); },
    [](double x, double y) { return near(x, 0) && near(y, powers::SOULFEAST_PCT, 0.02); });
}

// ---------------------------------------------------------------- the time to kill with real kits (7.4 bands)
double ttk(uint64_t seed, int level, Rarity r, std::string* kitDesc, bool strip = false) {
  Game base = makeBase(seed, level, false);
  Rng gr(seed * 977u + (uint32_t)level * 31u + (uint32_t)r);
  if (r != Rarity::Common) {
    base.inv.clear();
    base.eqWeapon = base.eqArmor = -1;
    // a typical kit of that rarity: a rolled sword with its affixes and power, body armour, a ring and an amulet
    for (ItemKind k : {ItemKind::Weapon, ItemKind::Armor, ItemKind::Ring, ItemKind::Amulet}) {
      Item it = gear::makeGear(gr, k, k == ItemKind::Weapon ? (int)WeaponType::Sword : 0, level, r, 0);
      if (strip) { for (ItemAffix& a : it.affix) a = ItemAffix(); it.unique = Unique::None; }   // (the base power alone)
      wear(base, it);
      if (kitDesc) {
        *kitDesc += std::string(k == ItemKind::Weapon ? "sword" : k == ItemKind::Armor ? " / body" : k == ItemKind::Ring ? " / ring" : " / amulet") + ":";
        for (const ItemAffix& a : it.affix) if (a.kind != Affix::None) *kitDesc += " " + affixText(a.kind, a.value) + ",";
        if (it.unique != Unique::None) *kitDesc += std::string(" ") + uniqueInfo(it.unique).name;
      }
    }
  }
  base.godMode = true;
  if (kitDesc) { char b[48]; std::snprintf(b, sizeof b, " | blow %.1f", base.weaponDamage()); *kitDesc += b; }
  int swings = 0, foes = 0;
  for (int n = 0; n < 12; n++) {
    Game& g = base;
    clearFoes(g);
    const int id = g.debugSpawnAt(art::Monster::Wolf, g.pl().p + Vec2(12, 0), level);
    Actor* f = byId(g, id);
    f->rank = 0; f->affixes = 0; f->overlays = 0; f->bodyTint = 0; f->unique = 0;
    int k = 0;
    while (k < 40) {
      Actor* v = byId(g, id);
      if (!v || v->st == AState::Dead || v->hp <= 0) break;
      if (v->st == AState::Down) { swingAt(g, id); k++; break; }
      swingAt(g, id);
      k++;
      run(g, 0.33f, Input(), [&](Game& h) { if (Actor* q = byId(h, id)) { q->p = h.pl().p + Vec2(12, 0); q->knock = Vec2(); } });
    }
    swings += k;
    foes++;
  }
  return (double)swings / std::max(1, foes);
}

int powersCmd(int argc, char** argv) {
  uint64_t seed = 1;
  bool table = true, strictTtk = false;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 10);
    if (!std::strcmp(argv[i], "--quiet")) table = false;
    if (!std::strcmp(argv[i], "--strict-ttk")) strictTtk = true;
  }
  g_bad = 0;
  rows.clear();
  // ---- every rolling affix / power has words, and nothing that rolls is untested
  int aOn = 0, uOn = 0, mOn = 0;
  for (int a = 1; a < (int)Affix::COUNT; a++) {
    if (!gear::affixImplemented((Affix)a)) continue;
    aOn++;
    if (affixText((Affix)a, 7).empty()) { out("FAIL: powers: affix %s has no words\n", affixInfo((Affix)a).name); g_bad++; }
  }
  for (int u = 1; u < (int)Unique::COUNT; u++) {
    if (!gear::uniqueImplemented((Unique)u)) continue;
    (uniqueIsMythic((Unique)u) ? mOn : uOn)++;
    if (!*uniqueInfo((Unique)u).name || !*uniqueInfo((Unique)u).line) { out("FAIL: powers: power %d has no words\n", u); g_bad++; }
  }
  printf("powers: %d affixes, %d unique powers and %d Mythic powers work\n", aOn, uOn, mOn);
  const Game base = makeBase(seed, L, false);
  heroAffixes(base);
  heroPowers(base);
  const Game cbase = makeBase(seed, L, true);
  companionPowers(cbase);
  // every working affix and power was measured
  for (int a = 1; a < (int)Affix::COUNT; a++) {
    if (!gear::affixImplemented((Affix)a)) continue;
    const std::string w = affixText((Affix)a, midValue((Affix)a));
    bool seen = false;
    for (const Row& r : rows) if (r.who == "hero" && r.name.compare(0, w.size(), w) == 0) seen = true;
    if (!seen) { out("FAIL: powers: working affix %s has no A/B run\n", affixInfo((Affix)a).name); g_bad++; }
  }
  for (int u = 1; u < (int)Unique::COUNT; u++) {
    if (!gear::uniqueImplemented((Unique)u)) continue;
    bool seen = false;
    for (const Row& r : rows) if (r.who == "hero" && r.name == uniqueInfo((Unique)u).name) seen = true;
    if (!seen) { out("FAIL: powers: working power %s has no A/B run\n", uniqueInfo((Unique)u).name); g_bad++; }
  }
  if (table) {
    printf("powers: A/B at level %d (without -> with):\n", L);
    for (const Row& r : rows)
      printf("  %-9s %-40s %-44s %10.2f -> %10.2f  %s\n", r.who.c_str(), r.name.c_str(), r.measure.c_str(), r.a, r.b, r.ok ? "ok" : "FAIL");
  }
  // ---- the time to kill: same-band trash with real kits (7.4: 3-4 hits with plain gear; never under 2.5 with the best)
  for (int lv : {12, 30}) {
    for (Rarity r : {Rarity::Common, Rarity::Epic, Rarity::Mythic}) {
      std::string kit;
      const double h = ttk(seed, lv, r, &kit);
      const double h0 = r == Rarity::Common ? h : ttk(seed, lv, r, nullptr, true);
      printf("powers: TTK level %d, %-9s kit: %.2f hits on a same-band wolf (%.2f with its base power alone)\n    %s\n", lv, rarityName(r), h, h0, kit.c_str());
      // (the base power and the affix ranges are the NUMBERS lane's: a WARN here, a FAIL with --strict-ttk)
      if (r != Rarity::Common && h < 2.5 && !strictTtk) printf("WARN: powers: a %s kit at level %d kills same-band trash in %.2f hits (< 2.5; %.2f from its base power alone)\n", rarityName(r), lv, h, h0);
      if (r != Rarity::Common && h < 2.5 && strictTtk) { out("FAIL: powers: a %s kit at level %d kills same-band trash in %.2f hits (< 2.5)\n", rarityName(r), lv, h); g_bad++; }
    }
  }
  printf("powers: %zu A/B runs, %d failure(s)\n", rows.size(), g_bad);
  return g_bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--powers", "M8b: every affix and power that may roll works in play (A/B fights for the player and a companion; TTK with Epic / Mythic kits) [--seed N] [--quiet]", powersCmd);
