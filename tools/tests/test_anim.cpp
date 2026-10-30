// rpg_test --anim (P2a Clear Sight, VISION_PLAN 15.36 item 11: the lead's frozen harness; a lane reports a harness
// problem instead of editing it). The animation pick (rpg/sim/anim.h) and the timing the view and the sim share.
//   rpg_test --anim [--seeds A..B] [--gates] [--write]
// Always (phase A, CI): A1, the v1 frame pick at rig 1 against a frozen copy of render.cpp's pre-P2a switch over every
// state x time x combo, and the golden hash tests/fixtures/golden_anim_v1.txt (--write records it).
// --gates (the P2a DONE gates the ANIM lane works to; FAIL until it lands):
//   A2  9 distinct swing effects by family (animSwingFx at rig 1 and 2, their sprites pairwise different); the hero's
//       effect ends with the strike state within one frame (dagger pace 0.8, mace 1.25: sim steps)
//   A3  enemy windups by family measured in the sim (dagger < unarmed < sword < spear = staff < axe < mace < greatsword,
//       in the plan's ratios within 5 %), each equal to animEnemyWindup within 1.5 frames; the spawn-weighted mean over
//       the bandits the sim makes stays today's 0.36 +- 0.01 s (measured in phase A: the bandits spawn with Actor::windup
//       0.36; the plan's 0.30 read ai.cpp's folk branch, the guards' and militia's swing)
//   A4  rig 2: a killed bandit shows Lying 0.45 s or less after the killing blow; lies animCorpseLife(2) = 60 s (gone by
//       60.2); at most animCorpseCap(2) = 24 at once with 30 killed; the hero walks through a corpse
//   A5  rig 2: the hero's fall plays under the death screen (Fall, then Lying by 0.45 s of Mode::Dead)
//   A6  (P2c FOLK-SIM-C, added) rig 2: the production pick pieces per weapon class: carry, drawn, the threat's draw and
//       turn, the block flash, the crossbow's states, the roll's bash, the heavy finisher's impact (checkA6 below)
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/sim/anim.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/skill_ops.h"
#include "tools/tests/tests.h"

namespace {

int g_fail = 0;

// ---- A1: render.cpp's frame pick before P2a, frozen here (never edit: it IS the reference)
int v1Reference(const Actor& a) {
  int fr = 0;
  switch (a.st) {
    case AState::Walk: fr = 1 + (int)(a.animT * 9) % 4; break;
    case AState::Roll: fr = 1 + (int)(a.stT * 20) % 4; break;
    case AState::Windup: case AState::Cast: fr = 5; break;
    case AState::Strike: fr = 6; break;
    case AState::Recover: fr = 6; break;
    case AState::Hurt: case AState::Dead: fr = 7; break;
    default: fr = 0; break;
  }
  return fr;
}

std::string goldenPath() { return std::string(EMB_SOURCE_DIR) + "/tests/fixtures/golden_anim_v1.txt"; }

int checkA1(bool write) {
  const int f0 = g_fail;
  Game g(1);
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](uint64_t v) { h ^= v; h *= 1099511628211ull; };
  int n = 0;
  for (int st = 0; st <= (int)AState::Down; st++)
    for (int combo = 0; combo < 3; combo++)
      for (int k = 0; k <= 720; k++) {
        Actor a;
        a.human = true;
        a.st = (AState)st;
        a.combo = combo;
        a.animT = a.stT = (float)k / 256.0f;   // (exact binary fractions: the golden is the same on MSVC and clang)
        a.look.weapon = (uint8_t)(k % 13);
        const AnimPick p = animPick(g, a, 1);
        const int want = v1Reference(a);
        if (p.frame != want) {
          if (g_fail - f0 < 5) std::printf("FAIL: A1 state %d t %.4f combo %d: animPick frame %d, v1 %d\n", st, a.stT, combo, p.frame, want);
          g_fail++;
        }
        mix((uint64_t)st); mix((uint64_t)combo); mix((uint64_t)k); mix(p.frame); mix((uint64_t)p.clip); mix(p.variant); mix(p.frames);
        n++;
      }
  char hx[24];
  std::snprintf(hx, sizeof hx, "%016llx", (unsigned long long)h);
  if (write) {
    if (FILE* f = std::fopen(goldenPath().c_str(), "wb")) {
      std::fprintf(f, "# P2a phase A: rpg_test --anim --write (the v1 frame pick at rig 1 over %d samples; never re-recorded by a lane)\n%s\n", n, hx);
      std::fclose(f);
      std::printf("anim: golden written: %s\n", hx);
    }
  } else {
    std::string want;
    if (FILE* f = std::fopen(goldenPath().c_str(), "rb")) {
      char line[256];
      while (std::fgets(line, sizeof line, f))
        if (line[0] != '#') { want = line; break; }
      std::fclose(f);
    }
    while (!want.empty() && (want.back() == '\n' || want.back() == '\r')) want.pop_back();
    if (want.empty()) { std::printf("FAIL: A1 no golden %s (rpg_test --anim --write)\n", goldenPath().c_str()); g_fail++; }
    else if (want != hx) { std::printf("FAIL: A1 golden %s, now %s\n", want.c_str(), hx); g_fail++; }
  }
  std::printf("A1 v1 frame pick at rig 1: %d samples, %s (%s)\n", n, g_fail == f0 ? "identical" : "DIFFERS", hx);
  return g_fail - f0;
}

