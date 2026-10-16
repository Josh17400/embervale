// rpg_test --spells: M9 magic (VISION_PLAN 6.3, 13 M9, 15.5, 15.29.3, 15.29.5). MAGIC lane.
// The owner's suite:
//   - the tables: every spell named, on its budget (+-5 %, section 6.3), ids unique and found again; every school a
//     novice-to-master list (>= 6 spells, tiers 0..4, a spectacle at the top) with its own silhouette (particle and
//     motion); the classic three mapped both ways; statuses, surfaces and the combo table; the perk trees;
//   - in play: every school spell (and every tradition's) cast at a dummy deals its power (+-5 %: the budget in the
//     game, not only in the table); the classic three keep their pre-M9 blows and fly as ProjKind::Spell;
//   - the BG3 layer: upcast (hold: Charge phases, magnitudeOf), dual-cast (gated by the perk), concentration (a heavy
//     blow breaks it), reactions (a ward / spell breaker answering a telegraphed cast), every status's effect, every
//     combo (rain dousing fire too), the loadout rule (at rest or camp);
//   - traditions: generated per culture, deterministic (two generations agree), 8..12 spells with their ranks, DPS
//     within +-12 % of Common at equal item level, the mainland signatures (Starcalling, Tidebinding, Ancestor calls,
//     Emberforge, Beastbinding), mastery and attunement, the trial;
//   - learning: tomes (the skill gate, shelves, loot sources), teachers (a mage's lessons, attunement, a tradition
//     spell taught), summons, the book block (version 2 and version 1).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/magic_ops.h"
#include "tools/tests/tests.h"

namespace {

int m_bad = 0;
bool m_table = true;
#define SP_FAIL(...) do { out("FAIL: spells: " __VA_ARGS__); m_bad++; } while (0)

using magic::Essence;
using magic::Form;
using magic::Phase;
using magic::School;
using magic::SpellDef;
using magic::Status;
using magic::Surface;

void spRun(Game& g, float secs, std::vector<Event>* keep = nullptr, const Input* hold = nullptr) {
  const int n = (int)std::lround(secs / SIM_DT);
  for (int f = 0; f < n; f++) {
    g.update(SIM_DT, hold ? *hold : Input());
    if (keep) keep->insert(keep->end(), g.events.begin(), g.events.end());
    g.events.clear();
    if (g.mode == Mode::LevelUp) g.mode = Mode::Play;
  }
}
int phases(const std::vector<Event>& ev, Phase ph, uint32_t spell) {
  int n = 0;
  for (const Event& e : ev) if (e.type == Ev::Spell && e.a == (int)ph && (!spell || e.spell == spell)) n++;
  return n;
}

// ---------------------------------------------------------------- the tables
void checkTables() {
  std::set<uint32_t> ids;
  int perSchool[magic::SCHOOLS] = {};
  bool tierSeen[magic::SCHOOLS][magic::TIERS] = {};
  int maxSize[magic::SCHOOLS] = {};
  std::map<int, std::map<int, int>> look;   // school -> (particle * 16 + motion) -> count
  for (const SpellDef& s : magic::schoolSpells()) {
    if (!s.id || !ids.insert(s.id).second) SP_FAIL("spell id %u repeats\n", s.id);
    if (s.name.empty()) SP_FAIL("spell %u has no name\n", s.id);
    if (magic::spellById(s.id) != &s) SP_FAIL("%s is not found by its id\n", s.name.c_str());
    if ((int)s.school >= magic::SCHOOLS) { SP_FAIL("%s has no school\n", s.name.c_str()); continue; }
    perSchool[(int)s.school]++;
    tierSeen[(int)s.school][std::min<int>(s.tier, magic::TIERS - 1)] = true;
    maxSize[(int)s.school] = std::max<int>(maxSize[(int)s.school], s.fx.size);
    look[(int)s.school][(int)s.fx.particle * 16 + (int)s.fx.motion]++;
    const float b = magic::budgetOf(s);
    if (b > 0 && std::fabs(s.power / b - 1.0f) > 0.05f) SP_FAIL("%s: power %.1f vs budget %.1f (%.1f%%)\n", s.name.c_str(), s.power, b, 100.0f * (s.power / b - 1.0f));
    if (s.cost <= 0) SP_FAIL("%s costs nothing\n", s.name.c_str());
    if (!s.fx.ramp[0] || !s.fx.ramp[4]) SP_FAIL("%s has no colour ramp\n", s.name.c_str());
    if (s.tier >= 4 && s.fx.size < 3) SP_FAIL("%s is a master spell without a spectacle (size %d)\n", s.name.c_str(), s.fx.size);
    if (m_table)
      out("spells: %-16s %-11s %-6s %-7s tier %d  power %6.1f  budget %6.1f (%+.1f%%)  cost %3.0f  cd %4.1f  dps %5.1f  size %d\n", s.name.c_str(),
          magic::schoolName(s.school), magic::formName(s.form), magic::essenceName(s.ess), s.tier, s.power, b, b > 0 ? 100.0f * (s.power / b - 1.0f) : 0.0f,
          s.cost, s.cooldown, magic::sustainedDps(s), s.fx.size);
  }
  int sig[magic::SCHOOLS] = {};
  for (int k = 0; k < magic::SCHOOLS; k++) {
    const char* nm = magic::schoolName((School)k);
    if (perSchool[k] < 6) SP_FAIL("the school %s has %d spells (wanted 6+)\n", nm, perSchool[k]);
    for (int t = 0; t < magic::TIERS; t++) if (!tierSeen[k][t]) SP_FAIL("the school %s has no %s spell\n", nm, magic::tierName(t));
    if (maxSize[k] < 4) SP_FAIL("the school %s has no spectacle at the top (size %d)\n", nm, maxSize[k]);
    int best = 0, bn = 0;
    for (const auto& kv : look[k]) if (kv.second > bn) { bn = kv.second; best = kv.first; }
    sig[k] = best;
    int n = 0;
    const skills::PerkDef* P = magic::schoolPerks(magic::schoolSkill((School)k), n);
    if (n < 6 || n > 12) SP_FAIL("the school %s has %d perks (wanted 6..10)\n", nm, n);
    std::set<int> spots;
    for (int i = 0; i < n; i++) {
      if (!spots.insert(P[i].x * 100 + P[i].y).second) SP_FAIL("%s perk %s sits on another star\n", nm, P[i].name);
      if (P[i].parent >= i || P[i].parent2 >= i) SP_FAIL("%s perk %s hangs from a later star\n", nm, P[i].name);
    }
    if (m_table) out("spells: school %-11s %2d spells, %2d perks\n", nm, perSchool[k], n);
  }
  for (int a = 0; a < magic::SCHOOLS; a++)
    for (int b = a + 1; b < magic::SCHOOLS; b++)
      if (sig[a] == sig[b]) SP_FAIL("the schools %s and %s share a silhouette\n", magic::schoolName((School)a), magic::schoolName((School)b));
  for (int l = 0; l < 3; l++) if (magic::legacySpellOf(magic::legacySpellId(l)) != l || !magic::spellById(magic::legacySpellId(l))) SP_FAIL("classic spell %d does not map\n", l);
  for (int s = 0; s < magic::STATUSES; s++) if (!*magic::statusName((Status)s)) SP_FAIL("status %d has no name\n", s);
  for (int s = 1; s < (int)Surface::COUNT; s++) if (!*magic::surfaceName((Surface)s)) SP_FAIL("surface %d has no name\n", s);
  struct C { Surface g; Essence e; Surface want; };
  const C combos[] = {{Surface::Water, Essence::Fire, Surface::Steam}, {Surface::Water, Essence::Frost, Surface::Ice}, {Surface::Water, Essence::Storm, Surface::Shock},
                      {Surface::Water, Essence::Venom, Surface::Poison}, {Surface::Oil, Essence::Fire, Surface::Fire}, {Surface::Ice, Essence::Fire, Surface::Water},
                      {Surface::Fire, Essence::Frost, Surface::Steam}, {Surface::Fire, Essence::Tide, Surface::Steam}, {Surface::Poison, Essence::Fire, Surface::Fire},
                      {Surface::Blood, Essence::Frost, Surface::Ice}, {Surface::Blood, Essence::Storm, Surface::Shock}, {Surface::Shock, Essence::Frost, Surface::Ice}};
  for (const C& c : combos)
    if (magic::comboOf(c.g, c.e) != c.want) SP_FAIL("combo %s + %s is not %s\n", magic::surfaceName(c.g), magic::essenceName(c.e), magic::surfaceName(c.want));
  if (magic::magnitudeOf(0, false) != 1.0f || std::fabs(magic::magnitudeOf(3, false) - 2.05f) > 1e-4f || std::fabs(magic::magnitudeOf(1, true) - 1.6f * 1.35f) > 1e-4f)
    SP_FAIL("magnitudeOf is wrong\n");
  if (magic::attuneSlots(1) != 1 || magic::attuneSlots(14) != 1 || magic::attuneSlots(15) != 2 || magic::attuneSlots(35) != 3) SP_FAIL("attunement slots are not 1 / 2 at 15 / 3 at 35\n");
  if (magic::masteryRank(299, false) != 1 || magic::masteryRank(300, false) != 2 || magic::masteryRank(1200, false) != 2 || magic::masteryRank(1200, true) != 3)
    SP_FAIL("mastery ranks are not 300 / 1200 + trial\n");
}

void checkBook() {
  magic::Book a;
  a.learn(magic::SPELL_FIREBOLT); a.learn(4); a.learn(magic::SPELL_MEND);
  a.sel = 2; a.mastery[77] = 300; a.attuned = {77}; a.trials = 1; a.res[(int)magic::Resource::Heat] = 40;
  a.lands = {0x1234567890ull, 42}; a.trialKills[0] = 7;
  if (a.slot[0] != magic::SPELL_FIREBOLT || a.slot[1] != 4 || a.slotOf(magic::SPELL_MEND) != 2) SP_FAIL("learning does not fill the quick slots in order\n");
  std::vector<uint8_t> blk, blk2;
  a.serialize(blk);
  magic::Book b;
  if (!b.deserialize(blk)) { SP_FAIL("the magic block does not read back\n"); return; }
  b.serialize(blk2);
  if (blk != blk2 || b.sel != 2 || b.mastery[77] != 300 || b.lands.size() != 2 || b.lands[0] != 0x1234567890ull || b.trialKills[0] != 7) SP_FAIL("the magic block round trip changed it\n");
  // version 1 (tests/fixtures/save_v20.bin's): the same fields without the lands and the trial kills
  std::vector<uint8_t> v1(blk.begin(), blk.end() - (4 + 16 + 1 + 4));
  v1[0] = 1;
  magic::Book c;
  if (!c.deserialize(v1) || !c.knows(4) || c.mastery[77] != 300 || !c.lands.empty()) SP_FAIL("a version-1 magic block does not read\n");
  std::vector<uint8_t> v9 = blk;
  v9[0] = 9;
  if (c.deserialize(v9)) SP_FAIL("an unknown magic block version was read\n");
}

// ---------------------------------------------------------------- traditions (generation)
std::vector<cult::Culture> sampleCultures() {
  std::vector<cult::Culture> v;
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++)
    for (uint32_t s : {11u, 2027u, 90001u}) {
      cult::Culture c = cult::Atlas::make((cult::Archetype)a, s);
      if (!c.id) c.id = cult::familyId(a * 7 + 3, (int32_t)(s % 1000u));
      v.push_back(c);
    }
  return v;
}
bool sameSpell(const SpellDef& a, const SpellDef& b) {
  if (a.id != b.id || a.name != b.name || a.form != b.form || a.ess != b.ess || a.tier != b.tier || a.rank != b.rank || a.power != b.power ||
      a.cost != b.cost || a.cooldown != b.cooldown || a.school != b.school || a.res != b.res || a.fx.particle != b.fx.particle || a.fx.size != b.fx.size)
    return false;
  for (int i = 0; i < 5; i++) if (a.fx.ramp[i] != b.fx.ramp[i]) return false;
  return true;
}
// the Common Arcana reference of a tradition spell at equal item level: the schools' spells of its form and tier (the
// rank and the essence of the tradition factored out), else the budget of the same form, tier and mods
float commonDps(const SpellDef& s) {
  float sum = 0;
  int n = 0;
  for (const SpellDef& c : magic::schoolSpells())
    if (c.form == s.form && c.tier == s.tier && magic::damaging(c) && magic::legacySpellOf(c.id) < 0) { sum += magic::sustainedDps(c) / magic::essenceMult(c.ess); n++; }
  if (n) return sum / (float)n;
  SpellDef r = s;
  r.rank = 1; r.ess = Essence::Fire; r.tradition = 0;
  r.power = magic::budgetOf(r);
  return magic::sustainedDps(r);
}
void checkTraditions() {
  int made = 0;
  std::map<int, int> sigs;
  std::set<std::string> names;
  for (const cult::Culture& c : sampleCultures()) {
    std::vector<SpellDef> s1, s2;
    const magic::Tradition t1 = magic::generateTradition(c, s1);
    const magic::Tradition t2 = magic::generateTradition(c, s2);
    if (t1.id != t2.id || t1.name != t2.name || t1.sig != t2.sig || t1.spells != t2.spells || s1.size() != s2.size()) { SP_FAIL("tradition of %s: two generations differ\n", c.name.c_str()); continue; }
    for (size_t i = 0; i < s1.size(); i++) if (!sameSpell(s1[i], s2[i])) SP_FAIL("tradition %s: spell %zu differs between generations\n", t1.name.c_str(), i);
    const magic::Tradition& t = magic::traditionOf(c);
    if (t.id != t1.id) SP_FAIL("traditionOf(%s) is not its generation\n", c.name.c_str());
    if (!t.id) {
      if (c.archetype != cult::Archetype::Heartland) SP_FAIL("%s (%s) teaches only Common Arcana\n", c.name.c_str(), cult::archetypeName(c.archetype));
      continue;
    }
    made++;
    sigs[(int)t.sig]++;
    names.insert(t.name);
    using G = magic::Signature;
    if (t.sig != G::Star && t.sig != G::Tide && t.sig != G::Ancestor && t.sig != G::Ember && t.sig != G::Beast) SP_FAIL("%s has a hidden kingdom's signature\n", t.name.c_str());
    if (t.spells.size() < 8 || t.spells.size() > 12) SP_FAIL("%s has %zu spells (8..12)\n", t.name.c_str(), t.spells.size());
    int r[4] = {};
    float ratio = 0;
    int dn = 0;
    for (size_t i = 0; i < t.spells.size(); i++) {
      const SpellDef* d = magic::spellById(t.spells[i]);
      if (!d) { SP_FAIL("%s: spell %u is unknown to spellById\n", t.name.c_str(), t.spells[i]); continue; }
      if (t.spells[i] != magic::traditionSpellId(t.id, (int)i) || d->tradition != t.id) SP_FAIL("%s: spell %s has the wrong ids\n", t.name.c_str(), d->name.c_str());
      r[std::min<int>(d->rank, 3)]++;
      const float b = magic::budgetOf(*d);
      if (b > 0 && std::fabs(d->power / b - 1.0f) > 0.05f) SP_FAIL("%s: %s power %.1f vs budget %.1f\n", t.name.c_str(), d->name.c_str(), d->power, b);
      if (d->name.empty() || d->name.size() > 26) SP_FAIL("%s: a spell name '%s'\n", t.name.c_str(), d->name.c_str());
      if (d->res != t.res) SP_FAIL("%s: %s is not paid in the tradition's resource\n", t.name.c_str(), d->name.c_str());
      if (magic::damaging(*d) && i + 1 < t.spells.size()) {   // (the signature trades DPS for its 60-120 s moment)
        const float rk = d->rank >= 3 ? 1.3f * 1.1f : d->rank == 2 ? 1.15f : 1.0f;
        ratio += magic::sustainedDps(*d) / (rk * magic::essenceMult(d->ess)) / commonDps(*d) * magic::essenceMult(d->ess);
        dn++;
      }
      if (m_table && c.seed == 11u)
        out("spells:   %-26s r%d %-11s %-6s %-7s tier %d power %6.1f budget %6.1f cost %3.0f %s\n", d->name.c_str(), d->rank, magic::schoolName(d->school), magic::formName(d->form),
            magic::essenceName(d->ess), d->tier, d->power, b, d->cost, i + 1 == t.spells.size() ? "(signature)" : "");
    }
    if (r[1] != 3 || r[2] < 3 || r[2] > 5 || r[3] < 2 || r[3] > 3) SP_FAIL("%s ranks %d/%d/%d (want 3, 3-5, 2-3)\n", t.name.c_str(), r[1], r[2], r[3]);
    const SpellDef* last = magic::spellById(t.spells.back());
    if (!last || last->rank != 3 || last->fx.size < 4) SP_FAIL("%s: its signature spell is not last at rank 3 with a spectacle\n", t.name.c_str());
    if (dn) {
      ratio /= (float)dn;
      if (std::fabs(ratio - 1.0f) > 0.12f) SP_FAIL("%s: DPS %.0f%% of Common at equal item level\n", t.name.c_str(), 100.0f * ratio);
    }
    if (m_table && c.seed == 11u) out("spells: tradition %-34s %-15s %-10s %2zu spells, DPS %.0f%% of Common\n", t.name.c_str(), magic::signatureName(t.sig),
                                       c.name.c_str(), t.spells.size(), dn ? 100.0f * ratio : 0.0f);
  }
  if (made < 20) SP_FAIL("only %d traditions generated\n", made);
  for (int s : {4, 5, 6, 7, 8}) if (!sigs[s]) SP_FAIL("no culture has the signature %s\n", magic::signatureName((magic::Signature)s));
  if ((int)names.size() < made * 2 / 3) SP_FAIL("tradition names repeat (%zu names for %d)\n", names.size(), made);
}

