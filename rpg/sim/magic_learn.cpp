// M9 "Arcana & Mastery": learning magic (VISION_PLAN 6.4, 15.5 "slow roll", 15.29.4). MAGIC lane (MagicOps, magic_ops.h).
//   - teachers: a mage (Role::Mage) teaches every school and, in a land with its own tradition, attunes the hero to it
//     and teaches its spells by mastery rank; a census Scholar teaches Illusion, Alteration and Conjuration; a priest
//     Restoration. Each teaches the spells the hero's school skill allows (25 per tier), a few at a time;
//   - tomes (ItemKind::Book, BOOK_TOME): on a mage's shelf (novice to adept), a scholar's or a priest's (novice and
//     apprentice); rare as loot, the expert and master ones only from a dungeon's hoard, its boss, a find in the wild
//     (ruins, wonders, distant lands) or a world boss's hoard; lost traditions' tomes only from those finds;
//   - attunement (6.3): 1 tradition slot, a 2nd at level 15, a 3rd at 35; a full set swaps the oldest out for an hour
//     of rites; mastery (6.4): rank 2 at 300 XP, rank 3 at 1200 XP and the Trial of the Threshold (kills under the
//     tradition's affinity, counted in magic_game.cpp; no dungeon is generated for it, so world generation is unchanged);
//   - the loadout rule (15.5): the prepared spells change at rest or at camp.
// Determinism: shelves and loot read hashes of the place and the day (never Game::rng_, so the M8b loot rolls keep
// their sequence).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include "rpg/culture/culture.h"
#include "engine/audio.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/sim/realm.h"
#include "rpg/sim/sea.h"
#include "rpg/world/source.h"

using art::Prop;
using magic::SpellDef;
using magic::School;

namespace {
enum : int { MK_LEARN = 0, MK_ATTUNE = 1, MK_TRAD = 2, MK_HIDDEN = 3 };   // MK_HIDDEN (M10): a hidden tradition's ways asked for

int siteOfActor(const Game& g, const Actor& a) {
  if (a.site >= 0 && a.site < (int)g.world.sites.size()) return a.site;
  if (a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size()) {
    const int s = g.world.over.bldgs[(size_t)a.bldg].site;
    if (s >= 0 && s < (int)g.world.sites.size()) return s;
  }
  return -1;
}
life::Job jobOf(const Game& g, const Actor& a) {
  const int si = siteOfActor(g, a);
  if (si < 0 || !g.world.sites[(size_t)si].settlement() || a.resident < 0) return life::Job::None;
  const life::Census* c = g.life.find(g.world.sites[(size_t)si].id);
  if (!c || a.resident >= (int)c->res.size()) return life::Job::None;
  return c->res[(size_t)a.resident].job;
}
// the schools a person teaches (bits by School; 0: not a teacher)
unsigned teaches(const Game& g, const Actor& a) {
  if (!a.npc || !a.human || a.hostile) return 0;
  if (a.role == Role::Mage) return 0x1Fu;
  const life::Job j = jobOf(g, a);
  if (j == life::Job::Scholar) return (1u << (int)School::Illusion) | (1u << (int)School::Alteration) | (1u << (int)School::Conjuration);
  if (a.role == Role::Priest || j == life::Job::Priest) return 1u << (int)School::Restoration;
  return 0;
}
uint32_t nameHash(const std::string& s) {
  uint32_t h = 2166136261u;
  for (char c : s) { h ^= (uint8_t)c; h *= 16777619u; }
  return h;
}
void addLand(Game& g, uint64_t culture) {
  if (!culture) return;
  std::vector<uint64_t>& L = g.arcana.book.lands;
  if (std::find(L.begin(), L.end(), culture) == L.end()) L.push_back(culture);
}
}  // namespace

namespace {
std::unique_ptr<cult::Culture> g_land;   // (tests / scripts) a forced land: MagicOps::setLand
}  // namespace
void MagicOps::setLand(const cult::Culture* c) {
  if (c) g_land = std::make_unique<cult::Culture>(*c);
  else g_land.reset();
}
const cult::Culture* MagicOps::cultureHere(const Game& g, const Actor* who) {
  if (g_land) return g_land.get();
  if (who) {
    const int si = siteOfActor(g, *who);
    if (const cult::Culture* c = titleCulture(g.world, si)) return c;
  }
  if (g.inside) {
    const int si = g.subSite >= 0 ? g.subSite : g.curSite;
    if (const cult::Culture* c = titleCulture(g.world, si)) return c;
    return nullptr;
  }
  return g.world.cultureAtTile((int)std::floor(g.pl().p.x / TILE), (int)std::floor(g.pl().p.y / TILE));
}

