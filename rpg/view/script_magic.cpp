// M9 script commands (MAGIC lane): stage the spells for screenshots and checks (VISION_PLAN 15.5, 15.29.3).
//   spell learn <spell>            learn it (spell: its name without spaces, any case: firebolt, frostlance, spark ...;
//                                  a tradition's spell of the land or of an attuned tradition by name; or its id)
//   spell slot <i> <spell>         put a known spell in quick slot i (1..4)
//   spell cast [i]                 cast quick slot i (default: the selected one) now, at the nearest foe in front (no
//                                  wind-up: the launch at once)
//   spell upcast <i> <level> [dual]  cast slot i now at an upcast level 0..3 (dual: dual-cast as well)
//   spell dual <i>                 cast slot i now as a dual-cast
//   spell charge <i> <level>       begin holding slot i and pour in up to <level> upcast levels (WindUp, Charge: the
//                                  hands glowing for a shot); `spell release` lets it go
//   spell release                  let a held cast go
//   spell mp <n>                   magicka to n (the pool's cap moves with it)
//   spell aim <dx> <dy>            the hero's aim (and facing)
//   spell rain on|off|auto         force the rain (Tidebinding, rain dousing fire) or let the land decide
//   spell moon on|off|auto         force the full moon (Starcalling)
//   spell land <archetype> [seed]  stand in a far land: the culture teachers and the signatures read (off: the real land)
//   spell attune                   attune to the land's tradition
//   spell trad <k|last>            learn the land's tradition's k-th spell (1 = the first, last = its signature;
//                                  attunes first) into slot 1
//   spell teacher [mage|priest|scholar]   a teacher at the hero's side, the talk with them open
//   spell choose <learn|attune|tradition>  choose the open talk's first option of that kind (a tradition's spell name
//                                  is not known to a script)
//   spell pool <starlight|heat|charge> <n>  a signature's pool (spell pool full: every pool to its cap)
//   surface <kind> <dx> <dy> [s]   lay a surface (water, ice, oil, fire, poison, shock, steam, blood) dx, dy tiles from the
//                                  hero for s seconds (default 20), combining with what lies there
//   status <name> <secs>           put a status (burning, chilled, frozen, wet, shocked, frightened, charmed, poisoned,
//                                  blinded) on the nearest foe
//   expect spell <spell>           the spell is known
//   expect status <name> [n]       at least n actors (default 1) carry that status
//   expect surface <kind> [n]      at least n tiles of that surface lie on the map
//   expect summon [n]              at least n summons (or charmed / bound foes) fight for the hero
//   expect combo [n]               at least n surface combos went off
//   expect cast [n]                at least n spells were cast
//   expect attuned [n]             at least n traditions attuned
//   expect conc <spell|none>       the concentration held
// (M10 ARCANA lane) spell hidden | steps | trial | plate | priest | hp | water and expect arcana: see cmdHidden below
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/sim/rooms.h"
#include "rpg/world/source.h"
#include "rpg/view/script_api.h"