// ---------------------------------------------------------------- the arena
void clearFoes(Game& g) {
  for (size_t i = 1; i < g.actors.size();) {
    if (!g.actors[i].comp) g.actors.erase(g.actors.begin() + (std::ptrdiff_t)i); else i++;
  }
  g.projs.clear();
  g.pickups.clear();
  MagicOps::clearMap(g);
}
void openCountry(Game& g) {
  const int32_t sx = g.world.ox + (int32_t)(g.pl().p.x / TILE), sy = g.world.oy + (int32_t)(g.pl().p.y / TILE);
  static const int off[][2] = {{90, 0}, {-90, 0}, {0, 90}, {0, -90}, {70, 70}, {-70, 70}, {70, -70}, {-70, -70}, {130, 0}, {-130, 0}};
  for (const auto& o : off) {
    g.teleportGlobal(sx + o[0], sy + o[1]);
    spRun(g, 0.5f);
    bool bad = g.settlementAt(g.pl().p) >= 0 || MagicOps::nearWater(g, g.pl().p, 6);
    for (int dx = -6; dx <= 6 && !bad; dx++)
      for (int dy = -4; dy <= 4 && !bad; dy++) {
        const int tx = (int)std::floor(g.pl().p.x / TILE) + dx, ty = (int)std::floor(g.pl().p.y / TILE) + dy;
        if (g.map().blocked(tx, ty) || g.map().propAt(tx, ty)) bad = true;
      }
    for (const Actor& a : g.actors) if (a.npc && !a.comp && !a.hostile && len2(a.p - g.pl().p) < (25.0f * TILE) * (25.0f * TILE)) bad = true;
    if (!bad) return;
  }
}
Game makeBase(uint64_t seed) {
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.hour = 12;
  spRun(g, 0.5f);
  openCountry(g);
  clearFoes(g);
  g.godMode = true;
  g.maxMp = g.mp = 1e5f;
  g.pl().aim = Vec2(1, 0); g.pl().face = 2;
  MagicOps::live(g).rainOverride = 0;
  MagicOps::live(g).moonOverride = 0;
  return g;
}
// a dummy that takes the blows: huge health, no armour, no feet
int dummy(Game& g, art::Monster m, Vec2 at, int level = 3) {
  const int id = g.debugSpawnAt(m, at, level);
  const int k = MagicOps::findActor(g, id);
  if (k < 0) return -1;
  Actor& a = g.actors[(size_t)k];
  a.hp = a.maxHp = 1e6f; a.armor = 0; a.speed = 0; a.regen = 0; a.affixes = 0; a.rank = 0; a.aggro = false; a.aggroR = 0;
  return id;
}
Actor* byId(Game& g, int id) {
  const int k = MagicOps::findActor(g, id);
  return k >= 0 ? &g.actors[(size_t)k] : nullptr;
}
void ready(Game& g) {
  Actor& p = g.pl();
  p.shootCd = 0; p.st = AState::Idle; p.aim = Vec2(1, 0); p.face = 2;
  for (float& c : MagicOps::live(g).cd) c = 0;
  MagicOps::live(g).cast = magic::Cast();
  g.mp = g.maxMp = 1e5f;
}
void prep(Game& g, uint32_t id, int slot = 0) {
  MagicOps::learn(g, id, false);
  g.arcana.book.slot[slot] = id;
  for (int k = 0; k < magic::QUICK_SLOTS; k++) if (k != slot && g.arcana.book.slot[k] == id) g.arcana.book.slot[k] = 0;
  g.arcana.book.sel = (uint8_t)slot;
}

// ---------------------------------------------------------------- every spell in play deals its power
// the blows a cast lands on the dummy (Ev::Hit f), the time it ran
std::vector<float> blowsOn(Game& g, int id, float secs, std::vector<Event>* evOut = nullptr) {
  std::vector<float> hits;
  const int n = (int)std::lround(secs / SIM_DT);
  for (int f = 0; f < n; f++) {
    Actor* a = byId(g, id);
    const Vec2 at = a ? a->p : Vec2();
    g.update(SIM_DT, Input());
    if (Actor* b = byId(g, id)) { b->p = at; b->knock = Vec2(); }   // (pinned: knockback and lunges do not move it)
    for (const Event& e : g.events) if (e.type == Ev::Hit && len2(e.p - (at + Vec2(0, -6))) < 20.0f * 20.0f) hits.push_back(e.f);
    if (evOut) evOut->insert(evOut->end(), g.events.begin(), g.events.end());
    g.events.clear();
    if (g.mode == Mode::LevelUp) g.mode = Mode::Play;
  }
  return hits;
}
void checkInPlay(const Game& base, const SpellDef& d, int& checked) {
  if (!magic::damaging(d) || magic::legacySpellOf(d.id) >= 0) return;
  Game g = base;
  if (d.tradition) { g.plLevel = std::max(g.plLevel, 12); g.arcana.book.attuned = {d.tradition}; }
  const bool undead = d.ess == Essence::Light || d.ess == Essence::Arcane;
  const int id = dummy(g, undead && d.id == 19 ? art::Monster::Skeleton : art::Monster::Wolf, g.pl().p + Vec2(40, 0));
  if (id < 0) { SP_FAIL("no dummy for %s\n", d.name.c_str()); return; }
  ready(g);
  prep(g, d.id);
  for (int r = 0; r < (int)magic::Resource::COUNT; r++) g.arcana.book.res[r] = r == (int)magic::Resource::Heat ? 99.0f : 999.0f;
  float want = d.power * MagicOps::powerOf(g, d);   // (now: what the spell lays may change the conditions after)
  std::vector<Event> ev;
  MagicOps::begin(g, 0, false, false);
  const float secs = d.form == Form::Beam ? d.duration + 0.5f : d.form == Form::Meteor ? d.castTime + 1.6f : d.form == Form::Storm ? d.castTime + 1.2f : d.castTime + 1.5f;
  const std::vector<float> hits = blowsOn(g, id, secs, &ev);
  if (!phases(ev, Phase::Launch, d.id)) SP_FAIL("%s: no Launch\n", d.name.c_str());
  if (hits.empty()) { SP_FAIL("%s: no blow landed on the dummy\n", d.name.c_str()); return; }
  const Actor* v = byId(g, id);
  if (v && (skills::tagsOf(*v) & skills::FT_UNDEAD) && d.ess == Essence::Light) want *= 1.5f;
  float got = hits[0];
  if (d.form == Form::Beam) { got = 0; for (float h : hits) got += h; got /= d.duration; }
  if (d.form == Form::Wall || d.form == Form::Totem) got = hits[0] * 2.0f;   // (a half-second tick)
  if (std::fabs(got / want - 1.0f) > 0.05f) SP_FAIL("%s in play: %.1f vs its power %.1f (%+.1f%%)\n", d.name.c_str(), got, want, 100.0f * (got / want - 1.0f));
  if (!phases(ev, Phase::Impact, d.id) && d.form != Form::Beam && d.form != Form::Wall && d.form != Form::Totem) SP_FAIL("%s: no Impact\n", d.name.c_str());
  checked++;
}
void checkAllInPlay(const Game& base) {
  int checked = 0;
  for (const SpellDef& d : magic::schoolSpells()) checkInPlay(base, d, checked);
  int tradChecked = 0;
  for (const cult::Culture& c : sampleCultures()) {
    if (c.seed != 11u) continue;
    const magic::Tradition& t = magic::traditionOf(c);
    for (uint32_t sid : t.spells) if (const SpellDef* d = magic::spellById(sid)) { const int before = checked; checkInPlay(base, *d, checked); tradChecked += checked - before; }
  }
  if (m_table) out("spells: in play: %d spells dealt their power within 5%% (%d of them traditions')\n", checked, tradChecked);
  if (checked < 40) SP_FAIL("only %d spells checked in play\n", checked);
}

// ---------------------------------------------------------------- the classic three, quick slots
void checkClassic(const Game& base) {
  Game g = base;
  ready(g);
  g.spellsKnown = 1; g.spell = Spell::Flames;
  std::vector<Event> ev;
  g.events.clear();
  MagicOps::cast(g);
  ev.insert(ev.end(), g.events.begin(), g.events.end());
  const float pm = gear::playerDamageMul(g.plLevel);
  if (g.projs.empty() || g.projs.back().kind != ProjKind::Spell || g.projs.back().spell != magic::SPELL_FIREBOLT || std::fabs(g.projs.back().dmg - 18 * pm) > 1e-3f ||
      g.projs.back().ench != Ench::Fire)
    SP_FAIL("the classic Firebolt did not fly as before (a spell missile of 18 damage with its fire)\n");
  if (!g.arcana.book.knows(magic::SPELL_FIREBOLT) || g.arcana.book.slot[0] != magic::SPELL_FIREBOLT) SP_FAIL("the classic spell is not in the book\n");
  spRun(g, 1.2f, &ev);
  if (!phases(ev, Phase::Launch, magic::SPELL_FIREBOLT) || !phases(ev, Phase::Impact, magic::SPELL_FIREBOLT)) SP_FAIL("the Firebolt's Launch / Impact were not sent\n");
  // a classic Firebolt still bursts on the others close by (the old Fireball's 40 %)
  {
    Game h = base;
    ready(h);
    h.spellsKnown = 1; h.spell = Spell::Flames;
    const int a = dummy(h, art::Monster::Wolf, h.pl().p + Vec2(48, 0));
    const int b = dummy(h, art::Monster::Wolf, h.pl().p + Vec2(52, 6));
    MagicOps::cast(h);
    spRun(h, 1.0f);
    const Actor* A = byId(h, a);
    const Actor* B = byId(h, b);
    if (!A || !B || A->hp >= A->maxHp || B->hp >= B->maxHp) SP_FAIL("the classic Firebolt's burst did not reach the second foe\n");
  }
  // Frost Lance and Mend keep their numbers
  {
    Game h = base;
    ready(h);
    h.spellsKnown = 4; h.spell = Spell::IceSpike;
    MagicOps::cast(h);
    if (h.projs.empty() || h.projs.back().kind != ProjKind::Spell || std::fabs(h.projs.back().dmg - 24 * pm) > 1e-3f || h.projs.back().ench != Ench::Frost)
      SP_FAIL("the classic Frost Lance did not fly as before\n");
    h.spellsKnown = 2; h.spell = Spell::Heal;
    h.pl().hp = 10; ready(h);
    MagicOps::syncLegacy(h);
    MagicOps::cast(h);
    if (std::fabs(h.pl().hp - std::min(h.pl().maxHp, 10.0f + 45 * pm)) > 0.01f) SP_FAIL("the classic Mend does not heal 45\n");
  }
  // a new school's spell: Spark flies as a spell missile from its quick slot
  if (!MagicOps::learn(g, 4, false)) SP_FAIL("SPARK could not be learned\n");
  Input in;
  in.castSlot = (int8_t)g.arcana.book.slotOf(4);
  ready(g);
  g.projs.clear();
  g.update(SIM_DT, in);
  bool spark = false;
  for (const Projectile& p : g.projs) spark |= p.kind == ProjKind::Spell && p.spell == 4 && p.mag == 1.0f;
  if (!spark) SP_FAIL("SPARK's quick slot did not loose a spell missile\n");
  // a save keeps the book
  std::vector<uint8_t> buf;
  g.serialize(buf);
  Game h(g.seed);
  if (!h.deserialize(buf) || !h.arcana.book.knows(4) || !h.arcana.book.knows(magic::SPELL_FIREBOLT) || !(h.spellsKnown & 1)) SP_FAIL("the book did not survive a save\n");
}