bool MagicOps::attuned(const Game& g, uint32_t tid) {
  if (!tid) return true;
  const std::vector<uint32_t>& A = g.arcana.book.attuned;
  return std::find(A.begin(), A.end(), tid) != A.end();
}
int MagicOps::masteryRankOf(const Game& g, uint32_t tid) {
  const magic::Book& B = g.arcana.book;
  const auto it = B.mastery.find(tid);
  const uint16_t xp = it == B.mastery.end() ? 0 : it->second;
  int ai = -1;
  for (size_t i = 0; i < B.attuned.size(); i++) if (B.attuned[i] == tid) ai = (int)i;
  if (const magic::Tradition* t = magic::traditionById(tid)) if (t->hidden) return magic::masteryRank(xp, (B.rite(tid) & 2u) != 0);   // (M10)
  return magic::masteryRank(xp, ai >= 0 && (B.trials & (1u << ai)));
}
void MagicOps::gainMastery(Game& g, uint32_t tid, int xp) {
  if (!tid || xp <= 0 || !attuned(g, tid)) return;
  uint16_t& m = g.arcana.book.mastery[tid];
  const int was = m;
  m = (uint16_t)std::min(65535, was + xp);
  const magic::Tradition* t = magic::traditionById(tid);
  const std::string nm = t ? t->name : std::string("THE TRADITION");
  if (was < magic::MASTERY_RANK2 && m >= magic::MASTERY_RANK2) MagicOps::ev(g, Ev::Notice, g.pl().p, (int)rgba(200, 220, 255), 0, nm + ": MASTERY RANK 2");
  if (was < magic::MASTERY_RANK3 && m >= magic::MASTERY_RANK3)
    MagicOps::ev(g, Ev::Notice, g.pl().p, (int)rgba(255, 220, 140), 0, nm + ": SEEK THE TRIAL OF THE THRESHOLD");
}
bool MagicOps::affinity(const Game& g, const magic::Tradition& t) {
  switch (t.sig) {
    case magic::Signature::Star: return g.isNight();
    case magic::Signature::Tide: return nearWater(g, g.pl().p, 4) || raining(g);
    case magic::Signature::Ancestor: return nearGraves(g, g.pl().p);
    case magic::Signature::Ember: return g.arcana.book.res[(int)magic::Resource::Heat] >= 50.0f;
    case magic::Signature::Beast:
      for (const Actor& a : g.actors) if (a.magicTag == magic::MT_BOUND && a.st != AState::Dead) return true;
      return false;
    default: return true;
  }
}

bool MagicOps::attune(Game& g, const magic::Tradition& t, std::string* why) {
  magic::Book& B = g.arcana.book;
  if (!t.id) { if (why) *why = "COMMON ARCANA NEEDS NO ATTUNEMENT"; return false; }
  if (attuned(g, t.id)) { if (why) *why = "YOU ARE ATTUNED TO IT ALREADY"; return false; }
  if (g.plLevel < (int)t.minLevel) { if (why) *why = "COME BACK AT LEVEL " + std::to_string((int)t.minLevel); return false; }
  const int slots = magic::attuneSlots(g.plLevel);
  if ((int)B.attuned.size() >= slots) {
    // a full set: the oldest goes, for an hour of rites (6.3: swapping costs a 1-hour rest at the tradition's shrine)
    B.attuned.erase(B.attuned.begin());
    for (size_t i = 0; i + 1 < std::size(B.trialKills); i++) B.trialKills[i] = B.trialKills[i + 1];
    B.trials >>= 1;
    g.hour += 1.0f;
    if (g.hour >= 24) { g.hour -= 24; g.day++; }
  }
  B.attuned.push_back(t.id);
  if (!B.mastery.count(t.id)) B.mastery[t.id] = 0;
  addLand(g, t.culture);
  if (t.res == magic::Resource::Starlight) B.res[(int)t.res] = std::max(B.res[(int)t.res], 30.0f);
  MagicOps::ev(g, Ev::Notice, g.pl().p, (int)rgba(220, 200, 255), 0, "ATTUNED: " + t.name);
  MagicOps::snd(g, (int)Sfx::LevelUp, g.pl().p, 0.9f);
  return true;
}