// ---- the sim gates' stage: a fresh endless game, the hero alone on open ground, nothing wild
uint8_t weaponOf(art::Family f) {
  switch (f) {
    case art::Family::Dagger: return 5;
    case art::Family::Sword: return 1;
    case art::Family::Greatsword: return art::WEAPON_GREATSWORD;
    case art::Family::Axe: return 2;
    case art::Family::Mace: return 6;
    case art::Family::Spear: return 8;
    case art::Family::Staff: return 4;
    case art::Family::Bow: return 3;
    default: return 0;
  }
}
void stage(Game& g, uint64_t seed) {
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.hour = 12;
  for (int f = 0; f < 120; f++) { g.update(SIM_DT, Input()); g.events.clear(); g.mode = Mode::Play; g.actors.resize(1); }
  g.actors.resize(1);
}
void tick(Game& g, const Input& in) { g.update(SIM_DT, in); g.events.clear(); }
void arm(Game& g, WeaponType t) {
  Rng r(91u + (uint32_t)t);
  g.inv.clear();
  g.eqWeapon = g.eqBow = g.eqStaff = g.eqArmor = g.eqHelmet = g.eqShield = g.eqRing = g.eqAmulet = g.eqGloves = g.eqBoots = g.eqCloak = -1;
  g.inv.push_back(gear::makeGear(r, ItemKind::Weapon, (int)t, 5, Rarity::Common, 0, Mat::Iron));
  g.eqWeapon = 0;
  AnimOps::refreshHero(g);
}