// ---------------------------------------------------------------- upcast, dual-cast, concentration
void checkUpcastDual(const Game& base) {
  Game g = base;
  ready(g);
  prep(g, 4);   // SPARK
  dummy(g, art::Monster::Wolf, g.pl().p + Vec2(150, 0));
  std::vector<Event> ev;
  Input press;
  press.castSlot = 0; press.castHold = true;
  g.update(SIM_DT, press);
  ev.insert(ev.end(), g.events.begin(), g.events.end());
  g.events.clear();
  Input hold;
  hold.castHold = true;
  spRun(g, 1.5f, &ev, &hold);
  if (phases(ev, Phase::WindUp, 4) != 1) SP_FAIL("a hold did not begin with a WindUp\n");
  if (phases(ev, Phase::Charge, 4) != 3) SP_FAIL("1.5 s of holding poured in %d upcast levels (wanted 3)\n", phases(ev, Phase::Charge, 4));
  g.projs.clear();
  spRun(g, 2 * SIM_DT);   // (let go)
  const Projectile* up = nullptr;
  for (const Projectile& p : g.projs) if (p.spell == 4) up = &p;
  const float pm = MagicOps::powerOf(g, *magic::spellById(4));
  if (!up || up->upcast != 3 || std::fabs(up->mag - magic::magnitudeOf(3, false)) > 1e-4f || std::fabs(up->dmg - magic::spellById(4)->power * pm * up->mag) > 0.01f)
    SP_FAIL("the upcast SPARK did not fly at magnitude %.2f\n", magic::magnitudeOf(3, false));
  if (MagicOps::live(g).stats.upcasts != 1) SP_FAIL("the upcast was not counted\n");
  // dual-casting needs the school's DUAL CASTING perk
  Game h = base;
  ready(h);
  prep(h, 4);
  MagicOps::begin(h, 0, false, true);
  if (h.projs.empty() || h.projs.back().dual) SP_FAIL("a dual-cast went off without the perk\n");
  h.skills[skills::Skill::Destruction].perks |= 1u << 1;
  ready(h);
  h.projs.clear();
  MagicOps::begin(h, 0, false, true);
  if (h.projs.empty() || !h.projs.back().dual || std::fabs(h.projs.back().mag - 1.6f) > 1e-4f) SP_FAIL("a dual-cast with the perk is not x1.6\n");
  // a dual-cast costs twice, an upcast more by level
  const SpellDef& sp = *magic::spellById(4);
  if (std::fabs(MagicOps::costOf(h, sp, 0, true) - 2 * MagicOps::costOf(h, sp, 0, false)) > 1e-3f || MagicOps::costOf(h, sp, 2, false) <= MagicOps::costOf(h, sp, 1, false))
    SP_FAIL("upcast / dual-cast costs do not grow\n");
}
void checkConcentration(const Game& base) {
  Game g = base;
  ready(g);
  g.skills[skills::Skill::Destruction].level = 80;
  prep(g, 14);   // INCINERATE
  const int id = dummy(g, art::Monster::Wolf, g.pl().p + Vec2(60, 0));
  std::vector<Event> ev;
  MagicOps::begin(g, 0, false, false);
  spRun(g, 1.0f, &ev);
  if (MagicOps::live(g).conc.spell != 14 || phases(ev, Phase::Channel, 14) < 5) SP_FAIL("INCINERATE did not channel (%d ticks)\n", phases(ev, Phase::Channel, 14));
  const Actor* v = byId(g, id);
  if (!v || v->hp >= v->maxHp) SP_FAIL("the beam did not burn its target\n");
  g.godMode = false;
  g.pl().hp = g.pl().maxHp;
  MagicOps::damageRaw(g, g.pl(), g.pl().maxHp * 0.3f, g.pl().p + Vec2(10, 0), id, Ench::None, 0);
  ev.clear();
  spRun(g, 0.1f, &ev);
  if (MagicOps::live(g).conc.spell || MagicOps::live(g).stats.concBroken != 1) SP_FAIL("a heavy blow did not break concentration\n");
  // a held charm: the foe fights for the hero while it lasts; one concentration at a time
  Game h = base;
  ready(h);
  h.skills[skills::Skill::Illusion].level = 60;
  prep(h, 36);   // CHARM
  const int w = dummy(h, art::Monster::Wolf, h.pl().p + Vec2(50, 0), 2);
  const int foe2 = dummy(h, art::Monster::Wolf, h.pl().p + Vec2(70, 20), 2);
  if (Actor* f2 = byId(h, foe2)) { f2->hp = f2->maxHp = 400; }
  MagicOps::begin(h, 0, false, false);
  spRun(h, 1.0f);
  const Actor* c = byId(h, w);
  if (!c || c->hostile || c->magicTag != magic::MT_CHARM || !c->hasStatus(Status::Charmed) || MagicOps::live(h).conc.spell != 36) SP_FAIL("CHARM did not turn the wolf (concentration held)\n");
  if (Actor* cc = byId(h, w)) cc->speed = 90;
  spRun(h, 3.0f);
  const Actor* f2 = byId(h, foe2);
  if (!f2 || f2->hp >= 400) SP_FAIL("the charmed wolf did not fight for the hero\n");
  MagicOps::releaseConc(h, false);
  spRun(h, 0.1f);
  c = byId(h, w);
  if (!c || !c->hostile || c->hasStatus(Status::Charmed)) SP_FAIL("letting the charm go did not free the wolf\n");
}

// ---------------------------------------------------------------- statuses
void checkStatuses(const Game& base) {
  Game g = base;
  const Vec2 o = g.pl().p;
  auto fresh = [&](Vec2 at) { const int id = dummy(g, art::Monster::Wolf, at); if (Actor* a = byId(g, id)) { a->hp = a->maxHp = 1000; } return id; };
  // burning and poisoned hurt over time
  for (Status s : {Status::Burning, Status::Poisoned}) {
    const int id = fresh(o + Vec2(60, 0));
    MagicOps::applyStatus(g, *byId(g, id), s, 3, g.pl().id);
    spRun(g, 1.0f);
    const Actor* a = byId(g, id);
    if (!a || a->hp >= 1000) SP_FAIL("%s did not hurt\n", magic::statusName(s));
    clearFoes(g);
  }
  // chilled slows; wet and chilled freeze; frozen holds and shatters (x1.5)
  {
    const int id = fresh(o + Vec2(60, 0));
    MagicOps::applyStatus(g, *byId(g, id), Status::Chilled, 3, g.pl().id);
    spRun(g, 0.1f);
    if (byId(g, id)->slowT <= 0) SP_FAIL("chilled did not slow\n");
    clearFoes(g);
    const int f = fresh(o + Vec2(60, 0));
    MagicOps::applyStatus(g, *byId(g, f), Status::Wet, 3, g.pl().id);
    MagicOps::applyStatus(g, *byId(g, f), Status::Chilled, 3, g.pl().id);
    if (!byId(g, f)->hasStatus(Status::Frozen)) SP_FAIL("wet and chilled did not freeze\n");
    if (!MagicOps::actorStep(g, *byId(g, f), SIM_DT)) SP_FAIL("frozen did not hold the foe\n");
    const float h0 = byId(g, f)->hp;
    MagicOps::damageRaw(g, *byId(g, f), 20, g.pl().p, g.pl().id, Ench::None, 0);
    const float shat = h0 - byId(g, f)->hp;
    const float h1 = byId(g, f)->hp;
    MagicOps::damageRaw(g, *byId(g, f), 20, g.pl().p, g.pl().id, Ench::None, 0);
    const float plain = h1 - byId(g, f)->hp;
    if (std::fabs(shat / std::max(0.01f, plain) - 1.5f) > 0.02f) SP_FAIL("a frozen foe did not shatter for x1.5 (%.1f vs %.1f)\n", shat, plain);
    clearFoes(g);
  }
  // wet conducts shock: a storm blow lands x1.5 on the soaked
  {
    Game h = base;
    ready(h);
    prep(h, 4);
    const int id = dummy(h, art::Monster::Wolf, h.pl().p + Vec2(40, 0));
    MagicOps::applyStatus(h, *byId(h, id), Status::Wet, 5, h.pl().id);
    MagicOps::begin(h, 0, false, false);
    const std::vector<float> hits = blowsOn(h, id, 0.8f);
    const float want = magic::spellById(4)->power * MagicOps::powerOf(h, *magic::spellById(4)) * 1.5f;
    if (hits.empty() || std::fabs(hits[0] / want - 1.0f) > 0.05f) SP_FAIL("wet did not conduct the shock (x1.5)\n");
  }
  // shocked twitches (stun ticks), frightened flees, blinded loses the hero at range and misses
  {
    const int id = fresh(o + Vec2(60, 0));
    MagicOps::applyStatus(g, *byId(g, id), Status::Shocked, 2, g.pl().id);
    int stunned = 0;
    for (int f = 0; f < 60; f++) { Actor& a = *byId(g, id); a.statusT[(int)Status::Shocked] = 2.0f - (float)f / 60.0f; stunned += MagicOps::aiOverride(g, a, SIM_DT); }
    if (stunned < 5 || stunned > 40) SP_FAIL("shocked stun ticks %d of 60 steps\n", stunned);
    clearFoes(g);
    const int fr = fresh(o + Vec2(40, 0));
    byId(g, fr)->speed = 60;
    MagicOps::applyStatus(g, *byId(g, fr), Status::Frightened, 3, g.pl().id);
    const float d0 = len(byId(g, fr)->p - g.pl().p);
    spRun(g, 1.0f);
    if (len(byId(g, fr)->p - g.pl().p) < d0 + 20) SP_FAIL("a frightened foe did not flee\n");
    clearFoes(g);
    const int bl = fresh(o + Vec2(80, 0));
    Actor& b = *byId(g, bl);
    b.aggro = true; b.target = g.pl().id;
    MagicOps::applyStatus(g, b, Status::Blinded, 3, g.pl().id);
    MagicOps::aiOverride(g, *byId(g, bl), SIM_DT);
    if (byId(g, bl)->aggro) SP_FAIL("a blinded foe kept the hero in sight\n");
    int miss = 0;
    g.godMode = false;
    for (int k = 0; k < 200; k++) {
      const float before = g.pl().hp;
      g.pl().hp = g.pl().maxHp; g.pl().iframes = 0;
      (void)before;
      const float hp0 = g.pl().hp;
      MagicOps::damageRaw(g, g.pl(), 1, byId(g, bl)->p, bl, Ench::None, 0);
      miss += g.pl().hp >= hp0;
    }
    g.godMode = true;
    if (miss < 60 || miss > 140) SP_FAIL("a blinded foe missed %d of 200 blows (about half)\n", miss);
    clearFoes(g);
  }
  // charmed: a mark of the mind turns a foe (CALM stills one)
  {
    Game h = base;
    ready(h);
    h.skills[skills::Skill::Illusion].level = 30;
    prep(h, 34);   // CALM
    const int id = dummy(h, art::Monster::Wolf, h.pl().p + Vec2(50, 0), 2);
    MagicOps::begin(h, 0, false, false);
    spRun(h, 1.0f);
    const Actor* a = byId(h, id);
    if (!a || a->magicTag != magic::MT_CALM || a->aggro) SP_FAIL("CALM did not still the wolf\n");
  }
  for (int s = 0; s < magic::STATUSES; s++) if (s != (int)Status::Charmed && !g.arcana.live.stats.statusApplied[s] && s != (int)Status::Frozen) SP_FAIL("status %s was never applied\n", magic::statusName((Status)s));
}