bool MagicOps::mayLearn(const Game& g, const SpellDef& d, std::string* why) {
  if (g.arcana.book.knows(d.id)) { if (why) *why = "YOU KNOW " + d.name + " ALREADY"; return false; }
  if (d.tradition) {
    if (!attuned(g, d.tradition)) { if (why) *why = "ATTUNE TO " + magic::traditionOfSpell(d).name + " FIRST"; return false; }
    const magic::Tradition& t = magic::traditionOfSpell(d);
    if (t.hidden) {   // (M10) the five steps (6.4): a hidden tradition's rank 3 also needs Honoured 60 and the second trial
      if (hiddenStep(g, t, nullptr, d.rank, why) != 0) return false;
      return true;
    }
    const int r = masteryRankOf(g, d.tradition);
    if ((int)d.rank > r) {
      if (why) *why = d.rank >= 3 ? "ITS SIGNATURE NEEDS MASTERY 1200 AND THE TRIAL OF THE THRESHOLD" : "IT NEEDS MASTERY RANK 2 (300)";
      return false;
    }
    return true;
  }
  const int need = magic::skillNeed(d);
  const int have = g.skills.level(magic::schoolSkill(d.school));
  if (have < need) { if (why) *why = "IT NEEDS " + std::string(magic::schoolName(d.school)) + " " + std::to_string(need); return false; }
  return true;
}

bool MagicOps::readTome(Game& g, int i) {
  if (i < 0 || i >= (int)g.inv.size() || !magic::isTome(g.inv[(size_t)i])) return false;
  const uint32_t id = g.inv[(size_t)i].seed;
  const SpellDef* d = magic::spellById(id);
  if (!d) { MagicOps::tell(g, "THE TOME'S SCRIPT IS UNKNOWN TO YOU"); return false; }
  std::string why;
  if (!mayLearn(g, *d, &why)) { MagicOps::tell(g, why); return false; }
  return learn(g, id, true);
}

// ---- the loadout (15.5: swapped at rest or camp)
bool MagicOps::mayPrepare(const Game& g, std::string* why) {
  const Actor& p = g.pl();
  for (const Actor& e : g.actors)
    if (e.hostile && e.st != AState::Dead && e.aggro && len2(e.p - p.p) < (12.0f * TILE) * (12.0f * TILE)) { if (why) *why = "NOT WITH FOES CLOSE"; return false; }
  if (g.arcana.live.prepT > 0 || g.inside) return true;
  if (!g.inside && g.settlementAt(p.p) >= 0) return true;
  const Map& m = g.map();
  const int cx = (int)std::floor(p.p.x / TILE), cy = (int)std::floor(p.p.y / TILE);
  for (int y = cy - 3; y <= cy + 3; y++)
    for (int x = cx - 3; x <= cx + 3; x++) {
      const int pp = m.propAt(x, y);
      if (pp && ((Prop)(pp - 1) == Prop::Campfire || (Prop)(pp - 1) == Prop::Bedroll || (Prop)(pp - 1) == Prop::Tent)) return true;
    }
  if (why) *why = "CHANGE YOUR PREPARED SPELLS AT REST OR AT CAMP";
  return false;
}
bool MagicOps::prepare(Game& g, int slot, uint32_t id, std::string* why) {
  magic::Book& B = g.arcana.book;
  if (slot < 0 || slot >= magic::QUICK_SLOTS) { if (why) *why = "NO SUCH SLOT"; return false; }
  if (id && !B.knows(id)) { if (why) *why = "YOU DO NOT KNOW THAT SPELL"; return false; }
  if (B.slot[slot] == id) return true;
  if ((B.slot[slot] != 0 || id == 0) && !mayPrepare(g, why)) return false;   // (filling an empty slot is always allowed)
  for (uint32_t& s : B.slot) if (id && s == id) s = 0;
  B.slot[slot] = id;
  if (g.arcana.live.cast.slot == slot) g.arcana.live.cast = magic::Cast();
  syncLegacy(g);
  return true;
}
void MagicOps::rested(Game& g, int hours) {
  if (hours <= 0) return;
  magic::Live& L = g.arcana.live;
  L.prepT = 180.0f;   // (a few minutes to set the loadout after waking)
  g.arcana.book.res[(int)magic::Resource::Heat] = 0;
  L.ward = L.wardT = 0;
}