int checkA2(uint64_t seed) {
  const int f0 = g_fail;
  // the 9 effects
  {
    Game g(1);
    art::RigScope keep;   // (P2c flip) the rig before A2 comes back after it (it was "back to rig 1")
    for (int rv = 1; rv <= 2; rv++) {
      art::setRigVersion(rv);
      std::set<int> fx;
      std::vector<Canvas> sp;
      for (int f = 0; f < art::FAMILY_COUNT; f++) {
        Actor a;
        a.human = true; a.st = AState::Strike; a.stT = 0.02f; a.face = 2;
        a.look.weapon = weaponOf((art::Family)f);
        const art::Fx x = animSwingFx(g, a);
        if (fx.insert((int)x).second) sp.push_back(art::fxSprite(x));
      }
      int same = 0;
      for (size_t i = 0; i < sp.size(); i++)
        for (size_t j = i + 1; j < sp.size(); j++) same += sp[i].w == sp[j].w && sp[i].h == sp[j].h && sp[i].px == sp[j].px;
      if (fx.size() != (size_t)art::FAMILY_COUNT || same)
        std::printf("FAIL: A2 rig %d: %zu distinct swing effects for %d families (%d identical sprite pairs)\n", rv, fx.size(), art::FAMILY_COUNT, same), g_fail++;
    }
  }
  // the effect ends with the strike (the hero, sim steps)
  for (WeaponType t : {WeaponType::Dagger, WeaponType::Mace, WeaponType::Sword}) {
    Game g(seed);
    stage(g, seed);
    arm(g, t);
    Input in;
    in.attack = true;
    // the frame the effect reaches its end (progress 1) while the strike lasts; if the strike ends first, where its
    // progress rate would have reached 1 (an effect too slow for the family is late, not "done at the strike's end")
    int strikeOn = -1, strikeOff = -1;
    float fxDone = -1, prFirst = -1, prLast = -1;
    int fFirst = -1, fLast = -1;
    for (int f = 0; f < 120 && strikeOff < 0; f++) {
      tick(g, in);
      in.attack = false;
      const Actor& p = g.pl();
      if (p.st == AState::Strike && strikeOn < 0) strikeOn = f;
      if (strikeOn >= 0 && p.st != AState::Strike && strikeOff < 0) { strikeOff = f; break; }
      const float pr = animSwingProgress(g, p);
      if (p.st == AState::Strike && pr >= 0) {
        if (fFirst < 0) { fFirst = f; prFirst = pr; }
        fLast = f; prLast = pr;
        if (fxDone < 0 && pr >= 1.0f) fxDone = (float)f;
      }
    }
    if (strikeOn < 0 || strikeOff < 0) { std::printf("FAIL: A2 %s: the hero never struck\n", t == WeaponType::Dagger ? "dagger" : t == WeaponType::Mace ? "mace" : "sword"); g_fail++; continue; }
    if (fxDone < 0) {
      const float rate = fLast > fFirst ? (prLast - prFirst) / (float)(fLast - fFirst) : 0.0f;
      fxDone = rate > 1e-4f ? (float)fLast + (1.0f - prLast) / rate : 999.0f;
    }
    const float d = std::fabs(fxDone - (float)strikeOff);
    std::printf("A2 %s (pace %.2f): strike %d frames, the effect reaches its end at frame %.1f of it (off by %.1f)\n",
                t == WeaponType::Dagger ? "dagger" : t == WeaponType::Mace ? "mace" : "sword", AnimOps::heroPace(g), strikeOff - strikeOn,
                fxDone - (float)strikeOn, d);
    if (d > 1.0f) { std::printf("FAIL: A2 the effect ends %.1f frames from the strike state's end (1 or less)\n", d); g_fail++; }
  }
  return g_fail - f0;
}

// the windup a bandit holding family f shows in the sim (s), or -1
float measureWindup(uint64_t seed, art::Family f, float& api) {
  api = -1;
  Game g(seed);
  stage(g, seed);
  g.godMode = true;
  const int bi = AnimOps::spawnBandit(g, g.pl().p + Vec2(14, 0), weaponOf(f));
  if (bi < 0) return -1;
  g.actors[(size_t)bi].ranged = false;
  api = animEnemyWindup(g.actors[(size_t)bi]);   // what the API says of this very bandit
  int on = -1;
  for (int k = 0; k < 60 * 8; k++) {
    tick(g, Input());
    g.mode = Mode::Play;
    if (bi >= (int)g.actors.size()) return -1;
    const Actor& b = g.actors[(size_t)bi];
    if (b.st == AState::Windup && on < 0) on = k;
    if (on >= 0 && b.st != AState::Windup) return (k - on) * SIM_DT;
  }
  return -1;
}

// today's spawn-weighted mean enemy windup (phase A, measured: every bandit spawns with Actor::windup 0.36 s)
constexpr double kTodayMeanWindup = 0.36;