// ---------------------------------------------------------------- surfaces and combos
void checkCombos(const Game& base) {
  Game g = base;
  const int tx = (int)std::floor(g.pl().p.x / TILE) + 3, ty = (int)std::floor((g.pl().p.y - 2) / TILE);
  const magic::Field& F = g.arcana.live.field;
  struct C { Surface lay; Essence e; Surface want; };
  const C cs[] = {{Surface::Water, Essence::Fire, Surface::Steam}, {Surface::Water, Essence::Frost, Surface::Ice}, {Surface::Water, Essence::Storm, Surface::Shock},
                  {Surface::Water, Essence::Venom, Surface::Poison}, {Surface::Oil, Essence::Fire, Surface::Fire}, {Surface::Ice, Essence::Fire, Surface::Water},
                  {Surface::Fire, Essence::Frost, Surface::Steam}, {Surface::Poison, Essence::Fire, Surface::Fire}, {Surface::Blood, Essence::Frost, Surface::Ice}};
  int k = 0;
  for (const C& c : cs) {
    const int x = tx + (k % 5) * 2, y = ty + (k / 5) * 3;
    k++;
    if (g.map().blocked(x, y)) continue;
    MagicOps::laySurface(g, x, y, c.lay, 20, 1, g.pl().id);
    const int before = g.arcana.live.stats.combos;
    if (!MagicOps::touchSurface(g, x, y, c.e, g.pl().id) || F.at(x, y) != c.want || g.arcana.live.stats.combos != before + 1)
      SP_FAIL("%s + %s did not make %s (got %s)\n", magic::surfaceName(c.lay), magic::essenceName(c.e), magic::surfaceName(c.want), magic::surfaceName(F.at(x, y)));
  }
  // laying one surface on another combos too (water poured on fire: steam)
  MagicOps::laySurface(g, tx, ty + 6, Surface::Fire, 20, 1, g.pl().id);
  MagicOps::laySurface(g, tx, ty + 6, Surface::Water, 20, 1, g.pl().id);
  if (F.at(tx, ty + 6) != Surface::Steam) SP_FAIL("water on fire did not steam\n");
  // shock runs through a whole puddle; fire runs along oil
  Game h = base;
  const magic::Field& H = h.arcana.live.field;
  for (int i = 0; i < 4; i++) MagicOps::laySurface(h, tx + i, ty, Surface::Water, 20, 1, h.pl().id);
  MagicOps::touchSurface(h, tx, ty, Essence::Storm, h.pl().id);
  int sh = 0;
  for (int i = 0; i < 4; i++) sh += H.at(tx + i, ty) == Surface::Shock;
  if (sh < 4) SP_FAIL("the shock ran through %d of 4 puddle tiles\n", sh);
  for (int i = 0; i < 4; i++) MagicOps::laySurface(h, tx + i, ty + 2, Surface::Oil, 20, 1, h.pl().id);
  MagicOps::touchSurface(h, tx, ty + 2, Essence::Fire, h.pl().id);
  spRun(h, 1.5f);
  int fire = 0;
  for (int i = 0; i < 4; i++) fire += H.at(tx + i, ty + 2) == Surface::Fire;
  if (fire < 4) SP_FAIL("the fire ran along %d of 4 oil tiles\n", fire);
  // what stands in them: electrified water bites, steam blinds, oil slows, burning ground burns
  {
    Game q = base;
    const int id = dummy(q, art::Monster::Wolf, q.pl().p + Vec2(48, 0));
    Actor& a = *byId(q, id);
    a.hp = a.maxHp = 1000;
    const int ax = (int)std::floor(a.p.x / TILE), ay = (int)std::floor((a.p.y - 2) / TILE);
    MagicOps::laySurface(q, ax, ay, Surface::Water, 20, 1, q.pl().id);
    MagicOps::touchSurface(q, ax, ay, Essence::Storm, q.pl().id);
    spRun(q, 1.0f);
    const Actor* b = byId(q, id);
    if (!b || b->hp >= 1000 || !b->hasStatus(Status::Shocked)) SP_FAIL("electrified water did not bite\n");
    q.arcana.live.field.clear();
    MagicOps::laySurface(q, ax, ay, Surface::Steam, 20, 1, q.pl().id);
    spRun(q, 0.2f);
    if (!byId(q, id)->hasStatus(Status::Blinded)) SP_FAIL("steam did not blind\n");
    q.arcana.live.field.clear();
    MagicOps::laySurface(q, ax, ay, Surface::Fire, 20, 1, q.pl().id);
    spRun(q, 0.2f);
    if (!byId(q, id)->hasStatus(Status::Burning)) SP_FAIL("burning ground did not set the foe alight\n");
    // rain douses the fire: the ground goes out four times as fast and the burning body is put out
    MagicOps::live(q).rainOverride = 1;
    spRun(q, 1.5f);
    if (byId(q, id)->hasStatus(Status::Burning) || q.arcana.live.field.at(ax, ay) == Surface::Fire) SP_FAIL("rain did not douse the fire\n");
  }
}

// ---------------------------------------------------------------- reactions, the loadout
void checkReactions(const Game& base) {
  Game g = base;
  ready(g);
  prep(g, 5, 1);   // STONE WARD in slot 2
  const int id = g.debugSpawnAt(art::Monster::Wraith, g.pl().p + Vec2(100, 0), 3);
  Actor* w = byId(g, id);
  if (!w || !w->ranged) { SP_FAIL("no ranged caster to react to\n"); return; }
  w->st = AState::Windup; w->stT = 0; w->target = g.pl().id; w->aggro = true;
  MagicOps::step(g, SIM_DT);
  if (g.arcana.live.stats.reactions != 1 || g.arcana.live.ward <= 0) SP_FAIL("a prepared ward did not answer a telegraphed cast\n");
  // SPELL BREAKER breaks the caster's spell
  Game h = base;
  ready(h);
  h.skills[skills::Skill::Alteration].level = 60;
  prep(h, 47, 0);
  const int id2 = h.debugSpawnAt(art::Monster::Wraith, h.pl().p + Vec2(100, 0), 3);
  Actor* w2 = byId(h, id2);
  w2->st = AState::Windup; w2->stT = 0; w2->target = h.pl().id; w2->aggro = true;
  std::vector<Event> ev;
  MagicOps::step(h, SIM_DT);
  ev.insert(ev.end(), h.events.begin(), h.events.end());
  if (byId(h, id2)->st != AState::Hurt || !phases(ev, Phase::Reaction, 47)) SP_FAIL("SPELL BREAKER did not counter the cast\n");
}
void checkLoadout(const Game& base) {
  Game g = base;
  prep(g, 4, 0);
  MagicOps::learn(g, 7, false);
  std::string why;
  // (learn put FEAR in the first empty slot: emptying an empty slot is a no-op; replacing needs a rest)
  const int empty = g.arcana.book.slotOf(7) == 1 ? 2 : 1;
  if (!MagicOps::prepare(g, empty, 0, &why)) SP_FAIL("an empty slot could not stay empty: %s\n", why.c_str());
  if (MagicOps::prepare(g, 0, 7, &why)) SP_FAIL("the loadout changed in the open, away from rest or camp\n");
  MagicOps::rested(g, 1);
  if (!MagicOps::prepare(g, 0, 7, &why)) SP_FAIL("the loadout could not change after a rest: %s\n", why.c_str());
  g.arcana.live.prepT = 0;
  if (!MagicOps::prepare(g, 3, 4, &why)) SP_FAIL("filling an empty slot was refused: %s\n", why.c_str());
}

// ---------------------------------------------------------------- summons
void checkSummons(const Game& base) {
  Game g = base;
  ready(g);
  prep(g, 6);   // SPIRIT WOLF
  const int foe = dummy(g, art::Monster::Wolf, g.pl().p + Vec2(70, 0));
  byId(g, foe)->hp = byId(g, foe)->maxHp = 800;
  const int pick0 = (int)g.pickups.size(), kills0 = g.kills;
  std::vector<Event> ev;
  MagicOps::begin(g, 0, false, false);
  spRun(g, 4.0f, &ev);
  int sid = -1;
  for (const Actor& a : g.actors) if (a.magicTag == magic::MT_SUMMON && a.st != AState::Dead) sid = a.id;
  if (sid < 0 || !phases(ev, Phase::Summon, 6)) { SP_FAIL("SPIRIT WOLF did not climb out of its rift\n"); return; }
  const Actor* s = byId(g, sid);
  if (s->hostile || s->faction != Faction::Player) SP_FAIL("the summon is not the hero's\n");
  if (byId(g, foe)->hp >= 800) SP_FAIL("the summon did not fight\n");
  MagicOps::damageRaw(g, *byId(g, sid), 1e5f, byId(g, foe)->p, foe, Ench::None, 0);
  spRun(g, 0.1f);
  s = byId(g, sid);
  if (s && s->st != AState::Dead) SP_FAIL("a slain summon did not fade\n");
  if ((int)g.pickups.size() != pick0 || g.kills != kills0) SP_FAIL("a faded summon left loot or counted as a kill\n");
  // it expires
  Game h = base;
  ready(h);
  prep(h, 6);
  MagicOps::begin(h, 0, false, false);
  spRun(h, 1.0f);
  for (Actor& a : h.actors) if (a.magicTag == magic::MT_SUMMON) a.magicT = 0.05f;
  spRun(h, 0.3f);
  for (const Actor& a : h.actors) if (a.magicTag == magic::MT_SUMMON && a.st != AState::Dead) SP_FAIL("a summon outlived its time\n");
  // RAISE DEAD: a marked foe that falls rises as the hero's thrall
  Game r = base;
  ready(r);
  r.skills[skills::Skill::Conjuration].level = 60;
  prep(r, 28);
  const int vic = dummy(r, art::Monster::Wolf, r.pl().p + Vec2(50, 0));
  MagicOps::begin(r, 0, false, false);
  spRun(r, 0.8f);
  if (byId(r, vic)->magicTag != magic::MT_RAISE) SP_FAIL("RAISE DEAD did not mark its target\n");
  MagicOps::killRaw(r, *byId(r, vic), r.pl().id);
  spRun(r, 0.2f);
  int thralls = 0;
  for (const Actor& a : r.actors) thralls += a.magicTag == magic::MT_SUMMON && a.st != AState::Dead;
  if (!thralls) SP_FAIL("the marked dead did not rise\n");
}