// ---- teachers
void MagicOps::talk(Game& g, Actor& a) {
  const unsigned sch = teaches(g, a);
  if (!sch) return;
  const uint32_t h = nameHash(a.name) ^ (uint32_t)a.site * 2654435761u;
  // the schools' spells this teacher would teach now (the hero's skill allows them), a few at a time: the classic Mend
  // and Frost Lance first (a mage's old lessons), then by tier, the teacher's own pick among equals
  std::vector<const SpellDef*> can;
  for (const SpellDef& s : magic::schoolSpells()) {
    if (!(sch & (1u << (int)s.school)) || !mayLearn(g, s)) continue;
    can.push_back(&s);
  }
  std::stable_sort(can.begin(), can.end(), [&](const SpellDef* x, const SpellDef* y) {
    const bool cx = x->id == magic::SPELL_MEND || x->id == magic::SPELL_FROST_LANCE, cy = y->id == magic::SPELL_MEND || y->id == magic::SPELL_FROST_LANCE;
    if (cx != cy) return cx;
    if (x->tier != y->tier) return x->tier < y->tier;
    return (hash32(x->id ^ h) & 0xFFFF) < (hash32(y->id ^ h) & 0xFFFF);
  });
  const int show = a.role == Role::Mage ? 3 : 2;
  for (int i = 0; i < (int)can.size() && i < show; i++)
    g.dlg.opts.push_back({"TEACH ME " + can[(size_t)i]->name + " (" + std::to_string(magic::teachPrice(*can[(size_t)i])) + " GOLD)", DLG_MAGIC + MK_LEARN, (int)can[(size_t)i]->id});
  // a mage of a land with its own tradition: attunement, then its spells by mastery rank
  if (a.role != Role::Mage) return;
  const cult::Culture* c = cultureHere(g, &a);
  if (!c) return;
  const magic::Tradition& t = magic::traditionOf(*c);
  if (!t.id) return;
  if (!attuned(g, t.id)) {
    if (t.hidden) g.dlg.opts.push_back({"TEACH ME THE WAYS OF " + t.name, DLG_MAGIC + MK_HIDDEN, (int)t.id});   // (M10) the five steps
    else g.dlg.opts.push_back({"ATTUNE ME TO " + t.name, DLG_MAGIC + MK_ATTUNE, (int)t.id});
    return;
  }
  int n = 0;
  for (uint32_t sid : t.spells) {
    const SpellDef* d = magic::spellById(sid);
    if (!d || !mayLearn(g, *d) || n >= 2) continue;
    g.dlg.opts.push_back({"TEACH ME " + d->name + " (" + std::to_string(magic::teachPrice(*d)) + " GOLD)", DLG_MAGIC + MK_TRAD, (int)sid});
    n++;
  }
}