int checkA3(uint64_t seed) {
  const int f0 = g_fail;
  static const float plan[art::FAMILY_COUNT] = {0.24f, 0.22f, 0.28f, 0.38f, 0.32f, 0.36f, 0.30f, 0.30f, 0.0f};   // by Family (bow: none)
  float w[art::FAMILY_COUNT] = {};
  for (int f = 0; f < art::FAMILY_COUNT; f++) {
    if (f == (int)art::Family::Bow) continue;
    float api = -1;
    w[f] = measureWindup(seed, (art::Family)f, api);
    std::printf("A3 %-10s windup %.3f s in the sim, animEnemyWindup %.3f\n", art::familyName((art::Family)f), w[f], api);
    if (w[f] < 0) { std::printf("FAIL: A3 %s: no windup measured\n", art::familyName((art::Family)f)); g_fail++; continue; }
    if (std::fabs(w[f] - api) > SIM_DT * 1.5f) { std::printf("FAIL: A3 %s: the sim winds up %.3f s, animEnemyWindup says %.3f\n", art::familyName((art::Family)f), w[f], api); g_fail++; }
  }
  if (w[(int)art::Family::Sword] > 0)
    for (int f = 0; f < art::FAMILY_COUNT; f++) {
      if (f == (int)art::Family::Bow || w[f] <= 0) continue;
      const float want = plan[f] / plan[(int)art::Family::Sword], have = w[f] / w[(int)art::Family::Sword];
      if (std::fabs(have - want) > 0.05f * want) { std::printf("FAIL: A3 %s: windup %.2f x the sword's, the plan %.2f\n", art::familyName((art::Family)f), have, want); g_fail++; }
    }
  // the spawn-weighted mean over the bandits the sim makes
  {
    Game g(seed);
    stage(g, seed);
    double sum = 0;
    int n = 0;
    std::map<int, int> fam;
    for (int k = 0; k < 300; k++) {
      const int bi = AnimOps::spawnBandit(g, g.pl().p + Vec2(400.0f + (float)(k % 20) * 8, (float)(k / 20) * 8), -1);
      if (bi < 0) continue;
      const Actor& b = g.actors[(size_t)bi];
      if (b.ranged || b.look.weapon == 3) continue;
      sum += animEnemyWindup(b);
      fam[(int)art::familyOf(b.look)]++;
      n++;
    }
    const double mean = n ? sum / n : 0;
    std::printf("A3 spawn-weighted mean windup over %d melee bandits: %.3f s (families:", n, mean);
    for (const auto& kv : fam) std::printf(" %s %d", art::familyName((art::Family)kv.first), kv.second);
    std::printf(")\n");
    if (std::fabs(mean - kTodayMeanWindup) > 0.01) { std::printf("FAIL: A3 the mean windup %.3f s is not today's %.2f +- 0.01\n", mean, kTodayMeanWindup); g_fail++; }
  }
  return g_fail - f0;
}

int checkA4(uint64_t seed) {
  const int f0 = g_fail;
  art::RigScope rig2(2);   // (P2c flip) rig 2 here; the previous rig is restored on return (it was "back to rig 1")
  {   // lying after the killing blow; the corpse's life
    Game g(seed);
    stage(g, seed);
    g.godMode = true;
    const int bi = AnimOps::spawnBandit(g, g.pl().p + Vec2(40, 0), 1);
    const int id = bi >= 0 ? g.actors[(size_t)bi].id : -1;
    AnimOps::damage(g, bi, 1e6f);
    float lyingAt = -1, goneAt = -1;
    for (int k = 0; k < 60 * 62 && goneAt < 0; k++) {
      int at = -1;
      for (size_t i = 0; i < g.actors.size(); i++) if (g.actors[i].id == id) at = (int)i;
      if (at < 0) { goneAt = k * SIM_DT; break; }
      const AnimPick p = animPick(g, g.actors[(size_t)at], 2);
      if (lyingAt < 0 && p.clip == art::Clip::Lying) lyingAt = k * SIM_DT;
      tick(g, Input());
      g.mode = Mode::Play;
    }
    std::printf("A4 rig 2: lying %.2f s after the blow, the corpse gone at %.1f s\n", lyingAt, goneAt);
    if (lyingAt < 0 || lyingAt > 0.45f) { std::printf("FAIL: A4 lying %.2f s after the killing blow (0.45 or less)\n", lyingAt); g_fail++; }
    const float life = animCorpseLife(2);
    if (std::fabs(life - 60.0f) > 0.01f) { std::printf("FAIL: A4 animCorpseLife(2) %.1f s (60)\n", life); g_fail++; }
    if (goneAt < 59.8f || goneAt > 60.2f) { std::printf("FAIL: A4 the corpse lay %.1f s (60)\n", goneAt); g_fail++; }
  }
  {   // the cap; walking through a corpse
    Game g(seed);
    stage(g, seed);
    g.godMode = true;
    int most = 0;
    for (int k = 0; k < 30; k++) {
      const int bi = AnimOps::spawnBandit(g, g.pl().p + Vec2(60.0f + (float)(k % 6) * 10, -30.0f + (float)(k / 6) * 12), 1);
      AnimOps::damage(g, bi, 1e6f);
      for (int s = 0; s < 6; s++) { tick(g, Input()); g.mode = Mode::Play; }
      int dead = 0;
      for (const Actor& a : g.actors) dead += a.human && a.st == AState::Dead;
      most = std::max(most, dead);
    }
    std::printf("A4 rig 2: at most %d corpses at once with 30 killed (cap %d)\n", most, animCorpseCap(2));
    if (animCorpseCap(2) != 24 || most > 24) { std::printf("FAIL: A4 %d corpses at once (24 at most)\n", most); g_fail++; }
    // a corpse right in the hero's way
    const Vec2 start = g.pl().p;
    const int bi = AnimOps::spawnBandit(g, start + Vec2(10, 0), 1);
    AnimOps::damage(g, bi, 1e6f);
    for (int s = 0; s < 30; s++) { tick(g, Input()); g.mode = Mode::Play; }
    Input in;
    in.move = Vec2(1, 0);
    for (int s = 0; s < 30; s++) { tick(g, in); g.mode = Mode::Play; }
    if (g.pl().p.x < start.x + 18.0f) { std::printf("FAIL: A4 a corpse blocks the hero (walked %.1f px of 18)\n", g.pl().p.x - start.x); g_fail++; }
  }
  return g_fail - f0;
}