// ---------------------------------------------------------------- traditions in play: signatures, mastery, attunement
const cult::Culture* cultureWith(magic::Signature s) {
  static std::vector<cult::Culture> all = sampleCultures();
  for (const cult::Culture& c : all) if (magic::traditionOf(c).sig == s) return &c;
  return nullptr;
}
const SpellDef* tradSpell(const magic::Tradition& t, int rank, bool dmg) {
  for (uint32_t sid : t.spells) if (const SpellDef* d = magic::spellById(sid)) if (d->rank == rank && (!dmg || magic::damaging(*d))) return d;
  return nullptr;
}
void checkSignatures(const Game& base) {
  using G = magic::Signature;
  // Starcalling: the pool fills only at night (clear skies), x1.5 under a full moon
  if (const cult::Culture* c = cultureWith(G::Star)) {
    const magic::Tradition& t = magic::traditionOf(*c);
    Game g = base;
    g.plLevel = 15;
    if (!MagicOps::attune(g, t)) SP_FAIL("could not attune to %s\n", t.name.c_str());
    float& star = g.arcana.book.res[(int)magic::Resource::Starlight];
    star = 0;
    g.hour = 12;
    spRun(g, 3.0f);
    if (star > 0.01f) SP_FAIL("starlight filled by day\n");
    g.hour = 23.5f;
    spRun(g, 3.0f);
    if (star < 1.0f) SP_FAIL("starlight did not fill at night (%.2f)\n", star);
    const SpellDef* d = tradSpell(t, 1, true);
    MagicOps::live(g).moonOverride = 1;
    if (!d || std::fabs(MagicOps::signatureMul(g, *d) - 1.5f) > 1e-4f) SP_FAIL("a full moon is not x1.5\n");
    g.hour = 12;
    if (d && MagicOps::signatureMul(g, *d) != 1.0f) SP_FAIL("the full moon works by day\n");
    if (d && d->res != magic::Resource::Starlight) SP_FAIL("Starcalling is not paid in starlight\n");
  } else SP_FAIL("no Starcalling culture\n");
  // Tidebinding: x1.6 in rain (or near water)
  if (const cult::Culture* c = cultureWith(G::Tide)) {
    const magic::Tradition& t = magic::traditionOf(*c);
    Game g = base;
    const SpellDef* d = tradSpell(t, 1, true);
    if (!d || MagicOps::signatureMul(g, *d) != 1.0f) SP_FAIL("Tidebinding is strong on dry land\n");
    MagicOps::live(g).rainOverride = 1;
    if (!d || std::fabs(MagicOps::signatureMul(g, *d) - 1.6f) > 1e-4f) SP_FAIL("Tidebinding is not x1.6 in rain\n");
  } else SP_FAIL("no Tidebinding culture\n");
  // Ancestor calls: charge from kills; x1.4 near graves
  if (const cult::Culture* c = cultureWith(G::Ancestor)) {
    const magic::Tradition& t = magic::traditionOf(*c);
    Game g = base;
    g.plLevel = 15;
    MagicOps::attune(g, t);
    spRun(g, 0.1f);
    const int id = dummy(g, art::Monster::Wolf, g.pl().p + Vec2(60, 0));
    MagicOps::killRaw(g, *byId(g, id), g.pl().id);
    spRun(g, 0.1f);
    if (g.arcana.book.res[(int)magic::Resource::Charge] < 1.0f) SP_FAIL("a kill gave no ancestral charge\n");
    const SpellDef* d = tradSpell(t, 1, false);
    const int px = (int)std::floor(g.pl().p.x / TILE) + 2, py = (int)std::floor(g.pl().p.y / TILE);
    Map& m = g.world.over;
    if (m.in(px, py) && !m.prop.empty()) {
      m.prop[(size_t)py * m.w + px] = (decltype(m.prop)::value_type)((int)art::Prop::GraveCairn + 1);
      if (!d || std::fabs(MagicOps::signatureMul(g, *d) - 1.4f) > 1e-4f) SP_FAIL("Ancestor calls are not x1.4 by a grave\n");
    }
    // its rank-1 summon calls an echo, paid in charge
    for (uint32_t sid : t.spells) {
      const SpellDef* s = magic::spellById(sid);
      if (!s || s->form != Form::Summon || s->rank != 1) continue;
      ready(g);
      g.arcana.book.res[(int)magic::Resource::Charge] = 3;
      prep(g, sid);
      MagicOps::begin(g, 0, false, false);
      spRun(g, 1.0f);
      int echoes = 0;
      for (const Actor& a : g.actors) echoes += a.magicTag == magic::MT_SUMMON && a.st != AState::Dead;
      if (!echoes || g.arcana.book.res[(int)magic::Resource::Charge] > 2.01f) SP_FAIL("the ancestors' echo did not answer for its charge\n");
      break;
    }
  } else SP_FAIL("no Ancestor calls culture\n");
  // Emberforge: blows build heat, spells spend it and imbue the weapon, overheating burns
  if (const cult::Culture* c = cultureWith(G::Ember)) {
    const magic::Tradition& t = magic::traditionOf(*c);
    Game g = base;
    g.plLevel = 15;
    MagicOps::attune(g, t);
    const int id = dummy(g, art::Monster::Wolf, g.pl().p + Vec2(20, 0));
    float& heat = g.arcana.book.res[(int)magic::Resource::Heat];
    heat = 0;
    for (int k = 0; k < 5; k++) MagicOps::damageRaw(g, *byId(g, id), 2, g.pl().p, g.pl().id, Ench::None, 0);
    if (heat < 30) SP_FAIL("blows built %.0f heat (wanted 35)\n", heat);
    const SpellDef* d = tradSpell(t, 1, true);
    ready(g);
    heat = 60;
    if (d) {
      prep(g, d->id);
      MagicOps::begin(g, 0, false, false);
      spRun(g, 0.1f);
      if (g.arcana.live.imbueHits != 5 || heat >= 60) SP_FAIL("an Emberforge spell did not spend heat and imbue the weapon\n");
    }
    heat = 100;
    g.godMode = false;
    spRun(g, 0.1f);
    if (!g.pl().hasStatus(Status::Burning)) SP_FAIL("overheating did not burn the hero\n");
  } else SP_FAIL("no Emberforge culture\n");
  // Beastbinding: a non-boss beast at or below the hero's level is bound 60 s as an ally
  if (const cult::Culture* c = cultureWith(G::Beast)) {
    const magic::Tradition& t = magic::traditionOf(*c);
    Game g = base;
    g.plLevel = 15;
    MagicOps::attune(g, t);
    const SpellDef* bind = nullptr;
    for (uint32_t sid : t.spells) if (const SpellDef* s = magic::spellById(sid)) if (s->form == Form::Mark && s->ess == Essence::Verdant) bind = s;
    if (!bind) { SP_FAIL("Beastbinding has no BIND BEAST\n"); return; }
    ready(g);
    prep(g, bind->id);
    const int id = dummy(g, art::Monster::Wolf, g.pl().p + Vec2(50, 0), 5);
    MagicOps::begin(g, 0, false, false);
    spRun(g, 1.0f);
    const Actor* a = byId(g, id);
    if (!a || a->magicTag != magic::MT_BOUND || a->hostile) SP_FAIL("BIND BEAST did not bind the wolf\n");
    else if (a->magicT < 55) SP_FAIL("the beast is bound for %.0f s (60)\n", a->magicT);
    Game h = base;
    h.plLevel = 15;
    MagicOps::attune(h, t);
    ready(h);
    prep(h, bind->id);
    const int big = dummy(h, art::Monster::Wolf, h.pl().p + Vec2(50, 0), 30);
    MagicOps::begin(h, 0, false, false);
    spRun(h, 1.0f);
    if (byId(h, big)->magicTag == magic::MT_BOUND) SP_FAIL("a beast above the hero's level was bound\n");
  } else SP_FAIL("no Beastbinding culture\n");
}
void checkMastery(const Game& base) {
  const cult::Culture* c = cultureWith(magic::Signature::Star);
  if (!c) return;
  const magic::Tradition& t = magic::traditionOf(*c);
  const cult::Culture* c2 = cultureWith(magic::Signature::Tide);
  Game g = base;
  g.plLevel = 5;
  if (MagicOps::attune(g, t)) SP_FAIL("a level-5 hero attuned to a foreign tradition (level 12)\n");
  g.plLevel = 12;
  if (!MagicOps::attune(g, t)) SP_FAIL("could not attune at level 12\n");
  if (c2) {   // one slot at 12: a second tradition swaps the first out; at 15 there are two
    MagicOps::attune(g, magic::traditionOf(*c2));
    if (g.arcana.book.attuned.size() != 1 || g.arcana.book.attuned[0] != magic::traditionOf(*c2).id) SP_FAIL("a full attunement did not swap\n");
    g.plLevel = 15;
    MagicOps::attune(g, t);
    if (g.arcana.book.attuned.size() != 2) SP_FAIL("level 15 does not hold two attunements\n");
  }
  const SpellDef* r1 = tradSpell(t, 1, false);
  const SpellDef* r2 = tradSpell(t, 2, false);
  const SpellDef* r3 = magic::spellById(t.spells.back());
  if (!r1 || !r2 || !r3) return;
  if (!MagicOps::mayLearn(g, *r1)) SP_FAIL("an attuned hero may not learn a rank-1 spell\n");
  if (MagicOps::mayLearn(g, *r2)) SP_FAIL("a rank-2 spell is learnable at mastery 0\n");
  // mastery by use: a hit is 1 XP
  g.hour = 23;
  MagicOps::live(g).rainOverride = 0;
  ready(g);
  for (float& r : g.arcana.book.res) r = 999;
  const SpellDef* bolt = tradSpell(t, 1, true);
  if (bolt) {
    prep(g, bolt->id);
    dummy(g, art::Monster::Wolf, g.pl().p + Vec2(40, 0));
    MagicOps::begin(g, 0, false, false);
    spRun(g, 1.0f);
    if (g.arcana.book.mastery[t.id] < 1) SP_FAIL("a tradition spell's hit gave no mastery\n");
  }
  MagicOps::gainMastery(g, t.id, magic::MASTERY_RANK2);
  if (!MagicOps::mayLearn(g, *r2)) SP_FAIL("a rank-2 spell is not learnable at mastery 300\n");
  g.arcana.book.mastery[t.id] = magic::MASTERY_RANK3;
  if (MagicOps::mayLearn(g, *r3)) SP_FAIL("the signature is learnable before the trial\n");
  // the Trial of the Threshold: kills soon after the tradition's spell struck, under its affinity (night)
  int ai = -1;
  for (size_t i = 0; i < g.arcana.book.attuned.size(); i++) if (g.arcana.book.attuned[i] == t.id) ai = (int)i;
  for (int k = 0; k < magic::TRIAL_KILLS + 1; k++) {
    const int id = dummy(g, art::Monster::Wolf, g.pl().p + Vec2(60, 10));
    g.arcana.live.tradHitT[t.id] = g.time;
    MagicOps::killRaw(g, *byId(g, id), g.pl().id);
    spRun(g, 0.05f);
  }
  if (ai < 0 || !(g.arcana.book.trials & (1u << ai))) SP_FAIL("the trial was not passed after %d kills under the night\n", magic::TRIAL_KILLS);
  if (!MagicOps::mayLearn(g, *r3)) SP_FAIL("the signature is not learnable after the trial\n");
  // the book remembers the land: a load makes the tradition again
  std::vector<uint8_t> buf;
  MagicOps::learn(g, r1->id, false);
  g.serialize(buf);
  Game h(g.seed);
  if (!h.deserialize(buf) || !h.arcana.book.knows(r1->id) || h.arcana.book.attuned.size() != g.arcana.book.attuned.size()) SP_FAIL("the attuned book did not survive a save\n");
}

// ---------------------------------------------------------------- tomes, teachers, shelves, loot
int optIdx(const Game& g, const std::string& start) {
  for (size_t i = 0; i < g.dlg.opts.size(); i++) if (g.dlg.opts[i].label.rfind(start, 0) == 0) return (int)i;
  return -1;
}
void checkLearning(const Game& base) {
  // a tome: the skill gate keeps it (not used up), the skill lets it teach
  {
    Game g = base;
    g.inv.push_back(magic::makeTome(11));   // ICE BURST (adept)
    const int i = (int)g.inv.size() - 1;
    if (!magic::isTome(g.inv[(size_t)i])) SP_FAIL("makeTome is not a tome\n");
    if (MagicOps::readTome(g, i) || g.arcana.book.knows(11)) SP_FAIL("an adept tome taught a novice\n");
    g.skills[skills::Skill::Destruction].level = 50;
    if (!MagicOps::readTome(g, i) || !g.arcana.book.knows(11)) SP_FAIL("the tome did not teach at Destruction 50\n");
  }
  // a mage teaches: MEND and FROST LANCE first (the old lessons, their old prices), more as the schools grow
  Game g = base;
  g.gold = 1000;
  const int id = MagicOps::spawnTeacher(g, g.pl().p + Vec2(20, 0), Role::Mage);
  Actor* m = byId(g, id);
  if (!m) { SP_FAIL("no teacher\n"); return; }
  MagicOps::talkTo(g, *m);
  const int om = optIdx(g, "TEACH ME MEND (120 GOLD)"), ofl = optIdx(g, "TEACH ME FROST LANCE (300 GOLD)");
  if (om < 0 || ofl < 0) {
    std::string all;
    for (const DlgOpt& o : g.dlg.opts) all += " [" + o.label + "]";
    SP_FAIL("a mage does not teach MEND / FROST LANCE:%s\n", all.c_str());
  } else {
    const DlgOpt o = g.dlg.opts[(size_t)om];
    MagicOps::choose(g, o);
    if (!g.arcana.book.knows(magic::SPELL_MEND) || g.gold != 880 || !(g.spellsKnown & 2)) SP_FAIL("learning MEND from a mage failed\n");
  }
  for (const DlgOpt& o : g.dlg.opts) if (o.action >= DLG_MAGIC && o.action < DLG_MAGIC_END && o.label.find("METEOR") != std::string::npos) SP_FAIL("a novice is offered METEOR\n");
  // a far land's mage: attunement, then its rank-1 spells
  const cult::Culture* c = cultureWith(magic::Signature::Star);
  if (c) {
    const magic::Tradition& t = magic::traditionOf(*c);
    MagicOps::setLand(c);
    g.plLevel = 12;
    g.mode = Mode::Play;
    MagicOps::talkTo(g, *byId(g, id));
    const int oa = optIdx(g, "ATTUNE ME TO");
    if (oa < 0) SP_FAIL("a mage of %s offers no attunement\n", t.name.c_str());
    else { const DlgOpt o = g.dlg.opts[(size_t)oa]; MagicOps::choose(g, o); }
    if (!MagicOps::attuned(g, t.id)) SP_FAIL("the mage did not attune the hero\n");
    g.mode = Mode::Play;
    MagicOps::talkTo(g, *byId(g, id));
    int taught = 0;
    for (const DlgOpt& o : std::vector<DlgOpt>(g.dlg.opts)) {
      if (o.action != DLG_MAGIC + 2) continue;
      const SpellDef* d = magic::spellById((uint32_t)o.arg);
      if (!d || d->rank != 1) SP_FAIL("a mage offers a rank-%d spell at mastery 0\n", d ? d->rank : 0);
      MagicOps::choose(g, o);
      taught += d && g.arcana.book.knows(d->id);
    }
    if (!taught) SP_FAIL("no tradition spell was taught by the mage\n");
    // the shelf: three school tomes and one of the land's
    std::vector<Item> stock;
    MagicOps::shopStock(g, *byId(g, id), stock);
    int tomes = 0, trad = 0;
    for (const Item& it : stock) if (magic::isTome(it)) { tomes++; if (const SpellDef* d = magic::spellById(it.seed)) trad += d->tradition == t.id; }
    if (tomes != 4 || trad != 1) SP_FAIL("a mage's shelf holds %d tomes, %d of the land's (wanted 4, 1)\n", tomes, trad);
    MagicOps::setLand(nullptr);
  }
  // a priest teaches Restoration only
  {
    Game h = base;
    const int pid = MagicOps::spawnTeacher(h, h.pl().p + Vec2(20, 0), Role::Priest, 9);
    MagicOps::talkTo(h, *byId(h, pid));
    int n = 0;
    for (const DlgOpt& o : h.dlg.opts) {
      if (o.action != DLG_MAGIC) continue;
      n++;
      const SpellDef* d = magic::spellById((uint32_t)o.arg);
      if (!d || d->school != School::Restoration) SP_FAIL("a priest teaches %s\n", d ? d->name.c_str() : "?");
    }
    if (!n) SP_FAIL("a priest teaches nothing\n");
  }
  // loot: tomes are rare on bodies and never above adept there; the master tomes only from finds and world bosses
  {
    Game h = base;
    int body = 0, bodyHigh = 0, finds = 0, master = 0;
    for (int k = 0; k < 4000; k++) {
      h.pickups.clear();
      MagicOps::loot(h, Vec2((float)(k * 37 % 3000), (float)(k * 53 % 2000)), 40, gear::LootSrc::Body);
      for (const Pickup& p : h.pickups) if (magic::isTome(p.item)) { body++; if (const SpellDef* d = magic::spellById(p.item.seed)) bodyHigh += d->tier > 2; }
      h.pickups.clear();
      MagicOps::loot(h, Vec2((float)(k * 31 % 3000), (float)(k * 59 % 2000)), 40, gear::LootSrc::WorldBoss);
      for (const Pickup& p : h.pickups) if (magic::isTome(p.item)) { finds++; if (const SpellDef* d = magic::spellById(p.item.seed)) master += d->tier >= 4; }
    }
    if (body < 4 || body > 60) SP_FAIL("bodies dropped %d tomes in 4000 (about 0.5%%)\n", body);
    if (bodyHigh) SP_FAIL("bodies dropped %d tomes above adept\n", bodyHigh);
    if (finds < 1200 || !master) SP_FAIL("world bosses dropped %d tomes, %d master\n", finds, master);
    if (m_table) out("spells: loot: %d tomes from 4000 bodies, %d from 4000 world bosses (%d master)\n", body, finds, master);
  }
}