bool MagicOps::choose(Game& g, const DlgOpt& o) {
  const int kind = o.action - DLG_MAGIC;
  int oi = -1;
  for (size_t i = 0; i < g.dlg.opts.size(); i++) if (g.dlg.opts[i].action == o.action && g.dlg.opts[i].arg == o.arg) oi = (int)i;
  if (kind == MK_LEARN || kind == MK_TRAD) {
    const SpellDef* d = magic::spellById((uint32_t)o.arg);
    if (!d) return false;
    std::string why;
    if (!mayLearn(g, *d, &why)) { g.dlg.text = why + "."; return true; }
    const int price = magic::teachPrice(*d);
    if (g.gold < price) {
      // (P1 integrator: the dialogue review) name the lesson and the shortfall, and take the offer off the list so a
      // second press doesn't get the same refusal
      g.dlg.text = d->name + " IS " + std::to_string(price) + " GOLD, AND YOU'RE " + std::to_string(price - g.gold) +
                   " SHORT. COME BACK WITH THE REST.";
      if (oi >= 0) g.dlg.opts.erase(g.dlg.opts.begin() + oi);
      return true;
    }
    g.gold -= price;
    learn(g, d->id, false);
    if (d->tradition) addLand(g, magic::traditionOfSpell(*d).culture);
    MagicOps::snd(g, (int)Sfx::LevelUp, g.pl().p, 1.2f);
    g.dlg.text = "YOU HAVE LEARNED " + d->name + ". USE IT WISELY.";
    if (oi >= 0) g.dlg.opts.erase(g.dlg.opts.begin() + oi);
    return true;
  }
  if (kind == MK_HIDDEN) {   // (M10) the teacher grants rank 1 when the first four steps are met; else says which is not
    const magic::Tradition* t = magic::traditionById((uint32_t)o.arg);
    if (!t) return false;
    std::string why;
    const int step = hiddenStep(g, *t, nullptr, 0, &why);
    if (step != 0 && step != 5) { g.dlg.text = why + "."; return true; }
    magic::Book& B = g.arcana.book;
    if (std::find(B.attuned.begin(), B.attuned.end(), t->id) == B.attuned.end()) {
      // the teacher's rank 1 is an attunement that needs no level gate past the steps (the steps are the gate)
      if (!attune(g, *t, &why)) { g.dlg.text = why + "."; return true; }
    }
    g.dlg.text = "YOU HAVE WALKED THE THRESHOLD. " + std::string(magic::signatureName(t->sig)) + " IS YOURS NOW, AS IT IS OURS. BEGIN WITH THESE.";
    if (oi >= 0) g.dlg.opts.erase(g.dlg.opts.begin() + oi);
    int n = 0;
    for (uint32_t sid : t->spells) {
      const SpellDef* d = magic::spellById(sid);
      if (!d || d->rank != 1 || !mayLearn(g, *d) || n >= 3) continue;
      g.dlg.opts.push_back({"TEACH ME " + d->name + " (" + std::to_string(magic::teachPrice(*d)) + " GOLD)", DLG_MAGIC + MK_TRAD, (int)sid});
      n++;
    }
    return true;
  }
  if (kind == MK_ATTUNE) {
    const magic::Tradition* t = magic::traditionById((uint32_t)o.arg);
    if (!t) return false;
    std::string why;
    if (!attune(g, *t, &why)) { g.dlg.text = why + "."; return true; }
    g.dlg.text = "THEN LISTEN, AND LEARN THE WAYS OF " + t->name + ". " + magic::signatureName(t->sig) + " IS OURS ALONE.";
    if (oi >= 0) g.dlg.opts.erase(g.dlg.opts.begin() + oi);
    return true;
  }
  return false;
}

// ---- shelves and loot
void MagicOps::shopStock(Game& g, const Actor& a, std::vector<Item>& stock) {
  const unsigned sch = teaches(g, a);
  if (!sch) return;
  const uint32_t h = nameHash(a.name) ^ hash32((uint32_t)(g.day / 7) * 0x9E37u + 17u);
  const int maxTier = a.role == Role::Mage ? 2 : 1;
  std::vector<uint32_t> pool;
  for (const SpellDef& s : magic::schoolSpells())
    if ((sch & (1u << (int)s.school)) && (int)s.tier <= maxTier) pool.push_back(s.id);
  const int want = a.role == Role::Mage ? 3 : 1;
  uint32_t r = h;
  for (int i = 0; i < want && !pool.empty(); i++) {
    r = hash32(r + 0x51u);
    const size_t k = r % (uint32_t)pool.size();
    stock.push_back(magic::makeTome(pool[k]));
    pool.erase(pool.begin() + (std::ptrdiff_t)k);
  }
  // a mage of a land with its own tradition sells one of its rank-1 tomes (M10: a hidden land's, its focus too: sold
  // nowhere else)
  if (a.role != Role::Mage) return;
  if (const cult::Culture* c = cultureHere(g, &a)) {
    const magic::Tradition& t = magic::traditionOf(*c);
    if (t.hidden) stock.push_back(focusOf(g, t));
    std::vector<uint32_t> r1;
    for (uint32_t sid : t.spells) if (const SpellDef* d = magic::spellById(sid)) if (t.id && d->rank == 1) r1.push_back(sid);
    if (!r1.empty()) stock.push_back(magic::makeTome(r1[(size_t)(hash32(h ^ 0x7AD1u) % (uint32_t)r1.size())]));
  }
}