int checkA5(uint64_t seed) {
  const int f0 = g_fail;
  art::RigScope rig2(2);   // (P2c flip) rig 2 here; the previous rig is restored on return (it was "back to rig 1")
  Game g(seed);
  stage(g, seed);
  g.godMode = false;
  AnimOps::damage(g, 0, 1e6f);
  bool fell = false;
  float lyingAt = -1;
  for (int k = 0; k < 60 && lyingAt < 0; k++) {
    tick(g, Input());
    const AnimPick p = animPick(g, g.pl(), 2);
    if (p.clip == art::Clip::Fall) fell = true;
    if (p.clip == art::Clip::Lying) lyingAt = (k + 1) * SIM_DT;
  }
  std::printf("A5 rig 2: the hero's fall %s, lying at %.2f s (mode %d)\n", fell ? "plays" : "never plays", lyingAt, (int)g.mode);
  if (!fell || lyingAt < 0 || lyingAt > 0.45f) { std::printf("FAIL: A5 the hero's fall under the death screen\n"); g_fail++; }
  return g_fail - f0;
}

std::string fmtA(const char* f, ...) {
  char b[512];
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(b, sizeof b, f, ap);
  va_end(ap);
  return b;
}

// ---- A6 (P2c FOLK-SIM-C, production; docs/P2C_CONTRACTS.md 6): the pick pieces at rig 2, per weapon class. One PASS line
// per piece unless it fails:
//   carry      animCarry per art id (sheathed, upright, slung, back, tool; civil trades never draw)
//   drawn      every fighter's weapon class is drawn after a swing (DRAWN part) and put away kDrawnHold s later; a tool,
//              the walking stick and bare fists never
//   threat     a fighter at rest draws when a hostile comes inside its aggro radius, and turns to face it: a quarter turn
//              after its reaction, a half turn through the side facing nearer the threat (never a snap from U to D)
//   block      the sword's parry and a shield bearer's frontal blow set blockT = kBlockShow; the pick shows Block raise /
//              impact / brace at 30 / 50 / 70 ms and nothing after 0.15 s; a blow from behind sets nothing
//   crossbow   aim (Draw v1, 2 frames), shoot (Release v1, 80 ms), crank (Draw v2, 3 frames in order); a bow keeps v0
//   bash       a shield bearer's roll is the bash (variant 1); a two-hander's or a bow's roll is not
//   impact     hit 3 of the greatsword, axe and mace flags the first kImpactShow s of its strike; hits 1-2 and a sword's 3 never
int checkA6(uint64_t seed) {
  const int f0 = g_fail;
  art::RigScope rig2(2);   // (P2c flip) rig 2 here; the previous rig is restored on return (it was "back to rig 1")
  auto fail = [&](const std::string& s) { std::printf("FAIL: A6 %s\n", s.c_str()); g_fail++; };
  // carry
  {
    struct C { uint8_t w; bool civil; AnimCarry want; };
    const C cs[] = {{0, false, AnimCarry::None},     {1, false, AnimCarry::Sheathed}, {2, false, AnimCarry::Sheathed}, {5, false, AnimCarry::Sheathed},
                    {6, false, AnimCarry::Sheathed}, {3, false, AnimCarry::Slung},    {4, false, AnimCarry::Upright},  {8, false, AnimCarry::Upright},
                    {9, false, AnimCarry::Upright},  {7, false, AnimCarry::Back},     {10, false, AnimCarry::Tool},    {11, false, AnimCarry::Tool},
                    {12, false, AnimCarry::Tool},    {6, true, AnimCarry::Tool},      {3, true, AnimCarry::Slung},     {4, true, AnimCarry::Upright}};
    int bad = 0;
    for (const C& c : cs) {
      art::HumanLook L;
      L.weapon = c.w; L.civil = c.civil ? 1 : 0;
      if (animCarry(L) != c.want) { fail(fmtA("carry weapon %d civil %d: %d, want %d", c.w, (int)c.civil, (int)animCarry(L), (int)c.want)); bad++; }
    }
    if (!bad) std::printf("A6 carry: %zu art ids ride as the plan says\n", sizeof cs / sizeof cs[0]);
  }
  Game g(seed);
  stage(g, seed);
  // drawn after a swing, per class, then put away
  {
    int bad = 0;
    for (uint8_t w = 0; w <= 12; w++)
      for (int civ = 0; civ < 2; civ++) {
        Actor a;
        a.id = 70000 + w * 2 + civ; a.human = true; a.npc = true; a.faction = Faction::Town; a.aggroR = 0;
        a.look.weapon = w; a.look.civil = (uint8_t)civ;
        a.st = AState::Strike;
        animStep(g, a, 0, SIM_DT);
        a.st = AState::Idle; a.stT = 0;
        const bool fighter = !civ && w >= 1 && w <= 8;
        AnimPick k = animPick(g, a, 2);
        const bool out1 = k.drawn && k.part == art::RigPart::Drawn && k.variant == 1;
        for (int f = 0; f < (int)std::lround((kDrawnHold + 0.2f) / SIM_DT); f++) { animStep(g, a, 0, SIM_DT); a.stT += SIM_DT; }
        k = animPick(g, a, 2);
        const bool away = !k.drawn && k.part == art::RigPart::Loco;
        if (fighter ? !(out1 && away) : (out1 || !away)) {
          fail(fmtA("drawn weapon %d civil %d: after the swing drawn %d, %.1f s later drawn %d part %d", w, civ, (int)out1, kDrawnHold + 0.2f, (int)k.drawn, (int)k.part));
          bad++;
        }
      }
    if (!bad) std::printf("A6 drawn: weapons 1-8 drawn after a swing and away %.1f s later; tools, the stick, fists and civil trades never\n", kDrawnHold + 0.2f);
  }
  // the threat: a guard facing up, a bandit below him; he draws and turns through a side to face it
  {
    const int bi = AnimOps::spawnBandit(g, g.pl().p + Vec2(6, 70), 1);
    if (bi < 0) fail("threat: no bandit");
    else {
      Actor gd;
      gd.id = 71000; gd.human = true; gd.npc = true; gd.faction = Faction::Town; gd.aggroR = 100;
      gd.look.weapon = 8; gd.look.shield = true;
      gd.p = g.pl().p; gd.face = 1; gd.st = AState::Idle;
      g.actors[(size_t)bi].p = gd.p + Vec2(6, 70);
      std::vector<int> faces;
      bool drew = false;
      for (int f = 0; f < 40; f++) {
        animStep(g, gd, 0, SIM_DT);
        gd.stT += SIM_DT;
        const AnimPick k = animPick(g, gd, 2);
        drew = drew || (k.drawn && k.part == art::RigPart::Drawn);
        if (faces.empty() || faces.back() != k.face) faces.push_back(k.face);
      }
      std::string seq;
      for (int fc : faces) seq += "DURL"[fc & 3];
      const bool ok = drew && faces.size() == 3 && faces[0] == 1 && (faces[1] == 2 || faces[1] == 3) && faces[2] == 0;
      if (!ok) fail("threat: the spear guard facing U with a bandit below: drew " + std::to_string((int)drew) + ", facings " + seq + " (want U, a side, D)");
      else std::printf("A6 threat: the guard draws and turns %s (a half turn through the side)\n", seq.c_str());
      // a quarter turn: facing R, the threat below: R then D, no third facing
      gd.drawnT = 0; gd.face = 2;
      faces.clear();
      for (int f = 0; f < 40; f++) {
        animStep(g, gd, 0, SIM_DT);
        const AnimPick k = animPick(g, gd, 2);
        if (faces.empty() || faces.back() != k.face) faces.push_back(k.face);
      }
      seq.clear();
      for (int fc : faces) seq += "DURL"[fc & 3];
      if (!(faces.size() == 2 && faces[0] == 2 && faces[1] == 0)) fail("threat: a quarter turn R -> D went " + seq);
      g.actors.erase(g.actors.begin() + bi);
    }
  }
  // the block: the sword's parry (the hero striking into a blow from the front), a shield bearer's frontal blow, none behind
  {
    auto blowFrom = [&](Vec2 off, AState st, bool shield) -> float {
      Game h(seed);
      stage(h, seed);
      Rng r(77);
      h.inv.clear();
      h.eqWeapon = h.eqShield = -1;
      h.inv.push_back(gear::makeGear(r, ItemKind::Weapon, (int)WeaponType::Sword, 5, Rarity::Common, 0, Mat::Iron));
      h.eqWeapon = 0;
      if (shield) { h.inv.push_back(gear::makeGear(r, ItemKind::Shield, 0, 5, Rarity::Common, 0, Mat::Iron)); h.eqShield = 1; }
      AnimOps::refreshHero(h);
      const int bi = AnimOps::spawnBandit(h, h.pl().p + off, 1);
      if (bi < 0) return -1;
      h.actors[(size_t)bi].ranged = false;
      Actor& p = h.pl();
      p.aim = Vec2(1, 0); p.face = 2; p.st = st; p.stT = 0.01f; p.blockT = 0;
      SkillOps::takenMul(h, h.actors[(size_t)bi].id, 10.0f);
      return p.blockT;
    };
    const float parry = blowFrom(Vec2(20, 0), AState::Strike, false), shieldF = blowFrom(Vec2(20, 0), AState::Idle, true),
                behind = blowFrom(Vec2(-20, 0), AState::Idle, true), bare = blowFrom(Vec2(20, 0), AState::Idle, false);
    if (std::fabs(parry - kBlockShow) > 1e-4f || std::fabs(shieldF - kBlockShow) > 1e-4f || behind != 0 || bare != 0)
      fail(fmtA("block: parry %.2f, shield front %.2f (%.2f each), shield behind %.2f, no shield idle %.2f (0 each)", parry, shieldF, kBlockShow, behind, bare));
    // the flash's frames on the sim clock
    Actor a;
    a.id = 72000; a.human = true; a.npc = true; a.look.weapon = 1; a.look.shield = true; a.st = AState::Hurt; a.blockT = kBlockShow;
    std::string seq;
    int f = 0, last = -1;
    for (; f < 20; f++) {
      const AnimPick k = animPick(g, a, 2);
      const int fr = k.clip == art::Clip::Block ? k.frame : 9;
      if (fr != last) { seq += fmtA("%s%d@%dms", seq.empty() ? "" : " ", fr, (int)std::lround(f * SIM_DT * 1000)); last = fr; }
      animStep(g, a, 0, SIM_DT);
    }
    // raise 0-30, impact 30-80, brace 80-150 ms on 60 Hz steps (frame 9: Block over)
    const bool ok = seq == "0@0ms 1@33ms 2@83ms 9@150ms";
    if (!ok) fail("block frames " + seq + " (want 0@0ms 1@33ms 2@83ms 9@150ms)");
    else if (g_fail == f0) std::printf("A6 block: parry and shield front set %.2f s; frames %s\n", kBlockShow, seq.c_str());
  }
  // the crossbow
  {
    Actor a;
    a.id = 73000; a.human = true; a.hostile = true; a.ranged = true; a.windup = 0.36f; a.look.weapon = 3; a.look.bowForm = 4;
    std::string seq;
    auto see = [&](AState st, float t) {
      a.st = st; a.stT = t;
      const AnimPick k = animPick(g, a, 2);
      seq += fmtA("%s%s/v%d/f%d/x%d", seq.empty() ? "" : " ", art::clipName(k.clip), k.variant, k.frame, k.xbow);
    };
    see(AState::Windup, 0.02f); see(AState::Windup, 0.30f); see(AState::Strike, 0.02f); see(AState::Strike, 0.12f); see(AState::Recover, 0.05f);
    see(AState::Recover, 0.30f);
    const std::string want = "draw/v1/f0/x1 draw/v1/f1/x1 release/v1/f0/x2 draw/v2/f0/x3 draw/v2/f1/x3 draw/v2/f2/x3";
    a.look.bowForm = 2;
    a.st = AState::Windup; a.stT = 0.3f;
    const AnimPick b = animPick(g, a, 2);
    if (seq != want || b.variant != 0 || b.xbow != 0) fail("crossbow " + seq + " (want " + want + "); a recurve's draw variant " + std::to_string(b.variant));
    else std::printf("A6 crossbow: %s; the recurve keeps draw v0\n", seq.c_str());
  }
  // the bash
  {
    auto roll = [&](uint8_t w, bool sh) {
      Actor a;
      a.id = 74000; a.human = true; a.look.weapon = w; a.look.shield = sh; a.st = AState::Roll; a.stT = 0.1f;
      const AnimPick k = animPick(g, a, 2);
      return k.bash && k.variant == 1 ? 1 : (!k.bash && k.variant == 0 ? 0 : 2);
    };
    const int sw = roll(1, true), sp = roll(8, true), gs = roll(7, true), bw = roll(3, true), none = roll(1, false), fists = roll(0, true);
    if (sw != 1 || sp != 1 || fists != 1 || gs != 0 || bw != 0 || none != 0)
      fail(fmtA("bash: sword+shield %d spear+shield %d fists+shield %d (1 each); greatsword %d bow %d no shield %d (0 each)", sw, sp, fists, gs, bw, none));
  }
  // the impact, in the sim: the hero's three blows with each family; the flag only on hit 3 of the heavy ones
  {
    int bad = 0;
    for (WeaponType t : {WeaponType::Greatsword, WeaponType::Axe, WeaponType::Mace, WeaponType::Sword, WeaponType::Dagger, WeaponType::Spear}) {
      Game h(seed);
      stage(h, seed);
      arm(h, t);
      int on[3] = {0, 0, 0}, strikes = 0;
      AState prev = AState::Idle;
      for (int f = 0; f < 240; f++) {
        Input in;
        in.attack = (f % 4) == 0;   // keep the combo going
        tick(h, in);
        const Actor& p = h.pl();
        if (p.st == AState::Strike && prev != AState::Strike) strikes++;
        prev = p.st;
        const AnimPick k = animPick(h, p, 2);
        if (k.impact) on[std::clamp(p.combo, 0, 2)]++;
      }
      const bool heavy = t == WeaponType::Greatsword || t == WeaponType::Axe || t == WeaponType::Mace;
      const int want = (int)std::ceil(kImpactShow / SIM_DT - 1e-3f);
      const bool ok = heavy ? (on[2] > 0 && on[0] == 0 && on[1] == 0) : (on[0] + on[1] + on[2] == 0);
      if (!ok || strikes < 3) {
        fail(fmtA("impact weapon %d: %d strikes, impact frames by hit %d/%d/%d (heavy %d: hit 3 only, about %d a finisher)", (int)t, strikes, on[0], on[1], on[2], (int)heavy, want));
        bad++;
      }
    }
    if (!bad) std::printf("A6 impact: hit 3 of the greatsword, axe and mace only (%d ms windows)\n", (int)std::lround(kImpactShow * 1000));
  }
  return g_fail - f0;
}

int animCmd(int argc, char** argv) {
  bool gates = false, write = false;
  uint64_t a = 1, b = 1;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "--gates")) gates = true;
    else if (!std::strcmp(argv[i], "--write")) write = true;
    else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
  }
  g_fail = 0;
  checkA1(write);
  if (gates)
    for (uint64_t s = a; s <= b; s++) {
      std::printf("-- seed %llu\n", (unsigned long long)s);
      checkA2(s);
      checkA3(s);
      checkA4(s);
      checkA5(s);
      checkA6(s);
    }
  std::printf(g_fail ? "anim: %d failure(s)\n" : "anim: 0 failures\n", g_fail);
  return g_fail ? 1 : 0;
}
RPG_TEST_CMD("--anim", "P2a animation pick and timing [--seeds A..B] [--gates] [--write]: A1 always; --gates A2-A6", animCmd);

}  // namespace
