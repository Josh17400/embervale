// M9 script commands (SPELLFX lane): stage every spell's life on screen without waiting for the sim's spell lists
// (VISION_PLAN 15.29.7). Events go through the same View::magicEvent the sim's Ev::Spell reaches; missiles fly through
// the sim as ProjKind::Spell (a synthetic spell's end is given by the view). Commands never touch a save.
//   fx spell <spell|form essence school> [size S] [mag M] [upcast N] [dual] [at dx dy]
//                      the full life: WindUp at the hero's hands, a Charge per upcast level, the Launch, then the form's
//                      own (a missile, a beam's Channel ticks and Release, a nova / storm / meteor Impact, a Summon, a
//                      Ward, a Blink, a heal) and its Aftermath. spell: a school spell's name without spaces (firebolt,
//                      frostlance, spark, mend, stoneward, spiritwolf, fear, oakflesh) or a Form Essence School triple
//                      (meteor fire destruction): any combination, size 1..4 (4 a spectacle). at: the target dx dy
//                      tiles from the hero (default: the nearest foe in front, else 5 tiles ahead)
//   fx phase <phase> <spell|form essence school> [dx dy] [mag M]   one phase at a point (windup, charge, launch,
//                      impact, aftermath, fizzle, channel, release, summon, ward, blink, reaction)
//   fx combo <surface> [dx dy]     a surface combo's burst (steam, shock, fire, ice, water, poison, blood, oil)
//   fx surface <kind> <w> <h> [dx dy] [secs]   an organic pool of a surface, w x h tiles round dx dy from the hero
//   fx status <name> [secs] [all|hero]          a status on the nearest foe (all: every foe within 12 tiles; hero)
//   fx clear                       the magic's effects, decals and every surface tile gone
//   fx reset                       the peak counters and the draw-time record start again
//   fx report                      print the magic's counters and draw times (stdout)
//   expect magicfx <field> <min>   last frame's count: effects particles lights decals surfaces missiles statuses;
//                                  peak_<field>: the most since the last reset; ms_worst / ms_avg <max>: the magic's
//                                  draw time (ms) must stay at or under max
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/view/magic_state.h"
#include "rpg/view/script_api.h"