// ---------------------------------------------------------------- (M10) the hidden kingdoms' magic
// a people beyond the Mist whose tradition has that signature (its land never the song land unless asked)
cult::Culture hiddenCulture(magic::Signature want, bool songLand = false) {
  for (uint32_t s = 1; s < 600; s++) {
    cult::Culture c = cult::Atlas::make((cult::Archetype)(s % (uint32_t)cult::Archetype::COUNT), 7000u + s * 13u);
    if (!c.id) c.id = cult::familyId((int)(s % 97u) * 7 + 3, (int32_t)(s % 1000u));
    c.hidden = true;
    c.hiddenLand = songLand ? magic::songLand() : (0x20000001u + s * 2u);
    if (!songLand && c.hiddenLand == magic::songLand()) continue;
    std::vector<SpellDef> sp;
    if (magic::generateTradition(c, sp).sig == want) return c;
  }
  return cult::Culture();
}
const SpellDef* sigSpell(const magic::Tradition& t, Form f, int rank = 0) {
  for (uint32_t sid : t.spells) if (const SpellDef* d = magic::spellById(sid)) if (d->form == f && (!rank || d->rank == rank)) return d;
  return nullptr;
}
// the hero attuned to a hidden tradition (the steps set as met)
void hiddenHero(Game& g, const magic::Tradition& t) {
  g.plLevel = std::max(g.plLevel, magic::HIDDEN_LEVEL);
  g.arcana.book.passRite(t.id, 0);
  if (!MagicOps::attuned(g, t.id)) g.arcana.book.attuned.push_back(t.id);
  ready(g);
}
void checkHiddenTables() {
  using G = magic::Signature;
  const uint32_t keepSong = magic::songLand();
  int made = 0;
  std::set<int> sigs;
  for (int k = 0; k < 6; k++) {
    static const G six[6] = {G::Blood, G::Rune, G::Song, G::Mask, G::Verdant, G::Dream};
    const cult::Culture c = hiddenCulture(six[k]);
    if (!c.id) { SP_FAIL("no hidden culture with %s\n", magic::signatureName(six[k])); continue; }
    std::vector<SpellDef> s1, s2;
    const magic::Tradition t1 = magic::generateTradition(c, s1), t2 = magic::generateTradition(c, s2);
    if (t1.id != t2.id || t1.spells != t2.spells || s1.size() != s2.size() || t1.motif[0] != t2.motif[0] || t1.motif[2] != t2.motif[2]) SP_FAIL("hidden %s: two generations differ\n", t1.name.c_str());
    for (size_t i = 0; i < s1.size() && i < s2.size(); i++) if (!sameSpell(s1[i], s2[i]) || s1[i].note != s2[i].note || s1[i].mask != s2[i].mask) SP_FAIL("hidden %s: spell %zu differs\n", t1.name.c_str(), i);
    const magic::Tradition& t = magic::traditionOf(c);
    if (t.id != t1.id) SP_FAIL("traditionOf(hidden %s) is not its generation\n", c.name.c_str());
    if (!t.hidden || t.minLevel != magic::HIDDEN_LEVEL || !magic::hiddenSignature(t.sig)) SP_FAIL("%s: hidden %d, min level %d, signature %s\n", t.name.c_str(), (int)t.hidden, t.minLevel, magic::signatureName(t.sig));
    if (t.focus.empty()) SP_FAIL("%s has no focus\n", t.name.c_str());
    if (t.spells.size() < 8 || t.spells.size() > 12) SP_FAIL("%s has %zu spells\n", t.name.c_str(), t.spells.size());
    int r[4] = {};
    for (uint32_t sid : t.spells) {
      const SpellDef* d = magic::spellById(sid);
      if (!d) { SP_FAIL("%s: spell %u unknown\n", t.name.c_str(), sid); continue; }
      r[std::min<int>(d->rank, 3)]++;
      const float b = magic::budgetOf(*d);
      if (b > 0 && std::fabs(d->power / b - 1.0f) > 0.05f) SP_FAIL("%s: %s power %.1f vs budget %.1f\n", t.name.c_str(), d->name.c_str(), d->power, b);
      if (b > 0 && d->power > magic::basePower(d->form) * magic::essenceMult(d->ess) * 3.2f * 1.3f * 1.1f * 1.1f) SP_FAIL("%s: %s past +10%% of the raw budget\n", t.name.c_str(), d->name.c_str());
      if (d->name.empty() || d->name.size() > 26) SP_FAIL("%s: a spell name '%s'\n", t.name.c_str(), d->name.c_str());
      const bool resOk = d->res == t.res || (t.sig == G::Song && d->res == magic::Resource::Magicka);
      if (!resOk) SP_FAIL("%s: %s not paid in the tradition's resource\n", t.name.c_str(), d->name.c_str());
      if (m_table && k < 6 && sid == t.spells.front())
        out("spells: hidden %-32s %-15s %2zu spells, focus %s\n", t.name.c_str(), magic::signatureName(t.sig), t.spells.size(), t.focus.c_str());
    }
    if (r[1] != 3 || r[2] < 3 || r[2] > 5 || r[3] < 2 || r[3] > 3) SP_FAIL("%s ranks %d/%d/%d\n", t.name.c_str(), r[1], r[2], r[3]);
    const SpellDef* last = magic::spellById(t.spells.back());
    if (!last || last->rank != 3 || last->fx.size < 4) SP_FAIL("%s: its signature spell is not last at rank 3\n", t.name.c_str());
    // each mechanic's own spell is there
    if (t.sig == G::Mask) { int masks = 0; for (uint32_t sid : t.spells) if (const SpellDef* d = magic::spellById(sid)) masks += d->mask >= 0; if (masks != 3) SP_FAIL("%s has %d masks (3)\n", t.name.c_str(), masks); }
    if (t.sig == G::Verdant && (!sigSpell(t, Form::Blink) || !sigSpell(t, Form::Wall, 1) || !sigSpell(t, Form::Nova, 1))) SP_FAIL("%s lacks the bridge, the thorns or the snare\n", t.name.c_str());
    if (t.sig == G::Dream && (!sigSpell(t, Form::Summon, 1) || !sigSpell(t, Form::Totem, 2))) SP_FAIL("%s lacks the decoy or the bubble\n", t.name.c_str());
    if (t.sig == G::Rune && !sigSpell(t, Form::Rune, 1)) SP_FAIL("%s lacks a rank-1 rune\n", t.name.c_str());
    if (t.sig == G::Song) {
      int noted = 0;
      for (uint32_t sid : t.spells) if (const SpellDef* d = magic::spellById(sid)) noted += d->note >= 0;
      if (noted != (int)t.spells.size() || t.motif[0] == t.motif[1] || t.motif[1] == t.motif[2] || t.motif[0] == t.motif[2]) SP_FAIL("%s: notes %d, motif %d %d %d\n", t.name.c_str(), noted, t.motif[0], t.motif[1], t.motif[2]);
    }
    made++;
    sigs.insert((int)t.sig);
  }
  if (made != 6 || sigs.size() != 6) SP_FAIL("hidden traditions: %d made, %zu signatures\n", made, sigs.size());
  // the nearest hidden land always sings, with fire among its essences
  magic::setSongLand(0x30000001u);
  for (int k = 0; k < 6; k++) {
    cult::Culture c = cult::Atlas::make((cult::Archetype)(k * 2 % (int)cult::Archetype::COUNT), 900u + (uint32_t)k);
    if (!c.id) c.id = cult::familyId(k * 11 + 5, k);
    c.hidden = true; c.hiddenLand = 0x30000001u;
    std::vector<SpellDef> sp;
    const magic::Tradition t = magic::generateTradition(c, sp);
    bool fire = false;
    for (Essence e : t.ess) fire |= e == Essence::Fire;
    if (t.sig != G::Song || !fire) SP_FAIL("the song land's people (%s) have %s, fire %d\n", c.name.c_str(), magic::signatureName(t.sig), (int)fire);
  }
  magic::setSongLand(keepSong);
  // the book's rites: version 3 only when one is passed
  magic::Book b;
  b.learn(4);
  std::vector<uint8_t> v2, v3;
  b.serialize(v2);
  b.passRite(4242, 0); b.passRite(4242, 1); b.passRite(77, 0);
  b.serialize(v3);
  magic::Book c;
  if (v2.empty() || v2[0] != 2 || v3[0] != 3) SP_FAIL("the magic block versions %d / %d (want 2 / 3)\n", v2.empty() ? -1 : v2[0], v3[0]);
  if (!c.deserialize(v3) || c.rite(4242) != 3 || c.rite(77) != 1 || c.rite(5) != 0) SP_FAIL("the rites did not round-trip\n");
  std::vector<uint8_t> v3b;
  c.serialize(v3b);
  if (v3b != v3) SP_FAIL("the version-3 block is not byte-stable\n");
}