void MagicOps::loot(Game& g, Vec2 at, int D, gear::LootSrc src) {
  using S = gear::LootSrc;
  int permille = 0, maxTier = 0;
  switch (src) {
    case S::Body: permille = 5; maxTier = std::min(2, D / 12); break;
    case S::Chest: permille = 30; maxTier = std::min(2, D / 10); break;
    case S::Hoard: permille = 100; maxTier = std::min(3, D / 9); break;
    case S::Boss: permille = 150; maxTier = std::min(3, D / 8); break;
    case S::Explore: permille = 150; maxTier = std::min(4, D / 8); break;
    case S::WorldBoss: permille = 400; maxTier = std::min(4, D / 7); break;
    default: return;
  }
  const uint32_t h = hash2((int)std::floor(at.x) * 7 + D, (int)std::floor(at.y) * 13 + (int)src, (uint32_t)g.seed ^ (uint32_t)g.kills * 977u ^ (uint32_t)g.day * 131u);
  if ((int)(h % 1000u) >= permille) return;
  // the strongest magic only from ruins, lost traditions and distant lands: a find in the wild may hold a lost
  // tradition's rank-2 tome of the land it lies in
  uint32_t id = 0;
  if (src == S::Explore || src == S::WorldBoss) {
    if (const cult::Culture* c = cultureHere(g, nullptr)) {
      const magic::Tradition& t = magic::traditionOf(*c);
      std::vector<uint32_t> r2;
      for (uint32_t sid : t.spells) if (const SpellDef* d = magic::spellById(sid)) if (t.id && d->rank == 2) r2.push_back(sid);
      if (!r2.empty() && (h >> 10) % 3u == 0) id = r2[(size_t)((h >> 12) % (uint32_t)r2.size())];
    }
  }
  if (!id) {
    const int tier = maxTier <= 0 ? 0 : (int)((h >> 14) % (uint32_t)(maxTier + 1));
    std::vector<uint32_t> pool;
    for (const SpellDef& s : magic::schoolSpells()) if ((int)s.tier == tier && magic::legacySpellOf(s.id) < 0) pool.push_back(s.id);
    if (pool.empty()) return;
    id = pool[(size_t)((h >> 18) % (uint32_t)pool.size())];
  }
  Pickup k;
  k.p = at + Vec2((float)((int)(h >> 4) % 9 - 4), (float)((int)(h >> 8) % 7 - 3));
  k.item = magic::makeTome(id);
  g.pickups.push_back(k);
}