namespace {
std::string low(const std::string& s) {
  std::string r;
  for (char ch : s) {
    if (ch == ' ' || ch == '_' || ch == '-' || ch == '\'') continue;
    r += (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
  }
  return r;
}
template <class E, class F> bool enumWord(const std::string& w, int count, F name, E& out) {
  for (int i = 0; i < count; i++) if (low(name((E)i)) == low(w)) { out = (E)i; return true; }
  return false;
}
// the spell words at a[i]: a school spell's name (one word) or a Form Essence School triple; returns the words used
int spellAt(ScriptCtx& c, size_t i, int size, uint32_t& id) {
  const std::string w = c.arg(i);
  for (const magic::SpellDef& s : magic::schoolSpells()) if (low(s.name) == low(w)) { id = s.id; return 1; }
  magic::Form f;
  magic::Essence e;
  magic::School sc;
  if (enumWord(w, (int)magic::Form::COUNT, magic::formName, f) && enumWord(c.arg(i + 1), (int)magic::Essence::COUNT, magic::essenceName, e) &&
      enumWord(c.arg(i + 2), magic::SCHOOLS, magic::schoolName, sc)) {
    id = mfx::makeSynth(f, e, sc, size, std::string(magic::essenceName(e)) + " " + magic::formName(f));
    return 3;
  }
  id = 0;
  return 0;
}
int sizeArg(ScriptCtx& c) {
  for (size_t k = 1; k + 1 < c.a.size(); k++) if (c.a[k] == "size") return std::atoi(c.a[k + 1].c_str());
  return 1;
}
Event spellEv(magic::Phase ph, uint32_t id, Vec2 p, Vec2 q, float f, uint32_t who, const std::string& s = "") {
  Event e;
  e.type = Ev::Spell; e.a = (int)ph; e.spell = id; e.p = p; e.q = q; e.f = f; e.id = who; e.s = s;
  return e;
}
Vec2 targetOf(ScriptCtx& c, bool hasAt, int dx, int dy) {
  const Game& g = c.game;
  const Actor& p = g.pl();
  if (hasAt) return Vec2(p.p.x + (float)dx * TILE, p.p.y + (float)dy * TILE);
  float bd = 220.0f * 220.0f;
  Vec2 best = p.p + norm(p.aim) * (5.0f * TILE);
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (!a.hostile || a.st == AState::Dead) continue;
    const float d = len2(a.p - p.p);
    if (d < bd) { bd = d; best = a.p; }
  }
  return best;
}

bool cmdFx(ScriptCtx& c) {
  Game& g = c.game;
  View& v = c.view;
  View::MagicFxState& S = MagicFxOps::st(v);
  const std::string op = c.arg(1);
  const Actor& hero = g.pl();
  const uint32_t me = (uint32_t)hero.id;
  if (op == "spell") {
    const int size = std::clamp(sizeArg(c), 1, 4);
    uint32_t id = 0;
    const int used = spellAt(c, 2, size, id);
    if (!used) { c.fail("fx spell: no spell '" + c.rest(2) + "'"); return true; }
    int upcast = 0, dx = 0, dy = 0;
    bool dual = false, hasAt = false;
    float mag = -1;
    for (size_t k = 2 + (size_t)used; k < c.a.size(); k++) {
      const std::string w = low(c.a[k]);
      if (w == "dual") dual = true;
      else if (w == "upcast" && k + 1 < c.a.size()) upcast = std::clamp(std::atoi(c.a[++k].c_str()), 0, 3);
      else if (w == "mag" && k + 1 < c.a.size()) mag = (float)std::atof(c.a[++k].c_str());
      else if (w == "size" && k + 1 < c.a.size()) ++k;
      else if (w == "at" && k + 2 < c.a.size()) { hasAt = true; dx = std::atoi(c.a[k + 1].c_str()); dy = std::atoi(c.a[k + 2].c_str()); k += 2; }
    }
    const magic::SpellDef* d = mfx::defOf(id);
    if (!d) { c.fail("fx spell: no def"); return true; }
    const float M = mag > 0 ? mag : magic::magnitudeOf(upcast, dual);
    const Vec2 hands = hero.p + Vec2(0, -8);
    const Vec2 tgt = targetOf(c, hasAt, dx, dy);
    const Vec2 dir = norm(tgt + Vec2(0, -6) - hands);
    MagicFxOps::stage(v, 0, spellEv(magic::Phase::WindUp, id, hands, hands, 1, me));
    for (int i = 1; i <= upcast; i++) MagicFxOps::stage(v, 0.28f * (float)i, spellEv(magic::Phase::Charge, id, hands, hands, magic::magnitudeOf(i, dual), me));
    const float tl = std::max(0.35f, d->castTime) + 0.28f * (float)upcast;
    MagicFxOps::stage(v, tl, spellEv(magic::Phase::Launch, id, hands, d->form == magic::Form::Meteor || d->form == magic::Form::Storm ? tgt : hands + dir * 24.0f, M, me));
    switch (d->form) {
      case magic::Form::Bolt: case magic::Form::Lance: case magic::Form::Orb: case magic::Form::Mark: {
        Projectile pr;
        pr.kind = ProjKind::Spell; pr.spell = id; pr.mag = M; pr.upcast = (uint8_t)upcast; pr.dual = dual;
        const float sp = std::max(120.0f, d->speed > 0 ? d->speed : 220.0f);
        pr.p = hands + dir * 6.0f; pr.v = dir * sp; pr.from = pr.p;
        pr.life = std::max(0.15f, len(tgt + Vec2(0, -6) - hands) / sp + 0.05f);
        pr.dmg = mfx::isSynth(id) ? 1.0f : d->power; pr.radius = 4; pr.fromPlayer = true; pr.owner = hero.id;
        MagicOps::syncLegacy(g);
        MagicFxOps::stageMissile(v, tl, pr);
        break;
      }
      case magic::Form::Beam:
        for (int i = 0; i < 10; i++) MagicFxOps::stage(v, tl + 0.1f * (float)i, spellEv(magic::Phase::Channel, id, hands, tgt + Vec2(0, -8), M, me));
        MagicFxOps::stage(v, tl + 1.05f, spellEv(magic::Phase::Release, id, hero.p, hero.p, M, me));
        break;
      case magic::Form::Nova: case magic::Form::Cone:
        MagicFxOps::stage(v, tl + 0.02f, spellEv(magic::Phase::Impact, id, hero.p, hero.p, M, me));
        MagicFxOps::stage(v, tl + 0.1f, spellEv(magic::Phase::Aftermath, id, hero.p + Vec2(0, 4), hero.p, M, 0));
        break;
      case magic::Form::Storm: case magic::Form::Meteor: case magic::Form::Rune: case magic::Form::Wall: case magic::Form::Totem:
        MagicFxOps::stage(v, tl + 0.05f, spellEv(magic::Phase::Impact, id, tgt, tgt, M, me));
        if (d->form != magic::Form::Meteor) MagicFxOps::stage(v, tl + 0.12f, spellEv(magic::Phase::Aftermath, id, tgt + Vec2(0, 4), tgt, M, 0));
        break;
      case magic::Form::Mend:
        MagicFxOps::stage(v, tl + 0.02f, spellEv(magic::Phase::Impact, id, hero.p, hero.p, M, me));
        break;
      case magic::Form::Summon:
        MagicFxOps::stage(v, tl + 0.02f, spellEv(magic::Phase::Summon, id, hasAt ? tgt : hero.p + dir * 28.0f, hero.p, M, 0));
        break;
      case magic::Form::Ward: case magic::Form::Aura:
        MagicFxOps::stage(v, tl + 0.02f, spellEv(magic::Phase::Ward, id, hero.p, hero.p, M, me));
        break;
      case magic::Form::Blink:
        MagicFxOps::stage(v, tl + 0.02f, spellEv(magic::Phase::Blink, id, hero.p, tgt, M, me));
        break;
      default: break;
    }
    return true;
  }
  if (op == "phase") {
    static const char* names[] = {"windup", "charge", "launch", "impact", "aftermath", "fizzle", "channel", "release", "combo", "statuson", "summon", "ward", "blink", "reaction"};
    int ph = -1;
    for (int i = 0; i < (int)magic::Phase::COUNT; i++) if (low(c.arg(2)) == names[i]) ph = i;
    if (ph < 0) { c.fail("fx phase: no phase '" + c.arg(2) + "'"); return true; }
    uint32_t id = 0;
    const int used = spellAt(c, 3, std::clamp(sizeArg(c), 1, 4), id);
    if (!used) { c.fail("fx phase: no spell '" + c.rest(3) + "'"); return true; }
    const size_t k = 3 + (size_t)used;
    const bool hasAt = !c.arg(k).empty() && c.arg(k) != "mag" && c.arg(k) != "size";
    const Vec2 at = hasAt ? hero.p + Vec2((float)std::atoi(c.arg(k).c_str()) * TILE, (float)std::atoi(c.arg(k + 1).c_str()) * TILE) : hero.p + Vec2(0, -8);
    float M = 1;
    for (size_t j = k; j + 1 < c.a.size(); j++) if (c.a[j] == "mag") M = (float)std::atof(c.a[j + 1].c_str());
    MagicFxOps::stage(v, 0, spellEv((magic::Phase)ph, id, at, hero.p + Vec2(0, -8), M, ph <= 2 ? me : 0));
    return true;
  }
  if (op == "combo") {
    const Vec2 at = hero.p + Vec2((float)std::atoi(c.arg(3).c_str()) * TILE, (float)std::atoi(c.arg(4).c_str()) * TILE);
    std::string name;
    for (int s = 1; s < (int)magic::Surface::COUNT; s++)
      if (low(magic::surfaceName((magic::Surface)s)) == low(c.arg(2)) || (low(c.arg(2)) == "shock" && s == (int)magic::Surface::Shock)) name = magic::surfaceName((magic::Surface)s);
    if (name.empty()) { c.fail("fx combo: no surface '" + c.arg(2) + "'"); return true; }
    MagicFxOps::stage(v, 0, spellEv(magic::Phase::Combo, 0, at, at, 1, 0, name));
    return true;
  }
  if (op == "surface") {
    magic::Surface s = magic::Surface::None;
    for (int k = 1; k < (int)magic::Surface::COUNT; k++)
      if (low(magic::surfaceName((magic::Surface)k)) == low(c.arg(2)) || (low(c.arg(2)) == "shock" && k == (int)magic::Surface::Shock)) s = (magic::Surface)k;
    if (s == magic::Surface::None) { c.fail("fx surface: no surface '" + c.arg(2) + "'"); return true; }
    const int w = std::max(1, std::atoi(c.arg(3).c_str())), h = std::max(1, std::atoi(c.arg(4).c_str()));
    const int cx = (int)std::floor(hero.p.x / TILE) + std::atoi(c.arg(5).c_str()), cy = (int)std::floor((hero.p.y - 2) / TILE) + std::atoi(c.arg(6).c_str());
    const float secs = c.arg(7).empty() ? 30.0f : (float)std::atof(c.arg(7).c_str());
    for (int j = 0; j < h; j++)
      for (int i = 0; i < w; i++) {
        // an organic pool: the ellipse in the w x h box, its edge ragged by a hash
        const float u = ((float)i + 0.5f) / (float)w * 2 - 1, q = ((float)j + 0.5f) / (float)h * 2 - 1;
        const float d = u * u + q * q;
        const float rag = 0.75f + 0.5f * mfx::hf(hash2(cx + i, cy + j, 0xF00Du));
        if (w > 2 && h > 2 && d > rag) continue;
        MagicOps::laySurface(g, cx - w / 2 + i, cy - h / 2 + j, s, secs, 2, hero.id);
      }
    return true;
  }
  if (op == "status") {
    magic::Status st;
    if (!enumWord(c.arg(2), magic::STATUSES, magic::statusName, st)) { c.fail("fx status: no status '" + c.arg(2) + "'"); return true; }
    const float secs = c.arg(3).empty() ? 20.0f : (float)std::atof(c.arg(3).c_str());
    const std::string who = low(c.arg(4));
    if (who == "hero") { MagicOps::applyStatus(g, g.pl(), st, secs, -1); return true; }
    int best = -1;
    float bd = 1e18f;
    for (size_t i = 1; i < g.actors.size(); i++) {
      Actor& a = g.actors[i];
      if (!a.hostile || a.st == AState::Dead) continue;
      const float d = len2(a.p - hero.p);
      if (who == "all") { if (d < (12.0f * TILE) * (12.0f * TILE)) MagicOps::applyStatus(g, a, st, secs, hero.id); continue; }
      if (d < bd) { bd = d; best = (int)i; }
    }
    if (who != "all") {
      if (best < 0) { c.fail("fx status: no foe near"); return true; }
      MagicOps::applyStatus(g, g.actors[(size_t)best], st, secs, hero.id);
    }
    return true;
  }
  if (op == "clear") { S.w.clear(); S.staged.clear(); g.arcana.live.field.clear(); return true; }
  if (op == "reset") { MagicFxOps::resetPeaks(v); return true; }
  if (op == "report") {
    const View::MagicFxStats& L = S.last;
    std::printf("magicfx: last frame effects %d particles %d lights %d decals %d surfaces %d missiles %d statuses %d ms %.3f | "
                "peak effects %d particles %d lights %d surfaces %d | draw ms worst %.3f avg %.3f over %d busy frames | update ms worst %.3f | dropped %d\n",
                L.effects, L.particles, L.lights, L.decals, L.surfaceTiles, L.missiles, L.statusOverlays, L.ms, S.peak.effects, S.peak.particles,
                S.peak.lights, S.peak.surfaceTiles, S.msWorst, S.msFrames ? S.msSum / S.msFrames : 0.0, S.msFrames, S.updWorst, S.w.dropped);
    std::fflush(stdout);
    return true;
  }
  c.fail("fx: spell | phase | combo | surface | status | clear | reset | report");
  return true;
}
EMB_SCRIPT_CMD("fx", "fx spell|phase|combo|surface|status|clear|reset|report ...: stage the magic's look (M9 SPELLFX)", cmdFx);

bool expMagicFx(ScriptCtx& c) {
  View::MagicFxState& S = MagicFxOps::st(c.view);
  const std::string f = low(c.arg(2));
  const double want = std::atof(c.arg(3).c_str());
  if (f == "msworst" || f == "msavg") {
    const double v = f == "msworst" ? S.msWorst : (S.msFrames ? S.msSum / S.msFrames : 0.0);
    if (v > want) c.fail("expect magicfx: " + c.arg(2) + " " + std::to_string(v) + " ms, over " + c.arg(3));
    return true;
  }
  const bool peak = f.rfind("peak", 0) == 0;
  const std::string name = peak ? f.substr(4) : f;
  const View::MagicFxStats& L = peak ? S.peak : S.last;
  double v = -1;
  if (name == "effects") v = std::max(L.effects, peak ? 0 : 0);
  else if (name == "particles") v = L.particles;
  else if (name == "lights") v = L.lights;
  else if (name == "decals") v = L.decals;
  else if (name == "surfaces" || name == "surfacetiles") v = L.surfaceTiles;
  else if (name == "missiles") v = L.missiles;
  else if (name == "statuses" || name == "statusoverlays") v = L.statusOverlays;
  if (v < 0) { c.fail("expect magicfx: no field '" + c.arg(2) + "'"); return true; }
  if (v < want) c.fail("expect magicfx: " + c.arg(2) + " " + std::to_string((int)v) + ", wanted " + c.arg(3));
  return true;
}
EMB_SCRIPT_CMD("expect:magicfx", "expect magicfx <field> <min> | ms_worst|ms_avg <max>: the magic's counts / draw time (M9)", expMagicFx);
}  // namespace