namespace {
std::string squashSp(const std::string& s) {
  std::string r;
  for (char ch : s) {
    if (ch == ' ' || ch == '_' || ch == '-' || ch == '\'') continue;
    r += (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
  }
  return r;
}
const magic::Tradition* landTradition(Game& g) {
  const cult::Culture* c = MagicOps::cultureHere(g, nullptr);
  return c ? &magic::traditionOf(*c) : nullptr;
}
uint32_t spellWord(Game& g, const std::string& w) {
  const std::string q = squashSp(w);
  for (const magic::SpellDef& s : magic::schoolSpells()) if (squashSp(s.name) == q) return s.id;
  std::vector<const magic::Tradition*> ts;
  if (const magic::Tradition* t = landTradition(g)) ts.push_back(t);
  for (uint32_t tid : g.arcana.book.attuned) if (const magic::Tradition* t = magic::traditionById(tid)) ts.push_back(t);
  for (const magic::Tradition* t : ts)
    for (uint32_t sid : t->spells) if (const magic::SpellDef* d = magic::spellById(sid)) if (squashSp(d->name) == q) return sid;
  const unsigned long id = std::strtoul(w.c_str(), nullptr, 10);
  return magic::spellById((uint32_t)id) ? (uint32_t)id : 0;
}
bool statusWord(const std::string& w, magic::Status& out) {
  for (int s = 0; s < magic::STATUSES; s++) if (squashSp(magic::statusName((magic::Status)s)) == squashSp(w)) { out = (magic::Status)s; return true; }
  return false;
}
bool surfaceWord(const std::string& w, magic::Surface& out) {
  const std::string q = squashSp(w);
  if (q == "shock") { out = magic::Surface::Shock; return true; }
  for (int s = 1; s < (int)magic::Surface::COUNT; s++) if (squashSp(magic::surfaceName((magic::Surface)s)) == q) { out = (magic::Surface)s; return true; }
  return false;
}
int8_t onOff(const std::string& w) { const std::string q = squashSp(w); return q == "on" || q == "1" ? 1 : q == "off" || q == "0" ? 0 : -1; }
// the slot a staged cast uses (1..4 -> 0..3; empty: the selected one); -1 when it holds nothing
int slotArg(Game& g, const std::string& w) {
  const int i = w.empty() ? g.arcana.book.sel : std::atoi(w.c_str()) - 1;
  if (i < 0 || i >= magic::QUICK_SLOTS || !g.arcana.book.slot[i]) return -1;
  return i;
}
void readyHero(Game& g) {
  g.pl().shootCd = 0;
  if (g.pl().st != AState::Dead) g.pl().st = AState::Idle;
  g.arcana.live.cast = magic::Cast();
}
// a staged cast: at once, at a level, dual or not (the slot's cooldown is the spell's own)
void stageCast(ScriptCtx& c, int slot, int level, bool dual) {
  Game& g = c.game;
  if (slot < 0) { c.fail("spell: that quick slot is empty"); return; }
  const magic::SpellDef* d = magic::spellById(g.arcana.book.slot[slot]);
  if (!d) { c.fail("spell: unknown spell in the slot"); return; }
  g.arcana.book.sel = (uint8_t)slot;
  g.arcana.live.legacyMirror = (int8_t)g.spell;
  readyHero(g);
  g.arcana.live.cd[slot] = 0;
  MagicOps::launch(g, *d, slot, level, dual);
}
int dlgPick(Game& g, int kind) {
  for (size_t i = 0; i < g.dlg.opts.size(); i++) if (g.dlg.opts[i].action == DLG_MAGIC + kind) return (int)i;
  return -1;
}

// ---- (M10 ARCANA lane) the hidden kingdoms' magic, staged
//   spell hidden <blood|rune|song|mask|verdant|dream> [steps]   stand in a land beyond the Mist whose tradition has that
//                                  signature (song: the nearest hidden land's people); steps: every step of learning met
//   spell steps                    the land's five steps met (landed, Honoured, level 25, both trials) and attuned
//   spell trial [2]                the nearest ruin made the land's Trial of the Threshold (2: the second) and entered
//   spell plate <kind> [k] [dy]    stand (dy tiles south of it) on the k-th plate (1..) of that kind in the trial (note, rune, mask, pod, altar,
//                                  mirror, press, sanctum); "motif k": the note plate singing the motif's k-th note in the
//                                  hall whose gate is still closed
//   spell priest [n]               n priests (default 2) of the land beside the hero (the dusk song-fire)
//   spell hp <percent>             the hero's health to that share of the most
//   spell water <n> [dx]           a strip of n water tiles starting dx tiles (default 2) ahead of the hero (a bridge's)
//   expect arcana <stat> [n]       at least n of: notes, refrains, runetriggers, runecombos, maskstrikes, maskhits,
//                                  growths, reverts, snares, decoys, decoyhits, slowed, bubbles, songfires, gates, blood,
//                                  rites (the hidden rites passed), mask (1 while one is worn), trial (1 in a trial),
//                                  open (its gates open now), free (1: the hero's tile is not blocked)
cult::Culture hiddenOf(magic::Signature want) {
  if (want == magic::Signature::Song && magic::songLand()) {   // the nearest hidden land's people: they always sing
    for (uint32_t s = 1; s < 64; s++) {
      cult::Culture c = cult::Atlas::make((cult::Archetype)(s % (uint32_t)cult::Archetype::COUNT), 4000u + s);
      if (!c.id) c.id = cult::familyId((int)(s % 97u) * 7 + 3, (int32_t)s);
      c.hidden = true; c.hiddenLand = magic::songLand();
      return c;
    }
  }
  for (uint32_t s = 1; s < 600; s++) {
    cult::Culture c = cult::Atlas::make((cult::Archetype)(s % (uint32_t)cult::Archetype::COUNT), 7000u + s * 13u);
    if (!c.id) c.id = cult::familyId((int)(s % 97u) * 7 + 3, (int32_t)(s % 1000u));
    c.hidden = true; c.hiddenLand = 0x20000001u + s * 2u;
    std::vector<magic::SpellDef> sp;
    if (magic::generateTradition(c, sp).sig == want) return c;
  }
  return cult::Culture();
}
void meetSteps(Game& g, const magic::Tradition& t) {
  const cult::Culture* c = MagicOps::cultureHere(g, nullptr);
  if (c && c->hiddenLand) g.sea.landMut(c->hiddenLand).flags |= sea::LF_LANDED;
  MagicOps::live(g).repOverride = 60;
  if (g.plLevel < magic::HIDDEN_LEVEL) g.plLevel = magic::HIDDEN_LEVEL;
  g.arcana.book.passRite(t.id, 0);
  g.arcana.book.passRite(t.id, 1);
  if (!MagicOps::attuned(g, t.id)) MagicOps::attune(g, t);
  g.arcana.book.mastery[t.id] = std::max<uint16_t>(g.arcana.book.mastery[t.id], magic::MASTERY_RANK3);
}
const TrialLayout* trialNow(Game& g) {
  if (!g.inside || g.subSite < 0 || g.subSite >= (int)g.world.sites.size()) return nullptr;
  return trialLayoutOf(g.world.sites[(size_t)g.subSite].id);
}
bool cmdHidden(ScriptCtx& c, const std::string& op) {
  Game& g = c.game;
  if (op == "hidden") {
    static const char* names[] = {"blood", "rune", "song", "mask", "verdant", "dream"};
    static const magic::Signature sigs[] = {magic::Signature::Blood, magic::Signature::Rune, magic::Signature::Song, magic::Signature::Mask, magic::Signature::Verdant, magic::Signature::Dream};
    const std::string w = squashSp(c.arg(2));
    for (int i = 0; i < 6; i++) {
      if (w != names[i]) continue;
      if (!magic::songLand()) MagicOps::findSongLand(g);
      const cult::Culture cu = hiddenOf(sigs[i]);
      if (!cu.id) { c.fail("spell hidden: no such people"); return true; }
      MagicOps::setLand(&cu);
      if (squashSp(c.arg(3)) == "steps") meetSteps(g, magic::traditionOf(cu));
      return true;
    }
    c.fail("spell hidden: blood | rune | song | mask | verdant | dream");
    return true;
  }
  if (op == "steps") {
    const magic::Tradition* t = landTradition(g);
    if (!t || !t->hidden) { c.fail("spell steps: this land has no hidden tradition"); return true; }
    meetSteps(g, *t);
    return true;
  }
  if (op == "trial") {
    const cult::Culture* cu = MagicOps::cultureHere(g, nullptr);
    if (!cu || !cu->hidden) { c.fail("spell trial: stand in a hidden land first (spell hidden ...)"); return true; }
    int best = -1;
    float bd = 1e30f;
    for (int i = 0; i < (int)g.world.sites.size(); i++) {
      const Site& s = g.world.sites[(size_t)i];
      if (s.type != SiteType::Ruin) continue;
      const float d = len2(Vec2((float)s.ex * TILE, (float)s.ey * TILE) - g.pl().p);
      if (d < bd) { bd = d; best = i; }
    }
    if (best < 0 || !MagicOps::markTrial(g, best, *cu, std::atoi(c.arg(2).c_str()) >= 2 ? 1 : 0)) { c.fail("spell trial: no ruin in the window"); return true; }
    MagicOps::enterSiteRaw(g, best);
    return true;
  }
  // (fixer M10 r1) spell realtrial [enter]: the WORLD lane's own Trial of the Threshold of the hidden land the hero
  // stands on (the travel to it; `enter`: walk in once it is in the window). spell reload [fresh]: save and load back
  // in place, as Continue does (fresh: the trial registry emptied first, as after a new process)
  if (op == "realtrial") {
    const cult::Culture* cu = MagicOps::cultureHere(g, nullptr);
    if (!cu || !cu->hidden || !g.world.src) { c.fail("spell realtrial: stand on a hidden land first (sea land)"); return true; }
    ew::SitePlan sp;
    bool ok = false;
    for (int k = 0; k < (int)g.world.kingdoms.size() && !ok; k++) {
      const cult::Culture* kc = g.world.cultureOfKingdom(k);
      if (kc && kc->id == cu->id) ok = g.world.src->trialOf(g.world.kingdoms[(size_t)k].id, 0, sp);
    }
    if (!ok) { c.fail("spell realtrial: this land's kingdom has no trial"); return true; }
    if (squashSp(c.arg(2)) != "enter") { g.beginTravelTo(sp.ex, sp.ey); return true; }
    for (int i = 0; i < (int)g.world.sites.size(); i++)
      if (g.world.sites[(size_t)i].id == sp.id) { MagicOps::enterSiteRaw(g, i); return true; }
    c.fail("spell realtrial enter: the trial is not in the window");
    return true;
  }
  if (op == "reload") {
    std::vector<uint8_t> buf;
    g.serialize(buf);
    if (squashSp(c.arg(2)) == "fresh") clearTrials();
    if (!g.deserialize(buf)) c.fail("spell reload: the save did not load");
    return true;
  }
  if (op == "plate") {
    const TrialLayout* T = trialNow(g);
    if (!T) { c.fail("spell plate: not in a trial"); return true; }
    const std::string w = squashSp(c.arg(2));
    const int k = std::max(1, std::atoi(c.arg(3).c_str()));
    int found = -1;
    if (w == "motif") {
      int room = 3;
      for (size_t gi = 0; gi < T->gates.size(); gi++) if (!((g.arcana.live.trial.gatesOpen >> gi) & 1u)) { room = std::min<int>(room, T->gates[gi].room); }
      for (size_t i = 0; i < T->plates.size(); i++)
        if (T->plates[i].kind == TP_NOTE && T->plates[i].room == room && T->plates[i].arg == T->spec.motif[std::clamp(k - 1, 0, 2)]) { found = (int)i; break; }
    } else {
      static const char* kinds[] = {"note", "rune", "mask", "pod", "altar", "mirror", "press", "sanctum"};
      int kind = -1;
      for (int i = 0; i < 8; i++) if (w == kinds[i]) kind = i;
      int n = 0;
      for (size_t i = 0; i < T->plates.size() && kind >= 0; i++) if (T->plates[i].kind == kind && ++n == k) { found = (int)i; break; }
    }
    if (found < 0) { c.fail("spell plate: no such plate"); return true; }
    const TrialPlate& p = T->plates[(size_t)found];
    const int dy = std::atoi(c.arg(4).c_str());   // (a 4th word: that many tiles south of it, off the plate)
    g.pl().p = Vec2((float)p.x * TILE + 8, (float)(p.y + dy) * TILE + 9);
    g.pl().face = 1;
    return true;
  }
  if (op == "motif") {   // spell motif <k>: the Songweaving spell that sings the motif's k-th note, learned into slot 1
    const magic::Tradition* t = landTradition(g);
    const int k = std::clamp(std::atoi(c.arg(2).c_str()) - 1, 0, 2);
    if (!t || t->sig != magic::Signature::Song) { c.fail("spell motif: this land does not sing"); return true; }
    if (!MagicOps::attuned(g, t->id)) MagicOps::attune(g, *t);
    for (uint32_t sid : t->spells)
      if (const magic::SpellDef* d = magic::spellById(sid))
        if (d->rank == 1 && d->note == (int)t->motif[k]) {
          MagicOps::learn(g, sid, false);
          for (uint32_t& s : g.arcana.book.slot) if (s == sid) s = 0;
          g.arcana.book.slot[0] = sid;
          g.arcana.book.sel = 0;
          return true;
        }
    c.fail("spell motif: no rank-1 spell sings that note");
    return true;
  }
  if (op == "priest") {
    const int n = c.arg(2).empty() ? 2 : std::max(1, std::atoi(c.arg(2).c_str()));
    for (int i = 0; i < n; i++) {
      const float a = 0.6f + 2.2f * (float)i;
      MagicOps::spawnTeacher(g, g.pl().p + Vec2(std::cos(a) * 34.0f, std::sin(a) * 18.0f - 6.0f), Role::Priest, 20 + i);
    }
    return true;
  }
  if (op == "clear") {   // spell clear: every foe and decoy gone, the spells' marks on the map wiped (a clean stage)
    for (size_t i = 1; i < g.actors.size();) {
      const Actor& a = g.actors[i];
      if ((a.hostile && !a.comp) || a.magicTag == magic::MT_DECOY || a.magicTag == magic::MT_SUMMON) g.actors.erase(g.actors.begin() + (std::ptrdiff_t)i); else i++;
    }
    g.projs.clear();
    MagicOps::clearMap(g);
    MagicOps::live(g).mask = -1;
    return true;
  }
  if (op == "shore") {   // spell shore: on the bank of the nearest water 1..3 tiles across, facing it (a vine bridge's place)
    const Map& m = g.map();
    const int hx = (int)std::floor(g.pl().p.x / TILE), hy = (int)std::floor((g.pl().p.y - 2) / TILE);
    int bx = -1, by = -1, bdx = 0, bdy = 0;
    long best = LONG_MAX;
    auto land = [&](int x, int y) { return m.in(x, y) && !m.blocked(x, y) && !groundWater(m.at(x, y)) && !m.propAt(x, y); };
    for (int y = std::max(2, hy - 90); y < std::min(m.h - 2, hy + 90); y++)
      for (int x = std::max(2, hx - 90); x < std::min(m.w - 2, hx + 90); x++) {
        if (!land(x, y)) continue;
        for (int dir = 0; dir < 4; dir++) {
          static const int ddx[4] = {1, -1, 0, 0}, ddy[4] = {0, 0, 1, -1};
          int n = 0;
          while (n <= 3 && m.in(x + ddx[dir] * (n + 1), y + ddy[dir] * (n + 1)) && m.at(x + ddx[dir] * (n + 1), y + ddy[dir] * (n + 1)) == Ground::Water) n++;
          if (n < 2 || n > 3 || !land(x + ddx[dir] * (n + 1), y + ddy[dir] * (n + 1))) continue;
          const long d = (long)(x - hx) * (x - hx) + (long)(y - hy) * (y - hy);
          if (d < best) { best = d; bx = x; by = y; bdx = ddx[dir]; bdy = ddy[dir]; }
        }
      }
    if (bx < 0) { c.fail("spell shore: no narrow water in the window"); return true; }
    g.pl().p = Vec2((float)bx * TILE + 8, (float)by * TILE + 9);
    g.pl().aim = Vec2((float)bdx, (float)bdy);
    g.pl().face = bdx > 0 ? 2 : bdx < 0 ? 3 : bdy > 0 ? 0 : 1;
    return true;
  }
  if (op == "hp") { Actor& p = g.pl(); p.hp = std::max(1.0f, p.maxHp * (float)std::atof(c.arg(2).c_str()) / 100.0f); return true; }
  if (op == "water") {
    const int n = std::max(1, std::atoi(c.arg(2).c_str()));
    const int dx0 = c.arg(3).empty() ? 2 : std::atoi(c.arg(3).c_str());
    const Actor& p = g.pl();
    const bool horiz = std::fabs(p.aim.x) >= std::fabs(p.aim.y);
    const int sx = horiz ? (p.aim.x >= 0 ? 1 : -1) : 0, sy = horiz ? 0 : (p.aim.y >= 0 ? 1 : -1);
    const int hx = (int)std::floor(p.p.x / TILE), hy = (int)std::floor((p.p.y - 2) / TILE);
    Map& m = g.map();
    for (int i = 0; i < n; i++)
      for (int j = -2; j <= 2; j++) {   // (a channel five tiles wide across the way)
        const int x = hx + sx * (dx0 + i) + (horiz ? 0 : j), y = hy + sy * (dx0 + i) + (horiz ? j : 0);
        if (m.in(x, y)) { m.setG(x, y, Ground::Water); m.setP(x, y, 0); }
      }
    m.rebuildSolid();
    return true;
  }
  return false;
}

bool cmdSpell(ScriptCtx& c) {
  Game& g = c.game;
  const std::string op = c.arg(1);
  if (op == "learn") { const uint32_t id = spellWord(g, c.rest(2)); if (!id) c.fail("spell: no spell called " + c.rest(2)); else MagicOps::learn(g, id, false); return true; }
  if (op == "slot") {
    const int i = std::atoi(c.arg(2).c_str()) - 1;
    const uint32_t id = spellWord(g, c.rest(3));
    if (i < 0 || i >= magic::QUICK_SLOTS || !id || !g.arcana.book.knows(id)) { c.fail("spell slot: <1..4> <a known spell>"); return true; }
    for (uint32_t& s : g.arcana.book.slot) if (s == id) s = 0;
    g.arcana.book.slot[i] = id;
    MagicOps::syncLegacy(g);
    return true;
  }
  if (op == "cast") { stageCast(c, slotArg(g, c.arg(2)), 0, false); return true; }
  if (op == "upcast") { stageCast(c, slotArg(g, c.arg(2)), std::atoi(c.arg(3).c_str()), squashSp(c.arg(4)) == "dual"); return true; }
  if (op == "dual") { stageCast(c, slotArg(g, c.arg(2)), 0, true); return true; }
  if (op == "charge") {
    const int slot = slotArg(g, c.arg(2));
    if (slot < 0) { c.fail("spell charge: that quick slot is empty"); return true; }
    readyHero(g);
    g.arcana.live.cd[slot] = 0;
    MagicOps::begin(g, slot, true, false);
    const magic::SpellDef* d = magic::spellById(g.arcana.book.slot[slot]);
    const int lv = std::max(0, std::min(3, std::atoi(c.arg(3).c_str())));
    if (d && g.arcana.live.cast.spell) g.arcana.live.cast.t = d->castTime + 0.45f * (float)lv - 0.01f;   // (the next steps pour the levels in)
    return true;
  }
  if (op == "release") { g.arcana.live.cast.held = false; return true; }
  if (op == "mp") { g.mp = (float)std::atof(c.arg(2).c_str()); if (g.maxMp < g.mp) g.maxMp = g.mp; return true; }
  if (op == "aim") {
    const Vec2 a = norm(Vec2((float)std::atof(c.arg(2).c_str()), (float)std::atof(c.arg(3).c_str())) + Vec2(0.0001f, 0));
    g.pl().aim = a;
    g.pl().face = std::fabs(a.x) > std::fabs(a.y) ? (a.x > 0 ? 2 : 3) : (a.y > 0 ? 0 : 1);
    return true;
  }
  if (op == "rain") { MagicOps::live(g).rainOverride = onOff(c.arg(2)); return true; }
  if (op == "moon") { MagicOps::live(g).moonOverride = onOff(c.arg(2)); return true; }
  if (op == "land") {
    const std::string w = squashSp(c.arg(2));
    if (w == "off" || w.empty()) { MagicOps::setLand(nullptr); return true; }
    for (int a = 0; a < (int)cult::Archetype::COUNT; a++)
      if (squashSp(cult::archetypeName((cult::Archetype)a)) == w) {
        const uint32_t seed = c.arg(3).empty() ? 11u : (uint32_t)std::strtoul(c.arg(3).c_str(), nullptr, 10);
        cult::Culture cu = cult::Atlas::make((cult::Archetype)a, seed);
        if (!cu.id) cu.id = cult::familyId(a * 7 + 3, (int32_t)(seed % 1000u));
        MagicOps::setLand(&cu);
        return true;
      }
    c.fail("spell land: no archetype called " + c.arg(2));
    return true;
  }
  if (op == "attune") {
    const magic::Tradition* t = landTradition(g);
    std::string why;
    if (!t || !t->id) c.fail("spell attune: this land teaches Common Arcana");
    else if (!MagicOps::attuned(g, t->id) && !MagicOps::attune(g, *t, &why)) c.fail("spell attune: " + why);
    return true;
  }
  if (op == "trad") {
    const magic::Tradition* t = landTradition(g);
    const int k = squashSp(c.arg(2)) == "last" && t ? (int)t->spells.size() - 1 : std::atoi(c.arg(2).c_str()) - 1;
    if (!t || !t->id || k < 0 || k >= (int)t->spells.size()) { c.fail("spell trad: no such tradition spell here"); return true; }
    if (!MagicOps::attuned(g, t->id)) MagicOps::attune(g, *t);
    const uint32_t sid = t->spells[(size_t)k];
    MagicOps::learn(g, sid, false);
    for (uint32_t& s : g.arcana.book.slot) if (s == sid) s = 0;
    g.arcana.book.slot[0] = sid;
    g.arcana.book.sel = 0;
    MagicOps::syncLegacy(g);
    return true;
  }
  if (op == "teacher") {
    const std::string w = squashSp(c.arg(2));
    const Role r = w == "priest" ? Role::Priest : Role::Mage;
    const Vec2 at = g.pl().p + Vec2(g.pl().face == 3 ? -18.0f : 18.0f, 0);
    const int id = MagicOps::spawnTeacher(g, at, r, w == "scholar" ? 11 : 7);
    const int k = MagicOps::findActor(g, id);
    if (k < 0) { c.fail("spell teacher: no teacher"); return true; }
    MagicOps::talkTo(g, g.actors[(size_t)k]);
    return true;
  }
  if (op == "choose") {
    const std::string w = squashSp(c.arg(2));
    const int kind = w == "attune" ? 1 : w == "tradition" ? 2 : 0;
    const int i = dlgPick(g, kind);
    if (i < 0 || g.mode != Mode::Dialogue) {
      std::string all;
      for (const DlgOpt& o : g.dlg.opts) all += " [" + o.label + "]";
      c.fail("spell choose: no " + c.arg(2) + " option among" + all);
      return true;
    }
    const DlgOpt o = g.dlg.opts[(size_t)i];
    MagicOps::choose(g, o);
    return true;
  }
  if (op == "pool") {
    const std::string w = squashSp(c.arg(2));
    if (w == "full") {   // every pool to its cap (heat just short of overheating)
      for (int r = 0; r < (int)magic::Resource::COUNT; r++) g.arcana.book.res[r] = magic::resourceCap((magic::Resource)r) - (r == (int)magic::Resource::Heat ? 1.0f : 0.0f);
      return true;
    }
    const magic::Resource r = w == "heat" ? magic::Resource::Heat : w == "charge" ? magic::Resource::Charge : magic::Resource::Starlight;
    g.arcana.book.res[(int)r] = (float)std::atof(c.arg(3).c_str());
    return true;
  }
  if (cmdHidden(c, op)) return true;   // (M10) hidden | steps | trial | plate | priest | hp | water
  c.fail("spell: learn | slot | cast | upcast | dual | charge | release | mp | aim | rain | moon | land | attune | trad | teacher | choose | pool | hidden | steps | trial | plate | priest | hp | water");
  return true;
}
EMB_SCRIPT_CMD("spell", "spell learn|slot|cast|upcast|dual|charge|release|mp|aim|rain|moon|land|attune|trad|teacher|choose|pool ...: stage the spells (M9)", cmdSpell);

bool cmdSurface(ScriptCtx& c) {
  magic::Surface s;
  if (!surfaceWord(c.arg(1), s)) { c.fail("surface: no surface called " + c.arg(1)); return true; }
  Game& g = c.game;
  const int tx = (int)std::floor(g.pl().p.x / TILE) + std::atoi(c.arg(2).c_str());
  const int ty = (int)std::floor((g.pl().p.y - 2) / TILE) + std::atoi(c.arg(3).c_str());
  const float life = c.arg(4).empty() ? 20.0f : (float)std::atof(c.arg(4).c_str());
  MagicOps::laySurface(g, tx, ty, s, life, 1, g.pl().id);
  return true;
}
EMB_SCRIPT_CMD("surface", "surface <kind> <dx> <dy> [secs]: lay a spell surface near the hero (M9)", cmdSurface);

bool cmdStatus(ScriptCtx& c) {
  magic::Status s;
  if (!statusWord(c.arg(1), s)) { c.fail("status: no status called " + c.arg(1)); return true; }
  Game& g = c.game;
  int best = -1;
  float bd = 1e18f;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (!a.hostile || a.st == AState::Dead) continue;
    const float d = len2(a.p - g.pl().p);
    if (d < bd) { bd = d; best = (int)i; }
  }
  if (best < 0) { c.fail("status: no foe near"); return true; }
  MagicOps::applyStatus(g, g.actors[(size_t)best], s, (float)std::atof(c.arg(2).c_str()), g.pl().id);
  return true;
}
EMB_SCRIPT_CMD("status", "status <name> <secs>: put a status on the nearest foe (M9)", cmdStatus);

bool expSpell(ScriptCtx& c) {
  const uint32_t id = spellWord(c.game, c.rest(2));
  if (!id || !c.game.arcana.book.knows(id)) c.fail("expect spell: " + c.rest(2) + " is not known");
  return true;
}
EMB_SCRIPT_CMD("expect:spell", "expect spell <spell>: it is known (M9)", expSpell);

bool expStatus(ScriptCtx& c) {
  magic::Status s;
  if (!statusWord(c.arg(2), s)) { c.fail("expect status: no status called " + c.arg(2)); return true; }
  const int want = c.arg(3).empty() ? 1 : std::atoi(c.arg(3).c_str());
  int n = 0;
  for (const Actor& a : c.game.actors) n += a.st != AState::Dead && a.hasStatus(s);
  if (n < want) c.fail("expect status: " + std::to_string(n) + " " + magic::statusName(s) + ", wanted " + std::to_string(want));
  return true;
}
EMB_SCRIPT_CMD("expect:status", "expect status <name> [n]: at least n actors carry it (M9)", expStatus);

bool expSurface(ScriptCtx& c) {
  magic::Surface s;
  if (!surfaceWord(c.arg(2), s)) { c.fail("expect surface: no surface called " + c.arg(2)); return true; }
  const int want = c.arg(3).empty() ? 1 : std::atoi(c.arg(3).c_str());
  int n = 0;
  for (const magic::SurfaceTile& t : c.game.arcana.live.field.tiles) n += t.kind == s;
  if (n < want) c.fail("expect surface: " + std::to_string(n) + " tiles of " + magic::surfaceName(s) + ", wanted " + std::to_string(want));
  return true;
}
EMB_SCRIPT_CMD("expect:surface", "expect surface <kind> [n]: at least n tiles of it (M9)", expSurface);

bool expSummon(ScriptCtx& c) {
  const int want = c.arg(2).empty() ? 1 : std::atoi(c.arg(2).c_str());
  int n = 0;
  for (const Actor& a : c.game.actors)
    n += a.st != AState::Dead && (a.magicTag == magic::MT_SUMMON || a.magicTag == magic::MT_CHARM || a.magicTag == magic::MT_BOUND);
  if (n < want) c.fail("expect summon: " + std::to_string(n) + " fight for the hero, wanted " + std::to_string(want));
  return true;
}
EMB_SCRIPT_CMD("expect:summon", "expect summon [n]: at least n summoned / charmed / bound bodies fight for the hero (M9)", expSummon);

bool expCount(ScriptCtx& c, int have, const char* what) {
  const int want = c.arg(2).empty() ? 1 : std::atoi(c.arg(2).c_str());
  if (have < want) c.fail(std::string("expect ") + what + ": " + std::to_string(have) + ", wanted " + std::to_string(want));
  return true;
}
bool expCombo(ScriptCtx& c) { return expCount(c, c.game.arcana.live.stats.combos, "combo"); }
EMB_SCRIPT_CMD("expect:combo", "expect combo [n]: at least n surface combos went off (M9)", expCombo);
bool expCast(ScriptCtx& c) { return expCount(c, c.game.arcana.live.stats.casts, "cast"); }
EMB_SCRIPT_CMD("expect:cast", "expect cast [n]: at least n spells were cast (M9)", expCast);
bool expAttuned(ScriptCtx& c) { return expCount(c, (int)c.game.arcana.book.attuned.size(), "attuned"); }
EMB_SCRIPT_CMD("expect:attuned", "expect attuned [n]: at least n traditions attuned (M9)", expAttuned);

bool expArcana(ScriptCtx& c) {
  const magic::Stats& S = c.game.arcana.live.stats;
  const std::string w = squashSp(c.arg(2));
  int have = -1;
  if (w == "notes") have = S.notes; else if (w == "refrains") have = S.refrains; else if (w == "runetriggers") have = S.runeTriggers;
  else if (w == "runecombos") have = S.runeCombos; else if (w == "maskstrikes") have = S.maskStrikes; else if (w == "maskhits") have = S.maskHits;
  else if (w == "growths") have = S.growths; else if (w == "reverts") have = S.reverts; else if (w == "snares") have = S.snares;
  else if (w == "decoys") have = S.decoys; else if (w == "decoyhits") have = S.decoyHits; else if (w == "slowed") have = S.slowedSteps;
  else if (w == "bubbles") have = S.bubbles; else if (w == "songfires") have = S.songFires; else if (w == "gates") have = S.gates;
  else if (w == "blood") have = S.bloodPaid; else if (w == "rites") have = (int)c.game.arcana.book.rites.size();
  else if (w == "mask") have = c.game.arcana.live.mask >= 0 ? 1 : 0;
  else if (w == "trial") have = trialNow(c.game) ? 1 : 0;   // (fixer M10 r1) standing in a trial
  else if (w == "open") { have = 0; for (uint32_t b = c.game.arcana.live.trial.gatesOpen; b; b &= b - 1) have++; }   // its gates open now
  else if (w == "free") { const Actor& h = c.game.pl(); const int tx = (int)std::floor(h.p.x / TILE), ty = (int)std::floor((h.p.y - 2) / TILE); have = c.game.map().blocked(tx, ty) ? 0 : 1; }   // the hero on a free tile
  if (have < 0) { c.fail("expect arcana: no stat called " + c.arg(2)); return true; }
  const int want = c.arg(3).empty() ? 1 : std::atoi(c.arg(3).c_str());
  if (have < want) c.fail("expect arcana " + c.arg(2) + ": " + std::to_string(have) + ", wanted " + std::to_string(want));
  return true;
}
EMB_SCRIPT_CMD("expect:arcana", "expect arcana <stat> [n]: the hidden signatures' counters (M10)", expArcana);

bool expConc(ScriptCtx& c) {
  const uint32_t held = c.game.arcana.live.conc.spell;
  if (squashSp(c.arg(2)) == "none") { if (held) c.fail("expect conc: a concentration is held"); return true; }
  const uint32_t id = spellWord(c.game, c.rest(2));
  if (!id || held != id) c.fail("expect conc: " + c.rest(2) + " is not held");
  return true;
}
EMB_SCRIPT_CMD("expect:conc", "expect conc <spell|none>: the concentration held (M9)", expConc);
}  // namespace