void checkHiddenPlay(const Game& base) {
  using G = magic::Signature;
  // ---- Blood rites: paid in health, x(1 + 0.8 (1 - hp / max)), healing halved
  {
    const cult::Culture c = hiddenCulture(G::Blood);
    const magic::Tradition& t = magic::traditionOf(c);
    Game g = base;
    g.godMode = false;
    hiddenHero(g, t);
    const SpellDef* d = tradSpell(t, 1, true);
    if (!d) SP_FAIL("blood: no rank-1 spell\n");
    else {
      Actor& p = g.pl();
      p.maxHp = 200; p.hp = 200;
      if (std::fabs(MagicOps::signatureMul(g, *d) - 1.0f) > 1e-4f) SP_FAIL("blood: x%.3f at full health\n", MagicOps::signatureMul(g, *d));
      p.hp = 100;
      if (std::fabs(MagicOps::signatureMul(g, *d) - 1.4f) > 1e-3f) SP_FAIL("blood: x%.3f at half health (1.4)\n", MagicOps::signatureMul(g, *d));
      p.hp = 20;
      if (std::fabs(MagicOps::signatureMul(g, *d) - 1.72f) > 1e-3f) SP_FAIL("blood: x%.3f at 10%% health (1.72)\n", MagicOps::signatureMul(g, *d));
      p.hp = 200;
      prep(g, d->id);
      const float mp0 = g.mp, hp0 = p.hp, cost = MagicOps::costOf(g, *d, 0, false);
      MagicOps::begin(g, 0, false, false);
      spRun(g, 0.05f);
      if (std::fabs((hp0 - g.pl().hp) - cost) > 0.5f || g.mp != mp0) SP_FAIL("blood: the cast cost %.1f health (%.1f) and %.0f magicka\n", hp0 - g.pl().hp, cost, mp0 - g.mp);
      // healing halved: MEND heals half of what it heals an unbound hero
      Game h = base, k = base;
      hiddenHero(h, t);
      k.plLevel = h.plLevel;
      for (Game* x : {&h, &k}) {
        ready(*x);
        prep(*x, magic::SPELL_MEND);
        x->godMode = false;
        x->pl().maxHp = 500; x->pl().hp = 10;
        MagicOps::begin(*x, 0, false, false);
        spRun(*x, 0.05f);
      }
      const float hb = h.pl().hp - 10, hk = k.pl().hp - 10;
      if (hk <= 0 || std::fabs(hb / hk - magic::BLOOD_HEAL) > 0.03f) SP_FAIL("blood: MEND healed %.1f bound vs %.1f (x0.5)\n", hb, hk);
      if (m_table) out("spells: blood: x1.00 / x1.40 / x1.72 at 100/50/10%% health; a cast paid %.0f health; MEND %.1f vs %.1f\n", cost, hb, hk);
    }
  }
  // ---- Runecarving: two runes within 3 tiles go off together (fire + frost: a steam burst); the charge refills
  {
    const cult::Culture c = hiddenCulture(G::Rune);
    const magic::Tradition& t = magic::traditionOf(c);
    const SpellDef* fire = nullptr; const SpellDef* frost = nullptr;
    for (uint32_t sid : t.spells) if (const SpellDef* d = magic::spellById(sid)) if (d->form == Form::Rune) { if (d->ess == Essence::Fire && !fire) fire = d; if (d->ess == Essence::Frost && !frost) frost = d; }
    if (!fire || !frost) SP_FAIL("rune: no fire / frost rune\n");
    else {
      float single = 0, pair = 0;
      int combos = 0, steam = 0, left = 0;
      for (int n = 1; n <= 2; n++) {
        Game g = base;
        hiddenHero(g, t);
        const Vec2 at = g.pl().p + Vec2(48, 0);
        for (int i = 0; i < n; i++) {
          magic::Zone z;
          z.spell = i == 0 ? fire->id : frost->id; z.kind = magic::ZoneKind::Rune; z.owner = g.pl().id; z.mag = 1;
          z.p = z.q = at + Vec2(32.0f * (float)i, 0); z.r = std::max(24.0f, fire->radius); z.life = 30; z.power = 40;
          MagicOps::live(g).zones.push_back(z);
        }
        const int id = dummy(g, art::Monster::Wolf, at);
        const float hp0 = byId(g, id)->hp;
        spRun(g, 0.1f);
        const float dmg = hp0 - (byId(g, id) ? byId(g, id)->hp : 0.0f);
        if (n == 1) single = dmg;
        else {
          pair = dmg;
          combos = MagicOps::live(g).stats.runeCombos;
          for (const magic::SurfaceTile& s : g.arcana.live.field.tiles) steam += s.kind == Surface::Steam;
          for (const magic::Zone& z : g.arcana.live.zones) left += z.kind == magic::ZoneKind::Rune;
        }
      }
      if (combos != 1 || left != 0 || !steam) SP_FAIL("rune: %d combos, %d runes left, %d steam tiles (1, 0, >0)\n", combos, left, steam);
      if (single <= 0 || pair < single * (magic::RUNE_COMBO_MULT - 0.02f)) SP_FAIL("rune: the linked pair dealt %.1f vs one rune %.1f (x1.5)\n", pair, single);
      Game g = base;
      hiddenHero(g, t);
      g.arcana.book.res[(int)magic::Resource::Charge] = 0;
      spRun(g, 2.0f * magic::RUNE_CHARGE_SECS + 0.2f);
      const float ch = g.arcana.book.res[(int)magic::Resource::Charge];
      if (std::fabs(ch - 2.0f) > 0.01f) SP_FAIL("rune: the charge refilled to %.1f in 7.2 s (2)\n", ch);
      if (m_table) out("spells: rune: one rune %.0f, a linked pair %.0f (x%.2f), steam %d tiles, charge %.0f in 7.2 s\n", single, pair, single > 0 ? pair / single : 0.0f, steam, ch);
    }
  }
  // ---- Songweaving: notes, and the motif's Refrain (free, x2)
  {
    const cult::Culture c = hiddenCulture(G::Song);
    const magic::Tradition& t = magic::traditionOf(c);
    const SpellDef* by[7] = {};
    for (uint32_t sid : t.spells) if (const SpellDef* d = magic::spellById(sid)) if (d->rank == 1 && d->note >= 0 && !by[d->note]) by[d->note] = d;
    if (!by[t.motif[0]] || !by[t.motif[1]] || !by[t.motif[2]]) SP_FAIL("song: the motif's spells are not all rank 1\n");
    else {
      for (int order = 0; order < 2; order++) {
        Game g = base;
        hiddenHero(g, t);
        const int id = dummy(g, art::Monster::Wolf, g.pl().p + Vec2(40, 0));
        std::vector<Event> ev;
        float mp0 = 0;
        float lastBlow = 0, refrainBlow = 0;
        for (int i = 0; i < 3; i++) {
          const SpellDef* d = by[t.motif[order == 0 ? i : 2 - i]];
          ready(g);
          g.mp = g.maxMp = 1e5f;
          if (i == 2) mp0 = g.mp;
          prep(g, d->id);
          MagicOps::begin(g, 0, false, false);
          const std::vector<float> hits = blowsOn(g, id, d->castTime + 1.4f, &ev);
          if (i == 2 && !hits.empty()) { lastBlow = hits.front(); refrainBlow = hits.size() > 1 ? hits.back() : 0; }
        }
        int notes = 0, refr = 0;
        for (const Event& e : ev) if (e.type == Ev::Spell && e.a == (int)Phase::Reaction) { notes += e.s == "NOTE"; refr += e.s == "REFRAIN"; }
        const magic::Stats& S = g.arcana.live.stats;
        const SpellDef* third = by[t.motif[order == 0 ? 2 : 0]];
        if (order == 0) {
          if (S.refrains != 1 || refr != 1 || notes != 3) SP_FAIL("song: in motif order %d refrains, %d notes (1, 3)\n", S.refrains, notes);
          if (mp0 - g.mp > 1.5f * MagicOps::costOf(g, *third, 0, false)) SP_FAIL("song: the Refrain was not free (%.0f paid for %.0f)\n", mp0 - g.mp, MagicOps::costOf(g, *third, 0, false));
          if (magic::damaging(*third) && (lastBlow <= 0 || std::fabs(refrainBlow / lastBlow - magic::REFRAIN_MULT) > 0.06f))
            SP_FAIL("song: the Refrain struck %.1f after %.1f (x2)\n", refrainBlow, lastBlow);
          if (g.arcana.book.res[(int)magic::Resource::Notes] != 0) SP_FAIL("song: the notes did not reset after the Refrain\n");
          if (m_table) out("spells: song: motif %d-%d-%d sung: %d notes, %d refrain, blow %.1f then refrain %.1f\n", t.motif[0], t.motif[1], t.motif[2], notes, S.refrains, lastBlow, refrainBlow);
        } else if (S.refrains != 0 && !(t.motif[0] == t.motif[2])) SP_FAIL("song: a reversed motif sang a Refrain\n");
      }
    }
  }
  // ---- Maskwork: a mask worn, blows build charge and carry its essence, the finisher is a spell-strike
  {
    const cult::Culture c = hiddenCulture(G::Mask);
    const magic::Tradition& t = magic::traditionOf(c);
    const SpellDef* fox = nullptr;
    for (uint32_t sid : t.spells) if (const SpellDef* d = magic::spellById(sid)) if (d->mask == 0) fox = d;
    if (!fox) SP_FAIL("mask: no ember fox\n");
    else {
      Game g = base;
      hiddenHero(g, t);
      prep(g, fox->id);
      MagicOps::begin(g, 0, false, false);
      spRun(g, fox->castTime + 0.2f);
      const magic::Live& L = g.arcana.live;
      if (L.mask != 0 || L.maskT < 55) SP_FAIL("mask: worn %d for %.0f s (0, 60)\n", L.mask, L.maskT);
      const int id = dummy(g, art::Monster::Wolf, g.pl().p + Vec2(18, 0));
      g.arcana.book.res[(int)magic::Resource::Charge] = 0;
      MagicOps::damageRaw(g, *byId(g, id), 3, g.pl().p, g.pl().id, Ench::None, 0);
      if (g.arcana.book.res[(int)magic::Resource::Charge] < 1 || !byId(g, id)->hasStatus(Status::Burning)) SP_FAIL("mask: a masked blow built no charge or carried no fire\n");
      const float hp0 = byId(g, id)->hp;
      Actor& p = g.pl();
      p.aim = Vec2(1, 0); p.combo = 2; p.comboT = 0.5f; p.st = AState::Strike; p.stT = 0; p.hitDone = true;
      spRun(g, 0.25f);
      if (L.stats.maskStrikes != 1 || byId(g, id)->hp >= hp0) SP_FAIL("mask: the finisher's spell-strike: %d, %.0f -> %.0f\n", L.stats.maskStrikes, hp0, byId(g, id)->hp);
      if (m_table) out("spells: mask: %s worn %.0f s, a blow +1 charge and fire, the finisher struck %.0f\n", fox->name.c_str(), L.maskT, hp0 - byId(g, id)->hp);
    }
  }
  // ---- Verdant shaping: thorn walls (solid, 30 s, restored exactly), a vine bridge (never strands), root snares
  {
    const cult::Culture c = hiddenCulture(G::Verdant);
    const magic::Tradition& t = magic::traditionOf(c);
    const SpellDef* wall = sigSpell(t, Form::Wall, 1);
    const SpellDef* bridge = sigSpell(t, Form::Blink);
    const SpellDef* snareS = sigSpell(t, Form::Nova, 1);
    if (wall) {
      Game g = base;
      hiddenHero(g, t);
      const Map before = g.world.over;
      prep(g, wall->id);
      MagicOps::begin(g, 0, false, false);
      spRun(g, wall->castTime + 0.2f);
      const int grown = (int)g.arcana.live.growths.size();
      int solid = 0;
      for (const magic::Growth& w : g.arcana.live.growths) solid += g.map().blocked(w.tx, w.ty);
      spRun(g, magic::VERDANT_SECS + 0.5f);
      int diff = 0;
      for (size_t i = 0; i < before.solid.size(); i++) diff += before.solid[i] != g.world.over.solid[i] || before.ground[i] != g.world.over.ground[i];
      if (grown < 3 || solid != grown || !g.arcana.live.growths.empty() || diff) SP_FAIL("verdant: thorns %d grown, %d solid, %zu left, %d tiles differ after\n", grown, solid, g.arcana.live.growths.size(), diff);
      if (m_table) out("spells: verdant: %d thorn tiles stood solid 30 s and went back exactly (%d tiles differ)\n", grown, diff);
    } else SP_FAIL("verdant: no thorn wall\n");
    if (bridge) {
      for (int width = 3; width <= 4; width++) {
        Game g = base;
        hiddenHero(g, t);
        Map& m = g.world.over;
        const int hx = (int)std::floor(g.pl().p.x / TILE), hy = (int)std::floor((g.pl().p.y - 2) / TILE);
        for (int i = 1; i <= width; i++) m.setG(hx + 1 + i, hy, Ground::Water);
        const Map before = m;
        g.pl().aim = Vec2(1, 0); g.pl().face = 2;
        prep(g, bridge->id);
        MagicOps::begin(g, 0, false, false);
        spRun(g, bridge->castTime + 0.2f);
        int bridged = 0;
        for (int i = 1; i <= width; i++) bridged += m.at(hx + 1 + i, hy) == Ground::Bridge;
        if (width == 4) { if (bridged) SP_FAIL("verdant: a vine bridge grew over 4 tiles of water\n"); continue; }
        if (bridged != 3) { SP_FAIL("verdant: the vine bridge covers %d of 3 water tiles\n", bridged); continue; }
        // the hero stands on the middle of it when it ends: it waits, then sets the hero ashore and turns back exactly
        g.pl().p = Vec2((float)(hx + 3) * TILE + 8, (float)hy * TILE + 9);
        spRun(g, magic::VERDANT_SECS - bridge->castTime - 0.2f + 1.0f);
        const bool waited = m.at(hx + 3, hy) == Ground::Bridge;
        spRun(g, 6.5f);
        int diff = 0;
        for (size_t i = 0; i < before.ground.size(); i++) diff += before.ground[i] != m.ground[i] || before.solid[i] != m.solid[i];
        const Ground under = m.at((int)std::floor(g.pl().p.x / TILE), (int)std::floor((g.pl().p.y - 2) / TILE));
        if (!waited || diff || groundWater(under)) SP_FAIL("verdant: the bridge waited %d, %d tiles differ, the hero %s\n", (int)waited, diff, groundWater(under) ? "stranded in water" : "ashore");
        if (m_table) out("spells: verdant: a vine bridge over 3 tiles (4: refused), waited for the hero, set ashore, %d tiles differ\n", diff);
      }
    } else SP_FAIL("verdant: no vine bridge\n");
    if (snareS) {
      Game g = base;
      hiddenHero(g, t);
      const int id = dummy(g, art::Monster::Wolf, g.pl().p + Vec2(30, 0));
      prep(g, snareS->id);
      MagicOps::begin(g, 0, false, false);
      spRun(g, snareS->castTime + 0.1f);
      const Actor* a = byId(g, id);
      if (!a || a->magicTag != magic::MT_PARALYZE || g.arcana.live.snared.empty()) SP_FAIL("verdant: the root snare did not hold the wolf\n");
    }
  }
  // ---- Dreamweft: decoys pull foes' attacks; a bubble slows foes to 40 %
  {
    const cult::Culture c = hiddenCulture(G::Dream);
    const magic::Tradition& t = magic::traditionOf(c);
    const SpellDef* dec = sigSpell(t, Form::Summon, 1);
    const SpellDef* bub = sigSpell(t, Form::Totem, 2);
    if (dec) {
      Game g = base;
      hiddenHero(g, t);
      const int fid = g.debugSpawnAt(art::Monster::Wolf, g.pl().p + Vec2(90, 0), 3);
      if (Actor* f = byId(g, fid)) { f->aggro = true; f->target = g.pl().id; f->hp = f->maxHp = 1e5f; }
      g.pl().aim = Vec2(1, 0);
      prep(g, dec->id);
      MagicOps::begin(g, 0, false, false);
      spRun(g, 3.0f);
      int decoys = 0, did = -1;
      for (const Actor& a : g.actors) if (a.magicTag == magic::MT_DECOY && a.st != AState::Dead) { decoys++; did = a.id; }
      const Actor* f = byId(g, fid);
      if (!decoys || !f || f->target != did || g.arcana.live.stats.decoyHits < 1) SP_FAIL("dream: decoys %d, the wolf targets %d (decoy %d), %d blows on it\n", decoys, f ? f->target : -9, did, g.arcana.live.stats.decoyHits);
      if (m_table) out("spells: dream: a decoy drew %d blows from the wolf in 3 s\n", g.arcana.live.stats.decoyHits);
    } else SP_FAIL("dream: no decoy\n");
    if (bub) {
      float moved[2] = {};
      for (int in = 0; in < 2; in++) {
        Game g = base;
        hiddenHero(g, t);
        const Vec2 start = g.pl().p + Vec2(140, 0);
        const int fid = g.debugSpawnAt(art::Monster::Wolf, start, 3);
        Actor* f = byId(g, fid);
        if (!f) continue;
        f->aggro = true; f->target = g.pl().id; f->hp = f->maxHp = 1e5f;
        if (in) { magic::Bubble b; b.p = start + Vec2(-40, 0); b.r = 120; b.life = 5; b.spell = bub->id; g.arcana.live.bubbles.push_back(b); }
        spRun(g, 0.8f);
        f = byId(g, fid);
        moved[in] = f ? len(f->p - start) : 0;
      }
      const float ratio = moved[0] > 1 ? moved[1] / moved[0] : 0;
      if (std::fabs(ratio - magic::DREAM_SLOW) > 0.12f) SP_FAIL("dream: a wolf in the bubble moved %.0f vs %.0f px (x%.2f, want 0.4)\n", moved[1], moved[0], ratio);
      if (m_table) out("spells: dream: the bubble slowed a wolf to x%.2f (%.0f vs %.0f px)\n", ratio, moved[1], moved[0]);
    } else SP_FAIL("dream: no bubble\n");
  }
}