// ---- (M10) the hidden traditions: the five steps (6.4), the standing, first contact, the focus, the song land
int MagicOps::standing(const Game& g, const magic::Tradition& t, const Actor* who) {
  if (g.arcana.live.repOverride > -1000) return g.arcana.live.repOverride;
  int site = who ? siteOfActor(g, *who) : -1;
  if (site < 0 && g.inside) site = g.subSite >= 0 ? g.subSite : g.curSite;
  if (site >= 0 && site < (int)g.world.sites.size()) {
    const int k = g.world.sites[(size_t)site].kingdom;
    if (k >= 0 && k < (int)g.world.kingdoms.size())
      if (const realm::KingdomState* K = g.realm.kingdom(g.world.kingdoms[(size_t)k].id)) return K->playerRep;
  }
  for (const realm::KingdomState& K : g.realm.kingdoms()) if (K.culture == t.culture && !K.fallen) return K.playerRep;
  return 0;
}
bool MagicOps::landed(const Game& g, const magic::Tradition& t) {
  const cult::Culture* c = nullptr;
  if (g_land && g_land->id == t.culture) c = g_land.get();
  else if (g.world.src && t.culture) c = &g.world.src->culture(t.culture);
  if (!c || !c->hiddenLand) return true;   // (no Mist between: nothing to cross)
  const sea::LandRecord* r = g.sea.land(c->hiddenLand);
  return r && (r->flags & sea::LF_LANDED);
}
int MagicOps::hiddenStep(const Game& g, const magic::Tradition& t, const Actor* teacher, int rank, std::string* why) {
  const magic::Book& B = g.arcana.book;
  auto no = [&](int step, const std::string& s) { if (why) *why = s; return step; };
  if (!landed(g, t)) return no(1, "NO ONE FROM BEYOND THE MIST HAS EVER STOOD HERE. COME BY SEA, AND SET FOOT ON OUR SHORE");
  const int rep = standing(g, t, teacher);
  if (rep < magic::REP_FRIENDLY) return no(2, "WE DO NOT TEACH STRANGERS. EARN OUR TRUST FIRST (FRIENDLY 25, YOU ARE " + std::to_string(rep) + ")");
  if (g.plLevel < magic::HIDDEN_LEVEL) return no(3, "YOU ARE NOT READY. COME BACK AT LEVEL " + std::to_string(magic::HIDDEN_LEVEL));
  if (!(B.rite(t.id) & 1u)) return no(4, "FIRST PASS THE TRIAL OF THE THRESHOLD. ITS DOOR WILL TEACH YOU OUR WAY");
  if (!attuned(g, t.id)) return no(5, "ASK ONE OF OUR TEACHERS FOR THE FIRST RANK");
  if (rank <= 1) return 0;
  const auto it = B.mastery.find(t.id);
  const int xp = it == B.mastery.end() ? 0 : it->second;
  if (rank == 2) return xp >= magic::MASTERY_RANK2 ? 0 : no(8, "IT NEEDS MASTERY RANK 2 (300)");
  if (rep < magic::REP_HONOURED) return no(6, "OUR SIGNATURE IS FOR THE HONOURED (60, YOU ARE " + std::to_string(rep) + ")");
  if (!(B.rite(t.id) & 2u)) return no(7, "THE SIGNATURE NEEDS THE SECOND TRIAL");
  if (xp < magic::MASTERY_RANK3) return no(8, "THE SIGNATURE NEEDS MASTERY 1200");
  return 0;
}
Item MagicOps::focusOf(Game& g, const magic::Tradition& t) {
  Rng r(hash32(t.id ^ 0xF0C05u));
  const cult::Culture* c = nullptr;
  if (g_land && g_land->id == t.culture) c = g_land.get();
  else if (g.world.src && t.culture) c = &g.world.src->culture(t.culture);
  Item it = gear::makeGearC(r, ItemKind::Staff, 0, std::clamp(g.plLevel + 2, 1, gear::MAX_D), Rarity::Rare, c);
  if (!t.focus.empty()) it.name = t.focus;
  if (const SpellDef* d = t.spells.empty() ? nullptr : magic::spellById(t.spells.back())) it.tint = d->fx.ramp[2];
  it.value = std::max(it.value, 900 + 40 * g.plLevel);
  return it;
}
void MagicOps::findSongLand(Game& g) {
  magic::Live& L = g.arcana.live;
  L.songWorld = g.seed + 1;
  if (!g.world.src) return;
  const int32_t R = 6144 * 4;
  uint32_t best = 0;
  int64_t bd = INT64_MAX;
  for (const ew::LandPlan& lp : g.world.src->landsIn(-R, -R, R, R)) {
    if (!lp.hidden) continue;
    const int64_t d = (int64_t)lp.cx * lp.cx + (int64_t)lp.cy * lp.cy;
    if (d < bd) { bd = d; best = lp.id; }
  }
  magic::setSongLand(best);
}

// ---- a load, the companions
void MagicOps::loaded(Game& g) {
  if (!g.world.src) return;
  findSongLand(g);   // (M10) before any hidden land's tradition is made again
  for (uint64_t c : g.arcana.book.lands) magic::traditionOf(g.world.src->culture(c));
}
uint32_t MagicOps::companionSpell(Game& g, uint64_t culture) {
  if (!culture || !g.world.src) return magic::SPELL_FIREBOLT;
  const magic::Tradition& t = magic::traditionOf(g.world.src->culture(culture));
  if (!t.id) return magic::SPELL_FIREBOLT;
  for (uint32_t sid : t.spells) {
    const SpellDef* d = magic::spellById(sid);
    if (d && d->rank == 1 && (d->form == magic::Form::Bolt || d->form == magic::Form::Lance) && magic::damaging(*d)) return sid;
  }
  return magic::SPELL_FIREBOLT;
}