// the five steps (6.4): each refuses with a reason until met; the focus only there; the teacher grants rank 1
void checkHiddenLearning(const Game& base) {
  cult::Culture c = hiddenCulture(magic::Signature::Song);
  const magic::Tradition& t = magic::traditionOf(c);
  Game g = base;
  g.gold = 50000;
  MagicOps::setLand(&c);
  const int id = MagicOps::spawnTeacher(g, g.pl().p + Vec2(20, 0), Role::Mage);
  std::string why;
  int step = MagicOps::hiddenStep(g, t, byId(g, id), 1, &why);
  if (step != 1) SP_FAIL("learning: unlanded step %d (1)\n", step);
  g.sea.landMut(c.hiddenLand).flags |= sea::LF_LANDED;
  MagicOps::live(g).repOverride = 10;
  step = MagicOps::hiddenStep(g, t, byId(g, id), 1, &why);
  if (step != 2 || why.find("FRIENDLY") == std::string::npos) SP_FAIL("learning: unfriendly step %d '%s'\n", step, why.c_str());
  // the talk refuses with the same reason
  MagicOps::talkTo(g, *byId(g, id));
  const int oh = optIdx(g, "TEACH ME THE WAYS OF");
  if (oh < 0) SP_FAIL("learning: the hidden mage offers no ways\n");
  else { const DlgOpt o = g.dlg.opts[(size_t)oh]; MagicOps::choose(g, o); if (g.dlg.text.find("TRUST") == std::string::npos) SP_FAIL("learning: the refusal says '%s'\n", g.dlg.text.c_str()); }
  g.mode = Mode::Play;
  MagicOps::live(g).repOverride = 30;
  g.plLevel = 20;
  if ((step = MagicOps::hiddenStep(g, t, byId(g, id), 1)) != 3) SP_FAIL("learning: level-20 step %d (3)\n", step);
  g.plLevel = 25;
  if ((step = MagicOps::hiddenStep(g, t, byId(g, id), 1)) != 4) SP_FAIL("learning: untried step %d (4)\n", step);
  MagicOps::passRite(g, t.id, 0);
  if ((step = MagicOps::hiddenStep(g, t, byId(g, id), 1)) != 5) SP_FAIL("learning: unattuned step %d (5)\n", step);
  MagicOps::talkTo(g, *byId(g, id));
  const int oh2 = optIdx(g, "TEACH ME THE WAYS OF");
  if (oh2 >= 0) { const DlgOpt o = g.dlg.opts[(size_t)oh2]; MagicOps::choose(g, o); }
  if (!MagicOps::attuned(g, t.id)) SP_FAIL("learning: the teacher did not grant rank 1\n");
  int taught = 0;
  for (const DlgOpt& o : std::vector<DlgOpt>(g.dlg.opts)) if (o.action == DLG_MAGIC + 2) { MagicOps::choose(g, o); taught++; }
  if (!taught) SP_FAIL("learning: no rank-1 spell offered after the grant\n");
  g.mode = Mode::Play;
  const SpellDef* r3 = magic::spellById(t.spells.back());
  g.arcana.book.mastery[t.id] = magic::MASTERY_RANK3;
  if (r3 && MagicOps::mayLearn(g, *r3, &why)) SP_FAIL("learning: the signature without Honoured\n");
  if ((step = MagicOps::hiddenStep(g, t, byId(g, id), 3)) != 6) SP_FAIL("learning: rank 3 at Friendly: step %d (6)\n", step);
  MagicOps::live(g).repOverride = 60;
  if ((step = MagicOps::hiddenStep(g, t, byId(g, id), 3)) != 7) SP_FAIL("learning: rank 3 without the second trial: step %d (7)\n", step);
  MagicOps::passRite(g, t.id, 1);
  if (r3 && !MagicOps::mayLearn(g, *r3, &why)) SP_FAIL("learning: the signature refused after every step: %s\n", why.c_str());
  // the focus: sold by the hidden land's mage, never by a mainland one
  std::vector<Item> stock;
  MagicOps::shopStock(g, *byId(g, id), stock);
  int focus = 0;
  for (const Item& it : stock) focus += it.kind == ItemKind::Staff && it.name == t.focus;
  if (focus != 1) SP_FAIL("learning: the hidden mage sells %d foci (1)\n", focus);
  if (const cult::Culture* main = cultureWith(magic::Signature::Star)) {
    MagicOps::setLand(main);
    std::vector<Item> s2;
    MagicOps::shopStock(g, *byId(g, id), s2);
    for (const Item& it : s2) if (it.kind == ItemKind::Staff) SP_FAIL("learning: a mainland mage sells a hidden focus\n");
  }
  // companions of that land cast its rank-1 bolt
  MagicOps::setLand(nullptr);
  if (m_table) out("spells: learning: the five steps refused in order (1 landed, 2 Friendly, 3 level 25, 4 trial, 5 teacher) and rank 3 (6 Honoured, 7 second trial)\n");
  // the priests of the song land sing fire at dusk, never at noon
  Game h = base;
  MagicOps::setLand(&c);
  MagicOps::spawnTeacher(h, h.pl().p + Vec2(30, 10), Role::Priest, 9);
  MagicOps::spawnTeacher(h, h.pl().p + Vec2(-40, 0), Role::Priest, 10);
  h.hour = 12;
  spRun(h, 4.0f);
  const int noon = h.arcana.live.stats.songFires;
  h.hour = 19;
  spRun(h, 8.0f);
  const int dusk = h.arcana.live.stats.songFires;
  if (noon != 0 || dusk < 3) SP_FAIL("priests: song-fire %d at noon, %d at dusk (0, >= 3)\n", noon, dusk);
  if (m_table) out("spells: priests: %d verses of song-fire in 8 s at dusk, %d at noon\n", dusk, noon);
  MagicOps::setLand(nullptr);
}

// a Trial of the Threshold for every signature: it generates the same twice, its way through is whole, and its puzzle
// passes the trial (the Song trial walked plate by plate)
void checkTrials(const Game& base) {
  using G = magic::Signature;
  Game g = base;
  int ruin = -1;
  for (int i = 0; i < (int)g.world.sites.size(); i++) if (g.world.sites[(size_t)i].type == SiteType::Ruin) { ruin = i; break; }
  if (ruin < 0) { SP_FAIL("trials: no ruin in the window\n"); return; }
  static const G six[6] = {G::Blood, G::Rune, G::Song, G::Mask, G::Verdant, G::Dream};
  for (G sig : six) {
    for (int which = 0; which < 2; which++) {
      const cult::Culture c = hiddenCulture(sig);
      Site s = g.world.sites[(size_t)ruin];
      s.id = 0x7E1A1000ull + (uint64_t)sig * 4 + (uint64_t)which;
      const magic::Tradition& t = magic::traditionOf(c);
      TrialSpec sp;
      sp.sig = (uint8_t)sig; sp.which = (uint8_t)which; sp.tradition = t.id;
      for (int i = 0; i < 3; i++) sp.motif[i] = t.motif[i];
      registerTrial(s.id, sp);
      Map m1, m2;
      genRuin(m1, s, s.seed);
      genRuin(m2, s, s.seed);
      const TrialLayout* T = trialLayoutOf(s.id);
      if (!T || T->plates.empty() || m1.ground != m2.ground || m1.prop != m2.prop) { SP_FAIL("trial %s: no layout or not deterministic\n", magic::signatureName(sig)); continue; }
      // the way through: from the exit to the sanctum plate over floor, gates and chasms counted open
      std::vector<int> dist((size_t)m1.w * m1.h, -1);
      std::vector<int> q{m1.exitY * m1.w + m1.exitX};
      dist[(size_t)q[0]] = 0;
      auto passable = [&](int x, int y) {
        if (!m1.in(x, y)) return false;
        if (groundWater(m1.at(x, y))) for (const IRect& ch : T->chasms) if (ch.contains(x, y)) return true;
        for (const TrialGate& gt : T->gates) if (x >= gt.x && x < gt.x + gt.w && y >= gt.y && y < gt.y + gt.h) return true;
        return !m1.blocked(x, y);
      };
      for (size_t qi = 0; qi < q.size(); qi++) {
        const int x = q[qi] % m1.w, y = q[qi] / m1.w;
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
          const int nx = x + dx[k], ny = y + dy[k];
          if (!passable(nx, ny) || dist[(size_t)ny * m1.w + nx] >= 0) continue;
          dist[(size_t)ny * m1.w + nx] = dist[(size_t)q[qi]] + 1;
          q.push_back(ny * m1.w + nx);
        }
      }
      int unreachable = 0;
      for (const TrialPlate& p : T->plates) unreachable += dist[(size_t)p.y * m1.w + p.x] < 0;
      bool sealed = true;   // closed, a gate holds the way: the sanctum is out of reach without the puzzle
      for (const TrialGate& gt : T->gates) for (int x = gt.x; x < gt.x + gt.w; x++) sealed &= m1.blocked(x, gt.y);
      if (unreachable || (!T->gates.empty() && !sealed)) SP_FAIL("trial %s %d: %d plates out of reach, gates sealed %d\n", magic::signatureName(sig), which, unreachable, (int)sealed);
    }
  }
  // the Song trial walked: three notes in the motif's order open each gate, the sanctum passes the rite
  {
    const cult::Culture c = hiddenCulture(G::Song);
    const magic::Tradition& t = magic::traditionOf(c);
    Game h = base;
    h.noWildSpawns = true;
    if (!MagicOps::markTrial(h, ruin, c, 0)) { SP_FAIL("trials: markTrial refused the ruin\n"); return; }
    MagicOps::enterSiteRaw(h, ruin);
    spRun(h, 0.1f);
    const TrialLayout* T = trialLayoutOf(h.world.sites[(size_t)ruin].id);
    if (!T || !h.inside) { SP_FAIL("trials: the Song trial did not load\n"); return; }
    for (size_t i = 1; i < h.actors.size(); i++) if (h.actors[i].hostile) h.actors[i].hp = 0, h.actors[i].st = AState::Dead;
    auto stand = [&](int x, int y) { h.pl().p = Vec2((float)x * TILE + 8, (float)y * TILE + 9); spRun(h, 0.1f); h.pl().p = Vec2((float)x * TILE + 8, (float)(y + 1) * TILE + 9); spRun(h, 0.1f); };
    const bool closed0 = h.map().blocked(T->gates[0].x, T->gates[0].y);
    for (int room = 1; room <= 2; room++)
      for (int k = 0; k < 3; k++)
        for (const TrialPlate& p : T->plates)
          if (p.kind == TP_NOTE && p.room == room && p.arg == T->spec.motif[k]) { stand(p.x, p.y); break; }
    const bool open = !h.map().blocked(T->gates[0].x, T->gates[0].y) && !h.map().blocked(T->gates[1].x, T->gates[1].y);
    stand(T->sanctumX, T->sanctumY);
    const bool passed = (h.arcana.book.rite(t.id) & 1u) != 0;
    if (!closed0 || !open || !passed || h.arcana.live.stats.gates != 2) SP_FAIL("trials: Song walked: closed %d, opened %d (%d gates), passed %d\n", (int)closed0, (int)open, h.arcana.live.stats.gates, (int)passed);
    // a wrong note does not open it
    Game w = base;
    MagicOps::markTrial(w, ruin, c, 0);
    MagicOps::enterSiteRaw(w, ruin);
    spRun(w, 0.1f);
    for (const TrialPlate& p : T->plates) if (p.kind == TP_NOTE && p.room == 1 && p.arg != T->spec.motif[0]) { w.pl().p = Vec2((float)p.x * TILE + 8, (float)p.y * TILE + 9); spRun(w, 0.1f); }
    if (w.arcana.live.stats.gates) SP_FAIL("trials: a wrong order opened the gate\n");
    // the book keeps the rite through a save
    std::vector<uint8_t> buf;
    h.serialize(buf);
    Game r(h.seed);
    if (!r.deserialize(buf) || !(r.arcana.book.rite(t.id) & 1u)) SP_FAIL("trials: the rite did not survive a save\n");
    if (m_table) out("spells: trials: 6 signatures x 2 trials deterministic and whole; the Song trial walked: 2 gates sung open, the rite passed and saved\n");
  }
  // the Rune trial: the guardians, coming at the hero over the runes, set them off; the hall cleared opens its gate
  {
    const cult::Culture c = hiddenCulture(G::Rune);
    Game h = base;
    h.godMode = true;
    MagicOps::markTrial(h, ruin, c, 0);
    MagicOps::enterSiteRaw(h, ruin);
    spRun(h, 0.1f);
    const TrialLayout* T = trialLayoutOf(h.world.sites[(size_t)ruin].id);
    if (!T || !h.inside) { SP_FAIL("trials: the Rune trial did not load\n"); return; }
    int runes = 0, guards = 0;
    for (const magic::Zone& z : h.arcana.live.zones) runes += z.kind == magic::ZoneKind::Rune;
    for (const Actor& a : h.actors) guards += a.hostile && a.st != AState::Dead && a.p.y < (float)(T->hall[0].y + T->hall[0].h) * TILE && a.p.y > (float)T->hall[0].y * TILE;
    h.pl().p = Vec2(17.0f * TILE + 8, (float)(T->hall[0].y + 8) * TILE + 9);
    for (Actor& a : h.actors) if (a.hostile) { a.aggro = true; a.target = h.pl().id; }
    spRun(h, 8.0f);
    const magic::Stats& S = h.arcana.live.stats;
    if (!runes || !guards || S.runeTriggers < 1) SP_FAIL("trials: Rune: %d runes, %d guardians, %d set off by them\n", runes, guards, S.runeTriggers);
    // what the runes left of the first hall's guardians the hero finishes: the hall cleared opens its gate
    const IRect& h1 = T->hall[0];
    int alive = 0;
    for (Actor& a : h.actors)
      if (a.hostile && a.st != AState::Dead && h1.contains((int)std::floor(a.p.x / TILE), (int)std::floor(a.p.y / TILE))) {
        alive++;
        for (int k = 0; k < 3 && a.st != AState::Dead; k++) MagicOps::killRaw(h, a, h.pl().id);   // (a collapsed skeleton: struck again)
      }
    spRun(h, 0.2f);
    if (S.runeTriggers < 4 || !S.gates) SP_FAIL("trials: Rune: %d runes set off, %d gates open after the hall was cleared\n", S.runeTriggers, S.gates);
    if (m_table) out("spells: trials: the Rune hall: %d runes, %d guardians (%d standing), %d runes set off by them (%d combos), gates %d\n", runes, guards, alive, S.runeTriggers, S.runeCombos, S.gates);
  }
  clearTrials();
}

int spellsCmd(int argc, char** argv) {
  uint64_t seed = 3;
  m_table = true;
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 10);
    if (!std::strcmp(argv[i], "--quiet")) m_table = false;
  }
  m_bad = 0;
  checkTables();
  checkBook();
  checkTraditions();
  const Game base = makeBase(seed);
  checkClassic(base);
  checkAllInPlay(base);
  checkUpcastDual(base);
  checkConcentration(base);
  checkStatuses(base);
  checkCombos(base);
  checkReactions(base);
  checkLoadout(base);
  checkSummons(base);
  checkSignatures(base);
  checkMastery(base);
  checkLearning(base);
  MagicOps::setLand(nullptr);
  // (M10) the hidden kingdoms' magic
  checkHiddenTables();
  {
    int checked = 0;
    using G = magic::Signature;
    for (G sig : {G::Blood, G::Rune, G::Song, G::Mask, G::Verdant, G::Dream}) {
      const cult::Culture c = hiddenCulture(sig);
      for (uint32_t sid : magic::traditionOf(c).spells) if (const SpellDef* d = magic::spellById(sid)) checkInPlay(base, *d, checked);
    }
    if (m_table) out("spells: in play: %d hidden traditions' spells dealt their power within 5%%\n", checked);
    if (checked < 25) SP_FAIL("only %d hidden spells checked in play\n", checked);
  }
  checkHiddenPlay(base);
  checkHiddenLearning(base);
  checkTrials(base);
  MagicOps::setLand(nullptr);
  out("spells: %s (%d failures)\n", m_bad ? "FAIL" : "OK", m_bad);
  return m_bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--spells", "M9: magic: every spell on its budget in the table and in play, upcast / dual / concentration / reactions, statuses, combos, traditions (signatures, mastery, attunement, determinism), tomes and teachers [--seed N] [--quiet]", spellsCmd);
